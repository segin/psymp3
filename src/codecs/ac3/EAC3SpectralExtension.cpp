/*
 * EAC3SpectralExtension.cpp - E-AC-3 spectral extension, A/52 Annex E §E3.6
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

void eac3SpxComputeBands(unsigned start_subband, unsigned begin_subband, unsigned end_subband,
                         const uint8_t structure[kSpxSubbands], EAC3SpxBands& bands)
{
    bands = EAC3SpxBands();
    bands.start_subband = start_subband;
    bands.begin_subband = begin_subband;
    bands.end_subband = end_subband;
    if (end_subband <= begin_subband) {
        return;
    }
    // §E3.6.2: the first band always starts at the first synthesised
    // sub-band; each later sub-band either opens a band or joins the last.
    bands.count = 1;
    bands.size[0] = 12;
    for (unsigned sbnd = begin_subband + 1; sbnd < end_subband && sbnd < kSpxSubbands; ++sbnd) {
        if (structure[sbnd] == 0) {
            bands.size[bands.count++] = 12;
        } else {
            bands.size[bands.count - 1] += 12;
        }
    }
}

float eac3SpxCoordinate(unsigned exponent, unsigned mantissa, unsigned master)
{
    // §E3.6.3. Below the largest exponent the mantissa is known to lie in
    // [0.5, 1), so its leading 1 is not sent and the two bits that are sent
    // follow it; at exponent 15 the mantissa is sent as it is.
    const float value = exponent == 15 ? static_cast<float>(mantissa) / 4.0f
                                       : static_cast<float>(mantissa + 4) / 8.0f;
    return std::ldexp(value, -static_cast<int>(exponent + 3 * master));
}

void eac3SpxBlendFactors(unsigned spxblnd, const EAC3SpxBands& bands, EAC3SpxChannel& channel)
{
    // §E3.6.4.2.1: the noise share grows with the band's frequency, offset
    // down by the transmitted blend, so higher bands are noisier.
    const double noise_offset = spxblnd / 32.0;
    const double end = static_cast<double>(eac3SpxBandStart(bands.end_subband));
    double mantissa = static_cast<double>(eac3SpxBandStart(bands.begin_subband));
    for (unsigned bnd = 0; bnd < bands.count; ++bnd) {
        const double size = bands.size[bnd];
        double ratio = ((mantissa + 0.5 * size) / end) - noise_offset;
        ratio = std::max(0.0, std::min(1.0, ratio));
        channel.noise_blend[bnd] = static_cast<float>(std::sqrt(ratio));
        channel.signal_blend[bnd] = static_cast<float>(std::sqrt(1.0 - ratio));
        mantissa += size;
    }
}

float EAC3SpxNoise::next()
{
    // x^16 + x^14 + x^13 + x^11 + 1, maximal length, Galois form.
    const bool bit = (m_state & 1u) != 0;
    m_state >>= 1;
    if (bit) {
        m_state ^= 0xb400u;
    }
    // Uniform over [-1, 1), then widened to unit variance.
    const float uniform = static_cast<float>(m_state) / 32768.0f - 1.0f;
    return uniform * 1.7320508f;
}

void eac3SpxSynthesise(float* tc, const EAC3SpxBands& bands,
                       const EAC3SpxChannel& channel, EAC3SpxNoise& noise)
{
    if (bands.count == 0) {
        return;
    }
    const unsigned copy_start = eac3SpxBandStart(bands.start_subband);
    const unsigned copy_end = eac3SpxBandStart(bands.begin_subband);
    if (copy_end <= copy_start || eac3SpxBandStart(bands.end_subband) > kBlockSamples) {
        return;
    }

    // --- §E3.6.4.1: translation ---
    // Copy the baseband upward band by band. A band that would run past the
    // end of the copy region starts again from its beginning instead, and is
    // marked so its border gets the notch filter.
    bool wrapped[kSpxSubbands] = {};
    unsigned copy = copy_start;
    unsigned insert = copy_end;
    for (unsigned bnd = 0; bnd < bands.count; ++bnd) {
        const unsigned size = bands.size[bnd];
        if (copy + size > copy_end) {
            copy = copy_start;
            wrapped[bnd] = true;
        }
        for (unsigned bin = 0; bin < size; ++bin) {
            if (copy == copy_end) {
                copy = copy_start;
            }
            tc[insert++] = tc[copy++];
        }
    }

    // --- §E3.6.4.2.2: banded RMS of what was translated ---
    float rms[kSpxSubbands] = {};
    unsigned bin_index = copy_end;
    for (unsigned bnd = 0; bnd < bands.count; ++bnd) {
        const unsigned size = bands.size[bnd];
        double accum = 0.0;
        for (unsigned bin = 0; bin < size; ++bin, ++bin_index) {
            accum += static_cast<double>(tc[bin_index]) * tc[bin_index];
        }
        rms[bnd] = static_cast<float>(std::sqrt(accum / size));
    }

    // --- §E3.6.4.2.3: notch the borders ---
    // After measuring, before blending. The filter spans five bins centred on
    // the first bin past each border, attenuating by the table's three values
    // and then the same two again in reverse. The printed pseudocode indexes
    // the table with a variable its loops never set; the text and the
    // symmetry it describes make the intent -- 0, 1, 2, 1, 0 -- plain.
    if (channel.attenuate) {
        const auto& atten = kSpxAttenuation[channel.attenuation_code & 31];
        auto notch = [&](unsigned centre) {
            static constexpr unsigned kTap[5] = { 0, 1, 2, 1, 0 };
            for (unsigned i = 0; i < 5; ++i) {
                tc[centre - 2 + i] *= atten[kTap[i]];
            }
        };
        notch(copy_end);
        unsigned band_start = copy_end + bands.size[0];
        for (unsigned bnd = 1; bnd < bands.count; ++bnd) {
            if (wrapped[bnd]) {
                notch(band_start);
            }
            band_start += bands.size[bnd];
        }
    }

    // --- §E3.6.4.2.4 and §E3.6.4.3: blend with noise, then scale ---
    // The noise is scaled to each band's measured energy so the blend keeps
    // it, and the result is scaled by the coordinate to restore the original
    // spectral envelope. The factor of 32 is the standard's own.
    bin_index = copy_end;
    for (unsigned bnd = 0; bnd < bands.count; ++bnd) {
        const unsigned size = bands.size[bnd];
        const float noise_scale = rms[bnd] * channel.noise_blend[bnd];
        const float signal_scale = channel.signal_blend[bnd];
        const float gain = channel.coordinate[bnd] * 32.0f;
        for (unsigned bin = 0; bin < size; ++bin, ++bin_index) {
            const float blended = tc[bin_index] * signal_scale + noise.next() * noise_scale;
            tc[bin_index] = blended * gain;
        }
    }
}

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3
