/*
 * test_iohandler_basic.cpp - Basic test without debug system
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

int main() {
    std::cout << "Running basic IOHandler test..." << std::endl;
    
    try {
        // Test 1: Exception handling
        std::cout << "1. Testing exception handling..." << std::endl;
        bool caught_exception = false;
        try {
            FileIOHandler handler{TagLib::String("nonexistent_file.txt")};
        } catch (const InvalidMediaException& e) {
            caught_exception = true;
            std::cout << "   ✓ InvalidMediaException caught" << std::endl;
        }
        
        if (!caught_exception) {
            std::cerr << "   ✗ Expected InvalidMediaException not caught" << std::endl;
            return 1;
        }
        
        // Test 2: URI parsing
        std::cout << "2. Testing URI parsing..." << std::endl;
        try {
            URI test_uri("http://example.com/test.mp3");
            std::cout << "   ✓ URI scheme: " << test_uri.scheme().to8Bit() << std::endl;
            std::cout << "   ✓ URI path: " << test_uri.path().to8Bit() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "   ✗ Exception during URI parsing: " << e.what() << std::endl;
            return 1;
        }
        
        std::cout << std::endl;
        std::cout << "Basic integration tests PASSED!" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}