/*
 * test_rfc_compliance_debugging_tools.cpp - Test RFC 9639 compliance debugging and validation tools
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

#include "FLACRFCComplianceValidator.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <chrono>

// Test comprehensive RFC compliance debugging and validation tools
void test_comprehensive_rfc_compliance_validator() {
    std::cout << "Testing comprehensive RFC 9639 compliance validator..." << std::endl;
    
    FLACRFCComplianceValidator validator;
    
    // Configure validator for comprehensive testing
    validator.setRealTimeValidation(true, 100); // 100μs threshold
    validator.setValidationCategories(true, true, true, true, true, true);
    validator.setMaxViolationHistory(1000);
    
    // Test valid FLAC frame header
    uint8_t valid_frame[] = {
        0xFF, 0xF8,  // Sync pattern (0x3FFE) + reserved bit (0) + blocking strategy (0)
        0x69, 0x0C,  // Block size (1152) + sample rate (44.1kHz) + channels (stereo) + sample size (16-bit)
        0x00,        // Frame number (simplified)
        0x12,        // CRC-8 (placeholder)
        // Frame data would follow...
        0x34, 0x56   // CRC-16 (placeholder)
    };
    
    auto analysis = validator.validateFrame(valid_frame, sizeof(valid_frame), 0, 0);
    
    std::cout << "Valid frame analysis: " << analysis.getComplianceSummary() << std::endl;
    
    // Test invalid frame with multiple violations
    uint8_t invalid_frame[] = {
        0xFF, 0xFA,  // Invalid sync pattern + reserved bit violation
        0x0F, 0x0F,  // Reserved block size + invalid sample rate
        0x07,        // Reserved sample size + reserved bit violation
        0x00,        // Frame number
        0x00, 0x00   // Invalid CRC
    };
    
    analysis = validator.validateFrame(invalid_frame, sizeof(invalid_frame), 1, 8);
    
    std::cout << "Invalid frame analysis: " << analysis.getComplianceSummary() << std::endl;
    std::cout << "Violations found: " << analysis.violations.size() << std::endl;
    
    for (const auto& violation : analysis.violations) {
        std::cout << "  " << violation.toString() << std::endl;
    }
    
    // Test sample validation
    int16_t test_samples[] = {-32768, -1000, 0, 1000, 32767, static_cast<int16_t>(40000)}; // Last sample out of range (will wrap)
    auto sample_analysis = validator.validateSamples(test_samples, 6, 2, 16, 16);
    
    std::cout << "Sample validation: " << sample_analysis.getComplianceSummary() << std::endl;
    
    // Generate compliance report
    std::string report = validator.generateComplianceReport();
    std::cout << "\nCompliance Report:\n" << report << std::endl;
    
    // Test violation statistics
    auto stats = validator.getViolationStats();
    std::cout << "Violation Statistics: " << stats.toString() << std::endl;
    
    std::cout << "Comprehensive RFC compliance validator test completed." << std::endl;
}

void test_bit_level_analyzer() {
    std::cout << "Testing bit-level analyzer tools..." << std::endl;
    
    // Test frame header analysis
    uint8_t test_frame[] = {
        0xFF, 0xF8,  // Valid sync pattern
        0x69, 0x0C,  // Block size 1152, sample rate 44.1kHz, stereo, 16-bit
        0x00, 0x12   // Frame number + CRC-8
    };
    
    auto analysis = BitLevelAnalyzer::analyzeFrameHeader(test_frame, sizeof(test_frame), 0, 0);
    
    std::cout << "Frame header compliance: " << analysis.getComplianceSummary() << std::endl;
    std::cout << "Sync pattern valid: " << (analysis.sync_pattern_valid ? "YES" : "NO") << std::endl;
    std::cout << "Block size valid: " << (analysis.block_size_valid ? "YES" : "NO") << std::endl;
    std::cout << "Sample rate valid: " << (analysis.sample_rate_valid ? "YES" : "NO") << std::endl;
    std::cout << "Channel assignment valid: " << (analysis.channel_assignment_valid ? "YES" : "NO") << std::endl;
    
    // Test frame header dump
    std::string header_dump = BitLevelAnalyzer::dumpFrameHeader(test_frame, sizeof(test_frame));
    std::cout << "\nFrame Header Dump:\n" << header_dump << std::endl;
    
    // Test CRC calculations
    uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t crc8 = BitLevelAnalyzer::calculateCRC8(test_data, sizeof(test_data));
    uint16_t crc16 = BitLevelAnalyzer::calculateCRC16(test_data, sizeof(test_data));
    
    std::cout << "CRC-8 of test data: 0x" << std::hex << static_cast<int>(crc8) << std::endl;
    std::cout << "CRC-16 of test data: 0x" << std::hex << crc16 << std::dec << std::endl;
    
    std::cout << "Bit-level analyzer test completed." << std::endl;
}

void test_global_rfc_validator() {
    std::cout << "Testing global RFC validator..." << std::endl;
    
    // Test quick compliance check
    uint8_t valid_frame[] = {0xFF, 0xF8, 0x69, 0x0C, 0x00};
    uint8_t invalid_frame[] = {0xFF, 0xFA, 0x0F, 0x0F, 0x07};
    
    bool valid_result = GlobalRFCValidator::quickComplianceCheck(valid_frame, sizeof(valid_frame), 0);
    bool invalid_result = GlobalRFCValidator::quickComplianceCheck(invalid_frame, sizeof(invalid_frame), 1);
    
    std::cout << "Quick check - valid frame: " << (valid_result ? "PASS" : "FAIL") << std::endl;
    std::cout << "Quick check - invalid frame: " << (invalid_result ? "PASS" : "FAIL") << std::endl;
    
    // Test violation logging
    GlobalRFCValidator::logViolation("9.1.2", "Test violation", "Testing violation logging",
                                   "Expected value", "Actual value", 0, 0);
    
    // Get global instance and check statistics
    auto& global_validator = GlobalRFCValidator::getInstance();
    auto stats = global_validator.getViolationStats();
    
    std::cout << "Global validator statistics: " << stats.toString() << std::endl;
    
    std::cout << "Global RFC validator test completed." << std::endl;
}

void test_performance_monitoring() {
    std::cout << "Testing performance monitoring..." << std::endl;
    
    FLACRFCComplianceValidator validator;
    validator.setRealTimeValidation(true, 50); // 50μs threshold for testing
    
    // Test with various frame sizes to monitor performance
    std::vector<size_t> frame_sizes = {64, 128, 256, 512, 1024};
    
    for (size_t size : frame_sizes) {
        std::vector<uint8_t> test_frame(size, 0);
        // Set up valid sync pattern
        test_frame[0] = 0xFF;
        test_frame[1] = 0xF8;
        test_frame[2] = 0x69;
        test_frame[3] = 0x0C;
        
        auto start = std::chrono::high_resolution_clock::now();
        auto analysis = validator.validateFrame(test_frame.data(), test_frame.size(), 0, 0);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Frame size " << size << " bytes: " << duration.count() << "μs";
        std::cout << " (" << (duration.count() <= 50 ? "WITHIN" : "EXCEEDS") << " threshold)" << std::endl;
    }
    
    std::cout << "Performance monitoring test completed." << std::endl;
}

void test_rfc_compliance_test_suite_generation() {
    std::cout << "Testing RFC compliance test suite generation..." << std::endl;
    
    FLACRFCComplianceValidator validator;
    
    // Test suite generation (simplified - just logs what would be created)
    bool success = validator.createRFCComplianceTestSuite("/tmp/flac_rfc_tests");
    
    std::cout << "Test suite generation: " << (success ? "SUCCESS" : "FAILED") << std::endl;
    
    std::cout << "RFC compliance test suite generation test completed." << std::endl;
}

int main() {
    std::cout << "RFC 9639 Compliance Debugging and Validation Tools Test Suite" << std::endl;
    std::cout << "=============================================================" << std::endl;
    
    try {
        test_comprehensive_rfc_compliance_validator();
        std::cout << std::endl;
        
        test_bit_level_analyzer();
        std::cout << std::endl;
        
        test_global_rfc_validator();
        std::cout << std::endl;
        
        test_performance_monitoring();
        std::cout << std::endl;
        
        test_rfc_compliance_test_suite_generation();
        std::cout << std::endl;
        
        std::cout << "All RFC compliance debugging and validation tools tests completed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}

#else

#include <iostream>

int main() {
    std::cout << "FLAC support not available - skipping RFC compliance debugging tools tests" << std::endl;
    return 0;
}

#endif // HAVE_FLAC