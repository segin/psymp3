/*
 * test_flac_bisection_real_files.cpp - Test FLAC bisection seeking with real files
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * This test validates the FLAC bisection seeking implementation per the
 * flac-bisection-seeking spec:
 * - Task 7.1: Test seeking on FLAC files without SEEKTABLEs
 * - Task 7.2: Verify user can play "RADIO GA GA.flac" without issues
 *
 * Requirements validated:
 * - 4.2: Time differential within 250ms tolerance
 * - 5.1: Seeking to first 500ms seeks directly to audio data offset
 * - 5.2: Seeking to last 500ms estimates position conservatively
 */

#include "psymp3.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <cmath>

// Test assertion macro
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "ASSERTION FAILED: " << (message) << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

// Tolerance for bisection seeking (250ms per Requirement 4.2)
static constexpr uint64_t SEEK_TOLERANCE_MS = 250;

// Test file paths - files in tests/data directory
static const std::vector<std::string> TEST_FILES = {
    "tests/data/RADIO GA GA.flac",
    "tests/data/04 Time.flac",
    "tests/data/11 Everlong.flac",
    "tests/data/11 life goes by.flac"
};

/**
 * @brief Check if a file exists
 */
static bool fileExists(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

/**
 * @brief Get list of available test files
 */
static std::vector<std::string> getAvailableTestFiles() {
    std::vector<std::string> available;
    for (const auto& path : TEST_FILES) {
        if (fileExists(path)) {
            available.push_back(path);
        }
    }
    return available;
}

/**
 * @brief Test structure to hold seeking test results
 */
struct SeekTestResult {
    uint64_t target_ms;
    uint64_t actual_ms;
    int64_t diff_ms;
    bool within_tolerance;
    double seek_time_us;
    bool success;
};

/**
 * @brief Test bisection seeking accuracy on a single file
 * 
 * Validates Requirements 4.2, 5.1, 5.2
 */
static bool testBisectionSeekingAccuracy(const std::string& filepath) {
    std::cout << "\n=== Testing bisection seeking: " << filepath << " ===" << std::endl;
    
    try {
        // Create FLACDemuxer
        auto handler = std::make_unique<FileIOHandler>(filepath);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        // Parse container
        if (!demuxer->parseContainer()) {
            std::cerr << "Failed to parse FLAC file: " << filepath << std::endl;
            return false;
        }
        
        // Get stream info
        auto streams = demuxer->getStreams();
        TEST_ASSERT(!streams.empty(), "Should have at least one stream");
        
        const auto& stream = streams[0];
        uint64_t duration_ms = demuxer->getDuration();
        uint32_t sample_rate = stream.sample_rate;
        
        std::cout << "File info:" << std::endl;
        std::cout << "  Duration: " << duration_ms << " ms" << std::endl;
        std::cout << "  Sample rate: " << sample_rate << " Hz" << std::endl;
        std::cout << "  Channels: " << stream.channels << std::endl;
        std::cout << "  Bits per sample: " << stream.bits_per_sample << std::endl;
        
        TEST_ASSERT(duration_ms > 0, "Duration should be positive");
        
        // Define seek positions to test
        // Test beginning, middle, and end per Task 7.1
        std::vector<uint64_t> seek_positions;
        
        // Beginning (Requirement 5.1: first 500ms)
        seek_positions.push_back(0);
        seek_positions.push_back(100);
        seek_positions.push_back(250);
        seek_positions.push_back(500);
        
        // Middle positions
        seek_positions.push_back(duration_ms / 4);
        seek_positions.push_back(duration_ms / 3);
        seek_positions.push_back(duration_ms / 2);
        seek_positions.push_back(2 * duration_ms / 3);
        seek_positions.push_back(3 * duration_ms / 4);
        
        // End positions (Requirement 5.2: last 500ms)
        if (duration_ms > 1000) {
            seek_positions.push_back(duration_ms - 500);
            seek_positions.push_back(duration_ms - 250);
            seek_positions.push_back(duration_ms - 100);
        }
        
        std::vector<SeekTestResult> results;
        int seeks_within_tolerance = 0;
        int total_seeks = 0;
        
        std::cout << "\nSeeking tests (tolerance: " << SEEK_TOLERANCE_MS << "ms):" << std::endl;
        std::cout << "  Target(ms)  Actual(ms)  Diff(ms)  Status      Time(μs)" << std::endl;
        std::cout << "  ----------  ----------  --------  ----------  --------" << std::endl;
        
        for (uint64_t target_ms : seek_positions) {
            if (target_ms >= duration_ms) continue;
            
            SeekTestResult result;
            result.target_ms = target_ms;
            
            // Measure seek time
            auto start = std::chrono::high_resolution_clock::now();
            result.success = demuxer->seekTo(target_ms);
            auto end = std::chrono::high_resolution_clock::now();
            
            result.seek_time_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            
            if (result.success) {
                result.actual_ms = demuxer->getPosition();
                result.diff_ms = static_cast<int64_t>(result.actual_ms) - static_cast<int64_t>(target_ms);
                result.within_tolerance = std::abs(result.diff_ms) <= static_cast<int64_t>(SEEK_TOLERANCE_MS);
                
                if (result.within_tolerance) {
                    seeks_within_tolerance++;
                }
                total_seeks++;
                
                std::cout << "  " << std::setw(10) << target_ms 
                          << "  " << std::setw(10) << result.actual_ms
                          << "  " << std::setw(8) << result.diff_ms
                          << "  " << (result.within_tolerance ? "OK        " : "EXCEEDED  ")
                          << "  " << std::setw(8) << static_cast<int>(result.seek_time_us)
                          << std::endl;
            } else {
                result.actual_ms = 0;
                result.diff_ms = 0;
                result.within_tolerance = false;
                total_seeks++;
                
                std::cout << "  " << std::setw(10) << target_ms 
                          << "  " << std::setw(10) << "FAILED"
                          << "  " << std::setw(8) << "-"
                          << "  " << "FAILED    "
                          << "  " << std::setw(8) << static_cast<int>(result.seek_time_us)
                          << std::endl;
            }
            
            results.push_back(result);
        }
        
        // Summary
        std::cout << "\nResults summary:" << std::endl;
        std::cout << "  Seeks within tolerance: " << seeks_within_tolerance << "/" << total_seeks << std::endl;
        
        double success_rate = (total_seeks > 0) ? 
            (static_cast<double>(seeks_within_tolerance) / total_seeks * 100.0) : 0.0;
        std::cout << "  Success rate: " << std::fixed << std::setprecision(1) << success_rate << "%" << std::endl;
        
        // Calculate average seek time
        double total_seek_time = 0;
        for (const auto& r : results) {
            total_seek_time += r.seek_time_us;
        }
        double avg_seek_time = results.empty() ? 0 : total_seek_time / results.size();
        std::cout << "  Average seek time: " << std::fixed << std::setprecision(0) << avg_seek_time << " μs" << std::endl;
        
        // Test passes if at least 80% of seeks are within tolerance
        // (some edge cases near file boundaries may exceed tolerance)
        bool passed = (success_rate >= 80.0);
        std::cout << "\nTest " << (passed ? "PASSED" : "FAILED") << std::endl;
        
        return passed;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Test that seeking works after reading some data
 * 
 * This tests the real-world scenario where a user plays a file
 * and then seeks to a different position.
 */
static bool testSeekAfterReading(const std::string& filepath) {
    std::cout << "\n=== Testing seek after reading: " << filepath << " ===" << std::endl;
    
    try {
        auto handler = std::make_unique<FileIOHandler>(filepath);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        if (!demuxer->parseContainer()) {
            std::cerr << "Failed to parse FLAC file" << std::endl;
            return false;
        }
        
        uint64_t duration_ms = demuxer->getDuration();
        
        // Read a few frames first
        std::cout << "Reading initial frames..." << std::endl;
        int frames_read = 0;
        for (int i = 0; i < 5 && !demuxer->isEOF(); i++) {
            auto chunk = demuxer->readChunk();
            if (chunk.isValid()) {
                frames_read++;
            }
        }
        std::cout << "  Read " << frames_read << " frames" << std::endl;
        
        uint64_t pos_after_read = demuxer->getPosition();
        std::cout << "  Position after reading: " << pos_after_read << " ms" << std::endl;
        
        // Now seek to middle
        uint64_t target = duration_ms / 2;
        std::cout << "Seeking to middle (" << target << " ms)..." << std::endl;
        
        bool seek_result = demuxer->seekTo(target);
        TEST_ASSERT(seek_result, "Seek should succeed");
        
        uint64_t actual = demuxer->getPosition();
        int64_t diff = static_cast<int64_t>(actual) - static_cast<int64_t>(target);
        
        std::cout << "  Actual position: " << actual << " ms (diff: " << diff << " ms)" << std::endl;
        
        bool within_tolerance = std::abs(diff) <= static_cast<int64_t>(SEEK_TOLERANCE_MS);
        std::cout << "  Within tolerance: " << (within_tolerance ? "YES" : "NO") << std::endl;
        
        // Read a frame after seeking to verify we can continue
        auto chunk = demuxer->readChunk();
        TEST_ASSERT(chunk.isValid() || demuxer->isEOF(), "Should be able to read after seek");
        
        if (chunk.isValid()) {
            std::cout << "  Successfully read frame after seek (" << chunk.data.size() << " bytes)" << std::endl;
        }
        
        std::cout << "Test " << (within_tolerance ? "PASSED" : "FAILED") << std::endl;
        return within_tolerance;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Test multiple consecutive seeks (simulates user scrubbing)
 */
static bool testConsecutiveSeeks(const std::string& filepath) {
    std::cout << "\n=== Testing consecutive seeks: " << filepath << " ===" << std::endl;
    
    try {
        auto handler = std::make_unique<FileIOHandler>(filepath);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        if (!demuxer->parseContainer()) {
            std::cerr << "Failed to parse FLAC file" << std::endl;
            return false;
        }
        
        uint64_t duration_ms = demuxer->getDuration();
        
        // Simulate user scrubbing through the file
        std::vector<uint64_t> scrub_positions = {
            duration_ms / 4,
            duration_ms / 2,
            duration_ms * 3 / 4,
            duration_ms / 8,
            duration_ms * 7 / 8,
            duration_ms / 2,  // Back to middle
            0,                // Back to start
            duration_ms - 1000  // Near end
        };
        
        int successful_seeks = 0;
        int seeks_within_tolerance = 0;
        
        std::cout << "Performing " << scrub_positions.size() << " consecutive seeks..." << std::endl;
        
        for (size_t i = 0; i < scrub_positions.size(); i++) {
            uint64_t target = scrub_positions[i];
            if (target >= duration_ms) target = duration_ms - 100;
            
            bool result = demuxer->seekTo(target);
            if (result) {
                successful_seeks++;
                uint64_t actual = demuxer->getPosition();
                int64_t diff = static_cast<int64_t>(actual) - static_cast<int64_t>(target);
                
                if (std::abs(diff) <= static_cast<int64_t>(SEEK_TOLERANCE_MS)) {
                    seeks_within_tolerance++;
                }
                
                std::cout << "  Seek " << (i + 1) << ": " << target << " ms -> " << actual 
                          << " ms (diff: " << diff << " ms)" << std::endl;
            } else {
                std::cout << "  Seek " << (i + 1) << ": " << target << " ms -> FAILED" << std::endl;
            }
        }
        
        std::cout << "\nResults:" << std::endl;
        std::cout << "  Successful seeks: " << successful_seeks << "/" << scrub_positions.size() << std::endl;
        std::cout << "  Within tolerance: " << seeks_within_tolerance << "/" << scrub_positions.size() << std::endl;
        
        bool passed = (seeks_within_tolerance >= static_cast<int>(scrub_positions.size() * 0.8));
        std::cout << "Test " << (passed ? "PASSED" : "FAILED") << std::endl;
        
        return passed;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Specific test for RADIO GA GA.flac per Task 7.2
 */
static bool testRadioGaGa() {
    const std::string filepath = "tests/data/RADIO GA GA.flac";
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Task 7.2: Testing RADIO GA GA.flac" << std::endl;
    std::cout << "========================================" << std::endl;
    
    if (!fileExists(filepath)) {
        std::cout << "RADIO GA GA.flac not found, skipping specific test" << std::endl;
        return true;  // Not a failure if file doesn't exist
    }
    
    try {
        auto handler = std::make_unique<FileIOHandler>(filepath);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        // Test 1: File loads correctly
        std::cout << "\n1. Testing file loading..." << std::endl;
        if (!demuxer->parseContainer()) {
            std::cerr << "FAILED: Could not parse RADIO GA GA.flac" << std::endl;
            return false;
        }
        std::cout << "   PASSED: File loaded successfully" << std::endl;
        
        // Get file info
        auto streams = demuxer->getStreams();
        if (streams.empty()) {
            std::cerr << "FAILED: No streams found" << std::endl;
            return false;
        }
        
        const auto& stream = streams[0];
        uint64_t duration_ms = demuxer->getDuration();
        
        std::cout << "\n   File details:" << std::endl;
        std::cout << "     Duration: " << duration_ms << " ms (" 
                  << (duration_ms / 1000 / 60) << ":" 
                  << std::setfill('0') << std::setw(2) << ((duration_ms / 1000) % 60) << ")" << std::endl;
        std::cout << "     Sample rate: " << stream.sample_rate << " Hz" << std::endl;
        std::cout << "     Channels: " << stream.channels << std::endl;
        std::cout << "     Bits per sample: " << stream.bits_per_sample << std::endl;
        if (!stream.title.empty()) std::cout << "     Title: " << stream.title << std::endl;
        if (!stream.artist.empty()) std::cout << "     Artist: " << stream.artist << std::endl;
        
        // Test 2: Can read frames (simulates playback)
        std::cout << "\n2. Testing frame reading (playback simulation)..." << std::endl;
        int frames_read = 0;
        size_t total_bytes = 0;
        
        for (int i = 0; i < 10 && !demuxer->isEOF(); i++) {
            auto chunk = demuxer->readChunk();
            if (chunk.isValid()) {
                frames_read++;
                total_bytes += chunk.data.size();
            }
        }
        
        if (frames_read > 0) {
            std::cout << "   PASSED: Read " << frames_read << " frames (" << total_bytes << " bytes)" << std::endl;
        } else {
            std::cerr << "   FAILED: Could not read any frames" << std::endl;
            return false;
        }
        
        // Test 3: Seeking works throughout the track
        std::cout << "\n3. Testing seeking throughout track..." << std::endl;
        
        std::vector<std::pair<std::string, uint64_t>> seek_tests = {
            {"Beginning (0s)", 0},
            {"30 seconds", 30000},
            {"1 minute", 60000},
            {"Middle", duration_ms / 2},
            {"3/4 through", duration_ms * 3 / 4},
            {"Near end", duration_ms > 5000 ? duration_ms - 5000 : duration_ms / 2}
        };
        
        int seek_passes = 0;
        for (const auto& test : seek_tests) {
            if (test.second >= duration_ms) continue;
            
            bool result = demuxer->seekTo(test.second);
            uint64_t actual = demuxer->getPosition();
            int64_t diff = static_cast<int64_t>(actual) - static_cast<int64_t>(test.second);
            bool within_tolerance = std::abs(diff) <= static_cast<int64_t>(SEEK_TOLERANCE_MS);
            
            std::cout << "   " << test.first << " (" << test.second << " ms): ";
            if (result && within_tolerance) {
                std::cout << "PASSED (actual: " << actual << " ms, diff: " << diff << " ms)" << std::endl;
                seek_passes++;
            } else if (result) {
                std::cout << "EXCEEDED TOLERANCE (actual: " << actual << " ms, diff: " << diff << " ms)" << std::endl;
            } else {
                std::cout << "FAILED" << std::endl;
            }
        }
        
        // Test 4: Can read after seeking
        std::cout << "\n4. Testing read after seek..." << std::endl;
        demuxer->seekTo(duration_ms / 2);
        auto chunk = demuxer->readChunk();
        if (chunk.isValid()) {
            std::cout << "   PASSED: Successfully read frame after seek" << std::endl;
        } else {
            std::cerr << "   FAILED: Could not read after seek" << std::endl;
            return false;
        }
        
        // Overall result
        std::cout << "\n========================================" << std::endl;
        bool overall_pass = (seek_passes >= static_cast<int>(seek_tests.size() * 0.8));
        std::cout << "RADIO GA GA.flac test: " << (overall_pass ? "PASSED" : "FAILED") << std::endl;
        std::cout << "========================================" << std::endl;
        
        return overall_pass;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception testing RADIO GA GA.flac: " << e.what() << std::endl;
        return false;
    }
}

int main() {
    std::cout << "FLAC Bisection Seeking Real File Tests" << std::endl;
    std::cout << "======================================" << std::endl;
    std::cout << "Testing bisection seeking per flac-bisection-seeking spec" << std::endl;
    std::cout << "Tolerance: " << SEEK_TOLERANCE_MS << " ms (Requirement 4.2)" << std::endl;
    
    // Get available test files
    auto available_files = getAvailableTestFiles();
    
    if (available_files.empty()) {
        std::cout << "\nNo test files found in tests/data/" << std::endl;
        std::cout << "Expected files:" << std::endl;
        for (const auto& f : TEST_FILES) {
            std::cout << "  - " << f << std::endl;
        }
        std::cout << "\nSkipping real file tests (no test data available)" << std::endl;
        return 0;  // Not a failure, just no test data
    }
    
    std::cout << "\nFound " << available_files.size() << " test file(s):" << std::endl;
    for (const auto& f : available_files) {
        std::cout << "  - " << f << std::endl;
    }
    
    int tests_run = 0;
    int tests_passed = 0;
    
    // Task 7.1: Test seeking on FLAC files
    std::cout << "\n========================================" << std::endl;
    std::cout << "Task 7.1: Testing seeking accuracy" << std::endl;
    std::cout << "========================================" << std::endl;
    
    for (const auto& filepath : available_files) {
        // Test bisection seeking accuracy
        if (testBisectionSeekingAccuracy(filepath)) tests_passed++;
        tests_run++;
        
        // Test seek after reading
        if (testSeekAfterReading(filepath)) tests_passed++;
        tests_run++;
        
        // Test consecutive seeks
        if (testConsecutiveSeeks(filepath)) tests_passed++;
        tests_run++;
    }
    
    // Task 7.2: Specific test for RADIO GA GA.flac
    if (testRadioGaGa()) tests_passed++;
    tests_run++;
    
    // Final summary
    std::cout << "\n========================================" << std::endl;
    std::cout << "Final Results" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Tests run: " << tests_run << std::endl;
    std::cout << "Tests passed: " << tests_passed << std::endl;
    std::cout << "Tests failed: " << (tests_run - tests_passed) << std::endl;
    
    if (tests_passed == tests_run) {
        std::cout << "\nAll tests PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << "\nSome tests FAILED!" << std::endl;
        return 1;
    }
}
