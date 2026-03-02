/*
 * BufferPool.h - Memory pool for efficient buffer allocation
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

#ifndef BUFFERPOOL_H
#define BUFFERPOOL_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace IO {

/**
 * @brief Memory pool for efficient buffer allocation and reuse
 * 
 * This class provides a thread-safe memory pool for frequently used buffer sizes
 * to reduce allocation/deallocation overhead and memory fragmentation.
 */
class IOBufferPool {
private:
    struct PoolEntry; // Forward declaration

public:
    /**
     * @brief Buffer handle for RAII management
     */
    class Buffer {
    public:
        Buffer() : m_data(nullptr), m_size(0) {}
        Buffer(uint8_t* data, size_t size, std::shared_ptr<PoolEntry> entry)
            : m_data(data), m_size(size), m_entry(std::move(entry)) {}
        
        // Move constructor
        Buffer(Buffer&& other) noexcept 
            : m_data(other.m_data), m_size(other.m_size), m_entry(std::move(other.m_entry)) {
            other.m_data = nullptr;
            other.m_size = 0;
        }
        
        // Move assignment
        Buffer& operator=(Buffer&& other) noexcept {
            if (this != &other) {
                release();
                m_data = other.m_data;
                m_size = other.m_size;
                m_entry = std::move(other.m_entry);
                other.m_data = nullptr;
                other.m_size = 0;
            }
            return *this;
        }
        
        // Disable copy constructor and assignment
        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;
        
        ~Buffer() {
            release();
        }
        
        uint8_t* data() const { return m_data; }
        size_t size() const { return m_size; }
        bool empty() const { return m_data == nullptr || m_size == 0; }
        
        void release();
        
    private:
        uint8_t* m_data;
        size_t m_size;
        std::shared_ptr<PoolEntry> m_entry;
    };
    
    /**
     * @brief Memory pressure levels for adaptive buffer management
     */
    enum class MemoryPressureLevel {
        Normal,   // Normal operation, full buffer pooling
        High,     // High memory pressure, reduced pooling
        Critical  // Critical memory pressure, minimal pooling
    };
    
    /**
     * @brief Get the singleton instance of the buffer pool
     * @return Reference to the global buffer pool instance
     */
    static IOBufferPool& getInstance();
    
    /**
     * @brief Acquire a buffer of the specified size
     * @param size Requested buffer size in bytes
     * @return Buffer handle (may be empty if allocation fails)
     */
    Buffer acquire(size_t size);
    
    /**
     * @brief Return a buffer to the pool for reuse
     * @param data Pointer to buffer data
     * @param size Size of the buffer
     */
    void release(uint8_t* data, size_t size);
    
    /**
     * @brief Get pool statistics
     * @return Map with pool statistics (total_allocated, pool_hits, etc.)
     */
    std::map<std::string, size_t> getStats() const;
    
    /**
     * @brief Clear all pooled buffers and free memory
     */
    void clear();
    
    /**
     * @brief Set maximum total memory that can be pooled
     * @param max_bytes Maximum pooled memory in bytes (default: 16MB)
     */
    void setMaxPoolSize(size_t max_bytes);
    
    /**
     * @brief Set maximum number of buffers to pool per size
     * @param max_buffers Maximum buffers per size (default: 8)
     */
    void setMaxBuffersPerSize(size_t max_buffers);
    
    /**
     * @brief Get current memory pressure level
     * @return Current memory pressure level
     */
    MemoryPressureLevel getMemoryPressureLevel() const { return m_memory_pressure_level; }
    
    /**
     * @brief Pre-allocate buffers for common sizes to reduce allocation overhead
     * This method allocates a small number of buffers for commonly used sizes
     * to improve performance during initial playback
     */
    void preAllocateCommonBuffers();
    
    /**
     * @brief Check if a size is in the common sizes list
     * @param size Buffer size to check
     * @return true if size is a common size, false otherwise
     */
    bool isCommonSize(size_t size) const;
    
    /**
     * @brief Optimize allocation patterns based on usage statistics
     * Analyzes buffer usage patterns and adjusts pool configuration for better efficiency
     */
    void optimizeAllocationPatterns();
    
    /**
     * @brief Compact memory by removing unused pool entries and consolidating fragmented allocations
     * This method helps reduce memory fragmentation and improves allocation efficiency
     */
    void compactMemory();
    
    /**
     * @brief Defragment buffer pools by analyzing size distribution and consolidation opportunities
     * This method identifies opportunities to reduce pool fragmentation
     */
    void defragmentPools();
    
    /**
     * @brief Evict least recently used buffers if pool is approaching limits
     * This method can be called externally to proactively manage memory usage
     */
    void evictIfNeeded();
    
    /**
     * @brief Enforce strict bounded cache limits to prevent memory leaks
     * This method implements aggressive memory management when limits are exceeded
     */
    void enforceBoundedLimits();
    
    /**
     * @brief Internal version of enforceBoundedLimits that assumes lock is already held
     */
    void enforceBoundedLimitsInternal();
    
    /**
     * @brief Get current memory usage as percentage of limit
     * @return Memory usage percentage (0-100)
     */
    float getMemoryUsagePercent() const;

private:
    /**
     * @brief Update current pool size with thread-local batching
     * @param delta Bytes to add (can be negative)
     */
    void updateCurrentSize(int64_t delta);

    IOBufferPool();
    ~IOBufferPool();
    
    // Disable copy constructor and assignment
    IOBufferPool(const IOBufferPool&) = delete;
    IOBufferPool& operator=(const IOBufferPool&) = delete;
    
    struct PoolEntry {
        std::vector<uint8_t*> available_buffers;
        size_t buffer_size;
        std::atomic<size_t> total_allocated;
        std::atomic<size_t> pool_hits;
        std::atomic<size_t> pool_misses;
        mutable std::mutex mutex; // Fine-grained lock for this pool entry
        IOBufferPool* pool_instance; // Back pointer to pool instance
        
        PoolEntry(size_t size, IOBufferPool* instance)
            : buffer_size(size), total_allocated(0), pool_hits(0), pool_misses(0), pool_instance(instance) {}

        // Disable copy/move because mutex is not copyable/movable
        PoolEntry(const PoolEntry&) = delete;
        PoolEntry& operator=(const PoolEntry&) = delete;

        /**
         * @brief Return a buffer to this pool entry
         * @param data Buffer data to return
         */
        void returnBuffer(uint8_t* data);
        
        /**
         * @brief Calculate hit rate for this pool entry
         * @return Hit rate as a value between 0.0 and 1.0
         */
        double getHitRate() const;
    };
    
    mutable std::shared_mutex m_pools_mutex; // Replaces m_mutex with read/write lock
    std::map<size_t, std::shared_ptr<PoolEntry>> m_pools; // Use shared_ptr for thread-local caching and lifetime management
    std::atomic<size_t> m_max_pool_size{16 * 1024 * 1024};     // 16MB default max pool size
    std::atomic<size_t> m_max_buffers_per_size{8};             // 8 buffers per size default
    std::atomic<size_t> m_current_pool_size{0};                // Current total pooled memory
    
    // Memory pressure monitoring
    std::atomic<MemoryPressureLevel> m_memory_pressure_level;   // Current memory pressure level
    std::thread m_monitoring_thread;               // Memory monitoring thread
    std::atomic<bool> m_monitoring_active{false};  // Flag to control monitoring thread
    
    // Adaptive pool parameters based on memory pressure
    std::atomic<size_t> m_effective_max_pool_size{16 * 1024 * 1024};  // Effective max pool size
    std::atomic<size_t> m_effective_max_buffers_per_size{8};          // Effective max buffers per size
    
    // Common buffer sizes for pre-allocation
    std::vector<size_t> m_common_sizes;
    
    /**
     * @brief Round size up to next power of 2 for efficient pooling
     * @param size Original size
     * @return Rounded size
     */
    size_t roundToPoolSize(size_t size) const;
    
    /**
     * @brief Check if we can pool a buffer of this size
     * @param size Buffer size
     * @return true if size is suitable for pooling, false otherwise
     */
    bool shouldPool(size_t size) const;
    
    /**
     * @brief Evict least recently used buffers if pool is full (internal method)
     * This method assumes the mutex is already locked
     */
    void evictIfNeededInternal();
    
    /**
     * @brief Start memory pressure monitoring thread
     */
    void startMemoryMonitoring();
    
    /**
     * @brief Stop memory pressure monitoring thread
     */
    void stopMemoryMonitoring();
    
    /**
     * @brief Memory pressure monitoring thread function
     */
    void monitorMemoryPressure();
    
    /**
     * @brief Detect current memory pressure level
     * @return Current memory pressure level
     */
    MemoryPressureLevel detectMemoryPressure();
    
    /**
     * @brief Adjust pool parameters based on memory pressure
     */
    void adjustPoolParametersForMemoryPressure();
    
    /**
     * @brief Convert memory pressure level to string for logging
     * @param level Memory pressure level
     * @return String representation of memory pressure level
     */
    static std::string memoryPressureLevelToString(MemoryPressureLevel level);
};

} // namespace IO
} // namespace PsyMP3

#endif // BUFFERPOOL_H
