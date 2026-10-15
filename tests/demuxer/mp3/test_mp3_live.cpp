/*
 * test_mp3_live.cpp - MP3NullDemuxer on internet radio
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"

using PsyMP3::Demuxer::MP3::MP3NullDemuxer;
using PsyMP3::IO::MemoryIOHandler;

namespace {

/** @brief A memory handler that calls itself live, as a station is. */
class LiveMp3Handler : public MemoryIOHandler {
public:
    /** @brief Copy @p data in. */
    explicit LiveMp3Handler(const std::string& data) : MemoryIOHandler(data.data(), data.size()) {}

    /**
     * @brief Report a live stream.
     * @return true.
     */
    bool isLiveStream() const override { return true; }
};

/**
 * @brief A station joined in the middle of a frame whose bytes happen to
 *        hold a Layer II sync word. That lone header used to pick the codec
 *        (mp2) for the whole session, which then refused every real Layer
 *        III frame: silence. A live stream's first header now counts only
 *        when the next frame confirms it.
 */
class FalseSyncTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    FalseSyncTest() : TestCase("Live MP3: a false sync in the joined frame does not pick the codec") {}
protected:
    /** @brief A Layer II header in junk, then Layer III frames. */
    void runTest() override {
        // MPEG-1 Layer II, 160 kbps, 44.1 kHz: valid, and 522 bytes "long".
        std::string data = std::string("\xFF\xFD\x90\x00", 4) + std::string(96, '\0');
        // MPEG-1 Layer III, 128 kbps, 44.1 kHz, 417 bytes each.
        const std::string frame = std::string("\xFF\xFB\x90\x64", 4) + std::string(413, '\0');
        for (int i = 0; i < 6; ++i) data += frame;
        MP3NullDemuxer demuxer(std::make_unique<LiveMp3Handler>(data));
        ASSERT_TRUE(demuxer.parseContainer(), "parses");
        const auto streams = demuxer.getStreams();
        ASSERT_EQUALS(size_t(1), streams.size(), "one stream");
        ASSERT_EQUALS(std::string("mp3"), streams[0].codec_name, "Layer III, as the real frames are");
    }
};

} // namespace

/**
 * @brief Run the live MP3 tests; the entry test_demuxer_suite calls.
 * @return Number of failed tests.
 */
int test_mp3_live_main()
{
    TestFramework::TestSuite suite("MP3 Live Stream Tests");
    suite.addTest(std::make_unique<FalseSyncTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
