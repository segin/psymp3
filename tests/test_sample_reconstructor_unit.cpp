/*
 * test_sample_reconstructor_unit.cpp - Unit tests for SampleReconstructor
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * These tests were written when the reconstructor converted every source
 * depth down to 16-bit. Since the pipeline was widened, it scales every depth
 * UP instead: the source's most significant bit is aligned with bit 31, so a
 * sample of depth d is multiplied by 1 << (32 - d) and clamped. The tests
 * below check that rule rather than the old one, which is why nothing here
 * "downscales" any more.
 */

#include "psymp3.h"
#include "test_framework.h"
#include "codecs/flac/SampleReconstructor.h"
#include <cstring>

using namespace PsyMP3::Codec::FLAC;
using namespace TestFramework;

namespace {

/// The scale the reconstructor applies to a sample of the given depth.
constexpr int64_t scaleFor(uint32_t depth) {
    return static_cast<int64_t>(1) << (32 - depth);
}

} // namespace

// 16-bit sources are scaled up by 2^16.
void test_16bit_to_full_scale() {
    SampleReconstructor reconstructor;

    int32_t ch0[] = {100, 200, 300};
    int32_t ch1[] = {10, 20, 30};
    int32_t* channels[] = {ch0, ch1};

    AudioSample output[6];
    memset(output, 0, sizeof(output));

    reconstructor.reconstructSamples(output, channels, 3, 2, 16);

    const int64_t s = scaleFor(16);
    // Should be interleaved: ch0[0], ch1[0], ch0[1], ch1[1], ch0[2], ch1[2]
    ASSERT_EQUALS(100 * s, static_cast<int64_t>(output[0]), "Sample 0 ch0");
    ASSERT_EQUALS(10 * s,  static_cast<int64_t>(output[1]), "Sample 0 ch1");
    ASSERT_EQUALS(200 * s, static_cast<int64_t>(output[2]), "Sample 1 ch0");
    ASSERT_EQUALS(20 * s,  static_cast<int64_t>(output[3]), "Sample 1 ch1");
    ASSERT_EQUALS(300 * s, static_cast<int64_t>(output[4]), "Sample 2 ch0");
    ASSERT_EQUALS(30 * s,  static_cast<int64_t>(output[5]), "Sample 2 ch1");
}

// 8-bit sources take the largest shift of all: 2^24.
void test_8bit_to_full_scale() {
    SampleReconstructor reconstructor;

    int32_t ch0[] = {10, 20};
    int32_t* channels[] = {ch0};

    AudioSample output[2];
    memset(output, 0, sizeof(output));

    reconstructor.reconstructSamples(output, channels, 2, 1, 8);

    const int64_t s = scaleFor(8);
    ASSERT_EQUALS(10 * s, static_cast<int64_t>(output[0]), "8-bit scaled: 10 << 24");
    ASSERT_EQUALS(20 * s, static_cast<int64_t>(output[1]), "8-bit scaled: 20 << 24");
}

// 24-bit sources are scaled up by 2^8 -- they used to be shifted DOWN by 8,
// which threw away exactly the resolution a lossless format exists to keep.
void test_24bit_to_full_scale() {
    SampleReconstructor reconstructor;

    int32_t ch0[] = {0x100000, 0x200000};
    int32_t* channels[] = {ch0};

    AudioSample output[2];
    memset(output, 0, sizeof(output));

    reconstructor.reconstructSamples(output, channels, 2, 1, 24);

    const int64_t s = scaleFor(24);
    ASSERT_EQUALS(0x100000 * s, static_cast<int64_t>(output[0]), "24-bit scaled");
    ASSERT_EQUALS(0x200000 * s, static_cast<int64_t>(output[1]), "24-bit scaled");
}

// 32-bit sources already fill the range and pass through untouched.
void test_32bit_passthrough() {
    SampleReconstructor reconstructor;

    int32_t ch0[] = {0x10000, 0x20000};
    int32_t* channels[] = {ch0};

    AudioSample output[2];
    memset(output, 0, sizeof(output));

    reconstructor.reconstructSamples(output, channels, 2, 1, 32);

    ASSERT_EQUALS(0x10000, static_cast<int64_t>(output[0]), "32-bit passthrough");
    ASSERT_EQUALS(0x20000, static_cast<int64_t>(output[1]), "32-bit passthrough");
}

// A source at its own peak must land at the edge of the output range without
// wrapping. Scaling is done in int64 precisely so that shifting these values
// cannot overflow on the way.
void test_peak_handling_without_wrap() {
    SampleReconstructor reconstructor;

    int32_t peak24[] = {8388607, -8388608};
    int32_t* peak24_channels[] = {peak24};
    AudioSample peak24_output[2];
    memset(peak24_output, 0, sizeof(peak24_output));

    reconstructor.reconstructSamples(peak24_output, peak24_channels, 2, 1, 24);

    // 8388607 << 8 is just short of INT32_MAX; -8388608 << 8 is exactly
    // INT32_MIN. Neither needs clamping, and neither may wrap.
    ASSERT_EQUALS(8388607LL * scaleFor(24), static_cast<int64_t>(peak24_output[0]),
                  "24-bit positive peak scales without wrapping");
    ASSERT_EQUALS(-8388608LL * scaleFor(24), static_cast<int64_t>(peak24_output[1]),
                  "24-bit negative peak scales without wrapping");

    int32_t peak32[] = {2147483647, static_cast<int32_t>(0x80000000u)};
    int32_t* peak32_channels[] = {peak32};
    AudioSample peak32_output[2];
    memset(peak32_output, 0, sizeof(peak32_output));

    reconstructor.reconstructSamples(peak32_output, peak32_channels, 2, 1, 32);

    ASSERT_EQUALS(2147483647LL, static_cast<int64_t>(peak32_output[0]),
                  "32-bit positive peak passes through");
    ASSERT_EQUALS(-2147483648LL, static_cast<int64_t>(peak32_output[1]),
                  "32-bit negative peak passes through");
}

// A sample wider than its declared depth is clamped, not wrapped: a malformed
// stream can present one, and shifting it would otherwise overflow.
void test_oversized_sample_is_clamped() {
    SampleReconstructor reconstructor;

    // Far wider than the 16 bits the call declares.
    int32_t ch0[] = {1 << 20, -(1 << 20)};
    int32_t* channels[] = {ch0};

    AudioSample output[2];
    memset(output, 0, sizeof(output));

    reconstructor.reconstructSamples(output, channels, 2, 1, 16);

    ASSERT_EQUALS(2147483647LL, static_cast<int64_t>(output[0]),
                  "Oversized positive sample clamps to INT32_MAX");
    ASSERT_EQUALS(-2147483648LL, static_cast<int64_t>(output[1]),
                  "Oversized negative sample clamps to INT32_MIN");
}

// 20-bit sources are scaled up by 2^12.
void test_20bit_to_full_scale() {
    SampleReconstructor reconstructor;

    int32_t ch0[] = {0x1000, 0x2000};
    int32_t* channels[] = {ch0};

    AudioSample output[2];
    memset(output, 0, sizeof(output));

    reconstructor.reconstructSamples(output, channels, 2, 1, 20);

    const int64_t s = scaleFor(20);
    ASSERT_EQUALS(0x1000 * s, static_cast<int64_t>(output[0]), "20-bit scaled");
    ASSERT_EQUALS(0x2000 * s, static_cast<int64_t>(output[1]), "20-bit scaled");
}

// Interleaving is independent of the scaling.
void test_stereo_interleaving() {
    SampleReconstructor reconstructor;

    int32_t left[] = {1, 2, 3, 4};
    int32_t right[] = {10, 20, 30, 40};
    int32_t* channels[] = {left, right};

    AudioSample output[8];
    memset(output, 0, sizeof(output));

    reconstructor.reconstructSamples(output, channels, 4, 2, 16);

    const int64_t s = scaleFor(16);
    // Should be interleaved: L, R, L, R, L, R, L, R
    ASSERT_EQUALS(1 * s,  static_cast<int64_t>(output[0]), "Left 0");
    ASSERT_EQUALS(10 * s, static_cast<int64_t>(output[1]), "Right 0");
    ASSERT_EQUALS(2 * s,  static_cast<int64_t>(output[2]), "Left 1");
    ASSERT_EQUALS(20 * s, static_cast<int64_t>(output[3]), "Right 1");
    ASSERT_EQUALS(3 * s,  static_cast<int64_t>(output[4]), "Left 2");
    ASSERT_EQUALS(30 * s, static_cast<int64_t>(output[5]), "Right 2");
    ASSERT_EQUALS(4 * s,  static_cast<int64_t>(output[6]), "Left 3");
    ASSERT_EQUALS(40 * s, static_cast<int64_t>(output[7]), "Right 3");
}

void test_multi_channel_interleaving() {
    SampleReconstructor reconstructor;

    int32_t ch0[] = {1, 2};
    int32_t ch1[] = {10, 20};
    int32_t ch2[] = {100, 200};
    int32_t* channels[] = {ch0, ch1, ch2};

    AudioSample output[6];
    memset(output, 0, sizeof(output));

    reconstructor.reconstructSamples(output, channels, 2, 3, 16);

    const int64_t s = scaleFor(16);
    // Should be interleaved: ch0, ch1, ch2, ch0, ch1, ch2
    ASSERT_EQUALS(1 * s,   static_cast<int64_t>(output[0]), "Ch0 sample 0");
    ASSERT_EQUALS(10 * s,  static_cast<int64_t>(output[1]), "Ch1 sample 0");
    ASSERT_EQUALS(100 * s, static_cast<int64_t>(output[2]), "Ch2 sample 0");
    ASSERT_EQUALS(2 * s,   static_cast<int64_t>(output[3]), "Ch0 sample 1");
    ASSERT_EQUALS(20 * s,  static_cast<int64_t>(output[4]), "Ch1 sample 1");
    ASSERT_EQUALS(200 * s, static_cast<int64_t>(output[5]), "Ch2 sample 1");
}

// The extremes of a 16-bit source map to the extremes of the output.
void test_16bit_extremes() {
    SampleReconstructor reconstructor;

    int32_t ch0[] = {32767, -32768, 0};
    int32_t* channels[] = {ch0};

    AudioSample output[3];
    memset(output, 0, sizeof(output));

    reconstructor.reconstructSamples(output, channels, 3, 1, 16);

    const int64_t s = scaleFor(16);
    ASSERT_EQUALS(32767 * s, static_cast<int64_t>(output[0]), "Max 16-bit value");
    ASSERT_EQUALS(-32768 * s, static_cast<int64_t>(output[1]), "Min 16-bit value");
    ASSERT_EQUALS(0LL, static_cast<int64_t>(output[2]), "Zero value");
}

int main() {
    TestSuite suite("SampleReconstructor Unit Tests");

    suite.addTest("16-bit to full scale", test_16bit_to_full_scale);
    suite.addTest("8-bit to full scale", test_8bit_to_full_scale);
    suite.addTest("24-bit to full scale", test_24bit_to_full_scale);
    suite.addTest("32-bit passthrough", test_32bit_passthrough);
    suite.addTest("Peak handling without wrap", test_peak_handling_without_wrap);
    suite.addTest("Oversized sample is clamped", test_oversized_sample_is_clamped);
    suite.addTest("20-bit to full scale", test_20bit_to_full_scale);
    suite.addTest("Stereo Interleaving", test_stereo_interleaving);
    suite.addTest("Multi-Channel Interleaving", test_multi_channel_interleaving);
    suite.addTest("16-bit extremes", test_16bit_extremes);

    auto results = suite.runAll();
    suite.printResults(results);

    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}
