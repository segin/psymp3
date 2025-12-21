/*
 * ID3v2Utils.cpp - ID3v2 utility functions implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

namespace PsyMP3 {
namespace Tag {
namespace ID3v2Utils {

// ============================================================================
// Synchsafe Integer Functions
// ============================================================================

bool canEncodeSynchsafe(uint32_t value) {
    // Synchsafe integers can only encode 28 bits
    return value <= 0x0FFFFFFF;
}

uint32_t encodeSynchsafe(uint32_t value) {
    // Each byte uses only 7 bits (MSB is always 0)
    // Input: 28-bit value
    // Output: 32-bit synchsafe value
    uint32_t result = 0;
    result |= (value & 0x0000007F);        // Bits 0-6
    result |= (value & 0x00003F80) << 1;   // Bits 7-13 -> 8-14
    result |= (value & 0x001FC000) << 2;   // Bits 14-20 -> 16-22
    result |= (value & 0x0FE00000) << 3;   // Bits 21-27 -> 24-30
    return result;
}

uint32_t decodeSynchsafe(uint32_t synchsafe) {
    // Extract 7 bits from each byte
    uint32_t result = 0;
    result |= (synchsafe & 0x0000007F);        // Bits 0-6
    result |= (synchsafe & 0x00007F00) >> 1;   // Bits 8-14 -> 7-13
    result |= (synchsafe & 0x007F0000) >> 2;   // Bits 16-22 -> 14-20
    result |= (synchsafe & 0x7F000000) >> 3;   // Bits 24-30 -> 21-27
    return result;
}

uint32_t decodeSynchsafeBytes(const uint8_t* data) {
    if (!data) {
        return 0;
    }
    // Big-endian byte order, 7 bits per byte
    return ((static_cast<uint32_t>(data[0] & 0x7F) << 21) |
            (static_cast<uint32_t>(data[1] & 0x7F) << 14) |
            (static_cast<uint32_t>(data[2] & 0x7F) << 7) |
            (static_cast<uint32_t>(data[3] & 0x7F)));
}

void encodeSynchsafeBytes(uint32_t value, uint8_t* out) {
    if (!out) {
        return;
    }
    // Big-endian byte order, 7 bits per byte
    out[0] = static_cast<uint8_t>((value >> 21) & 0x7F);
    out[1] = static_cast<uint8_t>((value >> 14) & 0x7F);
    out[2] = static_cast<uint8_t>((value >> 7) & 0x7F);
    out[3] = static_cast<uint8_t>(value & 0x7F);
}

// ============================================================================
// Text Encoding Functions
// ============================================================================

size_t getNullTerminatorSize(TextEncoding encoding) {
    switch (encoding) {
        case TextEncoding::UTF_16_BOM:
        case TextEncoding::UTF_16_BE:
            return 2;
        case TextEncoding::ISO_8859_1:
        case TextEncoding::UTF_8:
        default:
            return 1;
    }
}

size_t findNullTerminator(const uint8_t* data, size_t size, TextEncoding encoding) {
    if (!data || size == 0) {
        return 0;
    }
    
    size_t termSize = getNullTerminatorSize(encoding);
    
    if (termSize == 1) {
        // Single-byte null terminator
        for (size_t i = 0; i < size; ++i) {
            if (data[i] == 0x00) {
                return i;
            }
        }
    } else {
        // Double-byte null terminator (UTF-16)
        for (size_t i = 0; i + 1 < size; i += 2) {
            if (data[i] == 0x00 && data[i + 1] == 0x00) {
                return i;
            }
        }
    }
    
    return size;
}

std::string decodeISO8859_1(const uint8_t* data, size_t size) {
    // Delegate to centralized UTF8Util
    return PsyMP3::Core::Utility::UTF8Util::fromLatin1(data, size);
}

std::vector<uint8_t> encodeISO8859_1(const std::string& text) {
    // Delegate to centralized UTF8Util
    return PsyMP3::Core::Utility::UTF8Util::toLatin1(text);
}

std::string decodeUTF16_LE(const uint8_t* data, size_t size) {
    // Delegate to centralized UTF8Util
    return PsyMP3::Core::Utility::UTF8Util::fromUTF16LE(data, size);
}

std::string decodeUTF16_BE(const uint8_t* data, size_t size) {
    // Delegate to centralized UTF8Util
    return PsyMP3::Core::Utility::UTF8Util::fromUTF16BE(data, size);
}

std::string decodeUTF16_BOM(const uint8_t* data, size_t size) {
    // Delegate to centralized UTF8Util
    return PsyMP3::Core::Utility::UTF8Util::fromUTF16BOM(data, size);
}


std::vector<uint8_t> encodeUTF16_BOM(const std::string& text) {
    // Delegate to centralized UTF8Util
    return PsyMP3::Core::Utility::UTF8Util::toUTF16BOM(text);
}

std::vector<uint8_t> encodeUTF16_BE(const std::string& text) {
    // Delegate to centralized UTF8Util
    return PsyMP3::Core::Utility::UTF8Util::toUTF16BE(text);
}

std::string decodeText(const uint8_t* data, size_t size, TextEncoding encoding) {
    if (!data || size == 0) {
        return "";
    }
    
    switch (encoding) {
        case TextEncoding::ISO_8859_1:
            return decodeISO8859_1(data, size);
        case TextEncoding::UTF_16_BOM:
            return decodeUTF16_BOM(data, size);
        case TextEncoding::UTF_16_BE:
            return decodeUTF16_BE(data, size);
        case TextEncoding::UTF_8:
            return decodeUTF8Safe(data, size);
        default:
            return decodeISO8859_1(data, size);
    }
}

std::string decodeTextWithEncoding(const uint8_t* data, size_t size) {
    if (!data || size < 1) {
        return "";
    }
    
    TextEncoding encoding = static_cast<TextEncoding>(data[0]);
    if (data[0] > 3) {
        // Invalid encoding byte - treat as ISO-8859-1
        encoding = TextEncoding::ISO_8859_1;
    }
    
    return decodeText(data + 1, size - 1, encoding);
}

std::vector<uint8_t> encodeText(const std::string& text, TextEncoding encoding) {
    switch (encoding) {
        case TextEncoding::ISO_8859_1:
            return encodeISO8859_1(text);
        case TextEncoding::UTF_16_BOM:
            return encodeUTF16_BOM(text);
        case TextEncoding::UTF_16_BE:
            return encodeUTF16_BE(text);
        case TextEncoding::UTF_8:
            return std::vector<uint8_t>(text.begin(), text.end());
        default:
            return encodeISO8859_1(text);
    }
}

std::vector<uint8_t> encodeTextWithEncoding(const std::string& text, TextEncoding encoding) {
    std::vector<uint8_t> result;
    result.push_back(static_cast<uint8_t>(encoding));
    
    auto encoded = encodeText(text, encoding);
    result.insert(result.end(), encoded.begin(), encoded.end());
    
    return result;
}

// ============================================================================
// Unsynchronization Functions
// ============================================================================

std::vector<uint8_t> decodeUnsync(const uint8_t* data, size_t size) {
    if (!data || size == 0) {
        return {};
    }
    
    std::vector<uint8_t> result;
    result.reserve(size);
    
    for (size_t i = 0; i < size; ++i) {
        result.push_back(data[i]);
        
        // Skip 0x00 byte after 0xFF (unsync marker)
        if (data[i] == 0xFF && i + 1 < size && data[i + 1] == 0x00) {
            ++i; // Skip the 0x00
        }
    }
    
    return result;
}

std::vector<uint8_t> encodeUnsync(const uint8_t* data, size_t size) {
    if (!data || size == 0) {
        return {};
    }
    
    std::vector<uint8_t> result;
    result.reserve(size + size / 10); // Estimate 10% overhead
    
    for (size_t i = 0; i < size; ++i) {
        result.push_back(data[i]);
        
        // Insert 0x00 after 0xFF if followed by 0x00 or >= 0xE0
        if (data[i] == 0xFF) {
            if (i + 1 < size && (data[i + 1] == 0x00 || data[i + 1] >= 0xE0)) {
                result.push_back(0x00);
            } else if (i + 1 == size) {
                // 0xFF at end of data - add 0x00 to be safe
                result.push_back(0x00);
            }
        }
    }
    
    return result;
}

bool needsUnsync(const uint8_t* data, size_t size) {
    if (!data || size == 0) {
        return false;
    }
    
    for (size_t i = 0; i < size; ++i) {
        if (data[i] == 0xFF) {
            if (i + 1 < size && (data[i + 1] == 0x00 || data[i + 1] >= 0xE0)) {
                return true;
            }
            if (i + 1 == size) {
                return true; // 0xFF at end
            }
        }
    }
    
    return false;
}

// ============================================================================
// UTF-8 Validation and Repair - Delegate to UTF8Util
// ============================================================================

bool isValidUTF8(const std::string& text) {
    return PsyMP3::Core::Utility::UTF8Util::isValid(text);
}

std::string repairUTF8(const std::string& text) {
    return PsyMP3::Core::Utility::UTF8Util::repair(text);
}

std::string decodeUTF8Safe(const uint8_t* data, size_t size) {
    return PsyMP3::Core::Utility::UTF8Util::decodeSafe(data, size);
}

} // namespace ID3v2Utils
} // namespace Tag
} // namespace PsyMP3

