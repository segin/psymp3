/*
 * BoundedQueue.h - Thread-safe bounded queue with memory limits
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

#ifndef BOUNDEDQUEUE_H
#define BOUNDEDQUEUE_H

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Thread-safe bounded queue with memory limits
 * 
 * This template class provides a thread-safe queue with configurable
 * limits on both item count and memory usage. It supports non-blocking
 * operations for real-time applications.
 */
template<typename T>
class BoundedQueue {
public:
    /**
     * @brief Constructor
     * @param max_items Maximum number of items (0 = unlimited)
     * @param max_memory_bytes Maximum memory usage in bytes (0 = unlimited)
     * @param memory_calculator Function to calculate memory usage per item
     */
    BoundedQueue(size_t max_items = 0, 
                 size_t max_memory_bytes = 0,
                 std::function<size_t(const T&)> memory_calculator = nullptr)
        : m_max_items(max_items)
        , m_max_memory_bytes(max_memory_bytes)
        , m_memory_calculator(memory_calculator)
        , m_current_memory_bytes(0) {
        
        // Default memory calculator just returns sizeof(T)
        if (!m_memory_calculator) {
            m_memory_calculator = [](const T&) { return sizeof(T); };
        }
    }
    
    /**
     * @brief Destructor
     */
    ~BoundedQueue() = default;
    
    /**
     * @brief Try to push an item (non-blocking)
     * @param item Item to push
     * @return true if item was pushed, false if queue is full
     */
    bool tryPush(T&& item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        size_t item_memory = m_memory_calculator(item);
        
        // Check limits
        if (m_max_items > 0 && m_queue.size() >= m_max_items) {
            return false;
        }
        
        if (m_max_memory_bytes > 0 && (m_current_memory_bytes + item_memory) > m_max_memory_bytes) {
            return false;
        }
        
        // Add item
        m_queue.push(std::move(item));
        m_current_memory_bytes += item_memory;
        
        return true;
    }
    
    /**
     * @brief Try to push an item (non-blocking, copy version)
     * @param item Item to push
     * @return true if item was pushed, false if queue is full
     */
    bool tryPush(const T& item) {
        T copy = item;
        return tryPush(std::move(copy));
    }
    
    /**
     * @brief Try to pop an item (non-blocking)
     * @param item Reference to store the popped item
     * @return true if item was popped, false if queue is empty
     */
    bool tryPop(T& item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_queue.empty()) {
            return false;
        }
        
        item = std::move(m_queue.front());
        m_queue.pop();
        
        // Update memory usage
        size_t item_memory = m_memory_calculator(item);
        m_current_memory_bytes = (m_current_memory_bytes > item_memory) ? 
                                 (m_current_memory_bytes - item_memory) : 0;
        
        return true;
    }
    
    /**
     * @brief Get current queue size
     * @return Number of items in queue
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }
    
    /**
     * @brief Check if queue is empty
     * @return true if empty, false otherwise
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }
    
    /**
     * @brief Clear all items from queue
     */
    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        while (!m_queue.empty()) {
            m_queue.pop();
        }
        m_current_memory_bytes = 0;
    }
    
    /**
     * @brief Get current memory usage
     * @return Memory usage in bytes
     */
    size_t memoryUsage() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_current_memory_bytes;
    }
    
    /**
     * @brief Set maximum number of items
     * @param max_items Maximum items (0 = unlimited)
     */
    void setMaxItems(size_t max_items) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_max_items = max_items;
    }
    
    /**
     * @brief Set maximum memory usage
     * @param max_memory_bytes Maximum memory in bytes (0 = unlimited)
     */
    void setMaxMemoryBytes(size_t max_memory_bytes) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_max_memory_bytes = max_memory_bytes;
    }
    
    /**
     * @brief Get maximum number of items
     * @return Maximum items (0 = unlimited)
     */
    size_t getMaxItems() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_max_items;
    }
    
    /**
     * @brief Get maximum memory usage
     * @return Maximum memory in bytes (0 = unlimited)
     */
    size_t getMaxMemoryBytes() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_max_memory_bytes;
    }

private:
    mutable std::mutex m_mutex;
    std::queue<T> m_queue;
    size_t m_max_items;
    size_t m_max_memory_bytes;
    std::function<size_t(const T&)> m_memory_calculator;
    size_t m_current_memory_bytes;
};

#endif // BOUNDEDQUEUE_H