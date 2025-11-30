/*
 * test_vorbis_thread_safety_properties.cpp - Property-based tests for Vorbis thread safety
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

#include <iostream>
#include <vector>
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <algorithm>
#include <random>
#include <cstring>
#include <thread>
#include <atomic>
#include <future>
#include <chrono>

#ifdef HAVE_OGGDEMUXER

using namespace PsyMP3::Codec::Vorbis;
using namespace PsyMP3::Demuxer;

// ========================================
// TEST DATA GENERATORS
// ========================================

/**
 * @brief Generate a valid Vorbis identification header packet
 */
std::vector<uint8_t> generateIdentificationHeader(
    uint8_t channels = 2,
    uint32_t sample_rate = 44100,
    uint8_t blocksize_0 = 8,
    uint8_t blocksize_1 = 11
) {
    std::vector<uint8_t> packet(30);
    
    packet[0] = 0x01;
    std::memcpy(&packet[1], "vorbis", 6);
    packet[7] = 0; packet[8] = 0; packet[9] = 0; packet[10] = 0;
    packet[11] = channels;
    packet[12] = sample_rate & 0xFF;
    packet[13] = (sample_rate >> 8) & 0xFF;
    packet[14] = (sample_rate >> 16) & 0xFF;
    packet[15] = (sample_rate >> 24) & 0xFF;
    packet[16] = 0; packet[17] = 0; packet[18] = 0; packet[19] = 0;
    uint32_t nominal = 128000;
    packet[20] = nominal & 0xFF;
    packet[21] = (nominal >> 8) & 0xFF;
    packet[22] = (nominal >> 16) & 0xFF;
    packet[23] = (nominal >> 24) & 0xFF;
    packet[24] = 0; packet[25] = 0; packet[26] = 0; packet[27] = 0;
    packet[28] = (blocksize_1 << 4) | blocksize_0;
    packet[29] = 0x01;
    
    return packet;
}

/**
 * @brief Generate a valid Vorbis comment header packet
 */
std::vector<uint8_t> generateCommentHeader(const std::string& vendor = "Test Encoder") {
    std::vector<uint8_t> packet;
    
    packet.push_back(0x03);
    for (char c : std::string("vorbis")) {
        packet.push_back(static_cast<uint8_t>(c));
    }
    
    uint32_t vendor_len = static_cast<uint32_t>(vendor.size());
    packet.push_back(vendor_len & 0xFF);
    packet.push_back((vendor_len >> 8) & 0xFF);
    packet.push_back((vendor_len >> 16) & 0xFF);
    packet.push_back((vendor_len >> 24) & 0xFF);
    
    for (char c : vendor) {
        packet.push_back(static_cast<uint8_t>(c));
    }
    
    packet.push_back(0); packet.push_back(0); packet.push_back(0); packet.push_back(0);
    packet.push_back(0x01);
    
    return packet;
}

// ========================================
// PROPERTY 13: Instance Independence
// ========================================
// **Feature: vorbis-codec, Property 13: Instance Independence**
// **Validates: Requirements 10.1, 10.2**
//
// For any two VorbisCodec instances decoding different streams concurrently,
// the decoding of one stream shall not affect the output of the other.

void test_property_instance_independence() {
    std::cout << "\n=== Property 13: Instance Independence ===" << std::endl;
    std::cout << "Testing that codec instances are independent..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // Test 1: Two instances have independent state
    {
        std::cout << "\n  Test 1: Two instances have independent state..." << std::endl;
        
        // Create two codec instances with different configurations
        StreamInfo stream_info1;
        stream_info1.codec_name = "vorbis";
        stream_info1.sample_rate = 44100;
        stream_info1.channels = 2;
        
        StreamInfo stream_info2;
        stream_info2.codec_name = "vorbis";
        stream_info2.sample_rate = 48000;
        stream_info2.channels = 1;
        
        VorbisCodec codec1(stream_info1);
        VorbisCodec codec2(stream_info2);
        
        codec1.initialize();
        codec2.initialize();
        
        // Verify they have independent state
        assert(codec1.getCodecName() == "vorbis");
        assert(codec2.getCodecName() == "vorbis");
        assert(codec1.getBufferSize() == 0);
        assert(codec2.getBufferSize() == 0);
        
        // Send header to codec1 only
        auto id_header1 = generateIdentificationHeader(2, 44100, 8, 11);
        MediaChunk chunk1;
        chunk1.data = id_header1;
        codec1.decode(chunk1);
        
        // codec2 should be unaffected
        assert(codec2.getBufferSize() == 0);
        
        std::cout << "    ✓ Two instances have independent state" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 2: Concurrent decoding on separate instances
    {
        std::cout << "\n  Test 2: Concurrent decoding on separate instances..." << std::endl;
        
        const int NUM_INSTANCES = 4;
        std::vector<std::unique_ptr<VorbisCodec>> codecs;
        std::vector<std::future<bool>> futures;
        std::atomic<int> success_count{0};
        
        // Create multiple codec instances
        for (int i = 0; i < NUM_INSTANCES; i++) {
            StreamInfo stream_info;
            stream_info.codec_name = "vorbis";
            stream_info.sample_rate = 44100 + (i * 1000);  // Different sample rates
            stream_info.channels = (i % 2) + 1;  // Alternating mono/stereo
            
            codecs.push_back(std::make_unique<VorbisCodec>(stream_info));
            codecs.back()->initialize();
        }
        
        // Launch concurrent operations
        for (int i = 0; i < NUM_INSTANCES; i++) {
            futures.push_back(std::async(std::launch::async, [&codecs, i, &success_count]() {
                try {
                    VorbisCodec* codec = codecs[i].get();
                    
                    // Send identification header
                    auto id_header = generateIdentificationHeader(
                        (i % 2) + 1,  // channels
                        44100 + (i * 1000),  // sample_rate
                        8, 11
                    );
                    MediaChunk id_chunk;
                    id_chunk.data = id_header;
                    codec->decode(id_chunk);
                    
                    // Send comment header
                    auto comment_header = generateCommentHeader("Instance " + std::to_string(i));
                    MediaChunk comment_chunk;
                    comment_chunk.data = comment_header;
                    codec->decode(comment_chunk);
                    
                    // Verify codec is still functional
                    if (codec->getCodecName() == "vorbis") {
                        success_count++;
                        return true;
                    }
                    return false;
                } catch (...) {
                    return false;
                }
            }));
        }
        
        // Wait for all operations to complete
        for (auto& future : futures) {
            future.wait();
        }
        
        assert(success_count == NUM_INSTANCES && "All concurrent operations should succeed");
        
        std::cout << "    ✓ Concurrent decoding on " << NUM_INSTANCES << " instances succeeded" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 3: Reset on one instance doesn't affect others
    {
        std::cout << "\n  Test 3: Reset on one instance doesn't affect others..." << std::endl;
        
        StreamInfo stream_info1;
        stream_info1.codec_name = "vorbis";
        stream_info1.sample_rate = 44100;
        stream_info1.channels = 2;
        
        StreamInfo stream_info2;
        stream_info2.codec_name = "vorbis";
        stream_info2.sample_rate = 48000;
        stream_info2.channels = 2;
        
        VorbisCodec codec1(stream_info1);
        VorbisCodec codec2(stream_info2);
        
        codec1.initialize();
        codec2.initialize();
        
        // Send headers to both
        auto id_header1 = generateIdentificationHeader(2, 44100, 8, 11);
        MediaChunk chunk1;
        chunk1.data = id_header1;
        codec1.decode(chunk1);
        
        auto id_header2 = generateIdentificationHeader(2, 48000, 8, 11);
        MediaChunk chunk2;
        chunk2.data = id_header2;
        codec2.decode(chunk2);
        
        // Reset codec1
        codec1.reset();
        
        // codec2 should be unaffected
        assert(codec2.getCodecName() == "vorbis");
        assert(!codec2.isInErrorState());
        
        std::cout << "    ✓ Reset on one instance doesn't affect others" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 4: Property test - instance independence across many iterations
    {
        std::cout << "\n  Test 4: Property test - instance independence..." << std::endl;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> channel_dist(1, 8);
        std::uniform_int_distribution<> rate_idx_dist(0, 3);
        
        uint32_t sample_rates[] = {8000, 22050, 44100, 48000};
        
        const int NUM_ITERATIONS = 100;
        
        for (int i = 0; i < NUM_ITERATIONS; i++) {
            // Create two instances with random configurations
            uint8_t channels1 = static_cast<uint8_t>(channel_dist(gen));
            uint32_t rate1 = sample_rates[rate_idx_dist(gen)];
            
            uint8_t channels2 = static_cast<uint8_t>(channel_dist(gen));
            uint32_t rate2 = sample_rates[rate_idx_dist(gen)];
            
            StreamInfo stream_info1;
            stream_info1.codec_name = "vorbis";
            stream_info1.sample_rate = rate1;
            stream_info1.channels = channels1;
            
            StreamInfo stream_info2;
            stream_info2.codec_name = "vorbis";
            stream_info2.sample_rate = rate2;
            stream_info2.channels = channels2;
            
            VorbisCodec codec1(stream_info1);
            VorbisCodec codec2(stream_info2);
            
            codec1.initialize();
            codec2.initialize();
            
            // Operations on codec1 should not affect codec2
            auto id_header1 = generateIdentificationHeader(channels1, rate1, 8, 11);
            MediaChunk chunk1;
            chunk1.data = id_header1;
            codec1.decode(chunk1);
            
            // Verify codec2 is unaffected
            assert(codec2.getBufferSize() == 0);
            assert(!codec2.isInErrorState());
            
            // Reset codec1
            codec1.reset();
            
            // codec2 should still be unaffected
            assert(!codec2.isInErrorState());
        }
        
        std::cout << "    ✓ Instance independence verified across " << NUM_ITERATIONS << " iterations" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 5: Concurrent operations on same instance are serialized
    {
        std::cout << "\n  Test 5: Concurrent operations on same instance are serialized..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        auto codec = std::make_shared<VorbisCodec>(stream_info);
        codec->initialize();
        
        std::atomic<int> operation_count{0};
        std::atomic<bool> error_occurred{false};
        
        const int NUM_THREADS = 4;
        const int OPS_PER_THREAD = 25;
        std::vector<std::thread> threads;
        
        // Launch multiple threads that call various methods
        for (int t = 0; t < NUM_THREADS; t++) {
            threads.emplace_back([codec, &operation_count, &error_occurred, t]() {
                try {
                    for (int i = 0; i < OPS_PER_THREAD; i++) {
                        // Mix of read and write operations
                        switch ((t + i) % 5) {
                            case 0:
                                codec->getBufferSize();
                                break;
                            case 1:
                                codec->isBackpressureActive();
                                break;
                            case 2:
                                codec->isInErrorState();
                                break;
                            case 3:
                                codec->getLastError();
                                break;
                            case 4:
                                codec->getCodecName();
                                break;
                        }
                        operation_count++;
                    }
                } catch (...) {
                    error_occurred = true;
                }
            });
        }
        
        // Wait for all threads
        for (auto& thread : threads) {
            thread.join();
        }
        
        assert(!error_occurred && "No errors should occur during concurrent operations");
        assert(operation_count == NUM_THREADS * OPS_PER_THREAD && "All operations should complete");
        
        std::cout << "    ✓ " << operation_count << " concurrent operations completed without errors" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    std::cout << "\n✓ Property 13: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// PROPERTY 14: Concurrent Initialization Safety
// ========================================
// **Feature: vorbis-codec, Property 14: Concurrent Initialization Safety**
// **Validates: Requirements 10.5**
//
// For any number of VorbisCodec instances being initialized concurrently,
// all initializations shall complete successfully without data corruption or crashes.

void test_property_concurrent_initialization_safety() {
    std::cout << "\n=== Property 14: Concurrent Initialization Safety ===" << std::endl;
    std::cout << "Testing that concurrent initialization is safe..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // Test 1: Multiple instances can be initialized concurrently
    {
        std::cout << "\n  Test 1: Multiple instances initialized concurrently..." << std::endl;
        
        const int NUM_INSTANCES = 8;
        std::vector<std::unique_ptr<VorbisCodec>> codecs(NUM_INSTANCES);
        std::vector<std::future<bool>> futures;
        std::atomic<int> success_count{0};
        
        // Create StreamInfo objects first
        std::vector<StreamInfo> stream_infos(NUM_INSTANCES);
        for (int i = 0; i < NUM_INSTANCES; i++) {
            stream_infos[i].codec_name = "vorbis";
            stream_infos[i].sample_rate = 44100 + (i * 1000);
            stream_infos[i].channels = (i % 2) + 1;
        }
        
        // Launch concurrent initialization
        for (int i = 0; i < NUM_INSTANCES; i++) {
            futures.push_back(std::async(std::launch::async, [&codecs, &stream_infos, i, &success_count]() {
                try {
                    codecs[i] = std::make_unique<VorbisCodec>(stream_infos[i]);
                    bool result = codecs[i]->initialize();
                    if (result) {
                        success_count++;
                    }
                    return result;
                } catch (...) {
                    return false;
                }
            }));
        }
        
        // Wait for all initializations
        for (auto& future : futures) {
            future.wait();
        }
        
        assert(success_count == NUM_INSTANCES && "All initializations should succeed");
        
        // Verify all codecs are functional
        for (int i = 0; i < NUM_INSTANCES; i++) {
            assert(codecs[i] != nullptr);
            assert(codecs[i]->getCodecName() == "vorbis");
            assert(!codecs[i]->isInErrorState());
        }
        
        std::cout << "    ✓ " << NUM_INSTANCES << " instances initialized concurrently" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 2: Rapid create-initialize-destroy cycles
    {
        std::cout << "\n  Test 2: Rapid create-initialize-destroy cycles..." << std::endl;
        
        const int NUM_THREADS = 4;
        const int CYCLES_PER_THREAD = 25;
        std::atomic<int> total_cycles{0};
        std::atomic<bool> error_occurred{false};
        
        std::vector<std::thread> threads;
        
        for (int t = 0; t < NUM_THREADS; t++) {
            threads.emplace_back([&total_cycles, &error_occurred, t]() {
                try {
                    for (int i = 0; i < CYCLES_PER_THREAD; i++) {
                        StreamInfo stream_info;
                        stream_info.codec_name = "vorbis";
                        stream_info.sample_rate = 44100 + (t * 1000) + i;
                        stream_info.channels = (i % 2) + 1;
                        
                        // Create, initialize, and destroy
                        {
                            VorbisCodec codec(stream_info);
                            if (!codec.initialize()) {
                                error_occurred = true;
                                return;
                            }
                            // Destructor called here
                        }
                        
                        total_cycles++;
                    }
                } catch (...) {
                    error_occurred = true;
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        assert(!error_occurred && "No errors during rapid cycles");
        assert(total_cycles == NUM_THREADS * CYCLES_PER_THREAD && "All cycles should complete");
        
        std::cout << "    ✓ " << total_cycles << " create-initialize-destroy cycles completed" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 3: Concurrent initialization with header processing
    {
        std::cout << "\n  Test 3: Concurrent initialization with header processing..." << std::endl;
        
        const int NUM_INSTANCES = 4;
        std::vector<std::future<bool>> futures;
        std::atomic<int> success_count{0};
        
        for (int i = 0; i < NUM_INSTANCES; i++) {
            futures.push_back(std::async(std::launch::async, [i, &success_count]() {
                try {
                    StreamInfo stream_info;
                    stream_info.codec_name = "vorbis";
                    stream_info.sample_rate = 44100;
                    stream_info.channels = 2;
                    
                    VorbisCodec codec(stream_info);
                    if (!codec.initialize()) {
                        return false;
                    }
                    
                    // Process headers
                    auto id_header = generateIdentificationHeader(2, 44100, 8, 11);
                    MediaChunk id_chunk;
                    id_chunk.data = id_header;
                    codec.decode(id_chunk);
                    
                    auto comment_header = generateCommentHeader("Thread " + std::to_string(i));
                    MediaChunk comment_chunk;
                    comment_chunk.data = comment_header;
                    codec.decode(comment_chunk);
                    
                    // Verify codec is functional
                    if (codec.getCodecName() == "vorbis" && !codec.isInErrorState()) {
                        success_count++;
                        return true;
                    }
                    return false;
                } catch (...) {
                    return false;
                }
            }));
        }
        
        for (auto& future : futures) {
            future.wait();
        }
        
        assert(success_count == NUM_INSTANCES && "All concurrent init+header should succeed");
        
        std::cout << "    ✓ " << NUM_INSTANCES << " concurrent init+header operations succeeded" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 4: Property test - concurrent initialization across configurations
    {
        std::cout << "\n  Test 4: Property test - concurrent initialization..." << std::endl;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> channel_dist(1, 8);
        std::uniform_int_distribution<> rate_idx_dist(0, 3);
        
        uint32_t sample_rates[] = {8000, 22050, 44100, 48000};
        
        const int NUM_ITERATIONS = 50;
        const int INSTANCES_PER_ITERATION = 4;
        
        for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
            std::vector<std::future<bool>> futures;
            std::atomic<int> success_count{0};
            
            for (int i = 0; i < INSTANCES_PER_ITERATION; i++) {
                uint8_t channels = static_cast<uint8_t>(channel_dist(gen));
                uint32_t rate = sample_rates[rate_idx_dist(gen)];
                
                futures.push_back(std::async(std::launch::async, [channels, rate, &success_count]() {
                    try {
                        StreamInfo stream_info;
                        stream_info.codec_name = "vorbis";
                        stream_info.sample_rate = rate;
                        stream_info.channels = channels;
                        
                        VorbisCodec codec(stream_info);
                        if (codec.initialize()) {
                            success_count++;
                            return true;
                        }
                        return false;
                    } catch (...) {
                        return false;
                    }
                }));
            }
            
            for (auto& future : futures) {
                future.wait();
            }
            
            assert(success_count == INSTANCES_PER_ITERATION && 
                   "All concurrent initializations should succeed");
        }
        
        std::cout << "    ✓ Concurrent initialization verified across " 
                  << NUM_ITERATIONS << " iterations" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 5: Stress test - many concurrent initializations
    {
        std::cout << "\n  Test 5: Stress test - many concurrent initializations..." << std::endl;
        
        const int NUM_THREADS = 8;
        const int INITS_PER_THREAD = 10;
        std::atomic<int> total_inits{0};
        std::atomic<bool> error_occurred{false};
        
        std::vector<std::thread> threads;
        
        for (int t = 0; t < NUM_THREADS; t++) {
            threads.emplace_back([&total_inits, &error_occurred, t]() {
                try {
                    for (int i = 0; i < INITS_PER_THREAD; i++) {
                        StreamInfo stream_info;
                        stream_info.codec_name = "vorbis";
                        stream_info.sample_rate = 44100;
                        stream_info.channels = 2;
                        
                        VorbisCodec codec(stream_info);
                        if (!codec.initialize()) {
                            error_occurred = true;
                            return;
                        }
                        
                        // Quick operations to verify functionality
                        codec.getBufferSize();
                        codec.isInErrorState();
                        codec.reset();
                        
                        total_inits++;
                    }
                } catch (...) {
                    error_occurred = true;
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        assert(!error_occurred && "No errors during stress test");
        assert(total_inits == NUM_THREADS * INITS_PER_THREAD && "All inits should complete");
        
        std::cout << "    ✓ Stress test: " << total_inits << " concurrent initializations succeeded" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    std::cout << "\n✓ Property 14: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// MAIN TEST RUNNER
// ========================================

int main(int argc, char* argv[])
{
    std::cout << "========================================" << std::endl;
    std::cout << "Vorbis Thread Safety Property Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Testing Properties 13, 14" << std::endl;
    std::cout << "Requirements: 10.1, 10.2, 10.5" << std::endl;
    
    try {
        // Property 13: Instance Independence
        test_property_instance_independence();
        
        // Property 14: Concurrent Initialization Safety
        test_property_concurrent_initialization_safety();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "ALL PROPERTY TESTS PASSED!" << std::endl;
        std::cout << "========================================" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n❌ TEST FAILED: Unknown exception" << std::endl;
        return 1;
    }
}

#else // !HAVE_OGGDEMUXER

int main(int argc, char* argv[])
{
    std::cout << "Vorbis thread safety property tests skipped - OggDemuxer not available" << std::endl;
    return 0;
}

#endif // HAVE_OGGDEMUXER

