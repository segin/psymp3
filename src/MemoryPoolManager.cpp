/*
 * MemoryPoolManager.cpp - Memory pool management for I/O operations
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

MemoryPoolManager& MemoryPoolManager::getInstance() {
    static MemoryPoolManager instance;
    static bool initialized = false;
    
    // Initialize memory tracking after construction to avoid circular dependency
    if (!initialized) {
        initialized = true;
        instance.initializeMemoryTracking();
    }
    
    return instance;
}

MemoryPoolManager::MemoryPoolManager() {
    Debug::log("memory", "MemoryPoolManager::MemoryPoolManager() - Initializing memory pool manager");
    
    // Configure default pools
    m_pool_configs = {
        { 4 * 1024, 16, "small_file" },     // 4KB - small file reads
        { 8 * 1024, 12, "http_range" },     // 8KB - minimum HTTP range
        { 16 * 1024, 10, "medium_file" },   // 16KB - medium file reads
        { 32 * 1024, 8, "large_file" },     // 32KB - large file reads
        { 64 * 1024, 6, "default" },        // 64KB - default buffer size
        { 128 * 1024, 4, "read_ahead" },    // 128KB - read-ahead size
        { 256 * 1024, 2, "large_http" },    // 256KB - large HTTP range
        { 512 * 1024, 1, "max_buffer" }     // 512KB - maximum buffer size
    };
}

MemoryPoolManager::~MemoryPoolManager() {
    Debug::log("memory", "MemoryPoolManager::~MemoryPoolManager() - Cleaning up memory pools");
    
    // Unregister memory tracker callback
    if (m_memory_tracker_callback_id != -1) {
        MemoryTracker::getInstance().unregisterMemoryPressureCallback(m_memory_tracker_callback_id);
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Free all pooled buffers
    for (auto& pool : m_pools) {
        for (uint8_t* buffer : pool.second.free_buffers) {
            delete[] buffer;
        }
        pool.second.free_buffers.clear();
    }
    
    m_pools.clear();
}

void MemoryPoolManager::initializePools() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Debug::log("memory", "MemoryPoolManager::initializePools() - Initializing memory pools");
    
    // Create pools based on configuration
    for (const auto& config : m_pool_configs) {
        // Pre-allocate 25% of max buffers for common sizes
        size_t pre_allocate = config.max_buffers / 4;
        createPool(config.buffer_size, config.max_buffers, pre_allocate);
    }
}

void MemoryPoolManager::initializeMemoryTracking() {
    Debug::log("memory", "MemoryPoolManager::initializeMemoryTracking() - Registering memory tracker callback");
    
    // Register with memory tracker for pressure updates
    m_memory_tracker_callback_id = MemoryTracker::getInstance().registerMemoryPressureCallback(
        [this](int pressure) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_memory_pressure_level = pressure;
                
                // If pressure is high, clean up pools
                if (pressure > 70) {
                    cleanupPools();
                }
            }
            
            // Notify our own callbacks without holding the mutex
            notifyPressureCallbacks();
        }
    );
}

uint8_t* MemoryPoolManager::allocateBuffer(size_t size, const std::string& component_name) {
    if (size == 0) {
        return nullptr;
    }
    
    // Check if allocation is safe
    if (!isSafeToAllocate(size, component_name)) {
        Debug::log("memory", "MemoryPoolManager::allocateBuffer() - Unsafe to allocate ", size, 
                  " bytes for ", component_name);
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Update component usage statistics
    auto& usage = m_component_usage[component_name];
    usage.allocations++;
    
    // Try to find a suitable pool
    auto pool_it = findBestPool(size);
    if (pool_it != m_pools.end()) {
        auto& pool = pool_it->second;
        
        // Check if we have a free buffer in the pool
        if (!pool.free_buffers.empty()) {
            uint8_t* buffer = pool.free_buffers.back();
            pool.free_buffers.pop_back();
            pool.hits++;
            
            // Update memory tracking
            usage.current_usage += pool.buffer_size;
            usage.peak_usage = std::max(usage.peak_usage, usage.current_usage);
            m_total_allocated += pool.buffer_size;
            
            Debug::log("memory", "MemoryPoolManager::allocateBuffer() - Allocated ", pool.buffer_size, 
                      " bytes from pool for ", component_name);
            
            return buffer;
        }
        
        // No free buffer, but we can create a new one if within limits
        if (pool.allocated_buffers < getMaxBuffersPerPool()) {
            try {
                uint8_t* buffer = new uint8_t[pool.buffer_size];
                pool.allocated_buffers++;
                pool.peak_usage = std::max(pool.peak_usage, pool.allocated_buffers);
                
                // Update memory tracking
                usage.current_usage += pool.buffer_size;
                usage.peak_usage = std::max(usage.peak_usage, usage.current_usage);
                m_total_allocated += pool.buffer_size;
                
                Debug::log("memory", "MemoryPoolManager::allocateBuffer() - Created new ", pool.buffer_size, 
                          " byte buffer for ", component_name);
                
                return buffer;
            } catch (const std::bad_alloc& e) {
                Debug::log("memory", "MemoryPoolManager::allocateBuffer() - Failed to allocate ", 
                          pool.buffer_size, " bytes: ", e.what());
                pool.misses++;
                // Fall through to direct allocation
            }
        } else {
            pool.misses++;
        }
    }
    
    // No suitable pool or pool allocation failed, try direct allocation
    try {
        uint8_t* buffer = new uint8_t[size];
        
        // Update memory tracking
        usage.current_usage += size;
        usage.peak_usage = std::max(usage.peak_usage, usage.current_usage);
        m_total_allocated += size;
        
        Debug::log("memory", "MemoryPoolManager::allocateBuffer() - Direct allocation of ", 
                  size, " bytes for ", component_name);
        
        return buffer;
    } catch (const std::bad_alloc& e) {
        Debug::log("memory", "MemoryPoolManager::allocateBuffer() - Failed to allocate ", 
                  size, " bytes: ", e.what());
        return nullptr;
    }
}

void MemoryPoolManager::releaseBuffer(uint8_t* buffer, size_t size, const std::string& component_name) {
    if (!buffer) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Update component usage statistics
    auto& usage = m_component_usage[component_name];
    usage.deallocations++;
    
    if (usage.current_usage >= size) {
        usage.current_usage -= size;
    } else {
        usage.current_usage = 0;
    }
    
    if (m_total_allocated >= size) {
        m_total_allocated -= size;
    } else {
        m_total_allocated = 0;
    }
    
    // Check if this size should be pooled
    if (shouldPool(size)) {
        // Find the appropriate pool
        auto pool_it = findBestPool(size);
        if (pool_it != m_pools.end()) {
            auto& pool = pool_it->second;
            
            // Only return to pool if we're not over capacity
            if (pool.free_buffers.size() < getMaxBuffersPerPool()) {
                pool.free_buffers.push_back(buffer);
                m_total_pooled += pool.buffer_size;
                
                Debug::log("memory", "MemoryPoolManager::releaseBuffer() - Returned ", pool.buffer_size, 
                          " byte buffer to pool from ", component_name);
                return;
            }
        }
    }
    
    // Not pooled, delete directly
    delete[] buffer;
    Debug::log("memory", "MemoryPoolManager::releaseBuffer() - Deleted ", size, 
              " byte buffer from ", component_name);
}

void MemoryPoolManager::setMemoryLimits(size_t max_total_memory, size_t max_buffer_memory) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_max_total_memory = max_total_memory;
    m_max_buffer_memory = max_buffer_memory;
    
    Debug::log("memory", "MemoryPoolManager::setMemoryLimits() - Set limits: total=", 
              max_total_memory, ", buffer=", max_buffer_memory);
    
    // Clean up pools if we're over the new limits
    if (m_total_pooled > m_max_buffer_memory || m_total_allocated > m_max_total_memory) {
        cleanupPools();
    }
}

std::map<std::string, size_t> MemoryPoolManager::getMemoryStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::map<std::string, size_t> stats;
    stats["total_allocated"] = m_total_allocated;
    stats["total_pooled"] = m_total_pooled;
    stats["max_total_memory"] = m_max_total_memory;
    stats["max_buffer_memory"] = m_max_buffer_memory;
    stats["memory_pressure"] = m_memory_pressure_level;
    stats["pool_count"] = m_pools.size();
    
    // Add pool-specific stats
    size_t pool_index = 0;
    for (const auto& pool : m_pools) {
        std::string prefix = "pool_" + std::to_string(pool_index) + "_";
        stats[prefix + "size"] = pool.second.buffer_size;
        stats[prefix + "free_buffers"] = pool.second.free_buffers.size();
        stats[prefix + "allocated_buffers"] = pool.second.allocated_buffers;
        stats[prefix + "peak_usage"] = pool.second.peak_usage;
        stats[prefix + "hits"] = pool.second.hits;
        stats[prefix + "misses"] = pool.second.misses;
        pool_index++;
    }
    
    // Add component-specific stats
    for (const auto& component : m_component_usage) {
        std::string prefix = "component_" + component.first + "_";
        stats[prefix + "current_usage"] = component.second.current_usage;
        stats[prefix + "peak_usage"] = component.second.peak_usage;
        stats[prefix + "allocations"] = component.second.allocations;
        stats[prefix + "deallocations"] = component.second.deallocations;
    }
    
    return stats;
}

void MemoryPoolManager::optimizeMemoryUsage() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Debug::log("memory", "MemoryPoolManager::optimizeMemoryUsage() - Optimizing memory usage");
    
    // Update memory pressure level
    updateMemoryPressureLevel();
    
    // Clean up pools based on memory pressure
    cleanupPools();
    
    // Analyze usage patterns and adjust pool configurations
    for (auto& pool : m_pools) {
        // Calculate hit rate
        double hit_rate = 0.0;
        if (pool.second.hits + pool.second.misses > 0) {
            hit_rate = static_cast<double>(pool.second.hits) / 
                      (pool.second.hits + pool.second.misses);
        }
        
        // Adjust pool size based on hit rate and memory pressure
        size_t optimal_size = getMaxBuffersPerPool();
        
        if (hit_rate < 0.3) {
            // Low hit rate, reduce pool size
            optimal_size = std::max(size_t(1), optimal_size / 2);
        } else if (hit_rate > 0.8) {
            // High hit rate, increase pool size (but respect memory pressure)
            if (m_memory_pressure_level < 50) {
                optimal_size = std::min(optimal_size * 2, getMaxBuffersPerPool());
            }
        }
        
        // Resize pool if needed
        if (pool.second.free_buffers.size() > optimal_size) {
            size_t to_remove = pool.second.free_buffers.size() - optimal_size;
            
            for (size_t i = 0; i < to_remove; i++) {
                uint8_t* buffer = pool.second.free_buffers.back();
                pool.second.free_buffers.pop_back();
                delete[] buffer;
                
                if (pool.second.allocated_buffers > 0) {
                    pool.second.allocated_buffers--;
                }
                
                m_total_pooled -= pool.first;
            }
            
            Debug::log("memory", "MemoryPoolManager::optimizeMemoryUsage() - Removed ", 
                      to_remove, " buffers from pool size ", pool.first);
        }
    }
}

int MemoryPoolManager::registerMemoryPressureCallback(std::function<void(int)> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    int id = m_callback_id_counter++;
    m_pressure_callbacks.push_back(std::make_pair(id, callback));
    
    // Immediately notify with current pressure level
    callback(m_memory_pressure_level);
    
    return id;
}

void MemoryPoolManager::unregisterMemoryPressureCallback(int id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = std::find_if(m_pressure_callbacks.begin(), m_pressure_callbacks.end(),
                          [id](const auto& pair) { return pair.first == id; });
    
    if (it != m_pressure_callbacks.end()) {
        m_pressure_callbacks.erase(it);
    }
}

int MemoryPoolManager::getMemoryPressureLevel() const {
    return m_memory_pressure_level;
}

bool MemoryPoolManager::isSafeToAllocate(size_t requested_size, const std::string& component_name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if allocation would exceed total memory limit
    if (m_total_allocated + requested_size > m_max_total_memory) {
        Debug::log("memory", "MemoryPoolManager::isSafeToAllocate() - Total memory limit would be exceeded: ",
                  m_total_allocated + requested_size, " > ", m_max_total_memory);
        return false;
    }
    
    // Check if allocation would exceed buffer memory limit for buffer-related components
    if ((component_name == "http" || component_name == "file" || component_name == "buffer") &&
        m_total_allocated + requested_size > m_max_buffer_memory) {
        Debug::log("memory", "MemoryPoolManager::isSafeToAllocate() - Buffer memory limit would be exceeded: ",
                  m_total_allocated + requested_size, " > ", m_max_buffer_memory);
        return false;
    }
    
    // Check memory pressure level
    if (m_memory_pressure_level > 90 && requested_size > 64 * 1024) {
        Debug::log("memory", "MemoryPoolManager::isSafeToAllocate() - Critical memory pressure, rejecting large allocation: ", requested_size);
        return false;
    }
    
    if (m_memory_pressure_level > 75 && requested_size > 256 * 1024) {
        Debug::log("memory", "MemoryPoolManager::isSafeToAllocate() - High memory pressure, rejecting very large allocation: ", requested_size);
        return false;
    }
    
    return true;
}

size_t MemoryPoolManager::getOptimalBufferSize(size_t requested_size, 
                                             const std::string& component_name,
                                             bool sequential_access) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Start with the requested size
    size_t optimal_size = requested_size;
    
    // Round to nearest pool size for better reuse
    optimal_size = roundToPoolSize(optimal_size);
    
    // Adjust based on memory pressure
    if (m_memory_pressure_level > 90) {
        // Critical pressure - use minimum viable size
        optimal_size = std::min(optimal_size, size_t(64 * 1024));
    } else if (m_memory_pressure_level > 75) {
        // High pressure - reduce buffer sizes
        optimal_size = std::min(optimal_size, size_t(128 * 1024));
    } else if (m_memory_pressure_level > 50) {
        // Moderate pressure - use moderate buffer sizes
        optimal_size = std::min(optimal_size, size_t(256 * 1024));
    }
    
    // Adjust based on access pattern
    if (sequential_access && m_memory_pressure_level < 50) {
        // For sequential access with low memory pressure, larger buffers are better
        if (optimal_size < 64 * 1024) {
            optimal_size = 64 * 1024;
        }
    } else if (!sequential_access) {
        // For random access, smaller buffers are better
        if (optimal_size > 32 * 1024) {
            optimal_size = 32 * 1024;
        }
    }
    
    // Ensure we don't go below the requested size
    return std::max(optimal_size, requested_size);
}

void MemoryPoolManager::createPool(size_t size, size_t max_buffers, size_t pre_allocate) {
    // Create pool entry if it doesn't exist
    auto it = m_pools.find(size);
    if (it == m_pools.end()) {
        it = m_pools.emplace(size, PoolEntry(size)).first;
    }
    auto& pool = it->second;
    
    // Pre-allocate buffers
    pre_allocate = std::min(pre_allocate, max_buffers);
    
    for (size_t i = 0; i < pre_allocate; i++) {
        try {
            uint8_t* buffer = new uint8_t[size];
            pool.free_buffers.push_back(buffer);
            pool.allocated_buffers++;
            m_total_pooled += size;
        } catch (const std::bad_alloc& e) {
            Debug::log("memory", "MemoryPoolManager::createPool() - Failed to pre-allocate buffer: ", e.what());
            break;
        }
    }
    
    Debug::log("memory", "MemoryPoolManager::createPool() - Created pool for size ", size, 
              " with ", pre_allocate, " pre-allocated buffers");
}

std::map<size_t, MemoryPoolManager::PoolEntry>::iterator MemoryPoolManager::findBestPool(size_t size) {
    // First try to find an exact match
    auto it = m_pools.find(size);
    if (it != m_pools.end()) {
        return it;
    }
    
    // Find the smallest pool that can accommodate the requested size
    for (it = m_pools.begin(); it != m_pools.end(); ++it) {
        if (it->first >= size) {
            return it;
        }
    }
    
    // No suitable pool found
    return m_pools.end();
}

void MemoryPoolManager::updateMemoryPressureLevel() {
    // Use the memory tracker's pressure level
    m_memory_pressure_level = MemoryTracker::getInstance().getMemoryPressureLevel();
}

void MemoryPoolManager::notifyPressureCallbacks() {
    // Make a copy of callbacks to avoid holding the lock during callback execution
    std::vector<std::pair<int, std::function<void(int)>>> callbacks;
    int pressure_level;
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        callbacks = m_pressure_callbacks;
        pressure_level = m_memory_pressure_level;
    }
    
    // Notify all callbacks without holding the mutex
    for (const auto& callback : callbacks) {
        try {
            callback.second(pressure_level);
        } catch (...) {
            // Ignore exceptions from callbacks
        }
    }
}

void MemoryPoolManager::cleanupPools() {
    Debug::log("memory", "MemoryPoolManager::cleanupPools() - Cleaning up pools, pressure level: ", 
              m_memory_pressure_level);
    
    // Calculate how aggressive to be with cleanup based on memory pressure
    float cleanup_factor = 0.0f;
    if (m_memory_pressure_level > 90) {
        cleanup_factor = 0.75f;  // Remove 75% of pooled buffers
    } else if (m_memory_pressure_level > 75) {
        cleanup_factor = 0.5f;   // Remove 50% of pooled buffers
    } else if (m_memory_pressure_level > 50) {
        cleanup_factor = 0.25f;  // Remove 25% of pooled buffers
    } else {
        cleanup_factor = 0.1f;   // Remove 10% of pooled buffers
    }
    
    // Clean up each pool
    for (auto& pool : m_pools) {
        size_t to_remove = static_cast<size_t>(pool.second.free_buffers.size() * cleanup_factor);
        
        for (size_t i = 0; i < to_remove && !pool.second.free_buffers.empty(); i++) {
            uint8_t* buffer = pool.second.free_buffers.back();
            pool.second.free_buffers.pop_back();
            delete[] buffer;
            
            if (pool.second.allocated_buffers > 0) {
                pool.second.allocated_buffers--;
            }
            
            m_total_pooled -= pool.first;
        }
        
        if (to_remove > 0) {
            Debug::log("memory", "MemoryPoolManager::cleanupPools() - Removed ", to_remove, 
                      " buffers from pool size ", pool.first);
        }
    }
}

size_t MemoryPoolManager::roundToPoolSize(size_t size) const {
    // Find the smallest pool size that can accommodate the requested size
    for (const auto& config : m_pool_configs) {
        if (config.buffer_size >= size) {
            return config.buffer_size;
        }
    }
    
    // If larger than any pool, return the original size
    return size;
}

bool MemoryPoolManager::shouldPool(size_t size) const {
    // Don't pool tiny buffers
    if (size < 1024) {
        return false;
    }
    
    // Don't pool very large buffers
    if (size > 512 * 1024) {
        return false;
    }
    
    // Check if size is close to a standard pool size
    for (const auto& config : m_pool_configs) {
        // If within 10% of a standard size, consider it poolable
        if (std::abs(static_cast<int>(size) - static_cast<int>(config.buffer_size)) < 
            static_cast<int>(config.buffer_size * 0.1)) {
            return true;
        }
    }
    
    // Under high memory pressure, be more selective
    if (m_memory_pressure_level > 70 && size > 64 * 1024) {
        return false;
    }
    
    return true;
}

size_t MemoryPoolManager::getMaxBuffersPerPool() const {
    // Scale max buffers based on memory pressure
    // At 0% pressure: 16 buffers
    // At 100% pressure: 2 buffers
    return 16 - ((14 * m_memory_pressure_level) / 100);
}