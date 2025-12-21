/*
 * VorbisHeaderParser.cpp - Vorbis header parsing validation
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "demuxer/ogg/VorbisHeaderParser.h"
#include "debug.h"
#include <cstring>
#include <algorithm>
#include <cctype>

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

VorbisHeaderParser::VorbisHeaderParser() 
    : m_headers_count(0) {
    m_info.codec_name = "Vorbis";
}

bool VorbisHeaderParser::parseVorbisComment(const unsigned char* data, size_t size) {
    // VorbisComment format (after type byte and "vorbis" signature):
    // - vendor_length (4 bytes, little-endian)
    // - vendor_string (vendor_length bytes, UTF-8)
    // - user_comment_list_length (4 bytes, little-endian)
    // - For each comment:
    //   - comment_length (4 bytes, little-endian)
    //   - comment_string (comment_length bytes, UTF-8, format: "FIELD=value")
    // - framing_bit (1 byte, must have bit 0 set)
    
    size_t offset = 0;
    
    // Read vendor length
    if (offset + 4 > size) {
        Debug::log("ogg", "VorbisHeaderParser: comment header too short for vendor length");
        return false;
    }
    uint32_t vendor_length = data[offset] | (data[offset+1] << 8) | 
                             (data[offset+2] << 16) | (data[offset+3] << 24);
    offset += 4;
    
    // Read vendor string
    if (offset + vendor_length > size) {
        Debug::log("ogg", "VorbisHeaderParser: comment header too short for vendor string");
        return false;
    }
    m_comment.vendor = std::string(reinterpret_cast<const char*>(data + offset), vendor_length);
    offset += vendor_length;
    Debug::log("ogg", "VorbisHeaderParser: vendor='", m_comment.vendor, "'");
    
    // Read user comment list length
    if (offset + 4 > size) {
        Debug::log("ogg", "VorbisHeaderParser: comment header too short for comment count");
        return false;
    }
    uint32_t comment_count = data[offset] | (data[offset+1] << 8) | 
                             (data[offset+2] << 16) | (data[offset+3] << 24);
    offset += 4;
    Debug::log("ogg", "VorbisHeaderParser: comment_count=", comment_count);
    
    // Read each comment
    for (uint32_t i = 0; i < comment_count; ++i) {
        if (offset + 4 > size) {
            Debug::log("ogg", "VorbisHeaderParser: comment header truncated at comment ", i);
            break;  // Truncated, but we got some comments
        }
        
        uint32_t comment_length = data[offset] | (data[offset+1] << 8) | 
                                  (data[offset+2] << 16) | (data[offset+3] << 24);
        offset += 4;
        
        if (offset + comment_length > size) {
            Debug::log("ogg", "VorbisHeaderParser: comment ", i, " truncated");
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
            Debug::log("ogg", "VorbisHeaderParser: field '", field_name, "'='", field_value, "'");
        }
    }
    
    return true;
}

bool VorbisHeaderParser::parseHeader(ogg_packet* packet) {
    if (!packet || packet->bytes < 7) return false;
    
    unsigned char* data = packet->packet;
    
    // Common validation for all Vorbis headers: "vorbis" signature at offset 1
    if (memcmp(data + 1, "vorbis", 6) != 0) {
        return false;
    }
    
    int type = data[0];
    
    switch (type) {
        case 1: // ID Header
            if (m_headers_count != 0) return false; // Must be first
            if (packet->bytes < 30) return false; // Minimum size validation
            
            // Version (byte 7-10) must be 0
            if (data[7] != 0 || data[8] != 0 || data[9] != 0 || data[10] != 0) return false;
            
            // Channels (byte 11)
            m_info.channels = data[11];
            if (m_info.channels == 0) return false;
            
            // Sample Rate (byte 12-15) - Little Endian
            m_info.rate = data[12] | (data[13] << 8) | (data[14] << 16) | (data[15] << 24);
            if (m_info.rate == 0) return false;
            
            // Framing flag (last byte of packet check? Or defined bit?)
            // Spec says "framing_flag" found at end of ID header?
            // "1 bit [framing_flag] = 1"
            // The ID header is exactly defined.
            // But we typically trust if we got this far.
            if (!(data[29] & 1)) return false; // Formatting bit must be set
            
            m_headers_count = 1;
            return true;
            
        case 3: // Comment Header
            if (m_headers_count != 1) return false; // Must follow ID header (interleaved? No, usually ordered)
            // Ideally we accept mixed order? Spec mandates order: ID -> Comment -> Setup.
            
            // Parse VorbisComment data (starts after type byte and "vorbis" signature)
            if (packet->bytes > 7) {
                parseVorbisComment(data + 7, packet->bytes - 7);
            }
            
            m_headers_count = 2;
            return true;
            
        case 5: // Setup Header
            if (m_headers_count != 2) return false; // Must follow Comment header
            m_headers_count = 3;
            return true;
            
        default:
            return false; // Unknown header type
    }
}

bool VorbisHeaderParser::isHeadersComplete() const {
    return m_headers_count >= 3;
}

CodecInfo VorbisHeaderParser::getCodecInfo() const {
    return m_info;
}

OggVorbisComment VorbisHeaderParser::getVorbisComment() const {
    return m_comment;
}

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3
