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
{
    // Buffer size thresholds
    m_small_buffer_threshold = 16 * 1024;   // 16KB
    m_medium_buffer_threshold = 128 * 1024; // 128KB

    // Pool configuration constants
    m_default_max_pooled_buffers = 32;
    m_default_max_buffer_size = 1024 * 1024; // 1MB
    m_cleanup_interval = std::chrono::seconds(30);

    // Limit configuration constants
    m_min_pool_size = 1024;
    m_min_memory_pressure_reduce = 8;
    m_pressure_reduction_val = 256 * 1024;

    // Register for memory pressure updates
    MemoryTracker::getInstance().registerMemoryPressureCallback(
        [this](int pressure) { this->setMemoryPressure(pressure); }
    );
}

size_t EnhancedBufferPool::calculateRoundedSize(size_t target_size) const {
    // Use optimal buffer sizes based on common use cases
    if (target_size <= 4096) {
        return 4096; // 4KB - common for small chunks
    } else if (target_size <= 16384) {
        return 16384; // 16KB - common for medium chunks
    } else if (target_size <= 65536) {
        return 65536; // 64KB - common for large chunks
    } else {
        // Round up to nearest 64KB for very large buffers
        return ((target_size + 65535) / 65536) * 65536;
    }
}

} // namespace IO
} // namespace PsyMP3
