/*
 * BufferPool.cpp - Memory pool for efficient buffer allocation
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace IO {

// Buffer implementation
void IOBufferPool::Buffer::release() {
    if (m_data) {
        if (m_entry) {
            m_entry->returnBuffer(m_data);
        } else {
             // Direct allocation, delete
             delete[] m_data;
             Debug::log("memory", "BufferPool::release() - Direct deallocation of size ", m_size);
        }
        m_data = nullptr;
        m_size = 0;
        m_entry.reset();
    }
}

// PoolEntry implementation
void IOBufferPool::PoolEntry::returnBuffer(uint8_t* data) {
    if (!data) return;

    std::lock_guard<std::mutex> lock(mutex);

    // Check limits
    // Use atomic loads for thread safety
    size_t current_size = pool_instance->m_current_pool_size.load(std::memory_order_relaxed);
    size_t max_pool = pool_instance->m_effective_max_pool_size.load(std::memory_order_relaxed);
    size_t max_bufs = pool_instance->m_effective_max_buffers_per_size.load(std::memory_order_relaxed);

    if (available_buffers.size() >= max_bufs ||
        current_size + buffer_size > max_pool) {

        delete[] data;
        Debug::log("memory", "BufferPool::release() - Pool full for size ", buffer_size, ", deleting");
        return;
    }

    available_buffers.push_back(data);
    pool_instance->m_current_pool_size.fetch_add(buffer_size, std::memory_order_relaxed);

    Debug::log("memory", "BufferPool::release() - Returned buffer of size ", buffer_size,
              " to pool (pool size: ", available_buffers.size(),
              ", total pooled: ", pool_instance->m_current_pool_size.load(std::memory_order_relaxed), ")");
}

double IOBufferPool::PoolEntry::getHitRate() const {
    size_t total = pool_hits + pool_misses;
    if (total == 0) {
        return 0.0;
    }
    return static_cast<double>(pool_hits) / static_cast<double>(total);
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
            return Buffer(data, size, nullptr); // nullptr entry means direct allocation
        } catch (const std::bad_alloc& e) {
            Debug::log("memory", "BufferPool::acquire() - Direct allocation failed: ", e.what());
            return Buffer();
        }
    }
    
    size_t pool_size = roundToPoolSize(size);
    
    // Thread-local cache to avoid map lookup and shared_mutex contention
    static thread_local std::shared_ptr<PoolEntry> t_cached_entry;
    static thread_local size_t t_cached_size = 0;

    std::shared_ptr<PoolEntry> entry;

    // Check cache
    if (t_cached_size == pool_size) {
        entry = t_cached_entry;
    }
    
    if (!entry) {
        // Slow path: lookup in map with shared lock
        std::shared_lock<std::shared_mutex> read_lock(m_pools_mutex);
        
        auto it = m_pools.find(pool_size);
        if (it != m_pools.end()) {
            entry = it->second;
            // Update cache
            t_cached_entry = entry;
            t_cached_size = pool_size;
        }
    }

    if (entry) {
        // We have the entry, now acquire buffer
        std::lock_guard<std::mutex> entry_lock(entry->mutex);

        if (!entry->available_buffers.empty()) {
            // Reuse existing buffer
            uint8_t* data = entry->available_buffers.back();
            entry->available_buffers.pop_back();
            entry->pool_hits++;
            m_current_pool_size.fetch_sub(pool_size, std::memory_order_relaxed);

            Debug::log("memory", "BufferPool::acquire() - Reused buffer of size ", pool_size, " (pool hits: ", entry->pool_hits, ")");
            return Buffer(data, pool_size, entry);
        }
    }
    
    // Need to allocate new buffer
    try {
        uint8_t* data = new uint8_t[pool_size];
        
        if (!entry) {
            // Entry did not exist. Create it.
            // Needs exclusive lock on map
            std::unique_lock<std::shared_mutex> write_lock(m_pools_mutex);
            // Double check
            auto it = m_pools.find(pool_size);
            if (it == m_pools.end()) {
                entry = std::make_shared<PoolEntry>(pool_size, this);
                m_pools.emplace(pool_size, entry);
            } else {
                entry = it->second;
            }

            // Update cache
            t_cached_entry = entry;
            t_cached_size = pool_size;
        }
        
        // Update stats
        {
            std::lock_guard<std::mutex> entry_lock(entry->mutex);
            entry->total_allocated++;
            entry->pool_misses++;
        }

        Debug::log("memory", "BufferPool::acquire() - Allocated new buffer of size ", pool_size,
                  " (total allocated: ", entry->total_allocated, ", pool misses: ", entry->pool_misses, ")");

        return Buffer(data, pool_size, entry);
        
    } catch (const std::bad_alloc& e) {
        Debug::log("memory", "BufferPool::acquire() - Allocation failed for size ", pool_size, ": ", e.what());
        
        evictIfNeeded(); 
        enforceBoundedLimits(); 
        
        try {
            uint8_t* data = new uint8_t[pool_size];
            if (entry) {
                return Buffer(data, pool_size, entry);
            }
            return Buffer(data, pool_size, nullptr);
        } catch (const std::bad_alloc& e2) {
            Debug::log("memory", "BufferPool::acquire() - Allocation still failed: ", e2.what());
            return Buffer();
        }
    }
}

void IOBufferPool::release(uint8_t* data, size_t size) {
    // Legacy/Raw release method
    if (!data) return;
    
    if (!shouldPool(size)) {
        delete[] data;
        Debug::log("memory", "BufferPool::release() - Direct deallocation of size ", size);
        return;
    }
    
    size_t pool_size = roundToPoolSize(size);
    std::shared_ptr<PoolEntry> entry;
    
    // Try to find pool
    {
        std::shared_lock<std::shared_mutex> read_lock(m_pools_mutex);
        auto it = m_pools.find(pool_size);
        if (it != m_pools.end()) {
            entry = it->second;
        }
    }
    
    if (entry) {
        entry->returnBuffer(data);
    } else {
        delete[] data;
        Debug::log("memory", "BufferPool::release() - Pool doesn't exist for size ", pool_size, ", deleting");
    }
}

std::map<std::string, size_t> IOBufferPool::getStats() const {
    std::shared_lock<std::shared_mutex> read_lock(m_pools_mutex);
    
    std::map<std::string, size_t> stats;
    stats["pool_count"] = m_pools.size();
    stats["current_pool_size"] = m_current_pool_size.load();
    stats["max_pool_size"] = m_max_pool_size.load();
    stats["max_buffers_per_size"] = m_max_buffers_per_size.load();
    
    size_t total_allocated = 0;
    size_t total_pool_hits = 0;
    size_t total_pool_misses = 0;
    size_t total_pooled_buffers = 0;
    
    for (const auto& pool : m_pools) {
        std::lock_guard<std::mutex> entry_lock(pool.second->mutex);
        total_allocated += pool.second->total_allocated;
        total_pool_hits += pool.second->pool_hits;
        total_pool_misses += pool.second->pool_misses;
        total_pooled_buffers += pool.second->available_buffers.size();
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
    std::unique_lock<std::shared_mutex> write_lock(m_pools_mutex);
    
    size_t total_freed = 0;
    for (auto& pool : m_pools) {
        std::lock_guard<std::mutex> entry_lock(pool.second->mutex);
        for (uint8_t* buffer : pool.second->available_buffers) {
            delete[] buffer;
            total_freed += pool.second->buffer_size;
        }
        pool.second->available_buffers.clear();
    }
    
    m_current_pool_size = 0;
    
    Debug::log("memory", "BufferPool::clear() - Freed ", total_freed, " bytes from pool");
}

void IOBufferPool::setMaxPoolSize(size_t max_bytes) {
    std::unique_lock<std::shared_mutex> write_lock(m_pools_mutex);
    m_max_pool_size = max_bytes;
    
    Debug::log("memory", "BufferPool::setMaxPoolSize() - Set max pool size to ", max_bytes, " bytes");
    
    // Evict if current size exceeds new limit
    if (m_current_pool_size > m_max_pool_size) {
        evictIfNeededInternal();
    }
    
    // Enforce bounded limits to prevent memory leaks (lock already held)
    enforceBoundedLimitsInternal();
}

void IOBufferPool::setMaxBuffersPerSize(size_t max_buffers) {
    std::unique_lock<std::shared_mutex> write_lock(m_pools_mutex);
    m_max_buffers_per_size = max_buffers;
    
    Debug::log("memory", "BufferPool::setMaxBuffersPerSize() - Set max buffers per size to ", max_buffers);
    
    // Trim existing pools if they exceed new limit
    for (auto& pool : m_pools) {
        std::lock_guard<std::mutex> entry_lock(pool.second->mutex);
        while (pool.second->available_buffers.size() > m_max_buffers_per_size) {
            uint8_t* buffer = pool.second->available_buffers.back();
            pool.second->available_buffers.pop_back();
            delete[] buffer;
            m_current_pool_size -= pool.second->buffer_size; // atomic sub
        }
    }
}

size_t IOBufferPool::roundToPoolSize(size_t size) const {
    // Round up to next power of 2 for sizes up to 1MB
    if (size <= 1024 * 1024) {
        // Optimization: Use __builtin_clz for O(1) calculation instead of loop
        if (size <= 1) {
            return 1;
        }

        // Since size <= 1MB, it fits in uint32_t (20 bits)
        // __builtin_clz counts leading zeros in unsigned int (typically 32 bits)
        // We calculate next power of 2 by shifting 1 by (32 - leading_zeros(size - 1))
        // This is significantly faster (5x-8x) than the loop
        uint32_t v = static_cast<uint32_t>(size - 1);
        return 1U << (32 - __builtin_clz(v));
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
    std::unique_lock<std::shared_mutex> write_lock(m_pools_mutex);
    evictIfNeededInternal();
}

void IOBufferPool::evictIfNeededInternal() {
    // This method assumes the m_pools_mutex (write) is already locked
    
    if (m_current_pool_size <= m_effective_max_pool_size) {
        return; // No eviction needed
    }
    
    Debug::log("memory", "BufferPool::evictIfNeeded() - Evicting buffers, current size: ", m_current_pool_size.load());
    
    // Enhanced eviction strategy based on memory pressure and usage patterns
    std::vector<std::pair<size_t, PoolEntry*>> pool_entries; // (buffer_size, pool_entry)
    for (auto& pool_pair : m_pools) {
        pool_entries.emplace_back(pool_pair.first, pool_pair.second.get());
    }
    
    // Sort based on eviction strategy
    // Note: this calls getHitRate which reads atomic-ish counters (not atomic, but protected by mutex)
    // We are not holding entry mutexes during sort. This is a race if other threads update stats.
    // But stats are just for heuristics. It is acceptable.

    switch (m_memory_pressure_level.load()) {
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
    MemoryPressureLevel level = m_memory_pressure_level.load();
    if (level == MemoryPressureLevel::High) {
        eviction_factor = 0.75f; // High: remove 75% of buffers
    } else if (level == MemoryPressureLevel::Critical) {
        eviction_factor = 0.9f;  // Critical: remove 90% of buffers
    }
    
    size_t evicted_bytes = 0;
    for (const auto& pool_entry : pool_entries) {
        if (m_current_pool_size <= m_effective_max_pool_size) {
            break; // Evicted enough
        }
        
        PoolEntry* entry = pool_entry.second;

        // Lock entry to modify it
        std::lock_guard<std::mutex> entry_lock(entry->mutex);

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
                m_current_pool_size.fetch_sub(entry->buffer_size, std::memory_order_relaxed);
                evicted_bytes += entry->buffer_size;
            }
        }
    }
    
    Debug::log("memory", "BufferPool::evictIfNeeded() - Evicted ", evicted_bytes, 
              " bytes, new size: ", m_current_pool_size.load(),
              " (pressure level: ", memoryPressureLevelToString(m_memory_pressure_level.load()), ")");
}

// New memory management methods

void IOBufferPool::preAllocateCommonBuffers() {
    std::unique_lock<std::shared_mutex> write_lock(m_pools_mutex);
    
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
        std::shared_ptr<PoolEntry> entry;
        auto it = m_pools.find(size);
        if (it == m_pools.end()) {
            entry = std::make_shared<PoolEntry>(size, this);
            m_pools.emplace(size, entry);
        } else {
            entry = it->second;
        }
        
        // Lock entry
        std::lock_guard<std::mutex> entry_lock(entry->mutex);

        // Pre-allocate a small number of buffers (fewer than max)
        size_t to_allocate = std::min(static_cast<size_t>(2), static_cast<size_t>(m_max_buffers_per_size / 2));
        
        // Don't allocate if we already have enough
        if (entry->available_buffers.size() >= to_allocate) {
            continue;
        }
        
        // Allocate additional buffers
        size_t allocated = 0;
        for (size_t i = entry->available_buffers.size(); i < to_allocate; ++i) {
            try {
                uint8_t* buffer = new uint8_t[size];
                entry->available_buffers.push_back(buffer);
                m_current_pool_size.fetch_add(size, std::memory_order_relaxed);
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
            // No lock needed for simple atomic assignment
            
            Debug::log("memory", "BufferPool::monitorMemoryPressure() - Memory pressure changed from ",
                      memoryPressureLevelToString(m_memory_pressure_level), " to ",
                      memoryPressureLevelToString(new_pressure));
            
            m_memory_pressure_level = new_pressure;
            
            // Adjust pool parameters based on memory pressure
            adjustPoolParametersForMemoryPressure();
            
            // Evict buffers if needed
            if (new_pressure > MemoryPressureLevel::Normal) {
                evictIfNeeded();
                enforceBoundedLimits();
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
    // Using atomics, so no lock strictly required for safety,
    // but these updates should ideally be consistent.
    // However, they are read independently.
    
    switch (m_memory_pressure_level.load()) {
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
            m_effective_max_pool_size = m_max_pool_size.load();
            m_effective_max_buffers_per_size = m_max_buffers_per_size.load();
            break;
    }
    
    Debug::log("memory", "BufferPool::adjustPoolParametersForMemoryPressure() - Adjusted pool parameters: ",
              "max_pool_size=", m_effective_max_pool_size.load(), ", max_buffers_per_size=", m_effective_max_buffers_per_size.load());
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

// Additional memory management optimization methods

void IOBufferPool::optimizeAllocationPatterns() {
    std::unique_lock<std::shared_mutex> write_lock(m_pools_mutex);
    
    Debug::log("memory", "BufferPool::optimizeAllocationPatterns() - Analyzing allocation patterns");
    
    // Analyze usage patterns and optimize pool configuration
    std::vector<std::pair<size_t, double>> size_efficiency; // (size, efficiency_score)
    
    for (const auto& pool_pair : m_pools) {
        const PoolEntry& entry = *pool_pair.second;
        size_t buffer_size = pool_pair.first;
        
        // Calculate efficiency score based on hit rate and memory usage
        double hit_rate = entry.getHitRate();
        // Need to lock entry for size?
        // We hold write lock on map, but entries might be accessed via shared_ptrs.
        // Yes, need to lock.
        size_t memory_used = 0;
        {
            std::lock_guard<std::mutex> entry_lock(pool_pair.second->mutex);
            memory_used = pool_pair.second->available_buffers.size() * buffer_size;
        }
        
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
                if (it != m_pools.end()) {
                    // Evict half the buffers for this size
                    std::lock_guard<std::mutex> entry_lock(it->second->mutex);
                    if (!it->second->available_buffers.empty()) {
                        size_t to_evict = it->second->available_buffers.size() / 2;

                        for (size_t j = 0; j < to_evict; ++j) {
                            uint8_t* buffer = it->second->available_buffers.back();
                            it->second->available_buffers.pop_back();
                            delete[] buffer;
                            m_current_pool_size.fetch_sub(buffer_size, std::memory_order_relaxed);
                        }

                        Debug::log("memory", "BufferPool::optimizeAllocationPatterns() - Evicted ",
                                  to_evict, " buffers of size ", buffer_size, " (low efficiency: ", efficiency, ")");
                    }
                }
            }
        }
    }
}

void IOBufferPool::compactMemory() {
    std::unique_lock<std::shared_mutex> write_lock(m_pools_mutex);
    
    Debug::log("memory", "BufferPool::compactMemory() - Starting memory compaction");
    
    size_t initial_pool_size = m_current_pool_size;
    size_t freed_bytes = 0;
    
    // Remove empty pool entries
    auto it = m_pools.begin();
    while (it != m_pools.end()) {
        std::lock_guard<std::mutex> entry_lock(it->second->mutex);
        if (it->second->available_buffers.empty() &&
            it->second->total_allocated == 0) {
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
            size_t total_requests = pool_pair.second->pool_hits + pool_pair.second->pool_misses;
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
                if (pool_it != m_pools.end()) {
                    std::lock_guard<std::mutex> entry_lock(pool_it->second->mutex);
                    if (!pool_it->second->available_buffers.empty()) {
                        // Keep only one buffer for very low usage sizes
                        while (pool_it->second->available_buffers.size() > 1) {
                            uint8_t* buffer = pool_it->second->available_buffers.back();
                            pool_it->second->available_buffers.pop_back();
                            delete[] buffer;
                            m_current_pool_size.fetch_sub(buffer_size, std::memory_order_relaxed);
                            freed_bytes += buffer_size;
                        }

                        Debug::log("memory", "BufferPool::compactMemory() - Reduced pool for size ",
                                  buffer_size, " (low usage: ", usage, " requests)");
                    }
                }
            }
        }
    }
    
    Debug::log("memory", "BufferPool::compactMemory() - Compaction complete: freed ", freed_bytes, 
              " bytes (", static_cast<size_t>(initial_pool_size), " -> ", m_current_pool_size.load(), ")");
}

void IOBufferPool::defragmentPools() {
    std::shared_lock<std::shared_mutex> read_lock(m_pools_mutex);
    
    Debug::log("memory", "BufferPool::defragmentPools() - Starting pool defragmentation");
    
    // Analyze fragmentation by looking at size distribution
    std::map<size_t, size_t> size_categories; // (category_size, total_buffers)
    
    // Group similar sizes into categories
    for (const auto& pool_pair : m_pools) {
        size_t buffer_size = pool_pair.first;
        size_t buffer_count = 0;
        {
            std::lock_guard<std::mutex> entry_lock(pool_pair.second->mutex);
            buffer_count = pool_pair.second->available_buffers.size();
        }
        
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
            
            bool has_buffers = false;
            {
                std::lock_guard<std::mutex> entry_lock(pool_pair.second->mutex);
                has_buffers = !pool_pair.second->available_buffers.empty();
            }

            if (size_category == category_size && has_buffers) {
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

void IOBufferPool::enforceBoundedLimits() {
    std::unique_lock<std::shared_mutex> write_lock(m_pools_mutex);
    enforceBoundedLimitsInternal();
}

void IOBufferPool::enforceBoundedLimitsInternal() {
    // Note: Caller must hold m_pools_mutex lock (exclusive)
    
    Debug::log("memory", "IOBufferPool::enforceBoundedLimits() - Enforcing bounded cache limits");
    
    // Calculate current memory usage percentage
    float usage_percent = getMemoryUsagePercent();
    
    Debug::log("memory", "IOBufferPool::enforceBoundedLimits() - Current usage: ", usage_percent, 
              "% (", m_current_pool_size.load(), " / ", m_effective_max_pool_size.load(), " bytes)");
    
    // Enforce strict limits based on usage percentage
    if (usage_percent > 100.0f) {
        // Over limit - emergency cleanup
        Debug::log("memory", "IOBufferPool::enforceBoundedLimits() - Over limit, performing emergency cleanup");
        
        // Clear all pools immediately
        size_t freed_bytes = 0;
        for (auto& pool : m_pools) {
        std::lock_guard<std::mutex> entry_lock(pool.second->mutex);
            for (uint8_t* buffer : pool.second->available_buffers) {
                delete[] buffer;
                freed_bytes += pool.second->buffer_size;
            }
            pool.second->available_buffers.clear();
        }
        
        m_current_pool_size = 0;
        
        Debug::log("memory", "IOBufferPool::enforceBoundedLimits() - Emergency cleanup freed ", freed_bytes, " bytes");
        
    } else if (usage_percent > 95.0f) {
        // Critical usage - aggressive eviction
        Debug::log("memory", "IOBufferPool::enforceBoundedLimits() - Critical usage, aggressive eviction");
        
        // Remove 90% of all buffers
        size_t freed_bytes = 0;
        for (auto& pool : m_pools) {
        std::lock_guard<std::mutex> entry_lock(pool.second->mutex);
            size_t to_remove = (pool.second->available_buffers.size() * 9) / 10; // 90%
            
            for (size_t i = 0; i < to_remove && !pool.second->available_buffers.empty(); ++i) {
                uint8_t* buffer = pool.second->available_buffers.back();
                pool.second->available_buffers.pop_back();
                delete[] buffer;
                m_current_pool_size.fetch_sub(pool.second->buffer_size, std::memory_order_relaxed);
                freed_bytes += pool.second->buffer_size;
            }
        }
        
        Debug::log("memory", "IOBufferPool::enforceBoundedLimits() - Aggressive eviction freed ", freed_bytes, " bytes");
        
    } else if (usage_percent > 90.0f) {
        // High usage - moderate eviction
        Debug::log("memory", "IOBufferPool::enforceBoundedLimits() - High usage, moderate eviction");
        
        // Remove 50% of all buffers
        size_t freed_bytes = 0;
        for (auto& pool : m_pools) {
        std::lock_guard<std::mutex> entry_lock(pool.second->mutex);
            size_t to_remove = pool.second->available_buffers.size() / 2; // 50%
            
            for (size_t i = 0; i < to_remove && !pool.second->available_buffers.empty(); ++i) {
                uint8_t* buffer = pool.second->available_buffers.back();
                pool.second->available_buffers.pop_back();
                delete[] buffer;
                m_current_pool_size.fetch_sub(pool.second->buffer_size, std::memory_order_relaxed);
                freed_bytes += pool.second->buffer_size;
            }
        }
        
        Debug::log("memory", "IOBufferPool::enforceBoundedLimits() - Moderate eviction freed ", freed_bytes, " bytes");
        
    } else if (usage_percent > 80.0f) {
        // Moderate usage - light eviction
        Debug::log("memory", "IOBufferPool::enforceBoundedLimits() - Moderate usage, light eviction");
        
        // Remove 25% of all buffers
        size_t freed_bytes = 0;
        for (auto& pool : m_pools) {
        std::lock_guard<std::mutex> entry_lock(pool.second->mutex);
            size_t to_remove = pool.second->available_buffers.size() / 4; // 25%
            
            for (size_t i = 0; i < to_remove && !pool.second->available_buffers.empty(); ++i) {
                uint8_t* buffer = pool.second->available_buffers.back();
                pool.second->available_buffers.pop_back();
                delete[] buffer;
                m_current_pool_size.fetch_sub(pool.second->buffer_size, std::memory_order_relaxed);
                freed_bytes += pool.second->buffer_size;
            }
        }
        
        Debug::log("memory", "IOBufferPool::enforceBoundedLimits() - Light eviction freed ", freed_bytes, " bytes");
    }
    
    // Enforce absolute maximum limits to prevent runaway memory usage
    const size_t ABSOLUTE_MAX_POOL_SIZE = 32 * 1024 * 1024; // 32MB absolute maximum
    
    if (m_current_pool_size > ABSOLUTE_MAX_POOL_SIZE) {
        Debug::log("memory", "IOBufferPool::enforceBoundedLimits() - Absolute limit exceeded, emergency cleanup");
        
        // Emergency cleanup - clear everything
        size_t freed_bytes = 0;
        for (auto& pool : m_pools) {
        std::lock_guard<std::mutex> entry_lock(pool.second->mutex);
            for (uint8_t* buffer : pool.second->available_buffers) {
                delete[] buffer;
                freed_bytes += pool.second->buffer_size;
            }
            pool.second->available_buffers.clear();
        }
        
        m_current_pool_size = 0;
        
        // Reset limits to more conservative values
        m_max_pool_size = std::min(m_max_pool_size.load(), static_cast<size_t>(8 * 1024 * 1024)); // 8MB
        m_effective_max_pool_size = m_max_pool_size.load();

        m_max_buffers_per_size = std::min(m_max_buffers_per_size.load(), static_cast<size_t>(2)); // 2 buffers max
        m_effective_max_buffers_per_size = m_max_buffers_per_size.load();
        
        Debug::log("memory", "IOBufferPool::enforceBoundedLimits() - Emergency cleanup freed ", freed_bytes, 
                  " bytes, reset limits to conservative values");
    }
    
    Debug::log("memory", "IOBufferPool::enforceBoundedLimits() - Bounded limits enforcement complete, final usage: ", 
              getMemoryUsagePercent(), "%");
}

float IOBufferPool::getMemoryUsagePercent() const {
    // This method assumes the mutex is already locked when called from other methods
    size_t effective_max = m_effective_max_pool_size.load();
    if (effective_max == 0) {
        return 0.0f;
    }
    
    return static_cast<float>(m_current_pool_size.load()) / static_cast<float>(effective_max) * 100.0f;
}

} // namespace IO
} // namespace PsyMP3
