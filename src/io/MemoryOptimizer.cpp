/*
 * MemoryOptimizer.cpp - Memory optimization for I/O operations
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace IO {

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
    return m_memory_pressure_level.load();
}

size_t MemoryOptimizer::getOptimalBufferSize(size_t requested_size, 
                                           const std::string& usage_pattern,
                                           bool sequential_access) const {
    return calculateOptimalBufferSize(requested_size, m_memory_pressure_level.load(), sequential_access);
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
    stats["memory_pressure_level"] = static_cast<size_t>(m_memory_pressure_level.load());
    
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
    MemoryPressureLevel current_level = m_memory_pressure_level.load();
    if (current_level == MemoryPressureLevel::Critical) {
        // Critical memory pressure - aggressive optimization
        Debug::log("memory", "MemoryOptimizer::optimizeMemoryUsage() - Critical memory pressure, performing aggressive optimization");
        
        // Clear all buffer pools
        IOBufferPool::getInstance().clear();
        
        // Reduce buffer pool limits drastically
        size_t critical_pool_size = m_max_total_memory / 16; // 6.25% of total
        IOBufferPool::getInstance().setMaxPoolSize(critical_pool_size);
        IOBufferPool::getInstance().setMaxBuffersPerSize(1); // Minimal buffering
        
    } else if (current_level == MemoryPressureLevel::High) {
        // High memory pressure - moderate optimization
        Debug::log("memory", "MemoryOptimizer::optimizeMemoryUsage() - High memory pressure, performing moderate optimization");
        
        // Optimize buffer pools
        IOBufferPool::getInstance().optimizeAllocationPatterns();
        IOBufferPool::getInstance().compactMemory();
        
        // Reduce buffer pool limits moderately
        size_t high_pool_size = m_max_total_memory / 8; // 12.5% of total
        IOBufferPool::getInstance().setMaxPoolSize(high_pool_size);
        IOBufferPool::getInstance().setMaxBuffersPerSize(2); // Reduced buffering
        
    } else if (current_level == MemoryPressureLevel::Normal) {
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
    switch (m_memory_pressure_level.load()) {
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
    return m_memory_pressure_level.load() <= MemoryPressureLevel::Normal;
}

size_t MemoryOptimizer::getRecommendedReadAheadSize(size_t default_size) const {
    switch (m_memory_pressure_level.load()) {
        case MemoryPressureLevel::Critical:
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
    // Start memory pressure monitoring thread
    m_monitoring_active = true;
    m_monitoring_thread = std::thread([this]() {
        monitorMemoryPressure();
    });
}

void MemoryOptimizer::stopMemoryMonitoring() {
    // Stop memory pressure monitoring thread
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
        std::this_thread::sleep_for(5s);
        
        // Check system memory pressure
        MemoryPressureLevel new_pressure = detectMemoryPressure();
        
        // Update memory pressure level if changed
        if (new_pressure != m_memory_pressure_level.load()) {
            updateMemoryPressureLevel(new_pressure);
        }
    }
    
    Debug::log("memory", "MemoryOptimizer::monitorMemoryPressure() - Stopping memory pressure monitoring");
}

MemoryOptimizer::MemoryPressureLevel MemoryOptimizer::detectMemoryPressure() {
    // Get memory usage statistics
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_max_total_memory == 0) {
        return MemoryPressureLevel::Normal;
    }
    
    // Calculate memory usage percentage
    float usage_percent = static_cast<float>(m_total_memory_usage) / static_cast<float>(m_max_total_memory) * 100.0f;
    
    // Determine memory pressure level based on usage percentage
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

void MemoryOptimizer::updateMemoryPressureLevel(MemoryPressureLevel new_level) {
    Debug::log("memory", "MemoryOptimizer::updateMemoryPressureLevel() - Memory pressure changed from ",
              memoryPressureLevelToString(m_memory_pressure_level.load()), " to ",
              memoryPressureLevelToString(new_level));
    
    m_memory_pressure_level.store(new_level);
    
    // Trigger optimization if pressure increased
    if (new_level > MemoryPressureLevel::Normal) {
        optimizeMemoryUsage();
    }
}

bool MemoryOptimizer::getSystemMemoryInfo(size_t& total_memory, size_t& available_memory) const {
    // This is a simplified implementation - in practice, you'd use platform-specific APIs
    // Use a conservative default that works on both 32-bit and 64-bit systems
    // On 32-bit systems, size_t max is ~4GB, so we use 1GB as a safe default
    // On 64-bit systems, this is still a reasonable conservative estimate
    constexpr size_t DEFAULT_MEMORY = static_cast<size_t>(1024) * 1024 * 1024; // 1GB default
    total_memory = DEFAULT_MEMORY;
    available_memory = total_memory / 2;    // 50% available default
    return true;
}

size_t MemoryOptimizer::calculateOptimalBufferSize(size_t base_size, 
                                                 MemoryPressureLevel pressure_level,
                                                 bool sequential) const {
    size_t optimal_size = base_size;
    
    // Adjust based on memory pressure
    switch (pressure_level) {
        case MemoryPressureLevel::Critical:
            optimal_size = std::min(optimal_size, static_cast<size_t>(16 * 1024)); // 16KB max
            break;
        case MemoryPressureLevel::High:
            optimal_size = std::min(optimal_size, static_cast<size_t>(64 * 1024)); // 64KB max
            break;
        case MemoryPressureLevel::Normal:
            optimal_size = std::min(optimal_size, static_cast<size_t>(256 * 1024)); // 256KB max
            break;
        case MemoryPressureLevel::Low:
        default:
            // No restriction for low pressure
            break;
    }
    
    // Adjust based on access pattern
    if (sequential && pressure_level <= MemoryPressureLevel::Normal) {
        // For sequential access with low/normal pressure, larger buffers are better
        optimal_size = std::max(optimal_size, static_cast<size_t>(32 * 1024)); // 32KB min
    } else if (!sequential) {
        // For random access, smaller buffers are better
        optimal_size = std::min(optimal_size, static_cast<size_t>(16 * 1024)); // 16KB max
    }
    
    return optimal_size;
}

} // namespace IO
} // namespace PsyMP3