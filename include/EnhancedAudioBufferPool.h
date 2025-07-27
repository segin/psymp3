/*
 * EnhancedAudioBufferPool.h - Enhanced audio buffer pool for memory optimization
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

#ifndef ENHANCEDAUDIOBUFFERPOOL_H
#define ENHANCEDAUDIOBUFFERPOOL_H

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Enhanced audio buffer pool for memory optimization
 * 
 * This class provides an enhanced buffer pool specifically for audio samples
 * with memory pressure awareness, usage statistics tracking, and adaptive buffer management.
 */
class EnhancedAudioBufferPool {
public:
    /**
     * @brief Pool statistics structure
     */
    struct PoolStats {
        size_t total_buffers = 0;
        size_t largest_buffer_size = 0;
        size_t total_samples = 0;
        size_t buffer_hits = 0;
        size_t buffer_misses = 0;
        int memory_pressure = 0;
        size_t reuse_count = 0;
        float hit_ratio = 0.0f;
    };
    
    /**
     * @brief Get the singleton instance
     * @return Reference to the global enhanced audio buffer pool instance
     */
    static EnhancedAudioBufferPool& getInstance();
    
    /**
     * @brief Get a sample buffer with specified minimum and preferred sizes
     * @param min_samples Minimum required number of samples
     * @param preferred_samples Preferred number of samples (0 = use min_samples)
     * @return Buffer vector with at least min_samples capacity
     */
    std::vector<int16_t> getSampleBuffer(size_t min_samples, size_t preferred_samples = 0);
    
    /**
     * @brief Return a sample buffer to the pool for reuse
     * @param buffer Buffer to return (moved)
     */
    void returnSampleBuffer(std::vector<int16_t>&& buffer);
    
    /**
     * @brief Clear all pooled buffers
     */
    void clear();
    
    /**
     * @brief Set memory pressure level (0-100)
     * @param pressure_level Memory pressure level
     */
    void setMemoryPressure(int pressure_level);
    
    /**
     * @brief Get current memory pressure level
     * @return Memory pressure level (0-100)
     */
    int getMemoryPressure() const;
    
    /**
     * @brief Get pool statistics
     * @return Pool statistics structure
     */
    PoolStats getStats() const;

private:
    EnhancedAudioBufferPool();
    ~EnhancedAudioBufferPool();
    
    // Disable copy constructor and assignment
    EnhancedAudioBufferPool(const EnhancedAudioBufferPool&) = delete;
    EnhancedAudioBufferPool& operator=(const EnhancedAudioBufferPool&) = delete;
    
    // Buffer size thresholds (in samples)
    static constexpr size_t SMALL_BUFFER_THRESHOLD = 4096;   // ~85ms at 48kHz stereo
    static constexpr size_t MEDIUM_BUFFER_THRESHOLD = 32768; // ~680ms at 48kHz stereo
    
    // Pool configuration constants
    static constexpr size_t DEFAULT_MAX_POOLED_BUFFERS = 16;
    static constexpr size_t DEFAULT_MAX_SAMPLES_PER_BUFFER = 192 * 1024; // ~4s at 48kHz stereo
    static constexpr std::chrono::seconds CLEANUP_INTERVAL{30};
    
    // Buffer pools by size category
    std::vector<std::vector<int16_t>> m_small_buffers;
    std::vector<std::vector<int16_t>> m_medium_buffers;
    std::vector<std::vector<int16_t>> m_large_buffers;
    
    // Thread safety
    mutable std::mutex m_mutex;
    
    // Memory pressure tracking
    int m_memory_pressure;
    
    // Usage statistics
    size_t m_buffer_hits;
    size_t m_buffer_misses;
    size_t m_buffer_reuse_count;
    
    // Usage pattern tracking
    struct UsageStats {
        size_t request_count = 0;
        std::chrono::steady_clock::time_point last_request;
    };
    std::map<size_t, UsageStats> m_size_usage_stats;
    
    // Cleanup timing
    std::chrono::steady_clock::time_point m_last_cleanup;
    
    /**
     * @brief Get maximum number of pooled buffers based on memory pressure
     * @return Maximum pooled buffers
     */
    size_t getMaxPooledBuffers() const;
    
    /**
     * @brief Get maximum samples per buffer based on memory pressure
     * @return Maximum samples per buffer
     */
    size_t getMaxSamplesPerBuffer() const;
    
    /**
     * @brief Check if a buffer should be pooled
     * @param capacity Buffer capacity in samples
     * @return true if buffer should be pooled
     */
    bool shouldPoolBuffer(size_t capacity) const;
    
    /**
     * @brief Perform periodic cleanup of unused buffers
     */
    void performPeriodicCleanup();
};

// Alias for backward compatibility
using AudioBufferPool = EnhancedAudioBufferPool;

#endif // ENHANCEDAUDIOBUFFERPOOL_H