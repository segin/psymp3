/*
 * OggSeekingEngine.h - Seeking and Granule Arithmetic
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * Implements bisection search and granule arithmetic following libopusfile/libvorbisfile patterns.
 */

#ifndef HAS_OGGSEEKINGENGINE_H
#define HAS_OGGSEEKINGENGINE_H

#include <cstdint>
#include "io/IOHandler.h"
#include "demuxer/ogg/OggSyncManager.h"
#include "demuxer/ogg/OggStreamManager.h"

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

class OggSeekingEngine {
public:
    OggSeekingEngine(OggSyncManager& sync, OggStreamManager& stream, long sample_rate = 48000);

    // --- Granule Arithmetic (Task 11) ---
    static int64_t safeGranuleAdd(int64_t a, int64_t b);
    static int64_t safeGranuleSub(int64_t a, int64_t b);
    static bool isValidGranule(int64_t granule);
    
    // Time conversion
    double granuleToTime(int64_t granule) const;
    int64_t timeToGranule(double time_seconds) const;

    // --- Duration Calculation (Task 12) ---
    int64_t getLastGranule();
    double calculateDuration();

    // Sample position of the first sample in the stream. This is 0 for a normal
    // stream that begins at sample 0 and nonzero only for a mid-stream capture
    // (a live broadcast joined in progress), whose granule positions are offset
    // by the join point (RFC 9639 Section 10.1). Subtract it to report
    // stream-relative durations and positions.
    int64_t getStartGranule();

    // --- Bisection Search (Task 13) ---
    bool seekToGranule(int64_t granule_pos);
    bool seekToTime(double time_seconds);

    // Setters
    void setSampleRate(long rate) { m_sample_rate = rate; }

private:
    OggSyncManager& m_sync;
    OggStreamManager& m_stream;
    long m_sample_rate;
    
    // Duration caching to prevent state corruption during playback
    // The first call to getLastGranule() scans the file; subsequent calls return cached value
    int64_t m_cached_last_granule = -1;
    bool m_duration_cached = false;

    // Cached first-sample position of the stream (see getStartGranule()).
    int64_t m_start_granule = 0;
    bool m_start_cached = false;
    
    // Internal bisection helper
    bool bisectForward(int64_t target_granule, int64_t begin, int64_t end);
};

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

#endif // HAS_OGGSEEKINGENGINE_H
