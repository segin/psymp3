/*
 * FLACDemuxer.h - FLAC container demuxer (RFC 9639 compliant)
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FLACDEMUXER_H
#define FLACDEMUXER_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace Demuxer {
namespace FLAC {

/**
 * @brief FLAC metadata block types per RFC 9639 Section 8
 */
enum class FLACMetadataType : uint8_t {
    STREAMINFO = 0,      ///< Stream information (mandatory, always first)
    PADDING = 1,         ///< Padding block
    APPLICATION = 2,     ///< Application-specific data
    SEEKTABLE = 3,       ///< Seek table for efficient seeking
    VORBIS_COMMENT = 4,  ///< Vorbis-style comments (metadata)
    CUESHEET = 5,        ///< Cue sheet for CD-like track information
    PICTURE = 6,         ///< Embedded picture/artwork
    INVALID = 127        ///< Invalid/forbidden block type per RFC 9639 Table 1
};

/**
 * @brief FLAC channel assignment modes per RFC 9639 Section 9.1.3
 * 
 * Defines the interchannel decorrelation mode used in FLAC frames.
 * Independent mode stores each channel separately, while stereo modes
 * use decorrelation to improve compression efficiency.
 */
enum class FLACChannelMode : uint8_t {
    INDEPENDENT = 0,    ///< Independent channels (1-8 channels, no decorrelation)
    LEFT_SIDE = 1,      ///< Left-side stereo (left channel + side channel)
    RIGHT_SIDE = 2,     ///< Right-side stereo (side channel + right channel)
    MID_SIDE = 3        ///< Mid-side stereo (mid channel + side channel)
};

/**
 * @brief FLAC metadata block header information per RFC 9639 Section 8.1
 */
struct FLACMetadataBlock {
    FLACMetadataType type = FLACMetadataType::INVALID;  ///< Block type (bits 0-6 of header byte 0)
    bool is_last = false;                               ///< True if last metadata block (bit 7 of header byte 0)
    uint32_t length = 0;                                ///< Block data length in bytes (24-bit big-endian)
    uint64_t data_offset = 0;                           ///< File offset where block data starts
    
    FLACMetadataBlock() = default;
    
    FLACMetadataBlock(FLACMetadataType t, bool last, uint32_t len, uint64_t offset)
        : type(t), is_last(last), length(len), data_offset(offset) {}
    
    bool isValid() const {
        return type != FLACMetadataType::INVALID;
    }
};

/**
 * @brief FLAC STREAMINFO block data per RFC 9639 Section 8.2
 */
struct FLACStreamInfo {
    uint16_t min_block_size = 0;     ///< Minimum block size in samples (16-65535)
    uint16_t max_block_size = 0;     ///< Maximum block size in samples (16-65535)
    uint32_t min_frame_size = 0;     ///< Minimum frame size in bytes (0 if unknown)
    uint32_t max_frame_size = 0;     ///< Maximum frame size in bytes (0 if unknown)
    uint32_t sample_rate = 0;        ///< Sample rate in Hz (1-655350)
    uint8_t channels = 0;            ///< Number of channels (1-8)
    uint8_t bits_per_sample = 0;     ///< Bits per sample (4-32)
    uint64_t total_samples = 0;      ///< Total samples in stream (0 if unknown)
    uint8_t md5_signature[16];       ///< MD5 signature of uncompressed audio data
    
    FLACStreamInfo() {
        std::memset(md5_signature, 0, sizeof(md5_signature));
    }
    
    bool isValid() const {
        // Per RFC 9639: min/max block size must be >= 16
        return sample_rate > 0 && 
               channels > 0 && channels <= 8 && 
               bits_per_sample >= 4 && bits_per_sample <= 32 &&
               min_block_size >= 16 && max_block_size >= 16 &&
               min_block_size <= max_block_size;
    }
    
    uint64_t getDurationMs() const {
        if (sample_rate == 0 || total_samples == 0) return 0;
        return (total_samples * 1000) / sample_rate;
    }
};

/**
 * @brief FLAC seek point entry per RFC 9639 Section 8.5
 */
struct FLACSeekPoint {
    uint64_t sample_number = 0;      ///< Sample number of first sample in target frame
    uint64_t stream_offset = 0;      ///< Offset from first frame header to target frame
    uint16_t frame_samples = 0;      ///< Number of samples in target frame
    
    FLACSeekPoint() = default;
    
    FLACSeekPoint(uint64_t sample, uint64_t offset, uint16_t samples)
        : sample_number(sample), stream_offset(offset), frame_samples(samples) {}
    
    bool isPlaceholder() const {
        return sample_number == 0xFFFFFFFFFFFFFFFFULL;
    }
    
    bool isValid() const {
        return !isPlaceholder() && frame_samples > 0;
    }
};

/**
 * @brief FLAC cuesheet track index point per RFC 9639 Section 8.7.1.1
 */
struct FLACCuesheetIndexPoint {
    uint64_t offset = 0;         ///< Offset in samples relative to track offset
    uint8_t number = 0;          ///< Track index point number
    
    FLACCuesheetIndexPoint() = default;
    
    FLACCuesheetIndexPoint(uint64_t off, uint8_t num)
        : offset(off), number(num) {}
};

/**
 * @brief FLAC cuesheet track per RFC 9639 Section 8.7.1
 */
struct FLACCuesheetTrack {
    uint64_t offset = 0;         ///< Track offset in samples from beginning of audio stream
    uint8_t number = 0;          ///< Track number (1-99 for CD-DA, 170/255 for lead-out)
    char isrc[13] = {0};         ///< Track ISRC (12 characters + null terminator)
    bool is_audio = true;        ///< True if audio track, false if non-audio
    bool pre_emphasis = false;   ///< True if pre-emphasis is applied
    std::vector<FLACCuesheetIndexPoint> index_points;  ///< Track index points
    
    FLACCuesheetTrack() = default;
    
    bool isLeadOut() const {
        return number == 170 || number == 255;
    }
};

/**
 * @brief FLAC cuesheet block data per RFC 9639 Section 8.7
 */
struct FLACCuesheet {
    char media_catalog_number[129] = {0};  ///< Media catalog number (128 bytes + null terminator)
    uint64_t lead_in_samples = 0;          ///< Number of lead-in samples
    bool is_cd_da = false;                 ///< True if corresponds to CD-DA
    std::vector<FLACCuesheetTrack> tracks; ///< Cuesheet tracks
    
    FLACCuesheet() = default;
    
    bool isValid() const {
        // Requirement 16.6: Number of tracks must be at least 1
        return !tracks.empty();
    }
    
    size_t getTrackCount() const {
        return tracks.size();
    }
};

/**
 * @brief FLAC picture block data per RFC 9639 Section 8.8
 */
struct FLACPicture {
    uint32_t type = 0;                   ///< Picture type (0-20 defined, see RFC 9639 Table 13)
    std::string media_type;              ///< MIME type (e.g., "image/jpeg") or "-->" for URI
    std::string description;             ///< UTF-8 description of the picture
    uint32_t width = 0;                  ///< Width in pixels (0 if unknown)
    uint32_t height = 0;                 ///< Height in pixels (0 if unknown)
    uint32_t color_depth = 0;            ///< Color depth in bits per pixel (0 if unknown)
    uint32_t indexed_colors = 0;         ///< Number of colors for indexed images (0 for non-indexed)
    std::vector<uint8_t> data;           ///< Picture data or URI
    bool is_uri = false;                 ///< True if data contains a URI instead of binary data
    
    FLACPicture() = default;
    
    bool isValid() const {
        // Must have either data or be a URI reference
        return !data.empty() || is_uri;
    }
    
    /**
     * @brief Get picture type name per RFC 9639 Table 13
     */
    std::string getTypeName() const {
        static const char* type_names[] = {
            "Other",                           // 0
            "32x32 pixels file icon",          // 1
            "Other file icon",                 // 2
            "Cover (front)",                   // 3
            "Cover (back)",                    // 4
            "Leaflet page",                    // 5
            "Media",                           // 6
            "Lead artist/performer/soloist",   // 7
            "Artist/performer",                // 8
            "Conductor",                       // 9
            "Band/Orchestra",                  // 10
            "Composer",                        // 11
            "Lyricist/text writer",            // 12
            "Recording Location",              // 13
            "During recording",                // 14
            "During performance",              // 15
            "Movie/video screen capture",      // 16
            "A bright coloured fish",          // 17
            "Illustration",                    // 18
            "Band/artist logotype",            // 19
            "Publisher/Studio logotype"        // 20
        };
        if (type <= 20) {
            return type_names[type];
        }
        return "Unknown";
    }
};

/**
 * @brief FLAC frame information for streaming
 */
struct FLACFrame {
    uint64_t sample_offset = 0;      ///< Sample position of this frame in the stream
    uint64_t file_offset = 0;        ///< File position where frame starts
    uint32_t block_size = 0;         ///< Number of samples in this frame
    uint32_t frame_size = 0;         ///< Size of frame in bytes (estimated or actual)
    uint32_t sample_rate = 0;        ///< Sample rate for this frame
    uint8_t channels = 0;            ///< Channel count for this frame
    uint8_t bits_per_sample = 0;     ///< Bits per sample for this frame
    bool variable_block_size = false; ///< True if using variable block size strategy
    FLACChannelMode channel_mode = FLACChannelMode::INDEPENDENT;  ///< Channel assignment mode
    
    FLACFrame() = default;
    
    bool isValid() const {
        return block_size > 0 && sample_rate > 0 && 
               channels > 0 && bits_per_sample >= 4;
    }
};

/**
 * @brief FLAC frame index entry for sample-accurate seeking
 * 
 * Used to cache discovered frame positions during parsing for efficient seeking.
 * Per Requirements 22.4, 22.7: Build frame index during initial parsing and
 * provide sample-accurate seeking using index.
 */
struct FLACFrameIndexEntry {
    uint64_t sample_offset = 0;      ///< Sample position of this frame
    uint64_t file_offset = 0;        ///< File position where frame starts
    uint32_t block_size = 0;         ///< Number of samples in this frame
    
    FLACFrameIndexEntry() = default;
    
    FLACFrameIndexEntry(uint64_t sample, uint64_t file, uint32_t size)
        : sample_offset(sample), file_offset(file), block_size(size) {}
    
    bool isValid() const {
        return block_size > 0;
    }
};

/**
 * @brief FLAC container demuxer implementation per RFC 9639
 * 
 * This demuxer handles native FLAC files (.flac) by parsing the FLAC container
 * format and extracting FLAC bitstream data for decoding.
 * 
 * Thread safety: Uses public/private lock pattern per threading-safety-guidelines.md
 * Lock acquisition order:
 * 1. m_state_mutex (container state and position tracking)
 * 2. m_metadata_mutex (metadata access)
 */
class FLACDemuxer : public Demuxer {
public:
    /**
     * @brief Construct FLAC demuxer with I/O handler
     * @param handler IOHandler for reading FLAC data (takes ownership)
     */
    explicit FLACDemuxer(std::unique_ptr<IOHandler> handler);
    
    /**
     * @brief Destructor with proper resource cleanup
     */
    virtual ~FLACDemuxer();
    
    // ========================================================================
    // Demuxer interface implementation (public methods acquire locks)
    // ========================================================================
    
    bool parseContainer() override;
    std::vector<StreamInfo> getStreams() const override;
    StreamInfo getStreamInfo(uint32_t stream_id) const override;
    MediaChunk readChunk() override;
    MediaChunk readChunk(uint32_t stream_id) override;
    bool seekTo(uint64_t timestamp_ms) override;
    bool isEOF() const override;
    uint64_t getDuration() const override;
    uint64_t getPosition() const override;

private:
    // ========================================================================
    // Thread safety - Lock acquisition order documented above
    // ========================================================================
    mutable std::mutex m_state_mutex;    ///< Protects container state and position
    mutable std::mutex m_metadata_mutex; ///< Protects metadata access
    
    // ========================================================================
    // FLAC container state (protected by m_state_mutex)
    // ========================================================================
    bool m_container_parsed = false;     ///< True if container successfully parsed
    uint64_t m_file_size = 0;            ///< Total file size in bytes
    uint64_t m_audio_data_offset = 0;    ///< File offset where FLAC frames start
    uint64_t m_current_offset = 0;       ///< Current read position in file
    uint64_t m_current_sample = 0;       ///< Current sample position in stream
    bool m_eof = false;                  ///< End of file reached
    
    // ========================================================================
    // Frame parsing state (protected by m_state_mutex)
    // ========================================================================
    bool m_blocking_strategy_set = false;  ///< True if blocking strategy has been determined
    bool m_variable_block_size = false;    ///< True if variable block size (0xFFF9), false if fixed (0xFFF8)
    
    // ========================================================================
    // FLAC metadata (protected by m_metadata_mutex)
    // ========================================================================
    FLACStreamInfo m_streaminfo;                           ///< STREAMINFO block data
    std::vector<FLACSeekPoint> m_seektable;                ///< SEEKTABLE entries
    std::map<std::string, std::string> m_vorbis_comments;  ///< Vorbis comments metadata
    FLACCuesheet m_cuesheet;                               ///< CUESHEET block data
    std::vector<FLACPicture> m_pictures;                   ///< PICTURE block data (multiple allowed)
    
    // ========================================================================
    // Frame index for sample-accurate seeking (protected by m_metadata_mutex)
    // Requirements 22.4, 22.7: Build frame index during parsing for seeking
    // ========================================================================
    std::vector<FLACFrameIndexEntry> m_frame_index;        ///< Cached frame positions
    bool m_frame_index_complete = false;                   ///< True if entire file has been indexed
    
    // ========================================================================
    // Private unlocked implementations (assume locks are held)
    // ========================================================================
    bool parseContainer_unlocked();
    std::vector<StreamInfo> getStreams_unlocked() const;
    StreamInfo getStreamInfo_unlocked(uint32_t stream_id) const;
    MediaChunk readChunk_unlocked();
    bool seekTo_unlocked(uint64_t timestamp_ms);
    bool isEOF_unlocked() const;
    uint64_t getDuration_unlocked() const;
    uint64_t getPosition_unlocked() const;
    
    // ========================================================================
    // Helper methods (assume appropriate locks are held)
    // ========================================================================
    bool validateStreamMarker_unlocked();
    bool parseMetadataBlocks_unlocked();
    bool parseMetadataBlockHeader_unlocked(FLACMetadataBlock& block);
    bool parseStreamInfoBlock_unlocked(const FLACMetadataBlock& block);
    bool parseSeekTableBlock_unlocked(const FLACMetadataBlock& block);
    bool parseVorbisCommentBlock_unlocked(const FLACMetadataBlock& block);
    bool parsePaddingBlock_unlocked(const FLACMetadataBlock& block);
    bool parseApplicationBlock_unlocked(const FLACMetadataBlock& block);
    bool parseCuesheetBlock_unlocked(const FLACMetadataBlock& block);
    bool parsePictureBlock_unlocked(const FLACMetadataBlock& block);
    bool skipMetadataBlock_unlocked(const FLACMetadataBlock& block);
    
    bool findNextFrame_unlocked(FLACFrame& frame);
    bool parseFrameHeader_unlocked(FLACFrame& frame, const uint8_t* buffer, size_t size);
    uint32_t calculateFrameSize_unlocked(const FLACFrame& frame) const;
    
    /**
     * @brief Parse block size bits from frame header per RFC 9639 Table 14
     * 
     * Implements Requirements 5.1-5.18:
     * - Extracts bits 4-7 of frame byte 2
     * - Implements all 16 lookup table values
     * - Handles uncommon block sizes (8-bit and 16-bit)
     * - Rejects reserved pattern 0b0000
     * - Rejects forbidden uncommon block size 65536
     * 
     * @param bits The 4-bit block size code (bits 4-7 of frame byte 2)
     * @param buffer Pointer to frame data (for reading uncommon block sizes)
     * @param buffer_size Size of available buffer
     * @param header_offset Current offset within header (updated if uncommon bytes read)
     * @param block_size Output: the decoded block size in samples
     * @return true if block size is valid, false if reserved/forbidden pattern detected
     */
    bool parseBlockSizeBits_unlocked(uint8_t bits, const uint8_t* buffer, size_t buffer_size,
                                     size_t& header_offset, uint32_t& block_size);
    
    /**
     * @brief Parse sample rate bits from frame header per RFC 9639 Section 9.1.2
     * 
     * Implements Requirements 6.1-6.17:
     * - Extracts bits 0-3 of frame byte 2
     * - Implements all 16 lookup table values
     * - Handles uncommon sample rates in kHz, Hz, and tens of Hz
     * - Rejects forbidden pattern 0b1111
     * 
     * RFC 9639 Sample Rate Encoding:
     *   0b0000: Get from STREAMINFO (non-streamable subset)
     *   0b0001: 88200 Hz
     *   0b0010: 176400 Hz
     *   0b0011: 192000 Hz
     *   0b0100: 8000 Hz
     *   0b0101: 16000 Hz
     *   0b0110: 22050 Hz
     *   0b0111: 24000 Hz
     *   0b1000: 32000 Hz
     *   0b1001: 44100 Hz
     *   0b1010: 48000 Hz
     *   0b1011: 96000 Hz
     *   0b1100: 8-bit uncommon sample rate in kHz follows
     *   0b1101: 16-bit uncommon sample rate in Hz follows
     *   0b1110: 16-bit uncommon sample rate in tens of Hz follows
     *   0b1111: Forbidden (reject)
     * 
     * @param bits The 4-bit sample rate code (bits 0-3 of frame byte 2)
     * @param buffer Pointer to frame data (for reading uncommon sample rates)
     * @param buffer_size Size of available buffer
     * @param header_offset Current offset within header (updated if uncommon bytes read)
     * @param sample_rate Output: the decoded sample rate in Hz
     * @return true if sample rate is valid, false if forbidden pattern detected
     */
    bool parseSampleRateBits_unlocked(uint8_t bits, const uint8_t* buffer, size_t buffer_size,
                                      size_t& header_offset, uint32_t& sample_rate);
    
    /**
     * @brief Parse channel assignment bits from frame header per RFC 9639 Section 9.1.3
     * 
     * Implements Requirements 7.1-7.7:
     * - Extracts bits 4-7 of frame byte 3
     * - Supports independent channels 0b0000-0b0111 (1-8 channels)
     * - Supports stereo modes: left-side (0b1000), right-side (0b1001), mid-side (0b1010)
     * - Rejects reserved patterns 0b1011-0b1111
     * 
     * RFC 9639 Channel Assignment Encoding:
     *   0b0000-0b0111: 1-8 independent channels (value + 1)
     *   0b1000: Left-side stereo (left + side)
     *   0b1001: Right-side stereo (side + right)
     *   0b1010: Mid-side stereo (mid + side)
     *   0b1011-0b1111: Reserved (reject)
     * 
     * @param bits The 4-bit channel assignment code (bits 4-7 of frame byte 3)
     * @param channels Output: the number of channels (1-8)
     * @param mode Output: the channel assignment mode (independent or stereo variant)
     * @return true if channel assignment is valid, false if reserved pattern detected
     */
    bool parseChannelBits_unlocked(uint8_t bits, uint8_t& channels, FLACChannelMode& mode);
    
    /**
     * @brief Parse bit depth bits from frame header per RFC 9639 Section 9.1.4
     * 
     * Implements Requirements 8.1-8.11:
     * - Extracts bits 1-3 of frame byte 3
     * - Implements all 8 lookup table values
     * - Rejects reserved pattern 0b011
     * - Validates reserved bit at bit 0 is zero (logs warning if non-zero)
     * 
     * RFC 9639 Bit Depth Encoding:
     *   0b000: Get from STREAMINFO (non-streamable subset)
     *   0b001: 8 bits per sample
     *   0b010: 12 bits per sample
     *   0b011: Reserved (reject)
     *   0b100: 16 bits per sample
     *   0b101: 20 bits per sample
     *   0b110: 24 bits per sample
     *   0b111: 32 bits per sample
     * 
     * @param bits The 3-bit bit depth code (bits 1-3 of frame byte 3)
     * @param reserved_bit The reserved bit (bit 0 of frame byte 3) - must be 0
     * @param bit_depth Output: the decoded bit depth (4-32 bits per sample)
     * @return true if bit depth is valid, false if reserved pattern detected
     */
    bool parseBitDepthBits_unlocked(uint8_t bits, uint8_t reserved_bit, uint8_t& bit_depth);
    
    /**
     * @brief Parse coded number from frame header per RFC 9639 Section 9.1.5
     * 
     * Implements Requirements 9.1-9.10 for coded number decoding using
     * UTF-8-like variable-length encoding (1-7 bytes).
     * 
     * RFC 9639 Coded Number Encoding (extended UTF-8):
     *   0xxxxxxx                                           - 1 byte  (7 bits, 0-127)
     *   110xxxxx 10xxxxxx                                  - 2 bytes (11 bits, 128-2047)
     *   1110xxxx 10xxxxxx 10xxxxxx                         - 3 bytes (16 bits, 2048-65535)
     *   11110xxx 10xxxxxx 10xxxxxx 10xxxxxx                - 4 bytes (21 bits, 65536-2097151)
     *   111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx       - 5 bytes (26 bits, 2097152-67108863)
     *   1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx - 6 bytes (31 bits, 67108864-2147483647)
     *   11111110 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx - 7 bytes (36 bits)
     * 
     * For fixed block size streams: coded number is frame number (max 31 bits, 6 bytes)
     * For variable block size streams: coded number is sample number (max 36 bits, 7 bytes)
     * 
     * @param buffer Pointer to frame data starting at the coded number position
     * @param buffer_size Size of available buffer
     * @param bytes_consumed Output: number of bytes consumed by the coded number
     * @param coded_number Output: the decoded frame/sample number
     * @param is_variable_block_size True if variable block size (sample number), false if fixed (frame number)
     * @return true if coded number is valid, false if encoding is invalid
     */
    bool parseCodedNumber_unlocked(const uint8_t* buffer, size_t buffer_size,
                                   size_t& bytes_consumed, uint64_t& coded_number,
                                   bool is_variable_block_size);
    
    // ========================================================================
    // CRC-8 Validation (RFC 9639 Section 9.1.8)
    // ========================================================================
    
    /**
     * @brief Calculate CRC-8 checksum for frame header per RFC 9639 Section 9.1.8
     * 
     * Implements Requirements 10.1-10.3:
     * - Uses polynomial 0x07 (x^8 + x^2 + x + 1)
     * - Initializes CRC to 0
     * - Covers all frame header bytes except the CRC byte itself
     * 
     * @param data Pointer to frame header data (starting from sync code)
     * @param length Number of bytes to include in CRC calculation (excluding CRC byte)
     * @return The calculated 8-bit CRC value
     */
    static uint8_t calculateCRC8(const uint8_t* data, size_t length);
    
    /**
     * @brief Validate frame header CRC-8 per RFC 9639 Section 9.1.8
     * 
     * Implements Requirements 10.4-10.6:
     * - Validates CRC-8 after parsing header
     * - Logs CRC mismatches with frame position
     * - Attempts resynchronization on failure
     * - Supports strict mode rejection
     * 
     * @param header_data Pointer to complete frame header data (including CRC byte)
     * @param header_length Total length of frame header including CRC byte
     * @param frame_offset File offset of the frame (for logging)
     * @return true if CRC is valid or strict mode is disabled, false if CRC fails in strict mode
     */
    bool validateFrameHeaderCRC_unlocked(const uint8_t* header_data, size_t header_length,
                                         uint64_t frame_offset);
    
    uint64_t samplesToMs(uint64_t samples) const;
    uint64_t msToSamples(uint64_t ms) const;
    
    // ========================================================================
    // Seeking Helper Methods (Requirements 22.1-22.8)
    // ========================================================================
    
    /**
     * @brief Seek using SEEKTABLE entries per RFC 9639 Section 8.5
     * 
     * Implements Requirements 22.2, 22.3, 22.8:
     * - Find closest seek point not exceeding target sample
     * - Add byte offset to first frame header position
     * - Parse frames forward to exact position
     * 
     * @param target_sample Target sample position to seek to
     * @return true if seek succeeded, false otherwise
     */
    bool seekWithSeekTable_unlocked(uint64_t target_sample);
    
    /**
     * @brief Seek using frame index for sample-accurate positioning
     * 
     * Implements Requirements 22.4, 22.7:
     * - Use cached frame positions for seeking
     * - Provide sample-accurate seeking using index
     * 
     * @param target_sample Target sample position to seek to
     * @return true if seek succeeded, false otherwise
     */
    bool seekWithFrameIndex_unlocked(uint64_t target_sample);
    
    /**
     * @brief Add a frame to the frame index
     * 
     * Implements Requirement 22.4: Build frame index during initial parsing
     * 
     * @param frame Frame information to add to index
     */
    void addFrameToIndex_unlocked(const FLACFrame& frame);
    
    /**
     * @brief Parse frames forward from current position to target sample
     * 
     * Implements Requirement 22.3: Parse frames forward to exact position
     * 
     * @param target_sample Target sample position to reach
     * @return true if target was reached, false otherwise
     */
    bool parseFramesToSample_unlocked(uint64_t target_sample);
    
    // ========================================================================
    // CRC-16 Validation (RFC 9639 Section 9.3)
    // ========================================================================
    
    /**
     * @brief Calculate CRC-16 checksum for frame data per RFC 9639 Section 9.3
     * 
     * Implements Requirements 11.2-11.5:
     * - Uses polynomial 0x8005 (x^16 + x^15 + x^2 + x^0)
     * - Initializes CRC to 0
     * - Covers entire frame from sync code to end of subframes (excluding CRC itself)
     * 
     * @param data Pointer to frame data (starting from sync code)
     * @param length Number of bytes to include in CRC calculation (excluding CRC bytes)
     * @return The calculated 16-bit CRC value
     */
    static uint16_t calculateCRC16(const uint8_t* data, size_t length);
    
    /**
     * @brief Validate frame footer CRC-16 per RFC 9639 Section 9.3
     * 
     * Implements Requirements 11.1, 11.6-11.8:
     * - Ensures byte alignment with zero padding
     * - Reads 16-bit CRC from footer
     * - Logs CRC mismatches and attempts to continue
     * - Supports strict mode rejection
     * 
     * @param frame_data Pointer to complete frame data (including CRC bytes)
     * @param frame_length Total length of frame including CRC bytes
     * @param frame_offset File offset of the frame (for logging)
     * @return true if CRC is valid or strict mode is disabled, false if CRC fails in strict mode
     */
    bool validateFrameFooterCRC_unlocked(const uint8_t* frame_data, size_t frame_length,
                                          uint64_t frame_offset);
    
    // ========================================================================
    // Error Handling and Recovery (Requirements 24.1-24.8)
    // ========================================================================
    
    /**
     * @brief Attempt to derive stream parameters from frame headers
     * 
     * Implements Requirement 24.3: If STREAMINFO is missing, derive parameters
     * from frame headers. This is a fallback mechanism for corrupted files.
     * 
     * @return true if parameters were successfully derived, false otherwise
     */
    bool deriveParametersFromFrameHeaders_unlocked();
    
    /**
     * @brief Resynchronize to the next valid frame sync code after sync loss
     * 
     * Implements Requirement 24.4: If frame sync is lost, resynchronize to
     * the next valid sync code. This enables recovery from corrupted frames.
     * 
     * @param max_search_bytes Maximum bytes to search (default 4096)
     * @return true if resynchronization succeeded, false if no sync found
     */
    bool resyncToNextFrame_unlocked(size_t max_search_bytes = 4096);
    
    /**
     * @brief Skip a corrupted frame and attempt to continue playback
     * 
     * Implements Requirements 24.5, 24.6: Log CRC errors but attempt to
     * continue playback. Handle truncated files gracefully.
     * 
     * @param frame_offset File offset of the corrupted frame
     * @return true if successfully skipped to next frame, false otherwise
     */
    bool skipCorruptedFrame_unlocked(uint64_t frame_offset);
    
    /**
     * @brief Handle memory allocation failure gracefully
     * 
     * Implements Requirement 24.7: Return appropriate error codes on
     * allocation failure without crashing.
     * 
     * @param operation Description of the operation that failed
     * @param requested_size Size of the failed allocation
     */
    void handleAllocationFailure_unlocked(const char* operation, size_t requested_size);
    
    /**
     * @brief Handle I/O error gracefully
     * 
     * Implements Requirement 24.8: Handle read errors and EOF conditions
     * without crashing.
     * 
     * @param operation Description of the I/O operation that failed
     * @return false always (to indicate error to caller)
     */
    bool handleIOError_unlocked(const char* operation);
    
    // ========================================================================
    // CRC Lookup Tables (RFC 9639)
    // ========================================================================
    static const uint8_t s_crc8_table[256];   ///< CRC-8 polynomial 0x07
    static const uint16_t s_crc16_table[256]; ///< CRC-16 polynomial 0x8005
};

} // namespace FLAC
} // namespace Demuxer
} // namespace PsyMP3

#endif // FLACDEMUXER_H
