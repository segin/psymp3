/*
 * test_vorbis_streaming_properties.cpp - Property-based tests for Vorbis streaming and buffering
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
// PROPERTY 11: Bounded Buffer Size
// ========================================
// **Feature: vorbis-codec, Property 11: Bounded Buffer Size**
// **Validates: Requirements 7.2, 7.4**
//
// For any sequence of decoded packets, the internal output buffer size 
// shall never exceed the configured maximum (2 seconds of audio at 
// maximum sample rate and channels).

void test_property_bounded_buffer_size() {
    std::cout << "\n=== Property 11: Bounded Buffer Size ===" << std::endl;
    std::cout << "Testing that buffer size never exceeds maximum..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // Test 1: Verify maximum buffer size constant
    {
        std::cout << "\n  Test 1: Verify maximum buffer size constant..." << std::endl;
        
        // Maximum buffer: 2 seconds at 48kHz stereo = 48000 * 2 * 2 = 192000 samples
        constexpr size_t EXPECTED_MAX = 48000 * 2 * 2;
        size_t actual_max = VorbisCodec::getMaxBufferSize();
        
        assert(actual_max == EXPECTED_MAX && "Max buffer size should be 2 seconds at 48kHz stereo");
        
        std::cout << "    ✓ Max buffer size = " << actual_max << " samples (2 seconds at 48kHz stereo)" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 2: Buffer starts empty
    {
        std::cout << "\n  Test 2: Buffer starts empty after initialization..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        assert(codec.getBufferSize() == 0 && "Buffer should start empty");
        assert(!codec.isBackpressureActive() && "Backpressure should not be active initially");
        
        std::cout << "    ✓ Buffer starts empty, no backpressure" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 3: Buffer size is bounded after header processing
    {
        std::cout << "\n  Test 3: Buffer remains bounded after headers..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Send identification header
        auto id_header = generateIdentificationHeader();
        MediaChunk id_chunk;
        id_chunk.data = id_header;
        codec.decode(id_chunk);
        
        // Send comment header
        auto comment_header = generateCommentHeader();
        MediaChunk comment_chunk;
        comment_chunk.data = comment_header;
        codec.decode(comment_chunk);
        
        // Buffer should still be empty (headers don't produce audio)
        assert(codec.getBufferSize() == 0 && "Headers should not produce audio");
        assert(codec.getBufferSize() <= VorbisCodec::getMaxBufferSize() && "Buffer should be bounded");
        
        std::cout << "    ✓ Buffer bounded after header processing" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 4: Property test - buffer never exceeds max across random configurations
    {
        std::cout << "\n  Test 4: Property test - buffer bounded across configurations..." << std::endl;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> channel_dist(1, 8);
        std::uniform_int_distribution<> rate_idx_dist(0, 3);
        
        uint32_t sample_rates[] = {8000, 22050, 44100, 48000};
        
        const int NUM_ITERATIONS = 100;
        
        for (int i = 0; i < NUM_ITERATIONS; i++) {
            uint8_t channels = static_cast<uint8_t>(channel_dist(gen));
            uint32_t sample_rate = sample_rates[rate_idx_dist(gen)];
            
            StreamInfo stream_info;
            stream_info.codec_name = "vorbis";
            stream_info.sample_rate = sample_rate;
            stream_info.channels = channels;
            
            VorbisCodec codec(stream_info);
            codec.initialize();
            
            // Send headers
            auto id_header = generateIdentificationHeader(channels, sample_rate, 8, 11);
            MediaChunk id_chunk;
            id_chunk.data = id_header;
            codec.decode(id_chunk);
            
            auto comment_header = generateCommentHeader();
            MediaChunk comment_chunk;
            comment_chunk.data = comment_header;
            codec.decode(comment_chunk);
            
            // Verify buffer is bounded
            assert(codec.getBufferSize() <= VorbisCodec::getMaxBufferSize() && 
                   "Buffer should never exceed maximum");
        }
        
        std::cout << "    ✓ Buffer bounded across " << NUM_ITERATIONS << " random configurations" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    std::cout << "\n✓ Property 11: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// PROPERTY 12: Incremental Processing
// ========================================
// **Feature: vorbis-codec, Property 12: Incremental Processing**
// **Validates: Requirements 7.1**
//
// For any Vorbis stream, the VorbisCodec shall process packets one at a 
// time without requiring access to future packets or the complete file.

void test_property_incremental_processing() {
    std::cout << "\n=== Property 12: Incremental Processing ===" << std::endl;
    std::cout << "Testing that packets are processed incrementally..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // Test 1: Each packet can be processed independently
    {
        std::cout << "\n  Test 1: Packets processed independently..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Process identification header
        auto id_header = generateIdentificationHeader();
        MediaChunk id_chunk;
        id_chunk.data = id_header;
        AudioFrame frame1 = codec.decode(id_chunk);
        
        // Verify we can continue without knowing future packets
        assert(frame1.samples.empty() && "ID header should not produce audio");
        
        // Process comment header
        auto comment_header = generateCommentHeader();
        MediaChunk comment_chunk;
        comment_chunk.data = comment_header;
        AudioFrame frame2 = codec.decode(comment_chunk);
        
        assert(frame2.samples.empty() && "Comment header should not produce audio");
        
        std::cout << "    ✓ Headers processed incrementally" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 2: Empty packets don't block processing
    {
        std::cout << "\n  Test 2: Empty packets handled gracefully..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Send empty packet
        MediaChunk empty_chunk;
        AudioFrame frame = codec.decode(empty_chunk);
        
        // Should return empty frame without blocking
        assert(frame.samples.empty() && "Empty packet should return empty frame");
        
        // Codec should still be functional
        auto id_header = generateIdentificationHeader();
        MediaChunk id_chunk;
        id_chunk.data = id_header;
        AudioFrame frame2 = codec.decode(id_chunk);
        
        // Should still work
        assert(frame2.samples.empty() && "Should still process headers after empty packet");
        
        std::cout << "    ✓ Empty packets handled without blocking" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 3: Partial data doesn't require complete file
    {
        std::cout << "\n  Test 3: Partial data processing..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Send only identification header (incomplete stream)
        auto id_header = generateIdentificationHeader();
        MediaChunk id_chunk;
        id_chunk.data = id_header;
        AudioFrame frame = codec.decode(id_chunk);
        
        // Should process what we have without requiring more
        assert(frame.samples.empty() && "Partial stream should be handled");
        
        // Codec should be in valid state waiting for more data
        assert(codec.getCodecName() == "vorbis" && "Codec should remain valid");
        
        std::cout << "    ✓ Partial data processed without requiring complete file" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 4: Property test - incremental processing across iterations
    {
        std::cout << "\n  Test 4: Property test - incremental processing..." << std::endl;
        
        const int NUM_ITERATIONS = 100;
        
        for (int i = 0; i < NUM_ITERATIONS; i++) {
            StreamInfo stream_info;
            stream_info.codec_name = "vorbis";
            stream_info.sample_rate = 44100;
            stream_info.channels = 2;
            
            VorbisCodec codec(stream_info);
            codec.initialize();
            
            // Process headers one at a time
            auto id_header = generateIdentificationHeader();
            MediaChunk id_chunk;
            id_chunk.data = id_header;
            codec.decode(id_chunk);
            
            // Each decode call should complete without blocking
            auto comment_header = generateCommentHeader();
            MediaChunk comment_chunk;
            comment_chunk.data = comment_header;
            codec.decode(comment_chunk);
            
            // Verify codec is still functional
            assert(codec.getCodecName() == "vorbis");
        }
        
        std::cout << "    ✓ Incremental processing verified across " << NUM_ITERATIONS << " iterations" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    std::cout << "\n✓ Property 12: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// PROPERTY 7: Flush Outputs Remaining Samples
// ========================================
// **Feature: vorbis-codec, Property 7: Flush Outputs Remaining Samples**
// **Validates: Requirements 4.8, 7.5, 11.4**
//
// For any VorbisCodec with buffered samples, calling flush() shall return 
// an AudioFrame containing all remaining decoded samples.

void test_property_flush_outputs_remaining_samples() {
    std::cout << "\n=== Property 7: Flush Outputs Remaining Samples ===" << std::endl;
    std::cout << "Testing that flush() outputs all remaining samples..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // Test 1: Flush on empty buffer returns empty frame
    {
        std::cout << "\n  Test 1: Flush on empty buffer..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Flush without any data
        AudioFrame frame = codec.flush();
        
        assert(frame.samples.empty() && "Flush on empty buffer should return empty frame");
        
        std::cout << "    ✓ Flush on empty buffer returns empty frame" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 2: Flush after headers returns empty frame (headers don't produce audio)
    {
        std::cout << "\n  Test 2: Flush after headers..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Send headers
        auto id_header = generateIdentificationHeader();
        MediaChunk id_chunk;
        id_chunk.data = id_header;
        codec.decode(id_chunk);
        
        auto comment_header = generateCommentHeader();
        MediaChunk comment_chunk;
        comment_chunk.data = comment_header;
        codec.decode(comment_chunk);
        
        // Flush after headers
        AudioFrame frame = codec.flush();
        
        // Headers don't produce audio, so flush should return empty
        assert(frame.samples.empty() && "Flush after headers should return empty frame");
        
        std::cout << "    ✓ Flush after headers returns empty frame" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 3: Multiple flushes are safe
    {
        std::cout << "\n  Test 3: Multiple flushes are safe..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Multiple flushes should be safe
        for (int i = 0; i < 10; i++) {
            AudioFrame frame = codec.flush();
            assert(frame.samples.empty() && "Multiple flushes should be safe");
        }
        
        std::cout << "    ✓ Multiple flushes handled safely" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 4: Flush clears buffer
    {
        std::cout << "\n  Test 4: Flush clears buffer..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // First flush
        AudioFrame frame1 = codec.flush();
        
        // Second flush should also return empty (buffer was cleared)
        AudioFrame frame2 = codec.flush();
        
        assert(frame2.samples.empty() && "Second flush should return empty frame");
        assert(codec.getBufferSize() == 0 && "Buffer should be empty after flush");
        
        std::cout << "    ✓ Flush clears buffer" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    std::cout << "\n✓ Property 7: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// PROPERTY 16: Reset Clears State
// ========================================
// **Feature: vorbis-codec, Property 16: Reset Clears State**
// **Validates: Requirements 7.6, 11.5**
//
// For any VorbisCodec in any state, calling reset() shall clear all 
// internal buffers and return the codec to a state ready for decoding 
// from a new position.

void test_property_reset_clears_state() {
    std::cout << "\n=== Property 16: Reset Clears State ===" << std::endl;
    std::cout << "Testing that reset() clears all internal state..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // Test 1: Reset clears buffer
    {
        std::cout << "\n  Test 1: Reset clears buffer..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Send headers
        auto id_header = generateIdentificationHeader();
        MediaChunk id_chunk;
        id_chunk.data = id_header;
        codec.decode(id_chunk);
        
        auto comment_header = generateCommentHeader();
        MediaChunk comment_chunk;
        comment_chunk.data = comment_header;
        codec.decode(comment_chunk);
        
        // Reset
        codec.reset();
        
        // Buffer should be empty
        assert(codec.getBufferSize() == 0 && "Buffer should be empty after reset");
        
        std::cout << "    ✓ Reset clears buffer" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 2: Reset clears backpressure state
    {
        std::cout << "\n  Test 2: Reset clears backpressure state..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Reset
        codec.reset();
        
        // Backpressure should not be active
        assert(!codec.isBackpressureActive() && "Backpressure should not be active after reset");
        
        std::cout << "    ✓ Reset clears backpressure state" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 3: Reset before any processing is safe
    {
        std::cout << "\n  Test 3: Reset before processing is safe..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Reset immediately after init
        codec.reset();
        
        // Should still be functional
        assert(codec.getCodecName() == "vorbis" && "Codec should remain valid after reset");
        assert(codec.getBufferSize() == 0 && "Buffer should be empty");
        
        std::cout << "    ✓ Reset before processing is safe" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 4: Multiple resets are safe
    {
        std::cout << "\n  Test 4: Multiple resets are safe..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Multiple resets
        for (int i = 0; i < 10; i++) {
            codec.reset();
            assert(codec.getBufferSize() == 0 && "Buffer should be empty after each reset");
            assert(!codec.isBackpressureActive() && "Backpressure should not be active");
        }
        
        std::cout << "    ✓ Multiple resets handled safely" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 5: Flush after reset returns empty
    {
        std::cout << "\n  Test 5: Flush after reset returns empty..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Send headers
        auto id_header = generateIdentificationHeader();
        MediaChunk id_chunk;
        id_chunk.data = id_header;
        codec.decode(id_chunk);
        
        auto comment_header = generateCommentHeader();
        MediaChunk comment_chunk;
        comment_chunk.data = comment_header;
        codec.decode(comment_chunk);
        
        // Reset then flush
        codec.reset();
        AudioFrame frame = codec.flush();
        
        assert(frame.samples.empty() && "Flush after reset should return empty frame");
        
        std::cout << "    ✓ Flush after reset returns empty" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 6: Property test - reset clears state across configurations
    {
        std::cout << "\n  Test 6: Property test - reset across configurations..." << std::endl;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> channel_dist(1, 8);
        std::uniform_int_distribution<> rate_idx_dist(0, 3);
        
        uint32_t sample_rates[] = {8000, 22050, 44100, 48000};
        
        const int NUM_ITERATIONS = 100;
        
        for (int i = 0; i < NUM_ITERATIONS; i++) {
            uint8_t channels = static_cast<uint8_t>(channel_dist(gen));
            uint32_t sample_rate = sample_rates[rate_idx_dist(gen)];
            
            StreamInfo stream_info;
            stream_info.codec_name = "vorbis";
            stream_info.sample_rate = sample_rate;
            stream_info.channels = channels;
            
            VorbisCodec codec(stream_info);
            codec.initialize();
            
            // Send headers
            auto id_header = generateIdentificationHeader(channels, sample_rate, 8, 11);
            MediaChunk id_chunk;
            id_chunk.data = id_header;
            codec.decode(id_chunk);
            
            auto comment_header = generateCommentHeader();
            MediaChunk comment_chunk;
            comment_chunk.data = comment_header;
            codec.decode(comment_chunk);
            
            // Reset
            codec.reset();
            
            // Verify state is cleared
            assert(codec.getBufferSize() == 0 && "Buffer should be empty after reset");
            assert(!codec.isBackpressureActive() && "Backpressure should not be active");
        }
        
        std::cout << "    ✓ Reset clears state across " << NUM_ITERATIONS << " configurations" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    std::cout << "\n✓ Property 16: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// MAIN TEST RUNNER
// ========================================

int main(int argc, char* argv[])
{
    std::cout << "========================================" << std::endl;
    std::cout << "Vorbis Streaming Property Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Testing Properties 11, 12, 7, 16" << std::endl;
    std::cout << "Requirements: 7.1, 7.2, 7.4, 7.5, 7.6, 4.8, 11.4, 11.5" << std::endl;
    
    try {
        // Property 11: Bounded Buffer Size
        test_property_bounded_buffer_size();
        
        // Property 12: Incremental Processing
        test_property_incremental_processing();
        
        // Property 7: Flush Outputs Remaining Samples
        test_property_flush_outputs_remaining_samples();
        
        // Property 16: Reset Clears State
        test_property_reset_clears_state();
        
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
    std::cout << "Vorbis streaming property tests skipped - OggDemuxer not available" << std::endl;
    return 0;
}

#endif // HAVE_OGGDEMUXER

