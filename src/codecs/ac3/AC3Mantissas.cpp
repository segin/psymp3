/*
 * AC3Mantissas.cpp - AC-3 mantissa dequantization (A/52 §7.3).
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * Written from ATSC A/52:2012, "Digital Audio Compression Standard".
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

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

namespace {

/// A/52 Table 7.18: levels per bap for the symmetric quantizers.
constexpr unsigned kSymmetricLevels[6] = {0, 3, 5, 7, 11, 15};

/// Bits in the stream per bap, A/52 Table 7.18. For 1, 2 and 4 this is the
/// group's width, not one mantissa's share.
constexpr uint8_t kMantissaBits[16] = {
    0,  5,  7,  3,  7,  4,  5,  6,
    7,  8,  9, 10, 11, 12, 14, 16,
};

} // namespace

unsigned ac3MantissaGroupBits(uint8_t bap)
{
    return bap < 16 ? kMantissaBits[bap] : 0;
}

unsigned ac3MantissaGroupSize(uint8_t bap)
{
    switch (bap) {
    case 0:  return 0;   // nothing is coded
    case 1:  return 3;   // three 3-level codes in 5 bits: 3^3 = 27 fits in 32
    case 2:  return 3;   // three 5-level codes in 7 bits: 5^3 = 125 fits in 128
    case 4:  return 2;   // two 11-level codes in 7 bits: 11^2 = 121 fits in 128
    default: return 1;
    }
}

float ac3SymmetricMantissa(uint8_t bap, unsigned code)
{
    if (bap == 0 || bap > 5) {
        return 0.0f;
    }
    const unsigned levels = kSymmetricLevels[bap];
    if (code >= levels) {
        return 0.0f;
    }
    // A/52 Tables 7.19 to 7.23: a uniform quantizer centred on zero, so the
    // middle code is silence and the outermost pair reach (N-1)/N rather than
    // 1. The tables print exactly these values.
    return (2.0f * static_cast<float>(code) - static_cast<float>(levels - 1))
         / static_cast<float>(levels);
}

void AC3MantissaReader::reset()
{
    for (Group& group : m_groups) {
        group.remaining = 0;
        group.next_index = 0;
    }
}

float AC3MantissaReader::next(AC3BitReader& reader, uint8_t bap, uint8_t exponent)
{
    if (bap == 0) {
        // A/52 §7.3.4 fills these with dither. Until the dither generator is
        // wired to the block's dithflag, silence is the honest placeholder --
        // it is quiet rather than wrong in a way that hides other faults.
        return 0.0f;
    }
    if (bap > 15) {
        return 0.0f;
    }

    const unsigned bits = ac3MantissaGroupBits(bap);
    float value = 0.0f;

    if (bap == 1 || bap == 2 || bap == 4) {
        // Grouped. The codeword sits at the first member of the group; the
        // rest take their codes from what was already read.
        Group& group = m_groups[bap];
        if (group.remaining == 0) {
            const unsigned levels = kSymmetricLevels[bap];
            const unsigned size = ac3MantissaGroupSize(bap);
            uint32_t packed = reader.read(bits);
            // Digits most significant first, matching how the exponent groups
            // of §7.1.3 are packed: the first mantissa is the high digit.
            for (unsigned i = 0; i < size; ++i) {
                unsigned divisor = 1;
                for (unsigned j = i + 1; j < size; ++j) {
                    divisor *= levels;
                }
                group.codes[i] = static_cast<int>((packed / divisor) % levels);
            }
            group.remaining = size;
            group.next_index = 0;
        }
        value = ac3SymmetricMantissa(bap, static_cast<unsigned>(group.codes[group.next_index]));
        ++group.next_index;
        --group.remaining;
    } else if (bap <= 5) {
        // Symmetric but ungrouped: one code, one lookup.
        value = ac3SymmetricMantissa(bap, reader.read(bits));
    } else {
        // A/52 §7.3.2: an asymmetric two's complement fraction with the point
        // to the left of the most significant bit, so an n-bit word covers
        // -1.0 to 1.0 - 2^-(n-1).
        const uint32_t raw = reader.read(bits);
        const uint32_t sign = 1u << (bits - 1);
        const int32_t signed_value = (raw & sign)
                                   ? static_cast<int32_t>(raw) - static_cast<int32_t>(1u << bits)
                                   : static_cast<int32_t>(raw);
        value = static_cast<float>(signed_value) / static_cast<float>(sign);
    }

    // The exponent is a right shift, A/52 §7.3.2. In floating point that is a
    // division by a power of two, which is exact.
    return value / static_cast<float>(1u << std::min<uint8_t>(exponent, 24));
}

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3
