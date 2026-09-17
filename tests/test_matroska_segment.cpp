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
    DefaultFlagTest() : TestCase("FlagDefault's schema default of 1 decides which track plays") {}

protected:
    void runTest() override
    {
        // flag: 1 writes FlagDefault 1, 0 writes an explicit 0, and -1 omits the
        // element -- which RFC 9559 5.1.4.1.5 says *is* default, its schema
        // default being 1.
        auto audioTrack = [](uint64_t number, const char* codec, int flag) {
            std::vector<uint8_t> body = uintEl(Id::TrackNumber, number)
                                      + uintEl(Id::TrackType, TrackType::Audio)
                                      + strEl(Id::CodecID, codec)
                                      + element(Id::Audio, uintEl(Id::Channels, 2));
            if (flag >= 0) {
                body = body + uintEl(Id::FlagDefault, static_cast<uint64_t>(flag));
            }
            return element(Id::TrackEntry, body);
        };
        auto file = [](const std::vector<uint8_t>& tracks) {
            return ebmlHeader("matroska")
                 + element(Id::Segment,
                           element(Id::Info, uintEl(Id::TimestampScale, 1000000))
                         + element(Id::Tracks, tracks));
        };

        {   // What ffmpeg and mkvmerge actually emit: an explicit 0 on every
            // track that is not default, and nothing at all on the one that is,
            // because libmatroska renders with bWithDefault=false. Reading the
            // absence as "not default" picked track 1 -- precisely the track the
            // muxer marked ineligible.
            Parsed parsed(file(audioTrack(1, "A_VORBIS", 0) + audioTrack(2, "A_OPUS", -1)));
            const TrackEntry* chosen = parsed.parser().preferredAudioTrack();
            ASSERT_NOT_NULL(chosen, "A track was chosen");
            ASSERT_TRUE(chosen->number == 2,
                        "an omitted FlagDefault is default; an explicit 0 is not");
        }
        {   // Neither writes it, so both are eligible and file order decides --
            // 19.1 prefers the first of an equally preferable group.
            Parsed parsed(file(audioTrack(1, "A_VORBIS", -1) + audioTrack(2, "A_OPUS", -1)));
            const TrackEntry* chosen = parsed.parser().preferredAudioTrack();
            ASSERT_NOT_NULL(chosen, "A track was chosen");
            ASSERT_TRUE(chosen->number == 1, "equally default, so the first wins");
        }
        {   // An explicit 1 on the first track keeps it, though the second is
            // default by omission too.
            Parsed parsed(file(audioTrack(1, "A_VORBIS", 1) + audioTrack(2, "A_OPUS", -1)));
            const TrackEntry* chosen = parsed.parser().preferredAudioTrack();
            ASSERT_NOT_NULL(chosen, "A track was chosen");
            ASSERT_TRUE(chosen->number == 1, "explicit default on the first track");
        }
        {   // Nothing is eligible. A playable track is still better than none,
            // so the first decodable one stands in.
            Parsed parsed(file(audioTrack(1, "A_VORBIS", 0) + audioTrack(2, "A_OPUS", 0)));
            const TrackEntry* chosen = parsed.parser().preferredAudioTrack();
            ASSERT_NOT_NULL(chosen, "A playable track is still offered");
            ASSERT_TRUE(chosen->number == 1, "no eligible track, so first decodable");
        }
    }
};

class SchemaDefaultsTest : public TestCase {
public:
    SchemaDefaultsTest() : TestCase("Audio defaults apply when the elements are omitted") {}

protected:
    void runTest() override
    {
        auto parse = [](const std::vector<uint8_t>& track) {
            return ebmlHeader("matroska")
                 + element(Id::Segment,
                           element(Id::Info, uintEl(Id::TimestampScale, 1000000))
                         + element(Id::Tracks, track));
        };

        {   // No Audio element at all. RFC 8794 11.1.19: a reader must apply the
            // declared default of a mandatory element the writer left out.
            Parsed parsed(parse(element(Id::TrackEntry,
                                        uintEl(Id::TrackNumber, 1)
                                      + uintEl(Id::TrackType, TrackType::Audio)
                                      + strEl(Id::CodecID, "A_FLAC"))));
            const TrackEntry* t = parsed.parser().preferredAudioTrack();
            ASSERT_NOT_NULL(t, "A track was chosen");
            ASSERT_TRUE(t->sampling_frequency == 8000.0, "SamplingFrequency defaults to 8000");
            ASSERT_TRUE(t->channels == 1, "Channels defaults to 1");

            const StreamInfo info = parsed.parser().toStreamInfo(*t);
            ASSERT_EQUALS(8000u, info.sample_rate,
                          "a rate of 0 disables seek trimming, stamps every chunk 0, "
                          "and gets the track refused by Audio::setup");
            ASSERT_TRUE(info.channels == 1, "and one channel, not zero");
        }
        {   // An Audio element that states only the rate still takes the channel
            // default, and the stated rate still wins.
            Parsed parsed(parse(element(Id::TrackEntry,
                                        uintEl(Id::TrackNumber, 1)
                                      + uintEl(Id::TrackType, TrackType::Audio)
                                      + strEl(Id::CodecID, "A_FLAC")
                                      + element(Id::Audio,
                                                floatEl(Id::SamplingFrequency, 44100.0)))));
            const StreamInfo info =
                parsed.parser().toStreamInfo(*parsed.parser().preferredAudioTrack());
            ASSERT_EQUALS(44100u, info.sample_rate, "an explicit rate wins over the default");
            ASSERT_TRUE(info.channels == 1, "Channels still defaults");
        }
    }
};

class CodecTagTest : public TestCase {
public:
    CodecTagTest() : TestCase("Codec tags route PCM float and FLAC to the right decoder") {}

protected:
    void runTest() override
    {
        auto tagFor = [](const char* codec_id, uint64_t bit_depth) {
            const std::vector<uint8_t> track =
                element(Id::TrackEntry, uintEl(Id::TrackNumber, 1)
                                      + uintEl(Id::TrackType, TrackType::Audio)
                                      + strEl(Id::CodecID, codec_id)
                                      + element(Id::Audio,
                                                floatEl(Id::SamplingFrequency, 48000.0)
                                              + uintEl(Id::Channels, 2)
                                              + uintEl(Id::BitDepth, bit_depth)));
            Parsed parsed(ebmlHeader("matroska")
                          + element(Id::Segment,
                                    element(Id::Info, uintEl(Id::TimestampScale, 1000000))
                                  + element(Id::Tracks, track)));
            return parsed.parser().toStreamInfo(*parsed.parser().preferredAudioTrack());
        };

        // Float and integer PCM both state BitDepth 32, so the CodecID is the
        // only signal and PCMCodec keys its float path off this tag. Without it
        // every float bit pattern is read as int32 -- +1.0f becomes +1.065e9 --
        // and the track is full-scale noise.
        ASSERT_EQUALS(uint32_t{0x0003}, tagFor("A_PCM/FLOAT/IEEE", 32).codec_tag,
                      "A_PCM/FLOAT/IEEE carries WAVE_FORMAT_IEEE_FLOAT");
        ASSERT_EQUALS(uint32_t{0}, tagFor("A_PCM/INT/LIT", 16).codec_tag,
                      "integer PCM needs no tag");
        // codec_name "flac" with codec_tag 0 is what AudioCodecFactory sends to
        // the Ogg passthrough; Matroska blocks are bare frames and want the
        // native decoder instead.
        ASSERT_EQUALS(uint32_t{0x43614C66}, tagFor("A_FLAC", 16).codec_tag,
                      "A_FLAC is tagged like the native FLAC demuxer's streams");
    }
};

class OpusDelayTest : public TestCase {
public:
    OpusDelayTest() : TestCase("CodecDelay is left to the Opus decoder when OpusHead is usable") {}

protected:
    void runTest() override
    {
        auto delayFor = [](const std::vector<uint8_t>& codec_private) {
            const std::vector<uint8_t> track =
                element(Id::TrackEntry, uintEl(Id::TrackNumber, 1)
                                      + uintEl(Id::TrackType, TrackType::Audio)
                                      + strEl(Id::CodecID, "A_OPUS")
                                      + element(Id::CodecPrivate, codec_private)
                                      + uintEl(Id::CodecDelay, 6500000)
                                      + element(Id::Audio,
                                                floatEl(Id::SamplingFrequency, 48000.0)
                                              + uintEl(Id::Channels, 2)));
            Parsed parsed(ebmlHeader("matroska")
                          + element(Id::Segment,
                                    element(Id::Info, uintEl(Id::TimestampScale, 1000000))
                                  + element(Id::Tracks, track)));
            return parsed.parser().toStreamInfo(*parsed.parser().preferredAudioTrack()).encoder_delay;
        };

        // A real OpusHead, as ffmpeg writes it: magic, version 1, two channels,
        // pre_skip 312 little-endian, 48 kHz. OpusCodec seeds its own skip
        // counter from this, so trimming encoder_delay as well removed 624
        // frames of a track that owed 312.
        const std::vector<uint8_t> head{'O','p','u','s','H','e','a','d',
                                        1, 2, 0x38, 0x01, 0x80, 0xBB, 0x00, 0x00,
                                        0x00, 0x00, 0x00};
        ASSERT_EQUALS(uint32_t{0}, delayFor(head),
                      "the decoder applies its own pre-skip, so the container's is dropped");

        // Too short for OpusCodec to parse, so it will not skip anything. The
        // container's value has to survive or the priming is never trimmed.
        const std::vector<uint8_t> stub{'O','p','u','s','H','e','a','d'};
        ASSERT_EQUALS(uint32_t{312}, delayFor(stub),
                      "an unusable header falls back to CodecDelay in frames");

        // Full-length headers that OpusHeader::isValid() still rejects. The
        // decoder skips nothing for them either, so the container must trim.
        std::vector<uint8_t> no_channels = head;
        no_channels[9] = 0;
        ASSERT_EQUALS(uint32_t{312}, delayFor(no_channels),
                      "a zero channel count is rejected by the decoder, so CodecDelay applies");

        std::vector<uint8_t> family2 = head;
        family2[18] = 2;
        ASSERT_EQUALS(uint32_t{312}, delayFor(family2),
                      "mapping family 2 is rejected by the decoder, so CodecDelay applies");

        // Families 1 and 255 are ones the decoder accepts, so it trims them.
        for (uint8_t family : {uint8_t{1}, uint8_t{255}}) {
            std::vector<uint8_t> accepted = head;
            accepted[18] = family;
            ASSERT_EQUALS(uint32_t{0}, delayFor(accepted),
                          "an accepted mapping family leaves the trim to the decoder");
        }
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

class OpusRateTest : public TestCase {
public:
    OpusRateTest() : TestCase("An Opus track plays at 48 kHz whatever its SamplingFrequency says") {}

protected:
    void runTest() override
    {
        // draft-ietf-cellar-codec §3.4.32: SamplingFrequency is OpusHead's
        // Input Sample Rate, the rate the encoder was fed, and says nothing
        // about the decoder's output, which is always 48 kHz.
        const std::vector<uint8_t> track =
            element(Id::TrackEntry, uintEl(Id::TrackNumber, 1)
                                  + uintEl(Id::TrackType, TrackType::Audio)
                                  + strEl(Id::CodecID, "A_OPUS")
                                  + element(Id::CodecPrivate,
                                            std::vector<uint8_t>{'O','p','u','s','H','e','a','d',
                                                                 1, 2, 0x38, 0x01,
                                                                 0x44, 0xAC, 0x00, 0x00,  // 44100
                                                                 0x00, 0x00, 0x00})
                                  + uintEl(Id::CodecDelay, 6500000)
                                  + element(Id::Audio,
                                            floatEl(Id::SamplingFrequency, 44100.0)
                                          + uintEl(Id::Channels, 2)));
        Parsed parsed(ebmlHeader("matroska")
                      + element(Id::Segment,
                                element(Id::Info, uintEl(Id::TimestampScale, 1000000)
                                                + floatEl(Id::Duration, 1000.0))
                              + element(Id::Tracks, track)));
        const StreamInfo info =
            parsed.parser().toStreamInfo(*parsed.parser().preferredAudioTrack());
        ASSERT_EQUALS(uint32_t{48000}, info.sample_rate,
                      "the playback rate is the decoder's 48 kHz, not the input rate");
        ASSERT_EQUALS(uint64_t{48000}, info.duration_samples,
                      "one second counts 48000 frames, not 44100");
    }
};

class CodecDelayCeilingTest : public TestCase {
public:
    CodecDelayCeilingTest() : TestCase("An implausible CodecDelay is ignored, not trimmed") {}

protected:
    void runTest() override
    {
        // opusFile() carries a stub OpusHead the decoder cannot use, so the
        // container's CodecDelay is what gets applied, at 48 kHz.
        auto delayFor = [](uint64_t codec_delay_ns) {
            Parsed parsed(opusFile(1000000, 2008.0, codec_delay_ns));
            return parsed.parser().toStreamInfo(*parsed.parser().preferredAudioTrack())
                       .encoder_delay;
        };

        ASSERT_EQUALS(uint32_t{480000}, delayFor(10ULL * 1000000000ULL),
                      "10 s is the largest delay still honoured");
        ASSERT_EQUALS(uint32_t{0}, delayFor(10ULL * 1000000000ULL + 1),
                      "anything past 10 s is ignored");

        // An hour of "priming" converts cleanly to 172,800,000 frames, and
        // DemuxedStream would discard every one of them: the track plays as
        // silence.
        ASSERT_EQUALS(uint32_t{0}, delayFor(3600ULL * 1000000000ULL),
                      "an hour of delay would silence the track, so it is ignored");

        // The largest value wraps the nanoseconds-times-rate product and then
        // truncates to uint32_t, leaving an arbitrary frame count.
        ASSERT_EQUALS(uint32_t{0}, delayFor(UINT64_MAX),
                      "a delay that wraps the conversion is ignored");
    }
};

class HostileFloatTest : public TestCase {
public:
    HostileFloatTest() : TestCase("Non-finite and out-of-range floats never reach an integer cast") {}

protected:
    void runTest() override
    {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double inf = std::numeric_limits<double>::infinity();

        // SamplingFrequency and Duration are raw IEEE doubles, and converting a
        // NaN, an infinity or an out-of-range value to an integer is undefined.
        // On x86-64 some of these happen to come out as 0 and others do not,
        // so every case is pinned.
        auto streamFor = [](double sampling, bool with_output, double output) {
            std::vector<uint8_t> audio = floatEl(Id::SamplingFrequency, sampling)
                                       + uintEl(Id::Channels, 2);
            if (with_output) {
                audio = audio + floatEl(Id::OutputSamplingFrequency, output);
            }
            const std::vector<uint8_t> track =
                element(Id::TrackEntry, uintEl(Id::TrackNumber, 1)
                                      + uintEl(Id::TrackType, TrackType::Audio)
                                      + strEl(Id::CodecID, "A_PCM/INT/LIT")
                                      + element(Id::Audio, audio));
            Parsed parsed(ebmlHeader("matroska")
                          + element(Id::Segment,
                                    element(Id::Info, uintEl(Id::TimestampScale, 1000000))
                                  + element(Id::Tracks, track)));
            return parsed.parser().toStreamInfo(*parsed.parser().preferredAudioTrack());
        };
        auto rateFor = [&](double sampling) {
            return streamFor(sampling, false, 0.0).sample_rate;
        };

        ASSERT_EQUALS(uint32_t{0}, rateFor(nan), "a NaN rate is unknown");
        ASSERT_EQUALS(uint32_t{0}, rateFor(inf), "an infinite rate is unknown");
        ASSERT_EQUALS(uint32_t{0}, rateFor(-1000.0), "a negative rate is unknown");
        ASSERT_EQUALS(uint32_t{0}, rateFor(1e10), "a rate past uint32_t is unknown");
        ASSERT_EQUALS(uint32_t{0}, rateFor(0.25), "a rate below 1 Hz is unknown");
        ASSERT_EQUALS(uint32_t{1048575}, rateFor(1048575.0), "FLAC's ceiling is still believed");
        ASSERT_EQUALS(uint32_t{0}, rateFor(1048576.0), "one past it is not");

        // A broken OutputSamplingFrequency must not hide a good coded rate.
        ASSERT_EQUALS(uint32_t{44100}, streamFor(44100.0, true, inf).sample_rate,
                      "an infinite output rate falls back to SamplingFrequency");
        ASSERT_EQUALS(uint32_t{44100}, streamFor(44100.0, true, nan).sample_rate,
                      "a NaN output rate falls back to SamplingFrequency");

        auto durationFor = [](double ticks) {
            Parsed parsed(opusFile(1000000, ticks));
            return parsed.parser().info().durationMs();
        };
        ASSERT_EQUALS(uint64_t{0}, durationFor(nan), "a NaN Duration is unknown");
        ASSERT_EQUALS(uint64_t{0}, durationFor(inf), "an infinite Duration is unknown");
        ASSERT_EQUALS(uint64_t{0}, durationFor(-5.0), "a negative Duration is unknown");
        ASSERT_EQUALS(uint64_t{0}, durationFor(1e30), "a Duration past uint64_t is unknown");

        {   // 1e15 ms fits in uint64_t, so the file's claim is reported as it
            // stands, but at 48 kHz the frame count would wrap -- to a non-zero
            // value, unlike a power of two, whose product is a multiple of 2^64.
            // Unknown beats a wrapped number that looks real. 1e15 and 1e21 are
            // both exact doubles, so the tick arithmetic adds no rounding.
            Parsed parsed(opusFile(1000000, 1e15));
            const StreamInfo info =
                parsed.parser().toStreamInfo(*parsed.parser().preferredAudioTrack());
            ASSERT_EQUALS(uint64_t{1000000000000000ULL}, info.duration_ms,
                          "a representable Duration is kept");
            ASSERT_EQUALS(uint64_t{0}, info.duration_samples,
                          "a frame count that would wrap is left unknown");
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
    suite.addTest(std::make_unique<SchemaDefaultsTest>());
    suite.addTest(std::make_unique<CodecTagTest>());
    suite.addTest(std::make_unique<OpusDelayTest>());
    suite.addTest(std::make_unique<StreamInfoMappingTest>());
    suite.addTest(std::make_unique<HostileFloatTest>());
    suite.addTest(std::make_unique<CodecDelayCeilingTest>());
    suite.addTest(std::make_unique<OpusRateTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
