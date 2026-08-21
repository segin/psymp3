/*
 * test_raw_audio_tag_safety.cpp - Raw audio files should not go through TagLib
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"

using namespace TestFramework;

class RawAudioTrackTagSafetyTest : public TestCase {
public:
    RawAudioTrackTagSafetyTest() : TestCase("Raw audio tag loading skips TagLib") {}

protected:
    void runTest() override {
        const std::string path = "/tmp/psymp3-raw-tag-safety.alaw";

        {
            std::ofstream raw_file(path, std::ios::binary);
            ASSERT_TRUE(raw_file.good(), "Raw audio test file should be creatable");

            std::array<uint8_t, 8000> payload{};
            payload.fill(0x55);
            raw_file.write(reinterpret_cast<const char*>(payload.data()), payload.size());
            ASSERT_TRUE(raw_file.good(), "Raw audio payload should be writable");
        }

        track raw_track(TagLib::String(path, TagLib::String::UTF8));
        // Construction is deliberately lazy (no disk access); the behavior
        // under test lives in loadTags(), which must skip TagLib for raw
        // audio while synthesizing duration from the payload size.
        raw_track.loadTags();

        ASSERT_TRUE(raw_track.GetArtist().isEmpty(), "Raw tracks should not synthesize artist metadata");
        // The safety property is that no TagLib::FileRef is created for raw
        // audio; the title deliberately falls back to the filename stem as a
        // display placeholder (see the track constructor).
        ASSERT_EQUALS(TagLib::String("psymp3-raw-tag-safety"), raw_track.GetTitle(),
                      "Raw tracks should use the filename stem as placeholder title");
        ASSERT_TRUE(raw_track.GetAlbum().isEmpty(), "Raw tracks should not synthesize album metadata");
        ASSERT_EQUALS(1u, raw_track.GetLen(), "Raw tracks should synthesize duration without querying TagLib");

        std::remove(path.c_str());
    }
};

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    TestSuite suite("Raw Audio Tag Safety Tests");
    suite.addTest(std::make_unique<RawAudioTrackTagSafetyTest>());

    auto results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
