/*
 * MemoryOptimizer.h - Memory optimization for I/O operations
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

#ifndef MEMORYOPTIMIZER_H
#define MEMORYOPTIMIZER_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace IO {

/**
 * @brief Memory optimization for I/O operations
 * 
 * This class provides memory optimization strategies for I/O operations,
 * including buffer size optimization, memory pressure monitoring, and
 * adaptive memory management based on system conditions.
 */
class MemoryOptimizer {
public:
    /**
     * @brief Memory pressure levels for adaptive memory management
     */
    enum class MemoryPressureLevel {
        Low,      // Low memory pressure, can use more memory
        Normal,   // Normal operation, standard memory usage
        High,     // High memory pressure, reduce memory usage
        Critical  // Critical memory pressure, minimize memory usage
    };
    
    /**
     * @brief Get the singleton instance of the memory optimizer
     * @return Reference to the global memory optimizer instance
     */
    static MemoryOptimizer& getInstance();
    
    /**
     * @brief Get current memory pressure level
     * @return Current memory pressure level
     */
    MemoryPressureLevel getMemoryPressureLevel() const;
    
    /**
     * @brief Get optimal buffer size based on memory pressure and usage patterns
     * @param requested_size Requested buffer size
     * @param usage_pattern Usage pattern identifier (e.g., "http", "file", "audio")
     * @param sequential_access Whether access pattern is sequential
     * @return Optimized buffer size
     */
    size_t getOptimalBufferSize(size_t requested_size, 
                               const std::string& usage_pattern,
                               bool sequential_access = true) const;
    
    /**
     * @brief Check if memory allocation is within safe limits
     * @param requested_size Size of memory to allocate
     * @param component_name Name of component requesting memory
     * @return true if allocation is safe, false if it would cause memory pressure
     */
    bool isSafeToAllocate(size_t requested_size, const std::string& component_name) const;
    
    /**
     * @brief Register memory allocation with the optimizer
     * @param allocated_size Size of allocated memory
     * @param component_name Name of component that allocated memory
     */
    void registerAllocation(size_t allocated_size, const std::string& component_name);
    
    /**
     * @brief Register memory deallocation with the optimizer
     * @param deallocated_size Size of deallocated memory
     * @param component_name Name of component that deallocated memory
     */
    void registerDeallocation(size_t deallocated_size, const std::string& component_name);
    
    /**
     * @brief Get memory usage statistics
     * @return Map with memory usage statistics
     */
    std::map<std::string, size_t> getMemoryStats() const;
    
    /**
     * @brief Set maximum memory limits
     * @param max_total_memory Maximum total memory for all components
     * @param max_buffer_memory Maximum memory for buffers
     */
    void setMemoryLimits(size_t max_total_memory, size_t max_buffer_memory);
    
    /**
     * @brief Perform global memory optimization
     * This method analyzes current memory usage and performs appropriate optimizations
     * based on memory pressure levels
     */
    void optimizeMemoryUsage();
    
    /**
     * @brief Get recommended buffer pool parameters based on memory pressure
     * @param max_pool_size Reference to update with recommended max pool size
     * @param max_buffers_per_size Reference to update with recommended max buffers per size
     */
    void getRecommendedBufferPoolParams(size_t& max_pool_size, size_t& max_buffers_per_size) const;
    
    /**
     * @brief Check if read-ahead buffering should be enabled based on memory pressure
     * @return true if read-ahead should be enabled, false otherwise
     */
    bool shouldEnableReadAhead() const;
    
    /**
     * @brief Get recommended read-ahead size based on memory pressure
     * @param default_size Default read-ahead size
     * @return Recommended read-ahead size
     */
    size_t getRecommendedReadAheadSize(size_t default_size) const;
    
    /**
     * @brief Convert memory pressure level to string for logging
     * @param level Memory pressure level
     * @return String representation of memory pressure level
     */
    static std::string memoryPressureLevelToString(MemoryPressureLevel level);

private:
    MemoryOptimizer();
    ~MemoryOptimizer();
    
    // Disable copy constructor and assignment
    MemoryOptimizer(const MemoryOptimizer&) = delete;
    MemoryOptimizer& operator=(const MemoryOptimizer&) = delete;
    
    // Memory tracking
    mutable std::mutex m_mutex;
    std::map<std::string, size_t> m_component_memory_usage;
    size_t m_total_memory_usage = 0;
    size_t m_max_total_memory = 64 * 1024 * 1024;  // 64MB default
    size_t m_max_buffer_memory = 32 * 1024 * 1024; // 32MB default
    
    // Memory pressure monitoring
    MemoryPressureLevel m_memory_pressure_level = MemoryPressureLevel::Normal;
    std::thread m_monitoring_thread;
    std::atomic<bool> m_monitoring_active{false};
    
    // Usage pattern tracking
    struct UsagePattern {
        size_t total_allocations = 0;
        size_t total_deallocations = 0;
        size_t peak_memory = 0;
        size_t current_memory = 0;
        std::chrono::steady_clock::time_point last_allocation;
        std::chrono::steady_clock::time_point last_deallocation;
    };
    
    std::map<std::string, UsagePattern> m_usage_patterns;
    
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
     * @brief Update memory pressure level
     * @param new_level New memory pressure level
     */
    void updateMemoryPressureLevel(MemoryPressureLevel new_level);
    
    /**
     * @brief Get system memory information
     * @param total_memory Reference to update with total system memory
     * @param available_memory Reference to update with available system memory
     * @return true if information was retrieved successfully, false otherwise
     */
    bool getSystemMemoryInfo(size_t& total_memory, size_t& available_memory) const;
    
    /**
     * @brief Calculate optimal buffer size based on various factors
     * @param base_size Base buffer size
     * @param pressure_level Memory pressure level
     * @param sequential Sequential access pattern
     * @return Optimized buffer size
     */
    size_t calculateOptimalBufferSize(size_t base_size, 
                                     MemoryPressureLevel pressure_level,
                                     bool sequential) const;
};

} // namespace IO
} // namespace PsyMP3

#endif // MEMORYOPTIMIZER_H
