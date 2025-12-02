/*
 * test_flac_duration_calculation_properties.cpp - Property-based tests for FLAC duration calculation
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
#include <string>
#include <vector>
#include <cassert>
#include <cstdint>
#include <random>
#include <limits>

// ========================================
// STANDALONE DURATION CALCULATION IMPLEMENTATION
// ========================================

/**
 * RFC 9639 Section 8.2: STREAMINFO block contains total_samples and sample_rate
 * 
 * Duration calculation per Requirements 23.1, 23.4:
 * - Use total samples from STREAMINFO
 * - Convert samples to milliseconds using sample rate
 * - Use 64-bit integers for large files (Requirement 23.8)
 * - Return 0 for unknown duration when total_samples is 0 (Requirement 23.2)
 */

/**
 * Calculate duration in milliseconds from total samples and sample rate
 * 
 * This matches the FLACStreamInfo::getDurationMs() implementation:
 *   if (sample_rate == 0 || total_samples == 0) return 0;
 *   return (total_samples * 1000) / sample_rate;
 * 
 * @param total_samples Total number of samples in the stream (0 if unknown)
 * @param sample_rate Sample rate in Hz (1-655350 per RFC 9639)
 * @return Duration in milliseconds, or 0 if unknown
 */
uint64_t calculateDurationMs(uint64_t total_samples, uint32_t sample_rate) {
    // Requirement 23.2: Handle unknown duration when total samples is 0
    if (sample_rate == 0 || total_samples == 0) {
        return 0;
    }
    
    // Requirement 23.4: Convert samples to milliseconds using sample rate
    // Requirement 23.8: Use 64-bit integers for large files
    // Formula: duration_ms = (total_samples * 1000) / sample_rate
    return (total_samples * 1000) / sample_rate;
}

/**
 * Reference implementation using floating point for verification
 */
double calculateDurationMsFloat(uint64_t total_samples, uint32_t sample_rate) {
    if (sample_rate == 0 || total_samples == 0) {
        return 0.0;
    }
    return (static_cast<double>(total_samples) * 1000.0) / static_cast<double>(sample_rate);
}

// ========================================
// PROPERTY-BASED TESTS
// ========================================

// ========================================
// PROPERTY 20: Duration Calculation
// ========================================
// **Feature: flac-demuxer, Property 20: Duration Calculation**
// **Validates: Requirements 23.1, 23.4**
//
// For any FLAC stream with valid STREAMINFO, the duration calculation
// SHALL use total samples divided by sample rate.

void test_property_duration_calculation() {
    std::cout << "\n=== Property 20: Duration Calculation ===" << std::endl;
    std::cout << "Testing duration calculation from total samples and sample rate..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: Unknown duration (total_samples = 0)
    // ----------------------------------------
    std::cout << "\n  Test 1: Unknown duration (total_samples = 0)..." << std::endl;
    {
        // Requirement 23.2: Handle unknown duration when total samples is 0
        uint32_t sample_rates[] = {44100, 48000, 96000, 192000, 8000, 22050};
        
        for (uint32_t sr : sample_rates) {
            tests_run++;
            uint64_t duration = calculateDurationMs(0, sr);
            if (duration == 0) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: total_samples=0, sample_rate=" << sr 
                          << " should return 0, got " << duration << std::endl;
                assert(false && "Unknown duration should return 0");
            }
        }
        std::cout << "    All unknown duration tests passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 2: Invalid sample rate (sample_rate = 0)
    // ----------------------------------------
    std::cout << "\n  Test 2: Invalid sample rate (sample_rate = 0)..." << std::endl;
    {
        uint64_t total_samples_values[] = {1000, 44100, 1000000, 0xFFFFFFFFFFFFFFFFULL};
        
        for (uint64_t ts : total_samples_values) {
            tests_run++;
            uint64_t duration = calculateDurationMs(ts, 0);
            if (duration == 0) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: sample_rate=0 should return 0, got " << duration << std::endl;
                assert(false && "Invalid sample rate should return 0");
            }
        }
        std::cout << "    All invalid sample rate tests passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 3: Known duration calculations
    // ----------------------------------------
    std::cout << "\n  Test 3: Known duration calculations..." << std::endl;
    {
        struct TestCase {
            uint64_t total_samples;
            uint32_t sample_rate;
            uint64_t expected_ms;
            const char* description;
        };
        
        std::vector<TestCase> test_cases = {
            // Exact second boundaries
            {44100, 44100, 1000, "1 second at 44.1kHz"},
            {48000, 48000, 1000, "1 second at 48kHz"},
            {96000, 96000, 1000, "1 second at 96kHz"},
            {192000, 192000, 1000, "1 second at 192kHz"},
            
            // Multiple seconds
            {441000, 44100, 10000, "10 seconds at 44.1kHz"},
            {4410000, 44100, 100000, "100 seconds at 44.1kHz"},
            {44100 * 60, 44100, 60000, "1 minute at 44.1kHz"},
            {44100 * 3600, 44100, 3600000, "1 hour at 44.1kHz"},
            
            // Fractional seconds (integer truncation)
            {22050, 44100, 500, "0.5 seconds at 44.1kHz"},
            {11025, 44100, 250, "0.25 seconds at 44.1kHz"},
            
            // Common audio file durations
            {44100 * 180, 44100, 180000, "3 minute song at 44.1kHz"},
            {48000 * 240, 48000, 240000, "4 minute song at 48kHz"},
            
            // Low sample rates (telephony)
            {8000, 8000, 1000, "1 second at 8kHz"},
            {16000, 16000, 1000, "1 second at 16kHz"},
            
            // High sample rates
            {176400, 176400, 1000, "1 second at 176.4kHz"},
            {352800, 352800, 1000, "1 second at 352.8kHz"},
        };
        
        for (const auto& tc : test_cases) {
            tests_run++;
            uint64_t duration = calculateDurationMs(tc.total_samples, tc.sample_rate);
            if (duration == tc.expected_ms) {
                std::cout << "    " << tc.description << ": " << duration << " ms ✓" << std::endl;
                tests_passed++;
            } else {
                std::cerr << "    FAILED: " << tc.description << ": expected " 
                          << tc.expected_ms << " ms, got " << duration << " ms" << std::endl;
                assert(false && "Duration calculation mismatch");
            }
        }
    }
    
    // ----------------------------------------
    // Test 4: Large file support (64-bit integers)
    // ----------------------------------------
    std::cout << "\n  Test 4: Large file support (64-bit integers)..." << std::endl;
    {
        // Requirement 23.8: Use 64-bit integers for large files
        // Test files that would overflow 32-bit integers
        
        struct LargeFileTest {
            uint64_t total_samples;
            uint32_t sample_rate;
            const char* description;
        };
        
        std::vector<LargeFileTest> large_tests = {
            // 24-hour audio at 44.1kHz = 3,810,240,000 samples (exceeds 32-bit)
            {3810240000ULL, 44100, "24 hours at 44.1kHz"},
            
            // 100-hour audio at 48kHz = 17,280,000,000 samples
            {17280000000ULL, 48000, "100 hours at 48kHz"},
            
            // Maximum 36-bit sample count from STREAMINFO (68,719,476,735)
            {68719476735ULL, 44100, "Max 36-bit samples at 44.1kHz"},
            
            // Large file at high sample rate
            {192000ULL * 3600 * 24, 192000, "24 hours at 192kHz"},
        };
        
        for (const auto& lt : large_tests) {
            tests_run++;
            
            uint64_t duration = calculateDurationMs(lt.total_samples, lt.sample_rate);
            double expected_float = calculateDurationMsFloat(lt.total_samples, lt.sample_rate);
            
            // Allow for integer truncation difference (should be within 1ms)
            int64_t diff = static_cast<int64_t>(duration) - static_cast<int64_t>(expected_float);
            if (diff >= -1 && diff <= 1) {
                std::cout << "    " << lt.description << ": " << duration << " ms ✓" << std::endl;
                tests_passed++;
            } else {
                std::cerr << "    FAILED: " << lt.description << ": got " << duration 
                          << " ms, expected ~" << expected_float << " ms" << std::endl;
                assert(false && "Large file duration calculation failed");
            }
        }
    }
    
    // ----------------------------------------
    // Test 5: Random property testing
    // ----------------------------------------
    std::cout << "\n  Test 5: Random property testing (100 iterations)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        
        // Valid sample rates per RFC 9639 (1-655350 Hz)
        std::uniform_int_distribution<uint32_t> sr_dist(1, 655350);
        
        // Total samples (0 to max 36-bit value from STREAMINFO)
        std::uniform_int_distribution<uint64_t> ts_dist(1, 68719476735ULL);
        
        int random_passed = 0;
        
        for (int i = 0; i < 100; ++i) {
            uint32_t sample_rate = sr_dist(gen);
            uint64_t total_samples = ts_dist(gen);
            
            tests_run++;
            
            uint64_t duration = calculateDurationMs(total_samples, sample_rate);
            double expected_float = calculateDurationMsFloat(total_samples, sample_rate);
            
            // Property: duration should be approximately (total_samples * 1000) / sample_rate
            // Allow for integer truncation (difference should be < 1)
            double diff = std::abs(static_cast<double>(duration) - expected_float);
            
            if (diff < 1.0) {
                tests_passed++;
                random_passed++;
            } else {
                std::cerr << "    FAILED: total_samples=" << total_samples 
                          << ", sample_rate=" << sample_rate
                          << ", duration=" << duration 
                          << ", expected=" << expected_float << std::endl;
                assert(false && "Random property test failed");
            }
        }
        std::cout << "    " << random_passed << "/100 random tests passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 6: Duration formula verification
    // ----------------------------------------
    std::cout << "\n  Test 6: Duration formula verification..." << std::endl;
    {
        // Property: duration_ms = (total_samples * 1000) / sample_rate
        // This is the exact formula from Requirements 23.1 and 23.4
        
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint32_t> sr_dist(8000, 192000);
        std::uniform_int_distribution<uint64_t> ts_dist(1, 1000000000ULL);
        
        int formula_passed = 0;
        
        for (int i = 0; i < 100; ++i) {
            uint32_t sample_rate = sr_dist(gen);
            uint64_t total_samples = ts_dist(gen);
            
            tests_run++;
            
            uint64_t duration = calculateDurationMs(total_samples, sample_rate);
            uint64_t expected = (total_samples * 1000) / sample_rate;
            
            if (duration == expected) {
                tests_passed++;
                formula_passed++;
            } else {
                std::cerr << "    FAILED: Formula mismatch for total_samples=" << total_samples 
                          << ", sample_rate=" << sample_rate << std::endl;
                assert(false && "Duration formula verification failed");
            }
        }
        std::cout << "    " << formula_passed << "/100 formula verification tests passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 7: Monotonicity property
    // ----------------------------------------
    std::cout << "\n  Test 7: Monotonicity property..." << std::endl;
    {
        // Property: For fixed sample_rate, if total_samples increases, duration should not decrease
        
        uint32_t sample_rate = 44100;
        uint64_t prev_duration = 0;
        int monotonic_passed = 0;
        
        for (uint64_t samples = 0; samples <= 44100 * 100; samples += 4410) {
            tests_run++;
            
            uint64_t duration = calculateDurationMs(samples, sample_rate);
            
            if (duration >= prev_duration) {
                tests_passed++;
                monotonic_passed++;
            } else {
                std::cerr << "    FAILED: Duration decreased from " << prev_duration 
                          << " to " << duration << " at samples=" << samples << std::endl;
                assert(false && "Duration should be monotonically increasing");
            }
            
            prev_duration = duration;
        }
        std::cout << "    " << monotonic_passed << " monotonicity tests passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 20: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC DURATION CALCULATION PROPERTY-BASED TESTS" << std::endl;
    std::cout << "**Feature: flac-demuxer, Property 20: Duration Calculation**" << std::endl;
    std::cout << "**Validates: Requirements 23.1, 23.4**" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // Property 20: Duration Calculation
        // For any FLAC stream with valid STREAMINFO, the duration calculation
        // SHALL use total samples divided by sample rate.
        test_property_duration_calculation();
        
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
