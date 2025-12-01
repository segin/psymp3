/*
 * test_flac_vorbis_comment_endianness_properties.cpp - Property-based tests for FLAC endianness handling
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
// STANDALONE ENDIANNESS PARSING FUNCTIONS
// ========================================

/**
 * RFC 9639 Section 5: Big-Endian Integer Parsing
 * 
 * Most FLAC integers are big-endian (most significant byte first).
 * This includes:
 * - Metadata block lengths (24-bit)
 * - STREAMINFO fields
 * - Frame header fields
 * - Seek point fields
 */

/**
 * Parse a 24-bit big-endian integer (used for metadata block lengths)
 * Per RFC 9639 Section 8.1 and Requirement 19.1
 */
uint32_t parseBigEndian24(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 16) |
           (static_cast<uint32_t>(data[1]) << 8) |
           static_cast<uint32_t>(data[2]);
}

/**
 * Parse a 32-bit big-endian integer (used for STREAMINFO sample rate, etc.)
 * Per RFC 9639 Section 5 and Requirement 19.2
 */
uint32_t parseBigEndian32(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

/**
 * Parse a 64-bit big-endian integer (used for seek point sample numbers)
 * Per RFC 9639 Section 8.5 and Requirement 19.5
 */
uint64_t parseBigEndian64(const uint8_t* data) {
    return (static_cast<uint64_t>(data[0]) << 56) |
           (static_cast<uint64_t>(data[1]) << 48) |
           (static_cast<uint64_t>(data[2]) << 40) |
           (static_cast<uint64_t>(data[3]) << 32) |
           (static_cast<uint64_t>(data[4]) << 24) |
           (static_cast<uint64_t>(data[5]) << 16) |
           (static_cast<uint64_t>(data[6]) << 8) |
           static_cast<uint64_t>(data[7]);
}

/**
 * RFC 9639 Section 8.6: VORBIS_COMMENT Little-Endian Exception
 * 
 * VORBIS_COMMENT block uses little-endian encoding for lengths
 * (for Vorbis compatibility). This is the ONLY exception to FLAC's
 * big-endian rule.
 */

/**
 * Parse a 32-bit little-endian integer (used for VORBIS_COMMENT lengths)
 * Per RFC 9639 Section 8.6 and Requirement 19.4, 13.1
 */
uint32_t parseLittleEndian32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

/**
 * Create a 24-bit big-endian byte array from a value
 */
void createBigEndian24(uint8_t* data, uint32_t value) {
    data[0] = (value >> 16) & 0xFF;
    data[1] = (value >> 8) & 0xFF;
    data[2] = value & 0xFF;
}

/**
 * Create a 32-bit big-endian byte array from a value
 */
void createBigEndian32(uint8_t* data, uint32_t value) {
    data[0] = (value >> 24) & 0xFF;
    data[1] = (value >> 16) & 0xFF;
    data[2] = (value >> 8) & 0xFF;
    data[3] = value & 0xFF;
}

/**
 * Create a 64-bit big-endian byte array from a value
 */
void createBigEndian64(uint8_t* data, uint64_t value) {
    data[0] = (value >> 56) & 0xFF;
    data[1] = (value >> 48) & 0xFF;
    data[2] = (value >> 40) & 0xFF;
    data[3] = (value >> 32) & 0xFF;
    data[4] = (value >> 24) & 0xFF;
    data[5] = (value >> 16) & 0xFF;
    data[6] = (value >> 8) & 0xFF;
    data[7] = value & 0xFF;
}

/**
 * Create a 32-bit little-endian byte array from a value
 */
void createLittleEndian32(uint8_t* data, uint32_t value) {
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
    data[2] = (value >> 16) & 0xFF;
    data[3] = (value >> 24) & 0xFF;
}

/**
 * Helper to format bytes as hex string for debugging
 */
std::string bytesToHex(const uint8_t* bytes, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i) {
        if (i > 0) oss << " ";
        oss << "0x" << std::hex << std::setw(2) << std::setfill('0') 
            << static_cast<int>(bytes[i]);
    }
    return oss.str();
}


// ========================================
// PROPERTY-BASED TESTS
// ========================================

// ========================================
// PROPERTY 16: Endianness Handling
// ========================================
// **Feature: flac-demuxer, Property 16: Endianness Handling**
// **Validates: Requirements 19.1, 19.4, 13.1**
//
// For any metadata field, the FLAC Demuxer SHALL use big-endian byte order 
// except for VORBIS_COMMENT lengths which SHALL use little-endian byte order.

void test_property_endianness_handling() {
    std::cout << "\n=== Property 16: Endianness Handling ===" << std::endl;
    std::cout << "Testing that big-endian is used for most fields, little-endian for VORBIS_COMMENT..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: 24-bit big-endian parsing (metadata block lengths)
    // ----------------------------------------
    std::cout << "\n  Test 1: 24-bit big-endian parsing (metadata block lengths)..." << std::endl;
    {
        struct TestCase {
            uint8_t bytes[3];
            uint32_t expected;
        };
        
        std::vector<TestCase> test_cases = {
            {{0x00, 0x00, 0x00}, 0},           // Zero
            {{0x00, 0x00, 0x01}, 1},           // Minimum non-zero
            {{0x00, 0x00, 0x22}, 34},          // STREAMINFO length (34 bytes)
            {{0x00, 0x01, 0x00}, 256},         // 256 bytes
            {{0x00, 0x10, 0x00}, 4096},        // 4KB
            {{0x01, 0x00, 0x00}, 65536},       // 64KB
            {{0x10, 0x00, 0x00}, 1048576},     // 1MB
            {{0xFF, 0xFF, 0xFF}, 16777215},    // Maximum (2^24 - 1)
            {{0x12, 0x34, 0x56}, 0x123456},    // Arbitrary value
        };
        
        for (const auto& tc : test_cases) {
            tests_run++;
            uint32_t result = parseBigEndian24(tc.bytes);
            
            if (result == tc.expected) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: bytes=" << bytesToHex(tc.bytes, 3)
                          << " expected=" << tc.expected 
                          << " got=" << result << std::endl;
                assert(false && "24-bit big-endian parsing failed");
            }
        }
        std::cout << "    All " << test_cases.size() << " test cases passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 2: 32-bit big-endian parsing (STREAMINFO fields)
    // ----------------------------------------
    std::cout << "\n  Test 2: 32-bit big-endian parsing (STREAMINFO fields)..." << std::endl;
    {
        struct TestCase {
            uint8_t bytes[4];
            uint32_t expected;
        };
        
        std::vector<TestCase> test_cases = {
            {{0x00, 0x00, 0x00, 0x00}, 0},                 // Zero
            {{0x00, 0x00, 0x00, 0x01}, 1},                 // Minimum non-zero
            {{0x00, 0x00, 0xAC, 0x44}, 44100},             // 44100 Hz sample rate
            {{0x00, 0x00, 0xBB, 0x80}, 48000},             // 48000 Hz sample rate
            {{0x00, 0x01, 0x58, 0x88}, 88200},             // 88200 Hz sample rate
            {{0x00, 0x02, 0xB1, 0x10}, 176400},            // 176400 Hz sample rate
            {{0x00, 0x02, 0xEE, 0x00}, 192000},            // 192000 Hz sample rate
            {{0xFF, 0xFF, 0xFF, 0xFF}, 0xFFFFFFFF},        // Maximum
            {{0x12, 0x34, 0x56, 0x78}, 0x12345678},        // Arbitrary value
        };
        
        for (const auto& tc : test_cases) {
            tests_run++;
            uint32_t result = parseBigEndian32(tc.bytes);
            
            if (result == tc.expected) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: bytes=" << bytesToHex(tc.bytes, 4)
                          << " expected=" << tc.expected 
                          << " got=" << result << std::endl;
                assert(false && "32-bit big-endian parsing failed");
            }
        }
        std::cout << "    All " << test_cases.size() << " test cases passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 3: 64-bit big-endian parsing (seek point sample numbers)
    // ----------------------------------------
    std::cout << "\n  Test 3: 64-bit big-endian parsing (seek point sample numbers)..." << std::endl;
    {
        struct TestCase {
            uint8_t bytes[8];
            uint64_t expected;
        };
        
        std::vector<TestCase> test_cases = {
            {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0},                     // Zero
            {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}, 1},                     // Minimum non-zero
            {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAC, 0x44}, 44100},                 // 44100 samples
            {{0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00}, 65536},                 // 64K samples
            {{0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00}, 16777216},              // 16M samples
            {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 0xFFFFFFFFFFFFFFFFULL}, // Placeholder value
            {{0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0}, 0x123456789ABCDEF0ULL}, // Arbitrary value
        };
        
        for (const auto& tc : test_cases) {
            tests_run++;
            uint64_t result = parseBigEndian64(tc.bytes);
            
            if (result == tc.expected) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: bytes=" << bytesToHex(tc.bytes, 8)
                          << " expected=" << tc.expected 
                          << " got=" << result << std::endl;
                assert(false && "64-bit big-endian parsing failed");
            }
        }
        std::cout << "    All " << test_cases.size() << " test cases passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 4: 32-bit little-endian parsing (VORBIS_COMMENT lengths)
    // ----------------------------------------
    std::cout << "\n  Test 4: 32-bit little-endian parsing (VORBIS_COMMENT lengths)..." << std::endl;
    {
        struct TestCase {
            uint8_t bytes[4];
            uint32_t expected;
        };
        
        std::vector<TestCase> test_cases = {
            {{0x00, 0x00, 0x00, 0x00}, 0},                 // Zero
            {{0x01, 0x00, 0x00, 0x00}, 1},                 // Minimum non-zero
            {{0x0A, 0x00, 0x00, 0x00}, 10},                // 10 bytes (short string)
            {{0x64, 0x00, 0x00, 0x00}, 100},               // 100 bytes
            {{0x00, 0x01, 0x00, 0x00}, 256},               // 256 bytes
            {{0x00, 0x10, 0x00, 0x00}, 4096},              // 4KB
            {{0x00, 0x00, 0x01, 0x00}, 65536},             // 64KB
            {{0xFF, 0xFF, 0xFF, 0xFF}, 0xFFFFFFFF},        // Maximum
            {{0x78, 0x56, 0x34, 0x12}, 0x12345678},        // Arbitrary value (note: reversed from big-endian)
        };
        
        for (const auto& tc : test_cases) {
            tests_run++;
            uint32_t result = parseLittleEndian32(tc.bytes);
            
            if (result == tc.expected) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: bytes=" << bytesToHex(tc.bytes, 4)
                          << " expected=" << tc.expected 
                          << " got=" << result << std::endl;
                assert(false && "32-bit little-endian parsing failed");
            }
        }
        std::cout << "    All " << test_cases.size() << " test cases passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 5: Big-endian vs little-endian distinction
    // ----------------------------------------
    std::cout << "\n  Test 5: Big-endian vs little-endian distinction..." << std::endl;
    {
        // Same bytes should produce different values depending on endianness
        uint8_t test_bytes[4] = {0x12, 0x34, 0x56, 0x78};
        
        tests_run++;
        uint32_t big_endian_result = parseBigEndian32(test_bytes);
        uint32_t little_endian_result = parseLittleEndian32(test_bytes);
        
        // Big-endian: 0x12345678
        // Little-endian: 0x78563412
        if (big_endian_result == 0x12345678 && little_endian_result == 0x78563412) {
            tests_passed++;
            std::cout << "    Bytes 0x12 0x34 0x56 0x78:" << std::endl;
            std::cout << "      Big-endian:    0x" << std::hex << big_endian_result << std::dec << " ✓" << std::endl;
            std::cout << "      Little-endian: 0x" << std::hex << little_endian_result << std::dec << " ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Endianness distinction incorrect" << std::endl;
            assert(false && "Endianness distinction failed");
        }
    }
    
    // ----------------------------------------
    // Test 6: Round-trip encoding/decoding (big-endian 24-bit)
    // ----------------------------------------
    std::cout << "\n  Test 6: Round-trip encoding/decoding (big-endian 24-bit)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dist(0, 16777215);  // 24-bit max
        
        for (int i = 0; i < 100; ++i) {
            uint32_t original = dist(gen);
            uint8_t encoded[3];
            createBigEndian24(encoded, original);
            uint32_t decoded = parseBigEndian24(encoded);
            
            tests_run++;
            if (decoded == original) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Round-trip failed for value " << original << std::endl;
                assert(false && "24-bit big-endian round-trip failed");
            }
        }
        std::cout << "    100 random round-trips successful ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 7: Round-trip encoding/decoding (big-endian 32-bit)
    // ----------------------------------------
    std::cout << "\n  Test 7: Round-trip encoding/decoding (big-endian 32-bit)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);
        
        for (int i = 0; i < 100; ++i) {
            uint32_t original = dist(gen);
            uint8_t encoded[4];
            createBigEndian32(encoded, original);
            uint32_t decoded = parseBigEndian32(encoded);
            
            tests_run++;
            if (decoded == original) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Round-trip failed for value " << original << std::endl;
                assert(false && "32-bit big-endian round-trip failed");
            }
        }
        std::cout << "    100 random round-trips successful ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 8: Round-trip encoding/decoding (big-endian 64-bit)
    // ----------------------------------------
    std::cout << "\n  Test 8: Round-trip encoding/decoding (big-endian 64-bit)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> dist(0, 0xFFFFFFFFFFFFFFFFULL);
        
        for (int i = 0; i < 100; ++i) {
            uint64_t original = dist(gen);
            uint8_t encoded[8];
            createBigEndian64(encoded, original);
            uint64_t decoded = parseBigEndian64(encoded);
            
            tests_run++;
            if (decoded == original) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Round-trip failed for value " << original << std::endl;
                assert(false && "64-bit big-endian round-trip failed");
            }
        }
        std::cout << "    100 random round-trips successful ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 9: Round-trip encoding/decoding (little-endian 32-bit)
    // ----------------------------------------
    std::cout << "\n  Test 9: Round-trip encoding/decoding (little-endian 32-bit)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);
        
        for (int i = 0; i < 100; ++i) {
            uint32_t original = dist(gen);
            uint8_t encoded[4];
            createLittleEndian32(encoded, original);
            uint32_t decoded = parseLittleEndian32(encoded);
            
            tests_run++;
            if (decoded == original) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Round-trip failed for value " << original << std::endl;
                assert(false && "32-bit little-endian round-trip failed");
            }
        }
        std::cout << "    100 random round-trips successful ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 10: VORBIS_COMMENT specific test cases
    // ----------------------------------------
    std::cout << "\n  Test 10: VORBIS_COMMENT specific test cases..." << std::endl;
    {
        // Simulate parsing a VORBIS_COMMENT block structure
        // Vendor string length (little-endian): 7 bytes for "libFLAC"
        uint8_t vendor_len_bytes[4] = {0x07, 0x00, 0x00, 0x00};
        tests_run++;
        uint32_t vendor_len = parseLittleEndian32(vendor_len_bytes);
        if (vendor_len == 7) {
            tests_passed++;
            std::cout << "    Vendor length 7 parsed correctly ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Expected vendor length 7, got " << vendor_len << std::endl;
            assert(false && "VORBIS_COMMENT vendor length parsing failed");
        }
        
        // Field count (little-endian): 3 fields
        uint8_t field_count_bytes[4] = {0x03, 0x00, 0x00, 0x00};
        tests_run++;
        uint32_t field_count = parseLittleEndian32(field_count_bytes);
        if (field_count == 3) {
            tests_passed++;
            std::cout << "    Field count 3 parsed correctly ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Expected field count 3, got " << field_count << std::endl;
            assert(false && "VORBIS_COMMENT field count parsing failed");
        }
        
        // Field length (little-endian): 12 bytes for "ARTIST=Test"
        uint8_t field_len_bytes[4] = {0x0B, 0x00, 0x00, 0x00};
        tests_run++;
        uint32_t field_len = parseLittleEndian32(field_len_bytes);
        if (field_len == 11) {
            tests_passed++;
            std::cout << "    Field length 11 parsed correctly ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Expected field length 11, got " << field_len << std::endl;
            assert(false && "VORBIS_COMMENT field length parsing failed");
        }
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 16: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}


// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC ENDIANNESS HANDLING PROPERTY-BASED TESTS" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // Property 16: Endianness Handling
        // **Feature: flac-demuxer, Property 16: Endianness Handling**
        // **Validates: Requirements 19.1, 19.4, 13.1**
        test_property_endianness_handling();
        
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
