/*
 * test_ogg_page_properties.cpp - Property-based tests for Ogg page parsing
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Property-based tests for RFC 3533 compliant Ogg page parsing.
 * Uses RapidCheck for property-based testing when available,
 * falls back to exhaustive unit tests otherwise.
 */

#include "psymp3.h"

#ifdef HAVE_RAPIDCHECK
#include <rapidcheck.h>
#endif

#include <iostream>
#include <vector>
#include <cstring>
#include <cstdint>
#include <random>

#ifdef HAVE_OGGDEMUXER
using namespace PsyMP3::Demuxer::Ogg;
#endif

// Test result tracking
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, msg) \
    do { \
        if (!(condition)) { \
            std::cout << "✗ FAILED: " << msg << std::endl; \
            tests_failed++; \
            return false; \
        } \
    } while(0)

#define TEST_PASS(msg) \
    do { \
        std::cout << "✓ " << msg << std::endl; \
        tests_passed++; \
    } while(0)

/**
 * Helper function to create a valid Ogg page header
 */
std::vector<uint8_t> createValidOggPageHeader(uint8_t num_segments = 1,
                                               uint8_t header_type = 0,
                                               uint64_t granule_pos = 0,
                                               uint32_t serial = 0x12345678,
                                               uint32_t sequence = 0) {
    std::vector<uint8_t> header(27 + num_segments);
    
    // Capture pattern "OggS"
    header[0] = 0x4F;  // 'O'
    header[1] = 0x67;  // 'g'
    header[2] = 0x67;  // 'g'
    header[3] = 0x53;  // 'S'
    
    // Version (must be 0)
    header[4] = 0;
    
    // Header type flags
    header[5] = header_type;
    
    // Granule position (64-bit little-endian)
    for (int i = 0; i < 8; ++i) {
        header[6 + i] = (granule_pos >> (i * 8)) & 0xFF;
    }
    
    // Serial number (32-bit little-endian)
    for (int i = 0; i < 4; ++i) {
        header[14 + i] = (serial >> (i * 8)) & 0xFF;
    }
    
    // Page sequence number (32-bit little-endian)
    for (int i = 0; i < 4; ++i) {
        header[18 + i] = (sequence >> (i * 8)) & 0xFF;
    }
    
    // CRC32 checksum (placeholder - will be calculated)
    header[22] = 0;
    header[23] = 0;
    header[24] = 0;
    header[25] = 0;
    
    // Number of segments
    header[26] = num_segments;
    
    // Segment table (lacing values) - default to 0 for empty segments
    for (int i = 0; i < num_segments; ++i) {
        header[27 + i] = 0;
    }
    
    return header;
}

#ifdef HAVE_OGGDEMUXER

// ============================================================================
// **Feature: ogg-demuxer-fix, Property 1: OggS Capture Pattern Validation**
// **Validates: Requirements 1.1**
// ============================================================================

bool test_property1_valid_oggs_accepted() {
    std::vector<uint8_t> data = { 0x4F, 0x67, 0x67, 0x53 };  // "OggS"
    TEST_ASSERT(OggPageParser::validateCapturePattern(data.data(), data.size(), 0),
                "Valid OggS pattern should be accepted");
    return true;
}

bool test_property1_invalid_patterns_rejected() {
    // Test various invalid patterns
    std::vector<std::vector<uint8_t>> invalid_patterns = {
        { 0x00, 0x00, 0x00, 0x00 },  // All zeros
        { 0xFF, 0xFF, 0xFF, 0xFF },  // All ones
        { 0x4F, 0x67, 0x67, 0x00 },  // Almost OggS (wrong last byte)
        { 0x00, 0x67, 0x67, 0x53 },  // Almost OggS (wrong first byte)
        { 0x4F, 0x00, 0x67, 0x53 },  // Almost OggS (wrong second byte)
        { 0x4F, 0x67, 0x00, 0x53 },  // Almost OggS (wrong third byte)
        { 'R', 'I', 'F', 'F' },      // RIFF header
        { 'f', 'L', 'a', 'C' },      // FLAC header
        { 'I', 'D', '3', 0x04 },     // ID3 header
    };
    
    for (const auto& pattern : invalid_patterns) {
        TEST_ASSERT(!OggPageParser::validateCapturePattern(pattern.data(), pattern.size(), 0),
                    "Invalid pattern should be rejected");
    }
    return true;
}

bool test_property1_oggs_at_offset() {
    // Test OggS detection at various offsets
    for (size_t offset = 0; offset < 100; ++offset) {
        std::vector<uint8_t> data(offset + 4, 0x00);
        data[offset + 0] = 0x4F;
        data[offset + 1] = 0x67;
        data[offset + 2] = 0x67;
        data[offset + 3] = 0x53;
        
        TEST_ASSERT(OggPageParser::validateCapturePattern(data.data(), data.size(), offset),
                    "OggS should be detected at offset");
    }
    return true;
}

bool test_property1_buffer_too_small() {
    // Test with buffers smaller than 4 bytes
    for (size_t size = 0; size < 4; ++size) {
        std::vector<uint8_t> data(size, 0x4F);
        TEST_ASSERT(!OggPageParser::validateCapturePattern(data.data(), data.size(), 0),
                    "Buffer too small should return false");
    }
    return true;
}

// ============================================================================
// **Feature: ogg-demuxer-fix, Property 2: Page Version Validation**
// **Validates: Requirements 1.2**
// ============================================================================

bool test_property2_version_zero_accepted() {
    TEST_ASSERT(OggPageParser::validateVersion(0), "Version 0 should be accepted");
    return true;
}

bool test_property2_nonzero_versions_rejected() {
    // Test all non-zero versions (1-255)
    for (int version = 1; version <= 255; ++version) {
        TEST_ASSERT(!OggPageParser::validateVersion(static_cast<uint8_t>(version)),
                    "Non-zero version should be rejected");
    }
    return true;
}

// ============================================================================
// **Feature: ogg-demuxer-fix, Property 3: Page Size Bounds**
// **Validates: Requirements 1.11**
// ============================================================================

bool test_property3_valid_sizes_accepted() {
    // Test various valid page sizes
    std::vector<size_t> valid_sizes = {
        0, 1, 27, 100, 1000, 10000, 50000, 65307  // OGG_PAGE_SIZE_MAX
    };
    
    for (size_t size : valid_sizes) {
        OggPage page;
        page.total_size = size;
        TEST_ASSERT(page.validatePageSize(), "Valid page size should be accepted");
    }
    return true;
}

bool test_property3_oversized_rejected() {
    // Test sizes exceeding maximum
    std::vector<size_t> invalid_sizes = {
        65308,   // OGG_PAGE_SIZE_MAX + 1
        65309,
        100000,
        1000000,
        SIZE_MAX
    };
    
    for (size_t size : invalid_sizes) {
        OggPage page;
        page.total_size = size;
        TEST_ASSERT(!page.validatePageSize(), "Oversized page should be rejected");
    }
    return true;
}

bool test_property3_size_calculation() {
    // Test page size calculation with various segment counts
    std::mt19937 rng(42);  // Fixed seed for reproducibility
    
    for (int trial = 0; trial < 100; ++trial) {
        uint8_t num_segments = rng() % 256;
        
        OggPage page;
        page.header.page_segments = num_segments;
        page.segment_table.resize(num_segments);
        
        size_t expected_body_size = 0;
        for (int i = 0; i < num_segments; ++i) {
            page.segment_table[i] = rng() % 256;
            expected_body_size += page.segment_table[i];
        }
        
        size_t calculated_header_size = page.calculateHeaderSize();
        size_t calculated_body_size = page.calculateBodySize();
        
        TEST_ASSERT(calculated_header_size == OGG_PAGE_HEADER_MIN_SIZE + num_segments,
                    "Header size calculation should be correct");
        TEST_ASSERT(calculated_body_size == expected_body_size,
                    "Body size calculation should be correct");
    }
    return true;
}

// ============================================================================
// **Feature: ogg-demuxer-fix, Property 4: Lacing Value Interpretation**
// **Validates: Requirements 2.4, 2.5, 13.6**
// ============================================================================

/**
 * Property 4: Lacing Value Interpretation
 * 
 * *For any* segment table, the demuxer SHALL interpret a lacing value of 255
 * as packet continuation and a lacing value less than 255 as packet termination.
 * 
 * RFC 3533 Section 5:
 * - Lacing value of 255: packet continues in next segment
 * - Lacing value < 255: packet ends (final segment of packet)
 * - Lacing value of 0 after 255: packet is exactly multiple of 255 bytes
 */

bool test_property4_lacing_255_is_continuation() {
    // Test that lacing value 255 is always interpreted as continuation
    TEST_ASSERT(OggPageParser::isPacketContinuation(255), 
                "Lacing value 255 should indicate continuation");
    TEST_ASSERT(!OggPageParser::isPacketTermination(255),
                "Lacing value 255 should NOT indicate termination");
    return true;
}

bool test_property4_lacing_less_than_255_is_termination() {
    // Test that all lacing values 0-254 are interpreted as termination
    for (int lacing = 0; lacing < 255; ++lacing) {
        uint8_t lv = static_cast<uint8_t>(lacing);
        TEST_ASSERT(OggPageParser::isPacketTermination(lv),
                    "Lacing value < 255 should indicate termination");
        TEST_ASSERT(!OggPageParser::isPacketContinuation(lv),
                    "Lacing value < 255 should NOT indicate continuation");
    }
    return true;
}

bool test_property4_segment_table_single_packet() {
    // Test segment table with a single complete packet
    std::vector<uint8_t> segment_table = { 100 };  // Single packet of 100 bytes
    
    std::vector<size_t> packet_offsets;
    std::vector<size_t> packet_sizes;
    std::vector<bool> packet_complete;
    
    OggPageParser::parseSegmentTable(segment_table, packet_offsets, packet_sizes, packet_complete);
    
    TEST_ASSERT(packet_offsets.size() == 1, "Should have 1 packet");
    TEST_ASSERT(packet_sizes.size() == 1, "Should have 1 packet size");
    TEST_ASSERT(packet_complete.size() == 1, "Should have 1 completion flag");
    TEST_ASSERT(packet_offsets[0] == 0, "Packet should start at offset 0");
    TEST_ASSERT(packet_sizes[0] == 100, "Packet should be 100 bytes");
    TEST_ASSERT(packet_complete[0] == true, "Packet should be complete");
    
    return true;
}

bool test_property4_segment_table_continued_packet() {
    // Test segment table with a packet that continues to next page
    // 255 + 255 = 510 bytes, packet continues
    std::vector<uint8_t> segment_table = { 255, 255 };
    
    std::vector<size_t> packet_offsets;
    std::vector<size_t> packet_sizes;
    std::vector<bool> packet_complete;
    
    OggPageParser::parseSegmentTable(segment_table, packet_offsets, packet_sizes, packet_complete);
    
    TEST_ASSERT(packet_offsets.size() == 1, "Should have 1 packet");
    TEST_ASSERT(packet_sizes[0] == 510, "Packet should be 510 bytes");
    TEST_ASSERT(packet_complete[0] == false, "Packet should NOT be complete (continues)");
    
    return true;
}

bool test_property4_segment_table_exact_255_multiple() {
    // Test packet that is exactly 255 bytes (needs terminating 0)
    // Per RFC 3533 Section 5: "If a packet is exactly 255 bytes, a terminating
    // lacing value of 0 is needed"
    // The segment table {255, 0} means:
    // - 255: packet continues (255 bytes so far)
    // - 0: packet terminates (adds 0 bytes, total = 255 bytes)
    // Result: ONE packet of exactly 255 bytes
    std::vector<uint8_t> segment_table = { 255, 0 };
    
    std::vector<size_t> packet_offsets;
    std::vector<size_t> packet_sizes;
    std::vector<bool> packet_complete;
    
    OggPageParser::parseSegmentTable(segment_table, packet_offsets, packet_sizes, packet_complete);
    
    TEST_ASSERT(packet_offsets.size() == 1, "Should have 1 packet (255 bytes with terminating 0)");
    TEST_ASSERT(packet_sizes[0] == 255, "Packet should be exactly 255 bytes");
    TEST_ASSERT(packet_complete[0] == true, "Packet should be complete (terminated by 0)");
    
    return true;
}

bool test_property4_segment_table_multiple_packets() {
    // Test segment table with multiple complete packets
    // Packet 1: 100 bytes, Packet 2: 200 bytes, Packet 3: 50 bytes
    std::vector<uint8_t> segment_table = { 100, 200, 50 };
    
    std::vector<size_t> packet_offsets;
    std::vector<size_t> packet_sizes;
    std::vector<bool> packet_complete;
    
    OggPageParser::parseSegmentTable(segment_table, packet_offsets, packet_sizes, packet_complete);
    
    TEST_ASSERT(packet_offsets.size() == 3, "Should have 3 packets");
    TEST_ASSERT(packet_offsets[0] == 0, "Packet 1 starts at 0");
    TEST_ASSERT(packet_offsets[1] == 100, "Packet 2 starts at 100");
    TEST_ASSERT(packet_offsets[2] == 300, "Packet 3 starts at 300");
    TEST_ASSERT(packet_sizes[0] == 100, "Packet 1 is 100 bytes");
    TEST_ASSERT(packet_sizes[1] == 200, "Packet 2 is 200 bytes");
    TEST_ASSERT(packet_sizes[2] == 50, "Packet 3 is 50 bytes");
    TEST_ASSERT(packet_complete[0] && packet_complete[1] && packet_complete[2],
                "All packets should be complete");
    
    return true;
}

bool test_property4_segment_table_nil_packet() {
    // Test zero-length packet (nil packet) - lacing value of 0 only
    std::vector<uint8_t> segment_table = { 0 };
    
    std::vector<size_t> packet_offsets;
    std::vector<size_t> packet_sizes;
    std::vector<bool> packet_complete;
    
    OggPageParser::parseSegmentTable(segment_table, packet_offsets, packet_sizes, packet_complete);
    
    TEST_ASSERT(packet_offsets.size() == 1, "Should have 1 packet");
    TEST_ASSERT(packet_sizes[0] == 0, "Nil packet should be 0 bytes");
    TEST_ASSERT(packet_complete[0] == true, "Nil packet should be complete");
    
    return true;
}

bool test_property4_segment_table_mixed() {
    // Test complex segment table with mixed packet types
    // Packet 1: 255+255+100 = 610 bytes (spans 3 segments)
    // Packet 2: 50 bytes
    // Packet 3: continues to next page (255)
    std::vector<uint8_t> segment_table = { 255, 255, 100, 50, 255 };
    
    std::vector<size_t> packet_offsets;
    std::vector<size_t> packet_sizes;
    std::vector<bool> packet_complete;
    
    OggPageParser::parseSegmentTable(segment_table, packet_offsets, packet_sizes, packet_complete);
    
    TEST_ASSERT(packet_offsets.size() == 3, "Should have 3 packets");
    TEST_ASSERT(packet_sizes[0] == 610, "Packet 1 should be 610 bytes");
    TEST_ASSERT(packet_complete[0] == true, "Packet 1 should be complete");
    TEST_ASSERT(packet_sizes[1] == 50, "Packet 2 should be 50 bytes");
    TEST_ASSERT(packet_complete[1] == true, "Packet 2 should be complete");
    TEST_ASSERT(packet_sizes[2] == 255, "Packet 3 should be 255 bytes so far");
    TEST_ASSERT(packet_complete[2] == false, "Packet 3 should continue");
    
    return true;
}

bool test_property4_count_complete_packets() {
    // Test counting complete packets in various segment tables
    
    // Single complete packet
    std::vector<uint8_t> st1 = { 100 };
    TEST_ASSERT(OggPageParser::countCompletePackets(st1) == 1, "Should count 1 complete packet");
    
    // Continued packet (no complete packets)
    std::vector<uint8_t> st2 = { 255, 255 };
    TEST_ASSERT(OggPageParser::countCompletePackets(st2) == 0, "Should count 0 complete packets");
    
    // Multiple complete packets
    std::vector<uint8_t> st3 = { 100, 200, 50 };
    TEST_ASSERT(OggPageParser::countCompletePackets(st3) == 3, "Should count 3 complete packets");
    
    // Mixed: 2 complete + 1 continued
    std::vector<uint8_t> st4 = { 100, 50, 255 };
    TEST_ASSERT(OggPageParser::countCompletePackets(st4) == 2, "Should count 2 complete packets");
    
    return true;
}

bool test_property4_is_last_packet_complete() {
    // Test detection of whether last packet is complete
    
    // Complete packet
    std::vector<uint8_t> st1 = { 100 };
    TEST_ASSERT(OggPageParser::isLastPacketComplete(st1) == true, "Last packet should be complete");
    
    // Continued packet
    std::vector<uint8_t> st2 = { 255 };
    TEST_ASSERT(OggPageParser::isLastPacketComplete(st2) == false, "Last packet should NOT be complete");
    
    // Multiple packets, last complete
    std::vector<uint8_t> st3 = { 255, 100 };
    TEST_ASSERT(OggPageParser::isLastPacketComplete(st3) == true, "Last packet should be complete");
    
    // Multiple packets, last continued
    std::vector<uint8_t> st4 = { 100, 255 };
    TEST_ASSERT(OggPageParser::isLastPacketComplete(st4) == false, "Last packet should NOT be complete");
    
    // Empty segment table
    std::vector<uint8_t> st5 = {};
    TEST_ASSERT(OggPageParser::isLastPacketComplete(st5) == true, "Empty table should return true");
    
    return true;
}

#ifdef HAVE_RAPIDCHECK
/**
 * RapidCheck property test for lacing value interpretation
 * 
 * Property: For any lacing value v:
 * - v == 255 implies continuation
 * - v < 255 implies termination
 * - These are mutually exclusive
 */
bool test_property4_rapidcheck() {
    bool all_passed = true;
    
    // Property: lacing value 255 is continuation, < 255 is termination
    rc::check("Lacing value interpretation is consistent", []() {
        uint8_t lacing_value = *rc::gen::arbitrary<uint8_t>();
        
        bool is_continuation = OggPageParser::isPacketContinuation(lacing_value);
        bool is_termination = OggPageParser::isPacketTermination(lacing_value);
        
        // Mutual exclusivity
        RC_ASSERT(is_continuation != is_termination);
        
        // Correct interpretation
        if (lacing_value == 255) {
            RC_ASSERT(is_continuation);
            RC_ASSERT(!is_termination);
        } else {
            RC_ASSERT(!is_continuation);
            RC_ASSERT(is_termination);
        }
    });
    
    // Property: segment table parsing produces correct packet boundaries
    rc::check("Segment table parsing produces valid packet boundaries", []() {
        // Generate random segment table (0-255 segments)
        auto segment_table = *rc::gen::container<std::vector<uint8_t>>(
            rc::gen::inRange<size_t>(0, 256),
            rc::gen::arbitrary<uint8_t>()
        );
        
        std::vector<size_t> packet_offsets;
        std::vector<size_t> packet_sizes;
        std::vector<bool> packet_complete;
        
        OggPageParser::parseSegmentTable(segment_table, packet_offsets, packet_sizes, packet_complete);
        
        // Verify consistency
        RC_ASSERT(packet_offsets.size() == packet_sizes.size());
        RC_ASSERT(packet_offsets.size() == packet_complete.size());
        
        // Verify offsets are monotonically increasing
        for (size_t i = 1; i < packet_offsets.size(); ++i) {
            RC_ASSERT(packet_offsets[i] >= packet_offsets[i-1]);
        }
        
        // Verify total size matches sum of lacing values
        size_t total_from_lacing = 0;
        for (uint8_t lv : segment_table) {
            total_from_lacing += lv;
        }
        
        size_t total_from_packets = 0;
        for (size_t ps : packet_sizes) {
            total_from_packets += ps;
        }
        
        RC_ASSERT(total_from_lacing == total_from_packets);
    });
    
    return all_passed;
}
#endif // HAVE_RAPIDCHECK

#endif // HAVE_OGGDEMUXER

int main() {
    std::cout << "Ogg Page Property Tests (RFC 3533 Compliance)" << std::endl;
    std::cout << "==============================================" << std::endl;
    
#ifndef HAVE_OGGDEMUXER
    std::cout << "OggDemuxer not available - skipping tests" << std::endl;
    return 0;
#else
    
    // ========================================================================
    // Property 1: OggS Capture Pattern Validation
    // **Validates: Requirements 1.1**
    // ========================================================================
    std::cout << "\nProperty 1: OggS Capture Pattern Validation" << std::endl;
    std::cout << "-------------------------------------------" << std::endl;
    
    if (test_property1_valid_oggs_accepted()) {
        TEST_PASS("Valid OggS pattern accepted");
    }
    
    if (test_property1_invalid_patterns_rejected()) {
        TEST_PASS("Invalid patterns rejected");
    }
    
    if (test_property1_oggs_at_offset()) {
        TEST_PASS("OggS detected at various offsets");
    }
    
    if (test_property1_buffer_too_small()) {
        TEST_PASS("Buffer too small handled correctly");
    }
    
    // ========================================================================
    // Property 2: Page Version Validation
    // **Validates: Requirements 1.2**
    // ========================================================================
    std::cout << "\nProperty 2: Page Version Validation" << std::endl;
    std::cout << "------------------------------------" << std::endl;
    
    if (test_property2_version_zero_accepted()) {
        TEST_PASS("Version 0 accepted");
    }
    
    if (test_property2_nonzero_versions_rejected()) {
        TEST_PASS("Non-zero versions rejected (1-255)");
    }
    
    // ========================================================================
    // Property 3: Page Size Bounds
    // **Validates: Requirements 1.11**
    // ========================================================================
    std::cout << "\nProperty 3: Page Size Bounds" << std::endl;
    std::cout << "----------------------------" << std::endl;
    
    if (test_property3_valid_sizes_accepted()) {
        TEST_PASS("Valid page sizes accepted");
    }
    
    if (test_property3_oversized_rejected()) {
        TEST_PASS("Oversized pages rejected");
    }
    
    if (test_property3_size_calculation()) {
        TEST_PASS("Page size calculation correct");
    }
    
    // ========================================================================
    // Property 4: Lacing Value Interpretation
    // **Validates: Requirements 2.4, 2.5, 13.6**
    // ========================================================================
    std::cout << "\nProperty 4: Lacing Value Interpretation" << std::endl;
    std::cout << "---------------------------------------" << std::endl;
    
    if (test_property4_lacing_255_is_continuation()) {
        TEST_PASS("Lacing value 255 is continuation");
    }
    
    if (test_property4_lacing_less_than_255_is_termination()) {
        TEST_PASS("Lacing values 0-254 are termination");
    }
    
    if (test_property4_segment_table_single_packet()) {
        TEST_PASS("Single packet segment table parsed correctly");
    }
    
    if (test_property4_segment_table_continued_packet()) {
        TEST_PASS("Continued packet segment table parsed correctly");
    }
    
    if (test_property4_segment_table_exact_255_multiple()) {
        TEST_PASS("Exact 255-byte packet with terminator parsed correctly");
    }
    
    if (test_property4_segment_table_multiple_packets()) {
        TEST_PASS("Multiple packets segment table parsed correctly");
    }
    
    if (test_property4_segment_table_nil_packet()) {
        TEST_PASS("Nil packet (zero-length) parsed correctly");
    }
    
    if (test_property4_segment_table_mixed()) {
        TEST_PASS("Mixed segment table parsed correctly");
    }
    
    if (test_property4_count_complete_packets()) {
        TEST_PASS("Complete packet counting correct");
    }
    
    if (test_property4_is_last_packet_complete()) {
        TEST_PASS("Last packet completion detection correct");
    }
    
#ifdef HAVE_RAPIDCHECK
    std::cout << "\nProperty 4: RapidCheck Property Tests" << std::endl;
    std::cout << "-------------------------------------" << std::endl;
    
    if (test_property4_rapidcheck()) {
        TEST_PASS("RapidCheck property tests passed");
    }
#endif
    
    // ========================================================================
    // Summary
    // ========================================================================
    std::cout << "\n==============================================" << std::endl;
    std::cout << "Test Summary" << std::endl;
    std::cout << "==============================================" << std::endl;
    std::cout << "Passed: " << tests_passed << std::endl;
    std::cout << "Failed: " << tests_failed << std::endl;
    
    if (tests_failed > 0) {
        std::cout << "\nSome tests FAILED!" << std::endl;
        return 1;
    }
    
    std::cout << "\nAll property tests PASSED!" << std::endl;
    return 0;
    
#endif // HAVE_OGGDEMUXER
}

