/*
 * test_flac_frame_size_estimation_properties.cpp - Property-based tests for FLAC frame size estimation
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

#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <random>
#include <memory>
#include <sstream>
#include <iomanip>

// ========================================
// STANDALONE FRAME SIZE ESTIMATION
// ========================================

/**
 * Simulated STREAMINFO structure for testing
 * Based on RFC 9639 Section 8.2
 */
struct TestStreamInfo {
    uint16_t min_block_size = 0;     // Minimum block size in samples (16-65535)
    uint16_t max_block_size = 0;     // Maximum block size in samples (16-65535)
    uint32_t min_frame_size = 0;     // Minimum frame size in bytes (0 if unknown)
    uint32_t max_frame_size = 0;     // Maximum frame size in bytes (0 if unknown)
    uint32_t sample_rate = 0;        // Sample rate in Hz
    uint8_t channels = 0;            // Number of channels (1-8)
    uint8_t bits_per_sample = 0;     // Bits per sample (4-32)
    
    bool isValid() const {
        return sample_rate > 0 && 
               channels > 0 && channels <= 8 && 
               bits_per_sample >= 4 && bits_per_sample <= 32 &&
               min_block_size >= 16 && max_block_size >= 16 &&
               min_block_size <= max_block_size;
    }
};

/**
 * Simulated FLAC frame structure for testing
 */
struct TestFLACFrame {
    uint64_t file_offset = 0;
    uint32_t block_size = 0;
    uint32_t sample_rate = 0;
    uint8_t channels = 0;
    uint8_t bits_per_sample = 0;
};

/**
 * Frame size estimation function matching the implementation in FLACDemuxer
 * 
 * Implements Requirements 21.1, 21.2, 21.5, 25.1, 25.4:
 * - Requirement 21.1: Use STREAMINFO minimum frame size as primary estimate
 * - Requirement 21.2: For fixed block size streams, use minimum directly without scaling
 * - Requirement 21.5: Handle highly compressed streams with frames as small as 14 bytes
 * - Requirement 25.1: Avoid complex theoretical calculations
 * - Requirement 25.4: Prioritize minimum frame size over complex scaling algorithms
 */
uint32_t calculateFrameSize(const TestStreamInfo& streaminfo, const TestFLACFrame& frame) {
    // Method 1: Use STREAMINFO minimum frame size (preferred)
    if (streaminfo.isValid() && streaminfo.min_frame_size > 0) {
        uint32_t estimated_size = streaminfo.min_frame_size;
        
        // For fixed block size streams, use minimum directly
        if (streaminfo.min_block_size == streaminfo.max_block_size) {
            // Ensure minimum valid frame size (14 bytes)
            if (estimated_size < 14) {
                estimated_size = 14;
            }
            return estimated_size;
        }
        
        // Variable block size stream - use linear interpolation
        if (frame.block_size > 0 && streaminfo.max_frame_size > 0) {
            if (frame.block_size <= streaminfo.min_block_size) {
                return streaminfo.min_frame_size;
            }
            
            if (frame.block_size >= streaminfo.max_block_size) {
                return streaminfo.max_frame_size;
            }
            
            // Linear interpolation
            uint32_t block_range = streaminfo.max_block_size - streaminfo.min_block_size;
            uint32_t frame_range = streaminfo.max_frame_size - streaminfo.min_frame_size;
            
            if (block_range > 0) {
                uint32_t block_offset = frame.block_size - streaminfo.min_block_size;
                uint32_t frame_offset_estimate = (block_offset * frame_range) / block_range;
                estimated_size = streaminfo.min_frame_size + frame_offset_estimate;
            }
        }
        
        // Ensure minimum valid frame size
        if (estimated_size < 14) {
            estimated_size = 14;
        }
        
        return estimated_size;
    }
    
    // Method 2: Fallback estimation
    uint32_t frame_overhead = 16;
    uint32_t audio_data_estimate = 0;
    
    if (frame.block_size > 0 && frame.channels > 0 && frame.bits_per_sample > 0) {
        uint32_t uncompressed_size = frame.block_size * frame.channels * (frame.bits_per_sample / 8);
        audio_data_estimate = uncompressed_size / 2;  // 50% compression
    } else {
        audio_data_estimate = 4096;
    }
    
    uint32_t fallback_estimate = frame_overhead + audio_data_estimate;
    
    if (fallback_estimate < 14) {
        fallback_estimate = 14;
    }
    
    static constexpr uint32_t MAX_REASONABLE_FRAME_SIZE = 65536;
    if (fallback_estimate > MAX_REASONABLE_FRAME_SIZE) {
        fallback_estimate = MAX_REASONABLE_FRAME_SIZE;
    }
    
    return fallback_estimate;
}

// ========================================
// PROPERTY-BASED TESTS
// ========================================

// ========================================
// PROPERTY 18: Frame Size Estimation
// ========================================
// **Feature: flac-demuxer, Property 18: Frame Size Estimation**
// **Validates: Requirements 21.1**
//
// For any FLAC stream with valid STREAMINFO, the frame size estimation 
// SHALL use the STREAMINFO minimum frame size as the primary estimate.

void test_property_frame_size_estimation() {
    std::cout << "\n=== Property 18: Frame Size Estimation ===" << std::endl;
    std::cout << "Testing that STREAMINFO minimum frame size is used as primary estimate..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // ----------------------------------------
    // Test 1: Fixed block size streams use min_frame_size directly
    // ----------------------------------------
    std::cout << "\n  Test 1: Fixed block size streams use min_frame_size directly..." << std::endl;
    {
        std::uniform_int_distribution<uint32_t> block_size_dist(16, 65535);
        std::uniform_int_distribution<uint32_t> frame_size_dist(14, 100000);
        std::uniform_int_distribution<uint32_t> sample_rate_dist(8000, 192000);
        std::uniform_int_distribution<uint8_t> channels_dist(1, 8);
        std::uniform_int_distribution<uint8_t> bits_dist(8, 32);
        
        for (int i = 0; i < 100; ++i) {
            TestStreamInfo streaminfo;
            uint32_t block_size = block_size_dist(gen);
            streaminfo.min_block_size = block_size;
            streaminfo.max_block_size = block_size;  // Fixed block size
            streaminfo.min_frame_size = frame_size_dist(gen);
            streaminfo.max_frame_size = streaminfo.min_frame_size + frame_size_dist(gen);
            streaminfo.sample_rate = sample_rate_dist(gen);
            streaminfo.channels = channels_dist(gen);
            streaminfo.bits_per_sample = bits_dist(gen);
            
            TestFLACFrame frame;
            frame.block_size = block_size;
            frame.sample_rate = streaminfo.sample_rate;
            frame.channels = streaminfo.channels;
            frame.bits_per_sample = streaminfo.bits_per_sample;
            
            tests_run++;
            
            uint32_t estimated = calculateFrameSize(streaminfo, frame);
            uint32_t expected = std::max(streaminfo.min_frame_size, 14u);
            
            if (estimated == expected) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Fixed block size stream, expected " << expected 
                          << ", got " << estimated << std::endl;
                assert(false && "Fixed block size should use min_frame_size directly");
            }
        }
        std::cout << "    100 fixed block size tests passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 2: Highly compressed streams (small min_frame_size)
    // ----------------------------------------
    std::cout << "\n  Test 2: Highly compressed streams with small frame sizes..." << std::endl;
    {
        // Test frames as small as 14 bytes (Requirement 21.5)
        std::vector<uint32_t> small_frame_sizes = {14, 15, 16, 20, 30, 50, 100};
        
        for (uint32_t min_frame : small_frame_sizes) {
            TestStreamInfo streaminfo;
            streaminfo.min_block_size = 4096;
            streaminfo.max_block_size = 4096;  // Fixed
            streaminfo.min_frame_size = min_frame;
            streaminfo.max_frame_size = min_frame + 1000;
            streaminfo.sample_rate = 44100;
            streaminfo.channels = 2;
            streaminfo.bits_per_sample = 16;
            
            TestFLACFrame frame;
            frame.block_size = 4096;
            frame.sample_rate = 44100;
            frame.channels = 2;
            frame.bits_per_sample = 16;
            
            tests_run++;
            
            uint32_t estimated = calculateFrameSize(streaminfo, frame);
            uint32_t expected = std::max(min_frame, 14u);
            
            if (estimated == expected) {
                tests_passed++;
                std::cout << "    min_frame_size=" << min_frame << " -> estimated=" << estimated << " ✓" << std::endl;
            } else {
                std::cerr << "    FAILED: min_frame_size=" << min_frame 
                          << ", expected " << expected << ", got " << estimated << std::endl;
                assert(false && "Small frame size handling failed");
            }
        }
    }
    
    // ----------------------------------------
    // Test 3: Variable block size streams use interpolation
    // ----------------------------------------
    std::cout << "\n  Test 3: Variable block size streams use interpolation..." << std::endl;
    {
        TestStreamInfo streaminfo;
        streaminfo.min_block_size = 1024;
        streaminfo.max_block_size = 4096;
        streaminfo.min_frame_size = 1000;
        streaminfo.max_frame_size = 4000;
        streaminfo.sample_rate = 44100;
        streaminfo.channels = 2;
        streaminfo.bits_per_sample = 16;
        
        // Test at min block size
        {
            TestFLACFrame frame;
            frame.block_size = 1024;
            frame.sample_rate = 44100;
            frame.channels = 2;
            frame.bits_per_sample = 16;
            
            tests_run++;
            uint32_t estimated = calculateFrameSize(streaminfo, frame);
            if (estimated == streaminfo.min_frame_size) {
                tests_passed++;
                std::cout << "    At min_block_size: estimated=" << estimated << " ✓" << std::endl;
            } else {
                std::cerr << "    FAILED: At min_block_size, expected " << streaminfo.min_frame_size 
                          << ", got " << estimated << std::endl;
            }
        }
        
        // Test at max block size
        {
            TestFLACFrame frame;
            frame.block_size = 4096;
            frame.sample_rate = 44100;
            frame.channels = 2;
            frame.bits_per_sample = 16;
            
            tests_run++;
            uint32_t estimated = calculateFrameSize(streaminfo, frame);
            if (estimated == streaminfo.max_frame_size) {
                tests_passed++;
                std::cout << "    At max_block_size: estimated=" << estimated << " ✓" << std::endl;
            } else {
                std::cerr << "    FAILED: At max_block_size, expected " << streaminfo.max_frame_size 
                          << ", got " << estimated << std::endl;
            }
        }
        
        // Test at midpoint
        {
            TestFLACFrame frame;
            frame.block_size = 2560;  // Midpoint between 1024 and 4096
            frame.sample_rate = 44100;
            frame.channels = 2;
            frame.bits_per_sample = 16;
            
            tests_run++;
            uint32_t estimated = calculateFrameSize(streaminfo, frame);
            // Should be between min and max
            if (estimated >= streaminfo.min_frame_size && estimated <= streaminfo.max_frame_size) {
                tests_passed++;
                std::cout << "    At midpoint: estimated=" << estimated << " (in range) ✓" << std::endl;
            } else {
                std::cerr << "    FAILED: At midpoint, expected in range [" << streaminfo.min_frame_size 
                          << ", " << streaminfo.max_frame_size << "], got " << estimated << std::endl;
            }
        }
    }
    
    // ----------------------------------------
    // Test 4: Fallback when STREAMINFO unavailable
    // ----------------------------------------
    std::cout << "\n  Test 4: Fallback when STREAMINFO unavailable..." << std::endl;
    {
        TestStreamInfo streaminfo;  // Invalid/empty
        
        TestFLACFrame frame;
        frame.block_size = 4096;
        frame.sample_rate = 44100;
        frame.channels = 2;
        frame.bits_per_sample = 16;
        
        tests_run++;
        uint32_t estimated = calculateFrameSize(streaminfo, frame);
        
        // Should use fallback calculation
        // Expected: 16 (overhead) + (4096 * 2 * 2) / 2 = 16 + 8192 = 8208
        uint32_t expected_fallback = 16 + (4096 * 2 * 2) / 2;
        
        if (estimated == expected_fallback) {
            tests_passed++;
            std::cout << "    Fallback estimate: " << estimated << " ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Fallback expected " << expected_fallback 
                      << ", got " << estimated << std::endl;
        }
    }
    
    // ----------------------------------------
    // Test 5: Minimum frame size floor (14 bytes)
    // ----------------------------------------
    std::cout << "\n  Test 5: Minimum frame size floor (14 bytes)..." << std::endl;
    {
        TestStreamInfo streaminfo;
        streaminfo.min_block_size = 4096;
        streaminfo.max_block_size = 4096;
        streaminfo.min_frame_size = 5;  // Below minimum
        streaminfo.max_frame_size = 1000;
        streaminfo.sample_rate = 44100;
        streaminfo.channels = 2;
        streaminfo.bits_per_sample = 16;
        
        TestFLACFrame frame;
        frame.block_size = 4096;
        
        tests_run++;
        uint32_t estimated = calculateFrameSize(streaminfo, frame);
        
        if (estimated >= 14) {
            tests_passed++;
            std::cout << "    min_frame_size=5 -> estimated=" << estimated << " (>= 14) ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Expected >= 14, got " << estimated << std::endl;
        }
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 18: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// PROPERTY 18b: Random STREAMINFO Values
// ========================================
// Additional test with random STREAMINFO values to verify robustness

void test_property_random_streaminfo_values() {
    std::cout << "\n=== Property 18b: Random STREAMINFO Values ===" << std::endl;
    std::cout << "Testing frame size estimation with 100 random STREAMINFO configurations..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    
    std::uniform_int_distribution<uint32_t> block_size_dist(16, 65535);
    std::uniform_int_distribution<uint32_t> frame_size_dist(14, 100000);
    std::uniform_int_distribution<uint32_t> sample_rate_dist(8000, 192000);
    std::uniform_int_distribution<uint8_t> channels_dist(1, 8);
    std::uniform_int_distribution<uint8_t> bits_dist(8, 32);
    
    for (int i = 0; i < 100; ++i) {
        TestStreamInfo streaminfo;
        uint32_t min_bs = block_size_dist(gen);
        uint32_t max_bs = block_size_dist(gen);
        if (min_bs > max_bs) std::swap(min_bs, max_bs);
        
        streaminfo.min_block_size = min_bs;
        streaminfo.max_block_size = max_bs;
        
        uint32_t min_fs = frame_size_dist(gen);
        uint32_t max_fs = frame_size_dist(gen);
        if (min_fs > max_fs) std::swap(min_fs, max_fs);
        
        streaminfo.min_frame_size = min_fs;
        streaminfo.max_frame_size = max_fs;
        streaminfo.sample_rate = sample_rate_dist(gen);
        streaminfo.channels = channels_dist(gen);
        streaminfo.bits_per_sample = bits_dist(gen);
        
        // Generate random frame within block size range
        std::uniform_int_distribution<uint32_t> frame_block_dist(min_bs, max_bs);
        
        TestFLACFrame frame;
        frame.block_size = frame_block_dist(gen);
        frame.sample_rate = streaminfo.sample_rate;
        frame.channels = streaminfo.channels;
        frame.bits_per_sample = streaminfo.bits_per_sample;
        
        tests_run++;
        
        uint32_t estimated = calculateFrameSize(streaminfo, frame);
        
        // Verify properties:
        // 1. Result should be >= 14 (minimum valid frame size)
        // 2. Result should be reasonable (not excessively large)
        bool valid = (estimated >= 14) && (estimated <= 1000000);
        
        if (valid) {
            tests_passed++;
        } else {
            std::cerr << "  FAILED: Test " << i << ", estimated=" << estimated << std::endl;
            std::cerr << "    min_block_size=" << streaminfo.min_block_size 
                      << ", max_block_size=" << streaminfo.max_block_size << std::endl;
            std::cerr << "    min_frame_size=" << streaminfo.min_frame_size 
                      << ", max_frame_size=" << streaminfo.max_frame_size << std::endl;
            std::cerr << "    frame.block_size=" << frame.block_size << std::endl;
            assert(false && "Frame size estimation produced invalid result");
        }
    }
    
    std::cout << "  " << tests_passed << "/" << tests_run << " random configurations passed ✓" << std::endl;
    std::cout << "\n✓ Property 18b: Random STREAMINFO test PASSED" << std::endl;
    
    assert(tests_passed == tests_run);
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC FRAME SIZE ESTIMATION PROPERTY-BASED TESTS" << std::endl;
    std::cout << "**Feature: flac-demuxer, Property 18: Frame Size Estimation**" << std::endl;
    std::cout << "**Validates: Requirements 21.1**" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // Property 18: Frame Size Estimation
        test_property_frame_size_estimation();
        
        // Property 18b: Random STREAMINFO values
        test_property_random_streaminfo_values();
        
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "✅ ALL PROPERTY TESTS PASSED" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(70, '=') << std::endl;
        std::cerr << "❌ PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Exception: " << e.what() << std::endl;
        std::cerr << std::string(70, '=') << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n" << std::string(70, '=') << std::endl;
        std::cerr << "❌ PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Unknown exception occurred" << std::endl;
        std::cerr << std::string(70, '=') << std::endl;
        return 1;
    }
}
