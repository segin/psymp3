/*
 * test_ac3_downmix.cpp - AC-3 channel arrangements onto speaker layouts, A/52 §7.8
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

using Mode = AudioCodingMode;

bool near(double a, double b, double tolerance = 1e-5) { return std::abs(a - b) <= tolerance; }

/// Full-bandwidth channels per acmod, Table 5.8.
constexpr unsigned kChannels[8] = { 2, 1, 2, 3, 3, 4, 4, 5 };

/// Every gain in @p m is the one listed, and every other is zero.
struct Gain { unsigned out, in; double value; };
bool matches(const AC3OutputMatrix& m, std::initializer_list<Gain> expected)
{
    double want[kMaxOutputChannels][kMixInputs] = {};
    for (const auto& g : expected) {
        want[g.out][g.in] = g.value;
    }
    for (unsigned out = 0; out < kMaxOutputChannels; ++out) {
        for (unsigned in = 0; in < kMixInputs; ++in) {
            if (!near(m.gain[out][in], want[out][in])) {
                return false;
            }
        }
    }
    return true;
}

class NativeLayoutTest : public TestCase {
public:
    NativeLayoutTest() : TestCase("Each arrangement gets a layout with a speaker for every coded channel") {}

protected:
    void runTest() override
    {
        ASSERT_TRUE(ac3OutputChannels(Mode::Mono, false) == 1 && ac3OutputChannels(Mode::Mono, true) == 6,
                    "1/0 is mono, and needs 5.1 for a centre beside an LFE");
        ASSERT_TRUE(ac3OutputChannels(Mode::Stereo, false) == 2 && ac3OutputChannels(Mode::Stereo, true) == 3,
                    "2/0 is stereo or 2.1");
        ASSERT_TRUE(ac3OutputChannels(Mode::DualMono, false) == 2, "1+1 is a pair");
        ASSERT_TRUE(ac3OutputChannels(Mode::ThreeZero, false) == 6 &&
                    ac3OutputChannels(Mode::ThreeTwo, false) == 6,
                    "a centre needs the 6-channel layout even without an LFE");
        ASSERT_TRUE(ac3OutputChannels(Mode::TwoTwo, false) == 4 && ac3OutputChannels(Mode::TwoTwo, true) == 5,
                    "2/2 is quad or 4.1");
        ASSERT_TRUE(ac3OutputChannels(Mode::TwoOne, true) == 5 && ac3OutputChannels(Mode::ThreeOne, false) == 6,
                    "a single surround plays through a pair");

        // In its own layout nothing is mixed: each coded channel reaches one
        // speaker at unity, bar a single surround split over a pair.
        for (unsigned acmod = 0; acmod < 8; ++acmod) {
            for (bool lfe : { false, true }) {
                const auto mode = static_cast<Mode>(acmod);
                const auto m = ac3OutputMatrix(mode, lfe, ac3OutputChannels(mode, lfe), AC3MixLevels());
                const std::string where = "acmod " + std::to_string(acmod) + (lfe ? " + LFE" : "");
                for (unsigned in = 0; in < kMixInputs; ++in) {
                    const bool coded = in < kChannels[acmod] || (in == kMixLfeInput && lfe);
                    unsigned reached = 0;
                    for (unsigned out = 0; out < m.outputs; ++out) {
                        if (m.gain[out][in] != 0.0f) {
                            ++reached;
                            const bool split = (acmod == 4 || acmod == 5) && in == kChannels[acmod] - 1;
                            ASSERT_TRUE(near(m.gain[out][in], split ? 0.70710678 : 1.0), where + ": unity routing");
                        }
                    }
                    const bool split = (acmod == 4 || acmod == 5) && in == kChannels[acmod] - 1;
                    ASSERT_TRUE(reached == (coded ? (split ? 2u : 1u) : 0u), where + ": one speaker each");
                }
            }
        }
    }
};

class FiveOneTest : public TestCase {
public:
    FiveOneTest() : TestCase("3/2 lands in SDL's 5.1 order, with or without its LFE") {}

protected:
    void runTest() override
    {
        // Bitstream L C R Ls Rs LFE onto FL FR FC LFE SL SR.
        ASSERT_TRUE(matches(ac3OutputMatrix(Mode::ThreeTwo, true, 6, AC3MixLevels()),
                            { {0, 0, 1}, {1, 2, 1}, {2, 1, 1}, {3, 5, 1}, {4, 3, 1}, {5, 4, 1} }),
                    "3/2 + LFE");
        // Without an LFE the centre must not move into the LFE's slot.
        ASSERT_TRUE(matches(ac3OutputMatrix(Mode::ThreeTwo, false, 6, AC3MixLevels()),
                            { {0, 0, 1}, {1, 2, 1}, {2, 1, 1}, {4, 3, 1}, {5, 4, 1} }),
                    "3/2 alone leaves the LFE speaker silent");
    }
};

class StereoDownmixTest : public TestCase {
public:
    StereoDownmixTest() : TestCase("5.1 into two speakers is §7.8.2's Lo/Ro, normalised") {}

protected:
    void runTest() override
    {
        const AC3MixLevels levels = ac3MixLevels(0, 0);   // -3 dB, -3 dB
        const double norm = 1.0 + 0.707 + 0.707;
        ASSERT_TRUE(matches(ac3OutputMatrix(Mode::ThreeTwo, true, 2, levels),
                            { {0, 0, 1 / norm}, {0, 1, 0.707 / norm}, {0, 3, 0.707 / norm},
                              {1, 2, 1 / norm}, {1, 1, 0.707 / norm}, {1, 4, 0.707 / norm} }),
                    "Lo = L + clev C + slev Ls, Ro likewise, LFE dropped, peak scaled to unity");

        // 1/0 needs no normalising: -3 dB to each side already sums to 1.41
        // across the pair but only 0.707 in either channel.
        ASSERT_TRUE(matches(ac3OutputMatrix(Mode::Mono, false, 2, AC3MixLevels()),
                            { {0, 0, 0.70710678}, {1, 0, 0.70710678} }),
                    "1/0 to stereo");
    }
};

class SurroundTest : public TestCase {
public:
    SurroundTest() : TestCase("A single surround goes to a pair at -3 dB, or to 6.1's back centre") {}

protected:
    void runTest() override
    {
        ASSERT_TRUE(matches(ac3OutputMatrix(Mode::TwoOne, false, 4, AC3MixLevels()),
                            { {0, 0, 1}, {1, 1, 1}, {2, 2, 0.70710678}, {3, 2, 0.70710678} }),
                    "2/1 into quad");
        ASSERT_TRUE(matches(ac3OutputMatrix(Mode::ThreeOne, true, 7, AC3MixLevels()),
                            { {0, 0, 1}, {1, 2, 1}, {2, 1, 1}, {3, 5, 1}, {4, 3, 1} }),
                    "3/1 + LFE into 6.1");
    }
};

class DualMonoTest : public TestCase {
public:
    DualMonoTest() : TestCase("1+1 keeps its programmes apart, or sums them at -6 dB for one speaker") {}

protected:
    void runTest() override
    {
        ASSERT_TRUE(matches(ac3OutputMatrix(Mode::DualMono, false, 2, AC3MixLevels()),
                            { {0, 0, 1}, {1, 1, 1} }), "a pair of speakers");
        ASSERT_TRUE(matches(ac3OutputMatrix(Mode::DualMono, false, 1, AC3MixLevels()),
                            { {0, 0, 0.5}, {0, 1, 0.5} }), "one speaker");
    }
};

class MixLevelTest : public TestCase {
public:
    MixLevelTest() : TestCase("Mix level codes follow Tables 5.9, 5.10, D2.5 and D2.6") {}

protected:
    void runTest() override
    {
        auto is = [](const AC3MixLevels& l, double c, double s) { return near(l.center, c) && near(l.surround, s); };
        ASSERT_TRUE(is(ac3MixLevels(0, 0), 0.707, 0.707), "AC-3 00");
        ASSERT_TRUE(is(ac3MixLevels(2, 2), 0.5, 0.0), "AC-3 10");
        ASSERT_TRUE(is(ac3MixLevels(3, 3), 0.595, 0.5), "reserved takes the intermediate value of 01");
        ASSERT_TRUE(is(eac3MixLevels(0, 0), 1.414, 0.841), "E-AC-3 +3 dB centre; reserved surround is 0.841");
        ASSERT_TRUE(is(eac3MixLevels(4, 4), 0.707, 0.707), "E-AC-3 -3 dB");
        ASSERT_TRUE(is(eac3MixLevels(7, 7), 0.0, 0.0), "E-AC-3 -inf");
    }
};

class LfeMixTest : public TestCase {
public:
    LfeMixTest() : TestCase("E-AC-3's LFE mix level puts the LFE into stereo and mono output (§E3.9)") {}

protected:
    void runTest() override
    {
        // lfemixlevcod 10 is a 0 dB LFE mix level, mixed in at -4.5 dB.
        const AC3MixLevels levels = eac3MixLevels(4, 4, true, 10);
        const double g = std::pow(10.0, -4.5 / 20.0);
        ASSERT_TRUE(near(levels.lfe, g), "10 - 10 - 4.5 dB");
        ASSERT_TRUE(near(eac3MixLevels(4, 4, true, 0).lfe, std::pow(10.0, 5.5 / 20.0)), "code 0 is +5.5 dB");
        ASSERT_TRUE(eac3MixLevels(4, 4).lfe == 0.0f, "no code, no LFE mixing");
        ASSERT_TRUE(ac3MixLevels(0, 0).lfe == 0.0f, "and AC-3 has none");

        const AC3MixLevels plain = eac3MixLevels(4, 4);
        for (unsigned outputs : { 1u, 2u }) {
            const auto with = ac3OutputMatrix(Mode::ThreeTwo, true, outputs, levels);
            const auto without = ac3OutputMatrix(Mode::ThreeTwo, true, outputs, plain);
            const std::string where = std::to_string(outputs) + " channel(s)";
            for (unsigned out = 0; out < outputs; ++out) {
                ASSERT_TRUE(near(with.gain[out][kMixLfeInput], g), where + ": the LFE is mixed in");
                ASSERT_TRUE(without.gain[out][kMixLfeInput] == 0.0f, where + ": and not without the code");
                for (unsigned in = 0; in < kMixLfeInput; ++in) {
                    ASSERT_TRUE(near(with.gain[out][in], without.gain[out][in]),
                                where + ": the rest of the downmix is unchanged");
                }
            }
        }

        // An LFE speaker takes it as before, and nothing else does.
        const auto six = ac3OutputMatrix(Mode::ThreeTwo, true, 6, levels);
        for (unsigned out = 0; out < 6; ++out) {
            ASSERT_TRUE(six.gain[out][kMixLfeInput] == (out == 3 ? 1.0f : 0.0f),
                        "5.1 output keeps the LFE on its own speaker");
        }
    }
};

class NoOverloadTest : public TestCase {
public:
    NoOverloadTest() : TestCase("No output channel sums to more than unity, and nothing coded is lost") {}

protected:
    void runTest() override
    {
        for (const AC3MixLevels& levels : { ac3MixLevels(0, 0), eac3MixLevels(0, 3) }) {
            for (unsigned acmod = 0; acmod < 8; ++acmod) {
                for (bool lfe : { false, true }) {
                    for (unsigned outputs = 1; outputs <= kMaxOutputChannels; ++outputs) {
                        const auto m = ac3OutputMatrix(static_cast<Mode>(acmod), lfe, outputs, levels);
                        const std::string where = "acmod " + std::to_string(acmod) + " into " +
                                                  std::to_string(outputs);
                        for (unsigned out = 0; out < outputs; ++out) {
                            double sum = 0.0;
                            for (unsigned in = 0; in < kMixInputs; ++in) {
                                sum += std::abs(m.gain[out][in]);
                            }
                            ASSERT_TRUE(sum <= 1.0 + 1e-5, where + ": no overload");
                        }
                        for (unsigned in = 0; in < kChannels[acmod]; ++in) {
                            double reach = 0.0;
                            for (unsigned out = 0; out < outputs; ++out) {
                                reach += std::abs(m.gain[out][in]);
                            }
                            ASSERT_TRUE(reach > 0.0, where + ": every full-bandwidth channel is heard");
                        }
                    }
                }
            }
        }
    }
};

} // namespace

int main()
{
    TestSuite suite("AC-3 Downmix Tests");
    suite.addTest(std::make_unique<NativeLayoutTest>());
    suite.addTest(std::make_unique<FiveOneTest>());
    suite.addTest(std::make_unique<StereoDownmixTest>());
    suite.addTest(std::make_unique<SurroundTest>());
    suite.addTest(std::make_unique<DualMonoTest>());
    suite.addTest(std::make_unique<MixLevelTest>());
    suite.addTest(std::make_unique<LfeMixTest>());
    suite.addTest(std::make_unique<NoOverloadTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
