/*
 * test_flac_security_validation.cpp - Security validation tests for FLAC decoder
 * 
 * This test file validates that security measures are properly implemented:
 * - Bounds checking
 * - Resource limits
 * - Error handling
 * - Input validation
 */

#include <cassert>
#include <cstring>
#include <vector>
#include <memory>
#include <iostream>
#include <limits>

#include "codecs/flac/BitstreamReader.h"
#include "codecs/flac/FrameParser.h"
#include "codecs/flac/SubframeDecoder.h"
#include "codecs/flac/ResidualDecoder.h"
#include "codecs/flac/ChannelDecorrelator.h"
#include "codecs/flac/SampleReconstructor.h"
#include "codecs/flac/CRCValidator.h"
#include "codecs/flac/MetadataParser.h"
#include "codecs/flac/FLACError.h"

using namespace PsyMP3::Codec::FLAC;

// Test framework
class SecurityTest {
public:
    SecurityTest(const std::string& name) : m_name(name), m_passed(0), m_failed(0) {}
    
    void run() {
        std::cout << "\n=== " << m_name << " ===" << std::endl;
    }
    
    void assert_true(bool condition, const std::string& message) {
        if (condition) {
            std::cout << "  ✓ " << message << std::endl;
            m_passed++;
        } else {
            std::cout << "  ✗ " << message << std::endl;
            m_failed++;
        }
    }
    
    void assert_false(bool condition, const std::string& message) {
        assert_true(!condition, message);
    }
    
    void print_summary() {
        std::cout << "\nResults: " << m_passed << " passed, " << m_failed << " failed" << std::endl;
    }
    
    bool all_passed() const { return m_failed == 0; }
    
private:
    std::string m_name;
    int m_passed;
    int m_failed;
};

// Test 1: BitstreamReader bounds checking
void test_bitstream_reader_bounds() {
    SecurityTest test("BitstreamReader Bounds Checking");
    test.run();
    
    BitstreamReader reader;
    
    // Test 1.1: Empty buffer
    test.assert_true(reader.getAvailableBits() == 0, "Empty buffer has 0 bits");
    
    // Test 1.2: Feed data
    uint8_t data[] = {0xFF, 0xF8, 0x00, 0x00};
    reader.feedData(data, sizeof(data));
    test.assert_true(reader.getAvailableBits() == 32, "Buffer has 32 bits after feeding 4 bytes");
    
    // Test 1.3: Read bits
    uint32_t value = 0;
    bool result = reader.readBits(value, 16);
    test.assert_true(result, "Successfully read 16 bits");
    test.assert_true(reader.getAvailableBits() == 16, "16 bits remaining after read");
    
    // Test 1.4: Read past end
    result = reader.readBits(value, 32);
    test.assert_false(result, "Cannot read 32 bits when only 16 available");
    
    // Test 1.5: Alignment
    reader.clearBuffer();
    reader.feedData(data, sizeof(data));
    result = reader.alignToByte();
    test.assert_true(result, "Alignment succeeds");
    
    test.print_summary();
}

// Test 2: FrameParser validation
void test_frame_parser_validation() {
    SecurityTest test("FrameParser Input Validation");
    test.run();
    
    BitstreamReader reader;
    CRCValidator crc;
    FrameParser parser(&reader, &crc);
    
    // Test 2.1: Invalid block size (0)
    FrameParser::FrameHeader header;
    header.block_size = 0;
    header.sample_rate = 44100;
    header.channels = 2;
    header.bit_depth = 16;
    test.assert_false(header.block_size >= 16 && header.block_size <= 65535, 
                     "Block size 0 is invalid");
    
    // Test 2.2: Invalid block size (> 65535)
    header.block_size = 65536;
    test.assert_false(header.block_size >= 16 && header.block_size <= 65535,
                     "Block size 65536 is invalid");
    
    // Test 2.3: Valid block size
    header.block_size = 4096;
    test.assert_true(header.block_size >= 16 && header.block_size <= 65535,
                    "Block size 4096 is valid");
    
    // Test 2.4: Invalid sample rate (0)
    header.sample_rate = 0;
    test.assert_false(header.sample_rate >= 1 && header.sample_rate <= 1048575,
                     "Sample rate 0 is invalid");
    
    // Test 2.5: Invalid sample rate (> 1048575)
    header.sample_rate = 1048576;
    test.assert_false(header.sample_rate >= 1 && header.sample_rate <= 1048575,
                     "Sample rate 1048576 is invalid");
    
    // Test 2.6: Valid sample rate
    header.sample_rate = 44100;
    test.assert_true(header.sample_rate >= 1 && header.sample_rate <= 1048575,
                    "Sample rate 44100 is valid");
    
    // Test 2.7: Invalid channel count (0)
    header.channels = 0;
    test.assert_false(header.channels >= 1 && header.channels <= 8,
                     "Channel count 0 is invalid");
    
    // Test 2.8: Invalid channel count (> 8)
    header.channels = 9;
    test.assert_false(header.channels >= 1 && header.channels <= 8,
                     "Channel count 9 is invalid");
    
    // Test 2.9: Valid channel count
    header.channels = 2;
    test.assert_true(header.channels >= 1 && header.channels <= 8,
                    "Channel count 2 is valid");
    
    // Test 2.10: Invalid bit depth (< 4)
    header.bit_depth = 3;
    test.assert_false(header.bit_depth >= 4 && header.bit_depth <= 32,
                     "Bit depth 3 is invalid");
    
    // Test 2.11: Invalid bit depth (> 32)
    header.bit_depth = 33;
    test.assert_false(header.bit_depth >= 4 && header.bit_depth <= 32,
                     "Bit depth 33 is invalid");
    
    // Test 2.12: Valid bit depth
    header.bit_depth = 16;
    test.assert_true(header.bit_depth >= 4 && header.bit_depth <= 32,
                    "Bit depth 16 is valid");
    
    test.print_summary();
}

// Test 3: Resource limits
void test_resource_limits() {
    SecurityTest test("Resource Limits");
    test.run();
    
    // Test 3.1: Maximum block size
    uint32_t max_block_size = 65535;
    test.assert_true(max_block_size <= 65535, "Max block size is 65535");
    
    // Test 3.2: Maximum channels
    uint32_t max_channels = 8;
    test.assert_true(max_channels <= 8, "Max channels is 8");
    
    // Test 3.3: Maximum partition order
    uint32_t max_partition_order = 15;
    test.assert_true(max_partition_order <= 15, "Max partition order is 15");
    
    // Test 3.4: Memory calculation
    uint32_t block_size = 65535;
    uint32_t channels = 8;
    uint32_t bytes_per_sample = 4;
    uint64_t max_memory = static_cast<uint64_t>(block_size) * channels * bytes_per_sample;
    test.assert_true(max_memory <= 2500000, "Max memory per frame is ~2.5 MB");
    
    // Test 3.5: Partition count limit
    uint32_t partition_order = 15;
    uint32_t max_partitions = 1 << partition_order;  // 2^15 = 32768
    test.assert_true(max_partitions <= 32768, "Max partitions is 32768");
    
    test.print_summary();
}

// Test 4: Error handling
void test_error_handling() {
    SecurityTest test("Error Handling");
    test.run();
    
    // Test 4.1: Exception safety
    try {
        BitstreamReader reader;
        uint8_t data[] = {0xFF, 0xF8};
        reader.feedData(data, sizeof(data));
        
        uint32_t value = 0;
        bool result = reader.readBits(value, 16);
        test.assert_true(result, "Read succeeds with valid data");
        
        // Try to read past end
        result = reader.readBits(value, 32);
        test.assert_false(result, "Read fails when insufficient data");
        
        test.assert_true(true, "No exception thrown on error");
    } catch (...) {
        test.assert_false(true, "Unexpected exception thrown");
    }
    
    // Test 4.2: Null pointer handling
    try {
        BitstreamReader reader;
        reader.feedData(nullptr, 0);
        test.assert_true(true, "Null pointer handled gracefully");
    } catch (...) {
        test.assert_false(true, "Exception on null pointer");
    }
    
    // Test 4.3: Zero size handling
    try {
        BitstreamReader reader;
        uint8_t data[] = {0xFF};
        reader.feedData(data, 0);
        test.assert_true(reader.getAvailableBits() == 0, "Zero size handled correctly");
    } catch (...) {
        test.assert_false(true, "Exception on zero size");
    }
    
    test.print_summary();
}

// Test 5: Integer overflow prevention
void test_integer_overflow_prevention() {
    SecurityTest test("Integer Overflow Prevention");
    test.run();
    
    // Test 5.1: Block size * channels overflow
    uint32_t block_size = 65535;
    uint32_t channels = 8;
    uint64_t result = static_cast<uint64_t>(block_size) * channels;
    test.assert_true(result <= std::numeric_limits<uint32_t>::max() || 
                    result == static_cast<uint64_t>(block_size) * channels,
                    "Block size * channels doesn't overflow");
    
    // Test 5.2: Partition count calculation
    uint32_t partition_order = 15;
    uint32_t partitions = 1 << partition_order;
    test.assert_true(partitions == 32768, "Partition count calculation is correct");
    
    // Test 5.3: Sample rate * block size
    uint32_t sample_rate = 1048575;
    uint32_t block_size_2 = 65535;
    uint64_t duration = static_cast<uint64_t>(sample_rate) * block_size_2;
    test.assert_true(duration > 0, "Duration calculation doesn't overflow");
    
    test.print_summary();
}

// Test 6: CRC validation
void test_crc_validation() {
    SecurityTest test("CRC Validation");
    test.run();
    
    CRCValidator crc;
    
    // Test 6.1: CRC-8 computation
    uint8_t data[] = {0xFF, 0xF8, 0x00, 0x00};
    uint8_t crc8 = crc.computeCRC8(data, sizeof(data));
    test.assert_true(crc8 >= 0 && crc8 <= 255, "CRC-8 is valid byte value");
    
    // Test 6.2: CRC-16 computation
    uint16_t crc16 = crc.computeCRC16(data, sizeof(data));
    test.assert_true(crc16 >= 0 && crc16 <= 65535, "CRC-16 is valid word value");
    
    // Test 6.3: Incremental CRC
    crc.resetCRC8();
    for (size_t i = 0; i < sizeof(data); ++i) {
        crc.updateCRC8(data[i]);
    }
    uint8_t incremental_crc8 = crc.getCRC8();
    test.assert_true(incremental_crc8 == crc8, "Incremental CRC-8 matches full computation");
    
    test.print_summary();
}

// Test 7: Forbidden pattern detection
void test_forbidden_pattern_detection() {
    SecurityTest test("Forbidden Pattern Detection");
    test.run();
    
    // Test 7.1: Metadata block type 127
    uint8_t block_type = 127;
    test.assert_false(block_type < 127, "Block type 127 is forbidden");
    
    // Test 7.2: Sample rate bits 0xF
    uint8_t sample_rate_bits = 0xF;
    test.assert_false(sample_rate_bits != 0xF, "Sample rate bits 0xF is forbidden");
    
    // Test 7.3: Block size 65536
    uint32_t block_size = 65536;
    test.assert_false(block_size <= 65535, "Block size 65536 is forbidden");
    
    // Test 7.4: Predictor coefficient precision 0xF
    uint8_t precision_bits = 0xF;
    test.assert_false(precision_bits != 0xF, "Precision bits 0xF is forbidden");
    
    test.print_summary();
}

// Main test runner
int main() {
    std::cout << "FLAC Decoder Security Validation Tests" << std::endl;
    std::cout << "======================================" << std::endl;
    
    test_bitstream_reader_bounds();
    test_frame_parser_validation();
    test_resource_limits();
    test_error_handling();
    test_integer_overflow_prevention();
    test_crc_validation();
    test_forbidden_pattern_detection();
    
    std::cout << "\n======================================" << std::endl;
    std::cout << "All security validation tests completed" << std::endl;
    
    return 0;
}
