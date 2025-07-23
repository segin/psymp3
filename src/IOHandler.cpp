/*
 * IOHandler.cpp - Base I/O handler interface implementation
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

// IOHandler base class implementation

IOHandler::~IOHandler() {
    // Virtual destructor for proper polymorphic cleanup
    // Derived classes should override this to clean up their specific resources
}

size_t IOHandler::read(void* buffer, size_t size, size_t count) {
    // Default implementation returns 0 (no data read) for non-functional state
    // This follows fread-like semantics where 0 indicates EOF or error
    
    // Reset error state
    m_error = 0;
    
    if (m_closed) {
        m_error = EBADF;  // Bad file descriptor
        return 0;
    }
    
    if (!buffer) {
        m_error = EINVAL; // Invalid argument
        return 0;
    }
    
    if (size == 0 || count == 0) {
        // Not an error, just nothing to do
        return 0;
    }
    
    if (m_eof) {
        // Already at EOF, not an error
        return 0;
    }
    
    // Mark as EOF since we can't actually read anything in the base implementation
    m_eof = true;
    return 0;
}

int IOHandler::seek(off_t offset, int whence) {
    // Default implementation returns -1 (failure) for non-functional state
    // Support SEEK_SET, SEEK_CUR, SEEK_END positioning modes
    
    // Reset error state
    m_error = 0;
    
    if (m_closed) {
        m_error = EBADF;  // Bad file descriptor
        return -1;
    }
    
    // Calculate new position based on whence
    off_t new_position = m_position;
    switch (whence) {
        case SEEK_SET:
            new_position = offset;
            break;
        case SEEK_CUR:
            new_position += offset;
            break;
        case SEEK_END:
            // Can't determine end position in base class
            m_error = EINVAL;  // Invalid argument
            return -1;
        default:
            // Invalid whence parameter
            m_error = EINVAL;  // Invalid argument
            return -1;
    }
    
    // Validate new position
    if (new_position < 0) {
        m_error = EINVAL;  // Invalid argument
        return -1;
    }
    
    // Update position
    m_position = new_position;
    
    // Clear EOF if we've moved away from the end
    m_eof = false;
    
    return 0;
}

off_t IOHandler::tell() {
    // Return current byte offset with off_t for large file support
    // Return -1 on failure (e.g., if closed)
    
    // Reset error state
    m_error = 0;
    
    if (m_closed) {
        m_error = EBADF;  // Bad file descriptor
        return -1;
    }
    
    return m_position;
}

int IOHandler::close() {
    // Resource cleanup with standard return codes
    // 0 on success, non-zero on failure
    
    // Reset error state
    m_error = 0;
    
    if (m_closed) {
        // Already closed, not an error
        return 0;
    }
    
    m_closed = true;
    m_eof = true;
    return 0;
}

bool IOHandler::eof() {
    // Return boolean end-of-stream condition
    // True if at end of stream or closed, false otherwise
    return m_closed || m_eof;
}

off_t IOHandler::getFileSize() {
    // Return total size in bytes or -1 if unknown
    // Base implementation doesn't know the size, so return -1
    return -1;
}

int IOHandler::getLastError() const {
    // Return the last error code (0 = no error)
    return m_error;
}

// Cross-platform utility methods

std::string IOHandler::normalizePath(const std::string& path) {
    if (path.empty()) {
        return path;
    }
    
    std::string normalized = path;
    
#ifdef _WIN32
    // On Windows, convert forward slashes to backslashes
    std::replace(normalized.begin(), normalized.end(), '/', '\\');
    
    // Handle UNC paths (\\server\share) - don't normalize the leading slashes
    if (normalized.length() >= 2 && normalized[0] == '\\' && normalized[1] == '\\') {
        // UNC path - leave as is
        return normalized;
    }
    
    // Handle drive letters (C:\path) - ensure proper format
    if (normalized.length() >= 2 && normalized[1] == ':') {
        // Drive letter path - ensure uppercase drive letter
        normalized[0] = std::toupper(normalized[0]);
    }
#else
    // On Unix/Linux, convert backslashes to forward slashes
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    
    // Remove duplicate slashes (but preserve leading slash for absolute paths)
    std::string result;
    bool last_was_slash = false;
    bool is_first_char = true;
    
    for (char c : normalized) {
        if (c == '/') {
            if (!last_was_slash || is_first_char) {
                result += c;
                last_was_slash = true;
            }
        } else {
            result += c;
            last_was_slash = false;
        }
        is_first_char = false;
    }
    
    normalized = result;
#endif
    
    return normalized;
}

char IOHandler::getPathSeparator() {
#ifdef _WIN32
    return '\\';
#else
    return '/';
#endif
}

std::string IOHandler::getErrorMessage(int error_code, const std::string& context) {
    std::string message;
    
    if (!context.empty()) {
        message = context + ": ";
    }
    
    // Get standard error message
    const char* error_str = strerror(error_code);
    if (error_str) {
        message += error_str;
    } else {
        message += "Unknown error " + std::to_string(error_code);
    }
    
#ifdef _WIN32
    // On Windows, also try to get Windows-specific error message
    if (error_code != 0) {
        DWORD win_error = GetLastError();
        if (win_error != 0) {
            LPSTR messageBuffer = nullptr;
            size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                       NULL, win_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
            if (size > 0 && messageBuffer) {
                std::string win_msg(messageBuffer, size);
                // Remove trailing newlines
                while (!win_msg.empty() && (win_msg.back() == '\n' || win_msg.back() == '\r')) {
                    win_msg.pop_back();
                }
                if (!win_msg.empty()) {
                    message += " (Windows: " + win_msg + ")";
                }
                LocalFree(messageBuffer);
            }
        }
    }
#endif
    
    return message;
}

bool IOHandler::isRecoverableError(int error_code) {
    switch (error_code) {
        // Temporary I/O errors that might be recoverable
        case EIO:           // I/O error
        case EAGAIN:        // Resource temporarily unavailable
        case EINTR:         // Interrupted system call
        case ENOMEM:        // Out of memory (might be temporary)
        case ENOSPC:        // No space left on device (might be temporary)
            return true;
            
        // Network-related errors that might be recoverable
#ifdef ETIMEDOUT
        case ETIMEDOUT:     // Connection timed out
            return true;
#endif
#ifdef ECONNRESET
        case ECONNRESET:    // Connection reset by peer
            return true;
#endif
#ifdef ECONNABORTED
        case ECONNABORTED:  // Connection aborted
            return true;
#endif
#ifdef ENETDOWN
        case ENETDOWN:      // Network is down
            return true;
#endif
#ifdef ENETUNREACH
        case ENETUNREACH:   // Network is unreachable
            return true;
#endif
#ifdef EHOSTDOWN
        case EHOSTDOWN:     // Host is down
            return true;
#endif
#ifdef EHOSTUNREACH
        case EHOSTUNREACH:  // Host is unreachable
            return true;
#endif
            
        // Permanent errors that are not recoverable
        case ENOENT:        // No such file or directory
        case EACCES:        // Permission denied
        case EISDIR:        // Is a directory
        case ENOTDIR:       // Not a directory
        case EBADF:         // Bad file descriptor
        case EINVAL:        // Invalid argument
        case EROFS:         // Read-only file system
        case ELOOP:         // Too many symbolic links
        case ENAMETOOLONG:  // File name too long
        case EOVERFLOW:     // Value too large for defined data type
        default:
            return false;
    }
}

off_t IOHandler::getMaxFileSize() {
    // Return maximum file size supported on current platform
#ifdef _WIN32
    // Windows supports very large files with 64-bit operations
    return 0x7FFFFFFFFFFFFFFFLL;  // Maximum signed 64-bit value
#else
    // Unix/Linux - check if we have 64-bit off_t support
    if (sizeof(off_t) == 8) {
        return 0x7FFFFFFFFFFFFFFFLL;  // Maximum signed 64-bit value
    } else {
        return 0x7FFFFFFFL;           // Maximum signed 32-bit value
    }
#endif
}