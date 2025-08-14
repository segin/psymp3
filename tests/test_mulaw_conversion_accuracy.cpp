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

/**
 * @brief ITU-T G.711 μ-law reference implementation for validation
 */
class ITUTMuLawReference {
public:
    static int16_t mulawToPcm(uint8_t mulaw_sample) {
        // ITU-T G.711 μ-law decoding algorithm
        uint8_t complement = ~mulaw_sample;
        bool sign = (complement & 0x80) != 0;
        uint8_t exponent = (complement & 0x70) >> 4;
        uint8_t mantissa = complement & 0x0F;
        
        int16_t linear;
        if (exponent == 0) {
            linear = 33 + 2 * mantissa;
        } else {
            linear = (33 + 2 * mantissa) << exponent;
        }
        
        if (sign) {
            linear = -linear;
        }
        
        return linear;
    }
    
    static std::array<int16_t, 256> getExpectedValues() {
        std::array<int16_t, 256> expected;
        for (int i = 0; i < 256; ++i) {
            expected[i] = mulawToPcm(static_cast<uint8_t>(i));
        }
        return expected;
    }
};

/**
 * @brief μ-law lookup table from MuLawCodec implementation
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
    std::cout << "Testing all 256 μ-law values for accuracy..." << std::endl;
    
    auto expected_values = ITUTMuLawReference::getExpectedValues();
    
    for (int mulaw_value = 0; mulaw_value < 256; ++mulaw_value) {
        int16_t expected_pcm = expected_values[mulaw_value];
        int16_t actual_pcm = MULAW_TO_PCM_TEST[mulaw_value];
        
        SimpleTestFramework::assert_equals(expected_pcm, actual_pcm,
                     "μ-law value 0x" + std::to_string(mulaw_value) + 
                     " should convert to PCM " + std::to_string(expected_pcm) + 
                     " but got " + std::to_string(actual_pcm));
    }
}

void test_mulaw_silence_value_accuracy() {
    std::cout << "Testing μ-law silence value (0xFF)..." << std::endl;
    
    int16_t silence_pcm = MULAW_TO_PCM_TEST[0xFF];
    SimpleTestFramework::assert_equals(0, silence_pcm, "μ-law silence value (0xFF) must map to PCM 0");
    
    int16_t expected_silence = ITUTMuLawReference::mulawToPcm(0xFF);
    SimpleTestFramework::assert_equals(expected_silence, silence_pcm, 
                 "Silence value should match ITU-T reference implementation");
}

void test_mulaw_sign_bit_accuracy() {
    std::cout << "Testing μ-law sign bit handling..." << std::endl;
    
    for (int mulaw_value = 0x00; mulaw_value <= 0x7F; ++mulaw_value) {
        int16_t pcm_value = MULAW_TO_PCM_TEST[mulaw_value];
        SimpleTestFramework::assert_true(pcm_value < 0, 
                   "μ-law value 0x" + std::to_string(mulaw_value) + 
                   " should produce negative PCM, got " + std::to_string(pcm_value));
    }
    
    for (int mulaw_value = 0x80; mulaw_value <= 0xFE; ++mulaw_value) {
        int16_t pcm_value = MULAW_TO_PCM_TEST[mulaw_value];
        SimpleTestFramework::assert_true(pcm_value > 0, 
                   "μ-law value 0x" + std::to_string(mulaw_value) + 
                   " should produce positive PCM, got " + std::to_string(pcm_value));
    }
}

void test_mulaw_amplitude_extremes_accuracy() {
    std::cout << "Testing μ-law amplitude extremes..." << std::endl;
    
    int16_t max_neg_pcm = MULAW_TO_PCM_TEST[0x00];
    int16_t expected_max_neg = ITUTMuLawReference::mulawToPcm(0x00);
    
    SimpleTestFramework::assert_equals(expected_max_neg, max_neg_pcm, 
                 "Maximum negative μ-law (0x00) should produce " + 
                 std::to_string(expected_max_neg) + ", got " + std::to_string(max_neg_pcm));
    SimpleTestFramework::assert_true(max_neg_pcm < -30000, 
               "Maximum negative amplitude should be less than -30000");
    
    int16_t max_pos_pcm = MULAW_TO_PCM_TEST[0x80];
    int16_t expected_max_pos = ITUTMuLawReference::mulawToPcm(0x80);
    
    SimpleTestFramework::assert_equals(expected_max_pos, max_pos_pcm, 
                 "Maximum positive μ-law (0x80) should produce " + 
                 std::to_string(expected_max_pos) + ", got " + std::to_string(max_pos_pcm));
    SimpleTestFramework::assert_true(max_pos_pcm > 30000, 
               "Maximum positive amplitude should be greater than 30000");
}

void test_mulaw_edge_cases_accuracy() {
    std::cout << "Testing μ-law edge cases and boundaries..." << std::endl;
    
    int16_t min_neg_pcm = MULAW_TO_PCM_TEST[0x7F];
    int16_t expected_min_neg = ITUTMuLawReference::mulawToPcm(0x7F);
    
    SimpleTestFramework::assert_equals(expected_min_neg, min_neg_pcm, 
                 "Minimum negative μ-law (0x7F) should produce " + 
                 std::to_string(expected_min_neg) + ", got " + std::to_string(min_neg_pcm));
    SimpleTestFramework::assert_true(min_neg_pcm < 0, "Minimum negative should still be negative");
    
    int16_t min_pos_pcm = MULAW_TO_PCM_TEST[0xFE];
    int16_t expected_min_pos = ITUTMuLawReference::mulawToPcm(0xFE);
    
    SimpleTestFramework::assert_equals(expected_min_pos, min_pos_pcm, 
                 "Minimum positive μ-law (0xFE) should produce " + 
                 std::to_string(expected_min_pos) + ", got " + std::to_string(min_pos_pcm));
    SimpleTestFramework::assert_true(min_pos_pcm > 0, "Minimum positive should still be positive");
    
    std::vector<uint8_t> boundary_values = {
        0x0F, 0x10, 0x1F, 0x20, 0x2F, 0x30, 0x3F, 0x40,
        0x4F, 0x50, 0x5F, 0x60, 0x6F, 0x70, 0x7F,
        0x8F, 0x90, 0x9F, 0xA0, 0xAF, 0xB0, 0xBF, 0xC0,
        0xCF, 0xD0, 0xDF, 0xE0, 0xEF, 0xF0
    };
    
    for (uint8_t boundary_value : boundary_values) {
        int16_t actual_pcm = MULAW_TO_PCM_TEST[boundary_value];
        int16_t expected_pcm = ITUTMuLawReference::mulawToPcm(boundary_value);
        
        SimpleTestFramework::assert_equals(expected_pcm, actual_pcm,
                     "Boundary μ-law value 0x" + std::to_string(boundary_value) + 
                     " should produce PCM " + std::to_string(expected_pcm) + 
                     ", got " + std::to_string(actual_pcm));
    }
}

void test_mulaw_bitperfect_accuracy() {
    std::cout << "Testing bit-perfect accuracy against ITU-T reference..." << std::endl;
    
    auto expected_values = ITUTMuLawReference::getExpectedValues();
    
    bool all_accurate = true;
    std::string first_mismatch;
    
    for (int i = 0; i < 256; ++i) {
        if (MULAW_TO_PCM_TEST[i] != expected_values[i]) {
            if (all_accurate) {
                first_mismatch = "First mismatch at μ-law 0x" + std::to_string(i) + 
                               ": expected " + std::to_string(expected_values[i]) + 
                               ", got " + std::to_string(MULAW_TO_PCM_TEST[i]);
            }
            all_accurate = false;
        }
    }
    
    SimpleTestFramework::assert_true(all_accurate, "All μ-law values must be bit-perfect accurate. " + first_mismatch);
}

int main() {
    std::cout << "μ-law Conversion Accuracy Tests" << std::endl;
    std::cout << "===============================" << std::endl;
    
    test_all_mulaw_values_accuracy();
    test_mulaw_silence_value_accuracy();
    test_mulaw_sign_bit_accuracy();
    test_mulaw_amplitude_extremes_accuracy();
    test_mulaw_edge_cases_accuracy();
    test_mulaw_bitperfect_accuracy();
    
    SimpleTestFramework::print_results();
    
    return SimpleTestFramework::get_failure_count();
}