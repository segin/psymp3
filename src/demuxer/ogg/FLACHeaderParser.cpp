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

#include "demuxer/ogg/FLACHeaderParser.h"
#include <cstring>
#include <algorithm>
#include <cctype>

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

FLACHeaderParser::FLACHeaderParser()
    : m_headers_count(0), m_streaminfo_found(false),
      m_expected_headers(0), m_last_metadata_seen(false) {
    m_info.codec_name = "FLAC";
}

bool FLACHeaderParser::parseVorbisCommentBlock(const unsigned char* data, size_t size) {
    // FLAC VORBIS_COMMENT block body (RFC 9639 Section 8.6): identical to the
    // Vorbis comment payload but WITHOUT the trailing framing bit. All length
    // fields are little-endian.
    size_t offset = 0;

    if (offset + 4 > size) {
        Debug::log("ogg", "FLACHeaderParser: comment block too short for vendor length");
        return false;
    }
    uint32_t vendor_length = data[offset] | (data[offset+1] << 8) |
                             (data[offset+2] << 16) | (static_cast<uint32_t>(data[offset+3]) << 24);
    offset += 4;

    if (vendor_length > 1024 * 1024) {
        Debug::log("ogg", "FLACHeaderParser: vendor_length too large: ", vendor_length);
        return false;
    }
    if (offset + vendor_length > size) {
        Debug::log("ogg", "FLACHeaderParser: comment block too short for vendor string");
        return false;
    }
    m_comment.vendor = std::string(reinterpret_cast<const char*>(data + offset), vendor_length);
    offset += vendor_length;

    if (offset + 4 > size) {
        Debug::log("ogg", "FLACHeaderParser: comment block too short for comment count");
        return false;
    }
    uint32_t comment_count = data[offset] | (data[offset+1] << 8) |
                             (data[offset+2] << 16) | (static_cast<uint32_t>(data[offset+3]) << 24);
    offset += 4;

    if (comment_count > 10000) {
        Debug::log("ogg", "FLACHeaderParser: too many comments: ", comment_count);
        return false;
    }

    for (uint32_t i = 0; i < comment_count; ++i) {
        if (offset + 4 > size) break; // Truncated, keep what we have
        uint32_t comment_length = data[offset] | (data[offset+1] << 8) |
                                  (data[offset+2] << 16) | (static_cast<uint32_t>(data[offset+3]) << 24);
        offset += 4;

        if (comment_length > 1024 * 1024) {
            Debug::log("ogg", "FLACHeaderParser: comment_length too large: ", comment_length);
            return false;
        }
        if (offset + comment_length > size) break; // Truncated

        std::string comment(reinterpret_cast<const char*>(data + offset), comment_length);
        offset += comment_length;

        size_t eq_pos = comment.find('=');
        if (eq_pos != std::string::npos && eq_pos > 0) {
            std::string field_name = comment.substr(0, eq_pos);
            std::string field_value = comment.substr(eq_pos + 1);
            std::transform(field_name.begin(), field_name.end(), field_name.begin(),
                          [](unsigned char c) { return std::toupper(c); });
            m_comment.fields[field_name].push_back(field_value);
        }
    }

    return true;
}

bool FLACHeaderParser::parseHeader(ogg_packet* packet) {
    // Every packet routed here is at least a 4-byte FLAC metadata block header;
    // the first packet's larger encapsulation header is validated below.
    if (!packet || packet->bytes < 4) return false;

    unsigned char* data = packet->packet;

    // Packet 0: ID Header + STREAMINFO
    if (m_headers_count == 0) {
        // The first packet carries the Ogg-FLAC encapsulation header.
        if (packet->bytes < 13) return false; // Minimum encapsulation header size

        // Validation: 0x7f + "FLAC"
        if (memcmp(data, "\x7f" "FLAC", 5) != 0) return false;

        // Version 1.0 (major=1)
        if (data[5] != 1) return false;

        // Bytes 7-8: 16-bit big-endian count of header packets that follow this
        // first packet (STREAMINFO lives inside this packet). 0x0000 means the
        // count is unknown, in which case we fall back to the metadata blocks'
        // last-block flag to detect completion.
        m_expected_headers = (data[7] << 8) | data[8];

        // "fLaC" signature (bytes 9-12)
        if (memcmp(data + 9, "fLaC", 4) != 0) return false;

        // Metadata Block(s) start at 13
        // The first block MUST be STREAMINFO (type 0)
        if (packet->bytes < 13 + 4) return false; // Header + Block Header

        unsigned char* block_header = data + 13;
        int type = block_header[0] & 0x7F;
        if (block_header[0] & 0x80) {
            // STREAMINFO flagged as the last metadata block: no further headers.
            m_last_metadata_seen = true;
        }

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
        

        storeHeaderPacket(packet);
        m_streaminfo_found = true;
        m_headers_count = 1;

        return true;
    }

    // Subsequent packets are individual FLAC metadata blocks (VORBIS_COMMENT,
    // PICTURE, SEEKTABLE, ...), each wrapped in its own Ogg packet. Route them
    // here so the VORBIS_COMMENT tags are extracted and, crucially, so the
    // block is counted/stored as a header rather than delivered as audio.
    unsigned char block_type = data[0] & 0x7F;
    bool last_block = (data[0] & 0x80) != 0;

    // Length (24-bit BE) of the metadata block body.
    uint32_t block_length = (data[1] << 16) | (data[2] << 8) | data[3];

    if (block_type == 4 && packet->bytes >= 4) { // VORBIS_COMMENT
        size_t body_available = static_cast<size_t>(packet->bytes) - 4;
        size_t body_size = std::min(static_cast<size_t>(block_length), body_available);
        parseVorbisCommentBlock(data + 4, body_size);
    }

    if (last_block) {
        m_last_metadata_seen = true;
    }

    storeHeaderPacket(packet);
    m_headers_count++;
    return true;
}

bool FLACHeaderParser::isHeadersComplete() const {
    if (!m_streaminfo_found) {
        return false;
    }

    // With a known packet count, headers are complete once the first packet
    // plus all m_expected_headers follow-up packets have been consumed.
    if (m_expected_headers > 0) {
        return m_headers_count > m_expected_headers;
    }

    // Count unknown (0x0000): rely on the metadata blocks' last-block flag.
    return m_last_metadata_seen;
}

CodecInfo FLACHeaderParser::getCodecInfo() const {
    return m_info;
}

OggVorbisComment FLACHeaderParser::getVorbisComment() const {
    return m_comment;
}

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3
