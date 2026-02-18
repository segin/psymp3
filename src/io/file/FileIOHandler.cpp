/*
 * FileIOHandler.cpp - Implementation for the file I/O handler.
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
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

namespace PsyMP3 {
namespace IO {
namespace File {

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
    // Reset base class state using thread-safe methods
    updateClosedState(false);
    updateEofState(false);
    updatePosition(0);
    updateErrorState(0);
    
    // Initialize performance optimization state
    m_buffer_file_position = -1;
    m_buffer_valid_bytes = 0;
    m_buffer_offset = 0;
    m_last_read_position = -1;
    m_sequential_access = false;
    m_cached_file_size = -1;
    
    // Memory tracking is handled by base class constructor
    

    
    // Normalize the path for consistent cross-platform handling
    std::string normalized_path = normalizePath(path.to8Bit(false));
    Debug::log("io", "FileIOHandler::FileIOHandler() - Normalized path: ", normalized_path);
    
    // Security check: Validate file path for directory traversal attacks
    // Performed once in constructor to avoid overhead in every I/O operation
    if (normalized_path.find("..") != std::string::npos) {
        m_path_secure = false;
        std::string errorMsg = "Potential directory traversal attack detected in path: " + normalized_path;
        Debug::log("io", "FileIOHandler::FileIOHandler() - ", errorMsg);
        m_error = EACCES;
        throw InvalidMediaException(errorMsg);
    }
    m_path_secure = true;

    // Platform-specific file opening with Unicode support using RAII
#ifdef _WIN32
    // Windows: Use wide character API for proper Unicode support
    bool file_opened = m_file_handle.open(path.toCWString(), L"rb");
    
    // Handle file open failure with Windows-specific error handling
    if (!file_opened) {
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
                                           nullptr, win_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, nullptr);
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
    bool file_opened = m_file_handle.open(path.toCString(false), "rb");

    // Handle file open failure
    if (!file_opened) {
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
    
    // Get and log file size for debugging and optimization (without locks during construction)
    off_t fileSize = getFileSizeInternal();
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
        filesize_t maxFileSize = getMaxFileSize();
        if (static_cast<filesize_t>(fileSize) > maxFileSize) {
            Debug::log("io", "FileIOHandler::FileIOHandler() - Warning: File size exceeds platform maximum: ", 
                      fileSize, " > ", maxFileSize);
        }
        
        // Log warning for extremely large files that might cause issues
        // Use 64-bit constant but compare safely to avoid overflow on 32-bit off_t platforms
        static const int64_t LARGE_FILE_WARNING_SIZE = 1LL << 32; // 4GB
        if (static_cast<int64_t>(fileSize) > LARGE_FILE_WARNING_SIZE) {
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
 * This calls the base class read method which handles locking and calls our read_unlocked implementation.
 * 
 * @param buffer Buffer to read data into
 * @param size Size of each element to read
 * @param count Number of elements to read
 * @return Number of elements successfully read
 */
size_t FileIOHandler::read(void* buffer, size_t size, size_t count) {
    // Use base class locking - call base class read which will call our read_unlocked
    return IOHandler::read(buffer, size, count);
}

size_t FileIOHandler::read_unlocked(void* buffer, size_t size, size_t count) {
    // Validate operation parameters and preconditions
    if (!validateOperationParameters(buffer, size, count, "read")) {
        // Error already set by validateOperationParameters
        return 0;
    }
    
    if (size == 0 || count == 0) {
        // Not an error, just nothing to do
        Debug::log("io", "FileIOHandler::read_unlocked() - Zero size or count requested: size=", size, " count=", count);
        return 0;
    }
    

    
    size_t bytes_requested = size * count;
    size_t total_bytes_read = 0;
    uint8_t* dest_buffer = static_cast<uint8_t*>(buffer);
    
    // Get current position atomically
    off_t current_position = m_position.load();
    
    // Update access pattern tracking for optimization
    updateAccessPattern(current_position);
    
    // Periodic buffer pool optimization (thread-safe counter)
    static std::atomic<size_t> read_counter{0};
    size_t counter_val = read_counter.fetch_add(1);
    if (counter_val % 100 == 0) { // Every 100 reads
        optimizeBufferPoolUsage();
    }
    
    Debug::log("io", "FileIOHandler::read() - Reading ", bytes_requested, " bytes at position ", current_position, 
              " (sequential: ", (m_sequential_access ? "yes" : "no"), ")");
    
    // Read data using buffered approach
    while (total_bytes_read < bytes_requested && !m_eof) {
        size_t remaining_bytes = bytes_requested - total_bytes_read;
        
        // Check if current position is buffered
        off_t read_pos = current_position + total_bytes_read;
        bool position_buffered;
        size_t buffer_bytes_read = 0;
        
        {
            std::shared_lock<std::shared_mutex> buffer_lock(m_buffer_mutex);
            position_buffered = isPositionBuffered(read_pos);
            if (position_buffered) {
                // Read from buffer using the logical read position
                buffer_bytes_read = readFromBufferAtPosition(dest_buffer + total_bytes_read, remaining_bytes, read_pos);
            }
        }
        
        if (position_buffered) {
            total_bytes_read += buffer_bytes_read;
            
            if (buffer_bytes_read == 0) {
                // Buffer exhausted, need to fill it
                break;
            }
        } else {
            // Need to fill buffer
            off_t read_position = read_pos;
            
            // Determine read size - use read-ahead for sequential access
            size_t read_size = remaining_bytes;
            if (m_sequential_access && m_read_ahead_enabled) {
                read_size = std::max(remaining_bytes, m_read_ahead_size);
            }
            
            // Acquire file mutex before calling fillBuffer to avoid recursive locking
            bool fill_success;
            {
                std::lock_guard<std::mutex> file_lock(m_file_mutex);
                fill_success = fillBuffer(read_position, read_size);
                
                if (!fill_success && m_error.load() == 0) {
                    // Check if we hit EOF (file mutex already held)
                    if (feof(m_file_handle.get())) {
                        updateEofState(true);
                        Debug::log("io", "FileIOHandler::read() - Reached end of file during buffer fill");
                    } else {
                        int file_error = ferror(m_file_handle.get());
                        updateErrorState(file_error, "Buffer fill failed");
                        Debug::log("io", "FileIOHandler::read() - Buffer fill failed: ", strerror(file_error));
                    }
                }
            }
            
            if (!fill_success) {
                break;
            }
            
            // Now read from the newly filled buffer using the logical read position
            {
                std::shared_lock<std::shared_mutex> buffer_lock(m_buffer_mutex);
                buffer_bytes_read = readFromBufferAtPosition(dest_buffer + total_bytes_read, remaining_bytes, read_pos);
            }
            total_bytes_read += buffer_bytes_read;
            
            if (buffer_bytes_read == 0) {
                // Still no data available, likely EOF
                updateEofState(true);
                break;
            }
        }
    }
    
    // Update position with overflow protection using thread-safe method
    if (total_bytes_read > 0) {
        off_t new_position = current_position + static_cast<off_t>(total_bytes_read);
        if (!updatePosition(new_position)) {
            Debug::log("io", "FileIOHandler::read() - Position overflow prevented");
        }
    }
    
    // Calculate number of complete elements read
    size_t elements_read = total_bytes_read / size;
    
    Debug::log("io", "FileIOHandler::read_unlocked() - Read ", total_bytes_read, " bytes (", elements_read, " elements), new position: ", m_position.load());
    
    return elements_read;
}

/**
 * @brief Seeks to a position in the file.
 *
 * This calls the base class seek method which handles locking and calls our seek_unlocked implementation.
 * 
 * @param offset Offset to seek to (off_t for large file support)
 * @param whence SEEK_SET, SEEK_CUR, or SEEK_END positioning mode
 * @return 0 on success, -1 on failure
 */
int FileIOHandler::seek(off_t offset, int whence) {
    // Use base class locking - call base class seek which will call our seek_unlocked
    return IOHandler::seek(offset, whence);
}

int FileIOHandler::seek_unlocked(off_t offset, int whence) {
    // Acquire file-specific lock for this operation
    std::lock_guard<std::mutex> file_lock(m_file_mutex);
    
    // Reset error state
    updateErrorState(0);
    
    // Validate file handle state
    if (!validateFileHandle()) {
        updateErrorState(EBADF, "Bad file descriptor in seek");
        return -1;
    }
    
    // Validate whence parameter
    if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
        updateErrorState(EINVAL, "Invalid whence parameter in seek");
        return -1;
    }
    
    // Additional validation for large file support
    // Check for potential overflow in SEEK_CUR operations
    if (whence == SEEK_CUR) {
        off_t current_pos = tell_internal();
        if (current_pos < 0) {
            // tell_internal() failed, error already set
            return -1;
        }
        
        // Define max/min values for off_t based on its size
        static const off_t OFF_T_MAX_VAL = (sizeof(off_t) == 8) ? 0x7FFFFFFFFFFFFFFFLL : 0x7FFFFFFFL;
        static const off_t OFF_T_MIN_VAL = (sizeof(off_t) == 8) ? (-0x7FFFFFFFFFFFFFFFLL - 1) : (-0x7FFFFFFFL - 1);
        
        // Check for overflow in addition
        if (offset > 0 && current_pos > OFF_T_MAX_VAL - offset) {
            updateErrorState(EOVERFLOW, "SEEK_CUR overflow prevented");
            Debug::log("io", "FileIOHandler::seek() - SEEK_CUR overflow prevented: current=", current_pos, " offset=", offset);
            return -1;
        } else if (offset < 0 && current_pos < OFF_T_MIN_VAL - offset) {
            updateErrorState(EOVERFLOW, "SEEK_CUR underflow prevented");
            Debug::log("io", "FileIOHandler::seek() - SEEK_CUR underflow prevented: current=", current_pos, " offset=", offset);
            return -1;
        }
    }
    
    // For SEEK_SET, validate that offset is not negative
    if (whence == SEEK_SET && offset < 0) {
        updateErrorState(EINVAL, "SEEK_SET with negative offset");
        Debug::log("io", "FileIOHandler::seek() - SEEK_SET with negative offset: ", offset);
        return -1;
    }
    
    // Perform the seek operation
#ifdef _WIN32
    // Windows: Use _fseeki64 for large file support
    int result = _fseeki64(m_file_handle.get(), offset, whence);
    
    // Enhanced Windows error handling for seek operations
    if (result != 0) {
        DWORD win_error = GetLastError();
        Debug::log("io", "FileIOHandler::seek() - Windows _fseeki64 failed, error: ", win_error);
        
        // Handle specific Windows seek errors
        switch (win_error) {
            case ERROR_NEGATIVE_SEEK:
                Debug::log("io", "FileIOHandler::seek() - Attempted to seek to negative position");
                updateErrorState(EINVAL, "Attempted to seek to negative position");
                break;
            case ERROR_SEEK:
                Debug::log("io", "FileIOHandler::seek() - General seek error on Windows");
                updateErrorState(EIO, "General seek error on Windows");
                break;
            default:
                updateErrorState(errno, "Windows seek error");
                break;
        }
    }
#else
    // Unix/Linux: Use fseeko for large file support
    int result = fseeko(m_file_handle.get(), offset, whence);
    
    // Enhanced Unix/Linux error handling for seek operations
    if (result != 0) {
        Debug::log("io", "FileIOHandler::seek() - Unix/Linux fseeko failed, errno: ", errno, " (", strerror(errno), ")");
        
        // Handle specific Unix seek errors
        switch (errno) {
            case EBADF:
                Debug::log("io", "FileIOHandler::seek() - Bad file descriptor");
                updateErrorState(EBADF, "Bad file descriptor");
                break;
            case EINVAL:
                Debug::log("io", "FileIOHandler::seek() - Invalid seek parameters");
                updateErrorState(EINVAL, "Invalid seek parameters");
                break;
            case EOVERFLOW:
                Debug::log("io", "FileIOHandler::seek() - Seek position would overflow");
                updateErrorState(EOVERFLOW, "Seek position would overflow");
                break;
            case ESPIPE:
                Debug::log("io", "FileIOHandler::seek() - Seek not supported on this file type");
                updateErrorState(ESPIPE, "Seek not supported on this file type");
                break;
            default:
                updateErrorState(errno, "Unix seek error");
                break;
        }
    }
#endif
    
    if (result == 0) {
        // Seek successful, calculate the logical position based on seek parameters
        off_t new_logical_position;
        switch (whence) {
            case SEEK_SET:
                new_logical_position = offset;
                break;
            case SEEK_CUR:
                new_logical_position = m_position.load() + offset;
                break;
            case SEEK_END:
                // For SEEK_END, we need to get the file size
                if (m_cached_file_size >= 0) {
                    new_logical_position = m_cached_file_size + offset;
                } else {
                    // Fall back to tell_internal for SEEK_END if we don't know file size
                    new_logical_position = tell_internal();
                }
                break;
            default:
                new_logical_position = m_position.load(); // Shouldn't happen due to earlier validation
                break;
        }
        
        updatePosition(new_logical_position);
        
        // Handle EOF state based on position
        if (whence == SEEK_END && offset == 0) {
            // Seeking to the exact end of file - set EOF
            updateEofState(true);
        } else {
            // Check if we're at the end of the file
            off_t file_size = (m_cached_file_size >= 0) ? m_cached_file_size : getFileSizeInternal();
            if (file_size >= 0 && new_logical_position >= file_size) {
                updateEofState(true);
            } else {
                // Clear EOF flag if we've moved away from the end
                updateEofState(false);
            }
        }
        
        // Invalidate buffer since we've changed position (need buffer lock)
        {
            std::unique_lock<std::shared_mutex> buffer_lock(m_buffer_mutex);
            invalidateBuffer();
        }
        
        // Reset access pattern tracking after seek
        m_last_read_position = new_logical_position;
        m_sequential_access = false;
        
        Debug::log("io", "FileIOHandler::seek_unlocked() - Successful seek to logical position: ", new_logical_position);
    } else {
        // Seek failed
        updateErrorState(errno, "Seek operation failed");
        Debug::log("io", "FileIOHandler::seek_unlocked() - Seek failed: ", strerror(errno));
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
    // Use base class locking - call base class tell which will call our tell_unlocked
    return IOHandler::tell();
}

off_t FileIOHandler::tell_unlocked() {
    // Return the logical position, not the physical file position
    // The physical file position may be ahead due to buffering
    off_t logical_position = m_position.load();
    
    Debug::log("io", "FileIOHandler::tell_unlocked() - Returning logical position: ", logical_position);
    
    return logical_position;
}

off_t FileIOHandler::tell_internal() {
    // Reset error state
    updateErrorState(0);
    
    // Validate file handle state
    if (!validateFileHandle()) {
        updateErrorState(EBADF, "File is closed or invalid in tell");
        Debug::log("io", "FileIOHandler::tell_internal() - File is closed or invalid");
        return -1;
    }
    
    // Get current position
#ifdef _WIN32
    // Windows: Use _ftelli64 for large file support
    off_t position = _ftelli64(m_file_handle.get());
    
    if (position < 0) {
        // Error occurred - get Windows-specific error details
        updateErrorState(errno, "Windows _ftelli64 failed");
        DWORD win_error = GetLastError();
        Debug::log("io", "FileIOHandler::tell() - Windows _ftelli64 failed: ", strerror(errno), ", Windows error: ", win_error);
    }
#else
    // Unix/Linux: Use ftello for large file support
    off_t position = ftello(m_file_handle.get());
    
    if (position < 0) {
        // Error occurred
        updateErrorState(errno, "Failed to get position");
        Debug::log("io", "FileIOHandler::tell() - Failed to get position: ", strerror(errno));
    }
#endif
    
    if (position >= 0) {
        // NOTE: Do NOT call updatePosition() here! 
        // tell_internal() returns the physical file position, which may be ahead due to buffering.
        // The logical position is tracked separately in m_position.
        Debug::log("io", "FileIOHandler::tell_internal() - Physical file position: ", position);
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
    // Use base class locking - call base class close which will call our close_unlocked
    return IOHandler::close();
}

int FileIOHandler::close_unlocked() {
    // Acquire file-specific lock for this operation
    std::lock_guard<std::mutex> file_lock(m_file_mutex);
    
    // Reset error state
    updateErrorState(0);
    
    // Check if already closed
    if (m_closed.load() || !m_file_handle.is_valid()) {
        updateClosedState(true);
        Debug::log("io", "FileIOHandler::close_unlocked() - File already closed");
        return 0;  // Already closed, not an error
    }
    
    Debug::log("io", "FileIOHandler::close_unlocked() - Closing file: ", m_file_path.to8Bit(false));
    
    // Close the file using RAII
    int result = m_file_handle.close();
    if (result != 0) {
        // Close failed
        updateErrorState(errno, "Failed to close file");
        Debug::log("io", "FileIOHandler::close_unlocked() - Failed to close file: ", strerror(errno));
    } else {
        // Close successful
        updateClosedState(true);
        updateEofState(true);
        Debug::log("io", "FileIOHandler::close_unlocked() - File closed successfully");
    }
    
    // Clean up performance optimization resources (need buffer lock)
    {
        std::unique_lock<std::shared_mutex> buffer_lock(m_buffer_mutex);
        invalidateBuffer();
        m_read_buffer = IOBufferPool::Buffer(); // Release buffer back to pool
        m_cached_file_size = -1;
        m_last_read_position = -1;
        m_sequential_access = false;
    }
    
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
    // Return the atomic EOF/closed state directly to avoid UI thread blocking
    // caused by mutex contention on m_file_mutex. The atomic flags are updated
    // by all I/O operations that can hit EOF.
    return m_closed.load() || !m_file_handle.is_valid() || m_eof.load();
}

/**
 * @brief Get total size of the file in bytes.
 *
 * This uses fstat system call for accurate size reporting.
 * 
 * @return Size in bytes, or -1 if unknown
 */
off_t FileIOHandler::getFileSize() {
    // Thread-safe getFileSize operation using file lock
    std::lock_guard<std::mutex> file_lock(m_file_mutex);
    
    // Reset error state
    updateErrorState(0);
    
    // Return cached size if available for performance
    if (m_cached_file_size >= 0) {
        Debug::log("io", "FileIOHandler::getFileSize() - Returning cached size: ", m_cached_file_size);
        return m_cached_file_size;
    }
    
    // Validate file handle state
    if (!validateFileHandle()) {
        updateErrorState(EBADF, "Bad file descriptor in getFileSize");
        return -1;
    }
    
    // Use fstat on the file descriptor to get the size
#ifdef _WIN32
    // Windows: Use _fstat64 for large file support
    struct _stat64 file_stat;
    int fd = _fileno(m_file_handle.get());
    if (fd < 0) {
        updateErrorState(errno, "Invalid file descriptor on Windows");
        DWORD win_error = GetLastError();
        Debug::log("io", "FileIOHandler::getFileSize() - Invalid file descriptor, Windows error: ", win_error);
        return -1;
    }
    if (_fstat64(fd, &file_stat) != 0) {
        updateErrorState(errno, "_fstat64 failed on Windows");
        DWORD win_error = GetLastError();
        Debug::log("io", "FileIOHandler::getFileSize() - _fstat64 failed: ", strerror(errno), ", Windows error: ", win_error);
        return -1;
    }
    
    // Additional Windows-specific validation
    if (file_stat.st_size < 0) {
        updateErrorState(EINVAL, "Invalid file size reported by Windows");
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
    int fd = fileno(m_file_handle.get());
    if (fd < 0) {
        updateErrorState(errno, "Invalid file descriptor on Unix");
        Debug::log("io", "FileIOHandler::getFileSize() - Invalid file descriptor, errno: ", errno, " (", strerror(errno), ")");
        return -1;
    }
    if (fstat(fd, &file_stat) != 0) {
        updateErrorState(errno, "fstat failed on Unix");
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
            updateErrorState(EISDIR, "Path is a directory, not a regular file");
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
        updateErrorState(EINVAL, "Invalid file size reported by system");
        Debug::log("io", "FileIOHandler::getFileSize() - Invalid file size reported by system: ", file_stat.st_size);
        return -1;
    }
    
    // Cache the file size for future calls
    m_cached_file_size = file_stat.st_size;
    
    return file_stat.st_size;
}

off_t FileIOHandler::getFileSizeInternal() {
    // Internal method for constructor use - no locks to avoid deadlock
    // This assumes the file handle is valid and the object is being constructed
    
    if (!m_file_handle.is_valid()) {
        return -1;
    }
    
#ifdef _WIN32
    // Windows: Use _fstat64 for large file support
    struct _stat64 file_stat;
    int fd = _fileno(m_file_handle.get());
    if (fd < 0 || _fstat64(fd, &file_stat) != 0) {
        return -1;
    }
    return file_stat.st_size;
#else
    // Unix/Linux: Use fstat for file size determination
    struct stat file_stat;
    int fd = fileno(m_file_handle.get());
    if (fd < 0 || fstat(fd, &file_stat) != 0) {
        return -1;
    }
    return file_stat.st_size;
#endif
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
    if (!m_file_handle.is_valid()) {
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
    int fd = _fileno(m_file_handle.get());
    if (fd < 0) {
        DWORD win_error = GetLastError();
        Debug::log("io", "FileIOHandler::validateFileHandle() - Invalid file descriptor on Windows, error: ", win_error);
        return false;
    }
#else
    int fd = fileno(m_file_handle.get());
    if (fd < 0) {
        Debug::log("io", "FileIOHandler::validateFileHandle() - Invalid file descriptor");
        return false;
    }
#endif
    
    // NOTE: Removed expensive ftello/current_pos check here for performance.
    // Standard I/O operations will fail if the handle is corrupted, which is sufficient
    // for validation during high-frequency operations like read().

    
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
            FILE* new_file = _wfopen(m_file_path.toCWString(), L"rb");
            m_file_handle.reset(new_file, true);
            
            // Enhanced Windows error handling for reopen
            if (!m_file_handle.is_valid()) {
                DWORD win_error = GetLastError();
                Debug::log("io", "FileIOHandler::attemptErrorRecovery() - Windows reopen failed, error: ", win_error);
            }
#else
            m_file_handle.open(m_file_path.toCString(false), "rb");
#endif
            
            if (m_file_handle) {
                // Reopen successful, restore state
                updateClosedState(false);
                updateEofState(false);
                updateErrorState(0);
                
                // Try to restore position
                if (saved_position > 0) {
#ifdef _WIN32
                    if (_fseeki64(m_file_handle, saved_position, SEEK_SET) == 0) {
#else
                    if (fseeko(m_file_handle, saved_position, SEEK_SET) == 0) {
#endif
                        updatePosition(saved_position);
                        Debug::log("io", "FileIOHandler::attemptErrorRecovery() - Successfully reopened file and restored position: ", saved_position);
                        return true;
                    } else {
                        Debug::log("io", "FileIOHandler::attemptErrorRecovery() - Reopened file but failed to restore position");
                        updatePosition(0);
                        return true; // Still partially successful
                    }
                } else {
                    Debug::log("io", "FileIOHandler::attemptErrorRecovery() - Successfully reopened file");
                    return true;
                }
            } else {
                // Reopen failed, restore error state
                updateErrorState(saved_error);
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
            updateEofState(true);
            return false;
        }
        buffer_size_to_use = std::min(buffer_size_to_use, static_cast<size_t>(remaining));
    }
    
    // Seek to the desired position (assumes file mutex is already held by caller)
#ifdef _WIN32
    if (_fseeki64(m_file_handle, file_position, SEEK_SET) != 0) {
#else
    if (fseeko(m_file_handle, file_position, SEEK_SET) != 0) {
#endif
        m_error = errno;
        Debug::log("io", "FileIOHandler::fillBuffer() - Seek failed: ", strerror(errno));
        return false;
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
            updateEofState(true);
            Debug::log("io", "FileIOHandler::fillBuffer() - Reached EOF during buffer fill");
        } else {
            int file_error = ferror(m_file_handle);
            updateErrorState(file_error, "Read error during buffer fill");
            Debug::log("io", "FileIOHandler::fillBuffer() - Read error during buffer fill: ", strerror(file_error));
            
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
    // Use current position for backward compatibility
    return readFromBufferAtPosition(buffer, bytes_requested, m_position.load());
}

size_t FileIOHandler::readFromBufferAtPosition(void* buffer, size_t bytes_requested, off_t logical_position) {
    if (m_buffer_valid_bytes == 0 || m_buffer_file_position < 0) {
        Debug::log("io", "FileIOHandler::readFromBufferAtPosition() - Buffer is empty or invalid");
        return 0;
    }
    
    // Calculate logical position relative to buffer
    off_t buffer_offset = logical_position - m_buffer_file_position;
    
    if (buffer_offset < 0 || static_cast<size_t>(buffer_offset) >= m_buffer_valid_bytes) {
        Debug::log("io", "FileIOHandler::readFromBufferAtPosition() - Position ", logical_position, " not in buffer (buffer covers ", m_buffer_file_position, " to ", m_buffer_file_position + m_buffer_valid_bytes - 1, ")");
        return 0;
    }
    
    // Calculate how many bytes we can read
    size_t available_bytes = m_buffer_valid_bytes - static_cast<size_t>(buffer_offset);
    size_t bytes_to_copy = std::min(bytes_requested, available_bytes);
    
    // Copy data from buffer
    std::memcpy(buffer, m_read_buffer.data() + static_cast<size_t>(buffer_offset), bytes_to_copy);
    
    Debug::log("io", "FileIOHandler::readFromBufferAtPosition() - Read ", bytes_to_copy, " bytes from buffer at logical position ", logical_position, " (available: ", available_bytes, ")");
    
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
            size_t increased_size = std::min(static_cast<size_t>(recommended_pool_size * 1.3), static_cast<size_t>(16 * 1024 * 1024)); // Cap at 16MB
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

// Enhanced error handling methods for task 7.1

bool FileIOHandler::validateOperationParameters(const void* buffer, size_t size, size_t count, const std::string& operation_name) {
    // Reset error state
    m_error = 0;
    
    // Record operation start time for timeout detection
    m_operation_start_time = std::chrono::steady_clock::now();
    
    // Validate buffer for read operations
    if (operation_name == "read" && !buffer) {
        m_error = EINVAL;
        std::string error_msg = getFileOperationErrorMessage(EINVAL, operation_name, "null buffer parameter");
        Debug::log("io", "FileIOHandler::validateOperationParameters() - ", error_msg);
        safeErrorPropagation(EINVAL, error_msg);
        return false;
    }
    
    // Validate size and count parameters for read operations
    if (operation_name == "read") {
        // Check for reasonable parameter values to prevent abuse
        static const size_t MAX_REASONABLE_SIZE = 1024 * 1024 * 1024; // 1GB per element
        static const size_t MAX_REASONABLE_COUNT = SIZE_MAX / 1024;   // Reasonable count limit
        
        if (size > MAX_REASONABLE_SIZE) {
            m_error = EINVAL;
            std::string error_msg = getFileOperationErrorMessage(EINVAL, operation_name, 
                "unreasonably large element size: " + std::to_string(size) + " bytes");
            Debug::log("io", "FileIOHandler::validateOperationParameters() - ", error_msg);
            safeErrorPropagation(EINVAL, error_msg);
            return false;
        }
        
        if (count > MAX_REASONABLE_COUNT) {
            m_error = EINVAL;
            std::string error_msg = getFileOperationErrorMessage(EINVAL, operation_name, 
                "unreasonably large element count: " + std::to_string(count));
            Debug::log("io", "FileIOHandler::validateOperationParameters() - ", error_msg);
            safeErrorPropagation(EINVAL, error_msg);
            return false;
        }
        
        // Check for potential integer overflow in size calculation
        if (size > 0 && count > SIZE_MAX / size) {
            m_error = EOVERFLOW;
            std::string error_msg = getFileOperationErrorMessage(EOVERFLOW, operation_name, 
                "integer overflow in size calculation (size=" + std::to_string(size) + 
                ", count=" + std::to_string(count) + ")");
            Debug::log("io", "FileIOHandler::validateOperationParameters() - ", error_msg);
            safeErrorPropagation(EOVERFLOW, error_msg);
            return false;
        }
        
        // Check memory limits for the operation
        size_t total_bytes = size * count;
        if (!checkMemoryLimits(total_bytes)) {
            m_error = ENOMEM;
            std::string error_msg = getFileOperationErrorMessage(ENOMEM, operation_name, 
                "operation would exceed memory limits (" + std::to_string(total_bytes) + " bytes)");
            Debug::log("io", "FileIOHandler::validateOperationParameters() - ", error_msg);
            
            // Try to recover from memory pressure
            if (handleFileMemoryAllocationFailure(total_bytes, operation_name + " parameter validation")) {
                Debug::log("io", "FileIOHandler::validateOperationParameters() - Memory recovery successful, retrying validation");
                if (checkMemoryLimits(total_bytes)) {
                    m_error = 0; // Clear error
                    Debug::log("io", "FileIOHandler::validateOperationParameters() - Memory limits now satisfied after recovery");
                } else {
                    safeErrorPropagation(ENOMEM, error_msg);
                    return false;
                }
            } else {
                safeErrorPropagation(ENOMEM, error_msg);
                return false;
            }
        }
    }
    
    // Validate file handle state for all operations
    if (!validateFileHandle()) {
        if (m_error == 0) {
            m_error = EBADF;  // Bad file descriptor
        }
        std::string error_msg = getFileOperationErrorMessage(m_error, operation_name, "invalid file handle state");
        Debug::log("io", "FileIOHandler::validateOperationParameters() - ", error_msg);
        safeErrorPropagation(m_error, error_msg);
        return false;
    }
    
    // Check if file is closed
    if (m_closed) {
        m_error = EBADF;
        std::string error_msg = getFileOperationErrorMessage(EBADF, operation_name, "file is closed");
        Debug::log("io", "FileIOHandler::validateOperationParameters() - ", error_msg);
        safeErrorPropagation(EBADF, error_msg);
        return false;
    }
    
    // Validate file path for security (prevent directory traversal attacks)
    // Optimized: Path is validated in constructor, check flag here
    if (!m_path_secure) {
        m_error = EACCES;
        std::string error_msg = getFileOperationErrorMessage(EACCES, operation_name,
            "path security validation failed");
        Debug::log("io", "FileIOHandler::validateOperationParameters() - ", error_msg);
        safeErrorPropagation(EACCES, error_msg);
        return false;
    }
    
    // Check for timeout conditions on network file systems and slow storage
    if (m_timeout_enabled) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_operation_start_time);
        
        if (elapsed.count() >= m_default_timeout_seconds) {
            m_error = ETIMEDOUT;
            std::string error_msg = getFileOperationErrorMessage(ETIMEDOUT, operation_name, 
                "operation timeout (" + std::to_string(elapsed.count()) + "s >= " + 
                std::to_string(m_default_timeout_seconds) + "s)");
            Debug::log("io", "FileIOHandler::validateOperationParameters() - ", error_msg);
            
            // Try to handle timeout gracefully
            if (handleTimeout(operation_name, m_default_timeout_seconds)) {
                Debug::log("io", "FileIOHandler::validateOperationParameters() - Timeout recovery successful");
                m_error = 0; // Clear error
            } else {
                safeErrorPropagation(ETIMEDOUT, error_msg);
                return false;
            }
        }
    }
    
    Debug::log("io", "FileIOHandler::validateOperationParameters() - ", operation_name, " operation parameters validated successfully");
    return true;
}

bool FileIOHandler::handleTimeout(const std::string& operation_name, int timeout_seconds) {
    if (!m_timeout_enabled) {
        return true; // Timeout handling disabled
    }
    
    auto current_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - m_operation_start_time);
    
    if (elapsed.count() >= timeout_seconds) {
        m_error = ETIMEDOUT;
        std::string error_msg = getFileOperationErrorMessage(ETIMEDOUT, operation_name, 
            "operation timed out after " + std::to_string(elapsed.count()) + " seconds (limit: " + 
            std::to_string(timeout_seconds) + "s)");
        Debug::log("io", "FileIOHandler::handleTimeout() - ", error_msg);
        
        // Detect if this might be a network file system causing the timeout
        std::string path_str = m_file_path.to8Bit(false);
        bool likely_network_fs = false;
        
        // Check for common network file system indicators
        if (path_str.find("//") == 0 || path_str.find("\\\\") == 0) {
            likely_network_fs = true; // UNC path
            Debug::log("io", "FileIOHandler::handleTimeout() - Detected UNC path, likely network file system");
        } else if (path_str.find("/mnt/") == 0 || path_str.find("/net/") == 0) {
            likely_network_fs = true; // Common Unix network mount points
            Debug::log("io", "FileIOHandler::handleTimeout() - Detected network mount point");
        }
        
        // For network file systems, be more lenient with timeouts
        if (likely_network_fs && timeout_seconds < 60) {
            Debug::log("io", "FileIOHandler::handleTimeout() - Extending timeout for network file system to 60 seconds");
            m_default_timeout_seconds = 60; // Extend timeout for network FS
            m_operation_start_time = current_time; // Reset timer
            m_error = 0; // Clear timeout error
            return true; // Give it more time
        }
        
        // Attempt to recover from timeout
        if (attemptErrorRecovery()) {
            Debug::log("io", "FileIOHandler::handleTimeout() - Recovery successful for ", operation_name, " timeout");
            m_operation_start_time = current_time; // Reset timer after recovery
            return true;
        }
        
        // If recovery failed, try one more approach for slow storage
        Debug::log("io", "FileIOHandler::handleTimeout() - Attempting slow storage recovery");
        
        // Clear any buffered data that might be stale
        invalidateBuffer();
        
        // Try to flush any pending operations
        if (m_file_handle) {
            fflush(m_file_handle);
        }
        
        // Give one final chance with extended timeout
        if (elapsed.count() < timeout_seconds * 2) {
            Debug::log("io", "FileIOHandler::handleTimeout() - Giving final extended timeout chance");
            m_operation_start_time = current_time; // Reset timer
            m_error = 0; // Clear timeout error
            return true;
        }
        
        // Final timeout - propagate error safely
        safeErrorPropagation(ETIMEDOUT, error_msg);
        return false;
    }
    
    return true; // No timeout
}

std::string FileIOHandler::getFileOperationErrorMessage(int error_code, const std::string& operation_name, const std::string& additional_context) {
    std::string message = "File " + operation_name + " operation failed";
    
    if (!additional_context.empty()) {
        message += " (" + additional_context + ")";
    }
    
    message += " on file: " + m_file_path.to8Bit(false);
    
    // Add specific error details based on error code
    switch (error_code) {
        case ENOENT:
            message += " - File not found";
            break;
        case EACCES:
            message += " - Permission denied";
            break;
        case EISDIR:
            message += " - Path is a directory, not a regular file";
            break;
        case ENOTDIR:
            message += " - Path component is not a directory";
            break;
        case EBADF:
            message += " - Bad file descriptor or file is closed";
            break;
        case EINVAL:
            message += " - Invalid argument or parameter";
            break;
        case EIO:
            message += " - I/O error occurred";
            break;
        case ENOSPC:
            message += " - No space left on device";
            break;
        case ENOMEM:
            message += " - Out of memory";
            break;
        case EROFS:
            message += " - Read-only file system";
            break;
        case ELOOP:
            message += " - Too many symbolic links encountered";
            break;
        case ENAMETOOLONG:
            message += " - File name too long";
            break;
        case EOVERFLOW:
            message += " - Value too large for defined data type";
            break;
        case ETIMEDOUT:
            message += " - Operation timed out";
            break;
        case EAGAIN:
            message += " - Resource temporarily unavailable";
            break;
        case EINTR:
            message += " - Interrupted system call";
            break;
        default:
            // Use cross-platform error message utility
            message += " - " + getErrorMessage(error_code);
            break;
    }
    
    // Add recovery suggestion for recoverable errors
    if (isFileErrorRecoverable(error_code, operation_name)) {
        message += " (error may be recoverable)";
    }
    
    return message;
}

bool FileIOHandler::isFileErrorRecoverable(int error_code, const std::string& operation_name) {
    // Use base class method for general recoverability check
    if (!isRecoverableError(error_code)) {
        return false;
    }
    
    // File-specific recoverability checks
    switch (error_code) {
        case EIO:
            // I/O errors might be recoverable for network file systems
            Debug::log("io", "FileIOHandler::isFileErrorRecoverable() - I/O error for ", operation_name, " may be recoverable");
            return true;
            
        case EAGAIN:
        case EINTR:
            // These are typically recoverable
            Debug::log("io", "FileIOHandler::isFileErrorRecoverable() - Temporary error for ", operation_name, " is recoverable");
            return true;
            
        case ENOMEM:
            // Memory errors might be recoverable if memory is freed
            Debug::log("io", "FileIOHandler::isFileErrorRecoverable() - Memory error for ", operation_name, " may be recoverable");
            return true;
            
        case ENOSPC:
            // Disk full might be recoverable if space is freed
            Debug::log("io", "FileIOHandler::isFileErrorRecoverable() - Disk full error for ", operation_name, " may be recoverable");
            return true;
            
        case ETIMEDOUT:
            // Timeouts are often recoverable
            Debug::log("io", "FileIOHandler::isFileErrorRecoverable() - Timeout error for ", operation_name, " is recoverable");
            return true;
            
        default:
            return false;
    }
}

bool FileIOHandler::retryFileOperation(std::function<bool()> operation_func, const std::string& operation_name, int max_retries, int retry_delay_ms) {
    int retry_count = 0;
    
    while (retry_count <= max_retries) {
        // Record operation start time for timeout detection
        m_operation_start_time = std::chrono::steady_clock::now();
        
        // Attempt the operation
        if (operation_func()) {
            if (retry_count > 0) {
                Debug::log("io", "FileIOHandler::retryFileOperation() - ", operation_name, " succeeded after ", retry_count, " retries");
            }
            return true;
        }
        
        // Operation failed, check if we should retry
        if (retry_count >= max_retries) {
            Debug::log("io", "FileIOHandler::retryFileOperation() - ", operation_name, " failed after ", max_retries, " retries, giving up");
            break;
        }
        
        // Check if error is recoverable
        if (!isFileErrorRecoverable(m_error, operation_name)) {
            Debug::log("io", "FileIOHandler::retryFileOperation() - ", operation_name, " failed with non-recoverable error: ", m_error, ", not retrying");
            break;
        }
        
        retry_count++;
        Debug::log("io", "FileIOHandler::retryFileOperation() - ", operation_name, " failed (error: ", m_error, "), retrying (", retry_count, "/", max_retries, ")");
        
        // Wait before retrying (exponential backoff)
        int delay = retry_delay_ms * (1 << (retry_count - 1)); // Exponential backoff
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        
        // Attempt error recovery before retry
        attemptErrorRecovery();
    }
    
    return false;
}

bool FileIOHandler::handleFileMemoryAllocationFailure(size_t requested_size, const std::string& context) {
    Debug::log("memory", "FileIOHandler::handleFileMemoryAllocationFailure() - Failed to allocate ", 
              requested_size, " bytes for file operation: ", context);
    
    // First try the base class recovery mechanism
    if (handleMemoryAllocationFailure(requested_size, context)) {
        Debug::log("memory", "FileIOHandler::handleFileMemoryAllocationFailure() - Base class recovery successful");
        return true;
    }
    
    // File-specific memory recovery strategies
    
    // If this is a buffer allocation failure, try to reduce buffer sizes
    if (context.find("buffer") != std::string::npos || context.find("read") != std::string::npos) {
        Debug::log("memory", "FileIOHandler::handleFileMemoryAllocationFailure() - Attempting buffer size reduction");
        
        // Release current buffer back to pool
        if (!m_read_buffer.empty()) {
            m_read_buffer = IOBufferPool::Buffer();
            updateMemoryUsage(0);
            Debug::log("memory", "FileIOHandler::handleFileMemoryAllocationFailure() - Released current buffer");
        }
        
        // Try progressively smaller buffer sizes
        std::vector<size_t> fallback_sizes = {
            m_buffer_size / 2,    // Half current size
            m_buffer_size / 4,    // Quarter current size
            16 * 1024,            // 16KB
            8 * 1024,             // 8KB
            4 * 1024,             // 4KB
            1024                  // 1KB minimum
        };
        
        for (size_t fallback_size : fallback_sizes) {
            if (fallback_size < 1024) continue; // Don't go below 1KB
            
            if (checkMemoryLimits(fallback_size)) {
                m_read_buffer = IOBufferPool::getInstance().acquire(fallback_size);
                if (!m_read_buffer.empty()) {
                    m_buffer_size = fallback_size;
                    updateMemoryUsage(m_read_buffer.size());
                    Debug::log("memory", "FileIOHandler::handleFileMemoryAllocationFailure() - Successfully allocated fallback buffer: ", fallback_size, " bytes");
                    return true;
                }
            }
        }
        
        // If all buffer allocations failed, disable buffering
        Debug::log("memory", "FileIOHandler::handleFileMemoryAllocationFailure() - Disabling buffering due to memory pressure");
        m_buffer_size = 0;
        invalidateBuffer();
        return true; // Can still operate without buffering
    }
    
    // If this is a read-ahead allocation failure, disable read-ahead
    if (context.find("read-ahead") != std::string::npos || context.find("readahead") != std::string::npos) {
        Debug::log("memory", "FileIOHandler::handleFileMemoryAllocationFailure() - Disabling read-ahead optimization");
        m_read_ahead_enabled = false;
        m_read_ahead_size = 0;
        return true;
    }
    
    Debug::log("memory", "FileIOHandler::handleFileMemoryAllocationFailure() - All file-specific recovery strategies failed");
    return false;
}

bool FileIOHandler::handleFileResourceExhaustion(const std::string& resource_type, const std::string& context) {
    Debug::log("resource", "FileIOHandler::handleFileResourceExhaustion() - Resource exhausted: ", 
              resource_type, " in file context: ", context);
    
    // First try the base class recovery mechanism
    if (handleResourceExhaustion(resource_type, context)) {
        Debug::log("resource", "FileIOHandler::handleFileResourceExhaustion() - Base class recovery successful");
        return true;
    }
    
    // File-specific resource exhaustion recovery
    
    if (resource_type == "file_descriptors") {
        Debug::log("resource", "FileIOHandler::handleFileResourceExhaustion() - File descriptor exhaustion");
        
        // If we have a valid file handle, we can continue operating
        if (validateFileHandle()) {
            Debug::log("resource", "FileIOHandler::handleFileResourceExhaustion() - Current file handle is valid, continuing");
            return true;
        }
        
        // Try to reopen the file if it was closed due to FD exhaustion
        if (m_closed && !m_file_path.isEmpty()) {
            Debug::log("resource", "FileIOHandler::handleFileResourceExhaustion() - Attempting to reopen file");
            
            try {
#ifdef _WIN32
                m_file_handle.open(m_file_path.toCWString(), L"rb");
#else
                m_file_handle.open(m_file_path.toCString(false), "rb");
#endif
                if (m_file_handle) {
                    m_closed = false;
                    m_error = 0;
                    Debug::log("resource", "FileIOHandler::handleFileResourceExhaustion() - Successfully reopened file");
                    return true;
                }
            } catch (const std::exception& e) {
                Debug::log("resource", "FileIOHandler::handleFileResourceExhaustion() - Exception during file reopen: ", e.what());
            }
        }
        
        Debug::log("resource", "FileIOHandler::handleFileResourceExhaustion() - Could not recover from file descriptor exhaustion");
        return false;
    }
    
    if (resource_type == "disk_space") {
        Debug::log("resource", "FileIOHandler::handleFileResourceExhaustion() - Disk space exhaustion for file operations");
        
        // For read operations, disk space shouldn't matter
        if (context.find("read") != std::string::npos) {
            Debug::log("resource", "FileIOHandler::handleFileResourceExhaustion() - Disk space exhaustion during read operation, should not affect reading");
            return true;
        }
        
        // For write operations (future extension), we cannot recover
        Debug::log("resource", "FileIOHandler::handleFileResourceExhaustion() - Cannot recover from disk space exhaustion for write operations");
        return false;
    }
    
    if (resource_type == "memory") {
        // This should have been handled by handleFileMemoryAllocationFailure
        return handleFileMemoryAllocationFailure(0, context);
    }
    
    Debug::log("resource", "FileIOHandler::handleFileResourceExhaustion() - Unknown resource type or no recovery possible");
    return false;
}

void FileIOHandler::ensureSafeDestructorCleanup() noexcept {
    try {
        Debug::log("memory", "FileIOHandler::ensureSafeDestructorCleanup() - Ensuring safe cleanup");
        
        // Close file handle safely
        if (m_file_handle) {
            try {
                fclose(m_file_handle);
                Debug::log("memory", "FileIOHandler::ensureSafeDestructorCleanup() - File handle closed");
            } catch (...) {
                // Ignore exceptions during cleanup
            }
            m_file_handle.close();
        }
        
        // Release buffer safely
        try {
            m_read_buffer = IOBufferPool::Buffer();
            Debug::log("memory", "FileIOHandler::ensureSafeDestructorCleanup() - Buffer released");
        } catch (...) {
            // Ignore exceptions during cleanup
        }
        
        // Update memory tracking safely
        try {
            updateMemoryUsage(0);
            Debug::log("memory", "FileIOHandler::ensureSafeDestructorCleanup() - Memory tracking updated");
        } catch (...) {
            // Ignore exceptions during cleanup
        }
        
        // Reset state safely
        updateClosedState(true);
        updateEofState(true);
        m_buffer_file_position = -1;
        m_buffer_valid_bytes = 0;
        m_buffer_offset = 0;
        m_cached_file_size = -1;
        m_last_read_position = -1;
        m_sequential_access = false;
        m_retry_count = 0;
        m_error = 0;
        
        Debug::log("memory", "FileIOHandler::ensureSafeDestructorCleanup() - Safe cleanup completed");
        
    } catch (...) {
        // Absolutely no exceptions should escape from destructor cleanup
        // This is a last resort - just ensure basic state is reset
        m_file_handle.close();
        updateClosedState(true);
        updateErrorState(0);
    }
}

} // namespace File
} // namespace IO
} // namespace PsyMP3
