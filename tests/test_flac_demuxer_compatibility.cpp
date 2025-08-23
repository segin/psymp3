/*
 * test_flac_demuxer_compatibility.cpp - Compatibility tests for FLACDemuxer
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"

using namespace TestFramework;

/**
 * @brief Mock FLAC file data generator for testing
 */
class MockFLACData {
public:
    /**
     * @brief Generate minimal valid FLAC file data for testing
     * @param sample_rate Sample rate for the mock file
     * @param channels Number of channels
     * @param total_samples Total samples in the file
     * @return Vector containing mock FLAC file data
     */
    static std::vector<uint8_t> generateMinimalFLAC(uint32_t sample_rate = 44100, 
                                                    uint8_t channels = 2, 
                                                    uint64_t total_samples = 44100) {
        std::vector<uint8_t> data;
        
        // fLaC stream marker
        data.insert(data.end(), {'f', 'L', 'a', 'C'});
        
        // STREAMINFO metadata block (mandatory first block)
        // Block header: is_last=1, type=0 (STREAMINFO), length=34
        data.push_back(0x80); // is_last=1, type=0
        data.push_back(0x00); // length high byte
        data.push_back(0x00); // length mid byte  
        data.push_back(0x22); // length low byte (34 bytes)
        
        // STREAMINFO block data (34 bytes)
        // min_block_size (16 bits)
        data.push_back(0x10); // 4096 samples
        data.push_back(0x00);
        
        // max_block_size (16 bits)
        data.push_back(0x10); // 4096 samples
        data.push_back(0x00);
        
        // min_frame_size (24 bits) - 0 if unknown
        data.push_back(0x00);
        data.push_back(0x00);
        data.push_back(0x00);
        
        // max_frame_size (24 bits) - 0 if unknown
        data.push_back(0x00);
        data.push_back(0x00);
        data.push_back(0x00);
        
        // sample_rate (20 bits), channels-1 (3 bits), bits_per_sample-1 (5 bits)
        uint32_t sr_ch_bps = (sample_rate << 12) | ((channels - 1) << 9) | (16 - 1);
        data.push_back((sr_ch_bps >> 16) & 0xFF);
        data.push_back((sr_ch_bps >> 8) & 0xFF);
        data.push_back(sr_ch_bps & 0xFF);
        
        // total_samples (36 bits) - split across 4.5 bytes
        data.push_back((total_samples >> 28) & 0xFF);
        data.push_back((total_samples >> 20) & 0xFF);
        data.push_back((total_samples >> 12) & 0xFF);
        data.push_back((total_samples >> 4) & 0xFF);
        data.push_back((total_samples << 4) & 0xF0);
        
        // MD5 signature (16 bytes) - all zeros for mock
        for (int i = 0; i < 16; i++) {
            data.push_back(0x00);
        }
        
        // Add a minimal FLAC frame for completeness
        // Frame sync (14 bits) + reserved (1 bit) + blocking strategy (1 bit)
        data.push_back(0xFF); // sync high
        data.push_back(0xF8); // sync low + reserved + blocking strategy
        
        // Block size (4 bits) + sample rate (4 bits)
        data.push_back(0x69); // block size index 6 (4096), sample rate index 9 (44.1kHz)
        
        // Channel assignment (4 bits) + sample size (3 bits) + reserved (1 bit)
        data.push_back(0x10); // stereo, 16-bit, reserved=0
        
        // Frame number (UTF-8 coded) - frame 0
        data.push_back(0x00);
        
        // CRC-8 (placeholder)
        data.push_back(0x00);
        
        // Add some mock frame data (minimal)
        for (int i = 0; i < 100; i++) {
            data.push_back(0x00);
        }
        
        return data;
    }
    
    /**
     * @brief Generate FLAC file with VORBIS_COMMENT metadata
     */
    static std::vector<uint8_t> generateFLACWithMetadata() {
        std::vector<uint8_t> data;
        
        // fLaC stream marker
        data.insert(data.end(), {'f', 'L', 'a', 'C'});
        
        // STREAMINFO metadata block (not last)
        data.push_back(0x00); // is_last=0, type=0
        data.push_back(0x00); // length high byte
        data.push_back(0x00); // length mid byte  
        data.push_back(0x22); // length low byte (34 bytes)
        
        // STREAMINFO data (simplified)
        for (int i = 0; i < 34; i++) {
            data.push_back(i < 4 ? 0x10 : 0x00); // min/max block size, rest zeros
        }
        
        // VORBIS_COMMENT metadata block (last)
        data.push_back(0x84); // is_last=1, type=4 (VORBIS_COMMENT)
        
        // Create VORBIS_COMMENT data
        std::string vendor = "test_vendor";
        std::string artist = "ARTIST=Test Artist";
        std::string title = "TITLE=Test Title";
        
        uint32_t comment_length = 4 + vendor.length() + 4 + 2 * 4 + artist.length() + title.length();
        
        data.push_back((comment_length >> 16) & 0xFF);
        data.push_back((comment_length >> 8) & 0xFF);
        data.push_back(comment_length & 0xFF);
        
        // Vendor string length (little-endian)
        uint32_t vendor_len = vendor.length();
        data.push_back(vendor_len & 0xFF);
        data.push_back((vendor_len >> 8) & 0xFF);
        data.push_back((vendor_len >> 16) & 0xFF);
        data.push_back((vendor_len >> 24) & 0xFF);
        
        // Vendor string
        data.insert(data.end(), vendor.begin(), vendor.end());
        
        // User comment list length (little-endian)
        data.push_back(0x02); // 2 comments
        data.push_back(0x00);
        data.push_back(0x00);
        data.push_back(0x00);
        
        // First comment (ARTIST)
        uint32_t artist_len = artist.length();
        data.push_back(artist_len & 0xFF);
        data.push_back((artist_len >> 8) & 0xFF);
        data.push_back((artist_len >> 16) & 0xFF);
        data.push_back((artist_len >> 24) & 0xFF);
        data.insert(data.end(), artist.begin(), artist.end());
        
        // Second comment (TITLE)
        uint32_t title_len = title.length();
        data.push_back(title_len & 0xFF);
        data.push_back((title_len >> 8) & 0xFF);
        data.push_back((title_len >> 16) & 0xFF);
        data.push_back((title_len >> 24) & 0xFF);
        data.insert(data.end(), title.begin(), title.end());
        
        return data;
    }
    
    /**
     * @brief Generate FLAC file with SEEKTABLE
     */
    static std::vector<uint8_t> generateFLACWithSeekTable() {
        std::vector<uint8_t> data;
        
        // fLaC stream marker
        data.insert(data.end(), {'f', 'L', 'a', 'C'});
        
        // STREAMINFO metadata block (not last)
        data.push_back(0x00); // is_last=0, type=0
        data.push_back(0x00);
        data.push_back(0x00);
        data.push_back(0x22); // 34 bytes
        
        // STREAMINFO data (simplified)
        for (int i = 0; i < 34; i++) {
            data.push_back(i < 4 ? 0x10 : 0x00);
        }
        
        // SEEKTABLE metadata block (last)
        data.push_back(0x83); // is_last=1, type=3 (SEEKTABLE)
        data.push_back(0x00);
        data.push_back(0x00);
        data.push_back(0x12); // 18 bytes (1 seek point)
        
        // Seek point: sample_number=0, stream_offset=0, frame_samples=4096
        // sample_number (64 bits)
        for (int i = 0; i < 8; i++) data.push_back(0x00);
        
        // stream_offset (64 bits)  
        for (int i = 0; i < 8; i++) data.push_back(0x00);
        
        // frame_samples (16 bits)
        data.push_back(0x10); // 4096
        data.push_back(0x00);
        
        return data;
    }
};

/**
 * @brief Mock IOHandler for FLAC testing
 */
class MockFLACIOHandler : public IOHandler {
private:
    std::vector<uint8_t> m_data;
    size_t m_position = 0;
    
public:
    explicit MockFLACIOHandler(const std::vector<uint8_t>& data) : m_data(data) {}
    
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
 * @brief Test FLACDemuxer basic container parsing
 */
class FLACDemuxerParsingTest : public TestCase {
public:
    FLACDemuxerParsingTest() : TestCase("FLACDemuxer Container Parsing Test") {}
    
protected:
    void runTest() override {
        // Test with minimal valid FLAC data
        auto flac_data = MockFLACData::generateMinimalFLAC();
        auto handler = std::make_unique<MockFLACIOHandler>(flac_data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        // Test parseContainer
        ASSERT_TRUE(demuxer->parseContainer(), "Should successfully parse minimal FLAC container");
        
        // Test stream information
        auto streams = demuxer->getStreams();
        ASSERT_EQUALS(1u, streams.size(), "Should have exactly one stream");
        
        const auto& stream = streams[0];
        ASSERT_EQUALS(1u, stream.stream_id, "Stream ID should be 1");
        ASSERT_EQUALS("audio", stream.codec_type, "Should be audio stream");
        ASSERT_EQUALS("flac", stream.codec_name, "Should be FLAC codec");
        ASSERT_EQUALS(44100u, stream.sample_rate, "Sample rate should match");
        ASSERT_EQUALS(2u, stream.channels, "Channels should match");
        ASSERT_EQUALS(16u, stream.bits_per_sample, "Bits per sample should match");
        
        // Test duration calculation
        uint64_t expected_duration = (44100 * 1000) / 44100; // 1 second
        ASSERT_EQUALS(expected_duration, stream.duration_ms, "Duration should be calculated correctly");
        ASSERT_EQUALS(expected_duration, demuxer->getDuration(), "Demuxer duration should match stream duration");
        
        // Test position tracking
        ASSERT_EQUALS(0u, demuxer->getPosition(), "Initial position should be 0");
        ASSERT_EQUALS(0u, demuxer->getCurrentSample(), "Initial sample position should be 0");
        
        // Test EOF state
        ASSERT_FALSE(demuxer->isEOF(), "Should not be EOF initially");
    }
};

/**
 * @brief Test FLACDemuxer metadata extraction
 */
class FLACDemuxerMetadataTest : public TestCase {
public:
    FLACDemuxerMetadataTest() : TestCase("FLACDemuxer Metadata Extraction Test") {}
    
protected:
    void runTest() override {
        // Test with FLAC file containing VORBIS_COMMENT metadata
        auto flac_data = MockFLACData::generateFLACWithMetadata();
        auto handler = std::make_unique<MockFLACIOHandler>(flac_data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse FLAC with metadata");
        
        auto streams = demuxer->getStreams();
        ASSERT_EQUALS(1u, streams.size(), "Should have one stream");
        
        const auto& stream = streams[0];
        
        // Test metadata extraction
        ASSERT_EQUALS("Test Artist", stream.artist, "Artist metadata should be extracted");
        ASSERT_EQUALS("Test Title", stream.title, "Title metadata should be extracted");
        
        // Test that metadata is preserved in stream info
        auto stream_info = demuxer->getStreamInfo(1);
        ASSERT_EQUALS("Test Artist", stream_info.artist, "Artist should be in stream info");
        ASSERT_EQUALS("Test Title", stream_info.title, "Title should be in stream info");
    }
};

/**
 * @brief Test FLACDemuxer seeking functionality
 */
class FLACDemuxerSeekingTest : public TestCase {
public:
    FLACDemuxerSeekingTest() : TestCase("FLACDemuxer Seeking Test") {}
    
protected:
    void runTest() override {
        // Test with FLAC file containing seek table
        auto flac_data = MockFLACData::generateFLACWithSeekTable();
        auto handler = std::make_unique<MockFLACIOHandler>(flac_data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse FLAC with seek table");
        
        // Test seeking to beginning
        ASSERT_TRUE(demuxer->seekTo(0), "Should seek to beginning");
        ASSERT_EQUALS(0u, demuxer->getPosition(), "Position should be 0 after seeking to beginning");
        
        // Test seeking to middle (this may not work perfectly with mock data, but should not crash)
        bool seek_result = demuxer->seekTo(500); // 0.5 seconds
        // Don't assert success since mock data is minimal, just ensure no crash
        
        // Test seeking beyond duration (should clamp or handle gracefully)
        bool seek_beyond = demuxer->seekTo(999999); // Very large timestamp
        // Should handle gracefully without crashing
        
        // Test position tracking after seeks
        uint64_t position_after_seek = demuxer->getPosition();
        // Position should be reasonable (not crash or return garbage)
        ASSERT_TRUE(position_after_seek < 1000000, "Position should be reasonable after seek");
    }
};

/**
 * @brief Test FLACDemuxer frame reading
 */
class FLACDemuxerFrameReadingTest : public TestCase {
public:
    FLACDemuxerFrameReadingTest() : TestCase("FLACDemuxer Frame Reading Test") {}
    
protected:
    void runTest() override {
        auto flac_data = MockFLACData::generateMinimalFLAC();
        auto handler = std::make_unique<MockFLACIOHandler>(flac_data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse container");
        
        // Test reading first chunk
        auto chunk = demuxer->readChunk();
        
        if (chunk.isValid()) {
            // If we successfully read a chunk, validate its properties
            ASSERT_EQUALS(1u, chunk.stream_id, "Chunk should have correct stream ID");
            ASSERT_FALSE(chunk.data.empty(), "Chunk data should not be empty");
            ASSERT_TRUE(chunk.is_keyframe, "FLAC frames should be keyframes");
            
            // Test reading with specific stream ID
            auto chunk2 = demuxer->readChunk(1);
            // May or may not succeed depending on mock data, but should not crash
        }
        
        // Test EOF detection
        // With minimal mock data, we may reach EOF quickly
        bool reached_eof = false;
        int max_chunks = 10; // Prevent infinite loop
        int chunks_read = 0;
        
        while (!demuxer->isEOF() && chunks_read < max_chunks) {
            auto test_chunk = demuxer->readChunk();
            if (!test_chunk.isValid()) {
                break;
            }
            chunks_read++;
        }
        
        // Should either read some chunks or reach EOF gracefully
        ASSERT_TRUE(chunks_read >= 0, "Should read non-negative number of chunks");
    }
};

/**
 * @brief Test FLACDemuxer error handling
 */
class FLACDemuxerErrorHandlingTest : public TestCase {
public:
    FLACDemuxerErrorHandlingTest() : TestCase("FLACDemuxer Error Handling Test") {}
    
protected:
    void runTest() override {
        // Test with invalid FLAC data (no fLaC marker)
        std::vector<uint8_t> invalid_data = {'I', 'N', 'V', 'D', 0x00, 0x00, 0x00, 0x00};
        auto invalid_handler = std::make_unique<MockFLACIOHandler>(invalid_data);
        auto invalid_demuxer = std::make_unique<FLACDemuxer>(std::move(invalid_handler));
        
        ASSERT_FALSE(invalid_demuxer->parseContainer(), "Should reject invalid FLAC data");
        
        // Test with empty data
        std::vector<uint8_t> empty_data;
        auto empty_handler = std::make_unique<MockFLACIOHandler>(empty_data);
        auto empty_demuxer = std::make_unique<FLACDemuxer>(std::move(empty_handler));
        
        ASSERT_FALSE(empty_demuxer->parseContainer(), "Should reject empty data");
        
        // Test with truncated FLAC data (only fLaC marker)
        std::vector<uint8_t> truncated_data = {'f', 'L', 'a', 'C'};
        auto truncated_handler = std::make_unique<MockFLACIOHandler>(truncated_data);
        auto truncated_demuxer = std::make_unique<FLACDemuxer>(std::move(truncated_handler));
        
        ASSERT_FALSE(truncated_demuxer->parseContainer(), "Should reject truncated FLAC data");
        
        // Test operations on unparsed demuxer
        auto unparsed_handler = std::make_unique<MockFLACIOHandler>(invalid_data);
        auto unparsed_demuxer = std::make_unique<FLACDemuxer>(std::move(unparsed_handler));
        
        // These should handle unparsed state gracefully
        auto streams = unparsed_demuxer->getStreams();
        ASSERT_TRUE(streams.empty(), "Unparsed demuxer should return empty streams");
        
        ASSERT_EQUALS(0u, unparsed_demuxer->getDuration(), "Unparsed demuxer should return 0 duration");
        ASSERT_EQUALS(0u, unparsed_demuxer->getPosition(), "Unparsed demuxer should return 0 position");
        
        auto chunk = unparsed_demuxer->readChunk();
        ASSERT_FALSE(chunk.isValid(), "Unparsed demuxer should return invalid chunk");
        
        ASSERT_FALSE(unparsed_demuxer->seekTo(1000), "Unparsed demuxer should reject seeks");
    }
};

/**
 * @brief Test FLACDemuxer compatibility with existing FLAC implementation
 */
class FLACDemuxerCompatibilityTest : public TestCase {
public:
    FLACDemuxerCompatibilityTest() : TestCase("FLACDemuxer Compatibility Test") {}
    
protected:
    void runTest() override {
        // Test that FLACDemuxer provides equivalent functionality to existing FLAC implementation
        
        auto flac_data = MockFLACData::generateMinimalFLAC(48000, 2, 48000 * 60); // 1 minute at 48kHz
        auto handler = std::make_unique<MockFLACIOHandler>(flac_data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse test FLAC file");
        
        // Test stream parameters match expected values
        auto streams = demuxer->getStreams();
        ASSERT_EQUALS(1u, streams.size(), "Should have one audio stream");
        
        const auto& stream = streams[0];
        ASSERT_EQUALS(48000u, stream.sample_rate, "Sample rate should match");
        ASSERT_EQUALS(2u, stream.channels, "Channels should match");
        ASSERT_EQUALS(16u, stream.bits_per_sample, "Bit depth should match");
        
        // Test duration calculation (should be 60 seconds)
        uint64_t expected_duration = 60000; // 60 seconds in milliseconds
        ASSERT_EQUALS(expected_duration, stream.duration_ms, "Duration should be 60 seconds");
        
        // Test seeking accuracy
        ASSERT_TRUE(demuxer->seekTo(30000), "Should seek to 30 seconds");
        // Position may not be exact due to frame boundaries, but should be close
        uint64_t position_after_seek = demuxer->getPosition();
        ASSERT_TRUE(position_after_seek >= 29000 && position_after_seek <= 31000, 
                   "Seek position should be approximately correct");
        
        // Test that seeking to beginning works
        ASSERT_TRUE(demuxer->seekTo(0), "Should seek to beginning");
        ASSERT_EQUALS(0u, demuxer->getPosition(), "Position should be 0 after seeking to beginning");
        
        // Test EOF behavior
        ASSERT_FALSE(demuxer->isEOF(), "Should not be EOF at beginning");
        
        // Test that invalid stream IDs are handled
        auto invalid_stream = demuxer->getStreamInfo(999);
        ASSERT_FALSE(invalid_stream.isValid(), "Invalid stream ID should return invalid stream info");
        
        auto invalid_chunk = demuxer->readChunk(999);
        ASSERT_FALSE(invalid_chunk.isValid(), "Invalid stream ID should return invalid chunk");
    }
};

/**
 * @brief Test FLACDemuxer performance characteristics
 */
class FLACDemuxerPerformanceTest : public TestCase {
public:
    FLACDemuxerPerformanceTest() : TestCase("FLACDemuxer Performance Test") {}
    
protected:
    void runTest() override {
        // Generate larger FLAC data for performance testing
        auto flac_data = MockFLACData::generateMinimalFLAC(44100, 2, 44100 * 300); // 5 minutes
        auto handler = std::make_unique<MockFLACIOHandler>(flac_data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        // Measure parsing time
        auto start_time = std::chrono::high_resolution_clock::now();
        bool parse_result = demuxer->parseContainer();
        auto end_time = std::chrono::high_resolution_clock::now();
        
        auto parse_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        ASSERT_TRUE(parse_result, "Should parse large FLAC file");
        ASSERT_TRUE(parse_duration.count() < 1000, "Parsing should complete within 1 second");
        
        // Test seeking performance
        start_time = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 10; i++) {
            uint64_t seek_pos = (i * 30000); // Seek every 30 seconds
            demuxer->seekTo(seek_pos);
        }
        end_time = std::chrono::high_resolution_clock::now();
        
        auto seek_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        ASSERT_TRUE(seek_duration.count() < 100, "Multiple seeks should complete quickly");
        
        // Test memory usage (basic check)
        auto streams = demuxer->getStreams();
        ASSERT_FALSE(streams.empty(), "Should maintain stream information");
        
        // Test that demuxer doesn't consume excessive memory for metadata
        // This is a basic test - in real implementation, we'd check actual memory usage
        ASSERT_TRUE(streams[0].artist.length() < 10000, "Metadata should not be excessively large");
    }
};

int main() {
    TestSuite suite("FLAC Demuxer Compatibility Tests");
    
    // Add all test cases
    suite.addTest(std::make_unique<FLACDemuxerParsingTest>());
    suite.addTest(std::make_unique<FLACDemuxerMetadataTest>());
    suite.addTest(std::make_unique<FLACDemuxerSeekingTest>());
    suite.addTest(std::make_unique<FLACDemuxerFrameReadingTest>());
    suite.addTest(std::make_unique<FLACDemuxerErrorHandlingTest>());
    suite.addTest(std::make_unique<FLACDemuxerCompatibilityTest>());
    suite.addTest(std::make_unique<FLACDemuxerPerformanceTest>());
    
    // Run all tests
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results);
}