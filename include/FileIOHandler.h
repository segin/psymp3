/*
 * FileIOHandler.h - Concrete IOHandler for local file access.
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

#ifndef FILEIOHANDLER_H
#define FILEIOHANDLER_H

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Concrete IOHandler implementation for local file access
 * 
 * This class provides access to local files with cross-platform support
 * for Unicode filenames and large files (>2GB).
 */
class FileIOHandler : public IOHandler {
public:
    /**
     * @brief Constructs a FileIOHandler for a given local file path
     * @param path The file path to open with Unicode support
     * @throws InvalidMediaException if the file cannot be opened
     */
    explicit FileIOHandler(const TagLib::String& path);
    
    /**
     * @brief Destroys the FileIOHandler and closes the file
     */
    ~FileIOHandler() override;
    
    /**
     * @brief Read data from the file with fread-like semantics
     * @param buffer Buffer to read data into
     * @param size Size of each element to read
     * @param count Number of elements to read
     * @return Number of elements successfully read
     */
    size_t read(void* buffer, size_t size, size_t count) override;
    
    /**
     * @brief Seek to a position in the file
     * @param offset Offset to seek to (off_t for large file support)
     * @param whence SEEK_SET, SEEK_CUR, or SEEK_END positioning mode
     * @return 0 on success, -1 on failure
     */
    int seek(off_t offset, int whence) override;
    
    /**
     * @brief Get current byte offset position in the file
     * @return Current position as off_t for large file support, -1 on failure
     */
    off_t tell() override;
    
    /**
     * @brief Close the file and cleanup resources
     * @return 0 on success, standard error codes on failure
     */
    int close() override;
    
    /**
     * @brief Check if at end-of-file condition
     * @return true if at end of file, false otherwise
     */
    bool eof() override;
    
    /**
     * @brief Get total size of the file in bytes
     * @return Size in bytes, or -1 if unknown
     */
    off_t getFileSize() override;

private:
    // Private unlocked methods for thread-safe implementation
    
    /**
     * @brief Read data from the file (unlocked version)
     * @param buffer Buffer to read data into
     * @param size Size of each element to read
     * @param count Number of elements to read
     * @return Number of elements successfully read
     */
    size_t read_unlocked(void* buffer, size_t size, size_t count) override;
    
    /**
     * @brief Seek to a position in the file (unlocked version)
     * @param offset Offset to seek to
     * @param whence SEEK_SET, SEEK_CUR, or SEEK_END positioning mode
     * @return 0 on success, -1 on failure
     */
    int seek_unlocked(off_t offset, int whence) override;
    
    /**
     * @brief Get current byte offset position in the file (unlocked version)
     * @return Current position as off_t, -1 on failure
     */
    off_t tell_unlocked() override;
    
    /**
     * @brief Close the file and cleanup resources (unlocked version)
     * @return 0 on success, standard error codes on failure
     */
    int close_unlocked() override;

private:
    RAIIFileHandle m_file_handle;   // RAII-managed file handle for I/O operations
    TagLib::String m_file_path;     // Original file path for error reporting
    
    // Internal method for constructor use (no locks)
    off_t getFileSizeInternal();
    
    // Internal tell method that assumes file mutex is already held
    off_t tell_internal();
    
    // Thread safety for file operations
    mutable std::mutex m_file_mutex;        // Protects file handle operations
    mutable std::shared_mutex m_buffer_mutex;  // Protects buffer operations (allows concurrent reads)
    
    // Performance optimization members
    IOBufferPool::Buffer m_read_buffer;     // Internal read buffer for performance (from pool)
    size_t m_buffer_size = 64 * 1024;       // Default 64KB buffer size
    off_t m_buffer_file_position = -1;      // File position of buffer start
    size_t m_buffer_valid_bytes = 0;        // Number of valid bytes in buffer
    size_t m_buffer_offset = 0;             // Current offset within buffer
    
    // Read-ahead optimization
    bool m_read_ahead_enabled = true;       // Enable read-ahead optimization
    size_t m_read_ahead_size = 128 * 1024;  // Read-ahead buffer size (128KB)
    off_t m_last_read_position = -1;        // Track sequential access patterns
    bool m_sequential_access = false;       // Detected sequential access pattern
    
    // Seeking optimization
    off_t m_cached_file_size = -1;          // Cached file size to avoid repeated fstat calls
    
    // Error handling and recovery
    int m_retry_count = 0;                  // Current retry count for operations
    std::chrono::steady_clock::time_point m_last_error_time;  // Time of last error for rate limiting
    std::chrono::steady_clock::time_point m_operation_start_time;  // Start time for timeout detection
    bool m_timeout_enabled = true;          // Enable timeout handling
    int m_default_timeout_seconds = 30;     // Default timeout for file operations
    
    // Permission and access validation
    bool m_write_access_checked = false;    // Whether write access has been validated
    bool m_has_write_access = false;        // Whether file has write access (for future extensions)
    
    /**
     * @brief Validate that the file handle is in a usable state
     * @return true if handle is valid and file is open, false otherwise
     */
    bool validateFileHandle() const;
    
    /**
     * @brief Attempt to recover from certain error conditions
     * @return true if recovery was successful, false otherwise
     */
    bool attemptErrorRecovery();
    
    /**
     * @brief Validate file operation parameters and preconditions
     * @param buffer Buffer pointer for read operations (can be null for other operations)
     * @param size Element size for read operations (can be 0 for other operations)
     * @param count Element count for read operations (can be 0 for other operations)
     * @param operation_name Name of the operation for error reporting
     * @return true if parameters are valid, false otherwise
     */
    bool validateOperationParameters(const void* buffer, size_t size, size_t count, const std::string& operation_name);
    
    /**
     * @brief Handle timeout conditions for network file systems and slow storage
     * @param operation_name Name of the operation that timed out
     * @param timeout_seconds Timeout duration in seconds
     * @return true if timeout was handled gracefully, false if operation should fail
     */
    bool handleTimeout(const std::string& operation_name, int timeout_seconds = 30);
    
    /**
     * @brief Get specific error message for file operation failures
     * @param error_code System error code
     * @param operation_name Name of the operation that failed
     * @param additional_context Additional context for the error
     * @return Descriptive error message
     */
    std::string getFileOperationErrorMessage(int error_code, const std::string& operation_name, const std::string& additional_context = "");
    
    /**
     * @brief Check if current error condition is recoverable for file operations
     * @param error_code System error code
     * @param operation_name Name of the operation that failed
     * @return true if error might be recoverable, false otherwise
     */
    bool isFileErrorRecoverable(int error_code, const std::string& operation_name);
    
    /**
     * @brief Perform retry logic for recoverable file operation errors
     * @param operation_func Function to retry (returns true on success)
     * @param operation_name Name of the operation for logging
     * @param max_retries Maximum number of retry attempts (default: 3)
     * @param retry_delay_ms Delay between retries in milliseconds (default: 100)
     * @return true if operation succeeded after retries, false otherwise
     */
    bool retryFileOperation(std::function<bool()> operation_func, const std::string& operation_name, int max_retries = 3, int retry_delay_ms = 100);
    
    /**
     * @brief Fill internal buffer with data from file
     * @param file_position Position in file to start reading from
     * @param min_bytes Minimum number of bytes to read
     * @return true if buffer was filled successfully, false on error
     */
    bool fillBuffer(off_t file_position, size_t min_bytes = 0);
    
    /**
     * @brief Read data from internal buffer
     * @param buffer Destination buffer
     * @param bytes_requested Number of bytes to read
     * @return Number of bytes actually read from buffer
     */
    size_t readFromBuffer(void* buffer, size_t bytes_requested);
    
    /**
     * @brief Check if a file position is currently buffered
     * @param file_position Position to check
     * @return true if position is in current buffer, false otherwise
     */
    bool isPositionBuffered(off_t file_position) const;
    
    /**
     * @brief Detect and optimize for sequential access patterns
     * @param current_position Current read position
     */
    void updateAccessPattern(off_t current_position);
    
    /**
     * @brief Invalidate internal buffer (call when seeking or on errors)
     */
    void invalidateBuffer();
    
    /**
     * @brief Get optimal buffer size based on file size and access patterns
     * @param file_size Size of the file
     * @return Optimal buffer size in bytes
     */
    size_t getOptimalBufferSize(off_t file_size) const;
    
    /**
     * @brief Optimize buffer pool usage based on access patterns and memory pressure
     * This method analyzes current usage patterns and adjusts buffer pool settings
     * to optimize memory usage and performance
     */
    void optimizeBufferPoolUsage();
    
    /**
     * @brief Handle memory allocation failures specific to file operations
     * @param requested_size Size of the failed allocation
     * @param context Context where the allocation failed
     * @return true if recovery was successful, false otherwise
     */
    bool handleFileMemoryAllocationFailure(size_t requested_size, const std::string& context);
    
    /**
     * @brief Handle file-specific resource exhaustion scenarios
     * @param resource_type Type of resource that was exhausted
     * @param context Context where exhaustion occurred
     * @return true if recovery was successful, false otherwise
     */
    bool handleFileResourceExhaustion(const std::string& resource_type, const std::string& context);
    
    /**
     * @brief Ensure safe cleanup in destructors even during error conditions
     */
    void ensureSafeDestructorCleanup() noexcept;
    
    /**
     * @brief Provide detailed error analysis and recovery suggestions for file operations
     * @param error_code The error code that occurred
     * @param operation_name The operation that failed
     * @param context Additional context about the failure
     * @return Detailed error analysis with recovery suggestions
     */
    std::string analyzeFileError(int error_code, const std::string& operation_name, const std::string& context = "");
    
    /**
     * @brief Check if the file system supports the requested operation
     * @param operation_name The operation to check (e.g., "read", "seek", "tell")
     * @return true if operation is supported, false otherwise
     */
    bool isOperationSupported(const std::string& operation_name);
    
    /**
     * @brief Detect file system type for optimization and error handling
     * @return String describing the detected file system type
     */
    std::string detectFileSystemType();
};

#endif // FILEIOHANDLER_H