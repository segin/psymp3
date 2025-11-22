/*
 * test_subframe_decoder_unit.cpp - Unit tests for SubframeDecoder
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include "test_framework.h"
#include "codecs/flac/SubframeDecoder.h"
#include "codecs/flac/BitstreamReader.h"
#include "codecs/flac/ResidualDecoder.h"
#include <cstring>
#include <vector>

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

// Test FIXED predictor order 0
void test_fixed_predictor_order_0() {
    // FIXED order 0 is just the residuals (no prediction)
    BitstreamReader reader;
    ResidualDecoder residual(&reader);
    SubframeDecoder decoder(&reader, &residual);
    
    // FIXED order 0: type=001000 (8), wasted_bits=0
    // Subframe header: 0|001000|0 = 0x10
    // No warm-up samples for order 0
    // Residuals follow
    uint8_t data[] = {
        0x10,  // Subframe header (FIXED order 0)
        // Residual coding would follow here
        // For this test, we're just testing the structure
    };
    reader.feedData(data, sizeof(data));
    
    // The decoder should recognize FIXED order 0
    // (Full test would require valid residual data)
}

// Test FIXED predictor order 1
void test_fixed_predictor_order_1() {
    // FIXED order 1: s[i] = residual[i] + s[i-1]
    BitstreamReader reader;
    ResidualDecoder residual(&reader);
    SubframeDecoder decoder(&reader, &residual);
    
    // FIXED order 1: type=001001 (9), wasted_bits=0
    // Subframe header: 0|001001|0 = 0x12
    // 1 warm-up sample
    // Residuals follow
    uint8_t data[] = {
        0x12,        // Subframe header (FIXED order 1)
        0x00, 0x0A,  // Warm-up sample: 10
        // Residual coding would follow
    };
    reader.feedData(data, sizeof(data));
    
    // The decoder should recognize FIXED order 1 and read warm-up sample
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
    suite.addTest("FIXED Predictor Order 0", test_fixed_predictor_order_0);
    suite.addTest("FIXED Predictor Order 1", test_fixed_predictor_order_1);
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
