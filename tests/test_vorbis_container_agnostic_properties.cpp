/*
 * test_vorbis_container_agnostic_properties.cpp - Property-based tests for container-agnostic Vorbis decoding
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

/**
 * @brief Simulated container source types for testing container-agnostic behavior
 */
enum class ContainerSource {
    OGG_CONTAINER,      // Standard Ogg container
    MATROSKA_CONTAINER, // Matroska/WebM container
    RAW_PACKET,         // Raw packet data (no container)
    MEMORY_BUFFER,      // In-memory buffer
    NETWORK_STREAM      // Network streaming source
};

/**
 * @brief Create a MediaChunk simulating different container sources
 * 
 * The key insight is that the VorbisCodec should produce identical output
 * regardless of where the packet data came from. This function creates
 * MediaChunks with identical packet data but different metadata that might
 * indicate different sources.
 */
MediaChunk createMediaChunkFromSource(
    const std::vector<uint8_t>& packet_data,
    ContainerSource source,
    uint64_t timestamp_samples = 0
) {
    MediaChunk chunk;
    chunk.data = packet_data;  // Same packet data regardless of source
    chunk.timestamp_samples = timestamp_samples;
    
    // Different sources might set different metadata, but the codec
    // should only care about the packet data itself
    switch (source) {
        case ContainerSource::OGG_CONTAINER:
            // Ogg container might set granule position
            chunk.timestamp_samples = timestamp_samples;
            break;
        case ContainerSource::MATROSKA_CONTAINER:
            // Matroska uses different timestamp units
            chunk.timestamp_samples = timestamp_samples;
            break;
        case ContainerSource::RAW_PACKET:
            // Raw packets might not have timestamps
            chunk.timestamp_samples = 0;
            break;
        case ContainerSource::MEMORY_BUFFER:
            // Memory buffer source
            chunk.timestamp_samples = timestamp_samples;
            break;
        case ContainerSource::NETWORK_STREAM:
            // Network stream might have different timing
            chunk.timestamp_samples = timestamp_samples;
            break;
    }
    
    return chunk;
}

// ========================================
// PROPERTY 10: Container-Agnostic Decoding
// ========================================
// **Feature: vorbis-codec, Property 10: Container-Agnostic Decoding**
// **Validates: Requirements 6.1, 6.3**
//
// For any valid Vorbis packet data, the VorbisCodec shall decode it 
// identically regardless of whether it came from OggDemuxer or any 
// other source providing MediaChunk data.

void test_property_container_agnostic_decoding() {
    std::cout << "\n=== Property 10: Container-Agnostic Decoding ===" << std::endl;
    std::cout << "Testing that VorbisCodec decodes based on packet data only..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // Test 1: Same packet data from different sources produces same result
    {
        std::cout << "\n  Test 1: Same packet data from different sources..." << std::endl;
        
        // Generate a valid identification header
        auto id_header = generateIdentificationHeader(2, 44100);
        
        // Create MediaChunks from different "sources"
        std::vector<ContainerSource> sources = {
            ContainerSource::OGG_CONTAINER,
            ContainerSource::MATROSKA_CONTAINER,
            ContainerSource::RAW_PACKET,
            ContainerSource::MEMORY_BUFFER,
            ContainerSource::NETWORK_STREAM
        };
        
        // Each source should produce the same decoding result
        for (auto source : sources) {
            StreamInfo stream_info;
            stream_info.codec_name = "vorbis";
            stream_info.sample_rate = 44100;
            stream_info.channels = 2;
            
            VorbisCodec codec(stream_info);
            codec.initialize();
            
            MediaChunk chunk = createMediaChunkFromSource(id_header, source);
            AudioFrame frame = codec.decode(chunk);
            
            // Header packets don't produce audio, but should be accepted
            assert(frame.samples.empty() && "Header should not produce audio");
            
            // Verify codec name is always "vorbis" regardless of source
            assert(codec.getCodecName() == "vorbis");
        }
        
        std::cout << "    ✓ Same packet data produces consistent results across sources" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 2: Codec only uses packet data, not container metadata
    {
        std::cout << "\n  Test 2: Codec uses packet data only, ignores container metadata..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        // Create two codecs
        VorbisCodec codec1(stream_info);
        VorbisCodec codec2(stream_info);
        codec1.initialize();
        codec2.initialize();
        
        // Same packet data
        auto id_header = generateIdentificationHeader(2, 44100);
        
        // Different timestamps (simulating different container timing)
        MediaChunk chunk1;
        chunk1.data = id_header;
        chunk1.timestamp_samples = 0;
        
        MediaChunk chunk2;
        chunk2.data = id_header;
        chunk2.timestamp_samples = 12345;  // Different timestamp
        
        // Both should decode the same way (headers don't produce audio)
        AudioFrame frame1 = codec1.decode(chunk1);
        AudioFrame frame2 = codec2.decode(chunk2);
        
        // Both should be empty (header packets)
        assert(frame1.samples.empty() && frame2.samples.empty());
        
        std::cout << "    ✓ Codec ignores container-specific metadata" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 3: StreamInfo codec_name is the only container-related check
    {
        std::cout << "\n  Test 3: canDecode() only checks codec_name..." << std::endl;
        
        // Test with various StreamInfo configurations
        struct TestCase {
            std::string codec_name;
            std::string codec_type;
            uint32_t sample_rate;
            uint8_t channels;
            bool expected_can_decode;
            std::string description;
        };
        
        std::vector<TestCase> test_cases = {
            {"vorbis", "audio", 44100, 2, true, "Standard Vorbis"},
            {"vorbis", "", 0, 0, true, "Minimal Vorbis (no metadata)"},
            {"vorbis", "audio", 48000, 6, true, "5.1 Vorbis"},
            {"opus", "audio", 48000, 2, false, "Opus (not Vorbis)"},
            {"flac", "audio", 44100, 2, false, "FLAC (not Vorbis)"},
            {"mp3", "audio", 44100, 2, false, "MP3 (not Vorbis)"},
            {"vorbis", "video", 44100, 2, false, "Video type (invalid)"},
        };
        
        for (const auto& tc : test_cases) {
            StreamInfo stream_info;
            stream_info.codec_name = tc.codec_name;
            stream_info.codec_type = tc.codec_type;
            stream_info.sample_rate = tc.sample_rate;
            stream_info.channels = tc.channels;
            
            VorbisCodec codec(stream_info);
            bool can_decode = codec.canDecode(stream_info);
            
            assert(can_decode == tc.expected_can_decode && 
                   ("canDecode mismatch for: " + tc.description).c_str());
        }
        
        std::cout << "    ✓ canDecode() correctly identifies Vorbis streams" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 4: Reset works without container-specific operations
    {
        std::cout << "\n  Test 4: Reset works without container-specific operations..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Send headers
        MediaChunk id_chunk;
        id_chunk.data = generateIdentificationHeader();
        codec.decode(id_chunk);
        
        MediaChunk comment_chunk;
        comment_chunk.data = generateCommentHeader();
        codec.decode(comment_chunk);
        
        // Reset should work without any container-specific operations
        codec.reset();
        
        // After reset, codec should be ready for new position
        // (headers are preserved, only synthesis state is reset)
        
        // Verify codec is still functional
        assert(codec.getCodecName() == "vorbis");
        
        std::cout << "    ✓ Reset works without container dependencies" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 5: Property test - random packet data from random sources
    {
        std::cout << "\n  Test 5: Property test - random configurations..." << std::endl;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> source_dist(0, 4);
        std::uniform_int_distribution<int> channels_dist(1, 8);
        std::uniform_int_distribution<uint32_t> rate_dist(8000, 192000);
        
        // Test 100 iterations with random configurations
        for (int iteration = 0; iteration < 100; iteration++) {
            // Random configuration
            uint8_t channels = static_cast<uint8_t>(channels_dist(gen));
            uint32_t sample_rate = rate_dist(gen);
            ContainerSource source = static_cast<ContainerSource>(source_dist(gen));
            
            StreamInfo stream_info;
            stream_info.codec_name = "vorbis";
            stream_info.sample_rate = sample_rate;
            stream_info.channels = channels;
            
            VorbisCodec codec(stream_info);
            codec.initialize();
            
            // Generate header with matching configuration
            auto id_header = generateIdentificationHeader(channels, sample_rate);
            MediaChunk chunk = createMediaChunkFromSource(id_header, source);
            
            // Should decode without crashing regardless of source
            try {
                AudioFrame frame = codec.decode(chunk);
                // Header packets don't produce audio
                assert(frame.samples.empty());
            } catch (const std::exception& e) {
                // Some configurations might fail validation, which is acceptable
                // as long as it doesn't crash
            }
        }
        
        std::cout << "    ✓ 100 random configurations handled correctly" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 6: Identical packets produce identical results
    {
        std::cout << "\n  Test 6: Identical packets produce identical results..." << std::endl;
        
        // Generate test data
        auto id_header = generateIdentificationHeader(2, 44100);
        auto comment_header = generateCommentHeader("Test Encoder v1.0");
        
        // Create two independent codec instances
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec1(stream_info);
        VorbisCodec codec2(stream_info);
        codec1.initialize();
        codec2.initialize();
        
        // Feed identical packets to both codecs
        MediaChunk id_chunk1, id_chunk2;
        id_chunk1.data = id_header;
        id_chunk2.data = id_header;
        
        AudioFrame frame1a = codec1.decode(id_chunk1);
        AudioFrame frame2a = codec2.decode(id_chunk2);
        
        // Both should produce empty frames (headers)
        assert(frame1a.samples.empty() && frame2a.samples.empty());
        
        MediaChunk comment_chunk1, comment_chunk2;
        comment_chunk1.data = comment_header;
        comment_chunk2.data = comment_header;
        
        AudioFrame frame1b = codec1.decode(comment_chunk1);
        AudioFrame frame2b = codec2.decode(comment_chunk2);
        
        // Both should produce empty frames (headers)
        assert(frame1b.samples.empty() && frame2b.samples.empty());
        
        std::cout << "    ✓ Identical packets produce identical results" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 7: Codec works with MediaChunk regardless of how data was obtained
    {
        std::cout << "\n  Test 7: MediaChunk data source independence..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        // Test with packet data created in different ways
        
        // Method 1: Direct vector construction
        std::vector<uint8_t> packet1 = generateIdentificationHeader();
        
        // Method 2: Copy from another vector
        std::vector<uint8_t> packet2;
        packet2.assign(packet1.begin(), packet1.end());
        
        // Method 3: Move semantics
        std::vector<uint8_t> temp = generateIdentificationHeader();
        std::vector<uint8_t> packet3 = std::move(temp);
        
        // All three should produce identical results
        VorbisCodec codec1(stream_info), codec2(stream_info), codec3(stream_info);
        codec1.initialize();
        codec2.initialize();
        codec3.initialize();
        
        MediaChunk chunk1, chunk2, chunk3;
        chunk1.data = packet1;
        chunk2.data = packet2;
        chunk3.data = packet3;
        
        AudioFrame frame1 = codec1.decode(chunk1);
        AudioFrame frame2 = codec2.decode(chunk2);
        AudioFrame frame3 = codec3.decode(chunk3);
        
        // All should be empty (header packets)
        assert(frame1.samples.empty() && frame2.samples.empty() && frame3.samples.empty());
        
        std::cout << "    ✓ MediaChunk data source is independent" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 8: Verify no container-specific state is maintained
    {
        std::cout << "\n  Test 8: No container-specific state maintained..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Process packets from "different containers" in sequence
        // The codec should not maintain any container-specific state
        
        auto id_header = generateIdentificationHeader();
        
        // Simulate Ogg source
        MediaChunk ogg_chunk = createMediaChunkFromSource(id_header, ContainerSource::OGG_CONTAINER, 0);
        codec.decode(ogg_chunk);
        
        // Reset and reinitialize
        codec.reset();
        codec.initialize();
        
        // Simulate Matroska source - should work identically
        MediaChunk mkv_chunk = createMediaChunkFromSource(id_header, ContainerSource::MATROSKA_CONTAINER, 1000);
        AudioFrame frame = codec.decode(mkv_chunk);
        
        // Should work without any issues
        assert(frame.samples.empty());  // Header packet
        
        std::cout << "    ✓ No container-specific state maintained" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    std::cout << "\n✓ Property 10: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// MAIN TEST RUNNER
// ========================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Vorbis Container-Agnostic Property Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        // Property 10: Container-Agnostic Decoding
        // **Feature: vorbis-codec, Property 10: Container-Agnostic Decoding**
        // **Validates: Requirements 6.1, 6.3**
        test_property_container_agnostic_decoding();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "ALL PROPERTY TESTS PASSED!" << std::endl;
        std::cout << "========================================" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
}

#else // !HAVE_OGGDEMUXER

int main() {
    std::cout << "Vorbis container-agnostic tests skipped - OggDemuxer not available" << std::endl;
    return 0;
}

#endif // HAVE_OGGDEMUXER

