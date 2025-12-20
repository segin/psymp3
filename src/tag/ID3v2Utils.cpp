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
    if (!data || size == 0) {
        return "";
    }
    
    std::string result;
    result.reserve(size * 2); // Worst case: all chars need 2 bytes in UTF-8
    
    for (size_t i = 0; i < size; ++i) {
        uint8_t c = data[i];
        if (c == 0x00) {
            break; // Null terminator
        }
        
        if (c < 0x80) {
            // ASCII: direct copy
            result += static_cast<char>(c);
        } else {
            // Latin-1 extended (0x80-0xFF): convert to UTF-8
            // These map to U+0080 to U+00FF
            result += static_cast<char>(0xC0 | (c >> 6));
            result += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    
    return result;
}

std::vector<uint8_t> encodeISO8859_1(const std::string& text) {
    std::vector<uint8_t> result;
    result.reserve(text.size());
    
    size_t i = 0;
    while (i < text.size()) {
        uint8_t c = static_cast<uint8_t>(text[i]);
        
        if (c < 0x80) {
            // ASCII: direct copy
            result.push_back(c);
            ++i;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < text.size()) {
            // 2-byte UTF-8 sequence
            uint8_t c2 = static_cast<uint8_t>(text[i + 1]);
            if ((c2 & 0xC0) == 0x80) {
                uint32_t codepoint = ((c & 0x1F) << 6) | (c2 & 0x3F);
                if (codepoint <= 0xFF) {
                    result.push_back(static_cast<uint8_t>(codepoint));
                } else {
                    result.push_back('?'); // Outside Latin-1 range
                }
                i += 2;
            } else {
                result.push_back('?');
                ++i;
            }
        } else if ((c & 0xF0) == 0xE0 && i + 2 < text.size()) {
            // 3-byte UTF-8 sequence - outside Latin-1 range
            result.push_back('?');
            i += 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < text.size()) {
            // 4-byte UTF-8 sequence - outside Latin-1 range
            result.push_back('?');
            i += 4;
        } else {
            result.push_back('?');
            ++i;
        }
    }
    
    return result;
}

std::string decodeUTF16_LE(const uint8_t* data, size_t size) {
    if (!data || size < 2) {
        return "";
    }
    
    std::string result;
    result.reserve(size); // Approximate
    
    for (size_t i = 0; i + 1 < size; i += 2) {
        uint16_t unit = static_cast<uint16_t>(data[i]) | 
                        (static_cast<uint16_t>(data[i + 1]) << 8);
        
        if (unit == 0x0000) {
            break; // Null terminator
        }
        
        if (unit < 0x80) {
            // ASCII
            result += static_cast<char>(unit);
        } else if (unit < 0x800) {
            // 2-byte UTF-8
            result += static_cast<char>(0xC0 | (unit >> 6));
            result += static_cast<char>(0x80 | (unit & 0x3F));
        } else if (unit >= 0xD800 && unit <= 0xDBFF) {
            // High surrogate - need low surrogate
            if (i + 3 < size) {
                uint16_t low = static_cast<uint16_t>(data[i + 2]) | 
                               (static_cast<uint16_t>(data[i + 3]) << 8);
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    // Valid surrogate pair
                    uint32_t codepoint = 0x10000 + 
                        ((static_cast<uint32_t>(unit - 0xD800) << 10) | 
                         (low - 0xDC00));
                    // 4-byte UTF-8
                    result += static_cast<char>(0xF0 | (codepoint >> 18));
                    result += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                    result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                    result += static_cast<char>(0x80 | (codepoint & 0x3F));
                    i += 2; // Skip low surrogate
                    continue;
                }
            }
            // Invalid surrogate - use replacement character
            result += "\xEF\xBF\xBD";
        } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
            // Orphan low surrogate - use replacement character
            result += "\xEF\xBF\xBD";
        } else {
            // 3-byte UTF-8
            result += static_cast<char>(0xE0 | (unit >> 12));
            result += static_cast<char>(0x80 | ((unit >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (unit & 0x3F));
        }
    }
    
    return result;
}

std::string decodeUTF16_BE(const uint8_t* data, size_t size) {
    if (!data || size < 2) {
        return "";
    }
    
    std::string result;
    result.reserve(size);
    
    for (size_t i = 0; i + 1 < size; i += 2) {
        uint16_t unit = (static_cast<uint16_t>(data[i]) << 8) | 
                        static_cast<uint16_t>(data[i + 1]);
        
        if (unit == 0x0000) {
            break; // Null terminator
        }
        
        if (unit < 0x80) {
            result += static_cast<char>(unit);
        } else if (unit < 0x800) {
            result += static_cast<char>(0xC0 | (unit >> 6));
            result += static_cast<char>(0x80 | (unit & 0x3F));
        } else if (unit >= 0xD800 && unit <= 0xDBFF) {
            // High surrogate
            if (i + 3 < size) {
                uint16_t low = (static_cast<uint16_t>(data[i + 2]) << 8) | 
                               static_cast<uint16_t>(data[i + 3]);
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    uint32_t codepoint = 0x10000 + 
                        ((static_cast<uint32_t>(unit - 0xD800) << 10) | 
                         (low - 0xDC00));
                    result += static_cast<char>(0xF0 | (codepoint >> 18));
                    result += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                    result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                    result += static_cast<char>(0x80 | (codepoint & 0x3F));
                    i += 2;
                    continue;
                }
            }
            result += "\xEF\xBF\xBD";
        } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
            result += "\xEF\xBF\xBD";
        } else {
            result += static_cast<char>(0xE0 | (unit >> 12));
            result += static_cast<char>(0x80 | ((unit >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (unit & 0x3F));
        }
    }
    
    return result;
}

std::string decodeUTF16_BOM(const uint8_t* data, size_t size) {
    if (!data || size < 2) {
        return "";
    }
    
    // Check for BOM
    if (data[0] == 0xFF && data[1] == 0xFE) {
        // Little-endian BOM
        return decodeUTF16_LE(data + 2, size - 2);
    } else if (data[0] == 0xFE && data[1] == 0xFF) {
        // Big-endian BOM
        return decodeUTF16_BE(data + 2, size - 2);
    } else {
        // No BOM - assume big-endian (ID3v2 spec default)
        return decodeUTF16_BE(data, size);
    }
}


std::vector<uint8_t> encodeUTF16_BOM(const std::string& text) {
    std::vector<uint8_t> result;
    result.reserve(text.size() * 2 + 2);
    
    // Add little-endian BOM
    result.push_back(0xFF);
    result.push_back(0xFE);
    
    size_t i = 0;
    while (i < text.size()) {
        uint8_t c = static_cast<uint8_t>(text[i]);
        uint32_t codepoint = 0;
        
        if (c < 0x80) {
            codepoint = c;
            ++i;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < text.size()) {
            codepoint = ((c & 0x1F) << 6) | 
                        (static_cast<uint8_t>(text[i + 1]) & 0x3F);
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < text.size()) {
            codepoint = ((c & 0x0F) << 12) | 
                        ((static_cast<uint8_t>(text[i + 1]) & 0x3F) << 6) |
                        (static_cast<uint8_t>(text[i + 2]) & 0x3F);
            i += 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < text.size()) {
            codepoint = ((c & 0x07) << 18) | 
                        ((static_cast<uint8_t>(text[i + 1]) & 0x3F) << 12) |
                        ((static_cast<uint8_t>(text[i + 2]) & 0x3F) << 6) |
                        (static_cast<uint8_t>(text[i + 3]) & 0x3F);
            i += 4;
        } else {
            codepoint = 0xFFFD; // Replacement character
            ++i;
        }
        
        if (codepoint < 0x10000) {
            // BMP character - single UTF-16 code unit (little-endian)
            result.push_back(static_cast<uint8_t>(codepoint & 0xFF));
            result.push_back(static_cast<uint8_t>((codepoint >> 8) & 0xFF));
        } else if (codepoint <= 0x10FFFF) {
            // Supplementary character - surrogate pair (little-endian)
            codepoint -= 0x10000;
            uint16_t high = 0xD800 + static_cast<uint16_t>((codepoint >> 10) & 0x3FF);
            uint16_t low = 0xDC00 + static_cast<uint16_t>(codepoint & 0x3FF);
            result.push_back(static_cast<uint8_t>(high & 0xFF));
            result.push_back(static_cast<uint8_t>((high >> 8) & 0xFF));
            result.push_back(static_cast<uint8_t>(low & 0xFF));
            result.push_back(static_cast<uint8_t>((low >> 8) & 0xFF));
        } else {
            // Invalid codepoint - use replacement character
            result.push_back(0xFD);
            result.push_back(0xFF);
        }
    }
    
    return result;
}

std::vector<uint8_t> encodeUTF16_BE(const std::string& text) {
    std::vector<uint8_t> result;
    result.reserve(text.size() * 2);
    
    size_t i = 0;
    while (i < text.size()) {
        uint8_t c = static_cast<uint8_t>(text[i]);
        uint32_t codepoint = 0;
        
        if (c < 0x80) {
            codepoint = c;
            ++i;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < text.size()) {
            codepoint = ((c & 0x1F) << 6) | 
                        (static_cast<uint8_t>(text[i + 1]) & 0x3F);
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < text.size()) {
            codepoint = ((c & 0x0F) << 12) | 
                        ((static_cast<uint8_t>(text[i + 1]) & 0x3F) << 6) |
                        (static_cast<uint8_t>(text[i + 2]) & 0x3F);
            i += 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < text.size()) {
            codepoint = ((c & 0x07) << 18) | 
                        ((static_cast<uint8_t>(text[i + 1]) & 0x3F) << 12) |
                        ((static_cast<uint8_t>(text[i + 2]) & 0x3F) << 6) |
                        (static_cast<uint8_t>(text[i + 3]) & 0x3F);
            i += 4;
        } else {
            codepoint = 0xFFFD;
            ++i;
        }
        
        if (codepoint < 0x10000) {
            // BMP character (big-endian)
            result.push_back(static_cast<uint8_t>((codepoint >> 8) & 0xFF));
            result.push_back(static_cast<uint8_t>(codepoint & 0xFF));
        } else if (codepoint <= 0x10FFFF) {
            // Surrogate pair (big-endian)
            codepoint -= 0x10000;
            uint16_t high = 0xD800 + static_cast<uint16_t>((codepoint >> 10) & 0x3FF);
            uint16_t low = 0xDC00 + static_cast<uint16_t>(codepoint & 0x3FF);
            result.push_back(static_cast<uint8_t>((high >> 8) & 0xFF));
            result.push_back(static_cast<uint8_t>(high & 0xFF));
            result.push_back(static_cast<uint8_t>((low >> 8) & 0xFF));
            result.push_back(static_cast<uint8_t>(low & 0xFF));
        } else {
            result.push_back(0xFF);
            result.push_back(0xFD);
        }
    }
    
    return result;
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
// UTF-8 Validation and Repair
// ============================================================================

bool isValidUTF8(const std::string& text) {
    size_t i = 0;
    while (i < text.size()) {
        uint8_t c = static_cast<uint8_t>(text[i]);
        
        if (c < 0x80) {
            // ASCII
            ++i;
        } else if ((c & 0xE0) == 0xC0) {
            // 2-byte sequence
            if (i + 1 >= text.size() || 
                (static_cast<uint8_t>(text[i + 1]) & 0xC0) != 0x80) {
                return false;
            }
            // Check for overlong encoding
            if ((c & 0x1E) == 0) {
                return false;
            }
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            // 3-byte sequence
            if (i + 2 >= text.size() ||
                (static_cast<uint8_t>(text[i + 1]) & 0xC0) != 0x80 ||
                (static_cast<uint8_t>(text[i + 2]) & 0xC0) != 0x80) {
                return false;
            }
            // Check for overlong encoding
            if (c == 0xE0 && (static_cast<uint8_t>(text[i + 1]) & 0x20) == 0) {
                return false;
            }
            // Check for surrogate range (U+D800-U+DFFF)
            if (c == 0xED && (static_cast<uint8_t>(text[i + 1]) & 0x20) != 0) {
                return false;
            }
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            // 4-byte sequence
            if (i + 3 >= text.size() ||
                (static_cast<uint8_t>(text[i + 1]) & 0xC0) != 0x80 ||
                (static_cast<uint8_t>(text[i + 2]) & 0xC0) != 0x80 ||
                (static_cast<uint8_t>(text[i + 3]) & 0xC0) != 0x80) {
                return false;
            }
            // Check for overlong encoding
            if (c == 0xF0 && (static_cast<uint8_t>(text[i + 1]) & 0x30) == 0) {
                return false;
            }
            // Check for codepoints > U+10FFFF
            if (c > 0xF4 || (c == 0xF4 && static_cast<uint8_t>(text[i + 1]) > 0x8F)) {
                return false;
            }
            i += 4;
        } else {
            // Invalid start byte
            return false;
        }
    }
    
    return true;
}

std::string repairUTF8(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    
    size_t i = 0;
    while (i < text.size()) {
        uint8_t c = static_cast<uint8_t>(text[i]);
        
        if (c < 0x80) {
            result += text[i];
            ++i;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 < text.size() && 
                (static_cast<uint8_t>(text[i + 1]) & 0xC0) == 0x80 &&
                (c & 0x1E) != 0) {
                result += text[i];
                result += text[i + 1];
                i += 2;
            } else {
                result += "\xEF\xBF\xBD"; // Replacement character
                ++i;
            }
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 < text.size() &&
                (static_cast<uint8_t>(text[i + 1]) & 0xC0) == 0x80 &&
                (static_cast<uint8_t>(text[i + 2]) & 0xC0) == 0x80 &&
                !(c == 0xE0 && (static_cast<uint8_t>(text[i + 1]) & 0x20) == 0) &&
                !(c == 0xED && (static_cast<uint8_t>(text[i + 1]) & 0x20) != 0)) {
                result += text[i];
                result += text[i + 1];
                result += text[i + 2];
                i += 3;
            } else {
                result += "\xEF\xBF\xBD";
                ++i;
            }
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 < text.size() &&
                (static_cast<uint8_t>(text[i + 1]) & 0xC0) == 0x80 &&
                (static_cast<uint8_t>(text[i + 2]) & 0xC0) == 0x80 &&
                (static_cast<uint8_t>(text[i + 3]) & 0xC0) == 0x80 &&
                !(c == 0xF0 && (static_cast<uint8_t>(text[i + 1]) & 0x30) == 0) &&
                !(c > 0xF4 || (c == 0xF4 && static_cast<uint8_t>(text[i + 1]) > 0x8F))) {
                result += text[i];
                result += text[i + 1];
                result += text[i + 2];
                result += text[i + 3];
                i += 4;
            } else {
                result += "\xEF\xBF\xBD";
                ++i;
            }
        } else {
            result += "\xEF\xBF\xBD";
            ++i;
        }
    }
    
    return result;
}

std::string decodeUTF8Safe(const uint8_t* data, size_t size) {
    if (!data || size == 0) {
        return "";
    }
    
    // Find null terminator
    size_t len = 0;
    while (len < size && data[len] != 0x00) {
        ++len;
    }
    
    std::string text(reinterpret_cast<const char*>(data), len);
    
    if (isValidUTF8(text)) {
        return text;
    }
    
    return repairUTF8(text);
}

} // namespace ID3v2Utils
} // namespace Tag
} // namespace PsyMP3

