/*
 * EnhancedTemplateBufferPool.h - Enhanced template buffer pool for memory optimization
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

#ifndef ENHANCEDTEMPLATEBUFFERPOOL_H
#define ENHANCEDTEMPLATEBUFFERPOOL_H

#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
#include <map>
#include <algorithm>

namespace PsyMP3 {
namespace IO {

/**
 * @brief Enhanced template buffer pool for memory optimization
 *
 * This class provides an enhanced buffer pool with memory pressure awareness,
 * usage statistics tracking, and adaptive buffer management.
 */
template <typename T>
class EnhancedTemplateBufferPool {
public:
    /**
     * @brief Pool statistics structure
     */
    struct PoolStats {
        size_t total_buffers = 0;
        size_t largest_buffer_size = 0;
        size_t smallest_buffer_size = 0;
        size_t total_memory_bytes = 0;
        size_t total_samples = 0;
        size_t average_buffer_size = 0;
        size_t buffer_hits = 0;
        size_t buffer_misses = 0;
        int memory_pressure = 0;
        size_t reuse_count = 0;
        float hit_ratio = 0.0f;
    };

    virtual ~EnhancedTemplateBufferPool() {
        clear();
    }

    /**
     * @brief Get a buffer with specified minimum and preferred sizes
     * @param min_size Minimum required size
     * @param preferred_size Preferred size (0 = use min_size)
     * @return Buffer vector with at least min_size capacity
     */
    std::vector<T> getBuffer(size_t min_size, size_t preferred_size = 0) {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Perform periodic cleanup if needed
        performPeriodicCleanup();

        // Use preferred size if specified, otherwise use min_size
        size_t target_size = preferred_size > min_size ? preferred_size : min_size;

        // Update usage statistics
        auto& usage = m_size_usage_stats[min_size];
        usage.request_count++;
        usage.last_request = std::chrono::steady_clock::now();

        // Don't pool very large buffers
        if (min_size > getMaxBufferSize()) {
            m_buffer_misses++;
            std::vector<T> buffer;
            buffer.reserve(min_size);
            return buffer;
        }

        // Select appropriate buffer category
        std::vector<std::vector<T>>* category = &m_medium_buffers;
        if (min_size < m_small_buffer_threshold) {
            category = &m_small_buffers;
        } else if (min_size > m_medium_buffer_threshold) {
            category = &m_large_buffers;
        }

        // Find a suitable buffer from the pool
        for (auto it = category->begin(); it != category->end(); ++it) {
            if (it->capacity() >= min_size) {
                std::vector<T> buffer = std::move(*it);
                category->erase(it);
                buffer.clear(); // Clear contents but keep capacity
                m_buffer_hits++;
                m_buffer_reuse_count++;
                return buffer;
            }
        }

        // No suitable buffer found in the right category, try other categories
        if (category != &m_large_buffers) {
            for (auto it = m_large_buffers.begin(); it != m_large_buffers.end(); ++it) {
                if (it->capacity() >= min_size) {
                    std::vector<T> buffer = std::move(*it);
                    m_large_buffers.erase(it);
                    buffer.clear();
                    m_buffer_hits++;
                    m_buffer_reuse_count++;
                    return buffer;
                }
            }
        }

        if (category != &m_medium_buffers && min_size >= m_small_buffer_threshold) {
            for (auto it = m_medium_buffers.begin(); it != m_medium_buffers.end(); ++it) {
                if (it->capacity() >= min_size) {
                    std::vector<T> buffer = std::move(*it);
                    m_medium_buffers.erase(it);
                    buffer.clear();
                    m_buffer_hits++;
                    m_buffer_reuse_count++;
                    return buffer;
                }
            }
        }

        // No suitable buffer found, create new one
        m_buffer_misses++;
        std::vector<T> buffer;

        buffer.reserve(calculateRoundedSize(target_size));
        return buffer;
    }

    /**
     * @brief Return a buffer to the pool for reuse
     * @param buffer Buffer to return (moved)
     */
    void returnBuffer(std::vector<T>&& buffer) {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Only pool buffers that are reasonably sized
        if (!shouldPoolBuffer(buffer.capacity())) {
            return; // Let the buffer be destroyed naturally
        }

        // Determine which category to return to
        std::vector<std::vector<T>>* category = &m_medium_buffers;
        size_t capacity = buffer.capacity();

        if (capacity < m_small_buffer_threshold) {
            category = &m_small_buffers;
        } else if (capacity > m_medium_buffer_threshold) {
            category = &m_large_buffers;
        }

        // Check if we've reached the limit for this category
        size_t max_buffers = getMaxPooledBuffers();
        size_t category_max = max_buffers / 3; // Distribute evenly among categories

        if (category->size() < category_max) {
            buffer.clear(); // Clear contents but keep capacity
            category->push_back(std::move(buffer));
        }
        // Otherwise, let the buffer be destroyed naturally
    }

    /**
     * @brief Clear all pooled buffers
     */
    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_small_buffers.clear();
        m_medium_buffers.clear();
        m_large_buffers.clear();
    }

    /**
     * @brief Set memory pressure level (0-100)
     * @param pressure_level Memory pressure level
     */
    void setMemoryPressure(int pressure_level) {
        m_memory_pressure = std::max(0, std::min(100, pressure_level));

        // If memory pressure is high, proactively reduce pool size
        if (m_memory_pressure > 70) {
            std::lock_guard<std::mutex> lock(m_mutex);

            // Keep only half of the buffers in each category
            if (!m_small_buffers.empty()) {
                m_small_buffers.resize(m_small_buffers.size() / 2);
            }

            if (!m_medium_buffers.empty()) {
                m_medium_buffers.resize(m_medium_buffers.size() / 2);
            }

            if (!m_large_buffers.empty()) {
                m_large_buffers.resize(m_large_buffers.size() / 2);
            }
        }
    }

    /**
     * @brief Get current memory pressure level
     * @return Memory pressure level (0-100)
     */
    int getMemoryPressure() const {
        return m_memory_pressure;
    }

    /**
     * @brief Get pool statistics
     * @return Pool statistics structure
     */
    PoolStats getStats() const {
        std::lock_guard<std::mutex> lock(m_mutex);

        PoolStats stats{};
        stats.total_buffers = m_small_buffers.size() + m_medium_buffers.size() + m_large_buffers.size();
        stats.largest_buffer_size = 0;
        stats.smallest_buffer_size = static_cast<size_t>(-1);
        stats.total_memory_bytes = 0;
        stats.total_samples = 0;
        stats.buffer_hits = m_buffer_hits;
        stats.buffer_misses = m_buffer_misses;
        stats.memory_pressure = m_memory_pressure;
        stats.reuse_count = m_buffer_reuse_count;

        auto process_category = [&stats](const std::vector<std::vector<T>>& category) {
            for (const auto& buffer : category) {
                size_t capacity = buffer.capacity();
                stats.total_memory_bytes += capacity;
                stats.total_samples += capacity;
                stats.largest_buffer_size = std::max(stats.largest_buffer_size, capacity);
                stats.smallest_buffer_size = std::min(stats.smallest_buffer_size, capacity);
            }
        };

        process_category(m_small_buffers);
        process_category(m_medium_buffers);
        process_category(m_large_buffers);

        if (stats.total_buffers > 0) {
            stats.average_buffer_size = stats.total_memory_bytes / stats.total_buffers;
        }

        if (stats.smallest_buffer_size == static_cast<size_t>(-1)) {
            stats.smallest_buffer_size = 0;
        }

        // Calculate hit ratio
        size_t total_requests = stats.buffer_hits + stats.buffer_misses;
        stats.hit_ratio = total_requests > 0 ? static_cast<float>(stats.buffer_hits) / total_requests : 0.0f;

        return stats;
    }

protected:
    EnhancedTemplateBufferPool()
        : m_small_buffer_threshold(4096)
        , m_medium_buffer_threshold(32768)
        , m_default_max_pooled_buffers(16)
        , m_default_max_buffer_size(192 * 1024)
        , m_cleanup_interval(30)
        , m_min_pool_size(1024)
        , m_min_memory_pressure_reduce(4)
        , m_pressure_reduction_val(48 * 1024)
        , m_memory_pressure(0)
        , m_buffer_hits(0)
        , m_buffer_misses(0)
        , m_buffer_reuse_count(0)
        , m_last_cleanup(std::chrono::steady_clock::now()) {
    }

    // Disable copy constructor and assignment
    EnhancedTemplateBufferPool(const EnhancedTemplateBufferPool&) = delete;
    EnhancedTemplateBufferPool& operator=(const EnhancedTemplateBufferPool&) = delete;

    // Buffer size thresholds
    size_t m_small_buffer_threshold;
    size_t m_medium_buffer_threshold;

    // Pool configuration constants
    size_t m_default_max_pooled_buffers;
    size_t m_default_max_buffer_size;
    std::chrono::seconds m_cleanup_interval;

    // Memory limit sizes
    size_t m_min_pool_size;
    size_t m_min_memory_pressure_reduce;
    size_t m_pressure_reduction_val;

    // Buffer pools by size category
    std::vector<std::vector<T>> m_small_buffers;
    std::vector<std::vector<T>> m_medium_buffers;
    std::vector<std::vector<T>> m_large_buffers;

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
     * @brief Calculate optimal buffer size
     */
    virtual size_t calculateRoundedSize(size_t target_size) const = 0;

    /**
     * @brief Get maximum number of pooled buffers based on memory pressure
     * @return Maximum pooled buffers
     */
    size_t getMaxPooledBuffers() const {
        return m_default_max_pooled_buffers - ((m_default_max_pooled_buffers - m_min_memory_pressure_reduce) * m_memory_pressure) / 100;
    }

    /**
     * @brief Get maximum buffer size based on memory pressure
     * @return Maximum buffer size
     */
    size_t getMaxBufferSize() const {
        return m_default_max_buffer_size - ((m_default_max_buffer_size - m_pressure_reduction_val) * m_memory_pressure) / 100;
    }

    /**
     * @brief Check if a buffer should be pooled
     * @param capacity Buffer capacity
     * @return true if buffer should be pooled
     */
    bool shouldPoolBuffer(size_t capacity) const {
        // Don't pool tiny buffers
        if (capacity < m_min_pool_size) {
            return false;
        }

        // Don't pool buffers larger than the current max
        if (capacity > getMaxBufferSize()) {
            return false;
        }

        // Under high memory pressure, be more selective
        if (m_memory_pressure > 70 && capacity > m_medium_buffer_threshold) {
            return false;
        }

        return true;
    }

    /**
     * @brief Perform periodic cleanup of unused buffers
     */
    void performPeriodicCleanup() {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - m_last_cleanup) < m_cleanup_interval) {
            return; // Not time for cleanup yet
        }

        m_last_cleanup = now;

        // Clean up unused buffer sizes
        auto it = m_size_usage_stats.begin();
        while (it != m_size_usage_stats.end()) {
            // Remove stats for buffer sizes that haven't been used in a while
            if (std::chrono::duration_cast<std::chrono::minutes>(now - it->second.last_request).count() > 10) {
                it = m_size_usage_stats.erase(it);
            } else {
                ++it;
            }
        }

        // If memory pressure is moderate or higher, be more aggressive with cleanup
        if (m_memory_pressure >= 50) {
            // Remove least recently used buffers from each category
            if (m_small_buffers.size() > 2) {
                m_small_buffers.resize(m_small_buffers.size() * 3 / 4);
            }

            if (m_medium_buffers.size() > 2) {
                m_medium_buffers.resize(m_medium_buffers.size() * 3 / 4);
            }

            if (m_large_buffers.size() > 2) {
                m_large_buffers.resize(m_large_buffers.size() * 3 / 4);
            }
        }
    }
};

} // namespace IO
} // namespace PsyMP3

#endif // ENHANCEDTEMPLATEBUFFERPOOL_H
