/*
 * ALACCodec.h - AudioCodec-based Apple Lossless (ALAC) decoder
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * The bundled ALAC decoder (third_party/alac) is Copyright © 2011 Apple Inc.
 * and is used under the Apache License, Version 2.0.
 */

#ifndef ALACCODEC_H
#define ALACCODEC_H

// Forward declaration only: Apple's ALACDecoder header pulls in ALACAudioTypes.h,
// whose fourcc constants trip -Werror=multichar. Keeping it out of this header
// (and therefore out of psymp3.h) confines that to ALACCodec.cpp, where the
// implementation is #included behind a diagnostic pragma. m_decoder is held by
// unique_ptr, so the destructor is defined in the .cpp where the type is complete.
class ALACDecoder;

namespace PsyMP3 {
namespace Codec {
namespace ALAC {

class ALACCodec : public AudioCodec {
public:
    explicit ALACCodec(const StreamInfo& stream_info);
    ~ALACCodec() override;

    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    std::string getCodecName() const override { return "alac"; }
    bool canDecode(const StreamInfo& stream_info) const override;

private:
    bool initialize_unlocked();
    AudioFrame decode_unlocked(const MediaChunk& chunk);

    std::unique_ptr<ALACDecoder> m_decoder;
    std::vector<uint8_t> m_magic_cookie; // raw 'alac' box content (codec_data)
    uint32_t m_sample_rate = 0;
    uint16_t m_channels = 0;
    uint16_t m_bit_depth = 16;
    uint32_t m_frame_length = 4096;
    mutable std::mutex m_mutex;
};

namespace ALACCodecSupport {
void registerCodec();
std::unique_ptr<AudioCodec> createCodec(const StreamInfo& stream_info);
bool isALACStream(const StreamInfo& stream_info);
}

} // namespace ALAC
} // namespace Codec
} // namespace PsyMP3

#endif // ALACCODEC_H
