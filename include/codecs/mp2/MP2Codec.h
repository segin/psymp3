/*
 * MP2Codec.h - AudioCodec-based MPEG-1/2 Audio Layer II decoder using kjmp2
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * The bundled Layer II decoder (third_party/kjmp2) is Copyright © 2006-2013
 * Martin J. Fiedler and is used under the zlib license; see
 * third_party/kjmp2/kjmp2.txt.
 */

#ifndef MP2CODEC_H
#define MP2CODEC_H

// kjmp2 is a C library; give its declarations C linkage so they match the
// implementation compiled (with C linkage) in MP2Codec.cpp.
extern "C" {
#include "../../../third_party/kjmp2/kjmp2.h"
}

namespace PsyMP3 {
namespace Codec {
namespace MP2 {

class MP2Codec : public AudioCodec {
public:
    explicit MP2Codec(const StreamInfo& stream_info);
    ~MP2Codec() override;

    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    std::string getCodecName() const override { return "mp2"; }
    bool canDecode(const StreamInfo& stream_info) const override;

private:
    bool initialize_unlocked();
    AudioFrame decode_unlocked(const MediaChunk& chunk);
    void reset_unlocked();

    kjmp2_context_t m_decoder;
    uint32_t m_sample_rate = 0;
    mutable std::mutex m_mutex;
};

namespace MP2CodecSupport {
void registerCodec();
std::unique_ptr<AudioCodec> createCodec(const StreamInfo& stream_info);
bool isMP2Stream(const StreamInfo& stream_info);
}

} // namespace MP2
} // namespace Codec
} // namespace PsyMP3

#endif // MP2CODEC_H
