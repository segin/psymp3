/*
 * test_utility_unit.cpp - Unit tests for core utility functions
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#define FINAL_BUILD
#endif

#include "test_framework.h"
#include "core/utility/utility.h"
#include <cmath>
#include <iostream>

using namespace PsyMP3::Core::Utility;
using namespace TestFramework;

// ============================================================================
// Logarithmic Scale Tests
// ============================================================================

class LogarithmicScaleTest : public TestCase {
public:
    LogarithmicScaleTest() : TestCase("Utility::logarithmicScale") {}

protected:
    void runTest() override {
        // Test clamping of x
        ASSERT_EQUALS(0.0f, logarithmicScale(1, -0.5f), "Should clamp negative x to 0.0");
        ASSERT_EQUALS(1.0f, logarithmicScale(1, 1.5f), "Should clamp x > 1.0 to 1.0");

        // Test f <= 0 (identity)
        ASSERT_EQUALS(0.5f, logarithmicScale(0, 0.5f), "f=0 should return x unchanged");
        ASSERT_EQUALS(0.5f, logarithmicScale(-1, 0.5f), "f<0 should return x unchanged");

        // Test f < 5 (LUT path)
        // f=1, x=0.0 -> log10(1 + 0) = 0
        ASSERT_EQUALS(0.0f, logarithmicScale(1, 0.0f), "f=1, x=0 should be 0");

        // f=1, x=1.0 -> log10(1 + 9) = 1
        ASSERT_EQUALS(1.0f, logarithmicScale(1, 1.0f), "f=1, x=1 should be 1");

        // f=1, x=0.5 -> log10(1 + 4.5) = log10(5.5) approx 0.74036
        float val_f1 = logarithmicScale(1, 0.5f);
        bool close_f1 = std::abs(val_f1 - 0.74036f) < 0.001f;
        if (!close_f1) {
             std::cout << "f=1, x=0.5 returned " << val_f1 << " expected approx 0.74036" << std::endl;
        }
        ASSERT_TRUE(close_f1, "f=1, x=0.5 should be approx 0.74036");

        // Test f >= 5 (Loop path)
        // f=5, x=0.0 -> 0
        ASSERT_EQUALS(0.0f, logarithmicScale(5, 0.0f), "f=5, x=0 should be 0");
        // f=5, x=1.0 -> 1
        ASSERT_EQUALS(1.0f, logarithmicScale(5, 1.0f), "f=5, x=1 should be 1");

        // f=5, x=0.5
        // We can manually calculate it or trust it increases.
        // It should be > val_f1 (since log10(1+9x) > x for x in (0, 1))
        // Wait, log10(1+9x) > x?
        // x=0.5 -> 0.74. 0.74 > 0.5. Yes.
        float val_f5 = logarithmicScale(5, 0.5f);
        ASSERT_TRUE(val_f5 > val_f1, "Higher f should increase value for x=0.5");
        ASSERT_TRUE(val_f5 < 1.0f, "Result should stay < 1.0");
    }
};

// ============================================================================
// Test Registration
// ============================================================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    TestSuite suite("Utility Unit Tests");

    suite.addTest(std::make_unique<LogarithmicScaleTest>());

    auto results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
