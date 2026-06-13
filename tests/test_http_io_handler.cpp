#include "psymp3.h"
#include "test_framework.h"
#include <map>
#include <string>
#include <vector>
#include <iostream>

using namespace PsyMP3::IO::HTTP;
using namespace TestFramework;

// Mock responses for HTTPClient
namespace {
    std::map<std::string, HTTPClient::Response> mock_head_responses;
    std::map<std::string, HTTPClient::Response> mock_get_responses;
    int mock_network_error_status = 0;
}

namespace PsyMP3 {
namespace IO {
namespace HTTP {

HTTPClient::Response HTTPClient::head(const std::string& url, const std::map<std::string, std::string>& headers, int timeoutSeconds) {
    if (mock_network_error_status != 0) {
        return {mock_network_error_status, "Error", {}, "", false, false};
    }
    if (mock_head_responses.count(url)) {
        auto resp = mock_head_responses[url];
        resp.success = true;
        return resp;
    }
    return {404, "Not Found", {}, "", false, false};
}

HTTPClient::Response HTTPClient::get(const std::string& url, const std::map<std::string, std::string>& headers, int timeoutSeconds) {
    if (mock_network_error_status != 0) {
        return {mock_network_error_status, "Error", {}, "", false, false};
    }
    if (mock_get_responses.count(url)) {
        auto resp = mock_get_responses[url];
        resp.success = true;
        return resp;
    }
    return {404, "Not Found", {}, "", false, false};
}

HTTPClient::Response HTTPClient::getRange(const std::string& url, long start_byte, long end_byte, const std::map<std::string, std::string>& headers, int timeoutSeconds) {
    if (mock_network_error_status != 0) {
        return {mock_network_error_status, "Error", {}, "", false, false};
    }

    if (!mock_get_responses.count(url)) {
        return {404, "Not Found", {}, "", false, false};
    }

    auto& full_resp = mock_get_responses[url];
    if (start_byte < 0 || start_byte >= static_cast<long>(full_resp.body.size())) {
        return {416, "Range Not Satisfiable", {}, "", false, false};
    }

    long end = end_byte;
    if (end == -1 || end >= static_cast<long>(full_resp.body.size())) {
        end = full_resp.body.size() - 1;
    }

    HTTPClient::Response range_resp = full_resp;
    range_resp.statusCode = 206;
    range_resp.statusMessage = "Partial Content";
    range_resp.body = full_resp.body.substr(start_byte, end - start_byte + 1);
    range_resp.success = true;
    return range_resp;
}

// Stub out other methods since we don't test them or need them here
HTTPClient::Response HTTPClient::post(const std::string& url, const std::string& data, const std::string& contentType, const std::map<std::string, std::string>& headers, int timeoutSeconds) {
    return {500, "Not Implemented in Mock", {}, "", false, false};
}
std::string HTTPClient::urlEncode(const std::string& input) { return input; }
bool HTTPClient::parseURL(const std::string& url, std::string& host, int& port, std::string& path, bool& isHttps) { return true; }
void HTTPClient::closeAllConnections() {}
void HTTPClient::setConnectionTimeout(int timeout_seconds) {}
std::map<std::string, int> HTTPClient::getConnectionPoolStats() { return {}; }
void HTTPClient::initializeSSL() {}
void HTTPClient::cleanupSSL() {}
HTTPClient::Response HTTPClient::performRequest(const std::string& method, const std::string& url, const std::string& postData, const std::map<std::string, std::string>& headers, int timeoutSeconds) {
    return {500, "Not Implemented in Mock", {}, "", false, false};
}

} // namespace HTTP
} // namespace IO
} // namespace PsyMP3

class HTTPIOHandlerTest : public TestCase {
public:
    HTTPIOHandlerTest() : TestCase("HTTPIOHandler Tests") {}

    void setUp() override {
        mock_head_responses.clear();
        mock_get_responses.clear();
        mock_network_error_status = 0;

        // Setup standard mock responses
        std::map<std::string, std::string> headers1;
        headers1["Content-Length"] = "100";
        headers1["Content-Type"] = "audio/mpeg";
        headers1["Accept-Ranges"] = "bytes";

        mock_head_responses["http://example.com/test.mp3"] = {200, "OK", headers1, "", true, false};

        std::string body1(100, 'A'); // 100 bytes of 'A'
        mock_get_responses["http://example.com/test.mp3"] = {200, "OK", headers1, body1, true, false};

        // Ensure buffer pool has enough memory
        PsyMP3::IO::IOBufferPool::getInstance().setMaxPoolSize(1024 * 1024);
    }

protected:
    void runTest() override {
        testInitialization();
        testSeekAndRead();
        testNetworkErrorRecovery();
    }

    void testInitialization() {

        HTTPIOHandler handler("http://example.com/test.mp3");

        // Wait, HTTPIOHandler requires validateNetworkOperation("initialize").
        // But validateNetworkOperation checks m_initialized !
        // In initialize():
        // if (!validateNetworkOperation("initialize")) { return; }
        // BUT validateNetworkOperation returns false if !m_initialized
        // SO initialize() fails to initialize because it's not initialized!

        // Let's print out what validateNetworkOperation returns for initialize

        // Initialization should be triggered by constructor
        ASSERT_TRUE(handler.isInitialized(), "Handler should be initialized");
        ASSERT_EQUALS(100, handler.getContentLength(), "Content length should be parsed");
        ASSERT_EQUALS("audio/mpeg", handler.getMimeType(), "MIME type should be parsed");
        ASSERT_TRUE(handler.supportsRangeRequests(), "Range requests should be supported");
    }

    void testSeekAndRead() {
        HTTPIOHandler handler("http://example.com/test.mp3");

        char buffer[10];

        // Seek to 10
        ASSERT_EQUALS(0, handler.seek(10, SEEK_SET), "Seek should succeed");
        ASSERT_EQUALS(10, handler.tell(), "Tell should return 10");

        // Read 5 bytes
        size_t read_bytes = handler.read(buffer, 1, 5);
        ASSERT_EQUALS(5, read_bytes, "Should read 5 bytes");
        ASSERT_EQUALS(15, handler.tell(), "Position should be 15 after read");

        // Buffer should contain 'A'
        for(int i=0; i<5; i++) {
            ASSERT_EQUALS('A', buffer[i], "Buffer content should match mock");
        }
    }

    void testNetworkErrorRecovery() {
        // First head fails
        mock_network_error_status = 503; // Service Unavailable

        // Test fast failure since it's hard to test actual backoff/retry in synchronous tests
        // Actually we can just override max_retries or timeout?
        HTTPIOHandler handler("http://example.com/error.mp3");

        ASSERT_FALSE(handler.isInitialized(), "Handler should not initialize on permanent error");

        mock_network_error_status = 0; // reset
    }
};

int main() {
    TestSuite suite("HTTPIOHandler Test Suite");
    suite.addTest(std::make_unique<HTTPIOHandlerTest>());

    auto results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
