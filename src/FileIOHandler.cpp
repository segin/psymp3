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
    
    // Platform-specific file opening with Unicode support
#ifdef _WIN32
    // Windows: Use wide character API for proper Unicode support
    m_file_handle = _wfopen(path.toCWString(), L"rb");
#else
    // Unix/Linux: Use raw C string without UTF-8 conversion to preserve original filesystem encoding
    m_file_handle = fopen(path.toCString(false), "rb");
#endif

    // Handle file open failure
    if (!m_file_handle) {
        // Get system error code
        m_error = errno;
        
        // Create descriptive error message with system error details
        std::string errorMsg = "Could not open file: " + path.to8Bit(false) + " (";
        errorMsg += strerror(errno);
        errorMsg += ")";
        
        // Log the error
        Debug::log("io", "FileIOHandler::FileIOHandler() - ", errorMsg);
        
        // Throw exception with detailed error message
        throw InvalidMediaException(errorMsg);
    }
    
    // Log successful file opening
    Debug::log("io", "FileIOHandler::FileIOHandler() - Successfully opened file: ", path.to8Bit(false));
    
    // Get and log file size for debugging
    off_t fileSize = getFileSize();
    if (fileSize >= 0) {
        Debug::log("io", "FileIOHandler::FileIOHandler() - File size: ", fileSize, 
                  " bytes (", std::hex, fileSize, std::dec, ")");
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
    
    // Check if file is closed
    if (m_closed || !m_file_handle) {
        m_error = EBADF;  // Bad file descriptor
        return 0;
    }
    
    // Validate parameters
    if (!buffer) {
        m_error = EINVAL; // Invalid argument
        return 0;
    }
    
    if (size == 0 || count == 0) {
        // Not an error, just nothing to do
        return 0;
    }
    
    // Perform the read operation
    size_t result = fread(buffer, size, count, m_file_handle);
    
    // Update EOF state
    if (result < count) {
        if (feof(m_file_handle)) {
            m_eof = true;
        } else {
            // Read error occurred
            m_error = ferror(m_file_handle);
            clearerr(m_file_handle);
        }
    }
    
    // Update position
    m_position += result * size;
    
    return result;
}

/**
 * @brief Seeks to a position in the file.
 *
 * This uses 64-bit file operations (fseeko) to support large files.
 * 
 * @param offset Offset to seek to
 * @param whence SEEK_SET, SEEK_CUR, or SEEK_END positioning mode
 * @return 0 on success, -1 on failure
 */
int FileIOHandler::seek(long offset, int whence) {
    // Reset error state
    m_error = 0;
    
    // Check if file is closed
    if (m_closed || !m_file_handle) {
        m_error = EBADF;  // Bad file descriptor
        return -1;
    }
    
    // Validate whence parameter
    if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
        m_error = EINVAL;  // Invalid argument
        return -1;
    }
    
    // Perform the seek operation
#ifdef _WIN32
    // Windows: Use _fseeki64 for large file support
    int result = _fseeki64(m_file_handle, offset, whence);
#else
    // Unix/Linux: Use fseeko for large file support
    int result = fseeko(m_file_handle, offset, whence);
#endif
    
    if (result == 0) {
        // Seek successful, update position
        m_position = tell();
        
        // Clear EOF flag if we've moved away from the end
        m_eof = false;
    } else {
        // Seek failed
        m_error = errno;
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
    
    // Check if file is closed
    if (m_closed || !m_file_handle) {
        m_error = EBADF;  // Bad file descriptor
        return -1;
    }
    
    // Get current position
#ifdef _WIN32
    // Windows: Use _ftelli64 for large file support
    off_t position = _ftelli64(m_file_handle);
#else
    // Unix/Linux: Use ftello for large file support
    off_t position = ftello(m_file_handle);
#endif
    
    if (position < 0) {
        // Error occurred
        m_error = errno;
    } else {
        // Update cached position
        m_position = position;
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
        return 0;  // Already closed, not an error
    }
    
    // Close the file
    int result = fclose(m_file_handle);
    if (result != 0) {
        // Close failed
        m_error = errno;
    } else {
        // Close successful
        m_closed = true;
        m_eof = true;
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
        return true;
    }
    
    // Check EOF condition
    m_eof = (feof(m_file_handle) != 0);
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
    
    // Check if file is closed
    if (m_closed || !m_file_handle) {
        m_error = EBADF;  // Bad file descriptor
        return -1;
    }
    
    // Use fstat on the file descriptor to get the size
#ifdef _WIN32
    // Windows: Use _fstat64 for large file support
    struct _stat64 file_stat;
    int fd = _fileno(m_file_handle);
    if (_fstat64(fd, &file_stat) != 0) {
        m_error = errno;
        return -1;
    }
#else
    // Unix/Linux: Use fstat for file size determination
    struct stat file_stat;
    int fd = fileno(m_file_handle);
    if (fstat(fd, &file_stat) != 0) {
        m_error = errno;
        return -1;
    }
#endif
    
    return file_stat.st_size;
}