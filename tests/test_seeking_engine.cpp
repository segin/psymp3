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
#include <complex>
#include <sstream>

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

void test_seek_to_timestamp_uses_track_timescale_for_aac() {
    SeekingEngine engine;
    SampleTableInfo tables;
    tables.chunkOffsets = {44};
    tables.sampleToChunkEntries.push_back({0, 45, 1});
    tables.sampleSizes.assign(45, 128);

    uint64_t currentTime = 0;
    for (int i = 0; i < 45; ++i) {
        tables.sampleTimes.push_back(currentTime);
        currentTime += 1024;
    }

    SampleTableManager manager;
    ASSERT_TRUE(manager.BuildSampleTables(tables, 44100),
                "AAC sample tables should build with the track timescale");

    AudioTrackInfo track = {};
    track.trackId = 1;
    track.codecType = "aac";
    track.sampleRate = 44100;
    track.timescale = 44100;
    track.currentSampleIndex = 0;

    ASSERT_TRUE(engine.SeekToTimestamp(1.0, track, manager),
                "Seeking to 1.0 seconds in AAC tables should succeed");
    ASSERT_EQUALS(43ULL, track.currentSampleIndex,
                  "AAC seek should land on the access unit covering 1.0 seconds");
}

int main() {
    TestSuite suite("SeekingEngine Tests");
    suite.addTest("BinarySearchTimeToSample", test_binary_search_time_to_sample);
    suite.addTest("AACTrackTimescaleSeek", test_seek_to_timestamp_uses_track_timescale_for_aac);
    auto results = suite.runAll();
    suite.printResults(results);
    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}
