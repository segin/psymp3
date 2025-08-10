/*
 * test_demuxed_stream_unit.cpp - Unit tests for DemuxedStream bridge
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
 * @brief Mock AudioCodec for testing
 */
class MockAudioCodec : public AudioCodec {
private:
    bool m_initialized = false;
    uint32_t m_sample_rate = 44100;
    uint16_t m_channels = 2;
    uint16_t m_bits_per_sample = 16;
    size_t m_frame_counter = 0;
    
public:
    MockAudioCodec() = default;
    
    bool initialize(const StreamInfo& stream_info) override {
        m_sample_rate = stream_info.sample_rate;
        m_channels = stream_info.channels;
        m_bits_per_sample = stream_info.bits_per_sample;
        m_initialized = true;
        return true;
    }
    
    AudioFrame decode(const MediaChunk& chunk) override {
        if (!m_initialized) return AudioFrame{};
        
        AudioFrame frame;
        frame.sample_rate = m_sample_rate;
        frame.channels = m_channels;
        frame.bits_per_sample = m_bits_per_sample;
        
        // Generate mock PCM data (1024 samples per frame)
        size_t samples_per_frame = 1024;
        size_t bytes_per_sample = (m_bits_per_sample / 8) * m_channels;
        frame.data.resize(samples_per_frame * bytes_per_sample);
        
        // Fill with simple pattern based on frame counter
        for (size_t i = 0; i < frame.data.size(); ++i) {
            frame.data[i] = static_cast<uint8_t>((m_frame_counter + i) & 0xFF);
        }
        
        frame.sample_count = samples_per_frame;
        m_frame_counter++;
        
        return frame;
    }
    
    void reset() override {
        m_frame_counter = 0;
    }
    
    bool isInitialized() const override {
        return m_initialized;
    }
    
    std::string getCodecName() const override {
        return "mock";
    }
};

/**
 * @brief Mock Demuxer for DemuxedStream testing
 */
class MockStreamDemuxer : public Demuxer {
private:
    std::vector<MediaChunk> m_chunks;
    size_t m_chunk_index = 0;
    bool m_should_fail_parse = false;
    
public:
    explicit MockStreamDemuxer(std::unique_ptr<IOHandler> handler) : Demuxer(std::move(handler)) {
        // Pre-generate some mock chunks
        for (int i = 0; i < 10; ++i) {
            MediaChunk chunk;
            chunk.stream_id = 1;
            chunk.data = {static_cast<uint8_t>(i), static_cast<uint8_t>(i+1), 
                         static_cast<uint8_t>(i+2), static_cast<uint8_t>(i+3)};
            chunk.timestamp_samples = i * 1024;
            chunk.is_keyframe = true;
            m_chunks.push_back(chunk);
        }
    }
    
    void setShouldFailParse(bool fail) { m_should_fail_parse = fail; }
    
    bool parseContainer() override {
        if (m_should_fail_parse) return false;
        
        // Create mock stream info
        StreamInfo stream;
        stream.stream_id = 1;
        stream.codec_type = "audio";
        stream.codec_name = "mock";
        stream.sample_rate = 44100;
        stream.channels = 2;
        stream.bits_per_sample = 16;
        stream.duration_ms = 10000; // 10 seconds
        stream.duration_samples = 441000; // 10 seconds at 44.1kHz
        stream.artist = "Test Artist";
        stream.title = "Test Title";
        stream.album = "Test Album";
        
        m_streams.clear();
        m_streams.push_back(stream);
        m_duration_ms = 10000;
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
        return readChunk(1);
    }
    
    MediaChunk readChunk(uint32_t stream_id) override {
        if (m_chunk_index >= m_chunks.size()) {
            setEOF(true);
            return MediaChunk{};
        }
        
        MediaChunk chunk = m_chunks[m_chunk_index];
        m_chunk_index++;
        
        // Update position based on chunk
        updatePosition((m_chunk_index * 1000) / m_chunks.size() * getDuration() / 1000);
        
        return chunk;
    }
    
    bool seekTo(uint64_t timestamp_ms) override {
        // Simple seek implementation
        double ratio = static_cast<double>(timestamp_ms) / getDuration();
        m_chunk_index = static_cast<size_t>(ratio * m_chunks.size());
        if (m_chunk_index >= m_chunks.size()) {
            m_chunk_index = m_chunks.size();
            setEOF(true);
        } else {
            setEOF(false);
        }
        updatePosition(timestamp_ms);
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
 * @brief Mock IOHandler for DemuxedStream testing
 */
class StreamTestIOHandler : public IOHandler {
private:
    std::vector<uint8_t> m_data;
    size_t m_position = 0;
    
public:
    explicit StreamTestIOHandler(size_t size = 1024) : m_data(size, 0x42) {}
    
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
 * @brief Test DemuxedStream initialization
 */
class DemuxedStreamInitializationTest : public TestCase {
public:
    DemuxedStreamInitializationTest() : TestCase("DemuxedStream Initialization Test") {}
    
protected:
    void runTest() override {
        // Note: This test is conceptual since DemuxedStream constructor is complex
        // and requires actual file I/O or mocked MediaFactory integration
        
        // Test that we can create the necessary components
        auto handler = std::make_unique<StreamTestIOHandler>();
        auto demuxer = std::make_unique<MockStreamDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Mock demuxer should parse successfully");
        
        auto streams = demuxer->getStreams();
        ASSERT_EQUALS(1u, streams.size(), "Should have one stream");
        ASSERT_TRUE(streams[0].isAudio(), "Stream should be audio");
        
        auto codec = std::make_unique<MockAudioCodec>();
        ASSERT_TRUE(codec->initialize(streams[0]), "Mock codec should initialize successfully");
        
        // Test codec functionality
        MediaChunk chunk;
        chunk.stream_id = 1;
        chunk.data = {0x01, 0x02, 0x03, 0x04};
        
        AudioFrame frame = codec->decode(chunk);
        ASSERT_FALSE(frame.data.empty(), "Decoded frame should have data");
        ASSERT_EQUALS(44100u, frame.sample_rate, "Frame sample rate should match");
        ASSERT_EQUALS(2u, frame.channels, "Frame channels should match");
        ASSERT_EQUALS(16u, frame.bits_per_sample, "Frame bits per sample should match");
    }
};

/**
 * @brief Test stream information access
 */
class StreamInfoAccessTest : public TestCase {
public:
    StreamInfoAccessTest() : TestCase("Stream Info Access Test") {}
    
protected:
    void runTest() override {
        auto handler = std::make_unique<StreamTestIOHandler>();
        auto demuxer = std::make_unique<MockStreamDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Demuxer should parse successfully");
        
        // Test stream information
        auto streams = demuxer->getStreams();
        ASSERT_EQUALS(1u, streams.size(), "Should have one stream");
        
        const auto& stream = streams[0];
        ASSERT_EQUALS(1u, stream.stream_id, "Stream ID should be 1");
        ASSERT_EQUALS("audio", stream.codec_type, "Codec type should be audio");
        ASSERT_EQUALS("mock", stream.codec_name, "Codec name should be mock");
        ASSERT_EQUALS(44100u, stream.sample_rate, "Sample rate should be 44100");
        ASSERT_EQUALS(2u, stream.channels, "Channels should be 2");
        ASSERT_EQUALS(16u, stream.bits_per_sample, "Bits per sample should be 16");
        ASSERT_EQUALS(10000u, stream.duration_ms, "Duration should be 10 seconds");
        ASSERT_EQUALS("Test Artist", stream.artist, "Artist should be set");
        ASSERT_EQUALS("Test Title", stream.title, "Title should be set");
        ASSERT_EQUALS("Test Album", stream.album, "Album should be set");
        
        // Test specific stream info query
        auto stream_info = demuxer->getStreamInfo(1);
        ASSERT_TRUE(stream_info.isValid(), "Stream info should be valid");
        ASSERT_EQUALS(stream.stream_id, stream_info.stream_id, "Stream info should match");
        
        // Test invalid stream ID
        auto invalid_info = demuxer->getStreamInfo(999);
        ASSERT_FALSE(invalid_info.isValid(), "Invalid stream ID should return invalid info");
    }
};

/**
 * @brief Test chunk reading and buffering
 */
class ChunkReadingTest : public TestCase {
public:
    ChunkReadingTest() : TestCase("Chunk Reading Test") {}
    
protected:
    void runTest() override {
        auto handler = std::make_unique<StreamTestIOHandler>();
        auto demuxer = std::make_unique<MockStreamDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Demuxer should parse successfully");
        ASSERT_FALSE(demuxer->isEOF(), "Should not be EOF initially");
        
        // Read chunks sequentially
        std::vector<MediaChunk> chunks;
        while (!demuxer->isEOF() && chunks.size() < 15) { // Limit to prevent infinite loop
            auto chunk = demuxer->readChunk();
            if (chunk.isValid()) {
                chunks.push_back(chunk);
                ASSERT_EQUALS(1u, chunk.stream_id, "Chunk stream ID should be 1");
                ASSERT_FALSE(chunk.data.empty(), "Chunk data should not be empty");
                ASSERT_TRUE(chunk.is_keyframe, "Audio chunks should be keyframes");
            } else {
                break; // EOF reached
            }
        }
        
        ASSERT_EQUALS(10u, chunks.size(), "Should read all 10 mock chunks");
        ASSERT_TRUE(demuxer->isEOF(), "Should be EOF after reading all chunks");
        
        // Verify chunk progression
        for (size_t i = 0; i < chunks.size(); ++i) {
            ASSERT_EQUALS(i * 1024, chunks[i].timestamp_samples, "Chunk timestamp should progress");
            ASSERT_EQUALS(static_cast<uint8_t>(i), chunks[i].data[0], "Chunk data should be unique");
        }
    }
};

/**
 * @brief Test seeking functionality
 */
class SeekingTest : public TestCase {
public:
    SeekingTest() : TestCase("Seeking Test") {}
    
protected:
    void runTest() override {
        auto handler = std::make_unique<StreamTestIOHandler>();
        auto demuxer = std::make_unique<MockStreamDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Demuxer should parse successfully");
        ASSERT_EQUALS(10000u, demuxer->getDuration(), "Duration should be 10 seconds");
        ASSERT_EQUALS(0u, demuxer->getPosition(), "Initial position should be 0");
        
        // Test seeking to middle
        ASSERT_TRUE(demuxer->seekTo(5000), "Seek to 5 seconds should succeed");
        ASSERT_EQUALS(5000u, demuxer->getPosition(), "Position should be updated");
        ASSERT_FALSE(demuxer->isEOF(), "Should not be EOF after seeking to middle");
        
        // Test seeking to end
        ASSERT_TRUE(demuxer->seekTo(10000), "Seek to end should succeed");
        ASSERT_EQUALS(10000u, demuxer->getPosition(), "Position should be at end");
        ASSERT_TRUE(demuxer->isEOF(), "Should be EOF after seeking to end");
        
        // Test seeking beyond end
        ASSERT_TRUE(demuxer->seekTo(15000), "Seek beyond end should succeed");
        ASSERT_TRUE(demuxer->isEOF(), "Should be EOF after seeking beyond end");
        
        // Test seeking back to beginning
        ASSERT_TRUE(demuxer->seekTo(0), "Seek to beginning should succeed");
        ASSERT_EQUALS(0u, demuxer->getPosition(), "Position should be at beginning");
        ASSERT_FALSE(demuxer->isEOF(), "Should not be EOF after seeking to beginning");
        
        // Test that we can read after seeking
        auto chunk = demuxer->readChunk();
        ASSERT_TRUE(chunk.isValid(), "Should be able to read chunk after seeking");
    }
};

/**
 * @brief Test audio frame decoding
 */
class AudioFrameDecodingTest : public TestCase {
public:
    AudioFrameDecodingTest() : TestCase("Audio Frame Decoding Test") {}
    
protected:
    void runTest() override {
        auto handler = std::make_unique<StreamTestIOHandler>();
        auto demuxer = std::make_unique<MockStreamDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Demuxer should parse successfully");
        
        auto streams = demuxer->getStreams();
        auto codec = std::make_unique<MockAudioCodec>();
        ASSERT_TRUE(codec->initialize(streams[0]), "Codec should initialize successfully");
        
        // Read and decode chunks
        std::vector<AudioFrame> frames;
        for (int i = 0; i < 5; ++i) {
            auto chunk = demuxer->readChunk();
            if (chunk.isValid()) {
                auto frame = codec->decode(chunk);
                ASSERT_FALSE(frame.data.empty(), "Decoded frame should have data");
                ASSERT_EQUALS(44100u, frame.sample_rate, "Frame sample rate should be correct");
                ASSERT_EQUALS(2u, frame.channels, "Frame channels should be correct");
                ASSERT_EQUALS(16u, frame.bits_per_sample, "Frame bits per sample should be correct");
                ASSERT_EQUALS(1024u, frame.sample_count, "Frame sample count should be correct");
                
                // Verify frame data size
                size_t expected_size = frame.sample_count * (frame.bits_per_sample / 8) * frame.channels;
                ASSERT_EQUALS(expected_size, frame.data.size(), "Frame data size should be correct");
                
                frames.push_back(frame);
            }
        }
        
        ASSERT_EQUALS(5u, frames.size(), "Should decode 5 frames");
        
        // Verify frames are different (mock codec generates different data)
        ASSERT_NOT_EQUALS(frames[0].data[0], frames[1].data[0], "Frames should have different data");
        
        // Test codec reset
        codec->reset();
        auto reset_chunk = demuxer->readChunk();
        if (reset_chunk.isValid()) {
            auto reset_frame = codec->decode(reset_chunk);
            // After reset, the mock codec should start from frame 0 again
            ASSERT_EQUALS(frames[0].data[0], reset_frame.data[0], "Reset should restart frame generation");
        }
    }
};

/**
 * @brief Test error handling in stream processing
 */
class StreamErrorHandlingTest : public TestCase {
public:
    StreamErrorHandlingTest() : TestCase("Stream Error Handling Test") {}
    
protected:
    void runTest() override {
        auto handler = std::make_unique<StreamTestIOHandler>();
        auto demuxer = std::make_unique<MockStreamDemuxer>(std::move(handler));
        
        // Test parse failure
        demuxer->setShouldFailParse(true);
        ASSERT_FALSE(demuxer->parseContainer(), "Parse should fail when configured to fail");
        ASSERT_FALSE(demuxer->isParsed(), "Should not be parsed after failure");
        
        // Test that operations fail gracefully when not parsed
        auto streams = demuxer->getStreams();
        ASSERT_TRUE(streams.empty(), "Should have no streams when not parsed");
        
        auto chunk = demuxer->readChunk();
        ASSERT_FALSE(chunk.isValid(), "Should not read valid chunks when not parsed");
        
        // Test recovery after fixing parse issue
        demuxer->setShouldFailParse(false);
        ASSERT_TRUE(demuxer->parseContainer(), "Parse should succeed after reset");
        ASSERT_TRUE(demuxer->isParsed(), "Should be parsed after success");
        
        streams = demuxer->getStreams();
        ASSERT_FALSE(streams.empty(), "Should have streams after successful parse");
    }
};

/**
 * @brief Test stream switching functionality
 */
class StreamSwitchingTest : public TestCase {
public:
    StreamSwitchingTest() : TestCase("Stream Switching Test") {}
    
protected:
    void runTest() override {
        // This test is conceptual since our mock demuxer only has one stream
        // In a real implementation, this would test switching between multiple audio streams
        
        auto handler = std::make_unique<StreamTestIOHandler>();
        auto demuxer = std::make_unique<MockStreamDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Demuxer should parse successfully");
        
        auto streams = demuxer->getStreams();
        ASSERT_EQUALS(1u, streams.size(), "Mock demuxer has one stream");
        
        // Test reading from specific stream
        auto chunk1 = demuxer->readChunk(1);
        ASSERT_TRUE(chunk1.isValid(), "Should read valid chunk from stream 1");
        ASSERT_EQUALS(1u, chunk1.stream_id, "Chunk should be from stream 1");
        
        // Test reading from invalid stream
        auto chunk_invalid = demuxer->readChunk(999);
        ASSERT_FALSE(chunk_invalid.isValid(), "Should not read valid chunk from invalid stream");
        
        // Test stream info for valid and invalid streams
        auto valid_info = demuxer->getStreamInfo(1);
        ASSERT_TRUE(valid_info.isValid(), "Valid stream should have valid info");
        
        auto invalid_info = demuxer->getStreamInfo(999);
        ASSERT_FALSE(invalid_info.isValid(), "Invalid stream should have invalid info");
    }
};

int main() {
    TestSuite suite("DemuxedStream Unit Tests");
    
    // Add all test cases
    suite.addTest(std::make_unique<DemuxedStreamInitializationTest>());
    suite.addTest(std::make_unique<StreamInfoAccessTest>());
    suite.addTest(std::make_unique<ChunkReadingTest>());
    suite.addTest(std::make_unique<SeekingTest>());
    suite.addTest(std::make_unique<AudioFrameDecodingTest>());
    suite.addTest(std::make_unique<StreamErrorHandlingTest>());
    suite.addTest(std::make_unique<StreamSwitchingTest>());
    
    // Run all tests
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results);
}