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
    uint8_t cplendf = 0;
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

    /// True once a block has set the coupling strategy and bit allocation
    /// parameters. A frame whose first block omits them is malformed.
    bool have_allocation = false;
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
                        const char** reason = nullptr);

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3

#endif // AC3BLOCK_H
