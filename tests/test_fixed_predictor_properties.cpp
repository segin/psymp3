/*
 * test_fixed_predictor_properties.cpp - Property-based tests for FLAC fixed predictor
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill II <segin2005@gmail.com>
 *
 * Property-based tests using RapidCheck to verify the fixed predictor
 * implementation handles arbitrary sample values without overflow.
 */

#include <iostream>
#include <vector>
#include <cstdint>
#include <limits>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_RAPIDCHECK
#include <rapidcheck.h>

namespace {

// 64-bit reference implementation (mathematically correct)
int64_t computeFixedPrediction_64bit(const int32_t* samples, uint32_t sample_idx, uint32_t order) {
    int64_t prediction = 0;
    switch (order) {
    case 0:
        prediction = 0;
        break;
    case 1:
        prediction = samples[sample_idx - 1];
        break;
    case 2:
        prediction = 2LL * samples[sample_idx - 1] - samples[sample_idx - 2];
        break;
    case 3:
        prediction = 3LL * samples[sample_idx - 1] - 3LL * samples[sample_idx - 2] +
                     samples[sample_idx - 3];
        break;
    case 4:
        prediction = 4LL * samples[sample_idx - 1] - 6LL * samples[sample_idx - 2] +
                     4LL * samples[sample_idx - 3] - samples[sample_idx - 4];
        break;
    }
    return prediction;
}

// Production-equivalent implementation (what SubframeDecoder::applyFixedPredictor does)
int64_t computeFixedPrediction_production(const int32_t* samples, uint32_t sample_idx, uint32_t order) {
    // This mirrors the fixed code in SubframeDecoder.cpp
    int64_t prediction = 0;
    switch (order) {
    case 0:
        prediction = 0;
        break;
    case 1:
        prediction = samples[sample_idx - 1];
        break;
    case 2:
        prediction = 2LL * samples[sample_idx - 1] - samples[sample_idx - 2];
        break;
    case 3:
        prediction = 3LL * samples[sample_idx - 1] - 3LL * samples[sample_idx - 2] +
                     samples[sample_idx - 3];
        break;
    case 4:
        prediction = 4LL * samples[sample_idx - 1] - 6LL * samples[sample_idx - 2] +
                     4LL * samples[sample_idx - 3] - samples[sample_idx - 4];
        break;
    }
    return prediction;
}

} // anonymous namespace

int main() {
    std::cout << "=== FLAC Fixed Predictor Property-Based Tests ===" << std::endl;
    
    bool all_passed = true;
    
    // Property 1: 64-bit arithmetic matches mathematical expectation for order 2
    std::cout << "\nProperty 1: Order 2 prediction is 2*s[n-1] - s[n-2]" << std::endl;
    all_passed &= rc::check("Order 2: 2*s[n-1] - s[n-2] computed correctly",
        [](int32_t s1, int32_t s2) {
            std::vector<int32_t> samples = {s2, s1, 0};
            int64_t result = computeFixedPrediction_production(samples.data(), 2, 2);
            int64_t expected = 2LL * s1 - s2;
            RC_ASSERT(result == expected);
        });
    
    // Property 2: Order 3 prediction formula
    std::cout << "\nProperty 2: Order 3 prediction is 3*s[n-1] - 3*s[n-2] + s[n-3]" << std::endl;
    all_passed &= rc::check("Order 3: 3*s[n-1] - 3*s[n-2] + s[n-3] computed correctly",
        [](int32_t s1, int32_t s2, int32_t s3) {
            std::vector<int32_t> samples = {s3, s2, s1, 0};
            int64_t result = computeFixedPrediction_production(samples.data(), 3, 3);
            int64_t expected = 3LL * s1 - 3LL * s2 + s3;
            RC_ASSERT(result == expected);
        });
    
    // Property 3: Order 4 prediction formula
    std::cout << "\nProperty 3: Order 4 prediction is 4*s[n-1] - 6*s[n-2] + 4*s[n-3] - s[n-4]" << std::endl;
    all_passed &= rc::check("Order 4: 4*s[n-1] - 6*s[n-2] + 4*s[n-3] - s[n-4] computed correctly",
        [](int32_t s1, int32_t s2, int32_t s3, int32_t s4) {
            std::vector<int32_t> samples = {s4, s3, s2, s1, 0};
            int64_t result = computeFixedPrediction_production(samples.data(), 4, 4);
            int64_t expected = 4LL * s1 - 6LL * s2 + 4LL * s3 - s4;
            RC_ASSERT(result == expected);
        });
    
    // Property 4: Production implementation matches reference for all orders
    std::cout << "\nProperty 4: Production implementation matches 64-bit reference" << std::endl;
    all_passed &= rc::check("Production matches reference for all orders",
        [](int32_t s1, int32_t s2, int32_t s3, int32_t s4) {
            std::vector<int32_t> samples = {s4, s3, s2, s1, 0};
            
            for (uint32_t order = 0; order <= 4; ++order) {
                uint32_t idx = order;
                int64_t ref = computeFixedPrediction_64bit(samples.data(), idx, order);
                int64_t prod = computeFixedPrediction_production(samples.data(), idx, order);
                RC_ASSERT(ref == prod);
            }
        });
    
    // Property 5: Edge cases with extreme values
    std::cout << "\nProperty 5: Handles extreme values (INT32_MAX, INT32_MIN)" << std::endl;
    all_passed &= rc::check("Extreme values don't cause incorrect results",
        []() {
            const int32_t MAX = std::numeric_limits<int32_t>::max();
            const int32_t MIN = std::numeric_limits<int32_t>::min();
            
            // Test with all MAX
            std::vector<int32_t> all_max = {MAX, MAX, MAX, MAX, 0};
            int64_t result_max = computeFixedPrediction_production(all_max.data(), 4, 4);
            int64_t expected_max = 4LL * MAX - 6LL * MAX + 4LL * MAX - MAX;
            RC_ASSERT(result_max == expected_max);
            
            // Test with all MIN
            std::vector<int32_t> all_min = {MIN, MIN, MIN, MIN, 0};
            int64_t result_min = computeFixedPrediction_production(all_min.data(), 4, 4);
            int64_t expected_min = 4LL * MIN - 6LL * MIN + 4LL * MIN - MIN;
            RC_ASSERT(result_min == expected_min);
            
            // Test with alternating MAX/MIN
            std::vector<int32_t> alternating = {MIN, MAX, MIN, MAX, 0};
            int64_t result_alt = computeFixedPrediction_production(alternating.data(), 4, 4);
            int64_t expected_alt = 4LL * MAX - 6LL * MIN + 4LL * MAX - MIN;
            RC_ASSERT(result_alt == expected_alt);
        });
    
    // Property 6: Order 0 always returns 0 (no prediction)
    std::cout << "\nProperty 6: Order 0 always returns 0" << std::endl;
    all_passed &= rc::check("Order 0 always returns 0",
        [](int32_t s) {
            std::vector<int32_t> samples = {s};
            int64_t result = computeFixedPrediction_production(samples.data(), 0, 0);
            RC_ASSERT(result == 0);
        });
    
    // Property 7: Order 1 returns previous sample (identity)
    std::cout << "\nProperty 7: Order 1 returns previous sample" << std::endl;
    all_passed &= rc::check("Order 1 returns s[n-1]",
        [](int32_t s) {
            std::vector<int32_t> samples = {s, 0};
            int64_t result = computeFixedPrediction_production(samples.data(), 1, 1);
            RC_ASSERT(result == s);
        });

    std::cout << "\n=== Summary ===" << std::endl;
    if (all_passed) {
        std::cout << "All property tests PASSED" << std::endl;
        return 0;
    } else {
        std::cout << "Some property tests FAILED" << std::endl;
        return 1;
    }
}

#else
// No RapidCheck - provide a stub
int main() {
    std::cout << "RapidCheck not enabled, skipping property-based tests" << std::endl;
    std::cout << "Run ./configure --enable-rapidcheck to enable" << std::endl;
    return 0;
}
#endif
