/*
 * Base64.h - Base64 encoding/decoding utility
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef PSYMP3_CORE_UTILITY_BASE64_H
#define PSYMP3_CORE_UTILITY_BASE64_H

#include <string>
#include <vector>
#include <cstdint>

namespace PsyMP3 {
namespace Core {
namespace Utility {

class Base64 {
public:
    /**
     * @brief Decode a Base64 encoded string
     * 
     * @param input Base64 encoded string
     * @return Decoded binary data
     */
    static std::vector<uint8_t> decode(const std::string& input);

    /**
     * @brief Encode binary data to Base64 string
     * 
     * @param data Binary data to encode
     * @return Base64 encoded string
     */
    static std::string encode(const std::vector<uint8_t>& data);
};

} // namespace Utility
} // namespace Core
} // namespace PsyMP3

#endif // PSYMP3_CORE_UTILITY_BASE64_H
