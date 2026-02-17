/*
 * test_wav_g711_improved.cpp - Improved unit tests for G.711 conversion utilities
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
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
        // Silence (0) -> 0x55
        ASSERT_EQUALS(0, alaw2linear(0x55), "Silence 0x55 -> 0");

        // Silence (0) -> 0xD5 (with sign bit flipped)
        ASSERT_EQUALS(0, alaw2linear(0xD5), "Silence 0xD5 -> 0");

        // Max Positive (0x80) -> 5376
        // Verified from existing test
        ASSERT_EQUALS(5376, alaw2linear(0x80), "Max Positive (0x80) -> 5376");

        // Max Negative (0x00) -> -5376
        // Verified from existing test
        ASSERT_EQUALS(-5376, alaw2linear(0x00), "Max Negative (0x00) -> -5376");

        // Other values (0x7F -> -832, 0xFF -> 832)
        // Verified from existing test
        ASSERT_EQUALS(-832, alaw2linear(0x7F), "Value 0x7F -> -832");
        ASSERT_EQUALS(832, alaw2linear(0xFF), "Value 0xFF -> 832");
    }
};

class TestMuLawConversion : public TestCase {
public:
    TestMuLawConversion() : TestCase("Mu-Law Conversion") {}

    void runTest() override {
        // Silence (0) -> 0xFF
        ASSERT_EQUALS(0, ulaw2linear(0xFF), "Silence 0xFF -> 0");

        // Silence (0) -> 0x7F (with sign bit flipped)
        ASSERT_EQUALS(0, ulaw2linear(0x7F), "Silence 0x7F -> 0");

        // Max Positive (0x80) -> 32124
        // Verified from existing test
        ASSERT_EQUALS(32124, ulaw2linear(0x80), "Max Positive (0x80) -> 32124");

        // Max Negative (0x00) -> -32124
        // Verified from existing test
        ASSERT_EQUALS(-32124, ulaw2linear(0x00), "Max Negative (0x00) -> -32124");

        // Mid values (0x8F -> 16764, 0x0F -> -16764)
        // Verified from existing test
        ASSERT_EQUALS(16764, ulaw2linear(0x8F), "Value 0x8F -> 16764");
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
