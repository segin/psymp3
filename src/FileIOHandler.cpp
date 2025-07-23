/*
 * FileIOHandler.cpp - Implementation for the file I/O handler.
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

#include "psymp3.h"

/**
 * @brief Constructs a FileIOHandler for a given local file path.
 *
 * This opens the specified file in binary read mode. It handles both
 * standard and wide-character paths for cross-platform compatibility.
 * 
 * @param path The file path to open with Unicode support
 * @throws InvalidMediaException if the file cannot be opened
 */
FileIOHandler::FileIOHandler(const TagLib::String& path) : m_file_path(path) {
    // Reset base class state
    m_closed = false;
    m_eof = false;
    m_position = 0;
    m_error = 0;
    
    // Initialize performance optimization state
    m_buffer_file_position = -1;
    m_buffer_valid_bytes = 0;
    m_buffer_offset = 0;
    m_last_read_position = -1;
    m_sequential_access = false;
    m_cached_file_size = -1;
    
    // Register with global memory tracking
    {
        std::lock_guard<std::mutex> lock(s_memory_mutex);
        s_active_handlers++;
    }
    

    
    // Normalize the path for consistent cross-platform handling
    std::string normalized_path = normalizePath(path.to8Bit(false));
    Debug::log("io", "FileIOHandler::FileIOHandler() - Normalized path: ", normalized_path);
    
    // Platform-specific file opening with Unicode support
#ifdef _WIN32
    // Windows: Use wide character API for proper Unicode support
    m_file_handle = _wfopen(path.toCWString(), L"rb");
    
    // Handle file open failure with Windows-specific error handling
    if (!m_file_handle) {
        // Get system error code - Windows uses errno for file operations
        m_error = errno;
        
        // Additional Windows-specific error handling
        DWORD win_error = GetLastError();
        std::string win_error_msg;
        
        // Translate common Windows errors to more descriptive messages
        switch (win_error) {
            case ERROR_FILE_NOT_FOUND:
                win_error_msg = "File not found";
                break;
            case ERROR_PATH_NOT_FOUND:
                win_error_msg = "Path not found";
                break;
            case ERROR_ACCESS_DENIED:
                win_error_msg = "Access denied";
                break;
            case ERROR_SHARING_VIOLATION:
                win_error_msg = "File is being used by another process";
                break;
            case ERROR_LOCK_VIOLATION:
                win_error_msg = "File is locked";
                break;
            case ERROR_DISK_FULL:
                win_error_msg = "Disk full";
                break;
            case ERROR_INVALID_NAME:
                win_error_msg = "Invalid filename";
                break;
            case ERROR_TOO_MANY_OPEN_FILES:
                win_error_msg = "Too many open files";
                break;
            default:
                // Use FormatMessage for other Windows errors
                LPSTR messageBuffer = nullptr;
                size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                           NULL, win_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
                if (size > 0 && messageBuffer) {
                    win_error_msg = std::string(messageBuffer, size);
                    // Remove trailing newlines
                    while (!win_error_msg.empty() && (win_error_msg.back() == '\n' || win_error_msg.back() == '\r')) {
                        win_error_msg.pop_back();
                    }
                    LocalFree(messageBuffer);
                } else {
                    win_error_msg = "Unknown Windows error " + std::to_string(win_error);
                }
                break;
        }
        
        // Create descriptive error message with both errno and Windows error details
        std::string errorMsg = "Could not open file: " + path.to8Bit(false) + " (";
        errorMsg += strerror(errno);
        if (!win_error_msg.empty()) {
            errorMsg += ", Windows: " + win_error_msg;
        }
        errorMsg += ")";
        
        // Log the error with Windows-specific details
        Debug::log("io", "FileIOHandler::FileIOHandler() - Windows error: ", win_error, " (", win_error_msg, ")");
        Debug::log("io", "FileIOHandler::FileIOHandler() - ", errorMsg);
        
        // Throw exception with detailed error message
        throw InvalidMediaException(errorMsg);
    }
#else
    // Unix/Linux: Use raw C string without UTF-8 conversion to preserve original filesystem encoding
    m_file_handle = fopen(path.toCString(false), "rb");

    // Handle file open failure
    if (!m_file_handle) {
        // Get system error code
        m_error = errno;
        
        // Use cross-platform error message utility
        std::string errorMsg = getErrorMessage(errno, "Could not open file: " + normalized_path);
        
        // Log the error with cross-platform consistency
        Debug::log("io", "FileIOHandler::FileIOHandler() - ", errorMsg);
        
        // Check if this is a recoverable error
        if (isRecoverableError(errno)) {
            Debug::log("io", "FileIOHandler::FileIOHandler() - Error may be recoverable: ", errno);
        }
        
        // Throw exception with detailed error message
        throw InvalidMediaException(errorMsg);
    }
#endif
    
    // Log successful file opening
    Debug::log("io", "FileIOHandler::FileIOHandler() - Successfully opened file: ", path.to8Bit(false));
    
    // Get and log file size for debugging and optimization
    off_t fileSize = getFileSize();
    if (fileSize >= 0) {
        m_cached_file_size = fileSize;  // Cache for performance
        Debug::log("io", "FileIOHandler::FileIOHandler() - File size: ", fileSize, 
                  " bytes (", std::hex, fileSize, std::dec, ")");
        
        // Optimize buffer size based on file size
        m_buffer_size = getOptimalBufferSize(fileSize);
        Debug::log("io", "FileIOHandler::FileIOHandler() - Optimal buffer size: ", m_buffer_size, " bytes");
        
        // Pre-allocate buffer from pool for performance
        if (checkMemoryLimits(m_buffer_size)) {
            m_read_buffer = IOBufferPool::getInstance().acquire(m_buffer_size);
            if (!m_read_buffer.empty()) {
                updateMemoryUsage(m_read_buffer.size());
                Debug::log("io", "FileIOHandler::FileIOHandler() - Buffer acquired from pool successfully");
            } else {
                Debug::log("io", "FileIOHandler::FileIOHandler() - Warning: Could not acquire buffer from pool");
                // Fall back to smaller buffer size
                m_buffer_size = std::min(m_buffer_size, static_cast<size_t>(16 * 1024)); // 16KB fallback
                if (checkMemoryLimits(m_buffer_size)) {
                    m_read_buffer = IOBufferPool::getInstance().acquire(m_buffer_size);
                    if (!m_read_buffer.empty()) {
                        updateMemoryUsage(m_read_buffer.size());
                    }
                }
            }
        } else {
            Debug::log("io", "FileIOHandler::FileIOHandler() - Memory limit would be exceeded, using smaller buffer");
            m_buffer_size = std::min(m_buffer_size, static_cast<size_t>(8 * 1024)); // 8KB fallback
        }
        
        // Check against platform maximum file size
        off_t maxFileSize = getMaxFileSize();
        if (fileSize > maxFileSize) {
            Debug::log("io", "FileIOHandler::FileIOHandler() - Warning: File size exceeds platform maximum: ", 
                      fileSize, " > ", maxFileSize);
        }
        
        // Log warning for extremely large files that might cause issues
        static const off_t LARGE_FILE_WARNING_SIZE = 1LL << 32; // 4GB
        if (fileSize > LARGE_FILE_WARNING_SIZE) {
            Debug::log("io", "FileIOHandler::FileIOHandler() - Warning: Very large file (>4GB), ensure adequate system resources");
        }
    } else {
        Debug::log("io", "FileIOHandler::FileIOHandler() - Warning: Could not determine file size");
        // Use default buffer size for unknown file sizes
        if (checkMemoryLimits(m_buffer_size)) {
            m_read_buffer = IOBufferPool::getInstance().acquire(m_buffer_size);
            if (!m_read_buffer.empty()) {
                updateMemoryUsage(m_read_buffer.size());
            } else {
                Debug::log("io", "FileIOHandler::FileIOHandler() - Warning: Could not acquire default buffer from pool");
                m_buffer_size = 8 * 1024; // 8KB minimal fallback
                if (checkMemoryLimits(m_buffer_size)) {
                    m_read_buffer = IOBufferPool::getInstance().acquire(m_buffer_size);
                    if (!m_read_buffer.empty()) {
                        updateMemoryUsage(m_read_buffer.size());
                    }
                }
            }
        }
    }
}

/**
 * @brief Destroys the FileIOHandler object.
 *
 * This ensures the underlying file handle is closed properly.
 */
FileIOHandler::~FileIOHandler() {
    // Close the file if it's still open
    close();
}

/**
 * @brief Reads data from the file.
 *
 * This is a direct wrapper around the standard C fread function.
 * 
 * @param buffer Buffer to read data into
 * @param size Size of each element to read
 * @param count Number of elements to read
 * @return Number of elements successfully read
 */
size_t FileIOHandler::read(void* buffer, size_t size, size_t count) {
    // Reset error state
    m_error = 0;
    
    // Validate file handle state
    if (!validateFileHandle()) {
        m_error = EBADF;  // Bad file descriptor
        return 0;
    }
    
    // Validate parameters
    if (!buffer) {
        m_error = EINVAL; // Invalid argument
        Debug::log("io", "FileIOHandler::read() - Invalid buffer parameter (null)");
        return 0;
    }
    
    if (size == 0 || count == 0) {
        // Not an error, just nothing to do
        Debug::log("io", "FileIOHandler::read() - Zero size or count requested: size=", size, " count=", count);
        return 0;
    }
    
    // Additional validation: check for reasonable parameter values
    static const size_t MAX_REASONABLE_SIZE = 1024 * 1024 * 1024; // 1GB per element
    static const size_t MAX_REASONABLE_COUNT = SIZE_MAX / 1024;   // Reasonable count limit
    
    if (size > MAX_REASONABLE_SIZE) {
        m_error = EINVAL;
        Debug::log("io", "FileIOHandler::read() - Unreasonably large element size: ", size);
        return 0;
    }
    
    if (count > MAX_REASONABLE_COUNT) {
        m_error = EINVAL;
        Debug::log("io", "FileIOHandler::read() - Unreasonably large element count: ", count);
        return 0;
    }
    
    // Check for potential integer overflow in size calculation
    if (size > 0 && count > SIZE_MAX / size) {
        m_error = EOVERFLOW;  // Value too large for defined data type
        Debug::log("io", "FileIOHandler::read() - Integer overflow prevented: size=", size, " count=", count);
        return 0;
    }
    
    size_t bytes_requested = size * count;
    size_t total_bytes_read = 0;
    uint8_t* dest_buffer = static_cast<uint8_t*>(buffer);
    
    // Update access pattern tracking for optimization
    updateAccessPattern(m_position);
    
    // Periodic buffer pool optimization
    static size_t read_counter = 0;
    read_counter++;
    if (read_counter % 100 == 0) { // Every 100 reads
        optimizeBufferPoolUsage();
    }
    
    Debug::log("io", "FileIOHandler::read() - Reading ", bytes_requested, " bytes at position ", m_position, 
              " (sequential: ", (m_sequential_access ? "yes" : "no"), ")");
    
    // Read data using buffered approach
    while (total_bytes_read < bytes_requested && !m_eof) {
        size_t remaining_bytes = bytes_requested - total_bytes_read;
        
        // Check if current position is buffered
        if (isPositionBuffered(m_position + total_bytes_read)) {
            // Read from buffer
            size_t buffer_bytes_read = readFromBuffer(dest_buffer + total_bytes_read, remaining_bytes);
            total_bytes_read += buffer_bytes_read;
            
            if (buffer_bytes_read == 0) {
                // Buffer exhausted, need to fill it
                break;
            }
        } else {
            // Need to fill buffer
            off_t read_position = m_position + total_bytes_read;
            
            // Determine read size - use read-ahead for sequential access
            size_t read_size = remaining_bytes;
            if (m_sequential_access && m_read_ahead_enabled) {
                read_size = std::max(remaining_bytes, m_read_ahead_size);
            }
            
            if (!fillBuffer(read_position, read_size)) {
                // Buffer fill failed
                if (m_error == 0) {
                    // Check if we hit EOF
                    if (feof(m_file_handle)) {
                        m_eof = true;
                        Debug::log("io", "FileIOHandler::read() - Reached end of file during buffer fill");
                    } else {
                        m_error = ferror(m_file_handle);
                        Debug::log("io", "FileIOHandler::read() - Buffer fill failed: ", strerror(m_error));
                    }
                }
                break;
            }
            
            // Now read from the newly filled buffer
            size_t buffer_bytes_read = readFromBuffer(dest_buffer + total_bytes_read, remaining_bytes);
            total_bytes_read += buffer_bytes_read;
            
            if (buffer_bytes_read == 0) {
                // Still no data available, likely EOF
                m_eof = true;
                break;
            }
        }
    }
    
    // Update position with overflow protection
    static const off_t OFF_T_MAX_VAL = (sizeof(off_t) == 8) ? 0x7FFFFFFFFFFFFFFFLL : 0x7FFFFFFFL;
    if (total_bytes_read > 0 && m_position > OFF_T_MAX_VAL - static_cast<off_t>(total_bytes_read)) {
        Debug::log("io", "FileIOHandler::read() - Position overflow prevented, clamping to maximum");
        m_position = OFF_T_MAX_VAL;
    } else {
        m_position += static_cast<off_t>(total_bytes_read);
    }
    
    // Calculate number of complete elements read
    size_t elements_read = total_bytes_read / size;
    
    Debug::log("io", "FileIOHandler::read() - Read ", total_bytes_read, " bytes (", elements_read, " elements), new position: ", m_position);
    
    return elements_read;
}

/**
 * @brief Seeks to a position in the file.
 *
 * This uses 64-bit file operations (fseeko) to support large files.
 * 
 * @param offset Offset to seek to (off_t for large file support)
 * @param whence SEEK_SET, SEEK_CUR, or SEEK_END positioning mode
 * @return 0 on success, -1 on failure
 */
int FileIOHandler::seek(off_t offset, int whence) {
    // Reset error state
    m_error = 0;
    
    // Validate file handle state
    if (!validateFileHandle()) {
        m_error = EBADF;  // Bad file descriptor
        return -1;
    }
    
    // Validate whence parameter
    if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
        m_error = EINVAL;  // Invalid argument
        return -1;
    }
    
    // Additional validation for large file support
    // Check for potential overflow in SEEK_CUR operations
    if (whence == SEEK_CUR) {
        off_t current_pos = tell();
        if (current_pos < 0) {
            // tell() failed, error already set
            return -1;
        }
        
        // Define max/min values for off_t based on its size
        static const off_t OFF_T_MAX_VAL = (sizeof(off_t) == 8) ? 0x7FFFFFFFFFFFFFFFLL : 0x7FFFFFFFL;
        static const off_t OFF_T_MIN_VAL = (sizeof(off_t) == 8) ? (-0x7FFFFFFFFFFFFFFFLL - 1) : (-0x7FFFFFFFL - 1);
        
        // Check for overflow in addition
        if (offset > 0 && current_pos > OFF_T_MAX_VAL - offset) {
            m_error = EOVERFLOW;  // Value too large for defined data type
            Debug::log("io", "FileIOHandler::seek() - SEEK_CUR overflow prevented: current=", current_pos, " offset=", offset);
            return -1;
        } else if (offset < 0 && current_pos < OFF_T_MIN_VAL - offset) {
            m_error = EOVERFLOW;  // Value too large for defined data type
            Debug::log("io", "FileIOHandler::seek() - SEEK_CUR underflow prevented: current=", current_pos, " offset=", offset);
            return -1;
        }
    }
    
    // For SEEK_SET, validate that offset is not negative
    if (whence == SEEK_SET && offset < 0) {
        m_error = EINVAL;  // Invalid argument
        Debug::log("io", "FileIOHandler::seek() - SEEK_SET with negative offset: ", offset);
        return -1;
    }
    
    // Perform the seek operation
#ifdef _WIN32
    // Windows: Use _fseeki64 for large file support
    int result = _fseeki64(m_file_handle, offset, whence);
    
    // Enhanced Windows error handling for seek operations
    if (result != 0) {
        DWORD win_error = GetLastError();
        Debug::log("io", "FileIOHandler::seek() - Windows _fseeki64 failed, error: ", win_error);
        
        // Handle specific Windows seek errors
        switch (win_error) {
            case ERROR_NEGATIVE_SEEK:
                Debug::log("io", "FileIOHandler::seek() - Attempted to seek to negative position");
                m_error = EINVAL;
                break;
            case ERROR_SEEK:
                Debug::log("io", "FileIOHandler::seek() - General seek error on Windows");
                m_error = EIO;
                break;
            default:
                m_error = errno;
                break;
        }
    }
#else
    // Unix/Linux: Use fseeko for large file support
    int result = fseeko(m_file_handle, offset, whence);
    
    // Enhanced Unix/Linux error handling for seek operations
    if (result != 0) {
        Debug::log("io", "FileIOHandler::seek() - Unix/Linux fseeko failed, errno: ", errno, " (", strerror(errno), ")");
        
        // Handle specific Unix seek errors
        switch (errno) {
            case EBADF:
                Debug::log("io", "FileIOHandler::seek() - Bad file descriptor");
                m_error = EBADF;
                break;
            case EINVAL:
                Debug::log("io", "FileIOHandler::seek() - Invalid seek parameters");
                m_error = EINVAL;
                break;
            case EOVERFLOW:
                Debug::log("io", "FileIOHandler::seek() - Seek position would overflow");
                m_error = EOVERFLOW;
                break;
            case ESPIPE:
                Debug::log("io", "FileIOHandler::seek() - Seek not supported on this file type");
                m_error = ESPIPE;
                break;
            default:
                m_error = errno;
                break;
        }
    }
#endif
    
    if (result == 0) {
        // Seek successful, update position
        off_t new_position = tell();
        if (new_position >= 0) {
            m_position = new_position;
            // Clear EOF flag if we've moved away from the end
            m_eof = false;
            
            // Invalidate buffer since we've changed position
            invalidateBuffer();
            
            // Reset access pattern tracking after seek
            m_last_read_position = new_position;
            m_sequential_access = false;
            
            Debug::log("io", "FileIOHandler::seek() - Successful seek to position: ", m_position);
        } else {
            // tell() failed after successful seek - this shouldn't happen
            Debug::log("io", "FileIOHandler::seek() - Warning: seek succeeded but tell() failed");
        }
    } else {
        // Seek failed
        m_error = errno;
        Debug::log("io", "FileIOHandler::seek() - Seek failed: ", strerror(errno));
    }
    
    return result;
}

/**
 * @brief Get current byte offset position in the file.
 *
 * This uses 64-bit file operations (ftello) to support large files.
 * 
 * @return Current position as off_t for large file support, -1 on failure
 */
off_t FileIOHandler::tell() {
    // Reset error state
    m_error = 0;
    
    // Validate file handle state
    if (!validateFileHandle()) {
        m_error = EBADF;  // Bad file descriptor
        Debug::log("io", "FileIOHandler::tell() - File is closed or invalid");
        return -1;
    }
    
    // Get current position
#ifdef _WIN32
    // Windows: Use _ftelli64 for large file support
    off_t position = _ftelli64(m_file_handle);
    
    if (position < 0) {
        // Error occurred - get Windows-specific error details
        m_error = errno;
        DWORD win_error = GetLastError();
        Debug::log("io", "FileIOHandler::tell() - Windows _ftelli64 failed: ", strerror(errno), ", Windows error: ", win_error);
    }
#else
    // Unix/Linux: Use ftello for large file support
    off_t position = ftello(m_file_handle);
    
    if (position < 0) {
        // Error occurred
        m_error = errno;
        Debug::log("io", "FileIOHandler::tell() - Failed to get position: ", strerror(errno));
    }
#endif
    
    if (position >= 0) {
        // Update cached position
        m_position = position;
        Debug::log("io", "FileIOHandler::tell() - Current position: ", position);
    }
    
    return position;
}

/**
 * @brief Close the file and cleanup resources.
 *
 * This prevents double-closing and sets the handle to null.
 * 
 * @return 0 on success, standard error codes on failure
 */
int FileIOHandler::close() {
    // Reset error state
    m_error = 0;
    
    // Check if already closed
    if (m_closed || !m_file_handle) {
        m_closed = true;
        Debug::log("io", "FileIOHandler::close() - File already closed");
        return 0;  // Already closed, not an error
    }
    
    Debug::log("io", "FileIOHandler::close() - Closing file: ", m_file_path.to8Bit(false));
    
    // Close the file
    int result = fclose(m_file_handle);
    if (result != 0) {
        // Close failed
        m_error = errno;
        Debug::log("io", "FileIOHandler::close() - Failed to close file: ", strerror(errno));
    } else {
        // Close successful
        m_closed = true;
        m_eof = true;
        Debug::log("io", "FileIOHandler::close() - File closed successfully");
    }
    
    // Nullify the handle regardless of success/failure
    m_file_handle = nullptr;
    
    // Clean up performance optimization resources
    invalidateBuffer();
    m_read_buffer = IOBufferPool::Buffer(); // Release buffer back to pool
    m_cached_file_size = -1;
    m_last_read_position = -1;
    m_sequential_access = false;
    
    // Update memory tracking
    updateMemoryUsage(0);
    
    return result;
}

/**
 * @brief Check if at end-of-file condition.
 *
 * This is a direct wrapper around the standard C feof function.
 * 
 * @return true if at end of file, false otherwise
 */
bool FileIOHandler::eof() {
    // If file is closed or we've already detected EOF, return true
    if (m_closed || !m_file_handle || m_eof) {
        Debug::log("io", "FileIOHandler::eof() - EOF condition: closed=", m_closed, " cached_eof=", m_eof);
        return true;
    }
    
    // Check EOF condition
    m_eof = (feof(m_file_handle) != 0);
    Debug::log("io", "FileIOHandler::eof() - EOF check result: ", m_eof);
    return m_eof;
}

/**
 * @brief Get total size of the file in bytes.
 *
 * This uses fstat system call for accurate size reporting.
 * 
 * @return Size in bytes, or -1 if unknown
 */
off_t FileIOHandler::getFileSize() {
    // Reset error state
    m_error = 0;
    
    // Return cached size if available for performance
    if (m_cached_file_size >= 0) {
        Debug::log("io", "FileIOHandler::getFileSize() - Returning cached size: ", m_cached_file_size);
        return m_cached_file_size;
    }
    
    // Validate file handle state
    if (!validateFileHandle()) {
        m_error = EBADF;  // Bad file descriptor
        return -1;
    }
    
    // Use fstat on the file descriptor to get the size
#ifdef _WIN32
    // Windows: Use _fstat64 for large file support
    struct _stat64 file_stat;
    int fd = _fileno(m_file_handle);
    if (fd < 0) {
        m_error = errno;
        DWORD win_error = GetLastError();
        Debug::log("io", "FileIOHandler::getFileSize() - Invalid file descriptor, Windows error: ", win_error);
        return -1;
    }
    if (_fstat64(fd, &file_stat) != 0) {
        m_error = errno;
        DWORD win_error = GetLastError();
        Debug::log("io", "FileIOHandler::getFileSize() - _fstat64 failed: ", strerror(errno), ", Windows error: ", win_error);
        return -1;
    }
    
    // Additional Windows-specific validation
    if (file_stat.st_size < 0) {
        m_error = EINVAL;
        Debug::log("io", "FileIOHandler::getFileSize() - Invalid file size reported by Windows: ", file_stat.st_size);
        return -1;
    }
    
    // Log large file handling on Windows
    if (file_stat.st_size > 0x7FFFFFFF) { // > 2GB
        Debug::log("io", "FileIOHandler::getFileSize() - Large file detected on Windows: ", file_stat.st_size, " bytes");
    }
#else
    // Unix/Linux: Use fstat for file size determination
    struct stat file_stat;
    int fd = fileno(m_file_handle);
    if (fd < 0) {
        m_error = errno;
        Debug::log("io", "FileIOHandler::getFileSize() - Invalid file descriptor, errno: ", errno, " (", strerror(errno), ")");
        return -1;
    }
    if (fstat(fd, &file_stat) != 0) {
        m_error = errno;
        Debug::log("io", "FileIOHandler::getFileSize() - fstat failed: ", strerror(errno));
        
        // Add Unix-specific error context
        switch (errno) {
            case EBADF:
                Debug::log("io", "FileIOHandler::getFileSize() - Bad file descriptor");
                break;
            case EIO:
                Debug::log("io", "FileIOHandler::getFileSize() - I/O error occurred");
                break;
            case EOVERFLOW:
                Debug::log("io", "FileIOHandler::getFileSize() - File size cannot be represented");
                break;
            default:
                Debug::log("io", "FileIOHandler::getFileSize() - Unix error: ", errno);
                break;
        }
        return -1;
    }
    
    // Additional Unix-specific validation and logging
    if (!S_ISREG(file_stat.st_mode)) {
        // Check if it's a regular file
        if (S_ISDIR(file_stat.st_mode)) {
            Debug::log("io", "FileIOHandler::getFileSize() - Path is a directory, not a regular file");
            m_error = EISDIR;
            return -1;
        } else if (S_ISLNK(file_stat.st_mode)) {
            Debug::log("io", "FileIOHandler::getFileSize() - Path is a symbolic link");
        } else if (S_ISBLK(file_stat.st_mode) || S_ISCHR(file_stat.st_mode)) {
            Debug::log("io", "FileIOHandler::getFileSize() - Path is a device file");
        } else if (S_ISFIFO(file_stat.st_mode)) {
            Debug::log("io", "FileIOHandler::getFileSize() - Path is a FIFO/pipe");
        } else if (S_ISSOCK(file_stat.st_mode)) {
            Debug::log("io", "FileIOHandler::getFileSize() - Path is a socket");
        }
        // For non-regular files, we might not be able to determine size accurately
        // but we'll continue and let the caller handle it
    }
    
    // Log large file handling on Unix/Linux
    if (file_stat.st_size > 0x7FFFFFFF) { // > 2GB
        Debug::log("io", "FileIOHandler::getFileSize() - Large file detected on Unix/Linux: ", file_stat.st_size, " bytes");
    }
    
    // Log file permissions for debugging
    Debug::log("io", "FileIOHandler::getFileSize() - File mode: ", std::oct, file_stat.st_mode, std::dec, 
              ", size: ", file_stat.st_size, " bytes");
#endif
    
    // Validate that the size is reasonable (not negative, which shouldn't happen but let's be safe)
    if (file_stat.st_size < 0) {
        m_error = EINVAL;
        Debug::log("io", "FileIOHandler::getFileSize() - Invalid file size reported by system: ", file_stat.st_size);
        return -1;
    }
    
    // Cache the file size for future calls
    m_cached_file_size = file_stat.st_size;
    
    return file_stat.st_size;
}

/**
 * @brief Validate that the file handle is in a usable state.
 *
 * This performs comprehensive validation of the file handle state.
 * 
 * @return true if handle is valid and file is open, false otherwise
 */
bool FileIOHandler::validateFileHandle() const {
    // Check basic handle validity
    if (!m_file_handle) {
        Debug::log("io", "FileIOHandler::validateFileHandle() - File handle is null");
        return false;
    }
    
    // Check if file is marked as closed
    if (m_closed) {
        Debug::log("io", "FileIOHandler::validateFileHandle() - File is marked as closed");
        return false;
    }
    
    // Check if the file descriptor is valid
#ifdef _WIN32
    int fd = _fileno(m_file_handle);
    if (fd < 0) {
        DWORD win_error = GetLastError();
        Debug::log("io", "FileIOHandler::validateFileHandle() - Invalid file descriptor on Windows, error: ", win_error);
        return false;
    }
#else
    int fd = fileno(m_file_handle);
    if (fd < 0) {
        Debug::log("io", "FileIOHandler::validateFileHandle() - Invalid file descriptor");
        return false;
    }
#endif
    
    // Additional check: verify the file handle hasn't been corrupted
    // by checking if we can get the current position
    off_t current_pos;
#ifdef _WIN32
    current_pos = _ftelli64(m_file_handle);
    if (current_pos < 0 && errno != 0) {
        DWORD win_error = GetLastError();
        Debug::log("io", "FileIOHandler::validateFileHandle() - File handle appears corrupted on Windows, cannot get position: ", strerror(errno), ", Windows error: ", win_error);
        return false;
    }
#else
    current_pos = ftello(m_file_handle);
    if (current_pos < 0 && errno != 0) {
        Debug::log("io", "FileIOHandler::validateFileHandle() - File handle appears corrupted, cannot get position: ", strerror(errno));
        return false;
    }
#endif

    
    return true;
}

/**
 * @brief Attempt to recover from certain error conditions.
 *
 * This method attempts to recover from recoverable errors such as
 * temporary I/O errors or file handle corruption.
 * 
 * @return true if recovery was successful, false otherwise
 */
bool FileIOHandler::attemptErrorRecovery() {
    Debug::log("io", "FileIOHandler::attemptErrorRecovery() - Attempting error recovery");
    
    // If the file handle is null or corrupted, try to reopen the file
    if (!m_file_handle || m_closed) {
        Debug::log("io", "FileIOHandler::attemptErrorRecovery() - File handle is null or closed, attempting reopen");
        
        // Save current error state
        int saved_error = m_error;
        off_t saved_position = m_position;
        
        try {
            // Try to reopen the file
#ifdef _WIN32
            m_file_handle = _wfopen(m_file_path.toCWString(), L"rb");
            
            // Enhanced Windows error handling for reopen
            if (!m_file_handle) {
                DWORD win_error = GetLastError();
                Debug::log("io", "FileIOHandler::attemptErrorRecovery() - Windows reopen failed, error: ", win_error);
            }
#else
            m_file_handle = fopen(m_file_path.toCString(false), "rb");
#endif
            
            if (m_file_handle) {
                // Reopen successful, restore state
                m_closed = false;
                m_eof = false;
                m_error = 0;
                
                // Try to restore position
                if (saved_position > 0) {
#ifdef _WIN32
                    if (_fseeki64(m_file_handle, saved_position, SEEK_SET) == 0) {
#else
                    if (fseeko(m_file_handle, saved_position, SEEK_SET) == 0) {
#endif
                        m_position = saved_position;
                        Debug::log("io", "FileIOHandler::attemptErrorRecovery() - Successfully reopened file and restored position: ", saved_position);
                        return true;
                    } else {
                        Debug::log("io", "FileIOHandler::attemptErrorRecovery() - Reopened file but failed to restore position");
                        m_position = 0;
                        return true; // Still partially successful
                    }
                } else {
                    Debug::log("io", "FileIOHandler::attemptErrorRecovery() - Successfully reopened file");
                    return true;
                }
            } else {
                // Reopen failed, restore error state
                m_error = saved_error;
                Debug::log("io", "FileIOHandler::attemptErrorRecovery() - Failed to reopen file: ", strerror(errno));
                return false;
            }
        } catch (...) {
            // Exception during recovery, restore error state
            m_error = saved_error;
            Debug::log("io", "FileIOHandler::attemptErrorRecovery() - Exception during recovery attempt");
            return false;
        }
    }
    
    // For other types of errors, try to clear error flags
    if (m_file_handle && ferror(m_file_handle)) {
        Debug::log("io", "FileIOHandler::attemptErrorRecovery() - Clearing file error flags");
        clearerr(m_file_handle);
        m_error = 0;
        return true;
    }
    
    Debug::log("io", "FileIOHandler::attemptErrorRecovery() - No recovery action needed or possible");
    return false;
}

bool FileIOHandler::fillBuffer(off_t file_position, size_t min_bytes) {
    Debug::log("io", "FileIOHandler::fillBuffer() - Filling buffer at position ", file_position, " (min bytes: ", min_bytes, ")");
    
    // Validate file handle
    if (!validateFileHandle()) {
        return false;
    }
    
    // Determine buffer size to use
    size_t buffer_size_to_use = std::max(m_buffer_size, min_bytes);
    
    // For sequential access, use read-ahead
    if (m_sequential_access && m_read_ahead_enabled) {
        buffer_size_to_use = std::max(buffer_size_to_use, m_read_ahead_size);
    }
    
    // Don't read beyond file size if known
    if (m_cached_file_size > 0) {
        off_t remaining = m_cached_file_size - file_position;
        if (remaining <= 0) {
            Debug::log("io", "FileIOHandler::fillBuffer() - Position beyond file size");
            m_eof = true;
            return false;
        }
        buffer_size_to_use = std::min(buffer_size_to_use, static_cast<size_t>(remaining));
    }
    
    // Seek to the desired position if necessary
    off_t current_file_pos = tell();
    if (current_file_pos != file_position) {
#ifdef _WIN32
        if (_fseeki64(m_file_handle, file_position, SEEK_SET) != 0) {
#else
        if (fseeko(m_file_handle, file_position, SEEK_SET) != 0) {
#endif
            m_error = errno;
            Debug::log("io", "FileIOHandler::fillBuffer() - Seek failed: ", strerror(errno));
            return false;
        }
    }
    
    // Get buffer from pool if current buffer is too small
    if (m_read_buffer.empty() || m_read_buffer.size() < buffer_size_to_use) {
        // Check memory limits before allocating new buffer
        size_t additional_memory = buffer_size_to_use - (m_read_buffer.empty() ? 0 : m_read_buffer.size());
        
        if (!checkMemoryLimits(additional_memory)) {
            Debug::log("io", "FileIOHandler::fillBuffer() - Memory limit would be exceeded");
            // Try with current buffer size if available
            if (!m_read_buffer.empty()) {
                buffer_size_to_use = m_read_buffer.size();
            } else {
                return false;
            }
        } else {
            // Release current buffer and get a new one
            size_t old_size = m_read_buffer.empty() ? 0 : m_read_buffer.size();
            m_read_buffer = IOBufferPool::Buffer(); // Release current buffer
            m_read_buffer = IOBufferPool::getInstance().acquire(buffer_size_to_use);
            
            if (m_read_buffer.empty()) {
                Debug::log("io", "FileIOHandler::fillBuffer() - Buffer allocation failed from pool");
                // Try with smaller buffer
                buffer_size_to_use = std::min(buffer_size_to_use, static_cast<size_t>(64 * 1024));
                m_read_buffer = IOBufferPool::getInstance().acquire(buffer_size_to_use);
                
                if (m_read_buffer.empty()) {
                    Debug::log("io", "FileIOHandler::fillBuffer() - Even smaller buffer allocation failed");
                    return false;
                }
            }
            
            // Update memory usage tracking
            updateMemoryUsage(m_memory_usage - old_size + m_read_buffer.size());
        }
    }
    
    // Check memory limits before reading
    if (!checkMemoryLimits(buffer_size_to_use)) {
        Debug::log("io", "FileIOHandler::fillBuffer() - Memory limits would be exceeded");
        m_error = ENOMEM;
        return false;
    }
    
    // Read data into buffer
    size_t bytes_read = fread(m_read_buffer.data(), 1, buffer_size_to_use, m_file_handle);
    
    if (bytes_read == 0) {
        if (feof(m_file_handle)) {
            m_eof = true;
            Debug::log("io", "FileIOHandler::fillBuffer() - Reached EOF during buffer fill");
        } else {
            m_error = ferror(m_file_handle);
            Debug::log("io", "FileIOHandler::fillBuffer() - Read error during buffer fill: ", strerror(m_error));
            
            // Attempt error recovery
            if (isRecoverableError(m_error)) {
                if (attemptErrorRecovery()) {
                    Debug::log("io", "FileIOHandler::fillBuffer() - Error recovery successful, retrying");
                    clearerr(m_file_handle);
                    bytes_read = fread(m_read_buffer.data(), 1, buffer_size_to_use, m_file_handle);
                }
            }
        }
        
        if (bytes_read == 0) {
            return false;
        }
    }
    
    // Update memory usage tracking
    updateMemoryUsage(bytes_read);
    
    // Optimize buffer pool usage based on access patterns
    static size_t read_counter = 0;
    read_counter++;
    if (read_counter % 20 == 0) { // Optimize every 20 reads to avoid overhead
        optimizeBufferPoolUsage();
    }
    
    // Update buffer state
    m_buffer_file_position = file_position;
    m_buffer_valid_bytes = bytes_read;
    m_buffer_offset = 0;
    
    Debug::log("io", "FileIOHandler::fillBuffer() - Buffer filled with ", bytes_read, " bytes at position ", file_position);
    
    return true;
}

size_t FileIOHandler::readFromBuffer(void* buffer, size_t bytes_requested) {
    if (m_buffer_valid_bytes == 0 || m_buffer_file_position < 0) {
        Debug::log("io", "FileIOHandler::readFromBuffer() - Buffer is empty or invalid");
        return 0;
    }
    
    // Calculate current position relative to buffer
    off_t current_buffer_offset = m_position - m_buffer_file_position;
    
    if (current_buffer_offset < 0 || static_cast<size_t>(current_buffer_offset) >= m_buffer_valid_bytes) {
        Debug::log("io", "FileIOHandler::readFromBuffer() - Current position not in buffer");
        return 0;
    }
    
    // Calculate how many bytes we can read
    size_t available_bytes = m_buffer_valid_bytes - static_cast<size_t>(current_buffer_offset);
    size_t bytes_to_copy = std::min(bytes_requested, available_bytes);
    
    // Copy data from buffer
    std::memcpy(buffer, m_read_buffer.data() + static_cast<size_t>(current_buffer_offset), bytes_to_copy);
    
    Debug::log("io", "FileIOHandler::readFromBuffer() - Read ", bytes_to_copy, " bytes from buffer (available: ", available_bytes, ")");
    
    return bytes_to_copy;
}

bool FileIOHandler::isPositionBuffered(off_t file_position) const {
    if (m_buffer_valid_bytes == 0 || m_buffer_file_position < 0) {
        return false;
    }
    
    off_t buffer_end = m_buffer_file_position + static_cast<off_t>(m_buffer_valid_bytes);
    return (file_position >= m_buffer_file_position && file_position < buffer_end);
}

void FileIOHandler::updateAccessPattern(off_t current_position) {
    if (m_last_read_position >= 0) {
        // Check if this is sequential access
        off_t position_diff = current_position - m_last_read_position;
        
        // Consider it sequential if we're reading forward within a reasonable range
        static const off_t MAX_SEQUENTIAL_GAP = 64 * 1024; // 64KB
        if (position_diff >= 0 && position_diff <= MAX_SEQUENTIAL_GAP) {
            if (!m_sequential_access) {
                m_sequential_access = true;
                Debug::log("io", "FileIOHandler::updateAccessPattern() - Sequential access pattern detected");
            }
        } else {
            if (m_sequential_access) {
                m_sequential_access = false;
                Debug::log("io", "FileIOHandler::updateAccessPattern() - Sequential access pattern broken");
            }
        }
    }
    
    m_last_read_position = current_position;
}

void FileIOHandler::invalidateBuffer() {
    m_buffer_file_position = -1;
    m_buffer_valid_bytes = 0;
    m_buffer_offset = 0;
    Debug::log("io", "FileIOHandler::invalidateBuffer() - Buffer invalidated");
}

size_t FileIOHandler::getOptimalBufferSize(off_t file_size) const {
    // Base buffer size
    size_t optimal_size = 64 * 1024; // 64KB default
    
    if (file_size > 0) {
        // For small files, use smaller buffer to avoid waste
        if (file_size < 16 * 1024) { // < 16KB
            optimal_size = 4 * 1024; // 4KB
        } else if (file_size < 256 * 1024) { // < 256KB
            optimal_size = 16 * 1024; // 16KB
        } else if (file_size < 1024 * 1024) { // < 1MB
            optimal_size = 32 * 1024; // 32KB
        } else if (file_size < 16 * 1024 * 1024) { // < 16MB
            optimal_size = 64 * 1024; // 64KB
        } else if (file_size < 256 * 1024 * 1024) { // < 256MB
            optimal_size = 128 * 1024; // 128KB
        } else {
            // For very large files, use larger buffer
            optimal_size = 256 * 1024; // 256KB
        }
    }
    
    // Ensure buffer size is reasonable and within memory limits
    static const size_t MAX_BUFFER_SIZE = 1024 * 1024; // 1MB max
    static const size_t MIN_BUFFER_SIZE = 4 * 1024;    // 4KB min
    
    optimal_size = std::max(MIN_BUFFER_SIZE, std::min(optimal_size, MAX_BUFFER_SIZE));
    
    Debug::log("io", "FileIOHandler::getOptimalBufferSize() - File size: ", file_size, ", optimal buffer: ", optimal_size);
    
    return optimal_size;
}

void FileIOHandler::optimizeBufferPoolUsage() {
    // Get current buffer pool statistics
    auto pool_stats = IOBufferPool::getInstance().getStats();
    
    // Calculate hit rate and memory efficiency
    size_t total_requests = pool_stats["total_pool_hits"] + pool_stats["total_pool_misses"];
    if (total_requests == 0) {
        return; // No data to optimize on
    }
    
    double hit_rate = static_cast<double>(pool_stats["total_pool_hits"]) / total_requests;
    double memory_utilization = static_cast<double>(pool_stats["current_pool_size"]) / pool_stats["max_pool_size"];
    
    Debug::log("memory", "FileIOHandler::optimizeBufferPoolUsage() - Hit rate: ", hit_rate * 100, 
              "%, Memory utilization: ", memory_utilization * 100, "%");
    
    // Get memory optimizer recommendations
    MemoryOptimizer& optimizer = MemoryOptimizer::getInstance();
    size_t recommended_pool_size, recommended_buffers_per_size;
    optimizer.getRecommendedBufferPoolParams(recommended_pool_size, recommended_buffers_per_size);
    
    // Apply memory optimizer recommendations
    IOBufferPool::getInstance().setMaxPoolSize(recommended_pool_size);
    IOBufferPool::getInstance().setMaxBuffersPerSize(recommended_buffers_per_size);
    
    // Adjust buffer pool settings based on performance metrics and memory pressure
    MemoryOptimizer::MemoryPressureLevel pressure = optimizer.getMemoryPressureLevel();
    
    if (pressure >= MemoryOptimizer::MemoryPressureLevel::High) {
        // High memory pressure - be conservative with file I/O buffers
        if (hit_rate < 0.5) {
            // Low hit rate under pressure - reduce pool size
            size_t reduced_size = recommended_pool_size * 0.6;
            IOBufferPool::getInstance().setMaxPoolSize(reduced_size);
            Debug::log("memory", "FileIOHandler::optimizeBufferPoolUsage() - Reduced pool size to ", reduced_size, " bytes due to high memory pressure");
        }
        
        // Disable read-ahead under high memory pressure
        if (m_read_ahead_enabled) {
            m_read_ahead_enabled = false;
            Debug::log("memory", "FileIOHandler::optimizeBufferPoolUsage() - Disabled read-ahead due to high memory pressure");
        }
        
    } else if (pressure == MemoryOptimizer::MemoryPressureLevel::Low) {
        // Low memory pressure - can be more aggressive with optimization
        if (hit_rate > 0.8 && memory_utilization < 0.6) {
            // High hit rate with low memory usage - can afford to increase pool size
            size_t increased_size = std::min(recommended_pool_size * 1.3, static_cast<size_t>(16 * 1024 * 1024)); // Cap at 16MB
            IOBufferPool::getInstance().setMaxPoolSize(increased_size);
            Debug::log("memory", "FileIOHandler::optimizeBufferPoolUsage() - Increased pool size to ", increased_size, " bytes due to low memory pressure");
        }
        
        // Enable read-ahead under low memory pressure if not already enabled
        if (!m_read_ahead_enabled && optimizer.shouldEnableReadAhead()) {
            m_read_ahead_enabled = true;
            Debug::log("memory", "FileIOHandler::optimizeBufferPoolUsage() - Enabled read-ahead due to low memory pressure");
        }
    }
    
    // Optimize buffer sizes based on access patterns and memory pressure
    size_t optimal_buffer_size = optimizer.getOptimalBufferSize(m_buffer_size, "file", m_sequential_access);
    
    if (optimal_buffer_size != m_buffer_size) {
        Debug::log("memory", "FileIOHandler::optimizeBufferPoolUsage() - Adjusting buffer size from ", m_buffer_size, 
                  " to ", optimal_buffer_size, " based on memory optimizer recommendations");
        
        // Release current buffer back to pool
        if (!m_read_buffer.empty()) {
            m_read_buffer = IOBufferPool::Buffer(); // Release buffer
            updateMemoryUsage(0);
        }
        
        m_buffer_size = optimal_buffer_size;
        
        // Acquire new buffer with optimal size if memory allows
        if (checkMemoryLimits(m_buffer_size)) {
            m_read_buffer = IOBufferPool::getInstance().acquire(m_buffer_size);
            if (!m_read_buffer.empty()) {
                updateMemoryUsage(m_read_buffer.size());
                // Invalidate buffer since we have a new one
                invalidateBuffer();
            }
        }
    }
    
    // Adjust read-ahead size based on memory pressure
    if (m_read_ahead_enabled) {
        size_t recommended_read_ahead = optimizer.getRecommendedReadAheadSize(128 * 1024); // Default 128KB
        if (recommended_read_ahead != m_read_ahead_size) {
            m_read_ahead_size = recommended_read_ahead;
            Debug::log("memory", "FileIOHandler::optimizeBufferPoolUsage() - Adjusted read-ahead size to ", m_read_ahead_size, " bytes");
        }
    }
    
    // Register memory usage with optimizer
    size_t current_memory_usage = 0;
    if (!m_read_buffer.empty()) {
        current_memory_usage += m_read_buffer.size();
    }
    
    // Update memory tracking
    static size_t last_reported_usage = 0;
    if (current_memory_usage != last_reported_usage) {
        if (last_reported_usage > 0) {
            optimizer.registerDeallocation(last_reported_usage, "file");
        }
        if (current_memory_usage > 0) {
            optimizer.registerAllocation(current_memory_usage, "file");
        }
        last_reported_usage = current_memory_usage;
    }
    
    // Periodic global memory optimization
    static size_t optimization_counter = 0;
    optimization_counter++;
    
    if (optimization_counter % 50 == 0) { // Every 50 buffer optimizations
        IOHandler::performMemoryOptimization();
        Debug::log("memory", "FileIOHandler::optimizeBufferPoolUsage() - Performed global memory optimization");
    }
}

void FileIOHandler::optimizeBufferPoolUsage() {
    // Get current buffer pool statistics
    auto pool_stats = IOBufferPool::getInstance().getStats();
    
    // Calculate hit rate and memory efficiency
    size_t total_requests = pool_stats["total_pool_hits"] + pool_stats["total_pool_misses"];
    if (total_requests == 0) {
        return; // No data to optimize on
    }
    
    double hit_rate = static_cast<double>(pool_stats["total_pool_hits"]) / total_requests;
    double memory_utilization = static_cast<double>(pool_stats["current_pool_size"]) / pool_stats["max_pool_size"];
    
    Debug::log("memory", "FileIOHandler::optimizeBufferPoolUsage() - Hit rate: ", hit_rate * 100, 
              "%, Memory utilization: ", memory_utilization * 100, "%");
    
    // Adjust buffer pool settings based on performance metrics
    if (hit_rate < 0.6 && memory_utilization > 0.9) {
        // Very low hit rate with very high memory usage - aggressively reduce pool size
        size_t new_max_size = pool_stats["max_pool_size"] * 0.7;
        IOBufferPool::getInstance().setMaxPoolSize(new_max_size);
        Debug::log("memory", "FileIOHandler::optimizeBufferPoolUsage() - Aggressively reduced pool size to ", new_max_size, " bytes");
    } else if (hit_rate > 0.95 && memory_utilization < 0.3) {
        // Excellent hit rate with low memory usage - can increase pool size
        size_t new_max_size = pool_stats["max_pool_size"] * 1.3;
        IOBufferPool::getInstance().setMaxPoolSize(new_max_size);
        Debug::log("memory", "FileIOHandler::optimizeBufferPoolUsage() - Increased pool size to ", new_max_size, " bytes");
    }
    
    // Optimize buffer sizes based on file access patterns
    if (m_sequential_access && m_cached_file_size > 0) {
        // For sequential access on large files, prefer larger buffers
        if (m_cached_file_size > 10 * 1024 * 1024 && m_buffer_size < 256 * 1024) { // >10MB file
            m_buffer_size = std::min(static_cast<size_t>(512 * 1024), m_buffer_size * 2);
            Debug::log("memory", "FileIOHandler::optimizeBufferPoolUsage() - Increased buffer size to ", m_buffer_size, " bytes for large file sequential access");
        }
    } else if (!m_sequential_access && m_cached_file_size > 0) {
        // For random access, optimize buffer size based on file size
        if (m_cached_file_size < 1024 * 1024 && m_buffer_size > 32 * 1024) { // <1MB file
            m_buffer_size = std::max(static_cast<size_t>(16 * 1024), m_buffer_size / 2);
            Debug::log("memory", "FileIOHandler::optimizeBufferPoolUsage() - Decreased buffer size to ", m_buffer_size, " bytes for small file random access");
        }
    }
    
    // Periodic buffer pool maintenance
    static size_t optimization_counter = 0;
    optimization_counter++;
    
    if (optimization_counter % 50 == 0) { // Every 50 optimizations
        // Perform comprehensive buffer pool optimization
        IOBufferPool::getInstance().optimizeAllocationPatterns();
        IOBufferPool::getInstance().compactMemory();
        Debug::log("memory", "FileIOHandler::optimizeBufferPoolUsage() - Performed periodic buffer pool optimization");
    } else if (optimization_counter % 20 == 0) { // Every 20 optimizations
        // Lighter cleanup
        IOBufferPool::getInstance().defragmentPools();
        Debug::log("memory", "FileIOHandler::optimizeBufferPoolUsage() - Performed buffer pool defragmentation");
    }
}