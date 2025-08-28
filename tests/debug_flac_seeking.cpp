/*
 * debug_flac_seeking.cpp - Debug FLAC demuxer seeking issues
 */

#include "psymp3.h"
#include <iostream>

int main() {
    std::cout << "FLAC Demuxer Seeking Debug" << std::endl;
    std::cout << "==========================" << std::endl;
    
    // Enable debug logging
    Debug::init("", {"flac", "all"});
    
    // Try to find a test file
    std::vector<std::string> test_files = {
        "tests/data/11 life goes by.flac",
        "tests/data/RADIO GA GA.flac",
        "/mnt/8TB-3/music/almost monday/DIVE/11 life goes by.flac"
    };
    
    std::string test_file;
    for (const auto& path : test_files) {
        std::ifstream file(path);
        if (file.good()) {
            test_file = path;
            break;
        }
    }
    
    if (test_file.empty()) {
        std::cerr << "No test FLAC file found" << std::endl;
        return 1;
    }
    
    std::cout << "Using test file: " << test_file << std::endl;
    
    try {
        auto handler = std::make_unique<FileIOHandler>(test_file.c_str());
        auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        std::cout << "Parsing container..." << std::endl;
        if (!demuxer->parseContainer()) {
            std::cerr << "Failed to parse container" << std::endl;
            return 1;
        }
        
        auto streams = demuxer->getStreams();
        if (streams.empty()) {
            std::cerr << "No streams found" << std::endl;
            return 1;
        }
        
        const auto& stream = streams[0];
        std::cout << "Stream info:" << std::endl;
        std::cout << "  Sample rate: " << stream.sample_rate << " Hz" << std::endl;
        std::cout << "  Channels: " << stream.channels << std::endl;
        std::cout << "  Duration: " << stream.duration_ms << " ms" << std::endl;
        
        uint64_t duration = demuxer->getDuration();
        std::cout << "Demuxer duration: " << duration << " ms" << std::endl;
        
        // Test seeking to beginning (0 ms)
        std::cout << "\nTesting seek to beginning (0 ms)..." << std::endl;
        bool seek_result = demuxer->seekTo(0);
        std::cout << "Seek result: " << (seek_result ? "SUCCESS" : "FAILED") << std::endl;
        
        if (seek_result) {
            uint64_t position = demuxer->getPosition();
            std::cout << "Position after seek: " << position << " ms" << std::endl;
        }
        
        // Test seeking to middle
        uint64_t middle = duration / 2;
        std::cout << "\nTesting seek to middle (" << middle << " ms)..." << std::endl;
        seek_result = demuxer->seekTo(middle);
        std::cout << "Seek result: " << (seek_result ? "SUCCESS" : "FAILED") << std::endl;
        
        if (seek_result) {
            uint64_t position = demuxer->getPosition();
            std::cout << "Position after seek: " << position << " ms" << std::endl;
        }
        
        // Test frame reading
        std::cout << "\nTesting frame reading..." << std::endl;
        demuxer->seekTo(0); // Reset to beginning
        
        for (int i = 0; i < 3; i++) {
            auto chunk = demuxer->readChunk();
            if (!chunk.data.empty()) {
                std::cout << "Frame " << (i+1) << ": " << chunk.data.size() << " bytes, "
                          << "timestamp: " << chunk.timestamp_samples << " samples" << std::endl;
            } else {
                std::cout << "Frame " << (i+1) << ": EMPTY" << std::endl;
                break;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}