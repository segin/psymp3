/*
 * test_flac_frame_boundary_search_properties.cpp - Property-based tests for FLAC frame boundary search limit
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
#include <algorithm>

// ========================================
// STANDALONE FRAME BOUNDARY SEARCH
// ========================================

/**
 * Maximum search scope for frame boundary detection
 * Per Requirement 21.3: Limit search scope to 512 bytes maximum
 */
static constexpr size_t MAX_SEARCH_BYTES = 512;

/**
 * FLAC frame sync codes per RFC 9639 Section 9.1
 * - 0xFFF8: Fixed block size
 * - 0xFFF9: Variable block size
 */
static constexpr uint8_t SYNC_BYTE_1 = 0xFF;
static constexpr uint8_t SYNC_BYTE_2_FIXED = 0xF8;
static constexpr uint8_t SYNC_BYTE_2_VARIABLE = 0xF9;

/**
 * Result of frame boundary search
 */
struct SearchResult {
    bool found = false;
    size_t position = 0;
    size_t bytes_searched = 0;
    bool is_variable_block_size = false;
};

/**
 * Search for FLAC frame sync code within a buffer
 * 
 * Implements Requirement 21.3: Limit search scope to 512 bytes maximum
 * 
 * @param buffer Data buffer to search
 * @param buffer_size Size of buffer
 * @return SearchResult with position if found
 */
SearchResult findFrameSync(const uint8_t* buffer, size_t buffer_size) {
    SearchResult result;
    
    if (!buffer || buffer_size < 2) {
        result.bytes_searched = buffer_size;
        return result;
    }
    
    // Requirement 21.3: Limit search scope to 512 bytes maximum
    size_t search_limit = std::min(buffer_size, MAX_SEARCH_BYTES);
    
    for (size_t i = 0; i < search_limit - 1; ++i) {
        if (buffer[i] == SYNC_BYTE_1) {
            if (buffer[i + 1] == SYNC_BYTE_2_FIXED) {
                result.found = true;
                result.position = i;
                result.bytes_searched = i + 2;
                result.is_variable_block_size = false;
                return result;
            }
            if (buffer[i + 1] == SYNC_BYTE_2_VARIABLE) {
                result.found = true;
                result.position = i;
                result.bytes_searched = i + 2;
                result.is_variable_block_size = true;
                return result;
            }
        }
    }
    
    result.bytes_searched = search_limit;
    return result;
}

/**
 * Helper to format bytes as hex string for debugging
 */
std::string bytesToHex(const uint8_t* bytes, size_t len, size_t max_display = 16) {
    std::ostringstream oss;
    size_t display_len = std::min(len, max_display);
    for (size_t i = 0; i < display_len; ++i) {
        if (i > 0) oss << " ";
        oss << "0x" << std::hex << std::setw(2) << std::setfill('0') 
            << static_cast<int>(bytes[i]);
    }
    if (len > max_display) {
        oss << " ... (" << len << " bytes total)";
    }
    return oss.str();
}

// ========================================
// PROPERTY-BASED TESTS
// ========================================

// ========================================
// PROPERTY 19: Frame Boundary Search Limit
// ========================================
// **Feature: flac-demuxer, Property 19: Frame Boundary Search Limit**
// **Validates: Requirements 21.3**
//
// For any frame boundary detection operation, the search scope SHALL be 
// limited to 512 bytes maximum.

void test_property_frame_boundary_search_limit() {
    std::cout << "\n=== Property 19: Frame Boundary Search Limit ===" << std::endl;
    std::cout << "Testing that frame boundary search is limited to 512 bytes..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> byte_dist(0, 255);
    
    // ----------------------------------------
    // Test 1: Search stops at 512 bytes when no sync found
    // ----------------------------------------
    std::cout << "\n  Test 1: Search stops at 512 bytes when no sync found..." << std::endl;
    {
        // Create buffer larger than 512 bytes with no sync code
        std::vector<uint8_t> buffer(1024);
        for (auto& b : buffer) {
            b = byte_dist(gen);
            // Ensure no sync code
            if (b == 0xFF) b = 0xFE;
        }
        
        tests_run++;
        SearchResult result = findFrameSync(buffer.data(), buffer.size());
        
        if (!result.found && result.bytes_searched == MAX_SEARCH_BYTES) {
            tests_passed++;
            std::cout << "    Search stopped at " << result.bytes_searched << " bytes ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: bytes_searched=" << result.bytes_searched 
                      << ", expected " << MAX_SEARCH_BYTES << std::endl;
            assert(false && "Search should stop at 512 bytes");
        }
    }
    
    // ----------------------------------------
    // Test 2: Sync code at position 0 is found immediately
    // ----------------------------------------
    std::cout << "\n  Test 2: Sync code at position 0 is found immediately..." << std::endl;
    {
        std::vector<uint8_t> buffer(1024);
        buffer[0] = 0xFF;
        buffer[1] = 0xF8;  // Fixed block size sync
        
        tests_run++;
        SearchResult result = findFrameSync(buffer.data(), buffer.size());
        
        if (result.found && result.position == 0 && result.bytes_searched == 2) {
            tests_passed++;
            std::cout << "    Found at position 0, searched " << result.bytes_searched << " bytes ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: found=" << result.found << ", position=" << result.position 
                      << ", bytes_searched=" << result.bytes_searched << std::endl;
            assert(false && "Sync at position 0 should be found immediately");
        }
    }
    
    // ----------------------------------------
    // Test 3: Sync code within 512 bytes is found
    // ----------------------------------------
    std::cout << "\n  Test 3: Sync code within 512 bytes is found..." << std::endl;
    {
        std::vector<size_t> test_positions = {1, 10, 100, 256, 500, 510};
        
        for (size_t pos : test_positions) {
            std::vector<uint8_t> buffer(1024, 0x00);
            buffer[pos] = 0xFF;
            buffer[pos + 1] = 0xF9;  // Variable block size sync
            
            tests_run++;
            SearchResult result = findFrameSync(buffer.data(), buffer.size());
            
            if (result.found && result.position == pos) {
                tests_passed++;
                std::cout << "    Position " << pos << ": found at " << result.position 
                          << ", searched " << result.bytes_searched << " bytes ✓" << std::endl;
            } else {
                std::cerr << "    FAILED at position " << pos << ": found=" << result.found 
                          << ", position=" << result.position << std::endl;
                assert(false && "Sync within 512 bytes should be found");
            }
        }
    }
    
    // ----------------------------------------
    // Test 4: Sync code beyond 512 bytes is NOT found
    // ----------------------------------------
    std::cout << "\n  Test 4: Sync code beyond 512 bytes is NOT found..." << std::endl;
    {
        std::vector<size_t> test_positions = {512, 513, 600, 800, 1000};
        
        for (size_t pos : test_positions) {
            std::vector<uint8_t> buffer(1024, 0x00);
            buffer[pos] = 0xFF;
            buffer[pos + 1] = 0xF8;
            
            tests_run++;
            SearchResult result = findFrameSync(buffer.data(), buffer.size());
            
            if (!result.found && result.bytes_searched == MAX_SEARCH_BYTES) {
                tests_passed++;
                std::cout << "    Position " << pos << ": not found (beyond limit), searched " 
                          << result.bytes_searched << " bytes ✓" << std::endl;
            } else {
                std::cerr << "    FAILED at position " << pos << ": found=" << result.found 
                          << ", bytes_searched=" << result.bytes_searched << std::endl;
                assert(false && "Sync beyond 512 bytes should not be found");
            }
        }
    }
    
    // ----------------------------------------
    // Test 5: Random data with sync at random positions
    // ----------------------------------------
    std::cout << "\n  Test 5: Random data with sync at random positions (100 iterations)..." << std::endl;
    {
        std::uniform_int_distribution<size_t> pos_dist(0, 1023);
        
        int found_within_limit = 0;
        int not_found_beyond_limit = 0;
        
        for (int i = 0; i < 100; ++i) {
            std::vector<uint8_t> buffer(1024);
            for (auto& b : buffer) {
                b = byte_dist(gen);
                if (b == 0xFF) b = 0xFE;  // Remove accidental sync codes
            }
            
            // Place sync at random position
            size_t sync_pos = pos_dist(gen);
            if (sync_pos < 1023) {
                buffer[sync_pos] = 0xFF;
                buffer[sync_pos + 1] = (gen() % 2) ? 0xF8 : 0xF9;
            }
            
            tests_run++;
            SearchResult result = findFrameSync(buffer.data(), buffer.size());
            
            bool expected_found = (sync_pos < MAX_SEARCH_BYTES - 1);
            
            if (expected_found) {
                if (result.found && result.position == sync_pos) {
                    tests_passed++;
                    found_within_limit++;
                } else {
                    std::cerr << "    FAILED: sync at " << sync_pos << " should be found" << std::endl;
                    assert(false);
                }
            } else {
                if (!result.found && result.bytes_searched == MAX_SEARCH_BYTES) {
                    tests_passed++;
                    not_found_beyond_limit++;
                } else {
                    std::cerr << "    FAILED: sync at " << sync_pos << " should not be found" << std::endl;
                    assert(false);
                }
            }
        }
        
        std::cout << "    Found within limit: " << found_within_limit << std::endl;
        std::cout << "    Not found beyond limit: " << not_found_beyond_limit << std::endl;
        std::cout << "    All 100 random tests passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 6: Small buffers (less than 512 bytes)
    // ----------------------------------------
    std::cout << "\n  Test 6: Small buffers (less than 512 bytes)..." << std::endl;
    {
        std::vector<size_t> buffer_sizes = {2, 10, 50, 100, 256, 511};
        
        for (size_t buf_size : buffer_sizes) {
            std::vector<uint8_t> buffer(buf_size, 0x00);
            
            tests_run++;
            SearchResult result = findFrameSync(buffer.data(), buffer.size());
            
            // Should search entire buffer (no sync present)
            if (!result.found && result.bytes_searched == buf_size) {
                tests_passed++;
                std::cout << "    Buffer size " << buf_size << ": searched " 
                          << result.bytes_searched << " bytes ✓" << std::endl;
            } else {
                std::cerr << "    FAILED: buffer size " << buf_size 
                          << ", bytes_searched=" << result.bytes_searched << std::endl;
                assert(false && "Small buffer should be fully searched");
            }
        }
    }
    
    // ----------------------------------------
    // Test 7: Edge case - sync at position 510 (last valid position)
    // ----------------------------------------
    std::cout << "\n  Test 7: Edge case - sync at position 510..." << std::endl;
    {
        std::vector<uint8_t> buffer(1024, 0x00);
        buffer[510] = 0xFF;
        buffer[511] = 0xF8;
        
        tests_run++;
        SearchResult result = findFrameSync(buffer.data(), buffer.size());
        
        if (result.found && result.position == 510) {
            tests_passed++;
            std::cout << "    Sync at position 510 found ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: found=" << result.found << ", position=" << result.position << std::endl;
            assert(false && "Sync at position 510 should be found");
        }
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 19: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// PROPERTY 19b: Exhaustive Position Test
// ========================================
// Test all positions from 0 to 600 to verify boundary behavior

void test_property_exhaustive_position_test() {
    std::cout << "\n=== Property 19b: Exhaustive Position Test ===" << std::endl;
    std::cout << "Testing sync detection at all positions 0-600..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    for (size_t pos = 0; pos <= 600; ++pos) {
        std::vector<uint8_t> buffer(1024, 0x00);
        
        if (pos < 1023) {
            buffer[pos] = 0xFF;
            buffer[pos + 1] = 0xF8;
        }
        
        tests_run++;
        SearchResult result = findFrameSync(buffer.data(), buffer.size());
        
        bool expected_found = (pos < MAX_SEARCH_BYTES - 1);
        
        if (expected_found) {
            if (result.found && result.position == pos) {
                tests_passed++;
            } else {
                std::cerr << "  FAILED at position " << pos << ": expected found, got found=" 
                          << result.found << ", position=" << result.position << std::endl;
                assert(false);
            }
        } else {
            if (!result.found) {
                tests_passed++;
            } else {
                std::cerr << "  FAILED at position " << pos << ": expected not found, got found=" 
                          << result.found << ", position=" << result.position << std::endl;
                assert(false);
            }
        }
    }
    
    std::cout << "  " << tests_passed << "/" << tests_run << " positions tested correctly ✓" << std::endl;
    std::cout << "\n✓ Property 19b: Exhaustive position test PASSED" << std::endl;
    
    assert(tests_passed == tests_run);
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC FRAME BOUNDARY SEARCH PROPERTY-BASED TESTS" << std::endl;
    std::cout << "**Feature: flac-demuxer, Property 19: Frame Boundary Search Limit**" << std::endl;
    std::cout << "**Validates: Requirements 21.3**" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // Property 19: Frame Boundary Search Limit
        test_property_frame_boundary_search_limit();
        
        // Property 19b: Exhaustive position test
        test_property_exhaustive_position_test();
        
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
