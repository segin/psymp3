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
        struct RawExtTestCase {
            const char* path;
            bool expected;
            const char* description;
        };

        std::vector<RawExtTestCase> testCases = {
            // Positive cases (known raw extensions)
            {".ulaw", true, ".ulaw should be known raw extension"},
            {".ULAW", true, ".ULAW should be case-insensitive"},
            {"test.ulaw", true, "test.ulaw should be recognized"},
            {"/path/to/file.pcm", true, "path with .pcm should be recognized"},
            {".722", true, ".722 should be recognized"},
            {".f64be", true, ".f64be should be recognized"},
            // More raw extensions from the list in track.cpp
            {".alaw", true, ".alaw should be recognized"},
            {".s16le", true, ".s16le should be recognized"},
            {".mulaw", true, ".mulaw should be recognized"},
            {".s8", true, ".s8 should be recognized"},

            // Negative cases (non-raw extensions)
            {".mp3", false, ".mp3 is not raw audio"},
            {".flac", false, ".flac is not raw audio"},
            {".wav", false, ".wav is not raw audio (it is a container)"},
            {"test.txt", false, ".txt is not raw audio"},
            {".ulaww", false, ".ulaww is not a known raw extension"},
            {"ulaw", false, "ulaw without dot is not an extension"},
            {".PCM ", false, "extension with trailing space is not recognized"},

            // Edge cases
            {"", false, "Empty path should not be raw extension"},
            {"no_extension", false, "Path without extension should not be raw"},
            {".", false, "Path with only dot should not be raw"},
            {"...", false, "Path with multiple dots but no valid extension should not be raw"},
            {"file.", false, "Path ending with dot should not be raw"}
        };

        for (const auto& tc : testCases) {
            bool actual = track::isKnownRawAudioExtension(tc.path);
            if (tc.expected) {
                ASSERT_TRUE(actual, tc.description);
            } else {
                ASSERT_FALSE(actual, tc.description);
            }
        }

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
