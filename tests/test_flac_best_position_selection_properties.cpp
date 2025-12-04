/*
 * test_flac_best_position_selection_properties.cpp - Property-based tests for FLAC best position selection
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * **Feature: flac-bisection-seeking, Property 9: Best Position Selection**
 * **Validates: Requirements 4.3, 4.4**
 *
 * For any bisection search that exceeds tolerance, the final position SHALL be
 * the one with minimum time differential found during all iterations. When two
 * positions have equal differential, prefer the one before target.
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
#include <algorithm>

// ========================================
// BEST POSITION SELECTION LOGIC
// ========================================

/**
 * @brief Represents a candidate position found during bisection search
 */
struct CandidatePosition {
    uint64_t file_pos;       ///< File position of the frame
    uint64_t sample_offset;  ///< Sample offset of the frame
    int64_t time_diff_ms;    ///< Time differential from target in ms
    bool is_before_target;   ///< True if sample_offset <= target_sample
    
    CandidatePosition(uint64_t pos, uint64_t sample, int64_t diff, bool before)
        : file_pos(pos), sample_offset(sample), time_diff_ms(diff), is_before_target(before) {}
};

/**
 * @brief Calculate time differential in milliseconds
 */
int64_t calculateTimeDiffMs(uint64_t actual_sample, uint64_t target_sample, uint32_t sample_rate) {
    if (sample_rate == 0) return INT64_MAX;
    int64_t sample_diff = static_cast<int64_t>(actual_sample) - static_cast<int64_t>(target_sample);
    return (std::abs(sample_diff) * 1000) / static_cast<int64_t>(sample_rate);
}

/**
 * @brief Determine if candidate A is better than candidate B per Requirements 4.3, 4.4
 * 
 * Requirement 4.3: Track position with minimum differential
 * Requirement 4.4: Prefer positions before target when equal
 * 
 * @param a First candidate
 * @param b Second candidate
 * @return true if A is better than B
 */
bool isBetterPosition(const CandidatePosition& a, const CandidatePosition& b) {
    // Requirement 4.3: Prefer smaller time differential
    if (a.time_diff_ms < b.time_diff_ms) {
        return true;
    }
    
    // Requirement 4.4: When equal, prefer position before target
    if (a.time_diff_ms == b.time_diff_ms && a.is_before_target && !b.is_before_target) {
        return true;
    }
    
    return false;
}

/**
 * @brief Select the best position from a list of candidates per Requirements 4.3, 4.4
 * 
 * @param candidates List of candidate positions found during bisection
 * @return Index of the best candidate, or -1 if list is empty
 */
int selectBestPosition(const std::vector<CandidatePosition>& candidates) {
    if (candidates.empty()) return -1;
    
    int best_idx = 0;
    for (size_t i = 1; i < candidates.size(); ++i) {
        if (isBetterPosition(candidates[i], candidates[best_idx])) {
            best_idx = static_cast<int>(i);
        }
    }
    return best_idx;
}

/**
 * @brief Simulate best position tracking during bisection search
 * 
 * This simulates the logic in seekWithByteEstimation_unlocked for tracking
 * the best position found during iterative refinement.
 */
struct BestPositionTracker {
    uint64_t best_pos = 0;
    uint64_t best_sample = 0;
    int64_t best_diff_ms = INT64_MAX;
    bool best_is_before_target = true;
    
    /**
     * @brief Update best position if the new candidate is better
     * 
     * Implements Requirements 4.3, 4.4:
     * - Track position with minimum differential
     * - Prefer positions before target when equal
     */
    void update(uint64_t pos, uint64_t sample, int64_t diff_ms, bool is_before_target) {
        bool update_best = false;
        
        // Requirement 4.3: Prefer smaller time differential
        if (diff_ms < best_diff_ms) {
            update_best = true;
        } 
        // Requirement 4.4: When equal, prefer position before target
        else if (diff_ms == best_diff_ms && is_before_target && !best_is_before_target) {
            update_best = true;
        }
        
        if (update_best) {
            best_pos = pos;
            best_sample = sample;
            best_diff_ms = diff_ms;
            best_is_before_target = is_before_target;
        }
    }
};

// ========================================
// PROPERTY-BASED TESTS
// ========================================

/**
 * Property 9: Best Position Selection
 * 
 * Tests that the best position is selected correctly per Requirements 4.3, 4.4.
 */
void test_property_best_position_selection() {
    std::cout << "\n=== Property 9: Best Position Selection ===" << std::endl;
    std::cout << "Testing best position selection per Requirements 4.3, 4.4..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: Requirement 4.3 - Minimum differential wins
    // ----------------------------------------
    std::cout << "\n  Test 1: Requirement 4.3 - Minimum differential wins..." << std::endl;
    {
        tests_run++;
        
        std::vector<CandidatePosition> candidates = {
            CandidatePosition(1000, 44100, 500, true),   // 500ms before target
            CandidatePosition(2000, 88200, 200, false),  // 200ms after target (best)
            CandidatePosition(3000, 66150, 300, true),   // 300ms before target
        };
        
        int best_idx = selectBestPosition(candidates);
        if (best_idx == 1 && candidates[best_idx].time_diff_ms == 200) {
            std::cout << "    Minimum differential (200ms) selected ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected index 1 (200ms), got " << best_idx << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 2: Requirement 4.4 - Prefer before target when equal
    // ----------------------------------------
    std::cout << "\n  Test 2: Requirement 4.4 - Prefer before target when equal..." << std::endl;
    {
        tests_run++;
        
        std::vector<CandidatePosition> candidates = {
            CandidatePosition(1000, 44100, 250, false),  // 250ms after target
            CandidatePosition(2000, 33075, 250, true),   // 250ms before target (preferred)
            CandidatePosition(3000, 55125, 250, false),  // 250ms after target
        };
        
        int best_idx = selectBestPosition(candidates);
        if (best_idx == 1 && candidates[best_idx].is_before_target) {
            std::cout << "    Position before target preferred when equal ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected index 1 (before target), got " << best_idx << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 3: Smaller differential beats before-target preference
    // ----------------------------------------
    std::cout << "\n  Test 3: Smaller differential beats before-target preference..." << std::endl;
    {
        tests_run++;
        
        std::vector<CandidatePosition> candidates = {
            CandidatePosition(1000, 44100, 300, true),   // 300ms before target
            CandidatePosition(2000, 88200, 100, false),  // 100ms after target (best - smaller diff)
        };
        
        int best_idx = selectBestPosition(candidates);
        if (best_idx == 1 && candidates[best_idx].time_diff_ms == 100) {
            std::cout << "    Smaller differential (100ms) wins over before-target ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected index 1 (100ms), got " << best_idx << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 4: Single candidate is always best
    // ----------------------------------------
    std::cout << "\n  Test 4: Single candidate is always best..." << std::endl;
    {
        tests_run++;
        
        std::vector<CandidatePosition> candidates = {
            CandidatePosition(5000, 100000, 1000, false),  // Only candidate
        };
        
        int best_idx = selectBestPosition(candidates);
        if (best_idx == 0) {
            std::cout << "    Single candidate selected ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected index 0, got " << best_idx << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 5: Empty list returns -1
    // ----------------------------------------
    std::cout << "\n  Test 5: Empty list returns -1..." << std::endl;
    {
        tests_run++;
        
        std::vector<CandidatePosition> candidates;
        
        int best_idx = selectBestPosition(candidates);
        if (best_idx == -1) {
            std::cout << "    Empty list returns -1 ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected -1, got " << best_idx << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 6: BestPositionTracker - incremental updates
    // ----------------------------------------
    std::cout << "\n  Test 6: BestPositionTracker - incremental updates..." << std::endl;
    {
        tests_run++;
        
        BestPositionTracker tracker;
        
        // First update - any position is best
        tracker.update(1000, 44100, 500, true);
        assert(tracker.best_pos == 1000);
        assert(tracker.best_diff_ms == 500);
        
        // Better position (smaller diff)
        tracker.update(2000, 88200, 200, false);
        assert(tracker.best_pos == 2000);
        assert(tracker.best_diff_ms == 200);
        
        // Worse position (larger diff) - should not update
        tracker.update(3000, 66150, 300, true);
        assert(tracker.best_pos == 2000);
        assert(tracker.best_diff_ms == 200);
        
        // Equal diff but before target - should update per Req 4.4
        tracker.update(4000, 35280, 200, true);
        assert(tracker.best_pos == 4000);
        assert(tracker.best_is_before_target == true);
        
        std::cout << "    Incremental updates work correctly ✓" << std::endl;
        tests_passed++;
    }
    
    // ----------------------------------------
    // Test 7: Property test - minimum is always selected
    // ----------------------------------------
    std::cout << "\n  Test 7: Property test - minimum is always selected..." << std::endl;
    {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        
        int random_tests = 0;
        int random_passed = 0;
        
        for (int i = 0; i < 100; ++i) {
            tests_run++;
            random_tests++;
            
            // Generate random candidates
            std::uniform_int_distribution<size_t> count_dist(1, 20);
            size_t count = count_dist(gen);
            
            std::vector<CandidatePosition> candidates;
            std::uniform_int_distribution<uint64_t> pos_dist(0, 1000000000);
            std::uniform_int_distribution<int64_t> diff_dist(0, 10000);
            std::uniform_int_distribution<int> bool_dist(0, 1);
            
            for (size_t j = 0; j < count; ++j) {
                candidates.emplace_back(
                    pos_dist(gen),
                    pos_dist(gen),
                    diff_dist(gen),
                    bool_dist(gen) == 1
                );
            }
            
            int best_idx = selectBestPosition(candidates);
            
            // Verify the selected position is actually the best
            bool is_valid = true;
            for (size_t j = 0; j < candidates.size(); ++j) {
                if (static_cast<int>(j) != best_idx) {
                    // The selected position should not be worse than any other
                    if (isBetterPosition(candidates[j], candidates[best_idx])) {
                        is_valid = false;
                        break;
                    }
                }
            }
            
            if (is_valid) {
                tests_passed++;
                random_passed++;
            } else {
                std::cerr << "    FAILED: Selected position is not the best" << std::endl;
                assert(false);
            }
        }
        std::cout << "    " << random_passed << "/" << random_tests << " random tests passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 8: Property test - before-target preference with equal diffs
    // ----------------------------------------
    std::cout << "\n  Test 8: Property test - before-target preference with equal diffs..." << std::endl;
    {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        
        int random_tests = 0;
        int random_passed = 0;
        
        for (int i = 0; i < 50; ++i) {
            tests_run++;
            random_tests++;
            
            // Generate candidates with same time differential
            std::uniform_int_distribution<int64_t> diff_dist(0, 1000);
            int64_t common_diff = diff_dist(gen);
            
            std::vector<CandidatePosition> candidates;
            std::uniform_int_distribution<uint64_t> pos_dist(0, 1000000000);
            
            // Add some "after target" candidates
            for (int j = 0; j < 3; ++j) {
                candidates.emplace_back(pos_dist(gen), pos_dist(gen), common_diff, false);
            }
            
            // Add one "before target" candidate
            candidates.emplace_back(pos_dist(gen), pos_dist(gen), common_diff, true);
            
            // Add more "after target" candidates
            for (int j = 0; j < 2; ++j) {
                candidates.emplace_back(pos_dist(gen), pos_dist(gen), common_diff, false);
            }
            
            int best_idx = selectBestPosition(candidates);
            
            // The selected position should be before target (Requirement 4.4)
            if (candidates[best_idx].is_before_target) {
                tests_passed++;
                random_passed++;
            } else {
                std::cerr << "    FAILED: Should prefer before-target when diffs are equal" << std::endl;
                assert(false);
            }
        }
        std::cout << "    " << random_passed << "/" << random_tests << " before-target preference tests passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 9: Transitivity - if A > B and B > C, then A > C
    // ----------------------------------------
    std::cout << "\n  Test 9: Transitivity property..." << std::endl;
    {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        
        int random_tests = 0;
        int random_passed = 0;
        
        for (int i = 0; i < 50; ++i) {
            tests_run++;
            random_tests++;
            
            std::uniform_int_distribution<uint64_t> pos_dist(0, 1000000000);
            std::uniform_int_distribution<int64_t> diff_dist(0, 1000);
            std::uniform_int_distribution<int> bool_dist(0, 1);
            
            CandidatePosition a(pos_dist(gen), pos_dist(gen), diff_dist(gen), bool_dist(gen) == 1);
            CandidatePosition b(pos_dist(gen), pos_dist(gen), diff_dist(gen), bool_dist(gen) == 1);
            CandidatePosition c(pos_dist(gen), pos_dist(gen), diff_dist(gen), bool_dist(gen) == 1);
            
            // Check transitivity: if A > B and B > C, then A > C
            if (isBetterPosition(a, b) && isBetterPosition(b, c)) {
                if (isBetterPosition(a, c)) {
                    tests_passed++;
                    random_passed++;
                } else {
                    std::cerr << "    FAILED: Transitivity violated" << std::endl;
                    assert(false);
                }
            } else {
                // Transitivity doesn't apply in this case
                tests_passed++;
                random_passed++;
            }
        }
        std::cout << "    " << random_passed << "/" << random_tests << " transitivity tests passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 9: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC BEST POSITION SELECTION PROPERTY-BASED TESTS" << std::endl;
    std::cout << "**Feature: flac-bisection-seeking, Property 9: Best Position Selection**" << std::endl;
    std::cout << "**Validates: Requirements 4.3, 4.4**" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // Property 9: Best Position Selection
        test_property_best_position_selection();
        
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

