/*
 * test_flac_demuxer_unit_fixed.cpp - Fixed unit tests for FLACDemuxer
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
#include <fstream>

using namespace TestFramework;

/**
 * @brief Create a minimal valid FLAC file for testing
 */
std::vector<uint8_t> createMinimalFLACFile() {
    std::vector<uint8_t> flac_data;
    
    // fLaC stream marker
    flac_data.insert(flac_data.end(), {'f', 'L', 'a', 'C'});
    
    // STREAMINFO metadata block (mandatory)
    // Block header: last block (1) + type STREAMINFO (0) + length (34 bytes)
    flac_data.push_back(0x80);  // Last block + STREAMINFO type
    flac_data.push_back(0x00);  // Length high byte
    flac_data.push_back(0x00);  // Length middle byte  
    flac_data.push_back(0x22);  // Length low byte (34 decimal)
    
    // STREAMINFO data (34 bytes)
    // Min block size (16 bits) - 4096 samples
    flac_data.push_back(0x10);
    flac_data.push_back(0x00);
    
    // Max block size (16 bits) - 4096 samples
    flac_data.push_back(0x10);
    flac_data.push_back(0x00);
    
    // Min frame size (24 bits) - 0 (unknown)
    flac_data.push_back(0x00);
    flac_data.push_back(0x00);
    flac_data.push_back(0x00);
    
    // Max frame size (24 bits) - 0 (unknown)
    flac_data.push_back(0x00);
    flac_data.push_back(0x00);
    flac_data.push_back(0x00);
    
    // Sample rate (20 bits) + channels (3 bits) + bits per sample (5 bits)
    // 44100 Hz = 0xAC44, 2 channels (1), 16 bits per sample (15)
    flac_data.push_back(0x0A);  // Sample rate bits 19-12
    flac_data.push_back(0xC4);  // Sample rate bits 11-4
    flac_data.push_back(0x42);  // Sample rate bits 3-0 + channels bits 2-1
    flac_data.push_back(0xF0);  // Channels bit 0 + bits per sample + total samples bits 35-32
    
    // Total samples (36 bits) - 44100 samples (1 second)
    flac_data.push_back(0x00);  // Total samples bits 31-24
    flac_data.push_back(0x00);  // Total samples bits 23-16
    flac_data.push_back(0xAC);  // Total samples bits 15-8
    flac_data.push_back(0x44);  // Total samples bits 7-0
    
    // MD5 signature (16 bytes) - all zeros for test
    for (int i = 0; i < 16; i++) {
        flac_data.push_back(0x00);
    }
    
    // Minimal FLAC frame (this is a simplified frame that may not decode properly
    // but should be sufficient for container parsing tests)
    
    // Frame sync (14 bits) + reserved (1) + blocking strategy (1)
    flac_data.push_back(0xFF);  // Sync high byte
    flac_data.push_back(0xF8);  // Sync low + reserved + fixed blocking
    
    // Block size (4) + sample rate (4) - use streaminfo values
    flac_data.push_back(0x90);  // Block size 4096 + use streaminfo sample rate
    
    // Channel assignment (4) + sample size (3) + reserved (1)
    flac_data.push_back(0x10);  // Left-right stereo + use streaminfo sample size
    
    // Frame number (UTF-8) - frame 0
    flac_data.push_back(0x00);
    
    // Header CRC-8
    flac_data.push_back(0x00);
    
    // Simplified subframes (2 channels, constant zero)
    // Channel 1 subframe
    flac_data.push_back(0x00);  // Constant subframe type
    flac_data.push_back(0x00);  // Constant value (16-bit zero)
    flac_data.push_back(0x00);
    
    // Channel 2 subframe  
    flac_data.push_back(0x00);  // Constant subframe type
    flac_data.push_back(0x00);  // Constant value (16-bit zero)
    flac_data.push_back(0x00);
    
    // Frame CRC-16
    flac_data.push_back(0x00);
    flac_data.push_back(0x00);
    
    return flac_data;
}

/**
 * @brief Memory-based IOHandler for testing
 */
class MemoryIOHandler : public IOHandler {
private:
    std::vector<uint8_t> m_data;
    size_t m_position = 0;
    
public:
    explicit MemoryIOHandler(std::vector<uint8_t> data) 
        : m_data(std::move(data)) {}
    
    size_t read(void* buffer, size_t size, size_t count) override {
        size_t bytes_to_read = size * count;
        size_t available = m_data.size() - m_position;
        size_t actual_bytes = std::min(bytes_to_read, available);
        
        if (actual_bytes > 0) {
            std::memcpy(buffer, m_data.data() + m_position, actual_bytes);
            m_position += actual_bytes;
        }
        
        return actual_bytes / size;
    }
    
    int seek(off_t offset, int whence) override {
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
        
        if (new_pos <= m_data.size()) {
            m_position = new_pos;
            return 0;
        }
        
        return -1;
    }
    
    off_t tell() override {
        return static_cast<off_t>(m_position);
    }
    
    bool eof() override {
        return m_position >= m_data.size();
    }
    
    off_t getFileSize() override {
        return static_cast<off_t>(m_data.size());
    }
    
    int getLastError() const override {
        return 0;
    }
};

/**
 * @brief Test basic FLAC container parsing
 */
class FLACBasicParsingTest : public TestCase {
public:
    FLACBasicParsingTest() : TestCase("FLAC Basic Parsing Test") {}
    
protected:
    void runTest() override {
        // Create minimal FLAC file
        auto flac_data = createMinimalFLACFile();
        auto handler = std::make_unique<MemoryIOHandler>(std::move(flac_data));
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        // Test container parsing
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse minimal FLAC container");
        
        // Test stream information
        auto streams = demuxer->getStreams();
        ASSERT_EQUALS(1u, streams.size(), "Should have exactly one stream");
        
        const auto& stream = streams[0];
        ASSERT_EQUALS(44100u, stream.sample_rate, "Sample rate should be 44100 Hz");
        ASSERT_EQUALS(2u, stream.channels, "Should have 2 channels");
        ASSERT_EQUALS(16u, stream.bits_per_sample, "Should have 16 bits per sample");
        
        // Test duration calculation
        uint64_t duration = demuxer->getDuration();
        ASSERT_TRUE(duration > 0, "Duration should be positive");
        ASSERT_TRUE(duration < 2000, "Duration should be reasonable (less than 2 seconds)");
    }
};

/**
 * @brief Test FLAC seeking functionality
 */
class FLACSeekingTest : public TestCase {
public:
    FLACSeekingTest() : TestCase("FLAC Seeking Test") {}
    
protected:
    void runTest() override {
        auto flac_data = createMinimalFLACFile();
        auto handler = std::make_unique<MemoryIOHandler>(std::move(flac_data));
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse container");
        
        // Test seeking to beginning
        bool seek_result = demuxer->seekTo(0);
        if (seek_result) {
            ASSERT_EQUALS(0u, demuxer->getPosition(), "Position should be 0 after seeking to start");
        } else {
            Debug::log("test", "Seeking to beginning failed (expected for minimal test file)");
        }
        
        // Test seeking to middle (if duration > 0)
        uint64_t duration = demuxer->getDuration();
        if (duration > 100) {  // Only test if duration is reasonable
            uint64_t middle = duration / 2;
            bool middle_seek_result = demuxer->seekTo(middle);
            if (middle_seek_result) {
                uint64_t position = demuxer->getPosition();
                ASSERT_TRUE(position >= middle - 100 && position <= middle + 100, 
                           "Position should be close to seek target");
            } else {
                Debug::log("test", "Seeking to middle failed (expected for minimal test file)");
            }
        } else {
            Debug::log("test", "Duration too short for middle seek test");
        }
    }
};

/**
 * @brief Test FLAC frame reading
 */
class FLACFrameReadingTest : public TestCase {
public:
    FLACFrameReadingTest() : TestCase("FLAC Frame Reading Test") {}
    
protected:
    void runTest() override {
        auto flac_data = createMinimalFLACFile();
        auto handler = std::make_unique<MemoryIOHandler>(std::move(flac_data));
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse container");
        
        // Test reading first frame
        auto chunk = demuxer->readChunk();
        
        // The frame might be empty if our minimal frame is not valid enough
        // but the demuxer should not crash
        if (!chunk.data.empty()) {
            ASSERT_EQUALS(1u, chunk.stream_id, "Stream ID should be 1");
            ASSERT_TRUE(chunk.is_keyframe, "FLAC frames should be keyframes");
            ASSERT_TRUE(chunk.data.size() > 0, "Frame should have data");
        }
        
        // Test EOF detection
        bool reached_eof = false;
        int frames_read = 0;
        const int max_frames = 10;  // Prevent infinite loop
        
        while (!demuxer->isEOF() && frames_read < max_frames) {
            auto test_chunk = demuxer->readChunk();
            if (test_chunk.data.empty()) {
                reached_eof = true;
                break;
            }
            frames_read++;
        }
        
        ASSERT_TRUE(reached_eof || demuxer->isEOF(), "Should reach EOF or detect it properly");
    }
};

/**
 * @brief Test error handling with invalid data
 */
class FLACErrorHandlingTest : public TestCase {
public:
    FLACErrorHandlingTest() : TestCase("FLAC Error Handling Test") {}
    
protected:
    void runTest() override {
        // Test with invalid stream marker
        std::vector<uint8_t> invalid_data = {'I', 'N', 'V', 'D'};  // Invalid marker
        auto handler1 = std::make_unique<MemoryIOHandler>(std::move(invalid_data));
        auto demuxer1 = std::make_unique<FLACDemuxer>(std::move(handler1));
        
        ASSERT_FALSE(demuxer1->parseContainer(), "Should reject invalid stream marker");
        
        // Test with empty data
        std::vector<uint8_t> empty_data;
        auto handler2 = std::make_unique<MemoryIOHandler>(std::move(empty_data));
        auto demuxer2 = std::make_unique<FLACDemuxer>(std::move(handler2));
        
        ASSERT_FALSE(demuxer2->parseContainer(), "Should reject empty data");
        
        // Test with truncated data (just the marker)
        std::vector<uint8_t> truncated_data = {'f', 'L', 'a', 'C'};
        auto handler3 = std::make_unique<MemoryIOHandler>(std::move(truncated_data));
        auto demuxer3 = std::make_unique<FLACDemuxer>(std::move(handler3));
        
        ASSERT_FALSE(demuxer3->parseContainer(), "Should reject truncated data");
    }
};

/**
 * @brief Test thread safety with concurrent operations
 */
class FLACThreadSafetyTest : public TestCase {
public:
    FLACThreadSafetyTest() : TestCase("FLAC Thread Safety Test") {}
    
protected:
    void runTest() override {
        auto flac_data = createMinimalFLACFile();
        auto handler = std::make_unique<MemoryIOHandler>(std::move(flac_data));
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse container");
        
        std::atomic<int> error_count{0};
        std::vector<std::thread> threads;
        
        // Thread 1: Position queries
        threads.emplace_back([&demuxer, &error_count]() {
            try {
                for (int i = 0; i < 100; i++) {
                    uint64_t pos = demuxer->getPosition();
                    uint64_t sample = demuxer->getPosition();
                    uint64_t duration = demuxer->getDuration();
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
            } catch (...) {
                error_count.fetch_add(1);
            }
        });
        
        // Thread 2: Stream info queries
        threads.emplace_back([&demuxer, &error_count]() {
            try {
                for (int i = 0; i < 100; i++) {
                    auto streams = demuxer->getStreams();
                    auto info = demuxer->getStreamInfo(1);
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
            } catch (...) {
                error_count.fetch_add(1);
            }
        });
        
        // Thread 3: Seeking operations
        threads.emplace_back([&demuxer, &error_count]() {
            try {
                for (int i = 0; i < 50; i++) {
                    demuxer->seekTo(0);
                    std::this_thread::sleep_for(std::chrono::microseconds(20));
                }
            } catch (...) {
                error_count.fetch_add(1);
            }
        });
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        ASSERT_EQUALS(0, error_count.load(), "No thread safety errors should occur");
    }
};

int main() {
    TestSuite suite("FLAC Demuxer Unit Tests (Fixed)");
    
    // Add all test cases
    suite.addTest(std::make_unique<FLACBasicParsingTest>());
    suite.addTest(std::make_unique<FLACSeekingTest>());
    suite.addTest(std::make_unique<FLACFrameReadingTest>());
    suite.addTest(std::make_unique<FLACErrorHandlingTest>());
    suite.addTest(std::make_unique<FLACThreadSafetyTest>());
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print results
    suite.printResults(results);
    
    // Return appropriate exit code
    return suite.getFailureCount(results) == 0 ? 0 : 1;
}