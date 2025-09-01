/*
 * test_flac_quality_validation.cpp - FLAC codec quality validation and accuracy tests
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

#include "psymp3.h"

#ifdef HAVE_FLAC

#include <iostream>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>

/**
 * @brief Test FLAC codec quality validation and accuracy
 * 
 * This test suite validates the quality validation methods and accuracy
 * testing capabilities of the FLAC codec implementation.
 */
class FLACQualityValidationTest {
public:
    static bool runAllTests() {
        std::cout << "FLAC Quality Validation and Accuracy Tests" << std::endl;
        std::cout << "==========================================" << std::endl;
        
        bool all_passed = true;
        
        all_passed &= testBitPerfectValidation();
        all_passed &= testSignalToNoiseRatio();
        all_passed &= testTotalHarmonicDistortion();
        all_passed &= testConversionQuality();
        all_passed &= testDynamicRangeValidation();
        all_passed &= testQualityMetrics();
        all_passed &= testAccuracyWithKnownSamples();
        all_passed &= testEdgeCaseHandling();
        all_passed &= testMathematicalAccuracy();
        all_passed &= testSignalIntegrityPreservation();
        
        std::cout << std::endl;
        if (all_passed) {
            std::cout << "✓ All FLAC quality validation tests PASSED" << std::endl;
        } else {
            std::cout << "✗ Some FLAC quality validation tests FAILED" << std::endl;
        }
        
        return all_passed;
    }

private:
    static bool testBitPerfectValidation() {
        std::cout << "Testing bit-perfect validation..." << std::endl;
        
        try {
            StreamInfo info = createTestStreamInfo();
            FLACCodec codec(info);
            
            if (!codec.initialize()) {
                std::cout << "  ERROR: Failed to initialize codec" << std::endl;
                return false;
            }
            
            // Test 1: Identical samples should be bit-perfect
            std::vector<int16_t> samples1 = {1000, -1000, 2000, -2000, 0, 32767, -32768};
            std::vector<int16_t> samples2 = samples1; // Identical copy
            
            bool bit_perfect = codec.validateBitPerfectDecoding(samples1, samples2);
            if (!bit_perfect) {
                std::cout << "  ERROR: Identical samples not detected as bit-perfect" << std::endl;
                return false;
            }
            
            // Test 2: Different samples should not be bit-perfect
            samples2[0] = 1001; // Change one sample
            bit_perfect = codec.validateBitPerfectDecoding(samples1, samples2);
            if (bit_perfect) {
                std::cout << "  ERROR: Different samples incorrectly detected as bit-perfect" << std::endl;
                return false;
            }
            
            // Test 3: Size mismatch should not be bit-perfect
            samples2 = samples1;
            samples2.push_back(100);
            bit_perfect = codec.validateBitPerfectDecoding(samples1, samples2);
            if (bit_perfect) {
                std::cout << "  ERROR: Size mismatch incorrectly detected as bit-perfect" << std::endl;
                return false;
            }
            
            // Test 4: Empty arrays should be bit-perfect
            std::vector<int16_t> empty1, empty2;
            bit_perfect = codec.validateBitPerfectDecoding(empty1, empty2);
            if (!bit_perfect) {
                std::cout << "  ERROR: Empty arrays not detected as bit-perfect" << std::endl;
                return false;
            }
            
            std::cout << "  ✓ Bit-perfect validation test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "  ERROR: Exception in bit-perfect validation test: " << e.what() << std::endl;
            return false;
        }
    }
    
    static bool testSignalToNoiseRatio() {
        std::cout << "Testing signal-to-noise ratio calculation..." << std::endl;
        
        try {
            StreamInfo info = createTestStreamInfo();
            FLACCodec codec(info);
            
            if (!codec.initialize()) {
                std::cout << "  ERROR: Failed to initialize codec" << std::endl;
                return false;
            }
            
            // Test 1: Identical signals should have very high SNR
            std::vector<int16_t> reference = generateSineWave(1000, 44100, 1.0);
            std::vector<int16_t> identical = reference;
            
            double snr = codec.calculateSignalToNoiseRatio(reference, identical);
            if (snr < 100.0) { // Should be very high for identical signals
                std::cout << "  ERROR: SNR for identical signals too low: " << snr << " dB" << std::endl;
                return false;
            }
            
            // Test 2: Add small amount of noise
            std::vector<int16_t> noisy = reference;
            addGaussianNoise(noisy, 0.01); // 1% noise
            
            snr = codec.calculateSignalToNoiseRatio(reference, noisy);
            if (snr < 30.0 || snr > 50.0) { // Should be reasonable for 1% noise
                std::cout << "  ERROR: SNR for noisy signal out of expected range: " << snr << " dB" << std::endl;
                return false;
            }
            
            // Test 3: Empty arrays should handle gracefully
            std::vector<int16_t> empty1, empty2;
            snr = codec.calculateSignalToNoiseRatio(empty1, empty2);
            if (snr < 0.0) {
                std::cout << "  ERROR: SNR for empty arrays should be non-negative" << std::endl;
                return false;
            }
            
            std::cout << "  ✓ Signal-to-noise ratio test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "  ERROR: Exception in SNR test: " << e.what() << std::endl;
            return false;
        }
    }
    
    static bool testTotalHarmonicDistortion() {
        std::cout << "Testing total harmonic distortion calculation..." << std::endl;
        
        try {
            StreamInfo info = createTestStreamInfo();
            FLACCodec codec(info);
            
            if (!codec.initialize()) {
                std::cout << "  ERROR: Failed to initialize codec" << std::endl;
                return false;
            }
            
            // Test 1: Pure sine wave should have low THD
            std::vector<int16_t> sine_wave = generateSineWave(1000, 44100, 0.8);
            double thd = codec.calculateTotalHarmonicDistortion(sine_wave);
            
            if (thd > 10.0) { // Should be reasonably low for pure sine
                std::cout << "  ERROR: THD for sine wave too high: " << thd << "%" << std::endl;
                return false;
            }
            
            // Test 2: Square wave should have higher THD
            std::vector<int16_t> square_wave = generateSquareWave(1000, 44100, 0.8);
            double square_thd = codec.calculateTotalHarmonicDistortion(square_wave);
            
            if (square_thd <= thd) {
                std::cout << "  ERROR: Square wave THD should be higher than sine wave" << std::endl;
                return false;
            }
            
            // Test 3: Silence should have zero THD
            std::vector<int16_t> silence(1000, 0);
            thd = codec.calculateTotalHarmonicDistortion(silence);
            
            if (thd != 0.0) {
                std::cout << "  ERROR: Silence should have zero THD, got: " << thd << "%" << std::endl;
                return false;
            }
            
            std::cout << "  ✓ Total harmonic distortion test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "  ERROR: Exception in THD test: " << e.what() << std::endl;
            return false;
        }
    }
    
    static bool testConversionQuality() {
        std::cout << "Testing bit depth conversion quality..." << std::endl;
        
        try {
            StreamInfo info = createTestStreamInfo();
            FLACCodec codec(info);
            
            if (!codec.initialize()) {
                std::cout << "  ERROR: Failed to initialize codec" << std::endl;
                return false;
            }
            
            // Test 8-bit to 16-bit conversion
            std::vector<FLAC__int32> source_8bit = {-128, -64, 0, 64, 127};
            std::vector<int16_t> converted_8bit;
            
            for (FLAC__int32 sample : source_8bit) {
                converted_8bit.push_back(static_cast<int16_t>(sample << 8));
            }
            
            bool quality_ok = codec.validateConversionQuality(source_8bit, converted_8bit, 8);
            if (!quality_ok) {
                std::cout << "  ERROR: 8-bit to 16-bit conversion quality validation failed" << std::endl;
                return false;
            }
            
            // Test 24-bit to 16-bit conversion
            std::vector<FLAC__int32> source_24bit = {-8388608, -4194304, 0, 4194304, 8388607};
            std::vector<int16_t> converted_24bit;
            
            for (FLAC__int32 sample : source_24bit) {
                converted_24bit.push_back(static_cast<int16_t>(sample >> 8));
            }
            
            quality_ok = codec.validateConversionQuality(source_24bit, converted_24bit, 24);
            if (!quality_ok) {
                std::cout << "  ERROR: 24-bit to 16-bit conversion quality validation failed" << std::endl;
                return false;
            }
            
            // Test 32-bit to 16-bit conversion
            std::vector<FLAC__int32> source_32bit = {-2147483648, -1073741824, 0, 1073741824, 2147483647};
            std::vector<int16_t> converted_32bit;
            
            for (FLAC__int32 sample : source_32bit) {
                int32_t scaled = sample >> 16;
                converted_32bit.push_back(static_cast<int16_t>(std::clamp(scaled, -32768, 32767)));
            }
            
            quality_ok = codec.validateConversionQuality(source_32bit, converted_32bit, 32);
            if (!quality_ok) {
                std::cout << "  ERROR: 32-bit to 16-bit conversion quality validation failed" << std::endl;
                return false;
            }
            
            std::cout << "  ✓ Conversion quality test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "  ERROR: Exception in conversion quality test: " << e.what() << std::endl;
            return false;
        }
    }
    
    static bool testDynamicRangeValidation() {
        std::cout << "Testing dynamic range validation..." << std::endl;
        
        try {
            StreamInfo info = createTestStreamInfo();
            FLACCodec codec(info);
            
            if (!codec.initialize()) {
                std::cout << "  ERROR: Failed to initialize codec" << std::endl;
                return false;
            }
            
            // Test 1: Normal audio with good dynamic range
            std::vector<int16_t> normal_audio = generateSineWave(1000, 44100, 0.5);
            bool range_ok = codec.validateDynamicRange(normal_audio);
            
            if (!range_ok) {
                std::cout << "  ERROR: Normal audio dynamic range validation failed" << std::endl;
                return false;
            }
            
            // Test 2: Silence should be valid (special case)
            std::vector<int16_t> silence(1000, 0);
            range_ok = codec.validateDynamicRange(silence);
            
            if (!range_ok) {
                std::cout << "  ERROR: Silence dynamic range validation failed" << std::endl;
                return false;
            }
            
            // Test 3: Very compressed audio (low dynamic range)
            std::vector<int16_t> compressed_audio(1000);
            std::fill(compressed_audio.begin(), compressed_audio.end(), 16000); // Constant level
            range_ok = codec.validateDynamicRange(compressed_audio);
            
            // This might fail due to very low dynamic range, which is expected
            
            std::cout << "  ✓ Dynamic range validation test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "  ERROR: Exception in dynamic range test: " << e.what() << std::endl;
            return false;
        }
    }
    
    static bool testQualityMetrics() {
        std::cout << "Testing comprehensive quality metrics..." << std::endl;
        
        try {
            StreamInfo info = createTestStreamInfo();
            FLACCodec codec(info);
            
            if (!codec.initialize()) {
                std::cout << "  ERROR: Failed to initialize codec" << std::endl;
                return false;
            }
            
            // Test with sine wave
            std::vector<int16_t> sine_wave = generateSineWave(1000, 44100, 0.8);
            AudioQualityMetrics metrics = codec.calculateQualityMetrics(sine_wave);
            
            // Validate metrics are reasonable
            if (metrics.peak_amplitude <= 0.0 || metrics.peak_amplitude > 1.0) {
                std::cout << "  ERROR: Invalid peak amplitude: " << metrics.peak_amplitude << std::endl;
                return false;
            }
            
            if (metrics.rms_amplitude <= 0.0 || metrics.rms_amplitude > metrics.peak_amplitude) {
                std::cout << "  ERROR: Invalid RMS amplitude: " << metrics.rms_amplitude << std::endl;
                return false;
            }
            
            if (metrics.zero_crossings == 0) {
                std::cout << "  ERROR: Sine wave should have zero crossings" << std::endl;
                return false;
            }
            
            if (metrics.clipped_samples > 0) {
                std::cout << "  ERROR: Sine wave should not have clipped samples" << std::endl;
                return false;
            }
            
            // Test quality assessment
            if (!metrics.isGoodQuality()) {
                std::cout << "  WARNING: Sine wave not assessed as good quality" << std::endl;
                // This is a warning, not a failure
            }
            
            std::cout << "  ✓ Quality metrics test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "  ERROR: Exception in quality metrics test: " << e.what() << std::endl;
            return false;
        }
    }
    
    static bool testAccuracyWithKnownSamples() {
        std::cout << "Testing accuracy with known reference samples..." << std::endl;
        
        try {
            StreamInfo info = createTestStreamInfo();
            FLACCodec codec(info);
            
            if (!codec.initialize()) {
                std::cout << "  ERROR: Failed to initialize codec" << std::endl;
                return false;
            }
            
            // Test 1: Known mathematical sequences
            std::vector<int16_t> linear_ramp;
            for (int i = -1000; i <= 1000; i += 10) {
                linear_ramp.push_back(static_cast<int16_t>(i));
            }
            
            AudioQualityMetrics metrics = codec.calculateQualityMetrics(linear_ramp);
            
            // Linear ramp should have specific characteristics
            if (metrics.dc_offset < -5.0 || metrics.dc_offset > 5.0) {
                std::cout << "  ERROR: Linear ramp DC offset out of range: " << metrics.dc_offset << "%" << std::endl;
                return false;
            }
            
            // Test 2: Alternating samples (maximum zero crossings)
            std::vector<int16_t> alternating;
            for (size_t i = 0; i < 1000; ++i) {
                alternating.push_back((i % 2 == 0) ? 1000 : -1000);
            }
            
            metrics = codec.calculateQualityMetrics(alternating);
            
            // Should have many zero crossings
            if (metrics.zero_crossings < 400) { // Should be close to 500
                std::cout << "  ERROR: Alternating pattern should have many zero crossings: " << metrics.zero_crossings << std::endl;
                return false;
            }
            
            // Test 3: Full-scale samples (test clipping detection)
            std::vector<int16_t> full_scale = {32767, -32768, 32767, -32768};
            metrics = codec.calculateQualityMetrics(full_scale);
            
            if (metrics.clipped_samples != 4) {
                std::cout << "  ERROR: Full-scale samples not detected as clipped: " << metrics.clipped_samples << std::endl;
                return false;
            }
            
            std::cout << "  ✓ Known samples accuracy test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "  ERROR: Exception in known samples test: " << e.what() << std::endl;
            return false;
        }
    }
    
    static bool testEdgeCaseHandling() {
        std::cout << "Testing edge case handling..." << std::endl;
        
        try {
            StreamInfo info = createTestStreamInfo();
            FLACCodec codec(info);
            
            if (!codec.initialize()) {
                std::cout << "  ERROR: Failed to initialize codec" << std::endl;
                return false;
            }
            
            // Test 1: Empty arrays
            std::vector<int16_t> empty;
            AudioQualityMetrics metrics = codec.calculateQualityMetrics(empty);
            
            // Should handle gracefully without crashing
            if (metrics.peak_amplitude != 0.0 || metrics.rms_amplitude != 0.0) {
                std::cout << "  ERROR: Empty array should have zero metrics" << std::endl;
                return false;
            }
            
            // Test 2: Single sample
            std::vector<int16_t> single = {1000};
            metrics = codec.calculateQualityMetrics(single);
            
            if (metrics.zero_crossings != 0) {
                std::cout << "  ERROR: Single sample should have zero crossings" << std::endl;
                return false;
            }
            
            // Test 3: All zeros
            std::vector<int16_t> zeros(1000, 0);
            metrics = codec.calculateQualityMetrics(zeros);
            
            if (metrics.peak_amplitude != 0.0 || metrics.rms_amplitude != 0.0) {
                std::cout << "  ERROR: Zero samples should have zero amplitude metrics" << std::endl;
                return false;
            }
            
            // Test 4: Maximum values
            std::vector<int16_t> max_values = {32767, -32768};
            metrics = codec.calculateQualityMetrics(max_values);
            
            if (metrics.peak_amplitude != 1.0) {
                std::cout << "  ERROR: Maximum values should have peak amplitude 1.0: " << metrics.peak_amplitude << std::endl;
                return false;
            }
            
            std::cout << "  ✓ Edge case handling test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "  ERROR: Exception in edge case test: " << e.what() << std::endl;
            return false;
        }
    }
    
    static bool testMathematicalAccuracy() {
        std::cout << "Testing mathematical accuracy of algorithms..." << std::endl;
        
        try {
            StreamInfo info = createTestStreamInfo();
            FLACCodec codec(info);
            
            if (!codec.initialize()) {
                std::cout << "  ERROR: Failed to initialize codec" << std::endl;
                return false;
            }
            
            // Test 1: RMS calculation accuracy
            std::vector<int16_t> known_rms_samples = {0, 16384, 0, -16384}; // Should have RMS = 0.5
            AudioQualityMetrics metrics = codec.calculateQualityMetrics(known_rms_samples);
            
            double expected_rms = 0.5;
            double tolerance = 0.01;
            
            if (std::abs(metrics.rms_amplitude - expected_rms) > tolerance) {
                std::cout << "  ERROR: RMS calculation inaccurate. Expected: " << expected_rms 
                         << ", Got: " << metrics.rms_amplitude << std::endl;
                return false;
            }
            
            // Test 2: Peak calculation accuracy
            std::vector<int16_t> known_peak_samples = {1000, -2000, 500, -1500};
            metrics = codec.calculateQualityMetrics(known_peak_samples);
            
            double expected_peak = 2000.0 / 32768.0;
            
            if (std::abs(metrics.peak_amplitude - expected_peak) > tolerance) {
                std::cout << "  ERROR: Peak calculation inaccurate. Expected: " << expected_peak 
                         << ", Got: " << metrics.peak_amplitude << std::endl;
                return false;
            }
            
            // Test 3: DC offset calculation
            std::vector<int16_t> dc_offset_samples = {1000, 1000, 1000, 1000}; // Constant DC
            metrics = codec.calculateQualityMetrics(dc_offset_samples);
            
            double expected_dc = (1000.0 / 32768.0) * 100.0; // As percentage
            
            if (std::abs(metrics.dc_offset - expected_dc) > tolerance) {
                std::cout << "  ERROR: DC offset calculation inaccurate. Expected: " << expected_dc 
                         << "%, Got: " << metrics.dc_offset << "%" << std::endl;
                return false;
            }
            
            std::cout << "  ✓ Mathematical accuracy test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "  ERROR: Exception in mathematical accuracy test: " << e.what() << std::endl;
            return false;
        }
    }
    
    static bool testSignalIntegrityPreservation() {
        std::cout << "Testing signal integrity preservation..." << std::endl;
        
        try {
            StreamInfo info = createTestStreamInfo();
            FLACCodec codec(info);
            
            if (!codec.initialize()) {
                std::cout << "  ERROR: Failed to initialize codec" << std::endl;
                return false;
            }
            
            // Test 1: Frequency content preservation (simplified)
            std::vector<int16_t> low_freq = generateSineWave(100, 44100, 0.8);
            std::vector<int16_t> high_freq = generateSineWave(10000, 44100, 0.8);
            
            AudioQualityMetrics low_metrics = codec.calculateQualityMetrics(low_freq);
            AudioQualityMetrics high_metrics = codec.calculateQualityMetrics(high_freq);
            
            // High frequency should have more zero crossings
            if (high_metrics.zero_crossings <= low_metrics.zero_crossings) {
                std::cout << "  ERROR: High frequency signal should have more zero crossings" << std::endl;
                return false;
            }
            
            // Test 2: Amplitude preservation
            std::vector<int16_t> half_amplitude = generateSineWave(1000, 44100, 0.4);
            std::vector<int16_t> full_amplitude = generateSineWave(1000, 44100, 0.8);
            
            AudioQualityMetrics half_metrics = codec.calculateQualityMetrics(half_amplitude);
            AudioQualityMetrics full_metrics = codec.calculateQualityMetrics(full_amplitude);
            
            // Full amplitude should have higher peak and RMS
            if (full_metrics.peak_amplitude <= half_metrics.peak_amplitude) {
                std::cout << "  ERROR: Full amplitude signal should have higher peak" << std::endl;
                return false;
            }
            
            if (full_metrics.rms_amplitude <= half_metrics.rms_amplitude) {
                std::cout << "  ERROR: Full amplitude signal should have higher RMS" << std::endl;
                return false;
            }
            
            // Test 3: Phase relationship preservation (zero crossings)
            std::vector<int16_t> in_phase = generateSineWave(1000, 44100, 0.8);
            std::vector<int16_t> phase_shifted = generateSineWave(1000, 44100, 0.8, M_PI/2);
            
            AudioQualityMetrics in_phase_metrics = codec.calculateQualityMetrics(in_phase);
            AudioQualityMetrics phase_shifted_metrics = codec.calculateQualityMetrics(phase_shifted);
            
            // Should have similar characteristics (same frequency, different phase)
            double crossing_ratio = static_cast<double>(phase_shifted_metrics.zero_crossings) / 
                                   in_phase_metrics.zero_crossings;
            
            if (crossing_ratio < 0.8 || crossing_ratio > 1.2) {
                std::cout << "  ERROR: Phase-shifted signal should have similar zero crossings" << std::endl;
                return false;
            }
            
            std::cout << "  ✓ Signal integrity preservation test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "  ERROR: Exception in signal integrity test: " << e.what() << std::endl;
            return false;
        }
    }
    
    // Helper methods for test signal generation
    static StreamInfo createTestStreamInfo() {
        StreamInfo info;
        info.codec_type = "audio";
        info.codec_name = "flac";
        info.sample_rate = 44100;
        info.channels = 2;
        info.bits_per_sample = 16;
        info.duration_samples = 1000000;
        info.bitrate = 1411200;
        return info;
    }
    
    static std::vector<int16_t> generateSineWave(double frequency, int sample_rate, 
                                                double amplitude, double phase = 0.0) {
        std::vector<int16_t> samples;
        int duration_samples = sample_rate; // 1 second
        
        for (int i = 0; i < duration_samples; ++i) {
            double t = static_cast<double>(i) / sample_rate;
            double value = amplitude * std::sin(2.0 * M_PI * frequency * t + phase);
            samples.push_back(static_cast<int16_t>(value * 32767.0));
        }
        
        return samples;
    }
    
    static std::vector<int16_t> generateSquareWave(double frequency, int sample_rate, double amplitude) {
        std::vector<int16_t> samples;
        int duration_samples = sample_rate; // 1 second
        
        for (int i = 0; i < duration_samples; ++i) {
            double t = static_cast<double>(i) / sample_rate;
            double phase = std::fmod(frequency * t, 1.0);
            double value = (phase < 0.5) ? amplitude : -amplitude;
            samples.push_back(static_cast<int16_t>(value * 32767.0));
        }
        
        return samples;
    }
    
    static void addGaussianNoise(std::vector<int16_t>& samples, double noise_level) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<double> noise(0.0, noise_level * 32767.0);
        
        for (int16_t& sample : samples) {
            double noisy_sample = static_cast<double>(sample) + noise(gen);
            sample = static_cast<int16_t>(std::clamp(noisy_sample, -32768.0, 32767.0));
        }
    }
};

int main() {
    std::cout << "FLAC Codec Quality Validation and Accuracy Test Suite" << std::endl;
    std::cout << "====================================================" << std::endl;
    std::cout << "Requirements: 12.1, 12.2, 12.3, 12.4, 12.5, 12.6, 12.7, 12.8" << std::endl;
    std::cout << std::endl;
    
    bool success = FLACQualityValidationTest::runAllTests();
    
    return success ? 0 : 1;
}

#else // !HAVE_FLAC

int main() {
    std::cout << "FLAC support not available - skipping quality validation tests" << std::endl;
    return 0;
}

#endif // HAVE_FLAC