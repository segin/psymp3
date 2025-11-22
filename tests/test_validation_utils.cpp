/**
 * test_validation_utils.cpp - Unit tests for FLAC validation utilities
 * 
 * Tests validation and security functions for the Native FLAC decoder.
 * Covers bounds checking, resource limits, and input validation.
 * 
 * Requirements tested:
 * - Requirement 23: Forbidden pattern detection
 * - Requirement 37: Residual value limits
 * - Requirement 48: Security and DoS protection
 * - Requirement 57: Sample value range validation
 * - Requirement 58: Block size constraints
 */

#include "codecs/flac/ValidationUtils.h"
#include <cassert>
#include <cstdio>
#include <climits>

using namespace PsyMP3::Codec::FLAC;

// Test counter
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { \
        printf("Testing %s... ", name); \
        fflush(stdout); \
    } while(0)

#define PASS() \
    do { \
        printf("PASS\n"); \
        tests_passed++; \
    } while(0)

#define FAIL(msg) \
    do { \
        printf("FAIL: %s\n", msg); \
        tests_failed++; \
    } while(0)

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            FAIL(#expr " is false"); \
            return; \
        } \
    } while(0)

#define ASSERT_FALSE(expr) \
    do { \
        if (expr) { \
            FAIL(#expr " is true"); \
            return; \
        } \
    } while(0)

#define ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            char buf[256]; \
            snprintf(buf, sizeof(buf), #a " (%u) != " #b " (%u)", (unsigned)(a), (unsigned)(b)); \
            FAIL(buf); \
            return; \
        } \
    } while(0)

// ============================================================================
// Bounds Checking Tests
// ============================================================================

void test_buffer_bounds_valid() {
    TEST("buffer bounds checking with valid range");
    ASSERT_TRUE(ValidationUtils::checkBufferBounds(100, 0, 50));
    ASSERT_TRUE(ValidationUtils::checkBufferBounds(100, 50, 50));
    ASSERT_TRUE(ValidationUtils::checkBufferBounds(100, 99, 1));
    PASS();
}

void test_buffer_bounds_invalid() {
    TEST("buffer bounds checking with invalid range");
    ASSERT_FALSE(ValidationUtils::checkBufferBounds(100, 0, 101));
    ASSERT_FALSE(ValidationUtils::checkBufferBounds(100, 50, 51));
    ASSERT_FALSE(ValidationUtils::checkBufferBounds(100, 101, 0));
    PASS();
}

void test_array_index_valid() {
    TEST("array index checking with valid indices");
    ASSERT_TRUE(ValidationUtils::checkArrayIndex(100, 0));
    ASSERT_TRUE(ValidationUtils::checkArrayIndex(100, 50));
    ASSERT_TRUE(ValidationUtils::checkArrayIndex(100, 99));
    PASS();
}

void test_array_index_invalid() {
    TEST("array index checking with invalid indices");
    ASSERT_FALSE(ValidationUtils::checkArrayIndex(100, 100));
    ASSERT_FALSE(ValidationUtils::checkArrayIndex(100, 101));
    ASSERT_FALSE(ValidationUtils::checkArrayIndex(0, 0));
    PASS();
}

void test_multiply_overflow() {
    TEST("multiply overflow detection");
    uint32_t result;
    
    // Valid multiplications
    ASSERT_TRUE(ValidationUtils::checkMultiplyOverflow(100, 100, result));
    ASSERT_EQ(result, 10000);
    
    ASSERT_TRUE(ValidationUtils::checkMultiplyOverflow(0, 1000000, result));
    ASSERT_EQ(result, 0);
    
    // Overflow cases
    ASSERT_FALSE(ValidationUtils::checkMultiplyOverflow(0xFFFFFFFF, 2, result));
    ASSERT_FALSE(ValidationUtils::checkMultiplyOverflow(65536, 65536, result));
    
    PASS();
}

void test_add_overflow() {
    TEST("add overflow detection");
    uint32_t result;
    
    // Valid additions
    ASSERT_TRUE(ValidationUtils::checkAddOverflow(100, 200, result));
    ASSERT_EQ(result, 300);
    
    ASSERT_TRUE(ValidationUtils::checkAddOverflow(0, 0xFFFFFFFF, result));
    ASSERT_EQ(result, 0xFFFFFFFF);
    
    // Overflow cases
    ASSERT_FALSE(ValidationUtils::checkAddOverflow(0xFFFFFFFF, 1, result));
    ASSERT_FALSE(ValidationUtils::checkAddOverflow(0x80000000, 0x80000000, result));
    
    PASS();
}

void test_shift_overflow() {
    TEST("shift overflow detection");
    uint32_t result;
    
    // Valid shifts
    ASSERT_TRUE(ValidationUtils::checkShiftOverflow(1, 10, result));
    ASSERT_EQ(result, 1024);
    
    ASSERT_TRUE(ValidationUtils::checkShiftOverflow(0, 31, result));
    ASSERT_EQ(result, 0);
    
    // Overflow cases
    ASSERT_FALSE(ValidationUtils::checkShiftOverflow(1, 32, result));
    ASSERT_FALSE(ValidationUtils::checkShiftOverflow(0xFFFFFFFF, 1, result));
    ASSERT_FALSE(ValidationUtils::checkShiftOverflow(0x80000000, 1, result));
    
    PASS();
}

// ============================================================================
// Input Validation Tests
// ============================================================================

void test_block_size_validation() {
    TEST("block size validation");
    
    // Valid block sizes
    ASSERT_TRUE(ValidationUtils::validateBlockSize(16, false));
    ASSERT_TRUE(ValidationUtils::validateBlockSize(1024, false));
    ASSERT_TRUE(ValidationUtils::validateBlockSize(65535, false));
    
    // Valid small block (last frame)
    ASSERT_TRUE(ValidationUtils::validateBlockSize(1, true));
    ASSERT_TRUE(ValidationUtils::validateBlockSize(15, true));
    
    // Invalid block sizes
    ASSERT_FALSE(ValidationUtils::validateBlockSize(0, false));
    ASSERT_FALSE(ValidationUtils::validateBlockSize(15, false));  // Too small
    ASSERT_FALSE(ValidationUtils::validateBlockSize(65536, false));  // Forbidden
    ASSERT_FALSE(ValidationUtils::validateBlockSize(65537, false));  // Too large
    
    PASS();
}

void test_sample_rate_validation() {
    TEST("sample rate validation");
    
    // Valid sample rates
    ASSERT_TRUE(ValidationUtils::validateSampleRate(0));  // Get from STREAMINFO
    ASSERT_TRUE(ValidationUtils::validateSampleRate(8000));
    ASSERT_TRUE(ValidationUtils::validateSampleRate(44100));
    ASSERT_TRUE(ValidationUtils::validateSampleRate(192000));
    ASSERT_TRUE(ValidationUtils::validateSampleRate(1048575));  // Maximum
    
    // Invalid sample rates
    ASSERT_FALSE(ValidationUtils::validateSampleRate(1048576));  // Too high
    ASSERT_FALSE(ValidationUtils::validateSampleRate(0xFFFFFFFF));
    
    PASS();
}

void test_bit_depth_validation() {
    TEST("bit depth validation");
    
    // Valid bit depths
    ASSERT_TRUE(ValidationUtils::validateBitDepth(0));  // Get from STREAMINFO
    ASSERT_TRUE(ValidationUtils::validateBitDepth(4));  // Minimum
    ASSERT_TRUE(ValidationUtils::validateBitDepth(16));
    ASSERT_TRUE(ValidationUtils::validateBitDepth(24));
    ASSERT_TRUE(ValidationUtils::validateBitDepth(32));  // Maximum
    
    // Invalid bit depths
    ASSERT_FALSE(ValidationUtils::validateBitDepth(3));  // Too small
    ASSERT_FALSE(ValidationUtils::validateBitDepth(33));  // Too large
    
    PASS();
}

void test_channel_count_validation() {
    TEST("channel count validation");
    
    // Valid channel counts
    ASSERT_TRUE(ValidationUtils::validateChannelCount(1));
    ASSERT_TRUE(ValidationUtils::validateChannelCount(2));
    ASSERT_TRUE(ValidationUtils::validateChannelCount(6));
    ASSERT_TRUE(ValidationUtils::validateChannelCount(8));
    
    // Invalid channel counts
    ASSERT_FALSE(ValidationUtils::validateChannelCount(0));
    ASSERT_FALSE(ValidationUtils::validateChannelCount(9));
    
    PASS();
}

void test_partition_order_validation() {
    TEST("partition order validation");
    
    // Valid partition orders
    ASSERT_TRUE(ValidationUtils::validatePartitionOrder(0, 1024, 0));
    ASSERT_TRUE(ValidationUtils::validatePartitionOrder(4, 1024, 0));
    ASSERT_TRUE(ValidationUtils::validatePartitionOrder(8, 4096, 4));
    
    // Invalid partition orders
    ASSERT_FALSE(ValidationUtils::validatePartitionOrder(16, 1024, 0));  // Too large
    ASSERT_FALSE(ValidationUtils::validatePartitionOrder(4, 1000, 0));  // Not evenly divisible
    ASSERT_FALSE(ValidationUtils::validatePartitionOrder(10, 1024, 1024));  // Partition too small
    
    PASS();
}

void test_lpc_order_validation() {
    TEST("LPC order validation");
    
    // Valid LPC orders
    ASSERT_TRUE(ValidationUtils::validateLPCOrder(1, 1024));
    ASSERT_TRUE(ValidationUtils::validateLPCOrder(12, 1024));
    ASSERT_TRUE(ValidationUtils::validateLPCOrder(32, 1024));
    
    // Invalid LPC orders
    ASSERT_FALSE(ValidationUtils::validateLPCOrder(0, 1024));  // Too small
    ASSERT_FALSE(ValidationUtils::validateLPCOrder(33, 1024));  // Too large
    ASSERT_FALSE(ValidationUtils::validateLPCOrder(32, 32));  // Equals block size
    ASSERT_FALSE(ValidationUtils::validateLPCOrder(32, 16));  // Exceeds block size
    
    PASS();
}

void test_fixed_order_validation() {
    TEST("FIXED order validation");
    
    // Valid FIXED orders
    ASSERT_TRUE(ValidationUtils::validateFixedOrder(0, 1024));
    ASSERT_TRUE(ValidationUtils::validateFixedOrder(2, 1024));
    ASSERT_TRUE(ValidationUtils::validateFixedOrder(4, 1024));
    
    // Invalid FIXED orders
    ASSERT_FALSE(ValidationUtils::validateFixedOrder(5, 1024));  // Too large
    ASSERT_FALSE(ValidationUtils::validateFixedOrder(4, 4));  // Equals block size
    
    PASS();
}

void test_sample_value_validation() {
    TEST("sample value validation");
    
    // Valid 16-bit samples
    ASSERT_TRUE(ValidationUtils::validateSampleValue(0, 16));
    ASSERT_TRUE(ValidationUtils::validateSampleValue(32767, 16));
    ASSERT_TRUE(ValidationUtils::validateSampleValue(-32768, 16));
    
    // Invalid 16-bit samples
    ASSERT_FALSE(ValidationUtils::validateSampleValue(32768, 16));
    ASSERT_FALSE(ValidationUtils::validateSampleValue(-32769, 16));
    
    // Valid 8-bit samples
    ASSERT_TRUE(ValidationUtils::validateSampleValue(127, 8));
    ASSERT_TRUE(ValidationUtils::validateSampleValue(-128, 8));
    
    // Invalid 8-bit samples
    ASSERT_FALSE(ValidationUtils::validateSampleValue(128, 8));
    ASSERT_FALSE(ValidationUtils::validateSampleValue(-129, 8));
    
    PASS();
}

void test_residual_value_validation() {
    TEST("residual value validation");
    
    // Valid residuals
    ASSERT_TRUE(ValidationUtils::validateResidualValue(0));
    ASSERT_TRUE(ValidationUtils::validateResidualValue(1000));
    ASSERT_TRUE(ValidationUtils::validateResidualValue(-1000));
    ASSERT_TRUE(ValidationUtils::validateResidualValue(INT32_MAX));
    ASSERT_TRUE(ValidationUtils::validateResidualValue(INT32_MIN + 1));
    
    // Invalid residual (most negative value is forbidden)
    ASSERT_FALSE(ValidationUtils::validateResidualValue(INT32_MIN));
    
    PASS();
}

void test_forbidden_patterns() {
    TEST("forbidden pattern detection");
    
    // Valid patterns
    ASSERT_TRUE(ValidationUtils::checkForbiddenSampleRateBits(0b0000));
    ASSERT_TRUE(ValidationUtils::checkForbiddenSampleRateBits(0b1110));
    ASSERT_TRUE(ValidationUtils::checkForbiddenPredictorPrecision(0b0000));
    ASSERT_TRUE(ValidationUtils::checkForbiddenPredictorPrecision(0b1110));
    
    // Forbidden patterns
    ASSERT_FALSE(ValidationUtils::checkForbiddenSampleRateBits(0b1111));
    ASSERT_FALSE(ValidationUtils::checkForbiddenPredictorPrecision(0b1111));
    
    PASS();
}

void test_predictor_shift_validation() {
    TEST("predictor shift validation");
    
    // Valid shifts
    ASSERT_TRUE(ValidationUtils::validatePredictorShift(0));
    ASSERT_TRUE(ValidationUtils::validatePredictorShift(15));
    ASSERT_TRUE(ValidationUtils::validatePredictorShift(31));
    
    // Invalid shifts
    ASSERT_FALSE(ValidationUtils::validatePredictorShift(-1));
    ASSERT_FALSE(ValidationUtils::validatePredictorShift(32));
    
    PASS();
}

void test_wasted_bits_validation() {
    TEST("wasted bits validation");
    
    // Valid wasted bits
    ASSERT_TRUE(ValidationUtils::validateWastedBits(0, 16));
    ASSERT_TRUE(ValidationUtils::validateWastedBits(4, 16));
    ASSERT_TRUE(ValidationUtils::validateWastedBits(15, 16));
    
    // Invalid wasted bits
    ASSERT_FALSE(ValidationUtils::validateWastedBits(16, 16));  // Equals bit depth
    ASSERT_FALSE(ValidationUtils::validateWastedBits(17, 16));  // Exceeds bit depth
    
    PASS();
}

void test_metadata_block_length_validation() {
    TEST("metadata block length validation");
    
    // Valid lengths
    ASSERT_TRUE(ValidationUtils::validateMetadataBlockLength(0));
    ASSERT_TRUE(ValidationUtils::validateMetadataBlockLength(1024));
    ASSERT_TRUE(ValidationUtils::validateMetadataBlockLength(16777215));  // Maximum
    
    // Invalid lengths
    ASSERT_FALSE(ValidationUtils::validateMetadataBlockLength(16777216));
    
    PASS();
}

void test_streaminfo_block_sizes() {
    TEST("STREAMINFO block size constraints");
    
    // Valid constraints
    ASSERT_TRUE(ValidationUtils::validateStreamInfoBlockSizes(16, 16));
    ASSERT_TRUE(ValidationUtils::validateStreamInfoBlockSizes(16, 4096));
    ASSERT_TRUE(ValidationUtils::validateStreamInfoBlockSizes(4096, 4096));
    ASSERT_TRUE(ValidationUtils::validateStreamInfoBlockSizes(16, 65535));
    
    // Invalid constraints
    ASSERT_FALSE(ValidationUtils::validateStreamInfoBlockSizes(0, 4096));  // Min too small
    ASSERT_FALSE(ValidationUtils::validateStreamInfoBlockSizes(16, 0));  // Max too small
    ASSERT_FALSE(ValidationUtils::validateStreamInfoBlockSizes(4096, 16));  // Min > Max
    ASSERT_FALSE(ValidationUtils::validateStreamInfoBlockSizes(16, 65536));  // Max too large
    
    PASS();
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    printf("=== FLAC Validation Utils Test Suite ===\n\n");
    
    // Bounds checking tests
    printf("--- Bounds Checking Tests ---\n");
    test_buffer_bounds_valid();
    test_buffer_bounds_invalid();
    test_array_index_valid();
    test_array_index_invalid();
    test_multiply_overflow();
    test_add_overflow();
    test_shift_overflow();
    
    // Input validation tests
    printf("\n--- Input Validation Tests ---\n");
    test_block_size_validation();
    test_sample_rate_validation();
    test_bit_depth_validation();
    test_channel_count_validation();
    test_partition_order_validation();
    test_lpc_order_validation();
    test_fixed_order_validation();
    test_sample_value_validation();
    test_residual_value_validation();
    test_forbidden_patterns();
    test_predictor_shift_validation();
    test_wasted_bits_validation();
    test_metadata_block_length_validation();
    test_streaminfo_block_sizes();
    
    // Summary
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Total:  %d\n", tests_passed + tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}
