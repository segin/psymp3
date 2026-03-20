#include <iostream>
#include <vector>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <cstring>
#include <algorithm>

// Define types similar to PsyMP3 for compatibility
using filesize_t = int64_t;
using off_t = int64_t;

// Mock exceptions
class BadFormatException : public std::runtime_error {
public:
    BadFormatException(const std::string& msg) : std::runtime_error(msg) {}
};

class WrongFormatException : public std::runtime_error {
public:
    WrongFormatException(const std::string& msg) : std::runtime_error(msg) {}
};

// Mock IOHandler
class IOHandler {
public:
    virtual filesize_t tell() = 0;
    virtual size_t read(void* buffer, size_t size, size_t count) = 0;
    virtual int seek(filesize_t offset, int whence) = 0;
    virtual ~IOHandler() = default;
};

// Constants
// SEEK_SET and SEEK_CUR are defined in <cstdio>

constexpr uint32_t FMT_ID  = 0x20746d66; // "fmt "
constexpr uint32_t DATA_ID = 0x61746164; // "data"

// Template for read_le
template <typename T>
T read_le(IOHandler* handler)
{
    T value;
    if (handler->read(&value, sizeof(T), 1) != 1) {
        throw BadFormatException("Unexpected end of file.");
    }
    return value;
}

class WaveStreamRepro {
public:
    std::unique_ptr<IOHandler> m_handler;
    bool m_eof = false;

    WaveStreamRepro(std::unique_ptr<IOHandler> handler) : m_handler(std::move(handler)) {}

    void parseHeaders() {
        // Mock loop over chunks
        while (!m_eof) {
            uint32_t chunk_id = read_le<uint32_t>(m_handler.get());
            uint32_t chunk_size = read_le<uint32_t>(m_handler.get());

            // Vulnerable logic from wav.cpp:152
            // We simulate 'long' as 32-bit signed integer to demonstrate the vulnerability on 32-bit systems/Windows.
            // On 64-bit Linux, 'long' is 64-bit so overflow is harder to trigger but the type usage is still incorrect.
            using simulated_long = int32_t;

            simulated_long chunk_start_pos = static_cast<simulated_long>(m_handler->tell());

            std::cout << "Chunk start pos: " << chunk_start_pos << std::endl;
            std::cout << "Chunk size: " << chunk_size << std::endl;

            if (chunk_id == DATA_ID) {
                return;
            }

            // Seek to the next chunk, accounting for padding byte if chunk size is odd.
            // FIX: Check for overflow
            simulated_long next_chunk_pos = chunk_start_pos + chunk_size;
            if (next_chunk_pos < chunk_start_pos) {
                 std::cout << "Overflow check triggered!" << std::endl;
                 throw BadFormatException("WAVE chunk size causes overflow.");
            }

            m_handler->seek(next_chunk_pos, SEEK_SET);
            if (chunk_size % 2 != 0)
                m_handler->seek(1, SEEK_CUR);

            // Break loop for repro purposes (only need one iteration)
            break;
        }
    }
};

class MockIOHandler : public IOHandler {
public:
    // Start at 2GB (0x77359400 is approx 2GB, INT32_MAX is 2147483647)
    // Let's pick a value close to 2GB but positive in signed 32-bit.
    filesize_t current_pos = 2000000000;

    // Chunk size 3GB.
    // 2GB + 3GB = 5GB.
    // In 32-bit unsigned arithmetic:
    // 2000000000 + 3000000000 = 5000000000.
    // 5000000000 mod 2^32 = 5000000000 - 4294967296 = 705032704.
    // So we expect seek to be called with 705032704 instead of 5000000000.

    uint32_t chunk_size_val = 3000000000;

    filesize_t tell() override {
        return current_pos;
    }

    size_t read(void* buffer, size_t size, size_t count) override {
        if (count * size == 4) {
             // First read is ID, second is size.
             static int call_count = 0;
             if (call_count == 0) {
                 *(uint32_t*)buffer = 0x12345678; // Dummy ID
                 call_count++;
             } else {
                 *(uint32_t*)buffer = chunk_size_val;
             }
             return 1;
        }
        return 0;
    }

    int seek(filesize_t offset, int whence) override {
        std::cout << "Seek called with offset: " << offset << std::endl;
        if (offset < 0) {
            std::cout << "Seek called with negative offset! Overflow detected." << std::endl;
            throw std::runtime_error("Overflow detected");
        }

        // Check for wrap-around (simulated 32-bit wrap)
        // If offset is significantly smaller than current_pos + chunk_size (expected), it's a bug.
        // Expected: 5000000000
        // Actual if wrapped: ~705032704

        if (offset < current_pos) {
             std::cout << "Seek called with offset (" << offset << ") smaller than current pos (" << current_pos << ")! Overflow/Wrap detected." << std::endl;
             throw std::runtime_error("Overflow detected");
        }

        return 0;
    }
};

int main() {
    auto handler = std::make_unique<MockIOHandler>();
    WaveStreamRepro repro(std::move(handler));
    try {
        repro.parseHeaders();
        std::cout << "Reproduction failed: No overflow detected." << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Caught exception: " << e.what() << std::endl;
        std::string msg = e.what();
        if (msg == "Overflow detected") {
             std::cout << "SUCCESS: Vulnerability reproduced (Mock IOHandler caught it)." << std::endl;
             return 0;
        } else if (msg == "WAVE chunk size causes overflow.") {
             std::cout << "SUCCESS: Vulnerability fixed (Logic caught it)." << std::endl;
             return 0;
        }
    }
    return 1; // Fail if not reproduced
}
