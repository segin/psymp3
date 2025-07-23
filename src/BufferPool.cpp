/*
 * BufferPool.cpp - Memory pool for efficient buffer allocation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

void IOBufferPool::Buffer::release() {
    if (m_data && m_pool) {
        m_pool->release(m_data, m_size);
        m_data = nullptr;
        m_size = 0;
        m_pool = nullptr;
    }
}

IOBufferPool& IOBufferPool::getInstance() {
    static IOBufferPool instance;
    return instance;
}

IOBufferPool::IOBufferPool() : m_memory_pressure_level(MemoryPressureLevel::Normal) {
    Debug::log("memory", "IOBufferPool::IOBufferPool() - Initializing buffer pool");
    
    // Initialize common buffer sizes for fast allocation
    m_common_sizes = {
        4 * 1024,     // 4KB - Small metadata
        8 * 1024,     // 8KB - Minimum HTTP range
        16 * 1024,    // 16KB - Small audio frames
        32 * 1024,    // 32KB - Medium buffer
        64 * 1024,    // 64KB - Default buffer size
        128 * 1024,   // 128KB - Large buffer
        256 * 1024,   // 256KB - HTTP range batch
        512 * 1024    // 512KB - Maximum buffer size
    };
    
    // Pre-allocate buffers for common sizes to reduce allocation overhead during playback
    preAllocateCommonBuffers();
    
    // Start memory pressure monitoring thread
    startMemoryMonitoring();
}

IOBufferPool::~IOBufferPool() {
    Debug::log("memory", "IOBufferPool::~IOBufferPool() - Destroying buffer pool");
    
    // Stop memory monitoring thread
    stopMemoryMonitoring();
    
    // Clear all pooled buffers
    clear();
}

IOBufferPool::Buffer IOBufferPool::acquire(size_t size) {
    if (size == 0) {
        Debug::log("memory", "BufferPool::acquire() - Zero size requested");
        return Buffer();
    }
    
    // Check if this size should be pooled
    if (!shouldPool(size)) {
        Debug::log("memory", "BufferPool::acquire() - Size ", size, " not suitable for pooling, allocating directly");
        try {
            uint8_t* data = new uint8_t[size];
            return Buffer(data, size, nullptr); // nullptr pool means direct allocation
        } catch (const std::bad_alloc& e) {
            Debug::log("memory", "BufferPool::acquire() - Direct allocation failed: ", e.what());
            return Buffer();
        }
    }
    
    size_t pool_size = roundToPoolSize(size);
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_pools.find(pool_size);
    if (it != m_pools.end() && !it->second.available_buffers.empty()) {
        // Reuse existing buffer from pool
        uint8_t* data = it->second.available_buffers.back();
        it->second.available_buffers.pop_back();
        it->second.pool_hits++;
        m_current_pool_size -= pool_size;
        
        Debug::log("memory", "BufferPool::acquire() - Reused buffer of size ", pool_size, " (pool hits: ", it->second.pool_hits, ")");
        return Buffer(data, pool_size, this);
    }
    
    // Need to allocate new buffer
    try {
        uint8_t* data = new uint8_t[pool_size];
        
        // Create pool entry if it doesn't exist
        if (it == m_pools.end()) {
            it = m_pools.emplace(pool_size, PoolEntry(pool_size)).first;
        }
        
        it->second.total_allocated++;
        it->second.pool_misses++;
        
        Debug::log("memory", "BufferPool::acquire() - Allocated new buffer of size ", pool_size, 
                  " (total allocated: ", it->second.total_allocated, ", pool misses: ", it->second.pool_misses, ")");
        
        return Buffer(data, pool_size, this);
    } catch (const std::bad_alloc& e) {
        Debug::log("memory", "BufferPool::acquire() - Allocation failed for size ", pool_size, ": ", e.what());
        
        // Try to free some pooled memory and retry
        evictIfNeededInternal();
        
        try {
            uint8_t* data = new uint8_t[pool_size];
            if (it == m_pools.end()) {
                it = m_pools.emplace(pool_size, PoolEntry(pool_size)).first;
            }
            it->second.total_allocated++;
            it->second.pool_misses++;
            
            Debug::log("memory", "BufferPool::acquire() - Allocation succeeded after eviction");
            return Buffer(data, pool_size, this);
        } catch (const std::bad_alloc& e2) {
            Debug::log("memory", "BufferPool::acquire() - Allocation still failed after eviction: ", e2.what());
            return Buffer();
        }
    }
}

void IOBufferPool::release(uint8_t* data, size_t size) {
    if (!data) {
        return;
    }
    
    // If this was a direct allocation (not from pool), just delete it
    if (!shouldPool(size)) {
        delete[] data;
        Debug::log("memory", "BufferPool::release() - Direct deallocation of size ", size);
        return;
    }
    
    size_t pool_size = roundToPoolSize(size);
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_pools.find(pool_size);
    if (it == m_pools.end()) {
        // Pool doesn't exist, just delete
        delete[] data;
        Debug::log("memory", "BufferPool::release() - Pool doesn't exist for size ", pool_size, ", deleting");
        return;
    }
    
    // Check if we can add to pool (use effective limits based on memory pressure)
    if (it->second.available_buffers.size() >= m_effective_max_buffers_per_size ||
        m_current_pool_size + pool_size > m_effective_max_pool_size) {
        // Pool is full or would exceed limit, just delete
        delete[] data;
        Debug::log("memory", "BufferPool::release() - Pool full for size ", pool_size, ", deleting");
        return;
    }
    
    // Add to pool for reuse
    it->second.available_buffers.push_back(data);
    m_current_pool_size += pool_size;
    
    Debug::log("memory", "BufferPool::release() - Returned buffer of size ", pool_size, 
              " to pool (pool size: ", it->second.available_buffers.size(), 
              ", total pooled: ", m_current_pool_size, ")");
}

std::map<std::string, size_t> IOBufferPool::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::map<std::string, size_t> stats;
    stats["pool_count"] = m_pools.size();
    stats["current_pool_size"] = m_current_pool_size;
    stats["max_pool_size"] = m_max_pool_size;
    stats["max_buffers_per_size"] = m_max_buffers_per_size;
    
    size_t total_allocated = 0;
    size_t total_pool_hits = 0;
    size_t total_pool_misses = 0;
    size_t total_pooled_buffers = 0;
    
    for (const auto& pool : m_pools) {
        total_allocated += pool.second.total_allocated;
        total_pool_hits += pool.second.pool_hits;
        total_pool_misses += pool.second.pool_misses;
        total_pooled_buffers += pool.second.available_buffers.size();
    }
    
    stats["total_allocated"] = total_allocated;
    stats["total_pool_hits"] = total_pool_hits;
    stats["total_pool_misses"] = total_pool_misses;
    stats["total_pooled_buffers"] = total_pooled_buffers;
    
    if (total_pool_hits + total_pool_misses > 0) {
        stats["hit_rate_percent"] = (total_pool_hits * 100) / (total_pool_hits + total_pool_misses);
    } else {
        stats["hit_rate_percent"] = 0;
    }
    
    return stats;
}

void IOBufferPool::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t total_freed = 0;
    for (auto& pool : m_pools) {
        for (uint8_t* buffer : pool.second.available_buffers) {
            delete[] buffer;
            total_freed += pool.second.buffer_size;
        }
        pool.second.available_buffers.clear();
    }
    
    m_current_pool_size = 0;
    
    Debug::log("memory", "BufferPool::clear() - Freed ", total_freed, " bytes from pool");
}

void IOBufferPool::setMaxPoolSize(size_t max_bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_max_pool_size = max_bytes;
    
    Debug::log("memory", "BufferPool::setMaxPoolSize() - Set max pool size to ", max_bytes, " bytes");
    
    // Evict if current size exceeds new limit
    if (m_current_pool_size > m_max_pool_size) {
        evictIfNeededInternal();
    }
}

void IOBufferPool::setMaxBuffersPerSize(size_t max_buffers) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_max_buffers_per_size = max_buffers;
    
    Debug::log("memory", "BufferPool::setMaxBuffersPerSize() - Set max buffers per size to ", max_buffers);
    
    // Trim existing pools if they exceed new limit
    for (auto& pool : m_pools) {
        while (pool.second.available_buffers.size() > m_max_buffers_per_size) {
            uint8_t* buffer = pool.second.available_buffers.back();
            pool.second.available_buffers.pop_back();
            delete[] buffer;
            m_current_pool_size -= pool.second.buffer_size;
        }
    }
}

size_t IOBufferPool::roundToPoolSize(size_t size) const {
    // Round up to next power of 2 for sizes up to 1MB
    if (size <= 1024 * 1024) {
        size_t rounded = 1;
        while (rounded < size) {
            rounded <<= 1;
        }
        return rounded;
    }
    
    // For larger sizes, round to 64KB boundaries
    const size_t alignment = 64 * 1024;
    return ((size + alignment - 1) / alignment) * alignment;
}

bool IOBufferPool::shouldPool(size_t size) const {
    // Pool buffers between 1KB and 1MB
    static const size_t MIN_POOL_SIZE = 1024;        // 1KB
    static const size_t MAX_POOL_SIZE = 1024 * 1024; // 1MB
    
    return size >= MIN_POOL_SIZE && size <= MAX_POOL_SIZE;
}

void IOBufferPool::evictIfNeeded() {
    std::lock_guard<std::mutex> lock(m_mutex);
    evictIfNeededInternal();
}

void IOBufferPool::evictIfNeededInternal() {
    // This method assumes the mutex is already locked
    
    if (m_current_pool_size <= m_effective_max_pool_size) {
        return; // No eviction needed
    }
    
    Debug::log("memory", "BufferPool::evictIfNeeded() - Evicting buffers, current size: ", m_current_pool_size);
    
    // Enhanced eviction strategy based on memory pressure and usage patterns
    std::vector<std::pair<size_t, PoolEntry*>> pool_entries; // (buffer_size, pool_entry)
    for (auto& pool_pair : m_pools) {
        if (!pool_pair.second.available_buffers.empty()) {
            pool_entries.emplace_back(pool_pair.first, &pool_pair.second);
        }
    }
    
    // Sort based on eviction strategy
    switch (m_memory_pressure_level) {
        case MemoryPressureLevel::Critical:
            // In critical pressure, evict everything except the most frequently used sizes
            std::sort(pool_entries.begin(), pool_entries.end(), 
                [](const auto& a, const auto& b) {
                    // Sort by hit rate (lowest first)
                    double a_hit_rate = a.second->getHitRate();
                    double b_hit_rate = b.second->getHitRate();
                    return a_hit_rate < b_hit_rate;
                });
            break;
            
        case MemoryPressureLevel::High:
            // In high pressure, prioritize evicting larger buffers and less used ones
            std::sort(pool_entries.begin(), pool_entries.end(), 
                [](const auto& a, const auto& b) {
                    // Sort by size (largest first) and then by hit rate (lowest first)
                    if (a.first > b.first + 32768) return true;  // Significantly larger
                    if (b.first > a.first + 32768) return false; // Significantly larger
                    return a.second->getHitRate() < b.second->getHitRate();
                });
            break;
            
        case MemoryPressureLevel::Normal:
        default:
            // In normal pressure, just evict largest buffers first
            std::sort(pool_entries.begin(), pool_entries.end(), 
                [](const auto& a, const auto& b) {
                    return a.first > b.first; // Sort by size (largest first)
                });
            break;
    }
    
    // Determine how aggressive to be with eviction based on memory pressure
    float eviction_factor = 0.5f; // Default: remove half the buffers
    if (m_memory_pressure_level == MemoryPressureLevel::High) {
        eviction_factor = 0.75f; // High: remove 75% of buffers
    } else if (m_memory_pressure_level == MemoryPressureLevel::Critical) {
        eviction_factor = 0.9f;  // Critical: remove 90% of buffers
    }
    
    size_t evicted_bytes = 0;
    for (const auto& pool_entry : pool_entries) {
        if (m_current_pool_size <= m_effective_max_pool_size) {
            break; // Evicted enough
        }
        
        PoolEntry* entry = pool_entry.second;
        if (!entry->available_buffers.empty()) {
            // Calculate how many buffers to remove
            size_t to_remove = std::max(static_cast<size_t>(1), 
                                      static_cast<size_t>(entry->available_buffers.size() * eviction_factor));
            
            // Keep at least one buffer for common sizes if possible
            if (m_memory_pressure_level != MemoryPressureLevel::Critical && 
                isCommonSize(entry->buffer_size) && 
                entry->available_buffers.size() > 1) {
                to_remove = std::min(to_remove, entry->available_buffers.size() - 1);
            }
            
            // Remove buffers
            for (size_t i = 0; i < to_remove && !entry->available_buffers.empty(); ++i) {
                uint8_t* buffer = entry->available_buffers.back();
                entry->available_buffers.pop_back();
                delete[] buffer;
                m_current_pool_size -= entry->buffer_size;
                evicted_bytes += entry->buffer_size;
            }
        }
    }
    
    Debug::log("memory", "BufferPool::evictIfNeeded() - Evicted ", evicted_bytes, 
              " bytes, new size: ", m_current_pool_size, 
              " (pressure level: ", memoryPressureLevelToString(m_memory_pressure_level), ")");
}

// New memory management methods

void IOBufferPool::preAllocateCommonBuffers() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Only pre-allocate if we're not under memory pressure
    if (m_memory_pressure_level != MemoryPressureLevel::Normal) {
        Debug::log("memory", "BufferPool::preAllocateCommonBuffers() - Skipping pre-allocation due to memory pressure");
        return;
    }
    
    size_t total_pre_allocated = 0;
    
    // Pre-allocate buffers for common sizes
    for (size_t size : m_common_sizes) {
        // Skip if we would exceed the pool size
        if (m_current_pool_size + size > m_max_pool_size) {
            continue;
        }
        
        // Create pool entry if it doesn't exist
        auto it = m_pools.find(size);
        if (it == m_pools.end()) {
            it = m_pools.emplace(size, PoolEntry(size)).first;
        }
        
        // Pre-allocate a small number of buffers (fewer than max)
        size_t to_allocate = std::min(static_cast<size_t>(2), m_max_buffers_per_size / 2);
        
        // Don't allocate if we already have enough
        if (it->second.available_buffers.size() >= to_allocate) {
            continue;
        }
        
        // Allocate additional buffers
        size_t allocated = 0;
        for (size_t i = it->second.available_buffers.size(); i < to_allocate; ++i) {
            try {
                uint8_t* buffer = new uint8_t[size];
                it->second.available_buffers.push_back(buffer);
                m_current_pool_size += size;
                total_pre_allocated += size;
                allocated++;
            } catch (const std::bad_alloc& e) {
                Debug::log("memory", "BufferPool::preAllocateCommonBuffers() - Allocation failed: ", e.what());
                break;
            }
        }
        
        if (allocated > 0) {
            Debug::log("memory", "BufferPool::preAllocateCommonBuffers() - Pre-allocated ", 
                      allocated, " buffers of size ", size);
        }
    }
    
    if (total_pre_allocated > 0) {
        Debug::log("memory", "BufferPool::preAllocateCommonBuffers() - Total pre-allocated: ", 
                  total_pre_allocated, " bytes");
    }
}

bool IOBufferPool::isCommonSize(size_t size) const {
    return std::find(m_common_sizes.begin(), m_common_sizes.end(), size) != m_common_sizes.end();
}

void IOBufferPool::startMemoryMonitoring() {
    // Start memory pressure monitoring thread
    m_monitoring_active = true;
    m_monitoring_thread = std::thread([this]() {
        monitorMemoryPressure();
    });
}

void IOBufferPool::stopMemoryMonitoring() {
    // Stop memory pressure monitoring thread
    m_monitoring_active = false;
    if (m_monitoring_thread.joinable()) {
        m_monitoring_thread.join();
    }
}

void IOBufferPool::monitorMemoryPressure() {
    using namespace std::chrono_literals;
    
    Debug::log("memory", "BufferPool::monitorMemoryPressure() - Starting memory pressure monitoring");
    
    while (m_monitoring_active) {
        // Sleep for monitoring interval
        std::this_thread::sleep_for(5s);
        
        // Check system memory pressure
        MemoryPressureLevel new_pressure = detectMemoryPressure();
        
        // Update memory pressure level if changed
        if (new_pressure != m_memory_pressure_level) {
            std::lock_guard<std::mutex> lock(m_mutex);
            
            Debug::log("memory", "BufferPool::monitorMemoryPressure() - Memory pressure changed from ",
                      memoryPressureLevelToString(m_memory_pressure_level), " to ",
                      memoryPressureLevelToString(new_pressure));
            
            m_memory_pressure_level = new_pressure;
            
            // Adjust pool parameters based on memory pressure
            adjustPoolParametersForMemoryPressure();
            
            // Evict buffers if needed
            if (new_pressure > MemoryPressureLevel::Normal) {
                evictIfNeeded();
            }
            
            // Pre-allocate common buffers if pressure decreased
            if (new_pressure == MemoryPressureLevel::Normal) {
                preAllocateCommonBuffers();
            }
        }
    }
    
    Debug::log("memory", "BufferPool::monitorMemoryPressure() - Stopping memory pressure monitoring");
}

IOBufferPool::MemoryPressureLevel IOBufferPool::detectMemoryPressure() {
    // Get memory usage statistics from IOHandler
    std::map<std::string, size_t> memory_stats = IOHandler::getMemoryStats();
    
    // Check if we have memory stats
    if (memory_stats.empty()) {
        return MemoryPressureLevel::Normal;
    }
    
    // Get total memory usage and limits
    size_t total_usage = memory_stats["total_memory_usage"];
    size_t max_memory = memory_stats["max_total_memory"];
    
    if (max_memory == 0) {
        return MemoryPressureLevel::Normal;
    }
    
    // Calculate memory usage percentage
    float usage_percent = static_cast<float>(total_usage) / static_cast<float>(max_memory) * 100.0f;
    
    // Determine memory pressure level based on usage percentage
    if (usage_percent > 90.0f) {
        return MemoryPressureLevel::Critical;
    } else if (usage_percent > 75.0f) {
        return MemoryPressureLevel::High;
    } else {
        return MemoryPressureLevel::Normal;
    }
}

void IOBufferPool::adjustPoolParametersForMemoryPressure() {
    // This method assumes the mutex is already locked
    
    switch (m_memory_pressure_level) {
        case MemoryPressureLevel::Critical:
            // In critical pressure, drastically reduce pool size
            m_effective_max_pool_size = m_max_pool_size / 4;
            m_effective_max_buffers_per_size = m_max_buffers_per_size / 4;
            break;
            
        case MemoryPressureLevel::High:
            // In high pressure, reduce pool size
            m_effective_max_pool_size = m_max_pool_size / 2;
            m_effective_max_buffers_per_size = m_max_buffers_per_size / 2;
            break;
            
        case MemoryPressureLevel::Normal:
        default:
            // In normal pressure, use full pool size
            m_effective_max_pool_size = m_max_pool_size;
            m_effective_max_buffers_per_size = m_max_buffers_per_size;
            break;
    }
    
    Debug::log("memory", "BufferPool::adjustPoolParametersForMemoryPressure() - Adjusted pool parameters: ",
              "max_pool_size=", m_effective_max_pool_size, ", max_buffers_per_size=", m_effective_max_buffers_per_size);
}

std::string IOBufferPool::memoryPressureLevelToString(MemoryPressureLevel level) {
    switch (level) {
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

double IOBufferPool::PoolEntry::getHitRate() const {
    size_t total = pool_hits + pool_misses;
    if (total == 0) {
        return 0.0;
    }
    return static_cast<double>(pool_hits) / static_cast<double>(total);
}

// Additional memory management optimization methods

void IOBufferPool::optimizeAllocationPatterns() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Debug::log("memory", "BufferPool::optimizeAllocationPatterns() - Analyzing allocation patterns");
    
    // Analyze usage patterns and optimize pool configuration
    std::vector<std::pair<size_t, double>> size_efficiency; // (size, efficiency_score)
    
    for (const auto& pool_pair : m_pools) {
        const PoolEntry& entry = pool_pair.second;
        size_t buffer_size = pool_pair.first;
        
        // Calculate efficiency score based on hit rate and memory usage
        double hit_rate = entry.getHitRate();
        size_t memory_used = entry.available_buffers.size() * buffer_size;
        
        // Efficiency score: hit rate weighted by memory efficiency
        double efficiency_score = hit_rate;
        if (memory_used > 0) {
            // Penalize sizes that use a lot of memory with low hit rates
            efficiency_score *= (1.0 - static_cast<double>(memory_used) / m_current_pool_size);
        }
        
        size_efficiency.emplace_back(buffer_size, efficiency_score);
        
        Debug::log("memory", "BufferPool::optimizeAllocationPatterns() - Size ", buffer_size, 
                  ": hit_rate=", hit_rate, ", memory_used=", memory_used, ", efficiency=", efficiency_score);
    }
    
    // Sort by efficiency (lowest first for potential eviction)
    std::sort(size_efficiency.begin(), size_efficiency.end(),
        [](const auto& a, const auto& b) {
            return a.second < b.second;
        });
    
    // Evict inefficient buffer sizes if memory pressure is high
    if (m_memory_pressure_level >= MemoryPressureLevel::High) {
        size_t sizes_to_evict = size_efficiency.size() / 4; // Evict bottom 25%
        
        for (size_t i = 0; i < sizes_to_evict && i < size_efficiency.size(); ++i) {
            size_t buffer_size = size_efficiency[i].first;
            double efficiency = size_efficiency[i].second;
            
            // Only evict if efficiency is very low
            if (efficiency < 0.3) {
                auto it = m_pools.find(buffer_size);
                if (it != m_pools.end() && !it->second.available_buffers.empty()) {
                    // Evict half the buffers for this size
                    size_t to_evict = it->second.available_buffers.size() / 2;
                    
                    for (size_t j = 0; j < to_evict; ++j) {
                        uint8_t* buffer = it->second.available_buffers.back();
                        it->second.available_buffers.pop_back();
                        delete[] buffer;
                        m_current_pool_size -= buffer_size;
                    }
                    
                    Debug::log("memory", "BufferPool::optimizeAllocationPatterns() - Evicted ", 
                              to_evict, " buffers of size ", buffer_size, " (low efficiency: ", efficiency, ")");
                }
            }
        }
    }
}

void IOBufferPool::compactMemory() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Debug::log("memory", "BufferPool::compactMemory() - Starting memory compaction");
    
    size_t initial_pool_size = m_current_pool_size;
    size_t freed_bytes = 0;
    
    // Remove empty pool entries
    auto it = m_pools.begin();
    while (it != m_pools.end()) {
        if (it->second.available_buffers.empty() && 
            it->second.total_allocated == 0) {
            Debug::log("memory", "BufferPool::compactMemory() - Removing empty pool entry for size ", it->first);
            it = m_pools.erase(it);
        } else {
            ++it;
        }
    }
    
    // Consolidate fragmented allocations by temporarily reducing pool diversity
    if (m_pools.size() > 8) { // If we have too many different sizes
        // Find the least used sizes and consolidate them
        std::vector<std::pair<size_t, size_t>> size_usage; // (size, total_requests)
        
        for (const auto& pool_pair : m_pools) {
            size_t total_requests = pool_pair.second.pool_hits + pool_pair.second.pool_misses;
            size_usage.emplace_back(pool_pair.first, total_requests);
        }
        
        // Sort by usage (lowest first)
        std::sort(size_usage.begin(), size_usage.end(),
            [](const auto& a, const auto& b) {
                return a.second < b.second;
            });
        
        // Remove buffers from least used sizes
        size_t sizes_to_reduce = size_usage.size() / 3; // Reduce bottom third
        for (size_t i = 0; i < sizes_to_reduce && i < size_usage.size(); ++i) {
            size_t buffer_size = size_usage[i].first;
            size_t usage = size_usage[i].second;
            
            // Only reduce if usage is very low
            if (usage < 5) {
                auto pool_it = m_pools.find(buffer_size);
                if (pool_it != m_pools.end() && !pool_it->second.available_buffers.empty()) {
                    // Keep only one buffer for very low usage sizes
                    while (pool_it->second.available_buffers.size() > 1) {
                        uint8_t* buffer = pool_it->second.available_buffers.back();
                        pool_it->second.available_buffers.pop_back();
                        delete[] buffer;
                        m_current_pool_size -= buffer_size;
                        freed_bytes += buffer_size;
                    }
                    
                    Debug::log("memory", "BufferPool::compactMemory() - Reduced pool for size ", 
                              buffer_size, " (low usage: ", usage, " requests)");
                }
            }
        }
    }
    
    Debug::log("memory", "BufferPool::compactMemory() - Compaction complete: freed ", freed_bytes, 
              " bytes (", initial_pool_size, " -> ", m_current_pool_size, ")");
}

void IOBufferPool::defragmentPools() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Debug::log("memory", "BufferPool::defragmentPools() - Starting pool defragmentation");
    
    // Analyze fragmentation by looking at size distribution
    std::map<size_t, size_t> size_categories; // (category_size, total_buffers)
    
    // Group similar sizes into categories
    for (const auto& pool_pair : m_pools) {
        size_t buffer_size = pool_pair.first;
        size_t buffer_count = pool_pair.second.available_buffers.size();
        
        // Find the nearest power-of-2 category
        size_t category = 1;
        while (category < buffer_size) {
            category <<= 1;
        }
        
        size_categories[category] += buffer_count;
    }
    
    // Look for opportunities to consolidate similar sizes
    for (const auto& category_pair : size_categories) {
        size_t category_size = category_pair.first;
        size_t total_buffers = category_pair.second;
        
        if (total_buffers < 3) {
            continue; // Not worth consolidating
        }
        
        // Find all pool sizes in this category
        std::vector<size_t> sizes_in_category;
        for (const auto& pool_pair : m_pools) {
            size_t buffer_size = pool_pair.first;
            
            // Check if this size belongs to the current category
            size_t size_category = 1;
            while (size_category < buffer_size) {
                size_category <<= 1;
            }
            
            if (size_category == category_size && 
                !pool_pair.second.available_buffers.empty()) {
                sizes_in_category.push_back(buffer_size);
            }
        }
        
        // If we have multiple sizes in the same category, consider consolidation
        if (sizes_in_category.size() > 2) {
            Debug::log("memory", "BufferPool::defragmentPools() - Found ", sizes_in_category.size(), 
                      " sizes in category ", category_size, " - considering consolidation");
            
            // For now, just log the opportunity - actual consolidation would require
            // more sophisticated logic to avoid breaking existing allocations
        }
    }
    
    Debug::log("memory", "BufferPool::defragmentPools() - Defragmentation analysis complete");
}