/*
 * test_ac3_transform.cpp - AC-3 inverse transform and overlap-add (A/52 §7.9)
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

constexpr double kPi = 3.14159265358979323846;

/// The forward MDCT of A/52 §8.2.3.2, written out from its definition rather
/// than factored. The decoder has no forward transform in it -- this exists
/// only so a test can put a known signal in and see whether what comes back
/// out is the same signal.
void forwardMdct(const float* input, float* coefficients)
{
    const unsigned N = kTransformSize;
    for (unsigned k = 0; k < N / 2; ++k) {
        double sum = 0.0;
        for (unsigned n = 0; n < N; ++n) {
            const double phase = 2.0 * kPi / N * (n + 0.5 + N / 4.0) * (k + 0.5);
            sum += input[n] * std::cos(phase);
        }
        coefficients[k] = static_cast<float>(-2.0 * sum / N);
    }
}

/// The window applied on the way in, which the decoder applies again on the
/// way out; the two together give the power-complementary overlap.
void applyWindow(float* block)
{
    for (unsigned n = 0; n < kBlockSamples; ++n) {
        block[n] *= kWindow[n];
        block[kTransformSize - 1 - n] *= kWindow[n];
    }
}

/// A/52 §7.9 exists to reconstruct a signal from critically sampled blocks.
/// Feed a known waveform through a forward MDCT and back, and the overlap-add
/// of consecutive blocks must return it. Nothing else tests the de-interleave
/// in step 5, where a single swapped index still produces plausible-looking
/// audio.
class PerfectReconstructionTest : public TestCase {
public:
    PerfectReconstructionTest()
        : TestCase("Overlap-add reconstructs the signal it was given") {}

protected:
    void runTest() override
    {
        // Enough blocks that the delay line has to be right more than once.
        constexpr unsigned kBlocks = 6;
        constexpr unsigned kTotal = kBlockSamples * (kBlocks + 1);
        std::vector<float> signal(kTotal);
        for (unsigned n = 0; n < kTotal; ++n) {
            // Two tones well away from bin centres, so no coefficient is
            // exactly zero and an index error cannot hide in silence.
            signal[n] = 0.4f * std::sin(2.0 * kPi * n * 0.031)
                      + 0.25f * std::sin(2.0 * kPi * n * 0.117 + 0.7);
        }

        AC3TransformState state;
        std::vector<float> output(kTotal, 0.0f);
        for (unsigned b = 0; b < kBlocks; ++b) {
            float block[kTransformSize];
            for (unsigned n = 0; n < kTransformSize; ++n) {
                block[n] = signal[b * kBlockSamples + n];
            }
            applyWindow(block);

            float coefficients[kBlockSamples];
            forwardMdct(block, coefficients);
            // No rescaling: §8.2.3.2's -2/N and the decoder's factor of two
            // in §7.9.4.1 step 6 are the two halves of the same headroom
            // arrangement, so the pair round-trips at unity.
            ac3InverseTransform(coefficients, false, state,
                                &output[b * kBlockSamples]);
        }

        // The first block has nothing to overlap with, so reconstruction
        // starts one block in.
        double worst = 0.0;
        for (unsigned n = kBlockSamples; n < kBlockSamples * kBlocks; ++n) {
            worst = std::max(worst, std::abs(double(output[n]) - signal[n]));
        }
        // Exact to five decimals in practice; the bound is loose enough
        // that a float rounding difference cannot make this flaky, and tight
        // enough that any index or sign error fails it outright.
        ASSERT_TRUE(worst < 1e-3,
                    "overlap-add returns the original signal (worst error "
                    + std::to_string(worst) + ")");
    }
};

/// blksw picks two 256-sample transforms instead of one 512-sample one. They
/// are a different set of equations reading the same coefficients, so the
/// cheap mistake is to have them produce something that merely looks like
/// audio. Energy has to survive, and the delay line has to keep working.
class ShortBlockTest : public TestCase {
public:
    ShortBlockTest() : TestCase("Short transforms produce signal and keep the delay line") {}

protected:
    void runTest() override
    {
        float coefficients[kBlockSamples] = {};
        for (unsigned k = 0; k < kBlockSamples; ++k) {
            coefficients[k] = (k % 7 == 0) ? 0.1f : 0.0f;
        }

        AC3TransformState state;
        float first[kBlockSamples], second[kBlockSamples];
        ac3InverseTransform(coefficients, true, state, first);
        ac3InverseTransform(coefficients, true, state, second);

        double energy_first = 0.0, energy_second = 0.0;
        for (unsigned n = 0; n < kBlockSamples; ++n) {
            energy_first += double(first[n]) * first[n];
            energy_second += double(second[n]) * second[n];
            ASSERT_TRUE(std::isfinite(first[n]) && std::isfinite(second[n]),
                        "output stays finite");
            ASSERT_TRUE(std::abs(first[n]) <= 1.0f && std::abs(second[n]) <= 1.0f,
                        "output stays within full scale");
        }
        ASSERT_TRUE(energy_second > 0.0, "the transform produces signal");
        // The first block overlaps against a zero delay line and the second
        // against a real one, so they cannot come out identical.
        ASSERT_TRUE(energy_second > energy_first,
                    "the delay line carries into the second block");
    }
};

/// A silent block must stay silent: with no coefficients there is nothing to
/// reconstruct, and any constant leaking out of the twiddles or the window
/// would show up here as a DC offset.
class SilenceTest : public TestCase {
public:
    SilenceTest() : TestCase("Zero coefficients give zero samples") {}

protected:
    void runTest() override
    {
        float coefficients[kBlockSamples] = {};
        AC3TransformState state;
        float pcm[kBlockSamples];

        for (unsigned pass = 0; pass < 3; ++pass) {
            const bool shortblock = pass == 1;
            ac3InverseTransform(coefficients, shortblock, state, pcm);
            for (unsigned n = 0; n < kBlockSamples; ++n) {
                ASSERT_TRUE(pcm[n] == 0.0f, "silence in, silence out");
            }
        }
    }
};

/// Table 7.33 is transcribed from a printed two-column table, and four of
/// A/52's other tables turned out to have page furniture in them. These two
/// properties would not survive a single wrong digit.
class WindowTableTest : public TestCase {
public:
    WindowTableTest() : TestCase("The transform window is monotone and power-complementary") {}

protected:
    void runTest() override
    {
        for (unsigned n = 0; n + 1 < 256; ++n) {
            ASSERT_TRUE(kWindow[n] <= kWindow[n + 1], "the window never decreases");
        }
        ASSERT_TRUE(kWindow[0] > 0.0f && kWindow[255] <= 1.0f, "it runs from near 0 to 1");

        // w[n]^2 + w[255-n]^2 == 1 is what makes two overlapped blocks sum
        // back to the original signal. Five printed decimals bound the error.
        for (unsigned n = 0; n < 256; ++n) {
            const double sum = double(kWindow[n]) * kWindow[n]
                             + double(kWindow[255 - n]) * kWindow[255 - n];
            ASSERT_TRUE(std::abs(sum - 1.0) < 2e-5,
                        "window is power-complementary at " + std::to_string(n));
        }
    }
};

} // namespace

int main()
{
    TestSuite suite("AC-3 Inverse Transform Tests");
    suite.addTest(std::make_unique<WindowTableTest>());
    suite.addTest(std::make_unique<SilenceTest>());
    suite.addTest(std::make_unique<PerfectReconstructionTest>());
    suite.addTest(std::make_unique<ShortBlockTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
