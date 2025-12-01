/*
 * test_flac_frame_sync_properties.cpp - Property-based tests for FLAC frame sync code detection
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
// STANDALONE FRAME SYNC CODE DETECTION
// ========================================

/**
 * RFC 9639 Section 9.1: FLAC frame sync code
 * The sync code is 15 bits: 0b111111111111100
 * This appears as:
 *   - 0xFF 0xF8 for fixed block size (blocking strategy bit = 0)
 *   - 0xFF 0xF9 for variable block size (blocking strategy bit = 1)
 */

/**
 * Result of frame sync detection
 */
struct FrameSyncResult {
    bool found = false;           ///< True if sync code was found
    size_t offset = 0;            ///< Byte offset where sync was found
    bool is_variable = false;     ///< True if variable block size (0xFFF9)
    bool is_byte_aligned = true;  ///< True if sync is byte-aligned
};

/**
 * Detects FLAC frame sync code in a byte buffer
 * 
 * RFC 9639 Section 9.1:
 * - Sync code is 15 bits: 0b111111111111100
 * - Followed by 1-bit blocking strategy (0=fixed, 1=variable)
 * - Must be byte-aligned
 * 
 * @param buffer Pointer to data buffer
 * @param size Size of buffer in bytes
 * @param max_search Maximum bytes to search (per Requirement 21.3: 512 bytes)
 * @return FrameSyncResult with detection results
 */
FrameSyncResult detectFrameSync(const uint8_t* buffer, size_t size, size_t max_search = 512) {
    FrameSyncResult result;
    
    if (!buffer || size < 2) {
        return result;
    }
    
    // Limit search scope per Requirement 21.3
    size_t search_limit = std::min(size - 1, max_search);
    
    // Search for sync pattern (byte-aligned)
    for (size_t i = 0; i < search_limit; ++i) {
        // Requirement 4.1: Look for 15-bit pattern 0b111111111111100
        // This appears as 0xFF followed by 0xF8 or 0xF9
        if (buffer[i] == 0xFF) {
            uint8_t second_byte = buffer[i + 1];
            
            // Requirement 4.6: Fixed block size expects 0xFF 0xF8
            // Requirement 4.7: Variable block size expects 0xFF 0xF9
            if (second_byte == 0xF8 || second_byte == 0xF9) {
                result.found = true;
                result.offset = i;
                result.is_variable = (second_byte == 0xF9);
                result.is_byte_aligned = true;  // We only search byte-aligned positions
                return result;
            }
        }
    }
    
    return result;
}

/**
 * Validates that a sync code is at a byte-aligned position
 * 
 * @param offset Byte offset of the sync code
 * @return true if byte-aligned (always true for byte offsets)
 */
bool isByteAligned(size_t offset) {
    // Byte offsets are always byte-aligned by definition
    // This function exists to document the requirement
    return true;
}

/**
 * Extracts blocking strategy from sync code
 * 
 * @param sync_bytes Pointer to 2-byte sync code
 * @return true if variable block size, false if fixed
 */
bool extractBlockingStrategy(const uint8_t* sync_bytes) {
    if (!sync_bytes) return false;
    
    // Requirement 4.3: Extract blocking strategy bit
    // 0xFFF8 = fixed (bit = 0)
    // 0xFFF9 = variable (bit = 1)
    return (sync_bytes[1] == 0xF9);
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
// PROPERTY 6: Frame Sync Code Detection
// ========================================
// **Feature: flac-demuxer, Property 6: Frame Sync Code Detection**
// **Validates: Requirements 4.1, 4.2**
//
// For any byte sequence, the FLAC Demuxer SHALL correctly identify the 
// 15-bit sync pattern 0b111111111111100 at byte-aligned positions.

void test_property_frame_sync_detection() {
    std::cout << "\n=== Property 6: Frame Sync Code Detection ===" << std::endl;
    std::cout << "Testing detection of 15-bit sync pattern 0b111111111111100..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: Valid fixed block size sync (0xFFF8) detection
    // ----------------------------------------
    std::cout << "\n  Test 1: Fixed block size sync (0xFFF8) detection..." << std::endl;
    {
        // Create buffer with sync code at various positions
        std::vector<size_t> positions = {0, 1, 10, 100, 255, 500};
        
        for (size_t pos : positions) {
            std::vector<uint8_t> buffer(pos + 10, 0x00);
            buffer[pos] = 0xFF;
            buffer[pos + 1] = 0xF8;
            
            tests_run++;
            
            FrameSyncResult result = detectFrameSync(buffer.data(), buffer.size());
            
            if (result.found && result.offset == pos && !result.is_variable && result.is_byte_aligned) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED at position " << pos << ": found=" << result.found 
                          << ", offset=" << result.offset << ", is_variable=" << result.is_variable << std::endl;
                assert(false && "Fixed sync code should be detected");
            }
        }
        std::cout << "    Fixed sync (0xFFF8) detected at all positions ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 2: Valid variable block size sync (0xFFF9) detection
    // ----------------------------------------
    std::cout << "\n  Test 2: Variable block size sync (0xFFF9) detection..." << std::endl;
    {
        std::vector<size_t> positions = {0, 1, 10, 100, 255, 500};
        
        for (size_t pos : positions) {
            std::vector<uint8_t> buffer(pos + 10, 0x00);
            buffer[pos] = 0xFF;
            buffer[pos + 1] = 0xF9;
            
            tests_run++;
            
            FrameSyncResult result = detectFrameSync(buffer.data(), buffer.size());
            
            if (result.found && result.offset == pos && result.is_variable && result.is_byte_aligned) {
                tests_passed++;
            } else {
                std::cerr << "    FAILED at position " << pos << ": found=" << result.found 
                          << ", offset=" << result.offset << ", is_variable=" << result.is_variable << std::endl;
                assert(false && "Variable sync code should be detected");
            }
        }
        std::cout << "    Variable sync (0xFFF9) detected at all positions ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 3: Invalid sync patterns must not be detected
    // ----------------------------------------
    std::cout << "\n  Test 3: Invalid sync patterns rejection..." << std::endl;
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
            {{0x00, 0x00}, "0x0000 (null)"},
            {{0xFF, 0x00}, "0xFF00 (partial)"},
        };
        
        for (const auto& pattern : invalid_patterns) {
            std::vector<uint8_t> buffer = {pattern.bytes[0], pattern.bytes[1], 0x00, 0x00};
            
            tests_run++;
            
            FrameSyncResult result = detectFrameSync(buffer.data(), buffer.size());
            
            if (!result.found) {
                tests_passed++;
                std::cout << "    " << pattern.description << " rejected ✓" << std::endl;
            } else {
                std::cerr << "    FAILED: " << pattern.description << " was detected as sync!" << std::endl;
                assert(false && "Invalid sync pattern should not be detected");
            }
        }
    }
    
    // ----------------------------------------
    // Test 4: Search limit enforcement (512 bytes max)
    // ----------------------------------------
    std::cout << "\n  Test 4: Search limit enforcement (512 bytes max)..." << std::endl;
    {
        // Sync code beyond 512 bytes should not be found
        std::vector<uint8_t> buffer(600, 0x00);
        buffer[520] = 0xFF;
        buffer[521] = 0xF8;
        
        tests_run++;
        
        FrameSyncResult result = detectFrameSync(buffer.data(), buffer.size(), 512);
        
        if (!result.found) {
            tests_passed++;
            std::cout << "    Sync at offset 520 not found (beyond 512 limit) ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Sync beyond 512 bytes was found!" << std::endl;
            assert(false && "Sync beyond search limit should not be found");
        }
        
        // Sync code within 512 bytes should be found
        buffer[500] = 0xFF;
        buffer[501] = 0xF8;
        
        tests_run++;
        
        result = detectFrameSync(buffer.data(), buffer.size(), 512);
        
        if (result.found && result.offset == 500) {
            tests_passed++;
            std::cout << "    Sync at offset 500 found (within 512 limit) ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Sync within 512 bytes was not found!" << std::endl;
            assert(false && "Sync within search limit should be found");
        }
    }
    
    // ----------------------------------------
    // Test 5: First sync code is returned when multiple exist
    // ----------------------------------------
    std::cout << "\n  Test 5: First sync code returned when multiple exist..." << std::endl;
    {
        std::vector<uint8_t> buffer(100, 0x00);
        // Place sync codes at positions 10, 30, and 50
        buffer[10] = 0xFF; buffer[11] = 0xF8;
        buffer[30] = 0xFF; buffer[31] = 0xF9;
        buffer[50] = 0xFF; buffer[51] = 0xF8;
        
        tests_run++;
        
        FrameSyncResult result = detectFrameSync(buffer.data(), buffer.size());
        
        if (result.found && result.offset == 10 && !result.is_variable) {
            tests_passed++;
            std::cout << "    First sync at offset 10 returned ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: First sync was not returned (got offset " << result.offset << ")" << std::endl;
            assert(false && "First sync code should be returned");
        }
    }
    
    // ----------------------------------------
    // Test 6: Random data with embedded sync codes (100 iterations)
    // ----------------------------------------
    std::cout << "\n  Test 6: Random data with embedded sync codes (100 iterations)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> byte_dist(0, 255);
        std::uniform_int_distribution<> pos_dist(0, 400);
        std::uniform_int_distribution<> type_dist(0, 1);
        
        int random_passed = 0;
        
        for (int i = 0; i < 100; ++i) {
            // Generate random buffer
            std::vector<uint8_t> buffer(512);
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
            
            FrameSyncResult result = detectFrameSync(buffer.data(), buffer.size());
            
            if (result.found && result.offset == sync_pos && result.is_variable == is_variable) {
                tests_passed++;
                random_passed++;
            } else {
                std::cerr << "    FAILED iteration " << i << ": expected pos=" << sync_pos 
                          << ", got pos=" << result.offset << std::endl;
                assert(false && "Embedded sync code should be detected");
            }
        }
        std::cout << "    " << random_passed << "/100 random tests passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 7: Null pointer and empty buffer handling
    // ----------------------------------------
    std::cout << "\n  Test 7: Null pointer and empty buffer handling..." << std::endl;
    {
        tests_run++;
        FrameSyncResult result = detectFrameSync(nullptr, 0);
        if (!result.found) {
            tests_passed++;
            std::cout << "    Null pointer handled safely ✓" << std::endl;
        } else {
            assert(false && "Null pointer should return not found");
        }
        
        tests_run++;
        std::vector<uint8_t> empty;
        result = detectFrameSync(empty.data(), 0);
        if (!result.found) {
            tests_passed++;
            std::cout << "    Empty buffer handled safely ✓" << std::endl;
        } else {
            assert(false && "Empty buffer should return not found");
        }
        
        tests_run++;
        std::vector<uint8_t> single = {0xFF};
        result = detectFrameSync(single.data(), 1);
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
    std::cout << "\n✓ Property 6: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}


// ========================================
// PROPERTY 7: Blocking Strategy Consistency
// ========================================
// **Feature: flac-demuxer, Property 7: Blocking Strategy Consistency**
// **Validates: Requirements 4.8**
//
// For any FLAC stream, if the blocking strategy bit changes mid-stream, 
// the FLAC Demuxer SHALL reject the stream.

/**
 * Simulates blocking strategy tracking across multiple frames
 */
class BlockingStrategyTracker {
public:
    /**
     * Process a frame sync code and check for consistency
     * @param is_variable True if variable block size (0xFFF9), false if fixed (0xFFF8)
     * @return true if consistent with previous frames, false if strategy changed
     */
    bool processFrame(bool is_variable) {
        if (!m_strategy_set) {
            // First frame - set the strategy
            m_strategy_set = true;
            m_is_variable = is_variable;
            return true;
        }
        
        // Check consistency with established strategy
        if (is_variable != m_is_variable) {
            // Requirement 4.8: Reject if blocking strategy changes mid-stream
            return false;
        }
        
        return true;
    }
    
    /**
     * Reset tracker state
     */
    void reset() {
        m_strategy_set = false;
        m_is_variable = false;
    }
    
    /**
     * Get current blocking strategy
     */
    bool isVariable() const { return m_is_variable; }
    
    /**
     * Check if strategy has been set
     */
    bool isSet() const { return m_strategy_set; }
    
private:
    bool m_strategy_set = false;
    bool m_is_variable = false;
};

void test_property_blocking_strategy_consistency() {
    std::cout << "\n=== Property 7: Blocking Strategy Consistency ===" << std::endl;
    std::cout << "Testing that blocking strategy changes mid-stream are rejected..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: Consistent fixed block size stream
    // ----------------------------------------
    std::cout << "\n  Test 1: Consistent fixed block size stream..." << std::endl;
    {
        BlockingStrategyTracker tracker;
        
        // Simulate 10 frames all with fixed block size (0xFFF8)
        bool all_consistent = true;
        for (int i = 0; i < 10; ++i) {
            tests_run++;
            if (tracker.processFrame(false)) {  // false = fixed
                tests_passed++;
            } else {
                all_consistent = false;
            }
        }
        
        if (all_consistent) {
            std::cout << "    10 fixed block size frames accepted ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Consistent fixed stream was rejected!" << std::endl;
            assert(false && "Consistent fixed stream should be accepted");
        }
    }
    
    // ----------------------------------------
    // Test 2: Consistent variable block size stream
    // ----------------------------------------
    std::cout << "\n  Test 2: Consistent variable block size stream..." << std::endl;
    {
        BlockingStrategyTracker tracker;
        
        // Simulate 10 frames all with variable block size (0xFFF9)
        bool all_consistent = true;
        for (int i = 0; i < 10; ++i) {
            tests_run++;
            if (tracker.processFrame(true)) {  // true = variable
                tests_passed++;
            } else {
                all_consistent = false;
            }
        }
        
        if (all_consistent) {
            std::cout << "    10 variable block size frames accepted ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Consistent variable stream was rejected!" << std::endl;
            assert(false && "Consistent variable stream should be accepted");
        }
    }
    
    // ----------------------------------------
    // Test 3: Fixed to variable change must be rejected
    // ----------------------------------------
    std::cout << "\n  Test 3: Fixed to variable change rejection..." << std::endl;
    {
        BlockingStrategyTracker tracker;
        
        // First frame: fixed
        tests_run++;
        if (tracker.processFrame(false)) {
            tests_passed++;
        } else {
            assert(false && "First frame should be accepted");
        }
        
        // Second frame: variable (should be rejected)
        tests_run++;
        if (!tracker.processFrame(true)) {
            tests_passed++;
            std::cout << "    Fixed→Variable change rejected ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Fixed→Variable change was accepted!" << std::endl;
            assert(false && "Strategy change should be rejected");
        }
    }
    
    // ----------------------------------------
    // Test 4: Variable to fixed change must be rejected
    // ----------------------------------------
    std::cout << "\n  Test 4: Variable to fixed change rejection..." << std::endl;
    {
        BlockingStrategyTracker tracker;
        
        // First frame: variable
        tests_run++;
        if (tracker.processFrame(true)) {
            tests_passed++;
        } else {
            assert(false && "First frame should be accepted");
        }
        
        // Second frame: fixed (should be rejected)
        tests_run++;
        if (!tracker.processFrame(false)) {
            tests_passed++;
            std::cout << "    Variable→Fixed change rejected ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Variable→Fixed change was accepted!" << std::endl;
            assert(false && "Strategy change should be rejected");
        }
    }
    
    // ----------------------------------------
    // Test 5: Change at various positions in stream
    // ----------------------------------------
    std::cout << "\n  Test 5: Change detection at various stream positions..." << std::endl;
    {
        std::vector<int> change_positions = {2, 5, 10, 50, 100};
        
        for (int change_pos : change_positions) {
            BlockingStrategyTracker tracker;
            bool change_detected = false;
            
            for (int i = 0; i < change_pos + 5; ++i) {
                bool is_variable = (i >= change_pos);  // Change at change_pos
                
                tests_run++;
                if (tracker.processFrame(is_variable)) {
                    tests_passed++;
                } else {
                    // Change was detected
                    change_detected = true;
                    tests_passed++;
                    break;
                }
            }
            
            if (change_detected) {
                std::cout << "    Change at frame " << change_pos << " detected ✓" << std::endl;
            } else {
                std::cerr << "    FAILED: Change at frame " << change_pos << " not detected!" << std::endl;
                assert(false && "Strategy change should be detected");
            }
        }
    }
    
    // ----------------------------------------
    // Test 6: Random stream with single change (100 iterations)
    // ----------------------------------------
    std::cout << "\n  Test 6: Random streams with single change (100 iterations)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> len_dist(5, 50);
        std::uniform_int_distribution<> type_dist(0, 1);
        
        int random_passed = 0;
        
        for (int iter = 0; iter < 100; ++iter) {
            BlockingStrategyTracker tracker;
            
            int stream_length = len_dist(gen);
            int change_pos = std::uniform_int_distribution<>(1, stream_length - 1)(gen);
            bool initial_strategy = (type_dist(gen) == 1);
            
            bool change_detected = false;
            
            for (int i = 0; i < stream_length; ++i) {
                bool is_variable = (i < change_pos) ? initial_strategy : !initial_strategy;
                
                if (!tracker.processFrame(is_variable)) {
                    change_detected = true;
                    break;
                }
            }
            
            tests_run++;
            if (change_detected) {
                tests_passed++;
                random_passed++;
            } else {
                std::cerr << "    FAILED iteration " << iter << ": change at " << change_pos 
                          << " not detected" << std::endl;
                assert(false && "Strategy change should be detected");
            }
        }
        std::cout << "    " << random_passed << "/100 random tests passed ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Test 7: Reset allows new strategy
    // ----------------------------------------
    std::cout << "\n  Test 7: Reset allows new strategy..." << std::endl;
    {
        BlockingStrategyTracker tracker;
        
        // Set fixed strategy
        tests_run++;
        tracker.processFrame(false);
        tests_passed++;
        
        // Reset
        tracker.reset();
        
        // Now variable should be accepted
        tests_run++;
        if (tracker.processFrame(true)) {
            tests_passed++;
            std::cout << "    Reset allows new strategy ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Reset did not clear strategy!" << std::endl;
            assert(false && "Reset should allow new strategy");
        }
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 7: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC FRAME SYNC CODE PROPERTY-BASED TESTS" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // Property 6: Frame Sync Code Detection
        // **Feature: flac-demuxer, Property 6: Frame Sync Code Detection**
        // **Validates: Requirements 4.1, 4.2**
        test_property_frame_sync_detection();
        
        // Property 7: Blocking Strategy Consistency
        // **Feature: flac-demuxer, Property 7: Blocking Strategy Consistency**
        // **Validates: Requirements 4.8**
        test_property_blocking_strategy_consistency();
        
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
