/*
 * test_vorbis_channel_properties.cpp - Property-based tests for Vorbis channel handling
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

#include <iostream>
#include <vector>
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <algorithm>
#include <random>
#include <cstring>
#include <cmath>

#ifdef HAVE_OGGDEMUXER

using namespace PsyMP3::Codec::Vorbis;
using namespace PsyMP3::Demuxer;

// ========================================
// TEST DATA GENERATORS
// ========================================

/**
 * @brief Generate random float samples in the valid Vorbis range [-1.0, 1.0]
 */
std::vector<float> generateRandomFloatSamples(size_t count, std::mt19937& gen) {
    std::vector<float> samples(count);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    
    for (size_t i = 0; i < count; i++) {
        samples[i] = dist(gen);
    }
    
    return samples;
}

/**
 * @brief Generate float samples with known patterns for verification
 */
std::vector<float> generatePatternedFloatSamples(size_t count, int channel) {
    std::vector<float> samples(count);
    
    // Generate a pattern that encodes the channel number
    // This allows us to verify channel ordering in the output
    for (size_t i = 0; i < count; i++) {
        // Create a unique value based on channel and sample index
        // Normalize to [-1.0, 1.0] range
        float base = static_cast<float>(channel) / 10.0f;  // 0.0, 0.1, 0.2, etc.
        float offset = static_cast<float>(i % 100) / 1000.0f;  // Small variation
        samples[i] = std::clamp(base + offset, -1.0f, 1.0f);
    }
    
    return samples;
}

/**
 * @brief Generate float samples that include edge cases
 */
std::vector<float> generateEdgeCaseFloatSamples(size_t count, std::mt19937& gen) {
    std::vector<float> samples(count);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    
    for (size_t i = 0; i < count; i++) {
        switch (i % 10) {
            case 0: samples[i] = 0.0f; break;           // Zero
            case 1: samples[i] = 1.0f; break;           // Max positive
            case 2: samples[i] = -1.0f; break;          // Max negative
            case 3: samples[i] = 0.5f; break;           // Mid positive
            case 4: samples[i] = -0.5f; break;          // Mid negative
            case 5: samples[i] = 1.0001f; break;        // Slightly over max (should clamp)
            case 6: samples[i] = -1.0001f; break;       // Slightly under min (should clamp)
            case 7: samples[i] = 0.999999f; break;      // Near max
            case 8: samples[i] = -0.999999f; break;     // Near min
            default: samples[i] = dist(gen); break;     // Random
        }
    }
    
    return samples;
}

/**
 * @brief Create a multi-channel float array (simulating libvorbis output)
 */
class MultiChannelFloatBuffer {
public:
    MultiChannelFloatBuffer(int channels, int samples_per_channel)
        : m_channels(channels), m_samples_per_channel(samples_per_channel) {
        m_channel_data.resize(channels);
        m_channel_ptrs.resize(channels);
        
        for (int ch = 0; ch < channels; ch++) {
            m_channel_data[ch].resize(samples_per_channel);
            m_channel_ptrs[ch] = m_channel_data[ch].data();
        }
    }
    
    void setChannelData(int channel, const std::vector<float>& data) {
        if (channel >= 0 && channel < m_channels) {
            size_t copy_size = std::min(data.size(), static_cast<size_t>(m_samples_per_channel));
            std::copy(data.begin(), data.begin() + copy_size, m_channel_data[channel].begin());
        }
    }
    
    float** getChannelPointers() {
        return m_channel_ptrs.data();
    }
    
    int getChannels() const { return m_channels; }
    int getSamplesPerChannel() const { return m_samples_per_channel; }
    
    float getSample(int channel, int sample_index) const {
        if (channel >= 0 && channel < m_channels && 
            sample_index >= 0 && sample_index < m_samples_per_channel) {
            return m_channel_data[channel][sample_index];
        }
        return 0.0f;
    }
    
private:
    int m_channels;
    int m_samples_per_channel;
    std::vector<std::vector<float>> m_channel_data;
    std::vector<float*> m_channel_ptrs;
};

// ========================================
// PROPERTY 8: Channel Count Consistency
// ========================================
// **Feature: vorbis-codec, Property 8: Channel Count Consistency**
// **Validates: Requirements 5.1, 5.2, 5.3, 5.5**
//
// For any valid Vorbis stream with N channels, all decoded AudioFrames 
// shall contain samples with exactly N interleaved channels.

void test_property_channel_count_consistency() {
    std::cout << "\n=== Property 8: Channel Count Consistency ===" << std::endl;
    std::cout << "Testing that output sample count is always samples * channels..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // Test 1: Mono channel consistency
    {
        std::cout << "\n  Test 1: Mono (1 channel) consistency..." << std::endl;
        
        const int channels = 1;
        std::uniform_int_distribution<int> samples_dist(1, 8192);
        
        for (int iteration = 0; iteration < 100; iteration++) {
            int samples_per_channel = samples_dist(gen);
            
            MultiChannelFloatBuffer buffer(channels, samples_per_channel);
            buffer.setChannelData(0, generateRandomFloatSamples(samples_per_channel, gen));
            
            std::vector<int16_t> output;
            VorbisCodec::interleaveChannels(buffer.getChannelPointers(), 
                                            samples_per_channel, channels, output);
            
            // Property: output size must equal samples * channels
            size_t expected_size = static_cast<size_t>(samples_per_channel) * channels;
            assert(output.size() == expected_size && 
                   "Mono output size must equal samples * 1");
        }
        
        std::cout << "    ✓ 100 mono iterations passed" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 2: Stereo channel consistency
    {
        std::cout << "\n  Test 2: Stereo (2 channels) consistency..." << std::endl;
        
        const int channels = 2;
        std::uniform_int_distribution<int> samples_dist(1, 8192);
        
        for (int iteration = 0; iteration < 100; iteration++) {
            int samples_per_channel = samples_dist(gen);
            
            MultiChannelFloatBuffer buffer(channels, samples_per_channel);
            for (int ch = 0; ch < channels; ch++) {
                buffer.setChannelData(ch, generateRandomFloatSamples(samples_per_channel, gen));
            }
            
            std::vector<int16_t> output;
            VorbisCodec::interleaveChannels(buffer.getChannelPointers(), 
                                            samples_per_channel, channels, output);
            
            // Property: output size must equal samples * channels
            size_t expected_size = static_cast<size_t>(samples_per_channel) * channels;
            assert(output.size() == expected_size && 
                   "Stereo output size must equal samples * 2");
        }
        
        std::cout << "    ✓ 100 stereo iterations passed" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 3: Multi-channel consistency (3-8 channels)
    {
        std::cout << "\n  Test 3: Multi-channel (3-8 channels) consistency..." << std::endl;
        
        std::uniform_int_distribution<int> channels_dist(3, 8);
        std::uniform_int_distribution<int> samples_dist(1, 4096);
        
        for (int iteration = 0; iteration < 100; iteration++) {
            int channels = channels_dist(gen);
            int samples_per_channel = samples_dist(gen);
            
            MultiChannelFloatBuffer buffer(channels, samples_per_channel);
            for (int ch = 0; ch < channels; ch++) {
                buffer.setChannelData(ch, generateRandomFloatSamples(samples_per_channel, gen));
            }
            
            std::vector<int16_t> output;
            VorbisCodec::interleaveChannels(buffer.getChannelPointers(), 
                                            samples_per_channel, channels, output);
            
            // Property: output size must equal samples * channels
            size_t expected_size = static_cast<size_t>(samples_per_channel) * channels;
            assert(output.size() == expected_size && 
                   "Multi-channel output size must equal samples * channels");
        }
        
        std::cout << "    ✓ 100 multi-channel iterations passed" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 4: Edge case - single sample
    {
        std::cout << "\n  Test 4: Single sample per channel..." << std::endl;
        
        for (int channels = 1; channels <= 8; channels++) {
            MultiChannelFloatBuffer buffer(channels, 1);
            for (int ch = 0; ch < channels; ch++) {
                std::vector<float> single_sample = {0.5f};
                buffer.setChannelData(ch, single_sample);
            }
            
            std::vector<int16_t> output;
            VorbisCodec::interleaveChannels(buffer.getChannelPointers(), 1, channels, output);
            
            assert(output.size() == static_cast<size_t>(channels) && 
                   "Single sample output must have exactly 'channels' samples");
        }
        
        std::cout << "    ✓ Single sample edge case passed for 1-8 channels" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 5: Edge case - maximum Vorbis block size (8192)
    {
        std::cout << "\n  Test 5: Maximum block size (8192 samples)..." << std::endl;
        
        const int max_block_size = 8192;
        
        for (int channels = 1; channels <= 6; channels++) {
            MultiChannelFloatBuffer buffer(channels, max_block_size);
            for (int ch = 0; ch < channels; ch++) {
                buffer.setChannelData(ch, generateRandomFloatSamples(max_block_size, gen));
            }
            
            std::vector<int16_t> output;
            VorbisCodec::interleaveChannels(buffer.getChannelPointers(), 
                                            max_block_size, channels, output);
            
            size_t expected_size = static_cast<size_t>(max_block_size) * channels;
            assert(output.size() == expected_size && 
                   "Max block size output must equal 8192 * channels");
        }
        
        std::cout << "    ✓ Maximum block size passed for 1-6 channels" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 6: Property test with random channel counts and sample counts
    {
        std::cout << "\n  Test 6: Random channel/sample combinations (100 iterations)..." << std::endl;
        
        std::uniform_int_distribution<int> channels_dist(1, 8);
        std::uniform_int_distribution<int> samples_dist(1, 8192);
        
        for (int iteration = 0; iteration < 100; iteration++) {
            int channels = channels_dist(gen);
            int samples_per_channel = samples_dist(gen);
            
            MultiChannelFloatBuffer buffer(channels, samples_per_channel);
            for (int ch = 0; ch < channels; ch++) {
                buffer.setChannelData(ch, generateRandomFloatSamples(samples_per_channel, gen));
            }
            
            std::vector<int16_t> output;
            VorbisCodec::interleaveChannels(buffer.getChannelPointers(), 
                                            samples_per_channel, channels, output);
            
            // Property: output.size() == samples_per_channel * channels
            size_t expected_size = static_cast<size_t>(samples_per_channel) * channels;
            if (output.size() != expected_size) {
                std::cerr << "FAILED: channels=" << channels 
                          << " samples=" << samples_per_channel
                          << " expected=" << expected_size 
                          << " got=" << output.size() << std::endl;
                assert(false && "Channel count consistency violated");
            }
        }
        
        std::cout << "    ✓ 100 random combinations passed" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    std::cout << "\n✓ Property 8: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}


// ========================================
// PROPERTY 9: Channel Interleaving Correctness
// ========================================
// **Feature: vorbis-codec, Property 9: Channel Interleaving Correctness**
// **Validates: Requirements 5.5, 5.7**
//
// For any multi-channel Vorbis stream, the output samples shall be correctly 
// interleaved in Vorbis channel order (L, R for stereo; FL, FC, FR, RL, RR, LFE for 5.1, etc.).

void test_property_channel_interleaving_correctness() {
    std::cout << "\n=== Property 9: Channel Interleaving Correctness ===" << std::endl;
    std::cout << "Testing that channels are correctly interleaved in output..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // Test 1: Stereo interleaving verification
    {
        std::cout << "\n  Test 1: Stereo interleaving (L, R pattern)..." << std::endl;
        
        const int channels = 2;
        const int samples_per_channel = 100;
        
        MultiChannelFloatBuffer buffer(channels, samples_per_channel);
        
        // Set left channel to all 0.5, right channel to all -0.5
        std::vector<float> left_data(samples_per_channel, 0.5f);
        std::vector<float> right_data(samples_per_channel, -0.5f);
        buffer.setChannelData(0, left_data);
        buffer.setChannelData(1, right_data);
        
        std::vector<int16_t> output;
        VorbisCodec::interleaveChannels(buffer.getChannelPointers(), 
                                        samples_per_channel, channels, output);
        
        // Verify interleaving: [L0, R0, L1, R1, ...]
        int16_t expected_left = VorbisCodec::floatToInt16(0.5f);
        int16_t expected_right = VorbisCodec::floatToInt16(-0.5f);
        
        for (int i = 0; i < samples_per_channel; i++) {
            int16_t left_sample = output[i * 2];
            int16_t right_sample = output[i * 2 + 1];
            
            assert(left_sample == expected_left && "Left channel sample incorrect");
            assert(right_sample == expected_right && "Right channel sample incorrect");
        }
        
        std::cout << "    ✓ Stereo interleaving verified" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 2: Multi-channel interleaving with unique channel values
    {
        std::cout << "\n  Test 2: Multi-channel interleaving with unique values..." << std::endl;
        
        for (int channels = 1; channels <= 8; channels++) {
            const int samples_per_channel = 50;
            
            MultiChannelFloatBuffer buffer(channels, samples_per_channel);
            
            // Set each channel to a unique value based on channel index
            for (int ch = 0; ch < channels; ch++) {
                float channel_value = static_cast<float>(ch + 1) / 10.0f;  // 0.1, 0.2, 0.3, etc.
                std::vector<float> channel_data(samples_per_channel, channel_value);
                buffer.setChannelData(ch, channel_data);
            }
            
            std::vector<int16_t> output;
            VorbisCodec::interleaveChannels(buffer.getChannelPointers(), 
                                            samples_per_channel, channels, output);
            
            // Verify interleaving: for each sample position, channels should be in order
            for (int sample_idx = 0; sample_idx < samples_per_channel; sample_idx++) {
                for (int ch = 0; ch < channels; ch++) {
                    float expected_float = static_cast<float>(ch + 1) / 10.0f;
                    int16_t expected_int16 = VorbisCodec::floatToInt16(expected_float);
                    int16_t actual = output[sample_idx * channels + ch];
                    
                    if (actual != expected_int16) {
                        std::cerr << "FAILED at sample " << sample_idx << " channel " << ch
                                  << ": expected " << expected_int16 << " got " << actual << std::endl;
                        assert(false && "Channel interleaving incorrect");
                    }
                }
            }
        }
        
        std::cout << "    ✓ Multi-channel interleaving verified for 1-8 channels" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 3: Property test - random data preserves channel identity
    {
        std::cout << "\n  Test 3: Random data preserves channel identity (100 iterations)..." << std::endl;
        
        std::uniform_int_distribution<int> channels_dist(2, 8);
        std::uniform_int_distribution<int> samples_dist(10, 1000);
        
        for (int iteration = 0; iteration < 100; iteration++) {
            int channels = channels_dist(gen);
            int samples_per_channel = samples_dist(gen);
            
            MultiChannelFloatBuffer buffer(channels, samples_per_channel);
            
            // Generate unique random data for each channel
            std::vector<std::vector<float>> channel_data(channels);
            for (int ch = 0; ch < channels; ch++) {
                channel_data[ch] = generateRandomFloatSamples(samples_per_channel, gen);
                buffer.setChannelData(ch, channel_data[ch]);
            }
            
            std::vector<int16_t> output;
            VorbisCodec::interleaveChannels(buffer.getChannelPointers(), 
                                            samples_per_channel, channels, output);
            
            // Verify: each output sample matches the expected channel's input
            for (int sample_idx = 0; sample_idx < samples_per_channel; sample_idx++) {
                for (int ch = 0; ch < channels; ch++) {
                    int16_t expected = VorbisCodec::floatToInt16(channel_data[ch][sample_idx]);
                    int16_t actual = output[sample_idx * channels + ch];
                    
                    if (actual != expected) {
                        std::cerr << "FAILED iteration " << iteration 
                                  << " sample " << sample_idx << " channel " << ch
                                  << ": expected " << expected << " got " << actual << std::endl;
                        assert(false && "Channel identity not preserved");
                    }
                }
            }
        }
        
        std::cout << "    ✓ 100 random iterations passed" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 4: Verify sample ordering within interleaved output
    {
        std::cout << "\n  Test 4: Sample ordering verification..." << std::endl;
        
        const int channels = 4;
        const int samples_per_channel = 10;
        
        MultiChannelFloatBuffer buffer(channels, samples_per_channel);
        
        // Set each sample to encode its position: channel * 0.1 + sample_index * 0.001
        for (int ch = 0; ch < channels; ch++) {
            std::vector<float> data(samples_per_channel);
            for (int i = 0; i < samples_per_channel; i++) {
                data[i] = static_cast<float>(ch) * 0.1f + static_cast<float>(i) * 0.001f;
            }
            buffer.setChannelData(ch, data);
        }
        
        std::vector<int16_t> output;
        VorbisCodec::interleaveChannels(buffer.getChannelPointers(), 
                                        samples_per_channel, channels, output);
        
        // Verify the interleaved pattern
        for (int sample_idx = 0; sample_idx < samples_per_channel; sample_idx++) {
            for (int ch = 0; ch < channels; ch++) {
                float expected_float = static_cast<float>(ch) * 0.1f + 
                                       static_cast<float>(sample_idx) * 0.001f;
                int16_t expected = VorbisCodec::floatToInt16(expected_float);
                int16_t actual = output[sample_idx * channels + ch];
                
                assert(actual == expected && "Sample ordering incorrect");
            }
        }
        
        std::cout << "    ✓ Sample ordering verified" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 5: Edge case - alternating positive/negative samples
    {
        std::cout << "\n  Test 5: Alternating positive/negative samples..." << std::endl;
        
        const int channels = 2;
        const int samples_per_channel = 100;
        
        MultiChannelFloatBuffer buffer(channels, samples_per_channel);
        
        // Left channel: alternating 1.0, -1.0
        // Right channel: alternating -1.0, 1.0
        std::vector<float> left_data(samples_per_channel);
        std::vector<float> right_data(samples_per_channel);
        for (int i = 0; i < samples_per_channel; i++) {
            left_data[i] = (i % 2 == 0) ? 1.0f : -1.0f;
            right_data[i] = (i % 2 == 0) ? -1.0f : 1.0f;
        }
        buffer.setChannelData(0, left_data);
        buffer.setChannelData(1, right_data);
        
        std::vector<int16_t> output;
        VorbisCodec::interleaveChannels(buffer.getChannelPointers(), 
                                        samples_per_channel, channels, output);
        
        int16_t pos_max = VorbisCodec::floatToInt16(1.0f);
        int16_t neg_max = VorbisCodec::floatToInt16(-1.0f);
        
        for (int i = 0; i < samples_per_channel; i++) {
            int16_t left = output[i * 2];
            int16_t right = output[i * 2 + 1];
            
            int16_t expected_left = (i % 2 == 0) ? pos_max : neg_max;
            int16_t expected_right = (i % 2 == 0) ? neg_max : pos_max;
            
            assert(left == expected_left && "Left alternating pattern incorrect");
            assert(right == expected_right && "Right alternating pattern incorrect");
        }
        
        std::cout << "    ✓ Alternating pattern verified" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 6: Float to int16 conversion accuracy
    {
        std::cout << "\n  Test 6: Float to int16 conversion accuracy..." << std::endl;
        
        // Test specific float values and their expected int16 conversions
        struct ConversionTest {
            float input;
            int16_t expected_min;  // Allow small tolerance
            int16_t expected_max;
        };
        
        std::vector<ConversionTest> tests = {
            {0.0f, 0, 0},
            {1.0f, 32766, 32767},
            {-1.0f, -32767, -32766},
            {0.5f, 16383, 16384},
            {-0.5f, -16384, -16383},
            {1.5f, 32766, 32767},    // Should clamp to max
            {-1.5f, -32767, -32766}, // Should clamp to min
        };
        
        for (const auto& test : tests) {
            int16_t result = VorbisCodec::floatToInt16(test.input);
            
            if (result < test.expected_min || result > test.expected_max) {
                std::cerr << "FAILED: floatToInt16(" << test.input << ") = " << result
                          << ", expected [" << test.expected_min << ", " << test.expected_max << "]" 
                          << std::endl;
                assert(false && "Float to int16 conversion out of expected range");
            }
        }
        
        std::cout << "    ✓ Float to int16 conversion accuracy verified" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    std::cout << "\n✓ Property 9: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// MAIN TEST RUNNER
// ========================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Vorbis Channel Property Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        // Property 8: Channel Count Consistency
        // **Validates: Requirements 5.1, 5.2, 5.3, 5.5**
        test_property_channel_count_consistency();
        
        // Property 9: Channel Interleaving Correctness
        // **Validates: Requirements 5.5, 5.7**
        test_property_channel_interleaving_correctness();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "ALL PROPERTY TESTS PASSED!" << std::endl;
        std::cout << "========================================" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
}

#else // !HAVE_OGGDEMUXER

int main() {
    std::cout << "Vorbis channel property tests skipped - OggDemuxer not available" << std::endl;
    return 0;
}

#endif // HAVE_OGGDEMUXER

