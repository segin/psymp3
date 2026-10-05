/*
 * debug_flac_seeking.cpp - Debug FLAC demuxer seeking issues
 */

#include "psymp3.h"
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "FLAC Demuxer Seeking Debug" << std::endl;
    std::cout << "==========================" << std::endl;

    // Enable debug logging
    Debug::init("", {"flac", "all"});

    // The file to examine is given on the command line. Without one, fall
    // back to the suite's generated fixture (see TESTING.md); it is looked
    // for from both tests/ and the top of the tree. No path on any one
    // machine is named here.
    std::vector<std::string> test_files;
    if (argc > 1) {
        test_files.push_back(argv[1]);
    } else {
        test_files = {"data/fixture.flac", "tests/data/fixture.flac"};
    }

    std::string test_file;
    for (const auto& path : test_files) {
        std::ifstream file(path);
        if (file.good()) {
            test_file = path;
            break;
        }
    }

    if (test_file.empty()) {
        if (argc > 1) {
            std::cerr << "Cannot open " << argv[1] << std::endl;
        } else {
            std::cerr << "No FLAC file given and no fixture found" << std::endl;
            std::cerr << "Usage: " << argv[0] << " <file.flac>" << std::endl;
        }
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