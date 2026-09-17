/*
 * G722Codec.h - G.722 audio codec
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef G722CODEC_H
#define G722CODEC_H

namespace PsyMP3 {
namespace Codec {
namespace PCM {

/// G.722 wideband ADPCM, decoded in-tree by G722Decoder.
///
/// Not internally synchronised, and deliberately so. Like the other PCM codecs
/// it holds no mutex: DemuxedStream serialises decode(), flush() and reset()
/// under its own m_decode_mutex -- getData() and seekTo() both take it -- and
/// initialize() runs before the stream is shared. A lock here would only nest
/// inside one every caller already holds.
class G722Codec : public AudioCodec {
public:
    explicit G722Codec(const StreamInfo& stream_info);
    ~G722Codec() override;

    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    std::string getCodecName() const override { return "g722"; }
    bool canDecode(const StreamInfo& stream_info) const override;

private:
    std::unique_ptr<G722Decoder> m_decoder;
};

void registerG722Codec();

} // namespace PCM
} // namespace Codec
} // namespace PsyMP3

#endif // G722CODEC_H
