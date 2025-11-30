/*
 * VorbisCodec.h - Container-agnostic Vorbis audio codec
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
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

#ifndef VORBISCODEC_H
#define VORBISCODEC_H

// No direct includes - all includes should be in psymp3.h

#ifdef HAVE_OGGDEMUXER

namespace PsyMP3 {
namespace Codec {
namespace Vorbis {

// Bring demuxer types into this namespace
using PsyMP3::Demuxer::DemuxedStream;

/**
 * @brief Vorbis header information structure
 * 
 * Contains parsed data from the Vorbis identification header.
 */
struct VorbisHeaderInfo {
    uint32_t version;
    uint8_t channels;
    uint32_t sample_rate;
    uint32_t bitrate_maximum;
    uint32_t bitrate_nominal;
    uint32_t bitrate_minimum;
    uint8_t blocksize_0;  // Short block size (power of 2)
    uint8_t blocksize_1;  // Long block size (power of 2)
    
    bool isValid() const;
    static VorbisHeaderInfo parseFromPacket(const std::vector<uint8_t>& packet_data);
};

/**
 * @brief Vorbis comment information structure
 * 
 * Contains parsed data from the Vorbis comment header.
 */
struct VorbisCommentInfo {
    std::string vendor_string;
    std::vector<std::pair<std::string, std::string>> user_comments;
    
    static VorbisCommentInfo parseFromPacket(const std::vector<uint8_t>& packet_data);
};

/**
 * @brief Vorbis decoder class using DemuxedStream with OggDemuxer
 * 
 * This class replaces the old vorbisfile-based implementation with a cleaner
 * approach that delegates to DemuxedStream which uses OggDemuxer for container
 * parsing and VorbisCodec for audio decoding.
 */
class Vorbis : public Stream
{
public:
    Vorbis(TagLib::String name);
    virtual ~Vorbis();
    
    // Stream interface implementation
    virtual size_t getData(size_t len, void *buf) override;
    virtual void seekTo(unsigned long pos) override;
    virtual bool eof() override;
    
private:
    std::unique_ptr<DemuxedStream> m_demuxed_stream;
};

/**
 * @brief Container-agnostic Vorbis audio codec
 * 
 * This codec decodes Vorbis bitstream data from any container format
 * (primarily Ogg Vorbis) into standard 16-bit PCM audio. It uses libvorbis
 * directly for efficient, accurate decoding and supports all Vorbis quality
 * levels and encoding configurations.
 * 
 * Requirements: 11.1, 2.8
 */
class VorbisCodec : public AudioCodec
{
public:
    /**
     * @brief Construct VorbisCodec with stream information
     * @param stream_info Information about the audio stream
     * 
     * Requirements: 11.1, 11.2
     */
    explicit VorbisCodec(const StreamInfo& stream_info);
    
    /**
     * @brief Destructor with proper libvorbis cleanup
     * 
     * Cleans up libvorbis structures in reverse initialization order:
     * vorbis_block_clear, vorbis_dsp_clear, vorbis_comment_clear, vorbis_info_clear
     * 
     * Requirements: 2.8
     */
    ~VorbisCodec() override;
    
    // AudioCodec interface implementation
    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    std::string getCodecName() const override { return "vorbis"; }
    bool canDecode(const StreamInfo& stream_info) const override;
    
private:
    // ========== libvorbis decoder state ==========
    // Requirements: 2.1, 2.8
    vorbis_info m_vorbis_info;
    vorbis_comment m_vorbis_comment;
    vorbis_dsp_state m_vorbis_dsp;
    vorbis_block m_vorbis_block;
    
    // ========== Stream configuration ==========
    uint32_t m_sample_rate = 0;
    uint16_t m_channels = 0;
    uint16_t m_bits_per_sample = 16;  // Always output 16-bit
    
    // ========== Header processing state ==========
    // Requirements: 1.1, 2.2
    int m_header_packets_received = 0;
    bool m_decoder_initialized = false;
    
    // ========== Decoding buffers ==========
    // Requirements: 7.1, 7.2, 7.4
    std::vector<int16_t> m_output_buffer;
    float** m_pcm_channels = nullptr;
    
    // ========== Streaming and buffer management ==========
    // Requirements: 7.1, 7.2, 7.4
    // Maximum buffer size: 2 seconds at 48kHz stereo = 48000 * 2 * 2 = 192000 samples
    static constexpr size_t MAX_BUFFER_SAMPLES = 48000 * 2 * 2;  // 2 seconds at max rate
    static constexpr size_t BUFFER_HIGH_WATER_MARK = MAX_BUFFER_SAMPLES * 3 / 4;  // 75% full
    static constexpr size_t BUFFER_LOW_WATER_MARK = MAX_BUFFER_SAMPLES / 4;  // 25% full
    bool m_backpressure_active = false;  // True when buffer is too full
    
    // ========== Block size handling ==========
    // Requirements: 4.1, 4.2
    uint32_t m_block_size_short = 0;
    uint32_t m_block_size_long = 0;
    
    // ========== Position tracking ==========
    std::atomic<uint64_t> m_samples_decoded{0};
    std::atomic<uint64_t> m_granule_position{0};
    
    // ========== Error handling ==========
    // Thread-safe error state management (Requirements: 8.1, 8.2, 8.3, 10.7, 10.8)
    // - m_error_state: atomic for lock-free reads via isInErrorState()
    // - m_last_error: protected by m_mutex for thread-safe access
    // - All error logging uses PsyMP3's thread-safe Debug system
    std::atomic<bool> m_error_state{false};
    std::string m_last_error;
    
    // ========== Threading safety ==========
    // Following public/private lock pattern per threading-safety-guidelines.md
    // 
    // Thread Safety Guarantees (Requirements: 10.1, 10.2, 10.3, 10.5, 10.6):
    // - Each VorbisCodec instance maintains independent libvorbis state
    // - Multiple codec instances can operate concurrently without interference
    // - All public methods are thread-safe via mutex protection
    // - Atomic variables used for frequently-read state (error_state, samples_decoded)
    // - Destructor acquires lock to ensure no operations in progress during cleanup
    //
    // Lock acquisition order (to prevent deadlocks):
    // 1. VorbisCodec::m_mutex (instance-level lock)
    // Note: VorbisCodec only uses a single mutex, so no deadlock concerns within this class
    // External callers should not hold other locks when calling VorbisCodec methods
    //
    // Thread-safe error handling (Requirements: 10.7, 10.8):
    // - m_error_state is atomic for lock-free reads via isInErrorState()
    // - m_last_error is protected by m_mutex for thread-safe access
    // - Debug logging uses PsyMP3's thread-safe Debug system
    mutable std::mutex m_mutex;
    
    // ========== Header processing (private unlocked methods) ==========
    bool processHeaderPacket_unlocked(const std::vector<uint8_t>& packet_data);
    bool processIdentificationHeader_unlocked(const std::vector<uint8_t>& packet_data);
    bool processCommentHeader_unlocked(const std::vector<uint8_t>& packet_data);
    bool processSetupHeader_unlocked(const std::vector<uint8_t>& packet_data);
    
    // ========== Audio decoding (private unlocked methods) ==========
    AudioFrame decodeAudioPacket_unlocked(const std::vector<uint8_t>& packet_data);
    bool synthesizeBlock_unlocked();
    void convertFloatToPCM_unlocked(float** pcm, int samples, AudioFrame& frame);
    
public:
    // ========== Float to PCM conversion helpers (public for testing) ==========
    /**
     * @brief Convert a single float sample to 16-bit PCM with proper clamping
     * @param sample Float sample in range [-1.0, 1.0]
     * @return 16-bit signed PCM sample in range [-32768, 32767]
     * 
     * Requirements: 1.5, 5.1, 5.2
     */
    static int16_t floatToInt16(float sample);
    
    /**
     * @brief Interleave multi-channel float arrays into 16-bit PCM output
     * @param pcm Array of float channel pointers from libvorbis
     * @param samples Number of samples per channel
     * @param channels Number of channels
     * @param output Output vector for interleaved 16-bit samples
     * 
     * Requirements: 5.5, 5.7
     */
    static void interleaveChannels(float** pcm, int samples, int channels, 
                                   std::vector<int16_t>& output);
    
    // ========== Buffer status methods (public for testing and integration) ==========
    /**
     * @brief Get current buffer size in samples
     * @return Number of samples currently in the output buffer
     * 
     * Requirements: 7.2, 7.4
     */
    size_t getBufferSize() const;
    
    /**
     * @brief Get maximum buffer size in samples
     * @return Maximum number of samples the buffer can hold
     * 
     * Requirements: 7.2
     */
    static constexpr size_t getMaxBufferSize() { return MAX_BUFFER_SAMPLES; }
    
    /**
     * @brief Check if backpressure is currently active
     * @return true if buffer is too full and backpressure is being applied
     * 
     * Requirements: 7.4
     */
    bool isBackpressureActive() const;
    
    // ========== Error status methods (public for testing and integration) ==========
    /**
     * @brief Get the last error message
     * @return String describing the last error that occurred
     * 
     * Requirements: 8.8
     */
    std::string getLastError() const;
    
    /**
     * @brief Check if codec is in error state
     * @return true if a fatal error has occurred
     * 
     * Thread-safe: Uses atomic read, no lock required.
     * Requirements: 8.1, 8.2, 10.7
     */
    bool isInErrorState() const;
    
    /**
     * @brief Clear the error state (for error recovery)
     * 
     * Thread-safe: Uses atomic write and mutex for error message.
     * Requirements: 10.7
     */
    void clearErrorState();
    
private:
    
    // ========== Block size and windowing (private unlocked methods) ==========
    void handleVariableBlockSizes_unlocked(const vorbis_block* block);
    
    // ========== Streaming and buffer management (private unlocked methods) ==========
    // Requirements: 7.1, 7.2, 7.4
    /**
     * @brief Check if buffer has room for more samples
     * @return true if buffer can accept more samples, false if backpressure should be applied
     */
    bool canAcceptMoreSamples_unlocked() const;
    
    /**
     * @brief Get current buffer fill level as a percentage
     * @return Buffer fill percentage (0-100)
     */
    int getBufferFillPercent_unlocked() const;
    
    /**
     * @brief Apply backpressure logic based on buffer state
     * Updates m_backpressure_active based on high/low water marks
     */
    void updateBackpressureState_unlocked();
    
    // ========== Utility methods (private unlocked methods) ==========
    bool validateVorbisPacket_unlocked(const std::vector<uint8_t>& packet_data);
    void handleVorbisError_unlocked(int vorbis_error);
    void resetDecoderState_unlocked();
    void cleanupVorbisStructures_unlocked();
};

#ifdef HAVE_OGGDEMUXER

/**
 * @brief Vorbis codec support functions
 */
namespace VorbisCodecSupport {
    /**
     * @brief Register Vorbis codec with AudioCodecFactory (conditional compilation)
     */
    void registerCodec();
    
    /**
     * @brief Create Vorbis codec instance
     * @param stream_info Stream information
     * @return Unique pointer to VorbisCodec or nullptr if not supported
     */
    std::unique_ptr<AudioCodec> createCodec(const StreamInfo& stream_info);
    
    /**
     * @brief Check if stream is Vorbis format
     * @param stream_info Stream information to check
     * @return true if stream is Vorbis format
     */
    bool isVorbisStream(const StreamInfo& stream_info);
}

#endif // HAVE_OGGDEMUXER

} // namespace Vorbis
} // namespace Codec
} // namespace PsyMP3

#endif // HAVE_OGGDEMUXER

#endif // VORBISCODEC_H
