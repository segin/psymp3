/*
 * test_flac_streaminfo_block_size_properties.cpp - Property-based tests for FLAC STREAMINFO block size validation
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
// STANDALONE STREAMINFO PARSING AND VALIDATION
// ========================================

/**
 * RFC 9639 Section 8.2: STREAMINFO Block Structure (34 bytes)
 * 
 * - Bytes 0-1: minimum block size (u16 big-endian)
 * - Bytes 2-3: maximum block size (u16 big-endian)
 * - Bytes 4-6: minimum frame size (u24 big-endian)
 * - Bytes 7-9: maximum frame size (u24 big-endian)
 * - Bytes 10-13: sample rate (u20), channels-1 (u3), bits_per_sample-1 (u5)
 * - Bytes 13-17: total samples (u36)
 * - Bytes 18-33: MD5 signature (128 bits)
 */

struct StreamInfoData {
    uint16_t min_block_size;
    uint16_t max_block_size;
    uint32_t min_frame_size;
    uint32_t max_frame_size;
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;
    uint64_t total_samples;
    uint8_t md5_signature[16];
};


/**
 * Validation result for STREAMINFO block
 */
struct StreamInfoValidationResult {
    bool is_valid;
    std::string error_message;
};

/**
 * Creates a 34-byte STREAMINFO block from components
 */
void createStreamInfoBlock(uint8_t* data, const StreamInfoData& info) {
    // Bytes 0-1: minimum block size (u16 big-endian)
    data[0] = (info.min_block_size >> 8) & 0xFF;
    data[1] = info.min_block_size & 0xFF;
    
    // Bytes 2-3: maximum block size (u16 big-endian)
    data[2] = (info.max_block_size >> 8) & 0xFF;
    data[3] = info.max_block_size & 0xFF;
    
    // Bytes 4-6: minimum frame size (u24 big-endian)
    data[4] = (info.min_frame_size >> 16) & 0xFF;
    data[5] = (info.min_frame_size >> 8) & 0xFF;
    data[6] = info.min_frame_size & 0xFF;
    
    // Bytes 7-9: maximum frame size (u24 big-endian)
    data[7] = (info.max_frame_size >> 16) & 0xFF;
    data[8] = (info.max_frame_size >> 8) & 0xFF;
    data[9] = info.max_frame_size & 0xFF;
    
    // Bytes 10-13: sample rate (u20), channels-1 (u3), bits_per_sample-1 (u5)
    // Byte 10: sample_rate[19:12]
    data[10] = (info.sample_rate >> 12) & 0xFF;
    // Byte 11: sample_rate[11:4]
    data[11] = (info.sample_rate >> 4) & 0xFF;
    // Byte 12: sample_rate[3:0], channels-1[2:0], bits_per_sample-1[4]
    uint8_t channels_minus_1 = (info.channels > 0) ? (info.channels - 1) : 0;
    uint8_t bps_minus_1 = (info.bits_per_sample > 0) ? (info.bits_per_sample - 1) : 0;
    data[12] = ((info.sample_rate & 0x0F) << 4) | ((channels_minus_1 & 0x07) << 1) | ((bps_minus_1 >> 4) & 0x01);
    // Byte 13: bits_per_sample-1[3:0], total_samples[35:32]
    data[13] = ((bps_minus_1 & 0x0F) << 4) | ((info.total_samples >> 32) & 0x0F);
    
    // Bytes 14-17: total_samples[31:0]
    data[14] = (info.total_samples >> 24) & 0xFF;
    data[15] = (info.total_samples >> 16) & 0xFF;
    data[16] = (info.total_samples >> 8) & 0xFF;
    data[17] = info.total_samples & 0xFF;
    
    // Bytes 18-33: MD5 signature
    std::memcpy(&data[18], info.md5_signature, 16);
}


/**
 * Parses a 34-byte STREAMINFO block per RFC 9639 Section 8.2
 */
StreamInfoData parseStreamInfoBlock(const uint8_t* data) {
    StreamInfoData info;
    
    // Bytes 0-1: minimum block size (u16 big-endian)
    info.min_block_size = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    
    // Bytes 2-3: maximum block size (u16 big-endian)
    info.max_block_size = (static_cast<uint16_t>(data[2]) << 8) | data[3];
    
    // Bytes 4-6: minimum frame size (u24 big-endian)
    info.min_frame_size = (static_cast<uint32_t>(data[4]) << 16) |
                          (static_cast<uint32_t>(data[5]) << 8) |
                          data[6];
    
    // Bytes 7-9: maximum frame size (u24 big-endian)
    info.max_frame_size = (static_cast<uint32_t>(data[7]) << 16) |
                          (static_cast<uint32_t>(data[8]) << 8) |
                          data[9];
    
    // Bytes 10-13: sample rate (u20), channels-1 (u3), bits_per_sample-1 (u5)
    info.sample_rate = (static_cast<uint32_t>(data[10]) << 12) |
                       (static_cast<uint32_t>(data[11]) << 4) |
                       (data[12] >> 4);
    
    info.channels = ((data[12] >> 1) & 0x07) + 1;
    info.bits_per_sample = (((data[12] & 0x01) << 4) | (data[13] >> 4)) + 1;
    
    // Bytes 13-17: total samples (u36)
    info.total_samples = (static_cast<uint64_t>(data[13] & 0x0F) << 32) |
                         (static_cast<uint64_t>(data[14]) << 24) |
                         (static_cast<uint64_t>(data[15]) << 16) |
                         (static_cast<uint64_t>(data[16]) << 8) |
                         data[17];
    
    // Bytes 18-33: MD5 signature
    std::memcpy(info.md5_signature, &data[18], 16);
    
    return info;
}


/**
 * Validates STREAMINFO block size fields per RFC 9639 Section 8.2 and Table 1
 * 
 * Requirements 3.6, 3.7: min/max block size must be >= 16 (forbidden pattern if < 16)
 * Requirement 3.8: min_block_size must not exceed max_block_size
 */
StreamInfoValidationResult validateStreamInfoBlockSize(const StreamInfoData& info) {
    StreamInfoValidationResult result;
    result.is_valid = true;
    
    // Requirement 3.6: Reject if minimum block size < 16 (RFC 9639 Table 1 forbidden pattern)
    if (info.min_block_size < 16) {
        result.is_valid = false;
        result.error_message = "Forbidden: min_block_size < 16 (RFC 9639 Table 1)";
        return result;
    }
    
    // Requirement 3.7: Reject if maximum block size < 16 (RFC 9639 Table 1 forbidden pattern)
    if (info.max_block_size < 16) {
        result.is_valid = false;
        result.error_message = "Forbidden: max_block_size < 16 (RFC 9639 Table 1)";
        return result;
    }
    
    // Requirement 3.8: Reject if minimum block size exceeds maximum block size
    if (info.min_block_size > info.max_block_size) {
        result.is_valid = false;
        result.error_message = "Invalid: min_block_size > max_block_size";
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
// PROPERTY 4: STREAMINFO Block Size Validation
// ========================================
// **Feature: flac-demuxer, Property 4: STREAMINFO Block Size Validation**
// **Validates: Requirements 3.6, 3.7**
//
// For any STREAMINFO block with minimum block size < 16 or maximum block size < 16,
// the FLAC Demuxer SHALL reject the stream as a forbidden pattern.

void test_property_streaminfo_block_size_validation() {
    std::cout << "\n=== Property 4: STREAMINFO Block Size Validation ===" << std::endl;
    std::cout << "Testing that block sizes < 16 are rejected as forbidden patterns..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // Create a valid base STREAMINFO for testing
    StreamInfoData base_info;
    base_info.min_block_size = 4096;
    base_info.max_block_size = 4096;
    base_info.min_frame_size = 14;
    base_info.max_frame_size = 8192;
    base_info.sample_rate = 44100;
    base_info.channels = 2;
    base_info.bits_per_sample = 16;
    base_info.total_samples = 44100 * 60;  // 1 minute
    std::memset(base_info.md5_signature, 0, 16);
    
    // ----------------------------------------
    // Test 1: min_block_size < 16 must be rejected
    // ----------------------------------------
    std::cout << "\n  Test 1: min_block_size < 16 rejection..." << std::endl;
    {
        // Test all values 0-15 for min_block_size
        for (uint16_t min_bs = 0; min_bs < 16; ++min_bs) {
            StreamInfoData info = base_info;
            info.min_block_size = min_bs;
            info.max_block_size = 4096;  // Valid max
            
            tests_run++;
            
            StreamInfoValidationResult result = validateStreamInfoBlockSize(info);
            
            if (!result.is_valid) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: min_block_size=" << min_bs << " was accepted!" << std::endl;
                assert(false && "min_block_size < 16 should be rejected");
            }
        }
        std::cout << "    All 16 forbidden min_block_size values (0-15) rejected ✓" << std::endl;
    }


    // ----------------------------------------
    // Test 2: max_block_size < 16 must be rejected
    // ----------------------------------------
    std::cout << "\n  Test 2: max_block_size < 16 rejection..." << std::endl;
    {
        // Test all values 0-15 for max_block_size
        for (uint16_t max_bs = 0; max_bs < 16; ++max_bs) {
            StreamInfoData info = base_info;
            info.min_block_size = 16;  // Valid min (boundary)
            info.max_block_size = max_bs;
            
            tests_run++;
            
            StreamInfoValidationResult result = validateStreamInfoBlockSize(info);
            
            if (!result.is_valid) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: max_block_size=" << max_bs << " was accepted!" << std::endl;
                assert(false && "max_block_size < 16 should be rejected");
            }
        }
        std::cout << "    All 16 forbidden max_block_size values (0-15) rejected ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 3: Boundary value 16 must be accepted
    // ----------------------------------------
    std::cout << "\n  Test 3: Boundary value 16 acceptance..." << std::endl;
    {
        StreamInfoData info = base_info;
        info.min_block_size = 16;
        info.max_block_size = 16;
        
        tests_run++;
        
        StreamInfoValidationResult result = validateStreamInfoBlockSize(info);
        
        if (result.is_valid) {
            std::cout << "    min_block_size=16, max_block_size=16 accepted ✓" << std::endl;
            tests_passed++;
        } else {
            std::cerr << "    FAILED: Boundary value 16 was rejected!" << std::endl;
            assert(false && "Block size 16 should be accepted");
        }
    }
    
    // ----------------------------------------
    // Test 4: Valid block sizes (16-65535) must be accepted
    // ----------------------------------------
    std::cout << "\n  Test 4: Valid block sizes acceptance..." << std::endl;
    {
        std::vector<uint16_t> valid_sizes = {16, 17, 192, 256, 512, 576, 1024, 1152, 
                                              2048, 2304, 4096, 4608, 8192, 16384, 32768, 65535};
        
        for (uint16_t size : valid_sizes) {
            StreamInfoData info = base_info;
            info.min_block_size = size;
            info.max_block_size = size;
            
            tests_run++;
            
            StreamInfoValidationResult result = validateStreamInfoBlockSize(info);
            
            if (result.is_valid) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Valid block size " << size << " was rejected!" << std::endl;
                assert(false && "Valid block size should be accepted");
            }
        }
        std::cout << "    All " << valid_sizes.size() << " valid block sizes accepted ✓" << std::endl;
    }


    // ----------------------------------------
    // Test 5: Random valid block sizes (100 iterations)
    // ----------------------------------------
    std::cout << "\n  Test 5: Random valid block sizes (100 iterations)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint16_t> size_dist(16, 65535);  // Valid range
        
        for (int i = 0; i < 100; ++i) {
            uint16_t min_bs = size_dist(gen);
            uint16_t max_bs = size_dist(gen);
            
            // Ensure min <= max
            if (min_bs > max_bs) {
                std::swap(min_bs, max_bs);
            }
            
            StreamInfoData info = base_info;
            info.min_block_size = min_bs;
            info.max_block_size = max_bs;
            
            tests_run++;
            
            StreamInfoValidationResult result = validateStreamInfoBlockSize(info);
            
            if (result.is_valid) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Valid sizes min=" << min_bs << " max=" << max_bs << " rejected!" << std::endl;
                assert(false && "Valid block sizes should be accepted");
            }
        }
        std::cout << "    100 random valid block size combinations accepted ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 6: Round-trip encoding/decoding preserves block sizes
    // ----------------------------------------
    std::cout << "\n  Test 6: Round-trip encoding/decoding (100 iterations)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint16_t> size_dist(16, 65535);
        
        for (int i = 0; i < 100; ++i) {
            uint16_t min_bs = size_dist(gen);
            uint16_t max_bs = size_dist(gen);
            if (min_bs > max_bs) std::swap(min_bs, max_bs);
            
            StreamInfoData original = base_info;
            original.min_block_size = min_bs;
            original.max_block_size = max_bs;
            
            // Encode to bytes
            uint8_t data[34];
            createStreamInfoBlock(data, original);
            
            // Decode back
            StreamInfoData decoded = parseStreamInfoBlock(data);
            
            tests_run++;
            
            if (decoded.min_block_size == original.min_block_size &&
                decoded.max_block_size == original.max_block_size) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Round-trip mismatch!" << std::endl;
                std::cerr << "      Original: min=" << original.min_block_size << " max=" << original.max_block_size << std::endl;
                std::cerr << "      Decoded:  min=" << decoded.min_block_size << " max=" << decoded.max_block_size << std::endl;
                assert(false && "Round-trip should preserve block sizes");
            }
        }
        std::cout << "    100 round-trip tests successful ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 4: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}


// ========================================
// PROPERTY 5: STREAMINFO Block Size Ordering
// ========================================
// **Feature: flac-demuxer, Property 5: STREAMINFO Block Size Ordering**
// **Validates: Requirements 3.8**
//
// For any STREAMINFO block where minimum block size exceeds maximum block size,
// the FLAC Demuxer SHALL reject the stream.

void test_property_streaminfo_block_size_ordering() {
    std::cout << "\n=== Property 5: STREAMINFO Block Size Ordering ===" << std::endl;
    std::cout << "Testing that min_block_size > max_block_size is rejected..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // Create a valid base STREAMINFO for testing
    StreamInfoData base_info;
    base_info.min_block_size = 4096;
    base_info.max_block_size = 4096;
    base_info.min_frame_size = 14;
    base_info.max_frame_size = 8192;
    base_info.sample_rate = 44100;
    base_info.channels = 2;
    base_info.bits_per_sample = 16;
    base_info.total_samples = 44100 * 60;
    std::memset(base_info.md5_signature, 0, 16);
    
    // ----------------------------------------
    // Test 1: min_block_size > max_block_size must be rejected
    // ----------------------------------------
    std::cout << "\n  Test 1: min_block_size > max_block_size rejection..." << std::endl;
    {
        struct TestCase {
            uint16_t min_bs;
            uint16_t max_bs;
        };
        
        std::vector<TestCase> test_cases = {
            {17, 16},       // Boundary: min just above max
            {100, 50},      // min > max
            {4096, 2048},   // Common sizes, wrong order
            {65535, 16},    // Maximum min, minimum valid max
            {1000, 999},    // Off by one
            {32768, 16384}, // Power of 2 sizes, wrong order
        };
        
        for (const auto& tc : test_cases) {
            StreamInfoData info = base_info;
            info.min_block_size = tc.min_bs;
            info.max_block_size = tc.max_bs;
            
            tests_run++;
            
            StreamInfoValidationResult result = validateStreamInfoBlockSize(info);
            
            if (!result.is_valid) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: min=" << tc.min_bs << " > max=" << tc.max_bs << " was accepted!" << std::endl;
                assert(false && "min_block_size > max_block_size should be rejected");
            }
        }
        std::cout << "    All " << test_cases.size() << " invalid orderings rejected ✓" << std::endl;
    }


    // ----------------------------------------
    // Test 2: min_block_size == max_block_size must be accepted
    // ----------------------------------------
    std::cout << "\n  Test 2: min_block_size == max_block_size acceptance..." << std::endl;
    {
        std::vector<uint16_t> equal_sizes = {16, 192, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65535};
        
        for (uint16_t size : equal_sizes) {
            StreamInfoData info = base_info;
            info.min_block_size = size;
            info.max_block_size = size;
            
            tests_run++;
            
            StreamInfoValidationResult result = validateStreamInfoBlockSize(info);
            
            if (result.is_valid) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Equal sizes " << size << " rejected!" << std::endl;
                assert(false && "Equal block sizes should be accepted");
            }
        }
        std::cout << "    All " << equal_sizes.size() << " equal size combinations accepted ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 3: min_block_size < max_block_size must be accepted
    // ----------------------------------------
    std::cout << "\n  Test 3: min_block_size < max_block_size acceptance..." << std::endl;
    {
        struct TestCase {
            uint16_t min_bs;
            uint16_t max_bs;
        };
        
        std::vector<TestCase> test_cases = {
            {16, 17},       // Boundary: min just below max
            {16, 65535},    // Full range
            {192, 4096},    // Common FLAC sizes
            {576, 1152},    // CD-quality sizes
            {4096, 4608},   // Typical variable block sizes
            {1024, 8192},   // 8x range
        };
        
        for (const auto& tc : test_cases) {
            StreamInfoData info = base_info;
            info.min_block_size = tc.min_bs;
            info.max_block_size = tc.max_bs;
            
            tests_run++;
            
            StreamInfoValidationResult result = validateStreamInfoBlockSize(info);
            
            if (result.is_valid) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: min=" << tc.min_bs << " < max=" << tc.max_bs << " rejected!" << std::endl;
                assert(false && "min_block_size < max_block_size should be accepted");
            }
        }
        std::cout << "    All " << test_cases.size() << " valid orderings accepted ✓" << std::endl;
    }


    // ----------------------------------------
    // Test 4: Random valid orderings (100 iterations)
    // ----------------------------------------
    std::cout << "\n  Test 4: Random valid orderings (100 iterations)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint16_t> size_dist(16, 65535);
        
        for (int i = 0; i < 100; ++i) {
            uint16_t min_bs = size_dist(gen);
            uint16_t max_bs = size_dist(gen);
            
            // Ensure min <= max (valid ordering)
            if (min_bs > max_bs) {
                std::swap(min_bs, max_bs);
            }
            
            StreamInfoData info = base_info;
            info.min_block_size = min_bs;
            info.max_block_size = max_bs;
            
            tests_run++;
            
            StreamInfoValidationResult result = validateStreamInfoBlockSize(info);
            
            if (result.is_valid) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Valid ordering min=" << min_bs << " max=" << max_bs << " rejected!" << std::endl;
                assert(false && "Valid ordering should be accepted");
            }
        }
        std::cout << "    100 random valid orderings accepted ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 5: Random invalid orderings (100 iterations)
    // ----------------------------------------
    std::cout << "\n  Test 5: Random invalid orderings (100 iterations)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint16_t> size_dist(16, 65534);  // Leave room for +1
        
        for (int i = 0; i < 100; ++i) {
            uint16_t max_bs = size_dist(gen);
            // Ensure min > max (invalid ordering)
            uint16_t min_bs = max_bs + 1;
            if (min_bs < max_bs) {
                // Handle overflow - just use a simple invalid case
                min_bs = 65535;
                max_bs = 16;
            }
            
            StreamInfoData info = base_info;
            info.min_block_size = min_bs;
            info.max_block_size = max_bs;
            
            tests_run++;
            
            StreamInfoValidationResult result = validateStreamInfoBlockSize(info);
            
            if (!result.is_valid) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Invalid ordering min=" << min_bs << " max=" << max_bs << " accepted!" << std::endl;
                assert(false && "Invalid ordering should be rejected");
            }
        }
        std::cout << "    100 random invalid orderings rejected ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 5: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}


// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC STREAMINFO BLOCK SIZE PROPERTY-BASED TESTS" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // Property 4: STREAMINFO Block Size Validation
        // **Feature: flac-demuxer, Property 4: STREAMINFO Block Size Validation**
        // **Validates: Requirements 3.6, 3.7**
        test_property_streaminfo_block_size_validation();
        
        // Property 5: STREAMINFO Block Size Ordering
        // **Feature: flac-demuxer, Property 5: STREAMINFO Block Size Ordering**
        // **Validates: Requirements 3.8**
        test_property_streaminfo_block_size_ordering();
        
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

