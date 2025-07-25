/*
 * test_iohandler_integration_simple.cpp - Simple integration tests for IOHandler subsystem with PsyMP3
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include <fstream>
#include <iostream>
#include <cassert>

// Simple test framework functions
void assert_true(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "ASSERTION FAILED: " << message << std::endl;
        exit(1);
    }
}

void assert_false(bool condition, const std::string& message) {
    if (condition) {
        std::cerr << "ASSERTION FAILED: " << message << std::endl;
        exit(1);
    }
}

void assert_equals(long expected, long actual, const std::string& message) {
    if (expected != actual) {
        std::cerr << "ASSERTION FAILED: " << message << " (expected: " << expected << ", actual: " << actual << ")" << std::endl;
        exit(1);
    }
}

void createTestFile(const std::string& filename, const std::string& content) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to create test file: " + filename);
    }
    file.write(content.c_str(), content.length());
    file.close();
}

void cleanup_test_file(const std::string& filename) {
    std::remove(filename.c_str());
}

// Test 1: Exception Integration - Verify InvalidMediaException is thrown for invalid files
void test_exception_integration() {
    std::cout << "Testing exception integration with PsyMP3's InvalidMediaException..." << std::endl;
    
    bool exception_caught = false;
    try {
        FileIOHandler handler{TagLib::String("nonexistent_file_12345.txt")};
    } catch (const InvalidMediaException& e) {
        exception_caught = true;
        std::cout << "  ✓ InvalidMediaException caught: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Wrong exception type caught: " << e.what() << std::endl;
        exit(1);
    }
    
    assert_true(exception_caught, "InvalidMediaException should be thrown for nonexistent file");
    std::cout << "  ✓ Exception integration test passed" << std::endl;
}

// Test 2: TagLib::String Integration - Verify FileIOHandler accepts TagLib::String parameters
void test_taglib_string_integration() {
    std::cout << "Testing TagLib::String integration..." << std::endl;
    
    std::string test_file = "test_taglib_string.txt";
    createTestFile(test_file, "TagLib String Test");
    
    try {
        // Test with TagLib::String constructor
        TagLib::String taglib_path(test_file);
        FileIOHandler handler{taglib_path};
        
        // Verify file operations work
        char buffer[32];
        size_t bytes_read = handler.read(buffer, 1, 10);
        assert_true(bytes_read > 0, "Should read data from file opened with TagLib::String");
        
        off_t file_size = handler.getFileSize();
        assert_true(file_size > 0, "Should get valid file size");
        
        cleanup_test_file(test_file);
        
    } catch (const std::exception& e) {
        cleanup_test_file(test_file);
        throw;
    }
    
    std::cout << "  ✓ TagLib::String integration test passed" << std::endl;
}

// Test 3: IOHandler Interface Compatibility - Verify interface works as expected by demuxers
void test_iohandler_interface_compatibility() {
    std::cout << "Testing IOHandler interface compatibility..." << std::endl;
    
    std::string test_file = "test_interface.txt";
    createTestFile(test_file, "Interface compatibility test data");
    
    try {
        // Create IOHandler and test interface methods
        FileIOHandler handler{TagLib::String(test_file)};
        
        // Verify IOHandler interface methods work as expected by demuxers
        assert_false(handler.eof(), "New handler should not be at EOF");
        
        char buffer[32];
        size_t bytes_read = handler.read(buffer, 1, 10);
        assert_true(bytes_read > 0, "Should read data");
        
        off_t pos = handler.tell();
        assert_equals(10, pos, "Position should be 10 after reading 10 bytes");
        
        int seek_result = handler.seek(0, SEEK_SET);
        assert_equals(0, seek_result, "Seek should succeed");
        
        pos = handler.tell();
        assert_equals(0, pos, "Position should be 0 after seeking to start");
        
        off_t file_size = handler.getFileSize();
        assert_true(file_size > 0, "Should get valid file size");
        
        // Test error handling
        assert_equals(0, handler.getLastError(), "Should have no error initially");
        
        cleanup_test_file(test_file);
        
    } catch (const std::exception& e) {
        cleanup_test_file(test_file);
        throw;
    }
    
    std::cout << "  ✓ IOHandler interface compatibility test passed" << std::endl;
}

// Test 4: URI Integration - Verify URI parsing works
void test_uri_integration() {
    std::cout << "Testing URI integration..." << std::endl;
    
    // Test URI parsing
    URI file_uri("file:///path/to/file.mp3");
    assert_true(file_uri.scheme() == "file", "Should parse file URI scheme");
    assert_true(file_uri.path() == "/path/to/file.mp3", "Should parse file URI path");
    
    URI http_uri("http://example.com/stream.mp3");
    assert_true(http_uri.scheme() == "http", "Should parse HTTP URI scheme");
    assert_true(http_uri.path() == "example.com/stream.mp3", "Should parse HTTP URI path");
    
    // Test plain path (should default to file scheme)
    URI plain_path("/local/file.mp3");
    assert_true(plain_path.scheme() == "file", "Should default to file scheme for plain paths");
    
    std::cout << "  ✓ URI integration test passed" << std::endl;
}

// Test 5: Error Handling Integration - Verify consistent error reporting
void test_error_handling_integration() {
    std::cout << "Testing error handling integration..." << std::endl;
    
    std::string test_file = "test_error_handling.txt";
    createTestFile(test_file, "Error handling test");
    
    try {
        FileIOHandler handler{TagLib::String(test_file)};
        
        // Test error state management
        assert_equals(0, handler.getLastError(), "New handler should have no error");
        
        // Test invalid operations
        int result = handler.seek(-1, SEEK_SET);
        assert_equals(-1, result, "Seek to negative position should fail");
        
        // Test that error is properly reported
        int error = handler.getLastError();
        assert_true(error != 0, "Should have error after invalid seek");
        
        // Test error recovery
        result = handler.seek(0, SEEK_SET);
        assert_equals(0, result, "Valid seek should succeed after error");
        
        cleanup_test_file(test_file);
        
    } catch (const std::exception& e) {
        cleanup_test_file(test_file);
        throw;
    }
    
    std::cout << "  ✓ Error handling integration test passed" << std::endl;
}

// Test 6: Basic File Operations - Verify basic file I/O works correctly
void test_basic_file_operations() {
    std::cout << "Testing basic file operations..." << std::endl;
    
    std::string test_file = "test_basic_ops.txt";
    std::string test_content = "Hello, World! This is a test file for basic operations.";
    createTestFile(test_file, test_content);
    
    try {
        FileIOHandler handler{TagLib::String(test_file)};
        
        // Test reading entire file
        char buffer[256];
        size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
        assert_true(bytes_read == test_content.length(), "Should read entire file content");
        
        // Verify content matches
        std::string read_content(buffer, bytes_read);
        assert_true(read_content == test_content, "Read content should match written content");
        
        // Test seeking and reading again
        handler.seek(0, SEEK_SET);
        bytes_read = handler.read(buffer, 1, 10);
        assert_equals(10, bytes_read, "Should read 10 bytes after seek");
        
        std::string partial_content(buffer, bytes_read);
        assert_true(partial_content == test_content.substr(0, 10), "Partial read should match");
        
        cleanup_test_file(test_file);
        
    } catch (const std::exception& e) {
        cleanup_test_file(test_file);
        throw;
    }
    
    std::cout << "  ✓ Basic file operations test passed" << std::endl;
}

// Test 7: Memory Management Integration - Basic RAII verification
void test_memory_management_integration() {
    std::cout << "Testing memory management integration..." << std::endl;
    
    std::string test_file = "test_memory_management.txt";
    createTestFile(test_file, "Memory management test data");
    
    try {
        // Test RAII and proper cleanup
        {
            FileIOHandler handler{TagLib::String(test_file)};
            
            // Perform some operations that might allocate memory
            char buffer[1024];
            handler.read(buffer, 1, sizeof(buffer));
            
        } // Handler should be destroyed here with proper cleanup
        
        // The fact that we don't crash indicates proper RAII
        cleanup_test_file(test_file);
        
    } catch (const std::exception& e) {
        cleanup_test_file(test_file);
        throw;
    }
    
    std::cout << "  ✓ Memory management integration test passed" << std::endl;
}

int main() {
    std::cout << "Running IOHandler Integration Tests (Simple)..." << std::endl;
    std::cout << "=============================================" << std::endl;
    
    try {
        test_exception_integration();
        test_taglib_string_integration();
        test_iohandler_interface_compatibility();
        test_uri_integration();
        test_error_handling_integration();
        test_basic_file_operations();
        test_memory_management_integration();
        
        std::cout << std::endl;
        std::cout << "=============================================" << std::endl;
        std::cout << "All IOHandler integration tests PASSED!" << std::endl;
        std::cout << "✓ Exception handling integration verified" << std::endl;
        std::cout << "✓ TagLib::String compatibility verified" << std::endl;
        std::cout << "✓ IOHandler interface compatibility verified" << std::endl;
        std::cout << "✓ URI parsing integration verified" << std::endl;
        std::cout << "✓ Error handling consistency verified" << std::endl;
        std::cout << "✓ Basic file operations verified" << std::endl;
        std::cout << "✓ Memory management integration verified" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Integration test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Integration test failed with unknown exception" << std::endl;
        return 1;
    }
}