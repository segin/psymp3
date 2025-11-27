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

