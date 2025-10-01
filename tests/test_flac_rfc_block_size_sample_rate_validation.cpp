/*
 * test_flac_rfc_block_size_sample_rate_validation.cpp - Test RFC 9639 block size and sample rate decoding
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

#ifdef HAVE_FLAC

#include <cassert>
#include <iostream>
#include <vector>
#include <cstring>

/**
 * @brief Test RFC 9639 block size and sample rate validation
 * 
 * This test validates that the FLAC codec correctly implements RFC 9639
 * block size and sample rate decoding functionality. Since the decoding
 * methods are private, we test the overall functionality by verifying
 * that the codec can be initialized and that the validation methods
 * work correctly.
 */
void test_rfc_block_size_sample_rate_validation() {
    std::cout << "Testing RFC 9639 block size and sample rate validation..." << std::endl;
    
    // Test 1: Basic codec initialization with various configurations
    std::cout << "  Testing codec initialization with various configurations..." << std::endl;
    
    struct TestConfig {
        uint32_t sample_rate;
        uint16_t channels;
        uint16_t bits_per_sample;
        bool should_succeed;
        const char* description;
    };
    
    std::vector<TestConfig> configs = {
        // Valid configurations
        {44100, 2, 16, true, "Standard CD quality"},
        {48000, 2, 24, true, "Standard studio quality"},
        {96000, 2, 24, true, "High resolution"},
        {192000, 2, 32, true, "Ultra high resolution"},
        {8000, 1, 16, true, "Low quality mono"},
        {22050, 2, 8, true, "Low quality stereo"},
        {655350, 8, 32, true, "Maximum RFC 9639 limits"},
        
        // Invalid configurations (should fail)
        {0, 2, 16, false, "Zero sample rate"},
        {655351, 2, 16, false, "Sample rate above RFC 9639 limit"},
        {44100, 0, 16, false, "Zero channels"},
        {44100, 9, 16, false, "Too many channels"},
        {44100, 2, 3, false, "Bit depth below RFC 9639 minimum"},
        {44100, 2, 33, false, "Bit depth above RFC 9639 maximum"}
    };
    
    for (const auto& config : configs) {
        StreamInfo stream_info;
        stream_info.codec_name = "flac";
        stream_info.sample_rate = config.sample_rate;
        stream_info.channels = config.channels;
        stream_info.bits_per_sample = config.bits_per_sample;
        
        try {
            FLACCodec codec(stream_info);
            bool initialized = codec.initialize();
            
            if (config.should_succeed && !initialized) {
                std::cout << "    FAIL: " << config.description 
                         << " should have succeeded but failed" << std::endl;
                assert(false);
            } else if (!config.should_succeed && initialized) {
                std::cout << "    FAIL: " << config.description 
                         << " should have failed but succeeded" << std::endl;
                assert(false);
            } else {
                std::cout << "    PASS: " << config.description << std::endl;
            }
        } catch (const std::exception& e) {
            if (config.should_succeed) {
                std::cout << "    FAIL: " << config.description 
                         << " threw exception: " << e.what() << std::endl;
                assert(false);
            } else {
                std::cout << "    PASS: " << config.description 
                         << " correctly threw exception" << std::endl;
            }
        }
    }
    
    // Test 2: Verify codec name and capabilities
    std::cout << "  Testing codec identification..." << std::endl;
    
    StreamInfo stream_info;
    stream_info.codec_name = "flac";
    stream_info.sample_rate = 44100;
    stream_info.channels = 2;
    stream_info.bits_per_sample = 16;
    
    FLACCodec codec(stream_info);
    assert(codec.getCodecName() == "flac");
    assert(codec.supportsSeekReset() == true);
    
    // Test canDecode with various stream types
    StreamInfo flac_stream;
    flac_stream.codec_name = "flac";
    flac_stream.sample_rate = 44100;
    flac_stream.channels = 2;
    flac_stream.bits_per_sample = 16;
    assert(codec.canDecode(flac_stream) == true);
    
    StreamInfo mp3_stream;
    mp3_stream.codec_name = "mp3";
    mp3_stream.sample_rate = 44100;
    mp3_stream.channels = 2;
    mp3_stream.bits_per_sample = 16;
    assert(codec.canDecode(mp3_stream) == false);
    
    std::cout << "    PASS: Codec identification works correctly" << std::endl;
    
    // Test 3: Verify initialization and basic functionality
    std::cout << "  Testing codec initialization and basic functionality..." << std::endl;
    
    assert(codec.initialize() == true);
    assert(codec.getCurrentSample() == 0);
    
    // Test reset functionality
    codec.reset();
    assert(codec.getCurrentSample() == 0);
    
    // Test flush (should return empty frame when no data processed)
    AudioFrame flush_frame = codec.flush();
    assert(flush_frame.getSampleFrameCount() == 0);
    
    std::cout << "    PASS: Basic codec functionality works correctly" << std::endl;
    
    // Test 4: Test with edge case configurations
    std::cout << "  Testing edge case configurations..." << std::endl;
    
    // Test minimum valid configuration
    StreamInfo min_config;
    min_config.codec_name = "flac";
    min_config.sample_rate = 1;
    min_config.channels = 1;
    min_config.bits_per_sample = 4;
    
    FLACCodec min_codec(min_config);
    assert(min_codec.initialize() == true);
    std::cout << "    PASS: Minimum RFC 9639 configuration works" << std::endl;
    
    // Test maximum valid configuration
    StreamInfo max_config;
    max_config.codec_name = "flac";
    max_config.sample_rate = 655350;
    max_config.channels = 8;
    max_config.bits_per_sample = 32;
    
    FLACCodec max_codec(max_config);
    assert(max_codec.initialize() == true);
    std::cout << "    PASS: Maximum RFC 9639 configuration works" << std::endl;
    
    std::cout << "All RFC 9639 block size and sample rate validation tests passed!" << std::endl;
}

int main() {
    try {
        test_rfc_block_size_sample_rate_validation();
        std::cout << "SUCCESS: All tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cout << "FAILURE: Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "FAILURE: Test failed with unknown exception" << std::endl;
        return 1;
    }
}

#else // !HAVE_FLAC

int main() {
    std::cout << "FLAC support not available - skipping RFC 9639 block size and sample rate tests" << std::endl;
    return 0;
}

#endif // HAVE_FLAC