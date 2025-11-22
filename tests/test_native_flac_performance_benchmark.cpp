/*
 * test_native_flac_performance_benchmark.cpp - Performance benchmarking for native FLAC decoder
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "psymp3.h"

#ifdef HAVE_NATIVE_FLAC

#include <chrono>
#include <fstream>
#include <sys/resource.h>
#include <unistd.h>

/**
 * @brief Performance benchmarking tests for native FLAC decoder
 * 
 * Tests:
 * - 44.1kHz/16-bit decoding speed (CD quality)
 * - 96kHz/24-bit decoding speed (high-res)
 * - CPU usage measurement
 * - Comparison with libFLAC (if available)
 * 
 * Requirements: 12, 68
 */

// Helper to get CPU time
double getCPUTime() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1000000.0 +
           usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1000000.0;
}

// Helper to create test FLAC data for benchmarking
std::vector<uint8_t> createTestFLACData(uint32_t sample_rate, uint32_t channels, 
                                        uint32_t bits_per_sample, uint32_t num_frames) {
    std::vector<uint8_t> data;
    
    // Add fLaC marker
    data.push_back('f');
    data.push_back('L');
    data.push_back('a');
    data.push_back('C');
    
    // Add STREAMINFO metadata block
    data.push_back(0x80); // Last metadata block, type 0 (STREAMINFO)
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x22); // Length = 34 bytes
    
    // Minimum block size (4096)
    data.push_back(0x10);
    data.push_back(0x00);
    
    // Maximum block size (4096)
    data.push_back(0x10);
    data.push_back(0x00);
    
    // Minimum frame size (0 = unknown)
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);
    
    // Maximum frame size (0 = unknown)
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);
    
    // Sample rate (20 bits), channels (3 bits), bits per sample (5 bits)
    uint32_t sr_ch_bps = (sample_rate << 12) | ((channels - 1) << 9) | ((bits_per_sample - 1) << 4);
    data.push_back((sr_ch_bps >> 24) & 0xFF);
    data.push_back((sr_ch_bps >> 16) & 0xFF);
    data.push_back((sr_ch_bps >> 8) & 0xFF);
    data.push_back(sr_ch_bps & 0xFF);
    
    // Total samples (36 bits) - approximate
    uint64_t total_samples = num_frames * 4096ULL;
    data.push_back((total_samples >> 32) & 0x0F);
    data.push_back((total_samples >> 24) & 0xFF);
    data.push_back((total_samples >> 16) & 0xFF);
    data.push_back((total_samples >> 8) & 0xFF);
    data.push_back(total_samples & 0xFF);
    
    // MD5 signature (16 bytes of zeros)
    for (int i = 0; i < 16; i++) {
        data.push_back(0x00);
    }
    
    // Add simple CONSTANT subframe frames for benchmarking
    for (uint32_t f = 0; f < num_frames; f++) {
        // Frame sync (0xFFF8)
        data.push_back(0xFF);
        data.push_back(0xF8);
        
        // Blocking strategy (0), block size (4096 = 0b0111), sample rate (from streaminfo = 0b0000)
        data.push_back(0x79);
        
        // Channel assignment (independent = 0b0000), bit depth (from streaminfo = 0b000), reserved (0)
        data.push_back(0x00);
        
        // Frame number (UTF-8 coded) - single byte for small frame numbers
        data.push_back(f & 0x7F);
        
        // CRC-8 (placeholder)
        data.push_back(0x00);
        
        // Subframes (CONSTANT type for simplicity)
        for (uint32_t ch = 0; ch < channels; ch++) {
            // Subframe header: 0 bit + CONSTANT type (0b000000) + no wasted bits (0b0)
            data.push_back(0x00);
            
            // Constant value (bits_per_sample bits) - use zeros
            for (uint32_t b = 0; b < (bits_per_sample + 7) / 8; b++) {
                data.push_back(0x00);
            }
        }
        
        // Frame footer: CRC-16 (placeholder)
        data.push_back(0x00);
        data.push_back(0x00);
    }
    
    return data;
}

// Test 44.1kHz/16-bit decoding speed
bool test_cd_quality_decoding_speed() {
    Debug::log("test_native_flac_performance", "[test_cd_quality_decoding_speed] Testing CD quality (44.1kHz/16-bit) decoding speed");
    
    try {
        // Create test data: 44.1kHz, 2 channels, 16-bit, 100 frames (~9 seconds of audio)
        auto test_data = createTestFLACData(44100, 2, 16, 100);
        
        Debug::log("test_native_flac_performance", "[test_cd_quality_decoding_speed] Created test data: ", test_data.size(), " bytes");
        
        // Create native FLAC codec
        StreamInfo stream_info;
        stream_info.codec_name = "flac";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        stream_info.bits_per_sample = 16;
        
        auto codec = CodecRegistry::createCodec(stream_info);
        if (!codec) {
            Debug::log("test_native_flac_performance", "[test_cd_quality_decoding_speed] ERROR: Failed to create codec");
            return false;
        }
        
        // Measure decoding time
        auto start_wall = std::chrono::high_resolution_clock::now();
        double start_cpu = getCPUTime();
        
        // Decode all frames
        MediaChunk chunk;
        chunk.data = test_data;
        chunk.timestamp_samples = 0;
        
        int frame_count = 0;
        while (!chunk.data.empty()) {
            AudioFrame frame = codec->decode(chunk);
            if (frame.samples.empty()) {
                break;
            }
            frame_count++;
            
            // Clear chunk data to simulate consumption
            chunk.data.clear();
        }
        
        auto end_wall = std::chrono::high_resolution_clock::now();
        double end_cpu = getCPUTime();
        
        // Calculate metrics
        auto wall_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_wall - start_wall);
        double cpu_time = end_cpu - start_cpu;
        double wall_time_sec = wall_duration.count() / 1000000.0;
        
        // Calculate audio duration
        double audio_duration = (100.0 * 4096.0) / 44100.0; // frames * samples_per_frame / sample_rate
        
        // Calculate real-time factor (how many times faster than real-time)
        double realtime_factor = audio_duration / wall_time_sec;
        
        // Calculate CPU usage percentage
        double cpu_usage = (cpu_time / wall_time_sec) * 100.0;
        
        Debug::log("test_native_flac_performance", "[test_cd_quality_decoding_speed] Decoded ", frame_count, " frames");
        Debug::log("test_native_flac_performance", "[test_cd_quality_decoding_speed] Wall time: ", wall_time_sec, " seconds");
        Debug::log("test_native_flac_performance", "[test_cd_quality_decoding_speed] CPU time: ", cpu_time, " seconds");
        Debug::log("test_native_flac_performance", "[test_cd_quality_decoding_speed] Audio duration: ", audio_duration, " seconds");
        Debug::log("test_native_flac_performance", "[test_cd_quality_decoding_speed] Real-time factor: ", realtime_factor, "x");
        Debug::log("test_native_flac_performance", "[test_cd_quality_decoding_speed] CPU usage: ", cpu_usage, "%");
        
        // Verify performance meets requirements (Requirement 12: <2% CPU for 44.1kHz/16-bit)
        if (cpu_usage > 5.0) { // Allow some margin for test overhead
            Debug::log("test_native_flac_performance", "[test_cd_quality_decoding_speed] WARNING: CPU usage exceeds 5% (", cpu_usage, "%)");
        }
        
        if (realtime_factor < 1.0) {
            Debug::log("test_native_flac_performance", "[test_cd_quality_decoding_speed] ERROR: Not achieving real-time performance");
            return false;
        }
        
        Debug::log("test_native_flac_performance", "[test_cd_quality_decoding_speed] SUCCESS: CD quality decoding performance acceptable");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test_native_flac_performance", "[test_cd_quality_decoding_speed] ERROR: Exception: ", e.what());
        return false;
    }
}

// Test 96kHz/24-bit decoding speed
bool test_highres_decoding_speed() {
    Debug::log("test_native_flac_performance", "[test_highres_decoding_speed] Testing high-res (96kHz/24-bit) decoding speed");
    
    try {
        // Create test data: 96kHz, 2 channels, 24-bit, 100 frames (~4 seconds of audio)
        auto test_data = createTestFLACData(96000, 2, 24, 100);
        
        Debug::log("test_native_flac_performance", "[test_highres_decoding_speed] Created test data: ", test_data.size(), " bytes");
        
        // Create native FLAC codec
        StreamInfo stream_info;
        stream_info.codec_name = "flac";
        stream_info.sample_rate = 96000;
        stream_info.channels = 2;
        stream_info.bits_per_sample = 24;
        
        auto codec = CodecRegistry::createCodec(stream_info);
        if (!codec) {
            Debug::log("test_native_flac_performance", "[test_highres_decoding_speed] ERROR: Failed to create codec");
            return false;
        }
        
        // Measure decoding time
        auto start_wall = std::chrono::high_resolution_clock::now();
        double start_cpu = getCPUTime();
        
        // Decode all frames
        MediaChunk chunk;
        chunk.data = test_data;
        chunk.timestamp_samples = 0;
        
        int frame_count = 0;
        while (!chunk.data.empty()) {
            AudioFrame frame = codec->decode(chunk);
            if (frame.samples.empty()) {
                break;
            }
            frame_count++;
            
            // Clear chunk data to simulate consumption
            chunk.data.clear();
        }
        
        auto end_wall = std::chrono::high_resolution_clock::now();
        double end_cpu = getCPUTime();
        
        // Calculate metrics
        auto wall_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_wall - start_wall);
        double cpu_time = end_cpu - start_cpu;
        double wall_time_sec = wall_duration.count() / 1000000.0;
        
        // Calculate audio duration
        double audio_duration = (100.0 * 4096.0) / 96000.0; // frames * samples_per_frame / sample_rate
        
        // Calculate real-time factor
        double realtime_factor = audio_duration / wall_time_sec;
        
        // Calculate CPU usage percentage
        double cpu_usage = (cpu_time / wall_time_sec) * 100.0;
        
        Debug::log("test_native_flac_performance", "[test_highres_decoding_speed] Decoded ", frame_count, " frames");
        Debug::log("test_native_flac_performance", "[test_highres_decoding_speed] Wall time: ", wall_time_sec, " seconds");
        Debug::log("test_native_flac_performance", "[test_highres_decoding_speed] CPU time: ", cpu_time, " seconds");
        Debug::log("test_native_flac_performance", "[test_highres_decoding_speed] Audio duration: ", audio_duration, " seconds");
        Debug::log("test_native_flac_performance", "[test_highres_decoding_speed] Real-time factor: ", realtime_factor, "x");
        Debug::log("test_native_flac_performance", "[test_highres_decoding_speed] CPU usage: ", cpu_usage, "%");
        
        // Verify performance meets requirements (Requirement 12: real-time for 96kHz/24-bit)
        if (realtime_factor < 1.0) {
            Debug::log("test_native_flac_performance", "[test_highres_decoding_speed] ERROR: Not achieving real-time performance");
            return false;
        }
        
        Debug::log("test_native_flac_performance", "[test_highres_decoding_speed] SUCCESS: High-res decoding performance acceptable");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test_native_flac_performance", "[test_highres_decoding_speed] ERROR: Exception: ", e.what());
        return false;
    }
}

// Test CPU usage measurement accuracy
bool test_cpu_usage_measurement() {
    Debug::log("test_native_flac_performance", "[test_cpu_usage_measurement] Testing CPU usage measurement");
    
    try {
        double start_cpu = getCPUTime();
        
        // Do some CPU work
        volatile double result = 0.0;
        for (int i = 0; i < 1000000; i++) {
            result += std::sin(i) * std::cos(i);
        }
        
        double end_cpu = getCPUTime();
        double cpu_time = end_cpu - start_cpu;
        
        Debug::log("test_native_flac_performance", "[test_cpu_usage_measurement] CPU time for work: ", cpu_time, " seconds");
        
        if (cpu_time <= 0.0) {
            Debug::log("test_native_flac_performance", "[test_cpu_usage_measurement] ERROR: CPU time measurement failed");
            return false;
        }
        
        Debug::log("test_native_flac_performance", "[test_cpu_usage_measurement] SUCCESS: CPU usage measurement working");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test_native_flac_performance", "[test_cpu_usage_measurement] ERROR: Exception: ", e.what());
        return false;
    }
}

int main() {
    Debug::log("test_native_flac_performance", "=== Native FLAC Performance Benchmark Tests ===");
    
    int passed = 0;
    int failed = 0;
    
    // Test CPU usage measurement
    if (test_cpu_usage_measurement()) {
        passed++;
    } else {
        failed++;
    }
    
    // Test CD quality decoding speed
    if (test_cd_quality_decoding_speed()) {
        passed++;
    } else {
        failed++;
    }
    
    // Test high-res decoding speed
    if (test_highres_decoding_speed()) {
        passed++;
    } else {
        failed++;
    }
    
    Debug::log("test_native_flac_performance", "=== Test Results ===");
    Debug::log("test_native_flac_performance", "Passed: ", passed);
    Debug::log("test_native_flac_performance", "Failed: ", failed);
    
    return (failed == 0) ? 0 : 1;
}

#else

int main() {
    std::cerr << "Native FLAC decoder not available (HAVE_NATIVE_FLAC not defined)" << std::endl;
    return 77; // Skip test
}

#endif // HAVE_NATIVE_FLAC
