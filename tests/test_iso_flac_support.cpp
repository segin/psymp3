/*
 * test_iso_flac_support.cpp - Test FLAC-in-MP4 codec support
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include <cassert>
#include <iostream>
#include <vector>

// Test FLAC codec constant definition
void test_flac_codec_constant() {
    std::cout << "Testing FLAC codec constant definition..." << std::endl;
    
    // Verify FLAC codec constant is defined correctly
    uint32_t expected_flac = FOURCC('f','L','a','C');
    assert(CODEC_FLAC == expected_flac);
    
    std::cout << "✓ FLAC codec constant (fLaC) defined correctly" << std::endl;
}

// Test FLAC configuration validation
void test_flac_configuration_validation() {
    std::cout << "Testing FLAC configuration validation..." << std::endl;
    
    // Create a mock IOHandler for testing
    auto mockHandler = std::make_unique<FileIOHandler>("test_file.mp4");
    ISODemuxer demuxer(std::move(mockHandler));
    
    // Test valid FLAC configuration
    AudioTrackInfo validTrack;
    validTrack.codecType = "flac";
    validTrack.sampleRate = 44100;
    validTrack.channelCount = 2;
    validTrack.bitsPerSample = 16;
    validTrack.timescale = 44100;
    validTrack.duration = 1000;
    
    // Create minimal FLAC metadata (STREAMINFO block)
    std::vector<uint8_t> streamInfo = {
        0x00, 0x00, 0x00, 0x22, // Block header: type=0, length=34
        0x04, 0x00, 0x04, 0x00, // Min/max block size: 1024
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Min/max frame size: 0 (unknown)
        0xAC, 0x44, 0x10,       // Sample rate: 44100 Hz, channels: 2, bits: 16
        0x00, 0x00, 0x00, 0x00, 0x00, // Total samples: 0 (unknown)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // MD5 signature
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    validTrack.codecConfig = streamInfo;
    
    // Test validation - this should pass
    bool isValid = demuxer.ValidateFLACCodecConfiguration(validTrack);
    assert(isValid);
    std::cout << "✓ Valid FLAC configuration passes validation" << std::endl;
    
    // Test invalid sample rate
    AudioTrackInfo invalidTrack = validTrack;
    invalidTrack.sampleRate = 0;
    isValid = demuxer.ValidateFLACCodecConfiguration(invalidTrack);
    assert(!isValid);
    std::cout << "✓ Invalid sample rate fails validation" << std::endl;
    
    // Test invalid channel count
    invalidTrack = validTrack;
    invalidTrack.channelCount = 0;
    isValid = demuxer.ValidateFLACCodecConfiguration(invalidTrack);
    assert(!isValid);
    std::cout << "✓ Invalid channel count fails validation" << std::endl;
    
    // Test invalid bit depth
    invalidTrack = validTrack;
    invalidTrack.bitsPerSample = 2; // Below minimum of 4
    isValid = demuxer.ValidateFLACCodecConfiguration(invalidTrack);
    assert(!isValid);
    std::cout << "✓ Invalid bit depth fails validation" << std::endl;
    
    // Test missing codec configuration
    invalidTrack = validTrack;
    invalidTrack.codecConfig.clear();
    isValid = demuxer.ValidateFLACCodecConfiguration(invalidTrack);
    assert(!isValid);
    std::cout << "✓ Missing codec configuration fails validation" << std::endl;
}

// Test FLAC frame boundary detection
void test_flac_frame_boundary_detection() {
    std::cout << "Testing FLAC frame boundary detection..." << std::endl;
    
    auto mockHandler = std::make_unique<FileIOHandler>("test_file.mp4");
    ISODemuxer demuxer(std::move(mockHandler));
    
    // Create sample data with FLAC frame sync pattern
    std::vector<uint8_t> sampleData = {
        0xFF, 0xF8, 0x00, 0x00, // Valid FLAC frame header (sync + reserved bits)
        0x01, 0x02, 0x03, 0x04, // Frame data
        0xFF, 0xF9, 0x00, 0x00, // Another valid FLAC frame header
        0x05, 0x06, 0x07, 0x08  // More frame data
    };
    
    std::vector<size_t> frameOffsets;
    bool detected = demuxer.DetectFLACFrameBoundaries(sampleData, frameOffsets);
    
    assert(detected);
    assert(frameOffsets.size() >= 1); // Should detect at least one frame
    assert(frameOffsets[0] == 0);     // First frame should start at offset 0
    
    std::cout << "✓ FLAC frame boundaries detected correctly" << std::endl;
    
    // Test with invalid sync pattern
    std::vector<uint8_t> invalidData = {
        0x00, 0x00, 0x00, 0x00, // Invalid sync pattern
        0x01, 0x02, 0x03, 0x04
    };
    
    frameOffsets.clear();
    detected = demuxer.DetectFLACFrameBoundaries(invalidData, frameOffsets);
    assert(!detected || frameOffsets.empty());
    std::cout << "✓ Invalid sync pattern correctly rejected" << std::endl;
}

// Test FLAC frame header validation
void test_flac_frame_header_validation() {
    std::cout << "Testing FLAC frame header validation..." << std::endl;
    
    auto mockHandler = std::make_unique<FileIOHandler>("test_file.mp4");
    ISODemuxer demuxer(std::move(mockHandler));
    
    // Valid FLAC frame header
    std::vector<uint8_t> validHeader = {
        0xFF, 0xF8, // Sync pattern
        0x00,       // Reserved bit=0, blocking strategy, block size, sample rate
        0x00        // Channel assignment, sample size, reserved bit=0
    };
    
    bool isValid = demuxer.ValidateFLACFrameHeader(validHeader, 0);
    assert(isValid);
    std::cout << "✓ Valid FLAC frame header passes validation" << std::endl;
    
    // Invalid header with reserved bit set
    std::vector<uint8_t> invalidHeader = {
        0xFF, 0xF8, // Sync pattern
        0x02,       // Reserved bit=1 (invalid)
        0x00
    };
    
    isValid = demuxer.ValidateFLACFrameHeader(invalidHeader, 0);
    assert(!isValid);
    std::cout << "✓ Invalid reserved bit correctly rejected" << std::endl;
    
    // Invalid block size
    invalidHeader = {
        0xFF, 0xF8, // Sync pattern
        0x00,       // Block size index = 0 (reserved)
        0x00
    };
    
    isValid = demuxer.ValidateFLACFrameHeader(invalidHeader, 0);
    assert(!isValid);
    std::cout << "✓ Invalid block size correctly rejected" << std::endl;
}

int main() {
    std::cout << "Running FLAC-in-MP4 support tests..." << std::endl;
    
    try {
        test_flac_codec_constant();
        test_flac_configuration_validation();
        test_flac_frame_boundary_detection();
        test_flac_frame_header_validation();
        
        std::cout << "\n✅ All FLAC-in-MP4 support tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}