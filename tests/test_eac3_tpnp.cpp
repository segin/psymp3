/*
 * test_eac3_tpnp.cpp - E-AC-3 transient pre-noise processing, A/52 Annex E §E3.7
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * No encoder available to this project emits transient pre-noise processing,
 * so these tests hold the correction to the sample arithmetic of §E3.7.2.
 */

#include "psymp3.h"
#include "test_framework.h"

using namespace TestFramework;
using namespace PsyMP3::Codec::AC3;

namespace {

constexpr size_t kFrame = 1536;

/// A recognisable signal: every sample distinct, so a copy from the wrong
/// offset cannot pass for the right one.
std::vector<float> ramp(size_t n, float scale = 1e-4f)
{
    std::vector<float> v(n);
    for (size_t i = 0; i < n; ++i) {
        v[i] = scale * static_cast<float>(i + 1);
    }
    return v;
}

class PassThroughTest : public TestCase {
public:
    PassThroughTest() : TestCase("A frame without the flag passes through untouched") {}

protected:
    void runTest() override
    {
        EAC3TransientPreNoise tpnp;
        const auto input = ramp(kFrame);
        for (int frame = 0; frame < 3; ++frame) {
            auto pcm = input;
            tpnp.process(pcm.data(), pcm.size(), false, 0, 0);
            ASSERT_TRUE(pcm == input, "inactive frames are not modified");
        }
    }
};

/// The worked case: transient at sample 1024 (block 4), so the pre-noise runs
/// from block 3's start at 768, PN = 256. With transproclen 64 the corrected
/// span is PN + len + TC1 = 576 samples, from 448 to the transient; the
/// synthesis buffer is 2*TC1 + PN = 768 samples from 2*TC1 + 2*PN = 1024
/// before the transient, i.e. from sample 0.
class CorrectionSpanTest : public TestCase {
public:
    CorrectionSpanTest() : TestCase("The pre-noise is overwritten over exactly the spec's span") {}

protected:
    void runTest() override
    {
        EAC3TransientPreNoise tpnp;
        const auto input = ramp(kFrame);
        auto pcm = input;
        tpnp.process(pcm.data(), pcm.size(), true, /*transprocloc=*/256, /*transproclen=*/64);

        constexpr size_t kStart = 448, kTrans = 1024, kTC1 = 256, kTC2 = 128;
        for (size_t n = 0; n < kStart; ++n) {
            ASSERT_TRUE(pcm[n] == input[n], "before the corrected span, unchanged");
        }
        for (size_t n = kTrans; n < kFrame; ++n) {
            ASSERT_TRUE(pcm[n] == input[n], "the transient and after, unchanged");
        }
        // The middle is a straight copy of the synthesis buffer: output sample
        // kStart + i is input sample i.
        for (size_t i = kTC1; i < (kTrans - kStart) - kTC2; ++i) {
            ASSERT_TRUE(pcm[kStart + i] == input[i], "overwritten from the synthesis buffer");
        }
        // Both ends are cross-fades, so each output lies between its two
        // sources and differs from the original somewhere.
        bool faded = false;
        for (size_t i = 0; i < kTC1; ++i) {
            const float a = input[kStart + i], b = input[i];
            const float lo = std::min(a, b) * 0.999f, hi = std::max(a, b) * 1.415f;
            ASSERT_TRUE(pcm[kStart + i] >= lo && pcm[kStart + i] <= hi, "fade-in stays between its sources");
            faded = faded || pcm[kStart + i] != a;
        }
        ASSERT_TRUE(faded, "the first cross-fade changes the output");
    }
};

/// A reference that would need samples from before the stream began is not
/// applied: with no history there is nothing to copy from. Once there is, a
/// correction whose opening fade starts just before the frame still applies.
class OutOfRangeTest : public TestCase {
public:
    OutOfRangeTest() : TestCase("A correction needing samples before the stream is skipped") {}

protected:
    void runTest() override
    {
        EAC3TransientPreNoise tpnp;
        const auto input = ramp(kFrame);
        auto pcm = input;
        // Transient at 512: the buffer would start 2*TC1 + 2*PN = 1024 earlier.
        tpnp.process(pcm.data(), pcm.size(), true, 128, 32);
        ASSERT_TRUE(pcm == input, "no history, so nothing is changed");

        // The same frame again, with the first as history. The correction
        // now starts 32 samples before this frame -- inside the 256-sample
        // opening fade -- so it applies from the frame start on.
        auto second = input;
        tpnp.process(second.data(), second.size(), true, 128, 32);
        ASSERT_TRUE(second != input, "with the previous frame to draw on, it is");
        constexpr size_t kTrans = 512, kTC2 = 128;
        for (size_t n = kTrans; n < kFrame; ++n) {
            ASSERT_TRUE(second[n] == input[n], "nothing from the transient on changes");
        }
        // Sample 0 is 32 samples into the opening fade, so it must be exactly
        // that point of the cross-fade: the original weighted by the fade-out,
        // plus the synthesis buffer -- which starts 2*TC1 + 2*PN = 1024 before
        // the transient, i.e. at history sample 1024 -- weighted by the fade-in.
        // Sample 0 therefore draws on synthesis offset 32, history sample 1056.
        const double phase = 0.5 * 3.14159265358979323846 * (32.0 + 0.5) / 256.0;
        const double expected = input[0] * std::cos(phase) + input[1056] * std::sin(phase);
        ASSERT_TRUE(std::abs(second[0] - expected) < 1e-6,
                    "the partial fade resumes at its 33rd sample");
        ASSERT_TRUE(second[kTrans - kTC2 - 1] != input[kTrans - kTC2 - 1],
                    "the middle of the span is overwritten");
    }
};

} // namespace

int main()
{
    TestSuite suite("E-AC-3 Transient Pre-Noise Processing Tests");
    suite.addTest(std::make_unique<PassThroughTest>());
    suite.addTest(std::make_unique<CorrectionSpanTest>());
    suite.addTest(std::make_unique<OutOfRangeTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
