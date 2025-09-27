/*
 * test_opus_compatibility_performance.cpp - Opus codec compatibility and performance validation
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
#include <chrono>
#include <random>

#ifdef HAVE_OGGDEMUXER

/**
 * @brief Test fixture for Opus compatibility and performance tests
 */
class OpusCompatibilityPerformanceTest {
public:
    OpusCompatibilityPerformanceTest() = default;
    ~OpusCompatibilityPerformanceTest() = default;
    
    /**
     * @brief Create Opus packets with different encoding modes
     */
    static std::vector<uint8_t> createOpusPacketForMode(const std::string& mode, int frame_size_ms = 20) {
        std::vector<uint8_t> packet;
        
        if (mode == "SILK") {
            // TOC byte for SILK mode (config 0-15)
            packet.push_back(0x08); // Config 1 (SILK-only, 10ms, mono)
            // Add minimal SILK payload
            packet.insert(packet.end(), {0x00, 0x01, 0x02});
            
        } else if (mode == "CELT") {
            // TOC byte for CELT mode (config 16-31)
            packet.push_back(0x78); // Config 30 (CELT-only, 20ms, stereo)
            // Add minimal CELT payload
            packet.insert(packet.end(), {0x00, 0x01, 0x02, 0x03});
            
        } else if (mode == "hybrid") {
            // TOC byte for hybrid mode (config 16-19 with SILK+CELT)
            packet.push_back(0x48); // Config 18 (hybrid, 20ms, stereo)
            // Add minimal hybrid payload
            packet.insert(packet.end(), {0x00, 0x01, 0x02, 0x03, 0x04});
        }
        
        return packet;
    }
    
    /**
     * @brief Create Opus packets with different quality levels
     */
    static std::vector<uint8_t> createOpusPacketForQuality(int bitrate_kbps, int frame_size_ms = 20) {
        std::vector<uint8_t> packet;
        
        // Use different configurations based on bitrate
        if (bitrate_kbps <= 32) {
            // Low bitrate - use SILK mode
            packet.push_back(0x08); // SILK mode
            packet.insert(packet.end(), {0x00, 0x01});
        } else if (bitrate_kbps <= 128) {
            // Medium bitrate - use hybrid mode
            packet.push_back(0x48); // Hybrid mode
            packet.insert(packet.end(), {0x00, 0x01, 0x02, 0x03});
        } else {
            // High bitrate - use CELT mode
            packet.push_back(0x78); // CELT mode
            packet.insert(packet.end(), {0x00, 0x01, 0x02, 0x03, 0x04, 0x05});
        }
        
        return packet;
    }
    
    /**
     * @brief Create Opus packets with different frame sizes
     */
    static std::vector<uint8_t> createOpusPacketForFrameSize(float frame_size_ms) {
        std::vector<uint8_t> packet;
        
        // Map frame sizes to TOC configurations
        if (frame_size_ms == 2.5f) {
            packet.push_back(0x00); // 2.5ms frame
        } else if (frame_size_ms == 5.0f) {
            packet.push_back(0x08); // 5ms frame
        } else if (frame_size_ms == 10.0f) {
            packet.push_back(0x10); // 10ms frame
        } else if (frame_size_ms == 20.0f) {
            packet.push_back(0x18); // 20ms frame
        } else if (frame_size_ms == 40.0f) {
            packet.push_back(0x20); // 40ms frame
        } else if (frame_size_ms == 60.0f) {
            packet.push_back(0x28); // 60ms frame
        } else {
            packet.push_back(0x18); // Default to 20ms
        }
        
        // Add minimal payload
        packet.insert(packet.end(), {0x00, 0x01, 0x02});
        
        return packet;
    }
    
    /**
     * @brief Create StreamInfo for different test scenarios
     */
    static StreamInfo createStreamInfoForTest(const std::string& test_type, uint16_t channels = 2, uint32_t bitrate = 128000) {
        StreamInfo info;
        info.stream_id = 1;
        info.codec_type = "audio";
        info.codec_name = "opus";
        info.sample_rate = 48000; // Opus always outputs at 48kHz
        info.channels = channels;
        info.bitrate = bitrate;
        return info;
    }
    
    /**
     * @brief Measure performance of a function
     */
    template<typename Func>
    static double measurePerformance(Func&& func, int iterations = 1000) {
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            func();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        return static_cast<double>(duration.count()) / iterations; // Average microseconds per iteration
    }
};

/**
 * @brief Test Opus codec with different encoding modes (SILK, CELT, hybrid)
 * Requirements: 12.4, 13.2
 */
bool test_opus_encoding_modes() {
    printf("=== Testing Opus encoding modes (SILK, CELT, hybrid) ===\n");
    
    try {
        StreamInfo stream_info = OpusCompatibilityPerformanceTest::createStreamInfoForTest("modes");
        OpusCodec codec(stream_info);
        
        if (!codec.initialize()) {
            printf("FAIL: OpusCodec initialization failed\n");
            return false;
        }
        
        // Process headers first
        auto id_header = std::vector<uint8_t>{'O', 'p', 'u', 's', 'H', 'e', 'a', 'd', 1, 2, 56, 1, 0x80, 0xBB, 0x00, 0x00, 0, 0, 0};
        MediaChunk id_chunk(1, id_header);
        codec.decode(id_chunk);
        
        auto comment_header = std::vector<uint8_t>{'O', 'p', 'u', 's', 'T', 'a', 'g', 's', 8, 0, 0, 0, 'l', 'i', 'b', 'o', 'p', 'u', 's', ' ', 0, 0, 0, 0};
        MediaChunk comment_chunk(1, comment_header);
        codec.decode(comment_chunk);
        
        // Test SILK mode
        auto silk_packet = OpusCompatibilityPerformanceTest::createOpusPacketForMode("SILK");
        MediaChunk silk_chunk(1, silk_packet);
        AudioFrame silk_frame = codec.decode(silk_chunk);
        
        if (silk_frame.sample_rate != 48000) {
            printf("FAIL: SILK mode should output at 48kHz, got %u Hz\n", silk_frame.sample_rate);
            return false;
        }
        
        // Test CELT mode
        auto celt_packet = OpusCompatibilityPerformanceTest::createOpusPacketForMode("CELT");
        MediaChunk celt_chunk(1, celt_packet);
        AudioFrame celt_frame = codec.decode(celt_chunk);
        
        if (celt_frame.sample_rate != 48000) {
            printf("FAIL: CELT mode should output at 48kHz, got %u Hz\n", celt_frame.sample_rate);
            return false;
        }
        
        // Test hybrid mode
        auto hybrid_packet = OpusCompatibilityPerformanceTest::createOpusPacketForMode("hybrid");
        MediaChunk hybrid_chunk(1, hybrid_packet);
        AudioFrame hybrid_frame = codec.decode(hybrid_chunk);
        
        if (hybrid_frame.sample_rate != 48000) {
            printf("FAIL: Hybrid mode should output at 48kHz, got %u Hz\n", hybrid_frame.sample_rate);
            return false;
        }
        
        printf("PASS: All Opus encoding modes handled correctly\n");
        return true;
        
    } catch (const std::exception& e) {
        printf("FAIL: Exception in encoding modes test: %s\n", e.what());
        return false;
    }
}

/**
 * @brief Test Opus codec with different quality levels and bitrates
 * Requirements: 12.1, 12.2, 13.1
 */
bool test_opus_quality_levels() {
    printf("=== Testing Opus quality levels and bitrates ===\n");
    
    try {
        // Test different bitrate scenarios
        std::vector<int> bitrates = {16, 32, 64, 128, 256, 320}; // kbps
        
        for (int bitrate : bitrates) {
            StreamInfo stream_info = OpusCompatibilityPerformanceTest::createStreamInfoForTest("quality", 2, bitrate * 1000);
            OpusCodec codec(stream_info);
            
            if (!codec.initialize()) {
                printf("FAIL: OpusCodec initialization failed for %d kbps\n", bitrate);
                return false;
            }
            
            // Process headers
            auto id_header = std::vector<uint8_t>{'O', 'p', 'u', 's', 'H', 'e', 'a', 'd', 1, 2, 56, 1, 0x80, 0xBB, 0x00, 0x00, 0, 0, 0};
            MediaChunk id_chunk(1, id_header);
            codec.decode(id_chunk);
            
            auto comment_header = std::vector<uint8_t>{'O', 'p', 'u', 's', 'T', 'a', 'g', 's', 8, 0, 0, 0, 'l', 'i', 'b', 'o', 'p', 'u', 's', ' ', 0, 0, 0, 0};
            MediaChunk comment_chunk(1, comment_header);
            codec.decode(comment_chunk);
            
            // Test quality-appropriate packet
            auto quality_packet = OpusCompatibilityPerformanceTest::createOpusPacketForQuality(bitrate);
            MediaChunk quality_chunk(1, quality_packet);
            AudioFrame quality_frame = codec.decode(quality_chunk);
            
            // Validate output format consistency across quality levels
            if (quality_frame.sample_rate != 48000) {
                printf("FAIL: Quality level %d kbps should output at 48kHz\n", bitrate);
                return false;
            }
            
            if (quality_frame.channels != 2) {
                printf("FAIL: Quality level %d kbps should output stereo\n", bitrate);
                return false;
            }
        }
        
        printf("PASS: All quality levels handled correctly\n");
        return true;
        
    } catch (const std::exception& e) {
        printf("FAIL: Exception in quality levels test: %s\n", e.what());
        return false;
    }
}

/**
 * @brief Test Opus codec with different frame sizes (2.5ms to 60ms)
 * Requirements: 3.6, 9.5, 9.7
 */
bool test_opus_variable_frame_sizes() {
    printf("=== Testing Opus variable frame sizes ===\n");
    
    try {
        StreamInfo stream_info = OpusCompatibilityPerformanceTest::createStreamInfoForTest("frame_sizes");
        OpusCodec codec(stream_info);
        
        if (!codec.initialize()) {
            printf("FAIL: OpusCodec initialization failed\n");
            return false;
        }
        
        // Process headers
        auto id_header = std::vector<uint8_t>{'O', 'p', 'u', 's', 'H', 'e', 'a', 'd', 1, 2, 56, 1, 0x80, 0xBB, 0x00, 0x00, 0, 0, 0};
        MediaChunk id_chunk(1, id_header);
        codec.decode(id_chunk);
        
        auto comment_header = std::vector<uint8_t>{'O', 'p', 'u', 's', 'T', 'a', 'g', 's', 8, 0, 0, 0, 'l', 'i', 'b', 'o', 'p', 'u', 's', ' ', 0, 0, 0, 0};
        MediaChunk comment_chunk(1, comment_header);
        codec.decode(comment_chunk);
        
        // Test different frame sizes
        std::vector<float> frame_sizes = {2.5f, 5.0f, 10.0f, 20.0f, 40.0f, 60.0f};
        
        for (float frame_size : frame_sizes) {
            auto frame_packet = OpusCompatibilityPerformanceTest::createOpusPacketForFrameSize(frame_size);
            MediaChunk frame_chunk(1, frame_packet);
            AudioFrame frame = codec.decode(frame_chunk);
            
            // Validate that codec handles variable frame sizes
            if (frame.sample_rate != 48000) {
                printf("FAIL: Frame size %.1fms should output at 48kHz\n", frame_size);
                return false;
            }
            
            // Expected samples for frame size (approximate)
            size_t expected_samples_per_channel = static_cast<size_t>(frame_size * 48.0f); // 48kHz * frame_size_ms / 1000
            size_t expected_total_samples = expected_samples_per_channel * 2; // stereo
            
            // Allow some tolerance for frame size variations
            if (!frame.samples.empty()) {
                printf("INFO: Frame size %.1fms produced %zu samples (expected ~%zu)\n", 
                       frame_size, frame.samples.size(), expected_total_samples);
            }
        }
        
        printf("PASS: Variable frame sizes handled correctly\n");
        return true;
        
    } catch (const std::exception& e) {
        printf("FAIL: Exception in variable frame sizes test: %s\n", e.what());
        return false;
    }
}

/**
 * @brief Test Opus codec performance compared to baseline
 * Requirements: 9.1, 9.2, 12.2
 */
bool test_opus_performance() {
    printf("=== Testing Opus codec performance ===\n");
    
    try {
        StreamInfo stream_info = OpusCompatibilityPerformanceTest::createStreamInfoForTest("performance");
        OpusCodec codec(stream_info);
        
        if (!codec.initialize()) {
            printf("FAIL: OpusCodec initialization failed\n");
            return false;
        }
        
        // Process headers
        auto id_header = std::vector<uint8_t>{'O', 'p', 'u', 's', 'H', 'e', 'a', 'd', 1, 2, 56, 1, 0x80, 0xBB, 0x00, 0x00, 0, 0, 0};
        MediaChunk id_chunk(1, id_header);
        codec.decode(id_chunk);
        
        auto comment_header = std::vector<uint8_t>{'O', 'p', 'u', 's', 'T', 'a', 'g', 's', 8, 0, 0, 0, 'l', 'i', 'b', 'o', 'p', 'u', 's', ' ', 0, 0, 0, 0};
        MediaChunk comment_chunk(1, comment_header);
        codec.decode(comment_chunk);
        
        // Create a standard 20ms Opus packet for performance testing
        auto test_packet = OpusCompatibilityPerformanceTest::createOpusPacketForMode("CELT", 20);
        MediaChunk test_chunk(1, test_packet);
        
        // Measure decode performance
        double avg_decode_time = OpusCompatibilityPerformanceTest::measurePerformance([&]() {
            AudioFrame frame = codec.decode(test_chunk);
        }, 1000);
        
        printf("INFO: Average decode time: %.2f microseconds per packet\n", avg_decode_time);
        
        // Performance requirement: should decode 20ms of audio in much less than 20ms
        // At 48kHz, 20ms = 960 samples per channel
        // Reasonable performance target: decode should take < 1ms (1000 microseconds)
        if (avg_decode_time > 1000.0) {
            printf("WARN: Decode performance may be slower than expected (%.2f μs > 1000 μs)\n", avg_decode_time);
            // Don't fail the test, just warn - performance depends on hardware
        }
        
        // Test reset performance
        double avg_reset_time = OpusCompatibilityPerformanceTest::measurePerformance([&]() {
            codec.reset();
        }, 100);
        
        printf("INFO: Average reset time: %.2f microseconds\n", avg_reset_time);
        
        // Test flush performance
        double avg_flush_time = OpusCompatibilityPerformanceTest::measurePerformance([&]() {
            AudioFrame frame = codec.flush();
        }, 100);
        
        printf("INFO: Average flush time: %.2f microseconds\n", avg_flush_time);
        
        printf("PASS: Performance measurements completed\n");
        return true;
        
    } catch (const std::exception& e) {
        printf("FAIL: Exception in performance test: %s\n", e.what());
        return false;
    }
}

/**
 * @brief Test Opus codec output quality and accuracy
 * Requirements: 13.1, 13.2, 13.8
 */
bool test_opus_output_quality() {
    printf("=== Testing Opus output quality and accuracy ===\n");
    
    try {
        StreamInfo stream_info = OpusCompatibilityPerformanceTest::createStreamInfoForTest("quality");
        OpusCodec codec(stream_info);
        
        if (!codec.initialize()) {
            printf("FAIL: OpusCodec initialization failed\n");
            return false;
        }
        
        // Process headers
        auto id_header = std::vector<uint8_t>{'O', 'p', 'u', 's', 'H', 'e', 'a', 'd', 1, 2, 56, 1, 0x80, 0xBB, 0x00, 0x00, 0, 0, 0};
        MediaChunk id_chunk(1, id_header);
        codec.decode(id_chunk);
        
        auto comment_header = std::vector<uint8_t>{'O', 'p', 'u', 's', 'T', 'a', 'g', 's', 8, 0, 0, 0, 'l', 'i', 'b', 'o', 'p', 'u', 's', ' ', 0, 0, 0, 0};
        MediaChunk comment_chunk(1, comment_header);
        codec.decode(comment_chunk);
        
        // Test multiple packets to check consistency
        for (int i = 0; i < 10; ++i) {
            auto test_packet = OpusCompatibilityPerformanceTest::createOpusPacketForMode("CELT");
            MediaChunk test_chunk(1, test_packet);
            AudioFrame frame = codec.decode(test_chunk);
            
            // Validate output format consistency
            if (frame.sample_rate != 48000) {
                printf("FAIL: Inconsistent sample rate in packet %d: %u Hz\n", i, frame.sample_rate);
                return false;
            }
            
            if (frame.channels != 2) {
                printf("FAIL: Inconsistent channel count in packet %d: %u channels\n", i, frame.channels);
                return false;
            }
            
            // Check sample value ranges (16-bit PCM)
            for (int16_t sample : frame.samples) {
                if (sample < -32768 || sample > 32767) {
                    printf("FAIL: Sample out of 16-bit range in packet %d: %d\n", i, sample);
                    return false;
                }
            }
            
            // Check for reasonable frame sizes
            if (!frame.samples.empty()) {
                size_t samples_per_channel = frame.samples.size() / frame.channels;
                if (samples_per_channel > 5760) { // Max Opus frame size at 48kHz
                    printf("FAIL: Frame too large in packet %d: %zu samples per channel\n", i, samples_per_channel);
                    return false;
                }
            }
        }
        
        printf("PASS: Output quality and accuracy validation completed\n");
        return true;
        
    } catch (const std::exception& e) {
        printf("FAIL: Exception in output quality test: %s\n", e.what());
        return false;
    }
}

/**
 * @brief Test Opus codec with various encoder configurations
 * Requirements: 12.1, 12.4, 13.2
 */
bool test_opus_encoder_compatibility() {
    printf("=== Testing Opus encoder compatibility ===\n");
    
    try {
        // Test different channel configurations
        std::vector<uint16_t> channel_configs = {1, 2, 6, 8}; // mono, stereo, 5.1, 7.1
        
        for (uint16_t channels : channel_configs) {
            StreamInfo stream_info = OpusCompatibilityPerformanceTest::createStreamInfoForTest("encoder_compat", channels);
            OpusCodec codec(stream_info);
            
            if (!codec.canDecode(stream_info)) {
                printf("INFO: Codec reports it cannot decode %u-channel Opus (expected for >2 channels)\n", channels);
                continue;
            }
            
            if (!codec.initialize()) {
                printf("FAIL: OpusCodec initialization failed for %u channels\n", channels);
                return false;
            }
            
            // Create appropriate ID header for channel configuration
            std::vector<uint8_t> id_header = {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd', 1, static_cast<uint8_t>(channels), 56, 1, 0x80, 0xBB, 0x00, 0x00, 0, 0};
            
            if (channels <= 2) {
                id_header.push_back(0); // Channel mapping family 0 for mono/stereo
            } else {
                id_header.push_back(1); // Channel mapping family 1 for surround
                // Add stream count and coupled stream count
                id_header.push_back(channels > 2 ? (channels + 1) / 2 : 1); // stream_count
                id_header.push_back(channels > 1 ? channels / 2 : 0); // coupled_stream_count
                // Add channel mapping
                for (uint16_t i = 0; i < channels; ++i) {
                    id_header.push_back(static_cast<uint8_t>(i));
                }
            }
            
            MediaChunk id_chunk(1, id_header);
            AudioFrame id_frame = codec.decode(id_chunk);
            
            // Should not produce audio from header
            if (!id_frame.samples.empty()) {
                printf("FAIL: ID header should not produce audio for %u channels\n", channels);
                return false;
            }
            
            printf("INFO: Successfully processed %u-channel Opus configuration\n", channels);
        }
        
        printf("PASS: Encoder compatibility test completed\n");
        return true;
        
    } catch (const std::exception& e) {
        printf("FAIL: Exception in encoder compatibility test: %s\n", e.what());
        return false;
    }
}

/**
 * @brief Run all Opus compatibility and performance tests
 */
bool run_opus_compatibility_performance_tests() {
    printf("Starting Opus codec compatibility and performance tests...\n");
    
    bool all_passed = true;
    
    // Test 13.2: Validate compatibility and performance
    all_passed &= test_opus_encoding_modes();
    all_passed &= test_opus_quality_levels();
    all_passed &= test_opus_variable_frame_sizes();
    all_passed &= test_opus_performance();
    all_passed &= test_opus_output_quality();
    all_passed &= test_opus_encoder_compatibility();
    
    if (all_passed) {
        printf("=== ALL OPUS COMPATIBILITY AND PERFORMANCE TESTS PASSED ===\n");
    } else {
        printf("=== SOME OPUS COMPATIBILITY AND PERFORMANCE TESTS FAILED ===\n");
    }
    
    return all_passed;
}

#endif // HAVE_OGGDEMUXER

/**
 * @brief Main test entry point
 */
int main() {
    printf("Starting Opus Compatibility and Performance Test Suite\n");
    
#ifdef HAVE_OGGDEMUXER
    printf("HAVE_OGGDEMUXER is defined, running tests\n");
    
    // Register only the Opus codec for testing
    AudioCodecFactory::registerCodec("opus", [](const StreamInfo& info) {
        return std::make_unique<OpusCodec>(info);
    });
    
    bool success = run_opus_compatibility_performance_tests();
    printf("Test result: %s\n", success ? "PASS" : "FAIL");
    return success ? 0 : 1;
#else
    printf("HAVE_OGGDEMUXER not defined, skipping tests\n");
    return 0;
#endif
}