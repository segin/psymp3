/*
 * test_demuxed_stream_seek.cpp - DemuxedStream behaviour when a seek is refused
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

// Standard headers first, so the access override below reaches only
// PsyMP3's own classes.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

// DemuxedStream owns its demuxer privately; this test swaps it for one that
// refuses seeks.
#define private public
#include "psymp3.h"
#undef private

#include "test_framework.h"
#include "io/MemoryIOHandler.h"
#include "matroska_ebml_builder.h"

using namespace TestFramework;
using namespace PsyMP3;
using namespace PsyMP3::Demuxer;
using namespace PsyMP3::Demuxer::Matroska;
using namespace MatroskaBuilder;
using PsyMP3::IO::MemoryIOHandler;

namespace {

constexpr uint32_t kRate = 44100;
constexpr size_t kFramesPerBlock = 882; // 20 ms

/// One second of 16-bit stereo PCM in fifty clusters. Both channels hold a
/// running frame counter, so any lost or repeated audio shows as a break in
/// the count.
std::vector<uint8_t> rampFile()
{
    const std::vector<uint8_t> track =
        element(Id::TrackEntry, uintEl(Id::TrackNumber, 1)
                              + uintEl(Id::TrackType, TrackType::Audio)
                              + strEl(Id::CodecID, "A_PCM/INT/LIT")
                              + element(Id::Audio, floatEl(Id::SamplingFrequency, kRate)
                                                 + uintEl(Id::Channels, 2)
                                                 + uintEl(Id::BitDepth, 16)));
    std::vector<uint8_t> clusters;
    uint16_t counter = 0;
    for (int c = 0; c < 50; ++c) {
        std::vector<uint8_t> pcm;
        for (size_t f = 0; f < kFramesPerBlock; ++f, ++counter) {
            for (int ch = 0; ch < 2; ++ch) {
                pcm.push_back(static_cast<uint8_t>(counter & 0xFF));
                pcm.push_back(static_cast<uint8_t>(counter >> 8));
            }
        }
        std::vector<uint8_t> block{0x81, 0x00, 0x00, 0x80};
        clusters = clusters
                 + element(Id::Cluster, uintEl(Id::Timestamp, static_cast<uint64_t>(c) * 20)
                                      + element(Id::SimpleBlock, block + pcm));
    }
    return ebmlHeader("matroska")
         + element(Id::Segment,
                   element(Id::Info, uintEl(Id::TimestampScale, 1000000)
                                   + floatEl(Id::Duration, 1000.0))
                 + element(Id::Tracks, track) + clusters);
}

/// Passes everything through to the real demuxer except seeking, which it
/// refuses the way a demuxer without an index does.
class RefusingDemuxer : public PsyMP3::Demuxer::Demuxer {
    // The base class insists on a working handler; the wrapper never reads it.
    static constexpr uint8_t kPlaceholder[1] = {0};

public:
    explicit RefusingDemuxer(std::unique_ptr<PsyMP3::Demuxer::Demuxer> inner)
        : PsyMP3::Demuxer::Demuxer(std::make_unique<MemoryIOHandler>(kPlaceholder, sizeof(kPlaceholder)))
        , m_inner(std::move(inner)) {}

    bool parseContainer() override { return m_inner->parseContainer(); }
    std::vector<StreamInfo> getStreams() const override { return m_inner->getStreams(); }
    StreamInfo getStreamInfo(uint32_t id) const override { return m_inner->getStreamInfo(id); }
    MediaChunk readChunk() override { return m_inner->readChunk(); }
    MediaChunk readChunk(uint32_t id) override { return m_inner->readChunk(id); }
    bool seekTo(uint64_t) override { ++refused; return false; }
    bool isEOF() const override { return m_inner->isEOF(); }
    uint64_t getDuration() const override { return m_inner->getDuration(); }
    uint64_t getPosition() const override { return m_inner->getPosition(); }
    bool providesGranulePositions() const override { return m_inner->providesGranulePositions(); }
    uint64_t getGranulePosition(uint32_t id) const override { return m_inner->getGranulePosition(id); }

    int refused = 0;

private:
    std::unique_ptr<PsyMP3::Demuxer::Demuxer> m_inner;
};

/// Reads @p frames stereo frames and returns each frame's counter value.
std::vector<int32_t> readCounters(DemuxedStream& stream, size_t frames)
{
    std::vector<AudioSample> buffer(frames * 2);
    const size_t bytes = stream.getData(buffer.size() * sizeof(AudioSample), buffer.data());
    std::vector<int32_t> counters;
    for (size_t i = 0; i + 1 < bytes / sizeof(AudioSample); i += 2) {
        counters.push_back(buffer[i] >> 16);
    }
    return counters;
}

class RefusedSeekKeepsAudioTest : public TestCase {
public:
    RefusedSeekKeepsAudioTest() : TestCase("A refused seek leaves playback exactly where it was") {}

protected:
    void runTest() override
    {
        const std::vector<uint8_t> file = rampFile();
        DemuxedStream stream(std::make_unique<MemoryIOHandler>(file.data(), file.size()),
                             TagLib::String("ramp.mka"));
        ASSERT_EQUALS(uint32_t{kRate}, static_cast<uint32_t>(stream.getRate()), "the fixture opened");

        const std::vector<int32_t> before = readCounters(stream, 1000);
        ASSERT_EQUALS(size_t{1000}, before.size(), "a first read");
        ASSERT_EQUALS(int32_t{999}, before.back(), "the counter runs from 0");

        auto refusing = std::make_unique<RefusingDemuxer>(std::move(stream.m_demuxer));
        RefusingDemuxer* probe = refusing.get();
        stream.m_demuxer = std::move(refusing);

        stream.seekTo(500);
        ASSERT_EQUALS(1, probe->refused, "the seek reached the demuxer and was refused");

        // Discarding the buffered chunks before asking the demuxer made this
        // jump ahead by everything the stream had read in advance.
        const std::vector<int32_t> after = readCounters(stream, 1000);
        ASSERT_EQUALS(size_t{1000}, after.size(), "reading continues");
        ASSERT_EQUALS(int32_t{1000}, after.front(),
                      "the next frame is the one after the last frame read");
        for (size_t i = 1; i < after.size(); ++i) {
            ASSERT_EQUALS(after[i - 1] + 1, after[i], "and the count is unbroken");
        }
    }
};

} // namespace

int main()
{
    registerAllCodecs();
    registerAllDemuxers();

    TestSuite suite("DemuxedStream Seek Tests");
    suite.addTest(std::make_unique<RefusedSeekKeepsAudioTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return static_cast<int>(results.size()) - suite.getPassedCount(results);
}
