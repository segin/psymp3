/*
 * ID3v2Utils.h - ID3v2 utility functions
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef PSYMP3_TAG_ID3V2UTILS_H
#define PSYMP3_TAG_ID3V2UTILS_H

#include <cstdint>
#include <string>
#include <vector>

namespace PsyMP3 {
namespace Tag {
namespace ID3v2Utils {

/**
 * @brief ID3v2 text encoding types
 */
enum class TextEncoding : uint8_t {
    ISO_8859_1 = 0,  // Latin-1
    UTF_16_BOM = 1,  // UTF-16 with BOM
    UTF_16_BE = 2,   // UTF-16 Big Endian (no BOM)
    UTF_8 = 3        // UTF-8
};

// ============================================================================
// Synchsafe Integer Functions
// ============================================================================

/**
 * @brief Encode a 28-bit value as a synchsafe integer
 * 
 * Synchsafe integers are used in ID3v2 to avoid false sync patterns.
 * Each byte uses only 7 bits (MSB is always 0), allowing 28 bits
 * of data to be stored in 4 bytes.
 * 
 * @param value Value to encode (must be <= 0x0FFFFFFF)
 * @return 4-byte synchsafe encoded value
 */
uint32_t encodeSynchsafe(uint32_t value);

/**
 * @brief Decode a synchsafe integer to a regular 28-bit value
 * 
 * @param synchsafe 4-byte synchsafe encoded value
 * @return Decoded value (28-bit)
 */
uint32_t decodeSynchsafe(uint32_t synchsafe);

/**
 * @brief Decode synchsafe integer from raw bytes
 * 
 * @param data Pointer to 4 bytes of synchsafe data
 * @return Decoded value (28-bit)
 */
uint32_t decodeSynchsafeBytes(const uint8_t* data);

/**
 * @brief Encode synchsafe integer to raw bytes
 * 
 * @param value Value to encode (must be <= 0x0FFFFFFF)
 * @param out Output buffer (must be at least 4 bytes)
 */
void encodeSynchsafeBytes(uint32_t value, uint8_t* out);

/**
 * @brief Check if a value can be encoded as synchsafe (fits in 28 bits)
 * 
 * @param value Value to check
 * @return true if value <= 0x0FFFFFFF
 */
bool canEncodeSynchsafe(uint32_t value);

// ============================================================================
// Text Encoding Functions
// ============================================================================

/**
 * @brief Decode ID3v2 text data to UTF-8 string
 * 
 * Handles all ID3v2 text encodings:
 * - 0x00: ISO-8859-1 (Latin-1)
 * - 0x01: UTF-16 with BOM
 * - 0x02: UTF-16 Big Endian (no BOM)
 * - 0x03: UTF-8
 * 
 * @param data Pointer to text data (after encoding byte)
 * @param size Size of text data
 * @param encoding Text encoding type
 * @return UTF-8 encoded string
 */
std::string decodeText(const uint8_t* data, size_t size, TextEncoding encoding);

/**
 * @brief Decode ID3v2 text data with encoding byte prefix
 * 
 * @param data Pointer to data starting with encoding byte
 * @param size Total size including encoding byte
 * @return UTF-8 encoded string
 */
std::string decodeTextWithEncoding(const uint8_t* data, size_t size);

/**
 * @brief Encode UTF-8 string to ID3v2 text format
 * 
 * @param text UTF-8 string to encode
 * @param encoding Target encoding
 * @return Encoded bytes (without encoding byte prefix)
 */
std::vector<uint8_t> encodeText(const std::string& text, TextEncoding encoding);

/**
 * @brief Encode UTF-8 string with encoding byte prefix
 * 
 * @param text UTF-8 string to encode
 * @param encoding Target encoding
 * @return Encoded bytes with encoding byte prefix
 */
std::vector<uint8_t> encodeTextWithEncoding(const std::string& text, TextEncoding encoding);

/**
 * @brief Get the null terminator size for a given encoding
 * 
 * @param encoding Text encoding type
 * @return 1 for ISO-8859-1/UTF-8, 2 for UTF-16 variants
 */
size_t getNullTerminatorSize(TextEncoding encoding);

/**
 * @brief Find null terminator position in encoded text
 * 
 * @param data Pointer to text data
 * @param size Size of data
 * @param encoding Text encoding type
 * @return Position of null terminator, or size if not found
 */
size_t findNullTerminator(const uint8_t* data, size_t size, TextEncoding encoding);

/**
 * @brief Decode ISO-8859-1 (Latin-1) to UTF-8
 * 
 * @param data Pointer to Latin-1 data
 * @param size Size of data
 * @return UTF-8 encoded string
 */
std::string decodeISO8859_1(const uint8_t* data, size_t size);

/**
 * @brief Encode UTF-8 to ISO-8859-1 (Latin-1)
 * 
 * Characters outside Latin-1 range are replaced with '?'
 * 
 * @param text UTF-8 string
 * @return Latin-1 encoded bytes
 */
std::vector<uint8_t> encodeISO8859_1(const std::string& text);

/**
 * @brief Decode UTF-16 (with BOM) to UTF-8
 * 
 * @param data Pointer to UTF-16 data (may start with BOM)
 * @param size Size of data in bytes
 * @return UTF-8 encoded string
 */
std::string decodeUTF16_BOM(const uint8_t* data, size_t size);

/**
 * @brief Decode UTF-16 Big Endian to UTF-8
 * 
 * @param data Pointer to UTF-16BE data
 * @param size Size of data in bytes
 * @return UTF-8 encoded string
 */
std::string decodeUTF16_BE(const uint8_t* data, size_t size);

/**
 * @brief Decode UTF-16 Little Endian to UTF-8
 * 
 * @param data Pointer to UTF-16LE data
 * @param size Size of data in bytes
 * @return UTF-8 encoded string
 */
std::string decodeUTF16_LE(const uint8_t* data, size_t size);

/**
 * @brief Encode UTF-8 to UTF-16 with BOM
 * 
 * @param text UTF-8 string
 * @return UTF-16 encoded bytes with BOM
 */
std::vector<uint8_t> encodeUTF16_BOM(const std::string& text);

/**
 * @brief Encode UTF-8 to UTF-16 Big Endian
 * 
 * @param text UTF-8 string
 * @return UTF-16BE encoded bytes
 */
std::vector<uint8_t> encodeUTF16_BE(const std::string& text);

// ============================================================================
// Unsynchronization Functions
// ============================================================================

/**
 * @brief Decode unsynchronized data
 * 
 * ID3v2 unsynchronization replaces 0xFF 0x00 with 0xFF to avoid
 * false MPEG sync patterns. This function reverses that process.
 * 
 * @param data Pointer to unsynchronized data
 * @param size Size of data
 * @return Decoded data with 0xFF 0x00 sequences restored to 0xFF
 */
std::vector<uint8_t> decodeUnsync(const uint8_t* data, size_t size);

/**
 * @brief Encode data with unsynchronization
 * 
 * Replaces 0xFF followed by 0x00 or any byte >= 0xE0 with 0xFF 0x00 <byte>
 * 
 * @param data Pointer to data
 * @param size Size of data
 * @return Unsynchronized data
 */
std::vector<uint8_t> encodeUnsync(const uint8_t* data, size_t size);

/**
 * @brief Check if data needs unsynchronization
 * 
 * @param data Pointer to data
 * @param size Size of data
 * @return true if data contains patterns that need unsynchronization
 */
bool needsUnsync(const uint8_t* data, size_t size);

// ============================================================================
// UTF-8 Validation and Repair
// ============================================================================

/**
 * @brief Validate UTF-8 string
 * 
 * @param text String to validate
 * @return true if valid UTF-8
 */
bool isValidUTF8(const std::string& text);

/**
 * @brief Repair invalid UTF-8 sequences
 * 
 * Replaces invalid sequences with U+FFFD (replacement character)
 * 
 * @param text String to repair
 * @return Repaired UTF-8 string
 */
std::string repairUTF8(const std::string& text);

/**
 * @brief Decode UTF-8 with error handling
 * 
 * @param data Pointer to UTF-8 data
 * @param size Size of data
 * @return UTF-8 string with invalid sequences replaced
 */
std::string decodeUTF8Safe(const uint8_t* data, size_t size);

} // namespace ID3v2Utils
} // namespace Tag
} // namespace PsyMP3

#endif // PSYMP3_TAG_ID3V2UTILS_H

