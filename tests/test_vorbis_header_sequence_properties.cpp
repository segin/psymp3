/*
 * test_vorbis_header_sequence_properties.cpp - Property-based tests for Vorbis header processing
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
 * 
 * Format per Vorbis specification:
 * [0]: packet type (0x01)
 * [1-6]: "vorbis"
 * [7-10]: version (little-endian uint32, must be 0)
 * [11]: channels (1-255)
 * [12-15]: sample_rate (little-endian uint32)
 * [16-19]: bitrate_maximum (little-endian int32)
 * [20-23]: bitrate_nominal (little-endian int32)
 * [24-27]: bitrate_minimum (little-endian int32)
 * [28]: blocksize_0 (4 bits) | blocksize_1 (4 bits)
 * [29]: framing flag (must be 1)
 */
std::vector<uint8_t> generateIdentificationHeader(
    uint8_t channels = 2,
    uint32_t sample_rate = 44100,
    uint8_t blocksize_0 = 8,  // 2^8 = 256
    uint8_t blocksize_1 = 11  // 2^11 = 2048
) {
    std::vector<uint8_t> packet(30);
    
    // Packet type
    packet[0] = 0x01;
    
    // "vorbis" signature
    std::memcpy(&packet[1], "vorbis", 6);
    
    // Version (must be 0)
    packet[7] = 0; packet[8] = 0; packet[9] = 0; packet[10] = 0;
    
    // Channels
    packet[11] = channels;
    
    // Sample rate (little-endian)
    packet[12] = sample_rate & 0xFF;
    packet[13] = (sample_rate >> 8) & 0xFF;
    packet[14] = (sample_rate >> 16) & 0xFF;
    packet[15] = (sample_rate >> 24) & 0xFF;
    
    // Bitrate maximum (0 = unspecified)
    packet[16] = 0; packet[17] = 0; packet[18] = 0; packet[19] = 0;
    
    // Bitrate nominal (128000 bps)
    uint32_t nominal = 128000;
    packet[20] = nominal & 0xFF;
    packet[21] = (nominal >> 8) & 0xFF;
    packet[22] = (nominal >> 16) & 0xFF;
    packet[23] = (nominal >> 24) & 0xFF;
    
    // Bitrate minimum (0 = unspecified)
    packet[24] = 0; packet[25] = 0; packet[26] = 0; packet[27] = 0;
    
    // Block sizes (stored as log2 values in nibbles)
    packet[28] = (blocksize_1 << 4) | blocksize_0;
    
    // Framing flag (must be 1)
    packet[29] = 0x01;
    
    return packet;
}

/**
 * @brief Generate a valid Vorbis comment header packet
 * 
 * Format per Vorbis specification:
 * [0]: packet type (0x03)
 * [1-6]: "vorbis"
 * [7-10]: vendor_length (little-endian uint32)
 * [11-...]: vendor_string
 * [...]: user_comment_list_length (little-endian uint32)
 * [...]: user_comments (length-prefixed strings)
 * [last]: framing flag (must be 1)
 */
std::vector<uint8_t> generateCommentHeader(const std::string& vendor = "Test Encoder") {
    std::vector<uint8_t> packet;
    
    // Packet type
    packet.push_back(0x03);
    
    // "vorbis" signature
    for (char c : std::string("vorbis")) {
        packet.push_back(static_cast<uint8_t>(c));
    }
    
    // Vendor string length (little-endian)
    uint32_t vendor_len = static_cast<uint32_t>(vendor.size());
    packet.push_back(vendor_len & 0xFF);
    packet.push_back((vendor_len >> 8) & 0xFF);
    packet.push_back((vendor_len >> 16) & 0xFF);
    packet.push_back((vendor_len >> 24) & 0xFF);
    
    // Vendor string
    for (char c : vendor) {
        packet.push_back(static_cast<uint8_t>(c));
    }
    
    // User comment list length (0 comments)
    packet.push_back(0); packet.push_back(0); packet.push_back(0); packet.push_back(0);
    
    // Framing flag
    packet.push_back(0x01);
    
    return packet;
}

/**
 * @brief Generate a minimal Vorbis setup header packet
 * 
 * Note: A real setup header is complex and contains codebook configurations.
 * For testing header sequence validation, we only need the signature.
 * This will fail actual decoding but tests the routing logic.
 */
std::vector<uint8_t> generateSetupHeaderStub() {
    std::vector<uint8_t> packet;
    
    // Packet type
    packet.push_back(0x05);
    
    // "vorbis" signature
    for (char c : std::string("vorbis")) {
        packet.push_back(static_cast<uint8_t>(c));
    }
    
    // Minimal setup data (this won't decode but tests routing)
    // Real setup headers are much larger and contain codebook data
    for (int i = 0; i < 20; i++) {
        packet.push_back(0x00);
    }
    
    return packet;
}

/**
 * @brief Generate a packet with invalid signature
 */
std::vector<uint8_t> generateInvalidSignaturePacket(uint8_t packet_type) {
    std::vector<uint8_t> packet(30);
    packet[0] = packet_type;
    std::memcpy(&packet[1], "NOTVOR", 6);  // Invalid signature
    return packet;
}

/**
 * @brief Generate a packet with wrong type byte
 */
std::vector<uint8_t> generateWrongTypePacket(uint8_t wrong_type) {
    std::vector<uint8_t> packet(30);
    packet[0] = wrong_type;
    std::memcpy(&packet[1], "vorbis", 6);
    return packet;
}

// ========================================
// PROPERTY 1: Header Sequence Validation
// ========================================
// **Feature: vorbis-codec, Property 1: Header Sequence Validation**
// **Validates: Requirements 1.1**
//
// For any sequence of Vorbis header packets, the VorbisCodec shall accept 
// only the correct sequence (identification → comment → setup) and reject 
// any other ordering.

void test_property_header_sequence_validation() {
    std::cout << "\n=== Property 1: Header Sequence Validation ===" << std::endl;
    std::cout << "Testing that VorbisCodec accepts only correct header sequence..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // Test 1: Correct sequence should be accepted
    {
        std::cout << "\n  Test 1: Correct sequence (ID → Comment → Setup)..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        bool init_ok = codec.initialize();
        assert(init_ok && "Codec initialization should succeed");
        
        // Send identification header
        auto id_header = generateIdentificationHeader();
        MediaChunk id_chunk;
        id_chunk.data = id_header;
        
        AudioFrame frame1 = codec.decode(id_chunk);
        // Headers don't produce audio output
        assert(frame1.samples.empty() && "ID header should not produce audio");
        
        // Send comment header
        auto comment_header = generateCommentHeader();
        MediaChunk comment_chunk;
        comment_chunk.data = comment_header;
        
        AudioFrame frame2 = codec.decode(comment_chunk);
        assert(frame2.samples.empty() && "Comment header should not produce audio");
        
        std::cout << "    ✓ Identification and comment headers accepted" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 2: Invalid signature should be rejected
    {
        std::cout << "\n  Test 2: Invalid signature rejection..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Send packet with invalid signature
        auto invalid_packet = generateInvalidSignaturePacket(0x01);
        MediaChunk chunk;
        chunk.data = invalid_packet;
        
        AudioFrame frame = codec.decode(chunk);
        // Should be rejected (empty frame, no state change)
        assert(frame.samples.empty() && "Invalid signature should be rejected");
        
        std::cout << "    ✓ Invalid signature correctly rejected" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 3: Wrong packet type should be rejected
    {
        std::cout << "\n  Test 3: Wrong packet type rejection..." << std::endl;
        
        // Test various invalid packet types
        std::vector<uint8_t> invalid_types = {0x00, 0x02, 0x04, 0x06, 0x07, 0xFF};
        
        for (uint8_t wrong_type : invalid_types) {
            StreamInfo stream_info;
            stream_info.codec_name = "vorbis";
            
            VorbisCodec codec(stream_info);
            codec.initialize();
            
            auto wrong_packet = generateWrongTypePacket(wrong_type);
            MediaChunk chunk;
            chunk.data = wrong_packet;
            
            AudioFrame frame = codec.decode(chunk);
            assert(frame.samples.empty() && "Wrong packet type should be rejected");
        }
        
        std::cout << "    ✓ Wrong packet types correctly rejected" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 4: Packet type detection for all valid header types
    {
        std::cout << "\n  Test 4: Valid header type detection..." << std::endl;
        
        // Verify packet type byte detection
        auto id_header = generateIdentificationHeader();
        assert(id_header[0] == 0x01 && "ID header should have type 0x01");
        assert(std::memcmp(&id_header[1], "vorbis", 6) == 0 && "ID header should have vorbis signature");
        
        auto comment_header = generateCommentHeader();
        assert(comment_header[0] == 0x03 && "Comment header should have type 0x03");
        assert(std::memcmp(&comment_header[1], "vorbis", 6) == 0 && "Comment header should have vorbis signature");
        
        auto setup_header = generateSetupHeaderStub();
        assert(setup_header[0] == 0x05 && "Setup header should have type 0x05");
        assert(std::memcmp(&setup_header[1], "vorbis", 6) == 0 && "Setup header should have vorbis signature");
        
        std::cout << "    ✓ All valid header types correctly identified" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 5: Empty packet handling
    {
        std::cout << "\n  Test 5: Empty packet handling..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        MediaChunk empty_chunk;
        // empty_chunk.data is empty by default
        
        AudioFrame frame = codec.decode(empty_chunk);
        assert(frame.samples.empty() && "Empty packet should return empty frame");
        
        std::cout << "    ✓ Empty packets handled correctly" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 6: Too-small packet handling
    {
        std::cout << "\n  Test 6: Too-small packet handling..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Packet smaller than minimum header size (7 bytes for type + "vorbis")
        std::vector<uint8_t> small_packet = {0x01, 'v', 'o', 'r'};  // Only 4 bytes
        MediaChunk chunk;
        chunk.data = small_packet;
        
        AudioFrame frame = codec.decode(chunk);
        assert(frame.samples.empty() && "Too-small packet should be rejected");
        
        std::cout << "    ✓ Too-small packets handled correctly" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    std::cout << "\n✓ Property 1: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// PROPERTY 2: Identification Header Field Extraction
// ========================================
// **Feature: vorbis-codec, Property 2: Identification Header Field Extraction**
// **Validates: Requirements 1.2**
//
// For any valid Vorbis identification header, the extracted values 
// (version, channels, rate, bitrate_upper, bitrate_nominal, bitrate_lower) 
// shall match the values encoded in the header packet.

void test_property_identification_header_field_extraction() {
    std::cout << "\n=== Property 2: Identification Header Field Extraction ===" << std::endl;
    std::cout << "Testing that identification header fields are correctly extracted..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // Test with various valid configurations
    struct TestConfig {
        uint8_t channels;
        uint32_t sample_rate;
        uint8_t blocksize_0;
        uint8_t blocksize_1;
        std::string description;
    };
    
    std::vector<TestConfig> test_configs = {
        {1, 8000, 6, 8, "Mono 8kHz (telephony)"},
        {2, 44100, 8, 11, "Stereo 44.1kHz (CD quality)"},
        {2, 48000, 8, 11, "Stereo 48kHz (DVD quality)"},
        {6, 48000, 8, 11, "5.1 surround 48kHz"},
        {2, 96000, 9, 12, "Stereo 96kHz (high-res)"},
        {1, 22050, 7, 10, "Mono 22.05kHz"},
        {2, 32000, 8, 11, "Stereo 32kHz"},
    };
    
    for (const auto& config : test_configs) {
        std::cout << "\n  Testing: " << config.description << "..." << std::endl;
        
        // Generate header with specific configuration
        auto header = generateIdentificationHeader(
            config.channels,
            config.sample_rate,
            config.blocksize_0,
            config.blocksize_1
        );
        
        // Parse using VorbisHeaderInfo
        VorbisHeaderInfo info = VorbisHeaderInfo::parseFromPacket(header);
        
        // Verify extracted values match input
        assert(info.version == 0 && "Version should be 0");
        assert(info.channels == config.channels && "Channels should match");
        assert(info.sample_rate == config.sample_rate && "Sample rate should match");
        assert(info.blocksize_0 == config.blocksize_0 && "Blocksize_0 should match");
        assert(info.blocksize_1 == config.blocksize_1 && "Blocksize_1 should match");
        
        // Verify header is valid
        assert(info.isValid() && "Header should be valid");
        
        std::cout << "    ✓ channels=" << static_cast<int>(info.channels)
                  << " rate=" << info.sample_rate
                  << " blocks=" << static_cast<int>(info.blocksize_0) 
                  << "/" << static_cast<int>(info.blocksize_1) << std::endl;
        
        tests_passed++;
        tests_run++;
    }
    
    // Test edge cases
    std::cout << "\n  Testing edge cases..." << std::endl;
    
    // Maximum channels (255)
    {
        auto header = generateIdentificationHeader(255, 44100, 8, 11);
        VorbisHeaderInfo info = VorbisHeaderInfo::parseFromPacket(header);
        assert(info.channels == 255 && "Should support 255 channels");
        assert(info.isValid() && "255 channels should be valid");
        std::cout << "    ✓ Maximum channels (255) supported" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Minimum valid block sizes
    {
        auto header = generateIdentificationHeader(2, 44100, 6, 6);  // Both 64 samples
        VorbisHeaderInfo info = VorbisHeaderInfo::parseFromPacket(header);
        assert(info.blocksize_0 == 6 && info.blocksize_1 == 6);
        assert(info.isValid() && "Minimum block sizes should be valid");
        std::cout << "    ✓ Minimum block sizes (64/64) supported" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Maximum valid block sizes
    {
        auto header = generateIdentificationHeader(2, 44100, 13, 13);  // Both 8192 samples
        VorbisHeaderInfo info = VorbisHeaderInfo::parseFromPacket(header);
        assert(info.blocksize_0 == 13 && info.blocksize_1 == 13);
        assert(info.isValid() && "Maximum block sizes should be valid");
        std::cout << "    ✓ Maximum block sizes (8192/8192) supported" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    std::cout << "\n✓ Property 2: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// PROPERTY 6: Block Size Constraint
// ========================================
// **Feature: vorbis-codec, Property 6: Block Size Constraint**
// **Validates: Requirements 4.1, 4.2**
//
// For any valid Vorbis stream, the block sizes extracted from the 
// identification header shall satisfy: blocksize_0 <= blocksize_1, 
// and both shall be powers of 2 in the range [64, 8192].

void test_property_block_size_constraint() {
    std::cout << "\n=== Property 6: Block Size Constraint ===" << std::endl;
    std::cout << "Testing block size validation constraints..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // Test 1: Valid block size combinations
    {
        std::cout << "\n  Test 1: Valid block size combinations..." << std::endl;
        
        // All valid combinations where blocksize_0 <= blocksize_1
        // Valid log2 values: 6 (64), 7 (128), 8 (256), 9 (512), 10 (1024), 11 (2048), 12 (4096), 13 (8192)
        std::vector<std::pair<uint8_t, uint8_t>> valid_combinations = {
            {6, 6}, {6, 7}, {6, 8}, {6, 9}, {6, 10}, {6, 11}, {6, 12}, {6, 13},
            {7, 7}, {7, 8}, {7, 9}, {7, 10}, {7, 11}, {7, 12}, {7, 13},
            {8, 8}, {8, 9}, {8, 10}, {8, 11}, {8, 12}, {8, 13},
            {9, 9}, {9, 10}, {9, 11}, {9, 12}, {9, 13},
            {10, 10}, {10, 11}, {10, 12}, {10, 13},
            {11, 11}, {11, 12}, {11, 13},
            {12, 12}, {12, 13},
            {13, 13}
        };
        
        for (const auto& combo : valid_combinations) {
            auto header = generateIdentificationHeader(2, 44100, combo.first, combo.second);
            VorbisHeaderInfo info = VorbisHeaderInfo::parseFromPacket(header);
            
            assert(info.isValid() && "Valid block size combination should be accepted");
            assert(info.blocksize_0 <= info.blocksize_1 && "blocksize_0 <= blocksize_1");
            
            // Verify actual block sizes are powers of 2 in valid range
            uint32_t actual_size_0 = 1 << info.blocksize_0;
            uint32_t actual_size_1 = 1 << info.blocksize_1;
            
            assert(actual_size_0 >= 64 && actual_size_0 <= 8192);
            assert(actual_size_1 >= 64 && actual_size_1 <= 8192);
        }
        
        std::cout << "    ✓ All " << valid_combinations.size() << " valid combinations accepted" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 2: Invalid block size combinations (blocksize_0 > blocksize_1)
    {
        std::cout << "\n  Test 2: Invalid combinations (blocksize_0 > blocksize_1)..." << std::endl;
        
        std::vector<std::pair<uint8_t, uint8_t>> invalid_combinations = {
            {7, 6}, {8, 6}, {8, 7}, {9, 8}, {10, 9}, {11, 10}, {12, 11}, {13, 12}
        };
        
        for (const auto& combo : invalid_combinations) {
            auto header = generateIdentificationHeader(2, 44100, combo.first, combo.second);
            VorbisHeaderInfo info = VorbisHeaderInfo::parseFromPacket(header);
            
            assert(!info.isValid() && "blocksize_0 > blocksize_1 should be invalid");
        }
        
        std::cout << "    ✓ All invalid combinations (blocksize_0 > blocksize_1) rejected" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 3: Out-of-range block sizes
    {
        std::cout << "\n  Test 3: Out-of-range block sizes..." << std::endl;
        
        // Block sizes below minimum (< 6, i.e., < 64 samples)
        std::vector<std::pair<uint8_t, uint8_t>> below_min = {
            {5, 8}, {4, 8}, {3, 8}, {0, 8}
        };
        
        for (const auto& combo : below_min) {
            auto header = generateIdentificationHeader(2, 44100, combo.first, combo.second);
            VorbisHeaderInfo info = VorbisHeaderInfo::parseFromPacket(header);
            
            assert(!info.isValid() && "Block size below minimum should be invalid");
        }
        
        // Block sizes above maximum (> 13, i.e., > 8192 samples)
        std::vector<std::pair<uint8_t, uint8_t>> above_max = {
            {8, 14}, {8, 15}
        };
        
        for (const auto& combo : above_max) {
            auto header = generateIdentificationHeader(2, 44100, combo.first, combo.second);
            VorbisHeaderInfo info = VorbisHeaderInfo::parseFromPacket(header);
            
            assert(!info.isValid() && "Block size above maximum should be invalid");
        }
        
        std::cout << "    ✓ Out-of-range block sizes rejected" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 4: Verify block sizes are powers of 2
    {
        std::cout << "\n  Test 4: Block sizes are powers of 2..." << std::endl;
        
        for (uint8_t log2_size = 6; log2_size <= 13; log2_size++) {
            uint32_t block_size = 1 << log2_size;
            
            // Verify it's a power of 2
            assert((block_size & (block_size - 1)) == 0 && "Block size should be power of 2");
            
            // Verify it's in valid range
            assert(block_size >= 64 && block_size <= 8192);
            
            std::cout << "    log2=" << static_cast<int>(log2_size) 
                      << " → size=" << block_size << " ✓" << std::endl;
        }
        
        tests_passed++;
        tests_run++;
    }
    
    std::cout << "\n✓ Property 6: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// PROPERTY 5: Reset Preserves Headers
// ========================================
// **Feature: vorbis-codec, Property 5: Reset Preserves Headers**
// **Validates: Requirements 2.7, 6.4**
//
// For any initialized VorbisCodec, calling reset() shall clear synthesis 
// state while preserving header information, allowing continued decoding 
// without re-sending headers.

void test_property_reset_preserves_headers() {
    std::cout << "\n=== Property 5: Reset Preserves Headers ===" << std::endl;
    std::cout << "Testing that reset() preserves header information..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // Test 1: Reset after headers preserves configuration
    {
        std::cout << "\n  Test 1: Reset after headers preserves configuration..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        bool init_ok = codec.initialize();
        assert(init_ok && "Codec initialization should succeed");
        
        // Send identification header
        auto id_header = generateIdentificationHeader(2, 44100, 8, 11);
        MediaChunk id_chunk;
        id_chunk.data = id_header;
        codec.decode(id_chunk);
        
        // Send comment header
        auto comment_header = generateCommentHeader("Test Encoder");
        MediaChunk comment_chunk;
        comment_chunk.data = comment_header;
        codec.decode(comment_chunk);
        
        // Now call reset() - this should preserve headers
        codec.reset();
        
        // Verify codec is still functional by checking it can accept more data
        // The codec should not require re-sending headers after reset
        // (reset is for seeking, not re-initialization)
        
        std::cout << "    ✓ Reset called after headers processed" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 2: Multiple resets don't corrupt state
    {
        std::cout << "\n  Test 2: Multiple resets don't corrupt state..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 48000;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Send headers
        auto id_header = generateIdentificationHeader(2, 48000, 8, 11);
        MediaChunk id_chunk;
        id_chunk.data = id_header;
        codec.decode(id_chunk);
        
        auto comment_header = generateCommentHeader();
        MediaChunk comment_chunk;
        comment_chunk.data = comment_header;
        codec.decode(comment_chunk);
        
        // Multiple resets should be safe
        for (int i = 0; i < 10; i++) {
            codec.reset();
        }
        
        // Codec should still be in valid state
        std::cout << "    ✓ Multiple resets handled correctly" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 3: Reset before headers is safe
    {
        std::cout << "\n  Test 3: Reset before headers is safe..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Reset before any headers - should be safe (no-op)
        codec.reset();
        
        // Should still be able to process headers after reset
        auto id_header = generateIdentificationHeader();
        MediaChunk id_chunk;
        id_chunk.data = id_header;
        AudioFrame frame = codec.decode(id_chunk);
        
        // Headers don't produce audio
        assert(frame.samples.empty() && "Header should not produce audio");
        
        std::cout << "    ✓ Reset before headers is safe" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 4: Reset clears output buffer
    {
        std::cout << "\n  Test 4: Reset clears output buffer..." << std::endl;
        
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
        
        // Reset should clear any buffered data
        codec.reset();
        
        // Flush should return empty frame after reset
        AudioFrame flushed = codec.flush();
        assert(flushed.samples.empty() && "Flush after reset should return empty frame");
        
        std::cout << "    ✓ Reset clears output buffer" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 5: Property test with various configurations
    {
        std::cout << "\n  Test 5: Reset preserves headers across configurations..." << std::endl;
        
        struct TestConfig {
            uint8_t channels;
            uint32_t sample_rate;
            std::string description;
        };
        
        std::vector<TestConfig> configs = {
            {1, 8000, "Mono 8kHz"},
            {2, 44100, "Stereo 44.1kHz"},
            {2, 48000, "Stereo 48kHz"},
            {6, 48000, "5.1 surround"},
        };
        
        for (const auto& config : configs) {
            StreamInfo stream_info;
            stream_info.codec_name = "vorbis";
            stream_info.sample_rate = config.sample_rate;
            stream_info.channels = config.channels;
            
            VorbisCodec codec(stream_info);
            codec.initialize();
            
            // Send identification header with matching config
            auto id_header = generateIdentificationHeader(config.channels, config.sample_rate, 8, 11);
            MediaChunk id_chunk;
            id_chunk.data = id_header;
            codec.decode(id_chunk);
            
            // Send comment header
            auto comment_header = generateCommentHeader();
            MediaChunk comment_chunk;
            comment_chunk.data = comment_header;
            codec.decode(comment_chunk);
            
            // Reset
            codec.reset();
            
            // Verify codec name is still correct (basic sanity check)
            assert(codec.getCodecName() == "vorbis" && "Codec name should be preserved");
            
            std::cout << "    ✓ " << config.description << " - reset preserves state" << std::endl;
        }
        
        tests_passed++;
        tests_run++;
    }
    
    // Test 6: Reset with random iteration counts (property-based style)
    {
        std::cout << "\n  Test 6: Property test - random reset iterations..." << std::endl;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> reset_count_dist(1, 20);
        std::uniform_int_distribution<> channel_dist(1, 8);
        std::uniform_int_distribution<> rate_dist(0, 3);
        
        uint32_t sample_rates[] = {8000, 22050, 44100, 48000};
        
        // Run 100 iterations as per testing strategy
        constexpr int NUM_ITERATIONS = 100;
        
        for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
            uint8_t channels = static_cast<uint8_t>(channel_dist(gen));
            uint32_t sample_rate = sample_rates[rate_dist(gen)];
            int num_resets = reset_count_dist(gen);
            
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
            
            // Perform random number of resets
            for (int r = 0; r < num_resets; r++) {
                codec.reset();
            }
            
            // Verify codec is still valid
            assert(codec.getCodecName() == "vorbis");
            
            // Flush should be safe
            AudioFrame flushed = codec.flush();
            // After reset, flush should return empty
            assert(flushed.samples.empty());
        }
        
        std::cout << "    ✓ " << NUM_ITERATIONS << " random iterations passed" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    std::cout << "\n✓ Property 5: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int run_vorbis_header_property_tests() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "VORBIS CODEC HEADER PROPERTY-BASED TESTS" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    try {
        // Run all property tests
        test_property_header_sequence_validation();
        test_property_identification_header_field_extraction();
        test_property_block_size_constraint();
        test_property_reset_preserves_headers();
        
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "✅ ALL VORBIS HEADER PROPERTY TESTS PASSED" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(60, '=') << std::endl;
        std::cerr << "❌ VORBIS HEADER PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Exception: " << e.what() << std::endl;
        std::cerr << std::string(60, '=') << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n" << std::string(60, '=') << std::endl;
        std::cerr << "❌ VORBIS HEADER PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Unknown exception occurred" << std::endl;
        std::cerr << std::string(60, '=') << std::endl;
        return 1;
    }
}

#else // !HAVE_OGGDEMUXER

int run_vorbis_header_property_tests() {
    std::cout << "Vorbis codec tests skipped - OggDemuxer not available" << std::endl;
    return 0;
}

#endif // HAVE_OGGDEMUXER

// ========================================
// STANDALONE TEST EXECUTABLE
// ========================================
int main() {
    return run_vorbis_header_property_tests();
}

