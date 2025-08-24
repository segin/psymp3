/*
 * test_flac_demuxer_integration_comprehensive.cpp - Comprehensive integration tests for FLACDemuxer
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
#include <chrono>
#include <thread>
#include <atomic>
#include <random>

using namespace TestFramework;

// Test file path - using the provided test file
const char* TEST_FLAC_FILE = "/mnt/8TB-3/music/almost monday/DIVE/11 life goes by.flac";

/**
 * @brief Helper to check if test file exists
 */
bool checkTestFileExists() {
    std::ifstream file(TEST_FLAC_FILE);
    return file.good();
}

/**
 * @brief Performance measurement helper
 */
class PerformanceMeasurement {
private:
    std::chrono::high_resolution_clock::time_point m_start;
    std::string m_operation;
    
public:
    explicit PerformanceMeasurement(const std::string& operation) 
        : m_operation(operation) {
        m_start = std::chrono::high_resolution_clock::now();
    }
    
    ~PerformanceMeasurement() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - m_start);
        Debug::log("performance", "%s took %lld ms", m_operation.c_str(), duration.count());
    }
    
    uint64_t getElapsedMs() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start).count();
    }
};

/**
 * @brief Test real FLAC file parsing and basic functionality
 */
class FLACRealFileIntegrationTest : public TestCase {
public:
    FLACRealFileIntegrationTest() : TestCase("FLAC Real File Integration Test") {}
    
protected:
    void runTest() override {
        if (!checkTestFileExists()) {
            Debug::log("test", "Test file not found, skipping real file test");
            return;
        }
        
        PerformanceMeasurement perf("Real FLAC file parsing");
        
        auto handler = std::make_unique<FileIOHandler>(TEST_FLAC_FILE);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        // Test container parsing
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse real FLAC file successfully");
        
        // Test stream information
        auto streams = demuxer->getStreams();
        ASSERT_EQUALS(1u, streams.size(), "Should have exactly one stream");
        
        const auto& stream = streams[0];
        ASSERT_TRUE(stream.sample_rate > 0, "Sample rate should be valid");
        ASSERT_TRUE(stream.channels > 0 && stream.channels <= 8, "Channel count should be valid");
        ASSERT_TRUE(stream.bits_per_sample >= 4 && stream.bits_per_sample <= 32, "Bit depth should be valid");
        
        // Test duration
        uint64_t duration = demuxer->getDuration();
        ASSERT_TRUE(duration > 0, "Duration should be positive");
        ASSERT_TRUE(duration < 10 * 60 * 1000, "Duration should be reasonable (less than 10 minutes)");
        
        Debug::log("test", "Real FLAC file info: %u Hz, %u channels, %u bits, %llu ms duration",
                  stream.sample_rate, stream.channels, stream.bits_per_sample, duration);
    }
};

/**
 * @brief Test FLAC seeking performance and accuracy
 */
class FLACSeekingPerformanceTest : public TestCase {
public:
    FLACSeekingPerformanceTest() : TestCase("FLAC Seeking Performance Test") {}
    
protected:
    void runTest() override {
        if (!checkTestFileExists()) {
            Debug::log("test", "Test file not found, skipping seeking performance test");
            return;
        }
        
        auto handler = std::make_unique<FileIOHandler>(TEST_FLAC_FILE);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse container successfully");
        
        uint64_t duration = demuxer->getDuration();
        ASSERT_TRUE(duration > 0, "Duration should be positive");
        
        // Test seeking to various positions
        std::vector<uint64_t> seek_positions = {
            0,                    // Beginning
            duration / 4,         // 25%
            duration / 2,         // 50%
            duration * 3 / 4,     // 75%
            duration - 1000       // Near end
        };
        
        for (uint64_t pos : seek_positions) {
            if (pos >= duration) continue;
            
            PerformanceMeasurement perf("Seek to " + std::to_string(pos) + "ms");
            
            ASSERT_TRUE(demuxer->seekTo(pos), "Should seek to position " + std::to_string(pos));
            
            uint64_t actual_pos = demuxer->getPosition();
            uint64_t tolerance = std::max(static_cast<uint64_t>(1000), duration / 100); // 1 second or 1% of duration
            
            ASSERT_TRUE(actual_pos >= pos - tolerance && actual_pos <= pos + tolerance,
                       "Seek accuracy should be within tolerance");
            
            // Verify we can read after seeking
            auto chunk = demuxer->readChunk();
            ASSERT_TRUE(!chunk.data.empty() || demuxer->isEOF(), "Should be able to read after seeking");
            
            uint64_t seek_time = perf.getElapsedMs();
            ASSERT_TRUE(seek_time < 1000, "Seek should complete within 1 second");
        }
    }
};

/**
 * @brief Test frame reading performance and data integrity
 */
class FLACFrameReadingTest : public TestCase {
public:
    FLACFrameReadingTest() : TestCase("FLAC Frame Reading Test") {}
    
protected:
    void runTest() override {
        if (!checkTestFileExists()) {
            Debug::log("test", "Test file not found, skipping frame reading test");
            return;
        }
        
        auto handler = std::make_unique<FileIOHandler>(TEST_FLAC_FILE);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse container successfully");
        
        PerformanceMeasurement perf("Reading first 100 frames");
        
        int frames_read = 0;
        uint64_t total_data = 0;
        uint64_t last_timestamp = 0;
        
        for (int i = 0; i < 100; i++) {
            auto chunk = demuxer->readChunk();
            if (chunk.data.empty()) {
                break; // EOF reached
            }
            
            frames_read++;
            total_data += chunk.data.size();
            
            // Verify frame properties
            ASSERT_EQUALS(1u, chunk.stream_id, "Stream ID should be 1");
            ASSERT_TRUE(chunk.is_keyframe, "All FLAC frames should be keyframes");
            ASSERT_TRUE(chunk.data.size() > 0, "Frame should have data");
            ASSERT_TRUE(chunk.data.size() < 1024 * 1024, "Frame should not be excessively large");
            
            // Verify timestamp progression
            ASSERT_TRUE(chunk.timestamp_samples >= last_timestamp, "Timestamps should be non-decreasing");
            last_timestamp = chunk.timestamp_samples;
            
            // Verify FLAC frame sync pattern
            if (chunk.data.size() >= 2) {
                ASSERT_EQUALS(0xFF, chunk.data[0], "Frame should start with sync code");
                ASSERT_TRUE((chunk.data[1] & 0xFC) == 0xF8, "Frame should have valid sync pattern");
            }
        }
        
        ASSERT_TRUE(frames_read > 0, "Should read at least one frame");
        ASSERT_TRUE(total_data > 0, "Should read some data");
        
        uint64_t read_time = perf.getElapsedMs();
        if (frames_read > 0) {
            double frames_per_second = (frames_read * 1000.0) / read_time;
            Debug::log("test", "Read %d frames in %llu ms (%.2f frames/sec, %llu bytes total)",
                      frames_read, read_time, frames_per_second, total_data);
            
            ASSERT_TRUE(frames_per_second > 10, "Should read at least 10 frames per second");
        }
    }
};

/**
 * @brief Test IOHandler integration with different handler types
 */
class FLACIOHandlerIntegrationTest : public TestCase {
public:
    FLACIOHandlerIntegrationTest() : TestCase("FLAC IOHandler Integration Test") {}
    
protected:
    void runTest() override {
        if (!checkTestFileExists()) {
            Debug::log("test", "Test file not found, skipping IOHandler integration test");
            return;
        }
        
        testFileIOHandler();
        // Note: HTTPIOHandler test would require a network FLAC file
    }
    
private:
    void testFileIOHandler() {
        auto handler = std::make_unique<FileIOHandler>(TEST_FLAC_FILE);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should work with FileIOHandler");
        
        // Test basic operations
        auto streams = demuxer->getStreams();
        ASSERT_EQUALS(1u, streams.size(), "Should have one stream");
        
        uint64_t duration = demuxer->getDuration();
        ASSERT_TRUE(duration > 0, "Should have valid duration");
        
        // Test seeking
        ASSERT_TRUE(demuxer->seekTo(duration / 2), "Should seek to middle");
        
        // Test reading
        auto chunk = demuxer->readChunk();
        ASSERT_TRUE(!chunk.data.empty() || demuxer->isEOF(), "Should read data or be at EOF");
    }
};

/**
 * @brief Test memory usage and resource management
 */
class FLACMemoryUsageTest : public TestCase {
public:
    FLACMemoryUsageTest() : TestCase("FLAC Memory Usage Test") {}
    
protected:
    void runTest() override {
        if (!checkTestFileExists()) {
            Debug::log("test", "Test file not found, skipping memory usage test");
            return;
        }
        
        testMemoryUsageStability();
        testLargeFileHandling();
    }
    
private:
    void testMemoryUsageStability() {
        auto handler = std::make_unique<FileIOHandler>(TEST_FLAC_FILE);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse container successfully");
        
        // Read many frames to test memory stability
        int frames_read = 0;
        for (int i = 0; i < 1000; i++) {
            auto chunk = demuxer->readChunk();
            if (chunk.data.empty()) {
                break;
            }
            frames_read++;
            
            // Periodically seek to test memory management during seeking
            if (i % 100 == 0) {
                uint64_t duration = demuxer->getDuration();
                uint64_t seek_pos = (duration * i) / 1000;
                demuxer->seekTo(seek_pos);
            }
        }
        
        ASSERT_TRUE(frames_read > 0, "Should read frames successfully");
        Debug::log("test", "Read %d frames without memory issues", frames_read);
    }
    
    void testLargeFileHandling() {
        // Test with the real file (which should be reasonably large)
        auto handler = std::make_unique<FileIOHandler>(TEST_FLAC_FILE);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should handle large file parsing");
        
        uint64_t duration = demuxer->getDuration();
        
        // Test seeking to many random positions
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint64_t> dis(0, duration - 1000);
        
        for (int i = 0; i < 50; i++) {
            uint64_t seek_pos = dis(gen);
            ASSERT_TRUE(demuxer->seekTo(seek_pos), "Should seek to random position");
            
            auto chunk = demuxer->readChunk();
            // Should either read data or be at EOF
            ASSERT_TRUE(!chunk.data.empty() || demuxer->isEOF(), "Should handle random seeks");
        }
    }
};

/**
 * @brief Test compatibility with existing FLAC playback
 */
class FLACCompatibilityTest : public TestCase {
public:
    FLACCompatibilityTest() : TestCase("FLAC Compatibility Test") {}
    
protected:
    void runTest() override {
        if (!checkTestFileExists()) {
            Debug::log("test", "Test file not found, skipping compatibility test");
            return;
        }
        
        testMetadataCompatibility();
        testSeekingCompatibility();
        testFrameDataCompatibility();
    }
    
private:
    void testMetadataCompatibility() {
        auto handler = std::make_unique<FileIOHandler>(TEST_FLAC_FILE);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse container successfully");
        
        auto streams = demuxer->getStreams();
        ASSERT_EQUALS(1u, streams.size(), "Should have exactly one stream");
        
        const auto& stream = streams[0];
        
        // Verify metadata fields are populated
        ASSERT_TRUE(stream.sample_rate > 0, "Sample rate should be valid");
        ASSERT_TRUE(stream.channels > 0, "Channel count should be valid");
        ASSERT_TRUE(stream.bits_per_sample > 0, "Bit depth should be valid");
        
        // Check for common metadata fields
        if (!stream.title.empty()) {
            Debug::log("test", "Title: %s", stream.title.c_str());
        }
        if (!stream.artist.empty()) {
            Debug::log("test", "Artist: %s", stream.artist.c_str());
        }
        if (!stream.album.empty()) {
            Debug::log("test", "Album: %s", stream.album.c_str());
        }
        
        uint64_t duration = demuxer->getDuration();
        ASSERT_TRUE(duration > 0, "Duration should be calculated");
        
        Debug::log("test", "FLAC metadata: %u Hz, %u ch, %u bits, %llu ms",
                  stream.sample_rate, stream.channels, stream.bits_per_sample, duration);
    }
    
    void testSeekingCompatibility() {
        auto handler = std::make_unique<FileIOHandler>(TEST_FLAC_FILE);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse container successfully");
        
        uint64_t duration = demuxer->getDuration();
        
        // Test seeking to standard positions
        std::vector<double> positions = {0.0, 0.1, 0.25, 0.5, 0.75, 0.9};
        
        for (double pos : positions) {
            uint64_t seek_time = static_cast<uint64_t>(duration * pos);
            if (seek_time >= duration) seek_time = duration - 1000;
            
            ASSERT_TRUE(demuxer->seekTo(seek_time), "Should seek to " + std::to_string(pos * 100) + "%");
            
            uint64_t actual_pos = demuxer->getPosition();
            uint64_t tolerance = std::max(static_cast<uint64_t>(2000), duration / 50); // 2 seconds or 2% tolerance
            
            ASSERT_TRUE(actual_pos >= seek_time - tolerance && actual_pos <= seek_time + tolerance,
                       "Seek position should be reasonably accurate");
        }
    }
    
    void testFrameDataCompatibility() {
        auto handler = std::make_unique<FileIOHandler>(TEST_FLAC_FILE);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse container successfully");
        
        // Read several frames and verify they have valid FLAC structure
        for (int i = 0; i < 10; i++) {
            auto chunk = demuxer->readChunk();
            if (chunk.data.empty()) {
                break;
            }
            
            // Verify frame structure
            ASSERT_TRUE(chunk.data.size() >= 6, "Frame should have minimum size");
            ASSERT_EQUALS(0xFF, chunk.data[0], "Frame should start with sync code");
            ASSERT_TRUE((chunk.data[1] & 0xFC) == 0xF8, "Frame should have valid sync pattern");
            
            // Verify frame properties
            ASSERT_EQUALS(1u, chunk.stream_id, "Stream ID should be 1");
            ASSERT_TRUE(chunk.is_keyframe, "All FLAC frames should be keyframes");
            ASSERT_TRUE(chunk.timestamp_samples < UINT64_MAX, "Timestamp should be valid");
        }
    }
};

/**
 * @brief Test concurrent access and thread safety
 */
class FLACConcurrencyTest : public TestCase {
public:
    FLACConcurrencyTest() : TestCase("FLAC Concurrency Test") {}
    
protected:
    void runTest() override {
        if (!checkTestFileExists()) {
            Debug::log("test", "Test file not found, skipping concurrency test");
            return;
        }
        
        testConcurrentSeekingAndReading();
        testMultipleReaders();
    }
    
private:
    void testConcurrentSeekingAndReading() {
        auto handler = std::make_unique<FileIOHandler>(TEST_FLAC_FILE);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse container successfully");
        
        uint64_t duration = demuxer->getDuration();
        std::atomic<bool> error_occurred{false};
        std::atomic<int> operations_completed{0};
        
        // Start seeking thread
        std::thread seeker([&demuxer, &error_occurred, &operations_completed, duration]() {
            try {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<uint64_t> dis(0, duration - 1000);
                
                for (int i = 0; i < 20; i++) {
                    uint64_t seek_pos = dis(gen);
                    if (demuxer->seekTo(seek_pos)) {
                        operations_completed++;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            } catch (...) {
                error_occurred = true;
            }
        });
        
        // Start reading thread
        std::thread reader([&demuxer, &error_occurred, &operations_completed]() {
            try {
                for (int i = 0; i < 50; i++) {
                    auto chunk = demuxer->readChunk();
                    if (!chunk.data.empty()) {
                        operations_completed++;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            } catch (...) {
                error_occurred = true;
            }
        });
        
        // Wait for threads to complete
        seeker.join();
        reader.join();
        
        ASSERT_FALSE(error_occurred.load(), "No errors should occur during concurrent access");
        ASSERT_TRUE(operations_completed.load() > 0, "Should complete some operations");
        
        Debug::log("test", "Completed %d concurrent operations", operations_completed.load());
    }
    
    void testMultipleReaders() {
        // Test multiple demuxer instances on the same file
        std::vector<std::unique_ptr<FLACDemuxer>> demuxers;
        
        for (int i = 0; i < 3; i++) {
            auto handler = std::make_unique<FileIOHandler>(TEST_FLAC_FILE);
            auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
            ASSERT_TRUE(demuxer->parseContainer(), "Should parse container for instance " + std::to_string(i));
            demuxers.push_back(std::move(demuxer));
        }
        
        // Verify all instances work independently
        for (size_t i = 0; i < demuxers.size(); i++) {
            auto& demuxer = demuxers[i];
            
            uint64_t duration = demuxer->getDuration();
            ASSERT_TRUE(duration > 0, "Duration should be valid for instance " + std::to_string(i));
            
            // Seek to different positions
            uint64_t seek_pos = (duration * i) / demuxers.size();
            ASSERT_TRUE(demuxer->seekTo(seek_pos), "Should seek for instance " + std::to_string(i));
            
            // Read a frame
            auto chunk = demuxer->readChunk();
            ASSERT_TRUE(!chunk.data.empty() || demuxer->isEOF(), "Should read for instance " + std::to_string(i));
        }
    }
};

/**
 * @brief Test performance benchmarks and regression detection
 */
class FLACPerformanceBenchmarkTest : public TestCase {
public:
    FLACPerformanceBenchmarkTest() : TestCase("FLAC Performance Benchmark Test") {}
    
protected:
    void runTest() override {
        if (!checkTestFileExists()) {
            Debug::log("test", "Test file not found, skipping performance benchmark test");
            return;
        }
        
        benchmarkParsing();
        benchmarkSeeking();
        benchmarkReading();
    }
    
private:
    void benchmarkParsing() {
        PerformanceMeasurement perf("Container parsing benchmark");
        
        auto handler = std::make_unique<FileIOHandler>(TEST_FLAC_FILE);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse container successfully");
        
        uint64_t parse_time = perf.getElapsedMs();
        ASSERT_TRUE(parse_time < 5000, "Parsing should complete within 5 seconds");
        
        Debug::log("benchmark", "Container parsing took %llu ms", parse_time);
    }
    
    void benchmarkSeeking() {
        auto handler = std::make_unique<FileIOHandler>(TEST_FLAC_FILE);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse container successfully");
        
        uint64_t duration = demuxer->getDuration();
        
        PerformanceMeasurement perf("Seeking benchmark (50 seeks)");
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint64_t> dis(0, duration - 1000);
        
        int successful_seeks = 0;
        for (int i = 0; i < 50; i++) {
            uint64_t seek_pos = dis(gen);
            if (demuxer->seekTo(seek_pos)) {
                successful_seeks++;
            }
        }
        
        uint64_t seek_time = perf.getElapsedMs();
        ASSERT_TRUE(successful_seeks > 40, "Most seeks should succeed");
        ASSERT_TRUE(seek_time < 10000, "50 seeks should complete within 10 seconds");
        
        double avg_seek_time = static_cast<double>(seek_time) / successful_seeks;
        Debug::log("benchmark", "Average seek time: %.2f ms (%d/%d successful)", 
                  avg_seek_time, successful_seeks, 50);
    }
    
    void benchmarkReading() {
        auto handler = std::make_unique<FileIOHandler>(TEST_FLAC_FILE);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse container successfully");
        
        PerformanceMeasurement perf("Frame reading benchmark (200 frames)");
        
        int frames_read = 0;
        uint64_t total_bytes = 0;
        
        for (int i = 0; i < 200; i++) {
            auto chunk = demuxer->readChunk();
            if (chunk.data.empty()) {
                break;
            }
            
            frames_read++;
            total_bytes += chunk.data.size();
        }
        
        uint64_t read_time = perf.getElapsedMs();
        ASSERT_TRUE(frames_read > 0, "Should read some frames");
        ASSERT_TRUE(read_time < 5000, "Reading should complete within 5 seconds");
        
        double frames_per_sec = (frames_read * 1000.0) / read_time;
        double mbytes_per_sec = (total_bytes * 1000.0) / (read_time * 1024 * 1024);
        
        Debug::log("benchmark", "Read %d frames, %.2f frames/sec, %.2f MB/sec", 
                  frames_read, frames_per_sec, mbytes_per_sec);
        
        ASSERT_TRUE(frames_per_sec > 50, "Should read at least 50 frames per second");
    }
};

int main() {
    TestSuite suite("FLAC Demuxer Integration and Performance Tests");
    
    // Add all integration test cases
    suite.addTest(std::make_unique<FLACRealFileIntegrationTest>());
    suite.addTest(std::make_unique<FLACSeekingPerformanceTest>());
    suite.addTest(std::make_unique<FLACFrameReadingTest>());
    suite.addTest(std::make_unique<FLACIOHandlerIntegrationTest>());
    suite.addTest(std::make_unique<FLACMemoryUsageTest>());
    suite.addTest(std::make_unique<FLACCompatibilityTest>());
    suite.addTest(std::make_unique<FLACConcurrencyTest>());
    suite.addTest(std::make_unique<FLACPerformanceBenchmarkTest>());
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print results
    suite.printResults(results);
    
    // Return appropriate exit code
    return suite.getFailureCount(results) == 0 ? 0 : 1;
}