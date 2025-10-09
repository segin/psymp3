/*
 * test_flac_debug.cpp - Debug test for FLAC codec data flow
 */

#include "psymp3.h"

#ifdef HAVE_FLAC

int main() {
    Debug::log("test", "=== FLAC Debug Test ===");
    
    try {
        // Create a minimal FLAC stream info
        StreamInfo stream_info;
        stream_info.codec_name = "flac";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        stream_info.bits_per_sample = 16;
        
        Debug::log("test", "Creating FLAC codec...");
        FLACCodec codec(stream_info);
        
        Debug::log("test", "Initializing FLAC codec...");
        if (!codec.initialize()) {
            Debug::log("test", "ERROR: Failed to initialize FLAC codec");
            return 1;
        }
        
        Debug::log("test", "FLAC codec initialized successfully");
        
        // Create a test chunk with some dummy FLAC data
        // This is a minimal FLAC frame header pattern
        MediaChunk test_chunk;
        test_chunk.data = {
            0xFF, 0xF8, 0x69, 0x0C,  // FLAC sync + frame header start
            0x00, 0x00, 0x00, 0x00,  // More header data
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00
        };
        
        Debug::log("test", "Attempting to decode test chunk with ", test_chunk.data.size(), " bytes");
        
        AudioFrame result = codec.decode(test_chunk);
        
        Debug::log("test", "Decode result: ", result.getSampleFrameCount(), " sample frames");
        Debug::log("test", "Result samples size: ", result.samples.size());
        Debug::log("test", "Result channels: ", result.channels);
        Debug::log("test", "Result sample rate: ", result.sample_rate);
        
        if (result.getSampleFrameCount() > 0) {
            Debug::log("test", "SUCCESS: Got audio data from FLAC codec");
            
            // Check if samples are actually non-zero
            bool has_non_zero = false;
            for (size_t i = 0; i < std::min(result.samples.size(), size_t(10)); ++i) {
                if (result.samples[i] != 0) {
                    has_non_zero = true;
                    break;
                }
            }
            
            if (has_non_zero) {
                Debug::log("test", "SUCCESS: Audio data contains non-zero samples");
            } else {
                Debug::log("test", "WARNING: Audio data is all zeros (silence)");
            }
        } else {
            Debug::log("test", "WARNING: No audio data returned from FLAC codec");
        }
        
        // Get codec statistics
        FLACCodecStats stats = codec.getStats();
        Debug::log("test", "Codec stats:");
        Debug::log("test", "  Frames decoded: ", stats.frames_decoded);
        Debug::log("test", "  Samples decoded: ", stats.samples_decoded);
        Debug::log("test", "  Error count: ", stats.error_count);
        Debug::log("test", "  Total bytes processed: ", stats.total_bytes_processed);
        
        Debug::log("test", "=== FLAC Debug Test Complete ===");
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