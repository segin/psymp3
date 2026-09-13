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

#include <cstdint>

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

/// One transient pre-noise correction for one full-bandwidth channel.
///
/// A transform codec smears quantization noise across its whole block, so a
/// sharp attack is preceded by a short hiss the original did not have. The
/// encoder marks where the transient is and how long a stretch of clean
/// earlier audio can stand in for the smeared part; the decoder copies that
/// earlier audio over the pre-noise and cross-fades at both ends (§E3.7.2).
///
/// Positions are absolute sample indices on the channel's own timeline, not
/// offsets into a frame. §E3.7.1 lets a frame's parameters describe a
/// transient in the frame after it, and the copy reaches back as far as 1534
/// samples before the transient, well into earlier frames, so a frame is the
/// wrong unit to apply a correction in.
struct EAC3TransientCorrection {
    /// TC1 and TC2 of §E3.7.2, the two cross-fade lengths.
    static constexpr int64_t kTC1 = 256;
    static constexpr int64_t kTC2 = 128;
    /// The furthest before its transient a correction writes: PN +
    /// transproclen + TC1 at their largest, 511 + 255 + 256.
    static constexpr int64_t kMaxWriteReach = 1022;

    int64_t transloc = 0;   ///< the transient
    int64_t pnlen = 0;      ///< PN: from the start of the block before the transient's
    int64_t translen = 0;   ///< transproclen[ch], the time scaling length

    /// The first sample overwritten, PN + TC1 + transproclen before the transient.
    int64_t start() const { return transloc - (pnlen + translen + kTC1); }
    /// The first sample of the synthesis buffer, 2*TC1 + 2*PN before the transient.
    int64_t synthesisStart() const { return transloc - (2 * kTC1 + 2 * pnlen); }
};

/// Where one frame's transprocloc[ch] and transproclen[ch] put a correction.
///
/// @param frame_start  absolute position of the frame's first sample, which
///                     transprocloc -- in units of four samples -- counts from
EAC3TransientCorrection eac3TransientCorrection(int64_t frame_start, unsigned transprocloc,
                                                unsigned transproclen);

/// Apply @p correction to one channel's decoded PCM. @p line[0] is absolute
/// sample @p origin, and the line must hold every sample from
/// synthesisStart() up to the transient.
void eac3ApplyTransientCorrection(float* line, int64_t origin,
                                  const EAC3TransientCorrection& correction);

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3

#endif // PSYMP3_CODECS_AC3_EAC3TRANSIENTPRENOISE_H
