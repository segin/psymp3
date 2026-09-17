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
/// the count. With @p tail_padding_ns, the last block is a BlockGroup that
/// says that much of its end is padding.
std::vector<uint8_t> rampFile(uint32_t tail_padding_ns = 0)
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
        std::vector<uint8_t> stored = element(Id::SimpleBlock, block + pcm);
        if (c == 49 && tail_padding_ns != 0) {
            block[3] = 0x00;
            stored = element(Id::BlockGroup,
                             element(Id::Block, block + pcm)
                           + element(Id::DiscardPadding,
                                     {static_cast<uint8_t>(tail_padding_ns >> 24),
                                      static_cast<uint8_t>(tail_padding_ns >> 16),
                                      static_cast<uint8_t>(tail_padding_ns >> 8),
                                      static_cast<uint8_t>(tail_padding_ns)}));
        }
        clusters = clusters
                 + element(Id::Cluster, uintEl(Id::Timestamp, static_cast<uint64_t>(c) * 20)
                                      + stored);
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

/// Wraps a codec and hands back each chunk's audio one chunk late, the rest
/// at the flush -- the way a decoder that keeps output back behaves.
class LaggingCodec : public AudioCodec {
public:
    explicit LaggingCodec(std::unique_ptr<AudioCodec> inner)
        : AudioCodec(inner->getStreamInfo()), m_inner(std::move(inner))
    {
        m_initialized = true;
    }

    bool initialize() override { return true; }
    AudioFrame decode(const MediaChunk& chunk) override
    {
        AudioFrame out = std::move(m_held);
        m_held = m_inner->decode(chunk);
        return out;
    }
    AudioFrame flush() override
    {
        AudioFrame out = std::move(m_held);
        m_held = AudioFrame{};
        return out;
    }
    void reset() override { m_held = AudioFrame{}; m_inner->reset(); }
    std::string getCodecName() const override { return m_inner->getCodecName(); }
    bool canDecode(const StreamInfo& info) const override { return m_inner->canDecode(info); }

private:
    std::unique_ptr<AudioCodec> m_inner;
    AudioFrame m_held;
};

/// Reads the whole stream and returns each frame's counter value.
std::vector<int32_t> readAll(DemuxedStream& stream)
{
    std::vector<int32_t> counters;
    for (int reads = 0; reads < 1000 && !stream.eof(); ++reads) {
        const std::vector<int32_t> part = readCounters(stream, 4096);
        counters.insert(counters.end(), part.begin(), part.end());
    }
    return counters;
}

class TailPaddingTest : public TestCase {
public:
    TailPaddingTest() : TestCase("A last block's DiscardPadding is not played, however late the codec returns it") {}

protected:
    void runTest() override
    {
        constexpr int32_t kTotal = 50 * static_cast<int32_t>(kFramesPerBlock);
        constexpr int32_t kPadding = 441; // 10 ms at 44.1 kHz
        const std::vector<uint8_t> file = rampFile(10000000);

        for (bool lagging : {false, true}) {
            const std::string what = lagging ? "a lagging codec" : "PCM";
            DemuxedStream stream(std::make_unique<MemoryIOHandler>(file.data(), file.size()),
                                 TagLib::String("ramp.mka"));
            ASSERT_EQUALS(uint32_t{kRate}, static_cast<uint32_t>(stream.getRate()), what + ": opened");
            if (lagging) {
                stream.m_codec = std::make_unique<LaggingCodec>(std::move(stream.m_codec));
            }

            const std::vector<int32_t> counters = readAll(stream);
            ASSERT_EQUALS(size_t{kTotal - kPadding}, counters.size(), what + ": the padding is gone");
            // The counter is a 16-bit sample, so it wraps past 32767.
            for (size_t i = 0; i < counters.size(); ++i) {
                const auto expected = static_cast<uint16_t>(i);
                const auto got = static_cast<uint16_t>(counters[i]);
                if (got != expected) {
                    ASSERT_EQUALS(expected, got,
                                  what + ": and only the padding, at frame " + std::to_string(i));
                }
            }
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
    suite.addTest(std::make_unique<TailPaddingTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return static_cast<int>(results.size()) - suite.getPassedCount(results);
}
