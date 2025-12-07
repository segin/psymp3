/*
 * test_vorbis_parser.cpp - Unit tests for VorbisHeaderParser
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

#ifdef HAVE_OGGDEMUXER

#include "demuxer/ogg/VorbisHeaderParser.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <cassert>

using namespace PsyMP3::Demuxer::Ogg;

// Helper to create Vorbis packets
ogg_packet createVorbisIDHeader(int channels, long rate) {
    static std::vector<uint8_t> buffer;
    buffer.resize(30);
    
    buffer[0] = 0x01; // Type ID
    std::memcpy(&buffer[1], "vorbis", 6);
    // Version 0
    std::memset(&buffer[7], 0, 4);
    // Channels
    buffer[11] = (uint8_t)channels;
    // Rate
    buffer[12] = (rate) & 0xFF;
    buffer[13] = (rate >> 8) & 0xFF;
    buffer[14] = (rate >> 16) & 0xFF;
    buffer[15] = (rate >> 24) & 0xFF;
    // Bitrates (3x4 bytes) - zero
    std::memset(&buffer[16], 0, 12);
    // Blocksize
    buffer[28] = 0; // Invalid blocksize actually? But we check packet size
    // Framing bit
    buffer[29] = 1;
    
    ogg_packet packet;
    packet.packet = buffer.data();
    packet.bytes = buffer.size();
    packet.b_o_s = 1;
    packet.e_o_s = 0;
    packet.granulepos = 0;
    packet.packetno = 0;
    return packet;
}

ogg_packet createVorbisCommentHeader() {
    static std::vector<uint8_t> buffer;
    buffer = {0x03, 'v', 'o', 'r', 'b', 'i', 's'}; // Minimal
    
    ogg_packet packet;
    packet.packet = buffer.data();
    packet.bytes = buffer.size();
    packet.b_o_s = 0;
    packet.e_o_s = 0;
    packet.granulepos = 0;
    packet.packetno = 1;
    return packet;
}

ogg_packet createVorbisSetupHeader() {
    static std::vector<uint8_t> buffer;
    buffer = {0x05, 'v', 'o', 'r', 'b', 'i', 's'};
    
    ogg_packet packet;
    packet.packet = buffer.data();
    packet.bytes = buffer.size();
    packet.b_o_s = 0;
    packet.e_o_s = 0;
    packet.granulepos = 0;
    packet.packetno = 2;
    return packet;
}

#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "ASSERTION FAILED: " << (message) \
                      << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

bool testIDHeader() {
    std::cout << "Testing Vorbis ID Header..." << std::endl;
    VorbisHeaderParser parser;
    
    ogg_packet packet = createVorbisIDHeader(2, 44100);
    bool res = parser.parseHeader(&packet);
    
    ASSERT(res == true, "Should parse valid ID header");
    ASSERT(parser.getCodecInfo().codec_name == "Vorbis", "Codec name mismatch");
    ASSERT(parser.getCodecInfo().channels == 2, "Channels mismatch");
    ASSERT(parser.getCodecInfo().rate == 44100, "Rate mismatch");
    ASSERT(!parser.isHeadersComplete(), "Headers should not be complete");
    
    std::cout << "  ✓ Passed" << std::endl;
    return true;
}

bool testFullSequence() {
    std::cout << "Testing Vorbis Full Sequence..." << std::endl;
    VorbisHeaderParser parser;
    
    ogg_packet p1 = createVorbisIDHeader(1, 48000);
    ASSERT(parser.parseHeader(&p1), "ID header failed");
    
    ogg_packet p2 = createVorbisCommentHeader();
    ASSERT(parser.parseHeader(&p2), "Comment header failed");
    ASSERT(!parser.isHeadersComplete(), "Headers incomplete after comment");
    
    ogg_packet p3 = createVorbisSetupHeader();
    ASSERT(parser.parseHeader(&p3), "Setup header failed");
    ASSERT(parser.isHeadersComplete(), "Headers should be complete");
    
    std::cout << "  ✓ Passed" << std::endl;
    return true;
}

bool testInvalidSequence() {
    std::cout << "Testing Vorbis Invalid Sequence..." << std::endl;
    VorbisHeaderParser parser;
    
    ogg_packet p2 = createVorbisCommentHeader();
    ASSERT(!parser.parseHeader(&p2), "Should reject Comment as first packet");
    
    return true;
}

int main() {
    std::cout << "Running VorbisHeaderParser Tests..." << std::endl;
    int passed = 0;
    int total = 0;
    
    if (testIDHeader()) passed++; total++;
    if (testFullSequence()) passed++; total++;
    if (testInvalidSequence()) passed++; total++;
    
    if (passed == total) {
        std::cout << "All tests PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << (total - passed) << " tests FAILED!" << std::endl;
        return 1;
    }
}

#else
int main() { return 0; }
#endif
