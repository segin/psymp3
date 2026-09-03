/*
 * XHEAACCodec.h - xHE-AAC (MPEG-D USAC) audio codec via FDK-AAC
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef XHEAACCODEC_H
#define XHEAACCODEC_H

namespace PsyMP3 {
namespace Codec {
namespace AAC {

/**
 * @brief Decoder for xHE-AAC, i.e. MPEG-D USAC (audio object type 42).
 *
 * A separate class from AACCodec rather than a mode of it, because it is a
 * separate decoder: libfaad2 stops at object type 27 and has no USAC support
 * whatsoever, so xHE streams cannot be routed to it. xHE-AAC is also not a
 * profile of AAC-LC the way HE-AAC is -- it carries an ACELP/TCX speech core
 * alongside the transform core -- so there is no graceful degradation either.
 *
 * The demuxer decides which decoder a stream gets by inspecting the
 * AudioSpecificConfig's audioObjectType (see BoxParser), because both share
 * the esds objectTypeIndication 0x40.
 */
class XHEAACCodec : public AudioCodec {
public:
    explicit XHEAACCodec(const StreamInfo& stream_info);
    ~XHEAACCodec() override;

    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    std::string getCodecName() const override { return "xhe-aac"; }
    std::string getCodecProfile() const override { return "xHE-AAC"; }
    bool canDecode(const StreamInfo& stream_info) const override;

    /// True when the AudioSpecificConfig describes USAC (object type 42).
    static bool isUSACConfig(const std::vector<uint8_t>& asc);

private:
    bool initialize_unlocked();
    AudioFrame decode_unlocked(const MediaChunk& chunk);
    void destroyDecoder_unlocked();
    bool applyLoudnessConfig_unlocked();

    void* m_decoder = nullptr;   ///< HANDLE_AACDECODER, opaque here
    uint32_t m_sample_rate = 0;
    uint16_t m_channels = 0;
    uint32_t m_container_sample_rate = 0;
    bool m_decoder_initialized = false;
    mutable std::mutex m_mutex;
};

namespace XHEAACCodecSupport {
void registerCodec();
}

} // namespace AAC
} // namespace Codec
} // namespace PsyMP3

#endif // XHEAACCODEC_H
