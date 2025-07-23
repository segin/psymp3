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
    
    // Get and log file size for debugging
    off_t fileSize = getFileSize();
    if (fileSize >= 0) {
        Debug::log("io", "FileIOHandler::FileIOHandler() - File size: ", fileSize, 
                  " bytes (", std::hex, fileSize, std::dec, ")");
        
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
    // This prevents overflow when calculating total bytes to read
    if (size > 0 && count > SIZE_MAX / size) {
        m_error = EOVERFLOW;  // Value too large for defined data type
        Debug::log("io", "FileIOHandler::read() - Integer overflow prevented: size=", size, " count=", count);
        return 0;
    }
    
    // Perform the read operation
    size_t result = fread(buffer, size, count, m_file_handle);
    
    // Update EOF state
    if (result < count) {
        if (feof(m_file_handle)) {
            m_eof = true;
            Debug::log("io", "FileIOHandler::read() - Reached end of file");
        } else {
            // Read error occurred
            m_error = ferror(m_file_handle);
            Debug::log("io", "FileIOHandler::read() - Read error occurred: ", strerror(m_error));
            
            // Attempt error recovery for certain types of errors
            if (isRecoverableError(m_error)) {
                Debug::log("io", "FileIOHandler::read() - Attempting recovery from recoverable error: ", 
                          getErrorMessage(m_error));
                if (attemptErrorRecovery()) {
                    Debug::log("io", "FileIOHandler::read() - Error recovery successful");
                } else {
                    Debug::log("io", "FileIOHandler::read() - Error recovery failed");
                }
            }
            
            clearerr(m_file_handle);
        }
    }
    
    // Update position with overflow protection
    off_t bytes_read = (off_t)(result * size);
    // Check for overflow: if m_position + bytes_read would overflow
    // We use the fact that if a + b overflows, then a > MAX - b
    static const off_t OFF_T_MAX_VAL = (sizeof(off_t) == 8) ? 0x7FFFFFFFFFFFFFFFLL : 0x7FFFFFFFL;
    if (bytes_read > 0 && m_position > OFF_T_MAX_VAL - bytes_read) {
        // Position would overflow, but we still return the successful read count
        Debug::log("io", "FileIOHandler::read() - Position overflow prevented, clamping to maximum");
        m_position = OFF_T_MAX_VAL;
    } else {
        m_position += bytes_read;
    }
    
    return result;
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