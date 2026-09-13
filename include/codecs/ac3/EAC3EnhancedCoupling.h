/*
 * EAC3EnhancedCoupling.h - E-AC-3 enhanced channel coupling, A/52 Annex E §E3.5
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef PSYMP3_CODECS_AC3_EAC3ENHANCEDCOUPLING_H
#define PSYMP3_CODECS_AC3_EAC3ENHANCEDCOUPLING_H

#include <cstdint>

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

/// Enhanced coupling sub-bands, Table E3.7: 22 of them from bin 13.
constexpr unsigned kEcplSubbands = 22;

/// Table E3.9, ecplsubbndtab[]: first bin of each sub-band, 0..22. Four of
/// six bins, then twelve-bin sub-bands from bin 37 -- a formula, so nothing
/// to mistype.
constexpr unsigned eac3EcplSubbandStart(unsigned sbnd)
{
    return sbnd < 4 ? 13 + 6 * sbnd : 37 + 12 * (sbnd - 4);
}

/// Table E2.13, defecplbndstrc[]: a set entry merges the sub-band into the
/// band before it. Sub-bands 0 to 8 always stand alone.
constexpr uint8_t kDefaultEcplBandStructure[kEcplSubbands] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1,
};

/// Table E3.10: amplitude = (mantissa / 32) >> exponent; code 31 is silence.
/// Checked against its intent, -1.5 dB per step, to within 0.15 dB.
constexpr uint8_t kEcplAmpExponent[32] = { 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 0 };
constexpr uint8_t kEcplAmpMantissa[32] = { 0x20, 0x1b, 0x17, 0x13, 0x10, 0x1b, 0x17, 0x13, 0x10, 0x1b, 0x17, 0x13, 0x10, 0x1b, 0x17, 0x13, 0x10, 0x1b, 0x17, 0x13, 0x10, 0x1b, 0x17, 0x13, 0x10, 0x1b, 0x17, 0x13, 0x10, 0x1b, 0x17, 0x00 };

/// Table E3.10 decoded.
float eac3EcplAmplitude(unsigned code);
/// Table E3.11: code / 32, wrapped into [-1, 1) (units of pi).
float eac3EcplAngle(unsigned code);
/// Table E3.12: -code / 7.
float eac3EcplChaos(unsigned code);

/// The band structure over [begin, end) sub-bands.
struct EAC3EcplBands {
    unsigned begin_subband = 0;
    unsigned end_subband = 0;
    unsigned count = 0;
    unsigned start_bin[kEcplSubbands] = {};
    unsigned bins[kEcplSubbands] = {};
};

/// Group sub-bands into bands (§E3.5.2, §E2.3.3.19). Entries of @p structure
/// below sub-band 9, and at the first sub-band, are ignored: those always
/// start a band.
void eac3EcplComputeBands(unsigned begin_subband, unsigned end_subband,
                          const uint8_t structure[kEcplSubbands], EAC3EcplBands& bands);

/// §E3.5.5.1 steps 3-5: the coupling channel's non-aliased complex spectrum
/// for the current block, from the 512 windowed IMDCT samples (§7.9.4.1 steps
/// 1-5) of the previous, current and next blocks. A block without enhanced
/// coupling contributes zeros.
void eac3EcplAnalyse(const float previous[512], const float current[512],
                     const float next[512], float zr[256], float zi[256]);

/// One coupled channel's parameters for one block, as decoded and reused.
struct EAC3EcplChannel {
    uint8_t amp[kEcplSubbands] = {};
    uint8_t angle[kEcplSubbands] = {};
    uint8_t chaos[kEcplSubbands] = {};
    bool transient = false;        ///< ecpltrans[ch]
    bool first = false;            ///< the first channel in coupling
};

/// Random angle offsets, §E3.5.5.3: a fixed value per channel and bin used
/// when there is no transient, and fresh per-band values each block when
/// there is. Both uniform over [-1, 1); the standard fixes the distribution,
/// not the sequence.
class EAC3EcplRandom {
public:
    EAC3EcplRandom();
    float fixed(unsigned ch, unsigned bin) const { return m_fixed[ch][bin]; }
    float fresh();

private:
    float m_fixed[5][256];
    uint32_t m_state = 0x2545f491u;
};

/// §E3.5.5.2-§E3.5.5.4: regenerate channel @p ch's transform coefficients over
/// the enhanced coupling range from the coupling spectrum.
void eac3EcplRegenerate(const float zr[256], const float zi[256], const EAC3EcplBands& bands,
                        const EAC3EcplChannel& channel, unsigned ch, bool angle_interpolation,
                        EAC3EcplRandom& random, float* coefficients);

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3

#endif // PSYMP3_CODECS_AC3_EAC3ENHANCEDCOUPLING_H
