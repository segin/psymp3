/*
 * UTF8Util.h - Universal UTF-8 encoding/decoding utilities
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

#ifndef PSYMP3_CORE_UTILITY_UTF8UTIL_H
#define PSYMP3_CORE_UTILITY_UTF8UTIL_H

#include <cstdint>
#include <string>
#include <vector>

namespace PsyMP3 {
namespace Core {
namespace Utility {

/**
 * @brief Universal UTF-8 encoding/decoding utilities
 * 
 * This class provides centralized Unicode handling for the entire PsyMP3
 * application. All text in PsyMP3 is internally represented as UTF-8.
 * 
 * Supported conversions:
 * - ISO-8859-1 (Latin-1) <-> UTF-8
 * - UTF-16 (LE/BE/BOM) <-> UTF-8
 * - UTF-32 <-> UTF-8
 * - Codepoint <-> UTF-8
 * 
 * Thread Safety: All methods are stateless and thread-safe.
 */
class UTF8Util {
public:
    // ========================================================================
    // UTF-8 Validation
    // ========================================================================
    
    /**
     * @brief Check if a string is valid UTF-8
     * @param text String to validate
     * @return true if valid UTF-8, false otherwise
     */
    static bool isValid(const std::string& text);
    
    /**
     * @brief Check if a byte sequence is valid UTF-8
     * @param data Pointer to data
     * @param size Size of data
     * @return true if valid UTF-8, false otherwise
     */
    static bool isValid(const uint8_t* data, size_t size);
    
    /**
     * @brief Repair invalid UTF-8 sequences
     * 
     * Replaces invalid sequences with U+FFFD (replacement character)
     * 
     * @param text String to repair
     * @return Repaired UTF-8 string
     */
    static std::string repair(const std::string& text);
    
    /**
     * @brief Decode UTF-8 with error handling
     * 
     * @param data Pointer to UTF-8 data
     * @param size Size of data
     * @return UTF-8 string with invalid sequences replaced
     */
    static std::string decodeSafe(const uint8_t* data, size_t size);
    
    // ========================================================================
    // ISO-8859-1 (Latin-1) Conversion
    // ========================================================================
    
    /**
     * @brief Decode ISO-8859-1 (Latin-1) to UTF-8
     * 
     * @param data Pointer to Latin-1 data
     * @param size Size of data
     * @return UTF-8 encoded string
     */
    static std::string fromLatin1(const uint8_t* data, size_t size);
    
    /**
     * @brief Decode ISO-8859-1 string to UTF-8
     * 
     * @param text Latin-1 encoded string
     * @return UTF-8 encoded string
     */
    static std::string fromLatin1(const std::string& text);
    
    /**
     * @brief Encode UTF-8 to ISO-8859-1 (Latin-1)
     * 
     * Characters outside Latin-1 range are replaced with '?'
     * 
     * @param text UTF-8 string
     * @return Latin-1 encoded bytes
     */
    static std::vector<uint8_t> toLatin1(const std::string& text);
    
    // ========================================================================
    // UTF-16 Conversion
    // ========================================================================
    
    /**
     * @brief Decode UTF-16 Little Endian to UTF-8
     * 
     * @param data Pointer to UTF-16LE data
     * @param size Size of data in bytes
     * @return UTF-8 encoded string
     */
    static std::string fromUTF16LE(const uint8_t* data, size_t size);
    
    /**
     * @brief Decode UTF-16 Big Endian to UTF-8
     * 
     * @param data Pointer to UTF-16BE data
     * @param size Size of data in bytes
     * @return UTF-8 encoded string
     */
    static std::string fromUTF16BE(const uint8_t* data, size_t size);
    
    /**
     * @brief Decode UTF-16 with BOM to UTF-8
     * 
     * Automatically detects byte order from BOM. If no BOM present,
     * defaults to big-endian.
     * 
     * @param data Pointer to UTF-16 data (may start with BOM)
     * @param size Size of data in bytes
     * @return UTF-8 encoded string
     */
    static std::string fromUTF16BOM(const uint8_t* data, size_t size);
    
    /**
     * @brief Encode UTF-8 to UTF-16 Little Endian
     * 
     * @param text UTF-8 string
     * @return UTF-16LE encoded bytes (no BOM)
     */
    static std::vector<uint8_t> toUTF16LE(const std::string& text);
    
    /**
     * @brief Encode UTF-8 to UTF-16 Big Endian
     * 
     * @param text UTF-8 string
     * @return UTF-16BE encoded bytes (no BOM)
     */
    static std::vector<uint8_t> toUTF16BE(const std::string& text);
    
    /**
     * @brief Encode UTF-8 to UTF-16 with BOM
     * 
     * Uses little-endian byte order with BOM prefix.
     * 
     * @param text UTF-8 string
     * @return UTF-16 encoded bytes with BOM
     */
    static std::vector<uint8_t> toUTF16BOM(const std::string& text);
    
    // ========================================================================
    // UTF-32 Conversion
    // ========================================================================
    
    /**
     * @brief Decode UTF-32 Little Endian to UTF-8
     * 
     * @param data Pointer to UTF-32LE data
     * @param size Size of data in bytes (must be multiple of 4)
     * @return UTF-8 encoded string
     */
    static std::string fromUTF32LE(const uint8_t* data, size_t size);
    
    /**
     * @brief Decode UTF-32 Big Endian to UTF-8
     * 
     * @param data Pointer to UTF-32BE data
     * @param size Size of data in bytes (must be multiple of 4)
     * @return UTF-8 encoded string
     */
    static std::string fromUTF32BE(const uint8_t* data, size_t size);
    
    /**
     * @brief Encode UTF-8 to UTF-32 Little Endian
     * 
     * @param text UTF-8 string
     * @return UTF-32LE encoded bytes
     */
    static std::vector<uint8_t> toUTF32LE(const std::string& text);
    
    /**
     * @brief Encode UTF-8 to UTF-32 Big Endian
     * 
     * @param text UTF-8 string
     * @return UTF-32BE encoded bytes
     */
    static std::vector<uint8_t> toUTF32BE(const std::string& text);
    
    // ========================================================================
    // Codepoint Operations
    // ========================================================================
    
    /**
     * @brief Encode a single Unicode codepoint to UTF-8
     * 
     * @param codepoint Unicode codepoint (U+0000 to U+10FFFF)
     * @return UTF-8 encoded string (1-4 bytes)
     */
    static std::string encodeCodepoint(uint32_t codepoint);
    
    /**
     * @brief Decode first codepoint from UTF-8 string
     * 
     * @param text UTF-8 string
     * @param bytesConsumed Output: number of bytes consumed
     * @return Unicode codepoint, or 0xFFFD on error
     */
    static uint32_t decodeCodepoint(const std::string& text, size_t& bytesConsumed);
    
    /**
     * @brief Decode first codepoint from UTF-8 data
     * 
     * @param data Pointer to UTF-8 data
     * @param size Size of data
     * @param bytesConsumed Output: number of bytes consumed
     * @return Unicode codepoint, or 0xFFFD on error
     */
    static uint32_t decodeCodepoint(const uint8_t* data, size_t size, size_t& bytesConsumed);
    
    /**
     * @brief Get all codepoints from UTF-8 string
     * 
     * @param text UTF-8 string
     * @return Vector of Unicode codepoints
     */
    static std::vector<uint32_t> toCodepoints(const std::string& text);
    
    /**
     * @brief Create UTF-8 string from codepoints
     * 
     * @param codepoints Vector of Unicode codepoints
     * @return UTF-8 encoded string
     */
    static std::string fromCodepoints(const std::vector<uint32_t>& codepoints);
    
    // ========================================================================
    // String Utilities
    // ========================================================================
    
    /**
     * @brief Count the number of Unicode characters in UTF-8 string
     * 
     * @param text UTF-8 string
     * @return Number of Unicode characters (not bytes)
     */
    static size_t length(const std::string& text);
    
    /**
     * @brief Find null terminator position in encoded text
     * 
     * @param data Pointer to text data
     * @param size Size of data
     * @param bytesPerUnit 1 for UTF-8/Latin-1, 2 for UTF-16, 4 for UTF-32
     * @return Position of null terminator, or size if not found
     */
    static size_t findNullTerminator(const uint8_t* data, size_t size, size_t bytesPerUnit = 1);
    
    /**
     * @brief Check if codepoint is valid Unicode
     * 
     * @param codepoint Codepoint to check
     * @return true if valid (U+0000-U+10FFFF, excluding surrogates)
     */
    static bool isValidCodepoint(uint32_t codepoint);
    
    /**
     * @brief Get the replacement character (U+FFFD) as UTF-8
     * 
     * @return UTF-8 encoded replacement character
     */
    static const std::string& replacementCharacter();
    
private:
    // Internal helper to encode codepoint to output
    static void appendCodepoint(std::string& output, uint32_t codepoint);
};

} // namespace Utility
} // namespace Core
} // namespace PsyMP3

#endif // PSYMP3_CORE_UTILITY_UTF8UTIL_H

