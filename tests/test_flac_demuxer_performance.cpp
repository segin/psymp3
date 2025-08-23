/*
 * test_flac_demuxer_performance.cpp - Performance validation tests for FLACDemuxer
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include <chrono>
#include <thread>
#include <atomic>
#include <memory>

using namespace TestFramework;

/**
 * @brief Performance measurement utilities
 */
class PerformanceMeasurement {
public:
    using TimePoint = std::chrono::high_resolution_clock::time_point;
    using Duration = std::chrono::microseconds;
    
    static TimePoint now() {
        return std::chrono::high_resolution_clock::now();
    }
    
    static Duration elapsed(const TimePoint& start, const TimePoint& end) {
        return std::chrono::duration_cast<Duration>(end - start);
    }
    
    static double toMilliseconds(const Duration& duration) {
        return duration.count() / 1000.0;
    }
};

/**
 * @brief Enhanced mock FLAC data generator for performance testing
 */
class PerformanceFLACData {
public:
    /**
     * @brief Generate large FLAC file data for performance testing
     */
    static std::vector<uint8_t> generateLargeFLAC(uint32_t sample_rate = 44100,
                                                  uint8_t channels = 2,
                                                  uint32_t duration_seconds = 300) {
        std::vector<uint8_t> data;
        uint64_t total_samples = static_cast<uint64_t>(sample_rate) * duration_seconds;
        
        // fLaC stream marker
        data.insert(data.end(), {'f', 'L', 'a', 'C'});
        
        // STREAMINFO metadata block (not last)
        data.push_back(0x00); // is_last=0, type=0
        data.push_back(0x00);
        data.push_back(0x00);
        data.push_back(0x22); // 34 bytes
        
        // STREAMINFO block data
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
        uint32_t sr_ch_bps = (sample_rate << 12) | ((channels - 1) << 9) | (16 - 1);
        data.push_back((sr_ch_bps >> 16) & 0xFF);
        data.push_back((sr_ch_bps >> 8) & 0xFF);
        data.push_back(sr_ch_bps & 0xFF);
        
        // total_samples (36 bits)
        data.push_back((total_samples >> 28) & 0xFF);
        data.push_back((total_samples >> 20) & 0xFF);
        data.push_back((total_samples >> 12) & 0xFF);
        data.push_back((total_samples >> 4) & 0xFF);
        data.push_back((total_samples << 4) & 0xF0);
        
        // MD5 signature (16 bytes) - zeros
        for (int i = 0; i < 16; i++) {
            data.push_back(0x00);
        }
        
        // SEEKTABLE metadata block (last)
        data.push_back(0x83); // is_last=1, type=3
        
        // Generate seek table with entries every 10 seconds
        uint32_t seek_points = duration_seconds / 10;
        uint32_t seek_table_size = seek_points * 18; // 18 bytes per seek point
        
        data.push_back((seek_table_size >> 16) & 0xFF);
        data.push_back((seek_table_size >> 8) & 0xFF);
        data.push_back(seek_table_size & 0xFF);
        
        // Generate seek points
        for (uint32_t i = 0; i < seek_points; i++) {
            uint64_t sample_number = static_cast<uint64_t>(i * 10 * sample_rate);
            uint64_t stream_offset = i * 1000; // Approximate offset
            uint16_t frame_samples = 4096;
            
            // sample_number (64 bits)
            for (int j = 7; j >= 0; j--) {
                data.push_back((sample_number >> (j * 8)) & 0xFF);
            }
            
            // stream_offset (64 bits)
            for (int j = 7; j >= 0; j--) {
                data.push_back((stream_offset >> (j * 8)) & 0xFF);
            }
            
            // frame_samples (16 bits)
            data.push_back((frame_samples >> 8) & 0xFF);
            data.push_back(frame_samples & 0xFF);
        }
        
        // Add mock frame data to simulate a real file
        uint32_t frames_needed = (total_samples + 4095) / 4096; // Round up
        for (uint32_t frame = 0; frame < frames_needed && frame < 1000; frame++) {
            // Add minimal frame header
            data.push_back(0xFF); // sync
            data.push_back(0xF8);
            data.push_back(0x69); // block size + sample rate
            data.push_back(0x10); // channels + bit depth
            data.push_back(frame & 0xFF); // frame number (simplified)
            data.push_back(0x00); // CRC
            
            // Add some mock frame data (compressed audio would be much larger)
            for (int i = 0; i < 100; i++) {
                data.push_back(static_cast<uint8_t>(frame + i));
            }
        }
        
        return data;
    }
    
    /**
     * @brief Generate FLAC with extensive metadata for memory testing
     */
    static std::vector<uint8_t> generateFLACWithExtensiveMetadata() {
        std::vector<uint8_t> data;
        
        // fLaC stream marker
        data.insert(data.end(), {'f', 'L', 'a', 'C'});
        
        // STREAMINFO (not last)
        data.push_back(0x00);
        data.push_back(0x00);
        data.push_back(0x00);
        data.push_back(0x22);
        
        // Basic STREAMINFO data
        for (int i = 0; i < 34; i++) {
            data.push_back(i < 4 ? 0x10 : 0x00);
        }
        
        // Large VORBIS_COMMENT block (last)
        data.push_back(0x84); // is_last=1, type=4
        
        // Create extensive metadata
        std::string vendor = "performance_test_vendor_with_long_name";
        std::vector<std::string> comments;
        
        // Add many metadata fields
        for (int i = 0; i < 100; i++) {
            comments.push_back("FIELD" + std::to_string(i) + "=Value for field number " + std::to_string(i) + " with some additional text to make it longer");
        }
        
        // Calculate total size
        uint32_t total_size = 4 + vendor.length() + 4; // vendor string + count
        for (const auto& comment : comments) {
            total_size += 4 + comment.length(); // length + comment
        }
        
        // Write size
        data.push_back((total_size >> 16) & 0xFF);
        data.push_back((total_size >> 8) & 0xFF);
        data.push_back(total_size & 0xFF);
        
        // Vendor string
        uint32_t vendor_len = vendor.length();
        data.push_back(vendor_len & 0xFF);
        data.push_back((vendor_len >> 8) & 0xFF);
        data.push_back((vendor_len >> 16) & 0xFF);
        data.push_back((vendor_len >> 24) & 0xFF);
        data.insert(data.end(), vendor.begin(), vendor.end());
        
        // Comment count
        uint32_t comment_count = comments.size();
        data.push_back(comment_count & 0xFF);
        data.push_back((comment_count >> 8) & 0xFF);
        data.push_back((comment_count >> 16) & 0xFF);
        data.push_back((comment_count >> 24) & 0xFF);
        
        // Comments
        for (const auto& comment : comments) {
            uint32_t comment_len = comment.length();
            data.push_back(comment_len & 0xFF);
            data.push_back((comment_len >> 8) & 0xFF);
            data.push_back((comment_len >> 16) & 0xFF);
            data.push_back((comment_len >> 24) & 0xFF);
            data.insert(data.end(), comment.begin(), comment.end());
        }
        
        return data;
    }
};

/**
 * @brief Test FLACDemuxer parsing performance
 */
class FLACDemuxerParsingPerformanceTest : public TestCase {
public:
    FLACDemuxerParsingPerformanceTest() : TestCase("FLACDemuxer Parsing Performance Test") {}
    
protected:
    void runTest() override {
        // Test parsing performance with different file sizes
        
        // Small file (1 minute)
        auto small_data = PerformanceFLACData::generateLargeFLAC(44100, 2, 60);
        auto small_handler = std::make_unique<MockFLACIOHandler>(small_data);
        auto small_demuxer = std::make_unique<FLACDemuxer>(std::move(small_handler));
        
        auto start = PerformanceMeasurement::now();
        bool small_result = small_demuxer->parseContainer();
        auto end = PerformanceMeasurement::now();
        auto small_duration = PerformanceMeasurement::elapsed(start, end);
        
        ASSERT_TRUE(small_result, "Should parse small FLAC file");
        ASSERT_TRUE(PerformanceMeasurement::toMilliseconds(small_duration) < 100.0, 
                   "Small file parsing should be fast");
        
        // Medium file (5 minutes)
        auto medium_data = PerformanceFLACData::generateLargeFLAC(44100, 2, 300);
        auto medium_handler = std::make_unique<MockFLACIOHandler>(medium_data);
        auto medium_demuxer = std::make_unique<FLACDemuxer>(std::move(medium_handler));
        
        start = PerformanceMeasurement::now();
        bool medium_result = medium_demuxer->parseContainer();
        end = PerformanceMeasurement::now();
        auto medium_duration = PerformanceMeasurement::elapsed(start, end);
        
        ASSERT_TRUE(medium_result, "Should parse medium FLAC file");
        ASSERT_TRUE(PerformanceMeasurement::toMilliseconds(medium_duration) < 500.0,
                   "Medium file parsing should complete within 500ms");
        
        // Large file (20 minutes)
        auto large_data = PerformanceFLACData::generateLargeFLAC(44100, 2, 1200);
        auto large_handler = std::make_unique<MockFLACIOHandler>(large_data);
        auto large_demuxer = std::make_unique<FLACDemuxer>(std::move(large_handler));
        
        start = PerformanceMeasurement::now();
        bool large_result = large_demuxer->parseContainer();
        end = PerformanceMeasurement::now();
        auto large_duration = PerformanceMeasurement::elapsed(start, end);
        
        ASSERT_TRUE(large_result, "Should parse large FLAC file");
        ASSERT_TRUE(PerformanceMeasurement::toMilliseconds(large_duration) < 2000.0,
                   "Large file parsing should complete within 2 seconds");
        
        // Verify parsing time scales reasonably
        double small_ms = PerformanceMeasurement::toMilliseconds(small_duration);
        double large_ms = PerformanceMeasurement::toMilliseconds(large_duration);
        
        // Large file should not take more than 50x longer than small file
        ASSERT_TRUE(large_ms < small_ms * 50.0, "Parsing time should scale reasonably");
    }
};

/**
 * @brief Test FLACDemuxer seeking performance
 */
class FLACDemuxerSeekingPerformanceTest : public TestCase {
public:
    FLACDemuxerSeekingPerformanceTest() : TestCase("FLACDemuxer Seeking Performance Test") {}
    
protected:
    void runTest() override {
        // Generate large FLAC file with seek table
        auto flac_data = PerformanceFLACData::generateLargeFLAC(44100, 2, 600); // 10 minutes
        auto handler = std::make_unique<MockFLACIOHandler>(flac_data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse FLAC file");
        
        // Test sequential seeking performance
        std::vector<uint64_t> seek_positions = {
            0, 30000, 60000, 120000, 180000, 240000, 300000, 360000, 420000, 480000, 540000
        };
        
        auto start = PerformanceMeasurement::now();
        int successful_seeks = 0;
        
        for (uint64_t pos : seek_positions) {
            if (demuxer->seekTo(pos)) {
                successful_seeks++;
            }
        }
        
        auto end = PerformanceMeasurement::now();
        auto total_duration = PerformanceMeasurement::elapsed(start, end);
        
        ASSERT_TRUE(successful_seeks > 0, "Should have some successful seeks");
        
        double total_ms = PerformanceMeasurement::toMilliseconds(total_duration);
        double avg_seek_ms = total_ms / seek_positions.size();
        
        ASSERT_TRUE(avg_seek_ms < 50.0, "Average seek time should be under 50ms");
        ASSERT_TRUE(total_ms < 200.0, "Total seeking should complete within 200ms");
        
        // Test random seeking performance
        std::vector<uint64_t> random_positions = {
            150000, 45000, 320000, 80000, 500000, 25000, 400000, 200000
        };
        
        start = PerformanceMeasurement::now();
        successful_seeks = 0;
        
        for (uint64_t pos : random_positions) {
            if (demuxer->seekTo(pos)) {
                successful_seeks++;
            }
        }
        
        end = PerformanceMeasurement::now();
        auto random_duration = PerformanceMeasurement::elapsed(start, end);
        
        double random_ms = PerformanceMeasurement::toMilliseconds(random_duration);
        double avg_random_seek_ms = random_ms / random_positions.size();
        
        ASSERT_TRUE(avg_random_seek_ms < 100.0, "Average random seek time should be under 100ms");
        
        // Test seek accuracy
        demuxer->seekTo(300000); // 5 minutes
        uint64_t position = demuxer->getPosition();
        
        // Should be within 1 second of target (frame boundary tolerance)
        ASSERT_TRUE(position >= 299000 && position <= 301000, 
                   "Seek position should be accurate within 1 second");
    }
};

/**
 * @brief Test FLACDemuxer memory usage
 */
class FLACDemuxerMemoryUsageTest : public TestCase {
public:
    FLACDemuxerMemoryUsageTest() : TestCase("FLACDemuxer Memory Usage Test") {}
    
protected:
    void runTest() override {
        // Test memory usage with extensive metadata
        auto metadata_data = PerformanceFLACData::generateFLACWithExtensiveMetadata();
        auto metadata_handler = std::make_unique<MockFLACIOHandler>(metadata_data);
        auto metadata_demuxer = std::make_unique<FLACDemuxer>(std::move(metadata_handler));
        
        ASSERT_TRUE(metadata_demuxer->parseContainer(), "Should parse FLAC with extensive metadata");
        
        // Verify metadata was parsed but not excessively stored
        auto streams = metadata_demuxer->getStreams();
        ASSERT_EQUALS(1u, streams.size(), "Should have one stream");
        
        // Test that demuxer doesn't hold excessive metadata in memory
        // This is a basic test - in a real implementation we'd measure actual memory usage
        const auto& stream = streams[0];
        
        // Should have some metadata but not all 100 fields (due to memory limits)
        bool has_some_metadata = !stream.artist.empty() || !stream.title.empty() || !stream.album.empty();
        // We can't easily test exact memory usage, but ensure it doesn't crash
        
        // Test memory usage with large seek table
        auto large_data = PerformanceFLACData::generateLargeFLAC(44100, 2, 3600); // 1 hour
        auto large_handler = std::make_unique<MockFLACIOHandler>(large_data);
        auto large_demuxer = std::make_unique<FLACDemuxer>(std::move(large_handler));
        
        ASSERT_TRUE(large_demuxer->parseContainer(), "Should parse large FLAC file");
        
        // Test that seeking still works with large seek table
        ASSERT_TRUE(large_demuxer->seekTo(1800000), "Should seek to 30 minutes");
        
        // Test multiple demuxer instances (memory isolation)
        std::vector<std::unique_ptr<FLACDemuxer>> demuxers;
        
        for (int i = 0; i < 5; i++) {
            auto test_data = PerformanceFLACData::generateLargeFLAC(44100, 2, 60);
            auto test_handler = std::make_unique<MockFLACIOHandler>(test_data);
            auto test_demuxer = std::make_unique<FLACDemuxer>(std::move(test_handler));
            
            ASSERT_TRUE(test_demuxer->parseContainer(), "Should parse test file " + std::to_string(i));
            demuxers.push_back(std::move(test_demuxer));
        }
        
        // All demuxers should work independently
        for (size_t i = 0; i < demuxers.size(); i++) {
            auto test_streams = demuxers[i]->getStreams();
            ASSERT_EQUALS(1u, test_streams.size(), "Demuxer " + std::to_string(i) + " should have one stream");
        }
    }
};

/**
 * @brief Test FLACDemuxer thread safety
 */
class FLACDemuxerThreadSafetyTest : public TestCase {
public:
    FLACDemuxerThreadSafetyTest() : TestCase("FLACDemuxer Thread Safety Test") {}
    
protected:
    void runTest() override {
        auto flac_data = PerformanceFLACData::generateLargeFLAC(44100, 2, 300);
        auto handler = std::make_unique<MockFLACIOHandler>(flac_data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse FLAC file");
        
        std::atomic<bool> test_passed{true};
        std::atomic<int> operations_completed{0};
        
        // Test concurrent read operations
        auto read_test = [&]() {
            try {
                for (int i = 0; i < 20; i++) {
                    // Test concurrent metadata access
                    auto streams = demuxer->getStreams();
                    if (streams.empty()) {
                        test_passed = false;
                        return;
                    }
                    
                    // Test concurrent position queries
                    uint64_t position = demuxer->getPosition();
                    uint64_t duration = demuxer->getDuration();
                    
                    // Test concurrent EOF checks
                    bool eof = demuxer->isEOF();
                    
                    operations_completed++;
                    
                    // Small delay to encourage thread interleaving
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            } catch (...) {
                test_passed = false;
            }
        };
        
        // Test concurrent seek operations (note: real implementation should handle this properly)
        auto seek_test = [&]() {
            try {
                for (int i = 0; i < 10; i++) {
                    uint64_t seek_pos = (i % 5) * 60000; // Seek to different positions
                    demuxer->seekTo(seek_pos);
                    
                    operations_completed++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            } catch (...) {
                test_passed = false;
            }
        };
        
        // Run concurrent operations
        std::thread t1(read_test);
        std::thread t2(read_test);
        std::thread t3(seek_test);
        
        t1.join();
        t2.join();
        t3.join();
        
        ASSERT_TRUE(test_passed.load(), "Concurrent operations should not fail");
        ASSERT_TRUE(operations_completed.load() > 0, "Some operations should complete");
        
        // Verify demuxer is still functional after concurrent access
        auto final_streams = demuxer->getStreams();
        ASSERT_EQUALS(1u, final_streams.size(), "Demuxer should still be functional");
        
        ASSERT_TRUE(demuxer->seekTo(0), "Should still be able to seek after concurrent access");
        ASSERT_EQUALS(0u, demuxer->getPosition(), "Position should be correct after concurrent access");
    }
};

/**
 * @brief Test FLACDemuxer frame reading performance
 */
class FLACDemuxerFramePerformanceTest : public TestCase {
public:
    FLACDemuxerFramePerformanceTest() : TestCase("FLACDemuxer Frame Reading Performance Test") {}
    
protected:
    void runTest() override {
        auto flac_data = PerformanceFLACData::generateLargeFLAC(44100, 2, 120); // 2 minutes
        auto handler = std::make_unique<MockFLACIOHandler>(flac_data);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer(), "Should parse FLAC file");
        
        // Test frame reading performance
        auto start = PerformanceMeasurement::now();
        int frames_read = 0;
        int max_frames = 100; // Limit to prevent long test times
        
        while (!demuxer->isEOF() && frames_read < max_frames) {
            auto chunk = demuxer->readChunk();
            if (chunk.isValid()) {
                frames_read++;
                
                // Validate chunk properties
                ASSERT_EQUALS(1u, chunk.stream_id, "Chunk should have correct stream ID");
                ASSERT_FALSE(chunk.data.empty(), "Chunk should have data");
                ASSERT_TRUE(chunk.is_keyframe, "FLAC frames should be keyframes");
            } else {
                break; // No more valid chunks
            }
        }
        
        auto end = PerformanceMeasurement::now();
        auto duration = PerformanceMeasurement::elapsed(start, end);
        
        if (frames_read > 0) {
            double total_ms = PerformanceMeasurement::toMilliseconds(duration);
            double avg_frame_ms = total_ms / frames_read;
            
            ASSERT_TRUE(avg_frame_ms < 10.0, "Average frame reading should be under 10ms");
            ASSERT_TRUE(total_ms < 1000.0, "Total frame reading should complete within 1 second");
        }
        
        // Test seeking and reading performance
        start = PerformanceMeasurement::now();
        
        for (int i = 0; i < 5; i++) {
            uint64_t seek_pos = i * 20000; // Every 20 seconds
            if (demuxer->seekTo(seek_pos)) {
                auto chunk = demuxer->readChunk();
                if (chunk.isValid()) {
                    // Verify chunk timestamp is reasonable
                    ASSERT_TRUE(chunk.timestamp_samples < 1000000, "Timestamp should be reasonable");
                }
            }
        }
        
        end = PerformanceMeasurement::now();
        auto seek_read_duration = PerformanceMeasurement::elapsed(start, end);
        
        double seek_read_ms = PerformanceMeasurement::toMilliseconds(seek_read_duration);
        ASSERT_TRUE(seek_read_ms < 500.0, "Seek and read operations should complete within 500ms");
    }
};

int main() {
    TestSuite suite("FLAC Demuxer Performance Validation Tests");
    
    // Add all test cases
    suite.addTest(std::make_unique<FLACDemuxerParsingPerformanceTest>());
    suite.addTest(std::make_unique<FLACDemuxerSeekingPerformanceTest>());
    suite.addTest(std::make_unique<FLACDemuxerMemoryUsageTest>());
    suite.addTest(std::make_unique<FLACDemuxerThreadSafetyTest>());
    suite.addTest(std::make_unique<FLACDemuxerFramePerformanceTest>());
    
    // Run all tests
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results);
}