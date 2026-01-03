/*
 * SubframeDecoder.cpp - FLAC subframe decoding implementation
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

SubframeDecoder::SubframeDecoder(BitstreamReader *reader,
                                 ResidualDecoder *residual)
    : m_reader(reader), m_residual(residual) {}

SubframeDecoder::~SubframeDecoder() {}

bool SubframeDecoder::decodeSubframe(int32_t *output, uint32_t block_size,
                                     uint32_t bit_depth, bool is_side_channel) {
  if (!output || block_size == 0) {
    Debug::log("subframe_decoder",
               "Invalid parameters: output=%p, block_size=%u", output,
               block_size);
    return false;
  }

  // Parse subframe header
  SubframeHeader header;
  if (!parseSubframeHeader(header, bit_depth, is_side_channel)) {
    Debug::log("subframe_decoder", "Failed to parse subframe header");
    return false;
  }

  // Decode based on subframe type
  bool success = false;
  switch (header.type) {
  case SubframeType::CONSTANT:
    success = decodeConstant(output, block_size, header);
    break;

  case SubframeType::VERBATIM:
    success = decodeVerbatim(output, block_size, header);
    break;

  case SubframeType::FIXED:
    success = decodeFixed(output, block_size, header);
    break;

  case SubframeType::LPC:
    success = decodeLPC(output, block_size, header);
    break;

  case SubframeType::RESERVED:
  default:
    Debug::log("subframe_decoder", "Invalid subframe type");
    return false;
  }

  if (!success) {
    Debug::log("subframe_decoder", "Failed to decode subframe");
    return false;
  }

  // Apply wasted bits padding if present (Requirement 20)
  if (header.wasted_bits > 0) {
    for (uint32_t i = 0; i < block_size; i++) {
      output[i] <<= header.wasted_bits;
    }
  }

  return true;
}

bool SubframeDecoder::parseSubframeHeader(SubframeHeader &header,
                                          uint32_t frame_bit_depth,
                                          bool is_side_channel) {
  // Read mandatory 0 bit (Requirement 3)
  uint32_t zero_bit;
  if (!m_reader->readBits(zero_bit, 1)) {
    Debug::log("subframe_decoder", "Failed to read mandatory zero bit");
    return false;
  }

  if (zero_bit != 0) {
    Debug::log("subframe_decoder", "Mandatory zero bit is not zero: %u",
               zero_bit);
    return false;
  }

  // Read 6-bit subframe type (Requirement 3)
  uint32_t type_bits;
  if (!m_reader->readBits(type_bits, 6)) {
    Debug::log("subframe_decoder", "Failed to read subframe type");
    return false;
  }

  // Decode subframe type and order
  if (type_bits == 0) {
    // CONSTANT subframe
    header.type = SubframeType::CONSTANT;
    header.predictor_order = 0;
  } else if (type_bits == 1) {
    // VERBATIM subframe
    header.type = SubframeType::VERBATIM;
    header.predictor_order = 0;
  } else if ((type_bits & 0x38) == 0x08) {
    // FIXED predictor: 0b001xxx where xxx is order (0-4)
    header.type = SubframeType::FIXED;
    header.predictor_order = type_bits & 0x07;
    if (header.predictor_order > 4) {
      Debug::log("subframe_decoder", "Invalid FIXED predictor order: %u",
                 header.predictor_order);
      return false;
    }
  } else if ((type_bits & 0x20) == 0x20) {
    // LPC predictor: 0b1xxxxx where xxxxx is (order-1), so order is 1-32
    header.type = SubframeType::LPC;
    header.predictor_order = (type_bits & 0x1F) + 1;
    if (header.predictor_order < 1 || header.predictor_order > 32) {
      Debug::log("subframe_decoder", "Invalid LPC predictor order: %u",
                 header.predictor_order);
      return false;
    }
  } else {
    // Reserved subframe type
    Debug::log("subframe_decoder", "Reserved subframe type: 0x%02x", type_bits);
    header.type = SubframeType::RESERVED;
    return false;
  }

  // Read wasted bits flag (Requirement 20)
  uint32_t wasted_bits_flag;
  if (!m_reader->readBits(wasted_bits_flag, 1)) {
    Debug::log("subframe_decoder", "Failed to read wasted bits flag");
    return false;
  }

  header.wasted_bits = 0;
  if (wasted_bits_flag) {
    // Wasted bits present - decode unary-coded count
    // Count zeros until we hit a 1 bit
    if (!m_reader->readUnary(header.wasted_bits)) {
      Debug::log("subframe_decoder", "Failed to read wasted bits count");
      return false;
    }
    // Add 1 to get actual wasted bits count (k+1 encoding)
    header.wasted_bits++;
  }

  // Calculate subframe bit depth (Requirement 36)
  header.bit_depth = frame_bit_depth;

  // Subtract wasted bits
  if (header.wasted_bits >= header.bit_depth) {
    Debug::log("subframe_decoder", "Wasted bits (%u) >= bit depth (%u)",
               header.wasted_bits, header.bit_depth);
    return false;
  }
  header.bit_depth -= header.wasted_bits;

  // Add 1 bit for side channel (Requirement 36)
  if (is_side_channel) {
    header.bit_depth++;
  }

  // Validate bit depth is positive
  if (header.bit_depth == 0 || header.bit_depth > 33) {
    Debug::log("subframe_decoder", "Invalid subframe bit depth: %u",
               header.bit_depth);
    return false;
  }

  Debug::log("subframe_decoder",
             "Parsed subframe header: type=%d, order=%u, wasted=%u, bits=%u",
             static_cast<int>(header.type), header.predictor_order,
             header.wasted_bits, header.bit_depth);

  return true;
}

// Placeholder implementations for other methods (will be implemented in
// subsequent subtasks)

bool SubframeDecoder::decodeConstant(int32_t *output, uint32_t block_size,
                                     const SubframeHeader &header) {
  // Read single unencoded sample at subframe bit depth (Requirement 3)
  int32_t constant_value;
  if (!m_reader->readBitsSigned(constant_value, header.bit_depth)) {
    Debug::log("subframe_decoder", "Failed to read constant value");
    return false;
  }

  Debug::log("subframe_decoder", "CONSTANT subframe: value=%d, block_size=%u",
             constant_value, block_size);

  // Replicate constant value to all block samples (Requirement 3)
  for (uint32_t i = 0; i < block_size; i++) {
    output[i] = constant_value;
  }

  // Note: Wasted bits padding will be applied by caller

  return true;
}

bool SubframeDecoder::decodeVerbatim(int32_t *output, uint32_t block_size,
                                     const SubframeHeader &header) {
  // Read unencoded samples sequentially at subframe bit depth (Requirement 3)
  Debug::log("subframe_decoder",
             "VERBATIM subframe: block_size=%u, bit_depth=%u", block_size,
             header.bit_depth);

  for (uint32_t i = 0; i < block_size; i++) {
    int32_t sample;
    if (!m_reader->readBitsSigned(sample, header.bit_depth)) {
      Debug::log("subframe_decoder", "Failed to read verbatim sample %u", i);
      return false;
    }
    output[i] = sample;
  }

  // Note: Wasted bits padding will be applied by caller

  return true;
}

bool SubframeDecoder::decodeFixed(int32_t *output, uint32_t block_size,
                                  const SubframeHeader &header) {
  uint32_t order = header.predictor_order;

  Debug::log("subframe_decoder",
             "FIXED subframe: order=%u, block_size=%u, bit_depth=%u", order,
             block_size, header.bit_depth);

  // Validate predictor order (Requirement 4)
  if (order > 4) {
    Debug::log("subframe_decoder", "Invalid FIXED predictor order: %u", order);
    return false;
  }

  // Validate that we have enough samples for the predictor
  if (order > block_size) {
    Debug::log("subframe_decoder",
               "Predictor order (%u) exceeds block size (%u)", order,
               block_size);
    return false;
  }

  // Read warm-up samples (Requirement 35)
  // These are unencoded samples at subframe bit depth
  for (uint32_t i = 0; i < order; i++) {
    int32_t sample;
    if (!m_reader->readBitsSigned(sample, header.bit_depth)) {
      Debug::log("subframe_decoder", "Failed to read warm-up sample %u", i);
      return false;
    }
    output[i] = sample;
  }

  // Decode residuals (Requirement 4)
  uint32_t residual_count = block_size - order;
  if (residual_count > 0) {
    // TODO: This will be implemented when ResidualDecoder (Task 7) is complete
    // For now, we'll use a placeholder that will fail
    if (!m_residual) {
      Debug::log("subframe_decoder",
                 "ResidualDecoder not available (Task 7 not yet complete)");
      return false;
    }

    // Allocate temporary buffer for residuals
    int32_t *residuals = new int32_t[residual_count];

    // Decode residuals using ResidualDecoder
    bool success = m_residual->decodeResidual(residuals, block_size, order);

    if (success) {
      // Apply fixed predictor to reconstruct samples (Requirement 4, 54)
      applyFixedPredictor(output, residuals, residual_count, order);
    }

    delete[] residuals;

    if (!success) {
      Debug::log("subframe_decoder", "Failed to decode residuals");
      return false;
    }
  }

  return true;
}

bool SubframeDecoder::decodeLPC(int32_t *output, uint32_t block_size,
                                const SubframeHeader &header) {
  uint32_t order = header.predictor_order;

  Debug::log("subframe_decoder",
             "LPC subframe: order=%u, block_size=%u, bit_depth=%u", order,
             block_size, header.bit_depth);

  // Validate predictor order (Requirement 5)
  if (order < 1 || order > 32) {
    Debug::log("subframe_decoder", "Invalid LPC predictor order: %u", order);
    return false;
  }

  // Validate that we have enough samples for the predictor
  if (order > block_size) {
    Debug::log("subframe_decoder",
               "Predictor order (%u) exceeds block size (%u)", order,
               block_size);
    return false;
  }

  // Read warm-up samples (Requirement 35)
  // These are unencoded samples at subframe bit depth
  for (uint32_t i = 0; i < order; i++) {
    int32_t sample;
    if (!m_reader->readBitsSigned(sample, header.bit_depth)) {
      Debug::log("subframe_decoder", "Failed to read LPC warm-up sample %u", i);
      return false;
    }
    output[i] = sample;
  }

  // Parse coefficient precision (Requirement 28, 51)
  // 4-bit field encodes (precision - 1)
  uint32_t precision_bits;
  if (!m_reader->readBits(precision_bits, 4)) {
    Debug::log("subframe_decoder", "Failed to read coefficient precision");
    return false;
  }

  // Check for forbidden value (Requirement 28)
  if (precision_bits == 0x0F) {
    Debug::log("subframe_decoder", "Forbidden coefficient precision: 0x0F");
    return false;
  }

  uint32_t coeff_precision = precision_bits + 1; // 1-15 bits

  // Parse prediction right shift (Requirement 5, 51)
  // 5-bit signed value
  int32_t shift;
  if (!m_reader->readBitsSigned(shift, 5)) {
    Debug::log("subframe_decoder", "Failed to read prediction shift");
    return false;
  }

  // Check for forbidden negative shift (Requirement 28, 51)
  if (shift < 0) {
    Debug::log("subframe_decoder", "Forbidden negative prediction shift: %d",
               shift);
    return false;
  }

  Debug::log("subframe_decoder", "LPC parameters: precision=%u, shift=%d",
             coeff_precision, shift);

  // Read predictor coefficients (Requirement 5, 51, 52)
  // Coefficients are read in reverse chronological order
  int32_t *coeffs = new int32_t[order];
  for (uint32_t i = 0; i < order; i++) {
    if (!m_reader->readBitsSigned(coeffs[i], coeff_precision)) {
      Debug::log("subframe_decoder", "Failed to read LPC coefficient %u", i);
      delete[] coeffs;
      return false;
    }
  }

  // Decode residuals (Requirement 5)
  uint32_t residual_count = block_size - order;
  if (residual_count > 0) {
    // TODO: This will be implemented when ResidualDecoder (Task 7) is complete
    // For now, we'll use a placeholder that will fail
    if (!m_residual) {
      Debug::log("subframe_decoder",
                 "ResidualDecoder not available (Task 7 not yet complete)");
      delete[] coeffs;
      return false;
    }

    // Allocate temporary buffer for residuals
    int32_t *residuals = new int32_t[residual_count];

    // Decode residuals using ResidualDecoder
    bool success = m_residual->decodeResidual(residuals, block_size, order);

    if (success) {
      // Apply LPC predictor to reconstruct samples (Requirement 5, 51, 52, 54)
      applyLPCPredictor(output, residuals, coeffs, residual_count, order,
                        shift);
    }

    delete[] residuals;
    delete[] coeffs;

    if (!success) {
      Debug::log("subframe_decoder", "Failed to decode residuals");
      return false;
    }
  } else {
    delete[] coeffs;
  }

  return true;
}

void SubframeDecoder::applyFixedPredictor(int32_t *samples,
                                          const int32_t *residuals,
                                          uint32_t count, uint32_t order) {
  // Apply FIXED predictor formulas (Requirement 4)
  // Samples buffer already contains warm-up samples at positions [0..order-1]
  // We need to reconstruct samples[order..order+count-1] using
  // residuals[0..count-1]

  // Sequential decoding requirement (Requirement 54)
  for (uint32_t i = 0; i < count; i++) {
    int32_t prediction = 0;
    uint32_t sample_idx = order + i;

    switch (order) {
    case 0:
      // Order 0: s[i] = residual[i] (no prediction)
      prediction = 0;
      break;

    case 1:
      // Order 1: s[i] = residual[i] + s[i-1]
      prediction = samples[sample_idx - 1];
      break;

    case 2:
      // Order 2: s[i] = residual[i] + 2*s[i-1] - s[i-2]
      prediction = 2 * samples[sample_idx - 1] - samples[sample_idx - 2];
      break;

    case 3:
      // Order 3: s[i] = residual[i] + 3*s[i-1] - 3*s[i-2] + s[i-3]
      prediction = 3 * samples[sample_idx - 1] - 3 * samples[sample_idx - 2] +
                   samples[sample_idx - 3];
      break;

    case 4:
      // Order 4: s[i] = residual[i] + 4*s[i-1] - 6*s[i-2] + 4*s[i-3] - s[i-4]
      prediction = 4 * samples[sample_idx - 1] - 6 * samples[sample_idx - 2] +
                   4 * samples[sample_idx - 3] - samples[sample_idx - 4];
      break;

    default:
      // Should never happen - validated earlier
      prediction = 0;
      break;
    }

    // Reconstruct sample: s[i] = prediction + residual[i]
    samples[sample_idx] = prediction + residuals[i];
  }
}

void SubframeDecoder::applyLPCPredictor(int32_t *samples,
                                        const int32_t *residuals,
                                        const int32_t *coeffs, uint32_t count,
                                        uint32_t order, int32_t shift) {
  // Apply LPC predictor (Requirement 5, 51, 52)
  // Samples buffer already contains warm-up samples at positions [0..order-1]
  // We need to reconstruct samples[order..order+count-1] using
  // residuals[0..count-1]

  // Sequential decoding requirement (Requirement 54)
  for (uint32_t i = 0; i < count; i++) {
    uint32_t sample_idx = order + i;

    // Compute LPC prediction using 64-bit arithmetic to prevent overflow
    // (Requirement 5) prediction = sum(coeff[j] * sample[n-j-1]) >> shift
    int64_t sum = 0;

    // Apply coefficients in reverse chronological order (Requirement 52)
    // coeff[0] applies to most recent sample (sample[n-1])
    // coeff[1] applies to second most recent sample (sample[n-2])
    // etc.
    for (uint32_t j = 0; j < order; j++) {
      // Multiply coefficient with corresponding past sample
      sum += static_cast<int64_t>(coeffs[j]) *
             static_cast<int64_t>(samples[sample_idx - j - 1]);
    }

    // Apply quantization level shift (arithmetic right shift)
    int64_t prediction = sum >> shift;

    // Add residual to get reconstructed sample
    // Cast back to 32-bit (should be safe after shift)
    samples[sample_idx] = static_cast<int32_t>(prediction) + residuals[i];
  }
}

} // namespace FLAC
} // namespace Codec
} // namespace PsyMP3
