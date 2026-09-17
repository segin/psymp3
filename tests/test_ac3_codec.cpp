/*
 * test_ac3_codec.cpp - AC3Codec's handling of syncframes split across chunks
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"

using namespace TestFramework;
using PsyMP3::Codec::AC3::AC3Codec;

namespace {

// Two consecutive syncframes of mono AC-3 at 48 kHz and 32 kbit/s, 128 bytes
// each, cut from ffmpeg's encoding of a 1 kHz tone. Their headers are 65 bits
// long, so a chunk ending eight bytes into a frame ends inside its header.
const uint8_t kTwoFrames[256] = {
    0x0B, 0x77, 0x4D, 0x3D, 0x00, 0x40, 0x2F, 0x84, 0x29, 0x03, 0xF7, 0x00, 0x34, 0x4F, 0x1F, 0xCB,
    0x97, 0x5C, 0xBD, 0x76, 0xA9, 0xF3, 0xF7, 0xCF, 0x9E, 0x88, 0xEA, 0xF9, 0xF3, 0xEA, 0xE9, 0x4C,
    0xBF, 0xA9, 0x10, 0x6D, 0x1F, 0xF0, 0x3E, 0x8A, 0x57, 0x6A, 0x29, 0x8E, 0x7F, 0x78, 0xEF, 0x8A,
    0x14, 0x01, 0xBB, 0x60, 0x3E, 0xFD, 0x26, 0x49, 0xD5, 0xD0, 0x1D, 0x12, 0xB3, 0xE2, 0x6B, 0x10,
    0x06, 0xD1, 0x01, 0x00, 0xA2, 0xE1, 0x6A, 0x08, 0x27, 0xA4, 0x1D, 0xD1, 0xBE, 0x58, 0x40, 0x1B,
    0x47, 0xFC, 0x0F, 0xA2, 0x95, 0xDA, 0x8A, 0x63, 0x9F, 0xDE, 0x3B, 0xE2, 0x85, 0x00, 0x6E, 0xD8,
    0x0F, 0xBF, 0x49, 0x92, 0x75, 0x74, 0x07, 0x44, 0xAC, 0xF8, 0x9A, 0xC4, 0x01, 0xB4, 0x40, 0x40,
    0x28, 0xB8, 0x5A, 0x82, 0x09, 0xE9, 0x07, 0x74, 0x6F, 0x96, 0x00, 0x00, 0x00, 0x00, 0x25, 0x67,
    0x0B, 0x77, 0x4D, 0x3D, 0x00, 0x40, 0x2F, 0x84, 0x29, 0x03, 0xF7, 0x00, 0x34, 0x4F, 0x1F, 0xCB,
    0x97, 0x5C, 0xBD, 0x76, 0xA9, 0xF3, 0xF7, 0xCF, 0x9E, 0x88, 0xEA, 0xF9, 0xF3, 0xEA, 0xE9, 0x4C,
    0xBF, 0xA9, 0x10, 0x6D, 0x1F, 0xF0, 0x3E, 0x8A, 0x57, 0x6A, 0x29, 0x8E, 0x7F, 0x78, 0xEF, 0x8A,
    0x14, 0x01, 0xBB, 0x60, 0x3E, 0xFD, 0x26, 0x49, 0xD5, 0xD0, 0x1D, 0x12, 0xB3, 0xE2, 0x6B, 0x10,
    0x06, 0xD1, 0x01, 0x00, 0xA2, 0xE1, 0x6A, 0x08, 0x27, 0xA4, 0x1D, 0xD1, 0xBE, 0x58, 0x40, 0x1B,
    0x47, 0xFC, 0x0F, 0xA2, 0x95, 0xDA, 0x8A, 0x63, 0x9F, 0xDE, 0x3B, 0xE2, 0x85, 0x00, 0x6E, 0xD8,
    0x0F, 0xBF, 0x49, 0x92, 0x75, 0x74, 0x07, 0x44, 0xAC, 0xF8, 0x9A, 0xC4, 0x01, 0xB4, 0x40, 0x40,
    0x28, 0xB8, 0x5A, 0x82, 0x09, 0xE9, 0x07, 0x74, 0x6F, 0x96, 0x00, 0x00, 0x00, 0x00, 0x25, 0x67,
};

constexpr size_t kFrameBytes = 128;

StreamInfo monoAc3()
{
    StreamInfo info;
    info.codec_type = "audio";
    info.codec_name = "ac3";
    info.sample_rate = 48000;
    info.channels = 1;
    return info;
}

/// Decodes @p chunks in order and flushes, returning every sample.
std::vector<AudioSample> decodeAll(const std::vector<std::vector<uint8_t>>& chunks)
{
    AC3Codec codec(monoAc3());
    if (!codec.initialize()) {
        return {};
    }
    std::vector<AudioSample> out;
    for (const std::vector<uint8_t>& data : chunks) {
        MediaChunk chunk;
        chunk.data = data;
        const AudioFrame frame = codec.decode(chunk);
        out.insert(out.end(), frame.samples.begin(), frame.samples.end());
    }
    const AudioFrame tail = codec.flush();
    out.insert(out.end(), tail.samples.begin(), tail.samples.end());
    return out;
}

class SplitHeaderTest : public TestCase {
public:
    SplitHeaderTest() : TestCase("A syncframe whose header is split across chunks is still decoded") {}

protected:
    void runTest() override
    {
        const std::vector<uint8_t> both(std::begin(kTwoFrames), std::end(kTwoFrames));
        const std::vector<AudioSample> whole = decodeAll({both});
        ASSERT_EQUALS(size_t{2 * 1536}, whole.size(), "both frames decode from one chunk");

        // Cut inside the second frame's header, and just after it.
        for (size_t cut : {kFrameBytes + 2, kFrameBytes + 8, kFrameBytes + 9, kFrameBytes + 20}) {
            const std::vector<uint8_t> first(both.begin(), both.begin() + static_cast<std::ptrdiff_t>(cut));
            const std::vector<uint8_t> second(both.begin() + static_cast<std::ptrdiff_t>(cut), both.end());
            const std::vector<AudioSample> split = decodeAll({first, second});
            ASSERT_EQUALS(whole.size(), split.size(),
                          "a cut " + std::to_string(cut - kFrameBytes) + " bytes into the second frame loses nothing");
            ASSERT_TRUE(split == whole, "and changes nothing");
        }
    }
};

class CrcTest : public TestCase {
public:
    CrcTest() : TestCase("The CRC check passes intact frames and catches any flipped bit") {}

protected:
    void runTest() override
    {
        for (size_t f = 0; f < 2; ++f) {
            std::vector<uint8_t> frame(kTwoFrames + f * kFrameBytes, kTwoFrames + (f + 1) * kFrameBytes);
            ASSERT_TRUE(PsyMP3::Codec::AC3::ac3FrameCrcValid(frame.data(), frame.size()),
                        "frame " + std::to_string(f) + " is intact");
            // Everything after the sync word is covered.
            for (size_t bit = 16; bit < 8 * kFrameBytes; ++bit) {
                frame[bit / 8] ^= static_cast<uint8_t>(0x80 >> (bit % 8));
                if (PsyMP3::Codec::AC3::ac3FrameCrcValid(frame.data(), frame.size())) {
                    ASSERT_TRUE(false, "a flip of bit " + std::to_string(bit) + " goes unnoticed");
                }
                frame[bit / 8] ^= static_cast<uint8_t>(0x80 >> (bit % 8));
            }
        }
    }
};

class DamagedFrameTest : public TestCase {
public:
    DamagedFrameTest() : TestCase("A damaged frame is muted in place, not decoded or dropped") {}

protected:
    void runTest() override
    {
        const std::vector<uint8_t> first(kTwoFrames, kTwoFrames + kFrameBytes);
        std::vector<uint8_t> second(kTwoFrames + kFrameBytes, kTwoFrames + 2 * kFrameBytes);
        const std::vector<AudioSample> whole = decodeAll({first, second});
        second[60] ^= 0x10;
        const std::vector<AudioSample> damaged = decodeAll({first, second});

        ASSERT_EQUALS(whole.size(), damaged.size(), "the damaged frame still takes its time");
        ASSERT_TRUE(std::equal(whole.begin(), whole.begin() + 1536, damaged.begin()),
                    "the frame before it is untouched");
        // Its first block carries the fade-out of the frame before; the other
        // five are silent.
        bool silent = true;
        bool was_silent = true;
        for (size_t i = 1536 + 256; i < damaged.size(); ++i) {
            silent = silent && damaged[i] == 0;
            was_silent = was_silent && whole[i] == 0;
        }
        ASSERT_TRUE(silent, "the damaged frame is muted");
        ASSERT_FALSE(was_silent, "where the intact frame had sound");
    }
};

} // namespace

int main()
{
    TestSuite suite("AC-3 Codec");
    suite.addTest(std::make_unique<SplitHeaderTest>());
    suite.addTest(std::make_unique<CrcTest>());
    suite.addTest(std::make_unique<DamagedFrameTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return static_cast<int>(results.size()) - suite.getPassedCount(results);
}
