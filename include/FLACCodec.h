/*
 * FLACCodec.h - Container-agnostic FLAC audio codec
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

#ifndef FLACCODEC_H
#define FLACCODEC_H

// No direct includes - all includes should be in psymp3.h

#ifdef HAVE_FLAC

// Forward declarations
class FLACCodec;

/**
 * @brief FLAC frame information extracted during decoding
 * 
 * This structure contains the essential information about a FLAC frame
 * that's needed for proper decoding and output formatting. All fields
 * are validated against RFC 9639 FLAC specification requirements.
 */
struct FLACFrameInfo {
    uint32_t block_size = 0;         ///< Number of samples in this frame (16-65535 per RFC 9639)
    uint32_t sample_rate = 0;        ///< Sample rate for this frame (1-655350 Hz per RFC 9639)
    uint16_t channels = 0;           ///< Number of channels in this frame (1-8 per RFC 9639)
    uint16_t bits_per_sample = 0;    ///< Bits per sample in this frame (4-32 per RFC 9639)
    uint64_t sample_number = 0;      ///< Starting sample number for this frame
    uint8_t channel_assignment = 0;  ///< Channel assignment mode (independent, left-side, etc.)
    bool variable_block_size = false; ///< True if using variable block size strategy
    
    /**
     * @brief Check if frame information is valid per RFC 9639
     */
    bool isValid() const {
        return block_size >= 16 && block_size <= 65535 &&
               sample_rate >= 1 && sample_rate <= 655350 &&
               channels >= 1 && channels <= 8 && 
               bits_per_sample >= 4 && bits_per_sample <= 32;
    }
    
    /**
     * @brief Get frame duration in milliseconds
     */
    uint64_t getDurationMs() const {
        if (sample_rate == 0 || block_size == 0) return 0;
        return (static_cast<uint64_t>(block_size) * 1000ULL) / sample_rate;
    }
    
    /**
     * @brief Get expected output sample count for 16-bit conversion
     */
    size_t getOutputSampleCount() const {
        return static_cast<size_t>(block_size) * channels;
    }
    
    /**
     * @brief Get channel assignment type name for debugging
     */
    const char* getChannelAssignmentName() const {
        switch (channel_assignment) {
            case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
                return "independent";
            case 8: return "left-side";
            case 9: return "right-side";
            case 10: return "mid-side";
            default: return "reserved";
        }
    }
};

/**
 * @brief Audio quality metrics for validation and testing
 * 
 * This structure contains various audio quality measurements used
 * for validating codec accuracy and conversion quality.
 */
struct AudioQualityMetrics {
    double signal_to_noise_ratio_db = 0.0;     ///< SNR in decibels
    double total_harmonic_distortion = 0.0;    ///< THD as percentage
    double dynamic_range_db = 0.0;             ///< Dynamic range in decibels
    double peak_amplitude = 0.0;               ///< Peak sample amplitude (0.0-1.0)
    double rms_amplitude = 0.0;                ///< RMS amplitude (0.0-1.0)
    double dc_offset = 0.0;                    ///< DC offset as percentage
    size_t zero_crossings = 0;                 ///< Number of zero crossings
    size_t clipped_samples = 0;                ///< Number of clipped samples
    bool bit_perfect = false;                  ///< True if bit-perfect match with reference
    
    /**
     * @brief Check if quality metrics indicate good audio quality
     */
    bool isGoodQuality() const {
        return signal_to_noise_ratio_db > 90.0 &&  // >90dB SNR
               total_harmonic_distortion < 0.01 &&  // <1% THD
               dynamic_range_db > 80.0 &&           // >80dB dynamic range
               clipped_samples == 0;                // No clipping
    }
    
    /**
     * @brief Check if quality metrics indicate bit-perfect decoding
     */
    bool isBitPerfect() const {
        return bit_perfect && 
               signal_to_noise_ratio_db > 120.0 &&  // Theoretical maximum for 16-bit
               total_harmonic_distortion < 0.0001;  // Minimal distortion
    }
};

/**
 * @brief FLAC codec performance and debugging statistics
 * 
 * This structure tracks performance metrics and debugging information
 * for the FLAC codec to enable optimization and troubleshooting.
 */
struct FLACCodecStats {
    size_t frames_decoded = 0;        ///< Total number of FLAC frames decoded
    size_t samples_decoded = 0;       ///< Total number of audio samples decoded
    size_t total_bytes_processed = 0; ///< Total input bytes processed
    size_t conversion_operations = 0; ///< Number of bit depth conversions performed
    size_t error_count = 0;          ///< Number of decoding errors encountered
    double average_frame_size = 0.0; ///< Average frame size in bytes
    double decode_efficiency = 0.0;   ///< Samples decoded per second
    size_t memory_usage_bytes = 0;   ///< Current memory usage in bytes
    
    // Performance timing statistics
    uint64_t total_decode_time_us = 0;    ///< Total decoding time in microseconds
    uint64_t max_frame_decode_time_us = 0; ///< Maximum single frame decode time
    uint64_t min_frame_decode_time_us = UINT64_MAX; ///< Minimum single frame decode time
    
    // Error breakdown
    size_t crc_errors = 0;           ///< CRC validation failures
    size_t sync_errors = 0;          ///< Frame synchronization errors
    size_t memory_errors = 0;        ///< Memory allocation failures
    size_t libflac_errors = 0;       ///< libFLAC internal errors
    
    /**
     * @brief Calculate average decode time per frame in microseconds
     */
    double getAverageDecodeTimeUs() const {
        return frames_decoded > 0 ? static_cast<double>(total_decode_time_us) / frames_decoded : 0.0;
    }
    
    /**
     * @brief Calculate decode efficiency in samples per second
     */
    double getDecodeEfficiency() const {
        return total_decode_time_us > 0 ? 
               (static_cast<double>(samples_decoded) * 1000000.0) / total_decode_time_us : 0.0;
    }
    
    /**
     * @brief Get error rate as percentage
     */
    double getErrorRate() const {
        return frames_decoded > 0 ? 
               (static_cast<double>(error_count) * 100.0) / frames_decoded : 0.0;
    }
};

/**
 * @brief libFLAC stream decoder wrapper with callback integration
 * 
 * This class wraps libFLAC's stream decoder and provides the callback
 * interface for integrating with the FLACCodec. It handles the low-level
 * FLAC decoding operations while the parent codec manages higher-level
 * concerns like threading and output formatting.
 * 
 * @thread_safety This class is NOT thread-safe on its own. The parent
 *                FLACCodec is responsible for thread synchronization.
 */
class FLACStreamDecoder : public FLAC::Decoder::Stream {
public:
    explicit FLACStreamDecoder(FLACCodec* parent);
    virtual ~FLACStreamDecoder();
    
    // Data feeding interface for parent codec
    bool feedData(const uint8_t* data, size_t size);
    void clearInputBuffer();
    size_t getInputBufferSize() const;
    bool hasInputData() const;
    
    // Error state management
    bool hasError() const { return m_error_occurred; }
    FLAC__StreamDecoderErrorStatus getLastError() const { return m_last_error; }
    void clearError();
    
protected:
    // libFLAC decoder callbacks (called by libFLAC during decoding)
    
    /**
     * @brief Read callback - provides FLAC frame data to libFLAC
     * @param buffer Buffer to fill with FLAC data
     * @param bytes Pointer to number of bytes to read/actually read
     * @return Read status indicating success, EOF, or error
     */
    FLAC__StreamDecoderReadStatus read_callback(FLAC__byte buffer[], size_t *bytes) override;
    
    /**
     * @brief Write callback - receives decoded PCM samples from libFLAC
     * @param frame FLAC frame information (block size, channels, etc.)
     * @param buffer Array of decoded sample buffers (one per channel)
     * @return Write status indicating success or error
     */
    FLAC__StreamDecoderWriteStatus write_callback(const FLAC__Frame *frame, 
                                                  const FLAC__int32 * const buffer[]) override;
    
    /**
     * @brief Metadata callback - receives FLAC metadata during decoding
     * @param metadata FLAC metadata block (usually STREAMINFO)
     */
    void metadata_callback(const FLAC__StreamMetadata *metadata) override;
    
    /**
     * @brief Error callback - handles libFLAC decoding errors
     * @param status Error status from libFLAC decoder
     */
    void error_callback(FLAC__StreamDecoderErrorStatus status) override;
    
private:
    FLACCodec* m_parent;                    ///< Parent codec for callbacks
    std::vector<uint8_t> m_input_buffer;    ///< Input data buffer for libFLAC
    size_t m_buffer_position = 0;           ///< Current read position in buffer
    mutable std::mutex m_input_mutex;       ///< Thread safety for input buffer
    
    // Performance optimization
    static constexpr size_t INPUT_BUFFER_SIZE = 64 * 1024; ///< 64KB input buffer
    
    // Error handling state
    bool m_error_occurred = false;
    FLAC__StreamDecoderErrorStatus m_last_error = FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC;
    
    // Prevent copying
    FLACStreamDecoder(const FLACStreamDecoder&) = delete;
    FLACStreamDecoder& operator=(const FLACStreamDecoder&) = delete;
};

/**
 * @brief Container-agnostic FLAC audio codec implementation
 * 
 * This codec decodes FLAC bitstream data from MediaChunk containers into
 * 16-bit PCM audio samples. It follows PsyMP3's threading safety patterns
 * and integrates with conditional compilation for optional FLAC support.
 * 
 * The codec is designed to work with any demuxer that provides FLAC frame
 * data in MediaChunk format, including FLACDemuxer, OggDemuxer, and
 * potentially future ISO demuxers.
 * 
 * ARCHITECTURE OVERVIEW:
 * =====================
 * The FLACCodec implements a high-performance, container-agnostic FLAC decoder
 * that integrates seamlessly with PsyMP3's AudioCodec architecture. Key design
 * principles include:
 * 
 * 1. **Container Independence**: Works with any demuxer providing MediaChunk data
 * 2. **RFC 9639 Compliance**: Strict adherence to FLAC specification
 * 3. **Performance Optimization**: Real-time decoding for high-resolution files
 * 4. **Threading Safety**: Comprehensive thread-safe design with deadlock prevention
 * 5. **Memory Efficiency**: Optimized buffer management and minimal allocations
 * 6. **Error Resilience**: Robust handling of corrupted frames and edge cases
 * 
 * PERFORMANCE CHARACTERISTICS:
 * ===========================
 * - **44.1kHz/16-bit**: <1% CPU usage on target hardware
 * - **96kHz/24-bit**: Real-time performance without dropouts
 * - **Frame Processing**: <100μs per frame for real-time requirements
 * - **Memory Usage**: Bounded allocation with pre-allocated buffers
 * - **SIMD Optimization**: Vectorized bit depth conversion where available
 * 
 * BIT DEPTH CONVERSION ALGORITHMS:
 * ===============================
 * The codec implements optimized algorithms for converting FLAC samples to 16-bit PCM:
 * 
 * - **8-bit → 16-bit**: Bit-shift upscaling (sample << 8) for maximum performance
 * - **16-bit → 16-bit**: Direct copy with no conversion overhead
 * - **24-bit → 16-bit**: Optimized downscaling with optional triangular dithering
 * - **32-bit → 16-bit**: Arithmetic right-shift with overflow protection
 * - **SIMD Variants**: SSE2/NEON optimized batch conversion for high throughput
 * 
 * CHANNEL PROCESSING:
 * ==================
 * Supports all FLAC channel configurations per RFC 9639:
 * 
 * - **Independent Channels**: Standard mono/stereo processing
 * - **Left-Side Stereo**: Left + (Left-Right) difference reconstruction
 * - **Right-Side Stereo**: (Left+Right) sum + Right reconstruction
 * - **Mid-Side Stereo**: (Left+Right) sum + (Left-Right) difference reconstruction
 * - **Multi-Channel**: Up to 8 channels with proper interleaving
 * 
 * VARIABLE BLOCK SIZE HANDLING:
 * ============================
 * Efficiently handles FLAC's variable block size feature:
 * 
 * - **Fixed Block Sizes**: Optimized processing for standard sizes (192, 576, 1152, etc.)
 * - **Variable Block Sizes**: Dynamic buffer adaptation within same stream
 * - **Block Size Range**: 16-65535 samples per RFC 9639
 * - **Adaptive Buffering**: Automatic buffer resizing based on detected patterns
 * - **Performance Optimization**: Pre-allocation for common block sizes
 * 
 * @thread_safety This class is thread-safe. All public methods follow the
 *                public/private lock pattern to prevent deadlocks.
 * 
 * THREADING SAFETY GUARANTEES:
 * ============================
 * - All public methods acquire locks using RAII guards (std::lock_guard)
 * - All public methods call corresponding _unlocked private methods
 * - Internal method calls always use _unlocked versions to prevent deadlocks
 * - Exception safety is maintained through RAII lock guards
 * - No callbacks are invoked while holding internal locks
 * - Atomic variables used for frequently accessed state (current sample position)
 * 
 * LOCK ACQUISITION ORDER (to prevent deadlocks):
 * =============================================
 * 1. m_state_mutex (for codec state and configuration)
 * 2. m_thread_mutex (for threading state management)
 * 3. m_async_mutex (for asynchronous processing queues)
 * 4. m_decoder_mutex (for libFLAC decoder operations)
 * 5. m_buffer_mutex (for output buffer management)
 * 6. m_input_mutex (for input queue and frame reconstruction)
 * 7. libFLAC internal locks (managed by libFLAC)
 * 
 * CRITICAL THREADING RULES:
 * ========================
 * - NEVER acquire locks in different order to prevent deadlocks!
 * - NEVER call public methods from _unlocked methods!
 * - ALWAYS use RAII lock guards for exception safety
 * - NEVER invoke callbacks while holding internal locks
 * - Use atomic variables for high-frequency read-only access
 * 
 * USAGE EXAMPLE:
 * =============
 * @code
 * // Initialize codec with stream information
 * StreamInfo stream_info;
 * stream_info.sample_rate = 44100;
 * stream_info.channels = 2;
 * stream_info.bits_per_sample = 16;
 * 
 * FLACCodec codec(stream_info);
 * if (!codec.initialize()) {
 *     // Handle initialization error
 *     return false;
 * }
 * 
 * // Decode FLAC frames
 * MediaChunk chunk = demuxer.getNextChunk();
 * AudioFrame frame = codec.decode(chunk);
 * 
 * if (frame.sample_count > 0) {
 *     // Process decoded PCM samples
 *     processAudio(frame.samples);
 * }
 * 
 * // Flush remaining samples at end of stream
 * AudioFrame final_frame = codec.flush();
 * @endcode
 * 
 * TROUBLESHOOTING GUIDE:
 * =====================
 * Common issues and solutions:
 * 
 * 1. **Initialization Failures**:
 *    - Verify StreamInfo contains valid FLAC parameters
 *    - Check libFLAC library availability (HAVE_FLAC defined)
 *    - Ensure sample rate is 1-655350 Hz per RFC 9639
 *    - Verify channel count is 1-8 per RFC 9639
 *    - Check bit depth is 4-32 bits per RFC 9639
 * 
 * 2. **Decoding Errors**:
 *    - Corrupted frames: Codec will skip and continue
 *    - CRC failures: Logged as warnings, decoded data still used
 *    - Sync loss: Decoder automatically resets and resynchronizes
 *    - Memory errors: Check available system memory
 * 
 * 3. **Performance Issues**:
 *    - High CPU usage: Check for excessive buffer reallocations
 *    - Dropouts: Verify real-time performance requirements met
 *    - Memory usage: Monitor buffer sizes and allocation patterns
 *    - Threading contention: Review lock acquisition patterns
 * 
 * 4. **Quality Issues**:
 *    - Bit depth conversion artifacts: Enable dithering for 24→16 bit
 *    - Channel reconstruction errors: Verify stereo mode handling
 *    - Dynamic range loss: Check conversion algorithm selection
 *    - Clipping: Verify overflow protection in 32→16 bit conversion
 * 
 * PERFORMANCE TUNING RECOMMENDATIONS:
 * ==================================
 * 1. **Buffer Management**:
 *    - Use adaptive buffer sizing for variable block sizes
 *    - Pre-allocate buffers based on maximum expected block size
 *    - Monitor buffer utilization and adjust watermarks
 * 
 * 2. **SIMD Optimization**:
 *    - Enable SSE2 on x86/x64 platforms for batch conversion
 *    - Use NEON on ARM platforms for vectorized processing
 *    - Verify SIMD code paths are being used in performance-critical sections
 * 
 * 3. **Threading Optimization**:
 *    - Minimize lock contention by using atomic variables for read-only access
 *    - Consider asynchronous processing for high-throughput scenarios
 *    - Profile lock acquisition patterns to identify bottlenecks
 * 
 * 4. **Memory Optimization**:
 *    - Use memory pools for frequently allocated/deallocated buffers
 *    - Implement buffer reuse strategies to minimize allocation overhead
 *    - Monitor memory fragmentation in long-running scenarios
 */
class FLACCodec : public AudioCodec {
public:
    explicit FLACCodec(const StreamInfo& stream_info);
    ~FLACCodec() override;
    
    // AudioCodec interface implementation (thread-safe public methods)
    
    /**
     * @brief Initialize the FLAC codec with stream parameters
     * 
     * Configures the codec from StreamInfo parameters and initializes the libFLAC
     * decoder with performance optimizations. This method must be called before
     * any decoding operations.
     * 
     * INITIALIZATION PROCESS:
     * 1. Validates StreamInfo parameters against RFC 9639 specification
     * 2. Configures internal state (sample rate, channels, bit depth)
     * 3. Initializes libFLAC decoder with performance settings
     * 4. Pre-allocates buffers based on maximum expected block size
     * 5. Sets up bit depth conversion algorithms
     * 6. Initializes performance monitoring
     * 
     * PERFORMANCE OPTIMIZATIONS:
     * - Disables MD5 checking in libFLAC for maximum performance
     * - Pre-allocates buffers to minimize runtime allocations
     * - Configures optimal decoder settings for real-time processing
     * - Sets up SIMD-optimized conversion paths where available
     * 
     * @return true if initialization successful, false on error
     * 
     * @thread_safety Thread-safe. Uses m_state_mutex for synchronization.
     * 
     * @note This method follows the public/private lock pattern:
     *       - Acquires m_state_mutex using RAII guard
     *       - Calls initialize_unlocked() for actual implementation
     * 
     * COMMON FAILURE REASONS:
     * - Invalid sample rate (must be 1-655350 Hz per RFC 9639)
     * - Invalid channel count (must be 1-8 per RFC 9639)
     * - Invalid bit depth (must be 4-32 bits per RFC 9639)
     * - libFLAC initialization failure
     * - Memory allocation failure
     * - Missing FLAC library support (HAVE_FLAC not defined)
     * 
     * @see initialize_unlocked() for implementation details
     * @see configureFromStreamInfo_unlocked() for parameter validation
     * @see initializeFLACDecoder_unlocked() for libFLAC setup
     */
    bool initialize() override;
    
    /**
     * @brief Decode a FLAC frame from MediaChunk data
     * 
     * Processes a complete FLAC frame contained in a MediaChunk and returns
     * decoded 16-bit PCM samples as an AudioFrame. This is the primary decoding
     * method for real-time playback.
     * 
     * DECODING PROCESS:
     * 1. Validates input MediaChunk contains complete FLAC frame
     * 2. Feeds frame data to libFLAC decoder
     * 3. Processes libFLAC callbacks (read, write, metadata, error)
     * 4. Converts decoded samples to 16-bit PCM using optimized algorithms
     * 5. Handles channel reconstruction for stereo modes
     * 6. Creates AudioFrame with proper timing information
     * 7. Updates performance statistics
     * 
     * BIT DEPTH CONVERSION:
     * - 8-bit: Upscaled using bit shift (sample << 8)
     * - 16-bit: Direct copy (no conversion needed)
     * - 24-bit: Downscaled with optional dithering (sample >> 8)
     * - 32-bit: Downscaled with overflow protection (clamp + shift)
     * - SIMD: Vectorized conversion for batch processing where available
     * 
     * CHANNEL PROCESSING:
     * - Independent: Each channel processed separately
     * - Left-Side: Left + (Left-Right) difference reconstruction
     * - Right-Side: (Left+Right) sum + Right reconstruction
     * - Mid-Side: (Left+Right) sum + (Left-Right) difference reconstruction
     * 
     * @param chunk MediaChunk containing complete FLAC frame data
     * @return AudioFrame with decoded 16-bit PCM samples, empty on error
     * 
     * @thread_safety Thread-safe. Uses multiple mutexes for synchronization.
     * 
     * @note This method follows the public/private lock pattern:
     *       - Acquires m_state_mutex for codec state access
     *       - Acquires m_buffer_mutex for output buffer operations
     *       - Calls decode_unlocked() for actual implementation
     * 
     * ERROR HANDLING:
     * - Corrupted frames: Skipped, returns silence frame
     * - CRC failures: Logged as warning, decoded data still returned
     * - Sync loss: Decoder reset and resynchronization attempted
     * - Memory errors: Returns empty AudioFrame
     * - Invalid input: Returns empty AudioFrame
     * 
     * PERFORMANCE CHARACTERISTICS:
     * - Target: <100μs per frame for real-time requirements
     * - Memory: Minimal allocations during steady-state operation
     * - CPU: <1% usage for 44.1kHz/16-bit, real-time for 96kHz/24-bit
     * 
     * @see decode_unlocked() for implementation details
     * @see processFrameData_unlocked() for frame processing
     * @see convertSamples_unlocked() for bit depth conversion
     * @see processChannelAssignment_unlocked() for channel processing
     */
    AudioFrame decode(const MediaChunk& chunk) override;
    
    /**
     * @brief Flush remaining decoded samples from internal buffers
     * 
     * Outputs any remaining decoded samples that are buffered internally.
     * This method should be called at the end of a stream to ensure all
     * audio data is retrieved.
     * 
     * FLUSH PROCESS:
     * 1. Processes any remaining data in libFLAC decoder
     * 2. Converts and outputs buffered samples
     * 3. Clears internal buffers
     * 4. Updates final sample position
     * 5. Prepares codec for potential reset/reuse
     * 
     * @return AudioFrame with remaining samples, empty if no samples buffered
     * 
     * @thread_safety Thread-safe. Uses m_buffer_mutex for synchronization.
     * 
     * @note This method follows the public/private lock pattern:
     *       - Acquires m_buffer_mutex for buffer operations
     *       - Calls flush_unlocked() for actual implementation
     * 
     * TYPICAL USAGE:
     * - Called at end of stream to retrieve final samples
     * - Called before reset() to ensure no data loss
     * - Called during seeking to clear stale buffered data
     * 
     * @see flush_unlocked() for implementation details
     */
    AudioFrame flush() override;
    
    /**
     * @brief Reset codec state for seeking or new stream
     * 
     * Resets the codec to initial state, clearing all buffers and resetting
     * the libFLAC decoder. This enables sample-accurate seeking by allowing
     * the codec to start decoding from any FLAC frame.
     * 
     * RESET PROCESS:
     * 1. Clears all internal buffers (input, output, decode)
     * 2. Resets libFLAC decoder state
     * 3. Resets sample position tracking
     * 4. Clears error state
     * 5. Resets performance statistics (optional)
     * 6. Prepares for new decoding sequence
     * 
     * @thread_safety Thread-safe. Uses multiple mutexes for synchronization.
     * 
     * @note This method follows the public/private lock pattern:
     *       - Acquires m_state_mutex for codec state
     *       - Acquires m_buffer_mutex for buffer operations
     *       - Calls reset_unlocked() for actual implementation
     * 
     * SEEKING SUPPORT:
     * The reset functionality enables sample-accurate seeking by:
     * - Clearing decoder state without losing configuration
     * - Allowing decoding to start from any FLAC frame
     * - Maintaining consistent output format
     * - Preserving codec configuration and optimization settings
     * 
     * @see reset_unlocked() for implementation details
     * @see resetDecoderState_unlocked() for decoder reset
     */
    void reset() override;
    
    /**
     * @brief Get codec name identifier
     * 
     * Returns the codec identifier used by MediaFactory for codec selection
     * and registration. This identifier is used to match FLAC streams with
     * this codec implementation.
     * 
     * @return "flac" - the standard FLAC codec identifier
     * 
     * @thread_safety Thread-safe (no synchronization needed - returns constant)
     * 
     * @note This method is used by:
     *       - MediaFactory for codec registration and selection
     *       - Demuxers for codec capability queries
     *       - Debug logging and error reporting
     *       - Codec enumeration and discovery
     */
    std::string getCodecName() const override { return "flac"; }
    
    /**
     * @brief Check if codec can decode the given stream
     * 
     * Validates whether this codec can handle the stream described by the
     * StreamInfo parameters. Performs comprehensive validation against
     * RFC 9639 FLAC specification requirements.
     * 
     * VALIDATION CHECKS:
     * 1. Codec name matches "flac" (case-insensitive)
     * 2. Sample rate within RFC 9639 limits (1-655350 Hz)
     * 3. Channel count within RFC 9639 limits (1-8 channels)
     * 4. Bit depth within RFC 9639 limits (4-32 bits)
     * 5. libFLAC library availability (HAVE_FLAC defined)
     * 6. System resource availability for decoding
     * 
     * @param stream_info Stream parameters to validate
     * @return true if codec can decode the stream, false otherwise
     * 
     * @thread_safety Thread-safe. Uses m_state_mutex for synchronization.
     * 
     * @note This method follows the public/private lock pattern:
     *       - Acquires m_state_mutex for thread safety
     *       - Calls canDecode_unlocked() for actual validation
     * 
     * PERFORMANCE CONSIDERATIONS:
     * - Lightweight validation suitable for frequent calls
     * - No decoder initialization or resource allocation
     * - Fast parameter range checking
     * - Minimal computational overhead
     * 
     * @see canDecode_unlocked() for implementation details
     * @see validateConfiguration_unlocked() for detailed validation
     */
    bool canDecode(const StreamInfo& stream_info) const override;
    
    // FLAC-specific public interface
    
    /**
     * @brief Check if codec supports seek reset functionality
     * 
     * Indicates whether this codec implementation supports sample-accurate
     * seeking through the reset() method. FLAC's frame-based structure
     * enables seeking to any frame boundary.
     * 
     * @return true - FLAC codec always supports seek reset
     * 
     * @thread_safety Thread-safe (no synchronization needed - returns constant)
     * 
     * SEEK RESET CAPABILITY:
     * The FLAC codec supports seek reset because:
     * - FLAC frames are self-contained and can be decoded independently
     * - No inter-frame dependencies (unlike some predictive codecs)
     * - Frame headers contain all necessary decoding parameters
     * - libFLAC decoder can be reset without losing configuration
     * 
     * USAGE IN SEEKING:
     * 1. Demuxer seeks to target FLAC frame
     * 2. Calls codec.reset() to clear state
     * 3. Begins decoding from new position
     * 4. Sample-accurate playback resumes
     * 
     * @see reset() for seek reset implementation
     */
    bool supportsSeekReset() const;
    
    /**
     * @brief Get current sample position in stream
     * 
     * Returns the current playback position as a sample number from the
     * beginning of the stream. This value is updated after each successful
     * decode operation and is used for timing and seeking calculations.
     * 
     * @return Current sample position (0-based)
     * 
     * @thread_safety Thread-safe. Uses atomic variable for lock-free access.
     * 
     * @note This method uses atomic access for high-frequency reads without
     *       lock overhead. The position is updated atomically after each
     *       successful decode operation.
     * 
     * POSITION TRACKING:
     * - Initialized to 0 at codec creation
     * - Incremented by block_size after each decoded frame
     * - Reset to 0 by reset() method
     * - Maintained across variable block sizes
     * - Used for AudioFrame timestamp calculation
     * 
     * ACCURACY GUARANTEES:
     * - Sample-accurate position tracking
     * - Consistent across variable block sizes
     * - Atomic updates prevent race conditions
     */
    uint64_t getCurrentSample() const;
    
    /**
     * @brief Get codec performance and debugging statistics
     * 
     * Returns comprehensive statistics about codec performance, including
     * decoding efficiency, error counts, memory usage, and timing information.
     * This information is useful for performance monitoring and debugging.
     * 
     * @return FLACCodecStats structure with current statistics
     * 
     * @thread_safety Thread-safe. Uses m_state_mutex for synchronization.
     * 
     * STATISTICS INCLUDED:
     * - Frame and sample counts
     * - Error breakdown (CRC, sync, memory, libFLAC)
     * - Performance timing (average, min, max decode times)
     * - Memory usage information
     * - Decode efficiency metrics
     * 
     * @see FLACCodecStats for detailed field descriptions
     */
    FLACCodecStats getStats() const;
    
    // RFC 9639 compliance testing methods (public for unit testing)
    
    /**
     * @brief Test method for RFC 9639 bit depth validation
     * 
     * Public wrapper for validateBitDepthRFC9639_unlocked() to enable unit testing
     * of RFC 9639 bit depth compliance validation.
     * 
     * @param bits_per_sample Bit depth to validate (4-32 bits per RFC 9639)
     * @return true if bit depth is valid per RFC 9639, false otherwise
     * 
     * @thread_safety Thread-safe
     */
    bool testValidateBitDepthRFC9639(uint16_t bits_per_sample) const {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        return validateBitDepthRFC9639_unlocked(bits_per_sample);
    }
    
    /**
     * @brief Test method for sample format consistency validation
     * 
     * Public wrapper for validateSampleFormatConsistency_unlocked() to enable
     * unit testing of sample format consistency validation.
     * 
     * @param frame FLAC frame to validate against current codec configuration
     * @return true if sample format is consistent, false if mismatch detected
     * 
     * @thread_safety Thread-safe
     */
    bool testValidateSampleFormatConsistency(const FLAC__Frame* frame) const {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        return validateSampleFormatConsistency_unlocked(frame);
    }
    
    /**
     * @brief Test method for reserved bit depth values validation
     * 
     * Public wrapper for validateReservedBitDepthValues_unlocked() to enable
     * unit testing of reserved bit depth value handling.
     * 
     * @param bits_per_sample Bit depth value to check for reserved status
     * @return true if bit depth is not reserved, false if reserved value detected
     * 
     * @thread_safety Thread-safe
     */
    bool testValidateReservedBitDepthValues(uint16_t bits_per_sample) const {
        return validateReservedBitDepthValues_unlocked(bits_per_sample);
    }
    
    /**
     * @brief Test method for proper sign extension
     * 
     * Public wrapper for applyProperSignExtension_unlocked() to enable
     * unit testing of RFC 9639 compliant sign extension.
     * 
     * @param sample Raw sample value from FLAC decoder
     * @param source_bits Actual bit depth of the sample (4-31 bits)
     * @return Properly sign-extended 32-bit sample value
     * 
     * @thread_safety Thread-safe
     */
    FLAC__int32 testApplyProperSignExtension(FLAC__int32 sample, uint16_t source_bits) const {
        return applyProperSignExtension_unlocked(sample, source_bits);
    }
    
    /**
     * @brief Test method for bit-perfect reconstruction validation
     * 
     * Public wrapper for validateBitPerfectReconstruction_unlocked() to enable
     * unit testing of lossless reconstruction validation.
     * 
     * @param original Original FLAC samples before conversion
     * @param converted Converted 16-bit PCM samples
     * @param sample_count Number of samples to validate
     * @param source_bits Original bit depth of FLAC samples
     * @return true if reconstruction is bit-perfect within conversion limits
     * 
     * @thread_safety Thread-safe
     */
    bool testValidateBitPerfectReconstruction(const FLAC__int32* original, 
                                              const int16_t* converted, 
                                              size_t sample_count, 
                                              uint16_t source_bits) const {
        return validateBitPerfectReconstruction_unlocked(original, converted, sample_count, source_bits);
    }
    
    /**
     * @brief Test method for audio quality metrics calculation
     * 
     * Public wrapper for calculateAudioQualityMetrics_unlocked() to enable
     * unit testing of audio quality analysis.
     * 
     * @param samples Converted 16-bit PCM samples to analyze
     * @param sample_count Number of samples in the buffer
     * @param reference Optional reference samples for comparison (original FLAC)
     * @param reference_bits Bit depth of reference samples (if provided)
     * @return AudioQualityMetrics structure with comprehensive quality analysis
     * 
     * @thread_safety Thread-safe
     */
    AudioQualityMetrics testCalculateAudioQualityMetrics(const int16_t* samples, 
                                                         size_t sample_count, 
                                                         const FLAC__int32* reference = nullptr,
                                                         uint16_t reference_bits = 16) const {
        return calculateAudioQualityMetrics_unlocked(samples, sample_count, reference, reference_bits);
    }
    
    /**
     * @brief Test method for bit depth conversion functions
     * 
     * Public wrappers for bit depth conversion methods to enable unit testing.
     */
    int16_t testConvert8BitTo16Bit(FLAC__int32 sample) const {
        return convert8BitTo16Bit(sample);
    }
    
    int16_t testConvert24BitTo16Bit(FLAC__int32 sample) const {
        return convert24BitTo16Bit(sample);
    }
    
    int16_t testConvert32BitTo16Bit(FLAC__int32 sample) const {
        return convert32BitTo16Bit(sample);
    }
    
    // CRC Validation Control (RFC 9639 Compliance)
    
    /**
     * @brief Enable or disable CRC validation per RFC 9639
     * 
     * Controls whether the codec performs CRC validation on FLAC frames.
     * CRC validation provides data integrity checking but has performance
     * overhead. RFC 9639 requires CRC validation for full compliance.
     * 
     * CRC VALIDATION TYPES:
     * - Header CRC-8: Validates frame header integrity (polynomial x^8 + x^2 + x^1 + x^0)
     * - Footer CRC-16: Validates entire frame integrity (polynomial x^16 + x^15 + x^2 + x^0)
     * 
     * @param enabled true to enable CRC validation, false to disable
     * 
     * @thread_safety Thread-safe. Uses m_state_mutex for synchronization.
     * 
     * PERFORMANCE CONSIDERATIONS:
     * - Enabling CRC validation adds ~5-10% CPU overhead
     * - Recommended for untrusted sources or debugging
     * - Can be disabled for trusted sources to improve performance
     * - Automatically disabled if excessive errors occur
     * 
     * RFC 9639 COMPLIANCE:
     * - Full RFC compliance requires CRC validation enabled
     * - Decoders MAY disable validation for performance reasons
     * - Error recovery strategies differ based on validation mode
     * 
     * @see setCRCValidationStrict() for strict error handling mode
     * @see getCRCValidationEnabled() to query current state
     */
    void setCRCValidationEnabled(bool enabled);
    
    /**
     * @brief Check if CRC validation is currently enabled
     * 
     * Returns the current state of CRC validation. Note that validation
     * may be automatically disabled if excessive errors occur, even if
     * originally enabled.
     * 
     * @return true if CRC validation is enabled and active, false otherwise
     * 
     * @thread_safety Thread-safe. Uses m_state_mutex for synchronization.
     * 
     * AUTOMATIC DISABLING:
     * CRC validation may be automatically disabled if:
     * - Error rate exceeds threshold (default 10 errors)
     * - Systematic CRC failures indicate stream corruption
     * - Performance requirements cannot be met
     * 
     * @see setCRCValidationEnabled() to control validation
     * @see getCRCErrorCount() to check error statistics
     */
    bool getCRCValidationEnabled() const;
    
    /**
     * @brief Set strict CRC validation mode per RFC 9639
     * 
     * Controls error handling behavior when CRC validation fails.
     * In strict mode, frames with CRC errors are rejected completely.
     * In permissive mode, frames with CRC errors are used but logged.
     * 
     * STRICT MODE (strict=true):
     * - Frames with CRC errors are completely rejected
     * - Silence is output for rejected frames
     * - Provides maximum data integrity assurance
     * - May cause audio dropouts with corrupted streams
     * 
     * PERMISSIVE MODE (strict=false, default):
     * - Frames with CRC errors are used but logged as warnings
     * - Audio playback continues with potentially corrupted data
     * - Better user experience with slightly corrupted streams
     * - RFC 9639 compliant error recovery strategy
     * 
     * @param strict true for strict mode, false for permissive mode
     * 
     * @thread_safety Thread-safe. Uses m_state_mutex for synchronization.
     * 
     * RFC 9639 COMPLIANCE:
     * Both modes are RFC 9639 compliant. The specification allows decoders
     * to choose appropriate error recovery strategies based on application
     * requirements.
     * 
     * @see setCRCValidationEnabled() to enable/disable validation
     * @see getCRCValidationStrict() to query current mode
     */
    void setCRCValidationStrict(bool strict);
    
    /**
     * @brief Check if strict CRC validation mode is enabled
     * 
     * Returns whether the codec is operating in strict CRC validation mode.
     * This affects how CRC validation failures are handled.
     * 
     * @return true if strict mode enabled, false for permissive mode
     * 
     * @thread_safety Thread-safe. Uses m_state_mutex for synchronization.
     * 
     * @see setCRCValidationStrict() for mode descriptions
     */
    bool getCRCValidationStrict() const;
    
    /**
     * @brief Get current CRC error count
     * 
     * Returns the total number of CRC validation failures encountered
     * during decoding. This includes both header and footer CRC errors.
     * 
     * @return Total CRC error count since codec initialization
     * 
     * @thread_safety Thread-safe. Uses atomic access for performance.
     * 
     * ERROR TRACKING:
     * - Incremented for each CRC validation failure
     * - Includes both header CRC-8 and footer CRC-16 errors
     * - Used for automatic validation disabling threshold
     * - Reset by reset() method
     * 
     * @see getStats() for comprehensive error breakdown
     */
    size_t getCRCErrorCount() const;
    
    /**
     * @brief Set CRC error threshold for automatic validation disabling
     * 
     * Sets the maximum number of CRC errors before validation is automatically
     * disabled. This prevents excessive performance impact from systematically
     * corrupted streams while maintaining data integrity for good streams.
     * 
     * @param threshold Maximum CRC errors before auto-disable (0 = never disable)
     * 
     * @thread_safety Thread-safe. Uses m_state_mutex for synchronization.
     * 
     * AUTOMATIC DISABLING BEHAVIOR:
     * - When error count reaches threshold, validation is disabled
     * - Prevents performance degradation from corrupted streams
     * - Can be re-enabled manually with setCRCValidationEnabled(true)
     * - Default threshold is 10 errors
     * - Setting to 0 disables automatic disabling
     * 
     * PERFORMANCE CONSIDERATIONS:
     * - Lower thresholds improve performance with corrupted streams
     * - Higher thresholds provide better data integrity assurance
     * - Consider stream quality and application requirements
     * 
     * @see setCRCValidationEnabled() to manually control validation
     * @see getCRCErrorCount() to monitor current error count
     */
    void setCRCErrorThreshold(size_t threshold);
    
    /**
     * @brief Get current sample position in stream
     * 
     * Returns the current playback position as a sample number from the
     * beginning of the stream. This value is updated after each successful
     * decode operation and is used for timing and seeking calculations.
     * 
     * @return Current sample position (0-based)
     * 
     * @thread_safety Thread-safe. Uses atomic variable for lock-free access.
     * 
     * @note This method uses atomic access for high-frequency reads without
     *       lock overhead. The position is updated atomically after each
     *       successful decode operation.
     * 
     * POSITION TRACKING:
     * - Initialized to 0 at codec creation
     * - Incremented by block_size after each decoded frame
     * - Reset to 0 by reset() method
     * - Maintained across variable block sizes
     * - Used for AudioFrame timestamp calculation
     */
    
    /**
     * @brief Get comprehensive codec performance statistics
     * 
     * Returns detailed performance and debugging statistics collected during
     * codec operation. These statistics are useful for performance monitoring,
     * optimization, and troubleshooting.
     * 
     * STATISTICS INCLUDED:
     * - Frame and sample counts
     * - Processing times and efficiency metrics
     * - Memory usage and allocation statistics
     * - Error counts and types
     * - Buffer utilization metrics
     * - Conversion operation counts
     * 
     * @return FLACCodecStats structure with current statistics
     * 
     * @thread_safety Thread-safe. Uses m_state_mutex for synchronization.
     * 
     * @note Statistics are continuously updated during operation and can
     *       be retrieved at any time for monitoring purposes.
     * 
     * PERFORMANCE MONITORING:
     * Use these statistics to:
     * - Monitor real-time performance requirements
     * - Identify performance bottlenecks
     * - Track memory usage patterns
     * - Analyze error rates and types
     * - Optimize buffer sizes and allocation strategies
     * 
     * DEBUGGING APPLICATIONS:
     * - Troubleshoot decoding issues
     * - Verify codec efficiency
     * - Monitor resource utilization
     * - Validate optimization effectiveness
     */
    
    // Quality validation and accuracy testing interface
    
    /**
     * @brief Validate bit-perfect decoding accuracy
     * 
     * Compares decoded samples with reference samples to verify bit-perfect
     * accuracy. This is used for testing and validation of the codec
     * implementation against known reference data.
     * 
     * @param reference_samples Known correct PCM samples
     * @param decoded_samples Samples decoded by this codec
     * @return true if samples match exactly (bit-perfect), false otherwise
     * 
     * @thread_safety Thread-safe (no state modification)
     * 
     * BIT-PERFECT VALIDATION:
     * - Compares every sample value exactly
     * - Accounts for sample count differences
     * - Validates timing and channel ordering
     * - Used for codec accuracy verification
     * 
     * TESTING APPLICATIONS:
     * - Regression testing against reference implementations
     * - Validation of bit depth conversion accuracy
     * - Verification of channel processing correctness
     * - Quality assurance for codec releases
     */
    bool validateBitPerfectDecoding(const std::vector<int16_t>& reference_samples, 
                                   const std::vector<int16_t>& decoded_samples) const;
    
    /**
     * @brief Calculate Signal-to-Noise Ratio between reference and decoded samples
     * 
     * Computes SNR in decibels to quantify the quality of decoded audio
     * compared to a reference. Higher SNR indicates better quality.
     * 
     * @param reference_samples Reference PCM samples
     * @param decoded_samples Decoded PCM samples
     * @return SNR in decibels (higher is better)
     * 
     * @thread_safety Thread-safe (no state modification)
     * 
     * SNR CALCULATION:
     * SNR = 20 * log10(RMS_signal / RMS_noise)
     * where noise = reference - decoded
     * 
     * QUALITY THRESHOLDS:
     * - >120 dB: Bit-perfect (theoretical maximum for 16-bit)
     * - >90 dB: Excellent quality
     * - >60 dB: Good quality
     * - <40 dB: Poor quality (audible artifacts)
     */
    double calculateSignalToNoiseRatio(const std::vector<int16_t>& reference_samples,
                                      const std::vector<int16_t>& decoded_samples) const;
    
    /**
     * @brief Calculate Total Harmonic Distortion of audio samples
     * 
     * Measures the harmonic distortion present in decoded audio samples.
     * Lower THD indicates better audio quality and more accurate decoding.
     * 
     * @param samples PCM samples to analyze
     * @return THD as percentage (lower is better)
     * 
     * @thread_safety Thread-safe (no state modification)
     * 
     * THD CALCULATION:
     * Analyzes frequency spectrum to identify harmonic distortion
     * components relative to fundamental frequencies.
     * 
     * QUALITY THRESHOLDS:
     * - <0.01%: Excellent (inaudible distortion)
     * - <0.1%: Very good
     * - <1%: Acceptable
     * - >1%: Poor (audible distortion)
     */
    double calculateTotalHarmonicDistortion(const std::vector<int16_t>& samples) const;
    
    /**
     * @brief Validate bit depth conversion quality
     * 
     * Analyzes the quality of bit depth conversion by comparing source
     * samples with converted output. Validates conversion algorithms
     * and identifies potential quality issues.
     * 
     * @param source_samples Original FLAC samples (various bit depths)
     * @param converted_samples Converted 16-bit PCM samples
     * @param source_bit_depth Original bit depth (8, 16, 24, or 32)
     * @return true if conversion quality meets standards, false otherwise
     * 
     * @thread_safety Thread-safe (no state modification)
     * 
     * CONVERSION QUALITY VALIDATION:
     * - Verifies proper scaling and range mapping
     * - Checks for clipping and overflow
     * - Validates dithering effectiveness (24→16 bit)
     * - Ensures no DC offset introduction
     * - Verifies dynamic range preservation
     * 
     * QUALITY CRITERIA:
     * - No clipping or overflow
     * - Proper scaling for bit depth
     * - Minimal quantization noise
     * - Preserved dynamic range
     * - Correct dithering application
     */
    bool validateConversionQuality(const std::vector<FLAC__int32>& source_samples,
                                  const std::vector<int16_t>& converted_samples,
                                  uint16_t source_bit_depth) const;
    
    /**
     * @brief Validate dynamic range of decoded samples
     * 
     * Analyzes the dynamic range of decoded audio to ensure proper
     * utilization of the available bit depth and absence of compression
     * artifacts or range limitations.
     * 
     * @param samples PCM samples to analyze
     * @return true if dynamic range is adequate, false otherwise
     * 
     * @thread_safety Thread-safe (no state modification)
     * 
     * DYNAMIC RANGE ANALYSIS:
     * - Measures peak-to-RMS ratio
     * - Analyzes amplitude distribution
     * - Detects compression artifacts
     * - Validates bit depth utilization
     * 
     * VALIDATION CRITERIA:
     * - Adequate peak-to-RMS ratio (>20 dB typical)
     * - Full bit depth utilization when appropriate
     * - No artificial limiting or compression
     * - Proper noise floor characteristics
     */
    bool validateDynamicRange(const std::vector<int16_t>& samples) const;
    
    /**
     * @brief Calculate comprehensive audio quality metrics
     * 
     * Performs comprehensive analysis of decoded audio samples and returns
     * detailed quality metrics including SNR, THD, dynamic range, and
     * various other audio quality indicators.
     * 
     * @param samples PCM samples to analyze
     * @return AudioQualityMetrics structure with comprehensive analysis
     * 
     * @thread_safety Thread-safe (no state modification)
     * 
     * COMPREHENSIVE ANALYSIS:
     * - Signal-to-noise ratio calculation
     * - Total harmonic distortion measurement
     * - Dynamic range analysis
     * - Peak and RMS amplitude measurement
     * - DC offset detection
     * - Zero crossing analysis
     * - Clipping detection
     * - Bit-perfect validation flag
     * 
     * APPLICATIONS:
     * - Automated quality testing
     * - Performance benchmarking
     * - Codec validation and verification
     * - Audio quality monitoring
     * - Regression testing
     * 
     * @see AudioQualityMetrics for detailed metric descriptions
     */
    AudioQualityMetrics calculateQualityMetrics(const std::vector<int16_t>& samples) const;
    
    // Friend class for callback access
    friend class FLACStreamDecoder;

private:
    // Thread safety - documented lock acquisition order above
    mutable std::mutex m_state_mutex;    ///< Protects codec state and configuration
    mutable std::mutex m_buffer_mutex;   ///< Protects output buffer operations
    mutable std::mutex m_decoder_mutex;  ///< Protects libFLAC decoder operations
    std::atomic<bool> m_error_state{false}; ///< Thread-safe error state flag
    
    // FLAC decoder state (protected by m_state_mutex)
    std::unique_ptr<FLACStreamDecoder> m_decoder;
    bool m_decoder_initialized = false;
    
    // Stream configuration (protected by m_state_mutex)
    uint32_t m_sample_rate = 0;
    uint16_t m_channels = 0;
    uint16_t m_bits_per_sample = 0;
    uint64_t m_total_samples = 0;
    
    // STREAMINFO block size constraints (protected by m_state_mutex)
    uint32_t m_min_block_size = 0;  ///< Minimum block size from STREAMINFO
    uint32_t m_max_block_size = 0;  ///< Maximum block size from STREAMINFO
    
    // Decoding state (protected by m_state_mutex)
    std::atomic<uint64_t> m_current_sample{0};  ///< Atomic for fast reads
    uint32_t m_last_block_size = 0;
    bool m_stream_finished = false;
    
    // Variable block size handling (protected by m_state_mutex)
    bool m_variable_block_size = false;         ///< True if stream uses variable block sizes
    uint32_t m_current_block_size = 0;          ///< Current frame's block size
    
    // Block size optimization state (protected by m_state_mutex)
    static constexpr uint32_t STANDARD_BLOCK_SIZES[] = {
        192, 576, 1152, 2304, 4608, 9216, 18432, 36864  ///< Standard FLAC block sizes
    };
    static constexpr size_t NUM_STANDARD_BLOCK_SIZES = sizeof(STANDARD_BLOCK_SIZES) / sizeof(STANDARD_BLOCK_SIZES[0]);
    uint32_t m_preferred_block_size = 0;        ///< Detected preferred block size for optimization
    
    // Variable block size adaptation state (protected by m_state_mutex)
    uint32_t m_previous_block_size = 0;         ///< Previous frame's block size for transition detection
    uint32_t m_block_size_changes = 0;          ///< Count of block size changes for adaptation
    uint64_t m_total_samples_processed = 0;     ///< Total samples processed for timing consistency
    bool m_adaptive_buffering_enabled = true;   ///< Enable adaptive buffer resizing
    
    // Performance tracking for variable block sizes
    uint32_t m_smallest_block_seen = UINT32_MAX; ///< Smallest block size encountered
    uint32_t m_largest_block_seen = 0;           ///< Largest block size encountered
    double m_average_block_size = 0.0;           ///< Running average of block sizes
    
    // Output buffering (protected by m_buffer_mutex)
    std::vector<int16_t> m_output_buffer;
    size_t m_buffer_read_position = 0;
    static constexpr size_t MAX_BUFFER_SAMPLES = 48000 * 2 * 4; // 4 seconds stereo
    
    // Enhanced output buffer management (protected by m_buffer_mutex)
    size_t m_buffer_high_watermark = MAX_BUFFER_SAMPLES * 3 / 4;  // 75% of max buffer
    size_t m_buffer_low_watermark = MAX_BUFFER_SAMPLES / 4;       // 25% of max buffer
    bool m_buffer_overflow_detected = false;                      // Overflow detection flag
    bool m_backpressure_active = false;                          // Backpressure state
    size_t m_buffer_underrun_count = 0;                          // Underrun statistics
    size_t m_buffer_overrun_count = 0;                           // Overrun statistics
    
    // Buffer allocation strategy (protected by m_buffer_mutex)
    size_t m_preferred_buffer_size = 0;                          // Optimal buffer size for current stream
    size_t m_buffer_allocation_count = 0;                        // Number of buffer reallocations
    bool m_adaptive_buffer_sizing = true;                        // Enable adaptive buffer sizing
    
    // Flow control state (protected by m_buffer_mutex)
    std::condition_variable m_buffer_cv;                         // Condition variable for flow control
    bool m_buffer_full = false;                                  // Buffer full flag for backpressure
    size_t m_max_pending_samples = MAX_BUFFER_SAMPLES;           // Maximum samples to buffer
    
    // Input queue management (protected by m_input_mutex)
    mutable std::mutex m_input_mutex;                            // Protects input queue operations
    std::queue<MediaChunk> m_input_queue;                        // Queue of pending MediaChunks
    size_t m_max_input_queue_size = 32;                          // Maximum number of queued chunks
    size_t m_input_queue_bytes = 0;                              // Total bytes in input queue
    size_t m_max_input_queue_bytes = 1024 * 1024;               // Maximum bytes in input queue (1MB)
    std::condition_variable m_input_cv;                          // Condition variable for input flow control
    bool m_input_queue_full = false;                             // Input queue full flag
    
    // Frame reconstruction state (protected by m_input_mutex)
    std::vector<uint8_t> m_partial_frame_buffer;                 // Buffer for partial frame reconstruction
    size_t m_expected_frame_size = 0;                            // Expected size of current frame being reconstructed
    bool m_frame_reconstruction_active = false;                  // True if reconstructing a partial frame
    size_t m_frames_reconstructed = 0;                           // Statistics: number of frames reconstructed
    size_t m_partial_frames_received = 0;                        // Statistics: number of partial frames received
    
    // Input flow control state (protected by m_input_mutex)
    bool m_input_backpressure_active = false;                   // Input backpressure state
    size_t m_input_underrun_count = 0;                          // Input underrun statistics
    size_t m_input_overrun_count = 0;                           // Input overrun statistics
    size_t m_input_queue_high_watermark = 24;                   // 75% of max queue size
    size_t m_input_queue_low_watermark = 8;                     // 25% of max queue size
    
    // CRC validation configuration (protected by m_state_mutex)
    bool m_crc_validation_enabled = true;                       // Enable CRC validation per RFC 9639
    bool m_strict_crc_validation = false;                       // Strict mode: reject frames with CRC errors
    size_t m_crc_error_threshold = 10;                          // Maximum CRC errors before disabling validation
    bool m_crc_validation_disabled_due_to_errors = false;       // CRC validation disabled due to excessive errors
    
    // Threading and asynchronous processing state (protected by m_thread_mutex)
    mutable std::mutex m_thread_mutex;                          // Protects threading state
    std::unique_ptr<std::thread> m_decoder_thread;              // Background decoder thread
    std::atomic<bool> m_thread_active{false};                   // Thread active flag
    std::atomic<bool> m_thread_shutdown_requested{false};       // Thread shutdown request flag
    std::condition_variable m_thread_cv;                        // Thread communication
    std::condition_variable m_work_available_cv;                // Work available notification
    std::condition_variable m_work_completed_cv;                // Work completion notification
    
    // Asynchronous processing queues (protected by m_async_mutex)
    mutable std::mutex m_async_mutex;                           // Protects async processing state
    std::queue<MediaChunk> m_async_input_queue;                 // Async input processing queue
    std::queue<AudioFrame> m_async_output_queue;                // Async output queue
    size_t m_max_async_input_queue = 16;                        // Maximum async input queue size
    size_t m_max_async_output_queue = 8;                        // Maximum async output queue size
    bool m_async_processing_enabled = false;                    // Enable asynchronous processing
    
    // Thread synchronization state (protected by m_thread_mutex)
    bool m_thread_exception_occurred = false;                   // Thread exception flag
    std::string m_thread_exception_message;                     // Thread exception details
    std::atomic<size_t> m_pending_work_items{0};                // Number of pending work items
    std::atomic<size_t> m_completed_work_items{0};              // Number of completed work items
    
    // Thread performance monitoring (protected by m_thread_mutex)
    std::chrono::high_resolution_clock::time_point m_thread_start_time; // Thread start time
    std::atomic<uint64_t> m_thread_processing_time_us{0};       // Total thread processing time
    std::atomic<size_t> m_thread_frames_processed{0};           // Frames processed by thread
    std::atomic<size_t> m_thread_idle_cycles{0};                // Thread idle cycle count
    
    // Thread lifecycle management (protected by m_thread_mutex)
    bool m_thread_initialized = false;                          // Thread initialization flag
    bool m_clean_shutdown_completed = false;                    // Clean shutdown completion flag
    std::chrono::milliseconds m_thread_shutdown_timeout{5000};  // Thread shutdown timeout (5 seconds)
    std::chrono::milliseconds m_thread_work_timeout{1000};      // Work processing timeout (1 second)
    
    // Performance optimization state (protected by m_state_mutex)
    std::vector<uint8_t> m_input_buffer;     ///< Reusable input buffer
    std::vector<FLAC__int32> m_decode_buffer; ///< Reusable decode buffer
    FLACCodecStats m_stats;                  ///< Performance statistics
    
    // Thread-safe private implementations (assume locks are held)
    bool initialize_unlocked();
    AudioFrame decode_unlocked(const MediaChunk& chunk);
    AudioFrame flush_unlocked();
    void reset_unlocked();
    bool canDecode_unlocked(const StreamInfo& stream_info) const;
    
    // Configuration and validation methods (assume m_state_mutex is held)
    bool configureFromStreamInfo_unlocked(const StreamInfo& stream_info);
    bool validateConfiguration_unlocked() const;
    bool initializeFLACDecoder_unlocked();
    void cleanupFLAC_unlocked();
    
    // Frame processing methods (assume appropriate locks are held)
    bool processFrameData_unlocked(const uint8_t* data, size_t size);
    bool feedDataToDecoder_unlocked(const uint8_t* data, size_t size);
    AudioFrame extractDecodedSamples_unlocked();
    void handleWriteCallback_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[]);
    void handleMetadataCallback_unlocked(const FLAC__StreamMetadata* metadata);
    void handleErrorCallback_unlocked(FLAC__StreamDecoderErrorStatus status);
    
    // Bit depth conversion methods (assume m_buffer_mutex is held)
    void convertSamples_unlocked(const FLAC__int32* const buffer[], uint32_t block_size);
    int16_t convert8BitTo16Bit(FLAC__int32 sample) const;
    int16_t convert24BitTo16Bit(FLAC__int32 sample) const;
    int16_t convert32BitTo16Bit(FLAC__int32 sample) const;
    void convertSamplesGeneric_unlocked(const FLAC__int32* const buffer[], uint32_t block_size);
    
    // Specialized bit depth conversion methods for performance optimization
    void convertSamples8Bit_unlocked(const FLAC__int32* const buffer[], uint32_t block_size);
    void convertSamples16Bit_unlocked(const FLAC__int32* const buffer[], uint32_t block_size);
    void convertSamples24Bit_unlocked(const FLAC__int32* const buffer[], uint32_t block_size);
    void convertSamples32Bit_unlocked(const FLAC__int32* const buffer[], uint32_t block_size);
    
    // Vectorized conversion methods for batch processing of multiple samples
    void convertSamples8BitStandard_unlocked(const FLAC__int32* const buffer[], uint32_t block_size);
    void convertSamples8BitVectorized_unlocked(const FLAC__int32* const buffer[], uint32_t block_size);
    
    // 24-bit conversion methods with SIMD optimization
    void convertSamples24BitStandard_unlocked(const FLAC__int32* const buffer[], uint32_t block_size);
    void convertSamples24BitSIMD_unlocked(const FLAC__int32* const buffer[], uint32_t block_size);
    void convertSamples24BitScalar_unlocked(const FLAC__int32* const buffer[], uint32_t block_size);
    
#ifdef HAVE_SSE2
    // SSE2-optimized 24-bit conversion methods for x86/x64
    void convertSamples24BitSSE2Mono_unlocked(const FLAC__int32* input, uint32_t block_size);
    void convertSamples24BitSSE2Stereo_unlocked(const FLAC__int32* left, const FLAC__int32* right, uint32_t block_size);
#endif
    
#ifdef HAVE_NEON
    // NEON-optimized 24-bit conversion methods for ARM
    void convertSamples24BitNEONMono_unlocked(const FLAC__int32* input, uint32_t block_size);
    void convertSamples24BitNEONStereo_unlocked(const FLAC__int32* left, const FLAC__int32* right, uint32_t block_size);
#endif
    
    // 32-bit conversion methods with SIMD optimization and overflow protection
    void convertSamples32BitStandard_unlocked(const FLAC__int32* const buffer[], uint32_t block_size);
    void convertSamples32BitSIMD_unlocked(const FLAC__int32* const buffer[], uint32_t block_size);
    void convertSamples32BitScalar_unlocked(const FLAC__int32* const buffer[], uint32_t block_size);
    
#ifdef HAVE_SSE2
    // SSE2-optimized 32-bit conversion methods for x86/x64 with overflow protection
    void convertSamples32BitSSE2Mono_unlocked(const FLAC__int32* input, uint32_t block_size);
    void convertSamples32BitSSE2Stereo_unlocked(const FLAC__int32* left, const FLAC__int32* right, uint32_t block_size);
#endif
    
#ifdef HAVE_NEON
    // NEON-optimized 32-bit conversion methods for ARM with overflow protection
    void convertSamples32BitNEONMono_unlocked(const FLAC__int32* input, uint32_t block_size);
    void convertSamples32BitNEONStereo_unlocked(const FLAC__int32* left, const FLAC__int32* right, uint32_t block_size);
#endif
    
    // RFC 9639 bit depth and sample format compliance validation methods
    
    /**
     * @brief Validate bit depth against RFC 9639 specification requirements
     * 
     * Validates that the specified bit depth is within the RFC 9639 FLAC specification
     * range of 4-32 bits per sample. Also checks for reserved bit depth values that
     * may be defined in future FLAC specification revisions.
     * 
     * @param bits_per_sample Bit depth to validate (4-32 bits per RFC 9639)
     * @return true if bit depth is valid per RFC 9639, false otherwise
     * 
     * @thread_safety Assumes appropriate locks are held by caller
     * 
     * RFC 9639 COMPLIANCE:
     * - Section 4.1: FLAC supports bit depths from 4 to 32 bits per sample
     * - Reserved values: Currently all values 4-32 are valid, no reserved ranges
     * - Future compatibility: Method designed to handle future reserved values
     * 
     * @see validateReservedBitDepthValues_unlocked() for reserved value checking
     */
    bool validateBitDepthRFC9639_unlocked(uint16_t bits_per_sample) const;
    
    /**
     * @brief Validate sample format consistency between STREAMINFO and frame headers
     * 
     * Ensures that the bit depth specified in FLAC frame headers matches the
     * bit depth from the STREAMINFO metadata block. This prevents decoding
     * inconsistencies and ensures RFC 9639 compliance.
     * 
     * @param frame FLAC frame to validate against current codec configuration
     * @return true if sample format is consistent, false if mismatch detected
     * 
     * @thread_safety Assumes m_state_mutex is held by caller
     * 
     * RFC 9639 COMPLIANCE:
     * - Section 4: STREAMINFO must match frame header parameters
     * - Bit depth consistency required throughout stream
     * - Prevents decoder configuration mismatches
     * 
     * VALIDATION CHECKS:
     * - Frame bit depth matches codec configuration (m_bits_per_sample)
     * - Frame channel count matches codec configuration (m_channels)
     * - Frame sample rate matches codec configuration (m_sample_rate)
     * 
     * @see handleMetadataCallback_unlocked() for STREAMINFO processing
     */
    bool validateSampleFormatConsistency_unlocked(const FLAC__Frame* frame) const;
    
    /**
     * @brief Validate and handle reserved bit depth values per RFC 9639
     * 
     * Checks for bit depth values that may be reserved in the FLAC specification
     * for future use. Currently RFC 9639 defines all values 4-32 as valid, but
     * this method provides future-proofing for specification updates.
     * 
     * @param bits_per_sample Bit depth value to check for reserved status
     * @return true if bit depth is not reserved, false if reserved value detected
     * 
     * @thread_safety Thread-safe (no state access required)
     * 
     * RFC 9639 COMPLIANCE:
     * - Currently no reserved bit depth values in RFC 9639
     * - Future-proofing for potential specification updates
     * - Proper error handling for unsupported future formats
     * 
     * RESERVED VALUE HANDLING:
     * - Log warning for reserved values
     * - Return false to prevent decoding with unsupported formats
     * - Maintain backward compatibility with current specification
     */
    bool validateReservedBitDepthValues_unlocked(uint16_t bits_per_sample) const;
    
    /**
     * @brief Apply proper sign extension for samples less than 32 bits per RFC
     * 
     * Implements RFC 9639 compliant sign extension for FLAC samples with bit
     * depths less than 32 bits. This ensures proper handling of signed sample
     * values and prevents sign-related decoding errors.
     * 
     * @param sample Raw sample value from FLAC decoder
     * @param source_bits Actual bit depth of the sample (4-31 bits)
     * @return Properly sign-extended 32-bit sample value
     * 
     * @thread_safety Thread-safe (no state access required)
     * 
     * RFC 9639 COMPLIANCE:
     * - Section 9.2: Proper sign extension for samples < 32 bits
     * - Maintains signed sample representation
     * - Prevents overflow in subsequent processing
     * 
     * SIGN EXTENSION ALGORITHM:
     * 1. Calculate sign bit position for source bit depth
     * 2. Check if sign bit is set (negative value)
     * 3. If negative, extend sign bits to fill 32-bit value
     * 4. If positive, ensure upper bits are cleared
     * 
     * PERFORMANCE OPTIMIZATION:
     * - Bit manipulation operations for maximum speed
     * - Branch-free implementation where possible
     * - Optimized for common bit depths (8, 16, 24)
     * 
     * @see convert8BitTo16Bit() for 8-bit specific handling
     * @see convert24BitTo16Bit() for 24-bit specific handling
     * @see convert32BitTo16Bit() for 32-bit specific handling
     */
    FLAC__int32 applyProperSignExtension_unlocked(FLAC__int32 sample, uint16_t source_bits) const;
    
    /**
     * @brief Validate bit-perfect reconstruction for lossless decoding per RFC
     * 
     * Validates that the decoded and converted samples maintain bit-perfect
     * accuracy for lossless reconstruction. This is critical for FLAC's
     * lossless compression guarantee per RFC 9639.
     * 
     * @param original Original FLAC samples before conversion
     * @param converted Converted 16-bit PCM samples
     * @param sample_count Number of samples to validate
     * @param source_bits Original bit depth of FLAC samples
     * @return true if reconstruction is bit-perfect within conversion limits
     * 
     * @thread_safety Thread-safe (no state modification)
     * 
     * RFC 9639 COMPLIANCE:
     * - FLAC is a lossless codec - no information loss during decoding
     * - Bit-perfect reconstruction required for same bit depth
     * - Conversion accuracy validation for different bit depths
     * 
     * VALIDATION CRITERIA:
     * - Same bit depth: Exact bit-perfect match required
     * - Different bit depth: Conversion accuracy within expected tolerance
     * - No clipping or overflow in converted samples
     * - Proper handling of dynamic range limitations
     * 
     * BIT-PERFECT VALIDATION:
     * - 16-bit source: Exact match required (no conversion)
     * - 8-bit source: Validate upscaling accuracy (sample << 8)
     * - 24-bit source: Validate downscaling accuracy (sample >> 8)
     * - 32-bit source: Validate downscaling with overflow protection
     * 
     * @see calculateAudioQualityMetrics_unlocked() for quality analysis
     */
    bool validateBitPerfectReconstruction_unlocked(const FLAC__int32* original, 
                                                   const int16_t* converted, 
                                                   size_t sample_count, 
                                                   uint16_t source_bits) const;
    
    /**
     * @brief Calculate comprehensive audio quality metrics for validation
     * 
     * Computes detailed audio quality metrics including SNR, THD, dynamic range,
     * and bit-perfect validation. Used for validating codec accuracy and
     * conversion quality per RFC 9639 lossless requirements.
     * 
     * @param samples Converted 16-bit PCM samples to analyze
     * @param sample_count Number of samples in the buffer
     * @param reference Optional reference samples for comparison (original FLAC)
     * @param reference_bits Bit depth of reference samples (if provided)
     * @return AudioQualityMetrics structure with comprehensive quality analysis
     * 
     * @thread_safety Thread-safe (no state modification)
     * 
     * QUALITY METRICS CALCULATED:
     * - Signal-to-Noise Ratio (SNR) in decibels
     * - Total Harmonic Distortion (THD) as percentage
     * - Dynamic Range in decibels
     * - Peak and RMS amplitude measurements
     * - DC offset detection and measurement
     * - Zero crossing analysis
     * - Clipping detection and counting
     * - Bit-perfect validation (when reference provided)
     * 
     * RFC 9639 COMPLIANCE VALIDATION:
     * - Lossless decoding verification
     * - Conversion accuracy assessment
     * - Quality degradation detection
     * - Performance impact measurement
     * 
     * USAGE SCENARIOS:
     * - Codec accuracy validation during development
     * - Quality regression testing
     * - Performance optimization verification
     * - Debugging audio quality issues
     * 
     * @see AudioQualityMetrics for detailed metric descriptions
     * @see validateBitPerfectReconstruction_unlocked() for bit-perfect validation
     */
    AudioQualityMetrics calculateAudioQualityMetrics_unlocked(const int16_t* samples, 
                                                              size_t sample_count, 
                                                              const FLAC__int32* reference = nullptr,
                                                              uint16_t reference_bits = 16) const;
    
    // Channel processing methods (assume m_buffer_mutex is held)
    void processChannelAssignment_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[]);
    void processIndependentChannels_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[]);
    void processLeftSideStereo_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[]);
    void processRightSideStereo_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[]);
    void processMidSideStereo_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[]);
    
    // Optimized channel processing methods for performance (assume m_buffer_mutex is held)
    void processMonoChannelOptimized_unlocked(const FLAC__int32* input, uint32_t block_size, uint16_t bits_per_sample);
    void processStereoChannelsOptimized_unlocked(const FLAC__int32* left, const FLAC__int32* right, 
                                               uint32_t block_size, uint16_t bits_per_sample);
    void processMultiChannelOptimized_unlocked(const FLAC__int32* const buffer[], uint16_t channels, 
                                             uint32_t block_size, uint16_t bits_per_sample);
    
    // Error handling methods (assume appropriate locks are held)
    AudioFrame handleDecodingError_unlocked(const MediaChunk& chunk);
    bool recoverFromError_unlocked();
    bool recoverFromSyncLoss_unlocked(const MediaChunk& chunk);
    bool recoverFromCorruptedFrame_unlocked(const MediaChunk& chunk);
    bool recoverFromMemoryError_unlocked();
    void resetDecoderState_unlocked();
    AudioFrame createSilenceFrame_unlocked(uint32_t block_size);
    void setErrorState_unlocked(bool error_state);
    uint32_t estimateBlockSizeFromChunk_unlocked(const MediaChunk& chunk);
    
    // Decoder state recovery methods (assume appropriate locks are held)
    bool handleDecoderStateInconsistency_unlocked();
    bool recreateDecoder_unlocked();
    bool resetDecoderForNewStream_unlocked();
    
    // CRC validation methods (RFC 9639 compliance - assume appropriate locks are held)
    bool validateFrameCRC_unlocked(const uint8_t* frame_data, size_t frame_size);
    uint8_t calculateFrameHeaderCRC_unlocked(const uint8_t* header_data, size_t header_size);
    uint16_t calculateFrameFooterCRC_unlocked(const uint8_t* frame_data, size_t frame_size);
    void handleCRCMismatch_unlocked(const char* crc_type, uint32_t expected, uint32_t calculated, 
                                   const uint8_t* frame_data, size_t frame_size);
    bool shouldValidateCRC_unlocked() const;
    const char* getChannelAssignmentName(uint8_t channel_assignment) const;
    bool recoverFromOggError_unlocked();
    bool recoverFromDecoderMemoryError_unlocked();
    bool reinitializeDecoder_unlocked();
    bool ensureDecoderFunctional_unlocked();
    bool handleMemoryAllocationFailure_unlocked();
    bool validateCodecIntegrity_unlocked();
    
    // Memory management methods (assume appropriate locks are held)
    void optimizeBufferSizes_unlocked();
    void ensureBufferCapacity_unlocked(size_t required_samples);
    void freeUnusedMemory_unlocked();
    
    // AudioFrame creation and validation methods (assume appropriate locks are held)
    AudioFrame createAudioFrame_unlocked(const std::vector<int16_t>& samples, uint64_t timestamp_samples);
    AudioFrame createAudioFrame_unlocked(std::vector<int16_t>&& samples, uint64_t timestamp_samples);
    void validateAudioFrame_unlocked(AudioFrame& frame) const;
    void updateSamplePosition_unlocked(size_t sample_frame_count);
    
    // Advanced memory management methods (assume appropriate locks are held)
    size_t calculateCurrentMemoryUsage_unlocked() const;
    void implementMemoryPoolAllocation_unlocked();
    void optimizeMemoryFragmentation_unlocked();
    
    // Enhanced output buffer management methods (assume m_buffer_mutex is held)
    bool checkBufferCapacity_unlocked(size_t required_samples);
    void handleBufferOverflow_unlocked();
    void handleBufferUnderrun_unlocked();
    bool waitForBufferSpace_unlocked(size_t required_samples, std::chrono::milliseconds timeout = std::chrono::milliseconds(100));
    void notifyBufferSpaceAvailable_unlocked();
    void updateBufferWatermarks_unlocked();
    void resetBufferFlowControl_unlocked();
    
    // Buffer allocation and reuse methods (assume m_buffer_mutex is held)
    void optimizeBufferAllocation_unlocked(size_t required_samples);
    void adaptiveBufferResize_unlocked(size_t required_samples);
    bool requiresBufferReallocation_unlocked(size_t required_samples) const;
    void updateBufferStatistics_unlocked(bool overflow, bool underrun);
    size_t calculateOptimalBufferSize_unlocked(size_t required_samples) const;
    
    // Flow control and backpressure methods (assume m_buffer_mutex is held)
    bool isBackpressureActive_unlocked() const;
    void activateBackpressure_unlocked();
    void deactivateBackpressure_unlocked();
    bool shouldApplyBackpressure_unlocked(size_t required_samples) const;
    void handleBackpressure_unlocked(size_t required_samples);
    
    // Input queue management methods (assume m_input_mutex is held)
    bool enqueueInputChunk_unlocked(const MediaChunk& chunk);
    MediaChunk dequeueInputChunk_unlocked();
    bool hasInputChunks_unlocked() const;
    size_t getInputQueueSize_unlocked() const;
    void clearInputQueue_unlocked();
    bool isInputQueueFull_unlocked() const;
    void updateInputQueueWatermarks_unlocked();
    
    // Frame reconstruction methods (assume m_input_mutex is held)
    bool processPartialFrame_unlocked(const MediaChunk& chunk);
    bool reconstructFrame_unlocked(MediaChunk& complete_frame);
    void resetFrameReconstruction_unlocked();
    bool isFrameComplete_unlocked(const uint8_t* data, size_t size) const;
    size_t estimateFrameSize_unlocked(const uint8_t* data, size_t size) const;
    bool validateFrameHeader_unlocked(const uint8_t* data, size_t size) const;
    
    // RFC 9639 compliance validation helper methods (assume appropriate locks are held)
    bool validateRFC9639Compliance_unlocked(const uint8_t* data, size_t size) const;
    bool checkForbiddenBitPatterns_unlocked(const uint8_t* frame_header) const;
    bool validateReservedFields_unlocked(const uint8_t* frame_header) const;
    bool handleUnsupportedFeatures_unlocked(const uint8_t* frame_header) const;
    
    // RFC 9639 compliant error handling and recovery methods
    bool handleRFC9639Error_unlocked(FLAC__StreamDecoderErrorStatus status, const char* context);
    bool recoverFromForbiddenPattern_unlocked(const uint8_t* data, size_t size);
    bool recoverFromReservedFieldViolation_unlocked(const uint8_t* data, size_t size);
    bool shouldTerminateStream_unlocked(FLAC__StreamDecoderErrorStatus status) const;
    void logRFC9639Violation_unlocked(const char* violation_type, const char* rfc_section, 
                                     const uint8_t* data, size_t offset) const;
    bool validateBlockSizeBits_unlocked(uint8_t block_size_bits) const;
    bool validateSampleRateBits_unlocked(uint8_t sample_rate_bits) const;
    bool validateChannelAssignment_unlocked(uint8_t channel_assignment) const;
    bool validateBitDepthBits_unlocked(uint8_t bit_depth_bits) const;
    
    // RFC 9639 block size and sample rate decoding methods (assume appropriate locks are held)
    uint32_t decodeBlockSizeFromBits_unlocked(uint8_t block_size_bits, const uint8_t* header_data, size_t header_size, size_t& bytes_consumed) const;
    uint32_t decodeSampleRateFromBits_unlocked(uint8_t sample_rate_bits, const uint8_t* header_data, size_t header_size, size_t& bytes_consumed) const;
    
    // RFC 9639 uncommon value decoding methods (assume appropriate locks are held)
    uint32_t decodeUncommonBlockSize8Bit_unlocked(const uint8_t* header_data, size_t header_size, size_t& bytes_consumed) const;
    uint32_t decodeUncommonBlockSize16Bit_unlocked(const uint8_t* header_data, size_t header_size, size_t& bytes_consumed) const;
    uint32_t decodeUncommonSampleRate8BitKHz_unlocked(const uint8_t* header_data, size_t header_size, size_t& bytes_consumed) const;
    uint32_t decodeUncommonSampleRate16BitHz_unlocked(const uint8_t* header_data, size_t header_size, size_t& bytes_consumed) const;
    uint32_t decodeUncommonSampleRate16BitHzDiv10_unlocked(const uint8_t* header_data, size_t header_size, size_t& bytes_consumed) const;
    
    // RFC 9639 consistency validation methods (assume appropriate locks are held)
    bool validateStreamInfoConsistency_unlocked(uint32_t frame_block_size, uint32_t frame_sample_rate) const;
    bool validateBlockSizeRange_unlocked(uint32_t block_size) const;
    bool validateSampleRateRange_unlocked(uint32_t sample_rate) const;
    
    // RFC 9639 Section 9.2 Subframe Type Compliance Validation (assume appropriate locks are held)
    bool validateSubframeType_unlocked(uint8_t subframe_type_bits) const;
    bool validateConstantSubframe_unlocked(uint8_t subframe_type_bits) const;
    bool validateVerbatimSubframe_unlocked(uint8_t subframe_type_bits) const;
    bool validateFixedPredictorSubframe_unlocked(uint8_t subframe_type_bits) const;
    bool validateLinearPredictorSubframe_unlocked(uint8_t subframe_type_bits) const;
    bool validateWastedBitsFlag_unlocked(uint8_t wasted_bits_flag) const;
    uint8_t extractPredictorOrder_unlocked(uint8_t subframe_type_bits) const;
    
    // RFC 9639 Section 9.2.5 Entropy Coding Compliance Validation (assume appropriate locks are held)
    bool validateEntropyCoding_unlocked(const uint8_t* residual_data, size_t data_size, 
                                       uint32_t block_size, uint8_t predictor_order) const;
    bool validateRiceCodingMethod_unlocked(uint8_t coding_method_bits) const;
    bool validatePartitionOrder_unlocked(uint8_t partition_order, uint32_t block_size, 
                                        uint8_t predictor_order) const;
    bool validateRiceParameters_unlocked(const uint8_t* partition_data, size_t data_size,
                                        uint8_t coding_method, uint8_t partition_order,
                                        uint32_t block_size, uint8_t predictor_order) const;
    bool validateEscapeCode_unlocked(uint8_t parameter_bits, bool is_5bit_parameter) const;
    bool decodeRicePartition_unlocked(const uint8_t* partition_data, size_t data_size,
                                     uint8_t rice_parameter, uint32_t sample_count,
                                     std::vector<int32_t>& decoded_residuals) const;
    bool decodeEscapedPartition_unlocked(const uint8_t* partition_data, size_t data_size,
                                        uint8_t bits_per_sample, uint32_t sample_count,
                                        std::vector<int32_t>& decoded_residuals) const;
    int32_t decodeRiceSample_unlocked(const uint8_t* data, size_t& bit_offset, 
                                     uint8_t rice_parameter) const;
    int32_t zigzagDecode_unlocked(uint32_t folded_value) const;
    bool validateResidualRange_unlocked(int32_t residual_value) const;
    
    // Input flow control methods (assume m_input_mutex is held)
    bool checkInputQueueCapacity_unlocked(const MediaChunk& chunk);
    void handleInputOverflow_unlocked();
    void handleInputUnderrun_unlocked();
    bool waitForInputQueueSpace_unlocked(const MediaChunk& chunk, std::chrono::milliseconds timeout = std::chrono::milliseconds(100));
    
    // Quality validation and accuracy testing methods (thread-safe)
    bool validateBitPerfectDecoding_unlocked(const std::vector<int16_t>& reference_samples, 
                                           const std::vector<int16_t>& decoded_samples) const;
    double calculateSignalToNoiseRatio_unlocked(const std::vector<int16_t>& reference_samples,
                                               const std::vector<int16_t>& decoded_samples) const;
    double calculateTotalHarmonicDistortion_unlocked(const std::vector<int16_t>& samples) const;
    bool validateConversionQuality_unlocked(const std::vector<FLAC__int32>& source_samples,
                                          const std::vector<int16_t>& converted_samples,
                                          uint16_t source_bit_depth) const;
    bool validateDynamicRange_unlocked(const std::vector<int16_t>& samples) const;
    AudioQualityMetrics calculateQualityMetrics_unlocked(const std::vector<int16_t>& samples) const;
    
    // Bit-perfect validation helpers
    bool compareSamplesExact_unlocked(const std::vector<int16_t>& samples1,
                                     const std::vector<int16_t>& samples2) const;
    double calculateMeanSquaredError_unlocked(const std::vector<int16_t>& reference_samples,
                                            const std::vector<int16_t>& test_samples) const;
    double calculatePeakSignalToNoiseRatio_unlocked(const std::vector<int16_t>& reference_samples,
                                                   const std::vector<int16_t>& test_samples) const;
    
    // Conversion accuracy validation helpers
    bool validateBitDepthConversion_unlocked(FLAC__int32 source_sample, int16_t converted_sample,
                                           uint16_t source_bit_depth) const;
    double calculateConversionError_unlocked(const std::vector<FLAC__int32>& source_samples,
                                           const std::vector<int16_t>& converted_samples,
                                           uint16_t source_bit_depth) const;
    bool validateNoClipping_unlocked(const std::vector<int16_t>& samples) const;
    
    // Audio quality analysis helpers
    double calculateRMSAmplitude_unlocked(const std::vector<int16_t>& samples) const;
    double calculatePeakAmplitude_unlocked(const std::vector<int16_t>& samples) const;
    double calculateDCOffset_unlocked(const std::vector<int16_t>& samples) const;
    size_t countZeroCrossings_unlocked(const std::vector<int16_t>& samples) const;
    size_t countClippedSamples_unlocked(const std::vector<int16_t>& samples) const;
    
    // Reference decoder comparison helpers
    bool compareWithReferenceDecoder_unlocked(const MediaChunk& chunk,
                                            const std::vector<int16_t>& our_output) const;
    std::vector<int16_t> generateReferenceSamples_unlocked(const MediaChunk& chunk) const;
    
    // Input flow control methods (assume m_input_mutex is held)
    void notifyInputQueueSpaceAvailable_unlocked();
    bool shouldApplyInputBackpressure_unlocked(const MediaChunk& chunk) const;
    void handleInputBackpressure_unlocked(const MediaChunk& chunk);
    void activateInputBackpressure_unlocked();
    void deactivateInputBackpressure_unlocked();
    void resetInputFlowControl_unlocked();
    
    // Threading and asynchronous processing methods (thread-safe public interface)
    bool startDecoderThread();
    void stopDecoderThread();
    bool isDecoderThreadActive() const;
    void enableAsyncProcessing(bool enable = true);
    bool isAsyncProcessingEnabled() const;
    
    // Thread management methods (assume m_thread_mutex is held)
    bool startDecoderThread_unlocked();
    void stopDecoderThread_unlocked();
    void decoderThreadLoop();
    bool initializeDecoderThread_unlocked();
    void cleanupDecoderThread_unlocked();
    bool waitForThreadShutdown_unlocked(std::chrono::milliseconds timeout);
    
    // Thread synchronization methods (assume appropriate locks are held)
    void notifyWorkAvailable_unlocked();
    void notifyWorkCompleted_unlocked();
    bool waitForWorkCompletion_unlocked(std::chrono::milliseconds timeout);
    void handleThreadException_unlocked(const std::exception& e);
    void resetThreadState_unlocked();
    
    // Optimized threading methods for performance (assume appropriate locks are held)
    AudioFrame decodeChunkOptimized_unlocked(const MediaChunk& chunk);
    bool processFrameDataFast_unlocked(const uint8_t* data, size_t size);
    AudioFrame extractDecodedSamplesFast_unlocked();
    AudioFrame createSilenceFrameFast_unlocked(uint32_t block_size);
    bool hasAsyncInputFast_unlocked() const;
    void notifyWorkCompletedBatch_unlocked(size_t batch_size);
    void handleThreadExceptionFast_unlocked(const std::exception& e);
    void handleThreadTerminationFast_unlocked();
    
    // Asynchronous processing methods (assume m_async_mutex is held)
    bool enqueueAsyncInput_unlocked(const MediaChunk& chunk);
    MediaChunk dequeueAsyncInput_unlocked();
    bool enqueueAsyncOutput_unlocked(const AudioFrame& frame);
    AudioFrame dequeueAsyncOutput_unlocked();
    bool hasAsyncInput_unlocked() const;
    bool hasAsyncOutput_unlocked() const;
    void clearAsyncQueues_unlocked();
    
    // Thread lifecycle and exception handling (assume m_thread_mutex is held)
    void ensureThreadSafety_unlocked();
    bool isThreadHealthy_unlocked() const;
    void handleThreadTermination_unlocked();
    void logThreadStatistics_unlocked() const;
    
    // Performance monitoring (assume m_state_mutex is held)
    void updatePerformanceStats_unlocked(uint32_t block_size, uint64_t decode_time_us);
    void logPerformanceMetrics_unlocked() const;
    
    // Variable block size handling methods (assume m_state_mutex is held)
    void initializeBlockSizeHandling_unlocked();
    bool validateBlockSize_unlocked(uint32_t block_size) const;
    void updateBlockSizeTracking_unlocked(uint32_t block_size);
    void optimizeForBlockSize_unlocked(uint32_t block_size);
    bool isStandardBlockSize_unlocked(uint32_t block_size) const;
    void adaptBuffersForBlockSize_unlocked(uint32_t block_size);
    void detectPreferredBlockSize_unlocked(uint32_t block_size);
    
    // Fixed block size optimization methods (assume appropriate locks are held)
    void optimizeForFixedBlockSizes_unlocked();
    void preAllocateForStandardSizes_unlocked();
    size_t calculateOptimalBufferSize_unlocked() const;
    
    // Variable block size adaptation methods (assume appropriate locks are held)
    void handleBlockSizeTransition_unlocked(uint32_t old_size, uint32_t new_size);
    void smoothBlockSizeTransition_unlocked(uint32_t new_block_size);
    void maintainOutputTiming_unlocked(uint32_t block_size);
    void adaptiveBufferResize_unlocked(uint32_t block_size);
    bool requiresBufferReallocation_unlocked(uint32_t block_size) const;
    void optimizeForVariableBlockSizes_unlocked();
    
    // Prevent copying
    FLACCodec(const FLACCodec&) = delete;
    FLACCodec& operator=(const FLACCodec&) = delete;
};

/**
 * @brief Conditional compilation support for FLAC codec
 * 
 * This namespace provides compile-time detection and registration
 * of FLAC codec support based on library availability.
 */
namespace FLACCodecSupport {
    
    /**
     * @brief Check if FLAC codec is available at compile time
     */
    constexpr bool isAvailable() { return true; }
    
    /**
     * @brief Register FLAC codec with AudioCodecFactory (conditional compilation)
     */
    void registerCodec();
    
    /**
     * @brief Create FLAC codec instance (conditional compilation)
     */
    std::unique_ptr<AudioCodec> createCodec(const StreamInfo& stream_info);
    
    /**
     * @brief Check if StreamInfo represents a FLAC stream
     */
    bool isFLACStream(const StreamInfo& stream_info);
    
    /**
     * @brief Get FLAC codec capabilities and version information
     */
    std::string getCodecInfo();
    
} // namespace FLACCodecSupport

#else // !HAVE_FLAC

// Stub implementations when FLAC is not available

struct FLACFrameInfo {
    bool isValid() const { return false; }
    uint64_t getDurationMs() const { return 0; }
    size_t getOutputSampleCount() const { return 0; }
    const char* getChannelAssignmentName() const { return "unavailable"; }
};

struct FLACCodecStats {
    double getAverageDecodeTimeUs() const { return 0.0; }
    double getDecodeEfficiency() const { return 0.0; }
    double getErrorRate() const { return 0.0; }
};

namespace FLACCodecSupport {
    constexpr bool isAvailable() { return false; }
    inline void registerCodec() { /* No FLAC support */ }
    inline std::unique_ptr<AudioCodec> createCodec(const StreamInfo&) { return nullptr; }
    inline bool isFLACStream(const StreamInfo&) { return false; }
    inline std::string getCodecInfo() { return "FLAC codec not available"; }
}

#endif // HAVE_FLAC

#endif // FLACCODEC_H