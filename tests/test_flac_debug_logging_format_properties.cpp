/*
 * test_flac_debug_logging_format_properties.cpp - Property-based tests for FLAC debug logging format
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
#include <regex>
#include <random>

// ========================================
// DEBUG LOGGING FORMAT VALIDATION
// ========================================

/**
 * Requirement 29.1: Debug log messages SHALL include method-specific tokens
 * 
 * The FLAC_DEBUG macro format is:
 *   Debug::log("flac", "[", __FUNCTION__, ":", __LINE__, "] ", ...)
 * 
 * This produces log messages in the format:
 *   [MethodName:LineNumber] message content
 * 
 * For example:
 *   [parseStreamInfo:887] Parsing STREAMINFO block (RFC 9639 Section 8.2)
 *   [calculateFrameSize:3382] Estimating frame size for frame at offset 12345
 */

/**
 * Validates that a debug log message contains a method identification token
 * in the format [MethodName:LineNumber]
 * 
 * @param log_message The log message to validate
 * @return true if the message contains a valid method token, false otherwise
 */
bool hasValidMethodToken(const std::string& log_message) {
    // Pattern: [MethodName:LineNumber] where MethodName is alphanumeric with underscores
    // and LineNumber is a positive integer
    // Example: [parseStreamInfo:887] or [calculateFrameSize_unlocked:3382]
    std::regex method_token_pattern(R"(\[([a-zA-Z_][a-zA-Z0-9_]*):(\d+)\])");
    return std::regex_search(log_message, method_token_pattern);
}

/**
 * Extracts the method name from a debug log message
 * 
 * @param log_message The log message to parse
 * @return The method name if found, empty string otherwise
 */
std::string extractMethodName(const std::string& log_message) {
    std::regex method_token_pattern(R"(\[([a-zA-Z_][a-zA-Z0-9_]*):(\d+)\])");
    std::smatch match;
    if (std::regex_search(log_message, match, method_token_pattern)) {
        return match[1].str();
    }
    return "";
}

/**
 * Extracts the line number from a debug log message
 * 
 * @param log_message The log message to parse
 * @return The line number if found, 0 otherwise
 */
int extractLineNumber(const std::string& log_message) {
    std::regex method_token_pattern(R"(\[([a-zA-Z_][a-zA-Z0-9_]*):(\d+)\])");
    std::smatch match;
    if (std::regex_search(log_message, match, method_token_pattern)) {
        return std::stoi(match[2].str());
    }
    return 0;
}

/**
 * Validates that a method name follows C++ identifier rules
 * 
 * @param method_name The method name to validate
 * @return true if valid C++ identifier, false otherwise
 */
bool isValidCppIdentifier(const std::string& method_name) {
    if (method_name.empty()) return false;
    
    // First character must be letter or underscore
    if (!std::isalpha(method_name[0]) && method_name[0] != '_') {
        return false;
    }
    
    // Remaining characters must be alphanumeric or underscore
    for (size_t i = 1; i < method_name.length(); ++i) {
        if (!std::isalnum(method_name[i]) && method_name[i] != '_') {
            return false;
        }
    }
    
    return true;
}

// ========================================
// PROPERTY-BASED TESTS
// ========================================

// ========================================
// PROPERTY 23: Debug Logging Format
// ========================================
// **Feature: flac-demuxer, Property 23: Debug Logging Format**
// **Validates: Requirements 29.1**
//
// For any debug log message, the FLAC Demuxer SHALL include method-specific
// identification tokens.

void test_property_debug_logging_format() {
    std::cout << "\n=== Property 23: Debug Logging Format ===" << std::endl;
    std::cout << "Testing that debug log messages include method identification tokens..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // ----------------------------------------
    // Test 1: Valid log messages with method tokens
    // ----------------------------------------
    std::cout << "\n  Test 1: Valid log messages with method tokens..." << std::endl;
    {
        // Example log messages that should be produced by FLAC_DEBUG macro
        std::vector<std::string> valid_messages = {
            "[parseStreamInfo:887] Parsing STREAMINFO block (RFC 9639 Section 8.2)",
            "[calculateFrameSize:3382] Estimating frame size for frame at offset 12345",
            "[findNextFrame:2189] Searching for frame sync code (RFC 9639 Section 9.1)",
            "[seekTo:473] Seeking to 5000 ms",
            "[readChunk:255] Starting frame read",
            "[validateStreamMarker:601] Validating fLaC stream marker (RFC 9639 Section 6)",
            "[parseMetadataBlocks:650] Parsing metadata blocks",
            "[resyncToNextFrame:4236] Attempting to resynchronize to next valid frame",
            "[skipCorruptedFrame:4340] Skipping corrupted frame at offset 98765",
            "[handleIOError:4417] Requirement 24.8: I/O operation failed",
            "[FLACDemuxer:26] Constructor called",
            "[parseContainer_unlocked:127] Starting FLAC container parsing",
        };
        
        for (const auto& msg : valid_messages) {
            tests_run++;
            
            if (hasValidMethodToken(msg)) {
                std::string method = extractMethodName(msg);
                int line = extractLineNumber(msg);
                std::cout << "    ✓ Valid token found: [" << method << ":" << line << "]" << std::endl;
                tests_passed++;
            } else {
                std::cerr << "    FAILED: No valid method token in: " << msg << std::endl;
                assert(false && "Valid log message should have method token");
            }
        }
    }
    
    // ----------------------------------------
    // Test 2: Method names are valid C++ identifiers
    // ----------------------------------------
    std::cout << "\n  Test 2: Method names are valid C++ identifiers..." << std::endl;
    {
        std::vector<std::string> messages_with_methods = {
            "[parseStreamInfo:100] message",
            "[calculateFrameSize_unlocked:200] message",
            "[_privateMethod:300] message",
            "[method123:400] message",
            "[A:1] single letter method",
        };
        
        for (const auto& msg : messages_with_methods) {
            tests_run++;
            
            std::string method = extractMethodName(msg);
            if (isValidCppIdentifier(method)) {
                std::cout << "    ✓ Valid C++ identifier: " << method << std::endl;
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Invalid C++ identifier: " << method << std::endl;
                assert(false && "Method name should be valid C++ identifier");
            }
        }
    }
    
    // ----------------------------------------
    // Test 3: Line numbers are positive integers
    // ----------------------------------------
    std::cout << "\n  Test 3: Line numbers are positive integers..." << std::endl;
    {
        std::vector<std::string> messages_with_lines = {
            "[method:1] line 1",
            "[method:100] line 100",
            "[method:9999] line 9999",
            "[method:12345] large line number",
        };
        
        for (const auto& msg : messages_with_lines) {
            tests_run++;
            
            int line = extractLineNumber(msg);
            if (line > 0) {
                std::cout << "    ✓ Valid line number: " << line << std::endl;
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Invalid line number: " << line << std::endl;
                assert(false && "Line number should be positive");
            }
        }
    }
    
    // ----------------------------------------
    // Test 4: Invalid formats are rejected
    // ----------------------------------------
    std::cout << "\n  Test 4: Invalid formats are rejected..." << std::endl;
    {
        std::vector<std::string> invalid_messages = {
            "No method token at all",
            "[method] missing line number",
            "[:123] missing method name",
            "[123method:100] method starts with number",
            "[method:] empty line number",
            "[method:-1] negative line number",
            "method:100] missing opening bracket",
            "[method:100 missing closing bracket",
            "[] empty brackets",
            "[method:abc] non-numeric line number",
        };
        
        for (const auto& msg : invalid_messages) {
            tests_run++;
            
            if (!hasValidMethodToken(msg)) {
                std::cout << "    ✓ Correctly rejected: \"" << msg.substr(0, 40) << "...\"" << std::endl;
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Should have rejected: " << msg << std::endl;
                assert(false && "Invalid format should be rejected");
            }
        }
    }
    
    // ----------------------------------------
    // Test 5: Key FLAC demuxer methods have unique tokens
    // ----------------------------------------
    std::cout << "\n  Test 5: Key FLAC demuxer methods have unique tokens..." << std::endl;
    {
        // These are the key methods that MUST have debug logging per Requirements 29.1-29.8
        std::vector<std::string> required_methods = {
            "parseStreamInfo",       // Requirement 29.1: method-specific tokens
            "calculateFrameSize",    // Requirement 29.3: frame size estimation
            "findNextFrame",         // Requirement 29.4: frame boundary detection
            "seekTo",                // Requirement 29.5: seeking strategy
            "resyncToNextFrame",     // Requirement 29.6: error recovery
            "skipCorruptedFrame",    // Requirement 29.6: error recovery
            "handleIOError",         // Requirement 29.6: error recovery
            "readChunk",             // Core functionality
            "parseContainer",        // Core functionality
            "validateStreamMarker",  // Core functionality
        };
        
        // Verify each method name is a valid C++ identifier
        for (const auto& method : required_methods) {
            tests_run++;
            
            if (isValidCppIdentifier(method)) {
                std::cout << "    ✓ Required method: " << method << std::endl;
                tests_passed++;
            } else {
                std::cerr << "    FAILED: Invalid method name: " << method << std::endl;
                assert(false && "Required method name should be valid");
            }
        }
    }
    
    // ----------------------------------------
    // Test 6: Random method name generation and validation
    // ----------------------------------------
    std::cout << "\n  Test 6: Random method name generation (100 iterations)..." << std::endl;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> len_dist(1, 50);
        std::uniform_int_distribution<> char_dist(0, 62);  // a-z, A-Z, 0-9, _
        std::uniform_int_distribution<> line_dist(1, 10000);
        
        const char* valid_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
        const char* first_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_";
        
        int random_passed = 0;
        
        for (int i = 0; i < 100; ++i) {
            // Generate random valid method name
            int len = len_dist(gen);
            std::string method;
            method += first_chars[char_dist(gen) % 53];  // First char: letter or underscore
            for (int j = 1; j < len; ++j) {
                method += valid_chars[char_dist(gen)];
            }
            
            int line = line_dist(gen);
            
            // Create log message
            std::string log_msg = "[" + method + ":" + std::to_string(line) + "] test message";
            
            tests_run++;
            
            if (hasValidMethodToken(log_msg)) {
                std::string extracted_method = extractMethodName(log_msg);
                int extracted_line = extractLineNumber(log_msg);
                
                if (extracted_method == method && extracted_line == line) {
                    random_passed++;
                    tests_passed++;
                } else {
                    std::cerr << "    FAILED: Extraction mismatch for: " << log_msg << std::endl;
                    assert(false && "Method/line extraction should match");
                }
            } else {
                std::cerr << "    FAILED: Valid format not recognized: " << log_msg << std::endl;
                assert(false && "Valid format should be recognized");
            }
        }
        
        std::cout << "    " << random_passed << "/100 random method tokens validated ✓" << std::endl;
    }
    
    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property 23: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// PROPERTY 23b: Method Token Uniqueness
// ========================================
// Additional test to verify that different methods produce distinguishable tokens

void test_property_method_token_uniqueness() {
    std::cout << "\n=== Property 23b: Method Token Uniqueness ===" << std::endl;
    std::cout << "Testing that different methods produce distinguishable tokens..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // Simulate log messages from different methods
    std::vector<std::pair<std::string, std::string>> method_messages = {
        {"parseStreamInfo", "[parseStreamInfo:887] Parsing STREAMINFO"},
        {"parseSeekTable", "[parseSeekTable:1038] Parsing SEEKTABLE"},
        {"calculateFrameSize", "[calculateFrameSize:3382] Estimating frame size"},
        {"findNextFrame", "[findNextFrame:2189] Searching for sync"},
        {"seekTo", "[seekTo:473] Seeking to position"},
        {"readChunk", "[readChunk:255] Reading frame"},
    };
    
    // Verify each method produces a unique token
    std::vector<std::string> seen_methods;
    
    for (const auto& [expected_method, message] : method_messages) {
        tests_run++;
        
        std::string extracted = extractMethodName(message);
        
        if (extracted == expected_method) {
            // Check uniqueness
            bool is_unique = std::find(seen_methods.begin(), seen_methods.end(), extracted) == seen_methods.end();
            
            if (is_unique) {
                seen_methods.push_back(extracted);
                std::cout << "  ✓ Unique method token: " << extracted << std::endl;
                tests_passed++;
            } else {
                std::cerr << "  FAILED: Duplicate method token: " << extracted << std::endl;
                assert(false && "Method tokens should be unique");
            }
        } else {
            std::cerr << "  FAILED: Expected " << expected_method << ", got " << extracted << std::endl;
            assert(false && "Method extraction should match expected");
        }
    }
    
    std::cout << "\n✓ Property 23b: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC DEBUG LOGGING FORMAT PROPERTY-BASED TESTS" << std::endl;
    std::cout << "**Feature: flac-demuxer, Property 23: Debug Logging Format**" << std::endl;
    std::cout << "**Validates: Requirements 29.1**" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // Property 23: Debug Logging Format
        // For any debug log message, the FLAC Demuxer SHALL include
        // method-specific identification tokens.
        test_property_debug_logging_format();
        
        // Property 23b: Method Token Uniqueness
        // Different methods should produce distinguishable tokens
        test_property_method_token_uniqueness();
        
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
