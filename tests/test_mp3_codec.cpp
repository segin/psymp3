/*
 * test_mp3_codec.cpp - Unit tests for MP3Codec (Libmpg123 wrapper)
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

// Standard headers for test
#include <iostream>
#include <memory>
#include <vector>
#include <algorithm>
#include <cstring>
#include <stdexcept>

#ifdef HAVE_MP3
#include "codecs/mp3/MP3Codec.h"

struct IOHandlerState {
    bool read_called = false;
    bool seek_called = false;
    bool tell_called = false;
    bool close_called = false;
};

// Mock IOHandler to verify callbacks
class MockIOHandler : public PsyMP3::IO::IOHandler {
public:
    MockIOHandler(IOHandlerState& state) : PsyMP3::IO::IOHandler(), m_state(state) {
        // Minimal MP3 frame header
        unsigned char header[] = { 0xFF, 0xFB, 0x90, 0x64 };
        m_data.assign(header, header + sizeof(header));
        m_data.resize(4096, 0);
    }

    ~MockIOHandler() override = default;

    size_t read(void* buffer, size_t size, size_t count) override {
        m_state.read_called = true;
        size_t bytes_requested = size * count;
        if (m_pos >= m_data.size()) return 0;

        size_t bytes_available = m_data.size() - m_pos;
        size_t bytes_to_copy = std::min(bytes_requested, bytes_available);

        std::memcpy(buffer, m_data.data() + m_pos, bytes_to_copy);
        m_pos += bytes_to_copy;

        return bytes_to_copy / size;
    }

    int seek(off_t offset, int whence) override {
        m_state.seek_called = true;
        if (whence == SEEK_SET) {
            m_pos = offset;
        } else if (whence == SEEK_CUR) {
            m_pos += offset;
        } else if (whence == SEEK_END) {
            m_pos = m_data.size() + offset;
        }
        if (m_pos > m_data.size()) m_pos = m_data.size();
        return 0;
    }

    off_t tell() override {
        m_state.tell_called = true;
        return m_pos;
    }

    int close() override {
        m_state.close_called = true;
        return 0;
    }

    bool eof() override {
        return m_pos >= m_data.size();
    }

    off_t getFileSize() override {
        return m_data.size();
    }

private:
    std::vector<unsigned char> m_data;
    size_t m_pos = 0;
    IOHandlerState& m_state;
};

int main() {
    std::cout << "Running MP3Codec (Libmpg123) tests..." << std::endl;

    bool all_passed = true;

    // Test 1: Constructor and Cleanup
    std::cout << "1. Testing constructor and cleanup callback..." << std::endl;
    {
        IOHandlerState state;

        try {
            auto mock_handler = std::make_unique<MockIOHandler>(state);

            // Instantiate Libmpg123
            PsyMP3::Codec::MP3::Libmpg123 decoder(std::move(mock_handler));

            std::cout << "   ✓ Libmpg123 instantiated successfully" << std::endl;

            if (state.read_called) {
                std::cout << "   ✓ read() called during initialization" << std::endl;
            } else {
                 std::cout << "   - read() not called during initialization (might be delayed)" << std::endl;
            }

        } catch (const std::exception& e) {
            std::cerr << "   ✗ Exception during instantiation: " << e.what() << std::endl;
            all_passed = false;
        }

        // Verify close was called via cleanup_callback
        if (state.close_called) {
            std::cout << "   ✓ close() called upon destruction (cleanup_callback working)" << std::endl;
        } else {
            // Only consider it a failure if we expected success
            if (all_passed) {
                 std::cerr << "   ✗ close() NOT called upon destruction" << std::endl;
                 all_passed = false;
            }
        }
    }

    if (all_passed) {
        std::cout << "\nAll MP3Codec tests PASSED!" << std::endl;
        return 0;
    } else {
        std::cerr << "\nSome MP3Codec tests FAILED!" << std::endl;
        return 1;
    }
}

#else // !HAVE_MP3

int main() {
    std::cout << "MP3 support not enabled, skipping tests." << std::endl;
    return 0;
}

#endif // HAVE_MP3
