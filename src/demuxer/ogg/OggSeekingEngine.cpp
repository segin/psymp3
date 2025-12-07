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
    // Save current position
    int64_t saved_pos = m_sync.getPosition();
    
    // Seek to end
    int64_t file_size = m_sync.getFileSize();
    if (file_size <= 0) return -1;
    
    // Seek backward from end to find last page
    int64_t search_pos = file_size - 65536; // Last 64KB
    if (search_pos < 0) search_pos = 0;
    
    m_sync.seek(search_pos);
    
    ogg_page page;
    int64_t last_granule = -1;
    
    // Scan forward to find last page with valid granule
    while (m_sync.getNextPage(&page) == 1) {
        int64_t gp = ogg_page_granulepos(&page);
        if (gp >= 0) {
            last_granule = gp;
        }
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
    // Simple bisection search implementation
    // This is a simplified version; production would need more refinement
    
    const int MAX_ITERATIONS = 50;
    int iterations = 0;
    
    while (end - begin > 8192 && iterations < MAX_ITERATIONS) {
        int64_t mid = begin + (end - begin) / 2;
        
        m_sync.seek(mid);
        
        ogg_page page;
        if (m_sync.getNextPage(&page) != 1) {
            // Failed to get page, try a bit later
            begin = mid + 1;
            iterations++;
            continue;
        }
        
        int64_t gp = ogg_page_granulepos(&page);
        if (gp < 0) {
            // Page has no granule, try next
            begin = mid + 1;
            iterations++;
            continue;
        }
        
        if (gp < target_granule) {
            begin = mid;
        } else {
            end = mid;
        }
        iterations++;
    }
    
    // Final positioning: seek to begin and find the exact page
    m_sync.seek(begin);
    
    // Reset stream state
    m_stream.reset();
    
    return true;
}

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

