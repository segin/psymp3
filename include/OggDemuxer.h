/*
 * OggDemuxer.h - Ogg container demuxer
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
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

#ifndef OGGDEMUXER_H
#define OGGDEMUXER_H

// No direct includes - all includes should be in psymp3.h

// OggDemuxer is built if any Ogg-based codec is enabled
#ifdef HAVE_OGGDEMUXER

/**
 * @brief Ogg page header structure
 */
struct OggPageHeader {
    uint8_t capture_pattern[4];    // "OggS"
    uint8_t version;               // Stream structure version (0)
    uint8_t header_type;           // Header type flags
    uint64_t granule_position;     // Granule position
    uint32_t serial_number;        // Bitstream serial number
    uint32_t page_sequence;        // Page sequence number
    uint32_t checksum;             // Page checksum
    uint8_t page_segments;         // Number of segments in page
    
    // Flags for header_type
    static constexpr uint8_t CONTINUED_PACKET = 0x01;
    static constexpr uint8_t FIRST_PAGE = 0x02;
    static constexpr uint8_t LAST_PAGE = 0x04;
    
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
    std::string codec_name;        // "vorbis", "flac", "opus", "theora", etc.
    std::string codec_type;        // "audio", "video", "subtitle"
    
    // Stream-specific data
    std::vector<uint8_t> codec_setup_data;  // Codec-specific setup headers
    std::vector<OggPacket> header_packets;  // Codec header packets
    std::deque<OggPacket> m_packet_queue;    // Queued data packets for processing
    
    // Audio properties (filled from codec headers)
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
    uint32_t bitrate = 0;
    uint64_t total_samples = 0;
    uint64_t pre_skip = 0;
    
    // Metadata (filled from comment headers like OpusTags)
    std::string artist;
    std::string title;
    std::string album;
    
    // State tracking
    bool headers_complete = false;
    bool headers_sent = false;  // Track whether header packets have been sent to codec
    size_t next_header_index = 0;  // Index of next header packet to send
    uint64_t total_samples_processed = 0; // Running total of samples for timestamp generation
    std::vector<uint8_t> partial_packet_data;  // For packets split across pages
    uint64_t last_granule = 0;
    uint32_t last_page_sequence = 0;
};

/**
 * @brief Ogg container demuxer using libogg
 * 
 * The Ogg demuxer handles the Ogg container format, which can contain:
 * - Vorbis audio (.ogg)
 * - FLAC audio (.oga, .ogg)
 * - Opus audio (.opus, .oga)
 * - Theora video (.ogv)
 * - Speex audio (.spx)
 * - And other codecs
 * 
 * This demuxer focuses on audio streams but can be extended for video.
 * Uses libogg for proper Ogg packet parsing.
 */
class OggDemuxer : public Demuxer {
public:
    explicit OggDemuxer(std::unique_ptr<IOHandler> handler);
    ~OggDemuxer();
    
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
    
    // Public methods for testing granule position conversion
    uint64_t granuleToMs(uint64_t granule, uint32_t stream_id) const;
    uint64_t msToGranule(uint64_t timestamp_ms, uint32_t stream_id) const;
    
    // Public methods for testing duration calculation
    uint64_t getLastGranulePosition();
    uint64_t scanBufferForLastGranule(const std::vector<uint8_t>& buffer, size_t buffer_size);
    uint64_t scanForwardForLastGranule(long start_position);
    uint64_t getLastGranuleFromHeaders();
    void setFileSizeForTesting(uint64_t file_size) { m_file_size = file_size; }
    
    // Public methods for testing bisection search
    bool seekToPage(uint64_t target_granule, uint32_t stream_id);
    bool examinePacketsAtPosition(int64_t file_offset, uint32_t stream_id, uint64_t& granule_position);
    
    // Safe granule position arithmetic (following libopusfile patterns)
    // Made public for testing
    
    /**
     * @brief Safe granule position addition with overflow detection (follows op_granpos_add)
     * @param dst_gp Pointer to destination granule position
     * @param src_gp Source granule position
     * @param delta Delta to add
     * @return 0 on success, negative on overflow
     */
    int granposAdd(int64_t* dst_gp, int64_t src_gp, int32_t delta);
    
    /**
     * @brief Safe granule position subtraction with wraparound handling (follows op_granpos_diff)
     * @param delta Pointer to store the difference
     * @param gp_a First granule position
     * @param gp_b Second granule position (subtracted from gp_a)
     * @return 0 on success, negative on underflow
     */
    int granposDiff(int64_t* delta, int64_t gp_a, int64_t gp_b);
    
    /**
     * @brief Safe granule position comparison with proper ordering (follows op_granpos_cmp)
     * @param gp_a First granule position
     * @param gp_b Second granule position
     * @return -1 if gp_a < gp_b, 0 if equal, 1 if gp_a > gp_b
     */
    int granposCmp(int64_t gp_a, int64_t gp_b);
    
    // Reference-pattern page extraction functions (following libvorbisfile)
    // Made public for testing
    
    /**
     * @brief Get next page using ogg_sync_pageseek() patterns from libvorbisfile
     * Equivalent to _get_next_page() in libvorbisfile
     */
    int getNextPage(ogg_page* page, int64_t boundary = -1);
    
    /**
     * @brief Get previous page using backward scanning with CHUNKSIZE increments
     * Equivalent to _get_prev_page() in libvorbisfile
     */
    int getPrevPage(ogg_page* page);
    
    /**
     * @brief Get previous page with serial number awareness
     * Equivalent to _get_prev_page_serial() in libvorbisfile
     */
    int getPrevPageSerial(ogg_page* page, uint32_t serial_number);
    
    /**
     * @brief Fetch data into sync buffer like _get_data() in libvorbisfile
     */
    int getData(size_t bytes_requested = 0);
    
protected:
    // Protected access to streams for testing
    std::map<uint32_t, OggStream>& getStreamsForTesting() { return m_streams; }
    
private:
    std::map<uint32_t, OggStream> m_streams;
    uint64_t m_file_size = 0;
    bool m_eof = false;
    uint64_t m_max_granule_seen = 0;
    
    // libogg structures
    ogg_sync_state m_sync_state;
    std::map<uint32_t, ogg_stream_state> m_ogg_streams;
    
    // Thread safety for OggDemuxer-specific state
    mutable std::mutex m_ogg_state_mutex;    // Protects Ogg-specific state and libogg structures
    mutable std::mutex m_packet_queue_mutex; // Protects packet queues in OggStream objects
    
    /**
     * @brief Read data into libogg sync buffer
     */
    bool readIntoSyncBuffer(size_t bytes);
    
    /**
     * @brief Process pages using libogg
     */
    bool processPages();
    
    /**
     * @brief Identify codec from packet data
     */
    std::string identifyCodec(const std::vector<uint8_t>& packet_data);
    
    
    /**
     * @brief Parse Vorbis identification header
     */
    bool parseVorbisHeaders(OggStream& stream, const OggPacket& packet);
    
    /**
     * @brief Parse Vorbis comments from Vorbis comment header
     */
    void parseVorbisComments(OggStream& stream, const OggPacket& packet);
    
    /**
     * @brief Parse FLAC identification header
     */
    bool parseFLACHeaders(OggStream& stream, const OggPacket& packet);
    
    /**
     * @brief Parse Opus identification header
     */
    bool parseOpusHeaders(OggStream& stream, const OggPacket& packet);
    
    /**
     * @brief Parse OpusTags metadata from Opus comment header
     */
    void parseOpusTags(OggStream& stream, const OggPacket& packet);
    
    /**
     * @brief Parse Speex identification header
     */
    bool parseSpeexHeaders(OggStream& stream, const OggPacket& packet);
    
    /**
     * @brief Calculate duration from stream information
     */
    void calculateDuration();
    

    
    /**
     * @brief Find the best audio stream for playback
     */
    uint32_t findBestAudioStream() const;
    

    
    /**
     * @brief Find the granule position at a specific file offset
     */
    uint64_t findGranuleAtOffset(long file_offset, uint32_t stream_id);
    
    /**
     * @brief Find granule position using ogg_sync_pageseek() (following libvorbisfile patterns)
     */
    uint64_t findGranuleAtOffsetUsingPageseek(int64_t file_offset, uint32_t stream_id);
    
    /**
     * @brief Linear scan for target granule when bisection interval becomes small
     */
    int64_t linearScanForTarget(int64_t begin, int64_t end, uint64_t target_granule, uint32_t stream_id);
    

    
    /**
     * @brief Robustly find and read the next Ogg page from the current file position.
     */
    bool findAndReadNextPage(ogg_page& page, uint32_t target_stream_id = 0);
    
    /**
     * @brief Read and queue packets until we have data for the requested stream
     */
    void fillPacketQueue(uint32_t target_stream_id);
    

    
    /**
     * @brief Check if packet data starts with given signature
     */
    static bool hasSignature(const std::vector<uint8_t>& data, const char* signature);
    
    /**
     * @brief Validate ogg_page structure before accessing its fields
     */
    static bool validateOggPage(const ogg_page* page);
    
    /**
     * @brief Validate ogg_packet structure before accessing its fields
     */
    static bool validateOggPacket(const ogg_packet* packet, uint32_t stream_id);
    
    /**
     * @brief Get the number of samples in an Opus packet
     */
    int getOpusPacketSampleCount(const OggPacket& packet);

    /**
     * @brief Get the number of samples in a Vorbis packet
     */
    int getVorbisPacketSampleCount(const OggPacket& packet);

    /**
     * @brief Read little-endian values from packet data
     */
    template<typename T>
    static T readLE(const std::vector<uint8_t>& data, size_t offset) {
        if (offset + sizeof(T) > data.size()) {
            return 0;
        }
        T value;
        std::memcpy(&value, data.data() + offset, sizeof(T));
        return value;
    }
    
    /**
     * @brief Read big-endian values from packet data
     */
    template<typename T>
    static T readBE(const std::vector<uint8_t>& data, size_t offset) {
        if (offset + sizeof(T) > data.size()) {
            return 0;
        }
        T value;
        std::memcpy(&value, data.data() + offset, sizeof(T));
        if constexpr (sizeof(T) == 2) {
            return __builtin_bswap16(value);
        } else if constexpr (sizeof(T) == 4) {
            return __builtin_bswap32(value);
        } else if constexpr (sizeof(T) == 8) {
            return __builtin_bswap64(value);
        }
        return value;
    }
    
    // Error recovery methods (override base class methods)
    
    /**
     * @brief Skip to next valid Ogg page
     */
    bool skipToNextValidSection() const override;
    
    /**
     * @brief Reset Ogg parsing state
     */
    bool resetInternalState() const override;
    
    /**
     * @brief Enable fallback parsing mode for corrupted Ogg files
     */
    bool enableFallbackMode() const override;
    
    /**
     * @brief Recover from corrupted Ogg page
     */
    bool recoverFromCorruptedPage(long file_offset);
    
    /**
     * @brief Handle seeking errors with range clamping
     */
    bool handleSeekingError(uint64_t timestamp_ms, uint64_t& clamped_timestamp);
    
    /**
     * @brief Isolate stream errors to prevent affecting other streams
     */
    bool isolateStreamError(uint32_t stream_id, const std::string& error_context);
    
    /**
     * @brief Synchronize to next Ogg page boundary
     */
    bool synchronizeToPageBoundary();
    
    /**
     * @brief Validate and repair stream state
     */
    bool validateAndRepairStreamState(uint32_t stream_id);
    
private:
    // Error recovery state
    mutable bool m_fallback_mode = false;
    mutable std::set<uint32_t> m_corrupted_streams;
    mutable long m_last_valid_position = 0;
    
    // Page extraction constants (following libvorbisfile patterns)
    static constexpr size_t CHUNKSIZE = 65536;  // 64KB chunks for backward scanning
    static constexpr size_t READSIZE = 2048;    // Default read size for _get_data()
    
    // Page extraction state
    mutable int64_t m_offset = 0;               // Current file offset for page extraction
    mutable int64_t m_end = 0;                  // End boundary for page extraction
};

#endif // HAVE_OGGDEMUXER
#endif // OGGDEMUXER_H