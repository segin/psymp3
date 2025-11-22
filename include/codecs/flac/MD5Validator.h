/*
 * MD5Validator.h - MD5 checksum validation for FLAC decoded audio
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

#ifndef MD5VALIDATOR_H
#define MD5VALIDATOR_H

#include <cstdint>
#include <vector>

// Forward declaration for OpenSSL EVP context
struct evp_md_ctx_st;
typedef struct evp_md_ctx_st EVP_MD_CTX;

namespace PsyMP3 {
namespace Codec {
namespace FLAC {

/**
 * @brief MD5 checksum validator for FLAC decoded audio
 * 
 * Computes MD5 checksum of decoded audio samples according to RFC 9639
 * specification. The checksum is computed over all decoded samples with
 * channels interleaved on a per-sample basis, using signed little-endian
 * representation.
 * 
 * RFC 9639 MD5 COMPUTATION RULES:
 * ===============================
 * 1. Interleave all channels on per-sample basis (not per-frame)
 * 2. Use signed little-endian byte order for samples
 * 3. For non-byte-aligned bit depths, sign-extend to next byte boundary
 * 4. Include all samples from all frames in the stream
 * 5. Compare final MD5 with STREAMINFO MD5 checksum
 * 
 * USAGE PATTERN:
 * =============
 * MD5Validator validator;
 * validator.reset();
 * 
 * // For each decoded frame:
 * validator.update(decoded_samples, sample_count, channel_count, bit_depth);
 * 
 * // At end of stream:
 * uint8_t computed_md5[16];
 * validator.finalize(computed_md5);
 * bool valid = validator.compare(streaminfo_md5);
 * 
 * @thread_safety This class is NOT thread-safe. External synchronization
 *                required if used from multiple threads.
 * 
 * @see RFC 9639 Section 11 (MD5 checksum)
 * @see Requirements 25 (MD5 Checksum Validation)
 */
class MD5Validator {
public:
    MD5Validator();
    ~MD5Validator();
    
    /**
     * @brief Reset MD5 computation state
     * 
     * Initializes MD5 context for new stream. Must be called before
     * first update() call.
     * 
     * @return true if reset successful, false on error
     */
    bool reset();
    
    /**
     * @brief Update MD5 with decoded samples from a frame
     * 
     * Processes decoded samples and updates running MD5 checksum.
     * Samples must be in 32-bit signed integer format (pre-conversion).
     * 
     * @param samples Array of decoded samples (channel-interleaved)
     * @param sample_count Number of samples per channel
     * @param channel_count Number of channels
     * @param bit_depth Bits per sample (4-32)
     * @return true if update successful, false on error
     * 
     * Requirements: 25.1, 25.2, 25.3, 25.4
     */
    bool update(const int32_t* const* samples, 
                uint32_t sample_count,
                uint32_t channel_count,
                uint32_t bit_depth);
    
    /**
     * @brief Finalize MD5 computation and get result
     * 
     * Completes MD5 computation and stores result in output buffer.
     * After calling this, reset() must be called before new computation.
     * 
     * @param md5_out Output buffer for 16-byte MD5 checksum
     * @return true if finalization successful, false on error
     * 
     * Requirements: 25.5
     */
    bool finalize(uint8_t md5_out[16]);
    
    /**
     * @brief Compare computed MD5 with expected MD5
     * 
     * Compares the finalized MD5 checksum with expected value from
     * STREAMINFO metadata block.
     * 
     * @param expected_md5 Expected MD5 checksum (16 bytes)
     * @return true if checksums match, false if mismatch
     * 
     * Requirements: 25.5, 25.6
     */
    bool compare(const uint8_t expected_md5[16]) const;
    
    /**
     * @brief Check if MD5 checksum is zero (validation disabled)
     * 
     * Per RFC 9639, if STREAMINFO MD5 is all zeros, validation should
     * be skipped as the encoder did not compute the checksum.
     * 
     * @param md5 MD5 checksum to check (16 bytes)
     * @return true if MD5 is all zeros, false otherwise
     * 
     * Requirements: 25.7
     */
    static bool isZeroMD5(const uint8_t md5[16]);
    
    /**
     * @brief Get computed MD5 checksum
     * 
     * Returns the last finalized MD5 checksum. Only valid after
     * finalize() has been called.
     * 
     * @param md5_out Output buffer for 16-byte MD5 checksum
     */
    void getMD5(uint8_t md5_out[16]) const;
    
private:
    EVP_MD_CTX* m_ctx;                    // OpenSSL MD5 context
    uint8_t m_computed_md5[16];           // Last computed MD5
    bool m_finalized;                     // True if finalize() called
    
    // Sample conversion buffer for MD5 input
    std::vector<uint8_t> m_conversion_buffer;
    
    /**
     * @brief Convert samples to MD5 input format
     * 
     * Converts 32-bit signed samples to little-endian byte representation
     * with proper sign extension for non-byte-aligned bit depths.
     * 
     * @param samples Array of decoded samples (channel-interleaved)
     * @param sample_count Number of samples per channel
     * @param channel_count Number of channels
     * @param bit_depth Bits per sample (4-32)
     * @param output Output buffer for converted bytes
     * @return Number of bytes written to output
     * 
     * Requirements: 25.3, 25.4
     */
    size_t convertSamplesToBytes(const int32_t* const* samples,
                                 uint32_t sample_count,
                                 uint32_t channel_count,
                                 uint32_t bit_depth,
                                 uint8_t* output);
    
    /**
     * @brief Write sample in little-endian format
     * 
     * Writes a single sample value to output buffer in little-endian
     * byte order with proper byte count for bit depth.
     * 
     * @param sample Sample value (signed 32-bit)
     * @param bit_depth Bits per sample (4-32)
     * @param output Output buffer
     * @return Number of bytes written
     */
    size_t writeSampleLittleEndian(int32_t sample, 
                                   uint32_t bit_depth,
                                   uint8_t* output);
    
    // Prevent copying
    MD5Validator(const MD5Validator&) = delete;
    MD5Validator& operator=(const MD5Validator&) = delete;
};

} // namespace FLAC
} // namespace Codec
} // namespace PsyMP3

#endif // MD5VALIDATOR_H
