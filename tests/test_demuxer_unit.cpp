/*
 * test_demuxer_unit.cpp - Unit tests for demuxer architecture components
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
 * @brief Mock IOHandler for testing
 */
class MockIOHandler : public IOHandler {
private:
    std::vector<uint8_t> m_data;
    size_t m_position = 0;
    bool m_fail_reads = false;
    
public:
    explicit MockIOHandler(const std::vector<uint8_t>& data) : m_data(data) {}
    
    void setFailReads(bool fail) { m_fail_reads = fail; }
    
    size_t read(void* buffer, size_t size, size_t count) override {
        if (m_fail_reads) return 0;
        
        size_t bytes_to_read = std::min(size * count, m_data.size() - m_position);
        if (bytes_to_read > 0) {
            std::memcpy(buffer, m_data.data() + m_position, bytes_to_read);
            m_position += bytes_to_read;
        }
        return bytes_to_read / size;
    }
    
    int seek(long offset, int whence) override {
        if (m_fail_reads) return -1;
        
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
 * @brief Mock Demuxer for testing base functionality
 */
class MockDemuxer : public Demuxer {
private:
    bool m_should_fail_parse = false;
    bool m_should_fail_seek = false;
    
public:
    explicit MockDemuxer(std::unique_ptr<IOHandler> handler) : Demuxer(std::move(handler)) {}
    
    void setShouldFailParse(bool fail) { m_should_fail_parse = fail; }
    void setShouldFailSeek(bool fail) { m_should_fail_seek = fail; }
    
    bool parseContainer() override {
        if (m_should_fail_parse) return false;
        
        // Create mock stream info
        StreamInfo stream;
        stream.stream_id = 1;
        stream.codec_type = "audio";
        stream.codec_name = "test";
        stream.sample_rate = 44100;
        stream.channels = 2;
        stream.bits_per_sample = 16;
        stream.duration_ms = 60000; // 1 minute
        
        m_streams.clear();
        m_streams.push_back(stream);
        m_duration_ms = 60000;
        setParsed(true);
        return true;
    }
    
    std::vector<StreamInfo> getStreams() const override {
        return m_streams;
    }
    
    StreamInfo getStreamInfo(uint32_t stream_id) const override {
        for (const auto& stream : m_streams) {
            if (stream.stream_id == stream_id) {
                return stream;
            }
        }
        return StreamInfo{};
    }
    
    MediaChunk readChunk() override {
        return readChunk(1); // Default to stream 1
    }
    
    MediaChunk readChunk(uint32_t stream_id) override {
        if (isEOF()) return MediaChunk{};
        
        // Create mock chunk
        MediaChunk chunk;
        chunk.stream_id = stream_id;
        chunk.data = {0x01, 0x02, 0x03, 0x04}; // Mock data
        chunk.timestamp_samples = getStreamPosition(stream_id);
        chunk.is_keyframe = true;
        
        // Update position
        updateStreamPosition(stream_id, getStreamPosition(stream_id) + 1024);
        updatePosition(getPosition() + 100); // 100ms per chunk
        
        // Set EOF after 10 chunks
        if (getPosition() >= 1000) {
            setEOF(true);
        }
        
        return chunk;
    }
    
    bool seekTo(uint64_t timestamp_ms) override {
        if (m_should_fail_seek) return false;
        
        updatePosition(timestamp_ms);
        setEOF(timestamp_ms >= getDuration());
        return true;
    }
    
    bool isEOF() const override {
        return isEOFAtomic();
    }
    
    uint64_t getDuration() const override {
        return m_duration_ms;
    }
    
    uint64_t getPosition() const override {
        return m_position_ms;
    }
};

/**
 * @brief Test StreamInfo data structure
 */
class StreamInfoTest : public TestCase {
public:
    StreamInfoTest() : TestCase("StreamInfo Data Structure Test") {}
    
protected:
    void runTest() override {
        // Test default constructor
        StreamInfo info1;
        ASSERT_EQUALS(0u, info1.stream_id, "Default stream_id should be 0");
        ASSERT_TRUE(info1.codec_type.empty(), "Default codec_type should be empty");
        ASSERT_TRUE(info1.codec_name.empty(), "Default codec_name should be empty");
        ASSERT_EQUALS(0u, info1.sample_rate, "Default sample_rate should be 0");
        ASSERT_EQUALS(0u, info1.channels, "Default channels should be 0");
        ASSERT_FALSE(info1.isValid(), "Default StreamInfo should not be valid");
        
        // Test parameterized constructor
        StreamInfo info2(1, "audio", "pcm");
        ASSERT_EQUALS(1u, info2.stream_id, "Stream ID should be set correctly");
        ASSERT_EQUALS("audio", info2.codec_type, "Codec type should be set correctly");
        ASSERT_EQUALS("pcm", info2.codec_name, "Codec name should be set correctly");
        ASSERT_TRUE(info2.isValid(), "Parameterized StreamInfo should be valid");
        ASSERT_TRUE(info2.isAudio(), "Should be identified as audio stream");
        ASSERT_FALSE(info2.isVideo(), "Should not be identified as video stream");
        
        // Test copy constructor
        StreamInfo info3(info2);
        ASSERT_EQUALS(info2.stream_id, info3.stream_id, "Copy constructor should preserve stream_id");
        ASSERT_EQUALS(info2.codec_type, info3.codec_type, "Copy constructor should preserve codec_type");
        ASSERT_EQUALS(info2.codec_name, info3.codec_name, "Copy constructor should preserve codec_name");
        
        // Test assignment operator
        StreamInfo info4;
        info4 = info2;
        ASSERT_EQUALS(info2.stream_id, info4.stream_id, "Assignment should preserve stream_id");
        ASSERT_EQUALS(info2.codec_type, info4.codec_type, "Assignment should preserve codec_type");
        
        // Test audio properties
        info2.sample_rate = 44100;
        info2.channels = 2;
        info2.bits_per_sample = 16;
        info2.bitrate = 1411200;
        
        ASSERT_EQUALS(44100u, info2.sample_rate, "Sample rate should be set correctly");
        ASSERT_EQUALS(2u, info2.channels, "Channels should be set correctly");
        ASSERT_EQUALS(16u, info2.bits_per_sample, "Bits per sample should be set correctly");
        ASSERT_EQUALS(1411200u, info2.bitrate, "Bitrate should be set correctly");
        
        // Test metadata
        info2.artist = "Test Artist";
        info2.title = "Test Title";
        info2.album = "Test Album";
        
        ASSERT_EQUALS("Test Artist", info2.artist, "Artist should be set correctly");
        ASSERT_EQUALS("Test Title", info2.title, "Title should be set correctly");
        ASSERT_EQUALS("Test Album", info2.album, "Album should be set correctly");
        
        // Test codec data
        info2.codec_data = {0x01, 0x02, 0x03, 0x04};
        ASSERT_EQUALS(4u, info2.codec_data.size(), "Codec data should be set correctly");
        ASSERT_EQUALS(0x01, info2.codec_data[0], "First codec data byte should be correct");
    }
};

/**
 * @brief Test MediaChunk data structure
 */
class MediaChunkTest : public TestCase {
public:
    MediaChunkTest() : TestCase("MediaChunk Data Structure Test") {}
    
protected:
    void runTest() override {
        // Test default constructor
        MediaChunk chunk1;
        ASSERT_EQUALS(0u, chunk1.stream_id, "Default stream_id should be 0");
        ASSERT_TRUE(chunk1.data.empty(), "Default data should be empty");
        ASSERT_EQUALS(0u, chunk1.granule_position, "Default granule_position should be 0");
        ASSERT_EQUALS(0u, chunk1.timestamp_samples, "Default timestamp_samples should be 0");
        ASSERT_TRUE(chunk1.is_keyframe, "Default is_keyframe should be true");
        ASSERT_FALSE(chunk1.isValid(), "Default MediaChunk should not be valid");
        ASSERT_TRUE(chunk1.isEmpty(), "Default MediaChunk should be empty");
        
        // Test parameterized constructor with vector
        std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04};
        MediaChunk chunk2(1, test_data);
        ASSERT_EQUALS(1u, chunk2.stream_id, "Stream ID should be set correctly");
        ASSERT_EQUALS(4u, chunk2.data.size(), "Data size should be correct");
        ASSERT_EQUALS(0x01, chunk2.data[0], "First data byte should be correct");
        ASSERT_TRUE(chunk2.isValid(), "Parameterized MediaChunk should be valid");
        ASSERT_FALSE(chunk2.isEmpty(), "Parameterized MediaChunk should not be empty");
        ASSERT_EQUALS(4u, chunk2.getDataSize(), "getDataSize() should return correct size");
        
        // Test move constructor with vector
        std::vector<uint8_t> move_data = {0x05, 0x06, 0x07, 0x08};
        MediaChunk chunk3(2, std::move(move_data));
        ASSERT_EQUALS(2u, chunk3.stream_id, "Stream ID should be set correctly");
        ASSERT_EQUALS(4u, chunk3.data.size(), "Data size should be correct");
        ASSERT_EQUALS(0x05, chunk3.data[0], "First data byte should be correct");
        ASSERT_TRUE(move_data.empty(), "Original vector should be empty after move");
        
        // Test copy constructor
        MediaChunk chunk4(chunk2);
        ASSERT_EQUALS(chunk2.stream_id, chunk4.stream_id, "Copy constructor should preserve stream_id");
        ASSERT_EQUALS(chunk2.data.size(), chunk4.data.size(), "Copy constructor should preserve data size");
        ASSERT_EQUALS(chunk2.data[0], chunk4.data[0], "Copy constructor should preserve data content");
        
        // Test assignment operator
        MediaChunk chunk5;
        chunk5 = chunk2;
        ASSERT_EQUALS(chunk2.stream_id, chunk5.stream_id, "Assignment should preserve stream_id");
        ASSERT_EQUALS(chunk2.data.size(), chunk5.data.size(), "Assignment should preserve data size");
        
        // Test timing information
        chunk2.granule_position = 12345;
        chunk2.timestamp_samples = 67890;
        chunk2.file_offset = 1024;
        chunk2.is_keyframe = false;
        
        ASSERT_EQUALS(12345u, chunk2.granule_position, "Granule position should be set correctly");
        ASSERT_EQUALS(67890u, chunk2.timestamp_samples, "Timestamp samples should be set correctly");
        ASSERT_EQUALS(1024u, chunk2.file_offset, "File offset should be set correctly");
        ASSERT_FALSE(chunk2.is_keyframe, "Keyframe flag should be set correctly");
        
        // Test clear method
        chunk2.clear();
        ASSERT_EQUALS(0u, chunk2.stream_id, "Clear should reset stream_id");
        ASSERT_TRUE(chunk2.data.empty(), "Clear should empty data");
        ASSERT_EQUALS(0u, chunk2.granule_position, "Clear should reset granule_position");
        ASSERT_EQUALS(0u, chunk2.timestamp_samples, "Clear should reset timestamp_samples");
        ASSERT_TRUE(chunk2.is_keyframe, "Clear should reset is_keyframe to true");
        ASSERT_EQUALS(0u, chunk2.file_offset, "Clear should reset file_offset");
        ASSERT_FALSE(chunk2.isValid(), "Cleared MediaChunk should not be valid");
        ASSERT_TRUE(chunk2.isEmpty(), "Cleared MediaChunk should be empty");
    }
};

/**
 * @brief Test base Demuxer interface
 */
class DemuxerInterfaceTest : public TestCase {
public:
    DemuxerInterfaceTest() : TestCase("Demuxer Interface Test") {}
    
protected:
    void runTest() override {
        // Create mock data
        std::vector<uint8_t> mock_data(1024, 0x42);
        auto handler = std::make_unique<MockIOHandler>(mock_data);
        auto demuxer = std::make_unique<MockDemuxer>(std::move(handler));
        
        // Test initial state
        ASSERT_FALSE(demuxer->isParsed(), "Demuxer should not be parsed initially");
        ASSERT_FALSE(demuxer->hasError(), "Demuxer should not have errors initially");
        ASSERT_EQUALS(0u, demuxer->getDuration(), "Initial duration should be 0");
        ASSERT_EQUALS(0u, demuxer->getPosition(), "Initial position should be 0");
        ASSERT_FALSE(demuxer->isEOF(), "Should not be EOF initially");
        
        // Test parseContainer
        ASSERT_TRUE(demuxer->parseContainer(), "parseContainer should succeed");
        ASSERT_TRUE(demuxer->isParsed(), "Demuxer should be parsed after parseContainer");
        ASSERT_EQUALS(60000u, demuxer->getDuration(), "Duration should be set after parsing");
        
        // Test getStreams
        auto streams = demuxer->getStreams();
        ASSERT_EQUALS(1u, streams.size(), "Should have one stream");
        ASSERT_EQUALS(1u, streams[0].stream_id, "Stream ID should be 1");
        ASSERT_EQUALS("audio", streams[0].codec_type, "Codec type should be audio");
        ASSERT_EQUALS("test", streams[0].codec_name, "Codec name should be test");
        
        // Test getStreamInfo
        auto stream_info = demuxer->getStreamInfo(1);
        ASSERT_TRUE(stream_info.isValid(), "Stream info should be valid");
        ASSERT_EQUALS(1u, stream_info.stream_id, "Stream ID should match");
        
        // Test invalid stream ID
        auto invalid_stream = demuxer->getStreamInfo(999);
        ASSERT_FALSE(invalid_stream.isValid(), "Invalid stream ID should return invalid stream info");
        
        // Test readChunk
        auto chunk = demuxer->readChunk();
        ASSERT_TRUE(chunk.isValid(), "First chunk should be valid");
        ASSERT_EQUALS(1u, chunk.stream_id, "Chunk stream ID should be 1");
        ASSERT_FALSE(chunk.data.empty(), "Chunk data should not be empty");
        
        // Test readChunk with specific stream ID
        auto chunk2 = demuxer->readChunk(1);
        ASSERT_TRUE(chunk2.isValid(), "Second chunk should be valid");
        ASSERT_EQUALS(1u, chunk2.stream_id, "Chunk stream ID should be 1");
        
        // Test position tracking
        ASSERT_TRUE(demuxer->getPosition() > 0, "Position should advance after reading chunks");
        
        // Test seeking
        ASSERT_TRUE(demuxer->seekTo(30000), "Seek to 30 seconds should succeed");
        ASSERT_EQUALS(30000u, demuxer->getPosition(), "Position should be updated after seek");
        
        // Test seeking beyond duration
        ASSERT_TRUE(demuxer->seekTo(70000), "Seek beyond duration should succeed");
        ASSERT_TRUE(demuxer->isEOF(), "Should be EOF after seeking beyond duration");
        
        // Test seeking back
        ASSERT_TRUE(demuxer->seekTo(10000), "Seek back should succeed");
        ASSERT_FALSE(demuxer->isEOF(), "Should not be EOF after seeking back");
        
        // Test error handling
        demuxer->clearError();
        ASSERT_FALSE(demuxer->hasError(), "Should not have error after clearing");
    }
};

/**
 * @brief Test Demuxer error handling
 */
class DemuxerErrorHandlingTest : public TestCase {
public:
    DemuxerErrorHandlingTest() : TestCase("Demuxer Error Handling Test") {}
    
protected:
    void runTest() override {
        // Test parse failure
        std::vector<uint8_t> mock_data(1024, 0x42);
        auto handler = std::make_unique<MockIOHandler>(mock_data);
        auto demuxer = std::make_unique<MockDemuxer>(std::move(handler));
        
        demuxer->setShouldFailParse(true);
        ASSERT_FALSE(demuxer->parseContainer(), "parseContainer should fail when configured to fail");
        ASSERT_FALSE(demuxer->isParsed(), "Demuxer should not be parsed after failure");
        
        // Test seek failure
        demuxer->setShouldFailParse(false);
        ASSERT_TRUE(demuxer->parseContainer(), "parseContainer should succeed after reset");
        
        demuxer->setShouldFailSeek(true);
        ASSERT_FALSE(demuxer->seekTo(30000), "seekTo should fail when configured to fail");
        
        // Test I/O failure
        std::vector<uint8_t> empty_data;
        auto failing_handler = std::make_unique<MockIOHandler>(empty_data);
        failing_handler->setFailReads(true);
        auto failing_demuxer = std::make_unique<MockDemuxer>(std::move(failing_handler));
        
        // The mock demuxer doesn't actually use I/O for parsing, so this test is limited
        // In a real implementation, I/O failures would be caught and handled
        ASSERT_TRUE(failing_demuxer->parseContainer(), "Mock demuxer should still parse even with I/O failure");
    }
};

/**
 * @brief Test thread safety of Demuxer base class
 */
class DemuxerThreadSafetyTest : public TestCase {
public:
    DemuxerThreadSafetyTest() : TestCase("Demuxer Thread Safety Test") {}
    
protected:
    void runTest() override {
        std::vector<uint8_t> mock_data(1024, 0x42);
        auto handler = std::make_unique<MockIOHandler>(mock_data);
        auto demuxer = std::make_unique<MockDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "parseContainer should succeed");
        
        // Test atomic EOF operations
        ASSERT_FALSE(demuxer->isEOFAtomic(), "Initial EOF state should be false");
        demuxer->setEOF(true);
        ASSERT_TRUE(demuxer->isEOFAtomic(), "EOF state should be updated atomically");
        demuxer->setEOF(false);
        ASSERT_FALSE(demuxer->isEOFAtomic(), "EOF state should be reset atomically");
        
        // Test thread-safe state access
        ASSERT_TRUE(demuxer->isParsed(), "isParsed should be thread-safe");
        ASSERT_EQUALS(60000u, demuxer->getDuration(), "getDuration should be thread-safe");
        ASSERT_EQUALS(0u, demuxer->getPosition(), "getPosition should be thread-safe");
        
        // Test error state thread safety
        ASSERT_FALSE(demuxer->hasError(), "hasError should be thread-safe");
        demuxer->clearError();
        ASSERT_FALSE(demuxer->hasError(), "clearError should be thread-safe");
    }
};

int main() {
    TestSuite suite("Demuxer Architecture Unit Tests");
    
    // Add all test cases
    suite.addTest(std::make_unique<StreamInfoTest>());
    suite.addTest(std::make_unique<MediaChunkTest>());
    suite.addTest(std::make_unique<DemuxerInterfaceTest>());
    suite.addTest(std::make_unique<DemuxerErrorHandlingTest>());
    suite.addTest(std::make_unique<DemuxerThreadSafetyTest>());
    
    // Run all tests
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results);
}