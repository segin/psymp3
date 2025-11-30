/*
 * test_flac_seektable_placeholder_properties.cpp - Property-based tests for FLAC SEEKTABLE placeholder detection
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <random>
#include <memory>
#include <sstream>
#include <iomanip>

// ========================================
// STANDALONE SEEK POINT STRUCTURE
// ========================================

/**
 * RFC 9639 Section 8.5: FLAC seek point structure
 * Each seek point is exactly 18 bytes:
 * - u64 sample_number (big-endian)
 * - u64 stream_offset (big-endian)
 * - u16 frame_samples (big-endian)
 * 
 * A placeholder seek point has sample_number = 0xFFFFFFFFFFFFFFFF
 */
struct FLACSeekPoint {
    uint64_t sample_number = 0;      ///< Sample number of first sample in target frame
    uint64_t stream_offset = 0;      ///< Offset from first frame header to target frame
    uint16_t frame_samples = 0;      ///< Number of samples in target frame
    
    FLACSeekPoint() = default;
    
    FLACSeekPoint(uint64_t sample, uint64_t offset, uint16_t samples)
        : sample_number(sample), stream_offset(offset), frame_samples(samples) {}
    
    /**
     * RFC 9639 Section 8.5: "A placeholder point MUST have the sample number value 
     * 0xFFFFFFFFFFFFFFFF"
     */
    bool isPlaceholder() const {
        return sample_number == 0xFFFFFFFFFFFFFFFFULL;
    }
    
    bool isValid() const {
        return !isPlaceholder() && frame_samples > 0;
    }
};

/**
 * Parse a seek point from 18 bytes of big-endian data
 * @param data Pointer to 18 bytes of seek point data
 * @return Parsed FLACSeekPoint structure
 */
FLACSeekPoint parseSeekPoint(const uint8_t* data) {
    FLACSeekPoint point;
    
    // u64 sample number (big-endian)
    point.sample_number = (static_cast<uint64_t>(data[0]) << 56) |
                          (static_cast<uint64_t>(data[1]) << 48) |
                          (static_cast<uint64_t>(data[2]) << 40) |
                          (static_cast<uint64_t>(data[3]) << 32) |
                          (static_cast<uint64_t>(data[4]) << 24) |
                          (static_cast<uint64_t>(data[5]) << 16) |
                          (static_cast<uint64_t>(data[6]) << 8) |
                          data[7];
    
    // u64 stream offset (big-endian)
    point.stream_offset = (static_cast<uint64_t>(data[8]) << 56) |
                          (static_cast<uint64_t>(data[9]) << 48) |
                          (static_cast<uint64_t>(data[10]) << 40) |
                          (static_cast<uint64_t>(data[11]) << 32) |
                          (static_cast<uint64_t>(data[12]) << 24) |
                          (static_cast<uint64_t>(data[13]) << 16) |
                          (static_cast<uint64_t>(data[14]) << 8) |
                          data[15];
    
    // u16 frame samples (big-endian)
    point.frame_samples = (static_cast<uint16_t>(data[16]) << 8) | data[17];
    
    return point;
}

/**
 * Serialize a seek point to 18 bytes of big-endian data
 * @param point The seek point to serialize
 * @param data Output buffer (must be at least 18 bytes)
 */
void serializeSeekPoint(const FLACSeekPoint& point, uint8_t* data) {
    // u64 sample number (big-endian)
    data[0] = static_cast<uint8_t>((point.sample_number >> 56) & 0xFF);
    data[1] = static_cast<uint8_t>((point.sample_number >> 48) & 0xFF);
    data[2] = static_cast<uint8_t>((point.sample_number >> 40) & 0xFF);
    data[3] = static_cast<uint8_t>((point.sample_number >> 32) & 0xFF);
    data[4] = static_cast<uint8_t>((point.sample_number >> 24) & 0xFF);
    data[5] = static_cast<uint8_t>((point.sample_number >> 16) & 0xFF);
    data[6] = static_cast<uint8_t>((point.sample_number >> 8) & 0xFF);
    data[7] = static_cast<uint8_t>(point.sample_number & 0xFF);
    
    // u64 stream offset (big-endian)
    data[8] = static_cast<uint8_t>((point.stream_offset >> 56) & 0xFF);
    data[9] = static_cast<uint8_t>((point.stream_offset >> 48) & 0xFF);
    data[10] = static_cast<uint8_t>((point.stream_offset >> 40) & 0xFF);
    data[11] = static_cast<uint8_t>((point.stream_offset >> 32) & 0xFF);
    data[12] = static_cast<uint8_t>((point.stream_offset >> 24) & 0xFF);
    data[13] = static_cast<uint8_t>((point.stream_offset >> 16) & 0xFF);
    data[14] = static_cast<uint8_t>((point.stream_offset >> 8) & 0xFF);
    data[15] = static_cast<uint8_t>(point.stream_offset & 0xFF);
    
    // u16 frame samples (big-endian)
    data[16] = static_cast<uint8_t>((point.frame_samples >> 8) & 0xFF);
    data[17] = static_cast<uint8_t>(point.frame_samples & 0xFF);
}

/**
 * Helper to format bytes as hex string for debugging
 */
std::string bytesToHex(const uint8_t* bytes, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i) {
        if (i > 0) oss << " ";
        oss << "0x" << std::hex << std::setw(2) << std::setfill('0') 
            << static_cast<int>(bytes[i]);
    }
    return oss.str();
}

// ========================================
// PROPERTY-BASED TESTS
// ========================================

// ========================================
// PROPERTY 15: Seek Point Placeholder Detection
// ========================================
// **Feature: flac-demuxer, Property 15: Seek Point Placeholder Detection**
// **Validates: Requirements 12.5**
//
// For any seek point with sample number equal to 0xFFFFFFFFFFFFFFFF, 
// the FLAC Demuxer SHALL treat it as a placeholder.

void test_property_placeholder_detection() {
    std::cout << "\n=== Property 15: Seek Point Placeholder Detection ===" << std::endl;
    std::cout << "Testing that sample_number 0xFFFFFFFFFFFFFFFF is detected as placeholder..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: Exact placeholder value must be detected
    // ----------------------------------------
    std::cout << "\n  Test 1: Exact placeholder value detection..." << std::endl;
    {
        FLACSeekPoint placeholder(0xFFFFFFFFFFFFFFFFULL, 0, 0);
        tests_run++;
        
        if (placeholder.isPlaceholder()) {
            std::cout << "    Placeholder (0xFFFFFFFFFFFFFFFF) correctly detected ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Placeholder was not detected!" << std::endl;
            assert(false && "Placeholder should be detected");
        }
    }
    
    // ----------------------------------------
    // Test 2: Placeholder with various offset/samples values
    // ----------------------------------------
    std::cout << "\n  Test 2: Placeholder with various offset/samples values..." << std::endl;
    {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> offset_dist(0, 0xFFFFFFFFFFFFFFFFULL);
        std::uniform_int_distribution<uint16_t> samples_dist(0, 0xFFFF);
        
        for (int i = 0; i < 100; ++i) {
            uint64_t offset = offset_dist(gen);
            uint16_t samples = samples_dist(gen);
            
            FLACSeekPoint placeholder(0xFFFFFFFFFFFFFFFFULL, offset, samples);
            tests_run++;
            
            if (placeholder.isPlaceholder()) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Placeholder with offset=" << offset 
                          << ", samples=" << samples << " was not detected!" << std::endl;
                assert(false && "Placeholder should be detected regardless of offset/samples");
            }
        }
        std::cout << "    100 placeholder variations correctly detected ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 3: Non-placeholder values must NOT be detected as placeholder
    // ----------------------------------------
    std::cout << "\n  Test 3: Non-placeholder values rejection..." << std::endl;
    {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        
        // Generate random sample numbers that are NOT 0xFFFFFFFFFFFFFFFF
        std::uniform_int_distribution<uint64_t> sample_dist(0, 0xFFFFFFFFFFFFFFFEULL);
        
        for (int i = 0; i < 100; ++i) {
            uint64_t sample = sample_dist(gen);
            
            FLACSeekPoint point(sample, 0, 4096);
            tests_run++;
            
            if (!point.isPlaceholder()) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Non-placeholder sample=" << sample 
                          << " was incorrectly detected as placeholder!" << std::endl;
                assert(false && "Non-placeholder should not be detected as placeholder");
            }
        }
        std::cout << "    100 non-placeholder values correctly rejected ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 4: Boundary values near placeholder
    // ----------------------------------------
    std::cout << "\n  Test 4: Boundary values near placeholder..." << std::endl;
    {
        // Test values close to 0xFFFFFFFFFFFFFFFF
        std::vector<uint64_t> boundary_values = {
            0xFFFFFFFFFFFFFFFEULL,  // One less than placeholder
            0xFFFFFFFFFFFFFF00ULL,  // Lower byte different
            0xFFFFFFFFFFFF0000ULL,  // Lower two bytes different
            0xFFFFFFFFFFFE0000ULL,  // Different in middle
            0x7FFFFFFFFFFFFFFFULL,  // High bit clear
            0x0000000000000000ULL,  // Zero
            0x0000000000000001ULL,  // One
            0x8000000000000000ULL,  // Only high bit set
        };
        
        for (uint64_t sample : boundary_values) {
            FLACSeekPoint point(sample, 0, 4096);
            tests_run++;
            
            if (!point.isPlaceholder()) {
                std::cout << "    Sample 0x" << std::hex << sample << std::dec 
                          << " correctly not detected as placeholder ✓" << std::endl;
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Sample 0x" << std::hex << sample << std::dec 
                          << " was incorrectly detected as placeholder!" << std::endl;
                assert(false && "Boundary value should not be detected as placeholder");
            }
        }
    }
    
    // ----------------------------------------
    // Test 5: Round-trip serialization preserves placeholder status
    // ----------------------------------------
    std::cout << "\n  Test 5: Round-trip serialization preserves placeholder status..." << std::endl;
    {
        // Test placeholder round-trip
        {
            FLACSeekPoint original(0xFFFFFFFFFFFFFFFFULL, 12345, 4096);
            uint8_t buffer[18];
            serializeSeekPoint(original, buffer);
            FLACSeekPoint parsed = parseSeekPoint(buffer);
            
            tests_run++;
            if (parsed.isPlaceholder() && parsed.sample_number == original.sample_number) {
                std::cout << "    Placeholder round-trip preserved ✓" << std::endl;
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Placeholder round-trip failed!" << std::endl;
                assert(false && "Placeholder round-trip should preserve status");
            }
        }
        
        // Test non-placeholder round-trip
        {
            std::random_device rd;
            std::mt19937_64 gen(rd());
            std::uniform_int_distribution<uint64_t> sample_dist(0, 0xFFFFFFFFFFFFFFFEULL);
            
            for (int i = 0; i < 50; ++i) {
                uint64_t sample = sample_dist(gen);
                FLACSeekPoint original(sample, 12345, 4096);
                uint8_t buffer[18];
                serializeSeekPoint(original, buffer);
                FLACSeekPoint parsed = parseSeekPoint(buffer);
                
                tests_run++;
                if (!parsed.isPlaceholder() && parsed.sample_number == original.sample_number) {
                    tests_passed++;
                } else {
                    std::cerr << "    FAILED: Non-placeholder round-trip failed for sample=" 
                              << sample << std::endl;
                    assert(false && "Non-placeholder round-trip should preserve status");
                }
            }
            std::cout << "    50 non-placeholder round-trips preserved ✓" << std::endl;
        }
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 15: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// PROPERTY 15b: Placeholder Detection via Byte Pattern
// ========================================
// Additional test to verify placeholder detection works correctly
// when parsing from raw bytes (as would happen in actual SEEKTABLE parsing)

void test_property_placeholder_byte_pattern() {
    std::cout << "\n=== Property 15b: Placeholder Detection via Byte Pattern ===" << std::endl;
    std::cout << "Testing placeholder detection from raw 18-byte seek point data..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: Exact placeholder byte pattern
    // ----------------------------------------
    std::cout << "\n  Test 1: Exact placeholder byte pattern..." << std::endl;
    {
        // RFC 9639: Placeholder has sample_number = 0xFFFFFFFFFFFFFFFF
        // In big-endian, this is 8 bytes of 0xFF
        uint8_t placeholder_data[18] = {
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // sample_number
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // stream_offset
            0x00, 0x00                                        // frame_samples
        };
        
        FLACSeekPoint point = parseSeekPoint(placeholder_data);
        tests_run++;
        
        if (point.isPlaceholder()) {
            std::cout << "    Placeholder byte pattern " << bytesToHex(placeholder_data, 8) 
                      << " detected ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Placeholder byte pattern was not detected!" << std::endl;
            assert(false && "Placeholder byte pattern should be detected");
        }
    }
    
    // ----------------------------------------
    // Test 2: Placeholder with non-zero offset and samples
    // ----------------------------------------
    std::cout << "\n  Test 2: Placeholder with non-zero offset and samples..." << std::endl;
    {
        uint8_t placeholder_data[18] = {
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // sample_number (placeholder)
            0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x23, 0x45,  // stream_offset = 0x12345
            0x10, 0x00                                        // frame_samples = 4096
        };
        
        FLACSeekPoint point = parseSeekPoint(placeholder_data);
        tests_run++;
        
        if (point.isPlaceholder()) {
            std::cout << "    Placeholder with offset/samples detected ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Placeholder with offset/samples was not detected!" << std::endl;
            assert(false && "Placeholder should be detected regardless of offset/samples");
        }
    }
    
    // ----------------------------------------
    // Test 3: Non-placeholder byte patterns
    // ----------------------------------------
    std::cout << "\n  Test 3: Non-placeholder byte patterns..." << std::endl;
    {
        // Test case structure
        struct TestCase {
            const char* name;
            uint8_t data[18];
        };
        
        // Various non-placeholder patterns
        TestCase test_cases[] = {
            {"Zero sample", {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x10, 0x00}},
            {"One less than placeholder", {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE,
                                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                           0x10, 0x00}},
            {"High bit clear", {0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                               0x10, 0x00}},
            {"Typical seek point", {0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
                                   0x10, 0x00}},
        };
        
        for (const auto& tc : test_cases) {
            FLACSeekPoint point = parseSeekPoint(tc.data);
            tests_run++;
            
            if (!point.isPlaceholder()) {
                std::cout << "    " << tc.name << " correctly not detected as placeholder ✓" << std::endl;
                tests_passed++;
            } else {
                std::cerr << "    FAILED: " << tc.name << " was incorrectly detected as placeholder!" << std::endl;
                assert(false && "Non-placeholder byte pattern should not be detected as placeholder");
            }
        }
    }
    
    // ----------------------------------------
    // Test 4: Random non-placeholder byte patterns
    // ----------------------------------------
    std::cout << "\n  Test 4: Random non-placeholder byte patterns (100 iterations)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> byte_dist(0, 255);
        
        int random_passed = 0;
        
        for (int i = 0; i < 100; ++i) {
            uint8_t data[18];
            
            // Generate random bytes, but ensure sample_number != 0xFFFFFFFFFFFFFFFF
            do {
                for (int j = 0; j < 18; ++j) {
                    data[j] = static_cast<uint8_t>(byte_dist(gen));
                }
            } while (data[0] == 0xFF && data[1] == 0xFF && data[2] == 0xFF && data[3] == 0xFF &&
                     data[4] == 0xFF && data[5] == 0xFF && data[6] == 0xFF && data[7] == 0xFF);
            
            FLACSeekPoint point = parseSeekPoint(data);
            tests_run++;
            
            if (!point.isPlaceholder()) {
                tests_passed++;
                random_passed++;
            } else {
                std::cerr << "    FAILED: Random non-placeholder was detected as placeholder!" << std::endl;
                std::cerr << "    Data: " << bytesToHex(data, 18) << std::endl;
                assert(false && "Random non-placeholder should not be detected as placeholder");
            }
        }
        std::cout << "    " << random_passed << "/100 random non-placeholders correctly rejected ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 15b: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC SEEKTABLE PLACEHOLDER PROPERTY-BASED TESTS" << std::endl;
    std::cout << "**Feature: flac-demuxer, Property 15: Seek Point Placeholder Detection**" << std::endl;
    std::cout << "**Validates: Requirements 12.5**" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // Property 15: Seek Point Placeholder Detection
        // For any seek point with sample_number = 0xFFFFFFFFFFFFFFFF, treat as placeholder
        test_property_placeholder_detection();
        
        // Property 15b: Placeholder detection via byte pattern
        test_property_placeholder_byte_pattern();
        
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "✅ ALL PROPERTY TESTS PASSED" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(70, '=') << std::endl;
        std::cerr << "❌ PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Exception: " << e.what() << std::endl;
        std::cerr << std::string(70, '=') << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n" << std::string(70, '=') << std::endl;
        std::cerr << "❌ PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Unknown exception occurred" << std::endl;
        std::cerr << std::string(70, '=') << std::endl;
        return 1;
    }
}
