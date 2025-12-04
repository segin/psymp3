/*
 * test_flac_time_differential_properties.cpp - Property-based tests for FLAC time differential calculation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * **Feature: flac-bisection-seeking, Property 8: Time Differential Calculation**
 * **Validates: Requirements 4.1, 4.2**
 *
 * For any actual sample and target sample at sample rate R, the time differential
 * in milliseconds SHALL equal `abs(actual - target) * 1000 / R`.
 */

#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <cstdint>
#include <cmath>
#include <random>
#include <sstream>
#include <iomanip>
#include <limits>

// ========================================
// TIME DIFFERENTIAL CALCULATION LOGIC
// ========================================

/**
 * @brief Constants for time differential calculation
 */
struct TimeDifferentialConstants {
    static constexpr int64_t TOLERANCE_MS = 250;
    static constexpr uint32_t MIN_SAMPLE_RATE = 8000;    // 8 kHz minimum
    static constexpr uint32_t MAX_SAMPLE_RATE = 655350;  // RFC 9639 maximum
};

/**
 * @brief Calculate time differential in milliseconds per Requirement 4.1
 * 
 * Formula: abs(actual_sample - target_sample) * 1000 / sample_rate
 * 
 * @param actual_sample The actual sample position found
 * @param target_sample The target sample position desired
 * @param sample_rate The sample rate in Hz
 * @return Time differential in milliseconds
 */
int64_t calculateTimeDifferentialMs(uint64_t actual_sample, uint64_t target_sample, uint32_t sample_rate) {
    if (sample_rate == 0) return INT64_MAX;  // Avoid division by zero
    
    int64_t sample_diff = static_cast<int64_t>(actual_sample) - static_cast<int64_t>(target_sample);
    return (std::abs(sample_diff) * 1000) / static_cast<int64_t>(sample_rate);
}

/**
 * @brief Check if time differential is within tolerance per Requirement 4.2
 * 
 * @param time_diff_ms Time differential in milliseconds
 * @return true if within 250ms tolerance
 */
bool isWithinTolerance(int64_t time_diff_ms) {
    return time_diff_ms <= TimeDifferentialConstants::TOLERANCE_MS;
}

// ========================================
// PROPERTY-BASED TESTS
// ========================================

/**
 * Property 8: Time Differential Calculation
 * 
 * Tests that time differential is calculated correctly per Requirements 4.1, 4.2.
 */
void test_property_time_differential_calculation() {
    std::cout << "\n=== Property 8: Time Differential Calculation ===" << std::endl;
    std::cout << "Testing time differential calculation per Requirements 4.1, 4.2..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: Basic calculation at 44100 Hz
    // ----------------------------------------
    std::cout << "\n  Test 1: Basic calculation at 44100 Hz..." << std::endl;
    {
        tests_run++;
        
        // 44100 samples difference at 44100 Hz = 1000ms
        uint64_t actual = 88200;
        uint64_t target = 44100;
        uint32_t sample_rate = 44100;
        int64_t expected_ms = 1000;
        
        int64_t result = calculateTimeDifferentialMs(actual, target, sample_rate);
        if (result == expected_ms) {
            std::cout << "    " << (actual - target) << " samples at " << sample_rate << "Hz = " << result << "ms ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected " << expected_ms << "ms, got " << result << "ms" << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 2: Negative difference (actual < target) - absolute value
    // ----------------------------------------
    std::cout << "\n  Test 2: Negative difference (actual < target)..." << std::endl;
    {
        tests_run++;
        
        // Should return absolute value
        uint64_t actual = 44100;
        uint64_t target = 88200;
        uint32_t sample_rate = 44100;
        int64_t expected_ms = 1000;
        
        int64_t result = calculateTimeDifferentialMs(actual, target, sample_rate);
        if (result == expected_ms) {
            std::cout << "    Negative diff gives absolute value: " << result << "ms ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected " << expected_ms << "ms, got " << result << "ms" << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 3: Zero difference
    // ----------------------------------------
    std::cout << "\n  Test 3: Zero difference..." << std::endl;
    {
        tests_run++;
        
        uint64_t actual = 44100;
        uint64_t target = 44100;
        uint32_t sample_rate = 44100;
        int64_t expected_ms = 0;
        
        int64_t result = calculateTimeDifferentialMs(actual, target, sample_rate);
        if (result == expected_ms) {
            std::cout << "    Zero diff = " << result << "ms ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected " << expected_ms << "ms, got " << result << "ms" << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 4: 250ms tolerance boundary (exactly at tolerance)
    // ----------------------------------------
    std::cout << "\n  Test 4: 250ms tolerance boundary..." << std::endl;
    {
        tests_run++;
        
        // 11025 samples at 44100 Hz = 250ms
        uint64_t actual = 55125;
        uint64_t target = 44100;
        uint32_t sample_rate = 44100;
        int64_t expected_ms = 250;
        
        int64_t result = calculateTimeDifferentialMs(actual, target, sample_rate);
        if (result == expected_ms && isWithinTolerance(result)) {
            std::cout << "    11025 samples at 44100Hz = " << result << "ms (within tolerance) ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected " << expected_ms << "ms within tolerance, got " << result << "ms" << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 5: Just above tolerance (251ms)
    // ----------------------------------------
    std::cout << "\n  Test 5: Just above tolerance (251ms)..." << std::endl;
    {
        tests_run++;
        
        // 11070 samples at 44100 Hz = 251.02ms (rounds to 251ms)
        // Need at least 11070 samples to get 251ms: ceil(251 * 44100 / 1000) = 11070
        uint64_t actual = 55170;
        uint64_t target = 44100;
        uint32_t sample_rate = 44100;
        
        int64_t result = calculateTimeDifferentialMs(actual, target, sample_rate);
        if (result > 250 && !isWithinTolerance(result)) {
            std::cout << "    " << result << "ms is NOT within tolerance ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected > 250ms outside tolerance, got " << result << "ms" << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 6: High sample rate (192kHz)
    // ----------------------------------------
    std::cout << "\n  Test 6: High sample rate (192kHz)..." << std::endl;
    {
        tests_run++;
        
        // 192000 samples at 192000 Hz = 1000ms
        uint64_t actual = 384000;
        uint64_t target = 192000;
        uint32_t sample_rate = 192000;
        int64_t expected_ms = 1000;
        
        int64_t result = calculateTimeDifferentialMs(actual, target, sample_rate);
        if (result == expected_ms) {
            std::cout << "    192000 samples at 192kHz = " << result << "ms ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected " << expected_ms << "ms, got " << result << "ms" << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 7: Low sample rate (8kHz)
    // ----------------------------------------
    std::cout << "\n  Test 7: Low sample rate (8kHz)..." << std::endl;
    {
        tests_run++;
        
        // 8000 samples at 8000 Hz = 1000ms
        uint64_t actual = 16000;
        uint64_t target = 8000;
        uint32_t sample_rate = 8000;
        int64_t expected_ms = 1000;
        
        int64_t result = calculateTimeDifferentialMs(actual, target, sample_rate);
        if (result == expected_ms) {
            std::cout << "    8000 samples at 8kHz = " << result << "ms ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected " << expected_ms << "ms, got " << result << "ms" << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 8: 48kHz standard rate
    // ----------------------------------------
    std::cout << "\n  Test 8: 48kHz standard rate..." << std::endl;
    {
        tests_run++;
        
        // 12000 samples at 48000 Hz = 250ms (tolerance boundary)
        uint64_t actual = 60000;
        uint64_t target = 48000;
        uint32_t sample_rate = 48000;
        int64_t expected_ms = 250;
        
        int64_t result = calculateTimeDifferentialMs(actual, target, sample_rate);
        if (result == expected_ms) {
            std::cout << "    12000 samples at 48kHz = " << result << "ms ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected " << expected_ms << "ms, got " << result << "ms" << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 9: Large sample values (near end of long file)
    // ----------------------------------------
    std::cout << "\n  Test 9: Large sample values..." << std::endl;
    {
        tests_run++;
        
        // 10 minutes into a file at 44100 Hz
        uint64_t actual = 26460000 + 44100;  // 10 min + 1 sec
        uint64_t target = 26460000;           // 10 min
        uint32_t sample_rate = 44100;
        int64_t expected_ms = 1000;
        
        int64_t result = calculateTimeDifferentialMs(actual, target, sample_rate);
        if (result == expected_ms) {
            std::cout << "    Large sample values: " << result << "ms ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected " << expected_ms << "ms, got " << result << "ms" << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 10: Property test - random samples
    // ----------------------------------------
    std::cout << "\n  Test 10: Property test - random samples..." << std::endl;
    {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        
        int random_tests = 0;
        int random_passed = 0;
        
        // Common sample rates
        std::vector<uint32_t> sample_rates = {8000, 16000, 22050, 24000, 32000, 44100, 48000, 88200, 96000, 176400, 192000};
        
        for (int i = 0; i < 100; ++i) {
            tests_run++;
            random_tests++;
            
            // Pick random sample rate
            std::uniform_int_distribution<size_t> rate_dist(0, sample_rates.size() - 1);
            uint32_t sample_rate = sample_rates[rate_dist(gen)];
            
            // Generate random samples (up to 1 hour of audio)
            uint64_t max_samples = static_cast<uint64_t>(sample_rate) * 3600;
            std::uniform_int_distribution<uint64_t> sample_dist(0, max_samples);
            uint64_t actual = sample_dist(gen);
            uint64_t target = sample_dist(gen);
            
            // Calculate expected result manually
            int64_t sample_diff = static_cast<int64_t>(actual) - static_cast<int64_t>(target);
            int64_t expected_ms = (std::abs(sample_diff) * 1000) / static_cast<int64_t>(sample_rate);
            
            int64_t result = calculateTimeDifferentialMs(actual, target, sample_rate);
            
            if (result == expected_ms) {
                tests_passed++;
                random_passed++;
            } else {
                std::cerr << "    FAILED: actual=" << actual << ", target=" << target 
                          << ", rate=" << sample_rate << ", expected=" << expected_ms 
                          << "ms, got=" << result << "ms" << std::endl;
                assert(false);
            }
        }
        std::cout << "    " << random_passed << "/" << random_tests << " random tests passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 11: Symmetry property - |a - b| == |b - a|
    // ----------------------------------------
    std::cout << "\n  Test 11: Symmetry property..." << std::endl;
    {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        
        int random_tests = 0;
        int random_passed = 0;
        
        for (int i = 0; i < 50; ++i) {
            tests_run++;
            random_tests++;
            
            uint32_t sample_rate = 44100;
            std::uniform_int_distribution<uint64_t> sample_dist(0, 44100 * 3600);
            uint64_t a = sample_dist(gen);
            uint64_t b = sample_dist(gen);
            
            int64_t result_ab = calculateTimeDifferentialMs(a, b, sample_rate);
            int64_t result_ba = calculateTimeDifferentialMs(b, a, sample_rate);
            
            if (result_ab == result_ba) {
                tests_passed++;
                random_passed++;
            } else {
                std::cerr << "    FAILED: Symmetry violated: |" << a << " - " << b << "| = " 
                          << result_ab << "ms, |" << b << " - " << a << "| = " << result_ba << "ms" << std::endl;
                assert(false);
            }
        }
        std::cout << "    " << random_passed << "/" << random_tests << " symmetry tests passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 8: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

/**
 * Property 8b: Tolerance Check Correctness
 * 
 * Tests that tolerance checking works correctly per Requirement 4.2.
 */
void test_property_tolerance_check() {
    std::cout << "\n=== Property 8b: Tolerance Check Correctness ===" << std::endl;
    std::cout << "Testing tolerance check per Requirement 4.2..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: Values below tolerance
    // ----------------------------------------
    std::cout << "\n  Test 1: Values below tolerance..." << std::endl;
    {
        for (int64_t ms = 0; ms <= 249; ms += 50) {
            tests_run++;
            if (isWithinTolerance(ms)) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: " << ms << "ms should be within tolerance" << std::endl;
                assert(false);
            }
        }
        std::cout << "    Values 0-249ms are within tolerance ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 2: Exactly at tolerance (250ms)
    // ----------------------------------------
    std::cout << "\n  Test 2: Exactly at tolerance (250ms)..." << std::endl;
    {
        tests_run++;
        if (isWithinTolerance(250)) {
            std::cout << "    250ms is within tolerance ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: 250ms should be within tolerance" << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 3: Values above tolerance
    // ----------------------------------------
    std::cout << "\n  Test 3: Values above tolerance..." << std::endl;
    {
        for (int64_t ms = 251; ms <= 500; ms += 50) {
            tests_run++;
            if (!isWithinTolerance(ms)) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: " << ms << "ms should NOT be within tolerance" << std::endl;
                assert(false);
            }
        }
        std::cout << "    Values 251-500ms are NOT within tolerance ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 8b: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC TIME DIFFERENTIAL PROPERTY-BASED TESTS" << std::endl;
    std::cout << "**Feature: flac-bisection-seeking, Property 8: Time Differential Calculation**" << std::endl;
    std::cout << "**Validates: Requirements 4.1, 4.2**" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // Property 8: Time Differential Calculation
        test_property_time_differential_calculation();
        
        // Property 8b: Tolerance Check Correctness
        test_property_tolerance_check();
        
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "✅ ALL PROPERTY TESTS PASSED" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(70, '=') << std::endl;
        std::cerr << "❌ PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Exception: " << e.what() << std::endl;
        std::cerr << std::string(70, '=') << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n" << std::string(70, '=') << std::endl;
        std::cerr << "❌ PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Unknown exception occurred" << std::endl;
        std::cerr << std::string(70, '=') << std::endl;
        return 1;
    }
}

