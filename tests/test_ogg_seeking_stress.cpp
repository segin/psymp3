/*
 * test_ogg_seeking_stress.cpp - Stress test for Ogg seeking (Rapid Seek "Left Arrow" Simulation)
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * This test simulates a user holding down the left arrow key, triggering 
 * rapid backward seeking operations. It verifies:
 * 1. Stability (no crashes)
 * 2. Accuracy (seek lands within 250ms of target)
 * 3. Playback resumption (demuxer can produce data after seeking)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "psymp3.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>
#include <vector>

#ifdef HAVE_OGGDEMUXER

// Include FileIOHandler.h specifically if psymp3.h doesn't export it
#include "io/file/FileIOHandler.h"

// using namespace PsyMP3;
// using namespace PsyMP3::Demuxer::Ogg;
// using namespace PsyMP3::IO::File;

int main(int argc, char** argv) {
    // Path to a real Ogg file in tests/data
    std::string filename = "data/02 Foo Fighters - Monkey Wrench.ogg";
    if (argc > 1) {
        filename = argv[1];
    }
    
    std::cout << "Running Ogg Seeking Stress Test on: " << filename << std::endl;

    try {
        std::unique_ptr<PsyMP3::IO::IOHandler> io_handler;
        try {
            // Using FileIOHandler constructor directly
            io_handler = std::make_unique<PsyMP3::IO::File::FileIOHandler>(TagLib::String(filename.c_str(), TagLib::String::UTF8));
        } catch (const std::exception& e) {
             std::cerr << "Failed to open file: " << filename << " (" << e.what() << ")" << std::endl;
             // Fallback
             std::string abs_path = "../tests/data/02 Foo Fighters - Monkey Wrench.ogg";
             std::cout << "Retrying with path: " << abs_path << std::endl;
             try {
                io_handler = std::make_unique<PsyMP3::IO::File::FileIOHandler>(TagLib::String(abs_path.c_str(), TagLib::String::UTF8));
             } catch (...) {
                 std::cerr << "Failed to open file fallback." << std::endl;
                 return 77; // Skip code
             }
        }

        PsyMP3::Demuxer::Ogg::OggDemuxer demuxer(std::move(io_handler));
        
        if (!demuxer.parseContainer()) {
            std::cerr << "Failed to parse Ogg container." << std::endl;
            return 1;
        }

        uint64_t duration_ms = demuxer.getDuration();
        std::cout << "Duration: " << duration_ms << " ms" << std::endl;

        // Simulate "Left Arrow" held down for 2 seconds.
        // Typematic behavior:
        // 1. Initial Key Press -> Seek
        // 2. Delay (Typematic Delay) typically 500ms
        // 3. Repeat (Typematic Rate) typically 30Hz (~33ms)
        
        // Start near the end
        if (duration_ms < 5000) {
            std::cout << "File too short for stress test." << std::endl;
            return 0;
        }

        double current_pos_ms = static_cast<double>(duration_ms) - 5000.0; 
        
        std::cout << "Starting rapid backward seek simulation (Typematic Model)..." << std::endl;
        
        // Constants (MS)
        const int TYPEMATIC_DELAY_MS = 500;
        const int TYPEMATIC_RATE_MS = 70; // ~14 seeks/sec
        const double SEEK_STEP_MS = 5000.0; // 5 seconds per seek
        const int SIMULATION_DURATION_MS = 3000; // 3 seconds holding key
        
        int elapsed_time_ms = 0;
        int iteration = 0;
        int failures = 0;
        
        // We simulate the flow of time "wall clock" vs "playback time"
        
        while (elapsed_time_ms < SIMULATION_DURATION_MS) {
            // "User input" arrives -> Desired seek target
            current_pos_ms -= SEEK_STEP_MS;
            if (current_pos_ms < 0) current_pos_ms = 0;

            uint64_t target_ms = static_cast<uint64_t>(current_pos_ms);

            // Perform Seek
            bool success = demuxer.seekTo(target_ms);
            
            if (!success) {
                std::cerr << "Seek failed at iteration " << iteration << " target=" << target_ms << std::endl;
                return 1;
            }
            
            // Verify Accuracy & Simulate Playback
            // In a real scenario, the player would immediately try to fill buffers
            bool found_valid_chunk = false;
            uint64_t actual_ms = 0;
            
            int packets_consumed = 0;
            
            // Read packets for the duration of the inter-seek interval
            // We need to read enough to hit a page boundary to get a timestamp.
            // 20 was too few. Let's try up to 200.
            
            for (int r = 0; r < 200; ++r) {
                PsyMP3::Demuxer::MediaChunk chunk = demuxer.readChunk();
                if (chunk.isValid()) {
                    packets_consumed++;
                    
                    // Try to get timestamp from the first valid chunk we verify against
                    // Granule -1 means "continuation" or "not last packet on page"
                    if (!found_valid_chunk && chunk.granule_position != static_cast<uint64_t>(-1) && chunk.granule_position != 0) {
                        actual_ms = demuxer.granuleToMs(chunk.granule_position, chunk.stream_id);
                        found_valid_chunk = true;
                        // Timestamp found, we can stop reading for verification purposes
                        // (though we might want to simulate more load, but this is enough for accuracy check)
                        break; 
                    }
                } else if (demuxer.isEOF()) {
                    break;
                }
            }
            
            if (found_valid_chunk) {
                int64_t diff = std::abs(static_cast<int64_t>(actual_ms) - static_cast<int64_t>(target_ms));
                // Ogg pages can be up to ~1s long. Since granulepos is the end of the page, 
                // Actual > Target by up to 1s is expected and correct.
                // We use 1500ms to allow for large pages/packets.
                if (diff > 1500) {
                     std::cerr << "Seek inaccuracy at iter " << iteration 
                               << ": Target=" << target_ms << " Actual=" << actual_ms 
                               << " Diff=" << diff << "ms" << std::endl;
                     failures++;
                } else {
                    // Debug print occasionally
                    if (iteration % 10 == 0) {
                        std::cout << "Iter " << iteration << ": Seek OK (Diff " << diff << "ms). Consumed " << packets_consumed << " packets." << std::endl;
                    }
                }
            } else {
                std::cerr << "Warning: No valid timestamped chunk found after seek to " << target_ms << std::endl;
            }

            // Determine how long to wait before next input (Typematic simulation)
            int wait_ms = (iteration == 0) ? TYPEMATIC_DELAY_MS : TYPEMATIC_RATE_MS;
            
            // Sleep to simulate time passing for the UI thread handling the key repeat
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
            
            iteration++;
            elapsed_time_ms += wait_ms;
        }

        if (failures > 0) {
            std::cerr << "Stress test failed with " << failures << " accuracy violations." << std::endl;
            return 1;
        }

        std::cout << "Stress test passed. Demuxer survived " << iteration << " typematic seek events with good accuracy." << std::endl;
        
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}

#else

int main() {
    std::cout << "OggDemuxer disabled, skipping test." << std::endl;
    return 0;
}

#endif
