/*
 * OggDemuxer.h - RFC-compliant Ogg Demuxer
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * Main entry point for Ogg container support.
 */

#ifndef OGGDEMUXER_H
#define OGGDEMUXER_H

#include "demuxer/Demuxer.h"
#include "OggSyncManager.h"
#include "OggStreamManager.h"
#include "CodecHeaderParser.h"
#include <memory>
#include <map>
#include <mutex>
#include <deque>
#include <atomic>
#include <future>
#include <ogg/ogg.h>

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

struct OggPacket {
    std::vector<uint8_t> data;
    uint64_t granule_position;
    bool is_keyframe;
};

struct OggStream {
    uint32_t serial_number;
    std::string codec_name;
    std::string codec_type;
    uint32_t sample_rate;
    uint16_t channels;
    uint64_t pre_skip = 0;
    bool headers_complete = false;
    bool headers_sent = false;
    std::deque<ogg_packet> m_packet_queue; // Using libogg's ogg_packet
    std::vector<std::vector<uint8_t>> header_packets;
};

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
    mutable std::atomic<bool> m_duration_calculated;
    mutable std::atomic<uint64_t> m_cached_duration;

    // Async duration calculation
    mutable std::atomic<bool> m_calculating_duration{false};
    mutable std::future<void> m_duration_future;

    // Unlocked implementations
    MediaChunk readChunk_unlocked(uint32_t stream_id);
    bool seekTo_unlocked(uint64_t timestamp_ms);
    long getSampleRate() const;
    
    /**
     * @brief Create VorbisCommentTag from parsed header data
     * Called after headers are complete to populate m_tag
     */
    void createTagFromMetadata_unlocked();

    /**
     * @brief Calculate duration during container parsing
     * Runs on loader thread to prevent UI blocking
     */
    void calculateInitialDuration_unlocked();

public:
    // --- Legacy / Testing members required by unit tests ---
    
    std::map<uint32_t, OggStream>& getStreamsForTesting() { return m_test_streams; }

    uint64_t granuleToMs(uint64_t granule, uint32_t stream_id) const;
    uint64_t msToGranule(uint64_t timestamp_ms, uint32_t stream_id) const;

    int fetchAndProcessPacket();
    void fillPacketQueue(uint32_t stream_id);

    // Enhanced memory safety (Testing/Internal)
    bool performMemoryAudit();
    void enforceMemoryLimits();
    bool validateLiboggStructures();
    void performPeriodicMaintenance();

    bool performMemoryAudit_unlocked();
    void enforceMemoryLimits_unlocked();
    bool validateLiboggStructures_unlocked();
    void performPeriodicMaintenance_unlocked();

    static constexpr size_t MAX_PACKET_QUEUE_SIZE = 100;

private:
    std::map<uint32_t, OggStream> m_test_streams;
};

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

#endif // OGGDEMUXER_H
