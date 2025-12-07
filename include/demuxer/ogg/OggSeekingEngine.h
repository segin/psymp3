/*
 * OggSeekingEngine.h - Seeking and Granule Arithmetic
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * Implements bisection search and granule arithmetic following libopusfile/libvorbisfile patterns.
 */

#ifndef HAS_OGGSEEKINGENGINE_H
#define HAS_OGGSEEKINGENGINE_H

#include <cstdint>
#include "OggSyncManager.h"
#include "OggStreamManager.h"

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

    // --- Bisection Search (Task 13) ---
    bool seekToGranule(int64_t granule_pos);
    bool seekToTime(double time_seconds);

    // Setters
    void setSampleRate(long rate) { m_sample_rate = rate; }

private:
    OggSyncManager& m_sync;
    OggStreamManager& m_stream;
    long m_sample_rate;
    
    // Internal bisection helper
    bool bisectForward(int64_t target_granule, int64_t begin, int64_t end);
};

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

#endif // HAS_OGGSEEKINGENGINE_H
