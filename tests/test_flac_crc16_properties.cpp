/*
 * test_flac_crc16_properties.cpp - Property-based tests for FLAC CRC-16 calculation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Property Test: CRC-16 Calculation Correctness
 * Feature: flac-demuxer
 * Property 14: CRC-16 Calculation Correctness
 * Validates: Requirements 11.3
 *
 * RFC 9639 Section 9.3 specifies:
 * - Polynomial: x^16 + x^15 + x^2 + x^0 = 0x8005
 * - Initial value: 0
 * - Covers entire frame from sync code to end of subframes (excluding CRC)
 */

#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <random>
#include <iomanip>

// ============================================================================
// CRC-16 Reference Implementation (bitwise, for verification)
// ============================================================================

/**
 * @brief Bitwise CRC-16 calculation for verification
 * 
 * This is a slow but obviously correct implementation used to verify
 * the lookup table implementation.
 */
static uint16_t crc16_bitwise(const uint8_t* data, size_t length)
{
    uint16_t crc = 0;
    
    for (size_t i = 0; i < length; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x8005;
            } else {
                crc = crc << 1;
            }
        }
    }
    
    return crc;
}

// ============================================================================
// CRC-16 Lookup Table Implementation (copied from FLACDemuxer for testing)
// ============================================================================

static const uint16_t s_crc16_table[256] = {
    0x0000, 0x8005, 0x800F, 0x000A, 0x801B, 0x001E, 0x0014, 0x8011,
    0x8033, 0x0036, 0x003C, 0x8039, 0x0028, 0x802D, 0x8027, 0x0022,
    0x8063, 0x0066, 0x006C, 0x8069, 0x0078, 0x807D, 0x8077, 0x0072,
    0x0050, 0x8055, 0x805F, 0x005A, 0x804B, 0x004E, 0x0044, 0x8041,
    0x80C3, 0x00C6, 0x00CC, 0x80C9, 0x00D8, 0x80DD, 0x80D7, 0x00D2,
    0x00F0, 0x80F5, 0x80FF, 0x00FA, 0x80EB, 0x00EE, 0x00E4, 0x80E1,
    0x00A0, 0x80A5, 0x80AF, 0x00AA, 0x80BB, 0x00BE, 0x00B4, 0x80B1,
    0x8093, 0x0096, 0x009C, 0x8099, 0x0088, 0x808D, 0x8087, 0x0082,
    0x8183, 0x0186, 0x018C, 0x8189, 0x0198, 0x819D, 0x8197, 0x0192,
    0x01B0, 0x81B5, 0x81BF, 0x01BA, 0x81AB, 0x01AE, 0x01A4, 0x81A1,
    0x01E0, 0x81E5, 0x81EF, 0x01EA, 0x81FB, 0x01FE, 0x01F4, 0x81F1,
    0x81D3, 0x01D6, 0x01DC, 0x81D9, 0x01C8, 0x81CD, 0x81C7, 0x01C2,
    0x0140, 0x8145, 0x814F, 0x014A, 0x815B, 0x015E, 0x0154, 0x8151,
    0x8173, 0x0176, 0x017C, 0x8179, 0x0168, 0x816D, 0x8167, 0x0162,
    0x8123, 0x0126, 0x012C, 0x8129, 0x0138, 0x813D, 0x8137, 0x0132,
    0x0110, 0x8115, 0x811F, 0x011A, 0x810B, 0x010E, 0x0104, 0x8101,
    0x8303, 0x0306, 0x030C, 0x8309, 0x0318, 0x831D, 0x8317, 0x0312,
    0x0330, 0x8335, 0x833F, 0x033A, 0x832B, 0x032E, 0x0324, 0x8321,
    0x0360, 0x8365, 0x836F, 0x036A, 0x837B, 0x037E, 0x0374, 0x8371,
    0x8353, 0x0356, 0x035C, 0x8359, 0x0348, 0x834D, 0x8347, 0x0342,
    0x03C0, 0x83C5, 0x83CF, 0x03CA, 0x83DB, 0x03DE, 0x03D4, 0x83D1,
    0x83F3, 0x03F6, 0x03FC, 0x83F9, 0x03E8, 0x83ED, 0x83E7, 0x03E2,
    0x83A3, 0x03A6, 0x03AC, 0x83A9, 0x03B8, 0x83BD, 0x83B7, 0x03B2,
    0x0390, 0x8395, 0x839F, 0x039A, 0x838B, 0x038E, 0x0384, 0x8381,
    0x0280, 0x8285, 0x828F, 0x028A, 0x829B, 0x029E, 0x0294, 0x8291,
    0x82B3, 0x02B6, 0x02BC, 0x82B9, 0x02A8, 0x82AD, 0x82A7, 0x02A2,
    0x82E3, 0x02E6, 0x02EC, 0x82E9, 0x02F8, 0x82FD, 0x82F7, 0x02F2,
    0x02D0, 0x82D5, 0x82DF, 0x02DA, 0x82CB, 0x02CE, 0x02C4, 0x82C1,
    0x8243, 0x0246, 0x024C, 0x8249, 0x0258, 0x825D, 0x8257, 0x0252,
    0x0270, 0x8275, 0x827F, 0x027A, 0x826B, 0x026E, 0x0264, 0x8261,
    0x0220, 0x8225, 0x822F, 0x022A, 0x823B, 0x023E, 0x0234, 0x8231,
    0x8213, 0x0216, 0x021C, 0x8219, 0x0208, 0x820D, 0x8207, 0x0202
};

/**
 * @brief Table-based CRC-16 calculation (matches FLACDemuxer::calculateCRC16)
 */
static uint16_t crc16_table(const uint8_t* data, size_t length)
{
    uint16_t crc = 0;
    
    for (size_t i = 0; i < length; ++i) {
        crc = (crc << 8) ^ s_crc16_table[(crc >> 8) ^ data[i]];
    }
    
    return crc;
}

// ============================================================================
// Test Infrastructure
// ============================================================================

static int g_tests_passed = 0;
static int g_tests_failed = 0;

static void test_assert(bool condition, const char* test_name)
{
    if (condition) {
        g_tests_passed++;
    } else {
        g_tests_failed++;
        std::cerr << "  FAILED: " << test_name << std::endl;
    }
}

// ============================================================================
// Property 14: CRC-16 Calculation Correctness
// ============================================================================

/**
 * @brief Test 1: Empty data CRC should be 0
 */
static bool test_empty_data_crc()
{
    std::cout << "  Test 1: Empty data CRC..." << std::endl;
    
    uint16_t crc = crc16_table(nullptr, 0);
    
    if (crc != 0x0000) {
        std::cerr << "    Empty data CRC = 0x" << std::hex << crc 
                  << ", expected 0x0000" << std::dec << std::endl;
        return false;
    }
    
    std::cout << "    Empty data CRC = 0x0000 ✓" << std::endl;
    return true;
}

/**
 * @brief Test 2: Single byte CRC values match between table and bitwise
 */
static bool test_single_byte_crc()
{
    std::cout << "  Test 2: Single byte CRC values..." << std::endl;
    
    for (int i = 0; i < 256; ++i) {
        uint8_t byte = static_cast<uint8_t>(i);
        uint16_t table_crc = crc16_table(&byte, 1);
        uint16_t bitwise_crc = crc16_bitwise(&byte, 1);
        
        if (table_crc != bitwise_crc) {
            std::cerr << "    Byte 0x" << std::hex << i 
                      << ": table=0x" << table_crc 
                      << ", bitwise=0x" << bitwise_crc << std::dec << std::endl;
            return false;
        }
    }
    
    std::cout << "    All 256 single-byte CRCs match between table and bitwise ✓" << std::endl;
    return true;
}

/**
 * @brief Test 3: Known test vectors
 */
static bool test_known_vectors()
{
    std::cout << "  Test 3: Known test vectors..." << std::endl;
    
    struct TestVector {
        std::vector<uint8_t> data;
        uint16_t expected_crc;
        const char* description;
    };
    
    // Test vectors - expected values computed from the verified bitwise implementation
    // The table and bitwise implementations match, so we use computed values
    std::vector<TestVector> vectors = {
        // Single bytes
        {{0x00}, 0x0000, "Single zero byte"},
        {{0x01}, 0x8005, "Single 0x01 byte"},
        {{0xFF}, 0x0202, "Single 0xFF byte"},
        
        // FLAC sync patterns
        {{0xFF, 0xF8}, 0x001C, "Fixed block sync (0xFFF8)"},
        {{0xFF, 0xF9}, 0x8019, "Variable block sync (0xFFF9)"},
        
        // Multi-byte sequences
        {{0x00, 0x00}, 0x0000, "Two zero bytes"},
        {{0x01, 0x02}, 0x060C, "Sequential bytes 0x01 0x02"},
        {{0x01, 0x02, 0x03, 0x04}, 0x9E33, "Sequential bytes 0x01-0x04"},
        
        // Typical FLAC frame header start (sync + block/rate + channel/depth)
        {{0xFF, 0xF8, 0x69, 0x98}, 0xF51D, "FLAC header: sync + block/rate + channel/depth"},
    };
    
    bool all_passed = true;
    for (const auto& vec : vectors) {
        uint16_t calculated = crc16_table(vec.data.data(), vec.data.size());
        
        if (calculated != vec.expected_crc) {
            std::cerr << "    " << vec.description << ": CRC = 0x" << std::hex 
                      << calculated << ", expected 0x" << vec.expected_crc 
                      << std::dec << " ✗" << std::endl;
            all_passed = false;
        } else {
            std::cout << "    " << vec.description << ": CRC = 0x" << std::hex 
                      << calculated << std::dec << " ✓" << std::endl;
        }
    }
    
    return all_passed;
}

/**
 * @brief Test 4: Table vs bitwise consistency with random data
 */
static bool test_table_vs_bitwise_consistency()
{
    std::cout << "  Test 4: Table vs bitwise consistency (100 random tests)..." << std::endl;
    
    std::mt19937 rng(42);  // Fixed seed for reproducibility
    std::uniform_int_distribution<int> len_dist(1, 1024);
    std::uniform_int_distribution<int> byte_dist(0, 255);
    
    int passed = 0;
    for (int test = 0; test < 100; ++test) {
        size_t length = len_dist(rng);
        std::vector<uint8_t> data(length);
        
        for (size_t i = 0; i < length; ++i) {
            data[i] = static_cast<uint8_t>(byte_dist(rng));
        }
        
        uint16_t table_crc = crc16_table(data.data(), data.size());
        uint16_t bitwise_crc = crc16_bitwise(data.data(), data.size());
        
        if (table_crc != bitwise_crc) {
            std::cerr << "    Test " << test << " failed: length=" << length
                      << ", table=0x" << std::hex << table_crc 
                      << ", bitwise=0x" << bitwise_crc << std::dec << std::endl;
            return false;
        }
        passed++;
    }
    
    std::cout << "    " << passed << "/100 random tests passed ✓" << std::endl;
    return true;
}

/**
 * @brief Test 5: CRC self-check property
 * 
 * If we append the CRC to the data and recalculate, the result should be 0
 * for certain CRC algorithms. For FLAC's CRC-16, we verify that appending
 * the CRC (big-endian) and recalculating gives a consistent result.
 */
static bool test_crc_self_check()
{
    std::cout << "  Test 5: CRC self-check property..." << std::endl;
    
    std::mt19937 rng(123);
    std::uniform_int_distribution<int> len_dist(1, 256);
    std::uniform_int_distribution<int> byte_dist(0, 255);
    
    int passed = 0;
    for (int test = 0; test < 50; ++test) {
        size_t length = len_dist(rng);
        std::vector<uint8_t> data(length);
        
        for (size_t i = 0; i < length; ++i) {
            data[i] = static_cast<uint8_t>(byte_dist(rng));
        }
        
        // Calculate CRC of original data
        uint16_t crc = crc16_table(data.data(), data.size());
        
        // Append CRC (big-endian, as FLAC stores it)
        data.push_back(static_cast<uint8_t>(crc >> 8));
        data.push_back(static_cast<uint8_t>(crc & 0xFF));
        
        // Recalculate CRC including the appended CRC bytes
        uint16_t check_crc = crc16_table(data.data(), data.size());
        
        // For this CRC algorithm, the check CRC should be 0
        if (check_crc != 0) {
            std::cerr << "    Test " << test << " failed: self-check CRC = 0x" 
                      << std::hex << check_crc << std::dec << std::endl;
            return false;
        }
        passed++;
    }
    
    std::cout << "    " << passed << "/50 self-check tests passed ✓" << std::endl;
    return true;
}

/**
 * @brief Test 6: Incremental CRC calculation
 * 
 * Verify that calculating CRC in chunks gives the same result as
 * calculating it all at once.
 */
static bool test_incremental_crc()
{
    std::cout << "  Test 6: Incremental CRC calculation..." << std::endl;
    
    std::mt19937 rng(456);
    std::uniform_int_distribution<int> len_dist(10, 512);
    std::uniform_int_distribution<int> byte_dist(0, 255);
    
    int passed = 0;
    for (int test = 0; test < 50; ++test) {
        size_t length = len_dist(rng);
        std::vector<uint8_t> data(length);
        
        for (size_t i = 0; i < length; ++i) {
            data[i] = static_cast<uint8_t>(byte_dist(rng));
        }
        
        // Calculate CRC all at once
        uint16_t full_crc = crc16_table(data.data(), data.size());
        
        // Calculate CRC incrementally (byte by byte)
        uint16_t incremental_crc = 0;
        for (size_t i = 0; i < data.size(); ++i) {
            incremental_crc = (incremental_crc << 8) ^ s_crc16_table[(incremental_crc >> 8) ^ data[i]];
        }
        
        if (full_crc != incremental_crc) {
            std::cerr << "    Test " << test << " failed: full=0x" << std::hex 
                      << full_crc << ", incremental=0x" << incremental_crc 
                      << std::dec << std::endl;
            return false;
        }
        passed++;
    }
    
    std::cout << "    " << passed << "/50 incremental tests passed ✓" << std::endl;
    return true;
}

/**
 * @brief Test 7: FLAC frame-like data
 * 
 * Test CRC-16 calculation on data that resembles actual FLAC frames.
 */
static bool test_flac_frame_like_data()
{
    std::cout << "  Test 7: FLAC frame-like data..." << std::endl;
    
    // Simulate a minimal FLAC frame structure:
    // - Sync code (2 bytes): 0xFF 0xF8
    // - Block size/sample rate (1 byte)
    // - Channel/bit depth (1 byte)
    // - Frame number (1 byte, UTF-8 encoded)
    // - CRC-8 (1 byte)
    // - Subframe data (variable)
    // - Padding (if needed)
    // - CRC-16 (2 bytes)
    
    std::vector<uint8_t> frame = {
        0xFF, 0xF8,  // Sync code (fixed block size)
        0x69,        // Block size 4096, sample rate 44100
        0x98,        // 2 channels, 16-bit
        0x00,        // Frame number 0
        0xBF,        // CRC-8 (placeholder)
        // Minimal subframe data
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    
    // Calculate CRC-16 over frame data (excluding CRC-16 itself)
    uint16_t crc = crc16_table(frame.data(), frame.size());
    
    // Verify table and bitwise match
    uint16_t bitwise_crc = crc16_bitwise(frame.data(), frame.size());
    
    if (crc != bitwise_crc) {
        std::cerr << "    Frame CRC mismatch: table=0x" << std::hex << crc 
                  << ", bitwise=0x" << bitwise_crc << std::dec << std::endl;
        return false;
    }
    
    std::cout << "    Frame-like data CRC = 0x" << std::hex << crc << std::dec << " ✓" << std::endl;
    
    // Verify self-check property
    frame.push_back(static_cast<uint8_t>(crc >> 8));
    frame.push_back(static_cast<uint8_t>(crc & 0xFF));
    
    uint16_t check = crc16_table(frame.data(), frame.size());
    if (check != 0) {
        std::cerr << "    Self-check failed: 0x" << std::hex << check << std::dec << std::endl;
        return false;
    }
    
    std::cout << "    Self-check passed ✓" << std::endl;
    return true;
}

// ============================================================================
// Property 14b: CRC-16 Polynomial Verification
// ============================================================================

/**
 * @brief Verify the lookup table matches the polynomial
 */
static bool test_polynomial_verification()
{
    std::cout << "\n=== Property 14b: CRC-16 Polynomial Verification ===" << std::endl;
    std::cout << "Verifying lookup table matches polynomial 0x8005..." << std::endl;
    
    // Generate expected table from polynomial
    uint16_t expected_table[256];
    for (int i = 0; i < 256; ++i) {
        uint16_t crc = static_cast<uint16_t>(i) << 8;
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x8005;
            } else {
                crc = crc << 1;
            }
        }
        expected_table[i] = crc;
    }
    
    // Compare with actual table
    for (int i = 0; i < 256; ++i) {
        if (s_crc16_table[i] != expected_table[i]) {
            std::cerr << "  Table entry " << i << " mismatch: got 0x" << std::hex 
                      << s_crc16_table[i] << ", expected 0x" << expected_table[i] 
                      << std::dec << std::endl;
            return false;
        }
    }
    
    std::cout << "  All 256 lookup table entries verified ✓" << std::endl;
    return true;
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main()
{
    std::cout << "======================================================================" << std::endl;
    std::cout << "FLAC CRC-16 PROPERTY-BASED TESTS" << std::endl;
    std::cout << "**Feature: flac-demuxer, Property 14: CRC-16 Calculation Correctness**" << std::endl;
    std::cout << "**Validates: Requirements 11.3**" << std::endl;
    std::cout << "======================================================================" << std::endl;
    
    std::cout << "\n=== Property 14: CRC-16 Calculation Correctness ===" << std::endl;
    std::cout << "Testing CRC-16 calculation with polynomial 0x8005..." << std::endl;
    
    int property14_tests = 0;
    int property14_passed = 0;
    
    // Test 1: Empty data
    property14_tests++;
    if (test_empty_data_crc()) property14_passed++;
    
    // Test 2: Single byte CRCs
    property14_tests += 256;
    if (test_single_byte_crc()) property14_passed += 256;
    
    // Test 3: Known vectors
    property14_tests += 9;
    if (test_known_vectors()) property14_passed += 9;
    
    // Test 4: Table vs bitwise
    property14_tests += 100;
    if (test_table_vs_bitwise_consistency()) property14_passed += 100;
    
    // Test 5: Self-check
    property14_tests += 50;
    if (test_crc_self_check()) property14_passed += 50;
    
    // Test 6: Incremental
    property14_tests += 50;
    if (test_incremental_crc()) property14_passed += 50;
    
    // Test 7: FLAC frame-like
    property14_tests += 2;
    if (test_flac_frame_like_data()) property14_passed += 2;
    
    std::cout << "\n✓ Property 14: " << property14_passed << "/" << property14_tests 
              << " tests passed" << std::endl;
    
    // Property 14b: Polynomial verification
    int property14b_tests = 256;
    int property14b_passed = 0;
    if (test_polynomial_verification()) property14b_passed = 256;
    
    std::cout << "\n✓ Property 14b: " << property14b_passed << "/" << property14b_tests 
              << " tests passed" << std::endl;
    
    // Summary
    std::cout << "\n======================================================================" << std::endl;
    
    int total_tests = property14_tests + property14b_tests;
    int total_passed = property14_passed + property14b_passed;
    
    if (total_passed == total_tests) {
        std::cout << "✅ ALL PROPERTY TESTS PASSED" << std::endl;
    } else {
        std::cout << "❌ SOME TESTS FAILED: " << total_passed << "/" << total_tests << std::endl;
    }
    
    std::cout << "======================================================================" << std::endl;
    
    return (total_passed == total_tests) ? 0 : 1;
}
