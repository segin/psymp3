/*
 * test_flac_bit_depth_bits_properties.cpp - Property-based tests for FLAC bit depth bits parsing
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
// STANDALONE BIT DEPTH BITS PARSER
// ========================================

/**
 * RFC 9639 Section 9.1.4: Bit Depth Encoding
 * 
 * Bit depth bits (3 bits from frame byte 3, bits 1-3):
 *   0b000: Get from STREAMINFO (non-streamable subset)
 *   0b001: 8 bits per sample
 *   0b010: 12 bits per sample
 *   0b011: Reserved (reject)
 *   0b100: 16 bits per sample
 *   0b101: 20 bits per sample
 *   0b110: 24 bits per sample
 *   0b111: 32 bits per sample
 * 
 * Reserved bit (bit 0 of frame byte 3):
 *   Must be 0, log warning if non-zero but continue processing
 */

/**
 * Result of bit depth bits parsing
 */
struct BitDepthResult {
    bool valid = false;           ///< True if parsing succeeded
    uint8_t bit_depth = 0;        ///< Decoded bit depth (8, 12, 16, 20, 24, or 32)
    bool is_reserved = false;     ///< True if reserved pattern 0b011 detected
    bool from_streaminfo = false; ///< True if bit depth should come from STREAMINFO
    bool reserved_bit_warning = false;  ///< True if reserved bit was non-zero
    std::string error_message;    ///< Error description if invalid
};

// Simulated STREAMINFO bit depth for testing (16 bits is common)
static constexpr uint8_t STREAMINFO_BITS_PER_SAMPLE = 16;

/**
 * Parse bit depth bits per RFC 9639 Section 9.1.4
 * 
 * @param bits The 3-bit bit depth code (bits 1-3 of frame byte 3)
 * @param reserved_bit The reserved bit (bit 0 of frame byte 3) - must be 0
 * @return BitDepthResult with parsing results
 */
BitDepthResult parseBitDepthBits(uint8_t bits, uint8_t reserved_bit) {
    BitDepthResult result;
    
    // Ensure only 3 bits are used
    bits &= 0x07;
    
    // Requirement 8.10, 8.11: Validate reserved bit at bit 0 of frame byte 3
    // RFC 9639 Section 9.1.4: "A reserved bit. It MUST have value 0"
    if (reserved_bit != 0) {
        result.reserved_bit_warning = true;
        // Per Requirement 8.11: Log warning and continue processing
    }
    
    // Requirement 8.5: Reserved bit depth pattern 0b011
    // RFC 9639 Table 1: Bit depth bits 0b011 is reserved
    if (bits == 0x03) {
        result.valid = false;
        result.is_reserved = true;
        result.error_message = "Reserved bit depth pattern 0b011 (Requirement 8.5)";
        return result;
    }
    
    // Requirement 8.1: Extract bits 1-3 of frame byte 3 for bit depth
    // (Already done by caller - bits parameter contains the extracted value)
    
    switch (bits) {
        // Requirement 8.2: 0b000 = Get from STREAMINFO
        // RFC 9639 Section 9.1.4: "Get sample size in bits from STREAMINFO metadata block"
        // Note: This makes the stream non-streamable subset compliant
        case 0x00:
            result.bit_depth = STREAMINFO_BITS_PER_SAMPLE;
            result.from_streaminfo = true;
            result.valid = true;
            break;
            
        // Requirement 8.3: 0b001 = 8 bits per sample
        case 0x01:
            result.bit_depth = 8;
            result.valid = true;
            break;
            
        // Requirement 8.4: 0b010 = 12 bits per sample
        case 0x02:
            result.bit_depth = 12;
            result.valid = true;
            break;
            
        // 0b011 is reserved and already rejected above (Requirement 8.5)
            
        // Requirement 8.6: 0b100 = 16 bits per sample
        case 0x04:
            result.bit_depth = 16;
            result.valid = true;
            break;
            
        // Requirement 8.7: 0b101 = 20 bits per sample
        case 0x05:
            result.bit_depth = 20;
            result.valid = true;
            break;
            
        // Requirement 8.8: 0b110 = 24 bits per sample
        case 0x06:
            result.bit_depth = 24;
            result.valid = true;
            break;
            
        // Requirement 8.9: 0b111 = 32 bits per sample
        case 0x07:
            result.bit_depth = 32;
            result.valid = true;
            break;
            
        default:
            // Should never reach here since we handle all 8 values (0-7)
            // and 0b011 is already rejected
            result.valid = false;
            result.error_message = "Unexpected bit depth bits";
            break;
    }
    
    return result;
}

/**
 * Helper to format bits as binary string (3 bits)
 */
std::string bitsToBinary3(uint8_t bits) {
    std::ostringstream oss;
    oss << "0b";
    for (int i = 2; i >= 0; --i) {
        oss << ((bits >> i) & 1);
    }
    return oss.str();
}

// ========================================
// PROPERTY-BASED TESTS
// ========================================

// ========================================
// PROPERTY 12: Reserved Bit Depth Detection
// ========================================
// **Feature: flac-demuxer, Property 12: Reserved Bit Depth Detection**
// **Validates: Requirements 8.5**
//
// For any frame header with bit depth bits equal to 0b011, the FLAC Demuxer 
// SHALL reject as a reserved pattern.

void test_property_reserved_bit_depth() {
    std::cout << "\n=== Property 12: Reserved Bit Depth Detection ===" << std::endl;
    std::cout << "Testing that bit depth bits 0b011 are rejected as reserved..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: Reserved pattern 0b011 must be rejected
    // ----------------------------------------
    std::cout << "\n  Test 1: Reserved pattern 0b011 rejection..." << std::endl;
    {
        tests_run++;
        
        uint8_t reserved_bits = 0x03;  // 0b011
        BitDepthResult result = parseBitDepthBits(reserved_bits, 0);
        
        if (!result.valid && result.is_reserved) {
            tests_passed++;
            std::cout << "    Bit depth bits " << bitsToBinary3(reserved_bits) << " rejected as reserved ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Bit depth bits " << bitsToBinary3(reserved_bits) << " was not rejected!" << std::endl;
            std::cerr << "    valid=" << result.valid << ", is_reserved=" << result.is_reserved << std::endl;
            assert(false && "Reserved pattern 0b011 should be rejected");
        }
    }
    
    // ----------------------------------------
    // Test 2: Valid patterns (0b000-0b010, 0b100-0b111) must be accepted
    // ----------------------------------------
    std::cout << "\n  Test 2: Valid patterns acceptance..." << std::endl;
    {
        // All valid patterns: 0b000, 0b001, 0b010, 0b100, 0b101, 0b110, 0b111
        std::vector<uint8_t> valid_patterns = {0x00, 0x01, 0x02, 0x04, 0x05, 0x06, 0x07};
        std::vector<uint8_t> expected_depths = {STREAMINFO_BITS_PER_SAMPLE, 8, 12, 16, 20, 24, 32};
        
        for (size_t i = 0; i < valid_patterns.size(); ++i) {
            tests_run++;
            
            BitDepthResult result = parseBitDepthBits(valid_patterns[i], 0);
            
            if (result.valid && !result.is_reserved && result.bit_depth == expected_depths[i]) {
                tests_passed++;
                std::cout << "    " << bitsToBinary3(valid_patterns[i]) << " -> " 
                          << static_cast<int>(result.bit_depth) << " bits";
                if (result.from_streaminfo) {
                    std::cout << " (from STREAMINFO)";
                }
                std::cout << " ✓" << std::endl;
            } else {
                std::cerr << "    FAILED: " << bitsToBinary3(valid_patterns[i]) << std::endl;
                std::cerr << "    Expected: valid=true, bit_depth=" << static_cast<int>(expected_depths[i]) << std::endl;
                std::cerr << "    Got: valid=" << result.valid << ", bit_depth=" << static_cast<int>(result.bit_depth)
                          << ", is_reserved=" << result.is_reserved << std::endl;
                assert(false && "Valid pattern should be accepted");
            }
        }
    }
    
    // ----------------------------------------
    // Test 3: Boundary verification - all 8 patterns
    // ----------------------------------------
    std::cout << "\n  Test 3: Boundary verification - all 8 patterns..." << std::endl;
    {
        for (uint8_t bits = 0; bits <= 7; ++bits) {
            tests_run++;
            
            BitDepthResult result = parseBitDepthBits(bits, 0);
            
            if (bits == 0x03) {
                // 0b011 should be reserved
                if (!result.valid && result.is_reserved) {
                    tests_passed++;
                } else {
                    std::cerr << "    FAILED: " << bitsToBinary3(bits) << " should be reserved!" << std::endl;
                    assert(false && "Reserved pattern should be rejected");
                }
            } else {
                // All other patterns should be valid
                if (result.valid && !result.is_reserved) {
                    tests_passed++;
                } else {
                    std::cerr << "    FAILED: " << bitsToBinary3(bits) << " should be valid!" << std::endl;
                    assert(false && "Valid pattern should be accepted");
                }
            }
        }
        std::cout << "    All 8 patterns correctly classified ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 4: Reserved bit warning (bit 0 of byte 3)
    // ----------------------------------------
    std::cout << "\n  Test 4: Reserved bit warning (bit 0 of byte 3)..." << std::endl;
    {
        // Test with reserved bit = 0 (normal case)
        tests_run++;
        BitDepthResult result_normal = parseBitDepthBits(0x04, 0);  // 16 bits, reserved_bit = 0
        
        if (result_normal.valid && !result_normal.reserved_bit_warning) {
            tests_passed++;
            std::cout << "    Reserved bit = 0: No warning ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Reserved bit = 0 should not trigger warning" << std::endl;
            assert(false && "Reserved bit = 0 should not trigger warning");
        }
        
        // Test with reserved bit = 1 (should warn but continue)
        tests_run++;
        BitDepthResult result_warning = parseBitDepthBits(0x04, 1);  // 16 bits, reserved_bit = 1
        
        if (result_warning.valid && result_warning.reserved_bit_warning && result_warning.bit_depth == 16) {
            tests_passed++;
            std::cout << "    Reserved bit = 1: Warning logged, parsing continues ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Reserved bit = 1 should warn but continue" << std::endl;
            std::cerr << "    valid=" << result_warning.valid << ", warning=" << result_warning.reserved_bit_warning 
                      << ", bit_depth=" << static_cast<int>(result_warning.bit_depth) << std::endl;
            assert(false && "Reserved bit = 1 should warn but continue");
        }
    }
    
    // ----------------------------------------
    // Test 5: Random valid patterns (100 iterations)
    // ----------------------------------------
    std::cout << "\n  Test 5: Random valid patterns (100 iterations)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        // Valid patterns: 0, 1, 2, 4, 5, 6, 7 (excluding 3)
        std::vector<uint8_t> valid_values = {0, 1, 2, 4, 5, 6, 7};
        std::uniform_int_distribution<> idx_dist(0, static_cast<int>(valid_values.size()) - 1);
        
        int random_passed = 0;
        
        for (int i = 0; i < 100; ++i) {
            uint8_t bits = valid_values[static_cast<size_t>(idx_dist(gen))];
            
            tests_run++;
            
            BitDepthResult result = parseBitDepthBits(bits, 0);
            
            if (result.valid && !result.is_reserved && result.bit_depth > 0) {
                tests_passed++;
                random_passed++;
            } else {
                std::cerr << "    FAILED iteration " << i << ": bits=" << bitsToBinary3(bits) << std::endl;
                assert(false && "Valid pattern should be accepted");
            }
        }
        std::cout << "    " << random_passed << "/100 random valid patterns passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 6: Reserved pattern with various reserved bit values
    // ----------------------------------------
    std::cout << "\n  Test 6: Reserved pattern 0b011 with various reserved bit values..." << std::endl;
    {
        // Reserved pattern should be rejected regardless of reserved bit value
        for (uint8_t reserved_bit = 0; reserved_bit <= 1; ++reserved_bit) {
            tests_run++;
            
            BitDepthResult result = parseBitDepthBits(0x03, reserved_bit);
            
            if (!result.valid && result.is_reserved) {
                tests_passed++;
                std::cout << "    0b011 with reserved_bit=" << static_cast<int>(reserved_bit) 
                          << " rejected ✓" << std::endl;
            } else {
                std::cerr << "    FAILED: 0b011 with reserved_bit=" << static_cast<int>(reserved_bit) 
                          << " should be rejected" << std::endl;
                assert(false && "Reserved pattern should be rejected");
            }
        }
    }
    
    // ----------------------------------------
    // Test 7: Bit depth value verification
    // ----------------------------------------
    std::cout << "\n  Test 7: Bit depth value verification..." << std::endl;
    {
        struct TestCase {
            uint8_t bits;
            uint8_t expected_depth;
            const char* description;
        };
        
        std::vector<TestCase> test_cases = {
            {0x00, STREAMINFO_BITS_PER_SAMPLE, "from STREAMINFO"},
            {0x01, 8, "8 bits"},
            {0x02, 12, "12 bits"},
            // 0x03 is reserved
            {0x04, 16, "16 bits"},
            {0x05, 20, "20 bits"},
            {0x06, 24, "24 bits"},
            {0x07, 32, "32 bits"},
        };
        
        for (const auto& tc : test_cases) {
            tests_run++;
            
            BitDepthResult result = parseBitDepthBits(tc.bits, 0);
            
            if (result.valid && result.bit_depth == tc.expected_depth) {
                tests_passed++;
                std::cout << "    " << bitsToBinary3(tc.bits) << " -> " 
                          << static_cast<int>(result.bit_depth) << " bits (" << tc.description << ") ✓" << std::endl;
            } else {
                std::cerr << "    FAILED: " << bitsToBinary3(tc.bits) << std::endl;
                std::cerr << "    Expected: " << static_cast<int>(tc.expected_depth) << " bits" << std::endl;
                std::cerr << "    Got: " << static_cast<int>(result.bit_depth) << " bits" << std::endl;
                assert(false && "Bit depth value should match expected");
            }
        }
    }
    
    // ----------------------------------------
    // Test 8: STREAMINFO flag verification
    // ----------------------------------------
    std::cout << "\n  Test 8: STREAMINFO flag verification..." << std::endl;
    {
        // Only 0b000 should have from_streaminfo = true
        for (uint8_t bits = 0; bits <= 7; ++bits) {
            if (bits == 0x03) continue;  // Skip reserved
            
            tests_run++;
            
            BitDepthResult result = parseBitDepthBits(bits, 0);
            bool expected_from_streaminfo = (bits == 0x00);
            
            if (result.from_streaminfo == expected_from_streaminfo) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: " << bitsToBinary3(bits) << std::endl;
                std::cerr << "    Expected from_streaminfo: " << expected_from_streaminfo << std::endl;
                std::cerr << "    Got from_streaminfo: " << result.from_streaminfo << std::endl;
                assert(false && "STREAMINFO flag should be correct");
            }
        }
        std::cout << "    STREAMINFO flag correctly set for all patterns ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 12: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}


// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC BIT DEPTH BITS PROPERTY-BASED TESTS" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // Property 12: Reserved Bit Depth Detection
        // **Feature: flac-demuxer, Property 12: Reserved Bit Depth Detection**
        // **Validates: Requirements 8.5**
        test_property_reserved_bit_depth();
        
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
