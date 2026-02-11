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
        size_t best_match_length = 0;
        size_t best_match_distance = 0;

        // Search in sliding window
        size_t search_start = (cursor > m_window_size) ? cursor - m_window_size : 0;
        
        for (size_t i = search_start; i < cursor; ++i) {
            size_t match_length = 0;
            // Check match length
            while (match_length < m_lookahead_buffer_size &&
                   (cursor + match_length) < size &&
                   data[i + match_length] == data[cursor + match_length]) {
                match_length++;
            }

            if (match_length > best_match_length) {
                best_match_length = match_length;
                best_match_distance = cursor - i;
            }
        }

        // Output either a literal or a (distance, length) pair
        // Format:
        // Flag bit (0 = literal, 1 = pair)
        // If 0: followed by 8-bit literal
        // If 1: followed by 12-bit distance and 4-bit length (16 bits total)
        // This is a simplified encoding for demonstration
        
        if (best_match_length >= 3) { // Only encode if worth it
            // Tag with length/distance indicator (we'll implement a simple byte-oriented format here)
            // Bit 7 set: Reference (Length: bits 4-6 + 3, Distance: bits 0-3 combined with next byte)
            // This is a naive custom format for simplicity
             
             // Let's use a simpler marker-based approach or block-based approach for robustness
             // Actually, let's implement the standard Deflate-style LZ77 pair logic but simplified
             
             // Custom Format:
             // [0iiiiiii] - Literal byte
             // [1dddddll] [dddddddd] - Pointer. Distance 12 bits, Length 2-bit extension + 3
             // Length = (byte1 & 0x03) + 3. Max length = 6.
             // Distance = ((byte1 & 0x7C) << 6) | byte2. Max distance 8191.
             
            // Re-evaluating limits:
            // Match len max = 15 + 3 = 18?
            // Distance max = 4095.
            
            // Let's use:
            // High bit 0: Literal
            // High bit 1: Reference
            // If Ref:
            // Byte 1: 1 dddd lll
            // Byte 2: dddddddd
            // Distance = (High 4 bits << 8) | Low 8 bits. Range 0-4095.
            // Length = Low 3 bits + 3. Range 3-10.
            
            size_t clamped_length = std::min(best_match_length, (size_t)10);
            
            uint8_t byte1 = 0x80;
            size_t distance = best_match_distance;
            size_t length_code = clamped_length - 3;
            
            byte1 |= ((distance >> 8) & 0x0F) << 3;
            byte1 |= (length_code & 0x07);
            
            uint8_t byte2 = distance & 0xFF;
            
            compressed_data.push_back(byte1);
            compressed_data.push_back(byte2);
            
            cursor += clamped_length;
        } else {
            // Literal
            // If the byte has high bit set, we might need escaping or a different scheme.
            // Simple scheme:
            // 0xxxxxxx - Literal 0-127
            // 1xxxxxxx - Reference start... wait, this restricts data range.
            
            // Standard simple LZ77 often uses a control byte followed by 8 literals/pointers.
            // Let's implement that. It's robust.
            
            // Current block implementation:
            // We'll accumulate up to 8 items, then flush.
            // Or just output simple packets.
            
            // Let's switch to the classic "Flag Byte" approach.
            // 1 buffer of flags, followed by data.
            // But streaming is hard.
            
            // Let's implement the "0 = Literal, 1 = Reference" scheme but assume we can't escape bytes easily without overhead.
            // Actually, the simplest self-contained format for this task:
            // Token based.
            // 0[7 bits] -> Literal? No, can't encode all bytes.
            
            // Robust Approach:
            // Marker byte? No, data can be anything.
            
            // Let's go with the control-byte-per-8-items approach.
            // It's standard and easy.
            
            // But `compress` returns a vector, so we can't easily jump back and patch header.
            // We can buffer small chunks.
        }
    }
    
    // For this implementation, let's restart with a clean, standard control-bit approach.
    // We will build the output stream linearly.
    
    compressed_data.clear();
    cursor = 0;
    
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
            // Check if we finished the group but hit EOF? 
            // The loop condition `cursor < size` protects reading data, but logic inside handles incomplete flags 
            // only if the stream ends exactly after valid data.
            
            // If the last flag bit implies data but we are at EOF, that's an error or just end.
            // But we should check carefully.
            
            if (flags & (1 << i)) {
                // Ref
                if (cursor + 1 >= size) break; // Error or truncated
                
                uint8_t b1 = data[cursor++];
                uint8_t b2 = data[cursor++];
                
                size_t distance = (static_cast<size_t>(b1) << 4) | (b2 >> 4);
                size_t length = (b2 & 0x0F) + 3;
                
                if (distance > output.size()) {
                    // Invalid backreference (underrun)
                   // throw std::runtime_error("Invalid LZ77 backreference distance");
                   // Using exceptions might crash fuzzer if not caught. 
                   // Let's define behavior: ignore or output zeros?
                   // Standard robust behavior: min with available size or clamp.
                   distance = output.size(); // Or 0?
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
            
            // If we've processed as many items as encoded in this block (implicitly handled by stream end)
            // Ideally we'd know how many items, but standard "8 flags" consumes data until done.
        }
    }

    return output;
}

} // namespace Compression
} // namespace Core
} // namespace PsyMP3
