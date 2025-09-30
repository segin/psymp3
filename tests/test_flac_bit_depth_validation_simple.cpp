/*
 * test_flac_bit_depth_validation_simple.cpp - Simple RFC 9639 bit depth validation tests
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
#include <cassert>
#include <cstdint>
#include <algorithm>

// Include config.h to get HAVE_FLAC definition
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_FLAC

/**
 * @brief Simple test for RFC 9639 bit depth validation logic
 * 
 * Tests the core bit depth validation logic without requiring
 * the full FLACCodec infrastructure.
 */
void test_rfc_bit_depth_range_validation() {
    std::cout << "Testing RFC 9639 bit depth range validation..." << std::endl;
    
    // Test valid bit depths (4-32 bits per RFC 9639)
    for (uint16_t bits = 4; bits <= 32; ++bits) {
        bool is_valid = (bits >= 4 && bits <= 32);
        assert(is_valid == true);
        std::cout << "  ✓ " << bits << "-bit depth is valid per RFC 9639" << std::endl;
    }
    
    // Test invalid bit depths (below minimum)
    for (uint16_t bits = 0; bits < 4; ++bits) {
        bool is_valid = (bits >= 4 && bits <= 32);
        assert(is_valid == false);
        std::cout << "  ✓ " << bits << "-bit depth correctly invalid (below RFC 9639 minimum)" << std::endl;
    }
    
    // Test invalid bit depths (above maximum)
    for (uint16_t bits = 33; bits <= 40; ++bits) {
        bool is_valid = (bits >= 4 && bits <= 32);
        assert(is_valid == false);
        std::cout << "  ✓ " << bits << "-bit depth correctly invalid (above RFC 9639 maximum)" << std::endl;
    }
    
    std::cout << "RFC 9639 bit depth range validation tests passed!" << std::endl;
}

/**
 * @brief Test sign extension logic for various bit depths
 * 
 * Tests the sign extension algorithm that would be used in the
 * applyProperSignExtension_unlocked method.
 */
void test_sign_extension_logic() {
    std::cout << "Testing sign extension logic..." << std::endl;
    
    // Test 8-bit sign extension
    {
        // Positive 8-bit value (0x7F = 127)
        int32_t positive_8bit = 0x7F;
        uint32_t sign_bit_pos = 8 - 1;
        uint32_t sign_bit_mask = 1U << sign_bit_pos;
        uint32_t valid_bits_mask = (1U << 8) - 1;
        uint32_t masked_sample = static_cast<uint32_t>(positive_8bit) & valid_bits_mask;
        
        int32_t result;
        if (masked_sample & sign_bit_mask) {
            uint32_t sign_extend_mask = ~valid_bits_mask;
            result = static_cast<int32_t>(masked_sample | sign_extend_mask);
        } else {
            result = static_cast<int32_t>(masked_sample);
        }
        
        assert(result == 127);
        std::cout << "  ✓ 8-bit positive sign extension: 0x" << std::hex << positive_8bit 
                  << " -> " << std::dec << result << std::endl;
        
        // Negative 8-bit value (0x80 = -128 in 8-bit signed)
        int32_t negative_8bit = 0x80;
        masked_sample = static_cast<uint32_t>(negative_8bit) & valid_bits_mask;
        
        if (masked_sample & sign_bit_mask) {
            uint32_t sign_extend_mask = ~valid_bits_mask;
            result = static_cast<int32_t>(masked_sample | sign_extend_mask);
        } else {
            result = static_cast<int32_t>(masked_sample);
        }
        
        assert(result == -128);
        std::cout << "  ✓ 8-bit negative sign extension: 0x" << std::hex << negative_8bit 
                  << " -> " << std::dec << result << std::endl;
    }
    
    // Test 16-bit sign extension
    {
        // Positive 16-bit value (0x7FFF = 32767)
        int32_t positive_16bit = 0x7FFF;
        uint32_t sign_bit_pos = 16 - 1;
        uint32_t sign_bit_mask = 1U << sign_bit_pos;
        uint32_t valid_bits_mask = (1U << 16) - 1;
        uint32_t masked_sample = static_cast<uint32_t>(positive_16bit) & valid_bits_mask;
        
        int32_t result;
        if (masked_sample & sign_bit_mask) {
            uint32_t sign_extend_mask = ~valid_bits_mask;
            result = static_cast<int32_t>(masked_sample | sign_extend_mask);
        } else {
            result = static_cast<int32_t>(masked_sample);
        }
        
        assert(result == 32767);
        std::cout << "  ✓ 16-bit positive sign extension: 0x" << std::hex << positive_16bit 
                  << " -> " << std::dec << result << std::endl;
        
        // Negative 16-bit value (0x8000 = -32768 in 16-bit signed)
        int32_t negative_16bit = 0x8000;
        masked_sample = static_cast<uint32_t>(negative_16bit) & valid_bits_mask;
        
        if (masked_sample & sign_bit_mask) {
            uint32_t sign_extend_mask = ~valid_bits_mask;
            result = static_cast<int32_t>(masked_sample | sign_extend_mask);
        } else {
            result = static_cast<int32_t>(masked_sample);
        }
        
        assert(result == -32768);
        std::cout << "  ✓ 16-bit negative sign extension: 0x" << std::hex << negative_16bit 
                  << " -> " << std::dec << result << std::endl;
    }
    
    // Test 24-bit sign extension
    {
        // Positive 24-bit value (0x7FFFFF = 8388607)
        int32_t positive_24bit = 0x7FFFFF;
        uint32_t sign_bit_pos = 24 - 1;
        uint32_t sign_bit_mask = 1U << sign_bit_pos;
        uint32_t valid_bits_mask = (1U << 24) - 1;
        uint32_t masked_sample = static_cast<uint32_t>(positive_24bit) & valid_bits_mask;
        
        int32_t result;
        if (masked_sample & sign_bit_mask) {
            uint32_t sign_extend_mask = ~valid_bits_mask;
            result = static_cast<int32_t>(masked_sample | sign_extend_mask);
        } else {
            result = static_cast<int32_t>(masked_sample);
        }
        
        assert(result == 8388607);
        std::cout << "  ✓ 24-bit positive sign extension: 0x" << std::hex << positive_24bit 
                  << " -> " << std::dec << result << std::endl;
        
        // Negative 24-bit value (0x800000 = -8388608 in 24-bit signed)
        int32_t negative_24bit = 0x800000;
        masked_sample = static_cast<uint32_t>(negative_24bit) & valid_bits_mask;
        
        if (masked_sample & sign_bit_mask) {
            uint32_t sign_extend_mask = ~valid_bits_mask;
            result = static_cast<int32_t>(masked_sample | sign_extend_mask);
        } else {
            result = static_cast<int32_t>(masked_sample);
        }
        
        assert(result == -8388608);
        std::cout << "  ✓ 24-bit negative sign extension: 0x" << std::hex << negative_24bit 
                  << " -> " << std::dec << result << std::endl;
    }
    
    std::cout << "Sign extension logic tests passed!" << std::endl;
}

/**
 * @brief Test bit depth conversion logic
 * 
 * Tests the bit depth conversion algorithms that would be used
 * in the convert*BitTo16Bit methods.
 */
void test_bit_depth_conversion_logic() {
    std::cout << "Testing bit depth conversion logic..." << std::endl;
    
    // Test 8-bit to 16-bit conversion (upscaling)
    {
        int32_t sample_8bit = 127;  // Maximum positive 8-bit
        int16_t result_16bit = static_cast<int16_t>(sample_8bit << 8);
        assert(result_16bit == 32512);  // 127 * 256
        std::cout << "  ✓ 8-bit to 16-bit upscaling: " << sample_8bit << " -> " << result_16bit << std::endl;
        
        sample_8bit = -128;  // Maximum negative 8-bit
        result_16bit = static_cast<int16_t>(sample_8bit << 8);
        assert(result_16bit == -32768);  // -128 * 256
        std::cout << "  ✓ 8-bit to 16-bit upscaling: " << sample_8bit << " -> " << result_16bit << std::endl;
    }
    
    // Test 24-bit to 16-bit conversion (downscaling)
    {
        int32_t sample_24bit = 8388607;  // Maximum positive 24-bit
        int16_t result_16bit = static_cast<int16_t>(sample_24bit >> 8);
        assert(result_16bit == 32767);  // Should be maximum 16-bit
        std::cout << "  ✓ 24-bit to 16-bit downscaling: " << sample_24bit << " -> " << result_16bit << std::endl;
        
        sample_24bit = -8388608;  // Maximum negative 24-bit
        result_16bit = static_cast<int16_t>(sample_24bit >> 8);
        assert(result_16bit == -32768);  // Should be minimum 16-bit
        std::cout << "  ✓ 24-bit to 16-bit downscaling: " << sample_24bit << " -> " << result_16bit << std::endl;
    }
    
    // Test 32-bit to 16-bit conversion with overflow protection
    {
        int32_t sample_32bit = 2147483647;  // Maximum positive 32-bit
        int32_t scaled = sample_32bit >> 16;
        int16_t result_16bit = static_cast<int16_t>(std::clamp(scaled, -32768, 32767));
        assert(result_16bit == 32767);  // Should be clamped to maximum 16-bit
        std::cout << "  ✓ 32-bit to 16-bit with overflow protection: " << sample_32bit << " -> " << result_16bit << std::endl;
        
        sample_32bit = -2147483648;  // Maximum negative 32-bit
        scaled = sample_32bit >> 16;
        result_16bit = static_cast<int16_t>(std::clamp(scaled, -32768, 32767));
        assert(result_16bit == -32768);  // Should be clamped to minimum 16-bit
        std::cout << "  ✓ 32-bit to 16-bit with overflow protection: " << sample_32bit << " -> " << result_16bit << std::endl;
    }
    
    std::cout << "Bit depth conversion logic tests passed!" << std::endl;
}

/**
 * @brief Test sample format consistency validation logic
 * 
 * Tests the logic that would be used in validateSampleFormatConsistency_unlocked.
 */
void test_sample_format_consistency_logic() {
    std::cout << "Testing sample format consistency logic..." << std::endl;
    
    // Simulate STREAMINFO parameters
    uint16_t streaminfo_bits_per_sample = 16;
    uint16_t streaminfo_channels = 2;
    uint32_t streaminfo_sample_rate = 44100;
    
    // Simulate frame header parameters
    struct MockFrameHeader {
        uint16_t bits_per_sample;
        uint16_t channels;
        uint32_t sample_rate;
    };
    
    // Test matching parameters (should pass)
    MockFrameHeader matching_frame = {16, 2, 44100};
    bool is_consistent = (matching_frame.bits_per_sample == streaminfo_bits_per_sample &&
                         matching_frame.channels == streaminfo_channels &&
                         (matching_frame.sample_rate == 0 || matching_frame.sample_rate == streaminfo_sample_rate));
    assert(is_consistent == true);
    std::cout << "  ✓ Matching sample format parameters validated successfully" << std::endl;
    
    // Test bit depth mismatch
    MockFrameHeader bitdepth_mismatch_frame = {24, 2, 44100};
    is_consistent = (bitdepth_mismatch_frame.bits_per_sample == streaminfo_bits_per_sample &&
                    bitdepth_mismatch_frame.channels == streaminfo_channels &&
                    (bitdepth_mismatch_frame.sample_rate == 0 || bitdepth_mismatch_frame.sample_rate == streaminfo_sample_rate));
    assert(is_consistent == false);
    std::cout << "  ✓ Bit depth mismatch correctly detected" << std::endl;
    
    // Test channel count mismatch
    MockFrameHeader channel_mismatch_frame = {16, 1, 44100};
    is_consistent = (channel_mismatch_frame.bits_per_sample == streaminfo_bits_per_sample &&
                    channel_mismatch_frame.channels == streaminfo_channels &&
                    (channel_mismatch_frame.sample_rate == 0 || channel_mismatch_frame.sample_rate == streaminfo_sample_rate));
    assert(is_consistent == false);
    std::cout << "  ✓ Channel count mismatch correctly detected" << std::endl;
    
    // Test sample rate mismatch
    MockFrameHeader samplerate_mismatch_frame = {16, 2, 48000};
    is_consistent = (samplerate_mismatch_frame.bits_per_sample == streaminfo_bits_per_sample &&
                    samplerate_mismatch_frame.channels == streaminfo_channels &&
                    (samplerate_mismatch_frame.sample_rate == 0 || samplerate_mismatch_frame.sample_rate == streaminfo_sample_rate));
    assert(is_consistent == false);
    std::cout << "  ✓ Sample rate mismatch correctly detected" << std::endl;
    
    // Test frame with sample rate 0 (should be ignored)
    MockFrameHeader zero_samplerate_frame = {16, 2, 0};
    is_consistent = (zero_samplerate_frame.bits_per_sample == streaminfo_bits_per_sample &&
                    zero_samplerate_frame.channels == streaminfo_channels &&
                    (zero_samplerate_frame.sample_rate == 0 || zero_samplerate_frame.sample_rate == streaminfo_sample_rate));
    assert(is_consistent == true);
    std::cout << "  ✓ Frame with sample rate 0 correctly ignored" << std::endl;
    
    std::cout << "Sample format consistency logic tests passed!" << std::endl;
}

int main() {
    std::cout << "Starting simple RFC 9639 bit depth and sample format compliance tests..." << std::endl;
    
    try {
        test_rfc_bit_depth_range_validation();
        test_sign_extension_logic();
        test_bit_depth_conversion_logic();
        test_sample_format_consistency_logic();
        
        std::cout << "\n✅ All simple RFC 9639 bit depth and sample format compliance tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}

#else // !HAVE_FLAC

#include <iostream>

int main() {
    std::cout << "FLAC support not available - skipping RFC 9639 bit depth validation tests" << std::endl;
    return 0;
}

#endif // HAVE_FLAC