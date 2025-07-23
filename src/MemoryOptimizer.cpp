/*
 * MemoryOptimizer.cpp - Memory optimization utilities for demuxer architecture
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

// EnhancedBufferPool implementation
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

// EnhancedAudioBufferPool implementation
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

// MemoryOptimizer implementation
MemoryOptimizer& MemoryOptimizer::getInstance() {
    static MemoryOptimizer instance;
    return instance;
}

MemoryOptimizer::MemoryOptimizer() {
    Debug::log("memory", "MemoryOptimizer::MemoryOptimizer() - Initializing memory optimizer");
    
    // Start memory pressure monitoring thread
    startMemoryMonitoring();
}

MemoryOptimizer::~MemoryOptimizer() {
    Debug::log("memory", "MemoryOptimizer::~MemoryOptimizer() - Destroying memory optimizer");
    
    // Stop memory monitoring thread
    stopMemoryMonitoring();
}

MemoryOptimizer::MemoryPressureLevel MemoryOptimizer::getMemoryPressureLevel() const {
    return m_memory_pressure_level;
}

size_t MemoryOptimizer::getOptimalBufferSize(size_t requested_size, 
                                           const std::string& usage_pattern,
                                           bool sequential_access) const {
    return calculateOptimalBufferSize(requested_size, m_memory_pressure_level, sequential_access);
}

bool MemoryOptimizer::isSafeToAllocate(size_t requested_size, const std::string& component_name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if allocation would exceed total memory limit
    if (m_total_memory_usage + requested_size > m_max_total_memory) {
        Debug::log("memory", "MemoryOptimizer::isSafeToAllocate() - Total memory limit would be exceeded: ",
                  m_total_memory_usage + requested_size, " > ", m_max_total_memory);
        return false;
    }
    
    // Check if allocation would exceed buffer memory limit for buffer-related components
    if ((component_name == "http" || component_name == "file" || component_name == "buffer") &&
        m_total_memory_usage + requested_size > m_max_buffer_memory) {
        Debug::log("memory", "MemoryOptimizer::isSafeToAllocate() - Buffer memory limit would be exceeded: ",
                  m_total_memory_usage + requested_size, " > ", m_max_buffer_memory);
        return false;
    }
    
    // Check memory pressure level
    if (m_memory_pressure_level == MemoryPressureLevel::Critical && requested_size > 64 * 1024) {
        Debug::log("memory", "MemoryOptimizer::isSafeToAllocate() - Critical memory pressure, rejecting large allocation: ", requested_size);
        return false;
    }
    
    if (m_memory_pressure_level == MemoryPressureLevel::High && requested_size > 256 * 1024) {
        Debug::log("memory", "MemoryOptimizer::isSafeToAllocate() - High memory pressure, rejecting very large allocation: ", requested_size);
        return false;
    }
    
    return true;
}

void MemoryOptimizer::registerAllocation(size_t allocated_size, const std::string& component_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_total_memory_usage += allocated_size;
    m_component_memory_usage[component_name] += allocated_size;
    
    // Update usage pattern
    auto& pattern = m_usage_patterns[component_name];
    pattern.total_allocations++;
    pattern.current_memory += allocated_size;
    pattern.peak_memory = std::max(pattern.peak_memory, pattern.current_memory);
    pattern.last_allocation = std::chrono::steady_clock::now();
    
    Debug::log("memory", "MemoryOptimizer::registerAllocation() - ", component_name, " allocated ", allocated_size, 
              " bytes, total: ", m_total_memory_usage);
}

void MemoryOptimizer::registerDeallocation(size_t deallocated_size, const std::string& component_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_total_memory_usage -= deallocated_size;
    if (m_component_memory_usage[component_name] >= deallocated_size) {
        m_component_memory_usage[component_name] -= deallocated_size;
    } else {
        m_component_memory_usage[component_name] = 0;
    }
    
    // Update usage pattern
    auto& pattern = m_usage_patterns[component_name];
    pattern.total_deallocations++;
    if (pattern.current_memory >= deallocated_size) {
        pattern.current_memory -= deallocated_size;
    } else {
        pattern.current_memory = 0;
    }
    pattern.last_deallocation = std::chrono::steady_clock::now();
    
    Debug::log("memory", "MemoryOptimizer::registerDeallocation() - ", component_name, " deallocated ", deallocated_size, 
              " bytes, total: ", m_total_memory_usage);
}

std::map<std::string, size_t> MemoryOptimizer::getMemoryStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::map<std::string, size_t> stats;
    stats["total_memory_usage"] = m_total_memory_usage;
    stats["max_total_memory"] = m_max_total_memory;
    stats["max_buffer_memory"] = m_max_buffer_memory;
    stats["memory_pressure_level"] = static_cast<size_t>(m_memory_pressure_level);
    
    // Add component-specific stats
    for (const auto& component : m_component_memory_usage) {
        stats["component_" + component.first] = component.second;
    }
    
    // Add usage pattern stats
    for (const auto& pattern : m_usage_patterns) {
        const std::string& name = pattern.first;
        const UsagePattern& usage = pattern.second;
        
        stats["pattern_" + name + "_allocations"] = usage.total_allocations;
        stats["pattern_" + name + "_deallocations"] = usage.total_deallocations;
        stats["pattern_" + name + "_peak_memory"] = usage.peak_memory;
        stats["pattern_" + name + "_current_memory"] = usage.current_memory;
    }
    
    return stats;
}

void MemoryOptimizer::setMemoryLimits(size_t max_total_memory, size_t max_buffer_memory) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_max_total_memory = max_total_memory;
    m_max_buffer_memory = max_buffer_memory;
    
    Debug::log("memory", "MemoryOptimizer::setMemoryLimits() - Set limits: total=", max_total_memory, 
              ", buffer=", max_buffer_memory);
}

void MemoryOptimizer::optimizeMemoryUsage() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Debug::log("memory", "MemoryOptimizer::optimizeMemoryUsage() - Starting global memory optimization");
    
    // Calculate memory usage percentage
    float usage_percent = m_max_total_memory > 0 ? 
        static_cast<float>(m_total_memory_usage) / static_cast<float>(m_max_total_memory) * 100.0f : 0.0f;
    
    Debug::log("memory", "MemoryOptimizer::optimizeMemoryUsage() - Memory usage: ", usage_percent, 
              "% (", m_total_memory_usage, " / ", m_max_total_memory, " bytes)");
    
    // Perform optimization based on memory pressure
    if (m_memory_pressure_level == MemoryPressureLevel::Critical) {
        // Critical memory pressure - aggressive optimization
        Debug::log("memory", "MemoryOptimizer::optimizeMemoryUsage() - Critical memory pressure, performing aggressive optimization");
        
        // Clear all buffer pools
        IOBufferPool::getInstance().clear();
        
        // Reduce buffer pool limits drastically
        size_t critical_pool_size = m_max_total_memory / 16; // 6.25% of total
        IOBufferPool::getInstance().setMaxPoolSize(critical_pool_size);
        IOBufferPool::getInstance().setMaxBuffersPerSize(1); // Minimal buffering
        
    } else if (m_memory_pressure_level == MemoryPressureLevel::High) {
        // High memory pressure - moderate optimization
        Debug::log("memory", "MemoryOptimizer::optimizeMemoryUsage() - High memory pressure, performing moderate optimization");
        
        // Optimize buffer pools
        IOBufferPool::getInstance().optimizeAllocationPatterns();
        IOBufferPool::getInstance().compactMemory();
        
        // Reduce buffer pool limits moderately
        size_t high_pool_size = m_max_total_memory / 8; // 12.5% of total
        IOBufferPool::getInstance().setMaxPoolSize(high_pool_size);
        IOBufferPool::getInstance().setMaxBuffersPerSize(2); // Reduced buffering
        
    } else if (m_memory_pressure_level == MemoryPressureLevel::Normal) {
        // Normal memory pressure - light optimization
        Debug::log("memory", "MemoryOptimizer::optimizeMemoryUsage() - Normal memory pressure, performing light optimization");
        
        // Defragment pools and optimize patterns
        IOBufferPool::getInstance().defragmentPools();
        IOBufferPool::getInstance().optimizeAllocationPatterns();
        
        // Use reasonable buffer pool limits
        size_t normal_pool_size = m_max_total_memory / 4; // 25% of total
        IOBufferPool::getInstance().setMaxPoolSize(normal_pool_size);
        IOBufferPool::getInstance().setMaxBuffersPerSize(4); // Moderate buffering
        
    } else {
        // Low memory pressure - maintenance optimization
        Debug::log("memory", "MemoryOptimizer::optimizeMemoryUsage() - Low memory pressure, performing maintenance optimization");
        
        // Just defragment to maintain efficiency
        IOBufferPool::getInstance().defragmentPools();
        
        // Can afford to increase buffer pool limits for better performance
        size_t low_pool_size = m_max_total_memory / 3; // 33% of total
        IOBufferPool::getInstance().setMaxPoolSize(low_pool_size);
        IOBufferPool::getInstance().setMaxBuffersPerSize(8); // Full buffering
    }
    
    Debug::log("memory", "MemoryOptimizer::optimizeMemoryUsage() - Optimization complete");
}

void MemoryOptimizer::getRecommendedBufferPoolParams(size_t& max_pool_size, size_t& max_buffers_per_size) const {
    switch (m_memory_pressure_level) {
        case MemoryPressureLevel::Critical:
            max_pool_size = m_max_total_memory / 16; // 6.25% of total
            max_buffers_per_size = 1;
            break;
        case MemoryPressureLevel::High:
            max_pool_size = m_max_total_memory / 8; // 12.5% of total
            max_buffers_per_size = 2;
            break;
        case MemoryPressureLevel::Normal:
            max_pool_size = m_max_total_memory / 4; // 25% of total
            max_buffers_per_size = 4;
            break;
        case MemoryPressureLevel::Low:
        default:
            max_pool_size = m_max_total_memory / 3; // 33% of total
            max_buffers_per_size = 8;
            break;
    }
}

bool MemoryOptimizer::shouldEnableReadAhead() const {
    return m_memory_pressure_level <= MemoryPressureLevel::Normal;
}

size_t MemoryOptimizer::getRecommendedReadAheadSize(size_t default_size) const {
    switch (m_memory_pressure_level) {
        case MemoryPressureLevel::Critical:
            return 0; // Disable read-ahead completely
        case MemoryPressureLevel::High:
            return default_size / 4; // 25% of default
        case MemoryPressureLevel::Normal:
            return default_size / 2; // 50% of default
        case MemoryPressureLevel::Low:
        default:
            return default_size; // Full read-ahead
    }
}

std::string MemoryOptimizer::memoryPressureLevelToString(MemoryPressureLevel level) {
    switch (level) {
        case MemoryPressureLevel::Low:
            return "Low";
        case MemoryPressureLevel::Normal:
            return "Normal";
        case MemoryPressureLevel::High:
            return "High";
        case MemoryPressureLevel::Critical:
            return "Critical";
        default:
            return "Unknown";
    }
}

void MemoryOptimizer::startMemoryMonitoring() {
    m_monitoring_active = true;
    m_monitoring_thread = std::thread([this]() {
        monitorMemoryPressure();
    });
}

void MemoryOptimizer::stopMemoryMonitoring() {
    m_monitoring_active = false;
    if (m_monitoring_thread.joinable()) {
        m_monitoring_thread.join();
    }
}

void MemoryOptimizer::monitorMemoryPressure() {
    using namespace std::chrono_literals;
    
    Debug::log("memory", "MemoryOptimizer::monitorMemoryPressure() - Starting memory pressure monitoring");
    
    while (m_monitoring_active) {
        // Sleep for monitoring interval
        std::this_thread::sleep_for(10s);
        
        // Check system memory pressure
        MemoryPressureLevel new_pressure = detectMemoryPressure();
        
        // Update memory pressure level if changed
        if (new_pressure != m_memory_pressure_level) {
            updateMemoryPressureLevel(new_pressure);
        }
    }
    
    Debug::log("memory", "MemoryOptimizer::monitorMemoryPressure() - Stopping memory pressure monitoring");
}

MemoryOptimizer::MemoryPressureLevel MemoryOptimizer::detectMemoryPressure() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Calculate memory usage percentage
    if (m_max_total_memory == 0) {
        return MemoryPressureLevel::Normal;
    }
    
    float usage_percent = static_cast<float>(m_total_memory_usage) / static_cast<float>(m_max_total_memory) * 100.0f;
    
    // Get system memory information if available
    size_t total_system_memory = 0;
    size_t available_system_memory = 0;
    bool has_system_info = getSystemMemoryInfo(total_system_memory, available_system_memory);
    
    if (has_system_info && total_system_memory > 0) {
        float system_usage_percent = static_cast<float>(total_system_memory - available_system_memory) / 
                                   static_cast<float>(total_system_memory) * 100.0f;
        
        // Use the higher of application usage or system usage
        usage_percent = std::max(usage_percent, system_usage_percent);
        
        Debug::log("memory", "MemoryOptimizer::detectMemoryPressure() - App usage: ", usage_percent, 
                  "%, System usage: ", system_usage_percent, "%");
    }
    
    // Determine memory pressure level based on usage percentage
    if (usage_percent > 95.0f) {
        return MemoryPressureLevel::Critical;
    } else if (usage_percent > 85.0f) {
        return MemoryPressureLevel::High;
    } else if (usage_percent > 60.0f) {
        return MemoryPressureLevel::Normal;
    } else {
        return MemoryPressureLevel::Low;
    }
}

void MemoryOptimizer::updateMemoryPressureLevel(MemoryPressureLevel new_level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    MemoryPressureLevel old_level = m_memory_pressure_level;
    m_memory_pressure_level = new_level;
    
    Debug::log("memory", "MemoryOptimizer::updateMemoryPressureLevel() - Memory pressure changed from ",
              memoryPressureLevelToString(old_level), " to ", memoryPressureLevelToString(new_level));
    
    // Trigger optimization based on new pressure level
    if (new_level > old_level) {
        // Pressure increased - perform immediate optimization
        optimizeMemoryUsage();
    }
}

bool MemoryOptimizer::getSystemMemoryInfo(size_t& total_memory, size_t& available_memory) const {
#ifdef _WIN32
    // Windows memory information
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        total_memory = static_cast<size_t>(memInfo.ullTotalPhys);
        available_memory = static_cast<size_t>(memInfo.ullAvailPhys);
        return true;
    }
    return false;
#elif defined(__linux__)
    // Linux memory information from /proc/meminfo
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open()) {
        return false;
    }
    
    std::string line;
    size_t mem_total = 0;
    size_t mem_available = 0;
    bool found_total = false;
    bool found_available = false;
    
    while (std::getline(meminfo, line) && (!found_total || !found_available)) {
        if (line.find("MemTotal:") == 0) {
            std::istringstream iss(line);
            std::string label, value, unit;
            iss >> label >> value >> unit;
            mem_total = std::stoull(value) * 1024; // Convert KB to bytes
            found_total = true;
        } else if (line.find("MemAvailable:") == 0) {
            std::istringstream iss(line);
            std::string label, value, unit;
            iss >> label >> value >> unit;
            mem_available = std::stoull(value) * 1024; // Convert KB to bytes
            found_available = true;
        }
    }
    
    if (found_total && found_available) {
        total_memory = mem_total;
        available_memory = mem_available;
        return true;
    }
    return false;
#elif defined(__APPLE__)
    // macOS memory information
    int mib[2];
    size_t length;
    
    // Get total physical memory
    mib[0] = CTL_HW;
    mib[1] = HW_MEMSIZE;
    length = sizeof(total_memory);
    if (sysctl(mib, 2, &total_memory, &length, NULL, 0) != 0) {
        return false;
    }
    
    // Get available memory (this is more complex on macOS)
    vm_size_t page_size;
    vm_statistics64_data_t vm_stat;
    mach_msg_type_number_t host_size = sizeof(vm_stat) / sizeof(natural_t);
    
    if (host_page_size(mach_host_self(), &page_size) != KERN_SUCCESS) {
        return false;
    }
    
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm_stat, &host_size) != KERN_SUCCESS) {
        return false;
    }
    
    available_memory = (vm_stat.free_count + vm_stat.inactive_count) * page_size;
    return true;
#else
    // Unsupported platform
    return false;
#endif
}

size_t MemoryOptimizer::calculateOptimalBufferSize(size_t base_size, 
                                                 MemoryPressureLevel pressure_level,
                                                 bool sequential) const {
    size_t optimal_size = base_size;
    
    // Adjust based on memory pressure
    switch (pressure_level) {
        case MemoryPressureLevel::Critical:
            optimal_size = std::min(base_size, static_cast<size_t>(8 * 1024)); // Max 8KB
            break;
        case MemoryPressureLevel::High:
            optimal_size = std::min(base_size, static_cast<size_t>(32 * 1024)); // Max 32KB
            break;
        case MemoryPressureLevel::Normal:
            optimal_size = std::min(base_size, static_cast<size_t>(128 * 1024)); // Max 128KB
            break;
        case MemoryPressureLevel::Low:
        default:
            optimal_size = std::min(base_size, static_cast<size_t>(512 * 1024)); // Max 512KB
            break;
    }
    
    // Adjust for access pattern
    if (sequential && pressure_level <= MemoryPressureLevel::Normal) {
        // Sequential access can benefit from larger buffers
        optimal_size = std::min(optimal_size * 2, static_cast<size_t>(256 * 1024));
    } else if (!sequential && pressure_level >= MemoryPressureLevel::High) {
        // Random access under pressure should use smaller buffers
        optimal_size = optimal_size / 2;
    }
    
    // Ensure minimum buffer size
    optimal_size = std::max(optimal_size, static_cast<size_t>(4 * 1024)); // Min 4KB
    
    return optimal_size;
            return 0; // No read-ahead
        case MemoryPressureLevel::High:
            return default_size / 4; // 25% of default
        case MemoryPressureLevel::Normal:
            return default_size / 2; // 50% of default
        case MemoryPressureLevel::Low:
        default:
            return default_size; // Full read-ahead
    }
}

std::string MemoryOptimizer::memoryPressureLevelToString(MemoryPressureLevel level) {
    switch (level) {
        case MemoryPressureLevel::Low:
            return "Low";
        case MemoryPressureLevel::Normal:
            return "Normal";
        case MemoryPressureLevel::High:
            return "High";
        case MemoryPressureLevel::Critical:
            return "Critical";
        default:
            return "Unknown";
    }
}

void MemoryOptimizer::startMemoryMonitoring() {
    m_monitoring_active = true;
    m_monitoring_thread = std::thread([this]() {
        monitorMemoryPressure();
    });
}

void MemoryOptimizer::stopMemoryMonitoring() {
    m_monitoring_active = false;
    if (m_monitoring_thread.joinable()) {
        m_monitoring_thread.join();
    }
}

void MemoryOptimizer::monitorMemoryPressure() {
    using namespace std::chrono_literals;
    
    Debug::log("memory", "MemoryOptimizer::monitorMemoryPressure() - Starting memory pressure monitoring");
    
    while (m_monitoring_active) {
        // Sleep for monitoring interval
        std::this_thread::sleep_for(10s);
        
        // Check system memory pressure
        MemoryPressureLevel new_pressure = detectMemoryPressure();
        
        // Update memory pressure level if changed
        if (new_pressure != m_memory_pressure_level) {
            updateMemoryPressureLevel(new_pressure);
        }
    }
    
    Debug::log("memory", "MemoryOptimizer::monitorMemoryPressure() - Stopping memory pressure monitoring");
}

MemoryOptimizer::MemoryPressureLevel MemoryOptimizer::detectMemoryPressure() {
    size_t total_memory = 0;
    size_t available_memory = 0;
    
    if (!getSystemMemoryInfo(total_memory, available_memory)) {
        // Fallback to internal memory tracking
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_max_total_memory > 0) {
            float usage_percent = static_cast<float>(m_total_memory_usage) / static_cast<float>(m_max_total_memory) * 100.0f;
            
            if (usage_percent > 90.0f) {
                return MemoryPressureLevel::Critical;
            } else if (usage_percent > 75.0f) {
                return MemoryPressureLevel::High;
            } else if (usage_percent > 50.0f) {
                return MemoryPressureLevel::Normal;
            } else {
                return MemoryPressureLevel::Low;
            }
        }
        return MemoryPressureLevel::Normal;
    }
    
    // Calculate system memory usage percentage
    float usage_percent = total_memory > 0 ? 
        static_cast<float>(total_memory - available_memory) / static_cast<float>(total_memory) * 100.0f : 0.0f;
    
    if (usage_percent > 90.0f) {
        return MemoryPressureLevel::Critical;
    } else if (usage_percent > 80.0f) {
        return MemoryPressureLevel::High;
    } else if (usage_percent > 60.0f) {
        return MemoryPressureLevel::Normal;
    } else {
        return MemoryPressureLevel::Low;
    }
}

void MemoryOptimizer::updateMemoryPressureLevel(MemoryPressureLevel new_level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Debug::log("memory", "MemoryOptimizer::updateMemoryPressureLevel() - Memory pressure changed from ",
              memoryPressureLevelToString(m_memory_pressure_level), " to ",
              memoryPressureLevelToString(new_level));
    
    m_memory_pressure_level = new_level;
    
    // Trigger optimization if pressure increased
    if (new_level > MemoryPressureLevel::Normal) {
        optimizeMemoryUsage();
    }
}

bool MemoryOptimizer::getSystemMemoryInfo(size_t& total_memory, size_t& available_memory) const {
#ifdef _WIN32
    // Windows implementation
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        total_memory = status.ullTotalPhys;
        available_memory = status.ullAvailPhys;
        return true;
    }
#elif defined(__APPLE__)
    // macOS implementation
    int mib[2];
    size_t length;
    
    mib[0] = CTL_HW;
    mib[1] = HW_MEMSIZE;
    length = sizeof(total_memory);
    if (sysctl(mib, 2, &total_memory, &length, NULL, 0) == 0) {
        struct vm_statistics64 vm_stats;
        mach_port_t host_port = mach_host_self();
        mach_msg_type_number_t host_size = sizeof(vm_statistics64) / sizeof(integer_t);
        if (host_statistics64(host_port, HOST_VM_INFO64, (host_info64_t)&vm_stats, &host_size) == KERN_SUCCESS) {
            available_memory = vm_stats.free_count * PAGE_SIZE;
            return true;
        }
    }
#else
    // Linux implementation
    FILE* file = fopen("/proc/meminfo", "r");
    if (file) {
        char line[128];
        bool found_total = false, found_available = false;
        
        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "MemTotal:", 9) == 0) {
                total_memory = static_cast<size_t>(std::stoll(line + 9) * 1024);
                found_total = true;
            } else if (strncmp(line, "MemAvailable:", 13) == 0) {
                available_memory = static_cast<size_t>(std::stoll(line + 13) * 1024);
                found_available = true;
            }
            
            if (found_total && found_available) {
                break;
            }
        }
        fclose(file);
        
        return found_total && found_available;
    }
#endif
    
    return false;
}

size_t MemoryOptimizer::calculateOptimalBufferSize(size_t base_size, 
                                                  MemoryPressureLevel pressure_level,
                                                  bool sequential) const {
    size_t optimal_size = base_size;
    
    // Adjust based on memory pressure
    switch (pressure_level) {
        case MemoryPressureLevel::Critical:
            optimal_size = std::min(base_size, static_cast<size_t>(8 * 1024)); // Max 8KB
            break;
        case MemoryPressureLevel::High:
            optimal_size = std::min(base_size, static_cast<size_t>(32 * 1024)); // Max 32KB
            break;
        case MemoryPressureLevel::Normal:
            optimal_size = std::min(base_size, static_cast<size_t>(128 * 1024)); // Max 128KB
            break;
        case MemoryPressureLevel::Low:
        default:
            optimal_size = std::min(base_size, static_cast<size_t>(512 * 1024)); // Max 512KB
            break;
    }
    
    // Adjust for access pattern
    if (sequential && pressure_level <= MemoryPressureLevel::Normal) {
        optimal_size = std::min(optimal_size * 2, static_cast<size_t>(256 * 1024)); // Double for sequential, max 256KB
    } else if (!sequential && pressure_level >= MemoryPressureLevel::High) {
        optimal_size = optimal_size / 2; // Halve for random access under pressure
    }
    
    // Ensure minimum size
    optimal_size = std::max(optimal_size, static_cast<size_t>(4 * 1024)); // Min 4KB
    
    return optimal_size;
}

// MemoryTracker implementation
MemoryTracker& MemoryTracker::getInstance() {
    static MemoryTracker instance;
    return instance;
}

MemoryTracker::MemoryTracker() 
    : m_memory_pressure_level(0)
    , m_next_callback_id(1)
    , m_auto_tracking_enabled(false)
    , m_auto_tracking_interval_ms(5000)
    , m_cleanup_requested(false)
    , m_cleanup_urgency(0)
    , m_last_cleanup_request(std::chrono::steady_clock::now()) {
    // Initialize memory stats
    update();
}

MemoryTracker::~MemoryTracker() {
    // Stop auto-tracking if active
    stopAutoTracking();
}

void MemoryTracker::update() {
    MemoryStats new_stats{};
    int new_pressure_level = 0;
    
    // Set last update time
    new_stats.last_update = std::chrono::steady_clock::now();
    
    // Platform-specific memory usage detection
    #ifdef _WIN32
    // Windows implementation
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        new_stats.total_physical_memory = status.ullTotalPhys;
        new_stats.available_physical_memory = status.ullAvailPhys;
        new_pressure_level = static_cast<int>(100 - (status.ullAvailPhys * 100 / status.ullTotalPhys));
        new_stats.virtual_memory_usage = status.ullTotalVirtual - status.ullAvailVirtual;
    }
    
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        new_stats.process_memory_usage = pmc.WorkingSetSize;
        new_stats.peak_memory_usage = pmc.PeakWorkingSetSize;
    }
    #elif defined(__APPLE__)
    // macOS implementation
    int mib[2];
    size_t length;
    struct vm_statistics64 vm_stats;
    
    mib[0] = CTL_HW;
    mib[1] = HW_MEMSIZE;
    length = sizeof(new_stats.total_physical_memory);
    if (sysctl(mib, 2, &new_stats.total_physical_memory, &length, NULL, 0) == 0) {
        mach_port_t host_port = mach_host_self();
        mach_msg_type_number_t host_size = sizeof(vm_statistics64) / sizeof(integer_t);
        host_statistics64(host_port, HOST_VM_INFO64, (host_info64_t)&vm_stats, &host_size);
        
        new_stats.available_physical_memory = vm_stats.free_count * PAGE_SIZE;
        new_pressure_level = static_cast<int>(100 - (new_stats.available_physical_memory * 100 / new_stats.total_physical_memory));
    }
    
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count) == KERN_SUCCESS) {
        new_stats.process_memory_usage = t_info.resident_size;
        new_stats.virtual_memory_usage = t_info.virtual_size;
    }
    
    // Get peak memory usage (not directly available on macOS)
    new_stats.peak_memory_usage = new_stats.process_memory_usage;
    #else
    // Linux implementation
    FILE* file = fopen("/proc/meminfo", "r");
    if (file) {
        char line[128];
        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "MemTotal:", 9) == 0) {
                new_stats.total_physical_memory = static_cast<size_t>(std::stoll(line + 9) * 1024);
            } else if (strncmp(line, "MemAvailable:", 13) == 0) {
                new_stats.available_physical_memory = static_cast<size_t>(std::stoll(line + 13) * 1024);
            }
        }
        fclose(file);
        
        if (new_stats.total_physical_memory > 0) {
            new_pressure_level = static_cast<int>(100 - (new_stats.available_physical_memory * 100 / new_stats.total_physical_memory));
        }
    }
    
    // Get process memory usage
    file = fopen("/proc/self/statm", "r");
    if (file) {
        unsigned long size, resident;
        if (fscanf(file, "%lu %lu", &size, &resident) == 2) {
            new_stats.process_memory_usage = resident * sysconf(_SC_PAGESIZE);
            new_stats.virtual_memory_usage = size * sysconf(_SC_PAGESIZE);
        }
        fclose(file);
    }
    
    // Get peak memory usage
    file = fopen("/proc/self/status", "r");
    if (file) {
        char line[128];
        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "VmHWM:", 6) == 0) {
                new_stats.peak_memory_usage = static_cast<size_t>(std::stoll(line + 6) * 1024);
                break;
            }
        }
        fclose(file);
    }
    #endif
    
    // Update stats and pressure level
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Store previous stats for trend calculation
        if (m_memory_history.size() >= MEMORY_HISTORY_SIZE) {
            m_memory_history.pop_front();
        }
        
        // Add current memory usage to history
        m_memory_history.push_back({new_stats.last_update, new_stats.process_memory_usage});
        
        // Calculate memory usage trend
        calculateMemoryTrend();
        
        // Update stats
        m_stats = new_stats;
    }
    
    // Only notify if pressure level changed significantly (by at least 5%)
    int old_level = m_memory_pressure_level.load();
    if (std::abs(new_pressure_level - old_level) >= 5) {
        m_memory_pressure_level = new_pressure_level;
        notifyCallbacks();
    }
}

int MemoryTracker::getMemoryPressureLevel() const {
    return m_memory_pressure_level;
}

int MemoryTracker::registerMemoryPressureCallback(std::function<void(int)> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    int id = m_next_callback_id++;
    m_callbacks.push_back({id, callback});
    
    // Immediately notify with current pressure level
    callback(m_memory_pressure_level);
    
    return id;
}

void MemoryTracker::unregisterMemoryPressureCallback(int id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = std::find_if(m_callbacks.begin(), m_callbacks.end(),
                          [id](const CallbackInfo& info) { return info.id == id; });
    
    if (it != m_callbacks.end()) {
        m_callbacks.erase(it);
    }
}

MemoryTracker::MemoryStats MemoryTracker::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void MemoryTracker::startAutoTracking(unsigned int interval_ms) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_auto_tracking_enabled) {
        return; // Already running
    }
    
    m_auto_tracking_interval_ms = interval_ms;
    m_auto_tracking_enabled = true;
    
    // Start tracking thread
    m_auto_tracking_thread = std::thread(&MemoryTracker::autoTrackingThread, this);
}

void MemoryTracker::stopAutoTracking() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_auto_tracking_enabled) {
            return; // Not running
        }
        
        m_auto_tracking_enabled = false;
    }
    
    // Wait for thread to finish
    if (m_auto_tracking_thread.joinable()) {
        m_auto_tracking_thread.join();
    }
}

void MemoryTracker::requestMemoryCleanup(int urgency_level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Only allow cleanup requests at a reasonable interval
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - m_last_cleanup_request).count() < 10) {
        return; // Too soon for another cleanup
    }
    
    m_last_cleanup_request = now;
    m_cleanup_requested = true;
    m_cleanup_urgency = urgency_level;
    
    // Notify all callbacks with the urgency level
    for (const auto& info : m_callbacks) {
        try {
            info.callback(urgency_level);
        } catch (...) {
            // Ignore exceptions from callbacks
        }
    }
    
    // Reset cleanup flag after notification
    m_cleanup_requested = false;
}

void MemoryTracker::notifyCallbacks() {
    std::vector<CallbackInfo> callbacks;
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        callbacks = m_callbacks;
    }
    
    int level = m_memory_pressure_level.load();
    for (const auto& info : callbacks) {
        try {
            info.callback(level);
        } catch (...) {
            // Ignore exceptions from callbacks
        }
    }
}

void MemoryTracker::autoTrackingThread() {
    Debug::log("memory", "MemoryTracker: Auto-tracking thread started");
    
    while (m_auto_tracking_enabled) {
        // Update memory stats
        update();
        
        // Check if we need to request cleanup
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            
            // Request cleanup if memory pressure is high
            if (m_memory_pressure_level > 80 && m_stats.memory_usage_trend > 0.1f) {
                requestMemoryCleanup(m_memory_pressure_level);
            }
        }
        
        // Sleep for the specified interval
        std::this_thread::sleep_for(std::chrono::milliseconds(m_auto_tracking_interval_ms));
    }
    
    Debug::log("memory", "MemoryTracker: Auto-tracking thread stopped");
}

void MemoryTracker::calculateMemoryTrend() {
    if (m_memory_history.size() < 2) {
        m_stats.memory_usage_trend = 0.0f;
        return;
    }
    
    // Calculate trend over the available history
    auto oldest = m_memory_history.front();
    auto newest = m_memory_history.back();
    
    auto time_diff = std::chrono::duration_cast<std::chrono::seconds>(
        newest.first - oldest.first).count();
    
    if (time_diff <= 0) {
        m_stats.memory_usage_trend = 0.0f;
        return;
    }
    
    // Calculate memory change rate in MB per second
    float memory_diff_mb = static_cast<float>(newest.second - oldest.second) / (1024 * 1024);
    m_stats.memory_usage_trend = memory_diff_mb / time_diff;
}
// De
muxerStreamingBuffer implementation
class DemuxerStreamingBuffer {
public:
    /**
     * @brief Constructor
     * @param max_chunks Maximum number of chunks to buffer
     * @param max_bytes Maximum memory usage in bytes
     */
    DemuxerStreamingBuffer::DemuxerStreamingBuffer(size_t max_chunks, size_t max_bytes)
        : m_chunk_queue(max_chunks, max_bytes, calculateChunkMemory) {
    }
    
    /**
     * @brief Add a chunk to the buffer
     * @param chunk Chunk to add
     * @return true if chunk was added, false if buffer is full
     */
    bool DemuxerStreamingBuffer::addChunk(const std::vector<uint8_t>& chunk) {
        return m_chunk_queue.tryPush(chunk);
    }
    
    /**
     * @brief Add a chunk to the buffer
     * @param chunk Chunk to add (will be moved)
     * @return true if chunk was added, false if buffer is full
     */
    bool DemuxerStreamingBuffer::addChunk(std::vector<uint8_t>&& chunk) {
        return m_chunk_queue.tryPush(std::move(chunk));
    }
    
    /**
     * @brief Get a chunk from the buffer
     * @param chunk Reference to store the chunk
     * @return true if a chunk was retrieved, false if buffer is empty
     */
    bool DemuxerStreamingBuffer::getChunk(std::vector<uint8_t>& chunk) {
        return m_chunk_queue.tryPop(chunk);
    }
    
    /**
     * @brief Check if the buffer is empty
     */
    bool DemuxerStreamingBuffer::isEmpty() const {
        return m_chunk_queue.empty();
    }
    
    /**
     * @brief Get the number of chunks in the buffer
     */
    size_t DemuxerStreamingBuffer::getChunkCount() const {
        return m_chunk_queue.size();
    }
    
    /**
     * @brief Get the current memory usage in bytes
     */
    size_t DemuxerStreamingBuffer::getMemoryUsage() const {
        return m_chunk_queue.memoryUsage();
    }
    
    /**
     * @brief Clear the buffer
     */
    void DemuxerStreamingBuffer::clear() {
        m_chunk_queue.clear();
    }
    
    /**
     * @brief Set the maximum number of chunks
     */
    void DemuxerStreamingBuffer::setMaxChunks(size_t max_chunks) {
        m_chunk_queue.setMaxItems(max_chunks);
    }
    
    /**
     * @brief Set the maximum memory usage in bytes
     */
    void DemuxerStreamingBuffer::setMaxBytes(size_t max_bytes) {
        m_chunk_queue.setMaxMemoryBytes(max_bytes);
    }
    
    /**
     * @brief Get buffer statistics
     */
    DemuxerStreamingBuffer::BufferStats DemuxerStreamingBuffer::getStats() const {
        auto queue_stats = m_chunk_queue.getStats();
        
        BufferStats stats;
        stats.current_chunks = queue_stats.current_items;
        stats.current_bytes = queue_stats.current_memory_bytes;
        stats.max_chunks = queue_stats.max_items;
        stats.max_bytes = queue_stats.max_memory_bytes;
        stats.total_chunks_added = queue_stats.total_items_pushed;
        stats.total_chunks_retrieved = queue_stats.total_items_popped;
        stats.total_chunks_dropped = queue_stats.total_items_dropped;
        stats.fullness_ratio = queue_stats.fullness_ratio;
        
        return stats;
    }
    
private:
    static size_t calculateChunkMemory(const std::vector<uint8_t>& chunk) {
        // Base size of the vector structure
        size_t base_size = sizeof(std::vector<uint8_t>);
        
        // Size of the data
        size_t data_size = chunk.capacity() * sizeof(uint8_t);
        
        return base_size + data_size;
    }
    
    BoundedQueue<std::vector<uint8_t>> m_chunk_queue;
};