/*
 * Base64.cpp - Base64 encoding/decoding implementation
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD
#include "core/utility/Base64.h"

namespace PsyMP3 {
namespace Core {
namespace Utility {

static const std::string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::vector<uint8_t> Base64::decode(const std::string& input) {
    std::vector<uint8_t> result;
    // Estimate size: 3 bytes for every 4 chars roughly
    result.reserve((input.size() * 3) / 4);
    
    uint32_t val = 0;
    int valb = -8;
    
    for (unsigned char c : input) {
        if (c == '=') break;
        
        size_t pos = base64_chars.find(c);
        if (pos == std::string::npos) continue; // Skip whitespace/invalid chars
        
        val = (val << 6) | pos;
        valb += 6;
        
        if (valb >= 0) {
            result.push_back((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    
    return result;
}

std::string Base64::encode(const std::vector<uint8_t>& data) {
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    ret.reserve((data.size() * 4 / 3) + 4);

    for (uint8_t byte : data) {
        char_array_3[i++] = byte;
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for(i = 0; (i <4) ; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for(j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; (j < i + 1); j++)
            ret += base64_chars[char_array_4[j]];

        while((i++ < 3))
            ret += '=';
    }

    return ret;
}

} // namespace Utility
} // namespace Core
} // namespace PsyMP3
