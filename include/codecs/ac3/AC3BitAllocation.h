/*
 * AC3BitAllocation.h - AC-3 parametric bit allocation (A/52 §7.2).
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

#ifndef AC3BITALLOCATION_H
#define AC3BITALLOCATION_H

// All necessary headers are included via psymp3.h

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

/// Which kind of channel is being allocated. The excitation function starts
/// differently for each, A/52 §7.2.2.4.
enum class AllocationChannel {
    FullBandwidth, ///< an ordinary channel: the low-frequency compensation runs
    Coupling,      ///< the coupling channel: no compensation, starts at its own band
    LFE,           ///< as full-bandwidth, but band 6 is skipped and it ends at bin 7
};

/// Everything the routine takes from the bit stream, A/52 §7.2.2.1.
///
/// These are codes rather than values: the decoder looks each up in the tables
/// of §7.2 and must get the same numbers the encoder used, since the encoder
/// chose its mantissa bit counts assuming them.
struct AllocationParameters {
    uint8_t fscod = 0;      ///< selects the hearing threshold column
    uint8_t sdcycod = 0;    ///< slow decay
    uint8_t fdcycod = 0;    ///< fast decay
    uint8_t sgaincod = 0;   ///< slow gain
    uint8_t dbpbcod = 0;    ///< dB per bit, giving the knee
    uint8_t floorcod = 0;   ///< floor
    uint8_t fgaincod = 0;   ///< fast gain, per channel
    /// snroffset as computed in §7.2.2.1:
    /// (((csnroffst - 15) << 4) + fsnroffst) << 2.
    int snroffset = 0;
    /// cplfleak and cplsleak, §5.4.3.45-46. The coupling channel begins part
    /// way up the spectrum, so its leaky integrators cannot start from the
    /// signal the way a full-bandwidth channel's do -- the encoder transmits
    /// where they had got to. Ignored for the other channel kinds.
    uint8_t cplfleak = 0;
    uint8_t cplsleak = 0;
};

/// One delta bit allocation segment, A/52 §7.2.2.6. The encoder uses these to
/// push the masking curve up or down in multiples of 6 dB where the parametric
/// model alone would misjudge a block.
struct DeltaBitAllocation {
    uint8_t offset = 0;
    uint8_t length = 0;
    uint8_t bit_allocation = 0;
};

/// Runs the seven-step parametric bit allocation of A/52 §7.2.2 and fills
/// @p bap with one 4-bit pointer per bin, saying how many bits that mantissa
/// was given.
///
/// This is the routine the whole format turns on. The encoder ran the same
/// computation to decide how many bits to spend on each mantissa and wrote the
/// stream accordingly; the decoder has to arrive at the identical answer to
/// know how many bits to read back. A discrepancy of one in any band does not
/// make the audio slightly wrong -- it makes every subsequent mantissa read
/// come from the wrong bit position, so the block dissolves from that point
/// on. That is why every table lookup and shift below follows the standard's
/// pseudocode literally rather than being rearranged into something tidier.
///
/// @param exponents  decoded exponents, one per bin, from §7.1
/// @param start      first bin, inclusive
/// @param end        last bin, exclusive
/// @param deltas     delta bit allocation segments, or none
/// @param bap        output, at least @p end entries
/// @return false if the arguments are inconsistent, in which case bap is
///         untouched.
bool ac3ComputeBitAllocation(const uint8_t* exponents, unsigned start, unsigned end,
                             AllocationChannel channel,
                             const AllocationParameters& parameters,
                             const std::vector<DeltaBitAllocation>& deltas,
                             uint8_t* bap);

/// A/52 §7.2.2.4. Exposed for testing: it is a small state machine whose
/// behaviour changes at bands 7 and 20, and those boundaries are easy to get
/// wrong and invisible when they are.
int ac3CalcLowComp(int a, int b0, int b1, unsigned bin);

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3

#endif // AC3BITALLOCATION_H
