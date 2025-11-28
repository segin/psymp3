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
 * @brief Maximum Ogg page size per RFC 3533
 * Header (27 bytes) + max segments (255) + max segment data (255 * 255 = 65025)
 * Total: 27 + 255 + 65025 = 65307 bytes
 */
static constexpr size_t OGG_PAGE_SIZE_MAX = 65307;

/**
 * @brief Minimum Ogg page header size (fixed portion)
 */
static constexpr size_t OGG_PAGE_HEADER_MIN_SIZE = 27;

/**
 * @brief Maximum number of segments per page
 */
static constexpr size_t OGG_MAX_SEGMENTS = 255;

/**
 * @brief OggS capture pattern as 32-bit value (little-endian: "OggS")
 */
static constexpr uint32_t OGG_CAPTURE_PATTERN = 0x5367674F;  // "OggS" in little-endian

/**
 * @brief OggS capture pattern bytes
 */
static constexpr uint8_t OGG_CAPTURE_BYTES[4] = { 0x4F, 0x67, 0x67, 0x53 };  // "OggS"

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
 * @brief Complete Ogg page structure with segment table and body data
 * 
 * This structure represents a complete Ogg page as defined in RFC 3533 Section 6.
 * It includes the header, segment table (lacing values), and body data.
 */
struct OggPage {
    OggPageHeader header;
    std::vector<uint8_t> segment_table;  // Lacing values (0-255 entries)
    std::vector<uint8_t> body;           // Page body data
    
    // Calculated values
    size_t header_size;                  // 27 + number of segments
    size_t body_size;                    // Sum of lacing values
    size_t total_size;                   // header_size + body_size
    
    /**
     * @brief Validate the OggS capture pattern
     * @return true if capture pattern is valid "OggS" (0x4f676753)
     */
    bool validateCapturePattern() const {
        return header.capture_pattern[0] == 0x4F &&  // 'O'
               header.capture_pattern[1] == 0x67 &&  // 'g'
               header.capture_pattern[2] == 0x67 &&  // 'g'
               header.capture_pattern[3] == 0x53;    // 'S'
    }
    
    /**
     * @brief Validate the stream structure version
     * @return true if version is 0 (only valid version per RFC 3533)
     */
    bool validateVersion() const {
        return header.version == 0;
    }
    
    /**
     * @brief Check if this is a BOS (Beginning of Stream) page
     * @return true if FIRST_PAGE flag (0x02) is set
     */
    bool isBOS() const {
        return header.isFirstPage();
    }
    
    /**
     * @brief Check if this is an EOS (End of Stream) page
     * @return true if LAST_PAGE flag (0x04) is set
     */
    bool isEOS() const {
        return header.isLastPage();
    }
    
    /**
     * @brief Check if this page continues a packet from the previous page
     * @return true if CONTINUED_PACKET flag (0x01) is set
     */
    bool isContinued() const {
        return header.isContinuedPacket();
    }
    
    /**
     * @brief Check if this is a nil EOS page (header only, no content)
     * @return true if EOS flag is set and body is empty
     */
    bool isNilEOS() const {
        return isEOS() && body.empty();
    }
    
    /**
     * @brief Validate page size is within RFC 3533 limits
     * @return true if total_size <= 65307 bytes
     */
    bool validatePageSize() const {
        return total_size <= OGG_PAGE_SIZE_MAX;
    }
    
    /**
     * @brief Get the granule position from the page header
     * @return 64-bit granule position (codec-specific timing value)
     */
    uint64_t getGranulePosition() const {
        return header.granule_position;
    }
    
    /**
     * @brief Get the serial number identifying the logical bitstream
     * @return 32-bit serial number
     */
    uint32_t getSerialNumber() const {
        return header.serial_number;
    }
    
    /**
     * @brief Get the page sequence number for this logical bitstream
     * @return 32-bit page sequence number
     */
    uint32_t getPageSequence() const {
        return header.page_sequence;
    }
    
    /**
     * @brief Calculate header size from segment count
     * @return Header size = 27 + number_of_segments
     */
    size_t calculateHeaderSize() const {
        return OGG_PAGE_HEADER_MIN_SIZE + header.page_segments;
    }
    
    /**
     * @brief Calculate body size from segment table (sum of lacing values)
     * @return Sum of all lacing values in segment table
     */
    size_t calculateBodySize() const {
        size_t size = 0;
        for (uint8_t lacing_value : segment_table) {
            size += lacing_value;
        }
        return size;
    }
    
    /**
     * @brief Reset page to empty state
     */
    void clear() {
        std::memset(&header, 0, sizeof(header));
        segment_table.clear();
        body.clear();
        header_size = 0;
        body_size = 0;
        total_size = 0;
    }
};

/**
 * @brief Ogg page parser for RFC 3533 compliant page extraction
 * 
 * This class provides static methods for parsing Ogg pages from raw byte data.
 * It follows the exact patterns from libvorbisfile and libopusfile reference
 * implementations.
 */
class OggPageParser {
public:
    /**
     * @brief Parse error codes
     */
    enum class ParseResult {
        SUCCESS = 0,           // Page parsed successfully
        NEED_MORE_DATA = 1,    // Not enough data to parse page
        INVALID_CAPTURE = -1,  // Invalid "OggS" capture pattern
        INVALID_VERSION = -2,  // Invalid stream structure version
        INVALID_SIZE = -3,     // Page size exceeds maximum
        CRC_MISMATCH = -4,     // CRC32 checksum validation failed
        CORRUPT_DATA = -5      // General data corruption
    };
    
    /**
     * @brief Validate OggS capture pattern at given position
     * @param data Pointer to data buffer
     * @param size Size of data buffer
     * @param offset Offset to check for capture pattern
     * @return true if "OggS" pattern found at offset
     */
    static bool validateCapturePattern(const uint8_t* data, size_t size, size_t offset = 0);
    
    /**
     * @brief Validate stream structure version
     * @param version Version byte from page header
     * @return true if version is 0 (only valid version)
     */
    static bool validateVersion(uint8_t version);
    
    /**
     * @brief Parse header type flags
     * @param flags Header type byte
     * @param is_continued Output: true if continuation flag set
     * @param is_bos Output: true if BOS flag set
     * @param is_eos Output: true if EOS flag set
     */
    static void parseHeaderFlags(uint8_t flags, bool& is_continued, bool& is_bos, bool& is_eos);
    
    /**
     * @brief Parse a complete Ogg page from raw data
     * @param data Pointer to data buffer
     * @param size Size of data buffer
     * @param page Output: Parsed page structure
     * @param bytes_consumed Output: Number of bytes consumed from buffer
     * @return ParseResult indicating success or error type
     */
    static ParseResult parsePage(const uint8_t* data, size_t size, OggPage& page, size_t& bytes_consumed);
    
    /**
     * @brief Calculate page size from header and segment table
     * @param data Pointer to data buffer (must contain at least header)
     * @param size Size of data buffer
     * @param page_size Output: Total page size if calculable
     * @return true if page size could be calculated
     */
    static bool calculatePageSize(const uint8_t* data, size_t size, size_t& page_size);
    
    /**
     * @brief Calculate CRC32 checksum for page validation
     * @param data Pointer to page data
     * @param size Size of page data
     * @return CRC32 checksum using polynomial 0x04c11db7
     */
    static uint32_t calculateCRC32(const uint8_t* data, size_t size);
    
    /**
     * @brief Validate page CRC32 checksum
     * @param data Pointer to complete page data
     * @param size Size of page data
     * @return true if CRC32 checksum is valid
     */
    static bool validateCRC32(const uint8_t* data, size_t size);
    
    /**
     * @brief Find next OggS capture pattern in buffer
     * @param data Pointer to data buffer
     * @param size Size of data buffer
     * @param start_offset Offset to start searching from
     * @return Offset of next "OggS" pattern, or -1 if not found
     */
    static int64_t findNextCapturePattern(const uint8_t* data, size_t size, size_t start_offset = 0);
    
    /**
     * @brief Parse segment table to extract packet boundaries
     * 
     * RFC 3533 Section 5: Lacing values indicate packet boundaries:
     * - Value of 255: packet continues in next segment
     * - Value < 255: packet ends (final segment of packet)
     * - Value of 0 after 255: packet is exactly multiple of 255 bytes
     * 
     * @param segment_table Segment table (lacing values)
     * @param packet_offsets Output: byte offsets where each packet starts in body
     * @param packet_sizes Output: size of each packet
     * @param packet_complete Output: true if packet is complete (not continued)
     */
    static void parseSegmentTable(const std::vector<uint8_t>& segment_table,
                                  std::vector<size_t>& packet_offsets,
                                  std::vector<size_t>& packet_sizes,
                                  std::vector<bool>& packet_complete);
    
    /**
     * @brief Calculate number of complete packets in segment table
     * @param segment_table Segment table (lacing values)
     * @return Number of complete packets
     */
    static size_t countCompletePackets(const std::vector<uint8_t>& segment_table);
    
    /**
     * @brief Check if last packet in segment table is complete
     * @param segment_table Segment table (lacing values)
     * @return true if last packet is complete (last lacing value < 255)
     */
    static bool isLastPacketComplete(const std::vector<uint8_t>& segment_table);
    
    /**
     * @brief Get lacing value interpretation
     * @param lacing_value Lacing value from segment table
     * @return true if this lacing value indicates packet continuation (value == 255)
     */
    static bool isPacketContinuation(uint8_t lacing_value) {
        return lacing_value == 255;
    }
    
    /**
     * @brief Get lacing value interpretation
     * @param lacing_value Lacing value from segment table
     * @return true if this lacing value indicates packet termination (value < 255)
     */
    static bool isPacketTermination(uint8_t lacing_value) {
        return lacing_value < 255;
    }
    
private:
    // CRC32 lookup table (polynomial 0x04c11db7)
    static const uint32_t s_crc_lookup[256];
    
    // Initialize CRC lookup table
    static void initCRCTable();
    static bool s_crc_table_initialized;
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
    
    // Stream multiplexing handling (RFC 3533 Section 4)
    // Grouped bitstreams: all BOS pages appear before any data pages
    // Chained bitstreams: EOS immediately followed by BOS
    bool processPage(ogg_page* page);
    bool handleBOSPage(ogg_page* page, uint32_t serial_number);
    bool handleEOSPage(ogg_page* page, uint32_t serial_number);
    bool handleDataPage(ogg_page* page, uint32_t serial_number);
    bool isChainedStreamBoundary(ogg_page* page, uint32_t serial_number) const;
    bool isGroupedStream() const { return m_bos_serial_numbers.size() > 1; }
    bool isInHeadersPhase() const { return m_in_headers_phase; }
    uint32_t getChainCount() const { return m_chain_count; }
    void resetMultiplexingState();
    
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
    
    // ========================================================================
    // FLAC-in-Ogg Specific Handling (RFC 9639 Section 10.1)
    // ========================================================================
    
    /**
     * @brief Special granule position value indicating no completed packet on page
     * Per RFC 9639 Section 10.1: 0xFFFFFFFFFFFFFFFF
     */
    static constexpr uint64_t FLAC_OGG_GRANULE_NO_PACKET = 0xFFFFFFFFFFFFFFFFULL;
    
    /**
     * @brief Check if a granule position indicates no completed packet
     * Per RFC 9639 Section 10.1: granule position 0xFFFFFFFFFFFFFFFF means
     * no packet completes on this page
     * @param granule_position Granule position to check
     * @return true if granule indicates no completed packet
     */
    static bool isFlacOggNoPacketGranule(uint64_t granule_position);
    
    /**
     * @brief Check if a granule position is valid for a FLAC-in-Ogg header page
     * Per RFC 9639 Section 10.1: header pages MUST have granule position 0
     * @param granule_position Granule position to check
     * @return true if granule is valid for header page (must be 0)
     */
    static bool isFlacOggValidHeaderGranule(uint64_t granule_position);
    
    /**
     * @brief Convert FLAC-in-Ogg granule position to sample count
     * Per RFC 9639 Section 10.1: granule position is interchannel sample count
     * @param granule_position Granule position from page
     * @param stream FLAC stream for sample rate info
     * @return Sample count, or 0 if invalid granule
     */
    uint64_t flacOggGranuleToSamples(uint64_t granule_position, const OggStream& stream) const;
    
    /**
     * @brief Convert sample count to FLAC-in-Ogg granule position
     * @param samples Sample count (interchannel samples)
     * @return Granule position
     */
    static uint64_t flacOggSamplesToGranule(uint64_t samples);
    
    /**
     * @brief Validate FLAC-in-Ogg page granule position
     * Checks that granule position follows RFC 9639 Section 10.1 rules:
     * - Header pages: granule MUST be 0
     * - Audio pages with completed packet: granule is sample count
     * - Audio pages without completed packet: granule is 0xFFFFFFFFFFFFFFFF
     * @param page Ogg page to validate
     * @param stream FLAC stream info
     * @param is_header_page true if this is a header page
     * @return true if granule position is valid
     */
    bool validateFlacOggGranule(const ogg_page* page, const OggStream& stream, bool is_header_page) const;
    
    /**
     * @brief Process a FLAC-in-Ogg audio packet
     * Per RFC 9639 Section 10.1: each packet contains exactly one FLAC frame
     * @param packet Audio packet to process
     * @param stream FLAC stream info
     * @return true on success, false on error
     */
    bool processFlacOggAudioPacket(const OggPacket& packet, OggStream& stream);
    
    /**
     * @brief Check if FLAC-in-Ogg stream requires chaining due to property change
     * Per RFC 9639 Section 10.1: audio property changes require new stream
     * @param new_stream_info New stream info from potential chain
     * @param current_stream Current stream info
     * @return true if properties differ (chaining required)
     */
    bool flacOggRequiresChaining(const OggStream& new_stream_info, const OggStream& current_stream) const;
    
    /**
     * @brief Handle FLAC-in-Ogg mapping version mismatch
     * Per RFC 9639 Section 10.1: version 1.0 (0x01 0x00) is expected
     * @param major_version Major version byte
     * @param minor_version Minor version byte
     * @return true if version is supported or can be handled gracefully
     */
    bool handleFlacOggVersionMismatch(uint8_t major_version, uint8_t minor_version);
    
    /**
     * @brief Get FLAC frame sample count from audio packet
     * Parses FLAC frame header to extract block size
     * @param packet Audio packet containing FLAC frame
     * @return Sample count for this frame, or 0 on error
     */
    uint32_t getFlacFrameSampleCount(const OggPacket& packet) const;
    
    // ========================================================================
    // Thread Safety: Public/Private Lock Pattern (Requirements 11.1-11.7)
    // ========================================================================
    // Public methods acquire locks and call _unlocked versions
    // Private _unlocked methods assume locks are already held
    // This prevents deadlocks when public methods call other public methods
    
    /**
     * @brief Read next chunk from a specific stream (private implementation without lock)
     * Assumes m_packet_queue_mutex is already held by caller.
     * Requirements: 6.1, 6.2, 6.3, 6.4
     */
    MediaChunk readChunk_unlocked(uint32_t stream_id);
    
    /**
     * @brief Seek to a specific timestamp in milliseconds (private implementation without lock)
     * Assumes m_ogg_state_mutex is already held by caller.
     * Requirements: 7.1, 7.6, 7.7, 7.8
     */
    bool seekTo_unlocked(uint64_t timestamp_ms);
    
    /**
     * @brief Fill packet queue for a stream (private implementation without lock)
     * Assumes m_packet_queue_mutex is already held by caller.
     * Requirements: 6.1, 6.2, 6.3, 6.4
     */
    void fillPacketQueue_unlocked(uint32_t target_stream_id);
    
    /**
     * @brief Seek to a page with target granule position (private implementation without lock)
     * Assumes m_ogg_state_mutex is already held by caller.
     * Requirements: 7.1, 7.2, 7.11
     */
    bool seekToPage_unlocked(uint64_t target_granule, uint32_t stream_id);
    
    /**
     * @brief Set error state for thread-safe propagation to other threads
     * Uses atomic operations to safely propagate errors across threads.
     * Requirements: 11.7
     * @param error_code Error code to propagate
     */
    void setErrorState_unlocked(int error_code) {
        m_error_state.store(true, std::memory_order_release);
        m_error_code.store(error_code, std::memory_order_release);
        Debug::log("ogg", "Error state set: code=%d", error_code);
    }
    
    /**
     * @brief Check if error state has been set by another thread
     * Uses atomic operations for thread-safe error checking.
     * Requirements: 11.7
     * @return true if error state is set
     */
    bool hasErrorState() const {
        return m_error_state.load(std::memory_order_acquire);
    }
    
    /**
     * @brief Get the current error code
     * Uses atomic operations for thread-safe error code retrieval.
     * Requirements: 11.7
     * @return Current error code
     */
    int getErrorCode() const {
        return m_error_code.load(std::memory_order_acquire);
    }
    
    /**
     * @brief Clear error state
     * Uses atomic operations for thread-safe error state clearing.
     * Requirements: 11.7
     */
    void clearErrorState() {
        m_error_state.store(false, std::memory_order_release);
        m_error_code.store(0, std::memory_order_release);
    }
    
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
    
    // Error state propagation (Requirements 11.7)
    // Atomic flag for thread-safe error state propagation
    std::atomic<bool> m_error_state{false};
    std::atomic<int> m_error_code{0};
    
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
    
    // ========================================================================
    // Thread Safety: Lock Acquisition Order (to prevent deadlocks)
    // ========================================================================
    // Lock acquisition order (MUST be followed consistently):
    // 1. m_ogg_state_mutex (acquired first for seeking and state changes)
    // 2. m_packet_queue_mutex (acquired second for packet queue operations)
    // 3. m_page_cache_mutex (acquired third for page cache operations)
    // 4. m_seek_hints_mutex (acquired fourth for seek hint operations)
    //
    // CRITICAL: Always acquire locks in this order to prevent deadlocks.
    // Never acquire a lock that comes later in the order while holding
    // a lock that comes earlier.
    
    // Thread safety
    mutable std::mutex m_ogg_state_mutex;      // Protects: m_streams, m_ogg_streams, m_sync_state, seeking state
    mutable std::mutex m_packet_queue_mutex;   // Protects: packet queues in m_streams
    
    // Error recovery state
    mutable bool m_fallback_mode = false;
    mutable std::set<uint32_t> m_corrupted_streams;
    mutable long m_last_valid_position = 0;
    
    // Stream multiplexing state (RFC 3533 Section 4)
    // Grouped bitstreams: all BOS pages appear before any data pages
    // Chained bitstreams: EOS immediately followed by BOS
    bool m_in_headers_phase = true;           // True until first data page seen
    bool m_seen_data_page = false;            // True after first non-BOS page with data
    std::set<uint32_t> m_bos_serial_numbers;  // Serial numbers from BOS pages in current group
    std::set<uint32_t> m_eos_serial_numbers;  // Serial numbers that have seen EOS
    uint32_t m_chain_count = 0;               // Number of chained streams detected
    
    // Page extraction constants (following libvorbisfile patterns)
    static constexpr size_t CHUNKSIZE = 65536;
    static constexpr size_t READSIZE = 2048;
    
    // Page extraction state
    mutable int64_t m_offset = 0;
    mutable int64_t m_end = 0;
    
public:
    // ========================================================================
    // Page Header Field Extraction (RFC 3533 Section 6)
    // These methods wrap libogg functions for compatibility
    // ========================================================================
    
    /**
     * @brief Extract granule position from an ogg_page
     * Wrapper for ogg_page_granulepos()
     * @param page Pointer to ogg_page structure
     * @return 64-bit granule position, or -1 if invalid
     */
    static int64_t pageGranulePos(const ogg_page* page);
    
    /**
     * @brief Extract serial number from an ogg_page
     * Wrapper for ogg_page_serialno()
     * @param page Pointer to ogg_page structure
     * @return 32-bit serial number
     */
    static uint32_t pageSerialNo(const ogg_page* page);
    
    /**
     * @brief Extract page sequence number from an ogg_page
     * @param page Pointer to ogg_page structure
     * @return 32-bit page sequence number
     */
    static uint32_t pageSequenceNo(const ogg_page* page);
    
    /**
     * @brief Check if page is BOS (Beginning of Stream)
     * Wrapper for ogg_page_bos()
     * @param page Pointer to ogg_page structure
     * @return true if BOS flag is set
     */
    static bool pageBOS(const ogg_page* page);
    
    /**
     * @brief Check if page is EOS (End of Stream)
     * Wrapper for ogg_page_eos()
     * @param page Pointer to ogg_page structure
     * @return true if EOS flag is set
     */
    static bool pageEOS(const ogg_page* page);
    
    /**
     * @brief Check if page continues a packet from previous page
     * Wrapper for ogg_page_continued()
     * @param page Pointer to ogg_page structure
     * @return true if continuation flag is set
     */
    static bool pageContinued(const ogg_page* page);
    
    /**
     * @brief Get number of segments in page
     * @param page Pointer to ogg_page structure
     * @return Number of segments (0-255)
     */
    static uint8_t pageSegments(const ogg_page* page);
    
    /**
     * @brief Calculate page header size
     * @param page Pointer to ogg_page structure
     * @return Header size = 27 + number_of_segments
     */
    static size_t pageHeaderSize(const ogg_page* page);
    
    /**
     * @brief Calculate page body size
     * @param page Pointer to ogg_page structure
     * @return Sum of lacing values
     */
    static size_t pageBodySize(const ogg_page* page);
    
    /**
     * @brief Calculate total page size
     * @param page Pointer to ogg_page structure
     * @return header_size + body_size
     */
    static size_t pageTotalSize(const ogg_page* page);
    
    /**
     * @brief Validate page CRC32 checksum
     * @param page Pointer to ogg_page structure
     * @return true if CRC32 is valid
     */
    static bool pageValidateCRC(const ogg_page* page);
    
    /**
     * @brief Check if page has valid structure
     * @param page Pointer to ogg_page structure
     * @return true if page structure is valid
     */
    static bool pageIsValid(const ogg_page* page);
    
    // ========================================================================
    // Stream State Management (Requirements 6.5, 6.6, 6.7, 6.8, 6.9)
    // ========================================================================
    
    /**
     * @brief Check if a granule position indicates no packets finish on this page
     * Per RFC 3533 Section 6: granule position -1 means no packets finish on this page
     * @param granule_position Granule position to check
     * @return true if granule indicates no completed packets
     * 
     * Requirements: 6.9
     */
    static bool isNoPacketGranule(int64_t granule_position);
    
    /**
     * @brief Check if page loss has occurred for a stream
     * @param stream_id Stream ID to check
     * @param expected_seq Expected sequence number
     * @param actual_seq Actual sequence number from page
     * @return Number of pages lost (0 if no loss)
     * 
     * Requirements: 6.8
     */
    uint32_t detectPageLoss(uint32_t stream_id, uint32_t expected_seq, uint32_t actual_seq) const;
    
    /**
     * @brief Report page loss for error handling
     * @param stream_id Stream ID where loss occurred
     * @param pages_lost Number of pages lost
     * 
     * Requirements: 6.8
     */
    void reportPageLoss(uint32_t stream_id, uint32_t pages_lost);
    
    /**
     * @brief Check if a stream has reached EOS
     * @param stream_id Stream ID to check
     * @return true if stream has received EOS page
     * 
     * Requirements: 6.5
     */
    bool isStreamEOS(uint32_t stream_id) const;
    
    /**
     * @brief Get the number of packets queued for a stream
     * @param stream_id Stream ID to check
     * @return Number of packets in queue
     * 
     * Requirements: 6.6
     */
    size_t getQueuedPacketCount(uint32_t stream_id) const;
    
    /**
     * @brief Get total packets queued across all streams
     * @return Total number of packets in all queues
     */
    size_t getTotalQueuedPackets() const;
    
    // ========================================================================
    // Task 14: Error Handling and Robustness
    // ========================================================================
    
    // ========================================================================
    // Task 14.1: Container-Level Error Handling (Requirements 9.1, 9.2, 9.3, 9.4)
    // ========================================================================
    
    /**
     * @brief Skip corrupted pages using ogg_sync_pageseek() negative returns
     * 
     * Following libvorbisfile patterns:
     * - ogg_sync_pageseek() returns negative value for bytes to skip on corruption
     * - Skip those bytes and continue searching for next valid page
     * - Rely on libogg's internal CRC validation
     * 
     * @param bytes_skipped Output: number of bytes skipped
     * @return true if corrupted data was skipped
     * 
     * Requirements: 9.1, 9.2
     */
    bool skipCorruptedPages_unlocked(size_t& bytes_skipped);
    
    /**
     * @brief Handle missing packets (page loss detected via sequence numbers)
     * 
     * Per RFC 3533 Section 6: page sequence numbers detect page loss
     * Returns OV_HOLE/OP_HOLE equivalent for missing packets
     * 
     * @param stream_id Stream ID where loss occurred
     * @param expected_seq Expected sequence number
     * @param actual_seq Actual sequence number from page
     * 
     * Requirements: 9.3
     */
    void reportPageLoss_unlocked(uint32_t stream_id, uint32_t expected_seq, uint32_t actual_seq);
    
    /**
     * @brief Handle codec identification failures
     * 
     * When BOS packet doesn't match known codec signatures, return OP_ENOTFORMAT
     * and continue scanning for other streams
     * 
     * @param stream_id Stream ID with unknown codec
     * @param packet_data BOS packet data
     * @return true to continue processing other streams
     * 
     * Requirements: 9.4
     */
    bool handleCodecIdentificationFailure_unlocked(uint32_t stream_id, 
                                                   const std::vector<uint8_t>& packet_data);
    
    // ========================================================================
    // Task 14.2: Resource Error Handling (Requirements 9.5, 9.6, 9.7, 9.8)
    // ========================================================================
    
    /**
     * @brief Handle memory allocation failures
     * 
     * Returns OP_EFAULT/OV_EFAULT equivalent for memory allocation failures
     * 
     * @param requested_size Size that failed to allocate
     * @param context Context description
     * @return false (memory failures are not recoverable)
     * 
     * Requirements: 9.5
     */
    bool handleMemoryAllocationFailure_unlocked(size_t requested_size, 
                                                const std::string& context);
    
    /**
     * @brief Handle I/O failures
     * 
     * Returns OP_EREAD/OV_EREAD equivalent for I/O failures
     * 
     * @param operation Description of the I/O operation that failed
     * @return false (I/O failures are not recoverable)
     * 
     * Requirements: 9.6
     */
    bool handleIOFailure_unlocked(const std::string& operation);
    
    /**
     * @brief Clamp seeks to valid ranges
     * 
     * Ensure seek operations don't go beyond file boundaries
     * 
     * @param requested_position Requested seek position
     * @return Clamped position within valid range
     * 
     * Requirements: 9.7
     */
    uint64_t clampSeekPosition_unlocked(uint64_t requested_position);
    
    /**
     * @brief Parse what's possible from malformed metadata
     * 
     * When metadata is malformed, extract what we can and continue
     * Following opus_tags_parse() patterns
     * 
     * @param stream Stream to update with extracted metadata
     * @param metadata_packet Malformed metadata packet
     * @return true to continue processing
     * 
     * Requirements: 9.8
     */
    bool parsePartialMetadata_unlocked(OggStream& stream, 
                                       const std::vector<uint8_t>& metadata_packet);
    
    // ========================================================================
    // Task 14.3: Stream Error Handling (Requirements 9.9, 9.10, 9.11, 9.12)
    // ========================================================================
    
    /**
     * @brief Handle invalid granule position (-1)
     * 
     * Per RFC 3533 Section 6: granule position -1 means no packets finish on page
     * Continue searching for valid granule positions
     * 
     * @param stream_id Stream ID with invalid granule
     * @param granule_position Granule position value
     * @return true if granule is invalid (should continue searching)
     * 
     * Requirements: 9.9
     */
    bool handleInvalidGranulePosition_unlocked(uint32_t stream_id, uint64_t granule_position);
    
    /**
     * @brief Handle unexpected stream end
     * 
     * Returns OP_EBADLINK/OV_EBADLINK equivalent for unexpected stream end
     * 
     * @param stream_id Stream ID that ended unexpectedly
     * @return false (stream end is not recoverable)
     * 
     * Requirements: 9.10
     */
    bool handleUnexpectedStreamEnd_unlocked(uint32_t stream_id);
    
    /**
     * @brief Fall back to linear search on bisection failure
     * 
     * When bisection search fails, fall back to linear forward scanning
     * Following libvorbisfile/libopusfile patterns
     * 
     * @param target_granule Target granule position to find
     * @param stream_id Stream ID to search
     * @return true if target was found using linear search
     * 
     * Requirements: 9.11
     */
    bool fallbackToLinearSearch_unlocked(uint64_t target_granule, uint32_t stream_id);
    
    /**
     * @brief Use OP_PAGE_SIZE_MAX bounds checking
     * 
     * Validate page sizes against RFC 3533 maximum (65307 bytes)
     * 
     * @param page Page to validate
     * @return true if page size is within bounds
     * 
     * Requirements: 9.12
     */
    bool validatePageSizeBounds_unlocked(const ogg_page* page);
};

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

#endif // HAVE_OGGDEMUXER
#endif // OGGDEMUXER_H
