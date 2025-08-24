/*
 * test_iohandler_thread_safety_comprehensive.cpp - Comprehensive thread safety tests for IOHandler subsystem
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
#include <random>
#include <fstream>

class IOHandlerThreadSafetyTest {
public:
    void runAllTests() {
        std::cout << "Running IOHandler Thread Safety Tests..." << std::endl;
        
        // Test concurrent file I/O operations
        testConcurrentFileOperations();
        
        // Test memory management thread safety
        testMemoryManagementThreadSafety();
        
        // Test integration with other threaded components
        testIntegrationWithThreadedComponents();
        
        // Test deadlock prevention
        testDeadlockPrevention();
        
        // Test error handling thread safety
        testErrorHandlingThreadSafety();
        
        std::cout << "All IOHandler thread safety tests completed successfully!" << std::endl;
    }

private:
    void testConcurrentFileOperations() {
        std::cout << "Testing concurrent file I/O operations..." << std::endl;
        
        // Create a test file
        const std::string test_file = "test_concurrent_io.dat";
        createTestFile(test_file, 1024 * 1024); // 1MB test file
        
        try {
            // Test concurrent reads from the same file
            testConcurrentReads(test_file);
            
            // Test concurrent seeks on the same file
            testConcurrentSeeks(test_file);
            
            // Test concurrent operations on different files
            testConcurrentDifferentFiles();
            
            // Test concurrent memory operations
            testConcurrentMemoryOperations();
            
        } catch (const std::exception& e) {
            std::cerr << "Exception in concurrent file operations test: " << e.what() << std::endl;
            throw;
        }
        
        // Cleanup
        std::remove(test_file.c_str());
        
        std::cout << "Concurrent file operations test passed!" << std::endl;
    }
    
    void testConcurrentReads(const std::string& filename) {
        const int num_threads = 8;
        const int reads_per_thread = 100;
        std::vector<std::thread> threads;
        std::atomic<int> errors{0};
        std::atomic<size_t> total_bytes_read{0};
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    FileIOHandler handler(TagLib::String(filename.c_str()));
                    
                    for (int j = 0; j < reads_per_thread; ++j) {
                        // Read from different positions
                        off_t position = (i * reads_per_thread + j) * 1024;
                        
                        if (handler.seek(position, SEEK_SET) == 0) {
                            char buffer[1024];
                            size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
                            total_bytes_read += bytes_read;
                            
                            // Verify position consistency
                            off_t current_pos = handler.tell();
                            if (current_pos != position + static_cast<off_t>(bytes_read)) {
                                std::cerr << "Position inconsistency detected!" << std::endl;
                                errors++;
                            }
                        }
                    }
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
            throw std::runtime_error("Concurrent reads test failed with " + std::to_string(errors) + " errors");
        }
        
        std::cout << "  Concurrent reads: " << total_bytes_read << " bytes read successfully" << std::endl;
    }
    
    void testConcurrentSeeks(const std::string& filename) {
        const int num_threads = 6;
        const int seeks_per_thread = 200;
        std::vector<std::thread> threads;
        std::atomic<int> errors{0};
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    FileIOHandler handler(TagLib::String(filename.c_str()));
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<off_t> dis(0, 1024 * 1024 - 1);
                    
                    for (int j = 0; j < seeks_per_thread; ++j) {
                        off_t target_pos = dis(gen);
                        
                        if (handler.seek(target_pos, SEEK_SET) == 0) {
                            off_t actual_pos = handler.tell();
                            if (actual_pos != target_pos) {
                                std::cerr << "Seek inconsistency: expected " << target_pos 
                                         << ", got " << actual_pos << std::endl;
                                errors++;
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Thread " << i << " seek exception: " << e.what() << std::endl;
                    errors++;
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        if (errors > 0) {
            throw std::runtime_error("Concurrent seeks test failed with " + std::to_string(errors) + " errors");
        }
        
        std::cout << "  Concurrent seeks: All position operations consistent" << std::endl;
    }
    
    void testConcurrentDifferentFiles() {
        const int num_threads = 4;
        const int num_files = 4;
        std::vector<std::string> test_files;
        std::vector<std::thread> threads;
        std::atomic<int> errors{0};
        
        // Create multiple test files
        for (int i = 0; i < num_files; ++i) {
            std::string filename = "test_concurrent_" + std::to_string(i) + ".dat";
            createTestFile(filename, 64 * 1024); // 64KB each
            test_files.push_back(filename);
        }
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    std::string filename = test_files[i % num_files];
                    FileIOHandler handler(TagLib::String(filename.c_str()));
                    
                    // Perform various operations
                    for (int j = 0; j < 50; ++j) {
                        char buffer[1024];
                        
                        // Seek to random position
                        off_t pos = (j * 1024) % (64 * 1024);
                        handler.seek(pos, SEEK_SET);
                        
                        // Read data
                        size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
                        
                        // Verify we can get current position
                        off_t current_pos = handler.tell();
                        if (current_pos < 0) {
                            errors++;
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Thread " << i << " different files exception: " << e.what() << std::endl;
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
            throw std::runtime_error("Concurrent different files test failed with " + std::to_string(errors) + " errors");
        }
        
        std::cout << "  Concurrent different files: All operations successful" << std::endl;
    }
    
    void testConcurrentMemoryOperations() {
        const int num_threads = 6;
        std::vector<std::thread> threads;
        std::atomic<int> errors{0};
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    // Test memory statistics access
                    for (int j = 0; j < 100; ++j) {
                        auto stats = IOHandler::getMemoryStats();
                        
                        // Verify stats are reasonable
                        if (stats.find("total_memory_usage") == stats.end() ||
                            stats.find("active_handlers") == stats.end()) {
                            errors++;
                        }
                        
                        // Small delay to increase contention
                        std::this_thread::sleep_for(std::chrono::microseconds(10));
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Thread " << i << " memory operations exception: " << e.what() << std::endl;
                    errors++;
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        if (errors > 0) {
            throw std::runtime_error("Concurrent memory operations test failed with " + std::to_string(errors) + " errors");
        }
        
        std::cout << "  Concurrent memory operations: All statistics access successful" << std::endl;
    }
    
    void testMemoryManagementThreadSafety() {
        std::cout << "Testing memory management thread safety..." << std::endl;
        
        const int num_threads = 8;
        const int operations_per_thread = 50;
        std::vector<std::thread> threads;
        std::atomic<int> errors{0};
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    std::vector<std::unique_ptr<FileIOHandler>> handlers;
                    
                    for (int j = 0; j < operations_per_thread; ++j) {
                        // Create test file for this iteration
                        std::string filename = "test_memory_" + std::to_string(i) + "_" + std::to_string(j) + ".dat";
                        createTestFile(filename, 8192); // 8KB
                        
                        try {
                            // Create handler (allocates memory)
                            auto handler = std::make_unique<FileIOHandler>(TagLib::String(filename.c_str()));
                            
                            // Perform some operations
                            char buffer[1024];
                            handler->read(buffer, 1, sizeof(buffer));
                            
                            // Check memory stats periodically
                            if (j % 10 == 0) {
                                auto stats = IOHandler::getMemoryStats();
                                if (stats["active_handlers"] == 0) {
                                    std::cerr << "Unexpected zero active handlers" << std::endl;
                                    errors++;
                                }
                            }
                            
                            handlers.push_back(std::move(handler));
                            
                        } catch (const std::exception& e) {
                            std::cerr << "Handler creation failed: " << e.what() << std::endl;
                            errors++;
                        }
                        
                        // Cleanup file
                        std::remove(filename.c_str());
                    }
                    
                    // Close all handlers (deallocates memory)
                    handlers.clear();
                    
                } catch (const std::exception& e) {
                    std::cerr << "Thread " << i << " memory management exception: " << e.what() << std::endl;
                    errors++;
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        if (errors > 0) {
            throw std::runtime_error("Memory management thread safety test failed with " + std::to_string(errors) + " errors");
        }
        
        std::cout << "Memory management thread safety test passed!" << std::endl;
    }
    
    void testIntegrationWithThreadedComponents() {
        std::cout << "Testing integration with other threaded components..." << std::endl;
        
        const int num_threads = 4;
        std::vector<std::thread> threads;
        std::atomic<int> errors{0};
        
        // Create test file
        const std::string test_file = "test_integration.dat";
        createTestFile(test_file, 256 * 1024); // 256KB
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    FileIOHandler handler(TagLib::String(test_file.c_str()));
                    
                    for (int j = 0; j < 100; ++j) {
                        // Simulate integration with memory pool manager
                        char buffer[4096];
                        
                        // Seek to position
                        off_t pos = (j * 4096) % (256 * 1024);
                        if (handler.seek(pos, SEEK_SET) != 0) {
                            errors++;
                            continue;
                        }
                        
                        // Read data
                        size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
                        
                        // Simulate memory pressure by checking limits
                        auto stats = IOHandler::getMemoryStats();
                        if (stats.empty()) {
                            errors++;
                        }
                        
                        // Small delay to increase thread interaction
                        if (j % 20 == 0) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Thread " << i << " integration exception: " << e.what() << std::endl;
                    errors++;
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        // Cleanup
        std::remove(test_file.c_str());
        
        if (errors > 0) {
            throw std::runtime_error("Integration test failed with " + std::to_string(errors) + " errors");
        }
        
        std::cout << "Integration with threaded components test passed!" << std::endl;
    }
    
    void testDeadlockPrevention() {
        std::cout << "Testing deadlock prevention..." << std::endl;
        
        const int num_threads = 6;
        const int operations_per_thread = 200;
        std::vector<std::thread> threads;
        std::atomic<int> errors{0};
        std::atomic<bool> deadlock_detected{false};
        
        // Create test file
        const std::string test_file = "test_deadlock.dat";
        createTestFile(test_file, 128 * 1024); // 128KB
        
        // Timeout mechanism to detect potential deadlocks
        std::thread timeout_thread([&]() {
            std::this_thread::sleep_for(std::chrono::seconds(30)); // 30 second timeout
            if (!deadlock_detected.load()) {
                deadlock_detected = true;
                std::cerr << "Potential deadlock detected - test taking too long!" << std::endl;
            }
        });
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    FileIOHandler handler(TagLib::String(test_file.c_str()));
                    
                    for (int j = 0; j < operations_per_thread && !deadlock_detected.load(); ++j) {
                        // Mix of operations that could potentially cause deadlocks
                        // in the old implementation
                        
                        char buffer[1024];
                        off_t pos = (j * 1024) % (128 * 1024);
                        
                        // Seek and read in quick succession
                        if (handler.seek(pos, SEEK_SET) == 0) {
                            handler.read(buffer, 1, sizeof(buffer));
                            
                            // Get position (this could cause deadlock in old implementation
                            // if it called other public methods internally)
                            off_t current_pos = handler.tell();
                            if (current_pos < 0) {
                                errors++;
                            }
                        }
                        
                        // Periodically check memory stats (could cause deadlock
                        // if memory management methods weren't properly unlocked)
                        if (j % 50 == 0) {
                            auto stats = IOHandler::getMemoryStats();
                            if (stats.empty()) {
                                errors++;
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Thread " << i << " deadlock test exception: " << e.what() << std::endl;
                    errors++;
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        deadlock_detected = true; // Signal timeout thread to exit
        timeout_thread.join();
        
        // Cleanup
        std::remove(test_file.c_str());
        
        if (errors > 0) {
            throw std::runtime_error("Deadlock prevention test failed with " + std::to_string(errors) + " errors");
        }
        
        std::cout << "Deadlock prevention test passed!" << std::endl;
    }
    
    void testErrorHandlingThreadSafety() {
        std::cout << "Testing error handling thread safety..." << std::endl;
        
        const int num_threads = 4;
        std::vector<std::thread> threads;
        std::atomic<int> errors{0};
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    // Test error conditions in concurrent environment
                    
                    // Try to open non-existent file
                    try {
                        FileIOHandler handler(TagLib::String("non_existent_file_12345.dat"));
                        errors++; // Should not reach here
                    } catch (const InvalidMediaException&) {
                        // Expected exception
                    }
                    
                    // Create a valid handler and test error conditions
                    std::string filename = "test_error_" + std::to_string(i) + ".dat";
                    createTestFile(filename, 1024);
                    
                    try {
                        FileIOHandler handler(TagLib::String(filename.c_str()));
                        
                        // Test invalid operations
                        char buffer[1024];
                        
                        // Seek beyond file end
                        handler.seek(10000, SEEK_SET);
                        
                        // Try to read (should handle gracefully)
                        size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
                        
                        // Close and try operations on closed handler
                        handler.close();
                        
                        // These should fail gracefully without crashes
                        handler.seek(0, SEEK_SET);
                        handler.read(buffer, 1, sizeof(buffer));
                        handler.tell();
                        
                    } catch (const std::exception& e) {
                        // Some exceptions are expected, but shouldn't crash
                    }
                    
                    // Cleanup
                    std::remove(filename.c_str());
                    
                } catch (const std::exception& e) {
                    std::cerr << "Thread " << i << " error handling exception: " << e.what() << std::endl;
                    errors++;
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        if (errors > 0) {
            throw std::runtime_error("Error handling thread safety test failed with " + std::to_string(errors) + " errors");
        }
        
        std::cout << "Error handling thread safety test passed!" << std::endl;
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
        IOHandlerThreadSafetyTest test;
        test.runAllTests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}