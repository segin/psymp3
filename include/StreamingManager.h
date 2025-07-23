/*
 * StreamingManager.h - Progressive download and streaming support for ISO demuxer
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef STREAMINGMANAGER_H
#define STREAMINGMANAGER_H

#include "IOHandler.h"
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <queue>

/**
 * @brief Manages progressive download and streaming for ISO demuxer
 * 
 * Handles incomplete files, byte range requests, and buffering for
 * samples that are not yet available.
 */
class StreamingManager {
public:
    /**
     * @brief Constructor
     * @param io IOHandler for the media source
     */
    explicit StreamingManager(std::shared_ptr<IOHandler> io);
    
    /**
     * @brief Destructor
     */
    ~StreamingManager();
    
    /**
     * @brief Check if the file is a streaming source
     * @return true if streaming, false if complete file
     */
    bool isStreaming() const;
    
    /**
     * @brief Check if the movie box is at the end of the file
     * @return true if movie box is at end (progressive download)
     */
    bool isMovieBoxAtEnd() const;
    
    /**
     * @brief Locate the movie box in the file
     * @return Offset to movie box, or 0 if not found
     */
    uint64_t findMovieBox();
    
    /**
     * @brief Read data from the source, handling streaming scenarios
     * @param offset File offset to read from
     * @param buffer Buffer to read into
     * @param size Size of each element to read
     * @param count Number of elements to read
     * @return Number of elements successfully read
     */
    size_t readData(uint64_t offset, void* buffer, size_t size, size_t count);
    
    /**
     * @brief Check if data at a specific offset is available
     * @param offset File offset to check
     * @param size Size of data needed
     * @return true if data is available, false if not yet downloaded
     */
    bool isDataAvailable(uint64_t offset, size_t size);
    
    /**
     * @brief Request a byte range to be downloaded
     * @param offset Start offset
     * @param size Size of range to request
     * @return true if request was queued successfully
     */
    bool requestByteRange(uint64_t offset, size_t size);
    
    /**
     * @brief Wait for data to become available
     * @param offset File offset to wait for
     * @param size Size of data needed
     * @param timeout_ms Timeout in milliseconds (0 = no timeout)
     * @return true if data became available, false on timeout
     */
    bool waitForData(uint64_t offset, size_t size, uint32_t timeout_ms = 5000);
    
    /**
     * @brief Get the download progress
     * @return Percentage of file downloaded (0-100)
     */
    int getDownloadProgress() const;
    
    /**
     * @brief Check if the entire file has been downloaded
     * @return true if download is complete
     */
    bool isDownloadComplete() const;
    
    /**
     * @brief Get the total file size if known
     * @return File size in bytes, or -1 if unknown
     */
    int64_t getFileSize() const;
    
    /**
     * @brief Start background prefetching for a sample
     * @param offset Sample data offset
     * @param size Sample data size
     */
    void prefetchSample(uint64_t offset, size_t size);
    
    /**
     * @brief Set prefetch strategy for upcoming samples
     * @param lookahead Number of samples to prefetch ahead
     */
    void setPrefetchStrategy(uint32_t lookahead);
    
private:
    std::shared_ptr<IOHandler> io;
    bool isStreamingSource = false;
    bool movieBoxAtEnd = false;
    uint64_t movieBoxOffset = 0;
    int64_t fileSize = -1;
    
    // Track downloaded ranges
    struct ByteRange {
        uint64_t start;
        uint64_t end;
        
        bool operator<(const ByteRange& other) const {
            return start < other.start;
        }
    };
    
    std::vector<ByteRange> downloadedRanges;
    mutable std::mutex rangesMutex;
    
    // Background download thread
    std::thread downloadThread;
    std::atomic<bool> stopThread;
    std::queue<ByteRange> downloadQueue;
    std::mutex queueMutex;
    std::condition_variable queueCondition;
    
    // Prefetch settings
    uint32_t prefetchLookahead = 5;
    
    /**
     * @brief Background download thread function
     */
    void downloadThreadFunc();
    
    /**
     * @brief Download a specific byte range
     * @param range Range to download
     * @return true if successful
     */
    bool downloadRange(const ByteRange& range);
    
    /**
     * @brief Merge overlapping downloaded ranges
     */
    void mergeRanges();
    
    /**
     * @brief Check if a range is fully downloaded
     * @param start Start offset
     * @param end End offset
     * @return true if range is available
     */
    bool isRangeDownloaded(uint64_t start, uint64_t end) const;
    
    /**
     * @brief Add a downloaded range to the list
     * @param start Start offset
     * @param end End offset
     */
    void addDownloadedRange(uint64_t start, uint64_t end);
    
    /**
     * @brief Scan the file for the movie box
     * @return true if movie box was found
     */
    bool scanForMovieBox();
    
    /**
     * @brief Check if the IOHandler is HTTP-based
     * @return true if HTTP streaming
     */
    bool isHTTPSource() const;
};

#endif // STREAMINGMANAGER_H