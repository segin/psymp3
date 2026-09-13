/*
 * EAC3Frame.cpp - E-AC-3 bsi() and audfrm(), A/52 Annex E Tables E1.2 and E1.3
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

namespace {

/// Table E2.4: numblkscod 0..3 is 1, 2, 3 or 6 audio blocks.
constexpr uint8_t kBlocksForCode[4] = { 1, 2, 3, 6 };

/// ceiling(log2(n)) for the n >= 1 that nblkstrtbits needs.
unsigned ceilLog2(unsigned n)
{
    unsigned bits = 0;
    while ((1u << bits) < n) {
        ++bits;
    }
    return bits;
}

} // namespace

bool eac3ParseFrame(AC3BitReader& reader, AC3FrameHeader& header,
                    EAC3AudioFrame& frame, const char** reason)
{
    auto fail = [&](const char* why) { if (reason) { *reason = why; } return false; };

    frame = EAC3AudioFrame();
    const size_t start = reader.tell();

    // Let the shared header parser validate the sync word and bsid and fill
    // in rate, size and channel layout, then walk bsi() again in full from
    // just past the sync word: AC3FrameHeader stops at dialnorm, and the
    // audio frame cannot be found without reading everything after it.
    if (!ac3ParseFrameHeader(reader, header)) {
        return fail("bad syncframe header");
    }
    if (!header.isEAC3()) {
        return fail("not an E-AC-3 frame");
    }
    reader.seek(start + 16);

    // --- bsi(), Table E1.2 ---
    const unsigned strmtyp = reader.read(2);
    frame.substreamid = static_cast<uint8_t>(reader.read(3));
    const unsigned frmsiz = reader.read(11);
    const unsigned fscod = reader.read(2);
    unsigned numblkscod = 3;
    if (fscod == 0x3) {
        reader.skip(2);                                   // fscod2
    } else {
        numblkscod = reader.read(2);
    }
    const unsigned blocks = kBlocksForCode[numblkscod];
    frame.blocks = static_cast<uint8_t>(blocks);

    const unsigned acmod = reader.read(3);
    const bool lfeon = reader.readBit() != 0;
    reader.skip(5);                                       // bsid, already checked
    reader.skip(5);                                       // dialnorm
    frame.compre = reader.readBit() != 0;
    if (frame.compre) {
        frame.compr = static_cast<uint8_t>(reader.read(8));
    }
    if (acmod == 0x0) {
        // 1+1 carries a second programme, so these repeat.
        reader.skip(5);                                   // dialnorm2
        if (reader.readBit()) { reader.skip(8); }         // compr2e / compr2
    }
    if (strmtyp == 0x1) {
        frame.chanmape = reader.readBit() != 0;
        if (frame.chanmape) {
            frame.chanmap = static_cast<uint16_t>(reader.read(16));
        }
    }

    if (reader.readBit()) {                               // mixmdate
        if (acmod > 0x2) { reader.skip(2); }              // dmixmod
        if ((acmod & 0x1) && acmod > 0x2) { reader.skip(6); } // ltrtcmixlev, lorocmixlev
        if (acmod & 0x4) { reader.skip(6); }              // ltrtsurmixlev, lorosurmixlev
        if (lfeon && reader.readBit()) { reader.skip(5); } // lfemixlevcode / lfemixlevcod
        if (strmtyp == 0x0) {
            if (reader.readBit()) { reader.skip(6); }     // pgmscle / pgmscl
            if (acmod == 0x0 && reader.readBit()) { reader.skip(6); } // pgmscl2
            if (reader.readBit()) { reader.skip(6); }     // extpgmscle / extpgmscl
            const unsigned mixdef = reader.read(2);
            if (mixdef == 0x1) {
                reader.skip(5);                           // premixcmpsel, drcsrc, premixcmpscl
            } else if (mixdef == 0x2) {
                reader.skip(12);                          // mixdata
            } else if (mixdef == 0x3) {
                // Table E2.6 and §E2.3.1.22: option 4 reserves a field of
                // mixdeflen + 2 whole bytes after mixdeflen. The individual
                // scale factors and speech data the syntax table lays out sit
                // inside it and are padded to the byte by mixdatafill, so the
                // decoder -- which does no program mixing -- skips the field
                // by its length instead of walking every optional member.
                const unsigned mixdeflen = reader.read(5);
                reader.skip((mixdeflen + 2) * 8);
            }
            if (acmod < 0x2) {
                if (reader.readBit()) { reader.skip(14); } // paninfoe / panmean, paninfo
                if (acmod == 0x0 && reader.readBit()) { reader.skip(14); } // paninfo2
            }
            if (reader.readBit()) {                       // frmmixcfginfoe
                if (numblkscod == 0x0) {
                    reader.skip(5);                       // blkmixcfginfo[0]
                } else {
                    for (unsigned blk = 0; blk < blocks; ++blk) {
                        if (reader.readBit()) { reader.skip(5); } // blkmixcfginfo[blk]
                    }
                }
            }
        }
    }

    if (reader.readBit()) {                               // infomdate
        reader.skip(5);                                   // bsmod, copyrightb, origbs
        if (acmod == 0x2) { reader.skip(4); }             // dsurmod, dheadphonmod
        if (acmod >= 0x6) { reader.skip(2); }             // dsurexmod
        if (reader.readBit()) { reader.skip(8); }         // audprodie / mixlevel, roomtyp, adconvtyp
        if (acmod == 0x0 && reader.readBit()) { reader.skip(8); } // audprodi2e
        if (fscod < 0x3) { reader.skip(1); }              // sourcefscod
    }
    if (strmtyp == 0x0 && numblkscod != 0x3) {
        reader.skip(1);                                   // convsync
    }
    if (strmtyp == 0x2) {
        const bool blkid = numblkscod == 0x3 ? true : reader.readBit() != 0;
        if (blkid) { reader.skip(6); }                    // frmsizecod
    }
    if (reader.readBit()) {                               // addbsie
        const unsigned addbsil = reader.read(6);
        reader.skip((addbsil + 1) * 8);
    }

    // --- audfrm(), Table E1.3 ---
    const unsigned nfchans = header.channels;
    if (nfchans == 0 || nfchans > kMaxFullBandwidthChannels) {
        return fail("channel count out of range");
    }

    if (numblkscod == 0x3) {
        frame.expstre = reader.readBit() != 0;
        frame.ahte = reader.readBit() != 0;
    } else {
        frame.expstre = true;
        frame.ahte = false;
    }
    frame.snroffststr = static_cast<uint8_t>(reader.read(2));
    frame.transproce = reader.readBit() != 0;
    frame.blkswe = reader.readBit() != 0;
    frame.dithflage = reader.readBit() != 0;
    frame.bamode = reader.readBit() != 0;
    frame.frmfgaincode = reader.readBit() != 0;
    frame.dbaflde = reader.readBit() != 0;
    frame.skipflde = reader.readBit() != 0;
    frame.spxattene = reader.readBit() != 0;

    // Coupling strategy for every block, decided once for the frame.
    if (acmod > 0x1) {
        frame.cplstre[0] = true;
        frame.cplinu[0] = reader.readBit() != 0;
        for (unsigned blk = 1; blk < blocks; ++blk) {
            frame.cplstre[blk] = reader.readBit() != 0;
            frame.cplinu[blk] = frame.cplstre[blk] ? (reader.readBit() != 0)
                                                   : frame.cplinu[blk - 1];
        }
    }
    for (unsigned blk = 0; blk < blocks; ++blk) {
        frame.ncplblks += frame.cplinu[blk] ? 1u : 0u;
    }

    // Exponent strategies: sent per block, or as one Table E2.10 row per
    // channel for the whole (necessarily six-block) frame.
    if (frame.expstre) {
        for (unsigned blk = 0; blk < blocks; ++blk) {
            if (frame.cplinu[blk]) {
                frame.cplexpstr[blk] = static_cast<ExponentStrategy>(reader.read(2));
            }
            for (unsigned ch = 0; ch < nfchans; ++ch) {
                frame.chexpstr[blk][ch] = static_cast<ExponentStrategy>(reader.read(2));
            }
        }
    } else {
        if (acmod > 0x1 && frame.ncplblks > 0) {
            const unsigned code = reader.read(5);
            for (unsigned blk = 0; blk < kMaxEAC3Blocks; ++blk) {
                frame.cplexpstr[blk] = kFrameExponentStrategies[code][blk];
            }
        }
        for (unsigned ch = 0; ch < nfchans; ++ch) {
            const unsigned code = reader.read(5);
            for (unsigned blk = 0; blk < kMaxEAC3Blocks; ++blk) {
                frame.chexpstr[blk][ch] = kFrameExponentStrategies[code][blk];
            }
        }
    }
    if (lfeon) {
        for (unsigned blk = 0; blk < blocks; ++blk) {
            frame.lfeexpstr[blk] = reader.readBit() ? ExponentStrategy::D15
                                                    : ExponentStrategy::Reuse;
        }
    }

    // Converter exponent strategies exist only for re-encoding to AC-3.
    if (strmtyp == 0x0) {
        const bool convexpstre = numblkscod != 0x3 ? (reader.readBit() != 0) : true;
        if (convexpstre) {
            reader.skip(5 * nfchans);                     // convexpstr[ch]
        }
    }

    // AHT flags. §E3.4.2: a channel may use AHT only if its exponents are
    // sent exactly once in the frame -- the transform spans all six blocks,
    // so there is one set of exponents for it to use.
    if (frame.ahte) {
        unsigned ncplregs = 0;
        for (unsigned blk = 0; blk < kMaxEAC3Blocks; ++blk) {
            if (frame.cplstre[blk] || frame.cplexpstr[blk] != ExponentStrategy::Reuse) {
                ++ncplregs;
            }
        }
        frame.cplahtinu = (frame.ncplblks == 6 && ncplregs == 1) ? (reader.readBit() != 0) : false;
        for (unsigned ch = 0; ch < nfchans; ++ch) {
            unsigned nchregs = 0;
            for (unsigned blk = 0; blk < kMaxEAC3Blocks; ++blk) {
                if (frame.chexpstr[blk][ch] != ExponentStrategy::Reuse) {
                    ++nchregs;
                }
            }
            frame.chahtinu[ch] = nchregs == 1 ? (reader.readBit() != 0) : false;
        }
        if (lfeon) {
            unsigned nlferegs = 0;
            for (unsigned blk = 0; blk < kMaxEAC3Blocks; ++blk) {
                if (frame.lfeexpstr[blk] != ExponentStrategy::Reuse) {
                    ++nlferegs;
                }
            }
            frame.lfeahtinu = nlferegs == 1 ? (reader.readBit() != 0) : false;
        }
    }

    if (frame.snroffststr == 0x0) {
        frame.frmcsnroffst = static_cast<uint8_t>(reader.read(6));
        frame.frmfsnroffst = static_cast<uint8_t>(reader.read(4));
    }

    if (frame.transproce) {
        for (unsigned ch = 0; ch < nfchans; ++ch) {
            frame.chintransproc[ch] = reader.readBit() != 0;
            if (frame.chintransproc[ch]) {
                frame.transprocloc[ch] = static_cast<uint16_t>(reader.read(10));
                frame.transproclen[ch] = static_cast<uint8_t>(reader.read(8));
            }
        }
    }

    if (frame.spxattene) {
        for (unsigned ch = 0; ch < nfchans; ++ch) {
            frame.chinspxatten[ch] = reader.readBit() != 0;
            if (frame.chinspxatten[ch]) {
                frame.spxattencod[ch] = static_cast<uint8_t>(reader.read(5));
            }
        }
    }

    // Block start information only helps a decoder jump into the middle of a
    // frame; this one always reads blocks in order, so it is stepped over.
    const bool blkstrtinfoe = numblkscod != 0x0 ? (reader.readBit() != 0) : false;
    if (blkstrtinfoe) {
        const unsigned words_per_frame = frmsiz + 1;
        reader.skip((blocks - 1) * (4 + ceilLog2(words_per_frame)));
    }

    // The syntax-state flags audfrm ends by setting (firstspxcos,
    // firstcplcos, firstcplleak) belong to the block parser, which resets
    // them at the start of every frame.

    if (reader.overrun()) {
        return fail("ran past the end of the frame");
    }
    return true;
}

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3
