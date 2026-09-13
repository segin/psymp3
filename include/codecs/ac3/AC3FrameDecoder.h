/*
 * AC3FrameDecoder.h - One AC-3 syncframe to PCM
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef PSYMP3_CODECS_AC3_AC3FRAMEDECODER_H
#define PSYMP3_CODECS_AC3_AC3FRAMEDECODER_H

#include <cstdint>
#include <cstddef>
#include <vector>

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

/// Samples one syncframe yields per channel: kBlocksPerFrame of 256.
constexpr unsigned kFrameSamples = kBlocksPerFrame * kBlockSamples;

/// Drives a whole syncframe: header, six audio blocks, and the inverse
/// transform for each coded channel.
///
/// Kept separate from the AudioCodec wrapper so the decode path can be
/// exercised without a demuxer or an audio device -- the interesting failures
/// are all in here, and they are much easier to see against a reference
/// decode than through the pipeline.
class AC3FrameDecoder {
public:
    /// Decode one syncframe into interleaved samples.
    ///
    /// Output is in WAVE channel order (L R C LFE Ls Rs), not the bitstream's
    /// own order, so it can go straight to an audio device. Emits 256
    /// samples per channel per audio block on success -- 1536 for AC-3, as few
    /// as 256 for a one-block E-AC-3 frame -- and none for an E-AC-3
    /// dependent substream, which extends a program rather than adding time.
    ///
    /// @param data   the frame, starting at its sync word
    /// @param size   bytes available; the frame's own length is taken from
    ///               the header and must fit
    /// @param pcm    receives channels() * kFrameSamples interleaved floats
    /// @return false if the frame is malformed, in which case pcm is
    ///         untouched and the decoder's carry-over is left alone
    bool decode(const uint8_t* data, size_t size, std::vector<float>& pcm);

    /// Channels in the last frame decoded, including LFE.
    unsigned channels() const { return m_channels; }
    /// Sample rate of the last frame decoded.
    unsigned sampleRate() const { return m_sample_rate; }

    /// Drop the carry-over, for a seek. The block after a reset reconstructs
    /// against a zero delay line and so is not the signal the encoder meant;
    /// a caller that cares should decode one frame and discard it.
    void reset();

private:
    AC3FrameState m_state;
    AC3TransformState m_transforms[kChannelSlots];
    /// Per full-bandwidth channel, in bitstream order. Fed every E-AC-3
    /// frame whether or not it corrects anything, since a correction can
    /// reach back into the previous frame's output.
    EAC3TransientPreNoise m_tpnp[kMaxFullBandwidthChannels];
    unsigned m_channels = 0;
    unsigned m_sample_rate = 0;
};

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3

#endif // PSYMP3_CODECS_AC3_AC3FRAMEDECODER_H
