/*
 * test_flac_sync_resync_properties.cpp - Property-based tests for FLAC sync resynchronization
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
// STANDALONE SYNC RESYNCHRONIZATION LOGIC
// ========================================

/**
 * RFC 9639 Section 9.1: FLAC frame sync code
 * The sync code is 15 bits: 0b111111111111100
 * This appears as:
 *   - 0xFF 0xF8 for fixed block size (blocking strategy bit = 0)
 *   - 0xFF 0xF9 for variable block size (blocking strategy bit = 1)
 */

/**
 * Result of sync resynchronization
 */
struct ResyncResult {
    bool found = false;           ///< True if sync code was found
    size_t offset = 0;            ///< Byte offset where sync was found
    bool is_variable = false;     ///< True if variable block size (0xFFF9)
    size_t bytes_searched = 0;    ///< Number of bytes searched
};

/**
 * Simulates sync resynchronization after sync loss
 * 
 * Implements Requirement 24.4: If frame sync is lost, resynchronize to
 * the next valid sync code.
 * 
 * @param buffer Pointer to data buffer
 * @param size Size of buffer in bytes
 * @param start_offset Offset to start searching from (simulating sync loss position)
 * @param max_search Maximum bytes to search
 * @param expected_variable Expected blocking strategy (for consistency check)
 * @param check_strategy Whether to enforce blocking strategy consistency
 * @return ResyncResult with resynchronization results
 */
ResyncResult resyncToNextFrame(const uint8_t* buffer, size_t size, 
                                size_t start_offset, size_t max_search = 4096,
                                bool expected_variable = false, bool check_strategy = false) {
    ResyncResult result;
    
    if (!buffer || size < 2 || start_offset >= size - 1) {
        return result;
    }
    
    // Calculate actual search range
    size_t search_end = std::min(start_offset + max_search, size - 1);
    
    // Search for sync pattern starting from start_offset
    for (size_t i = start_offset; i < search_end; ++i) {
        result.bytes_searched = i - start_offset + 1;
        
        // Look for sync pattern: 0xFF followed by 0xF8 or 0xF9
        if (buffer[i] == 0xFF) {
            uint8_t second_byte = buffer[i + 1];
            
            if (second_byte == 0xF8 || second_byte == 0xF9) {
                bool is_variable = (second_byte == 0xF9);
                
                // Check blocking strategy consistency if required
                if (check_strategy && is_variable != expected_variable) {
                    // Skip this sync - blocking strategy mismatch
                    continue;
                }
                
                result.found = true;
                result.offset = i;
                result.is_variable = is_variable;
                return result;
            }
        }
    }
    
    result.bytes_searched = search_end - start_offset;
    return result;
}

/**
 * Helper to create a buffer with corrupted data followed by valid sync
 * 
 * @param corruption_length Number of bytes of corruption before sync
 * @param is_variable Whether to use variable block size sync
 * @return Buffer with corruption followed by valid sync code
 */
std::vector<uint8_t> createCorruptedBuffer(size_t corruption_length, bool is_variable) {
    std::vector<uint8_t> buffer(corruption_length + 10);
    
    // Fill with random-looking corruption (avoiding accidental sync codes)
    for (size_t i = 0; i < corruption_length; ++i) {
        buffer[i] = static_cast<uint8_t>((i * 7 + 13) % 256);
        // Ensure no accidental sync codes
        if (buffer[i] == 0xFF && i + 1 < corruption_length) {
            buffer[i] = 0xFE;
        }
    }
    
    // Place valid sync code after corruption
    buffer[corruption_length] = 0xFF;
    buffer[corruption_length + 1] = is_variable ? 0xF9 : 0xF8;
    
    return buffer;
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

// ========================================
// PROPERTY 21: Error Recovery - Sync Resynchronization
// ========================================
// **Feature: flac-demuxer, Property 21: Error Recovery - Sync Resynchronization**
// **Validates: Requirements 24.4**
//
// For any stream where frame sync is lost, the FLAC Demuxer SHALL 
// resynchronize to the next valid sync code.

void test_property_sync_resynchronization() {
    std::cout << "\n=== Property 21: Error Recovery - Sync Resynchronization ===" << std::endl;
    std::cout << "Testing resynchronization to next valid sync code after sync loss..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: Resync after small corruption (< 100 bytes)
    // ----------------------------------------
    std::cout << "\n  Test 1: Resync after small corruption (< 100 bytes)..." << std::endl;
    {
        std::vector<size_t> corruption_sizes = {1, 5, 10, 50, 99};
        
        for (size_t corruption_size : corruption_sizes) {
            auto buffer = createCorruptedBuffer(corruption_size, false);
            
            tests_run++;
            
            ResyncResult result = resyncToNextFrame(buffer.data(), buffer.size(), 0);
            
            if (result.found && result.offset == corruption_size && !result.is_variable) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED with corruption=" << corruption_size 
                          << ": found=" << result.found 
                          << ", offset=" << result.offset << std::endl;
                assert(false && "Should resync after small corruption");
            }
        }
        std::cout << "    Resync successful after small corruption ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 2: Resync after medium corruption (100-1000 bytes)
    // ----------------------------------------
    std::cout << "\n  Test 2: Resync after medium corruption (100-1000 bytes)..." << std::endl;
    {
        std::vector<size_t> corruption_sizes = {100, 250, 500, 750, 1000};
        
        for (size_t corruption_size : corruption_sizes) {
            auto buffer = createCorruptedBuffer(corruption_size, true);
            
            tests_run++;
            
            ResyncResult result = resyncToNextFrame(buffer.data(), buffer.size(), 0);
            
            if (result.found && result.offset == corruption_size && result.is_variable) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED with corruption=" << corruption_size 
                          << ": found=" << result.found 
                          << ", offset=" << result.offset << std::endl;
                assert(false && "Should resync after medium corruption");
            }
        }
        std::cout << "    Resync successful after medium corruption ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 3: Resync after large corruption (1000-4000 bytes)
    // ----------------------------------------
    std::cout << "\n  Test 3: Resync after large corruption (1000-4000 bytes)..." << std::endl;
    {
        std::vector<size_t> corruption_sizes = {1000, 2000, 3000, 4000};
        
        for (size_t corruption_size : corruption_sizes) {
            auto buffer = createCorruptedBuffer(corruption_size, false);
            
            tests_run++;
            
            ResyncResult result = resyncToNextFrame(buffer.data(), buffer.size(), 0);
            
            if (result.found && result.offset == corruption_size) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED with corruption=" << corruption_size 
                          << ": found=" << result.found 
                          << ", offset=" << result.offset << std::endl;
                assert(false && "Should resync after large corruption");
            }
        }
        std::cout << "    Resync successful after large corruption ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 4: Resync fails when sync beyond max search range
    // ----------------------------------------
    std::cout << "\n  Test 4: Resync fails when sync beyond max search range..." << std::endl;
    {
        // Create buffer with sync at 5000 bytes, but max search is 4096
        auto buffer = createCorruptedBuffer(5000, false);
        
        tests_run++;
        
        ResyncResult result = resyncToNextFrame(buffer.data(), buffer.size(), 0, 4096);
        
        if (!result.found) {
            tests_passed++;
            std::cout << "    Resync correctly fails when sync beyond range ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Found sync at " << result.offset 
                      << " which is beyond max search range!" << std::endl;
            assert(false && "Should not find sync beyond max search range");
        }
    }
    
    // ----------------------------------------
    // Test 5: Resync from non-zero start offset
    // ----------------------------------------
    std::cout << "\n  Test 5: Resync from non-zero start offset..." << std::endl;
    {
        // Create buffer with sync at position 500
        std::vector<uint8_t> buffer(1000, 0x00);
        buffer[500] = 0xFF;
        buffer[501] = 0xF8;
        
        // Start searching from position 100
        tests_run++;
        
        ResyncResult result = resyncToNextFrame(buffer.data(), buffer.size(), 100);
        
        if (result.found && result.offset == 500) {
            tests_passed++;
            std::cout << "    Resync from offset 100 found sync at 500 ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: expected offset 500, got " << result.offset << std::endl;
            assert(false && "Should find sync from non-zero start offset");
        }
        
        // Start searching from position 600 (after the sync)
        tests_run++;
        
        result = resyncToNextFrame(buffer.data(), buffer.size(), 600);
        
        if (!result.found) {
            tests_passed++;
            std::cout << "    Resync from offset 600 correctly finds nothing ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Should not find sync when starting after it!" << std::endl;
            assert(false && "Should not find sync when starting after it");
        }
    }
    
    // ----------------------------------------
    // Test 6: Blocking strategy consistency during resync
    // ----------------------------------------
    std::cout << "\n  Test 6: Blocking strategy consistency during resync..." << std::endl;
    {
        // Create buffer with variable sync at 100, fixed sync at 200
        std::vector<uint8_t> buffer(500, 0x00);
        buffer[100] = 0xFF;
        buffer[101] = 0xF9;  // Variable
        buffer[200] = 0xFF;
        buffer[201] = 0xF8;  // Fixed
        
        // Search expecting fixed - should skip variable and find fixed
        tests_run++;
        
        ResyncResult result = resyncToNextFrame(buffer.data(), buffer.size(), 0, 
                                                 4096, false, true);  // expect fixed
        
        if (result.found && result.offset == 200 && !result.is_variable) {
            tests_passed++;
            std::cout << "    Skipped variable sync, found fixed at 200 ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: expected fixed at 200, got " 
                      << (result.is_variable ? "variable" : "fixed") 
                      << " at " << result.offset << std::endl;
            assert(false && "Should skip mismatched blocking strategy");
        }
        
        // Search expecting variable - should find variable at 100
        tests_run++;
        
        result = resyncToNextFrame(buffer.data(), buffer.size(), 0, 
                                   4096, true, true);  // expect variable
        
        if (result.found && result.offset == 100 && result.is_variable) {
            tests_passed++;
            std::cout << "    Found variable sync at 100 ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: expected variable at 100, got " 
                      << (result.is_variable ? "variable" : "fixed") 
                      << " at " << result.offset << std::endl;
            assert(false && "Should find matching blocking strategy");
        }
    }
    
    // ----------------------------------------
    // Test 7: Multiple sync codes - finds first valid one
    // ----------------------------------------
    std::cout << "\n  Test 7: Multiple sync codes - finds first valid one..." << std::endl;
    {
        std::vector<uint8_t> buffer(500, 0x00);
        // Place sync codes at 50, 100, 150
        buffer[50] = 0xFF; buffer[51] = 0xF8;
        buffer[100] = 0xFF; buffer[101] = 0xF9;
        buffer[150] = 0xFF; buffer[151] = 0xF8;
        
        tests_run++;
        
        ResyncResult result = resyncToNextFrame(buffer.data(), buffer.size(), 0);
        
        if (result.found && result.offset == 50) {
            tests_passed++;
            std::cout << "    Found first sync at 50 ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: expected first sync at 50, got " << result.offset << std::endl;
            assert(false && "Should find first sync code");
        }
        
        // Start after first sync
        tests_run++;
        
        result = resyncToNextFrame(buffer.data(), buffer.size(), 52);
        
        if (result.found && result.offset == 100) {
            tests_passed++;
            std::cout << "    Found second sync at 100 when starting at 52 ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: expected sync at 100, got " << result.offset << std::endl;
            assert(false && "Should find next sync code");
        }
    }
    
    // ----------------------------------------
    // Test 8: Random corruption patterns (100 iterations)
    // ----------------------------------------
    std::cout << "\n  Test 8: Random corruption patterns (100 iterations)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> corruption_dist(1, 3000);
        std::uniform_int_distribution<> type_dist(0, 1);
        std::uniform_int_distribution<> byte_dist(0, 255);
        
        int random_passed = 0;
        
        for (int i = 0; i < 100; ++i) {
            size_t corruption_size = static_cast<size_t>(corruption_dist(gen));
            bool is_variable = (type_dist(gen) == 1);
            
            // Create buffer with random corruption
            std::vector<uint8_t> buffer(corruption_size + 100);
            for (size_t j = 0; j < corruption_size; ++j) {
                buffer[j] = static_cast<uint8_t>(byte_dist(gen));
            }
            
            // Clear ALL accidental sync codes in the corruption region
            // This is a two-pass approach to ensure no sync codes remain
            for (size_t j = 0; j < corruption_size; ++j) {
                if (buffer[j] == 0xFF && j + 1 < corruption_size) {
                    if (buffer[j + 1] == 0xF8 || buffer[j + 1] == 0xF9) {
                        buffer[j + 1] = 0x00;
                    }
                }
            }
            
            // Place valid sync after corruption
            buffer[corruption_size] = 0xFF;
            buffer[corruption_size + 1] = is_variable ? 0xF9 : 0xF8;
            
            tests_run++;
            
            ResyncResult result = resyncToNextFrame(buffer.data(), buffer.size(), 0);
            
            if (result.found && result.offset == corruption_size && result.is_variable == is_variable) {
                tests_passed++;
                random_passed++;
            } else {
                std::cerr << "    FAILED iteration " << i << ": corruption=" << corruption_size
                          << ", expected offset=" << corruption_size 
                          << ", got offset=" << result.offset << std::endl;
                assert(false && "Should resync after random corruption");
            }
        }
        std::cout << "    " << random_passed << "/100 random tests passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 9: Edge cases - null pointer and empty buffer
    // ----------------------------------------
    std::cout << "\n  Test 9: Edge cases - null pointer and empty buffer..." << std::endl;
    {
        tests_run++;
        ResyncResult result = resyncToNextFrame(nullptr, 0, 0);
        if (!result.found) {
            tests_passed++;
            std::cout << "    Null pointer handled safely ✓" << std::endl;
        } else {
            assert(false && "Null pointer should return not found");
        }
        
        tests_run++;
        std::vector<uint8_t> empty;
        result = resyncToNextFrame(empty.data(), 0, 0);
        if (!result.found) {
            tests_passed++;
            std::cout << "    Empty buffer handled safely ✓" << std::endl;
        } else {
            assert(false && "Empty buffer should return not found");
        }
        
        tests_run++;
        std::vector<uint8_t> single = {0xFF};
        result = resyncToNextFrame(single.data(), 1, 0);
        if (!result.found) {
            tests_passed++;
            std::cout << "    Single byte buffer handled safely ✓" << std::endl;
        } else {
            assert(false && "Single byte buffer should return not found");
        }
    }
    
    // ----------------------------------------
    // Test 10: Bytes searched tracking
    // ----------------------------------------
    std::cout << "\n  Test 10: Bytes searched tracking..." << std::endl;
    {
        auto buffer = createCorruptedBuffer(500, false);
        
        tests_run++;
        
        ResyncResult result = resyncToNextFrame(buffer.data(), buffer.size(), 0);
        
        // Should have searched at least 500 bytes to find the sync
        if (result.found && result.bytes_searched >= 500) {
            tests_passed++;
            std::cout << "    Bytes searched (" << result.bytes_searched << ") tracked correctly ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: bytes_searched=" << result.bytes_searched 
                      << " (expected >= 500)" << std::endl;
            assert(false && "Bytes searched should be tracked");
        }
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 21: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC SYNC RESYNCHRONIZATION PROPERTY-BASED TESTS" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // Property 21: Error Recovery - Sync Resynchronization
        // **Feature: flac-demuxer, Property 21: Error Recovery - Sync Resynchronization**
        // **Validates: Requirements 24.4**
        test_property_sync_resynchronization();
        
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
