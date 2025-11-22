/*
 * test_native_flac_threading.cpp - Threading tests for native FLAC decoder
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

#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <mutex>

/**
 * @brief Threading tests for native FLAC decoder
 * 
 * Tests:
 * - Concurrent decoder instances
 * - Lock contention measurement
 * - Thread safety validation
 * 
 * Requirements: 13, 64
 */

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

// Test concurrent decoder instances
bool test_concurrent_decoder_instances() {
    Debug::log("test_native_flac_threading", "[test_concurrent_decoder_instances] Testing concurrent decoder instances");
    
    try {
        const int num_threads = 4;
        const int frames_per_thread = 50;
        
        std::atomic<int> successful_threads{0};
        std::atomic<int> failed_threads{0};
        std::vector<std::thread> threads;
        
        // Create test data
        auto test_data = createTestFLACData(44100, 2, 16, frames_per_thread);
        
        // Launch threads, each with its own decoder instance
        for (int t = 0; t < num_threads; t++) {
            threads.emplace_back([t, &test_data, &successful_threads, &failed_threads]() {
                try {
                    // Create codec instance for this thread
                    StreamInfo stream_info;
                    stream_info.codec_name = "flac";
                    stream_info.sample_rate = 44100;
                    stream_info.channels = 2;
                    stream_info.bits_per_sample = 16;
                    
                    auto codec = CodecRegistry::createCodec(stream_info);
                    if (!codec) {
                        Debug::log("test_native_flac_threading", "[test_concurrent_decoder_instances] Thread ", t, " failed to create codec");
                        failed_threads++;
                        return;
                    }
                    
                    // Decode frames
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
                        
                        chunk.data.clear();
                    }
                    
                    Debug::log("test_native_flac_threading", "[test_concurrent_decoder_instances] Thread ", t, " decoded ", frame_count, " frames");
                    successful_threads++;
                    
                } catch (const std::exception& e) {
                    Debug::log("test_native_flac_threading", "[test_concurrent_decoder_instances] Thread ", t, " exception: ", e.what());
                    failed_threads++;
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        Debug::log("test_native_flac_threading", "[test_concurrent_decoder_instances] Successful threads: ", successful_threads.load());
        Debug::log("test_native_flac_threading", "[test_concurrent_decoder_instances] Failed threads: ", failed_threads.load());
        
        if (failed_threads.load() > 0) {
            Debug::log("test_native_flac_threading", "[test_concurrent_decoder_instances] ERROR: Some threads failed");
            return false;
        }
        
        if (successful_threads.load() != num_threads) {
            Debug::log("test_native_flac_threading", "[test_concurrent_decoder_instances] ERROR: Not all threads succeeded");
            return false;
        }
        
        Debug::log("test_native_flac_threading", "[test_concurrent_decoder_instances] SUCCESS: All threads completed successfully");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test_native_flac_threading", "[test_concurrent_decoder_instances] ERROR: Exception: ", e.what());
        return false;
    }
}

// Test lock contention measurement
bool test_lock_contention() {
    Debug::log("test_native_flac_threading", "[test_lock_contention] Testing lock contention");
    
    try {
        const int num_threads = 8;
        const int operations_per_thread = 100;
        
        // Create a shared decoder instance to test lock contention
        StreamInfo stream_info;
        stream_info.codec_name = "flac";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        stream_info.bits_per_sample = 16;
        
        auto codec = CodecRegistry::createCodec(stream_info);
        if (!codec) {
            Debug::log("test_native_flac_threading", "[test_lock_contention] ERROR: Failed to create codec");
            return false;
        }
        
        // Get raw pointer for thread sharing (codec lifetime managed by this scope)
        AudioCodec* codec_ptr = codec.get();
        
        // Create test data
        auto test_data = createTestFLACData(44100, 2, 16, 10);
        
        std::atomic<uint64_t> total_wait_time_us{0};
        std::atomic<int> total_operations{0};
        std::vector<std::thread> threads;
        
        // Launch threads that will contend for the decoder
        for (int t = 0; t < num_threads; t++) {
            threads.emplace_back([t, codec_ptr, &test_data, &total_wait_time_us, &total_operations, operations_per_thread]() {
                try {
                    for (int op = 0; op < operations_per_thread; op++) {
                        auto start = std::chrono::high_resolution_clock::now();
                        
                        // Perform operation that requires lock
                        MediaChunk chunk;
                        chunk.data = test_data;
                        chunk.timestamp_samples = 0;
                        
                        AudioFrame frame = codec_ptr->decode(chunk);
                        
                        auto end = std::chrono::high_resolution_clock::now();
                        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                        
                        total_wait_time_us += duration.count();
                        total_operations++;
                    }
                    
                } catch (const std::exception& e) {
                    Debug::log("test_native_flac_threading", "[test_lock_contention] Thread ", t, " exception: ", e.what());
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        uint64_t avg_wait_time_us = total_wait_time_us.load() / total_operations.load();
        
        Debug::log("test_native_flac_threading", "[test_lock_contention] Total operations: ", total_operations.load());
        Debug::log("test_native_flac_threading", "[test_lock_contention] Total wait time: ", total_wait_time_us.load(), " μs");
        Debug::log("test_native_flac_threading", "[test_lock_contention] Average operation time: ", avg_wait_time_us, " μs");
        
        // Check if lock contention is reasonable
        if (avg_wait_time_us > 10000) { // 10ms seems excessive for a decode operation
            Debug::log("test_native_flac_threading", "[test_lock_contention] WARNING: High lock contention detected");
        }
        
        Debug::log("test_native_flac_threading", "[test_lock_contention] SUCCESS: Lock contention measured");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test_native_flac_threading", "[test_lock_contention] ERROR: Exception: ", e.what());
        return false;
    }
}

// Test thread safety with stress test
bool test_thread_safety_stress() {
    Debug::log("test_native_flac_threading", "[test_thread_safety_stress] Testing thread safety under stress");
    
    try {
        const int num_threads = 16;
        const int iterations = 50;
        
        std::atomic<int> errors{0};
        std::vector<std::thread> threads;
        
        // Create test data
        auto test_data = createTestFLACData(44100, 2, 16, 20);
        
        // Launch many threads doing various operations
        for (int t = 0; t < num_threads; t++) {
            threads.emplace_back([t, &test_data, &errors, iterations]() {
                try {
                    for (int i = 0; i < iterations; i++) {
                        // Create codec
                        StreamInfo stream_info;
                        stream_info.codec_name = "flac";
                        stream_info.sample_rate = 44100;
                        stream_info.channels = 2;
                        stream_info.bits_per_sample = 16;
                        
                        auto codec = CodecRegistry::createCodec(stream_info);
                        if (!codec) {
                            errors++;
                            continue;
                        }
                        
                        // Decode some frames
                        MediaChunk chunk;
                        chunk.data = test_data;
                        chunk.timestamp_samples = 0;
                        
                        for (int f = 0; f < 5 && !chunk.data.empty(); f++) {
                            AudioFrame frame = codec->decode(chunk);
                            if (frame.samples.empty()) {
                                break;
                            }
                            
                            chunk.data.clear();
                        }
                        
                        // Reset
                        codec->reset();
                    }
                    
                } catch (const std::exception& e) {
                    Debug::log("test_native_flac_threading", "[test_thread_safety_stress] Thread ", t, " exception: ", e.what());
                    errors++;
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        Debug::log("test_native_flac_threading", "[test_thread_safety_stress] Total errors: ", errors.load());
        
        if (errors.load() > 0) {
            Debug::log("test_native_flac_threading", "[test_thread_safety_stress] ERROR: Thread safety violations detected");
            return false;
        }
        
        Debug::log("test_native_flac_threading", "[test_thread_safety_stress] SUCCESS: Thread safety validated under stress");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test_native_flac_threading", "[test_thread_safety_stress] ERROR: Exception: ", e.what());
        return false;
    }
}

// Test independent decoder state
bool test_independent_decoder_state() {
    Debug::log("test_native_flac_threading", "[test_independent_decoder_state] Testing independent decoder state");
    
    try {
        const int num_threads = 4;
        
        std::atomic<int> state_errors{0};
        std::vector<std::thread> threads;
        
        // Create different test data for each thread
        std::vector<std::vector<uint8_t>> test_data_sets;
        test_data_sets.push_back(createTestFLACData(44100, 2, 16, 10));
        test_data_sets.push_back(createTestFLACData(48000, 2, 16, 10));
        test_data_sets.push_back(createTestFLACData(96000, 2, 24, 10));
        test_data_sets.push_back(createTestFLACData(192000, 2, 24, 10));
        
        // Launch threads with different configurations
        for (int t = 0; t < num_threads; t++) {
            threads.emplace_back([t, &test_data_sets, &state_errors]() {
                try {
                    // Each thread uses different sample rate/bit depth
                    uint32_t sample_rates[] = {44100, 48000, 96000, 192000};
                    uint32_t bit_depths[] = {16, 16, 24, 24};
                    
                    StreamInfo stream_info;
                    stream_info.codec_name = "flac";
                    stream_info.sample_rate = sample_rates[t];
                    stream_info.channels = 2;
                    stream_info.bits_per_sample = bit_depths[t];
                    
                    auto codec = CodecRegistry::createCodec(stream_info);
                    if (!codec) {
                        Debug::log("test_native_flac_threading", "[test_independent_decoder_state] Thread ", t, " failed to create codec");
                        state_errors++;
                        return;
                    }
                    
                    // Decode frames
                    MediaChunk chunk;
                    chunk.data = test_data_sets[t];
                    chunk.timestamp_samples = 0;
                    
                    int frame_count = 0;
                    while (!chunk.data.empty() && frame_count < 5) {
                        AudioFrame frame = codec->decode(chunk);
                        if (frame.samples.empty()) {
                            break;
                        }
                        
                        // Verify frame has correct sample rate
                        if (frame.sample_rate != sample_rates[t]) {
                            Debug::log("test_native_flac_threading", "[test_independent_decoder_state] Thread ", t, 
                                     " sample rate mismatch: expected ", sample_rates[t], " got ", frame.sample_rate);
                            state_errors++;
                        }
                        
                        frame_count++;
                        
                        chunk.data.clear();
                    }
                    
                    Debug::log("test_native_flac_threading", "[test_independent_decoder_state] Thread ", t, 
                             " decoded ", frame_count, " frames at ", sample_rates[t], "Hz");
                    
                } catch (const std::exception& e) {
                    Debug::log("test_native_flac_threading", "[test_independent_decoder_state] Thread ", t, " exception: ", e.what());
                    state_errors++;
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        Debug::log("test_native_flac_threading", "[test_independent_decoder_state] State errors: ", state_errors.load());
        
        if (state_errors.load() > 0) {
            Debug::log("test_native_flac_threading", "[test_independent_decoder_state] ERROR: Decoder state not independent");
            return false;
        }
        
        Debug::log("test_native_flac_threading", "[test_independent_decoder_state] SUCCESS: Decoder state is independent");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test_native_flac_threading", "[test_independent_decoder_state] ERROR: Exception: ", e.what());
        return false;
    }
}

int main() {
    Debug::log("test_native_flac_threading", "=== Native FLAC Threading Tests ===");
    
    int passed = 0;
    int failed = 0;
    
    // Test concurrent decoder instances
    if (test_concurrent_decoder_instances()) {
        passed++;
    } else {
        failed++;
    }
    
    // Test lock contention
    if (test_lock_contention()) {
        passed++;
    } else {
        failed++;
    }
    
    // Test thread safety stress
    if (test_thread_safety_stress()) {
        passed++;
    } else {
        failed++;
    }
    
    // Test independent decoder state
    if (test_independent_decoder_state()) {
        passed++;
    } else {
        failed++;
    }
    
    Debug::log("test_native_flac_threading", "=== Test Results ===");
    Debug::log("test_native_flac_threading", "Passed: ", passed);
    Debug::log("test_native_flac_threading", "Failed: ", failed);
    
    return (failed == 0) ? 0 : 1;
}

#else

int main() {
    std::cerr << "Native FLAC decoder not available (HAVE_NATIVE_FLAC not defined)" << std::endl;
    return 77; // Skip test
}

#endif // HAVE_NATIVE_FLAC
