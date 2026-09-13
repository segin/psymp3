/*
 * test_eac3_aht.cpp - E-AC-3 Adaptive Hybrid Transform mantissas, A/52 Annex E §E3.4
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * No encoder available to this project emits AHT, so there is no reference
 * decode to compare with. These tests hold the decoder to the quantizer
 * characteristics and transform relations the standard states.
 */

#include "psymp3.h"
#include "test_framework.h"

using namespace TestFramework;
using namespace PsyMP3::Codec::AC3;

namespace {

constexpr double kPi = 3.14159265358979323846;

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
    /// A two's complement value in @p bits.
    void putSigned(int value, unsigned bits) { put(static_cast<uint32_t>(value) & ((1u << bits) - 1), bits); }
    std::vector<uint8_t> finish()
    {
        std::vector<uint8_t> out = m_data;
        out.resize(out.size() + 8, 0);
        return out;
    }

private:
    std::vector<uint8_t> m_data;
    size_t m_count = 0;
};

bool near(double a, double b, double tolerance) { return std::abs(a - b) <= tolerance; }

class GaqSectionsTest : public TestCase {
public:
    GaqSectionsTest() : TestCase("GAQ gain word counts follow §E3.4.2") {}

protected:
    void runTest() override
    {
        ASSERT_TRUE(eac3GaqSections(0, 17) == 0, "mode 0 sends no gains");
        ASSERT_TRUE(eac3GaqSections(1, 17) == 17 && eac3GaqSections(2, 5) == 5, "modes 1 and 2: one per bin");
        ASSERT_TRUE(eac3GaqSections(3, 0) == 0 && eac3GaqSections(3, 1) == 1 &&
                    eac3GaqSections(3, 3) == 1 && eac3GaqSections(3, 4) == 2,
                    "mode 3: three gains per word, rounded up");
    }
};

/// The inverse DCT must undo a DCT-II normalised by 1/6. That relation is
/// what fixes R_0 at 1/2: with 1/sqrt(2) it would not round-trip.
class InverseDctTest : public TestCase {
public:
    InverseDctTest() : TestCase("The AHT inverse DCT undoes a 1/6-normalised DCT-II") {}

protected:
    void runTest() override
    {
        const float original[6] = { 0.31f, -0.72f, 0.05f, 0.44f, -0.18f, 0.9f };
        float x[6];
        for (unsigned j = 0; j < 6; ++j) {
            double sum = 0.0;
            for (unsigned m = 0; m < 6; ++m) {
                sum += original[m] * std::cos(j * (2.0 * m + 1.0) * kPi / 12.0);
            }
            x[j] = static_cast<float>(sum / 6.0);
        }
        float c[6];
        eac3AhtInverseDct(x, c);
        for (unsigned m = 0; m < 6; ++m) {
            ASSERT_TRUE(near(c[m], original[m], 1e-5), "block " + std::to_string(m) + " recovered");
        }
    }
};

/// Table E3.5's quantizer characteristics, checked on real bit sequences.
class GaqDequantizeTest : public TestCase {
public:
    GaqDequantizeTest() : TestCase("GAQ mantissas decode to Table E3.5's reconstruction points") {}

protected:
    void runTest() override
    {
        // Gk = 1, hebap 8 (m = 3): a symmetric 7-level quantizer, step 2/7.
        {
            BitWriter w;
            w.putSigned(1, 3);
            w.putSigned(-3, 3);
            const auto bytes = w.finish();
            AC3BitReader r(bytes.data(), bytes.size());
            ASSERT_TRUE(near(eac3GaqDequantize(r, 8, 1), 2.0 / 7.0, 1e-4), "code 1 is 2/7");
            ASSERT_TRUE(near(eac3GaqDequantize(r, 8, 1), -6.0 / 7.0, 1e-4), "code -3 is -6/7");
            ASSERT_TRUE(r.tell() == 6, "three bits each, no tag");
        }
        // Gk = 2, hebap 8: small codewords are m - 1 = 2 bits, step 1/4;
        // the tag '10' is followed by a 2-bit large mantissa.
        {
            BitWriter w;
            w.putSigned(1, 2);                // small: 0.5 / 2
            w.putSigned(-2, 2);               // tag
            w.putSigned(1, 2);                // large: x = 0.5
            const auto bytes = w.finish();
            AC3BitReader r(bytes.data(), bytes.size());
            ASSERT_TRUE(near(eac3GaqDequantize(r, 8, 2), 0.25, 1e-6), "small mantissa attenuated by 2");
            ASSERT_TRUE(r.tell() == 2, "a small mantissa is one codeword");
            const double a = static_cast<int16_t>(0xd555) / 32768.0, b = 0x4000 / 32768.0;
            ASSERT_TRUE(near(eac3GaqDequantize(r, 8, 2), 0.5 + a * 0.5 + b, 1e-6), "large mantissa remapped");
            ASSERT_TRUE(r.tell() == 6, "the tag and the large codeword");
        }
        // Gk = 4, hebap 9 (m = 4): small codewords are m - 2 = 2 bits, step
        // 1/8; a large one is m = 4 bits after the tag.
        {
            BitWriter w;
            w.putSigned(-1, 2);
            w.putSigned(-2, 2);
            w.putSigned(-3, 4);
            const auto bytes = w.finish();
            AC3BitReader r(bytes.data(), bytes.size());
            ASSERT_TRUE(near(eac3GaqDequantize(r, 9, 4), -0.125, 1e-6), "small mantissa attenuated by 4");
            const double x = -3.0 / 8.0;
            const double a = static_cast<int16_t>(0xe666) / 32768.0, b = static_cast<int16_t>(0xeccd) / 32768.0;
            ASSERT_TRUE(near(eac3GaqDequantize(r, 9, 4), x + a * x + b, 1e-6),
                        "negative large mantissa takes the negative offset");
            ASSERT_TRUE(r.tell() == 8, "2 + 2 + 4 bits");
        }
    }
};

/// A vector-quantized bin: one index, the codebook row, the inverse DCT, and
/// the exponent -- and a zero-allocation bin that reads nothing.
class VqChannelTest : public TestCase {
public:
    VqChannelTest() : TestCase("A VQ bin reads one index and expands to six blocks") {}

protected:
    void runTest() override
    {
        uint8_t hebap[256] = {};
        uint8_t exponents[256] = {};
        hebap[0] = 1;                         // 2-bit index
        hebap[1] = 0;                         // nothing coded
        exponents[0] = 2;

        BitWriter w;
        w.put(0, 2);                          // gaqmod: none
        w.put(3, 2);                          // index 3
        const auto bytes = w.finish();
        AC3BitReader r(bytes.data(), bytes.size());
        EAC3AhtSpectrum spectrum;
        const char* why = nullptr;
        ASSERT_TRUE(eac3AhtReadChannel(r, hebap, 0, 2, exponents, spectrum, &why),
                    std::string("reads: ") + (why ? why : ""));
        ASSERT_TRUE(r.tell() == 4, "gaqmod and one 2-bit index");

        for (unsigned m = 0; m < 6; ++m) {
            double c = 0.5 * kAhtVq1[3][0] / 32768.0;
            for (unsigned j = 1; j < 6; ++j) {
                c += kAhtVq1[3][j] / 32768.0 * std::cos(j * (2.0 * m + 1.0) * kPi / 12.0);
            }
            c = 2.0 * c / 4.0;                // exponent 2
            ASSERT_TRUE(near(spectrum.value[m][0], c, 1e-5), "bin 0, block " + std::to_string(m));
            ASSERT_TRUE(spectrum.value[m][1] == 0.0f, "an unallocated bin is silent");
        }
    }
};

/// Gains apply only to active bins, and bins above endbap decode at unit gain.
class GaqChannelTest : public TestCase {
public:
    GaqChannelTest() : TestCase("GAQ gains attach to active bins in order") {}

protected:
    void runTest() override
    {
        uint8_t hebap[256] = {};
        uint8_t exponents[256] = {};
        hebap[0] = 9;                         // m = 4, active (below endbap 12)
        hebap[1] = 12;                        // m = 7, at endbap: unit gain, no gain word

        BitWriter w;
        w.put(1, 2);                          // gaqmod 1: 1-bit gains of 1 or 2
        w.put(1, 1);                          // bin 0 takes Gk = 2
        for (int j = 0; j < 6; ++j) {
            w.putSigned(1, 3);                // small: 0.25 / 2
        }
        for (int j = 0; j < 6; ++j) {
            w.putSigned(0, 7);                // zero
        }
        const auto bytes = w.finish();
        AC3BitReader r(bytes.data(), bytes.size());
        EAC3AhtSpectrum spectrum;
        const char* why = nullptr;
        ASSERT_TRUE(eac3AhtReadChannel(r, hebap, 0, 2, exponents, spectrum, &why),
                    std::string("reads: ") + (why ? why : ""));
        ASSERT_TRUE(r.tell() == 2 + 1 + 6 * 3 + 6 * 7, "gaqmod, one gain, and both bins' mantissas");

        for (unsigned m = 0; m < 6; ++m) {
            double c = 0.5 * 0.125;
            for (unsigned j = 1; j < 6; ++j) {
                c += 0.125 * std::cos(j * (2.0 * m + 1.0) * kPi / 12.0);
            }
            ASSERT_TRUE(near(spectrum.value[m][0], 2.0 * c, 1e-5), "bin 0, block " + std::to_string(m));
            ASSERT_TRUE(near(spectrum.value[m][1], 0.0, 1e-6), "bin 1 zero");
        }
    }
};

} // namespace

int main()
{
    TestSuite suite("E-AC-3 Adaptive Hybrid Transform Tests");
    suite.addTest(std::make_unique<GaqSectionsTest>());
    suite.addTest(std::make_unique<InverseDctTest>());
    suite.addTest(std::make_unique<GaqDequantizeTest>());
    suite.addTest(std::make_unique<VqChannelTest>());
    suite.addTest(std::make_unique<GaqChannelTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
