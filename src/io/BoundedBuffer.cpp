/*
 * BoundedBuffer.cpp - Bounded buffer implementation for memory-safe I/O operations
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace IO {

// BoundedBuffer implementation

BoundedBuffer::BoundedBuffer(size_t max_size, size_t initial_size) 
    : m_max_size(max_size) {
    Debug::log("memory", "BoundedBuffer::BoundedBuffer() - Creating bounded buffer with max_size=", max_size, ", initial_size=", initial_size);
    
    // Register with memory tracker
    MemoryTracker::getInstance().update();
    
    if (initial_size > 0) {
        if (!resize(initial_size)) {
            Debug::log("memory", "BoundedBuffer::BoundedBuffer() - Warning: Could not allocate initial size ", initial_size);
        }
    }
}

BoundedBuffer::~BoundedBuffer() {
    Debug::log("memory", "BoundedBuffer::~BoundedBuffer() - Destroying bounded buffer, peak usage: ", m_peak_usage);
    
    if (m_data) {
        // Return buffer to memory pool instead of direct delete
        MemoryPoolManager::getInstance().releaseBuffer(m_data, m_capacity, COMPONENT_NAME);
        m_data = nullptr;
    }
    
    m_total_deallocations++;
    updateMemoryTracking();
}

bool BoundedBuffer::resize(size_t new_size) {
    if (new_size > m_max_size) {
        Debug::log("memory", "BoundedBuffer::resize() - Requested size ", new_size, " exceeds max_size ", m_max_size);
        return false;
    }
    
    if (new_size <= m_capacity) {
        // Just update size, no reallocation needed
        m_size = new_size;
        return true;
    }
    
    // Need to reallocate
    return reallocate(new_size);
}

bool BoundedBuffer::reserve(size_t capacity) {
    if (capacity > m_max_size) {
        Debug::log("memory", "BoundedBuffer::reserve() - Requested capacity ", capacity, " exceeds max_size ", m_max_size);
        return false;
    }
    
    if (capacity <= m_capacity) {
        return true; // Already have enough capacity
    }
    
    return reallocate(capacity);
}

void BoundedBuffer::shrink_to_fit() {
    if (m_size < m_capacity) {
        reallocate(m_size);
    }
}

bool BoundedBuffer::append(const void* data, size_t size) {
    if (!data || size == 0) {
        return true; // Nothing to append
    }
    
    size_t new_size = m_size + size;
    if (new_size > m_max_size) {
        Debug::log("memory", "BoundedBuffer::append() - Appending ", size, " bytes would exceed max_size");
        return false;
    }
    
    // Ensure we have enough capacity
    if (new_size > m_capacity) {
        // Grow capacity by 50% or to new_size, whichever is larger
        size_t new_capacity = std::max(new_size, m_capacity + m_capacity / 2);
        new_capacity = std::min(new_capacity, m_max_size);
        
        if (!reallocate(new_capacity)) {
            return false;
        }
    }
    
    // Copy data
    std::memcpy(m_data + m_size, data, size);
    m_size = new_size;
    
    updateMemoryTracking();
    return true;
}

bool BoundedBuffer::set(const void* data, size_t size) {
    if (size > m_max_size) {
        Debug::log("memory", "BoundedBuffer::set() - Requested size ", size, " exceeds max_size ", m_max_size);
        return false;
    }
    
    if (size > m_capacity) {
        if (!reallocate(size)) {
            return false;
        }
    }
    
    if (data && size > 0) {
        std::memcpy(m_data, data, size);
    }
    
    m_size = size;
    updateMemoryTracking();
    return true;
}

size_t BoundedBuffer::copy_to(void* dest, size_t offset, size_t size) const {
    if (!dest || !m_data || offset >= m_size) {
        return 0;
    }
    
    size_t available = m_size - offset;
    size_t to_copy = std::min(size, available);
    
    std::memcpy(dest, m_data + offset, to_copy);
    return to_copy;
}

std::map<std::string, size_t> BoundedBuffer::getStats() const {
    std::map<std::string, size_t> stats;
    stats["current_size"] = m_size;
    stats["current_capacity"] = m_capacity;
    stats["max_size"] = m_max_size;
    stats["peak_usage"] = m_peak_usage;
    stats["total_allocations"] = m_total_allocations;
    stats["total_deallocations"] = m_total_deallocations;
    
    if (m_max_size > 0) {
        stats["usage_percent"] = (m_size * 100) / m_max_size;
        stats["capacity_percent"] = (m_capacity * 100) / m_max_size;
    }
    
    return stats;
}

bool BoundedBuffer::reallocate(size_t new_capacity) {
    if (new_capacity > m_max_size) {
        return false;
    }
    
    uint8_t* new_data = nullptr;
    
    if (new_capacity > 0) {
        // Use memory pool manager for allocation
        new_data = MemoryPoolManager::getInstance().allocateBuffer(
            new_capacity, COMPONENT_NAME);
        
        if (!new_data) {
            Debug::log("memory", "BoundedBuffer::reallocate() - Allocation failed for ", new_capacity, " bytes");
            return false;
        }
        
        // Copy existing data
        if (m_data && m_size > 0) {
            size_t copy_size = std::min(m_size, new_capacity);
            std::memcpy(new_data, m_data, copy_size);
            m_size = copy_size;
        }
    }
    
    // Free old data
    if (m_data) {
        // Return buffer to memory pool instead of direct delete
        MemoryPoolManager::getInstance().releaseBuffer(m_data, m_capacity, COMPONENT_NAME);
        m_total_deallocations++;
    }
    
    m_data = new_data;
    m_capacity = new_capacity;
    m_total_allocations++;
    
    updateMemoryTracking();
    
    Debug::log("memory", "BoundedBuffer::reallocate() - Reallocated to ", new_capacity, " bytes");
    return true;
}

void BoundedBuffer::updateMemoryTracking() {
    m_peak_usage = std::max(m_peak_usage, m_capacity);
}

// BoundedCircularBuffer implementation

BoundedCircularBuffer::BoundedCircularBuffer(size_t max_size) 
    : m_capacity(max_size) {
    Debug::log("memory", "BoundedCircularBuffer::BoundedCircularBuffer() - Creating circular buffer with capacity=", max_size);
    
    if (max_size > 0) {
        // Use memory pool manager for allocation
        m_buffer = MemoryPoolManager::getInstance().allocateBuffer(
            max_size, COMPONENT_NAME);
        
        if (!m_buffer) {
            Debug::log("memory", "BoundedCircularBuffer::BoundedCircularBuffer() - Allocation failed");
            m_capacity = 0;
        }
    }
}

BoundedCircularBuffer::~BoundedCircularBuffer() {
    Debug::log("memory", "BoundedCircularBuffer::~BoundedCircularBuffer() - Destroying circular buffer, peak usage: ", m_peak_usage);
    
    if (m_buffer) {
        // Return buffer to memory pool instead of direct delete
        MemoryPoolManager::getInstance().releaseBuffer(m_buffer, m_capacity, COMPONENT_NAME);
        m_buffer = nullptr;
    }
}

size_t BoundedCircularBuffer::write(const void* data, size_t size) {
    if (!data || size == 0 || !m_buffer) {
        return 0;
    }
    
    size_t available_space = space();
    size_t to_write = std::min(size, available_space);
    
    if (to_write == 0) {
        return 0; // Buffer is full
    }
    
    const uint8_t* src = static_cast<const uint8_t*>(data);
    size_t written = 0;
    
    while (written < to_write) {
        size_t chunk_size = std::min(to_write - written, m_capacity - m_write_pos);
        std::memcpy(m_buffer + m_write_pos, src + written, chunk_size);
        
        written += chunk_size;
        m_write_pos = (m_write_pos + chunk_size) % m_capacity;
    }
    
    m_count += to_write;
    m_total_bytes_written += to_write;
    m_peak_usage = std::max(m_peak_usage, m_count);
    
    return to_write;
}

size_t BoundedCircularBuffer::read(void* data, size_t size) {
    if (!data || size == 0 || !m_buffer) {
        return 0;
    }
    
    size_t available_data = available();
    size_t to_read = std::min(size, available_data);
    
    if (to_read == 0) {
        return 0; // Buffer is empty
    }
    
    uint8_t* dest = static_cast<uint8_t*>(data);
    size_t read_bytes = 0;
    
    while (read_bytes < to_read) {
        size_t chunk_size = std::min(to_read - read_bytes, m_capacity - m_read_pos);
        std::memcpy(dest + read_bytes, m_buffer + m_read_pos, chunk_size);
        
        read_bytes += chunk_size;
        m_read_pos = (m_read_pos + chunk_size) % m_capacity;
    }
    
    m_count -= to_read;
    m_total_bytes_read += to_read;
    
    return to_read;
}

size_t BoundedCircularBuffer::peek(void* data, size_t size) const {
    if (!data || size == 0 || !m_buffer) {
        return 0;
    }
    
    size_t available_data = available();
    size_t to_peek = std::min(size, available_data);
    
    if (to_peek == 0) {
        return 0; // Buffer is empty
    }
    
    uint8_t* dest = static_cast<uint8_t*>(data);
    size_t peeked = 0;
    size_t peek_pos = m_read_pos;
    
    while (peeked < to_peek) {
        size_t chunk_size = std::min(to_peek - peeked, m_capacity - peek_pos);
        std::memcpy(dest + peeked, m_buffer + peek_pos, chunk_size);
        
        peeked += chunk_size;
        peek_pos = (peek_pos + chunk_size) % m_capacity;
    }
    
    return to_peek;
}

size_t BoundedCircularBuffer::skip(size_t size) {
    size_t available_data = available();
    size_t to_skip = std::min(size, available_data);
    
    m_read_pos = (m_read_pos + to_skip) % m_capacity;
    m_count -= to_skip;
    
    return to_skip;
}

size_t BoundedCircularBuffer::available() const {
    return m_count;
}

size_t BoundedCircularBuffer::space() const {
    return m_capacity - m_count;
}

void BoundedCircularBuffer::clear() {
    m_read_pos = 0;
    m_write_pos = 0;
    m_count = 0;
}

std::map<std::string, size_t> BoundedCircularBuffer::getStats() const {
    std::map<std::string, size_t> stats;
    stats["capacity"] = m_capacity;
    stats["available"] = available();
    stats["space"] = space();
    stats["peak_usage"] = m_peak_usage;
    stats["total_bytes_written"] = m_total_bytes_written;
    stats["total_bytes_read"] = m_total_bytes_read;
    
    if (m_capacity > 0) {
        stats["usage_percent"] = (available() * 100) / m_capacity;
    }
    
    return stats;
}

} // namespace IO
} // namespace PsyMP3
