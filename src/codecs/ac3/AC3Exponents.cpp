/*
 * AC3Exponents.cpp - AC-3 exponent decoding (A/52 §7.1).
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * Written from ATSC A/52:2012, "Digital Audio Compression Standard". Section
 * and table numbers in the comments refer to that document.
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

unsigned ac3ExponentGroupSize(ExponentStrategy strategy)
{
    switch (strategy) {
    case ExponentStrategy::D15: return 1;
    case ExponentStrategy::D25: return 2;
    case ExponentStrategy::D45: return 4;
    case ExponentStrategy::Reuse: break;
    }
    return 0;
}

unsigned ac3ChannelEndMantissa(uint8_t chbwcod)
{
    // A/52 §7.1.3. chbwcod is 6 bits, so this tops out at 37 + 73*3 = 253,
    // just inside the 256 bins a block carries.
    return ((static_cast<unsigned>(chbwcod) + 12) * 3) + 37;
}

unsigned ac3CouplingStartMantissa(uint8_t cplbegf)
{
    return (static_cast<unsigned>(cplbegf) * 12) + 37;
}

unsigned ac3CouplingEndMantissa(uint8_t cplendf)
{
    return ((static_cast<unsigned>(cplendf) + 3) * 12) + 37;
}

unsigned ac3ChannelExponentGroups(ExponentStrategy strategy, unsigned end_mantissa)
{
    // A/52 §7.1.3. The added 3 and 9 are what round the division up for the
    // wider group sizes: a channel whose bins do not divide evenly still needs
    // a final, partly used group.
    if (end_mantissa == 0) {
        return 0;
    }
    switch (strategy) {
    case ExponentStrategy::D15: return (end_mantissa - 1) / 3;
    case ExponentStrategy::D25: return (end_mantissa - 1 + 3) / 6;
    case ExponentStrategy::D45: return (end_mantissa - 1 + 9) / 12;
    case ExponentStrategy::Reuse: break;
    }
    return 0;
}

unsigned ac3CouplingExponentGroups(ExponentStrategy strategy,
                                   unsigned start_mantissa, unsigned end_mantissa)
{
    // A/52 §7.1.3. The coupling channel divides exactly -- its bin range is
    // always a multiple of 12 -- so unlike a channel's own exponents there is
    // no rounding term here.
    if (end_mantissa <= start_mantissa) {
        return 0;
    }
    const unsigned span = end_mantissa - start_mantissa;
    switch (strategy) {
    case ExponentStrategy::D15: return span / 3;
    case ExponentStrategy::D25: return span / 6;
    case ExponentStrategy::D45: return span / 12;
    case ExponentStrategy::Reuse: break;
    }
    return 0;
}

unsigned ac3DecodeExponents(AC3BitReader& reader, ExponentStrategy strategy,
                            unsigned group_count, uint8_t absolute_exponent,
                            uint8_t* exponents, unsigned capacity)
{
    const unsigned group_size = ac3ExponentGroupSize(strategy);
    if (group_size == 0 || !exponents || capacity == 0) {
        return 0;
    }

    // Bin 0 holds the absolute exponent and every later bin is reached from
    // it, so the written count is one plus three deltas per group, each
    // repeated across the group size.
    const unsigned written = 1 + group_count * 3 * group_size;
    if (written > capacity) {
        return 0;
    }

    exponents[0] = absolute_exponent;
    int previous = absolute_exponent;
    unsigned out = 1;

    for (unsigned group = 0; group < group_count; ++group) {
        // Three mapped values packed into one 7-bit word, A/52 §7.1.3:
        // gexp = 25*M1 + 5*M2 + M3, each M in 0..4.
        uint32_t packed = reader.read(7);
        int deltas[3];
        deltas[0] = static_cast<int>(packed / 25);
        packed -= static_cast<uint32_t>(25 * deltas[0]);
        deltas[1] = static_cast<int>(packed / 5);
        deltas[2] = static_cast<int>(packed - static_cast<uint32_t>(5 * deltas[1]));

        for (int delta : deltas) {
            // The mapping is biased by 2 so a delta can be negative: 0..4
            // stands for -2..+2.
            previous += delta - 2;
            // A stream should never state an exponent outside 0..24, but the
            // value is an accumulation of deltas read from the file, so a
            // corrupt group would otherwise index the quantisation tables out
            // of range. Clamping keeps the damage to wrong audio rather than a
            // wrong memory access.
            if (previous < 0) {
                previous = 0;
            } else if (previous > kMaxExponent) {
                previous = kMaxExponent;
            }
            for (unsigned i = 0; i < group_size; ++i) {
                exponents[out++] = static_cast<uint8_t>(previous);
            }
        }
    }

    return reader.overrun() ? 0 : out;
}

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3
