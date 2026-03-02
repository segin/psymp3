/*
 * test_logarithmic_scale.cpp - Unit tests for logarithmicScale utility
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif
#include "test_framework.h"
#include "core/utility/utility.h"
#include <cmath>
#include <limits>
#include <algorithm>

using namespace PsyMP3::Core::Utility;
using namespace TestFramework;

// Helper to calculate expected value without LUT
float calculateExpected(int f, float x) {
    x = std::clamp(x, 0.0f, 1.0f);
    if (f <= 0) return x;
    for (int i = 0; i < f; ++i) {
        x = std::log10(1.0f + 9.0f * x);
    }
    return x;
}

class LogarithmicScaleTest : public TestCase {
public:
    LogarithmicScaleTest() : TestCase("logarithmicScale") {}

protected:
    void runTest() override {
        // Test f <= 0 (Identity/Clamped)
        ASSERT_EQUALS(0.5f, logarithmicScale(0, 0.5f), "f=0 should return x");
        ASSERT_EQUALS(0.5f, logarithmicScale(-1, 0.5f), "f=-1 should return x");

        // Test clamping for f <= 0
        ASSERT_EQUALS(0.0f, logarithmicScale(0, -0.1f), "f=0 should clamp negative x to 0");
        ASSERT_EQUALS(1.0f, logarithmicScale(0, 1.1f), "f=0 should clamp x > 1 to 1");

        // Test LUT range (f = 1 to 4)
        // We use a small epsilon because LUT uses quantization
        // Memory indicates accuracy within 0.002
        float tolerance = 0.002f;

        for (int f = 1; f < 5; ++f) {
            for (float x = 0.0f; x <= 1.0f; x += 0.1f) {
                float expected = calculateExpected(f, x);
                float actual = logarithmicScale(f, x);
                if (std::abs(expected - actual) > tolerance) {
                    std::ostringstream oss;
                    oss << "f=" << f << ", x=" << x << " failed. Expected: " << expected << ", Got: " << actual;
                    throw AssertionFailure(oss.str());
                }
            }
        }

        // Test Fallback range (f >= 5)
        // This should match calculateExpected exactly (or very closely due to float ops)
        for (int f = 5; f <= 6; ++f) {
             for (float x = 0.0f; x <= 1.0f; x += 0.1f) {
                float expected = calculateExpected(f, x);
                float actual = logarithmicScale(f, x);
                // Since fallback uses same logic as calculateExpected, they should be very close
                if (std::abs(expected - actual) > 0.00001f) {
                    std::ostringstream oss;
                    oss << "f=" << f << ", x=" << x << " failed fallback. Expected: " << expected << ", Got: " << actual;
                    throw AssertionFailure(oss.str());
                }
            }
        }

        // Edge Cases
        ASSERT_EQUALS(0.0f, logarithmicScale(1, 0.0f), "x=0 should map to 0");
        ASSERT_EQUALS(1.0f, logarithmicScale(1, 1.0f), "x=1 should map to 1");

        // Clamping for f > 0
        ASSERT_EQUALS(0.0f, logarithmicScale(1, -0.5f), "Negative x should be clamped to 0");
        ASSERT_EQUALS(1.0f, logarithmicScale(1, 1.5f), "x > 1 should be clamped to 1");
    }
};

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    TestSuite suite("Logarithmic Scale Tests");
    suite.addTest(std::make_unique<LogarithmicScaleTest>());

    auto results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
