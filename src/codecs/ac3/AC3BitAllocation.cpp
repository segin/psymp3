/*
 * AC3BitAllocation.cpp - AC-3 parametric bit allocation (A/52 §7.2).
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * Written from ATSC A/52:2012, "Digital Audio Compression Standard". The seven
 * steps below follow §7.2.2.1 through §7.2.2.7 in order, and each keeps the
 * standard's own pseudocode shape: the encoder ran this same computation to
 * decide how many bits every mantissa got, so the decoder has to land on the
 * identical answer or it reads the next mantissa from the wrong bit.
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

/// Log-domain addition, A/52 §7.2.2.3. Two values are combined by taking their
/// difference, halving it, and reading a correction out of latab.
int logAdd(int a, int b)
{
    const int difference = std::abs(a - b);
    const int larger = std::max(a, b);
    // Past 255 the correction has decayed to nothing, so the larger operand
    // stands alone; the table only covers the range where it matters.
    if (difference >= 512) {
        return larger;
    }
    return larger + kLogAdd[difference >> 1];
}

} // namespace

int ac3CalcLowComp(int a, int b0, int b1, unsigned bin)
{
    // A/52 §7.2.2.4. The low-frequency compensation keeps the allocation from
    // over-spending on the bottom bands, and it changes character twice: once
    // at band 7 and again at band 20.
    //
    // The standard prints the first branch as `if ((b0 + 256) == b1) ;` -- with
    // a stray semicolon that would make the assignment unconditional. The
    // second branch, which is otherwise identical, has no semicolon, so it is a
    // typo in the document rather than intent. Transcribing it literally would
    // set a = 384 for every band below 7 regardless of the comparison.
    if (bin < 7) {
        if ((b0 + 256) == b1) {
            a = 384;
        } else if (b0 > b1) {
            a = std::max(0, a - 64);
        }
    } else if (bin < 20) {
        if ((b0 + 256) == b1) {
            a = 320;
        } else if (b0 > b1) {
            a = std::max(0, a - 64);
        }
    } else {
        a = std::max(0, a - 128);
    }
    return a;
}

bool ac3ComputeBitAllocation(const uint8_t* exponents, unsigned start, unsigned end,
                             AllocationChannel channel,
                             const AllocationParameters& parameters,
                             const std::vector<DeltaBitAllocation>& deltas,
                             uint8_t* bap)
{
    if (!exponents || !bap || end <= start || end > kBinCount
        || parameters.fscod > 2 || parameters.sdcycod > 3 || parameters.fdcycod > 3
        || parameters.sgaincod > 3 || parameters.dbpbcod > 3
        || parameters.floorcod > 7 || parameters.fgaincod > 7) {
        return false;
    }

    // --- §7.2.2.1: parameters, by table lookup ---
    const int sdecay = kSlowDecay[parameters.sdcycod];
    const int fdecay = kFastDecay[parameters.fdcycod];
    const int sgain  = kSlowGain[parameters.sgaincod];
    const int dbknee = kDbPerBit[parameters.dbpbcod];
    const int floor  = kFloor[parameters.floorcod];
    const int fgain  = kFastGain[parameters.fgaincod];
    const int snroffset = parameters.snroffset;

    // --- §7.2.2.2: exponents into a log power spectral density ---
    // An exponent is a right shift, so a larger one means a quieter bin; the
    // subtraction from 3072 turns it back into a power.
    int psd[kBinCount];
    for (unsigned bin = start; bin < end; ++bin) {
        psd[bin] = 3072 - (static_cast<int>(exponents[bin]) << 7);
    }

    // --- §7.2.2.3: integrate the density into 1/6-octave bands ---
    int band_psd[kBandCount] = {0};
    {
        unsigned bin = start;
        unsigned band = kBinToBand[start];
        do {
            const unsigned last = std::min<unsigned>(kBandStart[band] + kBandSize[band], end);
            band_psd[band] = psd[bin];
            ++bin;
            for (; bin < last; ++bin) {
                band_psd[band] = logAdd(band_psd[band], psd[bin]);
            }
            ++band;
        } while (end > kBandStart[band - 1] + kBandSize[band - 1]);
    }

    const unsigned band_start = kBinToBand[start];
    const unsigned band_end = kBinToBand[end - 1] + 1;

    // --- §7.2.2.4: the excitation function ---
    // Two leaky integrators run across the bands: a fast one that tracks the
    // signal closely and a slow one that decays gently, and the excitation is
    // whichever is higher. That is the spreading of masking across frequency.
    int excite[kBandCount] = {0};
    int fastleak = 0;
    int slowleak = 0;
    unsigned begin = band_start;

    if (channel == AllocationChannel::Coupling) {
        // §7.2.2.1: the coupling channel's integrators are seeded from the
        // stream rather than from its own first band.
        fastleak = (static_cast<int>(parameters.cplfleak) << 8) + 768;
        slowleak = (static_cast<int>(parameters.cplsleak) << 8) + 768;
    }

    if (channel != AllocationChannel::Coupling && band_start == 0) {
        int lowcomp = 0;
        lowcomp = ac3CalcLowComp(lowcomp, band_psd[0], band_psd[1], 0);
        excite[0] = band_psd[0] - fgain - lowcomp;
        lowcomp = ac3CalcLowComp(lowcomp, band_psd[1], band_psd[2], 1);
        excite[1] = band_psd[1] - fgain - lowcomp;

        begin = 7;
        for (unsigned band = 2; band < 7; ++band) {
            // The LFE channel stops at band 6 and must not have the
            // compensation applied to its last band.
            const bool skip = (band_end == 7) && (band == 6);
            if (!skip) {
                lowcomp = ac3CalcLowComp(lowcomp, band_psd[band], band_psd[band + 1], band);
            }
            fastleak = band_psd[band] - fgain;
            slowleak = band_psd[band] - sgain;
            excite[band] = fastleak - lowcomp;
            if (!skip && (band_psd[band] <= band_psd[band + 1])) {
                begin = band + 1;
                break;
            }
        }

        for (unsigned band = begin; band < std::min<unsigned>(band_end, 22); ++band) {
            const bool skip = (band_end == 7) && (band == 6);
            if (!skip) {
                lowcomp = ac3CalcLowComp(lowcomp, band_psd[band], band_psd[band + 1], band);
            }
            fastleak -= fdecay;
            fastleak = std::max(fastleak, band_psd[band] - fgain);
            slowleak -= sdecay;
            slowleak = std::max(slowleak, band_psd[band] - sgain);
            excite[band] = std::max(fastleak - lowcomp, slowleak);
        }
        begin = 22;
    }

    for (unsigned band = begin; band < band_end; ++band) {
        fastleak -= fdecay;
        fastleak = std::max(fastleak, band_psd[band] - fgain);
        slowleak -= sdecay;
        slowleak = std::max(slowleak, band_psd[band] - sgain);
        excite[band] = std::max(fastleak, slowleak);
    }

    // --- §7.2.2.5: the masking curve ---
    int mask[kBandCount] = {0};
    for (unsigned band = band_start; band < band_end; ++band) {
        if (band_psd[band] < dbknee) {
            excite[band] += (dbknee - band_psd[band]) >> 2;
        }
        // Nothing below the threshold of hearing needs bits spent on it, and
        // the threshold depends on sample rate because a band spans a
        // different range of Hz at each.
        mask[band] = std::max(excite[band], static_cast<int>(kHearingThreshold[parameters.fscod][band]));
    }

    // --- §7.2.2.6: delta bit allocation ---
    // The encoder's override, where the parametric model alone misjudges a
    // block. Adjustments are in multiples of 6 dB, with 4 and above meaning a
    // reduction rather than an increase.
    // The first segment's offset is an absolute band number; every one after
    // it is measured from where the previous segment stopped (§5.4.3.55), so
    // the cursor runs on rather than being reseated.
    unsigned band = 0;
    for (const DeltaBitAllocation& delta : deltas) {
        band += delta.offset;
        const int adjust = delta.bit_allocation >= 4
                         ? (static_cast<int>(delta.bit_allocation) - 3) << 7
                         : (static_cast<int>(delta.bit_allocation) - 4) << 7;
        for (unsigned i = 0; i < delta.length && band < kBandCount; ++i, ++band) {
            mask[band] += adjust;
        }
    }

    if (Debug::isChannelEnabled("ac3")) {
        for (unsigned b = band_start; b < std::min(band_start + 6, band_end); ++b) {
            Debug::log("ac3", "     band ", b, " bndpsd=", band_psd[b],
                       " excite=", excite[b], " hth=",
                       (int)kHearingThreshold[parameters.fscod][b],
                       " mask=", mask[b], " snroffset=", snroffset,
                       " floor=", floor, " fgain=", fgain, " sgain=", sgain);
        }
    }

    // --- §7.2.2.7: the allocation itself ---
    {
        unsigned bin = start;
        unsigned band = kBinToBand[start];
        do {
            const unsigned last = std::min<unsigned>(kBandStart[band] + kBandSize[band], end);
            int band_mask = mask[band];
            band_mask -= snroffset;
            band_mask -= floor;
            if (band_mask < 0) {
                band_mask = 0;
            }
            // The masking value is quantised to the 32-step grid baptab is
            // indexed on; the low five bits would otherwise survive the shift
            // below and shift the address by one.
            band_mask &= 0x1FE0;
            band_mask += floor;
            for (; bin < last; ++bin) {
                int address = (psd[bin] - band_mask) >> 5;
                address = std::min(63, std::max(0, address));
                bap[bin] = kBapTable[address];
            }
            ++band;
        } while (end > kBandStart[band - 1] + kBandSize[band - 1]);
    }

    return true;
}

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3
