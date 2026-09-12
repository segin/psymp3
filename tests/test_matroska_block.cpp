/*
 * test_matroska_block.cpp - Matroska block headers and the four lacing modes
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"

using namespace TestFramework;
using namespace PsyMP3::Demuxer::Matroska;

namespace {

std::vector<uint8_t> operator+(std::vector<uint8_t> a, const std::vector<uint8_t>& b)
{
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

/// Block header bytes: a one-byte track number, a signed 16-bit timestamp and
/// the flags. Lacing lives in bits 1-2 of the flags.
std::vector<uint8_t> blockHeader(uint8_t track, int16_t timestamp, Lacing lacing,
                                 bool keyframe = true)
{
    const uint16_t raw = static_cast<uint16_t>(timestamp);
    uint8_t flags = static_cast<uint8_t>(static_cast<int>(lacing) << 1);
    if (keyframe) {
        flags |= 0x80;
    }
    return {static_cast<uint8_t>(0x80 | track),
            static_cast<uint8_t>(raw >> 8), static_cast<uint8_t>(raw & 0xFF),
            flags};
}

/// Payload of `length` bytes, each holding `fill`, so a frame's identity is
/// visible in its contents and a misplaced boundary is obvious.
std::vector<uint8_t> filled(size_t length, uint8_t fill)
{
    return std::vector<uint8_t>(length, fill);
}

/// Xiph codes a size as a run of 0xFF bytes plus a terminator below 0xFF.
std::vector<uint8_t> xiphSize(size_t size)
{
    std::vector<uint8_t> out;
    while (size >= 255) {
        out.push_back(0xFF);
        size -= 255;
    }
    out.push_back(static_cast<uint8_t>(size));
    return out;
}

/// A one-byte unsigned VINT.
std::vector<uint8_t> vint1(uint8_t value) { return {static_cast<uint8_t>(0x80 | value)}; }

/// A one-byte *signed* VINT, biased by 63.
std::vector<uint8_t> svint1(int value)
{
    return {static_cast<uint8_t>(0x80 | static_cast<uint8_t>(value + 63))};
}

class NoLacingTest : public TestCase {
public:
    NoLacingTest() : TestCase("An unlaced block is one frame and the header reads back") {}

protected:
    void runTest() override
    {
        const std::vector<uint8_t> block = blockHeader(1, 1234, Lacing::None) + filled(64, 0xAA);
        BlockHeader header;
        std::vector<BlockFrame> frames;
        ASSERT_TRUE(BlockParser::parse(block.data(), block.size(), header, frames), "parses");

        ASSERT_TRUE(header.track_number == 1, "TrackNumber");
        ASSERT_TRUE(header.timestamp_offset == 1234, "Timestamp");
        ASSERT_TRUE(header.lacing == Lacing::None, "No lacing");
        ASSERT_TRUE(header.keyframe, "Keyframe flag");
        ASSERT_EQUALS(size_t{1}, frames.size(), "One frame");
        ASSERT_EQUALS(size_t{64}, frames[0].size, "The frame is the rest of the block");
        ASSERT_TRUE(frames[0].data[0] == 0xAA, "and points at the payload");
    }
};

class NegativeTimestampTest : public TestCase {
public:
    NegativeTimestampTest() : TestCase("A block timestamp is signed") {}

protected:
    void runTest() override
    {
        // A frame may sit before the cluster that stores it. Read unsigned,
        // -1 becomes 65535, putting the frame about a minute into the future
        // at a millisecond tick -- which shows up as a seek landing nowhere
        // near where it was asked to, not as a parse error.
        const std::vector<uint8_t> block = blockHeader(1, -1, Lacing::None) + filled(8, 0x11);
        BlockHeader header;
        std::vector<BlockFrame> frames;
        ASSERT_TRUE(BlockParser::parse(block.data(), block.size(), header, frames), "parses");
        ASSERT_TRUE(header.timestamp_offset == -1,
                    "0xFFFF is -1, not 65535");

        const std::vector<uint8_t> far = blockHeader(1, -32768, Lacing::None) + filled(8, 0x11);
        ASSERT_TRUE(BlockParser::parse(far.data(), far.size(), header, frames), "parses");
        ASSERT_TRUE(header.timestamp_offset == -32768, "The most negative offset");
    }
};

class XiphLacingTest : public TestCase {
public:
    XiphLacingTest() : TestCase("Xiph lacing sizes are 255-runs, and the last frame is implicit") {}

protected:
    void runTest() override
    {
        // Three frames of 300, 100 and 50 bytes. Only the first two sizes are
        // stored; the last is whatever is left, which is the whole reason a
        // stated size that overruns the block has to be caught.
        const std::vector<uint8_t> block =
            blockHeader(2, 10, Lacing::Xiph)
            + std::vector<uint8_t>{0x02}            // three frames: count - 1
            + xiphSize(300) + xiphSize(100)
            + filled(300, 0xA1) + filled(100, 0xB2) + filled(50, 0xC3);

        BlockHeader header;
        std::vector<BlockFrame> frames;
        ASSERT_TRUE(BlockParser::parse(block.data(), block.size(), header, frames), "parses");
        ASSERT_TRUE(header.lacing == Lacing::Xiph, "Xiph lacing");
        ASSERT_EQUALS(size_t{3}, frames.size(), "Three frames");
        ASSERT_EQUALS(size_t{300}, frames[0].size, "300 spans two size bytes (0xFF 0x2D)");
        ASSERT_EQUALS(size_t{100}, frames[1].size, "100 is a single byte");
        ASSERT_EQUALS(size_t{50}, frames[2].size, "The last frame is the remainder");
        ASSERT_TRUE(frames[0].data[0] == 0xA1 && frames[1].data[0] == 0xB2
                        && frames[2].data[0] == 0xC3,
                    "Each frame points at its own bytes");
    }
};

class FixedLacingTest : public TestCase {
public:
    FixedLacingTest() : TestCase("Fixed lacing stores no sizes and must divide evenly") {}

protected:
    void runTest() override
    {
        const std::vector<uint8_t> block =
            blockHeader(1, 0, Lacing::Fixed)
            + std::vector<uint8_t>{0x03}   // four frames
            + filled(40, 0xD4);

        BlockHeader header;
        std::vector<BlockFrame> frames;
        ASSERT_TRUE(BlockParser::parse(block.data(), block.size(), header, frames), "parses");
        ASSERT_EQUALS(size_t{4}, frames.size(), "Four frames");
        for (const BlockFrame& frame : frames) {
            ASSERT_EQUALS(size_t{10}, frame.size, "40 bytes over four frames is 10 each");
        }

        // A remainder means the block is not what it says it is. Splitting it
        // anyway would hand a truncated frame to the decoder.
        const std::vector<uint8_t> uneven =
            blockHeader(1, 0, Lacing::Fixed) + std::vector<uint8_t>{0x02} + filled(40, 0xD4);
        ASSERT_FALSE(BlockParser::parse(uneven.data(), uneven.size(), header, frames),
                     "40 bytes do not divide into three frames, so the block is refused");
    }
};

class EBMLLacingTest : public TestCase {
public:
    EBMLLacingTest() : TestCase("EBML lacing sizes are signed deltas from the one before") {}

protected:
    void runTest() override
    {
        // Four frames of 100, 120, 118 and 62 bytes. The first size is stored
        // outright; the rest are deltas -- +20, then -2 -- and the last is the
        // remainder. A negative delta is the case that a plain unsigned read
        // gets wrong.
        const std::vector<uint8_t> block =
            blockHeader(1, 5, Lacing::EBML)
            + std::vector<uint8_t>{0x03}   // four frames
            + vint1(100) + svint1(20) + svint1(-2)
            + filled(100, 0x01) + filled(120, 0x02) + filled(118, 0x03) + filled(62, 0x04);

        BlockHeader header;
        std::vector<BlockFrame> frames;
        ASSERT_TRUE(BlockParser::parse(block.data(), block.size(), header, frames), "parses");
        ASSERT_TRUE(header.lacing == Lacing::EBML, "EBML lacing");
        ASSERT_EQUALS(size_t{4}, frames.size(), "Four frames");
        ASSERT_EQUALS(size_t{100}, frames[0].size, "The first size is stored outright");
        ASSERT_EQUALS(size_t{120}, frames[1].size, "A delta of +20");
        ASSERT_EQUALS(size_t{118}, frames[2].size,
                      "A delta of -2, which reading the VINT unsigned would make 8189");
        ASSERT_EQUALS(size_t{62}, frames[3].size, "The last frame is the remainder");
        ASSERT_TRUE(frames[0].data[0] == 0x01 && frames[1].data[0] == 0x02
                        && frames[2].data[0] == 0x03 && frames[3].data[0] == 0x04,
                    "Each frame lands on its own bytes");
    }
};

class SignedVIntBiasTest : public TestCase {
public:
    SignedVIntBiasTest() : TestCase("The lacing delta bias follows the VINT's width") {}

protected:
    void runTest() override
    {
        // 2^(7n-1) - 1: 63 at one byte, 8191 at two. Using one width's bias for
        // another leaves the first frame right and every later one wrong, which
        // reads as a codec fault rather than a container one.
        int64_t value = 0;
        const uint8_t one_zero[] = {0xBF};              // 63 - 63
        ASSERT_EQUALS(size_t{1}, BlockParser::decodeSignedVInt(one_zero, 1, value), "one byte");
        ASSERT_TRUE(value == 0, "0xBF is a delta of zero");

        const uint8_t one_neg[] = {0x80};               // 0 - 63
        BlockParser::decodeSignedVInt(one_neg, 1, value);
        ASSERT_TRUE(value == -63, "The most negative one-byte delta");

        const uint8_t one_pos[] = {0xFE};               // 126 - 63
        BlockParser::decodeSignedVInt(one_pos, 1, value);
        ASSERT_TRUE(value == 63, "The most positive one-byte delta");

        const uint8_t two_zero[] = {0x5F, 0xFF};        // 8191 - 8191
        ASSERT_EQUALS(size_t{2}, BlockParser::decodeSignedVInt(two_zero, 2, value), "two bytes");
        ASSERT_TRUE(value == 0, "A two-byte delta of zero uses a bias of 8191, not 63");

        const uint8_t two_neg[] = {0x40, 0x00};         // 0 - 8191
        BlockParser::decodeSignedVInt(two_neg, 2, value);
        ASSERT_TRUE(value == -8191, "The most negative two-byte delta");
    }
};

class MalformedBlockTest : public TestCase {
public:
    MalformedBlockTest() : TestCase("A block whose sizes do not fit its payload is refused") {}

protected:
    void runTest() override
    {
        BlockHeader header;
        std::vector<BlockFrame> frames;

        // Sizes that overrun the block. Nothing in the format cross-checks
        // them, so a truncated file produces this and a hostile one can aim it.
        const std::vector<uint8_t> overrun =
            blockHeader(1, 0, Lacing::Xiph) + std::vector<uint8_t>{0x01}
            + xiphSize(1000) + filled(10, 0xEE);
        ASSERT_FALSE(BlockParser::parse(overrun.data(), overrun.size(), header, frames),
                     "A stated size larger than the payload is refused");
        ASSERT_TRUE(frames.empty(), "and no frame is left pointing outside the buffer");

        // Header cut short: a track number and nothing else.
        const std::vector<uint8_t> truncated{0x81};
        ASSERT_FALSE(BlockParser::parse(truncated.data(), truncated.size(), header, frames),
                     "A block too short for its header is refused");

        // Laced, but the frame count byte is missing.
        const std::vector<uint8_t> no_count = blockHeader(1, 0, Lacing::Xiph);
        ASSERT_FALSE(BlockParser::parse(no_count.data(), no_count.size(), header, frames),
                     "A laced block with no frame count is refused");

        // Xiph sizes that stop mid-run: 0xFF promises more to come.
        const std::vector<uint8_t> dangling =
            blockHeader(1, 0, Lacing::Xiph) + std::vector<uint8_t>{0x01, 0xFF};
        ASSERT_FALSE(BlockParser::parse(dangling.data(), dangling.size(), header, frames),
                     "A Xiph size run that never terminates is refused");

        ASSERT_FALSE(BlockParser::parse(nullptr, 0, header, frames), "A null block is refused");
    }
};

class LacingFlagsTest : public TestCase {
public:
    LacingFlagsTest() : TestCase("The lacing mode comes from bits 1 and 2 of the flags") {}

protected:
    void runTest() override
    {
        // Fixed is 0b10 and EBML is 0b11, which is the pair most easily
        // swapped: reading them the other way round splits a fixed block by
        // deltas that are not there.
        struct { Lacing lacing; uint8_t bits; } cases[] = {
            {Lacing::None,  0x00}, {Lacing::Xiph, 0x02},
            {Lacing::Fixed, 0x04}, {Lacing::EBML, 0x06},
        };
        for (const auto& item : cases) {
            std::vector<uint8_t> block = {0x81, 0x00, 0x00,
                                          static_cast<uint8_t>(0x80 | item.bits)};
            if (item.lacing != Lacing::None) {
                block.push_back(0x00);          // one frame
                if (item.lacing == Lacing::Xiph || item.lacing == Lacing::EBML) {
                    // one frame means no sizes are stored at all
                }
            }
            const std::vector<uint8_t> full = block + filled(16, 0x77);
            BlockHeader header;
            std::vector<BlockFrame> frames;
            ASSERT_TRUE(BlockParser::parse(full.data(), full.size(), header, frames),
                        "A single-frame block parses in every lacing mode");
            ASSERT_TRUE(header.lacing == item.lacing, "The lacing mode is read correctly");
            ASSERT_EQUALS(size_t{1}, frames.size(), "One frame");
            ASSERT_EQUALS(size_t{16}, frames[0].size, "which is the whole remainder");
        }
    }
};

} // namespace

int main()
{
    TestSuite suite("Matroska Block Parsing Tests");
    suite.addTest(std::make_unique<NoLacingTest>());
    suite.addTest(std::make_unique<NegativeTimestampTest>());
    suite.addTest(std::make_unique<XiphLacingTest>());
    suite.addTest(std::make_unique<FixedLacingTest>());
    suite.addTest(std::make_unique<EBMLLacingTest>());
    suite.addTest(std::make_unique<SignedVIntBiasTest>());
    suite.addTest(std::make_unique<MalformedBlockTest>());
    suite.addTest(std::make_unique<LacingFlagsTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
