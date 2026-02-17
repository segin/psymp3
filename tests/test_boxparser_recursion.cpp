/*
 * test_boxparser_recursion.cpp - Verification test for BoxParser recursion depth limit
 */

#include "psymp3.h"
#include "demuxer/iso/BoxParser.h"
#include "io/MemoryIOHandler.h"
#include <vector>
#include <iostream>
#include <functional>

using namespace PsyMP3;
using namespace PsyMP3::Demuxer::ISO;
using namespace PsyMP3::IO;

int main() {
    std::cout << "Testing BoxParser Recursion Depth Limit..." << std::endl;

    // Create a buffer with deeply nested boxes
    // Each box will be of type 'TEST'
    // Box N contains Box N+1

    const uint32_t NESTING_LEVELS = 50;
    const uint32_t BOX_HEADER_SIZE = 8;
    const uint32_t TEST_BOX_TYPE = 0x54455354; // 'TEST'

    size_t bufferSize = (NESTING_LEVELS + 1) * BOX_HEADER_SIZE;
    std::vector<uint8_t> buffer(bufferSize, 0);

    for (uint32_t i = 0; i < NESTING_LEVELS; i++) {
        uint64_t offset = i * BOX_HEADER_SIZE;
        uint32_t size = (NESTING_LEVELS - i) * BOX_HEADER_SIZE;

        // Write size (Big Endian)
        buffer[offset + 0] = (size >> 24) & 0xFF;
        buffer[offset + 1] = (size >> 16) & 0xFF;
        buffer[offset + 2] = (size >> 8) & 0xFF;
        buffer[offset + 3] = (size >> 0) & 0xFF;

        // Write type (Big Endian)
        buffer[offset + 4] = (TEST_BOX_TYPE >> 24) & 0xFF;
        buffer[offset + 5] = (TEST_BOX_TYPE >> 16) & 0xFF;
        buffer[offset + 6] = (TEST_BOX_TYPE >> 8) & 0xFF;
        buffer[offset + 7] = (TEST_BOX_TYPE >> 0) & 0xFF;
    }

    auto io = std::make_shared<MemoryIOHandler>(buffer.data(), buffer.size());
    BoxParser parser(io);

    uint32_t maxDepthReached = 0;

    // Recursive handler
    std::function<bool(const BoxHeader&, uint64_t, uint32_t)> handler;
    handler = [&](const BoxHeader& header, uint64_t offset, uint32_t depth) -> bool {
        if (depth > maxDepthReached) {
            maxDepthReached = depth;
        }

        // Recurse into the box content
        uint64_t contentSize = header.size - 8;
        if (contentSize > 0) {
             // Pass depth to recursive call (ParseBoxRecursively takes current depth)
             // Inside ParseBoxRecursively, it will call handler with depth + 1
             // So we pass 'depth' here, effectively saying "we are currently at 'depth', parse my children"
             // Wait. ParseBoxRecursively takes 'depth' parameter.
             // If we are in handler at 'depth' (e.g. 1), it means we are handling a box at depth 1.
             // We want to parse children of this box. Children are at depth 1.
             // No, ParseBoxRecursively iterates children.
             // If I call ParseBoxRecursively(..., depth), it will check if depth >= MAX.
             // If not, it iterates boxes and calls handler(..., depth + 1).
             // So children will be handled at depth + 1.
             // This is correct.
             return parser.ParseBoxRecursively(header.dataOffset, contentSize, handler, depth);
        }
        return true;
    };

    // Start parsing from top level (depth 0)
    bool result = parser.ParseBoxRecursively(0, buffer.size(), handler, 0);

    std::cout << "Max depth reached: " << maxDepthReached << std::endl;
    std::cout << "Parse result: " << (result ? "true" : "false") << std::endl;

    // Verify results
    // We expect maxDepthReached to be exactly MAX_BOX_DEPTH (32).
    // Because at depth 32, handler is called.
    // Handler calls ParseBoxRecursively(..., 32).
    // ParseBoxRecursively(32) returns false immediately.
    // So handler doesn't get called with depth 33.

    if (maxDepthReached == BoxParser::MAX_BOX_DEPTH) {
        std::cout << "SUCCESS: Depth limit reached exactly at limit (" << maxDepthReached << ")." << std::endl;
        return 0;
    } else if (maxDepthReached < BoxParser::MAX_BOX_DEPTH) {
        std::cout << "FAILURE: Depth limit not reached? (Max: " << maxDepthReached << ")" << std::endl;
        return 1;
    } else {
        std::cout << "FAILURE: Depth limit exceeded! (" << maxDepthReached << ")" << std::endl;
        return 1;
    }
}
