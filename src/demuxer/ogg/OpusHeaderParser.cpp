/*
 * OpusHeaderParser.cpp - Opus header parsing validation
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill <segin2005@gmail.com>
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

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD
#include "demuxer/ogg/OpusHeaderParser.h"
#include "debug.h"
#include <cstring>
#include <algorithm>
#include <cctype>

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

OpusHeaderParser::OpusHeaderParser() 
    : m_headers_count(0) {
    m_info.codec_name = "Opus";
    m_info.rate = 48000; // Opus always outputs 48kHz
}

bool OpusHeaderParser::parseOpusTags(const unsigned char* data, size_t size) {
    // OpusTags format (after "OpusTags" signature):
    // - vendor_length (4 bytes, little-endian)
    // - vendor_string (vendor_length bytes, UTF-8)
    // - user_comment_list_length (4 bytes, little-endian)
    // - For each comment:
    //   - comment_length (4 bytes, little-endian)
    //   - comment_string (comment_length bytes, UTF-8, format: "FIELD=value")
    
    size_t offset = 0;
    
    // Read vendor length
    if (offset + 4 > size) {
        Debug::log("ogg", "OpusHeaderParser: OpusTags too short for vendor length");
        return false;
    }
    uint32_t vendor_length = data[offset] | (data[offset+1] << 8) | 
                             (data[offset+2] << 16) | (data[offset+3] << 24);
    offset += 4;
    
    // Resource limit check
    if (vendor_length > 1024 * 1024) { // 1MB limit
        Debug::log("ogg", "OpusHeaderParser: vendor_length too large: ", vendor_length);
        return false;
    }
    
    // Read vendor string
    if (offset + vendor_length > size) {
        Debug::log("ogg", "OpusHeaderParser: OpusTags too short for vendor string");
        return false;
    }
    m_comment.vendor = std::string(reinterpret_cast<const char*>(data + offset), vendor_length);
    offset += vendor_length;
    Debug::log("ogg", "OpusHeaderParser: vendor='", m_comment.vendor, "'");
    
    // Read user comment list length
    if (offset + 4 > size) {
        Debug::log("ogg", "OpusHeaderParser: OpusTags too short for comment count");
        return false;
    }
    uint32_t comment_count = data[offset] | (data[offset+1] << 8) | 
                             (data[offset+2] << 16) | (data[offset+3] << 24);
    offset += 4;
    
    // Resource limit check
    if (comment_count > 10000) {
        Debug::log("ogg", "OpusHeaderParser: too many comments: ", comment_count);
        return false;
    }
    
    Debug::log("ogg", "OpusHeaderParser: comment_count=", comment_count);
    
    // Read each comment
    for (uint32_t i = 0; i < comment_count; ++i) {
        if (offset + 4 > size) {
            Debug::log("ogg", "OpusHeaderParser: OpusTags truncated at comment ", i);
            break;  // Truncated, but we got some comments
        }
        
        uint32_t comment_length = data[offset] | (data[offset+1] << 8) | 
                                  (data[offset+2] << 16) | (data[offset+3] << 24);
        offset += 4;
        
        // Resource limit check
        if (comment_length > 1024 * 1024) { // 1MB limit
            Debug::log("ogg", "OpusHeaderParser: comment_length too large: ", comment_length);
            return false;
        }
        
        if (offset + comment_length > size) {
            Debug::log("ogg", "OpusHeaderParser: comment ", i, " truncated");
            break;  // Truncated
        }
        
        std::string comment(reinterpret_cast<const char*>(data + offset), comment_length);
        offset += comment_length;
        
        // Parse "FIELD=value" format
        size_t eq_pos = comment.find('=');
        if (eq_pos != std::string::npos && eq_pos > 0) {
            std::string field_name = comment.substr(0, eq_pos);
            std::string field_value = comment.substr(eq_pos + 1);
            
            // Normalize field name to uppercase (VorbisComment is case-insensitive)
            std::transform(field_name.begin(), field_name.end(), field_name.begin(),
                          [](unsigned char c) { return std::toupper(c); });
            
            // Add to multi-valued map
            m_comment.fields[field_name].push_back(field_value);
            Debug::log("ogg", "OpusHeaderParser: field '", field_name, "'='", field_value, "'");
        }
    }
    
    return true;
}

bool OpusHeaderParser::parseHeader(ogg_packet* packet) {
    if (!packet || packet->bytes < 8) return false;
    
    unsigned char* data = packet->packet;
    
    // Check for "OpusHead" or "OpusTags"
    
    // ID Header
    if (m_headers_count == 0) {
        if (packet->bytes < 19) return false; // Minimum size
        if (memcmp(data, "OpusHead", 8) != 0) return false;
        
        // Version (byte 8) must be 1
        if (data[8] != 1) return false;
        
        // Channels (byte 9)
        m_info.channels = data[9];
        if (m_info.channels == 0) return false;
        
        // Pre-skip (byte 10-11, little-endian)
        m_info.pre_skip = data[10] | (data[11] << 8);
        Debug::log("ogg", "OpusHeaderParser: pre_skip=", m_info.pre_skip);
        
        // Rate (byte 12-15) - informative
        // Gain (byte 16-17)
        // Mapping Family (byte 18)
        
        m_headers_count = 1;
        return true;
    }
    
    // Comment Header
    if (m_headers_count == 1) {
        if (memcmp(data, "OpusTags", 8) != 0) return false;
        
        // Parse VorbisComment data (starts after "OpusTags" signature)
        if (packet->bytes > 8) {
            parseOpusTags(data + 8, packet->bytes - 8);
        }
        
        m_headers_count = 2;
        return true;
    }
    
    return false; // Not expecting more headers (Opus only has 2 compulsory, rest are aux?)
    // Actually RFC 7845 says "The first page contains the identification header... The second page contains the comment header... All subsequent pages contain audio data."
    // So only 2 headers are defined as *metadata headers*.
}

bool OpusHeaderParser::isHeadersComplete() const {
    return m_headers_count >= 2;
}

CodecInfo OpusHeaderParser::getCodecInfo() const {
    return m_info;
}

OggVorbisComment OpusHeaderParser::getVorbisComment() const {
    return m_comment;
}

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3
