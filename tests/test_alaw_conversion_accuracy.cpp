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
 * This validates G711::alaw2linear against the ITU-T G.711 reference
 * (interval-midpoint outputs; the Sun reference implementation's table).
 */
class ALawValidation {
public:
    // ITU-T G.711 reference A-law to PCM mappings
    static const std::array<int16_t, 256> EXPECTED_ALAW_TO_PCM;
    
    static int16_t getExpectedValue(uint8_t alaw_sample) {
        return EXPECTED_ALAW_TO_PCM[alaw_sample];
    }
};

// ITU-T G.711 Table 2 reference values (Sun reference implementation:
// interval midpoints, +8 offset; A-law has no zero output)
const std::array<int16_t, 256> ALawValidation::EXPECTED_ALAW_TO_PCM = {{
    -5504, -5248, -6016, -5760, -4480, -4224, -4992, -4736,
    -7552, -7296, -8064, -7808, -6528, -6272, -7040, -6784,
    -2752, -2624, -3008, -2880, -2240, -2112, -2496, -2368,
    -3776, -3648, -4032, -3904, -3264, -3136, -3520, -3392,
    -22016, -20992, -24064, -23040, -17920, -16896, -19968, -18944,
    -30208, -29184, -32256, -31232, -26112, -25088, -28160, -27136,
    -11008, -10496, -12032, -11520, -8960, -8448, -9984, -9472,
    -15104, -14592, -16128, -15616, -13056, -12544, -14080, -13568,
    -344, -328, -376, -360, -280, -264, -312, -296,
    -472, -456, -504, -488, -408, -392, -440, -424,
    -88, -72, -120, -104, -24, -8, -56, -40,
    -216, -200, -248, -232, -152, -136, -184, -168,
    -1376, -1312, -1504, -1440, -1120, -1056, -1248, -1184,
    -1888, -1824, -2016, -1952, -1632, -1568, -1760, -1696,
    -688, -656, -752, -720, -560, -528, -624, -592,
    -944, -912, -1008, -976, -816, -784, -880, -848,
    5504, 5248, 6016, 5760, 4480, 4224, 4992, 4736,
    7552, 7296, 8064, 7808, 6528, 6272, 7040, 6784,
    2752, 2624, 3008, 2880, 2240, 2112, 2496, 2368,
    3776, 3648, 4032, 3904, 3264, 3136, 3520, 3392,
    22016, 20992, 24064, 23040, 17920, 16896, 19968, 18944,
    30208, 29184, 32256, 31232, 26112, 25088, 28160, 27136,
    11008, 10496, 12032, 11520, 8960, 8448, 9984, 9472,
    15104, 14592, 16128, 15616, 13056, 12544, 14080, 13568,
    344, 328, 376, 360, 280, 264, 312, 296,
    472, 456, 504, 488, 408, 392, 440, 424,
    88, 72, 120, 104, 24, 8, 56, 40,
    216, 200, 248, 232, 152, 136, 184, 168,
    1376, 1312, 1504, 1440, 1120, 1056, 1248, 1184,
    1888, 1824, 2016, 1952, 1632, 1568, 1760, 1696,
    688, 656, 752, 720, 560, 528, 624, 592,
    944, 912, 1008, 976, 816, 784, 880, 848,
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
    // Mid-riser quantizer: the closest-to-silence code decodes to -8.
    SimpleTestFramework::assert_equals(-8, silence_pcm, "A-law closest-to-silence value (0x55) should map to PCM -8");
    
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

        SimpleTestFramework::assert_true(pcm_value < 0,
                "A-law value 0x" + std::to_string(alaw_value) +
                " should produce negative PCM, got " + std::to_string(pcm_value));
    }
    
    // A-law sign bit logic: bit 7 set (0x80-0xFF) = positive values
    for (int alaw_value = 0x80; alaw_value <= 0xFF; ++alaw_value) {
        int16_t pcm_value = alaw2linear(static_cast<uint8_t>(alaw_value));

        SimpleTestFramework::assert_true(pcm_value > 0,
                "A-law value 0x" + std::to_string(alaw_value) +
                " should produce positive PCM, got " + std::to_string(pcm_value));
    }
}

void test_alaw_amplitude_extremes_accuracy() {
    std::cout << "Testing A-law amplitude extremes..." << std::endl;
    
    // ITU-T G.711 reference values (interval midpoints, +8 offset)

    int16_t val_00 = alaw2linear(0x00);
    SimpleTestFramework::assert_equals(-5504, val_00,
                 "A-law (0x00) should produce -5504");

    // 0x2A is max negative
    int16_t max_neg = alaw2linear(0x2A);
    SimpleTestFramework::assert_equals(-32256, max_neg,
                 "Maximum negative A-law (0x2A) should produce -32256");

    // 0xAA is max positive
    int16_t max_pos = alaw2linear(0xAA);
    SimpleTestFramework::assert_equals(32256, max_pos,
                 "Maximum positive A-law (0xAA) should produce 32256");
}

void test_alaw_even_bit_inversion_accuracy() {
    std::cout << "Testing A-law even-bit inversion characteristic..." << std::endl;
    
    // Test specific values that demonstrate even-bit inversion
    int16_t val_54_pcm = alaw2linear(0x54);
    SimpleTestFramework::assert_equals(-24, val_54_pcm, "A-law 0x54 should produce -24");
    
    int16_t val_56_pcm = alaw2linear(0x56);
    SimpleTestFramework::assert_equals(-56, val_56_pcm, "A-law 0x56 should produce -56");
    
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
