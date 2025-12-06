/*
 * test_flac_codec_minimal_container_agnostic.cpp - Minimal container-agnostic FLAC codec test
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

#ifdef HAVE_NATIVE_FLAC

/**
 * @brief Minimal test for container-agnostic FLAC codec operation
 * 
 * This test verifies the basic container-agnostic functionality without
 * requiring complex dependencies.
 */

/**
 * @brief Test that codec initializes from StreamInfo only
 */
bool test_streaminfo_only_initialization() {
    Debug::log("test", "[test_streaminfo_only_initialization] Testing StreamInfo-only initialization");
    
    try {
        // Test 1: Standard FLAC StreamInfo
        StreamInfo flac_info;
        flac_info.codec_type = "audio";
        flac_info.codec_name = "flac";
        flac_info.sample_rate = 44100;
        flac_info.channels = 2;
        flac_info.bits_per_sample = 16;
        flac_info.duration_samples = 1000000;
        
        FLACCodec codec(flac_info);
        
        if (!codec.initialize()) {
            Debug::log("test", "[test_streaminfo_only_initialization] Failed to initialize FLAC codec");
            return false;
        }
        
        if (!codec.canDecode(flac_info)) {
            Debug::log("test", "[test_streaminfo_only_initialization] Codec reports it cannot decode FLAC");
            return false;
        }
        
        if (codec.getCodecName() != "flac") {
            Debug::log("test", "[test_streaminfo_only_initialization] Codec name mismatch");
            return false;
        }
        
        Debug::log("test", "[test_streaminfo_only_initialization] Basic initialization: SUCCESS");
        
        // Test 2: Different audio parameters - codec should work the same
        StreamInfo hires_info;
        hires_info.codec_type = "audio";
        hires_info.codec_name = "flac";
        hires_info.sample_rate = 96000;
        hires_info.channels = 2;
        hires_info.bits_per_sample = 24;
        hires_info.duration_samples = 5000000;
        
        FLACCodec hires_codec(hires_info);
        
        if (!hires_codec.initialize()) {
            Debug::log("test", "[test_streaminfo_only_initialization] Failed to initialize high-res FLAC codec");
            return false;
        }
        
        Debug::log("test", "[test_streaminfo_only_initialization] High-res initialization: SUCCESS");
        
        // Test 3: Verify codec doesn't depend on container information
        StreamInfo minimal_info;
        minimal_info.codec_type = "audio";
        minimal_info.codec_name = "flac";
        minimal_info.sample_rate = 48000;
        minimal_info.channels = 1;
        minimal_info.bits_per_sample = 16;
        // No duration, bitrate, or other optional fields
        
        FLACCodec minimal_codec(minimal_info);
        
        if (!minimal_codec.initialize()) {
            Debug::log("test", "[test_streaminfo_only_initialization] Failed to initialize minimal FLAC codec");
            return false;
        }
        
        Debug::log("test", "[test_streaminfo_only_initialization] Minimal initialization: SUCCESS");
        
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test", "[test_streaminfo_only_initialization] Exception: ", e.what());
        return false;
    }
}

/**
 * @brief Test codec behavior with invalid StreamInfo
 */
bool test_invalid_streaminfo_rejection() {
    Debug::log("test", "[test_invalid_streaminfo_rejection] Testing invalid StreamInfo rejection");
    
    try {
        // Test 1: Invalid sample rate
        StreamInfo invalid_rate;
        invalid_rate.codec_type = "audio";
        invalid_rate.codec_name = "flac";
        invalid_rate.sample_rate = 0; // Invalid
        invalid_rate.channels = 2;
        invalid_rate.bits_per_sample = 16;
        
        FLACCodec codec1(invalid_rate);
        
        if (codec1.initialize()) {
            Debug::log("test", "[test_invalid_streaminfo_rejection] Codec should reject zero sample rate");
            return false;
        }
        
        Debug::log("test", "[test_invalid_streaminfo_rejection] Zero sample rate rejection: SUCCESS");
        
        // Test 2: Invalid channels
        StreamInfo invalid_channels;
        invalid_channels.codec_type = "audio";
        invalid_channels.codec_name = "flac";
        invalid_channels.sample_rate = 44100;
        invalid_channels.channels = 0; // Invalid
        invalid_channels.bits_per_sample = 16;
        
        FLACCodec codec2(invalid_channels);
        
        if (codec2.initialize()) {
            Debug::log("test", "[test_invalid_streaminfo_rejection] Codec should reject zero channels");
            return false;
        }
        
        Debug::log("test", "[test_invalid_streaminfo_rejection] Zero channels rejection: SUCCESS");
        
        // Test 3: Wrong codec name
        StreamInfo wrong_codec;
        wrong_codec.codec_type = "audio";
        wrong_codec.codec_name = "mp3"; // Not FLAC
        wrong_codec.sample_rate = 44100;
        wrong_codec.channels = 2;
        wrong_codec.bits_per_sample = 16;
        
        FLACCodec codec3(wrong_codec);
        
        if (codec3.canDecode(wrong_codec)) {
            Debug::log("test", "[test_invalid_streaminfo_rejection] Codec should not decode MP3");
            return false;
        }
        
        Debug::log("test", "[test_invalid_streaminfo_rejection] Wrong codec rejection: SUCCESS");
        
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test", "[test_invalid_streaminfo_rejection] Exception: ", e.what());
        return false;
    }
}

/**
 * @brief Test codec consistency across different StreamInfo configurations
 */
bool test_container_agnostic_consistency() {
    Debug::log("test", "[test_container_agnostic_consistency] Testing codec consistency");
    
    try {
        // Create multiple StreamInfo objects with same audio parameters
        // but different metadata - codec should behave consistently
        
        std::vector<StreamInfo> test_configs;
        
        // Config 1: Basic FLAC
        StreamInfo basic;
        basic.codec_type = "audio";
        basic.codec_name = "flac";
        basic.sample_rate = 44100;
        basic.channels = 2;
        basic.bits_per_sample = 16;
        test_configs.push_back(basic);
        
        // Config 2: With bitrate
        StreamInfo with_bitrate = basic;
        with_bitrate.bitrate = 1411200;
        test_configs.push_back(with_bitrate);
        
        // Config 3: With metadata
        StreamInfo with_metadata = basic;
        with_metadata.artist = "Test Artist";
        with_metadata.title = "Test Title";
        with_metadata.album = "Test Album";
        test_configs.push_back(with_metadata);
        
        // Config 4: With duration
        StreamInfo with_duration = basic;
        with_duration.duration_samples = 1000000;
        with_duration.duration_ms = 22675; // ~22.7 seconds at 44.1kHz
        test_configs.push_back(with_duration);
        
        // All configurations should initialize successfully
        for (size_t i = 0; i < test_configs.size(); ++i) {
            FLACCodec codec(test_configs[i]);
            
            if (!codec.initialize()) {
                Debug::log("test", "[test_container_agnostic_consistency] Failed to initialize config ", i);
                return false;
            }
            
            if (!codec.canDecode(test_configs[i])) {
                Debug::log("test", "[test_container_agnostic_consistency] Config ", i, " decode capability mismatch");
                return false;
            }
            
            if (codec.getCodecName() != "flac") {
                Debug::log("test", "[test_container_agnostic_consistency] Codec name inconsistency in config ", i);
                return false;
            }
            
            Debug::log("test", "[test_container_agnostic_consistency] Config ", i, " consistency: SUCCESS");
        }
        
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test", "[test_container_agnostic_consistency] Exception: ", e.what());
        return false;
    }
}

/**
 * @brief Main test function
 */
bool test_flac_codec_minimal_container_agnostic() {
    Debug::log("test", "=== FLAC Codec Minimal Container-Agnostic Tests ===");
    
    bool all_passed = true;
    
    // Test 1: StreamInfo-only initialization
    if (!test_streaminfo_only_initialization()) {
        Debug::log("test", "FAILED: StreamInfo-only initialization test");
        all_passed = false;
    }
    
    // Test 2: Invalid StreamInfo rejection
    if (!test_invalid_streaminfo_rejection()) {
        Debug::log("test", "FAILED: Invalid StreamInfo rejection test");
        all_passed = false;
    }
    
    // Test 3: Container-agnostic consistency
    if (!test_container_agnostic_consistency()) {
        Debug::log("test", "FAILED: Container-agnostic consistency test");
        all_passed = false;
    }
    
    if (all_passed) {
        Debug::log("test", "=== ALL MINIMAL CONTAINER-AGNOSTIC TESTS PASSED ===");
    } else {
        Debug::log("test", "=== SOME MINIMAL CONTAINER-AGNOSTIC TESTS FAILED ===");
    }
    
    return all_passed;
}

// Test entry point
int main() {
    try {
        bool success = test_flac_codec_minimal_container_agnostic();
        return success ? 0 : 1;
    } catch (const std::exception& e) {
        Debug::log("test", "Test suite exception: ", e.what());
        return 1;
    }
}

#else // !HAVE_NATIVE_FLAC

int main() {
    Debug::log("test", "Native FLAC codec not available - skipping minimal container-agnostic tests");
    return 0;
}

#endif // HAVE_NATIVE_FLAC
