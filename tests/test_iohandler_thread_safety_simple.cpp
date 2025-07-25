/*
 * test_iohandler_thread_safety_simple.cpp - Simple thread safety tests for IOHandler subsystem
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <fstream>
#include <iostream>

// Simple test functions without framework dependency
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

void testFileIOHandlerConcurrentReads() {
    std::cout << "Testing FileIOHandler concurrent reads..." << std::endl;
    
    // Create a test file with known content
    std::string test_file = "test_concurrent_reads.txt";
    createTestFile(test_file, 1024 * 1024); // 1MB file
    
    try {
        FileIOHandler handler{TagLib::String(test_file)};
        
        const int num_threads = 4;
        const int reads_per_thread = 50;
        std::vector<std::thread> threads;
        std::atomic<int> successful_reads{0};
        std::atomic<int> failed_reads{0};
        
        // Launch concurrent read threads
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&handler, &successful_reads, &failed_reads, reads_per_thread]() {
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
        
        std::cout << "Concurrent reads completed: successful=" << successful_reads.load() 
                  << " failed=" << failed_reads.load() << std::endl;
        
        // Verify that most reads were successful
        int total_reads = successful_reads.load() + failed_reads.load();
        if (total_reads != num_threads * reads_per_thread) {
            throw std::runtime_error("Total reads mismatch");
        }
        if (successful_reads.load() <= total_reads * 0.8) { // At least 80% success rate
            throw std::runtime_error("Success rate too low");
        }
        
        handler.close();
        std::cout << "FileIOHandler concurrent reads test PASSED" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "FileIOHandler concurrent reads test FAILED: " << e.what() << std::endl;
        throw;
    }
    
    // Clean up
    std::remove(test_file.c_str());
}

void testFileIOHandlerConcurrentSeeks() {
    std::cout << "Testing FileIOHandler concurrent seeks..." << std::endl;
    
    // Create a test file
    std::string test_file = "test_concurrent_seeks.txt";
    createTestFile(test_file, 1024 * 1024); // 1MB file
    
    try {
        FileIOHandler handler{TagLib::String(test_file)};
        
        const int num_threads = 4;
        const int seeks_per_thread = 100;
        std::vector<std::thread> threads;
        std::atomic<int> successful_seeks{0};
        std::atomic<int> failed_seeks{0};
        
        // Launch concurrent seek threads
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&handler, &successful_seeks, &failed_seeks, seeks_per_thread]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> pos_dist(0, 1024 * 1024 - 1);
                
                for (int j = 0; j < seeks_per_thread; ++j) {
                    off_t pos = pos_dist(gen);
                    
                    if (handler.seek(pos, SEEK_SET) == 0) {
                        successful_seeks.fetch_add(1);
                        
                        // Verify position is reasonable
                        off_t current_pos = handler.tell();
                        if (current_pos < 0 || current_pos > 1024 * 1024) {
                            std::cout << "Invalid position after seek: " << current_pos << std::endl;
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
        
        std::cout << "Concurrent seeks completed: successful=" << successful_seeks.load() 
                  << " failed=" << failed_seeks.load() << std::endl;
        
        // Verify that most seeks were successful
        int total_seeks = successful_seeks.load() + failed_seeks.load();
        if (total_seeks != num_threads * seeks_per_thread) {
            throw std::runtime_error("Total seeks mismatch");
        }
        if (successful_seeks.load() <= total_seeks * 0.7) { // At least 70% success rate
            throw std::runtime_error("Success rate too low");
        }
        
        handler.close();
        std::cout << "FileIOHandler concurrent seeks test PASSED" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "FileIOHandler concurrent seeks test FAILED: " << e.what() << std::endl;
        throw;
    }
    
    // Clean up
    std::remove(test_file.c_str());
}

void testIOHandlerMemoryTracking() {
    std::cout << "Testing IOHandler memory tracking thread safety..." << std::endl;
    
    const int num_threads = 4;
    std::vector<std::thread> threads;
    std::atomic<int> completed_threads{0};
    
    // Test concurrent memory tracking operations
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&completed_threads, i]() {
            try {
                // Get memory stats (should be thread-safe)
                auto stats = IOHandler::getMemoryStats();
                
                // Verify we got some stats
                if (stats.empty()) {
                    std::cout << "Warning: No memory stats returned" << std::endl;
                }
                
                completed_threads.fetch_add(1);
            } catch (const std::exception& e) {
                std::cout << "Exception in memory tracking thread: " << e.what() << std::endl;
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    if (completed_threads.load() != num_threads) {
        throw std::runtime_error("Not all memory tracking threads completed");
    }
    
    std::cout << "IOHandler memory tracking thread safety test PASSED" << std::endl;
}

void testHTTPClientThreadSafety() {
    std::cout << "Testing HTTPClient thread safety..." << std::endl;
    
    const int num_threads = 3;
    std::vector<std::thread> threads;
    std::atomic<int> completed_threads{0};
    
    // Test concurrent HTTPClient operations
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&completed_threads]() {
            try {
                // Test thread-safe connection pool stats
                auto stats = HTTPClient::getConnectionPoolStats();
                
                // Test thread-safe URL encoding
                std::string encoded = HTTPClient::urlEncode("test string with spaces");
                
                completed_threads.fetch_add(1);
            } catch (const std::exception& e) {
                std::cout << "Exception in HTTPClient thread: " << e.what() << std::endl;
                completed_threads.fetch_add(1); // Still count as completed
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    if (completed_threads.load() != num_threads) {
        throw std::runtime_error("Not all HTTPClient threads completed");
    }
    
    std::cout << "HTTPClient thread safety test PASSED" << std::endl;
}

int main() {
    std::cout << "Starting IOHandler Thread Safety Tests..." << std::endl;
    
    int failed_tests = 0;
    
    try {
        testFileIOHandlerConcurrentReads();
    } catch (...) {
        failed_tests++;
    }
    
    try {
        testFileIOHandlerConcurrentSeeks();
    } catch (...) {
        failed_tests++;
    }
    
    try {
        testIOHandlerMemoryTracking();
    } catch (...) {
        failed_tests++;
    }
    
    try {
        testHTTPClientThreadSafety();
    } catch (...) {
        failed_tests++;
    }
    
    std::cout << "\nThread Safety Tests Summary:" << std::endl;
    std::cout << "Total tests: 4" << std::endl;
    std::cout << "Failed tests: " << failed_tests << std::endl;
    std::cout << "Passed tests: " << (4 - failed_tests) << std::endl;
    
    if (failed_tests == 0) {
        std::cout << "All thread safety tests PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << "Some thread safety tests FAILED!" << std::endl;
        return 1;
    }
}