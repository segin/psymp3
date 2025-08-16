/*
 * test_codec_thread_safety.cpp - Thread safety tests for μ-law/A-law codecs
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
#include <mutex>
#include <chrono>
#include <random>
#include <iostream>

/**
 * @brief Thread safety test suite for μ-law and A-law codecs
 * 
 * Tests concurrent operation requirements:
 * - Requirement 11.1: Multiple codec instances maintain independent state
 * - Requirement 11.2: Codec instances don't interfere with each other
 * - Requirement 11.3: Use read-only shared tables safely
 */

static constexpr size_t NUM_THREADS = 8;
static constexpr size_t OPERATIONS_PER_THREAD = 1000;
static constexpr size_t PACKET_SIZE = 160; // 20ms at 8kHz
static constexpr size_t TEST_DURATION_MS = 5000;

int test_failures = 0;
std::mutex result_mutex;
std::vector<std::string> thread_results;

/**
 * @brief Generate random audio data for testing
 */
std::vector<uint8_t> generateTestData(size_t size) {
    std::vector<uint8_t> data(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    
    for (size_t i = 0; i < size; ++i) {
        data[i] = dist(gen);
    }
    
    return data;
}

/**
 * @brief Worker thread function for concurrent codec testing
 */
template<typename CodecType>
void codecWorkerThread(const std::string& codec_name, 
                      int thread_id,
                      std::atomic<bool>& should_stop,
                      std::atomic<int>& error_count) {
    try {
        // Create stream info
        StreamInfo stream_info;
        stream_info.codec_name = codec_name;
        stream_info.sample_rate = 8000;
        stream_info.channels = 1;
        stream_info.bits_per_sample = 8;
        
        // Create codec instance (each thread has its own)
        CodecType codec(stream_info);
        if (!codec.initialize()) {
            error_count++;
            return;
        }
        
        // Generate test data for this thread
        std::vector<uint8_t> test_data = generateTestData(PACKET_SIZE);
        
        size_t operations = 0;
        std::vector<int16_t> expected_output;
        bool first_decode = true;
        
        while (!should_stop && operations < OPERATIONS_PER_THREAD) {
            // Create media chunk
            MediaChunk chunk;
            chunk.data = test_data;
            chunk.timestamp_samples = operations * PACKET_SIZE;
            
            // Decode audio
            AudioFrame frame = codec.decode(chunk);
            
            if (frame.samples.empty()) {
                error_count++;
                break;
            }
            
            // Verify consistency - same input should produce same output
            if (first_decode) {
                expected_output = frame.samples;
                first_decode = false;
            } else {
                if (frame.samples != expected_output) {
                    std::lock_guard<std::mutex> lock(result_mutex);
                    thread_results.push_back("Thread " + std::to_string(thread_id) + 
                                           ": Inconsistent decode results");
                    error_count++;
                    break;
                }
            }
            
            operations++;
            
            // Small delay to allow thread interleaving
            if (operations % 100 == 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        }
        
        std::lock_guard<std::mutex> lock(result_mutex);
        thread_results.push_back("Thread " + std::to_string(thread_id) + 
                               " completed " + std::to_string(operations) + " operations");
        
    } catch (const std::exception& e) {
        error_count++;
        std::lock_guard<std::mutex> lock(result_mutex);
        thread_results.push_back("Thread " + std::to_string(thread_id) + 
                               " exception: " + e.what());
    }
}

/**
 * @brief Test concurrent initialization of multiple codec instances
 */
template<typename CodecType>
void testConcurrentInitialization(const std::string& codec_name) {
    std::cout << "Testing concurrent " << codec_name << " initialization..." << std::endl;
    
    try {
        std::atomic<int> init_success_count{0};
        std::atomic<int> init_error_count{0};
        std::vector<std::thread> threads;
        
        // Launch multiple threads that initialize codecs simultaneously
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    StreamInfo stream_info;
                    stream_info.codec_name = codec_name;
                    stream_info.sample_rate = 8000;
                    stream_info.channels = 1;
                    
                    CodecType codec(stream_info);
                    if (codec.initialize()) {
                        init_success_count++;
                    } else {
                        init_error_count++;
                    }
                } catch (...) {
                    init_error_count++;
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        std::cout << "  Successful initializations: " << init_success_count.load() << std::endl;
        std::cout << "  Failed initializations: " << init_error_count.load() << std::endl;
        
        if (init_success_count == NUM_THREADS && init_error_count == 0) {
            std::cout << "  PASS: Concurrent initialization successful" << std::endl;
        } else {
            std::cout << "  FAIL: Concurrent initialization had errors" << std::endl;
            test_failures++;
        }
        
    } catch (const std::exception& e) {
        std::cout << "  FAIL: Exception in concurrent initialization: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test concurrent μ-law codec operations
 */
void testMuLawConcurrentOperations() {
    std::cout << "Testing μ-law concurrent operations..." << std::endl;
    
    try {
#ifdef ENABLE_MULAW_CODEC
        thread_results.clear();
        std::atomic<bool> should_stop{false};
        std::atomic<int> error_count{0};
        std::vector<std::thread> threads;
        
        // Launch worker threads
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back(codecWorkerThread<MuLawCodec>, 
                               "mulaw", i, std::ref(should_stop), std::ref(error_count));
        }
        
        // Let threads run for specified duration
        std::this_thread::sleep_for(std::chrono::milliseconds(TEST_DURATION_MS));
        should_stop = true;
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Report results
        for (const auto& result : thread_results) {
            std::cout << "  " << result << std::endl;
        }
        
        if (error_count == 0) {
            std::cout << "  PASS: μ-law concurrent operations successful" << std::endl;
        } else {
            std::cout << "  FAIL: μ-law concurrent operations had " << error_count.load() << " errors" << std::endl;
            test_failures++;
        }
#else
        std::cout << "  SKIP: μ-law codec not enabled" << std::endl;
#endif
    } catch (const std::exception& e) {
        std::cout << "  FAIL: Exception in μ-law concurrent test: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test concurrent A-law codec operations
 */
void testALawConcurrentOperations() {
    std::cout << "Testing A-law concurrent operations..." << std::endl;
    
    try {
#ifdef ENABLE_ALAW_CODEC
        thread_results.clear();
        std::atomic<bool> should_stop{false};
        std::atomic<int> error_count{0};
        std::vector<std::thread> threads;
        
        // Launch worker threads
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back(codecWorkerThread<ALawCodec>, 
                               "alaw", i, std::ref(should_stop), std::ref(error_count));
        }
        
        // Let threads run for specified duration
        std::this_thread::sleep_for(std::chrono::milliseconds(TEST_DURATION_MS));
        should_stop = true;
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Report results
        for (const auto& result : thread_results) {
            std::cout << "  " << result << std::endl;
        }
        
        if (error_count == 0) {
            std::cout << "  PASS: A-law concurrent operations successful" << std::endl;
        } else {
            std::cout << "  FAIL: A-law concurrent operations had " << error_count.load() << " errors" << std::endl;
            test_failures++;
        }
#else
        std::cout << "  SKIP: A-law codec not enabled" << std::endl;
#endif
    } catch (const std::exception& e) {
        std::cout << "  FAIL: Exception in A-law concurrent test: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test concurrent initialization of codec instances
 */
void testConcurrentInitialization() {
    std::cout << "Testing concurrent codec initialization..." << std::endl;
    
    try {
#ifdef ENABLE_MULAW_CODEC
        testConcurrentInitialization<MuLawCodec>("mulaw");
#endif

#ifdef ENABLE_ALAW_CODEC
        testConcurrentInitialization<ALawCodec>("alaw");
#endif
    } catch (const std::exception& e) {
        std::cout << "  FAIL: Exception in concurrent initialization: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test mixed concurrent operations (both codecs simultaneously)
 */
void testMixedConcurrentOperations() {
    std::cout << "Testing mixed concurrent operations (μ-law + A-law)..." << std::endl;
    
    try {
#if defined(ENABLE_MULAW_CODEC) && defined(ENABLE_ALAW_CODEC)
        thread_results.clear();
        std::atomic<bool> should_stop{false};
        std::atomic<int> error_count{0};
        std::vector<std::thread> threads;
        
        // Launch half threads for μ-law, half for A-law
        for (int i = 0; i < NUM_THREADS / 2; ++i) {
            threads.emplace_back(codecWorkerThread<MuLawCodec>, 
                               "mulaw", i, std::ref(should_stop), std::ref(error_count));
        }
        
        for (int i = NUM_THREADS / 2; i < NUM_THREADS; ++i) {
            threads.emplace_back(codecWorkerThread<ALawCodec>, 
                               "alaw", i, std::ref(should_stop), std::ref(error_count));
        }
        
        // Let threads run
        std::this_thread::sleep_for(std::chrono::milliseconds(TEST_DURATION_MS));
        should_stop = true;
        
        // Wait for completion
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Report results
        for (const auto& result : thread_results) {
            std::cout << "  " << result << std::endl;
        }
        
        if (error_count == 0) {
            std::cout << "  PASS: Mixed concurrent operations successful" << std::endl;
        } else {
            std::cout << "  FAIL: Mixed concurrent operations had " << error_count.load() << " errors" << std::endl;
            test_failures++;
        }
#else
        std::cout << "  SKIP: Both codecs not enabled for mixed test" << std::endl;
#endif
    } catch (const std::exception& e) {
        std::cout << "  FAIL: Exception in mixed concurrent test: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test shared lookup table access safety
 */
void testSharedTableAccess() {
    std::cout << "Testing shared lookup table access safety..." << std::endl;
    
    try {
        // This test verifies that multiple threads can safely access
        // the shared lookup tables without corruption or race conditions
        
        std::atomic<int> table_access_count{0};
        std::atomic<int> table_error_count{0};
        std::vector<std::thread> threads;
        
        // Create threads that rapidly create and destroy codec instances
        // This tests the static table initialization safety
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    for (int j = 0; j < 100; ++j) {
#ifdef ENABLE_MULAW_CODEC
                        StreamInfo mulaw_info;
                        mulaw_info.codec_name = "mulaw";
                        mulaw_info.sample_rate = 8000;
                        mulaw_info.channels = 1;
                        
                        MuLawCodec mulaw_codec(mulaw_info);
                        if (mulaw_codec.initialize()) {
                            table_access_count++;
                        } else {
                            table_error_count++;
                        }
#endif

#ifdef ENABLE_ALAW_CODEC
                        StreamInfo alaw_info;
                        alaw_info.codec_name = "alaw";
                        alaw_info.sample_rate = 8000;
                        alaw_info.channels = 1;
                        
                        ALawCodec alaw_codec(alaw_info);
                        if (alaw_codec.initialize()) {
                            table_access_count++;
                        } else {
                            table_error_count++;
                        }
#endif
                    }
                } catch (...) {
                    table_error_count++;
                }
            });
        }
        
        // Wait for all threads
        for (auto& thread : threads) {
            thread.join();
        }
        
        std::cout << "  Table accesses: " << table_access_count.load() << std::endl;
        std::cout << "  Table errors: " << table_error_count.load() << std::endl;
        
        if (table_error_count == 0 && table_access_count > 0) {
            std::cout << "  PASS: Shared table access is thread-safe" << std::endl;
        } else {
            std::cout << "  FAIL: Shared table access had errors" << std::endl;
            test_failures++;
        }
        
    } catch (const std::exception& e) {
        std::cout << "  FAIL: Exception in shared table test: " << e.what() << std::endl;
        test_failures++;
    }
}

int main() {
    try {
        std::cout << "=== Codec Thread Safety Tests ===" << std::endl;
        
        testMuLawConcurrentOperations();
        testALawConcurrentOperations();
        testConcurrentInitialization();
        testMixedConcurrentOperations();
        testSharedTableAccess();
        
        std::cout << "=== Thread Safety Tests Complete ===" << std::endl;
        std::cout << "Test failures: " << test_failures << std::endl;
        
        return test_failures > 0 ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << "Test framework error: " << e.what() << std::endl;
        return 1;
    }
}