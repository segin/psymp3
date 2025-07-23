/*
 * HTTPIOHandler.h - HTTP streaming I/O handler
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

// No direct includes - all includes should be in psymp3.h

/**
 * @brief HTTP streaming I/O handler with range request support
 * 
 * This class provides HTTP streaming capabilities with intelligent buffering
 * and range request support for seeking. It handles HTTP metadata extraction,
 * efficient buffering, and network error recovery.
 */
class HTTPIOHandler : public IOHandler {
public:
    /**
     * @brief Constructor for HTTP streaming with URL
     * @param url The HTTP/HTTPS URL to stream from
     */
    explicit HTTPIOHandler(const std::string& url);
    
    /**
     * @brief Constructor for HTTP streaming with explicit content length
     * @param url The HTTP/HTTPS URL to stream from
     * @param content_length Known content length in bytes
     */
    HTTPIOHandler(const std::string& url, long content_length);
    
    /**
     * @brief Destructor - cleanup resources
     */
    ~HTTPIOHandler() override;
    
    // IOHandler interface implementation
    size_t read(void* buffer, size_t size, size_t count) override;
    int seek(off_t offset, int whence) override;
    off_t tell() override;
    int close() override;
    bool eof() override;
    off_t getFileSize() override;
    
    // HTTP-specific methods
    
    /**
     * @brief Get the content length from HTTP headers
     * @return Content length in bytes, or -1 if unknown
     */
    long getContentLength() const { return m_content_length; }
    
    /**
     * @brief Get the MIME type from HTTP headers
     * @return MIME type string, empty if not available
     */
    std::string getMimeType() const { return m_mime_type; }
    
    /**
     * @brief Check if server supports range requests
     * @return true if range requests are supported, false otherwise
     */
    bool supportsRangeRequests() const { return m_supports_ranges; }
    
    /**
     * @brief Check if the handler has been initialized
     * @return true if initialized, false otherwise
     */
    bool isInitialized() const { return m_initialized; }

private:
    // HTTP stream properties
    std::string m_url;                    // The HTTP URL
    long m_content_length = -1;           // Total content length (-1 if unknown)
    off_t m_current_position = 0;         // Current logical position in stream
    std::string m_mime_type;              // Content-Type from HTTP headers
    bool m_supports_ranges = false;       // Server supports range requests
    bool m_initialized = false;           // Initialization completed
    
    // Buffering system
    std::vector<uint8_t> m_buffer;        // Data buffer
    size_t m_buffer_offset = 0;           // Current offset within buffer
    off_t m_buffer_start_position = 0;    // Stream position of buffer start
    
    // Buffer configuration
    static constexpr size_t BUFFER_SIZE = 64 * 1024;      // Default 64KB buffer
    static constexpr size_t MIN_RANGE_SIZE = 8 * 1024;    // Minimum 8KB range requests
    
    // Private methods
    
    /**
     * @brief Initialize HTTP stream by performing HEAD request
     * Extracts metadata including Content-Length, Content-Type, and Accept-Ranges
     */
    void initialize();
    
    /**
     * @brief Fill buffer with data from HTTP stream
     * @param position Stream position to start reading from
     * @param min_size Minimum number of bytes to read (default: MIN_RANGE_SIZE)
     * @return true if buffer was filled successfully, false on error
     */
    bool fillBuffer(off_t position, size_t min_size = MIN_RANGE_SIZE);
    
    /**
     * @brief Read data from internal buffer
     * @param buffer Destination buffer
     * @param bytes_to_read Number of bytes to read
     * @return Number of bytes actually read from buffer
     */
    size_t readFromBuffer(void* buffer, size_t bytes_to_read);
    
    /**
     * @brief Check if a position is currently buffered
     * @param position Stream position to check
     * @return true if position is in current buffer, false otherwise
     */
    bool isPositionBuffered(off_t position) const;
    
    /**
     * @brief Parse Content-Type header and normalize MIME type
     * @param content_type Raw Content-Type header value
     * @return Normalized MIME type string
     */
    std::string normalizeMimeType(const std::string& content_type);
};

#endif // HTTPIOHANDLER_H