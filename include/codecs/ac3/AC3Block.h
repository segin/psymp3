/*
 * AC3Block.h - AC-3 audio block parsing (A/52 §5.4.3, §7.4, §7.5).
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef AC3BLOCK_H
#define AC3BLOCK_H

// All necessary headers are included via psymp3.h

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

/// Most full-bandwidth channels an AC-3 stream can carry, A/52 Table 5.8.
constexpr unsigned kMaxFullBandwidthChannels = 5;
/// Slots in the per-channel arrays: the five channels, then the coupling
/// channel, then LFE.
constexpr unsigned kCouplingSlot = kMaxFullBandwidthChannels;
constexpr unsigned kLfeSlot = kMaxFullBandwidthChannels + 1;
constexpr unsigned kChannelSlots = kMaxFullBandwidthChannels + 2;
/// Coupling sub-bands, A/52 §5.4.3.13: cplbegf and cplendf are four bits each
/// and ncplsubnd = 3 + cplendf - cplbegf, so eighteen is the ceiling.
constexpr unsigned kMaxCouplingBands = 18;

/// E-AC-3's frame layer, defined in EAC3Frame.h. The block parser reads
/// through it when it is given one.
struct EAC3AudioFrame;

/// A/52 Table 5.16, the delta bit allocation strategies. "Apply none" is
/// distinct from "reuse": it discards whatever the previous block sent.
constexpr uint8_t kDeltaReuse = 0;
constexpr uint8_t kDeltaNew = 1;
constexpr uint8_t kDeltaNone = 2;

/// The noise source for A/52 §7.3.4.
///
/// The spec asks only for "any reasonably random sequence" of about eight
/// bits, uniform over [-1, 1) and scaled by 0.707; the exact sequence is not
/// normative and no two decoders need agree on it. A 16-bit LFSR is cheap,
/// has no startup cost and does not repeat within a frame.
class AC3Dither {
public:
    /// A uniform value in roughly [-0.707, 0.707).
    float next()
    {
        // x^16 + x^14 + x^13 + x^11 + 1, the maximal-length Galois form.
        const bool bit = (m_state & 1u) != 0;
        m_state >>= 1;
        if (bit) {
            m_state ^= 0xb400u;
        }
        const int sample = static_cast<int>(m_state & 0xffu) - 128;
        return static_cast<float>(sample) * (0.707f / 128.0f);
    }

private:
    uint16_t m_state = 0xacedu; // any non-zero seed will do
};

/// What survives from one audio block to the next.
///
/// A block may say "reuse" for its exponents, leave the coupling strategy
/// alone, or omit the bit allocation parameters entirely, in which case the
/// previous block's stand. Six blocks share a frame, so this is the frame's
/// working memory and it is why a block cannot be decoded on its own.
struct AC3FrameState {
    // Coupling, A/52 §7.4.
    bool cplinu = false;
    bool chincpl[kMaxFullBandwidthChannels] = {};
    uint8_t cplbegf = 0;
    int cplendf = 0;             ///< signed: spectral extension derives it, down to -2
    unsigned ncplsubnd = 0;
    unsigned ncplbnd = 0;
    uint8_t cplbndstrc[kMaxCouplingBands] = {};
    float cplco[kMaxFullBandwidthChannels][kMaxCouplingBands] = {};
    bool phsflginu = false;
    bool phsflg[kMaxCouplingBands] = {};

    // Rematrixing, A/52 §7.5.
    bool rematflg[4] = {};

    // Exponents and the bin ranges they cover.
    uint8_t exponents[kChannelSlots][kBinCount] = {};
    ExponentStrategy expstr[kChannelSlots] = {};
    unsigned endmant[kChannelSlots] = {};
    unsigned strtmant[kChannelSlots] = {};

    // Bit allocation, A/52 §7.2.
    uint8_t sdcycod = 0, fdcycod = 0, sgaincod = 0, dbpbcod = 0, floorcod = 0;
    int csnroffst = 0;
    int fsnroffst[kChannelSlots] = {};
    uint8_t fgaincod[kChannelSlots] = {};
    std::vector<DeltaBitAllocation> deltas[kChannelSlots];
    /// Where the coupling channel's leaky integrators start, §5.4.3.45-46.
    uint8_t cplfleak = 0;
    uint8_t cplsleak = 0;


    /// True once a block has set the coupling strategy and bit allocation
    /// parameters. A frame whose first block omits them is malformed.
    bool have_allocation = false;

    /// Which of the frame's six blocks comes next. Block 0 is special in
    /// several places: it may not say "reuse", and an absent delta bit
    /// allocation there means "apply none" rather than "keep the last".
    unsigned block_index = 0;

    // --- E-AC-3 syntax state, reset with the rest at every syncframe ---
    // Table E1.4 stops transmitting some flags once a frame has established
    // them: the first block to couple a channel must send its coordinates,
    // and the first coupled block must send leak values, so those are implied
    // rather than read. These record whether that first time has passed.
    bool firstcplcos[kMaxFullBandwidthChannels] = { true, true, true, true, true };
    bool firstcplleak = true;
    /// Whether a coupling band structure is in force for this frame, so an
    /// omitted one means "reuse" rather than "take the default".
    bool cplbndstrc_set = false;
    bool spxinu = false;
    bool chinspx[kMaxFullBandwidthChannels] = {};

    // --- E-AC-3 spectral extension, §E3.6 ---
    uint8_t spxbegf = 0;
    uint8_t spxbndstrc[kSpxSubbands] = {};   ///< reset to Table E2.11 at block 0
    bool firstspxcos[kMaxFullBandwidthChannels] = { true, true, true, true, true };
    EAC3SpxBands spx_bands;
    EAC3SpxChannel spx[kMaxFullBandwidthChannels];

    /// Noise for unallocated bins and for spectral extension. Unlike
    /// everything above, these must not restart with each syncframe -- a
    /// sequence restarted 31 times a second is audible as a buzz -- so the
    /// frame decoder carries them across its per-frame reset.
    AC3Dither dither;
    EAC3SpxNoise spx_noise;

    // --- E-AC-3 Adaptive Hybrid Transform, §E3.4 ---
    /// Per slot: 0 when the slot does not use AHT, 1 while its mantissas are
    /// still to be read this frame, -1 once they have been. Table E1.4 keeps
    /// the same tri-state in chahtinu[] itself.
    int8_t aht[kChannelSlots] = {};
    /// All six blocks of an AHT slot's coefficients, read at once.
    EAC3AhtSpectrum aht_spectrum[kChannelSlots];

    // --- E-AC-3 enhanced coupling, §E3.5 ---
    bool ecplinu = false;
    bool ecplangleintrp = false;
    uint8_t ecplbegf = 0;
    uint8_t ecplbndstrc[kEcplSubbands] = {};
    bool ecplbndstrc_set = false;
    EAC3EcplBands ecpl_bands;
    EAC3EcplChannel ecpl[kMaxFullBandwidthChannels];
};

/// One decoded audio block: 256 transform coefficients per coded channel,
/// after decoupling and rematrixing.
struct AC3Block {
    /// Indexed by channel, then bin. Only the first nfchans rows are filled,
    /// plus the LFE row when the stream carries one.
    float coefficients[kChannelSlots][kSamplesPerBlock] = {};
    /// Whether each channel used the short transform this block, A/52 §7.9.
    bool block_switch[kMaxFullBandwidthChannels] = {};
    /// Dynamic range control word, or 1.0 when the block sends none.
    float dynamic_range = 1.0f;

    /// Enhanced coupling leaves the coupled channels unfinished: rebuilding
    /// them needs the coupling signal of the block after this one too
    /// (§E3.5.5.1), so the parser records what the frame decoder will need.
    bool ecplinu = false;
    bool ecpl_angle_interpolation = false;
    float ecpl_coupling[kSamplesPerBlock] = {};
    EAC3EcplBands ecpl_bands;
    EAC3EcplChannel ecpl[kMaxFullBandwidthChannels];
    bool chincpl[kMaxFullBandwidthChannels] = {};

    /// Spectral extension copies a channel's finished low band, which for an
    /// enhanced-coupled channel is not finished until the frame decoder has
    /// regenerated it; such channels are synthesised there instead.
    bool spx_deferred[kMaxFullBandwidthChannels] = {};
    EAC3SpxBands spx_bands;
    EAC3SpxChannel spx[kMaxFullBandwidthChannels];
};

/// Parses one audio block and produces its transform coefficients.
///
/// Runs the chain the standard lays out: the block's own fields, then
/// exponents (§7.1), bit allocation (§7.2), mantissas (§7.3), decoupling
/// (§7.4) and rematrixing (§7.5). What comes out is ready for the inverse
/// transform and nothing else.
///
/// @param state  carried between the six blocks of a frame; zero it at the
///               start of each frame, not each block
/// @return false when the block is malformed or the stream ran out, in which
///         case @p block holds whatever was decoded before the fault.
/// @param reason  set to a short description when parsing fails, for logging
///                and for telling one fault from another while bringing the
///                decoder up. Points at a literal; never freed.
bool ac3ParseAudioBlock(AC3BitReader& reader, const AC3FrameHeader& header,
                        AC3FrameState& state, AC3Block& block,
                        const char** reason = nullptr,
                        const EAC3AudioFrame* eac3 = nullptr);

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3

#endif // AC3BLOCK_H
