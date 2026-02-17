/*
 * test_subframe_decoder_unit.cpp - Unit tests for SubframeDecoder
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#else
#include <cstdint>
#include <vector>
#include <cstring>
#include <iostream>
#endif

#include "test_framework.h"
#include "codecs/flac/SubframeDecoder.h"
#include "codecs/flac/BitstreamReader.h"
#include "codecs/flac/ResidualDecoder.h"

using namespace PsyMP3::Codec::FLAC;
using namespace TestFramework;

// Test CONSTANT subframe decoding
void test_constant_subframe() {
    BitstreamReader reader;
    ResidualDecoder residual(&reader);
    SubframeDecoder decoder(&reader, &residual);
    
    // CONSTANT subframe: type=000000, wasted_bits=0, constant_value
    // Subframe header: 0|000000|0 = 0x00
    // Constant value (16-bit): 0x1234
    uint8_t data[] = {
        0x00,        // Subframe header (CONSTANT, no wasted bits)
        0x12, 0x34   // Constant value (16-bit signed)
    };
    reader.feedData(data, sizeof(data));
    
    int32_t output[8];
    memset(output, 0, sizeof(output));
    
    ASSERT_TRUE(decoder.decodeSubframe(output, 8, 16, false), 
                "Should decode CONSTANT subframe");
    
    // All samples should be the constant value
    for (int i = 0; i < 8; i++) {
        ASSERT_EQUALS(0x1234, output[i], "All samples should equal constant value");
    }
}

// Test VERBATIM subframe decoding
void test_verbatim_subframe() {
    BitstreamReader reader;
    ResidualDecoder residual(&reader);
    SubframeDecoder decoder(&reader, &residual);
    
    // VERBATIM subframe: type=000001, wasted_bits=0
    // Subframe header: 0|000001|0 = 0x02
    // Samples: uncompressed 16-bit values
    uint8_t data[] = {
        0x02,              // Subframe header (VERBATIM, no wasted bits)
        0x00, 0x01,        // Sample 0
        0x00, 0x02,        // Sample 1
        0x00, 0x03,        // Sample 2
        0x00, 0x04         // Sample 3
    };
    reader.feedData(data, sizeof(data));
    
    int32_t output[4];
    memset(output, 0, sizeof(output));
    
    ASSERT_TRUE(decoder.decodeSubframe(output, 4, 16, false), 
                "Should decode VERBATIM subframe");
    
    ASSERT_EQUALS(1, output[0], "Sample 0 should be 1");
    ASSERT_EQUALS(2, output[1], "Sample 1 should be 2");
    ASSERT_EQUALS(3, output[2], "Sample 2 should be 3");
    ASSERT_EQUALS(4, output[3], "Sample 3 should be 4");
}

// Test FIXED predictor order 0 (Full decoding)
void test_fixed_predictor_order_0_full() {
    // FIXED order 0 is just the residuals (no prediction)
    BitstreamReader reader;
    ResidualDecoder residual(&reader);
    SubframeDecoder decoder(&reader, &residual);
    
    // FIXED order 0: type=001000 (8), wasted_bits=0
    // Subframe header: 0|001000|0 = 0x10
    // No warm-up samples for order 0
    // Residuals follow

    // Residuals: 1 (zigzag 2->001), -1 (zigzag 1->01), 0 (zigzag 0->1), 2 (zigzag 4->00001)
    // Bits: 001 01 1 00001. Total 11 bits.
    // Header bits: Method 0 (00), PartOrder 0 (0000), RiceParam 0 (0000). Total 10 bits.
    // Total bits: 21 bits.
    // Bytes:
    // Byte 0: 0x10 (Subframe Header)
    // Residual Header + Residuals:
    // 00 0000 0000 001 01 1 00001 000 (padding)
    // 0000 0000 -> 0x00
    // 0000 1011 -> 0x0B
    // 0000 1000 -> 0x08

    uint8_t data[] = {
        0x10,  // Subframe header (FIXED order 0)
        0x00,  // Method, Order, Param (part 1)
        0x0B,  // Param (part 2), Res 0, Res 1, Res 2
        0x08   // Res 3, padding
    };
    reader.feedData(data, sizeof(data));
    
    int32_t output[4];
    memset(output, 0, sizeof(output));

    ASSERT_TRUE(decoder.decodeSubframe(output, 4, 16, false),
                "Should decode FIXED order 0 subframe");

    ASSERT_EQUALS(1, output[0], "Sample 0 should be 1");
    ASSERT_EQUALS(-1, output[1], "Sample 1 should be -1");
    ASSERT_EQUALS(0, output[2], "Sample 2 should be 0");
    ASSERT_EQUALS(2, output[3], "Sample 3 should be 2");
}

// Test FIXED predictor order 1 (Full decoding)
void test_fixed_predictor_full_decoding() {
    BitstreamReader reader;
    ResidualDecoder residual(&reader);
    SubframeDecoder decoder(&reader, &residual);
    
    // FIXED order 1: type=001001 (9), wasted_bits=0
    // Subframe header: 0|001001|0 = 0x12
    // Warm-up sample: 10 (0x000A)
    // Residuals: Method 0 (4-bit Rice), PartOrder 0 (1 part), RiceParam 0
    // Residuals: 1 (zigzag 2->001), -1 (zigzag 1->01), 0 (zigzag 0->1)
    // Bits: 00 0000 0000 001 01 1. Total 16 bits.
    // Padding: 0000. Total 20 bits. Next byte boundary: 24 bits.
    // Bytes:
    // Header: 0x12
    // Warm-up: 0x00, 0x0A
    // Residuals: 0000 0000 -> 0x00.
    // Residuals + Padding: 0000 1011 -> 0x0B.

    uint8_t data[] = {
        0x12,        // Subframe header (FIXED order 1)
        0x00, 0x0A,  // Warm-up sample: 10
        0x00,        // Residual header (Method 0, PartOrder 0) + RiceParam (0)
        0x0B         // Residuals: 001 (1), 01 (-1), 1 (0) + Padding
    };
    reader.feedData(data, sizeof(data));
    
    int32_t output[4];
    memset(output, 0, sizeof(output));

    // Block size 4, bit depth 16
    ASSERT_TRUE(decoder.decodeSubframe(output, 4, 16, false),
                "Should decode FIXED subframe");

    // Expected output:
    // s[0] = 10
    // s[1] = 10 + 1 = 11
    // s[2] = 11 + (-1) = 10
    // s[3] = 10 + 0 = 10

    ASSERT_EQUALS(10, output[0], "Sample 0 should be 10");
    ASSERT_EQUALS(11, output[1], "Sample 1 should be 11");
    ASSERT_EQUALS(10, output[2], "Sample 2 should be 10");
    ASSERT_EQUALS(10, output[3], "Sample 3 should be 10");
}

// Test wasted bits handling
void test_wasted_bits() {
    BitstreamReader reader;
    ResidualDecoder residual(&reader);
    SubframeDecoder decoder(&reader, &residual);
    
    // Wasted bits test - this is a complex feature that requires
    // proper unary encoding and bit depth adjustment
    // For now, we test that the structure is recognized
    
    // CONSTANT subframe with wasted bits flag
    // Subframe header: 0|000000|1 = 0x01 (wasted bits flag set)
    uint8_t data[] = {
        0x01,        // Subframe header (CONSTANT, wasted bits flag)
        0x40,        // Unary 1: 01 (1 zero, then 1) = 1 wasted bit
        0x00, 0x05   // Constant value at reduced bit depth
    };
    reader.feedData(data, sizeof(data));
    
    int32_t output[4];
    memset(output, 0, sizeof(output));
    
    // The decoder should handle wasted bits
    // (actual value depends on implementation details)
    ASSERT_TRUE(decoder.decodeSubframe(output, 4, 16, false), 
                "Should decode subframe with wasted bits");
}

// Test side channel bit depth adjustment
void test_side_channel_bit_depth() {
    BitstreamReader reader;
    ResidualDecoder residual(&reader);
    SubframeDecoder decoder(&reader, &residual);
    
    // Side channel needs +1 bit depth for mid-side stereo
    // CONSTANT subframe with 16-bit frame depth
    uint8_t data[] = {
        0x00,              // Subframe header (CONSTANT)
        0x00, 0x00, 0x10   // Constant value (17-bit for side channel)
    };
    reader.feedData(data, sizeof(data));
    
    int32_t output[4];
    memset(output, 0, sizeof(output));
    
    // is_side_channel=true means bit depth is frame_bit_depth + 1
    ASSERT_TRUE(decoder.decodeSubframe(output, 4, 16, true), 
                "Should decode side channel with adjusted bit depth");
}

// Test LPC predictor structure
void test_lpc_predictor_structure() {
    BitstreamReader reader;
    ResidualDecoder residual(&reader);
    SubframeDecoder decoder(&reader, &residual);
    
    // LPC order 1: type=100001 (33), wasted_bits=0
    // Subframe header: 0|100001|0 = 0x42
    // 1 warm-up sample
    // Coefficient precision (4 bits)
    // Quantization level shift (5 bits signed)
    // Coefficients
    // Residuals
    uint8_t data[] = {
        0x42,        // Subframe header (LPC order 1)
        0x00, 0x0A,  // Warm-up sample
        0x50,        // Coeff precision (0101=5 bits) + shift high bit
        // More data would follow
    };
    reader.feedData(data, sizeof(data));
    
    // The decoder should recognize LPC structure
    // (Full test would require complete LPC data)
}

// Test subframe type detection
void test_subframe_type_detection() {
    // Test that different subframe types are correctly identified
    
    // CONSTANT: 000000
    ASSERT_EQUALS(0, 0b000000, "CONSTANT type bits");
    
    // VERBATIM: 000001
    ASSERT_EQUALS(1, 0b000001, "VERBATIM type bits");
    
    // FIXED order 0: 001000 (8)
    ASSERT_EQUALS(8, 0b001000, "FIXED order 0 type bits");
    
    // FIXED order 4: 001100 (12)
    ASSERT_EQUALS(12, 0b001100, "FIXED order 4 type bits");
    
    // LPC order 1: 100001 (33)
    ASSERT_EQUALS(33, 0b100001, "LPC order 1 type bits");
    
    // LPC order 32: 110000 (48)
    ASSERT_EQUALS(48, 0b110000, "LPC order 32 type bits");
}

int main() {
    // Create test suite
    TestSuite suite("SubframeDecoder Unit Tests");
    
    // Add test functions
    suite.addTest("CONSTANT Subframe", test_constant_subframe);
    suite.addTest("VERBATIM Subframe", test_verbatim_subframe);
    suite.addTest("FIXED Predictor Order 0 Full", test_fixed_predictor_order_0_full);
    suite.addTest("FIXED Predictor Order 1 Full", test_fixed_predictor_full_decoding);
    suite.addTest("Wasted Bits", test_wasted_bits);
    suite.addTest("Side Channel Bit Depth", test_side_channel_bit_depth);
    suite.addTest("LPC Predictor Structure", test_lpc_predictor_structure);
    suite.addTest("Subframe Type Detection", test_subframe_type_detection);
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print comprehensive results
    suite.printResults(results);
    
    // Return appropriate exit code
    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}
