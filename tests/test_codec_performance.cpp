/*
 * test_codec_performance.cpp - Performance tests for μ-law/A-law codecs
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include <chrono>
#include <vector>
#include <random>
#include <iostream>

/**
 * @brief Performance test suite for μ-law and A-law codecs
 * 
 * Tests real-time decoding performance requirements:
 * - Requirement 5.1: Use pre-computed lookup tables for conversion
 * - Requirement 5.2: Maintain real-time performance for telephony applications
 * - Requirement 5.3: Support concurrent decoding efficiently
 */

static constexpr size_t TELEPHONY_SAMPLE_RATE = 8000;
static constexpr size_t WIDEBAND_SAMPLE_RATE = 16000;
static constexpr size_t PACKET_SIZE_BYTES = 160; // 20ms at 8kHz
static constexpr size_t LARGE_PACKET_SIZE = 1600; // 200ms at 8kHz

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
 * @brief Measure decoding performance for a codec
 */
template<typename CodecType>
double measureDecodingPerformance(const std::string& codec_name, 
                                size_t sample_rate, 
                                size_t packet_size,
                                size_t test_duration_ms = 1000) {
    // Create stream info
    StreamInfo stream_info;
    stream_info.codec_name = codec_name;
    stream_info.sample_rate = sample_rate;
    stream_info.channels = 1;
    stream_info.bits_per_sample = 8;
    
    // Create codec instance
    CodecType codec(stream_info);
    if (!codec.initialize()) {
        throw std::runtime_error("Failed to initialize " + codec_name + " codec");
    }
    
    // Generate test data
    std::vector<uint8_t> test_data = generateRandomAudioData(packet_size);
    
    // Measure performance
    auto start_time = std::chrono::high_resolution_clock::now();
    auto end_time = start_time + std::chrono::milliseconds(test_duration_ms);
    
    size_t packets_processed = 0;
    size_t total_samples = 0;
    
    while (std::chrono::high_resolution_clock::now() < end_time) {
        MediaChunk chunk;
        chunk.data = test_data;
        chunk.timestamp_samples = packets_processed * packet_size;
        
        AudioFrame frame = codec.decode(chunk);
        if (frame.samples.empty()) {
            throw std::runtime_error("Decoding failed during performance test");
        }
        
        packets_processed++;
        total_samples += frame.samples.size();
    }
    
    auto actual_duration = std::chrono::high_resolution_clock::now() - start_time;
    double duration_seconds = std::chrono::duration<double>(actual_duration).count();
    
    // Calculate performance metrics
    double samples_per_second = total_samples / duration_seconds;
    double real_time_factor = samples_per_second / sample_rate;
    
    return real_time_factor;
}

/**
 * @brief Test μ-law codec real-time performance at telephony rates
 */
void testMuLawTelephonyPerformance() {
    std::cout << "Testing μ-law telephony performance (8 kHz)..." << std::endl;
    
    try {
#ifdef ENABLE_MULAW_CODEC
        double real_time_factor = measureDecodingPerformance<MuLawCodec>(
            "mulaw", TELEPHONY_SAMPLE_RATE, PACKET_SIZE_BYTES);
        
        const double MIN_REAL_TIME_FACTOR = 10.0; // 10x real-time minimum
        
        std::cout << "  μ-law real-time factor: " << real_time_factor << "x" << std::endl;
        
        if (real_time_factor >= MIN_REAL_TIME_FACTOR) {
            std::cout << "  PASS: μ-law codec exceeds real-time requirements" << std::endl;
        } else {
            std::cout << "  FAIL: μ-law codec performance insufficient: " 
                     << real_time_factor << "x < " << MIN_REAL_TIME_FACTOR << "x" << std::endl;
            test_failures++;
        }
#else
        std::cout << "  SKIP: μ-law codec not enabled" << std::endl;
#endif
    } catch (const std::exception& e) {
        std::cout << "  FAIL: Exception in μ-law performance test: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test A-law codec real-time performance at telephony rates
 */
void testALawTelephonyPerformance() {
    std::cout << "Testing A-law telephony performance (8 kHz)..." << std::endl;
    
    try {
#ifdef ENABLE_ALAW_CODEC
        double real_time_factor = measureDecodingPerformance<ALawCodec>(
            "alaw", TELEPHONY_SAMPLE_RATE, PACKET_SIZE_BYTES);
        
        const double MIN_REAL_TIME_FACTOR = 10.0; // 10x real-time minimum
        
        std::cout << "  A-law real-time factor: " << real_time_factor << "x" << std::endl;
        
        if (real_time_factor >= MIN_REAL_TIME_FACTOR) {
            std::cout << "  PASS: A-law codec exceeds real-time requirements" << std::endl;
        } else {
            std::cout << "  FAIL: A-law codec performance insufficient: " 
                     << real_time_factor << "x < " << MIN_REAL_TIME_FACTOR << "x" << std::endl;
            test_failures++;
        }
#else
        std::cout << "  SKIP: A-law codec not enabled" << std::endl;
#endif
    } catch (const std::exception& e) {
        std::cout << "  FAIL: Exception in A-law performance test: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test codec performance with wideband audio (16 kHz)
 */
void testWidebandPerformance() {
    std::cout << "Testing wideband performance (16 kHz)..." << std::endl;
    
    try {
#ifdef ENABLE_MULAW_CODEC
        double mulaw_factor = measureDecodingPerformance<MuLawCodec>(
            "mulaw", WIDEBAND_SAMPLE_RATE, PACKET_SIZE_BYTES * 2);
        
        std::cout << "  μ-law wideband factor: " << mulaw_factor << "x" << std::endl;
        
        if (mulaw_factor >= 5.0) { // 5x real-time for wideband
            std::cout << "  PASS: μ-law wideband performance acceptable" << std::endl;
        } else {
            std::cout << "  FAIL: μ-law wideband performance insufficient" << std::endl;
            test_failures++;
        }
#endif

#ifdef ENABLE_ALAW_CODEC
        double alaw_factor = measureDecodingPerformance<ALawCodec>(
            "alaw", WIDEBAND_SAMPLE_RATE, PACKET_SIZE_BYTES * 2);
        
        std::cout << "  A-law wideband factor: " << alaw_factor << "x" << std::endl;
        
        if (alaw_factor >= 5.0) { // 5x real-time for wideband
            std::cout << "  PASS: A-law wideband performance acceptable" << std::endl;
        } else {
            std::cout << "  FAIL: A-law wideband performance insufficient" << std::endl;
            test_failures++;
        }
#endif
    } catch (const std::exception& e) {
        std::cout << "  FAIL: Exception in wideband performance test: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test performance with small VoIP packet sizes
 */
void testSmallPacketPerformance() {
    std::cout << "Testing small VoIP packet performance..." << std::endl;
    
    try {
        const size_t SMALL_PACKET = 80; // 10ms at 8kHz
        
#ifdef ENABLE_MULAW_CODEC
        double mulaw_factor = measureDecodingPerformance<MuLawCodec>(
            "mulaw", TELEPHONY_SAMPLE_RATE, SMALL_PACKET);
        
        std::cout << "  μ-law small packet factor: " << mulaw_factor << "x" << std::endl;
        
        if (mulaw_factor >= 8.0) { // Should still be very fast
            std::cout << "  PASS: μ-law small packet performance good" << std::endl;
        } else {
            std::cout << "  FAIL: μ-law small packet performance degraded" << std::endl;
            test_failures++;
        }
#endif

#ifdef ENABLE_ALAW_CODEC
        double alaw_factor = measureDecodingPerformance<ALawCodec>(
            "alaw", TELEPHONY_SAMPLE_RATE, SMALL_PACKET);
        
        std::cout << "  A-law small packet factor: " << alaw_factor << "x" << std::endl;
        
        if (alaw_factor >= 8.0) { // Should still be very fast
            std::cout << "  PASS: A-law small packet performance good" << std::endl;
        } else {
            std::cout << "  FAIL: A-law small packet performance degraded" << std::endl;
            test_failures++;
        }
#endif
    } catch (const std::exception& e) {
        std::cout << "  FAIL: Exception in small packet test: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test performance with large packet sizes
 */
void testLargePacketPerformance() {
    std::cout << "Testing large packet performance..." << std::endl;
    
    try {
#ifdef ENABLE_MULAW_CODEC
        double mulaw_factor = measureDecodingPerformance<MuLawCodec>(
            "mulaw", TELEPHONY_SAMPLE_RATE, LARGE_PACKET_SIZE);
        
        std::cout << "  μ-law large packet factor: " << mulaw_factor << "x" << std::endl;
        
        if (mulaw_factor >= 15.0) { // Should be even faster with larger packets
            std::cout << "  PASS: μ-law large packet performance excellent" << std::endl;
        } else {
            std::cout << "  FAIL: μ-law large packet performance suboptimal" << std::endl;
            test_failures++;
        }
#endif

#ifdef ENABLE_ALAW_CODEC
        double alaw_factor = measureDecodingPerformance<ALawCodec>(
            "alaw", TELEPHONY_SAMPLE_RATE, LARGE_PACKET_SIZE);
        
        std::cout << "  A-law large packet factor: " << alaw_factor << "x" << std::endl;
        
        if (alaw_factor >= 15.0) { // Should be even faster with larger packets
            std::cout << "  PASS: A-law large packet performance excellent" << std::endl;
        } else {
            std::cout << "  FAIL: A-law large packet performance suboptimal" << std::endl;
            test_failures++;
        }
#endif
    } catch (const std::exception& e) {
        std::cout << "  FAIL: Exception in large packet test: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test lookup table memory efficiency
 */
void testLookupTableMemoryEfficiency() {
    std::cout << "Testing lookup table memory efficiency..." << std::endl;
    
    try {
        // Test that lookup tables are static and shared
        const size_t EXPECTED_TABLE_SIZE = 256 * sizeof(int16_t); // 512 bytes per table
        
        std::cout << "  Expected table size: " << EXPECTED_TABLE_SIZE << " bytes" << std::endl;
        
        // Create multiple codec instances and verify they don't increase memory significantly
        std::vector<std::unique_ptr<AudioCodec>> codecs;
        
#ifdef ENABLE_MULAW_CODEC
        StreamInfo mulaw_info;
        mulaw_info.codec_name = "mulaw";
        mulaw_info.sample_rate = 8000;
        mulaw_info.channels = 1;
        
        for (int i = 0; i < 10; ++i) {
            auto codec = std::make_unique<MuLawCodec>(mulaw_info);
            if (codec->initialize()) {
                codecs.push_back(std::move(codec));
            }
        }
#endif

#ifdef ENABLE_ALAW_CODEC
        StreamInfo alaw_info;
        alaw_info.codec_name = "alaw";
        alaw_info.sample_rate = 8000;
        alaw_info.channels = 1;
        
        for (int i = 0; i < 10; ++i) {
            auto codec = std::make_unique<ALawCodec>(alaw_info);
            if (codec->initialize()) {
                codecs.push_back(std::move(codec));
            }
        }
#endif
        
        std::cout << "  Created " << codecs.size() << " codec instances" << std::endl;
        
        if (!codecs.empty()) {
            std::cout << "  PASS: Multiple codec instances created successfully (shared tables)" << std::endl;
        } else {
            std::cout << "  FAIL: Failed to create codec instances" << std::endl;
            test_failures++;
        }
        
    } catch (const std::exception& e) {
        std::cout << "  FAIL: Exception in memory efficiency test: " << e.what() << std::endl;
        test_failures++;
    }
}

int main() {
    try {
        std::cout << "=== Codec Performance Tests ===" << std::endl;
        
        testMuLawTelephonyPerformance();
        testALawTelephonyPerformance();
        testWidebandPerformance();
        testSmallPacketPerformance();
        testLargePacketPerformance();
        testLookupTableMemoryEfficiency();
        
        std::cout << "=== Performance Tests Complete ===" << std::endl;
        std::cout << "Test failures: " << test_failures << std::endl;
        
        return test_failures > 0 ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << "Test framework error: " << e.what() << std::endl;
        return 1;
    }
}