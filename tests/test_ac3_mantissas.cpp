/*
 * test_ac3_mantissas.cpp - AC-3 mantissa dequantization (A/52 §7.3)
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

bool near(float a, float b) { return std::fabs(a - b) < 1e-5f; }

/// Bits written most significant first, as A/52 §5.3 requires.
class BitWriter {
public:
    void put(uint32_t value, unsigned bits)
    {
        while (bits-- > 0) {
            if (m_count % 8 == 0) { m_data.push_back(0); }
            if ((value >> bits) & 1u) {
                m_data.back() |= static_cast<uint8_t>(0x80 >> (m_count % 8));
            }
            ++m_count;
        }
    }
    std::vector<uint8_t> finish() { m_data.resize(m_data.size() + 4, 0); return m_data; }

private:
    std::vector<uint8_t> m_data;
    size_t m_count = 0;
};

class SymmetricTableTest : public TestCase {
public:
    SymmetricTableTest() : TestCase("Symmetric quantizers match the printed tables") {}

protected:
    void runTest() override
    {
        // The values the standard prints in Tables 7.19 to 7.23. They are
        // computed here from the closed form rather than transcribed, so this
        // case is what shows the two agree.
        ASSERT_TRUE(near(ac3SymmetricMantissa(1, 0), -2.0f/3.0f), "bap 1 code 0");
        ASSERT_TRUE(near(ac3SymmetricMantissa(1, 1), 0.0f), "bap 1 code 1 is silence");
        ASSERT_TRUE(near(ac3SymmetricMantissa(1, 2), 2.0f/3.0f), "bap 1 code 2");

        ASSERT_TRUE(near(ac3SymmetricMantissa(2, 0), -4.0f/5.0f), "bap 2 code 0");
        ASSERT_TRUE(near(ac3SymmetricMantissa(2, 2), 0.0f), "bap 2 centre");
        ASSERT_TRUE(near(ac3SymmetricMantissa(2, 4), 4.0f/5.0f), "bap 2 code 4");

        ASSERT_TRUE(near(ac3SymmetricMantissa(3, 0), -6.0f/7.0f), "bap 3 code 0");
        ASSERT_TRUE(near(ac3SymmetricMantissa(3, 3), 0.0f), "bap 3 centre");
        ASSERT_TRUE(near(ac3SymmetricMantissa(3, 6), 6.0f/7.0f), "bap 3 code 6");

        ASSERT_TRUE(near(ac3SymmetricMantissa(4, 0), -10.0f/11.0f), "bap 4 code 0");
        ASSERT_TRUE(near(ac3SymmetricMantissa(4, 5), 0.0f), "bap 4 centre");
        ASSERT_TRUE(near(ac3SymmetricMantissa(4, 10), 10.0f/11.0f), "bap 4 code 10");

        ASSERT_TRUE(near(ac3SymmetricMantissa(5, 0), -14.0f/15.0f), "bap 5 code 0");
        ASSERT_TRUE(near(ac3SymmetricMantissa(5, 7), 0.0f), "bap 5 centre");
        ASSERT_TRUE(near(ac3SymmetricMantissa(5, 14), 14.0f/15.0f), "bap 5 code 14");

        // Every quantizer is centred and symmetric: the extremes mirror, and
        // no code reaches 1.0 -- the outermost is (N-1)/N.
        for (uint8_t bap = 1; bap <= 5; ++bap) {
            const unsigned levels = (bap == 1) ? 3u : (bap == 2) ? 5u
                                  : (bap == 3) ? 7u : (bap == 4) ? 11u : 15u;
            ASSERT_TRUE(near(ac3SymmetricMantissa(bap, 0),
                             -ac3SymmetricMantissa(bap, levels - 1)),
                        "the extremes mirror each other");
            ASSERT_TRUE(std::fabs(ac3SymmetricMantissa(bap, 0)) < 1.0f,
                        "and no code reaches full scale");
        }
    }
};

class GroupWidthTest : public TestCase {
public:
    GroupWidthTest() : TestCase("Grouped baps pack several mantissas into one codeword") {}

protected:
    void runTest() override
    {
        // A/52 Table 7.18. The packing works because the levels raised to the
        // group size fit inside the group's bits.
        ASSERT_TRUE(ac3MantissaGroupBits(1) == 5 && ac3MantissaGroupSize(1) == 3,
                    "three 3-level codes in five bits");
        ASSERT_TRUE(3 * 3 * 3 <= (1 << 5), "27 codes fit in 32");
        ASSERT_TRUE(ac3MantissaGroupBits(2) == 7 && ac3MantissaGroupSize(2) == 3,
                    "three 5-level codes in seven bits");
        ASSERT_TRUE(5 * 5 * 5 <= (1 << 7), "125 codes fit in 128");
        ASSERT_TRUE(ac3MantissaGroupBits(4) == 7 && ac3MantissaGroupSize(4) == 2,
                    "two 11-level codes in seven bits");
        ASSERT_TRUE(11 * 11 <= (1 << 7), "121 codes fit in 128");

        // The rest are one mantissa apiece.
        ASSERT_TRUE(ac3MantissaGroupSize(3) == 1 && ac3MantissaGroupBits(3) == 3, "bap 3");
        ASSERT_TRUE(ac3MantissaGroupSize(5) == 1 && ac3MantissaGroupBits(5) == 4, "bap 5");
        ASSERT_TRUE(ac3MantissaGroupSize(15) == 1 && ac3MantissaGroupBits(15) == 16, "bap 15");
        ASSERT_TRUE(ac3MantissaGroupSize(0) == 0, "bap 0 codes nothing");
    }
};

class GroupingTest : public TestCase {
public:
    GroupingTest() : TestCase("A group is read once and handed out to its members") {}

protected:
    void runTest() override
    {
        // The heart of §7.3: the codeword appears at the first mantissa of the
        // group, and the ones after it read nothing at all. A reader that
        // fetched bits for each would eat the next mantissa's bits and put
        // every later coefficient in the wrong place.
        BitWriter w;
        const unsigned codes[3] = {0, 1, 2};
        w.put(codes[0] * 9 + codes[1] * 3 + codes[2], 5);  // one bap-1 group
        w.put(0x2A, 6);                                     // a bap-7 mantissa after it
        auto data = w.finish();

        AC3BitReader reader(data.data(), data.size());
        AC3MantissaReader mantissas;

        const float first  = mantissas.next(reader, 1, 0);
        const size_t after_first = reader.tell();
        const float second = mantissas.next(reader, 1, 0);
        const float third  = mantissas.next(reader, 1, 0);

        ASSERT_TRUE(after_first == 5, "the whole group is read at the first member");
        ASSERT_TRUE(reader.tell() == 5,
                    "and the second and third members consume no further bits");
        ASSERT_TRUE(near(first, -2.0f/3.0f), "code 0");
        ASSERT_TRUE(near(second, 0.0f), "code 1");
        ASSERT_TRUE(near(third, 2.0f/3.0f), "code 2");

        // The mantissa after the group must start where the group ended.
        mantissas.next(reader, 7, 0);
        ASSERT_TRUE(reader.tell() == 11, "the next mantissa follows the group exactly");
    }
};

class AsymmetricTest : public TestCase {
public:
    AsymmetricTest() : TestCase("Asymmetric mantissas are two's complement fractions") {}

protected:
    void runTest() override
    {
        // A/52 §7.3.2: the point sits left of the most significant bit, so an
        // n-bit word spans -1.0 to 1.0 - 2^-(n-1), and the top bit is the sign.
        BitWriter w;
        w.put(0x00, 5);   // 0
        w.put(0x0F, 5);   // largest positive, 15/16
        w.put(0x10, 5);   // most negative, -1.0
        w.put(0x1F, 5);   // -1/16
        auto data = w.finish();

        AC3BitReader reader(data.data(), data.size());
        AC3MantissaReader mantissas;
        ASSERT_TRUE(near(mantissas.next(reader, 6, 0), 0.0f), "zero");
        ASSERT_TRUE(near(mantissas.next(reader, 6, 0), 15.0f/16.0f), "the largest positive");
        ASSERT_TRUE(near(mantissas.next(reader, 6, 0), -1.0f), "and the most negative is -1");
        ASSERT_TRUE(near(mantissas.next(reader, 6, 0), -1.0f/16.0f), "a small negative");

        ASSERT_TRUE(ac3MantissaGroupSize(6) == 1, "and none of them are grouped");
    }
};

class ExponentScalingTest : public TestCase {
public:
    ExponentScalingTest() : TestCase("The exponent divides the mantissa by a power of two") {}

protected:
    void runTest() override
    {
        // transform_coefficient = mantissa >> exponent. A larger exponent means
        // a quieter bin, which is why the power spectral density of §7.2
        // subtracts it.
        for (uint8_t exponent : {0, 1, 4, 12}) {
            BitWriter w;
            w.put(0x0F, 5);   // 15/16
            auto data = w.finish();
            AC3BitReader reader(data.data(), data.size());
            AC3MantissaReader mantissas;
            const float value = mantissas.next(reader, 6, exponent);
            ASSERT_TRUE(near(value, (15.0f/16.0f) / static_cast<float>(1u << exponent)),
                        "each step of the exponent halves the coefficient");
        }
    }
};

class ResetTest : public TestCase {
public:
    ResetTest() : TestCase("A part-used group does not survive into the next exponent set") {}

protected:
    void runTest() override
    {
        // Grouping state is per exponent set. Carrying a half-consumed group
        // into the next channel would hand it that channel's first mantissa
        // from the previous one's codeword.
        BitWriter w;
        w.put(0 * 9 + 1 * 3 + 2, 5);
        w.put(2 * 9 + 1 * 3 + 0, 5);
        auto data = w.finish();

        AC3BitReader reader(data.data(), data.size());
        AC3MantissaReader mantissas;
        mantissas.next(reader, 1, 0);   // takes the first of three
        ASSERT_TRUE(reader.tell() == 5, "one group read");

        mantissas.reset();
        const float value = mantissas.next(reader, 1, 0);
        ASSERT_TRUE(reader.tell() == 10, "after a reset the next group is read fresh");
        ASSERT_TRUE(near(value, 2.0f/3.0f),
                    "and its value comes from the new codeword, not the abandoned one");
    }
};

class ZeroBapTest : public TestCase {
public:
    ZeroBapTest() : TestCase("A bap of zero consumes no bits") {}

protected:
    void runTest() override
    {
        // §7.3.4 fills these with dither, which needs the block's dithflag.
        // Whatever the value, it must not read from the stream: a bin with no
        // bits allocated has none to read.
        BitWriter w;
        w.put(0x2A, 6);
        auto data = w.finish();
        AC3BitReader reader(data.data(), data.size());
        AC3MantissaReader mantissas;

        mantissas.next(reader, 0, 5);
        ASSERT_TRUE(reader.tell() == 0, "nothing is consumed for an unallocated bin");
        mantissas.next(reader, 7, 0);
        ASSERT_TRUE(reader.tell() == 6, "and the next real mantissa starts at bit zero");
    }
};

} // namespace

int main()
{
    TestSuite suite("AC-3 Mantissa Tests");
    suite.addTest(std::make_unique<SymmetricTableTest>());
    suite.addTest(std::make_unique<GroupWidthTest>());
    suite.addTest(std::make_unique<GroupingTest>());
    suite.addTest(std::make_unique<AsymmetricTest>());
    suite.addTest(std::make_unique<ExponentScalingTest>());
    suite.addTest(std::make_unique<ResetTest>());
    suite.addTest(std::make_unique<ZeroBapTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
