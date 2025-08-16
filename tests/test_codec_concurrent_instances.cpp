/*
 * test_codec_concurrent_instances.cpp - Concurrent codec instance tests
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
#include <memory>
#include <chrono>
#include <random>
#include <iostream>

/**
 * @brief Concurrent codec instance test suite
 * 
 * Tests requirements for concurrent codec operation:
 * - Requirement 5.3: Support concurrent decoding efficiently
 * - Requirement 11.1: Multiple codec instances maintain independent state
 * - Requirement 11.2: Codec instances don't interfere with each other
 */

static constexpr size_t MAX_CONCURRENT_INSTANCES = 16;
static constexpr size_t OPERATIONS_PER_INSTANCE = 500;
static constexpr size_t PACKET_SIZE = 160; // 20ms at 8kHz

int test_failures = 0;

/**
 * @brief Generate unique test data for each instance
 */
std::vector<uint8_t> generateUniqueTestData(size_t instance_id, size_t size) {
    std::vector<uint8_t> data(size);
    
    // Use instance_id as seed for reproducible but unique data per instance
    std::mt19937 local_rng(instance_id + 12345);
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    
    for (size_t i = 0; i < size; ++i) {
        data[i] = dist(local_rng);
    }
    
    return data;
}

/**
 * @brief Test concurrent codec instances with unique data
 */
template<typename CodecType>
void testConcurrentInstances(const std::string& codec_name, size_t num_instances) {
    std::cout << "Testing concurrent " << codec_name << " instances (" << num_instances << ")..." << std::endl;
    
    try {
        std::atomic<int> success_count{0};
        std::atomic<int> error_count{0};
        std::vector<std::thread> threads;
        std::vector<std::vector<int16_t>> results(num_instances);
        
        // Launch concurrent codec instances
        for (size_t i = 0; i < num_instances; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    // Create stream info
                    StreamInfo stream_info;
                    stream_info.codec_name = codec_name;
                    stream_info.sample_rate = 8000;
                    stream_info.channels = 1;
                    stream_info.bits_per_sample = 8;
                    
                    // Create codec instance
                    CodecType codec(stream_info);
                    if (!codec.initialize()) {
                        error_count++;
                        return;
                    }
                    
                    // Generate unique test data for this instance
                    std::vector<uint8_t> test_data = generateUniqueTestData(i, PACKET_SIZE);
                    
                    // Perform decoding operations
                    std::vector<int16_t> accumulated_samples;
                    
                    for (size_t op = 0; op < OPERATIONS_PER_INSTANCE; ++op) {
                        MediaChunk chunk;
                        chunk.data = test_data;
                        chunk.timestamp_samples = op * PACKET_SIZE;
                        
                        AudioFrame frame = codec.decode(chunk);
                        
                        if (frame.samples.empty()) {
                            error_count++;
                            return;
                        }
                        
                        // Accumulate samples for verification
                        accumulated_samples.insert(accumulated_samples.end(),
                                                 frame.samples.begin(),
                                                 frame.samples.end());
                    }
                    
                    // Store results for cross-verification
                    results[i] = accumulated_samples;
                    success_count++;
                    
                } catch (const std::exception& e) {
                    error_count++;
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Verify results
        std::cout << "  Successful instances: " << success_count.load() << std::endl;
        std::cout << "  Failed instances: " << error_count.load() << std::endl;
        
        // Verify that each instance produced consistent results
        bool results_consistent = true;
        for (size_t i = 0; i < num_instances; ++i) {
            if (results[i].empty()) {
                results_consistent = false;
                break;
            }
            
            // Each instance should produce the same output for the same input
            // (since we use the same test data for each operation within an instance)
            if (results[i].size() != OPERATIONS_PER_INSTANCE * PACKET_SIZE) {
                results_consistent = false;
                break;
            }
        }
        
        // Verify that different instances produce different results
        // (since they use different input data)
        bool instances_independent = true;
        for (size_t i = 1; i < num_instances && instances_independent; ++i) {
            if (!results[i].empty() && !results[0].empty()) {
                // Compare first few samples - they should be different
                // since each instance uses different input data
                size_t compare_size = std::min(results[i].size(), results[0].size());
                compare_size = std::min(compare_size, size_t(100)); // Compare first 100 samples
                
                bool samples_different = false;
                for (size_t j = 0; j < compare_size; ++j) {
                    if (results[i][j] != results[0][j]) {
                        samples_different = true;
                        break;
                    }
                }
                
                if (!samples_different) {
                    instances_independent = false;
                }
            }
        }
        
        if (success_count == static_cast<int>(num_instances) && 
            error_count == 0 && 
            results_consistent && 
            instances_independent) {
            std::cout << "  PASS: Concurrent instances operated independently and correctly" << std::endl;
        } else {
            std::cout << "  FAIL: Concurrent instances test failed - " <<
                       "success: " << success_count.load() <<
                       ", errors: " << error_count.load() <<
                       ", consistent: " << (results_consistent ? "yes" : "no") <<
                       ", independent: " << (instances_independent ? "yes" : "no") << std::endl;
            test_failures++;
        }
        
    } catch (const std::exception& e) {
        std::cout << "  FAIL: Exception in concurrent instances test: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test codec instance lifecycle under concurrent access
 */
template<typename CodecType>
void testInstanceLifecycle(const std::string& codec_name) {
    std::cout << "Testing instance lifecycle (" << codec_name << ")..." << std::endl;
    
    try {
        std::atomic<int> create_count{0};
        std::atomic<int> destroy_count{0};
        std::atomic<int> error_count{0};
        std::vector<std::thread> threads;
        
        // Launch threads that create and destroy codec instances rapidly
        for (size_t i = 0; i < 8; ++i) {
            threads.emplace_back([&, i]() {
                try {
                    for (int j = 0; j < 50; ++j) {
                        // Create codec
                        StreamInfo stream_info;
                        stream_info.codec_name = codec_name;
                        stream_info.sample_rate = 8000;
                        stream_info.channels = 1;
                        
                        auto codec = std::make_unique<CodecType>(stream_info);
                        if (codec->initialize()) {
                            create_count++;
                            
                            // Perform a few operations
                            std::vector<uint8_t> test_data = generateUniqueTestData(i * 50 + j, 80);
                            
                            MediaChunk chunk;
                            chunk.data = test_data;
                            chunk.timestamp_samples = 0;
                            
                            AudioFrame frame = codec->decode(chunk);
                            if (frame.samples.empty()) {
                                error_count++;
                            }
                        } else {
                            error_count++;
                        }
                        
                        // Destroy codec (automatic via unique_ptr)
                        codec.reset();
                        destroy_count++;
                        
                        // Small delay to allow interleaving
                        std::this_thread::sleep_for(std::chrono::microseconds(10));
                    }
                } catch (...) {
                    error_count++;
                }
            });
        }
        
        // Wait for completion
        for (auto& thread : threads) {
            thread.join();
        }
        
        std::cout << "  Created: " << create_count.load() << std::endl;
        std::cout << "  Destroyed: " << destroy_count.load() << std::endl;
        std::cout << "  Errors: " << error_count.load() << std::endl;
        
        if (error_count == 0 && create_count > 0 && destroy_count > 0) {
            std::cout << "  PASS: Instance lifecycle handled correctly" << std::endl;
        } else {
            std::cout << "  FAIL: Instance lifecycle had errors" << std::endl;
            test_failures++;
        }
        
    } catch (const std::exception& e) {
        std::cout << "  FAIL: Exception in lifecycle test: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test concurrent μ-law codec instances
 */
void testMuLawConcurrentInstances() {
    std::cout << "Testing μ-law concurrent instances..." << std::endl;
    
    try {
#ifdef ENABLE_MULAW_CODEC
        // Test with increasing numbers of concurrent instances
        for (size_t num_instances : {2, 4, 8, 16}) {
            testConcurrentInstances<MuLawCodec>("mulaw", num_instances);
        }
        
        testInstanceLifecycle<MuLawCodec>("mulaw");
#else
        std::cout << "  SKIP: μ-law codec not enabled" << std::endl;
#endif
    } catch (const std::exception& e) {
        std::cout << "  FAIL: Exception in μ-law concurrent instances: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test concurrent A-law codec instances
 */
void testALawConcurrentInstances() {
    std::cout << "Testing A-law concurrent instances..." << std::endl;
    
    try {
#ifdef ENABLE_ALAW_CODEC
        // Test with increasing numbers of concurrent instances
        for (size_t num_instances : {2, 4, 8, 16}) {
            testConcurrentInstances<ALawCodec>("alaw", num_instances);
        }
        
        testInstanceLifecycle<ALawCodec>("alaw");
#else
        std::cout << "  SKIP: A-law codec not enabled" << std::endl;
#endif
    } catch (const std::exception& e) {
        std::cout << "  FAIL: Exception in A-law concurrent instances: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test mixed concurrent instances (both codec types)
 */
void testMixedConcurrentInstances() {
    std::cout << "Testing mixed concurrent instances..." << std::endl;
    
    try {
#if defined(ENABLE_MULAW_CODEC) && defined(ENABLE_ALAW_CODEC)
        std::atomic<int> mulaw_success{0};
        std::atomic<int> alaw_success{0};
        std::atomic<int> error_count{0};
        std::vector<std::thread> threads;
        
        // Create mixed threads - some μ-law, some A-law
        for (size_t i = 0; i < 8; ++i) {
            if (i % 2 == 0) {
                // μ-law thread
                threads.emplace_back([&, i]() {
                    try {
                        StreamInfo stream_info;
                        stream_info.codec_name = "mulaw";
                        stream_info.sample_rate = 8000;
                        stream_info.channels = 1;
                        
                        MuLawCodec codec(stream_info);
                        if (!codec.initialize()) {
                            error_count++;
                            return;
                        }
                        
                        std::vector<uint8_t> test_data = generateUniqueTestData(i, PACKET_SIZE);
                        
                        for (size_t op = 0; op < 100; ++op) {
                            MediaChunk chunk;
                            chunk.data = test_data;
                            chunk.timestamp_samples = op * PACKET_SIZE;
                            
                            AudioFrame frame = codec.decode(chunk);
                            if (frame.samples.empty()) {
                                error_count++;
                                return;
                            }
                        }
                        
                        mulaw_success++;
                    } catch (...) {
                        error_count++;
                    }
                });
            } else {
                // A-law thread
                threads.emplace_back([&, i]() {
                    try {
                        StreamInfo stream_info;
                        stream_info.codec_name = "alaw";
                        stream_info.sample_rate = 8000;
                        stream_info.channels = 1;
                        
                        ALawCodec codec(stream_info);
                        if (!codec.initialize()) {
                            error_count++;
                            return;
                        }
                        
                        std::vector<uint8_t> test_data = generateUniqueTestData(i, PACKET_SIZE);
                        
                        for (size_t op = 0; op < 100; ++op) {
                            MediaChunk chunk;
                            chunk.data = test_data;
                            chunk.timestamp_samples = op * PACKET_SIZE;
                            
                            AudioFrame frame = codec.decode(chunk);
                            if (frame.samples.empty()) {
                                error_count++;
                                return;
                            }
                        }
                        
                        alaw_success++;
                    } catch (...) {
                        error_count++;
                    }
                });
            }
        }
        
        // Wait for completion
        for (auto& thread : threads) {
            thread.join();
        }
        
        std::cout << "  μ-law successes: " << mulaw_success.load() << std::endl;
        std::cout << "  A-law successes: " << alaw_success.load() << std::endl;
        std::cout << "  Errors: " << error_count.load() << std::endl;
        
        if (error_count == 0 && mulaw_success == 4 && alaw_success == 4) {
            std::cout << "  PASS: Mixed concurrent instances operated correctly" << std::endl;
        } else {
            std::cout << "  FAIL: Mixed concurrent instances had errors" << std::endl;
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

int main() {
    try {
        std::cout << "=== Codec Concurrent Instance Tests ===" << std::endl;
        
        testMuLawConcurrentInstances();
        testALawConcurrentInstances();
        testMixedConcurrentInstances();
        
        std::cout << "=== Concurrent Instance Tests Complete ===" << std::endl;
        std::cout << "Test failures: " << test_failures << std::endl;
        
        return test_failures > 0 ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << "Test framework error: " << e.what() << std::endl;
        return 1;
    }
}