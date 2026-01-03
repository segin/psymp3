/*
 * EnhancedBufferPool.cpp - Enhanced buffer pool for memory optimization
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace IO {

EnhancedBufferPool& EnhancedBufferPool::getInstance() {
    static EnhancedBufferPool instance;
    return instance;
}

EnhancedBufferPool::EnhancedBufferPool() 
    : m_memory_pressure(0)
    , m_buffer_hits(0)
    , m_buffer_misses(0)
    , m_buffer_reuse_count(0)
    , m_last_cleanup(std::chrono::steady_clock::now()) {
    // Register for memory pressure updates
    MemoryTracker::getInstance().registerMemoryPressureCallback(
        [this](int pressure) { this->setMemoryPressure(pressure); }
    );
}

EnhancedBufferPool::~EnhancedBufferPool() {
    clear();
}

std::vector<uint8_t> EnhancedBufferPool::getBuffer(size_t min_size, size_t preferred_size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Perform periodic cleanup if needed
    performPeriodicCleanup();
    
    // Use preferred size if specified, otherwise use min_size
    size_t target_size = preferred_size > min_size ? preferred_size : min_size;
    
    // Update usage statistics
    auto& usage = m_size_usage_stats[min_size];
    usage.request_count++;
    usage.last_request = std::chrono::steady_clock::now();
    
    // Don't pool very large buffers to avoid memory waste
    if (min_size > getMaxBufferSize()) {
        m_buffer_misses++;
        std::vector<uint8_t> buffer;
        buffer.reserve(min_size);
        return buffer;
    }
    
    // Select appropriate buffer category
    std::vector<std::vector<uint8_t>>* category = &m_medium_buffers;
    if (min_size < SMALL_BUFFER_THRESHOLD) {
        category = &m_small_buffers;
    } else if (min_size > MEDIUM_BUFFER_THRESHOLD) {
        category = &m_large_buffers;
    }
    
    // Find a suitable buffer from the pool
    for (auto it = category->begin(); it != category->end(); ++it) {
        if (it->capacity() >= min_size) {
            std::vector<uint8_t> buffer = std::move(*it);
            category->erase(it);
            buffer.clear(); // Clear contents but keep capacity
            m_buffer_hits++;
            m_buffer_reuse_count++;
            return buffer;
        }
    }
    
    // No suitable buffer found in the right category, try other categories
    if (category != &m_large_buffers) {
        for (auto it = m_large_buffers.begin(); it != m_large_buffers.end(); ++it) {
            if (it->capacity() >= min_size) {
                std::vector<uint8_t> buffer = std::move(*it);
                m_large_buffers.erase(it);
                buffer.clear();
                m_buffer_hits++;
                m_buffer_reuse_count++;
                return buffer;
            }
        }
    }
    
    if (category != &m_medium_buffers && min_size >= SMALL_BUFFER_THRESHOLD) {
        for (auto it = m_medium_buffers.begin(); it != m_medium_buffers.end(); ++it) {
            if (it->capacity() >= min_size) {
                std::vector<uint8_t> buffer = std::move(*it);
                m_medium_buffers.erase(it);
                buffer.clear();
                m_buffer_hits++;
                m_buffer_reuse_count++;
                return buffer;
            }
        }
    }
    
    // No suitable buffer found, create new one
    m_buffer_misses++;
    std::vector<uint8_t> buffer;
    
    // Use optimal buffer sizes based on common use cases
    if (target_size <= 4096) {
        buffer.reserve(4096); // 4KB - common for small chunks
    } else if (target_size <= 16384) {
        buffer.reserve(16384); // 16KB - common for medium chunks
    } else if (target_size <= 65536) {
        buffer.reserve(65536); // 64KB - common for large chunks
    } else {
        // Round up to nearest 64KB for very large buffers
        size_t rounded_size = ((target_size + 65535) / 65536) * 65536;
        buffer.reserve(rounded_size);
    }
    
    return buffer;
}

void EnhancedBufferPool::returnBuffer(std::vector<uint8_t>&& buffer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Only pool buffers that are reasonably sized and not too large
    if (!shouldPoolBuffer(buffer.capacity())) {
        return; // Let the buffer be destroyed naturally
    }
    
    // Determine which category to return to
    std::vector<std::vector<uint8_t>>* category = &m_medium_buffers;
    size_t capacity = buffer.capacity();
    
    if (capacity < SMALL_BUFFER_THRESHOLD) {
        category = &m_small_buffers;
    } else if (capacity > MEDIUM_BUFFER_THRESHOLD) {
        category = &m_large_buffers;
    }
    
    // Check if we've reached the limit for this category
    size_t max_buffers = getMaxPooledBuffers();
    size_t category_max = max_buffers / 3; // Distribute evenly among categories
    
    if (category->size() < category_max) {
        buffer.clear(); // Clear contents but keep capacity
        category->push_back(std::move(buffer));
    }
    // Otherwise, let the buffer be destroyed naturally
}

void EnhancedBufferPool::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_small_buffers.clear();
    m_medium_buffers.clear();
    m_large_buffers.clear();
}

void EnhancedBufferPool::setMemoryPressure(int pressure_level) {
    m_memory_pressure = std::max(0, std::min(100, pressure_level));
    
    // If memory pressure is high, proactively reduce pool size
    if (m_memory_pressure > 70) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Keep only half of the buffers in each category
        if (!m_small_buffers.empty()) {
            m_small_buffers.resize(m_small_buffers.size() / 2);
        }
        
        if (!m_medium_buffers.empty()) {
            m_medium_buffers.resize(m_medium_buffers.size() / 2);
        }
        
        if (!m_large_buffers.empty()) {
            m_large_buffers.resize(m_large_buffers.size() / 2);
        }
    }
}

int EnhancedBufferPool::getMemoryPressure() const {
    return m_memory_pressure;
}

EnhancedBufferPool::PoolStats EnhancedBufferPool::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    PoolStats stats{};
    stats.total_buffers = m_small_buffers.size() + m_medium_buffers.size() + m_large_buffers.size();
    stats.largest_buffer_size = 0;
    stats.smallest_buffer_size = SIZE_MAX;
    stats.total_memory_bytes = 0;
    stats.buffer_hits = m_buffer_hits;
    stats.buffer_misses = m_buffer_misses;
    stats.memory_pressure = m_memory_pressure;
    stats.reuse_count = m_buffer_reuse_count;
    
    auto process_category = [&stats](const std::vector<std::vector<uint8_t>>& category) {
        for (const auto& buffer : category) {
            size_t capacity = buffer.capacity();
            stats.total_memory_bytes += capacity;
            stats.largest_buffer_size = std::max(stats.largest_buffer_size, capacity);
            stats.smallest_buffer_size = std::min(stats.smallest_buffer_size, capacity);
        }
    };
    
    process_category(m_small_buffers);
    process_category(m_medium_buffers);
    process_category(m_large_buffers);
    
    if (stats.total_buffers > 0) {
        stats.average_buffer_size = stats.total_memory_bytes / stats.total_buffers;
    }
    
    if (stats.smallest_buffer_size == SIZE_MAX) {
        stats.smallest_buffer_size = 0;
    }
    
    // Calculate hit ratio
    size_t total_requests = stats.buffer_hits + stats.buffer_misses;
    stats.hit_ratio = total_requests > 0 ? static_cast<float>(stats.buffer_hits) / total_requests : 0.0f;
    
    return stats;
}

size_t EnhancedBufferPool::getMaxPooledBuffers() const {
    // Scale max buffers based on memory pressure
    // At 0% pressure: 32 buffers
    // At 100% pressure: 8 buffers
    return DEFAULT_MAX_POOLED_BUFFERS - ((DEFAULT_MAX_POOLED_BUFFERS - 8) * m_memory_pressure) / 100;
}

size_t EnhancedBufferPool::getMaxBufferSize() const {
    // Scale max buffer size based on memory pressure
    // At 0% pressure: 1MB
    // At 100% pressure: 256KB
    return DEFAULT_MAX_BUFFER_SIZE - ((DEFAULT_MAX_BUFFER_SIZE - 256 * 1024) * m_memory_pressure) / 100;
}

bool EnhancedBufferPool::shouldPoolBuffer(size_t capacity) const {
    // Don't pool tiny buffers
    if (capacity < 1024) {
        return false;
    }
    
    // Don't pool buffers larger than the current max
    if (capacity > getMaxBufferSize()) {
        return false;
    }
    
    // Under high memory pressure, be more selective
    if (m_memory_pressure > 70 && capacity > MEDIUM_BUFFER_THRESHOLD) {
        return false;
    }
    
    return true;
}

void EnhancedBufferPool::performPeriodicCleanup() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - m_last_cleanup) < CLEANUP_INTERVAL) {
        return; // Not time for cleanup yet
    }
    
    m_last_cleanup = now;
    
    // Clean up unused buffer sizes
    auto it = m_size_usage_stats.begin();
    while (it != m_size_usage_stats.end()) {
        // Remove stats for buffer sizes that haven't been used in a while
        if (std::chrono::duration_cast<std::chrono::minutes>(now - it->second.last_request).count() > 10) {
            it = m_size_usage_stats.erase(it);
        } else {
            ++it;
        }
    }
    
    // If memory pressure is moderate or higher, be more aggressive with cleanup
    if (m_memory_pressure >= 50) {
        // Remove least recently used buffers from each category
        if (m_small_buffers.size() > 2) {
            m_small_buffers.resize(m_small_buffers.size() * 3 / 4);
        }
        
        if (m_medium_buffers.size() > 2) {
            m_medium_buffers.resize(m_medium_buffers.size() * 3 / 4);
        }
        
        if (m_large_buffers.size() > 2) {
            m_large_buffers.resize(m_large_buffers.size() * 3 / 4);
        }
    }
}

} // namespace IO
} // namespace PsyMP3
