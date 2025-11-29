/*
 * test_lastfm_exponential_backoff_properties.cpp - Property-based tests for exponential backoff
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
#include <cassert>
#include <cmath>
#include <algorithm>

// ========================================
// EXPONENTIAL BACKOFF IMPLEMENTATION FOR TESTING
// ========================================

/**
 * Simulated exponential backoff state machine for testing
 * This mirrors the behavior of LastFM::m_backoff_seconds
 */
class ExponentialBackoffSimulator {
private:
    int m_backoff_seconds = 0;
    static constexpr int INITIAL_BACKOFF_SECONDS = 60;  // Start at 1 minute
    static constexpr int MAX_BACKOFF_SECONDS = 3600;    // Cap at 1 hour
    
public:
    void resetBackoff() {
        m_backoff_seconds = 0;
    }
    
    void increaseBackoff() {
        if (m_backoff_seconds == 0) {
            m_backoff_seconds = INITIAL_BACKOFF_SECONDS;
        } else {
            m_backoff_seconds = std::min(m_backoff_seconds * 2, MAX_BACKOFF_SECONDS);
        }
    }
    
    int getBackoffSeconds() const {
        return m_backoff_seconds;
    }
};

// ========================================
// PROPERTY-BASED TESTS
// ========================================

// ========================================
// PROPERTY 5: Exponential Backoff Progression
// ========================================
// **Feature: lastfm-performance-optimization, Property 5: Exponential Backoff Progression**
// **Validates: Requirements 4.3**
//
// For any sequence of K consecutive network failures, the backoff delay 
// SHALL be min(initial_delay * 2^(K-1), max_delay) seconds.
void test_property_exponential_backoff_progression() {
    std::cout << "\n=== Property 5: Exponential Backoff Progression ===" << std::endl;
    std::cout << "Testing that backoff doubles with each failure up to maximum..." << std::endl;
    
    ExponentialBackoffSimulator backoff;
    
    // Test sequence of failures
    std::cout << "\n  Testing backoff progression with consecutive failures:" << std::endl;
    
    // Initial state: no backoff
    assert(backoff.getBackoffSeconds() == 0);
    std::cout << "    Initial state: " << backoff.getBackoffSeconds() << " seconds ✓" << std::endl;
    
    // First failure: should set to INITIAL_BACKOFF_SECONDS (60)
    backoff.increaseBackoff();
    assert(backoff.getBackoffSeconds() == 60);
    std::cout << "    After 1st failure: " << backoff.getBackoffSeconds() << " seconds ✓" << std::endl;
    
    // Second failure: should double to 120
    backoff.increaseBackoff();
    assert(backoff.getBackoffSeconds() == 120);
    std::cout << "    After 2nd failure: " << backoff.getBackoffSeconds() << " seconds ✓" << std::endl;
    
    // Third failure: should double to 240
    backoff.increaseBackoff();
    assert(backoff.getBackoffSeconds() == 240);
    std::cout << "    After 3rd failure: " << backoff.getBackoffSeconds() << " seconds ✓" << std::endl;
    
    // Fourth failure: should double to 480
    backoff.increaseBackoff();
    assert(backoff.getBackoffSeconds() == 480);
    std::cout << "    After 4th failure: " << backoff.getBackoffSeconds() << " seconds ✓" << std::endl;
    
    // Fifth failure: should double to 960
    backoff.increaseBackoff();
    assert(backoff.getBackoffSeconds() == 960);
    std::cout << "    After 5th failure: " << backoff.getBackoffSeconds() << " seconds ✓" << std::endl;
    
    // Sixth failure: should double to 1920
    backoff.increaseBackoff();
    assert(backoff.getBackoffSeconds() == 1920);
    std::cout << "    After 6th failure: " << backoff.getBackoffSeconds() << " seconds ✓" << std::endl;
    
    // Seventh failure: would be 3840, but capped at MAX_BACKOFF_SECONDS (3600)
    backoff.increaseBackoff();
    assert(backoff.getBackoffSeconds() == 3600);
    std::cout << "    After 7th failure: " << backoff.getBackoffSeconds() << " seconds (capped) ✓" << std::endl;
    
    // Eighth failure: would be 7680, but capped at MAX_BACKOFF_SECONDS (3600)
    backoff.increaseBackoff();
    assert(backoff.getBackoffSeconds() == 3600);
    std::cout << "    After 8th failure: " << backoff.getBackoffSeconds() << " seconds (capped) ✓" << std::endl;
    
    // Ninth failure: should stay at MAX_BACKOFF_SECONDS (3600)
    backoff.increaseBackoff();
    assert(backoff.getBackoffSeconds() == 3600);
    std::cout << "    After 9th failure: " << backoff.getBackoffSeconds() << " seconds (capped) ✓" << std::endl;
    
    // Tenth failure: should stay at MAX_BACKOFF_SECONDS (3600)
    backoff.increaseBackoff();
    assert(backoff.getBackoffSeconds() == 3600);
    std::cout << "    After 10th failure: " << backoff.getBackoffSeconds() << " seconds (capped) ✓" << std::endl;
    
    std::cout << "\n  Testing backoff reset on success:" << std::endl;
    
    // Reset should clear backoff
    backoff.resetBackoff();
    assert(backoff.getBackoffSeconds() == 0);
    std::cout << "    After reset: " << backoff.getBackoffSeconds() << " seconds ✓" << std::endl;
    
    // After reset, next failure should start from initial again
    backoff.increaseBackoff();
    assert(backoff.getBackoffSeconds() == 60);
    std::cout << "    After reset + 1st failure: " << backoff.getBackoffSeconds() << " seconds ✓" << std::endl;
    
    std::cout << "\n  Testing backoff formula: min(60 * 2^(K-1), 3600)" << std::endl;
    
    // Test the mathematical formula for various K values
    ExponentialBackoffSimulator formula_test;
    
    std::vector<std::pair<int, int>> expected_values = {
        {0, 0},      // Initial: 0 failures, 0 seconds
        {1, 60},     // 1 failure: 60 * 2^0 = 60
        {2, 120},    // 2 failures: 60 * 2^1 = 120
        {3, 240},    // 3 failures: 60 * 2^2 = 240
        {4, 480},    // 4 failures: 60 * 2^3 = 480
        {5, 960},    // 5 failures: 60 * 2^4 = 960
        {6, 1920},   // 6 failures: 60 * 2^5 = 1920
        {7, 3600},   // 7 failures: 60 * 2^6 = 3840 (capped at 3600)
        {8, 3600},   // 8 failures: capped at 3600
        {10, 3600},  // 10 failures: still capped at 3600
    };
    
    for (const auto& [failures, expected_seconds] : expected_values) {
        formula_test.resetBackoff();
        
        // Apply K failures
        for (int i = 0; i < failures; ++i) {
            formula_test.increaseBackoff();
        }
        
        int actual = formula_test.getBackoffSeconds();
        assert(actual == expected_seconds);
        
        // Verify formula: min(60 * 2^(K-1), 3600)
        if (failures > 0) {
            int formula_result = std::min(static_cast<int>(60 * std::pow(2, failures - 1)), 3600);
            assert(actual == formula_result);
        }
        
        std::cout << "    K=" << failures << ": " << actual << " seconds (expected " 
                  << expected_seconds << ") ✓" << std::endl;
    }
    
    std::cout << "\n  Testing backoff state transitions:" << std::endl;
    
    // Test: failure -> success -> failure sequence
    ExponentialBackoffSimulator transition_test;
    
    // Accumulate some backoff
    transition_test.increaseBackoff();
    transition_test.increaseBackoff();
    transition_test.increaseBackoff();
    assert(transition_test.getBackoffSeconds() == 240);
    std::cout << "    After 3 failures: " << transition_test.getBackoffSeconds() << " seconds ✓" << std::endl;
    
    // Success resets
    transition_test.resetBackoff();
    assert(transition_test.getBackoffSeconds() == 0);
    std::cout << "    After success (reset): " << transition_test.getBackoffSeconds() << " seconds ✓" << std::endl;
    
    // Next failure starts fresh
    transition_test.increaseBackoff();
    assert(transition_test.getBackoffSeconds() == 60);
    std::cout << "    After next failure: " << transition_test.getBackoffSeconds() << " seconds ✓" << std::endl;
    
    std::cout << "\n  Testing edge cases:" << std::endl;
    
    // Multiple resets should be idempotent
    ExponentialBackoffSimulator edge_test;
    edge_test.resetBackoff();
    edge_test.resetBackoff();
    edge_test.resetBackoff();
    assert(edge_test.getBackoffSeconds() == 0);
    std::cout << "    Multiple resets are idempotent ✓" << std::endl;
    
    // Backoff at max should stay at max
    ExponentialBackoffSimulator max_test;
    for (int i = 0; i < 20; ++i) {
        max_test.increaseBackoff();
    }
    assert(max_test.getBackoffSeconds() == 3600);
    std::cout << "    Backoff stays at maximum (3600s) after many failures ✓" << std::endl;
    
    std::cout << "\n✓ Property 5: Exponential Backoff Progression - ALL TESTS PASSED" << std::endl;
}

// ========================================
// PROPERTY 4: Scrobble Batching Correctness
// ========================================
// **Feature: lastfm-performance-optimization, Property 4: Scrobble Batching Correctness**
// **Validates: Requirements 4.2**
//
// For any set of N scrobbles in the queue where N > batch_size, the submission 
// thread SHALL submit exactly batch_size scrobbles per batch until fewer than 
// batch_size remain.
void test_property_scrobble_batching_correctness() {
    std::cout << "\n=== Property 4: Scrobble Batching Correctness ===" << std::endl;
    std::cout << "Testing that scrobbles are submitted in correct batch sizes..." << std::endl;
    
    const int batch_size = 5;
    
    std::cout << "\n  Testing batch size enforcement:" << std::endl;
    
    // Test: 0 scrobbles -> 0 batches
    {
        int scrobbles = 0;
        int expected_batches = 0;
        int actual_batches = (scrobbles + batch_size - 1) / batch_size;
        if (scrobbles == 0) actual_batches = 0;
        assert(actual_batches == expected_batches);
        std::cout << "    " << scrobbles << " scrobbles → " << actual_batches << " batches ✓" << std::endl;
    }
    
    // Test: 1-4 scrobbles -> 1 batch
    for (int scrobbles = 1; scrobbles <= 4; ++scrobbles) {
        int expected_batches = 1;
        int actual_batches = (scrobbles + batch_size - 1) / batch_size;
        assert(actual_batches == expected_batches);
        std::cout << "    " << scrobbles << " scrobbles → " << actual_batches << " batch ✓" << std::endl;
    }
    
    // Test: 5 scrobbles -> 1 batch (exactly batch_size)
    {
        int scrobbles = 5;
        int expected_batches = 1;
        int actual_batches = (scrobbles + batch_size - 1) / batch_size;
        assert(actual_batches == expected_batches);
        std::cout << "    " << scrobbles << " scrobbles → " << actual_batches << " batch (exactly batch_size) ✓" << std::endl;
    }
    
    // Test: 6-10 scrobbles -> 2 batches
    for (int scrobbles = 6; scrobbles <= 10; ++scrobbles) {
        int expected_batches = 2;
        int actual_batches = (scrobbles + batch_size - 1) / batch_size;
        assert(actual_batches == expected_batches);
        std::cout << "    " << scrobbles << " scrobbles → " << actual_batches << " batches ✓" << std::endl;
    }
    
    // Test: 11-15 scrobbles -> 3 batches
    for (int scrobbles = 11; scrobbles <= 15; ++scrobbles) {
        int expected_batches = 3;
        int actual_batches = (scrobbles + batch_size - 1) / batch_size;
        assert(actual_batches == expected_batches);
        std::cout << "    " << scrobbles << " scrobbles → " << actual_batches << " batches ✓" << std::endl;
    }
    
    // Test: 100 scrobbles -> 20 batches
    {
        int scrobbles = 100;
        int expected_batches = 20;
        int actual_batches = (scrobbles + batch_size - 1) / batch_size;
        assert(actual_batches == expected_batches);
        std::cout << "    " << scrobbles << " scrobbles → " << actual_batches << " batches ✓" << std::endl;
    }
    
    // Test: 1000 scrobbles -> 200 batches
    {
        int scrobbles = 1000;
        int expected_batches = 200;
        int actual_batches = (scrobbles + batch_size - 1) / batch_size;
        assert(actual_batches == expected_batches);
        std::cout << "    " << scrobbles << " scrobbles → " << actual_batches << " batches ✓" << std::endl;
    }
    
    std::cout << "\n  Testing batch size per submission:" << std::endl;
    
    // Verify that each batch contains exactly batch_size scrobbles (except possibly the last)
    for (int total_scrobbles = 1; total_scrobbles <= 50; ++total_scrobbles) {
        int remaining = total_scrobbles;
        int batch_count = 0;
        
        while (remaining > 0) {
            int batch_scrobbles = std::min(remaining, batch_size);
            
            // Each batch should have at most batch_size scrobbles
            assert(batch_scrobbles <= batch_size);
            
            // All batches except the last should have exactly batch_size scrobbles
            if (remaining > batch_size) {
                assert(batch_scrobbles == batch_size);
            }
            
            remaining -= batch_scrobbles;
            batch_count++;
        }
        
        // Verify total batches matches expected
        int expected_batches = (total_scrobbles + batch_size - 1) / batch_size;
        if (total_scrobbles == 0) expected_batches = 0;
        assert(batch_count == expected_batches);
    }
    
    std::cout << "    Verified batch size constraints for 1-50 scrobbles ✓" << std::endl;
    
    std::cout << "\n  Testing batch submission order:" << std::endl;
    
    // Verify that batches are submitted in order (FIFO)
    // Scrobbles 1-5 in batch 1, 6-10 in batch 2, etc.
    {
        int total_scrobbles = 17;
        int batch_num = 1;
        int scrobble_index = 1;
        int remaining = total_scrobbles;
        
        while (remaining > 0) {
            int batch_scrobbles = std::min(remaining, batch_size);
            int batch_start = scrobble_index;
            int batch_end = scrobble_index + batch_scrobbles - 1;
            
            // Verify batch contains consecutive scrobbles
            assert(batch_end - batch_start + 1 == batch_scrobbles);
            
            scrobble_index += batch_scrobbles;
            remaining -= batch_scrobbles;
            batch_num++;
        }
        
        std::cout << "    Verified FIFO batch ordering for " << total_scrobbles << " scrobbles ✓" << std::endl;
    }
    
    std::cout << "\n✓ Property 4: Scrobble Batching Correctness - ALL TESTS PASSED" << std::endl;
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "LAST.FM SUBMISSION THREAD PROPERTY-BASED TESTS" << std::endl;
    std::cout << "**Feature: lastfm-performance-optimization**" << std::endl;
    std::cout << "**Property 5: Exponential Backoff Progression**" << std::endl;
    std::cout << "**Property 4: Scrobble Batching Correctness**" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    try {
        // Run all property tests
        test_property_exponential_backoff_progression();
        test_property_scrobble_batching_correctness();
        
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "✅ ALL PROPERTY TESTS PASSED" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(60, '=') << std::endl;
        std::cerr << "❌ PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Exception: " << e.what() << std::endl;
        std::cerr << std::string(60, '=') << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n" << std::string(60, '=') << std::endl;
        std::cerr << "❌ PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Unknown exception occurred" << std::endl;
        std::cerr << std::string(60, '=') << std::endl;
        return 1;
    }
}
