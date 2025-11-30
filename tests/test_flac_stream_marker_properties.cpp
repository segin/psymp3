/*
 * test_flac_stream_marker_properties.cpp - Property-based tests for FLAC stream marker validation
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
#include <fstream>
#include <iomanip>

// ========================================
// STANDALONE STREAM MARKER VALIDATION
// ========================================

/**
 * RFC 9639 Section 6: FLAC stream marker
 * The stream marker must be exactly 0x66 0x4C 0x61 0x43 ("fLaC" in ASCII)
 */
static constexpr uint8_t VALID_FLAC_MARKER[4] = {0x66, 0x4C, 0x61, 0x43};

/**
 * Validates a 4-byte stream marker against RFC 9639 Section 6
 * @param marker Pointer to 4 bytes to validate
 * @return true if marker is valid FLAC marker, false otherwise
 */
bool validateStreamMarker(const uint8_t* marker) {
    if (!marker) return false;
    
    return marker[0] == VALID_FLAC_MARKER[0] &&
           marker[1] == VALID_FLAC_MARKER[1] &&
           marker[2] == VALID_FLAC_MARKER[2] &&
           marker[3] == VALID_FLAC_MARKER[3];
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
// PROPERTY 1: Stream Marker Validation
// ========================================
// **Feature: flac-demuxer, Property 1: Stream Marker Validation**
// **Validates: Requirements 1.2, 1.3**
//
// For any 4-byte sequence at the start of a file, the FLAC Demuxer SHALL 
// accept only the exact sequence 0x66 0x4C 0x61 0x43 (fLaC) and reject 
// all other sequences without crashing.

void test_property_stream_marker_validation() {
    std::cout << "\n=== Property 1: Stream Marker Validation ===" << std::endl;
    std::cout << "Testing that only exact fLaC marker (0x66 0x4C 0x61 0x43) is accepted..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: Valid FLAC marker must be accepted
    // ----------------------------------------
    std::cout << "\n  Test 1: Valid FLAC marker acceptance..." << std::endl;
    {
        uint8_t valid_marker[4] = {0x66, 0x4C, 0x61, 0x43};
        tests_run++;
        
        bool result = validateStreamMarker(valid_marker);
        if (result) {
            std::cout << "    Valid marker " << bytesToHex(valid_marker, 4) << " accepted ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Valid marker was rejected!" << std::endl;
            assert(false && "Valid FLAC marker should be accepted");
        }
    }
    
    // ----------------------------------------
    // Test 2: All single-byte variations must be rejected
    // ----------------------------------------
    std::cout << "\n  Test 2: Single-byte variations rejection..." << std::endl;
    {
        // Test changing each byte position to all other possible values
        for (int pos = 0; pos < 4; ++pos) {
            for (int val = 0; val < 256; ++val) {
                uint8_t marker[4] = {0x66, 0x4C, 0x61, 0x43};
                
                // Skip if this would create the valid marker
                if (val == VALID_FLAC_MARKER[pos]) continue;
                
                marker[pos] = static_cast<uint8_t>(val);
                tests_run++;
                
                bool result = validateStreamMarker(marker);
                if (!result) {
                    tests_passed++;
                } else {
                    std::cerr << "    FAILED: Invalid marker " << bytesToHex(marker, 4) 
                              << " was accepted!" << std::endl;
                    assert(false && "Invalid marker should be rejected");
                }
            }
        }
        std::cout << "    All single-byte variations rejected ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 3: Random 4-byte sequences must be rejected (except valid marker)
    // ----------------------------------------
    std::cout << "\n  Test 3: Random 4-byte sequences (100 iterations)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> byte_dist(0, 255);
        
        int random_tests = 0;
        int random_passed = 0;
        
        for (int i = 0; i < 100; ++i) {
            uint8_t marker[4];
            
            // Generate random 4-byte sequence
            for (int j = 0; j < 4; ++j) {
                marker[j] = static_cast<uint8_t>(byte_dist(gen));
            }
            
            tests_run++;
            random_tests++;
            
            bool result = validateStreamMarker(marker);
            bool is_valid_marker = (marker[0] == 0x66 && marker[1] == 0x4C && 
                                    marker[2] == 0x61 && marker[3] == 0x43);
            
            // Result should match whether it's the valid marker
            if (result == is_valid_marker) {
                tests_passed++;
                random_passed++;
            } else {
                std::cerr << "    FAILED: Marker " << bytesToHex(marker, 4) 
                          << " result=" << result << " expected=" << is_valid_marker << std::endl;
                assert(false && "Marker validation result mismatch");
            }
        }
        std::cout << "    " << random_passed << "/" << random_tests << " random sequences handled correctly ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 4: Common file format markers must be rejected
    // ----------------------------------------
    std::cout << "\n  Test 4: Common file format markers rejection..." << std::endl;
    {
        struct TestCase {
            uint8_t marker[4];
            const char* name;
        };
        
        std::vector<TestCase> common_markers = {
            {{0x52, 0x49, 0x46, 0x46}, "RIFF (WAV)"},      // "RIFF"
            {{0x4F, 0x67, 0x67, 0x53}, "OggS (Ogg)"},      // "OggS"
            {{0x49, 0x44, 0x33, 0x00}, "ID3 (MP3)"},       // "ID3\0"
            {{0xFF, 0xFB, 0x00, 0x00}, "MP3 sync"},        // MP3 frame sync
            {{0x00, 0x00, 0x00, 0x00}, "Null bytes"},      // All zeros
            {{0xFF, 0xFF, 0xFF, 0xFF}, "All 0xFF"},        // All ones
            {{0x66, 0x4C, 0x61, 0x00}, "fLa\\0 (partial)"}, // Almost FLAC
            {{0x46, 0x4C, 0x41, 0x43}, "FLAC (uppercase)"}, // "FLAC" uppercase
            {{0x66, 0x6C, 0x61, 0x63}, "flac (lowercase)"}, // "flac" lowercase
        };
        
        for (const auto& tc : common_markers) {
            tests_run++;
            
            bool result = validateStreamMarker(tc.marker);
            if (!result) {
                std::cout << "    " << tc.name << " " << bytesToHex(tc.marker, 4) << " rejected ✓" << std::endl;
                tests_passed++;
            } else {
                std::cerr << "    FAILED: " << tc.name << " was accepted!" << std::endl;
                assert(false && "Common file format marker should be rejected");
            }
        }
    }
    
    // ----------------------------------------
    // Test 5: Null pointer handling (no crash)
    // ----------------------------------------
    std::cout << "\n  Test 5: Null pointer handling..." << std::endl;
    {
        tests_run++;
        
        bool result = validateStreamMarker(nullptr);
        if (!result) {
            std::cout << "    Null pointer rejected without crash ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Null pointer was accepted!" << std::endl;
            assert(false && "Null pointer should be rejected");
        }
    }
    
    // ----------------------------------------
    // Test 6: Case sensitivity verification
    // ----------------------------------------
    std::cout << "\n  Test 6: Case sensitivity verification..." << std::endl;
    {
        // The marker is case-sensitive: "fLaC" not "FLAC" or "flac"
        struct CaseTest {
            uint8_t marker[4];
            const char* description;
        };
        
        std::vector<CaseTest> case_tests = {
            {{0x46, 0x4C, 0x41, 0x43}, "FLAC (all uppercase)"},
            {{0x66, 0x6C, 0x61, 0x63}, "flac (all lowercase)"},
            {{0x46, 0x6C, 0x61, 0x43}, "FlaC (wrong case)"},
            {{0x66, 0x4C, 0x41, 0x43}, "fLAC (wrong case)"},
            {{0x66, 0x4C, 0x61, 0x63}, "fLac (wrong case)"},
        };
        
        for (const auto& ct : case_tests) {
            tests_run++;
            
            bool result = validateStreamMarker(ct.marker);
            if (!result) {
                std::cout << "    " << ct.description << " rejected ✓" << std::endl;
                tests_passed++;
            } else {
                std::cerr << "    FAILED: " << ct.description << " was accepted!" << std::endl;
                assert(false && "Case-incorrect marker should be rejected");
            }
        }
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 1: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// PROPERTY 1b: Exhaustive Single-Byte Variation Test
// ========================================
// Additional exhaustive test to verify ALL possible single-byte changes
// from the valid marker are rejected.

void test_property_exhaustive_single_byte_variations() {
    std::cout << "\n=== Property 1b: Exhaustive Single-Byte Variations ===" << std::endl;
    std::cout << "Testing all 1020 possible single-byte variations are rejected..." << std::endl;
    
    int variations_tested = 0;
    int variations_rejected = 0;
    
    // For each position (0-3) and each possible byte value (0-255)
    // that differs from the valid marker, verify rejection
    for (int pos = 0; pos < 4; ++pos) {
        for (int val = 0; val < 256; ++val) {
            // Skip the valid byte for this position
            if (val == VALID_FLAC_MARKER[pos]) continue;
            
            uint8_t marker[4] = {0x66, 0x4C, 0x61, 0x43};
            marker[pos] = static_cast<uint8_t>(val);
            
            variations_tested++;
            
            if (!validateStreamMarker(marker)) {
                variations_rejected++;
            } else {
                std::cerr << "  FAILED: Position " << pos << ", value 0x" 
                          << std::hex << val << std::dec << " was accepted!" << std::endl;
                assert(false && "Single-byte variation should be rejected");
            }
        }
    }
    
    // 4 positions × 255 invalid values = 1020 variations
    std::cout << "  Tested " << variations_tested << " single-byte variations" << std::endl;
    std::cout << "  All " << variations_rejected << " variations correctly rejected ✓" << std::endl;
    
    assert(variations_tested == 1020);
    assert(variations_rejected == 1020);
    
    std::cout << "\n✓ Property 1b: Exhaustive single-byte variation test PASSED" << std::endl;
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC STREAM MARKER PROPERTY-BASED TESTS" << std::endl;
    std::cout << "**Feature: flac-demuxer, Property 1: Stream Marker Validation**" << std::endl;
    std::cout << "**Validates: Requirements 1.2, 1.3**" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // Property 1: Stream Marker Validation
        // For any 4-byte sequence, only 0x66 0x4C 0x61 0x43 should be accepted
        test_property_stream_marker_validation();
        
        // Property 1b: Exhaustive single-byte variation test
        test_property_exhaustive_single_byte_variations();
        
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
