/*
 * test_matroska_cues.cpp - Matroska SeekHead, Cues, and the scan fallback
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

/// A Cluster holding a timestamp and one token block.
std::vector<uint8_t> cluster(uint64_t timestamp, bool crc_first = false)
{
    std::vector<uint8_t> body;
    if (crc_first) {
        // ffmpeg writes a CRC-32 ahead of the timestamp. A scan that reads only
        // the first child finds this and concludes the cluster is untimed.
        body = body + element(Id::CRC32, std::vector<uint8_t>{0, 0, 0, 0});
    }
    body = body + uintEl(Id::Timestamp, timestamp)
                + element(Id::SimpleBlock,
                          std::vector<uint8_t>{0x81, 0x00, 0x00, 0x80, 0xAA, 0xBB});
    return element(Id::Cluster, body);
}

/// A CuePoint for one track.
std::vector<uint8_t> cuePoint(uint64_t time_ticks, uint64_t track, uint64_t position)
{
    return element(Id::CuePoint,
                   uintEl(Id::CueTime, time_ticks)
                 + element(Id::CueTrackPositions,
                           uintEl(Id::CueTrack, track)
                         + uintEl(Id::CueClusterPosition, position)));
}

/// Holds bytes and hands out a reader over them.
///
/// The payload is placed after a lead-in rather than at offset 0. Both index
/// builders read an offset of 0 as "absent" -- SeekHead never named the Cues,
/// or no Cluster was found -- and that is sound precisely because offset 0 is
/// the one place neither can be: a file starts with its EBML header. Writing
/// the tests at offset 0 would be testing a layout that cannot occur.
class Bytes {
public:
    explicit Bytes(std::vector<uint8_t> data, size_t lead_in = 64)
        : m_offset(lead_in)
    {
        m_data.assign(lead_in, 0x00);
        m_data.insert(m_data.end(), data.begin(), data.end());
        m_handler = std::make_unique<MemoryIOHandler>(m_data.data(), m_data.size(), false);
        m_reader = std::make_unique<EBMLReader>(m_handler.get());
    }
    EBMLReader& reader() { return *m_reader; }
    uint64_t size() const { return m_data.size(); }
    /// Where the payload actually begins.
    uint64_t offset() const { return m_offset; }

private:
    std::vector<uint8_t> m_data;
    uint64_t m_offset;
    std::unique_ptr<MemoryIOHandler> m_handler;
    std::unique_ptr<EBMLReader> m_reader;
};

std::vector<uint8_t> audioTrack(uint64_t number, const char* codec = "A_OPUS")
{
    return element(Id::TrackEntry, uintEl(Id::TrackNumber, number)
                                 + uintEl(Id::TrackType, TrackType::Audio)
                                 + strEl(Id::CodecID, codec)
                                 + element(Id::Audio, uintEl(Id::Channels, 2)));
}

class ScanFindsTimestampTest : public TestCase {
public:
    ScanFindsTimestampTest() : TestCase("A scan finds a Cluster's timestamp past a leading CRC-32") {}

protected:
    void runTest() override
    {
        // Timestamp must precede a cluster's blocks, but it need not be the
        // first child, and ffmpeg does put a CRC-32 ahead of it. Reading only
        // the first child leaves the whole index empty -- which is what this
        // caught on a real file.
        std::vector<uint8_t> clusters =
            cluster(0, true) + cluster(500, true) + cluster(1000, true);
        Bytes bytes(clusters);

        CueIndex index;
        ASSERT_TRUE(index.buildByScanning(bytes.reader(), bytes.offset(), bytes.size()), "the scan builds");
        ASSERT_EQUALS(size_t{3}, index.size(), "every cluster is found, CRC-32 or not");
        ASSERT_TRUE(index.entries()[0].time_ticks == 0, "first timestamp");
        ASSERT_TRUE(index.entries()[1].time_ticks == 500, "second timestamp");
        ASSERT_TRUE(index.entries()[2].time_ticks == 1000, "third timestamp");

        // The same clusters without the CRC still work, so the fix did not
        // simply move the assumption one element along.
        std::vector<uint8_t> plain = cluster(0) + cluster(500);
        Bytes other(plain);
        CueIndex plain_index;
        ASSERT_TRUE(plain_index.buildByScanning(other.reader(), other.offset(), other.size()), "builds");
        ASSERT_EQUALS(size_t{2}, plain_index.size(), "both clusters found");
    }
};

class SeekTargetTest : public TestCase {
public:
    SeekTargetTest() : TestCase("A seek lands on or before its target, never after") {}

protected:
    void runTest() override
    {
        std::vector<uint8_t> clusters =
            cluster(0) + cluster(500) + cluster(1000) + cluster(1500);
        Bytes bytes(clusters);
        CueIndex index;
        ASSERT_TRUE(index.buildByScanning(bytes.reader(), bytes.offset(), bytes.size()), "builds");

        // Landing after the target would skip audio outright, so the entry
        // chosen is always the last one at or before it.
        ASSERT_TRUE(index.entryFor(0)->time_ticks == 0, "exactly the first");
        ASSERT_TRUE(index.entryFor(499)->time_ticks == 0, "just short of the second");
        ASSERT_TRUE(index.entryFor(500)->time_ticks == 500, "exactly on an entry");
        ASSERT_TRUE(index.entryFor(501)->time_ticks == 500, "just past an entry");
        ASSERT_TRUE(index.entryFor(1499)->time_ticks == 1000, "between entries");
        ASSERT_TRUE(index.entryFor(999999)->time_ticks == 1500,
                    "past the end of the file, the last entry");

        for (uint64_t target : {0ULL, 1ULL, 499ULL, 500ULL, 1234ULL, 99999ULL}) {
            ASSERT_TRUE(index.entryFor(target)->time_ticks <= target
                            || target < index.entries().front().time_ticks,
                        "no target is answered with an entry that comes after it");
        }
    }
};

class CuesTrackFilterTest : public TestCase {
public:
    CuesTrackFilterTest() : TestCase("Cues for another track are not used to seek this one") {}

protected:
    void runTest() override
    {
        // ffmpeg cues only the video track of a .mkv. Trusting those entries
        // would seek audio by video keyframe positions -- which is not a parse
        // error, just wrong, and quietly so.
        const std::vector<uint8_t> cues =
            element(Id::Cues, cuePoint(0, 1, 100) + cuePoint(1000, 1, 200));
        Bytes bytes(cues);

        CueIndex index;
        ASSERT_FALSE(index.parseCues(bytes.reader(), bytes.offset(), 0, /*track=*/3),
                     "a Cues element naming only track 1 yields nothing for track 3");
        ASSERT_TRUE(index.empty(), "and leaves no entries behind");

        // The same element read for the track it does describe.
        Bytes again(cues);
        CueIndex for_one;
        ASSERT_TRUE(for_one.parseCues(again.reader(), again.offset(), 0, /*track=*/1), "track 1 is cued");
        ASSERT_EQUALS(size_t{2}, for_one.size(), "both points");
    }
};

class CuePositionBaseTest : public TestCase {
public:
    CuePositionBaseTest() : TestCase("CueClusterPosition is relative to the Segment, not the file") {}

protected:
    void runTest() override
    {
        const std::vector<uint8_t> cues = element(Id::Cues, cuePoint(750, 1, 4096));
        Bytes bytes(cues);

        CueIndex index;
        // A position is an offset from the start of the Segment's payload. Used
        // as a file offset it points somewhere earlier than it should by
        // exactly the size of everything before the Segment.
        ASSERT_TRUE(index.parseCues(bytes.reader(), bytes.offset(), /*segment_data_offset=*/1000, 1), "parses");
        ASSERT_EQUALS(size_t{1}, index.size(), "one entry");
        ASSERT_TRUE(index.entries()[0].cluster_offset == 5096,
                    "4096 from a Segment starting at 1000 is file offset 5096");
        ASSERT_TRUE(index.entries()[0].time_ticks == 750, "and the time comes through");
    }
};

class CuesOrderingTest : public TestCase {
public:
    CuesOrderingTest() : TestCase("Cues are sorted, since nothing requires a file to write them in order") {}

protected:
    void runTest() override
    {
        const std::vector<uint8_t> cues =
            element(Id::Cues, cuePoint(2000, 1, 300)
                            + cuePoint(0, 1, 100)
                            + cuePoint(1000, 1, 200));
        Bytes bytes(cues);
        CueIndex index;
        ASSERT_TRUE(index.parseCues(bytes.reader(), bytes.offset(), 0, 1), "parses");
        ASSERT_EQUALS(size_t{3}, index.size(), "three points");
        ASSERT_TRUE(index.entries()[0].time_ticks == 0
                        && index.entries()[1].time_ticks == 1000
                        && index.entries()[2].time_ticks == 2000,
                    "out-of-order points are sorted, which the lookup depends on");
        ASSERT_TRUE(index.entryFor(1500)->time_ticks == 1000, "and the lookup is correct");
    }
};

class SeekHeadTest : public TestCase {
public:
    SeekHeadTest() : TestCase("SeekHead positions are resolved against the Segment") {}

protected:
    void runTest() override
    {
        auto seekEntry = [](uint32_t id, uint64_t position) {
            return element(Id::Seek, element(Id::SeekID, idBytes(id))
                                   + uintEl(Id::SeekPosition, position));
        };

        const std::vector<uint8_t> file =
            ebmlHeader("matroska")
            + element(Id::Segment,
                      element(Id::SeekHead, seekEntry(Id::Info, 100)
                                          + seekEntry(Id::Tracks, 200)
                                          + seekEntry(Id::Cues, 5000))
                    + element(Id::Info, uintEl(Id::TimestampScale, 1000000))
                    + element(Id::Tracks, audioTrack(1)));

        // A whole file starts at offset 0 by definition, so no lead-in here.
        Bytes bytes(file, /*lead_in=*/0);
        SegmentParser parser;
        parser.parse(bytes.reader());

        const uint64_t base = parser.segmentDataOffset();
        ASSERT_TRUE(base > 0, "the Segment's payload starts somewhere");
        ASSERT_TRUE(parser.seekPosition(Id::Cues) == base + 5000,
                    "a SeekHead position is an offset from the Segment, not the file");
        ASSERT_TRUE(parser.seekPosition(Id::Tracks) == base + 200, "Tracks");
        ASSERT_TRUE(parser.seekPosition(Id::Info) == base + 100, "Info");
        ASSERT_TRUE(parser.seekPosition(Id::Tags) == 0, "an element it does not mention is 0");
    }
};

class TracksAfterClustersTest : public TestCase {
public:
    TracksAfterClustersTest() : TestCase("Tracks written after the clusters are found through SeekHead") {}

protected:
    void runTest() override
    {
        // Legal, and a parse that stops at the first Cluster never reaches
        // them: without following SeekHead such a file reports no tracks and
        // cannot be played at all.
        const std::vector<uint8_t> head = ebmlHeader("matroska");
        const std::vector<uint8_t> info = element(Id::Info, uintEl(Id::TimestampScale, 1000000));
        const std::vector<uint8_t> clusters = cluster(0) + cluster(500);
        const std::vector<uint8_t> tracks = element(Id::Tracks, audioTrack(7, "A_FLAC"));

        // SeekHead has to be built first to know its own size, so the position
        // is computed from the sizes of what precedes Tracks inside the Segment.
        auto seekEntry = [](uint32_t id, uint64_t position) {
            return element(Id::Seek, element(Id::SeekID, idBytes(id))
                                   + uintEl(Id::SeekPosition, position));
        };
        std::vector<uint8_t> seek_head = element(Id::SeekHead, seekEntry(Id::Tracks, 0));
        const uint64_t tracks_at = seek_head.size() + info.size() + clusters.size();
        seek_head = element(Id::SeekHead, seekEntry(Id::Tracks, tracks_at));

        const std::vector<uint8_t> file =
            head + element(Id::Segment, seek_head + info + clusters + tracks);

        Bytes bytes(file, /*lead_in=*/0);
        SegmentParser parser;
        parser.parse(bytes.reader());

        ASSERT_EQUALS(size_t{1}, parser.tracks().size(),
                      "the track after the clusters is found, not missed");
        ASSERT_TRUE(parser.tracks()[0].number == 7, "and is the one written there");
        ASSERT_TRUE(parser.tracks()[0].codec_id == "A_FLAC", "with its CodecID intact");
        ASSERT_NOT_NULL(parser.preferredAudioTrack(), "so the file has playable audio");
    }
};

class EmptyAndMalformedTest : public TestCase {
public:
    EmptyAndMalformedTest() : TestCase("An index that cannot be built reports so rather than guessing") {}

protected:
    void runTest() override
    {
        {   CueIndex index;
            Bytes bytes(std::vector<uint8_t>{0x00});
            ASSERT_FALSE(index.parseCues(bytes.reader(), 0, 0, 1), "no Cues element there");
            ASSERT_NULL(index.entryFor(0), "an empty index answers nothing");
        }
        {   // A Cues offset of zero means SeekHead never mentioned it.
            CueIndex index;
            Bytes bytes(cluster(0));
            ASSERT_FALSE(index.parseCues(bytes.reader(), 0, 0, 1), "offset 0 is 'not present'");
        }
        {   // Something that is not a Cluster where the scan starts.
            CueIndex index;
            Bytes bytes(element(Id::Tags, uintEl(Id::TagName, 1)));
            ASSERT_FALSE(index.buildByScanning(bytes.reader(), bytes.offset(), bytes.size()),
                         "a region with no clusters yields no index");
        }
    }
};

} // namespace

int main()
{
    TestSuite suite("Matroska Cue Index Tests");
    suite.addTest(std::make_unique<ScanFindsTimestampTest>());
    suite.addTest(std::make_unique<SeekTargetTest>());
    suite.addTest(std::make_unique<CuesTrackFilterTest>());
    suite.addTest(std::make_unique<CuePositionBaseTest>());
    suite.addTest(std::make_unique<CuesOrderingTest>());
    suite.addTest(std::make_unique<SeekHeadTest>());
    suite.addTest(std::make_unique<TracksAfterClustersTest>());
    suite.addTest(std::make_unique<EmptyAndMalformedTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
