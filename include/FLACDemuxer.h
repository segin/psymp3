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

// Forward declarations
class ErrorRecoveryManager;

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
    // CRC validation modes for RFC 9639 compliance
    enum class CRCValidationMode {
        DISABLED,    ///< No CRC validation (maximum performance)
        ENABLED,     ///< CRC validation with error tolerance
        STRICT_MODE  ///< Strict CRC validation - reject frames with CRC errors
    };
    
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
    
    /**
     * @brief Set CRC validation mode for RFC 9639 compliance
     * @param mode CRC validation mode (DISABLED/ENABLED/STRICT)
     * @thread_safety Thread-safe
     */
    void setCRCValidationMode(CRCValidationMode mode);
    
    /**
     * @brief Get current CRC validation mode
     * @return Current CRC validation mode
     * @thread_safety Thread-safe
     */
    CRCValidationMode getCRCValidationMode() const;
    
    /**
     * @brief Set CRC error threshold for automatic validation disabling
     * @param threshold Maximum CRC errors before disabling validation (0 = never disable)
     * @thread_safety Thread-safe
     */
    void setCRCErrorThreshold(size_t threshold);
    
    /**
     * @brief Get CRC validation statistics
     * @return Structure containing CRC error counts and status
     * @thread_safety Thread-safe
     */
    struct CRCValidationStats {
        size_t crc8_errors = 0;
        size_t crc16_errors = 0;
        size_t total_errors = 0;
        bool validation_disabled_due_to_errors = false;
        CRCValidationMode current_mode = CRCValidationMode::ENABLED;
    };
    
    CRCValidationStats getCRCValidationStats() const;
    
    /**
     * @brief Reset CRC validation statistics and re-enable validation
     * @thread_safety Thread-safe
     */
    void resetCRCValidationStats();
    
    // RFC 9639 Streamable Subset Configuration (Section 7)
    
    /**
     * @brief Streamable subset validation modes per RFC 9639 Section 7
     */
    enum class StreamableSubsetMode {
        DISABLED,   ///< No streamable subset validation
        ENABLED,    ///< Validate streamable subset constraints with warnings
        STRICT_MODE ///< Strict streamable subset validation with errors
    };
    
    /**
     * @brief Set streamable subset validation mode per RFC 9639 Section 7
     * @param mode Streamable subset validation mode
     * @thread_safety Thread-safe
     * 
     * RFC 9639 STREAMABLE SUBSET CONSTRAINTS:
     * - Frame headers must not reference STREAMINFO for sample rate or bit depth
     * - Maximum block size of 16384 samples
     * - Maximum block size of 4608 samples for streams ≤48000 Hz
     * - Sample rate and bit depth must be encoded in frame headers
     */
    void setStreamableSubsetMode(StreamableSubsetMode mode);
    
    /**
     * @brief Get current streamable subset validation mode
     * @return Current streamable subset validation mode
     * @thread_safety Thread-safe
     */
    StreamableSubsetMode getStreamableSubsetMode() const;
    
    /**
     * @brief Get streamable subset validation statistics
     * @return Structure containing streamable subset validation statistics
     * @thread_safety Thread-safe
     */
    struct StreamableSubsetStats {
        size_t frames_validated = 0;
        size_t violations_detected = 0;
        size_t block_size_violations = 0;
        size_t frame_header_dependency_violations = 0;
        size_t sample_rate_encoding_violations = 0;
        size_t bit_depth_encoding_violations = 0;
        StreamableSubsetMode current_mode = StreamableSubsetMode::ENABLED;
        
        double getViolationRate() const {
            if (frames_validated == 0) return 0.0;
            return (static_cast<double>(violations_detected) / frames_validated) * 100.0;
        }
    };
    
    StreamableSubsetStats getStreamableSubsetStats() const;
    
    /**
     * @brief Reset streamable subset validation statistics
     * @thread_safety Thread-safe
     */
    void resetStreamableSubsetStats();
    
    /**
     * @brief Get error recovery manager for advanced error handling
     * @return Reference to error recovery manager
     * @thread_safety Thread-safe
     */
    ErrorRecoveryManager& getErrorRecoveryManager();
    
    /**
     * @brief Get error recovery manager (const version)
     * @return Const reference to error recovery manager
     * @thread_safety Thread-safe
     */
    const ErrorRecoveryManager& getErrorRecoveryManager() const;
    
    /**
     * @brief Recover from lost sync pattern (public interface)
     * @param search_start_offset File offset to start sync search
     * @param recovered_offset Output parameter for recovered sync offset
     * @return true if sync pattern was found and position recovered
     * @thread_safety Thread-safe
     */
    bool recoverFromLostSync(uint64_t search_start_offset, uint64_t& recovered_offset);
    
    /**
     * @brief Get sync loss recovery statistics
     * @return Structure containing sync recovery statistics
     * @thread_safety Thread-safe
     */
    struct SyncRecoveryStats {
        size_t sync_loss_count = 0;
        size_t sync_recovery_successes = 0;
        size_t sync_recovery_failures = 0;
        size_t total_sync_search_bytes = 0;
        uint64_t last_sync_recovery_offset = 0;
        
        double getSuccessRate() const {
            if (sync_loss_count == 0) return 0.0;
            return (static_cast<double>(sync_recovery_successes) / sync_loss_count) * 100.0;
        }
    };
    
    SyncRecoveryStats getSyncRecoveryStats() const;
    
    /**
     * @brief Detect and recover from frame corruption
     * @param frame Frame to check for corruption
     * @param frame_data Frame data for validation
     * @return true if frame is valid or corruption was successfully handled
     * @thread_safety Thread-safe
     */
    bool detectAndRecoverFromFrameCorruption(const FLACFrame& frame, const std::vector<uint8_t>& frame_data);
    
    /**
     * @brief Detect and recover from metadata corruption
     * @param block Metadata block to check for corruption
     * @return true if block is valid or corruption was successfully handled
     * @thread_safety Thread-safe
     */
    bool detectAndRecoverFromMetadataCorruption(const FLACMetadataBlock& block);
    
    /**
     * @brief Get corruption detection and recovery statistics
     * @return Structure containing corruption recovery statistics
     * @thread_safety Thread-safe
     */
    struct CorruptionRecoveryStats {
        size_t frame_corruption_count = 0;
        size_t metadata_corruption_count = 0;
        size_t frame_recovery_successes = 0;
        size_t frame_recovery_failures = 0;
        size_t metadata_recovery_successes = 0;
        size_t metadata_recovery_failures = 0;
        size_t total_corruption_skip_bytes = 0;
        
        double getFrameRecoverySuccessRate() const {
            if (frame_corruption_count == 0) return 0.0;
            return (static_cast<double>(frame_recovery_successes) / frame_corruption_count) * 100.0;
        }
        
        double getMetadataRecoverySuccessRate() const {
            if (metadata_corruption_count == 0) return 0.0;
            return (static_cast<double>(metadata_recovery_successes) / metadata_corruption_count) * 100.0;
        }
        
        size_t getTotalCorruptions() const {
            return frame_corruption_count + metadata_corruption_count;
        }
    };
    
    CorruptionRecoveryStats getCorruptionRecoveryStats() const;
    
    // Error Recovery Configuration
    
    /**
     * @brief Error recovery configuration options
     */
    struct ErrorRecoveryConfig {
        bool enable_sync_recovery = true;              ///< Enable sync pattern recovery
        bool enable_crc_recovery = true;               ///< Enable CRC error recovery
        bool enable_metadata_recovery = true;          ///< Enable metadata corruption recovery
        bool enable_frame_skipping = true;             ///< Enable corrupted frame skipping
        size_t max_recovery_attempts = 3;              ///< Maximum recovery attempts per error
        size_t sync_search_limit_bytes = 64 * 1024;    ///< Maximum bytes to search for sync pattern
        size_t corruption_skip_limit_bytes = 1024;     ///< Maximum bytes to skip for corruption recovery
        double error_rate_threshold = 10.0;            ///< Error rate threshold for disabling recovery (%)
        bool log_recovery_attempts = true;             ///< Log recovery attempts for debugging
        bool strict_rfc_compliance = false;            ///< Strict RFC 9639 compliance mode
    };
    
    /**
     * @brief Set error recovery configuration
     * @param config Error recovery configuration options
     * @thread_safety Thread-safe
     */
    void setErrorRecoveryConfig(const ErrorRecoveryConfig& config);
    
    /**
     * @brief Get current error recovery configuration
     * @return Current error recovery configuration
     * @thread_safety Thread-safe
     */
    ErrorRecoveryConfig getErrorRecoveryConfig() const;
    
    /**
     * @brief Reset error recovery configuration to defaults
     * @thread_safety Thread-safe
     */
    void resetErrorRecoveryConfig();
    
    /**
     * @brief Thread safety validation results
     */
    struct ThreadSafetyValidation {
        bool public_private_pattern_correct = false;   ///< Public/private lock pattern implemented correctly
        bool lock_ordering_documented = false;         ///< Lock acquisition order documented
        bool raii_guards_used = false;                 ///< RAII lock guards used consistently
        bool atomic_operations_correct = false;        ///< Atomic operations used appropriately
        bool no_callback_under_lock = false;           ///< No callbacks invoked while holding locks
        bool exception_safety_maintained = false;      ///< Exception safety maintained
        size_t public_methods_with_locks = 0;          ///< Number of public methods with proper locking
        size_t private_unlocked_methods = 0;           ///< Number of private _unlocked methods
        std::vector<std::string> violations;           ///< List of thread safety violations found
        std::vector<std::string> recommendations;      ///< Recommendations for improvement
        
        bool isValid() const {
            return public_private_pattern_correct && lock_ordering_documented && 
                   raii_guards_used && atomic_operations_correct && 
                   no_callback_under_lock && exception_safety_maintained;
        }
        
        double getComplianceScore() const {
            int total_checks = 6;
            int passed_checks = 0;
            if (public_private_pattern_correct) passed_checks++;
            if (lock_ordering_documented) passed_checks++;
            if (raii_guards_used) passed_checks++;
            if (atomic_operations_correct) passed_checks++;
            if (no_callback_under_lock) passed_checks++;
            if (exception_safety_maintained) passed_checks++;
            return (static_cast<double>(passed_checks) / total_checks) * 100.0;
        }
    };
    
    // Thread safety validation methods (public for testing)
    ThreadSafetyValidation validateThreadSafety() const;
    bool validateThreadSafetyImplementation() const;
    void createThreadSafetyDocumentation() const;

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
    
    // CRC validation configuration (protected by m_state_mutex)
    CRCValidationMode m_crc_validation_mode = CRCValidationMode::ENABLED;  ///< CRC validation mode
    size_t m_crc8_error_count = 0;                   ///< Count of CRC-8 header validation errors
    size_t m_crc16_error_count = 0;                  ///< Count of CRC-16 frame validation errors
    size_t m_crc_error_threshold = 10;               ///< Maximum CRC errors before disabling validation
    bool m_crc_validation_disabled_due_to_errors = false;  ///< CRC validation disabled due to excessive errors
    
    // Streamable subset validation configuration (protected by m_state_mutex)
    StreamableSubsetMode m_streamable_subset_mode = StreamableSubsetMode::ENABLED;  ///< Streamable subset validation mode
    size_t m_streamable_subset_frames_validated = 0;        ///< Number of frames validated for streamable subset
    size_t m_streamable_subset_violations_detected = 0;     ///< Number of streamable subset violations detected
    size_t m_streamable_subset_block_size_violations = 0;   ///< Number of block size violations
    size_t m_streamable_subset_header_dependency_violations = 0;  ///< Number of frame header dependency violations
    size_t m_streamable_subset_sample_rate_violations = 0;  ///< Number of sample rate encoding violations
    size_t m_streamable_subset_bit_depth_violations = 0;    ///< Number of bit depth encoding violations
    bool m_streamable_subset_disabled_due_to_errors = false; ///< Streamable subset validation disabled due to excessive errors
    
    // Error recovery configuration (protected by m_state_mutex)
    ErrorRecoveryConfig m_error_recovery_config;            ///< Error recovery configuration options
    
    // Error recovery system (protected by m_state_mutex)
    std::unique_ptr<ErrorRecoveryManager> m_error_recovery_manager;  ///< Centralized error recovery management
    
    // Sync loss recovery statistics (protected by m_state_mutex)
    size_t m_sync_loss_count = 0;                    ///< Number of sync losses encountered
    size_t m_sync_recovery_successes = 0;            ///< Number of successful sync recoveries
    size_t m_sync_recovery_failures = 0;             ///< Number of failed sync recoveries
    size_t m_total_sync_search_bytes = 0;            ///< Total bytes searched for sync recovery
    uint64_t m_last_sync_recovery_offset = 0;        ///< File offset of last successful sync recovery
    
    // Corruption detection and recovery statistics (protected by m_state_mutex)
    size_t m_frame_corruption_count = 0;             ///< Number of corrupted frames detected
    size_t m_metadata_corruption_count = 0;          ///< Number of corrupted metadata blocks detected
    size_t m_frame_recovery_successes = 0;           ///< Number of successful frame corruption recoveries
    size_t m_frame_recovery_failures = 0;            ///< Number of failed frame corruption recoveries
    size_t m_metadata_recovery_successes = 0;        ///< Number of successful metadata corruption recoveries
    size_t m_metadata_recovery_failures = 0;         ///< Number of failed metadata corruption recoveries
    size_t m_total_corruption_skip_bytes = 0;        ///< Total bytes skipped due to corruption
    
    // Performance monitoring and optimization state (protected by m_state_mutex)
    struct PerformanceStatistics {
        uint64_t frames_parsed = 0;                      ///< Total frames parsed
        uint64_t total_parse_time_us = 0;                ///< Total parsing time in microseconds
        uint64_t min_parse_time_us = UINT64_MAX;         ///< Minimum frame parse time
        uint64_t max_parse_time_us = 0;                  ///< Maximum frame parse time
        uint64_t sync_searches = 0;                      ///< Number of sync pattern searches
        uint64_t sync_search_time_us = 0;                ///< Total sync search time in microseconds
        uint64_t buffer_reallocations = 0;               ///< Number of buffer reallocations
        uint64_t cache_hits = 0;                         ///< Number of cache hits
        uint64_t cache_misses = 0;                       ///< Number of cache misses
        std::chrono::high_resolution_clock::time_point monitoring_start_time; ///< When monitoring started
    } m_perf_stats;
    
    // Cached frame processing parameters for performance (protected by m_state_mutex)
    uint8_t m_cached_bytes_per_sample = 0;              ///< Cached bytes per sample calculation
    bool m_cached_is_fixed_block_size = false;          ///< Cached fixed block size flag
    bool m_cached_is_high_sample_rate = false;          ///< Cached high sample rate flag
    bool m_cached_is_multichannel = false;              ///< Cached multichannel flag
    uint32_t m_cached_avg_frame_size = 0;               ///< Cached average frame size estimate
    uint32_t m_cached_frame_size_variance = 0;          ///< Cached frame size variance estimate
    
    // Memory management state (protected by m_state_mutex)
    size_t m_memory_limit_bytes = 64 * 1024 * 1024;     ///< Memory usage limit (64MB default)
    size_t m_peak_memory_usage = 0;                     ///< Peak memory usage observed
    
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
    bool parseMetadataBlockHeader_unlocked(FLACMetadataBlock& block);
    
    // Frame size calculation and boundary detection
    uint32_t calculateFrameSize(const FLACFrame& frame);
    uint32_t calculateFrameSize_unlocked(const FLACFrame& frame);
    bool validateFrameSize_unlocked(uint32_t frame_size, const FLACFrame& frame) const;
    uint32_t calculateTheoreticalMinFrameSize_unlocked(const FLACFrame& frame) const;
    uint32_t calculateTheoreticalMaxFrameSize_unlocked(const FLACFrame& frame) const;
    uint32_t findFrameEnd(const uint8_t* buffer, uint32_t buffer_size);
    uint32_t findFrameEndFromFile(uint64_t frame_start_offset);
    uint32_t findFrameEndFromFile_unlocked(uint64_t frame_start_offset);
    bool parseStreamInfoBlock(const FLACMetadataBlock& block);
    bool parseStreamInfoBlock_unlocked(const FLACMetadataBlock& block);
    bool parseSeekTableBlock(const FLACMetadataBlock& block);
    bool parseVorbisCommentBlock(const FLACMetadataBlock& block);
    bool parsePictureBlock(const FLACMetadataBlock& block);
    bool skipMetadataBlock(const FLACMetadataBlock& block);
    
    bool findNextFrame(FLACFrame& frame);
    bool findNextFrame_unlocked(FLACFrame& frame);
    bool getNextFrameFromSeekTable(FLACFrame& frame);
    bool parseFrameHeader(FLACFrame& frame);
    bool parseFrameHeader_unlocked(FLACFrame& frame);
    bool parseFrameHeaderFromBuffer_unlocked(FLACFrame& frame, const uint8_t* buffer, size_t buffer_size, uint64_t file_offset);
    bool validateFrameHeader(const FLACFrame& frame);
    bool validateFrameHeader_unlocked(const FLACFrame& frame);
    bool validateFrameHeaderAt(uint64_t file_offset);
    
    // UTF-8 decoding for frame/sample numbers
    bool decodeUTF8Number_unlocked(uint64_t& number, uint8_t* raw_header, uint32_t& raw_header_len);
    
    // CRC calculation methods
    uint8_t calculateHeaderCRC8_unlocked(const uint8_t* data, size_t length);
    uint16_t calculateFrameCRC16_unlocked(const uint8_t* data, size_t length);
    
    // CRC validation methods
    bool validateHeaderCRC8_unlocked(const uint8_t* data, size_t length, uint8_t expected_crc);
    bool validateFrameCRC16_unlocked(const uint8_t* data, size_t length, uint16_t expected_crc);
    
    // CRC error recovery strategies
    bool handleCRCError_unlocked(bool is_header_crc, const std::string& context);
    bool shouldContinueAfterCRCError_unlocked() const;
    
    bool readFrameData(const FLACFrame& frame, std::vector<uint8_t>& data);
    void resetPositionTracking();
    void updatePositionTracking(uint64_t sample_position, uint64_t file_offset);
    void updatePositionTracking_unlocked(uint64_t sample_position, uint64_t file_offset);
    bool validateSamplePosition_unlocked(uint64_t sample_position) const;

    bool recoverPositionAfterSeek_unlocked();
    
    bool seekWithTable(uint64_t target_sample);
    bool seekWithTable_unlocked(uint64_t target_sample);
    bool validateSeekTable_unlocked() const;
    bool validateSeekPoint_unlocked(const FLACSeekPoint& point) const;
    
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
    
    // RFC 9639 sync pattern validation methods
    bool validateFrameSync_unlocked(const uint8_t* data, size_t size) const;
    bool searchSyncPattern_unlocked(const uint8_t* buffer, size_t buffer_size, size_t& sync_offset) const;
    
    // Error handling and recovery methods
    bool attemptStreamInfoRecovery();
    bool attemptStreamInfoRecovery_unlocked();
    bool validateStreamInfoParameters() const;
    bool validateStreamInfoParameters_unlocked() const;
    bool validateRecoveredStreamInfo_unlocked() const;
    void createFallbackStreamInfo_unlocked();
    bool checkStreamInfoConsistency_unlocked(const FLACFrame& frame) const;
    bool recoverFromCorruptedMetadata();
    bool resynchronizeToNextFrame();
    bool resynchronizeToNextFrame_unlocked();
    void provideDefaultStreamInfo();
    
    // Sync loss recovery methods
    bool recoverFromLostSync_unlocked(uint64_t search_start_offset, uint64_t& recovered_offset);
    bool searchSyncPatternWithExpansion_unlocked(uint64_t start_offset, size_t initial_window, uint64_t& found_offset);
    bool validateSyncRecovery_unlocked(uint64_t sync_offset, FLACFrame& recovered_frame);
    void updateSyncLossStatistics_unlocked(bool recovery_successful, size_t bytes_searched);
    
    // Corruption detection and recovery methods
    bool detectFrameCorruption_unlocked(const FLACFrame& frame, const std::vector<uint8_t>& frame_data);
    bool detectMetadataCorruption_unlocked(const FLACMetadataBlock& block);
    bool skipCorruptedFrame_unlocked(uint64_t& next_frame_offset);
    bool skipCorruptedMetadataBlock_unlocked(const FLACMetadataBlock& block, uint64_t& next_block_offset);
    bool searchForNextValidFrame_unlocked(uint64_t start_offset, FLACFrame& found_frame);
    bool searchForNextValidMetadataBlock_unlocked(uint64_t start_offset, FLACMetadataBlock& found_block);
    void updateCorruptionStatistics_unlocked(bool is_frame_corruption, bool recovery_successful);
    
    // Frame-level error recovery methods
    bool handleLostFrameSync();
    bool handleLostFrameSync_unlocked();
    bool skipCorruptedFrame();
    bool skipCorruptedFrame_unlocked();
    bool validateFrameCRC(const FLACFrame& frame, const std::vector<uint8_t>& frame_data);
    MediaChunk createSilenceChunk(uint32_t block_size);
    bool recoverFromFrameError();
    bool recoverFromFrameError_unlocked();
    
    // Metadata validation methods
    bool validateMetadataBlockLength_unlocked(FLACMetadataType type, uint32_t length) const;
    const char* getMetadataBlockTypeName_unlocked(FLACMetadataType type) const;
    bool recoverFromCorruptedBlockHeader_unlocked(FLACMetadataBlock& block);
    
    // Memory management methods
    void initializeBuffers();
    void optimizeSeekTable();
    void limitVorbisComments();
    void limitPictureStorage();
    size_t calculateMemoryUsage() const;
    void freeUnusedMemory();
    bool ensureBufferCapacity(std::vector<uint8_t>& buffer, size_t required_size) const;
    
    // Enhanced memory management methods
    void trackMemoryUsage();
    void enforceMemoryLimits();
    void shrinkBuffersToOptimalSize();
    void setMemoryLimit(size_t limit_bytes);
    size_t getMemoryLimit() const;
    size_t getPeakMemoryUsage() const;
    
    /**
     * @brief Memory usage statistics structure
     */
    struct MemoryUsageStats {
        size_t current_usage = 0;              ///< Current total memory usage
        size_t peak_usage = 0;                 ///< Peak memory usage observed
        size_t memory_limit = 0;               ///< Configured memory limit
        double utilization_percentage = 0.0;   ///< Memory utilization as percentage
        
        // Component breakdown
        size_t seek_table_usage = 0;           ///< Memory used by seek table
        size_t vorbis_comments_usage = 0;      ///< Memory used by Vorbis comments
        size_t pictures_usage = 0;             ///< Memory used by embedded pictures
        size_t frame_buffer_usage = 0;         ///< Memory used by frame buffer
        size_t sync_buffer_usage = 0;          ///< Memory used by sync buffer
        size_t readahead_buffer_usage = 0;     ///< Memory used by readahead buffer
        size_t frame_index_usage = 0;          ///< Memory used by frame index
    };
    
    MemoryUsageStats getMemoryUsageStats() const;
    void logMemoryUsageStats() const;
    

    

    
    // Performance optimization methods
    size_t findSeekPointIndex(uint64_t target_sample) const;
    bool optimizedFrameSync(uint64_t start_offset, FLACFrame& frame);
    void prefetchNextFrame();
    bool isNetworkStream() const;
    void optimizeForNetworkStreaming();
    
    // Performance optimization methods
    void optimizeFrameProcessingPerformance();
    bool validatePerformanceOptimizations();
    void logPerformanceMetrics();
    size_t calculateOptimalSyncBufferSize() const;
    void cacheFrameProcessingParameters();
    void initializePerformanceMonitoring();
    
    // Performance monitoring methods
    void recordFrameParseTime(std::chrono::microseconds parse_time);
    void recordSyncSearchTime(std::chrono::microseconds search_time);
    void recordBufferReallocation();
    void recordCacheHit();
    void recordCacheMiss();
    void logPerformanceStatistics() const;
    
    // Frame indexing methods
    bool performInitialFrameIndexing();
    void addFrameToIndex(const FLACFrame& frame);
    void addFrameToIndex(uint64_t sample_offset, uint64_t file_offset, uint32_t block_size, uint32_t frame_size = 0);
    void enableFrameIndexing(bool enable);
    void clearFrameIndex();
};

/**
 * @brief FLAC-specific error types for enhanced error classification
 */
enum class FLACErrorType {
    IO_ERROR,           ///< File reading/seeking failures
    FORMAT_ERROR,       ///< Invalid FLAC format structure
    RFC_VIOLATION,      ///< Non-compliance with RFC 9639
    RESOURCE_ERROR,     ///< Memory/resource exhaustion
    LOGIC_ERROR         ///< Internal consistency failures
};

/**
 * @brief Error severity levels for recovery strategy selection
 */
enum class ErrorSeverity {
    RECOVERABLE,        ///< Can continue processing normally
    FRAME_SKIP,         ///< Skip current frame, continue with next
    STREAM_RESET,       ///< Reset to known good state
    FATAL               ///< Cannot continue processing
};

/**
 * @brief Detailed error context for debugging and recovery
 */
struct ErrorContext {
    FLACErrorType type;
    ErrorSeverity severity;
    std::string message;
    std::string rfc_section;        ///< RFC 9639 section reference
    uint64_t file_position;         ///< Byte position where error occurred
    uint64_t sample_position;       ///< Sample position where error occurred
    std::chrono::steady_clock::time_point timestamp;
    
    // Additional context details
    std::map<std::string, std::string> details;
    
    ErrorContext() = default;
    
    ErrorContext(FLACErrorType t, ErrorSeverity s, const std::string& msg)
        : type(t), severity(s), message(msg), file_position(0), sample_position(0),
          timestamp(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Error statistics for monitoring and analysis
 */
struct ErrorStatistics {
    // Error counts by type
    size_t io_errors = 0;
    size_t format_errors = 0;
    size_t rfc_violations = 0;
    size_t resource_errors = 0;
    size_t logic_errors = 0;
    
    // Error counts by severity
    size_t recoverable_errors = 0;
    size_t frame_skip_errors = 0;
    size_t stream_reset_errors = 0;
    size_t fatal_errors = 0;
    
    // Recovery statistics
    size_t successful_recoveries = 0;
    size_t failed_recoveries = 0;
    size_t recovery_attempts = 0;
    
    // Timing statistics
    std::chrono::steady_clock::time_point first_error_time;
    std::chrono::steady_clock::time_point last_error_time;
    
    ErrorStatistics() {
        auto now = std::chrono::steady_clock::now();
        first_error_time = now;
        last_error_time = now;
    }
    
    /**
     * @brief Get total error count across all types
     */
    size_t getTotalErrors() const {
        return io_errors + format_errors + rfc_violations + resource_errors + logic_errors;
    }
    
    /**
     * @brief Get recovery success rate as percentage
     */
    double getRecoverySuccessRate() const {
        if (recovery_attempts == 0) return 0.0;
        return (static_cast<double>(successful_recoveries) / recovery_attempts) * 100.0;
    }
};

/**
 * @brief Recovery strategy configuration
 */
struct RecoveryConfig {
    // Sync loss recovery settings
    bool enable_sync_recovery = true;
    size_t max_sync_search_bytes = 64 * 1024;      ///< Maximum bytes to search for sync pattern
    size_t sync_search_window_expansion = 1024;     ///< Bytes to expand search window on failure
    size_t max_sync_recovery_attempts = 3;          ///< Maximum sync recovery attempts per error
    
    // CRC error recovery settings
    bool enable_crc_recovery = true;
    size_t crc_error_threshold = 10;                ///< CRC errors before disabling validation
    bool strict_crc_mode = false;                   ///< Reject all frames with CRC errors
    
    // Metadata corruption recovery settings
    bool enable_metadata_recovery = true;
    size_t max_metadata_skip_bytes = 1024 * 1024;  ///< Maximum bytes to skip for metadata recovery
    bool allow_streaminfo_recovery = true;          ///< Allow STREAMINFO recovery from first frame
    
    // General recovery settings
    size_t max_recovery_attempts = 5;               ///< Maximum recovery attempts per error
    size_t error_history_limit = 100;               ///< Maximum error records to keep
    bool enable_fallback_mode = true;               ///< Enable fallback parsing mode
    
    RecoveryConfig() = default;
};

/**
 * @brief Centralized error recovery manager for FLAC demuxer
 * 
 * This class provides centralized error handling and recovery for the FLAC demuxer,
 * implementing sophisticated recovery strategies based on error type and severity.
 * 
 * @thread_safety This class is thread-safe. All public methods can be called
 *                concurrently from multiple threads.
 */
class ErrorRecoveryManager {
public:
    ErrorRecoveryManager();
    explicit ErrorRecoveryManager(const RecoveryConfig& config);
    
    /**
     * @brief Handle an error with automatic recovery attempt
     * @param context Error context information
     * @param demuxer Pointer to demuxer for recovery operations
     * @return true if error was handled successfully (recovery or acceptable failure)
     */
    bool handleError(const ErrorContext& context, FLACDemuxer* demuxer);
    
    /**
     * @brief Handle an error with simplified parameters
     * @param type Error type
     * @param severity Error severity
     * @param message Error message
     * @param demuxer Pointer to demuxer for recovery operations
     * @return true if error was handled successfully
     */
    bool handleError(FLACErrorType type, ErrorSeverity severity, 
                    const std::string& message, FLACDemuxer* demuxer);
    
    /**
     * @brief Recover from lost sync pattern
     * @param demuxer Pointer to demuxer for recovery operations
     * @param search_start_offset File offset to start sync search
     * @return true if sync pattern was found and position recovered
     */
    bool recoverFromLostSync(FLACDemuxer* demuxer, uint64_t search_start_offset);
    
    /**
     * @brief Recover from CRC validation errors
     * @param demuxer Pointer to demuxer for recovery operations
     * @param is_header_crc True if this is a header CRC error, false for frame CRC
     * @return true if recovery was successful or error is acceptable
     */
    bool recoverFromCRCError(FLACDemuxer* demuxer, bool is_header_crc);
    
    /**
     * @brief Recover from metadata corruption
     * @param demuxer Pointer to demuxer for recovery operations
     * @return true if recovery was successful
     */
    bool recoverFromMetadataCorruption(FLACDemuxer* demuxer);
    
    /**
     * @brief Get current error statistics
     * @return Copy of current error statistics
     */
    ErrorStatistics getErrorStatistics() const;
    
    /**
     * @brief Reset error statistics and history
     */
    void resetErrorStatistics();
    
    /**
     * @brief Update recovery configuration
     * @param config New recovery configuration
     */
    void setRecoveryConfig(const RecoveryConfig& config);
    
    /**
     * @brief Get current recovery configuration
     * @return Copy of current recovery configuration
     */
    RecoveryConfig getRecoveryConfig() const;
    
    /**
     * @brief Check if error recovery is enabled for a specific error type
     * @param type Error type to check
     * @return true if recovery is enabled for this error type
     */
    bool isRecoveryEnabled(FLACErrorType type) const;
    
    /**
     * @brief Get error history for debugging and analysis
     * @return Vector of recent error contexts (limited by config.error_history_limit)
     */
    std::vector<ErrorContext> getErrorHistory() const;

private:
    mutable std::mutex m_mutex;                     ///< Thread safety for all operations
    RecoveryConfig m_config;                        ///< Recovery configuration
    ErrorStatistics m_stats;                        ///< Error statistics
    std::vector<ErrorContext> m_error_history;      ///< Recent error history
    
    /**
     * @brief Select recovery strategy based on error context
     * @param context Error context
     * @return Recovery strategy to attempt
     */
    DemuxerErrorRecovery selectRecoveryStrategy(const ErrorContext& context) const;
    
    /**
     * @brief Execute recovery strategy
     * @param strategy Recovery strategy to execute
     * @param context Error context
     * @param demuxer Pointer to demuxer for recovery operations
     * @return true if recovery was successful
     */
    bool executeRecoveryStrategy(DemuxerErrorRecovery strategy, 
                                const ErrorContext& context, 
                                FLACDemuxer* demuxer);
    
    /**
     * @brief Update error statistics with new error
     * @param context Error context
     * @param recovery_successful True if recovery was successful
     */
    void updateStatistics(const ErrorContext& context, bool recovery_successful);
    
    /**
     * @brief Add error to history (thread-safe)
     * @param context Error context to add
     */
    void addToHistory(const ErrorContext& context);
    
    /**
     * @brief Check if we should attempt recovery based on error history
     * @param context Error context
     * @return true if recovery should be attempted
     */
    bool shouldAttemptRecovery(const ErrorContext& context) const;
    
    /**
     * @brief Get RFC 9639 section reference for error type
     * @param type Error type
     * @return RFC section reference string
     */
    std::string getRFCReference(FLACErrorType type) const;
};

#endif // FLACDEMUXER_H