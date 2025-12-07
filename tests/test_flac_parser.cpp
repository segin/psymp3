/*
 * test_flac_parser.cpp - Unit tests for FLACHeaderParser
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

#ifdef HAVE_OGGDEMUXER

#include "demuxer/ogg/FLACHeaderParser.h"
#include <iostream>
#include <vector>
#include <cstring>

using namespace PsyMP3::Demuxer::Ogg;

// Helper
ogg_packet createFLACIDHeader(int channels, long rate) {
    static std::vector<uint8_t> buffer;
    // Header (13 bytes) + Block Header (4 bytes) + STREAMINFO (34 bytes) = 51 bytes
    buffer.resize(51);
    
    // Ogg FLAC Header
    buffer[0] = 0x7F;
    std::memcpy(&buffer[1], "FLAC", 4);
    buffer[5] = 1; // Major
    buffer[6] = 0; // Minor
    buffer[7] = 0; buffer[8] = 1; // Num headers
    std::memcpy(&buffer[9], "fLaC", 4);
    
    // Metadata Block Header (STREAMINFO)
    buffer[13] = 0x80; // Last block + Type 0
    // Length 34 (0x000022)
    buffer[14] = 0; buffer[15] = 0; buffer[16] = 34;
    
    // STREAMINFO Data (starts at 17)
    unsigned char* streaminfo = &buffer[17];
    std::memset(streaminfo, 0, 34);
    
    // Sample Rate (20 bits), Channels (3 bits), BPS (5 bits)
    // Rate: rate (e.g. 44100 = 0xAC44)
    // Channels: channels - 1
    // Byte 10: RRRRRRRR (rate >> 12)
    // Byte 11: RRRRRRRR (rate >> 4)
    // Byte 12: RRRRCCCE ((rate & 0xF) << 4) | ((ch-1) << 1) | (bps_idx >> 4)
    
    streaminfo[10] = (rate >> 12) & 0xFF;
    streaminfo[11] = (rate >> 4) & 0xFF;
    streaminfo[12] = ((rate & 0x0F) << 4) | (((channels - 1) & 0x07) << 1); 
    // BPS default 16 (idx? No, value-1). 16-1 = 15 = 01111?
    // No, BPS is bits-1. 16 bits = 15 = 001111?
    // Wait, standard says: 3 bits for channels (k-1). 5 bits for BPS (k-1).
    // So 16 bps -> 15 (01111).
    // Lowest bit of byte 12 is top bit of BPS. (0)
    // Byte 13 top 4 bits are rest of BPS. (1111) -> 0xF0.
    streaminfo[12] |= (0); // Top bit of 15 (01111) is 0
    streaminfo[13] = (0xF0); 
    
    ogg_packet packet;
    packet.packet = buffer.data();
    packet.bytes = buffer.size();
    packet.b_o_s = 1;
    packet.e_o_s = 0;
    packet.granulepos = 0;
    packet.packetno = 0;
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
    std::cout << "Testing FLAC ID Header..." << std::endl;
    FLACHeaderParser parser;
    
    ogg_packet packet = createFLACIDHeader(2, 44100);
    bool res = parser.parseHeader(&packet);
    
    ASSERT(res == true, "Should parse valid ID header");
    ASSERT(parser.getCodecInfo().codec_name == "FLAC", "Codec name mismatch");
    ASSERT(parser.getCodecInfo().channels == 2, "Channels mismatch");
    ASSERT(parser.getCodecInfo().rate == 44100, "Rate mismatch");
    ASSERT(parser.isHeadersComplete(), "Headers should be complete (STREAMINFO found)");
    
    std::cout << "  ✓ Passed" << std::endl;
    return true;
}

int main() {
    std::cout << "Running FLACHeaderParser Tests..." << std::endl;
    int passed = 0;
    int total = 0;
    
    if (testIDHeader()) passed++; total++;
    
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
