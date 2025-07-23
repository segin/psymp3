/*
 * MemoryTracker.h - Memory usage tracking and pressure monitoring
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

#ifndef MEMORYTRACKER_H
#define MEMORYTRACKER_H

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Memory usage tracking and pressure monitoring
 * 
 * This class provides system-wide memory usage tracking and pressure monitoring
 * to help optimize memory usage across the application.
 */
class MemoryTracker {
public:
    /**
     * @brief Memory statistics structure
     */
    struct MemoryStats {
        size_t total_physical_memory = 0;
        size_t available_physical_memory = 0;
        size_t process_memory_usage = 0;
        size_t virtual_memory_usage = 0;
        size_t peak_memory_usage = 0;
        float memory_usage_trend = 0.0f;  // MB per second change rate
        std::chrono::steady_clock::time_point last_update;
    };
    
    /**
     * @brief Get the singleton instance of the memory tracker
     * @return Reference to the global memory tracker instance
     */
    static MemoryTracker& getInstance();
    
    /**
     * @brief Update memory statistics
     * This method should be called periodically to update memory usage information
     */
    void update();
    
    /**
     * @brief Get current memory pressure level (0-100)
     * @return Memory pressure level as percentage
     */
    int getMemoryPressureLevel() const;
    
    /**
     * @brief Register a callback for memory pressure changes
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
     * @brief Get current memory statistics
     * @return Current memory statistics
     */
    MemoryStats getStats() const;
    
    /**
     * @brief Start automatic memory tracking
     * @param interval_ms Update interval in milliseconds (default: 5000ms)
     */
    void startAutoTracking(unsigned int interval_ms = 5000);
    
    /**
     * @brief Stop automatic memory tracking
     */
    void stopAutoTracking();
    
    /**
     * @brief Request memory cleanup with specified urgency
     * @param urgency_level Urgency level (0-100)
     */
    void requestMemoryCleanup(int urgency_level);

private:
    MemoryTracker();
    ~MemoryTracker();
    
    // Disable copy constructor and assignment
    MemoryTracker(const MemoryTracker&) = delete;
    MemoryTracker& operator=(const MemoryTracker&) = delete;
    
    /**
     * @brief Callback information structure
     */
    struct CallbackInfo {
        int id;
        std::function<void(int)> callback;
    };
    
    // Memory tracking state
    mutable std::mutex m_mutex;
    MemoryStats m_stats;
    std::atomic<int> m_memory_pressure_level;
    
    // Callback management
    std::vector<CallbackInfo> m_callbacks;
    int m_next_callback_id;
    
    // Auto-tracking
    bool m_auto_tracking_enabled;
    unsigned int m_auto_tracking_interval_ms;
    std::thread m_auto_tracking_thread;
    
    // Memory cleanup management
    bool m_cleanup_requested;
    int m_cleanup_urgency;
    std::chrono::steady_clock::time_point m_last_cleanup_request;
    
    // Memory usage history for trend calculation
    static constexpr size_t MEMORY_HISTORY_SIZE = 10;
    std::deque<std::pair<std::chrono::steady_clock::time_point, size_t>> m_memory_history;
    
    /**
     * @brief Notify all registered callbacks of pressure level change
     */
    void notifyCallbacks();
    
    /**
     * @brief Auto-tracking thread function
     */
    void autoTrackingThread();
    
    /**
     * @brief Calculate memory usage trend from history
     */
    void calculateMemoryTrend();
};

#endif // MEMORYTRACKER_H