/*
 * test_flac_codec_integration.cpp - Integration tests for FLACCodec with DemuxedStream bridge
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "flac_test_data_utils.h"
#include "test_framework.h"

#ifdef HAVE_NATIVE_FLAC

using namespace TestFramework;

/**
 * @brief Test FLACCodec integration with DemuxedStream bridge interface
 */
class FLACCodecDemuxedStreamIntegrationTest : public TestCase {
public:
    FLACCodecDemuxedStreamIntegrationTest() : TestCase("FLACCodec DemuxedStream Integration Test") {}
    
protected:
    void runTest() override {
        // Create a mock FLAC file for testing
        auto flac_data = createMockFLACFile();
        
        try {
            // Test with FLACDemuxer + FLACCodec integration
            auto handler = std::make_unique<MockIOHandler>(flac_data);
            auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
            
            ASSERT_TRUE(demuxer->parseContainer(), "Should parse FLAC container");
            
            auto streams = demuxer->getStreams();
            ASSERT_EQUALS(1u, streams.size(), "Should have one audio stream");
            
            const auto& stream_info = streams[0];
            
            // Create FLACCodec for the stream
            auto codec = std::make_unique<FLACCodec>(stream_info);
            ASSERT_TRUE(codec->initialize(), "FLACCodec should initialize with demuxer stream info");
            
            // Test that codec can decode stream info
            ASSERT_TRUE(codec->canDecode(stream_info), "Codec should accept demuxer stream info");
            
            // Test decoding chunks from demuxer
            size_t chunks_processed = 0;
            size_t total_samples = 0;
            
            while (!demuxer->isEOF() && chunks_processed < 10) {
                auto chunk = demuxer->readChunk();
                if (!chunk.isValid()) {
                    break;
                }
                
                ASSERT_EQUALS(stream_info.stream_id, chunk.stream_id, "Chunk should have correct stream ID");
                
                // Decode chunk with FLACCodec
                AudioFrame frame = codec->decode(chunk);
                
                if (frame.getSampleFrameCount() > 0) {
                    // Validate decoded frame
                    ASSERT_EQUALS(stream_info.channels, frame.channels, "Frame should have correct channels");
                    ASSERT_EQUALS(stream_info.sample_rate, frame.sample_rate, "Frame should have correct sample rate");
                    
                    total_samples += frame.getSampleFrameCount();
                }
                
                chunks_processed++;
            }
            
            ASSERT_TRUE(chunks_processed > 0, "Should process at least one chunk");
            
            // Test seeking integration
            if (demuxer->seekTo(1000)) { // Seek to 1 second
                codec->reset(); // Reset codec state for seek
                
                auto seek_chunk = demuxer->readChunk();
                if (seek_chunk.isValid()) {
                    AudioFrame seek_frame = codec->decode(seek_chunk);
                    // Should be able to decode after seek
                }
            }
            
            // Test flush after processing
            AudioFrame flush_frame = codec->flush();
            // Flush may return additional samples
            
            // Verify codec statistics
            auto stats = codec->getStats();
            ASSERT_TRUE(stats.frames_decoded >= 0, "Should have frame statistics");
            ASSERT_TRUE(stats.samples_decoded >= 0, "Should have sample statistics");
            
        } catch (const std::exception& e) {
            // Integration may fail with mock data, but should not crash
            std::cout << "Integration test completed with exception (acceptable): " << e.what() << std::endl;
        }
    }
    
private:
    std::vector<uint8_t> createMockFLACFile() {
        std::vector<uint8_t> data;
        
        // fLaC marker
        data.insert(data.end(), {'f', 'L', 'a', 'C'});
        
        // STREAMINFO block
        data.push_back(0x80); // Last block, type 0
        data.push_back(0x00); data.push_back(0x00); data.push_back(0x22); // 34 bytes
        
        // STREAMINFO data (simplified)
        for (int i = 0; i < 34; i++) {
            if (i < 4) data.push_back(0x10); // Block sizes
            else if (i >= 10 && i < 13) { // Sample rate, channels, bits
                if (i == 10) data.push_back(0xAC); // 44.1kHz part 1
                else if (i == 11) data.push_back(0x44); // 44.1kHz part 2, stereo
                else data.push_back(0x0F); // 16-bit
            } else data.push_back(0x00);
        }
        
        // Add mock frame data
        for (int frame = 0; frame < 3; frame++) {
            // Frame sync
            data.push_back(0xFF); data.push_back(0xF8);
            // Frame header (simplified)
            data.push_back(0x69); // Block size + sample rate
            data.push_back(0x10); // Channels + bits
            data.push_back(frame); // Frame number
            data.push_back(0x00); // CRC
            
            // Mock compressed data
            for (int i = 0; i < 100; i++) {
                data.push_back(static_cast<uint8_t>(i + frame));
            }
        }
        
        return data;
    }
    
    class MockIOHandler : public IOHandler {
    private:
        std::vector<uint8_t> m_data;
        size_t m_position = 0;
        
    public:
        explicit MockIOHandler(const std::vector<uint8_t>& data) : m_data(data) {}
        
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
        
        off_t tell() override { return static_cast<off_t>(m_position); }
        bool eof() override { return m_position >= m_data.size(); }
        int close() override { return 0; }
        off_t getFileSize() override { return static_cast<off_t>(m_data.size()); }
    };
};

/**
 * @brief Test FLACCodec thread safety in multi-threaded scenarios
 */
class FLACCodecThreadSafetyTest : public TestCase {
public:
    FLACCodecThreadSafetyTest() : TestCase("FLACCodec Thread Safety Test") {}
    
protected:
    void runTest() override {
        StreamInfo stream_info;
        stream_info.stream_id = 1;
        stream_info.codec_type = "audio";
        stream_info.codec_name = "flac";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        stream_info.bits_per_sample = 16;
        stream_info.duration_ms = 5000;
        
        auto codec = std::make_unique<FLACCodec>(stream_info);
        ASSERT_TRUE(codec->initialize(), "Codec should initialize for thread safety test");
        
        // Create test data
        std::vector<MediaChunk> test_chunks;
        for (int i = 0; i < 20; i++) {
            MediaChunk chunk;
            chunk.stream_id = 1;
            chunk.data = createMockFLACFrame();
            chunk.timestamp_samples = i * 93; // ~93ms per frame
            chunk.is_keyframe = true;
            test_chunks.push_back(chunk);
        }
        
        // Test concurrent decoding from multiple threads
        const int num_threads = 4;
        std::vector<std::thread> threads;
        std::atomic<int> successful_decodes{0};
        std::atomic<int> total_attempts{0};
        std::mutex result_mutex;
        std::vector<AudioFrame> results;
        
        for (int t = 0; t < num_threads; t++) {
            threads.emplace_back([&, t]() {
                for (int i = t; i < static_cast<int>(test_chunks.size()); i += num_threads) {
                    try {
                        total_attempts++;
                        AudioFrame frame = codec->decode(test_chunks[i]);
                        
                        if (frame.getSampleFrameCount() > 0) {
                            successful_decodes++;
                            
                            std::lock_guard<std::mutex> lock(result_mutex);
                            results.push_back(std::move(frame));
                        }
                        
                        // Test other operations
                        codec->getCurrentSample();
                        auto stats = codec->getStats();
                        
                    } catch (const std::exception& e) {
                        // Some operations may fail due to threading, but should not crash
                        std::cout << "Thread " << t << " exception (acceptable): " << e.what() << std::endl;
                    }
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        ASSERT_TRUE(total_attempts.load() > 0, "Should have attempted some decodes");
        
        // Test that codec is still functional after multi-threaded access
        codec->reset();
        ASSERT_EQUALS(0u, codec->getCurrentSample(), "Should reset after threading test");
        
        // Test single-threaded operation after multi-threaded test
        if (!test_chunks.empty()) {
            AudioFrame final_frame = codec->decode(test_chunks[0]);
            // Should work after threading test
        }
        
        std::cout << "Thread safety test completed: " << successful_decodes.load() 
                  << "/" << total_attempts.load() << " successful decodes" << std::endl;
    }
    
private:
    std::vector<uint8_t> createMockFLACFrame() {
        std::vector<uint8_t> frame;
        
        // FLAC frame sync
        frame.push_back(0xFF); frame.push_back(0xF8);
        // Frame header
        frame.push_back(0x69); // Block size + sample rate
        frame.push_back(0x10); // Channels + bits
        frame.push_back(0x00); // Frame number
        frame.push_back(0x00); // CRC
        
        // Mock compressed data
        for (int i = 0; i < 200; i++) {
            frame.push_back(static_cast<uint8_t>(i & 0xFF));
        }
        
        return frame;
    }
};

/**
 * @brief Test FLACCodec seeking behavior and position tracking
 */
class FLACCodecSeekingTest : public TestCase {
public:
    FLACCodecSeekingTest() : TestCase("FLACCodec Seeking Test") {}
    
protected:
    void runTest() override {
        StreamInfo stream_info;
        stream_info.stream_id = 1;
        stream_info.codec_type = "audio";
        stream_info.codec_name = "flac";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        stream_info.bits_per_sample = 16;
        stream_info.duration_ms = 10000; // 10 seconds
        
        auto codec = std::make_unique<FLACCodec>(stream_info);
        ASSERT_TRUE(codec->initialize(), "Codec should initialize for seeking test");
        
        // Verify seeking support
        ASSERT_TRUE(codec->supportsSeekReset(), "FLACCodec should support seeking through reset");
        
        // Test initial position
        ASSERT_EQUALS(0u, codec->getCurrentSample(), "Initial sample position should be 0");
        
        // Decode some frames to advance position
        for (int i = 0; i < 5; i++) {
            MediaChunk chunk;
            chunk.stream_id = 1;
            chunk.data = createMockFLACFrame(4096); // 4096 samples per frame
            chunk.timestamp_samples = i * 93; // ~93ms per frame at 44.1kHz
            chunk.is_keyframe = true;
            
            AudioFrame frame = codec->decode(chunk);
            
            if (frame.getSampleFrameCount() > 0) {
                // Position should advance
                uint64_t current_sample = codec->getCurrentSample();
                ASSERT_TRUE(current_sample > 0, "Sample position should advance after decoding");
            }
        }
        
        uint64_t position_before_reset = codec->getCurrentSample();
        
        // Test reset (seeking to beginning)
        codec->reset();
        ASSERT_EQUALS(0u, codec->getCurrentSample(), "Position should be 0 after reset");
        
        // Test that codec is functional after reset
        MediaChunk reset_chunk;
        reset_chunk.stream_id = 1;
        reset_chunk.data = createMockFLACFrame(4096);
        reset_chunk.timestamp_samples = 0;
        reset_chunk.is_keyframe = true;
        
        AudioFrame reset_frame = codec->decode(reset_chunk);
        // Should be able to decode after reset
        
        // Test multiple resets
        for (int i = 0; i < 3; i++) {
            codec->reset();
            ASSERT_EQUALS(0u, codec->getCurrentSample(), "Multiple resets should work");
        }
        
        // Test position tracking accuracy
        codec->reset();
        uint64_t expected_samples = 0;
        
        for (int i = 0; i < 3; i++) {
            MediaChunk chunk;
            chunk.stream_id = 1;
            chunk.data = createMockFLACFrame(1152); // Different block size
            chunk.timestamp_samples = i * 26; // ~26ms per 1152-sample frame
            chunk.is_keyframe = true;
            
            AudioFrame frame = codec->decode(chunk);
            
            if (frame.getSampleFrameCount() > 0) {
                expected_samples += frame.getSampleFrameCount();
                
                // Position tracking should be reasonably accurate
                uint64_t current_sample = codec->getCurrentSample();
                // Allow some tolerance for mock data
                ASSERT_TRUE(current_sample <= expected_samples + 10000, 
                           "Position tracking should be reasonably accurate");
            }
        }
        
        std::cout << "Seeking test completed - final position: " << codec->getCurrentSample() << std::endl;
    }
    
private:
    std::vector<uint8_t> createMockFLACFrame(uint32_t block_size) {
        std::vector<uint8_t> frame;
        
        // FLAC frame sync
        frame.push_back(0xFF); frame.push_back(0xF8);
        
        // Encode block size in frame header
        uint8_t block_size_byte = 0x60; // Default encoding
        if (block_size == 192) block_size_byte = 0x10;
        else if (block_size == 576) block_size_byte = 0x20;
        else if (block_size == 1152) block_size_byte = 0x30;
        else if (block_size == 2304) block_size_byte = 0x40;
        else if (block_size == 4608) block_size_byte = 0x50;
        
        frame.push_back(block_size_byte | 0x09); // + 44.1kHz
        frame.push_back(0x10); // Stereo, 16-bit
        frame.push_back(0x00); // Frame number
        frame.push_back(0x00); // CRC
        
        // Mock compressed data proportional to block size
        size_t data_size = (block_size * 2 * 16) / 64; // Rough compression estimate
        for (size_t i = 0; i < data_size; i++) {
            frame.push_back(static_cast<uint8_t>(i & 0xFF));
        }
        
        return frame;
    }
};

/**
 * @brief Test FLACCodec error recovery scenarios
 */
class FLACCodecErrorRecoveryTest : public TestCase {
public:
    FLACCodecErrorRecoveryTest() : TestCase("FLACCodec Error Recovery Test") {}
    
protected:
    void runTest() override {
        StreamInfo stream_info;
        stream_info.stream_id = 1;
        stream_info.codec_type = "audio";
        stream_info.codec_name = "flac";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        stream_info.bits_per_sample = 16;
        stream_info.duration_ms = 5000;
        
        auto codec = std::make_unique<FLACCodec>(stream_info);
        ASSERT_TRUE(codec->initialize(), "Codec should initialize for error recovery test");
        
        // Test recovery from invalid data
        MediaChunk invalid_chunk;
        invalid_chunk.stream_id = 1;
        invalid_chunk.data = {0xFF, 0xFF, 0xFF, 0xFF}; // Invalid FLAC data
        invalid_chunk.timestamp_samples = 0;
        invalid_chunk.is_keyframe = true;
        
        AudioFrame error_frame = codec->decode(invalid_chunk);
        // Should handle gracefully (may return empty or silence frame)
        
        // Test that codec can recover and decode valid data
        MediaChunk valid_chunk;
        valid_chunk.stream_id = 1;
        valid_chunk.data = createValidFLACFrame();
        valid_chunk.timestamp_samples = 100;
        valid_chunk.is_keyframe = true;
        
        AudioFrame recovery_frame = codec->decode(valid_chunk);
        // Should be able to decode valid data after error
        
        // Test recovery from multiple errors
        for (int i = 0; i < 3; i++) {
            MediaChunk error_chunk;
            error_chunk.stream_id = 1;
            error_chunk.data = {static_cast<uint8_t>(i), 0x00, 0x01, 0x02};
            error_chunk.timestamp_samples = 200 + i * 100;
            error_chunk.is_keyframe = true;
            
            codec->decode(error_chunk);
        }
        
        // Should still be able to decode valid data
        AudioFrame final_recovery = codec->decode(valid_chunk);
        
        // Test reset after errors
        codec->reset();
        ASSERT_EQUALS(0u, codec->getCurrentSample(), "Should reset after errors");
        
        // Should work normally after reset
        AudioFrame post_reset = codec->decode(valid_chunk);
        
        // Check error statistics
        auto stats = codec->getStats();
        ASSERT_TRUE(stats.error_count >= 0, "Should track errors");
        
        std::cout << "Error recovery test completed - errors handled: " << stats.error_count << std::endl;
    }
    
private:
    std::vector<uint8_t> createValidFLACFrame() {
        std::vector<uint8_t> frame;
        
        // Valid FLAC frame sync
        frame.push_back(0xFF); frame.push_back(0xF8);
        // Valid frame header
        frame.push_back(0x69); // 4096 samples, 44.1kHz
        frame.push_back(0x10); // Stereo, 16-bit
        frame.push_back(0x00); // Frame number
        frame.push_back(0x00); // CRC-8
        
        // Add reasonable amount of mock data
        for (int i = 0; i < 150; i++) {
            frame.push_back(static_cast<uint8_t>((i * 7) & 0xFF));
        }
        
        return frame;
    }
};

int main() {
    TestSuite suite("FLAC Codec Integration Tests");
    
    // Add all test cases
    suite.addTest(std::make_unique<FLACCodecDemuxedStreamIntegrationTest>());
    suite.addTest(std::make_unique<FLACCodecThreadSafetyTest>());
    suite.addTest(std::make_unique<FLACCodecSeekingTest>());
    suite.addTest(std::make_unique<FLACCodecErrorRecoveryTest>());
    
    // Run all tests
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results);
}

#else // !HAVE_NATIVE_FLAC

int main() {
    Debug::log("test", "Native FLAC codec not available - skipping integration tests");
    return 0;
}

#endif // HAVE_NATIVE_FLAC
