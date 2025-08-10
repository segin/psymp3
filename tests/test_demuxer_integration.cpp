/*
 * test_demuxer_integration.cpp - Integration tests for demuxer implementations
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
 * @brief Create test data for different container formats
 */
class TestDataGenerator {
public:
    /**
     * @brief Generate minimal valid RIFF/WAV file data
     */
    static std::vector<uint8_t> generateRIFFWAVData() {
        std::vector<uint8_t> data;
        
        // RIFF header
        data.insert(data.end(), {'R', 'I', 'F', 'F'});
        
        // File size (little-endian) - will be updated at end
        size_t file_size_pos = data.size();
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00});
        
        // WAVE format
        data.insert(data.end(), {'W', 'A', 'V', 'E'});
        
        // fmt chunk
        data.insert(data.end(), {'f', 'm', 't', ' '});
        data.insert(data.end(), {0x10, 0x00, 0x00, 0x00}); // fmt chunk size (16)
        data.insert(data.end(), {0x01, 0x00}); // PCM format
        data.insert(data.end(), {0x02, 0x00}); // 2 channels
        data.insert(data.end(), {0x44, 0xAC, 0x00, 0x00}); // 44100 Hz sample rate
        data.insert(data.end(), {0x10, 0xB1, 0x02, 0x00}); // byte rate
        data.insert(data.end(), {0x04, 0x00}); // block align
        data.insert(data.end(), {0x10, 0x00}); // 16 bits per sample
        
        // data chunk
        data.insert(data.end(), {'d', 'a', 't', 'a'});
        data.insert(data.end(), {0x00, 0x04, 0x00, 0x00}); // data size (1024 bytes)
        
        // Sample PCM data (1024 bytes of sine wave pattern)
        for (int i = 0; i < 512; ++i) {
            int16_t sample = static_cast<int16_t>(sin(i * 0.1) * 16384);
            data.push_back(sample & 0xFF);
            data.push_back((sample >> 8) & 0xFF);
            data.push_back(sample & 0xFF); // Duplicate for stereo
            data.push_back((sample >> 8) & 0xFF);
        }
        
        // Update file size
        uint32_t total_size = static_cast<uint32_t>(data.size() - 8);
        data[file_size_pos] = total_size & 0xFF;
        data[file_size_pos + 1] = (total_size >> 8) & 0xFF;
        data[file_size_pos + 2] = (total_size >> 16) & 0xFF;
        data[file_size_pos + 3] = (total_size >> 24) & 0xFF;
        
        return data;
    }
    
    /**
     * @brief Generate minimal valid Ogg file data
     */
    static std::vector<uint8_t> generateOggData() {
        std::vector<uint8_t> data;
        
        // Ogg page header
        data.insert(data.end(), {'O', 'g', 'g', 'S'}); // capture pattern
        data.push_back(0x00); // version
        data.push_back(0x02); // header type (first page)
        
        // Granule position (8 bytes, little-endian)
        for (int i = 0; i < 8; ++i) data.push_back(0x00);
        
        // Serial number (4 bytes)
        data.insert(data.end(), {0x01, 0x00, 0x00, 0x00});
        
        // Page sequence number (4 bytes)
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00});
        
        // CRC checksum (4 bytes) - simplified, not actual CRC
        data.insert(data.end(), {0x12, 0x34, 0x56, 0x78});
        
        // Number of page segments
        data.push_back(0x01);
        
        // Segment table
        data.push_back(0x1E); // 30 bytes in segment
        
        // Vorbis identification header (simplified)
        data.push_back(0x01); // packet type
        data.insert(data.end(), {'v', 'o', 'r', 'b', 'i', 's'});
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00}); // version
        data.push_back(0x02); // channels
        data.insert(data.end(), {0x44, 0xAC, 0x00, 0x00}); // sample rate
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00}); // bitrate max
        data.insert(data.end(), {0x80, 0xBB, 0x00, 0x00}); // bitrate nominal
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00}); // bitrate min
        data.push_back(0xB0); // blocksize
        data.push_back(0x01); // framing flag
        
        return data;
    }
    
    /**
     * @brief Generate minimal valid AIFF file data
     */
    static std::vector<uint8_t> generateAIFFData() {
        std::vector<uint8_t> data;
        
        // FORM header
        data.insert(data.end(), {'F', 'O', 'R', 'M'});
        
        // File size (big-endian) - will be updated at end
        size_t file_size_pos = data.size();
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00});
        
        // AIFF format
        data.insert(data.end(), {'A', 'I', 'F', 'F'});
        
        // COMM chunk
        data.insert(data.end(), {'C', 'O', 'M', 'M'});
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x12}); // COMM chunk size (18)
        data.insert(data.end(), {0x00, 0x02}); // 2 channels
        data.insert(data.end(), {0x00, 0x00, 0x02, 0x00}); // 512 sample frames
        data.insert(data.end(), {0x00, 0x10}); // 16 bits per sample
        
        // Sample rate as 80-bit IEEE extended precision (44100 Hz)
        data.insert(data.end(), {0x40, 0x0E, 0xAC, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
        
        // SSND chunk
        data.insert(data.end(), {'S', 'S', 'N', 'D'});
        data.insert(data.end(), {0x00, 0x00, 0x04, 0x08}); // SSND chunk size (1032 bytes)
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00}); // offset
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00}); // block size
        
        // Sample PCM data (1024 bytes, big-endian)
        for (int i = 0; i < 512; ++i) {
            int16_t sample = static_cast<int16_t>(sin(i * 0.1) * 16384);
            data.push_back((sample >> 8) & 0xFF); // Big-endian
            data.push_back(sample & 0xFF);
            data.push_back((sample >> 8) & 0xFF); // Duplicate for stereo
            data.push_back(sample & 0xFF);
        }
        
        // Update file size (big-endian)
        uint32_t total_size = static_cast<uint32_t>(data.size() - 8);
        data[file_size_pos] = (total_size >> 24) & 0xFF;
        data[file_size_pos + 1] = (total_size >> 16) & 0xFF;
        data[file_size_pos + 2] = (total_size >> 8) & 0xFF;
        data[file_size_pos + 3] = total_size & 0xFF;
        
        return data;
    }
    
    /**
     * @brief Generate minimal valid MP4 file data
     */
    static std::vector<uint8_t> generateMP4Data() {
        std::vector<uint8_t> data;
        
        // ftyp box
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x20}); // box size (32 bytes)
        data.insert(data.end(), {'f', 't', 'y', 'p'}); // box type
        data.insert(data.end(), {'i', 's', 'o', 'm'}); // major brand
        data.insert(data.end(), {0x00, 0x00, 0x02, 0x00}); // minor version
        data.insert(data.end(), {'i', 's', 'o', 'm'}); // compatible brand 1
        data.insert(data.end(), {'i', 's', 'o', '2'}); // compatible brand 2
        data.insert(data.end(), {'a', 'v', 'c', '1'}); // compatible brand 3
        data.insert(data.end(), {'m', 'p', '4', '1'}); // compatible brand 4
        
        // mdat box (minimal)
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x10}); // box size (16 bytes)
        data.insert(data.end(), {'m', 'd', 'a', 't'}); // box type
        data.insert(data.end(), {0x00, 0x01, 0x02, 0x03}); // dummy media data
        data.insert(data.end(), {0x04, 0x05, 0x06, 0x07});
        
        return data;
    }
    
    /**
     * @brief Generate minimal valid FLAC file data
     */
    static std::vector<uint8_t> generateFLACData() {
        std::vector<uint8_t> data;
        
        // FLAC signature
        data.insert(data.end(), {'f', 'L', 'a', 'C'});
        
        // STREAMINFO metadata block
        data.push_back(0x80); // Last metadata block flag + block type (0)
        data.insert(data.end(), {0x00, 0x00, 0x22}); // block length (34 bytes)
        
        // STREAMINFO data
        data.insert(data.end(), {0x10, 0x00}); // min block size (4096)
        data.insert(data.end(), {0x10, 0x00}); // max block size (4096)
        data.insert(data.end(), {0x00, 0x00, 0x00}); // min frame size (0 = unknown)
        data.insert(data.end(), {0x00, 0x00, 0x00}); // max frame size (0 = unknown)
        
        // Sample rate (20 bits) + channels (3 bits) + bits per sample (5 bits)
        // 44100 Hz, 2 channels, 16 bits per sample
        data.insert(data.end(), {0x0A, 0xC4, 0x42}); // 44100 << 4 | (2-1) << 1 | (16-1) >> 4
        data.push_back(0xF0); // (16-1) << 4 | 0
        
        // Total samples (36 bits) - 44100 samples (1 second)
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00, 0xAC});
        
        // MD5 signature (16 bytes) - all zeros for simplicity
        for (int i = 0; i < 16; ++i) data.push_back(0x00);
        
        // Minimal FLAC frame (simplified)
        data.insert(data.end(), {0xFF, 0xF8, 0x69, 0x0C}); // Frame header
        data.insert(data.end(), {0x00, 0x01, 0x02, 0x03}); // Dummy frame data
        
        return data;
    }
};

/**
 * @brief Mock IOHandler that provides test data
 */
class TestDataIOHandler : public IOHandler {
private:
    std::vector<uint8_t> m_data;
    size_t m_position = 0;
    
public:
    explicit TestDataIOHandler(const std::vector<uint8_t>& data) : m_data(data) {}
    
    size_t read(void* buffer, size_t size, size_t count) override {
        size_t bytes_to_read = std::min(size * count, m_data.size() - m_position);
        if (bytes_to_read > 0) {
            std::memcpy(buffer, m_data.data() + m_position, bytes_to_read);
            m_position += bytes_to_read;
        }
        return bytes_to_read / size;
    }
    
    int seek(long offset, int whence) override {
        long new_pos;
        switch (whence) {
            case SEEK_SET: new_pos = offset; break;
            case SEEK_CUR: new_pos = static_cast<long>(m_position) + offset; break;
            case SEEK_END: new_pos = static_cast<long>(m_data.size()) + offset; break;
            default: return -1;
        }
        
        if (new_pos < 0 || new_pos > static_cast<long>(m_data.size())) {
            return -1;
        }
        
        m_position = static_cast<size_t>(new_pos);
        return 0;
    }
    
    long tell() override {
        return static_cast<long>(m_position);
    }
    
    bool eof() override {
        return m_position >= m_data.size();
    }
    
    void close() override {}
};

/**
 * @brief Test RIFF/WAV demuxer integration
 */
class RIFFDemuxerIntegrationTest : public TestCase {
public:
    RIFFDemuxerIntegrationTest() : TestCase("RIFF Demuxer Integration Test") {}
    
protected:
    void runTest() override {
        // Generate test WAV data
        auto wav_data = TestDataGenerator::generateRIFFWAVData();
        auto handler = std::make_unique<TestDataIOHandler>(wav_data);
        
        // Create demuxer through factory
        auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
        ASSERT_NOT_NULL(demuxer.get(), "RIFF demuxer should be created");
        
        // Test parsing
        ASSERT_TRUE(demuxer->parseContainer(), "RIFF container should parse successfully");
        ASSERT_TRUE(demuxer->isParsed(), "Demuxer should be in parsed state");
        
        // Test stream information
        auto streams = demuxer->getStreams();
        ASSERT_FALSE(streams.empty(), "Should have at least one stream");
        
        const auto& stream = streams[0];
        ASSERT_TRUE(stream.isAudio(), "First stream should be audio");
        ASSERT_EQUALS("pcm", stream.codec_name, "Codec should be PCM");
        ASSERT_EQUALS(44100u, stream.sample_rate, "Sample rate should be 44100");
        ASSERT_EQUALS(2u, stream.channels, "Should have 2 channels");
        ASSERT_EQUALS(16u, stream.bits_per_sample, "Should be 16-bit");
        
        // Test duration calculation
        ASSERT_TRUE(demuxer->getDuration() > 0, "Duration should be calculated");
        
        // Test chunk reading
        auto chunk = demuxer->readChunk();
        ASSERT_TRUE(chunk.isValid(), "Should read valid chunk");
        ASSERT_FALSE(chunk.data.empty(), "Chunk should have data");
        ASSERT_EQUALS(stream.stream_id, chunk.stream_id, "Chunk should belong to audio stream");
        
        // Test seeking
        uint64_t half_duration = demuxer->getDuration() / 2;
        ASSERT_TRUE(demuxer->seekTo(half_duration), "Should be able to seek to middle");
        ASSERT_EQUALS(half_duration, demuxer->getPosition(), "Position should be updated");
    }
};

/**
 * @brief Test Ogg demuxer integration
 */
class OggDemuxerIntegrationTest : public TestCase {
public:
    OggDemuxerIntegrationTest() : TestCase("Ogg Demuxer Integration Test") {}
    
protected:
    void runTest() override {
        // Generate test Ogg data
        auto ogg_data = TestDataGenerator::generateOggData();
        auto handler = std::make_unique<TestDataIOHandler>(ogg_data);
        
        // Create demuxer through factory
        auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
        ASSERT_NOT_NULL(demuxer.get(), "Ogg demuxer should be created");
        
        // Test parsing
        ASSERT_TRUE(demuxer->parseContainer(), "Ogg container should parse successfully");
        ASSERT_TRUE(demuxer->isParsed(), "Demuxer should be in parsed state");
        
        // Test stream information
        auto streams = demuxer->getStreams();
        ASSERT_FALSE(streams.empty(), "Should have at least one stream");
        
        const auto& stream = streams[0];
        ASSERT_TRUE(stream.isAudio(), "First stream should be audio");
        ASSERT_EQUALS("vorbis", stream.codec_name, "Codec should be Vorbis");
        ASSERT_EQUALS(44100u, stream.sample_rate, "Sample rate should be 44100");
        ASSERT_EQUALS(2u, stream.channels, "Should have 2 channels");
        
        // Test granule position support
        ASSERT_TRUE(demuxer->getGranulePosition(stream.stream_id) >= 0, "Should support granule positions");
        
        // Test chunk reading
        auto chunk = demuxer->readChunk();
        ASSERT_TRUE(chunk.isValid(), "Should read valid chunk");
        ASSERT_FALSE(chunk.data.empty(), "Chunk should have data");
        ASSERT_EQUALS(stream.stream_id, chunk.stream_id, "Chunk should belong to audio stream");
        
        // Ogg chunks should have granule position information
        ASSERT_TRUE(chunk.granule_position >= 0, "Ogg chunk should have granule position");
    }
};

/**
 * @brief Test AIFF demuxer integration
 */
class AIFFDemuxerIntegrationTest : public TestCase {
public:
    AIFFDemuxerIntegrationTest() : TestCase("AIFF Demuxer Integration Test") {}
    
protected:
    void runTest() override {
        // Generate test AIFF data
        auto aiff_data = TestDataGenerator::generateAIFFData();
        auto handler = std::make_unique<TestDataIOHandler>(aiff_data);
        
        // Create demuxer through factory
        auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
        ASSERT_NOT_NULL(demuxer.get(), "AIFF demuxer should be created");
        
        // Test parsing
        ASSERT_TRUE(demuxer->parseContainer(), "AIFF container should parse successfully");
        ASSERT_TRUE(demuxer->isParsed(), "Demuxer should be in parsed state");
        
        // Test stream information
        auto streams = demuxer->getStreams();
        ASSERT_FALSE(streams.empty(), "Should have at least one stream");
        
        const auto& stream = streams[0];
        ASSERT_TRUE(stream.isAudio(), "First stream should be audio");
        ASSERT_EQUALS("pcm", stream.codec_name, "Codec should be PCM");
        ASSERT_EQUALS(44100u, stream.sample_rate, "Sample rate should be 44100");
        ASSERT_EQUALS(2u, stream.channels, "Should have 2 channels");
        ASSERT_EQUALS(16u, stream.bits_per_sample, "Should be 16-bit");
        
        // Test chunk reading
        auto chunk = demuxer->readChunk();
        ASSERT_TRUE(chunk.isValid(), "Should read valid chunk");
        ASSERT_FALSE(chunk.data.empty(), "Chunk should have data");
        ASSERT_EQUALS(stream.stream_id, chunk.stream_id, "Chunk should belong to audio stream");
        
        // Test seeking
        ASSERT_TRUE(demuxer->seekTo(500), "Should be able to seek");
        ASSERT_EQUALS(500u, demuxer->getPosition(), "Position should be updated");
    }
};

/**
 * @brief Test MP4 demuxer integration
 */
class MP4DemuxerIntegrationTest : public TestCase {
public:
    MP4DemuxerIntegrationTest() : TestCase("MP4 Demuxer Integration Test") {}
    
protected:
    void runTest() override {
        // Generate test MP4 data
        auto mp4_data = TestDataGenerator::generateMP4Data();
        auto handler = std::make_unique<TestDataIOHandler>(mp4_data);
        
        // Create demuxer through factory
        auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
        ASSERT_NOT_NULL(demuxer.get(), "MP4 demuxer should be created");
        
        // Test parsing
        ASSERT_TRUE(demuxer->parseContainer(), "MP4 container should parse successfully");
        ASSERT_TRUE(demuxer->isParsed(), "Demuxer should be in parsed state");
        
        // MP4 files may have multiple streams (audio, video, etc.)
        auto streams = demuxer->getStreams();
        // Note: Our minimal MP4 data might not have complete stream info
        // This test verifies the demuxer can at least parse the container structure
        
        // Test that we can attempt to read chunks without crashing
        auto chunk = demuxer->readChunk();
        // Chunk may be empty for minimal test data, but should not crash
        
        // Test seeking capability
        ASSERT_TRUE(demuxer->seekTo(0), "Should be able to seek to beginning");
        ASSERT_EQUALS(0u, demuxer->getPosition(), "Position should be at beginning");
    }
};

/**
 * @brief Test FLAC demuxer integration
 */
class FLACDemuxerIntegrationTest : public TestCase {
public:
    FLACDemuxerIntegrationTest() : TestCase("FLAC Demuxer Integration Test") {}
    
protected:
    void runTest() override {
        // Generate test FLAC data
        auto flac_data = TestDataGenerator::generateFLACData();
        auto handler = std::make_unique<TestDataIOHandler>(flac_data);
        
        // Create demuxer through factory
        auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
        ASSERT_NOT_NULL(demuxer.get(), "FLAC demuxer should be created");
        
        // Test parsing
        ASSERT_TRUE(demuxer->parseContainer(), "FLAC container should parse successfully");
        ASSERT_TRUE(demuxer->isParsed(), "Demuxer should be in parsed state");
        
        // Test stream information
        auto streams = demuxer->getStreams();
        ASSERT_FALSE(streams.empty(), "Should have at least one stream");
        
        const auto& stream = streams[0];
        ASSERT_TRUE(stream.isAudio(), "First stream should be audio");
        ASSERT_EQUALS("flac", stream.codec_name, "Codec should be FLAC");
        ASSERT_EQUALS(44100u, stream.sample_rate, "Sample rate should be 44100");
        ASSERT_EQUALS(2u, stream.channels, "Should have 2 channels");
        ASSERT_EQUALS(16u, stream.bits_per_sample, "Should be 16-bit");
        
        // Test chunk reading
        auto chunk = demuxer->readChunk();
        ASSERT_TRUE(chunk.isValid(), "Should read valid chunk");
        ASSERT_FALSE(chunk.data.empty(), "Chunk should have data");
        ASSERT_EQUALS(stream.stream_id, chunk.stream_id, "Chunk should belong to audio stream");
    }
};

/**
 * @brief Test multi-threaded demuxer access
 */
class MultiThreadedDemuxerTest : public TestCase {
public:
    MultiThreadedDemuxerTest() : TestCase("Multi-threaded Demuxer Test") {}
    
protected:
    void runTest() override {
        // Generate test data
        auto wav_data = TestDataGenerator::generateRIFFWAVData();
        auto handler = std::make_unique<TestDataIOHandler>(wav_data);
        
        // Create demuxer
        auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
        ASSERT_NOT_NULL(demuxer.get(), "Demuxer should be created");
        ASSERT_TRUE(demuxer->parseContainer(), "Container should parse");
        
        // Test thread-safe operations
        std::atomic<bool> test_passed{true};
        std::atomic<int> chunks_read{0};
        
        // Simulate concurrent access (simplified test)
        auto test_concurrent_reads = [&]() {
            try {
                for (int i = 0; i < 5; ++i) {
                    // Test thread-safe state access
                    bool parsed = demuxer->isParsed();
                    uint64_t duration = demuxer->getDuration();
                    uint64_t position = demuxer->getPosition();
                    bool eof = demuxer->isEOF();
                    
                    if (!parsed || duration == 0) {
                        test_passed = false;
                        return;
                    }
                    
                    // Test thread-safe chunk reading
                    auto chunk = demuxer->readChunk();
                    if (chunk.isValid()) {
                        chunks_read++;
                    }
                    
                    // Small delay to allow thread interleaving
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            } catch (...) {
                test_passed = false;
            }
        };
        
        // Run concurrent operations
        std::thread t1(test_concurrent_reads);
        std::thread t2(test_concurrent_reads);
        
        t1.join();
        t2.join();
        
        ASSERT_TRUE(test_passed.load(), "Concurrent operations should not fail");
        ASSERT_TRUE(chunks_read.load() > 0, "Should read some chunks concurrently");
    }
};

/**
 * @brief Test error recovery scenarios
 */
class DemuxerErrorRecoveryTest : public TestCase {
public:
    DemuxerErrorRecoveryTest() : TestCase("Demuxer Error Recovery Test") {}
    
protected:
    void runTest() override {
        // Test with corrupted data
        std::vector<uint8_t> corrupted_data = {
            0x52, 0x49, 0x46, 0x46, // "RIFF"
            0xFF, 0xFF, 0xFF, 0xFF, // Invalid size
            0x57, 0x41, 0x56, 0x45, // "WAVE"
            // Truncated/corrupted data follows
            0x00, 0x01, 0x02, 0x03
        };
        
        auto handler = std::make_unique<TestDataIOHandler>(corrupted_data);
        auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
        
        if (demuxer) {
            // Parsing may fail with corrupted data
            bool parse_result = demuxer->parseContainer();
            
            if (!parse_result) {
                // Check that error information is available
                ASSERT_TRUE(demuxer->hasError(), "Should have error information");
                
                auto error = demuxer->getLastError();
                ASSERT_FALSE(error.category.empty(), "Error should have category");
                ASSERT_FALSE(error.message.empty(), "Error should have message");
                
                // Test error clearing
                demuxer->clearError();
                ASSERT_FALSE(demuxer->hasError(), "Error should be cleared");
            }
        }
        
        // Test with empty data
        std::vector<uint8_t> empty_data;
        auto empty_handler = std::make_unique<TestDataIOHandler>(empty_data);
        auto empty_demuxer = DemuxerFactory::createDemuxer(std::move(empty_handler));
        
        // Should either return nullptr or handle gracefully
        if (empty_demuxer) {
            ASSERT_FALSE(empty_demuxer->parseContainer(), "Empty data should not parse");
        }
    }
};

/**
 * @brief Test IOHandler integration with different sources
 */
class IOHandlerIntegrationTest : public TestCase {
public:
    IOHandlerIntegrationTest() : TestCase("IOHandler Integration Test") {}
    
protected:
    void runTest() override {
        // Test with different IOHandler implementations
        auto wav_data = TestDataGenerator::generateRIFFWAVData();
        
        // Test with TestDataIOHandler
        auto test_handler = std::make_unique<TestDataIOHandler>(wav_data);
        auto demuxer1 = DemuxerFactory::createDemuxer(std::move(test_handler));
        ASSERT_NOT_NULL(demuxer1.get(), "Should work with TestDataIOHandler");
        ASSERT_TRUE(demuxer1->parseContainer(), "Should parse with TestDataIOHandler");
        
        // Test seeking behavior
        ASSERT_EQUALS(0, demuxer1->getPosition(), "Initial position should be 0");
        
        auto chunk1 = demuxer1->readChunk();
        ASSERT_TRUE(chunk1.isValid(), "Should read first chunk");
        
        // Test seeking back to beginning
        ASSERT_TRUE(demuxer1->seekTo(0), "Should be able to seek to beginning");
        ASSERT_EQUALS(0u, demuxer1->getPosition(), "Position should be reset");
        
        auto chunk2 = demuxer1->readChunk();
        ASSERT_TRUE(chunk2.isValid(), "Should read chunk after seeking");
        
        // Test EOF behavior
        while (!demuxer1->isEOF()) {
            auto chunk = demuxer1->readChunk();
            if (!chunk.isValid()) break;
        }
        ASSERT_TRUE(demuxer1->isEOF(), "Should reach EOF");
        
        // Test reading after EOF
        auto eof_chunk = demuxer1->readChunk();
        ASSERT_FALSE(eof_chunk.isValid(), "Should not read valid chunk after EOF");
    }
};

int main() {
    TestSuite suite("Demuxer Integration Tests");
    
    // Add all test cases
    suite.addTest(std::make_unique<RIFFDemuxerIntegrationTest>());
    suite.addTest(std::make_unique<OggDemuxerIntegrationTest>());
    suite.addTest(std::make_unique<AIFFDemuxerIntegrationTest>());
    suite.addTest(std::make_unique<MP4DemuxerIntegrationTest>());
    suite.addTest(std::make_unique<FLACDemuxerIntegrationTest>());
    suite.addTest(std::make_unique<MultiThreadedDemuxerTest>());
    suite.addTest(std::make_unique<DemuxerErrorRecoveryTest>());
    suite.addTest(std::make_unique<IOHandlerIntegrationTest>());
    
    // Run all tests
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results);
}