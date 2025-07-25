/*
 * test_iohandler_simple.cpp - Simple IOHandler tests
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include <fstream>

using namespace TestFramework;

// Simple test utilities
class SimpleTestUtils {
public:
    static void createTestFile(const std::string& filename, const std::string& content) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            throw TestSetupFailure("Failed to create test file: " + filename);
        }
        file.write(content.c_str(), content.length());
        file.close();
    }
    
    static void cleanupTestFile(const std::string& filename) {
        std::remove(filename.c_str());
    }
};

// Simple FileIOHandler test
class SimpleFileIOTest : public TestCase {
public:
    SimpleFileIOTest() : TestCase("Simple FileIOHandler Test") {}
    
protected:
    void setUp() override {
        test_file = "simple_test.txt";
        test_content = "Hello, World!";
        SimpleTestUtils::createTestFile(test_file, test_content);
    }
    
    void tearDown() override {
        SimpleTestUtils::cleanupTestFile(test_file);
    }
    
    void runTest() override {
        FileIOHandler handler{TagLib::String(test_file)};
        
        // Test basic read
        char buffer[256];
        size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
        ASSERT_EQUALS(test_content.length(), bytes_read, "Should read correct number of bytes");
        
        std::string read_content(buffer, bytes_read);
        ASSERT_EQUALS(test_content, read_content, "Content should match");
        
        // Test file size
        off_t file_size = handler.getFileSize();
        ASSERT_EQUALS(static_cast<off_t>(test_content.length()), file_size, "File size should be correct");
        
        // Test EOF
        ASSERT_TRUE(handler.eof(), "Should be at EOF after reading entire file");
        
        // Test close
        int close_result = handler.close();
        ASSERT_EQUALS(0, close_result, "Close should succeed");
    }
    
private:
    std::string test_file;
    std::string test_content;
};

// Simple memory stats test
class SimpleMemoryTest : public TestCase {
public:
    SimpleMemoryTest() : TestCase("Simple Memory Test") {}
    
protected:
    void runTest() override {
        // Test memory stats are available
        auto stats = IOHandler::getMemoryStats();
        ASSERT_TRUE(stats.find("total_memory_usage") != stats.end(), 
                   "Memory stats should include total_memory_usage");
        
        // Test stats have reasonable values
        size_t total_usage = stats["total_memory_usage"];
        ASSERT_TRUE(total_usage >= 0, "Total memory usage should be non-negative");
    }
};

// Main test runner
int main() {
    TestSuite suite("Simple IOHandler Tests");
    
    // Add simple test cases
    suite.addTest(std::make_unique<SimpleFileIOTest>());
    suite.addTest(std::make_unique<SimpleMemoryTest>());
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print results
    suite.printResults(results);
    
    // Return appropriate exit code
    return suite.getFailureCount(results) == 0 ? 0 : 1;
}