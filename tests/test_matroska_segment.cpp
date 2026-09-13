/*
 * test_matroska_segment.cpp - Matroska Info/Tracks parsing and codec mapping
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
using PsyMP3::IO::MemoryIOHandler;

namespace {

using namespace MatroskaBuilder;

/// Runs a SegmentParser over bytes held in memory.
class Parsed {
public:
    explicit Parsed(std::vector<uint8_t> data)
        : m_data(std::move(data))
        , m_handler(m_data.data(), m_data.size(), false)
        , m_reader(&m_handler)
    {
        m_parser.parse(m_reader);
    }
    const SegmentParser& parser() const { return m_parser; }

private:
    std::vector<uint8_t> m_data;
    MemoryIOHandler m_handler;
    EBMLReader m_reader;
    SegmentParser m_parser;
};

/// A complete single-track file, as the other cases keep needing one.
std::vector<uint8_t> opusFile(uint64_t timestamp_scale = 1000000,
                              double duration_ticks = 2008.0,
                              uint64_t codec_delay_ns = 6500000)
{
    const std::vector<uint8_t> audio =
        element(Id::Audio, floatEl(Id::SamplingFrequency, 48000.0)
                         + uintEl(Id::Channels, 2));

    const std::vector<uint8_t> track =
        element(Id::TrackEntry, uintEl(Id::TrackNumber, 1)
                              + uintEl(Id::TrackUID, 0xDEADBEEF)
                              + uintEl(Id::TrackType, TrackType::Audio)
                              + strEl(Id::CodecID, "A_OPUS")
                              + element(Id::CodecPrivate,
                                        std::vector<uint8_t>{'O','p','u','s','H','e','a','d'})
                              + uintEl(Id::CodecDelay, codec_delay_ns)
                              + uintEl(Id::SeekPreRoll, 80000000)
                              + strEl(Id::Language, "und")
                              + audio);

    return ebmlHeader("matroska")
         + element(Id::Segment,
                   element(Id::Info, uintEl(Id::TimestampScale, timestamp_scale)
                                   + floatEl(Id::Duration, duration_ticks)
                                   + strEl(Id::MuxingApp, "PsyMP3 test"))
                 + element(Id::Tracks, track)
                 + element(Id::Cluster, uintEl(Id::Timestamp, 0)));
}

class CodecMappingTest : public TestCase {
public:
    CodecMappingTest() : TestCase("Matroska CodecIDs map onto the codecs PsyMP3 has") {}

protected:
    void runTest() override
    {
        ASSERT_TRUE(codecNameForId("A_OPUS") == "opus", "A_OPUS");
        ASSERT_TRUE(codecNameForId("A_VORBIS") == "vorbis", "A_VORBIS");
        ASSERT_TRUE(codecNameForId("A_FLAC") == "flac", "A_FLAC");
        ASSERT_TRUE(codecNameForId("A_MPEG/L3") == "mp3", "A_MPEG/L3");
        ASSERT_TRUE(codecNameForId("A_MPEG/L2") == "mp2", "A_MPEG/L2");
        ASSERT_TRUE(codecNameForId("A_TRUEHD") == "truehd", "A_TRUEHD");
        ASSERT_TRUE(codecNameForId("A_PCM/INT/LIT") == "pcm", "A_PCM/INT/LIT");

        // AAC is written bare and with a profile suffix; one decoder covers
        // the family, so both forms have to land on it.
        ASSERT_TRUE(codecNameForId("A_AAC") == "aac", "bare A_AAC");
        ASSERT_TRUE(codecNameForId("A_AAC/MPEG4/LC/SBR") == "aac", "A_AAC with a profile");
        ASSERT_TRUE(codecNameForId("A_AAC/MPEG2/MAIN") == "aac", "A_AAC/MPEG2/MAIN");

        // The prefix rule must not swallow a different codec that merely
        // starts the same way.
        ASSERT_TRUE(codecNameForId("A_AACPLUS").empty(),
                    "A prefix match requires the separator, so A_AACPLUS is not AAC");

        // AC-3 and E-AC-3 are decoded in tree, and are the commonest audio in .mkv.
        ASSERT_TRUE(codecNameForId("A_AC3") == "ac3", "A_AC3");
        ASSERT_TRUE(codecNameForId("A_EAC3") == "eac3", "A_EAC3");

        // Unsupported codecs map to nothing on purpose: a file can then be
        // refused by name instead of failing at its first packet. DTS is the
        // common one in .mkv.
        ASSERT_TRUE(codecNameForId("A_DTS").empty(), "DTS has no decoder in tree");
        ASSERT_TRUE(codecNameForId("V_VP9").empty(), "A video codec is not an audio codec");

        // PCM byte order is a property of the CodecID, and getting it wrong
        // decodes to static rather than to something subtly off -- the same
        // failure AIFF had.
        ASSERT_TRUE(codecIsBigEndianPCM("A_PCM/INT/BIG"), "A_PCM/INT/BIG is big-endian");
        ASSERT_FALSE(codecIsBigEndianPCM("A_PCM/INT/LIT"), "A_PCM/INT/LIT is not");
    }
};

class DocTypeTest : public TestCase {
public:
    DocTypeTest() : TestCase("Only matroska and webm DocTypes are accepted") {}

protected:
    void runTest() override
    {
        { Parsed parsed(opusFile());
          ASSERT_TRUE(parsed.parser().docType() == "matroska", "matroska is accepted"); }

        {   // Anything EBML-shaped has a header. Without this check a file that
            // is not Matroska at all is parsed as far as its Tracks before
            // failing on something much less obvious.
            std::vector<uint8_t> other = ebmlHeader("not-matroska")
                                       + element(Id::Segment, {});
            bool threw = false;
            try { Parsed parsed(std::move(other)); }
            catch (const std::exception&) { threw = true; }
            ASSERT_TRUE(threw, "A foreign DocType is refused up front");
        }
        {   bool threw = false;
            try { Parsed parsed(std::vector<uint8_t>{0x00, 0x01, 0x02, 0x03}); }
            catch (const std::exception&) { threw = true; }
            ASSERT_TRUE(threw, "Bytes that are not EBML at all are refused");
        }
    }
};

class SegmentInfoTest : public TestCase {
public:
    SegmentInfoTest() : TestCase("Duration is ticks scaled by TimestampScale, not milliseconds") {}

protected:
    void runTest() override
    {
        {   Parsed parsed(opusFile());
            const SegmentInfo& info = parsed.parser().info();
            ASSERT_TRUE(info.timestamp_scale_ns == 1000000, "The usual 1 ms tick");
            ASSERT_TRUE(info.duration_ticks == 2008.0, "Duration is a float, in ticks");
            ASSERT_TRUE(info.durationMs() == 2008, "2008 ticks of 1 ms is 2008 ms");
            ASSERT_TRUE(info.muxing_app == "PsyMP3 test", "MuxingApp reads back");
        }
        {   // The whole point of reading the scale rather than assuming it. At
            // 100 microseconds a tick, the same 2008 ticks is a fifth of the
            // duration -- a file read as if the scale were the default would be
            // wrong by exactly that ratio.
            Parsed parsed(opusFile(100000, 2008.0));
            ASSERT_TRUE(parsed.parser().info().durationMs() == 200,
                        "2008 ticks of 100 us is 200 ms, not 2008");
        }
        {   // A zero scale would make every timestamp in the file zero and put
            // a division by zero in the way of finding out.
            Parsed parsed(opusFile(0, 1000.0));
            ASSERT_TRUE(parsed.parser().info().timestamp_scale_ns == 1000000,
                        "A zero TimestampScale falls back to the default");
        }
    }
};

class TrackEntryTest : public TestCase {
public:
    TrackEntryTest() : TestCase("A TrackEntry and its Audio sub-element read back whole") {}

protected:
    void runTest() override
    {
        Parsed parsed(opusFile());
        const std::vector<TrackEntry>& tracks = parsed.parser().tracks();
        ASSERT_EQUALS(size_t{1}, tracks.size(), "One track");

        const TrackEntry& track = tracks[0];
        ASSERT_TRUE(track.number == 1, "TrackNumber");
        ASSERT_TRUE(track.uid == 0xDEADBEEF, "TrackUID");
        ASSERT_TRUE(track.isAudio(), "TrackType 2 is audio");
        ASSERT_TRUE(track.codec_id == "A_OPUS", "CodecID");
        ASSERT_TRUE(track.language == "und", "Language");
        ASSERT_TRUE(track.codec_delay_ns == 6500000, "CodecDelay in nanoseconds");
        ASSERT_TRUE(track.seek_preroll_ns == 80000000, "SeekPreRoll in nanoseconds");
        ASSERT_EQUALS(size_t{8}, track.codec_private.size(), "CodecPrivate survives");
        ASSERT_TRUE(track.codec_private[0] == 'O', "CodecPrivate is the bytes written");
        // The Audio sub-element is nested a level deeper than the rest.
        ASSERT_TRUE(track.sampling_frequency == 48000.0, "SamplingFrequency");
        ASSERT_TRUE(track.channels == 2, "Channels");

        ASSERT_TRUE(parsed.parser().firstClusterOffset() > 0,
                    "Parsing stopped at the first Cluster and recorded where it is");
    }
};

class TrackSelectionTest : public TestCase {
public:
    TrackSelectionTest() : TestCase("Track selection skips video and codecs with no decoder") {}

protected:
    void runTest() override
    {
        // A .mkv shaped like a real one: a video track first, then a DTS
        // track PsyMP3 cannot decode, then a FLAC track it can.
        const std::vector<uint8_t> video =
            element(Id::TrackEntry, uintEl(Id::TrackNumber, 1)
                                  + uintEl(Id::TrackType, TrackType::Video)
                                  + strEl(Id::CodecID, "V_MPEG4/ISO/AVC"));
        const std::vector<uint8_t> dts =
            element(Id::TrackEntry, uintEl(Id::TrackNumber, 2)
                                  + uintEl(Id::TrackType, TrackType::Audio)
                                  + strEl(Id::CodecID, "A_DTS")
                                  + element(Id::Audio, uintEl(Id::Channels, 6)));
        const std::vector<uint8_t> flac =
            element(Id::TrackEntry, uintEl(Id::TrackNumber, 3)
                                  + uintEl(Id::TrackType, TrackType::Audio)
                                  + strEl(Id::CodecID, "A_FLAC")
                                  + element(Id::Audio, floatEl(Id::SamplingFrequency, 44100.0)
                                                     + uintEl(Id::Channels, 2)));

        Parsed parsed(ebmlHeader("matroska")
                      + element(Id::Segment,
                                element(Id::Info, uintEl(Id::TimestampScale, 1000000))
                              + element(Id::Tracks, video + dts + flac)));

        ASSERT_EQUALS(size_t{3}, parsed.parser().tracks().size(), "All three tracks parse");
        const TrackEntry* chosen = parsed.parser().preferredAudioTrack();
        ASSERT_NOT_NULL(chosen, "An audio track was chosen");
        ASSERT_TRUE(chosen->number == 3,
                    "The video track is passed over and so is the DTS one, because "
                    "nothing in the tree decodes DTS -- picking it would open the "
                    "file and then fail at its first packet");

        // A file whose only audio is undecodable must report nothing rather
        // than hand back a track that cannot be played.
        Parsed no_playable(ebmlHeader("matroska")
                           + element(Id::Segment,
                                     element(Id::Info, uintEl(Id::TimestampScale, 1000000))
                                   + element(Id::Tracks, video + dts)));
        ASSERT_NULL(no_playable.parser().preferredAudioTrack(),
                    "No decodable audio means no track, not a hopeful guess");
    }
};

class DefaultFlagTest : public TestCase {
public:
    DefaultFlagTest() : TestCase("A track flagged default wins over an earlier one") {}

protected:
    void runTest() override
    {
        auto audioTrack = [](uint64_t number, const char* codec, bool is_default) {
            std::vector<uint8_t> body = uintEl(Id::TrackNumber, number)
                                      + uintEl(Id::TrackType, TrackType::Audio)
                                      + strEl(Id::CodecID, codec)
                                      + element(Id::Audio, uintEl(Id::Channels, 2));
            if (is_default) {
                body = body + uintEl(Id::FlagDefault, 1);
            }
            return element(Id::TrackEntry, body);
        };

        Parsed parsed(ebmlHeader("matroska")
                      + element(Id::Segment,
                                element(Id::Info, uintEl(Id::TimestampScale, 1000000))
                              + element(Id::Tracks, audioTrack(1, "A_VORBIS", false)
                                                  + audioTrack(2, "A_OPUS", true))));
        const TrackEntry* chosen = parsed.parser().preferredAudioTrack();
        ASSERT_NOT_NULL(chosen, "A track was chosen");
        ASSERT_TRUE(chosen->number == 2,
                    "FlagDefault beats file order, which is how a multi-language "
                    "release says which track to play");
    }
};

class StreamInfoMappingTest : public TestCase {
public:
    StreamInfoMappingTest() : TestCase("Matroska units are converted into the ones PsyMP3 uses") {}

protected:
    void runTest() override
    {
        {   Parsed parsed(opusFile());
            const TrackEntry* track = parsed.parser().preferredAudioTrack();
            ASSERT_NOT_NULL(track, "A track was chosen");
            const StreamInfo info = parsed.parser().toStreamInfo(*track);

            ASSERT_TRUE(info.codec_type == "audio", "Audio");
            ASSERT_TRUE(info.codec_name == "opus", "Mapped to the codec registry's name");
            ASSERT_TRUE(info.sample_rate == 48000, "Sample rate");
            ASSERT_TRUE(info.channels == 2, "Channels");
            ASSERT_TRUE(info.duration_ms == 2008, "Duration in milliseconds");
            ASSERT_EQUALS(size_t{8}, info.codec_data.size(), "CodecPrivate becomes codec_data");

            // 6,500,000 ns at 48 kHz is 312 sample frames, which is what
            // ffprobe calls initial_padding on the same file. Nanoseconds are
            // no use to a decoder that trims in frames.
            ASSERT_TRUE(info.encoder_delay == 312,
                        "CodecDelay converts from nanoseconds to sample frames");
        }
        {   // SBR: the stream is coded at half the rate it plays at, and
            // Matroska says so with OutputSamplingFrequency. Taking the coded
            // rate would play an HE-AAC track at half speed.
            const std::vector<uint8_t> track =
                element(Id::TrackEntry, uintEl(Id::TrackNumber, 1)
                                      + uintEl(Id::TrackType, TrackType::Audio)
                                      + strEl(Id::CodecID, "A_AAC/MPEG4/LC/SBR")
                                      + element(Id::Audio,
                                                floatEl(Id::SamplingFrequency, 22050.0)
                                              + floatEl(Id::OutputSamplingFrequency, 44100.0)
                                              + uintEl(Id::Channels, 2)));
            Parsed parsed(ebmlHeader("matroska")
                          + element(Id::Segment,
                                    element(Id::Info, uintEl(Id::TimestampScale, 1000000))
                                  + element(Id::Tracks, track)));
            const StreamInfo info =
                parsed.parser().toStreamInfo(*parsed.parser().preferredAudioTrack());
            ASSERT_TRUE(info.sample_rate == 44100,
                        "The playback rate wins over the coded rate, or an SBR "
                        "track plays at half speed");
        }
        {   // Big-endian PCM has to be flagged, or it decodes to static.
            const std::vector<uint8_t> track =
                element(Id::TrackEntry, uintEl(Id::TrackNumber, 1)
                                      + uintEl(Id::TrackType, TrackType::Audio)
                                      + strEl(Id::CodecID, "A_PCM/INT/BIG")
                                      + element(Id::Audio,
                                                floatEl(Id::SamplingFrequency, 44100.0)
                                              + uintEl(Id::Channels, 2)
                                              + uintEl(Id::BitDepth, 24)));
            Parsed parsed(ebmlHeader("matroska")
                          + element(Id::Segment,
                                    element(Id::Info, uintEl(Id::TimestampScale, 1000000))
                                  + element(Id::Tracks, track)));
            const StreamInfo info =
                parsed.parser().toStreamInfo(*parsed.parser().preferredAudioTrack());
            ASSERT_TRUE(info.big_endian_samples, "A_PCM/INT/BIG sets the byte-order flag");
            ASSERT_TRUE(info.bits_per_sample == 24, "BitDepth");
        }
    }
};

} // namespace

int main()
{
    TestSuite suite("Matroska Segment Parsing Tests");
    suite.addTest(std::make_unique<CodecMappingTest>());
    suite.addTest(std::make_unique<DocTypeTest>());
    suite.addTest(std::make_unique<SegmentInfoTest>());
    suite.addTest(std::make_unique<TrackEntryTest>());
    suite.addTest(std::make_unique<TrackSelectionTest>());
    suite.addTest(std::make_unique<DefaultFlagTest>());
    suite.addTest(std::make_unique<StreamInfoMappingTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
