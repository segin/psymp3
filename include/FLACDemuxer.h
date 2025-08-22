/*
 * FLACDemuxer.h - FLAC container demuxer
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

#ifndef FLACDEMUXER_H
#define FLACDEMUXER_H

// No direct includes - all includes should be in psymp3.h

/**
 * @brief FLAC metadata block types as defined in FLAC specification
 */
enum class FLACMetadataType : uint8_t {
    STREAMINFO = 0,      ///< Stream information (mandatory, always first)
    PADDING = 1,         ///< Padding block for future metadata
    APPLICATION = 2,     ///< Application-specific data
    SEEKTABLE = 3,       ///< Seek table for efficient seeking
    VORBIS_COMMENT = 4,  ///< Vorbis-style comments (metadata)
    CUESHEET = 5,        ///< Cue sheet for CD-like track information
    PICTURE = 6,         ///< Embedded picture/artwork
    INVALID = 127        ///< Invalid/unknown block type
};

/**
 * @brief FLAC metadata block header and data information
 */
struct FLACMetadataBlock {
    FLACMetadataType type = FLACMetadataType::INVALID;  ///< Block type
    bool is_last = false;                               ///< True if this is the last metadata block
    uint32_t length = 0;                               ///< Length of block data in bytes
    uint64_t data_offset = 0;                          ///< File offset where block data starts
    
    FLACMetadataBlock() = default;
    
    FLACMetadataBlock(FLACMetadataType t, bool last, uint32_t len, uint64_t offset)
        : type(t), is_last(last), length(len), data_offset(offset) {}
    
    /**
     * @brief Check if this is a valid metadata block
     */
    bool isValid() const {
        return type != FLACMetadataType::INVALID && length > 0;
    }
};

/**
 * @brief FLAC STREAMINFO block data (mandatory first metadata block)
 */
struct FLACStreamInfo {
    uint16_t min_block_size = 0;     ///< Minimum block size in samples
    uint16_t max_block_size = 0;     ///< Maximum block size in samples
    uint32_t min_frame_size = 0;     ///< Minimum frame size in bytes (0 if unknown)
    uint32_t max_frame_size = 0;     ///< Maximum frame size in bytes (0 if unknown)
    uint32_t sample_rate = 0;        ///< Sample rate in Hz
    uint8_t channels = 0;            ///< Number of channels (1-8)
    uint8_t bits_per_sample = 0;     ///< Bits per sample (4-32)
    uint64_t total_samples = 0;      ///< Total samples in stream (0 if unknown)
    uint8_t md5_signature[16];       ///< MD5 signature of uncompressed audio data
    
    FLACStreamInfo() {
        std::memset(md5_signature, 0, sizeof(md5_signature));
    }
    
    /**
     * @brief Check if STREAMINFO contains valid data
     */
    bool isValid() const {
        return sample_rate > 0 && channels > 0 && channels <= 8 && 
               bits_per_sample >= 4 && bits_per_sample <= 32 &&
               min_block_size > 0 && max_block_size >= min_block_size;
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
 * @brief FLAC seek point entry from SEEKTABLE metadata block
 */
struct FLACSeekPoint {
    uint64_t sample_number = 0;      ///< Sample number of first sample in target frame
    uint64_t stream_offset = 0;      ///< Offset from first frame to target frame
    uint16_t frame_samples = 0;      ///< Number of samples in target frame
    
    FLACSeekPoint() = default;
    
    FLACSeekPoint(uint64_t sample, uint64_t offset, uint16_t samples)
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
 * @brief FLAC frame header information for streaming
 */
struct FLACFrame {
    uint64_t sample_offset = 0;      ///< Sample position of this frame in the stream
    uint64_t file_offset = 0;        ///< File position where frame starts
    uint32_t block_size = 0;         ///< Number of samples in this frame
    uint32_t frame_size = 0;         ///< Size of frame in bytes (estimated or actual)
    uint32_t sample_rate = 0;        ///< Sample rate for this frame (may vary)
    uint8_t channels = 0;            ///< Channel assignment for this frame
    uint8_t bits_per_sample = 0;     ///< Bits per sample for this frame
    bool variable_block_size = false; ///< True if using variable block size strategy
    
    FLACFrame() = default;
    
    /**
     * @brief Check if frame header contains valid data
     */
    bool isValid() const {
        return block_size > 0 && sample_rate > 0 && channels > 0 && 
               bits_per_sample >= 4 && bits_per_sample <= 32;
    }
    
    /**
     * @brief Calculate frame duration in milliseconds
     */
    uint64_t getDurationMs() const {
        if (sample_rate == 0 || block_size == 0) return 0;
        return (static_cast<uint64_t>(block_size) * 1000) / sample_rate;
    }
};

/**
 * @brief FLAC picture metadata information
 */
struct FLACPicture {
    uint32_t picture_type = 0;       ///< Picture type (0=Other, 3=Cover front, etc.)
    std::string mime_type;           ///< MIME type (e.g., "image/jpeg")
    std::string description;         ///< Picture description
    uint32_t width = 0;              ///< Picture width in pixels
    uint32_t height = 0;             ///< Picture height in pixels
    uint32_t color_depth = 0;        ///< Color depth in bits per pixel
    uint32_t colors_used = 0;        ///< Number of colors used (0 for non-indexed)
    std::vector<uint8_t> data;       ///< Picture data
    
    /**
     * @brief Check if picture metadata is valid
     */
    bool isValid() const {
        return !mime_type.empty() && !data.empty() && width > 0 && height > 0;
    }
};

/**
 * @brief FLAC container demuxer implementation
 * 
 * This demuxer handles native FLAC files (.flac) by parsing the FLAC container
 * format and extracting FLAC bitstream data for decoding. It works in conjunction
 * with a separate FLACCodec to provide container-agnostic FLAC decoding.
 * 
 * The demuxer supports:
 * - Native FLAC container format parsing
 * - FLAC metadata blocks (STREAMINFO, VORBIS_COMMENT, SEEKTABLE, etc.)
 * - Efficient seeking using FLAC's built-in seek table or sample-accurate seeking
 * - Large file support for high-resolution and long-duration FLAC files
 * - Integration with the modern demuxer/codec architecture
 * 
 * @thread_safety Individual instances are NOT thread-safe. Use separate instances
 *                for concurrent access or implement external synchronization.
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
    
    // Demuxer interface implementation
    
    /**
     * @brief Parse FLAC container headers and identify streams
     * @return true if container was successfully parsed, false on error
     */
    bool parseContainer() override;
    
    /**
     * @brief Get information about all streams in the container
     * @return Vector containing single StreamInfo for FLAC audio stream
     */
    std::vector<StreamInfo> getStreams() const override;
    
    /**
     * @brief Get information about a specific stream
     * @param stream_id Stream ID (should be 1 for FLAC)
     * @return StreamInfo for the requested stream
     */
    StreamInfo getStreamInfo(uint32_t stream_id) const override;
    
    /**
     * @brief Read the next chunk of FLAC frame data
     * @return MediaChunk containing complete FLAC frame with headers
     */
    MediaChunk readChunk() override;
    
    /**
     * @brief Read the next chunk from a specific stream
     * @param stream_id Stream ID (should be 1 for FLAC)
     * @return MediaChunk containing FLAC frame data
     */
    MediaChunk readChunk(uint32_t stream_id) override;
    
    /**
     * @brief Seek to a specific time position
     * @param timestamp_ms Target time position in milliseconds
     * @return true if seek was successful, false on error
     */
    bool seekTo(uint64_t timestamp_ms) override;
    
    /**
     * @brief Check if we've reached the end of the FLAC stream
     * @return true if at end of file
     */
    bool isEOF() const override;
    
    /**
     * @brief Get total duration of the FLAC file in milliseconds
     * @return Duration in milliseconds, 0 if unknown
     */
    uint64_t getDuration() const override;
    
    /**
     * @brief Get current position in milliseconds
     * @return Current playback position in milliseconds
     */
    uint64_t getPosition() const override;
    
    /**
     * @brief Get current position in samples
     * @return Current playback position in samples
     */
    uint64_t getCurrentSample() const;

private:
    // FLAC container state
    bool m_container_parsed = false;     ///< True if container has been successfully parsed
    uint64_t m_file_size = 0;           ///< Total file size in bytes
    uint64_t m_audio_data_offset = 0;   ///< File offset where FLAC frames start
    uint64_t m_current_offset = 0;      ///< Current read position in file
    
    // FLAC metadata
    FLACStreamInfo m_streaminfo;                           ///< STREAMINFO block data
    std::vector<FLACSeekPoint> m_seektable;               ///< SEEKTABLE entries
    std::map<std::string, std::string> m_vorbis_comments; ///< Vorbis comments metadata
    std::vector<FLACPicture> m_pictures;                  ///< Embedded pictures
    
    // Frame parsing state
    uint64_t m_current_sample = 0;      ///< Current sample position in stream
    uint32_t m_last_block_size = 0;     ///< Block size of last parsed frame
    
    // Private helper methods (to be implemented)
    bool parseMetadataBlocks();
    bool parseMetadataBlockHeader(FLACMetadataBlock& block);
    bool parseStreamInfoBlock(const FLACMetadataBlock& block);
    bool parseSeekTableBlock(const FLACMetadataBlock& block);
    bool parseVorbisCommentBlock(const FLACMetadataBlock& block);
    bool parsePictureBlock(const FLACMetadataBlock& block);
    bool skipMetadataBlock(const FLACMetadataBlock& block);
    
    bool findNextFrame(FLACFrame& frame);
    bool parseFrameHeader(FLACFrame& frame);
    bool validateFrameHeader(const FLACFrame& frame);
    uint32_t calculateFrameSize(const FLACFrame& frame);
    
    bool readFrameData(const FLACFrame& frame, std::vector<uint8_t>& data);
    void resetPositionTracking();
    void updatePositionTracking(uint64_t sample_position, uint64_t file_offset);
    
    bool seekWithTable(uint64_t target_sample);
    bool seekBinary(uint64_t target_sample);
    bool seekLinear(uint64_t target_sample);
    
    uint64_t samplesToMs(uint64_t samples) const;
    uint64_t msToSamples(uint64_t ms) const;
    
    // Error handling and recovery methods
    bool attemptStreamInfoRecovery();
    bool validateStreamInfoParameters() const;
    bool recoverFromCorruptedMetadata();
    bool resynchronizeToNextFrame();
    void provideDefaultStreamInfo();
    
    // Frame-level error recovery methods
    bool handleLostFrameSync();
    bool skipCorruptedFrame();
    bool validateFrameCRC(const FLACFrame& frame, const std::vector<uint8_t>& frame_data);
    MediaChunk createSilenceChunk(uint32_t block_size);
    bool recoverFromFrameError();
};

#endif // FLACDEMUXER_H