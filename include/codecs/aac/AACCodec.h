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

/**
 * @brief The whole AAC family through one FDK-AAC decoder.
 *
 * AAC-LC, HE-AACv1 (SBR), HE-AACv2 (SBR + Parametric Stereo) and xHE-AAC
 * (MPEG-D USAC, object type 42) are all decoded here. FDK handles them
 * uniformly, so the only thing that varies by profile is whether loudness
 * normalisation is applied -- xHE-AAC expects it, the rest do not.
 */
class AACCodec : public AudioCodec {
public:
    explicit AACCodec(const StreamInfo& stream_info);
    ~AACCodec() override;

    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    std::string getCodecName() const override;
    /// "AAC-LC", "HE-AACv1", "HE-AACv2", "xHE-AAC", ... Empty until a frame
    /// has decoded, because SBR and PS are usually only found in the bitstream.
    std::string getCodecProfile() const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_profile;
    }
    bool canDecode(const StreamInfo& stream_info) const override;

    /// True when the AudioSpecificConfig describes USAC (object type 42).
    static bool isUSACConfig(const std::vector<uint8_t>& asc);

private:
    bool initialize_unlocked();
    AudioFrame decode_unlocked(const MediaChunk& chunk);
    void destroyDecoder_unlocked();
    void updateProfile_unlocked(int aot, int ext_aot);

    void* m_decoder = nullptr;   ///< HANDLE_AACDECODER, opaque here
    std::vector<int16_t> m_pcm;  ///< decode scratch, sized once
    uint32_t m_sample_rate = 0;
    uint16_t m_channels = 0;
    bool m_decoder_initialized = false;
    bool m_is_usac = false;
    std::string m_profile;  ///< set from the first decoded frame
    /// The container's rate, kept separately because m_sample_rate is later
    /// replaced by the decoder's (which SBR may have doubled). Chunk
    /// timestamps are counted in container sample units, so they must be
    /// converted with this one.
    uint32_t m_container_sample_rate = 0;
    int m_priming_frames = 0;  ///< post-seek warm-up frames to decode and discard
    /// FDK's own latency, which it reports per frame and which sits on top
    /// of whatever priming the container states. Dropped here so callers see
    /// the same first sample any other decoder would give them.
    uint32_t m_decoder_delay_remaining = 0;
    bool m_decoder_delay_known = false;
    /// How much of the decoder's delay is still owed at end of stream.
    /// FDK answers AACDEC_FLUSH for as long as it is asked, so draining it
    /// has to stop after exactly the delay it declared or the file never
    /// ends.
    uint32_t m_flush_remaining = 0;
    bool m_flushing = false;
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
