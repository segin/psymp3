/*
 * test_flac_demuxer_unit_comprehensive.cpp - Comprehensive unit tests for FLACDemuxer
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include <vector>
#include <memory>
#include <cstring>
#include <fstream>
#include <thread>
#include <chrono>

using namespace TestFramework;

// Test file path - using the provided test file
const char* TEST_FLAC_FILE = "/mnt/8TB-3/music/almost monday/DIVE/11 life goes by.flac";

/**
 * @brief Mock IOHandler for testing FLAC parsing with controlled data
 */
class MockFLACIOHandler : public IOHandler {
private:
    std::vector<uint8_t> m_data;
    size_t m_position = 0;
    bool m_simulate_errors = false;
    
public:
    explicit MockFLACIOHandler(const std::vector<uint8_t>& data) 
        : m_data(data), m_position(0) {}
    
    void setSimulateErrors(bool simulate) { m_simulate_errors = simulate; }
    
    size_t read(void* buffer, size_t size, size_t count) override {
        if (m_simulate_errors && m_position > m_data.size() / 2) {
            return 0; // Simulate I/O error
        }
        
        size_t total_bytes = size * count;
        size_t available = m_data.size() - m_position;
        size_t to_read = std::min(total_bytes, available);
        
        if (to_read > 0) {
            std::memcpy(buffer, m_data.data() + m_position, to_read);
            m_position += to_read;
        }
        
        return to_read / size; // Return number of elements read
    }
    
    int seek(off_t offset, int whence) override {
        if (m_simulate_errors) return -1;
        
        size_t new_pos;
        switch (whence) {
            case SEEK_SET:
                new_pos = static_cast<size_t>(offset);
                break;
            case SEEK_CUR:
                new_pos = m_position + static_cast<size_t>(offset);
                break;
            case SEEK_END:
                new_pos = m_data.size() + static_cast<size_t>(offset);
                break;
            default:
                return -1;
        }
        
        if (new_pos > m_data.size()) return -1;
        
        m_position = new_pos;
        return 0; // Success
    }
    
    off_t tell() override {
        return static_cast<off_t>(m_position);
    }
    
    off_t getFileSize() override {
        return static_cast<off_t>(m_data.size());
    }
    
    bool eof() override {
        return m_position >= m_data.size();
    }
    
    int close() override {
        return 0; // Success
    }
    
    int getLastError() const override {
        return 0; // No error
    }
};

/**
 * @brief Helper class to generate test FLAC data
 */
class FLACTestDataGenerator {
public:
    /**
     * @brief Generate minimal valid FLAC file data
     */
    static std::vector<uint8_t> generateMinimalFLAC() {
        std::vector<uint8_t> data;
        
        // fLaC stream marker (0x664C6143)
        data.push_back(0x66); // 'f'
        data.push_back(0x4C); // 'L'
        data.push_back(0x61); // 'a'
        data.push_back(0x43); // 'C'
        
        // STREAMINFO metadata block (mandatory, last block)
        data.push_back(0x80); // Last block flag (1) + STREAMINFO type (0)
        data.push_back(0x00); // Length high byte
        data.push_back(0x00); // Length middle byte
        data.push_back(0x22); // Length low byte = 34 bytes
        
        // STREAMINFO data (34 bytes)
        // Min block size (16 bits) = 4096
        data.push_back(0x10);
        data.push_back(0x00);
        
        // Max block size (16 bits) = 4096
        data.push_back(0x10);
        data.push_back(0x00);
        
        // Min frame size (24 bits) = 0 (unknown)
        data.push_back(0x00);
        data.push_back(0x00);
        data.push_back(0x00);
        
        // Max frame size (24 bits) = 0 (unknown)
        data.push_back(0x00);
        data.push_back(0x00);
        data.push_back(0x00);
        
        // Sample rate (20 bits) = 44100, channels (3 bits) = 2-1, bits per sample (5 bits) = 16-1, total samples high (4 bits) = 0
        uint32_t sr_ch_bps = (44100 << 12) | ((2-1) << 9) | ((16-1) << 4) | 0;
        data.push_back((sr_ch_bps >> 24) & 0xFF);
        data.push_back((sr_ch_bps >> 16) & 0xFF);
        data.push_back((sr_ch_bps >> 8) & 0xFF);
        data.push_back(sr_ch_bps & 0xFF);
        
        // Total samples (36 bits) = 1000000 (low 32 bits, high 4 bits are in the previous word)
        uint64_t total_samples = 1000000;
        data.push_back((total_samples >> 24) & 0xFF);
        data.push_back((total_samples >> 16) & 0xFF);
        data.push_back((total_samples >> 8) & 0xFF);
        data.push_back(total_samples & 0xFF);
        
        // MD5 signature (16 bytes) - all zeros for test
        for (int i = 0; i < 16; i++) {
            data.push_back(0x00);
        }
        
        // Add a minimal FLAC frame
        addMinimalFrame(data);
        
        return data;
    }
    
    /**
     * @brief Generate FLAC with SEEKTABLE
     */
    static std::vector<uint8_t> generateFLACWithSeekTable() {
        std::vector<uint8_t> data;
        
        // fLaC stream marker (0x664C6143)
        data.push_back(0x66); // 'f'
        data.push_back(0x4C); // 'L'
        data.push_back(0x61); // 'a'
        data.push_back(0x43); // 'C'
        
        // STREAMINFO metadata block (not last)
        data.push_back(0x00); // Not last block (0) + STREAMINFO type (0)
        data.push_back(0x00); // Length high byte
        data.push_back(0x00); // Length middle byte
        data.push_back(0x22); // Length low byte = 34 bytes
        
        // Add STREAMINFO data (same as minimal)
        addStreamInfoData(data);
        
        // SEEKTABLE metadata block (last block)
        data.push_back(0x83); // Last block flag + SEEKTABLE type (3)
        data.push_back(0x00); // Length high bytes
        data.push_back(0x00);
        data.push_back(0x36); // Length = 54 bytes (3 seek points * 18 bytes each)
        
        // Add 3 seek points
        addSeekPoint(data, 0, 0, 4096);           // First frame
        addSeekPoint(data, 500000, 8192, 4096);   // Middle
        addSeekPoint(data, 1000000, 16384, 4096); // End
        
        // Add minimal FLAC frame
        addMinimalFrame(data);
        
        return data;
    }
    
    /**
     * @brief Generate FLAC with VORBIS_COMMENT
     */
    static std::vector<uint8_t> generateFLACWithVorbisComment() {
        std::vector<uint8_t> data;
        
        // fLaC stream marker (0x664C6143)
        data.push_back(0x66); // 'f'
        data.push_back(0x4C); // 'L'
        data.push_back(0x61); // 'a'
        data.push_back(0x43); // 'C'
        
        // STREAMINFO metadata block (not last)
        data.push_back(0x00); // Not last block (0) + STREAMINFO type (0)
        data.push_back(0x00); // Length high byte
        data.push_back(0x00); // Length middle byte
        data.push_back(0x22); // Length low byte = 34 bytes
        
        addStreamInfoData(data);
        
        // VORBIS_COMMENT metadata block (last block)
        data.push_back(0x84); // Last block flag + VORBIS_COMMENT type (4)
        
        // Calculate comment block size
        std::string vendor = "Test Encoder";
        std::string title = "TITLE=Test Song";
        std::string artist = "ARTIST=Test Artist";
        
        uint32_t comment_size = 4 + vendor.length() + 4 + 2 * 4 + title.length() + artist.length();
        data.push_back((comment_size >> 16) & 0xFF);
        data.push_back((comment_size >> 8) & 0xFF);
        data.push_back(comment_size & 0xFF);
        
        // Vendor string
        uint32_t vendor_len = vendor.length();
        data.push_back(vendor_len & 0xFF);
        data.push_back((vendor_len >> 8) & 0xFF);
        data.push_back((vendor_len >> 16) & 0xFF);
        data.push_back((vendor_len >> 24) & 0xFF);
        data.insert(data.end(), vendor.begin(), vendor.end());
        
        // User comment list length
        data.push_back(0x02); // 2 comments
        data.push_back(0x00);
        data.push_back(0x00);
        data.push_back(0x00);
        
        // Comment 1: TITLE
        uint32_t title_len = title.length();
        data.push_back(title_len & 0xFF);
        data.push_back((title_len >> 8) & 0xFF);
        data.push_back((title_len >> 16) & 0xFF);
        data.push_back((title_len >> 24) & 0xFF);
        data.insert(data.end(), title.begin(), title.end());
        
        // Comment 2: ARTIST
        uint32_t artist_len = artist.length();
        data.push_back(artist_len & 0xFF);
        data.push_back((artist_len >> 8) & 0xFF);
        data.push_back((artist_len >> 16) & 0xFF);
        data.push_back((artist_len >> 24) & 0xFF);
        data.insert(data.end(), artist.begin(), artist.end());
        
        addMinimalFrame(data);
        
        return data;
    }
    
    /**
     * @brief Generate corrupted FLAC data for error testing
     */
    static std::vector<uint8_t> generateCorruptedFLAC() {
        std::vector<uint8_t> data;
        
        // Invalid stream marker
        data.push_back(0x66); // 'f'
        data.push_back(0x4C); // 'L'
        data.push_back(0x61); // 'a'
        data.push_back(0x58); // 'X' - Wrong marker
        
        // Rest of data is garbage
        for (int i = 0; i < 100; i++) {
            data.push_back(static_cast<uint8_t>(i));
        }
        
        return data;
    }

private:
    static void addStreamInfoData(std::vector<uint8_t>& data) {
        // Min/max block size
        data.insert(data.end(), {0x10, 0x00, 0x10, 0x00});
        
        // Min/max frame size (unknown)
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
        
        // Sample rate, channels, bits per sample
        // FLAC format according to RFC 9639:
        // Byte 10: sample_rate[19:12]
        // Byte 11: sample_rate[11:4] 
        // Byte 12: sample_rate[3:0] + channels[2:0] + bits_per_sample[4]
        // Byte 13: bits_per_sample[3:0] + total_samples[35:32]
        uint32_t sample_rate = 44100;      // 0xAC44
        uint32_t channels_minus_1 = 1;     // 2 channels - 1
        uint32_t bits_per_sample_minus_1 = 15; // 16 bits - 1
        
        data.push_back((sample_rate >> 12) & 0xFF);  // Byte 10: 0x0A
        data.push_back((sample_rate >> 4) & 0xFF);   // Byte 11: 0xC4
        data.push_back(((sample_rate & 0x0F) << 4) | (channels_minus_1 << 1) | ((bits_per_sample_minus_1 >> 4) & 0x01)); // Byte 12: 0x42
        // Total samples (36 bits): bottom 4 bits of byte 13 + bytes 14-17
        uint64_t total_samples = 1000000;
        data.push_back(((bits_per_sample_minus_1 & 0x0F) << 4) | ((total_samples >> 32) & 0x0F)); // Byte 13: bits_per_sample + total_samples[35:32]
        
        // Total samples continued (bytes 14-17)
        data.push_back((total_samples >> 24) & 0xFF);  // Byte 14
        data.push_back((total_samples >> 16) & 0xFF);  // Byte 15
        data.push_back((total_samples >> 8) & 0xFF);   // Byte 16
        data.push_back(total_samples & 0xFF);          // Byte 17
        
        // MD5 signature (16 bytes of zeros)
        for (int i = 0; i < 16; i++) {
            data.push_back(0x00);
        }
    }
    
    static void addSeekPoint(std::vector<uint8_t>& data, uint64_t sample, uint64_t offset, uint16_t samples) {
        // Sample number (64 bits)
        for (int i = 7; i >= 0; i--) {
            data.push_back((sample >> (i * 8)) & 0xFF);
        }
        
        // Stream offset (64 bits)
        for (int i = 7; i >= 0; i--) {
            data.push_back((offset >> (i * 8)) & 0xFF);
        }
        
        // Frame samples (16 bits)
        data.push_back((samples >> 8) & 0xFF);
        data.push_back(samples & 0xFF);
    }
    
    static void addMinimalFrame(std::vector<uint8_t>& data) {
        // Minimal FLAC frame header
        data.push_back(0xFF); // Sync code start
        data.push_back(0xF8); // Sync code end + reserved + blocking strategy
        data.push_back(0x69); // Block size + sample rate
        data.push_back(0x04); // Channel assignment + sample size + reserved
        data.push_back(0x00); // Frame number (UTF-8 coded, single byte)
        data.push_back(0x8A); // CRC-8 (dummy value)
        
        // Minimal frame data (just a few bytes to represent compressed audio)
        for (int i = 0; i < 50; i++) {
            data.push_back(0x00);
        }
        
        // Frame footer CRC-16 (dummy)
        data.push_back(0x00);
        data.push_back(0x00);
    }
};

/**
 * @brief Test FLAC stream marker validation
 */
class FLACStreamMarkerTest : public TestCase {
public:
    FLACStreamMarkerTest() : TestCase("FLAC Stream Marker Validation Test") {}
    
protected:
    void runTest() override {
        // Test valid fLaC marker
        auto valid_data = FLACTestDataGenerator::generateMinimalFLAC();
        auto valid_handler = std::make_unique<MockFLACIOHandler>(valid_data);
        auto valid_demuxer = std::make_unique<FLACDemuxer>(std::move(valid_handler));
        
        ASSERT_TRUE(valid_demuxer->parseContainer(), "Valid FLAC should parse successfully");
        
        // Test invalid marker
        auto invalid_data = FLACTestDataGenerator::generateCorruptedFLAC();
        auto invalid_handler = std::make_unique<MockFLACIOHandler>(invalid_data);
        auto invalid_demuxer = std::make_unique<FLACDemuxer>(std::move(invalid_handler));
        
        ASSERT_FALSE(invalid_demuxer->parseContainer(), "Invalid FLAC marker should be rejected");
    }
};

/**
 * @brief Test STREAMINFO metadata block parsing
 */
class FLACStreamInfoParsingTest : public TestCase {
public:
    FLACStreamInfoParsingTest() : TestCase("FLAC STREAMINFO Parsing Test") {}
    
protected:
    void runTest() override {
        auto data = FLACTestDataGenerator::generateMinimalFLAC();
        auto handler = std::make_unique<MockFLACIOHandler>(data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse container successfully");
        
        auto streams = demuxer->getStreams();
        ASSERT_EQUALS(1u, streams.size(), "Should have exactly one stream");
        
        const auto& stream = streams[0];
        ASSERT_EQUALS(44100u, stream.sample_rate, "Sample rate should be 44100");
        ASSERT_EQUALS(2u, stream.channels, "Should have 2 channels");
        ASSERT_EQUALS(16u, stream.bits_per_sample, "Should have 16 bits per sample");
        
        // Test duration calculation
        uint64_t expected_duration = (1000000 * 1000) / 44100; // samples * 1000 / sample_rate
        ASSERT_EQUALS(expected_duration, demuxer->getDuration(), "Duration should be calculated correctly");
    }
};

/**
 * @brief Test SEEKTABLE metadata block parsing
 */
class FLACSeekTableParsingTest : public TestCase {
public:
    FLACSeekTableParsingTest() : TestCase("FLAC SEEKTABLE Parsing Test") {}
    
protected:
    void runTest() override {
        auto data = FLACTestDataGenerator::generateFLACWithSeekTable();
        auto handler = std::make_unique<MockFLACIOHandler>(data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse container with seek table");
        
        // Test seeking using the seek table
        ASSERT_TRUE(demuxer->seekTo(11337), "Should seek to middle position using seek table"); // ~500000 samples
        
        uint64_t position = demuxer->getPosition();
        ASSERT_TRUE(position >= 11000 && position <= 12000, "Position should be approximately correct after seek");
    }
};

/**
 * @brief Test VORBIS_COMMENT metadata block parsing
 */
class FLACVorbisCommentParsingTest : public TestCase {
public:
    FLACVorbisCommentParsingTest() : TestCase("FLAC VORBIS_COMMENT Parsing Test") {}
    
protected:
    void runTest() override {
        auto data = FLACTestDataGenerator::generateFLACWithVorbisComment();
        auto handler = std::make_unique<MockFLACIOHandler>(data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse container with Vorbis comments");
        
        auto streams = demuxer->getStreams();
        ASSERT_EQUALS(1u, streams.size(), "Should have exactly one stream");
        
        const auto& stream = streams[0];
        ASSERT_EQUALS("Test Song", stream.title, "Title should be extracted from Vorbis comments");
        ASSERT_EQUALS("Test Artist", stream.artist, "Artist should be extracted from Vorbis comments");
    }
};

/**
 * @brief Test frame detection and header parsing
 */
class FLACFrameDetectionTest : public TestCase {
public:
    FLACFrameDetectionTest() : TestCase("FLAC Frame Detection Test") {}
    
protected:
    void runTest() override {
        auto data = FLACTestDataGenerator::generateMinimalFLAC();
        auto handler = std::make_unique<MockFLACIOHandler>(data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse container successfully");
        
        // Read first frame
        auto chunk = demuxer->readChunk();
        ASSERT_TRUE(chunk.data.size() > 0, "Should read frame data");
        ASSERT_EQUALS(1u, chunk.stream_id, "Stream ID should be 1 for FLAC");
        ASSERT_TRUE(chunk.is_keyframe, "All FLAC frames should be keyframes");
        
        // Verify frame starts with sync code
        ASSERT_TRUE(chunk.data.size() >= 2, "Frame should have at least sync code");
        ASSERT_EQUALS(0xFF, chunk.data[0], "Frame should start with sync code 0xFF");
        ASSERT_EQUALS(0xF8, chunk.data[1] & 0xFC, "Frame should have valid sync pattern");
    }
};

/**
 * @brief Test seeking algorithms
 */
class FLACSeekingAlgorithmsTest : public TestCase {
public:
    FLACSeekingAlgorithmsTest() : TestCase("FLAC Seeking Algorithms Test") {}
    
protected:
    void runTest() override {
        // Test with seek table
        testSeekTableSeeking();
        
        // Test without seek table (binary search)
        testBinarySearchSeeking();
    }
    
private:
    void testSeekTableSeeking() {
        auto data = FLACTestDataGenerator::generateFLACWithSeekTable();
        auto handler = std::make_unique<MockFLACIOHandler>(data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse container with seek table");
        
        // Seek to beginning
        ASSERT_TRUE(demuxer->seekTo(0), "Should seek to beginning");
        ASSERT_EQUALS(0u, demuxer->getPosition(), "Should be at sample 0");
        
        // Seek to middle (should use seek table)
        ASSERT_TRUE(demuxer->seekTo(11337), "Should seek to middle using seek table");
        uint64_t middle_sample = demuxer->getPosition();
        ASSERT_TRUE(middle_sample >= 490000 && middle_sample <= 510000, "Should be near middle sample");
        
        // Seek to end
        uint64_t duration = demuxer->getDuration();
        ASSERT_TRUE(demuxer->seekTo(duration - 100), "Should seek near end");
    }
    
    void testBinarySearchSeeking() {
        auto data = FLACTestDataGenerator::generateMinimalFLAC();
        auto handler = std::make_unique<MockFLACIOHandler>(data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse container without seek table");
        
        // Seek should still work using binary search
        ASSERT_TRUE(demuxer->seekTo(5000), "Should seek using binary search");
        
        // Position should be reasonable
        uint64_t position = demuxer->getPosition();
        ASSERT_TRUE(position >= 4000 && position <= 6000, "Binary search should provide reasonable accuracy");
    }
};

/**
 * @brief Test error handling and recovery
 */
class FLACErrorHandlingTest : public TestCase {
public:
    FLACErrorHandlingTest() : TestCase("FLAC Error Handling Test") {}
    
protected:
    void runTest() override {
        testInvalidStreamMarker();
        testCorruptedMetadata();
        testIOErrors();
        testFrameErrors();
    }
    
private:
    void testInvalidStreamMarker() {
        auto data = FLACTestDataGenerator::generateCorruptedFLAC();
        auto handler = std::make_unique<MockFLACIOHandler>(data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_FALSE(demuxer->parseContainer(), "Should reject invalid stream marker");
    }
    
    void testCorruptedMetadata() {
        auto data = FLACTestDataGenerator::generateMinimalFLAC();
        
        // Corrupt the STREAMINFO block length
        if (data.size() > 7) {
            data[7] = 0xFF; // Invalid length
        }
        
        auto handler = std::make_unique<MockFLACIOHandler>(data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        // Should handle corrupted metadata gracefully
        bool parsed = demuxer->parseContainer();
        // May succeed with recovery or fail gracefully - both are acceptable
        if (parsed) {
            // If it parsed, it should still provide basic functionality
            auto streams = demuxer->getStreams();
            ASSERT_TRUE(streams.size() <= 1, "Should not create invalid streams");
        }
    }
    
    void testIOErrors() {
        auto data = FLACTestDataGenerator::generateMinimalFLAC();
        auto handler = std::make_unique<MockFLACIOHandler>(data);
        handler->setSimulateErrors(true);
        
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        // Should handle I/O errors gracefully
        bool parsed = demuxer->parseContainer();
        ASSERT_FALSE(parsed, "Should fail gracefully on I/O errors");
    }
    
    void testFrameErrors() {
        auto data = FLACTestDataGenerator::generateMinimalFLAC();
        auto handler = std::make_unique<MockFLACIOHandler>(data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse container successfully");
        
        // Try to read beyond available data
        for (int i = 0; i < 10; i++) {
            auto chunk = demuxer->readChunk();
            if (chunk.data.empty()) {
                break; // EOF reached gracefully
            }
        }
        
        ASSERT_TRUE(demuxer->isEOF(), "Should detect EOF correctly");
    }
};

/**
 * @brief Test memory management and performance
 */
class FLACMemoryManagementTest : public TestCase {
public:
    FLACMemoryManagementTest() : TestCase("FLAC Memory Management Test") {}
    
protected:
    void runTest() override {
        testLargeFileHandling();
        testMemoryBounds();
        testBufferReuse();
    }
    
private:
    void testLargeFileHandling() {
        // Generate larger test data
        auto data = FLACTestDataGenerator::generateFLACWithSeekTable();
        
        // Extend with more frames
        for (int i = 0; i < 100; i++) {
            data.insert(data.end(), {0xFF, 0xF8, 0x69, 0x04, 0x00, 0x8A});
            for (int j = 0; j < 50; j++) {
                data.push_back(static_cast<uint8_t>(j));
            }
            data.insert(data.end(), {0x00, 0x00}); // CRC
        }
        
        auto handler = std::make_unique<MockFLACIOHandler>(data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should handle larger files");
        
        // Read multiple chunks to test memory management
        for (int i = 0; i < 10; i++) {
            auto chunk = demuxer->readChunk();
            if (chunk.data.empty()) break;
            
            // Verify chunk is reasonable size
            ASSERT_TRUE(chunk.data.size() < 1024 * 1024, "Chunks should not be excessively large");
        }
    }
    
    void testMemoryBounds() {
        auto data = FLACTestDataGenerator::generateMinimalFLAC();
        auto handler = std::make_unique<MockFLACIOHandler>(data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse successfully");
        
        // Test seeking to various positions doesn't cause memory issues
        uint64_t duration = demuxer->getDuration();
        for (uint64_t pos = 0; pos < duration; pos += duration / 10) {
            ASSERT_TRUE(demuxer->seekTo(pos), "Should seek without memory issues");
        }
    }
    
    void testBufferReuse() {
        auto data = FLACTestDataGenerator::generateMinimalFLAC();
        auto handler = std::make_unique<MockFLACIOHandler>(data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse successfully");
        
        // Read multiple chunks and verify they're independent
        auto chunk1 = demuxer->readChunk();
        auto chunk2 = demuxer->readChunk();
        
        if (!chunk1.data.empty() && !chunk2.data.empty()) {
            // Modify first chunk data
            if (!chunk1.data.empty()) {
                chunk1.data[0] = 0xAA;
            }
            
            // Second chunk should be unaffected
            ASSERT_NOT_EQUALS(0xAA, chunk2.data.empty() ? 0 : chunk2.data[0], 
                             "Chunks should be independent");
        }
    }
};

/**
 * @brief Test thread safety
 */
class FLACThreadSafetyTest : public TestCase {
public:
    FLACThreadSafetyTest() : TestCase("FLAC Thread Safety Test") {}
    
protected:
    void runTest() override {
        testConcurrentReading();
        testConcurrentSeeking();
        testConcurrentMetadataAccess();
    }
    
private:
    void testConcurrentReading() {
        auto data = FLACTestDataGenerator::generateFLACWithSeekTable();
        auto handler = std::make_unique<MockFLACIOHandler>(data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse container successfully");
        
        std::atomic<bool> error_occurred{false};
        std::atomic<int> chunks_read{0};
        
        // Start multiple reader threads
        std::vector<std::thread> threads;
        for (int i = 0; i < 3; i++) {
            threads.emplace_back([&demuxer, &error_occurred, &chunks_read]() {
                try {
                    for (int j = 0; j < 5; j++) {
                        auto chunk = demuxer->readChunk();
                        if (!chunk.data.empty()) {
                            chunks_read++;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                } catch (...) {
                    error_occurred = true;
                }
            });
        }
        
        // Wait for all threads
        for (auto& thread : threads) {
            thread.join();
        }
        
        ASSERT_FALSE(error_occurred.load(), "No errors should occur during concurrent reading");
        ASSERT_TRUE(chunks_read.load() > 0, "Should read some chunks successfully");
    }
    
    void testConcurrentSeeking() {
        auto data = FLACTestDataGenerator::generateFLACWithSeekTable();
        auto handler = std::make_unique<MockFLACIOHandler>(data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse container successfully");
        
        std::atomic<bool> error_occurred{false};
        std::atomic<int> seeks_completed{0};
        
        uint64_t duration = demuxer->getDuration();
        
        // Start multiple seeking threads
        std::vector<std::thread> threads;
        for (int i = 0; i < 3; i++) {
            threads.emplace_back([&demuxer, &error_occurred, &seeks_completed, duration, i]() {
                try {
                    for (int j = 0; j < 5; j++) {
                        uint64_t seek_pos = (duration * (i * 5 + j)) / 15;
                        if (demuxer->seekTo(seek_pos)) {
                            seeks_completed++;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                } catch (...) {
                    error_occurred = true;
                }
            });
        }
        
        // Wait for all threads
        for (auto& thread : threads) {
            thread.join();
        }
        
        ASSERT_FALSE(error_occurred.load(), "No errors should occur during concurrent seeking");
        ASSERT_TRUE(seeks_completed.load() > 0, "Should complete some seeks successfully");
    }
    
    void testConcurrentMetadataAccess() {
        auto data = FLACTestDataGenerator::generateFLACWithVorbisComment();
        auto handler = std::make_unique<MockFLACIOHandler>(data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse container successfully");
        
        std::atomic<bool> error_occurred{false};
        std::atomic<int> metadata_accesses{0};
        
        // Start multiple metadata access threads
        std::vector<std::thread> threads;
        for (int i = 0; i < 5; i++) {
            threads.emplace_back([&demuxer, &error_occurred, &metadata_accesses]() {
                try {
                    for (int j = 0; j < 10; j++) {
                        auto streams = demuxer->getStreams();
                        if (!streams.empty()) {
                            metadata_accesses++;
                        }
                        
                        uint64_t duration = demuxer->getDuration();
                        uint64_t position = demuxer->getPosition();
                        
                        if (duration > 0 && position <= duration) {
                            metadata_accesses++;
                        }
                        
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                } catch (...) {
                    error_occurred = true;
                }
            });
        }
        
        // Wait for all threads
        for (auto& thread : threads) {
            thread.join();
        }
        
        ASSERT_FALSE(error_occurred.load(), "No errors should occur during concurrent metadata access");
        ASSERT_TRUE(metadata_accesses.load() > 0, "Should access metadata successfully");
    }
};

int main() {
    TestSuite suite("FLAC Demuxer Unit Tests");
    
    // Add all unit test cases
    suite.addTest(std::make_unique<FLACStreamMarkerTest>());
    suite.addTest(std::make_unique<FLACStreamInfoParsingTest>());
    suite.addTest(std::make_unique<FLACSeekTableParsingTest>());
    suite.addTest(std::make_unique<FLACVorbisCommentParsingTest>());
    suite.addTest(std::make_unique<FLACFrameDetectionTest>());
    suite.addTest(std::make_unique<FLACSeekingAlgorithmsTest>());
    suite.addTest(std::make_unique<FLACErrorHandlingTest>());
    suite.addTest(std::make_unique<FLACMemoryManagementTest>());
    suite.addTest(std::make_unique<FLACThreadSafetyTest>());
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print results
    suite.printResults(results);
    
    // Return appropriate exit code
    return suite.getFailureCount(results) == 0 ? 0 : 1;
}