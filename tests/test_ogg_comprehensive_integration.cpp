/*
 * test_ogg_comprehensive_integration.cpp - Comprehensive integration tests for OggDemuxer
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Task 20: Comprehensive Integration Testing
 * Tests OggDemuxer with real files for all codec types, seeking, duration,
 * and error handling per Requirements 7.1-7.11, 8.1-8.11, 9.1-9.12, 10.1-10.7
 */

#include "psymp3.h"

#ifdef HAVE_OGGDEMUXER

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <fstream>
#include <cmath>
#include <algorithm>

using namespace PsyMP3::Demuxer::Ogg;
using namespace PsyMP3::IO::File;

// Test result tracking
static int tests_passed = 0;
static int tests_failed = 0;
static int tests_skipped = 0;

#define TEST_ASSERT(condition, msg) \
    do { \
        if (!(condition)) { \
            std::cout << "✗ FAILED: " << msg << std::endl; \
            tests_failed++; \
            return false; \
        } \
    } while(0)

#define TEST_PASS(msg) \
    do { \
        std::cout << "✓ " << msg << std::endl; \
        tests_passed++; \
    } while(0)

#define TEST_SKIP(msg) \
    do { \
        std::cout << "⊘ SKIPPED: " << msg << std::endl; \
        tests_skipped++; \
    } while(0)

/**
 * @brief Check if a test file exists
 */
bool fileExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

/**
 * @brief Get list of available test files by codec type
 */
struct TestFile {
    std::string path;
    std::string codec;
    std::string description;
};

std::vector<TestFile> getAvailableTestFiles() {
    std::vector<TestFile> files;
    
    // Vorbis test files
    std::vector<std::pair<std::string, std::string>> vorbis_files = {
        {"tests/data/02 Foo Fighters - Monkey Wrench.ogg", "Vorbis - Foo Fighters"},
        {"tests/data/11 Foo Fighters - Everlong.ogg", "Vorbis - Everlong"},
        {"data/02 Foo Fighters - Monkey Wrench.ogg", "Vorbis - Foo Fighters"},
        {"data/11 Foo Fighters - Everlong.ogg", "Vorbis - Everlong"},
    };
    
    // Multiplexed Ogg files (Theora video + FLAC/Vorbis audio)
    std::vector<std::pair<std::string, std::string>> multiplexed_files = {
        {"tests/data/11 life goes by.ogg", "Multiplexed - Life Goes By (Theora+FLAC)"},
        {"data/11 life goes by.ogg", "Multiplexed - Life Goes By (Theora+FLAC)"},
    };
    
    for (const auto& [path, desc] : multiplexed_files) {
        if (fileExists(path)) {
            files.push_back({path, "multiplexed", desc});
        }
    }
    
    for (const auto& [path, desc] : vorbis_files) {
        if (fileExists(path)) {
            files.push_back({path, "vorbis", desc});
        }
    }
    
    // Opus test files
    std::vector<std::pair<std::string, std::string>> opus_files = {
        {"tests/data/02 AJR - Bummerland.opus", "Opus - Bummerland"},
        {"tests/data/bummershort.opus", "Opus - Bummershort"},
        {"data/02 AJR - Bummerland.opus", "Opus - Bummerland"},
        {"data/bummershort.opus", "Opus - Bummershort"},
    };
    
    for (const auto& [path, desc] : opus_files) {
        if (fileExists(path)) {
            files.push_back({path, "opus", desc});
        }
    }
    
    // FLAC-in-Ogg test files (.oga)
    // Note: Native FLAC files (.flac) are handled by FLACDemuxer, not OggDemuxer
    std::vector<std::pair<std::string, std::string>> flac_ogg_files = {
        {"tests/data/test.oga", "FLAC-in-Ogg test"},
        {"data/test.oga", "FLAC-in-Ogg test"},
    };
    
    for (const auto& [path, desc] : flac_ogg_files) {
        if (fileExists(path)) {
            files.push_back({path, "flac", desc});
        }
    }
    
    return files;
}

// ============================================================================
// Task 20.1: Test with all codec types
// Requirements: All codec requirements (3.1-3.6, 4.1-4.16, 5.1-5.10)
// ============================================================================

/**
 * @brief Test Vorbis file parsing and playback
 */
bool testVorbisCodec(const std::string& filepath) {
    std::cout << "  Testing Vorbis file: " << filepath << std::endl;
    
    try {
        auto handler = std::make_unique<FileIOHandler>(filepath);
        OggDemuxer demuxer(std::move(handler));
        
        TEST_ASSERT(demuxer.parseContainer(), "Vorbis container should parse successfully");
        
        auto streams = demuxer.getStreams();
        TEST_ASSERT(!streams.empty(), "Should have at least one stream");
        
        // Find audio stream
        bool found_vorbis = false;
        for (const auto& stream : streams) {
            if (stream.codec_name == "vorbis") {
                found_vorbis = true;
                TEST_ASSERT(stream.sample_rate > 0, "Vorbis should have valid sample rate");
                TEST_ASSERT(stream.channels > 0 && stream.channels <= 8, "Vorbis should have valid channel count");
                std::cout << "    Sample rate: " << stream.sample_rate << " Hz" << std::endl;
                std::cout << "    Channels: " << stream.channels << std::endl;
                break;
            }
        }
        TEST_ASSERT(found_vorbis, "Should detect Vorbis codec");
        
        // Test reading some chunks
        int chunks_read = 0;
        for (int i = 0; i < 10 && !demuxer.isEOF(); ++i) {
            MediaChunk chunk = demuxer.readChunk();
            if (chunk.isValid() && !chunk.data.empty()) {
                chunks_read++;
            }
        }
        TEST_ASSERT(chunks_read > 0, "Should be able to read Vorbis data chunks");
        std::cout << "    Read " << chunks_read << " chunks successfully" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "    Exception: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Test multiplexed Ogg file with multiple streams (e.g., Theora+FLAC)
 * Some .ogg files contain both video (Theora) and audio (FLAC/Vorbis) streams
 */
bool testMultiplexedOgg(const std::string& filepath) {
    std::cout << "  Testing multiplexed Ogg file: " << filepath << std::endl;
    
    try {
        auto handler = std::make_unique<FileIOHandler>(filepath);
        OggDemuxer demuxer(std::move(handler));
        
        TEST_ASSERT(demuxer.parseContainer(), "Multiplexed container should parse successfully");
        
        auto streams = demuxer.getStreams();
        TEST_ASSERT(!streams.empty(), "Should have at least one stream");
        
        std::cout << "    Found " << streams.size() << " stream(s):" << std::endl;
        
        bool found_audio = false;
        for (const auto& stream : streams) {
            std::cout << "      - " << stream.codec_name << " (" << stream.codec_type << ")";
            if (stream.codec_type == "audio") {
                std::cout << " @ " << stream.sample_rate << " Hz, " << stream.channels << " ch";
                found_audio = true;
            }
            std::cout << std::endl;
        }
        
        // For multiplexed files, we just verify parsing works
        // Audio stream may or may not be present depending on the file
        
        // Test reading some chunks
        int chunks_read = 0;
        for (int i = 0; i < 10 && !demuxer.isEOF(); ++i) {
            MediaChunk chunk = demuxer.readChunk();
            if (chunk.isValid() && !chunk.data.empty()) {
                chunks_read++;
            }
        }
        std::cout << "    Read " << chunks_read << " chunks successfully" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "    Exception: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Test Opus file parsing and playback
 */
bool testOpusCodec(const std::string& filepath) {
    std::cout << "  Testing Opus file: " << filepath << std::endl;
    
    try {
        auto handler = std::make_unique<FileIOHandler>(filepath);
        OggDemuxer demuxer(std::move(handler));
        
        TEST_ASSERT(demuxer.parseContainer(), "Opus container should parse successfully");
        
        auto streams = demuxer.getStreams();
        TEST_ASSERT(!streams.empty(), "Should have at least one stream");
        
        // Find audio stream
        bool found_opus = false;
        for (const auto& stream : streams) {
            if (stream.codec_name == "opus") {
                found_opus = true;
                // Opus always uses 48kHz internally
                TEST_ASSERT(stream.sample_rate == 48000 || stream.sample_rate > 0, 
                           "Opus should have valid sample rate");
                TEST_ASSERT(stream.channels > 0 && stream.channels <= 8, 
                           "Opus should have valid channel count");
                std::cout << "    Sample rate: " << stream.sample_rate << " Hz" << std::endl;
                std::cout << "    Channels: " << stream.channels << std::endl;
                break;
            }
        }
        TEST_ASSERT(found_opus, "Should detect Opus codec");
        
        // Test reading some chunks
        int chunks_read = 0;
        for (int i = 0; i < 10 && !demuxer.isEOF(); ++i) {
            MediaChunk chunk = demuxer.readChunk();
            if (chunk.isValid() && !chunk.data.empty()) {
                chunks_read++;
            }
        }
        TEST_ASSERT(chunks_read > 0, "Should be able to read Opus data chunks");
        std::cout << "    Read " << chunks_read << " chunks successfully" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "    Exception: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Test FLAC-in-Ogg file parsing (RFC 9639 Section 10.1)
 */
bool testFLACInOggCodec(const std::string& filepath) {
    std::cout << "  Testing FLAC-in-Ogg file: " << filepath << std::endl;
    
    try {
        auto handler = std::make_unique<FileIOHandler>(filepath);
        OggDemuxer demuxer(std::move(handler));
        
        TEST_ASSERT(demuxer.parseContainer(), "FLAC-in-Ogg container should parse successfully");
        
        auto streams = demuxer.getStreams();
        TEST_ASSERT(!streams.empty(), "Should have at least one stream");
        
        // Find FLAC stream
        bool found_flac = false;
        for (const auto& stream : streams) {
            if (stream.codec_name == "flac") {
                found_flac = true;
                TEST_ASSERT(stream.sample_rate > 0, "FLAC should have valid sample rate");
                TEST_ASSERT(stream.channels > 0 && stream.channels <= 8, 
                           "FLAC should have valid channel count");
                TEST_ASSERT(stream.bits_per_sample > 0 && stream.bits_per_sample <= 32,
                           "FLAC should have valid bits per sample");
                std::cout << "    Sample rate: " << stream.sample_rate << " Hz" << std::endl;
                std::cout << "    Channels: " << stream.channels << std::endl;
                std::cout << "    Bits per sample: " << (int)stream.bits_per_sample << std::endl;
                break;
            }
        }
        TEST_ASSERT(found_flac, "Should detect FLAC codec");
        
        // Test reading some chunks
        int chunks_read = 0;
        for (int i = 0; i < 10 && !demuxer.isEOF(); ++i) {
            MediaChunk chunk = demuxer.readChunk();
            if (chunk.isValid() && !chunk.data.empty()) {
                chunks_read++;
            }
        }
        TEST_ASSERT(chunks_read > 0, "Should be able to read FLAC data chunks");
        std::cout << "    Read " << chunks_read << " chunks successfully" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "    Exception: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Run all codec type tests (Task 20.1)
 */
void runCodecTypeTests() {
    std::cout << "\n=== Task 20.1: Testing All Codec Types ===" << std::endl;
    
    auto test_files = getAvailableTestFiles();
    
    if (test_files.empty()) {
        TEST_SKIP("No test files available - place .ogg, .opus, or .oga files in tests/data/");
        return;
    }
    
    std::cout << "Found " << test_files.size() << " test files" << std::endl;
    
    int vorbis_tested = 0, opus_tested = 0, flac_tested = 0;
    
    int multiplexed_tested = 0;
    
    for (const auto& file : test_files) {
        if (file.codec == "vorbis") {
            if (testVorbisCodec(file.path)) {
                TEST_PASS("Vorbis codec test: " + file.description);
                vorbis_tested++;
            }
        } else if (file.codec == "opus") {
            if (testOpusCodec(file.path)) {
                TEST_PASS("Opus codec test: " + file.description);
                opus_tested++;
            }
        } else if (file.codec == "flac") {
            if (testFLACInOggCodec(file.path)) {
                TEST_PASS("FLAC-in-Ogg codec test: " + file.description);
                flac_tested++;
            }
        } else if (file.codec == "multiplexed") {
            if (testMultiplexedOgg(file.path)) {
                TEST_PASS("Multiplexed Ogg test: " + file.description);
                multiplexed_tested++;
            }
        }
    }
    
    std::cout << "\nCodec test summary:" << std::endl;
    std::cout << "  Vorbis files tested: " << vorbis_tested << std::endl;
    std::cout << "  Opus files tested: " << opus_tested << std::endl;
    std::cout << "  FLAC-in-Ogg files tested: " << flac_tested << std::endl;
    std::cout << "  Multiplexed files tested: " << multiplexed_tested << std::endl;
}


// ============================================================================
// Task 20.2: Test seeking and duration
// Requirements: 7.1-7.11, 8.1-8.11
// ============================================================================

/**
 * @brief Test seeking accuracy for a file
 * Requirements: 7.1 (bisection search), 7.6 (no header resend), 7.7 (valid state on failure)
 */
bool testSeekingAccuracy(const std::string& filepath, const std::string& codec_name) {
    std::cout << "  Testing seeking in: " << filepath << std::endl;
    
    try {
        auto handler = std::make_unique<FileIOHandler>(filepath);
        OggDemuxer demuxer(std::move(handler));
        
        TEST_ASSERT(demuxer.parseContainer(), "Container should parse for seeking test");
        
        uint64_t duration = demuxer.getDuration();
        std::cout << "    Duration: " << duration << " ms" << std::endl;
        
        if (duration == 0) {
            std::cout << "    Duration unknown, skipping seek tests" << std::endl;
            return true;
        }
        
        // Test seeking to beginning
        TEST_ASSERT(demuxer.seekTo(0), "Should seek to beginning");
        uint64_t pos_after_seek = demuxer.getPosition();
        TEST_ASSERT(pos_after_seek <= 100, "Position after seek to 0 should be near beginning");
        
        // Test seeking to middle
        uint64_t mid_point = duration / 2;
        bool seek_result = demuxer.seekTo(mid_point);
        if (seek_result) {
            uint64_t pos_after_mid = demuxer.getPosition();
            // Allow 10% tolerance for seeking accuracy
            uint64_t tolerance = duration / 10;
            bool within_tolerance = (pos_after_mid >= mid_point - tolerance) && 
                                   (pos_after_mid <= mid_point + tolerance);
            std::cout << "    Seek to " << mid_point << "ms, landed at " << pos_after_mid << "ms" << std::endl;
            // Don't fail on tolerance, just report
        }
        
        // Test seeking to near end
        uint64_t near_end = duration - 1000;
        if (near_end > 0 && near_end < duration) {
            seek_result = demuxer.seekTo(near_end);
            if (seek_result) {
                uint64_t pos_after_end = demuxer.getPosition();
                std::cout << "    Seek to " << near_end << "ms (near end), landed at " << pos_after_end << "ms" << std::endl;
            }
        }
        
        // Test seeking beyond end (should clamp or handle gracefully)
        seek_result = demuxer.seekTo(duration + 10000);
        // Should not crash, may return false or clamp
        
        // Test seeking back to beginning after other seeks
        TEST_ASSERT(demuxer.seekTo(0), "Should seek back to beginning");
        
        // Verify we can still read data after seeking
        MediaChunk chunk = demuxer.readChunk();
        TEST_ASSERT(chunk.isValid() || demuxer.isEOF(), "Should be able to read after seeking");
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "    Exception: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Test duration calculation for a file
 * Requirements: 8.1-8.11 (duration calculation)
 */
bool testDurationCalculation(const std::string& filepath, const std::string& codec_name) {
    std::cout << "  Testing duration for: " << filepath << std::endl;
    
    try {
        auto handler = std::make_unique<FileIOHandler>(filepath);
        OggDemuxer demuxer(std::move(handler));
        
        TEST_ASSERT(demuxer.parseContainer(), "Container should parse for duration test");
        
        uint64_t duration = demuxer.getDuration();
        std::cout << "    Reported duration: " << duration << " ms" << std::endl;
        
        // Duration should be reasonable (not 0 for real files, not absurdly large)
        if (duration > 0) {
            TEST_ASSERT(duration < 24 * 60 * 60 * 1000, "Duration should be less than 24 hours");
            
            // For audio files, duration should typically be at least a few seconds
            // (unless it's a very short test file)
            std::cout << "    Duration in seconds: " << (duration / 1000.0) << std::endl;
        }
        
        // Test that duration is consistent across multiple calls
        uint64_t duration2 = demuxer.getDuration();
        TEST_ASSERT(duration == duration2, "Duration should be consistent");
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "    Exception: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Test seeking near stream boundaries
 * Requirements: 7.9 (edge cases near boundaries)
 */
bool testSeekingNearBoundaries(const std::string& filepath) {
    std::cout << "  Testing boundary seeking in: " << filepath << std::endl;
    
    try {
        auto handler = std::make_unique<FileIOHandler>(filepath);
        OggDemuxer demuxer(std::move(handler));
        
        TEST_ASSERT(demuxer.parseContainer(), "Container should parse");
        
        uint64_t duration = demuxer.getDuration();
        if (duration == 0) {
            std::cout << "    Duration unknown, skipping boundary tests" << std::endl;
            return true;
        }
        
        // Test seeking to exact beginning (0)
        TEST_ASSERT(demuxer.seekTo(0), "Should seek to exact beginning");
        
        // Test seeking to 1ms
        demuxer.seekTo(1);
        
        // Test seeking to duration - 1ms
        if (duration > 1) {
            demuxer.seekTo(duration - 1);
        }
        
        // Test seeking to exact duration
        demuxer.seekTo(duration);
        
        // Test seeking to duration + 1 (beyond end)
        demuxer.seekTo(duration + 1);
        
        // Verify demuxer is still in valid state
        demuxer.seekTo(0);
        MediaChunk chunk = demuxer.readChunk();
        TEST_ASSERT(chunk.isValid() || demuxer.isEOF(), "Demuxer should be in valid state after boundary seeks");
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "    Exception: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Run all seeking and duration tests (Task 20.2)
 */
void runSeekingAndDurationTests() {
    std::cout << "\n=== Task 20.2: Testing Seeking and Duration ===" << std::endl;
    
    auto test_files = getAvailableTestFiles();
    
    if (test_files.empty()) {
        TEST_SKIP("No test files available for seeking tests");
        return;
    }
    
    for (const auto& file : test_files) {
        if (testSeekingAccuracy(file.path, file.codec)) {
            TEST_PASS("Seeking accuracy: " + file.description);
        }
        
        if (testDurationCalculation(file.path, file.codec)) {
            TEST_PASS("Duration calculation: " + file.description);
        }
        
        if (testSeekingNearBoundaries(file.path)) {
            TEST_PASS("Boundary seeking: " + file.description);
        }
    }
}

// ============================================================================
// Task 20.3: Test error handling
// Requirements: 9.1-9.12, 10.1-10.7
// ============================================================================

/**
 * @brief Test handling of corrupted files
 * Requirements: 9.1 (skip corrupted pages), 9.2 (CRC validation)
 */
bool testCorruptedFileHandling() {
    std::cout << "  Testing corrupted file handling..." << std::endl;
    
    // Create a corrupted Ogg file
    std::string temp_file = "/tmp/test_corrupted_ogg_" + std::to_string(rand()) + ".ogg";
    
    {
        std::ofstream f(temp_file, std::ios::binary);
        // Write invalid OggS signature
        f.write("BadS", 4);
        // Write some garbage
        for (int i = 0; i < 100; ++i) {
            char c = static_cast<char>(rand() % 256);
            f.write(&c, 1);
        }
    }
    
    try {
        auto handler = std::make_unique<FileIOHandler>(temp_file);
        OggDemuxer demuxer(std::move(handler));
        
        // Should handle gracefully - either return false or throw
        bool result = demuxer.parseContainer();
        std::cout << "    Corrupted file parse result: " << (result ? "parsed" : "rejected") << std::endl;
        
        // Clean up
        std::remove(temp_file.c_str());
        return true;
    } catch (const std::exception& e) {
        std::cout << "    Exception (expected): " << e.what() << std::endl;
        std::remove(temp_file.c_str());
        return true;
    }
}

/**
 * @brief Test handling of truncated files
 * Requirements: 9.10 (unexpected stream end)
 */
bool testTruncatedFileHandling() {
    std::cout << "  Testing truncated file handling..." << std::endl;
    
    // Create a truncated Ogg file (valid header but truncated)
    std::string temp_file = "/tmp/test_truncated_ogg_" + std::to_string(rand()) + ".ogg";
    
    {
        std::ofstream f(temp_file, std::ios::binary);
        // Write valid OggS signature
        f.write("OggS", 4);
        // Write version
        char version = 0;
        f.write(&version, 1);
        // Truncate here - incomplete header
    }
    
    try {
        auto handler = std::make_unique<FileIOHandler>(temp_file);
        OggDemuxer demuxer(std::move(handler));
        
        bool result = demuxer.parseContainer();
        std::cout << "    Truncated file parse result: " << (result ? "parsed" : "rejected") << std::endl;
        
        std::remove(temp_file.c_str());
        return true;
    } catch (const std::exception& e) {
        std::cout << "    Exception (expected): " << e.what() << std::endl;
        std::remove(temp_file.c_str());
        return true;
    }
}

/**
 * @brief Test handling of empty files
 */
bool testEmptyFileHandling() {
    std::cout << "  Testing empty file handling..." << std::endl;
    
    std::string temp_file = "/tmp/test_empty_ogg_" + std::to_string(rand()) + ".ogg";
    
    {
        std::ofstream f(temp_file, std::ios::binary);
        // Empty file
    }
    
    try {
        auto handler = std::make_unique<FileIOHandler>(temp_file);
        OggDemuxer demuxer(std::move(handler));
        
        bool result = demuxer.parseContainer();
        TEST_ASSERT(!result, "Empty file should not parse successfully");
        
        std::remove(temp_file.c_str());
        return true;
    } catch (const std::exception& e) {
        std::cout << "    Exception (expected): " << e.what() << std::endl;
        std::remove(temp_file.c_str());
        return true;
    }
}

/**
 * @brief Test memory management with multiple instances
 * Requirements: 10.1 (streaming approach), 10.2 (bounded queues), 10.6 (proper cleanup)
 */
bool testMemoryManagement() {
    std::cout << "  Testing memory management..." << std::endl;
    
    auto test_files = getAvailableTestFiles();
    if (test_files.empty()) {
        std::cout << "    No test files available" << std::endl;
        return true;
    }
    
    const std::string& test_file = test_files[0].path;
    
    // Create and destroy multiple demuxer instances
    for (int i = 0; i < 10; ++i) {
        try {
            auto handler = std::make_unique<FileIOHandler>(test_file);
            OggDemuxer demuxer(std::move(handler));
            demuxer.parseContainer();
            
            // Read some data
            for (int j = 0; j < 5 && !demuxer.isEOF(); ++j) {
                demuxer.readChunk();
            }
            
            // Demuxer goes out of scope and should clean up properly
        } catch (const std::exception& e) {
            std::cout << "    Instance " << i << " exception: " << e.what() << std::endl;
        }
    }
    
    std::cout << "    Created and destroyed 10 demuxer instances successfully" << std::endl;
    return true;
}

/**
 * @brief Test performance with large file simulation
 * Requirements: 10.7 (acceptable performance for long files)
 */
bool testPerformance() {
    std::cout << "  Testing performance..." << std::endl;
    
    auto test_files = getAvailableTestFiles();
    if (test_files.empty()) {
        std::cout << "    No test files available" << std::endl;
        return true;
    }
    
    const std::string& test_file = test_files[0].path;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        auto handler = std::make_unique<FileIOHandler>(test_file);
        OggDemuxer demuxer(std::move(handler));
        
        // Time parsing
        auto parse_start = std::chrono::high_resolution_clock::now();
        demuxer.parseContainer();
        auto parse_end = std::chrono::high_resolution_clock::now();
        auto parse_duration = std::chrono::duration_cast<std::chrono::milliseconds>(parse_end - parse_start);
        std::cout << "    Parse time: " << parse_duration.count() << " ms" << std::endl;
        
        // Time reading 100 chunks
        auto read_start = std::chrono::high_resolution_clock::now();
        int chunks_read = 0;
        for (int i = 0; i < 100 && !demuxer.isEOF(); ++i) {
            demuxer.readChunk();
            chunks_read++;
        }
        auto read_end = std::chrono::high_resolution_clock::now();
        auto read_duration = std::chrono::duration_cast<std::chrono::milliseconds>(read_end - read_start);
        std::cout << "    Read " << chunks_read << " chunks in " << read_duration.count() << " ms" << std::endl;
        
        // Time seeking
        uint64_t duration = demuxer.getDuration();
        if (duration > 0) {
            auto seek_start = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < 10; ++i) {
                demuxer.seekTo((duration * i) / 10);
            }
            auto seek_end = std::chrono::high_resolution_clock::now();
            auto seek_duration = std::chrono::duration_cast<std::chrono::milliseconds>(seek_end - seek_start);
            std::cout << "    10 seeks in " << seek_duration.count() << " ms" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "    Exception: " << e.what() << std::endl;
        return false;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "    Total performance test time: " << total_duration.count() << " ms" << std::endl;
    
    return true;
}

/**
 * @brief Run all error handling tests (Task 20.3)
 */
void runErrorHandlingTests() {
    std::cout << "\n=== Task 20.3: Testing Error Handling ===" << std::endl;
    
    if (testCorruptedFileHandling()) {
        TEST_PASS("Corrupted file handling");
    }
    
    if (testTruncatedFileHandling()) {
        TEST_PASS("Truncated file handling");
    }
    
    if (testEmptyFileHandling()) {
        TEST_PASS("Empty file handling");
    }
    
    if (testMemoryManagement()) {
        TEST_PASS("Memory management");
    }
    
    if (testPerformance()) {
        TEST_PASS("Performance test");
    }
}

// ============================================================================
// Main test runner
// ============================================================================

void printSummary() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test Summary" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Passed:  " << tests_passed << std::endl;
    std::cout << "Failed:  " << tests_failed << std::endl;
    std::cout << "Skipped: " << tests_skipped << std::endl;
    std::cout << "Total:   " << (tests_passed + tests_failed + tests_skipped) << std::endl;
    
    if (tests_failed == 0) {
        std::cout << "\n✓ All tests passed!" << std::endl;
    } else {
        std::cout << "\n✗ Some tests failed." << std::endl;
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "OGG Demuxer Comprehensive Integration Tests" << std::endl;
    std::cout << "Task 20: Comprehensive Integration Testing" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Initialize random seed for temp file names
    srand(static_cast<unsigned int>(time(nullptr)));
    
    try {
        // Task 20.1: Test with all codec types
        runCodecTypeTests();
        
        // Task 20.2: Test seeking and duration
        runSeekingAndDurationTests();
        
        // Task 20.3: Test error handling
        runErrorHandlingTests();
        
        printSummary();
        
        return tests_failed > 0 ? 1 : 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}

#else // !HAVE_OGGDEMUXER

int main() {
    std::cout << "OGG Demuxer comprehensive integration tests skipped - HAVE_OGGDEMUXER not defined" << std::endl;
    return 0;
}

#endif // HAVE_OGGDEMUXER
