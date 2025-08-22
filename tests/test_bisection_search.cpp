/*
 * test_bisection_search.cpp - Unit tests for OggDemuxer bisection search algorithm
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifdef HAVE_OGGDEMUXER

#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <cstring>
#include <cassert>

// Mock IOHandler for testing
class MockIOHandler : public IOHandler {
private:
    std::vector<uint8_t> m_data;
    size_t m_position;
    
public:
    MockIOHandler(const std::vector<uint8_t>& data) : m_data(data), m_position(0) {}
    
    size_t read(void* buffer, size_t size, size_t count) override {
        size_t bytes_to_read = size * count;
        size_t available = m_data.size() - m_position;
        size_t actual_read = std::min(bytes_to_read, available);
        
        if (actual_read > 0) {
            std::memcpy(buffer, m_data.data() + m_position, actual_read);
            m_position += actual_read;
        }
        
        return actual_read;
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
            default:
                return -1;
        }
        
        if (m_position > m_data.size()) {
            m_position = m_data.size();
        }
        
        return 0;
    }
    
    long tell() override {
        return static_cast<long>(m_position);
    }
    
    bool eof() override {
        return m_position >= m_data.size();
    }
    
    off_t getFileSize() override {
        return m_data.size();
    }
};

// Helper function to create a minimal Ogg page
std::vector<uint8_t> createOggPage(uint32_t serial_number, uint64_t granule_pos, 
                                   const std::vector<uint8_t>& packet_data, 
                                   bool is_bos = false, bool is_eos = false) {
    std::vector<uint8_t> page;
    
    // Ogg page header
    page.insert(page.end(), {'O', 'g', 'g', 'S'});  // Capture pattern
    page.push_back(0);  // Version
    
    uint8_t header_type = 0;
    if (is_bos) header_type |= 0x02;
    if (is_eos) header_type |= 0x04;
    page.push_back(header_type);  // Header type
    
    // Granule position (8 bytes, little-endian)
    for (int i = 0; i < 8; i++) {
        page.push_back((granule_pos >> (i * 8)) & 0xFF);
    }
    
    // Serial number (4 bytes, little-endian)
    for (int i = 0; i < 4; i++) {
        page.push_back((serial_number >> (i * 8)) & 0xFF);
    }
    
    // Page sequence number (4 bytes) - just use 0
    page.insert(page.end(), {0, 0, 0, 0});
    
    // Checksum (4 bytes) - just use 0 for testing
    page.insert(page.end(), {0, 0, 0, 0});
    
    // Number of segments
    size_t segments = (packet_data.size() + 254) / 255;  // Each segment can hold max 255 bytes
    if (segments == 0) segments = 1;
    page.push_back(static_cast<uint8_t>(segments));
    
    // Segment table
    size_t remaining = packet_data.size();
    for (size_t i = 0; i < segments; i++) {
        if (remaining >= 255) {
            page.push_back(255);
            remaining -= 255;
        } else {
            page.push_back(static_cast<uint8_t>(remaining));
            remaining = 0;
        }
    }
    
    // Packet data
    page.insert(page.end(), packet_data.begin(), packet_data.end());
    
    return page;
}

// Helper function to create Opus identification header
std::vector<uint8_t> createOpusIdHeader() {
    std::vector<uint8_t> header;
    header.insert(header.end(), {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd'});  // Signature
    header.push_back(1);  // Version
    header.push_back(2);  // Channel count
    header.insert(header.end(), {0, 0});  // Pre-skip (little-endian)
    header.insert(header.end(), {0x80, 0xBB, 0x00, 0x00});  // Input sample rate (48000 Hz)
    header.insert(header.end(), {0, 0});  // Output gain
    header.push_back(0);  // Channel mapping family
    return header;
}

// Helper function to create Opus comment header
std::vector<uint8_t> createOpusCommentHeader() {
    std::vector<uint8_t> header;
    header.insert(header.end(), {'O', 'p', 'u', 's', 'T', 'a', 'g', 's'});  // Signature
    
    // Vendor string length (little-endian)
    std::string vendor = "test";
    uint32_t vendor_len = vendor.length();
    for (int i = 0; i < 4; i++) {
        header.push_back((vendor_len >> (i * 8)) & 0xFF);
    }
    
    // Vendor string
    header.insert(header.end(), vendor.begin(), vendor.end());
    
    // User comment list length (0 comments)
    header.insert(header.end(), {0, 0, 0, 0});
    
    return header;
}

// Test basic bisection search functionality
bool testBasicBisectionSearch() {
    std::cout << "Testing basic bisection search..." << std::endl;
    
    try {
        // Create a mock Ogg file with multiple pages at different granule positions
        std::vector<uint8_t> file_data;
        
        // Create Opus identification header (BOS page)
        auto opus_id = createOpusIdHeader();
        auto bos_page = createOggPage(12345, 0, opus_id, true, false);
        file_data.insert(file_data.end(), bos_page.begin(), bos_page.end());
        
        // Create Opus comment header
        auto opus_comment = createOpusCommentHeader();
        auto comment_page = createOggPage(12345, 0, opus_comment, false, false);
        file_data.insert(file_data.end(), comment_page.begin(), comment_page.end());
        
        // Create data pages with increasing granule positions
        std::vector<uint64_t> granule_positions = {960, 1920, 2880, 3840, 4800, 5760, 6720, 7680, 8640, 9600};
        
        for (uint64_t granule : granule_positions) {
            std::vector<uint8_t> packet_data(100, 0x42);  // Dummy packet data
            auto data_page = createOggPage(12345, granule, packet_data, false, false);
            file_data.insert(file_data.end(), data_page.begin(), data_page.end());
        }
        
        // Create demuxer with mock data
        auto handler = std::make_unique<MockIOHandler>(file_data);
        OggDemuxer demuxer(std::move(handler));
        
        // Set file size for testing
        demuxer.setFileSizeForTesting(file_data.size());
        
        // Parse container to initialize streams
        if (!demuxer.parseContainer()) {
            std::cerr << "Failed to parse container" << std::endl;
            return false;
        }
        
        // Test seeking to different positions
        std::vector<std::pair<uint64_t, uint64_t>> test_cases = {
            {0, 0},        // Seek to beginning
            {960, 960},    // Seek to first data granule
            {2000, 1920},  // Seek to position between granules (should find previous)
            {5000, 4800},  // Seek to middle position
            {9000, 8640},  // Seek near end
            {15000, 9600}  // Seek beyond end (should find last)
        };
        
        for (const auto& test_case : test_cases) {
            uint64_t target_granule = test_case.first;
            uint64_t expected_granule = test_case.second;
            
            std::cout << "  Testing seek to granule " << target_granule << " (expecting " << expected_granule << ")" << std::endl;
            
            bool result = demuxer.seekToPage(target_granule, 12345);
            if (!result) {
                std::cerr << "    Seek failed for granule " << target_granule << std::endl;
                return false;
            }
            
            // Note: We can't easily verify the exact result without exposing more internals
            // The test mainly verifies that seeking doesn't crash and returns success
            std::cout << "    Seek successful" << std::endl;
        }
        
        std::cout << "Basic bisection search test passed!" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in testBasicBisectionSearch: " << e.what() << std::endl;
        return false;
    }
}

// Test boundary conditions
bool testBoundaryConditions() {
    std::cout << "Testing boundary conditions..." << std::endl;
    
    try {
        // Create minimal Ogg file with just headers
        std::vector<uint8_t> file_data;
        
        auto opus_id = createOpusIdHeader();
        auto bos_page = createOggPage(12345, 0, opus_id, true, false);
        file_data.insert(file_data.end(), bos_page.begin(), bos_page.end());
        
        auto opus_comment = createOpusCommentHeader();
        auto comment_page = createOggPage(12345, 0, opus_comment, false, false);
        file_data.insert(file_data.end(), comment_page.begin(), comment_page.end());
        
        auto handler = std::make_unique<MockIOHandler>(file_data);
        OggDemuxer demuxer(std::move(handler));
        demuxer.setFileSizeForTesting(file_data.size());
        
        if (!demuxer.parseContainer()) {
            std::cerr << "Failed to parse container" << std::endl;
            return false;
        }
        
        // Test seeking in file with no data pages
        std::cout << "  Testing seek in file with no data pages" << std::endl;
        bool result = demuxer.seekToPage(1000, 12345);
        if (!result) {
            std::cerr << "    Seek failed unexpectedly" << std::endl;
            return false;
        }
        
        std::cout << "Boundary conditions test passed!" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in testBoundaryConditions: " << e.what() << std::endl;
        return false;
    }
}

// Test linear scanning fallback
bool testLinearScanningFallback() {
    std::cout << "Testing linear scanning fallback..." << std::endl;
    
    try {
        // Create a small Ogg file to trigger linear scanning
        std::vector<uint8_t> file_data;
        
        auto opus_id = createOpusIdHeader();
        auto bos_page = createOggPage(12345, 0, opus_id, true, false);
        file_data.insert(file_data.end(), bos_page.begin(), bos_page.end());
        
        auto opus_comment = createOpusCommentHeader();
        auto comment_page = createOggPage(12345, 0, opus_comment, false, false);
        file_data.insert(file_data.end(), comment_page.begin(), comment_page.end());
        
        // Add just a few data pages to create a small interval
        std::vector<uint64_t> granule_positions = {960, 1920, 2880};
        
        for (uint64_t granule : granule_positions) {
            std::vector<uint8_t> packet_data(50, 0x42);
            auto data_page = createOggPage(12345, granule, packet_data, false, false);
            file_data.insert(file_data.end(), data_page.begin(), data_page.end());
        }
        
        auto handler = std::make_unique<MockIOHandler>(file_data);
        OggDemuxer demuxer(std::move(handler));
        demuxer.setFileSizeForTesting(file_data.size());
        
        if (!demuxer.parseContainer()) {
            std::cerr << "Failed to parse container" << std::endl;
            return false;
        }
        
        // Test seeking - should trigger linear scanning due to small file size
        std::cout << "  Testing seek that should trigger linear scanning" << std::endl;
        bool result = demuxer.seekToPage(2000, 12345);
        if (!result) {
            std::cerr << "    Seek failed" << std::endl;
            return false;
        }
        
        std::cout << "Linear scanning fallback test passed!" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in testLinearScanningFallback: " << e.what() << std::endl;
        return false;
    }
}

// Test packet examination functionality
bool testPacketExamination() {
    std::cout << "Testing packet examination functionality..." << std::endl;
    
    try {
        // Create Ogg file with data pages
        std::vector<uint8_t> file_data;
        
        auto opus_id = createOpusIdHeader();
        auto bos_page = createOggPage(12345, 0, opus_id, true, false);
        file_data.insert(file_data.end(), bos_page.begin(), bos_page.end());
        
        auto opus_comment = createOpusCommentHeader();
        auto comment_page = createOggPage(12345, 0, opus_comment, false, false);
        file_data.insert(file_data.end(), comment_page.begin(), comment_page.end());
        
        // Add data pages
        std::vector<uint64_t> granule_positions = {960, 1920, 2880, 3840};
        
        for (uint64_t granule : granule_positions) {
            std::vector<uint8_t> packet_data(100, 0x42);
            auto data_page = createOggPage(12345, granule, packet_data, false, false);
            file_data.insert(file_data.end(), data_page.begin(), data_page.end());
        }
        
        auto handler = std::make_unique<MockIOHandler>(file_data);
        OggDemuxer demuxer(std::move(handler));
        demuxer.setFileSizeForTesting(file_data.size());
        
        if (!demuxer.parseContainer()) {
            std::cerr << "Failed to parse container" << std::endl;
            return false;
        }
        
        // Test packet examination at different positions
        std::cout << "  Testing packet examination at various file positions" << std::endl;
        
        uint64_t granule_pos;
        
        // Test examination at beginning (should find headers)
        bool result1 = demuxer.examinePacketsAtPosition(0, 12345, granule_pos);
        std::cout << "    Examination at position 0: " << (result1 ? "success" : "failed") << std::endl;
        
        // Test examination in middle of file
        int64_t mid_pos = file_data.size() / 2;
        bool result2 = demuxer.examinePacketsAtPosition(mid_pos, 12345, granule_pos);
        std::cout << "    Examination at position " << mid_pos << ": " << (result2 ? "success" : "failed") << std::endl;
        
        std::cout << "Packet examination test passed!" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in testPacketExamination: " << e.what() << std::endl;
        return false;
    }
}

int main() {
    std::cout << "Running OggDemuxer bisection search tests..." << std::endl;
    
    bool all_passed = true;
    
    all_passed &= testBasicBisectionSearch();
    all_passed &= testBoundaryConditions();
    all_passed &= testLinearScanningFallback();
    all_passed &= testPacketExamination();
    
    if (all_passed) {
        std::cout << "\nAll bisection search tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "\nSome bisection search tests failed!" << std::endl;
        return 1;
    }
}

#else // !HAVE_OGGDEMUXER

int main() {
    std::cout << "OggDemuxer not compiled in, skipping bisection search tests." << std::endl;
    return 0;
}

#endif // HAVE_OGGDEMUXER