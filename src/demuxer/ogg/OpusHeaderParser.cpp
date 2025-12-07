/*
 * OpusHeaderParser.cpp - Opus header parsing validation
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "demuxer/ogg/OpusHeaderParser.h"
#include <cstring>

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

OpusHeaderParser::OpusHeaderParser() 
    : m_headers_count(0) {
    m_info.codec_name = "Opus";
    m_info.rate = 48000; // Opus always outputs 48kHz
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
        
        // Pre-skip (byte 10-11)
        // Rate (byte 12-15) - informative
        // Gain (byte 16-17)
        // Mapping Family (byte 18)
        
        m_headers_count = 1;
        return true;
    }
    
    // Comment Header
    if (m_headers_count == 1) {
        if (memcmp(data, "OpusTags", 8) != 0) return false;
        
        // Vendor string (starts at 8)
        // Length (4 bytes)
        // String
        // User Comment list length (4 bytes)
        // ...
        
        // We just validate signature here
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

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3
