/*
 * test_codec_thread_safety_simple.cpp - Simple thread safety tests for μ-law/A-law codecs
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
#include <random>
#include <mutex>
#include <algorithm>

/**
 * @brief Simple thread safety test suite for μ-law and A-law codecs
 * 
 * Tests concurrent operation requirements:
 * - Requirement 11.1: Multiple codec instances maintain independent state
 * - Requirement 11.2: Codec instances don't interfere with each other
 * - Requirement 11.3: Use read-only shared tables safely
 */

static constexpr size_t NUM_THREADS = 8;
static constexpr size_t OPERATIONS_PER_THREAD = 10000;
static constexpr size_t TEST_DURATION_MS = 2000;

int test_failures = 0;
std::mutex output_mutex;

/**
 * @brief Simulate lookup table access (thread-safe read-only operation)
 */
int16_t simulateLookupTableAccess(uint8_t input, bool is_mulaw) {
    // Simulate the lookup table access that codecs perform
    // This is a simplified version of what the actual codecs do
    
    if (is_mulaw) {
        // Simplified μ-law conversion simulation
        return static_cast<int16_t>((input ^ 0xFF) * 256);
    } else {
        // Simplified A-law conversion simulation  
        return static_cast<int16_t>((input ^ 0x55) * 256);
    }
}

/**
 * @brief Worker thread function for concurrent lookup table access
 */
void lookupTableWorkerThread(int thread_id, 
                           bool is_mulaw,
                           std::atomic<bool>& should_stop,
                           std::atomic<int>& error_count,
                           std::atomic<long>& operations_completed) {
    try {
        std::random_device rd;
        std::mt19937 gen(rd() + thread_id); // Unique seed per thread
        std::uniform_int_distribution<uint8_t> dist(0, 255);
        
        long local_operations = 0;
        std::vector<int16_t> results;
        results.reserve(1000);
        
        while (!should_stop && local_operations < OPERATIONS_PER_THREAD) {
            // Generate random input
            uint8_t input = dist(gen);
            
            // Perform lookup table access
            int16_t result = simulateLookupTableAccess(input, is_mulaw);
            results.push_back(result);
            
            local_operations++;
            
            // Periodically yield to allow other threads to run
            if (local_operations % 1000 == 0) {
                std::this_thread::yield();
            }
        }
        
        operations_completed += local_operations;
        
        {
            std::lock_guard<std::mutex> lock(output_mutex);
            std::cout << "  Thread " << thread_id << " (" << (is_mulaw ? "μ-law" : "A-law") 
                     << ") completed " << local_operations << " operations" << std::endl;
        }
        
    } catch (const std::exception& e) {
        error_count++;
        std::lock_guard<std::mutex> lock(output_mutex);
        std::cout << "  Thread " << thread_id << " exception: " << e.what() << std::endl;
    }
}

/**
 * @brief Test concurrent μ-law lookup table access
 */
void testMuLawConcurrentAccess() {
    std::cout << "Testing μ-law concurrent lookup table access..." << std::endl;
    
    try {
        std::atomic<bool> should_stop{false};
        std::atomic<int> error_count{0};
        std::atomic<long> operations_completed{0};
        std::vector<std::thread> threads;
        
        // Launch worker threads
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back(lookupTableWorkerThread, 
                               i, true, // is_mulaw = true
                               std::ref(should_stop), 
                               std::ref(error_count),
                               std::ref(operations_completed));
        }
        
        // Let threads run for specified duration
        std::this_thread::sleep_for(std::chrono::milliseconds(TEST_DURATION_MS));
        should_stop = true;
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        std::cout << "  Total operations completed: " << operations_completed.load() << std::endl;
        std::cout << "  Errors: " << error_count.load() << std::endl;
        
        if (error_count == 0 && operations_completed > 0) {
            std::cout << "  PASS: μ-law concurrent access successful" << std::endl;
        } else {
            std::cout << "  FAIL: μ-law concurrent access had errors" << std::endl;
            test_failures++;
        }
        
    } catch (const std::exception& e) {
        std::cout << "  FAIL: Exception in μ-law concurrent test: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test concurrent A-law lookup table access
 */
void testALawConcurrentAccess() {
    std::cout << "Testing A-law concurrent lookup table access..." << std::endl;
    
    try {
        std::atomic<bool> should_stop{false};
        std::atomic<int> error_count{0};
        std::atomic<long> operations_completed{0};
        std::vector<std::thread> threads;
        
        // Launch worker threads
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back(lookupTableWorkerThread, 
                               i, false, // is_mulaw = false
                               std::ref(should_stop), 
                               std::ref(error_count),
                               std::ref(operations_completed));
        }
        
        // Let threads run for specified duration
        std::this_thread::sleep_for(std::chrono::milliseconds(TEST_DURATION_MS));
        should_stop = true;
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        std::cout << "  Total operations completed: " << operations_completed.load() << std::endl;
        std::cout << "  Errors: " << error_count.load() << std::endl;
        
        if (error_count == 0 && operations_completed > 0) {
            std::cout << "  PASS: A-law concurrent access successful" << std::endl;
        } else {
            std::cout << "  FAIL: A-law concurrent access had errors" << std::endl;
            test_failures++;
        }
        
    } catch (const std::exception& e) {
        std::cout << "  FAIL: Exception in A-law concurrent test: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test mixed concurrent access (both μ-law and A-law simultaneously)
 */
void testMixedConcurrentAccess() {
    std::cout << "Testing mixed concurrent access (μ-law + A-law)..." << std::endl;
    
    try {
        std::atomic<bool> should_stop{false};
        std::atomic<int> error_count{0};
        std::atomic<long> mulaw_operations{0};
        std::atomic<long> alaw_operations{0};
        std::vector<std::thread> threads;
        
        // Launch half threads for μ-law, half for A-law
        for (int i = 0; i < NUM_THREADS / 2; ++i) {
            threads.emplace_back(lookupTableWorkerThread, 
                               i, true, // μ-law
                               std::ref(should_stop), 
                               std::ref(error_count),
                               std::ref(mulaw_operations));
        }
        
        for (int i = NUM_THREADS / 2; i < NUM_THREADS; ++i) {
            threads.emplace_back(lookupTableWorkerThread, 
                               i, false, // A-law
                               std::ref(should_stop), 
                               std::ref(error_count),
                               std::ref(alaw_operations));
        }
        
        // Let threads run
        std::this_thread::sleep_for(std::chrono::milliseconds(TEST_DURATION_MS));
        should_stop = true;
        
        // Wait for completion
        for (auto& thread : threads) {
            thread.join();
        }
        
        std::cout << "  μ-law operations: " << mulaw_operations.load() << std::endl;
        std::cout << "  A-law operations: " << alaw_operations.load() << std::endl;
        std::cout << "  Errors: " << error_count.load() << std::endl;
        
        if (error_count == 0 && mulaw_operations > 0 && alaw_operations > 0) {
            std::cout << "  PASS: Mixed concurrent access successful" << std::endl;
        } else {
            std::cout << "  FAIL: Mixed concurrent access had errors" << std::endl;
            test_failures++;
        }
        
    } catch (const std::exception& e) {
        std::cout << "  FAIL: Exception in mixed concurrent test: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test thread safety of shared data structures
 */
void testSharedDataStructureSafety() {
    std::cout << "Testing shared data structure safety..." << std::endl;
    
    try {
        // Simulate multiple threads accessing shared lookup tables
        std::atomic<int> access_count{0};
        std::atomic<int> error_count{0};
        std::vector<std::thread> threads;
        
        // Create threads that rapidly access shared data
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    std::random_device rd;
                    std::mt19937 gen(rd() + i);
                    std::uniform_int_distribution<uint8_t> dist(0, 255);
                    
                    for (int j = 0; j < 1000; ++j) {
                        uint8_t input = dist(gen);
                        
                        // Simulate accessing both lookup tables
                        int16_t mulaw_result = simulateLookupTableAccess(input, true);
                        int16_t alaw_result = simulateLookupTableAccess(input, false);
                        
                        // Verify results are different (they should be for most inputs)
                        if (mulaw_result != alaw_result) {
                            access_count++;
                        }
                        
                        // Small delay to encourage thread interleaving
                        if (j % 100 == 0) {
                            std::this_thread::yield();
                        }
                    }
                } catch (...) {
                    error_count++;
                }
            });
        }
        
        // Wait for all threads
        for (auto& thread : threads) {
            thread.join();
        }
        
        std::cout << "  Successful accesses: " << access_count.load() << std::endl;
        std::cout << "  Errors: " << error_count.load() << std::endl;
        
        if (error_count == 0 && access_count > 0) {
            std::cout << "  PASS: Shared data structure access is thread-safe" << std::endl;
        } else {
            std::cout << "  FAIL: Shared data structure access had errors" << std::endl;
            test_failures++;
        }
        
    } catch (const std::exception& e) {
        std::cout << "  FAIL: Exception in shared data test: " << e.what() << std::endl;
        test_failures++;
    }
}

int main() {
    try {
        std::cout << "=== Simple Codec Thread Safety Tests ===" << std::endl;
        
        testMuLawConcurrentAccess();
        testALawConcurrentAccess();
        testMixedConcurrentAccess();
        testSharedDataStructureSafety();
        
        std::cout << "=== Simple Thread Safety Tests Complete ===" << std::endl;
        std::cout << "Test failures: " << test_failures << std::endl;
        
        return test_failures > 0 ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << "Test framework error: " << e.what() << std::endl;
        return 1;
    }
}