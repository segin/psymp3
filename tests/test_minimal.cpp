/*
 * test_minimal.cpp - Minimal test to check basic functionality
 */

#include <iostream>
#include <fstream>

int main() {
    std::cout << "Starting minimal test..." << std::endl;
    
    // Create a simple test file
    std::string test_file = "minimal_test.txt";
    std::string test_content = "Hello";
    
    std::ofstream file(test_file);
    if (!file) {
        std::cout << "Failed to create test file" << std::endl;
        return 1;
    }
    file << test_content;
    file.close();
    
    std::cout << "Test file created successfully" << std::endl;
    
    // Clean up
    std::remove(test_file.c_str());
    
    std::cout << "Minimal test completed successfully" << std::endl;
    return 0;
}