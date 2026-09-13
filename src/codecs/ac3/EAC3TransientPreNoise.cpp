/*
 * EAC3TransientPreNoise.cpp - E-AC-3 transient pre-noise processing, A/52 Annex E §E3.7
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

/// §E3.7.2 asks for "nearly any pair of constant amplitude cross-fade
/// windows" and suggests Hanning. Constant amplitude means the pair sums to
/// one at every sample: the original and the synthesis buffer are the same
/// signal a few hundred samples apart, and mixing them must not swell. A
/// sine/cosine pair is constant in power instead, and bulges by up to 3 dB.
float fadeIn(int64_t n, int64_t length)
{
    const double x = (static_cast<double>(n) + 0.5) / static_cast<double>(length);
    return static_cast<float>(0.5 - 0.5 * std::cos(kA52Pi * x));
}

float fadeOut(int64_t n, int64_t length)
{
    return 1.0f - fadeIn(n, length);
}

} // namespace

EAC3TransientCorrection eac3TransientCorrection(int64_t frame_start, unsigned transprocloc,
                                                unsigned transproclen)
{
    // The location is sent at four-sample resolution. The pre-noise runs from
    // the start of the audio block before the one containing the transient
    // up to the transient itself. Blocks fall every 256 samples from the
    // frame start, and that earlier block may be in the previous frame.
    const int64_t offset = 4 * static_cast<int64_t>(transprocloc);
    const int64_t block = offset / static_cast<int64_t>(kSamplesPerBlock);
    EAC3TransientCorrection c;
    c.transloc = frame_start + offset;
    c.pnlen = offset - (block - 1) * static_cast<int64_t>(kSamplesPerBlock);
    c.translen = static_cast<int64_t>(transproclen);
    return c;
}

void eac3ApplyTransientCorrection(float* line, int64_t origin, const EAC3TransientCorrection& c)
{
    using C = EAC3TransientCorrection;
    const int64_t total = c.pnlen + c.translen + C::kTC1;
    const int64_t synth_len = 2 * C::kTC1 + c.pnlen;
    // PN is 256..511 by construction and transproclen at most 255, so the
    // overwrite never outruns the synthesis buffer. Checked anyway, since the
    // loops below would read past it.
    if (c.pnlen <= 0 || c.translen < 0 || total > synth_len || total < C::kTC1 + C::kTC2 ||
        synth_len > 2 * C::kTC1 + 2 * static_cast<int64_t>(kSamplesPerBlock)) {
        return;
    }

    // Copied out first: the buffer ends PN before the transient, and the
    // overwrite starts earlier than that.
    float synth[2 * C::kTC1 + 2 * kSamplesPerBlock];
    const float* from = line + (c.synthesisStart() - origin);
    std::copy(from, from + synth_len, synth);

    float* out = line + (c.start() - origin);
    // Fade from the original into the synthesis buffer ...
    for (int64_t i = 0; i < C::kTC1; ++i) {
        out[i] = out[i] * fadeOut(i, C::kTC1) + synth[i] * fadeIn(i, C::kTC1);
    }
    // ... overwrite the pre-noise with it ...
    for (int64_t i = C::kTC1; i < total - C::kTC2; ++i) {
        out[i] = synth[i];
    }
    // ... and fade back to the original just before the transient. The
    // printed loop indexes the TC2 windows with the running sample count;
    // they are TC2 long, so they are indexed from the start of this segment.
    for (int64_t i = total - C::kTC2; i < total; ++i) {
        const int64_t w = i - (total - C::kTC2);
        out[i] = out[i] * fadeIn(w, C::kTC2) + synth[i] * fadeOut(w, C::kTC2);
    }
}

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3
