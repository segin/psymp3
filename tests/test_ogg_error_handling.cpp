/*
 * test_ogg_error_handling.cpp - Error handling tests for OggDemuxer
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

#ifdef HAVE_OGGDEMUXER

#include "demuxer/ogg/OggStreamManager.h"
#include "demuxer/ogg/OggSeekingEngine.h"
#include "demuxer/ogg/CodecHeaderParser.h"
#include <iostream>
#include <vector>
#include <cstring>

#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "ASSERTION FAILED: " << (message) \
                      << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

using namespace PsyMP3::Demuxer::Ogg;

bool testNullPacketHandling() {
    std::cout << "Testing null packet handling..." << std::endl;
    
    // CodecHeaderParser::create should handle null
    auto parser = CodecHeaderParser::create(nullptr);
    ASSERT(parser == nullptr, "Should return null for null packet");
    
    std::cout << "  ✓ Passed" << std::endl;
    return true;
}

bool testEmptyPacketHandling() {
    std::cout << "Testing empty packet handling..." << std::endl;
    
    ogg_packet packet;
    packet.packet = nullptr;
    packet.bytes = 0;
    packet.b_o_s = 1;
    packet.e_o_s = 0;
    packet.granulepos = 0;
    packet.packetno = 0;
    
    auto parser = CodecHeaderParser::create(&packet);
    ASSERT(parser == nullptr, "Should return null for empty packet");
    
    std::cout << "  ✓ Passed" << std::endl;
    return true;
}

bool testInvalidGranuleHandling() {
    std::cout << "Testing invalid granule handling..." << std::endl;
    
    // -1 is the "unknown" granule position in Ogg
    ASSERT(!OggSeekingEngine::isValidGranule(-1), "-1 should be invalid");
    ASSERT(!OggSeekingEngine::isValidGranule(-100), "Negative should be invalid");
    ASSERT(OggSeekingEngine::isValidGranule(0), "0 should be valid");
    
    std::cout << "  ✓ Passed" << std::endl;
    return true;
}

bool testOverflowProtection() {
    std::cout << "Testing overflow protection..." << std::endl;
    
    int64_t max = std::numeric_limits<int64_t>::max();
    int64_t min = std::numeric_limits<int64_t>::min();
    
    // Overflow should saturate to max
    int64_t result = OggSeekingEngine::safeGranuleAdd(max, 100);
    ASSERT(result == max, "Should saturate to max on overflow");
    
    // Underflow should saturate to min
    result = OggSeekingEngine::safeGranuleSub(min, 100);
    ASSERT(result == min, "Should saturate to min on underflow");
    
    std::cout << "  ✓ Passed" << std::endl;
    return true;
}

bool testMalformedSignatures() {
    std::cout << "Testing malformed codec signatures..." << std::endl;
    
    // Almost-Vorbis signature
    std::vector<uint8_t> data = {0x01, 'v', 'o', 'r', 'b', 'i', 'X'}; // Wrong last char
    ogg_packet packet;
    packet.packet = data.data();
    packet.bytes = data.size();
    packet.b_o_s = 1;
    
    auto parser = CodecHeaderParser::create(&packet);
    ASSERT(parser == nullptr, "Should reject malformed Vorbis signature");
    
    // Almost-Opus signature
    data = {'O', 'p', 'u', 's', 'H', 'e', 'a', 'X'}; // Wrong last char
    packet.packet = data.data();
    packet.bytes = data.size();
    
    parser = CodecHeaderParser::create(&packet);
    ASSERT(parser == nullptr, "Should reject malformed Opus signature");
    
    std::cout << "  ✓ Passed" << std::endl;
    return true;
}

int main() {
    std::cout << "Running OggDemuxer Error Handling Tests..." << std::endl;
    std::cout << "===========================================" << std::endl;
    
    int passed = 0;
    int total = 0;
    
    if (testNullPacketHandling()) passed++; total++;
    if (testEmptyPacketHandling()) passed++; total++;
    if (testInvalidGranuleHandling()) passed++; total++;
    if (testOverflowProtection()) passed++; total++;
    if (testMalformedSignatures()) passed++; total++;
    
    std::cout << std::endl;
    if (passed == total) {
        std::cout << "All " << total << " tests PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << (total - passed) << " of " << total << " tests FAILED!" << std::endl;
        return 1;
    }
}

#else
int main() { return 0; }
#endif
