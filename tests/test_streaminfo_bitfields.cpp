/*
 * test_streaminfo_bitfields.cpp - Test FLAC STREAMINFO bit-field extraction logic
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>

// Test the bit-field extraction logic directly
struct TestStreamInfo {
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;
    uint64_t total_samples;
};

// Extract STREAMINFO fields using the same logic as FLACDemuxer::parseStreamInfoBlock_unlocked
bool extractStreamInfoFields(const uint8_t* data, TestStreamInfo& info) {
    // Parse STREAMINFO fields according to RFC 9639 bit layout
    // All fields are in big-endian format
    
    // Skip first 10 bytes (min/max block size, min/max frame size)
    const uint8_t* packed_data = data + 10;
    
    // RFC 9639 Section 8.2: Packed fields in bytes 10-13
    // Sample rate (20 bits), channels (3 bits), bits per sample (5 bits)
    // Total samples (36 bits) spans bytes 13-17
    
    // Sample rate (20 bits) - bytes 10-12 + upper 4 bits of byte 13
    // Correct bit extraction: 20 bits total
    info.sample_rate = (static_cast<uint32_t>(packed_data[0]) << 12) |
                       (static_cast<uint32_t>(packed_data[1]) << 4) |
                       ((static_cast<uint32_t>(packed_data[2]) >> 4) & 0x0F);
    
    // Channels (3 bits) - bits 1-3 of byte 12 (after sample rate), then add 1
    // Correct bit extraction: 3 bits from positions 1-3 of byte 12
    info.channels = ((packed_data[2] >> 1) & 0x07) + 1;
    
    // Bits per sample (5 bits) - bit 0 of byte 12 + upper 4 bits of byte 13, then add 1
    // Correct bit extraction: 1 bit from byte 12 + 4 bits from byte 13
    info.bits_per_sample = (((packed_data[2] & 0x01) << 4) | ((packed_data[3] >> 4) & 0x0F)) + 1;
    
    // Total samples (36 bits) - lower 4 bits of byte 13 + bytes 14-17
    // Correct bit extraction: 4 bits from byte 13 + 32 bits from bytes 14-17
    info.total_samples = (static_cast<uint64_t>(packed_data[3] & 0x0F) << 32) |
                         (static_cast<uint64_t>(packed_data[4]) << 24) |
                         (static_cast<uint64_t>(packed_data[5]) << 16) |
                         (static_cast<uint64_t>(packed_data[6]) << 8) |
                         static_cast<uint64_t>(packed_data[7]);
    
    return true;
}

// Generate test STREAMINFO data with known values
std::vector<uint8_t> generateStreamInfoData(uint32_t sample_rate, uint8_t channels, uint8_t bits_per_sample, uint64_t total_samples) {
    std::vector<uint8_t> data(34, 0);  // 34-byte STREAMINFO block
    
    // First 10 bytes (min/max block size, min/max frame size) - set to dummy values
    data[0] = 0x10; data[1] = 0x00; // Min block size = 4096
    data[2] = 0x10; data[3] = 0x00; // Max block size = 4096
    data[4] = 0x00; data[5] = 0x00; data[6] = 0x00; // Min frame size = 0
    data[7] = 0x00; data[8] = 0x00; data[9] = 0x00; // Max frame size = 0
    
    // Pack the fields according to RFC 9639:
    // Sample rate (20 bits), channels (3 bits, stored as channels-1), bits per sample (5 bits, stored as bps-1)
    uint8_t channels_minus_1 = channels - 1;
    uint8_t bps_minus_1 = bits_per_sample - 1;
    
    data[10] = (sample_rate >> 12) & 0xFF;  // Byte 10: SR[19:12]
    data[11] = (sample_rate >> 4) & 0xFF;   // Byte 11: SR[11:4]
    data[12] = ((sample_rate & 0x0F) << 4) | ((channels_minus_1 & 0x07) << 1) | ((bps_minus_1 >> 4) & 0x01); // Byte 12
    data[13] = ((bps_minus_1 & 0x0F) << 4) | ((total_samples >> 32) & 0x0F); // Byte 13: BPS[3:0] + TS[35:32]
    
    // Total samples (remaining 32 bits)
    data[14] = (total_samples >> 24) & 0xFF;
    data[15] = (total_samples >> 16) & 0xFF;
    data[16] = (total_samples >> 8) & 0xFF;
    data[17] = total_samples & 0xFF;
    
    // MD5 signature (16 bytes) - bytes 18-33, set to zeros
    // Already initialized to 0
    
    return data;
}

// Test a specific set of values
bool testValues(uint32_t expected_sample_rate, uint8_t expected_channels, uint8_t expected_bits_per_sample, uint64_t expected_total_samples, const std::string& test_name) {
    std::cout << "Testing " << test_name << "..." << std::endl;
    
    // Generate test data
    auto data = generateStreamInfoData(expected_sample_rate, expected_channels, expected_bits_per_sample, expected_total_samples);
    
    // Extract fields
    TestStreamInfo info;
    if (!extractStreamInfoFields(data.data(), info)) {
        std::cout << "  FAILED: Could not extract fields" << std::endl;
        return false;
    }
    
    // Verify results
    bool success = true;
    
    if (info.sample_rate != expected_sample_rate) {
        std::cout << "  FAILED: Sample rate mismatch - Expected: " << expected_sample_rate << ", Got: " << info.sample_rate << std::endl;
        success = false;
    }
    
    if (info.channels != expected_channels) {
        std::cout << "  FAILED: Channel count mismatch - Expected: " << static_cast<int>(expected_channels) << ", Got: " << static_cast<int>(info.channels) << std::endl;
        success = false;
    }
    
    if (info.bits_per_sample != expected_bits_per_sample) {
        std::cout << "  FAILED: Bits per sample mismatch - Expected: " << static_cast<int>(expected_bits_per_sample) << ", Got: " << static_cast<int>(info.bits_per_sample) << std::endl;
        success = false;
    }
    
    if (info.total_samples != expected_total_samples) {
        std::cout << "  FAILED: Total samples mismatch - Expected: " << expected_total_samples << ", Got: " << info.total_samples << std::endl;
        success = false;
    }
    
    if (success) {
        std::cout << "  PASSED: All fields extracted correctly" << std::endl;
        std::cout << "    Sample rate: " << info.sample_rate << " Hz" << std::endl;
        std::cout << "    Channels: " << static_cast<int>(info.channels) << std::endl;
        std::cout << "    Bits per sample: " << static_cast<int>(info.bits_per_sample) << std::endl;
        std::cout << "    Total samples: " << info.total_samples << std::endl;
    }
    
    return success;
}

int main() {
    std::cout << "FLAC STREAMINFO Bit-Field Extraction Test" << std::endl;
    std::cout << "=========================================" << std::endl;
    
    int passed = 0;
    int total = 0;
    
    // Test 1: Standard CD audio
    total++;
    if (testValues(44100, 2, 16, 1000000, "Standard CD Audio (44.1kHz, 16-bit, stereo)")) {
        passed++;
    }
    
    // Test 2: High-resolution audio
    total++;
    if (testValues(192000, 2, 24, 5000000, "High-Resolution Audio (192kHz, 24-bit, stereo)")) {
        passed++;
    }
    
    // Test 3: Multichannel audio
    total++;
    if (testValues(48000, 6, 16, 2400000, "Multichannel Audio (48kHz, 16-bit, 5.1 surround)")) {
        passed++;
    }
    
    // Test 4: Edge case - mono, low sample rate
    total++;
    if (testValues(8000, 1, 8, 100, "Edge Case (8kHz, 8-bit, mono)")) {
        passed++;
    }
    
    // Test 5: Maximum values
    total++;
    if (testValues(655350, 8, 32, 0xFFFFFFFFFULL, "Maximum Values (655.35kHz, 32-bit, 8-channel)")) {
        passed++;
    }
    
    // Test 6: Specific bit boundary test
    total++;
    if (testValues(96000, 4, 20, 0x123456789ULL, "Bit Boundary Test (96kHz, 20-bit, quad)")) {
        passed++;
    }
    
    std::cout << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "Test Results: " << passed << "/" << total << " tests passed" << std::endl;
    
    if (passed == total) {
        std::cout << "SUCCESS: All STREAMINFO bit-field extractions are correct!" << std::endl;
        return 0;
    } else {
        std::cout << "FAILURE: " << (total - passed) << " test(s) failed!" << std::endl;
        return 1;
    }
}