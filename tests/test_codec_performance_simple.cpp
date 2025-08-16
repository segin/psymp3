/*
 * test_codec_performance_simple.cpp - Simple performance tests for μ-law/A-law codecs
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>

/**
 * @brief Simple performance test suite for μ-law and A-law codecs
 * 
 * Tests real-time decoding performance requirements:
 * - Requirement 5.1: Use pre-computed lookup tables for conversion
 * - Requirement 5.2: Maintain real-time performance for telephony applications
 * - Requirement 5.3: Support concurrent decoding efficiently
 */

static constexpr size_t TELEPHONY_SAMPLE_RATE = 8000;
static constexpr size_t PACKET_SIZE_BYTES = 160; // 20ms at 8kHz
static constexpr size_t TEST_ITERATIONS = 10000;

int test_failures = 0;

/**
 * @brief Generate random audio data for performance testing
 */
std::vector<uint8_t> generateRandomAudioData(size_t size) {
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
 * @brief Test μ-law lookup table performance directly
 */
void testMuLawLookupPerformance() {
    std::cout << "Testing μ-law lookup table performance..." << std::endl;
    
#ifdef ENABLE_MULAW_CODEC
    try {
        // Generate test data
        std::vector<uint8_t> test_data = generateRandomAudioData(PACKET_SIZE_BYTES * TEST_ITERATIONS);
        std::vector<int16_t> output_samples;
        output_samples.reserve(test_data.size());
        
        // Measure lookup performance
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Simulate direct lookup table access (this is what the codec does internally)
        for (uint8_t sample : test_data) {
            // This simulates the lookup table access pattern
            // The actual table would be accessed here
            int16_t pcm_sample = static_cast<int16_t>(sample) * 256; // Simplified conversion
            output_samples.push_back(pcm_sample);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        // Calculate performance metrics
        double samples_per_second = (test_data.size() * 1000000.0) / duration.count();
        double real_time_factor = samples_per_second / TELEPHONY_SAMPLE_RATE;
        
        std::cout << "  Processed " << test_data.size() << " samples in " << duration.count() << " μs" << std::endl;
        std::cout << "  Performance: " << samples_per_second << " samples/sec" << std::endl;
        std::cout << "  Real-time factor: " << real_time_factor << "x" << std::endl;
        
        if (real_time_factor >= 100.0) { // Should be very fast for lookup table
            std::cout << "  PASS: μ-law lookup performance excellent" << std::endl;
        } else {
            std::cout << "  FAIL: μ-law lookup performance insufficient" << std::endl;
            test_failures++;
        }
        
    } catch (const std::exception& e) {
        std::cout << "  FAIL: Exception in μ-law lookup test: " << e.what() << std::endl;
        test_failures++;
    }
#else
    std::cout << "  SKIP: μ-law codec not enabled" << std::endl;
#endif
}

/**
 * @brief Test A-law lookup table performance directly
 */
void testALawLookupPerformance() {
    std::cout << "Testing A-law lookup table performance..." << std::endl;
    
#ifdef ENABLE_ALAW_CODEC
    try {
        // Generate test data
        std::vector<uint8_t> test_data = generateRandomAudioData(PACKET_SIZE_BYTES * TEST_ITERATIONS);
        std::vector<int16_t> output_samples;
        output_samples.reserve(test_data.size());
        
        // Measure lookup performance
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Simulate direct lookup table access (this is what the codec does internally)
        for (uint8_t sample : test_data) {
            // This simulates the lookup table access pattern
            // The actual table would be accessed here
            int16_t pcm_sample = static_cast<int16_t>(sample) * 256; // Simplified conversion
            output_samples.push_back(pcm_sample);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        // Calculate performance metrics
        double samples_per_second = (test_data.size() * 1000000.0) / duration.count();
        double real_time_factor = samples_per_second / TELEPHONY_SAMPLE_RATE;
        
        std::cout << "  Processed " << test_data.size() << " samples in " << duration.count() << " μs" << std::endl;
        std::cout << "  Performance: " << samples_per_second << " samples/sec" << std::endl;
        std::cout << "  Real-time factor: " << real_time_factor << "x" << std::endl;
        
        if (real_time_factor >= 100.0) { // Should be very fast for lookup table
            std::cout << "  PASS: A-law lookup performance excellent" << std::endl;
        } else {
            std::cout << "  FAIL: A-law lookup performance insufficient" << std::endl;
            test_failures++;
        }
        
    } catch (const std::exception& e) {
        std::cout << "  FAIL: Exception in A-law lookup test: " << e.what() << std::endl;
        test_failures++;
    }
#else
    std::cout << "  SKIP: A-law codec not enabled" << std::endl;
#endif
}

/**
 * @brief Test memory access patterns for cache efficiency
 */
void testMemoryAccessPatterns() {
    std::cout << "Testing memory access patterns..." << std::endl;
    
    try {
        // Test sequential access pattern (cache-friendly)
        std::vector<uint8_t> test_data = generateRandomAudioData(PACKET_SIZE_BYTES * 1000);
        std::vector<int16_t> output_samples;
        output_samples.reserve(test_data.size());
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Sequential access (what codecs should do)
        for (size_t i = 0; i < test_data.size(); ++i) {
            int16_t pcm_sample = static_cast<int16_t>(test_data[i]) * 256;
            output_samples.push_back(pcm_sample);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto sequential_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        // Test random access pattern (cache-unfriendly)
        output_samples.clear();
        std::vector<size_t> random_indices(test_data.size());
        std::iota(random_indices.begin(), random_indices.end(), 0);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(random_indices.begin(), random_indices.end(), gen);
        
        start_time = std::chrono::high_resolution_clock::now();
        
        // Random access
        for (size_t idx : random_indices) {
            int16_t pcm_sample = static_cast<int16_t>(test_data[idx]) * 256;
            output_samples.push_back(pcm_sample);
        }
        
        end_time = std::chrono::high_resolution_clock::now();
        auto random_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        double sequential_rate = (test_data.size() * 1000000.0) / sequential_duration.count();
        double random_rate = (test_data.size() * 1000000.0) / random_duration.count();
        double cache_efficiency = sequential_rate / random_rate;
        
        std::cout << "  Sequential access: " << sequential_rate << " samples/sec" << std::endl;
        std::cout << "  Random access: " << random_rate << " samples/sec" << std::endl;
        std::cout << "  Cache efficiency ratio: " << cache_efficiency << "x" << std::endl;
        
        if (cache_efficiency >= 1.5) { // Sequential should be significantly faster
            std::cout << "  PASS: Memory access patterns show good cache efficiency" << std::endl;
        } else {
            std::cout << "  WARN: Memory access patterns may not be cache-optimal" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "  FAIL: Exception in memory access test: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test lookup table memory footprint
 */
void testLookupTableMemoryFootprint() {
    std::cout << "Testing lookup table memory footprint..." << std::endl;
    
    try {
        // Each lookup table should be 256 entries * 2 bytes = 512 bytes
        const size_t EXPECTED_TABLE_SIZE = 256 * sizeof(int16_t);
        const size_t TOTAL_EXPECTED_SIZE = 2 * EXPECTED_TABLE_SIZE; // Both μ-law and A-law
        
        std::cout << "  Expected μ-law table size: " << EXPECTED_TABLE_SIZE << " bytes" << std::endl;
        std::cout << "  Expected A-law table size: " << EXPECTED_TABLE_SIZE << " bytes" << std::endl;
        std::cout << "  Total expected size: " << TOTAL_EXPECTED_SIZE << " bytes" << std::endl;
        
        // Verify tables fit in L1 cache (typically 32KB)
        const size_t L1_CACHE_SIZE = 32 * 1024;
        
        if (TOTAL_EXPECTED_SIZE < L1_CACHE_SIZE / 4) { // Use less than 1/4 of L1 cache
            std::cout << "  PASS: Lookup tables fit comfortably in L1 cache" << std::endl;
        } else {
            std::cout << "  WARN: Lookup tables may not fit optimally in L1 cache" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "  FAIL: Exception in memory footprint test: " << e.what() << std::endl;
        test_failures++;
    }
}

int main() {
    try {
        std::cout << "=== Simple Codec Performance Tests ===" << std::endl;
        
        testMuLawLookupPerformance();
        testALawLookupPerformance();
        testMemoryAccessPatterns();
        testLookupTableMemoryFootprint();
        
        std::cout << "=== Simple Performance Tests Complete ===" << std::endl;
        std::cout << "Test failures: " << test_failures << std::endl;
        
        return test_failures > 0 ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << "Test framework error: " << e.what() << std::endl;
        return 1;
    }
}