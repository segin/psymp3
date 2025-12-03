/*
 * test_flac_byte_position_estimation_properties.cpp - Property-based tests for FLAC byte position estimation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * **Feature: flac-bisection-seeking, Property 1: Byte Position Estimation Correctness**
 * **Validates: Requirements 1.1, 1.2, 1.4, 1.5**
 *
 * For any valid target sample, total samples, audio offset, and file size,
 * the estimated byte position SHALL equal:
 *   audio_offset + (target_sample / total_samples) * (file_size - audio_offset)
 * clamped to the valid range [audio_offset, file_size - 64].
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

// ========================================
// STANDALONE BYTE POSITION ESTIMATION
// ========================================

/**
 * Reference implementation of byte position estimation
 * This mirrors the FLACDemuxer::estimateBytePosition_unlocked() implementation
 * 
 * Requirements:
 * - 1.1: Calculate position using audio_offset + (target/total) * audio_size
 * - 1.2: Audio data size is file_size - audio_data_offset
 * - 1.4: Clamp to file_size - 64 if estimated position exceeds file size
 * - 1.5: Clamp to audio_data_offset if estimated position is before audio data
 */
uint64_t estimateBytePosition(uint64_t target_sample, uint64_t total_samples,
                               uint64_t audio_offset, uint64_t file_size) {
    // Handle edge case when total_samples is 0
    if (total_samples == 0) {
        return audio_offset;
    }
    
    // Calculate audio data size (Requirement 1.2)
    uint64_t audio_data_size = file_size - audio_offset;
    
    if (audio_data_size == 0) {
        return audio_offset;
    }
    
    // Calculate initial byte position using linear interpolation (Requirement 1.1)
    // estimated_pos = audio_offset + (target_sample / total_samples) * audio_data_size
    double ratio = static_cast<double>(target_sample) / static_cast<double>(total_samples);
    uint64_t estimated_offset = static_cast<uint64_t>(ratio * static_cast<double>(audio_data_size));
    uint64_t estimated_pos = audio_offset + estimated_offset;
    
    // Requirement 1.5: Clamp to audio_offset if before audio data
    if (estimated_pos < audio_offset) {
        return audio_offset;
    }
    
    // Requirement 1.4: Clamp to file_size - 64 if exceeds file size
    static constexpr uint64_t MIN_FRAME_ROOM = 64;
    if (file_size > MIN_FRAME_ROOM && estimated_pos >= file_size - MIN_FRAME_ROOM) {
        return file_size - MIN_FRAME_ROOM;
    }
    
    return estimated_pos;
}

/**
 * Helper to format large numbers with commas for readability
 */
std::string formatNumber(uint64_t n) {
    std::string s = std::to_string(n);
    int insertPosition = static_cast<int>(s.length()) - 3;
    while (insertPosition > 0) {
        s.insert(insertPosition, ",");
        insertPosition -= 3;
    }
    return s;
}

// ========================================
// PROPERTY-BASED TESTS
// ========================================

/**
 * Property 1: Byte Position Estimation Correctness
 * 
 * For any valid target sample, total samples, audio offset, and file size,
 * the estimated byte position SHALL equal:
 *   audio_offset + (target_sample / total_samples) * (file_size - audio_offset)
 * clamped to the valid range [audio_offset, file_size - 64].
 */
void test_property_byte_position_estimation_correctness() {
    std::cout << "\n=== Property 1: Byte Position Estimation Correctness ===" << std::endl;
    std::cout << "Testing byte position estimation formula..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: Zero total_samples returns audio_offset
    // ----------------------------------------
    std::cout << "\n  Test 1: Zero total_samples returns audio_offset..." << std::endl;
    {
        tests_run++;
        uint64_t result = estimateBytePosition(1000, 0, 42, 1000000);
        if (result == 42) {
            std::cout << "    total_samples=0 -> returns audio_offset=42 ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected 42, got " << result << std::endl;
            assert(false && "Zero total_samples should return audio_offset");
        }
    }
    
    // ----------------------------------------
    // Test 2: Target sample 0 returns audio_offset
    // ----------------------------------------
    std::cout << "\n  Test 2: Target sample 0 returns audio_offset..." << std::endl;
    {
        tests_run++;
        uint64_t result = estimateBytePosition(0, 44100 * 300, 8192, 50000000);
        if (result == 8192) {
            std::cout << "    target_sample=0 -> returns audio_offset=8192 ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected 8192, got " << result << std::endl;
            assert(false && "Target sample 0 should return audio_offset");
        }
    }
    
    // ----------------------------------------
    // Test 3: Target sample equals total_samples returns clamped position
    // ----------------------------------------
    std::cout << "\n  Test 3: Target sample equals total_samples returns clamped position..." << std::endl;
    {
        tests_run++;
        uint64_t total = 44100 * 300;  // 5 minutes at 44.1kHz
        uint64_t file_size = 50000000;
        uint64_t audio_offset = 8192;
        uint64_t result = estimateBytePosition(total, total, audio_offset, file_size);
        uint64_t expected = file_size - 64;  // Should be clamped
        if (result == expected) {
            std::cout << "    target=total -> returns file_size-64=" << expected << " ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected " << expected << ", got " << result << std::endl;
            assert(false && "Target equals total should return clamped position");
        }
    }
    
    // ----------------------------------------
    // Test 4: Midpoint calculation
    // ----------------------------------------
    std::cout << "\n  Test 4: Midpoint calculation..." << std::endl;
    {
        tests_run++;
        uint64_t total = 1000000;
        uint64_t file_size = 10000000;
        uint64_t audio_offset = 1000;
        uint64_t target = 500000;  // 50%
        
        uint64_t result = estimateBytePosition(target, total, audio_offset, file_size);
        
        // Expected: audio_offset + 0.5 * (file_size - audio_offset)
        // = 1000 + 0.5 * 9999000 = 1000 + 4999500 = 5000500
        uint64_t expected = audio_offset + (file_size - audio_offset) / 2;
        
        if (result == expected) {
            std::cout << "    50% position -> " << result << " ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected " << expected << ", got " << result << std::endl;
            assert(false && "Midpoint calculation incorrect");
        }
    }
    
    // ----------------------------------------
    // Test 5: Quarter position calculation
    // ----------------------------------------
    std::cout << "\n  Test 5: Quarter position calculation..." << std::endl;
    {
        tests_run++;
        uint64_t total = 1000000;
        uint64_t file_size = 10000000;
        uint64_t audio_offset = 0;
        uint64_t target = 250000;  // 25%
        
        uint64_t result = estimateBytePosition(target, total, audio_offset, file_size);
        
        // Expected: 0 + 0.25 * 10000000 = 2500000
        uint64_t expected = 2500000;
        
        if (result == expected) {
            std::cout << "    25% position -> " << result << " ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected " << expected << ", got " << result << std::endl;
            assert(false && "Quarter position calculation incorrect");
        }
    }
    
    // ----------------------------------------
    // Test 6: Clamping to file_size - 64
    // ----------------------------------------
    std::cout << "\n  Test 6: Clamping to file_size - 64..." << std::endl;
    {
        tests_run++;
        uint64_t total = 100;
        uint64_t file_size = 1000;
        uint64_t audio_offset = 100;
        uint64_t target = 99;  // 99% - should be near end
        
        uint64_t result = estimateBytePosition(target, total, audio_offset, file_size);
        uint64_t max_allowed = file_size - 64;  // 936
        
        if (result <= max_allowed) {
            std::cout << "    99% position clamped to <= " << max_allowed << " (got " << result << ") ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Result " << result << " exceeds max " << max_allowed << std::endl;
            assert(false && "Position should be clamped to file_size - 64");
        }
    }
    
    // ----------------------------------------
    // Test 7: Result always >= audio_offset
    // ----------------------------------------
    std::cout << "\n  Test 7: Result always >= audio_offset (random tests)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> offset_dist(0, 100000);
        std::uniform_int_distribution<uint64_t> size_dist(1000, 100000000);
        std::uniform_int_distribution<uint64_t> samples_dist(1, 100000000);
        
        int random_tests = 0;
        int random_passed = 0;
        
        for (int i = 0; i < 100; ++i) {
            uint64_t audio_offset = offset_dist(gen);
            uint64_t file_size = audio_offset + size_dist(gen);
            uint64_t total_samples = samples_dist(gen);
            std::uniform_int_distribution<uint64_t> target_dist(0, total_samples);
            uint64_t target_sample = target_dist(gen);
            
            tests_run++;
            random_tests++;
            
            uint64_t result = estimateBytePosition(target_sample, total_samples, audio_offset, file_size);
            
            if (result >= audio_offset) {
                tests_passed++;
                random_passed++;
            } else {
                std::cerr << "    FAILED: Result " << result << " < audio_offset " << audio_offset << std::endl;
                assert(false && "Result should always be >= audio_offset");
            }
        }
        std::cout << "    " << random_passed << "/" << random_tests << " random tests passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 8: Result always <= file_size - 64 (when file_size > 64)
    // ----------------------------------------
    std::cout << "\n  Test 8: Result always <= file_size - 64 (random tests)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> offset_dist(0, 100000);
        std::uniform_int_distribution<uint64_t> size_dist(1000, 100000000);
        std::uniform_int_distribution<uint64_t> samples_dist(1, 100000000);
        
        int random_tests = 0;
        int random_passed = 0;
        
        for (int i = 0; i < 100; ++i) {
            uint64_t audio_offset = offset_dist(gen);
            uint64_t file_size = audio_offset + size_dist(gen);
            uint64_t total_samples = samples_dist(gen);
            std::uniform_int_distribution<uint64_t> target_dist(0, total_samples);
            uint64_t target_sample = target_dist(gen);
            
            tests_run++;
            random_tests++;
            
            uint64_t result = estimateBytePosition(target_sample, total_samples, audio_offset, file_size);
            uint64_t max_allowed = file_size - 64;
            
            if (result <= max_allowed) {
                tests_passed++;
                random_passed++;
            } else {
                std::cerr << "    FAILED: Result " << result << " > max " << max_allowed << std::endl;
                assert(false && "Result should always be <= file_size - 64");
            }
        }
        std::cout << "    " << random_passed << "/" << random_tests << " random tests passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 9: Linear interpolation property
    // ----------------------------------------
    std::cout << "\n  Test 9: Linear interpolation property..." << std::endl;
    {
        // For positions not at boundaries, the result should follow linear interpolation
        // result ≈ audio_offset + (target/total) * audio_size
        
        std::random_device rd;
        std::mt19937_64 gen(rd());
        
        int linear_tests = 0;
        int linear_passed = 0;
        
        for (int i = 0; i < 50; ++i) {
            // Use values that won't hit clamping boundaries
            uint64_t audio_offset = 10000;
            uint64_t file_size = 100000000;  // 100 MB
            uint64_t total_samples = 44100 * 600;  // 10 minutes
            
            // Target in middle 80% to avoid boundary clamping
            std::uniform_int_distribution<uint64_t> target_dist(
                total_samples / 10, total_samples * 9 / 10);
            uint64_t target_sample = target_dist(gen);
            
            tests_run++;
            linear_tests++;
            
            uint64_t result = estimateBytePosition(target_sample, total_samples, audio_offset, file_size);
            
            // Calculate expected using double precision
            double ratio = static_cast<double>(target_sample) / static_cast<double>(total_samples);
            uint64_t audio_size = file_size - audio_offset;
            uint64_t expected = audio_offset + static_cast<uint64_t>(ratio * audio_size);
            
            // Allow small rounding difference (1 byte)
            int64_t diff = static_cast<int64_t>(result) - static_cast<int64_t>(expected);
            if (std::abs(diff) <= 1) {
                tests_passed++;
                linear_passed++;
            } else {
                std::cerr << "    FAILED: Expected ~" << expected << ", got " << result 
                          << " (diff=" << diff << ")" << std::endl;
                assert(false && "Linear interpolation should be accurate");
            }
        }
        std::cout << "    " << linear_passed << "/" << linear_tests << " linear interpolation tests passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 10: Monotonicity - larger target -> larger or equal result
    // ----------------------------------------
    std::cout << "\n  Test 10: Monotonicity property..." << std::endl;
    {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        
        int mono_tests = 0;
        int mono_passed = 0;
        
        for (int i = 0; i < 50; ++i) {
            uint64_t audio_offset = 8192;
            uint64_t file_size = 50000000;
            uint64_t total_samples = 44100 * 300;
            
            std::uniform_int_distribution<uint64_t> target_dist(0, total_samples - 1);
            uint64_t target1 = target_dist(gen);
            uint64_t target2 = target1 + 1 + (target_dist(gen) % 10000);
            if (target2 > total_samples) target2 = total_samples;
            
            tests_run++;
            mono_tests++;
            
            uint64_t result1 = estimateBytePosition(target1, total_samples, audio_offset, file_size);
            uint64_t result2 = estimateBytePosition(target2, total_samples, audio_offset, file_size);
            
            if (result2 >= result1) {
                tests_passed++;
                mono_passed++;
            } else {
                std::cerr << "    FAILED: target " << target1 << " -> " << result1 
                          << ", target " << target2 << " -> " << result2 << std::endl;
                assert(false && "Larger target should give larger or equal result");
            }
        }
        std::cout << "    " << mono_passed << "/" << mono_tests << " monotonicity tests passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 1: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

/**
 * Property 1b: Edge Cases for Byte Position Estimation
 * 
 * Tests specific edge cases mentioned in the requirements.
 */
void test_property_byte_position_edge_cases() {
    std::cout << "\n=== Property 1b: Byte Position Estimation Edge Cases ===" << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Edge Case 1: Very small file
    // ----------------------------------------
    std::cout << "\n  Edge Case 1: Very small file..." << std::endl;
    {
        tests_run++;
        uint64_t result = estimateBytePosition(50, 100, 10, 100);
        // file_size - 64 = 36, which is less than audio_offset + estimated
        // So should be clamped to 36
        if (result <= 36 && result >= 10) {
            std::cout << "    Small file handled correctly: " << result << " ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Result " << result << " out of expected range [10, 36]" << std::endl;
            assert(false && "Small file edge case failed");
        }
    }
    
    // ----------------------------------------
    // Edge Case 2: audio_offset equals file_size - 64
    // ----------------------------------------
    std::cout << "\n  Edge Case 2: audio_offset equals file_size - 64..." << std::endl;
    {
        tests_run++;
        uint64_t audio_offset = 936;
        uint64_t file_size = 1000;
        uint64_t result = estimateBytePosition(50, 100, audio_offset, file_size);
        // audio_size = 64, any position should be clamped
        if (result == audio_offset || result == file_size - 64) {
            std::cout << "    Boundary case handled: " << result << " ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Result " << result << " unexpected" << std::endl;
            assert(false && "Boundary edge case failed");
        }
    }
    
    // ----------------------------------------
    // Edge Case 3: Large file (> 4GB)
    // ----------------------------------------
    std::cout << "\n  Edge Case 3: Large file (> 4GB)..." << std::endl;
    {
        tests_run++;
        uint64_t file_size = 5ULL * 1024 * 1024 * 1024;  // 5 GB
        uint64_t audio_offset = 8192;
        uint64_t total_samples = 44100ULL * 3600 * 2;  // 2 hours at 44.1kHz
        uint64_t target = total_samples / 2;  // 50%
        
        uint64_t result = estimateBytePosition(target, total_samples, audio_offset, file_size);
        
        // Should be approximately in the middle
        uint64_t expected_approx = audio_offset + (file_size - audio_offset) / 2;
        int64_t diff = static_cast<int64_t>(result) - static_cast<int64_t>(expected_approx);
        
        if (std::abs(diff) < 1000) {  // Within 1KB of expected
            std::cout << "    Large file handled: " << formatNumber(result) << " ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Result " << formatNumber(result) 
                      << " far from expected " << formatNumber(expected_approx) << std::endl;
            assert(false && "Large file edge case failed");
        }
    }
    
    // ----------------------------------------
    // Edge Case 4: Very high sample rate (192kHz)
    // ----------------------------------------
    std::cout << "\n  Edge Case 4: Very high sample rate (192kHz)..." << std::endl;
    {
        tests_run++;
        uint64_t file_size = 500000000;  // 500 MB
        uint64_t audio_offset = 8192;
        uint64_t total_samples = 192000ULL * 600;  // 10 minutes at 192kHz
        uint64_t target = total_samples / 4;  // 25%
        
        uint64_t result = estimateBytePosition(target, total_samples, audio_offset, file_size);
        
        // Should be approximately at 25%
        uint64_t expected_approx = audio_offset + (file_size - audio_offset) / 4;
        int64_t diff = static_cast<int64_t>(result) - static_cast<int64_t>(expected_approx);
        
        if (std::abs(diff) < 100) {
            std::cout << "    High sample rate handled: " << formatNumber(result) << " ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Result " << formatNumber(result) 
                      << " far from expected " << formatNumber(expected_approx) << std::endl;
            assert(false && "High sample rate edge case failed");
        }
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 1b: " << tests_passed << "/" << tests_run << " edge case tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC BYTE POSITION ESTIMATION PROPERTY-BASED TESTS" << std::endl;
    std::cout << "**Feature: flac-bisection-seeking, Property 1: Byte Position Estimation Correctness**" << std::endl;
    std::cout << "**Validates: Requirements 1.1, 1.2, 1.4, 1.5**" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // Property 1: Byte Position Estimation Correctness
        test_property_byte_position_estimation_correctness();
        
        // Property 1b: Edge Cases
        test_property_byte_position_edge_cases();
        
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
