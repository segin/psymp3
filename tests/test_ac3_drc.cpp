/*
 * test_ac3_drc.cpp - AC-3 dynamic range control words, A/52 §7.7.1
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * ffmpeg's encoder writes no dynamic range words, so the corpus cannot show
 * whether they are honoured. These tests pin the decoding to the worked
 * values of Table 7.29 instead.
 */

#include "psymp3.h"
#include "test_framework.h"

using namespace TestFramework;
using namespace PsyMP3::Codec::AC3;

namespace {

double db(double gain) { return 20.0 * std::log10(gain); }

class GainWordTest : public TestCase {
public:
    GainWordTest() : TestCase("dynrng words decode to Table 7.29's gains") {}

protected:
    void runTest() override
    {
        // '0000 0000' is unity: X = 0 gives +6.02 dB, Y = 0 gives -6.02 dB.
        ASSERT_TRUE(ac3DynamicRangeGain(0x00) == 1.0f, "0x00 is exactly unity");

        // X = 3 is +24.08 dB; with Y = 0 the word is +18.06 dB, a factor of 8.
        ASSERT_TRUE(std::abs(ac3DynamicRangeGain(0x60) - 8.0f) < 1e-6f, "0x60 is x8");
        // The largest boost: X = 3, Y = 31, 24.08 - 0.14 = +23.95 dB.
        ASSERT_TRUE(std::abs(db(ac3DynamicRangeGain(0x7F)) - 23.95) < 0.02, "0x7F is +23.95 dB");
        // The largest cut: X = -4, Y = 0, -18.06 - 6.02 = -24.08 dB.
        ASSERT_TRUE(std::abs(ac3DynamicRangeGain(0x80) - 1.0f / 16.0f) < 1e-7f, "0x80 is 1/16");
        // X = -1 contributes nothing, so 0xE0 is Y alone at its smallest: -6.02 dB.
        ASSERT_TRUE(std::abs(ac3DynamicRangeGain(0xE0) - 0.5f) < 1e-7f, "0xE0 is one half");
        ASSERT_TRUE(std::abs(ac3DynamicRangeGain(0xFF) - 63.0f / 64.0f) < 1e-7f, "0xFF is 63/64");

        // Monotone within each shift, and each shift step is 6.02 dB.
        for (int word = 0; word < 256; ++word) {
            if ((word & 0x1F) != 0x1F) {
                ASSERT_TRUE(ac3DynamicRangeGain(static_cast<uint8_t>(word + 1)) >
                            ac3DynamicRangeGain(static_cast<uint8_t>(word)),
                            "Y raises the gain within a shift");
            }
        }
        ASSERT_TRUE(std::abs(db(ac3DynamicRangeGain(0x20)) - db(ac3DynamicRangeGain(0x00)) - 6.02) < 0.01,
                    "one step of X is 6.02 dB");
    }
};

} // namespace

int main()
{
    TestSuite suite("AC-3 Dynamic Range Control Tests");
    suite.addTest(std::make_unique<GainWordTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
