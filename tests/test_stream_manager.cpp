/*
 * test_stream_manager.cpp - Unit tests for OggStreamManager
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

#ifdef HAVE_OGGDEMUXER

#include "MockOggFile.h"
#include <cassert>

using namespace PsyMP3::Demuxer::Ogg;

// Simple assertion macro
#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "ASSERTION FAILED: " << (message) \
                      << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

bool testStreamLifecycle() {
    std::cout << "Testing OggStreamManager lifecycle..." << std::endl;
    
    try {
        {
            OggStreamManager stream(12345);
            ASSERT(stream.getSerialNumber() == 12345, "Serial number mismatch");
            ASSERT(!stream.areHeadersComplete(), "Should not have headers complete initially");
            // Destructor runs here
        }
        std::cout << "  ✓ Passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Failed: " << e.what() << std::endl;
        return false;
    }
}

bool testSubmitPageAndGetPacket() {
    std::cout << "Testing OggStreamManager page submission and packet extraction..." << std::endl;
    
    try {
        OggStreamManager stream(12345);
        auto data = MockOggFile::createSimpleOggFile(12345, 10);
        
        // We need to parse valid ogg_page struct from data
        // MockOggFile creates valid data, but we need to point ogg_page struct to it correctly manually here
        // or use OggSyncManager to parse it. 
        // Using OggSyncManager adds dependency. Let's construct ogg_page manually since we know offsets.
        
        ogg_page page;
        page.header = data.data();
        page.header_len = 28;
        page.body = data.data() + 28;
        page.body_len = 10;
        
        int res = stream.submitPage(&page);
        ASSERT(res == 0, "Failed to submit page");
        
        ogg_packet packet;
        res = stream.getPacket(&packet);
        ASSERT(res == 1, "Failed to extract packet");
        ASSERT(packet.bytes == 10, "Packet size mismatch");
        ASSERT(packet.packet[0] == 0x41, "Packet data mismatch");
        
        // No more packets
        res = stream.getPacket(&packet);
        ASSERT(res == 0, "Should handle no more packets");
        
        std::cout << "  ✓ Passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Failed: " << e.what() << std::endl;
        return false;
    }
}

bool testReset() {
    std::cout << "Testing OggStreamManager reset..." << std::endl;
    
    try {
        OggStreamManager stream(12345);
        // ... set some state ...
        stream.setHeadersComplete(true);
        
        stream.reset();
        
        // Reset typically clears internal buffers but might preserve serial no?
        // libogg reset clears state.
        
        ASSERT(stream.getSerialNumber() == 12345, "Serial preserved");
        // Headers complete flag is ours, we should verify logic. 
        // reset() is usually for seeking. Does it clear headers_complete?
        // Current implementation does NOT clear m_headers_complete in reset().
        // Maybe it should? If we seek back to start?
        // Usually reset is for clearing partial packets.
        
        std::cout << "  ✓ Passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Failed: " << e.what() << std::endl;
        return false;
    }
}

int main() {
    std::cout << "Running OggStreamManager Tests..." << std::endl;
    std::cout << "=============================================" << std::endl;
    
    int passed = 0;
    int total = 0;
    
    if (testStreamLifecycle()) passed++; total++;
    if (testSubmitPageAndGetPacket()) passed++; total++;
    if (testReset()) passed++; total++;
    
    if (passed == total) {
        std::cout << "All tests PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << (total - passed) << " tests FAILED!" << std::endl;
        return 1;
    }
}

#else

int main() {
    std::cout << "Ogg support disabled" << std::endl;
    return 0;
}

#endif
