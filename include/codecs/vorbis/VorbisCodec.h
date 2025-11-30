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
    std::vector<int16_t> m_output_buffer;
    float** m_pcm_channels = nullptr;
    
    // ========== Block size handling ==========
    // Requirements: 4.1, 4.2
    uint32_t m_block_size_short = 0;
    uint32_t m_block_size_long = 0;
    
    // ========== Position tracking ==========
    std::atomic<uint64_t> m_samples_decoded{0};
    std::atomic<uint64_t> m_granule_position{0};
    
    // ========== Error handling ==========
    // Requirements: 8.1, 8.2, 8.3
    std::atomic<bool> m_error_state{false};
    std::string m_last_error;
    
    // ========== Threading safety ==========
    // Following public/private lock pattern per threading-safety-guidelines.md
    // Lock acquisition order (to prevent deadlocks):
    // 1. VorbisCodec::m_mutex (instance-level lock)
    // Note: VorbisCodec only uses a single mutex, so no deadlock concerns within this class
    // Requirements: 10.1, 10.2, 10.3
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
    
    // ========== Block size and windowing (private unlocked methods) ==========
    void handleVariableBlockSizes_unlocked(const vorbis_block* block);
    
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
