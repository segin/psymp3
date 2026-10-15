/*
 * OggDemuxer.h - RFC-compliant Ogg Demuxer
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * Main entry point for Ogg container support.
 */

#ifndef OGGDEMUXER_H
#define OGGDEMUXER_H

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
    std::deque<OggPacket> m_packet_queue; // Owning copies; libogg's ogg_packet only borrows its data buffer
    std::vector<std::vector<uint8_t>> header_packets;
};

class OggDemuxer : public Demuxer {
public:
    std::string getContainerName() const override { return "Ogg"; }
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
  bool providesGranulePositions() const override { return true; }
  bool granulesArePageEnds() const override { return true; }

    /**
     * @brief Artist and title from the comments of the chained link playing
     *        now, once the stream has moved past its first link.
     * @return "Artist - Title", or the part that is known; "" before the
     *         first link change.
     */
    std::string getStreamTitle() const override;

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

    // First-sample position of the primary stream, nonzero only for a
    // mid-stream capture (see OggSeekingEngine::getStartGranule()). Subtracted
    // from reported positions/durations so they are stream-relative.
    mutable std::atomic<int64_t> m_start_granule{0};

    // Async duration calculation
    mutable std::atomic<bool> m_calculating_duration{false};
    mutable std::future<void> m_duration_future;

    // Chained Ogg: a radio station that restarts its encoder for each song
    // sends one logical stream after another. Each later link takes over the
    // primary stream's key, so the stream id callers hold stays valid.
    std::map<int, int> m_serial_alias;      // a later link's serial -> the primary key
    std::deque<MediaChunk> m_link_chunks;   // a new link's in-band headers, sent before its audio
    bool m_link_flag_pending = false;       // flag the next primary chunk new_link
    std::string m_link_title;               // the current link's "Artist - Title"

    /**
     * @brief The key in m_streams/m_parsers that pages of @p serial belong
     *        to: the primary key for a later chained link, else the serial.
     * @param serial A page's serial number.
     * @return The key.
     */
    int streamKeyForSerial_unlocked(int serial) const;

    /**
     * @brief Start the next link of a chained stream from its BOS page: read
     *        pages until its headers are complete, then put its stream and
     *        parser in place of the primary's, queue its in-band header
     *        packets (Vorbis) and flag the first chunk new_link.
     * @param bos_page The link's first page.
     * @return false if the link cannot be used (unknown codec, or the stream
     *         ended inside its headers); m_eof is then set.
     */
    bool beginNextLink_unlocked(ogg_page* bos_page);

    // Unlocked implementations
    MediaChunk readChunk_unlocked(uint32_t stream_id);
    bool seekTo_unlocked(uint64_t timestamp_ms);
    long getSampleRate() const;
    bool resetForPlayback_unlocked();
    
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
