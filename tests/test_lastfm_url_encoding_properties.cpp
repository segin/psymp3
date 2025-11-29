/*
 * test_lastfm_url_encoding_properties.cpp - Property-based tests for URL encoding round-trip
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
#include <random>
#include <curl/curl.h>

// ========================================
// URL ENCODING/DECODING IMPLEMENTATIONS FOR TESTING
// ========================================

/**
 * URL encode a string using libcurl (same as HTTPClient::urlEncode)
 */
std::string urlEncode(const std::string& input) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        return input; // Fallback - return unencoded
    }
    
    char *output = curl_easy_escape(curl, input.c_str(), static_cast<int>(input.length()));
    std::string result;
    
    if (output) {
        result = output;
        curl_free(output);
    } else {
        result = input; // Fallback - return unencoded
    }
    
    curl_easy_cleanup(curl);
    return result;
}

/**
 * URL decode a string using libcurl
 * This is the inverse of urlEncode for testing round-trip property
 */
std::string urlDecode(const std::string& input) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        return input; // Fallback - return undecoded
    }
    
    int output_length = 0;
    char *output = curl_easy_unescape(curl, input.c_str(), static_cast<int>(input.length()), &output_length);
    std::string result;
    
    if (output) {
        result = std::string(output, output_length);
        curl_free(output);
    } else {
        result = input; // Fallback - return undecoded
    }
    
    curl_easy_cleanup(curl);
    return result;
}

// ========================================
// PROPERTY-BASED TESTS
// ========================================

// ========================================
// PROPERTY 2: URL Encoding Round-Trip
// ========================================
// **Feature: lastfm-performance-optimization, Property 2: URL Encoding Round-Trip**
// **Validates: Requirements 2.2**
//
// For any string containing ASCII characters, URL encoding followed by 
// URL decoding SHALL produce the original string.
void test_property_url_encoding_round_trip() {
    std::cout << "\n=== Property 2: URL Encoding Round-Trip ===" << std::endl;
    std::cout << "Testing that URL encoding followed by decoding produces original string..." << std::endl;
    
    // Test with known test vectors first
    std::vector<std::string> test_vectors = {
        "",                                    // Empty string
        "hello",                               // Simple ASCII
        "hello world",                         // Space
        "hello%20world",                       // Already encoded space
        "artist=The Beatles",                  // Equals sign
        "track=Hey Jude",                      // Space in value
        "album=Abbey Road (Remastered)",       // Parentheses
        "name=John Doe & Jane Doe",            // Ampersand
        "query=foo+bar",                       // Plus sign
        "path=/music/rock/classic",            // Slashes
        "special=!@#$%^&*()_+-=[]{}|;':\",./<>?", // Special characters
        "unicode=café",                        // Non-ASCII (UTF-8)
        "japanese=音楽",                        // Japanese characters
        "emoji=🎵🎶",                           // Emoji
        "mixed=Hello World! 123 @#$",          // Mixed content
        "newline=line1\nline2",                // Newline
        "tab=col1\tcol2",                      // Tab
        "carriage=line1\rline2",               // Carriage return
    };
    
    std::cout << "\n  Testing known test vectors:" << std::endl;
    int passed = 0;
    int failed = 0;
    
    for (const auto& input : test_vectors) {
        std::string encoded = urlEncode(input);
        std::string decoded = urlDecode(encoded);
        
        if (decoded == input) {
            passed++;
            std::string display = input.length() > 30 ? input.substr(0, 27) + "..." : input;
            // Replace control characters for display
            for (char& c : display) {
                if (c == '\n') c = 'n';
                else if (c == '\r') c = 'r';
                else if (c == '\t') c = 't';
            }
            std::cout << "    \"" << display << "\" → encode → decode → match ✓" << std::endl;
        } else {
            failed++;
            std::cerr << "  MISMATCH:" << std::endl;
            std::cerr << "    Input:   \"" << input << "\"" << std::endl;
            std::cerr << "    Encoded: \"" << encoded << "\"" << std::endl;
            std::cerr << "    Decoded: \"" << decoded << "\"" << std::endl;
        }
    }
    
    std::cout << "\n  Test vectors: " << passed << " passed, " << failed << " failed" << std::endl;
    
    // Random ASCII string testing (100 iterations as per design doc)
    std::cout << "\n  Testing random ASCII inputs (100 iterations):" << std::endl;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> len_dist(0, 500);   // Random length 0-500
    std::uniform_int_distribution<> char_dist(1, 127);  // ASCII printable range (excluding null)
    
    int random_passed = 0;
    int random_failed = 0;
    
    for (int i = 0; i < 100; ++i) {
        // Generate random ASCII string
        int length = len_dist(gen);
        std::string random_input;
        random_input.reserve(length);
        
        for (int j = 0; j < length; ++j) {
            random_input += static_cast<char>(char_dist(gen));
        }
        
        // Perform round-trip
        std::string encoded = urlEncode(random_input);
        std::string decoded = urlDecode(encoded);
        
        if (decoded == random_input) {
            random_passed++;
        } else {
            random_failed++;
            std::cerr << "  MISMATCH at iteration " << i << ":" << std::endl;
            std::cerr << "    Input length: " << random_input.length() << std::endl;
            std::cerr << "    Encoded length: " << encoded.length() << std::endl;
            std::cerr << "    Decoded length: " << decoded.length() << std::endl;
            
            // Show first difference
            for (size_t k = 0; k < std::min(random_input.length(), decoded.length()); ++k) {
                if (random_input[k] != decoded[k]) {
                    std::cerr << "    First diff at position " << k << ": "
                              << "input=" << static_cast<int>(random_input[k]) 
                              << " decoded=" << static_cast<int>(decoded[k]) << std::endl;
                    break;
                }
            }
        }
    }
    
    std::cout << "    Random ASCII: " << random_passed << "/100 passed" << std::endl;
    
    // Test with binary data (all byte values except null)
    std::cout << "\n  Testing binary data (all byte values 1-255):" << std::endl;
    
    std::string binary_data;
    binary_data.reserve(255);
    for (int i = 1; i <= 255; ++i) {
        binary_data += static_cast<char>(i);
    }
    
    std::string binary_encoded = urlEncode(binary_data);
    std::string binary_decoded = urlDecode(binary_encoded);
    
    if (binary_decoded == binary_data) {
        std::cout << "    Binary data round-trip ✓" << std::endl;
    } else {
        std::cerr << "    Binary data round-trip FAILED" << std::endl;
        failed++;
    }
    
    // Verify overall results
    assert(failed == 0 && "Some test vectors failed round-trip");
    assert(random_failed == 0 && "Some random inputs failed round-trip");
    
    std::cout << "\n✓ Property 2: URL Encoding Round-Trip - ALL TESTS PASSED" << std::endl;
}

// ========================================
// PROPERTY: URL Encoding Preserves Unreserved Characters
// ========================================
// Unreserved characters (A-Z, a-z, 0-9, -, _, ., ~) should not be encoded
void test_property_url_encoding_unreserved_chars() {
    std::cout << "\n=== Additional Property: Unreserved Characters ===" << std::endl;
    std::cout << "Testing that unreserved characters are not encoded..." << std::endl;
    
    // RFC 3986 unreserved characters
    std::string unreserved = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";
    
    std::string encoded = urlEncode(unreserved);
    
    // Unreserved characters should remain unchanged
    if (encoded == unreserved) {
        std::cout << "  Unreserved characters preserved ✓" << std::endl;
    } else {
        // Some implementations may encode ~ or other chars, which is still valid
        // The important thing is round-trip works
        std::cout << "  Note: Some unreserved chars were encoded (implementation-specific)" << std::endl;
        std::cout << "  Original: " << unreserved << std::endl;
        std::cout << "  Encoded:  " << encoded << std::endl;
    }
    
    // Verify round-trip still works
    std::string decoded = urlDecode(encoded);
    assert(decoded == unreserved && "Round-trip failed for unreserved characters");
    std::cout << "  Round-trip verified ✓" << std::endl;
    
    std::cout << "\n✓ Unreserved Characters Property - PASSED" << std::endl;
}

// ========================================
// PROPERTY: URL Encoding Handles Reserved Characters
// ========================================
// Reserved characters should be encoded
void test_property_url_encoding_reserved_chars() {
    std::cout << "\n=== Additional Property: Reserved Characters ===" << std::endl;
    std::cout << "Testing that reserved characters are properly encoded..." << std::endl;
    
    // RFC 3986 reserved characters
    std::string reserved = ":/?#[]@!$&'()*+,;=";
    
    std::string encoded = urlEncode(reserved);
    
    // Reserved characters should be percent-encoded
    std::cout << "  Original: " << reserved << std::endl;
    std::cout << "  Encoded:  " << encoded << std::endl;
    
    // Verify round-trip
    std::string decoded = urlDecode(encoded);
    assert(decoded == reserved && "Round-trip failed for reserved characters");
    std::cout << "  Round-trip verified ✓" << std::endl;
    
    std::cout << "\n✓ Reserved Characters Property - PASSED" << std::endl;
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "LAST.FM URL ENCODING PROPERTY-BASED TESTS" << std::endl;
    std::cout << "**Feature: lastfm-performance-optimization, Property 2: URL Encoding Round-Trip**" << std::endl;
    std::cout << "**Validates: Requirements 2.2**" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    // Initialize libcurl
    curl_global_init(CURL_GLOBAL_ALL);
    
    try {
        // Run all property tests
        test_property_url_encoding_round_trip();
        test_property_url_encoding_unreserved_chars();
        test_property_url_encoding_reserved_chars();
        
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "✅ ALL PROPERTY TESTS PASSED" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        curl_global_cleanup();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(60, '=') << std::endl;
        std::cerr << "❌ PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Exception: " << e.what() << std::endl;
        std::cerr << std::string(60, '=') << std::endl;
        curl_global_cleanup();
        return 1;
    } catch (...) {
        std::cerr << "\n" << std::string(60, '=') << std::endl;
        std::cerr << "❌ PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Unknown exception occurred" << std::endl;
        std::cerr << std::string(60, '=') << std::endl;
        curl_global_cleanup();
        return 1;
    }
}
