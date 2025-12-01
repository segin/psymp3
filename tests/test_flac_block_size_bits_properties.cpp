/*
 * test_flac_block_size_bits_properties.cpp - Property-based tests for FLAC block size bits parsing
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <cstdint>
#include <random>
#include <sstream>
#include <iomanip>

// ========================================
// STANDALONE BLOCK SIZE BITS PARSER
// ========================================

/**
 * RFC 9639 Section 9.1.1, Table 14: Block Size Encoding
 * 
 * Block size bits (4 bits from frame byte 2, bits 4-7):
 *   0b0000: Reserved (reject)
 *   0b0001: 192 samples
 *   0b0010: 576 samples
 *   0b0011: 1152 samples
 *   0b0100: 2304 samples
 *   0b0101: 4608 samples
 *   0b0110: 8-bit uncommon block size minus 1 follows
 *   0b0111: 16-bit uncommon block size minus 1 follows
 *   0b1000: 256 samples
 *   0b1001: 512 samples
 *   0b1010: 1024 samples
 *   0b1011: 2048 samples
 *   0b1100: 4096 samples
 *   0b1101: 8192 samples
 *   0b1110: 16384 samples
 *   0b1111: 32768 samples
 */

/**
 * Result of block size bits parsing
 */
struct BlockSizeResult {
    bool valid = false;           ///< True if parsing succeeded
    uint32_t block_size = 0;      ///< Decoded block size in samples
    bool is_reserved = false;     ///< True if reserved pattern detected
    bool is_forbidden = false;    ///< True if forbidden pattern detected
    std::string error_message;    ///< Error description if invalid
};

/**
 * Parse block size bits per RFC 9639 Table 14
 * 
 * @param bits The 4-bit block size code (bits 4-7 of frame byte 2)
 * @param uncommon_buffer Optional buffer for uncommon block size bytes
 * @param uncommon_size Size of uncommon buffer
 * @return BlockSizeResult with parsing results
 */
BlockSizeResult parseBlockSizeBits(uint8_t bits, const uint8_t* uncommon_buffer = nullptr, 
                                    size_t uncommon_size = 0) {
    BlockSizeResult result;
    
    // Ensure only 4 bits are used
    bits &= 0x0F;
    
    // Requirement 5.2: Reserved block size pattern 0b0000
    // RFC 9639 Table 1: Block size bits 0b0000 is reserved
    if (bits == 0x00) {
        result.valid = false;
        result.is_reserved = true;
        result.error_message = "Reserved block size pattern 0b0000 (Requirement 5.2)";
        return result;
    }
    
    switch (bits) {
        // Requirement 5.3: 0b0001 = 192 samples
        case 0x01:
            result.valid = true;
            result.block_size = 192;
            break;
            
        // Requirement 5.4: 0b0010 = 576 samples
        case 0x02:
            result.valid = true;
            result.block_size = 576;
            break;
            
        // Requirement 5.5: 0b0011 = 1152 samples
        case 0x03:
            result.valid = true;
            result.block_size = 1152;
            break;
            
        // Requirement 5.6: 0b0100 = 2304 samples
        case 0x04:
            result.valid = true;
            result.block_size = 2304;
            break;
            
        // Requirement 5.7: 0b0101 = 4608 samples
        case 0x05:
            result.valid = true;
            result.block_size = 4608;
            break;
            
        // Requirement 5.8: 0b0110 = 8-bit uncommon block size minus 1 follows
        case 0x06:
            if (uncommon_buffer && uncommon_size >= 1) {
                // RFC 9639: stored value is (block_size - 1)
                result.block_size = static_cast<uint32_t>(uncommon_buffer[0]) + 1;
                result.valid = true;
            } else {
                result.valid = false;
                result.error_message = "Missing 8-bit uncommon block size data";
            }
            break;
            
        // Requirement 5.9: 0b0111 = 16-bit uncommon block size minus 1 follows
        case 0x07:
            if (uncommon_buffer && uncommon_size >= 2) {
                // RFC 9639: 16-bit big-endian, stored value is (block_size - 1)
                uint16_t uncommon_value = (static_cast<uint16_t>(uncommon_buffer[0]) << 8) |
                                           static_cast<uint16_t>(uncommon_buffer[1]);
                uint32_t decoded_size = static_cast<uint32_t>(uncommon_value) + 1;
                
                // Requirement 5.18: Reject forbidden uncommon block size 65536
                // RFC 9639 Table 1: uncommon block size 65536 is forbidden
                if (decoded_size == 65536) {
                    result.valid = false;
                    result.is_forbidden = true;
                    result.error_message = "Forbidden uncommon block size 65536 (Requirement 5.18)";
                } else {
                    result.block_size = decoded_size;
                    result.valid = true;
                }
            } else {
                result.valid = false;
                result.error_message = "Missing 16-bit uncommon block size data";
            }
            break;
            
        // Requirement 5.10: 0b1000 = 256 samples
        case 0x08:
            result.valid = true;
            result.block_size = 256;
            break;
            
        // Requirement 5.11: 0b1001 = 512 samples
        case 0x09:
            result.valid = true;
            result.block_size = 512;
            break;
            
        // Requirement 5.12: 0b1010 = 1024 samples
        case 0x0A:
            result.valid = true;
            result.block_size = 1024;
            break;
            
        // Requirement 5.13: 0b1011 = 2048 samples
        case 0x0B:
            result.valid = true;
            result.block_size = 2048;
            break;
            
        // Requirement 5.14: 0b1100 = 4096 samples
        case 0x0C:
            result.valid = true;
            result.block_size = 4096;
            break;
            
        // Requirement 5.15: 0b1101 = 8192 samples
        case 0x0D:
            result.valid = true;
            result.block_size = 8192;
            break;
            
        // Requirement 5.16: 0b1110 = 16384 samples
        case 0x0E:
            result.valid = true;
            result.block_size = 16384;
            break;
            
        // Requirement 5.17: 0b1111 = 32768 samples
        case 0x0F:
            result.valid = true;
            result.block_size = 32768;
            break;
            
        default:
            // Should never reach here
            result.valid = false;
            result.error_message = "Unexpected block size bits value";
            break;
    }
    
    return result;
}

/**
 * Helper to format bits as binary string
 */
std::string bitsToBinary(uint8_t bits) {
    std::ostringstream oss;
    oss << "0b";
    for (int i = 3; i >= 0; --i) {
        oss << ((bits >> i) & 1);
    }
    return oss.str();
}

// ========================================
// PROPERTY-BASED TESTS
// ========================================

// ========================================
// PROPERTY 8: Reserved Block Size Pattern Detection
// ========================================
// **Feature: flac-demuxer, Property 8: Reserved Block Size Pattern Detection**
// **Validates: Requirements 5.2**
//
// For any frame header with block size bits equal to 0b0000, the FLAC Demuxer 
// SHALL reject as a reserved pattern.

void test_property_reserved_block_size_pattern() {
    std::cout << "\n=== Property 8: Reserved Block Size Pattern Detection ===" << std::endl;
    std::cout << "Testing that block size bits 0b0000 are rejected as reserved..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: Reserved pattern 0b0000 must be rejected
    // ----------------------------------------
    std::cout << "\n  Test 1: Reserved pattern 0b0000 rejection..." << std::endl;
    {
        tests_run++;
        
        BlockSizeResult result = parseBlockSizeBits(0x00);
        
        if (!result.valid && result.is_reserved) {
            tests_passed++;
            std::cout << "    Block size bits 0b0000 rejected as reserved ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Block size bits 0b0000 was not rejected!" << std::endl;
            std::cerr << "    valid=" << result.valid << ", is_reserved=" << result.is_reserved << std::endl;
            assert(false && "Reserved pattern 0b0000 should be rejected");
        }
    }
    
    // ----------------------------------------
    // Test 2: All valid patterns (0b0001-0b1111) must be accepted
    // ----------------------------------------
    std::cout << "\n  Test 2: All valid patterns (0b0001-0b1111) acceptance..." << std::endl;
    {
        // Expected block sizes for each pattern (excluding uncommon patterns 0x06, 0x07)
        struct PatternExpectation {
            uint8_t bits;
            uint32_t expected_size;
            bool needs_uncommon_data;
        };
        
        std::vector<PatternExpectation> patterns = {
            {0x01, 192, false},
            {0x02, 576, false},
            {0x03, 1152, false},
            {0x04, 2304, false},
            {0x05, 4608, false},
            // 0x06 and 0x07 need uncommon data - tested separately
            {0x08, 256, false},
            {0x09, 512, false},
            {0x0A, 1024, false},
            {0x0B, 2048, false},
            {0x0C, 4096, false},
            {0x0D, 8192, false},
            {0x0E, 16384, false},
            {0x0F, 32768, false},
        };
        
        for (const auto& pattern : patterns) {
            tests_run++;
            
            BlockSizeResult result = parseBlockSizeBits(pattern.bits);
            
            if (result.valid && result.block_size == pattern.expected_size && !result.is_reserved) {
                tests_passed++;
                std::cout << "    " << bitsToBinary(pattern.bits) << " -> " 
                          << pattern.expected_size << " samples ✓" << std::endl;
            } else {
                std::cerr << "    FAILED: " << bitsToBinary(pattern.bits) << std::endl;
                std::cerr << "    Expected: valid=true, size=" << pattern.expected_size << std::endl;
                std::cerr << "    Got: valid=" << result.valid << ", size=" << result.block_size 
                          << ", is_reserved=" << result.is_reserved << std::endl;
                assert(false && "Valid pattern should be accepted");
            }
        }
    }
    
    // ----------------------------------------
    // Test 3: Uncommon 8-bit block size (0b0110)
    // ----------------------------------------
    std::cout << "\n  Test 3: Uncommon 8-bit block size (0b0110)..." << std::endl;
    {
        // Test various 8-bit uncommon values
        std::vector<std::pair<uint8_t, uint32_t>> test_cases = {
            {0, 1},      // Minimum: 0 + 1 = 1
            {1, 2},
            {127, 128},
            {254, 255},
            {255, 256},  // Maximum: 255 + 1 = 256
        };
        
        for (const auto& tc : test_cases) {
            tests_run++;
            
            uint8_t uncommon_data[1] = {tc.first};
            BlockSizeResult result = parseBlockSizeBits(0x06, uncommon_data, 1);
            
            if (result.valid && result.block_size == tc.second) {
                tests_passed++;
                std::cout << "    8-bit uncommon value " << static_cast<int>(tc.first) 
                          << " -> " << tc.second << " samples ✓" << std::endl;
            } else {
                std::cerr << "    FAILED: 8-bit uncommon value " << static_cast<int>(tc.first) << std::endl;
                std::cerr << "    Expected: " << tc.second << ", Got: " << result.block_size << std::endl;
                assert(false && "8-bit uncommon block size should be parsed correctly");
            }
        }
    }
    
    // ----------------------------------------
    // Test 4: Uncommon 16-bit block size (0b0111)
    // ----------------------------------------
    std::cout << "\n  Test 4: Uncommon 16-bit block size (0b0111)..." << std::endl;
    {
        // Test various 16-bit uncommon values (big-endian)
        struct TestCase {
            uint8_t high_byte;
            uint8_t low_byte;
            uint32_t expected_size;
        };
        
        std::vector<TestCase> test_cases = {
            {0x00, 0x00, 1},       // Minimum: 0 + 1 = 1
            {0x00, 0x01, 2},
            {0x00, 0xFF, 256},
            {0x01, 0x00, 257},
            {0x0F, 0xFF, 4096},
            {0x7F, 0xFF, 32768},
            {0xFF, 0xFD, 65534},   // Maximum valid: 65533 + 1 = 65534
            {0xFF, 0xFE, 65535},   // 65534 + 1 = 65535 (valid)
            // 0xFFFF (65535 + 1 = 65536) is forbidden - tested separately
        };
        
        for (const auto& tc : test_cases) {
            tests_run++;
            
            uint8_t uncommon_data[2] = {tc.high_byte, tc.low_byte};
            BlockSizeResult result = parseBlockSizeBits(0x07, uncommon_data, 2);
            
            if (result.valid && result.block_size == tc.expected_size) {
                tests_passed++;
                std::cout << "    16-bit uncommon 0x" << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<int>(tc.high_byte) << std::setw(2) << static_cast<int>(tc.low_byte)
                          << std::dec << " -> " << tc.expected_size << " samples ✓" << std::endl;
            } else {
                std::cerr << "    FAILED: 16-bit uncommon 0x" << std::hex 
                          << static_cast<int>(tc.high_byte) << static_cast<int>(tc.low_byte) << std::dec << std::endl;
                std::cerr << "    Expected: " << tc.expected_size << ", Got: " << result.block_size << std::endl;
                assert(false && "16-bit uncommon block size should be parsed correctly");
            }
        }
    }
    
    // ----------------------------------------
    // Test 5: Random valid patterns (100 iterations)
    // ----------------------------------------
    std::cout << "\n  Test 5: Random valid patterns (100 iterations)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> bits_dist(1, 15);  // 0b0001 to 0b1111
        
        int random_passed = 0;
        
        for (int i = 0; i < 100; ++i) {
            uint8_t bits = static_cast<uint8_t>(bits_dist(gen));
            
            tests_run++;
            
            // For uncommon patterns, provide appropriate data
            uint8_t uncommon_data[2] = {0x00, 0x10};  // Safe value (17 samples)
            const uint8_t* data_ptr = nullptr;
            size_t data_size = 0;
            
            if (bits == 0x06) {
                data_ptr = uncommon_data;
                data_size = 1;
            } else if (bits == 0x07) {
                data_ptr = uncommon_data;
                data_size = 2;
            }
            
            BlockSizeResult result = parseBlockSizeBits(bits, data_ptr, data_size);
            
            if (result.valid && !result.is_reserved && result.block_size > 0) {
                tests_passed++;
                random_passed++;
            } else {
                std::cerr << "    FAILED iteration " << i << ": bits=" << bitsToBinary(bits) << std::endl;
                assert(false && "Valid pattern should be accepted");
            }
        }
        std::cout << "    " << random_passed << "/100 random valid patterns passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 8: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}



// ========================================
// PROPERTY 9: Forbidden Block Size Detection
// ========================================
// **Feature: flac-demuxer, Property 9: Forbidden Block Size Detection**
// **Validates: Requirements 5.18**
//
// For any frame header with uncommon block size equal to 65536, the FLAC Demuxer 
// SHALL reject as a forbidden pattern.

void test_property_forbidden_block_size() {
    std::cout << "\n=== Property 9: Forbidden Block Size Detection ===" << std::endl;
    std::cout << "Testing that uncommon block size 65536 is rejected as forbidden..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: Forbidden uncommon block size 65536 must be rejected
    // ----------------------------------------
    std::cout << "\n  Test 1: Forbidden uncommon block size 65536 rejection..." << std::endl;
    {
        tests_run++;
        
        // 65536 = 65535 + 1, so stored value is 65535 = 0xFFFF
        uint8_t uncommon_data[2] = {0xFF, 0xFF};
        BlockSizeResult result = parseBlockSizeBits(0x07, uncommon_data, 2);
        
        if (!result.valid && result.is_forbidden) {
            tests_passed++;
            std::cout << "    Uncommon block size 65536 (0xFFFF + 1) rejected as forbidden ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Uncommon block size 65536 was not rejected!" << std::endl;
            std::cerr << "    valid=" << result.valid << ", is_forbidden=" << result.is_forbidden << std::endl;
            std::cerr << "    block_size=" << result.block_size << std::endl;
            assert(false && "Forbidden block size 65536 should be rejected");
        }
    }
    
    // ----------------------------------------
    // Test 2: Block size 65535 (just below forbidden) must be accepted
    // ----------------------------------------
    std::cout << "\n  Test 2: Block size 65535 (just below forbidden) acceptance..." << std::endl;
    {
        tests_run++;
        
        // 65535 = 65534 + 1, so stored value is 65534 = 0xFFFE
        uint8_t uncommon_data[2] = {0xFF, 0xFE};
        BlockSizeResult result = parseBlockSizeBits(0x07, uncommon_data, 2);
        
        if (result.valid && result.block_size == 65535 && !result.is_forbidden) {
            tests_passed++;
            std::cout << "    Block size 65535 (0xFFFE + 1) accepted ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Block size 65535 was rejected!" << std::endl;
            std::cerr << "    valid=" << result.valid << ", block_size=" << result.block_size << std::endl;
            assert(false && "Block size 65535 should be accepted");
        }
    }
    
    // ----------------------------------------
    // Test 3: Block size 65534 must be accepted
    // ----------------------------------------
    std::cout << "\n  Test 3: Block size 65534 acceptance..." << std::endl;
    {
        tests_run++;
        
        // 65534 = 65533 + 1, so stored value is 65533 = 0xFFFD
        uint8_t uncommon_data[2] = {0xFF, 0xFD};
        BlockSizeResult result = parseBlockSizeBits(0x07, uncommon_data, 2);
        
        if (result.valid && result.block_size == 65534 && !result.is_forbidden) {
            tests_passed++;
            std::cout << "    Block size 65534 (0xFFFD + 1) accepted ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Block size 65534 was rejected!" << std::endl;
            assert(false && "Block size 65534 should be accepted");
        }
    }
    
    // ----------------------------------------
    // Test 4: All standard block sizes (non-uncommon) must be accepted
    // ----------------------------------------
    std::cout << "\n  Test 4: All standard block sizes acceptance..." << std::endl;
    {
        // Standard block sizes from RFC 9639 Table 14
        std::vector<std::pair<uint8_t, uint32_t>> standard_sizes = {
            {0x01, 192},
            {0x02, 576},
            {0x03, 1152},
            {0x04, 2304},
            {0x05, 4608},
            {0x08, 256},
            {0x09, 512},
            {0x0A, 1024},
            {0x0B, 2048},
            {0x0C, 4096},
            {0x0D, 8192},
            {0x0E, 16384},
            {0x0F, 32768},
        };
        
        for (const auto& ss : standard_sizes) {
            tests_run++;
            
            BlockSizeResult result = parseBlockSizeBits(ss.first);
            
            if (result.valid && result.block_size == ss.second && !result.is_forbidden) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Standard block size " << ss.second << " was rejected!" << std::endl;
                assert(false && "Standard block sizes should be accepted");
            }
        }
        std::cout << "    All 13 standard block sizes accepted ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 5: 8-bit uncommon block sizes (1-256) must be accepted
    // ----------------------------------------
    std::cout << "\n  Test 5: 8-bit uncommon block sizes (1-256) acceptance..." << std::endl;
    {
        // Test boundary values for 8-bit uncommon
        std::vector<std::pair<uint8_t, uint32_t>> test_cases = {
            {0, 1},      // Minimum
            {127, 128},  // Middle
            {255, 256},  // Maximum
        };
        
        for (const auto& tc : test_cases) {
            tests_run++;
            
            uint8_t uncommon_data[1] = {tc.first};
            BlockSizeResult result = parseBlockSizeBits(0x06, uncommon_data, 1);
            
            if (result.valid && result.block_size == tc.second && !result.is_forbidden) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: 8-bit uncommon " << tc.second << " was rejected!" << std::endl;
                assert(false && "8-bit uncommon block sizes should be accepted");
            }
        }
        std::cout << "    8-bit uncommon block sizes (1-256) accepted ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 6: Random 16-bit uncommon block sizes (excluding 65536)
    // ----------------------------------------
    std::cout << "\n  Test 6: Random 16-bit uncommon block sizes (100 iterations)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        // Generate values 0-65534 (which decode to 1-65535, excluding forbidden 65536)
        std::uniform_int_distribution<> value_dist(0, 65534);
        
        int random_passed = 0;
        
        for (int i = 0; i < 100; ++i) {
            uint16_t stored_value = static_cast<uint16_t>(value_dist(gen));
            uint32_t expected_size = static_cast<uint32_t>(stored_value) + 1;
            
            tests_run++;
            
            uint8_t uncommon_data[2] = {
                static_cast<uint8_t>(stored_value >> 8),
                static_cast<uint8_t>(stored_value & 0xFF)
            };
            
            BlockSizeResult result = parseBlockSizeBits(0x07, uncommon_data, 2);
            
            if (result.valid && result.block_size == expected_size && !result.is_forbidden) {
                tests_passed++;
                random_passed++;
            } else {
                std::cerr << "    FAILED iteration " << i << ": stored=0x" << std::hex << stored_value 
                          << std::dec << ", expected=" << expected_size << std::endl;
                assert(false && "Valid 16-bit uncommon block size should be accepted");
            }
        }
        std::cout << "    " << random_passed << "/100 random 16-bit uncommon sizes passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 7: Only 65536 is forbidden (boundary test)
    // ----------------------------------------
    std::cout << "\n  Test 7: Only 65536 is forbidden (boundary verification)..." << std::endl;
    {
        // Test values around the forbidden boundary
        struct BoundaryTest {
            uint16_t stored_value;
            uint32_t decoded_size;
            bool should_be_forbidden;
        };
        
        std::vector<BoundaryTest> boundary_tests = {
            {0xFFFC, 65533, false},  // 65533 - valid
            {0xFFFD, 65534, false},  // 65534 - valid
            {0xFFFE, 65535, false},  // 65535 - valid (maximum valid)
            {0xFFFF, 65536, true},   // 65536 - FORBIDDEN
        };
        
        for (const auto& bt : boundary_tests) {
            tests_run++;
            
            uint8_t uncommon_data[2] = {
                static_cast<uint8_t>(bt.stored_value >> 8),
                static_cast<uint8_t>(bt.stored_value & 0xFF)
            };
            
            BlockSizeResult result = parseBlockSizeBits(0x07, uncommon_data, 2);
            
            if (bt.should_be_forbidden) {
                if (!result.valid && result.is_forbidden) {
                    tests_passed++;
                    std::cout << "    Block size " << bt.decoded_size << " correctly rejected as forbidden ✓" << std::endl;
                } else {
                    std::cerr << "    FAILED: Block size " << bt.decoded_size << " should be forbidden!" << std::endl;
                    assert(false && "65536 should be forbidden");
                }
            } else {
                if (result.valid && result.block_size == bt.decoded_size && !result.is_forbidden) {
                    tests_passed++;
                    std::cout << "    Block size " << bt.decoded_size << " correctly accepted ✓" << std::endl;
                } else {
                    std::cerr << "    FAILED: Block size " << bt.decoded_size << " should be valid!" << std::endl;
                    assert(false && "Valid block size should be accepted");
                }
            }
        }
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 9: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}


// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC BLOCK SIZE BITS PROPERTY-BASED TESTS" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // Property 8: Reserved Block Size Pattern Detection
        // **Feature: flac-demuxer, Property 8: Reserved Block Size Pattern Detection**
        // **Validates: Requirements 5.2**
        test_property_reserved_block_size_pattern();
        
        // Property 9: Forbidden Block Size Detection
        // **Feature: flac-demuxer, Property 9: Forbidden Block Size Detection**
        // **Validates: Requirements 5.18**
        test_property_forbidden_block_size();
        
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
