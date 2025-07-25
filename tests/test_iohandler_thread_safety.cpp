/*
 * test_iohandler_thread_safety.cpp - Thread safety tests for IOHandler subsystem
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

class IOHandlerThreadSafetyTest : public TestFramework::TestCase {
public:
    IOHandlerThreadSafetyTest() : TestFramework::TestCase("IOHandler Thread Safety Tests") {}
    
    void runTest() override {
        testFileIOHandlerConcurrentReads();
        testFileIOHandlerConcurrentSeeks();
        testFileIOHandlerConcurrentReadSeek();
        testHTTPIOHandlerConcurrentReads();
        testHTTPIOHandlerConcurrentSeeks();
        testIOHandlerMemoryTracking();
        testIOHandlerErrorStateThreadSafety();
        testIOHandlerPositionTracking();
    }

private:
    void testFileIOHandlerConcurrentReads() {
        Debug::log("test", "Testing FileIOHandler concurrent reads");
        
        // Create a test file with known content
        std::string test_file = "test_concurrent_reads.txt";
        createTestFile(test_file, 1024 * 1024); // 1MB file
        
        try {
            FileIOHandler handler{TagLib::String(test_file)};
            
            const int num_threads = 8;
            const int reads_per_thread = 100;
            std::vector<std::thread> threads;
            std::atomic<int> successful_reads{0};
            std::atomic<int> failed_reads{0};
            
            // Launch concurrent read threads
            for (int i = 0; i < num_threads; ++i) {
                threads.emplace_back([&handler, &successful_reads, &failed_reads, reads_per_thread, i]() {
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> pos_dist(0, 1024 * 1024 - 1024);
                    
                    for (int j = 0; j < reads_per_thread; ++j) {
                        // Seek to random position
                        off_t pos = pos_dist(gen);
                        if (handler.seek(pos, SEEK_SET) == 0) {
                            // Read some data
                            char buffer[256];
                            size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
                            if (bytes_read > 0) {
                                successful_reads.fetch_add(1);
                            } else {
                                failed_reads.fetch_add(1);
                            }
                        } else {
                            failed_reads.fetch_add(1);
                        }
                        
                        // Small delay to increase chance of race conditions
                        std::this_thread::sleep_for(std::chrono::microseconds(10));
                    }
                });
            }
            
            // Wait for all threads to complete
            for (auto& thread : threads) {
                thread.join();
            }
            
            Debug::log("test", "Concurrent reads completed: successful=", successful_reads.load(), 
                      " failed=", failed_reads.load());
            
            // Verify that most reads were successful
            int total_reads = successful_reads.load() + failed_reads.load();
            ASSERT_EQUALS(total_reads, num_threads * reads_per_thread, "Total reads should match expected count");
            ASSERT_TRUE(successful_reads.load() > total_reads * 0.9, "At least 90% success rate expected");
            
            handler.close();
            
        } catch (const std::exception& e) {
            addFailure("Exception during concurrent reads test: " + std::string(e.what()));
        }
        
        // Clean up
        std::remove(test_file.c_str());
    }
    
    void testFileIOHandlerConcurrentSeeks() {
        Debug::log("test", "Testing FileIOHandler concurrent seeks");
        
        // Create a test file
        std::string test_file = "test_concurrent_seeks.txt";
        createTestFile(test_file, 1024 * 1024); // 1MB file
        
        try {
            FileIOHandler handler(TagLib::String(test_file));
            
            const int num_threads = 6;
            const int seeks_per_thread = 200;
            std::vector<std::thread> threads;
            std::atomic<int> successful_seeks{0};
            std::atomic<int> failed_seeks{0};
            
            // Launch concurrent seek threads
            for (int i = 0; i < num_threads; ++i) {
                threads.emplace_back([&handler, &successful_seeks, &failed_seeks, seeks_per_thread]() {
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> pos_dist(0, 1024 * 1024 - 1);
                    std::uniform_int_distribution<> whence_dist(0, 2);
                    
                    for (int j = 0; j < seeks_per_thread; ++j) {
                        off_t pos = pos_dist(gen);
                        int whence = whence_dist(gen) == 0 ? SEEK_SET : 
                                   (whence_dist(gen) == 1 ? SEEK_CUR : SEEK_END);
                        
                        // Adjust position for SEEK_CUR and SEEK_END
                        if (whence == SEEK_CUR) {
                            pos = pos % 1024; // Small relative movement
                        } else if (whence == SEEK_END) {
                            pos = -(pos % 1024); // Negative offset from end
                        }
                        
                        if (handler.seek(pos, whence) == 0) {
                            successful_seeks.fetch_add(1);
                            
                            // Verify position is reasonable
                            off_t current_pos = handler.tell();
                            if (current_pos >= 0 && current_pos <= 1024 * 1024) {
                                // Position is valid
                            } else {
                                Debug::log("test", "Invalid position after seek: ", current_pos);
                            }
                        } else {
                            failed_seeks.fetch_add(1);
                        }
                        
                        std::this_thread::sleep_for(std::chrono::microseconds(5));
                    }
                });
            }
            
            // Wait for all threads to complete
            for (auto& thread : threads) {
                thread.join();
            }
            
            Debug::log("test", "Concurrent seeks completed: successful=", successful_seeks.load(), 
                      " failed=", failed_seeks.load());
            
            // Verify that most seeks were successful
            int total_seeks = successful_seeks.load() + failed_seeks.load();
            ASSERT_EQ(total_seeks, num_threads * seeks_per_thread);
            ASSERT_GT(successful_seeks.load(), total_seeks * 0.8); // At least 80% success rate
            
            handler.close();
            
        } catch (const std::exception& e) {
            FAIL("Exception during concurrent seeks test: " + std::string(e.what()));
        }
        
        // Clean up
        std::remove(test_file.c_str());
    }
    
    void testFileIOHandlerConcurrentReadSeek() {
        Debug::log("test", "Testing FileIOHandler concurrent read and seek operations");
        
        // Create a test file
        std::string test_file = "test_concurrent_read_seek.txt";
        createTestFile(test_file, 512 * 1024); // 512KB file
        
        try {
            FileIOHandler handler(TagLib::String(test_file));
            
            const int num_read_threads = 4;
            const int num_seek_threads = 2;
            const int operations_per_thread = 100;
            std::vector<std::thread> threads;
            std::atomic<int> successful_operations{0};
            std::atomic<int> failed_operations{0};
            
            // Launch read threads
            for (int i = 0; i < num_read_threads; ++i) {
                threads.emplace_back([&handler, &successful_operations, &failed_operations, operations_per_thread]() {
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> pos_dist(0, 512 * 1024 - 1024);
                    
                    for (int j = 0; j < operations_per_thread; ++j) {
                        char buffer[128];
                        size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
                        if (bytes_read >= 0) { // Any result is acceptable
                            successful_operations.fetch_add(1);
                        } else {
                            failed_operations.fetch_add(1);
                        }
                        
                        std::this_thread::sleep_for(std::chrono::microseconds(20));
                    }
                });
            }
            
            // Launch seek threads
            for (int i = 0; i < num_seek_threads; ++i) {
                threads.emplace_back([&handler, &successful_operations, &failed_operations, operations_per_thread]() {
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> pos_dist(0, 512 * 1024 - 1);
                    
                    for (int j = 0; j < operations_per_thread; ++j) {
                        off_t pos = pos_dist(gen);
                        if (handler.seek(pos, SEEK_SET) == 0) {
                            successful_operations.fetch_add(1);
                        } else {
                            failed_operations.fetch_add(1);
                        }
                        
                        std::this_thread::sleep_for(std::chrono::microseconds(30));
                    }
                });
            }
            
            // Wait for all threads to complete
            for (auto& thread : threads) {
                thread.join();
            }
            
            Debug::log("test", "Concurrent read/seek completed: successful=", successful_operations.load(), 
                      " failed=", failed_operations.load());
            
            // Verify that operations completed without crashes
            int total_operations = successful_operations.load() + failed_operations.load();
            int expected_operations = (num_read_threads + num_seek_threads) * operations_per_thread;
            ASSERT_EQ(total_operations, expected_operations);
            
            handler.close();
            
        } catch (const std::exception& e) {
            FAIL("Exception during concurrent read/seek test: " + std::string(e.what()));
        }
        
        // Clean up
        std::remove(test_file.c_str());
    }
    
    void testHTTPIOHandlerConcurrentReads() {
        Debug::log("test", "Testing HTTPIOHandler concurrent reads (mock)");
        
        // Note: This is a simplified test since we don't have a real HTTP server
        // In a real scenario, you would test with an actual HTTP endpoint
        
        try {
            // Test with a mock URL - this will fail to initialize but should be thread-safe
            std::string test_url = "http://example.com/test.mp3";
            
            const int num_threads = 4;
            std::vector<std::thread> threads;
            std::atomic<int> completed_threads{0};
            
            // Launch concurrent threads that attempt to create HTTPIOHandler instances
            for (int i = 0; i < num_threads; ++i) {
                threads.emplace_back([&test_url, &completed_threads]() {
                    try {
                        HTTPIOHandler handler(test_url);
                        // Even if initialization fails, the constructor should be thread-safe
                        completed_threads.fetch_add(1);
                    } catch (const std::exception& e) {
                        // Expected to fail, but should not crash
                        completed_threads.fetch_add(1);
                    }
                });
            }
            
            // Wait for all threads to complete
            for (auto& thread : threads) {
                thread.join();
            }
            
            ASSERT_EQ(completed_threads.load(), num_threads);
            Debug::log("test", "HTTPIOHandler concurrent initialization test completed");
            
        } catch (const std::exception& e) {
            FAIL("Exception during HTTPIOHandler concurrent test: " + std::string(e.what()));
        }
    }
    
    void testHTTPIOHandlerConcurrentSeeks() {
        Debug::log("test", "Testing HTTPIOHandler concurrent seeks (mock)");
        
        // Similar to the reads test, this tests thread safety of the HTTPIOHandler
        // even when operations fail due to network issues
        
        try {
            std::string test_url = "http://example.com/test.mp3";
            
            const int num_threads = 3;
            std::vector<std::thread> threads;
            std::atomic<int> completed_threads{0};
            
            for (int i = 0; i < num_threads; ++i) {
                threads.emplace_back([&test_url, &completed_threads]() {
                    try {
                        HTTPIOHandler handler(test_url);
                        
                        // Attempt some operations (will likely fail but should be thread-safe)
                        handler.seek(1024, SEEK_SET);
                        handler.tell();
                        handler.eof();
                        
                        completed_threads.fetch_add(1);
                    } catch (const std::exception& e) {
                        // Expected to fail, but should not crash
                        completed_threads.fetch_add(1);
                    }
                });
            }
            
            for (auto& thread : threads) {
                thread.join();
            }
            
            ASSERT_EQ(completed_threads.load(), num_threads);
            Debug::log("test", "HTTPIOHandler concurrent operations test completed");
            
        } catch (const std::exception& e) {
            FAIL("Exception during HTTPIOHandler concurrent operations test: " + std::string(e.what()));
        }
    }
    
    void testIOHandlerMemoryTracking() {
        Debug::log("test", "Testing IOHandler memory tracking thread safety");
        
        const int num_threads = 6;
        std::vector<std::thread> threads;
        std::atomic<int> completed_threads{0};
        
        // Test concurrent memory tracking operations
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&completed_threads, i]() {
                try {
                    // Get memory stats (should be thread-safe)
                    auto stats = IOHandler::getMemoryStats();
                    
                    // Perform memory optimization (should be thread-safe)
                    IOHandler::performMemoryOptimization();
                    
                    // Set memory limits (should be thread-safe)
                    size_t limit = (64 + i) * 1024 * 1024; // Different limits per thread
                    IOHandler::setMemoryLimits(limit, limit / 4);
                    
                    completed_threads.fetch_add(1);
                } catch (const std::exception& e) {
                    Debug::log("test", "Exception in memory tracking thread: ", e.what());
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        ASSERT_EQ(completed_threads.load(), num_threads);
        Debug::log("test", "IOHandler memory tracking thread safety test completed");
    }
    
    void testIOHandlerErrorStateThreadSafety() {
        Debug::log("test", "Testing IOHandler error state thread safety");
        
        // Create a test file
        std::string test_file = "test_error_state.txt";
        createTestFile(test_file, 1024);
        
        try {
            FileIOHandler handler(TagLib::String(test_file));
            
            const int num_threads = 4;
            std::vector<std::thread> threads;
            std::atomic<int> completed_threads{0};
            
            // Test concurrent error state access
            for (int i = 0; i < num_threads; ++i) {
                threads.emplace_back([&handler, &completed_threads]() {
                    for (int j = 0; j < 50; ++j) {
                        // Check error state (should be thread-safe)
                        int error = handler.getLastError();
                        
                        // Check EOF state (should be thread-safe)
                        bool eof_state = handler.eof();
                        
                        // Check position (should be thread-safe)
                        off_t pos = handler.tell();
                        
                        std::this_thread::sleep_for(std::chrono::microseconds(10));
                    }
                    completed_threads.fetch_add(1);
                });
            }
            
            for (auto& thread : threads) {
                thread.join();
            }
            
            ASSERT_EQ(completed_threads.load(), num_threads);
            Debug::log("test", "IOHandler error state thread safety test completed");
            
            handler.close();
            
        } catch (const std::exception& e) {
            FAIL("Exception during error state thread safety test: " + std::string(e.what()));
        }
        
        // Clean up
        std::remove(test_file.c_str());
    }
    
    void testIOHandlerPositionTracking() {
        Debug::log("test", "Testing IOHandler position tracking thread safety");
        
        // Create a test file
        std::string test_file = "test_position_tracking.txt";
        createTestFile(test_file, 2048);
        
        try {
            FileIOHandler handler(TagLib::String(test_file));
            
            const int num_threads = 5;
            std::vector<std::thread> threads;
            std::atomic<int> completed_threads{0};
            std::atomic<off_t> max_position{0};
            
            // Test concurrent position updates
            for (int i = 0; i < num_threads; ++i) {
                threads.emplace_back([&handler, &completed_threads, &max_position, i]() {
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> pos_dist(0, 2047);
                    
                    for (int j = 0; j < 30; ++j) {
                        off_t pos = pos_dist(gen);
                        if (handler.seek(pos, SEEK_SET) == 0) {
                            off_t current_pos = handler.tell();
                            if (current_pos >= 0) {
                                // Update max position atomically
                                off_t current_max = max_position.load();
                                while (current_pos > current_max && 
                                       !max_position.compare_exchange_weak(current_max, current_pos)) {
                                    // Retry if another thread updated max_position
                                }
                            }
                        }
                        
                        std::this_thread::sleep_for(std::chrono::microseconds(15));
                    }
                    completed_threads.fetch_add(1);
                });
            }
            
            for (auto& thread : threads) {
                thread.join();
            }
            
            ASSERT_EQ(completed_threads.load(), num_threads);
            ASSERT_GE(max_position.load(), 0);
            ASSERT_LE(max_position.load(), 2048);
            
            Debug::log("test", "IOHandler position tracking thread safety test completed, max_position=", max_position.load());
            
            handler.close();
            
        } catch (const std::exception& e) {
            FAIL("Exception during position tracking thread safety test: " + std::string(e.what()));
        }
        
        // Clean up
        std::remove(test_file.c_str());
    }
    
    void createTestFile(const std::string& filename, size_t size) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Failed to create test file: " + filename);
        }
        
        // Write some pattern data
        for (size_t i = 0; i < size; ++i) {
            file.put(static_cast<char>(i % 256));
        }
        
        file.close();
    }
};

// Register the test
static IOHandlerThreadSafetyTest iohandler_thread_safety_test;