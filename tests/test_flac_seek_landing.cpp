/*
 * test_flac_seek_landing.cpp - A seek in a native FLAC file plays from the
 * target
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include "io/MemoryIOHandler.h"
#include "flac_test_stream.h"

using namespace TestFramework;
using namespace PsyMP3;
using namespace PsyMP3::Demuxer;
using PsyMP3::IO::MemoryIOHandler;

namespace {

using FlacTestStream::Bytes;

/// "fLaC", STREAMINFO and @p frames frames: no SEEKTABLE, so a seek has to
/// search the file for its frame.
Bytes flacFile(uint32_t frames)
{
    Bytes file{'f', 'L', 'a', 'C'};
    const Bytes info = FlacTestStream::streamInfo(uint64_t{frames} * FlacTestStream::kBlock);
    file.insert(file.end(), info.begin(), info.end());
    for (uint32_t i = 0; i < frames; ++i) {
        const Bytes frame = FlacTestStream::frame(i);
        file.insert(file.end(), frame.begin(), frame.end());
    }
    return file;
}

/// The first channel of the next @p frames frames.
std::vector<AudioSample> readFirstChannel(DemuxedStream& stream, size_t frames)
{
    std::vector<AudioSample> buffer(frames * 2);
    size_t filled = 0;
    for (int attempt = 0; attempt < 64 && filled < buffer.size() && !stream.eof(); ++attempt) {
        filled += stream.getData((buffer.size() - filled) * sizeof(AudioSample),
                                 buffer.data() + filled) / sizeof(AudioSample);
    }
    std::vector<AudioSample> first;
    for (size_t i = 0; i + 1 < filled; i += 2) {
        first.push_back(buffer[i]);
    }
    return first;
}

class SeekLandingTest : public TestCase {
public:
    SeekLandingTest() : TestCase("A seek in a native FLAC file plays from the target") {}

protected:
    void runTest() override
    {
        const Bytes file = flacFile(40);
        DemuxedStream stream(std::make_unique<MemoryIOHandler>(file.data(), file.size()),
                             TagLib::String("counter.flac"));
        ASSERT_EQUALS(44100u, stream.getRate(), "opened");
        ASSERT_EQUALS(FlacTestStream::counterAt(0), readFirstChannel(stream, 1).front() >> 16,
                      "playback starts at the first sample");

        // The search accepts a frame within a quarter second of the target;
        // one after it used to be taken, and the audio in between skipped.
        for (unsigned long target_ms : {1000UL, 1500UL, 2345UL, 3000UL, 500UL, 1001UL}) {
            stream.seekTo(target_ms);
            const std::vector<AudioSample> after = readFirstChannel(stream, 16);
            ASSERT_EQUALS(size_t{16}, after.size(), "audio follows the seek");
            const uint64_t target = target_ms * FlacTestStream::kRate / 1000;
            ASSERT_EQUALS(FlacTestStream::counterAt(target), after.front() >> 16,
                          "a seek to " + std::to_string(target_ms) + " ms plays sample "
                          + std::to_string(target) + " first");
        }
    }
};

class DemuxerLandingTest : public TestCase {
public:
    DemuxerLandingTest() : TestCase("The FLAC demuxer's search lands at or before the target") {}

protected:
    void runTest() override
    {
        const Bytes file = flacFile(40);
        // A fresh demuxer each time, so every search starts without a frame
        // index to fall back on.
        for (unsigned long target_ms : {1000UL, 1500UL, 2345UL, 3000UL, 500UL}) {
            PsyMP3::Demuxer::FLAC::FLACDemuxer fresh(
                std::make_unique<MemoryIOHandler>(file.data(), file.size()));
            ASSERT_TRUE(fresh.parseContainer(), "parsed");
            ASSERT_TRUE(fresh.seekTo(target_ms), "the seek succeeds");
            const uint64_t target = target_ms * FlacTestStream::kRate / 1000;
            const uint64_t landing = fresh.getGranulePosition(1);
            ASSERT_TRUE(landing <= target && target - landing < FlacTestStream::kRate / 4 + FlacTestStream::kBlock,
                        "a seek to " + std::to_string(target_ms) + " ms lands at " + std::to_string(landing)
                        + ", at or not long before sample " + std::to_string(target));
        }
    }
};

} // namespace

int main()
{
    registerAllCodecs();
    registerAllDemuxers();

    TestSuite suite("FLAC seek landing");
    suite.addTest(std::make_unique<SeekLandingTest>());
    suite.addTest(std::make_unique<DemuxerLandingTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return static_cast<int>(results.size()) - suite.getPassedCount(results);
}
