/*
 * MemoryOptimizer.h - Memory optimization utilities for demuxer architecture
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

#ifndef MEMORY_OPTIMIZER_H
#define MEMORY_OPTIMIZER_H

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Enhanced buffer pool with adaptive sizing and memory pressure awareness
 */
class EnhancedBufferPool {
public:
    static EnhancedBufferPool& getInstance();
    
    /**
     * @brief Get a buffer of at least the specified size
     * @param min_size Minimum required size
     * @param preferred_size Preferred size (for optimization)
     * @return Reusable buffer vector
     */
    std::vector<uint8_t> getBuffer(size_t min_size, size_t preferred_size = 0);
    
    /**
     * @brief Return a buffer to the pool for reuse
     * @param buffer Buffer to return (will be moved)
     */
    void returnBuffer(std::vector<uint8_t>&& buffer);
    
    /**
     * @brief Clear all pooled buffers (for memory cleanup)
     */
    void clear();
    
    /**
     * @brief Set memory pressure level (0-100)
     * Higher values indicate more aggressive memory conservation
     */
    void setMemoryPressure(int pressure_level);
    
    /**
     * @brief Get current memory pressure level
     */
    int getMemoryPressure() const;
    
    /**
     * @brief Get current pool statistics
     */
    struct PoolStats {
        size_t total_buffers;
        size_t total_memory_bytes;
        size_t largest_buffer_size;
        size_t smallest_buffer_size;
        size_t average_buffer_size;
        size_t buffer_hits;
        size_t buffer_misses;
        int memory_pressure;
        float hit_ratio;
        size_t reuse_count;
    };
    PoolStats getStats() const;
    
private:
    EnhancedBufferPool();
    ~EnhancedBufferPool();
    
    mutable std::mutex m_mutex;
    std::vector<std::vector<uint8_t>> m_buffers;
    std::atomic<int> m_memory_pressure;
    std::atomic<size_t> m_buffer_hits;
    std::atomic<size_t> m_buffer_misses;
    std::atomic<size_t> m_buffer_reuse_count;
    std::chrono::steady_clock::time_point m_last_cleanup;
    
    // Adaptive sizing based on memory pressure
    size_t getMaxPooledBuffers() const;
    size_t getMaxBufferSize() const;
    bool shouldPoolBuffer(size_t capacity) const;
    void performPeriodicCleanup();
    
    // Buffer size categories for more efficient reuse
    std::vector<std::vector<uint8_t>> m_small_buffers;  // < 8KB
    std::vector<std::vector<uint8_t>> m_medium_buffers; // 8KB - 64KB
    std::vector<std::vector<uint8_t>> m_large_buffers;  // 64KB - 1MB
    
    // Usage tracking for adaptive optimization
    struct BufferSizeUsage {
        size_t request_count;
        std::chrono::steady_clock::time_point last_request;
    };
    std::unordered_map<size_t, BufferSizeUsage> m_size_usage_stats;
    
    static constexpr size_t SMALL_BUFFER_THRESHOLD = 8 * 1024;
    static constexpr size_t MEDIUM_BUFFER_THRESHOLD = 64 * 1024;
    static constexpr size_t DEFAULT_MAX_BUFFER_SIZE = 1024 * 1024; // 1MB
    static constexpr size_t DEFAULT_MAX_POOLED_BUFFERS = 32;
    static constexpr std::chrono::seconds CLEANUP_INTERVAL{30}; // Cleanup every 30 seconds
};

/**
 * @brief Enhanced audio buffer pool with adaptive sizing
 */
class EnhancedAudioBufferPool {
public:
    static EnhancedAudioBufferPool& getInstance();
    
    /**
     * @brief Get a sample buffer of at least the specified size
     * @param min_samples Minimum required sample count
     * @param preferred_samples Preferred sample count (for optimization)
     * @return Reusable sample buffer
     */
    std::vector<int16_t> getSampleBuffer(size_t min_samples, size_t preferred_samples = 0);
    
    /**
     * @brief Return a sample buffer to the pool for reuse
     * @param buffer Buffer to return (will be moved)
     */
    void returnSampleBuffer(std::vector<int16_t>&& buffer);
    
    /**
     * @brief Clear all pooled buffers
     */
    void clear();
    
    /**
     * @brief Set memory pressure level (0-100)
     * Higher values indicate more aggressive memory conservation
     */
    void setMemoryPressure(int pressure_level);
    
    /**
     * @brief Get current memory pressure level
     */
    int getMemoryPressure() const;
    
    /**
     * @brief Get current pool statistics
     */
    struct PoolStats {
        size_t total_buffers;
        size_t total_samples;
        size_t largest_buffer_size;
        size_t buffer_hits;
        size_t buffer_misses;
        int memory_pressure;
        float hit_ratio;
        size_t reuse_count;
    };
    PoolStats getStats() const;
    
private:
    EnhancedAudioBufferPool();
    ~EnhancedAudioBufferPool();
    
    mutable std::mutex m_mutex;
    std::vector<std::vector<int16_t>> m_sample_buffers;
    std::atomic<int> m_memory_pressure;
    std::atomic<size_t> m_buffer_hits;
    std::atomic<size_t> m_buffer_misses;
    std::atomic<size_t> m_buffer_reuse_count;
    std::chrono::steady_clock::time_point m_last_cleanup;
    
    // Adaptive sizing based on memory pressure
    size_t getMaxPooledBuffers() const;
    size_t getMaxSamplesPerBuffer() const;
    bool shouldPoolBuffer(size_t capacity) const;
    void performPeriodicCleanup();
    
    // Buffer size categories for more efficient reuse
    std::vector<std::vector<int16_t>> m_small_buffers;  // < 4K samples
    std::vector<std::vector<int16_t>> m_medium_buffers; // 4K - 32K samples
    std::vector<std::vector<int16_t>> m_large_buffers;  // 32K - 192K samples
    
    // Usage tracking for adaptive optimization
    struct BufferSizeUsage {
        size_t request_count;
        std::chrono::steady_clock::time_point last_request;
    };
    std::unordered_map<size_t, BufferSizeUsage> m_size_usage_stats;
    
    static constexpr size_t SMALL_BUFFER_THRESHOLD = 4 * 1024;
    static constexpr size_t MEDIUM_BUFFER_THRESHOLD = 32 * 1024;
    static constexpr size_t DEFAULT_MAX_SAMPLES_PER_BUFFER = 192 * 1024; // ~4 seconds at 48kHz
    static constexpr size_t DEFAULT_MAX_POOLED_BUFFERS = 16;
    static constexpr std::chrono::seconds CLEANUP_INTERVAL{30}; // Cleanup every 30 seconds
};

/**
 * @brief Bounded buffer queue with memory tracking
 * @tparam T Type of elements stored in the queue
 */
template<typename T>
class BoundedQueue {
public:
    /**
     * @brief Constructor
     * @param max_items Maximum number of items in the queue
     * @param max_memory_bytes Maximum memory usage in bytes
     * @param memory_estimator Function to estimate memory usage of an item
     */
    BoundedQueue(size_t max_items, size_t max_memory_bytes,
                 std::function<size_t(const T&)> memory_estimator)
        : m_max_items(max_items)
        , m_max_memory_bytes(max_memory_bytes)
        , m_current_memory_bytes(0)
        , m_memory_estimator(memory_estimator)
        , m_total_items_pushed(0)
        , m_total_items_popped(0)
        , m_total_items_dropped(0)
        , m_adaptive_sizing(true)
        , m_last_resize_time(std::chrono::steady_clock::now())
    {}
    
    /**
     * @brief Push an item to the queue if it doesn't exceed limits
     * @param item Item to push
     * @return true if item was pushed, false if limits would be exceeded
     */
    bool tryPush(const T& item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        size_t item_size = m_memory_estimator(item);
        
        if (m_queue.size() >= m_max_items || 
            m_current_memory_bytes + item_size > m_max_memory_bytes) {
            m_total_items_dropped++;
            return false;
        }
        
        m_queue.push(item);
        m_current_memory_bytes += item_size;
        m_total_items_pushed++;
        
        // Update usage statistics
        updateUsageStats();
        
        return true;
    }
    
    /**
     * @brief Push an item to the queue if it doesn't exceed limits
     * @param item Item to push (will be moved)
     * @return true if item was pushed, false if limits would be exceeded
     */
    bool tryPush(T&& item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        size_t item_size = m_memory_estimator(item);
        
        if (m_queue.size() >= m_max_items || 
            m_current_memory_bytes + item_size > m_max_memory_bytes) {
            m_total_items_dropped++;
            return false;
        }
        
        m_queue.push(std::move(item));
        m_current_memory_bytes += item_size;
        m_total_items_pushed++;
        
        // Update usage statistics
        updateUsageStats();
        
        return true;
    }
    
    /**
     * @brief Pop an item from the queue
     * @param item Reference to store the popped item
     * @return true if an item was popped, false if queue is empty
     */
    bool tryPop(T& item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_queue.empty()) {
            return false;
        }
        
        item = std::move(m_queue.front());
        size_t item_size = m_memory_estimator(item);
        m_queue.pop();
        m_total_items_popped++;
        
        if (item_size <= m_current_memory_bytes) {
            m_current_memory_bytes -= item_size;
        } else {
            m_current_memory_bytes = 0;
        }
        
        // Update usage statistics
        updateUsageStats();
        
        return true;
    }
    
    /**
     * @brief Check if the queue is empty
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }
    
    /**
     * @brief Get the number of items in the queue
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }
    
    /**
     * @brief Get the current memory usage in bytes
     */
    size_t memoryUsage() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_current_memory_bytes;
    }
    
    /**
     * @brief Clear the queue
     */
    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        while (!m_queue.empty()) {
            m_queue.pop();
        }
        
        m_current_memory_bytes = 0;
    }
    
    /**
     * @brief Set the maximum number of items
     */
    void setMaxItems(size_t max_items) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_max_items = max_items;
    }
    
    /**
     * @brief Set the maximum memory usage in bytes
     */
    void setMaxMemoryBytes(size_t max_memory_bytes) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_max_memory_bytes = max_memory_bytes;
    }
    
    /**
     * @brief Enable or disable adaptive sizing
     */
    void setAdaptiveSizing(bool enable) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_adaptive_sizing = enable;
    }
    
    /**
     * @brief Get queue statistics
     */
    struct QueueStats {
        size_t current_items;
        size_t current_memory_bytes;
        size_t max_items;
        size_t max_memory_bytes;
        size_t total_items_pushed;
        size_t total_items_popped;
        size_t total_items_dropped;
        float drop_ratio;
        float fullness_ratio;
        float memory_fullness_ratio;
        float throughput_items_per_sec;
    };
    
    /**
     * @brief Get current queue statistics
     */
    QueueStats getStats() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        QueueStats stats;
        stats.current_items = m_queue.size();
        stats.current_memory_bytes = m_current_memory_bytes;
        stats.max_items = m_max_items;
        stats.max_memory_bytes = m_max_memory_bytes;
        stats.total_items_pushed = m_total_items_pushed;
        stats.total_items_popped = m_total_items_popped;
        stats.total_items_dropped = m_total_items_dropped;
        
        // Calculate drop ratio
        if (stats.total_items_pushed + stats.total_items_dropped > 0) {
            stats.drop_ratio = static_cast<float>(stats.total_items_dropped) / 
                              (stats.total_items_pushed + stats.total_items_dropped);
        } else {
            stats.drop_ratio = 0.0f;
        }
        
        // Calculate fullness ratios
        stats.fullness_ratio = m_max_items > 0 ? 
            static_cast<float>(stats.current_items) / m_max_items : 0.0f;
        
        stats.memory_fullness_ratio = m_max_memory_bytes > 0 ? 
            static_cast<float>(stats.current_memory_bytes) / m_max_memory_bytes : 0.0f;
        
        // Calculate throughput
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            now - m_throughput_start_time).count();
        
        if (duration > 0) {
            stats.throughput_items_per_sec = static_cast<float>(m_total_items_popped) / duration;
        } else {
            stats.throughput_items_per_sec = 0.0f;
        }
        
        return stats;
    }
    
private:
    std::queue<T> m_queue;
    size_t m_max_items;
    size_t m_max_memory_bytes;
    size_t m_current_memory_bytes;
    std::function<size_t(const T&)> m_memory_estimator;
    mutable std::mutex m_mutex;
    
    // Statistics
    size_t m_total_items_pushed;
    size_t m_total_items_popped;
    size_t m_total_items_dropped;
    std::chrono::steady_clock::time_point m_throughput_start_time{std::chrono::steady_clock::now()};
    
    // Adaptive sizing
    bool m_adaptive_sizing;
    std::chrono::steady_clock::time_point m_last_resize_time;
    static constexpr std::chrono::seconds RESIZE_INTERVAL{60}; // Resize check interval
    
    // Usage tracking for adaptive sizing
    struct UsageWindow {
        size_t items_pushed;
        size_t items_popped;
        size_t items_dropped;
        std::chrono::steady_clock::time_point window_start;
    };
    std::deque<UsageWindow> m_usage_windows;
    static constexpr size_t MAX_USAGE_WINDOWS = 5;
    
    /**
     * @brief Update usage statistics and perform adaptive sizing if needed
     */
    void updateUsageStats() {
        if (!m_adaptive_sizing) {
            return;
        }
        
        auto now = std::chrono::steady_clock::now();
        
        // Check if we need to create a new usage window
        if (m_usage_windows.empty() || 
            std::chrono::duration_cast<std::chrono::seconds>(now - m_usage_windows.back().window_start).count() >= 10) {
            
            // Add new window
            UsageWindow window;
            window.items_pushed = 0;
            window.items_popped = 0;
            window.items_dropped = 0;
            window.window_start = now;
            
            m_usage_windows.push_back(window);
            
            // Limit number of windows
            if (m_usage_windows.size() > MAX_USAGE_WINDOWS) {
                m_usage_windows.pop_front();
            }
        }
        
        // Update current window
        if (!m_usage_windows.empty()) {
            UsageWindow& current = m_usage_windows.back();
            current.items_pushed = m_total_items_pushed - 
                (m_usage_windows.size() > 1 ? m_total_items_pushed : 0);
            current.items_popped = m_total_items_popped - 
                (m_usage_windows.size() > 1 ? m_total_items_popped : 0);
            current.items_dropped = m_total_items_dropped - 
                (m_usage_windows.size() > 1 ? m_total_items_dropped : 0);
        }
        
        // Check if we should resize the queue
        if (std::chrono::duration_cast<std::chrono::seconds>(now - m_last_resize_time) >= RESIZE_INTERVAL) {
            considerResize();
            m_last_resize_time = now;
        }
    }
    
    /**
     * @brief Consider resizing the queue based on usage patterns
     */
    void considerResize() {
        if (m_usage_windows.size() < 2) {
            return; // Not enough data
        }
        
        // Calculate drop ratio over recent windows
        size_t total_pushed = 0;
        size_t total_dropped = 0;
        
        for (const auto& window : m_usage_windows) {
            total_pushed += window.items_pushed;
            total_dropped += window.items_dropped;
        }
        
        float drop_ratio = (total_pushed + total_dropped > 0) ? 
            static_cast<float>(total_dropped) / (total_pushed + total_dropped) : 0.0f;
        
        // Calculate average queue fullness
        float avg_fullness = static_cast<float>(m_queue.size()) / m_max_items;
        
        // Adjust queue size based on metrics
        if (drop_ratio > 0.1f && avg_fullness > 0.8f) {
            // High drop rate and high fullness - increase size
            size_t new_max_items = static_cast<size_t>(m_max_items * 1.25f);
            size_t new_max_bytes = static_cast<size_t>(m_max_memory_bytes * 1.25f);
            
            // Cap at reasonable limits
            new_max_items = std::min(new_max_items, m_max_items * 2);
            new_max_bytes = std::min(new_max_bytes, m_max_memory_bytes * 2);
            
            m_max_items = new_max_items;
            m_max_memory_bytes = new_max_bytes;
        } else if (drop_ratio < 0.01f && avg_fullness < 0.3f) {
            // Low drop rate and low fullness - decrease size
            size_t new_max_items = static_cast<size_t>(m_max_items * 0.8f);
            size_t new_max_bytes = static_cast<size_t>(m_max_memory_bytes * 0.8f);
            
            // Ensure minimum sizes
            new_max_items = std::max(new_max_items, size_t(4));
            new_max_bytes = std::max(new_max_bytes, size_t(16384)); // 16KB minimum
            
            m_max_items = new_max_items;
            m_max_memory_bytes = new_max_bytes;
        }
    }
};

/**
 * @brief Memory usage tracker for system-wide memory pressure detection
 */
class MemoryTracker {
public:
    static MemoryTracker& getInstance();
    
    /**
     * @brief Update memory usage statistics
     */
    void update();
    
    /**
     * @brief Get current memory pressure level (0-100)
     * Higher values indicate more memory pressure
     */
    int getMemoryPressureLevel() const;
    
    /**
     * @brief Register a callback for memory pressure changes
     * @param callback Function to call when memory pressure changes
     * @return ID for unregistering the callback
     */
    int registerMemoryPressureCallback(std::function<void(int)> callback);
    
    /**
     * @brief Unregister a memory pressure callback
     * @param id ID returned from registerMemoryPressureCallback
     */
    void unregisterMemoryPressureCallback(int id);
    
    /**
     * @brief Get current memory usage statistics
     */
    struct MemoryStats {
        size_t total_physical_memory;
        size_t available_physical_memory;
        size_t process_memory_usage;
        int memory_pressure_level;
        size_t virtual_memory_usage;
        size_t peak_memory_usage;
        std::chrono::steady_clock::time_point last_update;
        float memory_usage_trend; // Positive values indicate increasing usage
    };
    MemoryStats getStats() const;
    
    /**
     * @brief Start automatic memory tracking
     * @param interval_ms Update interval in milliseconds
     */
    void startAutoTracking(unsigned int interval_ms = 5000);
    
    /**
     * @brief Stop automatic memory tracking
     */
    void stopAutoTracking();
    
    /**
     * @brief Request a memory cleanup operation
     * This will notify all registered components to reduce memory usage
     */
    void requestMemoryCleanup(int urgency_level = 50);
    
private:
    MemoryTracker();
    ~MemoryTracker();
    
    void notifyCallbacks();
    void autoTrackingThread();
    void calculateMemoryTrend();
    
    mutable std::mutex m_mutex;
    std::atomic<int> m_memory_pressure_level;
    MemoryStats m_stats;
    
    struct CallbackInfo {
        int id;
        std::function<void(int)> callback;
    };
    std::vector<CallbackInfo> m_callbacks;
    int m_next_callback_id;
    
    // Auto tracking
    std::atomic<bool> m_auto_tracking_enabled;
    unsigned int m_auto_tracking_interval_ms;
    std::thread m_auto_tracking_thread;
    
    // Memory trend tracking
    std::deque<std::pair<std::chrono::steady_clock::time_point, size_t>> m_memory_history;
    static constexpr size_t MEMORY_HISTORY_SIZE = 10;
    
    // Cleanup request handling
    std::atomic<bool> m_cleanup_requested;
    std::atomic<int> m_cleanup_urgency;
    std::chrono::steady_clock::time_point m_last_cleanup_request;
};

#endif // MEMORY_OPTIMIZER_H