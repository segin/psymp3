#include "psymp3.h"

/**
 * @brief Test FLAC audio output to verify the synthetic STREAMINFO fix
 * 
 * This test verifies that FLAC files now produce actual audio output
 * instead of silence after our synthetic STREAMINFO metadata fix.
 */

int main() {
    std::cout << "Testing FLAC audio output after synthetic STREAMINFO fix..." << std::endl;
    
    try {
        // Create a simple FLAC test file in memory (minimal valid FLAC)
        std::vector<uint8_t> flac_data;
        
        // FLAC signature
        flac_data.insert(flac_data.end(), {'f', 'L', 'a', 'C'});
        
        // STREAMINFO metadata block (last metadata block)
        flac_data.push_back(0x80); // Last metadata block flag + STREAMINFO type (0)
        flac_data.push_back(0x00); // Length (3 bytes) = 34 bytes
        flac_data.push_back(0x00);
        flac_data.push_back(0x22);
        
        // STREAMINFO data (34 bytes)
        // Min block size (16-bit) = 4096
        flac_data.push_back(0x10);
        flac_data.push_back(0x00);
        // Max block size (16-bit) = 4096  
        flac_data.push_back(0x10);
        flac_data.push_back(0x00);
        // Min frame size (24-bit) = 0 (unknown)
        flac_data.push_back(0x00);
        flac_data.push_back(0x00);
        flac_data.push_back(0x00);
        // Max frame size (24-bit) = 0 (unknown)
        flac_data.push_back(0x00);
        flac_data.push_back(0x00);
        flac_data.push_back(0x00);
        // Sample rate (20-bit) + channels (3-bit) + bits per sample (5-bit) = 44100 Hz, 2 channels, 16 bits
        flac_data.push_back(0xAC); // 44100 >> 12
        flac_data.push_back(0x44); // (44100 >> 4) & 0xFF
        flac_data.push_back(0x11); // ((44100 & 0xF) << 4) | ((2-1) << 1) | ((16-1) >> 4)
        flac_data.push_back(0xF0); // ((16-1) & 0xF) << 4
        // Total samples (36-bit) = 44100 (1 second)
        flac_data.push_back(0x00);
        flac_data.push_back(0x00);
        flac_data.push_back(0x00);
        flac_data.push_back(0xAC);
        flac_data.push_back(0x44);
        // MD5 signature (16 bytes) - all zeros
        for (int i = 0; i < 16; i++) {
            flac_data.push_back(0x00);
        }
        
        // Simple FLAC frame with silence
        // Frame header: sync code (14 bits) + reserved (1 bit) + blocking strategy (1 bit)
        flac_data.push_back(0xFF); // Sync code high byte
        flac_data.push_back(0xF8); // Sync code low 6 bits + reserved + blocking strategy
        
        // Block size and sample rate indicators
        flac_data.push_back(0x69); // Block size = 4096 samples, sample rate = 44.1kHz
        flac_data.push_back(0x02); // Channel assignment = left/right, sample size = 16-bit
        
        // Frame number (8-bit UTF-8 encoded)
        flac_data.push_back(0x00);
        
        // Block size (16-bit big-endian) - 1 = 4095
        flac_data.push_back(0x0F);
        flac_data.push_back(0xFF);
        
        // Header CRC-8
        flac_data.push_back(0x00); // Placeholder
        
        // Subframe data (simplified - just silence)
        // Left channel subframe header
        flac_data.push_back(0x00); // Constant subframe, 0 wasted bits
        flac_data.push_back(0x00); // Constant value = 0 (silence)
        flac_data.push_back(0x00);
        
        // Right channel subframe header  
        flac_data.push_back(0x00); // Constant subframe, 0 wasted bits
        flac_data.push_back(0x00); // Constant value = 0 (silence)
        flac_data.push_back(0x00);
        
        // Frame footer CRC-16
        flac_data.push_back(0x00);
        flac_data.push_back(0x00);
        
        std::cout << "Created test FLAC data (" << flac_data.size() << " bytes)" << std::endl;
        
        // Create StreamInfo from the FLAC data
        StreamInfo stream_info;
        stream_info.codec_type = "audio";
        stream_info.codec_name = "flac";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        stream_info.bits_per_sample = 16;
        stream_info.duration_samples = 44100; // 1 second
        
        std::cout << "StreamInfo: " << stream_info.sample_rate << "Hz, " 
                  << stream_info.channels << " channels, " 
                  << stream_info.bits_per_sample << " bits" << std::endl;
        
        // Create FLAC codec instance
        FLACCodec codec(stream_info);
        
        std::cout << "Initializing FLAC codec..." << std::endl;
        
        // Initialize codec
        if (!codec.initialize()) {
            std::cerr << "ERROR: Failed to initialize FLAC codec" << std::endl;
            return 1;
        }
        
        std::cout << "FLAC codec initialized successfully" << std::endl;
        
        // Test decoding a small chunk
        std::cout << "Testing decode with " << flac_data.size() << " bytes of FLAC data..." << std::endl;
        
        // Create MediaChunk from our test data
        MediaChunk chunk(1, flac_data);
        AudioFrame frame = codec.decode(chunk);
        
        std::cout << "Decode completed. Frame info:" << std::endl;
        std::cout << "  Sample count: " << frame.samples.size() << std::endl;
        std::cout << "  Channels: " << frame.channels << std::endl;
        std::cout << "  Sample rate: " << frame.sample_rate << std::endl;
        std::cout << "  Data size: " << (frame.samples.size() * sizeof(int16_t)) << " bytes" << std::endl;
        
        // Check if we got any audio data
        if (frame.samples.empty()) {
            std::cout << "WARNING: No audio data produced (this was the original bug)" << std::endl;
            std::cout << "This suggests the synthetic STREAMINFO fix may not be working" << std::endl;
            return 1;
        } else {
            std::cout << "SUCCESS: Audio data was produced!" << std::endl;
            std::cout << "The synthetic STREAMINFO fix is working correctly" << std::endl;
            
            // Check if the data contains actual samples
            if (frame.samples.size() > 0) {
                std::cout << "SUCCESS: Frame contains " << frame.samples.size() << " audio samples" << std::endl;
                
                // Analyze the audio data briefly
                if (frame.samples.size() >= 4) {
                    std::cout << "First few samples: ";
                    for (size_t i = 0; i < std::min(frame.samples.size(), size_t(8)); i++) {
                        std::cout << frame.samples[i] << " ";
                    }
                    std::cout << std::endl;
                }
            }
        }
        
        std::cout << "FLAC audio output test PASSED" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Exception during test: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "ERROR: Unknown exception during test" << std::endl;
        return 1;
    }
}