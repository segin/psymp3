/*
 * test_wav_g711_improved.cpp - Improved unit tests for G.711 conversion utilities
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "test_framework.h"
#include "core/utility/G711.h"

using namespace TestFramework;
using namespace PsyMP3::Core::Utility::G711;

class TestALawConversion : public TestCase {
public:
    TestALawConversion() : TestCase("A-Law Conversion") {}

    void runTest() override {
        // A-law is mid-riser: no code decodes to 0. The near-silence codes
        // 0x55/0xD5 decode to -8/+8 per ITU-T G.711 Table 2 (with the +8
        // interval-midpoint offset of the Sun reference implementation).
        ASSERT_EQUALS(-8, alaw2linear(0x55), "Near-silence 0x55 -> -8");
        ASSERT_EQUALS(8, alaw2linear(0xD5), "Near-silence 0xD5 -> +8");

        // Chord/step expansion for large values
        ASSERT_EQUALS(5504, alaw2linear(0x80), "0x80 -> 5504");
        ASSERT_EQUALS(-5504, alaw2linear(0x00), "0x00 -> -5504");

        // Mid-range values
        ASSERT_EQUALS(-848, alaw2linear(0x7F), "Value 0x7F -> -848");
        ASSERT_EQUALS(848, alaw2linear(0xFF), "Value 0xFF -> 848");
    }
};

class TestMuLawConversion : public TestCase {
public:
    TestMuLawConversion() : TestCase("Mu-Law Conversion") {}

    void runTest() override {
        // Mu-law silence is encoded as 0xFF.
        // Decoded linear value should be 0.
        ASSERT_EQUALS(0, ulaw2linear(0xFF), "Silence 0xFF -> 0");

        // 0x7F is the negative zero/silence in mu-law (sign bit flipped compared to 0xFF)
        ASSERT_EQUALS(0, ulaw2linear(0x7F), "Silence 0x7F -> 0");

        // Maximum positive value in Mu-law (0x80) decodes to 32124.
        // Mu-law has a larger dynamic range than A-law.
        ASSERT_EQUALS(32124, ulaw2linear(0x80), "Max Positive (0x80) -> 32124");

        // Maximum negative value in Mu-law (0x00) decodes to -32124.
        ASSERT_EQUALS(-32124, ulaw2linear(0x00), "Max Negative (0x00) -> -32124");

        // Test mid-range values
        // 0x8F decodes to 16764
        ASSERT_EQUALS(16764, ulaw2linear(0x8F), "Value 0x8F -> 16764");
        // 0x0F decodes to -16764
        ASSERT_EQUALS(-16764, ulaw2linear(0x0F), "Value 0x0F -> -16764");
    }
};

int main() {
    TestSuite suite("G.711 Conversion Tests (Improved)");
    suite.addTest(std::make_unique<TestALawConversion>());
    suite.addTest(std::make_unique<TestMuLawConversion>());

    auto results = suite.runAll();
    suite.printResults(results);

    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}
