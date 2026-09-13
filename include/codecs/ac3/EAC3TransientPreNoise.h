/*
 * EAC3TransientPreNoise.h - E-AC-3 transient pre-noise processing, A/52 Annex E §E3.7
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef PSYMP3_CODECS_AC3_EAC3TRANSIENTPRENOISE_H
#define PSYMP3_CODECS_AC3_EAC3TRANSIENTPRENOISE_H

#include <cstddef>
#include <vector>

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

/// Transient pre-noise processing for one full-bandwidth channel.
///
/// A transform codec smears quantization noise across its whole block, so a
/// sharp attack is preceded by a short hiss the original did not have. The
/// encoder marks where the transient is and how long a stretch of clean
/// earlier audio can stand in for the smeared part; the decoder copies that
/// earlier audio over the pre-noise and cross-fades at both ends (§E3.7.2).
///
/// The copy can reach back past the start of the current frame -- by up to
/// about 4 kB of samples -- so each channel keeps a history of what it last
/// output. The history is the channel's own decoded PCM, and the correction
/// is applied before the frame is handed on.
class EAC3TransientPreNoise {
public:
    /// TC1 and TC2 of §E3.7.2, the two cross-fade lengths.
    static constexpr unsigned kTC1 = 256;
    static constexpr unsigned kTC2 = 128;

    /// Apply the correction described by one frame's parameters, then
    /// remember the frame. @p pcm holds @p samples samples of this channel.
    ///
    /// @param active        chintransproc[ch]
    /// @param transprocloc  the transient, in units of four samples from the
    ///                      first sample of this frame
    /// @param transproclen  the time-scaling length, in samples
    void process(float* pcm, size_t samples, bool active,
                 unsigned transprocloc, unsigned transproclen);

    /// Forget the history, for a seek.
    void reset() { m_history.clear(); }

private:
    /// The channel's most recent output, oldest first, up to kHistory samples.
    std::vector<float> m_history;
    static constexpr size_t kHistory = 8192;
};

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3

#endif // PSYMP3_CODECS_AC3_EAC3TRANSIENTPRENOISE_H
