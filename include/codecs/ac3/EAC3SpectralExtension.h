/*
 * EAC3SpectralExtension.h - E-AC-3 spectral extension, A/52 Annex E §E3.6
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef PSYMP3_CODECS_AC3_EAC3SPECTRALEXTENSION_H
#define PSYMP3_CODECS_AC3_EAC3SPECTRALEXTENSION_H

#include <cstdint>

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

/// Spectral extension sub-bands, Table E3.13: 17 of 12 bins from bin 25, plus
/// an end marker at bin 229 for spxendf's largest value.
constexpr unsigned kSpxSubbands = 17;

/// A/52 Table E3.13, spxbandtable[]: the first transform coefficient of each
/// spectral extension sub-band, 0..17. Every sub-band is twelve bins wide, so
/// this is a formula rather than a table, which leaves nothing to mistype.
constexpr unsigned eac3SpxBandStart(unsigned subband) { return 25 + 12 * subband; }

/// A/52 Table E2.11, defspxbndstrc[]: the band structure a frame starts with,
/// by spectral extension sub-band. As with coupling, a set entry merges the
/// sub-band into the band before it.
constexpr uint8_t kDefaultSpxBandStructure[kSpxSubbands] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
};

/// A/52 Table E3.14, spxattentab[][]: the notch filter applied around the
/// border between the coded baseband and the synthesised region.
///
/// Transcribed from the printed table and checked against the closed form it
/// follows, 2^(-(spxattencod + 1) * (binindex + 1) / 15), to within 5e-10 --
/// that is, exact to the nine decimals printed.
constexpr float kSpxAttenuation[32][3] = {
    { 0.954841604f, 0.911722489f, 0.870550563f },  //  0
    { 0.911722489f, 0.831237896f, 0.757858283f },  //  1
    { 0.870550563f, 0.757858283f, 0.659753955f },  //  2
    { 0.831237896f, 0.690956440f, 0.574349177f },  //  3
    { 0.793700526f, 0.629960525f, 0.500000000f },  //  4
    { 0.757858283f, 0.574349177f, 0.435275282f },  //  5
    { 0.723634619f, 0.523647061f, 0.378929142f },  //  6
    { 0.690956440f, 0.477420802f, 0.329876978f },  //  7
    { 0.659753955f, 0.435275282f, 0.287174589f },  //  8
    { 0.629960525f, 0.396850263f, 0.250000000f },  //  9
    { 0.601512518f, 0.361817309f, 0.217637641f },  // 10
    { 0.574349177f, 0.329876978f, 0.189464571f },  // 11
    { 0.548412490f, 0.300756259f, 0.164938489f },  // 12
    { 0.523647061f, 0.274206245f, 0.143587294f },  // 13
    { 0.500000000f, 0.250000000f, 0.125000000f },  // 14
    { 0.477420802f, 0.227930622f, 0.108818820f },  // 15
    { 0.455861244f, 0.207809474f, 0.094732285f },  // 16
    { 0.435275282f, 0.189464571f, 0.082469244f },  // 17
    { 0.415618948f, 0.172739110f, 0.071793647f },  // 18
    { 0.396850263f, 0.157490131f, 0.062500000f },  // 19
    { 0.378929142f, 0.143587294f, 0.054409410f },  // 20
    { 0.361817309f, 0.130911765f, 0.047366143f },  // 21
    { 0.345478220f, 0.119355200f, 0.041234622f },  // 22
    { 0.329876978f, 0.108818820f, 0.035896824f },  // 23
    { 0.314980262f, 0.099212566f, 0.031250000f },  // 24
    { 0.300756259f, 0.090454327f, 0.027204705f },  // 25
    { 0.287174589f, 0.082469244f, 0.023683071f },  // 26
    { 0.274206245f, 0.075189065f, 0.020617311f },  // 27
    { 0.261823531f, 0.068551561f, 0.017948412f },  // 28
    { 0.250000000f, 0.062500000f, 0.015625000f },  // 29
    { 0.238710401f, 0.056982656f, 0.013602353f },  // 30
    { 0.227930622f, 0.051952369f, 0.011841536f },  // 31
};

/// The sub-band range and banding one block's spectral extension covers.
struct EAC3SpxBands {
    unsigned start_subband = 0;   ///< spxstrtf: the first sub-band copied from
    unsigned begin_subband = 0;   ///< spx_begin_subbnd: the first synthesised
    unsigned end_subband = 0;     ///< spx_end_subbnd: one past the last synthesised
    unsigned count = 0;           ///< nspxbnds
    unsigned size[kSpxSubbands] = {}; ///< spxbndsztab[], in bins
};

/// Group the synthesised sub-bands into bands, §E3.6.2.
///
/// @param structure spxbndstrc[], indexed by absolute spectral extension
///                  sub-band (entries below begin_subband are ignored)
void eac3SpxComputeBands(unsigned start_subband, unsigned begin_subband, unsigned end_subband,
                         const uint8_t structure[kSpxSubbands], EAC3SpxBands& bands);

/// One spectral extension coordinate, §E3.6.3: a 4-bit exponent and a 2-bit
/// mantissa whose leading 1 is implied, scaled down a further 18 dB per step
/// of the channel's master coordinate.
float eac3SpxCoordinate(unsigned exponent, unsigned mantissa, unsigned master);

/// Everything one channel needs to synthesise its extension region.
struct EAC3SpxChannel {
    float coordinate[kSpxSubbands] = {};   ///< spxco[ch][bnd]
    float noise_blend[kSpxSubbands] = {};  ///< nblendfact[ch][bnd]
    float signal_blend[kSpxSubbands] = {}; ///< sblendfact[ch][bnd]
    bool attenuate = false;                ///< chinspxatten[ch]
    uint8_t attenuation_code = 0;          ///< spxattencod[ch]
};

/// Blending factors for a channel, §E3.6.4.2.1: how much of each band is
/// noise and how much the translated signal, from spxblnd and the band's
/// frequency. Only computed when new coordinates arrive.
void eac3SpxBlendFactors(unsigned spxblnd, const EAC3SpxBands& bands, EAC3SpxChannel& channel);

/// Zero-mean, unit-variance noise for §E3.6.4.2.4.
///
/// The standard requires that variance, not a sequence, so any generator
/// will do. A uniform distribution has unit variance over [-sqrt(3), sqrt(3)),
/// which a 16-bit LFSR reaches cheaply; it carries on across blocks and
/// frames rather than restarting, which would repeat audibly.
class EAC3SpxNoise {
public:
    float next();

private:
    uint16_t m_state = 0x6b3d;
};

/// Synthesise one channel's coefficients from spx_begin_subbnd to
/// spx_end_subbnd, §E3.6.4: translate low coefficients upward, measure each
/// band, notch the borders if asked, blend with scaled noise, and scale by
/// the coordinates.
///
/// @param coefficients all 256 of the channel's transform coefficients; the
///                     baseband below the extension region must already be
///                     final (decoupled and rematrixed), since it is copied
void eac3SpxSynthesise(float* coefficients, const EAC3SpxBands& bands,
                       const EAC3SpxChannel& channel, EAC3SpxNoise& noise);

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3

#endif // PSYMP3_CODECS_AC3_EAC3SPECTRALEXTENSION_H
