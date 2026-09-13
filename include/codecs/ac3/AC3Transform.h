/*
 * AC3Transform.h - AC-3 inverse transform, windowing and overlap-add (A/52 §7.9)
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef PSYMP3_CODECS_AC3_AC3TRANSFORM_H
#define PSYMP3_CODECS_AC3_AC3TRANSFORM_H

#include <cstddef>

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

/// Coefficients, and so time samples, in one audio block.
constexpr unsigned kTransformSize = 512;
constexpr unsigned kBlockSamples = kTransformSize / 2;   // 256

/// The half-block each channel carries from one block into the next.
///
/// The MDCT is critically sampled: 256 coefficients stand for 512 time
/// samples, and a block on its own does not reconstruct anything. Each
/// inverse transform produces 512 windowed samples whose first half completes
/// the *previous* block and whose second half waits here for the next one.
/// That is why a channel cannot be decoded from a single block, and why a
/// seek has to discard the first block it lands on.
struct AC3TransformState {
    float delay[kBlockSamples] = {};
};

/// One block of one channel, from transform coefficients to PCM.
///
/// Implements the two transforms of A/52 §7.9.4 -- a single 512-point IMDCT
/// when the encoder saw no transient, or two 256-point ones when it did and
/// wanted the quantization noise kept inside a shorter window -- followed by
/// the windowing, de-interleaving and overlap-add of steps 5 and 6.
///
/// @param coefficients  256 transform coefficients for this block
/// @param block_switch  true for the two short transforms (blksw)
/// @param state         the channel's carry-over, updated in place
/// @param pcm           256 output samples, in [-1, 1) barring coding error
void ac3InverseTransform(const float* coefficients, bool block_switch,
                         AC3TransformState& state, float* pcm);

/// Steps 1 to 5 of §7.9.4.1 (or §7.9.4.2): the 512 windowed time samples one
/// block's coefficients produce, before any overlap-add.
///
/// E-AC-3 enhanced coupling (§E3.5.5.1) needs exactly this for a block and
/// both its neighbours, to rebuild a coupling signal free of time-domain
/// aliasing before it adjusts phase.
void ac3WindowedImdct(const float* coefficients, bool block_switch, float* windowed);

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3

#endif // PSYMP3_CODECS_AC3_AC3TRANSFORM_H
