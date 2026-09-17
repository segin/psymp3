/*
 * test_iso_seek.cpp - Seeks in MP4 files land where the audio is
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * The files are written here, box by box after ISO/IEC 14496-12 and -14,
 * around AAC made with FDK's encoder and FLAC frames built after RFC 9639.
 */

#include "psymp3.h"
#include "test_framework.h"
#include "io/MemoryIOHandler.h"
#include "fdk_aac_test_encoder.h"
#include "flac_test_stream.h"

#include <cstdlib>
#include <cstring>

using namespace TestFramework;
using namespace PsyMP3;
using namespace PsyMP3::Demuxer;
using PsyMP3::IO::MemoryIOHandler;

namespace {

using Bytes = std::vector<uint8_t>;

constexpr int kToneStartMs = 700;
constexpr int kTotalMs = 1500;

Bytes operator+(Bytes a, const Bytes& b)
{
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

Bytes be16(uint32_t value)
{
    return {static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
}

Bytes be32(uint32_t value)
{
    return {static_cast<uint8_t>(value >> 24), static_cast<uint8_t>(value >> 16),
            static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
}

Bytes text(const char* s)
{
    return Bytes(s, s + std::strlen(s));
}

Bytes box(const char* type, const Bytes& payload)
{
    return be32(static_cast<uint32_t>(8 + payload.size())) + text(type) + payload;
}

Bytes fullBox(const char* type, uint32_t flags, const Bytes& payload)
{
    return box(type, be32(flags & 0xFFFFFF) + payload);
}

/// An MPEG-4 descriptor, its length written in four bytes as muxers do.
Bytes descriptor(uint8_t tag, const Bytes& body)
{
    return Bytes{tag, 0x80, 0x80, 0x80, static_cast<uint8_t>(body.size())} + body;
}

Bytes unityMatrix()
{
    return be32(0x00010000) + be32(0) + be32(0)
         + be32(0) + be32(0x00010000) + be32(0)
         + be32(0) + be32(0) + be32(0x40000000);
}

/// An audio sample entry of type @p type, with @p extension boxes after it.
Bytes audioEntry(const char* type, uint32_t rate, uint16_t channels, const Bytes& extension)
{
    return box(type, Bytes(6, 0) + be16(1)              // data reference index
                   + be16(0) + be16(0) + be32(0)        // version, revision, vendor
                   + be16(channels) + be16(16)
                   + be16(0) + be16(0)                  // compression id, packet size
                   + be32(rate << 16) + extension);
}

/// A one-track MP4 holding @p samples, each @p delta long at @p timescale,
/// in one chunk.
Bytes mp4(const Bytes& sample_entry, uint32_t timescale, uint32_t delta,
          const std::vector<Bytes>& samples)
{
    const auto count = static_cast<uint32_t>(samples.size());
    const uint32_t media_duration = count * delta;
    const uint32_t movie_duration = static_cast<uint32_t>(uint64_t{media_duration} * 1000 / timescale);

    Bytes sizes;
    Bytes data;
    for (const Bytes& sample : samples) {
        sizes = sizes + be32(static_cast<uint32_t>(sample.size()));
        data = data + sample;
    }

    auto moov = [&](uint32_t data_offset) {
        const Bytes stbl = box("stbl",
            fullBox("stsd", 0, be32(1) + sample_entry)
          + fullBox("stts", 0, be32(1) + be32(count) + be32(delta))
          + fullBox("stsc", 0, be32(1) + be32(1) + be32(count) + be32(1))
          + fullBox("stsz", 0, be32(0) + be32(count) + sizes)
          + fullBox("stco", 0, be32(1) + be32(data_offset)));
        const Bytes minf = box("minf",
            fullBox("smhd", 0, be16(0) + be16(0))
          + box("dinf", fullBox("dref", 0, be32(1) + fullBox("url ", 1, {})))
          + stbl);
        const Bytes mdia = box("mdia",
            fullBox("mdhd", 0, be32(0) + be32(0) + be32(timescale) + be32(media_duration)
                                   + be16(0x55C4) + be16(0))
          + fullBox("hdlr", 0, be32(0) + text("soun") + be32(0) + be32(0) + be32(0)
                                   + text("SoundHandler") + Bytes{0})
          + minf);
        const Bytes tkhd = fullBox("tkhd", 7,
            be32(0) + be32(0) + be32(1) + be32(0) + be32(movie_duration)
          + be32(0) + be32(0) + be16(0) + be16(0) + be16(0x0100) + be16(0)
          + unityMatrix() + be32(0) + be32(0));
        const Bytes mvhd = fullBox("mvhd", 0,
            be32(0) + be32(0) + be32(1000) + be32(movie_duration)
          + be32(0x00010000) + be16(0x0100) + be16(0) + be32(0) + be32(0)
          + unityMatrix() + Bytes(24, 0) + be32(2));
        return box("moov", mvhd + box("trak", tkhd + mdia));
    };

    const Bytes ftyp = box("ftyp", text("M4A ") + be32(0) + text("M4A ") + text("mp42") + text("isom"));
    const size_t moov_size = moov(0).size();
    const auto data_offset = static_cast<uint32_t>(ftyp.size() + moov_size + 8);
    return ftyp + moov(data_offset) + box("mdat", data);
}

/// An M4A holding @p encoded at @p core_rate, the rate its access units are
/// coded at: that is the sample entry's rate and the media timescale, and
/// each access unit lasts 1024 of it.
Bytes m4a(const FdkTestEncoder::Encoded& encoded, uint32_t core_rate, uint16_t channels)
{
    const Bytes decoder_config =
        descriptor(0x04, Bytes{0x40, 0x15, 0x00, 0x00, 0x00} + be32(0) + be32(0)
                             + descriptor(0x05, encoded.asc));
    const Bytes esds = fullBox("esds", 0,
                               descriptor(0x03, be16(1) + Bytes{0x00} + decoder_config
                                                    + descriptor(0x06, Bytes{0x02})));
    return mp4(audioEntry("mp4a", core_rate, channels, esds), core_rate, 1024, encoded.units);
}

/// Reads @p frames frames and returns the first channel.
std::vector<AudioSample> readFirstChannel(DemuxedStream& stream, size_t frames)
{
    const size_t channels = stream.getChannels();
    std::vector<AudioSample> buffer(frames * channels);
    std::vector<AudioSample> first;
    size_t filled = 0;
    for (int attempt = 0; attempt < 64 && filled < buffer.size() && !stream.eof(); ++attempt) {
        filled += stream.getData((buffer.size() - filled) * sizeof(AudioSample),
                                 buffer.data() + filled) / sizeof(AudioSample);
    }
    for (size_t i = 0; i + channels <= filled; i += channels) {
        first.push_back(buffer[i]);
    }
    return first;
}

/// Index of the first sample louder than a tenth of full scale, or -1.
long firstLoud(const std::vector<AudioSample>& samples)
{
    for (size_t i = 0; i < samples.size(); ++i) {
        if (std::abs(static_cast<double>(samples[i])) > 0.1 * 2147483647.0) {
            return static_cast<long>(i);
        }
    }
    return -1;
}

class AacSeekTest : public TestCase {
public:
    AacSeekTest() : TestCase("A seek in an M4A finds the audio where playing from the start does") {}

protected:
    void runTest() override
    {
        struct Case {
            const char* what;
            int aot;
            uint32_t core_rate;
        };
        for (const Case& c : {Case{"AAC-LC: ", 2, 44100}, Case{"HE-AAC: ", 5, 22050}}) {
            const std::string what = c.what;
            const FdkTestEncoder::Encoded encoded = FdkTestEncoder::encode(c.aot, 2, kToneStartMs, kTotalMs);
            ASSERT_TRUE(!encoded.asc.empty() && encoded.units.size() > 30, what + "the encoder ran");
            const Bytes file = m4a(encoded, c.core_rate, 2);

            DemuxedStream stream(std::make_unique<MemoryIOHandler>(file.data(), file.size()),
                                 TagLib::String("tone.m4a"));
            ASSERT_EQUALS(44100u, stream.getRate(), what + "the rate is the decoder's");

            const std::vector<AudioSample> whole = readFirstChannel(stream, 2 * FdkTestEncoder::kInputRate);
            const long onset = firstLoud(whole);
            ASSERT_TRUE(onset > FdkTestEncoder::kInputRate * kToneStartMs / 1000,
                        what + "the tone is found after its start");

            // Each access unit holds 1024 core samples, so every tenth of a
            // millisecond or so lands somewhere different within one. The
            // sample table gives the landing, and the stream trims to the
            // target from there.
            for (long before_ms : {250L, 243L, 237L}) {
                const auto target_ms = static_cast<unsigned long>(onset * 1000 / FdkTestEncoder::kInputRate - before_ms);
                stream.seekTo(target_ms);
                const std::vector<AudioSample> after = readFirstChannel(stream, FdkTestEncoder::kInputRate / 2);
                const long seek_onset = firstLoud(after);
                ASSERT_TRUE(seek_onset > 0, what + "the audio after the seek starts quiet");
                const long found = static_cast<long>(target_ms * FdkTestEncoder::kInputRate / 1000) + seek_onset;
                ASSERT_TRUE(std::labs(found - onset) <= 1,
                            what + "a seek to " + std::to_string(target_ms) + " ms finds the tone at "
                            + std::to_string(found) + ", not " + std::to_string(onset));
            }
        }
    }
};

class FlacTest : public TestCase {
public:
    FlacTest() : TestCase("FLAC in MP4 plays from its first frame, and seeks to the sample") {}

protected:
    void runTest() override
    {
        constexpr uint32_t kFrames = 20;
        std::vector<Bytes> frames;
        for (uint32_t i = 0; i < kFrames; ++i) {
            frames.push_back(FlacTestStream::frame(i));
        }
        const Bytes entry = audioEntry("fLaC", 44100, 2,
                                       fullBox("dfLa", 0, FlacTestStream::streamInfo(kFrames * FlacTestStream::kBlock)));
        const Bytes file = mp4(entry, 44100, FlacTestStream::kBlock, frames);

        DemuxedStream stream(std::make_unique<MemoryIOHandler>(file.data(), file.size()),
                             TagLib::String("counter.m4a"));
        ASSERT_EQUALS(44100u, stream.getRate(), "opened");
        // Each sample is its counter at full scale.
        using FlacTestStream::counterAt;
        const std::vector<AudioSample> start = readFirstChannel(stream, 2 * FlacTestStream::kBlock);
        ASSERT_EQUALS(size_t{2 * FlacTestStream::kBlock}, start.size(), "audio plays");
        for (uint32_t i = 0; i < start.size(); ++i) {
            if ((start[i] >> 16) != counterAt(i)) {
                ASSERT_EQUALS(counterAt(i), start[i] >> 16,
                              "sample " + std::to_string(i) + " is the first frame's, in order");
            }
        }

        stream.seekTo(1000);
        const std::vector<AudioSample> after = readFirstChannel(stream, 16);
        ASSERT_EQUALS(counterAt(44100), after.front() >> 16, "a seek to 1 s plays sample 44100 first");
    }
};

} // namespace

int main()
{
    registerAllCodecs();
    registerAllDemuxers();

    TestSuite suite("MP4 seeking");
    suite.addTest(std::make_unique<AacSeekTest>());
    suite.addTest(std::make_unique<FlacTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return static_cast<int>(results.size()) - suite.getPassedCount(results);
}
