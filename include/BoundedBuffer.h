/*
 * BoundedBuffer.h - Bounded buffer for memory-safe I/O operations
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

#ifndef BOUNDEDBUFFER_H
#define BOUNDEDBUFFER_H

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Bounded buffer for memory-safe I/O operations
 * 
 * This class provides a memory-safe buffer with a maximum size limit
 * to prevent excessive memory usage. It uses the MemoryPoolManager for
 * efficient buffer allocation and reuse.
 */
class BoundedBuffer {
public:
    /**
     * @brief Construct a bounded buffer with maximum size limit
     * @param max_size Maximum size in bytes (0 = unlimited)
     * @param initial_size Initial size to allocate (0 = no initial allocation)
     */
    BoundedBuffer(size_t max_size = 0, size_t initial_size = 0);
    
    /**
     * @brief Destructor - free buffer memory
     */
    ~BoundedBuffer();
    
    /**
     * @brief Resize the buffer
     * @param new_size New size in bytes
     * @return true if resize was successful, false if exceeds max_size
     */
    bool resize(size_t new_size);
    
    /**
     * @brief Reserve capacity for future growth
     * @param capacity Capacity to reserve in bytes
     * @return true if reservation was successful, false if exceeds max_size
     */
    bool reserve(size_t capacity);
    
    /**
     * @brief Shrink capacity to fit current size
     */
    void shrink_to_fit();
    
    /**
     * @brief Append data to the buffer
     * @param data Data to append
     * @param size Size of data in bytes
     * @return true if append was successful, false if would exceed max_size
     */
    bool append(const void* data, size_t size);
    
    /**
     * @brief Set buffer contents
     * @param data Data to set
     * @param size Size of data in bytes
     * @return true if set was successful, false if exceeds max_size
     */
    bool set(const void* data, size_t size);
    
    /**
     * @brief Copy data from buffer to destination
     * @param dest Destination buffer
     * @param offset Offset in buffer to start copying from
     * @param size Maximum number of bytes to copy
     * @return Number of bytes actually copied
     */
    size_t copy_to(void* dest, size_t offset, size_t size) const;
    
    /**
     * @brief Get buffer data pointer (read-only)
     * @return Pointer to buffer data, or nullptr if empty
     */
    const uint8_t* data() const { return m_data; }
    
    /**
     * @brief Get buffer data pointer (read-write)
     * @return Pointer to buffer data, or nullptr if empty
     */
    uint8_t* data() { return m_data; }
    
    /**
     * @brief Get current buffer size
     * @return Size in bytes
     */
    size_t size() const { return m_size; }
    
    /**
     * @brief Get current buffer capacity
     * @return Capacity in bytes
     */
    size_t capacity() const { return m_capacity; }
    
    /**
     * @brief Get maximum buffer size
     * @return Maximum size in bytes (0 = unlimited)
     */
    size_t max_size() const { return m_max_size; }
    
    /**
     * @brief Check if buffer is empty
     * @return true if empty, false otherwise
     */
    bool empty() const { return m_size == 0; }
    
    /**
     * @brief Clear buffer contents (keeps capacity)
     */
    void clear() { m_size = 0; }
    
    /**
     * @brief Get buffer statistics
     * @return Map with buffer statistics
     */
    std::map<std::string, size_t> getStats() const;

private:
    uint8_t* m_data = nullptr;       // Buffer data
    size_t m_size = 0;               // Current size
    size_t m_capacity = 0;           // Current capacity
    size_t m_max_size = 0;           // Maximum size (0 = unlimited)
    
    // Memory tracking
    size_t m_peak_usage = 0;         // Peak memory usage
    size_t m_total_allocations = 0;  // Total number of allocations
    size_t m_total_deallocations = 0; // Total number of deallocations
    
    // Component name for memory pool manager
    static constexpr const char* COMPONENT_NAME = "bounded_buffer";
    
    /**
     * @brief Reallocate buffer to new capacity
     * @param new_capacity New capacity in bytes
     * @return true if reallocation was successful, false otherwise
     */
    bool reallocate(size_t new_capacity);
    
    /**
     * @brief Update memory tracking statistics
     */
    void updateMemoryTracking();
};

/**
 * @brief Bounded circular buffer for streaming I/O operations
 * 
 * This class provides a memory-safe circular buffer with a fixed capacity
 * for efficient streaming I/O operations.
 */
class BoundedCircularBuffer {
public:
    /**
     * @brief Construct a bounded circular buffer with fixed capacity
     * @param capacity Buffer capacity in bytes
     */
    explicit BoundedCircularBuffer(size_t capacity);
    
    /**
     * @brief Destructor - free buffer memory
     */
    ~BoundedCircularBuffer();
    
    /**
     * @brief Write data to the buffer
     * @param data Data to write
     * @param size Size of data in bytes
     * @return Number of bytes actually written
     */
    size_t write(const void* data, size_t size);
    
    /**
     * @brief Read data from the buffer
     * @param data Destination buffer
     * @param size Maximum number of bytes to read
     * @return Number of bytes actually read
     */
    size_t read(void* data, size_t size);
    
    /**
     * @brief Peek at data without removing it
     * @param data Destination buffer
     * @param size Maximum number of bytes to peek
     * @return Number of bytes actually peeked
     */
    size_t peek(void* data, size_t size) const;
    
    /**
     * @brief Skip data in the buffer
     * @param size Number of bytes to skip
     * @return Number of bytes actually skipped
     */
    size_t skip(size_t size);
    
    /**
     * @brief Get number of bytes available for reading
     * @return Number of bytes available
     */
    size_t available() const;
    
    /**
     * @brief Get space available for writing
     * @return Number of bytes available for writing
     */
    size_t space() const;
    
    /**
     * @brief Get buffer capacity
     * @return Buffer capacity in bytes
     */
    size_t capacity() const { return m_capacity; }
    
    /**
     * @brief Check if buffer is empty
     * @return true if empty, false otherwise
     */
    bool empty() const { return m_count == 0; }
    
    /**
     * @brief Check if buffer is full
     * @return true if full, false otherwise
     */
    bool full() const { return m_count == m_capacity; }
    
    /**
     * @brief Clear buffer contents
     */
    void clear();
    
    /**
     * @brief Get buffer statistics
     * @return Map with buffer statistics
     */
    std::map<std::string, size_t> getStats() const;

private:
    uint8_t* m_buffer = nullptr;     // Buffer data
    size_t m_capacity = 0;           // Buffer capacity
    size_t m_count = 0;              // Number of bytes in buffer
    size_t m_read_pos = 0;           // Read position
    size_t m_write_pos = 0;          // Write position
    
    // Memory tracking
    size_t m_peak_usage = 0;         // Peak memory usage
    size_t m_total_bytes_written = 0; // Total bytes written
    size_t m_total_bytes_read = 0;   // Total bytes read
    
    // Component name for memory pool manager
    static constexpr const char* COMPONENT_NAME = "circular_buffer";
};

#endif // BOUNDEDBUFFER_H