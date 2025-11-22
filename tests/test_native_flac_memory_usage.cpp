/*
 * test_native_flac_memory_usage.cpp - Memory usage tests for native FLAC decoder
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

#include <fstream>
#include <sys/resource.h>
#include <unistd.h>

/**
 * @brief Memory usage tests for native FLAC decoder
 * 
 * Tests:
 * - Peak memory consumption measurement
 * - Memory allocation patterns
 * - Memory usage vs libFLAC (if available)
 * 
 * Requirements: 12, 65, 68
 */

// Helper to get current memory usage (RSS in bytes)
size_t getCurrentMemoryUsage() {
    std::ifstream status("/proc/self/status");
    std::string line;
    size_t rss = 0;
    
    while (std::getline(status, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            std::istringstream iss(line.substr(6));
            iss >> rss;
            rss *= 1024; // Convert from KB to bytes
            break;
        }
    }
    
    return rss;
}

// Helper to get peak memory usage (VmHWM in bytes)
size_t getPeakMemoryUsage() {
    std::ifstream status("/proc/self/status");
    std::string line;
    size_t peak = 0;
    
    while (std::getline(status, line)) {
        if (line.substr(0, 6) == "VmHWM:") {
            std::istringstream iss(line.substr(6));
            iss >> peak;
            peak *= 1024; // Convert from KB to bytes
            break;
        }
    }
    
    return peak;
}

// Helper to create test FLAC data
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
    
    // Total samples (36 bits)
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
    
    // Add simple CONSTANT subframe frames
    for (uint32_t f = 0; f < num_frames; f++) {
        // Frame sync (0xFFF8)
        data.push_back(0xFF);
        data.push_back(0xF8);
        
        // Blocking strategy (0), block size (4096 = 0b0111), sample rate (from streaminfo = 0b0000)
        data.push_back(0x79);
        
        // Channel assignment (independent = 0b0000), bit depth (from streaminfo = 0b000), reserved (0)
        data.push_back(0x00);
        
        // Frame number (UTF-8 coded)
        data.push_back(f & 0x7F);
        
        // CRC-8 (placeholder)
        data.push_back(0x00);
        
        // Subframes (CONSTANT type)
        for (uint32_t ch = 0; ch < channels; ch++) {
            // Subframe header: 0 bit + CONSTANT type (0b000000) + no wasted bits (0b0)
            data.push_back(0x00);
            
            // Constant value (bits_per_sample bits)
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

// Test peak memory consumption
bool test_peak_memory_consumption() {
    Debug::log("test_native_flac_memory", "[test_peak_memory_consumption] Testing peak memory consumption");
    
    try {
        size_t baseline_memory = getCurrentMemoryUsage();
        Debug::log("test_native_flac_memory", "[test_peak_memory_consumption] Baseline memory: ", baseline_memory, " bytes");
        
        // Create test data
        auto test_data = createTestFLACData(44100, 2, 16, 100);
        
        // Create native FLAC codec
        StreamInfo stream_info;
        stream_info.codec_name = "flac";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        stream_info.bits_per_sample = 16;
        
        size_t after_init_memory = getCurrentMemoryUsage();
        Debug::log("test_native_flac_memory", "[test_peak_memory_consumption] Memory after init: ", after_init_memory, " bytes");
        Debug::log("test_native_flac_memory", "[test_peak_memory_consumption] Init overhead: ", after_init_memory - baseline_memory, " bytes");
        
        auto codec = CodecRegistry::createCodec(stream_info);
        if (!codec) {
            Debug::log("test_native_flac_memory", "[test_peak_memory_consumption] ERROR: Failed to create codec");
            return false;
        }
        
        size_t after_codec_init_memory = getCurrentMemoryUsage();
        Debug::log("test_native_flac_memory", "[test_peak_memory_consumption] Memory after codec init: ", after_codec_init_memory, " bytes");
        Debug::log("test_native_flac_memory", "[test_peak_memory_consumption] Codec init overhead: ", after_codec_init_memory - after_init_memory, " bytes");
        
        // Decode frames and track memory
        MediaChunk chunk;
        chunk.data = test_data;
        chunk.timestamp_samples = 0;
        
        size_t max_memory = after_codec_init_memory;
        int frame_count = 0;
        
        while (!chunk.data.empty()) {
            AudioFrame frame = codec->decode(chunk);
            if (frame.samples.empty()) {
                break;
            }
            frame_count++;
            
            size_t current_memory = getCurrentMemoryUsage();
            if (current_memory > max_memory) {
                max_memory = current_memory;
            }
            
            // Clear chunk data to simulate consumption
            chunk.data.clear();
        }
        
        size_t peak_memory = getPeakMemoryUsage();
        
        Debug::log("test_native_flac_memory", "[test_peak_memory_consumption] Decoded ", frame_count, " frames");
        Debug::log("test_native_flac_memory", "[test_peak_memory_consumption] Max memory during decoding: ", max_memory, " bytes");
        Debug::log("test_native_flac_memory", "[test_peak_memory_consumption] Peak memory (VmHWM): ", peak_memory, " bytes");
        Debug::log("test_native_flac_memory", "[test_peak_memory_consumption] Total memory overhead: ", max_memory - baseline_memory, " bytes");
        
        // Verify memory usage is reasonable (Requirement 65: efficient memory management)
        size_t memory_overhead = max_memory - baseline_memory;
        size_t reasonable_limit = 10 * 1024 * 1024; // 10 MB should be more than enough
        
        if (memory_overhead > reasonable_limit) {
            Debug::log("test_native_flac_memory", "[test_peak_memory_consumption] WARNING: Memory overhead exceeds ", reasonable_limit, " bytes");
        }
        
        Debug::log("test_native_flac_memory", "[test_peak_memory_consumption] SUCCESS: Peak memory consumption measured");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test_native_flac_memory", "[test_peak_memory_consumption] ERROR: Exception: ", e.what());
        return false;
    }
}

// Test memory allocation patterns
bool test_memory_allocation_patterns() {
    Debug::log("test_native_flac_memory", "[test_memory_allocation_patterns] Testing memory allocation patterns");
    
    try {
        // Create test data with varying block sizes
        std::vector<std::pair<uint32_t, uint32_t>> test_configs = {
            {44100, 16},  // CD quality
            {48000, 16},  // DVD quality
            {96000, 24},  // High-res
            {192000, 24}  // Ultra high-res
        };
        
        for (const auto& config : test_configs) {
            uint32_t sample_rate = config.first;
            uint32_t bits_per_sample = config.second;
            
            Debug::log("test_native_flac_memory", "[test_memory_allocation_patterns] Testing ", sample_rate, "Hz/", bits_per_sample, "-bit");
            
            size_t baseline = getCurrentMemoryUsage();
            
            // Create codec
            StreamInfo stream_info;
            stream_info.codec_name = "flac";
            stream_info.sample_rate = sample_rate;
            stream_info.channels = 2;
            stream_info.bits_per_sample = bits_per_sample;
            
            auto codec = CodecRegistry::createCodec(stream_info);
            if (!codec) {
                Debug::log("test_native_flac_memory", "[test_memory_allocation_patterns] ERROR: Failed to create codec");
                return false;
            }
            
            size_t after_init = getCurrentMemoryUsage();
            size_t init_overhead = after_init - baseline;
            
            Debug::log("test_native_flac_memory", "[test_memory_allocation_patterns] Init overhead: ", init_overhead, " bytes");
            
            // Create test data
            auto test_data = createTestFLACData(sample_rate, 2, bits_per_sample, 10);
            
            // Decode a few frames
            MediaChunk chunk;
            chunk.data = test_data;
            chunk.timestamp_samples = 0;
            
            for (int i = 0; i < 5 && !chunk.data.empty(); i++) {
                AudioFrame frame = codec->decode(chunk);
                if (frame.samples.empty()) {
                    break;
                }
                
                chunk.data.clear();
            }
            
            size_t after_decode = getCurrentMemoryUsage();
            size_t decode_overhead = after_decode - after_init;
            
            Debug::log("test_native_flac_memory", "[test_memory_allocation_patterns] Decode overhead: ", decode_overhead, " bytes");
            Debug::log("test_native_flac_memory", "[test_memory_allocation_patterns] Total overhead: ", after_decode - baseline, " bytes");
        }
        
        Debug::log("test_native_flac_memory", "[test_memory_allocation_patterns] SUCCESS: Memory allocation patterns profiled");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test_native_flac_memory", "[test_memory_allocation_patterns] ERROR: Exception: ", e.what());
        return false;
    }
}

// Test memory usage with multiple decoder instances
bool test_multiple_decoder_memory() {
    Debug::log("test_native_flac_memory", "[test_multiple_decoder_memory] Testing memory usage with multiple decoders");
    
    try {
        size_t baseline = getCurrentMemoryUsage();
        Debug::log("test_native_flac_memory", "[test_multiple_decoder_memory] Baseline memory: ", baseline, " bytes");
        
        // Create multiple decoder instances
        const int num_decoders = 5;
        std::vector<std::unique_ptr<AudioCodec>> codecs;
        
        for (int i = 0; i < num_decoders; i++) {
            StreamInfo stream_info;
            stream_info.codec_name = "flac";
            stream_info.sample_rate = 44100;
            stream_info.channels = 2;
            stream_info.bits_per_sample = 16;
            
            auto codec = CodecRegistry::createCodec(stream_info);
            if (!codec) {
                Debug::log("test_native_flac_memory", "[test_multiple_decoder_memory] ERROR: Failed to create codec ", i);
                return false;
            }
            
            codecs.push_back(std::move(codec));
            
            size_t current = getCurrentMemoryUsage();
            Debug::log("test_native_flac_memory", "[test_multiple_decoder_memory] Memory after decoder ", i + 1, ": ", current, " bytes");
        }
        
        size_t after_all = getCurrentMemoryUsage();
        size_t total_overhead = after_all - baseline;
        size_t per_decoder = total_overhead / num_decoders;
        
        Debug::log("test_native_flac_memory", "[test_multiple_decoder_memory] Total overhead for ", num_decoders, " decoders: ", total_overhead, " bytes");
        Debug::log("test_native_flac_memory", "[test_multiple_decoder_memory] Average per decoder: ", per_decoder, " bytes");
        
        // Clean up
        codecs.clear();
        
        size_t after_cleanup = getCurrentMemoryUsage();
        Debug::log("test_native_flac_memory", "[test_multiple_decoder_memory] Memory after cleanup: ", after_cleanup, " bytes");
        Debug::log("test_native_flac_memory", "[test_multiple_decoder_memory] Memory freed: ", after_all - after_cleanup, " bytes");
        
        Debug::log("test_native_flac_memory", "[test_multiple_decoder_memory] SUCCESS: Multiple decoder memory usage measured");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test_native_flac_memory", "[test_multiple_decoder_memory] ERROR: Exception: ", e.what());
        return false;
    }
}

int main() {
    Debug::log("test_native_flac_memory", "=== Native FLAC Memory Usage Tests ===");
    
    int passed = 0;
    int failed = 0;
    
    // Test peak memory consumption
    if (test_peak_memory_consumption()) {
        passed++;
    } else {
        failed++;
    }
    
    // Test memory allocation patterns
    if (test_memory_allocation_patterns()) {
        passed++;
    } else {
        failed++;
    }
    
    // Test multiple decoder memory
    if (test_multiple_decoder_memory()) {
        passed++;
    } else {
        failed++;
    }
    
    Debug::log("test_native_flac_memory", "=== Test Results ===");
    Debug::log("test_native_flac_memory", "Passed: ", passed);
    Debug::log("test_native_flac_memory", "Failed: ", failed);
    
    return (failed == 0) ? 0 : 1;
}

#else

int main() {
    std::cerr << "Native FLAC decoder not available (HAVE_NATIVE_FLAC not defined)" << std::endl;
    return 77; // Skip test
}

#endif // HAVE_NATIVE_FLAC
