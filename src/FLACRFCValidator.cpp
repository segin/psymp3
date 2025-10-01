/*
 * FLACRFCValidator.cpp - Lightweight RFC 9639 compliance validation for runtime use
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

#ifdef HAVE_FLAC

// ============================================================================
// Lightweight RFC 9639 Validation for Runtime Use
// ============================================================================

namespace FLACRFCValidator {

bool quickSyncPatternCheck(const uint8_t* data, size_t size) {
    if (!data || size < 2) {
        return false;
    }
    
    // RFC 9639 Section 9.1: Sync pattern is 0x3FFE (14 bits) followed by reserved bit (0)
    uint16_t sync_pattern = (static_cast<uint16_t>(data[0]) << 6) | ((data[1] >> 2) & 0x3F);
    if (sync_pattern != 0x3FFE) {
        return false;
    }
    
    // Check reserved bit (bit 1 of byte 1)
    uint8_t reserved_bit = (data[1] >> 1) & 0x01;
    return (reserved_bit == 0);
}

bool quickFrameHeaderCheck(const uint8_t* data, size_t size) {
    if (!data || size < 4) {
        return false;
    }
    
    // Quick sync pattern check
    if (!quickSyncPatternCheck(data, size)) {
        return false;
    }
    
    uint8_t byte1 = data[1];
    uint8_t byte2 = data[2];
    uint8_t byte3 = data[3];
    
    // Quick forbidden value checks
    uint8_t block_size_bits = (byte1 >> 4) & 0x0F;
    uint8_t sample_rate_bits = byte2 & 0x0F;
    uint8_t sample_size_bits = (byte3 >> 1) & 0x07;
    uint8_t reserved_sample = byte3 & 0x01;
    
    // Check for forbidden/reserved values
    if (block_size_bits == 0x00 ||           // Reserved block size
        sample_rate_bits == 0x0F ||          // Invalid sample rate
        sample_size_bits == 0x03 ||          // Reserved sample size
        reserved_sample != 0) {              // Reserved bit must be 0
        return false;
    }
    
    return true;
}

void logRFCViolation(const std::string& rfc_section,
                    const std::string& violation_type,
                    const std::string& description,
                    size_t frame_number) {
    Debug::log("flac_rfc_validator", "[RFC_9639_VIOLATION] Section ", rfc_section, 
              ": ", violation_type, " - ", description, " (Frame: ", frame_number, ")");
}

} // namespace FLACRFCValidator

#endif // HAVE_FLAC