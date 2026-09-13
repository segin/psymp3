/*
 * AC3Transform.cpp - AC-3 inverse transform, windowing and overlap-add (A/52 §7.9)
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

namespace {

constexpr unsigned kN = kTransformSize;          // 512, per the spec's N
constexpr unsigned kLongPoints = kN / 4;         // 128-point complex IFFT
constexpr unsigned kShortPoints = kN / 8;        //  64-point, twice

/// The twiddles of §7.9.4.1 step 2 and §7.9.4.2 step 2.
///
/// xcos1/xsin1 for the long transform, xcos2/xsin2 for the short one. Built
/// once on first use rather than tabulated: they are pure cosines of known
/// angles, so a table would be a transcription risk for no gain.
struct Twiddles {
    float long_cos[kLongPoints], long_sin[kLongPoints];
    float short_cos[kShortPoints], short_sin[kShortPoints];
    /// Roots of unity for the IFFTs, biggest one first; the short transform
    /// uses the same table strided.
    float fft_cos[kLongPoints / 2], fft_sin[kLongPoints / 2];

    Twiddles()
    {
        const double pi = 3.14159265358979323846;
        for (unsigned k = 0; k < kLongPoints; ++k) {
            const double a = 2.0 * pi * (8.0 * k + 1.0) / (8.0 * kN);
            long_cos[k] = static_cast<float>(-std::cos(a));
            long_sin[k] = static_cast<float>(-std::sin(a));
        }
        for (unsigned k = 0; k < kShortPoints; ++k) {
            const double a = 2.0 * pi * (8.0 * k + 1.0) / (4.0 * kN);
            short_cos[k] = static_cast<float>(-std::cos(a));
            short_sin[k] = static_cast<float>(-std::sin(a));
        }
        for (unsigned k = 0; k < kLongPoints / 2; ++k) {
            const double a = 2.0 * pi * k / kLongPoints;
            fft_cos[k] = static_cast<float>(std::cos(a));
            fft_sin[k] = static_cast<float>(std::sin(a));
        }
    }
};

const Twiddles& twiddles()
{
    static const Twiddles t;
    return t;
}

/// In-place radix-2 decimation-in-time inverse FFT, no 1/N scaling.
///
/// The spec writes step 3 as a double loop, which is the definition of the
/// transform rather than a way to compute it: at 128 points that is 16384
/// complex multiplies per channel per block. This is the ordinary radix-2
/// factorisation of the same sum. `points` must be a power of two and at
/// most kLongPoints.
void inverseFft(float* re, float* im, unsigned points)
{
    // Bit reversal.
    for (unsigned i = 1, j = 0; i < points; ++i) {
        unsigned bit = points >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
    }

    const Twiddles& t = twiddles();
    for (unsigned len = 2; len <= points; len <<= 1) {
        // The root table is sized for kLongPoints, so a shorter transform
        // walks it with a stride instead of needing its own.
        const unsigned stride = kLongPoints / len;
        for (unsigned i = 0; i < points; i += len) {
            for (unsigned j = 0; j < len / 2; ++j) {
                // Positive sine: this is the inverse transform, e^{+i...}.
                const float wr = t.fft_cos[j * stride];
                const float wi = t.fft_sin[j * stride];
                const unsigned a = i + j;
                const unsigned b = a + len / 2;
                const float xr = re[b] * wr - im[b] * wi;
                const float xi = re[b] * wi + im[b] * wr;
                re[b] = re[a] - xr;
                im[b] = im[a] - xi;
                re[a] += xr;
                im[a] += xi;
            }
        }
    }
}

/// Steps 2 to 4 of §7.9.4.1 and §7.9.4.2, which are the same three operations
/// at two sizes: pre-twiddle into a complex sequence, inverse FFT it, then
/// post-twiddle by the same factors.
///
/// @param x       the transform coefficients, strided by `step` from `first`
/// @param points  N/4 for the long transform, N/8 for each short one
void twiddleFftTwiddle(const float* x, unsigned first, unsigned step,
                       unsigned points, const float* tcos, const float* tsin,
                       float* yr, float* yi)
{
    // Pre-IFFT: Z[k] = (X[2*points-2*k-1] + j * X[2*k]) * (cos[k] + j sin[k]).
    for (unsigned k = 0; k < points; ++k) {
        const float a = x[first + (2 * points - 2 * k - 1) * step];
        const float b = x[first + (2 * k) * step];
        yr[k] = a * tcos[k] - b * tsin[k];
        yi[k] = b * tcos[k] + a * tsin[k];
    }

    inverseFft(yr, yi, points);

    // Post-IFFT: y[n] = z[n] * (cos[n] + j sin[n]).
    for (unsigned n = 0; n < points; ++n) {
        const float zr = yr[n];
        const float zi = yi[n];
        yr[n] = zr * tcos[n] - zi * tsin[n];
        yi[n] = zi * tcos[n] + zr * tsin[n];
    }
}

} // namespace

void ac3InverseTransform(const float* coefficients, bool block_switch,
                         AC3TransformState& state, float* pcm)
{
    const Twiddles& t = twiddles();
    float windowed[kTransformSize];

    if (!block_switch) {
        // --- §7.9.4.1, one 512-sample transform ---
        float yr[kLongPoints], yi[kLongPoints];
        twiddleFftTwiddle(coefficients, 0, 1, kLongPoints,
                          t.long_cos, t.long_sin, yr, yi);

        // Step 5. The de-interleaving is the whole reason this is written out
        // rather than looped: the four quarters of the output each take a
        // different one of yr/yi, in a different direction, against a
        // different end of the window.
        constexpr unsigned e = kN / 8;   // 64
        for (unsigned n = 0; n < e; ++n) {
            windowed[2 * n]                 = -yi[e + n]         * kWindow[2 * n];
            windowed[2 * n + 1]             =  yr[e - n - 1]     * kWindow[2 * n + 1];
            windowed[kN / 4 + 2 * n]        = -yr[n]             * kWindow[kN / 4 + 2 * n];
            windowed[kN / 4 + 2 * n + 1]    =  yi[kN / 4 - n - 1] * kWindow[kN / 4 + 2 * n + 1];
            windowed[kN / 2 + 2 * n]        = -yr[e + n]         * kWindow[kN / 2 - 2 * n - 1];
            windowed[kN / 2 + 2 * n + 1]    =  yi[e - n - 1]     * kWindow[kN / 2 - 2 * n - 2];
            windowed[3 * kN / 4 + 2 * n]    =  yi[n]             * kWindow[kN / 4 - 2 * n - 1];
            windowed[3 * kN / 4 + 2 * n + 1] = -yr[kN / 4 - n - 1] * kWindow[kN / 4 - 2 * n - 2];
        }
    } else {
        // --- §7.9.4.2, two 256-sample transforms ---
        // The coefficients interleave the two halves: the even ones belong to
        // the first transform and the odd ones to the second.
        float yr1[kShortPoints], yi1[kShortPoints];
        float yr2[kShortPoints], yi2[kShortPoints];
        twiddleFftTwiddle(coefficients, 0, 2, kShortPoints,
                          t.short_cos, t.short_sin, yr1, yi1);
        twiddleFftTwiddle(coefficients, 1, 2, kShortPoints,
                          t.short_cos, t.short_sin, yr2, yi2);

        constexpr unsigned e = kN / 8;   // 64
        for (unsigned n = 0; n < e; ++n) {
            windowed[2 * n]                 = -yi1[n]            * kWindow[2 * n];
            windowed[2 * n + 1]             =  yr1[e - n - 1]    * kWindow[2 * n + 1];
            windowed[kN / 4 + 2 * n]        = -yr1[n]            * kWindow[kN / 4 + 2 * n];
            windowed[kN / 4 + 2 * n + 1]    =  yi1[e - n - 1]    * kWindow[kN / 4 + 2 * n + 1];
            windowed[kN / 2 + 2 * n]        = -yr2[n]            * kWindow[kN / 2 - 2 * n - 1];
            windowed[kN / 2 + 2 * n + 1]    =  yi2[e - n - 1]    * kWindow[kN / 2 - 2 * n - 2];
            windowed[3 * kN / 4 + 2 * n]    =  yi2[n]            * kWindow[kN / 4 - 2 * n - 1];
            windowed[3 * kN / 4 + 2 * n + 1] = -yr2[e - n - 1]   * kWindow[kN / 4 - 2 * n - 2];
        }
    }

    // --- Step 6: overlap and add ---
    // The factor of two undoes headroom the encoder left itself. The spec
    // asks for saturating arithmetic here because the reconstruction is the
    // original signal plus coding error and can exceed full scale even when
    // the input did not; clamping at this stage keeps a peak from wrapping
    // into a click further down the pipeline.
    for (unsigned n = 0; n < kBlockSamples; ++n) {
        const float sum = 2.0f * (windowed[n] + state.delay[n]);
        pcm[n] = std::max(-1.0f, std::min(1.0f, sum));
        state.delay[n] = windowed[kBlockSamples + n];
    }
}

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3
