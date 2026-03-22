/*
 * AACCodec.h - Container-agnostic AAC audio codec
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef AACCODEC_H
#define AACCODEC_H

namespace PsyMP3 {
namespace Codec {
namespace AAC {

class AACCodec : public AudioCodec {
public:
    explicit AACCodec(const StreamInfo& stream_info);
    ~AACCodec() override;

    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    std::string getCodecName() const override { return "aac"; }
    bool canDecode(const StreamInfo& stream_info) const override;

private:
    bool initialize_unlocked();
    AudioFrame decode_unlocked(const MediaChunk& chunk);
    AudioFrame flush_unlocked();
    void reset_unlocked();
    void destroyDecoder_unlocked();
    bool configureDecoder_unlocked();
    bool initializeDecoderFromASC_unlocked();

    NeAACDecHandle m_decoder = nullptr;
    uint32_t m_sample_rate = 0;
    uint16_t m_channels = 0;
    bool m_decoder_initialized = false;
    mutable std::mutex m_mutex;
};

namespace AACCodecSupport {
void registerCodec();
std::unique_ptr<AudioCodec> createCodec(const StreamInfo& stream_info);
bool isAACStream(const StreamInfo& stream_info);
}

} // namespace AAC
} // namespace Codec
} // namespace PsyMP3

#endif // AACCODEC_H
