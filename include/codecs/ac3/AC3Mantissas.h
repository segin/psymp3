/*
 * AC3Mantissas.h - AC-3 mantissa dequantization (A/52 §7.3).
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

#ifndef AC3MANTISSAS_H
#define AC3MANTISSAS_H

// All necessary headers are included via psymp3.h

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

/// Bits a mantissa of this bap occupies in the stream, A/52 Table 7.18.
///
/// For the grouped baps -- 1, 2 and 4 -- this is the size of the whole group,
/// which is read once at the position of its first member. The members after
/// it read nothing.
unsigned ac3MantissaGroupBits(uint8_t bap);

/// Mantissas packed into one group: 3 for bap 1 and 2, 2 for bap 4, 1 for
/// everything else that codes anything, 0 for bap 0.
unsigned ac3MantissaGroupSize(uint8_t bap);

/// Reads mantissas for one exponent set, dequantizing as it goes.
///
/// The grouping is why this is a class rather than a function. Three mantissas
/// of bap 1 share a single 5-bit codeword, and the codeword appears at the
/// first of them: the second and third read nothing from the stream at all and
/// take their values from the group already held. A reader that fetched bits
/// for each of them would consume the next mantissa's bits instead, and every
/// coefficient after it would come from the wrong place.
///
/// State is per exponent set, so a fresh reader is used for each channel of
/// each block.
class AC3MantissaReader {
public:
    /// One dequantized coefficient, already scaled by its exponent.
    ///
    /// Returns the transform coefficient of A/52 §7.3.2:
    /// quantization_table[code] >> exponent, expressed as a float in
    /// [-1, 1) rather than a fixed-point word, since the inverse transform
    /// works in floating point regardless.
    ///
    /// @param bap       allocation pointer for this bin
    /// @param exponent  its exponent, 0..24
    float next(AC3BitReader& reader, uint8_t bap, uint8_t exponent);

    /// Forgets any part-used group. Called between exponent sets.
    void reset();

private:
    /// A group holds up to three codes; the count left says how many of them
    /// are still to be handed out.
    struct Group {
        int codes[3] = {0, 0, 0};
        unsigned remaining = 0;
        unsigned next_index = 0;
    };
    Group m_groups[6]; ///< indexed by bap; only 1, 2 and 4 are ever used
};

/// The dequantized value of a symmetric code, A/52 Tables 7.19 to 7.23.
///
/// Each N-level quantizer is uniform and centred: code c stands for
/// (2c - (N-1)) / N. The standard prints the five tables in full; computing
/// them from that form gives the identical values without transcribing
/// forty-one fractions by hand, and the tests check the computation against
/// the printed ones.
float ac3SymmetricMantissa(uint8_t bap, unsigned code);

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3

#endif // AC3MANTISSAS_H
