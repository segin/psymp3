/*
 * EAC3Frame.h - E-AC-3 bsi() and audfrm(), A/52 Annex E Tables E1.2 and E1.3
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef PSYMP3_CODECS_AC3_EAC3FRAME_H
#define PSYMP3_CODECS_AC3_EAC3FRAME_H

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

/// Longest E-AC-3 syncframe in audio blocks, Table E2.4.
constexpr unsigned kMaxEAC3Blocks = 6;

/// The frame-level layer E-AC-3 adds above the audio blocks.
///
/// AC-3 repeats every strategy decision in each block. E-AC-3 hoists most of
/// them into audfrm(), once per syncframe: which optional syntax the blocks
/// carry at all, the coupling and exponent strategies for all six blocks,
/// and whether the Adaptive Hybrid Transform, transient pre-noise processing
/// or spectral extension attenuation are in play. The audio blocks that
/// follow read these rather than their own fields, so a block cannot be
/// parsed without the frame it belongs to.
struct EAC3AudioFrame {
    // --- bsi, beyond what AC3FrameHeader keeps ---
    uint8_t substreamid = 0;
    bool compre = false;      ///< a compression gain word is present
    uint8_t compr = 0;
    bool chanmape = false;    ///< dependent substream supplies a channel map
    uint16_t chanmap = 0;     ///< Table E2.5 locations, MSB first
    uint8_t blocks = 6;       ///< audio blocks in the frame

    // --- audfrm: which optional block syntax is present ---
    bool expstre = true;      ///< per-block exponent strategies (else Table E2.10)
    bool ahte = false;        ///< Adaptive Hybrid Transform may be used
    uint8_t snroffststr = 0;  ///< 0 frame-wide, 1 per block one value, 2 per block per channel
    bool transproce = false;  ///< transient pre-noise processing data present
    bool blkswe = false;      ///< blksw[] present in blocks (else all 0)
    bool dithflage = false;   ///< dithflag[] present in blocks (else all 1)
    bool bamode = false;      ///< bit allocation parameters present (else defaults)
    bool frmfgaincode = false;///< fast gain codes may be sent per block
    bool dbaflde = false;     ///< delta bit allocation syntax present
    bool skipflde = false;    ///< skip field syntax present
    bool spxattene = false;   ///< spectral extension attenuation present

    // --- audfrm: coupling ---
    bool cplstre[kMaxEAC3Blocks] = {};
    bool cplinu[kMaxEAC3Blocks] = {};
    unsigned ncplblks = 0;

    // --- audfrm: exponent strategies, resolved to one per block ---
    ExponentStrategy cplexpstr[kMaxEAC3Blocks] = {};
    ExponentStrategy chexpstr[kMaxEAC3Blocks][kMaxFullBandwidthChannels] = {};
    ExponentStrategy lfeexpstr[kMaxEAC3Blocks] = {};

    // --- audfrm: AHT ---
    bool cplahtinu = false;
    bool chahtinu[kMaxFullBandwidthChannels] = {};
    bool lfeahtinu = false;

    // --- audfrm: frame SNR offsets (snroffststr == 0) ---
    uint8_t frmcsnroffst = 0;
    uint8_t frmfsnroffst = 0;

    // --- audfrm: transient pre-noise processing, §E3.7 ---
    bool chintransproc[kMaxFullBandwidthChannels] = {};
    uint16_t transprocloc[kMaxFullBandwidthChannels] = {};
    uint8_t transproclen[kMaxFullBandwidthChannels] = {};

    // --- audfrm: spectral extension attenuation ---
    bool chinspxatten[kMaxFullBandwidthChannels] = {};
    uint8_t spxattencod[kMaxFullBandwidthChannels] = {};
};

/// Parse a whole E-AC-3 bsi() and audfrm(), leaving @p reader at the first
/// audio block.
///
/// @p reader must be positioned at the sync word. @p header receives the
/// fields AC3FrameHeader describes, @p frame everything the blocks need.
/// Returns false, with @p reason set, for a frame that is malformed or uses
/// syntax this decoder does not follow (for instance mixdef 3's variable
/// mixing data, which it skips by length but cannot validate).
bool eac3ParseFrame(AC3BitReader& reader, AC3FrameHeader& header,
                    EAC3AudioFrame& frame, const char** reason = nullptr);

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3

#endif // PSYMP3_CODECS_AC3_EAC3FRAME_H
