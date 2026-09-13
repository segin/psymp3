/*
 * AC3FrameDecoder.h - AC-3 and E-AC-3 syncframes to PCM
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
#include <memory>
#include <vector>

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

/// Samples one six-block syncframe yields per channel.
constexpr unsigned kFrameSamples = kBlocksPerFrame * kBlockSamples;

/// Drives syncframes to PCM: header, audio blocks, the inverse transform for
/// each coded channel, and the E-AC-3 steps that need more than one block.
///
/// AC-3 output is immediate. E-AC-3 output runs one syncframe behind:
/// enhanced coupling rebuilds a block from its neighbours on both sides
/// (§E3.5.5.1), and a frame's last block has its next neighbour in the
/// following frame; transient pre-noise processing (§E3.7) then works on the
/// whole finished frame. The lag is in when samples are handed over, not in
/// where they fall -- the stream stays sample-aligned, and flush() releases
/// the last frame.
///
/// Kept separate from the AudioCodec wrapper so the decode can be diffed
/// against a reference without a demuxer or an audio device in the way.
class AC3FrameDecoder {
public:
    /// Decode one syncframe into interleaved WAVE-order samples (L R C LFE
    /// Ls Rs): this frame's for AC-3, the previous frame's for E-AC-3 (none
    /// for the first), and none for a substream this decoder does not play.
    /// Returns false for a malformed frame, with @p pcm left empty.
    bool decode(const uint8_t* data, size_t size, std::vector<float>& pcm);

    /// Release an E-AC-3 frame still held back, at the end of the stream.
    void flush(std::vector<float>& pcm);

    /// Channels in the last frame released, including LFE.
    unsigned channels() const { return m_channels; }
    /// Sample rate of the last frame released.
    unsigned sampleRate() const { return m_sample_rate; }

    /// Drop all carry-over, for a seek.
    void reset();

private:
    /// A parsed syncframe and the PCM it is being finished into.
    struct Frame {
        bool valid = false;
        bool eac3 = false;
        unsigned blocks = 0;
        unsigned channels = 0;
        unsigned fbw = 0;
        unsigned acmod = 0;
        bool lfeon = false;
        unsigned sample_rate = 0;
        EAC3AudioFrame params;
        std::unique_ptr<AC3Block[]> block;   // large, so on the heap
        std::vector<float> pcm;
    };

    /// Everything after parsing for one block: enhanced coupling against its
    /// neighbours (@p next may be null), deferred spectral extension, dynamic
    /// range control, and the inverse transform into the frame's PCM.
    void finishBlock(Frame& frame, unsigned index, const AC3Block* next);
    /// Transient pre-noise processing over a finished frame, then hand it on.
    void releaseFrame(Frame& frame, std::vector<float>& pcm);

    AC3FrameState m_state;
    AC3TransformState m_transforms[kChannelSlots];
    EAC3TransientPreNoise m_tpnp[kMaxFullBandwidthChannels];
    EAC3EcplRandom m_ecpl_random;
    /// The last finished block's coupling channel as windowed time samples,
    /// the "previous" of §E3.5.5.1, when that block used enhanced coupling.
    float m_previous_coupling[kTransformSize] = {};
    bool m_previous_ecpl = false;
    Frame m_pending;
    unsigned m_channels = 0;
    unsigned m_sample_rate = 0;
};

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3

#endif // PSYMP3_CODECS_AC3_AC3FRAMEDECODER_H
