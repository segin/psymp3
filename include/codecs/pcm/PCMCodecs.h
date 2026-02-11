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

// No direct includes - all includes should be in psymp3.h

// Forward declarations for namespaced codec types
namespace PsyMP3 {
namespace Codec {
namespace MP3 {
    class Libmpg123;
}
}
namespace IO {
    class MemoryIOHandler;
}
}

namespace PsyMP3 {
namespace Codec {
namespace PCM {

// Make SimplePCMCodec available in this namespace
using PsyMP3::Codec::SimplePCMCodec;

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
    PsyMP3::Codec::MP3::Libmpg123* m_mp3_stream = nullptr;  // Fully qualified
    PsyMP3::IO::MemoryIOHandler* m_io_handler = nullptr;    // Raw pointer to handler (owned by m_mp3_stream)
    std::vector<uint8_t> m_buffer;            // Accumulated data buffer (initial only)
    bool m_header_written = false;
};

} // namespace PCM
} // namespace Codec
} // namespace PsyMP3

#endif // PCMCODECS_H