/*
 * test_flac_mini_player.cpp - Mini player to test FLAC pipeline with seeking
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * This test creates a mini-player that exercises the full FLAC pipeline:
 * - Opens a FLAC file via FLACDemuxer
 * - Reads frames (simulating playback)
 * - Performs seeks to various positions
 * - Validates seeking accuracy per flac-bisection-seeking spec
 *
 * Task 7.1: Test seeking on FLAC files without SEEKTABLEs
 * Task 7.2: Verify user can play "RADIO GA GA.flac" without issues
 */

#include "psymp3.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <vector>
#include <cmath>
#include <thread>

// Tolerance for bisection seeking (250ms per Requirement 4.2)
static constexpr uint64_t SEEK_TOLERANCE_MS = 250;

/**
 * @brief Mini player class that simulates a real media player
 */
class MiniPlayer {
public:
    MiniPlayer() = default;
    ~MiniPlayer() = default;

    /**
     * @brief Open a FLAC file for playback
     */
    bool open(const std::string& filepath) {
        std::cout << "\n[MiniPlayer] Opening: " << filepath << std::endl;
        
        m_filepath = filepath;
        
        // Create IOHandler
        auto handler = std::make_unique<FileIOHandler>(filepath);
        if (!handler) {
            std::cerr << "[MiniPlayer] Failed to create IOHandler" << std::endl;
            return false;
        }
        
        // Create FLACDemuxer
        m_demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        if (!m_demuxer) {
            std::cerr << "[MiniPlayer] Failed to create FLACDemuxer" << std::endl;
            return false;
        }
        
        // Parse container
        if (!m_demuxer->parseContainer()) {
            std::cerr << "[MiniPlayer] Failed to parse FLAC container" << std::endl;
            return false;
        }
        
        // Get stream info
        auto streams = m_demuxer->getStreams();
        if (streams.empty()) {
            std::cerr << "[MiniPlayer] No streams found" << std::endl;
            return false;
        }
        
        m_stream_info = streams[0];
        m_duration_ms = m_demuxer->getDuration();
        
        std::cout << "[MiniPlayer] File opened successfully:" << std::endl;
        std::cout << "  Duration: " << formatTime(m_duration_ms) << std::endl;
        std::cout << "  Sample rate: " << m_stream_info.sample_rate << " Hz" << std::endl;
        std::cout << "  Channels: " << m_stream_info.channels << std::endl;
        std::cout << "  Bits per sample: " << m_stream_info.bits_per_sample << std::endl;
        if (!m_stream_info.title.empty()) {
            std::cout << "  Title: " << m_stream_info.title << std::endl;
        }
        if (!m_stream_info.artist.empty()) {
            std::cout << "  Artist: " << m_stream_info.artist << std::endl;
        }
        
        m_is_open = true;
        return true;
    }

    /**
     * @brief Close the current file
     */
    void close() {
        m_demuxer.reset();
        m_is_open = false;
        m_filepath.clear();
        std::cout << "[MiniPlayer] Closed" << std::endl;
    }

    /**
     * @brief Simulate playback by reading frames
     * @param duration_ms How long to "play" in milliseconds
     * @return Number of frames read
     */
    int play(uint64_t duration_ms) {
        if (!m_is_open) {
            std::cerr << "[MiniPlayer] Not open" << std::endl;
            return 0;
        }
        
        uint64_t start_pos = m_demuxer->getPosition();
        uint64_t target_end = start_pos + duration_ms;
        int frames_read = 0;
        size_t total_bytes = 0;
        
        std::cout << "[MiniPlayer] Playing from " << formatTime(start_pos) 
                  << " for " << duration_ms << "ms..." << std::endl;
        
        while (!m_demuxer->isEOF() && m_demuxer->getPosition() < target_end) {
            auto chunk = m_demuxer->readChunk();
            if (chunk.isValid()) {
                frames_read++;
                total_bytes += chunk.data.size();
            } else {
                break;
            }
        }
        
        uint64_t end_pos = m_demuxer->getPosition();
        std::cout << "[MiniPlayer] Played " << frames_read << " frames (" 
                  << total_bytes << " bytes), now at " << formatTime(end_pos) << std::endl;
        
        return frames_read;
    }

    /**
     * @brief Seek to a specific position
     * @param position_ms Target position in milliseconds
     * @return SeekResult with details about the seek
     */
    struct SeekResult {
        uint64_t target_ms;
        uint64_t actual_ms;
        int64_t diff_ms;
        bool success;
        bool within_tolerance;
        double seek_time_us;
    };
    
    SeekResult seek(uint64_t position_ms) {
        SeekResult result;
        result.target_ms = position_ms;
        result.success = false;
        result.within_tolerance = false;
        result.seek_time_us = 0;
        
        if (!m_is_open) {
            std::cerr << "[MiniPlayer] Not open" << std::endl;
            return result;
        }
        
        std::cout << "[MiniPlayer] Seeking to " << formatTime(position_ms) << "..." << std::endl;
        
        // Measure seek time
        auto start = std::chrono::high_resolution_clock::now();
        result.success = m_demuxer->seekTo(position_ms);
        auto end = std::chrono::high_resolution_clock::now();
        
        result.seek_time_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        if (result.success) {
            result.actual_ms = m_demuxer->getPosition();
            result.diff_ms = static_cast<int64_t>(result.actual_ms) - static_cast<int64_t>(position_ms);
            result.within_tolerance = std::abs(result.diff_ms) <= static_cast<int64_t>(SEEK_TOLERANCE_MS);
            
            std::cout << "[MiniPlayer] Seek result: actual=" << formatTime(result.actual_ms)
                      << ", diff=" << result.diff_ms << "ms"
                      << ", " << (result.within_tolerance ? "OK" : "EXCEEDED")
                      << " (" << result.seek_time_us << " μs)" << std::endl;
        } else {
            std::cout << "[MiniPlayer] Seek FAILED" << std::endl;
        }
        
        return result;
    }

    /**
     * @brief Get current playback position
     */
    uint64_t getPosition() const {
        return m_is_open ? m_demuxer->getPosition() : 0;
    }

    /**
     * @brief Get total duration
     */
    uint64_t getDuration() const {
        return m_duration_ms;
    }

    /**
     * @brief Check if at end of file
     */
    bool isEOF() const {
        return m_is_open ? m_demuxer->isEOF() : true;
    }

    /**
     * @brief Check if file is open
     */
    bool isOpen() const {
        return m_is_open;
    }

private:
    std::string formatTime(uint64_t ms) const {
        uint64_t seconds = ms / 1000;
        uint64_t minutes = seconds / 60;
        seconds %= 60;
        uint64_t millis = ms % 1000;
        
        std::ostringstream oss;
        oss << minutes << ":" << std::setfill('0') << std::setw(2) << seconds
            << "." << std::setw(3) << millis;
        return oss.str();
    }

    std::unique_ptr<FLACDemuxer> m_demuxer;
    StreamInfo m_stream_info;
    std::string m_filepath;
    uint64_t m_duration_ms = 0;
    bool m_is_open = false;
};

/**
 * @brief Test basic playback and seeking
 */
static bool testBasicPlaybackAndSeeking(const std::string& filepath) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test: Basic Playback and Seeking" << std::endl;
    std::cout << "========================================" << std::endl;
    
    MiniPlayer player;
    
    // Open file
    if (!player.open(filepath)) {
        std::cerr << "Failed to open file" << std::endl;
        return false;
    }
    
    uint64_t duration = player.getDuration();
    
    // Play first 2 seconds
    std::cout << "\n--- Playing first 2 seconds ---" << std::endl;
    int frames1 = player.play(2000);
    if (frames1 == 0) {
        std::cerr << "Failed to read any frames" << std::endl;
        return false;
    }
    
    // Seek to middle
    std::cout << "\n--- Seeking to middle ---" << std::endl;
    auto result1 = player.seek(duration / 2);
    if (!result1.success) {
        std::cerr << "Seek to middle failed" << std::endl;
        return false;
    }
    
    // Play 2 seconds from middle
    std::cout << "\n--- Playing 2 seconds from middle ---" << std::endl;
    int frames2 = player.play(2000);
    if (frames2 == 0) {
        std::cerr << "Failed to read frames after seek" << std::endl;
        return false;
    }
    
    // Seek to 30 seconds
    std::cout << "\n--- Seeking to 30 seconds ---" << std::endl;
    auto result2 = player.seek(30000);
    
    // Seek to 1 minute
    std::cout << "\n--- Seeking to 1 minute ---" << std::endl;
    auto result3 = player.seek(60000);
    
    // Seek back to beginning
    std::cout << "\n--- Seeking back to beginning ---" << std::endl;
    auto result4 = player.seek(0);
    
    // Play 1 second from beginning
    std::cout << "\n--- Playing 1 second from beginning ---" << std::endl;
    int frames3 = player.play(1000);
    
    player.close();
    
    // Summary
    std::cout << "\n--- Summary ---" << std::endl;
    std::cout << "Frames read (first 2s): " << frames1 << std::endl;
    std::cout << "Frames read (middle 2s): " << frames2 << std::endl;
    std::cout << "Frames read (beginning 1s): " << frames3 << std::endl;
    std::cout << "Seek to middle: " << (result1.within_tolerance ? "OK" : "EXCEEDED") 
              << " (diff: " << result1.diff_ms << "ms)" << std::endl;
    std::cout << "Seek to 30s: " << (result2.within_tolerance ? "OK" : "EXCEEDED")
              << " (diff: " << result2.diff_ms << "ms)" << std::endl;
    std::cout << "Seek to 1min: " << (result3.within_tolerance ? "OK" : "EXCEEDED")
              << " (diff: " << result3.diff_ms << "ms)" << std::endl;
    std::cout << "Seek to beginning: " << (result4.within_tolerance ? "OK" : "EXCEEDED")
              << " (diff: " << result4.diff_ms << "ms)" << std::endl;
    
    // Pass if at least 3 out of 4 seeks are within tolerance
    int seeks_ok = (result1.within_tolerance ? 1 : 0) + 
                   (result2.within_tolerance ? 1 : 0) +
                   (result3.within_tolerance ? 1 : 0) +
                   (result4.within_tolerance ? 1 : 0);
    
    bool passed = (seeks_ok >= 3);
    std::cout << "\nTest " << (passed ? "PASSED" : "FAILED") 
              << " (" << seeks_ok << "/4 seeks within tolerance)" << std::endl;
    
    return passed;
}

/**
 * @brief Test seeking to multiple positions (simulates user scrubbing)
 */
static bool testSeekingScrubbing(const std::string& filepath) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test: Seeking Scrubbing Simulation" << std::endl;
    std::cout << "========================================" << std::endl;
    
    MiniPlayer player;
    
    if (!player.open(filepath)) {
        std::cerr << "Failed to open file" << std::endl;
        return false;
    }
    
    uint64_t duration = player.getDuration();
    
    // Simulate user scrubbing through the file
    std::vector<uint64_t> scrub_positions;
    
    // Add positions at 10% intervals
    for (int i = 0; i <= 10; i++) {
        uint64_t pos = (duration * i) / 10;
        if (pos < duration) {
            scrub_positions.push_back(pos);
        }
    }
    
    // Add some random-ish positions
    scrub_positions.push_back(duration / 3);
    scrub_positions.push_back(duration * 2 / 3);
    scrub_positions.push_back(15000);  // 15 seconds
    scrub_positions.push_back(45000);  // 45 seconds
    scrub_positions.push_back(90000);  // 1.5 minutes
    
    std::cout << "\nPerforming " << scrub_positions.size() << " seeks..." << std::endl;
    std::cout << std::setw(12) << "Target" << std::setw(12) << "Actual" 
              << std::setw(10) << "Diff" << std::setw(10) << "Status" 
              << std::setw(12) << "Time(μs)" << std::endl;
    std::cout << std::string(56, '-') << std::endl;
    
    int seeks_ok = 0;
    int seeks_total = 0;
    
    for (uint64_t target : scrub_positions) {
        auto result = player.seek(target);
        seeks_total++;
        
        if (result.within_tolerance) {
            seeks_ok++;
        }
        
        std::cout << std::setw(12) << target 
                  << std::setw(12) << result.actual_ms
                  << std::setw(10) << result.diff_ms
                  << std::setw(10) << (result.within_tolerance ? "OK" : "EXCEEDED")
                  << std::setw(12) << static_cast<int>(result.seek_time_us)
                  << std::endl;
        
        // Read one frame after each seek to verify we can continue
        auto chunk = player.play(100);  // Play 100ms
    }
    
    player.close();
    
    double success_rate = (seeks_total > 0) ? 
        (static_cast<double>(seeks_ok) / seeks_total * 100.0) : 0.0;
    
    std::cout << "\n--- Summary ---" << std::endl;
    std::cout << "Seeks within tolerance: " << seeks_ok << "/" << seeks_total << std::endl;
    std::cout << "Success rate: " << std::fixed << std::setprecision(1) 
              << success_rate << "%" << std::endl;
    
    // Pass if at least 80% of seeks are within tolerance
    bool passed = (success_rate >= 80.0);
    std::cout << "\nTest " << (passed ? "PASSED" : "FAILED") << std::endl;
    
    return passed;
}

/**
 * @brief Test seeking with fresh demuxer instances (no cached frame index)
 */
static bool testSeekingWithFreshDemuxer(const std::string& filepath) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test: Seeking with Fresh Demuxer (No Cache)" << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::vector<uint64_t> test_positions = {0, 30000, 60000, 120000, 180000};
    
    int seeks_ok = 0;
    int seeks_total = 0;
    
    std::cout << "\nEach seek uses a fresh demuxer instance..." << std::endl;
    std::cout << std::setw(12) << "Target" << std::setw(12) << "Actual" 
              << std::setw(10) << "Diff" << std::setw(10) << "Status" << std::endl;
    std::cout << std::string(44, '-') << std::endl;
    
    for (uint64_t target : test_positions) {
        // Create fresh demuxer for each seek
        MiniPlayer player;
        
        if (!player.open(filepath)) {
            std::cerr << "Failed to open file" << std::endl;
            continue;
        }
        
        if (target >= player.getDuration()) {
            player.close();
            continue;
        }
        
        auto result = player.seek(target);
        seeks_total++;
        
        if (result.within_tolerance) {
            seeks_ok++;
        }
        
        std::cout << std::setw(12) << target 
                  << std::setw(12) << result.actual_ms
                  << std::setw(10) << result.diff_ms
                  << std::setw(10) << (result.within_tolerance ? "OK" : "EXCEEDED")
                  << std::endl;
        
        player.close();
    }
    
    double success_rate = (seeks_total > 0) ? 
        (static_cast<double>(seeks_ok) / seeks_total * 100.0) : 0.0;
    
    std::cout << "\n--- Summary ---" << std::endl;
    std::cout << "Seeks within tolerance: " << seeks_ok << "/" << seeks_total << std::endl;
    std::cout << "Success rate: " << std::fixed << std::setprecision(1) 
              << success_rate << "%" << std::endl;
    
    bool passed = (success_rate >= 80.0);
    std::cout << "\nTest " << (passed ? "PASSED" : "FAILED") << std::endl;
    
    return passed;
}

/**
 * @brief Test RADIO GA GA.flac specifically (Task 7.2)
 */
static bool testRadioGaGa() {
    const std::string filepath = "tests/data/RADIO GA GA.flac";
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Task 7.2: RADIO GA GA.flac Test" << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::ifstream file(filepath);
    if (!file.good()) {
        std::cout << "RADIO GA GA.flac not found, skipping" << std::endl;
        return true;  // Not a failure if file doesn't exist
    }
    file.close();
    
    MiniPlayer player;
    
    // Test 1: Open and verify metadata
    std::cout << "\n1. Opening file..." << std::endl;
    if (!player.open(filepath)) {
        std::cerr << "FAILED: Could not open file" << std::endl;
        return false;
    }
    std::cout << "   PASSED" << std::endl;
    
    // Test 2: Play first 5 seconds
    std::cout << "\n2. Playing first 5 seconds..." << std::endl;
    int frames = player.play(5000);
    if (frames > 0) {
        std::cout << "   PASSED: Read " << frames << " frames" << std::endl;
    } else {
        std::cerr << "   FAILED: No frames read" << std::endl;
        return false;
    }
    
    // Test 3: Seek to various positions
    std::cout << "\n3. Testing seeks throughout track..." << std::endl;
    
    struct SeekTest {
        std::string name;
        uint64_t position_ms;
    };
    
    std::vector<SeekTest> seek_tests = {
        {"Beginning", 0},
        {"30 seconds", 30000},
        {"1 minute", 60000},
        {"2 minutes", 120000},
        {"3 minutes", 180000},
        {"4 minutes", 240000},
        {"5 minutes", 300000}
    };
    
    int seeks_ok = 0;
    uint64_t duration = player.getDuration();
    
    for (const auto& test : seek_tests) {
        if (test.position_ms >= duration) continue;
        
        auto result = player.seek(test.position_ms);
        
        std::cout << "   " << test.name << " (" << test.position_ms << "ms): ";
        if (result.success && result.within_tolerance) {
            std::cout << "OK (diff: " << result.diff_ms << "ms)" << std::endl;
            seeks_ok++;
        } else if (result.success) {
            std::cout << "EXCEEDED (diff: " << result.diff_ms << "ms)" << std::endl;
        } else {
            std::cout << "FAILED" << std::endl;
        }
        
        // Read a frame after each seek
        player.play(100);
    }
    
    // Test 4: Verify can play after seeking
    std::cout << "\n4. Playing after final seek..." << std::endl;
    player.seek(duration / 2);
    frames = player.play(2000);
    if (frames > 0) {
        std::cout << "   PASSED: Read " << frames << " frames" << std::endl;
    } else {
        std::cerr << "   FAILED: No frames read after seek" << std::endl;
        return false;
    }
    
    player.close();
    
    // Overall result
    int total_seeks = std::min(static_cast<int>(seek_tests.size()), 
                               static_cast<int>(duration / 60000) + 2);
    double success_rate = (total_seeks > 0) ? 
        (static_cast<double>(seeks_ok) / total_seeks * 100.0) : 0.0;
    
    std::cout << "\n--- Summary ---" << std::endl;
    std::cout << "Seeks within tolerance: " << seeks_ok << "/" << total_seeks << std::endl;
    std::cout << "Success rate: " << std::fixed << std::setprecision(1) 
              << success_rate << "%" << std::endl;
    
    bool passed = (success_rate >= 70.0);  // 70% threshold for this specific test
    std::cout << "\nRADIO GA GA.flac test: " << (passed ? "PASSED" : "FAILED") << std::endl;
    
    return passed;
}

/**
 * @brief Check if a file exists
 */
static bool fileExists(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

int main(int argc, char* argv[]) {
    std::cout << "FLAC Mini Player Test" << std::endl;
    std::cout << "=====================" << std::endl;
    std::cout << "Testing FLAC pipeline with seeking (Task 7.1, 7.2)" << std::endl;
    std::cout << "Tolerance: " << SEEK_TOLERANCE_MS << "ms" << std::endl;
    
    // Determine which file to test
    std::string test_file;
    
    if (argc > 1) {
        test_file = argv[1];
    } else {
        // Try to find a test file
        std::vector<std::string> candidates = {
            "tests/data/RADIO GA GA.flac",
            "tests/data/04 Time.flac",
            "tests/data/11 Everlong.flac",
            "tests/data/11 life goes by.flac"
        };
        
        for (const auto& path : candidates) {
            if (fileExists(path)) {
                test_file = path;
                break;
            }
        }
    }
    
    if (test_file.empty()) {
        std::cout << "\nNo test files found. Please provide a FLAC file as argument." << std::endl;
        return 0;
    }
    
    std::cout << "\nUsing test file: " << test_file << std::endl;
    
    int tests_run = 0;
    int tests_passed = 0;
    
    // Run tests
    if (testBasicPlaybackAndSeeking(test_file)) tests_passed++;
    tests_run++;
    
    if (testSeekingScrubbing(test_file)) tests_passed++;
    tests_run++;
    
    if (testSeekingWithFreshDemuxer(test_file)) tests_passed++;
    tests_run++;
    
    // Run RADIO GA GA specific test
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

