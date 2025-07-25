/*
 * test_iohandler_no_memory.cpp - Test IOHandler without memory management
 */

#include <iostream>
#include <fstream>

// Include only the basic headers we need
#include "../include/IOHandler.h"
#include "../include/FileIOHandler.h"
#include "../include/exceptions.h"

int main() {
    std::cout << "Testing IOHandler without memory management..." << std::endl;
    
    try {
        // Create a simple test file
        std::string test_file = "no_memory_test.txt";
        std::string test_content = "Test content";
        
        std::ofstream file(test_file);
        if (!file) {
            std::cout << "Failed to create test file" << std::endl;
            return 1;
        }
        file << test_content;
        file.close();
        
        std::cout << "Test file created" << std::endl;
        
        // Try to create FileIOHandler
        std::cout << "Creating FileIOHandler..." << std::endl;
        
        // This is where it might hang due to memory management initialization
        FileIOHandler handler{TagLib::String(test_file)};
        
        std::cout << "FileIOHandler created successfully" << std::endl;
        
        // Clean up
        std::remove(test_file.c_str());
        
        std::cout << "Test completed successfully" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return 1;
    }
}