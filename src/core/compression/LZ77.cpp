/*
 * LZ77.cpp - Lempel-Ziv '77 compression implementation
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill <segin2005@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "core/compression/LZ77.h"
#include <algorithm>
#include <stdexcept>

namespace PsyMP3 {
namespace Core {
namespace Compression {

LZ77Compressor::LZ77Compressor(size_t window_size, size_t lookahead_buffer_size)
    : m_window_size(window_size), m_lookahead_buffer_size(lookahead_buffer_size) {
    if (m_window_size > 4095) m_window_size = 4095; // Limit for this simple implementation
    if (m_lookahead_buffer_size > 15) m_lookahead_buffer_size = 15; // Limit for encoding
}

std::vector<uint8_t> LZ77Compressor::compress(const uint8_t* data, size_t size) {
    std::vector<uint8_t> compressed_data;
    size_t cursor = 0;
    
    while (cursor < size) {
        // Collect encoded tokens for a group of 8
        uint8_t flags = 0;
        std::vector<uint8_t> group_data;
        
        for (int i = 0; i < 8 && cursor < size; ++i) {
            size_t best_match_length = 0;
            size_t best_match_distance = 0;

            // Search window
            size_t search_start = (cursor > m_window_size) ? cursor - m_window_size : 0;
            for (size_t j = search_start; j < cursor; ++j) {
                size_t match_length = 0;
                while (match_length < m_lookahead_buffer_size &&
                       (cursor + match_length) < size &&
                       data[j + match_length] == data[cursor + match_length]) {
                    match_length++;
                }
                if (match_length > best_match_length) {
                    best_match_length = match_length;
                    best_match_distance = cursor - j;
                }
            }

            if (best_match_length >= 3) {
                // Pointer (Flag = 1)
                // Encoding: <Distance:12><Length:4>
                // Distance 0-4095
                // Length 3-18 (code 0-15 + 3)
                
                size_t encode_len = std::min(best_match_length, (size_t)18);
                size_t len_code = encode_len - 3;
                size_t dist = best_match_distance;
                
                uint8_t b1 = (dist >> 4) & 0xFF;
                uint8_t b2 = ((dist & 0x0F) << 4) | (len_code & 0x0F);
                
                group_data.push_back(b1);
                group_data.push_back(b2);
                
                flags |= (1 << i);
                cursor += encode_len;
            } else {
                // Literal (Flag = 0)
                group_data.push_back(data[cursor]);
                cursor++;
            }
        }
        
        compressed_data.push_back(flags);
        compressed_data.insert(compressed_data.end(), group_data.begin(), group_data.end());
    }

    return compressed_data;
}

std::vector<uint8_t> LZ77Decompressor::decompress(const uint8_t* data, size_t size) {
    std::vector<uint8_t> output;
    size_t cursor = 0;

    while (cursor < size) {
        if (cursor >= size) break;
        uint8_t flags = data[cursor++];
        
        for (int i = 0; i < 8 && cursor < size; ++i) {
            if (flags & (1 << i)) {
                // Ref
                if (cursor + 1 >= size) break; // Error or truncated
                
                uint8_t b1 = data[cursor++];
                uint8_t b2 = data[cursor++];
                
                size_t distance = (static_cast<size_t>(b1) << 4) | (b2 >> 4);
                size_t length = (b2 & 0x0F) + 3;
                
                if (distance > output.size()) {
                    // Invalid backreference (underrun)
                   // Clamp to valid range or ignore
                   distance = output.size();
                }
                
                if (distance == 0) {
                     // Degenerate case, skip or insert nothing
                } else {
                    size_t start = output.size() - distance;
                    for (size_t j = 0; j < length; ++j) {
                        output.push_back(output[start + j]); // Allows overlapping copy naturally
                    }
                }
            } else {
                // Literal
                if (cursor >= size) break; // Should effectively not happen if logic correct
                output.push_back(data[cursor++]);
            }
        }
    }

    return output;
}

} // namespace Compression
} // namespace Core
} // namespace PsyMP3
