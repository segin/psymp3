/*
 * test_debug.cpp - Test to isolate debug system issues
 */

#include <iostream>
#include <fstream>

// Minimal test without including psymp3.h
int main() {
    std::cout << "Testing basic file operations without IOHandler..." << std::endl;
    
    // Test basic file creation
    std::string test_file = "debug_test.txt";
    std::string test_content = "Debug test content";
    
    // Create file
    std::ofstream out_file(test_file);
    if (!out_file) {
        std::cout << "Failed to create file" << std::endl;
        return 1;
    }
    out_file << test_content;
    out_file.close();
    std::cout << "File created successfully" << std::endl;
    
    // Read file
    std::ifstream in_file(test_file);
    if (!in_file) {
        std::cout << "Failed to open file for reading" << std::endl;
        return 1;
    }
    
    std::string read_content;
    std::getline(in_file, read_content);
    in_file.close();
    
    if (read_content == test_content) {
        std::cout << "File read successfully: " << read_content << std::endl;
    } else {
        std::cout << "Content mismatch: expected '" << test_content << "', got '" << read_content << "'" << std::endl;
        return 1;
    }
    
    // Clean up
    std::remove(test_file.c_str());
    std::cout << "Test completed successfully" << std::endl;
    
    return 0;
}