/*
 * HTTPIOHandler.cpp - HTTP-based IOHandler implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

HTTPIOHandler::HTTPIOHandler(const std::string& url) 
    : m_url(url) {
    m_buffer.reserve(BUFFER_SIZE);
}

HTTPIOHandler::HTTPIOHandler(const std::string& url, long content_length)
    : m_url(url), m_content_length(content_length) {
    m_buffer.reserve(BUFFER_SIZE);
}

HTTPIOHandler::~HTTPIOHandler() {
    close();
}

void HTTPIOHandler::initialize() {
    if (m_initialized) return;
    
    // Perform HEAD request to get metadata
    auto response = HTTPClient::head(m_url);
    
    if (!response.success) {
        throw std::runtime_error("Failed to connect to HTTP server: " + response.statusMessage);
    }
    
    // Extract content length
    auto it = response.headers.find("content-length");
    if (it != response.headers.end()) {
        try {
            m_content_length = std::stol(it->second);
        } catch (const std::exception&) {
            m_content_length = -1;
        }
    }
    
    // Extract MIME type
    it = response.headers.find("content-type");
    if (it != response.headers.end()) {
        m_mime_type = it->second;
        // Remove charset if present (e.g., "audio/mpeg; charset=utf-8" -> "audio/mpeg")
        size_t semicolon = m_mime_type.find(';');
        if (semicolon != std::string::npos) {
            m_mime_type = m_mime_type.substr(0, semicolon);
        }
    }
    
    // Check for range support
    it = response.headers.find("accept-ranges");
    if (it != response.headers.end()) {
        m_supports_ranges = (it->second.find("bytes") != std::string::npos);
    }
    
    m_initialized = true;
}

size_t HTTPIOHandler::read(void* buffer, size_t size, size_t count) {
    if (!m_initialized) {
        initialize();
    }
    
    if (m_eof || size == 0 || count == 0) {
        return 0;
    }
    
    size_t bytes_to_read = size * count;
    size_t total_read = 0;
    uint8_t* output = static_cast<uint8_t*>(buffer);
    
    while (bytes_to_read > 0 && !m_eof) {
        // Check if we need to fill the buffer
        if (!isPositionBuffered(m_current_position)) {
            if (!fillBuffer(m_current_position, bytes_to_read)) {
                break; // Error or EOF
            }
        }
        
        // Read from buffer
        size_t chunk_read = readFromBuffer(output + total_read, bytes_to_read);
        if (chunk_read == 0) {
            break; // EOF or error
        }
        
        total_read += chunk_read;
        bytes_to_read -= chunk_read;
        m_current_position += chunk_read;
        
        // Check for EOF
        if (m_content_length > 0 && m_current_position >= m_content_length) {
            m_eof = true;
            break;
        }
    }
    
    return total_read / size; // Return number of complete items read
}

int HTTPIOHandler::seek(long offset, int whence) {
    if (!m_initialized) {
        initialize();
    }
    
    long new_position;
    
    switch (whence) {
    case SEEK_SET:
        new_position = offset;
        break;
    case SEEK_CUR:
        new_position = m_current_position + offset;
        break;
    case SEEK_END:
        if (m_content_length < 0) {
            return -1; // Cannot seek to end without known content length
        }
        new_position = m_content_length + offset;
        break;
    default:
        return -1;
    }
    
    // Validate position
    if (new_position < 0) {
        return -1;
    }
    
    if (m_content_length > 0 && new_position > m_content_length) {
        return -1;
    }
    
    m_current_position = new_position;
    m_eof = false;
    
    return 0;
}

long HTTPIOHandler::tell() {
    return m_current_position;
}

int HTTPIOHandler::close() {
    m_buffer.clear();
    m_buffer_offset = 0;
    m_buffer_start_position = 0;
    return 0;
}

bool HTTPIOHandler::eof() {
    return m_eof;
}

bool HTTPIOHandler::fillBuffer(long position, size_t min_size) {
    // Determine range size - use larger of min_size and BUFFER_SIZE
    size_t range_size = std::max(min_size, BUFFER_SIZE);
    
    // If we have content length, don't read past it
    if (m_content_length > 0) {
        range_size = std::min(range_size, static_cast<size_t>(m_content_length - position));
        if (range_size == 0) {
            m_eof = true;
            return false;
        }
    }
    
    HTTPClient::Response response;
    
    if (m_supports_ranges || position > 0) {
        // Use range request
        long end_byte = position + range_size - 1;
        if (m_content_length > 0) {
            end_byte = std::min(end_byte, m_content_length - 1);
        }
        response = HTTPClient::getRange(m_url, position, end_byte);
    } else {
        // Use regular GET request (for servers that don't support ranges)
        response = HTTPClient::get(m_url);
    }
    
    if (!response.success) {
        return false;
    }
    
    // Update buffer
    m_buffer.assign(response.body.begin(), response.body.end());
    m_buffer_offset = 0;
    m_buffer_start_position = position;
    
    // If we got less data than expected, we might have hit EOF
    if (response.body.size() < range_size && m_content_length < 0) {
        m_eof = true;
    }
    
    return !m_buffer.empty();
}

size_t HTTPIOHandler::readFromBuffer(void* buffer, size_t bytes_to_read) {
    if (m_buffer_offset >= m_buffer.size()) {
        return 0;
    }
    
    size_t available = m_buffer.size() - m_buffer_offset;
    size_t to_copy = std::min(bytes_to_read, available);
    
    std::memcpy(buffer, m_buffer.data() + m_buffer_offset, to_copy);
    m_buffer_offset += to_copy;
    
    return to_copy;
}

bool HTTPIOHandler::isPositionBuffered(long position) const {
    if (m_buffer.empty()) {
        return false;
    }
    
    long buffer_end = m_buffer_start_position + static_cast<long>(m_buffer.size());
    return position >= m_buffer_start_position && position < buffer_end;
}