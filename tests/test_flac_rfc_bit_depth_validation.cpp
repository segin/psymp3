/*
 * test_flac_rfc_bit_depth_validation.cpp - RFC 9639 bit depth and sample format compliance tests
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

#ifdef HAVE_FLAC

#include <cassert>
#include <iostream>
#include <vector>
#include <random>
#include <cmath>

using PsyMP3::Codec::FLAC::FLAC__int32;
using PsyMP3::Codec::FLAC::FLAC__Frame;

/**
 * @brief Test RFC 9639 bit depth validation
 * 
 * Tests the validateBitDepthRFC9639_unlocked method to ensure it correctly
 * validates bit depths according to RFC 9639 specification (4-32 bits).
 */
void test_rfc_bit_depth_validation() {
    std::cout << "Testing RFC 9639 bit depth validation..." << std::endl;
    
    // Create a test codec instance
    StreamInfo stream_info;
    stream_info.codec_name = "flac";
    stream_info.sample_rate = 44100;
    stream_info.channels = 2;
    stream_info.bits_per_sample = 16;
    
    FLACCodec codec(stream_info);
    
    // Test valid bit depths (4-32 bits per RFC 9639)
    for (uint16_t bits = 4; bits <= 32; ++bits) {
        bool result = codec.testValidateBitDepthRFC9639(bits);
        assert(result == true);
        std::cout << "  ✓ " << bits << "-bit depth validated as RFC 9639 compliant" << std::endl;
    }
    
    // Test invalid bit depths (below minimum)
    for (uint16_t bits = 0; bits < 4; ++bits) {
        bool result = codec.testValidateBitDepthRFC9639(bits);
        assert(result == false);
        std::cout << "  ✓ " << bits << "-bit depth correctly rejected (below RFC 9639 minimum)" << std::endl;
    }
    
    // Test invalid bit depths (above maximum)
    for (uint16_t bits = 33; bits <= 40; ++bits) {
        bool result = codec.testValidateBitDepthRFC9639(bits);
        assert(result == false);
        std::cout << "  ✓ " << bits << "-bit depth correctly rejected (above RFC 9639 maximum)" << std::endl;
    }
    
    std::cout << "RFC 9639 bit depth validation tests passed!" << std::endl;
}

/**
 * @brief Test sample format consistency validation
 * 
 * Tests the validateSampleFormatConsistency_unlocked method to ensure it
 * correctly validates consistency between STREAMINFO and frame headers.
 */
void test_sample_format_consistency() {
    std::cout << "Testing sample format consistency validation..." << std::endl;
    
    // Create a test codec instance
    StreamInfo stream_info;
    stream_info.codec_name = "flac";
    stream_info.sample_rate = 44100;
    stream_info.channels = 2;
    stream_info.bits_per_sample = 16;
    
    FLACCodec codec(stream_info);
    assert(codec.initialize());
    
    // Create a mock FLAC frame with matching parameters
    FLAC__Frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.header.bits_per_sample = 16;
    frame.header.channels = 2;
    frame.header.sample_rate = 44100;
    frame.header.blocksize = 1024;
    
    // Test matching parameters (should pass)
    bool result = codec.testValidateSampleFormatConsistency(&frame);
    assert(result == true);
    std::cout << "  ✓ Matching sample format parameters validated successfully" << std::endl;
    
    // Test bit depth mismatch
    frame.header.bits_per_sample = 24;
    result = codec.testValidateSampleFormatConsistency(&frame);
    assert(result == false);
    std::cout << "  ✓ Bit depth mismatch correctly detected" << std::endl;
    
    // Reset and test channel count mismatch
    frame.header.bits_per_sample = 16;
    frame.header.channels = 1;
    result = codec.testValidateSampleFormatConsistency(&frame);
    assert(result == false);
    std::cout << "  ✓ Channel count mismatch correctly detected" << std::endl;
    
    // Reset and test sample rate mismatch
    frame.header.channels = 2;
    frame.header.sample_rate = 48000;
    result = codec.testValidateSampleFormatConsistency(&frame);
    assert(result == false);
    std::cout << "  ✓ Sample rate mismatch correctly detected" << std::endl;
    
    std::cout << "Sample format consistency validation tests passed!" << std::endl;
}

/**
 * @brief Test proper sign extension for various bit depths
 * 
 * Tests the applyProperSignExtension_unlocked method to ensure it correctly
 * handles sign extension for samples with bit depths less than 32 bits.
 */
void test_proper_sign_extension() {
    std::cout << "Testing proper sign extension..." << std::endl;
    
    // Create a test codec instance
    StreamInfo stream_info;
    stream_info.codec_name = "flac";
    stream_info.sample_rate = 44100;
    stream_info.channels = 2;
    stream_info.bits_per_sample = 16;
    
    FLACCodec codec(stream_info);
    
    // Test 8-bit sign extension
    {
        // Positive 8-bit value
        FLAC__int32 positive_8bit = 127;
        FLAC__int32 result = codec.testApplyProperSignExtension(positive_8bit, 8);
        assert(result == 127);
        std::cout << "  ✓ 8-bit positive sign extension: " << positive_8bit << " -> " << result << std::endl;
        
        // Negative 8-bit value
        FLAC__int32 negative_8bit = -128;
        FLAC__int32 result_neg = codec.testApplyProperSignExtension(negative_8bit, 8);
        assert(result_neg == -128);
        std::cout << "  ✓ 8-bit negative sign extension: " << negative_8bit << " -> " << result_neg << std::endl;
        
        // Test with raw bit pattern (0x80 = -128 in 8-bit signed)
        FLAC__int32 raw_negative = 0x80;
        FLAC__int32 result_raw = codec.testApplyProperSignExtension(raw_negative, 8);
        assert(result_raw == -128);
        std::cout << "  ✓ 8-bit raw negative pattern: 0x" << std::hex << raw_negative << " -> " << std::dec << result_raw << std::endl;
    }
    
    // Test 16-bit sign extension
    {
        // Positive 16-bit value
        FLAC__int32 positive_16bit = 32767;
        FLAC__int32 result = codec.testApplyProperSignExtension(positive_16bit, 16);
        assert(result == 32767);
        std::cout << "  ✓ 16-bit positive sign extension: " << positive_16bit << " -> " << result << std::endl;
        
        // Negative 16-bit value
        FLAC__int32 negative_16bit = -32768;
        FLAC__int32 result_neg = codec.testApplyProperSignExtension(negative_16bit, 16);
        assert(result_neg == -32768);
        std::cout << "  ✓ 16-bit negative sign extension: " << negative_16bit << " -> " << result_neg << std::endl;
    }
    
    // Test 24-bit sign extension
    {
        // Positive 24-bit value
        FLAC__int32 positive_24bit = 8388607;
        FLAC__int32 result = codec.testApplyProperSignExtension(positive_24bit, 24);
        assert(result == 8388607);
        std::cout << "  ✓ 24-bit positive sign extension: " << positive_24bit << " -> " << result << std::endl;
        
        // Negative 24-bit value
        FLAC__int32 negative_24bit = -8388608;
        FLAC__int32 result_neg = codec.testApplyProperSignExtension(negative_24bit, 24);
        assert(result_neg == -8388608);
        std::cout << "  ✓ 24-bit negative sign extension: " << negative_24bit << " -> " << result_neg << std::endl;
        
        // Test with raw bit pattern (0x800000 = -8388608 in 24-bit signed)
        FLAC__int32 raw_negative = 0x800000;
        FLAC__int32 result_raw = codec.testApplyProperSignExtension(raw_negative, 24);
        assert(result_raw == -8388608);
        std::cout << "  ✓ 24-bit raw negative pattern: 0x" << std::hex << raw_negative << " -> " << std::dec << result_raw << std::endl;
    }
    
    // Test 32-bit (no extension needed)
    {
        FLAC__int32 value_32bit = -2147483648;
        FLAC__int32 result = codec.testApplyProperSignExtension(value_32bit, 32);
        assert(result == value_32bit);
        std::cout << "  ✓ 32-bit no extension needed: " << value_32bit << " -> " << result << std::endl;
    }
    
    std::cout << "Proper sign extension tests passed!" << std::endl;
}

/**
 * @brief Test bit-perfect reconstruction validation
 * 
 * Tests the validateBitPerfectReconstruction_unlocked method to ensure it
 * correctly validates lossless reconstruction for various bit depths.
 */
void test_bit_perfect_reconstruction() {
    std::cout << "Testing bit-perfect reconstruction validation..." << std::endl;
    
    // Create a test codec instance
    StreamInfo stream_info;
    stream_info.codec_name = "flac";
    stream_info.sample_rate = 44100;
    stream_info.channels = 2;
    stream_info.bits_per_sample = 16;
    
    FLACCodec codec(stream_info);
    
    // Test 16-bit bit-perfect reconstruction (should be exact)
    {
        std::vector<FLAC__int32> original_16bit = {-32768, -16384, 0, 16383, 32767};
        std::vector<int16_t> converted_16bit = {-32768, -16384, 0, 16383, 32767};
        
        bool result = codec.testValidateBitPerfectReconstruction(
            original_16bit.data(), converted_16bit.data(), original_16bit.size(), 16);
        assert(result == true);
        std::cout << "  ✓ 16-bit bit-perfect reconstruction validated" << std::endl;
        
        // Test with mismatch
        converted_16bit[2] = 1; // Introduce error
        result = codec.testValidateBitPerfectReconstruction(
            original_16bit.data(), converted_16bit.data(), original_16bit.size(), 16);
        assert(result == false);
        std::cout << "  ✓ 16-bit reconstruction error correctly detected" << std::endl;
    }
    
    // Test 8-bit to 16-bit reconstruction (upscaling)
    {
        std::vector<FLAC__int32> original_8bit = {-128, -64, 0, 63, 127};
        std::vector<int16_t> converted_16bit;
        
        // Convert using the codec's conversion function
        for (FLAC__int32 sample : original_8bit) {
            converted_16bit.push_back(codec.testConvert8BitTo16Bit(sample));
        }
        
        bool result = codec.testValidateBitPerfectReconstruction(
            original_8bit.data(), converted_16bit.data(), original_8bit.size(), 8);
        assert(result == true);
        std::cout << "  ✓ 8-bit to 16-bit reconstruction validated" << std::endl;
    }
    
    // Test 24-bit to 16-bit reconstruction (downscaling)
    {
        std::vector<PsyMP3::Codec::FLAC::FLAC__int32> original_24bit = {-8388608, -4194304, 0, 4194303, 8388607};
        std::vector<int16_t> converted_16bit;
        
        // Convert using the codec's conversion function
        for (PsyMP3::Codec::FLAC::FLAC__int32 sample : original_24bit) {
            converted_16bit.push_back(codec.testConvert24BitTo16Bit(sample));
        }
        
        bool result = codec.testValidateBitPerfectReconstruction(
            original_24bit.data(), converted_16bit.data(), original_24bit.size(), 24);
        assert(result == true);
        std::cout << "  ✓ 24-bit to 16-bit reconstruction validated" << std::endl;
    }
    
    std::cout << "Bit-perfect reconstruction validation tests passed!" << std::endl;
}

/**
 * @brief Test audio quality metrics calculation
 * 
 * Tests the calculateAudioQualityMetrics_unlocked method to ensure it
 * correctly calculates various audio quality metrics.
 */
void test_audio_quality_metrics() {
    std::cout << "Testing audio quality metrics calculation..." << std::endl;
    
    // Create a test codec instance
    StreamInfo stream_info;
    stream_info.codec_name = "flac";
    stream_info.sample_rate = 44100;
    stream_info.channels = 2;
    stream_info.bits_per_sample = 16;
    
    FLACCodec codec(stream_info);
    
    // Test with silence (should have specific characteristics)
    {
        std::vector<int16_t> silence(1000, 0);
        AudioQualityMetrics metrics = codec.testCalculateAudioQualityMetrics(
            silence.data(), silence.size());
        
        assert(metrics.peak_amplitude == 0.0);
        assert(metrics.rms_amplitude == 0.0);
        assert(metrics.zero_crossings == 0);
        assert(metrics.clipped_samples == 0);
        std::cout << "  ✓ Silence metrics calculated correctly" << std::endl;
    }
    
    // Test with full-scale sine wave
    {
        std::vector<int16_t> sine_wave;
        const size_t sample_count = 1000;
        const double frequency = 1000.0; // 1kHz
        const double sample_rate = 44100.0;
        
        for (size_t i = 0; i < sample_count; ++i) {
            double t = static_cast<double>(i) / sample_rate;
            double amplitude = 0.9 * std::sin(2.0 * M_PI * frequency * t);
            sine_wave.push_back(static_cast<int16_t>(amplitude * 32767.0));
        }
        
        AudioQualityMetrics metrics = codec.testCalculateAudioQualityMetrics(
            sine_wave.data(), sine_wave.size());
        
        assert(metrics.peak_amplitude > 0.8 && metrics.peak_amplitude < 1.0);
        assert(metrics.rms_amplitude > 0.6 && metrics.rms_amplitude < 0.7);
        assert(metrics.zero_crossings > 40); // Should have many zero crossings
        assert(metrics.clipped_samples == 0);
        std::cout << "  ✓ Sine wave metrics calculated correctly" << std::endl;
        std::cout << "    Peak: " << metrics.peak_amplitude << ", RMS: " << metrics.rms_amplitude 
                  << ", Zero crossings: " << metrics.zero_crossings << std::endl;
    }
    
    // Test with clipped signal
    {
        std::vector<int16_t> clipped_signal;
        for (int i = 0; i < 100; ++i) {
            clipped_signal.push_back(32767);  // Maximum positive
            clipped_signal.push_back(-32768); // Maximum negative
        }
        
        AudioQualityMetrics metrics = codec.testCalculateAudioQualityMetrics(
            clipped_signal.data(), clipped_signal.size());
        
        assert(metrics.peak_amplitude == 1.0);
        assert(metrics.clipped_samples == clipped_signal.size());
        std::cout << "  ✓ Clipped signal metrics calculated correctly" << std::endl;
        std::cout << "    Clipped samples: " << metrics.clipped_samples << std::endl;
    }
    
    std::cout << "Audio quality metrics calculation tests passed!" << std::endl;
}

/**
 * @brief Test reserved bit depth values handling
 * 
 * Tests the validateReservedBitDepthValues_unlocked method to ensure it
 * correctly handles reserved bit depth values per RFC 9639.
 */
void test_reserved_bit_depth_values() {
    std::cout << "Testing reserved bit depth values handling..." << std::endl;
    
    // Create a test codec instance
    StreamInfo stream_info;
    stream_info.codec_name = "flac";
    stream_info.sample_rate = 44100;
    stream_info.channels = 2;
    stream_info.bits_per_sample = 16;
    
    FLACCodec codec(stream_info);
    
    // Test all valid bit depths (currently no reserved values in RFC 9639)
    for (uint16_t bits = 4; bits <= 32; ++bits) {
        bool result = codec.testValidateReservedBitDepthValues(bits);
        assert(result == true);
        std::cout << "  ✓ " << bits << "-bit depth not reserved" << std::endl;
    }
    
    // Test common bit depths (should be recognized as common)
    std::vector<uint16_t> common_depths = {8, 16, 24, 32};
    for (uint16_t bits : common_depths) {
        bool result = codec.testValidateReservedBitDepthValues(bits);
        assert(result == true);
        std::cout << "  ✓ " << bits << "-bit depth recognized as common" << std::endl;
    }
    
    // Test uncommon but valid bit depths (should still be valid)
    std::vector<uint16_t> uncommon_depths = {4, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15, 17, 18, 19, 20, 21, 22, 23, 25, 26, 27, 28, 29, 30, 31};
    for (uint16_t bits : uncommon_depths) {
        bool result = codec.testValidateReservedBitDepthValues(bits);
        assert(result == true);
        std::cout << "  ✓ " << bits << "-bit depth valid but uncommon" << std::endl;
    }
    
    std::cout << "Reserved bit depth values handling tests passed!" << std::endl;
}

int main() {
    std::cout << "Starting RFC 9639 bit depth and sample format compliance tests..." << std::endl;
    
    try {
        test_rfc_bit_depth_validation();
        test_sample_format_consistency();
        test_proper_sign_extension();
        test_bit_perfect_reconstruction();
        test_audio_quality_metrics();
        test_reserved_bit_depth_values();
        
        std::cout << "\n✅ All RFC 9639 bit depth and sample format compliance tests passed!" << std::endl;
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