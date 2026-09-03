/*
 * SampleReconstructor.cpp - FLAC sample reconstruction implementation
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace Codec {
namespace FLAC {

namespace {
// Right-shift by `shift` bits, rounding half away from zero. Arithmetic >>
// floors toward negative infinity, which biases negative results one LSB too
// low if the half-LSB is simply subtracted; rounding the magnitude and
// reapplying the sign keeps rounding symmetric and lets exactly-representable
// values (multiples of 2^shift) round-trip unchanged.
inline int32_t roundedDownshift(int32_t sample, uint32_t shift) {
  const int64_t s = sample;
  const int64_t half = shift ? (1LL << (shift - 1)) : 0;
  const int64_t rounded = s >= 0 ? (s + half) >> shift
                                 : -(((-s) + half) >> shift);
  return static_cast<int32_t>(rounded);
}
} // namespace

SampleReconstructor::SampleReconstructor() {}

SampleReconstructor::~SampleReconstructor() {}

int32_t SampleReconstructor::upscale8To16(int32_t sample) {
  // Requirement 9.2: Left-shift by 8 bits to scale to 16-bit range.
  // Shift through uint32_t: left-shifting a negative signed value is UB, and
  // decoded samples are routinely negative. The bit pattern is identical.
  return static_cast<int32_t>(static_cast<uint32_t>(sample) << 8);
}

int32_t SampleReconstructor::downscale24To16(int32_t sample) {
  // Requirement 9.3: Right-shift by 8 bits, rounding half away from zero.
  return roundedDownshift(sample, 8);
}

int32_t SampleReconstructor::downscale32To16(int32_t sample) {
  // Requirement 9.4: Right-shift by 16 bits, rounding half away from zero.
  return roundedDownshift(sample, 16);
}

int32_t SampleReconstructor::downscale20To16(int32_t sample) {
  // Requirement 9.6: Right-shift by 4 bits, rounding half away from zero.
  return roundedDownshift(sample, 4);
}

int32_t SampleReconstructor::upscaleTo16(int32_t sample,
                                         uint32_t source_bit_depth) {
  // Requirement 9.5: Left-shift to 16-bit range with proper scaling
  // For bit depths 4-12, left-shift to fill 16-bit range
  uint32_t shift_amount = 16 - source_bit_depth;
  // Shift through uint32_t (negative-left-shift is UB; see upscale8To16).
  return static_cast<int32_t>(static_cast<uint32_t>(sample) << shift_amount);
}

int16_t SampleReconstructor::validateAndClip(int32_t sample) {
  // Requirement 10.5: Verify samples within 16-bit range
  // Requirement 57: Sample value range validation
  // Requirement 9.8: Prevent clipping during conversion and maintain audio
  // quality

  // Check if sample is within valid 16-bit signed range [-32768, 32767]
  if (sample > 32767) {
    // Clip to maximum 16-bit value
    return 32767;
  } else if (sample < -32768) {
    // Clip to minimum 16-bit value
    return -32768;
  }

  // Sample is within valid range
  return static_cast<int16_t>(sample);
}

AudioSample SampleReconstructor::convertToFullScale(int32_t sample,
                                                   uint32_t source_bit_depth) {
  // Align the source's most significant bit with bit 31 so every depth uses
  // the full int32 range. Computed in int64 and clamped: shifting a negative
  // value left is undefined behaviour, and a malformed stream can present a
  // sample wider than its declared depth.
  if (source_bit_depth >= 32) {
    return static_cast<AudioSample>(sample);
  }
  const int64_t scaled = static_cast<int64_t>(sample) *
                         (static_cast<int64_t>(1) << (32 - source_bit_depth));
  if (scaled > static_cast<int64_t>(std::numeric_limits<AudioSample>::max())) {
    return std::numeric_limits<AudioSample>::max();
  }
  if (scaled < static_cast<int64_t>(std::numeric_limits<AudioSample>::min())) {
    return std::numeric_limits<AudioSample>::min();
  }
  return static_cast<AudioSample>(scaled);
}

void SampleReconstructor::reconstructSamples(AudioSample *output,
                                             int32_t **channels,
                                             uint32_t block_size,
                                             uint32_t channel_count,
                                             uint32_t source_bit_depth) {
  // Requirements: 9, 10
  // Requirement 10.1: Produce interleaved PCM samples

  // Requirement 10.6: Validate sample count and parameters
  if (!output || !channels || block_size == 0 || channel_count == 0) {
    return;
  }

  // Validate bit depth is in supported range (4-32 bits)
  if (source_bit_depth < 4 || source_bit_depth > 32) {
    return;
  }

  // Validate all channel pointers are valid
  for (uint32_t i = 0; i < channel_count; ++i) {
    if (!channels[i]) {
      return;
    }
  }

  // Interleave channels: for each sample position, output all channels
  // Requirement 10.2: Interleave left and right channels for stereo
  // Requirement 10.3: Interleave all channels in order for multi-channel

  size_t output_index = 0;

  for (uint32_t sample_idx = 0; sample_idx < block_size; ++sample_idx) {
    for (uint32_t channel_idx = 0; channel_idx < channel_count; ++channel_idx) {
      // Get the sample from this channel
      int32_t sample = channels[channel_idx][sample_idx];

      // Scale the source depth UP to full-scale S32 rather than down to 16
      // bits. The old path discarded 8 bits of every 24-bit FLAC, which is
      // precisely the resolution a lossless format exists to preserve.
      output[output_index++] = convertToFullScale(sample, source_bit_depth);
    }
  }

  // Requirement 10.4: Output correct number of samples per frame
  // Total samples output = block_size * channel_count
}

} // namespace FLAC
} // namespace Codec
} // namespace PsyMP3
