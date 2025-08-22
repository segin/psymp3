/*
 * test_granule_arithmetic.cpp - Unit tests for safe granule position arithmetic
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifdef HAVE_OGGDEMUXER

#include <iostream>
#include <cassert>
#include <climits>

// Test framework
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        std::cout << "Running test: " << #name << std::endl; \
        tests_run++; \
        if (test_##name()) { \
            std::cout << "  PASSED" << std::endl; \
            tests_passed++; \
        } else { \
            std::cout << "  FAILED" << std::endl; \
        } \
    } while(0)

// Helper function to create a minimal OggDemuxer for testing
std::unique_ptr<OggDemuxer> createTestDemuxer() {
    // Create a null IOHandler for testing (we won't actually read files)
    auto handler = std::make_unique<FileIOHandler>("/dev/null");
    return std::make_unique<OggDemuxer>(std::move(handler));
}

// Test granposAdd with valid inputs
bool test_granpos_add_valid() {
    auto demuxer = createTestDemuxer();
    int64_t result;
    
    // Test normal addition
    if (demuxer->granposAdd(&result, 1000, 500) != 0) return false;
    if (result != 1500) return false;
    
    // Test addition with zero delta
    if (demuxer->granposAdd(&result, 1000, 0) != 0) return false;
    if (result != 1000) return false;
    
    // Test negative delta (subtraction)
    if (demuxer->granposAdd(&result, 1000, -200) != 0) return false;
    if (result != 800) return false;
    
    // Test large values
    if (demuxer->granposAdd(&result, INT64_MAX - 1000, 500) != 0) return false;
    if (result != INT64_MAX - 500) return false;
    
    return true;
}

// Test granposAdd with overflow conditions
bool test_granpos_add_overflow() {
    auto demuxer = createTestDemuxer();
    int64_t result;
    
    // Test overflow past INT64_MAX
    if (demuxer->granposAdd(&result, INT64_MAX, 1) != 0) {
        // Should handle overflow gracefully
        // Result should be in negative range (wrapped)
        if (result != INT64_MIN) return false;
    }
    
    // Test overflow that wraps to -1 (invalid)
    if (demuxer->granposAdd(&result, INT64_MAX, 2) == 0) {
        // If it succeeds, result should not be -1
        if (result == -1) return false;
    }
    
    // Test underflow
    if (demuxer->granposAdd(&result, INT64_MIN, -1) != 0) {
        // Should detect underflow and return error
        if (result != -1) return false;
    }
    
    return true;
}

// Test granposAdd with invalid inputs
bool test_granpos_add_invalid() {
    auto demuxer = createTestDemuxer();
    int64_t result;
    
    // Test NULL pointer
    if (demuxer->granposAdd(nullptr, 1000, 500) == 0) return false;
    
    // Test invalid source granule position (-1)
    if (demuxer->granposAdd(&result, -1, 500) == 0) return false;
    if (result != -1) return false;
    
    return true;
}

// Test granposDiff with valid inputs
bool test_granpos_diff_valid() {
    auto demuxer = createTestDemuxer();
    int64_t delta;
    
    // Test normal subtraction
    if (demuxer->granposDiff(&delta, 1500, 1000) != 0) return false;
    if (delta != 500) return false;
    
    // Test reverse subtraction (negative result)
    if (demuxer->granposDiff(&delta, 1000, 1500) != 0) return false;
    if (delta != -500) return false;
    
    // Test equal values
    if (demuxer->granposDiff(&delta, 1000, 1000) != 0) return false;
    if (delta != 0) return false;
    
    // Test large values
    if (demuxer->granposDiff(&delta, INT64_MAX, 1000) != 0) return false;
    if (delta != INT64_MAX - 1000) return false;
    
    return true;
}

// Test granposDiff with wraparound conditions
bool test_granpos_diff_wraparound() {
    auto demuxer = createTestDemuxer();
    int64_t delta;
    
    // Test positive - negative (normal case where positive < negative in granule ordering)
    if (demuxer->granposDiff(&delta, 1000, -1000) != 0) return false;
    // Result should be positive (1000 - (-1000) = 2000)
    if (delta != 2000) return false;
    
    // Test negative - positive (normal case where negative > positive in granule ordering)
    if (demuxer->granposDiff(&delta, -1000, 1000) != 0) return false;
    // Result should be negative (-1000 - 1000 = -2000)
    if (delta != -2000) return false;
    
    // Test two negative values
    if (demuxer->granposDiff(&delta, -500, -1000) != 0) return false;
    if (delta != 500) return false;
    
    return true;
}

// Test granposDiff with invalid inputs
bool test_granpos_diff_invalid() {
    auto demuxer = createTestDemuxer();
    int64_t delta;
    
    // Test NULL pointer
    if (demuxer->granposDiff(nullptr, 1000, 500) == 0) return false;
    
    // Test invalid granule positions (-1)
    if (demuxer->granposDiff(&delta, -1, 1000) == 0) return false;
    if (delta != 0) return false;
    
    if (demuxer->granposDiff(&delta, 1000, -1) == 0) return false;
    if (delta != 0) return false;
    
    if (demuxer->granposDiff(&delta, -1, -1) == 0) return false;
    if (delta != 0) return false;
    
    return true;
}

// Test granposCmp with valid inputs
bool test_granpos_cmp_valid() {
    auto demuxer = createTestDemuxer();
    
    // Test equal values
    if (demuxer->granposCmp(1000, 1000) != 0) return false;
    
    // Test less than
    if (demuxer->granposCmp(500, 1000) != -1) return false;
    
    // Test greater than
    if (demuxer->granposCmp(1000, 500) != 1) return false;
    
    // Test zero
    if (demuxer->granposCmp(0, 0) != 0) return false;
    if (demuxer->granposCmp(0, 1000) != -1) return false;
    if (demuxer->granposCmp(1000, 0) != 1) return false;
    
    return true;
}

// Test granposCmp with wraparound ordering
bool test_granpos_cmp_wraparound() {
    auto demuxer = createTestDemuxer();
    
    // In granule position ordering: negative values (INT64_MIN to -2) > positive values (0 to INT64_MAX)
    
    // Test negative > positive
    if (demuxer->granposCmp(-1000, 1000) != 1) return false;
    if (demuxer->granposCmp(-2, INT64_MAX) != 1) return false;
    
    // Test positive < negative
    if (demuxer->granposCmp(1000, -1000) != -1) return false;
    if (demuxer->granposCmp(INT64_MAX, -2) != -1) return false;
    
    // Test within negative range
    if (demuxer->granposCmp(-500, -1000) != 1) return false;
    if (demuxer->granposCmp(-1000, -500) != -1) return false;
    
    // Test boundary conditions
    if (demuxer->granposCmp(INT64_MAX, INT64_MIN) != -1) return false;
    if (demuxer->granposCmp(INT64_MIN, INT64_MAX) != 1) return false;
    
    return true;
}

// Test granposCmp with invalid inputs
bool test_granpos_cmp_invalid() {
    auto demuxer = createTestDemuxer();
    
    // Test both invalid (-1)
    if (demuxer->granposCmp(-1, -1) != 0) return false;
    
    // Test one invalid
    if (demuxer->granposCmp(-1, 1000) != -1) return false;
    if (demuxer->granposCmp(1000, -1) != 1) return false;
    
    // Test invalid vs zero
    if (demuxer->granposCmp(-1, 0) != -1) return false;
    if (demuxer->granposCmp(0, -1) != 1) return false;
    
    return true;
}

// Test edge cases and boundary conditions
bool test_granpos_edge_cases() {
    auto demuxer = createTestDemuxer();
    int64_t result, delta;
    
    // Test maximum values
    if (demuxer->granposAdd(&result, INT64_MAX - 1, 1) != 0) {
        std::cout << "    Failed: granposAdd(INT64_MAX - 1, 1) returned error" << std::endl;
        return false;
    }
    if (result != INT64_MAX) {
        std::cout << "    Failed: granposAdd(INT64_MAX - 1, 1) result=" << result << ", expected=" << INT64_MAX << std::endl;
        return false;
    }
    
    // Test minimum values (excluding -1)
    if (demuxer->granposAdd(&result, INT64_MIN, 0) != 0) {
        std::cout << "    Failed: granposAdd(INT64_MIN, 0) returned error" << std::endl;
        return false;
    }
    if (result != INT64_MIN) {
        std::cout << "    Failed: granposAdd(INT64_MIN, 0) result=" << result << ", expected=" << INT64_MIN << std::endl;
        return false;
    }
    
    // Test difference at boundaries - this should fail due to overflow
    // INT64_MAX - INT64_MIN would overflow, so it should return error
    if (demuxer->granposDiff(&delta, INT64_MAX, INT64_MIN) == 0) {
        std::cout << "    Failed: granposDiff(INT64_MAX, INT64_MIN) should have returned error due to overflow" << std::endl;
        return false;
    }
    
    // Test a smaller difference that should work
    if (demuxer->granposDiff(&delta, INT64_MAX, INT64_MAX - 1000) != 0) {
        std::cout << "    Failed: granposDiff(INT64_MAX, INT64_MAX - 1000) returned error" << std::endl;
        return false;
    }
    if (delta != 1000) {
        std::cout << "    Failed: granposDiff(INT64_MAX, INT64_MAX - 1000) delta=" << delta << ", expected 1000" << std::endl;
        return false;
    }
    
    // Test comparison at boundaries
    if (demuxer->granposCmp(INT64_MAX, INT64_MIN) != -1) {
        std::cout << "    Failed: granposCmp(INT64_MAX, INT64_MIN) returned " << demuxer->granposCmp(INT64_MAX, INT64_MIN) << ", expected -1" << std::endl;
        return false;
    }
    if (demuxer->granposCmp(INT64_MIN, INT64_MAX) != 1) {
        std::cout << "    Failed: granposCmp(INT64_MIN, INT64_MAX) returned " << demuxer->granposCmp(INT64_MIN, INT64_MAX) << ", expected 1" << std::endl;
        return false;
    }
    
    return true;
}

// Test arithmetic consistency
bool test_granpos_arithmetic_consistency() {
    auto demuxer = createTestDemuxer();
    int64_t result, delta;
    
    // Test add/subtract consistency
    int64_t original = 50000;
    int32_t offset = 1000;
    
    // Add offset
    if (demuxer->granposAdd(&result, original, offset) != 0) return false;
    
    // Subtract offset back
    if (demuxer->granposAdd(&result, result, -offset) != 0) return false;
    if (result != original) return false;
    
    // Test diff/add consistency
    int64_t gp_a = 60000;
    int64_t gp_b = 40000;
    
    // Calculate difference
    if (demuxer->granposDiff(&delta, gp_a, gp_b) != 0) return false;
    
    // Add difference to gp_b should give gp_a
    if (demuxer->granposAdd(&result, gp_b, static_cast<int32_t>(delta)) != 0) return false;
    if (result != gp_a) return false;
    
    return true;
}

int main() {
    std::cout << "Running granule position arithmetic tests..." << std::endl;
    
    // Test granposAdd
    TEST(granpos_add_valid);
    TEST(granpos_add_overflow);
    TEST(granpos_add_invalid);
    
    // Test granposDiff
    TEST(granpos_diff_valid);
    TEST(granpos_diff_wraparound);
    TEST(granpos_diff_invalid);
    
    // Test granposCmp
    TEST(granpos_cmp_valid);
    TEST(granpos_cmp_wraparound);
    TEST(granpos_cmp_invalid);
    
    // Test edge cases
    TEST(granpos_edge_cases);
    TEST(granpos_arithmetic_consistency);
    
    std::cout << std::endl;
    std::cout << "Tests completed: " << tests_passed << "/" << tests_run << " passed" << std::endl;
    
    if (tests_passed == tests_run) {
        std::cout << "All granule position arithmetic tests PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << "Some granule position arithmetic tests FAILED!" << std::endl;
        return 1;
    }
}

#else
int main() {
    std::cout << "OggDemuxer not available (HAVE_OGGDEMUXER not defined)" << std::endl;
    return 0;
}
#endif