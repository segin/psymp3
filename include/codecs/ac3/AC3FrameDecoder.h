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
/// each coded channel, the E-AC-3 steps that need more than one block, and
/// the mapping of coded channels onto output channels.
///
/// Decoded samples go into one planar line per coded channel, positioned by
/// absolute sample index, and are released from there. AC-3 releases all a
/// frame decodes at once. E-AC-3 cannot:
///
/// - Enhanced coupling rebuilds a block from its neighbours on both sides
///   (§E3.5.5.1), so a frame's last block waits for the next frame.
/// - Transient pre-noise processing (§E3.7) rewrites up to 1022 samples
///   before a transient, and a frame may describe a transient in the frame
///   after it. So samples are held back until no correction still to come
///   can reach them.
///
/// The hold is in when samples are handed over, not where they fall: output
/// is contiguous and sample-aligned, and flush() releases the rest.
///
/// Kept separate from the AudioCodec wrapper so the decode can be diffed
/// against a reference without a demuxer or an audio device in the way.
class AC3FrameDecoder {
public:
    /// Decode one syncframe, putting into @p pcm whatever interleaved samples
    /// that releases, in the order channels() describes. A substream this
    /// decoder does not play releases nothing. Returns false for a malformed
    /// frame, with @p pcm left empty.
    bool decode(const uint8_t* data, size_t size, std::vector<float>& pcm);

    /// Release everything still held back, at the end of a stream.
    void flush(std::vector<float>& pcm);

    /// Channels in the samples released, including LFE.
    unsigned channels() const { return m_output_channels; }
    /// Sample rate of the samples released.
    unsigned sampleRate() const { return m_sample_rate; }

    /// Drop all carry-over, for a seek.
    void reset();

private:
    /// A parsed syncframe whose blocks are still being finished.
    struct Frame {
        bool valid = false;
        bool eac3 = false;
        unsigned blocks = 0;
        unsigned fbw = 0;
        unsigned acmod = 0;
        bool lfeon = false;
        unsigned sample_rate = 0;
        EAC3AudioFrame params;
        std::unique_ptr<AC3Block[]> block;   // large, so on the heap
    };

    /// A transient pre-noise correction waiting for its transient to decode.
    struct Correction {
        unsigned channel = 0;
        EAC3TransientCorrection span;
    };

    /// Everything after parsing for one block: enhanced coupling against its
    /// neighbours (@p next may be null), deferred spectral extension, dynamic
    /// range control, and the inverse transform onto the lines.
    void finishBlock(Frame& frame, unsigned index, const AC3Block* next);
    /// Queue a frame's corrections, positioned from its first sample.
    void queueCorrections(const Frame& frame, uint64_t frame_start);
    /// Apply every queued correction whose transient has been decoded.
    void applyCorrections();
    /// Hand on samples up to @p end, mapped to the output channels.
    void release(uint64_t end, std::vector<float>& pcm);
    /// Finish and release everything, with no next frame to wait for.
    void drain(std::vector<float>& pcm);
    /// Start releasing in @p frame's channel arrangement.
    void setLayout(const Frame& frame);

    AC3FrameState m_state;
    AC3TransformState m_transforms[kChannelSlots];
    EAC3EcplRandom m_ecpl_random;
    /// The last finished block's coupling channel as windowed time samples,
    /// the "previous" of §E3.5.5.1, when that block used enhanced coupling.
    float m_previous_coupling[kTransformSize] = {};
    bool m_previous_ecpl = false;
    Frame m_pending;

    /// The full-bandwidth channels in bitstream order, then the LFE.
    static constexpr unsigned kLines = kMaxFullBandwidthChannels + 1;
    std::vector<float> m_line[kLines];
    uint64_t m_line_start = 0;   ///< absolute position of each line's first sample
    uint64_t m_decoded = 0;      ///< one past the last sample decoded
    uint64_t m_released = 0;     ///< one past the last sample handed on
    uint64_t m_next_frame = 0;   ///< where the next frame's first sample falls
    std::vector<Correction> m_corrections;

    // The arrangement samples are being released in.
    bool m_have_layout = false;
    bool m_layout_eac3 = false;
    unsigned m_layout_acmod = 0;
    bool m_layout_lfeon = false;
    unsigned m_sample_rate = 0;
    unsigned m_output_channels = 0;
    /// Gain from each line to each output channel.
    float m_gain[8][kLines] = {};
};

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3

#endif // PSYMP3_CODECS_AC3_AC3FRAMEDECODER_H
