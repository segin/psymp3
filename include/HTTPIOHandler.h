/*
 * HTTPIOHandler.h - HTTP-based IOHandler for streaming media
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

#ifndef HTTPIOHANDLER_H
#define HTTPIOHANDLER_H

#include "IOHandler.h"
#include "HTTPClient.h"

/**
 * @brief IOHandler implementation for HTTP-based media streaming
 * 
 * Provides streaming access to HTTP URLs using range requests for seeking.
 * Supports both seeking and sequential reading with efficient buffering.
 */
class HTTPIOHandler : public IOHandler {
public:
    /**
     * @brief Constructor for HTTP URL
     * @param url The HTTP URL to stream from
     */
    explicit HTTPIOHandler(const std::string& url);
    
    /**
     * @brief Constructor with explicit content length
     * @param url The HTTP URL to stream from
     * @param content_length Known content length (avoids HEAD request)
     */
    HTTPIOHandler(const std::string& url, long content_length);
    
    ~HTTPIOHandler() override;

    size_t read(void* buffer, size_t size, size_t count) override;
    int seek(long offset, int whence) override;
    long tell() override;
    int close() override;
    bool eof() override;

    /**
     * @brief Get the total content length if known
     * @return Content length in bytes, or -1 if unknown
     */
    long getContentLength() const { return m_content_length; }
    
    /**
     * @brief Get the MIME type from HTTP headers
     * @return MIME type string, or empty if unknown
     */
    std::string getMimeType() const { return m_mime_type; }
    
    /**
     * @brief Check if the server supports range requests
     * @return true if range requests are supported
     */
    bool supportsRangeRequests() const { return m_supports_ranges; }

private:
    std::string m_url;
    long m_content_length = -1;
    long m_current_position = 0;
    std::string m_mime_type;
    bool m_supports_ranges = false;
    bool m_initialized = false;
    bool m_eof = false;
    
    // Buffer for read-ahead and partial reads
    std::vector<uint8_t> m_buffer;
    size_t m_buffer_offset = 0;
    long m_buffer_start_position = 0;
    
    static constexpr size_t BUFFER_SIZE = 64 * 1024; // 64KB buffer
    static constexpr size_t MIN_RANGE_SIZE = 8 * 1024; // 8KB minimum range request
    
    /**
     * @brief Initialize by performing HEAD request to get metadata
     */
    void initialize();
    
    /**
     * @brief Fill buffer starting from specified position
     * @param position Position to start reading from
     * @param min_size Minimum amount to read
     * @return true if successful, false on error
     */
    bool fillBuffer(long position, size_t min_size = MIN_RANGE_SIZE);
    
    /**
     * @brief Read data from current buffer position
     * @param buffer Output buffer
     * @param bytes_to_read Number of bytes to read
     * @return Number of bytes actually read
     */
    size_t readFromBuffer(void* buffer, size_t bytes_to_read);
    
    /**
     * @brief Check if position is within current buffer
     * @param position Position to check
     * @return true if position is buffered
     */
    bool isPositionBuffered(long position) const;
};

#endif // HTTPIOHANDLER_H