/*
 * test_streaminfo_real_files.cpp - Test FLAC STREAMINFO parsing with real files
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include <iostream>
#include <vector>
#include <string>
#include <memory>

// Test STREAMINFO parsing with real FLAC files
bool testStreamInfoWithFile(const std::string& filename) {
    std::cout << "Testing STREAMINFO parsing with: " << filename << std::endl;
    
    try {
        // Create IOHandler for the file
        auto handler = std::make_unique<FileIOHandler>(filename);
        if (!handler || handler->getLastError() != 0) {
            std::cout << "  FAILED: Could not open file: " << filename << std::endl;
            return false;
        }
        
        // Create FLAC demuxer
        FLACDemuxer demuxer(std::move(handler));
        
        // Parse the container (which includes STREAMINFO parsing)
        if (!demuxer.parseContainer()) {
            std::cout << "  FAILED: Could not parse FLAC container" << std::endl;
            return false;
        }
        
        // Get stream info to verify STREAMINFO was parsed correctly
        auto streams = demuxer.getStreams();
        if (streams.empty()) {
            std::cout << "  FAILED: No streams found" << std::endl;
            return false;
        }
        
        const auto& stream = streams[0];
        
        // Validate that we got reasonable values
        if (stream.sample_rate == 0 || stream.channels == 0 || stream.bits_per_sample == 0) {
            std::cout << "  FAILED: Invalid STREAMINFO values extracted" << std::endl;
            std::cout << "    Sample rate: " << stream.sample_rate << " Hz" << std::endl;
            std::cout << "    Channels: " << static_cast<int>(stream.channels) << std::endl;
            std::cout << "    Bits per sample: " << static_cast<int>(stream.bits_per_sample) << std::endl;
            return false;
        }
        
        // Check for reasonable ranges
        if (stream.sample_rate < 1000 || stream.sample_rate > 1000000) {
            std::cout << "  WARNING: Unusual sample rate: " << stream.sample_rate << " Hz" << std::endl;
        }
        
        if (stream.channels < 1 || stream.channels > 8) {
            std::cout << "  WARNING: Unusual channel count: " << static_cast<int>(stream.channels) << std::endl;
        }
        
        if (stream.bits_per_sample < 8 || stream.bits_per_sample > 32) {
            std::cout << "  WARNING: Unusual bit depth: " << static_cast<int>(stream.bits_per_sample) << std::endl;
        }
        
        std::cout << "  PASSED: STREAMINFO parsed successfully" << std::endl;
        std::cout << "    Sample rate: " << stream.sample_rate << " Hz" << std::endl;
        std::cout << "    Channels: " << static_cast<int>(stream.channels) << std::endl;
        std::cout << "    Bits per sample: " << static_cast<int>(stream.bits_per_sample) << std::endl;
        std::cout << "    Duration: " << stream.duration_ms << " ms" << std::endl;
        
        if (!stream.title.empty()) {
            std::cout << "    Title: " << stream.title << std::endl;
        }
        if (!stream.artist.empty()) {
            std::cout << "    Artist: " << stream.artist << std::endl;
        }
        if (!stream.album.empty()) {
            std::cout << "    Album: " << stream.album << std::endl;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "  FAILED: Exception during parsing: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cout << "  FAILED: Unknown exception during parsing" << std::endl;
        return false;
    }
}

int main() {
    std::cout << "FLAC STREAMINFO Real File Verification Test" << std::endl;
    std::cout << "===========================================" << std::endl;
    
    // List of FLAC files to test
    std::vector<std::string> test_files = {
        "data/11 Everlong.flac",
        "data/11 life goes by.flac", 
        "data/RADIO GA GA.flac",
        "data/04 Time.flac"  // 6-channel file for multichannel testing
    };
    
    int passed = 0;
    int total = 0;
    
    for (const auto& filename : test_files) {
        total++;
        if (testStreamInfoWithFile(filename)) {
            passed++;
        }
        std::cout << std::endl;
    }
    
    std::cout << "===========================================" << std::endl;
    std::cout << "Test Results: " << passed << "/" << total << " files processed successfully" << std::endl;
    
    if (passed == total) {
        std::cout << "SUCCESS: All FLAC files parsed correctly!" << std::endl;
        return 0;
    } else {
        std::cout << "PARTIAL SUCCESS: " << (total - passed) << " file(s) failed to parse" << std::endl;
        return 1;
    }
}