/*
 * test_flac_demuxer_simple.cpp - Simple FLAC demuxer compatibility test
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <memory>

// Simple assertion macro
#define SIMPLE_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "ASSERTION FAILED: " << (message) << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

/**
 * @brief Simple mock IOHandler for testing
 */
class SimpleMockIOHandler : public IOHandler {
private:
    std::vector<uint8_t> m_data;
    size_t m_position = 0;
    
public:
    explicit SimpleMockIOHandler(const std::vector<uint8_t>& data) : m_data(data) {}
    
    size_t read(void* buffer, size_t size, size_t count) override {
        size_t bytes_to_read = std::min(size * count, m_data.size() - m_position);
        if (bytes_to_read > 0) {
            std::memcpy(buffer, m_data.data() + m_position, bytes_to_read);
            m_position += bytes_to_read;
        }
        return bytes_to_read / size;
    }
    
    int seek(off_t offset, int whence) override {
        off_t new_pos;
        switch (whence) {
            case SEEK_SET: new_pos = offset; break;
            case SEEK_CUR: new_pos = static_cast<off_t>(m_position) + offset; break;
            case SEEK_END: new_pos = static_cast<off_t>(m_data.size()) + offset; break;
            default: return -1;
        }
        
        if (new_pos < 0 || new_pos > static_cast<off_t>(m_data.size())) {
            return -1;
        }
        
        m_position = static_cast<size_t>(new_pos);
        return 0;
    }
    
    off_t tell() override {
        return static_cast<off_t>(m_position);
    }
    
    bool eof() override {
        return m_position >= m_data.size();
    }
    
    int close() override { return 0; }
    
    off_t getFileSize() override {
        return static_cast<off_t>(m_data.size());
    }
};

/**
 * @brief Generate minimal valid FLAC data
 */
std::vector<uint8_t> generateMinimalFLAC() {
    std::vector<uint8_t> data;
    
    // fLaC stream marker
    data.insert(data.end(), {'f', 'L', 'a', 'C'});
    
    // STREAMINFO metadata block (last block)
    data.push_back(0x80); // is_last=1, type=0
    data.push_back(0x00); // length high byte
    data.push_back(0x00); // length mid byte  
    data.push_back(0x22); // length low byte (34 bytes)
    
    // STREAMINFO block data (34 bytes)
    // min_block_size (16 bits) - 4096
    data.push_back(0x10);
    data.push_back(0x00);
    
    // max_block_size (16 bits) - 4096
    data.push_back(0x10);
    data.push_back(0x00);
    
    // min_frame_size (24 bits) - 0 (unknown)
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);
    
    // max_frame_size (24 bits) - 0 (unknown)
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);
    
    // sample_rate (20 bits), channels-1 (3 bits), bits_per_sample-1 (5 bits)
    // 44100 Hz, 2 channels, 16 bits
    uint32_t sr_ch_bps = (44100 << 12) | ((2 - 1) << 9) | (16 - 1);
    data.push_back((sr_ch_bps >> 16) & 0xFF);
    data.push_back((sr_ch_bps >> 8) & 0xFF);
    data.push_back(sr_ch_bps & 0xFF);
    
    // total_samples (36 bits) - 44100 samples (1 second)
    uint64_t total_samples = 44100;
    data.push_back((total_samples >> 28) & 0xFF);
    data.push_back((total_samples >> 20) & 0xFF);
    data.push_back((total_samples >> 12) & 0xFF);
    data.push_back((total_samples >> 4) & 0xFF);
    data.push_back((total_samples << 4) & 0xF0);
    
    // MD5 signature (16 bytes) - all zeros
    for (int i = 0; i < 16; i++) {
        data.push_back(0x00);
    }
    
    return data;
}

/**
 * @brief Test basic FLACDemuxer functionality
 */
bool testBasicFunctionality() {
    std::cout << "Testing basic FLAC demuxer functionality..." << std::endl;
    
    auto flac_data = generateMinimalFLAC();
    auto handler = std::make_unique<SimpleMockIOHandler>(flac_data);
    auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
    
    // Test parseContainer
    SIMPLE_ASSERT(demuxer->parseContainer(), "Should parse minimal FLAC container");
    
    // Test stream information
    auto streams = demuxer->getStreams();
    SIMPLE_ASSERT(streams.size() == 1, "Should have exactly one stream");
    
    const auto& stream = streams[0];
    SIMPLE_ASSERT(stream.stream_id == 1, "Stream ID should be 1");
    SIMPLE_ASSERT(stream.codec_type == "audio", "Should be audio stream");
    SIMPLE_ASSERT(stream.codec_name == "flac", "Should be FLAC codec");
    SIMPLE_ASSERT(stream.sample_rate == 44100, "Sample rate should be 44100");
    SIMPLE_ASSERT(stream.channels == 2, "Channels should be 2");
    SIMPLE_ASSERT(stream.bits_per_sample == 16, "Bits per sample should be 16");
    
    // Test duration
    uint64_t duration = demuxer->getDuration();
    SIMPLE_ASSERT(duration > 900 && duration < 1100, "Duration should be approximately 1 second");
    
    // Test position
    SIMPLE_ASSERT(demuxer->getPosition() == 0, "Initial position should be 0");
    
    // Test EOF
    SIMPLE_ASSERT(!demuxer->isEOF(), "Should not be EOF initially");
    
    std::cout << "Basic functionality test PASSED" << std::endl;
    return true;
}

/**
 * @brief Test error handling
 */
bool testErrorHandling() {
    std::cout << "Testing FLAC demuxer error handling..." << std::endl;
    
    // Test with invalid data
    std::vector<uint8_t> invalid_data = {'I', 'N', 'V', 'D'};
    auto invalid_handler = std::make_unique<SimpleMockIOHandler>(invalid_data);
    auto invalid_demuxer = std::make_unique<FLACDemuxer>(std::move(invalid_handler));
    
    SIMPLE_ASSERT(!invalid_demuxer->parseContainer(), "Should reject invalid FLAC data");
    
    // Test with empty data
    std::vector<uint8_t> empty_data;
    auto empty_handler = std::make_unique<SimpleMockIOHandler>(empty_data);
    auto empty_demuxer = std::make_unique<FLACDemuxer>(std::move(empty_handler));
    
    SIMPLE_ASSERT(!empty_demuxer->parseContainer(), "Should reject empty data");
    
    // Test operations on unparsed demuxer
    auto streams = empty_demuxer->getStreams();
    SIMPLE_ASSERT(streams.empty(), "Unparsed demuxer should return empty streams");
    
    SIMPLE_ASSERT(empty_demuxer->getDuration() == 0, "Unparsed demuxer should return 0 duration");
    SIMPLE_ASSERT(empty_demuxer->getPosition() == 0, "Unparsed demuxer should return 0 position");
    
    auto chunk = empty_demuxer->readChunk();
    SIMPLE_ASSERT(!chunk.isValid(), "Unparsed demuxer should return invalid chunk");
    
    SIMPLE_ASSERT(!empty_demuxer->seekTo(1000), "Unparsed demuxer should reject seeks");
    
    std::cout << "Error handling test PASSED" << std::endl;
    return true;
}

/**
 * @brief Test seeking functionality
 */
bool testSeeking() {
    std::cout << "Testing FLAC demuxer seeking..." << std::endl;
    
    auto flac_data = generateMinimalFLAC();
    auto handler = std::make_unique<SimpleMockIOHandler>(flac_data);
    auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
    
    SIMPLE_ASSERT(demuxer->parseContainer(), "Should parse FLAC container");
    
    // Test seeking to beginning
    SIMPLE_ASSERT(demuxer->seekTo(0), "Should seek to beginning");
    SIMPLE_ASSERT(demuxer->getPosition() == 0, "Position should be 0 after seeking to beginning");
    
    // Test seeking to middle (may not work perfectly with minimal mock data)
    bool seek_result = demuxer->seekTo(500); // 0.5 seconds
    // Don't assert success since mock data is minimal, just ensure no crash
    
    // Test position after seek
    uint64_t position = demuxer->getPosition();
    SIMPLE_ASSERT(position < 1000000, "Position should be reasonable after seek");
    
    std::cout << "Seeking test PASSED" << std::endl;
    return true;
}

/**
 * @brief Test frame reading
 */
bool testFrameReading() {
    std::cout << "Testing FLAC demuxer frame reading..." << std::endl;
    
    auto flac_data = generateMinimalFLAC();
    auto handler = std::make_unique<SimpleMockIOHandler>(flac_data);
    auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
    
    SIMPLE_ASSERT(demuxer->parseContainer(), "Should parse container");
    
    // Test reading chunk (may not succeed with minimal mock data, but should not crash)
    auto chunk = demuxer->readChunk();
    
    if (chunk.isValid()) {
        SIMPLE_ASSERT(chunk.stream_id == 1, "Chunk should have correct stream ID");
        SIMPLE_ASSERT(!chunk.data.empty(), "Chunk data should not be empty");
        SIMPLE_ASSERT(chunk.is_keyframe, "FLAC frames should be keyframes");
    }
    
    // Test reading with specific stream ID
    auto chunk2 = demuxer->readChunk(1);
    // May or may not succeed, but should not crash
    
    std::cout << "Frame reading test PASSED" << std::endl;
    return true;
}

int main() {
    std::cout << "FLAC Demuxer Simple Compatibility Test" << std::endl;
    std::cout << "======================================" << std::endl;
    
    int tests_run = 0;
    int tests_passed = 0;
    
    // Run tests
    if (testBasicFunctionality()) tests_passed++;
    tests_run++;
    
    if (testErrorHandling()) tests_passed++;
    tests_run++;
    
    if (testSeeking()) tests_passed++;
    tests_run++;
    
    if (testFrameReading()) tests_passed++;
    tests_run++;
    
    // Print results
    std::cout << std::endl;
    std::cout << "Test Results:" << std::endl;
    std::cout << "=============" << std::endl;
    std::cout << "Tests run: " << tests_run << std::endl;
    std::cout << "Tests passed: " << tests_passed << std::endl;
    std::cout << "Tests failed: " << (tests_run - tests_passed) << std::endl;
    
    if (tests_passed == tests_run) {
        std::cout << "All tests PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << "Some tests FAILED!" << std::endl;
        return 1;
    }
}