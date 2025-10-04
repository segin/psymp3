/*
 * test_flac_deadlock_fix.cpp - Test for FLAC codec deadlock fix
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

#ifdef HAVE_FLAC

int main() {
    Debug::log("test", "Testing FLAC codec deadlock fix...");
    
    try {
        // Create a minimal FLAC stream info
        StreamInfo stream_info;
        stream_info.codec_name = "flac";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        stream_info.bits_per_sample = 16;
        
        // Create FLAC codec
        FLACCodec codec(stream_info);
        
        // Initialize codec
        if (!codec.initialize()) {
            Debug::log("test", "ERROR: Failed to initialize FLAC codec");
            return 1;
        }
        
        Debug::log("test", "SUCCESS: FLAC codec initialized without deadlock");
        
        // Test basic functionality
        MediaChunk empty_chunk;
        empty_chunk.data.resize(1024, 0); // Empty data
        
        AudioFrame frame = codec.decode(empty_chunk);
        Debug::log("test", "SUCCESS: FLAC codec decode completed without deadlock");
        
        Debug::log("test", "FLAC codec deadlock fix test PASSED");
        return 0;
        
    } catch (const std::exception& e) {
        Debug::log("test", "ERROR: Exception during test: ", e.what());
        return 1;
    }
}

#else

int main() {
    Debug::log("test", "FLAC support not compiled in, test skipped");
    return 0;
}

#endif