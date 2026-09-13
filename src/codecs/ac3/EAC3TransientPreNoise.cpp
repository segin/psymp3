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

constexpr double kPi = 3.14159265358979323846;

/// A constant-amplitude pair: fade-in(n)^2 + fade-out(n)^2 == 1. §E3.7.2 asks
/// only for "nearly any pair of constant amplitude cross-fade windows" and
/// suggests Hanning; this is the power-complementary sine/cosine form of it.
float fadeIn(unsigned n, unsigned length)
{
    const double x = (static_cast<double>(n) + 0.5) / length;
    return static_cast<float>(std::sin(0.5 * kPi * x));
}

float fadeOut(unsigned n, unsigned length)
{
    const double x = (static_cast<double>(n) + 0.5) / length;
    return static_cast<float>(std::cos(0.5 * kPi * x));
}

} // namespace

void EAC3TransientPreNoise::process(float* pcm, size_t samples, bool active,
                                    unsigned transprocloc, unsigned transproclen)
{
    // Work on history + this frame as one timeline, so offsets that reach
    // before the frame start land in samples already output. Those earlier
    // samples can be read but not rewritten: they have already gone.
    const size_t history = m_history.size();
    std::vector<float> line(history + samples);
    std::copy(m_history.begin(), m_history.end(), line.begin());
    std::copy(pcm, pcm + samples, line.begin() + static_cast<std::ptrdiff_t>(history));

    if (active) {
        // §E3.7.2. The location is sent at four-sample resolution. The
        // pre-noise runs from the start of the audio block before the one
        // containing the transient up to the transient itself.
        const long transloc = static_cast<long>(history) + 4L * transprocloc;
        const long block = static_cast<long>(transprocloc) * 4 / kBlockSamples;
        const long prior_block_start = static_cast<long>(history) + (block - 1) * kBlockSamples;
        const long pnlen = transloc - prior_block_start;
        const long translen = static_cast<long>(transproclen);
        const long total = pnlen + translen + kTC1;

        // The synthesis buffer: 2*TC1 + PN samples taken from 2*TC1 + 2*PN
        // before the transient. (The printed pseudocode writes this as
        // 2*tc; TC1 is the only parameter it can mean.)
        const long synth_start = transloc - (2L * kTC1 + 2L * pnlen);
        const long synth_len = 2L * kTC1 + pnlen;
        const long start = transloc - total;

        // The corrected span may begin shortly before this frame. Those
        // samples are already gone, but if they all fall inside the opening
        // cross-fade -- where the synthesis buffer is only fading in -- the
        // rest of the correction can still be applied without a seam: the
        // fade simply starts part way through. Further back than that and the
        // overwrite itself would begin at the frame boundary, with a jump.
        const long written = static_cast<long>(history);
        const bool in_range = pnlen > 0 && translen >= 0 && synth_start >= 0 &&
                              start + static_cast<long>(kTC1) > written &&
                              start >= 0 &&
                              transloc <= static_cast<long>(line.size()) &&
                              total >= static_cast<long>(kTC1 + kTC2) &&
                              total <= synth_len;
        if (in_range) {
            std::vector<float> synth(line.begin() + synth_start,
                                     line.begin() + synth_start + synth_len);
            const long first = std::max(0L, written - start);
            // Fade from the original into the synthesis buffer ...
            for (long i = first; i < kTC1; ++i) {
                line[start + i] = line[start + i] * fadeOut(static_cast<unsigned>(i), kTC1)
                                + synth[i] * fadeIn(static_cast<unsigned>(i), kTC1);
            }
            // ... overwrite the pre-noise with it ...
            for (long i = kTC1; i < total - kTC2; ++i) {
                line[start + i] = synth[i];
            }
            // ... and fade back to the original just before the transient.
            // The printed loop indexes the TC2 windows with the running
            // sample count; they are TC2 long, so they are indexed from the
            // start of this last segment.
            for (long i = total - kTC2; i < total; ++i) {
                const unsigned w = static_cast<unsigned>(i - (total - kTC2));
                line[start + i] = line[start + i] * fadeIn(w, kTC2)
                                + synth[i] * fadeOut(w, kTC2);
            }
        }
    }

    std::copy(line.begin() + static_cast<std::ptrdiff_t>(history), line.end(), pcm);

    // Keep the most recent output for the next frame to reach back into.
    const size_t keep = std::min(kHistory, line.size());
    m_history.assign(line.end() - static_cast<std::ptrdiff_t>(keep), line.end());
}

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3
