/*
 * test_flac_frame_boundary_search_properties.cpp - Property-based tests for FLAC frame boundary search
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * **Feature: flac-bisection-seeking, Property 2: Frame Sync Detection (RFC 9639 Section 9.1)**
 * **Validates: Requirements 2.1, 2.2**
 *
 * For any byte buffer containing a valid FLAC frame sync pattern (0xFF followed by 
 * 0xF8 or 0xF9), the frame finder SHALL locate the sync pattern and return its position.
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
// STANDALONE FRAME BOUNDARY SEARCH IMPLEMENTATION
// ========================================

/**
 * RFC 9639 Section 9.1: FLAC frame sync code
 * The sync code is 15 bits: 0b111111111111100
 * This appears as:
 *   - 0xFF 0xF8 for fixed block size (blocking strategy bit = 0)
 *   - 0xFF 0xF9 for variable block size (blocking strategy bit = 1)
 */

/**
 * Result of frame boundary search
 */
struct FrameBoundaryResult {
    bool found = false;           ///< True if valid frame was found
    size_t frame_pos = 0;         ///< Byte offset where frame starts
    uint64_t frame_sample = 0;    ///< Sample offset of the frame
    uint32_t block_size = 0;      ///< Block size of the frame
    bool is_variable = false;     ///< True if variable block size (0xFFF9)
};

/**
 * Searches for a valid FLAC frame sync pattern in a buffer
 * 
 * Implements RFC 9639 Section 9.1 frame discovery:
 * - Requirement 2.1: Search forward for 15-bit sync pattern (0xFFF8 or 0xFFF9)
 * - Requirement 2.2: Verify blocking strategy bit matches stream's established strategy
 * - Requirement 2.7: Report failure if no valid frame found within search limit
 * - Requirement 2.8: Continue searching past false positive sync patterns
 * 
 * @param buffer Pointer to data buffer
 * @param size Size of buffer in bytes
 * @param max_search Maximum bytes to search (default 64KB per Requirement 2.7)
 * @param expected_variable Expected blocking strategy (-1 = any, 0 = fixed, 1 = variable)
 * @return FrameBoundaryResult with search results
 */
FrameBoundaryResult findFrameBoundary(const uint8_t* buffer, size_t size, 
                                       size_t max_search = 65536,
                                       int expected_variable = -1) {
    FrameBoundaryResult result;
    
    if (!buffer || size < 2) {
        return result;
    }
    
    // Requirement 2.7: Limit search to max_search bytes (64KB default)
    size_t search_limit = std::min(size - 1, max_search);
    
    // Requirement 2.1: Search for 15-bit sync pattern
    for (size_t i = 0; i < search_limit; ++i) {
        // Look for 0xFF followed by 0xF8 or 0xF9
        if (buffer[i] == 0xFF) {
            uint8_t second_byte = buffer[i + 1];
            
            if (second_byte == 0xF8 || second_byte == 0xF9) {
                bool is_variable = (second_byte == 0xF9);
                
                // Requirement 2.2: Verify blocking strategy matches if specified
                if (expected_variable != -1) {
                    bool expected_var = (expected_variable == 1);
                    if (is_variable != expected_var) {
                        // Requirement 2.8: Continue searching past mismatched sync
                        continue;
                    }
                }
                
                // Found valid sync pattern
                result.found = true;
                result.frame_pos = i;
                result.is_variable = is_variable;
                
                // For this standalone test, we simulate sample offset and block size
                // In the real implementation, these come from parsing the frame header
                result.frame_sample = 0;  // Would be parsed from coded number
                result.block_size = 4096; // Would be parsed from header
                
                return result;
            }
        }
    }
    
    // Requirement 2.7: No valid frame found within search limit
    return result;
}

/**
 * Helper to format bytes as hex string for debugging
 */
std::string bytesToHex(const uint8_t* bytes, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len && i < 16; ++i) {
        if (i > 0) oss << " ";
        oss << "0x" << std::hex << std::setw(2) << std::setfill('0') 
            << static_cast<int>(bytes[i]);
    }
    if (len > 16) oss << " ...";
    return oss.str();
}

// ========================================
// PROPERTY-BASED TESTS
// ========================================

/**
 * **Feature: flac-bisection-seeking, Property 2: Frame Sync Detection (RFC 9639 Section 9.1)**
 * **Validates: Requirements 2.1, 2.2**
 *
 * For any byte buffer containing a valid FLAC frame sync pattern (0xFF followed by 
 * 0xF8 or 0xF9), the frame finder SHALL locate the sync pattern and return its position.
 */
void test_property_frame_sync_detection_for_bisection() {
    std::cout << "\n=== Property 2: Frame Sync Detection (RFC 9639 Section 9.1) ===" << std::endl;
    std::cout << "For any buffer with valid sync pattern, finder SHALL locate it..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: Fixed block size sync (0xFFF8) at various positions
    // ----------------------------------------
    std::cout << "\n  Test 1: Fixed block size sync (0xFFF8) detection..." << std::endl;
    {
        std::vector<size_t> positions = {0, 1, 10, 100, 1000, 10000, 60000};
        
        for (size_t pos : positions) {
            std::vector<uint8_t> buffer(pos + 100, 0x00);
            buffer[pos] = 0xFF;
            buffer[pos + 1] = 0xF8;
            
            tests_run++;
            
            FrameBoundaryResult result = findFrameBoundary(buffer.data(), buffer.size());
            
            if (result.found && result.frame_pos == pos && !result.is_variable) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED at position " << pos << ": found=" << result.found 
                          << ", frame_pos=" << result.frame_pos << std::endl;
                assert(false && "Fixed sync code should be detected");
            }
        }
        std::cout << "    Fixed sync (0xFFF8) detected at all positions ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 2: Variable block size sync (0xFFF9) at various positions
    // ----------------------------------------
    std::cout << "\n  Test 2: Variable block size sync (0xFFF9) detection..." << std::endl;
    {
        std::vector<size_t> positions = {0, 1, 10, 100, 1000, 10000, 60000};
        
        for (size_t pos : positions) {
            std::vector<uint8_t> buffer(pos + 100, 0x00);
            buffer[pos] = 0xFF;
            buffer[pos + 1] = 0xF9;
            
            tests_run++;
            
            FrameBoundaryResult result = findFrameBoundary(buffer.data(), buffer.size());
            
            if (result.found && result.frame_pos == pos && result.is_variable) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED at position " << pos << ": found=" << result.found 
                          << ", frame_pos=" << result.frame_pos << std::endl;
                assert(false && "Variable sync code should be detected");
            }
        }
        std::cout << "    Variable sync (0xFFF9) detected at all positions ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 3: Blocking strategy filtering (Requirement 2.2)
    // ----------------------------------------
    std::cout << "\n  Test 3: Blocking strategy filtering (Requirement 2.2)..." << std::endl;
    {
        // Buffer with fixed sync at position 10, variable sync at position 50
        std::vector<uint8_t> buffer(100, 0x00);
        buffer[10] = 0xFF; buffer[11] = 0xF8;  // Fixed at 10
        buffer[50] = 0xFF; buffer[51] = 0xF9;  // Variable at 50
        
        // When expecting fixed, should find position 10
        tests_run++;
        FrameBoundaryResult result = findFrameBoundary(buffer.data(), buffer.size(), 65536, 0);
        if (result.found && result.frame_pos == 10 && !result.is_variable) {
            tests_passed++;
            std::cout << "    Expected fixed, found fixed at 10 ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Expected fixed at 10, got pos=" << result.frame_pos << std::endl;
            assert(false && "Should find fixed sync when expecting fixed");
        }
        
        // When expecting variable, should skip fixed and find position 50
        tests_run++;
        result = findFrameBoundary(buffer.data(), buffer.size(), 65536, 1);
        if (result.found && result.frame_pos == 50 && result.is_variable) {
            tests_passed++;
            std::cout << "    Expected variable, found variable at 50 ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Expected variable at 50, got pos=" << result.frame_pos << std::endl;
            assert(false && "Should find variable sync when expecting variable");
        }
        
        // When expecting any, should find first (fixed at 10)
        tests_run++;
        result = findFrameBoundary(buffer.data(), buffer.size(), 65536, -1);
        if (result.found && result.frame_pos == 10) {
            tests_passed++;
            std::cout << "    Expected any, found first at 10 ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Expected first at 10, got pos=" << result.frame_pos << std::endl;
            assert(false && "Should find first sync when expecting any");
        }
    }
    
    // ----------------------------------------
    // Test 4: 64KB search limit (Requirement 2.7)
    // ----------------------------------------
    std::cout << "\n  Test 4: 64KB search limit (Requirement 2.7)..." << std::endl;
    {
        // Sync beyond 64KB should not be found
        std::vector<uint8_t> buffer(70000, 0x00);
        buffer[66000] = 0xFF;
        buffer[66001] = 0xF8;
        
        tests_run++;
        FrameBoundaryResult result = findFrameBoundary(buffer.data(), buffer.size(), 65536);
        if (!result.found) {
            tests_passed++;
            std::cout << "    Sync at 66000 not found (beyond 64KB limit) ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Sync beyond 64KB was found!" << std::endl;
            assert(false && "Sync beyond search limit should not be found");
        }
        
        // Sync within 64KB should be found
        buffer[64000] = 0xFF;
        buffer[64001] = 0xF8;
        
        tests_run++;
        result = findFrameBoundary(buffer.data(), buffer.size(), 65536);
        if (result.found && result.frame_pos == 64000) {
            tests_passed++;
            std::cout << "    Sync at 64000 found (within 64KB limit) ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Sync within 64KB was not found!" << std::endl;
            assert(false && "Sync within search limit should be found");
        }
    }
    
    // ----------------------------------------
    // Test 5: Invalid patterns must not be detected
    // ----------------------------------------
    std::cout << "\n  Test 5: Invalid sync patterns rejection..." << std::endl;
    {
        struct InvalidPattern {
            uint8_t bytes[2];
            const char* description;
        };
        
        std::vector<InvalidPattern> invalid_patterns = {
            {{0xFF, 0xF0}, "0xFFF0 (wrong low nibble)"},
            {{0xFF, 0xFA}, "0xFFFA (reserved)"},
            {{0xFF, 0xFB}, "0xFFFB (MP3 sync)"},
            {{0xFF, 0xFC}, "0xFFFC (reserved)"},
            {{0xFF, 0xFD}, "0xFFFD (reserved)"},
            {{0xFF, 0xFE}, "0xFFFE (reserved)"},
            {{0xFF, 0xFF}, "0xFFFF (all ones)"},
            {{0xFE, 0xF8}, "0xFEF8 (wrong first byte)"},
        };
        
        for (const auto& pattern : invalid_patterns) {
            std::vector<uint8_t> buffer = {pattern.bytes[0], pattern.bytes[1], 0x00, 0x00};
            
            tests_run++;
            FrameBoundaryResult result = findFrameBoundary(buffer.data(), buffer.size());
            
            if (!result.found) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED: " << pattern.description << " was detected!" << std::endl;
                assert(false && "Invalid sync pattern should not be detected");
            }
        }
        std::cout << "    All invalid patterns rejected ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 6: Random data with embedded sync codes (100 iterations)
    // ----------------------------------------
    std::cout << "\n  Test 6: Random data with embedded sync codes (100 iterations)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> byte_dist(0, 255);
        std::uniform_int_distribution<> pos_dist(0, 60000);
        std::uniform_int_distribution<> type_dist(0, 1);
        
        int random_passed = 0;
        
        for (int i = 0; i < 100; ++i) {
            // Generate random buffer
            std::vector<uint8_t> buffer(65000);
            for (auto& b : buffer) {
                b = static_cast<uint8_t>(byte_dist(gen));
            }
            
            // Clear any accidental sync codes
            for (size_t j = 0; j < buffer.size() - 1; ++j) {
                if (buffer[j] == 0xFF && (buffer[j+1] == 0xF8 || buffer[j+1] == 0xF9)) {
                    buffer[j+1] = 0x00;
                }
            }
            
            // Insert sync code at random position
            size_t sync_pos = static_cast<size_t>(pos_dist(gen));
            bool is_variable = (type_dist(gen) == 1);
            buffer[sync_pos] = 0xFF;
            buffer[sync_pos + 1] = is_variable ? 0xF9 : 0xF8;
            
            tests_run++;
            
            FrameBoundaryResult result = findFrameBoundary(buffer.data(), buffer.size());
            
            if (result.found && result.frame_pos == sync_pos && result.is_variable == is_variable) {
                tests_passed++;
                random_passed++;
            } else {
                std::cerr << "    FAILED iteration " << i << ": expected pos=" << sync_pos 
                          << ", got pos=" << result.frame_pos << std::endl;
                assert(false && "Embedded sync code should be detected");
            }
        }
        std::cout << "    " << random_passed << "/100 random tests passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 7: Edge cases - null, empty, single byte
    // ----------------------------------------
    std::cout << "\n  Test 7: Edge cases handling..." << std::endl;
    {
        tests_run++;
        FrameBoundaryResult result = findFrameBoundary(nullptr, 0);
        if (!result.found) {
            tests_passed++;
            std::cout << "    Null pointer handled safely ✓" << std::endl;
        } else {
            assert(false && "Null pointer should return not found");
        }
        
        tests_run++;
        std::vector<uint8_t> empty;
        result = findFrameBoundary(empty.data(), 0);
        if (!result.found) {
            tests_passed++;
            std::cout << "    Empty buffer handled safely ✓" << std::endl;
        } else {
            assert(false && "Empty buffer should return not found");
        }
        
        tests_run++;
        std::vector<uint8_t> single = {0xFF};
        result = findFrameBoundary(single.data(), 1);
        if (!result.found) {
            tests_passed++;
            std::cout << "    Single byte buffer handled safely ✓" << std::endl;
        } else {
            assert(false && "Single byte buffer should return not found");
        }
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 2: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC FRAME BOUNDARY SEARCH PROPERTY-BASED TESTS" << std::endl;
    std::cout << "Feature: flac-bisection-seeking" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // **Feature: flac-bisection-seeking, Property 2: Frame Sync Detection (RFC 9639 Section 9.1)**
        // **Validates: Requirements 2.1, 2.2**
        test_property_frame_sync_detection_for_bisection();
        
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
