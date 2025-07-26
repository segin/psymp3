/*
 * test_ogg_duration_calculation.cpp - Unit tests for OggDemuxer duration calculation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifdef HAVE_OGGDEMUXER

#include <iostream>
#include <cassert>
#include <memory>
#include <vector>

// Mock IOHandler for testing
class MockIOHandlerForDuration : public IOHandler {
public:
    MockIOHandlerForDuration() : m_data(), m_position(0) {}
    
    size_t read(void* buffer, size_t size, size_t count) override {
        size_t bytes_to_read = size * count;
        size_t available = m_data.size() - m_position;
        size_t actual_read = std::min(bytes_to_read, available);
        
        if (actual_read > 0) {
            std::memcpy(buffer, m_data.data() + m_position, actual_read);
            m_position += actual_read;
        }
        
        return actual_read / size;
    }
    
    int seek(long offset, int whence) override {
        switch (whence) {
            case SEEK_SET:
                m_position = offset;
                break;
            case SEEK_CUR:
                m_position += offset;
                break;
            case SEEK_END:
                m_position = m_data.size() + offset;
                break;
        }
        
        if (m_position < 0) m_position = 0;
        if (m_position > m_data.size()) m_position = m_data.size();
        
        return 0;
    }
    
    long tell() override {
        return m_position;
    }
    
    bool eof() override {
        return m_position >= m_data.size();
    }
    
    off_t getFileSize() override {
        return m_data.size();
    }
    
    void setData(const std::vector<uint8_t>& data) {
        m_data = data;
        m_position = 0;
    }
    
    // Helper to create mock Ogg page data
    void createMockOggFile(const std::vector<uint64_t>& granule_positions) {
        m_data.clear();
        
        for (size_t i = 0; i < granule_positions.size(); i++) {
            // Create a minimal Ogg page header
            std::vector<uint8_t> page_header(27, 0);
            
            // OggS signature
            page_header[0] = 'O';
            page_header[1] = 'g';
            page_header[2] = 'g';
            page_header[3] = 'S';
            
            // Version
            page_header[4] = 0;
            
            // Header type (0 for normal page, 4 for last page)
            page_header[5] = (i == granule_positions.size() - 1) ? 4 : 0;
            
            // Granule position (little-endian)
            uint64_t granule = granule_positions[i];
            for (int j = 0; j < 8; j++) {
                page_header[6 + j] = (granule >> (j * 8)) & 0xFF;
            }
            
            // Serial number
            uint32_t serial = 12345;
            for (int j = 0; j < 4; j++) {
                page_header[14 + j] = (serial >> (j * 8)) & 0xFF;
            }
            
            // Page sequence
            uint32_t sequence = i;
            for (int j = 0; j < 4; j++) {
                page_header[18 + j] = (sequence >> (j * 8)) & 0xFF;
            }
            
            // Checksum (we'll leave as 0 for testing)
            // page_header[22-25] = 0
            
            // Page segments
            page_header[26] = 1; // One segment
            
            // Add header to data
            m_data.insert(m_data.end(), page_header.begin(), page_header.end());
            
            // Add segment table (1 byte indicating segment size)
            m_data.push_back(100); // 100 byte segment
            
            // Add dummy packet data (100 bytes)
            for (int j = 0; j < 100; j++) {
                m_data.push_back(0xAA);
            }
        }
    }
    
private:
    std::vector<uint8_t> m_data;
    size_t m_position;
};

// Test helper class to access duration calculation methods
class TestableOggDemuxerForDuration : public OggDemuxer {
public:
    TestableOggDemuxerForDuration(std::unique_ptr<IOHandler> handler) : OggDemuxer(std::move(handler)) {}
    
    // Expose methods for testing
    uint64_t testGetLastGranulePosition() {
        return getLastGranulePosition();
    }
    
    uint64_t testScanBufferForLastGranule(const std::vector<uint8_t>& buffer, size_t buffer_size) {
        return scanBufferForLastGranule(buffer, buffer_size);
    }
    
    uint64_t testScanForwardForLastGranule(long start_position) {
        return scanForwardForLastGranule(start_position);
    }
    
    uint64_t testGetLastGranuleFromHeaders() {
        return getLastGranuleFromHeaders();
    }
    
    // Helper to add test streams with total_samples
    void addTestStreamWithSamples(uint32_t stream_id, const std::string& codec_name, 
                                 uint32_t sample_rate, uint16_t channels, 
                                 uint64_t total_samples, uint64_t pre_skip = 0) {
        OggStream stream;
        stream.serial_number = stream_id;
        stream.codec_name = codec_name;
        stream.codec_type = "audio";
        stream.sample_rate = sample_rate;
        stream.channels = channels;
        stream.total_samples = total_samples;
        stream.pre_skip = pre_skip;
        stream.headers_complete = true;
        
        getStreamsForTesting()[stream_id] = stream;
    }
    

};

void test_scan_buffer_for_last_granule() {
    std::cout << "Testing scanBufferForLastGranule..." << std::endl;
    
    auto mock_handler = std::make_unique<MockIOHandlerForDuration>();
    TestableOggDemuxerForDuration demuxer(std::move(mock_handler));
    
    // Create a buffer with multiple Ogg pages
    std::vector<uint8_t> buffer;
    
    // Add first page with granule 1000
    std::vector<uint8_t> page1 = {
        'O', 'g', 'g', 'S',  // OggS signature
        0,                   // Version
        0,                   // Header type
        // Granule position (little-endian): 1000
        0xE8, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // Serial number: 12345
        0x39, 0x30, 0x00, 0x00,
        // Page sequence: 0
        0x00, 0x00, 0x00, 0x00,
        // Checksum: 0
        0x00, 0x00, 0x00, 0x00,
        // Page segments: 1
        0x01,
        // Segment table
        0x10, // 16 bytes
    };
    
    // Add 16 bytes of dummy data
    for (int i = 0; i < 16; i++) {
        page1.push_back(0xAA);
    }
    
    buffer.insert(buffer.end(), page1.begin(), page1.end());
    
    // Add second page with granule 2000
    std::vector<uint8_t> page2 = {
        'O', 'g', 'g', 'S',  // OggS signature
        0,                   // Version
        0,                   // Header type
        // Granule position (little-endian): 2000
        0xD0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // Serial number: 12345
        0x39, 0x30, 0x00, 0x00,
        // Page sequence: 1
        0x01, 0x00, 0x00, 0x00,
        // Checksum: 0
        0x00, 0x00, 0x00, 0x00,
        // Page segments: 1
        0x01,
        // Segment table
        0x10, // 16 bytes
    };
    
    // Add 16 bytes of dummy data
    for (int i = 0; i < 16; i++) {
        page2.push_back(0xBB);
    }
    
    buffer.insert(buffer.end(), page2.begin(), page2.end());
    
    // Test scanning
    uint64_t result = demuxer.testScanBufferForLastGranule(buffer, buffer.size());
    
    // Should find the highest granule position (2000)
    if (result != 2000) {
        std::cerr << "FAIL: Expected granule 2000, got " << result << std::endl;
        assert(false);
    }
    
    std::cout << "✓ scanBufferForLastGranule correctly found highest granule position" << std::endl;
    
    // Test with empty buffer
    std::vector<uint8_t> empty_buffer;
    result = demuxer.testScanBufferForLastGranule(empty_buffer, 0);
    assert(result == 0);
    std::cout << "✓ scanBufferForLastGranule handles empty buffer" << std::endl;
    
    // Test with invalid data
    std::vector<uint8_t> invalid_buffer = {'X', 'Y', 'Z', 'W', 0, 0, 0, 0};
    result = demuxer.testScanBufferForLastGranule(invalid_buffer, invalid_buffer.size());
    assert(result == 0);
    std::cout << "✓ scanBufferForLastGranule handles invalid data" << std::endl;
    
    std::cout << "✓ scanBufferForLastGranule tests passed" << std::endl;
}

void test_get_last_granule_from_headers() {
    std::cout << "Testing getLastGranuleFromHeaders..." << std::endl;
    
    auto mock_handler = std::make_unique<MockIOHandlerForDuration>();
    TestableOggDemuxerForDuration demuxer(std::move(mock_handler));
    
    // Test with no streams
    uint64_t result = demuxer.testGetLastGranuleFromHeaders();
    assert(result == 0);
    std::cout << "✓ getLastGranuleFromHeaders returns 0 for no streams" << std::endl;
    
    // Add Vorbis stream with total samples
    demuxer.addTestStreamWithSamples(1, "vorbis", 44100, 2, 132300); // 3 seconds at 44.1kHz
    result = demuxer.testGetLastGranuleFromHeaders();
    assert(result == 132300);
    std::cout << "✓ getLastGranuleFromHeaders works for Vorbis stream" << std::endl;
    
    // Add Opus stream with pre-skip
    demuxer.addTestStreamWithSamples(2, "opus", 48000, 2, 144000, 312); // 3 seconds at 48kHz + pre-skip
    result = demuxer.testGetLastGranuleFromHeaders();
    assert(result == 144000 + 312); // Should include pre-skip for Opus
    std::cout << "✓ getLastGranuleFromHeaders works for Opus stream with pre-skip" << std::endl;
    
    // Add FLAC stream
    demuxer.addTestStreamWithSamples(3, "flac", 44100, 2, 88200); // 2 seconds at 44.1kHz
    result = demuxer.testGetLastGranuleFromHeaders();
    assert(result == 144000 + 312); // Should still be the Opus stream (highest)
    std::cout << "✓ getLastGranuleFromHeaders returns highest granule from multiple streams" << std::endl;
    
    std::cout << "✓ getLastGranuleFromHeaders tests passed" << std::endl;
}

void test_get_last_granule_position_integration() {
    std::cout << "Testing getLastGranulePosition integration..." << std::endl;
    
    auto mock_handler = std::make_unique<MockIOHandlerForDuration>();
    auto* mock_ptr = mock_handler.get();
    TestableOggDemuxerForDuration demuxer(std::move(mock_handler));
    
    // Create mock file with multiple pages
    std::vector<uint64_t> granule_positions = {1000, 2000, 3000, 4000, 5000};
    mock_ptr->createMockOggFile(granule_positions);
    
    // Initialize the file size in the demuxer (normally done in parseContainer)
    demuxer.setFileSizeForTesting(mock_ptr->getFileSize());
    
    // Test getting last granule position
    uint64_t result = demuxer.testGetLastGranulePosition();
    
    // Debug: check file size and content
    std::cout << "Debug: Mock file size = " << mock_ptr->getFileSize() << std::endl;
    
    // Should find the last granule position (5000)
    if (result != 5000) {
        std::cerr << "FAIL: Expected granule 5000, got " << result << std::endl;
        
        // Let's try the buffer scan directly to see what's happening
        std::vector<uint8_t> debug_buffer(mock_ptr->getFileSize());
        mock_ptr->seek(0, SEEK_SET);
        size_t bytes_read = mock_ptr->read(debug_buffer.data(), 1, debug_buffer.size());
        std::cout << "Debug: Read " << bytes_read << " bytes from mock file" << std::endl;
        
        uint64_t buffer_result = demuxer.testScanBufferForLastGranule(debug_buffer, bytes_read);
        std::cout << "Debug: Buffer scan result = " << buffer_result << std::endl;
        
        assert(false);
    }
    
    std::cout << "✓ getLastGranulePosition found correct last granule" << std::endl;
    
    // Test with empty file
    mock_ptr->setData(std::vector<uint8_t>());
    demuxer.setFileSizeForTesting(0);
    result = demuxer.testGetLastGranulePosition();
    assert(result == 0);
    std::cout << "✓ getLastGranulePosition handles empty file" << std::endl;
    
    // Test fallback to headers when file scanning fails
    std::vector<uint8_t> invalid_data = {'i', 'n', 'v', 'a', 'l', 'i', 'd'};
    mock_ptr->setData(invalid_data);
    demuxer.setFileSizeForTesting(invalid_data.size());
    demuxer.addTestStreamWithSamples(1, "vorbis", 44100, 2, 88200); // 2 seconds
    
    result = demuxer.testGetLastGranulePosition();
    assert(result == 88200); // Should fall back to header info
    std::cout << "✓ getLastGranulePosition falls back to header info when scanning fails" << std::endl;
    
    std::cout << "✓ getLastGranulePosition integration tests passed" << std::endl;
}

void test_duration_calculation_edge_cases() {
    std::cout << "Testing duration calculation edge cases..." << std::endl;
    
    auto mock_handler = std::make_unique<MockIOHandlerForDuration>();
    auto* mock_ptr = mock_handler.get();
    TestableOggDemuxerForDuration demuxer(std::move(mock_handler));
    
    // Test with very large file (should use multiple scan sizes)
    std::vector<uint64_t> many_granules;
    for (int i = 1; i <= 100; i++) {
        many_granules.push_back(i * 1000);
    }
    mock_ptr->createMockOggFile(many_granules);
    demuxer.setFileSizeForTesting(mock_ptr->getFileSize());
    
    uint64_t result = demuxer.testGetLastGranulePosition();
    assert(result == 100000); // Last granule should be 100 * 1000
    std::cout << "✓ getLastGranulePosition handles large files with many pages" << std::endl;
    
    // Test with corrupted pages mixed with valid ones
    std::vector<uint8_t> mixed_data;
    
    // Add some garbage data
    for (int i = 0; i < 1000; i++) {
        mixed_data.push_back(0xFF);
    }
    
    // Add a valid page
    std::vector<uint8_t> valid_page = {
        'O', 'g', 'g', 'S', 0, 0,
        0x10, 0x27, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // granule 10000
        0x39, 0x30, 0x00, 0x00, // serial
        0x00, 0x00, 0x00, 0x00, // sequence
        0x00, 0x00, 0x00, 0x00, // checksum
        0x01, 0x10 // segments and segment table
    };
    
    // Add 16 bytes of data
    for (int i = 0; i < 16; i++) {
        valid_page.push_back(0xCC);
    }
    
    mixed_data.insert(mixed_data.end(), valid_page.begin(), valid_page.end());
    
    // Add more garbage
    for (int i = 0; i < 500; i++) {
        mixed_data.push_back(0x00);
    }
    
    mock_ptr->setData(mixed_data);
    demuxer.setFileSizeForTesting(mixed_data.size());
    result = demuxer.testGetLastGranulePosition();
    assert(result == 10000);
    std::cout << "✓ getLastGranulePosition handles corrupted data mixed with valid pages" << std::endl;
    
    std::cout << "✓ Duration calculation edge case tests passed" << std::endl;
}

int main() {
    std::cout << "Running OggDemuxer duration calculation tests..." << std::endl;
    
    try {
        test_scan_buffer_for_last_granule();
        test_get_last_granule_from_headers();
        test_get_last_granule_position_integration();
        test_duration_calculation_edge_cases();
        
        std::cout << std::endl << "✓ All OggDemuxer duration calculation tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}

#else
int main() {
    std::cout << "OggDemuxer not available (HAVE_OGGDEMUXER not defined)" << std::endl;
    return 0;
}
#endif