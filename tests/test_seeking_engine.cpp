/*
 * test_seeking_engine.cpp - Unit tests for SeekingEngine
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

// Include standard headers first to avoid #define private public messing them up
#include <vector>
#include <string>
#include <cstdint>
#include <map>
#include <memory>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <functional>
#include <chrono>

// Expose private members for testing
#define private public
#include "psymp3.h"
#undef private

#include "test_framework.h"

using namespace PsyMP3::Demuxer::ISO;
using namespace TestFramework;

void test_binary_search_time_to_sample() {
    SeekingEngine engine;
    std::vector<SampleTableManager::SampleInfo> samples;

    // Create samples with uniform duration of 1000ms (1s)
    // Total duration: 5 seconds
    for (int i = 0; i < 5; ++i) {
        SampleTableManager::SampleInfo info;
        info.duration = 1000;
        info.size = 100;
        info.offset = i * 100;
        info.isKeyframe = true;
        samples.push_back(info);
    }

    // Test case 1: Find sample at 0.5s (should be index 0)
    // 0.5s < 1.0s (duration of first sample)
    uint64_t index0 = engine.BinarySearchTimeToSample(0.5, samples);
    ASSERT_EQUALS(0ULL, index0, "0.5s should map to sample 0");

    // Test case 2: Find sample at 1.5s (should be index 1)
    // 1.5s is in the second sample (1.0s to 2.0s)
    uint64_t index1 = engine.BinarySearchTimeToSample(1.5, samples);
    ASSERT_EQUALS(1ULL, index1, "1.5s should map to sample 1");

    // Test case 3: Find sample at 4.5s (should be index 4)
    uint64_t index4 = engine.BinarySearchTimeToSample(4.5, samples);
    ASSERT_EQUALS(4ULL, index4, "4.5s should map to sample 4");

    // Test case 4: Out of bounds (should return last sample?)
    uint64_t indexLast = engine.BinarySearchTimeToSample(10.0, samples);
    ASSERT_EQUALS(4ULL, indexLast, "10.0s should map to last sample");
}

int main() {
    TestSuite suite("SeekingEngine Tests");
    suite.addTest("BinarySearchTimeToSample", test_binary_search_time_to_sample);
    auto results = suite.runAll();
    suite.printResults(results);
    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}
