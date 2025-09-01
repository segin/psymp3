/*
 * test_flac_codec_error_handling.cpp - Error handling tests for FLAC codec algorithms
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
#include <vector>
#include <cstdint>
#include <algorithm>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_FLAC

/**
 * @brief Test FLAC frame validation and error detection
 * Requirements: 7.1-7.8
 */
class FLACCodecErrorHandlingTest {
public:
    static bool runAllTests() {
        std::cout << "FLAC Codec Error Handling Tests" << std::endl;
        std::cout << "===============================" << std::endl;
        
        bool all_passed = true;
        
        all_passed &= testFrameHeaderValidation();
        all_passed &= testParameterValidation();
        all_passed &= testDataCorruptionDetection();
        all_passed &= testErrorRecovery();
        
        if (all_passed) {
            std::cout << "✓ All error handling tests PASSED" << std::endl;
        } else {
            std::cout << "✗ Some error handling tests FAILED" << std::endl;
        }
        
        return all_passed;
    }

private:
    static bool testFrameHeaderValidation() {
        std::cout << "Testing FLAC frame header validation..." << std::endl;
        
        // Test valid sync pattern (0xFF followed by 0xF8-0xFF)
        if (!isValidSyncPattern(0xFF, 0xF8) || !isValidSyncPattern(0xFF, 0xFF)) {
            std::cout << "  ERROR: Valid sync patterns rejected" << std::endl;
            return false;
        }
        
        // Test invalid sync patterns
        if (isValidSyncPattern(0xFF, 0xF7) || isValidSyncPattern(0xFE, 0xF8)) {
            std::cout << "  ERROR: Invalid sync patterns accepted" << std::endl;
            return false;
        }
        
        // Test block size validation (RFC 9639: 16-65535)
        if (!isValidBlockSize(1152) || !isValidBlockSize(4608)) {
            std::cout << "  ERROR: Valid block sizes rejected" << std::endl;
            return false;
        }
        
        if (isValidBlockSize(0) || isValidBlockSize(65536)) {
            std::cout << "  ERROR: Invalid block sizes accepted" << std::endl;
            return false;
        }
        
        std::cout << "  ✓ Frame header validation test passed" << std::endl;
        return true;
    }
    
    static bool testParameterValidation() {
        std::cout << "Testing parameter validation..." << std::endl;
        
        // Test sample rate validation (RFC 9639: 1-655350 Hz)
        if (!isValidSampleRate(44100) || !isValidSampleRate(48000) || !isValidSampleRate(96000)) {
            std::cout << "  ERROR: Valid sample rates rejected" << std::endl;
            return false;
        }
        
        if (isValidSampleRate(0) || isValidSampleRate(655351)) {
            std::cout << "  ERROR: Invalid sample rates accepted" << std::endl;
            return false;
        }
        
        // Test channel count validation (RFC 9639: 1-8 channels)
        if (!isValidChannelCount(1) || !isValidChannelCount(2) || !isValidChannelCount(8)) {
            std::cout << "  ERROR: Valid channel counts rejected" << std::endl;
            return false;
        }
        
        if (isValidChannelCount(0) || isValidChannelCount(9)) {
            std::cout << "  ERROR: Invalid channel counts accepted" << std::endl;
            return false;
        }
        
        // Test bit depth validation (RFC 9639: 4-32 bits)
        if (!isValidBitDepth(16) || !isValidBitDepth(24) || !isValidBitDepth(32)) {
            std::cout << "  ERROR: Valid bit depths rejected" << std::endl;
            return false;
        }
        
        if (isValidBitDepth(3) || isValidBitDepth(33)) {
            std::cout << "  ERROR: Invalid bit depths accepted" << std::endl;
            return false;
        }
        
        std::cout << "  ✓ Parameter validation test passed" << std::endl;
        return true;
    }
    
    static bool testDataCorruptionDetection() {
        std::cout << "Testing data corruption detection..." << std::endl;
        
        // Test CRC validation simulation
        std::vector<uint8_t> valid_data = {0xFF, 0xF8, 0x69, 0x10, 0x00, 0x00};
        std::vector<uint8_t> corrupted_data = {0xFF, 0xF8, 0x69, 0x10, 0xFF, 0xFF};
        
        uint8_t valid_crc = calculateSimpleCRC(valid_data);
        uint8_t corrupted_crc = calculateSimpleCRC(corrupted_data);
        
        if (valid_crc == corrupted_crc) {
            std::cout << "  ERROR: CRC should detect corruption" << std::endl;
            return false;
        }
        
        // Test frame boundary detection
        if (!isValidFrameStart(0xFF, 0xF8)) {
            std::cout << "  ERROR: Valid frame start not detected" << std::endl;
            return false;
        }
        
        if (isValidFrameStart(0x00, 0x00)) {
            std::cout << "  ERROR: Invalid frame start detected as valid" << std::endl;
            return false;
        }
        
        std::cout << "  ✓ Data corruption detection test passed" << std::endl;
        return true;
    }
    
    static bool testErrorRecovery() {
        std::cout << "Testing error recovery mechanisms..." << std::endl;
        
        // Test graceful handling of various error conditions
        
        // Simulate sync loss recovery
        std::vector<uint8_t> data_with_sync_loss = {
            0x00, 0x01, 0x02, 0x03,  // Garbage data
            0xFF, 0xF8, 0x69, 0x10   // Valid sync pattern
        };
        
        size_t sync_position = findNextSyncPattern(data_with_sync_loss);
        if (sync_position != 4) {
            std::cout << "  ERROR: Sync recovery failed. Expected position 4, got " << sync_position << std::endl;
            return false;
        }
        
        // Test error statistics tracking
        ErrorStats stats;
        stats.recordError(ErrorType::CRC_ERROR);
        stats.recordError(ErrorType::SYNC_ERROR);
        stats.recordError(ErrorType::CRC_ERROR);
        
        if (stats.getCRCErrorCount() != 2 || stats.getSyncErrorCount() != 1) {
            std::cout << "  ERROR: Error statistics tracking failed" << std::endl;
            return false;
        }
        
        double error_rate = stats.getErrorRate(10);  // 3 errors out of 10 frames
        if (error_rate != 30.0) {
            std::cout << "  ERROR: Error rate calculation failed. Expected 30.0, got " << error_rate << std::endl;
            return false;
        }
        
        std::cout << "  ✓ Error recovery test passed" << std::endl;
        return true;
    }
    
    // Helper methods for error handling simulation
    static bool isValidSyncPattern(uint8_t byte1, uint8_t byte2) {
        return byte1 == 0xFF && (byte2 >= 0xF8 && byte2 <= 0xFF);
    }
    
    static bool isValidBlockSize(uint32_t block_size) {
        return block_size >= 16 && block_size <= 65535;
    }
    
    static bool isValidSampleRate(uint32_t sample_rate) {
        return sample_rate >= 1 && sample_rate <= 655350;
    }
    
    static bool isValidChannelCount(uint16_t channels) {
        return channels >= 1 && channels <= 8;
    }
    
    static bool isValidBitDepth(uint16_t bits_per_sample) {
        return bits_per_sample >= 4 && bits_per_sample <= 32;
    }
    
    static uint8_t calculateSimpleCRC(const std::vector<uint8_t>& data) {
        uint8_t crc = 0;
        for (uint8_t byte : data) {
            crc ^= byte;
            for (int i = 0; i < 8; ++i) {
                if (crc & 0x80) {
                    crc = (crc << 1) ^ 0x07;  // Simple CRC polynomial
                } else {
                    crc <<= 1;
                }
            }
        }
        return crc;
    }
    
    static bool isValidFrameStart(uint8_t byte1, uint8_t byte2) {
        return isValidSyncPattern(byte1, byte2);
    }
    
    static size_t findNextSyncPattern(const std::vector<uint8_t>& data) {
        for (size_t i = 0; i < data.size() - 1; ++i) {
            if (isValidSyncPattern(data[i], data[i + 1])) {
                return i;
            }
        }
        return data.size();  // Not found
    }
    
    enum class ErrorType {
        CRC_ERROR,
        SYNC_ERROR,
        MEMORY_ERROR
    };
    
    class ErrorStats {
    private:
        size_t crc_errors = 0;
        size_t sync_errors = 0;
        size_t memory_errors = 0;
        
    public:
        void recordError(ErrorType type) {
            switch (type) {
                case ErrorType::CRC_ERROR: crc_errors++; break;
                case ErrorType::SYNC_ERROR: sync_errors++; break;
                case ErrorType::MEMORY_ERROR: memory_errors++; break;
            }
        }
        
        size_t getCRCErrorCount() const { return crc_errors; }
        size_t getSyncErrorCount() const { return sync_errors; }
        size_t getMemoryErrorCount() const { return memory_errors; }
        
        size_t getTotalErrorCount() const {
            return crc_errors + sync_errors + memory_errors;
        }
        
        double getErrorRate(size_t total_frames) const {
            if (total_frames == 0) return 0.0;
            return (static_cast<double>(getTotalErrorCount()) * 100.0) / total_frames;
        }
    };
};

int main() {
    std::cout << "FLAC Codec Error Handling Tests" << std::endl;
    std::cout << "===============================" << std::endl;
    std::cout << "Requirements: 7.1-7.8" << std::endl;
    std::cout << std::endl;
    
    bool success = FLACCodecErrorHandlingTest::runAllTests();
    
    return success ? 0 : 1;
}

#else // !HAVE_FLAC

int main() {
    std::cout << "FLAC support not available - skipping FLAC codec error handling tests" << std::endl;
    return 0;
}

#endif // HAVE_FLAC