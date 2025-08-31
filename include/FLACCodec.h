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
 * @thread_safety This class is thread-safe. All public methods follow the
 *                public/private lock pattern to prevent deadlocks.
 * 
 * THREADING SAFETY GUARANTEES:
 * - All public methods acquire locks using RAII guards (std::lock_guard)
 * - All public methods call corresponding _unlocked private methods
 * - Internal method calls always use _unlocked versions to prevent deadlocks
 * - Exception safety is maintained through RAII lock guards
 * - No callbacks are invoked while holding internal locks
 * 
 * LOCK ACQUISITION ORDER (to prevent deadlocks):
 * 1. m_state_mutex (for codec state and configuration)
 * 2. m_buffer_mutex (for output buffer management)  
 * 3. libFLAC internal locks (managed by libFLAC)
 * 
 * NEVER acquire locks in different order to prevent deadlocks!
 * NEVER call public methods from _unlocked methods!
 */
class FLACCodec : public AudioCodec {
public:
    explicit FLACCodec(const StreamInfo& stream_info);
    ~FLACCodec() override;
    
    // AudioCodec interface implementation (thread-safe public methods)
    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    std::string getCodecName() const override { return "flac"; }
    bool canDecode(const StreamInfo& stream_info) const override;
    
    // FLAC-specific public interface
    bool supportsSeekReset() const;
    uint64_t getCurrentSample() const;
    FLACCodecStats getStats() const;
    
    // Friend class for callback access
    friend class FLACStreamDecoder;

private:
    // Thread safety - documented lock acquisition order above
    mutable std::mutex m_state_mutex;    ///< Protects codec state and configuration
    mutable std::mutex m_buffer_mutex;   ///< Protects output buffer operations
    std::atomic<bool> m_error_state{false}; ///< Thread-safe error state flag
    
    // FLAC decoder state (protected by m_state_mutex)
    std::unique_ptr<FLACStreamDecoder> m_decoder;
    bool m_decoder_initialized = false;
    
    // Stream configuration (protected by m_state_mutex)
    uint32_t m_sample_rate = 0;
    uint16_t m_channels = 0;
    uint16_t m_bits_per_sample = 0;
    uint64_t m_total_samples = 0;
    
    // Decoding state (protected by m_state_mutex)
    std::atomic<uint64_t> m_current_sample{0};  ///< Atomic for fast reads
    uint32_t m_last_block_size = 0;
    bool m_stream_finished = false;
    
    // Output buffering (protected by m_buffer_mutex)
    std::vector<int16_t> m_output_buffer;
    size_t m_buffer_read_position = 0;
    static constexpr size_t MAX_BUFFER_SAMPLES = 48000 * 2 * 4; // 4 seconds stereo
    
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
    
    // Channel processing methods (assume m_buffer_mutex is held)
    void processChannelAssignment_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[]);
    void processIndependentChannels_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[]);
    void processLeftSideStereo_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[]);
    void processRightSideStereo_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[]);
    void processMidSideStereo_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[]);
    
    // Error handling methods (assume appropriate locks are held)
    AudioFrame handleDecodingError_unlocked(const MediaChunk& chunk);
    bool recoverFromError_unlocked();
    void resetDecoderState_unlocked();
    AudioFrame createSilenceFrame_unlocked(uint32_t block_size);
    void setErrorState_unlocked(bool error_state);
    
    // Memory management methods (assume appropriate locks are held)
    void optimizeBufferSizes_unlocked();
    void ensureBufferCapacity_unlocked(size_t required_samples);
    void freeUnusedMemory_unlocked();
    
    // Performance monitoring (assume m_state_mutex is held)
    void updatePerformanceStats_unlocked(uint32_t block_size, uint64_t decode_time_us);
    void logPerformanceMetrics_unlocked() const;
    
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