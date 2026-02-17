/*
 * MemoryIOHandler.cpp - Memory-based IOHandler implementation
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

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif
#include "io/MemoryIOHandler.h"
#include <algorithm>
#include <cstring>
#include <mutex>
#include <shared_mutex>

namespace PsyMP3 {
namespace IO {

MemoryIOHandler::MemoryIOHandler(const void* data, size_t size, bool copy)
    : m_own_buffer(copy), m_pos(0), m_discarded_bytes(0) {
    if (copy) {
        if (data && size > 0) {
            m_buffer.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
            updateMemoryUsage(m_buffer.capacity());
        }
    } else {
        m_external_data = static_cast<const uint8_t*>(data);
        m_external_size = size;
    }
}

MemoryIOHandler::MemoryIOHandler()
    : m_own_buffer(true), m_pos(0), m_discarded_bytes(0) {
}

MemoryIOHandler::~MemoryIOHandler() {
    if (m_own_buffer) {
        updateMemoryUsage(0);
    }
}


size_t MemoryIOHandler::read(void* buffer, size_t size, size_t count) {
    // We override read() completely to ensure exclusive access because we update m_pos
    std::unique_lock<std::shared_mutex> lock(m_operation_mutex);

    if (m_closed.load()) {
        updateErrorState(EBADF);
        return 0;
    }

    size_t bytes_requested = size * count;
    if (bytes_requested == 0) return 0;

    size_t available = 0;
    const uint8_t* source = nullptr;

    if (m_own_buffer) {
        if (m_pos < m_buffer.size()) {
            available = m_buffer.size() - m_pos;
            source = m_buffer.data() + m_pos;
        }
    } else {
        if (m_pos < m_external_size) {
            available = m_external_size - m_pos;
            source = m_external_data + m_pos;
        }
    }

    size_t to_read = std::min(bytes_requested, available);

    if (to_read > 0) {
        std::memcpy(buffer, source, to_read);
        m_pos += to_read;
        updatePosition(m_pos); // Update atomic base class position too
    }

    if (m_pos >= (m_own_buffer ? m_buffer.size() : m_external_size)) {
        updateEofState(true);
    } else {
        updateEofState(false);
    }

    return (size > 0) ? (to_read / size) : 0;
}

int MemoryIOHandler::seek(off_t offset, int whence) {
    std::unique_lock<std::shared_mutex> lock(m_operation_mutex);

    if (m_closed.load()) {
        updateErrorState(EBADF);
        return -1;
    }

    // Virtual file size is accumulated discarded + current buffer size
    // Note: external buffer logic implies static content, so m_discarded_bytes should be 0 there.
    size_t logical_size = (m_own_buffer ? m_buffer.size() : m_external_size) + m_discarded_bytes;

    // Current logical position
    off_t logical_pos = static_cast<off_t>(m_pos + m_discarded_bytes);
    off_t new_logical_pos = logical_pos;

    switch (whence) {
        case SEEK_SET:
            new_logical_pos = offset;
            break;
        case SEEK_CUR:
            new_logical_pos = logical_pos + offset;
            break;
        case SEEK_END:
            // Since we are streaming and potentially growing, SEEK_END is tricky.
            // If we treat it as a growing stream, SEEK_END usually means end of current available data.
            new_logical_pos = static_cast<off_t>(logical_size) + offset;
            break;
        default:
            updateErrorState(EINVAL);
            return -1;
    }

    if (new_logical_pos < 0) {
        updateErrorState(EINVAL);
        return -1;
    }

    // Convert back to physical buffer offset
    if (static_cast<size_t>(new_logical_pos) < m_discarded_bytes) {
        // Seeking before the current window
        // This is invalid for a discarded stream.
        // We return error.
        updateErrorState(EINVAL, "Cannot seek to discarded data");
        return -1;
    }

    size_t new_buffer_pos = static_cast<size_t>(new_logical_pos) - m_discarded_bytes;

    // It is valid to seek past end of buffer (read will return 0)
    m_pos = new_buffer_pos;

    // Update base class position to logical position
    updatePosition(new_logical_pos);

    size_t current_buffer_size = m_own_buffer ? m_buffer.size() : m_external_size;
    if (m_pos >= current_buffer_size) {
        updateEofState(true);
    } else {
        updateEofState(false);
    }

    return 0;
}

off_t MemoryIOHandler::tell() {
    std::shared_lock<std::shared_mutex> lock(m_operation_mutex);
    // Return logical position
    return static_cast<off_t>(m_pos + m_discarded_bytes);
}

int MemoryIOHandler::close() {
    std::unique_lock<std::shared_mutex> lock(m_operation_mutex);
    m_closed.store(true);
    m_buffer.clear();
    m_buffer.shrink_to_fit();
    updateMemoryUsage(0);
    return 0;
}

bool MemoryIOHandler::eof() {
    std::shared_lock<std::shared_mutex> lock(m_operation_mutex);
    size_t total_size = m_own_buffer ? m_buffer.size() : m_external_size;
    return m_pos >= total_size;
}

off_t MemoryIOHandler::getFileSize() {
    std::shared_lock<std::shared_mutex> lock(m_operation_mutex);
    return static_cast<off_t>(m_own_buffer ? m_buffer.size() : m_external_size);
}

size_t MemoryIOHandler::write(const void* data, size_t size) {
    std::unique_lock<std::shared_mutex> lock(m_operation_mutex);

    if (!m_own_buffer) {
        // Cannot write to external buffer reference
        return 0;
    }

    if (size == 0) return 0;



    // Check memory limits before allocation
    if (!checkMemoryLimits(size)) {
        if (!handleMemoryAllocationFailure(size, "MemoryIOHandler::write")) {
            return 0;
        }
    }

    try {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        m_buffer.insert(m_buffer.end(), p, p + size);
        updateMemoryUsage(m_buffer.capacity());

        // If we were at EOF, we might not be anymore
        updateEofState(m_pos >= m_buffer.size());

        return size;
    } catch (const std::bad_alloc&) {
        handleMemoryAllocationFailure(size, "MemoryIOHandler::write bad_alloc");
        return 0;
    }
}

void MemoryIOHandler::discard(size_t count) {
    std::unique_lock<std::shared_mutex> lock(m_operation_mutex);

    if (!m_own_buffer || m_buffer.empty() || count == 0) return;

    size_t to_remove = std::min(count, m_buffer.size());

    m_buffer.erase(m_buffer.begin(), m_buffer.begin() + to_remove);
    updateMemoryUsage(m_buffer.capacity());

    // Update discarded bytes counter for virtual positioning
    m_discarded_bytes += to_remove;

    // Adjust physical position
    if (m_pos >= to_remove) {
        m_pos -= to_remove;
    } else {
        // If we discarded past the current position, we are now at 0 (start of remaining buffer)
        // This implicitly moves the file pointer forward if it was trailing behind
        m_pos = 0;
    }

    // updatePosition reflects LOGICAL position, which shouldn't change just because we discarded data
    // unless the read pointer was implicitly moved.
    // However, if we discard data *before* the read pointer, the read pointer's logical position is unchanged.
    // If we discard data *including* the read pointer, the read pointer is now at the new start.
    // Let's recalculate logical position.
    updatePosition(static_cast<off_t>(m_pos + m_discarded_bytes));
}

void MemoryIOHandler::discardRead() {
    std::unique_lock<std::shared_mutex> lock(m_operation_mutex);

    if (!m_own_buffer || m_buffer.empty() || m_pos == 0) return;

    // We want to discard m_pos bytes (the ones already read)
    size_t to_remove = std::min(m_pos, m_buffer.size());

    m_buffer.erase(m_buffer.begin(), m_buffer.begin() + to_remove);
    updateMemoryUsage(m_buffer.capacity());

    // Update discarded bytes counter
    m_discarded_bytes += to_remove;

    // Adjust physical position
    m_pos -= to_remove;

    // Logical position remains the same (we are just shifting the window)
    // updatePosition(static_cast<off_t>(m_pos + m_discarded_bytes)); // Same value
}

void MemoryIOHandler::clear() {
    std::unique_lock<std::shared_mutex> lock(m_operation_mutex);
    if (m_own_buffer) {
        m_buffer.clear();
        updateMemoryUsage(m_buffer.capacity());
    }
    m_pos = 0;
    m_discarded_bytes = 0;
    updatePosition(0);
}

} // namespace IO
} // namespace PsyMP3
