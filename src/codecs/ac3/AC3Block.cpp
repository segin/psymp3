/*
 * AC3Block.cpp - AC-3 audio block parsing (A/52 §5.4.3, §7.4, §7.5).
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * Written from ATSC A/52:2012, "Digital Audio Compression Standard". The
 * parsing order below follows Table 5.3 exactly; the fields are not
 * independently addressable, so reading one out of turn shifts everything
 * after it.
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

/// A/52 §7.5: which bins each rematrixing band covers. The fourth band's end
/// depends on the channel bandwidth, so it is clamped by the caller.
constexpr unsigned kRematrixBands[5] = {13, 25, 37, 61, 253};

/// Decodes a coupling coordinate, A/52 §7.4.3. The exponent is offset by the
/// master coordinate, which extends the range beyond what four bits reach.
float couplingCoordinate(unsigned mantissa, unsigned exponent, unsigned master)
{
    // §7.4.3: an exponent of 15 means the mantissa is used without its
    // implicit leading one, which is how the smallest coordinates are spelled.
    float value = exponent == 15
                ? static_cast<float>(mantissa) / 16.0f
                : (static_cast<float>(mantissa) + 16.0f) / 32.0f;
    const unsigned shift = exponent + 3 * master;
    return value / static_cast<float>(1u << std::min(shift, 31u));
}

} // namespace

bool ac3ParseAudioBlock(AC3BitReader& reader, const AC3FrameHeader& header,
                        AC3FrameState& state, AC3Block& block,
                        const char** reason)
{
    auto fail = [&](const char* why) { if (reason) { *reason = why; } return false; };
    const unsigned nfchans = header.channels;
    if (nfchans == 0 || nfchans > kMaxFullBandwidthChannels) {
        return fail("channel count out of range");
    }
    const bool dual_mono = header.acmod == AudioCodingMode::DualMono;
    const bool stereo = header.acmod == AudioCodingMode::Stereo;

    block = AC3Block();

    // --- block switch and dither flags ---
    for (unsigned ch = 0; ch < nfchans; ++ch) {
        block.block_switch[ch] = reader.readBit() != 0;
    }
    bool dithflag[kMaxFullBandwidthChannels] = {};
    for (unsigned ch = 0; ch < nfchans; ++ch) {
        dithflag[ch] = reader.readBit() != 0;
    }
    (void)dithflag; // §7.3.4 dither is not generated yet; see AC3MantissaReader

    // --- dynamic range control ---
    if (reader.readBit()) {
        reader.skip(8); // dynrng, applied at output rather than here
    }
    if (dual_mono && reader.readBit()) {
        reader.skip(8); // dynrng2
    }

Debug::log("ac3", "  after dynrng: bit ", reader.tell());

    // --- coupling strategy ---
    if (reader.readBit()) { // cplstre
        state.cplinu = reader.readBit() != 0;
        if (state.cplinu) {
            for (unsigned ch = 0; ch < nfchans; ++ch) {
                state.chincpl[ch] = reader.readBit() != 0;
            }
            state.phsflginu = stereo ? (reader.readBit() != 0) : false;
            state.cplbegf = static_cast<uint8_t>(reader.read(4));
            state.cplendf = static_cast<uint8_t>(reader.read(4));
            if (state.cplendf + 3 < state.cplbegf) {
                return fail("inverted coupling range");
            }
            state.ncplsubnd = 3u + state.cplendf - state.cplbegf;
            if (state.ncplsubnd > kMaxCouplingBands) {
                return fail("too many coupling sub-bands");
            }
            // Sub-bands may be joined into wider coupling bands; band 0 always
            // starts one, and each set bit merges the sub-band into it.
            state.ncplbnd = 1;
            state.cplbndstrc[0] = 0;
            for (unsigned bnd = 1; bnd < state.ncplsubnd; ++bnd) {
                state.cplbndstrc[bnd] = static_cast<uint8_t>(reader.readBit());
                if (!state.cplbndstrc[bnd]) {
                    ++state.ncplbnd;
                }
            }
            state.strtmant[kCouplingSlot] = ac3CouplingStartMantissa(state.cplbegf);
            state.endmant[kCouplingSlot] = ac3CouplingEndMantissa(state.cplendf);
        }
    }

Debug::log("ac3", "  after cplstre: bit ", reader.tell(), " cplinu=", state.cplinu,
               " begf=", (unsigned)state.cplbegf, " endf=", (unsigned)state.cplendf,
               " nbnd=", state.ncplbnd);

    // --- coupling coordinates ---
    if (state.cplinu) {
        for (unsigned ch = 0; ch < nfchans; ++ch) {
            if (!state.chincpl[ch]) {
                continue;
            }
            if (reader.readBit()) { // cplcoe
                const unsigned master = reader.read(2);
                for (unsigned bnd = 0; bnd < state.ncplbnd; ++bnd) {
                    const unsigned exponent = reader.read(4);
                    const unsigned mantissa = reader.read(4);
                    state.cplco[ch][bnd] = couplingCoordinate(mantissa, exponent, master);
                }
            }
        }
        if (stereo && state.phsflginu) {
            for (unsigned bnd = 0; bnd < state.ncplbnd; ++bnd) {
                state.phsflg[bnd] = reader.readBit() != 0;
            }
        }
    }

Debug::log("ac3", "  after cplco: bit ", reader.tell());

    // --- rematrixing, 2/0 only ---
    if (stereo) {
        if (reader.readBit()) { // rematstr
            unsigned bands = 4;
            if (state.cplinu) {
                bands = state.cplbegf > 2 ? 4u : (state.cplbegf > 0 ? 3u : 2u);
            }
            for (unsigned rbnd = 0; rbnd < 4; ++rbnd) {
                state.rematflg[rbnd] = rbnd < bands ? (reader.readBit() != 0) : false;
            }
        }
    }

Debug::log("ac3", "  after remat: bit ", reader.tell());

    // --- exponent strategies ---
    if (state.cplinu) {
        state.expstr[kCouplingSlot] = static_cast<ExponentStrategy>(reader.read(2));
    }
    for (unsigned ch = 0; ch < nfchans; ++ch) {
        state.expstr[ch] = static_cast<ExponentStrategy>(reader.read(2));
    }
    if (header.lfeon) {
        state.expstr[kLfeSlot] = reader.readBit()
                               ? ExponentStrategy::D15 : ExponentStrategy::Reuse;
    }

    // --- channel bandwidth ---
    for (unsigned ch = 0; ch < nfchans; ++ch) {
        state.strtmant[ch] = 0;
        if (state.expstr[ch] == ExponentStrategy::Reuse) {
            continue;
        }
        if (state.chincpl[ch] && state.cplinu) {
            state.endmant[ch] = state.strtmant[kCouplingSlot];
        } else {
            const uint8_t chbwcod = static_cast<uint8_t>(reader.read(6));
            state.endmant[ch] = ac3ChannelEndMantissa(chbwcod);
            if (state.endmant[ch] == 0) {
                // A/52 §5.4.3.24: past 60 the stream is invalid and the
                // decoder is told to mute rather than carry on.
                return fail("channel bandwidth code past 60");
            }
        }
    }

Debug::log("ac3", "  after expstr/bw: bit ", reader.tell());

    // --- exponents ---
    if (state.cplinu && state.expstr[kCouplingSlot] != ExponentStrategy::Reuse) {
        // §7.1.3: cplabsexp is sent as four bits standing for a five-bit
        // value whose low bit is always zero, so it is doubled. It is a
        // reference point rather than a real exponent.
        const uint8_t absolute = static_cast<uint8_t>(reader.read(4) << 1);
        const unsigned groups = ac3CouplingExponentGroups(
            state.expstr[kCouplingSlot], state.strtmant[kCouplingSlot],
            state.endmant[kCouplingSlot]);
        if (ac3DecodeExponents(reader, state.expstr[kCouplingSlot], groups, absolute,
                               state.exponents[kCouplingSlot] + state.strtmant[kCouplingSlot],
                               kBinCount - state.strtmant[kCouplingSlot],
                               /*absolute_is_bin=*/false) == 0) {
            return fail("coupling exponents");
        }
    }
    for (unsigned ch = 0; ch < nfchans; ++ch) {
        if (state.expstr[ch] == ExponentStrategy::Reuse) {
            continue;
        }
        const uint8_t absolute = static_cast<uint8_t>(reader.read(4));
        const unsigned groups = ac3ChannelExponentGroups(state.expstr[ch], state.endmant[ch]);
        if (ac3DecodeExponents(reader, state.expstr[ch], groups, absolute,
                               state.exponents[ch], kBinCount) == 0) {
            return fail("channel exponents");
        }
        reader.skip(2); // gainrng, used only by encoders
    }
    if (header.lfeon && state.expstr[kLfeSlot] != ExponentStrategy::Reuse) {
        state.strtmant[kLfeSlot] = 0;
        state.endmant[kLfeSlot] = kLfeEndMantissa;
        const uint8_t absolute = static_cast<uint8_t>(reader.read(4));
        if (ac3DecodeExponents(reader, ExponentStrategy::D15, kLfeExponentGroups, absolute,
                               state.exponents[kLfeSlot], kBinCount) == 0) {
            return fail("LFE exponents");
        }
    }

Debug::log("ac3", "  after exponents: bit ", reader.tell());

    // --- bit allocation parameters ---
    if (reader.readBit()) { // baie
        state.sdcycod = static_cast<uint8_t>(reader.read(2));
        state.fdcycod = static_cast<uint8_t>(reader.read(2));
        state.sgaincod = static_cast<uint8_t>(reader.read(2));
        state.dbpbcod = static_cast<uint8_t>(reader.read(2));
        state.floorcod = static_cast<uint8_t>(reader.read(3));
        state.have_allocation = true;
    }
    if (reader.readBit()) { // snroffste
        state.csnroffst = static_cast<int>(reader.read(6));
        if (state.cplinu) {
            state.fsnroffst[kCouplingSlot] = static_cast<int>(reader.read(4));
            state.fgaincod[kCouplingSlot] = static_cast<uint8_t>(reader.read(3));
        }
        for (unsigned ch = 0; ch < nfchans; ++ch) {
            state.fsnroffst[ch] = static_cast<int>(reader.read(4));
            state.fgaincod[ch] = static_cast<uint8_t>(reader.read(3));
        }
        if (header.lfeon) {
            state.fsnroffst[kLfeSlot] = static_cast<int>(reader.read(4));
            state.fgaincod[kLfeSlot] = static_cast<uint8_t>(reader.read(3));
        }
        state.have_allocation = true;
    }
    if (state.cplinu && reader.readBit()) { // cplleake
        reader.skip(6); // cplfleak, cplsleak
    }

Debug::log("ac3", "  after snroffst: bit ", reader.tell());

    // --- delta bit allocation ---
    if (reader.readBit()) { // deltbaie
        uint8_t cpldeltbae = 0;
        uint8_t deltbae[kMaxFullBandwidthChannels] = {};
        if (state.cplinu) {
            cpldeltbae = static_cast<uint8_t>(reader.read(2));
        }
        for (unsigned ch = 0; ch < nfchans; ++ch) {
            deltbae[ch] = static_cast<uint8_t>(reader.read(2));
        }
        auto readSegments = [&](unsigned slot) {
            state.deltas[slot].clear();
            const unsigned count = reader.read(3) + 1;
            for (unsigned seg = 0; seg < count; ++seg) {
                DeltaBitAllocation delta;
                delta.offset = static_cast<uint8_t>(reader.read(5));
                delta.length = static_cast<uint8_t>(reader.read(4));
                delta.bit_allocation = static_cast<uint8_t>(reader.read(3));
                state.deltas[slot].push_back(delta);
            }
        };
        // A value of 1 is "new information follows"; 0 reuses the previous
        // block's and 2 clears it.
        if (state.cplinu && cpldeltbae == 1) {
            readSegments(kCouplingSlot);
        }
        for (unsigned ch = 0; ch < nfchans; ++ch) {
            if (deltbae[ch] == 1) {
                readSegments(ch);
            }
        }
    }

    if (!state.have_allocation) {
        return fail("no bit allocation parameters yet");
    }

Debug::log("ac3", "  after deltba: bit ", reader.tell());

    // --- Table 5.3: unused dummy data ---
    // An encoder may pad a block with whole null bytes here. Nothing reads
    // them, but they sit between the allocation and the mantissas, so a
    // decoder that does not step over them starts the mantissa stream inside
    // the padding and loses the rest of the frame.
    if (reader.readBit()) { // skiple
        const unsigned skipl = reader.read(9);
        reader.skip(skipl * 8); // skipfld
        Debug::log("ac3", "  skipped ", skipl, " dummy bytes");
    }

    // --- bit allocation and mantissas ---
    // Every channel's allocation is computed before any mantissa is read,
    // because the mantissas of all channels are interleaved in one stream and
    // their widths come from these pointers.
    uint8_t bap[kChannelSlots][kBinCount] = {};
    auto allocate = [&](unsigned slot, AllocationChannel kind) {
        AllocationParameters parameters;
        parameters.fscod = header.fscod;
        parameters.sdcycod = state.sdcycod;
        parameters.fdcycod = state.fdcycod;
        parameters.sgaincod = state.sgaincod;
        parameters.dbpbcod = state.dbpbcod;
        parameters.floorcod = state.floorcod;
        parameters.fgaincod = state.fgaincod[slot];
        parameters.snroffset = (((state.csnroffst - 15) << 4) + state.fsnroffst[slot]) << 2;
        return ac3ComputeBitAllocation(state.exponents[slot], state.strtmant[slot],
                                       state.endmant[slot], kind, parameters,
                                       state.deltas[slot], bap[slot]);
    };

    for (unsigned ch = 0; ch < nfchans; ++ch) {
        if (state.endmant[ch] == 0) {
            return fail("channel has no bandwidth");
        }
        if (!allocate(ch, AllocationChannel::FullBandwidth)) {
            return fail("channel allocation");
        }
    }
    if (state.cplinu && !allocate(kCouplingSlot, AllocationChannel::Coupling)) {
        return fail("coupling allocation");
    }
    if (header.lfeon && !allocate(kLfeSlot, AllocationChannel::LFE)) {
        return fail("LFE allocation");
    }

    // Mantissas are interleaved channel by channel across the spectrum: for
    // each channel in turn, every bin it codes. The coupling channel's
    // mantissas are read at the point the first coupled channel reaches its
    // coupling range.
    // Histogram the allocation before any mantissa is read: if the widths are
    // wrong, this is where it shows rather than three blocks later.
    if (Debug::isChannelEnabled("ac3")) {
        for (unsigned ch = 0; ch < nfchans; ++ch) {
            unsigned counts[16] = {0};
            unsigned bits = 0;
            for (unsigned bin = 0; bin < state.endmant[ch]; ++bin) {
                counts[bap[ch][bin] & 15]++;
                bits += ac3MantissaGroupBits(bap[ch][bin]);
            }
            unsigned high = 0;
            for (unsigned k = 6; k < 16; ++k) { high += counts[k]; }
            Debug::log("ac3", "   ch", ch, " endmant=", state.endmant[ch],
                       " csnroffst=", state.csnroffst,
                       " fsnroffst=", state.fsnroffst[ch],
                       " floorcod=", (unsigned)state.floorcod,
                       " bap0=", counts[0],
                       " bap1-5=", counts[1]+counts[2]+counts[3]+counts[4]+counts[5],
                       " bap>=6=", high, " bap15=", counts[15], " rawbits=", bits);
        }
    }

    AC3MantissaReader mantissas;
    bool coupling_read = false;
    float coupling[kSamplesPerBlock] = {};

    for (unsigned ch = 0; ch < nfchans; ++ch) {
        for (unsigned bin = 0; bin < state.endmant[ch]; ++bin) {
            block.coefficients[ch][bin] =
                mantissas.next(reader, bap[ch][bin], state.exponents[ch][bin]);
        }
        if (state.cplinu && state.chincpl[ch] && !coupling_read) {
            for (unsigned bin = state.strtmant[kCouplingSlot];
                 bin < state.endmant[kCouplingSlot]; ++bin) {
                coupling[bin] = mantissas.next(reader, bap[kCouplingSlot][bin],
                                               state.exponents[kCouplingSlot][bin]);
            }
            coupling_read = true;
        }
    }
    if (header.lfeon) {
        for (unsigned bin = 0; bin < kLfeEndMantissa; ++bin) {
            block.coefficients[kLfeSlot][bin] =
                mantissas.next(reader, bap[kLfeSlot][bin], state.exponents[kLfeSlot][bin]);
        }
    }

Debug::log("ac3", "  after mantissas: bit ", reader.tell());

    // --- decoupling, A/52 §7.4.4 ---
    // Each coupled channel gets the shared coupling channel back, scaled by
    // its own per-band coordinate. That is what makes coupling cheap: one
    // spectrum is sent and several channels reconstruct from it.
    if (state.cplinu && coupling_read) {
        for (unsigned ch = 0; ch < nfchans; ++ch) {
            if (!state.chincpl[ch]) {
                continue;
            }
            unsigned band = 0;
            unsigned sub = 0;
            for (unsigned bin = state.strtmant[kCouplingSlot];
                 bin < state.endmant[kCouplingSlot]; ++bin) {
                const unsigned this_sub = (bin - state.strtmant[kCouplingSlot]) / 12;
                while (sub < this_sub) {
                    ++sub;
                    if (sub < state.ncplsubnd && !state.cplbndstrc[sub]) {
                        ++band;
                    }
                }
                float value = coupling[bin] * state.cplco[ch][band];
                // In 2/0 the second channel's phase may be inverted per band.
                if (stereo && state.phsflginu && ch == 1 && band < kMaxCouplingBands
                    && state.phsflg[band]) {
                    value = -value;
                }
                block.coefficients[ch][bin] = value;
            }
            // endmant is deliberately left alone. It is where this channel's
            // own mantissas stop, and the next block reads that many again;
            // widening it to the coupling range would have the next block read
            // mantissas for bins it never codes and run off the end of the
            // frame. The coupling bins are filled from the shared channel
            // here, not read per channel.
        }
    }

    // --- rematrixing, A/52 §7.5 ---
    // 2/0 only: the encoder may have sent sum and difference instead of left
    // and right where that codes better. Undoing it is the same operation.
    if (stereo) {
        for (unsigned rbnd = 0; rbnd < 4; ++rbnd) {
            if (!state.rematflg[rbnd]) {
                continue;
            }
            const unsigned begin = rbnd == 0 ? 13u : kRematrixBands[rbnd - 1];
            unsigned end = kRematrixBands[rbnd];
            end = std::min(end, std::min(state.endmant[0], state.endmant[1]));
            for (unsigned bin = begin; bin < end; ++bin) {
                const float sum = block.coefficients[0][bin];
                const float difference = block.coefficients[1][bin];
                block.coefficients[0][bin] = sum + difference;
                block.coefficients[1][bin] = sum - difference;
            }
        }
    }

    if (reader.overrun()) {
        return fail("ran past the end of the frame");
    }
    return true;
}

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3
