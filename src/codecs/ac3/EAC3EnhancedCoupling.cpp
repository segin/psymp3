/*
 * EAC3EnhancedCoupling.cpp - E-AC-3 enhanced channel coupling, A/52 Annex E §E3.5
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

constexpr double kPi = 3.14159265358979323846;
constexpr unsigned kN = 512;

/// Wrap an angle, in units of pi, into [-1, 1].
float wrapAngle(float y)
{
    while (y > 1.0f) {
        y -= 2.0f;
    }
    while (y < -1.0f) {
        y += 2.0f;
    }
    return y;
}

/// A 32-bit xorshift, uniform over [-1, 1).
float uniform(uint32_t& state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return static_cast<float>(state) / 2147483648.0f - 1.0f;
}

} // namespace

float eac3EcplAmplitude(unsigned code)
{
    if (code >= 31) {
        return 0.0f;
    }
    return std::ldexp(kEcplAmpMantissa[code] / 32.0f, -static_cast<int>(kEcplAmpExponent[code]));
}

float eac3EcplAngle(unsigned code)
{
    const int k = static_cast<int>(code & 63);
    return static_cast<float>(k < 32 ? k : k - 64) / 32.0f;
}

float eac3EcplChaos(unsigned code)
{
    return -static_cast<float>(code & 7) / 7.0f;
}

void eac3EcplComputeBands(unsigned begin_subband, unsigned end_subband,
                          const uint8_t structure[kEcplSubbands], EAC3EcplBands& bands)
{
    bands = EAC3EcplBands();
    bands.begin_subband = begin_subband;
    bands.end_subband = end_subband;
    for (unsigned sbnd = begin_subband; sbnd < end_subband && sbnd < kEcplSubbands; ++sbnd) {
        const bool joins = sbnd > begin_subband && sbnd >= 9 && structure[sbnd] != 0;
        const unsigned width = eac3EcplSubbandStart(sbnd + 1) - eac3EcplSubbandStart(sbnd);
        if (!joins) {
            bands.start_bin[bands.count] = eac3EcplSubbandStart(sbnd);
            bands.bins[bands.count] = 0;
            ++bands.count;
        }
        bands.bins[bands.count - 1] += width;
    }
}

void eac3EcplAnalyse(const float previous[512], const float current[512],
                     const float next[512], float zr[256], float zi[256])
{
    // Step 3: overlap the current block with its neighbours' halves, so the
    // time-domain aliasing of each cancels and the signal is continuous.
    //
    // The factor of two is not printed in this step, but it is the same one
    // §7.9.4.1 step 6 applies to every overlap-add "to undo headroom scaling
    // performed in the encoder", and without it the whole chain has a gain of
    // exactly one half: regenerating the reference channel of a consistent
    // signal at amplitude code 0 -- defined as 0 dB -- measured 0.500000, in
    // every bin, with a residual under 1e-7 once that was removed.
    float pcm[kN];
    for (unsigned n = 0; n < kN / 2; ++n) {
        pcm[n] = 2.0f * (previous[n + kN / 2] + current[n]);
        pcm[n + kN / 2] = 2.0f * (current[n + kN / 2] + next[n]);
    }

    // Step 4: window again and shift so the DFT that follows stacks oddly,
    // as the MDCT does.
    double real[kN], imag[kN];
    for (unsigned n = 0; n < kN / 2; ++n) {
        const double a = kPi * n / kN;
        const double b = kPi * (n + kN / 2) / kN;
        real[n] = pcm[n] * kWindow[n] * std::cos(a);
        imag[n] = pcm[n] * kWindow[n] * -std::sin(a);
        real[n + kN / 2] = pcm[n + kN / 2] * kWindow[kN / 2 - n - 1] * std::cos(b);
        imag[n + kN / 2] = pcm[n + kN / 2] * kWindow[kN / 2 - n - 1] * -std::sin(b);
    }

    // Step 5: the DFT, Z[k] = 1/N sum (re + j im)(cos - j sin). Only the
    // first half of the spectrum is used. Enhanced coupling is rare enough
    // that the direct sum, over a precomputed table, is the clearer choice.
    static double cos_table[kN], sin_table[kN];
    static bool ready = false;
    if (!ready) {
        for (unsigned i = 0; i < kN; ++i) {
            cos_table[i] = std::cos(2.0 * kPi * i / kN);
            sin_table[i] = std::sin(2.0 * kPi * i / kN);
        }
        ready = true;
    }
    for (unsigned k = 0; k < kN / 2; ++k) {
        double sr = 0.0, si = 0.0;
        for (unsigned n = 0; n < kN; ++n) {
            const double c = cos_table[(k * n) % kN];
            const double s = sin_table[(k * n) % kN];
            sr += real[n] * c + imag[n] * s;
            si += imag[n] * c - real[n] * s;
        }
        zr[k] = static_cast<float>(sr / kN);
        zi[k] = static_cast<float>(si / kN);
    }
}

EAC3EcplRandom::EAC3EcplRandom()
{
    // Fixed per channel and bin, generated once and reused for every block
    // of every frame, as §E3.5.5.3 requires.
    uint32_t seed = 0x9e3779b9u;
    for (unsigned ch = 0; ch < 5; ++ch) {
        for (unsigned bin = 0; bin < 256; ++bin) {
            m_fixed[ch][bin] = uniform(seed);
        }
    }
}

float EAC3EcplRandom::fresh()
{
    return uniform(m_state);
}

void eac3EcplRegenerate(const float zr[256], const float zi[256], const EAC3EcplBands& bands,
                        const EAC3EcplChannel& channel, unsigned ch, bool angle_interpolation,
                        EAC3EcplRandom& random, float* coefficients)
{
    if (bands.count == 0 || ch >= 5) {
        return;
    }
    const unsigned first_bin = bands.start_bin[0];
    const unsigned last_bin = bands.start_bin[bands.count - 1] + bands.bins[bands.count - 1];

    // §E3.5.5.2: band amplitudes and chaos, the chaos softening the amplitude
    // of every channel but the first when no transient is flagged.
    float amp_band[kEcplSubbands] = {}, chaos_band[kEcplSubbands] = {}, angle_band[kEcplSubbands] = {};
    for (unsigned bnd = 0; bnd < bands.count; ++bnd) {
        amp_band[bnd] = eac3EcplAmplitude(channel.amp[bnd]);
        chaos_band[bnd] = channel.first ? 0.0f : eac3EcplChaos(channel.chaos[bnd]);
        angle_band[bnd] = channel.first ? 0.0f : eac3EcplAngle(channel.angle[bnd]);
        if (!channel.transient && !channel.first) {
            amp_band[bnd] *= 1.0f + 0.38f * chaos_band[bnd];
        }
    }

    // Per-bin values, indexed relative to the start of the coupling range.
    const unsigned span = last_bin - first_bin;
    float amp[256] = {}, chaos[256] = {}, angle[256] = {}, noise[256] = {};
    {
        unsigned rel = 0;
        for (unsigned bnd = 0; bnd < bands.count; ++bnd) {
            const float band_noise = channel.transient ? random.fresh() : 0.0f;
            for (unsigned i = 0; i < bands.bins[bnd]; ++i, ++rel) {
                amp[rel] = amp_band[bnd];
                chaos[rel] = chaos_band[bnd];
                angle[rel] = angle_band[bnd];
                noise[rel] = channel.transient ? band_noise : random.fixed(ch, first_bin + rel);
            }
        }
    }

    // §E3.5.5.3: optionally interpolate the angles linearly between band
    // centres. The printed pseudocode starts its bin cursor at the range's
    // absolute first coefficient and then reassigns it band-relative values;
    // it is band-relative throughout, which is how it is written here. A
    // single band has no neighbour to interpolate towards.
    if (angle_interpolation && bands.count > 1) {
        unsigned bin = 0;
        float y = 0.0f, slope = 0.0f;
        unsigned nbins_curr = 0;
        for (unsigned bnd = 1; bnd < bands.count; ++bnd) {
            const unsigned nbins_prev = bands.bins[bnd - 1];
            nbins_curr = bands.bins[bnd];
            const float angle_prev = angle_band[bnd - 1];
            float angle_curr = angle_band[bnd];
            while ((angle_curr - angle_prev) > 1.0f) {
                angle_curr -= 2.0f;
            }
            while ((angle_prev - angle_curr) > 1.0f) {
                angle_curr += 2.0f;
            }
            slope = (angle_curr - angle_prev) / ((nbins_curr + nbins_prev) / 2.0f);

            if (bnd == 1 && nbins_prev > 1) {
                // The lower half of the first band, walking down from its centre.
                int down;
                if (nbins_prev % 2 == 0) {
                    y = angle_prev - slope / 2.0f;
                    down = static_cast<int>(nbins_prev / 2) - 1;
                } else {
                    y = angle_prev - slope;
                    down = static_cast<int>((nbins_prev - 3) / 2);
                }
                const unsigned count = static_cast<unsigned>(down + 1);
                for (unsigned j = 0; j < count; ++j) {
                    angle[down--] = wrapAngle(y);
                    y -= slope;
                }
                bin = count;
            }
            unsigned count;
            if (nbins_prev % 2 == 0) {
                y = angle_prev + slope / 2.0f;
                count = nbins_curr / 2 + nbins_prev / 2;
            } else {
                y = angle_prev;
                count = nbins_curr / 2 + (nbins_prev + 1) / 2;
            }
            for (unsigned j = 0; j < count && bin < span; ++j) {
                angle[bin++] = wrapAngle(y);
                y += slope;
            }
        }
        const unsigned count = nbins_curr % 2 == 0 ? nbins_curr / 2 : nbins_curr / 2 + 1;
        for (unsigned j = 0; j < count && bin < span; ++j) {
            angle[bin++] = wrapAngle(y);
            y += slope;
        }
    }

    // Decorrelate with the random offsets, scaled by chaos, and wrap.
    for (unsigned rel = 0; rel < span; ++rel) {
        float a = angle[rel] + chaos[rel] * noise[rel];
        if (a < -1.0f) {
            a += 2.0f;
        } else if (a >= 1.0f) {
            a -= 2.0f;
        }
        angle[rel] = a;
    }

    // §E3.5.5.4: rotate and scale the coupling spectrum, then fold it back to
    // MDCT coefficients.
    for (unsigned rel = 0; rel < span; ++rel) {
        const unsigned k = first_bin + rel;
        const double phase = kPi * angle[rel];
        const double c = amp[rel] * std::cos(phase), s = amp[rel] * std::sin(phase);
        const double chr = zr[k] * c - zi[k] * s;
        const double chi = zi[k] * c + zr[k] * s;
        const double yk = std::cos(2.0 * kPi * (kN / 4.0 + 0.5) / kN * (k + 0.5));
        const unsigned mirror = kN / 2 - 1 - k;
        const double ym = std::cos(2.0 * kPi * (kN / 4.0 + 0.5) / kN * (mirror + 0.5));
        coefficients[k] = static_cast<float>(-2.0 * (yk * chr + ym * chi));
    }
}

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3
