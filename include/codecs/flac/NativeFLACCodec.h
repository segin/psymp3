/*
 * NativeFLACCodec.h - Native FLAC decoder without libFLAC dependency
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

#ifndef NATIVEFLACCODEC_H
#define NATIVEFLACCODEC_H

// No direct includes - all includes should be in psymp3.h

#ifdef HAVE_NATIVE_FLAC

namespace PsyMP3 {
namespace Codec {
namespace FLAC {

/**
 * @brief Native FLAC decoder implementation without libFLAC dependency
 * 
 * This codec implements a complete FLAC decoder following RFC 9639 specification
 * without requiring the external libFLAC library. It provides the same FLACCodec
 * interface as the libFLAC wrapper, allowing build-time selection between
 * implementations via configure flags.
 * 
 * ARCHITECTURE OVERVIEW:
 * =====================
 * The native decoder implements all FLAC decoding components from scratch:
 * - BitstreamReader: Bit-level reading with variable-length field support
 * - FrameParser: Frame header/footer parsing and validation
 * - SubframeDecoder: CONSTANT, VERBATIM, FIXED, and LPC subframe decoding
 * - ResidualDecoder: Rice/Golomb entropy decoding with partition support
 * - ChannelDecorrelator: Stereo decorrelation (left-side, right-side, mid-side)
 * - SampleReconstructor: Bit depth conversion and sample interleaving
 * - CRCValidator: CRC-8 and CRC-16 validation per RFC 9639
 * - MetadataParser: STREAMINFO and other metadata block parsing
 * 
 * RFC 9639 COMPLIANCE:
 * ===================
 * This implementation strictly follows RFC 9639 FLAC specification:
 * - Frame structure per Section 9
 * - Subframe types per Section 9.2
 * - Prediction algorithms per Section 4.3
 * - Entropy coding per Section 4.4
 * - CRC polynomials per specification
 * - Channel assignments per Section 9.1
 * - Bit depth range 4-32 bits
 * - Sample rate range 1-655350 Hz
 * - Channel count 1-8
 * 
 * PERFORMANCE CHARACTERISTICS:
 * ===========================
 * - 44.1kHz/16-bit: <2% CPU usage on target hardware
 * - 96kHz/24-bit: Real-time performance without dropouts
 * - Frame processing: <100μs per frame for real-time requirements
 * - Memory usage: Bounded allocation with pre-allocated buffers
 * - Zero external dependencies beyond C++17 standard library
 * 
 * THREADING SAFETY:
 * ================
 * This class follows PsyMP3's public/private lock pattern:
 * - All public methods acquire locks using RAII guards
 * - All public methods call corresponding _unlocked private methods
 * - Internal method calls always use _unlocked versions
 * - Exception safety maintained through RAII lock guards
 * - No callbacks invoked while holding internal locks
 * 
 * LOCK ACQUISITION ORDER (to prevent deadlocks):
 * =============================================
 * 1. m_state_mutex (for codec state and configuration)
 * 2. m_decoder_mutex (for decoder operations)
 * 3. m_buffer_mutex (for output buffer management)
 * 
 * @thread_safety This class is thread-safe. All public methods follow the
 *                public/private lock pattern to prevent deadlocks.
 * 
 * @see RFC 9639 FLAC specification in docs/rfc9639.txt
 * @see Design document in .kiro/specs/native-flac-decoder/design.md
 * @see Requirements in .kiro/specs/native-flac-decoder/requirements.md
 */
class FLACCodec : public AudioCodec {
public:
    explicit FLACCodec(const StreamInfo& stream_info);
    ~FLACCodec() override;
    
    // AudioCodec interface implementation (thread-safe public methods)
    
    /**
     * @brief Initialize the native FLAC codec
     * 
     * Configures the codec from StreamInfo parameters and initializes all
     * decoding components. This method must be called before any decoding
     * operations.
     * 
     * @return true if initialization successful, false on error
     * @thread_safety Thread-safe. Uses m_state_mutex for synchronization.
     */
    bool initialize() override;
    
    /**
     * @brief Decode a FLAC frame from MediaChunk data
     * 
     * Processes a complete FLAC frame and returns decoded 16-bit PCM samples.
     * 
     * @param chunk MediaChunk containing complete FLAC frame data
     * @return AudioFrame with decoded 16-bit PCM samples, empty on error
     * @thread_safety Thread-safe. Uses multiple mutexes for synchronization.
     */
    AudioFrame decode(const MediaChunk& chunk) override;
    
    /**
     * @brief Flush remaining decoded samples from internal buffers
     * 
     * @return AudioFrame with remaining samples, empty if no samples buffered
     * @thread_safety Thread-safe. Uses m_buffer_mutex for synchronization.
     */
    AudioFrame flush() override;
    
    /**
     * @brief Reset codec state for seeking or new stream
     * 
     * @thread_safety Thread-safe. Uses multiple mutexes for synchronization.
     */
    void reset() override;
    
    /**
     * @brief Get codec name identifier
     * 
     * @return "flac" - the standard FLAC codec identifier
     * @thread_safety Thread-safe (no synchronization needed - returns constant)
     */
    std::string getCodecName() const override { return "flac"; }
    
    /**
     * @brief Check if codec can decode the given stream
     * 
     * @param stream_info Stream parameters to validate
     * @return true if codec can decode the stream, false otherwise
     * @thread_safety Thread-safe. Uses m_state_mutex for synchronization.
     */
    bool canDecode(const StreamInfo& stream_info) const override;
    
    // FLAC-specific public interface
    
    /**
     * @brief Check if codec supports seek reset functionality
     * 
     * @return true - native FLAC codec always supports seek reset
     * @thread_safety Thread-safe (no synchronization needed - returns constant)
     */
    bool supportsSeekReset() const;
    
    /**
     * @brief Get current sample position in stream
     * 
     * @return Current sample position (0-based)
     * @thread_safety Thread-safe. Uses atomic variable for lock-free access.
     */
    uint64_t getCurrentSample() const;
    
    /**
     * @brief Get codec performance and debugging statistics
     * 
     * @return FLACCodecStats structure with current statistics
     * @thread_safety Thread-safe. Uses m_state_mutex for synchronization.
     */
    FLACCodecStats getStats() const;
    
    /**
     * @brief Seek to specific sample position
     * 
     * Uses seek table if available for fast seeking, otherwise falls back
     * to frame scanning. Resets decoder state and positions to target sample.
     * 
     * @param target_sample Target sample position (0-based)
     * @return true if seek successful, false on error
     * @thread_safety Thread-safe. Uses m_state_mutex for synchronization.
     * 
     * Requirements: 43
     */
    bool seek(uint64_t target_sample);
    
    /**
     * @brief Set seek table for efficient seeking
     * 
     * Stores seek table parsed from SEEKTABLE metadata block.
     * Must be called before seeking operations for optimal performance.
     * 
     * @param seek_table Vector of seek points from metadata
     * @thread_safety Thread-safe. Uses m_state_mutex for synchronization.
     * 
     * Requirements: 43
     */
    void setSeekTable(const std::vector<SeekPoint>& seek_table);
    
    /**
     * @brief Set STREAMINFO metadata for MD5 validation
     * 
     * Stores STREAMINFO metadata including MD5 checksum for validation.
     * Must be called before decoding if MD5 validation is desired.
     * 
     * @param streaminfo STREAMINFO metadata from file
     * @thread_safety Thread-safe. Uses m_state_mutex for synchronization.
     * 
     * Requirements: 25
     */
    void setStreamInfo(const StreamInfoMetadata& streaminfo);
    
    /**
     * @brief Enable or disable MD5 validation
     * 
     * Controls whether MD5 checksum validation is performed during decoding.
     * When enabled, decoded samples are hashed and compared with STREAMINFO
     * MD5 at end of stream. When disabled, MD5 computation is skipped for
     * performance.
     * 
     * @param enabled true to enable MD5 validation, false to disable
     * @thread_safety Thread-safe. Uses m_state_mutex for synchronization.
     * 
     * Requirements: 25.8
     */
    void setMD5ValidationEnabled(bool enabled);
    
    /**
     * @brief Check MD5 validation result
     * 
     * Returns the result of MD5 validation after stream decoding completes.
     * Only valid if MD5 validation was enabled and stream has been fully
     * decoded.
     * 
     * @return true if MD5 matches, false if mismatch or validation not performed
     * @thread_safety Thread-safe. Uses m_state_mutex for synchronization.
     * 
     * Requirements: 25.5, 25.6
     */
    bool checkMD5Validation() const;
    
private:
    // State management enum (must be defined before methods that use it)
    enum class DecoderState {
        UNINITIALIZED,
        INITIALIZED,
        DECODING,
        DECODER_ERROR,  // Named DECODER_ERROR to avoid conflict with Windows ERROR macro
        END_OF_STREAM
    };
    
    // Private implementation methods (assume locks are already held)
    // These methods are called by public methods after acquiring appropriate locks
    
    bool initialize_unlocked();
    AudioFrame decode_unlocked(const MediaChunk& chunk);
    AudioFrame flush_unlocked();
    void reset_unlocked();
    bool canDecode_unlocked(const StreamInfo& stream_info) const;
    bool seek_unlocked(uint64_t target_sample);
    void setSeekTable_unlocked(const std::vector<SeekPoint>& seek_table);
    void setStreamInfo_unlocked(const StreamInfoMetadata& streaminfo);
    void setMD5ValidationEnabled_unlocked(bool enabled);
    bool checkMD5Validation_unlocked() const;
    
    // Seeking helpers
    bool seekUsingTable(uint64_t target_sample);
    bool seekByScanning(uint64_t target_sample);
    SeekPoint findNearestSeekPoint(uint64_t target_sample) const;
    
    // Streamable subset validation (Requirement 19)
    bool validateStreamableSubset(const FrameHeader& header) const;
    
    // Error recovery methods (Requirement 11)
    
    /**
     * @brief Handle sync loss by searching for next valid frame
     * 
     * Searches forward in bitstream for next valid frame sync pattern.
     * Validates frame header CRC to ensure sync is genuine.
     * 
     * @return true if valid sync found, false if no sync found
     * @thread_safety Assumes locks already held by caller
     * 
     * Requirements: 11.1
     */
    bool recoverFromSyncLoss();
    
    /**
     * @brief Handle invalid frame header by skipping to next frame
     * 
     * Attempts to find next valid frame after header parsing failure.
     * 
     * @return true if next frame found, false if recovery failed
     * @thread_safety Assumes locks already held by caller
     * 
     * Requirements: 11.2
     */
    bool recoverFromInvalidHeader();
    
    /**
     * @brief Handle subframe decoding error by outputting silence
     * 
     * Fills affected channel buffer with silence (zeros) when subframe
     * decoding fails. Allows decoding to continue with other channels.
     * 
     * @param channel_buffer Buffer to fill with silence
     * @param sample_count Number of samples to fill
     * @thread_safety Assumes locks already held by caller
     * 
     * Requirements: 11.3
     */
    void recoverFromSubframeError(int32_t* channel_buffer, uint32_t sample_count);
    
    /**
     * @brief Handle CRC validation failure
     * 
     * Logs error and decides whether to use decoded data or discard frame.
     * RFC 9639 allows using data even with CRC mismatch.
     * 
     * @param error_type Type of CRC error (header or frame)
     * @return true to use decoded data, false to discard frame
     * @thread_safety Assumes locks already held by caller
     * 
     * Requirements: 11.4
     */
    bool recoverFromCRCError(FLACError error_type);
    
    /**
     * @brief Handle memory allocation failure
     * 
     * Cleans up partially allocated resources and returns to safe state.
     * 
     * @thread_safety Assumes locks already held by caller
     * 
     * Requirements: 11.6
     */
    void recoverFromMemoryError();
    
    /**
     * @brief Handle unrecoverable error by resetting to clean state
     * 
     * Transitions decoder to ERROR state and clears all buffers.
     * Decoder must be reinitialized before further use.
     * 
     * @thread_safety Assumes locks already held by caller
     * 
     * Requirements: 11.8
     */
    void handleUnrecoverableError();
    
    // State management methods (Requirement 64)
    
    /**
     * @brief Transition decoder to new state
     * 
     * Validates state transitions and updates decoder state.
     * Logs state changes for debugging.
     * 
     * @param new_state Target state to transition to
     * @return true if transition valid, false if invalid transition
     * @thread_safety Assumes locks already held by caller
     * 
     * Requirements: 64.8
     */
    bool transitionState(DecoderState new_state);
    
    /**
     * @brief Check if decoder can transition to target state
     * 
     * Validates state transition rules.
     * 
     * @param current Current decoder state
     * @param target Target state to transition to
     * @return true if transition is valid, false otherwise
     * @thread_safety Assumes locks already held by caller
     * 
     * Requirements: 64.8
     */
    bool isValidStateTransition(DecoderState current, DecoderState target) const;
    
    /**
     * @brief Reset decoder from error state
     * 
     * Allows recovery from ERROR state by reinitializing decoder.
     * Clears error counters and resets state to UNINITIALIZED.
     * 
     * @return true if reset successful, false if reset failed
     * @thread_safety Assumes locks already held by caller
     * 
     * Requirements: 64.5, 64.6
     */
    bool resetFromErrorState();
    
    /**
     * @brief Get human-readable state name
     * 
     * @param state Decoder state
     * @return String representation of state
     * @thread_safety Thread-safe (no state modification)
     */
    const char* getStateName(DecoderState state) const;
    
    // Component instances
    std::unique_ptr<BitstreamReader> m_bitstream_reader;
    std::unique_ptr<FrameParser> m_frame_parser;
    std::unique_ptr<SubframeDecoder> m_subframe_decoder;
    std::unique_ptr<ResidualDecoder> m_residual_decoder;
    std::unique_ptr<ChannelDecorrelator> m_channel_decorrelator;
    std::unique_ptr<SampleReconstructor> m_sample_reconstructor;
    std::unique_ptr<CRCValidator> m_crc_validator;
    std::unique_ptr<MetadataParser> m_metadata_parser;
    std::unique_ptr<MD5Validator> m_md5_validator;
    
    // State management members
    StreamInfo m_stream_info;
    DecoderState m_state;
    std::atomic<uint64_t> m_current_sample;
    bool m_initialized;
    
    // Error tracking (Requirement 11)
    FLACError m_last_error;
    uint32_t m_consecutive_errors;
    static constexpr uint32_t MAX_CONSECUTIVE_ERRORS = 10;
    
    // Seeking support
    std::vector<SeekPoint> m_seek_table;
    bool m_has_seek_table;
    
    // MD5 validation support
    StreamInfoMetadata m_streaminfo;
    bool m_has_streaminfo;
    bool m_md5_validation_enabled;
    
    // Buffers
    static constexpr size_t MAX_BLOCK_SIZE = 65535;
    static constexpr size_t MAX_CHANNELS = 8;
    static constexpr size_t INPUT_BUFFER_SIZE = 64 * 1024;  // 64KB
    
    std::vector<uint8_t> m_input_buffer;
    std::vector<int32_t> m_decode_buffer[MAX_CHANNELS];  // Per-channel decode buffers
    std::vector<int16_t> m_output_buffer;
    
    // Performance statistics
    mutable FLACCodecStats m_stats;

    // Threading mutexes (lock acquisition order documented above)
    mutable std::mutex m_state_mutex;
    mutable std::mutex m_decoder_mutex;
    mutable std::mutex m_buffer_mutex;
    
    // Prevent copying
    FLACCodec(const FLACCodec&) = delete;
    FLACCodec& operator=(const FLACCodec&) = delete;
};

} // namespace FLAC
} // namespace Codec
} // namespace PsyMP3

#endif // HAVE_NATIVE_FLAC

#endif // NATIVEFLACCODEC_H
