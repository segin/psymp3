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

int IOHandler::seek(long offset, int whence) {
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