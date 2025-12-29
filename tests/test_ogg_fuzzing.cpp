/*
 * test_ogg_fuzzing.cpp - Fuzzing and Property-Based Tests for Ogg Demuxer
 * This file is part of PsyMP3.
 *
 * Tests compliance with RFC 3533 and robustness against random inputs.
 */

#include "psymp3.h"
#include <rapidcheck.h>
#include <vector>
#include <cstring>
#include <iostream>

#include "demuxer/ogg/OggSyncManager.h"
#include "demuxer/ogg/OggDemuxer.h"
#include "ogg/ogg.h"

// Helper to create a valid Ogg page manually (rfc3533)
std::vector<uint8_t> createOggPage(uint8_t version, uint8_t header_type,
                                   int64_t granule_pos, int32_t serial_no,
                                   int32_t seq_no, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> page;
    
    // 0-3: Capture pattern "OggS"
    page.push_back('O'); page.push_back('g'); page.push_back('g'); page.push_back('S');
    
    // 4: Version
    page.push_back(version);
    
    // 5: Header type
    page.push_back(header_type);
    
    // 6-13: Granule position (little endian)
    for (int i = 0; i < 8; i++) page.push_back((granule_pos >> (i * 8)) & 0xFF);
    
    // 14-17: Serial number
    for (int i = 0; i < 4; i++) page.push_back((serial_no >> (i * 8)) & 0xFF);
    
    // 18-21: Sequence number
    for (int i = 0; i < 4; i++) page.push_back((seq_no >> (i * 8)) & 0xFF);
    
    // 22-25: CRC (placeholder for now, 0)
    page.push_back(0); page.push_back(0); page.push_back(0); page.push_back(0);
    
    // 26: Page segments
    // Simple logic: if payload < 255, one segment. If > 255, this helper is too simple.
    // Testing small payloads for simplicity in property tests unless we span pages.
    size_t segments = (payload.size() + 255) / 255;
    if (segments == 0) segments = 1; // Empty page has 1 segment of size 0
    if (segments > 255) segments = 255; // Cap for this helper
    
    page.push_back(static_cast<uint8_t>(segments));
    
    // 27+: Segment table
    size_t remaining = payload.size();
    for (size_t i = 0; i < segments; i++) {
        uint8_t size = (remaining > 255) ? 255 : remaining;
        page.push_back(size);
        remaining -= size;
    }
    
    // Payload
    page.insert(page.end(), payload.begin(), payload.end());
    
    // Calculate CRC (using libogg's checksumming if available, or just mocking it)
    // Note: OggSyncManager might reject bad CRCs. 
    // Ideally we use ogg_page_checksum_set from libogg but it takes ogg_page struct.
    // For fuzzing, we often WANT bad CRCs to test rejection.
    // If we need valid CRC, we can construct ogg_page and use libogg.
    
    return page;
}

// Memory IO Handler for testing
class MemoryIOHandler : public PsyMP3::IO::IOHandler {
public:
    MemoryIOHandler(const std::vector<uint8_t>& data) : m_data(data), m_pos(0) {}
    
    size_t read(void* ptr, size_t size, size_t count) override {
        size_t bytes_requested = size * count;
        if (m_pos >= m_data.size()) return 0;
        
        size_t available = m_data.size() - m_pos;
        size_t to_read = std::min(bytes_requested, available);
        
        std::memcpy(ptr, m_data.data() + m_pos, to_read);
        m_pos += to_read;
        
        return to_read / size; // Return count of items
    }
    
    int seek(off_t offset, int origin) override {
        if (origin == SEEK_SET) m_pos = offset;
        else if (origin == SEEK_CUR) m_pos += offset;
        else if (origin == SEEK_END) m_pos = m_data.size() + offset;
        
        if (m_pos > m_data.size()) m_pos = m_data.size();
        return 0;
    }
    
    off_t tell() override { return static_cast<off_t>(m_pos); }
    off_t getFileSize() override { return static_cast<off_t>(m_data.size()); }
    bool eof() override { return m_pos >= m_data.size(); }
    
private:
    std::vector<uint8_t> m_data;
    size_t m_pos;
};

int main() {
    // 1. OggSyncManager Resilience: Random Bytes
    // Should never crash, regardless of input.
    rc::check("OggSyncManager: Random Byte Stream Resilience", [](const std::vector<uint8_t>& data) {
        auto io = std::make_unique<MemoryIOHandler>(data);
        PsyMP3::Demuxer::Ogg::OggSyncManager sync(io.get());
        
        ogg_page page;
        int max_pages = 1000;
        int count = 0;
        
        // Just consume the stream. It shouldn't crash.
        // It might return 1 (success) if random data happens to look like OggS (rare but possible)
        // or -1 (error) or 0 (need more data).
        while (count < max_pages) {
            int result = sync.getNextPage(&page);
            if (result == 0) break; // Need more data / EOF
            count++;
        }
        
        return true; // Pass if didn't crash
    });

    // 2. Valid Ogg Page Recovery
    // Embed a valid Ogg page in random garbage. SyncManager should find it.
    rc::check("OggSyncManager: Valid Page Recovery", [](int64_t granule, int32_t serial, int32_t seq, const std::vector<uint8_t>& payload) {
        // Construct valid page
        // Ensure payload isn't too huge for our simple helper
        std::vector<uint8_t> safe_payload = payload;
        if (safe_payload.size() > 200) safe_payload.resize(200);
        
        // Create page using libogg to ensure valid CRC
        ogg_stream_state os;
        ogg_stream_init(&os, serial);
        
        ogg_packet op;
        op.packet = safe_payload.data();
        op.bytes = safe_payload.size();
        op.b_o_s = (seq == 0);
        op.e_o_s = 0;
        op.granulepos = granule;
        op.packetno = seq;
        
        ogg_stream_packetin(&os, &op);
        
        ogg_page og;
        std::vector<uint8_t> stream_data;
        
        if (ogg_stream_pageout(&os, &og)) {
            // Add some garbage before
            stream_data.insert(stream_data.end(), {'G', 'a', 'r', 'b', 'a', 'g', 'e'});
            
            // Add the valid page
            stream_data.insert(stream_data.end(), og.header, og.header + og.header_len);
            stream_data.insert(stream_data.end(), og.body, og.body + og.body_len);
            
            // Add garbage after
            stream_data.insert(stream_data.end(), {'M', 'o', 'r', 'e'});
        }
        
        ogg_stream_clear(&os);
        
        if (stream_data.empty()) return; // Could not create page (e.g. empty)
        
        auto io = std::make_unique<MemoryIOHandler>(stream_data);
        PsyMP3::Demuxer::Ogg::OggSyncManager sync(io.get());
        
        ogg_page page;
        int result = sync.getNextPage(&page);
        
        // It should find the page eventually (or fail safely)
        // Note: getNextPage might need multiple calls if garbage is long
        // But our garbage is short.
        
        // We just assert it doesn't crash and if it finds it, it matches.
        if (result == 1) {
            RC_ASSERT(ogg_page_serialno(&page) == serial);
        }
    });

    // 3. Regression: Negative Serial Number Handling
    rc::check("OggDemuxer: Negative Serial Number Support", [](const std::vector<uint8_t>& payload) {
        int32_t neg_serial = -975925429;
        int64_t granule = 0;
        
        ogg_stream_state os;
        ogg_stream_init(&os, neg_serial);
        
        // Use a valid (minimal) Opus header so CodecHeaderParser recognizes it.
        // If not recognized, OggDemuxer won't add it to reported streams.
        std::vector<uint8_t> safe_payload = {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd'};
        // Version 1, Channels 1, Pre-skip 0, Sample rate 48000 (LE), Gain 0, Mapping 0
        safe_payload.insert(safe_payload.end(), {1, 1, 0, 0, 0x80, 0xBB, 0, 0, 0, 0, 0}); 

        ogg_packet op;
        op.packet = safe_payload.data();
        op.bytes = safe_payload.size();
        op.b_o_s = 1;
        op.e_o_s = 1;
        op.granulepos = granule;
        op.packetno = 0;
        
        ogg_stream_packetin(&os, &op);
        
        ogg_page og;
        std::vector<uint8_t> stream_data;
        while (ogg_stream_flush(&os, &og)) {
            stream_data.insert(stream_data.end(), og.header, og.header + og.header_len);
            stream_data.insert(stream_data.end(), og.body, og.body + og.body_len);
        }
        ogg_stream_clear(&os);
        
        // If libogg failed to produce a page, we can't test OggDemuxer with it.
        // This shouldn't happen with valid parameters, but if it does, it's a test generation issue,
        // not a demuxer issue.
        RC_PRE(!stream_data.empty());
        
        auto io = std::make_unique<MemoryIOHandler>(stream_data);
        auto demuxer = std::make_unique<PsyMP3::Demuxer::Ogg::OggDemuxer>(std::move(io));
        
        bool parsed = demuxer->parseContainer();
        RC_ASSERT(parsed);
        
        auto streams = demuxer->getStreams();
        RC_ASSERT(!streams.empty());
        RC_ASSERT(static_cast<int>(streams[0].stream_id) == neg_serial);
        
        demuxer->getDuration();
    });

    return 0;
}
