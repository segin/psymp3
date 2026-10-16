/*
 * test_live_stream_buffer.cpp - LiveStreamIOHandler's buffer, driven directly
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * A scripted producer hands the buffer exactly the chunks a test gives it, so
 * the reader's position relative to what has arrived is under the test's
 * control -- which a real connection, with its own timing, cannot promise.
 */

#include "psymp3.h"
#include "test_framework.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using PsyMP3::IO::HTTP::LiveStreamIOHandler;

namespace {

/**
 * @brief The byte at stream offset @p i: a pattern with no short period, so
 *        data at the wrong offset shows.
 * @param i Offset into the stream.
 * @return The byte there.
 */
char liveByte(int64_t i)
{
    return static_cast<char>((i * 7 + i / 251) & 0xFF);
}

/**
 * @brief A live handler whose producer appends what the test feeds it.
 */
class ScriptedLiveHandler : public LiveStreamIOHandler {
public:
    /** @brief Start the producer. */
    ScriptedLiveHandler() { startWorker(); }

    /**
     * @brief Stop the producer before this class's members go. It waits on
     *        the script's own condition, which abort() does not signal, so
     *        it is ended through the script first.
     */
    ~ScriptedLiveHandler() override
    {
        end();
        stopWorker();
    }

    /**
     * @brief Have the producer append the next @p len bytes of the pattern,
     *        and wait until it has.
     * @param len How many.
     */
    void feed(size_t len)
    {
        std::unique_lock<std::mutex> lock(m_script_mutex);
        m_pending += len;
        m_script_cv.notify_all();
        m_script_cv.wait(lock, [this] { return m_pending == 0 || m_stop; });
    }

    /** @brief End the stream cleanly. */
    void end()
    {
        std::lock_guard<std::mutex> lock(m_script_mutex);
        m_stop = true;
        m_script_cv.notify_all();
    }

    // What a real producer does with a station's titles and pictures.
    using LiveStreamIOHandler::recordTitle;
    using LiveStreamIOHandler::recordPicture;
    using LiveStreamIOHandler::recordArtworkUrl;

protected:
    /** @brief Append fed bytes as they are asked for, until ended or aborted. */
    void runWorker() override
    {
        int64_t offset = 0;
        for (;;) {
            size_t len;
            {
                std::unique_lock<std::mutex> lock(m_script_mutex);
                m_script_cv.wait(lock, [this] { return m_pending > 0 || m_stop || m_abort; });
                if (m_stop || m_abort) break;
                len = m_pending;
            }
            std::string chunk(len, '\0');
            for (size_t i = 0; i < len; ++i) chunk[i] = liveByte(offset + static_cast<int64_t>(i));
            if (!appendAudio(chunk.data(), chunk.size())) break;
            offset += static_cast<int64_t>(len);
            std::lock_guard<std::mutex> lock(m_script_mutex);
            m_pending = 0;
            m_script_cv.notify_all();
        }
        {
            std::lock_guard<std::mutex> lock(m_script_mutex);
            m_stop = true;
            m_script_cv.notify_all();
        }
        finishWorker(false, std::string());
    }

private:
    std::mutex m_script_mutex;
    std::condition_variable m_script_cv;
    size_t m_pending = 0;
    bool m_stop = false;
};

/**
 * @brief A reader that seeks ahead of what has arrived (as skipping a large
 *        ID3v2 tag does) gets the right bytes there, and the buffer survives
 *        the data arriving: trimming the history used to measure from the
 *        reader's position, which past the end of the data made it erase more
 *        than the buffer held.
 */
class SeekAheadOfDataTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    SeekAheadOfDataTest() : TestCase("A seek ahead of the data received reads the right bytes when they come") {}
protected:
    /** @brief Seek 900 KiB ahead of 100 KiB received, then feed past it. */
    void runTest() override {
        ScriptedLiveHandler handler;
        handler.feed(100 * 1024);
        const int64_t target = 1000 * 1024;
        ASSERT_EQUALS(0, handler.seek(target, SEEK_SET), "a seek within the forward window is allowed");
        // Arrives in pieces, the first of which leaves the reader still ahead.
        handler.feed(300 * 1024);
        handler.feed(700 * 1024);
        std::string got(4096, '\0');
        ASSERT_EQUALS(got.size(), handler.read(&got[0], 1, got.size()), "the bytes at the target are read");
        bool match = true;
        for (size_t i = 0; i < got.size(); ++i) {
            if (got[i] != liveByte(target + static_cast<int64_t>(i))) { match = false; break; }
        }
        ASSERT_TRUE(match, "they are the stream's bytes at that offset");
        ASSERT_EQUALS(target + static_cast<int64_t>(got.size()), handler.tell(), "tell follows the read");
        // The history behind the reader is still there for a short seek back.
        ASSERT_EQUALS(0, handler.seek(target - 64 * 1024, SEEK_SET), "a short seek back still works");
        char c = 0;
        ASSERT_EQUALS(size_t(1), handler.read(&c, 1, 1), "and reads");
        ASSERT_EQUALS(liveByte(target - 64 * 1024), c, "the right byte");
        handler.end();
    }
};

/**
 * @brief Once aborted, a reader that has caught up sees the end at once:
 *        eof() used to stay false until the producer had wound up, so a
 *        read-until-eof loop spun meanwhile. And a SEEK_CUR far enough to
 *        overflow is refused rather than computed.
 */
class AbortEofAndSeekRangeTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    AbortEofAndSeekRangeTest() : TestCase("eof() right after abort; SEEK_CUR overflow refused") {}
protected:
    /** @brief Read what came, abort, check eof; then the huge seeks. */
    void runTest() override {
        ScriptedLiveHandler handler;
        handler.feed(1000);
        std::string got(1000, '\0');
        ASSERT_EQUALS(got.size(), handler.read(&got[0], 1, got.size()), "what arrived is read");
        ASSERT_EQUALS(-1, handler.seek(std::numeric_limits<int64_t>::max(), SEEK_CUR),
                      "a SEEK_CUR past the largest offset is refused");
        ASSERT_EQUALS(int64_t(1000), handler.tell(), "and moves nothing");
        handler.abort();
        ASSERT_TRUE(handler.eof(), "aborted and caught up: at the end");
    }
};

/**
 * @brief The smallest file sniffMimeType() takes for a PNG: its signature
 *        and an IHDR chunk of @p width x @p height.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @return The bytes.
 */
std::string tinyPng(uint32_t width, uint32_t height)
{
    std::string png("\x89PNG\r\n\x1a\n", 8);
    png += std::string("\0\0\0\x0dIHDR", 8);
    for (uint32_t v : {width, height}) {
        for (int shift = 24; shift >= 0; shift -= 8) png += static_cast<char>((v >> shift) & 0xFF);
    }
    png += std::string("\x08\x02\0\0\0", 5); // 8-bit RGB
    png += std::string(4, '\0');             // CRC, unchecked
    return png;
}

/**
 * @brief Where pictures apply: from their offset until the next picture or
 *        the next title, so a song sent without one shows none rather than
 *        the last song's; one sent again for the same song is not repeated.
 */
class PictureTrackTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    PictureTrackTest() : TestCase("Pictures apply from their offset to the next title or picture") {}
protected:
    /** @brief Two songs, a picture for the first, none, then one, for the second. */
    void runTest() override {
        ASSERT_TRUE(LiveStreamIOHandler::pictureFromBytes("<html>home page</html>") == nullptr,
                    "a home page is no picture");
        const auto first = LiveStreamIOHandler::pictureFromBytes(tinyPng(300, 200));
        ASSERT_TRUE(first != nullptr, "a PNG is a picture");
        ASSERT_EQUALS(std::string("image/png"), first->mime_type, "typed by its header");
        ASSERT_EQUALS(300u, first->width, "its size read");

        ScriptedLiveHandler handler;
        handler.feed(1000);
        handler.recordTitle("A - One");
        handler.recordPicture(first);
        ASSERT_TRUE(handler.getStreamPictureAt(0) == first, "the first song's picture from its start");
        handler.feed(1000);
        handler.recordPicture(LiveStreamIOHandler::pictureFromBytes(tinyPng(300, 200)));
        ASSERT_TRUE(handler.getStreamPictureAt(1500) == first, "the same picture again is not a new one");
        handler.recordTitle("B - Two");
        ASSERT_TRUE(handler.getStreamPictureAt(1999) == first, "still the first song's up to the second");
        ASSERT_TRUE(handler.getStreamPictureAt(2000) == nullptr, "the second song came with none");
        handler.feed(1000);
        const auto second = LiveStreamIOHandler::pictureFromBytes(tinyPng(500, 500));
        handler.recordPicture(second);
        ASSERT_TRUE(handler.getStreamPictureAt(2500) == nullptr, "a picture sent later starts later");
        ASSERT_TRUE(handler.getStreamPictureAt(3000) == second, "and applies from there");
        ASSERT_TRUE(handler.getStreamPictureAt(500) == first, "earlier audio keeps its own");
        handler.end();
    }
};

/**
 * @brief A one-thread HTTP server for artwork tests: /art.png answers a
 *        picture, /home a page, anything else 404; it counts requests.
 */
class ArtServer {
public:
    /** @brief Listen on a free loopback port and serve until destroyed. */
    ArtServer()
    {
        m_listen = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        ::bind(m_listen, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        socklen_t len = sizeof(addr);
        ::getsockname(m_listen, reinterpret_cast<sockaddr*>(&addr), &len);
        m_port = ntohs(addr.sin_port);
        ::listen(m_listen, 8);
        m_thread = std::thread([this] { serve(); });
    }

    /** @brief Stop serving and wait for the thread. */
    ~ArtServer()
    {
        m_stop = true;
        ::shutdown(m_listen, SHUT_RDWR);
        ::close(m_listen);
        m_thread.join();
    }

    /**
     * @brief The URL of @p path on this server.
     * @param path Starting with '/'.
     * @return The URL.
     */
    std::string url(const std::string& path) const
    {
        return "http://127.0.0.1:" + std::to_string(m_port) + path;
    }

    /** @brief Requests answered so far. */
    int requests() const { return m_requests; }

private:
    /** @brief Answer each connection's one request, then close it. */
    void serve()
    {
        while (!m_stop) {
            const int fd = ::accept(m_listen, nullptr, nullptr);
            if (fd < 0) return;
            std::string request;
            char buf[1024];
            while (request.find("\r\n\r\n") == std::string::npos) {
                const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
                if (n <= 0) break;
                request.append(buf, static_cast<size_t>(n));
            }
            ++m_requests;
            std::string type = "text/html", body = "<html>home</html>", status = "200 OK";
            if (request.compare(0, 13, "GET /art.png ") == 0) {
                type = "image/png";
                body = tinyPng(640, 640);
            } else if (request.compare(0, 10, "GET /home ") != 0) {
                status = "404 Not Found";
                body = "no";
            }
            const std::string reply = "HTTP/1.1 " + status + "\r\nContent-Type: " + type + "\r\nContent-Length: " +
                                      std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
            ::send(fd, reply.data(), reply.size(), 0);
            ::close(fd);
        }
    }

    int m_listen = -1;
    uint16_t m_port = 0;
    std::atomic<bool> m_stop{false};
    std::atomic<int> m_requests{0};
    std::thread m_thread;
};

/**
 * @brief Wait up to two seconds for the picture at @p offset to be there.
 * @param handler The handler.
 * @param offset Where.
 * @return It, or null if none came.
 */
std::shared_ptr<const PsyMP3::Tag::Picture> waitForPicture(ScriptedLiveHandler& handler, int64_t offset)
{
    for (int i = 0; i < 200; ++i) {
        if (auto picture = handler.getStreamPictureAt(offset)) return picture;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return nullptr;
}

/**
 * @brief Artwork by URL: fetched in the background and shown once there; a
 *        URL that is a home page shows nothing; each URL is fetched once,
 *        however many songs name it (a station's logo with every title).
 */
class ArtworkUrlTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    ArtworkUrlTest() : TestCase("Artwork URLs: fetched once, kept only when they are pictures") {}
protected:
    /** @brief A picture URL, a home page, then the picture URL for a later song. */
    void runTest() override {
        ArtServer server;
        ScriptedLiveHandler handler;
        handler.feed(1000);
        handler.recordTitle("A - One");
        handler.recordArtworkUrl(server.url("/art.png"));
        const auto picture = waitForPicture(handler, 500);
        ASSERT_TRUE(picture != nullptr, "the picture arrives");
        ASSERT_EQUALS(640u, picture->width, "and is the server's");

        handler.feed(1000);
        handler.recordTitle("B - Two");
        handler.recordArtworkUrl(server.url("/home"));
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        ASSERT_TRUE(handler.getStreamPictureAt(2000) == nullptr, "a home page gives no picture");
        ASSERT_TRUE(handler.getStreamPictureAt(1500) == picture, "the song before keeps its own");
        ASSERT_EQUALS(2, server.requests(), "it was asked for once");

        handler.feed(1000);
        handler.recordTitle("C - Three");
        handler.recordArtworkUrl(server.url("/art.png"));
        ASSERT_TRUE(handler.getStreamPictureAt(3000) == picture, "a known URL's picture is there at once");
        handler.feed(1000);
        handler.recordTitle("D - Four");
        handler.recordArtworkUrl(server.url("/home"));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ASSERT_EQUALS(2, server.requests(), "neither URL was fetched again");
        handler.end();
    }
};

} // namespace

/**
 * @brief Run the live buffer tests; the entry test_http_suite calls.
 * @return Number of failed tests.
 */
int test_live_stream_buffer_main()
{
    TestFramework::TestSuite suite("Live Stream Buffer Tests");
    suite.addTest(std::make_unique<SeekAheadOfDataTest>());
    suite.addTest(std::make_unique<AbortEofAndSeekRangeTest>());
    suite.addTest(std::make_unique<PictureTrackTest>());
    suite.addTest(std::make_unique<ArtworkUrlTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
