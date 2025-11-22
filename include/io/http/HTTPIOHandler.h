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
    // Private unlocked methods for thread-safe implementation
    
    /**
     * @brief Read data from HTTP stream (unlocked version)
     * @param buffer Buffer to read data into
     * @param size Size of each element to read
     * @param count Number of elements to read
     * @return Number of elements successfully read
     */
    size_t read_unlocked(void* buffer, size_t size, size_t count) override;
    
    /**
     * @brief Seek to a position in HTTP stream (unlocked version)
     * @param offset Offset to seek to
     * @param whence SEEK_SET, SEEK_CUR, or SEEK_END positioning mode
     * @return 0 on success, -1 on failure
     */
    int seek_unlocked(off_t offset, int whence) override;
    
    /**
     * @brief Get current byte offset position (unlocked version)
     * @return Current position as off_t, -1 on failure
     */
    off_t tell_unlocked() override;
    
    /**
     * @brief Close HTTP stream and cleanup resources (unlocked version)
     * @return 0 on success, standard error codes on failure
     */
    int close_unlocked() override;

private:
    // HTTP stream properties
    std::string m_url;                    // The HTTP URL
    std::atomic<long> m_content_length{-1};           // Total content length (-1 if unknown) (thread-safe)
    std::atomic<off_t> m_current_position{0};         // Current logical position in stream (thread-safe)
    std::string m_mime_type;              // Content-Type from HTTP headers
    std::atomic<bool> m_supports_ranges{false};       // Server supports range requests (thread-safe)
    std::atomic<bool> m_initialized{false};           // Initialization completed (thread-safe)
    
    // Thread safety for HTTP operations
    mutable std::mutex m_http_mutex;          // Protects HTTP client operations
    mutable std::shared_mutex m_buffer_mutex; // Protects buffer operations (allows concurrent reads)
    mutable std::mutex m_initialization_mutex; // Protects initialization process
    
    // Enhanced buffering system
    IOBufferPool::Buffer m_buffer;          // Primary data buffer (from pool)
    size_t m_buffer_offset = 0;           // Current offset within buffer
    off_t m_buffer_start_position = 0;    // Stream position of buffer start
    
    // Read-ahead buffering for performance
    IOBufferPool::Buffer m_read_ahead_buffer;    // Read-ahead buffer for sequential access (from pool)
    off_t m_read_ahead_position = -1;          // Position of read-ahead buffer
    bool m_read_ahead_active = false;          // Read-ahead is active
    
    // Adaptive buffer configuration
    size_t m_buffer_size = 64 * 1024;          // Current buffer size (adaptive)
    size_t m_min_buffer_size = 8 * 1024;       // Minimum 8KB buffer
    size_t m_max_buffer_size = 512 * 1024;     // Maximum 512KB buffer
    size_t m_read_ahead_size = 128 * 1024;     // Read-ahead buffer size
    
    // Performance tracking
    std::chrono::steady_clock::time_point m_last_request_time;
    size_t m_total_requests = 0;
    size_t m_total_bytes_downloaded = 0;
    double m_average_speed = 0.0;  // bytes per second
    
    // Access pattern detection
    off_t m_last_read_position = -1;
    bool m_sequential_access = false;
    size_t m_sequential_reads = 0;
    
    // Connection optimization
    static constexpr size_t MIN_RANGE_SIZE = 8 * 1024;     // Minimum range request size
    static constexpr size_t RANGE_BATCH_SIZE = 256 * 1024; // Batch multiple small requests
    static constexpr size_t SPEED_SAMPLE_COUNT = 10;       // Number of speed samples to average
    
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
    
    /**
     * @brief Update access pattern tracking for optimization
     * @param position Current read position
     */
    void updateAccessPattern(off_t position);
    
    /**
     * @brief Get optimal buffer size based on network conditions and access patterns
     * @return Optimal buffer size in bytes
     */
    size_t getOptimalBufferSize() const;
    
    /**
     * @brief Update network performance statistics
     * @param bytes_transferred Number of bytes transferred
     * @param duration_ms Duration of transfer in milliseconds
     */
    void updatePerformanceStats(size_t bytes_transferred, std::chrono::milliseconds duration_ms);
    
    /**
     * @brief Perform intelligent read-ahead based on access patterns
     * @param current_position Current read position
     * @return true if read-ahead was performed, false otherwise
     */
    bool performReadAhead(off_t current_position);
    
    /**
     * @brief Check if read-ahead buffer contains the requested position
     * @param position Position to check
     * @return true if position is in read-ahead buffer, false otherwise
     */
    bool isPositionInReadAhead(off_t position) const;
    
    // Network error handling methods
    
    /**
     * @brief Distinguish between temporary and permanent network failures
     * @param http_status HTTP status code
     * @param curl_error libcurl error code (if applicable)
     * @return true if error is temporary and may be recoverable, false otherwise
     */
    bool isNetworkErrorRecoverable(int http_status, int curl_error = 0) const;
    
    /**
     * @brief Get descriptive error message for HTTP status codes and network errors
     * @param http_status HTTP status code
     * @param curl_error libcurl error code (if applicable)
     * @param operation_context Context of the operation that failed
     * @return Descriptive error message
     */
    std::string getNetworkErrorMessage(int http_status, int curl_error = 0, const std::string& operation_context = "") const;
    
    /**
     * @brief Handle network timeout conditions without hanging
     * @param operation_name Name of the operation that timed out
     * @param timeout_seconds Timeout duration in seconds
     * @return true if timeout was handled gracefully, false if operation should fail
     */
    bool handleNetworkTimeout(const std::string& operation_name, int timeout_seconds = 30);
    
    /**
     * @brief Implement retry mechanism for transient network errors
     * @param operation_func Function to retry (returns HTTPClient::Response)
     * @param operation_name Name of the operation for logging
     * @param max_retries Maximum number of retry attempts (default: 3)
     * @param base_delay_ms Base delay between retries in milliseconds (default: 1000)
     * @return HTTPClient::Response from successful operation or final failure
     */
    HTTPClient::Response retryNetworkOperation(std::function<HTTPClient::Response()> operation_func, 
                                             const std::string& operation_name, 
                                             int max_retries = 3, 
                                             int base_delay_ms = 1000);
    
    /**
     * @brief Validate network operation parameters and preconditions
     * @param operation_name Name of the operation for error reporting
     * @return true if parameters are valid, false otherwise
     */
    bool validateNetworkOperation(const std::string& operation_name);

private:
    // Network error handling state
    int m_network_retry_count = 0;                    // Current retry count for network operations
    std::chrono::steady_clock::time_point m_last_network_error_time;  // Time of last network error
    std::chrono::steady_clock::time_point m_network_operation_start_time;  // Start time for timeout detection
    bool m_network_timeout_enabled = true;           // Enable network timeout handling
    int m_default_network_timeout_seconds = 30;      // Default timeout for network operations
    
    // Network error statistics
    size_t m_total_network_errors = 0;               // Total network errors encountered
    size_t m_recoverable_network_errors = 0;         // Number of recoverable errors
    size_t m_timeout_errors = 0;                     // Number of timeout errors
    size_t m_connection_errors = 0;                  // Number of connection errors
    size_t m_http_errors = 0;                        // Number of HTTP protocol errors
    
    /**
     * @brief Read data from read-ahead buffer
     * @param buffer Destination buffer
     * @param position Stream position to read from
     * @param bytes_requested Number of bytes to read
     * @return Number of bytes actually read from read-ahead buffer
     */
    size_t readFromReadAhead(void* buffer, off_t position, size_t bytes_requested);
    
    /**
     * @brief Optimize range request size based on network conditions
     * @param requested_size Originally requested size
     * @return Optimized request size
     */
    size_t optimizeRangeRequestSize(size_t requested_size) const;
    
    /**
     * @brief Optimize buffer pool usage based on access patterns and memory pressure
     * This method analyzes current usage patterns and adjusts buffer pool settings
     * to optimize memory usage and performance
     */
    void optimizeBufferPoolUsage();
    
    /**
     * @brief Enforce bounded cache limits to prevent memory leaks
     * This method ensures that buffer usage stays within safe limits and
     * implements aggressive cleanup when necessary
     */
    void enforceBoundedCacheLimits();
    
    /**
     * @brief Clean up resources in error paths to prevent memory leaks
     * @param context Description of the error context
     */
    void cleanupOnError(const std::string& context);
};

#endif // HTTPIOHANDLER_H