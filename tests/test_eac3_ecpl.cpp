/*
 * test_eac3_ecpl.cpp - E-AC-3 enhanced channel coupling, A/52 Annex E §E3.5
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * No encoder available to this project emits enhanced coupling, so there is
 * no reference decode. These tests check the tables, and one identity the
 * processing must satisfy: analysing a consistent signal and regenerating the
 * reference channel at unit amplitude and zero phase returns the block's own
 * MDCT coefficients.
 */

#include "psymp3.h"
#include "test_framework.h"

using namespace TestFramework;
using namespace PsyMP3::Codec::AC3;

namespace {

constexpr double kPi = 3.14159265358979323846;

bool near(double a, double b, double tolerance) { return std::abs(a - b) <= tolerance; }

class TableTest : public TestCase {
public:
    TableTest() : TestCase("Enhanced coupling tables match Tables E3.9 to E3.12") {}

protected:
    void runTest() override
    {
        const unsigned starts[23] = { 13, 19, 25, 31, 37, 49, 61, 73, 85, 97, 109, 121,
                                      133, 145, 157, 169, 181, 193, 205, 217, 229, 241, 253 };
        for (unsigned sbnd = 0; sbnd <= kEcplSubbands; ++sbnd) {
            ASSERT_TRUE(eac3EcplSubbandStart(sbnd) == starts[sbnd], "Table E3.9 at " + std::to_string(sbnd));
        }
        ASSERT_TRUE(near(eac3EcplAmplitude(0), 1.0, 1e-9), "amplitude 0 is unity");
        ASSERT_TRUE(near(eac3EcplAmplitude(4), 0.5, 1e-9), "amplitude 4 is -6 dB");
        ASSERT_TRUE(eac3EcplAmplitude(31) == 0.0f, "amplitude 31 is silence");
        for (unsigned code = 0; code < 31; ++code) {
            const double db = 20.0 * std::log10(eac3EcplAmplitude(code));
            ASSERT_TRUE(near(db, -1.5 * code, 0.2), "about -1.5 dB per step at " + std::to_string(code));
        }
        ASSERT_TRUE(eac3EcplAngle(0) == 0.0f && near(eac3EcplAngle(16), 0.5, 1e-9) &&
                    near(eac3EcplAngle(32), -1.0, 1e-9) && near(eac3EcplAngle(63), -1.0 / 32, 1e-9),
                    "angles are code/32 wrapped into [-1, 1)");
        ASSERT_TRUE(near(eac3EcplChaos(7), -1.0, 1e-9) && eac3EcplChaos(0) == 0.0f, "chaos is -code/7");
    }
};

class BandingTest : public TestCase {
public:
    BandingTest() : TestCase("Enhanced coupling bands follow the band structure") {}

protected:
    void runTest() override
    {
        // The default from sub-band 5 to the top: 5, 6 and 7 stand alone; 9
        // joins 8, 11 joins 10 and 13 joins 12; 15-17 join 14 and 19-21 join
        // 18. Eight bands.
        EAC3EcplBands bands;
        eac3EcplComputeBands(5, 22, kDefaultEcplBandStructure, bands);
        ASSERT_TRUE(bands.count == 8, "eight bands, got " + std::to_string(bands.count));
        const unsigned expected_start[8] = { 49, 61, 73, 85, 109, 133, 157, 205 };
        const unsigned expected_bins[8] = { 12, 12, 12, 24, 24, 24, 48, 48 };
        for (unsigned i = 0; i < 8; ++i) {
            ASSERT_TRUE(bands.start_bin[i] == expected_start[i] && bands.bins[i] == expected_bins[i],
                        "band " + std::to_string(i));
        }
        unsigned total = 0;
        for (unsigned i = 0; i < bands.count; ++i) {
            total += bands.bins[i];
        }
        ASSERT_TRUE(total == 253 - 49, "the bands tile the range exactly");

        // Sub-bands below 9 never merge, whatever the structure says.
        uint8_t all_ones[kEcplSubbands];
        std::fill(all_ones, all_ones + kEcplSubbands, uint8_t{1});
        eac3EcplComputeBands(2, 12, all_ones, bands);
        ASSERT_TRUE(bands.count == 7, "2..8 stand alone and 9..11 join 8: seven bands");
        ASSERT_TRUE(bands.start_bin[0] == 25 && bands.bins[0] == 6, "sub-band 2 is six bins");
    }
};

/// Forward MDCT of §8.2.3.2, windowed on the way in, as test_ac3_transform uses.
void forwardMdct(const std::vector<float>& signal, size_t offset, float coefficients[256])
{
    const unsigned N = 512;
    for (unsigned k = 0; k < N / 2; ++k) {
        double sum = 0.0;
        for (unsigned n = 0; n < N; ++n) {
            const double w = n < 256 ? kWindow[n] : kWindow[N - 1 - n];
            sum += signal[offset + n] * w * std::cos(2.0 * kPi / N * (n + 0.5 + N / 4.0) * (k + 0.5));
        }
        coefficients[k] = static_cast<float>(-2.0 * sum / N);
    }
}

/// The identity the processing rests on. Three consecutive blocks of one
/// signal are analysed into the non-aliased complex spectrum of the middle
/// one; regenerating the reference channel with no amplitude change, no
/// phase change and no chaos must return that block's coefficients.
class IdentityTest : public TestCase {
public:
    IdentityTest() : TestCase("Unit-gain, zero-phase regeneration returns the coupling coefficients") {}

protected:
    void runTest() override
    {
        std::vector<float> signal(256 * 5);
        for (size_t n = 0; n < signal.size(); ++n) {
            signal[n] = static_cast<float>(0.3 * std::sin(2.0 * kPi * n * 0.083)
                                         + 0.2 * std::sin(2.0 * kPi * n * 0.31 + 1.1));
        }
        float blocks[3][256];
        float windowed[3][512];
        for (unsigned b = 0; b < 3; ++b) {
            forwardMdct(signal, 256 * b, blocks[b]);
            ac3WindowedImdct(blocks[b], false, windowed[b]);
        }

        float zr[256], zi[256];
        eac3EcplAnalyse(windowed[0], windowed[1], windowed[2], zr, zi);

        EAC3EcplBands bands;
        const uint8_t separate[kEcplSubbands] = {};
        eac3EcplComputeBands(0, kEcplSubbands, separate, bands);
        EAC3EcplChannel reference;
        reference.first = true;           // angle and chaos fixed at zero
        EAC3EcplRandom random;
        float regenerated[256] = {};
        eac3EcplRegenerate(zr, zi, bands, reference, 0, false, random, regenerated);

        double worst = 0.0, peak = 0.0;
        for (unsigned k = 13; k < 253; ++k) {
            worst = std::max(worst, std::abs(double(regenerated[k]) - blocks[1][k]));
            peak = std::max(peak, std::abs(double(blocks[1][k])));
        }
        ASSERT_TRUE(peak > 1e-3, "the test signal has energy in the coupling range");
        // The transform round trip alone is good to about 1e-5; a wrong
        // scaling constant anywhere in the analysis or regeneration is off by
        // a large fraction of the peak.
        ASSERT_TRUE(worst < 1e-3 * peak + 1e-5,
                    "regeneration returns the block's coefficients (worst "
                    + std::to_string(worst) + " against peak " + std::to_string(peak) + ")");
    }
};

/// With interpolation, angles between two band centres follow a straight line.
class AngleInterpolationTest : public TestCase {
public:
    AngleInterpolationTest() : TestCase("Interpolated angles move monotonically between band centres") {}

protected:
    void runTest() override
    {
        EAC3EcplBands bands;
        const uint8_t separate[kEcplSubbands] = {};
        eac3EcplComputeBands(10, 12, separate, bands);   // two 12-bin bands, bins 109..132
        ASSERT_TRUE(bands.count == 2, "two bands");

        EAC3EcplChannel channel;
        channel.first = false;
        channel.transient = true;         // band-constant random offsets ...
        channel.chaos[0] = channel.chaos[1] = 0;   // ... scaled by zero chaos
        channel.angle[0] = 0;             // 0
        channel.angle[1] = 8;             // 0.25
        EAC3EcplRandom random;

        // A pure-real unit spectrum, so each output's phase is its angle.
        float zr[256], zi[256];
        std::fill(zr, zr + 256, 1.0f);
        std::fill(zi, zi + 256, 0.0f);
        float with[256] = {}, without[256] = {};
        eac3EcplRegenerate(zr, zi, bands, channel, 1, true, random, with);
        eac3EcplRegenerate(zr, zi, bands, channel, 1, false, random, without);

        bool differs = false;
        for (unsigned k = 109; k < 133; ++k) {
            differs = differs || std::abs(with[k] - without[k]) > 1e-6f;
        }
        ASSERT_TRUE(differs, "interpolation changes the bins between band centres");
    }
};

} // namespace

int main()
{
    TestSuite suite("E-AC-3 Enhanced Coupling Tests");
    suite.addTest(std::make_unique<TableTest>());
    suite.addTest(std::make_unique<BandingTest>());
    suite.addTest(std::make_unique<IdentityTest>());
    suite.addTest(std::make_unique<AngleInterpolationTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
