/*
 * EnhancedAudioBufferPool.cpp - Enhanced audio buffer pool for memory optimization
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

EnhancedAudioBufferPool& EnhancedAudioBufferPool::getInstance() {
    static EnhancedAudioBufferPool instance;
    return instance;
}

EnhancedAudioBufferPool::EnhancedAudioBufferPool() 
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

EnhancedAudioBufferPool::~EnhancedAudioBufferPool() {
    clear();
}

std::vector<int16_t> EnhancedAudioBufferPool::getSampleBuffer(size_t min_samples, size_t preferred_samples) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Perform periodic cleanup if needed
    performPeriodicCleanup();
    
    // Use preferred size if specified, otherwise use min_size
    size_t target_samples = preferred_samples > min_samples ? preferred_samples : min_samples;
    
    // Update usage statistics
    auto& usage = m_size_usage_stats[min_samples];
    usage.request_count++;
    usage.last_request = std::chrono::steady_clock::now();
    
    // Don't pool very large buffers
    if (min_samples > getMaxSamplesPerBuffer()) {
        m_buffer_misses++;
        std::vector<int16_t> buffer;
        buffer.reserve(min_samples);
        return buffer;
    }
    
    // Select appropriate buffer category
    std::vector<std::vector<int16_t>>* category = &m_medium_buffers;
    if (min_samples < SMALL_BUFFER_THRESHOLD) {
        category = &m_small_buffers;
    } else if (min_samples > MEDIUM_BUFFER_THRESHOLD) {
        category = &m_large_buffers;
    }
    
    // Find a suitable buffer from the pool
    for (auto it = category->begin(); it != category->end(); ++it) {
        if (it->capacity() >= min_samples) {
            std::vector<int16_t> buffer = std::move(*it);
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
            if (it->capacity() >= min_samples) {
                std::vector<int16_t> buffer = std::move(*it);
                m_large_buffers.erase(it);
                buffer.clear();
                m_buffer_hits++;
                m_buffer_reuse_count++;
                return buffer;
            }
        }
    }
    
    if (category != &m_medium_buffers && min_samples >= SMALL_BUFFER_THRESHOLD) {
        for (auto it = m_medium_buffers.begin(); it != m_medium_buffers.end(); ++it) {
            if (it->capacity() >= min_samples) {
                std::vector<int16_t> buffer = std::move(*it);
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
    std::vector<int16_t> buffer;
    
    // Use optimal buffer sizes based on common audio frame sizes
    if (target_samples <= 2048) {
        buffer.reserve(2048); // ~42ms at 48kHz stereo
    } else if (target_samples <= 4096) {
        buffer.reserve(4096); // ~85ms at 48kHz stereo
    } else if (target_samples <= 16384) {
        buffer.reserve(16384); // ~340ms at 48kHz stereo
    } else if (target_samples <= 32768) {
        buffer.reserve(32768); // ~680ms at 48kHz stereo
    } else {
        // Round up to nearest 16K samples for very large buffers
        size_t rounded_size = ((target_samples + 16383) / 16384) * 16384;
        buffer.reserve(rounded_size);
    }
    
    return buffer;
}

void EnhancedAudioBufferPool::returnSampleBuffer(std::vector<int16_t>&& buffer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Only pool buffers that are reasonably sized
    if (!shouldPoolBuffer(buffer.capacity())) {
        return; // Let the buffer be destroyed naturally
    }
    
    // Determine which category to return to
    std::vector<std::vector<int16_t>>* category = &m_medium_buffers;
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

void EnhancedAudioBufferPool::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_small_buffers.clear();
    m_medium_buffers.clear();
    m_large_buffers.clear();
}

void EnhancedAudioBufferPool::setMemoryPressure(int pressure_level) {
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

int EnhancedAudioBufferPool::getMemoryPressure() const {
    return m_memory_pressure;
}

EnhancedAudioBufferPool::PoolStats EnhancedAudioBufferPool::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    PoolStats stats{};
    stats.total_buffers = m_small_buffers.size() + m_medium_buffers.size() + m_large_buffers.size();
    stats.largest_buffer_size = 0;
    stats.total_samples = 0;
    stats.buffer_hits = m_buffer_hits;
    stats.buffer_misses = m_buffer_misses;
    stats.memory_pressure = m_memory_pressure;
    stats.reuse_count = m_buffer_reuse_count;
    
    auto process_category = [&stats](const std::vector<std::vector<int16_t>>& category) {
        for (const auto& buffer : category) {
            size_t capacity = buffer.capacity();
            stats.total_samples += capacity;
            stats.largest_buffer_size = std::max(stats.largest_buffer_size, capacity);
        }
    };
    
    process_category(m_small_buffers);
    process_category(m_medium_buffers);
    process_category(m_large_buffers);
    
    // Calculate hit ratio
    size_t total_requests = stats.buffer_hits + stats.buffer_misses;
    stats.hit_ratio = total_requests > 0 ? static_cast<float>(stats.buffer_hits) / total_requests : 0.0f;
    
    return stats;
}

size_t EnhancedAudioBufferPool::getMaxPooledBuffers() const {
    // Scale max buffers based on memory pressure
    // At 0% pressure: 16 buffers
    // At 100% pressure: 4 buffers
    return DEFAULT_MAX_POOLED_BUFFERS - ((DEFAULT_MAX_POOLED_BUFFERS - 4) * m_memory_pressure) / 100;
}

size_t EnhancedAudioBufferPool::getMaxSamplesPerBuffer() const {
    // Scale max buffer size based on memory pressure
    // At 0% pressure: 192K samples
    // At 100% pressure: 48K samples
    return DEFAULT_MAX_SAMPLES_PER_BUFFER - ((DEFAULT_MAX_SAMPLES_PER_BUFFER - 48 * 1024) * m_memory_pressure) / 100;
}

bool EnhancedAudioBufferPool::shouldPoolBuffer(size_t capacity) const {
    // Don't pool tiny buffers
    if (capacity < 1024) {
        return false;
    }
    
    // Don't pool buffers larger than the current max
    if (capacity > getMaxSamplesPerBuffer()) {
        return false;
    }
    
    // Under high memory pressure, be more selective
    if (m_memory_pressure > 70 && capacity > MEDIUM_BUFFER_THRESHOLD) {
        return false;
    }
    
    return true;
}

void EnhancedAudioBufferPool::performPeriodicCleanup() {
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