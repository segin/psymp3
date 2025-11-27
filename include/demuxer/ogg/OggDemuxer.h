/*
 * OggDemuxer.h - Ogg container demuxer
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * This is a placeholder header for the new RFC-compliant OggDemuxer implementation.
 * The implementation follows the ogg-demuxer-fix spec which requires:
 * - Exact behavior patterns from libvorbisfile and libopusfile reference implementations
 * - RFC 3533 compliance for Ogg container format
 * - RFC 7845 compliance for Opus encapsulation
 * - RFC 9639 Section 10.1 compliance for FLAC-in-Ogg
 * - Property-based testing with RapidCheck
 */

#ifndef OGGDEMUXER_H
#define OGGDEMUXER_H

// No direct includes - all includes should be in psymp3.h

// OggDemuxer is built if any Ogg-based codec is enabled
#ifdef HAVE_OGGDEMUXER

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

/**
 * @brief Ogg page header structure (RFC 3533 Section 6)
 */
struct OggPageHeader {
    uint8_t capture_pattern[4];    // "OggS" (0x4f676753)
    uint8_t version;               // Stream structure version (must be 0)
    uint8_t header_type;           // Header type flags
    uint64_t granule_position;     // Granule position (codec-specific)
    uint32_t serial_number;        // Bitstream serial number
    uint32_t page_sequence;        // Page sequence number
    uint32_t checksum;             // CRC32 checksum (polynomial 0x04c11db7)
    uint8_t page_segments;         // Number of segments in page (0-255)
    
    // Header type flags (RFC 3533 Section 6)
    static constexpr uint8_t CONTINUED_PACKET = 0x01;  // Continuation flag
    static constexpr uint8_t FIRST_PAGE = 0x02;        // BOS flag
    static constexpr uint8_t LAST_PAGE = 0x04;         // EOS flag
    
    bool isContinuedPacket() const { return header_type & CONTINUED_PACKET; }
    bool isFirstPage() const { return header_type & FIRST_PAGE; }
    bool isLastPage() const { return header_type & LAST_PAGE; }
};

/**
 * @brief Ogg packet data
 */
struct OggPacket {
    uint32_t stream_id;
    std::vector<uint8_t> data;
    uint64_t granule_position;
    bool is_first_packet;
    bool is_last_packet;
    bool is_continued;
};

/**
 * @brief Information about an Ogg logical bitstream
 */
struct OggStream {
    uint32_t serial_number;
    std::string codec_name;        // "vorbis", "flac", "opus", "speex", "theora"
    std::string codec_type;        // "audio", "video", "subtitle"
    
    // Header management
    std::vector<OggPacket> header_packets;
    bool headers_complete = false;
    bool headers_sent = false;
    size_t next_header_index = 0;
    uint16_t expected_header_count = 0;  // FLAC-in-Ogg: from identification header
    
    // Audio properties
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
    uint32_t bitrate = 0;
    uint64_t total_samples = 0;
    uint64_t pre_skip = 0;              // Opus-specific
    uint8_t bits_per_sample = 0;        // FLAC-specific
    
    // FLAC-in-Ogg specific (RFC 9639 Section 10.1)
    uint8_t flac_mapping_version_major = 0;
    uint8_t flac_mapping_version_minor = 0;
    uint16_t flac_min_block_size = 0;
    uint16_t flac_max_block_size = 0;
    uint32_t flac_min_frame_size = 0;
    uint32_t flac_max_frame_size = 0;
    
    // Metadata
    std::string artist;
    std::string title;
    std::string album;
    
    // Packet buffering
    std::deque<OggPacket> m_packet_queue;
    uint64_t total_samples_processed = 0;
    
    // Page sequence tracking
    uint32_t last_page_sequence = 0;
    bool page_sequence_initialized = false;
};

/**
 * @brief Ogg container demuxer using libogg
 * 
 * This demuxer implements RFC 3533 compliant Ogg container parsing with support for:
 * - Ogg Vorbis (.ogg)
 * - Ogg Opus (.opus, .oga)
 * - FLAC-in-Ogg (.oga) per RFC 9639 Section 10.1
 * - Ogg Speex (.spx)
 * 
 * The implementation follows the exact behavior patterns of libvorbisfile and libopusfile
 * reference implementations for seeking, granule position handling, and error recovery.
 */
class OggDemuxer : public Demuxer {
public:
    explicit OggDemuxer(std::unique_ptr<IOHandler> handler);
    ~OggDemuxer();
    
    // Demuxer interface implementation
    bool parseContainer() override;
    std::vector<StreamInfo> getStreams() const override;
    StreamInfo getStreamInfo(uint32_t stream_id) const override;
    MediaChunk readChunk() override;
    MediaChunk readChunk(uint32_t stream_id) override;
    bool seekTo(uint64_t timestamp_ms) override;
    bool isEOF() const override;
    uint64_t getDuration() const override;
    uint64_t getPosition() const override;
    uint64_t getGranulePosition(uint32_t stream_id) const override;
    
    // Time conversion methods
    uint64_t granuleToMs(uint64_t granule, uint32_t stream_id) const;
    uint64_t msToGranule(uint64_t timestamp_ms, uint32_t stream_id) const;
    
    // Codec detection and header processing
    std::string identifyCodec(const std::vector<uint8_t>& packet_data);
    bool parseVorbisHeaders(OggStream& stream, const OggPacket& packet);
    bool parseFLACHeaders(OggStream& stream, const OggPacket& packet);
    bool parseOpusHeaders(OggStream& stream, const OggPacket& packet);
    bool parseSpeexHeaders(OggStream& stream, const OggPacket& packet);
    std::map<uint32_t, OggStream>& getStreamsForTesting() { return m_streams; }
    
    // Duration calculation methods
    uint64_t getLastGranulePosition();
    uint64_t scanBufferForLastGranule(const std::vector<uint8_t>& buffer, size_t buffer_size);
    uint64_t scanBackwardForLastGranule(int64_t scan_start, size_t scan_size);
    uint64_t scanChunkForLastGranule(const std::vector<uint8_t>& buffer, size_t buffer_size, 
                                     uint32_t preferred_serial, bool has_preferred_serial);
    uint64_t scanForwardForLastGranule(int64_t start_position);
    uint64_t getLastGranuleFromHeaders();
    void setFileSizeForTesting(uint64_t file_size) { m_file_size = file_size; }
    
    // Seeking methods (following ov_pcm_seek_page/op_pcm_seek_page patterns)
    bool seekToPage(uint64_t target_granule, uint32_t stream_id);
    bool examinePacketsAtPosition(int64_t file_offset, uint32_t stream_id, uint64_t& granule_position);
    
    // Data streaming methods
    void fillPacketQueue(uint32_t target_stream_id);
    int fetchAndProcessPacket();
    
    // Safe granule position arithmetic (following libopusfile patterns)
    int granposAdd(int64_t* dst_gp, int64_t src_gp, int32_t delta);
    int granposDiff(int64_t* delta, int64_t gp_a, int64_t gp_b);
    int granposCmp(int64_t gp_a, int64_t gp_b);
    
    // Page extraction methods (following libvorbisfile patterns)
    int getNextPage(ogg_page* page, int64_t boundary = -1);
    int getPrevPage(ogg_page* page);
    int getPrevPageSerial(ogg_page* page, uint32_t serial_number);
    int getData(size_t bytes_requested = 0);
    
    // Memory and resource management
    void cleanupLiboggStructures_unlocked();
    bool validateBufferSize(size_t requested_size, const char* operation_name);
    bool enforcePacketQueueLimits_unlocked(uint32_t stream_id);
    void resetSyncStateAfterSeek_unlocked();
    void resetStreamState_unlocked(uint32_t stream_id, uint32_t new_serial_number);
    bool performMemoryAudit_unlocked();
    void enforceMemoryLimits_unlocked();
    bool validateLiboggStructures_unlocked();
    void performPeriodicMaintenance_unlocked();
    bool detectInfiniteLoop_unlocked(const std::string& operation_name);
    
    // Performance optimization
    bool performReadAheadBuffering_unlocked(size_t target_buffer_size);
    void cachePageForSeeking_unlocked(int64_t file_offset, uint64_t granule_position, 
                                      uint32_t stream_id, const std::vector<uint8_t>& page_data);
    bool findCachedPageNearTarget_unlocked(uint64_t target_granule, uint32_t stream_id, 
                                           int64_t& file_offset, uint64_t& granule_position);
    void addSeekHint_unlocked(uint64_t timestamp_ms, int64_t file_offset, uint64_t granule_position);
    bool findBestSeekHint_unlocked(uint64_t target_timestamp_ms, int64_t& file_offset, 
                                   uint64_t& granule_position);
    bool optimizedRead_unlocked(void* buffer, size_t size, size_t count, long& bytes_read);
    bool processPacketWithMinimalCopy_unlocked(const ogg_packet& packet, uint32_t stream_id, 
                                               OggPacket& output_packet);
    void cleanupPerformanceCaches_unlocked();
    
protected:
    // Error recovery methods (override base class)
    bool skipToNextValidSection() const override;
    bool resetInternalState() const override;
    bool enableFallbackMode() const override;
    
    // Utility methods
    static bool hasSignature(const std::vector<uint8_t>& data, const char* signature);
    static bool validateOggPage(const ogg_page* page);
    static bool validateOggPacket(const ogg_packet* packet, uint32_t stream_id);
    int getOpusPacketSampleCount(const OggPacket& packet);
    int getVorbisPacketSampleCount(const OggPacket& packet);
    
    template<typename T>
    static T readLE(const std::vector<uint8_t>& data, size_t offset) {
        if (offset + sizeof(T) > data.size()) return 0;
        T value;
        std::memcpy(&value, data.data() + offset, sizeof(T));
        return value;
    }
    
    template<typename T>
    static T readBE(const std::vector<uint8_t>& data, size_t offset) {
        if (offset + sizeof(T) > data.size()) return 0;
        T value;
        std::memcpy(&value, data.data() + offset, sizeof(T));
        if constexpr (sizeof(T) == 2) return __builtin_bswap16(value);
        else if constexpr (sizeof(T) == 4) return __builtin_bswap32(value);
        else if constexpr (sizeof(T) == 8) return __builtin_bswap64(value);
        return value;
    }
    
private:
    std::map<uint32_t, OggStream> m_streams;
    uint64_t m_file_size = 0;
    bool m_eof = false;
    uint64_t m_max_granule_seen = 0;
    
    // libogg structures
    ogg_sync_state m_sync_state;
    std::map<uint32_t, ogg_stream_state> m_ogg_streams;
    
    // Memory management
    size_t m_max_packet_queue_size = 100;
    std::atomic<size_t> m_total_memory_usage{0};
    size_t m_max_memory_usage = 50 * 1024 * 1024;
    
    // Performance optimization
    size_t m_read_ahead_buffer_size = 64 * 1024;
    size_t m_page_cache_size = 32;
    size_t m_io_buffer_size = 32 * 1024;
    uint64_t m_seek_hint_granularity = 1000;
    
    // Performance tracking
    std::atomic<uint64_t> m_bytes_read_total{0};
    std::atomic<uint32_t> m_seek_operations{0};
    std::atomic<uint32_t> m_cache_hits{0};
    std::atomic<uint32_t> m_cache_misses{0};
    
    // Page cache
    struct CachedPage {
        int64_t file_offset;
        uint64_t granule_position;
        uint32_t stream_id;
        std::vector<uint8_t> page_data;
        std::chrono::steady_clock::time_point access_time;
    };
    mutable std::vector<CachedPage> m_page_cache;
    mutable std::mutex m_page_cache_mutex;
    
    // Seek hints
    struct SeekHint {
        uint64_t timestamp_ms;
        int64_t file_offset;
        uint64_t granule_position;
    };
    mutable std::vector<SeekHint> m_seek_hints;
    mutable std::mutex m_seek_hints_mutex;
    
    // Thread safety
    mutable std::mutex m_ogg_state_mutex;
    mutable std::mutex m_packet_queue_mutex;
    
    // Error recovery state
    mutable bool m_fallback_mode = false;
    mutable std::set<uint32_t> m_corrupted_streams;
    mutable long m_last_valid_position = 0;
    
    // Page extraction constants (following libvorbisfile patterns)
    static constexpr size_t CHUNKSIZE = 65536;
    static constexpr size_t READSIZE = 2048;
    
    // Page extraction state
    mutable int64_t m_offset = 0;
    mutable int64_t m_end = 0;
};

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

#endif // HAVE_OGGDEMUXER
#endif // OGGDEMUXER_H
