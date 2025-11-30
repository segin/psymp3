/*
 * test_flac_metadata_block_header_properties.cpp - Property-based tests for FLAC metadata block header parsing
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
// STANDALONE METADATA BLOCK HEADER PARSING
// ========================================

/**
 * RFC 9639 Section 8.1: Metadata Block Header Structure
 * 
 * The metadata block header is 4 bytes:
 * - Byte 0, Bit 7: is_last flag (1 = last metadata block)
 * - Byte 0, Bits 0-6: block type (0-6 defined, 7-126 reserved, 127 forbidden)
 * - Bytes 1-3: 24-bit big-endian block length
 */

struct MetadataBlockHeader {
    bool is_last;
    uint8_t block_type;
    uint32_t block_length;
    bool is_valid;  // false if forbidden pattern detected
};

/**
 * Parses a 4-byte metadata block header per RFC 9639 Section 8.1
 * @param header Pointer to 4 bytes of header data
 * @return Parsed header structure with is_valid=false if forbidden pattern
 */
MetadataBlockHeader parseMetadataBlockHeader(const uint8_t* header) {
    MetadataBlockHeader result;
    result.is_valid = true;
    
    if (!header) {
        result.is_valid = false;
        result.is_last = false;
        result.block_type = 127;
        result.block_length = 0;
        return result;
    }
    
    // Requirement 2.2: Extract bit 7 as is_last flag
    result.is_last = (header[0] & 0x80) != 0;
    
    // Requirement 2.3: Extract bits 0-6 as block type
    result.block_type = header[0] & 0x7F;
    
    // Requirement 2.4, 18.1: Block type 127 is forbidden
    if (result.block_type == 127) {
        result.is_valid = false;
    }
    
    // Requirement 2.5: Extract bytes 1-3 as 24-bit big-endian block length
    result.block_length = (static_cast<uint32_t>(header[1]) << 16) |
                          (static_cast<uint32_t>(header[2]) << 8) |
                          static_cast<uint32_t>(header[3]);
    
    return result;
}

/**
 * Creates a 4-byte metadata block header from components
 */
void createMetadataBlockHeader(uint8_t* header, bool is_last, uint8_t block_type, uint32_t block_length) {
    // Byte 0: is_last (bit 7) | block_type (bits 0-6)
    header[0] = (is_last ? 0x80 : 0x00) | (block_type & 0x7F);
    
    // Bytes 1-3: 24-bit big-endian block length
    header[1] = (block_length >> 16) & 0xFF;
    header[2] = (block_length >> 8) & 0xFF;
    header[3] = block_length & 0xFF;
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
// PROPERTY 2: Metadata Block Header Bit Extraction
// ========================================
// **Feature: flac-demuxer, Property 2: Metadata Block Header Bit Extraction**
// **Validates: Requirements 2.2, 2.3**
//
// For any metadata block header byte, extracting bit 7 SHALL produce the 
// correct is_last flag, and extracting bits 0-6 SHALL produce the correct 
// block type value.

void test_property_metadata_block_header_bit_extraction() {
    std::cout << "\n=== Property 2: Metadata Block Header Bit Extraction ===" << std::endl;
    std::cout << "Testing that bit 7 extracts is_last flag and bits 0-6 extract block type..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: is_last flag extraction (bit 7)
    // ----------------------------------------
    std::cout << "\n  Test 1: is_last flag extraction..." << std::endl;
    {
        // Test all possible first bytes to verify is_last extraction
        for (int byte0 = 0; byte0 < 256; ++byte0) {
            uint8_t header[4] = {static_cast<uint8_t>(byte0), 0x00, 0x00, 0x22};
            tests_run++;
            
            MetadataBlockHeader result = parseMetadataBlockHeader(header);
            bool expected_is_last = (byte0 & 0x80) != 0;
            
            if (result.is_last == expected_is_last) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: byte0=0x" << std::hex << byte0 << std::dec
                          << " expected is_last=" << expected_is_last 
                          << " got " << result.is_last << std::endl;
                assert(false && "is_last flag extraction failed");
            }
        }
        std::cout << "    All 256 byte values correctly extract is_last flag ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 2: block_type extraction (bits 0-6)
    // ----------------------------------------
    std::cout << "\n  Test 2: block_type extraction..." << std::endl;
    {
        // Test all possible first bytes to verify block_type extraction
        for (int byte0 = 0; byte0 < 256; ++byte0) {
            uint8_t header[4] = {static_cast<uint8_t>(byte0), 0x00, 0x00, 0x22};
            tests_run++;
            
            MetadataBlockHeader result = parseMetadataBlockHeader(header);
            uint8_t expected_type = byte0 & 0x7F;
            
            if (result.block_type == expected_type) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: byte0=0x" << std::hex << byte0 << std::dec
                          << " expected type=" << static_cast<int>(expected_type) 
                          << " got " << static_cast<int>(result.block_type) << std::endl;
                assert(false && "block_type extraction failed");
            }
        }
        std::cout << "    All 256 byte values correctly extract block_type ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 3: Combined is_last and block_type extraction
    // ----------------------------------------
    std::cout << "\n  Test 3: Combined extraction verification..." << std::endl;
    {
        // Test specific combinations
        struct TestCase {
            uint8_t byte0;
            bool expected_is_last;
            uint8_t expected_type;
        };
        
        std::vector<TestCase> test_cases = {
            {0x00, false, 0},   // STREAMINFO, not last
            {0x80, true, 0},    // STREAMINFO, last
            {0x01, false, 1},   // PADDING, not last
            {0x81, true, 1},    // PADDING, last
            {0x02, false, 2},   // APPLICATION, not last
            {0x82, true, 2},    // APPLICATION, last
            {0x03, false, 3},   // SEEKTABLE, not last
            {0x83, true, 3},    // SEEKTABLE, last
            {0x04, false, 4},   // VORBIS_COMMENT, not last
            {0x84, true, 4},    // VORBIS_COMMENT, last
            {0x05, false, 5},   // CUESHEET, not last
            {0x85, true, 5},    // CUESHEET, last
            {0x06, false, 6},   // PICTURE, not last
            {0x86, true, 6},    // PICTURE, last
            {0x7E, false, 126}, // Reserved type 126, not last
            {0xFE, true, 126},  // Reserved type 126, last
        };
        
        for (const auto& tc : test_cases) {
            uint8_t header[4] = {tc.byte0, 0x00, 0x00, 0x22};
            tests_run++;
            
            MetadataBlockHeader result = parseMetadataBlockHeader(header);
            
            if (result.is_last == tc.expected_is_last && result.block_type == tc.expected_type) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: byte0=0x" << std::hex << static_cast<int>(tc.byte0) << std::dec
                          << " expected is_last=" << tc.expected_is_last << " type=" << static_cast<int>(tc.expected_type)
                          << " got is_last=" << result.is_last << " type=" << static_cast<int>(result.block_type) << std::endl;
                assert(false && "Combined extraction failed");
            }
        }
        std::cout << "    All " << test_cases.size() << " specific combinations verified ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 4: Block length extraction (24-bit big-endian)
    // ----------------------------------------
    std::cout << "\n  Test 4: Block length extraction (24-bit big-endian)..." << std::endl;
    {
        struct LengthTestCase {
            uint8_t byte1, byte2, byte3;
            uint32_t expected_length;
        };
        
        std::vector<LengthTestCase> length_tests = {
            {0x00, 0x00, 0x00, 0},           // Zero length
            {0x00, 0x00, 0x01, 1},           // Minimum non-zero
            {0x00, 0x00, 0x22, 34},          // STREAMINFO length (34 bytes)
            {0x00, 0x01, 0x00, 256},         // 256 bytes
            {0x00, 0x10, 0x00, 4096},        // 4KB
            {0x01, 0x00, 0x00, 65536},       // 64KB
            {0x10, 0x00, 0x00, 1048576},     // 1MB
            {0xFF, 0xFF, 0xFF, 16777215},    // Maximum (2^24 - 1)
            {0x12, 0x34, 0x56, 0x123456},    // Arbitrary value
        };
        
        for (const auto& tc : length_tests) {
            uint8_t header[4] = {0x00, tc.byte1, tc.byte2, tc.byte3};
            tests_run++;
            
            MetadataBlockHeader result = parseMetadataBlockHeader(header);
            
            if (result.block_length == tc.expected_length) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: bytes=" << bytesToHex(&header[1], 3)
                          << " expected length=" << tc.expected_length 
                          << " got " << result.block_length << std::endl;
                assert(false && "Block length extraction failed");
            }
        }
        std::cout << "    All " << length_tests.size() << " length values correctly extracted ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 5: Random header generation and round-trip
    // ----------------------------------------
    std::cout << "\n  Test 5: Random header round-trip (100 iterations)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> bool_dist(0, 1);
        std::uniform_int_distribution<> type_dist(0, 126);  // Exclude 127 (forbidden)
        std::uniform_int_distribution<> length_dist(0, 16777215);  // 24-bit max
        
        for (int i = 0; i < 100; ++i) {
            bool is_last = bool_dist(gen) == 1;
            uint8_t block_type = static_cast<uint8_t>(type_dist(gen));
            uint32_t block_length = static_cast<uint32_t>(length_dist(gen));
            
            // Create header
            uint8_t header[4];
            createMetadataBlockHeader(header, is_last, block_type, block_length);
            
            // Parse it back
            MetadataBlockHeader result = parseMetadataBlockHeader(header);
            
            tests_run++;
            
            if (result.is_last == is_last && 
                result.block_type == block_type && 
                result.block_length == block_length) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Round-trip mismatch" << std::endl;
                std::cerr << "      Input: is_last=" << is_last << " type=" << static_cast<int>(block_type) 
                          << " length=" << block_length << std::endl;
                std::cerr << "      Output: is_last=" << result.is_last << " type=" << static_cast<int>(result.block_type) 
                          << " length=" << result.block_length << std::endl;
                assert(false && "Round-trip failed");
            }
        }
        std::cout << "    100 random round-trips successful ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 2: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}


// ========================================
// PROPERTY 3: Forbidden Block Type Detection
// ========================================
// **Feature: flac-demuxer, Property 3: Forbidden Block Type Detection**
// **Validates: Requirements 2.4, 18.1**
//
// For any metadata block with type 127, the FLAC Demuxer SHALL reject 
// the stream as a forbidden pattern.

void test_property_forbidden_block_type_detection() {
    std::cout << "\n=== Property 3: Forbidden Block Type Detection ===" << std::endl;
    std::cout << "Testing that block type 127 is always rejected as forbidden..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: Block type 127 must be rejected (is_last = false)
    // ----------------------------------------
    std::cout << "\n  Test 1: Block type 127 with is_last=false..." << std::endl;
    {
        uint8_t header[4] = {0x7F, 0x00, 0x00, 0x22};  // type=127, is_last=false
        tests_run++;
        
        MetadataBlockHeader result = parseMetadataBlockHeader(header);
        
        if (!result.is_valid) {
            std::cout << "    Block type 127 (is_last=false) correctly rejected ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Block type 127 was accepted!" << std::endl;
            assert(false && "Block type 127 should be rejected");
        }
    }
    
    // ----------------------------------------
    // Test 2: Block type 127 must be rejected (is_last = true)
    // ----------------------------------------
    std::cout << "\n  Test 2: Block type 127 with is_last=true..." << std::endl;
    {
        uint8_t header[4] = {0xFF, 0x00, 0x00, 0x22};  // type=127, is_last=true
        tests_run++;
        
        MetadataBlockHeader result = parseMetadataBlockHeader(header);
        
        if (!result.is_valid) {
            std::cout << "    Block type 127 (is_last=true) correctly rejected ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Block type 127 was accepted!" << std::endl;
            assert(false && "Block type 127 should be rejected");
        }
    }
    
    // ----------------------------------------
    // Test 3: Block type 127 with all possible lengths
    // ----------------------------------------
    std::cout << "\n  Test 3: Block type 127 with various lengths..." << std::endl;
    {
        std::vector<uint32_t> test_lengths = {0, 1, 34, 256, 4096, 65536, 16777215};
        
        for (uint32_t length : test_lengths) {
            uint8_t header[4];
            header[0] = 0x7F;  // type=127, is_last=false
            header[1] = (length >> 16) & 0xFF;
            header[2] = (length >> 8) & 0xFF;
            header[3] = length & 0xFF;
            
            tests_run++;
            
            MetadataBlockHeader result = parseMetadataBlockHeader(header);
            
            if (!result.is_valid) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Block type 127 with length " << length << " was accepted!" << std::endl;
                assert(false && "Block type 127 should be rejected regardless of length");
            }
        }
        std::cout << "    Block type 127 rejected for all " << test_lengths.size() << " length values ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 4: Valid block types (0-6) must be accepted
    // ----------------------------------------
    std::cout << "\n  Test 4: Valid block types (0-6) acceptance..." << std::endl;
    {
        const char* type_names[] = {
            "STREAMINFO", "PADDING", "APPLICATION", "SEEKTABLE",
            "VORBIS_COMMENT", "CUESHEET", "PICTURE"
        };
        
        for (int type = 0; type <= 6; ++type) {
            uint8_t header[4] = {static_cast<uint8_t>(type), 0x00, 0x00, 0x22};
            tests_run++;
            
            MetadataBlockHeader result = parseMetadataBlockHeader(header);
            
            if (result.is_valid && result.block_type == type) {
                std::cout << "    Type " << type << " (" << type_names[type] << ") accepted ✓" << std::endl;
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Valid type " << type << " was rejected!" << std::endl;
                assert(false && "Valid block type should be accepted");
            }
        }
    }
    
    // ----------------------------------------
    // Test 5: Reserved block types (7-126) must be accepted (but skipped by demuxer)
    // ----------------------------------------
    std::cout << "\n  Test 5: Reserved block types (7-126) acceptance..." << std::endl;
    {
        int reserved_accepted = 0;
        
        for (int type = 7; type <= 126; ++type) {
            uint8_t header[4] = {static_cast<uint8_t>(type), 0x00, 0x00, 0x22};
            tests_run++;
            
            MetadataBlockHeader result = parseMetadataBlockHeader(header);
            
            // Reserved types should be parsed successfully (is_valid=true)
            // The demuxer will skip them, but parsing should succeed
            if (result.is_valid && result.block_type == type) {
                tests_passed++;
                reserved_accepted++;
            } else {
                std::cerr << "    FAILED: Reserved type " << type << " was rejected!" << std::endl;
                assert(false && "Reserved block type should be parseable");
            }
        }
        std::cout << "    All " << reserved_accepted << " reserved types (7-126) accepted ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 6: Only type 127 is forbidden (boundary test)
    // ----------------------------------------
    std::cout << "\n  Test 6: Boundary test - only type 127 is forbidden..." << std::endl;
    {
        // Type 126 should be valid (reserved but not forbidden)
        uint8_t header126[4] = {0x7E, 0x00, 0x00, 0x22};
        tests_run++;
        
        MetadataBlockHeader result126 = parseMetadataBlockHeader(header126);
        if (result126.is_valid) {
            std::cout << "    Type 126 (reserved) accepted ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Type 126 should be valid!" << std::endl;
            assert(false && "Type 126 should be valid");
        }
        
        // Type 127 should be invalid (forbidden)
        uint8_t header127[4] = {0x7F, 0x00, 0x00, 0x22};
        tests_run++;
        
        MetadataBlockHeader result127 = parseMetadataBlockHeader(header127);
        if (!result127.is_valid) {
            std::cout << "    Type 127 (forbidden) rejected ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Type 127 should be forbidden!" << std::endl;
            assert(false && "Type 127 should be forbidden");
        }
    }
    
    // ----------------------------------------
    // Test 7: Random valid types should all be accepted
    // ----------------------------------------
    std::cout << "\n  Test 7: Random valid types (100 iterations)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> type_dist(0, 126);  // All valid types
        std::uniform_int_distribution<> bool_dist(0, 1);
        std::uniform_int_distribution<> length_dist(0, 16777215);
        
        for (int i = 0; i < 100; ++i) {
            uint8_t block_type = static_cast<uint8_t>(type_dist(gen));
            bool is_last = bool_dist(gen) == 1;
            uint32_t length = static_cast<uint32_t>(length_dist(gen));
            
            uint8_t header[4];
            createMetadataBlockHeader(header, is_last, block_type, length);
            
            tests_run++;
            
            MetadataBlockHeader result = parseMetadataBlockHeader(header);
            
            if (result.is_valid) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Valid type " << static_cast<int>(block_type) << " was rejected!" << std::endl;
                assert(false && "Valid block type should be accepted");
            }
        }
        std::cout << "    100 random valid types all accepted ✓" << std::endl;
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
    std::cout << "FLAC METADATA BLOCK HEADER PROPERTY-BASED TESTS" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // Property 2: Metadata Block Header Bit Extraction
        // **Feature: flac-demuxer, Property 2: Metadata Block Header Bit Extraction**
        // **Validates: Requirements 2.2, 2.3**
        test_property_metadata_block_header_bit_extraction();
        
        // Property 3: Forbidden Block Type Detection
        // **Feature: flac-demuxer, Property 3: Forbidden Block Type Detection**
        // **Validates: Requirements 2.4, 18.1**
        test_property_forbidden_block_type_detection();
        
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
