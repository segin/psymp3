/*
 * test_eac3_spx.cpp - E-AC-3 spectral extension synthesis, A/52 Annex E §E3.6
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * No encoder available to this project emits spectral extension, so there is
 * no reference decode to diff against. These tests hold the synthesis to the
 * arithmetic the standard specifies instead.
 */

#include "psymp3.h"
#include "test_framework.h"

using namespace TestFramework;
using namespace PsyMP3::Codec::AC3;

namespace {

bool near(double a, double b, double tolerance) { return std::abs(a - b) <= tolerance; }

/// §E3.6.2 grouping, with Table E2.11's default structure.
class BandingTest : public TestCase {
public:
    BandingTest() : TestCase("Sub-bands group into bands by the band structure") {}

protected:
    void runTest() override
    {
        ASSERT_TRUE(eac3SpxBandStart(0) == 25 && eac3SpxBandStart(2) == 49 &&
                    eac3SpxBandStart(17) == 229, "Table E3.13 band starts");

        // Sub-bands 5..12 under the default: 8, 10 and 12 merge into the band
        // before them, so the bands are {5} {6} {7,8} {9,10} {11,12}.
        EAC3SpxBands bands;
        eac3SpxComputeBands(2, 5, 13, kDefaultSpxBandStructure, bands);
        ASSERT_TRUE(bands.count == 5, "five bands");
        const unsigned expected[5] = { 12, 12, 24, 24, 24 };
        unsigned total = 0;
        for (unsigned i = 0; i < 5; ++i) {
            ASSERT_TRUE(bands.size[i] == expected[i], "band " + std::to_string(i) + " size");
            total += bands.size[i];
        }
        ASSERT_TRUE(total == (13 - 5) * 12u, "the bands cover exactly the synthesised range");

        // A structure of all zeros keeps every sub-band its own band.
        const uint8_t separate[kSpxSubbands] = {};
        eac3SpxComputeBands(0, 3, 9, separate, bands);
        ASSERT_TRUE(bands.count == 6, "no merging, one band per sub-band");
    }
};

/// §E3.6.3: an implied leading 1 below exponent 15, and 18 dB per master step.
class CoordinateTest : public TestCase {
public:
    CoordinateTest() : TestCase("Spectral extension coordinates decode per §E3.6.3") {}

protected:
    void runTest() override
    {
        ASSERT_TRUE(near(eac3SpxCoordinate(15, 2, 0), 2.0 / 4.0 * std::pow(2.0, -15), 1e-12),
                    "exponent 15 takes the mantissa as sent");
        ASSERT_TRUE(near(eac3SpxCoordinate(0, 0, 0), 0.5, 1e-12), "implied leading 1: 4/8");
        ASSERT_TRUE(near(eac3SpxCoordinate(0, 3, 0), 7.0 / 8.0, 1e-12), "7/8 at the top");
        ASSERT_TRUE(near(eac3SpxCoordinate(2, 1, 0), 5.0 / 8.0 / 4.0, 1e-12), "exponent shifts right");
        // Each master step is three more shifts: 2^-3, about 18 dB.
        ASSERT_TRUE(near(eac3SpxCoordinate(1, 1, 2), 5.0 / 8.0 * std::pow(2.0, -7), 1e-12),
                    "master coordinate adds 3 shifts per step");
    }
};

/// §E3.6.4.2.1: the two factors are square roots of a ratio and its
/// complement, so their squares sum to one and noise rises with frequency.
class BlendFactorTest : public TestCase {
public:
    BlendFactorTest() : TestCase("Blend factors are power-complementary and rise with frequency") {}

protected:
    void runTest() override
    {
        EAC3SpxBands bands;
        const uint8_t separate[kSpxSubbands] = {};
        eac3SpxComputeBands(1, 4, 14, separate, bands);
        EAC3SpxChannel channel;
        eac3SpxBlendFactors(8, bands, channel);
        for (unsigned bnd = 0; bnd < bands.count; ++bnd) {
            const double sum = double(channel.noise_blend[bnd]) * channel.noise_blend[bnd]
                             + double(channel.signal_blend[bnd]) * channel.signal_blend[bnd];
            ASSERT_TRUE(near(sum, 1.0, 1e-6), "noise^2 + signal^2 == 1");
            if (bnd > 0) {
                ASSERT_TRUE(channel.noise_blend[bnd] >= channel.noise_blend[bnd - 1],
                            "higher bands carry at least as much noise");
            }
        }
        // First band's ratio by hand: mid-point (73 + 6) / 193 - 8/32.
        const double ratio = (73.0 + 6.0) / 193.0 - 8.0 / 32.0;
        ASSERT_TRUE(near(channel.noise_blend[0], std::sqrt(ratio), 1e-6), "band 0 by hand");
    }
};

/// With no noise and a coordinate of 1/32, synthesis is pure translation, so
/// the extension region must be an exact copy of the baseband -- including
/// restarting from the copy start when a band would overrun it.
class TranslationTest : public TestCase {
public:
    TranslationTest() : TestCase("Translation copies the baseband and wraps at its end") {}

protected:
    void runTest() override
    {
        // Copy region sub-bands 1..3 (bins 37..60, 24 bins); synthesise
        // sub-bands 3..7 (bins 61..108) as bands of 12, 24 and 12 bins.
        uint8_t structure[kSpxSubbands] = {};
        structure[5] = 1;                 // sub-band 5 joins 4: sizes 12, 24, 12
        EAC3SpxBands bands;
        eac3SpxComputeBands(1, 3, 7, structure, bands);
        ASSERT_TRUE(bands.count == 3 && bands.size[1] == 24, "banding as intended");

        EAC3SpxChannel channel;
        for (unsigned bnd = 0; bnd < bands.count; ++bnd) {
            channel.coordinate[bnd] = 1.0f / 32.0f;
            channel.signal_blend[bnd] = 1.0f;
            channel.noise_blend[bnd] = 0.0f;
        }
        float tc[kBlockSamples] = {};
        for (unsigned bin = 0; bin < 61; ++bin) {
            tc[bin] = 0.001f * static_cast<float>(bin + 1);
        }
        EAC3SpxNoise noise;
        eac3SpxSynthesise(tc, bands, channel, noise);

        // Band 0 (61..72) copies 37..48. Band 1 (73..96) needs 24 bins but
        // only 49..60 remain, so it wraps and copies 37..60. Band 2 (97..108)
        // would overrun again from 61, so it restarts at 37.
        auto expect = [&](unsigned insert, unsigned copy, unsigned count, const char* what) {
            for (unsigned i = 0; i < count; ++i) {
                ASSERT_TRUE(near(tc[insert + i], 0.001 * (copy + i + 1), 1e-6), what);
            }
        };
        expect(61, 37, 12, "band 0 copies the start of the baseband");
        expect(73, 37, 24, "band 1 wraps to the copy start");
        expect(97, 37, 12, "band 2 wraps again");
        ASSERT_TRUE(tc[109] == 0.0f, "nothing is written past the extension region");
    }
};

/// §E3.6.4.2.3: five taps, 0 1 2 1 0 of the table row, centred on the first
/// synthesised bin, applied before blending.
class NotchTest : public TestCase {
public:
    NotchTest() : TestCase("The border notch is symmetric about the first synthesised bin") {}

protected:
    void runTest() override
    {
        const uint8_t separate[kSpxSubbands] = {};
        EAC3SpxBands bands;
        eac3SpxComputeBands(0, 3, 4, separate, bands);   // one band, bins 61..72
        EAC3SpxChannel channel;
        channel.coordinate[0] = 1.0f / 32.0f;
        channel.signal_blend[0] = 1.0f;
        channel.attenuate = true;
        channel.attenuation_code = 14;                   // row of 0.5, 0.25, 0.125

        float tc[kBlockSamples] = {};
        for (unsigned bin = 0; bin < 61; ++bin) {
            tc[bin] = 1.0f;
        }
        EAC3SpxNoise noise;
        eac3SpxSynthesise(tc, bands, channel, noise);
        // Bins 59, 60 are baseband; 61, 62, 63 the start of the copy.
        ASSERT_TRUE(near(tc[58], 1.0, 1e-6), "outside the filter");
        ASSERT_TRUE(near(tc[59], 0.5, 1e-6), "tap 0");
        ASSERT_TRUE(near(tc[60], 0.25, 1e-6), "tap 1");
        ASSERT_TRUE(near(tc[61], 0.125, 1e-6), "tap 2, the centre");
        ASSERT_TRUE(near(tc[62], 0.25, 1e-6), "tap 1 again");
        ASSERT_TRUE(near(tc[63], 0.5, 1e-6), "tap 0 again");
        ASSERT_TRUE(near(tc[64], 1.0, 1e-6), "past the filter");
    }
};

/// All noise: each band's output energy should match the translated band's
/// RMS times the coordinate times 32, since the noise has unit variance.
class NoiseEnergyTest : public TestCase {
public:
    NoiseEnergyTest() : TestCase("Noise-only synthesis reproduces the measured band energy") {}

protected:
    void runTest() override
    {
        // Sub-band 7 opens the second band and 8..16 join it, giving bands
        // {6} and {7..16}: 12 and 120 bins.
        const uint8_t merged[kSpxSubbands] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
        EAC3SpxBands bands;
        eac3SpxComputeBands(0, 6, 17, merged, bands);
        ASSERT_TRUE(bands.count == 2 && bands.size[1] == 120, "one wide band to average over");

        EAC3SpxChannel channel;
        channel.coordinate[1] = 0.25f;
        channel.noise_blend[1] = 1.0f;
        channel.signal_blend[1] = 0.0f;
        channel.coordinate[0] = 0.25f;
        channel.noise_blend[0] = 1.0f;

        double measured = 0.0, predicted = 0.0;
        EAC3SpxNoise noise;
        constexpr unsigned kTrials = 200;
        for (unsigned trial = 0; trial < kTrials; ++trial) {
            float tc[kBlockSamples] = {};
            for (unsigned bin = 0; bin < 97; ++bin) {
                tc[bin] = 0.01f * static_cast<float>((bin % 7) + 1);
            }
            // The translated band's RMS, before synthesis overwrites it.
            float copy[kBlockSamples];
            std::copy(tc, tc + kBlockSamples, copy);
            eac3SpxSynthesise(copy, bands, channel, noise);
            double energy = 0.0;
            for (unsigned bin = 109; bin < 229; ++bin) {
                energy += double(copy[bin]) * copy[bin];
            }
            measured += energy / 120.0;

            // Recompute the translated RMS independently of the synthesis.
            // Band 1 needs 120 bins and only 60 remain after band 0's copy,
            // so translation restarts it at the copy start, bin 25, and it
            // wraps within the 72-bin copy region 25..96.
            double translated = 0.0;
            const unsigned from = eac3SpxBandStart(0);
            for (unsigned i = 0; i < 120; ++i) {
                unsigned src = from + i;
                while (src >= 97) {
                    src -= 72;
                }
                translated += double(tc[src]) * tc[src];
            }
            const double rms = std::sqrt(translated / 120.0);
            const double gain = 0.25 * 32.0 * rms;
            predicted += gain * gain;
        }
        const double ratio = measured / predicted;
        ASSERT_TRUE(ratio > 0.9 && ratio < 1.1,
                    "output energy matches rms * coordinate * 32 (ratio " + std::to_string(ratio) + ")");
    }
};

/// The noise source must actually be zero-mean and unit-variance.
class NoiseStatisticsTest : public TestCase {
public:
    NoiseStatisticsTest() : TestCase("Spectral extension noise is zero-mean with unit variance") {}

protected:
    void runTest() override
    {
        EAC3SpxNoise noise;
        double sum = 0.0, squares = 0.0;
        constexpr unsigned kSamples = 65535;
        for (unsigned i = 0; i < kSamples; ++i) {
            const double v = noise.next();
            sum += v;
            squares += v * v;
        }
        const double mean = sum / kSamples;
        const double variance = squares / kSamples - mean * mean;
        ASSERT_TRUE(std::abs(mean) < 0.01, "zero mean (" + std::to_string(mean) + ")");
        ASSERT_TRUE(near(variance, 1.0, 0.01), "unit variance (" + std::to_string(variance) + ")");
    }
};

} // namespace

int main()
{
    TestSuite suite("E-AC-3 Spectral Extension Tests");
    suite.addTest(std::make_unique<BandingTest>());
    suite.addTest(std::make_unique<CoordinateTest>());
    suite.addTest(std::make_unique<BlendFactorTest>());
    suite.addTest(std::make_unique<TranslationTest>());
    suite.addTest(std::make_unique<NotchTest>());
    suite.addTest(std::make_unique<NoiseEnergyTest>());
    suite.addTest(std::make_unique<NoiseStatisticsTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
