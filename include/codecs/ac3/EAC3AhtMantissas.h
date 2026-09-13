/*
 * EAC3AhtMantissas.h - E-AC-3 Adaptive Hybrid Transform mantissas, A/52 Annex E §E3.4
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef PSYMP3_CODECS_AC3_EAC3AHTMANTISSAS_H
#define PSYMP3_CODECS_AC3_EAC3AHTMANTISSAS_H

#include <cstdint>

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

/// A channel's coefficients for all six blocks of a frame, [block][bin].
///
/// The Adaptive Hybrid Transform codes a stationary channel across the whole
/// frame at once: each bin's six MDCT coefficients -- one per block -- are
/// run through a DCT, and the six DCT outputs are what the bitstream carries.
/// So all six blocks' worth arrives in the first block that reads the
/// channel's mantissas, and later blocks take theirs from here.
struct EAC3AhtSpectrum {
    float value[6][256] = {};
};

/// Read one AHT channel's mantissas -- Table E1.4 from gaqmod to the last
/// pre_chmant -- and produce its MDCT coefficients for all six blocks.
///
/// @param hebap      high-efficiency allocation pointers for bins [start, end)
/// @param exponents  the channel's exponents; AHT channels send one set per
///                   frame, so it applies to every block
bool eac3AhtReadChannel(AC3BitReader& reader, const uint8_t* hebap,
                        unsigned start, unsigned end, const uint8_t* exponents,
                        EAC3AhtSpectrum& spectrum, const char** reason = nullptr);

/// §E3.4.2: gain words in the stream for @p active_bins GAQ-coded bins.
unsigned eac3GaqSections(unsigned gaqmod, unsigned active_bins);

/// Read and dequantize one GAQ-coded mantissa (§E3.4.4.2, Tables E3.5/E3.6).
///
/// @param hebap  8..19
/// @param gain   Gk: 1, 2 or 4. With 1 there is no tag and a single
///               quantizer; with 2 or 4 a full-scale negative small codeword
///               is the tag announcing a large mantissa after it.
float eac3GaqDequantize(AC3BitReader& reader, unsigned hebap, unsigned gain);

/// §E3.4.5, the inverse DCT across the six blocks of one bin:
/// C(m) = 2 * sum_j R_j X(j) cos(j (2m + 1) pi / 12), R_0 = 1/2, R_j = 1.
void eac3AhtInverseDct(const float x[6], float c[6]);

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3

#endif // PSYMP3_CODECS_AC3_EAC3AHTMANTISSAS_H
