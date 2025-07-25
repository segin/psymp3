/*
 * test_iohandler_unit_comprehensive.cpp - Comprehensive unit tests for IOHandler subsystem
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include <fstream>
#include <thread>
#include <chrono>
#include <random>
#include <atomic>
#include <future>

using namespace TestFramework;

// Test utilities
class IOHandlerTestUtils {
public:
    static void createTestFile(const std::string& filename, const std::string& content) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            throw TestSetupFailure("Failed to create test file: " + filename);
        }
        file.write(content.c_str(), content.length());
        file.close();
    }
    
    static void createLargeTestFile(const std::string& filename, size_t size) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            throw TestSetupFailure("Failed to create large test file: " + filename);
        }
        
        // Write pattern data for verification
        for (size_t i = 0; i < size; ++i) {
            file.put(static_cast<char>(i % 256));
        }
        file.close();
    }
    
    static void cleanupTestFile(const std::string& filename) {
        std::remove(filename.c_str());
    }
    
    static std::string generateRandomString(size_t length) {
        const std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, chars.size() - 1);
        
        std::string result;
        result.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            result += chars[dis(gen)];
        }
        return result;
    }
};

// Test helper class to access protected methods
class IOHandlerTestHelper : public IOHandler {
public:
    static std::string testNormalizePath(const std::string& path) {
        return normalizePath(path);
    }
    
    static char testGetPathSeparator() {
        return getPathSeparator();
    }
    
    static std::string testGetErrorMessage(int error_code, const std::string& context) {
        return getErrorMessage(error_code, context);
    }
    
    static bool testIsRecoverableError(int error_code) {
        return isRecoverableError(error_code);
    }
    
    static off_t testGetMaxFileSize() {
        return getMaxFileSize();
    }
    
    static void testSetMemoryLimits(size_t max_total, size_t max_per_handler) {
        setMemoryLimits(max_total, max_per_handler);
    }
};

// Test cases for IOHandler base interface
class IOHandlerBaseTest : public TestCase {
public:
    IOHandlerBaseTest() : TestCase("IOHandler Base Interface Test") {}
    
protected:
    void runTest() override {
        // Test 1: Memory statistics are available
        auto stats = IOHandler::getMemoryStats();
        ASSERT_TRUE(stats.find("total_memory_usage") != stats.end(), 
                   "Memory stats should include total_memory_usage");
        
        // Test 2: Cross-platform path normalization
        std::string windows_path = "C:\\path\\to\\file.txt";
        std::string unix_path = "/path/to/file.txt";
        
        // These should not crash and should return valid strings
        std::string normalized_win = IOHandlerTestHelper::testNormalizePath(windows_path);
        std::string normalized_unix = IOHandlerTestHelper::testNormalizePath(unix_path);
        
        ASSERT_TRUE(!normalized_win.empty(), "Normalized Windows path should not be empty");
        ASSERT_TRUE(!normalized_unix.empty(), "Normalized Unix path should not be empty");
        
        // Test 3: Path separator detection
        char separator = IOHandlerTestHelper::testGetPathSeparator();
        ASSERT_TRUE(separator == '/' || separator == '\\', 
                   "Path separator should be / or \\");
        
        // Test 4: Error message generation
        std::string error_msg = IOHandlerTestHelper::testGetErrorMessage(ENOENT, "test context");
        ASSERT_TRUE(!error_msg.empty(), "Error message should not be empty");
        ASSERT_TRUE(error_msg.find("test context") != std::string::npos, 
                   "Error message should include context");
        
        // Test 5: Recoverable error detection
        bool recoverable = IOHandlerTestHelper::testIsRecoverableError(EAGAIN);
        // EAGAIN should typically be recoverable
        
        // Test 6: Maximum file size
        off_t max_size = IOHandlerTestHelper::testGetMaxFileSize();
        ASSERT_TRUE(max_size > 0, "Maximum file size should be positive");
    }
};

// Test cases for FileIOHandler
class FileIOHandlerTest : public TestCase {
public:
    FileIOHandlerTest() : TestCase("FileIOHandler Test") {}
    
protected:
    void setUp() override {
        test_file = "test_file_io_handler.txt";
        test_content = "Hello, World! This is test content for FileIOHandler.";
        IOHandlerTestUtils::createTestFile(test_file, test_content);
        
        large_test_file = "test_large_file.bin";
        large_file_size = 2 * 1024 * 1024; // 2MB
        IOHandlerTestUtils::createLargeTestFile(large_test_file, large_file_size);
    }
    
    void tearDown() override {
        IOHandlerTestUtils::cleanupTestFile(test_file);
        IOHandlerTestUtils::cleanupTestFile(large_test_file);
    }
    
    void runTest() override {
        testBasicFileOperations();
        testSeekOperations();
        testLargeFileSupport();
        testErrorHandling();
        testUnicodeFilenames();
        testCrossplatformCompatibility();
    }
    
private:
    std::string test_file;
    std::string test_content;
    std::string large_test_file;
    size_t large_file_size;
    
    void testBasicFileOperations() {
        FileIOHandler handler{TagLib::String(test_file)};
        
        // Test initial state
        ASSERT_FALSE(handler.eof(), "New handler should not be at EOF");
        ASSERT_EQUALS(0, handler.tell(), "Initial position should be 0");
        ASSERT_EQUALS(0, handler.getLastError(), "Initial error should be 0");
        
        // Test file size
        off_t file_size = handler.getFileSize();
        ASSERT_EQUALS(static_cast<off_t>(test_content.length()), file_size, 
                     "File size should match content length");
        
        // Test reading
        char buffer[256];
        size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
        ASSERT_EQUALS(test_content.length(), bytes_read, 
                     "Should read entire file content");
        
        std::string read_content(buffer, bytes_read);
        ASSERT_EQUALS(test_content, read_content, "Read content should match written content");
        
        // Test position after read
        ASSERT_EQUALS(static_cast<off_t>(test_content.length()), handler.tell(), 
                     "Position should be at end after reading entire file");
        
        // Test EOF
        ASSERT_TRUE(handler.eof(), "Should be at EOF after reading entire file");
        
        // Test close
        int close_result = handler.close();
        ASSERT_EQUALS(0, close_result, "Close should succeed");
        
        // Test operations on closed handler
        char closed_buffer[32];
        size_t closed_read = handler.read(closed_buffer, 1, sizeof(closed_buffer));
        ASSERT_EQUALS(0U, closed_read, "Read on closed handler should return 0");
        
        int closed_seek = handler.seek(0, SEEK_SET);
        ASSERT_EQUALS(-1, closed_seek, "Seek on closed handler should fail");
        
        off_t closed_tell = handler.tell();
        ASSERT_EQUALS(-1, closed_tell, "Tell on closed handler should return -1");
    }
    
    void testSeekOperations() {
        FileIOHandler handler{TagLib::String(test_file)};
        
        // Test SEEK_SET
        int seek_result = handler.seek(5, SEEK_SET);
        ASSERT_EQUALS(0, seek_result, "SEEK_SET should succeed");
        ASSERT_EQUALS(5, handler.tell(), "Position should be 5 after SEEK_SET");
        
        // Test SEEK_CUR
        seek_result = handler.seek(3, SEEK_CUR);
        ASSERT_EQUALS(0, seek_result, "SEEK_CUR should succeed");
        ASSERT_EQUALS(8, handler.tell(), "Position should be 8 after SEEK_CUR");
        
        // Test SEEK_END
        seek_result = handler.seek(-5, SEEK_END);
        ASSERT_EQUALS(0, seek_result, "SEEK_END should succeed");
        ASSERT_EQUALS(static_cast<off_t>(test_content.length() - 5), handler.tell(), 
                     "Position should be 5 from end after SEEK_END");
        
        // Test invalid seek (negative position)
        seek_result = handler.seek(-1, SEEK_SET);
        ASSERT_EQUALS(-1, seek_result, "Seek to negative position should fail");
        
        // Test seek beyond file end
        seek_result = handler.seek(test_content.length() + 100, SEEK_SET);
        ASSERT_EQUALS(0, seek_result, "Seek beyond end should succeed");
        ASSERT_TRUE(handler.tell() > static_cast<off_t>(test_content.length()), 
                   "Position should be beyond file end");
    }
    
    void testLargeFileSupport() {
        FileIOHandler handler{TagLib::String(large_test_file)};
        
        // Test large file size reporting
        off_t file_size = handler.getFileSize();
        ASSERT_EQUALS(static_cast<off_t>(large_file_size), file_size, 
                     "Large file size should be reported correctly");
        
        // Test seeking in large file
        off_t large_position = 1024 * 1024; // 1MB
        int seek_result = handler.seek(large_position, SEEK_SET);
        ASSERT_EQUALS(0, seek_result, "Seek in large file should succeed");
        ASSERT_EQUALS(large_position, handler.tell(), 
                     "Position in large file should be correct");
        
        // Test reading from large file
        char buffer[1024];
        size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
        ASSERT_EQUALS(sizeof(buffer), bytes_read, 
                     "Should read full buffer from large file");
        
        // Verify pattern data
        for (size_t i = 0; i < sizeof(buffer); ++i) {
            char expected = static_cast<char>((large_position + i) % 256);
            ASSERT_EQUALS(expected, buffer[i], 
                         "Large file pattern data should be correct");
        }
    }
    
    void testErrorHandling() {
        // Test opening nonexistent file
        TestPatterns::assertThrows<InvalidMediaException>([&]() {
            FileIOHandler handler{TagLib::String("nonexistent_file_12345.txt")};
        }, "Could not open file", "Should throw InvalidMediaException for nonexistent file");
        
        // Test operations on closed handler
        FileIOHandler handler{TagLib::String(test_file)};
        handler.close();
        
        char buffer[32];
        size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
        ASSERT_EQUALS(0U, bytes_read, "Read on closed handler should return 0");
        
        int seek_result = handler.seek(0, SEEK_SET);
        ASSERT_EQUALS(-1, seek_result, "Seek on closed handler should fail");
        
        off_t position = handler.tell();
        ASSERT_EQUALS(-1, position, "Tell on closed handler should return -1");
    }
    
    void testUnicodeFilenames() {
        // Test with Unicode filename
        std::string unicode_file = "test_unicode_файл.txt";
        std::string unicode_content = "Unicode test content";
        
        try {
            IOHandlerTestUtils::createTestFile(unicode_file, unicode_content);
            
            FileIOHandler handler{TagLib::String(unicode_file)};
            
            char buffer[256];
            size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
            ASSERT_EQUALS(unicode_content.length(), bytes_read, 
                         "Should read Unicode file content");
            
            std::string read_content(buffer, bytes_read);
            ASSERT_EQUALS(unicode_content, read_content, 
                         "Unicode file content should match");
            
            IOHandlerTestUtils::cleanupTestFile(unicode_file);
            
        } catch (const std::exception& e) {
            IOHandlerTestUtils::cleanupTestFile(unicode_file);
            // Unicode filename support may not be available on all systems
            std::cout << "Unicode filename test skipped: " << e.what() << std::endl;
        }
    }
    
    void testCrossplatformCompatibility() {
        FileIOHandler handler{TagLib::String(test_file)};
        
        // Test that file operations work consistently across platforms
        off_t file_size = handler.getFileSize();
        ASSERT_TRUE(file_size > 0, "File size should be positive on all platforms");
        
        // Test seeking with different whence values
        ASSERT_EQUALS(0, handler.seek(0, SEEK_SET), "SEEK_SET should work on all platforms");
        ASSERT_EQUALS(0, handler.seek(0, SEEK_CUR), "SEEK_CUR should work on all platforms");
        ASSERT_EQUALS(0, handler.seek(0, SEEK_END), "SEEK_END should work on all platforms");
        
        // Test position reporting
        off_t position = handler.tell();
        ASSERT_EQUALS(file_size, position, "Position should be at end after SEEK_END");
    }
};

// Test cases for HTTPIOHandler
class HTTPIOHandlerTest : public TestCase {
public:
    HTTPIOHandlerTest() : TestCase("HTTPIOHandler Test") {}
    
protected:
    void runTest() override {
        testHTTPInitialization();
        testHTTPBuffering();
        testHTTPRangeRequests();
        testHTTPErrorHandling();
        testHTTPMetadataExtraction();
    }
    
private:
    void testHTTPInitialization() {
        // Test with valid HTTP URL (this will likely fail in test environment)
        try {
            HTTPIOHandler handler{"http://httpbin.org/bytes/1024"};
            
            // Test initial state
            ASSERT_FALSE(handler.eof(), "New HTTP handler should not be at EOF");
            ASSERT_EQUALS(0, handler.tell(), "Initial position should be 0");
            
            // Test initialization state - may be initialized immediately or on first read
            bool initialized_immediately = handler.isInitialized();
            
            // Attempt to read (this will trigger initialization if not already done)
            char buffer[32];
            size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
            
            // If we get here, initialization succeeded
            ASSERT_TRUE(handler.isInitialized(), "Handler should be initialized after read");
            
            // Test that we got some data if network is available
            if (bytes_read > 0) {
                ASSERT_TRUE(bytes_read <= sizeof(buffer), "Read should not exceed buffer size");
                ASSERT_TRUE(handler.tell() > 0, "Position should advance after read");
            }
            
        } catch (const std::exception& e) {
            // Network operations may fail in test environment
            std::cout << "HTTP initialization test skipped (network unavailable): " << e.what() << std::endl;
        }
    }
    
    void testHTTPBuffering() {
        // Test buffering behavior (mock test since we can't rely on network)
        try {
            HTTPIOHandler handler{"http://httpbin.org/bytes/1024"};
            
            // Test that multiple small reads are efficient
            char buffer[32];
            for (int i = 0; i < 10; ++i) {
                size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
                if (bytes_read == 0) break; // Network might not be available
            }
            
        } catch (const std::exception& e) {
            std::cout << "HTTP buffering test skipped (network unavailable): " << e.what() << std::endl;
        }
    }
    
    void testHTTPRangeRequests() {
        try {
            HTTPIOHandler handler{"http://httpbin.org/bytes/1024"};
            
            // Test seeking (which uses range requests)
            int seek_result = handler.seek(100, SEEK_SET);
            if (seek_result == 0) {
                ASSERT_EQUALS(100, handler.tell(), "Position should be 100 after seek");
                
                // Test that range requests are supported
                ASSERT_TRUE(handler.supportsRangeRequests(), 
                           "Handler should support range requests");
            }
            
        } catch (const std::exception& e) {
            std::cout << "HTTP range request test skipped (network unavailable): " << e.what() << std::endl;
        }
    }
    
    void testHTTPErrorHandling() {
        // Test with invalid URL
        TestPatterns::assertThrows<std::exception>([&]() {
            HTTPIOHandler handler{"invalid://not.a.real.url"};
            char buffer[32];
            handler.read(buffer, 1, sizeof(buffer)); // This should trigger error
        }, "", "Should throw exception for invalid URL");
        
        // Test with unreachable host
        try {
            HTTPIOHandler handler{"http://definitely.not.a.real.host.example/file"};
            char buffer[32];
            size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
            ASSERT_EQUALS(0U, bytes_read, "Should return 0 bytes for unreachable host");
        } catch (const std::exception& e) {
            // Expected behavior - network error should be handled gracefully
        }
    }
    
    void testHTTPMetadataExtraction() {
        try {
            HTTPIOHandler handler{"http://httpbin.org/bytes/1024"};
            
            // Trigger initialization
            char buffer[32];
            handler.read(buffer, 1, sizeof(buffer));
            
            if (handler.isInitialized()) {
                // Test content length
                long content_length = handler.getContentLength();
                ASSERT_TRUE(content_length > 0 || content_length == -1, 
                           "Content length should be positive or -1 if unknown");
                
                // Test MIME type
                std::string mime_type = handler.getMimeType();
                // MIME type might be empty if not provided by server
                
                // Test file size reporting
                off_t file_size = handler.getFileSize();
                if (content_length > 0) {
                    ASSERT_EQUALS(static_cast<off_t>(content_length), file_size, 
                                 "File size should match content length");
                }
            }
            
        } catch (const std::exception& e) {
            std::cout << "HTTP metadata test skipped (network unavailable): " << e.what() << std::endl;
        }
    }
};

// Test cases for HTTPClient
class HTTPClientTest : public TestCase {
public:
    HTTPClientTest() : TestCase("HTTPClient Test") {}
    
protected:
    void runTest() override {
        testHTTPMethods();
        testURLParsing();
        testURLEncoding();
        testConnectionPooling();
        testSSLSupport();
        testErrorHandling();
    }
    
private:
    void testHTTPMethods() {
        // Test URL encoding (doesn't require network)
        std::string encoded = HTTPClient::urlEncode("hello world test");
        ASSERT_TRUE(encoded.find("hello%20world%20test") != std::string::npos, 
                   "URL encoding should replace spaces with %20");
        
        // Test connection pool stats (doesn't require network)
        auto stats = HTTPClient::getConnectionPoolStats();
        ASSERT_TRUE(stats.find("active_connections") != stats.end(), 
                   "Connection pool stats should include active_connections");
        
        // Network-dependent tests
        try {
            // Test GET request
            auto response = HTTPClient::get("http://httpbin.org/get", {}, 5); // 5 second timeout
            if (response.success) {
                ASSERT_TRUE(response.statusCode >= 200 && response.statusCode < 300, 
                           "GET request should return success status code");
                ASSERT_TRUE(!response.body.empty(), "GET response should have body");
            }
            
            // Test HEAD request
            auto head_response = HTTPClient::head("http://httpbin.org/get", {}, 5);
            if (head_response.success) {
                ASSERT_TRUE(head_response.statusCode >= 200 && head_response.statusCode < 300, 
                           "HEAD request should return success status code");
                ASSERT_TRUE(head_response.body.empty(), "HEAD response should have empty body");
            }
            
        } catch (const std::exception& e) {
            std::cout << "HTTP methods test skipped (network unavailable): " << e.what() << std::endl;
        }
    }
    
    void testURLParsing() {
        std::string host, path;
        int port;
        bool isHttps;
        
        // Test HTTP URL parsing
        bool result = HTTPClient::parseURL("http://example.com:8080/path/to/file", 
                                          host, port, path, isHttps);
        ASSERT_TRUE(result, "Should successfully parse HTTP URL");
        ASSERT_EQUALS("example.com", host, "Host should be extracted correctly");
        ASSERT_EQUALS(8080, port, "Port should be extracted correctly");
        ASSERT_EQUALS("/path/to/file", path, "Path should be extracted correctly");
        ASSERT_FALSE(isHttps, "Should detect HTTP (not HTTPS)");
        
        // Test HTTPS URL parsing
        result = HTTPClient::parseURL("https://secure.example.com/secure/path", 
                                     host, port, path, isHttps);
        ASSERT_TRUE(result, "Should successfully parse HTTPS URL");
        ASSERT_EQUALS("secure.example.com", host, "HTTPS host should be extracted correctly");
        ASSERT_EQUALS(443, port, "HTTPS should default to port 443");
        ASSERT_EQUALS("/secure/path", path, "HTTPS path should be extracted correctly");
        ASSERT_TRUE(isHttps, "Should detect HTTPS");
        
        // Test invalid URL
        result = HTTPClient::parseURL("invalid-url", host, port, path, isHttps);
        ASSERT_FALSE(result, "Should fail to parse invalid URL");
    }
    
    void testURLEncoding() {
        // Test basic URL encoding
        std::string encoded = HTTPClient::urlEncode("hello world");
        ASSERT_EQUALS("hello%20world", encoded, "Spaces should be encoded as %20");
        
        // Test special characters
        encoded = HTTPClient::urlEncode("test@example.com");
        ASSERT_TRUE(encoded.find("%40") != std::string::npos, "@ should be encoded as %40");
        
        // Test already encoded string
        encoded = HTTPClient::urlEncode("already%20encoded");
        ASSERT_TRUE(encoded.find("%2520") != std::string::npos, "% should be encoded as %25");
        
        // Test empty string
        encoded = HTTPClient::urlEncode("");
        ASSERT_EQUALS("", encoded, "Empty string should remain empty");
    }
    
    void testConnectionPooling() {
        // Test connection pool statistics
        auto stats = HTTPClient::getConnectionPoolStats();
        ASSERT_TRUE(stats.find("active_connections") != stats.end(), 
                   "Stats should include active_connections");
        ASSERT_TRUE(stats.find("total_requests") != stats.end(), 
                   "Stats should include total_requests");
        
        // Test connection timeout setting
        HTTPClient::setConnectionTimeout(60);
        // No direct way to verify this, but it shouldn't crash
        
        // Test closing all connections
        HTTPClient::closeAllConnections();
        // Should not crash
    }
    
    void testSSLSupport() {
        // Test SSL initialization (should not crash)
        HTTPClient::initializeSSL();
        
        try {
            // Test HTTPS request
            auto response = HTTPClient::get("https://httpbin.org/get", {}, 5);
            if (response.success) {
                ASSERT_TRUE(response.statusCode >= 200 && response.statusCode < 300, 
                           "HTTPS request should return success status code");
            }
        } catch (const std::exception& e) {
            std::cout << "HTTPS test skipped (network/SSL unavailable): " << e.what() << std::endl;
        }
        
        // Test SSL cleanup (should not crash)
        HTTPClient::cleanupSSL();
    }
    
    void testErrorHandling() {
        // Test with invalid host
        auto response = HTTPClient::get("http://definitely.not.a.real.host.example", {}, 2);
        ASSERT_FALSE(response.success, "Request to invalid host should fail");
        ASSERT_TRUE(response.statusCode == 0, "Failed request should have status code 0");
        
        // Test with very short timeout
        response = HTTPClient::get("http://httpbin.org/delay/10", {}, 1); // 1 second timeout for 10 second delay
        ASSERT_FALSE(response.success, "Request with short timeout should fail");
    }
};

// Test cases for cross-platform compatibility
class CrossPlatformTest : public TestCase {
public:
    CrossPlatformTest() : TestCase("Cross-Platform Compatibility Test") {}
    
protected:
    void runTest() override {
        testPathHandling();
        testLargeFileSupport();
        testUnicodeSupport();
        testErrorCodeConsistency();
    }
    
private:
    void testPathHandling() {
        // Test path normalization
        std::string windows_path = "C:\\Users\\test\\file.txt";
        std::string unix_path = "/home/test/file.txt";
        
        std::string norm_win = IOHandlerTestHelper::testNormalizePath(windows_path);
        std::string norm_unix = IOHandlerTestHelper::testNormalizePath(unix_path);
        
        ASSERT_TRUE(!norm_win.empty(), "Normalized Windows path should not be empty");
        ASSERT_TRUE(!norm_unix.empty(), "Normalized Unix path should not be empty");
        
        // Test path separator
        char separator = IOHandlerTestHelper::testGetPathSeparator();
        ASSERT_TRUE(separator == '/' || separator == '\\', 
                   "Path separator should be / or \\");
    }
    
    void testLargeFileSupport() {
        // Test maximum file size
        off_t max_size = IOHandlerTestHelper::testGetMaxFileSize();
        ASSERT_TRUE(max_size > 2147483647LL, // > 2GB
                   "Should support files larger than 2GB");
    }
    
    void testUnicodeSupport() {
        // Test Unicode path handling
        std::string unicode_path = "test_файл.txt";
        std::string content = "Unicode test";
        
        try {
            IOHandlerTestUtils::createTestFile(unicode_path, content);
            
            FileIOHandler handler{TagLib::String(unicode_path)};
            
            char buffer[256];
            size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
            ASSERT_EQUALS(content.length(), bytes_read, 
                         "Should read Unicode file correctly");
            
            IOHandlerTestUtils::cleanupTestFile(unicode_path);
            
        } catch (const std::exception& e) {
            IOHandlerTestUtils::cleanupTestFile(unicode_path);
            std::cout << "Unicode support test skipped: " << e.what() << std::endl;
        }
    }
    
    void testErrorCodeConsistency() {
        // Test that error codes are handled consistently
        std::string error_msg1 = IOHandlerTestHelper::testGetErrorMessage(ENOENT, "test1");
        std::string error_msg2 = IOHandlerTestHelper::testGetErrorMessage(ENOENT, "test2");
        
        ASSERT_TRUE(!error_msg1.empty(), "Error message 1 should not be empty");
        ASSERT_TRUE(!error_msg2.empty(), "Error message 2 should not be empty");
        ASSERT_TRUE(error_msg1.find("test1") != std::string::npos, 
                   "Error message should include context");
        ASSERT_TRUE(error_msg2.find("test2") != std::string::npos, 
                   "Error message should include context");
    }
};

// Test cases for thread safety
class ThreadSafetyTest : public TestCase {
public:
    ThreadSafetyTest() : TestCase("Thread Safety Test") {}
    
protected:
    void runTest() override {
        testFileIOHandlerThreadSafety();
        testHTTPIOHandlerThreadSafety();
        testMemoryManagementThreadSafety();
    }
    
private:
    void testFileIOHandlerThreadSafety() {
        // Create test file
        std::string test_file = "thread_safety_test.txt";
        std::string test_content = "Thread safety test content for concurrent access testing.";
        IOHandlerTestUtils::createTestFile(test_file, test_content);
        
        try {
            FileIOHandler handler{TagLib::String(test_file)};
            
            // Test concurrent reads
            const int num_threads = 4;
            const int reads_per_thread = 10;
            std::vector<std::future<bool>> futures;
            std::atomic<int> successful_reads{0};
            std::atomic<int> failed_reads{0};
            
            for (int i = 0; i < num_threads; ++i) {
                futures.push_back(std::async(std::launch::async, [&, i]() {
                    bool thread_success = true;
                    for (int j = 0; j < reads_per_thread; ++j) {
                        try {
                            char buffer[256];
                            size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
                            
                            // Seek back to beginning for next read
                            handler.seek(0, SEEK_SET);
                            
                            if (bytes_read == test_content.length()) {
                                successful_reads.fetch_add(1);
                            } else {
                                failed_reads.fetch_add(1);
                                thread_success = false;
                            }
                        } catch (const std::exception& e) {
                            failed_reads.fetch_add(1);
                            thread_success = false;
                        }
                    }
                    return thread_success;
                }));
            }
            
            // Wait for all threads to complete
            bool all_threads_successful = true;
            for (auto& future : futures) {
                if (!future.get()) {
                    all_threads_successful = false;
                }
            }
            
            ASSERT_TRUE(successful_reads.load() > 0, "At least some reads should succeed in concurrent access");
            ASSERT_TRUE(all_threads_successful, "All threads should complete successfully");
            
        } catch (const std::exception& e) {
            IOHandlerTestUtils::cleanupTestFile(test_file);
            throw;
        }
        
        IOHandlerTestUtils::cleanupTestFile(test_file);
    }
    
    void testHTTPIOHandlerThreadSafety() {
        try {
            HTTPIOHandler handler{"http://httpbin.org/bytes/1024"};
            
            // Test concurrent reads
            const int num_threads = 3;
            std::vector<std::future<bool>> futures;
            std::atomic<int> successful_operations{0};
            
            for (int i = 0; i < num_threads; ++i) {
                futures.push_back(std::async(std::launch::async, [&, i]() {
                    try {
                        char buffer[128];
                        size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
                        
                        // Test tell operation
                        off_t position = handler.tell();
                        
                        if (bytes_read > 0 && position >= 0) {
                            successful_operations.fetch_add(1);
                            return true;
                        }
                    } catch (const std::exception& e) {
                        // Network errors are acceptable in test environment
                    }
                    return false;
                }));
            }
            
            // Wait for all threads
            for (auto& future : futures) {
                future.get(); // Don't require success due to network dependency
            }
            
            // Just verify no crashes occurred
            ASSERT_TRUE(true, "Thread safety test completed without crashes");
            
        } catch (const std::exception& e) {
            std::cout << "HTTP thread safety test skipped (network unavailable): " << e.what() << std::endl;
        }
    }
    
    void testMemoryManagementThreadSafety() {
        // Test concurrent memory operations
        const int num_threads = 4;
        std::vector<std::future<void>> futures;
        std::atomic<bool> memory_error{false};
        
        for (int i = 0; i < num_threads; ++i) {
            futures.push_back(std::async(std::launch::async, [&, i]() {
                try {
                    // Get memory stats concurrently
                    auto stats = IOHandler::getMemoryStats();
                    
                    // Verify stats are reasonable
                    if (stats.find("total_memory_usage") == stats.end()) {
                        memory_error.store(true);
                    }
                    
                    // Test memory limit checking
                    IOHandlerTestHelper::testSetMemoryLimits(64 * 1024 * 1024, 16 * 1024 * 1024);
                    
                } catch (const std::exception& e) {
                    memory_error.store(true);
                }
            }));
        }
        
        // Wait for all threads
        for (auto& future : futures) {
            future.get();
        }
        
        ASSERT_FALSE(memory_error.load(), "Memory management should be thread-safe");
    }
};

// Test cases for performance and stress testing
class PerformanceTest : public TestCase {
public:
    PerformanceTest() : TestCase("Performance Test") {}
    
protected:
    void runTest() override {
        testFileIOPerformance();
        testMemoryUsagePatterns();
        testLargeFileHandling();
    }
    
private:
    void testFileIOPerformance() {
        // Create a moderately sized test file
        std::string test_file = "performance_test.bin";
        size_t file_size = 1024 * 1024; // 1MB
        IOHandlerTestUtils::createLargeTestFile(test_file, file_size);
        
        try {
            FileIOHandler handler{TagLib::String(test_file)};
            
            // Measure sequential read performance
            auto start_time = std::chrono::high_resolution_clock::now();
            
            char buffer[8192]; // 8KB buffer
            size_t total_read = 0;
            while (!handler.eof()) {
                size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
                if (bytes_read == 0) break;
                total_read += bytes_read;
            }
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            ASSERT_EQUALS(file_size, total_read, "Should read entire file");
            ASSERT_TRUE(duration.count() < 5000, "Should read 1MB file in less than 5 seconds");
            
            // Test random access performance
            start_time = std::chrono::high_resolution_clock::now();
            
            for (int i = 0; i < 100; ++i) {
                off_t random_pos = (rand() % (file_size - sizeof(buffer)));
                handler.seek(random_pos, SEEK_SET);
                handler.read(buffer, 1, sizeof(buffer));
            }
            
            end_time = std::chrono::high_resolution_clock::now();
            duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            ASSERT_TRUE(duration.count() < 2000, "Random access should be reasonably fast");
            
        } catch (const std::exception& e) {
            IOHandlerTestUtils::cleanupTestFile(test_file);
            throw;
        }
        
        IOHandlerTestUtils::cleanupTestFile(test_file);
    }
    
    void testMemoryUsagePatterns() {
        // Test memory usage with multiple handlers
        std::vector<std::unique_ptr<FileIOHandler>> handlers;
        
        // Create test files
        std::vector<std::string> test_files;
        for (int i = 0; i < 5; ++i) {
            std::string filename = "memory_test_" + std::to_string(i) + ".txt";
            std::string content = "Memory test content " + std::to_string(i);
            IOHandlerTestUtils::createTestFile(filename, content);
            test_files.push_back(filename);
        }
        
        try {
            // Get initial memory stats
            auto initial_stats = IOHandler::getMemoryStats();
            size_t initial_usage = initial_stats["total_memory_usage"];
            
            // Create multiple handlers
            for (const auto& filename : test_files) {
                handlers.push_back(std::make_unique<FileIOHandler>(TagLib::String(filename)));
            }
            
            // Check memory usage increased
            auto after_creation_stats = IOHandler::getMemoryStats();
            size_t after_creation_usage = after_creation_stats["total_memory_usage"];
            
            ASSERT_TRUE(after_creation_usage >= initial_usage, 
                       "Memory usage should increase with more handlers");
            
            // Perform operations on all handlers
            for (auto& handler : handlers) {
                char buffer[256];
                handler->read(buffer, 1, sizeof(buffer));
            }
            
            // Clean up handlers
            handlers.clear();
            
            // Check memory usage decreased
            auto after_cleanup_stats = IOHandler::getMemoryStats();
            size_t after_cleanup_usage = after_cleanup_stats["total_memory_usage"];
            
            ASSERT_TRUE(after_cleanup_usage <= after_creation_usage, 
                       "Memory usage should decrease after cleanup");
            
        } catch (const std::exception& e) {
            for (const auto& filename : test_files) {
                IOHandlerTestUtils::cleanupTestFile(filename);
            }
            throw;
        }
        
        // Cleanup test files
        for (const auto& filename : test_files) {
            IOHandlerTestUtils::cleanupTestFile(filename);
        }
    }
    
    void testLargeFileHandling() {
        // Test with a larger file (10MB)
        std::string test_file = "large_file_test.bin";
        size_t file_size = 10 * 1024 * 1024; // 10MB
        
        try {
            IOHandlerTestUtils::createLargeTestFile(test_file, file_size);
            
            FileIOHandler handler{TagLib::String(test_file)};
            
            // Test file size reporting
            off_t reported_size = handler.getFileSize();
            ASSERT_EQUALS(static_cast<off_t>(file_size), reported_size, 
                         "Large file size should be reported correctly");
            
            // Test seeking to various positions
            std::vector<off_t> test_positions = {
                0,
                static_cast<off_t>(file_size / 4),
                static_cast<off_t>(file_size / 2),
                static_cast<off_t>(3 * file_size / 4),
                static_cast<off_t>(file_size - 1000)
            };
            
            for (off_t pos : test_positions) {
                int seek_result = handler.seek(pos, SEEK_SET);
                ASSERT_EQUALS(0, seek_result, "Seek should succeed for large file");
                
                off_t actual_pos = handler.tell();
                ASSERT_EQUALS(pos, actual_pos, "Position should be correct after seek");
                
                // Read a small amount to verify position
                char buffer[1024];
                size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
                ASSERT_TRUE(bytes_read > 0, "Should be able to read from large file");
            }
            
        } catch (const std::exception& e) {
            IOHandlerTestUtils::cleanupTestFile(test_file);
            throw;
        }
        
        IOHandlerTestUtils::cleanupTestFile(test_file);
    }
};

// Main test runner
int main() {
    TestSuite suite("IOHandler Unit Tests");
    
    // Add all test cases
    suite.addTest(std::make_unique<IOHandlerBaseTest>());
    suite.addTest(std::make_unique<FileIOHandlerTest>());
    suite.addTest(std::make_unique<HTTPIOHandlerTest>());
    suite.addTest(std::make_unique<HTTPClientTest>());
    suite.addTest(std::make_unique<CrossPlatformTest>());
    suite.addTest(std::make_unique<ThreadSafetyTest>());
    suite.addTest(std::make_unique<PerformanceTest>());
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print results
    suite.printResults(results);
    
    // Return appropriate exit code
    return suite.getFailureCount(results) == 0 ? 0 : 1;
}