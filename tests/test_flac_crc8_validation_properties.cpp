/*
 * test_flac_crc8_validation_properties.cpp - Property-based tests for FLAC CRC-8 validation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * **Feature: flac-bisection-seeking, Property 3: CRC-8 Validation (RFC 9639 Section 9.1.8)**
 * **Validates: Requirements 2.3, 2.8**
 *
 * For any FLAC frame header, the CRC-8 calculated using polynomial 0x07 over 
 * header bytes (excluding CRC byte) SHALL match the CRC byte in the header for 
 * valid frames, and SHALL NOT match for corrupted frames.
 */

#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <random>
#include <sstream>
#include <iomanip>

// ========================================
// STANDALONE CRC-8 IMPLEMENTATION
// ========================================

/**
 * RFC 9639 Section 9.1.8: CRC-8 for frame header validation
 * Polynomial: x^8 + x^2 + x + 1 (0x07)
 * Initial value: 0
 * No final XOR
 */

// CRC-8 lookup table for polynomial 0x07
static const uint8_t crc8_table[256] = {
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
    0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
    0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
    0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
    0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
    0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
    0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
    0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
    0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
    0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
    0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
    0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
    0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
    0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
    0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
    0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
    0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
    0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
    0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
    0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
    0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
    0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
    0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
    0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
    0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
    0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
    0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

/**
 * Calculate CRC-8 using lookup table
 */
uint8_t calculateCRC8(const uint8_t* data, size_t length) {
    uint8_t crc = 0;
    for (size_t i = 0; i < length; ++i) {
        crc = crc8_table[crc ^ data[i]];
    }
    return crc;
}

/**
 * Validate CRC-8 in a frame header
 * 
 * @param header_data Complete frame header including CRC byte
 * @param header_length Total length of header including CRC byte
 * @return true if CRC matches, false if corrupted
 */
bool validateFrameHeaderCRC8(const uint8_t* header_data, size_t header_length) {
    if (!header_data || header_length < 5) {
        return false;  // Minimum: sync(2) + header(2) + CRC(1)
    }
    
    // CRC is calculated over all bytes except the CRC byte itself
    size_t crc_data_length = header_length - 1;
    uint8_t expected_crc = header_data[header_length - 1];
    uint8_t calculated_crc = calculateCRC8(header_data, crc_data_length);
    
    return calculated_crc == expected_crc;
}

/**
 * Generate a valid FLAC frame header with correct CRC-8
 * 
 * @param is_variable True for variable block size (0xFFF9), false for fixed (0xFFF8)
 * @param block_size_bits Block size encoding (4 bits)
 * @param sample_rate_bits Sample rate encoding (4 bits)
 * @param channel_bits Channel assignment (4 bits)
 * @param bit_depth_bits Bit depth encoding (3 bits)
 * @param frame_number Frame/sample number (simplified to single byte)
 * @return Vector containing valid frame header with correct CRC-8
 */
std::vector<uint8_t> generateValidFrameHeader(
    bool is_variable,
    uint8_t block_size_bits,
    uint8_t sample_rate_bits,
    uint8_t channel_bits,
    uint8_t bit_depth_bits,
    uint8_t frame_number
) {
    std::vector<uint8_t> header;
    
    // Sync code (2 bytes)
    header.push_back(0xFF);
    header.push_back(is_variable ? 0xF9 : 0xF8);
    
    // Block size (4 bits) + Sample rate (4 bits)
    header.push_back((block_size_bits << 4) | (sample_rate_bits & 0x0F));
    
    // Channel assignment (4 bits) + Bit depth (3 bits) + Reserved (1 bit)
    header.push_back((channel_bits << 4) | ((bit_depth_bits & 0x07) << 1) | 0x00);
    
    // Frame/sample number (simplified to single byte UTF-8)
    header.push_back(frame_number & 0x7F);
    
    // Calculate and append CRC-8
    uint8_t crc = calculateCRC8(header.data(), header.size());
    header.push_back(crc);
    
    return header;
}

/**
 * Helper to format bytes as hex string
 */
std::string bytesToHex(const uint8_t* bytes, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len && i < 16; ++i) {
        if (i > 0) oss << " ";
        oss << "0x" << std::hex << std::setw(2) << std::setfill('0') 
            << static_cast<int>(bytes[i]);
    }
    if (len > 16) oss << " ...";
    return oss.str();
}

// ========================================
// PROPERTY-BASED TESTS
// ========================================

/**
 * **Feature: flac-bisection-seeking, Property 3: CRC-8 Validation (RFC 9639 Section 9.1.8)**
 * **Validates: Requirements 2.3, 2.8**
 *
 * For any FLAC frame header, the CRC-8 calculated using polynomial 0x07 over 
 * header bytes (excluding CRC byte) SHALL match the CRC byte in the header for 
 * valid frames, and SHALL NOT match for corrupted frames.
 */
void test_property_crc8_validation() {
    std::cout << "\n=== Property 3: CRC-8 Validation (RFC 9639 Section 9.1.8) ===" << std::endl;
    std::cout << "Testing CRC-8 validation for valid and corrupted frames..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: Valid headers with correct CRC-8 should pass validation
    // ----------------------------------------
    std::cout << "\n  Test 1: Valid headers with correct CRC-8..." << std::endl;
    {
        // Generate various valid headers
        struct HeaderConfig {
            bool is_variable;
            uint8_t block_size_bits;
            uint8_t sample_rate_bits;
            uint8_t channel_bits;
            uint8_t bit_depth_bits;
            const char* description;
        };
        
        std::vector<HeaderConfig> configs = {
            {false, 0x08, 0x09, 0x01, 0x04, "Fixed, 256 samples, 44.1kHz, stereo, 16-bit"},
            {false, 0x0C, 0x0A, 0x01, 0x04, "Fixed, 4096 samples, 48kHz, stereo, 16-bit"},
            {true,  0x0C, 0x09, 0x01, 0x06, "Variable, 4096 samples, 44.1kHz, stereo, 24-bit"},
            {false, 0x05, 0x04, 0x00, 0x01, "Fixed, 4608 samples, 8kHz, mono, 8-bit"},
            {true,  0x0E, 0x0B, 0x07, 0x04, "Variable, 16384 samples, 96kHz, 8ch, 16-bit"},
        };
        
        for (const auto& cfg : configs) {
            auto header = generateValidFrameHeader(
                cfg.is_variable, cfg.block_size_bits, cfg.sample_rate_bits,
                cfg.channel_bits, cfg.bit_depth_bits, 0x00
            );
            
            tests_run++;
            
            if (validateFrameHeaderCRC8(header.data(), header.size())) {
                tests_passed++;
                std::cout << "    " << cfg.description << " ✓" << std::endl;
            } else {
                std::cerr << "    FAILED: " << cfg.description << std::endl;
                std::cerr << "    Header: " << bytesToHex(header.data(), header.size()) << std::endl;
                assert(false && "Valid header should pass CRC validation");
            }
        }
    }
    
    // ----------------------------------------
    // Test 2: Corrupted headers should fail validation (Requirement 2.8)
    // ----------------------------------------
    std::cout << "\n  Test 2: Corrupted headers should fail validation..." << std::endl;
    {
        // Generate a valid header first
        auto valid_header = generateValidFrameHeader(false, 0x0C, 0x09, 0x01, 0x04, 0x00);
        
        // Test corruption at each byte position (except CRC byte)
        for (size_t corrupt_pos = 0; corrupt_pos < valid_header.size() - 1; ++corrupt_pos) {
            auto corrupted = valid_header;
            corrupted[corrupt_pos] ^= 0x01;  // Flip one bit
            
            tests_run++;
            
            if (!validateFrameHeaderCRC8(corrupted.data(), corrupted.size())) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Corruption at byte " << corrupt_pos 
                          << " was not detected!" << std::endl;
                assert(false && "Corrupted header should fail CRC validation");
            }
        }
        std::cout << "    Single-bit corruption detected at all positions ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 3: Wrong CRC byte should fail validation
    // ----------------------------------------
    std::cout << "\n  Test 3: Wrong CRC byte should fail validation..." << std::endl;
    {
        auto valid_header = generateValidFrameHeader(false, 0x0C, 0x09, 0x01, 0x04, 0x00);
        uint8_t correct_crc = valid_header.back();
        
        // Test all wrong CRC values
        int wrong_crc_detected = 0;
        for (int wrong_crc = 0; wrong_crc < 256; ++wrong_crc) {
            if (static_cast<uint8_t>(wrong_crc) == correct_crc) continue;
            
            auto corrupted = valid_header;
            corrupted.back() = static_cast<uint8_t>(wrong_crc);
            
            tests_run++;
            
            if (!validateFrameHeaderCRC8(corrupted.data(), corrupted.size())) {
                tests_passed++;
                wrong_crc_detected++;
            } else {
                std::cerr << "    FAILED: Wrong CRC 0x" << std::hex << wrong_crc 
                          << " was not detected!" << std::dec << std::endl;
                assert(false && "Wrong CRC should fail validation");
            }
        }
        std::cout << "    All 255 wrong CRC values detected ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 4: Random valid headers (100 iterations)
    // ----------------------------------------
    std::cout << "\n  Test 4: Random valid headers (100 iterations)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> bool_dist(0, 1);
        std::uniform_int_distribution<> block_dist(1, 15);  // Avoid 0 (reserved)
        std::uniform_int_distribution<> rate_dist(0, 14);   // Avoid 15 (forbidden)
        std::uniform_int_distribution<> chan_dist(0, 10);   // Avoid 11-15 (reserved)
        std::uniform_int_distribution<> depth_dist(0, 7);   // Avoid 3 (reserved)
        std::uniform_int_distribution<> frame_dist(0, 127);
        
        int random_passed = 0;
        
        for (int i = 0; i < 100; ++i) {
            bool is_variable = (bool_dist(gen) == 1);
            uint8_t block_bits = static_cast<uint8_t>(block_dist(gen));
            uint8_t rate_bits = static_cast<uint8_t>(rate_dist(gen));
            uint8_t chan_bits = static_cast<uint8_t>(chan_dist(gen));
            uint8_t depth_bits = static_cast<uint8_t>(depth_dist(gen));
            if (depth_bits == 3) depth_bits = 4;  // Skip reserved
            uint8_t frame_num = static_cast<uint8_t>(frame_dist(gen));
            
            auto header = generateValidFrameHeader(
                is_variable, block_bits, rate_bits, chan_bits, depth_bits, frame_num
            );
            
            tests_run++;
            
            if (validateFrameHeaderCRC8(header.data(), header.size())) {
                tests_passed++;
                random_passed++;
            } else {
                std::cerr << "    FAILED iteration " << i << std::endl;
                assert(false && "Random valid header should pass CRC validation");
            }
        }
        std::cout << "    " << random_passed << "/100 random valid headers passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 5: Random corrupted headers (100 iterations)
    // ----------------------------------------
    std::cout << "\n  Test 5: Random corrupted headers (100 iterations)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> bool_dist(0, 1);
        std::uniform_int_distribution<> block_dist(1, 15);
        std::uniform_int_distribution<> rate_dist(0, 14);
        std::uniform_int_distribution<> chan_dist(0, 10);
        std::uniform_int_distribution<> depth_dist(0, 7);
        std::uniform_int_distribution<> frame_dist(0, 127);
        std::uniform_int_distribution<> byte_dist(0, 255);
        
        int random_passed = 0;
        
        for (int i = 0; i < 100; ++i) {
            // Generate valid header
            bool is_variable = (bool_dist(gen) == 1);
            uint8_t block_bits = static_cast<uint8_t>(block_dist(gen));
            uint8_t rate_bits = static_cast<uint8_t>(rate_dist(gen));
            uint8_t chan_bits = static_cast<uint8_t>(chan_dist(gen));
            uint8_t depth_bits = static_cast<uint8_t>(depth_dist(gen));
            if (depth_bits == 3) depth_bits = 4;
            uint8_t frame_num = static_cast<uint8_t>(frame_dist(gen));
            
            auto header = generateValidFrameHeader(
                is_variable, block_bits, rate_bits, chan_bits, depth_bits, frame_num
            );
            
            // Corrupt a random byte (not the CRC byte)
            std::uniform_int_distribution<> pos_dist(0, static_cast<int>(header.size()) - 2);
            size_t corrupt_pos = static_cast<size_t>(pos_dist(gen));
            uint8_t original = header[corrupt_pos];
            uint8_t corrupted_byte;
            do {
                corrupted_byte = static_cast<uint8_t>(byte_dist(gen));
            } while (corrupted_byte == original);
            header[corrupt_pos] = corrupted_byte;
            
            tests_run++;
            
            if (!validateFrameHeaderCRC8(header.data(), header.size())) {
                tests_passed++;
                random_passed++;
            } else {
                std::cerr << "    FAILED iteration " << i << ": corruption not detected" << std::endl;
                assert(false && "Random corrupted header should fail CRC validation");
            }
        }
        std::cout << "    " << random_passed << "/100 random corrupted headers detected ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 6: Edge cases
    // ----------------------------------------
    std::cout << "\n  Test 6: Edge cases..." << std::endl;
    {
        // Null pointer
        tests_run++;
        if (!validateFrameHeaderCRC8(nullptr, 0)) {
            tests_passed++;
            std::cout << "    Null pointer handled safely ✓" << std::endl;
        } else {
            assert(false && "Null pointer should fail validation");
        }
        
        // Too short header
        tests_run++;
        uint8_t short_header[] = {0xFF, 0xF8, 0x00, 0x00};
        if (!validateFrameHeaderCRC8(short_header, 4)) {
            tests_passed++;
            std::cout << "    Too short header rejected ✓" << std::endl;
        } else {
            assert(false && "Too short header should fail validation");
        }
        
        // Minimum valid header (5 bytes)
        tests_run++;
        auto min_header = generateValidFrameHeader(false, 0x08, 0x09, 0x01, 0x04, 0x00);
        if (validateFrameHeaderCRC8(min_header.data(), min_header.size())) {
            tests_passed++;
            std::cout << "    Minimum valid header accepted ✓" << std::endl;
        } else {
            assert(false && "Minimum valid header should pass validation");
        }
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 3: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC CRC-8 VALIDATION PROPERTY-BASED TESTS" << std::endl;
    std::cout << "Feature: flac-bisection-seeking" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // **Feature: flac-bisection-seeking, Property 3: CRC-8 Validation (RFC 9639 Section 9.1.8)**
        // **Validates: Requirements 2.3, 2.8**
        test_property_crc8_validation();
        
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "✅ ALL PROPERTY TESTS PASSED" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(70, '=') << std::endl;
        std::cerr << "❌ PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Exception: " << e.what() << std::endl;
        std::cerr << std::string(70, '=') << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n" << std::string(70, '=') << std::endl;
        std::cerr << "❌ PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Unknown exception occurred" << std::endl;
        std::cerr << std::string(70, '=') << std::endl;
        return 1;
    }
}
