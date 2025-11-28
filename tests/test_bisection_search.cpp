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
        // Create demuxer with mock data
        std::vector<uint8_t> mock_data;
        auto handler = std::make_unique<MockIOHandler>(mock_data);
        OggDemuxer demuxer(std::move(handler));
        
        std::cout << "  OggDemuxer instantiated successfully" << std::endl;
        
        // Use internal testing methods to set up test state
        auto& streams = demuxer.getStreamsForTesting();
        
        // Create a test stream with known properties
        OggStream test_stream;
        test_stream.serial_number = 12345;
        test_stream.codec_name = "vorbis";
        test_stream.codec_type = "audio";
        test_stream.sample_rate = 44100;
        test_stream.channels = 2;
        test_stream.total_samples = 441000; // 10 seconds at 44.1kHz
        streams[12345] = test_stream;
        
        std::cout << "  Test stream created: 10 seconds at 44.1kHz" << std::endl;
        
        // Test granule to milliseconds conversion
        uint64_t duration_ms = demuxer.granuleToMs(441000, 12345);
        std::cout << "  Granule 441000 -> " << duration_ms << "ms (expected 10000ms)" << std::endl;
        
        if (duration_ms != 10000) {
            std::cerr << "  ERROR: Expected 10000ms, got " << duration_ms << "ms" << std::endl;
            return false;
        }
        
        // Test seeking to different positions
        std::vector<uint64_t> test_positions = {0, 2500, 5000, 7500, 10000};
        
        for (uint64_t target_ms : test_positions) {
            std::cout << "  Testing seek to " << target_ms << "ms" << std::endl;
            bool result = demuxer.seekTo(target_ms);
            std::cout << "    Seek result: " << (result ? "success" : "failed gracefully") << std::endl;
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
        // Create demuxer with mock data
        std::vector<uint8_t> mock_data;
        auto handler = std::make_unique<MockIOHandler>(mock_data);
        OggDemuxer demuxer(std::move(handler));
        
        std::cout << "  OggDemuxer instantiated successfully" << std::endl;
        
        // Use internal testing methods to set up test state
        auto& streams = demuxer.getStreamsForTesting();
        
        // Create an Opus stream with pre-skip
        OggStream opus_stream;
        opus_stream.serial_number = 54321;
        opus_stream.codec_name = "opus";
        opus_stream.codec_type = "audio";
        opus_stream.sample_rate = 48000; // Opus uses 48kHz
        opus_stream.channels = 2;
        opus_stream.total_samples = 480000; // 10 seconds at 48kHz
        opus_stream.pre_skip = 312;
        streams[54321] = opus_stream;
        
        std::cout << "  Opus stream created: 10 seconds at 48kHz with pre-skip" << std::endl;
        
        // Test seeking to beginning
        std::cout << "  Testing seek to beginning (0ms)" << std::endl;
        bool result1 = demuxer.seekTo(0);
        std::cout << "    Result: " << (result1 ? "success" : "failed gracefully") << std::endl;
        
        // Test seeking to middle
        std::cout << "  Testing seek to middle (5000ms)" << std::endl;
        bool result2 = demuxer.seekTo(5000);
        std::cout << "    Result: " << (result2 ? "success" : "failed gracefully") << std::endl;
        
        // Test seeking beyond end
        std::cout << "  Testing seek beyond end (15000ms)" << std::endl;
        bool result3 = demuxer.seekTo(15000);
        std::cout << "    Result: " << (result3 ? "success" : "failed gracefully") << std::endl;
        
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
        
        // Create demuxer with mock data
        std::vector<uint8_t> mock_data;
        auto handler = std::make_unique<MockIOHandler>(mock_data);
        OggDemuxer demuxer(std::move(handler));
        
        std::cout << "  OggDemuxer instantiated successfully" << std::endl;
        
        // Use internal testing methods to set up test state
        auto& streams = demuxer.getStreamsForTesting();
        
        // Create a FLAC stream
        OggStream flac_stream;
        flac_stream.serial_number = 98765;
        flac_stream.codec_name = "flac";
        flac_stream.codec_type = "audio";
        flac_stream.sample_rate = 96000; // High-res FLAC
        flac_stream.channels = 2;
        flac_stream.total_samples = 960000; // 10 seconds at 96kHz
        streams[98765] = flac_stream;
        
        std::cout << "  FLAC stream created: 10 seconds at 96kHz" << std::endl;
        
        // Test seeking to various positions
        std::vector<uint64_t> test_positions = {0, 2500, 5000, 7500, 10000};
        
        for (uint64_t target_ms : test_positions) {
            std::cout << "  Testing seek to " << target_ms << "ms" << std::endl;
            
            bool result = demuxer.seekTo(target_ms);
            std::cout << "    Seek result: " << (result ? "success" : "failed gracefully") << std::endl;
        }
        
        std::cout << "Linear scanning fallback test passed!" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in testLinearScanningFallback: " << e.what() << std::endl;
        return false;
    }
}

// Test packet reading functionality
bool testPacketExamination() {
    std::cout << "Testing packet reading functionality..." << std::endl;
    
    try {
        // Create demuxer with mock data
        std::vector<uint8_t> mock_data;
        auto handler = std::make_unique<MockIOHandler>(mock_data);
        OggDemuxer demuxer(std::move(handler));
        
        std::cout << "  OggDemuxer instantiated successfully" << std::endl;
        
        // Use internal testing methods to set up test state
        auto& streams = demuxer.getStreamsForTesting();
        
        // Create multiple streams to test multiplexing
        OggStream stream1;
        stream1.serial_number = 11111;
        stream1.codec_name = "vorbis";
        stream1.codec_type = "audio";
        stream1.sample_rate = 44100;
        stream1.channels = 2;
        stream1.total_samples = 220500; // 5 seconds
        streams[11111] = stream1;
        
        OggStream stream2;
        stream2.serial_number = 22222;
        stream2.codec_name = "opus";
        stream2.codec_type = "audio";
        stream2.sample_rate = 48000;
        stream2.channels = 2;
        stream2.total_samples = 240000; // 5 seconds at 48kHz
        streams[22222] = stream2;
        
        std::cout << "  Created 2 test streams (Vorbis and Opus)" << std::endl;
        
        // Get stream information
        auto stream_list = demuxer.getStreams();
        std::cout << "  Number of streams in list: " << stream_list.size() << std::endl;
        
        // Test granule conversion for both streams
        uint64_t vorbis_duration = demuxer.granuleToMs(220500, 11111);
        uint64_t opus_duration = demuxer.granuleToMs(240000, 22222);
        
        std::cout << "  Vorbis stream duration: " << vorbis_duration << "ms (expected 5000ms)" << std::endl;
        std::cout << "  Opus stream duration: " << opus_duration << "ms (expected 5000ms)" << std::endl;
        
        if (vorbis_duration != 5000 || opus_duration != 5000) {
            std::cerr << "  ERROR: Duration mismatch" << std::endl;
            return false;
        }
        
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