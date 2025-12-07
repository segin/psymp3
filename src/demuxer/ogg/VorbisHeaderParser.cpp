/*
 * VorbisHeaderParser.cpp - Vorbis header parsing validation
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "demuxer/ogg/VorbisHeaderParser.h"
#include <cstring>
#include <iostream>

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

VorbisHeaderParser::VorbisHeaderParser() 
    : m_headers_count(0) {
    m_info.codec_name = "Vorbis";
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

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3
