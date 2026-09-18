/*
 * test_matroska_tags.cpp - Matroska Tags as they apply to the track played
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

constexpr uint64_t kAudioUid = 0xA1;
constexpr uint64_t kVideoUid = 0xB2;

std::vector<uint8_t> tag(std::vector<uint8_t> targets, std::vector<uint8_t> simple_tags)
{
    return element(Id::Tag, element(Id::Targets, targets) + simple_tags);
}

std::vector<uint8_t> simpleTag(const std::string& name, const std::string& value)
{
    return element(Id::SimpleTag, strEl(Id::TagName, name) + strEl(Id::TagString, value));
}

/// An audio and a video track, one cluster, and @p tags after it, found
/// through a SeekHead.
std::vector<uint8_t> fileWithTags(const std::vector<uint8_t>& tags)
{
    const std::vector<uint8_t> tracks = element(Id::Tracks,
        element(Id::TrackEntry, uintEl(Id::TrackNumber, 1)
                              + uintEl(Id::TrackUID, kAudioUid)
                              + uintEl(Id::TrackType, TrackType::Audio)
                              + strEl(Id::CodecID, "A_PCM/INT/LIT")
                              + element(Id::Audio, floatEl(Id::SamplingFrequency, 44100.0)
                                                 + uintEl(Id::Channels, 2)
                                                 + uintEl(Id::BitDepth, 16)))
      + element(Id::TrackEntry, uintEl(Id::TrackNumber, 2)
                              + uintEl(Id::TrackUID, kVideoUid)
                              + uintEl(Id::TrackType, TrackType::Video)
                              + strEl(Id::CodecID, "V_UNCOMPRESSED")));
    const std::vector<uint8_t> front = element(Id::Info, uintEl(Id::TimestampScale, 1000000)) + tracks;
    const std::vector<uint8_t> cluster =
        element(Id::Cluster, uintEl(Id::Timestamp, 0)
                           + element(Id::SimpleBlock, {0x81, 0x00, 0x00, 0x80, 0, 0, 0, 0}));
    const std::vector<uint8_t> tags_element = element(Id::Tags, tags);

    // SeekPosition eight bytes wide, so the SeekHead's size is known before
    // the position it holds.
    auto seekHead = [](uint64_t position) {
        std::vector<uint8_t> bytes(8);
        for (int i = 7; i >= 0; --i, position >>= 8) {
            bytes[static_cast<size_t>(i)] = static_cast<uint8_t>(position & 0xFF);
        }
        return element(Id::SeekHead, element(Id::Seek, element(Id::SeekID, idBytes(Id::Tags))
                                                     + element(Id::SeekPosition, bytes)));
    };
    const size_t head = seekHead(0).size();
    const std::vector<uint8_t> body = seekHead(head + front.size() + cluster.size())
                                    + front + cluster + tags_element;
    return ebmlHeader("matroska") + element(Id::Segment, body);
}

std::vector<std::string> valuesOf(const std::vector<uint8_t>& file, const std::string& key)
{
    MatroskaDemuxer demuxer(std::make_unique<MemoryIOHandler>(file.data(), file.size()));
    if (!demuxer.parseContainer()) {
        return {"<unparsed>"};
    }
    return demuxer.getTag().getTagValues(key);
}

using Values = std::vector<std::string>;

class LevelsTest : public TestCase {
public:
    LevelsTest() : TestCase("A lower level's value replaces an upper one's, and an empty one cancels it") {}

protected:
    void runTest() override
    {
        const std::vector<uint8_t> file = fileWithTags(
            tag(uintEl(Id::TargetTypeValue, 70), simpleTag("TITLE", "Collection"))
          + tag(uintEl(Id::TargetTypeValue, 50),
                simpleTag("TITLE", "Album") + simpleTag("ARTIST", "Various Artists")
              + simpleTag("GENRE", "Rock") + simpleTag("DATE_RELEASED", "2001"))
          + tag(uintEl(Id::TargetTypeValue, 30),
                simpleTag("TITLE", "Song") + simpleTag("ARTIST", "A") + simpleTag("GENRE", "")));

        ASSERT_TRUE(valuesOf(file, "TITLE") == Values{"Song"}, "the track's own title");
        ASSERT_TRUE(valuesOf(file, "ALBUM") == Values{"Album"}, "the album's title, not the collection's");
        ASSERT_TRUE(valuesOf(file, "ARTIST") == Values{"A"}, "the track's artist replaces the album's");
        ASSERT_TRUE(valuesOf(file, "ALBUMARTIST") == Values{"Various Artists"}, "which is the album artist");
        ASSERT_TRUE(valuesOf(file, "GENRE").empty(), "an empty GENRE cancels the album's");
        ASSERT_TRUE(valuesOf(file, "DATE_RELEASED") == Values{"2001"}, "the album's date passes down");

        // The same value at two levels is one value.
        const std::vector<uint8_t> same = fileWithTags(
            tag(uintEl(Id::TargetTypeValue, 50), simpleTag("ARTIST", "Band"))
          + tag(uintEl(Id::TargetTypeValue, 30), simpleTag("ARTIST", "Band")));
        ASSERT_TRUE(valuesOf(same, "ARTIST") == Values{"Band"}, "not \"Band, Band\"");
        ASSERT_TRUE(valuesOf(same, "TITLE").empty(), "and there is no track title to invent");

        // Several values at one level stay separate.
        const std::vector<uint8_t> several = fileWithTags(
            tag(uintEl(Id::TargetTypeValue, 30), simpleTag("ARTIST", "X") + simpleTag("ARTIST", "Y")));
        ASSERT_TRUE(valuesOf(several, "ARTIST") == (Values{"X", "Y"}), "two artists stay two");
    }
};

class TargetsTest : public TestCase {
public:
    TargetsTest() : TestCase("Tags aimed at other tracks, chapters or attachments are left out") {}

protected:
    void runTest() override
    {
        const std::vector<uint8_t> file = fileWithTags(
            tag({}, simpleTag("TITLE", "Album"))
          + tag(uintEl(Id::TagTrackUID, kVideoUid), simpleTag("TITLE", "Video"))
          + tag(uintEl(Id::TargetTypeValue, 30) + uintEl(Id::TagTrackUID, kAudioUid),
                simpleTag("ARTIST", "A"))
          + tag(uintEl(Id::TargetTypeValue, 30) + uintEl(Id::TagChapterUID, 5),
                simpleTag("TITLE", "Chapter 1"))
          + tag(uintEl(Id::TagAttachmentUID, 9), simpleTag("COMMENT", "cover"))
          + tag(uintEl(Id::TargetTypeValue, 30) + uintEl(Id::TagTrackUID, 0),
                simpleTag("COMPOSER", "C")));

        ASSERT_TRUE(valuesOf(file, "ALBUM") == Values{"Album"}, "the unscoped album title");
        ASSERT_TRUE(valuesOf(file, "ARTIST") == Values{"A"}, "the tag aimed at this track");
        ASSERT_TRUE(valuesOf(file, "TITLE").empty(), "neither the video's nor a chapter's title");
        ASSERT_TRUE(valuesOf(file, "COMMENT").empty(), "nor an attachment's");
        ASSERT_TRUE(valuesOf(file, "COMPOSER") == Values{"C"}, "a TagTrackUID of 0 means every track");
    }
};

} // namespace

int test_matroska_tags_main()
{
    TestSuite suite("Matroska Tags");
    suite.addTest(std::make_unique<LevelsTest>());
    suite.addTest(std::make_unique<TargetsTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return static_cast<int>(results.size()) - suite.getPassedCount(results);
}
