/*
 * test_flac_codec_streaminfo_only.cpp - StreamInfo-only initialization tests
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

/**
 * @brief Test suite for StreamInfo-only codec initialization
 * 
 * This test suite verifies that the FLACCodec initializes correctly using
 * only StreamInfo parameters without requiring any demuxer-specific information,
 * addressing requirements 5.4, 5.5, 5.6, 5.7.
 */

/**
 * @brief Test basic StreamInfo initialization with valid parameters
 */
bool test_valid_streaminfo_initialization() {
    Debug::log("test", "[test_valid_streaminfo_initialization] Testing codec initialization with valid StreamInfo");
    
    try {
        // Test 1: Standard CD quality FLAC
        {
            StreamInfo cd_quality;
            cd_quality.codec_type = "audio";
            cd_quality.codec_name = "flac";
            cd_quality.sample_rate = 44100;
            cd_quality.channels = 2;
            cd_quality.bits_per_sample = 16;
            cd_quality.duration_samples = 1000000;
            cd_quality.bitrate = 1411200;
            
            FLACCodec codec(cd_quality);
            
            if (!codec.initialize()) {
                Debug::log("test", "[test_valid_streaminfo_initialization] Failed to initialize CD quality codec");
                return false;
            }
            
            if (!codec.canDecode(cd_quality)) {
                Debug::log("test", "[test_valid_streaminfo_initialization] Codec reports cannot decode CD quality");
                return false;
            }
            
            Debug::log("test", "[test_valid_streaminfo_initialization] CD quality initialization: SUCCESS");
        }
        
        // Test 2: High resolution FLAC
        {
            StreamInfo hires;
            hires.codec_type = "audio";
            hires.codec_name = "flac";
            hires.sample_rate = 96000;
            hires.channels = 2;
            hires.bits_per_sample = 24;
            hires.duration_samples = 5000000;
            hires.bitrate = 4608000;
            
            FLACCodec codec(hires);
            
            if (!codec.initialize()) {
                Debug::log("test", "[test_valid_streaminfo_initialization] Failed to initialize high-res codec");
                return false;
            }
            
            Debug::log("test", "[test_valid_streaminfo_initialization] High-res initialization: SUCCESS");
        }
        
        // Test 3: Mono FLAC
        {
            StreamInfo mono;
            mono.codec_type = "audio";
            mono.codec_name = "flac";
            mono.sample_rate = 48000;
            mono.channels = 1;
            mono.bits_per_sample = 16;
            mono.duration_samples = 2000000;
            
            FLACCodec codec(mono);
            
            if (!codec.initialize()) {
                Debug::log("test", "[test_valid_streaminfo_initialization] Failed to initialize mono codec");
                return false;
            }
            
            Debug::log("test", "[test_valid_streaminfo_initialization] Mono initialization: SUCCESS");
        }
        
        // Test 4: 8-bit FLAC (edge case)
        {
            StreamInfo low_bit;
            low_bit.codec_type = "audio";
            low_bit.codec_name = "flac";
            low_bit.sample_rate = 22050;
            low_bit.channels = 1;
            low_bit.bits_per_sample = 8;
            low_bit.duration_samples = 500000;
            
            FLACCodec codec(low_bit);
            
            if (!codec.initialize()) {
                Debug::log("test", "[test_valid_streaminfo_initialization] Failed to initialize 8-bit codec");
                return false;
            }
            
            Debug::log("test", "[test_valid_streaminfo_initialization] 8-bit initialization: SUCCESS");
        }
        
        // Test 5: 32-bit FLAC (high precision)
        {
            StreamInfo high_bit;
            high_bit.codec_type = "audio";
            high_bit.codec_name = "flac";
            high_bit.sample_rate = 192000;
            high_bit.channels = 2;
            high_bit.bits_per_sample = 32;
            high_bit.duration_samples = 10000000;
            
            FLACCodec codec(high_bit);
            
            if (!codec.initialize()) {
                Debug::log("test", "[test_valid_streaminfo_initialization] Failed to initialize 32-bit codec");
                return false;
            }
            
            Debug::log("test", "[test_valid_streaminfo_initialization] 32-bit initialization: SUCCESS");
        }
        
        Debug::log("test", "[test_valid_streaminfo_initialization] All valid StreamInfo tests passed");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test", "[test_valid_streaminfo_initialization] Exception: ", e.what());
        return false;
    }
}

/**
 * @brief Test StreamInfo initialization with edge case parameters
 */
bool test_edge_case_streaminfo() {
    Debug::log("test", "[test_edge_case_streaminfo] Testing codec initialization with edge case parameters");
    
    try {
        // Test 1: Minimum valid sample rate (RFC 9639: 1 Hz minimum)
        {
            StreamInfo min_rate;
            min_rate.codec_type = "audio";
            min_rate.codec_name = "flac";
            min_rate.sample_rate = 1;
            min_rate.channels = 1;
            min_rate.bits_per_sample = 16;
            min_rate.duration_samples = 100;
            
            FLACCodec codec(min_rate);
            
            if (!codec.initialize()) {
                Debug::log("test", "[test_edge_case_streaminfo] Failed to initialize minimum sample rate codec");
                return false;
            }
            
            Debug::log("test", "[test_edge_case_streaminfo] Minimum sample rate initialization: SUCCESS");
        }
        
        // Test 2: Maximum valid sample rate (RFC 9639: 655350 Hz maximum)
        {
            StreamInfo max_rate;
            max_rate.codec_type = "audio";
            max_rate.codec_name = "flac";
            max_rate.sample_rate = 655350;
            max_rate.channels = 1;
            max_rate.bits_per_sample = 16;
            max_rate.duration_samples = 1000000;
            
            FLACCodec codec(max_rate);
            
            if (!codec.initialize()) {
                Debug::log("test", "[test_edge_case_streaminfo] Failed to initialize maximum sample rate codec");
                return false;
            }
            
            Debug::log("test", "[test_edge_case_streaminfo] Maximum sample rate initialization: SUCCESS");
        }
        
        // Test 3: Maximum channels (RFC 9639: 8 channels maximum)
        {
            StreamInfo max_channels;
            max_channels.codec_type = "audio";
            max_channels.codec_name = "flac";
            max_channels.sample_rate = 48000;
            max_channels.channels = 8;
            max_channels.bits_per_sample = 16;
            max_channels.duration_samples = 1000000;
            
            FLACCodec codec(max_channels);
            
            if (!codec.initialize()) {
                Debug::log("test", "[test_edge_case_streaminfo] Failed to initialize 8-channel codec");
                return false;
            }
            
            Debug::log("test", "[test_edge_case_streaminfo] 8-channel initialization: SUCCESS");
        }
        
        // Test 4: Minimum bit depth (RFC 9639: 4 bits minimum)
        {
            StreamInfo min_bits;
            min_bits.codec_type = "audio";
            min_bits.codec_name = "flac";
            min_bits.sample_rate = 44100;
            min_bits.channels = 2;
            min_bits.bits_per_sample = 4;
            min_bits.duration_samples = 1000000;
            
            FLACCodec codec(min_bits);
            
            if (!codec.initialize()) {
                Debug::log("test", "[test_edge_case_streaminfo] Failed to initialize 4-bit codec");
                return false;
            }
            
            Debug::log("test", "[test_edge_case_streaminfo] 4-bit initialization: SUCCESS");
        }
        
        // Test 5: Zero duration (unknown length stream)
        {
            StreamInfo unknown_duration;
            unknown_duration.codec_type = "audio";
            unknown_duration.codec_name = "flac";
            unknown_duration.sample_rate = 44100;
            unknown_duration.channels = 2;
            unknown_duration.bits_per_sample = 16;
            unknown_duration.duration_samples = 0; // Unknown duration
            
            FLACCodec codec(unknown_duration);
            
            if (!codec.initialize()) {
                Debug::log("test", "[test_edge_case_streaminfo] Failed to initialize unknown duration codec");
                return false;
            }
            
            Debug::log("test", "[test_edge_case_streaminfo] Unknown duration initialization: SUCCESS");
        }
        
        Debug::log("test", "[test_edge_case_streaminfo] All edge case StreamInfo tests passed");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test", "[test_edge_case_streaminfo] Exception: ", e.what());
        return false;
    }
}

/**
 * @brief Test StreamInfo initialization with invalid parameters
 */
bool test_invalid_streaminfo_handling() {
    Debug::log("test", "[test_invalid_streaminfo_handling] Testing codec rejection of invalid StreamInfo");
    
    try {
        // Test 1: Invalid sample rate (0)
        {
            StreamInfo invalid_rate;
            invalid_rate.codec_type = "audio";
            invalid_rate.codec_name = "flac";
            invalid_rate.sample_rate = 0; // Invalid
            invalid_rate.channels = 2;
            invalid_rate.bits_per_sample = 16;
            
            FLACCodec codec(invalid_rate);
            
            if (codec.initialize()) {
                Debug::log("test", "[test_invalid_streaminfo_handling] Codec should reject zero sample rate");
                return false;
            }
            
            Debug::log("test", "[test_invalid_streaminfo_handling] Zero sample rate rejection: SUCCESS");
        }
        
        // Test 2: Invalid sample rate (too high)
        {
            StreamInfo too_high_rate;
            too_high_rate.codec_type = "audio";
            too_high_rate.codec_name = "flac";
            too_high_rate.sample_rate = 1000000; // Above RFC 9639 limit
            too_high_rate.channels = 2;
            too_high_rate.bits_per_sample = 16;
            
            FLACCodec codec(too_high_rate);
            
            if (codec.initialize()) {
                Debug::log("test", "[test_invalid_streaminfo_handling] Codec should reject excessive sample rate");
                return false;
            }
            
            Debug::log("test", "[test_invalid_streaminfo_handling] Excessive sample rate rejection: SUCCESS");
        }
        
        // Test 3: Invalid channel count (0)
        {
            StreamInfo invalid_channels;
            invalid_channels.codec_type = "audio";
            invalid_channels.codec_name = "flac";
            invalid_channels.sample_rate = 44100;
            invalid_channels.channels = 0; // Invalid
            invalid_channels.bits_per_sample = 16;
            
            FLACCodec codec(invalid_channels);
            
            if (codec.initialize()) {
                Debug::log("test", "[test_invalid_streaminfo_handling] Codec should reject zero channels");
                return false;
            }
            
            Debug::log("test", "[test_invalid_streaminfo_handling] Zero channels rejection: SUCCESS");
        }
        
        // Test 4: Invalid channel count (too many)
        {
            StreamInfo too_many_channels;
            too_many_channels.codec_type = "audio";
            too_many_channels.codec_name = "flac";
            too_many_channels.sample_rate = 44100;
            too_many_channels.channels = 16; // Above RFC 9639 limit
            too_many_channels.bits_per_sample = 16;
            
            FLACCodec codec(too_many_channels);
            
            if (codec.initialize()) {
                Debug::log("test", "[test_invalid_streaminfo_handling] Codec should reject excessive channels");
                return false;
            }
            
            Debug::log("test", "[test_invalid_streaminfo_handling] Excessive channels rejection: SUCCESS");
        }
        
        // Test 5: Invalid bit depth (0)
        {
            StreamInfo invalid_bits;
            invalid_bits.codec_type = "audio";
            invalid_bits.codec_name = "flac";
            invalid_bits.sample_rate = 44100;
            invalid_bits.channels = 2;
            invalid_bits.bits_per_sample = 0; // Invalid
            
            FLACCodec codec(invalid_bits);
            
            if (codec.initialize()) {
                Debug::log("test", "[test_invalid_streaminfo_handling] Codec should reject zero bit depth");
                return false;
            }
            
            Debug::log("test", "[test_invalid_streaminfo_handling] Zero bit depth rejection: SUCCESS");
        }
        
        // Test 6: Invalid bit depth (too low)
        {
            StreamInfo too_low_bits;
            too_low_bits.codec_type = "audio";
            too_low_bits.codec_name = "flac";
            too_low_bits.sample_rate = 44100;
            too_low_bits.channels = 2;
            too_low_bits.bits_per_sample = 3; // Below RFC 9639 minimum
            
            FLACCodec codec(too_low_bits);
            
            if (codec.initialize()) {
                Debug::log("test", "[test_invalid_streaminfo_handling] Codec should reject insufficient bit depth");
                return false;
            }
            
            Debug::log("test", "[test_invalid_streaminfo_handling] Insufficient bit depth rejection: SUCCESS");
        }
        
        // Test 7: Invalid bit depth (too high)
        {
            StreamInfo too_high_bits;
            too_high_bits.codec_type = "audio";
            too_high_bits.codec_name = "flac";
            too_high_bits.sample_rate = 44100;
            too_high_bits.channels = 2;
            too_high_bits.bits_per_sample = 64; // Above RFC 9639 maximum
            
            FLACCodec codec(too_high_bits);
            
            if (codec.initialize()) {
                Debug::log("test", "[test_invalid_streaminfo_handling] Codec should reject excessive bit depth");
                return false;
            }
            
            Debug::log("test", "[test_invalid_streaminfo_handling] Excessive bit depth rejection: SUCCESS");
        }
        
        // Test 8: Wrong codec name
        {
            StreamInfo wrong_codec;
            wrong_codec.codec_type = "audio";
            wrong_codec.codec_name = "mp3"; // Not FLAC
            wrong_codec.sample_rate = 44100;
            wrong_codec.channels = 2;
            wrong_codec.bits_per_sample = 16;
            
            FLACCodec codec(wrong_codec);
            
            if (codec.canDecode(wrong_codec)) {
                Debug::log("test", "[test_invalid_streaminfo_handling] Codec should not claim to decode MP3");
                return false;
            }
            
            Debug::log("test", "[test_invalid_streaminfo_handling] Wrong codec rejection: SUCCESS");
        }
        
        Debug::log("test", "[test_invalid_streaminfo_handling] All invalid StreamInfo rejection tests passed");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test", "[test_invalid_streaminfo_handling] Exception: ", e.what());
        return false;
    }
}

/**
 * @brief Test that codec doesn't require demuxer-specific information
 */
bool test_no_demuxer_dependencies() {
    Debug::log("test", "[test_no_demuxer_dependencies] Testing codec independence from demuxer information");
    
    try {
        // Create StreamInfo with minimal required information
        StreamInfo minimal_info;
        minimal_info.codec_type = "audio";
        minimal_info.codec_name = "flac";
        minimal_info.sample_rate = 44100;
        minimal_info.channels = 2;
        minimal_info.bits_per_sample = 16;
        // Note: No container_format, bitrate, duration, or other optional fields
        
        FLACCodec codec(minimal_info);
        
        if (!codec.initialize()) {
            Debug::log("test", "[test_no_demuxer_dependencies] Failed to initialize with minimal StreamInfo");
            return false;
        }
        
        if (!codec.canDecode(minimal_info)) {
            Debug::log("test", "[test_no_demuxer_dependencies] Codec reports cannot decode minimal StreamInfo");
            return false;
        }
        
        // Test that codec works without optional StreamInfo fields
        StreamInfo incomplete_info;
        incomplete_info.codec_type = "audio";
        incomplete_info.codec_name = "flac";
        incomplete_info.sample_rate = 48000;
        incomplete_info.channels = 1;
        incomplete_info.bits_per_sample = 24;
        // Missing: duration_samples, bitrate, container_format, etc.
        
        FLACCodec incomplete_codec(incomplete_info);
        
        if (!incomplete_codec.initialize()) {
            Debug::log("test", "[test_no_demuxer_dependencies] Failed to initialize with incomplete StreamInfo");
            return false;
        }
        
        Debug::log("test", "[test_no_demuxer_dependencies] Codec independence from demuxer information: SUCCESS");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test", "[test_no_demuxer_dependencies] Exception: ", e.what());
        return false;
    }
}

/**
 * @brief Test codec behavior with missing or incomplete StreamInfo
 */
bool test_incomplete_streaminfo_handling() {
    Debug::log("test", "[test_incomplete_streaminfo_handling] Testing codec handling of incomplete StreamInfo");
    
    try {
        // Test 1: Missing codec name
        {
            StreamInfo no_codec_name;
            no_codec_name.codec_type = "audio";
            // codec_name is empty/default
            no_codec_name.sample_rate = 44100;
            no_codec_name.channels = 2;
            no_codec_name.bits_per_sample = 16;
            
            FLACCodec codec(no_codec_name);
            
            if (codec.canDecode(no_codec_name)) {
                Debug::log("test", "[test_incomplete_streaminfo_handling] Codec should not decode without codec name");
                return false;
            }
            
            Debug::log("test", "[test_incomplete_streaminfo_handling] Missing codec name handling: SUCCESS");
        }
        
        // Test 2: StreamInfo with only codec name
        {
            StreamInfo only_codec_name;
            only_codec_name.codec_type = "audio";
            only_codec_name.codec_name = "flac";
            // All other fields are default/zero
            
            FLACCodec codec(only_codec_name);
            
            if (codec.initialize()) {
                Debug::log("test", "[test_incomplete_streaminfo_handling] Codec should not initialize with only codec name");
                return false;
            }
            
            Debug::log("test", "[test_incomplete_streaminfo_handling] Incomplete StreamInfo rejection: SUCCESS");
        }
        
        // Test 3: Partially complete StreamInfo
        {
            StreamInfo partial_info;
            partial_info.codec_type = "audio";
            partial_info.codec_name = "flac";
            partial_info.sample_rate = 44100;
            // Missing channels and bits_per_sample
            
            FLACCodec codec(partial_info);
            
            if (codec.initialize()) {
                Debug::log("test", "[test_incomplete_streaminfo_handling] Codec should not initialize with partial info");
                return false;
            }
            
            Debug::log("test", "[test_incomplete_streaminfo_handling] Partial StreamInfo rejection: SUCCESS");
        }
        
        Debug::log("test", "[test_incomplete_streaminfo_handling] All incomplete StreamInfo tests passed");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test", "[test_incomplete_streaminfo_handling] Exception: ", e.what());
        return false;
    }
}

/**
 * @brief Test codec configuration consistency across different StreamInfo variations
 */
bool test_streaminfo_configuration_consistency() {
    Debug::log("test", "[test_streaminfo_configuration_consistency] Testing configuration consistency");
    
    try {
        // Create multiple StreamInfo objects with same audio parameters but different metadata
        StreamInfo base_config;
        base_config.codec_type = "audio";
        base_config.codec_name = "flac";
        base_config.sample_rate = 44100;
        base_config.channels = 2;
        base_config.bits_per_sample = 16;
        base_config.duration_samples = 1000000;
        
        std::vector<StreamInfo> variations;
        
        // Variation 1: With bitrate
        StreamInfo with_bitrate = base_config;
        with_bitrate.bitrate = 1411200;
        variations.push_back(with_bitrate);
        
        // Variation 2: With additional metadata
        StreamInfo with_metadata = base_config;
        with_metadata.artist = "Test Artist";
        with_metadata.title = "Test Title";
        variations.push_back(with_metadata);
        
        // Variation 3: With both bitrate and metadata
        StreamInfo with_both = base_config;
        with_both.bitrate = 1411200;
        with_both.album = "Test Album";
        variations.push_back(with_both);
        
        // Variation 4: Minimal (just audio parameters)
        variations.push_back(base_config);
        
        // All variations should initialize successfully and behave consistently
        for (size_t i = 0; i < variations.size(); ++i) {
            FLACCodec codec(variations[i]);
            
            if (!codec.initialize()) {
                Debug::log("test", "[test_streaminfo_configuration_consistency] Failed to initialize variation ", i);
                return false;
            }
            
            if (!codec.canDecode(variations[i])) {
                Debug::log("test", "[test_streaminfo_configuration_consistency] Variation ", i, " decode capability mismatch");
                return false;
            }
            
            // Check that codec name is consistent
            if (codec.getCodecName() != "flac") {
                Debug::log("test", "[test_streaminfo_configuration_consistency] Codec name inconsistency in variation ", i);
                return false;
            }
            
            Debug::log("test", "[test_streaminfo_configuration_consistency] Variation ", i, " configuration: SUCCESS");
        }
        
        Debug::log("test", "[test_streaminfo_configuration_consistency] All configuration consistency tests passed");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test", "[test_streaminfo_configuration_consistency] Exception: ", e.what());
        return false;
    }
}

/**
 * @brief Main test function for StreamInfo-only initialization
 */
bool test_flac_codec_streaminfo_only() {
    Debug::log("test", "=== FLAC Codec StreamInfo-Only Initialization Tests ===");
    
    bool all_passed = true;
    
    // Test 1: Valid StreamInfo initialization
    if (!test_valid_streaminfo_initialization()) {
        Debug::log("test", "FAILED: Valid StreamInfo initialization test");
        all_passed = false;
    }
    
    // Test 2: Edge case parameters
    if (!test_edge_case_streaminfo()) {
        Debug::log("test", "FAILED: Edge case StreamInfo test");
        all_passed = false;
    }
    
    // Test 3: Invalid parameter handling
    if (!test_invalid_streaminfo_handling()) {
        Debug::log("test", "FAILED: Invalid StreamInfo handling test");
        all_passed = false;
    }
    
    // Test 4: No demuxer dependencies
    if (!test_no_demuxer_dependencies()) {
        Debug::log("test", "FAILED: Demuxer independence test");
        all_passed = false;
    }
    
    // Test 5: Incomplete StreamInfo handling
    if (!test_incomplete_streaminfo_handling()) {
        Debug::log("test", "FAILED: Incomplete StreamInfo handling test");
        all_passed = false;
    }
    
    // Test 6: Configuration consistency
    if (!test_streaminfo_configuration_consistency()) {
        Debug::log("test", "FAILED: Configuration consistency test");
        all_passed = false;
    }
    
    if (all_passed) {
        Debug::log("test", "=== ALL STREAMINFO-ONLY TESTS PASSED ===");
    } else {
        Debug::log("test", "=== SOME STREAMINFO-ONLY TESTS FAILED ===");
    }
    
    return all_passed;
}

// Test entry point
int main() {
    try {
        bool success = test_flac_codec_streaminfo_only();
        return success ? 0 : 1;
    } catch (const std::exception& e) {
        Debug::log("test", "Test suite exception: ", e.what());
        return 1;
    }
}

#else // !HAVE_FLAC

int main() {
    Debug::log("test", "FLAC codec not available - skipping StreamInfo-only tests");
    return 0;
}

#endif // HAVE_FLAC