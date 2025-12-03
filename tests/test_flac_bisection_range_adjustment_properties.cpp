/*
 * test_flac_bisection_range_adjustment_properties.cpp - Property-based tests for FLAC bisection range adjustment
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * **Feature: flac-bisection-seeking, Property 6: Bisection Range Adjustment**
 * **Validates: Requirements 3.1, 3.2**
 *
 * For any bisection iteration where actual sample < target sample, the new search
 * range SHALL have low bound >= current position. For any iteration where actual
 * sample > target sample, the new search range SHALL have high bound <= current position.
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
// BISECTION RANGE ADJUSTMENT LOGIC
// ========================================

/**
 * @brief Represents the state of a bisection search
 */
struct BisectionState {
    uint64_t target_sample;      // Target sample position
    uint64_t low_pos;            // Lower bound (byte position)
    uint64_t high_pos;           // Upper bound (byte position)
    uint64_t best_pos;           // Best position found so far
    uint64_t best_sample;        // Sample at best position
    int64_t best_diff_ms;        // Best time differential (ms)
    int iteration;               // Current iteration count
    
    static constexpr int MAX_ITERATIONS = 10;
    static constexpr int64_t TOLERANCE_MS = 250;
    static constexpr uint64_t MIN_SEARCH_RANGE = 64;
};

/**
 * @brief Adjust bisection search range based on actual vs target sample
 * 
 * Implements Requirements 3.1, 3.2:
 * - Requirement 3.1: When actual < target, adjust search to upper half (low_pos = frame_pos + block_size)
 * - Requirement 3.2: When actual > target, adjust search to lower half (high_pos = frame_pos)
 * 
 * @param state Current bisection state (modified in place)
 * @param frame_pos Position of the found frame
 * @param frame_sample Sample offset of the found frame
 * @param block_size Block size of the found frame
 */
void adjustBisectionRange(BisectionState& state, uint64_t frame_pos, 
                          uint64_t frame_sample, uint32_t block_size) {
    if (frame_sample < state.target_sample) {
        // Requirement 3.1: Actual < target, search upper half
        state.low_pos = frame_pos + block_size;
    } else {
        // Requirement 3.2: Actual > target, search lower half
        state.high_pos = frame_pos;
    }
}

/**
 * @brief Check if bisection search range has collapsed
 * 
 * Implements Requirement 3.5: Accept position when search range < minimum frame size
 * 
 * @param state Current bisection state
 * @return true if range has collapsed, false otherwise
 */
bool isRangeCollapsed(const BisectionState& state) {
    return state.high_pos <= state.low_pos + BisectionState::MIN_SEARCH_RANGE;
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
 * Property 6: Bisection Range Adjustment
 * 
 * For any bisection iteration where actual sample < target sample, the new search
 * range SHALL have low bound >= current position. For any iteration where actual
 * sample > target sample, the new search range SHALL have high bound <= current position.
 */
void test_property_bisection_range_adjustment() {
    std::cout << "\n=== Property 6: Bisection Range Adjustment ===" << std::endl;
    std::cout << "Testing bisection range adjustment logic..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: Requirement 3.1 - Actual < Target moves low bound up
    // ----------------------------------------
    std::cout << "\n  Test 1: Requirement 3.1 - Actual < Target moves low bound up..." << std::endl;
    {
        tests_run++;
        
        BisectionState state;
        state.target_sample = 1000000;  // Target at 1M samples
        state.low_pos = 0;
        state.high_pos = 10000000;
        
        uint64_t frame_pos = 2000000;
        uint64_t frame_sample = 500000;  // Actual < Target
        uint32_t block_size = 4096;
        
        uint64_t old_low = state.low_pos;
        adjustBisectionRange(state, frame_pos, frame_sample, block_size);
        
        // Requirement 3.1: low_pos should be >= frame_pos + block_size
        if (state.low_pos >= frame_pos + block_size && state.low_pos > old_low) {
            std::cout << "    Actual < Target: low_pos moved from " << old_low 
                      << " to " << state.low_pos << " ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: low_pos should be >= " << (frame_pos + block_size) 
                      << ", got " << state.low_pos << std::endl;
            assert(false && "Requirement 3.1: low_pos should move up when actual < target");
        }
    }
    
    // ----------------------------------------
    // Test 2: Requirement 3.2 - Actual > Target moves high bound down
    // ----------------------------------------
    std::cout << "\n  Test 2: Requirement 3.2 - Actual > Target moves high bound down..." << std::endl;
    {
        tests_run++;
        
        BisectionState state;
        state.target_sample = 500000;  // Target at 500K samples
        state.low_pos = 0;
        state.high_pos = 10000000;
        
        uint64_t frame_pos = 6000000;
        uint64_t frame_sample = 1000000;  // Actual > Target
        uint32_t block_size = 4096;
        
        uint64_t old_high = state.high_pos;
        adjustBisectionRange(state, frame_pos, frame_sample, block_size);
        
        // Requirement 3.2: high_pos should be <= frame_pos
        if (state.high_pos <= frame_pos && state.high_pos < old_high) {
            std::cout << "    Actual > Target: high_pos moved from " << old_high 
                      << " to " << state.high_pos << " ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: high_pos should be <= " << frame_pos 
                      << ", got " << state.high_pos << std::endl;
            assert(false && "Requirement 3.2: high_pos should move down when actual > target");
        }
    }
    
    // ----------------------------------------
    // Test 3: Actual == Target (edge case)
    // ----------------------------------------
    std::cout << "\n  Test 3: Actual == Target (edge case)..." << std::endl;
    {
        tests_run++;
        
        BisectionState state;
        state.target_sample = 500000;
        state.low_pos = 0;
        state.high_pos = 10000000;
        
        uint64_t frame_pos = 5000000;
        uint64_t frame_sample = 500000;  // Actual == Target
        uint32_t block_size = 4096;
        
        uint64_t old_high = state.high_pos;
        adjustBisectionRange(state, frame_pos, frame_sample, block_size);
        
        // When actual == target, we treat it as actual >= target (Requirement 3.2)
        // high_pos should be <= frame_pos
        if (state.high_pos <= frame_pos) {
            std::cout << "    Actual == Target: high_pos moved to " << state.high_pos << " ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: high_pos should be <= " << frame_pos 
                      << ", got " << state.high_pos << std::endl;
            assert(false && "Actual == Target should be treated as actual >= target");
        }
    }
    
    // ----------------------------------------
    // Test 4: Random tests - Requirement 3.1 (actual < target)
    // ----------------------------------------
    std::cout << "\n  Test 4: Random tests - Requirement 3.1 (actual < target)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        
        int random_tests = 0;
        int random_passed = 0;
        
        for (int i = 0; i < 100; ++i) {
            std::uniform_int_distribution<uint64_t> pos_dist(1000, 100000000);
            std::uniform_int_distribution<uint32_t> block_dist(16, 65535);
            
            BisectionState state;
            state.low_pos = pos_dist(gen) % 1000000;
            state.high_pos = state.low_pos + pos_dist(gen);
            state.target_sample = pos_dist(gen);
            
            uint64_t frame_pos = state.low_pos + (state.high_pos - state.low_pos) / 2;
            // Ensure actual < target
            uint64_t frame_sample = state.target_sample > 1000 ? state.target_sample - 1000 : 0;
            uint32_t block_size = block_dist(gen);
            
            tests_run++;
            random_tests++;
            
            uint64_t old_low = state.low_pos;
            adjustBisectionRange(state, frame_pos, frame_sample, block_size);
            
            // Requirement 3.1: low_pos should be >= frame_pos + block_size
            if (state.low_pos >= frame_pos + block_size) {
                tests_passed++;
                random_passed++;
            } else {
                std::cerr << "    FAILED: low_pos " << state.low_pos 
                          << " < frame_pos + block_size " << (frame_pos + block_size) << std::endl;
                assert(false && "Requirement 3.1 violated");
            }
        }
        std::cout << "    " << random_passed << "/" << random_tests << " random tests passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 5: Random tests - Requirement 3.2 (actual > target)
    // ----------------------------------------
    std::cout << "\n  Test 5: Random tests - Requirement 3.2 (actual > target)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        
        int random_tests = 0;
        int random_passed = 0;
        
        for (int i = 0; i < 100; ++i) {
            std::uniform_int_distribution<uint64_t> pos_dist(1000, 100000000);
            std::uniform_int_distribution<uint32_t> block_dist(16, 65535);
            
            BisectionState state;
            state.low_pos = pos_dist(gen) % 1000000;
            state.high_pos = state.low_pos + pos_dist(gen);
            state.target_sample = pos_dist(gen);
            
            uint64_t frame_pos = state.low_pos + (state.high_pos - state.low_pos) / 2;
            // Ensure actual > target
            uint64_t frame_sample = state.target_sample + 1000;
            uint32_t block_size = block_dist(gen);
            
            tests_run++;
            random_tests++;
            
            uint64_t old_high = state.high_pos;
            adjustBisectionRange(state, frame_pos, frame_sample, block_size);
            
            // Requirement 3.2: high_pos should be <= frame_pos
            if (state.high_pos <= frame_pos) {
                tests_passed++;
                random_passed++;
            } else {
                std::cerr << "    FAILED: high_pos " << state.high_pos 
                          << " > frame_pos " << frame_pos << std::endl;
                assert(false && "Requirement 3.2 violated");
            }
        }
        std::cout << "    " << random_passed << "/" << random_tests << " random tests passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 6: Range decreases or stays same (never increases beyond original)
    // Note: In real bisection, the range always shrinks because:
    // - When actual < target: low_pos moves up past frame_pos
    // - When actual > target: high_pos moves down to frame_pos
    // The key property is that the search space is always reduced.
    // ----------------------------------------
    std::cout << "\n  Test 6: Range decreases or stays same..." << std::endl;
    {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        
        int random_tests = 0;
        int random_passed = 0;
        
        for (int i = 0; i < 50; ++i) {
            BisectionState state;
            state.low_pos = 0;
            state.high_pos = 100000000;
            state.target_sample = 50000000;
            
            tests_run++;
            random_tests++;
            
            bool property_holds = true;
            uint64_t initial_range = state.high_pos - state.low_pos;
            
            // Simulate multiple iterations
            for (int iter = 0; iter < 10 && !isRangeCollapsed(state); ++iter) {
                // Frame position must be within current search range
                if (state.high_pos <= state.low_pos) break;
                
                std::uniform_int_distribution<uint64_t> pos_dist(state.low_pos, state.high_pos - 1);
                uint64_t frame_pos = pos_dist(gen);
                
                // Randomly choose actual < or > target
                std::uniform_int_distribution<int> dir_dist(0, 1);
                uint64_t frame_sample = dir_dist(gen) == 0 ? 
                    state.target_sample - 1000 : state.target_sample + 1000;
                uint32_t block_size = 4096;
                
                uint64_t old_low = state.low_pos;
                uint64_t old_high = state.high_pos;
                
                adjustBisectionRange(state, frame_pos, frame_sample, block_size);
                
                // Verify the key properties:
                // 1. low_pos never decreases
                // 2. high_pos never increases
                if (state.low_pos < old_low || state.high_pos > old_high) {
                    property_holds = false;
                    std::cerr << "    FAILED: Bounds moved in wrong direction" << std::endl;
                    std::cerr << "      old_low=" << old_low << " new_low=" << state.low_pos << std::endl;
                    std::cerr << "      old_high=" << old_high << " new_high=" << state.high_pos << std::endl;
                    break;
                }
            }
            
            // Final range should be <= initial range
            uint64_t final_range = (state.high_pos > state.low_pos) ? 
                                   (state.high_pos - state.low_pos) : 0;
            if (final_range > initial_range) {
                property_holds = false;
            }
            
            if (property_holds) {
                tests_passed++;
                random_passed++;
            } else {
                assert(false && "Range bounds should never move in wrong direction");
            }
        }
        std::cout << "    " << random_passed << "/" << random_tests << " random tests passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 6: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

/**
 * Property 6b: Range Collapse Detection
 * 
 * Tests that range collapse is correctly detected per Requirement 3.5.
 */
void test_property_range_collapse_detection() {
    std::cout << "\n=== Property 6b: Range Collapse Detection ===" << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: Range exactly at minimum
    // ----------------------------------------
    std::cout << "\n  Test 1: Range exactly at minimum..." << std::endl;
    {
        tests_run++;
        
        BisectionState state;
        state.low_pos = 1000;
        state.high_pos = 1000 + BisectionState::MIN_SEARCH_RANGE;
        
        if (isRangeCollapsed(state)) {
            std::cout << "    Range of " << BisectionState::MIN_SEARCH_RANGE 
                      << " bytes detected as collapsed ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Range should be detected as collapsed" << std::endl;
            assert(false && "Range at minimum should be collapsed");
        }
    }
    
    // ----------------------------------------
    // Test 2: Range below minimum
    // ----------------------------------------
    std::cout << "\n  Test 2: Range below minimum..." << std::endl;
    {
        tests_run++;
        
        BisectionState state;
        state.low_pos = 1000;
        state.high_pos = 1000 + BisectionState::MIN_SEARCH_RANGE - 1;
        
        if (isRangeCollapsed(state)) {
            std::cout << "    Range of " << (BisectionState::MIN_SEARCH_RANGE - 1) 
                      << " bytes detected as collapsed ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Range should be detected as collapsed" << std::endl;
            assert(false && "Range below minimum should be collapsed");
        }
    }
    
    // ----------------------------------------
    // Test 3: Range above minimum
    // ----------------------------------------
    std::cout << "\n  Test 3: Range above minimum..." << std::endl;
    {
        tests_run++;
        
        BisectionState state;
        state.low_pos = 1000;
        state.high_pos = 1000 + BisectionState::MIN_SEARCH_RANGE + 1;
        
        if (!isRangeCollapsed(state)) {
            std::cout << "    Range of " << (BisectionState::MIN_SEARCH_RANGE + 1) 
                      << " bytes NOT detected as collapsed ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Range should NOT be detected as collapsed" << std::endl;
            assert(false && "Range above minimum should not be collapsed");
        }
    }
    
    // ----------------------------------------
    // Test 4: Large range
    // ----------------------------------------
    std::cout << "\n  Test 4: Large range..." << std::endl;
    {
        tests_run++;
        
        BisectionState state;
        state.low_pos = 0;
        state.high_pos = 100000000;
        
        if (!isRangeCollapsed(state)) {
            std::cout << "    Large range NOT detected as collapsed ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Large range should NOT be collapsed" << std::endl;
            assert(false && "Large range should not be collapsed");
        }
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 6b: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC BISECTION RANGE ADJUSTMENT PROPERTY-BASED TESTS" << std::endl;
    std::cout << "**Feature: flac-bisection-seeking, Property 6: Bisection Range Adjustment**" << std::endl;
    std::cout << "**Validates: Requirements 3.1, 3.2**" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // Property 6: Bisection Range Adjustment
        test_property_bisection_range_adjustment();
        
        // Property 6b: Range Collapse Detection
        test_property_range_collapse_detection();
        
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
