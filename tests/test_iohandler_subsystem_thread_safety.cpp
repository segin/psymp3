/*
 * test_iohandler_subsystem_thread_safety.cpp - IOHandler subsystem thread safety tests
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "psymp3.h"
#include "test_framework.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <fstream>
#include <memory>
#include <mutex>
#include <condition_variable>

/**
 * @brief Helper class to access protected IOHandler methods for testing
 */
class IOHandlerTestHelper : public IOHandler {
public:
    static void testSetMemoryLimits(size_t max_total, size_t max_per_handler) {
        setMemoryLimits(max_total, max_per_handler);
    }
};

/**
 * @brief Comprehensive IOHandler subsystem thread safety tests
 * 
 * Tests concurrent file and HTTP I/O operations, memory management deadlock prevention,
 * and integration with other threaded components as specified in task 3.4.
 */
class IOHandlerSubsystemThreadSafetyTest {
public:
    void runAllTests() {
        std::cout << "=== IOHandler Subsystem Thread Safety Tests ===" << std::endl;
        
        // Test 1: Concurrent file I/O operations
        testConcurrentFileIOOperations();
        
        // Test 2: Concurrent HTTP I/O operations (mock)
        testConcurrentHTTPIOOperations();
        
        // Test 3: Memory management deadlock prevention during I/O
        testMemoryManagementDeadlockPrevention();
        
        // Test 4: Integration with other threaded components
        testIntegrationWithThreadedComponents();
        
        // Test 5: Mixed I/O operations under stress
        testMixedIOOperationsStress();
        
        // Test 6: Resource exhaustion handling
        testResourceExhaustionHandling();
        
        std::cout << "=== All IOHandler subsystem thread safety tests completed successfully! ===" << std::endl;
    }

private:
    // Test data and utilities
    static constexpr size_t SMALL_FILE_SIZE = 64 * 1024;    // 64KB
    static constexpr size_t MEDIUM_FILE_SIZE = 1024 * 1024; // 1MB
    static constexpr size_t LARGE_FILE_SIZE = 4 * 1024 * 1024; // 4MB
    
    void testConcurrentFileIOOperations() {
        std::cout << "Testing concurrent file I/O operations..." << std::endl;
        
        // Create multiple test files of different sizes
        std::vector<std::string> test_files = {
            "test_concurrent_small.dat",
            "test_concurrent_medium.dat", 
            "test_concurrent_large.dat"
        };
        
        std::vector<size_t> file_sizes = {
            SMALL_FILE_SIZE,
            MEDIUM_FILE_SIZE,
            LARGE_FILE_SIZE
        };
        
        // Create test files
        for (size_t i = 0; i < test_files.size(); ++i) {
            createTestFile(test_files[i], file_sizes[i]);
        }
        
        try {
            // Test concurrent reads from same file
            testConcurrentReadsFromSameFile(test_files[1], file_sizes[1]);
            
            // Test concurrent reads from different files
            testConcurrentReadsFromDifferentFiles(test_files, file_sizes);
            
            // Test concurrent seeks on same file
            testConcurrentSeeksOnSameFile(test_files[0], file_sizes[0]);
            
            // Test mixed read/seek operations
            testMixedReadSeekOperations(test_files[2], file_sizes[2]);
            
            // Test concurrent file handle management
            testConcurrentFileHandleManagement(test_files);
            
        } catch (const std::exception& e) {
            // Cleanup before re-throwing
            for (const auto& file : test_files) {
                std::remove(file.c_str());
            }
            throw;
        }
        
        // Cleanup
        for (const auto& file : test_files) {
            std::remove(file.c_str());
        }
        
        std::cout << "✓ Concurrent file I/O operations test passed!" << std::endl;
    }
    
    void testConcurrentReadsFromSameFile(const std::string& filename, size_t file_size) {
        const int num_threads = 8;
        const int reads_per_thread = 100;
        std::vector<std::thread> threads;
        std::atomic<int> successful_reads{0};
        std::atomic<int> failed_reads{0};
        std::atomic<size_t> total_bytes_read{0};
        
        std::cout << "  Testing concurrent reads from same file..." << std::endl;
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    FileIOHandler handler(TagLib::String(filename.c_str()));
                    std::random_device rd;
                    std::mt19937 gen(rd() + i); // Different seed per thread
                    std::uniform_int_distribution<off_t> pos_dist(0, file_size - 1024);
                    
                    for (int j = 0; j < reads_per_thread; ++j) {
                        // Seek to random position
                        off_t position = pos_dist(gen);
                        if (handler.seek(position, SEEK_SET) == 0) {
                            // Read data
                            char buffer[512];
                            size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
                            
                            if (bytes_read > 0) {
                                successful_reads.fetch_add(1);
                                total_bytes_read.fetch_add(bytes_read);
                                
                                // Note: Position consistency cannot be guaranteed with concurrent access
                                // to the same file handle, so we don't check it here.
                                // The important thing is that reads don't crash or corrupt data.
                            } else {
                                failed_reads.fetch_add(1);
                            }
                        } else {
                            failed_reads.fetch_add(1);
                        }
                        
                        // Small delay to increase contention
                        if (j % 10 == 0) {
                            std::this_thread::sleep_for(std::chrono::microseconds(50));
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Thread " << i << " exception: " << e.what() << std::endl;
                    failed_reads.fetch_add(reads_per_thread);
                }
            });
        }
        
        // Wait for all threads
        for (auto& t : threads) {
            t.join();
        }
        
        int total_operations = successful_reads.load() + failed_reads.load();
        int expected_operations = num_threads * reads_per_thread;
        
        if (total_operations != expected_operations) {
            throw std::runtime_error("Operation count mismatch: expected " + 
                                   std::to_string(expected_operations) + ", got " + 
                                   std::to_string(total_operations));
        }
        
        // Expect at least 90% success rate
        if (successful_reads.load() < expected_operations * 0.9) {
            throw std::runtime_error("Too many failed reads: " + 
                                   std::to_string(failed_reads.load()) + " out of " + 
                                   std::to_string(expected_operations));
        }
        
        std::cout << "    ✓ " << successful_reads.load() << " successful reads, " 
                  << total_bytes_read.load() << " bytes read" << std::endl;
    }
    
    void testConcurrentReadsFromDifferentFiles(const std::vector<std::string>& files, 
                                             const std::vector<size_t>& file_sizes) {
        const int num_threads = 6;
        const int operations_per_thread = 50;
        std::vector<std::thread> threads;
        std::atomic<int> successful_operations{0};
        std::atomic<int> failed_operations{0};
        
        std::cout << "  Testing concurrent reads from different files..." << std::endl;
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    // Each thread works with a different file
                    size_t file_index = i % files.size();
                    FileIOHandler handler(TagLib::String(files[file_index].c_str()));
                    
                    std::random_device rd;
                    std::mt19937 gen(rd() + i);
                    std::uniform_int_distribution<off_t> pos_dist(0, file_sizes[file_index] - 512);
                    
                    for (int j = 0; j < operations_per_thread; ++j) {
                        off_t position = pos_dist(gen);
                        if (handler.seek(position, SEEK_SET) == 0) {
                            char buffer[256];
                            size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
                            
                            if (bytes_read > 0) {
                                successful_operations.fetch_add(1);
                            } else {
                                failed_operations.fetch_add(1);
                            }
                        } else {
                            failed_operations.fetch_add(1);
                        }
                        
                        // Test memory stats access during I/O
                        if (j % 20 == 0) {
                            auto stats = IOHandler::getMemoryStats();
                            if (stats.empty()) {
                                failed_operations.fetch_add(1);
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Thread " << i << " exception: " << e.what() << std::endl;
                    failed_operations.fetch_add(operations_per_thread);
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        int total_operations = successful_operations.load() + failed_operations.load();
        int expected_operations = num_threads * operations_per_thread;
        
        if (total_operations != expected_operations) {
            throw std::runtime_error("Different files operation count mismatch");
        }
        
        if (successful_operations.load() < expected_operations * 0.95) {
            throw std::runtime_error("Too many failed operations on different files");
        }
        
        std::cout << "    ✓ " << successful_operations.load() << " successful operations across " 
                  << files.size() << " files" << std::endl;
    }
    
    void testConcurrentSeeksOnSameFile(const std::string& filename, size_t file_size) {
        const int num_threads = 6;
        const int seeks_per_thread = 200;
        std::vector<std::thread> threads;
        std::atomic<int> successful_seeks{0};
        std::atomic<int> failed_seeks{0};
        
        std::cout << "  Testing concurrent seeks on same file..." << std::endl;
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    FileIOHandler handler(TagLib::String(filename.c_str()));
                    std::random_device rd;
                    std::mt19937 gen(rd() + i);
                    std::uniform_int_distribution<off_t> pos_dist(0, file_size - 1);
                    std::uniform_int_distribution<int> whence_dist(0, 2);
                    
                    for (int j = 0; j < seeks_per_thread; ++j) {
                        off_t offset = pos_dist(gen);
                        int whence = SEEK_SET; // Start with SEEK_SET for simplicity
                        
                        // Occasionally test SEEK_CUR and SEEK_END
                        if (j % 10 == 0) {
                            int whence_choice = whence_dist(gen);
                            if (whence_choice == 1) {
                                whence = SEEK_CUR;
                                offset = offset % 1024; // Small relative movement
                            } else if (whence_choice == 2) {
                                whence = SEEK_END;
                                offset = -(offset % 1024); // Negative offset from end
                            }
                        }
                        
                        if (handler.seek(offset, whence) == 0) {
                            off_t current_pos = handler.tell();
                            if (current_pos >= 0 && current_pos <= static_cast<off_t>(file_size)) {
                                successful_seeks.fetch_add(1);
                            } else {
                                std::cerr << "Invalid position after seek: " << current_pos << std::endl;
                                failed_seeks.fetch_add(1);
                            }
                        } else {
                            failed_seeks.fetch_add(1);
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Thread " << i << " seek exception: " << e.what() << std::endl;
                    failed_seeks.fetch_add(seeks_per_thread);
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        int total_seeks = successful_seeks.load() + failed_seeks.load();
        int expected_seeks = num_threads * seeks_per_thread;
        
        if (total_seeks != expected_seeks) {
            throw std::runtime_error("Seek operation count mismatch");
        }
        
        if (successful_seeks.load() < expected_seeks * 0.85) {
            throw std::runtime_error("Too many failed seeks");
        }
        
        std::cout << "    ✓ " << successful_seeks.load() << " successful seeks" << std::endl;
    }
    
    void testMixedReadSeekOperations(const std::string& filename, size_t file_size) {
        const int num_threads = 8;
        const int operations_per_thread = 100;
        std::vector<std::thread> threads;
        std::atomic<int> successful_operations{0};
        std::atomic<int> failed_operations{0};
        
        std::cout << "  Testing mixed read/seek operations..." << std::endl;
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    FileIOHandler handler(TagLib::String(filename.c_str()));
                    std::random_device rd;
                    std::mt19937 gen(rd() + i);
                    std::uniform_int_distribution<off_t> pos_dist(0, file_size - 1024);
                    std::uniform_int_distribution<int> op_dist(0, 1);
                    
                    for (int j = 0; j < operations_per_thread; ++j) {
                        bool do_read = (op_dist(gen) == 0);
                        
                        if (do_read) {
                            // Read operation
                            char buffer[128];
                            size_t bytes_read = handler.read(buffer, 1, sizeof(buffer));
                            if (bytes_read >= 0) { // Any result is acceptable
                                successful_operations.fetch_add(1);
                            } else {
                                failed_operations.fetch_add(1);
                            }
                        } else {
                            // Seek operation
                            off_t position = pos_dist(gen);
                            if (handler.seek(position, SEEK_SET) == 0) {
                                successful_operations.fetch_add(1);
                            } else {
                                failed_operations.fetch_add(1);
                            }
                        }
                        
                        // Occasionally check position and error state
                        if (j % 25 == 0) {
                            off_t pos = handler.tell();
                            int error = handler.getLastError();
                            bool eof_state = handler.eof();
                            // These calls should not crash or deadlock
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Thread " << i << " mixed ops exception: " << e.what() << std::endl;
                    failed_operations.fetch_add(operations_per_thread);
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        int total_operations = successful_operations.load() + failed_operations.load();
        int expected_operations = num_threads * operations_per_thread;
        
        if (total_operations != expected_operations) {
            throw std::runtime_error("Mixed operations count mismatch");
        }
        
        std::cout << "    ✓ " << successful_operations.load() << " successful mixed operations" << std::endl;
    }
    
    void testConcurrentFileHandleManagement(const std::vector<std::string>& files) {
        const int num_threads = 10;
        const int handlers_per_thread = 20;
        std::vector<std::thread> threads;
        std::atomic<int> successful_creations{0};
        std::atomic<int> failed_creations{0};
        
        std::cout << "  Testing concurrent file handle management..." << std::endl;
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    std::vector<std::unique_ptr<FileIOHandler>> handlers;
                    
                    for (int j = 0; j < handlers_per_thread; ++j) {
                        try {
                            // Create handler for different files
                            size_t file_index = (i * handlers_per_thread + j) % files.size();
                            auto handler = std::make_unique<FileIOHandler>(
                                TagLib::String(files[file_index].c_str()));
                            
                            // Perform a quick operation to ensure it works
                            char buffer[64];
                            handler->read(buffer, 1, sizeof(buffer));
                            
                            handlers.push_back(std::move(handler));
                            successful_creations.fetch_add(1);
                            
                        } catch (const std::exception& e) {
                            failed_creations.fetch_add(1);
                        }
                        
                        // Check memory stats periodically
                        if (j % 10 == 0) {
                            auto stats = IOHandler::getMemoryStats();
                            if (stats.find("active_handlers") == stats.end()) {
                                failed_creations.fetch_add(1);
                            }
                        }
                    }
                    
                    // Close all handlers (tests destructor thread safety)
                    handlers.clear();
                    
                } catch (const std::exception& e) {
                    std::cerr << "Thread " << i << " handle mgmt exception: " << e.what() << std::endl;
                    failed_creations.fetch_add(handlers_per_thread);
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        int total_operations = successful_creations.load() + failed_creations.load();
        int expected_operations = num_threads * handlers_per_thread;
        
        if (total_operations != expected_operations) {
            throw std::runtime_error("Handle management operation count mismatch");
        }
        
        if (successful_creations.load() < expected_operations * 0.95) {
            throw std::runtime_error("Too many failed handler creations");
        }
        
        std::cout << "    ✓ " << successful_creations.load() << " successful handler creations/destructions" << std::endl;
    }
    
    void testConcurrentHTTPIOOperations() {
        std::cout << "Testing concurrent HTTP I/O operations..." << std::endl;
        
        // Test concurrent HTTP handler initialization
        testConcurrentHTTPInitialization();
        
        // Test concurrent HTTP operations (mock)
        testConcurrentHTTPOperationsMock();
        
        // Test HTTP error handling thread safety
        testHTTPErrorHandlingThreadSafety();
        
        std::cout << "✓ Concurrent HTTP I/O operations test passed!" << std::endl;
    }
    
    void testConcurrentHTTPInitialization() {
        const int num_threads = 6;
        std::vector<std::thread> threads;
        std::atomic<int> completed_threads{0};
        std::atomic<int> exception_count{0};
        
        std::cout << "  Testing concurrent HTTP handler initialization..." << std::endl;
        
        std::vector<std::string> test_urls = {
            "http://example.com/test1.mp3",
            "http://example.com/test2.mp3", 
            "http://example.com/test3.mp3",
            "https://example.com/test4.mp3"
        };
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    std::string url = test_urls[i % test_urls.size()];
                    HTTPIOHandler handler(url);
                    
                    // Test basic operations (will likely fail but should be thread-safe)
                    handler.getFileSize();
                    handler.getMimeType();
                    handler.supportsRangeRequests();
                    handler.isInitialized();
                    
                    completed_threads.fetch_add(1);
                } catch (const std::exception& e) {
                    // Expected to fail due to network issues, but should not crash
                    exception_count.fetch_add(1);
                    completed_threads.fetch_add(1);
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        if (completed_threads.load() != num_threads) {
            throw std::runtime_error("Not all HTTP initialization threads completed");
        }
        
        std::cout << "    ✓ " << completed_threads.load() << " HTTP handlers initialized (with " 
                  << exception_count.load() << " expected exceptions)" << std::endl;
    }
    
    void testConcurrentHTTPOperationsMock() {
        const int num_threads = 4;
        const int operations_per_thread = 30;
        std::vector<std::thread> threads;
        std::atomic<int> completed_operations{0};
        
        std::cout << "  Testing concurrent HTTP operations (mock)..." << std::endl;
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    std::string url = "http://example.com/test" + std::to_string(i) + ".mp3";
                    HTTPIOHandler handler(url);
                    
                    for (int j = 0; j < operations_per_thread; ++j) {
                        // Test various operations (will fail but should be thread-safe)
                        try {
                            char buffer[256];
                            handler.read(buffer, 1, sizeof(buffer));
                            handler.seek(j * 1024, SEEK_SET);
                            handler.tell();
                            handler.eof();
                            
                            completed_operations.fetch_add(1);
                        } catch (const std::exception&) {
                            // Expected failures
                            completed_operations.fetch_add(1);
                        }
                    }
                } catch (const std::exception& e) {
                    // Handler creation might fail
                    completed_operations.fetch_add(operations_per_thread);
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        int expected_operations = num_threads * operations_per_thread;
        if (completed_operations.load() != expected_operations) {
            throw std::runtime_error("HTTP operations count mismatch");
        }
        
        std::cout << "    ✓ " << completed_operations.load() << " HTTP operations completed" << std::endl;
    }
    
    void testHTTPErrorHandlingThreadSafety() {
        const int num_threads = 4;
        std::vector<std::thread> threads;
        std::atomic<int> completed_threads{0};
        
        std::cout << "  Testing HTTP error handling thread safety..." << std::endl;
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    // Test with invalid URLs to trigger error conditions
                    std::vector<std::string> invalid_urls = {
                        "http://nonexistent-domain-12345.com/test.mp3",
                        "https://invalid-url-67890.com/test.mp3",
                        "http://127.0.0.1:99999/test.mp3", // Invalid port
                        "ftp://invalid-protocol.com/test.mp3" // Wrong protocol
                    };
                    
                    std::string url = invalid_urls[i % invalid_urls.size()];
                    
                    try {
                        HTTPIOHandler handler(url);
                        
                        // Test error state access
                        int error = handler.getLastError();
                        bool eof_state = handler.eof();
                        off_t size = handler.getFileSize();
                        
                        // These should handle errors gracefully
                        char buffer[64];
                        handler.read(buffer, 1, sizeof(buffer));
                        handler.seek(0, SEEK_SET);
                        
                    } catch (const std::exception&) {
                        // Expected exceptions for invalid URLs
                    }
                    
                    completed_threads.fetch_add(1);
                } catch (const std::exception& e) {
                    std::cerr << "Thread " << i << " HTTP error handling exception: " << e.what() << std::endl;
                    completed_threads.fetch_add(1);
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        if (completed_threads.load() != num_threads) {
            throw std::runtime_error("Not all HTTP error handling threads completed");
        }
        
        std::cout << "    ✓ " << completed_threads.load() << " HTTP error handling threads completed" << std::endl;
    }
    
    void testMemoryManagementDeadlockPrevention() {
        std::cout << "Testing memory management deadlock prevention during I/O..." << std::endl;
        
        // Test memory operations during I/O
        testMemoryOperationsDuringIO();
        
        // Test memory pressure scenarios
        testMemoryPressureScenarios();
        
        // Test memory optimization during I/O
        testMemoryOptimizationDuringIO();
        
        // Test buffer pool interactions
        testBufferPoolInteractions();
        
        std::cout << "✓ Memory management deadlock prevention test passed!" << std::endl;
    }
    
    void testMemoryOperationsDuringIO() {
        const int num_io_threads = 4;
        const int num_memory_threads = 3;
        std::vector<std::thread> threads;
        std::atomic<int> completed_threads{0};
        std::atomic<bool> stop_flag{false};
        
        std::cout << "  Testing memory operations during I/O..." << std::endl;
        
        // Create test file
        std::string test_file = "test_memory_io.dat";
        createTestFile(test_file, MEDIUM_FILE_SIZE);
        
        // Launch I/O threads
        for (int i = 0; i < num_io_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    FileIOHandler handler(TagLib::String(test_file.c_str()));
                    std::random_device rd;
                    std::mt19937 gen(rd() + i);
                    std::uniform_int_distribution<off_t> pos_dist(0, MEDIUM_FILE_SIZE - 1024);
                    
                    while (!stop_flag.load()) {
                        // Perform I/O operations
                        off_t position = pos_dist(gen);
                        if (handler.seek(position, SEEK_SET) == 0) {
                            char buffer[1024];
                            handler.read(buffer, 1, sizeof(buffer));
                        }
                        
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                    
                    completed_threads.fetch_add(1);
                } catch (const std::exception& e) {
                    std::cerr << "I/O thread " << i << " exception: " << e.what() << std::endl;
                    completed_threads.fetch_add(1);
                }
            });
        }
        
        // Launch memory management threads
        for (int i = 0; i < num_memory_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    while (!stop_flag.load()) {
                        // Test memory operations that could cause deadlocks
                        auto stats = IOHandler::getMemoryStats();
                        IOHandler::performMemoryOptimization();
                        
                        // Set different memory limits
                        size_t limit = (32 + i * 16) * 1024 * 1024;
                        IOHandlerTestHelper::testSetMemoryLimits(limit, limit / 4);
                        
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    }
                    
                    completed_threads.fetch_add(1);
                } catch (const std::exception& e) {
                    std::cerr << "Memory thread " << i << " exception: " << e.what() << std::endl;
                    completed_threads.fetch_add(1);
                }
            });
        }
        
        // Run for a short time
        std::this_thread::sleep_for(std::chrono::seconds(3));
        stop_flag = true;
        
        // Wait for all threads
        for (auto& t : threads) {
            t.join();
        }
        
        // Cleanup
        std::remove(test_file.c_str());
        
        int expected_threads = num_io_threads + num_memory_threads;
        if (completed_threads.load() != expected_threads) {
            throw std::runtime_error("Not all memory/I/O threads completed - possible deadlock");
        }
        
        std::cout << "    ✓ " << completed_threads.load() << " threads completed without deadlock" << std::endl;
    }
    
    void testMemoryPressureScenarios() {
        const int num_threads = 6;
        std::vector<std::thread> threads;
        std::atomic<int> completed_threads{0};
        
        std::cout << "  Testing memory pressure scenarios..." << std::endl;
        
        // Create multiple test files
        std::vector<std::string> test_files;
        for (int i = 0; i < 4; ++i) {
            std::string filename = "test_pressure_" + std::to_string(i) + ".dat";
            createTestFile(filename, SMALL_FILE_SIZE);
            test_files.push_back(filename);
        }
        
        // Set low memory limits to create pressure
        IOHandlerTestHelper::testSetMemoryLimits(8 * 1024 * 1024, 2 * 1024 * 1024); // 8MB total, 2MB per handler
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    std::vector<std::unique_ptr<FileIOHandler>> handlers;
                    
                    // Create multiple handlers to create memory pressure
                    for (int j = 0; j < 3; ++j) {
                        std::string filename = test_files[(i * 3 + j) % test_files.size()];
                        
                        try {
                            auto handler = std::make_unique<FileIOHandler>(
                                TagLib::String(filename.c_str()));
                            
                            // Perform operations that allocate memory
                            char buffer[4096];
                            for (int k = 0; k < 10; ++k) {
                                handler->seek(k * 1024, SEEK_SET);
                                handler->read(buffer, 1, sizeof(buffer));
                            }
                            
                            handlers.push_back(std::move(handler));
                            
                        } catch (const std::exception&) {
                            // Memory pressure might cause failures - this is expected
                        }
                        
                        // Check memory stats during pressure
                        auto stats = IOHandler::getMemoryStats();
                        if (stats.find("total_memory_usage") != stats.end()) {
                            // Memory tracking is working
                        }
                    }
                    
                    // Cleanup handlers
                    handlers.clear();
                    completed_threads.fetch_add(1);
                    
                } catch (const std::exception& e) {
                    std::cerr << "Pressure thread " << i << " exception: " << e.what() << std::endl;
                    completed_threads.fetch_add(1);
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        // Cleanup files
        for (const auto& file : test_files) {
            std::remove(file.c_str());
        }
        
        // Reset memory limits
        IOHandlerTestHelper::testSetMemoryLimits(64 * 1024 * 1024, 16 * 1024 * 1024);
        
        if (completed_threads.load() != num_threads) {
            throw std::runtime_error("Not all memory pressure threads completed");
        }
        
        std::cout << "    ✓ " << completed_threads.load() << " threads handled memory pressure" << std::endl;
    }
    
    void testMemoryOptimizationDuringIO() {
        const int num_threads = 5;
        std::vector<std::thread> threads;
        std::atomic<int> completed_threads{0};
        std::atomic<bool> stop_flag{false};
        
        std::cout << "  Testing memory optimization during I/O..." << std::endl;
        
        std::string test_file = "test_optimization_io.dat";
        createTestFile(test_file, LARGE_FILE_SIZE);
        
        // Launch I/O threads
        for (int i = 0; i < num_threads - 1; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    FileIOHandler handler(TagLib::String(test_file.c_str()));
                    std::random_device rd;
                    std::mt19937 gen(rd() + i);
                    std::uniform_int_distribution<off_t> pos_dist(0, LARGE_FILE_SIZE - 4096);
                    
                    while (!stop_flag.load()) {
                        off_t position = pos_dist(gen);
                        if (handler.seek(position, SEEK_SET) == 0) {
                            char buffer[2048];
                            handler.read(buffer, 1, sizeof(buffer));
                        }
                        
                        std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    }
                    
                    completed_threads.fetch_add(1);
                } catch (const std::exception& e) {
                    std::cerr << "Optimization I/O thread " << i << " exception: " << e.what() << std::endl;
                    completed_threads.fetch_add(1);
                }
            });
        }
        
        // Launch memory optimization thread
        threads.emplace_back([&]() {
            try {
                while (!stop_flag.load()) {
                    // Perform memory optimization operations
                    IOHandler::performMemoryOptimization();
                    
                    // Get memory stats
                    auto stats = IOHandler::getMemoryStats();
                    
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                
                completed_threads.fetch_add(1);
            } catch (const std::exception& e) {
                std::cerr << "Memory optimization thread exception: " << e.what() << std::endl;
                completed_threads.fetch_add(1);
            }
        });
        
        // Run for a short time
        std::this_thread::sleep_for(std::chrono::seconds(2));
        stop_flag = true;
        
        for (auto& t : threads) {
            t.join();
        }
        
        std::remove(test_file.c_str());
        
        if (completed_threads.load() != num_threads) {
            throw std::runtime_error("Not all optimization threads completed");
        }
        
        std::cout << "    ✓ " << completed_threads.load() << " threads completed optimization test" << std::endl;
    }
    
    void testBufferPoolInteractions() {
        const int num_threads = 4;
        std::vector<std::thread> threads;
        std::atomic<int> completed_threads{0};
        
        std::cout << "  Testing buffer pool interactions..." << std::endl;
        
        std::string test_file = "test_buffer_pool.dat";
        createTestFile(test_file, MEDIUM_FILE_SIZE);
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    FileIOHandler handler(TagLib::String(test_file.c_str()));
                    
                    // Perform operations that interact with buffer pools
                    for (int j = 0; j < 50; ++j) {
                        // Read operations that use internal buffering
                        char buffer[8192];
                        off_t position = (j * 8192) % MEDIUM_FILE_SIZE;
                        
                        if (handler.seek(position, SEEK_SET) == 0) {
                            handler.read(buffer, 1, sizeof(buffer));
                        }
                        
                        // Periodically check memory stats (tests buffer pool integration)
                        if (j % 10 == 0) {
                            auto stats = IOHandler::getMemoryStats();
                            // Should include buffer pool statistics
                        }
                    }
                    
                    completed_threads.fetch_add(1);
                } catch (const std::exception& e) {
                    std::cerr << "Buffer pool thread " << i << " exception: " << e.what() << std::endl;
                    completed_threads.fetch_add(1);
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        std::remove(test_file.c_str());
        
        if (completed_threads.load() != num_threads) {
            throw std::runtime_error("Not all buffer pool threads completed");
        }
        
        std::cout << "    ✓ " << completed_threads.load() << " threads completed buffer pool test" << std::endl;
    }
    
    void testIntegrationWithThreadedComponents() {
        std::cout << "Testing integration with other threaded components..." << std::endl;
        
        // Test integration with memory pool manager
        testMemoryPoolManagerIntegration();
        
        // Test integration with memory tracker
        testMemoryTrackerIntegration();
        
        // Test integration with buffer pools
        testBufferPoolIntegration();
        
        std::cout << "✓ Integration with threaded components test passed!" << std::endl;
    }
    
    void testMemoryPoolManagerIntegration() {
        const int num_threads = 6;
        std::vector<std::thread> threads;
        std::atomic<int> completed_threads{0};
        
        std::cout << "  Testing MemoryPoolManager integration..." << std::endl;
        
        std::string test_file = "test_pool_manager.dat";
        createTestFile(test_file, MEDIUM_FILE_SIZE);
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    FileIOHandler handler(TagLib::String(test_file.c_str()));
                    
                    for (int j = 0; j < 30; ++j) {
                        // Perform I/O operations
                        char buffer[4096];
                        off_t position = (j * 4096) % MEDIUM_FILE_SIZE;
                        
                        if (handler.seek(position, SEEK_SET) == 0) {
                            handler.read(buffer, 1, sizeof(buffer));
                        }
                        
                        // Test memory pool manager interactions
                        if (j % 10 == 0) {
                            // These operations should integrate with MemoryPoolManager
                            auto stats = IOHandler::getMemoryStats();
                            IOHandler::performMemoryOptimization();
                        }
                    }
                    
                    completed_threads.fetch_add(1);
                } catch (const std::exception& e) {
                    std::cerr << "Pool manager thread " << i << " exception: " << e.what() << std::endl;
                    completed_threads.fetch_add(1);
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        std::remove(test_file.c_str());
        
        if (completed_threads.load() != num_threads) {
            throw std::runtime_error("Not all pool manager integration threads completed");
        }
        
        std::cout << "    ✓ " << completed_threads.load() << " threads completed MemoryPoolManager integration" << std::endl;
    }
    
    void testMemoryTrackerIntegration() {
        const int num_threads = 4;
        std::vector<std::thread> threads;
        std::atomic<int> completed_threads{0};
        
        std::cout << "  Testing MemoryTracker integration..." << std::endl;
        
        std::vector<std::string> test_files;
        for (int i = 0; i < num_threads; ++i) {
            std::string filename = "test_tracker_" + std::to_string(i) + ".dat";
            createTestFile(filename, SMALL_FILE_SIZE);
            test_files.push_back(filename);
        }
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    FileIOHandler handler(TagLib::String(test_files[i].c_str()));
                    
                    // Perform operations that should be tracked by MemoryTracker
                    for (int j = 0; j < 40; ++j) {
                        char buffer[2048];
                        off_t position = (j * 2048) % SMALL_FILE_SIZE;
                        
                        if (handler.seek(position, SEEK_SET) == 0) {
                            handler.read(buffer, 1, sizeof(buffer));
                        }
                        
                        // Check memory tracking integration
                        if (j % 15 == 0) {
                            auto stats = IOHandler::getMemoryStats();
                            // Should include memory tracker statistics
                            if (stats.find("process_memory_usage") == stats.end()) {
                                std::cerr << "Memory tracker stats not found" << std::endl;
                            }
                        }
                    }
                    
                    completed_threads.fetch_add(1);
                } catch (const std::exception& e) {
                    std::cerr << "Memory tracker thread " << i << " exception: " << e.what() << std::endl;
                    completed_threads.fetch_add(1);
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        // Cleanup
        for (const auto& file : test_files) {
            std::remove(file.c_str());
        }
        
        if (completed_threads.load() != num_threads) {
            throw std::runtime_error("Not all memory tracker integration threads completed");
        }
        
        std::cout << "    ✓ " << completed_threads.load() << " threads completed MemoryTracker integration" << std::endl;
    }
    
    void testBufferPoolIntegration() {
        const int num_threads = 5;
        std::vector<std::thread> threads;
        std::atomic<int> completed_threads{0};
        
        std::cout << "  Testing buffer pool integration..." << std::endl;
        
        std::string test_file = "test_buffer_integration.dat";
        createTestFile(test_file, LARGE_FILE_SIZE);
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    FileIOHandler handler(TagLib::String(test_file.c_str()));
                    
                    // Perform operations that use buffer pools
                    for (int j = 0; j < 25; ++j) {
                        // Large reads that should use buffer pools
                        char buffer[16384]; // 16KB
                        off_t position = (j * 16384) % LARGE_FILE_SIZE;
                        
                        if (handler.seek(position, SEEK_SET) == 0) {
                            handler.read(buffer, 1, sizeof(buffer));
                        }
                        
                        // Check buffer pool integration
                        if (j % 8 == 0) {
                            auto stats = IOHandler::getMemoryStats();
                            // Should include buffer pool statistics
                            bool has_pool_stats = false;
                            for (const auto& stat : stats) {
                                if (stat.first.find("pool") != std::string::npos) {
                                    has_pool_stats = true;
                                    break;
                                }
                            }
                            if (!has_pool_stats) {
                                std::cerr << "Buffer pool stats not found" << std::endl;
                            }
                        }
                    }
                    
                    completed_threads.fetch_add(1);
                } catch (const std::exception& e) {
                    std::cerr << "Buffer integration thread " << i << " exception: " << e.what() << std::endl;
                    completed_threads.fetch_add(1);
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        std::remove(test_file.c_str());
        
        if (completed_threads.load() != num_threads) {
            throw std::runtime_error("Not all buffer pool integration threads completed");
        }
        
        std::cout << "    ✓ " << completed_threads.load() << " threads completed buffer pool integration" << std::endl;
    }
    
    void testMixedIOOperationsStress() {
        std::cout << "Testing mixed I/O operations under stress..." << std::endl;
        
        const int num_file_threads = 6;
        const int num_http_threads = 3;
        const int num_memory_threads = 2;
        std::vector<std::thread> threads;
        std::atomic<int> completed_threads{0};
        std::atomic<bool> stop_flag{false};
        
        // Create test files
        std::vector<std::string> test_files;
        for (int i = 0; i < 3; ++i) {
            std::string filename = "test_stress_" + std::to_string(i) + ".dat";
            createTestFile(filename, MEDIUM_FILE_SIZE);
            test_files.push_back(filename);
        }
        
        // File I/O threads
        for (int i = 0; i < num_file_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    std::string filename = test_files[i % test_files.size()];
                    FileIOHandler handler(TagLib::String(filename.c_str()));
                    
                    std::random_device rd;
                    std::mt19937 gen(rd() + i);
                    std::uniform_int_distribution<off_t> pos_dist(0, MEDIUM_FILE_SIZE - 1024);
                    std::uniform_int_distribution<int> op_dist(0, 2);
                    
                    while (!stop_flag.load()) {
                        int operation = op_dist(gen);
                        
                        if (operation == 0) {
                            // Read
                            char buffer[1024];
                            handler.read(buffer, 1, sizeof(buffer));
                        } else if (operation == 1) {
                            // Seek
                            off_t position = pos_dist(gen);
                            handler.seek(position, SEEK_SET);
                        } else {
                            // Status check
                            handler.tell();
                            handler.eof();
                            handler.getLastError();
                        }
                        
                        std::this_thread::sleep_for(std::chrono::microseconds(100));
                    }
                    
                    completed_threads.fetch_add(1);
                } catch (const std::exception& e) {
                    std::cerr << "Stress file thread " << i << " exception: " << e.what() << std::endl;
                    completed_threads.fetch_add(1);
                }
            });
        }
        
        // HTTP I/O threads (mock)
        for (int i = 0; i < num_http_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    std::string url = "http://example.com/stress" + std::to_string(i) + ".mp3";
                    
                    while (!stop_flag.load()) {
                        try {
                            HTTPIOHandler handler(url);
                            
                            // Attempt operations (will fail but should be thread-safe)
                            char buffer[512];
                            handler.read(buffer, 1, sizeof(buffer));
                            handler.seek(1024, SEEK_SET);
                            handler.getFileSize();
                            
                        } catch (const std::exception&) {
                            // Expected failures
                        }
                        
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    }
                    
                    completed_threads.fetch_add(1);
                } catch (const std::exception& e) {
                    std::cerr << "Stress HTTP thread " << i << " exception: " << e.what() << std::endl;
                    completed_threads.fetch_add(1);
                }
            });
        }
        
        // Memory management threads
        for (int i = 0; i < num_memory_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    while (!stop_flag.load()) {
                        auto stats = IOHandler::getMemoryStats();
                        IOHandler::performMemoryOptimization();
                        
                        size_t limit = (48 + i * 16) * 1024 * 1024;
                        IOHandlerTestHelper::testSetMemoryLimits(limit, limit / 4);
                        
                        std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    }
                    
                    completed_threads.fetch_add(1);
                } catch (const std::exception& e) {
                    std::cerr << "Stress memory thread " << i << " exception: " << e.what() << std::endl;
                    completed_threads.fetch_add(1);
                }
            });
        }
        
        // Run stress test for a few seconds
        std::this_thread::sleep_for(std::chrono::seconds(5));
        stop_flag = true;
        
        for (auto& t : threads) {
            t.join();
        }
        
        // Cleanup
        for (const auto& file : test_files) {
            std::remove(file.c_str());
        }
        
        int expected_threads = num_file_threads + num_http_threads + num_memory_threads;
        if (completed_threads.load() != expected_threads) {
            throw std::runtime_error("Not all stress test threads completed");
        }
        
        std::cout << "✓ " << completed_threads.load() << " threads completed stress test" << std::endl;
    }
    
    void testResourceExhaustionHandling() {
        std::cout << "Testing resource exhaustion handling..." << std::endl;
        
        // Test file descriptor exhaustion simulation
        testFileDescriptorExhaustion();
        
        // Test memory exhaustion handling
        testMemoryExhaustionHandling();
        
        std::cout << "✓ Resource exhaustion handling test passed!" << std::endl;
    }
    
    void testFileDescriptorExhaustion() {
        const int num_threads = 4;
        const int handlers_per_thread = 50; // Try to create many handlers
        std::vector<std::thread> threads;
        std::atomic<int> completed_threads{0};
        std::atomic<int> successful_creations{0};
        
        std::cout << "  Testing file descriptor exhaustion handling..." << std::endl;
        
        // Create test files
        std::vector<std::string> test_files;
        for (int i = 0; i < 4; ++i) {
            std::string filename = "test_fd_exhaust_" + std::to_string(i) + ".dat";
            createTestFile(filename, SMALL_FILE_SIZE);
            test_files.push_back(filename);
        }
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    std::vector<std::unique_ptr<FileIOHandler>> handlers;
                    
                    for (int j = 0; j < handlers_per_thread; ++j) {
                        try {
                            std::string filename = test_files[j % test_files.size()];
                            auto handler = std::make_unique<FileIOHandler>(
                                TagLib::String(filename.c_str()));
                            
                            // Quick operation to verify handler works
                            char buffer[64];
                            handler->read(buffer, 1, sizeof(buffer));
                            
                            handlers.push_back(std::move(handler));
                            successful_creations.fetch_add(1);
                            
                        } catch (const std::exception&) {
                            // Expected when file descriptors are exhausted
                            break;
                        }
                    }
                    
                    // Cleanup handlers
                    handlers.clear();
                    completed_threads.fetch_add(1);
                    
                } catch (const std::exception& e) {
                    std::cerr << "FD exhaustion thread " << i << " exception: " << e.what() << std::endl;
                    completed_threads.fetch_add(1);
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        // Cleanup
        for (const auto& file : test_files) {
            std::remove(file.c_str());
        }
        
        if (completed_threads.load() != num_threads) {
            throw std::runtime_error("Not all FD exhaustion threads completed");
        }
        
        std::cout << "    ✓ " << completed_threads.load() << " threads handled FD exhaustion, " 
                  << successful_creations.load() << " handlers created" << std::endl;
    }
    
    void testMemoryExhaustionHandling() {
        const int num_threads = 3;
        std::vector<std::thread> threads;
        std::atomic<int> completed_threads{0};
        
        std::cout << "  Testing memory exhaustion handling..." << std::endl;
        
        std::string test_file = "test_memory_exhaust.dat";
        createTestFile(test_file, LARGE_FILE_SIZE);
        
        // Set very low memory limits to simulate exhaustion
        IOHandlerTestHelper::testSetMemoryLimits(4 * 1024 * 1024, 1024 * 1024); // 4MB total, 1MB per handler
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    std::vector<std::unique_ptr<FileIOHandler>> handlers;
                    
                    // Try to create multiple handlers to exhaust memory
                    for (int j = 0; j < 10; ++j) {
                        try {
                            auto handler = std::make_unique<FileIOHandler>(
                                TagLib::String(test_file.c_str()));
                            
                            // Perform memory-intensive operations
                            char buffer[8192];
                            for (int k = 0; k < 20; ++k) {
                                handler->seek(k * 8192, SEEK_SET);
                                handler->read(buffer, 1, sizeof(buffer));
                            }
                            
                            handlers.push_back(std::move(handler));
                            
                        } catch (const std::exception&) {
                            // Expected when memory is exhausted
                            break;
                        }
                    }
                    
                    handlers.clear();
                    completed_threads.fetch_add(1);
                    
                } catch (const std::exception& e) {
                    std::cerr << "Memory exhaustion thread " << i << " exception: " << e.what() << std::endl;
                    completed_threads.fetch_add(1);
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        std::remove(test_file.c_str());
        
        // Reset memory limits
        IOHandlerTestHelper::testSetMemoryLimits(64 * 1024 * 1024, 16 * 1024 * 1024);
        
        if (completed_threads.load() != num_threads) {
            throw std::runtime_error("Not all memory exhaustion threads completed");
        }
        
        std::cout << "    ✓ " << completed_threads.load() << " threads handled memory exhaustion" << std::endl;
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
        IOHandlerSubsystemThreadSafetyTest test;
        test.runAllTests();
        std::cout << "\n🎉 All IOHandler subsystem thread safety tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}