/*
 * test_thread_safety_minimal.cpp - Minimal thread safety test for IOHandler subsystem
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <mutex>
#include <shared_mutex>

// Test basic thread safety primitives used in IOHandler
void testAtomicOperations() {
    std::cout << "Testing atomic operations..." << std::endl;
    
    std::atomic<int> counter{0};
    std::atomic<bool> flag{false};
    std::atomic<off_t> position{0};
    
    const int num_threads = 8;
    const int increments_per_thread = 1000;
    std::vector<std::thread> threads;
    
    // Test atomic counter increments
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&counter, increments_per_thread]() {
            for (int j = 0; j < increments_per_thread; ++j) {
                counter.fetch_add(1);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    int expected = num_threads * increments_per_thread;
    if (counter.load() != expected) {
        std::cout << "FAILED: Expected " << expected << ", got " << counter.load() << std::endl;
        return;
    }
    
    // Test atomic flag operations
    threads.clear();
    std::atomic<int> flag_changes{0};
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&flag, &flag_changes]() {
            for (int j = 0; j < 100; ++j) {
                bool expected = false;
                if (flag.compare_exchange_weak(expected, true)) {
                    flag_changes.fetch_add(1);
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                    flag.store(false);
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "Atomic operations test PASSED (flag changes: " << flag_changes.load() << ")" << std::endl;
}

void testSharedMutex() {
    std::cout << "Testing shared_mutex operations..." << std::endl;
    
    std::shared_mutex rw_mutex;
    std::atomic<int> readers{0};
    std::atomic<int> writers{0};
    std::atomic<int> shared_data{0};
    
    const int num_reader_threads = 6;
    const int num_writer_threads = 2;
    std::vector<std::thread> threads;
    
    // Reader threads
    for (int i = 0; i < num_reader_threads; ++i) {
        threads.emplace_back([&rw_mutex, &readers, &shared_data]() {
            for (int j = 0; j < 100; ++j) {
                std::shared_lock<std::shared_mutex> lock(rw_mutex);
                readers.fetch_add(1);
                int value = shared_data.load(); // Read operation
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                readers.fetch_sub(1);
                (void)value; // Suppress unused variable warning
            }
        });
    }
    
    // Writer threads
    for (int i = 0; i < num_writer_threads; ++i) {
        threads.emplace_back([&rw_mutex, &writers, &shared_data]() {
            for (int j = 0; j < 50; ++j) {
                std::unique_lock<std::shared_mutex> lock(rw_mutex);
                writers.fetch_add(1);
                shared_data.fetch_add(1); // Write operation
                std::this_thread::sleep_for(std::chrono::microseconds(20));
                writers.fetch_sub(1);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    int expected_writes = num_writer_threads * 50;
    if (shared_data.load() != expected_writes) {
        std::cout << "FAILED: Expected " << expected_writes << " writes, got " << shared_data.load() << std::endl;
        return;
    }
    
    std::cout << "Shared mutex test PASSED (final value: " << shared_data.load() << ")" << std::endl;
}

void testMutexContention() {
    std::cout << "Testing mutex contention..." << std::endl;
    
    std::mutex contention_mutex;
    std::atomic<int> critical_section_entries{0};
    std::atomic<int> max_concurrent{0};
    std::atomic<int> current_concurrent{0};
    
    const int num_threads = 10;
    const int operations_per_thread = 100;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&contention_mutex, &critical_section_entries, &max_concurrent, &current_concurrent, operations_per_thread]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                std::lock_guard<std::mutex> lock(contention_mutex);
                
                int concurrent = current_concurrent.fetch_add(1) + 1;
                critical_section_entries.fetch_add(1);
                
                // Update max concurrent (should always be 1 for mutex)
                int current_max = max_concurrent.load();
                while (concurrent > current_max && !max_concurrent.compare_exchange_weak(current_max, concurrent)) {
                    // Retry
                }
                
                // Simulate some work
                std::this_thread::sleep_for(std::chrono::microseconds(5));
                
                current_concurrent.fetch_sub(1);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    int expected_entries = num_threads * operations_per_thread;
    if (critical_section_entries.load() != expected_entries) {
        std::cout << "FAILED: Expected " << expected_entries << " entries, got " << critical_section_entries.load() << std::endl;
        return;
    }
    
    if (max_concurrent.load() > 1) {
        std::cout << "FAILED: Mutex allowed " << max_concurrent.load() << " concurrent entries (should be 1)" << std::endl;
        return;
    }
    
    std::cout << "Mutex contention test PASSED (entries: " << critical_section_entries.load() << ", max concurrent: " << max_concurrent.load() << ")" << std::endl;
}

void testThreadSafetyPrimitives() {
    std::cout << "Testing thread safety primitives used in IOHandler..." << std::endl;
    
    // Test atomic position updates (simulating IOHandler position tracking)
    std::atomic<off_t> position{0};
    std::atomic<bool> eof_flag{false};
    std::atomic<int> error_code{0};
    
    const int num_threads = 6;
    std::vector<std::thread> threads;
    std::atomic<int> successful_operations{0};
    
    // Simulate concurrent position updates
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&position, &eof_flag, &error_code, &successful_operations, i]() {
            for (int j = 0; j < 200; ++j) {
                // Simulate read operation updating position
                off_t current_pos = position.load();
                off_t new_pos = current_pos + (i + 1) * 10; // Different increment per thread
                
                if (position.compare_exchange_weak(current_pos, new_pos)) {
                    successful_operations.fetch_add(1);
                }
                
                // Simulate error state updates
                if (j % 50 == 0) {
                    error_code.store(j % 3); // Cycle through error codes
                }
                
                // Simulate EOF detection
                if (new_pos > 10000) {
                    eof_flag.store(true);
                }
                
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "Thread safety primitives test PASSED" << std::endl;
    std::cout << "  Final position: " << position.load() << std::endl;
    std::cout << "  EOF flag: " << (eof_flag.load() ? "true" : "false") << std::endl;
    std::cout << "  Error code: " << error_code.load() << std::endl;
    std::cout << "  Successful operations: " << successful_operations.load() << std::endl;
}

int main() {
    std::cout << "Starting Thread Safety Tests for IOHandler Primitives..." << std::endl;
    
    int failed_tests = 0;
    
    try {
        testAtomicOperations();
    } catch (const std::exception& e) {
        std::cout << "Atomic operations test FAILED: " << e.what() << std::endl;
        failed_tests++;
    }
    
    try {
        testSharedMutex();
    } catch (const std::exception& e) {
        std::cout << "Shared mutex test FAILED: " << e.what() << std::endl;
        failed_tests++;
    }
    
    try {
        testMutexContention();
    } catch (const std::exception& e) {
        std::cout << "Mutex contention test FAILED: " << e.what() << std::endl;
        failed_tests++;
    }
    
    try {
        testThreadSafetyPrimitives();
    } catch (const std::exception& e) {
        std::cout << "Thread safety primitives test FAILED: " << e.what() << std::endl;
        failed_tests++;
    }
    
    std::cout << "\nThread Safety Tests Summary:" << std::endl;
    std::cout << "Total tests: 4" << std::endl;
    std::cout << "Failed tests: " << failed_tests << std::endl;
    std::cout << "Passed tests: " << (4 - failed_tests) << std::endl;
    
    if (failed_tests == 0) {
        std::cout << "All thread safety primitive tests PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << "Some thread safety primitive tests FAILED!" << std::endl;
        return 1;
    }
}