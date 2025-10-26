/*
 * test_flac_audio_output_debug.cpp - Debug FLAC audio output pipeline
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

#ifdef HAVE_FLAC

class FLACAudioOutputDebugger {
public:
    static void debugAudioPipeline(const std::string& flac_file) {
        std::cout << "=== FLAC Audio Output Pipeline Debug ===" << std::endl;
        std::cout << "File: " << flac_file << std::endl;
        
        try {
            // Step 1: Check if file exists and is readable
            std::cout << "\n1. File Access Check:" << std::endl;
            std::ifstream file(flac_file, std::ios::binary);
            if (!file.is_open()) {
                std::cout << "   ERROR: Cannot open file: " << flac_file << std::endl;
                return;
            }
            
            file.seekg(0, std::ios::end);
            size_t file_size = file.tellg();
            file.seekg(0, std::ios::beg);
            std::cout << "   File size: " << file_size << " bytes" << std::endl;
            
            // Read first few bytes to check FLAC signature
            std::vector<uint8_t> header(4);
            file.read(reinterpret_cast<char*>(header.data()), 4);
            if (header[0] == 'f' && header[1] == 'L' && header[2] == 'a' && header[3] == 'C') {
                std::cout << "   FLAC signature found: fLaC" << std::endl;
            } else {
                std::cout << "   WARNING: No FLAC signature found" << std::endl;
            }
            file.close();
            
            // Step 2: Test IOHandler
            std::cout << "\n2. IOHandler Test:" << std::endl;
            auto io_handler = std::make_unique<FileIOHandler>(flac_file);
            if (!io_handler) {
                std::cout << "   ERROR: Failed to create IOHandler" << std::endl;
                return;
            }
            std::cout << "   IOHandler created successfully" << std::endl;
            
            // Step 3: Test FLAC Demuxer
            std::cout << "\n3. FLAC Demuxer Test:" << std::endl;
            FLACDemuxer demuxer(std::move(io_handler));
            if (!demuxer.parseContainer()) {
                std::cout << "   ERROR: Failed to parse FLAC container" << std::endl;
                return;
            }
            std::cout << "   FLAC demuxer initialized successfully" << std::endl;
            
            // Get stream info
            auto streams = demuxer.getStreams();
            if (streams.empty()) {
                std::cout << "   ERROR: No streams found in FLAC file" << std::endl;
                return;
            }
            
            const StreamInfo& stream_info = streams[0];
            std::cout << "   Stream info: " << stream_info.sample_rate << "Hz, " 
                      << stream_info.channels << " channels, " 
                      << stream_info.bits_per_sample << " bits" << std::endl;
            
            // Step 4: Test FLAC Codec Creation
            std::cout << "\n4. FLAC Codec Test:" << std::endl;
            FLACCodec codec(stream_info);
            if (!codec.initialize()) {
                std::cout << "   ERROR: Failed to initialize FLAC codec" << std::endl;
                return;
            }
            std::cout << "   FLAC codec initialized successfully" << std::endl;
            
            // Step 5: Test Decoding Process
            std::cout << "\n5. Decoding Test:" << std::endl;
            int frames_processed = 0;
            int frames_with_audio = 0;
            size_t total_samples = 0;
            
            for (int i = 0; i < 10; ++i) { // Test first 10 frames
                MediaChunk chunk = demuxer.readChunk(); // Read next chunk
                if (chunk.data.empty()) {
                    std::cout << "   No more chunks available after " << i << " frames" << std::endl;
                    break;
                }
                
                std::cout << "   Frame " << i << ": chunk size = " << chunk.data.size() << " bytes" << std::endl;
                
                AudioFrame audio_frame = codec.decode(chunk);
                frames_processed++;
                
                if (audio_frame.getSampleFrameCount() > 0) {
                    frames_with_audio++;
                    total_samples += audio_frame.samples.size();
                    
                    std::cout << "     -> AudioFrame: " << audio_frame.getSampleFrameCount() 
                              << " sample frames, " << audio_frame.samples.size() << " samples, "
                              << audio_frame.channels << " channels, " << audio_frame.sample_rate << "Hz" << std::endl;
                    
                    // Check if samples contain actual audio data (not all zeros)
                    bool has_non_zero = false;
                    for (size_t j = 0; j < std::min(audio_frame.samples.size(), size_t(100)); ++j) {
                        if (audio_frame.samples[j] != 0) {
                            has_non_zero = true;
                            break;
                        }
                    }
                    
                    if (has_non_zero) {
                        std::cout << "     -> Contains non-zero audio data ✓" << std::endl;
                        
                        // Show sample values for first few samples
                        std::cout << "     -> First few samples: ";
                        for (size_t j = 0; j < std::min(audio_frame.samples.size(), size_t(8)); ++j) {
                            std::cout << audio_frame.samples[j] << " ";
                        }
                        std::cout << std::endl;
                    } else {
                        std::cout << "     -> WARNING: All samples are zero (silence)" << std::endl;
                    }
                } else {
                    std::cout << "     -> Empty AudioFrame returned" << std::endl;
                }
            }
            
            // Step 6: Summary
            std::cout << "\n6. Summary:" << std::endl;
            std::cout << "   Frames processed: " << frames_processed << std::endl;
            std::cout << "   Frames with audio: " << frames_with_audio << std::endl;
            std::cout << "   Total samples decoded: " << total_samples << std::endl;
            
            if (frames_with_audio == 0) {
                std::cout << "   PROBLEM: No audio frames produced by FLAC codec!" << std::endl;
                
                // Additional debugging
                std::cout << "\n7. Additional Codec Debug:" << std::endl;
                auto stats = codec.getStats();
                std::cout << "   Codec stats:" << std::endl;
                std::cout << "     Frames decoded: " << stats.frames_decoded << std::endl;
                std::cout << "     Samples decoded: " << stats.samples_decoded << std::endl;
                std::cout << "     Error count: " << stats.error_count << std::endl;
                std::cout << "     CRC errors: " << stats.crc_errors << std::endl;
                
            } else {
                std::cout << "   SUCCESS: FLAC codec is producing audio data!" << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cout << "   EXCEPTION: " << e.what() << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <flac_file>" << std::endl;
        return 1;
    }
    
    std::string flac_file = argv[1];
    FLACAudioOutputDebugger::debugAudioPipeline(flac_file);
    
    return 0;
}

#else
int main() {
    std::cout << "FLAC support not compiled in" << std::endl;
    return 1;
}
#endif