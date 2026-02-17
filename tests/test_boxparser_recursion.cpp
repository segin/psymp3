/*
 * test_boxparser_recursion.cpp - Verify BoxParser recursion limit
 */

#include "psymp3.h"
#include "demuxer/iso/BoxParser.h"
#include "io/MemoryIOHandler.h"
#include <iostream>
#include <vector>

using namespace PsyMP3;
using namespace PsyMP3::Demuxer::ISO;
using namespace PsyMP3::IO;

int main() {
    std::cout << "Testing BoxParser Recursion Limit..." << std::endl;

    // Case 1: At limit (MAX_BOX_DEPTH)
    {
        // Use a dummy buffer
        std::vector<uint8_t> buffer(1024, 0);
        // Using copy=false to avoid allocation overhead, buffer scope covers parser lifetime
        auto io = std::make_shared<MemoryIOHandler>(buffer.data(), buffer.size(), false);
        BoxParser parser(io);

        std::cout << "Case 1: Call with depth = MAX_BOX_DEPTH (" << BoxParser::MAX_BOX_DEPTH << ")" << std::endl;
        bool result = parser.ParseBoxRecursively(0, 0, [](const BoxHeader&, uint64_t, uint32_t){ return true; }, BoxParser::MAX_BOX_DEPTH);
        if (result == false) {
            std::cout << "SUCCESS: correctly returned false (limit hit)." << std::endl;
        } else {
            std::cout << "FAILURE: returned true, expected false (limit hit)." << std::endl;
            return 1;
        }
    }

    // Case 2: Above limit
    {
        std::vector<uint8_t> buffer(1024, 0);
        auto io = std::make_shared<MemoryIOHandler>(buffer.data(), buffer.size(), false);
        BoxParser parser(io);

        std::cout << "Case 2: Call with depth = MAX_BOX_DEPTH + 1" << std::endl;
        bool result = parser.ParseBoxRecursively(0, 0, [](const BoxHeader&, uint64_t, uint32_t){ return true; }, BoxParser::MAX_BOX_DEPTH + 1);
        if (result == false) {
            std::cout << "SUCCESS: correctly returned false (above limit)." << std::endl;
        } else {
            std::cout << "FAILURE: returned true, expected false (above limit)." << std::endl;
            return 1;
        }
    }

    // Case 3: Below limit
    {
        std::vector<uint8_t> buffer(1024, 0);
        auto io = std::make_shared<MemoryIOHandler>(buffer.data(), buffer.size(), false);
        BoxParser parser(io);

        std::cout << "Case 3: Call with depth = MAX_BOX_DEPTH - 1" << std::endl;
        // 0 size means loop doesn't run, returns true (success)
        bool result = parser.ParseBoxRecursively(0, 0, [](const BoxHeader&, uint64_t, uint32_t){ return true; }, BoxParser::MAX_BOX_DEPTH - 1);
        if (result == true) {
            std::cout << "SUCCESS: correctly returned true (below limit)." << std::endl;
        } else {
            std::cout << "FAILURE: returned false, expected true (below limit)." << std::endl;
            return 1;
        }
    }

    // Case 4: Verify handler receives correct depth
    {
        std::cout << "Case 4: Verify handler receives depth + 1" << std::endl;
        // We need data for one box.
        // Box: size 8, type 'free' (any valid type)
        // 0-3: size (BE)
        // 4-7: type (BE)
        std::vector<uint8_t> valid_box_data = {0, 0, 0, 8, 'f', 'r', 'e', 'e'};
        auto io = std::make_shared<MemoryIOHandler>(valid_box_data.data(), valid_box_data.size(), false);
        BoxParser parser(io);

        uint32_t start_depth = 5;
        bool handler_called = false;
        uint32_t received_depth = 0;

        // Call with depth=5, size=8. BoxParser should find the box and call handler with depth=6.
        parser.ParseBoxRecursively(0, 8, [&](const BoxHeader&, uint64_t, uint32_t d) -> bool {
            handler_called = true;
            received_depth = d;
            return true;
        }, start_depth);

        if (handler_called) {
            if (received_depth == start_depth + 1) {
                std::cout << "SUCCESS: Handler received depth " << received_depth << " (expected " << start_depth + 1 << ")" << std::endl;
            } else {
                std::cout << "FAILURE: Handler received depth " << received_depth << " (expected " << start_depth + 1 << ")" << std::endl;
                return 1;
            }
        } else {
            std::cout << "FAILURE: Handler was not called." << std::endl;
            return 1;
        }
    }

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
