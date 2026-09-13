/*
 * AC3Exponents.h - AC-3 exponent decoding (A/52 §7.1).
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

#ifndef AC3EXPONENTS_H
#define AC3EXPONENTS_H

// All necessary headers are included via psymp3.h

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

/// A/52 Table 7.4. E-AC-3 reuses these unchanged; Annex E adds AHT on top
/// rather than replacing them.
enum class ExponentStrategy {
    Reuse = 0, ///< keep the previous block's exponents; nothing is coded
    D15   = 1, ///< one exponent per mantissa
    D25   = 2, ///< one per pair
    D45   = 3, ///< one per quad
};

/// The largest exponent a stream can state. A/52 §7.1.3 calls these "5-bit
/// absolute exponents", but the coded range stops at 24: an exponent is a
/// right-shift applied to a mantissa, and 24 already shifts a 24-bit value
/// away entirely.
constexpr uint8_t kMaxExponent = 24;

/// Mantissas that share one differential, A/52 Table 7.4: 1, 2 or 4.
unsigned ac3ExponentGroupSize(ExponentStrategy strategy);

/// Number of coded groups for an independent or coupled channel, A/52 §7.1.3.
/// Zero for Reuse, which codes nothing.
unsigned ac3ChannelExponentGroups(ExponentStrategy strategy, unsigned end_mantissa);

/// Number of coded groups for the coupling channel, A/52 §7.1.3.
unsigned ac3CouplingExponentGroups(ExponentStrategy strategy,
                                   unsigned start_mantissa, unsigned end_mantissa);

/// End mantissa bin for an independent channel from its bandwidth code,
/// A/52 §7.1.3: endmant = ((chbwcod + 12) * 3) + 37.
unsigned ac3ChannelEndMantissa(uint8_t chbwcod);

/// Coupling channel bin range, A/52 §7.1.3.
unsigned ac3CouplingStartMantissa(uint8_t cplbegf);
unsigned ac3CouplingEndMantissa(uint8_t cplendf);

/// The LFE channel is fixed: bins 0 to 7, always two groups, always D15.
constexpr unsigned kLfeEndMantissa = 7;
constexpr unsigned kLfeExponentGroups = 2;

/// Decodes one channel's exponents into @p exponents.
///
/// Exponents are differentially coded and packed three to a 7-bit word, so a
/// group states three deltas of -2..+2 each and every delta is then repeated
/// across its group size. Bin 0 carries the absolute exponent, which is sent
/// separately, and every later bin is reached by accumulation -- which is why
/// a single corrupt group skews everything above it rather than one bin.
///
/// @param absolute_exponent  exps[ch][0], the exponent of bin 0. For the
///                           coupling channel this is cplabsexp doubled: it is
///                           sent as 4 bits standing for a 5-bit value whose
///                           low bit is always zero, and it is a reference
///                           point rather than a real exponent.
/// @param group_count        groups to read, from ac3ChannelExponentGroups()
///                           or ac3CouplingExponentGroups()
/// @param capacity           size of @p exponents
/// @return exponents written, or 0 if the buffer is too small or the stream
///         ran out.
unsigned ac3DecodeExponents(AC3BitReader& reader, ExponentStrategy strategy,
                            unsigned group_count, uint8_t absolute_exponent,
                            uint8_t* exponents, unsigned capacity);

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3

#endif // AC3EXPONENTS_H
