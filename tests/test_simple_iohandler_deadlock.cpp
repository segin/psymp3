/*
 * test_simple_iohandler_deadlock.cpp - Simple test to isolate IOHandler deadlock issues
 */

#include "psymp3.h"
#include "test_framework.h"
#include <thread>
#include <vector>
#include <atomic>
#include <fstream>

void createTestFile(const std::string& filename, size_t size) {
    std::ofstream file(filename, std::ios::binary);
    std::vector<char> data(size, 'A');
    file.write(data.data(), data.size());
}

int main() {
    std::cout << "Running Simple IOHandler Deadlock Test..." << std::endl;
    
    // Create a test file
    const std::string test_file = "simple_test.dat";
    createTestFile(test_file, 1024);
    
    try {
        const int num_threads = 4;
        std::vector<std::thread> threads;
        std::atomic<int> errors{0};
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    FileIOHandler handler(TagLib::String(test_file.c_str()));
                    
                    // Simple read operation
                    char buffer[64];
                    size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
                    
                    std::cout << "Thread " << i << " read " << bytes_read << " bytes" << std::endl;
                    
                } catch (const std::exception& e) {
                    std::cerr << "Thread " << i << " exception: " << e.what() << std::endl;
                    errors++;
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        if (errors > 0) {
            std::cerr << "Test failed with " << errors << " errors" << std::endl;
            return 1;
        }
        
        std::cout << "Simple test passed!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Test exception: " << e.what() << std::endl;
        return 1;
    }
    
    // Cleanup
    std::remove(test_file.c_str());
    
    return 0;
}