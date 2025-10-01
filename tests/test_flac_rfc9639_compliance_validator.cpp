/*
 * test_flac_rfc9639_compliance_validator.cpp - Test RFC 9639 compliance validation and debugging tools
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

#include "psymp3.h"

#ifdef HAVE_FLAC

// Include the comprehensive testing validator from tests directory
#include "FLACRFCComplianceValidator.h"

#include <cassert>
#include <iostream>
#include <vector>
#include <cstring>

/**
 * @brief Test RFC 9639 sync pattern validation
 */
void test_sync_pattern_validation() {
    std::cout << "Testing RFC 9639 sync pattern validation..." << std::endl;
    
    FLACRFCComplianceValidator validator;
    
    // Test valid sync pattern (0x3FFE followed by reserved bit 0)
    uint8_t valid_frame[] = {
        0xFF, 0xF8,  // Sync pattern 0x3FFE + reserved bit 0 + blocking strategy 0
        0x10, 0x00   // Block size 0001 + sample rate 0000 + channel assignment 0000 + sample size 000 + reserved bit 0
    };
    
    FrameComplianceAnalysis analysis = validator.validateFrame(valid_frame, sizeof(valid_frame), 0, 0);
    assert(analysis.sync_pattern_valid);
    std::cout << "✓ Valid sync pattern correctly identified" << std::endl;
    
    // Test invalid sync pattern
    uint8_t invalid_sync[] = {
        0xFF, 0xF0,  // Invalid sync pattern
        0x10, 0x00
    };
    
    analysis = validator.validateFrame(invalid_sync, sizeof(invalid_sync), 1, 0);
    assert(!analysis.sync_pattern_valid);
    assert(!analysis.is_compliant);
    std::cout << "✓ Invalid sync pattern correctly rejected" << std::endl;
    
    // Test reserved bit violation
    uint8_t reserved_bit_violation[] = {
        0xFF, 0xFA,  // Valid sync but reserved bit = 1
        0x10, 0x00
    };
    
    analysis = validator.validateFrame(reserved_bit_violation, sizeof(reserved_bit_violation), 2, 0);
    assert(!analysis.sync_pattern_valid);
    assert(!analysis.is_compliant);
    std::cout << "✓ Reserved bit violation correctly detected" << std::endl;
}

/**
 * @brief Test RFC 9639 frame header validation
 */
void test_frame_header_validation() {
    std::cout << "Testing RFC 9639 frame header validation..." << std::endl;
    
    FLACRFCComplianceValidator validator;
    
    // Test valid frame header
    uint8_t valid_header[] = {
        0xFF, 0xF8,  // Valid sync pattern + reserved bit 0 + blocking strategy 0
        0x10,        // Block size 0001 (192 samples) + sample rate 0000 (from STREAMINFO)
        0x10,        // Channel assignment 0001 (2 channels) + sample size 000 (from STREAMINFO) + reserved bit 0
        0x00         // Frame number/sample number (simplified)
    };
    
    FrameComplianceAnalysis analysis = validator.validateFrame(valid_header, sizeof(valid_header), 0, 0);
    assert(analysis.sync_pattern_valid);
    assert(analysis.block_size_valid);
    assert(analysis.sample_rate_valid);
    assert(analysis.channel_assignment_valid);
    assert(analysis.sample_size_valid);
    std::cout << "✓ Valid frame header correctly validated" << std::endl;
    
    // Test reserved block size (0x00)
    uint8_t reserved_block_size[] = {
        0xFF, 0xF8,  // Valid sync
        0x00,        // Reserved block size 0000 + sample rate 0000
        0x10,        // Valid channel assignment and sample size
        0x00
    };
    
    analysis = validator.validateFrame(reserved_block_size, sizeof(reserved_block_size), 1, 0);
    std::cout << "Block size valid: " << analysis.block_size_valid << ", compliant: " << analysis.is_compliant << std::endl;
    std::cout << "Violations: " << analysis.violations.size() << std::endl;
    for (const auto& violation : analysis.violations) {
        std::cout << "  " << violation.toString() << std::endl;
    }
    assert(!analysis.block_size_valid);
    assert(!analysis.is_compliant);
    std::cout << "✓ Reserved block size correctly rejected" << std::endl;
    
    // Test invalid sample rate (0x0F)
    uint8_t invalid_sample_rate[] = {
        0xFF, 0xF8,  // Valid sync
        0x1F,        // Valid block size + invalid sample rate 1111
        0x10,        // Valid channel assignment and sample size
        0x00
    };
    
    analysis = validator.validateFrame(invalid_sample_rate, sizeof(invalid_sample_rate), 2, 0);
    assert(!analysis.sample_rate_valid);
    assert(!analysis.is_compliant);
    std::cout << "✓ Invalid sample rate correctly rejected" << std::endl;
    
    // Test reserved sample size (0x03)
    uint8_t reserved_sample_size[] = {
        0xFF, 0xF8,  // Valid sync
        0x10,        // Valid block size and sample rate
        0x16,        // Valid channel assignment + reserved sample size 011 + reserved bit 0
        0x00
    };
    
    analysis = validator.validateFrame(reserved_sample_size, sizeof(reserved_sample_size), 3, 0);
    assert(!analysis.sample_size_valid);
    assert(!analysis.is_compliant);
    std::cout << "✓ Reserved sample size correctly rejected" << std::endl;
}

/**
 * @brief Test RFC 9639 channel assignment validation
 */
void test_channel_assignment_validation() {
    std::cout << "Testing RFC 9639 channel assignment validation..." << std::endl;
    
    FLACRFCComplianceValidator validator;
    
    // Test valid independent channels (0-7)
    for (uint8_t channels = 0; channels < 8; channels++) {
        uint8_t valid_independent[] = {
            0xFF, 0xF8,  // Valid sync
            0x10,        // Valid block size and sample rate
            static_cast<uint8_t>((channels << 4) | 0x00),  // Channel assignment + valid sample size
            0x00
        };
        
        FrameComplianceAnalysis analysis = validator.validateFrame(valid_independent, sizeof(valid_independent), channels, 0);
        assert(analysis.channel_assignment_valid);
        assert(analysis.is_compliant);
    }
    std::cout << "✓ Valid independent channel assignments correctly validated" << std::endl;
    
    // Test valid stereo modes (8-10) with 2 channels
    const char* stereo_modes[] = {"left-side", "right-side", "mid-side"};
    for (uint8_t mode = 8; mode <= 10; mode++) {
        uint8_t valid_stereo[] = {
            0xFF, 0xF8,  // Valid sync
            0x10,        // Valid block size and sample rate
            static_cast<uint8_t>((mode << 4) | 0x00),  // Stereo mode + valid sample size
            0x00
        };
        
        FrameComplianceAnalysis analysis = validator.validateFrame(valid_stereo, sizeof(valid_stereo), mode, 0);
        assert(analysis.channel_assignment_valid);
        std::cout << "✓ Valid " << stereo_modes[mode - 8] << " stereo mode correctly validated" << std::endl;
    }
    
    // Test reserved channel assignments (11-15)
    for (uint8_t reserved = 11; reserved <= 15; reserved++) {
        uint8_t reserved_channel[] = {
            0xFF, 0xF8,  // Valid sync
            0x10,        // Valid block size and sample rate
            static_cast<uint8_t>((reserved << 4) | 0x00),  // Reserved channel assignment
            0x00
        };
        
        FrameComplianceAnalysis analysis = validator.validateFrame(reserved_channel, sizeof(reserved_channel), reserved, 0);
        assert(!analysis.channel_assignment_valid);
        assert(!analysis.is_compliant);
    }
    std::cout << "✓ Reserved channel assignments correctly rejected" << std::endl;
}

/**
 * @brief Test bit-level analysis tools
 */
void test_bit_level_analysis() {
    std::cout << "Testing bit-level analysis tools..." << std::endl;
    
    // Test frame header dump
    uint8_t test_header[] = {
        0xFF, 0xF8,  // Sync pattern + reserved bit + blocking strategy
        0x19,        // Block size 0001 (192) + sample rate 1001 (44.1kHz)
        0x20,        // Channel assignment 0010 (3 channels) + sample size 000 (from STREAMINFO) + reserved bit 0
        0x00         // Frame number
    };
    
    std::string dump = BitLevelAnalyzer::dumpFrameHeader(test_header, sizeof(test_header));
    assert(!dump.empty());
    assert(dump.find("RFC 9639 Frame Header Analysis") != std::string::npos);
    assert(dump.find("Sync Pattern: 0x3ffe (VALID)") != std::string::npos);
    assert(dump.find("192 samples") != std::string::npos);
    assert(dump.find("44100 Hz") != std::string::npos);
    std::cout << "✓ Frame header dump correctly generated" << std::endl;
    
    // Test CRC calculation
    uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t crc8 = BitLevelAnalyzer::calculateCRC8(test_data, sizeof(test_data));
    uint16_t crc16 = BitLevelAnalyzer::calculateCRC16(test_data, sizeof(test_data));
    
    // CRC values should be non-zero for non-zero input
    assert(crc8 != 0);
    assert(crc16 != 0);
    std::cout << "✓ CRC calculation functions working" << std::endl;
}

/**
 * @brief Test performance monitoring
 */
void test_performance_monitoring() {
    std::cout << "Testing performance monitoring..." << std::endl;
    
    FLACRFCComplianceValidator validator;
    validator.setRealTimeValidation(true, 50); // 50μs threshold
    
    // Validate multiple frames to test performance tracking
    uint8_t test_frame[] = {
        0xFF, 0xF8, 0x19, 0x20, 0x00
    };
    
    for (size_t i = 0; i < 10; i++) {
        FrameComplianceAnalysis analysis = validator.validateFrame(test_frame, sizeof(test_frame), i, i * 100);
        assert(analysis.is_compliant);
    }
    
    // Get statistics
    auto stats = validator.getViolationStats();
    assert(stats.total_frames_analyzed == 10);
    assert(stats.compliant_frames == 10);
    assert(stats.compliance_percentage == 100.0);
    
    std::cout << "✓ Performance monitoring working correctly" << std::endl;
    std::cout << "  Analyzed " << stats.total_frames_analyzed << " frames" << std::endl;
    std::cout << "  Compliance: " << stats.compliance_percentage << "%" << std::endl;
}

/**
 * @brief Test compliance report generation
 */
void test_compliance_report_generation() {
    std::cout << "Testing compliance report generation..." << std::endl;
    
    FLACRFCComplianceValidator validator;
    
    // Add some valid frames
    uint8_t valid_frame[] = {0xFF, 0xF8, 0x19, 0x20, 0x00};
    validator.validateFrame(valid_frame, sizeof(valid_frame), 0, 0);
    
    // Add some invalid frames to create violations
    uint8_t invalid_frame[] = {0xFF, 0xF0, 0x00, 0x00, 0x00}; // Invalid sync
    validator.validateFrame(invalid_frame, sizeof(invalid_frame), 1, 100);
    
    // Generate report
    std::string report = validator.generateComplianceReport();
    assert(!report.empty());
    assert(report.find("RFC 9639 FLAC Compliance Report") != std::string::npos);
    assert(report.find("Total frames analyzed: 2") != std::string::npos);
    assert(report.find("Compliant frames: 1") != std::string::npos);
    assert(report.find("Violation Details:") != std::string::npos);
    
    std::cout << "✓ Compliance report correctly generated" << std::endl;
}

/**
 * @brief Test global RFC validator singleton
 */
void test_global_rfc_validator() {
    std::cout << "Testing global RFC validator..." << std::endl;
    
    // Test quick compliance check
    uint8_t valid_frame[] = {0xFF, 0xF8, 0x19, 0x20, 0x00};
    bool is_compliant = GlobalRFCValidator::quickComplianceCheck(valid_frame, sizeof(valid_frame), 0);
    assert(is_compliant);
    std::cout << "✓ Quick compliance check for valid frame passed" << std::endl;
    
    uint8_t invalid_frame[] = {0xFF, 0xF0, 0x00, 0x00, 0x00}; // Invalid sync
    is_compliant = GlobalRFCValidator::quickComplianceCheck(invalid_frame, sizeof(invalid_frame), 1);
    assert(!is_compliant);
    std::cout << "✓ Quick compliance check for invalid frame failed as expected" << std::endl;
    
    // Test violation logging
    GlobalRFCValidator::logViolation("9.1", "Test violation", "Test description", 
                                   "Expected value", "Actual value", 0, 0);
    
    // Get instance and check violation was logged
    FLACRFCComplianceValidator& instance = GlobalRFCValidator::getInstance();
    auto stats = instance.getViolationStats();
    assert(stats.total_violations > 0);
    std::cout << "✓ Global violation logging working" << std::endl;
}

/**
 * @brief Run all RFC 9639 compliance validator tests
 */
int main() {
    std::cout << "Running RFC 9639 Compliance Validator Tests" << std::endl;
    std::cout << "===========================================" << std::endl;
    
    try {
        test_sync_pattern_validation();
        test_frame_header_validation();
        test_channel_assignment_validation();
        test_bit_level_analysis();
        test_performance_monitoring();
        test_compliance_report_generation();
        test_global_rfc_validator();
        
        std::cout << "\n✅ All RFC 9639 compliance validator tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}

#else

int main() {
    std::cout << "FLAC support not available - skipping RFC 9639 compliance validator tests" << std::endl;
    return 0;
}

#endif // HAVE_FLAC