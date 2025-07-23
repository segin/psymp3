/*
 * StreamingManager.cpp - Progressive download and streaming support for ISO demuxer
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include <algorithm>
#include <chrono>

StreamingManager::StreamingManager(std::shared_ptr<IOHandler> io)
    : io(io), stopThread(false) {
    
    // Check if this is a streaming source
    fileSize = io->getFileSize();
    isStreamingSource = (fileSize < 0 || isHTTPSource());
    
    if (isStreamingSource) {
        Debug::log("streaming", "StreamingManager: Detected streaming source");
        
        // Start background download thread
        downloadThread = std::thread(&StreamingManager::downloadThreadFunc, this);
        
        // Scan for movie box location
        scanForMovieBox();
    } else {
        Debug::log("streaming", "StreamingManager: Complete file available, size: ", fileSize);
        
        // For complete files, add the entire file as a downloaded range
        if (fileSize > 0) {
            addDownloadedRange(0, fileSize - 1);
        }
    }
}

StreamingManager::~StreamingManager() {
    // Stop background thread
    if (downloadThread.joinable()) {
        stopThread = true;
        queueCondition.notify_all();
        downloadThread.join();
    }
}

bool StreamingManager::isStreaming() const {
    return isStreamingSource;
}

bool StreamingManager::isMovieBoxAtEnd() const {
    return movieBoxAtEnd;
}

uint64_t StreamingManager::findMovieBox() {
    if (movieBoxOffset > 0) {
        return movieBoxOffset;
    }
    
    // If we haven't found it yet, scan for it
    if (scanForMovieBox()) {
        return movieBoxOffset;
    }
    
    return 0;
}

size_t StreamingManager::readData(uint64_t offset, void* buffer, size_t size, size_t count) {
    if (!isStreaming()) {
        // For complete files, just read directly
        io->seek(offset, SEEK_SET);
        return io->read(buffer, size, count);
    }
    
    // For streaming, check if data is available
    size_t totalSize = size * count;
    if (!isDataAvailable(offset, totalSize)) {
        // Request the data
        requestByteRange(offset, totalSize);
        
        // Wait for data to become available
        if (!waitForData(offset, totalSize)) {
            // Timeout or error
            return 0;
        }
    }
    
    // Data is available, read it
    io->seek(offset, SEEK_SET);
    return io->read(buffer, size, count);
}

bool StreamingManager::isDataAvailable(uint64_t offset, size_t size) {
    if (!isStreaming()) {
        return true; // Complete file is always available
    }
    
    return isRangeDownloaded(offset, offset + size - 1);
}

bool StreamingManager::requestByteRange(uint64_t offset, size_t size) {
    if (!isStreaming()) {
        return true; // Complete file is always available
    }
    
    // Check if we already have this range
    if (isDataAvailable(offset, size)) {
        return true;
    }
    
    // Calculate optimal range to request
    // Request at least 64KB to reduce overhead
    constexpr size_t MIN_REQUEST_SIZE = 64 * 1024;
    size_t requestSize = std::max(size, MIN_REQUEST_SIZE);
    
    // Queue the request
    ByteRange range;
    range.start = offset;
    range.end = offset + requestSize - 1;
    
    // Ensure we don't request beyond file size if known
    if (fileSize > 0 && range.end >= static_cast<uint64_t>(fileSize)) {
        range.end = fileSize - 1;
    }
    
    // Add to download queue
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        downloadQueue.push(range);
    }
    
    // Notify download thread
    queueCondition.notify_one();
    
    return true;
}

bool StreamingManager::waitForData(uint64_t offset, size_t size, uint32_t timeout_ms) {
    if (!isStreaming()) {
        return true; // Complete file is always available
    }
    
    auto startTime = std::chrono::steady_clock::now();
    
    while (!isDataAvailable(offset, size)) {
        // Check timeout
        if (timeout_ms > 0) {
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
            
            if (elapsed >= timeout_ms) {
                Debug::log("streaming", "StreamingManager: Timeout waiting for data at offset ", offset);
                return false;
            }
        }
        
        // Sleep briefly to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return true;
}

int StreamingManager::getDownloadProgress() const {
    if (!isStreaming() || fileSize <= 0) {
        return 100; // Complete file or unknown size
    }
    
    // Calculate total downloaded bytes
    uint64_t downloadedBytes = 0;
    
    {
        std::lock_guard<std::mutex> lock(rangesMutex);
        for (const auto& range : downloadedRanges) {
            downloadedBytes += (range.end - range.start + 1);
        }
    }
    
    return static_cast<int>((downloadedBytes * 100) / fileSize);
}

bool StreamingManager::isDownloadComplete() const {
    if (!isStreaming() || fileSize <= 0) {
        return true; // Complete file or unknown size
    }
    
    return getDownloadProgress() == 100;
}

int64_t StreamingManager::getFileSize() const {
    return fileSize;
}

void StreamingManager::prefetchSample(uint64_t offset, size_t size) {
    if (!isStreaming()) {
        return; // No need to prefetch for complete files
    }
    
    // Request this sample and additional data for prefetching
    constexpr size_t PREFETCH_BUFFER = 128 * 1024; // 128KB prefetch buffer
    requestByteRange(offset, size + PREFETCH_BUFFER);
}

void StreamingManager::setPrefetchStrategy(uint32_t lookahead) {
    prefetchLookahead = lookahead;
}

void StreamingManager::downloadThreadFunc() {
    while (!stopThread) {
        ByteRange range;
        bool hasRange = false;
        
        // Get next range from queue
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            if (downloadQueue.empty()) {
                // Wait for new requests
                queueCondition.wait_for(lock, std::chrono::seconds(1));
                continue;
            }
            
            range = downloadQueue.front();
            downloadQueue.pop();
            hasRange = true;
        }
        
        if (hasRange) {
            // Download the range
            downloadRange(range);
        }
    }
}

bool StreamingManager::downloadRange(const ByteRange& range) {
    // Check if range is already downloaded
    if (isRangeDownloaded(range.start, range.end)) {
        return true;
    }
    
    Debug::log("streaming", "StreamingManager: Downloading range ", range.start, "-", range.end);
    
    // For HTTP sources, use range requests
    if (isHTTPSource()) {
        // The HTTPIOHandler already handles range requests internally
        io->seek(range.start, SEEK_SET);
        
        // Calculate size to read
        size_t sizeToRead = range.end - range.start + 1;
        std::vector<uint8_t> buffer(sizeToRead);
        
        // Read the data
        size_t bytesRead = io->read(buffer.data(), 1, sizeToRead);
        
        if (bytesRead > 0) {
            // Mark range as downloaded
            addDownloadedRange(range.start, range.start + bytesRead - 1);
            return true;
        }
        
        return false;
    } else {
        // For other streaming sources, just read the data
        io->seek(range.start, SEEK_SET);
        
        // Calculate size to read
        size_t sizeToRead = range.end - range.start + 1;
        std::vector<uint8_t> buffer(sizeToRead);
        
        // Read the data
        size_t bytesRead = io->read(buffer.data(), 1, sizeToRead);
        
        if (bytesRead > 0) {
            // Mark range as downloaded
            addDownloadedRange(range.start, range.start + bytesRead - 1);
            return true;
        }
        
        return false;
    }
}

void StreamingManager::mergeRanges() {
    std::lock_guard<std::mutex> lock(rangesMutex);
    
    if (downloadedRanges.size() <= 1) {
        return;
    }
    
    // Sort ranges by start offset
    std::sort(downloadedRanges.begin(), downloadedRanges.end());
    
    // Merge overlapping ranges
    std::vector<ByteRange> mergedRanges;
    mergedRanges.push_back(downloadedRanges[0]);
    
    for (size_t i = 1; i < downloadedRanges.size(); i++) {
        ByteRange& last = mergedRanges.back();
        const ByteRange& current = downloadedRanges[i];
        
        // Check if ranges overlap or are adjacent
        if (current.start <= last.end + 1) {
            // Merge ranges
            last.end = std::max(last.end, current.end);
        } else {
            // Add new range
            mergedRanges.push_back(current);
        }
    }
    
    // Update ranges
    downloadedRanges = std::move(mergedRanges);
}

bool StreamingManager::isRangeDownloaded(uint64_t start, uint64_t end) const {
    std::lock_guard<std::mutex> lock(rangesMutex);
    
    for (const auto& range : downloadedRanges) {
        if (start >= range.start && end <= range.end) {
            return true;
        }
    }
    
    return false;
}

void StreamingManager::addDownloadedRange(uint64_t start, uint64_t end) {
    std::lock_guard<std::mutex> lock(rangesMutex);
    
    ByteRange range;
    range.start = start;
    range.end = end;
    
    downloadedRanges.push_back(range);
    
    // Merge overlapping ranges
    mergeRanges();
}

bool StreamingManager::scanForMovieBox() {
    // Scan for 'moov' box
    constexpr uint32_t MOOV_BOX = 0x6D6F6F76; // 'moov' in big-endian
    constexpr uint32_t FTYP_BOX = 0x66747970; // 'ftyp' in big-endian
    
    // First check the beginning of the file (after ftyp)
    uint8_t buffer[16];
    io->seek(0, SEEK_SET);
    
    if (io->read(buffer, 1, 8) == 8) {
        uint32_t size = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
        uint32_t type = (buffer[4] << 24) | (buffer[5] << 16) | (buffer[6] << 8) | buffer[7];
        
        if (type == FTYP_BOX && size >= 8 && size < 1024) {
            // Found ftyp box, check if moov follows it
            io->seek(size, SEEK_SET);
            
            if (io->read(buffer, 1, 8) == 8) {
                uint32_t moovSize = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
                uint32_t moovType = (buffer[4] << 24) | (buffer[5] << 16) | (buffer[6] << 8) | buffer[7];
                
                if (moovType == MOOV_BOX && moovSize >= 8) {
                    // Found moov box at beginning
                    movieBoxOffset = size;
                    movieBoxAtEnd = false;
                    Debug::log("streaming", "StreamingManager: Found moov box at beginning, offset: ", movieBoxOffset);
                    return true;
                }
            }
        }
    }
    
    // If we have file size, check the end of the file
    if (fileSize > 0) {
        // Look for moov box near the end
        // Start scanning from 16MB before the end
        uint64_t scanStart = (fileSize > 16 * 1024 * 1024) ? (fileSize - 16 * 1024 * 1024) : 0;
        
        // Request this range to be downloaded
        requestByteRange(scanStart, fileSize - scanStart);
        
        // Wait for data to become available
        if (!waitForData(scanStart, std::min<size_t>(4096, fileSize - scanStart))) {
            return false;
        }
        
        // Scan for moov box
        constexpr size_t SCAN_BUFFER_SIZE = 4096;
        std::vector<uint8_t> scanBuffer(SCAN_BUFFER_SIZE);
        
        for (uint64_t offset = scanStart; offset < static_cast<uint64_t>(fileSize) - 8; offset += (SCAN_BUFFER_SIZE - 8)) {
            // Read scan buffer
            io->seek(offset, SEEK_SET);
            size_t bytesRead = io->read(scanBuffer.data(), 1, SCAN_BUFFER_SIZE);
            
            if (bytesRead < 8) {
                break;
            }
            
            // Scan buffer for moov box
            for (size_t i = 0; i <= bytesRead - 8; i++) {
                uint32_t boxSize = (scanBuffer[i] << 24) | (scanBuffer[i+1] << 16) | 
                                  (scanBuffer[i+2] << 8) | scanBuffer[i+3];
                uint32_t boxType = (scanBuffer[i+4] << 24) | (scanBuffer[i+5] << 16) | 
                                  (scanBuffer[i+6] << 8) | scanBuffer[i+7];
                
                if (boxType == MOOV_BOX && boxSize >= 8) {
                    // Found moov box
                    movieBoxOffset = offset + i;
                    movieBoxAtEnd = true;
                    Debug::log("streaming", "StreamingManager: Found moov box near end, offset: ", movieBoxOffset);
                    return true;
                }
            }
        }
    }
    
    Debug::log("streaming", "StreamingManager: Movie box not found");
    return false;
}

bool StreamingManager::isHTTPSource() const {
    // Check if the IOHandler is an HTTPIOHandler
    // This is a simple check based on the file size behavior
    // HTTPIOHandler returns content length from getFileSize()
    
    // A more robust approach would be to use dynamic_cast, but we'll
    // use this simpler approach for now
    
    // If file size is known and we can seek to the end, it's likely not HTTP
    if (fileSize > 0) {
        long currentPos = io->tell();
        if (io->seek(0, SEEK_END) == 0) {
            io->seek(currentPos, SEEK_SET);
            return false;
        }
    }
    
    // Otherwise, assume it's HTTP if streaming
    return isStreamingSource;
}