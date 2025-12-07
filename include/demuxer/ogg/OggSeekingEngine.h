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
    OggSeekingEngine(OggSyncManager& sync, OggStreamManager& stream);

    /**
     * @brief Seek to a specific granule position
     */
    bool seekToGranule(int64_t granule_pos);

    /**
     * @brief Convert time to granule (codec specific implementation needed really, 
     * but engine facilitates the search)
     */
    
    // Add granule arithmetic helpers
    static int64_t safeGranuleAdd(int64_t a, int64_t b);

private:
    OggSyncManager& m_sync;
    OggStreamManager& m_stream;
};

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

#endif // HAS_OGGSEEKINGENGINE_H
