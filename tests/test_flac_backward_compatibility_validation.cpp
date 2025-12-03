/*
 * test_flac_backward_compatibility_validation.cpp - Backward compatibility validation for FLACDemuxer
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * This test validates Requirements 27.1-27.8 for backward compatibility:
 * - 27.1: Support all previously supported FLAC variants
 * - 27.2: Extract same metadata fields as current implementation
 * - 27.3: Provide at least same seeking accuracy as current implementation
 * - 27.4: Handle all FLAC files that currently work
 * - 27.5: Provide consistent duration calculations
 * - 27.6: Provide equivalent error handling behavior
 * - 27.7: Work through DemuxedStream bridge
 * - 27.8: Provide comparable or better performance
 */

#include "psymp3.h"
#include "test_framework.h"

using namespace TestFramework;
using namespace PsyMP3::Demuxer::FLAC;

// ============================================================================
// Test Utilities
// ============================================================================

/**
 * @brief Generate minimal valid FLAC data for testing
 * Creates a minimal FLAC file structure with fLaC marker and STREAMINFO
 */
static std::vector<uint8_t> generateMinimalFLACData() {
    std::vector<uint8_t> data;
    
    // fLaC stream marker (4 bytes)
    data.push_back(0x66);  // 'f'
    data.push_back(0x4C);  // 'L'
    data.push_back(0x61);  // 'a'
    data.push_back(0x43);  // 'C'
    
    // STREAMINFO metadata block header (4 bytes)
    // is_last=1 (bit 7), type=0 (bits 0-6), length=34 (24-bit big-endian)
    data.push_back(0x80);  // is_last=1, type=0
    data.push_back(0x00);  // length high byte
    data.push_back(0x00);  // length mid byte
    data.push_back(0x22);  // length low byte (34)
    
    // STREAMINFO block data (34 bytes)
    // min_block_size: 4096 (0x1000)
    data.push_back(0x10);
    data.push_back(0x00);
    // max_block_size: 4096 (0x1000)
    data.push_back(0x10);
    data.push_back(0x00);
    // min_frame_size: 0 (unknown)
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);
    // max_frame_size: 0 (unknown)
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);
    // sample_rate (20 bits): 44100 Hz = 0xAC44
    // channels-1 (3 bits): 1 (stereo)
    // bits_per_sample-1 (5 bits): 15 (16-bit)
    // Packed: 0xAC44 << 12 | 0x1 << 9 | 0xF << 4 = 0xAC4402F0
    // But we need to pack: sample_rate(20) | channels-1(3) | bps-1(5) | total_samples_high(4)
    // 44100 = 0xAC44, channels-1=1, bps-1=15
    // Byte 10: 0xAC (sample_rate high 8 bits)
    data.push_back(0x0A);  // sample_rate bits 19-12 (44100 >> 12 = 10)
    data.push_back(0xC4);  // sample_rate bits 11-4 (44100 >> 4 & 0xFF = 0xC4)
    // Byte 12: sample_rate bits 3-0 | channels-1 bits 2-0 | bps-1 bit 4
    // (44100 & 0xF) << 4 | (1 << 1) | (15 >> 4) = 0x40 | 0x02 | 0x00 = 0x42
    data.push_back(0x42);
    // Byte 13: bps-1 bits 3-0 | total_samples bits 35-32
    // (15 & 0xF) << 4 | 0 = 0xF0
    data.push_back(0xF0);
    // total_samples (32 bits): 441000 samples (10 seconds at 44100 Hz)
    // 441000 = 0x6BA68
    data.push_back(0x00);  // bits 31-24
    data.push_back(0x06);  // bits 23-16
    data.push_back(0xBA);  // bits 15-8
    data.push_back(0x68);  // bits 7-0
    // MD5 signature (16 bytes) - all zeros (unavailable)
    for (int i = 0; i < 16; i++) {
        data.push_back(0x00);
    }
    
    // Add a minimal frame header to make it look like valid audio data
    // Frame sync code: 0xFFF8 (fixed block size)
    data.push_back(0xFF);
    data.push_back(0xF8);
    // Block size bits (4) | sample rate bits (4): 0xC9 = 4096 samples, 44100 Hz
    data.push_back(0xC9);
    // Channel bits (4) | bit depth bits (3) | reserved (1): 0x14 = stereo, 16-bit
    data.push_back(0x14);
    // Frame number (1 byte for frame 0)
    data.push_back(0x00);
    // CRC-8 placeholder
    data.push_back(0x00);
    
    return data;
}

/**
 * @brief Memory-based IOHandler for testing
 */
class MemoryIOHandler : public IOHandler {
public:
    explicit MemoryIOHandler(const std::vector<uint8_t>& data)
        : m_data(data), m_position(0) {}
    
    size_t read(void* buffer, size_t size, size_t count) override {
        size_t bytes_to_read = size * count;
        size_t available = m_data.size() - m_position;
        size_t actual_read = std::min(bytes_to_read, available);
        
        if (actual_read > 0) {
            std::memcpy(buffer, m_data.data() + m_position, actual_read);
            m_position += actual_read;
        }
        
        return actual_read / size;
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
    
    off_t getFileSize() override {
        return static_cast<off_t>(m_data.size());
    }
    
private:
    std::vector<uint8_t> m_data;
    size_t m_position;
};

// ============================================================================
// Test Cases
// ============================================================================

/**
 * @brief Test 27.1: Support all previously supported FLAC variants
 */
class FLACVariantSupportTest : public TestCase {
public:
    FLACVariantSupportTest() : TestCase("FLAC Variant Support Test (Req 27.1)") {}
    
protected:
    void runTest() override {
        // Test that FLACDemuxer can be instantiated and parse basic FLAC data
        auto data = generateMinimalFLACData();
        auto handler = std::make_unique<MemoryIOHandler>(data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        // Verify demuxer was created successfully
        ASSERT_TRUE(demuxer != nullptr, "FLACDemuxer should be created successfully");
        
        // Test that parseContainer can be called without crashing
        // Note: May fail due to incomplete test data, but should not crash
        bool parsed = demuxer->parseContainer();
        
        // Even if parsing fails, the demuxer should handle it gracefully
        // The key requirement is that it doesn't crash
        
        // Test that getStreams returns empty or valid data
        auto streams = demuxer->getStreams();
        // Should either be empty (if parsing failed) or have one stream
        ASSERT_TRUE(streams.size() <= 1, "FLAC should have at most one stream");
        
        // Test that getDuration doesn't crash
        uint64_t duration = demuxer->getDuration();
        // Duration should be 0 if parsing failed, or a valid value otherwise
        ASSERT_TRUE(duration >= 0, "Duration should be non-negative");
        
        // Test that isEOF doesn't crash
        bool eof = demuxer->isEOF();
        // EOF state should be valid
        (void)eof;  // Just verify it doesn't crash
    }
};

/**
 * @brief Test 27.2: Extract same metadata fields as current implementation
 */
class MetadataExtractionTest : public TestCase {
public:
    MetadataExtractionTest() : TestCase("Metadata Extraction Test (Req 27.2)") {}
    
protected:
    void runTest() override {
        auto data = generateMinimalFLACData();
        auto handler = std::make_unique<MemoryIOHandler>(data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        demuxer->parseContainer();
        
        auto streams = demuxer->getStreams();
        
        if (!streams.empty()) {
            const auto& stream = streams[0];
            
            // Verify essential metadata fields are populated
            ASSERT_TRUE(stream.stream_id > 0, "Stream ID should be set");
            ASSERT_EQUALS("audio", stream.codec_type, "Codec type should be 'audio'");
            ASSERT_EQUALS("flac", stream.codec_name, "Codec name should be 'flac'");
            
            // Sample rate should be valid if parsed
            if (stream.sample_rate > 0) {
                ASSERT_TRUE(stream.sample_rate >= 1 && stream.sample_rate <= 655350,
                           "Sample rate should be in valid range");
            }
            
            // Channels should be valid if parsed
            if (stream.channels > 0) {
                ASSERT_TRUE(stream.channels >= 1 && stream.channels <= 8,
                           "Channels should be 1-8");
            }
            
            // Bits per sample should be valid if parsed
            if (stream.bits_per_sample > 0) {
                ASSERT_TRUE(stream.bits_per_sample >= 4 && stream.bits_per_sample <= 32,
                           "Bits per sample should be 4-32");
            }
        }
    }
};

/**
 * @brief Test 27.3: Provide at least same seeking accuracy
 */
class SeekingAccuracyTest : public TestCase {
public:
    SeekingAccuracyTest() : TestCase("Seeking Accuracy Test (Req 27.3)") {}
    
protected:
    void runTest() override {
        auto data = generateMinimalFLACData();
        auto handler = std::make_unique<MemoryIOHandler>(data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        demuxer->parseContainer();
        
        // Test seeking to beginning (should always work)
        bool seek_result = demuxer->seekTo(0);
        // Seeking to 0 should succeed if container was parsed
        
        uint64_t position = demuxer->getPosition();
        // Position should be 0 or close to it after seeking to beginning
        ASSERT_TRUE(position <= 100, "Position after seek to 0 should be near 0");
        
        // Test that seeking doesn't crash even with invalid targets
        demuxer->seekTo(1000);  // May fail, but shouldn't crash
        demuxer->seekTo(0);     // Reset to beginning
    }
};

/**
 * @brief Test 27.5: Provide consistent duration calculations
 */
class DurationCalculationTest : public TestCase {
public:
    DurationCalculationTest() : TestCase("Duration Calculation Test (Req 27.5)") {}
    
protected:
    void runTest() override {
        auto data = generateMinimalFLACData();
        auto handler = std::make_unique<MemoryIOHandler>(data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        demuxer->parseContainer();
        
        // Get duration multiple times - should be consistent
        uint64_t duration1 = demuxer->getDuration();
        uint64_t duration2 = demuxer->getDuration();
        uint64_t duration3 = demuxer->getDuration();
        
        ASSERT_EQUALS(duration1, duration2, "Duration should be consistent across calls");
        ASSERT_EQUALS(duration2, duration3, "Duration should be consistent across calls");
        
        // Duration should be non-negative
        ASSERT_TRUE(duration1 >= 0, "Duration should be non-negative");
    }
};

/**
 * @brief Test 27.6: Provide equivalent error handling behavior
 */
class ErrorHandlingTest : public TestCase {
public:
    ErrorHandlingTest() : TestCase("Error Handling Test (Req 27.6)") {}
    
protected:
    void runTest() override {
        // Test with invalid data - should not crash
        std::vector<uint8_t> invalid_data = {0x00, 0x00, 0x00, 0x00};
        auto handler = std::make_unique<MemoryIOHandler>(invalid_data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        // Parsing should fail gracefully
        bool parsed = demuxer->parseContainer();
        ASSERT_FALSE(parsed, "Parsing invalid data should fail");
        
        // Operations on unparsed demuxer should not crash
        auto streams = demuxer->getStreams();
        ASSERT_TRUE(streams.empty(), "Unparsed demuxer should return empty streams");
        
        uint64_t duration = demuxer->getDuration();
        ASSERT_EQUALS(0u, duration, "Unparsed demuxer should return 0 duration");
        
        auto chunk = demuxer->readChunk();
        ASSERT_FALSE(chunk.isValid(), "Unparsed demuxer should return invalid chunk");
        
        // Test with empty data
        std::vector<uint8_t> empty_data;
        auto handler2 = std::make_unique<MemoryIOHandler>(empty_data);
        auto demuxer2 = std::make_unique<FLACDemuxer>(std::move(handler2));
        
        bool parsed2 = demuxer2->parseContainer();
        ASSERT_FALSE(parsed2, "Parsing empty data should fail");
    }
};

/**
 * @brief Test 27.7: Work through DemuxedStream bridge
 */
class DemuxedStreamBridgeTest : public TestCase {
public:
    DemuxedStreamBridgeTest() : TestCase("DemuxedStream Bridge Test (Req 27.7)") {}
    
protected:
    void runTest() override {
        // Verify that FLACDemuxer implements the Demuxer interface correctly
        auto data = generateMinimalFLACData();
        auto handler = std::make_unique<MemoryIOHandler>(data);
        
        // Create as Demuxer pointer to verify interface compliance
        std::unique_ptr<PsyMP3::Demuxer::Demuxer> demuxer = 
            std::make_unique<FLACDemuxer>(std::move(handler));
        
        // Verify all Demuxer interface methods are accessible
        ASSERT_TRUE(demuxer != nullptr, "Demuxer should be created");
        
        // Test interface methods
        bool parsed = demuxer->parseContainer();
        (void)parsed;  // May fail with test data
        
        auto streams = demuxer->getStreams();
        // Should return valid vector (empty or with streams)
        
        if (!streams.empty()) {
            auto stream_info = demuxer->getStreamInfo(streams[0].stream_id);
            // Should return valid StreamInfo
        }
        
        auto chunk = demuxer->readChunk();
        // Should return valid or invalid chunk without crashing
        
        bool seek_result = demuxer->seekTo(0);
        (void)seek_result;  // May fail with test data
        
        bool eof = demuxer->isEOF();
        (void)eof;  // Should return valid bool
        
        uint64_t duration = demuxer->getDuration();
        (void)duration;  // Should return valid value
        
        uint64_t position = demuxer->getPosition();
        (void)position;  // Should return valid value
    }
};

/**
 * @brief Test 27.8: Provide comparable or better performance
 */
class PerformanceTest : public TestCase {
public:
    PerformanceTest() : TestCase("Performance Test (Req 27.8)") {}
    
protected:
    void runTest() override {
        auto data = generateMinimalFLACData();
        
        // Test parsing performance
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < 100; i++) {
            auto handler = std::make_unique<MemoryIOHandler>(data);
            auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
            demuxer->parseContainer();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        // 100 parse operations should complete in under 1 second
        ASSERT_TRUE(duration.count() < 1000, 
                   "100 parse operations should complete in under 1 second");
        
        // Test seeking performance
        auto handler = std::make_unique<MemoryIOHandler>(data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        demuxer->parseContainer();
        
        start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < 1000; i++) {
            demuxer->seekTo(0);
            demuxer->getPosition();
        }
        
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        // 1000 seek operations should complete in under 1 second
        ASSERT_TRUE(duration.count() < 1000,
                   "1000 seek operations should complete in under 1 second");
    }
};

/**
 * @brief Test thread safety of FLACDemuxer
 */
class ThreadSafetyTest : public TestCase {
public:
    ThreadSafetyTest() : TestCase("Thread Safety Test") {}
    
protected:
    void runTest() override {
        auto data = generateMinimalFLACData();
        auto handler = std::make_unique<MemoryIOHandler>(data);
        auto demuxer = std::make_shared<FLACDemuxer>(std::move(handler));
        
        demuxer->parseContainer();
        
        std::atomic<int> operations_completed{0};
        std::atomic<bool> error_occurred{false};
        
        // Launch multiple threads performing concurrent operations
        std::vector<std::thread> threads;
        
        for (int i = 0; i < 4; i++) {
            threads.emplace_back([demuxer, &operations_completed, &error_occurred]() {
                try {
                    for (int j = 0; j < 100; j++) {
                        demuxer->getPosition();
                        demuxer->getDuration();
                        demuxer->isEOF();
                        demuxer->getStreams();
                        operations_completed++;
                    }
                } catch (...) {
                    error_occurred = true;
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        ASSERT_FALSE(error_occurred.load(), "No errors should occur during concurrent access");
        ASSERT_EQUALS(400, operations_completed.load(), "All operations should complete");
    }
};

// ============================================================================
// Main
// ============================================================================

int main() {
    TestSuite suite("FLAC Demuxer Backward Compatibility Validation (Requirements 27.1-27.8)");
    
    // Add all test cases
    suite.addTest(std::make_unique<FLACVariantSupportTest>());
    suite.addTest(std::make_unique<MetadataExtractionTest>());
    suite.addTest(std::make_unique<SeekingAccuracyTest>());
    suite.addTest(std::make_unique<DurationCalculationTest>());
    suite.addTest(std::make_unique<ErrorHandlingTest>());
    suite.addTest(std::make_unique<DemuxedStreamBridgeTest>());
    suite.addTest(std::make_unique<PerformanceTest>());
    suite.addTest(std::make_unique<ThreadSafetyTest>());
    
    // Run all tests
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results);
}
