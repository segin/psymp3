/*
 * test_ac3_exponents.cpp - AC-3 differential exponent decoding (A/52 §7.1)
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

/// Packs three differentials into the 7-bit word A/52 §7.1.3 describes:
/// gexp = 25*M1 + 5*M2 + M3, where each M is the delta biased by 2.
uint8_t packGroup(int d0, int d1, int d2)
{
    return static_cast<uint8_t>(25 * (d0 + 2) + 5 * (d1 + 2) + (d2 + 2));
}

/// A bit stream holding the given groups, most significant bit first.
std::vector<uint8_t> groupStream(const std::vector<uint8_t>& groups)
{
    std::vector<uint8_t> out;
    size_t bits = 0;
    for (uint8_t group : groups) {
        for (int i = 6; i >= 0; --i) {
            if (bits % 8 == 0) {
                out.push_back(0);
            }
            if ((group >> i) & 1) {
                out.back() |= static_cast<uint8_t>(0x80 >> (bits % 8));
            }
            ++bits;
        }
    }
    out.resize(out.size() + 4, 0); // slack so a read never runs short
    return out;
}

class GroupSizeTest : public TestCase {
public:
    GroupSizeTest() : TestCase("Each strategy spreads one differential over its group") {}

protected:
    void runTest() override
    {
        // A/52 Table 7.4.
        ASSERT_TRUE(ac3ExponentGroupSize(ExponentStrategy::D15) == 1, "D15: one per mantissa");
        ASSERT_TRUE(ac3ExponentGroupSize(ExponentStrategy::D25) == 2, "D25: one per pair");
        ASSERT_TRUE(ac3ExponentGroupSize(ExponentStrategy::D45) == 4, "D45: one per quad");
        ASSERT_TRUE(ac3ExponentGroupSize(ExponentStrategy::Reuse) == 0,
                    "Reuse codes nothing at all");
    }
};

class BinRangeTest : public TestCase {
public:
    BinRangeTest() : TestCase("Bandwidth and coupling codes give the bin ranges") {}

protected:
    void runTest() override
    {
        // A/52 §7.1.3: endmant = ((chbwcod + 12) * 3) + 37.
        ASSERT_TRUE(ac3ChannelEndMantissa(0) == 73, "chbwcod 0 ends at bin 73");
        // A/52 §5.4.3.24 caps the code at 60, and 60 lands on bin 253 -- which is
        // exactly the span the banding tables cover. A larger code is not a wider
        // channel, it is an invalid stream the decoder must mute on.
        ASSERT_TRUE(ac3ChannelEndMantissa(60) == kMaxEndMantissa, "chbwcod 60 ends at 253");
        ASSERT_TRUE(ac3ChannelEndMantissa(61) == 0, "61 is refused rather than extrapolated");
        ASSERT_TRUE(ac3ChannelEndMantissa(63) == 0, "as is the largest the field can hold");

        // cplstrtmant = cplbegf*12 + 37, cplendmant = (cplendf+3)*12 + 37.
        ASSERT_TRUE(ac3CouplingStartMantissa(0) == 37, "coupling starts at 37");
        ASSERT_TRUE(ac3CouplingStartMantissa(1) == 49, "and moves in twelves");
        ASSERT_TRUE(ac3CouplingEndMantissa(0) == 73, "cplendf 0 ends at 73");

        // The coupling range is always a multiple of twelve, which is why its
        // group count divides exactly where a channel's needs rounding.
        const unsigned span = ac3CouplingEndMantissa(2) - ac3CouplingStartMantissa(1);
        ASSERT_TRUE(span % 12 == 0, "the coupling span is a multiple of twelve");
    }
};

class GroupCountTest : public TestCase {
public:
    GroupCountTest() : TestCase("Group counts round up for the wider strategies") {}

protected:
    void runTest() override
    {
        // A/52 §7.1.3. The +3 and +9 exist so a channel whose bins do not
        // divide evenly still gets its final, partly used group; dropping them
        // loses the top of the spectrum.
        ASSERT_TRUE(ac3ChannelExponentGroups(ExponentStrategy::D15, 73) == 24, "D15 over 73 bins");
        ASSERT_TRUE(ac3ChannelExponentGroups(ExponentStrategy::D25, 73) == 12, "D25 over 73 bins");
        ASSERT_TRUE(ac3ChannelExponentGroups(ExponentStrategy::D45, 73) == 6, "D45 over 73 bins");

        // Every reachable end bin must be covered. endmant is always
        // ((chbwcod + 12) * 3) + 37, so it is one more than a multiple of
        // three -- 73, 76, 79 and so on -- and the rounding terms are sized for
        // exactly those. A bin count outside that sequence cannot occur, and
        // testing one would say nothing about the formula.
        for (uint8_t chbwcod = 0; chbwcod <= kMaxChannelBandwidthCode; ++chbwcod) {
            const unsigned end = ac3ChannelEndMantissa(chbwcod);
            for (auto strategy : {ExponentStrategy::D15, ExponentStrategy::D25,
                                  ExponentStrategy::D45}) {
                const unsigned groups = ac3ChannelExponentGroups(strategy, end);
                const unsigned covered = 1 + groups * 3 * ac3ExponentGroupSize(strategy);
                ASSERT_TRUE(covered >= end,
                            "the group count reaches the channel's last bin, which is "
                            "what the rounding terms in the formula are for");
            }
        }

        ASSERT_TRUE(ac3ChannelExponentGroups(ExponentStrategy::Reuse, 73) == 0,
                    "Reuse reads no groups");
        // The coupling channel divides exactly, so no rounding term.
        ASSERT_TRUE(ac3CouplingExponentGroups(ExponentStrategy::D15, 37, 73) == 12, "coupling D15");
        ASSERT_TRUE(ac3CouplingExponentGroups(ExponentStrategy::D45, 37, 73) == 3, "coupling D45");
    }
};

class DifferentialDecodeTest : public TestCase {
public:
    DifferentialDecodeTest() : TestCase("Exponents accumulate from bin 0 through the deltas") {}

protected:
    void runTest() override
    {
        // Two groups: deltas +1,+1,+1 then -1,0,+2, from an absolute of 5.
        // Running total: 6,7,8, then 7,7,9.
        auto stream = groupStream({packGroup(1, 1, 1), packGroup(-1, 0, 2)});
        AC3BitReader reader(stream.data(), stream.size());

        uint8_t exponents[64] = {0};
        const unsigned count = ac3DecodeExponents(reader, ExponentStrategy::D15, 2, 5,
                                                  exponents, 64);
        ASSERT_TRUE(count == 7, "bin 0 plus two groups of three");
        const uint8_t expected[7] = {5, 6, 7, 8, 7, 7, 9};
        for (unsigned i = 0; i < 7; ++i) {
            ASSERT_TRUE(exponents[i] == expected[i],
                        "each bin is the previous plus its differential");
        }
    }
};

class BiasTest : public TestCase {
public:
    BiasTest() : TestCase("A differential of zero is the mapped value two") {}

protected:
    void runTest() override
    {
        // The mapping is biased by 2 so deltas can be negative: the packed
        // values 0..4 stand for -2..+2. Reading them unbiased would make every
        // exponent climb, and since they accumulate the error compounds up the
        // spectrum rather than staying put.
        auto stream = groupStream({packGroup(0, 0, 0)});
        AC3BitReader reader(stream.data(), stream.size());
        uint8_t exponents[16] = {0};
        const unsigned count = ac3DecodeExponents(reader, ExponentStrategy::D15, 1, 10,
                                                  exponents, 16);
        ASSERT_TRUE(count == 4, "bin 0 plus three");
        for (unsigned i = 0; i < 4; ++i) {
            ASSERT_TRUE(exponents[i] == 10, "a zero differential leaves the exponent alone");
        }

        // The extremes of the range.
        auto extremes = groupStream({packGroup(2, -2, 2)});
        AC3BitReader reader2(extremes.data(), extremes.size());
        ac3DecodeExponents(reader2, ExponentStrategy::D15, 1, 10, exponents, 16);
        ASSERT_TRUE(exponents[1] == 12 && exponents[2] == 10 && exponents[3] == 12,
                    "+2, -2 and +2 again");
    }
};

class GroupExpansionTest : public TestCase {
public:
    GroupExpansionTest() : TestCase("D25 and D45 copy each exponent across its group") {}

protected:
    void runTest() override
    {
        // One differential per pair: the same value lands on both bins.
        auto stream = groupStream({packGroup(1, 1, 1)});
        AC3BitReader reader(stream.data(), stream.size());
        uint8_t exponents[32] = {0};
        unsigned count = ac3DecodeExponents(reader, ExponentStrategy::D25, 1, 4, exponents, 32);
        ASSERT_TRUE(count == 7, "bin 0 plus three differentials over pairs");
        const uint8_t pairs[7] = {4, 5, 5, 6, 6, 7, 7};
        for (unsigned i = 0; i < 7; ++i) {
            ASSERT_TRUE(exponents[i] == pairs[i], "each differential fills its pair");
        }

        // And across quads.
        auto quad_stream = groupStream({packGroup(1, 0, 0)});
        AC3BitReader quad_reader(quad_stream.data(), quad_stream.size());
        count = ac3DecodeExponents(quad_reader, ExponentStrategy::D45, 1, 4, exponents, 32);
        ASSERT_TRUE(count == 13, "bin 0 plus three differentials over quads");
        for (unsigned i = 1; i <= 4; ++i) {
            ASSERT_TRUE(exponents[i] == 5, "the first quad carries the +1");
        }
        for (unsigned i = 5; i <= 12; ++i) {
            ASSERT_TRUE(exponents[i] == 5, "and the rest hold at 5");
        }
    }
};

class ClampTest : public TestCase {
public:
    ClampTest() : TestCase("An exponent cannot be driven outside the coded range") {}

protected:
    void runTest() override
    {
        // Exponents accumulate deltas read from the file, so a corrupt group
        // can walk them anywhere. They index quantisation tables later, so the
        // range has to hold whatever the stream says.
        std::vector<uint8_t> climbing(12, packGroup(2, 2, 2));
        auto stream = groupStream(climbing);
        AC3BitReader reader(stream.data(), stream.size());
        uint8_t exponents[64] = {0};
        const unsigned count = ac3DecodeExponents(reader, ExponentStrategy::D15, 12, 20,
                                                  exponents, 64);
        ASSERT_TRUE(count > 0, "decoding still succeeds");
        for (unsigned i = 0; i < count; ++i) {
            ASSERT_TRUE(exponents[i] <= kMaxExponent, "no exponent exceeds 24");
        }

        std::vector<uint8_t> falling(12, packGroup(-2, -2, -2));
        auto down = groupStream(falling);
        AC3BitReader down_reader(down.data(), down.size());
        const unsigned down_count = ac3DecodeExponents(down_reader, ExponentStrategy::D15, 12, 4,
                                                       exponents, 64);
        for (unsigned i = 0; i < down_count; ++i) {
            ASSERT_TRUE(exponents[i] <= kMaxExponent, "and none wraps below zero");
        }
    }
};

class RefusalTest : public TestCase {
public:
    RefusalTest() : TestCase("Decoding refuses rather than overflowing its buffer") {}

protected:
    void runTest() override
    {
        auto stream = groupStream({packGroup(0, 0, 0), packGroup(0, 0, 0)});
        AC3BitReader reader(stream.data(), stream.size());
        uint8_t small[4] = {0};
        // Two D15 groups need seven exponents; four will not do.
        ASSERT_TRUE(ac3DecodeExponents(reader, ExponentStrategy::D15, 2, 5, small, 4) == 0,
                    "a buffer too small is refused before anything is written");

        uint8_t exponents[64] = {0};
        AC3BitReader reuse_reader(stream.data(), stream.size());
        ASSERT_TRUE(ac3DecodeExponents(reuse_reader, ExponentStrategy::Reuse, 2, 5,
                                       exponents, 64) == 0,
                    "Reuse decodes nothing: the previous block's exponents stand");

        // A stream that ends mid-group must not hand back half an answer.
        std::vector<uint8_t> truncated{0x00};
        AC3BitReader short_reader(truncated.data(), truncated.size());
        ASSERT_TRUE(ac3DecodeExponents(short_reader, ExponentStrategy::D15, 8, 5,
                                       exponents, 64) == 0,
                    "running past the end of the stream is refused");
    }
};

} // namespace

int main()
{
    TestSuite suite("AC-3 Exponent Decoding Tests");
    suite.addTest(std::make_unique<GroupSizeTest>());
    suite.addTest(std::make_unique<BinRangeTest>());
    suite.addTest(std::make_unique<GroupCountTest>());
    suite.addTest(std::make_unique<DifferentialDecodeTest>());
    suite.addTest(std::make_unique<BiasTest>());
    suite.addTest(std::make_unique<GroupExpansionTest>());
    suite.addTest(std::make_unique<ClampTest>());
    suite.addTest(std::make_unique<RefusalTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
