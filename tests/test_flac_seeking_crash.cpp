#include "psymp3.h"

/**
 * @brief Test FLAC seeking to reproduce memory corruption crash
 * 
 * This test attempts to reproduce the "double free or corruption" crash
 * that occurs when seeking back and forth in FLAC files.
 */

int main() {
    std::cout << "Testing FLAC seeking crash reproduction..." << std::endl;
    
    try {
        // Create a simple FLAC test file in memory
        std::vector<uint8_t> flac_data;
        
        // FLAC signature
        flac_data.insert(flac_data.end(), {'f', 'L', 'a', 'C'});
        
        // STREAMINFO metadata block (last metadata block)
        flac_data.push_back(0x80); // Last metadata block flag + STREAMINFO type (0)
        flac_data.push_back(0x00); // Length (3 bytes) = 34 bytes
        flac_data.push_back(0x00);
        flac_data.push_back(0x22);
        
        // STREAMINFO data (34 bytes) - simplified
        for (int i = 0; i < 34; i++) {
            flac_data.push_back(0x00);
        }
        
        // Add a simple FLAC frame
        flac_data.push_back(0xFF); // Sync code
        flac_data.push_back(0xF8); // Sync + reserved + blocking strategy
        flac_data.push_back(0x69); // Block size + sample rate
        flac_data.push_back(0x02); // Channel assignment + sample size
        flac_data.push_back(0x00); // Frame number
        flac_data.push_back(0x0F); // Block size - 1 (high)
        flac_data.push_back(0xFF); // Block size - 1 (low)
        flac_data.push_back(0x00); // Header CRC
        
        // Minimal subframe data
        for (int i = 0; i < 10; i++) {
            flac_data.push_back(0x00);
        }
        
        // Frame CRC
        flac_data.push_back(0x00);
        flac_data.push_back(0x00);
        
        std::cout << "Created test FLAC data (" << flac_data.size() << " bytes)" << std::endl;
        
        // Create StreamInfo
        StreamInfo stream_info;
        stream_info.codec_type = "audio";
        stream_info.codec_name = "flac";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        stream_info.bits_per_sample = 16;
        stream_info.duration_samples = 44100 * 10; // 10 seconds
        
        // Create FLAC codec instance
        FLACCodec codec(stream_info);
        
        std::cout << "Initializing FLAC codec..." << std::endl;
        if (!codec.initialize()) {
            std::cerr << "ERROR: Failed to initialize FLAC codec" << std::endl;
            return 1;
        }
        
        std::cout << "FLAC codec initialized successfully" << std::endl;
        
        // Simulate seeking back and forth multiple times
        std::cout << "Starting seeking stress test..." << std::endl;
        
        for (int iteration = 0; iteration < 50; iteration++) {
            std::cout << "Iteration " << iteration + 1 << "/50" << std::endl;
            
            try {
                // Create MediaChunk from our test data
                MediaChunk chunk(1, flac_data);
                
                // Try to decode
                AudioFrame frame = codec.decode(chunk);
                
                // Simulate seeking by reinitializing (this might trigger the bug)
                if (iteration % 5 == 0) {
                    std::cout << "  Simulating seek (reinitialize)..." << std::endl;
                    
                    // This might be where the memory corruption happens
                    codec.initialize();
                }
                
                // Try different operations that might trigger memory issues
                if (iteration % 3 == 0) {
                    std::cout << "  Testing frame processing..." << std::endl;
                    
                    // Process the frame data
                    if (!frame.samples.empty()) {
                        // Access the samples (this might trigger corruption if memory is bad)
                        volatile int16_t first_sample = frame.samples[0];
                        volatile size_t sample_count = frame.samples.size();
                        (void)first_sample; // Suppress unused variable warning
                        (void)sample_count;
                    }
                }
                
            } catch (const std::exception& e) {
                std::cerr << "Exception in iteration " << iteration + 1 << ": " << e.what() << std::endl;
                // Continue to see if we can trigger the crash
            }
            
            // Small delay to make output readable
            if (iteration % 10 == 9) {
                std::cout << "  Completed " << iteration + 1 << " iterations..." << std::endl;
            }
        }
        
        std::cout << "Seeking stress test completed without crash" << std::endl;
        std::cout << "FLAC seeking crash test PASSED" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Exception during test: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "ERROR: Unknown exception during test" << std::endl;
        return 1;
    }
}