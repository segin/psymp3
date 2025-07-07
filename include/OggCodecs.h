/*
 * OggCodecs.h - Audio codecs for Ogg-contained formats
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
 * TORTIOUS ACTION, ARISING OUT of OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef OGGCODECS_H
#define OGGCODECS_H

#include "AudioCodec.h"

/**
 * @brief Vorbis passthrough codec
 * 
 * This forwards Vorbis data to the existing vorbis decoder.
 * Used when Vorbis streams are found inside Ogg containers.
 */
class VorbisPassthroughCodec : public AudioCodec {
public:
    explicit VorbisPassthroughCodec(const StreamInfo& stream_info);
    ~VorbisPassthroughCodec() override;
    
    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    std::string getCodecName() const override { return "vorbis_passthrough"; }
    bool canDecode(const StreamInfo& stream_info) const override;
    
private:
    class Vorbis* m_vorbis_stream = nullptr;  // Forward declaration
    std::vector<uint8_t> m_buffer;                  // Accumulated data buffer
    bool m_headers_written = false;
};

/**
 * @brief FLAC passthrough codec for Ogg FLAC
 * 
 * This forwards FLAC data to the existing FLAC decoder.
 * Used when FLAC streams are found inside Ogg containers (Ogg FLAC).
 */
class OggFLACPassthroughCodec : public AudioCodec {
public:
    explicit OggFLACPassthroughCodec(const StreamInfo& stream_info);
    ~OggFLACPassthroughCodec() override;
    
    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    std::string getCodecName() const override { return "ogg_flac_passthrough"; }
    bool canDecode(const StreamInfo& stream_info) const override;
    
private:
    class FlacDecoder* m_flac_stream = nullptr;      // Forward declaration
    std::vector<uint8_t> m_buffer;                  // Accumulated data buffer
    bool m_headers_written = false;
};

/**
 * @brief Opus passthrough codec
 * 
 * This forwards Opus data to the existing opus decoder.
 * Used when Opus streams are found inside Ogg containers.
 */
class OpusPassthroughCodec : public AudioCodec {
public:
    explicit OpusPassthroughCodec(const StreamInfo& stream_info);
    ~OpusPassthroughCodec() override;
    
    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    std::string getCodecName() const override { return "opus_passthrough"; }
    bool canDecode(const StreamInfo& stream_info) const override;
    
private:
    class OpusFile* m_opus_stream = nullptr;      // Forward declaration
    std::vector<uint8_t> m_buffer;                  // Accumulated data buffer
    bool m_headers_written = false;
};

/**
 * @brief Speex codec (basic implementation)
 * 
 * Speex is a legacy codec primarily used for speech.
 * This provides basic support for Speex streams in Ogg containers.
 */
class SpeexCodec : public AudioCodec {
public:
    explicit SpeexCodec(const StreamInfo& stream_info);
    ~SpeexCodec() override;
    
    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    std::string getCodecName() const override { return "speex"; }
    bool canDecode(const StreamInfo& stream_info) const override;
    
private:
    // Speex decoder state would go here
    // For now, this is a placeholder that returns silence
    bool m_initialized_speex = false;
    uint32_t m_frame_size = 160;  // Default Speex frame size
};

#endif // OGGCODECS_H