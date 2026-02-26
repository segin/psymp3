/*
 * test_http_client_parse_url.cpp - Unit tests for HTTPClient::parseURL
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include "io/http/HTTPClient.h"

using namespace PsyMP3::IO::HTTP;
using namespace TestFramework;

class HTTPClientParseURLTest : public TestCase {
public:
    HTTPClientParseURLTest() : TestCase("HTTPClient::parseURL") {}

protected:
    void runTest() override {
        std::string host, path;
        int port;
        bool isHttps;

        // 1. Happy path (http)
        ASSERT_TRUE(HTTPClient::parseURL("http://example.com/path", host, port, path, isHttps), "Basic http URL should be parsed");
        ASSERT_EQUALS("example.com", host, "Host should be example.com");
        ASSERT_EQUALS(80, port, "Default http port should be 80");
        ASSERT_EQUALS("/path", path, "Path should be /path");
        ASSERT_FALSE(isHttps, "isHttps should be false for http");

        // 2. Happy path (https)
        ASSERT_TRUE(HTTPClient::parseURL("https://example.org/another/path", host, port, path, isHttps), "Basic https URL should be parsed");
        ASSERT_EQUALS("example.org", host, "Host should be example.org");
        ASSERT_EQUALS(443, port, "Default https port should be 443");
        ASSERT_EQUALS("/another/path", path, "Path should be /another/path");
        ASSERT_TRUE(isHttps, "isHttps should be true for https");

        // 3. Explicit port
        ASSERT_TRUE(HTTPClient::parseURL("http://localhost:8080/api", host, port, path, isHttps), "URL with explicit port should be parsed");
        ASSERT_EQUALS("localhost", host, "Host should be localhost");
        ASSERT_EQUALS(8080, port, "Port should be 8080");
        ASSERT_EQUALS("/api", path, "Path should be /api");

        // 4. No path
        ASSERT_TRUE(HTTPClient::parseURL("http://example.com", host, port, path, isHttps), "URL with no path should be parsed");
        ASSERT_EQUALS("example.com", host, "Host should be example.com");
        ASSERT_EQUALS("/", path, "Empty path should default to /");

        // 5. No path with trailing slash
        ASSERT_TRUE(HTTPClient::parseURL("https://example.com/", host, port, path, isHttps), "URL with trailing slash should be parsed");
        ASSERT_EQUALS("example.com", host, "Host should be example.com");
        ASSERT_EQUALS("/", path, "Path should be /");

        // 6. IPv4 host
        ASSERT_TRUE(HTTPClient::parseURL("http://127.0.0.1/test", host, port, path, isHttps), "URL with IPv4 host should be parsed");
        ASSERT_EQUALS("127.0.0.1", host, "Host should be 127.0.0.1");

        // 7. Complex path and query
        ASSERT_TRUE(HTTPClient::parseURL("https://example.com/search?q=test&v=1#hash", host, port, path, isHttps), "URL with query and hash should be parsed");
        ASSERT_EQUALS("/search?q=test&v=1#hash", path, "Path should include query and hash");

        // 8. Invalid scheme
        ASSERT_FALSE(HTTPClient::parseURL("ftp://example.com/file", host, port, path, isHttps), "Unsupported scheme should return false");

        // 9. Missing ://
        ASSERT_FALSE(HTTPClient::parseURL("example.com/path", host, port, path, isHttps), "URL missing :// should return false");

        // 10. Empty host
        ASSERT_FALSE(HTTPClient::parseURL("http:///path", host, port, path, isHttps), "URL with empty host should return false");

        // 11. Invalid port (non-numeric)
        ASSERT_FALSE(HTTPClient::parseURL("http://example.com:abc/path", host, port, path, isHttps), "URL with non-numeric port should return false");

        // 12. Invalid port (empty after colon)
        ASSERT_FALSE(HTTPClient::parseURL("http://example.com:/path", host, port, path, isHttps), "URL with empty port after colon should return false");

        // 13. Port limits (0)
        ASSERT_TRUE(HTTPClient::parseURL("http://example.com:0/path", host, port, path, isHttps), "URL with port 0 should be parsed");
        ASSERT_EQUALS(0, port, "Port should be 0");

        // 14. Port limits (65535)
        ASSERT_TRUE(HTTPClient::parseURL("http://example.com:65535/path", host, port, path, isHttps), "URL with port 65535 should be parsed");
        ASSERT_EQUALS(65535, port, "Port should be 65535");

        // 15. Port overflow (valid integer but invalid port, currently accepted by parser logic as it just does stoi)
        // Ideally this should fail if logic was stricter, but we test current behavior.
        ASSERT_TRUE(HTTPClient::parseURL("http://example.com:65536/path", host, port, path, isHttps), "URL with port 65536 should be parsed (current behavior)");
        ASSERT_EQUALS(65536, port, "Port should be 65536");

        // 16. Basic Auth (currently fails)
        ASSERT_FALSE(HTTPClient::parseURL("http://user:pass@example.com/path", host, port, path, isHttps), "Basic Auth URLs are not currently supported");

        // 17. IPv6 (currently fails)
        ASSERT_FALSE(HTTPClient::parseURL("http://[::1]:8080/path", host, port, path, isHttps), "IPv6 URLs are not currently supported");

        // 18. Empty scheme
        ASSERT_FALSE(HTTPClient::parseURL("://example.com", host, port, path, isHttps), "Empty scheme should fail");
    }
};

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    TestSuite suite("HTTPClient::parseURL Unit Tests");
    suite.addTest(std::make_unique<HTTPClientParseURLTest>());

    auto results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
