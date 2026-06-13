/*
 * test_fft_unit.cpp - Unit tests for the FFT class
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif
#include "test_framework.h"
#include "core/fft.h"

#include <cmath>
#include <vector>
#include <memory>
#include <iostream>

using namespace PsyMP3::Core;
using namespace TestFramework;

class FFTBasicPropertiesTest : public TestCase {
public:
    FFTBasicPropertiesTest() : TestCase("FFT Basic Properties") {}

protected:
    void runTest() override {
        FFT fft(1024);

        // Test default mode
        ASSERT_EQUALS("mat-og", fft.getFFTModeName(), "Default FFT mode should be 'mat-og'");
        ASSERT_TRUE(fft.getFFTMode() == FFTMode::Original, "Default mode enum mismatch");

        // Test mode setting
        fft.setFFTMode(FFTMode::Optimized);
        ASSERT_EQUALS("vibe-1", fft.getFFTModeName(), "Optimized FFT mode should be 'vibe-1'");
        ASSERT_TRUE(fft.getFFTMode() == FFTMode::Optimized, "Optimized mode enum mismatch");

        fft.setFFTMode(FFTMode::NeomatIn);
        ASSERT_EQUALS("neomat-in", fft.getFFTModeName(), "NeomatIn FFT mode should be 'neomat-in'");
        ASSERT_TRUE(fft.getFFTMode() == FFTMode::NeomatIn, "NeomatIn mode enum mismatch");

        fft.setFFTMode(FFTMode::NeomatOut);
        ASSERT_EQUALS("neomat-out", fft.getFFTModeName(), "NeomatOut FFT mode should be 'neomat-out'");
        ASSERT_TRUE(fft.getFFTMode() == FFTMode::NeomatOut, "NeomatOut mode enum mismatch");
    }
};

class FFTDCSignalTest : public TestCase {
public:
    FFTDCSignalTest() : TestCase("FFT DC Signal Test") {}

protected:
    void runTest() override {
        const int size = 1024;
        std::vector<float> input(size, 1.0f);
        std::vector<float> output(size, 0.0f);

        FFT fft(size);
        std::vector<FFTMode> modes = {FFTMode::Original, FFTMode::Optimized, FFTMode::NeomatIn, FFTMode::NeomatOut};

        for (auto mode : modes) {
            fft.setFFTMode(mode);
            fft.fft(output.data(), input.data());

            // The DC component (bin 0) should be 1.0 (after normalization by size)
            ASSERT_TRUE(std::abs(output[0] - 1.0f) < 0.001f,
                        "DC component should be 1.0 for mode " + fft.getFFTModeName() + ", got " + std::to_string(output[0]));

            // All other bins should be zero
            for (int i = 1; i < size; ++i) {
                ASSERT_TRUE(std::abs(output[i]) < 0.001f,
                            "Non-DC component at bin " + std::to_string(i) + " should be 0.0 for mode " + fft.getFFTModeName() + ", got " + std::to_string(output[i]));
            }
        }
    }
};

class FFTNyquistTest : public TestCase {
public:
    FFTNyquistTest() : TestCase("FFT Nyquist Test") {}

protected:
    void runTest() override {
        const int size = 1024;
        std::vector<float> input(size);
        // Nyquist frequency signal: 1, -1, 1, -1, ...
        for (int i = 0; i < size; ++i) {
            input[i] = (i % 2 == 0) ? 1.0f : -1.0f;
        }
        std::vector<float> output(size, 0.0f);

        FFT fft(size);
        std::vector<FFTMode> modes = {FFTMode::Original, FFTMode::Optimized, FFTMode::NeomatIn, FFTMode::NeomatOut};

        for (auto mode : modes) {
            fft.setFFTMode(mode);
            fft.fft(output.data(), input.data());

            // The Nyquist frequency component is at size/2
            ASSERT_TRUE(std::abs(output[size / 2] - 1.0f) < 0.001f,
                        "Nyquist component should be ~1.0 for mode " + fft.getFFTModeName() + ", got " + std::to_string(output[size / 2]));

            // All other bins should be zero
            for (int i = 0; i < size; ++i) {
                if (i != size / 2) {
                    ASSERT_TRUE(std::abs(output[i]) < 0.001f,
                                "Non-Nyquist component at bin " + std::to_string(i) + " should be 0.0 for mode " + fft.getFFTModeName() + ", got " + std::to_string(output[i]));
                }
            }
        }
    }
};

class FFTSingleSineTest : public TestCase {
public:
    FFTSingleSineTest() : TestCase("FFT Single Sine Wave Test") {}

protected:
    void runTest() override {
        const int size = 1024;
        const int target_bin = 64; // arbitrary frequency
        std::vector<float> input(size);

        for (int i = 0; i < size; ++i) {
            input[i] = std::sin(2.0f * M_PI_F * target_bin * i / size);
        }

        std::vector<float> output(size, 0.0f);

        FFT fft(size);
        std::vector<FFTMode> modes = {FFTMode::Original, FFTMode::Optimized, FFTMode::NeomatIn, FFTMode::NeomatOut};

        for (auto mode : modes) {
            fft.setFFTMode(mode);
            fft.fft(output.data(), input.data());

            // We expect peaks at target_bin and size - target_bin for real input
            // Due to the normalization in the current implementations:
            float expected_magnitude = 0.5f;

            ASSERT_TRUE(std::abs(output[target_bin] - expected_magnitude) < 0.001f,
                        "Component at target bin " + std::to_string(target_bin) + " should be ~0.5 for mode " + fft.getFFTModeName() + ", got " + std::to_string(output[target_bin]));

            ASSERT_TRUE(std::abs(output[size - target_bin] - expected_magnitude) < 0.001f,
                        "Component at target bin " + std::to_string(size - target_bin) + " should be ~0.5 for mode " + fft.getFFTModeName() + ", got " + std::to_string(output[size - target_bin]));

            // Other bins should be near zero (though some spectral leakage might occur with floating point)
            for (int i = 0; i < size; ++i) {
                if (i != target_bin && i != size - target_bin) {
                    ASSERT_TRUE(std::abs(output[i]) < 0.001f,
                                "Spurious component at bin " + std::to_string(i) + " for mode " + fft.getFFTModeName() + ", got " + std::to_string(output[i]));
                }
            }
        }
    }
};

class FFTImpulseResponseTest : public TestCase {
public:
    FFTImpulseResponseTest() : TestCase("FFT Impulse Response Test") {}

protected:
    void runTest() override {
        const int size = 1024;
        std::vector<float> input(size, 0.0f);
        input[0] = 1.0f; // Impulse at t=0

        std::vector<float> output(size, 0.0f);

        FFT fft(size);
        std::vector<FFTMode> modes = {FFTMode::Original, FFTMode::Optimized, FFTMode::NeomatIn, FFTMode::NeomatOut};

        for (auto mode : modes) {
            fft.setFFTMode(mode);
            fft.fft(output.data(), input.data());

            // An impulse gives a flat frequency response.
            // Normalization is by size, so each bin should be 1.0 / size
            float expected = 1.0f / size;

            for (int i = 0; i < size; ++i) {
                ASSERT_TRUE(std::abs(output[i] - expected) < 0.0001f,
                            "Component at bin " + std::to_string(i) + " should be " + std::to_string(expected) + " for mode " + fft.getFFTModeName() + ", got " + std::to_string(output[i]));
            }
        }
    }
};

class FFTModeEquivalenceTest : public TestCase {
public:
    FFTModeEquivalenceTest() : TestCase("FFT Mode Equivalence Test") {}

protected:
    void runTest() override {
        const int size = 1024;
        std::vector<float> input(size);

        // Generate pseudo-random input
        for (int i = 0; i < size; ++i) {
            input[i] = std::sin(2.0f * M_PI_F * 10 * i / size) + 0.5f * std::cos(2.0f * M_PI_F * 50 * i / size) + (float)(rand() % 1000) / 1000.0f;
        }

        std::vector<float> output_ref(size, 0.0f);
        std::vector<float> output_test(size, 0.0f);

        FFT fft(size);

        // Use Original as reference
        fft.setFFTMode(FFTMode::Original);
        fft.fft(output_ref.data(), input.data());

        std::vector<FFTMode> modes_to_test = {FFTMode::Optimized, FFTMode::NeomatIn, FFTMode::NeomatOut};

        for (auto mode : modes_to_test) {
            fft.setFFTMode(mode);
            fft.fft(output_test.data(), input.data());

            for (int i = 0; i < size; ++i) {
                // Allow a small tolerance due to different arithmetic order in optimized algorithms
                ASSERT_TRUE(std::abs(output_ref[i] - output_test[i]) < 0.0001f,
                            "Mode " + fft.getFFTModeName() + " differs from reference at bin " + std::to_string(i) + ": Ref=" + std::to_string(output_ref[i]) + ", Test=" + std::to_string(output_test[i]));
            }
        }
    }
};

int main() {
    TestSuite suite("FFT Unit Tests");

    suite.addTest(std::make_unique<FFTBasicPropertiesTest>());
    suite.addTest(std::make_unique<FFTDCSignalTest>());
    suite.addTest(std::make_unique<FFTNyquistTest>());
    suite.addTest(std::make_unique<FFTSingleSineTest>());
    suite.addTest(std::make_unique<FFTImpulseResponseTest>());
    suite.addTest(std::make_unique<FFTModeEquivalenceTest>());

    auto results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
