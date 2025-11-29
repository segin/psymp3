/*
 * test_lastfm_md5_properties.cpp - Property-based tests for Last.fm MD5 hash optimization
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
#include <iomanip>
#include <sstream>
#include <random>
#include <openssl/evp.h>

// ========================================
// STANDALONE MD5 IMPLEMENTATIONS FOR TESTING
// ========================================

/**
 * Reference MD5 implementation using iostream (the old slow method)
 * This is used to verify the optimized implementation produces identical results
 */
std::string md5Hash_reference(const std::string& input) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return "";
    
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    
    if (EVP_DigestInit_ex(ctx, EVP_md5(), nullptr) &&
        EVP_DigestUpdate(ctx, input.c_str(), input.length()) &&
        EVP_DigestFinal_ex(ctx, hash, &hash_len)) {
        
        // Old iostream-based hex formatting (slow but known correct)
        std::ostringstream hexHash;
        for (unsigned int i = 0; i < hash_len; i++) {
            hexHash << std::hex << std::setw(2) << std::setfill('0') 
                    << static_cast<unsigned int>(hash[i]);
        }
        
        EVP_MD_CTX_free(ctx);
        return hexHash.str();
    }
    
    EVP_MD_CTX_free(ctx);
    return "";
}

/**
 * Optimized MD5 implementation using lookup table (the new fast method)
 * This is the implementation being tested
 */
std::string md5Hash_optimized(const std::string& input) {
    // Optimized MD5 implementation using lookup table for hex conversion
    // instead of iostream formatting (Requirements 1.1, 1.2)
    static constexpr char hex_chars[] = "0123456789abcdef";
    
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return "";
    
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    
    if (EVP_DigestInit_ex(ctx, EVP_md5(), nullptr) &&
        EVP_DigestUpdate(ctx, input.c_str(), input.length()) &&
        EVP_DigestFinal_ex(ctx, hash, &hash_len)) {
        
        // Pre-allocate output string (MD5 is always 16 bytes = 32 hex chars)
        std::string result;
        result.reserve(32);
        
        // Convert bytes to hex using lookup table and bit shifting
        for (unsigned int i = 0; i < hash_len; i++) {
            result += hex_chars[(hash[i] >> 4) & 0x0F];
            result += hex_chars[hash[i] & 0x0F];
        }
        
        EVP_MD_CTX_free(ctx);
        return result;
    }
    
    EVP_MD_CTX_free(ctx);
    return "";
}

// ========================================
// PROPERTY-BASED TESTS
// ========================================

// ========================================
// PROPERTY 1: MD5 Hash Correctness
// ========================================
// **Feature: lastfm-performance-optimization, Property 1: MD5 Hash Correctness**
// **Validates: Requirements 1.1, 1.2**
//
// For any input string, the md5Hash function SHALL produce the same 
// 32-character lowercase hexadecimal output as the reference OpenSSL MD5 
// implementation.
void test_property_md5_hash_correctness() {
    std::cout << "\n=== Property 1: MD5 Hash Correctness ===" << std::endl;
    std::cout << "Testing that optimized MD5 produces identical output to reference implementation..." << std::endl;
    
    // Test with RFC 1321 test vectors first
    std::vector<std::pair<std::string, std::string>> rfc_test_vectors = {
        {"", "d41d8cd98f00b204e9800998ecf8427e"},
        {"a", "0cc175b9c0f1b6a831c399e269772661"},
        {"abc", "900150983cd24fb0d6963f7d28e17f72"},
        {"message digest", "f96b697d7cb7938d525a2f31aaf161d0"},
        {"abcdefghijklmnopqrstuvwxyz", "c3fcd3d76192e4007dfb496cca67e13b"},
        {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", 
         "d174ab98d277d9f5a5611c2c9f419d9f"},
        {"12345678901234567890123456789012345678901234567890123456789012345678901234567890",
         "57edf4a22be3c955ac49da2e2107b67a"}
    };
    
    std::cout << "\n  Testing RFC 1321 test vectors:" << std::endl;
    for (const auto& [input, expected] : rfc_test_vectors) {
        std::string optimized_result = md5Hash_optimized(input);
        std::string reference_result = md5Hash_reference(input);
        
        // Verify optimized matches reference
        assert(optimized_result == reference_result);
        
        // Verify both match expected RFC value
        assert(optimized_result == expected);
        
        std::string display_input = input.length() > 20 ? 
            input.substr(0, 17) + "..." : input;
        std::cout << "    \"" << display_input << "\" → " << optimized_result << " ✓" << std::endl;
    }
    
    std::cout << "\n  Testing random inputs (100 iterations):" << std::endl;
    
    // Random number generator for property-based testing
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> len_dist(0, 1000);  // Random length 0-1000
    std::uniform_int_distribution<> char_dist(0, 255);  // Random byte values
    
    int test_count = 0;
    int passed_count = 0;
    
    for (int i = 0; i < 100; ++i) {
        // Generate random input string
        int length = len_dist(gen);
        std::string random_input;
        random_input.reserve(length);
        
        for (int j = 0; j < length; ++j) {
            random_input += static_cast<char>(char_dist(gen));
        }
        
        // Compute hashes with both implementations
        std::string optimized_result = md5Hash_optimized(random_input);
        std::string reference_result = md5Hash_reference(random_input);
        
        test_count++;
        
        // Verify they match
        if (optimized_result == reference_result) {
            passed_count++;
        } else {
            std::cerr << "  MISMATCH at iteration " << i << ":" << std::endl;
            std::cerr << "    Input length: " << length << std::endl;
            std::cerr << "    Optimized: " << optimized_result << std::endl;
            std::cerr << "    Reference: " << reference_result << std::endl;
            assert(false && "MD5 hash mismatch between optimized and reference");
        }
        
        // Verify output format (32 lowercase hex characters)
        assert(optimized_result.length() == 32);
        for (char c : optimized_result) {
            assert((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
        }
    }
    
    std::cout << "    Passed " << passed_count << "/" << test_count << " random input tests ✓" << std::endl;
    
    // Test edge cases
    std::cout << "\n  Testing edge cases:" << std::endl;
    
    // Empty string
    assert(md5Hash_optimized("") == md5Hash_reference(""));
    std::cout << "    Empty string ✓" << std::endl;
    
    // Single character
    assert(md5Hash_optimized("x") == md5Hash_reference("x"));
    std::cout << "    Single character ✓" << std::endl;
    
    // Null bytes in string
    std::string with_nulls = std::string("hello\0world", 11);
    assert(md5Hash_optimized(with_nulls) == md5Hash_reference(with_nulls));
    std::cout << "    String with null bytes ✓" << std::endl;
    
    // Very long string (10KB)
    std::string long_string(10240, 'A');
    assert(md5Hash_optimized(long_string) == md5Hash_reference(long_string));
    std::cout << "    Long string (10KB) ✓" << std::endl;
    
    // Binary data (all byte values)
    std::string binary_data;
    binary_data.reserve(256);
    for (int i = 0; i < 256; ++i) {
        binary_data += static_cast<char>(i);
    }
    assert(md5Hash_optimized(binary_data) == md5Hash_reference(binary_data));
    std::cout << "    Binary data (all byte values) ✓" << std::endl;
    
    // Last.fm specific test cases (password hashes, auth tokens)
    std::string password = "mysecretpassword123";
    std::string password_hash = md5Hash_optimized(password);
    std::string timestamp = "1732924800";
    std::string auth_token = md5Hash_optimized(password_hash + timestamp);
    
    // Verify auth token generation matches reference
    std::string ref_password_hash = md5Hash_reference(password);
    std::string ref_auth_token = md5Hash_reference(ref_password_hash + timestamp);
    assert(auth_token == ref_auth_token);
    std::cout << "    Last.fm auth token generation ✓" << std::endl;
    
    std::cout << "\n✓ Property 1: MD5 Hash Correctness - ALL TESTS PASSED" << std::endl;
}

// ========================================
// PROPERTY 2: MD5 Output Format Consistency
// ========================================
// For any input, the MD5 hash output SHALL always be exactly 32 lowercase
// hexadecimal characters.
void test_property_md5_output_format() {
    std::cout << "\n=== Property 2: MD5 Output Format Consistency ===" << std::endl;
    std::cout << "Testing that MD5 output is always 32 lowercase hex characters..." << std::endl;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> len_dist(0, 5000);
    std::uniform_int_distribution<> char_dist(0, 255);
    
    int test_count = 0;
    
    for (int i = 0; i < 100; ++i) {
        int length = len_dist(gen);
        std::string random_input;
        random_input.reserve(length);
        
        for (int j = 0; j < length; ++j) {
            random_input += static_cast<char>(char_dist(gen));
        }
        
        std::string hash = md5Hash_optimized(random_input);
        
        // Verify length is exactly 32
        assert(hash.length() == 32);
        
        // Verify all characters are lowercase hex
        for (char c : hash) {
            bool is_valid = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
            assert(is_valid && "Invalid character in MD5 hash output");
        }
        
        test_count++;
    }
    
    std::cout << "  Verified " << test_count << " random inputs produce valid 32-char hex output ✓" << std::endl;
    std::cout << "\n✓ Property 2: MD5 Output Format Consistency - ALL TESTS PASSED" << std::endl;
}

// ========================================
// PROPERTY 3: MD5 Determinism
// ========================================
// For any input, calling md5Hash multiple times SHALL produce identical output.
void test_property_md5_determinism() {
    std::cout << "\n=== Property 3: MD5 Determinism ===" << std::endl;
    std::cout << "Testing that MD5 produces identical output for same input..." << std::endl;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> len_dist(0, 1000);
    std::uniform_int_distribution<> char_dist(0, 255);
    
    int test_count = 0;
    
    for (int i = 0; i < 50; ++i) {
        int length = len_dist(gen);
        std::string random_input;
        random_input.reserve(length);
        
        for (int j = 0; j < length; ++j) {
            random_input += static_cast<char>(char_dist(gen));
        }
        
        // Call md5Hash multiple times with same input
        std::string hash1 = md5Hash_optimized(random_input);
        std::string hash2 = md5Hash_optimized(random_input);
        std::string hash3 = md5Hash_optimized(random_input);
        
        // All results should be identical
        assert(hash1 == hash2);
        assert(hash2 == hash3);
        
        test_count++;
    }
    
    std::cout << "  Verified " << test_count << " inputs produce deterministic output ✓" << std::endl;
    std::cout << "\n✓ Property 3: MD5 Determinism - ALL TESTS PASSED" << std::endl;
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "LAST.FM MD5 HASH PROPERTY-BASED TESTS" << std::endl;
    std::cout << "**Feature: lastfm-performance-optimization, Property 1: MD5 Hash Correctness**" << std::endl;
    std::cout << "**Validates: Requirements 1.1, 1.2**" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    try {
        // Run all property tests
        test_property_md5_hash_correctness();
        test_property_md5_output_format();
        test_property_md5_determinism();
        
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "✅ ALL PROPERTY TESTS PASSED" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(60, '=') << std::endl;
        std::cerr << "❌ PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Exception: " << e.what() << std::endl;
        std::cerr << std::string(60, '=') << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n" << std::string(60, '=') << std::endl;
        std::cerr << "❌ PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Unknown exception occurred" << std::endl;
        std::cerr << std::string(60, '=') << std::endl;
        return 1;
    }
}
