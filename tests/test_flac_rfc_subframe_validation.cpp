/*
 * test_flac_rfc_subframe_validation.cpp - RFC 9639 Subframe Type Validation Tests
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include <iostream>
#include <bitset>
#include <cassert>
#include <cstdint>

// Mock minimal dependencies for testing
namespace Debug {
    template<typename... Args>
    void log(const char* category, Args&&... args) {
        // Silent for tests
    }
}

// Mock StreamInfo for testing
struct StreamInfo {
    uint32_t sample_rate = 44100;
    uint16_t channels = 2;
    uint16_t bits_per_sample = 16;
    std::string codec = "flac";
};

// Minimal FLACCodec mock for testing subframe validation
class FLACCodecTest {
private:
    uint32_t m_sample_rate = 44100;
    
public:
    explicit FLACCodecTest(uint32_t sample_rate = 44100) : m_sample_rate(sample_rate) {}
    
    // RFC 9639 Section 9.2 Subframe Type Compliance Validation
    bool validateSubframeType_unlocked(uint8_t subframe_type_bits) const {
        // Validate individual subframe types per RFC 9639 specification
        if (validateConstantSubframe_unlocked(subframe_type_bits)) {
            return true;
        }
        
        if (validateVerbatimSubframe_unlocked(subframe_type_bits)) {
            return true;
        }
        
        if (validateFixedPredictorSubframe_unlocked(subframe_type_bits)) {
            return true;
        }
        
        if (validateLinearPredictorSubframe_unlocked(subframe_type_bits)) {
            return true;
        }
        
        // Check for reserved values per RFC 9639 Table 19
        if ((subframe_type_bits >= 0x02 && subframe_type_bits <= 0x07) ||
            (subframe_type_bits >= 0x0D && subframe_type_bits <= 0x1F)) {
            return false; // Reserved
        }
        
        return false; // Invalid
    }
    
    bool validateConstantSubframe_unlocked(uint8_t subframe_type_bits) const {
        return subframe_type_bits == 0x00;
    }
    
    bool validateVerbatimSubframe_unlocked(uint8_t subframe_type_bits) const {
        return subframe_type_bits == 0x01;
    }
    
    bool validateFixedPredictorSubframe_unlocked(uint8_t subframe_type_bits) const {
        if (subframe_type_bits >= 0x08 && subframe_type_bits <= 0x0C) {
            uint8_t predictor_order = subframe_type_bits - 0x08;
            return predictor_order <= 4;
        }
        return false;
    }
    
    bool validateLinearPredictorSubframe_unlocked(uint8_t subframe_type_bits) const {
        if (subframe_type_bits >= 0x20 && subframe_type_bits <= 0x3F) {
            uint8_t predictor_order = subframe_type_bits - 0x1F;
            
            if (predictor_order < 1 || predictor_order > 32) {
                return false;
            }
            
            // RFC 9639 Section 7: Streamable subset restriction
            if (m_sample_rate <= 48000 && predictor_order > 12) {
                return false;
            }
            
            return true;
        }
        return false;
    }
    
    bool validateWastedBitsFlag_unlocked(uint8_t wasted_bits_flag) const {
        return wasted_bits_flag <= 1;
    }
    
    uint8_t extractPredictorOrder_unlocked(uint8_t subframe_type_bits) const {
        if (subframe_type_bits >= 0x08 && subframe_type_bits <= 0x0C) {
            return subframe_type_bits - 0x08;
        } else if (subframe_type_bits >= 0x20 && subframe_type_bits <= 0x3F) {
            return subframe_type_bits - 0x1F;
        }
        return 0;
    }
};

void testConstantSubframe() {
    std::cout << "Testing CONSTANT subframe validation..." << std::endl;
    
    FLACCodecTest codec;
    
    // Test valid CONSTANT subframe (0b000000 = 0x00)
    assert(codec.validateConstantSubframe_unlocked(0x00) == true);
    assert(codec.validateSubframeType_unlocked(0x00) == true);
    std::cout << "✓ Valid CONSTANT subframe (0x00) accepted" << std::endl;
    
    // Test invalid values
    assert(codec.validateConstantSubframe_unlocked(0x01) == false);
    assert(codec.validateConstantSubframe_unlocked(0x08) == false);
    std::cout << "✓ Invalid CONSTANT subframe values rejected" << std::endl;
}

void testVerbatimSubframe() {
    std::cout << "Testing VERBATIM subframe validation..." << std::endl;
    
    FLACCodecTest codec;
    
    // Test valid VERBATIM subframe (0b000001 = 0x01)
    assert(codec.validateVerbatimSubframe_unlocked(0x01) == true);
    assert(codec.validateSubframeType_unlocked(0x01) == true);
    std::cout << "✓ Valid VERBATIM subframe (0x01) accepted" << std::endl;
    
    // Test invalid values
    assert(codec.validateVerbatimSubframe_unlocked(0x00) == false);
    assert(codec.validateVerbatimSubframe_unlocked(0x02) == false);
    std::cout << "✓ Invalid VERBATIM subframe values rejected" << std::endl;
}

void testReservedSubframes() {
    std::cout << "Testing reserved subframe validation..." << std::endl;
    
    FLACCodecTest codec;
    
    // Test reserved range 0x02-0x07 (0b000010 - 0b000111)
    for (uint8_t i = 0x02; i <= 0x07; i++) {
        assert(codec.validateSubframeType_unlocked(i) == false);
    }
    std::cout << "✓ Reserved subframe range 0x02-0x07 rejected" << std::endl;
    
    // Test reserved range 0x0D-0x1F (0b001101 - 0b011111)
    for (uint8_t i = 0x0D; i <= 0x1F; i++) {
        assert(codec.validateSubframeType_unlocked(i) == false);
    }
    std::cout << "✓ Reserved subframe range 0x0D-0x1F rejected" << std::endl;
}

void testFixedPredictorSubframes() {
    std::cout << "Testing FIXED predictor subframe validation..." << std::endl;
    
    FLACCodecTest codec;
    
    // Test valid FIXED predictor subframes (0x08-0x0C, orders 0-4)
    for (uint8_t i = 0x08; i <= 0x0C; i++) {
        uint8_t expected_order = i - 0x08;
        assert(codec.validateFixedPredictorSubframe_unlocked(i) == true);
        assert(codec.validateSubframeType_unlocked(i) == true);
        assert(codec.extractPredictorOrder_unlocked(i) == expected_order);
        std::cout << "✓ Valid FIXED predictor subframe 0x" << std::hex << (unsigned)i 
                  << " (order " << std::dec << (unsigned)expected_order << ") accepted" << std::endl;
    }
    
    // Test invalid values
    assert(codec.validateFixedPredictorSubframe_unlocked(0x07) == false);
    assert(codec.validateFixedPredictorSubframe_unlocked(0x0D) == false);
    std::cout << "✓ Invalid FIXED predictor subframe values rejected" << std::endl;
}

void testLinearPredictorSubframes() {
    std::cout << "Testing LPC predictor subframe validation..." << std::endl;
    
    FLACCodecTest codec_44k(44100);  // <= 48kHz, streamable subset applies
    FLACCodecTest codec_96k(96000);  // > 48kHz, no streamable subset restriction
    
    // Test valid LPC predictor subframes (0x20-0x3F, orders 1-32)
    for (uint8_t i = 0x20; i <= 0x3F; i++) {
        uint8_t expected_order = i - 0x1F; // 1-based
        
        assert(codec_96k.validateLinearPredictorSubframe_unlocked(i) == true);
        assert(codec_96k.validateSubframeType_unlocked(i) == true);
        assert(codec_96k.extractPredictorOrder_unlocked(i) == expected_order);
        
        // For 44.1kHz, orders > 12 should be rejected (streamable subset)
        if (expected_order <= 12) {
            assert(codec_44k.validateLinearPredictorSubframe_unlocked(i) == true);
            assert(codec_44k.validateSubframeType_unlocked(i) == true);
        } else {
            assert(codec_44k.validateLinearPredictorSubframe_unlocked(i) == false);
            assert(codec_44k.validateSubframeType_unlocked(i) == false);
        }
    }
    std::cout << "✓ Valid LPC predictor subframes validated correctly" << std::endl;
    std::cout << "✓ Streamable subset restriction (order <= 12 for <= 48kHz) enforced" << std::endl;
    
    // Test invalid values
    assert(codec_96k.validateLinearPredictorSubframe_unlocked(0x1F) == false);
    assert(codec_96k.validateLinearPredictorSubframe_unlocked(0x40) == false);
    std::cout << "✓ Invalid LPC predictor subframe values rejected" << std::endl;
}

void testWastedBitsFlag() {
    std::cout << "Testing wasted bits flag validation..." << std::endl;
    
    FLACCodecTest codec;
    
    // Test valid wasted bits flags
    assert(codec.validateWastedBitsFlag_unlocked(0) == true);
    assert(codec.validateWastedBitsFlag_unlocked(1) == true);
    std::cout << "✓ Valid wasted bits flags (0, 1) accepted" << std::endl;
    
    // Test invalid wasted bits flags
    assert(codec.validateWastedBitsFlag_unlocked(2) == false);
    assert(codec.validateWastedBitsFlag_unlocked(255) == false);
    std::cout << "✓ Invalid wasted bits flags rejected" << std::endl;
}

void testPredictorOrderExtraction() {
    std::cout << "Testing predictor order extraction..." << std::endl;
    
    FLACCodecTest codec;
    
    // Test FIXED predictor order extraction
    for (uint8_t i = 0x08; i <= 0x0C; i++) {
        uint8_t expected_order = i - 0x08;
        assert(codec.extractPredictorOrder_unlocked(i) == expected_order);
    }
    std::cout << "✓ FIXED predictor order extraction correct" << std::endl;
    
    // Test LPC predictor order extraction
    for (uint8_t i = 0x20; i <= 0x3F; i++) {
        uint8_t expected_order = i - 0x1F;
        assert(codec.extractPredictorOrder_unlocked(i) == expected_order);
    }
    std::cout << "✓ LPC predictor order extraction correct" << std::endl;
    
    // Test non-predictor subframes
    assert(codec.extractPredictorOrder_unlocked(0x00) == 0); // CONSTANT
    assert(codec.extractPredictorOrder_unlocked(0x01) == 0); // VERBATIM
    std::cout << "✓ Non-predictor subframes return order 0" << std::endl;
}

void testComprehensiveSubframeValidation() {
    std::cout << "Testing comprehensive subframe type validation..." << std::endl;
    
    FLACCodecTest codec;
    
    // Test all 64 possible 6-bit subframe type values
    for (uint16_t i = 0; i <= 0x3F; i++) {
        uint8_t subframe_type = static_cast<uint8_t>(i);
        bool should_be_valid = false;
        
        // Determine expected validity based on RFC 9639 Table 19
        if (subframe_type == 0x00) {
            should_be_valid = true; // CONSTANT
        } else if (subframe_type == 0x01) {
            should_be_valid = true; // VERBATIM
        } else if (subframe_type >= 0x02 && subframe_type <= 0x07) {
            should_be_valid = false; // Reserved
        } else if (subframe_type >= 0x08 && subframe_type <= 0x0C) {
            should_be_valid = true; // FIXED predictor
        } else if (subframe_type >= 0x0D && subframe_type <= 0x1F) {
            should_be_valid = false; // Reserved
        } else if (subframe_type >= 0x20 && subframe_type <= 0x3F) {
            // LPC predictor - check streamable subset restriction
            uint8_t predictor_order = subframe_type - 0x1F; // 1-based
            // For 44.1kHz (default), orders > 12 are invalid per RFC 9639 Section 7
            should_be_valid = (predictor_order <= 12);
        }
        
        bool actual_valid = codec.validateSubframeType_unlocked(subframe_type);
        
        if (actual_valid != should_be_valid) {
            std::cout << "✗ Validation mismatch for subframe type 0x" << std::hex << (unsigned)subframe_type
                      << " (0b" << std::bitset<6>(subframe_type) << "): expected " 
                      << (should_be_valid ? "valid" : "invalid") 
                      << ", got " << (actual_valid ? "valid" : "invalid") << std::endl;
            assert(false);
        }
    }
    
    std::cout << "✓ All 64 subframe type values validated correctly" << std::endl;
}

int main() {
    std::cout << "RFC 9639 Subframe Type Validation Test" << std::endl;
    std::cout << "======================================" << std::endl;
    
    try {
        testConstantSubframe();
        testVerbatimSubframe();
        testReservedSubframes();
        testFixedPredictorSubframes();
        testLinearPredictorSubframes();
        testWastedBitsFlag();
        testPredictorOrderExtraction();
        testComprehensiveSubframeValidation();
        
        std::cout << "✓ All RFC 9639 subframe type validation tests PASSED" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "✗ Test failed with unknown exception" << std::endl;
        return 1;
    }
}