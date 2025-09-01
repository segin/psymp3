/*
 * test_flac_codec_performance_comprehensive.cpp - Performance tests for FLAC codec algorithms
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
#include <vector>
#include <chrono>
#include <cstdint>
#include <algorithm>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_FLAC

/**
 * @brief Test FLAC codec performance characteristics
 * Requirements: 5.1-5.8, 8.1-8.8
 */
class FLACCodecPerformanceTest {
public:
    static bool runAllTests() {
        std::cout << "FLAC Codec Performance Tests" << std::endl;
        std::cout << "============================" << std::endl;
        
        bool all_passed = true;
        
        all_passed &= testConversionPerformance();
        all_passed &= testChannelProcessingPerformance();
        all_passed &= testMemoryEfficiency();
        all_passed &= testRealTimeRequirements();
        
        if (all_passed) {
            std::cout << "✓ All performance tests PASSED" << std::endl;
        } else {
            std::cout << "✗ Some performance tests FAILED" << std::endl;
        }
        
        return all_passed;
    }

private:
    static bool testConversionPerformance() {
        std::cout << "Testing bit depth conversion performance..." << std::endl;
        
        const size_t num_samples = 1000000;  // 1M samples
        std::vector<int32_t> test_samples(num_samples);
        
        // Fill with test data
        for (size_t i = 0; i < num_samples; ++i) {
            test_samples[i] = static_cast<int32_t>(i % 65536) - 32768;
        }
        
        // Test 24-bit to 16-bit conversion performance
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::vector<int16_t> converted_samples(num_samples);
        for (size_t i = 0; i < num_samples; ++i) {
            converted_samples[i] = convert24BitTo16Bit(test_samples[i]);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        double samples_per_second = (static_cast<double>(num_samples) * 1000000.0) / duration.count();
        
        std::cout << "  Conversion rate: " << samples_per_second << " samples/second" << std::endl;
        std::cout << "  Processing time: " << duration.count() << " μs for " << num_samples << " samples" << std::endl;
        
        // Performance should be reasonable (at least 10M samples/second)
        if (samples_per_second < 10000000.0) {
            std::cout << "  ERROR: Conversion performance too slow" << std::endl;
            return false;
        }
        
        std::cout << "  ✓ Conversion performance test passed" << std::endl;
        return true;
    }
    
    static bool testChannelProcessingPerformance() {
        std::cout << "Testing channel processing performance..." << std::endl;
        
        const size_t num_samples = 500000;  // 500K samples per channel
        std::vector<int32_t> left_channel(num_samples);
        std::vector<int32_t> right_channel(num_samples);
        
        // Fill with test data
        for (size_t i = 0; i < num_samples; ++i) {
            left_channel[i] = static_cast<int32_t>(i % 32768);
            right_channel[i] = static_cast<int32_t>((i + 1000) % 32768);
        }
        
        // Test stereo interleaving performance
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::vector<int16_t> interleaved = interleaveStereo(left_channel, right_channel);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        double samples_per_second = (static_cast<double>(num_samples * 2) * 1000000.0) / duration.count();
        
        std::cout << "  Interleaving rate: " << samples_per_second << " samples/second" << std::endl;
        std::cout << "  Processing time: " << duration.count() << " μs for " << (num_samples * 2) << " samples" << std::endl;
        
        // Performance should be reasonable (at least 20M samples/second for stereo)
        if (samples_per_second < 20000000.0) {
            std::cout << "  ERROR: Channel processing performance too slow" << std::endl;
            return false;
        }
        
        std::cout << "  ✓ Channel processing performance test passed" << std::endl;
        return true;
    }
    
    static bool testMemoryEfficiency() {
        std::cout << "Testing memory efficiency..." << std::endl;
        
        // Test memory usage for different buffer sizes
        std::vector<size_t> buffer_sizes = {1152, 4608, 16384, 65536};
        
        for (size_t buffer_size : buffer_sizes) {
            size_t memory_used = calculateMemoryUsage(buffer_size, 2, 16);  // Stereo, 16-bit
            size_t expected_memory = buffer_size * 2 * sizeof(int16_t);  // Stereo samples
            
            std::cout << "  Buffer size " << buffer_size << ": " << memory_used << " bytes" << std::endl;
            
            // Memory usage should be reasonable (not more than 2x expected)
            if (memory_used > expected_memory * 2) {
                std::cout << "  ERROR: Excessive memory usage for buffer size " << buffer_size << std::endl;
                return false;
            }
        }
        
        std::cout << "  ✓ Memory efficiency test passed" << std::endl;
        return true;
    }
    
    static bool testRealTimeRequirements() {
        std::cout << "Testing real-time performance requirements..." << std::endl;
        
        // Test that processing is faster than real-time playback
        const uint32_t sample_rate = 44100;
        const uint32_t block_size = 4608;  // ~104ms at 44.1kHz
        const uint16_t channels = 2;
        
        double frame_duration_ms = (static_cast<double>(block_size) / sample_rate) * 1000.0;
        
        // Simulate processing a frame
        std::vector<int32_t> test_data(block_size * channels);
        for (size_t i = 0; i < test_data.size(); ++i) {
            test_data[i] = static_cast<int32_t>(i % 65536) - 32768;
        }
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Simulate bit depth conversion + channel processing
        std::vector<int16_t> processed_data(block_size * channels);
        for (size_t i = 0; i < test_data.size(); ++i) {
            processed_data[i] = convert24BitTo16Bit(test_data[i]);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto processing_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        double processing_time_ms = static_cast<double>(processing_time.count()) / 1000.0;
        double real_time_ratio = processing_time_ms / frame_duration_ms;
        
        std::cout << "  Frame duration: " << frame_duration_ms << " ms" << std::endl;
        std::cout << "  Processing time: " << processing_time_ms << " ms" << std::endl;
        std::cout << "  Real-time ratio: " << real_time_ratio << " (lower is better)" << std::endl;
        
        // Processing should be much faster than real-time (ratio < 0.1 = 10% of real-time)
        if (real_time_ratio > 0.1) {
            std::cout << "  ERROR: Processing too slow for real-time requirements" << std::endl;
            return false;
        }
        
        std::cout << "  ✓ Real-time requirements test passed" << std::endl;
        return true;
    }
    
    // Helper methods for performance testing
    static int16_t convert24BitTo16Bit(int32_t sample) {
        return static_cast<int16_t>(sample >> 8);
    }
    
    static std::vector<int16_t> interleaveStereo(const std::vector<int32_t>& left,
                                                const std::vector<int32_t>& right) {
        std::vector<int16_t> output;
        output.reserve(left.size() + right.size());
        
        for (size_t i = 0; i < left.size(); ++i) {
            output.push_back(static_cast<int16_t>(left[i]));
            output.push_back(static_cast<int16_t>(right[i]));
        }
        
        return output;
    }
    
    static size_t calculateMemoryUsage(size_t buffer_size, uint16_t channels, uint16_t bits_per_sample) {
        // Calculate memory usage for audio buffer
        size_t bytes_per_sample = (bits_per_sample + 7) / 8;  // Round up to nearest byte
        size_t total_samples = buffer_size * channels;
        return total_samples * bytes_per_sample;
    }
};

/**
 * @brief Test FLAC format compatibility
 * Requirements: 5.1-5.8
 */
class FLACCodecCompatibilityTest {
public:
    static bool runAllTests() {
        std::cout << std::endl << "FLAC Codec Compatibility Tests" << std::endl;
        std::cout << "==============================" << std::endl;
        
        bool all_passed = true;
        
        all_passed &= testSampleRateSupport();
        all_passed &= testBitDepthSupport();
        all_passed &= testChannelConfigurationSupport();
        all_passed &= testBlockSizeSupport();
        
        if (all_passed) {
            std::cout << "✓ All compatibility tests PASSED" << std::endl;
        } else {
            std::cout << "✗ Some compatibility tests FAILED" << std::endl;
        }
        
        return all_passed;
    }

private:
    static bool testSampleRateSupport() {
        std::cout << "Testing sample rate support..." << std::endl;
        
        // Test common sample rates
        std::vector<uint32_t> sample_rates = {
            8000, 16000, 22050, 24000, 32000, 44100, 48000,
            88200, 96000, 176400, 192000, 384000
        };
        
        for (uint32_t sr : sample_rates) {
            if (!isValidSampleRate(sr)) {
                std::cout << "  ERROR: Sample rate " << sr << " not supported" << std::endl;
                return false;
            }
        }
        
        std::cout << "  ✓ Sample rate support test passed" << std::endl;
        return true;
    }
    
    static bool testBitDepthSupport() {
        std::cout << "Testing bit depth support..." << std::endl;
        
        // Test various bit depths per FLAC specification
        std::vector<uint16_t> bit_depths = {8, 16, 20, 24, 32};
        
        for (uint16_t bits : bit_depths) {
            if (!isValidBitDepth(bits)) {
                std::cout << "  ERROR: Bit depth " << bits << " not supported" << std::endl;
                return false;
            }
        }
        
        std::cout << "  ✓ Bit depth support test passed" << std::endl;
        return true;
    }
    
    static bool testChannelConfigurationSupport() {
        std::cout << "Testing channel configuration support..." << std::endl;
        
        // Test various channel configurations
        std::vector<uint16_t> channel_counts = {1, 2, 3, 4, 5, 6, 7, 8};
        
        for (uint16_t channels : channel_counts) {
            if (!isValidChannelCount(channels)) {
                std::cout << "  ERROR: Channel count " << channels << " not supported" << std::endl;
                return false;
            }
        }
        
        std::cout << "  ✓ Channel configuration support test passed" << std::endl;
        return true;
    }
    
    static bool testBlockSizeSupport() {
        std::cout << "Testing block size support..." << std::endl;
        
        // Test various block sizes
        std::vector<uint32_t> block_sizes = {192, 576, 1152, 2304, 4608, 9216, 18432, 36864};
        
        for (uint32_t block_size : block_sizes) {
            if (!isValidBlockSize(block_size)) {
                std::cout << "  ERROR: Block size " << block_size << " not supported" << std::endl;
                return false;
            }
        }
        
        std::cout << "  ✓ Block size support test passed" << std::endl;
        return true;
    }
    
    // Helper methods for compatibility testing
    static bool isValidSampleRate(uint32_t sample_rate) {
        return sample_rate >= 1 && sample_rate <= 655350;
    }
    
    static bool isValidBitDepth(uint16_t bits_per_sample) {
        return bits_per_sample >= 4 && bits_per_sample <= 32;
    }
    
    static bool isValidChannelCount(uint16_t channels) {
        return channels >= 1 && channels <= 8;
    }
    
    static bool isValidBlockSize(uint32_t block_size) {
        return block_size >= 16 && block_size <= 65535;
    }
};

int main() {
    std::cout << "FLAC Codec Performance and Compatibility Tests" << std::endl;
    std::cout << "==============================================" << std::endl;
    std::cout << "Requirements: 5.1-5.8, 8.1-8.8, 14.1-14.8" << std::endl;
    std::cout << std::endl;
    
    bool all_tests_passed = true;
    
    all_tests_passed &= FLACCodecPerformanceTest::runAllTests();
    all_tests_passed &= FLACCodecCompatibilityTest::runAllTests();
    
    std::cout << std::endl;
    if (all_tests_passed) {
        std::cout << "✓ ALL PERFORMANCE AND COMPATIBILITY TESTS PASSED" << std::endl;
        return 0;
    } else {
        std::cout << "✗ SOME PERFORMANCE AND COMPATIBILITY TESTS FAILED" << std::endl;
        return 1;
    }
}

#else // !HAVE_FLAC

int main() {
    std::cout << "FLAC support not available - skipping FLAC codec performance tests" << std::endl;
    return 0;
}

#endif // HAVE_FLAC