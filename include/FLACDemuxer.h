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
 * @brief Frame index entry for efficient seeking
 * 
 * This structure stores discovered frame positions during parsing or playback
 * to enable sample-accurate seeking without the limitations of binary search
 * on compressed audio streams.
 */
struct FLACFrameIndexEntry {
    uint64_t sample_offset = 0;      ///< Sample position of this frame in the stream
    uint64_t file_offset = 0;        ///< File position where frame starts
    uint32_t block_size = 0;         ///< Number of samples in this frame
    uint32_t frame_size = 0;         ///< Actual size of frame in bytes (if known)
    
    FLACFrameIndexEntry() = default;
    
    FLACFrameIndexEntry(uint64_t sample, uint64_t offset, uint32_t blocks, uint32_t size = 0)
        : sample_offset(sample), file_offset(offset), block_size(blocks), frame_size(size) {}
    
    /**
     * @brief Check if this index entry is valid
     */
    bool isValid() const {
        return block_size > 0 && file_offset > 0;
    }
    
    /**
     * @brief Get the sample range covered by this frame
     */
    std::pair<uint64_t, uint64_t> getSampleRange() const {
        return {sample_offset, sample_offset + block_size};
    }
    
    /**
     * @brief Check if a target sample falls within this frame
     */
    bool containsSample(uint64_t target_sample) const {
        return target_sample >= sample_offset && target_sample < (sample_offset + block_size);
    }
};

/**
 * @brief Frame index for efficient seeking in FLAC streams
 * 
 * This class maintains a sorted index of discovered frame positions to enable
 * sample-accurate seeking without the architectural limitations of binary search
 * on compressed audio streams.
 */
class FLACFrameIndex {
public:
    // Configuration constants
    static constexpr size_t MAX_INDEX_ENTRIES = 50000;        ///< Maximum index entries to prevent memory exhaustion
    static constexpr size_t INDEX_GRANULARITY_SAMPLES = 44100; ///< Target samples between index entries (1 second at 44.1kHz)
    static constexpr size_t MEMORY_LIMIT_BYTES = 8 * 1024 * 1024; ///< Maximum memory usage for index (8MB)
    
    FLACFrameIndex() = default;
    
    /**
     * @brief Add a frame to the index
     * @param entry Frame index entry to add
     * @return true if added successfully, false if rejected (duplicate, memory limit, etc.)
     */
    bool addFrame(const FLACFrameIndexEntry& entry);
    
    /**
     * @brief Find the best frame index entry for seeking to a target sample
     * @param target_sample Target sample position
     * @return Pointer to best index entry, or nullptr if no suitable entry found
     */
    const FLACFrameIndexEntry* findBestEntry(uint64_t target_sample) const;
    
    /**
     * @brief Find frame index entry that contains the target sample
     * @param target_sample Target sample position
     * @return Pointer to containing index entry, or nullptr if not found
     */
    const FLACFrameIndexEntry* findContainingEntry(uint64_t target_sample) const;
    
    /**
     * @brief Get all index entries (for debugging or analysis)
     */
    const std::vector<FLACFrameIndexEntry>& getEntries() const { return m_entries; }
    
    /**
     * @brief Get number of entries in the index
     */
    size_t size() const { return m_entries.size(); }
    
    /**
     * @brief Check if index is empty
     */
    bool empty() const { return m_entries.empty(); }
    
    /**
     * @brief Clear all index entries
     */
    void clear();
    
    /**
     * @brief Get memory usage of the index in bytes
     */
    size_t getMemoryUsage() const;
    
    /**
     * @brief Check if index should accept more entries based on memory and granularity
     */
    bool shouldAddEntry(const FLACFrameIndexEntry& entry) const;
    
    /**
     * @brief Get coverage statistics for the index
     */
    struct IndexStats {
        uint64_t first_sample = 0;
        uint64_t last_sample = 0;
        uint64_t total_samples_covered = 0;
        double coverage_percentage = 0.0;
        size_t entry_count = 0;
        size_t memory_usage = 0;
    };
    
    IndexStats getStats() const;

private:
    std::vector<FLACFrameIndexEntry> m_entries;  ///< Sorted list of frame index entries
    mutable std::mutex m_mutex;                  ///< Thread safety for index access
    
    /**
     * @brief Ensure entries are sorted by sample offset
     */
    void ensureSorted();
    
    /**
     * @brief Check if we should skip adding this entry due to granularity
     */
    bool checkGranularity(const FLACFrameIndexEntry& entry) const;
};

/**
 * @brief FLAC picture metadata information (memory-optimized)
 */
struct FLACPicture {
    uint32_t picture_type = 0;       ///< Picture type (0=Other, 3=Cover front, etc.)
    std::string mime_type;           ///< MIME type (e.g., "image/jpeg")
    std::string description;         ///< Picture description
    uint32_t width = 0;              ///< Picture width in pixels
    uint32_t height = 0;             ///< Picture height in pixels
    uint32_t color_depth = 0;        ///< Color depth in bits per pixel
    uint32_t colors_used = 0;        ///< Number of colors used (0 for non-indexed)
    
    // Memory optimization: Store picture data location instead of loading it all
    uint64_t data_offset = 0;        ///< File offset where picture data starts
    uint32_t data_size = 0;          ///< Size of picture data in bytes
    mutable std::vector<uint8_t> cached_data; ///< Cached picture data (loaded on demand)
    
    /**
     * @brief Check if picture metadata is valid
     */
    bool isValid() const {
        return !mime_type.empty() && data_size > 0 && width > 0 && height > 0;
    }
    
    /**
     * @brief Get picture data, loading from file if necessary
     * @param handler IOHandler to read data if not cached
     * @return Reference to picture data
     */
    const std::vector<uint8_t>& getData(IOHandler* handler) const;
    
    /**
     * @brief Clear cached picture data to free memory
     */
    void clearCache() const {
        cached_data.clear();
        cached_data.shrink_to_fit();
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
 * - Thread-safe concurrent access for seeking and reading operations
 * 
 * @thread_safety This class is thread-safe. Multiple threads can safely call
 *                public methods concurrently. Internal synchronization uses
 *                the public/private lock pattern to prevent deadlocks.
 * 
 * Lock acquisition order (to prevent deadlocks):
 * 1. m_state_mutex (for container state and position tracking)
 * 2. m_metadata_mutex (for metadata access)
 * 3. IOHandler locks (managed by IOHandler implementation)
 */
class FLACDemuxer : public Demuxer {
public:
    // Memory management constants
    static constexpr size_t MAX_SEEK_TABLE_ENTRIES = 10000;     ///< Maximum seek table entries to prevent memory exhaustion
    static constexpr size_t MAX_VORBIS_COMMENTS = 1000;        ///< Maximum number of Vorbis comments
    static constexpr size_t MAX_COMMENT_LENGTH = 8192;         ///< Maximum length of individual comment
    static constexpr size_t MAX_PICTURES = 50;                 ///< Maximum number of embedded pictures
    static constexpr size_t MAX_PICTURE_SIZE = 16 * 1024 * 1024; ///< Maximum picture size (16MB)
    static constexpr size_t FRAME_BUFFER_SIZE = 64 * 1024;     ///< Frame reading buffer size
    static constexpr size_t SYNC_SEARCH_BUFFER_SIZE = 8192;    ///< Buffer size for frame sync search
    static constexpr size_t MAX_FRAME_SIZE = 1024 * 1024;      ///< Maximum expected frame size (1MB)
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
     * @thread_safety Thread-safe
     */
    bool parseContainer() override;
    
    /**
     * @brief Get information about all streams in the container
     * @return Vector containing single StreamInfo for FLAC audio stream
     * @thread_safety Thread-safe
     */
    std::vector<StreamInfo> getStreams() const override;
    
    /**
     * @brief Get information about a specific stream
     * @param stream_id Stream ID (should be 1 for FLAC)
     * @return StreamInfo for the requested stream
     * @thread_safety Thread-safe
     */
    StreamInfo getStreamInfo(uint32_t stream_id) const override;
    
    /**
     * @brief Read the next chunk of FLAC frame data
     * @return MediaChunk containing complete FLAC frame with headers
     * @thread_safety Thread-safe
     */
    MediaChunk readChunk() override;
    
    /**
     * @brief Read the next chunk from a specific stream
     * @param stream_id Stream ID (should be 1 for FLAC)
     * @return MediaChunk containing FLAC frame data
     * @thread_safety Thread-safe
     */
    MediaChunk readChunk(uint32_t stream_id) override;
    
    /**
     * @brief Seek to a specific time position
     * @param timestamp_ms Target time position in milliseconds
     * @return true if seek was successful, false on error
     * @thread_safety Thread-safe
     */
    bool seekTo(uint64_t timestamp_ms) override;
    
    /**
     * @brief Check if we've reached the end of the FLAC stream
     * @return true if at end of file
     * @thread_safety Thread-safe
     */
    bool isEOF() const override;
    
    /**
     * @brief Get total duration of the FLAC file in milliseconds
     * @return Duration in milliseconds, 0 if unknown
     * @thread_safety Thread-safe
     */
    uint64_t getDuration() const override;
    
    /**
     * @brief Get current position in milliseconds
     * @return Current playback position in milliseconds
     * @thread_safety Thread-safe
     */
    uint64_t getPosition() const override;
    
    /**
     * @brief Get current position in samples
     * @return Current playback position in samples
     * @thread_safety Thread-safe
     */
    uint64_t getCurrentSample() const;
    
    /**
     * @brief Enable or disable frame indexing for efficient seeking
     * @param enable True to enable frame indexing, false to disable
     * @thread_safety Thread-safe
     */
    void setFrameIndexingEnabled(bool enable);
    
    /**
     * @brief Check if frame indexing is enabled
     * @return True if frame indexing is enabled
     * @thread_safety Thread-safe
     */
    bool isFrameIndexingEnabled() const;
    
    /**
     * @brief Get frame index statistics for debugging and analysis
     * @return Frame index statistics structure
     * @thread_safety Thread-safe
     */
    FLACFrameIndex::IndexStats getFrameIndexStats() const;
    
    /**
     * @brief Trigger initial frame indexing (if not already done)
     * @return True if indexing completed successfully
     * @thread_safety Thread-safe
     */
    bool buildFrameIndex();

private:
    // Thread safety - Lock acquisition order documented above
    mutable std::mutex m_state_mutex;    ///< Protects container state and position tracking
    mutable std::mutex m_metadata_mutex; ///< Protects metadata access
    mutable std::atomic<bool> m_error_state{false}; ///< Thread-safe error state flag
    
    // FLAC container state (protected by m_state_mutex)
    bool m_container_parsed = false;     ///< True if container has been successfully parsed
    uint64_t m_file_size = 0;           ///< Total file size in bytes
    uint64_t m_audio_data_offset = 0;   ///< File offset where FLAC frames start
    uint64_t m_current_offset = 0;      ///< Current read position in file
    
    // FLAC metadata (protected by m_metadata_mutex)
    FLACStreamInfo m_streaminfo;                           ///< STREAMINFO block data
    std::vector<FLACSeekPoint> m_seektable;               ///< SEEKTABLE entries
    std::map<std::string, std::string> m_vorbis_comments; ///< Vorbis comments metadata
    std::vector<FLACPicture> m_pictures;                  ///< Embedded pictures
    
    // Frame parsing state (protected by m_state_mutex)
    std::atomic<uint64_t> m_current_sample{0};      ///< Current sample position in stream (atomic for fast reads)
    uint32_t m_last_block_size = 0;                 ///< Block size of last parsed frame
    
    // Memory management state (protected by m_state_mutex)
    mutable std::vector<uint8_t> m_frame_buffer;     ///< Reusable buffer for frame reading
    mutable std::vector<uint8_t> m_sync_buffer;      ///< Reusable buffer for sync search
    size_t m_memory_usage_bytes = 0;                 ///< Current memory usage estimate
    
    // Performance optimization state (protected by m_state_mutex)
    bool m_seek_table_sorted = false;                ///< True if seek table is sorted for binary search
    uint64_t m_last_seek_position = 0;               ///< Last successful seek position for optimization
    bool m_is_network_stream = false;                ///< True if this appears to be a network stream
    mutable std::vector<uint8_t> m_readahead_buffer; ///< Read-ahead buffer for network streams
    
    // Frame indexing system (protected by m_state_mutex)
    FLACFrameIndex m_frame_index;                    ///< Frame index for efficient seeking
    bool m_frame_indexing_enabled = true;            ///< True if frame indexing is enabled
    bool m_initial_indexing_complete = false;        ///< True if initial parsing-based indexing is complete
    size_t m_frames_indexed_during_parsing = 0;      ///< Number of frames indexed during initial parsing
    size_t m_frames_indexed_during_playback = 0;     ///< Number of frames indexed during playback
    
    // Thread-safe public method implementations (assume locks are held)
    bool parseContainer_unlocked();
    std::vector<StreamInfo> getStreams_unlocked() const;
    StreamInfo getStreamInfo_unlocked(uint32_t stream_id) const;
    MediaChunk readChunk_unlocked();
    MediaChunk readChunk_unlocked(uint32_t stream_id);
    bool seekTo_unlocked(uint64_t timestamp_ms);
    bool isEOF_unlocked() const;
    uint64_t getDuration_unlocked() const;
    uint64_t getPosition_unlocked() const;
    uint64_t getCurrentSample_unlocked() const;
    
    // Private helper methods (assume appropriate locks are held)
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
    bool validateFrameHeaderAt(uint64_t file_offset);
    uint32_t calculateFrameSize(const FLACFrame& frame);
    
    bool readFrameData(const FLACFrame& frame, std::vector<uint8_t>& data);
    void resetPositionTracking();
    void updatePositionTracking(uint64_t sample_position, uint64_t file_offset);
    
    bool seekWithTable(uint64_t target_sample);
    
    /**
     * @brief Frame index-based seeking (preferred method for accurate seeking)
     * 
     * Uses the frame index built during parsing or playback to provide sample-accurate
     * seeking without the architectural limitations of binary search on compressed streams.
     * 
     * @param target_sample Target sample position to seek to
     * @return true if successful, false if index insufficient or I/O error
     */
    bool seekWithIndex(uint64_t target_sample);
    
    /**
     * @brief Binary search seeking with architectural limitations acknowledgment
     * 
     * ARCHITECTURAL LIMITATION: Binary search is fundamentally incompatible with 
     * compressed audio streams due to variable-length frame encoding.
     * 
     * PROBLEM: Cannot predict frame positions in variable-length compressed data.
     * FLAC frames have unpredictable sizes based on audio content and compression.
     * 
     * CURRENT APPROACH: Attempts binary search but expects frequent failures.
     * FALLBACK STRATEGY: Returns to beginning position when search fails.
     * FUTURE SOLUTION: Frame indexing during initial parsing for accurate seeking.
     * 
     * @param target_sample Target sample position to seek to
     * @return true if successful or acceptable approximation found, false if fallback used
     */
    bool seekBinary(uint64_t target_sample);
    
    bool seekLinear(uint64_t target_sample);
    
    uint64_t samplesToMs(uint64_t samples) const;
    uint64_t msToSamples(uint64_t ms) const;
    
    // Thread safety helper methods
    void setErrorState(bool error_state);
    bool getErrorState() const;
    
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
    
    // Memory management methods
    void initializeBuffers();
    void optimizeSeekTable();
    void limitVorbisComments();
    void limitPictureStorage();
    size_t calculateMemoryUsage() const;
    void freeUnusedMemory();
    bool ensureBufferCapacity(std::vector<uint8_t>& buffer, size_t required_size) const;
    
    // Performance optimization methods
    size_t findSeekPointIndex(uint64_t target_sample) const;
    bool optimizedFrameSync(uint64_t start_offset, FLACFrame& frame);
    void prefetchNextFrame();
    bool isNetworkStream() const;
    void optimizeForNetworkStreaming();
    
    // Frame indexing methods
    bool performInitialFrameIndexing();
    void addFrameToIndex(const FLACFrame& frame);
    void addFrameToIndex(uint64_t sample_offset, uint64_t file_offset, uint32_t block_size, uint32_t frame_size = 0);
    void enableFrameIndexing(bool enable);
    void clearFrameIndex();
};

#endif // FLACDEMUXER_H