/*
 * test_iohandler_memory_deadlock_prevention.cpp - Test memory management deadlock prevention
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <fstream>

class IOHandlerMemoryDeadlockTest {
public:
    void runAllTests() {
        std::cout << "Running IOHandler Memory Deadlock Prevention Tests..." << std::endl;
        
        // Test that memory management doesn't cause deadlocks during I/O
        testMemoryManagementDuringIO();
        
        // Test concurrent memory optimization
        testConcurrentMemoryOptimization();
        
        // Test memory limit checking during concurrent operations
        testMemoryLimitCheckingConcurrency();
        
        std::cout << "All IOHandler memory deadlock prevention tests completed successfully!" << std::endl;
    }

private:
    void testMemoryManagementDuringIO() {
        std::cout << "Testing memory management during I/O operations..." << std::endl;
        
        const int num_threads = 6;
        const int operations_per_thread = 100;
        std::vector<std::thread> threads;
        std::atomic<int> errors{0};
        std::atomic<bool> test_running{true};
        
        // Create test file
        const std::string test_file = "test_memory_io.dat";
        createTestFile(test_file, 512 * 1024); // 512KB
        
        // Thread that continuously performs I/O operations
        for (int i = 0; i < num_threads - 2; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    FileIOHandler handler(TagLib::String(test_file.c_str()));
                    
                    int operation_count = 0;
                    while (test_running.load() && operation_count < operations_per_thread) {
                        char buffer[4096];
                        
                        // Seek to random position
                        off_t pos = (operation_count * 4096) % (512 * 1024);
                        
                        // These operations should not deadlock with memory management
                        int seek_result = handler.seek(pos, SEEK_SET);
                        if (seek_result == 0) {
                            // Read data
                            size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
                            
                            // Verify position (this should not deadlock)
                            off_t current_pos = handler.tell();
                            if (current_pos >= 0 && current_pos != pos + static_cast<off_t>(bytes_read)) {
                                // Only count as error if tell() succeeded
                                errors++;
                            }
                        }
                        
                        operation_count++;
                        
                        // Small delay to allow memory operations to interleave
                        if (operation_count % 20 == 0) {
                            std::this_thread::sleep_for(std::chrono::microseconds(100));
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "I/O thread " << i << " exception: " << e.what() << std::endl;
                    errors++;
                }
            });
        }
        
        // Thread that continuously accesses memory statistics
        threads.emplace_back([&]() {
            try {
                int stats_count = 0;
                while (test_running.load() && stats_count < operations_per_thread) {
                    // This should not deadlock with I/O operations
                    auto stats = IOHandler::getMemoryStats();
                    
                    // Verify we got reasonable stats
                    if (stats.find("total_memory_usage") == stats.end()) {
                        errors++;
                    }
                    
                    stats_count++;
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
                }
            } catch (const std::exception& e) {
                std::cerr << "Memory stats thread exception: " << e.what() << std::endl;
                errors++;
            }
        });
        
        // Thread that triggers memory optimization
        threads.emplace_back([&]() {
            try {
                int optimization_count = 0;
                while (test_running.load() && optimization_count < 10) {
                    // This should not deadlock with I/O operations
                    IOHandler::performMemoryOptimization();
                    
                    optimization_count++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            } catch (const std::exception& e) {
                std::cerr << "Memory optimization thread exception: " << e.what() << std::endl;
                errors++;
            }
        });
        
        // Let threads run for a while
        std::this_thread::sleep_for(std::chrono::seconds(5));
        test_running = false;
        
        for (auto& t : threads) {
            t.join();
        }
        
        // Cleanup
        std::remove(test_file.c_str());
        
        if (errors > 0) {
            throw std::runtime_error("Memory management during I/O test failed with " + std::to_string(errors) + " errors");
        }
        
        std::cout << "Memory management during I/O test passed!" << std::endl;
    }
    
    void testConcurrentMemoryOptimization() {
        std::cout << "Testing concurrent memory optimization..." << std::endl;
        
        const int num_threads = 4;
        std::vector<std::thread> threads;
        std::atomic<int> errors{0};
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    for (int j = 0; j < 20; ++j) {
                        // Multiple threads calling memory optimization simultaneously
                        // This tests that the unlocked version is properly used internally
                        IOHandler::performMemoryOptimization();
                        
                        // Also test memory stats access
                        auto stats = IOHandler::getMemoryStats();
                        if (stats.empty()) {
                            errors++;
                        }
                        
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Thread " << i << " memory optimization exception: " << e.what() << std::endl;
                    errors++;
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        if (errors > 0) {
            throw std::runtime_error("Concurrent memory optimization test failed with " + std::to_string(errors) + " errors");
        }
        
        std::cout << "Concurrent memory optimization test passed!" << std::endl;
    }
    
    void testMemoryLimitCheckingConcurrency() {
        std::cout << "Testing memory limit checking concurrency..." << std::endl;
        
        const int num_threads = 8;
        std::vector<std::thread> threads;
        std::atomic<int> errors{0};
        
        // Create multiple test files to trigger memory allocations
        std::vector<std::string> test_files;
        for (int i = 0; i < num_threads; ++i) {
            std::string filename = "test_memory_limit_" + std::to_string(i) + ".dat";
            createTestFile(filename, 64 * 1024); // 64KB each
            test_files.push_back(filename);
        }
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    std::string filename = test_files[i];
                    FileIOHandler handler(TagLib::String(filename.c_str()));
                    
                    for (int j = 0; j < 50; ++j) {
                        char buffer[8192];
                        
                        // Perform I/O that may trigger memory limit checks
                        off_t pos = (j * 8192) % (64 * 1024);
                        if (handler.seek(pos, SEEK_SET) == 0) {
                            size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
                            
                            // This internally may call checkMemoryLimits
                            // which should not deadlock with other operations
                            if (bytes_read == 0 && !handler.eof()) {
                                // Only count as error if we're not at EOF
                                off_t file_size = handler.getFileSize();
                                if (pos < file_size) {
                                    errors++;
                                }
                            }
                        }
                        
                        // Periodically check memory stats
                        if (j % 10 == 0) {
                            auto stats = IOHandler::getMemoryStats();
                            if (stats.find("active_handlers") == stats.end()) {
                                errors++;
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Thread " << i << " memory limit checking exception: " << e.what() << std::endl;
                    errors++;
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        // Cleanup
        for (const auto& filename : test_files) {
            std::remove(filename.c_str());
        }
        
        if (errors > 0) {
            throw std::runtime_error("Memory limit checking concurrency test failed with " + std::to_string(errors) + " errors");
        }
        
        std::cout << "Memory limit checking concurrency test passed!" << std::endl;
    }
    
    void createTestFile(const std::string& filename, size_t size) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Failed to create test file: " + filename);
        }
        
        // Write test pattern
        for (size_t i = 0; i < size; ++i) {
            file.put(static_cast<char>(i % 256));
        }
        
        file.close();
        if (!file) {
            throw std::runtime_error("Failed to write test file: " + filename);
        }
    }
};

int main() {
    try {
        IOHandlerMemoryDeadlockTest test;
        test.runAllTests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}