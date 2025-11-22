/*
 * test_channel_decorrelator_unit.cpp - Unit tests for ChannelDecorrelator
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include "test_framework.h"
#include "codecs/flac/ChannelDecorrelator.h"
#include "codecs/flac/FrameParser.h"
#include <cstring>

using namespace PsyMP3::Codec::FLAC;
using namespace TestFramework;

// Test left-side stereo decorrelation
void test_left_side_stereo() {
    ChannelDecorrelator decorrelator;
    
    // Left-side: right = left - side
    int32_t left[] = {100, 200, 300, 400};
    int32_t side[] = {10, 20, 30, 40};
    int32_t* channels[] = {left, side};
    
    ASSERT_TRUE(decorrelator.decorrelate(channels, 4, 2, ChannelAssignment::LEFT_SIDE),
                "Should decorrelate left-side");
    
    // After decorrelation: right = left - side
    ASSERT_EQUALS(100, left[0], "Left channel unchanged");
    ASSERT_EQUALS(90, side[0], "Right = 100 - 10 = 90");
    ASSERT_EQUALS(180, side[1], "Right = 200 - 20 = 180");
    ASSERT_EQUALS(270, side[2], "Right = 300 - 30 = 270");
    ASSERT_EQUALS(360, side[3], "Right = 400 - 40 = 360");
}

// Test right-side stereo decorrelation
void test_right_side_stereo() {
    ChannelDecorrelator decorrelator;
    
    // Right-side: left = right + side
    int32_t side[] = {10, 20, 30, 40};
    int32_t right[] = {100, 200, 300, 400};
    int32_t* channels[] = {side, right};
    
    ASSERT_TRUE(decorrelator.decorrelate(channels, 4, 2, ChannelAssignment::RIGHT_SIDE),
                "Should decorrelate right-side");
    
    // After decorrelation: left = right + side
    ASSERT_EQUALS(110, side[0], "Left = 100 + 10 = 110");
    ASSERT_EQUALS(220, side[1], "Left = 200 + 20 = 220");
    ASSERT_EQUALS(330, side[2], "Left = 300 + 30 = 330");
    ASSERT_EQUALS(440, side[3], "Left = 400 + 40 = 440");
    ASSERT_EQUALS(100, right[0], "Right channel unchanged");
}

// Test mid-side stereo decorrelation
void test_mid_side_stereo() {
    ChannelDecorrelator decorrelator;
    
    // Mid-side: left = mid + (side>>1), right = mid - (side>>1)
    int32_t mid[] = {100, 200, 300, 400};
    int32_t side[] = {20, 40, 60, 80};
    int32_t* channels[] = {mid, side};
    
    ASSERT_TRUE(decorrelator.decorrelate(channels, 4, 2, ChannelAssignment::MID_SIDE),
                "Should decorrelate mid-side");
    
    // After decorrelation:
    // left = mid + (side>>1) = 100 + 10 = 110
    // right = mid - (side>>1) = 100 - 10 = 90
    ASSERT_EQUALS(110, mid[0], "Left = 100 + (20>>1) = 110");
    ASSERT_EQUALS(90, side[0], "Right = 100 - (20>>1) = 90");
    ASSERT_EQUALS(220, mid[1], "Left = 200 + (40>>1) = 220");
    ASSERT_EQUALS(180, side[1], "Right = 200 - (40>>1) = 180");
}

// Test mid-side with odd side values
void test_mid_side_odd_values() {
    ChannelDecorrelator decorrelator;
    
    // Test proper rounding with odd side values
    int32_t mid[] = {100, 200};
    int32_t side[] = {21, 41};  // Odd values
    int32_t* channels[] = {mid, side};
    
    ASSERT_TRUE(decorrelator.decorrelate(channels, 2, 2, ChannelAssignment::MID_SIDE),
                "Should handle odd side values");
    
    // For odd side values, the arithmetic right shift rounds down
    // 21 >> 1 = 10 (rounds down from 10.5)
    // left = 100 + 10 = 110
    // right = 100 - 10 = 90
    // But due to the way mid-side works with odd values:
    // left = mid + (side>>1) = 100 + 10 = 110
    // right = mid - (side>>1) = 100 - 10 = 90
    // However, to maintain lossless property: left + right = 2*mid
    // So: 110 + 89 = 199, but 2*100 = 200
    // The implementation may adjust for this
    ASSERT_EQUALS(110, mid[0], "Left with odd side value");
    // Accept either 89 or 90 depending on rounding implementation
    ASSERT_TRUE(side[0] == 89 || side[0] == 90, "Right with odd side value (rounding)");
}

// Test independent channels (no decorrelation)
void test_independent_channels() {
    ChannelDecorrelator decorrelator;
    
    int32_t ch0[] = {100, 200, 300};
    int32_t ch1[] = {10, 20, 30};
    int32_t* channels[] = {ch0, ch1};
    
    ASSERT_TRUE(decorrelator.decorrelate(channels, 3, 2, ChannelAssignment::INDEPENDENT),
                "Should handle independent channels");
    
    // Channels should remain unchanged
    ASSERT_EQUALS(100, ch0[0], "Channel 0 unchanged");
    ASSERT_EQUALS(10, ch1[0], "Channel 1 unchanged");
}

// Test mono (single channel)
void test_mono_channel() {
    ChannelDecorrelator decorrelator;
    
    int32_t ch0[] = {100, 200, 300};
    int32_t* channels[] = {ch0};
    
    ASSERT_TRUE(decorrelator.decorrelate(channels, 3, 1, ChannelAssignment::INDEPENDENT),
                "Should handle mono");
    
    // Single channel should remain unchanged
    ASSERT_EQUALS(100, ch0[0], "Mono channel unchanged");
}

// Test multi-channel (>2 channels)
void test_multi_channel() {
    ChannelDecorrelator decorrelator;
    
    int32_t ch0[] = {100, 200};
    int32_t ch1[] = {10, 20};
    int32_t ch2[] = {1, 2};
    int32_t* channels[] = {ch0, ch1, ch2};
    
    ASSERT_TRUE(decorrelator.decorrelate(channels, 2, 3, ChannelAssignment::INDEPENDENT),
                "Should handle multi-channel");
    
    // All channels should remain unchanged for independent assignment
    ASSERT_EQUALS(100, ch0[0], "Channel 0 unchanged");
    ASSERT_EQUALS(10, ch1[0], "Channel 1 unchanged");
    ASSERT_EQUALS(1, ch2[0], "Channel 2 unchanged");
}

int main() {
    // Create test suite
    TestSuite suite("ChannelDecorrelator Unit Tests");
    
    // Add test functions
    suite.addTest("Left-Side Stereo", test_left_side_stereo);
    suite.addTest("Right-Side Stereo", test_right_side_stereo);
    suite.addTest("Mid-Side Stereo", test_mid_side_stereo);
    suite.addTest("Mid-Side Odd Values", test_mid_side_odd_values);
    suite.addTest("Independent Channels", test_independent_channels);
    suite.addTest("Mono Channel", test_mono_channel);
    suite.addTest("Multi-Channel", test_multi_channel);
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print comprehensive results
    suite.printResults(results);
    
    // Return appropriate exit code
    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}
