/*
 * validate_rfc_frame_header.cpp - Simple validation of RFC 9639 frame header implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include <iostream>
#include <iomanip>
#include <vector>

/**
 * @brief Simple standalone validation of RFC 9639 frame header patterns
 * 
 * This test validates the frame header patterns that should be accepted
 * or rejected according to RFC 9639 specification without requiring
 * the full FLAC codec infrastructure.
 */
class RFC9639FrameHeaderValidator {
public:
    static bool validateSyncPattern(const uint8_t* data, size_t size) {
        if (!data || size < 2) return false;
        
        // RFC 9639 Section 9.1: 15-bit sync code 0b111111111111100
        uint16_t sync_pattern = (static_cast<uint16_t>(data[0]) << 8) | data[1];
        
        // Valid patterns: 0xFFF8 (fixed) or 0xFFF9 (variable)
        return (sync_pattern == 0xFFF8) || (sync_pattern == 0xFFF9);
    }
    
    static bool validateBlockSizeBits(uint8_t block_size_bits) {
        // RFC 9639 Section 9.1.1 Table 14
        return block_size_bits != 0x0; // 0x0 is reserved
    }
    
    static bool validateSampleRateBits(uint8_t sample_rate_bits) {
        // RFC 9639 Section 9.1.2 Table 15
        return sample_rate_bits != 0xF; // 0xF is forbidden
    }
    
    static bool validateChannelAssignment(uint8_t channel_assignment) {
        // RFC 9639 Section 9.1.3 Table 16
        return channel_assignment <= 0xA; // 0xB-0xF are reserved
    }
    
    static bool validateBitDepthBits(uint8_t bit_depth_bits) {
        // RFC 9639 Section 9.1.4 Table 17
        return bit_depth_bits != 0x3; // 0x3 is reserved
    }
    
    static bool runValidationTests() {
        std::cout << "RFC 9639 Frame Header Validation Test" << std::endl;
        std::cout << "=====================================" << std::endl;
        
        bool allPassed = true;
        
        // Test valid sync patterns
        std::cout << "Testing sync patterns..." << std::endl;
        uint8_t valid_fixed[] = {0xFF, 0xF8};
        uint8_t valid_variable[] = {0xFF, 0xF9};
        uint8_t invalid_sync[] = {0xFF, 0xF0};
        
        if (!validateSyncPattern(valid_fixed, 2)) {
            std::cout << "✗ Valid fixed sync pattern rejected" << std::endl;
            allPassed = false;
        } else {
            std::cout << "✓ Valid fixed sync pattern accepted" << std::endl;
        }
        
        if (!validateSyncPattern(valid_variable, 2)) {
            std::cout << "✗ Valid variable sync pattern rejected" << std::endl;
            allPassed = false;
        } else {
            std::cout << "✓ Valid variable sync pattern accepted" << std::endl;
        }
        
        if (validateSyncPattern(invalid_sync, 2)) {
            std::cout << "✗ Invalid sync pattern accepted" << std::endl;
            allPassed = false;
        } else {
            std::cout << "✓ Invalid sync pattern rejected" << std::endl;
        }
        
        // Test block size validation
        std::cout << "Testing block size bits..." << std::endl;
        if (validateBlockSizeBits(0x0)) {
            std::cout << "✗ Reserved block size (0x0) accepted" << std::endl;
            allPassed = false;
        } else {
            std::cout << "✓ Reserved block size (0x0) rejected" << std::endl;
        }
        
        if (!validateBlockSizeBits(0x1)) {
            std::cout << "✗ Valid block size (0x1) rejected" << std::endl;
            allPassed = false;
        } else {
            std::cout << "✓ Valid block size (0x1) accepted" << std::endl;
        }
        
        // Test sample rate validation
        std::cout << "Testing sample rate bits..." << std::endl;
        if (validateSampleRateBits(0xF)) {
            std::cout << "✗ Forbidden sample rate (0xF) accepted" << std::endl;
            allPassed = false;
        } else {
            std::cout << "✓ Forbidden sample rate (0xF) rejected" << std::endl;
        }
        
        if (!validateSampleRateBits(0x9)) {
            std::cout << "✗ Valid sample rate (0x9) rejected" << std::endl;
            allPassed = false;
        } else {
            std::cout << "✓ Valid sample rate (0x9) accepted" << std::endl;
        }
        
        // Test channel assignment validation
        std::cout << "Testing channel assignment..." << std::endl;
        if (validateChannelAssignment(0xB)) {
            std::cout << "✗ Reserved channel assignment (0xB) accepted" << std::endl;
            allPassed = false;
        } else {
            std::cout << "✓ Reserved channel assignment (0xB) rejected" << std::endl;
        }
        
        if (!validateChannelAssignment(0x8)) {
            std::cout << "✗ Valid channel assignment (0x8) rejected" << std::endl;
            allPassed = false;
        } else {
            std::cout << "✓ Valid channel assignment (0x8) accepted" << std::endl;
        }
        
        // Test bit depth validation
        std::cout << "Testing bit depth bits..." << std::endl;
        if (validateBitDepthBits(0x3)) {
            std::cout << "✗ Reserved bit depth (0x3) accepted" << std::endl;
            allPassed = false;
        } else {
            std::cout << "✓ Reserved bit depth (0x3) rejected" << std::endl;
        }
        
        if (!validateBitDepthBits(0x4)) {
            std::cout << "✗ Valid bit depth (0x4) rejected" << std::endl;
            allPassed = false;
        } else {
            std::cout << "✓ Valid bit depth (0x4) accepted" << std::endl;
        }
        
        std::cout << std::endl;
        if (allPassed) {
            std::cout << "✓ All RFC 9639 frame header validation tests PASSED" << std::endl;
        } else {
            std::cout << "✗ Some RFC 9639 frame header validation tests FAILED" << std::endl;
        }
        
        return allPassed;
    }
};

int main() {
    return RFC9639FrameHeaderValidator::runValidationTests() ? 0 : 1;
}