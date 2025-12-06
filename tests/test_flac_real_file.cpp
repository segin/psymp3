/*
 * test_flac_real_file.cpp - FLAC demuxer test with real FLAC file
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include <iostream>
#include <cassert>
#include <chrono>
#include <fstream>

// Simple assertion macro
#define SIMPLE_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "ASSERTION FAILED: " << (message) << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

// Test file path
const char* TEST_FLAC_FILE = "/mnt/8TB-3/music/almost monday/DIVE/11 life goes by.flac";

/**
 * @brief Check if test file exists
 */
bool checkTestFileExists() {
    std::ifstream file(TEST_FLAC_FILE);
    return file.good();
}

/**
 * @brief Test FLACDemuxer with real FLAC file
 */
bool testRealFLACFile() {
    std::cout << "Testing FLACDemuxer with real FLAC file..." << std::endl;
    std::cout << "File: " << TEST_FLAC_FILE << std::endl;
    
    if (!checkTestFileExists()) {
        std::cout << "Test file not found, skipping real file test" << std::endl;
        return true; // Skip test if file doesn't exist
    }
    
    try {
        // Create FLACDemuxer with real file
        auto handler = std::make_unique<FileIOHandler>(TEST_FLAC_FILE);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        // Measure parsing time
        auto start_time = std::chrono::high_resolution_clock::now();
        bool parse_result = demuxer->parseContainer();
        auto end_time = std::chrono::high_resolution_clock::now();
        
        auto parse_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        SIMPLE_ASSERT(parse_result, "Should successfully parse real FLAC file");
        
        std::cout << "Parse time: " << parse_duration.count() << " ms" << std::endl;
        
        // Test stream information
        auto streams = demuxer->getStreams();
        SIMPLE_ASSERT(streams.size() == 1, "Should have exactly one stream");
        
        const auto& stream = streams[0];
        std::cout << "Stream info:" << std::endl;
        std::cout << "  Stream ID: " << stream.stream_id << std::endl;
        std::cout << "  Codec: " << stream.codec_name << std::endl;
        std::cout << "  Sample rate: " << stream.sample_rate << " Hz" << std::endl;
        std::cout << "  Channels: " << stream.channels << std::endl;
        std::cout << "  Bits per sample: " << stream.bits_per_sample << std::endl;
        std::cout << "  Duration: " << stream.duration_ms << " ms" << std::endl;
        
        // Validate basic stream properties
        SIMPLE_ASSERT(stream.stream_id == 1, "Stream ID should be 1");
        SIMPLE_ASSERT(stream.codec_type == "audio", "Should be audio stream");
        SIMPLE_ASSERT(stream.codec_name == "flac", "Should be FLAC codec");
        SIMPLE_ASSERT(stream.sample_rate > 0, "Sample rate should be positive");
        SIMPLE_ASSERT(stream.channels > 0 && stream.channels <= 8, "Channels should be reasonable");
        SIMPLE_ASSERT(stream.bits_per_sample >= 8 && stream.bits_per_sample <= 32, "Bit depth should be reasonable");
        
        // Test duration
        uint64_t duration = demuxer->getDuration();
        SIMPLE_ASSERT(duration > 0, "Duration should be positive");
        SIMPLE_ASSERT(duration == stream.duration_ms, "Duration should match stream duration");
        
        std::cout << "  Demuxer duration: " << duration << " ms" << std::endl;
        
        // Test position tracking
        SIMPLE_ASSERT(demuxer->getPosition() == 0, "Initial position should be 0");
        SIMPLE_ASSERT(!demuxer->isEOF(), "Should not be EOF initially");
        
        // Test metadata extraction
        if (!stream.artist.empty()) {
            std::cout << "  Artist: " << stream.artist << std::endl;
        }
        if (!stream.title.empty()) {
            std::cout << "  Title: " << stream.title << std::endl;
        }
        if (!stream.album.empty()) {
            std::cout << "  Album: " << stream.album << std::endl;
        }
        
        std::cout << "Real FLAC file test PASSED" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception during real file test: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Test seeking with real FLAC file
 */
bool testRealFLACSeeking() {
    std::cout << "Testing FLACDemuxer seeking with real FLAC file..." << std::endl;
    
    if (!checkTestFileExists()) {
        std::cout << "Test file not found, skipping seeking test" << std::endl;
        return true; // Skip test if file doesn't exist
    }
    
    try {
        auto handler = std::make_unique<FileIOHandler>(TEST_FLAC_FILE);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        SIMPLE_ASSERT(demuxer->parseContainer(), "Should parse FLAC file");
        
        uint64_t duration = demuxer->getDuration();
        std::cout << "File duration: " << duration << " ms" << std::endl;
        
        // Test seeking to beginning
        auto start_time = std::chrono::high_resolution_clock::now();
        SIMPLE_ASSERT(demuxer->seekTo(0), "Should seek to beginning");
        auto end_time = std::chrono::high_resolution_clock::now();
        
        auto seek_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        std::cout << "Seek to beginning: " << seek_duration.count() << " μs" << std::endl;
        
        SIMPLE_ASSERT(demuxer->getPosition() == 0, "Position should be 0 after seeking to beginning");
        
        // Test seeking to middle
        uint64_t middle_pos = duration / 2;
        start_time = std::chrono::high_resolution_clock::now();
        bool middle_seek = demuxer->seekTo(middle_pos);
        end_time = std::chrono::high_resolution_clock::now();
        
        seek_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        std::cout << "Seek to middle (" << middle_pos << " ms): " << seek_duration.count() << " μs" << std::endl;
        
        if (middle_seek) {
            uint64_t actual_pos = demuxer->getPosition();
            std::cout << "Actual position after seek: " << actual_pos << " ms" << std::endl;
            
            // Allow some tolerance for frame boundaries
            uint64_t tolerance = 5000; // 5 seconds tolerance
            SIMPLE_ASSERT(actual_pos >= middle_pos - tolerance && actual_pos <= middle_pos + tolerance,
                         "Seek position should be approximately correct");
        }
        
        // Test seeking to near end
        uint64_t near_end = duration - 5000; // 5 seconds from end
        if (near_end > 0 && near_end < duration) {
            start_time = std::chrono::high_resolution_clock::now();
            bool end_seek = demuxer->seekTo(near_end);
            end_time = std::chrono::high_resolution_clock::now();
            
            seek_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            std::cout << "Seek to near end (" << near_end << " ms): " << seek_duration.count() << " μs" << std::endl;
            
            if (end_seek) {
                uint64_t actual_pos = demuxer->getPosition();
                std::cout << "Actual position after end seek: " << actual_pos << " ms" << std::endl;
            }
        }
        
        // Test multiple random seeks for performance
        std::vector<uint64_t> seek_positions = {
            duration / 4,
            duration * 3 / 4,
            duration / 8,
            duration * 7 / 8,
            duration / 3
        };
        
        start_time = std::chrono::high_resolution_clock::now();
        int successful_seeks = 0;
        
        for (uint64_t pos : seek_positions) {
            if (pos < duration && demuxer->seekTo(pos)) {
                successful_seeks++;
            }
        }
        
        end_time = std::chrono::high_resolution_clock::now();
        auto total_seek_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        std::cout << "Multiple seeks (" << successful_seeks << "/" << seek_positions.size() << "): " 
                  << total_seek_time.count() << " μs total" << std::endl;
        
        if (successful_seeks > 0) {
            double avg_seek_time = static_cast<double>(total_seek_time.count()) / successful_seeks;
            std::cout << "Average seek time: " << avg_seek_time << " μs" << std::endl;
        }
        
        std::cout << "Real FLAC seeking test PASSED" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception during seeking test: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Test frame reading with real FLAC file
 */
bool testRealFLACFrameReading() {
    std::cout << "Testing FLACDemuxer frame reading with real FLAC file..." << std::endl;
    
    if (!checkTestFileExists()) {
        std::cout << "Test file not found, skipping frame reading test" << std::endl;
        return true; // Skip test if file doesn't exist
    }
    
    try {
        auto handler = std::make_unique<FileIOHandler>(TEST_FLAC_FILE);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        SIMPLE_ASSERT(demuxer->parseContainer(), "Should parse FLAC file");
        
        // Test reading first few frames
        int frames_read = 0;
        int max_frames = 10; // Limit to prevent long test times
        size_t total_data_size = 0;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        while (!demuxer->isEOF() && frames_read < max_frames) {
            auto chunk = demuxer->readChunk();
            if (!chunk.isValid()) {
                break;
            }
            
            frames_read++;
            total_data_size += chunk.data.size();
            
            // Validate chunk properties
            SIMPLE_ASSERT(chunk.stream_id == 1, "Chunk should have correct stream ID");
            SIMPLE_ASSERT(!chunk.data.empty(), "Chunk should have data");
            SIMPLE_ASSERT(chunk.is_keyframe, "FLAC frames should be keyframes");
            
            std::cout << "Frame " << frames_read << ": " << chunk.data.size() << " bytes, "
                      << "timestamp: " << chunk.timestamp_samples << " samples" << std::endl;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto read_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        std::cout << "Read " << frames_read << " frames in " << read_duration.count() << " μs" << std::endl;
        std::cout << "Total data read: " << total_data_size << " bytes" << std::endl;
        
        if (frames_read > 0) {
            double avg_frame_time = static_cast<double>(read_duration.count()) / frames_read;
            double avg_frame_size = static_cast<double>(total_data_size) / frames_read;
            
            std::cout << "Average frame read time: " << avg_frame_time << " μs" << std::endl;
            std::cout << "Average frame size: " << avg_frame_size << " bytes" << std::endl;
        }
        
        // Test seeking and reading
        uint64_t duration = demuxer->getDuration();
        uint64_t seek_pos = duration / 4; // Seek to 25%
        
        if (demuxer->seekTo(seek_pos)) {
            std::cout << "Seeking to " << seek_pos << " ms and reading frame..." << std::endl;
            
            auto seek_chunk = demuxer->readChunk();
            if (seek_chunk.isValid()) {
                std::cout << "Frame after seek: " << seek_chunk.data.size() << " bytes, "
                          << "timestamp: " << seek_chunk.timestamp_samples << " samples" << std::endl;
            }
        }
        
        std::cout << "Real FLAC frame reading test PASSED" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception during frame reading test: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Test FLACDemuxer performance characteristics
 */
bool testPerformanceCharacteristics() {
    std::cout << "Testing FLACDemuxer performance characteristics..." << std::endl;
    
    if (!checkTestFileExists()) {
        std::cout << "Test file not found, skipping performance test" << std::endl;
        return true; // Skip test if file doesn't exist
    }
    
    try {
        // Test parsing performance
        auto handler = std::make_unique<FileIOHandler>(TEST_FLAC_FILE);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        auto parse_start = std::chrono::high_resolution_clock::now();
        bool parsed = demuxer->parseContainer();
        auto parse_end = std::chrono::high_resolution_clock::now();
        
        SIMPLE_ASSERT(parsed, "Should parse FLAC file");
        
        auto parse_time = std::chrono::duration_cast<std::chrono::microseconds>(parse_end - parse_start);
        std::cout << "Parse time: " << parse_time.count() << " μs" << std::endl;
        
        // Performance targets
        SIMPLE_ASSERT(parse_time.count() < 100000, "Parse time should be under 100ms");
        
        uint64_t duration = demuxer->getDuration();
        std::cout << "File duration: " << duration << " ms" << std::endl;
        
        // Test multiple seeks for average performance
        std::vector<uint64_t> seek_positions;
        for (int i = 1; i <= 10; i++) {
            seek_positions.push_back((duration * i) / 11); // Divide into 11 segments
        }
        
        auto seek_start = std::chrono::high_resolution_clock::now();
        int successful_seeks = 0;
        
        for (uint64_t pos : seek_positions) {
            if (demuxer->seekTo(pos)) {
                successful_seeks++;
            }
        }
        
        auto seek_end = std::chrono::high_resolution_clock::now();
        auto total_seek_time = std::chrono::duration_cast<std::chrono::microseconds>(seek_end - seek_start);
        
        std::cout << "Successful seeks: " << successful_seeks << "/" << seek_positions.size() << std::endl;
        std::cout << "Total seek time: " << total_seek_time.count() << " μs" << std::endl;
        
        if (successful_seeks > 0) {
            double avg_seek_time = static_cast<double>(total_seek_time.count()) / successful_seeks;
            std::cout << "Average seek time: " << avg_seek_time << " μs" << std::endl;
            
            // Performance target: average seek should be under 50ms
            SIMPLE_ASSERT(avg_seek_time < 50000, "Average seek time should be under 50ms");
        }
        
        // Test frame reading performance
        demuxer->seekTo(0); // Reset to beginning
        
        auto read_start = std::chrono::high_resolution_clock::now();
        int frames_read = 0;
        size_t total_bytes = 0;
        
        for (int i = 0; i < 20 && !demuxer->isEOF(); i++) {
            auto chunk = demuxer->readChunk();
            if (chunk.isValid()) {
                frames_read++;
                total_bytes += chunk.data.size();
            }
        }
        
        auto read_end = std::chrono::high_resolution_clock::now();
        auto read_time = std::chrono::duration_cast<std::chrono::microseconds>(read_end - read_start);
        
        std::cout << "Frame reading: " << frames_read << " frames, " << total_bytes << " bytes in " 
                  << read_time.count() << " μs" << std::endl;
        
        if (frames_read > 0) {
            double avg_frame_time = static_cast<double>(read_time.count()) / frames_read;
            std::cout << "Average frame read time: " << avg_frame_time << " μs" << std::endl;
            
            // Performance target: frame reading should be under 10ms per frame
            SIMPLE_ASSERT(avg_frame_time < 10000, "Average frame read time should be under 10ms");
        }
        
        std::cout << "Performance characteristics test PASSED" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception during performance test: " << e.what() << std::endl;
        return false;
    }
}

int main() {
    std::cout << "FLAC Demuxer Real File Performance Test" << std::endl;
    std::cout << "=======================================" << std::endl;
    std::cout << "Test file: " << TEST_FLAC_FILE << std::endl;
    std::cout << std::endl;
    
    int tests_run = 0;
    int tests_passed = 0;
    
    // Run tests
    if (testRealFLACFile()) tests_passed++;
    tests_run++;
    
    if (testRealFLACSeeking()) tests_passed++;
    tests_run++;
    
    if (testRealFLACFrameReading()) tests_passed++;
    tests_run++;
    
    if (testPerformanceCharacteristics()) tests_passed++;
    tests_run++;
    
    // Print results
    std::cout << std::endl;
    std::cout << "Test Results:" << std::endl;
    std::cout << "=============" << std::endl;
    std::cout << "Tests run: " << tests_run << std::endl;
    std::cout << "Tests passed: " << tests_passed << std::endl;
    std::cout << "Tests failed: " << (tests_run - tests_passed) << std::endl;
    
    if (tests_passed == tests_run) {
        std::cout << "All tests PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << "Some tests FAILED!" << std::endl;
        return 1;
    }
}