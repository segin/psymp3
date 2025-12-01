/*
 * test_flac_sample_rate_bits_properties.cpp - Property-based tests for FLAC sample rate bits parsing
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
// STANDALONE SAMPLE RATE BITS PARSER
// ========================================

/**
 * RFC 9639 Section 9.1.2: Sample Rate Encoding
 * 
 * Sample rate bits (4 bits from frame byte 2, bits 0-3):
 *   0b0000: Get from STREAMINFO (non-streamable subset)
 *   0b0001: 88200 Hz
 *   0b0010: 176400 Hz
 *   0b0011: 192000 Hz
 *   0b0100: 8000 Hz
 *   0b0101: 16000 Hz
 *   0b0110: 22050 Hz
 *   0b0111: 24000 Hz
 *   0b1000: 32000 Hz
 *   0b1001: 44100 Hz
 *   0b1010: 48000 Hz
 *   0b1011: 96000 Hz
 *   0b1100: 8-bit uncommon sample rate in kHz follows
 *   0b1101: 16-bit uncommon sample rate in Hz follows
 *   0b1110: 16-bit uncommon sample rate in tens of Hz follows
 *   0b1111: Forbidden (reject)
 */

/**
 * Result of sample rate bits parsing
 */
struct SampleRateResult {
    bool valid = false;           ///< True if parsing succeeded
    uint32_t sample_rate = 0;     ///< Decoded sample rate in Hz
    bool is_forbidden = false;    ///< True if forbidden pattern detected
    bool uses_streaminfo = false; ///< True if sample rate comes from STREAMINFO
    std::string error_message;    ///< Error description if invalid
};

/**
 * Parse sample rate bits per RFC 9639 Section 9.1.2
 * 
 * @param bits The 4-bit sample rate code (bits 0-3 of frame byte 2)
 * @param streaminfo_sample_rate Sample rate from STREAMINFO (used when bits == 0b0000)
 * @param uncommon_buffer Optional buffer for uncommon sample rate bytes
 * @param uncommon_size Size of uncommon buffer
 * @return SampleRateResult with parsing results
 */
SampleRateResult parseSampleRateBits(uint8_t bits, uint32_t streaminfo_sample_rate = 44100,
                                      const uint8_t* uncommon_buffer = nullptr, 
                                      size_t uncommon_size = 0) {
    SampleRateResult result;
    
    // Ensure only 4 bits are used
    bits &= 0x0F;
    
    // Requirement 6.17: Forbidden sample rate pattern 0b1111
    // RFC 9639 Table 1: Sample rate bits 0b1111 is forbidden
    if (bits == 0x0F) {
        result.valid = false;
        result.is_forbidden = true;
        result.error_message = "Forbidden sample rate pattern 0b1111 (Requirement 6.17)";
        return result;
    }
    
    switch (bits) {
        // Requirement 6.2: 0b0000 = Get from STREAMINFO
        case 0x00:
            result.valid = true;
            result.sample_rate = streaminfo_sample_rate;
            result.uses_streaminfo = true;
            break;
            
        // Requirement 6.3: 0b0001 = 88200 Hz
        case 0x01:
            result.valid = true;
            result.sample_rate = 88200;
            break;
            
        // Requirement 6.4: 0b0010 = 176400 Hz
        case 0x02:
            result.valid = true;
            result.sample_rate = 176400;
            break;
            
        // Requirement 6.5: 0b0011 = 192000 Hz
        case 0x03:
            result.valid = true;
            result.sample_rate = 192000;
            break;
            
        // Requirement 6.6: 0b0100 = 8000 Hz
        case 0x04:
            result.valid = true;
            result.sample_rate = 8000;
            break;
            
        // Requirement 6.7: 0b0101 = 16000 Hz
        case 0x05:
            result.valid = true;
            result.sample_rate = 16000;
            break;
            
        // Requirement 6.8: 0b0110 = 22050 Hz
        case 0x06:
            result.valid = true;
            result.sample_rate = 22050;
            break;
            
        // Requirement 6.9: 0b0111 = 24000 Hz
        case 0x07:
            result.valid = true;
            result.sample_rate = 24000;
            break;
            
        // Requirement 6.10: 0b1000 = 32000 Hz
        case 0x08:
            result.valid = true;
            result.sample_rate = 32000;
            break;
            
        // Requirement 6.11: 0b1001 = 44100 Hz
        case 0x09:
            result.valid = true;
            result.sample_rate = 44100;
            break;
            
        // Requirement 6.12: 0b1010 = 48000 Hz
        case 0x0A:
            result.valid = true;
            result.sample_rate = 48000;
            break;
            
        // Requirement 6.13: 0b1011 = 96000 Hz
        case 0x0B:
            result.valid = true;
            result.sample_rate = 96000;
            break;
            
        // Requirement 6.14: 0b1100 = 8-bit uncommon sample rate in kHz follows
        case 0x0C:
            if (uncommon_buffer && uncommon_size >= 1) {
                // RFC 9639: stored value is sample rate in kHz
                result.sample_rate = static_cast<uint32_t>(uncommon_buffer[0]) * 1000;
                result.valid = true;
            } else {
                result.valid = false;
                result.error_message = "Missing 8-bit uncommon sample rate data";
            }
            break;
            
        // Requirement 6.15: 0b1101 = 16-bit uncommon sample rate in Hz follows
        case 0x0D:
            if (uncommon_buffer && uncommon_size >= 2) {
                // RFC 9639: 16-bit big-endian, stored value is sample rate in Hz
                uint16_t uncommon_value = (static_cast<uint16_t>(uncommon_buffer[0]) << 8) |
                                           static_cast<uint16_t>(uncommon_buffer[1]);
                result.sample_rate = static_cast<uint32_t>(uncommon_value);
                result.valid = true;
            } else {
                result.valid = false;
                result.error_message = "Missing 16-bit uncommon sample rate data";
            }
            break;
            
        // Requirement 6.16: 0b1110 = 16-bit uncommon sample rate in tens of Hz follows
        case 0x0E:
            if (uncommon_buffer && uncommon_size >= 2) {
                // RFC 9639: 16-bit big-endian, stored value is sample rate in tens of Hz
                uint16_t uncommon_value = (static_cast<uint16_t>(uncommon_buffer[0]) << 8) |
                                           static_cast<uint16_t>(uncommon_buffer[1]);
                result.sample_rate = static_cast<uint32_t>(uncommon_value) * 10;
                result.valid = true;
            } else {
                result.valid = false;
                result.error_message = "Missing 16-bit uncommon sample rate data";
            }
            break;
            
        // 0x0F is forbidden and already rejected above
        default:
            // Should never reach here
            result.valid = false;
            result.error_message = "Unexpected sample rate bits value";
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
// PROPERTY 10: Forbidden Sample Rate Detection
// ========================================
// **Feature: flac-demuxer, Property 10: Forbidden Sample Rate Detection**
// **Validates: Requirements 6.17**
//
// For any frame header with sample rate bits equal to 0b1111, the FLAC Demuxer 
// SHALL reject as a forbidden pattern.

void test_property_forbidden_sample_rate() {
    std::cout << "\n=== Property 10: Forbidden Sample Rate Detection ===" << std::endl;
    std::cout << "Testing that sample rate bits 0b1111 are rejected as forbidden..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: Forbidden pattern 0b1111 must be rejected
    // ----------------------------------------
    std::cout << "\n  Test 1: Forbidden pattern 0b1111 rejection..." << std::endl;
    {
        tests_run++;
        
        SampleRateResult result = parseSampleRateBits(0x0F);
        
        if (!result.valid && result.is_forbidden) {
            tests_passed++;
            std::cout << "    Sample rate bits 0b1111 rejected as forbidden ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Sample rate bits 0b1111 was not rejected!" << std::endl;
            std::cerr << "    valid=" << result.valid << ", is_forbidden=" << result.is_forbidden << std::endl;
            assert(false && "Forbidden pattern 0b1111 should be rejected");
        }
    }
    
    // ----------------------------------------
    // Test 2: All valid patterns (0b0000-0b1110) must be accepted
    // ----------------------------------------
    std::cout << "\n  Test 2: All valid patterns (0b0000-0b1110) acceptance..." << std::endl;
    {
        // Expected sample rates for each pattern (excluding uncommon patterns 0x0C, 0x0D, 0x0E)
        struct PatternExpectation {
            uint8_t bits;
            uint32_t expected_rate;
            bool needs_uncommon_data;
            bool uses_streaminfo;
        };
        
        std::vector<PatternExpectation> patterns = {
            {0x00, 44100, false, true},   // From STREAMINFO (default 44100)
            {0x01, 88200, false, false},
            {0x02, 176400, false, false},
            {0x03, 192000, false, false},
            {0x04, 8000, false, false},
            {0x05, 16000, false, false},
            {0x06, 22050, false, false},
            {0x07, 24000, false, false},
            {0x08, 32000, false, false},
            {0x09, 44100, false, false},
            {0x0A, 48000, false, false},
            {0x0B, 96000, false, false},
            // 0x0C, 0x0D, 0x0E need uncommon data - tested separately
        };
        
        for (const auto& pattern : patterns) {
            tests_run++;
            
            SampleRateResult result = parseSampleRateBits(pattern.bits);
            
            if (result.valid && result.sample_rate == pattern.expected_rate && !result.is_forbidden) {
                tests_passed++;
                std::cout << "    " << bitsToBinary(pattern.bits) << " -> " 
                          << pattern.expected_rate << " Hz";
                if (pattern.uses_streaminfo) {
                    std::cout << " (from STREAMINFO)";
                }
                std::cout << " ✓" << std::endl;
            } else {
                std::cerr << "    FAILED: " << bitsToBinary(pattern.bits) << std::endl;
                std::cerr << "    Expected: valid=true, rate=" << pattern.expected_rate << std::endl;
                std::cerr << "    Got: valid=" << result.valid << ", rate=" << result.sample_rate 
                          << ", is_forbidden=" << result.is_forbidden << std::endl;
                assert(false && "Valid pattern should be accepted");
            }
        }
    }
    
    // ----------------------------------------
    // Test 3: Uncommon 8-bit sample rate in kHz (0b1100)
    // ----------------------------------------
    std::cout << "\n  Test 3: Uncommon 8-bit sample rate in kHz (0b1100)..." << std::endl;
    {
        // Test various 8-bit uncommon values (in kHz)
        std::vector<std::pair<uint8_t, uint32_t>> test_cases = {
            {1, 1000},       // 1 kHz
            {8, 8000},       // 8 kHz
            {22, 22000},     // 22 kHz
            {44, 44000},     // 44 kHz
            {48, 48000},     // 48 kHz
            {96, 96000},     // 96 kHz
            {192, 192000},   // 192 kHz
            {255, 255000},   // Maximum: 255 kHz
        };
        
        for (const auto& tc : test_cases) {
            tests_run++;
            
            uint8_t uncommon_data[1] = {tc.first};
            SampleRateResult result = parseSampleRateBits(0x0C, 44100, uncommon_data, 1);
            
            if (result.valid && result.sample_rate == tc.second && !result.is_forbidden) {
                tests_passed++;
                std::cout << "    8-bit uncommon value " << static_cast<int>(tc.first) 
                          << " kHz -> " << tc.second << " Hz ✓" << std::endl;
            } else {
                std::cerr << "    FAILED: 8-bit uncommon value " << static_cast<int>(tc.first) << " kHz" << std::endl;
                std::cerr << "    Expected: " << tc.second << " Hz, Got: " << result.sample_rate << " Hz" << std::endl;
                assert(false && "8-bit uncommon sample rate should be parsed correctly");
            }
        }
    }
    
    // ----------------------------------------
    // Test 4: Uncommon 16-bit sample rate in Hz (0b1101)
    // ----------------------------------------
    std::cout << "\n  Test 4: Uncommon 16-bit sample rate in Hz (0b1101)..." << std::endl;
    {
        // Test various 16-bit uncommon values (in Hz, big-endian)
        struct TestCase {
            uint8_t high_byte;
            uint8_t low_byte;
            uint32_t expected_rate;
        };
        
        std::vector<TestCase> test_cases = {
            {0x00, 0x01, 1},         // 1 Hz
            {0x00, 0xFF, 255},       // 255 Hz
            {0x01, 0x00, 256},       // 256 Hz
            {0x1F, 0x40, 8000},      // 8000 Hz
            {0xAC, 0x44, 44100},     // 44100 Hz
            {0xBB, 0x80, 48000},     // 48000 Hz
            {0xFF, 0xFF, 65535},     // Maximum: 65535 Hz
        };
        
        for (const auto& tc : test_cases) {
            tests_run++;
            
            uint8_t uncommon_data[2] = {tc.high_byte, tc.low_byte};
            SampleRateResult result = parseSampleRateBits(0x0D, 44100, uncommon_data, 2);
            
            if (result.valid && result.sample_rate == tc.expected_rate && !result.is_forbidden) {
                tests_passed++;
                std::cout << "    16-bit uncommon 0x" << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<int>(tc.high_byte) << std::setw(2) << static_cast<int>(tc.low_byte)
                          << std::dec << " -> " << tc.expected_rate << " Hz ✓" << std::endl;
            } else {
                std::cerr << "    FAILED: 16-bit uncommon 0x" << std::hex 
                          << static_cast<int>(tc.high_byte) << static_cast<int>(tc.low_byte) << std::dec << std::endl;
                std::cerr << "    Expected: " << tc.expected_rate << " Hz, Got: " << result.sample_rate << " Hz" << std::endl;
                assert(false && "16-bit uncommon sample rate should be parsed correctly");
            }
        }
    }
    
    // ----------------------------------------
    // Test 5: Uncommon 16-bit sample rate in tens of Hz (0b1110)
    // ----------------------------------------
    std::cout << "\n  Test 5: Uncommon 16-bit sample rate in tens of Hz (0b1110)..." << std::endl;
    {
        // Test various 16-bit uncommon values (in tens of Hz, big-endian)
        struct TestCase {
            uint8_t high_byte;
            uint8_t low_byte;
            uint32_t expected_rate;
        };
        
        std::vector<TestCase> test_cases = {
            {0x00, 0x01, 10},         // 1 * 10 = 10 Hz
            {0x00, 0x64, 1000},       // 100 * 10 = 1000 Hz
            {0x03, 0x20, 8000},       // 800 * 10 = 8000 Hz
            {0x11, 0x3A, 44100},      // 4410 * 10 = 44100 Hz (0x113A = 4410)
            {0x12, 0xC0, 48000},      // 4800 * 10 = 48000 Hz
            {0x25, 0x80, 96000},      // 9600 * 10 = 96000 Hz
            {0xFF, 0xFF, 655350},     // Maximum: 65535 * 10 = 655350 Hz
        };
        
        for (const auto& tc : test_cases) {
            tests_run++;
            
            uint8_t uncommon_data[2] = {tc.high_byte, tc.low_byte};
            SampleRateResult result = parseSampleRateBits(0x0E, 44100, uncommon_data, 2);
            
            if (result.valid && result.sample_rate == tc.expected_rate && !result.is_forbidden) {
                tests_passed++;
                std::cout << "    16-bit uncommon (x10) 0x" << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<int>(tc.high_byte) << std::setw(2) << static_cast<int>(tc.low_byte)
                          << std::dec << " -> " << tc.expected_rate << " Hz ✓" << std::endl;
            } else {
                std::cerr << "    FAILED: 16-bit uncommon (x10) 0x" << std::hex 
                          << static_cast<int>(tc.high_byte) << static_cast<int>(tc.low_byte) << std::dec << std::endl;
                std::cerr << "    Expected: " << tc.expected_rate << " Hz, Got: " << result.sample_rate << " Hz" << std::endl;
                assert(false && "16-bit uncommon sample rate (x10) should be parsed correctly");
            }
        }
    }
    
    // ----------------------------------------
    // Test 6: Random valid patterns (100 iterations)
    // ----------------------------------------
    std::cout << "\n  Test 6: Random valid patterns (100 iterations)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> bits_dist(0, 14);  // 0b0000 to 0b1110 (excluding forbidden 0b1111)
        
        int random_passed = 0;
        
        for (int i = 0; i < 100; ++i) {
            uint8_t bits = static_cast<uint8_t>(bits_dist(gen));
            
            tests_run++;
            
            // For uncommon patterns, provide appropriate data
            uint8_t uncommon_data_8bit[1] = {0x2C};   // 44 kHz for 8-bit uncommon
            uint8_t uncommon_data_16bit[2] = {0x00, 0x2C};  // 44 Hz or 440 Hz for 16-bit uncommon
            const uint8_t* data_ptr = nullptr;
            size_t data_size = 0;
            
            if (bits == 0x0C) {
                data_ptr = uncommon_data_8bit;
                data_size = 1;
            } else if (bits == 0x0D || bits == 0x0E) {
                data_ptr = uncommon_data_16bit;
                data_size = 2;
            }
            
            SampleRateResult result = parseSampleRateBits(bits, 44100, data_ptr, data_size);
            
            if (result.valid && !result.is_forbidden && result.sample_rate > 0) {
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
    // Test 7: STREAMINFO sample rate propagation (0b0000)
    // ----------------------------------------
    std::cout << "\n  Test 7: STREAMINFO sample rate propagation (0b0000)..." << std::endl;
    {
        // Test that various STREAMINFO sample rates are correctly propagated
        std::vector<uint32_t> streaminfo_rates = {8000, 16000, 22050, 44100, 48000, 96000, 192000};
        
        for (uint32_t rate : streaminfo_rates) {
            tests_run++;
            
            SampleRateResult result = parseSampleRateBits(0x00, rate);
            
            if (result.valid && result.sample_rate == rate && result.uses_streaminfo && !result.is_forbidden) {
                tests_passed++;
                std::cout << "    STREAMINFO rate " << rate << " Hz propagated correctly ✓" << std::endl;
            } else {
                std::cerr << "    FAILED: STREAMINFO rate " << rate << " Hz not propagated!" << std::endl;
                std::cerr << "    Got: " << result.sample_rate << " Hz, uses_streaminfo=" << result.uses_streaminfo << std::endl;
                assert(false && "STREAMINFO sample rate should be propagated");
            }
        }
    }
    
    // ----------------------------------------
    // Test 8: Only 0b1111 is forbidden (boundary test)
    // ----------------------------------------
    std::cout << "\n  Test 8: Only 0b1111 is forbidden (boundary verification)..." << std::endl;
    {
        // Test all 16 possible values
        for (uint8_t bits = 0; bits <= 15; ++bits) {
            tests_run++;
            
            // Provide uncommon data for patterns that need it
            uint8_t uncommon_data[2] = {0x00, 0x2C};
            const uint8_t* data_ptr = nullptr;
            size_t data_size = 0;
            
            if (bits == 0x0C) {
                data_ptr = uncommon_data;
                data_size = 1;
            } else if (bits == 0x0D || bits == 0x0E) {
                data_ptr = uncommon_data;
                data_size = 2;
            }
            
            SampleRateResult result = parseSampleRateBits(bits, 44100, data_ptr, data_size);
            
            if (bits == 0x0F) {
                // 0b1111 should be forbidden
                if (!result.valid && result.is_forbidden) {
                    tests_passed++;
                    std::cout << "    " << bitsToBinary(bits) << " correctly rejected as forbidden ✓" << std::endl;
                } else {
                    std::cerr << "    FAILED: " << bitsToBinary(bits) << " should be forbidden!" << std::endl;
                    assert(false && "0b1111 should be forbidden");
                }
            } else {
                // All other values should be valid
                if (result.valid && !result.is_forbidden) {
                    tests_passed++;
                } else {
                    std::cerr << "    FAILED: " << bitsToBinary(bits) << " should be valid!" << std::endl;
                    assert(false && "Valid pattern should be accepted");
                }
            }
        }
        std::cout << "    All 16 patterns correctly classified ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 10: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}


// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC SAMPLE RATE BITS PROPERTY-BASED TESTS" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // Property 10: Forbidden Sample Rate Detection
        // **Feature: flac-demuxer, Property 10: Forbidden Sample Rate Detection**
        // **Validates: Requirements 6.17**
        test_property_forbidden_sample_rate();
        
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

