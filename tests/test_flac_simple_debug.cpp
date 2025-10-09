/*
 * test_flac_simple_debug.cpp - Simple debug test for FLAC codec
 */

#include <iostream>
#include <vector>
#include <cstdint>

// Minimal includes for FLAC codec testing
#include "../include/FLACCodec.h"
#include "../include/AudioCodec.h"
#include "../include/debug.h"

int main() {
    std::cout << "=== FLAC Simple Debug Test ===" << std::endl;
    
    try {
        // Create a minimal FLAC stream info
        StreamInfo stream_info;
        stream_info.codec_name = "flac";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        stream_info.bits_per_sample = 16;
        
        std::cout << "Creating FLAC codec..." << std::endl;
        FLACCodec codec(stream_info);
        
        std::cout << "Initializing FLAC codec..." << std::endl;
        if (!codec.initialize()) {
            std::cout << "ERROR: Failed to initialize FLAC codec" << std::endl;
            return 1;
        }
        
        std::cout << "FLAC codec initialized successfully" << std::endl;
        
        // Create an empty test chunk
        MediaChunk test_chunk;
        test_chunk.data = std::vector<uint8_t>(1024, 0);  // 1KB of zeros
        
        std::cout << "Attempting to decode test chunk with " << test_chunk.data.size() << " bytes" << std::endl;
        
        AudioFrame result = codec.decode(test_chunk);
        
        std::cout << "Decode result: " << result.getSampleFrameCount() << " sample frames" << std::endl;
        std::cout << "Result samples size: " << result.samples.size() << std::endl;
        std::cout << "Result channels: " << result.channels << std::endl;
        std::cout << "Result sample rate: " << result.sample_rate << std::endl;
        
        // Get codec statistics
        FLACCodecStats stats = codec.getStats();
        std::cout << "Codec stats:" << std::endl;
        std::cout << "  Frames decoded: " << stats.frames_decoded << std::endl;
        std::cout << "  Samples decoded: " << stats.samples_decoded << std::endl;
        std::cout << "  Error count: " << stats.error_count << std::endl;
        std::cout << "  Total bytes processed: " << stats.total_bytes_processed << std::endl;
        
        std::cout << "=== FLAC Simple Debug Test Complete ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "ERROR: Exception during test: " << e.what() << std::endl;
        return 1;
    }
}