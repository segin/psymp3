/*
 * MLPCodec.h - AudioCodec-based MLP/Dolby TrueHD decoder
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * The bundled MLP/TrueHD decoder (third_party/mlp) is used under the
 * Apache License, Version 2.0.
 */

#ifndef MLPCODEC_H
#define MLPCODEC_H

// Forward declaration only, so that the decoder's substream state -- some
// hundreds of kilobytes of fixed arrays -- stays out of psymp3.h. m_decoder is
// held by unique_ptr, so the destructor is defined in the .cpp where the type is
// complete.
namespace mlp {
class Decoder;
}

namespace PsyMP3 {
namespace Codec {
namespace MLP {

class MLPCodec : public AudioCodec {
public:
    explicit MLPCodec(const StreamInfo& stream_info);
    ~MLPCodec() override;

    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    std::string getCodecName() const override;
    bool canDecode(const StreamInfo& stream_info) const override;

private:
    AudioFrame decode_unlocked(const MediaChunk& chunk);

    std::unique_ptr<mlp::Decoder> m_decoder;
    std::vector<int32_t> m_pcm;         // scratch for one access unit
    mutable std::mutex m_mutex;
};

namespace MLPCodecSupport {
void registerCodec();
std::unique_ptr<AudioCodec> createCodec(const StreamInfo& stream_info);
bool isMLPStream(const StreamInfo& stream_info);
}

} // namespace MLP
} // namespace Codec
} // namespace PsyMP3

#endif // MLPCODEC_H
