/*
 * test_streaming_manager.cpp - Tests for StreamingManager
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

// Mock IOHandler for testing streaming scenarios
class MockStreamingIOHandler : public IOHandler {
public:
    MockStreamingIOHandler(bool isStreaming, bool movieBoxAtEnd)
        : isStreaming(isStreaming), movieBoxAtEnd(movieBoxAtEnd), position(0) {
        
        // Create a simple ISO file structure in memory
        createTestFile();
    }
    
    size_t read(void* buffer, size_t size, size_t count) override {
        if (position >= fileData.size()) {
            return 0;
        }
        
        size_t bytesToRead = size * count;
        size_t bytesAvailable = fileData.size() - position;
        size_t bytesToCopy = std::min(bytesToRead, bytesAvailable);
        
        if (isStreaming) {
            // Simulate streaming by checking if data is "downloaded"
            size_t endPos = position + bytesToCopy;
            if (endPos > downloadedBytes) {
                // Only read up to what's been "downloaded"
                bytesToCopy = downloadedBytes > position ? downloadedBytes - position : 0;
            }
        }
        
        if (bytesToCopy > 0) {
            std::memcpy(buffer, fileData.data() + position, bytesToCopy);
            position += bytesToCopy;
        }
        
        return bytesToCopy / size;
    }
    
    int seek(long offset, int whence) override {
        switch (whence) {
            case SEEK_SET:
                position = offset;
                break;
            case SEEK_CUR:
                position += offset;
                break;
            case SEEK_END:
                position = fileData.size() + offset;
                break;
            default:
                return -1;
        }
        
        if (position < 0) {
            position = 0;
            return -1;
        }
        
        if (position > fileData.size()) {
            position = fileData.size();
        }
        
        return 0;
    }
    
    off_t tell() override {
        return position;
    }
    
    int close() override {
        return 0;
    }
    
    bool eof() override {
        return position >= fileData.size();
    }
    
    off_t getFileSize() override {
        return isStreaming ? -1 : fileData.size();
    }
    
    // Test helpers
    void simulateDownloadProgress(size_t bytes) {
        downloadedBytes = std::min(bytes, fileData.size());
    }
    
    void simulateCompleteDownload() {
        downloadedBytes = fileData.size();
    }
    
    size_t getFileDataSize() const {
        return fileData.size();
    }
    
private:
    bool isStreaming;
    bool movieBoxAtEnd;
    std::vector<uint8_t> fileData;
    long position;
    size_t downloadedBytes = 0;
    
    void createTestFile() {
        // Create a simplified ISO file structure
        // This is not a valid ISO file, just enough structure for testing
        
        // Start with ftyp box
        addBox(FOURCC('f','t','y','p'), {
            // Major brand: isom
            0x69, 0x73, 0x6F, 0x6D,
            // Minor version: 0
            0x00, 0x00, 0x00, 0x00,
            // Compatible brands: isom, iso2, mp41
            0x69, 0x73, 0x6F, 0x6D,
            0x69, 0x73, 0x6F, 0x32,
            0x6D, 0x70, 0x34, 0x31
        });
        
        // If movie box is at beginning, add it now
        if (!movieBoxAtEnd) {
            addMovieBox();
        }
        
        // Add mdat box with some dummy data
        std::vector<uint8_t> mdatData(1024, 0xAA);
        addBox(FOURCC('m','d','a','t'), mdatData);
        
        // If movie box is at end, add it now
        if (movieBoxAtEnd) {
            addMovieBox();
        }
    }
    
    void addMovieBox() {
        // Start moov box
        size_t moovStart = fileData.size();
        
        // Add moov header
        fileData.push_back(0); // Size placeholder
        fileData.push_back(0);
        fileData.push_back(0);
        fileData.push_back(0);
        fileData.push_back('m');
        fileData.push_back('o');
        fileData.push_back('o');
        fileData.push_back('v');
        
        // Add mvhd box (simplified)
        addBox(FOURCC('m','v','h','d'), {
            0x00, 0x00, 0x00, 0x00, // Version and flags
            0x00, 0x00, 0x00, 0x00, // Creation time
            0x00, 0x00, 0x00, 0x00, // Modification time
            0x00, 0x00, 0x03, 0xE8, // Timescale (1000)
            0x00, 0x00, 0x00, 0x0A, // Duration (10 seconds)
            0x00, 0x01, 0x00, 0x00, // Rate (1.0)
            0x01, 0x00, 0x00, 0x00  // Volume (1.0) and reserved
        });
        
        // Add a simple audio track
        size_t trakStart = fileData.size();
        
        // Add trak header
        fileData.push_back(0); // Size placeholder
        fileData.push_back(0);
        fileData.push_back(0);
        fileData.push_back(0);
        fileData.push_back('t');
        fileData.push_back('r');
        fileData.push_back('a');
        fileData.push_back('k');
        
        // Add tkhd box (simplified)
        addBox(FOURCC('t','k','h','d'), {
            0x00, 0x00, 0x00, 0x03, // Version and flags
            0x00, 0x00, 0x00, 0x00, // Creation time
            0x00, 0x00, 0x00, 0x00, // Modification time
            0x00, 0x00, 0x00, 0x01, // Track ID
            0x00, 0x00, 0x00, 0x00, // Reserved
            0x00, 0x00, 0x00, 0x0A, // Duration (10 seconds)
            0x00, 0x00, 0x00, 0x00, // Reserved
            0x00, 0x00, 0x00, 0x00, // Reserved
            0x00, 0x00, 0x00, 0x00, // Layer and alternate group
            0x00, 0x00, 0x00, 0x00, // Volume and reserved
            0x00, 0x01, 0x00, 0x00, // Matrix
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x01, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x40, 0x00, 0x00, 0x00  // Width and height
        });
        
        // Add mdia box
        size_t mdiaStart = fileData.size();
        
        // Add mdia header
        fileData.push_back(0); // Size placeholder
        fileData.push_back(0);
        fileData.push_back(0);
        fileData.push_back(0);
        fileData.push_back('m');
        fileData.push_back('d');
        fileData.push_back('i');
        fileData.push_back('a');
        
        // Add mdhd box
        addBox(FOURCC('m','d','h','d'), {
            0x00, 0x00, 0x00, 0x00, // Version and flags
            0x00, 0x00, 0x00, 0x00, // Creation time
            0x00, 0x00, 0x00, 0x00, // Modification time
            0x00, 0x00, 0xAC, 0x44, // Timescale (44100)
            0x00, 0x02, 0xB1, 0x10, // Duration (176400 samples = 4 seconds)
            0x55, 0xC4, 0x00, 0x00  // Language (eng) and quality
        });
        
        // Add hdlr box
        addBox(FOURCC('h','d','l','r'), {
            0x00, 0x00, 0x00, 0x00, // Version and flags
            0x00, 0x00, 0x00, 0x00, // Predefined
            's', 'o', 'u', 'n',     // Handler type (sound)
            0x00, 0x00, 0x00, 0x00, // Reserved
            0x00, 0x00, 0x00, 0x00, // Reserved
            0x00, 0x00, 0x00, 0x00, // Reserved
            'S', 'o', 'u', 'n', 'd', 'H', 'a', 'n', 'd', 'l', 'e', 'r', 0x00 // Name
        });
        
        // Add minf box
        size_t minfStart = fileData.size();
        
        // Add minf header
        fileData.push_back(0); // Size placeholder
        fileData.push_back(0);
        fileData.push_back(0);
        fileData.push_back(0);
        fileData.push_back('m');
        fileData.push_back('i');
        fileData.push_back('n');
        fileData.push_back('f');
        
        // Add smhd box
        addBox(FOURCC('s','m','h','d'), {
            0x00, 0x00, 0x00, 0x00, // Version and flags
            0x00, 0x00, 0x00, 0x00  // Balance and reserved
        });
        
        // Add dinf box
        size_t dinfStart = fileData.size();
        
        // Add dinf header
        fileData.push_back(0); // Size placeholder
        fileData.push_back(0);
        fileData.push_back(0);
        fileData.push_back(0);
        fileData.push_back('d');
        fileData.push_back('i');
        fileData.push_back('n');
        fileData.push_back('f');
        
        // Add dref box
        addBox(FOURCC('d','r','e','f'), {
            0x00, 0x00, 0x00, 0x00, // Version and flags
            0x00, 0x00, 0x00, 0x01, // Entry count
            // URL box
            0x00, 0x00, 0x00, 0x0C, // Size
            'u', 'r', 'l', ' ',     // Type
            0x00, 0x00, 0x00, 0x01  // Version and flags (self-contained)
        });
        
        // Update dinf size
        uint32_t dinfSize = fileData.size() - dinfStart;
        fileData[dinfStart] = (dinfSize >> 24) & 0xFF;
        fileData[dinfStart+1] = (dinfSize >> 16) & 0xFF;
        fileData[dinfStart+2] = (dinfSize >> 8) & 0xFF;
        fileData[dinfStart+3] = dinfSize & 0xFF;
        
        // Add stbl box
        size_t stblStart = fileData.size();
        
        // Add stbl header
        fileData.push_back(0); // Size placeholder
        fileData.push_back(0);
        fileData.push_back(0);
        fileData.push_back(0);
        fileData.push_back('s');
        fileData.push_back('t');
        fileData.push_back('b');
        fileData.push_back('l');
        
        // Add stsd box
        addBox(FOURCC('s','t','s','d'), {
            0x00, 0x00, 0x00, 0x00, // Version and flags
            0x00, 0x00, 0x00, 0x01, // Entry count
            // MP4A box
            0x00, 0x00, 0x00, 0x20, // Size
            'm', 'p', '4', 'a',     // Type
            0x00, 0x00, 0x00, 0x00, // Reserved
            0x00, 0x00, 0x00, 0x01, // Data reference index
            0x00, 0x00, 0x00, 0x00, // Reserved
            0x00, 0x00, 0x00, 0x00, // Reserved
            0x00, 0x02, 0x00, 0x10, // Channel count (2) and sample size (16)
            0x00, 0x00, 0x00, 0x00, // Reserved
            0x00, 0x00, 0xAC, 0x44  // Sample rate (44100)
        });
        
        // Add stts box (time-to-sample)
        addBox(FOURCC('s','t','t','s'), {
            0x00, 0x00, 0x00, 0x00, // Version and flags
            0x00, 0x00, 0x00, 0x01, // Entry count
            0x00, 0x00, 0x00, 0x0A, // Sample count (10)
            0x00, 0x00, 0x04, 0x00  // Sample delta (1024)
        });
        
        // Add stsc box (sample-to-chunk)
        addBox(FOURCC('s','t','s','c'), {
            0x00, 0x00, 0x00, 0x00, // Version and flags
            0x00, 0x00, 0x00, 0x01, // Entry count
            0x00, 0x00, 0x00, 0x01, // First chunk
            0x00, 0x00, 0x00, 0x0A, // Samples per chunk (10)
            0x00, 0x00, 0x00, 0x01  // Sample description index
        });
        
        // Add stsz box (sample size)
        addBox(FOURCC('s','t','s','z'), {
            0x00, 0x00, 0x00, 0x00, // Version and flags
            0x00, 0x00, 0x00, 0x00, // Sample size (0 = variable)
            0x00, 0x00, 0x00, 0x0A, // Sample count (10)
            0x00, 0x00, 0x04, 0x00, // Sample 1 size (1024)
            0x00, 0x00, 0x04, 0x00, // Sample 2 size (1024)
            0x00, 0x00, 0x04, 0x00, // Sample 3 size (1024)
            0x00, 0x00, 0x04, 0x00, // Sample 4 size (1024)
            0x00, 0x00, 0x04, 0x00, // Sample 5 size (1024)
            0x00, 0x00, 0x04, 0x00, // Sample 6 size (1024)
            0x00, 0x00, 0x04, 0x00, // Sample 7 size (1024)
            0x00, 0x00, 0x04, 0x00, // Sample 8 size (1024)
            0x00, 0x00, 0x04, 0x00, // Sample 9 size (1024)
            0x00, 0x00, 0x04, 0x00  // Sample 10 size (1024)
        });
        
        // Add stco box (chunk offset)
        addBox(FOURCC('s','t','c','o'), {
            0x00, 0x00, 0x00, 0x00, // Version and flags
            0x00, 0x00, 0x00, 0x01, // Entry count
            0x00, 0x00, 0x01, 0x00  // Chunk offset (256)
        });
        
        // Update stbl size
        uint32_t stblSize = fileData.size() - stblStart;
        fileData[stblStart] = (stblSize >> 24) & 0xFF;
        fileData[stblStart+1] = (stblSize >> 16) & 0xFF;
        fileData[stblStart+2] = (stblSize >> 8) & 0xFF;
        fileData[stblStart+3] = stblSize & 0xFF;
        
        // Update minf size
        uint32_t minfSize = fileData.size() - minfStart;
        fileData[minfStart] = (minfSize >> 24) & 0xFF;
        fileData[minfStart+1] = (minfSize >> 16) & 0xFF;
        fileData[minfStart+2] = (minfSize >> 8) & 0xFF;
        fileData[minfStart+3] = minfSize & 0xFF;
        
        // Update mdia size
        uint32_t mdiaSize = fileData.size() - mdiaStart;
        fileData[mdiaStart] = (mdiaSize >> 24) & 0xFF;
        fileData[mdiaStart+1] = (mdiaSize >> 16) & 0xFF;
        fileData[mdiaStart+2] = (mdiaSize >> 8) & 0xFF;
        fileData[mdiaStart+3] = mdiaSize & 0xFF;
        
        // Update trak size
        uint32_t trakSize = fileData.size() - trakStart;
        fileData[trakStart] = (trakSize >> 24) & 0xFF;
        fileData[trakStart+1] = (trakSize >> 16) & 0xFF;
        fileData[trakStart+2] = (trakSize >> 8) & 0xFF;
        fileData[trakStart+3] = trakSize & 0xFF;
        
        // Update moov size
        uint32_t moovSize = fileData.size() - moovStart;
        fileData[moovStart] = (moovSize >> 24) & 0xFF;
        fileData[moovStart+1] = (moovSize >> 16) & 0xFF;
        fileData[moovStart+2] = (moovSize >> 8) & 0xFF;
        fileData[moovStart+3] = moovSize & 0xFF;
    }
    
    void addBox(uint32_t type, const std::vector<uint8_t>& data) {
        // Add box size (8 bytes header + data size)
        uint32_t size = 8 + data.size();
        fileData.push_back((size >> 24) & 0xFF);
        fileData.push_back((size >> 16) & 0xFF);
        fileData.push_back((size >> 8) & 0xFF);
        fileData.push_back(size & 0xFF);
        
        // Add box type
        fileData.push_back((type >> 24) & 0xFF);
        fileData.push_back((type >> 16) & 0xFF);
        fileData.push_back((type >> 8) & 0xFF);
        fileData.push_back(type & 0xFF);
        
        // Add box data
        fileData.insert(fileData.end(), data.begin(), data.end());
    }
};

// Test functions
void testCompleteFile() {
    std::cout << "Testing StreamingManager with complete file..." << std::endl;
    
    // Create mock handler with complete file
    auto mockHandler = std::make_shared<MockStreamingIOHandler>(false, false);
    
    // Create streaming manager
    StreamingManager manager(mockHandler);
    
    // Check if streaming is detected correctly
    if (manager.isStreaming()) {
        std::cout << "FAIL: Complete file incorrectly detected as streaming" << std::endl;
        return;
    }
    
    // Check if movie box is found
    uint64_t moovOffset = manager.findMovieBox();
    if (moovOffset == 0) {
        std::cout << "FAIL: Movie box not found in complete file" << std::endl;
        return;
    }
    
    std::cout << "PASS: Complete file tests" << std::endl;
}

void testProgressiveDownload() {
    std::cout << "Testing StreamingManager with progressive download..." << std::endl;
    
    // Create mock handler with movie box at end
    auto mockHandler = std::make_shared<MockStreamingIOHandler>(true, true);
    
    // Create streaming manager
    StreamingManager manager(mockHandler);
    
    // Check if streaming is detected correctly
    if (!manager.isStreaming()) {
        std::cout << "FAIL: Progressive download not detected as streaming" << std::endl;
        return;
    }
    
    // Check if movie box at end is detected
    if (!manager.isMovieBoxAtEnd()) {
        std::cout << "FAIL: Movie box at end not detected" << std::endl;
        return;
    }
    
    // Simulate partial download
    mockHandler->simulateDownloadProgress(mockHandler->getFileDataSize() / 2);
    
    // Try to read data that's not yet available
    uint8_t buffer[1024];
    size_t bytesRead = manager.readData(mockHandler->getFileDataSize() - 100, buffer, 1, 50);
    
    if (bytesRead > 0) {
        std::cout << "FAIL: Should not be able to read unavailable data" << std::endl;
        return;
    }
    
    // Simulate complete download
    mockHandler->simulateCompleteDownload();
    
    // Now we should be able to read the data
    bytesRead = manager.readData(mockHandler->getFileDataSize() - 100, buffer, 1, 50);
    
    if (bytesRead != 50) {
        std::cout << "FAIL: Should be able to read available data after download" << std::endl;
        return;
    }
    
    // Check if movie box is found after complete download
    uint64_t moovOffset = manager.findMovieBox();
    if (moovOffset == 0) {
        std::cout << "FAIL: Movie box not found after complete download" << std::endl;
        return;
    }
    
    std::cout << "PASS: Progressive download tests" << std::endl;
}

void testByteRangeRequests() {
    std::cout << "Testing byte range requests..." << std::endl;
    
    // Create mock handler with streaming
    auto mockHandler = std::make_shared<MockStreamingIOHandler>(true, false);
    
    // Create streaming manager
    StreamingManager manager(mockHandler);
    
    // Request a byte range
    uint64_t testOffset = 100;
    size_t testSize = 50;
    
    // Initially no data is available
    mockHandler->simulateDownloadProgress(0);
    
    if (manager.isDataAvailable(testOffset, testSize)) {
        std::cout << "FAIL: Data should not be available before download" << std::endl;
        return;
    }
    
    // Request the byte range
    manager.requestByteRange(testOffset, testSize);
    
    // Simulate download of the requested range
    mockHandler->simulateDownloadProgress(testOffset + testSize);
    
    // Wait a bit for background thread to process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Now the data should be available
    if (!manager.isDataAvailable(testOffset, testSize)) {
        std::cout << "FAIL: Data should be available after download" << std::endl;
        return;
    }
    
    // Test reading the data
    uint8_t buffer[50];
    size_t bytesRead = manager.readData(testOffset, buffer, 1, testSize);
    
    if (bytesRead != testSize) {
        std::cout << "FAIL: Should read all requested bytes" << std::endl;
        return;
    }
    
    std::cout << "PASS: Byte range request tests" << std::endl;
}

void testPrefetching() {
    std::cout << "Testing prefetching..." << std::endl;
    
    // Create mock handler with streaming
    auto mockHandler = std::make_shared<MockStreamingIOHandler>(true, false);
    
    // Create streaming manager
    StreamingManager manager(mockHandler);
    
    // Set prefetch strategy
    manager.setPrefetchStrategy(3);
    
    // Initially no data is available
    mockHandler->simulateDownloadProgress(0);
    
    // Request prefetch for a sample
    uint64_t sampleOffset = 200;
    size_t sampleSize = 1024;
    manager.prefetchSample(sampleOffset, sampleSize);
    
    // Simulate download progress
    mockHandler->simulateDownloadProgress(sampleOffset + sampleSize + 128 * 1024);
    
    // Wait a bit for background thread to process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Check if prefetched data is available
    if (!manager.isDataAvailable(sampleOffset, sampleSize)) {
        std::cout << "FAIL: Prefetched data should be available" << std::endl;
        return;
    }
    
    // Check if additional prefetch buffer is available
    if (!manager.isDataAvailable(sampleOffset + sampleSize, 64 * 1024)) {
        std::cout << "FAIL: Prefetch buffer should be available" << std::endl;
        return;
    }
    
    std::cout << "PASS: Prefetching tests" << std::endl;
}

int main() {
    std::cout << "Running StreamingManager tests..." << std::endl;
    
    testCompleteFile();
    testProgressiveDownload();
    testByteRangeRequests();
    testPrefetching();
    
    std::cout << "All tests completed." << std::endl;
    return 0;
}