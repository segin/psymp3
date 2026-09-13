/*
 * AC3FrameDecoder.cpp - AC-3 and E-AC-3 syncframes to PCM
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

namespace {

/// Where each bitstream channel belongs in WAVE order (L R C LFE Ls Rs).
///
/// A/52 Table 5.8 orders the coded channels the way the arrangement is
/// written -- 3/2 is L, C, R, Ls, Rs -- which puts centre second. Every audio
/// device expects centre third and the LFE fourth, so the two disagree for
/// any mode with a centre channel. Indexed by acmod; -1 marks a slot the mode
/// does not carry.
constexpr int kWaveOrder[8][5] = {
    { 0,  1, -1, -1, -1},  // 1+1: two independent programmes, left as they are
    { 0, -1, -1, -1, -1},  // 1/0: C alone, so it is the only channel
    { 0,  1, -1, -1, -1},  // 2/0: L R
    { 0,  2,  1, -1, -1},  // 3/0: L C R     -> L R C
    { 0,  1,  2, -1, -1},  // 2/1: L R S
    { 0,  2,  1,  3, -1},  // 3/1: L C R S   -> L R C S
    { 0,  1,  2,  3, -1},  // 2/2: L R Ls Rs
    { 0,  2,  1,  3,  4},  // 3/2: L C R Ls Rs -> L R C Ls Rs
};

/// The interleaved output slot of full-bandwidth channel @p ch, or -1. The
/// LFE takes WAVE slot 3, so everything from there on moves up one.
int outputSlot(unsigned acmod, bool lfeon, unsigned ch)
{
    const int slot = kWaveOrder[acmod][ch];
    if (slot < 0) {
        return -1;
    }
    return (lfeon && slot >= 3) ? slot + 1 : slot;
}

} // namespace

void AC3FrameDecoder::reset()
{
    m_state = AC3FrameState();
    for (auto& transform : m_transforms) {
        transform = AC3TransformState();
    }
    for (auto& tpnp : m_tpnp) {
        tpnp.reset();
    }
    m_pending = Frame();
    m_previous_ecpl = false;
    std::fill(std::begin(m_previous_coupling), std::end(m_previous_coupling), 0.0f);
}

void AC3FrameDecoder::finishBlock(Frame& frame, unsigned index, const AC3Block* next)
{
    AC3Block& block = frame.block[index];

    // --- enhanced coupling, §E3.5.5 ---
    // The coupling channel is rebuilt free of time-domain aliasing from this
    // block and both neighbours; a neighbour without enhanced coupling
    // contributes silence.
    float current[kTransformSize] = {};
    if (block.ecplinu) {
        static const float kSilence[kTransformSize] = {};
        ac3WindowedImdct(block.ecpl_coupling, false, current);
        float following[kTransformSize] = {};
        if (next && next->ecplinu) {
            ac3WindowedImdct(next->ecpl_coupling, false, following);
        }
        float zr[kBlockSamples];
        float zi[kBlockSamples];
        eac3EcplAnalyse(m_previous_ecpl ? m_previous_coupling : kSilence, current, following, zr, zi);
        for (unsigned ch = 0; ch < frame.fbw; ++ch) {
            if (block.chincpl[ch]) {
                eac3EcplRegenerate(zr, zi, block.ecpl_bands, block.ecpl[ch], ch,
                                   block.ecpl_angle_interpolation, m_ecpl_random,
                                   block.coefficients[ch]);
            }
        }
    }
    if (block.ecplinu) {
        std::copy(std::begin(current), std::end(current), std::begin(m_previous_coupling));
    }
    m_previous_ecpl = block.ecplinu;

    // Spectral extension held back by the parser because the channel's low
    // band included enhanced-coupled bins, which are only now finished.
    for (unsigned ch = 0; ch < frame.fbw; ++ch) {
        if (block.spx_deferred[ch]) {
            eac3SpxSynthesise(block.coefficients[ch], block.spx_bands, block.spx[ch],
                              m_state.spx_noise);
        }
    }

    // --- dynamic range control and the inverse transform ---
    // §7.7.1: the gain scales the block before it is transformed, which is
    // the same as scaling its output and keeps the overlap with neighbouring
    // blocks consistent. In 1+1 mode channel 2 has its own.
    float samples[kBlockSamples];
    float* dst = frame.pcm.data() + static_cast<size_t>(index) * kBlockSamples * frame.channels;
    for (unsigned ch = 0; ch < frame.fbw; ++ch) {
        const float gain = (ch == 1) ? block.dynamic_range2 : block.dynamic_range;
        if (gain != 1.0f) {
            for (unsigned bin = 0; bin < kBlockSamples; ++bin) {
                block.coefficients[ch][bin] *= gain;
            }
        }
        ac3InverseTransform(block.coefficients[ch], block.block_switch[ch],
                            m_transforms[ch], samples);
        const int out = outputSlot(frame.acmod, frame.lfeon, ch);
        if (out < 0) {
            continue;
        }
        for (unsigned n = 0; n < kBlockSamples; ++n) {
            dst[n * frame.channels + static_cast<unsigned>(out)] = samples[n];
        }
    }
    if (frame.lfeon) {
        // The LFE is never block-switched: A/52 §5.4.2.1 gives blksw only to
        // the full-bandwidth channels.
        if (block.dynamic_range != 1.0f) {
            for (unsigned bin = 0; bin < kBlockSamples; ++bin) {
                block.coefficients[kLfeSlot][bin] *= block.dynamic_range;
            }
        }
        ac3InverseTransform(block.coefficients[kLfeSlot], false, m_transforms[kLfeSlot], samples);
        const unsigned out = std::min(3u, frame.channels - 1);
        for (unsigned n = 0; n < kBlockSamples; ++n) {
            dst[n * frame.channels + out] = samples[n];
        }
    }
}

void AC3FrameDecoder::releaseFrame(Frame& frame, std::vector<float>& pcm)
{
    // --- transient pre-noise processing, §E3.7 ---
    // On the finished frame, channel by channel. Every E-AC-3 frame goes
    // through it so each channel's history stays continuous, because a
    // correction in one frame may copy from the one before.
    if (frame.eac3) {
        const size_t samples = static_cast<size_t>(frame.blocks) * kBlockSamples;
        std::vector<float> channel(samples);
        for (unsigned ch = 0; ch < frame.fbw; ++ch) {
            const int out = outputSlot(frame.acmod, frame.lfeon, ch);
            if (out < 0) {
                continue;
            }
            for (size_t n = 0; n < samples; ++n) {
                channel[n] = frame.pcm[n * frame.channels + static_cast<unsigned>(out)];
            }
            m_tpnp[ch].process(channel.data(), samples, frame.params.chintransproc[ch],
                               frame.params.transprocloc[ch], frame.params.transproclen[ch]);
            for (size_t n = 0; n < samples; ++n) {
                frame.pcm[n * frame.channels + static_cast<unsigned>(out)] = channel[n];
            }
        }
    }
    m_channels = frame.channels;
    m_sample_rate = frame.sample_rate;
    pcm.insert(pcm.end(), frame.pcm.begin(), frame.pcm.end());
}

bool AC3FrameDecoder::decode(const uint8_t* data, size_t size, std::vector<float>& pcm)
{
    pcm.clear();

    AC3FrameHeader header;
    if (!parseAC3FrameHeader(data, size, header) || !header.isDecodable()) {
        return false;
    }
    if (header.frame_size == 0 || header.frame_size > size) {
        return false;
    }

    AC3BitReader reader(data, header.frame_size);
    AC3FrameHeader consumed;
    Frame frame;
    frame.eac3 = header.isEAC3();
    if (frame.eac3) {
        const char* why = nullptr;
        if (!eac3ParseFrame(reader, consumed, frame.params, &why)) {
            Debug::log("ac3", "E-AC-3 frame header failed: ", why ? why : "unknown");
            return false;
        }
        if (header.isAuxiliarySubstream()) {
            // §E3.8.1: a reference decoder plays independent substream 0 and
            // skips everything else. A dependent substream carries channels
            // beyond that program's 5.1 -- rendering them is optional and not
            // done here -- and an independent substream with another id is a
            // different program. Either way this program's state is untouched.
            return true;
        }
    } else if (!ac3ParseFrameHeader(reader, consumed)) {
        return false;
    }

    // Each syncframe starts its own bit allocation and exponent history --
    // that is what makes a frame the unit a decoder can resynchronise on --
    // but the transform's delay line survives, because the overlap that
    // reconstructs the signal spans the frame boundary. The noise generators
    // survive too: restarting them every frame would repeat the same noise
    // 31 times a second.
    const AC3Dither dither = m_state.dither;
    const EAC3SpxNoise spx_noise = m_state.spx_noise;
    m_state = AC3FrameState();
    m_state.dither = dither;
    m_state.spx_noise = spx_noise;

    frame.blocks = header.blocks;
    frame.channels = header.outputChannels();
    frame.fbw = header.channels;
    frame.acmod = static_cast<unsigned>(header.acmod);
    frame.lfeon = header.lfeon;
    frame.sample_rate = header.sample_rate;
    frame.block.reset(new AC3Block[frame.blocks]);
    frame.pcm.assign(static_cast<size_t>(frame.channels) * frame.blocks * kBlockSamples, 0.0f);

    for (unsigned b = 0; b < frame.blocks; ++b) {
        const char* reason = nullptr;
        if (!ac3ParseAudioBlock(reader, header, m_state, frame.block[b], &reason,
                                frame.eac3 ? &frame.params : nullptr)) {
            Debug::log("ac3", "frame failed at block ", b, ": ", reason ? reason : "unknown");
            return false;
        }
    }
    frame.valid = true;

    if (!frame.eac3) {
        for (unsigned b = 0; b < frame.blocks; ++b) {
            finishBlock(frame, b, nullptr);
        }
        releaseFrame(frame, pcm);
        return true;
    }

    // E-AC-3: the frame held back last time can now finish its last block,
    // with this frame's first as its next neighbour, and be released.
    if (m_pending.valid) {
        const AC3Block* next = m_pending.acmod == frame.acmod ? &frame.block[0] : nullptr;
        finishBlock(m_pending, m_pending.blocks - 1, next);
        releaseFrame(m_pending, pcm);
    }
    for (unsigned b = 0; b + 1 < frame.blocks; ++b) {
        finishBlock(frame, b, &frame.block[b + 1]);
    }
    m_pending = std::move(frame);
    return true;
}

void AC3FrameDecoder::flush(std::vector<float>& pcm)
{
    pcm.clear();
    if (!m_pending.valid) {
        return;
    }
    // The last block of the stream has no next neighbour.
    finishBlock(m_pending, m_pending.blocks - 1, nullptr);
    releaseFrame(m_pending, pcm);
    m_pending = Frame();
}

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3
