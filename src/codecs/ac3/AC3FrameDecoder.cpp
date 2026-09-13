/*
 * AC3FrameDecoder.cpp - One AC-3 syncframe to PCM
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
/// any mode with a centre channel. The LFE is appended to the bitstream order
/// but sits in the middle of the WAVE order, which is the other half of the
/// same mismatch.
///
/// Indexed by acmod; -1 marks a slot the mode does not carry.
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

} // namespace

void AC3FrameDecoder::reset()
{
    m_state = AC3FrameState();
    for (auto& transform : m_transforms) {
        transform = AC3TransformState();
    }
}

bool AC3FrameDecoder::decode(const uint8_t* data, size_t size, std::vector<float>& pcm)
{
    AC3FrameHeader header;
    if (!parseAC3FrameHeader(data, size, header) || !header.isDecodable()) {
        return false;
    }
    if (header.frame_size == 0 || header.frame_size > size) {
        return false;
    }

    AC3BitReader reader(data, header.frame_size);
    AC3FrameHeader consumed;
    EAC3AudioFrame eac3;
    const bool is_eac3 = header.isEAC3();
    if (is_eac3) {
        const char* why = nullptr;
        if (!eac3ParseFrame(reader, consumed, eac3, &why)) {
            Debug::log("ac3", "E-AC-3 frame header failed: ", why ? why : "unknown");
            return false;
        }
        if (header.strmtyp == 0x1) {
            // A dependent substream carries channels beyond the independent
            // program's 5.1 (§E3.8). Mixing them in is not supported, so the
            // independent program plays on its own; its state is left alone
            // so the next independent frame is unaffected.
            pcm.clear();
            return true;
        }
    } else if (!ac3ParseFrameHeader(reader, consumed)) {
        return false;
    }

    // Each syncframe starts its own bit allocation and exponent history --
    // that is what makes a frame the unit a decoder can resynchronise on --
    // but the transform's delay line deliberately survives, because the
    // overlap that reconstructs the signal spans the frame boundary.
    m_state = AC3FrameState();

    const unsigned channels = header.outputChannels();
    const unsigned fbw = header.channels;
    const unsigned acmod = static_cast<unsigned>(header.acmod);

    // Six blocks for AC-3; E-AC-3 frames carry 1, 2, 3 or 6.
    const unsigned blocks = header.blocks;
    pcm.assign(static_cast<size_t>(channels) * blocks * kBlockSamples, 0.0f);

    for (unsigned b = 0; b < blocks; ++b) {
        AC3Block block;
        const char* reason = nullptr;
        if (!ac3ParseAudioBlock(reader, header, m_state, block, &reason,
                                is_eac3 ? &eac3 : nullptr)) {
            Debug::log("ac3", "frame failed at block ", b, ": ",
                       reason ? reason : "unknown");
            return false;
        }

        float samples[kBlockSamples];
        for (unsigned ch = 0; ch < fbw; ++ch) {
            ac3InverseTransform(block.coefficients[ch], block.block_switch[ch],
                                m_transforms[ch], samples);
            const int slot = kWaveOrder[acmod][ch];
            if (slot < 0) {
                continue;
            }
            // The LFE takes WAVE slot 3, so every channel at or past it in
            // WAVE order shifts up by one to make room.
            unsigned out = static_cast<unsigned>(slot);
            if (header.lfeon && out >= 3) {
                ++out;
            }
            float* dst = pcm.data() + static_cast<size_t>(b) * kBlockSamples * channels;
            for (unsigned n = 0; n < kBlockSamples; ++n) {
                dst[n * channels + out] = samples[n];
            }
        }

        if (header.lfeon) {
            // The LFE is never block-switched: A/52 §5.4.2.1 gives blksw only
            // to the full-bandwidth channels.
            ac3InverseTransform(block.coefficients[kLfeSlot], false,
                                m_transforms[kLfeSlot], samples);
            const unsigned out = std::min(3u, channels - 1);
            float* dst = pcm.data() + static_cast<size_t>(b) * kBlockSamples * channels;
            for (unsigned n = 0; n < kBlockSamples; ++n) {
                dst[n * channels + out] = samples[n];
            }
        }
    }

    m_channels = channels;
    m_sample_rate = header.sample_rate;
    return true;
}

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3
