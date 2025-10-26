/*
 * test_flac_codec_simple_debug.cpp - Simple FLAC codec debug test
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

#ifdef HAVE_FLAC

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <flac_file>" << std::endl;
        return 1;
    }
    
    std::string flac_file = argv[1];
    std::cout << "=== Simple FLAC Codec Debug ===" << std::endl;
    std::cout << "File: " << flac_file << std::endl;
    
    try {
        // Step 1: Create a simple StreamInfo for testing
        StreamInfo stream_info;
        stream_info.codec_name = "flac";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        stream_info.bits_per_sample = 16;
        stream_info.duration_samples = 0; // Unknown
        
        std::cout << "\n1. Creating FLAC Codec:" << std::endl;
        std::cout << "   StreamInfo: " << stream_info.sample_rate << "Hz, " 
                  << stream_info.channels << " channels, " 
                  << stream_info.bits_per_sample << " bits" << std::endl;
        
        // Step 2: Create and initialize FLAC codec
        FLACCodec codec(stream_info);
        if (!codec.initialize()) {
            std::cout << "   ERROR: Failed to initialize FLAC codec" << std::endl;
            return 1;
        }
        std::cout << "   FLAC codec initialized successfully" << std::endl;
        
        // Step 3: Test with some dummy data to see if codec processes it
        std::cout << "\n2. Testing Codec Processing:" << std::endl;
        
        // Create a simple test chunk with FLAC header-like data
        MediaChunk test_chunk;
        test_chunk.data = {0x66, 0x4C, 0x61, 0x43}; // "fLaC" signature
        test_chunk.stream_id = 0;
        test_chunk.timestamp_ms = 0;
        
        std::cout << "   Testing with " << test_chunk.data.size() << " byte chunk" << std::endl;
        
        AudioFrame result = codec.decode(test_chunk);
        
        std::cout << "   Result: " << result.getSampleFrameCount() << " sample frames" << std::endl;
        std::cout << "   Samples size: " << result.samples.size() << std::endl;
        std::cout << "   Channels: " << result.channels << std::endl;
        std::cout << "   Sample rate: " << result.sample_rate << std::endl;
        
        if (result.getSampleFrameCount() > 0) {
            std::cout << "   SUCCESS: Codec produced audio output!" << std::endl;
            
            // Check if samples contain data
            bool has_non_zero = false;
            for (size_t i = 0; i < std::min(result.samples.size(), size_t(10)); ++i) {
                if (result.samples[i] != 0) {
                    has_non_zero = true;
                    break;
                }
            }
            
            if (has_non_zero) {
                std::cout << "   Audio contains non-zero samples ✓" << std::endl;
            } else {
                std::cout << "   WARNING: All samples are zero (silence)" << std::endl;
            }
        } else {
            std::cout << "   PROBLEM: Codec produced no audio output" << std::endl;
        }
        
        // Step 4: Check codec statistics
        std::cout << "\n3. Codec Statistics:" << std::endl;
        auto stats = codec.getStats();
        std::cout << "   Frames decoded: " << stats.frames_decoded << std::endl;
        std::cout << "   Samples decoded: " << stats.samples_decoded << std::endl;
        std::cout << "   Error count: " << stats.error_count << std::endl;
        std::cout << "   CRC errors: " << stats.crc_errors << std::endl;
        
        if (stats.frames_decoded == 0) {
            std::cout << "   ISSUE: No frames were decoded by the codec" << std::endl;
        }
        
        if (stats.error_count > 0) {
            std::cout << "   ISSUE: Codec reported " << stats.error_count << " errors" << std::endl;
        }
        
        std::cout << "\n=== Test Complete ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "EXCEPTION: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

#else
int main() {
    std::cout << "FLAC support not compiled in" << std::endl;
    return 1;
}
#endif