/*
 * test_compression_lz77_edge_cases.cpp
 *
 * Focused edge case tests for LZ77 decompression.
 */

#include "core/compression/LZ77.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <string>

using namespace PsyMP3::Core::Compression;

// Simple assertion macro
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

int main() {
    std::cout << "[EDGE] Running LZ77 edge case tests...\n";

    LZ77Decompressor decompressor;

    // Test 1: Truncated Flags (Input size 1)
    // Input: [0x00] (Flags: 8 literals expected) but no data follows.
    {
        std::cout << "  Test 1: Truncated Flags... ";
        std::vector<uint8_t> input = {0x00};
        auto output = decompressor.decompress(input.data(), input.size());
        ASSERT_EQ(output.size(), 0);
        std::cout << "Passed.\n";
    }

    // Test 2: Truncated Literal
    // Input: [0x01] (Flags: 1st item Ref, rest Literals) - Wait, 0 = Literal, 1 = Ref.
    // Let's use 0x00 (All Literals).
    // Input: [0x00, 'A'] (Flags: 8 literals, but only 1 byte provided).
    // Expected: Output contains 'A', then stops.
    {
        std::cout << "  Test 2: Truncated Literal stream... ";
        std::vector<uint8_t> input = {0x00, 'A'};
        auto output = decompressor.decompress(input.data(), input.size());
        ASSERT_EQ(output.size(), 1);
        ASSERT_EQ(output[0], 'A');
        std::cout << "Passed.\n";
    }

    // Test 3: Truncated Reference (0 bytes available)
    // Input: [0x01] (Flags: 1st item is Ref). No data follows.
    // Expected: Output empty.
    {
        std::cout << "  Test 3: Truncated Reference (0 bytes)... ";
        std::vector<uint8_t> input = {0x01};
        auto output = decompressor.decompress(input.data(), input.size());
        ASSERT_EQ(output.size(), 0);
        std::cout << "Passed.\n";
    }

    // Test 4: Truncated Reference (1 byte available)
    // Input: [0x01, 0x00] (Flags: Ref, 1 byte data). Needs 2 bytes for Ref.
    // Expected: Output empty (ref skipped).
    {
        std::cout << "  Test 4: Truncated Reference (1 byte)... ";
        std::vector<uint8_t> input = {0x01, 0x00};
        auto output = decompressor.decompress(input.data(), input.size());
        ASSERT_EQ(output.size(), 0);
        std::cout << "Passed.\n";
    }

    // Test 5: Invalid Backreference (Distance > Output Size)
    // We need some initial data to be valid and a full block to reset flags.
    // Block 1: 8 Literals [0x00, 'A'...'H'] -> Output "ABCDEFGH"
    // Block 2: Flags [0x01] (1st item Ref)
    //          Ref bytes [0x06, 0x40] -> Dist 100, Len 3.
    //
    // Distance 100 > Output Size 8.
    // Implementation clamps distance to output.size() = 8.
    // Start = 8 - 8 = 0.
    // Copies output[0]...output[2] -> "ABC".
    // Result: "ABCDEFGHABC".
    {
        std::cout << "  Test 5: Invalid Backreference... ";
        std::vector<uint8_t> input = {
            0x00, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', // Block 1
            0x01, 0x06, 0x40                               // Block 2
        };
        auto output = decompressor.decompress(input.data(), input.size());

        ASSERT_EQ(output.size(), 11);
        std::string s(output.begin(), output.end());
        ASSERT_EQ(s, "ABCDEFGHABC");
        std::cout << "Passed.\n";
    }

    // Test 6: Mid-block Truncation
    // Flags say 8 literals. Only 4 provided.
    // Input: [0x00, '1', '2', '3', '4']
    {
        std::cout << "  Test 6: Mid-block Truncation... ";
        std::vector<uint8_t> input = {0x00, '1', '2', '3', '4'};
        auto output = decompressor.decompress(input.data(), input.size());
        ASSERT_EQ(output.size(), 4);
        std::string s(output.begin(), output.end());
        ASSERT_EQ(s, "1234");
        std::cout << "Passed.\n";
    }

    std::cout << "[EDGE] All tests passed.\n";
    return 0;
}
