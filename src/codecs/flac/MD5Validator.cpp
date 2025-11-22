/*
 * MD5Validator.cpp - MD5 checksum validation for FLAC decoded audio
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

#include "psymp3.h"

#ifdef HAVE_NATIVE_FLAC

namespace PsyMP3 {
namespace Codec {
namespace FLAC {

MD5Validator::MD5Validator()
    : m_ctx(nullptr)
    , m_finalized(false)
{
    // Initialize computed MD5 to zeros
    for (int i = 0; i < 16; i++) {
        m_computed_md5[i] = 0;
    }
    
    // Pre-allocate conversion buffer for typical frame size
    // Max: 65535 samples * 8 channels * 4 bytes = ~2MB
    m_conversion_buffer.reserve(65535 * 8 * 4);
}

MD5Validator::~MD5Validator()
{
    if (m_ctx) {
        EVP_MD_CTX_free(m_ctx);
        m_ctx = nullptr;
    }
}

bool MD5Validator::reset()
{
    // Free existing context if any
    if (m_ctx) {
        EVP_MD_CTX_free(m_ctx);
        m_ctx = nullptr;
    }
    
    // Create new MD5 context
    m_ctx = EVP_MD_CTX_new();
    if (!m_ctx) {
        Debug::log("MD5Validator", "Failed to create MD5 context");
        return false;
    }
    
    // Initialize MD5 computation
    if (!EVP_DigestInit_ex(m_ctx, EVP_md5(), nullptr)) {
        Debug::log("MD5Validator", "Failed to initialize MD5 digest");
        EVP_MD_CTX_free(m_ctx);
        m_ctx = nullptr;
        return false;
    }
    
    m_finalized = false;
    
    // Clear computed MD5
    for (int i = 0; i < 16; i++) {
        m_computed_md5[i] = 0;
    }
    
    return true;
}

bool MD5Validator::update(const int32_t* const* samples,
                          uint32_t sample_count,
                          uint32_t channel_count,
                          uint32_t bit_depth)
{
    if (!m_ctx) {
        Debug::log("MD5Validator", "MD5 context not initialized");
        return false;
    }
    
    if (m_finalized) {
        Debug::log("MD5Validator", "Cannot update after finalization");
        return false;
    }
    
    if (!samples || sample_count == 0 || channel_count == 0) {
        return true;  // Nothing to update
    }
    
    // Validate bit depth
    if (bit_depth < 4 || bit_depth > 32) {
        Debug::log("MD5Validator", "Invalid bit depth: %u", bit_depth);
        return false;
    }
    
    // Calculate bytes per sample (sign-extended to next byte boundary)
    uint32_t bytes_per_sample = (bit_depth + 7) / 8;
    size_t total_bytes = sample_count * channel_count * bytes_per_sample;
    
    // Ensure conversion buffer is large enough
    if (m_conversion_buffer.size() < total_bytes) {
        m_conversion_buffer.resize(total_bytes);
    }
    
    // Convert samples to little-endian byte representation
    size_t bytes_written = convertSamplesToBytes(samples, sample_count, 
                                                 channel_count, bit_depth,
                                                 m_conversion_buffer.data());
    
    if (bytes_written == 0) {
        Debug::log("MD5Validator", "Failed to convert samples to bytes");
        return false;
    }
    
    // Update MD5 with converted bytes
    if (!EVP_DigestUpdate(m_ctx, m_conversion_buffer.data(), bytes_written)) {
        Debug::log("MD5Validator", "Failed to update MD5 digest");
        return false;
    }
    
    return true;
}

bool MD5Validator::finalize(uint8_t md5_out[16])
{
    if (!m_ctx) {
        Debug::log("MD5Validator", "MD5 context not initialized");
        return false;
    }
    
    if (m_finalized) {
        // Already finalized, just copy result
        for (int i = 0; i < 16; i++) {
            md5_out[i] = m_computed_md5[i];
        }
        return true;
    }
    
    unsigned int md5_len = 0;
    if (!EVP_DigestFinal_ex(m_ctx, m_computed_md5, &md5_len)) {
        Debug::log("MD5Validator", "Failed to finalize MD5 digest");
        return false;
    }
    
    if (md5_len != 16) {
        Debug::log("MD5Validator", "Unexpected MD5 length: %u", md5_len);
        return false;
    }
    
    m_finalized = true;
    
    // Copy to output
    for (int i = 0; i < 16; i++) {
        md5_out[i] = m_computed_md5[i];
    }
    
    return true;
}

bool MD5Validator::compare(const uint8_t expected_md5[16]) const
{
    if (!m_finalized) {
        Debug::log("MD5Validator", "Cannot compare before finalization");
        return false;
    }
    
    // Compare all 16 bytes
    for (int i = 0; i < 16; i++) {
        if (m_computed_md5[i] != expected_md5[i]) {
            return false;
        }
    }
    
    return true;
}

bool MD5Validator::isZeroMD5(const uint8_t md5[16])
{
    for (int i = 0; i < 16; i++) {
        if (md5[i] != 0) {
            return false;
        }
    }
    return true;
}

void MD5Validator::getMD5(uint8_t md5_out[16]) const
{
    for (int i = 0; i < 16; i++) {
        md5_out[i] = m_computed_md5[i];
    }
}

size_t MD5Validator::convertSamplesToBytes(const int32_t* const* samples,
                                           uint32_t sample_count,
                                           uint32_t channel_count,
                                           uint32_t bit_depth,
                                           uint8_t* output)
{
    size_t bytes_written = 0;
    
    // Interleave channels on per-sample basis
    // For each sample position, write all channels before moving to next sample
    for (uint32_t sample_idx = 0; sample_idx < sample_count; sample_idx++) {
        for (uint32_t ch = 0; ch < channel_count; ch++) {
            int32_t sample = samples[ch][sample_idx];
            
            // Write sample in little-endian format
            size_t sample_bytes = writeSampleLittleEndian(sample, bit_depth, 
                                                          output + bytes_written);
            bytes_written += sample_bytes;
        }
    }
    
    return bytes_written;
}

size_t MD5Validator::writeSampleLittleEndian(int32_t sample,
                                             uint32_t bit_depth,
                                             uint8_t* output)
{
    // Calculate bytes per sample (sign-extended to next byte boundary)
    uint32_t bytes_per_sample = (bit_depth + 7) / 8;
    
    // For non-byte-aligned bit depths, sign-extend to next byte boundary
    // This is done by shifting left to align MSB, then arithmetic shift right
    if (bit_depth % 8 != 0) {
        // Shift left to align MSB to bit 31
        int32_t shift_left = 32 - bit_depth;
        sample = sample << shift_left;
        
        // Arithmetic shift right to sign-extend to byte boundary
        int32_t shift_right = 32 - (bytes_per_sample * 8);
        sample = sample >> shift_right;
    }
    
    // Write bytes in little-endian order
    for (uint32_t i = 0; i < bytes_per_sample; i++) {
        output[i] = static_cast<uint8_t>(sample & 0xFF);
        sample >>= 8;
    }
    
    return bytes_per_sample;
}

} // namespace FLAC
} // namespace Codec
} // namespace PsyMP3

#endif // HAVE_NATIVE_FLAC
