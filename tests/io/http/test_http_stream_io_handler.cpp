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
        FakeServer server([&](const FakeServer::Send& send) {
            // One byte per TCP send, so the block reaches the handler in
            // pieces and has to be put back together.
            for (char c : head) {
                if (!send(std::string(1, c))) return;
                std::this_thread::sleep_for(std::chrono::microseconds(200));
            }
            send(body);
        });
        HTTPStreamIOHandler h(server.url());
        ASSERT_TRUE(h.isIcy(), "an ICY v1 reply is ICY");
        ASSERT_EQUALS(std::string("Old Server"), h.getHeader("icy-name"), "header from the ICY block");
        ASSERT_EQUALS(std::string("8192"), h.getHeader("icy-metaint"), "icy-metaint from the ICY block");
        ASSERT_EQUALS(std::string("audio/aacp"), h.getMimeType(), "Content-Type from the ICY block");
        const std::string got = readAll(h);
        ASSERT_EQUALS(body.size(), got.size(), "the header block is not part of the body");
        ASSERT_TRUE(got == body, "body starts right after the blank line");
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

    TestFramework::TestSuite suite("HTTPStreamIOHandler Tests");
    suite.addTest(std::make_unique<IcecastStreamTest>());
    suite.addTest(std::make_unique<ShoutcastV1Test>());
    suite.addTest(std::make_unique<Http09BodyTest>());
    suite.addTest(std::make_unique<FileWithLengthTest>());
    suite.addTest(std::make_unique<ErrorStatusTest>());
    suite.addTest(std::make_unique<SeekWindowTest>());
    suite.addTest(std::make_unique<CloseWakesReadTest>());
    suite.addTest(std::make_unique<RequestTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
#endif
}
