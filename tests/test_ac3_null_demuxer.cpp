/*
 * test_ac3_null_demuxer.cpp - AC3NullDemuxer's frame walk
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * The demuxer reads syncframe headers and nothing else, so the frames here are
 * headers padded out to their stated length.
 */

#include "psymp3.h"
#include "test_framework.h"
#include "io/MemoryIOHandler.h"

using namespace TestFramework;
using PsyMP3::Demuxer::AC3::AC3NullDemuxer;
using PsyMP3::IO::MemoryIOHandler;

namespace {

class BitWriter {
public:
    void put(uint32_t value, unsigned bits)
    {
        for (unsigned i = bits; i-- > 0;) {
            if (m_count % 8 == 0) {
                m_data.push_back(0);
            }
            if ((value >> i) & 1) {
                m_data.back() |= static_cast<uint8_t>(0x80 >> (m_count % 8));
            }
            ++m_count;
        }
    }
    std::vector<uint8_t> finish(size_t size)
    {
        m_data.resize(size, 0);
        return m_data;
    }

private:
    std::vector<uint8_t> m_data;
    size_t m_count = 0;
};

constexpr size_t kFrameBytes = 128; // fscod 0 (48 kHz), frmsizecod 0 (32 kbit/s)

/// An AC-3 frame in 1+1 mode with every optional bsi field present, which is
/// the longest header A/52 Table 5.2 allows, 153 bits before addbsi.
std::vector<uint8_t> richDualMonoFrame()
{
    BitWriter w;
    w.put(0x0B77, 16);  // syncword
    w.put(0, 16);       // crc1
    w.put(0, 2);        // fscod
    w.put(0, 6);        // frmsizecod
    w.put(8, 5);        // bsid
    w.put(0, 3);        // bsmod
    w.put(0, 3);        // acmod: 1+1
    w.put(0, 1);        // lfeon
    w.put(27, 5);       // dialnorm
    w.put(1, 1); w.put(0x55, 8);            // compre, compr
    w.put(1, 1); w.put(0x09, 8);            // langcode, langcod
    w.put(1, 1); w.put(10, 5); w.put(1, 2); // audprodie, mixlevel, roomtyp
    w.put(27, 5);                           // dialnorm2
    w.put(1, 1); w.put(0x55, 8);            // compr2e, compr2
    w.put(1, 1); w.put(0x09, 8);            // langcod2e, langcod2
    w.put(1, 1); w.put(10, 5); w.put(1, 2); // audprodi2e, mixlevel2, roomtyp2
    w.put(1, 1);        // copyrightb
    w.put(1, 1);        // origbs
    w.put(1, 1); w.put(0x1234, 14);         // timecod1e, timecod1
    w.put(1, 1); w.put(0x0567, 14);         // timecod2e, timecod2
    w.put(1, 1); w.put(0, 6);               // addbsie, addbsil (one byte follows)
    return w.finish(kFrameBytes);
}

class RichDualMonoTest : public TestCase {
public:
    RichDualMonoTest() : TestCase("Dual-mono frames with every bsi field present are walked") {}

protected:
    void runTest() override
    {
        constexpr int kFrames = 3;
        std::vector<uint8_t> file;
        for (int i = 0; i < kFrames; ++i) {
            const std::vector<uint8_t> frame = richDualMonoFrame();
            file.insert(file.end(), frame.begin(), frame.end());
        }

        AC3NullDemuxer demuxer(std::make_unique<MemoryIOHandler>(file.data(), file.size()));
        ASSERT_TRUE(demuxer.parseContainer(), "the stream opens");
        ASSERT_EQUALS(uint64_t{kFrames * 1536}, demuxer.getStreams().at(0).duration_samples,
                      "every frame is counted");
        for (int i = 0; i < kFrames; ++i) {
            const MediaChunk chunk = demuxer.readChunk();
            ASSERT_EQUALS(kFrameBytes, chunk.data.size(), "frame " + std::to_string(i) + " is read");
        }
        ASSERT_TRUE(demuxer.readChunk().data.empty(), "and then the stream ends");
    }
};

/// An E-AC-3 frame of 128 bytes: just the header fields the demuxer reads.
std::vector<uint8_t> eac3Frame(uint8_t strmtyp, uint8_t substreamid, uint8_t acmod, bool lfeon)
{
    BitWriter w;
    w.put(0x0B77, 16);        // syncword
    w.put(strmtyp, 2);
    w.put(substreamid, 3);
    w.put(kFrameBytes / 2 - 1, 11); // frmsiz
    w.put(0, 2);              // fscod: 48 kHz
    w.put(3, 2);              // numblkscod: six blocks
    w.put(acmod, 3);
    w.put(lfeon ? 1 : 0, 1);
    w.put(16, 5);             // bsid
    w.put(27, 5);             // dialnorm
    return w.finish(kFrameBytes);
}

class ProgramLayoutTest : public TestCase {
public:
    ProgramLayoutTest() : TestCase("The layout comes from program 1, not from the frame the file opens on") {}

protected:
    void runTest() override
    {
        // 5.1 program audio, each frame followed by a dependent substream
        // carrying two more channels, and the file cut so that a dependent
        // frame comes first.
        std::vector<uint8_t> file;
        for (int i = 0; i < 3; ++i) {
            const std::vector<uint8_t> dependent = eac3Frame(1, 0, 2, false);
            const std::vector<uint8_t> independent = eac3Frame(0, 0, 7, true);
            file.insert(file.end(), dependent.begin(), dependent.end());
            file.insert(file.end(), independent.begin(), independent.end());
        }

        AC3NullDemuxer demuxer(std::make_unique<MemoryIOHandler>(file.data(), file.size()));
        ASSERT_TRUE(demuxer.parseContainer(), "the stream opens");
        const StreamInfo stream = demuxer.getStreams().at(0);
        ASSERT_EQUALS(std::string("eac3"), stream.codec_name, "as E-AC-3");
        ASSERT_EQUALS(uint16_t{6}, stream.channels, "with program 1's six channels");
        ASSERT_EQUALS(uint64_t{3 * 1536}, stream.duration_samples,
                      "and only program 1's frames count towards its length");
    }
};

/// A mono AC-3 frame with the shortest bsi, 128 bytes.
std::vector<uint8_t> plainFrame()
{
    BitWriter w;
    w.put(0x0B77, 16);  // syncword
    w.put(0, 16);       // crc1
    w.put(0, 2);        // fscod
    w.put(0, 6);        // frmsizecod
    w.put(8, 5);        // bsid
    w.put(0, 3);        // bsmod
    w.put(1, 3);        // acmod: 1/0
    w.put(0, 1);        // lfeon
    w.put(27, 5);       // dialnorm
    w.put(0, 8);        // compre, langcode, audprodie, copyrightb, origbs,
                        // timecod1e, timecod2e, addbsie
    return w.finish(kFrameBytes);
}

class DamageMidStreamTest : public TestCase {
public:
    DamageMidStreamTest() : TestCase("A damaged stretch mid-stream is stepped over, not taken for the end") {}

protected:
    void runTest() override
    {
        const std::vector<uint8_t> frame = plainFrame();
        std::vector<uint8_t> file;
        std::vector<uint64_t> offsets;
        for (int i = 0; i < 4; ++i) {
            if (i == 2) {
                file.insert(file.end(), 50, 0x00); // a lost stretch where a header should be
            }
            offsets.push_back(file.size());
            file.insert(file.end(), frame.begin(), frame.end());
        }

        AC3NullDemuxer demuxer(std::make_unique<MemoryIOHandler>(file.data(), file.size()));
        ASSERT_TRUE(demuxer.parseContainer(), "the stream opens");
        ASSERT_EQUALS(uint64_t{4 * 1536}, demuxer.getStreams().at(0).duration_samples,
                      "the frames after the damage count");
        for (int i = 0; i < 4; ++i) {
            const MediaChunk chunk = demuxer.readChunk();
            ASSERT_EQUALS(kFrameBytes, chunk.data.size(), "frame " + std::to_string(i) + " is read");
            ASSERT_EQUALS(offsets[static_cast<size_t>(i)], chunk.file_offset, "from where it starts");
            ASSERT_EQUALS(uint64_t{static_cast<uint64_t>(i) * 1536}, chunk.timestamp_samples,
                          "and the damage takes up no time");
        }
        ASSERT_TRUE(demuxer.readChunk().data.empty(), "and then the stream ends");
    }
};

} // namespace

int main()
{
    TestSuite suite("AC-3 Null Demuxer");
    suite.addTest(std::make_unique<RichDualMonoTest>());
    suite.addTest(std::make_unique<ProgramLayoutTest>());
    suite.addTest(std::make_unique<DamageMidStreamTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return static_cast<int>(results.size()) - suite.getPassedCount(results);
}
