/*
 * test_bitstream_reader_unit.cpp - Unit tests for BitstreamReader
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include "test_framework.h"
#include "codecs/flac/BitstreamReader.h"
#include <cstring>

using namespace PsyMP3::Codec::FLAC;
using namespace TestFramework;

// Test bit reading accuracy
void test_bit_reading_accuracy() {
    BitstreamReader reader;
    
    // Feed test data: 0b10110011 0b11001010
    uint8_t data[] = {0xB3, 0xCA};
    reader.feedData(data, 2);
    
    // Read 4 bits: should get 0b1011 = 11
    uint32_t value;
    ASSERT_TRUE(reader.readBits(value, 4), "Read 4 bits");
    ASSERT_EQUALS(11u, value, "First 4 bits should be 11");
    
    // Read 3 bits: should get 0b001 = 1
    ASSERT_TRUE(reader.readBits(value, 3), "Read 3 bits");
    ASSERT_EQUALS(1u, value, "Next 3 bits should be 1");
    
    // Read 5 bits: should get 0b11100 = 28
    ASSERT_TRUE(reader.readBits(value, 5), "Read 5 bits");
    ASSERT_EQUALS(28u, value, "Next 5 bits should be 28");
    
    // Read 4 bits: should get 0b1010 = 10
    ASSERT_TRUE(reader.readBits(value, 4), "Read 4 bits");
    ASSERT_EQUALS(10u, value, "Last 4 bits should be 10");
    
    // No more data
    ASSERT_FALSE(reader.readBits(value, 1), "Should fail when no more data");
}

// Test signed bit reading
void test_signed_bit_reading() {
    BitstreamReader reader;
    
    // Feed test data with negative values
    uint8_t data[] = {0xFF, 0x80};  // -1 (8-bit), -128 (8-bit)
    reader.feedData(data, 2);
    
    // Read 8-bit signed: should get -1
    int32_t value;
    ASSERT_TRUE(reader.readBitsSigned(value, 8), "Read signed 8 bits");
    ASSERT_EQUALS(-1, value, "Should read -1");
    
    // Read 8-bit signed: should get -128
    ASSERT_TRUE(reader.readBitsSigned(value, 8), "Read signed 8 bits");
    ASSERT_EQUALS(-128, value, "Should read -128");
}

// Test unary decoding
void test_unary_decoding() {
    BitstreamReader reader;
    
    // Feed test data: 0b00001xxx (unary 4), 0b01xxxxxx (unary 1), 0b1xxxxxxx (unary 0)
    uint8_t data[] = {0x08, 0x40, 0x80};
    reader.feedData(data, 3);
    
    // Read unary: should get 4 (four zeros then a one)
    uint32_t value;
    ASSERT_TRUE(reader.readUnary(value), "Read unary value");
    ASSERT_EQUALS(4u, value, "Should read unary 4");
    
    // Skip remaining bits in first byte
    reader.alignToByte();
    
    // Read unary: should get 1 (one zero then a one)
    ASSERT_TRUE(reader.readUnary(value), "Read unary value");
    ASSERT_EQUALS(1u, value, "Should read unary 1");
    
    // Skip remaining bits
    reader.alignToByte();
    
    // Read unary: should get 0 (immediate one)
    ASSERT_TRUE(reader.readUnary(value), "Read unary value");
    ASSERT_EQUALS(0u, value, "Should read unary 0");
}

// Test UTF-8 number decoding (1-byte)
void test_utf8_1byte() {
    BitstreamReader reader;
    
    // 1-byte UTF-8: 0x00-0x7F
    uint8_t data[] = {0x00, 0x42, 0x7F};
    reader.feedData(data, 3);
    
    uint64_t value;
    ASSERT_TRUE(reader.readUTF8(value), "Read UTF-8 value");
    ASSERT_EQUALS(0u, value, "Should read 0");
    
    ASSERT_TRUE(reader.readUTF8(value), "Read UTF-8 value");
    ASSERT_EQUALS(0x42u, value, "Should read 0x42");
    
    ASSERT_TRUE(reader.readUTF8(value), "Read UTF-8 value");
    ASSERT_EQUALS(0x7Fu, value, "Should read 0x7F");
}

// Test UTF-8 number decoding (2-byte)
void test_utf8_2byte() {
    BitstreamReader reader;
    
    // 2-byte UTF-8: 0xC2 0x80 = 0x80
    uint8_t data[] = {0xC2, 0x80};
    reader.feedData(data, 2);
    
    uint64_t value;
    ASSERT_TRUE(reader.readUTF8(value), "Read UTF-8 value");
    ASSERT_EQUALS(0x80u, value, "Should read 0x80");
}

// Test UTF-8 number decoding (3-byte)
void test_utf8_3byte() {
    BitstreamReader reader;
    
    // 3-byte UTF-8: 0xE0 0xA0 0x80 = 0x800
    uint8_t data[] = {0xE0, 0xA0, 0x80};
    reader.feedData(data, 3);
    
    uint64_t value;
    ASSERT_TRUE(reader.readUTF8(value), "Read UTF-8 value");
    ASSERT_EQUALS(0x800u, value, "Should read 0x800");
}

// Test Rice code decoding
void test_rice_code_decoding() {
    BitstreamReader reader;
    
    // Rice code with parameter 3:
    // Byte 1: 0b10001001
    //   Code 1: 1|000 -> unary=0, remainder=000 -> folded=0 -> zigzag=0
    //   Code 2: 1|001 -> unary=0, remainder=001 -> folded=1 -> zigzag=-1
    // Byte 2: 0b01000000
    //   Code 3: 01|000 -> unary=1, remainder=000 -> folded=8 -> zigzag=4
    uint8_t data[] = {0b10001001, 0b01000000};
    reader.feedData(data, 2);
    
    int32_t value;
    
    // Decode with rice_param=3
    ASSERT_TRUE(reader.readRiceCode(value, 3), "Read Rice code");
    ASSERT_EQUALS(0, value, "Folded 0 -> zigzag 0");
    
    ASSERT_TRUE(reader.readRiceCode(value, 3), "Read Rice code");
    ASSERT_EQUALS(-1, value, "Folded 1 -> zigzag -1");
    
    ASSERT_TRUE(reader.readRiceCode(value, 3), "Read Rice code");
    ASSERT_EQUALS(4, value, "Folded 8 -> zigzag 4");
}

// Test buffer management
void test_buffer_management() {
    BitstreamReader reader;
    
    // Feed initial data
    uint8_t data1[] = {0xAA, 0xBB};
    reader.feedData(data1, 2);
    
    ASSERT_EQUALS(2u, reader.getAvailableBytes(), "Should have 2 bytes");
    ASSERT_EQUALS(16u, reader.getAvailableBits(), "Should have 16 bits");
    
    // Read some bits
    uint32_t value;
    ASSERT_TRUE(reader.readBits(value, 8), "Read 8 bits");
    ASSERT_EQUALS(0xAAu, value, "Should read 0xAA");
    
    // After reading 8 bits, we have 8 bits left (not a complete byte)
    ASSERT_EQUALS(8u, reader.getAvailableBits(), "Should have 8 bits left");
    
    // Feed more data
    uint8_t data2[] = {0xCC, 0xDD};
    reader.feedData(data2, 2);
    
    // Now we have 8 + 16 = 24 bits
    ASSERT_EQUALS(24u, reader.getAvailableBits(), "Should have 24 bits");
    
    // Clear buffer
    reader.clearBuffer();
    ASSERT_EQUALS(0u, reader.getAvailableBytes(), "Should have 0 bytes after clear");
    ASSERT_EQUALS(0u, reader.getAvailableBits(), "Should have 0 bits after clear");
}

// Test byte alignment
void test_byte_alignment() {
    BitstreamReader reader;
    
    uint8_t data[] = {0xAA, 0xBB, 0xCC};
    reader.feedData(data, 3);
    
    // Initially aligned
    ASSERT_TRUE(reader.isAligned(), "Should be initially aligned");
    
    // Read 3 bits - no longer aligned
    uint32_t value;
    ASSERT_TRUE(reader.readBits(value, 3), "Read 3 bits");
    ASSERT_FALSE(reader.isAligned(), "Should not be aligned after reading 3 bits");
    
    // Align to byte boundary
    ASSERT_TRUE(reader.alignToByte(), "Align to byte");
    ASSERT_TRUE(reader.isAligned(), "Should be aligned after alignToByte");
    
    // Next read should be from byte boundary
    ASSERT_TRUE(reader.readBits(value, 8), "Read 8 bits");
    ASSERT_EQUALS(0xBBu, value, "Should read 0xBB from byte boundary");
}

// Test position tracking
void test_position_tracking() {
    BitstreamReader reader;
    
    uint8_t data[] = {0xAA, 0xBB, 0xCC};
    reader.feedData(data, 3);
    
    ASSERT_EQUALS(0u, reader.getBitPosition(), "Initial bit position should be 0");
    ASSERT_EQUALS(0u, reader.getBytePosition(), "Initial byte position should be 0");
    
    // Read 12 bits
    uint32_t value;
    ASSERT_TRUE(reader.readBits(value, 12), "Read 12 bits");
    
    ASSERT_EQUALS(12u, reader.getBitPosition(), "Bit position should be 12");
    ASSERT_EQUALS(1u, reader.getBytePosition(), "Byte position should be 1");
    
    // Read 4 more bits
    ASSERT_TRUE(reader.readBits(value, 4), "Read 4 bits");
    
    ASSERT_EQUALS(16u, reader.getBitPosition(), "Bit position should be 16");
    ASSERT_EQUALS(2u, reader.getBytePosition(), "Byte position should be 2");
}

// Test edge case: reading 32 bits at once
void test_read_32_bits() {
    BitstreamReader reader;
    
    uint8_t data[] = {0x12, 0x34, 0x56, 0x78};
    reader.feedData(data, 4);
    
    uint32_t value;
    ASSERT_TRUE(reader.readBits(value, 32), "Read 32 bits");
    ASSERT_EQUALS(0x12345678u, value, "Should read 0x12345678");
}

// Test edge case: buffer underflow
void test_buffer_underflow() {
    BitstreamReader reader;
    
    uint8_t data[] = {0xAA};
    reader.feedData(data, 1);
    
    uint32_t value;
    // Try to read more bits than available
    ASSERT_FALSE(reader.readBits(value, 16), "Should fail when reading more bits than available");
}

// Test skip bits
void test_skip_bits() {
    BitstreamReader reader;
    
    uint8_t data[] = {0xAA, 0xBB, 0xCC};
    reader.feedData(data, 3);
    
    // Skip 12 bits
    ASSERT_TRUE(reader.skipBits(12), "Skip 12 bits");
    
    // Read next 8 bits - should be 0xBC (last 4 bits of 0xBB + first 4 bits of 0xCC)
    uint32_t value;
    ASSERT_TRUE(reader.readBits(value, 8), "Read 8 bits");
    ASSERT_EQUALS(0xBCu, value, "Should read 0xBC after skipping");
}

int main() {
    // Create test suite
    TestSuite suite("BitstreamReader Unit Tests");
    
    // Add test functions
    suite.addTest("Bit Reading Accuracy", test_bit_reading_accuracy);
    suite.addTest("Signed Bit Reading", test_signed_bit_reading);
    suite.addTest("Unary Decoding", test_unary_decoding);
    suite.addTest("UTF-8 1-byte", test_utf8_1byte);
    suite.addTest("UTF-8 2-byte", test_utf8_2byte);
    suite.addTest("UTF-8 3-byte", test_utf8_3byte);
    suite.addTest("Rice Code Decoding", test_rice_code_decoding);
    suite.addTest("Buffer Management", test_buffer_management);
    suite.addTest("Byte Alignment", test_byte_alignment);
    suite.addTest("Position Tracking", test_position_tracking);
    suite.addTest("Read 32 Bits", test_read_32_bits);
    suite.addTest("Buffer Underflow", test_buffer_underflow);
    suite.addTest("Skip Bits", test_skip_bits);
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print comprehensive results
    suite.printResults(results);
    
    // Return appropriate exit code
    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}
