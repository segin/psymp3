/*
 * test_http_stream_io_handler.cpp - HTTPStreamIOHandler against a local server
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Each test starts a one-connection server on 127.0.0.1 that answers with a
 * scripted reply -- an Icecast stream, a SHOUTcast v1 "ICY 200 OK" stream, a
 * plain file, an error -- so the handler is exercised through real libcurl
 * without the internet.
 */

#include "psymp3.h"
#include "test_framework.h"

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using PsyMP3::IO::HTTP::HTTPStreamIOHandler;

#ifndef _WIN32

namespace {

/**
 * @brief The test body's byte at offset @p i: a pattern that does not repeat
 *        on any short period, so a byte out of place shows.
 * @param i Offset into the body.
 * @return The byte expected there.
 */
uint8_t patternByte(size_t i)
{
    return static_cast<uint8_t>((i * 7 + i / 251) & 0xFF);
}

/**
 * @brief The first @p n bytes of the test pattern, as a string.
 * @param n Length.
 * @return patternByte(0) .. patternByte(n - 1).
 */
std::string patternBytes(size_t n)
{
    std::string s(n, '\0');
    for (size_t i = 0; i < n; ++i) s[i] = static_cast<char>(patternByte(i));
    return s;
}

/**
 * @brief A one-connection HTTP server on 127.0.0.1: reads the request head,
 *        then lets a callback send a scripted reply.
 */
class FakeServer {
public:
    using Send = std::function<bool(const std::string&)>;

    /**
     * @brief Listen on a free loopback port and serve one connection on a
     *        thread of its own.
     * @param reply Called with a Send once the request head has been read;
     *        whatever it sends is the whole reply. The connection closes
     *        when it returns.
     */
    explicit FakeServer(std::function<void(const Send&)> reply)
    {
        m_listen = ::socket(AF_INET, SOCK_STREAM, 0);
        int one = 1;
        ::setsockopt(m_listen, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        ::bind(m_listen, reinterpret_cast<sockaddr*>(&addr), sizeof addr);
        ::listen(m_listen, 1);
        socklen_t len = sizeof addr;
        ::getsockname(m_listen, reinterpret_cast<sockaddr*>(&addr), &len);
        m_port = ntohs(addr.sin_port);

        m_thread = std::thread([this, reply] {
            const int fd = ::accept(m_listen, nullptr, nullptr);
            if (fd < 0) return;
            std::string request;
            char buf[1024];
            while (request.find("\r\n\r\n") == std::string::npos) {
                const ssize_t n = ::recv(fd, buf, sizeof buf, 0);
                if (n <= 0) break;
                request.append(buf, static_cast<size_t>(n));
            }
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_request = request;
            }
            reply([fd](const std::string& bytes) {
                size_t sent = 0;
                while (sent < bytes.size()) {
                    const ssize_t n = ::send(fd, bytes.data() + sent, bytes.size() - sent, 0);
                    if (n <= 0) return false;
                    sent += static_cast<size_t>(n);
                }
                return true;
            });
            ::close(fd);
        });
    }

    /**
     * @brief Stop listening and wait for the connection thread, which ends
     *        once its reply has been sent or the client has gone.
     */
    ~FakeServer()
    {
        // Unblocks an accept() that never got its connection.
        ::shutdown(m_listen, SHUT_RDWR);
        if (m_thread.joinable()) m_thread.join();
        ::close(m_listen);
    }

    /**
     * @brief The URL that reaches this server.
     * @return http://127.0.0.1:<port>/stream
     */
    std::string url() const { return "http://127.0.0.1:" + std::to_string(m_port) + "/stream"; }

    /**
     * @brief The request head the client sent, request line and headers.
     * @return It, or "" before a request has arrived.
     */
    std::string request() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_request;
    }

private:
    int m_listen = -1;
    int m_port = 0;
    std::thread m_thread;
    mutable std::mutex m_mutex;
    std::string m_request;
};

/**
 * @brief Read a handler to its end, in uneven pieces.
 * @param h The handler.
 * @return Every byte it delivered.
 */
std::string readAll(HTTPStreamIOHandler& h)
{
    std::string out;
    char buf[7001];
    for (;;) {
        const size_t n = h.read(buf, 1, sizeof buf);
        if (n == 0) break;
        out.append(buf, n);
    }
    return out;
}

std::string interleave(const std::string& audio, size_t metaint, const std::vector<std::string>& blocks);

/** @brief An Icecast-style reply: HTTP status line with icy-* headers. */
class IcecastStreamTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    IcecastStreamTest() : TestCase("Icecast reply: ICY headers, body read whole") {}
protected:
    /** @brief Check the reply is seen as ICY and live, and its body arrives intact. */
    void runTest() override {
        const std::string body = patternBytes(200000);
        FakeServer server([&](const FakeServer::Send& send) {
            send("HTTP/1.0 200 OK\r\nContent-Type: audio/mpeg; charset=x\r\n"
                 "icy-name: Test Radio\r\nicy-br: 128\r\n\r\n");
            send(body);
        });
        HTTPStreamIOHandler h(server.url());
        ASSERT_TRUE(h.isIcy(), "icy-* headers mark the reply as ICY");
        ASSERT_TRUE(h.isLive(), "an ICY reply is a stream");
        ASSERT_EQUALS(std::string("audio/mpeg"), h.getMimeType(), "MIME type without parameters");
        ASSERT_EQUALS(std::string("Test Radio"), h.getHeader("ICY-Name"), "headers are found in any case");
        ASSERT_EQUALS(int64_t(-1), h.getContentLength(), "no Content-Length");
        ASSERT_EQUALS(PsyMP3::IO::filesize_t(-1), h.getFileSize(), "a stream has no size");
        const std::string got = readAll(h);
        ASSERT_EQUALS(body.size(), got.size(), "every body byte arrives");
        ASSERT_TRUE(got == body, "body bytes arrive unchanged and in order");
        ASSERT_TRUE(h.eof(), "EOF once the server has closed and the buffer is drained");
    }
};

/** @brief A SHOUTcast v1 reply, whose "ICY 200 OK" head libcurl passes on as body. */
class ShoutcastV1Test : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    ShoutcastV1Test() : TestCase("SHOUTcast v1 'ICY 200 OK', header split byte by byte") {}
protected:
    /** @brief Check the ICY head is parsed into headers and kept out of the body. */
    void runTest() override {
        const std::string head = "ICY 200 OK\r\nicy-name: Old Server\r\n"
                                 "content-type: audio/aacp\r\nicy-metaint: 8192\r\n\r\n";
        const std::string body = patternBytes(50000);
        // Metadata counts from the first byte after the ICY head.
        const std::string wire = interleave(body, 8192, {"StreamTitle='V1 Song';"});
        FakeServer server([&](const FakeServer::Send& send) {
            // One byte per TCP send, so the block reaches the handler in
            // pieces and has to be put back together.
            for (char c : head) {
                if (!send(std::string(1, c))) return;
                std::this_thread::sleep_for(std::chrono::microseconds(200));
            }
            send(wire);
        });
        HTTPStreamIOHandler h(server.url());
        ASSERT_TRUE(h.isIcy(), "an ICY v1 reply is ICY");
        ASSERT_EQUALS(std::string("Old Server"), h.getHeader("icy-name"), "header from the ICY block");
        ASSERT_EQUALS(std::string("8192"), h.getHeader("icy-metaint"), "icy-metaint from the ICY block");
        ASSERT_EQUALS(std::string("audio/aacp"), h.getMimeType(), "Content-Type from the ICY block");
        const std::string got = readAll(h);
        ASSERT_EQUALS(body.size(), got.size(), "neither the head nor the metadata is in the audio");
        ASSERT_TRUE(got == body, "audio starts right after the blank line");
        ASSERT_EQUALS(std::string("V1 Song"), h.getStreamTitleAt(0), "title from a v1 server");
    }
};

/** @brief A reply with no status line at all, starting with bytes that look like "ICY". */
class Http09BodyTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    Http09BodyTest() : TestCase("HTTP/0.9 reply that is not ICY is all body") {}
protected:
    /** @brief Check none of it is mistaken for an ICY head and dropped. */
    void runTest() override {
        const std::string body = "IC" + patternBytes(10000);
        FakeServer server([&](const FakeServer::Send& send) {
            send(body.substr(0, 1));
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            send(body.substr(1));
        });
        HTTPStreamIOHandler h(server.url());
        ASSERT_FALSE(h.isIcy(), "no ICY headers");
        ASSERT_TRUE(h.isLive(), "no length, so a stream");
        ASSERT_TRUE(readAll(h) == body, "first bytes are kept, not taken for a header");
    }
};

/** @brief An ordinary file reply, with Content-Length. */
class FileWithLengthTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    FileWithLengthTest() : TestCase("A file of known length is not live") {}
protected:
    /** @brief Check it is not treated as a stream, so openHTTPIOHandler hands it on. */
    void runTest() override {
        FakeServer server([&](const FakeServer::Send& send) {
            send("HTTP/1.1 200 OK\r\nContent-Type: audio/flac\r\nContent-Length: 1000\r\n"
                 "Connection: close\r\n\r\n" + patternBytes(1000));
        });
        HTTPStreamIOHandler h(server.url());
        ASSERT_FALSE(h.isIcy(), "no ICY headers");
        ASSERT_FALSE(h.isLive(), "Content-Length makes it a file");
        ASSERT_EQUALS(int64_t(1000), h.getContentLength(), "Content-Length parsed");
    }
};

/** @brief Error replies in both dialects. */
class ErrorStatusTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    ErrorStatusTest() : TestCase("Error statuses throw (HTTP 404, ICY 401)") {}
protected:
    /** @brief Check the constructor throws InvalidMediaException for each. */
    void runTest() override {
        bool threw = false;
        {
            FakeServer server([](const FakeServer::Send& send) {
                send("HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nnot found");
            });
            try {
                HTTPStreamIOHandler h(server.url());
            } catch (const InvalidMediaException&) {
                threw = true;
            }
        }
        ASSERT_TRUE(threw, "HTTP 404 throws InvalidMediaException");

        threw = false;
        {
            FakeServer server([](const FakeServer::Send& send) {
                send("ICY 401 Service Unavailable\r\n\r\n");
            });
            try {
                HTTPStreamIOHandler h(server.url());
            } catch (const InvalidMediaException&) {
                threw = true;
            }
        }
        ASSERT_TRUE(threw, "ICY 401 throws InvalidMediaException");
    }
};

/** @brief Seeking, and a body bigger than the read-ahead limit. */
class SeekWindowTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    SeekWindowTest() : TestCase("Seeks within the kept window only; 3 MB through the bounded buffer") {}
protected:
    /**
     * @brief Check short backward seeks work, SEEK_END and seeks past the
     *        kept history fail, and 3 MB arrive intact through the buffer.
     */
    void runTest() override {
        const size_t total = 3 * 1024 * 1024;
        const std::string body = patternBytes(total);
        FakeServer server([&](const FakeServer::Send& send) {
            send("HTTP/1.0 200 OK\r\nicy-name: Seek\r\n\r\n");
            // In pieces, so the worker's waits for the reader are exercised.
            for (size_t off = 0; off < total; off += 32768) {
                if (!send(body.substr(off, 32768))) return;
            }
        });
        HTTPStreamIOHandler h(server.url());

        char buf[4096];
        ASSERT_EQUALS(sizeof buf, h.read(buf, 1, sizeof buf), "first read");
        ASSERT_EQUALS(0, h.seek(-3, SEEK_CUR), "a short step back is allowed");
        ASSERT_EQUALS(PsyMP3::IO::filesize_t(4093), h.tell(), "tell after stepping back");
        uint8_t b = 0;
        ASSERT_EQUALS(size_t(1), h.read(&b, 1, 1), "read after stepping back");
        ASSERT_EQUALS(patternByte(4093), b, "the byte stepped back to");
        ASSERT_EQUALS(-1, h.seek(0, SEEK_END), "SEEK_END is refused");
        ASSERT_EQUALS(0, h.seek(0, SEEK_SET), "back to the start while it is still held");

        // Read it all, checking every byte.
        std::string got;
        for (;;) {
            const size_t n = h.read(buf, 1, sizeof buf);
            if (n == 0) break;
            got.append(buf, n);
        }
        ASSERT_EQUALS(total, got.size(), "every byte arrives");
        ASSERT_TRUE(got == body, "bytes unchanged through the bounded buffer");

        ASSERT_EQUALS(0, h.seek(-1000, SEEK_CUR), "a step back within the history");
        ASSERT_EQUALS(-1, h.seek(0, SEEK_SET), "the start has been dropped by now");
    }
};

/** @brief A station that goes quiet while a read is waiting. */
class CloseWakesReadTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    CloseWakesReadTest() : TestCase("close() releases a read waiting on a silent stream") {}
protected:
    /** @brief Check close() from another thread ends the waiting read promptly. */
    void runTest() override {
        std::atomic<bool> done{false};
        FakeServer server([&](const FakeServer::Send& send) {
            send("HTTP/1.0 200 OK\r\nicy-name: Silent\r\n\r\n0123456789");
            // Then nothing, until the test is over.
            while (!done) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
        HTTPStreamIOHandler h(server.url());

        size_t got = 0;
        auto start = std::chrono::steady_clock::now();
        std::thread reader([&] {
            char buf[100];
            got = h.read(buf, 1, sizeof buf);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        h.close();
        reader.join();
        done = true;
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        ASSERT_EQUALS(size_t(10), got, "the read returns what had arrived");
        ASSERT_TRUE(ms < 3000, "the read returns promptly after close()");
        ASSERT_TRUE(h.eof(), "closed is EOF");
    }
};

/**
 * @brief Interleave ICY metadata blocks into audio, as a server does.
 * @param audio The audio bytes.
 * @param metaint Audio bytes between blocks.
 * @param blocks Metadata for each gap in turn, unpadded; "" sends a zero
 *        length byte. Gaps past the end of the list also get "".
 * @return The body as it goes over the wire.
 */
std::string interleave(const std::string& audio, size_t metaint, const std::vector<std::string>& blocks)
{
    std::string out;
    size_t gap = 0;
    for (size_t off = 0; off < audio.size(); off += metaint) {
        out += audio.substr(off, metaint);
        if (off + metaint > audio.size()) break; // no block after a short tail
        const std::string meta = gap < blocks.size() ? blocks[gap] : std::string();
        ++gap;
        const size_t units = (meta.size() + 15) / 16;
        out += static_cast<char>(units);
        out += meta;
        out += std::string(units * 16 - meta.size(), '\0');
    }
    return out;
}

/** @brief The pure helpers: StreamTitle parsing and ICY text decoding. */
class MetadataParsingTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    MetadataParsingTest() : TestCase("StreamTitle parsing and UTF-8/Latin-1 decoding") {}
protected:
    /** @brief Check titles with apostrophes, padding, other fields and both encodings. */
    void runTest() override {
        std::string t;
        ASSERT_TRUE(HTTPStreamIOHandler::parseStreamTitle("StreamTitle='A - B';", t), "plain title found");
        ASSERT_EQUALS(std::string("A - B"), t, "plain title");
        ASSERT_TRUE(HTTPStreamIOHandler::parseStreamTitle(
            std::string("StreamTitle='Guns N' Roses - Patience';StreamUrl='x';\0\0\0", 54), t),
            "title with an apostrophe found");
        ASSERT_EQUALS(std::string("Guns N' Roses - Patience"), t, "apostrophe kept, other fields ignored");
        ASSERT_TRUE(HTTPStreamIOHandler::parseStreamTitle("StreamTitle='';", t), "empty title found");
        ASSERT_EQUALS(std::string(), t, "empty title");
        ASSERT_TRUE(HTTPStreamIOHandler::parseStreamTitle(std::string("StreamTitle='Cut off\0\0", 22), t),
                    "unterminated title found");
        ASSERT_EQUALS(std::string("Cut off"), t, "unterminated title stops at the padding");
        ASSERT_FALSE(HTTPStreamIOHandler::parseStreamTitle("StreamUrl='x';", t), "no StreamTitle");

        ASSERT_EQUALS(std::string("Beyonc\xC3\xA9"), HTTPStreamIOHandler::decodeIcyText("Beyonc\xC3\xA9"),
                      "valid UTF-8 is kept");
        ASSERT_EQUALS(std::string("Beyonc\xC3\xA9"), HTTPStreamIOHandler::decodeIcyText("Beyonc\xE9"),
                      "invalid UTF-8 is read as Latin-1");
        ASSERT_EQUALS(std::string("plain"), HTTPStreamIOHandler::decodeIcyText("plain"), "ASCII unchanged");
    }
};

/** @brief A stream with icy-metaint: metadata must leave the audio, titles must be kept. */
class MetadataStreamTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    MetadataStreamTest() : TestCase("icy-metaint stream: blocks stripped, titles at their offsets") {}
protected:
    /**
     * @brief Send audio with titles (one repeated, one Latin-1, gaps with no
     *        block) in odd-sized pieces; check the audio arrives exactly and
     *        each title is in effect where it should be.
     */
    void runTest() override {
        const size_t metaint = 1000;
        const std::string audio = patternBytes(10500);
        const std::vector<std::string> blocks = {
            "StreamTitle='First';",           // after audio 1000
            "",                                // 2000: no block
            "StreamTitle='First';",           // 3000: same title again
            "StreamTitle='Caf\xE9 Second';",  // 4000: Latin-1
            "StreamTitle='Third';StreamUrl='http://x/';",  // 5000
        };
        const std::string wire = interleave(audio, metaint, blocks);
        FakeServer server([&](const FakeServer::Send& send) {
            send("HTTP/1.0 200 OK\r\nicy-name: Caf\xE9 Radio\r\nicy-metaint: 1000\r\n\r\n");
            for (size_t off = 0; off < wire.size(); off += 333) {
                if (!send(wire.substr(off, 333))) return;
            }
        });
        HTTPStreamIOHandler h(server.url());
        const std::string got = readAll(h);
        ASSERT_EQUALS(audio.size(), got.size(), "metadata is not left in the audio");
        ASSERT_TRUE(got == audio, "audio unchanged around the blocks");

        ASSERT_EQUALS(std::string("Caf\xC3\xA9 Radio"), h.getStationName(), "icy-name, Latin-1 decoded");
        ASSERT_EQUALS(uint64_t(3), h.getStreamTitleCount(), "a repeated title is not a new one");
        ASSERT_EQUALS(std::string("First"), h.getStreamTitleAt(0), "the first title covers the start");
        ASSERT_EQUALS(std::string("First"), h.getStreamTitleAt(3999), "still the first before the change");
        ASSERT_EQUALS(std::string("Caf\xC3\xA9 Second"), h.getStreamTitleAt(4000),
                      "second title from the audio offset it arrived at");
        ASSERT_EQUALS(std::string("Third"), h.getStreamTitleAt(5000), "third title");
        ASSERT_EQUALS(std::string("Third"), h.getStreamTitleAt(10499), "last title holds to the end");
    }
};

/** @brief Request headers the handler must send. */
class MetadataRequestTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    MetadataRequestTest() : TestCase("Asks for ICY metadata") {}
protected:
    /** @brief Check the request carries Icy-MetaData: 1. */
    void runTest() override {
        FakeServer server([](const FakeServer::Send& send) {
            send("HTTP/1.0 200 OK\r\nicy-name: R\r\n\r\nabc");
        });
        {
            HTTPStreamIOHandler h(server.url());
            (void)readAll(h);
        }
        ASSERT_TRUE(server.request().find("\r\nIcy-MetaData: 1\r\n") != std::string::npos,
                    "Icy-MetaData: 1 sent");
    }
};

/**
 * @brief A server on 127.0.0.1 that answers any number of connections, each
 *        with a reply chosen by its number (0, 1, 2...).
 */
class SequenceServer {
public:
    /**
     * @brief Listen and serve until destroyed.
     * @param reply Called with the connection's number and a Send; the
     *        connection closes when it returns.
     */
    explicit SequenceServer(std::function<void(int, const FakeServer::Send&)> reply)
    {
        m_listen = ::socket(AF_INET, SOCK_STREAM, 0);
        int one = 1;
        ::setsockopt(m_listen, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        ::bind(m_listen, reinterpret_cast<sockaddr*>(&addr), sizeof addr);
        ::listen(m_listen, 8);
        socklen_t len = sizeof addr;
        ::getsockname(m_listen, reinterpret_cast<sockaddr*>(&addr), &len);
        m_port = ntohs(addr.sin_port);
        m_thread = std::thread([this, reply] {
            for (int n = 0;; ++n) {
                const int fd = ::accept(m_listen, nullptr, nullptr);
                if (fd < 0) return;
                char buf[2048];
                std::string request;
                while (request.find("\r\n\r\n") == std::string::npos) {
                    const ssize_t got = ::recv(fd, buf, sizeof buf, 0);
                    if (got <= 0) break;
                    request.append(buf, static_cast<size_t>(got));
                }
                m_connections = n + 1;
                reply(n, [fd](const std::string& bytes) {
                    size_t sent = 0;
                    while (sent < bytes.size()) {
                        const ssize_t s = ::send(fd, bytes.data() + sent, bytes.size() - sent, 0);
                        if (s <= 0) return false;
                        sent += static_cast<size_t>(s);
                    }
                    return true;
                });
                ::close(fd);
            }
        });
    }

    /** @brief Stop accepting and wait for the server thread. */
    ~SequenceServer()
    {
        ::shutdown(m_listen, SHUT_RDWR);
        ::close(m_listen);
        if (m_thread.joinable()) m_thread.join();
    }

    /**
     * @brief The URL that reaches this server.
     * @return http://127.0.0.1:<port>/stream
     */
    std::string url() const { return "http://127.0.0.1:" + std::to_string(m_port) + "/stream"; }

    /**
     * @brief Connections accepted so far.
     * @return The count.
     */
    int connections() const { return m_connections.load(); }

private:
    int m_listen = -1;
    int m_port = 0;
    std::thread m_thread;
    std::atomic<int> m_connections{0};
};

/** @brief A station that drops the connection, comes back, then refuses. */
class ReconnectTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    ReconnectTest() : TestCase("A dropped ICY stream reconnects, then gives up") {}
protected:
    /**
     * @brief Serve part A, close; serve part B with a new title, close; then
     *        answer 503. The reader must get A then B, the second title from
     *        where B starts, and the end after the allowed failures.
     */
    void runTest() override {
        const int saved = HTTPStreamIOHandler::reconnectAttempts();
        HTTPStreamIOHandler::setReconnectAttempts(2);
        const std::string a = patternBytes(4000);
        const std::string b = patternBytes(6000).substr(1000);
        SequenceServer server([&](int n, const FakeServer::Send& send) {
            if (n == 0) {
                send("HTTP/1.0 200 OK\r\nicy-name: Flaky FM\r\nicy-metaint: 1000\r\n\r\n");
                send(interleave(a, 1000, {"StreamTitle='One - First';"}));
            } else if (n == 1) {
                send("HTTP/1.0 200 OK\r\nicy-name: Flaky FM\r\nicy-metaint: 1000\r\n\r\n");
                send(interleave(b, 1000, {"StreamTitle='Two - Second';"}));
            } else {
                send("HTTP/1.0 503 Service Unavailable\r\nContent-Length: 0\r\n\r\n");
            }
        });
        std::string got;
        {
            HTTPStreamIOHandler h(server.url());
            got = readAll(h);
            ASSERT_EQUALS(std::string("One - First"), h.getStreamTitleAt(0), "first connection's title");
            ASSERT_EQUALS(std::string("Two - Second"),
                          h.getStreamTitleAt(static_cast<PsyMP3::IO::filesize_t>(a.size())),
                          "second connection's title from where its audio starts");
            ASSERT_EQUALS(EIO, h.getLastError(), "the end is reported as an error once given up");
        }
        HTTPStreamIOHandler::setReconnectAttempts(saved);
        ASSERT_TRUE(got == a + b, "both connections' audio, in order, metadata removed");
        ASSERT_TRUE(server.connections() >= 3, "it came back after each drop until refused");
    }
};

/** @brief StreamTitles as stations send them, taken apart. */
class SplitTitleTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    SplitTitleTest() : TestCase("StreamTitle split into artist and title (plain and iHeart)") {}
protected:
    /** @brief Check the forms seen from 181.FM, Radio Paradise and iHeart. */
    void runTest() override {
        using Parts = HTTPStreamIOHandler::TitleParts;
        Parts p = HTTPStreamIOHandler::splitStreamTitle("Judas Priest - Living After Midnight ");
        ASSERT_EQUALS(std::string("Judas Priest"), p.artist, "plain: artist");
        ASSERT_EQUALS(std::string("Living After Midnight"), p.title, "plain: title, trailing space gone");
        ASSERT_TRUE(p.is_music, "plain: music");

        p = HTTPStreamIOHandler::splitStreamTitle("Soho - Hippy Chick - Live");
        ASSERT_EQUALS(std::string("Soho"), p.artist, "split at the first dash");
        ASSERT_EQUALS(std::string("Hippy Chick - Live"), p.title, "the rest is the title");

        p = HTTPStreamIOHandler::splitStreamTitle("Station Jingle");
        ASSERT_EQUALS(std::string(), p.artist, "no dash: no artist");
        ASSERT_EQUALS(std::string("Station Jingle"), p.title, "no dash: all title");

        p = HTTPStreamIOHandler::splitStreamTitle(
            "Ray Parker, Jr. - text=\"Ghostbusters\" song_spot=\"M\" spotInstanceId=\"-1\" "
            "length=\"00:04:09\" MediaBaseId=\"1092443\" amgArtworkURL=\"https://i.example/x?a=b\" ");
        ASSERT_EQUALS(std::string("Ray Parker, Jr."), p.artist, "iHeart song: artist before the dash");
        ASSERT_EQUALS(std::string("Ghostbusters"), p.title, "iHeart song: title from text=");
        ASSERT_TRUE(p.is_music, "iHeart song_spot M is music");

        p = HTTPStreamIOHandler::splitStreamTitle(
            " - text=\"Halloween Radio - Fly In - Thriller 03\" song_spot=\"T\" length=\"00:00:14\" "
            "MediaBaseId=\"\" amgArtworkURL=\"\" ");
        ASSERT_EQUALS(std::string(), p.artist, "iHeart spot: no artist");
        ASSERT_EQUALS(std::string("Halloween Radio - Fly In - Thriller 03"), p.title, "iHeart spot: text= whole");
        ASSERT_FALSE(p.is_music, "iHeart song_spot T is not music");

        p = HTTPStreamIOHandler::splitStreamTitle("");
        ASSERT_EQUALS(std::string(), p.title, "empty stays empty");
    }
};

/** @brief A load given up while its station has not answered. */
class AbortGroupConnectTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    AbortGroupConnectTest() : TestCase("Aborting a load's group ends a connect that gets no answer") {}
protected:
    /**
     * @brief Construct a handler inside a group against a server that never
     *        answers; abort the group; the constructor must throw promptly.
     */
    void runTest() override {
        std::atomic<bool> done{false};
        FakeServer server([&](const FakeServer::Send&) {
            while (!done) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
        auto group = std::make_shared<PsyMP3::IO::HTTP::StreamAbortGroup>();
        std::atomic<bool> threw{false};
        const auto start = std::chrono::steady_clock::now();
        std::thread loader([&] {
            PsyMP3::IO::HTTP::StreamAbortGroup::Scope scope(group);
            try {
                HTTPStreamIOHandler h(server.url());
            } catch (const InvalidMediaException&) {
                threw = true;
            }
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        group->abort();
        loader.join();
        done = true;
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        ASSERT_TRUE(threw, "the constructor gives up with InvalidMediaException");
        ASSERT_TRUE(ms < 3000, "promptly, not after the stall check");
        ASSERT_TRUE(group->isAborted(), "the group says it was aborted");
    }
};

/** @brief Opening into a group that has already been given up. */
class AbortedGroupRefusesTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    AbortedGroupRefusesTest() : TestCase("A handler created in an aborted group gives up at once") {}
protected:
    /** @brief Abort first, then construct; it must throw without connecting. */
    void runTest() override {
        auto group = std::make_shared<PsyMP3::IO::HTTP::StreamAbortGroup>();
        group->abort();
        PsyMP3::IO::HTTP::StreamAbortGroup::Scope scope(group);
        bool threw = false;
        try {
            // Nothing listens here; an attempt to connect would fail anyway,
            // but the refusal must come before one is made.
            HTTPStreamIOHandler h("http://127.0.0.1:9/never");
        } catch (const InvalidMediaException& e) {
            threw = std::string(e.what()).find("aborted") != std::string::npos;
        }
        ASSERT_TRUE(threw, "refused as aborted");
    }
};

/** @brief A group detached after its load: the stream plays on. */
class DetachedGroupTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    DetachedGroupTest() : TestCase("Aborting a detached group leaves its stream alone") {}
protected:
    /** @brief Open in a group, detach, abort; the stream must still read whole. */
    void runTest() override {
        const std::string body = patternBytes(100000);
        FakeServer server([&](const FakeServer::Send& send) {
            send("HTTP/1.0 200 OK\r\nicy-name: Kept\r\n\r\n");
            send(body);
        });
        auto group = std::make_shared<PsyMP3::IO::HTTP::StreamAbortGroup>();
        std::unique_ptr<HTTPStreamIOHandler> h;
        {
            PsyMP3::IO::HTTP::StreamAbortGroup::Scope scope(group);
            h = std::make_unique<HTTPStreamIOHandler>(server.url());
        }
        group->detach();
        group->abort();
        ASSERT_FALSE(group->isAborted(), "a detached group ignores abort()");
        ASSERT_TRUE(readAll(*h) == body, "the stream still reads whole");
    }
};

/** @brief Quitting while the playing station is silent. */
class AbortAllTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    AbortAllTest() : TestCase("abortAll() releases a read waiting on a silent stream") {}
protected:
    /** @brief Block a read on a silent stream, call abortAll(), check it returns. */
    void runTest() override {
        std::atomic<bool> done{false};
        FakeServer server([&](const FakeServer::Send& send) {
            send("HTTP/1.0 200 OK\r\nicy-name: Silent\r\n\r\nabc");
            while (!done) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
        HTTPStreamIOHandler h(server.url());
        size_t got = 0;
        const auto start = std::chrono::steady_clock::now();
        std::thread reader([&] {
            char buf[64];
            got = h.read(buf, 1, sizeof buf);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        HTTPStreamIOHandler::abortAll();
        reader.join();
        done = true;
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        ASSERT_EQUALS(size_t(3), got, "the read returns what had arrived");
        ASSERT_TRUE(ms < 3000, "promptly");
    }
};

/** @brief What the handler sends. */
class RequestTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    RequestTest() : TestCase("One plain GET, no Range") {}
protected:
    /** @brief Check the request is a GET for the URL's path, without a Range header. */
    void runTest() override {
        FakeServer server([](const FakeServer::Send& send) {
            send("HTTP/1.0 200 OK\r\nicy-name: R\r\n\r\nabc");
        });
        {
            HTTPStreamIOHandler h(server.url());
            (void)readAll(h);
        }
        const std::string req = server.request();
        ASSERT_TRUE(req.compare(0, 12, "GET /stream ") == 0, "a GET for the path");
        std::string lower = req;
        for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        ASSERT_TRUE(lower.find("\r\nrange:") == std::string::npos, "no Range header");
    }
};

} // namespace

#endif // !_WIN32

/**
 * @brief Run the HTTPStreamIOHandler tests; the entry test_http_suite calls.
 * @return The number of failed tests, or 77 (skip) where there is no POSIX
 *         socket API for the fake server.
 */
int test_http_stream_io_handler_main()
{
#ifdef _WIN32
    return 77; // the fake server is POSIX sockets
#else
    // A server whose client has hung up must get an error, not a signal.
    ::signal(SIGPIPE, SIG_IGN);
    // These servers close the connection when their reply is done, which an
    // ICY stream would take for a dropped station and reconnect to; only
    // ReconnectTest wants that, and turns it on for itself.
    const int saved_reconnects = HTTPStreamIOHandler::reconnectAttempts();
    HTTPStreamIOHandler::setReconnectAttempts(0);

    TestFramework::TestSuite suite("HTTPStreamIOHandler Tests");
    suite.addTest(std::make_unique<IcecastStreamTest>());
    suite.addTest(std::make_unique<ShoutcastV1Test>());
    suite.addTest(std::make_unique<Http09BodyTest>());
    suite.addTest(std::make_unique<FileWithLengthTest>());
    suite.addTest(std::make_unique<ErrorStatusTest>());
    suite.addTest(std::make_unique<SeekWindowTest>());
    suite.addTest(std::make_unique<CloseWakesReadTest>());
    suite.addTest(std::make_unique<RequestTest>());
    suite.addTest(std::make_unique<MetadataParsingTest>());
    suite.addTest(std::make_unique<MetadataStreamTest>());
    suite.addTest(std::make_unique<MetadataRequestTest>());
    suite.addTest(std::make_unique<SplitTitleTest>());
    suite.addTest(std::make_unique<AbortGroupConnectTest>());
    suite.addTest(std::make_unique<AbortedGroupRefusesTest>());
    suite.addTest(std::make_unique<DetachedGroupTest>());
    suite.addTest(std::make_unique<AbortAllTest>());
    suite.addTest(std::make_unique<ReconnectTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    HTTPStreamIOHandler::setReconnectAttempts(saved_reconnects);
    return suite.getFailureCount(results);
#endif
}
