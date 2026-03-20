/*
 * FLACTypes.h - Shared data structures for FLAC codec
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef FLACTYPES_H
#define FLACTYPES_H

#include <cstdint>
#include <cstddef>
#include <climits>

/**
 * @brief FLAC frame information extracted during decoding
 * 
 * This structure contains the essential information about a FLAC frame
 * that's needed for proper decoding and output formatting. All fields
 * are validated against RFC 9639 FLAC specification requirements.
 */
struct FLACFrameInfo {
    uint32_t block_size = 0;         ///< Number of samples in this frame (16-65535 per RFC 9639)
    uint32_t sample_rate = 0;        ///< Sample rate for this frame (1-655350 Hz per RFC 9639)
    uint16_t channels = 0;           ///< Number of channels in this frame (1-8 per RFC 9639)
    uint16_t bits_per_sample = 0;    ///< Bits per sample in this frame (4-32 per RFC 9639)
    uint64_t sample_number = 0;      ///< Starting sample number for this frame
    uint8_t channel_assignment = 0;  ///< Channel assignment mode (independent, left-side, etc.)
    bool variable_block_size = false; ///< True if using variable block size strategy
    
    /**
     * @brief Check if frame information is valid per RFC 9639
     */
    bool isValid() const {
        return block_size >= 16 && block_size <= 65535 &&
               sample_rate >= 1 && sample_rate <= 655350 &&
               channels >= 1 && channels <= 8 && 
               bits_per_sample >= 4 && bits_per_sample <= 32;
    }
    
    /**
     * @brief Get frame duration in milliseconds
     */
    uint64_t getDurationMs() const {
        if (sample_rate == 0 || block_size == 0) return 0;
        return (static_cast<uint64_t>(block_size) * 1000ULL) / sample_rate;
    }
    
    /**
     * @brief Get expected output sample count for 16-bit conversion
     */
    size_t getOutputSampleCount() const {
        return static_cast<size_t>(block_size) * channels;
    }
    
    /**
     * @brief Get channel assignment type name for debugging
     */
    const char* getChannelAssignmentName() const {
        switch (channel_assignment) {
            case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
                return "independent";
            case 8: return "left-side";
            case 9: return "right-side";
            case 10: return "mid-side";
            default: return "reserved";
        }
    }
};

/**
 * @brief Audio quality metrics for validation and testing
 * 
 * This structure contains various audio quality measurements used
 * for validating codec accuracy and conversion quality.
 */
struct AudioQualityMetrics {
    double signal_to_noise_ratio_db = 0.0;     ///< SNR in decibels
    double total_harmonic_distortion = 0.0;    ///< THD as percentage
    double dynamic_range_db = 0.0;             ///< Dynamic range in decibels
    double peak_amplitude = 0.0;               ///< Peak sample amplitude (0.0-1.0)
    double rms_amplitude = 0.0;                ///< RMS amplitude (0.0-1.0)
    double dc_offset = 0.0;                    ///< DC offset as percentage
    size_t zero_crossings = 0;                 ///< Number of zero crossings
    size_t clipped_samples = 0;                ///< Number of clipped samples
    bool bit_perfect = false;                  ///< True if bit-perfect match with reference
    
    /**
     * @brief Check if quality metrics indicate good audio quality
     */
    bool isGoodQuality() const {
        return signal_to_noise_ratio_db > 90.0 &&  // >90dB SNR
               total_harmonic_distortion < 0.01 &&  // <1% THD
               dynamic_range_db > 80.0 &&           // >80dB dynamic range
               clipped_samples == 0;                // No clipping
    }
    
    /**
     * @brief Check if quality metrics indicate bit-perfect decoding
     */
    bool isBitPerfect() const {
        return bit_perfect && 
               signal_to_noise_ratio_db > 120.0 &&  // Theoretical maximum for 16-bit
               total_harmonic_distortion < 0.0001;  // Minimal distortion
    }
};

/**
 * @brief FLAC codec performance and debugging statistics
 * 
 * This structure tracks performance metrics and debugging information
 * for the FLAC codec to enable optimization and troubleshooting.
 */
struct FLACCodecStats {
    size_t frames_decoded = 0;        ///< Total number of FLAC frames decoded
    size_t samples_decoded = 0;       ///< Total number of audio samples decoded
    size_t total_bytes_processed = 0; ///< Total input bytes processed
    size_t conversion_operations = 0; ///< Number of bit depth conversions performed
    size_t error_count = 0;          ///< Number of decoding errors encountered
    double average_frame_size = 0.0; ///< Average frame size in bytes
    double decode_efficiency = 0.0;   ///< Samples decoded per second
    size_t memory_usage_bytes = 0;   ///< Current memory usage in bytes
    
    // Performance timing statistics
    uint64_t total_decode_time_us = 0;    ///< Total decoding time in microseconds
    uint64_t max_frame_decode_time_us = 0; ///< Maximum single frame decode time
    uint64_t min_frame_decode_time_us = UINT64_MAX; ///< Minimum single frame decode time
    
    // Error breakdown
    size_t crc_errors = 0;           ///< CRC validation failures
    size_t sync_errors = 0;          ///< Frame synchronization errors
    size_t memory_errors = 0;        ///< Memory allocation failures
    size_t libflac_errors = 0;       ///< libFLAC internal errors
    
    /**
     * @brief Calculate average decode time per frame in microseconds
     */
    double getAverageDecodeTimeUs() const {
        return frames_decoded > 0 ? static_cast<double>(total_decode_time_us) / frames_decoded : 0.0;
    }
    
    /**
     * @brief Calculate decode efficiency in samples per second
     */
    double getDecodeEfficiency() const {
        return total_decode_time_us > 0 ? 
               (static_cast<double>(samples_decoded) * 1000000.0) / total_decode_time_us : 0.0;
    }
    
    /**
     * @brief Get error rate as percentage
     */
    double getErrorRate() const {
        return frames_decoded > 0 ? 
               (static_cast<double>(error_count) * 100.0) / frames_decoded : 0.0;
    }
};

#endif // FLACTYPES_H
