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
    
    FLACFrame() = default;
    
    bool isValid() const {
        return block_size > 0 && sample_rate > 0 && 
               channels > 0 && bits_per_sample >= 4;
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
    
    uint64_t samplesToMs(uint64_t samples) const;
    uint64_t msToSamples(uint64_t ms) const;
};

} // namespace FLAC
} // namespace Demuxer
} // namespace PsyMP3

#endif // FLACDEMUXER_H
