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
    
    // Initialize the HTTP stream
    initialize();
}

HTTPIOHandler::HTTPIOHandler(const std::string& url, long content_length)
    : m_url(url), m_content_length(content_length) {
    Debug::log("HTTPIOHandler", "Creating HTTP handler for URL: ", url, " (content length: ", content_length, ")");
    
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
            Debug::log("HTTPIOHandler", "HEAD request failed: ", response.statusMessage, " (status: ", response.statusCode, ")");
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
    
    Debug::log("HTTPIOHandler", "Reading ", bytes_requested, " bytes at position ", static_cast<long long>(m_current_position));
    
    // Check if we have enough data in buffer
    if (!isPositionBuffered(m_current_position) || 
        !isPositionBuffered(m_current_position + bytes_requested - 1)) {
        
        // Need to fill buffer
        if (!fillBuffer(m_current_position, bytes_requested)) {
            Debug::log("HTTPIOHandler", "Failed to fill buffer for read operation");
            m_error = -1;
            return 0;
        }
    }
    
    // Read from buffer
    size_t bytes_read = readFromBuffer(buffer, bytes_requested);
    m_current_position += bytes_read;
    
    // Update position tracking
    m_position = m_current_position;
    
    // Check for EOF condition
    if (m_content_length > 0 && m_current_position >= m_content_length) {
        m_eof = true;
    }
    
    Debug::log("HTTPIOHandler", "Read ", bytes_read, " bytes, new position: ", static_cast<long long>(m_current_position));
    
    return bytes_read / size;  // Return number of complete elements read
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
    
    Debug::log("HTTPIOHandler", "Closing HTTP handler for: ", m_url);
    
    // Clear buffer
    m_buffer.clear();
    m_buffer.shrink_to_fit();
    
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
    Debug::log("HTTPIOHandler", "Filling buffer at position ", static_cast<long long>(position), " (min size: ", min_size, ")");
    
    // Calculate range size - use larger of min_size and BUFFER_SIZE
    size_t range_size = std::max(min_size, BUFFER_SIZE);
    
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
    
    // Store response data in buffer
    m_buffer.assign(reinterpret_cast<const uint8_t*>(response.body.data()),
                    reinterpret_cast<const uint8_t*>(response.body.data() + response.body.size()));
    m_buffer_offset = 0;
    m_buffer_start_position = position;
    
    Debug::log("HTTPIOHandler", "Buffer filled: ", m_buffer.size(), " bytes starting at position ", static_cast<long long>(m_buffer_start_position));
    
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