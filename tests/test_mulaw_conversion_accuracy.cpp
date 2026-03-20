/*
 * test_mulaw_conversion_accuracy.cpp - Comprehensive μ-law conversion accuracy tests
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
 * @brief μ-law lookup table from MuLawCodec implementation
 *
 * This table contains the expected 16-bit scaled PCM values produced by
 * the current G711::ulaw2linear implementation.
 */
const int16_t MULAW_TO_PCM_TEST[256] = {
    -32124, -31100, -30076, -29052, -28028, -27004, -25980, -24956,
    -23932, -22908, -21884, -20860, -19836, -18812, -17788, -16764,
    -15996, -15484, -14972, -14460, -13948, -13436, -12924, -12412,
    -11900, -11388, -10876, -10364,  -9852,  -9340,  -8828,  -8316,
     -7932,  -7676,  -7420,  -7164,  -6908,  -6652,  -6396,  -6140,
     -5884,  -5628,  -5372,  -5116,  -4860,  -4604,  -4348,  -4092,
     -3900,  -3772,  -3644,  -3516,  -3388,  -3260,  -3132,  -3004,
     -2876,  -2748,  -2620,  -2492,  -2364,  -2236,  -2108,  -1980,
     -1884,  -1820,  -1756,  -1692,  -1628,  -1564,  -1500,  -1436,
     -1372,  -1308,  -1244,  -1180,  -1116,  -1052,   -988,   -924,
      -876,   -844,   -812,   -780,   -748,   -716,   -684,   -652,
      -620,   -588,   -556,   -524,   -492,   -460,   -428,   -396,
      -372,   -356,   -340,   -324,   -308,   -292,   -276,   -260,
      -244,   -228,   -212,   -196,   -180,   -164,   -148,   -132,
      -120,   -112,   -104,    -96,    -88,    -80,    -72,    -64,
       -56,    -48,    -40,    -32,    -24,    -16,     -8,      0,
     32124,  31100,  30076,  29052,  28028,  27004,  25980,  24956,
     23932,  22908,  21884,  20860,  19836,  18812,  17788,  16764,
     15996,  15484,  14972,  14460,  13948,  13436,  12924,  12412,
     11900,  11388,  10876,  10364,   9852,   9340,   8828,   8316,
      7932,   7676,   7420,   7164,   6908,   6652,   6396,   6140,
      5884,   5628,   5372,   5116,   4860,   4604,   4348,   4092,
      3900,   3772,   3644,   3516,   3388,   3260,   3132,   3004,
      2876,   2748,   2620,   2492,   2364,   2236,   2108,   1980,
      1884,   1820,   1756,   1692,   1628,   1564,   1500,   1436,
      1372,   1308,   1244,   1180,   1116,   1052,    988,    924,
       876,    844,    812,    780,    748,    716,    684,    652,
       620,    588,    556,    524,    492,    460,    428,    396,
       372,    356,    340,    324,    308,    292,    276,    260,
       244,    228,    212,    196,    180,    164,    148,    132,
       120,    112,    104,     96,     88,     80,     72,     64,
        56,     48,     40,     32,     24,     16,      8,      0
};

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

void test_all_mulaw_values_accuracy() {
    std::cout << "Testing all 256 μ-law values for accuracy against internal table..." << std::endl;
    
    for (int mulaw_value = 0; mulaw_value < 256; ++mulaw_value) {
        int16_t expected_pcm = MULAW_TO_PCM_TEST[mulaw_value];
        int16_t actual_pcm = ulaw2linear(static_cast<uint8_t>(mulaw_value));
        
        SimpleTestFramework::assert_equals(expected_pcm, actual_pcm,
                     "μ-law value 0x" + std::to_string(mulaw_value) + 
                     " mismatch");
    }
}

void test_mulaw_silence_value_accuracy() {
    std::cout << "Testing μ-law silence value (0xFF)..." << std::endl;
    
    int16_t silence_pcm = ulaw2linear(0xFF);
    SimpleTestFramework::assert_equals(0, silence_pcm, "μ-law silence value (0xFF) must map to PCM 0");
    
    int16_t val_7F = ulaw2linear(0x7F);
    SimpleTestFramework::assert_equals(0, val_7F, "μ-law 0x7F is also silence (0)");
}

void test_mulaw_sign_bit_accuracy() {
    std::cout << "Testing μ-law sign bit handling..." << std::endl;
    
    for (int mulaw_value = 0x00; mulaw_value <= 0x7F; ++mulaw_value) {
        int16_t pcm_value = ulaw2linear(static_cast<uint8_t>(mulaw_value));

        // 0x7F is 0, so handle it
        if (mulaw_value != 0x7F) {
            SimpleTestFramework::assert_true(pcm_value < 0,
                    "μ-law value 0x" + std::to_string(mulaw_value) +
                    " should produce negative PCM, got " + std::to_string(pcm_value));
        }
    }
    
    for (int mulaw_value = 0x80; mulaw_value <= 0xFE; ++mulaw_value) {
        int16_t pcm_value = ulaw2linear(static_cast<uint8_t>(mulaw_value));
        // 0xFF is 0, already excluded by loop range
        SimpleTestFramework::assert_true(pcm_value > 0, 
                   "μ-law value 0x" + std::to_string(mulaw_value) + 
                   " should produce positive PCM, got " + std::to_string(pcm_value));
    }
}

void test_mulaw_amplitude_extremes_accuracy() {
    std::cout << "Testing μ-law amplitude extremes..." << std::endl;
    
    int16_t max_neg_pcm = ulaw2linear(0x00);
    // Implementation specific: Matches -32124 (full 16-bit range scaling)
    SimpleTestFramework::assert_equals(-32124, max_neg_pcm,
                 "Maximum negative μ-law (0x00) should produce -32124");
    
    int16_t max_pos_pcm = ulaw2linear(0x80);
    SimpleTestFramework::assert_equals(32124, max_pos_pcm,
                 "Maximum positive μ-law (0x80) should produce 32124");
}

int main() {
    std::cout << "μ-law Conversion Accuracy Tests" << std::endl;
    std::cout << "===============================" << std::endl;
    
    test_all_mulaw_values_accuracy();
    test_mulaw_silence_value_accuracy();
    test_mulaw_sign_bit_accuracy();
    test_mulaw_amplitude_extremes_accuracy();
    
    SimpleTestFramework::print_results();
    
    return SimpleTestFramework::get_failure_count();
}
