/*
 * test_flac_channel_bits_properties.cpp - Property-based tests for FLAC channel bits parsing
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
// STANDALONE CHANNEL BITS PARSER
// ========================================

/**
 * RFC 9639 Section 9.1.3: Channel Assignment Encoding
 * 
 * Channel bits (4 bits from frame byte 3, bits 4-7):
 *   0b0000-0b0111: 1-8 independent channels (value + 1)
 *   0b1000: Left-side stereo (left + side)
 *   0b1001: Right-side stereo (side + right)
 *   0b1010: Mid-side stereo (mid + side)
 *   0b1011-0b1111: Reserved (reject)
 */

/**
 * Channel assignment mode per RFC 9639 Section 9.1.3
 */
enum class ChannelMode : uint8_t {
    INDEPENDENT = 0,    ///< Independent channels (1-8 channels, no decorrelation)
    LEFT_SIDE = 1,      ///< Left-side stereo (left channel + side channel)
    RIGHT_SIDE = 2,     ///< Right-side stereo (side channel + right channel)
    MID_SIDE = 3        ///< Mid-side stereo (mid channel + side channel)
};

/**
 * Result of channel bits parsing
 */
struct ChannelResult {
    bool valid = false;           ///< True if parsing succeeded
    uint8_t channels = 0;         ///< Number of channels (1-8)
    ChannelMode mode = ChannelMode::INDEPENDENT;  ///< Channel assignment mode
    bool is_reserved = false;     ///< True if reserved pattern detected
    std::string error_message;    ///< Error description if invalid
};

/**
 * Parse channel bits per RFC 9639 Section 9.1.3
 * 
 * @param bits The 4-bit channel assignment code (bits 4-7 of frame byte 3)
 * @return ChannelResult with parsing results
 */
ChannelResult parseChannelBits(uint8_t bits) {
    ChannelResult result;
    
    // Ensure only 4 bits are used
    bits &= 0x0F;
    
    // Requirement 7.7: Reserved channel bits 0b1011-0b1111
    // RFC 9639 Table 1: Channel bits 0b1011-0b1111 are reserved
    if (bits >= 0x0B) {
        result.valid = false;
        result.is_reserved = true;
        result.error_message = "Reserved channel bits 0b" + 
            std::to_string((bits >> 3) & 1) + std::to_string((bits >> 2) & 1) +
            std::to_string((bits >> 1) & 1) + std::to_string(bits & 1) +
            " (Requirement 7.7)";
        return result;
    }
    
    // Requirement 7.1: Extract bits 4-7 of frame byte 3 for channel assignment
    // (Already done by caller - bits parameter contains the extracted value)
    
    if (bits <= 0x07) {
        // Requirement 7.2: Independent channels (1-8)
        // RFC 9639 Section 9.1.3: "n channels, where n is the value plus 1"
        result.channels = bits + 1;
        
        // Requirement 7.3: Mode is independent
        result.mode = ChannelMode::INDEPENDENT;
        result.valid = true;
    } else if (bits == 0x08) {
        // Requirement 7.4: Left-side stereo
        // RFC 9639 Section 9.1.3: "left/side stereo: channel 0 is the left channel, 
        // channel 1 is the side (difference) channel"
        result.channels = 2;
        result.mode = ChannelMode::LEFT_SIDE;
        result.valid = true;
    } else if (bits == 0x09) {
        // Requirement 7.5: Right-side stereo
        // RFC 9639 Section 9.1.3: "side/right stereo: channel 0 is the side (difference) 
        // channel, channel 1 is the right channel"
        result.channels = 2;
        result.mode = ChannelMode::RIGHT_SIDE;
        result.valid = true;
    } else if (bits == 0x0A) {
        // Requirement 7.6: Mid-side stereo
        // RFC 9639 Section 9.1.3: "mid/side stereo: channel 0 is the mid (average) 
        // channel, channel 1 is the side (difference) channel"
        result.channels = 2;
        result.mode = ChannelMode::MID_SIDE;
        result.valid = true;
    }
    // 0x0B-0x0F are reserved and already rejected above
    
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

/**
 * Helper to get channel mode name
 */
std::string getModeName(ChannelMode mode) {
    switch (mode) {
        case ChannelMode::INDEPENDENT: return "independent";
        case ChannelMode::LEFT_SIDE: return "left-side";
        case ChannelMode::RIGHT_SIDE: return "right-side";
        case ChannelMode::MID_SIDE: return "mid-side";
        default: return "unknown";
    }
}

// ========================================
// PROPERTY-BASED TESTS
// ========================================

// ========================================
// PROPERTY 11: Reserved Channel Bits Detection
// ========================================
// **Feature: flac-demuxer, Property 11: Reserved Channel Bits Detection**
// **Validates: Requirements 7.7**
//
// For any frame header with channel bits in range 0b1011-0b1111, the FLAC Demuxer 
// SHALL reject as a reserved pattern.

void test_property_reserved_channel_bits() {
    std::cout << "\n=== Property 11: Reserved Channel Bits Detection ===" << std::endl;
    std::cout << "Testing that channel bits 0b1011-0b1111 are rejected as reserved..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: Reserved patterns 0b1011-0b1111 must be rejected
    // ----------------------------------------
    std::cout << "\n  Test 1: Reserved patterns 0b1011-0b1111 rejection..." << std::endl;
    {
        // All reserved patterns: 0b1011 (11), 0b1100 (12), 0b1101 (13), 0b1110 (14), 0b1111 (15)
        std::vector<uint8_t> reserved_patterns = {0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
        
        for (uint8_t bits : reserved_patterns) {
            tests_run++;
            
            ChannelResult result = parseChannelBits(bits);
            
            if (!result.valid && result.is_reserved) {
                tests_passed++;
                std::cout << "    Channel bits " << bitsToBinary(bits) << " rejected as reserved ✓" << std::endl;
            } else {
                std::cerr << "    FAILED: Channel bits " << bitsToBinary(bits) << " was not rejected!" << std::endl;
                std::cerr << "    valid=" << result.valid << ", is_reserved=" << result.is_reserved << std::endl;
                assert(false && "Reserved pattern should be rejected");
            }
        }
    }
    
    // ----------------------------------------
    // Test 2: Independent channels (0b0000-0b0111) must be accepted
    // ----------------------------------------
    std::cout << "\n  Test 2: Independent channels (0b0000-0b0111) acceptance..." << std::endl;
    {
        for (uint8_t bits = 0x00; bits <= 0x07; ++bits) {
            tests_run++;
            
            ChannelResult result = parseChannelBits(bits);
            uint8_t expected_channels = bits + 1;
            
            if (result.valid && result.channels == expected_channels && 
                result.mode == ChannelMode::INDEPENDENT && !result.is_reserved) {
                tests_passed++;
                std::cout << "    " << bitsToBinary(bits) << " -> " 
                          << static_cast<int>(expected_channels) << " channel(s), "
                          << getModeName(result.mode) << " ✓" << std::endl;
            } else {
                std::cerr << "    FAILED: " << bitsToBinary(bits) << std::endl;
                std::cerr << "    Expected: valid=true, channels=" << static_cast<int>(expected_channels) 
                          << ", mode=independent" << std::endl;
                std::cerr << "    Got: valid=" << result.valid << ", channels=" << static_cast<int>(result.channels)
                          << ", mode=" << getModeName(result.mode) 
                          << ", is_reserved=" << result.is_reserved << std::endl;
                assert(false && "Independent channel pattern should be accepted");
            }
        }
    }
    
    // ----------------------------------------
    // Test 3: Stereo modes (0b1000-0b1010) must be accepted
    // ----------------------------------------
    std::cout << "\n  Test 3: Stereo modes (0b1000-0b1010) acceptance..." << std::endl;
    {
        struct StereoTestCase {
            uint8_t bits;
            ChannelMode expected_mode;
            const char* mode_name;
        };
        
        std::vector<StereoTestCase> stereo_cases = {
            {0x08, ChannelMode::LEFT_SIDE, "left-side"},
            {0x09, ChannelMode::RIGHT_SIDE, "right-side"},
            {0x0A, ChannelMode::MID_SIDE, "mid-side"},
        };
        
        for (const auto& tc : stereo_cases) {
            tests_run++;
            
            ChannelResult result = parseChannelBits(tc.bits);
            
            if (result.valid && result.channels == 2 && 
                result.mode == tc.expected_mode && !result.is_reserved) {
                tests_passed++;
                std::cout << "    " << bitsToBinary(tc.bits) << " -> 2 channels, "
                          << tc.mode_name << " stereo ✓" << std::endl;
            } else {
                std::cerr << "    FAILED: " << bitsToBinary(tc.bits) << std::endl;
                std::cerr << "    Expected: valid=true, channels=2, mode=" << tc.mode_name << std::endl;
                std::cerr << "    Got: valid=" << result.valid << ", channels=" << static_cast<int>(result.channels)
                          << ", mode=" << getModeName(result.mode) 
                          << ", is_reserved=" << result.is_reserved << std::endl;
                assert(false && "Stereo mode pattern should be accepted");
            }
        }
    }
    
    // ----------------------------------------
    // Test 4: Boundary verification - all 16 patterns
    // ----------------------------------------
    std::cout << "\n  Test 4: Boundary verification - all 16 patterns..." << std::endl;
    {
        for (uint8_t bits = 0; bits <= 15; ++bits) {
            tests_run++;
            
            ChannelResult result = parseChannelBits(bits);
            
            if (bits >= 0x0B) {
                // 0b1011-0b1111 should be reserved
                if (!result.valid && result.is_reserved) {
                    tests_passed++;
                } else {
                    std::cerr << "    FAILED: " << bitsToBinary(bits) << " should be reserved!" << std::endl;
                    assert(false && "Reserved pattern should be rejected");
                }
            } else {
                // 0b0000-0b1010 should be valid
                if (result.valid && !result.is_reserved) {
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
    // Test 5: Random valid patterns (100 iterations)
    // ----------------------------------------
    std::cout << "\n  Test 5: Random valid patterns (100 iterations)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> bits_dist(0, 10);  // 0b0000 to 0b1010 (valid range)
        
        int random_passed = 0;
        
        for (int i = 0; i < 100; ++i) {
            uint8_t bits = static_cast<uint8_t>(bits_dist(gen));
            
            tests_run++;
            
            ChannelResult result = parseChannelBits(bits);
            
            if (result.valid && !result.is_reserved && result.channels > 0 && result.channels <= 8) {
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
    // Test 6: Random reserved patterns (100 iterations)
    // ----------------------------------------
    std::cout << "\n  Test 6: Random reserved patterns (100 iterations)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> bits_dist(11, 15);  // 0b1011 to 0b1111 (reserved range)
        
        int random_passed = 0;
        
        for (int i = 0; i < 100; ++i) {
            uint8_t bits = static_cast<uint8_t>(bits_dist(gen));
            
            tests_run++;
            
            ChannelResult result = parseChannelBits(bits);
            
            if (!result.valid && result.is_reserved) {
                tests_passed++;
                random_passed++;
            } else {
                std::cerr << "    FAILED iteration " << i << ": bits=" << bitsToBinary(bits) << std::endl;
                assert(false && "Reserved pattern should be rejected");
            }
        }
        std::cout << "    " << random_passed << "/100 random reserved patterns rejected ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 7: Channel count verification for independent mode
    // ----------------------------------------
    std::cout << "\n  Test 7: Channel count verification for independent mode..." << std::endl;
    {
        for (uint8_t bits = 0x00; bits <= 0x07; ++bits) {
            tests_run++;
            
            ChannelResult result = parseChannelBits(bits);
            uint8_t expected_channels = bits + 1;
            
            // Verify channel count is exactly bits + 1
            if (result.valid && result.channels == expected_channels) {
                tests_passed++;
                std::cout << "    " << bitsToBinary(bits) << " -> " 
                          << static_cast<int>(result.channels) << " channel(s) ✓" << std::endl;
            } else {
                std::cerr << "    FAILED: " << bitsToBinary(bits) << std::endl;
                std::cerr << "    Expected channels: " << static_cast<int>(expected_channels) << std::endl;
                std::cerr << "    Got channels: " << static_cast<int>(result.channels) << std::endl;
                assert(false && "Channel count should be bits + 1");
            }
        }
    }
    
    // ----------------------------------------
    // Test 8: Stereo modes always have 2 channels
    // ----------------------------------------
    std::cout << "\n  Test 8: Stereo modes always have 2 channels..." << std::endl;
    {
        std::vector<uint8_t> stereo_patterns = {0x08, 0x09, 0x0A};
        
        for (uint8_t bits : stereo_patterns) {
            tests_run++;
            
            ChannelResult result = parseChannelBits(bits);
            
            if (result.valid && result.channels == 2) {
                tests_passed++;
                std::cout << "    " << bitsToBinary(bits) << " (" << getModeName(result.mode) 
                          << ") -> 2 channels ✓" << std::endl;
            } else {
                std::cerr << "    FAILED: " << bitsToBinary(bits) << std::endl;
                std::cerr << "    Expected: 2 channels, Got: " << static_cast<int>(result.channels) << std::endl;
                assert(false && "Stereo modes should have 2 channels");
            }
        }
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 11: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}


// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC CHANNEL BITS PROPERTY-BASED TESTS" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // Property 11: Reserved Channel Bits Detection
        // **Feature: flac-demuxer, Property 11: Reserved Channel Bits Detection**
        // **Validates: Requirements 7.7**
        test_property_reserved_channel_bits();
        
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
