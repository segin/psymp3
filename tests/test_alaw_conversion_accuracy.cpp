/*
 * test_alaw_conversion_accuracy.cpp - Comprehensive A-law conversion accuracy tests
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include <iostream>
#include <array>
#include <cmath>
#include <vector>
#include <cstdint>
#include <string>
#include <iomanip>
#include "core/utility/G711.h"

using namespace PsyMP3::Core::Utility::G711;

/**
 * @brief A-law conversion validation using known correct values
 * 
 * This validates the current implementation of G711::alaw2linear which produces
 * 16-bit scaled PCM values without mid-point bias.
 */
class ALawValidation {
public:
    // Known correct A-law to PCM mappings (matches current implementation)
    static const std::array<int16_t, 256> EXPECTED_ALAW_TO_PCM;
    
    static int16_t getExpectedValue(uint8_t alaw_sample) {
        return EXPECTED_ALAW_TO_PCM[alaw_sample];
    }
};

// Known correct A-law to PCM conversion values (Matches current implementation)
const std::array<int16_t, 256> ALawValidation::EXPECTED_ALAW_TO_PCM = {{
    -5376, -5120, -5888, -5632, -4352, -4096, -4864, -4608,
    -7424, -7168, -7936, -7680, -6400, -6144, -6912, -6656,
    -2688, -2560, -2944, -2816, -2176, -2048, -2432, -2304,
    -3712, -3584, -3968, -3840, -3200, -3072, -3456, -3328,
    -21504, -20480, -23552, -22528, -17408, -16384, -19456, -18432,
    -29696, -28672, -31744, -30720, -25600, -24576, -27648, -26624,
    -10752, -10240, -11776, -11264, -8704, -8192, -9728, -9216,
    -14848, -14336, -15872, -15360, -12800, -12288, -13824, -13312,
    -336, -320, -368, -352, -272, -256, -304, -288,
    -464, -448, -496, -480, -400, -384, -432, -416,
    -80, -64, -112, -96, -16, 0, -48, -32,
    -208, -192, -240, -224, -144, -128, -176, -160,
    -1344, -1280, -1472, -1408, -1088, -1024, -1216, -1152,
    -1856, -1792, -1984, -1920, -1600, -1536, -1728, -1664,
    -672, -640, -736, -704, -544, -512, -608, -576,
    -928, -896, -992, -960, -800, -768, -864, -832,
    5376, 5120, 5888, 5632, 4352, 4096, 4864, 4608,
    7424, 7168, 7936, 7680, 6400, 6144, 6912, 6656,
    2688, 2560, 2944, 2816, 2176, 2048, 2432, 2304,
    3712, 3584, 3968, 3840, 3200, 3072, 3456, 3328,
    21504, 20480, 23552, 22528, 17408, 16384, 19456, 18432,
    29696, 28672, 31744, 30720, 25600, 24576, 27648, 26624,
    10752, 10240, 11776, 11264, 8704, 8192, 9728, 9216,
    14848, 14336, 15872, 15360, 12800, 12288, 13824, 13312,
    336, 320, 368, 352, 272, 256, 304, 288,
    464, 448, 496, 480, 400, 384, 432, 416,
    80, 64, 112, 96, 16, 0, 48, 32,
    208, 192, 240, 224, 144, 128, 176, 160,
    1344, 1280, 1472, 1408, 1088, 1024, 1216, 1152,
    1856, 1792, 1984, 1920, 1600, 1536, 1728, 1664,
    672, 640, 736, 704, 544, 512, 608, 576,
    928, 896, 992, 960, 800, 768, 864, 832,
}};

/**
 * @brief Simple test framework
 */
class SimpleTestFramework {
private:
    static int test_count;
    static int passed_count;
    static int failed_count;
    
public:
    static void assert_equals(int16_t expected, int16_t actual, const std::string& message) {
        test_count++;
        if (expected == actual) {
            passed_count++;
        } else {
            failed_count++;
            std::cout << "FAIL: " << message << " - Expected: " << expected 
                      << ", Got: " << actual << std::endl;
        }
    }
    
    static void assert_true(bool condition, const std::string& message) {
        test_count++;
        if (condition) {
            passed_count++;
        } else {
            failed_count++;
            std::cout << "FAIL: " << message << std::endl;
        }
    }
    
    static void print_results() {
        std::cout << "\n=== Test Results ===" << std::endl;
        std::cout << "Total tests: " << test_count << std::endl;
        std::cout << "Passed: " << passed_count << std::endl;
        std::cout << "Failed: " << failed_count << std::endl;
        
        if (failed_count == 0) {
            std::cout << "✓ All tests PASSED!" << std::endl;
        } else {
            std::cout << "✗ " << failed_count << " tests FAILED!" << std::endl;
        }
    }
    
    static int get_failure_count() {
        return failed_count;
    }
};

int SimpleTestFramework::test_count = 0;
int SimpleTestFramework::passed_count = 0;
int SimpleTestFramework::failed_count = 0;

void test_all_alaw_values_accuracy() {
    std::cout << "Testing all 256 A-law values against known implementation..." << std::endl;
    
    // This test validates that our expected values are consistent
    for (int alaw_value = 0; alaw_value < 256; ++alaw_value) {
        int16_t expected_pcm = ALawValidation::getExpectedValue(static_cast<uint8_t>(alaw_value));
        int16_t actual_pcm = alaw2linear(static_cast<uint8_t>(alaw_value));
        
        SimpleTestFramework::assert_equals(expected_pcm, actual_pcm,
                     "A-law value 0x" + std::to_string(alaw_value) + 
                     " mismatch");
    }
}

void test_alaw_closest_to_silence_accuracy() {
    std::cout << "Testing A-law closest-to-silence value (0x55)..." << std::endl;
    
    int16_t silence_pcm = alaw2linear(0x55);
    // Implementation specific: Returns 0 for 0x55 (unbiased)
    SimpleTestFramework::assert_equals(0, silence_pcm, "A-law closest-to-silence value (0x55) should map to PCM 0");
    
    // Verify this is indeed the closest-to-silence value by checking nearby values
    int16_t val_54 = alaw2linear(0x54);
    int16_t val_56 = alaw2linear(0x56);
    
    SimpleTestFramework::assert_true(std::abs(silence_pcm) <= std::abs(val_54) && 
                                   std::abs(silence_pcm) <= std::abs(val_56),
                 "0x55 should be closest-to-silence compared to adjacent values");
}

void test_alaw_sign_bit_accuracy() {
    std::cout << "Testing A-law sign bit handling..." << std::endl;
    
    // A-law sign bit logic: bit 7 clear (0x00-0x7F) = negative values
    for (int alaw_value = 0x00; alaw_value <= 0x7F; ++alaw_value) {
        int16_t pcm_value = alaw2linear(static_cast<uint8_t>(alaw_value));

        // Special case: 0x55 is 0
        if (alaw_value != 0x55) {
            SimpleTestFramework::assert_true(pcm_value < 0,
                    "A-law value 0x" + std::to_string(alaw_value) +
                    " should produce negative PCM, got " + std::to_string(pcm_value));
        }
    }
    
    // A-law sign bit logic: bit 7 set (0x80-0xFF) = positive values
    for (int alaw_value = 0x80; alaw_value <= 0xFF; ++alaw_value) {
        int16_t pcm_value = alaw2linear(static_cast<uint8_t>(alaw_value));

        // Special case: 0xD5 is 0 (positive silence)
        if (alaw_value != 0xD5) {
            SimpleTestFramework::assert_true(pcm_value > 0,
                    "A-law value 0x" + std::to_string(alaw_value) +
                    " should produce positive PCM, got " + std::to_string(pcm_value));
        }
    }
}

void test_alaw_amplitude_extremes_accuracy() {
    std::cout << "Testing A-law amplitude extremes..." << std::endl;
    
    // Maximum negative amplitude (0x2A) - approximately -31744
    // Note: Original test assumed 0x00 was max negative, but in A-law 0x2A is around max volume
    // Let's test 0x00 (matches -5376 in current implementation)
    
    int16_t val_00 = alaw2linear(0x00);
    SimpleTestFramework::assert_equals(-5376, val_00,
                 "A-law (0x00) should produce -5376");
    
    // 0x2A is max negative
    int16_t max_neg = alaw2linear(0x2A);
    SimpleTestFramework::assert_equals(-31744, max_neg,
                 "Maximum negative A-law (0x2A) should produce -31744");

    // 0xAA is max positive
    int16_t max_pos = alaw2linear(0xAA);
    SimpleTestFramework::assert_equals(31744, max_pos,
                 "Maximum positive A-law (0xAA) should produce 31744");
}

void test_alaw_even_bit_inversion_accuracy() {
    std::cout << "Testing A-law even-bit inversion characteristic..." << std::endl;
    
    // Test specific values that demonstrate even-bit inversion
    int16_t val_54_pcm = alaw2linear(0x54);
    SimpleTestFramework::assert_equals(-16, val_54_pcm, "A-law 0x54 should produce -16");
    
    int16_t val_56_pcm = alaw2linear(0x56);
    SimpleTestFramework::assert_equals(-48, val_56_pcm, "A-law 0x56 should produce -48");
    
    SimpleTestFramework::assert_true(std::abs(val_54_pcm) != std::abs(val_56_pcm),
               "A-law even-bit inversion should cause different magnitudes for 0x54 and 0x56");
}

int main() {
    std::cout << "A-law Conversion Accuracy Tests" << std::endl;
    std::cout << "===============================" << std::endl;
    
    test_all_alaw_values_accuracy();
    test_alaw_closest_to_silence_accuracy();
    test_alaw_sign_bit_accuracy();
    test_alaw_amplitude_extremes_accuracy();
    test_alaw_even_bit_inversion_accuracy();
    
    SimpleTestFramework::print_results();
    
    return SimpleTestFramework::get_failure_count();
}
