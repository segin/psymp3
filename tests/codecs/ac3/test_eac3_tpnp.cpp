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

constexpr int64_t kTC1 = EAC3TransientCorrection::kTC1;
constexpr int64_t kTC2 = EAC3TransientCorrection::kTC2;

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

/// The Hanning fade-in the implementation is expected to use.
double hann(int64_t n, int64_t length)
{
    return 0.5 - 0.5 * std::cos(3.14159265358979323846 * (n + 0.5) / static_cast<double>(length));
}

class PlacementTest : public TestCase {
public:
    PlacementTest() : TestCase("A frame's parameters place the correction in absolute samples") {}

protected:
    void runTest() override
    {
        // Transient at sample 1024, in block 4: the pre-noise runs from block
        // 3's start at 768, so PN = 256. The corrected span is PN + len + TC1
        // = 576 samples back from the transient; the synthesis buffer starts
        // 2*TC1 + 2*PN = 1024 before it.
        const auto a = eac3TransientCorrection(0, 256, 64);
        ASSERT_TRUE(a.transloc == 1024 && a.pnlen == 256 && a.translen == 64, "the worked case");
        ASSERT_TRUE(a.start() == 448 && a.synthesisStart() == 0, "its span and buffer");

        // A transient in block 0 of a frame starting at 1536: the block before
        // it, and so the pre-noise, the overwrite and the buffer, all begin in
        // the previous frame.
        const auto b = eac3TransientCorrection(1536, 16, 10);
        ASSERT_TRUE(b.transloc == 1600 && b.pnlen == 320, "PN reaches into the previous frame");
        ASSERT_TRUE(b.start() == 1014 && b.synthesisStart() == 448, "and so does everything else");

        // §E3.7.1: a frame may describe a transient in the frame after it.
        const auto c = eac3TransientCorrection(0, 500, 0);
        ASSERT_TRUE(c.transloc == 2000 && c.pnlen == 2000 - 1536, "a transient past the frame's end");
    }
};

class CorrectionSpanTest : public TestCase {
public:
    CorrectionSpanTest() : TestCase("The pre-noise is overwritten over exactly the spec's span") {}

protected:
    void runTest() override
    {
        const auto input = ramp(1536);
        auto pcm = input;
        const auto c = eac3TransientCorrection(0, 256, 64);
        eac3ApplyTransientCorrection(pcm.data(), 0, c);

        constexpr size_t kStart = 448, kTrans = 1024;
        const int64_t total = kTrans - kStart;
        for (size_t n = 0; n < kStart; ++n) {
            ASSERT_TRUE(pcm[n] == input[n], "before the corrected span, unchanged");
        }
        for (size_t n = kTrans; n < pcm.size(); ++n) {
            ASSERT_TRUE(pcm[n] == input[n], "the transient and after, unchanged");
        }
        // Output sample kStart + i draws on synthesis sample i, input sample i.
        for (int64_t i = 0; i < kTC1; ++i) {
            const double expected = input[kStart + i] * (1.0 - hann(i, kTC1)) + input[i] * hann(i, kTC1);
            ASSERT_TRUE(std::abs(pcm[kStart + i] - expected) < 1e-6, "the opening cross-fade");
        }
        for (int64_t i = kTC1; i < total - kTC2; ++i) {
            ASSERT_TRUE(pcm[kStart + i] == input[i], "overwritten from the synthesis buffer");
        }
        for (int64_t i = total - kTC2; i < total; ++i) {
            const int64_t w = i - (total - kTC2);
            const double expected = input[kStart + i] * hann(w, kTC2) + input[i] * (1.0 - hann(w, kTC2));
            ASSERT_TRUE(std::abs(pcm[kStart + i] - expected) < 1e-6, "the closing cross-fade");
        }
    }
};

/// §E3.7.2's windows are constant amplitude: cross-fading a signal into an
/// identical one must leave it exactly as it was.
class ConstantAmplitudeTest : public TestCase {
public:
    ConstantAmplitudeTest() : TestCase("Cross-fading between equal signals leaves them unchanged") {}

protected:
    void runTest() override
    {
        std::vector<float> pcm(2048, 0.5f);
        eac3ApplyTransientCorrection(pcm.data(), 0, eac3TransientCorrection(0, 400, 200));
        for (float v : pcm) {
            ASSERT_TRUE(std::abs(v - 0.5f) < 1e-6f, "no swell or dip anywhere in the span");
        }
    }
};

/// A line that holds only recent samples is addressed by absolute position:
/// the same correction on it and on the whole timeline agrees sample for
/// sample.
class OriginTest : public TestCase {
public:
    OriginTest() : TestCase("A correction lands at the same samples on a line with a later origin") {}

protected:
    void runTest() override
    {
        const auto c = eac3TransientCorrection(2560, 16, 10);   // transient at 2624
        auto full = ramp(4096);
        constexpr int64_t kOrigin = 1000;
        std::vector<float> part(full.begin() + kOrigin, full.begin() + kOrigin + 2048);
        ASSERT_TRUE(c.synthesisStart() >= kOrigin, "the partial line covers the buffer");

        eac3ApplyTransientCorrection(full.data(), 0, c);
        eac3ApplyTransientCorrection(part.data(), kOrigin, c);
        bool changed = false;
        for (size_t n = 0; n < part.size(); ++n) {
            ASSERT_TRUE(part[n] == full[kOrigin + n], "sample " + std::to_string(kOrigin + n));
            changed = changed || part[n] != 1e-4f * static_cast<float>(kOrigin + n + 1);
        }
        ASSERT_TRUE(changed, "and the correction did something");
    }
};

} // namespace

int test_eac3_tpnp_main()
{
    TestSuite suite("E-AC-3 Transient Pre-Noise Processing Tests");
    suite.addTest(std::make_unique<PlacementTest>());
    suite.addTest(std::make_unique<CorrectionSpanTest>());
    suite.addTest(std::make_unique<ConstantAmplitudeTest>());
    suite.addTest(std::make_unique<OriginTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
