/*
 * test_ac3_bit_allocation.cpp - AC-3 parametric bit allocation (A/52 §7.2)
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

AllocationParameters defaultParameters()
{
    AllocationParameters p;
    p.fscod = 0;
    p.sdcycod = 1;
    p.fdcycod = 1;
    p.sgaincod = 1;
    p.dbpbcod = 2;
    p.floorcod = 4;
    p.fgaincod = 4;
    p.snroffset = ((0 - 15) << 4) << 2;   // csnroffst 0, fsnroffst 0
    return p;
}

class LowCompTest : public TestCase {
public:
    LowCompTest() : TestCase("Low-frequency compensation changes character at bands 7 and 20") {}

protected:
    void runTest() override
    {
        // A/52 §7.2.2.4. Below band 7 a step of exactly 256 sets 384; from 7 to
        // 19 the same step sets 320; from 20 up the value simply decays and the
        // comparison is not made at all. Those two boundaries are the whole
        // shape of the function.
        ASSERT_TRUE(ac3CalcLowComp(0, 100, 356, 0) == 384, "a 256 step below band 7 gives 384");
        ASSERT_TRUE(ac3CalcLowComp(0, 100, 356, 6) == 384, "still 384 at band 6");
        ASSERT_TRUE(ac3CalcLowComp(0, 100, 356, 7) == 320, "but 320 from band 7");
        ASSERT_TRUE(ac3CalcLowComp(0, 100, 356, 19) == 320, "and through band 19");

        // From band 20 the step is ignored and the value only decays.
        ASSERT_TRUE(ac3CalcLowComp(300, 100, 356, 20) == 172, "band 20 decays by 128");
        ASSERT_TRUE(ac3CalcLowComp(64, 100, 356, 20) == 0, "and cannot go below zero");

        // A falling band decays by 64 in the lower two ranges.
        ASSERT_TRUE(ac3CalcLowComp(200, 500, 100, 3) == 136, "a falling band decays by 64");
        ASSERT_TRUE(ac3CalcLowComp(32, 500, 100, 3) == 0, "clamped at zero");

        // Neither equal nor falling: the value is left alone. The standard
        // prints this branch with a stray semicolon that would set 384
        // unconditionally; taken literally that would fire here too.
        ASSERT_TRUE(ac3CalcLowComp(200, 100, 200, 3) == 200,
                    "a band that neither steps by 256 nor falls leaves the value alone");
    }
};

class RangeTest : public TestCase {
public:
    RangeTest() : TestCase("Every allocation pointer is a valid quantiser index") {}

protected:
    void runTest() override
    {
        // bap indexes the quantisation tables of §7.3, so a value outside 0..15
        // would read past them however plausible the audio looked.
        uint8_t exponents[kBinCount];
        uint8_t bap[kBinCount] = {0};
        for (unsigned trial = 0; trial < 4; ++trial) {
            for (unsigned i = 0; i < kBinCount; ++i) {
                exponents[i] = static_cast<uint8_t>((i * (trial + 1)) % (kMaxExponent + 1));
            }
            ASSERT_TRUE(ac3ComputeBitAllocation(exponents, 0, 253,
                                                AllocationChannel::FullBandwidth,
                                                defaultParameters(), {}, bap),
                        "allocation succeeds");
            for (unsigned i = 0; i < 253; ++i) {
                ASSERT_TRUE(bap[i] <= 15, "every pointer is within the quantiser table");
            }
        }
    }
};

class LoudSignalTest : public TestCase {
public:
    LoudSignalTest() : TestCase("A louder band is given at least as many bits as a quiet one") {}

protected:
    void runTest() override
    {
        // An exponent is a right shift, so a smaller one is a louder bin. The
        // allocation should never spend fewer bits on the louder spectrum --
        // that is the whole point of the psychoacoustic model.
        uint8_t loud[kBinCount], quiet[kBinCount];
        uint8_t loud_bap[kBinCount] = {0}, quiet_bap[kBinCount] = {0};
        for (unsigned i = 0; i < kBinCount; ++i) {
            loud[i] = 2;
            quiet[i] = 20;
        }
        const auto parameters = defaultParameters();
        ASSERT_TRUE(ac3ComputeBitAllocation(loud, 0, 253, AllocationChannel::FullBandwidth,
                                            parameters, {}, loud_bap), "loud allocates");
        ASSERT_TRUE(ac3ComputeBitAllocation(quiet, 0, 253, AllocationChannel::FullBandwidth,
                                            parameters, {}, quiet_bap), "quiet allocates");

        unsigned loud_total = 0, quiet_total = 0;
        for (unsigned i = 0; i < 253; ++i) {
            loud_total += loud_bap[i];
            quiet_total += quiet_bap[i];
        }
        ASSERT_TRUE(loud_total >= quiet_total,
                    "a uniformly louder spectrum is not allocated fewer bits");
    }
};

class SnrOffsetTest : public TestCase {
public:
    SnrOffsetTest() : TestCase("Raising the SNR offset never reduces the allocation") {}

protected:
    void runTest() override
    {
        // snroffset is subtracted from the masking curve, so raising it lowers
        // the curve and buys more bits. It is the knob the encoder iterates on
        // to make a frame fit its bit budget, and the direction has to be right
        // or the decoder's totals diverge from the encoder's.
        uint8_t exponents[kBinCount];
        for (unsigned i = 0; i < kBinCount; ++i) {
            exponents[i] = static_cast<uint8_t>(6 + (i % 5));
        }

        unsigned previous_total = 0;
        for (int csnroffst = 0; csnroffst <= 15; csnroffst += 5) {
            auto parameters = defaultParameters();
            parameters.snroffset = ((csnroffst - 15) << 4) << 2;
            uint8_t bap[kBinCount] = {0};
            ASSERT_TRUE(ac3ComputeBitAllocation(exponents, 0, 253,
                                                AllocationChannel::FullBandwidth,
                                                parameters, {}, bap), "allocates");
            unsigned total = 0;
            for (unsigned i = 0; i < 253; ++i) {
                total += bap[i];
            }
            ASSERT_TRUE(total >= previous_total,
                        "a higher SNR offset allocates at least as many bits");
            previous_total = total;
        }
    }
};

class DeltaTest : public TestCase {
public:
    DeltaTest() : TestCase("Delta bit allocation moves the masking curve where told") {}

protected:
    void runTest() override
    {
        uint8_t exponents[kBinCount];
        for (unsigned i = 0; i < kBinCount; ++i) {
            exponents[i] = 8;
        }
        const auto parameters = defaultParameters();

        uint8_t plain[kBinCount] = {0}, adjusted[kBinCount] = {0};
        ASSERT_TRUE(ac3ComputeBitAllocation(exponents, 0, 253,
                                            AllocationChannel::FullBandwidth,
                                            parameters, {}, plain), "without deltas");

        // A delta of 0 lowers the curve by 4 steps of 6 dB, which buys bits in
        // the bands it covers; the encoder sends these where the parametric
        // model alone would misjudge a block.
        std::vector<DeltaBitAllocation> deltas{{0, 10, 0}};
        ASSERT_TRUE(ac3ComputeBitAllocation(exponents, 0, 253,
                                            AllocationChannel::FullBandwidth,
                                            parameters, deltas, adjusted), "with deltas");

        unsigned plain_total = 0, adjusted_total = 0;
        for (unsigned i = 0; i < 60; ++i) {
            plain_total += plain[i];
            adjusted_total += adjusted[i];
        }
        ASSERT_TRUE(adjusted_total != plain_total,
                    "the covered bands are allocated differently, which is the "
                    "whole purpose of the override");
    }
};

class RefusalTest : public TestCase {
public:
    RefusalTest() : TestCase("Parameters outside their tables are refused") {}

protected:
    void runTest() override
    {
        // Every code below indexes a table. A stream stating one out of range
        // would otherwise read past it, and these come straight from the file.
        uint8_t exponents[kBinCount] = {0};
        uint8_t bap[kBinCount] = {0};
        auto parameters = defaultParameters();

        ASSERT_FALSE(ac3ComputeBitAllocation(nullptr, 0, 253, AllocationChannel::FullBandwidth,
                                             parameters, {}, bap), "no exponents");
        ASSERT_FALSE(ac3ComputeBitAllocation(exponents, 0, 253, AllocationChannel::FullBandwidth,
                                             parameters, {}, nullptr), "nowhere to write");
        ASSERT_FALSE(ac3ComputeBitAllocation(exponents, 100, 100,
                                             AllocationChannel::FullBandwidth,
                                             parameters, {}, bap), "an empty range");
        ASSERT_FALSE(ac3ComputeBitAllocation(exponents, 0, kBinCount + 1,
                                             AllocationChannel::FullBandwidth,
                                             parameters, {}, bap), "past the last bin");

        parameters.fscod = 3;   // reserved: there is no such hearing threshold column
        ASSERT_FALSE(ac3ComputeBitAllocation(exponents, 0, 253,
                                             AllocationChannel::FullBandwidth,
                                             parameters, {}, bap), "a reserved sample rate");
        parameters = defaultParameters();
        parameters.floorcod = 8;
        ASSERT_FALSE(ac3ComputeBitAllocation(exponents, 0, 253,
                                             AllocationChannel::FullBandwidth,
                                             parameters, {}, bap), "a floor code past the table");
    }
};

class DeterminismTest : public TestCase {
public:
    DeterminismTest() : TestCase("The same input always allocates the same bits") {}

protected:
    void runTest() override
    {
        // The encoder ran this computation to decide what to write. If the
        // decoder's answer varied at all -- on uninitialised state, say -- it
        // would read the next mantissa from the wrong bit and lose the block.
        uint8_t exponents[kBinCount];
        for (unsigned i = 0; i < kBinCount; ++i) {
            exponents[i] = static_cast<uint8_t>((i * 7) % 25);
        }
        uint8_t first[kBinCount] = {0}, second[kBinCount] = {0};
        const auto parameters = defaultParameters();
        ac3ComputeBitAllocation(exponents, 0, 253, AllocationChannel::FullBandwidth,
                                parameters, {}, first);
        ac3ComputeBitAllocation(exponents, 0, 253, AllocationChannel::FullBandwidth,
                                parameters, {}, second);
        for (unsigned i = 0; i < 253; ++i) {
            ASSERT_TRUE(first[i] == second[i], "the allocation is reproducible");
        }

        // The LFE channel runs a shorter range and must not disturb the rest.
        uint8_t lfe[kBinCount] = {0};
        ASSERT_TRUE(ac3ComputeBitAllocation(exponents, 0, 7, AllocationChannel::LFE,
                                            parameters, {}, lfe), "the LFE range allocates");
        uint8_t coupling[kBinCount] = {0};
        ASSERT_TRUE(ac3ComputeBitAllocation(exponents, 37, 73, AllocationChannel::Coupling,
                                            parameters, {}, coupling), "so does the coupling range");
    }
};

} // namespace

int main()
{
    TestSuite suite("AC-3 Bit Allocation Tests");
    suite.addTest(std::make_unique<LowCompTest>());
    suite.addTest(std::make_unique<RangeTest>());
    suite.addTest(std::make_unique<LoudSignalTest>());
    suite.addTest(std::make_unique<SnrOffsetTest>());
    suite.addTest(std::make_unique<DeltaTest>());
    suite.addTest(std::make_unique<RefusalTest>());
    suite.addTest(std::make_unique<DeterminismTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
