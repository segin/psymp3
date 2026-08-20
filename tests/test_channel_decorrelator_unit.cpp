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
    int64_t left[] = {100, 200, 300, 400};
    int64_t side[] = {10, 20, 30, 40};
    int64_t* channels[] = {left, side};
    
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
    int64_t side[] = {10, 20, 30, 40};
    int64_t right[] = {100, 200, 300, 400};
    int64_t* channels[] = {side, right};
    
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
    
    // RFC 9639 §4.2 mid-side reconstruction: mid is shifted left 1 bit and
    // the side channel's LSB ORed in, then left = (mid+side)>>1 and
    // right = (mid-side)>>1. With even side values the LSB term is 0.
    int64_t mid[] = {100, 200, 300, 400};
    int64_t side[] = {20, 40, 60, 80};
    int64_t* channels[] = {mid, side};

    ASSERT_TRUE(decorrelator.decorrelate(channels, 4, 2, ChannelAssignment::MID_SIDE),
                "Should decorrelate mid-side");

    // mid=100, side=20: mid<<1 = 200; left = (200+20)>>1 = 110, right = (200-20)>>1 = 90
    ASSERT_EQUALS(110, mid[0], "Left = (200+20)>>1 = 110");
    ASSERT_EQUALS(90, side[0], "Right = (200-20)>>1 = 90");
    ASSERT_EQUALS(220, mid[1], "Left = (400+40)>>1 = 220");
    ASSERT_EQUALS(180, side[1], "Right = (400-40)>>1 = 180");
}

// Test mid-side with odd side values
void test_mid_side_odd_values() {
    ChannelDecorrelator decorrelator;
    
    // An odd side sample means the encoder's mid channel (floor((L+R)/2))
    // dropped a set LSB; RFC 9639 §4.2 restores it: "if a side channel
    // sample is odd, 1 has to be added to the corresponding mid channel
    // sample after it has been shifted left by 1 bit".
    int64_t mid[] = {100, 200};
    int64_t side[] = {21, 41};  // Odd values
    int64_t* channels[] = {mid, side};

    ASSERT_TRUE(decorrelator.decorrelate(channels, 2, 2, ChannelAssignment::MID_SIDE),
                "Should handle odd side values");

    // mid=100, side=21: (100<<1)|1 = 201; left = (201+21)>>1 = 111,
    // right = (201-21)>>1 = 90. Losslessness check: 111-90 = 21 = side,
    // floor((111+90)/2) = 100 = mid.
    ASSERT_EQUALS(111, mid[0], "Left with odd side value");
    ASSERT_EQUALS(90, side[0], "Right with odd side value");
    // mid=200, side=41: (200<<1)|1 = 401; left = 221, right = 180.
    ASSERT_EQUALS(221, mid[1], "Left with odd side value (second sample)");
    ASSERT_EQUALS(180, side[1], "Right with odd side value (second sample)");
}

// Test independent channels (no decorrelation)
void test_independent_channels() {
    ChannelDecorrelator decorrelator;
    
    int64_t ch0[] = {100, 200, 300};
    int64_t ch1[] = {10, 20, 30};
    int64_t* channels[] = {ch0, ch1};
    
    ASSERT_TRUE(decorrelator.decorrelate(channels, 3, 2, ChannelAssignment::INDEPENDENT),
                "Should handle independent channels");
    
    // Channels should remain unchanged
    ASSERT_EQUALS(100, ch0[0], "Channel 0 unchanged");
    ASSERT_EQUALS(10, ch1[0], "Channel 1 unchanged");
}

// Test mono (single channel)
void test_mono_channel() {
    ChannelDecorrelator decorrelator;
    
    int64_t ch0[] = {100, 200, 300};
    int64_t* channels[] = {ch0};
    
    ASSERT_TRUE(decorrelator.decorrelate(channels, 3, 1, ChannelAssignment::INDEPENDENT),
                "Should handle mono");
    
    // Single channel should remain unchanged
    ASSERT_EQUALS(100, ch0[0], "Mono channel unchanged");
}

// Test multi-channel (>2 channels)
void test_multi_channel() {
    ChannelDecorrelator decorrelator;
    
    int64_t ch0[] = {100, 200};
    int64_t ch1[] = {10, 20};
    int64_t ch2[] = {1, 2};
    int64_t* channels[] = {ch0, ch1, ch2};
    
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
