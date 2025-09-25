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
    
    // Stream configuration
    uint32_t m_sample_rate = 48000;  // Opus always outputs at 48kHz
    uint16_t m_channels = 0;
    uint16_t m_pre_skip = 0;
    int16_t m_output_gain = 0;
    
    // Header processing state
    int m_header_packets_received = 0;
    bool m_decoder_initialized = false;
    
    // Decoding buffers
    std::vector<int16_t> m_output_buffer;
    std::vector<float> m_float_buffer;  // For float decoding if needed
    
    // Position tracking
    std::atomic<uint64_t> m_samples_decoded{0};
    std::atomic<uint64_t> m_samples_to_skip{0};
    
    // Error handling
    std::atomic<bool> m_error_state{false};
    std::string m_last_error;
    
    // Threading safety - following public/private lock pattern
    mutable std::mutex m_mutex;
    
    // Header processing (private unlocked methods)
    bool processHeaderPacket_unlocked(const std::vector<uint8_t>& packet_data);
    bool processIdentificationHeader_unlocked(const std::vector<uint8_t>& packet_data);
    bool processCommentHeader_unlocked(const std::vector<uint8_t>& packet_data);
    
    // Audio decoding (private unlocked methods)
    AudioFrame decodeAudioPacket_unlocked(const std::vector<uint8_t>& packet_data);
    void applyPreSkip_unlocked(AudioFrame& frame);
    void applyOutputGain_unlocked(AudioFrame& frame);
    
    // Utility methods (private unlocked methods)
    bool validateOpusPacket_unlocked(const std::vector<uint8_t>& packet_data);
    void handleDecoderError_unlocked(int opus_error);
    void resetDecoderState_unlocked();
    bool initializeOpusDecoder_unlocked();
};

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

#endif // OPUSCODEC_H