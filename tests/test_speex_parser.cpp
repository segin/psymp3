/*
 * test_speex_parser.cpp - Unit tests for SpeexHeaderParser
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

#ifdef HAVE_OGGDEMUXER


namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

// Helper
ogg_packet createSpeexIDHeader(int channels, long rate) {
    static std::vector<uint8_t> buffer;
    buffer.resize(80);
    std::memset(buffer.data(), 0, 80);
    
    std::memcpy(&buffer[0], "Speex   ", 8);
    // Rate at 36
    buffer[36] = rate & 0xFF;
    buffer[37] = (rate >> 8) & 0xFF;
    buffer[38] = (rate >> 16) & 0xFF;
    buffer[39] = (rate >> 24) & 0xFF;
    
    // Channels at 48
    buffer[48] = channels & 0xFF;
    buffer[49] = (channels >> 8) & 0xFF;
    buffer[50] = (channels >> 16) & 0xFF;
    buffer[51] = (channels >> 24) & 0xFF;
    
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
    std::cout << "Testing Speex ID Header..." << std::endl;
    SpeexHeaderParser parser;
    
    ogg_packet packet = createSpeexIDHeader(1, 16000); // 16kHz mono
    bool res = parser.parseHeader(&packet);
    
    ASSERT(res == true, "Should parse valid ID header");
    ASSERT(parser.getCodecInfo().codec_name == "Speex", "Codec name mismatch");
    ASSERT(parser.getCodecInfo().channels == 1, "Channels mismatch");
    ASSERT(parser.getCodecInfo().rate == 16000, "Rate mismatch");
    
    std::cout << "  ✓ Passed" << std::endl;
    return true;
}

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

int main() {
    std::cout << "Running SpeexHeaderParser Tests..." << std::endl;
    int passed = 0;
    int total = 0;
    
    if (PsyMP3::Demuxer::Ogg::testIDHeader()) passed++; total++;
    
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
