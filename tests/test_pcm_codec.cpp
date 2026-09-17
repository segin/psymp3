/*
 * test_pcm_codec.cpp - PCMCodec's sample conversions
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"

using namespace TestFramework;

namespace {

class FloatRangeTest : public TestCase {
public:
    FloatRangeTest() : TestCase("Float samples are clamped, and NaN is silence") {}

protected:
    void runTest() override
    {
        StreamInfo info;
        info.codec_type = "audio";
        info.codec_name = "pcm";
        info.codec_tag = 0x0003; // WAVE_FORMAT_IEEE_FLOAT
        info.sample_rate = 48000;
        info.channels = 1;
        info.bits_per_sample = 32;
        PCMCodec codec(info);
        ASSERT_TRUE(codec.initialize(), "the codec takes 32-bit float");

        MediaChunk chunk;
        chunk.stream_id = 1;
        chunk.data = {
            0x00, 0x00, 0xF8, 0x7F, // quiet NaN
            0x00, 0x00, 0x00, 0x40, // +2.0
            0x00, 0x00, 0x80, 0xFF, // -infinity
            0x00, 0x00, 0x00, 0x3F, // +0.5
        };
        const AudioFrame frame = codec.decode(chunk);
        ASSERT_EQUALS(size_t{4}, frame.samples.size(), "four samples");
        ASSERT_EQUALS(AudioSample{0}, frame.samples[0], "NaN plays as silence");
        ASSERT_EQUALS(AudioSample{2147483520}, frame.samples[1], "+2.0 is clamped to full scale");
        ASSERT_EQUALS(AudioSample{-2147483520}, frame.samples[2], "-infinity likewise");
        ASSERT_EQUALS(AudioSample{1073741760}, frame.samples[3], "+0.5 is half scale");
    }
};

} // namespace

int main()
{
    TestSuite suite("PCM Codec");
    suite.addTest(std::make_unique<FloatRangeTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return static_cast<int>(results.size()) - suite.getPassedCount(results);
}
