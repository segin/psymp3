/*
 * test_minimal_demuxer.cpp - Minimal test to check OggDemuxer construction
 */

#include "psymp3.h"

#ifdef HAVE_OGGDEMUXER

#include <iostream>

int main() {
    std::cout << "Testing minimal OggDemuxer construction..." << std::endl;
    
    try {
        // Create a simple test file
        std::ofstream file("test.ogg", std::ios::binary);
        file << "OggS";  // Just write the Ogg signature
        file.close();
        
        std::cout << "Creating FileIOHandler..." << std::endl;
        auto handler = std::make_unique<FileIOHandler>("test.ogg");
        
        std::cout << "Creating OggDemuxer..." << std::endl;
        OggDemuxer demuxer(std::move(handler));
        
        std::cout << "OggDemuxer created successfully!" << std::endl;
        
        std::remove("test.ogg");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::remove("test.ogg");
        return 1;
    }
}

#else

int main() {
    std::cout << "OggDemuxer not available" << std::endl;
    return 0;
}

#endif