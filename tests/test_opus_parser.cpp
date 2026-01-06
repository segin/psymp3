/*
 * test_opus_parser.cpp - Unit tests for OpusHeaderParser
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

#ifdef HAVE_OGGDEMUXER


namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

// Helper
ogg_packet createOpusIDHeader(int channels) {
    static std::vector<uint8_t> buffer;
    buffer.resize(19);
    
    std::memcpy(&buffer[0], "OpusHead", 8);
    buffer[8] = 1; // Version
    buffer[9] = channels; // Channels
    buffer[10] = 0; buffer[11] = 0; // Pre-skip
    buffer[12] = 0x80; buffer[13] = 0xBB; buffer[14] = 0; buffer[15] = 0; // 48000 Hz
    buffer[16] = 0; buffer[17] = 0; // Gain
    buffer[18] = 0; // Mapping Family
    
    ogg_packet packet;
    packet.packet = buffer.data();
    packet.bytes = buffer.size();
    packet.b_o_s = 1;
    packet.e_o_s = 0;
    packet.granulepos = 0;
    packet.packetno = 0;
    return packet;
}

ogg_packet createOpusTagsHeader() {
    static std::vector<uint8_t> buffer;
    buffer = {'O', 'p', 'u', 's', 'T', 'a', 'g', 's'};
    
    ogg_packet packet;
    packet.packet = buffer.data();
    packet.bytes = buffer.size();
    packet.b_o_s = 0;
    packet.e_o_s = 0;
    packet.granulepos = 0;
    packet.packetno = 1;
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
    std::cout << "Testing Opus ID Header..." << std::endl;
    OpusHeaderParser parser;
    
    ogg_packet packet = createOpusIDHeader(2);
    bool res = parser.parseHeader(&packet);
    
    ASSERT(res == true, "Should parse valid ID header");
    ASSERT(parser.getCodecInfo().codec_name == "Opus", "Codec name mismatch");
    ASSERT(parser.getCodecInfo().channels == 2, "Channels mismatch");
    ASSERT(!parser.isHeadersComplete(), "Headers should not be complete");
    
    std::cout << "  ✓ Passed" << std::endl;
    return true;
}

bool testFullSequence() {
    std::cout << "Testing Opus Full Sequence..." << std::endl;
    OpusHeaderParser parser;
    
    ogg_packet p1 = createOpusIDHeader(2);
    ASSERT(parser.parseHeader(&p1), "ID header failed");
    
    ogg_packet p2 = createOpusTagsHeader();
    ASSERT(parser.parseHeader(&p2), "Tags header failed");
    
    ASSERT(parser.isHeadersComplete(), "Headers should be complete");
    
    std::cout << "  ✓ Passed" << std::endl;
    return true;
}

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

int main() {
    std::cout << "Running OpusHeaderParser Tests..." << std::endl;
    int passed = 0;
    int total = 0;
    
    if (PsyMP3::Demuxer::Ogg::testIDHeader()) passed++; total++;
    if (testFullSequence()) passed++; total++;
    
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
