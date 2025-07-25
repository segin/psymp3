/*
 * Demuxer.cpp - Generic container demuxer base class implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

Demuxer::Demuxer(std::unique_ptr<IOHandler> handler) 
    : m_handler(std::move(handler)) {
    // Validate handler
    if (!m_handler) {
        reportError("Initialization", "Null IOHandler provided to demuxer constructor");
        throw std::invalid_argument("Null IOHandler provided to demuxer");
    }
    
    // Test handler functionality
    long pos = m_handler->tell();
    if (pos < 0) {
        reportError("Initialization", "IOHandler tell() failed during initialization");
        throw std::runtime_error("IOHandler is not functional");
    }
}

// BufferPool implementation
BufferPool& BufferPool::getInstance() {
    static BufferPool instance;
    return instance;
}

std::vector<uint8_t> BufferPool::getBuffer(size_t min_size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Look for a suitable buffer in the pool
    for (auto it = m_buffers.begin(); it != m_buffers.end(); ++it) {
        if (it->capacity() >= min_size) {
            std::vector<uint8_t> buffer = std::move(*it);
            m_buffers.erase(it);
            buffer.clear(); // Clear contents but keep capacity
            return buffer;
        }
    }
    
    // No suitable buffer found, create a new one
    try {
        std::vector<uint8_t> buffer;
        buffer.reserve(std::max(min_size, static_cast<size_t>(4096))); // Minimum 4KB
        return buffer;
    } catch (const std::bad_alloc& e) {
        // Clear pool and try again with exact size
        m_buffers.clear();
        try {
            std::vector<uint8_t> buffer;
            buffer.reserve(min_size);
            return buffer;
        } catch (const std::bad_alloc& e2) {
            // Return empty buffer - caller should handle this
            return std::vector<uint8_t>();
        }
    }
}

void BufferPool::returnBuffer(std::vector<uint8_t>&& buffer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Only pool buffers that are reasonably sized and not too large
    if (buffer.capacity() >= 1024 && buffer.capacity() <= MAX_BUFFER_SIZE && 
        m_buffers.size() < MAX_POOLED_BUFFERS) {
        buffer.clear(); // Clear contents but keep capacity
        m_buffers.push_back(std::move(buffer));
    }
    // Otherwise, let the buffer be destroyed
}

void BufferPool::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_buffers.clear();
}

BufferPool::PoolStats BufferPool::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    PoolStats stats;
    stats.total_buffers = m_buffers.size();
    stats.total_memory_bytes = 0;
    stats.largest_buffer_size = 0;
    
    for (const auto& buffer : m_buffers) {
        stats.total_memory_bytes += buffer.capacity();
        stats.largest_buffer_size = std::max(stats.largest_buffer_size, buffer.capacity());
    }
    
    return stats;
}

// DemuxerFactory implementation moved to DemuxerFactory.cpp