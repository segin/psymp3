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
        // A-law silence is encoded as 0x55 (or 0xD5 with alternate sign bit)
        // Decoded linear value should be 0.
        ASSERT_EQUALS(0, alaw2linear(0x55), "Silence 0x55 -> 0");
        ASSERT_EQUALS(0, alaw2linear(0xD5), "Silence 0xD5 -> 0");

        // Maximum positive value in A-law is 0x80, which decodes to 5376.
        // This validates the exponent/mantissa expansion logic for large positive values.
        ASSERT_EQUALS(5376, alaw2linear(0x80), "Max Positive (0x80) -> 5376");

        // Maximum negative value in A-law is 0x00, which decodes to -5376.
        // This validates the sign bit handling and expansion for large negative values.
        ASSERT_EQUALS(-5376, alaw2linear(0x00), "Max Negative (0x00) -> -5376");

        // Test mid-range values to verify non-boundary decoding
        // 0x7F decodes to -832
        ASSERT_EQUALS(-832, alaw2linear(0x7F), "Value 0x7F -> -832");
        // 0xFF decodes to 832
        ASSERT_EQUALS(832, alaw2linear(0xFF), "Value 0xFF -> 832");
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
