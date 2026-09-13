/*
 * test_eac3_frame.cpp - E-AC-3 bsi() and audfrm(), A/52 Annex E Tables E1.2/E1.3
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"

using namespace TestFramework;
using namespace PsyMP3::Codec::AC3;

namespace {

/// Writes bits most significant first, so a test can spell a frame out field
/// by field in the order A/52 prints them.
class BitWriter {
public:
    void put(uint32_t value, unsigned bits)
    {
        while (bits-- > 0) {
            const uint32_t bit = (value >> bits) & 1u;
            if (m_count % 8 == 0) {
                m_data.push_back(0);
            }
            if (bit) {
                m_data.back() |= static_cast<uint8_t>(0x80 >> (m_count % 8));
            }
            ++m_count;
        }
    }
    size_t bits() const { return m_count; }
    std::vector<uint8_t> finish(size_t bytes)
    {
        while (m_data.size() < bytes) {
            m_data.push_back(0);
        }
        return m_data;
    }

private:
    std::vector<uint8_t> m_data;
    size_t m_count = 0;
};

constexpr uint16_t kFrmsiz = 199;         // 200 words, 400 bytes: room to spare
constexpr size_t kFrameBytes = (kFrmsiz + 1) * 2;

/// What a test varies. Everything else is written as the smallest legal
/// choice: no metadata, no compression word, no optional syntax.
struct Spec {
    uint8_t strmtyp = 0;
    uint8_t numblkscod = 3;               // six blocks
    uint8_t acmod = 2;
    bool lfeon = false;
    bool chanmape = false;
    uint16_t chanmap = 0;
    bool expstre = false;                 // only written for six-block frames
    bool ahte = false;
    uint8_t snroffststr = 0;
    std::vector<bool> cplinu;             // one per block when acmod > 1; strategy sent each block
    uint8_t frmcplexpstr = 0;
    std::vector<uint8_t> frmchexpstr;     // one per channel, when !expstre
    std::vector<std::vector<uint8_t>> chexpstr; // [blk][ch], when expstre
    std::vector<uint8_t> cplexpstr;       // [blk], when expstre and coupled
    bool convexpstre = false;             // only written when numblkscod != 3
    std::vector<bool> ahtflags;           // written in order, when ahte
    uint8_t frmcsnroffst = 0;
    uint8_t frmfsnroffst = 0;
    bool blkstrtinfoe = false;
};

unsigned channelsFor(uint8_t acmod)
{
    static constexpr uint8_t kChannels[8] = { 2, 1, 2, 3, 3, 4, 4, 5 };
    return kChannels[acmod];
}

/// Build bsi() and audfrm() per Tables E1.2 and E1.3, and report the bit at
/// which the first audio block would begin.
std::vector<uint8_t> buildFrame(const Spec& s, size_t& audblk_bit)
{
    static constexpr uint8_t kBlocks[4] = { 1, 2, 3, 6 };
    const unsigned blocks = kBlocks[s.numblkscod];
    const unsigned nfchans = channelsFor(s.acmod);

    BitWriter w;
    w.put(kSyncWord, 16);
    // --- bsi ---
    w.put(s.strmtyp, 2);
    w.put(0, 3);                          // substreamid
    w.put(kFrmsiz, 11);
    w.put(0, 2);                          // fscod: 48 kHz
    w.put(s.numblkscod, 2);
    w.put(s.acmod, 3);
    w.put(s.lfeon ? 1 : 0, 1);
    w.put(16, 5);                         // bsid
    w.put(31, 5);                         // dialnorm
    w.put(0, 1);                          // compre
    if (s.acmod == 0) {
        w.put(31, 5);                     // dialnorm2
        w.put(0, 1);                      // compr2e
    }
    if (s.strmtyp == 1) {
        w.put(s.chanmape ? 1 : 0, 1);
        if (s.chanmape) {
            w.put(s.chanmap, 16);
        }
    }
    w.put(0, 1);                          // mixmdate
    w.put(0, 1);                          // infomdate
    if (s.strmtyp == 0 && s.numblkscod != 3) {
        w.put(0, 1);                      // convsync
    }
    w.put(0, 1);                          // addbsie

    // --- audfrm ---
    if (s.numblkscod == 3) {
        w.put(s.expstre ? 1 : 0, 1);
        w.put(s.ahte ? 1 : 0, 1);
    }
    w.put(s.snroffststr, 2);
    w.put(0, 1);                          // transproce
    w.put(0, 1);                          // blkswe
    w.put(0, 1);                          // dithflage
    w.put(0, 1);                          // bamode
    w.put(0, 1);                          // frmfgaincode
    w.put(0, 1);                          // dbaflde
    w.put(0, 1);                          // skipflde
    w.put(0, 1);                          // spxattene
    unsigned ncplblks = 0;
    if (s.acmod > 1) {
        w.put(s.cplinu[0] ? 1 : 0, 1);
        for (unsigned blk = 1; blk < blocks; ++blk) {
            w.put(1, 1);                  // cplstre: a fresh strategy every block
            w.put(s.cplinu[blk] ? 1 : 0, 1);
        }
        for (unsigned blk = 0; blk < blocks; ++blk) {
            ncplblks += s.cplinu[blk] ? 1 : 0;
        }
    }
    const bool expstre = s.numblkscod == 3 ? s.expstre : true;
    if (expstre) {
        for (unsigned blk = 0; blk < blocks; ++blk) {
            if (s.acmod > 1 && s.cplinu[blk]) {
                w.put(s.cplexpstr[blk], 2);
            }
            for (unsigned ch = 0; ch < nfchans; ++ch) {
                w.put(s.chexpstr[blk][ch], 2);
            }
        }
    } else {
        if (s.acmod > 1 && ncplblks > 0) {
            w.put(s.frmcplexpstr, 5);
        }
        for (unsigned ch = 0; ch < nfchans; ++ch) {
            w.put(s.frmchexpstr[ch], 5);
        }
    }
    if (s.lfeon) {
        for (unsigned blk = 0; blk < blocks; ++blk) {
            w.put(blk == 0 ? 1 : 0, 1);   // lfeexpstr
        }
    }
    if (s.strmtyp == 0) {
        if (s.numblkscod != 3) {
            w.put(s.convexpstre ? 1 : 0, 1);
        }
        if (s.numblkscod == 3 || s.convexpstre) {
            for (unsigned ch = 0; ch < nfchans; ++ch) {
                w.put(0, 5);              // convexpstr
            }
        }
    }
    if (s.ahte) {
        for (bool flag : s.ahtflags) {
            w.put(flag ? 1 : 0, 1);
        }
    }
    if (s.snroffststr == 0) {
        w.put(s.frmcsnroffst, 6);
        w.put(s.frmfsnroffst, 4);
    }
    if (s.numblkscod != 0) {
        w.put(s.blkstrtinfoe ? 1 : 0, 1);
        if (s.blkstrtinfoe) {
            // (blocks - 1) * (4 + ceil(log2(frmsiz + 1))): 200 words -> 8 bits.
            w.put(0, (blocks - 1) * (4 + 8));
        }
    }
    audblk_bit = w.bits();
    return w.finish(kFrameBytes);
}

bool parse(const std::vector<uint8_t>& bytes, AC3FrameHeader& header,
           EAC3AudioFrame& frame, size_t& end_bit, std::string& why)
{
    AC3BitReader reader(bytes.data(), bytes.size());
    const char* reason = nullptr;
    const bool ok = eac3ParseFrame(reader, header, frame, &reason);
    end_bit = reader.tell();
    why = reason ? reason : "";
    return ok;
}

/// ffmpeg's shape: six blocks, frame exponent strategies, frame SNR offsets.
/// The strategies for every block must come out of Table E2.10.
class FrameStrategyTest : public TestCase {
public:
    FrameStrategyTest() : TestCase("Frame exponent strategies resolve through Table E2.10") {}

protected:
    void runTest() override
    {
        Spec s;
        s.acmod = 2;
        s.cplinu = { true, true, true, true, true, true };
        s.frmcplexpstr = 5;               // D25 R R D25 R D45
        s.frmchexpstr = { 16, 31 };       // D45 D15 R R R R / all D45
        s.frmcsnroffst = 33;
        s.frmfsnroffst = 9;

        size_t expected_bit = 0;
        const auto bytes = buildFrame(s, expected_bit);
        AC3FrameHeader header; EAC3AudioFrame frame; size_t end_bit = 0; std::string why;
        ASSERT_TRUE(parse(bytes, header, frame, end_bit, why), "parses: " + why);
        ASSERT_TRUE(end_bit == expected_bit, "stops exactly where the first audio block begins ("
                    + std::to_string(end_bit) + " vs " + std::to_string(expected_bit) + ")");

        ASSERT_TRUE(frame.blocks == 6 && !frame.expstre && !frame.ahte, "six blocks, table strategies");
        ASSERT_TRUE(frame.ncplblks == 6, "coupled in every block");
        for (unsigned blk = 0; blk < 6; ++blk) {
            ASSERT_TRUE(frame.cplexpstr[blk] == kFrameExponentStrategies[5][blk], "coupling row 5");
            ASSERT_TRUE(frame.chexpstr[blk][0] == kFrameExponentStrategies[16][blk], "channel 0 row 16");
            ASSERT_TRUE(frame.chexpstr[blk][1] == kFrameExponentStrategies[31][blk], "channel 1 row 31");
        }
        ASSERT_TRUE(frame.chexpstr[1][0] == ExponentStrategy::D15, "row 16 sends D15 in block 1");
        ASSERT_TRUE(frame.frmcsnroffst == 33 && frame.frmfsnroffst == 9, "frame SNR offsets");
    }
};

/// A frame of fewer than six blocks has no expstre or ahte bit: strategies are
/// per block, and the converter and block-start syntax appear instead.
class ShortFrameTest : public TestCase {
public:
    ShortFrameTest() : TestCase("Short frames read per-block strategies and block start info") {}

protected:
    void runTest() override
    {
        for (uint8_t code : { uint8_t{0}, uint8_t{1}, uint8_t{2} }) {
            const unsigned blocks = code + 1u;
            Spec s;
            s.numblkscod = code;
            s.acmod = 1;                  // mono: no coupling syntax at all
            s.lfeon = true;
            s.chexpstr.assign(blocks, std::vector<uint8_t>{ 1 });
            s.chexpstr[0][0] = 2;         // D25 in block 0, D15 after
            s.convexpstre = code == 1;
            s.blkstrtinfoe = code != 0;

            size_t expected_bit = 0;
            const auto bytes = buildFrame(s, expected_bit);
            AC3FrameHeader header; EAC3AudioFrame frame; size_t end_bit = 0; std::string why;
            ASSERT_TRUE(parse(bytes, header, frame, end_bit, why), "parses: " + why);
            ASSERT_TRUE(frame.blocks == blocks, "block count follows numblkscod");
            ASSERT_TRUE(frame.expstre && !frame.ahte, "implied: per-block strategies, no AHT");
            ASSERT_TRUE(frame.chexpstr[0][0] == ExponentStrategy::D25, "block 0 strategy");
            for (unsigned blk = 1; blk < blocks; ++blk) {
                ASSERT_TRUE(frame.chexpstr[blk][0] == ExponentStrategy::D15, "later strategy");
            }
            ASSERT_TRUE(frame.lfeexpstr[0] == ExponentStrategy::D15, "LFE exponents in block 0");
            ASSERT_TRUE(end_bit == expected_bit, "lands on the first audio block with "
                        + std::to_string(blocks) + " block(s)");
        }
    }
};

/// §E3.4.2: a channel's AHT flag exists only if its exponents are sent once
/// in the frame. Getting that count wrong reads one bit too many or too few
/// and shifts everything after it.
class AhtFlagTest : public TestCase {
public:
    AhtFlagTest() : TestCase("AHT flags are present only for channels with one exponent set") {}

protected:
    void runTest() override
    {
        Spec s;
        s.acmod = 3;                      // 3/0: three channels, coupling syntax present
        s.expstre = true;
        s.ahte = true;
        s.cplinu = { false, false, false, false, false, false };
        s.chexpstr.assign(6, std::vector<uint8_t>{ 0, 0, 0 });
        s.chexpstr[0] = { 1, 1, 1 };      // every channel sends in block 0 ...
        s.chexpstr[3][1] = 3;             // ... channel 1 again in block 3
        // Channel 0 and 2 have one set each, so they carry a flag; channel 1
        // has two and does not. No coupling, so no coupling flag either.
        s.ahtflags = { true, false };

        size_t expected_bit = 0;
        const auto bytes = buildFrame(s, expected_bit);
        AC3FrameHeader header; EAC3AudioFrame frame; size_t end_bit = 0; std::string why;
        ASSERT_TRUE(parse(bytes, header, frame, end_bit, why), "parses: " + why);
        ASSERT_TRUE(frame.chahtinu[0], "channel 0 reads its flag");
        ASSERT_TRUE(!frame.chahtinu[1], "channel 1 has no flag to read");
        ASSERT_TRUE(!frame.chahtinu[2], "channel 2 reads its flag, which is clear");
        ASSERT_TRUE(!frame.cplahtinu, "no coupling, no coupling flag");
        ASSERT_TRUE(end_bit == expected_bit, "exactly two AHT bits consumed");
    }
};

/// Coupling strategy is decided for the frame, and a block that sends no new
/// strategy keeps the previous block's.
class CouplingStrategyTest : public TestCase {
public:
    CouplingStrategyTest() : TestCase("Per-block coupling strategy is gathered into the frame") {}

protected:
    void runTest() override
    {
        Spec s;
        s.acmod = 7;                      // 3/2
        s.lfeon = true;
        s.expstre = true;
        s.cplinu = { true, true, false, false, true, true };
        s.chexpstr.assign(6, std::vector<uint8_t>(5, 0));
        s.chexpstr[0].assign(5, 1);
        s.cplexpstr = { 1, 0, 0, 0, 2, 0 };

        size_t expected_bit = 0;
        const auto bytes = buildFrame(s, expected_bit);
        AC3FrameHeader header; EAC3AudioFrame frame; size_t end_bit = 0; std::string why;
        ASSERT_TRUE(parse(bytes, header, frame, end_bit, why), "parses: " + why);
        const bool expected[6] = { true, true, false, false, true, true };
        for (unsigned blk = 0; blk < 6; ++blk) {
            ASSERT_TRUE(frame.cplinu[blk] == expected[blk], "cplinu for block " + std::to_string(blk));
            ASSERT_TRUE(frame.cplstre[blk], "every block sent a strategy");
        }
        ASSERT_TRUE(frame.ncplblks == 4, "four coupled blocks");
        ASSERT_TRUE(frame.cplexpstr[0] == ExponentStrategy::D15 &&
                    frame.cplexpstr[4] == ExponentStrategy::D25, "coupling strategies where coupled");
        ASSERT_TRUE(end_bit == expected_bit, "lands on the first audio block");
    }
};

/// A dependent substream may carry a custom channel map, which sits between
/// the compression word and the mixing metadata.
class DependentSubstreamTest : public TestCase {
public:
    DependentSubstreamTest() : TestCase("A dependent substream's channel map is read") {}

protected:
    void runTest() override
    {
        Spec s;
        s.strmtyp = 1;
        s.acmod = 2;
        s.chanmape = true;
        s.chanmap = 0xA5C3;
        s.cplinu = { false, false, false, false, false, false };
        s.frmchexpstr = { 0, 0 };

        size_t expected_bit = 0;
        const auto bytes = buildFrame(s, expected_bit);
        AC3FrameHeader header; EAC3AudioFrame frame; size_t end_bit = 0; std::string why;
        ASSERT_TRUE(parse(bytes, header, frame, end_bit, why), "parses: " + why);
        ASSERT_TRUE(header.strmtyp == 1, "dependent substream");
        ASSERT_TRUE(frame.chanmape && frame.chanmap == 0xA5C3, "channel map");
        ASSERT_TRUE(end_bit == expected_bit, "no converter syntax in a dependent substream");
    }
};

/// The two Annex E tables the block parser leans on.
class TableShapeTest : public TestCase {
public:
    TableShapeTest() : TestCase("Tables E2.10 and E2.12 keep their printed shape") {}

protected:
    void runTest() override
    {
        for (unsigned row = 0; row < 32; ++row) {
            ASSERT_TRUE(kFrameExponentStrategies[row][0] != ExponentStrategy::Reuse,
                        "a frame always opens with exponents");
            for (unsigned other = row + 1; other < 32; ++other) {
                bool same = true;
                for (unsigned blk = 0; blk < 6; ++blk) {
                    same = same && kFrameExponentStrategies[row][blk] == kFrameExponentStrategies[other][blk];
                }
                ASSERT_TRUE(!same, "no two rows alike");
            }
        }
        // Table E2.12: sub-bands 8, 10, 11 and 13..17 merge into the band before.
        const uint8_t expected[18] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1 };
        for (unsigned i = 0; i < 18; ++i) {
            ASSERT_TRUE(kDefaultCouplingBandStructure[i] == expected[i],
                        "default banding at sub-band " + std::to_string(i));
        }
    }
};

} // namespace

int main()
{
    TestSuite suite("E-AC-3 Frame Tests");
    suite.addTest(std::make_unique<TableShapeTest>());
    suite.addTest(std::make_unique<FrameStrategyTest>());
    suite.addTest(std::make_unique<ShortFrameTest>());
    suite.addTest(std::make_unique<AhtFlagTest>());
    suite.addTest(std::make_unique<CouplingStrategyTest>());
    suite.addTest(std::make_unique<DependentSubstreamTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
