/*
 * SpeexHeaderParser.cpp - Speex header parsing validation
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
#include "demuxer/ogg/SpeexHeaderParser.h"

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

SpeexHeaderParser::SpeexHeaderParser() 
    : m_headers_count(0) {
    m_info.codec_name = "Speex";
}

bool SpeexHeaderParser::parseHeader(ogg_packet* packet) {
    if (!packet || packet->bytes < 8) return false;
    
    unsigned char* data = packet->packet;
    
    // Packet 0: ID Header "Speex   "
    if (m_headers_count == 0) {
        if (packet->bytes < 80) return false; // Minimum Speex header size
        
        if (memcmp(data, "Speex   ", 8) != 0) return false;
        
        // Rate (36-39) - LE
        m_info.rate = data[36] | (data[37] << 8) | (data[38] << 16) | (data[39] << 24);
        
        // Nb Channels (48-51) - LE
        m_info.channels = data[48] | (data[49] << 8) | (data[50] << 16) | (data[51] << 24);
        
        if (m_info.rate == 0) return false;
        
        m_headers_count = 1;
        return true;
    }
    
    // Packet 1: Comment Header
    // No specific signature? Usually Vorbis-style comments
    // Just accept it.
    if (m_headers_count == 1) {
        m_headers_count = 2;
        return true;
    }
    
    // Packet 2 onwards: Audio
    return false; 
}

bool SpeexHeaderParser::isHeadersComplete() const {
    return m_headers_count >= 2;
}

CodecInfo SpeexHeaderParser::getCodecInfo() const {
    return m_info;
}

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3
