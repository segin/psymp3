/*
 * FLACRFC9639.h - RFC 9639 FLAC specification compliance utilities
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

#ifndef FLACRFC9639_H
#define FLACRFC9639_H

// No direct includes - all includes should be in psymp3.h

/**
 * @brief RFC 9639 FLAC Specification Compliance Utilities
 * 
 * This namespace provides utilities for strict RFC 9639 compliance including:
 * - Stream marker validation (Section 6)
 * - Metadata block header parsing (Section 8.1)
 * - STREAMINFO parsing (Section 8.2)
 * - Frame header parsing (Section 9.1)
 * - CRC validation (Sections 9.1.8, 9.3)
 * - Big-endian integer parsing (Section 5)
 * - Forbidden pattern detection (Section 5, Table 1)
 * - Streamable subset validation (Section 7)
 */
namespace FLACRFC9639 {

// ============================================================================
// RFC 9639 Section 6: Stream Marker Validation
// ============================================================================

/**
 * @brief FLAC stream marker bytes per RFC 9639 Section 6
 * 
 * The FLAC stream marker is the 4-byte ASCII string "fLaC" (0x66 0x4C 0x61 0x43)
 * that must appear at the beginning of every native FLAC file.
 */
constexpr uint8_t FLAC_STREAM_MARKER[4] = {0x66, 0x4C, 0x61, 0x43};  // "fLaC"

/**
 * @brief Validate FLAC stream marker per RFC 9639 Section 6
 * 
 * Validates that the provided 4-byte sequence matches the exact FLAC stream marker.
 * Per RFC 9639 Section 6, the stream marker MUST be exactly 0x66 0x4C 0x61 0x43.
 * 
 * @param marker Pointer to 4-byte buffer containing potential stream marker
 * @return true if marker is valid, false otherwise
 * 
 * @note This function does not log errors. Use validateStreamMarkerWithLogging()
 *       for detailed error reporting.
 */
inline bool validateStreamMarker(const uint8_t* marker) {
    if (!marker) return false;
    
    return marker[0] == FLAC_STREAM_MARKER[0] &&
           marker[1] == FLAC_STREAM_MARKER[1] &&
           marker[2] == FLAC_STREAM_MARKER[2] &&
           marker[3] == FLAC_STREAM_MARKER[3];
}

/**
 * @brief Validate FLAC stream marker with detailed logging
 * 
 * Validates the stream marker and logs detailed information about what was found
 * versus what was expected. This is useful for debugging format detection issues.
 * 
 * @param marker Pointer to 4-byte buffer containing potential stream marker
 * @param debug_channel Debug channel name for logging (e.g., "flac")
 * @return true if marker is valid, false otherwise
 */
bool validateStreamMarkerWithLogging(const uint8_t* marker, const char* debug_channel = "flac");

/**
 * @brief Get human-readable description of stream marker validation failure
 * 
 * Analyzes the provided marker bytes and returns a descriptive error message
 * explaining why the marker is invalid. This helps identify common format
 * misidentifications (e.g., MP3, Ogg, WAV).
 * 
 * @param marker Pointer to 4-byte buffer containing invalid stream marker
 * @return String describing the validation failure
 */
std::string getStreamMarkerErrorDescription(const uint8_t* marker);

// ============================================================================
// RFC 9639 Section 5: Big-Endian Integer Parsing
// ============================================================================

/**
 * @brief Parse 16-bit unsigned big-endian integer per RFC 9639 Section 5
 * 
 * All FLAC integers are big-endian except Vorbis comment lengths (little-endian).
 * 
 * @param data Pointer to 2-byte buffer containing big-endian integer
 * @return Parsed 16-bit unsigned integer value
 */
inline uint16_t parseBigEndianU16(const uint8_t* data) {
    return (static_cast<uint16_t>(data[0]) << 8) |
            static_cast<uint16_t>(data[1]);
}

/**
 * @brief Parse 24-bit unsigned big-endian integer per RFC 9639 Section 5
 * 
 * Used for metadata block lengths and STREAMINFO frame sizes.
 * 
 * @param data Pointer to 3-byte buffer containing big-endian integer
 * @return Parsed 24-bit unsigned integer value (stored in uint32_t)
 */
inline uint32_t parseBigEndianU24(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 16) |
           (static_cast<uint32_t>(data[1]) << 8) |
            static_cast<uint32_t>(data[2]);
}

/**
 * @brief Parse 32-bit unsigned big-endian integer per RFC 9639 Section 5
 * 
 * Used for various FLAC metadata fields.
 * 
 * @param data Pointer to 4-byte buffer containing big-endian integer
 * @return Parsed 32-bit unsigned integer value
 */
inline uint32_t parseBigEndianU32(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
            static_cast<uint32_t>(data[3]);
}

/**
 * @brief Parse 64-bit unsigned big-endian integer per RFC 9639 Section 5
 * 
 * Used for seek table sample numbers and byte offsets.
 * 
 * @param data Pointer to 8-byte buffer containing big-endian integer
 * @return Parsed 64-bit unsigned integer value
 */
inline uint64_t parseBigEndianU64(const uint8_t* data) {
    return (static_cast<uint64_t>(data[0]) << 56) |
           (static_cast<uint64_t>(data[1]) << 48) |
           (static_cast<uint64_t>(data[2]) << 40) |
           (static_cast<uint64_t>(data[3]) << 32) |
           (static_cast<uint64_t>(data[4]) << 24) |
           (static_cast<uint64_t>(data[5]) << 16) |
           (static_cast<uint64_t>(data[6]) << 8) |
            static_cast<uint64_t>(data[7]);
}

/**
 * @brief Parse 32-bit unsigned little-endian integer (Vorbis comments only)
 * 
 * Per RFC 9639 Section 8.6, Vorbis comment lengths are little-endian
 * (exception to FLAC's big-endian rule for Vorbis compatibility).
 * 
 * @param data Pointer to 4-byte buffer containing little-endian integer
 * @return Parsed 32-bit unsigned integer value
 */
inline uint32_t parseLittleEndianU32(const uint8_t* data) {
    return  static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

// ============================================================================
// RFC 9639 Section 8.1: Metadata Block Header Parsing
// ============================================================================

/**
 * @brief Metadata block type enumeration per RFC 9639 Section 8
 */
enum class MetadataBlockType : uint8_t {
    STREAMINFO = 0,      ///< Stream information (mandatory, always first)
    PADDING = 1,         ///< Padding block for future metadata
    APPLICATION = 2,     ///< Application-specific data
    SEEKTABLE = 3,       ///< Seek table for efficient seeking
    VORBIS_COMMENT = 4,  ///< Vorbis-style comments (metadata)
    CUESHEET = 5,        ///< Cue sheet for CD-like track information
    PICTURE = 6,         ///< Embedded picture/artwork
    RESERVED_MIN = 7,    ///< Start of reserved range
    RESERVED_MAX = 126,  ///< End of reserved range
    FORBIDDEN = 127      ///< Forbidden block type per RFC 9639 Table 1
};

/**
 * @brief Metadata block header structure per RFC 9639 Section 8.1
 */
struct MetadataBlockHeader {
    bool is_last;                    ///< True if this is the last metadata block
    MetadataBlockType block_type;    ///< Block type (0-126 valid, 127 forbidden)
    uint32_t block_length;           ///< Length of block data in bytes (24-bit value)
    
    MetadataBlockHeader() 
        : is_last(false), block_type(MetadataBlockType::FORBIDDEN), block_length(0) {}
    
    /**
     * @brief Check if block type is valid (not forbidden or reserved)
     */
    bool isValidType() const {
        uint8_t type_value = static_cast<uint8_t>(block_type);
        return type_value <= 6;  // Only types 0-6 are currently defined
    }
    
    /**
     * @brief Check if block type is reserved for future use
     */
    bool isReservedType() const {
        uint8_t type_value = static_cast<uint8_t>(block_type);
        return type_value >= 7 && type_value <= 126;
    }
    
    /**
     * @brief Check if block type is forbidden per RFC 9639 Table 1
     */
    bool isForbiddenType() const {
        return block_type == MetadataBlockType::FORBIDDEN;
    }
    
    /**
     * @brief Get human-readable block type name
     */
    const char* getTypeName() const;
};

/**
 * @brief Parse metadata block header per RFC 9639 Section 8.1
 * 
 * Parses the 4-byte metadata block header:
 * - Byte 0, bit 7: is_last flag (0=more blocks, 1=last block)
 * - Byte 0, bits 0-6: block type (0-126 valid, 127 forbidden)
 * - Bytes 1-3: 24-bit big-endian block length
 * 
 * @param data Pointer to 4-byte buffer containing metadata block header
 * @param header Output parameter for parsed header
 * @return true if header was parsed successfully, false if forbidden type detected
 */
bool parseMetadataBlockHeader(const uint8_t* data, MetadataBlockHeader& header);

/**
 * @brief Parse metadata block header with detailed logging
 * 
 * @param data Pointer to 4-byte buffer containing metadata block header
 * @param header Output parameter for parsed header
 * @param debug_channel Debug channel name for logging
 * @return true if header was parsed successfully, false if forbidden type detected
 */
bool parseMetadataBlockHeaderWithLogging(const uint8_t* data, MetadataBlockHeader& header, 
                                        const char* debug_channel = "flac");

/**
 * @brief Validate metadata block length is reasonable
 * 
 * Checks that the block length is within reasonable bounds to prevent
 * memory exhaustion attacks or corrupted metadata.
 * 
 * @param header Metadata block header to validate
 * @param file_size Total file size (0 if unknown)
 * @return true if block length is reasonable, false otherwise
 */
bool validateMetadataBlockLength(const MetadataBlockHeader& header, uint64_t file_size = 0);

// ============================================================================
// RFC 9639 Section 8.2: STREAMINFO Block Parsing
// ============================================================================

/**
 * @brief STREAMINFO block data structure per RFC 9639 Section 8.2
 * 
 * STREAMINFO is exactly 34 bytes and contains essential stream parameters.
 * It is mandatory and must be the first metadata block.
 * 
 * Note: Named FLACStreamInfo to avoid conflict with Demuxer::StreamInfo
 */
struct FLACStreamInfo {
    uint16_t min_block_size;     ///< Minimum block size in samples (16-bit)
    uint16_t max_block_size;     ///< Maximum block size in samples (16-bit)
    uint32_t min_frame_size;     ///< Minimum frame size in bytes, 0=unknown (24-bit)
    uint32_t max_frame_size;     ///< Maximum frame size in bytes, 0=unknown (24-bit)
    uint32_t sample_rate;        ///< Sample rate in Hz (20-bit)
    uint8_t channels;            ///< Number of channels: 1-8 (stored as channels-1, 3-bit)
    uint8_t bits_per_sample;     ///< Bits per sample: 4-32 (stored as bps-1, 5-bit)
    uint64_t total_samples;      ///< Total samples in stream, 0=unknown (36-bit)
    uint8_t md5_signature[16];   ///< MD5 signature of uncompressed audio data (128-bit)
    
    FLACStreamInfo() : min_block_size(0), max_block_size(0), min_frame_size(0), 
                       max_frame_size(0), sample_rate(0), channels(0), 
                       bits_per_sample(0), total_samples(0) {
        std::memset(md5_signature, 0, sizeof(md5_signature));
    }
    
    /**
     * @brief Check if STREAMINFO contains valid data
     * 
     * Validates:
     * - Sample rate > 0 (required for audio streams)
     * - Channels in range 1-8
     * - Bits per sample in range 4-32
     * - Min block size >= 16 (forbidden pattern check)
     * - Max block size >= min block size
     */
    bool isValid() const {
        return sample_rate > 0 && 
               channels >= 1 && channels <= 8 &&
               bits_per_sample >= 4 && bits_per_sample <= 32 &&
               min_block_size >= 16 &&  // RFC 9639 Table 1 forbidden pattern
               max_block_size >= 16 &&  // RFC 9639 Table 1 forbidden pattern
               max_block_size >= min_block_size;
    }
    
    /**
     * @brief Calculate duration in milliseconds from total samples
     */
    uint64_t getDurationMs() const {
        if (sample_rate == 0 || total_samples == 0) return 0;
        return (total_samples * 1000) / sample_rate;
    }
};

/**
 * @brief Parse STREAMINFO block per RFC 9639 Section 8.2
 * 
 * Parses the 34-byte STREAMINFO block with bit-level field extraction:
 * - Bytes 0-1: u(16) min block size
 * - Bytes 2-3: u(16) max block size
 * - Bytes 4-6: u(24) min frame size
 * - Bytes 7-9: u(24) max frame size
 * - Bytes 10-12 (bits 0-19): u(20) sample rate
 * - Byte 12 (bits 20-22): u(3) channels-1
 * - Bytes 12-13 (bits 23-27): u(5) bits per sample-1
 * - Bytes 13-17 (bits 28-63): u(36) total samples
 * - Bytes 18-33: u(128) MD5 signature
 * 
 * @param data Pointer to 34-byte STREAMINFO block data
 * @param info Output parameter for parsed STREAMINFO
 * @return true if successfully parsed and valid, false if forbidden patterns detected
 */
bool parseFLACStreamInfo(const uint8_t* data, FLACStreamInfo& info);

/**
 * @brief Parse STREAMINFO block with detailed logging
 * 
 * @param data Pointer to 34-byte STREAMINFO block data
 * @param info Output parameter for parsed STREAMINFO
 * @param debug_channel Debug channel name for logging
 * @return true if successfully parsed and valid, false if forbidden patterns detected
 */
bool parseFLACStreamInfoWithLogging(const uint8_t* data, FLACStreamInfo& info,
                                   const char* debug_channel = "flac");

/**
 * @brief Validate STREAMINFO parameters per RFC 9639 requirements
 * 
 * Checks for:
 * - Forbidden patterns (min/max block size < 16)
 * - Sample rate > 0 for audio streams
 * - Valid channel count (1-8)
 * - Valid bit depth (4-32)
 * - Logical consistency (max >= min for block/frame sizes)
 * 
 * @param info STREAMINFO structure to validate
 * @return true if valid, false if forbidden patterns or invalid parameters detected
 */
bool validateFLACStreamInfo(const FLACStreamInfo& info);

// ============================================================================
// RFC 9639 Section 9.1: Frame Sync Code Detection
// ============================================================================

/**
 * @brief FLAC frame sync pattern per RFC 9639 Section 9.1
 * 
 * The sync code is 15 bits: 0b111111111111100 (0xFFF8 when byte-aligned)
 * Bit 0 of the second byte indicates blocking strategy:
 * - 0 = fixed block size (sync pattern 0xFFF8)
 * - 1 = variable block size (sync pattern 0xFFF9)
 */
constexpr uint16_t FLAC_FRAME_SYNC_PATTERN = 0xFFF8;  // 15-bit sync: 0b111111111111100
constexpr uint16_t FLAC_FRAME_SYNC_MASK = 0xFFFE;     // Mask for 15-bit sync (ignore blocking bit)

/**
 * @brief Blocking strategy enumeration per RFC 9639 Section 9.1
 */
enum class BlockingStrategy : uint8_t {
    FIXED = 0,      ///< Fixed block size - frame header contains frame number
    VARIABLE = 1    ///< Variable block size - frame header contains sample number
};

/**
 * @brief Frame sync detection result
 */
struct FrameSyncResult {
    bool found;                          ///< True if sync pattern was found
    size_t offset;                       ///< Byte offset where sync pattern starts
    BlockingStrategy blocking_strategy;  ///< Blocking strategy from sync code
    
    FrameSyncResult() : found(false), offset(0), blocking_strategy(BlockingStrategy::FIXED) {}
    
    FrameSyncResult(bool f, size_t off, BlockingStrategy bs) 
        : found(f), offset(off), blocking_strategy(bs) {}
};

/**
 * @brief Detect FLAC frame sync pattern per RFC 9639 Section 9.1
 * 
 * Searches for the 15-bit sync pattern 0b111111111111100 in the provided buffer.
 * The sync pattern must be byte-aligned.
 * 
 * @param data Pointer to data buffer to search
 * @param size Size of data buffer in bytes
 * @param result Output parameter for sync detection result
 * @return true if sync pattern found, false otherwise
 */
bool detectFrameSync(const uint8_t* data, size_t size, FrameSyncResult& result);

/**
 * @brief Detect frame sync pattern with detailed logging
 * 
 * @param data Pointer to data buffer to search
 * @param size Size of data buffer in bytes
 * @param result Output parameter for sync detection result
 * @param debug_channel Debug channel name for logging
 * @return true if sync pattern found, false otherwise
 */
bool detectFrameSyncWithLogging(const uint8_t* data, size_t size, FrameSyncResult& result,
                               const char* debug_channel = "flac");

/**
 * @brief Validate frame sync pattern at specific offset
 * 
 * Checks if the bytes at the given offset form a valid FLAC frame sync pattern.
 * 
 * @param data Pointer to data buffer
 * @param offset Offset in buffer to check
 * @param size Total size of buffer
 * @return true if valid sync pattern at offset, false otherwise
 */
bool validateFrameSyncAt(const uint8_t* data, size_t offset, size_t size);

/**
 * @brief Extract blocking strategy from sync code
 * 
 * The blocking strategy is encoded in bit 0 of the second sync byte:
 * - 0 = fixed block size
 * - 1 = variable block size
 * 
 * @param sync_bytes Pointer to 2-byte sync pattern
 * @return Blocking strategy extracted from sync code
 */
inline BlockingStrategy extractBlockingStrategy(const uint8_t* sync_bytes) {
    return (sync_bytes[1] & 0x01) ? BlockingStrategy::VARIABLE : BlockingStrategy::FIXED;
}

// ============================================================================
// RFC 9639 Section 9.1.1: Block Size Bits Parser (Table 14)
// ============================================================================

/**
 * @brief Parse block size from 4-bit encoding per RFC 9639 Table 14
 * 
 * @param block_size_bits 4-bit block size encoding from frame header
 * @param uncommon_block_size_8bit Optional 8-bit uncommon block size (for codes 0110, 0111)
 * @param uncommon_block_size_16bit Optional 16-bit uncommon block size (for codes 0110, 0111)
 * @param block_size Output parameter for decoded block size in samples
 * @return true if successfully parsed, false if reserved/forbidden pattern
 */
bool parseBlockSizeBits(uint8_t block_size_bits, uint8_t uncommon_block_size_8bit,
                       uint16_t uncommon_block_size_16bit, uint32_t& block_size);

// ============================================================================
// RFC 9639 Section 9.1.2: Sample Rate Bits Parser
// ============================================================================

/**
 * @brief Parse sample rate from 4-bit encoding per RFC 9639 Section 9.1.2
 * 
 * @param sample_rate_bits 4-bit sample rate encoding from frame header
 * @param uncommon_sample_rate_8bit Optional 8-bit uncommon sample rate (for codes 1100-1110)
 * @param uncommon_sample_rate_16bit Optional 16-bit uncommon sample rate (for codes 1100-1110)
 * @param sample_rate Output parameter for decoded sample rate in Hz
 * @return true if successfully parsed, false if forbidden pattern (0b1111)
 */
bool parseSampleRateBits(uint8_t sample_rate_bits, uint8_t uncommon_sample_rate_8bit,
                        uint16_t uncommon_sample_rate_16bit, uint32_t& sample_rate);

// ============================================================================
// RFC 9639 Section 9.1.3: Channel Assignment Parser
// ============================================================================

/**
 * @brief Channel assignment enumeration per RFC 9639 Section 9.1.3
 */
enum class ChannelAssignment : uint8_t {
    INDEPENDENT_1 = 0,   ///< 1 channel (mono)
    INDEPENDENT_2 = 1,   ///< 2 channels (stereo)
    INDEPENDENT_3 = 2,   ///< 3 channels
    INDEPENDENT_4 = 3,   ///< 4 channels (quad)
    INDEPENDENT_5 = 4,   ///< 5 channels
    INDEPENDENT_6 = 5,   ///< 6 channels (5.1)
    INDEPENDENT_7 = 6,   ///< 7 channels
    INDEPENDENT_8 = 7,   ///< 8 channels (7.1)
    LEFT_SIDE = 8,       ///< Left/side stereo
    RIGHT_SIDE = 9,      ///< Right/side stereo
    MID_SIDE = 10,       ///< Mid/side stereo
    RESERVED = 255       ///< Reserved/invalid
};

/**
 * @brief Parse channel assignment from 4-bit encoding per RFC 9639 Section 9.1.3
 * 
 * @param channel_bits 4-bit channel assignment encoding from frame header
 * @param assignment Output parameter for channel assignment type
 * @param channels Output parameter for number of channels
 * @return true if successfully parsed, false if reserved pattern (0b1011-0b1111)
 */
bool parseChannelAssignment(uint8_t channel_bits, ChannelAssignment& assignment, uint8_t& channels);

// ============================================================================
// RFC 9639 Section 9.1.4: Bit Depth Parser
// ============================================================================

/**
 * @brief Parse bit depth from 3-bit encoding per RFC 9639 Section 9.1.4
 * 
 * @param bit_depth_bits 3-bit bit depth encoding from frame header
 * @param bits_per_sample Output parameter for decoded bits per sample
 * @return true if successfully parsed, false if reserved pattern (0b011)
 */
bool parseBitDepthBits(uint8_t bit_depth_bits, uint8_t& bits_per_sample);

// ============================================================================
// RFC 9639 Section 8.5: SEEKTABLE Parser
// ============================================================================

/**
 * @brief SEEKTABLE seek point structure per RFC 9639 Section 8.5
 */
struct SeekPoint {
    uint64_t sample_number;   ///< Sample number of first sample in target frame
    uint64_t stream_offset;   ///< Byte offset from first frame to target frame
    uint16_t frame_samples;   ///< Number of samples in target frame
    
    SeekPoint() : sample_number(0), stream_offset(0), frame_samples(0) {}
    
    SeekPoint(uint64_t sample, uint64_t offset, uint16_t samples)
        : sample_number(sample), stream_offset(offset), frame_samples(samples) {}
    
    /**
     * @brief Check if this is a placeholder seek point
     */
    bool isPlaceholder() const {
        return sample_number == 0xFFFFFFFFFFFFFFFFULL;
    }
    
    /**
     * @brief Check if this seek point is valid
     */
    bool isValid() const {
        return !isPlaceholder() && frame_samples > 0;
    }
};

/**
 * @brief Parse SEEKTABLE block per RFC 9639 Section 8.5
 * 
 * @param data Pointer to SEEKTABLE block data
 * @param length Length of SEEKTABLE block in bytes (must be multiple of 18)
 * @param seek_points Output vector of parsed seek points
 * @return true if successfully parsed, false if invalid length
 */
bool parseSeekTable(const uint8_t* data, uint32_t length, std::vector<SeekPoint>& seek_points);

/**
 * @brief Validate SEEKTABLE per RFC 9639 Section 8.5
 * 
 * Checks that:
 * - Seek points are sorted in ascending order by sample number
 * - Seek points are unique (except placeholders)
 * 
 * @param seek_points Vector of seek points to validate
 * @return true if valid, false if validation fails
 */
bool validateSeekTable(const std::vector<SeekPoint>& seek_points);

// ============================================================================
// RFC 9639 Section 8.6: VORBIS_COMMENT Parser
// ============================================================================

/**
 * @brief Parse VORBIS_COMMENT block per RFC 9639 Section 8.6
 * 
 * Note: Vorbis comment lengths use little-endian encoding (exception to FLAC's big-endian rule)
 * 
 * @param data Pointer to VORBIS_COMMENT block data
 * @param length Length of VORBIS_COMMENT block in bytes
 * @param vendor_string Output parameter for vendor string
 * @param comments Output map of comment name/value pairs
 * @return true if successfully parsed, false on error
 */
bool parseVorbisComment(const uint8_t* data, uint32_t length, 
                       std::string& vendor_string,
                       std::map<std::string, std::string>& comments);

/**
 * @brief Validate Vorbis comment field name per RFC 9639 Section 8.6
 * 
 * Field names must contain only printable ASCII 0x20-0x7E except 0x3D (equals sign)
 * 
 * @param field_name Field name to validate
 * @return true if valid, false otherwise
 */
bool validateVorbisCommentFieldName(const std::string& field_name);

// ============================================================================
// RFC 9639 Section 8.8: PICTURE Parser
// ============================================================================

/**
 * @brief PICTURE block structure per RFC 9639 Section 8.8
 */
struct Picture {
    uint32_t picture_type;       ///< Picture type (0=Other, 3=Cover front, etc.)
    std::string mime_type;       ///< MIME type (e.g., "image/jpeg")
    std::string description;     ///< Picture description (UTF-8)
    uint32_t width;              ///< Picture width in pixels
    uint32_t height;             ///< Picture height in pixels
    uint32_t color_depth;        ///< Color depth in bits per pixel
    uint32_t colors_used;        ///< Number of colors used (0 for non-indexed)
    std::vector<uint8_t> data;   ///< Picture data
    
    Picture() : picture_type(0), width(0), height(0), color_depth(0), colors_used(0) {}
    
    /**
     * @brief Check if picture metadata is valid
     */
    bool isValid() const {
        return !mime_type.empty() && !data.empty() && width > 0 && height > 0;
    }
    
    /**
     * @brief Check if this is a URI reference (MIME type "-->")
     */
    bool isURI() const {
        return mime_type == "-->";
    }
};

/**
 * @brief Parse PICTURE block per RFC 9639 Section 8.8
 * 
 * @param data Pointer to PICTURE block data
 * @param length Length of PICTURE block in bytes
 * @param picture Output parameter for parsed picture
 * @return true if successfully parsed, false on error
 */
bool parsePicture(const uint8_t* data, uint32_t length, Picture& picture);

// ============================================================================
// RFC 9639 Section 7: Streamable Subset Detection
// ============================================================================

/**
 * @brief Check if stream conforms to streamable subset per RFC 9639 Section 7
 * 
 * Streamable subset requirements:
 * - Frame headers must not reference STREAMINFO for sample rate or bit depth
 * - Maximum block size of 16384 samples
 * - Maximum block size of 4608 samples for streams ≤48000 Hz
 * 
 * @param streaminfo STREAMINFO block to check
 * @param sample_rate_from_frame Sample rate from frame header (0 if from STREAMINFO)
 * @param bit_depth_from_frame Bit depth from frame header (0 if from STREAMINFO)
 * @return true if conforms to streamable subset, false otherwise
 */
bool isStreamableSubset(const FLACStreamInfo& streaminfo, 
                       uint32_t sample_rate_from_frame,
                       uint8_t bit_depth_from_frame);

// ============================================================================
// RFC 9639 Section 9.1.5: UTF-8-like Coded Number Parsing
// ============================================================================

/**
 * @brief Parse UTF-8-like variable-length coded number per RFC 9639 Section 9.1.5
 * 
 * FLAC uses a UTF-8-like encoding for frame/sample numbers:
 * - 1 byte:  0xxxxxxx (0-127)
 * - 2 bytes: 110xxxxx 10xxxxxx
 * - 3 bytes: 1110xxxx 10xxxxxx 10xxxxxx
 * - 4 bytes: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 * - 5 bytes: 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
 * - 6 bytes: 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
 * - 7 bytes: 11111110 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
 * 
 * For fixed block size streams, this represents the frame number.
 * For variable block size streams, this represents the sample number.
 * 
 * @param data Pointer to buffer containing coded number (at least 7 bytes available)
 * @param number Output parameter for decoded number
 * @param bytes_consumed Output parameter for number of bytes consumed (1-7)
 * @return true if successfully parsed, false if invalid encoding
 */
bool parseCodedNumber(const uint8_t* data, uint64_t& number, size_t& bytes_consumed);

/**
 * @brief Parse UTF-8-like coded number with detailed logging
 * 
 * @param data Pointer to buffer containing coded number
 * @param number Output parameter for decoded number
 * @param bytes_consumed Output parameter for number of bytes consumed
 * @param debug_channel Debug channel name for logging
 * @return true if successfully parsed, false if invalid encoding
 */
bool parseCodedNumberWithLogging(const uint8_t* data, uint64_t& number, size_t& bytes_consumed,
                                const char* debug_channel = "flac");

// ============================================================================
// RFC 9639 Sections 9.1.8, 9.3: CRC Validation
// ============================================================================

/**
 * @brief Calculate CRC-8 checksum per RFC 9639 Section 9.1.8
 * 
 * CRC-8 is used for frame header validation with polynomial 0x07 (x^8 + x^2 + x + 1).
 * The CRC is calculated over all frame header bytes except the CRC byte itself.
 * 
 * @param data Pointer to data buffer
 * @param length Number of bytes to process
 * @return Calculated CRC-8 checksum
 */
uint8_t calculateCRC8(const uint8_t* data, size_t length);

/**
 * @brief Calculate CRC-16 checksum per RFC 9639 Section 9.3
 * 
 * CRC-16 is used for frame footer validation with polynomial 0x8005 (x^16 + x^15 + x^2 + 1).
 * The CRC is calculated over the entire frame from sync code through subframes.
 * 
 * @param data Pointer to data buffer
 * @param length Number of bytes to process
 * @return Calculated CRC-16 checksum
 */
uint16_t calculateCRC16(const uint8_t* data, size_t length);

/**
 * @brief Validate frame header CRC-8 per RFC 9639 Section 9.1.8
 * 
 * @param data Pointer to frame header data (excluding CRC byte)
 * @param length Number of bytes in header (excluding CRC byte)
 * @param expected_crc Expected CRC-8 value from frame header
 * @return true if CRC matches, false otherwise
 */
inline bool validateHeaderCRC8(const uint8_t* data, size_t length, uint8_t expected_crc) {
    return calculateCRC8(data, length) == expected_crc;
}

/**
 * @brief Validate frame footer CRC-16 per RFC 9639 Section 9.3
 * 
 * @param data Pointer to complete frame data (excluding CRC-16 bytes)
 * @param length Number of bytes in frame (excluding CRC-16 bytes)
 * @param expected_crc Expected CRC-16 value from frame footer
 * @return true if CRC matches, false otherwise
 */
inline bool validateFrameCRC16(const uint8_t* data, size_t length, uint16_t expected_crc) {
    return calculateCRC16(data, length) == expected_crc;
}

/**
 * @brief Validate frame header CRC-8 with detailed logging
 * 
 * @param data Pointer to frame header data (excluding CRC byte)
 * @param length Number of bytes in header (excluding CRC byte)
 * @param expected_crc Expected CRC-8 value from frame header
 * @param file_offset File offset where frame header starts (for logging)
 * @param debug_channel Debug channel name for logging
 * @return true if CRC matches, false otherwise
 */
bool validateHeaderCRC8WithLogging(const uint8_t* data, size_t length, uint8_t expected_crc,
                                  uint64_t file_offset, const char* debug_channel = "flac");

/**
 * @brief Validate frame footer CRC-16 with detailed logging
 * 
 * @param data Pointer to complete frame data (excluding CRC-16 bytes)
 * @param length Number of bytes in frame (excluding CRC-16 bytes)
 * @param expected_crc Expected CRC-16 value from frame footer
 * @param file_offset File offset where frame starts (for logging)
 * @param debug_channel Debug channel name for logging
 * @return true if CRC matches, false otherwise
 */
bool validateFrameCRC16WithLogging(const uint8_t* data, size_t length, uint16_t expected_crc,
                                  uint64_t file_offset, const char* debug_channel = "flac");

// ============================================================================
// RFC 9639 Section 5, Table 1: Forbidden Pattern Detection
// ============================================================================

/**
 * @brief Forbidden pattern types per RFC 9639 Table 1
 */
enum class ForbiddenPattern {
    METADATA_BLOCK_TYPE_127,        ///< Metadata block type 127
    STREAMINFO_MIN_BLOCK_SIZE_LT_16, ///< STREAMINFO min block size < 16
    STREAMINFO_MAX_BLOCK_SIZE_LT_16, ///< STREAMINFO max block size < 16
    SAMPLE_RATE_BITS_1111,          ///< Sample rate bits 0b1111
    UNCOMMON_BLOCK_SIZE_65536,      ///< Uncommon block size value 65536
    NONE                            ///< No forbidden pattern detected
};

/**
 * @brief Check if metadata block type is forbidden per RFC 9639 Table 1
 * 
 * @param block_type Block type value (0-127)
 * @return METADATA_BLOCK_TYPE_127 if forbidden, NONE otherwise
 */
inline ForbiddenPattern checkForbiddenBlockType(uint8_t block_type) {
    return (block_type == 127) ? ForbiddenPattern::METADATA_BLOCK_TYPE_127 : ForbiddenPattern::NONE;
}

/**
 * @brief Check if STREAMINFO block size is forbidden per RFC 9639 Table 1
 * 
 * @param min_block_size Minimum block size from STREAMINFO
 * @param max_block_size Maximum block size from STREAMINFO
 * @return Forbidden pattern type if detected, NONE otherwise
 */
ForbiddenPattern checkForbiddenBlockSize(uint16_t min_block_size, uint16_t max_block_size);

/**
 * @brief Check if sample rate bits are forbidden per RFC 9639 Table 1
 * 
 * @param sample_rate_bits 4-bit sample rate encoding from frame header
 * @return SAMPLE_RATE_BITS_1111 if forbidden, NONE otherwise
 */
inline ForbiddenPattern checkForbiddenSampleRateBits(uint8_t sample_rate_bits) {
    return (sample_rate_bits == 0x0F) ? ForbiddenPattern::SAMPLE_RATE_BITS_1111 : ForbiddenPattern::NONE;
}

/**
 * @brief Check if uncommon block size is forbidden per RFC 9639 Table 1
 * 
 * @param uncommon_block_size Uncommon block size value (after adding 1)
 * @return UNCOMMON_BLOCK_SIZE_65536 if forbidden, NONE otherwise
 */
inline ForbiddenPattern checkForbiddenUncommonBlockSize(uint32_t uncommon_block_size) {
    return (uncommon_block_size == 65536) ? ForbiddenPattern::UNCOMMON_BLOCK_SIZE_65536 : ForbiddenPattern::NONE;
}

/**
 * @brief Get human-readable description of forbidden pattern
 * 
 * @param pattern Forbidden pattern type
 * @return String describing the forbidden pattern
 */
const char* getForbiddenPatternDescription(ForbiddenPattern pattern);

/**
 * @brief Log forbidden pattern detection with context
 * 
 * @param pattern Forbidden pattern type
 * @param context Additional context information
 * @param debug_channel Debug channel name for logging
 */
void logForbiddenPattern(ForbiddenPattern pattern, const std::string& context,
                        const char* debug_channel = "flac");

} // namespace FLACRFC9639

#endif // FLACRFC9639_H
