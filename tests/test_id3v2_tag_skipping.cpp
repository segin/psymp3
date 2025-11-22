/*
 * test_id3v2_tag_skipping.cpp - Test ID3v2 tag skipping in FLAC files
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include <iostream>
#include <vector>
#include <cstring>

/**
 * @brief Create a minimal FLAC file with ID3v2 tag prepended
 * 
 * This creates a test FLAC file with an ID3v2.3 tag at the beginning.
 * The ID3v2 tag structure:
 * - 3 bytes: "ID3"
 * - 2 bytes: version (0x03 0x00 for ID3v2.3)
 * - 1 byte: flags (0x00)
 * - 4 bytes: size as synchsafe integer
 * - N bytes: tag data
 * 
 * Followed by a minimal FLAC stream:
 * - 4 bytes: "fLaC"
 * - STREAMINFO metadata block
 */
std::vector<uint8_t> createFLACWithID3v2Tag() {
    std::vector<uint8_t> data;
    
    // ID3v2.3 header
    data.push_back('I');
    data.push_back('D');
    data.push_back('3');
    data.push_back(0x03);  // Major version
    data.push_back(0x00);  // Minor version
    data.push_back(0x00);  // Flags
    
    // Tag size: 100 bytes as synchsafe integer (7 bits per byte)
    // 100 = 0x64 = 0b01100100
    // Synchsafe: 0x00 0x00 0x00 0x64
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x64);  // 100 bytes
    
    // Add 100 bytes of dummy tag data
    for (int i = 0; i < 100; i++) {
        data.push_back(0x00);
    }
    
    // Now add the FLAC stream marker
    data.push_back('f');
    data.push_back('L');
    data.push_back('a');
    data.push_back('C');
    
    // Add minimal STREAMINFO block (last metadata block)
    data.push_back(0x80);  // Last block flag + type 0 (STREAMINFO)
    data.push_back(0x00);  // Length MSB
    data.push_back(0x00);  // Length
    data.push_back(0x22);  // Length LSB (34 bytes)
    
    // STREAMINFO data (34 bytes)
    // Min block size (16-bit): 4096
    data.push_back(0x10);
    data.push_back(0x00);
    
    // Max block size (16-bit): 4096
    data.push_back(0x10);
    data.push_back(0x00);
    
    // Min frame size (24-bit): 0 (unknown)
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);
    
    // Max frame size (24-bit): 0 (unknown)
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);
    
    // Sample rate (20-bit): 44100 Hz = 0xAC44
    // Channels (3-bit): 2 channels (value 1)
    // Bits per sample (5-bit): 16 bits (value 15)
    // Combined: 0xAC44 << 4 | 0x1 << 1 | 0xF >> 4 = 0xAC441
    data.push_back(0x0A);  // Sample rate bits 19-12
    data.push_back(0xC4);  // Sample rate bits 11-4
    data.push_back(0x42);  // Sample rate bits 3-0, channels bits 2-0, bps bit 4
    data.push_back(0xF0);  // Bits per sample bits 3-0, total samples bits 35-32
    
    // Total samples (36-bit): 0 (unknown)
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);
    
    // MD5 signature (128-bit): all zeros
    for (int i = 0; i < 16; i++) {
        data.push_back(0x00);
    }
    
    return data;
}

/**
 * @brief Test that FLACDemuxer can skip ID3v2 tags
 */
bool testID3v2TagSkipping() {
    std::cout << "Testing ID3v2 tag skipping..." << std::endl;
    
    // Create test data with ID3v2 tag
    std::vector<uint8_t> test_data = createFLACWithID3v2Tag();
    
    std::cout << "  Created test FLAC file with ID3v2 tag (" << test_data.size() << " bytes)" << std::endl;
    std::cout << "  ID3v2 tag size: 110 bytes (10 byte header + 100 byte data)" << std::endl;
    std::cout << "  fLaC marker should be at offset 110" << std::endl;
    
    // Write test data to temporary file
    const char* temp_file = "/tmp/test_flac_with_id3.flac";
    FILE* f = fopen(temp_file, "wb");
    if (!f) {
        std::cerr << "  ERROR: Failed to create temporary test file" << std::endl;
        return false;
    }
    
    size_t written = fwrite(test_data.data(), 1, test_data.size(), f);
    fclose(f);
    
    if (written != test_data.size()) {
        std::cerr << "  ERROR: Failed to write complete test data" << std::endl;
        return false;
    }
    
    std::cout << "  Wrote test file to " << temp_file << std::endl;
    
    // Create IOHandler for the test file
    auto handler = std::make_unique<FileIOHandler>(temp_file);
    if (!handler) {
        std::cerr << "  ERROR: Failed to create IOHandler" << std::endl;
        return false;
    }
    
    std::cout << "  Created IOHandler for test file" << std::endl;
    
    // Create FLACDemuxer
    PsyMP3::Demuxer::FLAC::FLACDemuxer demuxer(std::move(handler));
    
    std::cout << "  Created FLACDemuxer" << std::endl;
    
    // Parse container - this should skip the ID3v2 tag and find the fLaC marker
    bool parse_result = demuxer.parseContainer();
    
    if (!parse_result) {
        std::cerr << "  ERROR: Failed to parse FLAC container (ID3v2 tag skipping may have failed)" << std::endl;
        return false;
    }
    
    std::cout << "  Successfully parsed FLAC container (ID3v2 tag was skipped)" << std::endl;
    
    // Verify stream info was parsed correctly
    StreamInfo stream_info = demuxer.getStreamInfo(0);
    
    if (stream_info.sample_rate != 44100) {
        std::cerr << "  ERROR: Incorrect sample rate: " << stream_info.sample_rate << " (expected 44100)" << std::endl;
        return false;
    }
    
    if (stream_info.channels != 2) {
        std::cerr << "  ERROR: Incorrect channel count: " << stream_info.channels << " (expected 2)" << std::endl;
        return false;
    }
    
    if (stream_info.bits_per_sample != 16) {
        std::cerr << "  ERROR: Incorrect bits per sample: " << stream_info.bits_per_sample << " (expected 16)" << std::endl;
        return false;
    }
    
    std::cout << "  Stream info verified: 44100 Hz, 2 channels, 16 bits" << std::endl;
    std::cout << "  SUCCESS: ID3v2 tag skipping works correctly!" << std::endl;
    
    // Clean up
    remove(temp_file);
    
    return true;
}

/**
 * @brief Test that FLACDemuxer can skip multiple ID3v2 tags
 */
bool testMultipleID3v2Tags() {
    std::cout << "\nTesting multiple ID3v2 tag skipping..." << std::endl;
    
    std::vector<uint8_t> data;
    
    // Add first ID3v2 tag (50 bytes)
    data.push_back('I');
    data.push_back('D');
    data.push_back('3');
    data.push_back(0x03);
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x32);  // 50 bytes
    for (int i = 0; i < 50; i++) {
        data.push_back(0x00);
    }
    
    // Add second ID3v2 tag (30 bytes)
    data.push_back('I');
    data.push_back('D');
    data.push_back('3');
    data.push_back(0x03);
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x1E);  // 30 bytes
    for (int i = 0; i < 30; i++) {
        data.push_back(0x00);
    }
    
    // Add FLAC stream (same as before)
    std::vector<uint8_t> flac_part = createFLACWithID3v2Tag();
    // Skip the ID3v2 tag part, only copy from fLaC marker onwards
    data.insert(data.end(), flac_part.begin() + 110, flac_part.end());
    
    std::cout << "  Created test FLAC file with 2 ID3v2 tags (" << data.size() << " bytes)" << std::endl;
    
    // Write and test
    const char* temp_file = "/tmp/test_flac_multi_id3.flac";
    FILE* f = fopen(temp_file, "wb");
    if (!f) {
        std::cerr << "  ERROR: Failed to create temporary test file" << std::endl;
        return false;
    }
    
    fwrite(data.data(), 1, data.size(), f);
    fclose(f);
    
    auto handler = std::make_unique<FileIOHandler>(temp_file);
    if (!handler) {
        std::cerr << "  ERROR: Failed to create IOHandler" << std::endl;
        return false;
    }
    
    PsyMP3::Demuxer::FLAC::FLACDemuxer demuxer(std::move(handler));
    
    bool parse_result = demuxer.parseContainer();
    
    if (!parse_result) {
        std::cerr << "  ERROR: Failed to parse FLAC container with multiple ID3v2 tags" << std::endl;
        return false;
    }
    
    std::cout << "  SUCCESS: Multiple ID3v2 tags skipped correctly!" << std::endl;
    
    remove(temp_file);
    return true;
}

int main() {
    std::cout << "=== ID3v2 Tag Skipping Tests ===" << std::endl;
    std::cout << std::endl;
    
    bool test1 = testID3v2TagSkipping();
    bool test2 = testMultipleID3v2Tags();
    
    std::cout << std::endl;
    std::cout << "=== Test Results ===" << std::endl;
    std::cout << "Single ID3v2 tag: " << (test1 ? "PASS" : "FAIL") << std::endl;
    std::cout << "Multiple ID3v2 tags: " << (test2 ? "PASS" : "FAIL") << std::endl;
    
    bool all_passed = test1 && test2;
    std::cout << std::endl;
    std::cout << "Overall: " << (all_passed ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << std::endl;
    
    return all_passed ? 0 : 1;
}
