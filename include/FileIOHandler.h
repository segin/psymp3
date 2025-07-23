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
    FILE* m_file_handle = nullptr;  // File handle for I/O operations
    TagLib::String m_file_path;     // Original file path for error reporting
    
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
};

#endif // FILEIOHANDLER_H