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

float ac3DynamicRangeGain(uint8_t dynrng)
{
    // X is the top three bits as a signed integer; Y the low five, with an
    // implied leading 1 making it a fraction between 1/2 and 63/64.
    const int x = static_cast<int8_t>(dynrng) >> 5;
    const unsigned y = dynrng & 0x1Fu;
    return std::ldexp(static_cast<float>(32 + y) / 64.0f, x + 1);
}

bool ac3ParseAudioBlock(AC3BitReader& reader, const AC3FrameHeader& header,
                        AC3FrameState& state, AC3Block& block,
                        const char** reason, const EAC3AudioFrame* eac3)
{
    auto fail = [&](const char* why) { if (reason) { *reason = why; } return false; };
    const unsigned nfchans = header.channels;
    if (nfchans == 0 || nfchans > kMaxFullBandwidthChannels) {
        return fail("channel count out of range");
    }
    const bool dual_mono = header.acmod == AudioCodingMode::DualMono;
    const bool stereo = header.acmod == AudioCodingMode::Stereo;

    block = AC3Block();
    const unsigned blk = state.block_index;

    // Which slots code their mantissas with the Adaptive Hybrid Transform
    // is a frame-level decision; the flags record whether each slot's six
    // blocks of mantissas have been read yet.
    if (eac3 && blk == 0) {
        for (unsigned ch = 0; ch < nfchans; ++ch) {
            state.aht[ch] = eac3->chahtinu[ch] ? 1 : 0;
        }
        state.aht[kCouplingSlot] = eac3->cplahtinu ? 1 : 0;
        state.aht[kLfeSlot] = eac3->lfeahtinu ? 1 : 0;
    }

    // --- block switch and dither flags ---
    // E-AC-3 may switch either off for the whole frame, in which case no
    // block transforms short and every channel is dithered (Table E1.4).
    if (!eac3 || eac3->blkswe) {
        for (unsigned ch = 0; ch < nfchans; ++ch) {
            block.block_switch[ch] = reader.readBit() != 0;
        }
    }
    bool dithflag[kMaxFullBandwidthChannels] = {};
    for (unsigned ch = 0; ch < nfchans; ++ch) {
        dithflag[ch] = (!eac3 || eac3->dithflage) ? (reader.readBit() != 0) : true;
    }

    // --- dynamic range control, §7.7.1 ---
    // Applied by default, as the standard requires: broadcasters rely on it.
    // A block without a word keeps the last one, except block 0, which
    // starts from unity.
    if (reader.readBit()) {
        state.dynrng_gain = ac3DynamicRangeGain(static_cast<uint8_t>(reader.read(8)));
    } else if (state.block_index == 0) {
        state.dynrng_gain = 1.0f;
    }
    if (dual_mono) {
        if (reader.readBit()) {
            state.dynrng2_gain = ac3DynamicRangeGain(static_cast<uint8_t>(reader.read(8)));
        } else if (state.block_index == 0) {
            state.dynrng2_gain = 1.0f;
        }
    }
    block.dynamic_range = state.dynrng_gain;
    block.dynamic_range2 = dual_mono ? state.dynrng2_gain : state.dynrng_gain;

Debug::log("ac3", "  after dynrng: bit ", reader.tell());

    // --- spectral extension strategy and coordinates, E-AC-3 only ---
    if (eac3) {
        if (blk == 0) {
            // §E3.6.2: the default banding is loaded at frame start, so a
            // first block that omits the structure uses it.
            std::copy(kDefaultSpxBandStructure, kDefaultSpxBandStructure + kSpxSubbands,
                      state.spxbndstrc);
        }
        const bool spxstre = blk == 0 ? true : (reader.readBit() != 0);
        if (spxstre) {
            state.spxinu = reader.readBit() != 0;
            if (state.spxinu) {
                if (header.acmod == AudioCodingMode::Mono) {
                    state.chinspx[0] = true;
                } else {
                    for (unsigned ch = 0; ch < nfchans; ++ch) {
                        state.chinspx[ch] = reader.readBit() != 0;
                    }
                }
                const unsigned spxstrtf = reader.read(2);
                state.spxbegf = static_cast<uint8_t>(reader.read(3));
                const unsigned spxendf = reader.read(3);
                const unsigned begin = state.spxbegf < 6 ? state.spxbegf + 2u : state.spxbegf * 2u - 3u;
                const unsigned end = spxendf < 3 ? spxendf + 5u : spxendf * 2u + 3u;
                if (end > kSpxSubbands || begin >= end || spxstrtf >= begin) {
                    return fail("spectral extension range out of bounds");
                }
                if (reader.readBit()) { // spxbndstrce
                    for (unsigned sbnd = begin + 1; sbnd < end; ++sbnd) {
                        state.spxbndstrc[sbnd] = static_cast<uint8_t>(reader.readBit());
                    }
                }
                eac3SpxComputeBands(spxstrtf, begin, end, state.spxbndstrc, state.spx_bands);
            } else {
                for (unsigned ch = 0; ch < nfchans; ++ch) {
                    state.chinspx[ch] = false;
                    state.firstspxcos[ch] = true;
                }
            }
        }
        if (state.spxinu) {
            for (unsigned ch = 0; ch < nfchans; ++ch) {
                if (!state.chinspx[ch]) {
                    state.firstspxcos[ch] = true;
                    continue;
                }
                // Coordinates are mandatory the first time in a frame.
                bool spxcoe = false;
                if (state.firstspxcos[ch]) {
                    spxcoe = true;
                    state.firstspxcos[ch] = false;
                } else {
                    spxcoe = reader.readBit() != 0;
                }
                if (spxcoe) {
                    const unsigned spxblnd = reader.read(5);
                    const unsigned master = reader.read(2);
                    for (unsigned bnd = 0; bnd < state.spx_bands.count; ++bnd) {
                        const unsigned exponent = reader.read(4);
                        const unsigned mantissa = reader.read(2);
                        state.spx[ch].coordinate[bnd] = eac3SpxCoordinate(exponent, mantissa, master);
                    }
                    eac3SpxBlendFactors(spxblnd, state.spx_bands, state.spx[ch]);
                }
                state.spx[ch].attenuate = eac3->chinspxatten[ch];
                state.spx[ch].attenuation_code = eac3->spxattencod[ch];
            }
        }
    }

    // --- coupling strategy ---
    // E-AC-3 decides the strategy for every block in audfrm().
    const bool cplstre = eac3 ? eac3->cplstre[blk] : (reader.readBit() != 0);
    if (cplstre) {
        state.cplinu = eac3 ? eac3->cplinu[blk] : (reader.readBit() != 0);
        if (state.cplinu) {
            state.ecplinu = eac3 ? (reader.readBit() != 0) : false;
            if (eac3 && stereo) {
                // E-AC-3 2/0 always couples both channels; the flags are implied.
                state.chincpl[0] = true;
                state.chincpl[1] = true;
            } else {
                for (unsigned ch = 0; ch < nfchans; ++ch) {
                    state.chincpl[ch] = reader.readBit() != 0;
                }
            }
            if (!state.ecplinu) {
                state.phsflginu = stereo ? (reader.readBit() != 0) : false;
                state.cplbegf = static_cast<uint8_t>(reader.read(4));
                if (eac3 && state.spxinu) {
                    // §E3.3.1: coupling ends where spectral extension begins.
                    state.cplendf = state.spxbegf < 6 ? state.spxbegf - 2 : state.spxbegf * 2 - 7;
                } else {
                    state.cplendf = static_cast<int>(reader.read(4));
                }
                if (state.cplendf + 3 < static_cast<int>(state.cplbegf)) {
                    return fail("inverted coupling range");
                }
                state.ncplsubnd = static_cast<unsigned>(3 + state.cplendf - static_cast<int>(state.cplbegf));
                if (state.ncplsubnd > kMaxCouplingBands) {
                    return fail("too many coupling sub-bands");
                }
                // Sub-bands may be joined into wider coupling bands; band 0 always
                // starts one, and each set bit merges the sub-band into it.
                // AC-3 always sends the structure. E-AC-3 may omit it: the first
                // coupled block of a frame then takes Table E2.12's default, and a
                // later block keeps the previous block's (§E2.3.3.15).
                const bool cplbndstrce = eac3 ? (reader.readBit() != 0) : true;
                state.cplbndstrc[0] = 0;
                if (cplbndstrce) {
                    for (unsigned bnd = 1; bnd < state.ncplsubnd; ++bnd) {
                        state.cplbndstrc[bnd] = static_cast<uint8_t>(reader.readBit());
                    }
                } else if (!state.cplbndstrc_set) {
                    // The default is indexed by absolute sub-band, and the
                    // structure here is relative to cplbegf.
                    for (unsigned bnd = 1; bnd < state.ncplsubnd; ++bnd) {
                        state.cplbndstrc[bnd] = kDefaultCouplingBandStructure[state.cplbegf + bnd];
                    }
                }
                state.cplbndstrc_set = true;
                state.ncplbnd = 1;
                for (unsigned bnd = 1; bnd < state.ncplsubnd; ++bnd) {
                    if (!state.cplbndstrc[bnd]) {
                        ++state.ncplbnd;
                    }
                }
                state.strtmant[kCouplingSlot] = ac3CouplingStartMantissa(state.cplbegf);
                state.endmant[kCouplingSlot] = ac3CouplingEndMantissa(state.cplendf);
            } else {
                // Table E1.4, §E2.3.3.16-19: the enhanced coupling range, in
                // its own sub-bands, and how they group into bands.
                state.phsflginu = false;
                state.ecplbegf = static_cast<uint8_t>(reader.read(4));
                const unsigned f = state.ecplbegf;
                const unsigned begin = f < 3 ? f * 2 : (f < 13 ? f + 2 : f * 2 - 10);
                unsigned end = 0;
                if (!state.spxinu) {
                    end = reader.read(4) + 7;              // ecplendf
                } else {
                    end = state.spxbegf < 6 ? state.spxbegf + 5u : state.spxbegf * 2u;
                }
                if (end > kEcplSubbands || begin >= end) {
                    return fail("enhanced coupling range out of bounds");
                }
                if (reader.readBit()) {                    // ecplbndstrce
                    for (unsigned sbnd = std::max(9u, begin + 1); sbnd < end; ++sbnd) {
                        state.ecplbndstrc[sbnd] = static_cast<uint8_t>(reader.readBit());
                    }
                } else if (!state.ecplbndstrc_set) {
                    std::copy(kDefaultEcplBandStructure, kDefaultEcplBandStructure + kEcplSubbands,
                              state.ecplbndstrc);
                }
                state.ecplbndstrc_set = true;
                eac3EcplComputeBands(begin, end, state.ecplbndstrc, state.ecpl_bands);
                // The coupling channel's exponents and mantissas cover the
                // enhanced range; A/52's ncplgrps formula over that range is
                // exactly §E3.3.5's.
                state.strtmant[kCouplingSlot] = eac3EcplSubbandStart(begin);
                state.endmant[kCouplingSlot] = eac3EcplSubbandStart(end);
            }
        } else if (eac3) {
            // Coupling switched off: the next block to switch it on has to
            // send fresh coordinates and leak values again.
            for (unsigned ch = 0; ch < nfchans; ++ch) {
                state.chincpl[ch] = false;
                state.firstcplcos[ch] = true;
            }
            state.firstcplleak = true;
            state.phsflginu = false;
            state.ecplinu = false;
        }
    }

Debug::log("ac3", "  after cplstre: bit ", reader.tell(), " cplinu=", state.cplinu,
               " begf=", (unsigned)state.cplbegf, " endf=", state.cplendf,
               " nbnd=", state.ncplbnd);

    // --- coupling coordinates ---
    if (state.cplinu && state.ecplinu) {
        // Table E1.4. The first coupled channel is the phase reference, so it
        // sends no angle, chaos or transient flag; every parameter is
        // mandatory the first time a channel is coupled in a frame.
        int firstchincpl = -1;
        state.ecplangleintrp = reader.readBit() != 0;
        for (unsigned ch = 0; ch < nfchans; ++ch) {
            if (!state.chincpl[ch]) {
                state.firstcplcos[ch] = true;
                continue;
            }
            if (firstchincpl < 0) {
                firstchincpl = static_cast<int>(ch);
            }
            const bool later = static_cast<int>(ch) > firstchincpl;
            bool param1 = false;
            bool param2 = false;
            if (state.firstcplcos[ch]) {
                param1 = true;
                param2 = later;
                state.firstcplcos[ch] = false;
            } else {
                param1 = reader.readBit() != 0;
                param2 = later ? (reader.readBit() != 0) : false;
            }
            EAC3EcplChannel& p = state.ecpl[ch];
            p.first = !later;
            if (param1) {
                for (unsigned bnd = 0; bnd < state.ecpl_bands.count; ++bnd) {
                    p.amp[bnd] = static_cast<uint8_t>(reader.read(5));
                }
            }
            if (param2) {
                for (unsigned bnd = 0; bnd < state.ecpl_bands.count; ++bnd) {
                    p.angle[bnd] = static_cast<uint8_t>(reader.read(6));
                    p.chaos[bnd] = static_cast<uint8_t>(reader.read(3));
                }
            }
            p.transient = later ? (reader.readBit() != 0) : false;
        }
    } else if (state.cplinu) {
        bool any_new_coordinates = false;
        for (unsigned ch = 0; ch < nfchans; ++ch) {
            if (!state.chincpl[ch]) {
                state.firstcplcos[ch] = true;
                continue;
            }
            // E-AC-3 does not send cplcoe the first time a channel is coupled
            // in a frame: coordinates are mandatory then.
            bool cplcoe = false;
            if (eac3 && state.firstcplcos[ch]) {
                cplcoe = true;
                state.firstcplcos[ch] = false;
            } else {
                cplcoe = reader.readBit() != 0;
            }
            if (cplcoe) {
                any_new_coordinates = true;
                const unsigned master = reader.read(2);
                for (unsigned bnd = 0; bnd < state.ncplbnd; ++bnd) {
                    const unsigned exponent = reader.read(4);
                    const unsigned mantissa = reader.read(4);
                    state.cplco[ch][bnd] = couplingCoordinate(mantissa, exponent, master);
                }
            }
        }
        // The phase flags ride along with the coordinates: if neither channel
        // sent new ones this block, the flags are not in the stream either.
        if (stereo && state.phsflginu && any_new_coordinates) {
            for (unsigned bnd = 0; bnd < state.ncplbnd; ++bnd) {
                state.phsflg[bnd] = reader.readBit() != 0;
            }
        }
    }

Debug::log("ac3", "  after cplco: bit ", reader.tell());

    // --- rematrixing, 2/0 only ---
    if (stereo) {
        // E-AC-3 block 0 always carries the flags, so rematstr is implied.
        const bool rematstr = (eac3 && blk == 0) ? true : (reader.readBit() != 0);
        if (rematstr) {
            unsigned bands = 4;
            if (state.cplinu && state.ecplinu) {
                const unsigned f = state.ecplbegf;
                bands = f == 0 ? 0u : f == 1 ? 1u : f == 2 ? 2u : f < 5 ? 3u : 4u;
            } else if (state.cplinu) {
                bands = state.cplbegf > 2 ? 4u : (state.cplbegf > 0 ? 3u : 2u);
            } else if (state.spxinu) {
                bands = state.spxbegf < 2 ? 3u : 4u;       // §E3.3.2
            }
            for (unsigned rbnd = 0; rbnd < 4; ++rbnd) {
                state.rematflg[rbnd] = rbnd < bands ? (reader.readBit() != 0) : false;
            }
        }
    }

Debug::log("ac3", "  after remat: bit ", reader.tell());

    // --- exponent strategies ---
    if (eac3) {
        // E-AC-3 settles them for the whole frame in audfrm().
        if (state.cplinu) {
            state.expstr[kCouplingSlot] = eac3->cplexpstr[blk];
        }
        for (unsigned ch = 0; ch < nfchans; ++ch) {
            state.expstr[ch] = eac3->chexpstr[blk][ch];
        }
        if (header.lfeon) {
            state.expstr[kLfeSlot] = eac3->lfeexpstr[blk];
        }
    } else {
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
    }

    // --- channel bandwidth ---
    for (unsigned ch = 0; ch < nfchans; ++ch) {
        state.strtmant[ch] = 0;
        if (state.chincpl[ch] && state.cplinu) {
            // §5.4.3.61: a coupled channel's mantissa count comes from
            // cplbegf, a property of this block's coupling strategy, so it
            // has to be re-derived even when the channel reuses its
            // exponents -- coupling can start, or its range move, in a block
            // that sends none.
            state.endmant[ch] = state.strtmant[kCouplingSlot];
            continue;
        }
        if (state.spxinu && state.chinspx[ch]) {
            // §E3.3.3: no chbwcod is sent; the coded band stops at the first
            // synthesised sub-band.
            state.endmant[ch] = eac3SpxBandStart(state.spx_bands.begin_subband);
            continue;
        }
        if (state.expstr[ch] == ExponentStrategy::Reuse) {
            continue;
        }
        const uint8_t chbwcod = static_cast<uint8_t>(reader.read(6));
        state.endmant[ch] = ac3ChannelEndMantissa(chbwcod);
        if (state.endmant[ch] == 0) {
            // A/52 §5.4.3.24: past 60 the stream is invalid and the
            // decoder is told to mute rather than carry on.
            return fail("channel bandwidth code past 60");
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
    if (eac3) {
        const auto setFine = [&](int value) {
            state.fsnroffst[kCouplingSlot] = value;
            state.fsnroffst[kLfeSlot] = value;
            for (unsigned ch = 0; ch < nfchans; ++ch) {
                state.fsnroffst[ch] = value;
            }
        };
        const auto setFastGain = [&](uint8_t code) {
            state.fgaincod[kCouplingSlot] = code;
            state.fgaincod[kLfeSlot] = code;
            for (unsigned ch = 0; ch < nfchans; ++ch) {
                state.fgaincod[ch] = code;
            }
        };

        // Parametric model: sent only if audfrm() enabled the syntax, and
        // otherwise the defaults of Table E1.4.
        if (eac3->bamode) {
            if (reader.readBit()) { // baie
                state.sdcycod = static_cast<uint8_t>(reader.read(2));
                state.fdcycod = static_cast<uint8_t>(reader.read(2));
                state.sgaincod = static_cast<uint8_t>(reader.read(2));
                state.dbpbcod = static_cast<uint8_t>(reader.read(2));
                state.floorcod = static_cast<uint8_t>(reader.read(3));
            }
        } else {
            state.sdcycod = 0x2;
            state.fdcycod = 0x1;
            state.sgaincod = 0x1;
            state.dbpbcod = 0x2;
            state.floorcod = 0x7;
        }

        // SNR offsets, Table E2.9: strategy 1 is one pair for the whole
        // frame; 2 and 3 are sent per block (always in block 0) and reused
        // when a block omits them.
        if (eac3->snroffststr == 0x0) {
            state.csnroffst = eac3->frmcsnroffst;
            setFine(eac3->frmfsnroffst);
        } else {
            const bool snroffste = blk == 0 ? true : (reader.readBit() != 0);
            if (snroffste) {
                state.csnroffst = static_cast<int>(reader.read(6));
                if (eac3->snroffststr == 0x1) {
                    setFine(static_cast<int>(reader.read(4))); // blkfsnroffst
                } else if (eac3->snroffststr == 0x2) {
                    if (state.cplinu) {
                        state.fsnroffst[kCouplingSlot] = static_cast<int>(reader.read(4));
                    }
                    for (unsigned ch = 0; ch < nfchans; ++ch) {
                        state.fsnroffst[ch] = static_cast<int>(reader.read(4));
                    }
                    if (header.lfeon) {
                        state.fsnroffst[kLfeSlot] = static_cast<int>(reader.read(4));
                    }
                } else {
                    return fail("reserved SNR offset strategy");
                }
            }
        }

        // Fast gain codes default to 4 unless a block sends its own.
        const bool fgaincode = eac3->frmfgaincode ? (reader.readBit() != 0) : false;
        if (fgaincode) {
            if (state.cplinu) {
                state.fgaincod[kCouplingSlot] = static_cast<uint8_t>(reader.read(3));
            }
            for (unsigned ch = 0; ch < nfchans; ++ch) {
                state.fgaincod[ch] = static_cast<uint8_t>(reader.read(3));
            }
            if (header.lfeon) {
                state.fgaincod[kLfeSlot] = static_cast<uint8_t>(reader.read(3));
            }
        } else {
            setFastGain(0x4);
        }

        // An offset for re-encoding to AC-3; nothing a decoder applies.
        if (header.strmtyp == 0x0 && reader.readBit()) { // convsnroffste
            reader.skip(10);                            // convsnroffst
        }

        if (state.cplinu) {
            bool cplleake = false;
            if (state.firstcplleak) {
                cplleake = true;
                state.firstcplleak = false;
            } else {
                cplleake = reader.readBit() != 0;
            }
            if (cplleake) {
                state.cplfleak = static_cast<uint8_t>(reader.read(3));
                state.cplsleak = static_cast<uint8_t>(reader.read(3));
            }
        }
        state.have_allocation = true;
    } else {
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
            state.cplfleak = static_cast<uint8_t>(reader.read(3));
            state.cplsleak = static_cast<uint8_t>(reader.read(3));
        }
    }

Debug::log("ac3", "  after snroffst: bit ", reader.tell());

    // --- delta bit allocation ---
    const bool deltbaie = (!eac3 || eac3->dbaflde) ? (reader.readBit() != 0) : false;
    if (deltbaie) {
        uint8_t cpldeltbae = kDeltaReuse;
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
        // Table 5.16: 0 reuses the previous block's segments, 1 means new
        // information follows, 2 means apply none -- which is not the same as
        // reusing, so the stored segments have to go -- and 3 is reserved.
        auto applyStrategy = [&](unsigned slot, uint8_t strategy) {
            switch (strategy) {
            case kDeltaReuse:  break;
            case kDeltaNew:    readSegments(slot); break;
            case kDeltaNone:   state.deltas[slot].clear(); break;
            default:           return false; // reserved: 5.4.3.48 says mute
            }
            return true;
        };
        if (state.cplinu && !applyStrategy(kCouplingSlot, cpldeltbae)) {
            return fail("reserved delta bit allocation strategy");
        }
        for (unsigned ch = 0; ch < nfchans; ++ch) {
            if (!applyStrategy(ch, deltbae[ch])) {
                return fail("reserved delta bit allocation strategy");
            }
        }
    } else if (blk == 0 || (eac3 && !eac3->dbaflde)) {
        // An E-AC-3 frame without the syntax has no delta allocation at all.
        // 5.4.3.47: deltbaie of 0 in block 0 is defined to mean the same as a
        // deltbae of '10' everywhere -- no delta allocation at all.
        for (auto& slot : state.deltas) {
            slot.clear();
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
    if ((!eac3 || eac3->skipflde) && reader.readBit()) { // skiple
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
        parameters.cplfleak = state.cplfleak;
        parameters.cplsleak = state.cplsleak;
        return ac3ComputeBitAllocation(state.exponents[slot], state.strtmant[slot],
                                       state.endmant[slot], kind, parameters,
                                       state.deltas[slot], bap[slot],
                                       state.aht[slot] != 0 ? kHebapTable : nullptr);
    };

    // §7.2.2.1.1: when every SNR offset in the block is zero the encoder is
    // saying it spent no bits at all. Every bap is zero and the parametric
    // routine is skipped entirely -- which is not the same as running it and
    // getting zeros, because csnroffst == 0 makes snroffset negative.
    bool any_snr_offset = state.csnroffst != 0;
    for (unsigned ch = 0; ch < nfchans && !any_snr_offset; ++ch) {
        any_snr_offset = state.fsnroffst[ch] != 0;
    }
    if (state.cplinu && state.fsnroffst[kCouplingSlot] != 0) {
        any_snr_offset = true;
    }
    if (header.lfeon && state.fsnroffst[kLfeSlot] != 0) {
        any_snr_offset = true;
    }

    for (unsigned ch = 0; ch < nfchans; ++ch) {
        if (state.endmant[ch] == 0) {
            return fail("channel has no bandwidth");
        }
        if (any_snr_offset && !allocate(ch, AllocationChannel::FullBandwidth)) {
            return fail("channel allocation");
        }
    }
    if (any_snr_offset) {
        if (state.cplinu && !allocate(kCouplingSlot, AllocationChannel::Coupling)) {
            return fail("coupling allocation");
        }
        if (header.lfeon && !allocate(kLfeSlot, AllocationChannel::LFE)) {
            return fail("LFE allocation");
        }
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
                // A grouped bap pays its codeword once for the whole group,
                // so charging every member the full width would treble it.
                const unsigned size = ac3MantissaGroupSize(bap[ch][bin]);
                bits += size ? ac3MantissaGroupBits(bap[ch][bin]) / size : 0;
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

    // One slot's mantissas for this block. A conventional slot reads them
    // here. An AHT slot reads all six blocks' worth the first time it is
    // reached in a frame -- the transform spans the frame -- and every block,
    // that one included, then takes its own row.
    auto readSlot = [&](unsigned slot, unsigned first, unsigned last, float* out) {
        if (state.aht[slot] == 0) {
            for (unsigned bin = first; bin < last; ++bin) {
                out[bin] = mantissas.next(reader, bap[slot][bin], state.exponents[slot][bin]);
            }
            return true;
        }
        if (state.aht[slot] == 1) {
            const char* why = nullptr;
            if (!eac3AhtReadChannel(reader, bap[slot], first, last, state.exponents[slot],
                                    state.aht_spectrum[slot], &why)) {
                return fail(why ? why : "AHT mantissas");
            }
            state.aht[slot] = -1;
        }
        for (unsigned bin = first; bin < last; ++bin) {
            out[bin] = state.aht_spectrum[slot].value[blk][bin];
        }
        return true;
    };

    for (unsigned ch = 0; ch < nfchans; ++ch) {
        if (!readSlot(ch, 0, state.endmant[ch], block.coefficients[ch])) {
            return false;
        }
        if (state.cplinu && state.chincpl[ch] && !coupling_read) {
            if (!readSlot(kCouplingSlot, state.strtmant[kCouplingSlot],
                          state.endmant[kCouplingSlot], coupling)) {
                return false;
            }
            coupling_read = true;
        }
    }
    if (header.lfeon) {
        if (!readSlot(kLfeSlot, 0, kLfeEndMantissa, block.coefficients[kLfeSlot])) {
            return false;
        }
    }

Debug::log("ac3", "  after mantissas: bit ", reader.tell());

    // --- decoupling, A/52 §7.4.4 ---
    // Each coupled channel gets the shared coupling channel back, scaled by
    // its own per-band coordinate. That is what makes coupling cheap: one
    // spectrum is sent and several channels reconstruct from it.
    if (state.cplinu && coupling_read && state.ecplinu) {
        block.ecplinu = true;
        block.ecpl_angle_interpolation = state.ecplangleintrp;
        std::copy(coupling, coupling + kSamplesPerBlock, block.ecpl_coupling);
        block.ecpl_bands = state.ecpl_bands;
        for (unsigned ch = 0; ch < nfchans; ++ch) {
            block.chincpl[ch] = state.chincpl[ch];
            block.ecpl[ch] = state.ecpl[ch];
        }
    }
    if (state.cplinu && coupling_read && !state.ecplinu) {
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

    // --- dither, A/52 §7.3.4 ---
    // Bins that got no bits are filled with noise rather than silence, which
    // keeps the reconstruction from sounding hollow where the allocator spent
    // nothing. The spec puts this after decoupling on purpose: coupled
    // channels share one spectrum, and dithering them separately afterwards
    // is what keeps their top ends uncorrelated.
    for (unsigned ch = 0; ch < nfchans; ++ch) {
        if (!dithflag[ch]) {
            continue;
        }
        const unsigned end = (state.cplinu && state.chincpl[ch])
                           ? state.endmant[kCouplingSlot] : state.endmant[ch];
        for (unsigned bin = 0; bin < end; ++bin) {
            const unsigned slot = (state.cplinu && state.chincpl[ch]
                                   && bin >= state.strtmant[kCouplingSlot])
                                ? kCouplingSlot : ch;
            if (bap[slot][bin] != 0 || state.aht[slot] != 0) {
                continue;
            }
            block.coefficients[ch][bin] =
                state.dither.next() / static_cast<float>(1u << state.exponents[slot][bin]);
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

    // --- spectral extension synthesis, §E3.6.4 ---
    // Last: it copies a channel's own finished low band upward, so the
    // baseband has to be decoupled and rematrixed first. (Annex E does not
    // place it in the pipeline explicitly; this is the only order in which
    // the copy is of the channel rather than of a coupling or sum signal.)
    if (state.spxinu) {
        for (unsigned ch = 0; ch < nfchans; ++ch) {
            if (!state.chinspx[ch]) {
                continue;
            }
            if (block.ecplinu && state.chincpl[ch]) {
                block.spx_deferred[ch] = true;
                continue;
            }
            eac3SpxSynthesise(block.coefficients[ch], state.spx_bands, state.spx[ch],
                              state.spx_noise);
        }
        block.spx_bands = state.spx_bands;
        for (unsigned ch = 0; ch < nfchans; ++ch) {
            block.spx[ch] = state.spx[ch];
        }
    }

    if (reader.overrun()) {
        return fail("ran past the end of the frame");
    }
    ++state.block_index;
    return true;
}

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3
