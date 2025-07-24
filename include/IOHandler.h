/*
 * IOHandler.h - Abstract I/O handler interface
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

#ifndef IOHANDLER_H
#define IOHANDLER_H

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Base IOHandler interface for unified I/O operations
 * 
 * This class provides a consistent interface for reading media data
 * from various sources including local files, HTTP streams, and other protocols.
 * All concrete implementations must provide virtual destructor for proper cleanup.
 */
class IOHandler {
public:
    /**
     * @brief Constructor for IOHandler base class
     */
    IOHandler();
    
    /**
     * @brief Virtual destructor for proper polymorphic cleanup
     */
    virtual ~IOHandler();
    
    /**
     * @brief Read data from the source with fread-like semantics
     * @param buffer Buffer to read data into
     * @param size Size of each element to read
     * @param count Number of elements to read
     * @return Number of elements successfully read
     */
    virtual size_t read(void* buffer, size_t size, size_t count);
    
    /**
     * @brief Seek to a position in the source
     * @param offset Offset to seek to (off_t for large file support)
     * @param whence SEEK_SET, SEEK_CUR, or SEEK_END positioning mode
     * @return 0 on success, -1 on failure
     */
    virtual int seek(off_t offset, int whence);
    
    /**
     * @brief Get current byte offset position
     * @param return Current position as off_t for large file support, -1 on failure
     */
    virtual off_t tell();
    
    /**
     * @brief Close the I/O source and cleanup resources
     * @return 0 on success, standard error codes on failure
     */
    virtual int close();
    
    /**
     * @brief Check if at end-of-stream condition
     * @return true if at end of stream, false otherwise
     */
    virtual bool eof();
    
    /**
     * @brief Get total size of the source in bytes
     * @return Size in bytes, or -1 if unknown
     */
    virtual off_t getFileSize();
    
    /**
     * @brief Get the last error code
     * @return Error code (0 = no error)
     */
    virtual int getLastError() const;

protected:
    /**
     * @brief Cross-platform utility methods for consistent behavior
     */
    
    /**
     * @brief Normalize path separators for the current platform
     * @param path The path to normalize
     * @return Normalized path with platform-appropriate separators
     */
    static std::string normalizePath(const std::string& path);
    
    /**
     * @brief Get platform-appropriate path separator
     * @return Path separator character ('\\' on Windows, '/' on Unix)
     */
    static char getPathSeparator();
    
    /**
     * @brief Convert error code to consistent error message across platforms
     * @param error_code The error code to convert
     * @param context Additional context for the error
     * @return Descriptive error message
     */
    static std::string getErrorMessage(int error_code, const std::string& context = "");
    
    /**
     * @brief Check if the given error code represents a temporary/recoverable error
     * @param error_code The error code to check
     * @return true if error is potentially recoverable, false otherwise
     */
    static bool isRecoverableError(int error_code);
    
    /**
     * @brief Get maximum file size supported on current platform
     * @return Maximum file size in bytes
     */
    static off_t getMaxFileSize();
    
public:
    /**
     * @brief Get current memory usage statistics for all IOHandlers
     * @return Map with memory usage statistics
     */
    static std::map<std::string, size_t> getMemoryStats();

protected:
    
    /**
     * @brief Set global memory limits for IOHandler operations
     * @param max_total_memory Maximum total memory for all IOHandlers (default: 64MB)
     * @param max_per_handler Maximum memory per IOHandler instance (default: 16MB)
     */
    static void setMemoryLimits(size_t max_total_memory, size_t max_per_handler);
    
    /**
     * @brief Perform global memory optimization across all IOHandlers
     * This method analyzes current memory usage and performs appropriate optimizations
     * based on memory pressure levels
     */
    static void performMemoryOptimization();

protected:
    /**
     * @brief Common state tracking for derived classes
     */
    bool m_closed = false;   // Indicates if the handler is closed
    bool m_eof = false;      // Indicates end-of-stream condition
    off_t m_position = 0;    // Current byte offset position
    int m_error = 0;         // Last error code (0 = no error)
    
    // Memory usage tracking
    size_t m_memory_usage = 0;  // Current memory usage by this handler
    
    /**
     * @brief Update memory usage tracking
     * @param new_usage New memory usage in bytes
     */
    void updateMemoryUsage(size_t new_usage);
    
    /**
     * @brief Check if memory usage is within limits
     * @param additional_bytes Additional bytes to be allocated
     * @return true if allocation is within limits, false otherwise
     */
    bool checkMemoryLimits(size_t additional_bytes) const;
    
    /**
     * @brief Handle memory allocation failures with recovery mechanisms
     * @param requested_size Size of the failed allocation
     * @param context Context where the allocation failed
     * @return true if recovery was successful, false otherwise
     */
    bool handleMemoryAllocationFailure(size_t requested_size, const std::string& context);
    
    /**
     * @brief Handle resource exhaustion scenarios
     * @param resource_type Type of resource that was exhausted
     * @param context Context where exhaustion occurred
     * @return true if recovery was successful, false otherwise
     */
    bool handleResourceExhaustion(const std::string& resource_type, const std::string& context);
    
    /**
     * @brief Safely propagate errors without resource leaks
     * @param error_code Error code to propagate
     * @param error_message Error message
     * @param cleanup_func Optional cleanup function to call before propagation
     */
    void safeErrorPropagation(int error_code, const std::string& error_message, 
                             std::function<void()> cleanup_func = nullptr);

private:
    // Global memory tracking
    static std::mutex s_memory_mutex;
    static size_t s_total_memory_usage;
    static size_t s_max_total_memory;
    static size_t s_max_per_handler_memory;
    static size_t s_active_handlers;
    static std::chrono::steady_clock::time_point s_last_memory_warning;
};

#endif // IOHANDLER_H