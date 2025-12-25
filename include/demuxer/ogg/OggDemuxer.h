/*
 * OggDemuxer.h - RFC-compliant Ogg Demuxer
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * Main entry point for Ogg container support.
 */

#ifndef OGGDEMUXER_H
#define OGGDEMUXER_H

#include "../../../include/demuxer/Demuxer.h"
#include "OggSyncManager.h"
#include "OggStreamManager.h"
#include "CodecHeaderParser.h"
#include <memory>
#include <map>
#include <mutex>

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

class OggDemuxer : public Demuxer {
public:
    explicit OggDemuxer(std::unique_ptr<PsyMP3::IO::IOHandler> handler);
    ~OggDemuxer() override;

    // Demuxer Interface
    bool parseContainer() override;
    std::vector<StreamInfo> getStreams() const override;
    StreamInfo getStreamInfo(uint32_t stream_id) const override;
    MediaChunk readChunk() override; // Primary stream
    MediaChunk readChunk(uint32_t stream_id) override;
    
    bool seekTo(uint64_t timestamp_ms) override;
    bool isEOF() const override;
    uint64_t getDuration() const override;
    uint64_t getPosition() const override;
    uint64_t getGranulePosition(uint32_t stream_id) const override;

    // Registration
    static bool registerDemuxer();

private:
    // Thread safety
    mutable std::recursive_mutex m_ogg_mutex;

    // Components
    std::unique_ptr<OggSyncManager> m_sync;
    std::map<int, std::unique_ptr<OggStreamManager>> m_streams;
    std::map<int, std::unique_ptr<CodecHeaderParser>> m_parsers;
    int m_primary_serial;
    bool m_has_primary_serial;
    bool m_eof;

    // Unlocked implementations
    MediaChunk readChunk_unlocked(uint32_t stream_id);
    bool seekTo_unlocked(uint64_t timestamp_ms);
    long getSampleRate() const;
    
    /**
     * @brief Create VorbisCommentTag from parsed header data
     * Called after headers are complete to populate m_tag
     */
    void createTagFromMetadata_unlocked();
};

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

#endif // OGGDEMUXER_H
