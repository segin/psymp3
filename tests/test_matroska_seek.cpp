/*
 * test_matroska_seek.cpp - Matroska seek landing and granule reporting
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include "io/MemoryIOHandler.h"
#include "matroska_ebml_builder.h"

using namespace TestFramework;
using namespace PsyMP3::Demuxer::Matroska;
using namespace MatroskaBuilder;
using PsyMP3::IO::MemoryIOHandler;

namespace {

constexpr uint32_t kSampleRate   = 44100;
constexpr uint16_t kChannels     = 2;
constexpr uint16_t kBitDepth     = 16;
constexpr uint64_t kBlockMs      = 20;   // one cluster per 20 ms
constexpr int      kClusterCount = 50;   // one second in all

/// Bytes of PCM in one kBlockMs block. A_PCM carries no internal framing, so a
/// reader derives the frame count from the payload size and the audio
/// parameters -- a token payload would be a block claiming 20 ms while holding
/// four samples, which no muxer would emit and which would give any
/// decode-side test the wrong duration.
constexpr size_t kFrameBytes =
    static_cast<size_t>(kSampleRate) * kBlockMs / 1000 * kChannels * (kBitDepth / 8);

/// Samples at kSampleRate for a whole number of milliseconds, the same
/// arithmetic the demuxer stamps its chunks with.
constexpr uint64_t samplesAt(uint64_t milliseconds)
{
    return (milliseconds * kSampleRate) / 1000;
}

/// A SimpleBlock holding one unlaced frame for a track.
std::vector<uint8_t> simpleBlock(uint64_t track, int16_t relative_ticks,
                                 const std::vector<uint8_t>& frame)
{
    const auto raw = static_cast<uint16_t>(relative_ticks);
    std::vector<uint8_t> body{static_cast<uint8_t>(0x80 | track),
                              static_cast<uint8_t>(raw >> 8),
                              static_cast<uint8_t>(raw & 0xFF),
                              0x80}; // keyframe, no lacing
    return element(Id::SimpleBlock, body + frame);
}

std::vector<uint8_t> pcmTrack(uint64_t number)
{
    return element(Id::TrackEntry,
                   uintEl(Id::TrackNumber, number)
                 + uintEl(Id::TrackUID, 0x1000 + number)
                 + uintEl(Id::TrackType, TrackType::Audio)
                 + strEl(Id::CodecID, "A_PCM/INT/LIT")
                 + strEl(Id::Language, "und")
                 + element(Id::Audio, floatEl(Id::SamplingFrequency, kSampleRate)
                                    + uintEl(Id::Channels, kChannels)
                                    + uintEl(Id::BitDepth, kBitDepth)));
}

/// One second of PCM as fifty 20 ms clusters, with no Cues.
///
/// Without Cues the index is built by walking the cluster headers, which is the
/// fallback a great many real files take. Cues and the scan produce the same
/// CueEntry, so the landing this pins down is the landing either path yields.
/// Each block carries a full kFrameBytes of PCM, so the fixture really holds
/// the duration its timestamps claim.
std::vector<uint8_t> buildFile()
{
    const std::vector<uint8_t> info =
        element(Id::Info, uintEl(Id::TimestampScale, 1000000)
                        + floatEl(Id::Duration, static_cast<double>(kClusterCount * kBlockMs)));
    const std::vector<uint8_t> tracks = element(Id::Tracks, pcmTrack(1));

    std::vector<uint8_t> clusters;
    for (int i = 0; i < kClusterCount; ++i) {
        clusters = clusters
                 + element(Id::Cluster,
                           uintEl(Id::Timestamp, static_cast<uint64_t>(i) * kBlockMs)
                         + simpleBlock(1, 0, std::vector<uint8_t>(kFrameBytes, 0x5A)));
    }

    return ebmlHeader("matroska") + element(Id::Segment, info + tracks + clusters);
}

/// An opened demuxer over an in-memory copy of the fixture.
std::unique_ptr<MatroskaDemuxer> openFixture()
{
    const std::vector<uint8_t> file = buildFile();
    // copy=true: the handler owns the bytes, so the vector above may die here.
    auto handler = std::make_unique<MemoryIOHandler>(file.data(), file.size());
    auto demuxer = std::make_unique<MatroskaDemuxer>(std::move(handler));
    if (!demuxer->parseContainer()) {
        return nullptr;
    }
    return demuxer;
}

class ReportsGranulePositionsTest : public TestCase {
public:
    ReportsGranulePositionsTest()
        : TestCase("Granule positions are reported once the sample rate is known") {}

protected:
    void runTest() override
    {
        auto demuxer = openFixture();
        ASSERT_TRUE(demuxer != nullptr, "fixture should parse");

        // The granule is counted in samples, so the rate has to have survived
        // parsing for any of the rest to mean anything. Asserting it here turns
        // a broken fixture into an obvious failure rather than a granule of 0
        // that looks like the fix regressing.
        const auto streams = demuxer->getStreams();
        ASSERT_EQUALS(static_cast<size_t>(1), streams.size(), "one audio track");
        ASSERT_EQUALS(kSampleRate, streams.front().sample_rate,
                      "Audio/SamplingFrequency reached StreamInfo");

        // This is the whole fix. DemuxedStream::seekTo gates its discard
        // counter on this flag: false leaves the counter at zero and nothing
        // between the cluster boundary and the target is ever dropped.
        ASSERT_TRUE(demuxer->providesGranulePositions(),
                    "a demuxer that seeks to a boundary must report where it landed");
        ASSERT_EQUALS(static_cast<uint64_t>(0), demuxer->getGranulePosition(1),
                      "before anything is read the stream sits at its head");
    }
};

class SeekReportsLandingTest : public TestCase {
public:
    SeekReportsLandingTest()
        : TestCase("A seek between clusters reports the cluster it landed on") {}

protected:
    void runTest() override
    {
        auto demuxer = openFixture();
        ASSERT_TRUE(demuxer != nullptr, "fixture should parse");

        // 530 ms falls inside the cluster that starts at 520 ms.
        ASSERT_TRUE(demuxer->seekTo(530), "seek should succeed");

        const uint64_t landing = samplesAt(520);
        const uint64_t target  = samplesAt(530);

        ASSERT_EQUALS(landing, demuxer->getGranulePosition(1),
                      "the granule is the landing cluster, in samples");
        ASSERT_TRUE(demuxer->getGranulePosition(1) < target,
                    "the landing precedes the target; the gap is what gets trimmed");

        // The first frame handed out has to agree with the granule, because
        // the stream compares one against the other to decide what to drop.
        auto chunk = demuxer->readChunk();
        ASSERT_TRUE(chunk.isValid(), "a chunk should follow the seek");
        ASSERT_EQUALS(landing, chunk.timestamp_samples,
                      "the first frame after a seek starts at the landing");
    }
};

class SeekOnBoundaryTest : public TestCase {
public:
    SeekOnBoundaryTest()
        : TestCase("A seek that falls on a cluster boundary needs no trimming") {}

protected:
    void runTest() override
    {
        auto demuxer = openFixture();
        ASSERT_TRUE(demuxer != nullptr, "fixture should parse");

        ASSERT_TRUE(demuxer->seekTo(540), "seek should succeed");
        ASSERT_EQUALS(samplesAt(540), demuxer->getGranulePosition(1),
                      "landing on a boundary lands exactly, with nothing to drop");
    }
};

class GranuleTracksPlaybackTest : public TestCase {
public:
    GranuleTracksPlaybackTest()
        : TestCase("The granule follows playback, not just seeks") {}

protected:
    void runTest() override
    {
        auto demuxer = openFixture();
        ASSERT_TRUE(demuxer != nullptr, "fixture should parse");

        for (uint64_t i = 0; i < 4; ++i) {
            auto chunk = demuxer->readChunk();
            ASSERT_TRUE(chunk.isValid(), "should read a chunk");
            ASSERT_EQUALS(samplesAt(i * kBlockMs), demuxer->getGranulePosition(1),
                          "the granule is the last chunk handed out");
        }
    }
};

class SeekBackwardsTest : public TestCase {
public:
    SeekBackwardsTest()
        : TestCase("Seeking backwards moves the granule back") {}

protected:
    void runTest() override
    {
        auto demuxer = openFixture();
        ASSERT_TRUE(demuxer != nullptr, "fixture should parse");

        ASSERT_TRUE(demuxer->seekTo(800), "forward seek should succeed");
        ASSERT_EQUALS(samplesAt(800), demuxer->getGranulePosition(1), "lands at 800 ms");

        // A stale granule here would put the stream's discard target behind its
        // own position and silently swallow the rest of the track.
        ASSERT_TRUE(demuxer->seekTo(100), "backward seek should succeed");
        ASSERT_EQUALS(samplesAt(100), demuxer->getGranulePosition(1), "lands back at 100 ms");
    }
};

// ---------------------------------------------------------------------------
// A *cued* fixture, for the path the fixture above cannot reach.
//
// buildFile() writes no Cues, so every test above runs through
// CueIndex::buildByScanning, which records each Cluster's own Timestamp and is
// therefore self-consistent by construction. The Cues path is not: a CuePoint
// pairs CueTime -- the timestamp of a Block (RFC 9559 §5.1.5.1.1) -- with the
// position of the Cluster holding it (§5.1.5.1.2.2), and the block need not be
// the cluster's first. So CueTime can be strictly greater than the cluster's
// Timestamp, while reading restarts at the cluster head.
// ---------------------------------------------------------------------------

constexpr uint64_t kClusterMs       = 100;  // five 20 ms blocks per cluster
constexpr int      kCuedClusters    = 12;
constexpr uint64_t kCueOffsetMs     = 40;   // cues name the *third* block
constexpr int      kCueEveryN       = 4;    // sparse: only every 4th cluster

/// One cluster holding five blocks at relative 0/20/40/60/80 ticks.
std::vector<uint8_t> multiBlockCluster(uint64_t timestamp_ms)
{
    std::vector<uint8_t> body = uintEl(Id::Timestamp, timestamp_ms);
    for (int b = 0; b < 5; ++b) {
        body = body + simpleBlock(1, static_cast<int16_t>(b * 20),
                                  std::vector<uint8_t>(kFrameBytes, 0x5A));
    }
    return element(Id::Cluster, body);
}

/// A SeekHead whose positions are relative to the Segment's data start. Its own
/// size feeds back into the positions it stores, so it is measured to a fixed
/// point, exactly as the corpus generator does it.
std::vector<uint8_t> seekHeadFor(const std::vector<std::pair<uint32_t, uint64_t>>& entries)
{
    auto build = [&](uint64_t own_size) {
        std::vector<uint8_t> body;
        for (const auto& entry : entries) {
            body = body + element(Id::Seek,
                                  element(Id::SeekID, idBytes(entry.first))
                                + uintEl(Id::SeekPosition, own_size + entry.second));
        }
        return element(Id::SeekHead, body);
    };
    uint64_t own_size = 0;
    for (int attempt = 0; attempt < 8; ++attempt) {
        const uint64_t measured = build(own_size).size();
        if (measured == own_size) {
            break;
        }
        own_size = measured;
    }
    return build(own_size);
}

/// Cues for every kCueEveryN-th cluster only, each naming a block kCueOffsetMs
/// into its cluster. Sparse on purpose: if these cues were ever ignored and the
/// scan fallback ran instead, its entries would land on a different cluster and
/// the assertions below would fail rather than quietly still passing.
std::vector<uint8_t> sparseCues(const std::vector<std::vector<uint8_t>>& cluster_bytes,
                                uint64_t track, uint64_t first_cluster_relative,
                                size_t first_cued = 0)
{
    std::vector<uint8_t> points;
    uint64_t position = first_cluster_relative;
    for (size_t i = 0; i < cluster_bytes.size(); ++i) {
        if (i % kCueEveryN == 0 && i >= first_cued) {
            points = points + element(Id::CuePoint,
                          uintEl(Id::CueTime, i * kClusterMs + kCueOffsetMs)
                        + element(Id::CueTrackPositions,
                                  uintEl(Id::CueTrack, track)
                                + uintEl(Id::CueClusterPosition, position)));
        }
        position += cluster_bytes[i].size();
    }
    return element(Id::Cues, points);
}

/// Cues carrying a CueTime whose payload is nine octets wide.
///
/// EBMLReader::readUInt refuses an integer wider than eight octets by throwing,
/// and kMaxSizeLength bounds the VINT's *width*, not its value, so a single
/// size byte legally announces nine. This is the cheapest way to make the index
/// build throw over a file whose clusters are perfectly readable.
std::vector<uint8_t> damagedCues(uint64_t first_cluster_relative)
{
    return element(Id::Cues,
                   element(Id::CuePoint,
                           element(Id::CueTime, std::vector<uint8_t>(9, 0x00))
                         + element(Id::CueTrackPositions,
                                   uintEl(Id::CueTrack, 1)
                                 + uintEl(Id::CueClusterPosition, first_cluster_relative))));
}

std::vector<uint8_t> buildCuedFile(bool damaged_cues = false, size_t first_cued = 0)
{
    const std::vector<uint8_t> info =
        element(Id::Info, uintEl(Id::TimestampScale, 1000000)
                        + floatEl(Id::Duration,
                                  static_cast<double>(kCuedClusters * kClusterMs)));
    const std::vector<uint8_t> tracks = element(Id::Tracks, pcmTrack(1));

    std::vector<std::vector<uint8_t>> cluster_bytes;
    for (int i = 0; i < kCuedClusters; ++i) {
        cluster_bytes.push_back(multiBlockCluster(static_cast<uint64_t>(i) * kClusterMs));
    }
    std::vector<uint8_t> clusters_flat;
    for (const auto& c : cluster_bytes) {
        clusters_flat = clusters_flat + c;
    }

    // Cues sit after the clusters, as real muxers write them, so only the
    // SeekHead can find them.
    const std::vector<uint8_t> head =
        seekHeadFor({{Id::Info,   0},
                     {Id::Tracks, info.size()},
                     {Id::Cues,   info.size() + tracks.size() + clusters_flat.size()}});
    const uint64_t first_cluster_at = head.size() + info.size() + tracks.size();
    const std::vector<uint8_t> cues =
        damaged_cues ? damagedCues(first_cluster_at)
                     : sparseCues(cluster_bytes, 1, first_cluster_at, first_cued);

    return ebmlHeader("matroska")
         + element(Id::Segment, head + info + tracks + clusters_flat + cues);
}

std::unique_ptr<MatroskaDemuxer> openCuedFixture()
{
    const std::vector<uint8_t> file = buildCuedFile();
    auto handler = std::make_unique<MemoryIOHandler>(file.data(), file.size());
    auto demuxer = std::make_unique<MatroskaDemuxer>(std::move(handler));
    if (!demuxer->parseContainer()) {
        return nullptr;
    }
    return demuxer;
}

class SeekLandsOnClusterHeadNotCueTimeTest : public TestCase {
public:
    SeekLandsOnClusterHeadNotCueTimeTest()
        : TestCase("A cued seek reports the cluster head, not CueTime") {}

protected:
    void runTest() override
    {
        auto demuxer = openCuedFixture();
        ASSERT_TRUE(demuxer != nullptr, "cued fixture should parse");

        // Cues exist at CueTime 40, 440 and 840 (clusters 0, 4, 8). A seek to
        // 250 ms takes the cue at 40, whose cluster starts at 0 ms.
        //
        // Three outcomes separate here, which is the point of the sparse cues:
        //   samplesAt(0)   - the cue was used and the landing is the cluster head
        //   samplesAt(40)  - the cue was used but CueTime was reported (the bug)
        //   samplesAt(200) - the cues were ignored and the scan fallback ran
        ASSERT_TRUE(demuxer->seekTo(250), "seek should succeed");
        ASSERT_EQUALS(samplesAt(0), demuxer->getGranulePosition(1),
                      "landing is the cluster's own Timestamp, not the cued block's");

        auto chunk = demuxer->readChunk();
        ASSERT_TRUE(chunk.isValid(), "a chunk should follow the seek");
        ASSERT_EQUALS(samplesAt(0), chunk.timestamp_samples,
                      "the first frame really is the one the landing names");

        // Again further in, so a fixture that happened to land on zero proves
        // nothing: cues 40/440 are <= 500, so the cue at 440 wins and its
        // cluster starts at 400 ms.
        ASSERT_TRUE(demuxer->seekTo(500), "second seek should succeed");
        ASSERT_EQUALS(samplesAt(400), demuxer->getGranulePosition(1),
                      "cluster head again, four clusters along");
    }
};

class DamagedCuesCostSeekingNotPlaybackTest : public TestCase {
public:
    DamagedCuesCostSeekingNotPlaybackTest()
        : TestCase("A damaged Cues element costs seeking, not playback") {}

protected:
    void runTest() override
    {
        // Cues are only SHOULD-be-present (RFC 9559 5.1.5) and every Cluster in
        // this file is intact, so a CueTime the reader refuses is a reason to
        // lose the index -- not the track. The index build used to sit outside
        // parseContainer's try, so this exception escaped and DemuxedStream
        // refused a file whose audio reads perfectly.
        const std::vector<uint8_t> file = buildCuedFile(/*damaged_cues=*/true);
        auto handler = std::make_unique<MemoryIOHandler>(file.data(), file.size());
        MatroskaDemuxer demuxer(std::move(handler));

        // Catch the throw here rather than letting it escape runTest(). The
        // harness records an escaping exception as ERROR, and
        // TestSuite::getFailureCount counts only FAILED, so main()'s exit code
        // would stay 0 and this regression would sail through make check while
        // printing a failure nobody's build acts on.
        bool opened = false;
        try {
            opened = demuxer.parseContainer();
        } catch (const std::exception&) {
            opened = false;
        }
        ASSERT_TRUE(opened,
                    "a Cues element that throws must not refuse the whole file");

        auto chunk = demuxer.readChunk();
        ASSERT_TRUE(chunk.isValid(), "the clusters are intact, so playback continues");
        ASSERT_EQUALS(samplesAt(0), chunk.timestamp_samples, "starting at the first cluster");

        // What degrades: with no index a seek restarts at the first cluster and
        // the stream decodes forward to the target. Refusing instead left
        // DemuxedStream carrying on from wherever it was.
        ASSERT_TRUE(demuxer.seekTo(250), "an unindexed seek restarts from the first cluster");
        ASSERT_EQUALS(samplesAt(0), demuxer.getGranulePosition(1),
                      "and reports the head of the file as its landing");
        chunk = demuxer.readChunk();
        ASSERT_TRUE(chunk.isValid(), "reading continues from the first cluster");
        ASSERT_EQUALS(samplesAt(0), chunk.timestamp_samples, "with the first frame");
    }
};

class SeekBeforeFirstCueTest : public TestCase {
public:
    SeekBeforeFirstCueTest()
        : TestCase("A target before the first cue lands at the head, not at that cue") {}

protected:
    void runTest() override
    {
        // Cues only from cluster 4 (400 ms) on. A seek to 150 ms has no cue at or
        // before it; taking the first cue anyway landed at 400 ms and skipped
        // 250 ms of audio. Restarting at the first cluster lands before it.
        const std::vector<uint8_t> file = buildCuedFile(false, 4);
        auto handler = std::make_unique<MemoryIOHandler>(file.data(), file.size());
        MatroskaDemuxer demuxer(std::move(handler));
        ASSERT_TRUE(demuxer.parseContainer(), "fixture should parse");
        ASSERT_TRUE(demuxer.seekTo(150), "seek should succeed");
        ASSERT_EQUALS(samplesAt(0), demuxer.getGranulePosition(1),
                      "landing is the head of the file, before the target");
        ASSERT_TRUE(demuxer.seekTo(450), "a cued target still uses its cue");
        ASSERT_EQUALS(samplesAt(400), demuxer.getGranulePosition(1), "cluster 4's head");
    }
};

// ---------------------------------------------------------------------------
// A cluster's first audio block need not sit at the cluster's Timestamp. In a
// file with video, the audio of a cluster often starts some way in, behind a
// video frame; its time is the Timestamp plus the block's own signed offset.
// ---------------------------------------------------------------------------

constexpr int16_t kAudioOffsetMs = 30;

std::vector<uint8_t> buildOffsetFile()
{
    const std::vector<uint8_t> info =
        element(Id::Info, uintEl(Id::TimestampScale, 1000000)
                        + floatEl(Id::Duration, 1000.0));
    const std::vector<uint8_t> video =
        element(Id::TrackEntry, uintEl(Id::TrackNumber, 2)
                              + uintEl(Id::TrackUID, 0x2002)
                              + uintEl(Id::TrackType, TrackType::Video)
                              + strEl(Id::CodecID, "V_UNCOMPRESSED"));
    const std::vector<uint8_t> tracks = element(Id::Tracks, pcmTrack(1) + video);

    std::vector<uint8_t> clusters;
    for (int i = 0; i < 10; ++i) {
        clusters = clusters
                 + element(Id::Cluster,
                           uintEl(Id::Timestamp, static_cast<uint64_t>(i) * 100)
                         + simpleBlock(2, 0, std::vector<uint8_t>(16, 0x11))
                         + simpleBlock(1, kAudioOffsetMs, std::vector<uint8_t>(kFrameBytes, 0x5A)));
    }
    return ebmlHeader("matroska") + element(Id::Segment, info + tracks + clusters);
}

class SeekLandsOnFirstAudioBlockTest : public TestCase {
public:
    SeekLandsOnFirstAudioBlockTest()
        : TestCase("A seek reports the first audio block's time, offset included") {}

protected:
    void runTest() override
    {
        const std::vector<uint8_t> file = buildOffsetFile();
        auto handler = std::make_unique<MemoryIOHandler>(file.data(), file.size());
        MatroskaDemuxer demuxer(std::move(handler));
        ASSERT_TRUE(demuxer.parseContainer(), "fixture should parse");

        // Cluster 3 starts at 300 ms, and its audio 30 ms later. Reporting the
        // cluster's own Timestamp labelled every frame after the seek 30 ms
        // early, for the rest of the track.
        ASSERT_TRUE(demuxer.seekTo(350), "seek should succeed");
        ASSERT_EQUALS(samplesAt(300 + kAudioOffsetMs), demuxer.getGranulePosition(1),
                      "landing is the audio block's time, not the cluster's");
        auto chunk = demuxer.readChunk();
        ASSERT_TRUE(chunk.isValid(), "the audio frame follows");
        ASSERT_EQUALS(samplesAt(300 + kAudioOffsetMs), chunk.timestamp_samples,
                      "and it is the frame the landing names");
    }
};

class HeaderStrippedFramesTest : public TestCase {
public:
    HeaderStrippedFramesTest()
        : TestCase("Frames of a header-stripped track come out with their header back") {}

protected:
    void runTest() override
    {
        const std::vector<uint8_t> prefix{0xDE, 0xAD};
        const std::vector<uint8_t> track =
            element(Id::TrackEntry,
                    uintEl(Id::TrackNumber, 1)
                  + uintEl(Id::TrackType, TrackType::Audio)
                  + strEl(Id::CodecID, "A_PCM/INT/LIT")
                  + element(Id::ContentEncodings,
                            element(Id::ContentEncoding,
                                    element(Id::ContentCompression,
                                            uintEl(Id::ContentCompAlgo, 3)
                                          + element(Id::ContentCompSettings, prefix))))
                  + element(Id::Audio, floatEl(Id::SamplingFrequency, kSampleRate)
                                     + uintEl(Id::Channels, kChannels)
                                     + uintEl(Id::BitDepth, kBitDepth)));
        const std::vector<uint8_t> stored(kFrameBytes - prefix.size(), 0x5A);
        const std::vector<uint8_t> file =
            ebmlHeader("matroska")
          + element(Id::Segment,
                    element(Id::Info, uintEl(Id::TimestampScale, 1000000))
                  + element(Id::Tracks, track)
                  + element(Id::Cluster, uintEl(Id::Timestamp, 0)
                                       + simpleBlock(1, 0, stored)));
        auto handler = std::make_unique<MemoryIOHandler>(file.data(), file.size());
        MatroskaDemuxer demuxer(std::move(handler));
        ASSERT_TRUE(demuxer.parseContainer(), "fixture should parse");

        auto chunk = demuxer.readChunk();
        ASSERT_TRUE(chunk.isValid(), "the frame is read");
        ASSERT_EQUALS(kFrameBytes, chunk.data.size(), "stored bytes plus the stripped header");
        ASSERT_TRUE(chunk.data[0] == 0xDE && chunk.data[1] == 0xAD,
                    "the stripped header comes first");
        ASSERT_TRUE(chunk.data[2] == 0x5A, "followed by the stored frame");
    }
};


/// A file whose middle is a run of zero bytes that nothing stores. It places a
/// block beyond the reader's allocation ceiling without holding 64 MiB.
class GapIOHandler : public PsyMP3::IO::IOHandler {
public:
    GapIOHandler(std::vector<uint8_t> head, uint64_t gap, std::vector<uint8_t> tail)
        : m_head(std::move(head)), m_gap(gap), m_tail(std::move(tail)) {}

    size_t read(void* buffer, size_t size, size_t count) override
    {
        if (size == 0 || count == 0) {
            return 0;
        }
        const uint64_t total = m_head.size() + m_gap + m_tail.size();
        const uint64_t left = total - std::min(total, m_pos);
        const uint64_t wanted = std::min<uint64_t>(left, static_cast<uint64_t>(size) * count);
        auto* out = static_cast<uint8_t*>(buffer);
        for (uint64_t done = 0; done < wanted;) {
            const uint64_t at = m_pos + done;
            uint64_t span = wanted - done;
            if (at < m_head.size()) {
                span = std::min<uint64_t>(span, m_head.size() - at);
                std::memcpy(out + done, m_head.data() + at, span);
            } else if (at < m_head.size() + m_gap) {
                span = std::min<uint64_t>(span, m_head.size() + m_gap - at);
                std::memset(out + done, 0, span);
            } else {
                std::memcpy(out + done, m_tail.data() + (at - m_head.size() - m_gap), span);
            }
            done += span;
        }
        m_pos += wanted;
        return static_cast<size_t>(wanted / size);
    }

    int seek(PsyMP3::IO::filesize_t offset, int whence) override
    {
        const int64_t base = whence == SEEK_CUR ? static_cast<int64_t>(m_pos)
                           : whence == SEEK_END ? static_cast<int64_t>(getFileSize())
                           : 0;
        if (base + offset < 0) {
            return -1;
        }
        m_pos = static_cast<uint64_t>(base + offset);
        return 0;
    }

    PsyMP3::IO::filesize_t tell() override { return static_cast<PsyMP3::IO::filesize_t>(m_pos); }
    int close() override { return 0; }
    bool eof() override { return m_pos >= static_cast<uint64_t>(getFileSize()); }
    PsyMP3::IO::filesize_t getFileSize() override
    {
        return static_cast<PsyMP3::IO::filesize_t>(m_head.size() + m_gap + m_tail.size());
    }

private:
    std::vector<uint8_t> m_head;
    uint64_t m_gap;
    std::vector<uint8_t> m_tail;
    uint64_t m_pos = 0;
};

class OversizedVideoBlockTest : public TestCase {
public:
    OversizedVideoBlockTest()
        : TestCase("A video block too large to read does not end audio playback") {}

protected:
    void runTest() override
    {
        const std::vector<uint8_t> info = element(Id::Info, uintEl(Id::TimestampScale, 1000000));
        const std::vector<uint8_t> video =
            element(Id::TrackEntry, uintEl(Id::TrackNumber, 2)
                                  + uintEl(Id::TrackType, TrackType::Video)
                                  + strEl(Id::CodecID, "V_UNCOMPRESSED"));
        const std::vector<uint8_t> tracks = element(Id::Tracks, pcmTrack(1) + video);
        const std::vector<uint8_t> frame(kFrameBytes, 0x5A);

        // Audio at 0 ms, a video frame one byte over the ceiling, audio at
        // 20 ms. The video block is written by hand: only its first bytes
        // (track 2, time 0, keyframe) are stored, and the gap supplies the rest.
        const uint64_t video_size = EBMLReader::kMaxBinarySize + 1;
        const std::vector<uint8_t> video_lead{0x82, 0x00, 0x00, 0x80};
        const std::vector<uint8_t> before = uintEl(Id::Timestamp, 0) + simpleBlock(1, 0, frame)
                                          + idBytes(Id::SimpleBlock) + sizeBytes(video_size)
                                          + video_lead;
        const std::vector<uint8_t> after = simpleBlock(1, 20, frame);
        const uint64_t gap = video_size - video_lead.size();

        const uint64_t cluster_size = before.size() + gap + after.size();
        const std::vector<uint8_t> cluster_header = idBytes(Id::Cluster) + sizeBytes(cluster_size);
        const std::vector<uint8_t> in_segment = info + tracks + cluster_header;
        const uint64_t segment_size = in_segment.size() + cluster_size;
        const std::vector<uint8_t> head = ebmlHeader("matroska")
                                        + idBytes(Id::Segment) + sizeBytes(segment_size)
                                        + in_segment + before;

        MatroskaDemuxer demuxer(std::make_unique<GapIOHandler>(head, gap, after));
        ASSERT_TRUE(demuxer.parseContainer(), "fixture should parse");

        auto first = demuxer.readChunk();
        ASSERT_TRUE(first.isValid(), "the first audio frame is read");
        auto second = demuxer.readChunk();
        ASSERT_TRUE(second.isValid(), "the audio after the oversized video block is still read");
        ASSERT_EQUALS(samplesAt(20), second.timestamp_samples, "and it is the 20 ms frame");
    }
};

/// A Block (inside a BlockGroup) holding one unlaced frame for a track.
std::vector<uint8_t> block(uint64_t track, int16_t relative_ticks,
                           const std::vector<uint8_t>& frame)
{
    const auto raw = static_cast<uint16_t>(relative_ticks);
    std::vector<uint8_t> body{static_cast<uint8_t>(0x80 | track),
                              static_cast<uint8_t>(raw >> 8),
                              static_cast<uint8_t>(raw & 0xFF),
                              0x00};
    return element(Id::Block, body + frame);
}

/// DiscardPadding, a signed integer, written in four bytes.
std::vector<uint8_t> discardPadding(int32_t nanoseconds)
{
    const auto raw = static_cast<uint32_t>(nanoseconds);
    return element(Id::DiscardPadding, {static_cast<uint8_t>(raw >> 24), static_cast<uint8_t>(raw >> 16),
                                        static_cast<uint8_t>(raw >> 8), static_cast<uint8_t>(raw)});
}

class DiscardPaddingTest : public TestCase {
public:
    DiscardPaddingTest() : TestCase("DiscardPadding reaches the chunk of the frame it pads") {}

protected:
    void runTest() override
    {
        const std::vector<uint8_t> frame(kFrameBytes, 0x5A);
        const std::vector<uint8_t> cluster =
            element(Id::Cluster,
                    uintEl(Id::Timestamp, 0)
                  // Padding at the start, given before the Block.
                  + element(Id::BlockGroup, discardPadding(-20000000) + block(1, 0, frame))
                  + simpleBlock(1, 20, frame)
                  // Padding at the end, given after the Block, the order
                  // FFmpeg writes.
                  + element(Id::BlockGroup, block(1, 40, frame) + discardPadding(10000000)));
        const std::vector<uint8_t> file =
            ebmlHeader("matroska")
          + element(Id::Segment, element(Id::Info, uintEl(Id::TimestampScale, 1000000))
                               + element(Id::Tracks, pcmTrack(1)) + cluster);
        auto handler = std::make_unique<MemoryIOHandler>(file.data(), file.size());
        MatroskaDemuxer demuxer(std::move(handler));
        ASSERT_TRUE(demuxer.parseContainer(), "fixture should parse");

        const MediaChunk head = demuxer.readChunk();
        ASSERT_TRUE(head.isValid(), "the first block is read");
        ASSERT_EQUALS(uint32_t{882}, head.padding_head_frames, "20 ms at 44.1 kHz off the front");
        ASSERT_EQUALS(uint32_t{0}, head.padding_tail_frames, "and none off the end");

        const MediaChunk plain = demuxer.readChunk();
        ASSERT_TRUE(plain.isValid(), "the SimpleBlock is read");
        ASSERT_EQUALS(uint32_t{0}, plain.padding_head_frames + plain.padding_tail_frames,
                      "a block without DiscardPadding has none");

        const MediaChunk tail = demuxer.readChunk();
        ASSERT_TRUE(tail.isValid(), "the last block is read");
        ASSERT_EQUALS(samplesAt(40), tail.timestamp_samples, "it is the 40 ms block");
        ASSERT_EQUALS(uint32_t{441}, tail.padding_tail_frames, "10 ms off the end");
        ASSERT_EQUALS(uint32_t{0}, tail.padding_head_frames, "and none off the front");
    }
};

} // namespace

int main()
{
    TestSuite suite("Matroska Seek Landing Tests");
    suite.addTest(std::make_unique<ReportsGranulePositionsTest>());
    suite.addTest(std::make_unique<SeekReportsLandingTest>());
    suite.addTest(std::make_unique<SeekOnBoundaryTest>());
    suite.addTest(std::make_unique<GranuleTracksPlaybackTest>());
    suite.addTest(std::make_unique<SeekBackwardsTest>());
    suite.addTest(std::make_unique<SeekLandsOnClusterHeadNotCueTimeTest>());
    suite.addTest(std::make_unique<DamagedCuesCostSeekingNotPlaybackTest>());
    suite.addTest(std::make_unique<SeekBeforeFirstCueTest>());
    suite.addTest(std::make_unique<SeekLandsOnFirstAudioBlockTest>());
    suite.addTest(std::make_unique<HeaderStrippedFramesTest>());
    suite.addTest(std::make_unique<OversizedVideoBlockTest>());
    suite.addTest(std::make_unique<DiscardPaddingTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    // Every test that did not pass counts: getFailureCount() skips a test that
    // ended in an exception.
    return static_cast<int>(results.size()) - suite.getPassedCount(results);
}
