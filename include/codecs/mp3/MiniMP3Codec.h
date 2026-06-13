/*
 * MiniMP3Codec.h - AudioCodec-based MP3 decoder using minimp3
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef MINIMP3CODEC_H
#define MINIMP3CODEC_H

#include "../../../third_party/minimp3/minimp3.h"

namespace PsyMP3 {
namespace Codec {
namespace MP3 {

class MiniMP3Codec : public AudioCodec {
public:
    explicit MiniMP3Codec(const StreamInfo& stream_info);
    ~MiniMP3Codec() override;

    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    std::string getCodecName() const override { return "mp3"; }
    bool canDecode(const StreamInfo& stream_info) const override;

private:
    bool initialize_unlocked();
    AudioFrame decode_unlocked(const MediaChunk& chunk);
    AudioFrame flush_unlocked();
    void reset_unlocked();

    mp3dec_t m_decoder;
    uint32_t m_sample_rate = 0;
    uint16_t m_channels = 0;
    mutable std::mutex m_mutex;
};

namespace MiniMP3CodecSupport {
void registerCodec();
std::unique_ptr<AudioCodec> createCodec(const StreamInfo& stream_info);
bool isMP3Stream(const StreamInfo& stream_info);
}

} // namespace MP3
} // namespace Codec
} // namespace PsyMP3

#endif // MINIMP3CODEC_H
