/*
 * test_flac_diagnostic.cpp - Detailed diagnostic test for FLACDemuxer
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include <iostream>
#include <fstream>
#include <iomanip>

// Test file path
const char* TEST_FLAC_FILE = "/mnt/8TB-3/music/almost monday/DIVE/11 life goes by.flac";

/**
 * @brief Check if test file exists and get basic info
 */
bool analyzeTestFile() {
    std::cout << "=== FLAC File Analysis ===" << std::endl;
    std::cout << "File: " << TEST_FLAC_FILE << std::endl;
    
    std::ifstream file(TEST_FLAC_FILE, std::ios::binary);
    if (!file) {
        std::cout << "ERROR: Cannot open test file" << std::endl;
        return false;
    }
    
    // Get file size
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::cout << "File size: " << file_size << " bytes (" << (file_size / 1024.0 / 1024.0) << " MB)" << std::endl;
    
    // Read first 16 bytes to check FLAC marker
    uint8_t header[16];
    file.read(reinterpret_cast<char*>(header), 16);
    
    std::cout << "First 16 bytes (hex): ";
    for (int i = 0; i < 16; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(header[i]) << " ";
    }
    std::cout << std::dec << std::endl;
    
    // Check for fLaC marker
    if (header[0] == 'f' && header[1] == 'L' && header[2] == 'a' && header[3] == 'C') {
        std::cout << "✓ Valid fLaC stream marker found" << std::endl;
    } else {
        std::cout << "✗ Invalid or missing fLaC stream marker" << std::endl;
        std::cout << "Expected: 66 4C 61 43 (fLaC)" << std::endl;
        std::cout << "Found:    " << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(header[0]) << " " << static_cast<int>(header[1]) << " "
                  << static_cast<int>(header[2]) << " " << static_cast<int>(header[3]) << std::dec << std::endl;
        return false;
    }
    
    // Analyze first metadata block header
    std::cout << "\nFirst metadata block header:" << std::endl;
    uint8_t block_type = header[4] & 0x7F;
    bool is_last = (header[4] & 0x80) != 0;
    uint32_t block_length = (static_cast<uint32_t>(header[5]) << 16) |
                           (static_cast<uint32_t>(header[6]) << 8) |
                           static_cast<uint32_t>(header[7]);
    
    std::cout << "Block type: " << static_cast<int>(block_type) << " (should be 0 for STREAMINFO)" << std::endl;
    std::cout << "Is last block: " << (is_last ? "yes" : "no") << std::endl;
    std::cout << "Block length: " << block_length << " bytes (should be 34 for STREAMINFO)" << std::endl;
    
    if (block_type == 0 && block_length == 34) {
        std::cout << "✓ Valid STREAMINFO metadata block header" << std::endl;
    } else {
        std::cout << "✗ Invalid STREAMINFO metadata block header" << std::endl;
    }
    
    return true;
}

/**
 * @brief Test basic IOHandler functionality with the file
 */
bool testIOHandler() {
    std::cout << "\n=== IOHandler Test ===" << std::endl;
    
    try {
        auto handler = std::make_unique<FileIOHandler>(TEST_FLAC_FILE);
        std::cout << "✓ FileIOHandler created successfully" << std::endl;
        
        // Test basic operations
        uint8_t buffer[16];
        size_t bytes_read = handler->read(buffer, 1, 16);
        std::cout << "Read " << bytes_read << " bytes" << std::endl;
        
        if (bytes_read >= 4) {
            if (buffer[0] == 'f' && buffer[1] == 'L' && buffer[2] == 'a' && buffer[3] == 'C') {
                std::cout << "✓ IOHandler correctly reads fLaC marker" << std::endl;
            } else {
                std::cout << "✗ IOHandler read incorrect data" << std::endl;
                return false;
            }
        }
        
        // Test seeking
        if (handler->seek(0, SEEK_SET) == 0) {
            std::cout << "✓ IOHandler seek works" << std::endl;
        } else {
            std::cout << "✗ IOHandler seek failed" << std::endl;
            return false;
        }
        
        // Test tell
        off_t pos = handler->tell();
        std::cout << "Current position: " << pos << std::endl;
        
        // Test file size
        off_t size = handler->getFileSize();
        std::cout << "File size via IOHandler: " << size << " bytes" << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "✗ IOHandler test failed: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Test FLACDemuxer construction without parsing
 */
bool testFLACDemuxerConstruction() {
    std::cout << "\n=== FLACDemuxer Construction Test ===" << std::endl;
    
    try {
        auto handler = std::make_unique<FileIOHandler>(TEST_FLAC_FILE);
        std::cout << "✓ IOHandler created" << std::endl;
        
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        std::cout << "✓ FLACDemuxer constructed successfully" << std::endl;
        
        // Test initial state
        std::cout << "Initial state:" << std::endl;
        std::cout << "  Duration: " << demuxer->getDuration() << " ms" << std::endl;
        std::cout << "  Position: " << demuxer->getPosition() << " ms" << std::endl;
        std::cout << "  EOF: " << (demuxer->isEOF() ? "yes" : "no") << std::endl;
        
        auto streams = demuxer->getStreams();
        std::cout << "  Streams: " << streams.size() << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "✗ FLACDemuxer construction failed: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Test FLACDemuxer parsing step by step
 */
bool testFLACDemuxerParsing() {
    std::cout << "\n=== FLACDemuxer Parsing Test ===" << std::endl;
    
    try {
        auto handler = std::make_unique<FileIOHandler>(TEST_FLAC_FILE);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        std::cout << "✓ FLACDemuxer created" << std::endl;
        
        std::cout << "Attempting to parse container..." << std::endl;
        
        // This is where the issue likely occurs
        bool parse_result = demuxer->parseContainer();
        
        if (parse_result) {
            std::cout << "✓ Container parsed successfully" << std::endl;
            
            // Test parsed state
            std::cout << "Parsed state:" << std::endl;
            std::cout << "  Duration: " << demuxer->getDuration() << " ms" << std::endl;
            std::cout << "  Position: " << demuxer->getPosition() << " ms" << std::endl;
            
            auto streams = demuxer->getStreams();
            std::cout << "  Streams: " << streams.size() << std::endl;
            
            if (!streams.empty()) {
                const auto& stream = streams[0];
                std::cout << "  Stream 0:" << std::endl;
                std::cout << "    ID: " << stream.stream_id << std::endl;
                std::cout << "    Codec: " << stream.codec_name << std::endl;
                std::cout << "    Sample rate: " << stream.sample_rate << " Hz" << std::endl;
                std::cout << "    Channels: " << stream.channels << std::endl;
                std::cout << "    Bits per sample: " << stream.bits_per_sample << std::endl;
                std::cout << "    Duration: " << stream.duration_ms << " ms" << std::endl;
            }
            
            return true;
        } else {
            std::cout << "✗ Container parsing failed" << std::endl;
            return false;
        }
        
    } catch (const std::exception& e) {
        std::cout << "✗ FLACDemuxer parsing failed with exception: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Test seeking functionality if parsing succeeds
 */
bool testFLACDemuxerSeeking() {
    std::cout << "\n=== FLACDemuxer Seeking Test ===" << std::endl;
    
    try {
        auto handler = std::make_unique<FileIOHandler>(TEST_FLAC_FILE);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        if (!demuxer->parseContainer()) {
            std::cout << "Skipping seeking test - parsing failed" << std::endl;
            return false;
        }
        
        std::cout << "✓ Container parsed for seeking test" << std::endl;
        
        // Test seeking to beginning
        std::cout << "Testing seek to beginning..." << std::endl;
        if (demuxer->seekTo(0)) {
            std::cout << "✓ Seek to beginning successful" << std::endl;
            std::cout << "  Position after seek: " << demuxer->getPosition() << " ms" << std::endl;
        } else {
            std::cout << "✗ Seek to beginning failed" << std::endl;
        }
        
        // Test seeking to middle
        uint64_t duration = demuxer->getDuration();
        if (duration > 0) {
            uint64_t middle = duration / 2;
            std::cout << "Testing seek to middle (" << middle << " ms)..." << std::endl;
            if (demuxer->seekTo(middle)) {
                std::cout << "✓ Seek to middle successful" << std::endl;
                std::cout << "  Position after seek: " << demuxer->getPosition() << " ms" << std::endl;
            } else {
                std::cout << "✗ Seek to middle failed" << std::endl;
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "✗ Seeking test failed with exception: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Test frame reading if parsing succeeds
 */
bool testFLACDemuxerFrameReading() {
    std::cout << "\n=== FLACDemuxer Frame Reading Test ===" << std::endl;
    
    try {
        auto handler = std::make_unique<FileIOHandler>(TEST_FLAC_FILE);
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        if (!demuxer->parseContainer()) {
            std::cout << "Skipping frame reading test - parsing failed" << std::endl;
            return false;
        }
        
        std::cout << "✓ Container parsed for frame reading test" << std::endl;
        
        // Try to read first frame
        std::cout << "Attempting to read first frame..." << std::endl;
        auto chunk = demuxer->readChunk();
        
        if (chunk.isValid()) {
            std::cout << "✓ First frame read successfully" << std::endl;
            std::cout << "  Stream ID: " << chunk.stream_id << std::endl;
            std::cout << "  Data size: " << chunk.data.size() << " bytes" << std::endl;
            std::cout << "  Timestamp: " << chunk.timestamp_samples << " samples" << std::endl;
            std::cout << "  Is keyframe: " << (chunk.is_keyframe ? "yes" : "no") << std::endl;
        } else {
            std::cout << "✗ First frame read failed" << std::endl;
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "✗ Frame reading test failed with exception: " << e.what() << std::endl;
        return false;
    }
}

int main() {
    std::cout << "FLAC Demuxer Diagnostic Test" << std::endl;
    std::cout << "============================" << std::endl;
    std::cout << "Using RFC 9639 as reference specification" << std::endl;
    std::cout << std::endl;
    
    bool all_passed = true;
    
    // Run diagnostic tests in order
    if (!analyzeTestFile()) {
        std::cout << "File analysis failed - cannot continue" << std::endl;
        return 1;
    }
    
    if (!testIOHandler()) {
        std::cout << "IOHandler test failed" << std::endl;
        all_passed = false;
    }
    
    if (!testFLACDemuxerConstruction()) {
        std::cout << "FLACDemuxer construction failed" << std::endl;
        all_passed = false;
    }
    
    if (!testFLACDemuxerParsing()) {
        std::cout << "FLACDemuxer parsing failed" << std::endl;
        all_passed = false;
    } else {
        // Only test these if parsing succeeded
        testFLACDemuxerSeeking();
        testFLACDemuxerFrameReading();
    }
    
    std::cout << "\n=== Diagnostic Summary ===" << std::endl;
    if (all_passed) {
        std::cout << "✓ All basic tests passed" << std::endl;
        std::cout << "FLACDemuxer appears to be working correctly" << std::endl;
        return 0;
    } else {
        std::cout << "✗ Some tests failed" << std::endl;
        std::cout << "FLACDemuxer needs debugging and fixes" << std::endl;
        return 1;
    }
}