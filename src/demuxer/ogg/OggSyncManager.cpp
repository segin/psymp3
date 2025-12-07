/*
 * OggSyncManager.cpp - Ogg Sync Layer (RFC 3533)
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "demuxer/ogg/OggSyncManager.h"
#include <cstring>
#include <stdexcept>

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

OggSyncManager::OggSyncManager(PsyMP3::IO::IOHandler* io_handler)
    : m_io_handler(io_handler) {
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
        return -1; // Memory error
    }

    long bytes_read = m_io_handler->read(buffer, 1, bytes_requested);
    if (bytes_read > 0) {
        ogg_sync_wrote(&m_sync_state, bytes_read);
    } else if (bytes_read < 0) {
        return -1; // I/O Error
    }

    return bytes_read;
}

int OggSyncManager::getNextPage(ogg_page* page) {
    // libopusfile pattern: loop until we get a page or need more data
    while (true) {
        int result = ogg_sync_pageout(&m_sync_state, page);
        if (result > 0) {
            return 1; // Got a page
        }
        if (result < 0) {
            // Stream error, typically means loss of sync or corrupt data.
            // RFC 3533 says we should skip bytes to resync.
            // libogg does this internally but reports error.
            // We'll continue trying to find the next valid page.
             continue;
        }

        // Need more data
        long bytes_read = getData(4096);
        if (bytes_read <= 0) {
            return 0; // EOF or error
        }
    }
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

int OggSyncManager::getPrevPage(ogg_page* page) {
    long current_pos = m_io_handler->tell();
    if (current_pos <= 0) return 0;

    const long CHUNK_SIZE = 4096;
    long offset = current_pos;
    
    while (offset > 0) {
        long seek_size = (offset > CHUNK_SIZE) ? CHUNK_SIZE : offset;
        offset -= seek_size;
        
        if (m_io_handler->seek(offset, SEEK_SET) != 0) return -1;
        reset();
        
        bool found_any = false;
        ogg_page temp_page;
        
        // Scan forward from offset to current_pos
        while (m_io_handler->tell() < current_pos) {
             int page_res = getNextPage(&temp_page);
             if (page_res == 1) {
                 found_any = true;
                 *page = temp_page; // Shallow copy of struct
                 // Note: buffer pointers in page point to sync state memory. 
                 // They remain valid until we reset/clear or write enough to cause reallocation.
             }
        }
        
        if (found_any) {
             return 1;
        }
    }
    return 0;
}

int OggSyncManager::getPrevPageSerial(ogg_page* page, long serial) {
    long current_pos = m_io_handler->tell();
    long offset = current_pos;
    const long CHUNK_SIZE = 4096;
    
    while (offset > 0) {
        long seek_size = (offset > CHUNK_SIZE) ? CHUNK_SIZE : offset;
        offset -= seek_size;
        
        if (m_io_handler->seek(offset, SEEK_SET) != 0) return -1;
        reset();
        
        ogg_page temp_page;
        while (getNextPage(&temp_page) == 1) {
            if (ogg_page_serialno(&temp_page) == serial) {
                *page = temp_page;
                return 1;
            }
        }
    }
    return 0;
}

int64_t OggSyncManager::getPosition() const {
    return m_io_handler->tell();
}

int64_t OggSyncManager::getFileSize() const {
    // Save current position
    long current = m_io_handler->tell();
    // Seek to end
    const_cast<PsyMP3::IO::IOHandler*>(m_io_handler)->seek(0, SEEK_END);
    long size = m_io_handler->tell();
    // Restore position
    const_cast<PsyMP3::IO::IOHandler*>(m_io_handler)->seek(current, SEEK_SET);
    return size;
}

bool OggSyncManager::seek(int64_t position) {
    reset();
    return m_io_handler->seek(position, SEEK_SET) == 0;
}

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3
