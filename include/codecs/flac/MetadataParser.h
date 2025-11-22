#ifndef METADATAPARSER_H
#define METADATAPARSER_H

#include <cstdint>
#include <vector>
#include <string>

namespace PsyMP3 {
namespace Codec {
namespace FLAC {

// Forward declaration
class BitstreamReader;

/**
 * MetadataType - FLAC metadata block types per RFC 9639
 */
enum class MetadataType : uint8_t {
    STREAMINFO = 0,
    PADDING = 1,
    APPLICATION = 2,
    SEEKTABLE = 3,
    VORBIS_COMMENT = 4,
    CUESHEET = 5,
    PICTURE = 6,
    RESERVED_START = 7,
    RESERVED_END = 126,
    FORBIDDEN = 127
};

/**
 * StreamInfoMetadata - STREAMINFO block (mandatory first metadata block)
 * 
 * Contains essential stream parameters:
 * - Block size range (min/max)
 * - Frame size range (min/max, 0 = unknown)
 * - Sample rate (1-1048575 Hz)
 * - Channel count (1-8)
 * - Bits per sample (4-32)
 * - Total samples (0 = unknown)
 * - MD5 checksum of decoded audio
 */
struct StreamInfoMetadata {
    uint32_t min_block_size;      // 16-bit: minimum block size in samples
    uint32_t max_block_size;      // 16-bit: maximum block size in samples
    uint32_t min_frame_size;      // 24-bit: minimum frame size in bytes (0 = unknown)
    uint32_t max_frame_size;      // 24-bit: maximum frame size in bytes (0 = unknown)
    uint32_t sample_rate;         // 20-bit: sample rate in Hz
    uint32_t channels;            // 3-bit: channel count (1-8)
    uint32_t bits_per_sample;     // 5-bit: bits per sample (4-32)
    uint64_t total_samples;       // 36-bit: total samples in stream (0 = unknown)
    uint8_t md5_sum[16];          // 128-bit: MD5 checksum of decoded audio
    
    StreamInfoMetadata() 
        : min_block_size(0), max_block_size(0),
          min_frame_size(0), max_frame_size(0),
          sample_rate(0), channels(0), bits_per_sample(0),
          total_samples(0) {
        for (int i = 0; i < 16; i++) md5_sum[i] = 0;
    }
};

/**
 * SeekPoint - Single entry in seek table
 * 
 * Provides fast seeking to specific sample positions:
 * - sample_number: target sample number (0xFFFFFFFFFFFFFFFF = placeholder)
 * - byte_offset: byte offset from first frame
 * - frame_samples: number of samples in target frame
 */
struct SeekPoint {
    uint64_t sample_number;       // 64-bit: sample number of first sample in target frame
    uint64_t byte_offset;         // 64-bit: byte offset from first frame
    uint16_t frame_samples;       // 16-bit: number of samples in target frame
    
    SeekPoint() : sample_number(0), byte_offset(0), frame_samples(0) {}
    
    bool isPlaceholder() const {
        return sample_number == 0xFFFFFFFFFFFFFFFFULL;
    }
};

/**
 * VorbisCommentField - Single field in Vorbis comment
 * 
 * Format: "FIELDNAME=value" (UTF-8)
 * Field names are case-insensitive, printable ASCII except '='
 */
struct VorbisCommentField {
    std::string name;             // Field name (e.g., "TITLE", "ARTIST")
    std::string value;            // Field value (UTF-8)
    
    VorbisCommentField() {}
    VorbisCommentField(const std::string& n, const std::string& v) 
        : name(n), value(v) {}
};

/**
 * VorbisComment - Vorbis comment metadata block
 * 
 * Contains textual metadata tags:
 * - vendor_string: encoder identification
 * - fields: array of name=value pairs
 */
struct VorbisComment {
    std::string vendor_string;    // Vendor/encoder identification
    std::vector<VorbisCommentField> fields;  // Metadata fields
    
    VorbisComment() {}
    
    // Helper to find field by name (case-insensitive)
    const VorbisCommentField* findField(const std::string& name) const;
};

/**
 * PictureMetadata - Picture metadata block (album art)
 * 
 * Contains embedded image data with metadata:
 * - picture_type: type of picture (cover, icon, etc.)
 * - mime_type: MIME type (e.g., "image/jpeg")
 * - description: UTF-8 description
 * - width, height: image dimensions
 * - depth: color depth
 * - colors: number of colors (for indexed images)
 * - data: raw image data
 */
struct PictureMetadata {
    uint32_t picture_type;        // Picture type (0-20, see RFC 9639)
    std::string mime_type;        // MIME type string
    std::string description;      // UTF-8 description
    uint32_t width;               // Image width in pixels
    uint32_t height;              // Image height in pixels
    uint32_t depth;               // Color depth in bits
    uint32_t colors;              // Number of colors (for indexed images)
    std::vector<uint8_t> data;    // Picture data
    
    PictureMetadata() 
        : picture_type(0), width(0), height(0), depth(0), colors(0) {}
};

/**
 * CuesheetTrackIndex - Index point within cuesheet track
 */
struct CuesheetTrackIndex {
    uint64_t offset;              // Offset in samples from track start
    uint8_t number;               // Index point number
    
    CuesheetTrackIndex() : offset(0), number(0) {}
};

/**
 * CuesheetTrack - Track entry in cuesheet
 */
struct CuesheetTrack {
    uint64_t offset;              // Track offset in samples
    uint8_t number;               // Track number (1-99, 170 = lead-out)
    char isrc[13];                // ISRC code (12 chars + null terminator)
    bool is_audio;                // True if audio track
    bool pre_emphasis;            // True if pre-emphasis flag set
    std::vector<CuesheetTrackIndex> indices;  // Index points
    
    CuesheetTrack() : offset(0), number(0), is_audio(true), pre_emphasis(false) {
        isrc[0] = '\0';
    }
};

/**
 * CuesheetMetadata - Cuesheet metadata block
 * 
 * Contains CD table of contents information:
 * - media_catalog_number: UPC/EAN code
 * - lead_in: lead-in sample count
 * - is_cd: true if CD cuesheet
 * - tracks: array of track entries
 */
struct CuesheetMetadata {
    char media_catalog_number[129];  // 128 chars + null terminator
    uint64_t lead_in;             // Lead-in sample count
    bool is_cd;                   // True if CD cuesheet
    std::vector<CuesheetTrack> tracks;  // Track entries
    
    CuesheetMetadata() : lead_in(0), is_cd(false) {
        media_catalog_number[0] = '\0';
    }
};

/**
 * ApplicationMetadata - Application-specific metadata block
 */
struct ApplicationMetadata {
    uint32_t id;                  // Application ID (32-bit)
    std::vector<uint8_t> data;    // Application-specific data
    
    ApplicationMetadata() : id(0) {}
};

/**
 * MetadataParser - Parses FLAC metadata blocks
 * 
 * Handles all metadata block types defined in RFC 9639:
 * - STREAMINFO (mandatory)
 * - PADDING
 * - APPLICATION
 * - SEEKTABLE
 * - VORBIS_COMMENT
 * - CUESHEET
 * - PICTURE
 * 
 * Usage:
 *   MetadataParser parser(bitstream_reader);
 *   MetadataType type;
 *   bool is_last;
 *   if (parser.parseMetadataBlockHeader(type, is_last)) {
 *       if (type == MetadataType::STREAMINFO) {
 *           StreamInfoMetadata info;
 *           parser.parseStreamInfo(info);
 *       }
 *   }
 */
class MetadataParser {
public:
    explicit MetadataParser(BitstreamReader* reader);
    ~MetadataParser();
    
    // Metadata block header parsing
    bool parseMetadataBlockHeader(MetadataType& type, bool& is_last, uint32_t& length);
    
    // Specific metadata block parsers
    bool parseStreamInfo(StreamInfoMetadata& info);
    bool parseSeekTable(std::vector<SeekPoint>& points, uint32_t block_length);
    bool parseVorbisComment(VorbisComment& comment);
    bool parsePicture(PictureMetadata& picture);
    bool parseCuesheet(CuesheetMetadata& cuesheet);
    bool parseApplication(ApplicationMetadata& app, uint32_t block_length);
    
    // Skip/padding handlers
    bool skipPadding(uint32_t block_length);
    bool skipMetadataBlock(uint32_t block_length);
    
    // Validation
    bool validateStreamInfo(const StreamInfoMetadata& info) const;
    bool validateSeekTable(const std::vector<SeekPoint>& points) const;
    
    // Ogg FLAC support (Requirement 49)
    bool parseOggFLACHeader(uint8_t& major_version, uint8_t& minor_version, uint16_t& header_count);
    bool skipOggFLACSignature();
    
private:
    BitstreamReader* m_reader;
    
    // Helper methods
    bool readString(std::string& str, uint32_t length);
    bool readLittleEndian32(uint32_t& value);
    bool verifyPaddingZeros(uint32_t byte_count);
    
    // Vorbis comment helpers
    bool parseVorbisCommentField(VorbisCommentField& field);
    bool isValidFieldName(const std::string& name) const;
    
    // Cuesheet helpers
    bool parseCuesheetTrack(CuesheetTrack& track);
    bool parseCuesheetTrackIndex(CuesheetTrackIndex& index);
};

} // namespace FLAC
} // namespace Codec
} // namespace PsyMP3

#endif // METADATAPARSER_H
