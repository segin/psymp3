/*
 * SampleReconstructor.cpp - FLAC sample reconstruction implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace Codec {
namespace FLAC {

SampleReconstructor::SampleReconstructor() {}

SampleReconstructor::~SampleReconstructor() {}

int16_t SampleReconstructor::upscale8To16(int32_t sample) {
  // Requirement 9.2: Left-shift by 8 bits to scale to 16-bit range
  return static_cast<int16_t>(sample << 8);
}

int16_t SampleReconstructor::downscale24To16(int32_t sample) {
  // Requirement 9.3: Right-shift by 8 bits with rounding
  // Add 0.5 LSB (128) before shifting for proper rounding
  int32_t rounded = sample + 128;
  return static_cast<int16_t>(rounded >> 8);
}

int16_t SampleReconstructor::downscale32To16(int32_t sample) {
  // Requirement 9.4: Right-shift by 16 bits with rounding
  // Add 0.5 LSB (32768) before shifting for proper rounding
  int32_t rounded = sample + 32768;
  return static_cast<int16_t>(rounded >> 16);
}

int16_t SampleReconstructor::downscale20To16(int32_t sample) {
  // Requirement 9.6: Right-shift by 4 bits with rounding
  // Add 0.5 LSB (8) before shifting for proper rounding
  int32_t rounded = sample + 8;
  return static_cast<int16_t>(rounded >> 4);
}

int16_t SampleReconstructor::upscaleTo16(int32_t sample,
                                         uint32_t source_bit_depth) {
  // Requirement 9.5: Left-shift to 16-bit range with proper scaling
  // For bit depths 4-12, left-shift to fill 16-bit range
  uint32_t shift_amount = 16 - source_bit_depth;
  return static_cast<int16_t>(sample << shift_amount);
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

int16_t SampleReconstructor::convertTo16Bit(int32_t sample,
                                            uint32_t source_bit_depth) {
  // Requirement 9: Bit depth conversion

  switch (source_bit_depth) {
  case 16:
    // Requirement 9.1: 16-bit passthrough (no conversion)
    return static_cast<int16_t>(sample);

  case 8:
    // Requirement 9.2: 8-bit to 16-bit upscaling
    return upscale8To16(sample);

  case 24:
    // Requirement 9.3: 24-bit to 16-bit downscaling with rounding
    return downscale24To16(sample);

  case 32:
    // Requirement 9.4: 32-bit to 16-bit downscaling
    return downscale32To16(sample);

  case 20:
    // Requirement 9.6: 20-bit to 16-bit downscaling
    return downscale20To16(sample);

  case 4:
  case 5:
  case 6:
  case 7:
  case 9:
  case 10:
  case 11:
  case 12:
    // Requirement 9.5: 4-12 bit depths upscaling
    return upscaleTo16(sample, source_bit_depth);

  default:
    // For any other bit depth, attempt to scale appropriately
    if (source_bit_depth < 16) {
      return upscaleTo16(sample, source_bit_depth);
    } else {
      // Downscale by appropriate amount
      uint32_t shift_amount = source_bit_depth - 16;
      int32_t rounding = 1 << (shift_amount - 1);
      return static_cast<int16_t>((sample + rounding) >> shift_amount);
    }
  }
}

void SampleReconstructor::reconstructSamples(int16_t *output,
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

      // Convert to 16-bit with bit depth conversion
      // Requirement 9: Bit depth conversion
      int16_t converted = convertTo16Bit(sample, source_bit_depth);

      // Validate and clip to prevent overflow
      // Requirement 10.5: Ensure values are within valid 16-bit range
      // Requirement 57: Sample value range validation
      // Requirement 9.8: Prevent clipping during conversion
      converted = validateAndClip(converted);

      // Store interleaved sample
      output[output_index++] = converted;
    }
  }

  // Requirement 10.4: Output correct number of samples per frame
  // Total samples output = block_size * channel_count
}

} // namespace FLAC
} // namespace Codec
} // namespace PsyMP3
