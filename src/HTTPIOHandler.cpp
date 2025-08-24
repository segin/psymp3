/*
 * HTTPIOHandler.cpp - HTTP streaming I/O handler implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

HTTPIOHandler::HTTPIOHandler(const std::string& url) 
    : m_url(url) {
    Debug::log("HTTPIOHandler", "Creating HTTP handler for URL: ", url);
    
    // Initialize performance tracking
    m_last_request_time = std::chrono::steady_clock::now();
    m_total_requests = 0;
    m_total_bytes_downloaded = 0;
    m_average_speed = 0.0;
    m_last_read_position = -1;
    m_sequential_access = false;
    m_sequential_reads = 0;
    m_read_ahead_position = -1;
    m_read_ahead_active = false;
    
    // Initialize the HTTP stream
    initialize();
}

HTTPIOHandler::HTTPIOHandler(const std::string& url, long content_length)
    : m_url(url), m_content_length(content_length) {
    Debug::log("HTTPIOHandler", "Creating HTTP handler for URL: ", url, " (content length: ", content_length, ")");
    
    // Initialize performance tracking
    m_last_request_time = std::chrono::steady_clock::now();
    m_total_requests = 0;
    m_total_bytes_downloaded = 0;
    m_average_speed = 0.0;
    m_last_read_position = -1;
    m_sequential_access = false;
    m_sequential_reads = 0;
    m_read_ahead_position = -1;
    m_read_ahead_active = false;
    
    // Initialize the HTTP stream
    initialize();
}

HTTPIOHandler::~HTTPIOHandler() {
    Debug::log("HTTPIOHandler", "Destroying HTTP handler for URL: ", m_url);
    close();
}

void HTTPIOHandler::initialize() {
    Debug::log("HTTPIOHandler", "Initializing HTTP stream for: ", m_url);
    
    // Thread-safe initialization check
    std::lock_guard<std::mutex> init_lock(m_initialization_mutex);
    
    if (m_initialized.load()) {
        Debug::log("HTTPIOHandler", "Already initialized, skipping");
        return;
    }
    
    try {
        // Validate network operation
        if (!validateNetworkOperation("initialize")) {
            return;
        }
        
        // Perform HEAD request to get metadata with retry logic
        HTTPClient::Response response = retryNetworkOperation(
            [this]() { return HTTPClient::head(m_url); },
            "HEAD request",
            3,  // max retries
            1000  // base delay 1 second
        );
        
        if (!response.success) {
            std::string errorMsg = getNetworkErrorMessage(response.statusCode, 0, "HEAD request");
            Debug::log("HTTPIOHandler", errorMsg);
            m_error = response.statusCode > 0 ? response.statusCode : -1;
            cleanupOnError("HEAD request failed during initialization");
            return;
        }
        
        Debug::log("HTTPIOHandler", "HEAD request successful (status: ", response.statusCode, ")");
        
        // Extract Content-Length header for total file size determination
        auto content_length_it = response.headers.find("Content-Length");
        if (content_length_it != response.headers.end()) {
            try {
                long parsed_length = std::stol(content_length_it->second);
                if (m_content_length == -1) {
                    m_content_length = parsed_length;
                }
                Debug::log("HTTPIOHandler", "Content-Length: ", m_content_length, " bytes");
            } catch (const std::exception& e) {
                Debug::log("HTTPIOHandler", "Failed to parse Content-Length: ", e.what());
            }
        } else {
            Debug::log("HTTPIOHandler", "No Content-Length header found");
        }
        
        // Parse Content-Type header and normalize MIME type information
        auto content_type_it = response.headers.find("Content-Type");
        if (content_type_it != response.headers.end()) {
            m_mime_type = normalizeMimeType(content_type_it->second);
            Debug::log("HTTPIOHandler", "Content-Type: ", content_type_it->second, " (normalized: ", m_mime_type, ")");
        } else {
            Debug::log("HTTPIOHandler", "No Content-Type header found");
        }
        
        // Detect Accept-Ranges header for range request capability
        auto accept_ranges_it = response.headers.find("Accept-Ranges");
        if (accept_ranges_it != response.headers.end()) {
            std::string accept_ranges = accept_ranges_it->second;
            // Convert to lowercase for comparison
            std::transform(accept_ranges.begin(), accept_ranges.end(), 
                          accept_ranges.begin(), ::tolower);
            
            m_supports_ranges = (accept_ranges == "bytes");
            Debug::log("HTTPIOHandler", "Accept-Ranges: ", accept_ranges_it->second, " (supports ranges: ", (m_supports_ranges ? "yes" : "no"), ")");
        } else {
            // Some servers support ranges without advertising it
            // We'll try a small range request to test
            Debug::log("HTTPIOHandler", "No Accept-Ranges header, testing range support");
            
            HTTPClient::Response range_test = HTTPClient::getRange(m_url, 0, 0);
            m_supports_ranges = (range_test.success && 
                               (range_test.statusCode == 206 || range_test.statusCode == 200));
            
            Debug::log("HTTPIOHandler", "Range test result: ", (m_supports_ranges ? "supported" : "not supported"), " (status: ", range_test.statusCode, ")");
        }
        
        m_initialized.store(true);
        updateErrorState(0);
        Debug::log("HTTPIOHandler", "HTTP stream initialization completed successfully");
        
    } catch (const std::exception& e) {
        Debug::log("HTTPIOHandler", "Exception during initialization: ", e.what());
        updateErrorState(-1, "Exception during initialization");
        m_initialized.store(false);
        cleanupOnError("Exception during initialization");
    }
}

std::string HTTPIOHandler::normalizeMimeType(const std::string& content_type) {
    if (content_type.empty()) {
        return "";
    }
    
    // Extract MIME type before any semicolon (parameters)
    size_t semicolon_pos = content_type.find(';');
    std::string mime_type = (semicolon_pos != std::string::npos) 
                           ? content_type.substr(0, semicolon_pos)
                           : content_type;
    
    // Trim whitespace
    size_t start = mime_type.find_first_not_of(" \t");
    if (start != std::string::npos) {
        size_t end = mime_type.find_last_not_of(" \t");
        mime_type = mime_type.substr(start, end - start + 1);
    }
    
    // Convert to lowercase for consistency
    std::transform(mime_type.begin(), mime_type.end(), mime_type.begin(), ::tolower);
    
    return mime_type;
}

size_t HTTPIOHandler::read(void* buffer, size_t size, size_t count) {
    // Use base class locking - call base class read which will call our read_unlocked
    return IOHandler::read(buffer, size, count);
}

size_t HTTPIOHandler::read_unlocked(void* buffer, size_t size, size_t count) {
    // Acquire HTTP-specific locks for this operation
    std::shared_lock<std::shared_mutex> buffer_lock(m_buffer_mutex);
    
    if (!m_initialized.load()) {
        Debug::log("HTTPIOHandler", "Attempted read on uninitialized handler");
        return 0;
    }
    
    if (m_closed.load()) {
        Debug::log("HTTPIOHandler", "Attempted read on closed handler");
        return 0;
    }
    
    if (m_eof.load()) {
        Debug::log("HTTPIOHandler", "Attempted read at EOF");
        return 0;
    }
    
    size_t bytes_requested = size * count;
    if (bytes_requested == 0) {
        return 0;
    }
    
    // Get current position atomically
    off_t current_position = m_current_position.load();
    
    // Update access pattern tracking
    updateAccessPattern(current_position);
    
    Debug::log("HTTPIOHandler", "Reading ", bytes_requested, " bytes at position ", static_cast<long long>(current_position),
              " (sequential: ", (m_sequential_access ? "yes" : "no"), ", speed: ", m_average_speed, " B/s)");
    
    size_t total_bytes_read = 0;
    uint8_t* dest_buffer = static_cast<uint8_t*>(buffer);
    
    // Try to read from read-ahead buffer first
    if (m_read_ahead_active && isPositionInReadAhead(m_current_position)) {
        size_t read_ahead_bytes = readFromReadAhead(dest_buffer, m_current_position, bytes_requested);
        total_bytes_read += read_ahead_bytes;
        Debug::log("HTTPIOHandler", "Read ", read_ahead_bytes, " bytes from read-ahead buffer");
    }
    
    // Read remaining data from main buffer or network
    while (total_bytes_read < bytes_requested && !m_eof) {
        size_t remaining_bytes = bytes_requested - total_bytes_read;
        off_t read_position = current_position + total_bytes_read;
        
        // Check if we have data in main buffer
        if (isPositionBuffered(read_position)) {
            // Read from main buffer
            size_t buffer_bytes_read = readFromBuffer(dest_buffer + total_bytes_read, remaining_bytes);
            total_bytes_read += buffer_bytes_read;
            
            if (buffer_bytes_read == 0) {
                break; // Buffer exhausted
            }
        } else {
            // Need to fill buffer - use adaptive sizing
            size_t optimal_size = getOptimalBufferSize();
            size_t request_size = std::max(remaining_bytes, optimal_size);
            
            // Optimize request size for network conditions
            request_size = optimizeRangeRequestSize(request_size);
            
            if (!fillBuffer(read_position, request_size)) {
                std::string errorMsg = getErrorMessage(-1, "Failed to fill buffer for read operation");
                Debug::log("HTTPIOHandler", errorMsg);
                updateErrorState(-1, "fillBuffer failed during read operation");
                cleanupOnError("fillBuffer failed during read operation");
                break;
            }
            
            // Read from newly filled buffer
            size_t buffer_bytes_read = readFromBuffer(dest_buffer + total_bytes_read, remaining_bytes);
            total_bytes_read += buffer_bytes_read;
            
            if (buffer_bytes_read == 0) {
                updateEofState(true);
                break;
            }
        }
    }
    
    // Update position atomically
    off_t new_position = current_position + total_bytes_read;
    m_current_position.store(new_position);
    updatePosition(new_position);
    
    // Check for EOF condition
    long content_length = m_content_length.load();
    if (content_length > 0 && new_position >= content_length) {
        updateEofState(true);
    }
    
    // Perform read-ahead for sequential access
    if (m_sequential_access && total_bytes_read > 0) {
        performReadAhead(new_position);
    }
    
    Debug::log("HTTPIOHandler", "Read ", total_bytes_read, " bytes, new position: ", static_cast<long long>(new_position));
    
    return total_bytes_read / size;  // Return number of complete elements read
}

int HTTPIOHandler::seek(off_t offset, int whence) {
    // Use base class locking - call base class seek which will call our seek_unlocked
    return IOHandler::seek(offset, whence);
}

int HTTPIOHandler::seek_unlocked(off_t offset, int whence) {
    // Acquire HTTP-specific lock for this operation
    std::lock_guard<std::mutex> http_lock(m_http_mutex);
    
    if (!m_initialized.load()) {
        Debug::log("HTTPIOHandler", "Attempted seek on uninitialized handler");
        return -1;
    }
    
    if (m_closed.load()) {
        Debug::log("HTTPIOHandler", "Attempted seek on closed handler");
        return -1;
    }
    
    off_t current_pos = m_current_position.load();
    long content_length = m_content_length.load();
    off_t new_position;
    
    switch (whence) {
        case SEEK_SET:
            new_position = offset;
            break;
        case SEEK_CUR:
            new_position = current_pos + offset;
            break;
        case SEEK_END:
            if (content_length <= 0) {
                Debug::log("HTTPIOHandler", "SEEK_END not supported without known content length");
                return -1;
            }
            new_position = content_length + offset;
            break;
        default:
            Debug::log("HTTPIOHandler", "Invalid seek whence: ", whence);
            return -1;
    }
    
    // Validate new position
    if (new_position < 0) {
        Debug::log("HTTPIOHandler", "Seek to negative position: ", static_cast<long long>(new_position));
        return -1;
    }
    
    if (content_length > 0 && new_position > content_length) {
        Debug::log("HTTPIOHandler", "Seek beyond end of content: ", static_cast<long long>(new_position), " > ", content_length);
        return -1;
    }
    
    // Check if server supports range requests for seeking
    if (new_position != current_pos && !m_supports_ranges.load()) {
        Debug::log("HTTPIOHandler", "Seek requested but server doesn't support range requests");
        return -1;
    }
    
    Debug::log("HTTPIOHandler", "Seeking from ", static_cast<long long>(current_pos), " to ", static_cast<long long>(new_position));
    
    m_current_position.store(new_position);
    updatePosition(new_position);
    
    // Clear EOF flag if we're not at the end
    if (content_length <= 0 || new_position < content_length) {
        updateEofState(false);
    }
    
    return 0;
}

off_t HTTPIOHandler::tell() {
    // Use base class locking - call base class tell which will call our tell_unlocked
    return IOHandler::tell();
}

off_t HTTPIOHandler::tell_unlocked() {
    // Return current position using atomic load
    return m_current_position.load();
}

int HTTPIOHandler::close() {
    // Use base class locking - call base class close which will call our close_unlocked
    return IOHandler::close();
}

int HTTPIOHandler::close_unlocked() {
    // Acquire HTTP-specific locks for this operation
    std::unique_lock<std::shared_mutex> buffer_lock(m_buffer_mutex);
    
    if (m_closed.load()) {
        return 0;
    }
    
    Debug::log("HTTPIOHandler", "Closing HTTP handler for: ", m_url, 
              " (total requests: ", m_total_requests, ", total bytes: ", m_total_bytes_downloaded, 
              ", avg speed: ", m_average_speed, " B/s)");
    
    // Release buffers back to pool
    m_buffer = IOBufferPool::Buffer();
    m_read_ahead_buffer = IOBufferPool::Buffer();
    
    // Update memory tracking
    updateMemoryUsage(0);
    
    // Reset state
    m_read_ahead_active = false;
    m_read_ahead_position = -1;
    
    updateClosedState(true);
    return 0;
}

bool HTTPIOHandler::eof() {
    // Thread-safe EOF check using atomic load
    return m_eof.load();
}

off_t HTTPIOHandler::getFileSize() {
    // Thread-safe file size access using atomic load
    return m_content_length.load();
}

bool HTTPIOHandler::fillBuffer(off_t position, size_t min_size) {
    auto start_time = std::chrono::steady_clock::now();
    
    Debug::log("HTTPIOHandler", "Filling buffer at position ", static_cast<long long>(position), " (min size: ", min_size, ")");
    
    // Calculate range size - use adaptive sizing
    size_t range_size = std::max(min_size, m_buffer_size);
    
    // Don't read beyond content length if known
    if (m_content_length > 0) {
        off_t remaining = m_content_length - position;
        if (remaining <= 0) {
            Debug::log("HTTPIOHandler", "Position beyond content length");
            m_eof = true;
            return false;
        }
        range_size = std::min(range_size, static_cast<size_t>(remaining));
    }
    
    // Validate network operation
    if (!validateNetworkOperation("fillBuffer")) {
        cleanupOnError("Network operation validation failed in fillBuffer");
        return false;
    }
    
    // Perform HTTP request with retry logic
    HTTPClient::Response response = retryNetworkOperation(
        [this, position, range_size]() {
            // Use range request if supported or if we're not at the beginning
            if (m_supports_ranges || position > 0) {
                long end_byte = position + range_size - 1;
                Debug::log("HTTPIOHandler", "Making range request: bytes=", static_cast<long long>(position), "-", end_byte);
                return HTTPClient::getRange(m_url, position, end_byte);
            } else {
                // Fall back to regular GET request
                Debug::log("HTTPIOHandler", "Making regular GET request");
                return HTTPClient::get(m_url);
            }
        },
        "fillBuffer",
        3,  // max retries
        1000  // base delay 1 second
    );
    
    // Check response success and status codes
    if (!response.success) {
        std::string errorMsg = getNetworkErrorMessage(response.statusCode, 0, "buffer fill");
        Debug::log("HTTPIOHandler", errorMsg);
        m_error = response.statusCode > 0 ? response.statusCode : -1;
        cleanupOnError("HTTP request failed in fillBuffer");
        return false;
    }
    
    // For range requests, accept both 206 (Partial Content) and 200 (OK) responses
    if ((m_supports_ranges || position > 0) && response.statusCode != 206 && response.statusCode != 200) {
        std::string errorMsg = getNetworkErrorMessage(response.statusCode, 0, "range request");
        Debug::log("HTTPIOHandler", errorMsg);
        m_error = response.statusCode;
        return false;
    }
    
    Debug::log("HTTPIOHandler", "HTTP request successful (status: ", response.statusCode, ", body size: ", response.body.size(), ")");
    
    // Check memory limits before allocating
    if (!checkMemoryLimits(response.body.size())) {
        Debug::log("HTTPIOHandler", "Memory limits would be exceeded for ", response.body.size(), " bytes");
        m_error = ENOMEM;
        cleanupOnError("Memory limits exceeded in fillBuffer");
        return false;
    }
    
    // Get buffer from pool and store response data
    m_buffer = IOBufferPool::getInstance().acquire(response.body.size());
    if (m_buffer.empty()) {
        Debug::log("HTTPIOHandler", "Failed to acquire buffer from pool for ", response.body.size(), " bytes");
        cleanupOnError("Failed to acquire buffer from pool in fillBuffer");
        return false;
    }
    
    std::memcpy(m_buffer.data(), response.body.data(), response.body.size());
    m_buffer_offset = 0;
    m_buffer_start_position = position;
    
    // Update memory usage tracking
    updateMemoryUsage(m_buffer.size());
    
    // Optimize buffer pool usage based on access patterns
    optimizeBufferPoolUsage();
    
    // Enforce bounded cache limits to prevent memory leaks
    enforceBoundedCacheLimits();
    
    // Periodic global memory optimization and buffer pool optimization
    static size_t global_optimization_counter = 0;
    global_optimization_counter++;
    
    if (global_optimization_counter % 50 == 0) { // Every 50 buffer fills
        optimizeBufferPoolUsage();
        enforceBoundedCacheLimits();
    }
    
    if (global_optimization_counter % 100 == 0) { // Every 100 buffer fills
        IOHandler::performMemoryOptimization();
        Debug::log("memory", "HTTPIOHandler::fillBuffer() - Performed global memory optimization");
    }
    
    // Update performance statistics
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    updatePerformanceStats(response.body.size(), duration);
    
    Debug::log("HTTPIOHandler", "Buffer filled: ", m_buffer.size(), " bytes starting at position ", 
              static_cast<long long>(m_buffer_start_position), " (took ", duration.count(), "ms)");
    
    return true;
}

size_t HTTPIOHandler::readFromBuffer(void* buffer, size_t bytes_to_read) {
    if (m_buffer.empty()) {
        Debug::log("HTTPIOHandler", "Buffer is empty");
        return 0;
    }
    
    // Calculate offset within buffer for current position
    off_t buffer_relative_pos = m_current_position - m_buffer_start_position;
    
    if (buffer_relative_pos < 0 || 
        static_cast<size_t>(buffer_relative_pos) >= m_buffer.size()) {
        Debug::log("HTTPIOHandler", "Current position not in buffer");
        return 0;
    }
    
    // Calculate how many bytes we can read from buffer
    size_t available_bytes = m_buffer.size() - static_cast<size_t>(buffer_relative_pos);
    size_t bytes_to_copy = std::min(bytes_to_read, available_bytes);
    
    // Copy data from buffer
    std::memcpy(buffer, 
                m_buffer.data() + static_cast<size_t>(buffer_relative_pos), 
                bytes_to_copy);
    
    Debug::log("HTTPIOHandler", "Read ", bytes_to_copy, " bytes from buffer (available: ", available_bytes, ", requested: ", bytes_to_read, ")");
    
    return bytes_to_copy;
}

bool HTTPIOHandler::isPositionBuffered(off_t position) const {
    if (m_buffer.empty()) {
        return false;
    }
    
    off_t buffer_end = m_buffer_start_position + static_cast<off_t>(m_buffer.size());
    return (position >= m_buffer_start_position && position < buffer_end);
}

void HTTPIOHandler::updateAccessPattern(off_t position) {
    if (m_last_read_position >= 0) {
        off_t position_diff = position - m_last_read_position;
        
        // Consider it sequential if reading forward within reasonable range
        static const off_t MAX_SEQUENTIAL_GAP = 128 * 1024; // 128KB
        if (position_diff >= 0 && position_diff <= MAX_SEQUENTIAL_GAP) {
            m_sequential_reads++;
            if (m_sequential_reads >= 3 && !m_sequential_access) {
                m_sequential_access = true;
                Debug::log("HTTPIOHandler", "Sequential access pattern detected");
            }
        } else {
            m_sequential_reads = 0;
            if (m_sequential_access) {
                m_sequential_access = false;
                m_read_ahead_active = false; // Disable read-ahead for random access
                Debug::log("HTTPIOHandler", "Sequential access pattern broken");
            }
        }
    }
    
    m_last_read_position = position;
}

size_t HTTPIOHandler::getOptimalBufferSize() const {
    size_t optimal_size = m_buffer_size;
    
    // Adjust based on network speed
    if (m_average_speed > 0) {
        // For fast connections, use larger buffers
        if (m_average_speed > 1024 * 1024) { // > 1MB/s
            optimal_size = std::min(m_max_buffer_size, static_cast<size_t>(256 * 1024)); // 256KB
        } else if (m_average_speed > 512 * 1024) { // > 512KB/s
            optimal_size = std::min(m_max_buffer_size, static_cast<size_t>(128 * 1024)); // 128KB
        } else if (m_average_speed < 64 * 1024) { // < 64KB/s
            optimal_size = std::max(m_min_buffer_size, static_cast<size_t>(32 * 1024)); // 32KB
        }
    }
    
    // Adjust for sequential vs random access
    if (m_sequential_access) {
        optimal_size = std::min(m_max_buffer_size, optimal_size * 2); // Double for sequential
    }
    
    return optimal_size;
}

void HTTPIOHandler::updatePerformanceStats(size_t bytes_transferred, std::chrono::milliseconds duration_ms) {
    m_total_requests++;
    m_total_bytes_downloaded += bytes_transferred;
    
    if (duration_ms.count() > 0) {
        double speed = static_cast<double>(bytes_transferred) / (duration_ms.count() / 1000.0);
        
        // Use exponential moving average for speed calculation
        if (m_average_speed == 0.0) {
            m_average_speed = speed;
        } else {
            const double alpha = 0.3; // Smoothing factor
            m_average_speed = alpha * speed + (1.0 - alpha) * m_average_speed;
        }
        
        // Adapt buffer size based on performance
        if (m_total_requests % 5 == 0) { // Adjust every 5 requests
            size_t new_buffer_size = getOptimalBufferSize();
            if (new_buffer_size != m_buffer_size) {
                Debug::log("HTTPIOHandler", "Adapting buffer size from ", m_buffer_size, " to ", new_buffer_size, 
                          " (speed: ", m_average_speed, " B/s)");
                m_buffer_size = new_buffer_size;
            }
        }
    }
    
    m_last_request_time = std::chrono::steady_clock::now();
}

bool HTTPIOHandler::performReadAhead(off_t current_position) {
    if (!m_sequential_access || m_read_ahead_active) {
        return false; // Only read-ahead for sequential access
    }
    
    // Don't read-ahead if we're near the end
    if (m_content_length > 0 && current_position + static_cast<off_t>(m_read_ahead_size) >= m_content_length) {
        return false;
    }
    
    off_t read_ahead_start = current_position;
    
    // Check if we already have this data in main buffer
    if (isPositionBuffered(read_ahead_start)) {
        off_t buffer_end = m_buffer_start_position + static_cast<off_t>(m_buffer.size());
        read_ahead_start = buffer_end; // Start read-ahead after current buffer
    }
    
    Debug::log("HTTPIOHandler", "Performing read-ahead at position ", static_cast<long long>(read_ahead_start));
    
    // Calculate read-ahead size
    size_t read_size = m_read_ahead_size;
    if (m_content_length > 0) {
        off_t remaining = m_content_length - read_ahead_start;
        if (remaining <= 0) {
            return false;
        }
        read_size = std::min(read_size, static_cast<size_t>(remaining));
    }
    
    // Perform read-ahead request with error handling
    if (!m_supports_ranges) {
        return false; // Can't do read-ahead without range support
    }
    
    HTTPClient::Response response = retryNetworkOperation(
        [this, read_ahead_start, read_size]() {
            long end_byte = read_ahead_start + read_size - 1;
            return HTTPClient::getRange(m_url, read_ahead_start, end_byte);
        },
        "read-ahead",
        2,  // fewer retries for read-ahead (not critical)
        500  // shorter delay for read-ahead
    );
    
    if (response.success && (response.statusCode == 206 || response.statusCode == 200)) {
        // Check memory limits before allocating read-ahead buffer
        if (!checkMemoryLimits(response.body.size())) {
            Debug::log("HTTPIOHandler", "Memory limits would be exceeded for read-ahead buffer of ", response.body.size(), " bytes");
            return false;
        }
        
        // Get read-ahead buffer from pool and store response data
        m_read_ahead_buffer = IOBufferPool::getInstance().acquire(response.body.size());
        if (m_read_ahead_buffer.empty()) {
            Debug::log("HTTPIOHandler", "Failed to acquire read-ahead buffer from pool");
            return false;
        }
        
        std::memcpy(m_read_ahead_buffer.data(), response.body.data(), response.body.size());
        m_read_ahead_position = read_ahead_start;
        m_read_ahead_active = true;
        
        // Update memory usage tracking (include both main and read-ahead buffers)
        updateMemoryUsage(m_buffer.size() + m_read_ahead_buffer.size());
        
        Debug::log("HTTPIOHandler", "Read-ahead successful: ", m_read_ahead_buffer.size(), 
                  " bytes at position ", static_cast<long long>(m_read_ahead_position));
        return true;
    }
    
    Debug::log("HTTPIOHandler", "Read-ahead failed: ", response.statusMessage);
    return false;
}

bool HTTPIOHandler::isPositionInReadAhead(off_t position) const {
    if (!m_read_ahead_active || m_read_ahead_buffer.empty()) {
        return false;
    }
    
    off_t read_ahead_end = m_read_ahead_position + static_cast<off_t>(m_read_ahead_buffer.size());
    return (position >= m_read_ahead_position && position < read_ahead_end);
}

size_t HTTPIOHandler::readFromReadAhead(void* buffer, off_t position, size_t bytes_requested) {
    if (!isPositionInReadAhead(position)) {
        return 0;
    }
    
    off_t offset_in_buffer = position - m_read_ahead_position;
    size_t available_bytes = m_read_ahead_buffer.size() - static_cast<size_t>(offset_in_buffer);
    size_t bytes_to_copy = std::min(bytes_requested, available_bytes);
    
    std::memcpy(buffer, m_read_ahead_buffer.data() + static_cast<size_t>(offset_in_buffer), bytes_to_copy);
    
    return bytes_to_copy;
}

size_t HTTPIOHandler::optimizeRangeRequestSize(size_t requested_size) const {
    // Batch small requests to reduce HTTP overhead
    if (requested_size < RANGE_BATCH_SIZE) {
        return std::min(RANGE_BATCH_SIZE, m_buffer_size);
    }
    
    // For large requests, align to buffer boundaries for efficiency
    size_t aligned_size = ((requested_size + m_buffer_size - 1) / m_buffer_size) * m_buffer_size;
    
    // Cap at maximum buffer size to avoid excessive memory usage
    return std::min(aligned_size, m_max_buffer_size);
}

void HTTPIOHandler::optimizeBufferPoolUsage() {
    // Get current buffer pool statistics
    auto pool_stats = IOBufferPool::getInstance().getStats();
    
    // Calculate hit rate and memory efficiency
    size_t total_requests = pool_stats["total_pool_hits"] + pool_stats["total_pool_misses"];
    if (total_requests == 0) {
        return; // No data to optimize on
    }
    
    double hit_rate = static_cast<double>(pool_stats["total_pool_hits"]) / total_requests;
    double memory_utilization = static_cast<double>(pool_stats["current_pool_size"]) / pool_stats["max_pool_size"];
    
    Debug::log("memory", "HTTPIOHandler::optimizeBufferPoolUsage() - Hit rate: ", hit_rate * 100, 
              "%, Memory utilization: ", memory_utilization * 100, "%");
    
    // Get memory optimizer recommendations
    MemoryOptimizer& optimizer = MemoryOptimizer::getInstance();
    size_t recommended_pool_size, recommended_buffers_per_size;
    optimizer.getRecommendedBufferPoolParams(recommended_pool_size, recommended_buffers_per_size);
    
    // Apply memory optimizer recommendations
    IOBufferPool::getInstance().setMaxPoolSize(recommended_pool_size);
    IOBufferPool::getInstance().setMaxBuffersPerSize(recommended_buffers_per_size);
    
    // Adjust buffer pool settings based on performance metrics and memory pressure
    MemoryOptimizer::MemoryPressureLevel pressure = optimizer.getMemoryPressureLevel();
    
    if (pressure >= MemoryOptimizer::MemoryPressureLevel::High) {
        // High memory pressure - be conservative
        if (hit_rate < 0.6) {
            // Low hit rate under pressure - reduce pool size aggressively
            size_t reduced_size = recommended_pool_size * 0.5;
            IOBufferPool::getInstance().setMaxPoolSize(reduced_size);
            Debug::log("memory", "HTTPIOHandler::optimizeBufferPoolUsage() - Reduced pool size to ", reduced_size, " bytes due to high memory pressure and low hit rate");
        }
    } else if (pressure == MemoryOptimizer::MemoryPressureLevel::Low) {
        // Low memory pressure - can be more aggressive with optimization
        if (hit_rate > 0.9 && memory_utilization < 0.5) {
            // High hit rate with low memory usage - can afford to increase pool size
            size_t increased_size = std::min(static_cast<size_t>(recommended_pool_size * 1.5), static_cast<size_t>(32 * 1024 * 1024)); // Cap at 32MB
            IOBufferPool::getInstance().setMaxPoolSize(increased_size);
            Debug::log("memory", "HTTPIOHandler::optimizeBufferPoolUsage() - Increased pool size to ", increased_size, " bytes due to low memory pressure and high hit rate");
        }
    }
    
    // Optimize buffer sizes based on access patterns and memory pressure
    size_t optimal_buffer_size = optimizer.getOptimalBufferSize(m_buffer_size, "http", m_sequential_access);
    
    if (optimal_buffer_size != m_buffer_size) {
        Debug::log("memory", "HTTPIOHandler::optimizeBufferPoolUsage() - Adjusting buffer size from ", m_buffer_size, 
                  " to ", optimal_buffer_size, " based on memory optimizer recommendations");
        m_buffer_size = optimal_buffer_size;
        
        // Update min/max buffer sizes based on memory pressure
        if (pressure >= MemoryOptimizer::MemoryPressureLevel::High) {
            m_max_buffer_size = std::min(m_max_buffer_size, static_cast<size_t>(128 * 1024)); // Cap at 128KB under pressure
            m_min_buffer_size = std::max(m_min_buffer_size, static_cast<size_t>(4 * 1024));   // Min 4KB
        } else if (pressure == MemoryOptimizer::MemoryPressureLevel::Low) {
            m_max_buffer_size = std::min(static_cast<size_t>(1024 * 1024), m_max_buffer_size * 2); // Can use up to 1MB under low pressure
        }
        
        // Ensure buffer size is within bounds
        m_buffer_size = std::max(m_min_buffer_size, std::min(m_max_buffer_size, m_buffer_size));
    }
    
    // Register memory usage with optimizer
    size_t current_memory_usage = 0;
    if (!m_buffer.empty()) {
        current_memory_usage += m_buffer.size();
    }
    if (!m_read_ahead_buffer.empty()) {
        current_memory_usage += m_read_ahead_buffer.size();
    }
    
    // Update memory tracking
    static size_t last_reported_usage = 0;
    if (current_memory_usage != last_reported_usage) {
        if (last_reported_usage > 0) {
            optimizer.registerDeallocation(last_reported_usage, "http");
        }
        if (current_memory_usage > 0) {
            optimizer.registerAllocation(current_memory_usage, "http");
        }
        last_reported_usage = current_memory_usage;
    }
}
// Network error handling methods

bool HTTPIOHandler::isNetworkErrorRecoverable(int http_status, int curl_error) const {
    // Check libcurl errors first
    if (curl_error != 0) {
        switch (curl_error) {
            // Temporary network errors that may be recoverable
            case CURLE_COULDNT_CONNECT:
            case CURLE_COULDNT_RESOLVE_HOST:
            case CURLE_COULDNT_RESOLVE_PROXY:
            case CURLE_OPERATION_TIMEDOUT:
            case CURLE_RECV_ERROR:
            case CURLE_SEND_ERROR:
            case CURLE_PARTIAL_FILE:
            case CURLE_GOT_NOTHING:
            case CURLE_SSL_CONNECT_ERROR:
            case CURLE_AGAIN:
                Debug::log("http", "HTTPIOHandler::isNetworkErrorRecoverable() - libcurl error ", curl_error, " is potentially recoverable");
                return true;
                
            // Permanent errors that are not recoverable
            case CURLE_URL_MALFORMAT:
            case CURLE_NOT_BUILT_IN:
            case CURLE_UNSUPPORTED_PROTOCOL:
            case CURLE_FAILED_INIT:
            case CURLE_OUT_OF_MEMORY:
            case CURLE_SSL_CACERT:
            case CURLE_TOO_MANY_REDIRECTS:
                Debug::log("http", "HTTPIOHandler::isNetworkErrorRecoverable() - libcurl error ", curl_error, " is not recoverable");
                return false;
                
            default:
                Debug::log("http", "HTTPIOHandler::isNetworkErrorRecoverable() - Unknown libcurl error ", curl_error, ", assuming not recoverable");
                return false;
        }
    }
    
    // Check HTTP status codes
    if (http_status > 0) {
        // 1xx Informational - not errors
        if (http_status >= 100 && http_status < 200) {
            return true;
        }
        
        // 2xx Success - not errors
        if (http_status >= 200 && http_status < 300) {
            return true;
        }
        
        // 3xx Redirection - usually handled automatically by libcurl
        if (http_status >= 300 && http_status < 400) {
            return true;
        }
        
        // 4xx Client errors - mostly permanent
        if (http_status >= 400 && http_status < 500) {
            switch (http_status) {
                case 408: // Request Timeout
                case 429: // Too Many Requests
                case 449: // Retry With (Microsoft extension)
                    Debug::log("http", "HTTPIOHandler::isNetworkErrorRecoverable() - HTTP status ", http_status, " is potentially recoverable");
                    return true;
                    
                case 400: // Bad Request
                case 401: // Unauthorized
                case 403: // Forbidden
                case 404: // Not Found
                case 405: // Method Not Allowed
                case 406: // Not Acceptable
                case 410: // Gone
                case 413: // Payload Too Large
                case 414: // URI Too Long
                case 415: // Unsupported Media Type
                case 416: // Range Not Satisfiable
                default:
                    Debug::log("http", "HTTPIOHandler::isNetworkErrorRecoverable() - HTTP status ", http_status, " is not recoverable");
                    return false;
            }
        }
        
        // 5xx Server errors - often temporary and recoverable
        if (http_status >= 500 && http_status < 600) {
            switch (http_status) {
                case 500: // Internal Server Error
                case 502: // Bad Gateway
                case 503: // Service Unavailable
                case 504: // Gateway Timeout
                case 507: // Insufficient Storage
                case 508: // Loop Detected
                case 510: // Not Extended
                case 511: // Network Authentication Required
                    Debug::log("http", "HTTPIOHandler::isNetworkErrorRecoverable() - HTTP status ", http_status, " is potentially recoverable");
                    return true;
                    
                case 501: // Not Implemented
                case 505: // HTTP Version Not Supported
                case 506: // Variant Also Negotiates
                default:
                    Debug::log("http", "HTTPIOHandler::isNetworkErrorRecoverable() - HTTP status ", http_status, " is not recoverable");
                    return false;
            }
        }
    }
    
    Debug::log("http", "HTTPIOHandler::isNetworkErrorRecoverable() - Unknown error condition, assuming not recoverable");
    return false;
}

std::string HTTPIOHandler::getNetworkErrorMessage(int http_status, int curl_error, const std::string& operation_context) const {
    std::string message = "Network operation failed";
    
    if (!operation_context.empty()) {
        message += " during " + operation_context;
    }
    
    message += " for URL '" + m_url + "'";
    
    // Add libcurl error details
    if (curl_error != 0) {
        message += " (libcurl error " + std::to_string(curl_error) + ": " + curl_easy_strerror(static_cast<CURLcode>(curl_error)) + ")";
    }
    
    // Add HTTP status details
    if (http_status > 0) {
        message += " (HTTP status " + std::to_string(http_status);
        
        // Add descriptive status messages
        switch (http_status) {
            // 4xx Client errors
            case 400: message += ": Bad Request"; break;
            case 401: message += ": Unauthorized"; break;
            case 403: message += ": Forbidden"; break;
            case 404: message += ": Not Found"; break;
            case 405: message += ": Method Not Allowed"; break;
            case 408: message += ": Request Timeout"; break;
            case 410: message += ": Gone"; break;
            case 413: message += ": Payload Too Large"; break;
            case 414: message += ": URI Too Long"; break;
            case 415: message += ": Unsupported Media Type"; break;
            case 416: message += ": Range Not Satisfiable"; break;
            case 429: message += ": Too Many Requests"; break;
            
            // 5xx Server errors
            case 500: message += ": Internal Server Error"; break;
            case 501: message += ": Not Implemented"; break;
            case 502: message += ": Bad Gateway"; break;
            case 503: message += ": Service Unavailable"; break;
            case 504: message += ": Gateway Timeout"; break;
            case 505: message += ": HTTP Version Not Supported"; break;
            case 507: message += ": Insufficient Storage"; break;
            case 508: message += ": Loop Detected"; break;
            case 510: message += ": Not Extended"; break;
            case 511: message += ": Network Authentication Required"; break;
            
            default:
                // No additional description for unknown status codes
                break;
        }
        
        message += ")";
    }
    
    // Add recovery suggestions for recoverable errors
    if (isNetworkErrorRecoverable(http_status, curl_error)) {
        message += " - This error may be temporary and the operation could be retried";
        
        // Specific suggestions based on error type
        if (curl_error == CURLE_COULDNT_CONNECT || curl_error == CURLE_COULDNT_RESOLVE_HOST) {
            message += " (check network connectivity)";
        } else if (curl_error == CURLE_OPERATION_TIMEDOUT || http_status == 408 || http_status == 504) {
            message += " (try increasing timeout or retrying later)";
        } else if (http_status == 429) {
            message += " (server is rate limiting, retry with longer delay)";
        } else if (http_status >= 500 && http_status < 600) {
            message += " (server error, retry later)";
        }
    }
    
    return message;
}

bool HTTPIOHandler::handleNetworkTimeout(const std::string& operation_name, int timeout_seconds) {
    Debug::log("http", "HTTPIOHandler::handleNetworkTimeout() - Handling timeout for ", operation_name, " (", timeout_seconds, "s)");
    
    if (!m_network_timeout_enabled) {
        Debug::log("http", "HTTPIOHandler::handleNetworkTimeout() - Network timeout handling disabled");
        return true;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_network_operation_start_time).count();
    
    if (elapsed > timeout_seconds) {
        m_error = ETIMEDOUT;
        m_timeout_errors++;
        m_total_network_errors++;
        
        // Log detailed timeout information
        Debug::log("http", "HTTPIOHandler::handleNetworkTimeout() - ", operation_name, " timed out after ", elapsed, " seconds (limit: ", timeout_seconds, ")");
        Debug::log("http", "HTTPIOHandler::handleNetworkTimeout() - URL: ", m_url);
        Debug::log("http", "HTTPIOHandler::handleNetworkTimeout() - Position: ", m_current_position);
        Debug::log("http", "HTTPIOHandler::handleNetworkTimeout() - Total timeout errors: ", m_timeout_errors);
        
        // Network timeouts are often recoverable
        if (isNetworkErrorRecoverable(0, CURLE_OPERATION_TIMEDOUT)) {
            Debug::log("http", "HTTPIOHandler::handleNetworkTimeout() - Timeout may be recoverable, resetting timeout tracking");
            
            // Reset timeout tracking
            m_network_operation_start_time = now;
            
            // Invalidate buffers that might contain stale data
            m_buffer = IOBufferPool::Buffer();
            m_buffer_offset = 0;
            m_buffer_start_position = 0;
            
            // Disable read-ahead temporarily after timeout
            m_read_ahead_active = false;
            m_read_ahead_buffer = IOBufferPool::Buffer();
            
            return true; // Allow retry
        }
        
        return false; // Timeout is not recoverable
    }
    
    return true; // No timeout
}

HTTPClient::Response HTTPIOHandler::retryNetworkOperation(std::function<HTTPClient::Response()> operation_func, 
                                                         const std::string& operation_name, 
                                                         int max_retries, 
                                                         int base_delay_ms) {
    HTTPClient::Response response;
    int retry_count = 0;
    
    Debug::log("http", "HTTPIOHandler::retryNetworkOperation() - Starting ", operation_name, " with up to ", max_retries, " retries");
    
    while (retry_count <= max_retries) {
        // Reset network operation timeout tracking
        m_network_operation_start_time = std::chrono::steady_clock::now();
        
        // Attempt the operation
        response = operation_func();
        
        if (response.success) {
            if (retry_count > 0) {
                Debug::log("http", "HTTPIOHandler::retryNetworkOperation() - ", operation_name, " succeeded after ", retry_count, " retries");
                m_recoverable_network_errors++;
            }
            return response;
        }
        
        // Operation failed, check if we should retry
        if (retry_count >= max_retries) {
            Debug::log("http", "HTTPIOHandler::retryNetworkOperation() - ", operation_name, " failed after ", max_retries, " retries, giving up");
            m_total_network_errors++;
            break;
        }
        
        // Determine error type for statistics
        bool is_connection_error = (response.statusCode == 0); // Usually indicates connection failure
        bool is_http_error = (response.statusCode >= 400);
        
        if (is_connection_error) {
            m_connection_errors++;
        } else if (is_http_error) {
            m_http_errors++;
        }
        
        // Check if error is recoverable
        if (!isNetworkErrorRecoverable(response.statusCode, 0)) {
            Debug::log("http", "HTTPIOHandler::retryNetworkOperation() - ", operation_name, " failed with non-recoverable error: HTTP ", response.statusCode, ", not retrying");
            m_total_network_errors++;
            break;
        }
        
        retry_count++;
        m_network_retry_count = retry_count;
        m_last_network_error_time = std::chrono::steady_clock::now();
        
        Debug::log("http", "HTTPIOHandler::retryNetworkOperation() - ", operation_name, " failed (HTTP ", response.statusCode, ": ", response.statusMessage, "), retrying (", retry_count, "/", max_retries, ")");
        
        // Calculate delay with exponential backoff and jitter
        int delay = base_delay_ms * (1 << (retry_count - 1)); // Exponential backoff
        
        // Add jitter to prevent thundering herd
        int jitter = rand() % (delay / 4 + 1); // Up to 25% jitter
        delay += jitter;
        
        // Cap maximum delay at 30 seconds
        delay = std::min(delay, 30000);
        
        // Special handling for rate limiting (HTTP 429)
        if (response.statusCode == 429) {
            // Check for Retry-After header
            auto retry_after_it = response.headers.find("Retry-After");
            if (retry_after_it != response.headers.end()) {
                try {
                    int retry_after_seconds = std::stoi(retry_after_it->second);
                    delay = std::min(retry_after_seconds * 1000, 60000); // Cap at 60 seconds
                    Debug::log("http", "HTTPIOHandler::retryNetworkOperation() - Using Retry-After header: ", retry_after_seconds, " seconds");
                } catch (const std::exception& e) {
                    Debug::log("http", "HTTPIOHandler::retryNetworkOperation() - Failed to parse Retry-After header: ", e.what());
                }
            } else {
                // Use longer delay for rate limiting
                delay = std::max(delay, 5000); // At least 5 seconds for rate limiting
            }
        }
        
        Debug::log("http", "HTTPIOHandler::retryNetworkOperation() - Waiting ", delay, "ms before retry");
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        
        // Clear any cached state that might be stale
        m_buffer = IOBufferPool::Buffer();
        m_buffer_offset = 0;
        m_buffer_start_position = 0;
        m_read_ahead_active = false;
        m_read_ahead_buffer = IOBufferPool::Buffer();
    }
    
    m_total_network_errors++;
    return response; // Return the final failed response
}

bool HTTPIOHandler::validateNetworkOperation(const std::string& operation_name) {
    // Reset error state
    m_error = 0;
    
    // Check if handler is initialized
    if (!m_initialized) {
        m_error = EINVAL;  // Invalid argument
        Debug::log("http", "HTTPIOHandler::validateNetworkOperation() - ", operation_name, " failed: handler not initialized");
        return false;
    }
    
    // Check if handler is closed
    if (m_closed) {
        m_error = EBADF;  // Bad file descriptor
        Debug::log("http", "HTTPIOHandler::validateNetworkOperation() - ", operation_name, " failed: handler is closed");
        return false;
    }
    
    // Validate URL
    if (m_url.empty()) {
        m_error = EINVAL;  // Invalid argument
        Debug::log("http", "HTTPIOHandler::validateNetworkOperation() - ", operation_name, " failed: empty URL");
        return false;
    }
    
    // Check for timeout conditions
    if (m_network_timeout_enabled) {
        if (!handleNetworkTimeout(operation_name, m_default_network_timeout_seconds)) {
            // Timeout already set error code
            return false;
        }
    }
    
    // Check if we're hitting too many errors (circuit breaker pattern)
    static const size_t MAX_CONSECUTIVE_ERRORS = 10;
    if (m_network_retry_count >= static_cast<int>(MAX_CONSECUTIVE_ERRORS)) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - m_last_network_error_time).count();
        
        // Reset error count after 5 minutes of no operations
        if (elapsed >= 5) {
            Debug::log("http", "HTTPIOHandler::validateNetworkOperation() - Resetting error count after ", elapsed, " minutes");
            m_network_retry_count = 0;
        } else {
            m_error = ECONNABORTED;  // Connection aborted
            Debug::log("http", "HTTPIOHandler::validateNetworkOperation() - ", operation_name, " failed: too many consecutive errors (", m_network_retry_count, "), circuit breaker activated");
            return false;
        }
    }
    
    Debug::log("http", "HTTPIOHandler::validateNetworkOperation() - ", operation_name, " validation successful");
    return true;
}

void HTTPIOHandler::enforceBoundedCacheLimits() {
    // Get current memory usage statistics
    auto memory_stats = IOHandler::getMemoryStats();
    size_t total_usage = memory_stats["total_memory_usage"];
    size_t max_memory = memory_stats["max_total_memory"];
    
    if (max_memory == 0) {
        return; // No limits set
    }
    
    float usage_percent = static_cast<float>(total_usage) / static_cast<float>(max_memory) * 100.0f;
    
    // Calculate current buffer memory usage
    size_t current_buffer_memory = m_buffer.size() + m_read_ahead_buffer.size();
    
    Debug::log("memory", "HTTPIOHandler::enforceBoundedCacheLimits() - Memory usage: ", usage_percent, 
              "%, Buffer memory: ", current_buffer_memory, " bytes");
    
    // Enforce strict limits based on memory pressure
    if (usage_percent > 95.0f) {
        // Critical memory pressure - release all buffers except minimal main buffer
        Debug::log("memory", "HTTPIOHandler::enforceBoundedCacheLimits() - Critical memory pressure, releasing buffers");
        
        // Release read-ahead buffer immediately
        if (!m_read_ahead_buffer.empty()) {
            m_read_ahead_buffer = IOBufferPool::Buffer();
            m_read_ahead_active = false;
            m_read_ahead_position = -1;
            Debug::log("memory", "HTTPIOHandler::enforceBoundedCacheLimits() - Released read-ahead buffer");
        }
        
        // Reduce main buffer size if it's large
        if (m_buffer.size() > 32 * 1024) {
            // Force a smaller buffer on next fill
            m_buffer_size = 16 * 1024; // 16KB minimum
            m_max_buffer_size = 32 * 1024; // 32KB maximum
            Debug::log("memory", "HTTPIOHandler::enforceBoundedCacheLimits() - Reduced buffer sizes due to critical pressure");
        }
        
    } else if (usage_percent > 85.0f) {
        // High memory pressure - disable read-ahead and reduce buffer sizes
        Debug::log("memory", "HTTPIOHandler::enforceBoundedCacheLimits() - High memory pressure, optimizing buffers");
        
        // Disable read-ahead
        if (m_read_ahead_active) {
            m_read_ahead_buffer = IOBufferPool::Buffer();
            m_read_ahead_active = false;
            m_read_ahead_position = -1;
            Debug::log("memory", "HTTPIOHandler::enforceBoundedCacheLimits() - Disabled read-ahead due to high pressure");
        }
        
        // Reduce buffer sizes
        m_buffer_size = std::min(m_buffer_size, static_cast<size_t>(64 * 1024)); // 64KB max
        m_max_buffer_size = std::min(m_max_buffer_size, static_cast<size_t>(128 * 1024)); // 128KB max
        
    } else if (usage_percent > 75.0f) {
        // Moderate memory pressure - reduce read-ahead size
        Debug::log("memory", "HTTPIOHandler::enforceBoundedCacheLimits() - Moderate memory pressure, reducing read-ahead");
        
        // Reduce read-ahead size
        m_read_ahead_size = std::min(m_read_ahead_size, static_cast<size_t>(64 * 1024)); // 64KB max
        
        // Reduce buffer sizes moderately
        m_buffer_size = std::min(m_buffer_size, static_cast<size_t>(128 * 1024)); // 128KB max
    }
    
    // Enforce absolute maximum limits to prevent runaway memory usage
    const size_t ABSOLUTE_MAX_BUFFER_SIZE = 1024 * 1024; // 1MB absolute maximum
    const size_t ABSOLUTE_MAX_TOTAL_BUFFER_MEMORY = 2 * 1024 * 1024; // 2MB total maximum
    
    if (current_buffer_memory > ABSOLUTE_MAX_TOTAL_BUFFER_MEMORY) {
        Debug::log("memory", "HTTPIOHandler::enforceBoundedCacheLimits() - Absolute memory limit exceeded, emergency cleanup");
        
        // Emergency cleanup - release all buffers
        m_buffer = IOBufferPool::Buffer();
        m_read_ahead_buffer = IOBufferPool::Buffer();
        m_read_ahead_active = false;
        m_read_ahead_position = -1;
        
        // Reset to minimal sizes
        m_buffer_size = 16 * 1024; // 16KB
        m_max_buffer_size = 64 * 1024; // 64KB
        m_read_ahead_size = 32 * 1024; // 32KB
        
        // Update memory tracking
        updateMemoryUsage(0);
        
        Debug::log("memory", "HTTPIOHandler::enforceBoundedCacheLimits() - Emergency cleanup completed");
    }
    
    // Ensure buffer sizes don't exceed absolute limits
    m_buffer_size = std::min(m_buffer_size, ABSOLUTE_MAX_BUFFER_SIZE);
    m_max_buffer_size = std::min(m_max_buffer_size, ABSOLUTE_MAX_BUFFER_SIZE);
    m_read_ahead_size = std::min(m_read_ahead_size, ABSOLUTE_MAX_BUFFER_SIZE / 2);
}

void HTTPIOHandler::cleanupOnError(const std::string& context) {
    Debug::log("memory", "HTTPIOHandler::cleanupOnError() - Cleaning up resources due to error in: ", context);
    
    try {
        // Release all buffers to prevent memory leaks
        if (!m_buffer.empty()) {
            m_buffer = IOBufferPool::Buffer();
            Debug::log("memory", "HTTPIOHandler::cleanupOnError() - Released main buffer");
        }
        
        if (!m_read_ahead_buffer.empty()) {
            m_read_ahead_buffer = IOBufferPool::Buffer();
            m_read_ahead_active = false;
            m_read_ahead_position = -1;
            Debug::log("memory", "HTTPIOHandler::cleanupOnError() - Released read-ahead buffer");
        }
        
        // Update memory tracking to reflect cleanup
        updateMemoryUsage(0);
        
        // Reset state to prevent further issues
        m_buffer_offset = 0;
        m_buffer_start_position = 0;
        
        // Reset network error counters to prevent accumulation
        m_network_retry_count = 0;
        m_total_network_errors = 0;
        
        Debug::log("memory", "HTTPIOHandler::cleanupOnError() - Cleanup completed successfully");
        
    } catch (const std::exception& e) {
        Debug::log("memory", "HTTPIOHandler::cleanupOnError() - Exception during cleanup: ", e.what());
        // Continue cleanup even if some operations fail
        
        // Force reset of critical state
        m_buffer = IOBufferPool::Buffer();
        m_read_ahead_buffer = IOBufferPool::Buffer();
        m_read_ahead_active = false;
        updateMemoryUsage(0);
        
    } catch (...) {
        Debug::log("memory", "HTTPIOHandler::cleanupOnError() - Unknown exception during cleanup");
        // Force reset of critical state
        m_buffer = IOBufferPool::Buffer();
        m_read_ahead_buffer = IOBufferPool::Buffer();
        m_read_ahead_active = false;
        updateMemoryUsage(0);
    }
}