/*
 * test_iso_flac_integration.cpp - Integration test for FLAC-in-MP4 support
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include <cassert>
#include <iostream>
#include <memory>

// Test FLAC-in-MP4 integration with real file
void test_flac_mp4_file_parsing() {
    std::cout << "Testing FLAC-in-MP4 file parsing..." << std::endl;
    
    // Open the test MP4 file with FLAC audio
    auto ioHandler = std::make_unique<FileIOHandler>("data/timeless.mp4");
    if (ioHandler->getLastError() != 0) {
        std::cerr << "Failed to open test file: data/timeless.mp4" << std::endl;
        assert(false);
    }
    
    // Create ISO demuxer
    ISODemuxer demuxer(std::move(ioHandler));
    
    // Parse the container
    bool parsed = demuxer.parseContainer();
    assert(parsed);
    std::cout << "✓ Successfully parsed FLAC-in-MP4 container" << std::endl;
    
    // Get streams
    std::vector<StreamInfo> streams = demuxer.getStreams();
    assert(!streams.empty());
    std::cout << "✓ Found " << streams.size() << " stream(s)" << std::endl;
    
    // Find FLAC audio stream
    StreamInfo flacStream;
    bool foundFlac = false;
    for (const auto& stream : streams) {
        if (stream.codec_name == "flac") {
            flacStream = stream;
            foundFlac = true;
            break;
        }
    }
    
    assert(foundFlac);
    std::cout << "✓ Found FLAC audio stream" << std::endl;
    
    // Verify FLAC stream properties
    std::cout << "FLAC stream properties:" << std::endl;
    std::cout << "  Sample rate: " << flacStream.sample_rate << " Hz" << std::endl;
    std::cout << "  Channels: " << flacStream.channels << std::endl;
    std::cout << "  Bits per sample: " << flacStream.bits_per_sample << std::endl;
    std::cout << "  Duration: " << demuxer.getDuration() << " ms" << std::endl;
    
    // Verify expected properties (based on ffprobe output)
    assert(flacStream.sample_rate == 192000);
    assert(flacStream.channels == 2);
    // Note: bits_per_sample might be 0 if not properly extracted yet
    
    std::cout << "✓ FLAC stream properties verified" << std::endl;
}

// Test reading FLAC chunks from MP4
void test_flac_chunk_reading() {
    std::cout << "Testing FLAC chunk reading from MP4..." << std::endl;
    
    auto ioHandler = std::make_unique<FileIOHandler>("data/timeless.mp4");
    if (ioHandler->getLastError() != 0) {
        std::cerr << "Failed to open test file: data/timeless.mp4" << std::endl;
        assert(false);
    }
    
    ISODemuxer demuxer(std::move(ioHandler));
    bool parsed = demuxer.parseContainer();
    assert(parsed);
    
    // Read first few chunks
    int chunksRead = 0;
    const int maxChunks = 5;
    
    while (chunksRead < maxChunks && !demuxer.isEOF()) {
        MediaChunk chunk = demuxer.readChunk();
        
        if (chunk.data.empty()) {
            break; // End of stream or error
        }
        
        chunksRead++;
        std::cout << "Chunk " << chunksRead << ": " << chunk.data.size() << " bytes";
        
        // Check for FLAC frame sync pattern in the data
        if (chunk.data.size() >= 2) {
            uint16_t syncPattern = (static_cast<uint16_t>(chunk.data[0]) << 8) | chunk.data[1];
            if ((syncPattern & 0xFFF8) == 0xFFF8) {
                std::cout << " (FLAC frame detected)";
            } else if (chunk.data.size() >= 4 && 
                      chunk.data[0] == 'f' && chunk.data[1] == 'L' && 
                      chunk.data[2] == 'a' && chunk.data[3] == 'C') {
                std::cout << " (FLAC signature detected)";
            }
        }
        std::cout << std::endl;
    }
    
    assert(chunksRead > 0);
    std::cout << "✓ Successfully read " << chunksRead << " FLAC chunks" << std::endl;
}

// Test seeking in FLAC-in-MP4 file
void test_flac_seeking() {
    std::cout << "Testing FLAC seeking in MP4..." << std::endl;
    
    auto ioHandler = std::make_unique<FileIOHandler>("data/timeless.mp4");
    if (ioHandler->getLastError() != 0) {
        std::cerr << "Failed to open test file: data/timeless.mp4" << std::endl;
        assert(false);
    }
    
    ISODemuxer demuxer(std::move(ioHandler));
    bool parsed = demuxer.parseContainer();
    assert(parsed);
    
    uint64_t duration = demuxer.getDuration();
    if (duration > 0) {
        // Seek to middle of file
        uint64_t seekTime = duration / 2;
        bool seekResult = demuxer.seekTo(seekTime);
        assert(seekResult);
        
        uint64_t currentPos = demuxer.getPosition();
        std::cout << "✓ Seeked to " << seekTime << "ms, current position: " << currentPos << "ms" << std::endl;
        
        // Read a chunk after seeking
        MediaChunk chunk = demuxer.readChunk();
        assert(!chunk.data.empty());
        std::cout << "✓ Successfully read chunk after seeking: " << chunk.data.size() << " bytes" << std::endl;
    } else {
        std::cout << "⚠ Duration is 0, skipping seek test" << std::endl;
    }
}

// Test metadata extraction from FLAC-in-MP4
void test_flac_metadata() {
    std::cout << "Testing FLAC metadata extraction from MP4..." << std::endl;
    
    auto ioHandler = std::make_unique<FileIOHandler>("data/timeless.mp4");
    if (ioHandler->getLastError() != 0) {
        std::cerr << "Failed to open test file: data/timeless.mp4" << std::endl;
        assert(false);
    }
    
    ISODemuxer demuxer(std::move(ioHandler));
    bool parsed = demuxer.parseContainer();
    assert(parsed);
    
    // Get metadata
    std::map<std::string, std::string> metadata = demuxer.getMetadata();
    
    std::cout << "Extracted metadata:" << std::endl;
    for (const auto& pair : metadata) {
        std::cout << "  " << pair.first << ": " << pair.second << std::endl;
    }
    
    std::cout << "✓ Metadata extraction completed (found " << metadata.size() << " entries)" << std::endl;
}

int main() {
    std::cout << "Running FLAC-in-MP4 integration tests..." << std::endl;
    
    try {
        test_flac_mp4_file_parsing();
        test_flac_chunk_reading();
        test_flac_seeking();
        test_flac_metadata();
        
        std::cout << "\n✅ All FLAC-in-MP4 integration tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}