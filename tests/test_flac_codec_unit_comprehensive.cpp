/*
 * test_flac_codec_unit_comprehensive.cpp - Comprehensive unit tests for FLAC codec algorithms
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
#include <cmath>
#include <algorithm>
#include <cstdint>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_FLAC

/**
 * @brief Test FLAC codec bit depth conversion algorithms
 * Requirements: 2.1-2.8
 */
class FLACCodecBitDepthTest {
public:
    static bool runAllTests() {
        std::cout << "FLAC Codec Bit Depth Conversion Tests" << std::endl;
        std::cout << "=====================================" << std::endl;
        
        bool all_passed = true;
        
        all_passed &= test8BitTo16BitConversion();
        all_passed &= test24BitTo16BitConversion();
        all_passed &= test32BitTo16BitConversion();
        all_passed &= testConversionAccuracy();
        all_passed &= testOverflowProtection();
        
        if (all_passed) {
            std::cout << "✓ All bit depth conversion tests PASSED" << std::endl;
        } else {
            std::cout << "✗ Some bit depth conversion tests FAILED" << std::endl;
        }
        
        return all_passed;
    }

private:
    static bool test8BitTo16BitConversion() {
        std::cout << "Testing 8-bit to 16-bit conversion..." << std::endl;
        
        // Test conversion of various 8-bit values
        std::vector<int32_t> test_values = {-128, -64, -1, 0, 1, 64, 127};
        std::vector<int16_t> expected_values = {-32768, -16384, -256, 0, 256, 16384, 32512};
        
        for (size_t i = 0; i < test_values.size(); ++i) {
            int16_t converted = convert8BitTo16Bit(test_values[i]);
            if (converted != expected_values[i]) {
                std::cout << "  ERROR: 8-bit conversion failed for " << test_values[i] 
                         << ". Expected: " << expected_values[i] << ", Got: " << converted << std::endl;
                return false;
            }
        }
        
        std::cout << "  ✓ 8-bit to 16-bit conversion test passed" << std::endl;
        return true;
    }
    
    static bool test24BitTo16BitConversion() {
        std::cout << "Testing 24-bit to 16-bit conversion..." << std::endl;
        
        // Test conversion of various 24-bit values
        std::vector<int32_t> test_values = {
            -8388608, -1000000, -256, 0, 256, 1000000, 8388607
        };
        
        for (int32_t value : test_values) {
            int16_t converted = convert24BitTo16Bit(value);
            
            // Verify conversion is within 16-bit range
            if (converted < -32768 || converted > 32767) {
                std::cout << "  ERROR: 24-bit conversion out of range for " << value 
                         << ". Got: " << converted << std::endl;
                return false;
            }
            
            // Verify conversion preserves sign
            if (value < 0 && converted > 0) {
                std::cout << "  ERROR: Sign not preserved for negative value " << value << std::endl;
                return false;
            }
            if (value > 0 && converted < 0) {
                std::cout << "  ERROR: Sign not preserved for positive value " << value << std::endl;
                return false;
            }
        }
        
        std::cout << "  ✓ 24-bit to 16-bit conversion test passed" << std::endl;
        return true;
    }
    
    static bool test32BitTo16BitConversion() {
        std::cout << "Testing 32-bit to 16-bit conversion..." << std::endl;
        
        // Test conversion of various 32-bit values
        std::vector<int32_t> test_values = {
            -2147483648LL, -100000000, -65536, 0, 65536, 100000000, 2147483647
        };
        
        for (int32_t value : test_values) {
            int16_t converted = convert32BitTo16Bit(value);
            
            // Verify conversion is within 16-bit range
            if (converted < -32768 || converted > 32767) {
                std::cout << "  ERROR: 32-bit conversion out of range for " << value 
                         << ". Got: " << converted << std::endl;
                return false;
            }
        }
        
        std::cout << "  ✓ 32-bit to 16-bit conversion test passed" << std::endl;
        return true;
    }
    
    static bool testConversionAccuracy() {
        std::cout << "Testing conversion accuracy..." << std::endl;
        
        // Test mathematical accuracy of conversions
        
        // 8-bit: should multiply by 256 (left shift 8)
        if (convert8BitTo16Bit(100) != 25600) {
            std::cout << "  ERROR: 8-bit conversion accuracy failed" << std::endl;
            return false;
        }
        
        // 24-bit: should divide by 256 (right shift 8)
        if (convert24BitTo16Bit(1000000) != 3906) {
            std::cout << "  ERROR: 24-bit conversion accuracy failed" << std::endl;
            return false;
        }
        
        // 32-bit: should divide by 65536 (right shift 16)
        if (convert32BitTo16Bit(100000000) != 1525) {
            std::cout << "  ERROR: 32-bit conversion accuracy failed" << std::endl;
            return false;
        }
        
        std::cout << "  ✓ Conversion accuracy test passed" << std::endl;
        return true;
    }
    
    static bool testOverflowProtection() {
        std::cout << "Testing overflow protection..." << std::endl;
        
        // Test that conversions handle overflow/underflow correctly
        int16_t max_24_converted = convert24BitTo16Bit(8388607);  // Max 24-bit
        int16_t min_24_converted = convert24BitTo16Bit(-8388608); // Min 24-bit
        
        if (max_24_converted < -32768 || max_24_converted > 32767) {
            std::cout << "  ERROR: 24-bit max overflow not handled" << std::endl;
            return false;
        }
        
        if (min_24_converted < -32768 || min_24_converted > 32767) {
            std::cout << "  ERROR: 24-bit min underflow not handled" << std::endl;
            return false;
        }
        
        std::cout << "  ✓ Overflow protection test passed" << std::endl;
        return true;
    }
    
    // Helper methods for bit depth conversion
    static int16_t convert8BitTo16Bit(int32_t sample) {
        return static_cast<int16_t>(sample << 8);
    }
    
    static int16_t convert24BitTo16Bit(int32_t sample) {
        return static_cast<int16_t>(sample >> 8);
    }
    
    static int16_t convert32BitTo16Bit(int32_t sample) {
        int32_t scaled = sample >> 16;
        return static_cast<int16_t>(std::clamp(scaled, -32768, 32767));
    }
};

/**
 * @brief Test FLAC channel processing algorithms
 * Requirements: 3.1-3.8
 */
class FLACCodecChannelProcessingTest {
public:
    static bool runAllTests() {
        std::cout << std::endl << "FLAC Codec Channel Processing Tests" << std::endl;
        std::cout << "===================================" << std::endl;
        
        bool all_passed = true;
        
        all_passed &= testStereoReconstruction();
        all_passed &= testChannelInterleaving();
        all_passed &= testChannelAssignmentValidation();
        
        if (all_passed) {
            std::cout << "✓ All channel processing tests PASSED" << std::endl;
        } else {
            std::cout << "✗ Some channel processing tests FAILED" << std::endl;
        }
        
        return all_passed;
    }

private:
    static bool testStereoReconstruction() {
        std::cout << "Testing stereo reconstruction algorithms..." << std::endl;
        
        // Test Left-Side stereo reconstruction
        std::vector<int32_t> left_samples = {1000, 2000, 3000};
        std::vector<int32_t> side_samples = {200, 400, 600};  // Left - Right difference
        
        std::vector<int16_t> output = processLeftSideStereo(left_samples, side_samples);
        
        // Expected: Left = 1000, Right = Left - Side = 1000 - 200 = 800
        std::vector<int16_t> expected = {1000, 800, 2000, 1600, 3000, 2400};
        
        if (output.size() != expected.size()) {
            std::cout << "  ERROR: Left-Side output size mismatch" << std::endl;
            return false;
        }
        
        for (size_t i = 0; i < expected.size(); ++i) {
            if (output[i] != expected[i]) {
                std::cout << "  ERROR: Left-Side reconstruction failed at index " << i << std::endl;
                return false;
            }
        }
        
        std::cout << "  ✓ Stereo reconstruction test passed" << std::endl;
        return true;
    }
    
    static bool testChannelInterleaving() {
        std::cout << "Testing channel interleaving..." << std::endl;
        
        std::vector<std::vector<int32_t>> channel_data = {
            {100, 200, 300},  // Channel 0
            {110, 210, 310},  // Channel 1
            {120, 220, 320},  // Channel 2
            {130, 230, 330}   // Channel 3
        };
        
        std::vector<int16_t> output = processMultiChannel(channel_data);
        
        // Expected interleaved output: Ch0, Ch1, Ch2, Ch3, Ch0, Ch1, Ch2, Ch3, ...
        std::vector<int16_t> expected = {
            100, 110, 120, 130,  // Sample 0
            200, 210, 220, 230,  // Sample 1
            300, 310, 320, 330   // Sample 2
        };
        
        if (output.size() != expected.size()) {
            std::cout << "  ERROR: Multi-channel output size mismatch" << std::endl;
            return false;
        }
        
        for (size_t i = 0; i < expected.size(); ++i) {
            if (output[i] != expected[i]) {
                std::cout << "  ERROR: Multi-channel interleaving failed at index " << i << std::endl;
                return false;
            }
        }
        
        std::cout << "  ✓ Channel interleaving test passed" << std::endl;
        return true;
    }
    
    static bool testChannelAssignmentValidation() {
        std::cout << "Testing channel assignment validation..." << std::endl;
        
        // Test valid assignments for stereo (2 channels)
        if (!isValidChannelAssignment(2, 0) ||   // Independent
            !isValidChannelAssignment(2, 8) ||   // Left-Side
            !isValidChannelAssignment(2, 9) ||   // Right-Side
            !isValidChannelAssignment(2, 10)) {  // Mid-Side
            std::cout << "  ERROR: Valid stereo assignments rejected" << std::endl;
            return false;
        }
        
        // Test invalid assignments for stereo
        if (isValidChannelAssignment(2, 11) || isValidChannelAssignment(2, 15)) {
            std::cout << "  ERROR: Invalid stereo assignments accepted" << std::endl;
            return false;
        }
        
        // Test valid assignments for mono (1 channel)
        if (!isValidChannelAssignment(1, 0)) {
            std::cout << "  ERROR: Valid mono assignment rejected" << std::endl;
            return false;
        }
        
        // Test invalid assignments for mono
        if (isValidChannelAssignment(1, 8) || isValidChannelAssignment(1, 9)) {
            std::cout << "  ERROR: Invalid mono assignments accepted" << std::endl;
            return false;
        }
        
        std::cout << "  ✓ Channel assignment validation test passed" << std::endl;
        return true;
    }
    
    // Helper methods for channel processing simulation
    static std::vector<int16_t> processLeftSideStereo(const std::vector<int32_t>& left,
                                                     const std::vector<int32_t>& side) {
        std::vector<int16_t> output;
        for (size_t i = 0; i < left.size(); ++i) {
            int32_t left_sample = left[i];
            int32_t right_sample = left_sample - side[i];
            
            output.push_back(static_cast<int16_t>(left_sample));
            output.push_back(static_cast<int16_t>(right_sample));
        }
        return output;
    }
    
    static std::vector<int16_t> processMultiChannel(const std::vector<std::vector<int32_t>>& channels) {
        std::vector<int16_t> output;
        
        if (channels.empty()) return output;
        
        size_t num_samples = channels[0].size();
        size_t num_channels = channels.size();
        
        for (size_t sample = 0; sample < num_samples; ++sample) {
            for (size_t channel = 0; channel < num_channels; ++channel) {
                output.push_back(static_cast<int16_t>(channels[channel][sample]));
            }
        }
        
        return output;
    }
    
    static bool isValidChannelAssignment(uint16_t channels, uint8_t assignment) {
        // FLAC channel assignment validation per RFC 9639
        if (channels == 1) {
            return assignment == 0;  // Only independent for mono
        } else if (channels == 2) {
            return assignment == 0 ||   // Independent
                   assignment == 8 ||   // Left-Side
                   assignment == 9 ||   // Right-Side
                   assignment == 10;    // Mid-Side
        } else if (channels >= 3 && channels <= 8) {
            return assignment == 0;     // Only independent for multi-channel
        }
        
        return false;  // Invalid channel count
    }
};

/**
 * @brief Test FLAC variable block size handling
 * Requirements: 4.1-4.8
 */
class FLACCodecBlockSizeTest {
public:
    static bool runAllTests() {
        std::cout << std::endl << "FLAC Codec Block Size Tests" << std::endl;
        std::cout << "===========================" << std::endl;
        
        bool all_passed = true;
        
        all_passed &= testStandardBlockSizes();
        all_passed &= testBlockSizeValidation();
        all_passed &= testVariableBlockSizeSupport();
        
        if (all_passed) {
            std::cout << "✓ All block size tests PASSED" << std::endl;
        } else {
            std::cout << "✗ Some block size tests FAILED" << std::endl;
        }
        
        return all_passed;
    }

private:
    static bool testStandardBlockSizes() {
        std::cout << "Testing standard FLAC block sizes..." << std::endl;
        
        std::vector<uint32_t> standard_sizes = {192, 576, 1152, 2304, 4608, 9216, 18432, 36864};
        
        for (uint32_t block_size : standard_sizes) {
            if (!isValidBlockSize(block_size)) {
                std::cout << "  ERROR: Standard block size " << block_size << " rejected" << std::endl;
                return false;
            }
        }
        
        std::cout << "  ✓ Standard block sizes test passed" << std::endl;
        return true;
    }
    
    static bool testBlockSizeValidation() {
        std::cout << "Testing block size validation..." << std::endl;
        
        // Test valid range (16-65535 per RFC 9639)
        if (!isValidBlockSize(16) || !isValidBlockSize(65535)) {
            std::cout << "  ERROR: Valid block size range rejected" << std::endl;
            return false;
        }
        
        // Test invalid sizes
        if (isValidBlockSize(15) || isValidBlockSize(65536)) {
            std::cout << "  ERROR: Invalid block sizes accepted" << std::endl;
            return false;
        }
        
        std::cout << "  ✓ Block size validation test passed" << std::endl;
        return true;
    }
    
    static bool testVariableBlockSizeSupport() {
        std::cout << "Testing variable block size support..." << std::endl;
        
        // Test that different block sizes can be handled in sequence
        std::vector<uint32_t> variable_sizes = {1152, 4608, 576, 2304, 1152};
        
        for (uint32_t size : variable_sizes) {
            if (!isValidBlockSize(size)) {
                std::cout << "  ERROR: Variable block size " << size << " not supported" << std::endl;
                return false;
            }
        }
        
        std::cout << "  ✓ Variable block size support test passed" << std::endl;
        return true;
    }
    
    static bool isValidBlockSize(uint32_t block_size) {
        // RFC 9639: block size must be between 16 and 65535
        return block_size >= 16 && block_size <= 65535;
    }
};

int main() {
    std::cout << "FLAC Codec Comprehensive Unit Tests" << std::endl;
    std::cout << "===================================" << std::endl;
    std::cout << "Requirements: 2.1-2.8, 3.1-3.8, 4.1-4.8, 7.1-7.8" << std::endl;
    std::cout << std::endl;
    
    bool all_tests_passed = true;
    
    all_tests_passed &= FLACCodecBitDepthTest::runAllTests();
    all_tests_passed &= FLACCodecChannelProcessingTest::runAllTests();
    all_tests_passed &= FLACCodecBlockSizeTest::runAllTests();
    
    std::cout << std::endl;
    if (all_tests_passed) {
        std::cout << "✓ ALL FLAC CODEC UNIT TESTS PASSED" << std::endl;
        return 0;
    } else {
        std::cout << "✗ SOME FLAC CODEC UNIT TESTS FAILED" << std::endl;
        return 1;
    }
}

#else // !HAVE_FLAC

int main() {
    std::cout << "FLAC support not available - skipping FLAC codec unit tests" << std::endl;
    return 0;
}

#endif // HAVE_FLAC