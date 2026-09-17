/*
 * test_mp2_codec.cpp - MP2Codec channel handling
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"

using namespace TestFramework;
using namespace PsyMP3::Codec::MP2;

namespace {

// One MPEG-1 Layer II frame: 48 kHz, 32 kbit/s, single channel (header mode
// 3), a 1 kHz tone. Encoded by ffmpeg (as an oracle only); the second frame
// of the stream, so the tone is already present.
const uint8_t kMonoFrame[96] = {
    0xFF, 0xFD, 0x14, 0xC4, 0x53, 0x68, 0x22, 0x2A, 0xA9, 0xD3, 0x3A, 0xEB, 0x7D, 0x3C, 0x26, 0xD2,
    0x6C, 0xF7, 0x8F, 0x92, 0xF0, 0x9B, 0x49, 0xB3, 0xDE, 0x3E, 0x4B, 0xC2, 0x6D, 0x26, 0xCF, 0x78,
    0xF9, 0x2F, 0x09, 0xB4, 0x9B, 0x3D, 0xE3, 0xE4, 0xBC, 0x26, 0xD2, 0x6C, 0xF7, 0x8F, 0x92, 0xF0,
    0x9B, 0x49, 0xB3, 0xDE, 0x3E, 0x4B, 0xC2, 0x6D, 0x26, 0xCF, 0x78, 0xF9, 0x2F, 0x09, 0xB4, 0x9B,
    0x3D, 0xE3, 0xE4, 0xBC, 0x26, 0xD2, 0x6C, 0xF7, 0x8F, 0x92, 0xF0, 0x9B, 0x49, 0xB3, 0xDE, 0x3E,
    0x4B, 0xC2, 0x6D, 0x26, 0xCF, 0x78, 0xF9, 0x2F, 0x09, 0xB4, 0x9B, 0x3D, 0xE3, 0xE4, 0x80, 0x00,
};

StreamInfo mp2Stream(uint16_t channels)
{
    StreamInfo info;
    info.codec_type = "audio";
    info.codec_name = "mp2";
    info.sample_rate = 48000;
    info.channels = channels;
    return info;
}

AudioFrame decodeFrame(MP2Codec& codec)
{
    MediaChunk chunk;
    chunk.data.assign(kMonoFrame, kMonoFrame + sizeof(kMonoFrame));
    return codec.decode(chunk);
}

class MonoStreamTest : public TestCase {
public:
    MonoStreamTest() : TestCase("A mono stream decodes to mono, one sample per frame") {}

protected:
    void runTest() override
    {
        MP2Codec codec(mp2Stream(1));
        ASSERT_TRUE(codec.initialize(), "the codec initialises");
        ASSERT_EQUALS(uint16_t{1}, codec.getStreamInfo().channels,
                      "the device is opened for the stream's one channel");

        const AudioFrame frame = decodeFrame(codec);
        // kjmp2 duplicates mono into two channels. Handing that pair to a
        // mono device played every sample twice: half speed, an octave down.
        ASSERT_EQUALS(uint16_t{1}, frame.channels, "the frame is labelled mono");
        ASSERT_EQUALS(size_t{1152}, frame.samples.size(), "1152 samples, not 2304");

        AudioSample peak = 0;
        for (AudioSample s : frame.samples) {
            peak = std::max(peak, static_cast<AudioSample>(std::abs(static_cast<int64_t>(s))));
        }
        ASSERT_TRUE(peak > (1 << 24), "the tone is there, at full-scale S32");
    }
};

class StereoOutputTest : public TestCase {
public:
    StereoOutputTest() : TestCase("Anything but a mono stream is decoded and reported as stereo") {}

protected:
    void runTest() override
    {
        for (uint16_t declared : {uint16_t{0}, uint16_t{2}}) {
            MP2Codec codec(mp2Stream(declared));
            ASSERT_TRUE(codec.initialize(), "the codec initialises");
            ASSERT_EQUALS(uint16_t{2}, codec.getStreamInfo().channels,
                          "kjmp2's two channels are what the device must expect");
            const AudioFrame frame = decodeFrame(codec);
            ASSERT_EQUALS(uint16_t{2}, frame.channels, "the frame is stereo");
            ASSERT_EQUALS(size_t{2304}, frame.samples.size(), "1152 stereo frames");
            ASSERT_TRUE(frame.samples[1000] == frame.samples[1001],
                        "a mono source comes out as the same sample in both channels");
        }
    }
};

} // namespace

int main()
{
    TestSuite suite("MP2 Codec Tests");
    suite.addTest(std::make_unique<MonoStreamTest>());
    suite.addTest(std::make_unique<StereoOutputTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return static_cast<int>(results.size()) - suite.getPassedCount(results);
}
