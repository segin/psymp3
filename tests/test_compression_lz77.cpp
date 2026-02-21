/*
 * test_compression_lz77.cpp
 * 
 * Unit, Property, and Fuzzer tests for LZ77 compression
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// Disable standard tests if harness is disabled, BUT enable if we are building 
// specifically for this test (which we are if we are compiling this file)
// The makefile logic usually handles this, but for safety:
#undef NDEBUG

#include "core/compression/LZ77.h"
#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <cstring>
#include <random>
#include <algorithm>

using namespace PsyMP3::Core::Compression;

// Simple assertions
#define ASSERT_EQ(a, b) \
    if ((a) != (b)) { \
        std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ \
                  << ": " << #a << " != " << #b << "\n"; \
        std::cerr << "  LHS: " << (a) << "\n"; \
        std::cerr << "  RHS: " << (b) << "\n"; \
        return 1; \
    }

#define ASSERT_TRUE(a) \
    if (!(a)) { \
        std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ \
                  << ": " << #a << " is false\n"; \
        return 1; \
    }

// Helper to convert string to vector
std::vector<uint8_t> to_vec(const std::string& s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

// Unit Tests
int unit_tests() {
    std::cout << "[UNIT] Running unit tests...\n";
    
    LZ77Compressor compressor;
    LZ77Decompressor decompressor;

    // Test 1: Empty input
    {
        std::vector<uint8_t> input;
        auto compressed = compressor.compress(input.data(), input.size());
        auto output = decompressor.decompress(compressed.data(), compressed.size());
        ASSERT_EQ(output.size(), 0);
    }

    // Test 2: Simple repetitive string
    {
        std::string s = "bananabanana";
        auto input = to_vec(s);
        auto compressed = compressor.compress(input.data(), input.size());
        auto output = decompressor.decompress(compressed.data(), compressed.size());
        
        std::string result(output.begin(), output.end());
        ASSERT_EQ(s, result);
    }

    // Test 3: No repetition
    {
        std::string s = "abcdefghijklmnopqrstuvwxyz";
        auto input = to_vec(s);
        auto compressed = compressor.compress(input.data(), input.size());
        auto output = decompressor.decompress(compressed.data(), compressed.size());
        
        std::string result(output.begin(), output.end());
        ASSERT_EQ(s, result);
    }

    // Test 4: Long repetition
    {
        std::string s(1000, 'A');
        auto input = to_vec(s);
        auto compressed = compressor.compress(input.data(), input.size());
        auto output = decompressor.decompress(compressed.data(), compressed.size());
        
        ASSERT_EQ(output.size(), 1000);
        for(size_t i=0; i<output.size(); ++i) {
            if (output[i] != 'A') {
                std::cerr << "Mismatch at index " << i << "\n";
                return 1;
            }
        }
        
        // Should be significantly compressed
        // With max match length 18, we expect ~130 bytes for 1000 chars.
        // 1000 / 18 = 55 matches. 55 * 2 bytes = 110 bytes. + Overhead.
        ASSERT_TRUE(compressed.size() < 200);
    }

    std::cout << "[UNIT] Passed.\n";
    return 0;
}

// Edge Case Tests
int edge_case_tests() {
    std::cout << "[EDGE] Running edge case tests...\n";

    LZ77Decompressor decompressor;

    // 1. Truncated Literal (Flags says literal, but no data)
    {
        // Flags: 0 (all literals). Data: empty.
        // Expected: Empty output.
        std::vector<uint8_t> input = {0x00};
        auto output = decompressor.decompress(input.data(), input.size());
        ASSERT_EQ(output.size(), 0);
    }

    // 2. Truncated Literal Partial (Flags says 8 literals, but only 1 byte provided)
    {
        // Flags: 0. Data: 1 byte.
        // Expected: 1 byte output.
        std::vector<uint8_t> input = {0x00, 0x41}; // 'A'
        auto output = decompressor.decompress(input.data(), input.size());
        ASSERT_EQ(output.size(), 1);
        ASSERT_EQ(output[0], 0x41);
    }

    // 3. Truncated Reference (Flags says Ref, but no data)
    {
        // Flags: 1 (Ref at bit 0). Data: empty.
        std::vector<uint8_t> input = {0x01};
        auto output = decompressor.decompress(input.data(), input.size());
        ASSERT_EQ(output.size(), 0);
    }

    // 4. Truncated Reference Partial (Flags says Ref, but only 1 byte provided)
    {
        // Flags: 1. Data: 1 byte.
        std::vector<uint8_t> input = {0x01, 0x00};
        auto output = decompressor.decompress(input.data(), input.size());
        ASSERT_EQ(output.size(), 0); // Should break before reading 2nd byte
    }

    // 5. Invalid Distance (Backreference underrun)
    {
        // Flags: 0 (Literal 'A'), 1 (Ref).
        // Literal 'A'. Then Ref with large distance.
        // Input: [Flags, 'A', DistHigh, DistLow+Len]
        // Flags: Bit 0=0, Bit 1=1. -> 0x02.
        // Data: 'A' (0x41).
        // Ref: Dist=100. Len=3.
        // Dist 100 -> 0x064.
        // b1 = (100 >> 4) = 6.
        // b2 = ((100 & 0xF) << 4) | (0). = 0x40.

        std::vector<uint8_t> input = {0x02, 0x41, 0x06, 0x40};
        auto output = decompressor.decompress(input.data(), input.size());

        // Expected: 'A'. Then Ref 100 clamped to 1 (size).
        // Start = 1 - 1 = 0. Length 3.
        // Copy 'A', 'A', 'A'.
        // Total: 'A', 'A', 'A', 'A'. Size 4.

        ASSERT_EQ(output.size(), 4);
        for(auto b : output) ASSERT_EQ(b, 0x41);
    }

    // 6. Input ending mid-block (e.g. 8 items flags, but only 2 items provided)
    {
        // Flags: 0 (8 literals).
        // Data: 'A', 'B'.
        std::vector<uint8_t> input = {0x00, 0x41, 0x42};
        auto output = decompressor.decompress(input.data(), input.size());
        ASSERT_EQ(output.size(), 2);
        ASSERT_EQ(output[0], 0x41);
        ASSERT_EQ(output[1], 0x42);
    }

    std::cout << "[EDGE] Passed.\n";
    return 0;
}

// Property Tests
int property_tests() {
    std::cout << "[PROP] Running property tests (Round-trip integrity)...\n";
    
    LZ77Compressor compressor;
    LZ77Decompressor decompressor;
    
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> char_dist(0, 255);
    std::uniform_int_distribution<int> pattern_dist(0, 10);
    
    for (int i = 0; i < 100; ++i) {
        // Generate random data with some structure to ensure compression triggers
        std::vector<uint8_t> input;
        size_t len = 100 + (rng() % 5000);
        
        while (input.size() < len) {
            if (pattern_dist(rng) < 3 && input.size() > 10) {
                // Repeat a previous chunk
                size_t dist = 1 + (rng() % std::min(input.size(), (size_t)100));
                size_t fragment_len = 3 + (rng() % 15);
                size_t start = input.size() - dist;
                for(size_t k=0; k<fragment_len; ++k) {
                    input.push_back(input[start + k % dist]); // % dist handles overlapping copy naturally logic-wise
                }
            } else {
                input.push_back(static_cast<uint8_t>(char_dist(rng)));
            }
        }
        
        // Compress & Decompress
        auto compressed = compressor.compress(input.data(), input.size());
        auto output = decompressor.decompress(compressed.data(), compressed.size());
        
        if (input != output) {
            std::cerr << "Property test failed on iteration " << i << "\n";
            std::cerr << "Input size: " << input.size() << ", Output size: " << output.size() << "\n";
             // Debug dump first few bytes
            for(size_t k=0; k<std::min(input.size(), (size_t)20); ++k) 
                std::cerr << std::hex << (int)input[k] << " ";
            std::cerr << "\n";
            for(size_t k=0; k<std::min(output.size(), (size_t)20); ++k) 
                std::cerr << std::hex << (int)output[k] << " ";
            std::cerr << "\n";
            return 1;
        }
    }
    
    std::cout << "[PROP] Passed.\n";
    return 0;
}

// Fuzzer Tests
int fuzzer_tests() {
    std::cout << "[FUZZ] Running fuzzer tests (Decompressor robustness)...\n";
    
    LZ77Decompressor decompressor;
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> byte_dist(0, 255);
    
    // Decompress completely random garbage
    for (int i = 0; i < 1000; ++i) {
        size_t len = 1 + (rng() % 1024);
        std::vector<uint8_t> garbage(len);
        for(size_t j=0; j<len; ++j) garbage[j] = static_cast<uint8_t>(byte_dist(rng));
        
        try {
            // It should not crash (segfault)
            // It might produce garbage output or throw exceptions (if we used them)
            // Currently our implementation silences errors or clamps them.
            auto output = decompressor.decompress(garbage.data(), garbage.size());
            
            // Just ensure we survived
            (void)output; 
        } catch (...) {
            // Exceptions are acceptable, crashes are not
        }
    }
    
    std::cout << "[FUZZ] Passed.\n";
    return 0;
}

int main(int argc, char** argv) {
    if (unit_tests() != 0) return 1;
    if (edge_case_tests() != 0) return 1;
    if (property_tests() != 0) return 1;
    if (fuzzer_tests() != 0) return 1;
    
    std::cout << "All LZ77 tests passed successfully.\n";
    return 0;
}
