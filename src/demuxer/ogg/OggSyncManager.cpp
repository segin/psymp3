/*
 * OggSyncManager.cpp - Ogg Sync Layer (RFC 3533)
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill II <segin2005@gmail.com>
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
#endif // !FINAL_BUILD

#include "demuxer/ogg/OggSyncManager.h"
#include <cstring>
#include <stdexcept>

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

OggSyncManager::OggSyncManager(PsyMP3::IO::IOHandler* io_handler)
    : m_io_handler(io_handler), m_logical_offset(0) {
    if (!m_io_handler) {
        throw std::invalid_argument("IOHandler cannot be null");
    }
    ogg_sync_init(&m_sync_state);
}

OggSyncManager::~OggSyncManager() {
    ogg_sync_clear(&m_sync_state);
}

long OggSyncManager::getData(size_t bytes_requested) {
    if (bytes_requested == 0) {
        return 0;
    }

    char* buffer = ogg_sync_buffer(&m_sync_state, bytes_requested);
    if (!buffer) {
        Debug::log("ogg", "OggSyncManager::getData() ogg_sync_buffer failed");
        return -1; // Memory error
    }

    long bytes_read = m_io_handler->read(buffer, 1, bytes_requested);
    Debug::log("ogg", "OggSyncManager::getData() read ", bytes_read, " of ", bytes_requested, " bytes requested");
    if (bytes_read > 0) {
        ogg_sync_wrote(&m_sync_state, bytes_read);
    } else if (bytes_read < 0) {
        Debug::log("ogg", "OggSyncManager::getData() I/O error");
        return -1; // I/O Error
    }

    return bytes_read;
}

int OggSyncManager::getNextPage(ogg_page* page) {
    // libopusfile pattern: loop until we get a page or need more data
    int iteration = 0;
    while (true) {
        // Use pageseek to track exact logical consumption
        int bytes_consumed = ogg_sync_pageseek(&m_sync_state, page);
        
        Debug::log("ogg", "OggSyncManager::getNextPage() iteration ", iteration, " pageseek returned ", bytes_consumed);
        
        if (bytes_consumed > 0) {
            // Found a page, bytes_consumed is the page size
            m_logical_offset += bytes_consumed;
            Debug::log("ogg", "OggSyncManager::getNextPage() got page, size=", bytes_consumed, " new offset=", m_logical_offset);
            return 1;
        }
        
        if (bytes_consumed < 0) {
            // -n bytes were skipped to reach the next page sync
            m_logical_offset += (-bytes_consumed);
            Debug::log("ogg", "OggSyncManager::getNextPage() sync error, skipped ", -bytes_consumed, " bytes, new offset=", m_logical_offset);
            continue; // Continue to try and get the page after skip
        }

        // Need more data (result == 0)
        long bytes_read = getData(4096);
        if (bytes_read <= 0) {
            Debug::log("ogg", "OggSyncManager::getNextPage() EOF/error after ", iteration, " iterations");
            return 0; // EOF or error
        }
        iteration++;
    }
}

int OggSyncManager::getNextPage(SafeOggPage* safe_page) {
    ogg_page temp_page;
    int result = getNextPage(&temp_page);
    if (result == 1) {
        safe_page->clone(&temp_page);
    }
    return result;
}

void OggSyncManager::reset() {
    ogg_sync_reset(&m_sync_state);
}

char* OggSyncManager::getBuffer(size_t size) {
    return ogg_sync_buffer(&m_sync_state, size);
}

int OggSyncManager::wroteBytes(long bytes) {
    return ogg_sync_wrote(&m_sync_state, bytes);
}

int64_t OggSyncManager::findPrevPage() {
    long current_pos = m_io_handler->tell();
    if (current_pos <= 0) return -1;

    const long CHUNK_SIZE = 4096;
    long offset = current_pos;
    int64_t found_offset = -1;
    
    while (offset > 0) {
        long seek_size = (offset > CHUNK_SIZE) ? CHUNK_SIZE : offset;
        offset -= seek_size;
        
        if (!seek(offset)) return -1;
        
        ogg_page temp_page;
        // Scan forward from offset to current_pos
        while (m_io_handler->tell() < current_pos) {
             long page_start_offset = m_logical_offset;
             int page_res = getNextPage(&temp_page);
             if (page_res == 1) {
                 found_offset = page_start_offset;
             } else if (page_res == 0) {
                 break;
             }
        }
        
        if (found_offset != -1) {
             return found_offset;
        }
    }
    return -1;
}

int64_t OggSyncManager::findPrevPageSerial(long serial) {
    long current_pos = m_io_handler->tell();
    long offset = current_pos;
    const long CHUNK_SIZE = 4096;
    
    while (offset > 0) {
        long seek_size = (offset > CHUNK_SIZE) ? CHUNK_SIZE : offset;
        offset -= seek_size;
        
        if (!seek(offset)) return -1;
        
        ogg_page temp_page;
        int64_t found_offset = -1;
        while (m_io_handler->tell() < current_pos) {
            long page_start_offset = m_logical_offset;
            int res = getNextPage(&temp_page);
            if (res == 1) {
                if (ogg_page_serialno(&temp_page) == serial) {
                    found_offset = page_start_offset;
                }
            } else if (res == 0) {
                break;
            }
        }
        if (found_offset != -1) {
            return found_offset;
        }
    }
    return -1;
}

int64_t OggSyncManager::getPosition() const {
    return m_io_handler->tell();
}

int64_t OggSyncManager::getFileSize() {
    // Save current position
    long current = m_io_handler->tell();
    // Seek to end
    m_io_handler->seek(0, SEEK_END);
    long size = m_io_handler->tell();
    // Restore position
    m_io_handler->seek(current, SEEK_SET);
    return size;
}

bool OggSyncManager::seek(int64_t position) {
    reset();
    m_logical_offset = position;
    return m_io_handler->seek(position, SEEK_SET) == 0;
}

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3
