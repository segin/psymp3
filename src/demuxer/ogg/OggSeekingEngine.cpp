/*
 * OggSeekingEngine.cpp - Seeking and Granule Arithmetic
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

#include <cstdint>
#include <limits>

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
    // Return cached value if already calculated (prevents state corruption during playback)
    if (m_duration_cached) {
        Debug::log("ogg", "OggSeekingEngine::getLastGranule() returning cached granule: ", m_cached_last_granule);
        return m_cached_last_granule;
    }
    
    Debug::log("ogg", "OggSeekingEngine::getLastGranule() scanning file for duration...");
    
    // Save current logical position
    int64_t saved_pos = m_sync.getLogicalPosition();
    int serial = m_stream.getSerialNumber();
    
    // Seek to end
    int64_t file_size = m_sync.getFileSize();
    if (file_size <= 0) return -1;
    
    // Progressive backward search from the end. Keep expanding the window until
    // a page with a granule for our stream is found, or the whole file has been
    // scanned (search_pos == 0, which always terminates the loop). The previous
    // 1 MB cap gave up early and returned duration 0 on chained files or files
    // with >1 MB of trailing data, where the primary stream's last page lies
    // further back.
    int64_t search_size = 65536; // Start with 64KB
    int64_t last_granule = -1;

    while (true) {
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
    
    // Cache the result so we never re-scan during playback
    m_cached_last_granule = last_granule;
    m_duration_cached = true;
    
    Debug::log("ogg", "OggSeekingEngine::getLastGranule() found and cached granule: ", last_granule);
    
    return last_granule;
}


int64_t OggSeekingEngine::getStartGranule() {
    if (m_start_cached) {
        return m_start_granule;
    }

    int64_t saved_pos = m_sync.getLogicalPosition();
    int serial = m_stream.getSerialNumber();

    m_sync.seek(0);

    // Find the granule positions of the first two data pages of our stream.
    // Header pages carry granule 0 (RFC 9639 Section 10.1), so the first two
    // pages with a positive, increasing granule are the first two audio pages.
    int64_t first_gp = -1;
    int64_t second_gp = -1;
    ogg_page page;
    int scanned = 0;
    const int MAX_PAGES = 4096; // bound the scan on pathological input
    while (m_sync.getNextPage(&page) == 1 && scanned < MAX_PAGES) {
        ++scanned;
        if (ogg_page_serialno(&page) != serial) {
            continue;
        }
        int64_t gp = ogg_page_granulepos(&page);
        if (gp <= 0) {
            continue;
        }
        if (first_gp < 0) {
            first_gp = gp;
        } else if (gp > first_gp) {
            second_gp = gp;
            break;
        }
    }

    m_sync.seek(saved_pos);

    // The samples on the first data page are approximated by the granule delta
    // to the second data page (exact for fixed-block-size streams), so the
    // stream begins at first_gp - that span. Only honor a base that is
    // unambiguously a mid-stream join: a stream starting at sample 0 yields a
    // base at or near zero, and the block-size estimate can drift by a block or
    // two for variable block sizes, so treat anything below one second as zero
    // to guarantee normal files are unaffected.
    int64_t start = 0;
    if (first_gp > 0 && second_gp > first_gp) {
        int64_t base = first_gp - (second_gp - first_gp);
        if (base >= m_sample_rate) {
            start = base;
        }
    }

    m_start_granule = start;
    m_start_cached = true;
    Debug::log("ogg", "OggSeekingEngine::getStartGranule() base granule: ", start);
    return start;
}

double OggSeekingEngine::calculateDuration() {
    int64_t last = getLastGranule();
    if (last < 0) return 0.0;
    int64_t span = last - getStartGranule();
    if (span < 0) span = last; // defensive: never report negative duration
    return granuleToTime(span);
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

    // Granule the page we finally land on starts at. The refinement below
    // often breaks on the very first page it reads, having narrowed the range
    // to a couple of kilobytes, so it cannot be relied on to observe a page
    // before the target. The bisection is therefore the one that has to
    // remember: every time it moves `begin` past a page, that page's granule
    // is where the next one starts. Zero is right at the head of the stream.
    m_landing_granule = 0;
    
    while (end - begin > 2048 && iterations < MAX_ITERATIONS) {
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
            m_landing_granule = gp;
        } else {
            end = mid;
        }
        iterations++;
    }
    
    // Linear refinement: Ensure we are exactly at the right page
    // The bisection might have left us at 'begin', which could be a few pages before the target.
    m_sync.seek(begin);
    m_stream.reset();
    
    ogg_page page;
    int64_t best_offset = begin;
    
    // Scan forward up to a reasonable limit or until we find the page
    // (In case file is huge and bisection failed oddly, don't scan forever, 
    // but typically we are very close).
    // The previous loop ensures we are within a small range of the target.
    
    while (m_sync.getNextPage(&page) == 1) {
        if (ogg_page_serialno(&page) == serial) {
            int64_t gp = ogg_page_granulepos(&page);
            if (gp != -1) {
                if (gp < target_granule) {
                    // This page is entirely before the target.
                    // The next page *might* be the one.
                    // Update best_offset to the *end* of this page (current logical pos)
                    best_offset = m_sync.getLogicalPosition();
                    // Whatever page follows starts where this one ended.
                    m_landing_granule = gp;
                } else {
                    // This page contains the target (or is the first one after).
                    // We want to start processing FROM this page.
                    // The 'best_offset' should be the start of THIS page.
                    // But wait, 'getLogicalPosition' returns offset AFTER the page we just read.
                    // We need the offset of the page we just read.
                    // OggSyncManager doesn't easily convert "current page" to "start offset" 
                    // unless we tracked it.
                    // However, 'best_offset' tracked the end of the *previous* page.
                    // So if we seek to 'best_offset', we will read this page next.
                    break;
                }
            }
        }
    }
    
    m_sync.seek(best_offset);
    m_stream.reset();
    
    return true;
}

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

