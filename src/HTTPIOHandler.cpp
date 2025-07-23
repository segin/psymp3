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
    
    if (m_initialized) {
        Debug::log("HTTPIOHandler", "Already initialized, skipping");
        return;
    }
    
    try {
        // Perform HEAD request to get metadata
        HTTPClient::Response response = HTTPClient::head(m_url);
        
        if (!response.success) {
            std::string errorMsg = getErrorMessage(-1, "HEAD request failed: " + response.statusMessage + " (status: " + std::to_string(response.statusCode) + ")");
            Debug::log("HTTPIOHandler", errorMsg);
            m_error = -1;
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
        
        m_initialized = true;
        m_error = 0;
        Debug::log("HTTPIOHandler", "HTTP stream initialization completed successfully");
        
    } catch (const std::exception& e) {
        Debug::log("HTTPIOHandler", "Exception during initialization: ", e.what());
        m_error = -1;
        m_initialized = false;
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
    if (!m_initialized) {
        Debug::log("HTTPIOHandler", "Attempted read on uninitialized handler");
        return 0;
    }
    
    if (m_closed) {
        Debug::log("HTTPIOHandler", "Attempted read on closed handler");
        return 0;
    }
    
    if (m_eof) {
        Debug::log("HTTPIOHandler", "Attempted read at EOF");
        return 0;
    }
    
    size_t bytes_requested = size * count;
    if (bytes_requested == 0) {
        return 0;
    }
    
    // Update access pattern tracking
    updateAccessPattern(m_current_position);
    
    Debug::log("HTTPIOHandler", "Reading ", bytes_requested, " bytes at position ", static_cast<long long>(m_current_position),
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
        off_t read_position = m_current_position + total_bytes_read;
        
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
                m_error = -1;
                break;
            }
            
            // Read from newly filled buffer
            size_t buffer_bytes_read = readFromBuffer(dest_buffer + total_bytes_read, remaining_bytes);
            total_bytes_read += buffer_bytes_read;
            
            if (buffer_bytes_read == 0) {
                m_eof = true;
                break;
            }
        }
    }
    
    // Update position
    m_current_position += total_bytes_read;
    m_position = m_current_position;
    
    // Check for EOF condition
    if (m_content_length > 0 && m_current_position >= m_content_length) {
        m_eof = true;
    }
    
    // Perform read-ahead for sequential access
    if (m_sequential_access && total_bytes_read > 0) {
        performReadAhead(m_current_position);
    }
    
    Debug::log("HTTPIOHandler", "Read ", total_bytes_read, " bytes, new position: ", static_cast<long long>(m_current_position));
    
    return total_bytes_read / size;  // Return number of complete elements read
}

int HTTPIOHandler::seek(off_t offset, int whence) {
    if (!m_initialized) {
        Debug::log("HTTPIOHandler", "Attempted seek on uninitialized handler");
        return -1;
    }
    
    if (m_closed) {
        Debug::log("HTTPIOHandler", "Attempted seek on closed handler");
        return -1;
    }
    
    off_t new_position;
    
    switch (whence) {
        case SEEK_SET:
            new_position = offset;
            break;
        case SEEK_CUR:
            new_position = m_current_position + offset;
            break;
        case SEEK_END:
            if (m_content_length <= 0) {
                Debug::log("HTTPIOHandler", "SEEK_END not supported without known content length");
                return -1;
            }
            new_position = m_content_length + offset;
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
    
    if (m_content_length > 0 && new_position > m_content_length) {
        Debug::log("HTTPIOHandler", "Seek beyond end of content: ", static_cast<long long>(new_position), " > ", m_content_length);
        return -1;
    }
    
    // Check if server supports range requests for seeking
    if (new_position != m_current_position && !m_supports_ranges) {
        Debug::log("HTTPIOHandler", "Seek requested but server doesn't support range requests");
        return -1;
    }
    
    Debug::log("HTTPIOHandler", "Seeking from ", static_cast<long long>(m_current_position), " to ", static_cast<long long>(new_position));
    
    m_current_position = new_position;
    m_position = m_current_position;
    
    // Clear EOF flag if we're not at the end
    if (m_content_length <= 0 || m_current_position < m_content_length) {
        m_eof = false;
    }
    
    return 0;
}

off_t HTTPIOHandler::tell() {
    return m_current_position;
}

int HTTPIOHandler::close() {
    if (m_closed) {
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
    
    m_closed = true;
    return 0;
}

bool HTTPIOHandler::eof() {
    return m_eof;
}

off_t HTTPIOHandler::getFileSize() {
    return m_content_length;
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
    
    HTTPClient::Response response;
    
    // Use range request if supported or if we're not at the beginning
    if (m_supports_ranges || position > 0) {
        long end_byte = position + range_size - 1;
        Debug::log("HTTPIOHandler", "Making range request: bytes=", static_cast<long long>(position), "-", end_byte);
        
        response = HTTPClient::getRange(m_url, position, end_byte);
        
        // Accept both 206 (Partial Content) and 200 (OK) responses
        if (response.success && (response.statusCode == 206 || response.statusCode == 200)) {
            Debug::log("HTTPIOHandler", "Range request successful (status: ", response.statusCode, ", body size: ", response.body.size(), ")");
        } else {
            Debug::log("HTTPIOHandler", "Range request failed (status: ", response.statusCode, "): ", response.statusMessage);
            return false;
        }
    } else {
        // Fall back to regular GET request
        Debug::log("HTTPIOHandler", "Making regular GET request");
        response = HTTPClient::get(m_url);
        
        if (!response.success) {
            Debug::log("HTTPIOHandler", "GET request failed (status: ", response.statusCode, "): ", response.statusMessage);
            return false;
        }
        
        Debug::log("HTTPIOHandler", "GET request successful (status: ", response.statusCode, ", body size: ", response.body.size(), ")");
    }
    
    // Check memory limits before allocating
    if (!checkMemoryLimits(response.body.size())) {
        Debug::log("HTTPIOHandler", "Memory limits would be exceeded for ", response.body.size(), " bytes");
        m_error = ENOMEM;
        return false;
    }
    
    // Get buffer from pool and store response data
    m_buffer = IOBufferPool::getInstance().acquire(response.body.size());
    if (m_buffer.empty()) {
        Debug::log("HTTPIOHandler", "Failed to acquire buffer from pool for ", response.body.size(), " bytes");
        return false;
    }
    
    std::memcpy(m_buffer.data(), response.body.data(), response.body.size());
    m_buffer_offset = 0;
    m_buffer_start_position = position;
    
    // Update memory usage tracking
    updateMemoryUsage(m_buffer.size());
    
    // Optimize buffer pool usage based on access patterns
    optimizeBufferPoolUsage();
    
    // Periodic global memory optimization and buffer pool optimization
    static size_t global_optimization_counter = 0;
    global_optimization_counter++;
    
    if (global_optimization_counter % 50 == 0) { // Every 50 buffer fills
        optimizeBufferPoolUsage();
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
    
    // Perform read-ahead request
    HTTPClient::Response response;
    if (m_supports_ranges) {
        long end_byte = read_ahead_start + read_size - 1;
        response = HTTPClient::getRange(m_url, read_ahead_start, end_byte);
    } else {
        return false; // Can't do read-ahead without range support
    }
    
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
            size_t increased_size = std::min(recommended_pool_size * 1.5, static_cast<size_t>(32 * 1024 * 1024)); // Cap at 32MB
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