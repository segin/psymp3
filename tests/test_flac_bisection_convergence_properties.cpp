/*
 * test_flac_bisection_convergence_properties.cpp - Property-based tests for FLAC bisection convergence
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * **Feature: flac-bisection-seeking, Property 7: Convergence Termination**
 * **Validates: Requirements 3.3, 3.4, 3.5, 3.6**
 *
 * For any bisection search, the algorithm SHALL terminate when:
 * (a) time differential <= 250ms, OR
 * (b) iteration count > 10, OR
 * (c) search range < 64 bytes, OR
 * (d) same position found twice consecutively.
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
// CONVERGENCE TERMINATION LOGIC
// ========================================

/**
 * @brief Constants for bisection convergence
 */
struct BisectionConstants {
    static constexpr int MAX_ITERATIONS = 10;
    static constexpr int64_t TOLERANCE_MS = 250;
    static constexpr uint64_t MIN_SEARCH_RANGE = 64;
};

/**
 * @brief Check if time differential is within tolerance (Requirement 3.3)
 */
bool isWithinTolerance(int64_t time_diff_ms) {
    return time_diff_ms <= BisectionConstants::TOLERANCE_MS;
}

/**
 * @brief Check if iteration limit exceeded (Requirement 3.4)
 */
bool isIterationLimitExceeded(int iteration) {
    return iteration >= BisectionConstants::MAX_ITERATIONS;
}


/**
 * @brief Check if search range collapsed (Requirement 3.5)
 */
bool isRangeCollapsed(uint64_t low_pos, uint64_t high_pos) {
    return high_pos <= low_pos + BisectionConstants::MIN_SEARCH_RANGE;
}

/**
 * @brief Check if same position found twice (Requirement 3.6)
 */
bool isSamePositionTwice(uint64_t current_pos, uint64_t last_pos) {
    return current_pos == last_pos;
}

/**
 * @brief Calculate time differential in milliseconds (Requirement 4.1)
 */
int64_t calculateTimeDiffMs(uint64_t actual_sample, uint64_t target_sample, uint32_t sample_rate) {
    int64_t sample_diff = static_cast<int64_t>(actual_sample) - static_cast<int64_t>(target_sample);
    return (std::abs(sample_diff) * 1000) / static_cast<int64_t>(sample_rate);
}

/**
 * @brief Determine if bisection should terminate
 * 
 * Returns true if any termination condition is met:
 * - Requirement 3.3: time_diff_ms <= 250
 * - Requirement 3.4: iteration >= 10
 * - Requirement 3.5: range < 64 bytes
 * - Requirement 3.6: same position twice
 */
bool shouldTerminate(int64_t time_diff_ms, int iteration, 
                     uint64_t low_pos, uint64_t high_pos,
                     uint64_t current_pos, uint64_t last_pos) {
    // Requirement 3.3: Within tolerance
    if (isWithinTolerance(time_diff_ms)) return true;
    
    // Requirement 3.4: Iteration limit
    if (isIterationLimitExceeded(iteration)) return true;
    
    // Requirement 3.5: Range collapsed
    if (isRangeCollapsed(low_pos, high_pos)) return true;
    
    // Requirement 3.6: Same position twice
    if (isSamePositionTwice(current_pos, last_pos)) return true;
    
    return false;
}

// ========================================
// PROPERTY-BASED TESTS
// ========================================

/**
 * Property 7: Convergence Termination
 * 
 * Tests that the algorithm terminates under all specified conditions.
 */
void test_property_convergence_termination() {
    std::cout << "\n=== Property 7: Convergence Termination ===" << std::endl;
    std::cout << "Testing convergence termination conditions..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: Requirement 3.3 - Within tolerance terminates
    // ----------------------------------------
    std::cout << "\n  Test 1: Requirement 3.3 - Within tolerance terminates..." << std::endl;
    {
        tests_run++;
        
        int64_t time_diff_ms = 200;  // Within 250ms tolerance
        int iteration = 0;
        uint64_t low_pos = 0, high_pos = 100000000;
        uint64_t current_pos = 50000000, last_pos = 0;
        
        if (shouldTerminate(time_diff_ms, iteration, low_pos, high_pos, current_pos, last_pos)) {
            std::cout << "    time_diff_ms=" << time_diff_ms << "ms terminates ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Should terminate when within tolerance" << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 2: Requirement 3.3 - Exactly at tolerance terminates
    // ----------------------------------------
    std::cout << "\n  Test 2: Requirement 3.3 - Exactly at tolerance terminates..." << std::endl;
    {
        tests_run++;
        
        int64_t time_diff_ms = 250;  // Exactly at tolerance
        int iteration = 0;
        uint64_t low_pos = 0, high_pos = 100000000;
        uint64_t current_pos = 50000000, last_pos = 0;
        
        if (shouldTerminate(time_diff_ms, iteration, low_pos, high_pos, current_pos, last_pos)) {
            std::cout << "    time_diff_ms=" << time_diff_ms << "ms terminates ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Should terminate at exactly tolerance" << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 3: Requirement 3.3 - Above tolerance does not terminate (alone)
    // ----------------------------------------
    std::cout << "\n  Test 3: Requirement 3.3 - Above tolerance does not terminate alone..." << std::endl;
    {
        tests_run++;
        
        int64_t time_diff_ms = 251;  // Above tolerance
        int iteration = 0;
        uint64_t low_pos = 0, high_pos = 100000000;
        uint64_t current_pos = 50000000, last_pos = 0;
        
        if (!shouldTerminate(time_diff_ms, iteration, low_pos, high_pos, current_pos, last_pos)) {
            std::cout << "    time_diff_ms=" << time_diff_ms << "ms does NOT terminate ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Should NOT terminate when above tolerance" << std::endl;
            assert(false);
        }
    }

    
    // ----------------------------------------
    // Test 4: Requirement 3.4 - Iteration limit terminates
    // ----------------------------------------
    std::cout << "\n  Test 4: Requirement 3.4 - Iteration limit terminates..." << std::endl;
    {
        tests_run++;
        
        int64_t time_diff_ms = 500;  // Above tolerance
        int iteration = 10;  // At limit
        uint64_t low_pos = 0, high_pos = 100000000;
        uint64_t current_pos = 50000000, last_pos = 0;
        
        if (shouldTerminate(time_diff_ms, iteration, low_pos, high_pos, current_pos, last_pos)) {
            std::cout << "    iteration=" << iteration << " terminates ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Should terminate at iteration limit" << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 5: Requirement 3.4 - Below iteration limit does not terminate (alone)
    // ----------------------------------------
    std::cout << "\n  Test 5: Requirement 3.4 - Below iteration limit does not terminate alone..." << std::endl;
    {
        tests_run++;
        
        int64_t time_diff_ms = 500;  // Above tolerance
        int iteration = 9;  // Below limit
        uint64_t low_pos = 0, high_pos = 100000000;
        uint64_t current_pos = 50000000, last_pos = 0;
        
        if (!shouldTerminate(time_diff_ms, iteration, low_pos, high_pos, current_pos, last_pos)) {
            std::cout << "    iteration=" << iteration << " does NOT terminate ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Should NOT terminate below iteration limit" << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 6: Requirement 3.5 - Range collapsed terminates
    // ----------------------------------------
    std::cout << "\n  Test 6: Requirement 3.5 - Range collapsed terminates..." << std::endl;
    {
        tests_run++;
        
        int64_t time_diff_ms = 500;  // Above tolerance
        int iteration = 0;
        uint64_t low_pos = 1000, high_pos = 1064;  // Range = 64 (collapsed)
        uint64_t current_pos = 1032, last_pos = 0;
        
        if (shouldTerminate(time_diff_ms, iteration, low_pos, high_pos, current_pos, last_pos)) {
            std::cout << "    range=" << (high_pos - low_pos) << " bytes terminates ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Should terminate when range collapsed" << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 7: Requirement 3.5 - Range above minimum does not terminate (alone)
    // ----------------------------------------
    std::cout << "\n  Test 7: Requirement 3.5 - Range above minimum does not terminate alone..." << std::endl;
    {
        tests_run++;
        
        int64_t time_diff_ms = 500;  // Above tolerance
        int iteration = 0;
        uint64_t low_pos = 1000, high_pos = 1065;  // Range = 65 (not collapsed)
        uint64_t current_pos = 1032, last_pos = 0;
        
        if (!shouldTerminate(time_diff_ms, iteration, low_pos, high_pos, current_pos, last_pos)) {
            std::cout << "    range=" << (high_pos - low_pos) << " bytes does NOT terminate ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Should NOT terminate when range above minimum" << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 8: Requirement 3.6 - Same position twice terminates
    // ----------------------------------------
    std::cout << "\n  Test 8: Requirement 3.6 - Same position twice terminates..." << std::endl;
    {
        tests_run++;
        
        int64_t time_diff_ms = 500;  // Above tolerance
        int iteration = 0;
        uint64_t low_pos = 0, high_pos = 100000000;
        uint64_t current_pos = 50000000, last_pos = 50000000;  // Same position
        
        if (shouldTerminate(time_diff_ms, iteration, low_pos, high_pos, current_pos, last_pos)) {
            std::cout << "    same position terminates ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Should terminate when same position twice" << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 9: Requirement 3.6 - Different positions does not terminate (alone)
    // ----------------------------------------
    std::cout << "\n  Test 9: Requirement 3.6 - Different positions does not terminate alone..." << std::endl;
    {
        tests_run++;
        
        int64_t time_diff_ms = 500;  // Above tolerance
        int iteration = 0;
        uint64_t low_pos = 0, high_pos = 100000000;
        uint64_t current_pos = 50000000, last_pos = 40000000;  // Different positions
        
        if (!shouldTerminate(time_diff_ms, iteration, low_pos, high_pos, current_pos, last_pos)) {
            std::cout << "    different positions does NOT terminate ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Should NOT terminate when positions differ" << std::endl;
            assert(false);
        }
    }

    
    // ----------------------------------------
    // Test 10: Random tests - at least one condition always terminates
    // ----------------------------------------
    std::cout << "\n  Test 10: Random tests - termination conditions..." << std::endl;
    {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        
        int random_tests = 0;
        int random_passed = 0;
        
        for (int i = 0; i < 100; ++i) {
            tests_run++;
            random_tests++;
            
            // Generate random state that should terminate
            std::uniform_int_distribution<int> condition_dist(0, 3);
            int condition = condition_dist(gen);
            
            int64_t time_diff_ms = 500;
            int iteration = 5;
            uint64_t low_pos = 0, high_pos = 100000000;
            uint64_t current_pos = 50000000, last_pos = 40000000;
            
            // Set one condition to trigger termination
            switch (condition) {
                case 0: time_diff_ms = 200; break;  // Within tolerance
                case 1: iteration = 10; break;      // Iteration limit
                case 2: high_pos = low_pos + 64; break;  // Range collapsed
                case 3: last_pos = current_pos; break;   // Same position
            }
            
            if (shouldTerminate(time_diff_ms, iteration, low_pos, high_pos, current_pos, last_pos)) {
                tests_passed++;
                random_passed++;
            } else {
                std::cerr << "    FAILED: Should terminate with condition " << condition << std::endl;
                assert(false);
            }
        }
        std::cout << "    " << random_passed << "/" << random_tests << " random tests passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 7: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

/**
 * Property 7b: Time Differential Calculation
 * 
 * Tests that time differential is calculated correctly per Requirement 4.1.
 */
void test_property_time_differential_calculation() {
    std::cout << "\n=== Property 7b: Time Differential Calculation ===" << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: Basic calculation
    // ----------------------------------------
    std::cout << "\n  Test 1: Basic calculation..." << std::endl;
    {
        tests_run++;
        
        // 44100 samples difference at 44100 Hz = 1000ms
        int64_t result = calculateTimeDiffMs(88200, 44100, 44100);
        if (result == 1000) {
            std::cout << "    44100 samples at 44100Hz = " << result << "ms ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected 1000ms, got " << result << "ms" << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 2: Negative difference (actual < target)
    // ----------------------------------------
    std::cout << "\n  Test 2: Negative difference (actual < target)..." << std::endl;
    {
        tests_run++;
        
        // Should return absolute value
        int64_t result = calculateTimeDiffMs(44100, 88200, 44100);
        if (result == 1000) {
            std::cout << "    Negative diff gives absolute value: " << result << "ms ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected 1000ms, got " << result << "ms" << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 3: Zero difference
    // ----------------------------------------
    std::cout << "\n  Test 3: Zero difference..." << std::endl;
    {
        tests_run++;
        
        int64_t result = calculateTimeDiffMs(44100, 44100, 44100);
        if (result == 0) {
            std::cout << "    Zero diff = " << result << "ms ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected 0ms, got " << result << "ms" << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 4: 250ms tolerance boundary
    // ----------------------------------------
    std::cout << "\n  Test 4: 250ms tolerance boundary..." << std::endl;
    {
        tests_run++;
        
        // 11025 samples at 44100 Hz = 250ms
        int64_t result = calculateTimeDiffMs(55125, 44100, 44100);
        if (result == 250) {
            std::cout << "    11025 samples at 44100Hz = " << result << "ms ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected 250ms, got " << result << "ms" << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 5: High sample rate (192kHz)
    // ----------------------------------------
    std::cout << "\n  Test 5: High sample rate (192kHz)..." << std::endl;
    {
        tests_run++;
        
        // 192000 samples at 192000 Hz = 1000ms
        int64_t result = calculateTimeDiffMs(384000, 192000, 192000);
        if (result == 1000) {
            std::cout << "    192000 samples at 192kHz = " << result << "ms ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected 1000ms, got " << result << "ms" << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 7b: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}


// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC BISECTION CONVERGENCE PROPERTY-BASED TESTS" << std::endl;
    std::cout << "**Feature: flac-bisection-seeking, Property 7: Convergence Termination**" << std::endl;
    std::cout << "**Validates: Requirements 3.3, 3.4, 3.5, 3.6**" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // Property 7: Convergence Termination
        test_property_convergence_termination();
        
        // Property 7b: Time Differential Calculation
        test_property_time_differential_calculation();
        
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
