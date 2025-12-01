/*
 * test_flac_cuesheet_track_count_properties.cpp - Property-based tests for FLAC CUESHEET track count validation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <random>
#include <sstream>
#include <iomanip>

// ========================================
// STANDALONE CUESHEET PARSING AND VALIDATION
// ========================================

/**
 * RFC 9639 Section 8.7: CUESHEET Block Structure
 * 
 * - u(128*8): Media catalog number in ASCII (128 bytes)
 * - u(64): Number of lead-in samples
 * - u(1): CD-DA flag (1 if CD-DA, 0 otherwise)
 * - u(7+258*8): Reserved bits (259 bytes total with CD-DA flag)
 * - u(8): Number of tracks
 * - Cuesheet tracks: Variable length based on track count
 */

struct CuesheetData {
    char media_catalog_number[129];  // 128 bytes + null terminator
    uint64_t lead_in_samples;
    bool is_cd_da;
    uint8_t num_tracks;
};

/**
 * Validation result for CUESHEET block
 */
struct CuesheetValidationResult {
    bool is_valid;
    std::string error_message;
};

/**
 * Creates a minimal CUESHEET block header (without track data)
 * Returns the size of the header in bytes
 */
size_t createCuesheetHeader(uint8_t* data, const CuesheetData& cuesheet) {
    size_t offset = 0;
    
    // Media catalog number (128 bytes)
    std::memcpy(data + offset, cuesheet.media_catalog_number, 128);
    offset += 128;
    
    // Lead-in samples (u64 big-endian)
    data[offset++] = (cuesheet.lead_in_samples >> 56) & 0xFF;
    data[offset++] = (cuesheet.lead_in_samples >> 48) & 0xFF;
    data[offset++] = (cuesheet.lead_in_samples >> 40) & 0xFF;
    data[offset++] = (cuesheet.lead_in_samples >> 32) & 0xFF;
    data[offset++] = (cuesheet.lead_in_samples >> 24) & 0xFF;
    data[offset++] = (cuesheet.lead_in_samples >> 16) & 0xFF;
    data[offset++] = (cuesheet.lead_in_samples >> 8) & 0xFF;
    data[offset++] = cuesheet.lead_in_samples & 0xFF;
    
    // CD-DA flag (bit 7) + reserved bits (259 bytes total)
    // First byte: CD-DA flag in bit 7, rest are reserved (0)
    data[offset++] = cuesheet.is_cd_da ? 0x80 : 0x00;
    // Remaining 258 bytes of reserved bits
    std::memset(data + offset, 0, 258);
    offset += 258;
    
    // Number of tracks (u8)
    data[offset++] = cuesheet.num_tracks;
    
    return offset;  // Should be 396 bytes
}

/**
 * Parses a CUESHEET block header (without track data)
 */
CuesheetData parseCuesheetHeader(const uint8_t* data) {
    CuesheetData cuesheet;
    size_t offset = 0;
    
    // Media catalog number (128 bytes)
    std::memcpy(cuesheet.media_catalog_number, data + offset, 128);
    cuesheet.media_catalog_number[128] = '\0';
    offset += 128;
    
    // Lead-in samples (u64 big-endian)
    cuesheet.lead_in_samples = (static_cast<uint64_t>(data[offset]) << 56) |
                               (static_cast<uint64_t>(data[offset + 1]) << 48) |
                               (static_cast<uint64_t>(data[offset + 2]) << 40) |
                               (static_cast<uint64_t>(data[offset + 3]) << 32) |
                               (static_cast<uint64_t>(data[offset + 4]) << 24) |
                               (static_cast<uint64_t>(data[offset + 5]) << 16) |
                               (static_cast<uint64_t>(data[offset + 6]) << 8) |
                               data[offset + 7];
    offset += 8;
    
    // CD-DA flag (bit 7 of first byte)
    cuesheet.is_cd_da = (data[offset] & 0x80) != 0;
    offset += 259;  // Skip CD-DA flag byte + 258 reserved bytes
    
    // Number of tracks (u8)
    cuesheet.num_tracks = data[offset];
    
    return cuesheet;
}

/**
 * Validates CUESHEET track count per RFC 9639 Section 8.7
 * 
 * Requirement 16.6: Number of tracks must be at least 1
 * Requirement 16.7: For CD-DA, number of tracks must be at most 100
 */
CuesheetValidationResult validateCuesheetTrackCount(const CuesheetData& cuesheet) {
    CuesheetValidationResult result;
    result.is_valid = true;
    
    // Requirement 16.6: Number of tracks must be at least 1
    // RFC 9639 Section 8.7: "The number of tracks MUST be at least 1"
    if (cuesheet.num_tracks < 1) {
        result.is_valid = false;
        result.error_message = "Invalid CUESHEET: number of tracks must be at least 1";
        return result;
    }
    
    // Requirement 16.7: For CD-DA, number of tracks must be at most 100
    // RFC 9639 Section 8.7: "For CD-DA, this number MUST be no more than 100"
    if (cuesheet.is_cd_da && cuesheet.num_tracks > 100) {
        result.is_valid = false;
        result.error_message = "Invalid CD-DA CUESHEET: number of tracks must be at most 100";
        return result;
    }
    
    return result;
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
// PROPERTY 17: CUESHEET Track Count Validation
// ========================================
// **Feature: flac-demuxer, Property 17: CUESHEET Track Count Validation**
// **Validates: Requirements 16.6**
//
// For any CUESHEET block with number of tracks less than 1, 
// the FLAC Demuxer SHALL reject as invalid.

void test_property_cuesheet_track_count_validation() {
    std::cout << "\n=== Property 17: CUESHEET Track Count Validation ===" << std::endl;
    std::cout << "Testing that track count < 1 is rejected as invalid..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // Create a valid base CUESHEET for testing
    CuesheetData base_cuesheet;
    std::memset(base_cuesheet.media_catalog_number, 0, sizeof(base_cuesheet.media_catalog_number));
    base_cuesheet.lead_in_samples = 88200;  // 2 seconds at 44100 Hz
    base_cuesheet.is_cd_da = false;
    base_cuesheet.num_tracks = 1;
    
    // ----------------------------------------
    // Test 1: num_tracks = 0 must be rejected
    // ----------------------------------------
    std::cout << "\n  Test 1: num_tracks = 0 rejection..." << std::endl;
    {
        CuesheetData cuesheet = base_cuesheet;
        cuesheet.num_tracks = 0;
        
        tests_run++;
        
        CuesheetValidationResult result = validateCuesheetTrackCount(cuesheet);
        
        if (!result.is_valid) {
            tests_passed++;
            std::cout << "    num_tracks=0 rejected ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: num_tracks=0 was accepted!" << std::endl;
            assert(false && "num_tracks=0 should be rejected");
        }
    }
    
    // ----------------------------------------
    // Test 2: num_tracks = 1 must be accepted (boundary)
    // ----------------------------------------
    std::cout << "\n  Test 2: num_tracks = 1 acceptance (boundary)..." << std::endl;
    {
        CuesheetData cuesheet = base_cuesheet;
        cuesheet.num_tracks = 1;
        
        tests_run++;
        
        CuesheetValidationResult result = validateCuesheetTrackCount(cuesheet);
        
        if (result.is_valid) {
            tests_passed++;
            std::cout << "    num_tracks=1 accepted ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: num_tracks=1 was rejected!" << std::endl;
            assert(false && "num_tracks=1 should be accepted");
        }
    }
    
    // ----------------------------------------
    // Test 3: Valid track counts (1-255) for non-CD-DA must be accepted
    // ----------------------------------------
    std::cout << "\n  Test 3: Valid track counts (1-255) for non-CD-DA..." << std::endl;
    {
        std::vector<uint8_t> valid_counts = {1, 2, 10, 50, 99, 100, 101, 150, 200, 255};
        
        for (uint8_t count : valid_counts) {
            CuesheetData cuesheet = base_cuesheet;
            cuesheet.is_cd_da = false;
            cuesheet.num_tracks = count;
            
            tests_run++;
            
            CuesheetValidationResult result = validateCuesheetTrackCount(cuesheet);
            
            if (result.is_valid) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Valid track count " << static_cast<int>(count) 
                          << " for non-CD-DA was rejected!" << std::endl;
                assert(false && "Valid track count should be accepted");
            }
        }
        std::cout << "    All " << valid_counts.size() << " valid non-CD-DA track counts accepted ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 4: CD-DA track counts 1-100 must be accepted
    // ----------------------------------------
    std::cout << "\n  Test 4: CD-DA track counts 1-100 acceptance..." << std::endl;
    {
        std::vector<uint8_t> valid_cd_counts = {1, 2, 10, 50, 99, 100};
        
        for (uint8_t count : valid_cd_counts) {
            CuesheetData cuesheet = base_cuesheet;
            cuesheet.is_cd_da = true;
            cuesheet.num_tracks = count;
            
            tests_run++;
            
            CuesheetValidationResult result = validateCuesheetTrackCount(cuesheet);
            
            if (result.is_valid) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Valid CD-DA track count " << static_cast<int>(count) 
                          << " was rejected!" << std::endl;
                assert(false && "Valid CD-DA track count should be accepted");
            }
        }
        std::cout << "    All " << valid_cd_counts.size() << " valid CD-DA track counts accepted ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 5: CD-DA track counts > 100 must be rejected
    // ----------------------------------------
    std::cout << "\n  Test 5: CD-DA track counts > 100 rejection..." << std::endl;
    {
        std::vector<uint8_t> invalid_cd_counts = {101, 102, 150, 200, 255};
        
        for (uint8_t count : invalid_cd_counts) {
            CuesheetData cuesheet = base_cuesheet;
            cuesheet.is_cd_da = true;
            cuesheet.num_tracks = count;
            
            tests_run++;
            
            CuesheetValidationResult result = validateCuesheetTrackCount(cuesheet);
            
            if (!result.is_valid) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Invalid CD-DA track count " << static_cast<int>(count) 
                          << " was accepted!" << std::endl;
                assert(false && "CD-DA track count > 100 should be rejected");
            }
        }
        std::cout << "    All " << invalid_cd_counts.size() << " invalid CD-DA track counts rejected ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 6: Random valid track counts (100 iterations)
    // ----------------------------------------
    std::cout << "\n  Test 6: Random valid track counts (100 iterations)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> count_dist(1, 255);
        std::uniform_int_distribution<int> cd_dist(0, 1);
        
        for (int i = 0; i < 100; ++i) {
            CuesheetData cuesheet = base_cuesheet;
            cuesheet.is_cd_da = (cd_dist(gen) == 1);
            
            // Generate valid track count based on CD-DA flag
            if (cuesheet.is_cd_da) {
                std::uniform_int_distribution<int> cd_count_dist(1, 100);
                cuesheet.num_tracks = static_cast<uint8_t>(cd_count_dist(gen));
            } else {
                cuesheet.num_tracks = static_cast<uint8_t>(count_dist(gen));
            }
            
            tests_run++;
            
            CuesheetValidationResult result = validateCuesheetTrackCount(cuesheet);
            
            if (result.is_valid) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Valid track count " << static_cast<int>(cuesheet.num_tracks) 
                          << " (CD-DA=" << cuesheet.is_cd_da << ") was rejected!" << std::endl;
                assert(false && "Valid track count should be accepted");
            }
        }
        std::cout << "    100 random valid track counts accepted ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 7: Round-trip encoding/decoding preserves track count
    // ----------------------------------------
    std::cout << "\n  Test 7: Round-trip encoding/decoding (100 iterations)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> count_dist(1, 255);
        std::uniform_int_distribution<int> cd_dist(0, 1);
        
        for (int i = 0; i < 100; ++i) {
            CuesheetData original = base_cuesheet;
            original.is_cd_da = (cd_dist(gen) == 1);
            original.num_tracks = static_cast<uint8_t>(count_dist(gen));
            original.lead_in_samples = static_cast<uint64_t>(gen()) << 32 | gen();
            
            // Encode to bytes
            uint8_t data[396];
            createCuesheetHeader(data, original);
            
            // Decode back
            CuesheetData decoded = parseCuesheetHeader(data);
            
            tests_run++;
            
            if (decoded.num_tracks == original.num_tracks &&
                decoded.is_cd_da == original.is_cd_da &&
                decoded.lead_in_samples == original.lead_in_samples) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Round-trip mismatch!" << std::endl;
                std::cerr << "      Original: num_tracks=" << static_cast<int>(original.num_tracks) 
                          << ", is_cd_da=" << original.is_cd_da 
                          << ", lead_in=" << original.lead_in_samples << std::endl;
                std::cerr << "      Decoded:  num_tracks=" << static_cast<int>(decoded.num_tracks) 
                          << ", is_cd_da=" << decoded.is_cd_da 
                          << ", lead_in=" << decoded.lead_in_samples << std::endl;
                assert(false && "Round-trip should preserve cuesheet data");
            }
        }
        std::cout << "    100 round-trip tests successful ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 17: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC CUESHEET TRACK COUNT PROPERTY-BASED TESTS" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // Property 17: CUESHEET Track Count Validation
        // **Feature: flac-demuxer, Property 17: CUESHEET Track Count Validation**
        // **Validates: Requirements 16.6**
        test_property_cuesheet_track_count_validation();
        
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

