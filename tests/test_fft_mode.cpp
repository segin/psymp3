/*
 * test_fft_mode.cpp - Unit tests for FFT mode setting and getting
 * This file is part of PsyMP3.
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "test_framework.h"
#include "core/fft.h"

using namespace PsyMP3::Core;
using namespace TestFramework;

class FFTModeTest : public TestCase {
public:
    FFTModeTest() : TestCase("FFTModeTest") {}

protected:
    void runTest() override {
        // FFT size must be a power of 2
        FFT fft(2);

        // Test Original mode
        fft.setFFTMode(FFTMode::Original);
        ASSERT_TRUE(fft.getFFTMode() == FFTMode::Original, "getFFTMode should return Original");
        ASSERT_TRUE(fft.getFFTModeName() == "mat-og", "getFFTModeName should return mat-og");

        // Test Optimized mode
        fft.setFFTMode(FFTMode::Optimized);
        ASSERT_TRUE(fft.getFFTMode() == FFTMode::Optimized, "getFFTMode should return Optimized");
        ASSERT_TRUE(fft.getFFTModeName() == "vibe-1", "getFFTModeName should return vibe-1");

        // Test NeomatIn mode
        fft.setFFTMode(FFTMode::NeomatIn);
        ASSERT_TRUE(fft.getFFTMode() == FFTMode::NeomatIn, "getFFTMode should return NeomatIn");
        ASSERT_TRUE(fft.getFFTModeName() == "neomat-in", "getFFTModeName should return neomat-in");

        // Test NeomatOut mode
        fft.setFFTMode(FFTMode::NeomatOut);
        ASSERT_TRUE(fft.getFFTMode() == FFTMode::NeomatOut, "getFFTMode should return NeomatOut");
        ASSERT_TRUE(fft.getFFTModeName() == "neomat-out", "getFFTModeName should return neomat-out");
    }
};

int main() {
    try {
        FFTModeTest test;
        TestCaseInfo info = test.run();

        if (info.result != TestResult::PASSED) {
            std::cerr << "Test failed: " << info.failure_message << std::endl;
            return 1;
        }

        std::cout << "All FFT mode tests passed." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << std::endl;
        return 1;
    }
}
