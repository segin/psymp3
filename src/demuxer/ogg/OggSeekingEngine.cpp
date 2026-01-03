/*
 * OggSeekingEngine.cpp - Seeking and Granule Arithmetic
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "demuxer/ogg/OggSeekingEngine.h"
#include <limits>
#include <cmath>

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

OggSeekingEngine::OggSeekingEngine(OggSyncManager& sync, OggStreamManager& stream, long sample_rate)
    : m_sync(sync), m_stream(stream), m_sample_rate(sample_rate) {
}

// --- Granule Arithmetic ---

int64_t OggSeekingEngine::safeGranuleAdd(int64_t a, int64_t b) {
    // Prevent overflow
    if (b > 0 && a > std::numeric_limits<int64_t>::max() - b) {
        return std::numeric_limits<int64_t>::max();
    }
    if (b < 0 && a < std::numeric_limits<int64_t>::min() - b) {
        return std::numeric_limits<int64_t>::min();
    }
    return a + b;
}

int64_t OggSeekingEngine::safeGranuleSub(int64_t a, int64_t b) {
    // Prevent underflow
    if (b < 0 && a > std::numeric_limits<int64_t>::max() + b) {
        return std::numeric_limits<int64_t>::max();
    }
    if (b > 0 && a < std::numeric_limits<int64_t>::min() + b) {
        return std::numeric_limits<int64_t>::min();
    }
    return a - b;
}

bool OggSeekingEngine::isValidGranule(int64_t granule) {
    return granule >= 0; // -1 is used as "unknown" in Ogg
}

double OggSeekingEngine::granuleToTime(int64_t granule) const {
    if (m_sample_rate == 0 || granule < 0) return 0.0;
    return static_cast<double>(granule) / static_cast<double>(m_sample_rate);
}

int64_t OggSeekingEngine::timeToGranule(double time_seconds) const {
    if (m_sample_rate == 0 || time_seconds < 0.0) return 0;
    return static_cast<int64_t>(time_seconds * static_cast<double>(m_sample_rate));
}

// --- Duration Calculation ---

int64_t OggSeekingEngine::getLastGranule() {
    // Save current logical position
    int64_t saved_pos = m_sync.getLogicalPosition();
    int serial = m_stream.getSerialNumber();
    
    // Seek to end
    int64_t file_size = m_sync.getFileSize();
    if (file_size <= 0) return -1;
    
    // Progressive search from end to find last page
    int64_t search_size = 65536; // Start with 64KB
    int64_t last_granule = -1;
    
    while (search_size <= 1024 * 1024) { // Up to 1MB logic, but also handles small files
        int64_t search_pos = file_size - search_size;
        if (search_pos < 0) search_pos = 0;
        
        m_sync.seek(search_pos);
        
        ogg_page page;
        bool found_any_gp = false;
        
        // Scan forward to find last page with valid granule for our stream
        while (m_sync.getNextPage(&page) == 1) {
            if (ogg_page_serialno(&page) == serial) {
                int64_t gp = ogg_page_granulepos(&page);
                if (gp >= 0) {
                    last_granule = gp;
                    found_any_gp = true;
                }
            }
        }
        
        if (found_any_gp || search_pos == 0) {
            break;
        }
        
        search_size *= 2; // Expand search window
    }
    
    // Restore position
    m_sync.seek(saved_pos);
    
    return last_granule;
}

double OggSeekingEngine::calculateDuration() {
    int64_t last = getLastGranule();
    if (last < 0) return 0.0;
    return granuleToTime(last);
}

// --- Bisection Search ---

bool OggSeekingEngine::seekToGranule(int64_t granule_pos) {
    if (granule_pos < 0) return false;
    
    int64_t file_size = m_sync.getFileSize();
    if (file_size <= 0) return false;
    
    return bisectForward(granule_pos, 0, file_size);
}

bool OggSeekingEngine::seekToTime(double time_seconds) {
    int64_t target_granule = timeToGranule(time_seconds);
    return seekToGranule(target_granule);
}

bool OggSeekingEngine::bisectForward(int64_t target_granule, int64_t begin, int64_t end) {
    const int MAX_ITERATIONS = 50;
    int iterations = 0;
    int serial = m_stream.getSerialNumber();
    
    while (end - begin > 8192 && iterations < MAX_ITERATIONS) {
        int64_t mid = begin + (end - begin) / 2;
        
        m_sync.seek(mid);
        
        ogg_page page;
        bool found_stream_page = false;
        
        // Find first page for our stream after mid
        while (m_sync.getNextPage(&page) == 1) {
            if (ogg_page_serialno(&page) == serial) {
                found_stream_page = true;
                break;
            }
        }
        
        if (!found_stream_page) {
            // Failed to find target stream in this half, move end closer
            end = mid;
            iterations++;
            continue;
        }
        
        int64_t gp = ogg_page_granulepos(&page);
        if (gp < 0) {
            // Page has no granule, search further forward
            // (A better implementation would look for the next page with a GP)
            // But for now, we'll treat it as unknown and move begin.
            begin = m_sync.getLogicalPosition(); 
            iterations++;
            continue;
        }
        
        if (gp < target_granule) {
            begin = m_sync.getLogicalPosition();
        } else {
            end = mid;
        }
        iterations++;
    }
    
    // Final positioning: seek to the offset found by begin and reset stream
    m_sync.seek(begin);
    m_stream.reset();
    
    return true;
}

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

