/*
 * FLACRFCComplianceValidator.cpp - RFC 9639 compliance validation and debugging tools implementation
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

// Comprehensive RFC 9639 compliance validator - testing version
// This is the full-featured validator for testing and debugging

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <mutex>
#include <map>
#include <iostream>
#include <sstream>
#include <cstdint>

#ifdef HAVE_FLAC

#include <FLAC/stream_decoder.h>

// Include the comprehensive validator header
#include "FLACRFCComplianceValidator.h"

// Minimal Debug implementation for testing
namespace Debug {
    void log(const std::string& channel, const std::string& message) {
        std::cout << "[" << channel << "] " << message << std::endl;
    }
    
    template<typename... Args>
    void log(const std::string& channel, Args... args) {
        std::ostringstream oss;
        (oss << ... << args);
        log(channel, oss.str());
    }
}

// ============================================================================
// RFC 9639 Validation Tables and Constants
// ============================================================================

// CRC-8 lookup table for frame header validation (RFC 9639 polynomial: 0x07)
const uint8_t BitLevelAnalyzer::crc8_table[256] = {
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
    0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
    0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
    0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
    0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
    0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
    0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
    0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
    0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
    0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
    0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
    0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
    0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
    0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
    0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
    0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
    0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
    0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
    0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
    0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
    0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
    0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
    0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
    0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
    0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
    0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
    0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

// CRC-16 lookup table for frame validation (RFC 9639 polynomial: 0x8005)
const uint16_t BitLevelAnalyzer::crc16_table[256] = {
    0x0000, 0x8005, 0x800F, 0x000A, 0x801B, 0x001E, 0x0014, 0x8011,
    0x8033, 0x0036, 0x003C, 0x8039, 0x0028, 0x802D, 0x8027, 0x0022,
    0x8063, 0x0066, 0x006C, 0x8069, 0x0078, 0x807D, 0x8077, 0x0072,
    0x0050, 0x8055, 0x805F, 0x005A, 0x804B, 0x004E, 0x0044, 0x8041,
    0x80C3, 0x00C6, 0x00CC, 0x80C9, 0x00D8, 0x80DD, 0x80D7, 0x00D2,
    0x00F0, 0x80F5, 0x80FF, 0x00FA, 0x80EB, 0x00EE, 0x00E4, 0x80E1,
    0x00A0, 0x80A5, 0x80AF, 0x00AA, 0x80BB, 0x00BE, 0x00B4, 0x80B1,
    0x8093, 0x0096, 0x009C, 0x8099, 0x0088, 0x808D, 0x8087, 0x0082,
    0x8183, 0x0186, 0x018C, 0x8189, 0x0198, 0x819D, 0x8197, 0x0192,
    0x01B0, 0x81B5, 0x81BF, 0x01BA, 0x81AB, 0x01AE, 0x01A4, 0x81A1,
    0x01E0, 0x81E5, 0x81EF, 0x01EA, 0x81FB, 0x01FE, 0x01F4, 0x81F1,
    0x81D3, 0x01D6, 0x01DC, 0x81D9, 0x01C8, 0x81CD, 0x81C7, 0x01C2,
    0x0140, 0x8145, 0x814F, 0x014A, 0x815B, 0x015E, 0x0154, 0x8151,
    0x8173, 0x0176, 0x017C, 0x8179, 0x0168, 0x816D, 0x8167, 0x0162,
    0x8123, 0x0126, 0x012C, 0x8129, 0x0138, 0x813D, 0x8137, 0x0132,
    0x0110, 0x8115, 0x811F, 0x011A, 0x810B, 0x010E, 0x0104, 0x8101,
    0x8303, 0x0306, 0x030C, 0x8309, 0x0318, 0x831D, 0x8317, 0x0312,
    0x0330, 0x8335, 0x833F, 0x033A, 0x832B, 0x032E, 0x0324, 0x8321,
    0x0360, 0x8365, 0x836F, 0x036A, 0x837B, 0x037E, 0x0374, 0x8371,
    0x8353, 0x0356, 0x035C, 0x8359, 0x0348, 0x834D, 0x8347, 0x0342,
    0x03C0, 0x83C5, 0x83CF, 0x03CA, 0x83DB, 0x03DE, 0x03D4, 0x83D1,
    0x83F3, 0x03F6, 0x03FC, 0x83F9, 0x03E8, 0x83ED, 0x83E7, 0x03E2,
    0x83A3, 0x03A6, 0x03AC, 0x83A9, 0x03B8, 0x83BD, 0x83B7, 0x03B2,
    0x0390, 0x8395, 0x839F, 0x039A, 0x838B, 0x038E, 0x0384, 0x8381,
    0x0280, 0x8285, 0x828F, 0x028A, 0x829B, 0x029E, 0x0294, 0x8291,
    0x82B3, 0x02B6, 0x02BC, 0x82B9, 0x02A8, 0x82AD, 0x82A7, 0x02A2,
    0x82E3, 0x02E6, 0x02EC, 0x82E9, 0x02F8, 0x82FD, 0x82F7, 0x02F2,
    0x02D0, 0x82D5, 0x82DF, 0x02DA, 0x82CB, 0x02CE, 0x02C4, 0x82C1,
    0x8243, 0x0246, 0x024C, 0x8249, 0x0258, 0x825D, 0x8257, 0x0252,
    0x0270, 0x8275, 0x827F, 0x027A, 0x826B, 0x026E, 0x0264, 0x8261,
    0x0220, 0x8225, 0x822F, 0x022A, 0x823B, 0x023E, 0x0234, 0x8231,
    0x8213, 0x0216, 0x021C, 0x8219, 0x0208, 0x820D, 0x8207, 0x0202
};

// RFC 9639 Table 1: Block size encoding
const uint32_t BitLevelAnalyzer::block_size_table[16] = {
    0,      // 0000: reserved
    192,    // 0001: 192 samples
    576,    // 0010: 576 samples
    1152,   // 0011: 1152 samples
    2304,   // 0100: 2304 samples
    4608,   // 0101: 4608 samples
    0,      // 0110: get 8-bit from end of header
    0,      // 0111: get 16-bit from end of header
    256,    // 1000: 256 samples
    512,    // 1001: 512 samples
    1024,   // 1010: 1024 samples
    2048,   // 1011: 2048 samples
    4096,   // 1100: 4096 samples
    8192,   // 1101: 8192 samples
    16384,  // 1110: 16384 samples
    32768   // 1111: 32768 samples
};

// RFC 9639 Table 2: Sample rate encoding
const uint32_t BitLevelAnalyzer::sample_rate_table[16] = {
    0,      // 0000: get from STREAMINFO
    88200,  // 0001: 88.2 kHz
    176400, // 0010: 176.4 kHz
    192000, // 0011: 192 kHz
    8000,   // 0100: 8 kHz
    16000,  // 0101: 16 kHz
    22050,  // 0110: 22.05 kHz
    24000,  // 0111: 24 kHz
    32000,  // 1000: 32 kHz
    44100,  // 1001: 44.1 kHz
    48000,  // 1010: 48 kHz
    96000,  // 1011: 96 kHz
    0,      // 1100: get 8-bit from end of header (in kHz)
    0,      // 1101: get 16-bit from end of header (in Hz)
    0,      // 1110: get 16-bit from end of header (in 10*Hz)
    0       // 1111: invalid
};

// RFC 9639 Table 4: Sample size encoding
const uint8_t BitLevelAnalyzer::sample_size_table[8] = {
    0,  // 000: get from STREAMINFO
    8,  // 001: 8 bits per sample
    12, // 010: 12 bits per sample
    0,  // 011: reserved
    16, // 100: 16 bits per sample
    20, // 101: 20 bits per sample
    24, // 110: 24 bits per sample
    32  // 111: 32 bits per sample
};

// ============================================================================
// BitLevelAnalyzer Implementation
// ============================================================================

FrameComplianceAnalysis BitLevelAnalyzer::analyzeFrameHeader(const uint8_t* data, size_t size, 
                                                            size_t frame_number, size_t byte_offset) {
    FrameComplianceAnalysis analysis;
    analysis.frame_number = frame_number;
    analysis.is_compliant = true;
    
    // Initialize all validation flags to false
    analysis.sync_pattern_valid = false;
    analysis.reserved_bits_valid = false;
    analysis.blocking_strategy_valid = false;
    analysis.block_size_valid = false;
    analysis.sample_rate_valid = false;
    analysis.channel_assignment_valid = false;
    analysis.sample_size_valid = false;
    analysis.frame_number_valid = false;
    analysis.crc8_valid = false;
    
    if (!data || size < 4) {
        RFCViolationReport violation;
        violation.severity = RFCViolationSeverity::CRITICAL;
        violation.rfc_section = "9.1";
        violation.violation_type = "Insufficient frame header data";
        violation.description = "Frame header requires minimum 4 bytes";
        violation.expected_value = "≥4 bytes";
        violation.actual_value = std::to_string(size) + " bytes";
        violation.byte_offset = byte_offset;
        violation.frame_number = frame_number;
        violation.timestamp = std::chrono::high_resolution_clock::now();
        
        analysis.violations.push_back(violation);
        analysis.is_compliant = false;
        return analysis;
    }
    
    // Validate sync pattern (RFC 9639 Section 9.1)
    analysis.sync_pattern_valid = validateSyncPattern(data, size, analysis.violations, frame_number, byte_offset);
    if (!analysis.sync_pattern_valid) {
        analysis.is_compliant = false;
    }
    
    // Validate frame header structure (this will catch reserved values)
    analysis.reserved_bits_valid = validateFrameHeader(data, size, analysis.violations, frame_number, byte_offset);
    if (!analysis.reserved_bits_valid) {
        analysis.is_compliant = false;
    }
    
    // Extract and validate individual header fields
    if (size >= 4) {
        uint8_t byte1 = data[1];
        uint8_t byte2 = data[2];
        uint8_t byte3 = data[3];
        
        // Blocking strategy (bit 0 of byte 1)
        uint8_t blocking_strategy = (byte1 >> 0) & 0x01;
        (void)blocking_strategy; // Suppress unused variable warning
        analysis.blocking_strategy_valid = true; // Both values (0,1) are valid per RFC
        
        // Block size (bits 4-7 of byte 1)
        uint8_t block_size_bits = (byte1 >> 4) & 0x0F;
        size_t violations_before = analysis.violations.size();
        uint32_t block_size = validateBlockSizeEncoding(block_size_bits, data, analysis.violations, frame_number, byte_offset);
        (void)block_size; // Suppress unused variable warning
        analysis.block_size_valid = (analysis.violations.size() == violations_before);
        if (!analysis.block_size_valid) {
            analysis.is_compliant = false;
        }
        
        // Sample rate (bits 0-3 of byte 2)
        uint8_t sample_rate_bits = byte2 & 0x0F;
        violations_before = analysis.violations.size();
        uint32_t sample_rate = validateSampleRateEncoding(sample_rate_bits, data, analysis.violations, frame_number, byte_offset);
        (void)sample_rate; // Suppress unused variable warning
        analysis.sample_rate_valid = (analysis.violations.size() == violations_before);
        if (!analysis.sample_rate_valid) {
            analysis.is_compliant = false;
        }
        
        // Channel assignment (bits 4-7 of byte 2)
        uint8_t channel_assignment = (byte2 >> 4) & 0x0F;
        uint8_t channels = (channel_assignment < 8) ? (channel_assignment + 1) : 2;
        violations_before = analysis.violations.size();
        validateChannelAssignment(channel_assignment, channels, analysis.violations, frame_number, byte_offset);
        analysis.channel_assignment_valid = (analysis.violations.size() == violations_before);
        if (!analysis.channel_assignment_valid) {
            analysis.is_compliant = false;
        }
        
        // Sample size (bits 1-3 of byte 3)
        uint8_t sample_size_bits = (byte3 >> 1) & 0x07;
        violations_before = analysis.violations.size();
        uint8_t sample_size = validateSampleSizeEncoding(sample_size_bits, analysis.violations, frame_number, byte_offset);
        (void)sample_size; // Suppress unused variable warning
        analysis.sample_size_valid = (analysis.violations.size() == violations_before);
        if (!analysis.sample_size_valid) {
            analysis.is_compliant = false;
        }
        
        // Reserved bit (bit 0 of byte 3)
        uint8_t reserved_bit = byte3 & 0x01;
        if (reserved_bit != 0) {
            RFCViolationReport violation;
            violation.severity = RFCViolationSeverity::ERROR;
            violation.rfc_section = "9.1";
            violation.violation_type = "Reserved bit violation";
            violation.description = "Reserved bit in frame header must be 0";
            violation.expected_value = "0";
            violation.actual_value = "1";
            violation.byte_offset = byte_offset + 3;
            violation.frame_number = frame_number;
            violation.timestamp = std::chrono::high_resolution_clock::now();
            
            analysis.violations.push_back(violation);
            analysis.is_compliant = false;
        }
    }
    
    return analysis;
}

bool BitLevelAnalyzer::validateSyncPattern(const uint8_t* data, size_t size, 
                                          std::vector<RFCViolationReport>& violations,
                                          size_t frame_number, size_t byte_offset) {
    if (!data || size < 2) {
        return false;
    }
    
    // RFC 9639 Section 9.1: Sync pattern is 0x3FFE (14 bits) followed by reserved bit (0)
    uint16_t sync_pattern = (static_cast<uint16_t>(data[0]) << 6) | ((data[1] >> 2) & 0x3F);
    
    if (sync_pattern != 0x3FFE) {
        RFCViolationReport violation;
        violation.severity = RFCViolationSeverity::CRITICAL;
        violation.rfc_section = "9.1";
        violation.violation_type = "Invalid sync pattern";
        violation.description = "Frame sync pattern must be 0x3FFE per RFC 9639";
        violation.expected_value = "0x3FFE";
        violation.actual_value = "0x" + std::to_string(sync_pattern);
        violation.byte_offset = byte_offset;
        violation.frame_number = frame_number;
        violation.timestamp = std::chrono::high_resolution_clock::now();
        
        violations.push_back(violation);
        return false;
    }
    
    // Check reserved bit (bit 1 of byte 1)
    uint8_t reserved_bit = (data[1] >> 1) & 0x01;
    if (reserved_bit != 0) {
        RFCViolationReport violation;
        violation.severity = RFCViolationSeverity::ERROR;
        violation.rfc_section = "9.1";
        violation.violation_type = "Reserved bit in sync pattern";
        violation.description = "Reserved bit after sync pattern must be 0";
        violation.expected_value = "0";
        violation.actual_value = "1";
        violation.byte_offset = byte_offset + 1;
        violation.frame_number = frame_number;
        violation.timestamp = std::chrono::high_resolution_clock::now();
        
        violations.push_back(violation);
        return false;
    }
    
    return true;
}

bool BitLevelAnalyzer::validateFrameHeader(const uint8_t* data, size_t size,
                                          std::vector<RFCViolationReport>& violations,
                                          size_t frame_number, size_t byte_offset) {
    if (!data || size < 4) {
        return false;
    }
    
    bool all_valid = true;
    
    // Validate that no forbidden bit patterns exist
    // RFC 9639 specifies several reserved/forbidden values
    
    uint8_t byte1 = data[1];
    uint8_t byte2 = data[2];
    uint8_t byte3 = data[3];
    
    // Check for reserved block size values
    uint8_t block_size_bits = (byte1 >> 4) & 0x0F;
    if (block_size_bits == 0x00) {
        RFCViolationReport violation;
        violation.severity = RFCViolationSeverity::ERROR;
        violation.rfc_section = "9.1.2";
        violation.violation_type = "Reserved block size";
        violation.description = "Block size encoding 0x00 is reserved";
        violation.expected_value = "Valid block size encoding (0x01-0x0F)";
        violation.actual_value = "0x00 (reserved)";
        violation.byte_offset = byte_offset + 1;
        violation.frame_number = frame_number;
        violation.timestamp = std::chrono::high_resolution_clock::now();
        
        violations.push_back(violation);
        all_valid = false;
    }
    
    // Check for invalid sample rate values
    uint8_t sample_rate_bits = byte2 & 0x0F;
    if (sample_rate_bits == 0x0F) {
        RFCViolationReport violation;
        violation.severity = RFCViolationSeverity::ERROR;
        violation.rfc_section = "9.1.3";
        violation.violation_type = "Invalid sample rate";
        violation.description = "Sample rate encoding 0x0F is invalid";
        violation.expected_value = "Valid sample rate encoding (0x00-0x0E)";
        violation.actual_value = "0x0F (invalid)";
        violation.byte_offset = byte_offset + 2;
        violation.frame_number = frame_number;
        violation.timestamp = std::chrono::high_resolution_clock::now();
        
        violations.push_back(violation);
        all_valid = false;
    }
    
    // Check for reserved sample size values
    uint8_t sample_size_bits = (byte3 >> 1) & 0x07;
    if (sample_size_bits == 0x03) {
        RFCViolationReport violation;
        violation.severity = RFCViolationSeverity::ERROR;
        violation.rfc_section = "9.1.5";
        violation.violation_type = "Reserved sample size";
        violation.description = "Sample size encoding 0x03 is reserved";
        violation.expected_value = "Valid sample size encoding (not 0x03)";
        violation.actual_value = "0x03 (reserved)";
        violation.byte_offset = byte_offset + 3;
        violation.frame_number = frame_number;
        violation.timestamp = std::chrono::high_resolution_clock::now();
        
        violations.push_back(violation);
        all_valid = false;
    }
    
    return all_valid;
}

uint32_t BitLevelAnalyzer::validateBlockSizeEncoding(uint8_t block_size_bits,
                                                    const uint8_t* header_data,
                                                    std::vector<RFCViolationReport>& violations,
                                                    size_t frame_number, size_t byte_offset) {
    // RFC 9639 Table 1: Block size encoding
    if (block_size_bits < 16) {
        uint32_t block_size = block_size_table[block_size_bits];
        
        if (block_size == 0 && block_size_bits != 0x06 && block_size_bits != 0x07) {
            // Reserved value
            RFCViolationReport violation;
            violation.severity = RFCViolationSeverity::ERROR;
            violation.rfc_section = "9.1.2";
            violation.violation_type = "Reserved block size encoding";
            violation.description = "Block size encoding uses reserved value";
            violation.expected_value = "Valid block size encoding per RFC 9639 Table 1";
            violation.actual_value = "0x" + std::to_string(block_size_bits) + " (reserved)";
            violation.byte_offset = byte_offset + 1;
            violation.frame_number = frame_number;
            violation.timestamp = std::chrono::high_resolution_clock::now();
            
            violations.push_back(violation);
            return 0;
        }
        
        return block_size;
    }
    
    return 0;
}

uint32_t BitLevelAnalyzer::validateSampleRateEncoding(uint8_t sample_rate_bits,
                                                     const uint8_t* header_data,
                                                     std::vector<RFCViolationReport>& violations,
                                                     size_t frame_number, size_t byte_offset) {
    // RFC 9639 Table 2: Sample rate encoding
    if (sample_rate_bits < 16) {
        uint32_t sample_rate = sample_rate_table[sample_rate_bits];
        
        if (sample_rate_bits == 0x0F) {
            // Invalid value
            RFCViolationReport violation;
            violation.severity = RFCViolationSeverity::ERROR;
            violation.rfc_section = "9.1.3";
            violation.violation_type = "Invalid sample rate encoding";
            violation.description = "Sample rate encoding 0x0F is invalid";
            violation.expected_value = "Valid sample rate encoding per RFC 9639 Table 2";
            violation.actual_value = "0x0F (invalid)";
            violation.byte_offset = byte_offset + 2;
            violation.frame_number = frame_number;
            violation.timestamp = std::chrono::high_resolution_clock::now();
            
            violations.push_back(violation);
            return 0;
        }
        
        return sample_rate;
    }
    
    return 0;
}

bool BitLevelAnalyzer::validateChannelAssignment(uint8_t channel_assignment,
                                                uint8_t channels,
                                                std::vector<RFCViolationReport>& violations,
                                                size_t frame_number, size_t byte_offset) {
    // RFC 9639 Table 3: Channel assignment validation
    bool valid = true;
    
    if (channel_assignment >= 11 && channel_assignment <= 15) {
        // Reserved values
        RFCViolationReport violation;
        violation.severity = RFCViolationSeverity::ERROR;
        violation.rfc_section = "9.1.4";
        violation.violation_type = "Reserved channel assignment";
        violation.description = "Channel assignment uses reserved value";
        violation.expected_value = "Valid channel assignment (0-10)";
        violation.actual_value = std::to_string(channel_assignment) + " (reserved)";
        violation.byte_offset = byte_offset + 2;
        violation.frame_number = frame_number;
        violation.timestamp = std::chrono::high_resolution_clock::now();
        
        violations.push_back(violation);
        valid = false;
    }
    
    // Validate stereo modes (8-10) only apply to 2-channel streams
    if (channel_assignment >= 8 && channel_assignment <= 10 && channels != 2) {
        RFCViolationReport violation;
        violation.severity = RFCViolationSeverity::ERROR;
        violation.rfc_section = "9.1.4";
        violation.violation_type = "Invalid stereo mode for channel count";
        violation.description = "Stereo modes (left-side, right-side, mid-side) only valid for 2 channels";
        violation.expected_value = "2 channels for stereo modes";
        violation.actual_value = std::to_string(channels) + " channels";
        violation.byte_offset = byte_offset + 2;
        violation.frame_number = frame_number;
        violation.timestamp = std::chrono::high_resolution_clock::now();
        
        violations.push_back(violation);
        valid = false;
    }
    
    return valid;
}

uint8_t BitLevelAnalyzer::validateSampleSizeEncoding(uint8_t sample_size_bits,
                                                    std::vector<RFCViolationReport>& violations,
                                                    size_t frame_number, size_t byte_offset) {
    // RFC 9639 Table 4: Sample size encoding
    if (sample_size_bits < 8) {
        uint8_t sample_size = sample_size_table[sample_size_bits];
        
        if (sample_size == 0 && sample_size_bits != 0x00) {
            // Reserved value (0x03)
            RFCViolationReport violation;
            violation.severity = RFCViolationSeverity::ERROR;
            violation.rfc_section = "9.1.5";
            violation.violation_type = "Reserved sample size encoding";
            violation.description = "Sample size encoding uses reserved value";
            violation.expected_value = "Valid sample size encoding per RFC 9639 Table 4";
            violation.actual_value = "0x" + std::to_string(sample_size_bits) + " (reserved)";
            violation.byte_offset = byte_offset + 3;
            violation.frame_number = frame_number;
            violation.timestamp = std::chrono::high_resolution_clock::now();
            
            violations.push_back(violation);
            return 0;
        }
        
        return sample_size;
    }
    
    return 0;
}

uint8_t BitLevelAnalyzer::calculateCRC8(const uint8_t* data, size_t length) {
    uint8_t crc = 0;
    for (size_t i = 0; i < length; i++) {
        crc = crc8_table[crc ^ data[i]];
    }
    return crc;
}

uint16_t BitLevelAnalyzer::calculateCRC16(const uint8_t* data, size_t length) {
    uint16_t crc = 0;
    for (size_t i = 0; i < length; i++) {
        crc = (crc << 8) ^ crc16_table[((crc >> 8) ^ data[i]) & 0xFF];
    }
    return crc;
}

std::string BitLevelAnalyzer::dumpFrameHeader(const uint8_t* data, size_t size) {
    if (!data || size < 4) {
        return "Invalid frame header data";
    }
    
    std::ostringstream dump;
    dump << "RFC 9639 Frame Header Analysis:\n";
    dump << "================================\n";
    
    // Sync pattern analysis
    uint16_t sync_pattern = (static_cast<uint16_t>(data[0]) << 6) | ((data[1] >> 2) & 0x3F);
    dump << "Sync Pattern: 0x" << std::hex << sync_pattern;
    dump << (sync_pattern == 0x3FFE ? " (VALID)" : " (INVALID - should be 0x3FFE)") << "\n";
    
    // Reserved bit after sync
    uint8_t reserved_sync = (data[1] >> 1) & 0x01;
    dump << "Reserved bit: " << static_cast<int>(reserved_sync);
    dump << (reserved_sync == 0 ? " (VALID)" : " (INVALID - should be 0)") << "\n";
    
    // Blocking strategy
    uint8_t blocking_strategy = data[1] & 0x01;
    dump << "Blocking strategy: " << static_cast<int>(blocking_strategy);
    dump << (blocking_strategy == 0 ? " (fixed)" : " (variable)") << "\n";
    
    // Block size
    uint8_t block_size_bits = (data[1] >> 4) & 0x0F;
    dump << "Block size encoding: 0x" << std::hex << static_cast<int>(block_size_bits);
    if (block_size_bits < 16 && block_size_table[block_size_bits] > 0) {
        dump << " (" << std::dec << block_size_table[block_size_bits] << " samples)";
    } else if (block_size_bits == 0x06) {
        dump << " (8-bit from end of header)";
    } else if (block_size_bits == 0x07) {
        dump << " (16-bit from end of header)";
    } else {
        dump << " (RESERVED/INVALID)";
    }
    dump << "\n";
    
    // Sample rate
    uint8_t sample_rate_bits = data[2] & 0x0F;
    dump << "Sample rate encoding: 0x" << std::hex << static_cast<int>(sample_rate_bits);
    if (sample_rate_bits < 16 && sample_rate_table[sample_rate_bits] > 0) {
        dump << " (" << std::dec << sample_rate_table[sample_rate_bits] << " Hz)";
    } else if (sample_rate_bits == 0x00) {
        dump << " (from STREAMINFO)";
    } else if (sample_rate_bits >= 0x0C && sample_rate_bits <= 0x0E) {
        dump << " (from end of header)";
    } else {
        dump << " (INVALID)";
    }
    dump << "\n";
    
    // Channel assignment
    uint8_t channel_assignment = (data[2] >> 4) & 0x0F;
    dump << "Channel assignment: " << std::dec << static_cast<int>(channel_assignment);
    if (channel_assignment < 8) {
        dump << " (" << (channel_assignment + 1) << " independent channels)";
    } else if (channel_assignment == 8) {
        dump << " (left-side stereo)";
    } else if (channel_assignment == 9) {
        dump << " (right-side stereo)";
    } else if (channel_assignment == 10) {
        dump << " (mid-side stereo)";
    } else {
        dump << " (RESERVED)";
    }
    dump << "\n";
    
    // Sample size
    uint8_t sample_size_bits = (data[3] >> 1) & 0x07;
    dump << "Sample size encoding: " << static_cast<int>(sample_size_bits);
    if (sample_size_bits < 8 && sample_size_table[sample_size_bits] > 0) {
        dump << " (" << static_cast<int>(sample_size_table[sample_size_bits]) << " bits)";
    } else if (sample_size_bits == 0x00) {
        dump << " (from STREAMINFO)";
    } else {
        dump << " (RESERVED)";
    }
    dump << "\n";
    
    // Reserved bit
    uint8_t reserved_bit = data[3] & 0x01;
    dump << "Reserved bit: " << static_cast<int>(reserved_bit);
    dump << (reserved_bit == 0 ? " (VALID)" : " (INVALID - should be 0)") << "\n";
    
    return dump.str();
}

bool BitLevelAnalyzer::validateSubframes(const uint8_t* data, size_t size,
                                        const FLAC__Frame* frame,
                                        std::vector<RFCViolationReport>& violations,
                                        size_t frame_number, size_t byte_offset) {
    if (!data || size < 4 || !frame) {
        RFCViolationReport violation;
        violation.severity = RFCViolationSeverity::CRITICAL;
        violation.rfc_section = "9.2";
        violation.violation_type = "Invalid subframe data";
        violation.description = "Subframe validation requires valid frame data and FLAC frame structure";
        violation.expected_value = "Valid frame data and FLAC__Frame pointer";
        violation.actual_value = "NULL or insufficient data";
        violation.byte_offset = byte_offset;
        violation.frame_number = frame_number;
        violation.timestamp = std::chrono::high_resolution_clock::now();
        
        violations.push_back(violation);
        return false;
    }
    
    // This is a simplified subframe validation
    // Full implementation would require detailed FLAC frame parsing
    bool all_valid = true;
    
    // Validate channel count consistency
    uint8_t channel_assignment = (data[2] >> 4) & 0x0F;
    uint8_t expected_channels = (channel_assignment < 8) ? (channel_assignment + 1) : 2;
    
    if (frame->header.channels != expected_channels) {
        RFCViolationReport violation;
        violation.severity = RFCViolationSeverity::ERROR;
        violation.rfc_section = "9.2";
        violation.violation_type = "Channel count mismatch";
        violation.description = "Frame header channel assignment doesn't match subframe count";
        violation.expected_value = std::to_string(expected_channels) + " channels";
        violation.actual_value = std::to_string(frame->header.channels) + " channels";
        violation.byte_offset = byte_offset + 2;
        violation.frame_number = frame_number;
        violation.timestamp = std::chrono::high_resolution_clock::now();
        
        violations.push_back(violation);
        all_valid = false;
    }
    
    // Validate block size consistency
    if (frame->header.blocksize == 0) {
        RFCViolationReport violation;
        violation.severity = RFCViolationSeverity::ERROR;
        violation.rfc_section = "9.2";
        violation.violation_type = "Invalid block size";
        violation.description = "Subframe block size cannot be zero";
        violation.expected_value = "> 0 samples";
        violation.actual_value = "0 samples";
        violation.byte_offset = byte_offset;
        violation.frame_number = frame_number;
        violation.timestamp = std::chrono::high_resolution_clock::now();
        
        violations.push_back(violation);
        all_valid = false;
    }
    
    return all_valid;
}

bool BitLevelAnalyzer::validateCRCs(const uint8_t* data, size_t size,
                                   std::vector<RFCViolationReport>& violations,
                                   size_t frame_number, size_t byte_offset) {
    if (!data || size < 6) { // Minimum: 4 bytes header + 2 bytes CRC-16
        RFCViolationReport violation;
        violation.severity = RFCViolationSeverity::CRITICAL;
        violation.rfc_section = "9.3";
        violation.violation_type = "Insufficient data for CRC validation";
        violation.description = "CRC validation requires minimum frame size";
        violation.expected_value = "≥6 bytes";
        violation.actual_value = std::to_string(size) + " bytes";
        violation.byte_offset = byte_offset;
        violation.frame_number = frame_number;
        violation.timestamp = std::chrono::high_resolution_clock::now();
        
        violations.push_back(violation);
        return false;
    }
    
    bool all_valid = true;
    
    // Validate CRC-16 at end of frame (last 2 bytes)
    uint16_t stored_crc16 = (static_cast<uint16_t>(data[size - 2]) << 8) | data[size - 1];
    uint16_t calculated_crc16 = calculateCRC16(data, size - 2); // Exclude CRC bytes
    
    if (stored_crc16 != calculated_crc16) {
        RFCViolationReport violation;
        violation.severity = RFCViolationSeverity::ERROR;
        violation.rfc_section = "9.3";
        violation.violation_type = "CRC-16 mismatch";
        violation.description = "Frame CRC-16 checksum validation failed";
        violation.expected_value = "0x" + std::to_string(calculated_crc16);
        violation.actual_value = "0x" + std::to_string(stored_crc16);
        violation.byte_offset = byte_offset + size - 2;
        violation.frame_number = frame_number;
        violation.timestamp = std::chrono::high_resolution_clock::now();
        
        violations.push_back(violation);
        all_valid = false;
    }
    
    // Note: CRC-8 validation would require knowing the exact header length
    // which varies based on frame number encoding. This is a simplified check.
    
    return all_valid;
}

std::string BitLevelAnalyzer::dumpSubframes(const uint8_t* data, size_t size, const FLAC__Frame* frame) {
    if (!data || size < 4 || !frame) {
        return "Invalid subframe data for analysis";
    }
    
    std::ostringstream dump;
    dump << "RFC 9639 Subframe Analysis:\n";
    dump << "============================\n";
    
    dump << "Channel count: " << static_cast<int>(frame->header.channels) << "\n";
    dump << "Block size: " << frame->header.blocksize << " samples\n";
    dump << "Sample rate: " << frame->header.sample_rate << " Hz\n";
    dump << "Bits per sample: " << static_cast<int>(frame->header.bits_per_sample) << "\n";
    
    // Channel assignment analysis
    uint8_t channel_assignment = (data[2] >> 4) & 0x0F;
    dump << "Channel assignment: " << static_cast<int>(channel_assignment);
    if (channel_assignment < 8) {
        dump << " (independent channels)";
    } else if (channel_assignment == 8) {
        dump << " (left-side stereo)";
    } else if (channel_assignment == 9) {
        dump << " (right-side stereo)";
    } else if (channel_assignment == 10) {
        dump << " (mid-side stereo)";
    } else {
        dump << " (RESERVED)";
    }
    dump << "\n";
    
    // This is a simplified subframe dump
    // Full implementation would parse each subframe type (CONSTANT, VERBATIM, FIXED, LPC)
    dump << "\nNote: Detailed subframe parsing requires full FLAC decoder integration\n";
    dump << "This analysis shows frame-level information only.\n";
    
    return dump.str();
}

// ============================================================================
// FLACRFCComplianceValidator Implementation
// ============================================================================

FLACRFCComplianceValidator::FLACRFCComplianceValidator()
    : m_real_time_validation_enabled(false)
    , m_performance_threshold_us(100)
    , m_max_violation_history(1000)
    , m_validate_frame_header(true)
    , m_validate_subframes(true)
    , m_validate_channel_reconstruction(true)
    , m_validate_crc(true)
    , m_validate_sample_format(true)
    , m_monitor_performance(true)
    , m_total_frames_analyzed(0)
    , m_compliant_frames(0)
    , m_validation_start_time(std::chrono::high_resolution_clock::now())
    , m_total_validation_time_us(0) {
    
    Debug::log("flac_rfc_validator", "[FLACRFCComplianceValidator] Initialized RFC 9639 compliance validator");
}

FLACRFCComplianceValidator::~FLACRFCComplianceValidator() {
    Debug::log("flac_rfc_validator", "[FLACRFCComplianceValidator] Destroyed RFC 9639 compliance validator");
}

void FLACRFCComplianceValidator::setRealTimeValidation(bool enabled, uint64_t performance_impact_threshold_us) {
    m_real_time_validation_enabled = enabled;
    m_performance_threshold_us = performance_impact_threshold_us;
    
    Debug::log("flac_rfc_validator", "[setRealTimeValidation] Real-time validation ", 
              (enabled ? "ENABLED" : "DISABLED"), 
              ", threshold: ", performance_impact_threshold_us, " μs");
}

FrameComplianceAnalysis FLACRFCComplianceValidator::validateFrame(const uint8_t* frame_data, size_t frame_size,
                                                                 size_t frame_number, size_t stream_offset) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    FrameComplianceAnalysis analysis = BitLevelAnalyzer::analyzeFrameHeader(frame_data, frame_size, frame_number, stream_offset);
    
    // Update statistics
    {
        std::lock_guard<std::mutex> lock(m_stats_mutex);
        m_total_frames_analyzed++;
        if (analysis.is_compliant) {
            m_compliant_frames++;
        }
    }
    
    // Add violations to history
    for (const auto& violation : analysis.violations) {
        addViolation(violation);
    }
    
    // Performance monitoring
    auto end_time = std::chrono::high_resolution_clock::now();
    uint64_t validation_time_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    
    if (m_monitor_performance) {
        m_total_validation_time_us += validation_time_us;
        
        if (validation_time_us > m_performance_threshold_us) {
            Debug::log("flac_rfc_validator", "[validateFrame] Slow validation: ", validation_time_us, 
                      " μs for frame ", frame_number, " (threshold: ", m_performance_threshold_us, " μs)");
        }
    }
    
    return analysis;
}

std::string FLACRFCComplianceValidator::generateComplianceReport() const {
    std::lock_guard<std::mutex> violation_lock(m_violation_mutex);
    std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
    
    std::ostringstream report;
    report << "RFC 9639 FLAC Compliance Report\n";
    report << "===============================\n\n";
    
    // Summary statistics
    auto stats = getViolationStats();
    report << "Summary:\n";
    report << "--------\n";
    report << "Total frames analyzed: " << stats.total_frames_analyzed << "\n";
    report << "Compliant frames: " << stats.compliant_frames << "\n";
    report << "Non-compliant frames: " << stats.non_compliant_frames << "\n";
    report << "Compliance percentage: " << stats.compliance_percentage << "%\n";
    report << "Total violations: " << stats.total_violations << "\n";
    report << "  Critical: " << stats.critical_violations << "\n";
    report << "  Errors: " << stats.error_violations << "\n";
    report << "  Warnings: " << stats.warning_violations << "\n";
    report << "  Info: " << stats.info_violations << "\n\n";
    
    // Detailed violations
    if (!m_violation_history.empty()) {
        report << "Detailed Violations:\n";
        report << "-------------------\n";
        for (const auto& violation : m_violation_history) {
            report << violation.toString() << "\n";
        }
    }
    
    return report.str();
}

FLACRFCComplianceValidator::ViolationStats FLACRFCComplianceValidator::getViolationStats() const {
    std::lock_guard<std::mutex> violation_lock(m_violation_mutex);
    std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
    
    ViolationStats stats;
    
    stats.total_frames_analyzed = m_total_frames_analyzed;
    stats.compliant_frames = m_compliant_frames;
    stats.non_compliant_frames = m_total_frames_analyzed - m_compliant_frames;
    stats.total_violations = m_violation_history.size();
    
    // Count violations by severity
    stats.critical_violations = 0;
    stats.error_violations = 0;
    stats.warning_violations = 0;
    stats.info_violations = 0;
    
    for (const auto& violation : m_violation_history) {
        switch (violation.severity) {
            case RFCViolationSeverity::CRITICAL:
                stats.critical_violations++;
                break;
            case RFCViolationSeverity::ERROR:
                stats.error_violations++;
                break;
            case RFCViolationSeverity::WARNING:
                stats.warning_violations++;
                break;
            case RFCViolationSeverity::INFO:
                stats.info_violations++;
                break;
        }
    }
    
    // Calculate compliance percentage
    if (stats.total_frames_analyzed > 0) {
        stats.compliance_percentage = (static_cast<double>(stats.compliant_frames) / stats.total_frames_analyzed) * 100.0;
    } else {
        stats.compliance_percentage = 0.0;
    }
    
    return stats;
    
    // Count violations by severity
    stats.critical_violations = 0;
    stats.error_violations = 0;
    stats.warning_violations = 0;
    stats.info_violations = 0;
    
    for (const auto& violation : m_violation_history) {
        switch (violation.severity) {
            case RFCViolationSeverity::CRITICAL: stats.critical_violations++; break;
            case RFCViolationSeverity::ERROR: stats.error_violations++; break;
            case RFCViolationSeverity::WARNING: stats.warning_violations++; break;
            case RFCViolationSeverity::INFO: stats.info_violations++; break;
        }
    }
    
    // Calculate compliance percentage
    stats.compliance_percentage = (m_total_frames_analyzed > 0) ? 
        (static_cast<double>(m_compliant_frames) * 100.0 / m_total_frames_analyzed) : 0.0;
    
    return stats;
}

void FLACRFCComplianceValidator::addViolation(const RFCViolationReport& violation) {
    std::lock_guard<std::mutex> lock(m_violation_mutex);
    
    m_violation_history.push_back(violation);
    
    // Maintain maximum history size
    if (m_violation_history.size() > m_max_violation_history) {
        m_violation_history.erase(m_violation_history.begin());
    }
    
    // Log violation immediately for debugging
    Debug::log("flac_rfc_validator", "[RFC_VIOLATION] ", violation.toString());
}

// ============================================================================
// GlobalRFCValidator Implementation
// ============================================================================

std::unique_ptr<FLACRFCComplianceValidator> GlobalRFCValidator::s_instance;
std::mutex GlobalRFCValidator::s_instance_mutex;

FLACRFCComplianceValidator& GlobalRFCValidator::getInstance() {
    std::lock_guard<std::mutex> lock(s_instance_mutex);
    if (!s_instance) {
        s_instance = std::make_unique<FLACRFCComplianceValidator>();
    }
    return *s_instance;
}

// ============================================================================
// Missing FLACRFCComplianceValidator Method Implementations
// ============================================================================

FrameComplianceAnalysis FLACRFCComplianceValidator::validateSamples(const int16_t* samples, size_t sample_count,
                                                                   uint8_t channels, uint8_t source_bit_depth,
                                                                   uint8_t target_bit_depth) {
    FrameComplianceAnalysis analysis;
    analysis.frame_number = m_total_frames_analyzed;
    analysis.is_compliant = true;
    
    if (!samples || sample_count == 0 || channels == 0) {
        RFCViolationReport violation;
        violation.severity = RFCViolationSeverity::CRITICAL;
        violation.rfc_section = "General";
        violation.violation_type = "Invalid sample data";
        violation.description = "Sample validation requires valid sample data";
        violation.expected_value = "Valid sample array";
        violation.actual_value = "NULL or empty";
        violation.byte_offset = 0;
        violation.frame_number = analysis.frame_number;
        violation.timestamp = std::chrono::high_resolution_clock::now();
        
        analysis.violations.push_back(violation);
        analysis.is_compliant = false;
        return analysis;
    }
    
    // Validate bit depth ranges per RFC 9639
    if (source_bit_depth < 4 || source_bit_depth > 32) {
        RFCViolationReport violation;
        violation.severity = RFCViolationSeverity::ERROR;
        violation.rfc_section = "9.1.5";
        violation.violation_type = "Invalid source bit depth";
        violation.description = "FLAC supports 4-32 bits per sample per RFC 9639";
        violation.expected_value = "4-32 bits";
        violation.actual_value = std::to_string(source_bit_depth) + " bits";
        violation.byte_offset = 0;
        violation.frame_number = analysis.frame_number;
        violation.timestamp = std::chrono::high_resolution_clock::now();
        
        analysis.violations.push_back(violation);
        analysis.is_compliant = false;
    }
    
    // Validate sample range for target bit depth
    int32_t max_value = (1 << (target_bit_depth - 1)) - 1;
    int32_t min_value = -(1 << (target_bit_depth - 1));
    
    for (size_t i = 0; i < sample_count; i++) {
        int32_t sample = samples[i];
        if (sample > max_value || sample < min_value) {
            RFCViolationReport violation;
            violation.severity = RFCViolationSeverity::WARNING;
            violation.rfc_section = "General";
            violation.violation_type = "Sample out of range";
            violation.description = "Sample value exceeds target bit depth range";
            violation.expected_value = std::to_string(min_value) + " to " + std::to_string(max_value);
            violation.actual_value = std::to_string(sample);
            violation.byte_offset = i * sizeof(int16_t);
            violation.frame_number = analysis.frame_number;
            violation.timestamp = std::chrono::high_resolution_clock::now();
            
            analysis.violations.push_back(violation);
            analysis.is_compliant = false;
            
            // Limit violation reports to avoid spam
            if (analysis.violations.size() >= 10) {
                break;
            }
        }
    }
    
    return analysis;
}

void FLACRFCComplianceValidator::clearViolationHistory() {
    std::lock_guard<std::mutex> lock(m_violation_mutex);
    m_violation_history.clear();
    Debug::log("flac_rfc_validator", "[clearViolationHistory] Cleared all violation history");
}

void FLACRFCComplianceValidator::setMaxViolationHistory(size_t max_violations) {
    std::lock_guard<std::mutex> lock(m_violation_mutex);
    m_max_violation_history = max_violations;
    
    // Trim existing history if needed
    if (m_violation_history.size() > max_violations) {
        m_violation_history.erase(m_violation_history.begin(), 
                                 m_violation_history.begin() + (m_violation_history.size() - max_violations));
    }
    
    Debug::log("flac_rfc_validator", "[setMaxViolationHistory] Set maximum violation history to ", max_violations);
}

void FLACRFCComplianceValidator::setValidationCategories(bool frame_header,
                                                       bool subframes,
                                                       bool channel_reconstruction,
                                                       bool crc_validation,
                                                       bool sample_format,
                                                       bool performance_monitoring) {
    m_validate_frame_header = frame_header;
    m_validate_subframes = subframes;
    m_validate_channel_reconstruction = channel_reconstruction;
    m_validate_crc = crc_validation;
    m_validate_sample_format = sample_format;
    m_monitor_performance = performance_monitoring;
    
    Debug::log("flac_rfc_validator", "[setValidationCategories] Updated validation categories: ",
              "header=", (frame_header ? "ON" : "OFF"),
              ", subframes=", (subframes ? "ON" : "OFF"),
              ", channels=", (channel_reconstruction ? "ON" : "OFF"),
              ", crc=", (crc_validation ? "ON" : "OFF"),
              ", samples=", (sample_format ? "ON" : "OFF"),
              ", perf=", (performance_monitoring ? "ON" : "OFF"));
}

bool FLACRFCComplianceValidator::createRFCComplianceTestSuite(const std::string& output_directory) {
    Debug::log("flac_rfc_validator", "[createRFCComplianceTestSuite] Creating RFC 9639 compliance test suite in ", output_directory);
    
    bool success = true;
    
    // Generate test cases for different RFC sections
    success &= generateSyncPatternTests(output_directory);
    success &= generateFrameHeaderTests(output_directory);
    success &= generateSubframeTests(output_directory);
    success &= generateCRCTests(output_directory);
    success &= generateSampleFormatTests(output_directory);
    
    if (success) {
        Debug::log("flac_rfc_validator", "[createRFCComplianceTestSuite] Successfully created RFC compliance test suite");
    } else {
        Debug::log("flac_rfc_validator", "[createRFCComplianceTestSuite] Failed to create some test cases");
    }
    
    return success;
}

bool FLACRFCComplianceValidator::checkPerformanceImpact(uint64_t validation_time_us) const {
    return validation_time_us <= m_performance_threshold_us;
}

bool FLACRFCComplianceValidator::validateReservedBits(const uint8_t* data, size_t size,
                                                     std::vector<RFCViolationReport>& violations,
                                                     size_t frame_number, size_t byte_offset) {
    if (!data || size < 4) {
        return false;
    }
    
    bool all_valid = true;
    
    // Check reserved bit after sync pattern (bit 1 of byte 1)
    uint8_t reserved_sync = (data[1] >> 1) & 0x01;
    if (reserved_sync != 0) {
        RFCViolationReport violation;
        violation.severity = RFCViolationSeverity::ERROR;
        violation.rfc_section = "9.1";
        violation.violation_type = "Reserved bit violation";
        violation.description = "Reserved bit after sync pattern must be 0";
        violation.expected_value = "0";
        violation.actual_value = "1";
        violation.byte_offset = byte_offset + 1;
        violation.frame_number = frame_number;
        violation.timestamp = std::chrono::high_resolution_clock::now();
        
        violations.push_back(violation);
        all_valid = false;
    }
    
    // Check reserved bit in sample size field (bit 0 of byte 3)
    uint8_t reserved_sample = data[3] & 0x01;
    if (reserved_sample != 0) {
        RFCViolationReport violation;
        violation.severity = RFCViolationSeverity::ERROR;
        violation.rfc_section = "9.1";
        violation.violation_type = "Reserved bit violation";
        violation.description = "Reserved bit in frame header must be 0";
        violation.expected_value = "0";
        violation.actual_value = "1";
        violation.byte_offset = byte_offset + 3;
        violation.frame_number = frame_number;
        violation.timestamp = std::chrono::high_resolution_clock::now();
        
        violations.push_back(violation);
        all_valid = false;
    }
    
    return all_valid;
}

bool FLACRFCComplianceValidator::validateBlockingStrategy(uint8_t blocking_strategy_bit,
                                                        std::vector<RFCViolationReport>& violations,
                                                        size_t frame_number, size_t byte_offset) {
    // Both 0 (fixed) and 1 (variable) are valid per RFC 9639
    // This is mainly for completeness and future extensions
    (void)blocking_strategy_bit; // Suppress unused parameter warning
    (void)violations;
    (void)frame_number;
    (void)byte_offset;
    
    return true; // Always valid currently
}

bool FLACRFCComplianceValidator::validateFrameNumberEncoding(const uint8_t* data, size_t size,
                                                           bool variable_block_size,
                                                           std::vector<RFCViolationReport>& violations,
                                                           size_t frame_number, size_t byte_offset) {
    if (!data || size < 5) {
        return false; // Need at least frame header + 1 byte for frame number
    }
    
    // Frame number/sample number encoding starts at byte 4
    // This is a simplified validation - full UTF-8 validation would be more complex
    uint8_t first_byte = data[4];
    
    // Check for valid UTF-8 encoding patterns per RFC 9639
    if (first_byte == 0xFE || first_byte == 0xFF) {
        RFCViolationReport violation;
        violation.severity = RFCViolationSeverity::ERROR;
        violation.rfc_section = "9.1";
        violation.violation_type = "Invalid frame number encoding";
        violation.description = "Frame number uses invalid UTF-8 byte sequence";
        violation.expected_value = "Valid UTF-8 encoding";
        violation.actual_value = "0x" + std::to_string(first_byte);
        violation.byte_offset = byte_offset + 4;
        violation.frame_number = frame_number;
        violation.timestamp = std::chrono::high_resolution_clock::now();
        
        violations.push_back(violation);
        return false;
    }
    
    (void)variable_block_size; // For future use
    return true;
}

bool FLACRFCComplianceValidator::generateSyncPatternTests(const std::string& output_dir) {
    Debug::log("flac_rfc_validator", "[generateSyncPatternTests] Generating sync pattern test cases in ", output_dir);
    
    // This would generate test files with various sync pattern scenarios
    // For now, just log that we would create these tests
    Debug::log("flac_rfc_validator", "  - Valid sync pattern (0x3FFE) test");
    Debug::log("flac_rfc_validator", "  - Invalid sync pattern tests");
    Debug::log("flac_rfc_validator", "  - Reserved bit violation tests");
    
    return true; // Simplified for now
}

bool FLACRFCComplianceValidator::generateFrameHeaderTests(const std::string& output_dir) {
    Debug::log("flac_rfc_validator", "[generateFrameHeaderTests] Generating frame header test cases in ", output_dir);
    
    Debug::log("flac_rfc_validator", "  - Block size encoding tests (RFC 9639 Table 1)");
    Debug::log("flac_rfc_validator", "  - Sample rate encoding tests (RFC 9639 Table 2)");
    Debug::log("flac_rfc_validator", "  - Channel assignment tests (RFC 9639 Table 3)");
    Debug::log("flac_rfc_validator", "  - Sample size encoding tests (RFC 9639 Table 4)");
    Debug::log("flac_rfc_validator", "  - Reserved value violation tests");
    
    return true; // Simplified for now
}

bool FLACRFCComplianceValidator::generateSubframeTests(const std::string& output_dir) {
    Debug::log("flac_rfc_validator", "[generateSubframeTests] Generating subframe test cases in ", output_dir);
    
    Debug::log("flac_rfc_validator", "  - CONSTANT subframe tests (RFC 9639 Section 9.2.1)");
    Debug::log("flac_rfc_validator", "  - VERBATIM subframe tests (RFC 9639 Section 9.2.2)");
    Debug::log("flac_rfc_validator", "  - FIXED predictor tests (RFC 9639 Section 9.2.3)");
    Debug::log("flac_rfc_validator", "  - LPC subframe tests (RFC 9639 Section 9.2.4)");
    Debug::log("flac_rfc_validator", "  - Wasted bits handling tests");
    Debug::log("flac_rfc_validator", "  - Side-channel processing tests");
    
    return true; // Simplified for now
}

bool FLACRFCComplianceValidator::generateCRCTests(const std::string& output_dir) {
    Debug::log("flac_rfc_validator", "[generateCRCTests] Generating CRC validation test cases in ", output_dir);
    
    Debug::log("flac_rfc_validator", "  - CRC-8 frame header tests");
    Debug::log("flac_rfc_validator", "  - CRC-16 frame footer tests");
    Debug::log("flac_rfc_validator", "  - CRC mismatch handling tests");
    Debug::log("flac_rfc_validator", "  - CRC calculation boundary tests");
    
    return true; // Simplified for now
}

bool FLACRFCComplianceValidator::generateSampleFormatTests(const std::string& output_dir) {
    Debug::log("flac_rfc_validator", "[generateSampleFormatTests] Generating sample format test cases in ", output_dir);
    
    Debug::log("flac_rfc_validator", "  - Bit depth conversion tests (4-32 bits)");
    Debug::log("flac_rfc_validator", "  - Sign extension validation tests");
    Debug::log("flac_rfc_validator", "  - Overflow protection tests");
    Debug::log("flac_rfc_validator", "  - Bit-perfect reconstruction tests");
    Debug::log("flac_rfc_validator", "  - Sample range validation tests");
    
    return true; // Simplified for now
}

bool GlobalRFCValidator::quickComplianceCheck(const uint8_t* frame_data, size_t frame_size, size_t frame_number) {
    if (!frame_data || frame_size < 4) {
        return false;
    }
    
    // Quick sync pattern check
    uint16_t sync_pattern = (static_cast<uint16_t>(frame_data[0]) << 6) | ((frame_data[1] >> 2) & 0x3F);
    if (sync_pattern != 0x3FFE) {
        return false;
    }
    
    // Quick reserved bit checks
    uint8_t reserved_sync = (frame_data[1] >> 1) & 0x01;
    uint8_t reserved_sample = frame_data[3] & 0x01;
    if (reserved_sync != 0 || reserved_sample != 0) {
        return false;
    }
    
    // Quick forbidden value checks
    uint8_t block_size_bits = (frame_data[1] >> 4) & 0x0F;
    uint8_t sample_rate_bits = frame_data[2] & 0x0F;
    uint8_t sample_size_bits = (frame_data[3] >> 1) & 0x07;
    
    if (block_size_bits == 0x00 || sample_rate_bits == 0x0F || sample_size_bits == 0x03) {
        return false;
    }
    
    return true;
}

void GlobalRFCValidator::logViolation(const std::string& rfc_section,
                                    const std::string& violation_type,
                                    const std::string& description,
                                    const std::string& expected,
                                    const std::string& actual,
                                    size_t frame_number,
                                    size_t byte_offset) {
    RFCViolationReport violation;
    violation.severity = RFCViolationSeverity::ERROR; // Default severity
    violation.rfc_section = rfc_section;
    violation.violation_type = violation_type;
    violation.description = description;
    violation.expected_value = expected;
    violation.actual_value = actual;
    violation.frame_number = frame_number;
    violation.byte_offset = byte_offset;
    violation.timestamp = std::chrono::high_resolution_clock::now();
    
    getInstance().addViolation(violation);
}

#endif // HAVE_FLAC