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
    
    // Public methods for testing codec detection and header processing
    std::string identifyCodec(const std::vector<uint8_t>& packet_data);
    bool parseVorbisHeaders(OggStream& stream, const OggPacket& packet);
    bool parseFLACHeaders(OggStream& stream, const OggPacket& packet);
    bool parseOpusHeaders(OggStream& stream, const OggPacket& packet);
    bool parseSpeexHeaders(OggStream& stream, const OggPacket& packet);
    std::map<uint32_t, OggStream>& getStreamsForTesting() { return m_streams; }
    
    // Public methods for testing duration calculation
    uint64_t getLastGranulePosition();
    uint64_t scanBufferForLastGranule(const std::vector<uint8_t>& buffer, size_t buffer_size);
    uint64_t scanBackwardForLastGranule(int64_t scan_start, size_t scan_size);
    uint64_t scanChunkForLastGranule(const std::vector<uint8_t>& buffer, size_t buffer_size, 
                                     uint32_t preferred_serial, bool has_preferred_serial);
    uint64_t scanForwardForLastGranule(int64_t start_position);
    uint64_t getLastGranuleFromHeaders();
    void setFileSizeForTesting(uint64_t file_size) { m_file_size = file_size; }
    
    /**
     * @brief Bisection search algorithm for timestamp-based seeking (follows ov_pcm_seek_page/op_pcm_seek_page)
     * 
     * This function implements the exact bisection search algorithm used in libvorbisfile's
     * ov_pcm_seek_page() and libopusfile's op_pcm_seek_page() functions. It performs
     * efficient binary search through the file to find the page containing the target
     * granule position.
     * 
     * The bisection algorithm:
     * 1. Initializes search interval [begin, end] spanning the entire file
     * 2. Repeatedly bisects the interval using (begin + end) / 2
     * 3. Uses getNextPage() with ogg_sync_pageseek() for page discovery
     * 4. Compares granule positions using granposCmp() for proper ordering
     * 5. Adjusts interval boundaries based on comparison results
     * 6. Switches to linear scanning when interval becomes small (< 8KB)
     * 7. Uses ogg_stream_packetpeek() to examine packets without consuming them
     * 8. Handles invalid granule positions (-1) by continuing search
     * 
     * This algorithm provides O(log n) seeking performance and is essential for
     * responsive user interaction in large audio files.
     * 
     * @param target_granule Target granule position to seek to
     * @param stream_id Stream ID for granule position interpretation
     * @return true if target position found and stream positioned correctly
     */
    bool seekToPage(uint64_t target_granule, uint32_t stream_id);
    bool examinePacketsAtPosition(int64_t file_offset, uint32_t stream_id, uint64_t& granule_position);
    
    // Public methods for testing data streaming
    void fillPacketQueue(uint32_t target_stream_id);
    int fetchAndProcessPacket();
    
    // Safe granule position arithmetic (following libopusfile patterns)
    // Made public for testing
    
    /**
     * @brief Safe granule position addition with overflow detection (follows op_granpos_add)
     * 
     * This function implements safe granule position arithmetic following the exact patterns
     * used in libopusfile's op_granpos_add() function. Granule positions in Ogg streams
     * use a special encoding where -1 indicates an invalid/unset position, and the valid
     * range wraps around from positive to negative values.
     * 
     * The granule position space is organized as:
     * [ -1 (invalid) ][ 0 ... INT64_MAX ][ INT64_MIN ... -2 ][ -1 (invalid) ]
     * 
     * This function detects overflow conditions that would wrap past the invalid -1 value
     * and returns appropriate error codes to prevent corruption of timing information.
     * 
     * @param dst_gp Pointer to destination granule position (must not be null)
     * @param src_gp Source granule position to add to
     * @param delta Delta value to add (can be positive or negative)
     * @return 0 on success, negative error code on overflow/underflow
     */
    int granposAdd(int64_t* dst_gp, int64_t src_gp, int32_t delta);
    
    /**
     * @brief Safe granule position subtraction with wraparound handling (follows op_granpos_diff)
     * 
     * This function implements safe granule position subtraction following the exact patterns
     * used in libopusfile's op_granpos_diff() function. It correctly handles the wraparound
     * behavior of granule positions where negative values (except -1) are considered larger
     * than positive values.
     * 
     * The function computes gp_a - gp_b while handling:
     * - Invalid granule positions (-1) which result in invalid differences
     * - Wraparound from positive to negative granule position ranges
     * - Underflow conditions that would produce invalid results
     * 
     * This is essential for accurate seeking and duration calculations in Ogg streams.
     * 
     * @param delta Pointer to store the computed difference (must not be null)
     * @param gp_a First granule position (minuend)
     * @param gp_b Second granule position (subtrahend) - subtracted from gp_a
     * @return 0 on success, negative error code on underflow or invalid input
     */
    int granposDiff(int64_t* delta, int64_t gp_a, int64_t gp_b);
    
    /**
     * @brief Safe granule position comparison with proper ordering (follows op_granpos_cmp)
     * 
     * This function implements granule position comparison following the exact patterns
     * used in libopusfile's op_granpos_cmp() function. It correctly handles the special
     * ordering semantics of granule positions where:
     * 
     * 1. Invalid positions (-1) are considered equal to each other
     * 2. Invalid positions are considered less than all valid positions
     * 3. Negative positions (except -1) are considered greater than positive positions
     * 4. Within the same sign range, normal integer comparison applies
     * 
     * This ordering is crucial for bisection search algorithms and duration calculations
     * to work correctly with the wraparound granule position encoding.
     * 
     * @param gp_a First granule position to compare
     * @param gp_b Second granule position to compare
     * @return -1 if gp_a < gp_b, 0 if gp_a == gp_b, 1 if gp_a > gp_b
     */
    int granposCmp(int64_t gp_a, int64_t gp_b);
    
    // Reference-pattern page extraction functions (following libvorbisfile)
    // Made public for testing
    
    /**
     * @brief Get next page using ogg_sync_pageseek() patterns from libvorbisfile
     * 
     * This function implements the exact page extraction algorithm used in libvorbisfile's
     * _get_next_page() function. It uses ogg_sync_pageseek() for robust page discovery
     * that can handle corrupted data by skipping invalid bytes.
     * 
     * The function:
     * 1. Uses ogg_sync_pageseek() to find the next valid page boundary
     * 2. Handles negative return values (corrupted bytes) by skipping them
     * 3. Fetches additional data when needed using _get_data() patterns
     * 4. Respects boundary limits to prevent reading beyond specified positions
     * 5. Maintains proper file position tracking for seeking operations
     * 
     * This is the foundation for all page-based operations including seeking,
     * duration calculation, and sequential packet reading.
     * 
     * Equivalent to _get_next_page() in libvorbisfile.
     * 
     * @param page Pointer to ogg_page structure to fill (must not be null)
     * @param boundary Optional boundary position (-1 for no limit)
     * @return 1 if page found, 0 if need more data, negative on error
     */
    int getNextPage(ogg_page* page, int64_t boundary = -1);
    
    /**
     * @brief Get previous page using backward scanning with CHUNKSIZE increments
     * 
     * This function implements the exact backward page scanning algorithm used in
     * libvorbisfile's _get_prev_page() function. It performs chunk-based backward
     * scanning to efficiently find the previous valid page without reading the
     * entire file.
     * 
     * The algorithm:
     * 1. Moves backward by CHUNKSIZE (65536) byte increments
     * 2. Scans forward within each chunk to find valid pages
     * 3. Handles file beginning boundary conditions gracefully
     * 4. Uses exponentially increasing chunk sizes for efficiency
     * 5. Maintains proper synchronization with libogg page parsing
     * 
     * This is essential for seeking operations and duration calculation that
     * need to scan backward from the end of the file to find the last valid
     * granule position.
     * 
     * Equivalent to _get_prev_page() in libvorbisfile.
     * 
     * @param page Pointer to ogg_page structure to fill (must not be null)
     * @return 1 if page found, 0 if at beginning of file, negative on error
     */
    int getPrevPage(ogg_page* page);
    
    /**
     * @brief Get previous page with serial number awareness
     * 
     * This function implements the exact serial-number-aware backward scanning
     * algorithm used in libvorbisfile's _get_prev_page_serial() function. It
     * extends the basic backward scanning to prefer pages from a specific
     * logical bitstream.
     * 
     * The algorithm:
     * 1. Performs backward chunk-based scanning like getPrevPage()
     * 2. Prefers pages with the specified serial number
     * 3. Falls back to any valid page if preferred serial not found
     * 4. Handles multiplexed streams correctly
     * 5. Maintains proper granule position tracking per stream
     * 
     * This is crucial for duration calculation in multiplexed Ogg files where
     * different logical streams may have different lengths and we need to find
     * the last page of the longest stream.
     * 
     * Equivalent to _get_prev_page_serial() in libvorbisfile.
     * 
     * @param page Pointer to ogg_page structure to fill (must not be null)
     * @param serial_number Preferred serial number to search for
     * @return 1 if page found, 0 if at beginning of file, negative on error
     */
    int getPrevPageSerial(ogg_page* page, uint32_t serial_number);
    
    /**
     * @brief Fetch data into sync buffer like _get_data() in libvorbisfile
     * 
     * This function implements the exact data fetching algorithm used in
     * libvorbisfile's _get_data() function. It manages the libogg sync buffer
     * and handles I/O operations with proper error handling.
     * 
     * The function:
     * 1. Requests buffer space from libogg using ogg_sync_buffer()
     * 2. Performs I/O operations through the IOHandler interface
     * 3. Submits read data to libogg using ogg_sync_wrote()
     * 4. Handles EOF and error conditions gracefully
     * 5. Maintains proper buffer size limits to prevent memory exhaustion
     * 6. Provides thread-safe access to I/O operations
     * 
     * This is the foundation for all data input operations and ensures
     * consistent behavior with the reference implementation.
     * 
     * Equivalent to _get_data() in libvorbisfile.
     * 
     * @param bytes_requested Number of bytes to request (0 for default)
     * @return Number of bytes read, 0 on EOF, negative on error
     */
    int getData(size_t bytes_requested = 0);
    
    /**
     * @brief Clean up all libogg structures (assumes locks are held)
     */
    void cleanupLiboggStructures_unlocked();
    
    /**
     * @brief Validate buffer sizes to prevent overflow (Requirement 8.4)
     */
    bool validateBufferSize(size_t requested_size, const char* operation_name);
    
    /**
     * @brief Check and enforce packet queue limits (Requirement 8.2)
     */
    bool enforcePacketQueueLimits_unlocked(uint32_t stream_id);
    
    /**
     * @brief Reset sync state after seeks (Requirement 12.9)
     */
    void resetSyncStateAfterSeek_unlocked();
    
    /**
     * @brief Reset stream state for stream switching (Requirement 12.10)
     */
    void resetStreamState_unlocked(uint32_t stream_id, uint32_t new_serial_number);
    
    /**
     * @brief Perform comprehensive memory audit (Requirement 8.2, 8.3)
     */
    bool performMemoryAudit_unlocked();
    
    /**
     * @brief Enforce strict memory limits to prevent exhaustion (Requirement 8.2)
     */
    void enforceMemoryLimits_unlocked();
    
    /**
     * @brief Validate libogg structures for corruption (Requirement 8.1, 8.3)
     */
    bool validateLiboggStructures_unlocked();
    
    /**
     * @brief Perform periodic maintenance to prevent resource leaks (Requirement 8.2, 8.3)
     */
    void performPeriodicMaintenance_unlocked();
    
    /**
     * @brief Detect and prevent infinite loops in packet processing (Requirement 8.1)
     */
    bool detectInfiniteLoop_unlocked(const std::string& operation_name);
    
    /**
     * @brief Implement efficient read-ahead buffering for network sources (Requirement 8.4)
     */
    bool performReadAheadBuffering_unlocked(size_t target_buffer_size);
    
    /**
     * @brief Cache recently accessed pages for seeking performance (Requirement 8.6)
     */
    void cachePageForSeeking_unlocked(int64_t file_offset, uint64_t granule_position, 
                                      uint32_t stream_id, const std::vector<uint8_t>& page_data);
    
    /**
     * @brief Find cached page near target position (Requirement 8.6)
     */
    bool findCachedPageNearTarget_unlocked(uint64_t target_granule, uint32_t stream_id, 
                                           int64_t& file_offset, uint64_t& granule_position);
    
    /**
     * @brief Add seek hint to reduce bisection iterations (Requirement 8.3)
     */
    void addSeekHint_unlocked(uint64_t timestamp_ms, int64_t file_offset, uint64_t granule_position);
    
    /**
     * @brief Find best seek hint for target timestamp (Requirement 8.3)
     */
    bool findBestSeekHint_unlocked(uint64_t target_timestamp_ms, int64_t& file_offset, 
                                   uint64_t& granule_position);
    
    /**
     * @brief Optimize I/O operations with efficient buffering (Requirements 8.1, 8.3)
     */
    bool optimizedRead_unlocked(void* buffer, size_t size, size_t count, long& bytes_read);
    
    /**
     * @brief Minimize memory copying in packet processing (Requirement 8.5)
     */
    bool processPacketWithMinimalCopy_unlocked(const ogg_packet& packet, uint32_t stream_id, 
                                               OggPacket& output_packet);
    
    /**
     * @brief Clean up performance caches and hints
     */
    void cleanupPerformanceCaches_unlocked();
    
protected:
    
private:
    std::map<uint32_t, OggStream> m_streams;
    uint64_t m_file_size = 0;
    bool m_eof = false;
    uint64_t m_max_granule_seen = 0;
    
    // libogg structures
    ogg_sync_state m_sync_state;
    std::map<uint32_t, ogg_stream_state> m_ogg_streams;
    
    // Memory management (Requirement 8.2)
    size_t m_max_packet_queue_size;
    std::atomic<size_t> m_total_memory_usage;
    size_t m_max_memory_usage;
    
    // Performance optimization settings (Requirements 8.1-8.7)
    size_t m_read_ahead_buffer_size;
    size_t m_page_cache_size;
    size_t m_io_buffer_size;
    uint64_t m_seek_hint_granularity;
    
    // Performance tracking
    std::atomic<uint64_t> m_bytes_read_total;
    std::atomic<uint32_t> m_seek_operations;
    std::atomic<uint32_t> m_cache_hits;
    std::atomic<uint32_t> m_cache_misses;
    
    // Page cache for seeking performance (Requirement 8.6)
    struct CachedPage {
        int64_t file_offset;
        uint64_t granule_position;
        uint32_t stream_id;
        std::vector<uint8_t> page_data;
        std::chrono::steady_clock::time_point access_time;
    };
    mutable std::vector<CachedPage> m_page_cache;
    mutable std::mutex m_page_cache_mutex;
    
    // Seek hints for bisection optimization (Requirement 8.3)
    struct SeekHint {
        uint64_t timestamp_ms;
        int64_t file_offset;
        uint64_t granule_position;
    };
    mutable std::vector<SeekHint> m_seek_hints;
    mutable std::mutex m_seek_hints_mutex;
    
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
     * @brief Parse Vorbis comments from Vorbis comment header
     */
    void parseVorbisComments(OggStream& stream, const OggPacket& packet);
    
    /**
     * @brief Parse OpusTags metadata from Opus comment header
     */
    void parseOpusTags(OggStream& stream, const OggPacket& packet);
    
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
    
protected:
    
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