/*
 * test_ogg_multiplexed_seeking.cpp - Unit tests for Ogg seeking in multiplexed streams
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * This test validates that OggSeekingEngine correctly handles seeking operations
 * when multiple logical streams (multiplexing) are present in the same physical
 * file, ensuring it ignores pages from streams other than the primary one.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#ifdef HAVE_OGGDEMUXER

#include <algorithm>
#include <cmath>

// --- Mock Classes ---

class MockIOHandler : public PsyMP3::IO::IOHandler {
private:
    std::vector<uint8_t> m_data;
    size_t m_position = 0;

public:
    MockIOHandler(const std::vector<uint8_t>& data) : m_data(data) {}
    
    // Minimal IOHandler implementation for read/seek
    size_t read(void* buffer, size_t size, size_t count) override {
        if (size == 0 || count == 0) return 0;
        size_t bytes_requested = size * count;
        size_t bytes_available = m_data.size() - m_position;
        size_t bytes_to_read = std::min(bytes_requested, bytes_available);
        
        if (bytes_to_read > 0) {
            std::memcpy(buffer, m_data.data() + m_position, bytes_to_read);
            m_position += bytes_to_read;
        }
        return bytes_to_read / size;
    }
    
    int seek(off_t offset, int whence) override {
        switch (whence) {
            case SEEK_SET:
                m_position = std::min(static_cast<size_t>(offset), m_data.size());
                break;
            case SEEK_CUR:
                if (offset < 0 && static_cast<size_t>(-offset) > m_position) {
                    m_position = 0;
                } else {
                    m_position = std::min(m_position + offset, m_data.size());
                }
                break;
            case SEEK_END:
                m_position = m_data.size();
                break;
        }
        return 0;
    }
    
    off_t tell() override {
        return static_cast<off_t>(m_position);
    }
    
    bool eof() override {
        return m_position >= m_data.size();
    }
    
    off_t getFileSize() override {
        return static_cast<off_t>(m_data.size());
    }
};

// --- Test Implementation ---

// Uses libogg to properly encapsulate data into pages
std::vector<uint8_t> create_multiplexed_stream(uint32_t primary_serial, uint32_t secondary_serial) {
    std::vector<uint8_t> buffer;
    
    ogg_stream_state os_primary;
    ogg_stream_state os_secondary;
    
    ogg_stream_init(&os_primary, primary_serial);
    ogg_stream_init(&os_secondary, secondary_serial);
    
    // Create interleaved pages
    // Pattern: Primary Page 1, Secondary Page 1, Primary Page 2 (Target), Secondary Page 2
    
    // Primary Page 1 (Granule 0)
    ogg_packet op;
    memset(&op, 0, sizeof(op));
    op.packet = (unsigned char*)"primary_packet_1";
    op.bytes = 16;
    op.b_o_s = 1;
    op.e_o_s = 0;
    op.granulepos = 0;
    op.packetno = 0;
    ogg_stream_packetin(&os_primary, &op);
    
    ogg_page og;
    while(ogg_stream_flush(&os_primary, &og)) {
        buffer.insert(buffer.end(), og.header, og.header + og.header_len);
        buffer.insert(buffer.end(), og.body, og.body + og.body_len);
    }
    
    // Secondary Page 1 (Granule 0)
    memset(&op, 0, sizeof(op));
    op.packet = (unsigned char*)"secondary_packet_1";
    op.bytes = 18;
    op.b_o_s = 1;
    op.e_o_s = 0;
    op.granulepos = 0;
    op.packetno = 0;
    ogg_stream_packetin(&os_secondary, &op);
    
    while(ogg_stream_flush(&os_secondary, &og)) {
        buffer.insert(buffer.end(), og.header, og.header + og.header_len);
        buffer.insert(buffer.end(), og.body, og.body + og.body_len);
    }
    
    // Primary Page 2 (Granule 1000) - This is our seek target area
    memset(&op, 0, sizeof(op));
    op.packet = (unsigned char*)"primary_packet_2";
    op.bytes = 16;
    op.b_o_s = 0;
    op.e_o_s = 0;
    op.granulepos = 1000;
    op.packetno = 1;
    ogg_stream_packetin(&os_primary, &op);
    
    while(ogg_stream_flush(&os_primary, &og)) {
        buffer.insert(buffer.end(), og.header, og.header + og.header_len);
        buffer.insert(buffer.end(), og.body, og.body + og.body_len);
    }
    
    // Secondary Page 2 (Granule 500) - Interleaved noise
    memset(&op, 0, sizeof(op));
    op.packet = (unsigned char*)"secondary_packet_2";
    op.bytes = 18;
    op.b_o_s = 0;
    op.e_o_s = 0;
    op.granulepos = 500;
    op.packetno = 1;
    ogg_stream_packetin(&os_secondary, &op);
    
    while(ogg_stream_flush(&os_secondary, &og)) {
        buffer.insert(buffer.end(), og.header, og.header + og.header_len);
        buffer.insert(buffer.end(), og.body, og.body + og.body_len);
    }
    
    ogg_stream_clear(&os_primary);
    ogg_stream_clear(&os_secondary);
    
    return buffer;
}

void test_multiplexed_seeking_rejection() {
    std::cout << "Running Serial Number Validation Test..." << std::endl;
    
    uint32_t primary_serial = 12345;
    uint32_t secondary_serial = 67890;
    
    std::vector<uint8_t> data = create_multiplexed_stream(primary_serial, secondary_serial);
    std::cout << "Generated Ogg data size: " << data.size() << " bytes" << std::endl;
    
    MockIOHandler handler(data);
    
    PsyMP3::Demuxer::Ogg::OggSyncManager sync_manager(&handler);
    PsyMP3::Demuxer::Ogg::OggStreamManager stream_manager(primary_serial);
    PsyMP3::Demuxer::Ogg::OggSeekingEngine seeking_engine(sync_manager, stream_manager);

    // Pre-verification: List all pages
    {
        ogg_page page;
        int page_count = 0;
        int found_pages = 0;
        while (sync_manager.getNextPage(&page) == 1) {
            std::cout << "Page " << page_count++ << ": Serial=" << ogg_page_serialno(&page) 
                      << " Granule=" << ogg_page_granulepos(&page) << std::endl;
            found_pages++;
        }
        std::cout << "Total pages found in pre-scan: " << found_pages << std::endl;
        sync_manager.reset(); // Reset for actual test
        handler.seek(0, SEEK_SET);
    }
    
    // Configure seeking engine
    seeking_engine.setSampleRate(44100);
    
    // Let's try to find the last granule in the file for the primary stream.
    uint64_t last_granule = seeking_engine.getLastGranule();
    
    std::cout << "Last Granule Found: " << last_granule << std::endl;
    
    if (last_granule != 1000) {
        std::cerr << "FAILURE: Expected last granule 1000 (primary), got " << last_granule << std::endl;
        exit(1);
    }
    
    std::cout << "PASSED: Correctly ignored secondary stream pages." << std::endl;
}

int main() {
    try {
        test_multiplexed_seeking_rejection();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test Exception: " << e.what() << std::endl;
        return 1;
    }
}

#else

int main() {
    std::cout << "OggDemuxer disabled, skipping test." << std::endl;
    return 0;
}

#endif
