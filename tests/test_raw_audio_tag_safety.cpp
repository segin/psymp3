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

        ASSERT_TRUE(raw_track.GetArtist().isEmpty(), "Raw tracks should not synthesize artist metadata");
        ASSERT_TRUE(raw_track.GetTitle().isEmpty(), "Raw tracks should not synthesize title metadata");
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
