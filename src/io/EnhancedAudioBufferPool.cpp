/*
 * EnhancedAudioBufferPool.cpp - Enhanced audio buffer pool for memory optimization
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace IO {

EnhancedAudioBufferPool& EnhancedAudioBufferPool::getInstance() {
    static EnhancedAudioBufferPool instance;
    return instance;
}

EnhancedAudioBufferPool::EnhancedAudioBufferPool() 
{
    // Buffer size thresholds (in samples)
    m_small_buffer_threshold = 4096;   // ~85ms at 48kHz stereo
    m_medium_buffer_threshold = 32768; // ~680ms at 48kHz stereo

    // Pool configuration constants
    m_default_max_pooled_buffers = 16;
    m_default_max_buffer_size = 192 * 1024; // ~4s at 48kHz stereo
    m_cleanup_interval = std::chrono::seconds(30);

    // Limit configuration constants
    m_min_pool_size = 1024;
    m_min_memory_pressure_reduce = 4;
    m_pressure_reduction_val = 48 * 1024;

    // Register for memory pressure updates
    MemoryTracker::getInstance().registerMemoryPressureCallback(
        [this](int pressure) { this->setMemoryPressure(pressure); }
    );
}

size_t EnhancedAudioBufferPool::calculateRoundedSize(size_t target_size) const {
    // Use optimal buffer sizes based on common audio frame sizes
    if (target_size <= 2048) {
        return 2048; // ~42ms at 48kHz stereo
    } else if (target_size <= 4096) {
        return 4096; // ~85ms at 48kHz stereo
    } else if (target_size <= 16384) {
        return 16384; // ~340ms at 48kHz stereo
    } else if (target_size <= 32768) {
        return 32768; // ~680ms at 48kHz stereo
    } else {
        // Round up to nearest 16K samples for very large buffers
        return ((target_size + 16383) / 16384) * 16384;
    }
}

} // namespace IO
} // namespace PsyMP3
