/*
 * UTF8Util.cpp - Universal UTF-8 encoding/decoding utilities implementation
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
namespace Core {
namespace Utility {

// Static replacement character string
static const std::string REPLACEMENT_CHAR = "\xEF\xBF\xBD"; // U+FFFD

const std::string& UTF8Util::replacementCharacter() {
    return REPLACEMENT_CHAR;
}

bool UTF8Util::isValidCodepoint(uint32_t codepoint) {
    // Valid range: U+0000 to U+10FFFF, excluding surrogates (U+D800-U+DFFF)
    return codepoint <= 0x10FFFF && (codepoint < 0xD800 || codepoint > 0xDFFF);
}

void UTF8Util::appendCodepoint(std::string& output, uint32_t codepoint) {
    if (codepoint < 0x80) {
        output += static_cast<char>(codepoint);
    } else if (codepoint < 0x800) {
        output += static_cast<char>(0xC0 | (codepoint >> 6));
        output += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint < 0x10000) {
        output += static_cast<char>(0xE0 | (codepoint >> 12));
        output += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        output += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0x10FFFF) {
        output += static_cast<char>(0xF0 | (codepoint >> 18));
        output += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        output += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        output += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else {
        output += REPLACEMENT_CHAR;
    }
}

std::string UTF8Util::encodeCodepoint(uint32_t codepoint) {
    std::string result;
    appendCodepoint(result, codepoint);
    return result;
}


uint32_t UTF8Util::decodeCodepoint(const uint8_t* data, size_t size, size_t& bytesConsumed) {
    if (!data || size == 0) {
        bytesConsumed = 0;
        return 0xFFFD;
    }
    
    uint8_t c = data[0];
    
    if (c < 0x80) {
        // ASCII
        bytesConsumed = 1;
        return c;
    } else if ((c & 0xE0) == 0xC0) {
        // 2-byte sequence
        if (size < 2 || (data[1] & 0xC0) != 0x80) {
            bytesConsumed = 1;
            return 0xFFFD;
        }
        // Check for overlong encoding
        if ((c & 0x1E) == 0) {
            bytesConsumed = 2;
            return 0xFFFD;
        }
        bytesConsumed = 2;
        return ((c & 0x1F) << 6) | (data[1] & 0x3F);
    } else if ((c & 0xF0) == 0xE0) {
        // 3-byte sequence
        if (size < 3 || (data[1] & 0xC0) != 0x80 || (data[2] & 0xC0) != 0x80) {
            bytesConsumed = 1;
            return 0xFFFD;
        }
        // Check for overlong encoding
        if (c == 0xE0 && (data[1] & 0x20) == 0) {
            bytesConsumed = 3;
            return 0xFFFD;
        }
        uint32_t cp = ((c & 0x0F) << 12) | ((data[1] & 0x3F) << 6) | (data[2] & 0x3F);
        // Check for surrogate range
        if (cp >= 0xD800 && cp <= 0xDFFF) {
            bytesConsumed = 3;
            return 0xFFFD;
        }
        bytesConsumed = 3;
        return cp;
    } else if ((c & 0xF8) == 0xF0) {
        // 4-byte sequence
        if (size < 4 || (data[1] & 0xC0) != 0x80 || 
            (data[2] & 0xC0) != 0x80 || (data[3] & 0xC0) != 0x80) {
            bytesConsumed = 1;
            return 0xFFFD;
        }
        // Check for overlong encoding
        if (c == 0xF0 && (data[1] & 0x30) == 0) {
            bytesConsumed = 4;
            return 0xFFFD;
        }
        uint32_t cp = ((c & 0x07) << 18) | ((data[1] & 0x3F) << 12) | 
                      ((data[2] & 0x3F) << 6) | (data[3] & 0x3F);
        // Check for codepoints > U+10FFFF
        if (cp > 0x10FFFF) {
            bytesConsumed = 4;
            return 0xFFFD;
        }
        bytesConsumed = 4;
        return cp;
    }
    
    // Invalid start byte
    bytesConsumed = 1;
    return 0xFFFD;
}

uint32_t UTF8Util::decodeCodepoint(const std::string& text, size_t& bytesConsumed) {
    return decodeCodepoint(reinterpret_cast<const uint8_t*>(text.data()), 
                          text.size(), bytesConsumed);
}


bool UTF8Util::isValid(const uint8_t* data, size_t size) {
    size_t i = 0;
    while (i < size) {
        size_t consumed;
        uint32_t cp = decodeCodepoint(data + i, size - i, consumed);
        if (cp == 0xFFFD && !(data[i] == 0xEF && i + 2 < size && 
            data[i+1] == 0xBF && data[i+2] == 0xBD)) {
            // Got replacement char but input wasn't actually U+FFFD
            return false;
        }
        i += consumed;
    }
    return true;
}

bool UTF8Util::isValid(const std::string& text) {
    return isValid(reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

std::string UTF8Util::repair(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    
    size_t i = 0;
    while (i < text.size()) {
        size_t consumed;
        uint32_t cp = decodeCodepoint(
            reinterpret_cast<const uint8_t*>(text.data()) + i,
            text.size() - i, consumed);
        appendCodepoint(result, cp);
        i += consumed;
    }
    
    return result;
}

std::string UTF8Util::decodeSafe(const uint8_t* data, size_t size) {
    if (!data || size == 0) {
        return "";
    }
    
    // Find null terminator
    size_t len = 0;
    while (len < size && data[len] != 0x00) {
        ++len;
    }
    
    std::string text(reinterpret_cast<const char*>(data), len);
    
    if (isValid(text)) {
        return text;
    }
    
    return repair(text);
}


// ============================================================================
// ISO-8859-1 (Latin-1) Conversion
// ============================================================================

std::string UTF8Util::fromLatin1(const uint8_t* data, size_t size) {
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

std::string UTF8Util::fromLatin1(const std::string& text) {
    return fromLatin1(reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

std::vector<uint8_t> UTF8Util::toLatin1(const std::string& text) {
    std::vector<uint8_t> result;
    result.reserve(text.size());
    
    size_t i = 0;
    while (i < text.size()) {
        size_t consumed;
        uint32_t cp = decodeCodepoint(
            reinterpret_cast<const uint8_t*>(text.data()) + i,
            text.size() - i, consumed);
        
        if (cp <= 0xFF) {
            result.push_back(static_cast<uint8_t>(cp));
        } else {
            result.push_back('?'); // Outside Latin-1 range
        }
        i += consumed;
    }
    
    return result;
}


// ============================================================================
// UTF-16 Conversion
// ============================================================================

std::string UTF8Util::fromUTF16LE(const uint8_t* data, size_t size) {
    if (!data || size < 2) {
        return "";
    }
    
    std::string result;
    result.reserve(size);
    
    for (size_t i = 0; i + 1 < size; i += 2) {
        uint16_t unit = static_cast<uint16_t>(data[i]) | 
                        (static_cast<uint16_t>(data[i + 1]) << 8);
        
        if (unit == 0x0000) {
            break; // Null terminator
        }
        
        if (unit >= 0xD800 && unit <= 0xDBFF) {
            // High surrogate - need low surrogate
            if (i + 3 < size) {
                uint16_t low = static_cast<uint16_t>(data[i + 2]) | 
                               (static_cast<uint16_t>(data[i + 3]) << 8);
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    // Valid surrogate pair
                    uint32_t codepoint = 0x10000 + 
                        ((static_cast<uint32_t>(unit - 0xD800) << 10) | 
                         (low - 0xDC00));
                    appendCodepoint(result, codepoint);
                    i += 2; // Skip low surrogate
                    continue;
                }
            }
            // Invalid surrogate - use replacement character
            result += REPLACEMENT_CHAR;
        } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
            // Orphan low surrogate - use replacement character
            result += REPLACEMENT_CHAR;
        } else {
            appendCodepoint(result, unit);
        }
    }
    
    return result;
}

std::string UTF8Util::fromUTF16BE(const uint8_t* data, size_t size) {
    if (!data || size < 2) {
        return "";
    }
    
    std::string result;
    result.reserve(size);
    
    for (size_t i = 0; i + 1 < size; i += 2) {
        uint16_t unit = (static_cast<uint16_t>(data[i]) << 8) | 
                        static_cast<uint16_t>(data[i + 1]);
        
        if (unit == 0x0000) {
            break;
        }
        
        if (unit >= 0xD800 && unit <= 0xDBFF) {
            if (i + 3 < size) {
                uint16_t low = (static_cast<uint16_t>(data[i + 2]) << 8) | 
                               static_cast<uint16_t>(data[i + 3]);
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    uint32_t codepoint = 0x10000 + 
                        ((static_cast<uint32_t>(unit - 0xD800) << 10) | 
                         (low - 0xDC00));
                    appendCodepoint(result, codepoint);
                    i += 2;
                    continue;
                }
            }
            result += REPLACEMENT_CHAR;
        } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
            result += REPLACEMENT_CHAR;
        } else {
            appendCodepoint(result, unit);
        }
    }
    
    return result;
}


std::string UTF8Util::fromUTF16BOM(const uint8_t* data, size_t size) {
    if (!data || size < 2) {
        return "";
    }
    
    // Check for BOM
    if (data[0] == 0xFF && data[1] == 0xFE) {
        // Little-endian BOM
        return fromUTF16LE(data + 2, size - 2);
    } else if (data[0] == 0xFE && data[1] == 0xFF) {
        // Big-endian BOM
        return fromUTF16BE(data + 2, size - 2);
    } else {
        // No BOM - assume big-endian (ID3v2 spec default)
        return fromUTF16BE(data, size);
    }
}

std::vector<uint8_t> UTF8Util::toUTF16LE(const std::string& text) {
    std::vector<uint8_t> result;
    result.reserve(text.size() * 2);
    
    size_t i = 0;
    while (i < text.size()) {
        size_t consumed;
        uint32_t codepoint = decodeCodepoint(
            reinterpret_cast<const uint8_t*>(text.data()) + i,
            text.size() - i, consumed);
        i += consumed;
        
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

std::vector<uint8_t> UTF8Util::toUTF16BE(const std::string& text) {
    std::vector<uint8_t> result;
    result.reserve(text.size() * 2);
    
    size_t i = 0;
    while (i < text.size()) {
        size_t consumed;
        uint32_t codepoint = decodeCodepoint(
            reinterpret_cast<const uint8_t*>(text.data()) + i,
            text.size() - i, consumed);
        i += consumed;
        
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

std::vector<uint8_t> UTF8Util::toUTF16BOM(const std::string& text) {
    std::vector<uint8_t> result;
    result.reserve(text.size() * 2 + 2);
    
    // Add little-endian BOM
    result.push_back(0xFF);
    result.push_back(0xFE);
    
    auto encoded = toUTF16LE(text);
    result.insert(result.end(), encoded.begin(), encoded.end());
    
    return result;
}


// ============================================================================
// UTF-32 Conversion
// ============================================================================

std::string UTF8Util::fromUTF32LE(const uint8_t* data, size_t size) {
    if (!data || size < 4) {
        return "";
    }
    
    std::string result;
    result.reserve(size);
    
    for (size_t i = 0; i + 3 < size; i += 4) {
        uint32_t codepoint = static_cast<uint32_t>(data[i]) |
                            (static_cast<uint32_t>(data[i + 1]) << 8) |
                            (static_cast<uint32_t>(data[i + 2]) << 16) |
                            (static_cast<uint32_t>(data[i + 3]) << 24);
        
        if (codepoint == 0) {
            break;
        }
        
        if (isValidCodepoint(codepoint)) {
            appendCodepoint(result, codepoint);
        } else {
            result += REPLACEMENT_CHAR;
        }
    }
    
    return result;
}

std::string UTF8Util::fromUTF32BE(const uint8_t* data, size_t size) {
    if (!data || size < 4) {
        return "";
    }
    
    std::string result;
    result.reserve(size);
    
    for (size_t i = 0; i + 3 < size; i += 4) {
        uint32_t codepoint = (static_cast<uint32_t>(data[i]) << 24) |
                            (static_cast<uint32_t>(data[i + 1]) << 16) |
                            (static_cast<uint32_t>(data[i + 2]) << 8) |
                            static_cast<uint32_t>(data[i + 3]);
        
        if (codepoint == 0) {
            break;
        }
        
        if (isValidCodepoint(codepoint)) {
            appendCodepoint(result, codepoint);
        } else {
            result += REPLACEMENT_CHAR;
        }
    }
    
    return result;
}

std::vector<uint8_t> UTF8Util::toUTF32LE(const std::string& text) {
    std::vector<uint8_t> result;
    result.reserve(text.size() * 4);
    
    size_t i = 0;
    while (i < text.size()) {
        size_t consumed;
        uint32_t codepoint = decodeCodepoint(
            reinterpret_cast<const uint8_t*>(text.data()) + i,
            text.size() - i, consumed);
        i += consumed;
        
        result.push_back(static_cast<uint8_t>(codepoint & 0xFF));
        result.push_back(static_cast<uint8_t>((codepoint >> 8) & 0xFF));
        result.push_back(static_cast<uint8_t>((codepoint >> 16) & 0xFF));
        result.push_back(static_cast<uint8_t>((codepoint >> 24) & 0xFF));
    }
    
    return result;
}

std::vector<uint8_t> UTF8Util::toUTF32BE(const std::string& text) {
    std::vector<uint8_t> result;
    result.reserve(text.size() * 4);
    
    size_t i = 0;
    while (i < text.size()) {
        size_t consumed;
        uint32_t codepoint = decodeCodepoint(
            reinterpret_cast<const uint8_t*>(text.data()) + i,
            text.size() - i, consumed);
        i += consumed;
        
        result.push_back(static_cast<uint8_t>((codepoint >> 24) & 0xFF));
        result.push_back(static_cast<uint8_t>((codepoint >> 16) & 0xFF));
        result.push_back(static_cast<uint8_t>((codepoint >> 8) & 0xFF));
        result.push_back(static_cast<uint8_t>(codepoint & 0xFF));
    }
    
    return result;
}


// ============================================================================
// Codepoint and String Utilities
// ============================================================================

std::vector<uint32_t> UTF8Util::toCodepoints(const std::string& text) {
    std::vector<uint32_t> result;
    result.reserve(text.size()); // Upper bound
    
    size_t i = 0;
    while (i < text.size()) {
        size_t consumed;
        uint32_t cp = decodeCodepoint(
            reinterpret_cast<const uint8_t*>(text.data()) + i,
            text.size() - i, consumed);
        result.push_back(cp);
        i += consumed;
    }
    
    return result;
}

std::string UTF8Util::fromCodepoints(const std::vector<uint32_t>& codepoints) {
    std::string result;
    result.reserve(codepoints.size() * 4); // Upper bound
    
    for (uint32_t cp : codepoints) {
        appendCodepoint(result, cp);
    }
    
    return result;
}

size_t UTF8Util::length(const std::string& text) {
    size_t count = 0;
    size_t i = 0;
    
    while (i < text.size()) {
        size_t consumed;
        decodeCodepoint(
            reinterpret_cast<const uint8_t*>(text.data()) + i,
            text.size() - i, consumed);
        ++count;
        i += consumed;
    }
    
    return count;
}

size_t UTF8Util::findNullTerminator(const uint8_t* data, size_t size, size_t bytesPerUnit) {
    if (!data || size == 0) {
        return 0;
    }
    
    if (bytesPerUnit == 1) {
        // Single-byte null terminator
        for (size_t i = 0; i < size; ++i) {
            if (data[i] == 0x00) {
                return i;
            }
        }
    } else if (bytesPerUnit == 2) {
        // Double-byte null terminator (UTF-16)
        for (size_t i = 0; i + 1 < size; i += 2) {
            if (data[i] == 0x00 && data[i + 1] == 0x00) {
                return i;
            }
        }
    } else if (bytesPerUnit == 4) {
        // Quad-byte null terminator (UTF-32)
        for (size_t i = 0; i + 3 < size; i += 4) {
            if (data[i] == 0x00 && data[i + 1] == 0x00 && 
                data[i + 2] == 0x00 && data[i + 3] == 0x00) {
                return i;
            }
        }
    }
    
    return size;
}

} // namespace Utility
} // namespace Core
} // namespace PsyMP3

