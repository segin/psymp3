/*
 * test_boxparser_recursion.cpp - Verify BoxParser recursion limit
 */

#include "psymp3.h"
#include "demuxer/iso/BoxParser.h"
#include "io/IOHandler.h"
#include <iostream>
#include <vector>
#include <cstring>

using namespace PsyMP3;
using namespace PsyMP3::Demuxer::ISO;
using namespace PsyMP3::IO;

class SimpleMemoryIOHandler : public IOHandler {
    const uint8_t* m_data;
    size_t m_size;
    off_t m_pos;
public:
    SimpleMemoryIOHandler(const uint8_t* data, size_t size) : m_data(data), m_size(size), m_pos(0) {}

    // Virtual destructor is handled by base

    // Implement pure virtuals or overrides
    size_t read(void* buffer, size_t size, size_t count) override {
        size_t total_size = size * count;
        if (m_pos + total_size > m_size) {
            total_size = m_size - m_pos;
        }
        if (total_size == 0) return 0;

        std::memcpy(buffer, m_data + m_pos, total_size);
        m_pos += total_size;
        return total_size / size; // Return count read
    }

    int seek(off_t offset, int whence) override {
        off_t new_pos = m_pos;
        if (whence == SEEK_SET) new_pos = offset;
        else if (whence == SEEK_CUR) new_pos += offset;
        else if (whence == SEEK_END) new_pos = m_size + offset;

        if (new_pos < 0 || new_pos > (off_t)m_size) return -1;
        m_pos = new_pos;
        return 0;
    }

    off_t tell() override { return m_pos; }

    off_t getFileSize() override { return m_size; }

    // Other methods can use base implementation (stubbed)
};

int main() {
    std::cout << "Testing BoxParser Recursion Limit..." << std::endl;

    // Create a dummy buffer
    std::vector<uint8_t> buffer(1024, 0);
    auto io = std::make_shared<SimpleMemoryIOHandler>(buffer.data(), buffer.size());
    BoxParser parser(io);

    // Check limit constant
    if (BoxParser::MAX_BOX_DEPTH != 32) {
        std::cout << "Warning: MAX_BOX_DEPTH is " << BoxParser::MAX_BOX_DEPTH << " expected 32" << std::endl;
    }

    bool result;

    // Case 1: At limit (MAX_BOX_DEPTH)
    std::cout << "Case 1: Call with depth = MAX_BOX_DEPTH (" << BoxParser::MAX_BOX_DEPTH << ")" << std::endl;
    result = parser.ParseBoxRecursively(0, 0, [](const BoxHeader&, uint64_t, uint32_t){ return true; }, BoxParser::MAX_BOX_DEPTH);
    if (result == false) {
        std::cout << "SUCCESS: correctly returned false (limit hit)." << std::endl;
    } else {
        std::cout << "FAILURE: returned true, expected false (limit hit)." << std::endl;
        return 1;
    }

    // Case 2: Above limit (MAX_BOX_DEPTH + 1)
    std::cout << "Case 2: Call with depth = MAX_BOX_DEPTH + 1" << std::endl;
    result = parser.ParseBoxRecursively(0, 0, [](const BoxHeader&, uint64_t, uint32_t){ return true; }, BoxParser::MAX_BOX_DEPTH + 1);
    if (result == false) {
        std::cout << "SUCCESS: correctly returned false (above limit)." << std::endl;
    } else {
        std::cout << "FAILURE: returned true, expected false (above limit)." << std::endl;
        return 1;
    }

    // Case 3: Below limit (MAX_BOX_DEPTH - 1)
    std::cout << "Case 3: Call with depth = MAX_BOX_DEPTH - 1" << std::endl;
    result = parser.ParseBoxRecursively(0, 0, [](const BoxHeader&, uint64_t, uint32_t){ return true; }, BoxParser::MAX_BOX_DEPTH - 1);
    if (result == true) {
        std::cout << "SUCCESS: correctly returned true (below limit)." << std::endl;
    } else {
        std::cout << "FAILURE: returned false, expected true (below limit)." << std::endl;
        return 1;
    }

    // Case 4: Verify handler receives correct depth
    std::cout << "Case 4: Verify handler receives depth + 1" << std::endl;
    // We need data for one box.
    // Box: size 8, type 'free' (any valid type)
    // 0-3: size (BE)
    // 4-7: type (BE)
    std::vector<uint8_t> valid_box_data = {0, 0, 0, 8, 'f', 'r', 'e', 'e'};
    auto io2 = std::make_shared<SimpleMemoryIOHandler>(valid_box_data.data(), valid_box_data.size());
    BoxParser parser2(io2);

    uint32_t start_depth = 5;
    bool handler_called = false;
    uint32_t received_depth = 0;

    // Call with depth=5, size=8. BoxParser should find the box and call handler with depth=6.
    parser2.ParseBoxRecursively(0, 8, [&](const BoxHeader&, uint64_t, uint32_t d) -> bool {
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
        std::cout << "FAILURE: Handler was not called. (Maybe box parsing failed?)" << std::endl;
        return 1;
    }

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
