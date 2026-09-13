/*
 * AC3Codec.h - AudioCodec-based AC-3 and E-AC-3 (A/52) decoder
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef PSYMP3_CODECS_AC3_AC3CODEC_H
#define PSYMP3_CODECS_AC3_AC3CODEC_H

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

/// Adapts AC3FrameDecoder to the pipeline: MediaChunks in, full-scale S32
/// AudioFrames out.
///
/// A chunk is not assumed to be one syncframe. MP4 and Matroska do deliver
/// exactly one, but RIFF has no framing of its own: WAVE and AVI hand over
/// the data chunk in block_align-sized slices, and encoders set block_align
/// to anything from one frame to several. So input is buffered, every
/// complete syncframe in it is decoded, and a partial frame at the end waits
/// for the next chunk.
///
/// E-AC-3 goes through the same decoder, whose output runs one syncframe
/// behind its input; flush() releases the last frame at the end of a stream.
class AC3Codec : public AudioCodec {
public:
    explicit AC3Codec(const StreamInfo& stream_info);
    ~AC3Codec() override = default;

    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    std::string getCodecName() const override { return m_stream_info.codec_name == "eac3" ? "eac3" : "ac3"; }
    bool canDecode(const StreamInfo& stream_info) const override;

private:
    AudioFrame decode_unlocked(const MediaChunk& chunk);
    /// Scale decoded floats to full-scale S32 and stamp them.
    AudioFrame toAudioFrame_unlocked(const std::vector<float>& pcm);

    AC3FrameDecoder m_decoder;
    std::vector<float> m_pcm;       // scratch for one syncframe
    std::vector<float> m_out;       // everything decoded from one chunk
    std::vector<uint8_t> m_pending; // bytes not yet forming a whole syncframe
    /// Sample position of the next sample handed on. Output is contiguous, so
    /// it is taken from the first chunk after a reset and then advanced by
    /// what is emitted -- which stays exact although E-AC-3 output lags its
    /// input by a frame.
    uint64_t m_output_timestamp = 0;
    bool m_output_started = false;
    mutable std::mutex m_mutex;
};

namespace AC3CodecSupport {
void registerCodec();
std::unique_ptr<AudioCodec> createCodec(const StreamInfo& stream_info);
bool isAC3Stream(const StreamInfo& stream_info);
}

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3

#endif // PSYMP3_CODECS_AC3_AC3CODEC_H
