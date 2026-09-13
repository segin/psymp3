/*
 * AC3Tables.h - Bit allocation tables, transcribed from ATSC A/52:2012.
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * The tables below are data from the standard, extracted from the published
 * document rather than copied from any implementation. Each cites its table
 * number. They are the normative input to the bit allocation routine of §7.2,
 * which the decoder must reproduce exactly: the encoder assumed these values
 * when deciding how many bits each mantissa got, so a single wrong entry does
 * not colour the sound, it makes the decoder read the wrong number of bits and
 * lose the rest of the block.
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

#ifndef AC3TABLES_H
#define AC3TABLES_H

// All necessary headers are included via psymp3.h

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

/// 1/6-octave bands the bit allocation works in, A/52 Table 7.12.
constexpr unsigned kBandCount = 50;
/// Transform coefficients per block. Only 0..252 are ever addressed, since
/// A/52 §5.4.3.24 caps a channel's last bin at 253.
constexpr unsigned kBinCount = 256;

/// A/52 Table 7.6, slowdec[].
constexpr uint16_t kSlowDecay[4] = { 15, 17, 19, 21 };
/// A/52 Table 7.7, fastdec[].
constexpr uint16_t kFastDecay[4] = { 63, 83, 103, 123 };
/// A/52 Table 7.8, slowgain[].
constexpr uint16_t kSlowGain[4] = { 1344, 1240, 1144, 1040 };
/// A/52 Table 7.9, dbpbtab[].
constexpr uint16_t kDbPerBit[4] = { 0, 1792, 2304, 2816 };

/// A/52 Table 7.10, floortab[]. Signed: the last entry is 0xf800, which is
/// -2048, and is what lets the floor be effectively removed.
constexpr int16_t kFloor[8] = { 752, 688, 624, 560, 496, 368, 240, -2048 };

/// A/52 Table 7.11, fastgain[].
constexpr uint16_t kFastGain[8] = { 128, 256, 384, 512, 640, 768, 896, 1024 };

/// A/52 Table 7.12: the first bin of each band, and its width.
constexpr uint8_t kBandStart[kBandCount] = {
      0,   1,   2,   3,   4,   5,   6,   7,   8,   9,
     10,  11,  12,  13,  14,  15,  16,  17,  18,  19,
     20,  21,  22,  23,  24,  25,  26,  27,  28,  31,
     34,  37,  40,  43,  46,  49,  55,  61,  67,  73,
     79,  85,  97, 109, 121, 133, 157, 181, 205, 229,
};
constexpr uint8_t kBandSize[kBandCount] = {
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
      1,   1,   1,   1,   1,   1,   1,   1,   3,   3,
      3,   3,   3,   3,   3,   6,   6,   6,   6,   6,
      6,  12,  12,  12,  12,  24,  24,  24,  24,  24,
};

/// A/52 Table 7.13, masktab[]: which band a bin belongs to.
///
/// The last three entries are zero, exactly as the standard prints them. Bins
/// 253 to 255 cannot be addressed -- a channel's last bin stops at 253 -- so
/// they are padding rather than a band 0 mapping, and the array is never read
/// there.
constexpr uint8_t kBinToBand[kBinCount] = {
      0,   1,   2,   3,   4,   5,   6,   7,   8,   9,
     10,  11,  12,  13,  14,  15,  16,  17,  18,  19,
     20,  21,  22,  23,  24,  25,  26,  27,  28,  28,
     28,  29,  29,  29,  30,  30,  30,  31,  31,  31,
     32,  32,  32,  33,  33,  33,  34,  34,  34,  35,
     35,  35,  35,  35,  35,  36,  36,  36,  36,  36,
     36,  37,  37,  37,  37,  37,  37,  38,  38,  38,
     38,  38,  38,  39,  39,  39,  39,  39,  39,  40,
     40,  40,  40,  40,  40,  41,  41,  41,  41,  41,
     41,  41,  41,  41,  41,  41,  41,  42,  42,  42,
     42,  42,  42,  42,  42,  42,  42,  42,  42,  43,
     43,  43,  43,  43,  43,  43,  43,  43,  43,  43,
     43,  44,  44,  44,  44,  44,  44,  44,  44,  44,
     44,  44,  44,  45,  45,  45,  45,  45,  45,  45,
     45,  45,  45,  45,  45,  45,  45,  45,  45,  45,
     45,  45,  45,  45,  45,  45,  45,  46,  46,  46,
     46,  46,  46,  46,  46,  46,  46,  46,  46,  46,
     46,  46,  46,  46,  46,  46,  46,  46,  46,  46,
     46,  47,  47,  47,  47,  47,  47,  47,  47,  47,
     47,  47,  47,  47,  47,  47,  47,  47,  47,  47,
     47,  47,  47,  47,  47,  48,  48,  48,  48,  48,
     48,  48,  48,  48,  48,  48,  48,  48,  48,  48,
     48,  48,  48,  48,  48,  48,  48,  48,  48,  49,
     49,  49,  49,  49,  49,  49,  49,  49,  49,  49,
     49,  49,  49,  49,  49,  49,  49,  49,  49,  49,
     49,  49,  49,   0,   0,   0,
};

/// A/52 Table 7.14, latab[]: log-addition, indexed by half the difference
/// between two log-domain operands.
constexpr uint16_t kLogAdd[256] = {
    0x0040, 0x003f, 0x003e, 0x003d, 0x003c, 0x003b, 0x003a, 0x0039,
    0x0038, 0x0037, 0x0036, 0x0035, 0x0034, 0x0034, 0x0033, 0x0032,
    0x0031, 0x0030, 0x002f, 0x002f, 0x002e, 0x002d, 0x002c, 0x002c,
    0x002b, 0x002a, 0x0029, 0x0029, 0x0028, 0x0027, 0x0026, 0x0026,
    0x0025, 0x0024, 0x0024, 0x0023, 0x0023, 0x0022, 0x0021, 0x0021,
    0x0020, 0x0020, 0x001f, 0x001e, 0x001e, 0x001d, 0x001d, 0x001c,
    0x001c, 0x001b, 0x001b, 0x001a, 0x001a, 0x0019, 0x0019, 0x0018,
    0x0018, 0x0017, 0x0017, 0x0016, 0x0016, 0x0015, 0x0015, 0x0015,
    0x0014, 0x0014, 0x0013, 0x0013, 0x0013, 0x0012, 0x0012, 0x0012,
    0x0011, 0x0011, 0x0011, 0x0010, 0x0010, 0x0010, 0x000f, 0x000f,
    0x000f, 0x000e, 0x000e, 0x000e, 0x000d, 0x000d, 0x000d, 0x000d,
    0x000c, 0x000c, 0x000c, 0x000c, 0x000b, 0x000b, 0x000b, 0x000b,
    0x000a, 0x000a, 0x000a, 0x000a, 0x000a, 0x0009, 0x0009, 0x0009,
    0x0009, 0x0009, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008,
    0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0006, 0x0006,
    0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0005, 0x0005,
    0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0004, 0x0004,
    0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
    0x0004, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003,
    0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0002,
    0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
    0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
    0x0002, 0x0002, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
    0x0001, 0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

/// A/52 Table 7.15, hth[fscod][band]: the hearing threshold, which depends on
/// sample rate because a band covers a different span of Hz at each.
constexpr uint16_t kHearingThreshold[3][kBandCount] = {
    { 0x04d0, 0x04d0, 0x0440, 0x0400, 0x03e0, 0x03c0, 0x03b0, 0x03b0, 0x03a0, 0x03a0, 0x03a0, 0x03a0, 0x03a0, 0x0390, 0x0390, 0x0390, 0x0380, 0x0380, 0x0370, 0x0370, 0x0360, 0x0360, 0x0350, 0x0350, 0x0340, 0x0340, 0x0330, 0x0320, 0x0310, 0x0300, 0x02f0, 0x02f0, 0x02f0, 0x02f0, 0x0300, 0x0310, 0x0340, 0x0390, 0x03e0, 0x0420, 0x0460, 0x0490, 0x04a0, 0x0460, 0x0440, 0x0440, 0x0520, 0x0800, 0x002c, 0x0840 },
    { 0x04f0, 0x04f0, 0x0460, 0x0410, 0x03e0, 0x03d0, 0x03c0, 0x03b0, 0x03b0, 0x03a0, 0x03a0, 0x03a0, 0x03a0, 0x03a0, 0x0390, 0x0390, 0x0390, 0x0380, 0x0380, 0x0380, 0x0370, 0x0370, 0x0360, 0x0360, 0x0350, 0x0350, 0x0340, 0x0340, 0x0320, 0x0310, 0x0300, 0x02f0, 0x02f0, 0x02f0, 0x02f0, 0x0300, 0x0320, 0x0350, 0x0390, 0x03e0, 0x0420, 0x0450, 0x04a0, 0x0490, 0x0460, 0x0440, 0x0480, 0x0630, 0x0001, 0x0840 },
    { 0x0580, 0x0580, 0x04b0, 0x0450, 0x0420, 0x03f0, 0x03e0, 0x03d0, 0x03c0, 0x03b0, 0x03b0, 0x03b0, 0x03a0, 0x03a0, 0x03a0, 0x03a0, 0x03a0, 0x03a0, 0x03a0, 0x03a0, 0x0390, 0x0390, 0x0390, 0x0390, 0x0380, 0x0380, 0x0380, 0x0370, 0x0360, 0x0350, 0x0340, 0x0330, 0x0320, 0x0310, 0x0300, 0x02f0, 0x02f0, 0x02f0, 0x0300, 0x0310, 0x0330, 0x0350, 0x03c0, 0x0410, 0x0470, 0x04a0, 0x0460, 0x0440, 0x0020, 0x04e0 },
};

/// A/52 Table 7.16, baptab[]: how many bits a mantissa gets, from the
/// difference between its power and the masking curve.
constexpr uint8_t kBapTable[64] = {
     0,  1,  1,  1,  1,  1,  2,  2,  3,  3,  3,  4,  4,  5,  5,  6,
     6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  8,  9,  9,  9,  9, 10,
    10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 14,
    14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15,
};

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3

#endif // AC3TABLES_H
