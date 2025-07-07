/*
 * PCMCodecs.h - PCM and PCM-variant audio codecs
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

#ifndef PCMCODECS_H
#define PCMCODECS_H

#include "AudioCodec.h"

/**
 * @brief Linear PCM codec (8-bit, 16-bit, 24-bit, 32-bit integer and float)
 */
class PCMCodec : public SimplePCMCodec {
public:
    explicit PCMCodec(const StreamInfo& stream_info);
    
    std::string getCodecName() const override { return "pcm"; }
    bool canDecode(const StreamInfo& stream_info) const override;
    
protected:
    size_t convertSamples(const std::vector<uint8_t>& input_data, 
                          std::vector<int16_t>& output_samples) override;
    size_t getBytesPerInputSample() const override;
    
private:
    enum class PCMFormat {
        PCM_8_UNSIGNED,
        PCM_16_SIGNED,
        PCM_24_SIGNED,
        PCM_32_SIGNED,
        PCM_32_FLOAT
    } m_pcm_format;
    
    void detectPCMFormat();
};

/**
 * @brief A-law codec (ITU-T G.711)
 * 
 * A-law is a logarithmic companding algorithm used primarily in European
 * and international telecommunications systems. Common in VoIP and telephony.
 */
class ALawCodec : public SimplePCMCodec {
public:
    explicit ALawCodec(const StreamInfo& stream_info);
    
    std::string getCodecName() const override { return "alaw"; }
    bool canDecode(const StreamInfo& stream_info) const override;
    
protected:
    size_t convertSamples(const std::vector<uint8_t>& input_data, 
                          std::vector<int16_t>& output_samples) override;
    size_t getBytesPerInputSample() const override { return 1; }
    
private:
    /**
     * @brief Convert A-law sample to linear PCM
     */
    static int16_t alaw2linear(uint8_t alaw_sample);
};

/**
 * @brief μ-law (mu-law) codec (ITU-T G.711)
 * 
 * μ-law is a logarithmic companding algorithm used primarily in North American
 * and Japanese telecommunications systems. Common in VoIP and telephony.
 */
class MuLawCodec : public SimplePCMCodec {
public:
    explicit MuLawCodec(const StreamInfo& stream_info);
    
    std::string getCodecName() const override { return "mulaw"; }
    bool canDecode(const StreamInfo& stream_info) const override;
    
protected:
    size_t convertSamples(const std::vector<uint8_t>& input_data, 
                          std::vector<int16_t>& output_samples) override;
    size_t getBytesPerInputSample() const override { return 1; }
    
private:
    /**
     * @brief Convert μ-law sample to linear PCM
     */
    static int16_t mulaw2linear(uint8_t mulaw_sample);
};

/**
 * @brief Passthrough codec for existing MP3 decoder
 * 
 * This forwards MP3 data to the existing libmpg123-based decoder.
 * Used when MP3 streams are found inside containers like RIFF WAVE.
 */
class MP3PassthroughCodec : public AudioCodec {
public:
    explicit MP3PassthroughCodec(const StreamInfo& stream_info);
    ~MP3PassthroughCodec() override;
    
    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    std::string getCodecName() const override { return "mp3_passthrough"; }
    bool canDecode(const StreamInfo& stream_info) const override;
    
private:
    class MP3Stream* m_mp3_stream = nullptr;  // Forward declaration
    std::vector<uint8_t> m_buffer;            // Accumulated data buffer
    bool m_header_written = false;
};

#endif // PCMCODECS_H