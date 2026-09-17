/*
 * test_aac_output_format.cpp - DemuxedStream takes AAC's output format from
 * the decoder, not from the container
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * The streams are made with FDK's encoder (fdk_aac_test_encoder.h).
 */

#include "psymp3.h"
#include "test_framework.h"
#include "io/MemoryIOHandler.h"
#include "matroska_ebml_builder.h"

#include "fdk_aac_test_encoder.h"

#include <cmath>
#include <cstdlib>

using namespace TestFramework;
using namespace PsyMP3;
using namespace PsyMP3::Demuxer;
using namespace PsyMP3::Demuxer::Matroska;
using namespace MatroskaBuilder;
using PsyMP3::IO::MemoryIOHandler;

namespace {

constexpr int kToneStartMs = 700;
constexpr int kTotalMs = 1500;

using FdkTestEncoder::Encoded;
using FdkTestEncoder::kInputRate;

Encoded encode(int aot, int channels)
{
    return FdkTestEncoder::encode(aot, channels, kToneStartMs, kTotalMs);
}

/// A Matroska file holding @p encoded one access unit per cluster, with the
/// Audio element ffmpeg writes for such a stream: the core rate as both
/// SamplingFrequency and OutputSamplingFrequency.
std::vector<uint8_t> matroska(const Encoded& encoded, uint64_t channels)
{
    constexpr double kCoreRate = kInputRate / 2.0;
    const std::vector<uint8_t> track =
        element(Id::TrackEntry, uintEl(Id::TrackNumber, 1)
                              + uintEl(Id::TrackType, TrackType::Audio)
                              + strEl(Id::CodecID, "A_AAC")
                              + element(Id::CodecPrivate, encoded.asc)
                              + element(Id::Audio, floatEl(Id::SamplingFrequency, kCoreRate)
                                                 + floatEl(Id::OutputSamplingFrequency, kCoreRate)
                                                 + uintEl(Id::Channels, channels)));
    std::vector<uint8_t> clusters;
    for (size_t i = 0; i < encoded.units.size(); ++i) {
        const auto ms = static_cast<uint64_t>(std::llround(i * 1024 * 1000.0 / kCoreRate));
        const std::vector<uint8_t> block = std::vector<uint8_t>{0x81, 0x00, 0x00, 0x80} + encoded.units[i];
        clusters = clusters + element(Id::Cluster, uintEl(Id::Timestamp, ms) + element(Id::SimpleBlock, block));
    }
    return ebmlHeader("matroska")
         + element(Id::Segment,
                   element(Id::Info, uintEl(Id::TimestampScale, 1000000)
                                   + floatEl(Id::Duration, static_cast<double>(kTotalMs)))
                 + element(Id::Tracks, track) + clusters);
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

class OutputFormatTest : public TestCase {
public:
    OutputFormatTest() : TestCase("HE-AAC in Matroska plays at the rate the decoder outputs") {}

protected:
    void runTest() override
    {
        for (int aot : {5, 29}) {
            const std::string what = aot == 5 ? "HE-AAC: " : "HE-AACv2: ";
            const Encoded encoded = encode(aot, 2);
            ASSERT_TRUE(!encoded.asc.empty() && encoded.units.size() > 30, what + "the encoder ran");
            // HE-AACv2 codes one channel and Parametric Stereo makes two.
            const std::vector<uint8_t> file = matroska(encoded, aot == 29 ? 1 : 2);

            DemuxedStream stream(std::make_unique<MemoryIOHandler>(file.data(), file.size()),
                                 TagLib::String("he.mka"));
            ASSERT_EQUALS(44100u, stream.getRate(), what + "the rate is the decoder's before anything is read");
            ASSERT_EQUALS(2u, stream.getChannels(), what + "and so is the channel count");
            ASSERT_EQUALS(static_cast<unsigned long long>(kInputRate) * kTotalMs / 1000,
                          stream.getSLength(), what + "the length counts output samples");

            // Where the tone starts, played from the top.
            const std::vector<AudioSample> whole = readFirstChannel(stream, 2 * kInputRate);
            const long onset = firstLoud(whole);
            ASSERT_TRUE(onset > kInputRate * kToneStartMs / 1000, what + "the tone is found after its start");

            // A seek to a quarter of a second before it finds it at the same
            // time. The demuxer lands in its own units, at the core rate; taken
            // as output samples, the landing put playback past the onset.
            const unsigned long target_ms = static_cast<unsigned long>(onset * 1000 / kInputRate) - 250;
            stream.seekTo(target_ms);
            const std::vector<AudioSample> after = readFirstChannel(stream, kInputRate / 2);
            const long seek_onset = firstLoud(after);
            ASSERT_TRUE(seek_onset > 0, what + "the audio after the seek starts quiet");
            // The decoder drops its first frame after a seek, and the stream
            // counts it. What is left is the block times' millisecond grain.
            const long found = static_cast<long>(target_ms * kInputRate / 1000) + seek_onset;
            ASSERT_TRUE(std::labs(found - onset) <= 64,
                        what + "the tone is at " + std::to_string(onset) + " after the seek too, not "
                        + std::to_string(found));
        }
    }
};

} // namespace

int main()
{
    registerAllCodecs();
    registerAllDemuxers();

    TestSuite suite("AAC output format");
    suite.addTest(std::make_unique<OutputFormatTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return static_cast<int>(results.size()) - suite.getPassedCount(results);
}
