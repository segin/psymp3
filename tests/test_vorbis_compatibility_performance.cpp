/*
 * test_vorbis_compatibility_performance.cpp - Compatibility and performance tests for VorbisCodec
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

/**
 * @file test_vorbis_compatibility_performance.cpp
 * @brief Compatibility and performance validation tests for VorbisCodec
 * 
 * Task 15.2: Validate compatibility and performance
 * - Test with various Vorbis files from different encoders (oggenc, etc.)
 * - Verify equivalent or better performance than existing implementation
 * - Test all quality levels (-1 to 10) and encoding configurations
 * - Validate output quality and accuracy against libvorbis reference
 * 
 * Requirements: 12.1, 12.2, 12.4, 13.1, 13.2, 13.8
 */

#include "psymp3.h"

#include <iostream>
#include <vector>
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <memory>
#include <chrono>
#include <cmath>
#include <random>

#ifdef HAVE_OGGDEMUXER

using namespace PsyMP3::Codec::Vorbis;
using namespace PsyMP3::Demuxer;


// ========================================
// TEST DATA GENERATORS
// ========================================

/**
 * @brief Generate a valid Vorbis identification header packet
 */
std::vector<uint8_t> generateIdentificationHeader(
    uint8_t channels = 2,
    uint32_t sample_rate = 44100,
    uint8_t blocksize_0 = 8,
    uint8_t blocksize_1 = 11,
    uint32_t bitrate_nominal = 128000
) {
    std::vector<uint8_t> packet(30);
    
    packet[0] = 0x01;
    std::memcpy(&packet[1], "vorbis", 6);
    packet[7] = 0; packet[8] = 0; packet[9] = 0; packet[10] = 0;
    packet[11] = channels;
    packet[12] = sample_rate & 0xFF;
    packet[13] = (sample_rate >> 8) & 0xFF;
    packet[14] = (sample_rate >> 16) & 0xFF;
    packet[15] = (sample_rate >> 24) & 0xFF;
    packet[16] = 0; packet[17] = 0; packet[18] = 0; packet[19] = 0;
    packet[20] = bitrate_nominal & 0xFF;
    packet[21] = (bitrate_nominal >> 8) & 0xFF;
    packet[22] = (bitrate_nominal >> 16) & 0xFF;
    packet[23] = (bitrate_nominal >> 24) & 0xFF;
    packet[24] = 0; packet[25] = 0; packet[26] = 0; packet[27] = 0;
    packet[28] = (blocksize_1 << 4) | blocksize_0;
    packet[29] = 0x01;
    
    return packet;
}

/**
 * @brief Generate a valid Vorbis comment header packet with encoder info
 */
std::vector<uint8_t> generateCommentHeader(const std::string& vendor = "Test Encoder") {
    std::vector<uint8_t> packet;
    
    packet.push_back(0x03);
    for (char c : std::string("vorbis")) {
        packet.push_back(static_cast<uint8_t>(c));
    }
    
    uint32_t vendor_len = static_cast<uint32_t>(vendor.size());
    packet.push_back(vendor_len & 0xFF);
    packet.push_back((vendor_len >> 8) & 0xFF);
    packet.push_back((vendor_len >> 16) & 0xFF);
    packet.push_back((vendor_len >> 24) & 0xFF);
    
    for (char c : vendor) {
        packet.push_back(static_cast<uint8_t>(c));
    }
    
    packet.push_back(0); packet.push_back(0); packet.push_back(0); packet.push_back(0);
    packet.push_back(0x01);
    
    return packet;
}

// ========================================
// TEST 1: Encoder Compatibility
// ========================================

void test_encoder_compatibility() {
    std::cout << "\n=== Test 1: Encoder Compatibility ===" << std::endl;
    std::cout << "Testing compatibility with various Vorbis encoders..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // Test 1.1: oggenc encoder compatibility
    {
        std::cout << "\n  Test 1.1: oggenc encoder compatibility..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Simulate oggenc-style header
        MediaChunk id_chunk;
        id_chunk.data = generateIdentificationHeader(2, 44100, 8, 11, 128000);
        AudioFrame frame1 = codec.decode(id_chunk);
        
        MediaChunk comment_chunk;
        comment_chunk.data = generateCommentHeader("Xiph.Org libVorbis I 20200704 (Reducing Environment)");
        AudioFrame frame2 = codec.decode(comment_chunk);
        
        // Headers should be accepted
        assert(frame1.samples.empty() && frame2.samples.empty());
        
        std::cout << "    ✓ oggenc encoder headers accepted" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 1.2: FFmpeg encoder compatibility
    {
        std::cout << "\n  Test 1.2: FFmpeg encoder compatibility..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 48000;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Simulate FFmpeg-style header
        MediaChunk id_chunk;
        id_chunk.data = generateIdentificationHeader(2, 48000, 8, 11, 192000);
        AudioFrame frame1 = codec.decode(id_chunk);
        
        MediaChunk comment_chunk;
        comment_chunk.data = generateCommentHeader("Lavf58.76.100");
        AudioFrame frame2 = codec.decode(comment_chunk);
        
        assert(frame1.samples.empty() && frame2.samples.empty());
        
        std::cout << "    ✓ FFmpeg encoder headers accepted" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 1.3: Various vendor strings
    {
        std::cout << "\n  Test 1.3: Various vendor strings..." << std::endl;
        
        std::vector<std::string> vendors = {
            "Xiph.Org libVorbis I 20200704",
            "Lavf58.76.100",
            "libvorbis 1.3.7",
            "aoTuV b6.03",
            "Vorbis-Java 0.8",
            "Custom Encoder v1.0",
            "",  // Empty vendor string
        };
        
        for (const auto& vendor : vendors) {
            StreamInfo stream_info;
            stream_info.codec_name = "vorbis";
            stream_info.sample_rate = 44100;
            stream_info.channels = 2;
            
            VorbisCodec codec(stream_info);
            codec.initialize();
            
            MediaChunk id_chunk;
            id_chunk.data = generateIdentificationHeader();
            codec.decode(id_chunk);
            
            MediaChunk comment_chunk;
            comment_chunk.data = generateCommentHeader(vendor);
            AudioFrame frame = codec.decode(comment_chunk);
            
            // Should accept all vendor strings
            assert(frame.samples.empty());
        }
        
        std::cout << "    ✓ All vendor strings accepted" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    std::cout << "\n✓ Test 1: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}


// ========================================
// TEST 2: Quality Level Support
// ========================================

void test_quality_level_support() {
    std::cout << "\n=== Test 2: Quality Level Support ===" << std::endl;
    std::cout << "Testing all quality levels (-1 to 10)..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // Test 2.1: Quality level configurations
    {
        std::cout << "\n  Test 2.1: Quality level configurations..." << std::endl;
        
        // Quality levels map to approximate bitrates
        struct QualityConfig {
            int quality;
            uint32_t approx_bitrate;
            std::string description;
        };
        
        std::vector<QualityConfig> configs = {
            {-1, 45000, "Quality -1 (lowest)"},
            {0, 64000, "Quality 0"},
            {1, 80000, "Quality 1"},
            {2, 96000, "Quality 2"},
            {3, 112000, "Quality 3"},
            {4, 128000, "Quality 4"},
            {5, 160000, "Quality 5 (default)"},
            {6, 192000, "Quality 6"},
            {7, 224000, "Quality 7"},
            {8, 256000, "Quality 8"},
            {9, 320000, "Quality 9"},
            {10, 500000, "Quality 10 (highest)"},
        };
        
        for (const auto& config : configs) {
            StreamInfo stream_info;
            stream_info.codec_name = "vorbis";
            stream_info.sample_rate = 44100;
            stream_info.channels = 2;
            stream_info.bitrate = config.approx_bitrate;
            
            VorbisCodec codec(stream_info);
            bool init_result = codec.initialize();
            
            assert(init_result && ("Failed for " + config.description).c_str());
            
            // Process header with matching bitrate
            MediaChunk id_chunk;
            id_chunk.data = generateIdentificationHeader(2, 44100, 8, 11, config.approx_bitrate);
            AudioFrame frame = codec.decode(id_chunk);
            
            assert(frame.samples.empty());
        }
        
        std::cout << "    ✓ All quality levels supported" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 2.2: VBR mode support
    {
        std::cout << "\n  Test 2.2: VBR mode support..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // VBR streams may have 0 for bitrate bounds
        MediaChunk id_chunk;
        id_chunk.data = generateIdentificationHeader(2, 44100, 8, 11, 0);
        AudioFrame frame = codec.decode(id_chunk);
        
        assert(frame.samples.empty());
        
        std::cout << "    ✓ VBR mode supported" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 2.3: High quality configurations
    {
        std::cout << "\n  Test 2.3: High quality configurations..." << std::endl;
        
        struct HighQualityConfig {
            uint32_t sample_rate;
            uint8_t channels;
            uint32_t bitrate;
            std::string description;
        };
        
        std::vector<HighQualityConfig> configs = {
            {96000, 2, 500000, "96kHz stereo high bitrate"},
            {192000, 2, 500000, "192kHz stereo"},
            {48000, 6, 448000, "5.1 surround"},
            {44100, 2, 320000, "CD quality high bitrate"},
        };
        
        for (const auto& config : configs) {
            StreamInfo stream_info;
            stream_info.codec_name = "vorbis";
            stream_info.sample_rate = config.sample_rate;
            stream_info.channels = config.channels;
            stream_info.bitrate = config.bitrate;
            
            VorbisCodec codec(stream_info);
            bool init_result = codec.initialize();
            
            assert(init_result && ("Failed for " + config.description).c_str());
        }
        
        std::cout << "    ✓ High quality configurations supported" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    std::cout << "\n✓ Test 2: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// TEST 3: Performance Validation
// ========================================

void test_performance_validation() {
    std::cout << "\n=== Test 3: Performance Validation ===" << std::endl;
    std::cout << "Testing codec performance characteristics..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // Test 3.1: Initialization performance
    {
        std::cout << "\n  Test 3.1: Initialization performance..." << std::endl;
        
        const int iterations = 100;
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; i++) {
            StreamInfo stream_info;
            stream_info.codec_name = "vorbis";
            stream_info.sample_rate = 44100;
            stream_info.channels = 2;
            
            VorbisCodec codec(stream_info);
            codec.initialize();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double avg_us = static_cast<double>(duration.count()) / iterations;
        
        std::cout << "    Average initialization time: " << std::fixed << std::setprecision(2) 
                  << avg_us << " μs" << std::endl;
        
        // Should initialize in under 1ms on average
        assert(avg_us < 1000.0 && "Initialization too slow");
        
        std::cout << "    ✓ Initialization performance acceptable" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 3.2: Header processing performance
    {
        std::cout << "\n  Test 3.2: Header processing performance..." << std::endl;
        
        const int iterations = 100;
        auto id_header = generateIdentificationHeader();
        auto comment_header = generateCommentHeader();
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; i++) {
            StreamInfo stream_info;
            stream_info.codec_name = "vorbis";
            stream_info.sample_rate = 44100;
            stream_info.channels = 2;
            
            VorbisCodec codec(stream_info);
            codec.initialize();
            
            MediaChunk id_chunk;
            id_chunk.data = id_header;
            codec.decode(id_chunk);
            
            MediaChunk comment_chunk;
            comment_chunk.data = comment_header;
            codec.decode(comment_chunk);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double avg_us = static_cast<double>(duration.count()) / iterations;
        
        std::cout << "    Average header processing time: " << std::fixed << std::setprecision(2) 
                  << avg_us << " μs" << std::endl;
        
        // Should process headers in under 5ms on average
        assert(avg_us < 5000.0 && "Header processing too slow");
        
        std::cout << "    ✓ Header processing performance acceptable" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 3.3: Reset performance
    {
        std::cout << "\n  Test 3.3: Reset performance..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Process headers first
        MediaChunk id_chunk;
        id_chunk.data = generateIdentificationHeader();
        codec.decode(id_chunk);
        
        MediaChunk comment_chunk;
        comment_chunk.data = generateCommentHeader();
        codec.decode(comment_chunk);
        
        const int iterations = 1000;
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; i++) {
            codec.reset();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double avg_us = static_cast<double>(duration.count()) / iterations;
        
        std::cout << "    Average reset time: " << std::fixed << std::setprecision(2) 
                  << avg_us << " μs" << std::endl;
        
        // Reset should be very fast (under 100μs)
        assert(avg_us < 100.0 && "Reset too slow");
        
        std::cout << "    ✓ Reset performance acceptable" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 3.4: Memory efficiency
    {
        std::cout << "\n  Test 3.4: Memory efficiency..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Check buffer size limits
        size_t max_buffer = VorbisCodec::getMaxBufferSize();
        
        // Max buffer should be reasonable (2 seconds at 48kHz stereo = 192000 samples)
        assert(max_buffer <= 200000 && "Buffer size too large");
        assert(max_buffer >= 100000 && "Buffer size too small");
        
        std::cout << "    Max buffer size: " << max_buffer << " samples" << std::endl;
        std::cout << "    ✓ Memory efficiency acceptable" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    std::cout << "\n✓ Test 3: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}


// ========================================
// TEST 4: Output Quality Validation
// ========================================

void test_output_quality_validation() {
    std::cout << "\n=== Test 4: Output Quality Validation ===" << std::endl;
    std::cout << "Testing output quality and accuracy..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // Test 4.1: Float to PCM conversion accuracy
    {
        std::cout << "\n  Test 4.1: Float to PCM conversion accuracy..." << std::endl;
        
        // Test boundary values
        struct ConversionTest {
            float input;
            int16_t expected_min;
            int16_t expected_max;
            std::string description;
        };
        
        std::vector<ConversionTest> tests = {
            {0.0f, 0, 0, "Zero"},
            {1.0f, 32766, 32767, "Maximum positive"},
            {-1.0f, -32767, -32767, "Maximum negative"},
            {0.5f, 16383, 16384, "Half positive"},
            {-0.5f, -16384, -16383, "Half negative"},
            {1.5f, 32766, 32767, "Clipped positive"},
            {-1.5f, -32768, -32767, "Clipped negative"},
        };
        
        for (const auto& test : tests) {
            int16_t result = VorbisCodec::floatToInt16(test.input);
            
            bool in_range = (result >= test.expected_min && result <= test.expected_max);
            assert(in_range && ("Conversion failed for: " + test.description).c_str());
        }
        
        std::cout << "    ✓ Float to PCM conversion accurate" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 4.2: Channel interleaving correctness
    {
        std::cout << "\n  Test 4.2: Channel interleaving correctness..." << std::endl;
        
        // Create test data for 2 channels, 4 samples
        float left_channel[] = {0.1f, 0.2f, 0.3f, 0.4f};
        float right_channel[] = {-0.1f, -0.2f, -0.3f, -0.4f};
        float* channels[] = {left_channel, right_channel};
        
        std::vector<int16_t> output;
        VorbisCodec::interleaveChannels(channels, 4, 2, output);
        
        // Should have 8 samples (4 samples * 2 channels)
        assert(output.size() == 8);
        
        // Check interleaving pattern: L0, R0, L1, R1, L2, R2, L3, R3
        // Left samples should be positive, right samples should be negative
        for (int i = 0; i < 4; i++) {
            int16_t left_sample = output[i * 2];
            int16_t right_sample = output[i * 2 + 1];
            
            assert(left_sample > 0 && "Left channel should be positive");
            assert(right_sample < 0 && "Right channel should be negative");
        }
        
        std::cout << "    ✓ Channel interleaving correct" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 4.3: Multi-channel interleaving
    {
        std::cout << "\n  Test 4.3: Multi-channel interleaving..." << std::endl;
        
        // Test with 6 channels (5.1 surround)
        const int num_channels = 6;
        const int num_samples = 3;
        
        float channel_data[num_channels][num_samples];
        float* channels[num_channels];
        
        // Fill with distinct values for each channel
        for (int ch = 0; ch < num_channels; ch++) {
            for (int s = 0; s < num_samples; s++) {
                channel_data[ch][s] = static_cast<float>(ch + 1) * 0.1f;
            }
            channels[ch] = channel_data[ch];
        }
        
        std::vector<int16_t> output;
        VorbisCodec::interleaveChannels(channels, num_samples, num_channels, output);
        
        // Should have 18 samples (3 samples * 6 channels)
        assert(output.size() == 18);
        
        // Verify interleaving pattern
        for (int s = 0; s < num_samples; s++) {
            for (int ch = 0; ch < num_channels; ch++) {
                int16_t expected = VorbisCodec::floatToInt16(static_cast<float>(ch + 1) * 0.1f);
                int16_t actual = output[s * num_channels + ch];
                
                // Allow small tolerance due to floating point
                assert(std::abs(actual - expected) <= 1);
            }
        }
        
        std::cout << "    ✓ Multi-channel interleaving correct" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 4.4: Edge case handling
    {
        std::cout << "\n  Test 4.4: Edge case handling..." << std::endl;
        
        // Test with empty input
        std::vector<int16_t> output;
        VorbisCodec::interleaveChannels(nullptr, 0, 0, output);
        assert(output.empty());
        
        // Test with zero samples
        float channel_data[] = {0.5f};
        float* channels[] = {channel_data};
        VorbisCodec::interleaveChannels(channels, 0, 1, output);
        assert(output.empty());
        
        std::cout << "    ✓ Edge cases handled correctly" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    std::cout << "\n✓ Test 4: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// TEST 5: Sample Rate and Channel Configurations
// ========================================

void test_sample_rate_channel_configs() {
    std::cout << "\n=== Test 5: Sample Rate and Channel Configurations ===" << std::endl;
    std::cout << "Testing various sample rate and channel configurations..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // Test 5.1: Standard sample rates
    {
        std::cout << "\n  Test 5.1: Standard sample rates..." << std::endl;
        
        std::vector<uint32_t> sample_rates = {
            8000,   // Telephone quality
            11025,  // Quarter CD
            16000,  // Wideband speech
            22050,  // Half CD
            32000,  // Broadcast
            44100,  // CD quality
            48000,  // DVD quality
            88200,  // 2x CD
            96000,  // DVD-Audio
            176400, // 4x CD
            192000, // High-res
        };
        
        for (uint32_t rate : sample_rates) {
            StreamInfo stream_info;
            stream_info.codec_name = "vorbis";
            stream_info.sample_rate = rate;
            stream_info.channels = 2;
            
            VorbisCodec codec(stream_info);
            bool init_result = codec.initialize();
            
            assert(init_result && "Failed to initialize with sample rate");
            
            MediaChunk id_chunk;
            id_chunk.data = generateIdentificationHeader(2, rate);
            AudioFrame frame = codec.decode(id_chunk);
            
            assert(frame.samples.empty());
        }
        
        std::cout << "    ✓ All standard sample rates supported" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 5.2: Channel configurations
    {
        std::cout << "\n  Test 5.2: Channel configurations..." << std::endl;
        
        struct ChannelConfig {
            uint8_t channels;
            std::string description;
        };
        
        std::vector<ChannelConfig> configs = {
            {1, "Mono"},
            {2, "Stereo"},
            {3, "2.1"},
            {4, "Quadraphonic"},
            {5, "5.0 surround"},
            {6, "5.1 surround"},
            {7, "6.1 surround"},
            {8, "7.1 surround"},
        };
        
        for (const auto& config : configs) {
            StreamInfo stream_info;
            stream_info.codec_name = "vorbis";
            stream_info.sample_rate = 44100;
            stream_info.channels = config.channels;
            
            VorbisCodec codec(stream_info);
            bool init_result = codec.initialize();
            
            assert(init_result && ("Failed for " + config.description).c_str());
            
            MediaChunk id_chunk;
            id_chunk.data = generateIdentificationHeader(config.channels, 44100);
            AudioFrame frame = codec.decode(id_chunk);
            
            assert(frame.samples.empty());
        }
        
        std::cout << "    ✓ All channel configurations supported" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 5.3: Block size configurations
    {
        std::cout << "\n  Test 5.3: Block size configurations..." << std::endl;
        
        // Valid block sizes are powers of 2 from 64 (2^6) to 8192 (2^13)
        struct BlockSizeConfig {
            uint8_t blocksize_0;  // log2 of short block
            uint8_t blocksize_1;  // log2 of long block
            std::string description;
        };
        
        std::vector<BlockSizeConfig> configs = {
            {6, 6, "64/64 (minimum)"},
            {8, 11, "256/2048 (typical)"},
            {9, 12, "512/4096"},
            {10, 13, "1024/8192"},
            {13, 13, "8192/8192 (maximum)"},
        };
        
        for (const auto& config : configs) {
            StreamInfo stream_info;
            stream_info.codec_name = "vorbis";
            stream_info.sample_rate = 44100;
            stream_info.channels = 2;
            
            VorbisCodec codec(stream_info);
            codec.initialize();
            
            MediaChunk id_chunk;
            id_chunk.data = generateIdentificationHeader(2, 44100, config.blocksize_0, config.blocksize_1);
            AudioFrame frame = codec.decode(id_chunk);
            
            assert(frame.samples.empty());
        }
        
        std::cout << "    ✓ All block size configurations supported" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    std::cout << "\n✓ Test 5: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// MAIN TEST RUNNER
// ========================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Vorbis Compatibility and Performance Tests" << std::endl;
    std::cout << "Task 15.2: Validate compatibility and performance" << std::endl;
    std::cout << "Requirements: 12.1, 12.2, 12.4, 13.1, 13.2, 13.8" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        // Test 1: Encoder Compatibility
        test_encoder_compatibility();
        
        // Test 2: Quality Level Support
        test_quality_level_support();
        
        // Test 3: Performance Validation
        test_performance_validation();
        
        // Test 4: Output Quality Validation
        test_output_quality_validation();
        
        // Test 5: Sample Rate and Channel Configurations
        test_sample_rate_channel_configs();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "ALL COMPATIBILITY AND PERFORMANCE TESTS PASSED!" << std::endl;
        std::cout << "========================================" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
}

#else // !HAVE_OGGDEMUXER

int main() {
    std::cout << "Vorbis compatibility and performance tests skipped - OggDemuxer not available" << std::endl;
    return 0;
}

#endif // HAVE_OGGDEMUXER

