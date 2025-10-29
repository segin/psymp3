/*
 * test_flac_streamable_subset_real_files.cpp - Simple streamable subset test
 * This file is part of PsyMP3.
 */

#include "psymp3.h"

#ifdef HAVE_FLAC

int main() {
    std::cout << "=== FLAC Streamable Subset Test ===" << std::endl;
    
    try {
        // Test with one FLAC file
        auto handler = std::make_unique<FileIOHandler>("tests/data/04 Time.flac");
        if (!handler) {
            std::cout << "✗ Failed to create IOHandler" << std::endl;
            return 1;
        }
        
        FLACDemuxer demuxer(std::move(handler));
        
        // Test streamable subset mode setting
        demuxer.setStreamableSubsetMode(FLACDemuxer::StreamableSubsetMode::ENABLED);
        auto mode = demuxer.getStreamableSubsetMode();
        
        if (mode == FLACDemuxer::StreamableSubsetMode::ENABLED) {
            std::cout << "✓ Streamable subset mode configuration works" << std::endl;
        } else {
            std::cout << "✗ Streamable subset mode configuration failed" << std::endl;
            return 1;
        }
        
        // Test statistics
        auto stats = demuxer.getStreamableSubsetStats();
        std::cout << "✓ Streamable subset statistics accessible" << std::endl;
        std::cout << "  Initial violations: " << stats.total_violations << std::endl;
        
        // Reset statistics
        demuxer.resetStreamableSubsetStats();
        stats = demuxer.getStreamableSubsetStats();
        
        if (stats.total_violations == 0 && stats.frames_validated == 0) {
            std::cout << "✓ Statistics reset works" << std::endl;
        } else {
            std::cout << "✗ Statistics reset failed" << std::endl;
            return 1;
        }
        
        std::cout << "✓ All streamable subset tests PASSED" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "✗ Exception: " << e.what() << std::endl;
        return 1;
    }
}

#else

int main() {
    std::cout << "FLAC support not available - skipping test" << std::endl;
    return 0;
}

#endif