/*
 * test_codec_identification.cpp - Unit tests for CodecHeaderParser detection
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

#ifdef HAVE_OGGDEMUXER

#include <cassert>

using namespace PsyMP3::Demuxer::Ogg;

// Helper to create a BOS packet
ogg_packet createBOSPacket(const std::string& id_string) {
    static std::vector<uint8_t> buffer;
    buffer.assign(id_string.begin(), id_string.end());
    
    ogg_packet packet;
    packet.packet = buffer.data();
    packet.bytes = buffer.size();
    packet.b_o_s = 1;
    packet.e_o_s = 0;
    packet.granulepos = 0;
    packet.packetno = 0;
    return packet;
}

// Special for binary data
ogg_packet createBinaryBOSPacket(const std::vector<uint8_t>& data) {
    static std::vector<uint8_t> buffer;
    buffer = data;
    
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

bool testVorbisDetection() {
    std::cout << "Testing Vorbis detection..." << std::endl;
    // Vorbis: 0x01 + "vorbis"
    std::vector<uint8_t> data = {0x01};
    std::string s = "vorbis";
    data.insert(data.end(), s.begin(), s.end());
    
    ogg_packet packet = createBinaryBOSPacket(data);
    auto parser = CodecHeaderParser::create(&packet);
    
    ASSERT(parser != nullptr, "Vorbis parser not created");
    ASSERT(parser->getCodecInfo().codec_name == "Vorbis", "Incorrect codec identified");
    
    std::cout << "  ✓ Passed" << std::endl;
    return true;
}

bool testOpusDetection() {
    std::cout << "Testing Opus detection..." << std::endl;
    ogg_packet packet = createBOSPacket("OpusHead");
    auto parser = CodecHeaderParser::create(&packet);
    
    ASSERT(parser != nullptr, "Opus parser not created");
    ASSERT(parser->getCodecInfo().codec_name == "Opus", "Incorrect codec identified");
    
    std::cout << "  ✓ Passed" << std::endl;
    return true;
}

bool testFLACDetection() {
    std::cout << "Testing FLAC detection..." << std::endl;
    // FLAC: 0x7f + "FLAC"
    std::vector<uint8_t> data = {0x7f, 'F', 'L', 'A', 'C'};
    
    ogg_packet packet = createBinaryBOSPacket(data);
    auto parser = CodecHeaderParser::create(&packet);
    
    ASSERT(parser != nullptr, "FLAC parser not created");
    ASSERT(parser->getCodecInfo().codec_name == "FLAC", "Incorrect codec identified");
    
    std::cout << "  ✓ Passed" << std::endl;
    return true;
}

bool testSpeexDetection() {
    std::cout << "Testing Speex detection..." << std::endl;
    ogg_packet packet = createBOSPacket("Speex   ");
    auto parser = CodecHeaderParser::create(&packet);
    
    ASSERT(parser != nullptr, "Speex parser not created");
    ASSERT(parser->getCodecInfo().codec_name == "Speex", "Incorrect codec identified");
    
    std::cout << "  ✓ Passed" << std::endl;
    return true;
}

bool testUnknownDetection() {
    std::cout << "Testing Unknown detection..." << std::endl;
    ogg_packet packet = createBOSPacket("Unknown1");
    auto parser = CodecHeaderParser::create(&packet);
    
    ASSERT(parser == nullptr, "Unknown parser should be null");
    
    std::cout << "  ✓ Passed" << std::endl;
    return true;
}

int main() {
    std::cout << "Running CodecHeaderParser Tests..." << std::endl;
    std::cout << "==================================" << std::endl;
    
    int passed = 0;
    int total = 0;
    
    if (testVorbisDetection()) passed++; total++;
    if (testOpusDetection()) passed++; total++;
    if (testFLACDetection()) passed++; total++;
    if (testSpeexDetection()) passed++; total++;
    if (testUnknownDetection()) passed++; total++;
    
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
