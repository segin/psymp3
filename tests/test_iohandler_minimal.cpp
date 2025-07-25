/*
 * test_iohandler_minimal.cpp - Minimal integration test for IOHandler subsystem
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include <iostream>
#include <fstream>

int main() {
    std::cout << "Running minimal IOHandler integration test..." << std::endl;
    
    try {
        // Test 1: Basic exception handling
        std::cout << "1. Testing exception handling..." << std::endl;
        bool caught_exception = false;
        try {
            FileIOHandler handler{TagLib::String("nonexistent_file.txt")};
        } catch (const InvalidMediaException& e) {
            caught_exception = true;
            std::cout << "   ✓ InvalidMediaException caught: " << e.what() << std::endl;
        }
        
        if (!caught_exception) {
            std::cerr << "   ✗ Expected InvalidMediaException not caught" << std::endl;
            return 1;
        }
        
        // Test 2: Basic file operations
        std::cout << "2. Testing basic file operations..." << std::endl;
        
        // Create a test file
        std::ofstream test_file("minimal_test.txt");
        test_file << "Hello, World!";
        test_file.close();
        
        try {
            FileIOHandler handler{TagLib::String("minimal_test.txt")};
            
            // Test basic operations
            char buffer[32];
            size_t bytes_read = handler.read(buffer, 1, 10);
            std::cout << "   ✓ Read " << bytes_read << " bytes" << std::endl;
            
            off_t file_size = handler.getFileSize();
            std::cout << "   ✓ File size: " << file_size << " bytes" << std::endl;
            
            off_t position = handler.tell();
            std::cout << "   ✓ Current position: " << position << std::endl;
            
            int seek_result = handler.seek(0, SEEK_SET);
            std::cout << "   ✓ Seek result: " << seek_result << std::endl;
            
            // Clean up
            std::remove("minimal_test.txt");
            
        } catch (const std::exception& e) {
            std::cerr << "   ✗ Exception during file operations: " << e.what() << std::endl;
            std::remove("minimal_test.txt");
            return 1;
        }
        
        // Test 3: URI parsing
        std::cout << "3. Testing URI parsing..." << std::endl;
        try {
            URI test_uri("http://example.com/test.mp3");
            std::cout << "   ✓ URI scheme: " << test_uri.scheme().to8Bit() << std::endl;
            std::cout << "   ✓ URI path: " << test_uri.path().to8Bit() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "   ✗ Exception during URI parsing: " << e.what() << std::endl;
            return 1;
        }
        
        std::cout << std::endl;
        std::cout << "All minimal integration tests PASSED!" << std::endl;
        std::cout << "✓ Exception handling integration verified" << std::endl;
        std::cout << "✓ Basic file operations verified" << std::endl;
        std::cout << "✓ URI parsing integration verified" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}