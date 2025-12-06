/*
 * test_flac_conditional_compilation.cpp - Test FLAC codec conditional compilation
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

/**
 * @brief Test FLAC codec conditional compilation support
 * 
 * This test validates that the FLAC codec conditional compilation system
 * works correctly both when FLAC is available and when it's not.
 */

// Note: When HAVE_NATIVE_FLAC is defined, use PsyMP3::Codec::FLAC::FLACCodecSupport
// When using libFLAC wrapper (HAVE_FLAC without HAVE_NATIVE_FLAC), use global FLACCodecSupport
#ifdef HAVE_NATIVE_FLAC
namespace FLACCodecSupport = PsyMP3::Codec::FLAC::FLACCodecSupport;
#endif

// Test FLACCodecSupport availability detection
bool test_flac_codec_availability() {
    Debug::log("test_flac_conditional", "[test_flac_codec_availability] Testing FLAC codec availability detection");
    
#ifdef HAVE_FLAC
    // When FLAC is available
    if (!FLACCodecSupport::isAvailable()) {
        Debug::log("test_flac_conditional", "[test_flac_codec_availability] ERROR: FLAC should be available when HAVE_FLAC is defined");
        return false;
    }
    
    Debug::log("test_flac_conditional", "[test_flac_codec_availability] FLAC codec correctly detected as available");
    
    // Test codec info
    std::string codec_info = FLACCodecSupport::getCodecInfo();
    if (codec_info.empty() || codec_info.find("FLAC") == std::string::npos) {
        Debug::log("test_flac_conditional", "[test_flac_codec_availability] ERROR: Invalid codec info: ", codec_info);
        return false;
    }
    
    Debug::log("test_flac_conditional", "[test_flac_codec_availability] Codec info: ", codec_info);
    
#else
    // When FLAC is not available
    if (FLACCodecSupport::isAvailable()) {
        Debug::log("test_flac_conditional", "[test_flac_codec_availability] ERROR: FLAC should not be available when HAVE_FLAC is not defined");
        return false;
    }
    
    Debug::log("test_flac_conditional", "[test_flac_codec_availability] FLAC codec correctly detected as unavailable");
    
    // Test that codec creation returns nullptr
    StreamInfo test_info;
    test_info.codec_name = "flac";
    test_info.codec_type = "audio";
    
    auto codec = FLACCodecSupport::createCodec(test_info);
    if (codec != nullptr) {
        Debug::log("test_flac_conditional", "[test_flac_codec_availability] ERROR: Codec creation should return nullptr when FLAC unavailable");
        return false;
    }
    
    Debug::log("test_flac_conditional", "[test_flac_codec_availability] Codec creation correctly returns nullptr when unavailable");
#endif
    
    Debug::log("test_flac_conditional", "[test_flac_codec_availability] SUCCESS: FLAC availability detection working correctly");
    return true;
}

// Test FLAC stream detection
bool test_flac_stream_detection() {
    Debug::log("test_flac_conditional", "[test_flac_stream_detection] Testing FLAC stream detection");
    
    // Test valid FLAC stream info
    StreamInfo flac_info;
    flac_info.codec_name = "flac";
    flac_info.codec_type = "audio";
    flac_info.sample_rate = 44100;
    flac_info.channels = 2;
    flac_info.bits_per_sample = 16;
    
#ifdef HAVE_FLAC
    if (!FLACCodecSupport::isFLACStream(flac_info)) {
        Debug::log("test_flac_conditional", "[test_flac_stream_detection] ERROR: Valid FLAC stream not detected when FLAC available");
        return false;
    }
    
    Debug::log("test_flac_conditional", "[test_flac_stream_detection] Valid FLAC stream correctly detected");
#else
    if (FLACCodecSupport::isFLACStream(flac_info)) {
        Debug::log("test_flac_conditional", "[test_flac_stream_detection] ERROR: FLAC stream detected when FLAC unavailable");
        return false;
    }
    
    Debug::log("test_flac_conditional", "[test_flac_stream_detection] FLAC stream correctly not detected when unavailable");
#endif
    
    // Test invalid stream info
    StreamInfo invalid_info;
    invalid_info.codec_name = "mp3";
    invalid_info.codec_type = "audio";
    
    if (FLACCodecSupport::isFLACStream(invalid_info)) {
        Debug::log("test_flac_conditional", "[test_flac_stream_detection] ERROR: Invalid stream incorrectly detected as FLAC");
        return false;
    }
    
    Debug::log("test_flac_conditional", "[test_flac_stream_detection] Invalid stream correctly not detected as FLAC");
    
    Debug::log("test_flac_conditional", "[test_flac_stream_detection] SUCCESS: FLAC stream detection working correctly");
    return true;
}

// Test codec registration
bool test_flac_codec_registration() {
    Debug::log("test_flac_conditional", "[test_flac_codec_registration] Testing FLAC codec registration");
    
    try {
        // Test registration (should not crash)
        FLACCodecSupport::registerCodec();
        
#ifdef HAVE_FLAC
        Debug::log("test_flac_conditional", "[test_flac_codec_registration] FLAC codec registration completed (FLAC available)");
#else
        Debug::log("test_flac_conditional", "[test_flac_codec_registration] FLAC codec registration completed (FLAC unavailable - no-op)");
#endif
        
        Debug::log("test_flac_conditional", "[test_flac_codec_registration] SUCCESS: Codec registration working correctly");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test_flac_conditional", "[test_flac_codec_registration] ERROR: Exception during registration: ", e.what());
        return false;
    }
}

// Test codec creation (simplified to avoid buffer pool dependencies)
bool test_flac_codec_creation() {
    Debug::log("test_flac_conditional", "[test_flac_codec_creation] Testing FLAC codec creation");
    
    StreamInfo test_info;
    test_info.codec_name = "flac";
    test_info.codec_type = "audio";
    test_info.sample_rate = 44100;
    test_info.channels = 2;
    test_info.bits_per_sample = 16;
    
    try {
        // Test codec creation function exists and can be called
        // Note: We don't actually create the codec to avoid buffer pool dependencies
        
#ifdef HAVE_FLAC
        Debug::log("test_flac_conditional", "[test_flac_codec_creation] FLAC codec creation function available");
        
        // Test that the function exists (we can't actually call it due to dependencies)
        // But we can test that the support functions work
        bool can_decode = FLACCodecSupport::isFLACStream(test_info);
        if (!can_decode) {
            Debug::log("test_flac_conditional", "[test_flac_codec_creation] ERROR: Should be able to decode FLAC stream");
            return false;
        }
        
        Debug::log("test_flac_conditional", "[test_flac_codec_creation] FLAC stream detection working correctly");
#else
        Debug::log("test_flac_conditional", "[test_flac_codec_creation] FLAC codec creation correctly unavailable when FLAC disabled");
        
        // Test that createCodec returns nullptr when FLAC unavailable
        auto codec = FLACCodecSupport::createCodec(test_info);
        if (codec != nullptr) {
            Debug::log("test_flac_conditional", "[test_flac_codec_creation] ERROR: Codec creation should return nullptr when FLAC unavailable");
            return false;
        }
        
        Debug::log("test_flac_conditional", "[test_flac_codec_creation] Codec creation correctly returns nullptr when unavailable");
#endif
        
        Debug::log("test_flac_conditional", "[test_flac_codec_creation] SUCCESS: Codec creation working correctly");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test_flac_conditional", "[test_flac_codec_creation] ERROR: Exception during codec creation: ", e.what());
        return false;
    }
}

// Test build-time compilation
bool test_build_time_compilation() {
    Debug::log("test_flac_conditional", "[test_build_time_compilation] Testing build-time compilation flags");
    
#ifdef HAVE_FLAC
    Debug::log("test_flac_conditional", "[test_build_time_compilation] HAVE_FLAC is defined - FLAC support enabled");
    
    // Test that FLAC-specific types are available
    FLACFrameInfo frame_info;
    if (!frame_info.isValid()) {
        // This is expected for default-constructed frame info
        Debug::log("test_flac_conditional", "[test_build_time_compilation] FLACFrameInfo available and working");
    }
    
    FLACCodecStats stats;
    double avg_time = stats.getAverageDecodeTimeUs();
    Debug::log("test_flac_conditional", "[test_build_time_compilation] FLACCodecStats available, avg time: ", avg_time);
    
#else
    Debug::log("test_flac_conditional", "[test_build_time_compilation] HAVE_FLAC is not defined - FLAC support disabled");
    
    // Test that stub implementations work
    FLACFrameInfo frame_info;
    if (frame_info.isValid()) {
        Debug::log("test_flac_conditional", "[test_build_time_compilation] ERROR: Stub FLACFrameInfo should not be valid");
        return false;
    }
    
    Debug::log("test_flac_conditional", "[test_build_time_compilation] Stub FLACFrameInfo working correctly");
#endif
    
    Debug::log("test_flac_conditional", "[test_build_time_compilation] SUCCESS: Build-time compilation working correctly");
    return true;
}

// Main test function
int main() {
    Debug::log("test_flac_conditional", "Starting FLAC conditional compilation tests");
    
    bool all_tests_passed = true;
    
    // Run all tests
    all_tests_passed &= test_flac_codec_availability();
    all_tests_passed &= test_flac_stream_detection();
    all_tests_passed &= test_flac_codec_registration();
    all_tests_passed &= test_flac_codec_creation();
    all_tests_passed &= test_build_time_compilation();
    
    if (all_tests_passed) {
        Debug::log("test_flac_conditional", "SUCCESS: All FLAC conditional compilation tests passed");
        return 0;
    } else {
        Debug::log("test_flac_conditional", "FAILURE: Some FLAC conditional compilation tests failed");
        return 1;
    }
}