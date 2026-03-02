// tests/repro_ogg_duration_perf.cpp

#include "psymp3.h"
#include "demuxer/ogg/OggDemuxer.h"
#include "io/IOHandler.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <thread>
#include <chrono>
#include <algorithm>
#include <atomic>
#include <ogg/ogg.h>

using namespace PsyMP3;
using namespace PsyMP3::IO;
using namespace PsyMP3::Demuxer::Ogg;

// Helper to create a Vorbis ID Header payload
std::vector<uint8_t> createVorbisIDHeader() {
    std::vector<uint8_t> data;
    data.push_back(0x01); // Type
    const char* id = "vorbis";
    for(int i=0; i<6; i++) data.push_back(id[i]);

    // Version (4 bytes) - 0
    for(int i=0; i<4; i++) data.push_back(0);

    // Channels (1 byte) - 2
    data.push_back(2);

    // Sample Rate (4 bytes) - 44100
    uint32_t rate = 44100;
    for(int i=0; i<4; i++) data.push_back((rate >> (i*8)) & 0xFF);

    // Bitrates (3x4 bytes) - 0
    for(int i=0; i<12; i++) data.push_back(0);

    // Blocksize (1 byte) - 0 (valid enough for test)
    data.push_back(0);

    // Framing flag (1 byte) - 1
    data.push_back(1);

    return data;
}

// Helper to create an Ogg page
std::vector<uint8_t> createOggPage(int serial, int seq, int64_t granule, bool bos, bool eos, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> data;

    // Capture pattern
    data.push_back('O'); data.push_back('g'); data.push_back('g'); data.push_back('S');
    data.push_back(0); // Version

    uint8_t flags = 0;
    if (bos) flags |= 0x02;
    if (eos) flags |= 0x04;
    data.push_back(flags);

    // Granule position (8 bytes)
    for (int i = 0; i < 8; i++) {
        data.push_back((granule >> (i*8)) & 0xFF);
    }

    // Serial number (4 bytes)
    for (int i = 0; i < 4; i++) {
        data.push_back((serial >> (i*8)) & 0xFF);
    }

    // Page sequence (4 bytes)
    for (int i = 0; i < 4; i++) {
        data.push_back((seq >> (i*8)) & 0xFF);
    }

    // Checksum (4 bytes) - placeholder
    for (int i = 0; i < 4; i++) data.push_back(0);

    // Page segments
    int segments = (payload.size() + 254) / 255;
    data.push_back(segments);

    // Segment table
    size_t remaining = payload.size();
    for(int i=0; i<segments; i++) {
        int len = std::min((size_t)255, remaining);
        data.push_back(len);
        remaining -= len;
    }

    // Packet data
    data.insert(data.end(), payload.begin(), payload.end());

    // Calculate checksum
    ogg_page page;
    page.header = data.data();
    page.header_len = 27 + 1 + segments; // Fixed + SegCountByte + SegTable
    page.body = data.data() + page.header_len;
    page.body_len = payload.size();

    ogg_page_checksum_set(&page);

    return data;
}

class DelayedMockIOHandler : public IOHandler {
private:
    std::vector<uint8_t> m_head_page;
    std::vector<uint8_t> m_tail_page;
    size_t m_file_size;
    std::atomic<size_t> m_position{0};
    int m_delay_ms;

public:
    DelayedMockIOHandler(int delay_ms) : m_delay_ms(delay_ms) {
        auto vorbis_header = createVorbisIDHeader();
        m_head_page = createOggPage(1234, 0, 0, true, false, vorbis_header);

        // Use a dummy payload for tail page
        std::vector<uint8_t> dummy_payload(1, 0xAA);
        m_tail_page = createOggPage(1234, 100, 100000, false, true, dummy_payload);

        m_file_size = 10 * 1024 * 1024; // 10 MB
    }

    size_t read(void* buffer, size_t size, size_t count) override {
        // Only delay on read, to simulate slow download/IO
        if (m_delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(m_delay_ms));
        }

        size_t bytes_requested = size * count;
        if (bytes_requested == 0) return 0;

        size_t available = m_file_size - m_position;
        size_t to_read = std::min(bytes_requested, available);

        uint8_t* out = static_cast<uint8_t*>(buffer);
        std::memset(out, 0, to_read);

        // If reading from beginning
        if (m_position < m_head_page.size()) {
            size_t copy_len = std::min(to_read, m_head_page.size() - m_position);
            std::memcpy(out, m_head_page.data() + m_position, copy_len);
        }

        // If reading from end
        size_t tail_start = m_file_size - m_tail_page.size();

        // Check intersection with tail
        size_t read_start = m_position;
        size_t read_end = m_position + to_read;

        size_t overlap_start = std::max(read_start, tail_start);
        size_t overlap_end = std::min(read_end, m_file_size);

        if (overlap_start < overlap_end) {
            size_t overlap_len = overlap_end - overlap_start;
            size_t dest_offset = overlap_start - read_start;
            size_t src_offset = overlap_start - tail_start;

            std::memcpy(out + dest_offset, m_tail_page.data() + src_offset, overlap_len);
        }

        m_position += to_read;
        return to_read / size;
    }

    int seek(off_t offset, int whence) override {
        off_t new_pos = 0;
        switch(whence) {
            case SEEK_SET: new_pos = offset; break;
            case SEEK_CUR: new_pos = m_position + offset; break;
            case SEEK_END: new_pos = m_file_size + offset; break;
        }

        if (new_pos < 0) new_pos = 0;
        if (new_pos > static_cast<off_t>(m_file_size)) new_pos = m_file_size;

        m_position = new_pos;
        return 0;
    }

    off_t tell() override {
        return m_position;
    }

    bool eof() override {
        return m_position >= m_file_size;
    }

    off_t getFileSize() override {
        return m_file_size;
    }
};

int main(int argc, char** argv) {
    bool verify_eventual = false;
    if (argc > 1 && std::string(argv[1]) == "--verify-eventual") {
        verify_eventual = true;
    }

    // 100ms delay per read to simulate very slow I/O
    auto handler = std::make_unique<DelayedMockIOHandler>(100);
    OggDemuxer demuxer(std::move(handler));

    std::cout << "Parsing container..." << std::endl;
    // We expect parseContainer to take some time due to header reading,
    // but getDuration should ideally be instant (after optimization).
    if (!demuxer.parseContainer()) {
        std::cerr << "Failed to parse container" << std::endl;
        return 1;
    }

    std::cout << "Calling getDuration()..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    uint64_t duration = demuxer.getDuration();
    auto end = std::chrono::high_resolution_clock::now();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "getDuration() returned: " << duration << " ms" << std::endl;
    std::cout << "Time taken: " << ms << " ms" << std::endl;

    if (verify_eventual) {
        if (duration > 0) {
            std::cout << "Duration already calculated." << std::endl;
        } else {
            std::cout << "Waiting for async calculation..." << std::endl;
            int retries = 50;
            while (retries-- > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                duration = demuxer.getDuration();
                if (duration > 0) {
                     std::cout << "Async calculation finished. Duration: " << duration << std::endl;
                     break;
                }
            }
            if (duration == 0) {
                std::cout << "Async calculation TIMED OUT." << std::endl;
                return 1;
            }
        }
    }

    // Pass if FAST (< 50ms)
    // Fail if SLOW (> 50ms)
    // But initially (baseline) we expect SLOW.
    // So main returns 0 if execution successful, but prints FAST/SLOW.

    if (ms > 50) {
        std::cout << "RESULT: SLOW" << std::endl;
    } else {
        std::cout << "RESULT: FAST" << std::endl;
    }

    return 0;
}
