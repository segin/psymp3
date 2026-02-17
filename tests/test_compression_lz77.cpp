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
        
        // Check if compression actually happened (simple check, might not compress for very short strings due to overhead)
        // "bananabanana" (12 bytes) -> "banana" (6) + <dist:6, len:6> (2 bytes) + flags
        // It SHOULD be smaller or equal.
        // std::cout << "Original: " << input.size() << ", Compressed: " << compressed.size() << "\n";
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
        // 1000 bytes with max match length 18 (2 bytes token + 1/8 flag overhead) -> ~120-130 bytes
        ASSERT_TRUE(compressed.size() < 150);
    }

    std::cout << "[UNIT] Passed.\n";
    return 0;
}

// Property Tests
int property_tests() {
    #ifdef HAVE_RAPIDCHECK
    // If we had RapidCheck enabled, we would use it here.
    // For now, implementing a manual random property test.
    #endif
    
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

// Edge Case Tests
int edge_case_tests() {
    std::cout << "[EDGE] Running edge case tests...\n";

    LZ77Decompressor decompressor;

    // Test 1: Truncated Input (Flags only, Literal)
    {
        // 0x00 flags = 8 literals. But no data.
        std::vector<uint8_t> input = { 0x00 };
        auto output = decompressor.decompress(input.data(), input.size());
        // Should handle gracefully and produce empty output
        ASSERT_EQ(output.size(), 0);
    }

    // Test 2: Truncated Input (Flags only, Reference)
    {
        // 0x01 flags = 1st bit Ref. But no data.
        std::vector<uint8_t> input = { 0x01 };
        auto output = decompressor.decompress(input.data(), input.size());
        ASSERT_EQ(output.size(), 0);
    }

    // Test 3: Truncated Reference (Partial data)
    {
        // 0x01 flags = 1st bit Ref. Need 2 bytes. Only 1 provided.
        // Data: [Flags=0x01, Byte1=0x00]
        std::vector<uint8_t> input = { 0x01, 0x00 };
        auto output = decompressor.decompress(input.data(), input.size());
        ASSERT_EQ(output.size(), 0);
    }

    // Test 4: Truncated Literal
    {
        // 0x00 flags = 8 literals. 1st bit is Literal. Need 1 byte. 0 provided.
        // Data: [Flags=0x00]
        // Checked in Test 1, but let's be explicit
        std::vector<uint8_t> input = { 0x00 };
        auto output = decompressor.decompress(input.data(), input.size());
        ASSERT_EQ(output.size(), 0);
    }

    // Test 5: Invalid Backreference (Distance > Output Size)
    {
        // Flags=0x01 (Ref).
        // Ref: Dist=5, Length=3.
        // Encoding: <Distance:12><Length:4> (Wait, implementation format is different)

        // Implementation:
        // b1 = (dist >> 4) & 0xFF
        // b2 = ((dist & 0x0F) << 4) | (len_code & 0x0F)
        // len_code = length - 3

        // Let's encode Distance=100 (0x64), Length=3 (code 0).
        // b1 = (0x64 >> 4) = 0x06
        // b2 = ((0x64 & 0x0F) << 4) | 0 = (0x04 << 4) = 0x40

        // Input: [0x01, 0x06, 0x40]
        // Output size is 0 initially. Distance 100 > 0.
        // Code should clamp distance to output.size() (0).
        // If distance becomes 0, it does nothing.

        std::vector<uint8_t> input = { 0x01, 0x06, 0x40 };
        auto output = decompressor.decompress(input.data(), input.size());
        ASSERT_EQ(output.size(), 0);
    }

    // Test 6: Zero Distance
    {
        // Distance=0.
        // b1 = 0
        // b2 = 0 (len code 0)
        std::vector<uint8_t> input = { 0x01, 0x00, 0x00 };
        auto output = decompressor.decompress(input.data(), input.size());
        ASSERT_EQ(output.size(), 0);
    }

    // Test 7: Non-multiple of 8 input items
    {
        // Flags=0x00 (8 Literals).
        // Provide 3 bytes: 'A', 'B', 'C'.
        // Loop should run 3 times then stop because cursor >= size.
        std::vector<uint8_t> input = { 0x00, 'A', 'B', 'C' };
        auto output = decompressor.decompress(input.data(), input.size());
        ASSERT_EQ(output.size(), 3);
        if (output.size() == 3) {
            ASSERT_EQ(output[0], 'A');
            ASSERT_EQ(output[1], 'B');
            ASSERT_EQ(output[2], 'C');
        }
    }

    std::cout << "[EDGE] Passed.\n";
    return 0;
}

int main(int argc, char** argv) {
    if (unit_tests() != 0) return 1;
    if (property_tests() != 0) return 1;
    if (fuzzer_tests() != 0) return 1;
    if (edge_case_tests() != 0) return 1;
    
    std::cout << "All LZ77 tests passed successfully.\n";
    return 0;
}
