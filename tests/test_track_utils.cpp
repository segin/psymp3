/*
 * tests/test_track_utils.cpp - Unit tests for track utility functions
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"

using namespace TestFramework;

class TrackUtilsTest : public TestCase {
public:
    TrackUtilsTest() : TestCase("Track utility functions tests") {}

protected:
    void runTest() override {
        // Test isKnownRawAudioExtension

        // Positive cases (known raw extensions)
        ASSERT_TRUE(track::isKnownRawAudioExtension(".ulaw"), ".ulaw should be known raw extension");
        ASSERT_TRUE(track::isKnownRawAudioExtension(".ULAW"), ".ULAW should be case-insensitive");
        ASSERT_TRUE(track::isKnownRawAudioExtension("test.ulaw"), "test.ulaw should be recognized");
        ASSERT_TRUE(track::isKnownRawAudioExtension("/path/to/file.pcm"), "path with .pcm should be recognized");
        ASSERT_TRUE(track::isKnownRawAudioExtension(".722"), ".722 should be recognized");
        ASSERT_TRUE(track::isKnownRawAudioExtension(".f64be"), ".f64be should be recognized");

        // Negative cases (non-raw extensions)
        ASSERT_FALSE(track::isKnownRawAudioExtension(".mp3"), ".mp3 is not raw audio");
        ASSERT_FALSE(track::isKnownRawAudioExtension(".flac"), ".flac is not raw audio");
        ASSERT_FALSE(track::isKnownRawAudioExtension(".wav"), ".wav is not raw audio (it is a container)");
        ASSERT_FALSE(track::isKnownRawAudioExtension("test.txt"), ".txt is not raw audio");

        // Edge cases
        ASSERT_FALSE(track::isKnownRawAudioExtension(""), "Empty path should not be raw extension");
        ASSERT_FALSE(track::isKnownRawAudioExtension("no_extension"), "Path without extension should not be raw");
        ASSERT_FALSE(track::isKnownRawAudioExtension("."), "Path with only dot should not be raw");

        // Test shouldCreateTagLibRefForPath

        // Cases where it should return true (TagLib SHOULD be used)
        ASSERT_TRUE(track::shouldCreateTagLibRefForPath("test.mp3"), "Should create TagLib ref for .mp3");
        ASSERT_TRUE(track::shouldCreateTagLibRefForPath("test.flac"), "Should create TagLib ref for .flac");
        ASSERT_TRUE(track::shouldCreateTagLibRefForPath("test.ogg"), "Should create TagLib ref for .ogg");

        // Cases where it should return false (TagLib SHOULD NOT be used)
        ASSERT_FALSE(track::shouldCreateTagLibRefForPath(""), "Should not create TagLib ref for empty path");
        ASSERT_FALSE(track::shouldCreateTagLibRefForPath("test.pcm"), "Should not create TagLib ref for raw audio (.pcm)");
        ASSERT_FALSE(track::shouldCreateTagLibRefForPath("test.alaw"), "Should not create TagLib ref for raw audio (.alaw)");
        ASSERT_TRUE(track::shouldCreateTagLibRefForPath("test.wav"), "Should allow TagLib for .wav because RIFF can carry metadata");
    }
};

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    TestSuite suite("Track Utility Tests");
    suite.addTest(std::make_unique<TrackUtilsTest>());

    auto results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
