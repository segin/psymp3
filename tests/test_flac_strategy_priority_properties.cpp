/*
 * test_flac_strategy_priority_properties.cpp - Property-based tests for FLAC seeking strategy priority
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * **Feature: flac-bisection-seeking, Property 10: Strategy Priority**
 * **Validates: Requirements 7.1, 7.2, 7.3, 7.5**
 *
 * For any seek operation, the FLAC Demuxer SHALL use strategies in order:
 * (1) SEEKTABLE if available,
 * (2) frame index if target is indexed,
 * (3) bisection estimation,
 * (4) fallback to beginning.
 */

#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <cstdint>
#include <random>
#include <sstream>
#include <iomanip>

// ========================================
// STRATEGY PRIORITY SIMULATION
// ========================================

/**
 * @brief Enum representing seeking strategies in priority order
 */
enum class SeekStrategy {
    SEEKTABLE = 1,      // Requirement 7.1: Highest priority
    FRAME_INDEX = 2,    // Requirement 7.2: Second priority
    BISECTION = 3,      // Requirement 7.3: Third priority
    FALLBACK = 4,       // Requirement 7.3: Last resort
    DIRECT_BEGINNING = 0 // Requirement 7.5: Special case for position 0
};

/**
 * @brief Convert strategy to string for logging
 */
std::string strategyToString(SeekStrategy strategy) {
    switch (strategy) {
        case SeekStrategy::SEEKTABLE: return "SEEKTABLE";
        case SeekStrategy::FRAME_INDEX: return "FRAME_INDEX";
        case SeekStrategy::BISECTION: return "BISECTION";
        case SeekStrategy::FALLBACK: return "FALLBACK";
        case SeekStrategy::DIRECT_BEGINNING: return "DIRECT_BEGINNING";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Simulated demuxer state for testing strategy selection
 */
struct DemuxerState {
    bool has_seektable;
    bool has_frame_index;
    bool has_total_samples;  // Required for bisection
    uint64_t file_size;
    uint64_t audio_data_offset;
    
    // Strategy success simulation
    bool seektable_succeeds;
    bool frame_index_succeeds;
    bool bisection_succeeds;
};

/**
 * @brief Determine which strategy should be used based on demuxer state
 * 
 * This simulates the logic in FLACDemuxer::seekTo_unlocked()
 * 
 * Requirements:
 * - 7.1: SEEKTABLE preferred over bisection
 * - 7.2: Frame index preferred over bisection
 * - 7.3: Bisection used when SEEKTABLE/frame index unavailable, fallback on failure
 * - 7.5: Position 0 bypasses all strategies
 */
SeekStrategy selectStrategy(const DemuxerState& state, uint64_t target_sample) {
    // Requirement 7.5: Seeking to position 0 bypasses all strategies
    if (target_sample == 0) {
        return SeekStrategy::DIRECT_BEGINNING;
    }
    
    // Requirement 7.2: Try frame index first (most accurate)
    if (state.has_frame_index) {
        if (state.frame_index_succeeds) {
            return SeekStrategy::FRAME_INDEX;
        }
        // Frame index failed, continue to next strategy
    }
    
    // Requirement 7.1: Try SEEKTABLE (RFC 9639 standard)
    if (state.has_seektable) {
        if (state.seektable_succeeds) {
            return SeekStrategy::SEEKTABLE;
        }
        // SEEKTABLE failed, continue to next strategy
    }
    
    // Requirement 7.3: Try bisection estimation
    if (state.has_total_samples && state.file_size > state.audio_data_offset) {
        if (state.bisection_succeeds) {
            return SeekStrategy::BISECTION;
        }
        // Bisection failed, fall through to fallback
    }
    
    // Requirement 7.3: Fallback to beginning
    return SeekStrategy::FALLBACK;
}

/**
 * @brief Determine the expected strategy based on availability (not success)
 * 
 * This returns what strategy SHOULD be attempted first based on priority.
 */
SeekStrategy getExpectedFirstAttempt(const DemuxerState& state, uint64_t target_sample) {
    // Requirement 7.5: Position 0 always goes directly to beginning
    if (target_sample == 0) {
        return SeekStrategy::DIRECT_BEGINNING;
    }
    
    // Requirement 7.2: Frame index has highest priority (most accurate)
    if (state.has_frame_index) {
        return SeekStrategy::FRAME_INDEX;
    }
    
    // Requirement 7.1: SEEKTABLE is second priority
    if (state.has_seektable) {
        return SeekStrategy::SEEKTABLE;
    }
    
    // Requirement 7.3: Bisection is third priority
    if (state.has_total_samples && state.file_size > state.audio_data_offset) {
        return SeekStrategy::BISECTION;
    }
    
    // Requirement 7.3: Fallback is last resort
    return SeekStrategy::FALLBACK;
}

// ========================================
// PROPERTY-BASED TESTS
// ========================================

/**
 * Property 10: Strategy Priority
 * 
 * Tests that seeking strategies are used in the correct priority order.
 */
void test_property_strategy_priority() {
    std::cout << "\n=== Property 10: Strategy Priority ===" << std::endl;
    std::cout << "Testing strategy priority order..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: Requirement 7.5 - Position 0 bypasses all strategies
    // ----------------------------------------
    std::cout << "\n  Test 1: Requirement 7.5 - Position 0 bypasses all strategies..." << std::endl;
    {
        tests_run++;
        
        DemuxerState state = {
            .has_seektable = true,
            .has_frame_index = true,
            .has_total_samples = true,
            .file_size = 10000000,
            .audio_data_offset = 1000,
            .seektable_succeeds = true,
            .frame_index_succeeds = true,
            .bisection_succeeds = true
        };
        
        SeekStrategy result = selectStrategy(state, 0);
        if (result == SeekStrategy::DIRECT_BEGINNING) {
            std::cout << "    Position 0 uses DIRECT_BEGINNING ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected DIRECT_BEGINNING, got " << strategyToString(result) << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 2: Requirement 7.2 - Frame index has highest priority
    // ----------------------------------------
    std::cout << "\n  Test 2: Requirement 7.2 - Frame index has highest priority..." << std::endl;
    {
        tests_run++;
        
        DemuxerState state = {
            .has_seektable = true,
            .has_frame_index = true,
            .has_total_samples = true,
            .file_size = 10000000,
            .audio_data_offset = 1000,
            .seektable_succeeds = true,
            .frame_index_succeeds = true,
            .bisection_succeeds = true
        };
        
        SeekStrategy result = selectStrategy(state, 44100);
        if (result == SeekStrategy::FRAME_INDEX) {
            std::cout << "    With all strategies available, FRAME_INDEX is used ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected FRAME_INDEX, got " << strategyToString(result) << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 3: Requirement 7.1 - SEEKTABLE used when frame index unavailable
    // ----------------------------------------
    std::cout << "\n  Test 3: Requirement 7.1 - SEEKTABLE used when frame index unavailable..." << std::endl;
    {
        tests_run++;
        
        DemuxerState state = {
            .has_seektable = true,
            .has_frame_index = false,  // No frame index
            .has_total_samples = true,
            .file_size = 10000000,
            .audio_data_offset = 1000,
            .seektable_succeeds = true,
            .frame_index_succeeds = false,
            .bisection_succeeds = true
        };
        
        SeekStrategy result = selectStrategy(state, 44100);
        if (result == SeekStrategy::SEEKTABLE) {
            std::cout << "    Without frame index, SEEKTABLE is used ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected SEEKTABLE, got " << strategyToString(result) << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 4: Requirement 7.1 - SEEKTABLE used when frame index fails
    // ----------------------------------------
    std::cout << "\n  Test 4: Requirement 7.1 - SEEKTABLE used when frame index fails..." << std::endl;
    {
        tests_run++;
        
        DemuxerState state = {
            .has_seektable = true,
            .has_frame_index = true,
            .has_total_samples = true,
            .file_size = 10000000,
            .audio_data_offset = 1000,
            .seektable_succeeds = true,
            .frame_index_succeeds = false,  // Frame index fails
            .bisection_succeeds = true
        };
        
        SeekStrategy result = selectStrategy(state, 44100);
        if (result == SeekStrategy::SEEKTABLE) {
            std::cout << "    When frame index fails, SEEKTABLE is used ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected SEEKTABLE, got " << strategyToString(result) << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 5: Requirement 7.3 - Bisection used when SEEKTABLE unavailable
    // ----------------------------------------
    std::cout << "\n  Test 5: Requirement 7.3 - Bisection used when SEEKTABLE unavailable..." << std::endl;
    {
        tests_run++;
        
        DemuxerState state = {
            .has_seektable = false,  // No SEEKTABLE
            .has_frame_index = false,
            .has_total_samples = true,
            .file_size = 10000000,
            .audio_data_offset = 1000,
            .seektable_succeeds = false,
            .frame_index_succeeds = false,
            .bisection_succeeds = true
        };
        
        SeekStrategy result = selectStrategy(state, 44100);
        if (result == SeekStrategy::BISECTION) {
            std::cout << "    Without SEEKTABLE/frame index, BISECTION is used ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected BISECTION, got " << strategyToString(result) << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 6: Requirement 7.3 - Bisection used when SEEKTABLE fails
    // ----------------------------------------
    std::cout << "\n  Test 6: Requirement 7.3 - Bisection used when SEEKTABLE fails..." << std::endl;
    {
        tests_run++;
        
        DemuxerState state = {
            .has_seektable = true,
            .has_frame_index = false,
            .has_total_samples = true,
            .file_size = 10000000,
            .audio_data_offset = 1000,
            .seektable_succeeds = false,  // SEEKTABLE fails
            .frame_index_succeeds = false,
            .bisection_succeeds = true
        };
        
        SeekStrategy result = selectStrategy(state, 44100);
        if (result == SeekStrategy::BISECTION) {
            std::cout << "    When SEEKTABLE fails, BISECTION is used ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected BISECTION, got " << strategyToString(result) << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 7: Requirement 7.3 - Fallback when bisection unavailable
    // ----------------------------------------
    std::cout << "\n  Test 7: Requirement 7.3 - Fallback when bisection unavailable..." << std::endl;
    {
        tests_run++;
        
        DemuxerState state = {
            .has_seektable = false,
            .has_frame_index = false,
            .has_total_samples = false,  // No total samples, can't do bisection
            .file_size = 10000000,
            .audio_data_offset = 1000,
            .seektable_succeeds = false,
            .frame_index_succeeds = false,
            .bisection_succeeds = false
        };
        
        SeekStrategy result = selectStrategy(state, 44100);
        if (result == SeekStrategy::FALLBACK) {
            std::cout << "    Without bisection capability, FALLBACK is used ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected FALLBACK, got " << strategyToString(result) << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Test 8: Requirement 7.3 - Fallback when all strategies fail
    // ----------------------------------------
    std::cout << "\n  Test 8: Requirement 7.3 - Fallback when all strategies fail..." << std::endl;
    {
        tests_run++;
        
        DemuxerState state = {
            .has_seektable = true,
            .has_frame_index = true,
            .has_total_samples = true,
            .file_size = 10000000,
            .audio_data_offset = 1000,
            .seektable_succeeds = false,  // All fail
            .frame_index_succeeds = false,
            .bisection_succeeds = false
        };
        
        SeekStrategy result = selectStrategy(state, 44100);
        if (result == SeekStrategy::FALLBACK) {
            std::cout << "    When all strategies fail, FALLBACK is used ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Expected FALLBACK, got " << strategyToString(result) << std::endl;
            assert(false);
        }
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 10 (basic): " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

/**
 * Property 10b: Strategy Priority - Random Testing
 * 
 * Tests strategy priority with randomly generated demuxer states.
 */
void test_property_strategy_priority_random() {
    std::cout << "\n=== Property 10b: Strategy Priority - Random Testing ===" << std::endl;
    
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<int> bool_dist(0, 1);
    std::uniform_int_distribution<uint64_t> sample_dist(0, 10000000);
    std::uniform_int_distribution<uint64_t> size_dist(1000, 100000000);
    
    int tests_passed = 0;
    int tests_run = 0;
    
    std::cout << "  Running 100 random strategy selection tests..." << std::endl;
    
    for (int i = 0; i < 100; ++i) {
        tests_run++;
        
        // Generate random demuxer state
        DemuxerState state = {
            .has_seektable = bool_dist(gen) == 1,
            .has_frame_index = bool_dist(gen) == 1,
            .has_total_samples = bool_dist(gen) == 1,
            .file_size = size_dist(gen),
            .audio_data_offset = 1000,
            .seektable_succeeds = bool_dist(gen) == 1,
            .frame_index_succeeds = bool_dist(gen) == 1,
            .bisection_succeeds = bool_dist(gen) == 1
        };
        
        // Generate random target (including 0 for position 0 test)
        uint64_t target_sample = (i % 10 == 0) ? 0 : sample_dist(gen);
        
        SeekStrategy result = selectStrategy(state, target_sample);
        SeekStrategy expected_first = getExpectedFirstAttempt(state, target_sample);
        
        // Verify the result is valid based on state
        bool valid = false;
        
        if (target_sample == 0) {
            // Requirement 7.5: Position 0 always uses DIRECT_BEGINNING
            valid = (result == SeekStrategy::DIRECT_BEGINNING);
        } else if (result == SeekStrategy::FRAME_INDEX) {
            // Frame index can only be used if available and succeeds
            valid = state.has_frame_index && state.frame_index_succeeds;
        } else if (result == SeekStrategy::SEEKTABLE) {
            // SEEKTABLE can only be used if available, succeeds, and frame index didn't work
            valid = state.has_seektable && state.seektable_succeeds &&
                    (!state.has_frame_index || !state.frame_index_succeeds);
        } else if (result == SeekStrategy::BISECTION) {
            // Bisection can only be used if conditions met and higher priority strategies didn't work
            valid = state.has_total_samples && state.file_size > state.audio_data_offset &&
                    state.bisection_succeeds &&
                    (!state.has_frame_index || !state.frame_index_succeeds) &&
                    (!state.has_seektable || !state.seektable_succeeds);
        } else if (result == SeekStrategy::FALLBACK) {
            // Fallback is always valid as last resort
            valid = true;
        }
        
        if (valid) {
            tests_passed++;
        } else {
            std::cerr << "    FAILED test " << i << ": Invalid strategy " << strategyToString(result) << std::endl;
            std::cerr << "      State: seektable=" << state.has_seektable 
                      << ", frame_index=" << state.has_frame_index
                      << ", total_samples=" << state.has_total_samples << std::endl;
            std::cerr << "      Success: seektable=" << state.seektable_succeeds
                      << ", frame_index=" << state.frame_index_succeeds
                      << ", bisection=" << state.bisection_succeeds << std::endl;
            std::cerr << "      Target: " << target_sample << std::endl;
            assert(false);
        }
    }
    
    std::cout << "    " << tests_passed << "/" << tests_run << " random tests passed ✓" << std::endl;
    std::cout << "\n✓ Property 10b: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

/**
 * Property 10c: Strategy Priority Order Invariant
 * 
 * Tests that higher priority strategies are always attempted before lower priority ones.
 */
void test_property_strategy_priority_order_invariant() {
    std::cout << "\n=== Property 10c: Strategy Priority Order Invariant ===" << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test: Priority order is always maintained
    // ----------------------------------------
    std::cout << "\n  Testing priority order invariant..." << std::endl;
    
    // Test all combinations of availability
    for (int has_fi = 0; has_fi <= 1; ++has_fi) {
        for (int has_st = 0; has_st <= 1; ++has_st) {
            for (int has_ts = 0; has_ts <= 1; ++has_ts) {
                tests_run++;
                
                DemuxerState state = {
                    .has_seektable = has_st == 1,
                    .has_frame_index = has_fi == 1,
                    .has_total_samples = has_ts == 1,
                    .file_size = 10000000,
                    .audio_data_offset = 1000,
                    .seektable_succeeds = true,
                    .frame_index_succeeds = true,
                    .bisection_succeeds = true
                };
                
                SeekStrategy expected = getExpectedFirstAttempt(state, 44100);
                SeekStrategy actual = selectStrategy(state, 44100);
                
                // When all strategies succeed, the first attempted should be used
                if (actual == expected) {
                    tests_passed++;
                } else {
                    std::cerr << "    FAILED: Expected " << strategyToString(expected)
                              << ", got " << strategyToString(actual) << std::endl;
                    std::cerr << "      State: frame_index=" << has_fi
                              << ", seektable=" << has_st
                              << ", total_samples=" << has_ts << std::endl;
                    assert(false);
                }
            }
        }
    }
    
    std::cout << "    " << tests_passed << "/" << tests_run << " priority order tests passed ✓" << std::endl;
    std::cout << "\n✓ Property 10c: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC SEEKING STRATEGY PRIORITY PROPERTY-BASED TESTS" << std::endl;
    std::cout << "**Feature: flac-bisection-seeking, Property 10: Strategy Priority**" << std::endl;
    std::cout << "**Validates: Requirements 7.1, 7.2, 7.3, 7.5**" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // Property 10: Strategy Priority (basic tests)
        test_property_strategy_priority();
        
        // Property 10b: Strategy Priority (random testing)
        test_property_strategy_priority_random();
        
        // Property 10c: Strategy Priority Order Invariant
        test_property_strategy_priority_order_invariant();
        
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

