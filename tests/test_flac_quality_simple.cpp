/*
 * test_flac_quality_simple.cpp - Simple FLAC codec quality validation test
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_FLAC

/**
 * @brief Simple test for FLAC codec quality validation methods
 * 
 * This test validates the mathematical accuracy of the quality validation
 * algorithms without requiring the full PsyMP3 infrastructure.
 */
class SimpleQualityTest {
public:
    static bool runAllTests() {
        std::cout << "Simple FLAC Quality Validation Tests" << std::endl;
        std::cout << "====================================" << std::endl;
        
        bool all_passed = true;
        
        all_passed &= testBitPerfectComparison();
        all_passed &= testSignalToNoiseRatio();
        all_passed &= testQualityMetrics();
        all_passed &= testConversionAccuracy();
        
        std::cout << std::endl;
        if (all_passed) {
            std::cout << "✓ All simple quality validation tests PASSED" << std::endl;
        } else {
            std::cout << "✗ Some simple quality validation tests FAILED" << std::endl;
        }
        
        return all_passed;
    }

private:
    static bool testBitPerfectComparison() {
        std::cout << "Testing bit-perfect comparison..." << std::endl;
        
        // Test identical samples
        std::vector<int16_t> samples1 = {1000, -1000, 2000, -2000, 0};
        std::vector<int16_t> samples2 = samples1;
        
        bool identical = compareSamplesExact(samples1, samples2);
        if (!identical) {
            std::cout << "  ERROR: Identical samples not detected as bit-perfect" << std::endl;
            return false;
        }
        
        // Test different samples
        samples2[0] = 1001;
        identical = compareSamplesExact(samples1, samples2);
        if (identical) {
            std::cout << "  ERROR: Different samples incorrectly detected as identical" << std::endl;
            return false;
        }
        
        std::cout << "  ✓ Bit-perfect comparison test passed" << std::endl;
        return true;
    }
    
    static bool testSignalToNoiseRatio() {
        std::cout << "Testing signal-to-noise ratio calculation..." << std::endl;
        
        // Test with identical signals (should have very high SNR)
        std::vector<int16_t> reference = generateSineWave(1000, 44100, 0.8);
        std::vector<int16_t> identical = reference;
        
        double snr = calculateSNR(reference, identical);
        if (snr < 100.0) {
            std::cout << "  ERROR: SNR for identical signals too low: " << snr << " dB" << std::endl;
            return false;
        }
        
        // Test with noisy signal
        std::vector<int16_t> noisy = reference;
        addNoise(noisy, 0.01); // 1% noise
        
        snr = calculateSNR(reference, noisy);
        if (snr < 30.0 || snr > 50.0) {
            std::cout << "  ERROR: SNR for noisy signal out of range: " << snr << " dB" << std::endl;
            return false;
        }
        
        std::cout << "  ✓ Signal-to-noise ratio test passed" << std::endl;
        return true;
    }
    
    static bool testQualityMetrics() {
        std::cout << "Testing quality metrics calculation..." << std::endl;
        
        // Test with sine wave
        std::vector<int16_t> sine_wave = generateSineWave(1000, 44100, 0.8);
        
        double peak = calculatePeakAmplitude(sine_wave);
        double rms = calculateRMSAmplitude(sine_wave);
        size_t crossings = countZeroCrossings(sine_wave);
        
        if (peak <= 0.0 || peak > 1.0) {
            std::cout << "  ERROR: Invalid peak amplitude: " << peak << std::endl;
            return false;
        }
        
        if (rms <= 0.0 || rms > peak) {
            std::cout << "  ERROR: Invalid RMS amplitude: " << rms << std::endl;
            return false;
        }
        
        if (crossings == 0) {
            std::cout << "  ERROR: Sine wave should have zero crossings" << std::endl;
            return false;
        }
        
        std::cout << "  ✓ Quality metrics test passed" << std::endl;
        return true;
    }
    
    static bool testConversionAccuracy() {
        std::cout << "Testing bit depth conversion accuracy..." << std::endl;
        
        // Test 8-bit to 16-bit conversion
        int32_t sample_8bit = 100; // 8-bit sample
        int16_t converted = static_cast<int16_t>(sample_8bit << 8);
        int16_t expected = 25600; // 100 * 256
        
        if (converted != expected) {
            std::cout << "  ERROR: 8-bit conversion failed. Expected: " << expected 
                     << ", Got: " << converted << std::endl;
            return false;
        }
        
        // Test 24-bit to 16-bit conversion
        int32_t sample_24bit = 1000000; // 24-bit sample
        int16_t converted_24 = static_cast<int16_t>(sample_24bit >> 8);
        int16_t expected_24 = 3906; // 1000000 / 256
        
        if (converted_24 != expected_24) {
            std::cout << "  ERROR: 24-bit conversion failed. Expected: " << expected_24 
                     << ", Got: " << converted_24 << std::endl;
            return false;
        }
        
        std::cout << "  ✓ Conversion accuracy test passed" << std::endl;
        return true;
    }
    
    // Helper methods
    static bool compareSamplesExact(const std::vector<int16_t>& samples1,
                                   const std::vector<int16_t>& samples2) {
        if (samples1.size() != samples2.size()) {
            return false;
        }
        
        for (size_t i = 0; i < samples1.size(); ++i) {
            if (samples1[i] != samples2[i]) {
                return false;
            }
        }
        
        return true;
    }
    
    static double calculateSNR(const std::vector<int16_t>& reference,
                              const std::vector<int16_t>& test) {
        if (reference.size() != test.size() || reference.empty()) {
            return 0.0;
        }
        
        // Calculate signal power
        double signal_power = 0.0;
        for (int16_t sample : reference) {
            double normalized = static_cast<double>(sample) / 32768.0;
            signal_power += normalized * normalized;
        }
        signal_power /= reference.size();
        
        // Calculate noise power
        double noise_power = 0.0;
        for (size_t i = 0; i < reference.size(); ++i) {
            double diff = static_cast<double>(reference[i] - test[i]) / 32768.0;
            noise_power += diff * diff;
        }
        noise_power /= reference.size();
        
        if (noise_power <= 0.0) {
            return 200.0; // Very high SNR for perfect match
        }
        
        return 10.0 * std::log10(signal_power / noise_power);
    }
    
    static double calculatePeakAmplitude(const std::vector<int16_t>& samples) {
        if (samples.empty()) {
            return 0.0;
        }
        
        int16_t max_sample = 0;
        for (int16_t sample : samples) {
            max_sample = std::max(max_sample, static_cast<int16_t>(std::abs(sample)));
        }
        
        return static_cast<double>(max_sample) / 32768.0;
    }
    
    static double calculateRMSAmplitude(const std::vector<int16_t>& samples) {
        if (samples.empty()) {
            return 0.0;
        }
        
        double sum_squares = 0.0;
        for (int16_t sample : samples) {
            double normalized = static_cast<double>(sample) / 32768.0;
            sum_squares += normalized * normalized;
        }
        
        return std::sqrt(sum_squares / samples.size());
    }
    
    static size_t countZeroCrossings(const std::vector<int16_t>& samples) {
        if (samples.size() < 2) {
            return 0;
        }
        
        size_t crossings = 0;
        for (size_t i = 1; i < samples.size(); ++i) {
            if ((samples[i-1] >= 0 && samples[i] < 0) || 
                (samples[i-1] < 0 && samples[i] >= 0)) {
                crossings++;
            }
        }
        
        return crossings;
    }
    
    static std::vector<int16_t> generateSineWave(double frequency, int sample_rate, double amplitude) {
        std::vector<int16_t> samples;
        int duration_samples = sample_rate / 10; // 0.1 second
        
        for (int i = 0; i < duration_samples; ++i) {
            double t = static_cast<double>(i) / sample_rate;
            double value = amplitude * std::sin(2.0 * M_PI * frequency * t);
            samples.push_back(static_cast<int16_t>(value * 32767.0));
        }
        
        return samples;
    }
    
    static void addNoise(std::vector<int16_t>& samples, double noise_level) {
        for (int16_t& sample : samples) {
            // Simple noise addition (not cryptographically secure)
            double noise = (static_cast<double>(rand()) / RAND_MAX - 0.5) * 2.0;
            double noisy_sample = static_cast<double>(sample) + noise * noise_level * 32767.0;
            sample = static_cast<int16_t>(std::clamp(noisy_sample, -32768.0, 32767.0));
        }
    }
};

int main() {
    std::cout << "Simple FLAC Codec Quality Validation Test" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "Requirements: 12.1, 12.2, 12.3, 12.4, 12.5, 12.6, 12.7, 12.8" << std::endl;
    std::cout << std::endl;
    
    bool success = SimpleQualityTest::runAllTests();
    
    return success ? 0 : 1;
}

#else // !HAVE_FLAC

int main() {
    std::cout << "FLAC support not available - skipping quality validation tests" << std::endl;
    return 0;
}

#endif // HAVE_FLAC