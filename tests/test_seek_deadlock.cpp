/*
 * test_seek_deadlock.cpp - Test to isolate seek-related deadlock issues
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
    std::cout << "Running Seek Deadlock Test..." << std::endl;
    
    // Create a test file
    const std::string test_file = "seek_test.dat";
    createTestFile(test_file, 4096);
    
    try {
        const int num_threads = 4;
        std::vector<std::thread> threads;
        std::atomic<int> errors{0};
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    FileIOHandler handler(TagLib::String(test_file.c_str()));
                    
                    // Perform seek and read operations
                    for (int j = 0; j < 10; ++j) {
                        off_t position = (i * 10 + j) * 64;
                        
                        if (handler.seek(position, SEEK_SET) == 0) {
                            char buffer[64];
                            size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
                            
                            // Verify position
                            off_t current_pos = handler.tell();
                            off_t expected_pos = position + static_cast<off_t>(bytes_read);
                            
                            // Only check position if we're not at EOF
                            if (bytes_read > 0 && current_pos != expected_pos) {
                                std::cerr << "Thread " << i << " position mismatch! Expected: " << expected_pos 
                                         << ", Got: " << current_pos << ", Bytes read: " << bytes_read << std::endl;
                                errors++;
                            }
                        } else {
                            std::cerr << "Thread " << i << " seek failed!" << std::endl;
                            errors++;
                        }
                    }
                    
                    std::cout << "Thread " << i << " completed" << std::endl;
                    
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
        
        std::cout << "Seek test passed!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Test exception: " << e.what() << std::endl;
        return 1;
    }
    
    // Cleanup
    std::remove(test_file.c_str());
    
    return 0;
}