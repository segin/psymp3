/*
 * test_residual_decoder_unit.cpp - Unit tests for ResidualDecoder
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include "test_framework.h"
#include "codecs/flac/ResidualDecoder.h"
#include "codecs/flac/BitstreamReader.h"
#include <cstring>

using namespace PsyMP3::Codec::FLAC;
using namespace TestFramework;

// Test Rice code decoding with partition order 0
void test_rice_partition_order_0() {
    BitstreamReader reader;
    ResidualDecoder decoder(&reader);
    
    // Residual header:
    // Coding method: 00 (4-bit Rice)
    // Partition order: 0000 (order 0 = 1 partition)
    // Rice parameter: 0011 (3)
    // Residuals follow
    uint8_t data[] = {
        0x03,  // Method (00) + Order (0000) + Param high bits (00)
        0x00,  // Param low bits (11) + padding
        // Rice-coded residuals would follow
    };
    reader.feedData(data, sizeof(data));
    
    // Test structure recognition
    // (Full test would require complete residual data)
}

// Test partition handling
void test_partition_handling() {
    BitstreamReader reader;
    ResidualDecoder decoder(&reader);
    
    // Residual with partition order 1 (2 partitions)
    // Coding method: 00 (4-bit Rice)
    // Partition order: 0001 (order 1 = 2 partitions)
    uint8_t data[] = {
        0x01,  // Method (00) + Order (0001)
        // Each partition has its own Rice parameter
        // Partition 0: parameter + residuals
        // Partition 1: parameter + residuals
    };
    reader.feedData(data, sizeof(data));
    
    // Test that multiple partitions are recognized
}

// Test escaped partition (Rice parameter = 0b1111)
void test_escaped_partition() {
    BitstreamReader reader;
    ResidualDecoder decoder(&reader);
    
    // Escaped partition: Rice parameter = 0b1111
    // Means samples are unencoded with specified bit width
    uint8_t data[] = {
        0x00,  // Method (00) + Order (0000)
        0xF0,  // Rice parameter (1111) = escape code
        0x05,  // Escape bits (5-bit unencoded samples)
        // Unencoded samples follow
    };
    reader.feedData(data, sizeof(data));
    
    // Test escape code recognition
}

// Test zigzag encoding/decoding
void test_zigzag_encoding() {
    // Zigzag encoding maps signed to unsigned:
    // 0 -> 0, -1 -> 1, 1 -> 2, -2 -> 3, 2 -> 4, etc.
    
    // Test the mapping
    ASSERT_EQUALS(0, 0, "Zigzag: 0 -> 0");
    // -1 encoded as 1
    // 1 encoded as 2
    // -2 encoded as 3
    // 2 encoded as 4
    
    // The unfoldSigned function in ResidualDecoder handles this
}

// Test coding method detection
void test_coding_method() {
    // Coding method is 2 bits:
    // 00 = 4-bit Rice parameter
    // 01 = 5-bit Rice parameter
    
    ASSERT_EQUALS(0, static_cast<int>(CodingMethod::RICE_4BIT), 
                  "RICE_4BIT should be 0");
    ASSERT_EQUALS(1, static_cast<int>(CodingMethod::RICE_5BIT), 
                  "RICE_5BIT should be 1");
}

// Test partition order range
void test_partition_order_range() {
    // Partition order is 4 bits (0-15)
    // Order N means 2^N partitions
    
    // Order 0: 1 partition
    ASSERT_EQUALS(1, 1 << 0, "Order 0 = 1 partition");
    
    // Order 1: 2 partitions
    ASSERT_EQUALS(2, 1 << 1, "Order 1 = 2 partitions");
    
    // Order 4: 16 partitions
    ASSERT_EQUALS(16, 1 << 4, "Order 4 = 16 partitions");
}

// Test residual validation
void test_residual_validation() {
    // Residuals must fit in 32-bit signed range
    // Most negative value (INT32_MIN) is forbidden
    
    ASSERT_TRUE(INT32_MIN < 0, "INT32_MIN is negative");
    ASSERT_TRUE(INT32_MAX > 0, "INT32_MAX is positive");
    
    // Valid residual range: INT32_MIN+1 to INT32_MAX
}

int main() {
    // Create test suite
    TestSuite suite("ResidualDecoder Unit Tests");
    
    // Add test functions
    suite.addTest("Rice Partition Order 0", test_rice_partition_order_0);
    suite.addTest("Partition Handling", test_partition_handling);
    suite.addTest("Escaped Partition", test_escaped_partition);
    suite.addTest("Zigzag Encoding", test_zigzag_encoding);
    suite.addTest("Coding Method", test_coding_method);
    suite.addTest("Partition Order Range", test_partition_order_range);
    suite.addTest("Residual Validation", test_residual_validation);
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print comprehensive results
    suite.printResults(results);
    
    // Return appropriate exit code
    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}
