/*
 * FLACRFC9639.cpp - RFC 9639 FLAC specification compliance utilities implementation
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
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

namespace FLACRFC9639 {

// ============================================================================
// RFC 9639 Section 6: Stream Marker Validation Implementation
// ============================================================================

bool validateStreamMarkerWithLogging(const uint8_t* marker, const char* debug_channel) {
    if (!marker) {
        Debug::log(debug_channel, "[validateStreamMarker] NULL marker pointer provided");
        return false;
    }
    
    // Check if marker matches expected "fLaC" (0x66 0x4C 0x61 0x43)
    bool is_valid = FLACRFC9639::validateStreamMarker(marker);
    
    if (is_valid) {
        Debug::log(debug_channel, "[validateStreamMarker] Valid FLAC stream marker found: fLaC");
        return true;
    }
    
    // Log detailed information about what was found
    Debug::log(debug_channel, "[validateStreamMarker] Invalid FLAC stream marker");
    Debug::log(debug_channel, "[validateStreamMarker] Expected: 0x66 0x4C 0x61 0x43 (fLaC)");
    Debug::log(debug_channel, "[validateStreamMarker] Found:    0x", 
              std::hex, std::setfill('0'), std::setw(2), static_cast<int>(marker[0]), " 0x",
              std::setw(2), static_cast<int>(marker[1]), " 0x",
              std::setw(2), static_cast<int>(marker[2]), " 0x",
              std::setw(2), static_cast<int>(marker[3]), std::dec);
    
    // Check if bytes are printable ASCII and log as string if so
    if (std::isprint(marker[0]) && std::isprint(marker[1]) && 
        std::isprint(marker[2]) && std::isprint(marker[3])) {
        std::string found_marker(reinterpret_cast<const char*>(marker), 4);
        Debug::log(debug_channel, "[validateStreamMarker] Found (ASCII): '", found_marker, "'");
    }
    
    // Provide helpful identification of common formats
    std::string error_desc = FLACRFC9639::getStreamMarkerErrorDescription(marker);
    if (!error_desc.empty()) {
        Debug::log(debug_channel, "[validateStreamMarker] ", error_desc);
    }
    
    return false;
}

std::string getStreamMarkerErrorDescription(const uint8_t* marker) {
    if (!marker) {
        return "NULL marker pointer";
    }
    
    // Check for common audio format signatures
    
    // MP3 with ID3 tag
    if (marker[0] == 'I' && marker[1] == 'D' && marker[2] == '3') {
        return "File appears to be MP3 with ID3 tag, not FLAC";
    }
    
    // Ogg container
    if (marker[0] == 'O' && marker[1] == 'g' && marker[2] == 'g' && marker[3] == 'S') {
        return "File appears to be Ogg container, not native FLAC (use Ogg demuxer for Ogg FLAC)";
    }
    
    // RIFF/WAV container
    if (marker[0] == 'R' && marker[1] == 'I' && marker[2] == 'F' && marker[3] == 'F') {
        return "File appears to be RIFF/WAV container, not FLAC";
    }
    
    // MPEG audio frame sync
    if (marker[0] == 0xFF && (marker[1] & 0xE0) == 0xE0) {
        return "File appears to be MPEG audio (MP3/MP2), not FLAC";
    }
    
    // Check if all bytes are printable ASCII
    if (std::isprint(marker[0]) && std::isprint(marker[1]) && 
        std::isprint(marker[2]) && std::isprint(marker[3])) {
        std::string found_marker(reinterpret_cast<const char*>(marker), 4);
        return "Invalid FLAC stream marker '" + found_marker + "' - not a FLAC file";
    }
    
    // Binary data that doesn't match known formats
    return "Invalid FLAC stream marker (binary data) - not a FLAC file";
}

// ============================================================================
// RFC 9639 Section 8.1: Metadata Block Header Parsing Implementation
// ============================================================================

const char* MetadataBlockHeader::getTypeName() const {
    switch (block_type) {
        case MetadataBlockType::STREAMINFO:     return "STREAMINFO";
        case MetadataBlockType::PADDING:        return "PADDING";
        case MetadataBlockType::APPLICATION:    return "APPLICATION";
        case MetadataBlockType::SEEKTABLE:      return "SEEKTABLE";
        case MetadataBlockType::VORBIS_COMMENT: return "VORBIS_COMMENT";
        case MetadataBlockType::CUESHEET:       return "CUESHEET";
        case MetadataBlockType::PICTURE:        return "PICTURE";
        case MetadataBlockType::FORBIDDEN:      return "FORBIDDEN";
        default:
            if (isReservedType()) {
                return "RESERVED";
            }
            return "UNKNOWN";
    }
}

bool parseMetadataBlockHeader(const uint8_t* data, MetadataBlockHeader& header) {
    if (!data) {
        return false;
    }
    
    // Parse byte 0: is_last flag (bit 7) and block type (bits 0-6)
    header.is_last = (data[0] & 0x80) != 0;
    uint8_t type_value = data[0] & 0x7F;
    header.block_type = static_cast<MetadataBlockType>(type_value);
    
    // Check for forbidden block type 127 per RFC 9639 Table 1
    if (header.isForbiddenType()) {
        return false;
    }
    
    // Parse bytes 1-3: 24-bit big-endian block length
    header.block_length = parseBigEndianU24(data + 1);
    
    return true;
}

bool parseMetadataBlockHeaderWithLogging(const uint8_t* data, MetadataBlockHeader& header, 
                                        const char* debug_channel) {
    if (!data) {
        Debug::log(debug_channel, "[parseMetadataBlockHeader] NULL data pointer provided");
        return false;
    }
    
    // Parse the header
    bool success = parseMetadataBlockHeader(data, header);
    
    if (!success) {
        if (header.isForbiddenType()) {
            Debug::log(debug_channel, "[parseMetadataBlockHeader] FORBIDDEN block type 127 detected (RFC 9639 Table 1)");
            Debug::log(debug_channel, "[parseMetadataBlockHeader] This is a forbidden pattern - rejecting stream");
        } else {
            Debug::log(debug_channel, "[parseMetadataBlockHeader] Failed to parse metadata block header");
        }
        return false;
    }
    
    // Log parsed header information
    Debug::log(debug_channel, "[parseMetadataBlockHeader] Parsed metadata block header:");
    Debug::log(debug_channel, "[parseMetadataBlockHeader]   Block type: ", header.getTypeName(), 
              " (", static_cast<int>(header.block_type), ")");
    Debug::log(debug_channel, "[parseMetadataBlockHeader]   Is last: ", header.is_last ? "yes" : "no");
    Debug::log(debug_channel, "[parseMetadataBlockHeader]   Block length: ", header.block_length, " bytes");
    
    // Warn about reserved block types
    if (header.isReservedType()) {
        Debug::log(debug_channel, "[parseMetadataBlockHeader] WARNING: Reserved block type ", 
                  static_cast<int>(header.block_type), " - skipping");
    }
    
    return true;
}

bool validateMetadataBlockLength(const MetadataBlockHeader& header, uint64_t file_size) {
    // Check for zero-length blocks (suspicious but technically allowed for PADDING)
    if (header.block_length == 0) {
        if (header.block_type != MetadataBlockType::PADDING) {
            return false;  // Zero-length non-PADDING block is suspicious
        }
        return true;  // Zero-length PADDING is acceptable
    }
    
    // Maximum reasonable block lengths per type
    constexpr uint32_t MAX_STREAMINFO_LENGTH = 34;  // STREAMINFO is exactly 34 bytes
    constexpr uint32_t MAX_SEEKTABLE_LENGTH = 10000 * 18;  // 10000 seek points max
    constexpr uint32_t MAX_VORBIS_COMMENT_LENGTH = 1024 * 1024;  // 1MB max for comments
    constexpr uint32_t MAX_PICTURE_LENGTH = 16 * 1024 * 1024;  // 16MB max for pictures
    constexpr uint32_t MAX_PADDING_LENGTH = 16 * 1024 * 1024;  // 16MB max for padding
    constexpr uint32_t MAX_APPLICATION_LENGTH = 16 * 1024 * 1024;  // 16MB max for application data
    constexpr uint32_t MAX_CUESHEET_LENGTH = 1024 * 1024;  // 1MB max for cuesheet
    
    // Validate based on block type
    switch (header.block_type) {
        case MetadataBlockType::STREAMINFO:
            if (header.block_length != MAX_STREAMINFO_LENGTH) {
                return false;  // STREAMINFO must be exactly 34 bytes
            }
            break;
            
        case MetadataBlockType::SEEKTABLE:
            if (header.block_length % 18 != 0) {
                return false;  // SEEKTABLE length must be multiple of 18
            }
            if (header.block_length > MAX_SEEKTABLE_LENGTH) {
                return false;  // Too many seek points
            }
            break;
            
        case MetadataBlockType::VORBIS_COMMENT:
            if (header.block_length > MAX_VORBIS_COMMENT_LENGTH) {
                return false;  // Vorbis comments too large
            }
            break;
            
        case MetadataBlockType::PICTURE:
            if (header.block_length > MAX_PICTURE_LENGTH) {
                return false;  // Picture too large
            }
            break;
            
        case MetadataBlockType::PADDING:
            if (header.block_length > MAX_PADDING_LENGTH) {
                return false;  // Padding too large
            }
            break;
            
        case MetadataBlockType::APPLICATION:
            if (header.block_length > MAX_APPLICATION_LENGTH) {
                return false;  // Application data too large
            }
            break;
            
        case MetadataBlockType::CUESHEET:
            if (header.block_length > MAX_CUESHEET_LENGTH) {
                return false;  // Cuesheet too large
            }
            break;
            
        default:
            // For reserved types, use a conservative limit
            if (header.block_length > MAX_APPLICATION_LENGTH) {
                return false;
            }
            break;
    }
    
    // If file size is known, validate block doesn't exceed file
    if (file_size > 0 && header.block_length > file_size) {
        return false;
    }
    
    return true;
}

// ============================================================================
// RFC 9639 Section 9.1.5: UTF-8-like Coded Number Parsing Implementation
// ============================================================================

bool parseCodedNumber(const uint8_t* data, uint64_t& number, size_t& bytes_consumed) {
    if (!data) {
        return false;
    }
    
    uint8_t first_byte = data[0];
    
    // Determine number of bytes based on first byte pattern
    if ((first_byte & 0x80) == 0) {
        // 1 byte: 0xxxxxxx
        number = first_byte;
        bytes_consumed = 1;
        return true;
    }
    else if ((first_byte & 0xE0) == 0xC0) {
        // 2 bytes: 110xxxxx 10xxxxxx
        if ((data[1] & 0xC0) != 0x80) return false;
        number = ((first_byte & 0x1F) << 6) | (data[1] & 0x3F);
        bytes_consumed = 2;
        return true;
    }
    else if ((first_byte & 0xF0) == 0xE0) {
        // 3 bytes: 1110xxxx 10xxxxxx 10xxxxxx
        if ((data[1] & 0xC0) != 0x80 || (data[2] & 0xC0) != 0x80) return false;
        number = ((first_byte & 0x0F) << 12) | ((data[1] & 0x3F) << 6) | (data[2] & 0x3F);
        bytes_consumed = 3;
        return true;
    }
    else if ((first_byte & 0xF8) == 0xF0) {
        // 4 bytes: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        if ((data[1] & 0xC0) != 0x80 || (data[2] & 0xC0) != 0x80 || (data[3] & 0xC0) != 0x80) return false;
        number = ((static_cast<uint64_t>(first_byte) & 0x07) << 18) | 
                 ((static_cast<uint64_t>(data[1]) & 0x3F) << 12) |
                 ((static_cast<uint64_t>(data[2]) & 0x3F) << 6) | 
                 (data[3] & 0x3F);
        bytes_consumed = 4;
        return true;
    }
    else if ((first_byte & 0xFC) == 0xF8) {
        // 5 bytes: 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
        if ((data[1] & 0xC0) != 0x80 || (data[2] & 0xC0) != 0x80 || 
            (data[3] & 0xC0) != 0x80 || (data[4] & 0xC0) != 0x80) return false;
        number = ((static_cast<uint64_t>(first_byte) & 0x03) << 24) | 
                 ((static_cast<uint64_t>(data[1]) & 0x3F) << 18) |
                 ((static_cast<uint64_t>(data[2]) & 0x3F) << 12) |
                 ((static_cast<uint64_t>(data[3]) & 0x3F) << 6) | 
                 (data[4] & 0x3F);
        bytes_consumed = 5;
        return true;
    }
    else if ((first_byte & 0xFE) == 0xFC) {
        // 6 bytes: 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
        if ((data[1] & 0xC0) != 0x80 || (data[2] & 0xC0) != 0x80 || 
            (data[3] & 0xC0) != 0x80 || (data[4] & 0xC0) != 0x80 || 
            (data[5] & 0xC0) != 0x80) return false;
        number = ((static_cast<uint64_t>(first_byte) & 0x01) << 30) | 
                 ((static_cast<uint64_t>(data[1]) & 0x3F) << 24) |
                 ((static_cast<uint64_t>(data[2]) & 0x3F) << 18) |
                 ((static_cast<uint64_t>(data[3]) & 0x3F) << 12) |
                 ((static_cast<uint64_t>(data[4]) & 0x3F) << 6) | 
                 (data[5] & 0x3F);
        bytes_consumed = 6;
        return true;
    }
    else if (first_byte == 0xFE) {
        // 7 bytes: 11111110 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
        if ((data[1] & 0xC0) != 0x80 || (data[2] & 0xC0) != 0x80 || 
            (data[3] & 0xC0) != 0x80 || (data[4] & 0xC0) != 0x80 || 
            (data[5] & 0xC0) != 0x80 || (data[6] & 0xC0) != 0x80) return false;
        number = ((static_cast<uint64_t>(data[1]) & 0x3F) << 30) |
                 ((static_cast<uint64_t>(data[2]) & 0x3F) << 24) |
                 ((static_cast<uint64_t>(data[3]) & 0x3F) << 18) |
                 ((static_cast<uint64_t>(data[4]) & 0x3F) << 12) |
                 ((static_cast<uint64_t>(data[5]) & 0x3F) << 6) | 
                 (data[6] & 0x3F);
        bytes_consumed = 7;
        return true;
    }
    
    // Invalid encoding
    return false;
}

bool parseCodedNumberWithLogging(const uint8_t* data, uint64_t& number, size_t& bytes_consumed,
                                const char* debug_channel) {
    if (!data) {
        Debug::log(debug_channel, "[parseCodedNumber] NULL data pointer provided");
        return false;
    }
    
    bool success = parseCodedNumber(data, number, bytes_consumed);
    
    if (success) {
        Debug::log(debug_channel, "[parseCodedNumber] Parsed coded number: ", number, 
                  " (", bytes_consumed, " bytes)");
    } else {
        Debug::log(debug_channel, "[parseCodedNumber] Failed to parse coded number");
        Debug::log(debug_channel, "[parseCodedNumber] First byte: 0x", 
                  std::hex, std::setfill('0'), std::setw(2), static_cast<int>(data[0]), std::dec);
    }
    
    return success;
}

// ============================================================================
// RFC 9639 Sections 9.1.8, 9.3: CRC Validation Implementation
// ============================================================================

uint8_t calculateCRC8(const uint8_t* data, size_t length) {
    // CRC-8 with polynomial 0x07 (x^8 + x^2 + x + 1)
    uint8_t crc = 0;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}

uint16_t calculateCRC16(const uint8_t* data, size_t length) {
    // CRC-16 with polynomial 0x8005 (x^16 + x^15 + x^2 + 1)
    uint16_t crc = 0;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x8005;
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}

bool validateHeaderCRC8WithLogging(const uint8_t* data, size_t length, uint8_t expected_crc,
                                  uint64_t file_offset, const char* debug_channel) {
    if (!data) {
        Debug::log(debug_channel, "[validateHeaderCRC8] NULL data pointer provided");
        return false;
    }
    
    uint8_t calculated_crc = calculateCRC8(data, length);
    bool is_valid = (calculated_crc == expected_crc);
    
    if (is_valid) {
        Debug::log(debug_channel, "[validateHeaderCRC8] CRC-8 validation passed at offset ", file_offset);
    } else {
        Debug::log(debug_channel, "[validateHeaderCRC8] CRC-8 validation FAILED at offset ", file_offset);
        Debug::log(debug_channel, "[validateHeaderCRC8] Expected: 0x", 
                  std::hex, std::setfill('0'), std::setw(2), static_cast<int>(expected_crc), std::dec);
        Debug::log(debug_channel, "[validateHeaderCRC8] Calculated: 0x", 
                  std::hex, std::setfill('0'), std::setw(2), static_cast<int>(calculated_crc), std::dec);
    }
    
    return is_valid;
}

bool validateFrameCRC16WithLogging(const uint8_t* data, size_t length, uint16_t expected_crc,
                                  uint64_t file_offset, const char* debug_channel) {
    if (!data) {
        Debug::log(debug_channel, "[validateFrameCRC16] NULL data pointer provided");
        return false;
    }
    
    uint16_t calculated_crc = calculateCRC16(data, length);
    bool is_valid = (calculated_crc == expected_crc);
    
    if (is_valid) {
        Debug::log(debug_channel, "[validateFrameCRC16] CRC-16 validation passed at offset ", file_offset);
    } else {
        Debug::log(debug_channel, "[validateFrameCRC16] CRC-16 validation FAILED at offset ", file_offset);
        Debug::log(debug_channel, "[validateFrameCRC16] Expected: 0x", 
                  std::hex, std::setfill('0'), std::setw(4), static_cast<int>(expected_crc), std::dec);
        Debug::log(debug_channel, "[validateFrameCRC16] Calculated: 0x", 
                  std::hex, std::setfill('0'), std::setw(4), static_cast<int>(calculated_crc), std::dec);
    }
    
    return is_valid;
}

// ============================================================================
// RFC 9639 Section 5, Table 1: Forbidden Pattern Detection Implementation
// ============================================================================

ForbiddenPattern checkForbiddenBlockSize(uint16_t min_block_size, uint16_t max_block_size) {
    if (min_block_size < 16) {
        return ForbiddenPattern::STREAMINFO_MIN_BLOCK_SIZE_LT_16;
    }
    if (max_block_size < 16) {
        return ForbiddenPattern::STREAMINFO_MAX_BLOCK_SIZE_LT_16;
    }
    return ForbiddenPattern::NONE;
}

const char* getForbiddenPatternDescription(ForbiddenPattern pattern) {
    switch (pattern) {
        case ForbiddenPattern::METADATA_BLOCK_TYPE_127:
            return "Metadata block type 127 (forbidden per RFC 9639 Table 1)";
        case ForbiddenPattern::STREAMINFO_MIN_BLOCK_SIZE_LT_16:
            return "STREAMINFO minimum block size < 16 (forbidden per RFC 9639 Table 1)";
        case ForbiddenPattern::STREAMINFO_MAX_BLOCK_SIZE_LT_16:
            return "STREAMINFO maximum block size < 16 (forbidden per RFC 9639 Table 1)";
        case ForbiddenPattern::SAMPLE_RATE_BITS_1111:
            return "Sample rate bits 0b1111 (forbidden per RFC 9639 Table 1)";
        case ForbiddenPattern::UNCOMMON_BLOCK_SIZE_65536:
            return "Uncommon block size 65536 (forbidden per RFC 9639 Table 1)";
        case ForbiddenPattern::NONE:
            return "No forbidden pattern detected";
        default:
            return "Unknown forbidden pattern";
    }
}

void logForbiddenPattern(ForbiddenPattern pattern, const std::string& context,
                        const char* debug_channel) {
    if (pattern == ForbiddenPattern::NONE) {
        return;  // Nothing to log
    }
    
    Debug::log(debug_channel, "[ForbiddenPattern] FORBIDDEN PATTERN DETECTED");
    Debug::log(debug_channel, "[ForbiddenPattern] Type: ", getForbiddenPatternDescription(pattern));
    if (!context.empty()) {
        Debug::log(debug_channel, "[ForbiddenPattern] Context: ", context);
    }
    Debug::log(debug_channel, "[ForbiddenPattern] This stream violates RFC 9639 and must be rejected");
}

// ============================================================================
// RFC 9639 Section 8.2: STREAMINFO Block Parsing Implementation
// ============================================================================

bool parseFLACStreamInfo(const uint8_t* data, FLACStreamInfo& info) {
    if (!data) {
        return false;
    }
    
    // Parse bytes 0-1: u(16) min block size (big-endian)
    info.min_block_size = parseBigEndianU16(data);
    
    // Parse bytes 2-3: u(16) max block size (big-endian)
    info.max_block_size = parseBigEndianU16(data + 2);
    
    // Check for forbidden patterns per RFC 9639 Table 1
    ForbiddenPattern pattern = checkForbiddenBlockSize(info.min_block_size, info.max_block_size);
    if (pattern != ForbiddenPattern::NONE) {
        return false;  // Forbidden pattern detected
    }
    
    // Parse bytes 4-6: u(24) min frame size (big-endian)
    info.min_frame_size = parseBigEndianU24(data + 4);
    
    // Parse bytes 7-9: u(24) max frame size (big-endian)
    info.max_frame_size = parseBigEndianU24(data + 7);
    
    // Parse bytes 10-12: u(20) sample rate (big-endian, bits 0-19 of 3-byte value)
    // Sample rate is in bits 0-19 of bytes 10-12
    uint32_t sample_rate_and_more = parseBigEndianU24(data + 10);
    info.sample_rate = (sample_rate_and_more >> 4);  // Upper 20 bits
    
    // Parse byte 12 bits 20-22: u(3) channels-1
    // Channels are in bits 1-3 of byte 12 (after sample rate)
    uint8_t channels_minus_1 = (data[12] >> 1) & 0x07;
    info.channels = channels_minus_1 + 1;
    
    // Parse bytes 12-13 bits 23-27: u(5) bits per sample-1
    // Bits per sample spans bit 0 of byte 12 and bits 7-4 of byte 13
    uint8_t bps_minus_1 = ((data[12] & 0x01) << 4) | ((data[13] >> 4) & 0x0F);
    info.bits_per_sample = bps_minus_1 + 1;
    
    // Parse bytes 13-17 bits 28-63: u(36) total samples
    // Total samples is in lower 4 bits of byte 13 and all of bytes 14-17
    info.total_samples = (static_cast<uint64_t>(data[13] & 0x0F) << 32) |
                        (static_cast<uint64_t>(data[14]) << 24) |
                        (static_cast<uint64_t>(data[15]) << 16) |
                        (static_cast<uint64_t>(data[16]) << 8) |
                         static_cast<uint64_t>(data[17]);
    
    // Parse bytes 18-33: u(128) MD5 signature
    std::memcpy(info.md5_signature, data + 18, 16);
    
    return true;
}

bool parseFLACStreamInfoWithLogging(const uint8_t* data, FLACStreamInfo& info,
                                   const char* debug_channel) {
    if (!data) {
        Debug::log(debug_channel, "[parseStreamInfo] NULL data pointer provided");
        return false;
    }
    
    Debug::log(debug_channel, "[parseStreamInfo] Parsing STREAMINFO block (34 bytes)");
    
    bool success = parseFLACStreamInfo(data, info);
    
    if (!success) {
        Debug::log(debug_channel, "[parseStreamInfo] Failed to parse STREAMINFO");
        
        // Check for specific forbidden patterns
        uint16_t min_block = parseBigEndianU16(data);
        uint16_t max_block = parseBigEndianU16(data + 2);
        ForbiddenPattern pattern = checkForbiddenBlockSize(min_block, max_block);
        
        if (pattern != ForbiddenPattern::NONE) {
            logForbiddenPattern(pattern, "STREAMINFO block", debug_channel);
        }
        
        return false;
    }
    
    // Log parsed STREAMINFO details
    Debug::log(debug_channel, "[parseStreamInfo] Successfully parsed STREAMINFO:");
    Debug::log(debug_channel, "[parseStreamInfo]   Min block size: ", info.min_block_size, " samples");
    Debug::log(debug_channel, "[parseStreamInfo]   Max block size: ", info.max_block_size, " samples");
    Debug::log(debug_channel, "[parseStreamInfo]   Min frame size: ", info.min_frame_size, " bytes", 
              (info.min_frame_size == 0 ? " (unknown)" : ""));
    Debug::log(debug_channel, "[parseStreamInfo]   Max frame size: ", info.max_frame_size, " bytes",
              (info.max_frame_size == 0 ? " (unknown)" : ""));
    Debug::log(debug_channel, "[parseStreamInfo]   Sample rate: ", info.sample_rate, " Hz");
    Debug::log(debug_channel, "[parseStreamInfo]   Channels: ", static_cast<int>(info.channels));
    Debug::log(debug_channel, "[parseStreamInfo]   Bits per sample: ", static_cast<int>(info.bits_per_sample));
    Debug::log(debug_channel, "[parseStreamInfo]   Total samples: ", info.total_samples,
              (info.total_samples == 0 ? " (unknown)" : ""));
    
    if (info.total_samples > 0 && info.sample_rate > 0) {
        uint64_t duration_ms = info.getDurationMs();
        uint64_t duration_sec = duration_ms / 1000;
        uint64_t minutes = duration_sec / 60;
        uint64_t seconds = duration_sec % 60;
        Debug::log(debug_channel, "[parseStreamInfo]   Duration: ", minutes, ":", 
                  (seconds < 10 ? "0" : ""), seconds, " (", duration_ms, " ms)");
    }
    
    // Validate the parsed STREAMINFO
    if (!validateFLACStreamInfo(info)) {
        Debug::log(debug_channel, "[parseStreamInfo] WARNING: STREAMINFO validation failed");
        return false;
    }
    
    return true;
}

bool validateFLACStreamInfo(const FLACStreamInfo& info) {
    // Check for forbidden patterns per RFC 9639 Table 1
    FLACRFC9639::ForbiddenPattern pattern = FLACRFC9639::checkForbiddenBlockSize(info.min_block_size, info.max_block_size);
    if (pattern != FLACRFC9639::ForbiddenPattern::NONE) {
        return false;
    }
    
    // Validate sample rate (must be > 0 for audio streams)
    if (info.sample_rate == 0) {
        return false;  // Invalid for audio streams
    }
    
    // Validate channel count (1-8)
    if (info.channels < 1 || info.channels > 8) {
        return false;
    }
    
    // Validate bits per sample (4-32)
    if (info.bits_per_sample < 4 || info.bits_per_sample > 32) {
        return false;
    }
    
    // Validate logical consistency
    if (info.max_block_size < info.min_block_size) {
        return false;
    }
    
    // If frame sizes are specified, validate consistency
    if (info.min_frame_size > 0 && info.max_frame_size > 0) {
        if (info.max_frame_size < info.min_frame_size) {
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// RFC 9639 Section 9.1: Frame Sync Code Detection Implementation
// ============================================================================

bool detectFrameSync(const uint8_t* data, size_t size, FrameSyncResult& result) {
    if (!data || size < 2) {
        result.found = false;
        return false;
    }
    
    // Search for 15-bit sync pattern: 0b111111111111100
    // This appears as 0xFF 0xF8 or 0xFF 0xF9 when byte-aligned
    for (size_t i = 0; i < size - 1; i++) {
        // Check for sync pattern: first byte must be 0xFF
        if (data[i] != 0xFF) {
            continue;
        }
        
        // Check second byte: upper 7 bits must be 0b1111100 (0xF8 or 0xF9)
        uint8_t second_byte = data[i + 1];
        if ((second_byte & 0xFE) == 0xF8) {
            // Valid sync pattern found
            result.found = true;
            result.offset = i;
            result.blocking_strategy = extractBlockingStrategy(data + i);
            return true;
        }
    }
    
    // No sync pattern found
    result.found = false;
    return false;
}

bool detectFrameSyncWithLogging(const uint8_t* data, size_t size, FrameSyncResult& result,
                               const char* debug_channel) {
    if (!data) {
        Debug::log(debug_channel, "[detectFrameSync] NULL data pointer provided");
        result.found = false;
        return false;
    }
    
    if (size < 2) {
        Debug::log(debug_channel, "[detectFrameSync] Buffer too small (", size, " bytes, need at least 2)");
        result.found = false;
        return false;
    }
    
    Debug::log(debug_channel, "[detectFrameSync] Searching for frame sync in ", size, " bytes");
    
    bool found = detectFrameSync(data, size, result);
    
    if (found) {
        Debug::log(debug_channel, "[detectFrameSync] Frame sync found at offset ", result.offset);
        Debug::log(debug_channel, "[detectFrameSync] Sync bytes: 0x", 
                  std::hex, std::setfill('0'), std::setw(2), static_cast<int>(data[result.offset]), 
                  " 0x", std::setw(2), static_cast<int>(data[result.offset + 1]), std::dec);
        Debug::log(debug_channel, "[detectFrameSync] Blocking strategy: ", 
                  (result.blocking_strategy == BlockingStrategy::FIXED ? "FIXED" : "VARIABLE"));
    } else {
        Debug::log(debug_channel, "[detectFrameSync] No frame sync pattern found in buffer");
    }
    
    return found;
}

bool validateFrameSyncAt(const uint8_t* data, size_t offset, size_t size) {
    if (!data || offset + 1 >= size) {
        return false;
    }
    
    // Check for valid sync pattern at offset
    if (data[offset] != 0xFF) {
        return false;
    }
    
    uint8_t second_byte = data[offset + 1];
    return (second_byte & 0xFE) == 0xF8;
}

// ============================================================================
// RFC 9639 Section 9.1.1: Block Size Bits Parser Implementation
// ============================================================================

bool parseBlockSizeBits(uint8_t block_size_bits, uint8_t uncommon_block_size_8bit,
                       uint16_t uncommon_block_size_16bit, uint32_t& block_size) {
    // Per RFC 9639 Table 14
    switch (block_size_bits) {
        case 0b0000:  // Reserved
            return false;
        case 0b0001:  // 192 samples
            block_size = 192;
            return true;
        case 0b0010:  // 576 samples
            block_size = 576;
            return true;
        case 0b0011:  // 1152 samples
            block_size = 1152;
            return true;
        case 0b0100:  // 2304 samples
            block_size = 2304;
            return true;
        case 0b0101:  // 4608 samples
            block_size = 4608;
            return true;
        case 0b0110:  // 8-bit uncommon block size (stored as n-1)
            block_size = uncommon_block_size_8bit + 1;
            // Check for forbidden pattern: 65536 samples
            if (block_size == 65536) {
                return false;  // Forbidden per RFC 9639 Table 1
            }
            return true;
        case 0b0111:  // 16-bit uncommon block size (stored as n-1)
            block_size = uncommon_block_size_16bit + 1;
            // Check for forbidden pattern: 65536 samples
            if (block_size == 65536) {
                return false;  // Forbidden per RFC 9639 Table 1
            }
            return true;
        case 0b1000:  // 256 samples
            block_size = 256;
            return true;
        case 0b1001:  // 512 samples
            block_size = 512;
            return true;
        case 0b1010:  // 1024 samples
            block_size = 1024;
            return true;
        case 0b1011:  // 2048 samples
            block_size = 2048;
            return true;
        case 0b1100:  // 4096 samples
            block_size = 4096;
            return true;
        case 0b1101:  // 8192 samples
            block_size = 8192;
            return true;
        case 0b1110:  // 16384 samples
            block_size = 16384;
            return true;
        case 0b1111:  // 32768 samples
            block_size = 32768;
            return true;
        default:
            return false;
    }
}

// ============================================================================
// RFC 9639 Section 9.1.2: Sample Rate Bits Parser Implementation
// ============================================================================

bool parseSampleRateBits(uint8_t sample_rate_bits, uint8_t uncommon_sample_rate_8bit,
                        uint16_t uncommon_sample_rate_16bit, uint32_t& sample_rate) {
    // Per RFC 9639 Section 9.1.2
    switch (sample_rate_bits) {
        case 0b0000:  // Get from STREAMINFO
            sample_rate = 0;  // Caller must use STREAMINFO
            return true;
        case 0b0001:  // 88.2 kHz
            sample_rate = 88200;
            return true;
        case 0b0010:  // 176.4 kHz
            sample_rate = 176400;
            return true;
        case 0b0011:  // 192 kHz
            sample_rate = 192000;
            return true;
        case 0b0100:  // 8 kHz
            sample_rate = 8000;
            return true;
        case 0b0101:  // 16 kHz
            sample_rate = 16000;
            return true;
        case 0b0110:  // 22.05 kHz
            sample_rate = 22050;
            return true;
        case 0b0111:  // 24 kHz
            sample_rate = 24000;
            return true;
        case 0b1000:  // 32 kHz
            sample_rate = 32000;
            return true;
        case 0b1001:  // 44.1 kHz
            sample_rate = 44100;
            return true;
        case 0b1010:  // 48 kHz
            sample_rate = 48000;
            return true;
        case 0b1011:  // 96 kHz
            sample_rate = 96000;
            return true;
        case 0b1100:  // 8-bit uncommon sample rate in kHz
            sample_rate = uncommon_sample_rate_8bit * 1000;
            return true;
        case 0b1101:  // 16-bit uncommon sample rate in Hz
            sample_rate = uncommon_sample_rate_16bit;
            return true;
        case 0b1110:  // 16-bit uncommon sample rate in tens of Hz
            sample_rate = uncommon_sample_rate_16bit * 10;
            return true;
        case 0b1111:  // Forbidden
            return false;  // Forbidden per RFC 9639 Table 1
        default:
            return false;
    }
}

// ============================================================================
// RFC 9639 Section 9.1.3: Channel Assignment Parser Implementation
// ============================================================================

bool parseChannelAssignment(uint8_t channel_bits, ChannelAssignment& assignment, uint8_t& channels) {
    // Per RFC 9639 Section 9.1.3
    switch (channel_bits) {
        case 0b0000:  // 1 channel (mono)
            assignment = ChannelAssignment::INDEPENDENT_1;
            channels = 1;
            return true;
        case 0b0001:  // 2 channels (stereo)
            assignment = ChannelAssignment::INDEPENDENT_2;
            channels = 2;
            return true;
        case 0b0010:  // 3 channels
            assignment = ChannelAssignment::INDEPENDENT_3;
            channels = 3;
            return true;
        case 0b0011:  // 4 channels (quad)
            assignment = ChannelAssignment::INDEPENDENT_4;
            channels = 4;
            return true;
        case 0b0100:  // 5 channels
            assignment = ChannelAssignment::INDEPENDENT_5;
            channels = 5;
            return true;
        case 0b0101:  // 6 channels (5.1)
            assignment = ChannelAssignment::INDEPENDENT_6;
            channels = 6;
            return true;
        case 0b0110:  // 7 channels
            assignment = ChannelAssignment::INDEPENDENT_7;
            channels = 7;
            return true;
        case 0b0111:  // 8 channels (7.1)
            assignment = ChannelAssignment::INDEPENDENT_8;
            channels = 8;
            return true;
        case 0b1000:  // Left/side stereo
            assignment = ChannelAssignment::LEFT_SIDE;
            channels = 2;
            return true;
        case 0b1001:  // Right/side stereo
            assignment = ChannelAssignment::RIGHT_SIDE;
            channels = 2;
            return true;
        case 0b1010:  // Mid/side stereo
            assignment = ChannelAssignment::MID_SIDE;
            channels = 2;
            return true;
        case 0b1011:  // Reserved
        case 0b1100:  // Reserved
        case 0b1101:  // Reserved
        case 0b1110:  // Reserved
        case 0b1111:  // Reserved
            assignment = ChannelAssignment::RESERVED;
            return false;
        default:
            return false;
    }
}

// ============================================================================
// RFC 9639 Section 9.1.4: Bit Depth Parser Implementation
// ============================================================================

bool parseBitDepthBits(uint8_t bit_depth_bits, uint8_t& bits_per_sample) {
    // Per RFC 9639 Section 9.1.4
    switch (bit_depth_bits) {
        case 0b000:  // Get from STREAMINFO
            bits_per_sample = 0;  // Caller must use STREAMINFO
            return true;
        case 0b001:  // 8 bits per sample
            bits_per_sample = 8;
            return true;
        case 0b010:  // 12 bits per sample
            bits_per_sample = 12;
            return true;
        case 0b011:  // Reserved
            return false;
        case 0b100:  // 16 bits per sample
            bits_per_sample = 16;
            return true;
        case 0b101:  // 20 bits per sample
            bits_per_sample = 20;
            return true;
        case 0b110:  // 24 bits per sample
            bits_per_sample = 24;
            return true;
        case 0b111:  // 32 bits per sample
            bits_per_sample = 32;
            return true;
        default:
            return false;
    }
}

// ============================================================================
// RFC 9639 Section 8.5: SEEKTABLE Parser Implementation
// ============================================================================

bool parseSeekTable(const uint8_t* data, uint32_t length, std::vector<SeekPoint>& seek_points) {
    if (!data || length == 0) {
        return false;
    }
    
    // SEEKTABLE length must be multiple of 18 (each seek point is 18 bytes)
    if (length % 18 != 0) {
        return false;
    }
    
    size_t num_seek_points = length / 18;
    seek_points.clear();
    seek_points.reserve(num_seek_points);
    
    for (size_t i = 0; i < num_seek_points; i++) {
        const uint8_t* point_data = data + (i * 18);
        
        SeekPoint point;
        point.sample_number = parseBigEndianU64(point_data);
        point.stream_offset = parseBigEndianU64(point_data + 8);
        point.frame_samples = parseBigEndianU16(point_data + 16);
        
        seek_points.push_back(point);
    }
    
    return true;
}

bool validateSeekTable(const std::vector<SeekPoint>& seek_points) {
    if (seek_points.empty()) {
        return true;  // Empty seek table is valid
    }
    
    // Check that seek points are sorted and unique
    uint64_t last_sample = 0;
    for (const auto& point : seek_points) {
        if (point.isPlaceholder()) {
            continue;  // Placeholders can appear anywhere
        }
        
        if (point.sample_number < last_sample) {
            return false;  // Not sorted
        }
        
        if (point.sample_number == last_sample && last_sample != 0) {
            return false;  // Duplicate (not unique)
        }
        
        last_sample = point.sample_number;
    }
    
    return true;
}

// ============================================================================
// RFC 9639 Section 8.6: VORBIS_COMMENT Parser Implementation
// ============================================================================

bool parseVorbisComment(const uint8_t* data, uint32_t length, 
                       std::string& vendor_string,
                       std::map<std::string, std::string>& comments) {
    if (!data || length < 8) {  // Minimum: 4 bytes vendor length + 4 bytes comment count
        return false;
    }
    
    size_t offset = 0;
    
    // Parse vendor string length (little-endian)
    uint32_t vendor_length = parseLittleEndianU32(data + offset);
    offset += 4;
    
    if (offset + vendor_length > length) {
        return false;  // Vendor string exceeds block length
    }
    
    // Parse vendor string (UTF-8)
    vendor_string = std::string(reinterpret_cast<const char*>(data + offset), vendor_length);
    offset += vendor_length;
    
    if (offset + 4 > length) {
        return false;  // Not enough data for comment count
    }
    
    // Parse comment count (little-endian)
    uint32_t comment_count = parseLittleEndianU32(data + offset);
    offset += 4;
    
    comments.clear();
    
    // Parse each comment
    for (uint32_t i = 0; i < comment_count; i++) {
        if (offset + 4 > length) {
            return false;  // Not enough data for comment length
        }
        
        // Parse comment length (little-endian)
        uint32_t comment_length = parseLittleEndianU32(data + offset);
        offset += 4;
        
        if (offset + comment_length > length) {
            return false;  // Comment exceeds block length
        }
        
        // Parse comment string (UTF-8)
        std::string comment(reinterpret_cast<const char*>(data + offset), comment_length);
        offset += comment_length;
        
        // Split on first equals sign
        size_t equals_pos = comment.find('=');
        if (equals_pos != std::string::npos) {
            std::string name = comment.substr(0, equals_pos);
            std::string value = comment.substr(equals_pos + 1);
            
            // Validate field name
            if (validateVorbisCommentFieldName(name)) {
                // Convert name to uppercase for case-insensitive comparison
                std::transform(name.begin(), name.end(), name.begin(), ::toupper);
                comments[name] = value;
            }
        }
    }
    
    return true;
}

bool validateVorbisCommentFieldName(const std::string& field_name) {
    if (field_name.empty()) {
        return false;
    }
    
    // Field names must contain only printable ASCII 0x20-0x7E except 0x3D (equals)
    for (char c : field_name) {
        if (c < 0x20 || c > 0x7E || c == 0x3D) {
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// RFC 9639 Section 8.8: PICTURE Parser Implementation
// ============================================================================

bool parsePicture(const uint8_t* data, uint32_t length, Picture& picture) {
    if (!data || length < 32) {  // Minimum size for picture metadata
        return false;
    }
    
    size_t offset = 0;
    
    // Parse picture type (32-bit big-endian)
    picture.picture_type = parseBigEndianU32(data + offset);
    offset += 4;
    
    // Parse MIME type length (32-bit big-endian)
    uint32_t mime_length = parseBigEndianU32(data + offset);
    offset += 4;
    
    if (offset + mime_length > length) {
        return false;
    }
    
    // Parse MIME type (ASCII)
    picture.mime_type = std::string(reinterpret_cast<const char*>(data + offset), mime_length);
    offset += mime_length;
    
    if (offset + 4 > length) {
        return false;
    }
    
    // Parse description length (32-bit big-endian)
    uint32_t desc_length = parseBigEndianU32(data + offset);
    offset += 4;
    
    if (offset + desc_length > length) {
        return false;
    }
    
    // Parse description (UTF-8)
    picture.description = std::string(reinterpret_cast<const char*>(data + offset), desc_length);
    offset += desc_length;
    
    if (offset + 20 > length) {  // Need 20 bytes for width, height, depth, colors, data length
        return false;
    }
    
    // Parse picture dimensions and color info
    picture.width = parseBigEndianU32(data + offset);
    offset += 4;
    picture.height = parseBigEndianU32(data + offset);
    offset += 4;
    picture.color_depth = parseBigEndianU32(data + offset);
    offset += 4;
    picture.colors_used = parseBigEndianU32(data + offset);
    offset += 4;
    
    // Parse picture data length (32-bit big-endian)
    uint32_t data_length = parseBigEndianU32(data + offset);
    offset += 4;
    
    if (offset + data_length > length) {
        return false;
    }
    
    // Parse picture data
    picture.data.resize(data_length);
    std::memcpy(picture.data.data(), data + offset, data_length);
    
    return true;
}

// ============================================================================
// RFC 9639 Section 7: Streamable Subset Detection Implementation
// ============================================================================

bool isStreamableSubset(const FLACStreamInfo& streaminfo, 
                       uint32_t sample_rate_from_frame,
                       uint8_t bit_depth_from_frame) {
    // Check if frame headers reference STREAMINFO for sample rate or bit depth
    // Per RFC 9639 Section 7, streamable subset requires these in frame headers
    if (sample_rate_from_frame == 0 || bit_depth_from_frame == 0) {
        return false;  // Frame header references STREAMINFO - not streamable
    }
    
    // Check maximum block size constraint
    if (streaminfo.max_block_size > 16384) {
        return false;  // Exceeds streamable subset limit
    }
    
    // Check sample rate-specific block size constraint
    if (streaminfo.sample_rate <= 48000 && streaminfo.max_block_size > 4608) {
        return false;  // Exceeds streamable subset limit for ≤48kHz streams
    }
    
    return true;
}

} // namespace FLACRFC9639
