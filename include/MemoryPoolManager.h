/*
 * MemoryPoolManager.h - Memory pool management for I/O operations
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef MEMORYPOOLMANAGER_H
#define MEMORYPOOLMANAGER_H

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Memory pool management for I/O operations
 * 
 * This class provides centralized memory pool management for the IOHandler subsystem,
 * coordinating buffer allocation, memory pressure monitoring, and resource optimization.
 */
class MemoryPoolManager {
public:
    /**
     * @brief Get the singleton instance of the memory pool manager
     * @return Reference to the global memory pool manager instance
     */
    static MemoryPoolManager& getInstance();
    
    /**
     * @brief Initialize memory pools with optimal sizes
     * This method pre-allocates memory pools for common buffer sizes
     * to improve performance and reduce memory fragmentation
     */
    void initializePools();
    
    /**
     * @brief Initialize memory tracker integration
     * This method registers the memory pressure callback with MemoryTracker
     * and should be called after construction to avoid circular dependency
     */
    void initializeMemoryTracking();
    
    /**
     * @brief Allocate a buffer from the appropriate pool
     * @param size Requested buffer size in bytes
     * @param component_name Name of component requesting the buffer
     * @return Allocated buffer pointer, or nullptr if allocation fails
     */
    uint8_t* allocateBuffer(size_t size, const std::string& component_name);
    
    /**
     * @brief Release a buffer back to the pool
     * @param buffer Buffer pointer to release
     * @param size Size of the buffer
     * @param component_name Name of component releasing the buffer
     */
    void releaseBuffer(uint8_t* buffer, size_t size, const std::string& component_name);
    
    /**
     * @brief Set global memory limits for all pools
     * @param max_total_memory Maximum total memory for all pools
     * @param max_buffer_memory Maximum memory for buffer pools
     */
    void setMemoryLimits(size_t max_total_memory, size_t max_buffer_memory);
    
    /**
     * @brief Get memory usage statistics
     * @return Map with memory usage statistics
     */
    std::map<std::string, size_t> getMemoryStats() const;
    
    /**
     * @brief Perform global memory optimization
     * This method analyzes current memory usage and performs appropriate optimizations
     * based on memory pressure levels
     */
    void optimizeMemoryUsage();
    
    /**
     * @brief Register a memory pressure callback
     * @param callback Function to call when memory pressure changes
     * @return Callback ID for unregistering
     */
    int registerMemoryPressureCallback(std::function<void(int)> callback);
    
    /**
     * @brief Unregister a memory pressure callback
     * @param id Callback ID returned from registerMemoryPressureCallback
     */
    void unregisterMemoryPressureCallback(int id);
    
    /**
     * @brief Get current memory pressure level (0-100)
     * @return Memory pressure level as percentage
     */
    int getMemoryPressureLevel() const;
    
    /**
     * @brief Check if memory allocation is within safe limits
     * @param requested_size Size of memory to allocate
     * @param component_name Name of component requesting memory
     * @return true if allocation is safe, false if it would cause memory pressure
     */
    bool isSafeToAllocate(size_t requested_size, const std::string& component_name) const;
    
    /**
     * @brief Get optimal buffer size based on memory pressure and usage patterns
     * @param requested_size Requested buffer size
     * @param component_name Component name for usage pattern tracking
     * @param sequential_access Whether access pattern is sequential
     * @return Optimized buffer size
     */
    size_t getOptimalBufferSize(size_t requested_size, 
                               const std::string& component_name,
                               bool sequential_access = true) const;

private:
    // Private unlocked versions of public methods (assumes m_mutex is already held)
    
    /**
     * @brief Allocate a buffer from the appropriate pool (unlocked version)
     * @param size Requested buffer size in bytes
     * @param component_name Name of component requesting the buffer
     * @return Allocated buffer pointer, or nullptr if allocation fails
     * @note Assumes m_mutex is already held by caller
     */
    uint8_t* allocateBuffer_unlocked(size_t size, const std::string& component_name);
    
    /**
     * @brief Release a buffer back to the pool (unlocked version)
     * @param buffer Buffer pointer to release
     * @param size Size of the buffer
     * @param component_name Name of component releasing the buffer
     * @note Assumes m_mutex is already held by caller
     */
    void releaseBuffer_unlocked(uint8_t* buffer, size_t size, const std::string& component_name);
    
    /**
     * @brief Get memory usage statistics (unlocked version)
     * @return Map with memory usage statistics
     * @note Assumes m_mutex is already held by caller
     */
    std::map<std::string, size_t> getMemoryStats_unlocked() const;
    
    /**
     * @brief Perform global memory optimization (unlocked version)
     * @note Assumes m_mutex is already held by caller
     */
    void optimizeMemoryUsage_unlocked();
    
    /**
     * @brief Check if memory allocation is within safe limits (unlocked version)
     * @param requested_size Size of memory to allocate
     * @param component_name Name of component requesting memory
     * @return true if allocation is safe, false if it would cause memory pressure
     * @note Assumes m_mutex is already held by caller
     */
    bool isSafeToAllocate_unlocked(size_t requested_size, const std::string& component_name) const;
    
    /**
     * @brief Get optimal buffer size based on memory pressure and usage patterns (unlocked version)
     * @param requested_size Requested buffer size
     * @param component_name Component name for usage pattern tracking
     * @param sequential_access Whether access pattern is sequential
     * @return Optimized buffer size
     * @note Assumes m_mutex is already held by caller
     */
    size_t getOptimalBufferSize_unlocked(size_t requested_size, 
                                        const std::string& component_name,
                                        bool sequential_access = true) const;
    
    /**
     * @brief Notify all registered callbacks of pressure level change (unlocked version)
     * @note Assumes m_mutex is already held by caller, but releases it during callback execution
     */
    void notifyPressureCallbacks_unlocked();
    MemoryPoolManager();
    ~MemoryPoolManager();
    
    // Disable copy constructor and assignment
    MemoryPoolManager(const MemoryPoolManager&) = delete;
    MemoryPoolManager& operator=(const MemoryPoolManager&) = delete;
    
    // Memory pool configuration
    struct PoolConfig {
        size_t buffer_size;          // Size of each buffer in this pool
        size_t max_buffers;          // Maximum number of buffers in this pool
        std::string usage_pattern;   // Typical usage pattern for this pool
    };
    
    // Memory pool entry
    struct PoolEntry {
        size_t buffer_size;                  // Size of each buffer in this pool
        std::vector<uint8_t*> free_buffers;  // Available buffers
        size_t allocated_buffers;            // Total number of allocated buffers
        size_t peak_usage;                   // Peak number of allocated buffers
        size_t hits;                         // Number of successful allocations from pool
        size_t misses;                       // Number of allocations that missed the pool
        
        PoolEntry(size_t size) : buffer_size(size), allocated_buffers(0), 
                                peak_usage(0), hits(0), misses(0) {}
    };
    
    // Component memory usage tracking
    struct ComponentUsage {
        size_t current_usage;        // Current memory usage
        size_t peak_usage;           // Peak memory usage
        size_t allocations;          // Total number of allocations
        size_t deallocations;        // Total number of deallocations
        
        ComponentUsage() : current_usage(0), peak_usage(0), 
                          allocations(0), deallocations(0) {}
    };
    
    // Memory pools
    mutable std::mutex m_mutex;
    std::map<size_t, PoolEntry> m_pools;
    std::vector<PoolConfig> m_pool_configs;
    
    // Memory usage tracking
    size_t m_total_allocated = 0;
    size_t m_total_pooled = 0;
    size_t m_max_total_memory = 64 * 1024 * 1024;  // 64MB default
    size_t m_max_buffer_memory = 32 * 1024 * 1024; // 32MB default
    std::map<std::string, ComponentUsage> m_component_usage;
    
    // Memory pressure monitoring
    int m_memory_pressure_level = 0;
    int m_callback_id_counter = 0;
    int m_memory_tracker_callback_id = -1;
    std::vector<std::pair<int, std::function<void(int)>>> m_pressure_callbacks;
    
    // Common buffer sizes for pre-allocation
    static constexpr size_t COMMON_SIZES[] = {
        4 * 1024,      // 4KB - small file reads
        8 * 1024,      // 8KB - minimum HTTP range
        16 * 1024,     // 16KB - medium file reads
        32 * 1024,     // 32KB - large file reads
        64 * 1024,     // 64KB - default buffer size
        128 * 1024,    // 128KB - read-ahead size
        256 * 1024,    // 256KB - large HTTP range
        512 * 1024     // 512KB - maximum buffer size
    };
    
    /**
     * @brief Create a memory pool for a specific buffer size
     * @param size Buffer size for this pool
     * @param max_buffers Maximum number of buffers in this pool
     * @param pre_allocate Number of buffers to pre-allocate
     */
    void createPool(size_t size, size_t max_buffers, size_t pre_allocate = 0);
    
    /**
     * @brief Find the best pool for a given buffer size
     * @param size Requested buffer size
     * @return Iterator to the best matching pool, or m_pools.end() if no suitable pool
     */
    std::map<size_t, PoolEntry>::iterator findBestPool(size_t size);
    
    /**
     * @brief Update memory pressure level based on current usage
     */
    void updateMemoryPressureLevel();
    
    /**
     * @brief Notify all registered callbacks of pressure level change
     */
    void notifyPressureCallbacks();
    
    /**
     * @brief Clean up pools based on memory pressure
     * This method removes unused buffers from pools when memory pressure is high
     */
    void cleanupPools();
    
    /**
     * @brief Round size to nearest pool size
     * @param size Requested size
     * @return Rounded size that matches a pool
     */
    size_t roundToPoolSize(size_t size) const;
    
    /**
     * @brief Check if a size should be pooled
     * @param size Buffer size
     * @return true if size should be pooled, false otherwise
     */
    bool shouldPool(size_t size) const;
    
    /**
     * @brief Get maximum buffers per pool based on memory pressure
     * @return Maximum number of buffers per pool
     */
    size_t getMaxBuffersPerPool() const;
};

#endif // MEMORYPOOLMANAGER_H