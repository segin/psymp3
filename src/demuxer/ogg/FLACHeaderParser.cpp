/*
 * FLACHeaderParser.cpp - FLAC header parsing validation
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill II <segin2005@gmail.com>
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

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

FLACHeaderParser::FLACHeaderParser() 
    : m_headers_count(0), m_streaminfo_found(false) {
    m_info.codec_name = "FLAC";
}

bool FLACHeaderParser::parseHeader(ogg_packet* packet) {
    if (!packet || packet->bytes < 13) return false; // Minimum Ogg FLAC encapsulation header size
    
    unsigned char* data = packet->packet;
    
    // Packet 0: ID Header + STREAMINFO
    if (m_headers_count == 0) {
        // Validation: 0x7f + "FLAC"
        if (memcmp(data, "\x7f" "FLAC", 5) != 0) return false;
        
        // Version 1.0 (major=1)
        if (data[5] != 1) return false;
        
        // "fLaC" signature (bytes 9-12)
        if (memcmp(data + 9, "fLaC", 4) != 0) return false;
        
        // Metadata Block(s) start at 13
        // The first block MUST be STREAMINFO (type 0)
        if (packet->bytes < 13 + 4) return false; // Header + Block Header
        
        unsigned char* block_header = data + 13;
        // bool is_last = (block_header[0] & 0x80) != 0;
        int type = block_header[0] & 0x7F;
        
        if (type != 0) return false; // First block must be STREAMINFO
        
        // Length (24-bit BE)
        int length = (block_header[1] << 16) | (block_header[2] << 8) | block_header[3];
        
        if (packet->bytes < 13 + 4 + length) return false; // Truncated packet
        
        // Parse STREAMINFO (34 bytes)
        if (length < 34) return false;
        unsigned char* streaminfo = block_header + 4;
        
        // Bytes 0-9: Block sizes, Frame sizes - skip
        // Bytes 10-12: Sample Rate (20), Channels (3), BPS (5), Total Samples (36)
        
        // Byte 10: RRRRRRRR
        // Byte 11: RRRRRRRR
        // Byte 12: RRRRCCCE
        // Byte 13: EEEE...
        // ...
        
        // Sample Rate: 20 bits (bytes 10, 11, top 4 of 12)
        long sample_rate = (streaminfo[10] << 12) | (streaminfo[11] << 4) | ((streaminfo[12] >> 4) & 0x0F);
        m_info.rate = sample_rate;
        if (m_info.rate == 0) return false;
        
        // Channels: 3 bits (bits 1-3 of byte 12) -> (streaminfo[12] >> 1) & 0x07
        int channels_minus_1 = (streaminfo[12] >> 1) & 0x07;
        m_info.channels = channels_minus_1 + 1;
        
        // Bits Per Sample: 5 bits (bit 0 of 12, top 4 of 13)
        // int bps_minus_1 = ((streaminfo[12] & 0x01) << 4) | ((streaminfo[13] >> 4) & 0x0F);
        
        m_streaminfo_found = true;
        m_headers_count = 1;
        
        // If there are more metadata blocks in this packet, we could parse them
        // But for now we just needed STREAMINFO.
        return true;
    }
    
    // Subsequent packets might be comment blocks etc.
    // Ogg FLAC usually puts all metadata in the first few packets.
    // We just return true to accept them until audio starts.
    // How do we know audio started?
    // Audio packets don't have Ogg FLAC header signature?
    // RFC 3533: "The first packet of a logical bitstream... ID header"
    // "Subsequent pages... audio".
    // Wait, Ogg FLAC audio packets are just FLAC frames?
    // Yes.
    // We only validate the first header packet here for identification.
    
    m_headers_count++;
    return true;
}

bool FLACHeaderParser::isHeadersComplete() const {
    return m_streaminfo_found; // Technically we might wait for Comments, but STREAMINFO is minimal
}

CodecInfo FLACHeaderParser::getCodecInfo() const {
    return m_info;
}

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3
