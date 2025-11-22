/*
 * test_sample_reconstructor_unit.cpp - Unit tests for SampleReconstructor
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include "test_framework.h"
#include "codecs/flac/SampleReconstructor.h"
#include <cstring>

using namespace PsyMP3::Codec::FLAC;
using namespace TestFramework;

// Test 16-bit passthrough (no conversion)
void test_16bit_passthrough() {
    SampleReconstructor reconstructor;
    
    int32_t ch0[] = {100, 200, 300};
    int32_t ch1[] = {10, 20, 30};
    int32_t* channels[] = {ch0, ch1};
    
    int16_t output[6];
    memset(output, 0, sizeof(output));
    
    reconstructor.reconstructSamples(output, channels, 3, 2, 16);
    
    // Should be interleaved: ch0[0], ch1[0], ch0[1], ch1[1], ch0[2], ch1[2]
    ASSERT_EQUALS(100, static_cast<int>(output[0]), "Sample 0 ch0");
    ASSERT_EQUALS(10, static_cast<int>(output[1]), "Sample 0 ch1");
    ASSERT_EQUALS(200, static_cast<int>(output[2]), "Sample 1 ch0");
    ASSERT_EQUALS(20, static_cast<int>(output[3]), "Sample 1 ch1");
    ASSERT_EQUALS(300, static_cast<int>(output[4]), "Sample 2 ch0");
    ASSERT_EQUALS(30, static_cast<int>(output[5]), "Sample 2 ch1");
}

// Test 8-bit to 16-bit upscaling
void test_8bit_upscaling() {
    SampleReconstructor reconstructor;
    
    // 8-bit samples: left-shift by 8 to scale to 16-bit
    int32_t ch0[] = {10, 20};
    int32_t* channels[] = {ch0};
    
    int16_t output[2];
    memset(output, 0, sizeof(output));
    
    reconstructor.reconstructSamples(output, channels, 2, 1, 8);
    
    // 10 << 8 = 2560
    ASSERT_EQUALS(2560, static_cast<int>(output[0]), "8-bit upscaled: 10 << 8");
    // 20 << 8 = 5120
    ASSERT_EQUALS(5120, static_cast<int>(output[1]), "8-bit upscaled: 20 << 8");
}

// Test 24-bit to 16-bit downscaling
void test_24bit_downscaling() {
    SampleReconstructor reconstructor;
    
    // 24-bit samples: right-shift by 8 with rounding
    int32_t ch0[] = {0x100000, 0x200000};  // Large 24-bit values
    int32_t* channels[] = {ch0};
    
    int16_t output[2];
    memset(output, 0, sizeof(output));
    
    reconstructor.reconstructSamples(output, channels, 2, 1, 24);
    
    // 0x100000 >> 8 = 0x1000 = 4096
    ASSERT_EQUALS(4096, static_cast<int>(output[0]), "24-bit downscaled");
    // 0x200000 >> 8 = 0x2000 = 8192
    ASSERT_EQUALS(8192, static_cast<int>(output[1]), "24-bit downscaled");
}

// Test 32-bit to 16-bit downscaling
void test_32bit_downscaling() {
    SampleReconstructor reconstructor;
    
    // 32-bit samples: right-shift by 16 with rounding
    int32_t ch0[] = {0x10000, 0x20000};
    int32_t* channels[] = {ch0};
    
    int16_t output[2];
    memset(output, 0, sizeof(output));
    
    reconstructor.reconstructSamples(output, channels, 2, 1, 32);
    
    // 0x10000 >> 16 = 1
    ASSERT_EQUALS(1, static_cast<int>(output[0]), "32-bit downscaled");
    // 0x20000 >> 16 = 2
    ASSERT_EQUALS(2, static_cast<int>(output[1]), "32-bit downscaled");
}

// Test 20-bit to 16-bit downscaling
void test_20bit_downscaling() {
    SampleReconstructor reconstructor;
    
    // 20-bit samples: right-shift by 4 with rounding
    int32_t ch0[] = {0x1000, 0x2000};
    int32_t* channels[] = {ch0};
    
    int16_t output[2];
    memset(output, 0, sizeof(output));
    
    reconstructor.reconstructSamples(output, channels, 2, 1, 20);
    
    // 0x1000 >> 4 = 0x100 = 256
    ASSERT_EQUALS(256, static_cast<int>(output[0]), "20-bit downscaled");
    // 0x2000 >> 4 = 0x200 = 512
    ASSERT_EQUALS(512, static_cast<int>(output[1]), "20-bit downscaled");
}

// Test channel interleaving (stereo)
void test_stereo_interleaving() {
    SampleReconstructor reconstructor;
    
    int32_t left[] = {1, 2, 3, 4};
    int32_t right[] = {10, 20, 30, 40};
    int32_t* channels[] = {left, right};
    
    int16_t output[8];
    memset(output, 0, sizeof(output));
    
    reconstructor.reconstructSamples(output, channels, 4, 2, 16);
    
    // Should be interleaved: L, R, L, R, L, R, L, R
    ASSERT_EQUALS(1, static_cast<int>(output[0]), "Left 0");
    ASSERT_EQUALS(10, static_cast<int>(output[1]), "Right 0");
    ASSERT_EQUALS(2, static_cast<int>(output[2]), "Left 1");
    ASSERT_EQUALS(20, static_cast<int>(output[3]), "Right 1");
    ASSERT_EQUALS(3, static_cast<int>(output[4]), "Left 2");
    ASSERT_EQUALS(30, static_cast<int>(output[5]), "Right 2");
    ASSERT_EQUALS(4, static_cast<int>(output[6]), "Left 3");
    ASSERT_EQUALS(40, static_cast<int>(output[7]), "Right 3");
}

// Test multi-channel interleaving
void test_multi_channel_interleaving() {
    SampleReconstructor reconstructor;
    
    int32_t ch0[] = {1, 2};
    int32_t ch1[] = {10, 20};
    int32_t ch2[] = {100, 200};
    int32_t* channels[] = {ch0, ch1, ch2};
    
    int16_t output[6];
    memset(output, 0, sizeof(output));
    
    reconstructor.reconstructSamples(output, channels, 2, 3, 16);
    
    // Should be interleaved: ch0, ch1, ch2, ch0, ch1, ch2
    ASSERT_EQUALS(1, static_cast<int>(output[0]), "Ch0 sample 0");
    ASSERT_EQUALS(10, static_cast<int>(output[1]), "Ch1 sample 0");
    ASSERT_EQUALS(100, static_cast<int>(output[2]), "Ch2 sample 0");
    ASSERT_EQUALS(2, static_cast<int>(output[3]), "Ch0 sample 1");
    ASSERT_EQUALS(20, static_cast<int>(output[4]), "Ch1 sample 1");
    ASSERT_EQUALS(200, static_cast<int>(output[5]), "Ch2 sample 1");
}

// Test sample validation (clipping prevention)
void test_sample_validation() {
    SampleReconstructor reconstructor;
    
    // Test that samples are validated to be within 16-bit range
    // INT16_MIN = -32768, INT16_MAX = 32767
    
    int32_t ch0[] = {32767, -32768, 0};
    int32_t* channels[] = {ch0};
    
    int16_t output[3];
    memset(output, 0, sizeof(output));
    
    reconstructor.reconstructSamples(output, channels, 3, 1, 16);
    
    ASSERT_EQUALS(32767, static_cast<int>(output[0]), "Max 16-bit value");
    ASSERT_EQUALS(-32768, static_cast<int>(output[1]), "Min 16-bit value");
    ASSERT_EQUALS(0, static_cast<int>(output[2]), "Zero value");
}

int main() {
    // Create test suite
    TestSuite suite("SampleReconstructor Unit Tests");
    
    // Add test functions
    suite.addTest("16-bit Passthrough", test_16bit_passthrough);
    suite.addTest("8-bit Upscaling", test_8bit_upscaling);
    suite.addTest("24-bit Downscaling", test_24bit_downscaling);
    suite.addTest("32-bit Downscaling", test_32bit_downscaling);
    suite.addTest("20-bit Downscaling", test_20bit_downscaling);
    suite.addTest("Stereo Interleaving", test_stereo_interleaving);
    suite.addTest("Multi-Channel Interleaving", test_multi_channel_interleaving);
    suite.addTest("Sample Validation", test_sample_validation);
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print comprehensive results
    suite.printResults(results);
    
    // Return appropriate exit code
    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}
