/*
 * OpusCodec.h - Container-agnostic Opus audio codec
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

#ifndef OPUSCODEC_H
#define OPUSCODEC_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace Codec {
namespace Opus {

/**
 * @brief Opus header structure for identification header parsing
 */
struct OpusHeader {
    uint8_t version;
    uint8_t channel_count;
    uint16_t pre_skip;
    uint32_t input_sample_rate;
    int16_t output_gain;
    uint8_t channel_mapping_family;
    
    // Channel mapping (for multichannel)
    uint8_t stream_count;
    uint8_t coupled_stream_count;
    std::vector<uint8_t> channel_mapping;
    
    bool isValid() const;
    static OpusHeader parseFromPacket(const std::vector<uint8_t>& packet_data);
};

/**
 * @brief Opus comments structure for comment header parsing
 */
struct OpusComments {
    std::string vendor_string;
    std::vector<std::pair<std::string, std::string>> user_comments;
    
    static OpusComments parseFromPacket(const std::vector<uint8_t>& packet_data);
};

/**
 * @brief Container-agnostic Opus audio codec
 * 
 * This codec decodes Opus bitstream data from any container format
 * (primarily Ogg Opus) into standard 16-bit PCM audio at 48kHz.
 * It uses libopus directly for efficient, accurate decoding and
 * supports all Opus modes (SILK, CELT, hybrid).
 */
class OpusCodec : public AudioCodec {
public:
    explicit OpusCodec(const StreamInfo& stream_info);
    ~OpusCodec() override;
    
    // AudioCodec interface implementation
    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    std::string getCodecName() const override { return "opus"; }
    bool canDecode(const StreamInfo& stream_info) const override;
    
private:
    // libopus decoder state
    OpusDecoder* m_opus_decoder = nullptr;
    OpusMSDecoder* m_opus_ms_decoder = nullptr;  // For multi-stream decoding
    bool m_use_multistream = false;
    
    // Stream configuration
    uint32_t m_sample_rate = 48000;  // Opus always outputs at 48kHz
    uint16_t m_channels = 0;
    uint16_t m_pre_skip = 0;
    int16_t m_output_gain = 0;
    
    // Multi-channel configuration
    uint8_t m_channel_mapping_family = 0;
    uint8_t m_stream_count = 0;
    uint8_t m_coupled_stream_count = 0;
    std::vector<uint8_t> m_channel_mapping;
    
    // Header processing state
    int m_header_packets_received = 0;
    bool m_decoder_initialized = false;
    bool m_flushed = false;  // Track if flush() has been called to prevent infinite loop
    
    // Decoding buffers
    std::vector<int16_t> m_output_buffer;
    std::vector<float> m_float_buffer;  // For float decoding if needed
    
    // Bounded output buffer management (Requirements 7.1, 7.2, 7.4)
    std::queue<AudioFrame> m_output_queue;
    size_t m_max_output_buffer_frames;
    size_t m_max_output_buffer_samples;
    std::atomic<size_t> m_buffered_samples{0};
    std::atomic<bool> m_buffer_overflow{false};
    
    // Streaming support state (Requirements 7.3, 7.8)
    std::vector<uint8_t> m_partial_packet_buffer;
    std::atomic<uint64_t> m_frames_processed{0};
    std::chrono::steady_clock::time_point m_last_decode_time;
    
    // Position tracking
    std::atomic<uint64_t> m_samples_decoded{0};
    std::atomic<uint64_t> m_samples_to_skip{0};
    
    // Error handling
    std::atomic<bool> m_error_state{false};
    std::string m_last_error;
    
    // Thread-local error state for concurrent operation (Requirement 8.8)
    thread_local static std::string tl_last_error;
    thread_local static int tl_last_opus_error;
    thread_local static std::chrono::steady_clock::time_point tl_last_error_time;
    
    // Threading safety - following public/private lock pattern
    // Lock acquisition order (to prevent deadlocks):
    // 1. OpusCodec::m_mutex (instance-level lock)
    // Note: OpusCodec only uses a single mutex, so no deadlock concerns within this class
    mutable std::mutex m_mutex;
    
    // Performance optimization state (per-instance to ensure thread safety)
    int m_last_frame_size = 0;
    int m_frame_size_changes = 0;
    
    // Header processing (private unlocked methods)
    bool processHeaderPacket_unlocked(const std::vector<uint8_t>& packet_data);
    bool processIdentificationHeader_unlocked(const std::vector<uint8_t>& packet_data);
    bool processCommentHeader_unlocked(const std::vector<uint8_t>& packet_data);
    bool validateOpusHeaderParameters_unlocked(uint8_t version, uint8_t channels, uint8_t mapping_family, uint32_t input_sample_rate);
    
    // Multi-channel support (private unlocked methods)
    bool configureChannelMapping_unlocked(uint8_t mapping_family, uint8_t channels, const std::vector<uint8_t>& packet_data);
    bool validateChannelConfiguration_unlocked(uint8_t channels, uint8_t mapping_family);
    bool configureChannelMappingFamily0_unlocked(uint8_t channels);
    bool configureChannelMappingFamily1_unlocked(uint8_t channels, const std::vector<uint8_t>& packet_data);
    bool configureChannelMappingFamily255_unlocked(uint8_t channels, const std::vector<uint8_t>& packet_data);
    bool validateSurroundSoundConfiguration_unlocked(uint8_t channels);
    void processChannelMapping_unlocked(AudioFrame& frame);
    bool applyVorbisChannelOrdering_unlocked(AudioFrame& frame);
    bool applyCustomChannelMapping_unlocked(AudioFrame& frame);
    bool validateChannelMappingConfiguration_unlocked();
    bool isVorbisChannelOrderingSupported_unlocked(uint8_t channels);
    void handleUnsupportedChannelConfiguration_unlocked(uint8_t channels, const std::string& reason);
    
    // Audio decoding (private    // Audio decoding
    AudioFrame decodeAudioPacket_unlocked(const MediaChunk& chunk);
    AudioFrame decodeAudioPacket_unlocked(const std::vector<uint8_t>& packet_data); // Legacy overload for header processing
    void applyPreSkip_unlocked(AudioFrame& frame);
    void applyOutputGain_unlocked(AudioFrame& frame);
    
    // Initialization methods (private unlocked methods)
    bool initialize_unlocked();
    bool extractOpusParameters_unlocked();
    bool validateOpusParameters_unlocked();
    bool setupInternalBuffers_unlocked();
    bool resizeBuffersForChannels_unlocked(uint16_t channels);
    
    // Utility methods (private unlocked methods)
    bool validateOpusPacket_unlocked(const std::vector<uint8_t>& packet_data);
    void handleDecoderError_unlocked(int opus_error);
    void resetDecoderState_unlocked();
    bool initializeOpusDecoder_unlocked();
    bool initializeMultiStreamDecoder_unlocked();
    bool validateDecoderState_unlocked() const;
    bool recoverFromError_unlocked();
    
    // Performance optimization methods (private unlocked methods)
    bool configureOpusOptimizations_unlocked();
    bool enableSIMDOptimizations_unlocked();
    void optimizeBufferSizes_unlocked(uint16_t channels);
    bool isMonoStereoOptimizable_unlocked() const;
    void optimizeMemoryAccessPatterns_unlocked(AudioFrame& frame);
    bool handleVariableFrameSizeEfficiently_unlocked(int frame_size_samples);
    void optimizeCacheEfficiency_unlocked();
    
    // Enhanced error handling and validation methods (private unlocked methods)
    bool validateInputParameters_unlocked(const std::vector<uint8_t>& packet_data) const;
    bool validateHeaderPacketParameters_unlocked(const std::vector<uint8_t>& packet_data) const;
    bool validateMemoryAllocation_unlocked(size_t requested_size) const;
    bool handleMemoryAllocationFailure_unlocked();
    void reportDetailedError_unlocked(const std::string& error_type, const std::string& details);
    bool isRecoverableError_unlocked(int opus_error) const;
    
    // Thread-local error state management (private unlocked methods)
    void setThreadLocalError_unlocked(int opus_error, const std::string& error_message);
    std::string getThreadLocalError_unlocked() const;
    void clearThreadLocalError_unlocked();
    bool hasRecentThreadLocalError_unlocked() const;
    
    // Thread safety validation (private unlocked methods)
    bool validateThreadSafetyState_unlocked() const;
    
    // Enhanced error recovery and state management (private unlocked methods)
    bool performUnrecoverableErrorReset_unlocked();
    bool attemptGracefulRecovery_unlocked(int opus_error);
    void saveDecoderState_unlocked();
    bool restoreDecoderState_unlocked();
    bool validateRecoverySuccess_unlocked() const;
    
    // Buffer management methods (private unlocked methods)
    bool initializeOutputBuffers_unlocked();
    bool canBufferFrame_unlocked(const AudioFrame& frame) const;
    bool bufferFrame_unlocked(const AudioFrame& frame);
    AudioFrame getBufferedFrame_unlocked();
    bool hasBufferedFrames_unlocked() const;
    void clearOutputBuffers_unlocked();
    size_t getBufferedSampleCount_unlocked() const;
    bool isOutputBufferFull_unlocked() const;
    
    // Streaming support methods (private unlocked methods)
    bool handlePartialPacketData_unlocked(const std::vector<uint8_t>& packet_data);
    bool isValidPartialPacket_unlocked(const std::vector<uint8_t>& packet_data) const;
    void maintainStreamingLatency_unlocked();
};

#ifdef HAVE_OGGDEMUXER

/**
 * @brief Opus codec support functions
 */
namespace OpusCodecSupport {
    /**
     * @brief Register Opus codec with AudioCodecFactory (conditional compilation)
     */
    void registerCodec();
    
    /**
     * @brief Create Opus codec instance
     * @param stream_info Stream information
     * @return Unique pointer to OpusCodec or nullptr if not supported
     */
    std::unique_ptr<AudioCodec> createCodec(const StreamInfo& stream_info);
    
    /**
     * @brief Check if stream is Opus format
     * @param stream_info Stream information to check
     * @return true if stream is Opus format
     */
    bool isOpusStream(const StreamInfo& stream_info);
}

} // namespace Opus
} // namespace Codec
} // namespace PsyMP3

#endif // HAVE_OGGDEMUXER

#endif // OPUSCODEC_H