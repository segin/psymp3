// MetadataParser.cpp - FLAC metadata block parser implementation
// Implements RFC 9639 metadata parsing

#include "psymp3.h"
#include "codecs/flac/MetadataParser.h"
#include "codecs/flac/BitstreamReader.h"

namespace PsyMP3 {
namespace Codec {
namespace FLAC {

MetadataParser::MetadataParser(BitstreamReader* reader)
    : m_reader(reader) {
}

MetadataParser::~MetadataParser() {
}

/**
 * parseMetadataBlockHeader - Parse metadata block header
 * 
 * Metadata block header format (32 bits):
 * - 1 bit: last-block flag (1 = last metadata block before audio)
 * - 7 bits: block type (0-6 defined, 7-126 reserved, 127 forbidden)
 * - 24 bits: block length in bytes
 * 
 * Requirements: 41
 */
bool MetadataParser::parseMetadataBlockHeader(MetadataType& type, bool& is_last, uint32_t& length) {
    if (!m_reader) {
        return false;
    }
    
    // Read 1-bit last-block flag
    uint32_t last_flag;
    if (!m_reader->readBits(last_flag, 1)) {
        return false;
    }
    is_last = (last_flag == 1);
    
    // Read 7-bit block type
    uint32_t block_type;
    if (!m_reader->readBits(block_type, 7)) {
        return false;
    }
    
    // Reject forbidden type 127
    if (block_type == 127) {
        return false;
    }
    
    type = static_cast<MetadataType>(block_type);
    
    // Read 24-bit block length
    if (!m_reader->readBits(length, 24)) {
        return false;
    }
    
    return true;
}

/**
 * parseStreamInfo - Parse STREAMINFO metadata block
 * 
 * STREAMINFO format (34 bytes):
 * - 16 bits: minimum block size
 * - 16 bits: maximum block size
 * - 24 bits: minimum frame size (0 = unknown)
 * - 24 bits: maximum frame size (0 = unknown)
 * - 20 bits: sample rate
 * - 3 bits: channels - 1
 * - 5 bits: bits per sample - 1
 * - 36 bits: total samples (0 = unknown)
 * - 128 bits: MD5 checksum
 * 
 * Requirements: 42, 58
 */
bool MetadataParser::parseStreamInfo(StreamInfoMetadata& info) {
    if (!m_reader) {
        return false;
    }
    
    // Read 16-bit minimum block size
    uint32_t min_block;
    if (!m_reader->readBits(min_block, 16)) {
        return false;
    }
    info.min_block_size = min_block;
    
    // Read 16-bit maximum block size
    uint32_t max_block;
    if (!m_reader->readBits(max_block, 16)) {
        return false;
    }
    info.max_block_size = max_block;
    
    // Read 24-bit minimum frame size
    uint32_t min_frame;
    if (!m_reader->readBits(min_frame, 24)) {
        return false;
    }
    info.min_frame_size = min_frame;
    
    // Read 24-bit maximum frame size
    uint32_t max_frame;
    if (!m_reader->readBits(max_frame, 24)) {
        return false;
    }
    info.max_frame_size = max_frame;
    
    // Read 20-bit sample rate
    uint32_t sample_rate;
    if (!m_reader->readBits(sample_rate, 20)) {
        return false;
    }
    info.sample_rate = sample_rate;
    
    // Read 3-bit channel count (stored as channels - 1)
    uint32_t channels_minus_1;
    if (!m_reader->readBits(channels_minus_1, 3)) {
        return false;
    }
    info.channels = channels_minus_1 + 1;
    
    // Read 5-bit bits per sample (stored as bps - 1)
    uint32_t bps_minus_1;
    if (!m_reader->readBits(bps_minus_1, 5)) {
        return false;
    }
    info.bits_per_sample = bps_minus_1 + 1;
    
    // Read 36-bit total samples
    // Split into high 4 bits and low 32 bits
    uint32_t total_high;
    if (!m_reader->readBits(total_high, 4)) {
        return false;
    }
    uint32_t total_low;
    if (!m_reader->readBits(total_low, 32)) {
        return false;
    }
    info.total_samples = (static_cast<uint64_t>(total_high) << 32) | total_low;
    
    // Read 128-bit MD5 checksum (16 bytes)
    for (int i = 0; i < 16; i++) {
        uint32_t byte;
        if (!m_reader->readBits(byte, 8)) {
            return false;
        }
        info.md5_sum[i] = static_cast<uint8_t>(byte);
    }
    
    // Validate constraints
    return validateStreamInfo(info);
}

/**
 * validateStreamInfo - Validate STREAMINFO constraints
 * 
 * Requirements: 58
 */
bool MetadataParser::validateStreamInfo(const StreamInfoMetadata& info) const {
    // Minimum block size must be >= 16 (except for last block)
    if (info.min_block_size < 16) {
        return false;
    }
    
    // Maximum block size must be >= 16
    if (info.max_block_size < 16) {
        return false;
    }
    
    // Maximum block size must be <= 65535
    if (info.max_block_size > 65535) {
        return false;
    }
    
    // Minimum must not exceed maximum
    if (info.min_block_size > info.max_block_size) {
        return false;
    }
    
    // Sample rate must be > 0
    if (info.sample_rate == 0) {
        return false;
    }
    
    // Channels must be 1-8
    if (info.channels < 1 || info.channels > 8) {
        return false;
    }
    
    // Bits per sample must be 4-32
    if (info.bits_per_sample < 4 || info.bits_per_sample > 32) {
        return false;
    }
    
    return true;
}

/**
 * parseSeekTable - Parse SEEKTABLE metadata block
 * 
 * Seek table format:
 * - Array of seek points (18 bytes each)
 * - Each seek point:
 *   - 64 bits: sample number (0xFFFFFFFFFFFFFFFF = placeholder)
 *   - 64 bits: byte offset from first frame
 *   - 16 bits: number of samples in target frame
 * 
 * Requirements: 43
 */
bool MetadataParser::parseSeekTable(std::vector<SeekPoint>& points, uint32_t block_length) {
    if (!m_reader) {
        return false;
    }
    
    // Each seek point is 18 bytes
    if (block_length % 18 != 0) {
        return false;
    }
    
    uint32_t num_points = block_length / 18;
    points.clear();
    points.reserve(num_points);
    
    for (uint32_t i = 0; i < num_points; i++) {
        SeekPoint point;
        
        // Read 64-bit sample number (split into two 32-bit reads)
        uint32_t sample_high, sample_low;
        if (!m_reader->readBits(sample_high, 32)) {
            return false;
        }
        if (!m_reader->readBits(sample_low, 32)) {
            return false;
        }
        point.sample_number = (static_cast<uint64_t>(sample_high) << 32) | sample_low;
        
        // Read 64-bit byte offset
        uint32_t offset_high, offset_low;
        if (!m_reader->readBits(offset_high, 32)) {
            return false;
        }
        if (!m_reader->readBits(offset_low, 32)) {
            return false;
        }
        point.byte_offset = (static_cast<uint64_t>(offset_high) << 32) | offset_low;
        
        // Read 16-bit frame sample count
        uint32_t frame_samples;
        if (!m_reader->readBits(frame_samples, 16)) {
            return false;
        }
        point.frame_samples = static_cast<uint16_t>(frame_samples);
        
        points.push_back(point);
    }
    
    // Validate seek table
    return validateSeekTable(points);
}

/**
 * validateSeekTable - Validate seek table constraints
 * 
 * Requirements: 43
 */
bool MetadataParser::validateSeekTable(const std::vector<SeekPoint>& points) const {
    // Verify ascending sample number order (excluding placeholders)
    uint64_t last_sample = 0;
    for (const auto& point : points) {
        if (!point.isPlaceholder()) {
            if (point.sample_number < last_sample) {
                return false;  // Not in ascending order
            }
            last_sample = point.sample_number;
        }
    }
    
    return true;
}

/**
 * parseVorbisComment - Parse VORBIS_COMMENT metadata block
 * 
 * Vorbis comment format (little-endian lengths!):
 * - 32 bits: vendor string length (little-endian)
 * - N bytes: vendor string (UTF-8)
 * - 32 bits: field count (little-endian)
 * - For each field:
 *   - 32 bits: field length (little-endian)
 *   - N bytes: field string "NAME=value" (UTF-8)
 * 
 * Requirements: 44
 */
bool MetadataParser::parseVorbisComment(VorbisComment& comment) {
    if (!m_reader) {
        return false;
    }
    
    // Read 32-bit vendor string length (little-endian)
    uint32_t vendor_length;
    if (!readLittleEndian32(vendor_length)) {
        return false;
    }
    
    // Read vendor string
    if (!readString(comment.vendor_string, vendor_length)) {
        return false;
    }
    
    // Read 32-bit field count (little-endian)
    uint32_t field_count;
    if (!readLittleEndian32(field_count)) {
        return false;
    }
    
    // Sanity check: limit field count to prevent DoS
    if (field_count > 10000) {
        return false;
    }
    
    comment.fields.clear();
    comment.fields.reserve(field_count);
    
    // Parse each field
    for (uint32_t i = 0; i < field_count; i++) {
        VorbisCommentField field;
        if (!parseVorbisCommentField(field)) {
            return false;
        }
        comment.fields.push_back(field);
    }
    
    return true;
}

/**
 * parseVorbisCommentField - Parse single Vorbis comment field
 * 
 * Requirements: 44
 */
bool MetadataParser::parseVorbisCommentField(VorbisCommentField& field) {
    // Read 32-bit field length (little-endian)
    uint32_t field_length;
    if (!readLittleEndian32(field_length)) {
        return false;
    }
    
    // Sanity check: limit field length
    if (field_length > 1000000) {  // 1MB limit
        return false;
    }
    
    // Read field string
    std::string field_str;
    if (!readString(field_str, field_length)) {
        return false;
    }
    
    // Split on '=' to get name and value
    size_t eq_pos = field_str.find('=');
    if (eq_pos == std::string::npos) {
        return false;  // Invalid field format
    }
    
    field.name = field_str.substr(0, eq_pos);
    field.value = field_str.substr(eq_pos + 1);
    
    // Validate field name
    if (!isValidFieldName(field.name)) {
        return false;
    }
    
    return true;
}

/**
 * isValidFieldName - Validate Vorbis comment field name
 * 
 * Field names must be printable ASCII except '='
 * Requirements: 44
 */
bool MetadataParser::isValidFieldName(const std::string& name) const {
    if (name.empty()) {
        return false;
    }
    
    for (char c : name) {
        // Must be printable ASCII (0x20-0x7E) except '=' (0x3D)
        if (c < 0x20 || c > 0x7E || c == '=') {
            return false;
        }
    }
    
    return true;
}

/**
 * findField - Find Vorbis comment field by name (case-insensitive)
 * 
 * Requirements: 44
 */
const VorbisCommentField* VorbisComment::findField(const std::string& name) const {
    for (const auto& field : fields) {
        // Case-insensitive comparison
        if (field.name.size() == name.size()) {
            bool match = true;
            for (size_t i = 0; i < name.size(); i++) {
                char c1 = std::tolower(field.name[i]);
                char c2 = std::tolower(name[i]);
                if (c1 != c2) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return &field;
            }
        }
    }
    return nullptr;
}

/**
 * parsePicture - Parse PICTURE metadata block
 * 
 * Picture format:
 * - 32 bits: picture type
 * - 32 bits: MIME type length
 * - N bytes: MIME type string
 * - 32 bits: description length
 * - N bytes: description string (UTF-8)
 * - 32 bits: width
 * - 32 bits: height
 * - 32 bits: color depth
 * - 32 bits: number of colors (for indexed images)
 * - 32 bits: picture data length
 * - N bytes: picture data
 * 
 * Requirements: 46
 */
bool MetadataParser::parsePicture(PictureMetadata& picture) {
    if (!m_reader) {
        return false;
    }
    
    // Read 32-bit picture type
    if (!m_reader->readBits(picture.picture_type, 32)) {
        return false;
    }
    
    // Read 32-bit MIME type length
    uint32_t mime_length;
    if (!m_reader->readBits(mime_length, 32)) {
        return false;
    }
    
    // Sanity check
    if (mime_length > 1000) {
        return false;
    }
    
    // Read MIME type string
    if (!readString(picture.mime_type, mime_length)) {
        return false;
    }
    
    // Read 32-bit description length
    uint32_t desc_length;
    if (!m_reader->readBits(desc_length, 32)) {
        return false;
    }
    
    // Sanity check
    if (desc_length > 100000) {
        return false;
    }
    
    // Read description string
    if (!readString(picture.description, desc_length)) {
        return false;
    }
    
    // Read 32-bit width
    if (!m_reader->readBits(picture.width, 32)) {
        return false;
    }
    
    // Read 32-bit height
    if (!m_reader->readBits(picture.height, 32)) {
        return false;
    }
    
    // Read 32-bit color depth
    if (!m_reader->readBits(picture.depth, 32)) {
        return false;
    }
    
    // Read 32-bit number of colors
    if (!m_reader->readBits(picture.colors, 32)) {
        return false;
    }
    
    // Read 32-bit picture data length
    uint32_t data_length;
    if (!m_reader->readBits(data_length, 32)) {
        return false;
    }
    
    // Sanity check: limit picture size to 10MB
    if (data_length > 10 * 1024 * 1024) {
        return false;
    }
    
    // Read picture data
    picture.data.clear();
    picture.data.reserve(data_length);
    for (uint32_t i = 0; i < data_length; i++) {
        uint32_t byte;
        if (!m_reader->readBits(byte, 8)) {
            return false;
        }
        picture.data.push_back(static_cast<uint8_t>(byte));
    }
    
    return true;
}

/**
 * parseCuesheet - Parse CUESHEET metadata block
 * 
 * Cuesheet format:
 * - 128 bytes: media catalog number (ASCII)
 * - 64 bits: lead-in sample count
 * - 1 bit: CD flag
 * - 7 + 258*8 bits: reserved (must be 0)
 * - 8 bits: track count
 * - For each track:
 *   - Track data (see parseCuesheetTrack)
 * 
 * Requirements: 47
 */
bool MetadataParser::parseCuesheet(CuesheetMetadata& cuesheet) {
    if (!m_reader) {
        return false;
    }
    
    // Read 128-byte media catalog number
    for (int i = 0; i < 128; i++) {
        uint32_t byte;
        if (!m_reader->readBits(byte, 8)) {
            return false;
        }
        cuesheet.media_catalog_number[i] = static_cast<char>(byte);
    }
    cuesheet.media_catalog_number[128] = '\0';
    
    // Read 64-bit lead-in sample count
    uint32_t lead_in_high, lead_in_low;
    if (!m_reader->readBits(lead_in_high, 32)) {
        return false;
    }
    if (!m_reader->readBits(lead_in_low, 32)) {
        return false;
    }
    cuesheet.lead_in = (static_cast<uint64_t>(lead_in_high) << 32) | lead_in_low;
    
    // Read 1-bit CD flag
    uint32_t cd_flag;
    if (!m_reader->readBits(cd_flag, 1)) {
        return false;
    }
    cuesheet.is_cd = (cd_flag == 1);
    
    // Skip 7 + 258*8 = 2071 reserved bits
    if (!m_reader->skipBits(2071)) {
        return false;
    }
    
    // Read 8-bit track count
    uint32_t track_count;
    if (!m_reader->readBits(track_count, 8)) {
        return false;
    }
    
    // Sanity check
    if (track_count > 100) {
        return false;
    }
    
    cuesheet.tracks.clear();
    cuesheet.tracks.reserve(track_count);
    
    // Parse each track
    for (uint32_t i = 0; i < track_count; i++) {
        CuesheetTrack track;
        if (!parseCuesheetTrack(track)) {
            return false;
        }
        cuesheet.tracks.push_back(track);
    }
    
    return true;
}

/**
 * parseCuesheetTrack - Parse single cuesheet track
 * 
 * Requirements: 47
 */
bool MetadataParser::parseCuesheetTrack(CuesheetTrack& track) {
    // Read 64-bit track offset
    uint32_t offset_high, offset_low;
    if (!m_reader->readBits(offset_high, 32)) {
        return false;
    }
    if (!m_reader->readBits(offset_low, 32)) {
        return false;
    }
    track.offset = (static_cast<uint64_t>(offset_high) << 32) | offset_low;
    
    // Read 8-bit track number
    uint32_t track_num;
    if (!m_reader->readBits(track_num, 8)) {
        return false;
    }
    track.number = static_cast<uint8_t>(track_num);
    
    // Read 12-byte ISRC
    for (int i = 0; i < 12; i++) {
        uint32_t byte;
        if (!m_reader->readBits(byte, 8)) {
            return false;
        }
        track.isrc[i] = static_cast<char>(byte);
    }
    track.isrc[12] = '\0';
    
    // Read 1-bit track type (0 = audio, 1 = non-audio)
    uint32_t type_bit;
    if (!m_reader->readBits(type_bit, 1)) {
        return false;
    }
    track.is_audio = (type_bit == 0);
    
    // Read 1-bit pre-emphasis flag
    uint32_t pre_emph;
    if (!m_reader->readBits(pre_emph, 1)) {
        return false;
    }
    track.pre_emphasis = (pre_emph == 1);
    
    // Skip 6 + 13*8 = 110 reserved bits
    if (!m_reader->skipBits(110)) {
        return false;
    }
    
    // Read 8-bit index point count
    uint32_t index_count;
    if (!m_reader->readBits(index_count, 8)) {
        return false;
    }
    
    // Sanity check
    if (index_count > 100) {
        return false;
    }
    
    track.indices.clear();
    track.indices.reserve(index_count);
    
    // Parse each index point
    for (uint32_t i = 0; i < index_count; i++) {
        CuesheetTrackIndex index;
        if (!parseCuesheetTrackIndex(index)) {
            return false;
        }
        track.indices.push_back(index);
    }
    
    return true;
}

/**
 * parseCuesheetTrackIndex - Parse single cuesheet track index point
 * 
 * Requirements: 47
 */
bool MetadataParser::parseCuesheetTrackIndex(CuesheetTrackIndex& index) {
    // Read 64-bit index offset
    uint32_t offset_high, offset_low;
    if (!m_reader->readBits(offset_high, 32)) {
        return false;
    }
    if (!m_reader->readBits(offset_low, 32)) {
        return false;
    }
    index.offset = (static_cast<uint64_t>(offset_high) << 32) | offset_low;
    
    // Read 8-bit index number
    uint32_t index_num;
    if (!m_reader->readBits(index_num, 8)) {
        return false;
    }
    index.number = static_cast<uint8_t>(index_num);
    
    // Skip 3*8 = 24 reserved bits
    if (!m_reader->skipBits(24)) {
        return false;
    }
    
    return true;
}

/**
 * parseApplication - Parse APPLICATION metadata block
 * 
 * Application format:
 * - 32 bits: application ID
 * - N bytes: application data
 * 
 * Requirements: 45
 */
bool MetadataParser::parseApplication(ApplicationMetadata& app, uint32_t block_length) {
    if (!m_reader) {
        return false;
    }
    
    if (block_length < 4) {
        return false;  // Must have at least 4 bytes for ID
    }
    
    // Read 32-bit application ID
    if (!m_reader->readBits(app.id, 32)) {
        return false;
    }
    
    // Read application data
    uint32_t data_length = block_length - 4;
    app.data.clear();
    app.data.reserve(data_length);
    
    for (uint32_t i = 0; i < data_length; i++) {
        uint32_t byte;
        if (!m_reader->readBits(byte, 8)) {
            return false;
        }
        app.data.push_back(static_cast<uint8_t>(byte));
    }
    
    return true;
}

/**
 * skipPadding - Skip PADDING metadata block
 * 
 * Padding blocks should contain all zeros.
 * Requirements: 45
 */
bool MetadataParser::skipPadding(uint32_t block_length) {
    // Optionally verify padding is all zeros
    return verifyPaddingZeros(block_length);
}

/**
 * skipMetadataBlock - Skip unknown/reserved metadata block
 * 
 * Requirements: 41, 45
 */
bool MetadataParser::skipMetadataBlock(uint32_t block_length) {
    if (!m_reader) {
        return false;
    }
    
    // Skip block_length bytes
    return m_reader->skipBits(block_length * 8);
}

/**
 * verifyPaddingZeros - Verify padding block contains all zeros
 * 
 * Requirements: 45
 */
bool MetadataParser::verifyPaddingZeros(uint32_t byte_count) {
    if (!m_reader) {
        return false;
    }
    
    for (uint32_t i = 0; i < byte_count; i++) {
        uint32_t byte;
        if (!m_reader->readBits(byte, 8)) {
            return false;
        }
        if (byte != 0) {
            // Padding should be all zeros, but we'll be lenient
            // and just skip the rest
            return m_reader->skipBits((byte_count - i - 1) * 8);
        }
    }
    
    return true;
}

/**
 * readString - Read UTF-8 string from bitstream
 */
bool MetadataParser::readString(std::string& str, uint32_t length) {
    if (!m_reader) {
        return false;
    }
    
    str.clear();
    str.reserve(length);
    
    for (uint32_t i = 0; i < length; i++) {
        uint32_t byte;
        if (!m_reader->readBits(byte, 8)) {
            return false;
        }
        str.push_back(static_cast<char>(byte));
    }
    
    return true;
}

/**
 * readLittleEndian32 - Read 32-bit little-endian value
 * 
 * Note: Vorbis comments use little-endian, unlike rest of FLAC
 * Requirements: 44
 */
bool MetadataParser::readLittleEndian32(uint32_t& value) {
    if (!m_reader) {
        return false;
    }
    
    // Read 4 bytes in little-endian order
    uint32_t byte0, byte1, byte2, byte3;
    if (!m_reader->readBits(byte0, 8)) return false;
    if (!m_reader->readBits(byte1, 8)) return false;
    if (!m_reader->readBits(byte2, 8)) return false;
    if (!m_reader->readBits(byte3, 8)) return false;
    
    value = byte0 | (byte1 << 8) | (byte2 << 16) | (byte3 << 24);
    return true;
}

/**
 * parseOggFLACHeader - Parse Ogg FLAC identification header
 * 
 * Ogg FLAC header format:
 * - 5 bytes: "\x7FFLAC" signature
 * - 1 byte: major version (should be 1)
 * - 1 byte: minor version (should be 0)
 * - 2 bytes: number of header packets (big-endian)
 * - Then follows native FLAC metadata blocks starting with "fLaC"
 * 
 * Requirements: 49.2, 49.3, 49.4
 */
bool MetadataParser::parseOggFLACHeader(uint8_t& major_version, uint8_t& minor_version, uint16_t& header_count) {
    if (!m_reader) {
        return false;
    }
    
    // Verify "\x7FFLAC" signature (5 bytes)
    uint32_t byte;
    
    // Read 0x7F
    if (!m_reader->readBits(byte, 8)) return false;
    if (byte != 0x7F) return false;
    
    // Read 'F'
    if (!m_reader->readBits(byte, 8)) return false;
    if (byte != 'F') return false;
    
    // Read 'L'
    if (!m_reader->readBits(byte, 8)) return false;
    if (byte != 'L') return false;
    
    // Read 'A'
    if (!m_reader->readBits(byte, 8)) return false;
    if (byte != 'A') return false;
    
    // Read 'C'
    if (!m_reader->readBits(byte, 8)) return false;
    if (byte != 'C') return false;
    
    // Read major version (1 byte)
    if (!m_reader->readBits(byte, 8)) return false;
    major_version = static_cast<uint8_t>(byte);
    
    // Read minor version (1 byte)
    if (!m_reader->readBits(byte, 8)) return false;
    minor_version = static_cast<uint8_t>(byte);
    
    // Read number of header packets (2 bytes, big-endian)
    uint32_t header_count_high, header_count_low;
    if (!m_reader->readBits(header_count_high, 8)) return false;
    if (!m_reader->readBits(header_count_low, 8)) return false;
    header_count = static_cast<uint16_t>((header_count_high << 8) | header_count_low);
    
    return true;
}

/**
 * skipOggFLACSignature - Skip Ogg FLAC signature to get to native FLAC data
 * 
 * After the Ogg FLAC header (9 bytes), the native FLAC stream marker "fLaC"
 * follows. This method skips the Ogg FLAC signature so that the bitstream
 * reader is positioned at the native FLAC data.
 * 
 * Requirements: 49.5
 */
bool MetadataParser::skipOggFLACSignature() {
    if (!m_reader) {
        return false;
    }
    
    // Skip 9 bytes: "\x7FFLAC" (5) + version (2) + header count (2)
    for (int i = 0; i < 9; i++) {
        uint32_t byte;
        if (!m_reader->readBits(byte, 8)) {
            return false;
        }
    }
    
    return true;
}

} // namespace FLAC
} // namespace Codec
} // namespace PsyMP3
