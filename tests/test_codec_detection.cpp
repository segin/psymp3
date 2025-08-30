/*
 * test_codec_detection.cpp - Unit tests for OggDemuxer codec detection and header processing
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifdef HAVE_OGGDEMUXER

#include <fstream>
#include <vector>
#include <cstring>
#include <iostream>
#include <cassert>

// Simple assertion macro
#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "ASSERTION FAILED: " << (message) \
                      << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

// Test helper class to create mock codec headers
class MockCodecHeaders {
public:
    // Create Vorbis identification header (packet type 1)
    static std::vector<uint8_t> createVorbisIdHeader() {
        std::vector<uint8_t> data;
        
        // Packet type (1 = identification)
        data.push_back(0x01);
        
        // Vorbis signature
        data.insert(data.end(), {'v', 'o', 'r', 'b', 'i', 's'});
        
        // Version (4 bytes, little-endian) - version 0
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00});
        
        // Channels (1 byte) - stereo
        data.push_back(0x02);
        
        // Sample rate (4 bytes, little-endian) - 44100 Hz
        data.push_back(0x44);  // 44100 & 0xFF
        data.push_back(0xAC);  // (44100 >> 8) & 0xFF
        data.push_back(0x00);  // (44100 >> 16) & 0xFF
        data.push_back(0x00);  // (44100 >> 24) & 0xFF
        
        // Bitrate maximum (4 bytes, little-endian) - 320000 bps
        data.push_back(0x00);  // 320000 & 0xFF
        data.push_back(0xE1);  // (320000 >> 8) & 0xFF
        data.push_back(0x04);  // (320000 >> 16) & 0xFF
        data.push_back(0x00);  // (320000 >> 24) & 0xFF
        
        // Bitrate nominal (4 bytes, little-endian) - 192000 bps
        data.push_back(0x00);  // 192000 & 0xFF
        data.push_back(0xEE);  // (192000 >> 8) & 0xFF
        data.push_back(0x02);  // (192000 >> 16) & 0xFF
        data.push_back(0x00);  // (192000 >> 24) & 0xFF
        
        // Bitrate minimum (4 bytes, little-endian) - 128000 bps
        data.push_back(0x00);  // 128000 & 0xFF
        data.push_back(0xFA);  // (128000 >> 8) & 0xFF
        data.push_back(0x01);  // (128000 >> 16) & 0xFF
        data.push_back(0x00);  // (128000 >> 24) & 0xFF
        
        // Blocksize (1 byte) - 4 bits each for blocksize_0 and blocksize_1
        data.push_back(0xB8);  // blocksize_0=8, blocksize_1=11 (typical values)
        
        // Framing flag (1 byte) - must be 1
        data.push_back(0x01);
        
        return data;
    }
    
    // Create Vorbis comment header (packet type 3)
    static std::vector<uint8_t> createVorbisCommentHeader() {
        std::vector<uint8_t> data;
        
        // Packet type (3 = comment)
        data.push_back(0x03);
        
        // Vorbis signature
        data.insert(data.end(), {'v', 'o', 'r', 'b', 'i', 's'});
        
        // Vendor string length (4 bytes, little-endian)
        std::string vendor = "Test Encoder v1.0";
        uint32_t vendor_len = vendor.length();
        data.push_back(vendor_len & 0xFF);
        data.push_back((vendor_len >> 8) & 0xFF);
        data.push_back((vendor_len >> 16) & 0xFF);
        data.push_back((vendor_len >> 24) & 0xFF);
        
        // Vendor string
        data.insert(data.end(), vendor.begin(), vendor.end());
        
        // User comment list length (4 bytes, little-endian) - 3 comments
        data.insert(data.end(), {0x03, 0x00, 0x00, 0x00});
        
        // Comment 1: ARTIST=Test Artist
        std::string comment1 = "ARTIST=Test Artist";
        uint32_t comment1_len = comment1.length();
        data.push_back(comment1_len & 0xFF);
        data.push_back((comment1_len >> 8) & 0xFF);
        data.push_back((comment1_len >> 16) & 0xFF);
        data.push_back((comment1_len >> 24) & 0xFF);
        data.insert(data.end(), comment1.begin(), comment1.end());
        
        // Comment 2: TITLE=Test Title
        std::string comment2 = "TITLE=Test Title";
        uint32_t comment2_len = comment2.length();
        data.push_back(comment2_len & 0xFF);
        data.push_back((comment2_len >> 8) & 0xFF);
        data.push_back((comment2_len >> 16) & 0xFF);
        data.push_back((comment2_len >> 24) & 0xFF);
        data.insert(data.end(), comment2.begin(), comment2.end());
        
        // Comment 3: ALBUM=Test Album
        std::string comment3 = "ALBUM=Test Album";
        uint32_t comment3_len = comment3.length();
        data.push_back(comment3_len & 0xFF);
        data.push_back((comment3_len >> 8) & 0xFF);
        data.push_back((comment3_len >> 16) & 0xFF);
        data.push_back((comment3_len >> 24) & 0xFF);
        data.insert(data.end(), comment3.begin(), comment3.end());
        
        // Framing flag (1 byte) - must be 1
        data.push_back(0x01);
        
        return data;
    }
    
    // Create Vorbis setup header (packet type 5)
    static std::vector<uint8_t> createVorbisSetupHeader() {
        std::vector<uint8_t> data;
        
        // Packet type (5 = setup)
        data.push_back(0x05);
        
        // Vorbis signature
        data.insert(data.end(), {'v', 'o', 'r', 'b', 'i', 's'});
        
        // Minimal setup data (codebook count = 1, minimal codebook)
        data.insert(data.end(), {0x01, 0x00, 0x00, 0x00}); // 1 codebook
        data.insert(data.end(), {0x42, 0x43, 0x56}); // "BCV" sync pattern
        data.insert(data.end(), {0x01, 0x00, 0x00, 0x00}); // Codebook dimensions
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00}); // Codebook entries
        
        // Add some dummy setup data to make it realistic
        for (int i = 0; i < 50; i++) {
            data.push_back(0x00);
        }
        
        // Framing flag (1 byte) - must be 1
        data.push_back(0x01);
        
        return data;
    }
    
    // Create Opus identification header (OpusHead)
    static std::vector<uint8_t> createOpusIdHeader() {
        std::vector<uint8_t> data;
        
        // OpusHead signature
        data.insert(data.end(), {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd'});
        
        // Version (1 byte) - version 1
        data.push_back(0x01);
        
        // Channel count (1 byte) - stereo
        data.push_back(0x02);
        
        // Pre-skip (2 bytes, little-endian) - 312 samples
        data.push_back(0x38);  // 312 & 0xFF
        data.push_back(0x01);  // (312 >> 8) & 0xFF
        
        // Input sample rate (4 bytes, little-endian) - 48000 Hz
        data.push_back(0x80);  // 48000 & 0xFF
        data.push_back(0xBB);  // (48000 >> 8) & 0xFF
        data.push_back(0x00);  // (48000 >> 16) & 0xFF
        data.push_back(0x00);  // (48000 >> 24) & 0xFF
        
        // Output gain (2 bytes, little-endian) - 0 dB
        data.insert(data.end(), {0x00, 0x00});
        
        // Channel mapping family (1 byte) - 0 (RTP mapping)
        data.push_back(0x00);
        
        return data;
    }
    
    // Create Opus comment header (OpusTags)
    static std::vector<uint8_t> createOpusCommentHeader() {
        std::vector<uint8_t> data;
        
        // OpusTags signature
        data.insert(data.end(), {'O', 'p', 'u', 's', 'T', 'a', 'g', 's'});
        
        // Vendor string length (4 bytes, little-endian)
        std::string vendor = "Test Opus Encoder v1.0";
        uint32_t vendor_len = vendor.length();
        data.push_back(vendor_len & 0xFF);
        data.push_back((vendor_len >> 8) & 0xFF);
        data.push_back((vendor_len >> 16) & 0xFF);
        data.push_back((vendor_len >> 24) & 0xFF);
        
        // Vendor string
        data.insert(data.end(), vendor.begin(), vendor.end());
        
        // User comment list length (4 bytes, little-endian) - 2 comments
        data.insert(data.end(), {0x02, 0x00, 0x00, 0x00});
        
        // Comment 1: ARTIST=Opus Test Artist
        std::string comment1 = "ARTIST=Opus Test Artist";
        uint32_t comment1_len = comment1.length();
        data.push_back(comment1_len & 0xFF);
        data.push_back((comment1_len >> 8) & 0xFF);
        data.push_back((comment1_len >> 16) & 0xFF);
        data.push_back((comment1_len >> 24) & 0xFF);
        data.insert(data.end(), comment1.begin(), comment1.end());
        
        // Comment 2: TITLE=Opus Test Title
        std::string comment2 = "TITLE=Opus Test Title";
        uint32_t comment2_len = comment2.length();
        data.push_back(comment2_len & 0xFF);
        data.push_back((comment2_len >> 8) & 0xFF);
        data.push_back((comment2_len >> 16) & 0xFF);
        data.push_back((comment2_len >> 24) & 0xFF);
        data.insert(data.end(), comment2.begin(), comment2.end());
        
        return data;
    }
    
    // Create FLAC identification header (\x7fFLAC)
    static std::vector<uint8_t> createFLACIdHeader() {
        std::vector<uint8_t> data;
        
        // Ogg FLAC signature
        data.insert(data.end(), {0x7f, 'F', 'L', 'A', 'C'});
        
        // Version (1 byte) - version 1
        data.push_back(0x01);
        
        // Number of header packets (1 byte) - 1
        data.push_back(0x01);
        
        // Native FLAC signature
        data.insert(data.end(), {'f', 'L', 'a', 'C'});
        
        // STREAMINFO metadata block header (4 bytes)
        data.push_back(0x00);  // Last block flag (0) + block type (0 = STREAMINFO)
        data.insert(data.end(), {0x00, 0x00, 0x22}); // Block length (34 bytes)
        
        // STREAMINFO block data (34 bytes)
        // Minimum block size (2 bytes) - 4096
        data.insert(data.end(), {0x10, 0x00});
        
        // Maximum block size (2 bytes) - 4096
        data.insert(data.end(), {0x10, 0x00});
        
        // Minimum frame size (3 bytes) - 0 (unknown)
        data.insert(data.end(), {0x00, 0x00, 0x00});
        
        // Maximum frame size (3 bytes) - 0 (unknown)
        data.insert(data.end(), {0x00, 0x00, 0x00});
        
        // Sample rate (20 bits), channels (3 bits), bits per sample (5 bits) - 44100 Hz, stereo, 16-bit
        // 44100 = 0xAC44, channels-1 = 1, bits-1 = 15
        data.push_back(0xAC);  // Sample rate bits 19-12
        data.push_back(0x44);  // Sample rate bits 11-4
        data.push_back(0x2F);  // Sample rate bits 3-0 (4) + channels-1 (1) + bits-1 bits 4-1 (15>>1=7)
        data.push_back(0x80);  // bits-1 bit 0 (15&1=1) + reserved (0) + total samples bits 35-29 (0)
        
        // Total samples (36 bits) - 1000000 samples
        uint64_t total_samples = 1000000;
        data.push_back((total_samples >> 28) & 0xFF);
        data.push_back((total_samples >> 20) & 0xFF);
        data.push_back((total_samples >> 12) & 0xFF);
        data.push_back((total_samples >> 4) & 0xFF);
        data.push_back((total_samples << 4) & 0xF0);
        
        // MD5 signature (16 bytes) - all zeros for test
        for (int i = 0; i < 16; i++) {
            data.push_back(0x00);
        }
        
        return data;
    }
    
    // Create Speex identification header
    static std::vector<uint8_t> createSpeexIdHeader() {
        std::vector<uint8_t> data;
        
        // Speex signature (8 bytes with spaces)
        data.insert(data.end(), {'S', 'p', 'e', 'e', 'x', ' ', ' ', ' '});
        
        // Speex version string (20 bytes)
        std::string version = "speex-1.2";
        data.insert(data.end(), version.begin(), version.end());
        // Pad to 20 bytes
        while (data.size() < 28) {
            data.push_back(0x00);
        }
        
        // Speex version ID (4 bytes) - version 1
        data.insert(data.end(), {0x01, 0x00, 0x00, 0x00});
        
        // Header size (4 bytes) - 80 bytes
        data.insert(data.end(), {0x50, 0x00, 0x00, 0x00});
        
        // Sample rate (4 bytes) - 16000 Hz
        data.push_back(0x80);  // 16000 & 0xFF
        data.push_back(0x3E);  // (16000 >> 8) & 0xFF
        data.push_back(0x00);  // (16000 >> 16) & 0xFF
        data.push_back(0x00);  // (16000 >> 24) & 0xFF
        
        // Mode (4 bytes) - narrowband
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00});
        
        // Mode bitstream version (4 bytes) - version 4
        data.insert(data.end(), {0x04, 0x00, 0x00, 0x00});
        
        // Channels (4 bytes) - mono
        data.insert(data.end(), {0x01, 0x00, 0x00, 0x00});
        
        // Bitrate (4 bytes) - 8000 bps
        data.push_back(0x40);  // 8000 & 0xFF
        data.push_back(0x1F);  // (8000 >> 8) & 0xFF
        data.push_back(0x00);  // (8000 >> 16) & 0xFF
        data.push_back(0x00);  // (8000 >> 24) & 0xFF
        
        // Frame size (4 bytes) - 160 samples
        data.insert(data.end(), {0xA0, 0x00, 0x00, 0x00});
        
        // VBR (4 bytes) - 0 (CBR)
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00});
        
        // Frames per packet (4 bytes) - 1
        data.insert(data.end(), {0x01, 0x00, 0x00, 0x00});
        
        // Extra headers (4 bytes) - 0
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00});
        
        // Reserved 1 (4 bytes) - 0
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00});
        
        // Reserved 2 (4 bytes) - 0
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00});
        
        return data;
    }
    
    // Create unknown codec header
    static std::vector<uint8_t> createUnknownCodecHeader() {
        std::vector<uint8_t> data;
        
        // Unknown signature
        data.insert(data.end(), {'U', 'N', 'K', 'N', 'O', 'W', 'N'});
        
        // Some dummy data
        for (int i = 0; i < 20; i++) {
            data.push_back(0x42 + i);
        }
        
        return data;
    }
};

// Test codec identification
bool testVorbisCodecIdentification() {
    std::cout << "Testing Vorbis codec identification..." << std::endl;
    
    try {
        // Create a dummy file for IOHandler
        std::ofstream dummy_file("test_dummy.ogg", std::ios::binary);
        dummy_file.write("dummy", 5);
        dummy_file.close();
        
        auto handler = std::make_unique<FileIOHandler>("test_dummy.ogg");
        OggDemuxer demuxer(std::move(handler));
        
        // Test Vorbis identification header
        auto vorbis_header = MockCodecHeaders::createVorbisIdHeader();
        std::string codec = demuxer.identifyCodec(vorbis_header);
        
        ASSERT(codec == "vorbis", "Should identify Vorbis codec");
        
        std::remove("test_dummy.ogg");
        std::cout << "  ✓ Vorbis codec identification test passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Vorbis codec identification test failed: " << e.what() << std::endl;
        std::remove("test_dummy.ogg");
        return false;
    }
}

bool testOpusCodecIdentification() {
    std::cout << "Testing Opus codec identification..." << std::endl;
    
    try {
        // Create a dummy file for IOHandler
        std::ofstream dummy_file("test_dummy.ogg", std::ios::binary);
        dummy_file.write("dummy", 5);
        dummy_file.close();
        
        auto handler = std::make_unique<FileIOHandler>("test_dummy.ogg");
        OggDemuxer demuxer(std::move(handler));
        
        // Test Opus identification header
        auto opus_header = MockCodecHeaders::createOpusIdHeader();
        std::string codec = demuxer.identifyCodec(opus_header);
        
        ASSERT(codec == "opus", "Should identify Opus codec");
        
        std::remove("test_dummy.ogg");
        std::cout << "  ✓ Opus codec identification test passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Opus codec identification test failed: " << e.what() << std::endl;
        std::remove("test_dummy.ogg");
        return false;
    }
}

bool testFLACCodecIdentification() {
    std::cout << "Testing FLAC codec identification..." << std::endl;
    
    try {
        // Create a dummy file for IOHandler
        std::ofstream dummy_file("test_dummy.ogg", std::ios::binary);
        dummy_file.write("dummy", 5);
        dummy_file.close();
        
        auto handler = std::make_unique<FileIOHandler>("test_dummy.ogg");
        OggDemuxer demuxer(std::move(handler));
        
        // Test FLAC identification header
        auto flac_header = MockCodecHeaders::createFLACIdHeader();
        std::string codec = demuxer.identifyCodec(flac_header);
        
        ASSERT(codec == "flac", "Should identify FLAC codec");
        
        std::remove("test_dummy.ogg");
        std::cout << "  ✓ FLAC codec identification test passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ FLAC codec identification test failed: " << e.what() << std::endl;
        std::remove("test_dummy.ogg");
        return false;
    }
}

bool testSpeexCodecIdentification() {
    std::cout << "Testing Speex codec identification..." << std::endl;
    
    try {
        // Create a dummy file for IOHandler
        std::ofstream dummy_file("test_dummy.ogg", std::ios::binary);
        dummy_file.write("dummy", 5);
        dummy_file.close();
        
        auto handler = std::make_unique<FileIOHandler>("test_dummy.ogg");
        OggDemuxer demuxer(std::move(handler));
        
        // Test Speex identification header
        auto speex_header = MockCodecHeaders::createSpeexIdHeader();
        std::string codec = demuxer.identifyCodec(speex_header);
        
        ASSERT(codec == "speex", "Should identify Speex codec");
        
        std::remove("test_dummy.ogg");
        std::cout << "  ✓ Speex codec identification test passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Speex codec identification test failed: " << e.what() << std::endl;
        std::remove("test_dummy.ogg");
        return false;
    }
}

bool testUnknownCodecIdentification() {
    std::cout << "Testing unknown codec identification..." << std::endl;
    
    try {
        // Create a dummy file for IOHandler
        std::ofstream dummy_file("test_dummy.ogg", std::ios::binary);
        dummy_file.write("dummy", 5);
        dummy_file.close();
        
        auto handler = std::make_unique<FileIOHandler>("test_dummy.ogg");
        OggDemuxer demuxer(std::move(handler));
        
        // Test unknown codec header
        auto unknown_header = MockCodecHeaders::createUnknownCodecHeader();
        std::string codec = demuxer.identifyCodec(unknown_header);
        
        ASSERT(codec.empty(), "Should return empty string for unknown codec");
        
        std::remove("test_dummy.ogg");
        std::cout << "  ✓ Unknown codec identification test passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Unknown codec identification test failed: " << e.what() << std::endl;
        std::remove("test_dummy.ogg");
        return false;
    }
}

bool testEmptyPacketIdentification() {
    std::cout << "Testing empty packet identification..." << std::endl;
    
    try {
        // Create a dummy file for IOHandler
        std::ofstream dummy_file("test_dummy.ogg", std::ios::binary);
        dummy_file.write("dummy", 5);
        dummy_file.close();
        
        auto handler = std::make_unique<FileIOHandler>("test_dummy.ogg");
        OggDemuxer demuxer(std::move(handler));
        
        // Test empty packet
        std::vector<uint8_t> empty_packet;
        std::string codec = demuxer.identifyCodec(empty_packet);
        
        ASSERT(codec.empty(), "Should return empty string for empty packet");
        
        std::remove("test_dummy.ogg");
        std::cout << "  ✓ Empty packet identification test passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Empty packet identification test failed: " << e.what() << std::endl;
        std::remove("test_dummy.ogg");
        return false;
    }
}

// Test header parsing
bool testVorbisHeaderParsing() {
    std::cout << "Testing Vorbis header parsing..." << std::endl;
    
    try {
        // Create a dummy file for IOHandler
        std::ofstream dummy_file("test_dummy.ogg", std::ios::binary);
        dummy_file.write("dummy", 5);
        dummy_file.close();
        
        auto handler = std::make_unique<FileIOHandler>("test_dummy.ogg");
        OggDemuxer demuxer(std::move(handler));
        
        // Get access to streams for testing
        auto& streams = demuxer.getStreamsForTesting();
        
        // Create a test stream
        OggStream stream;
        stream.serial_number = 12345;
        stream.codec_name = "vorbis";
        streams[12345] = stream;
        
        // Test identification header parsing
        auto id_header = MockCodecHeaders::createVorbisIdHeader();
        OggPacket id_packet;
        id_packet.stream_id = 12345;
        id_packet.data = id_header;
        id_packet.granule_position = 0;
        id_packet.is_first_packet = true;
        id_packet.is_last_packet = false;
        
        bool result = demuxer.parseVorbisHeaders(streams[12345], id_packet);
        ASSERT(result, "Should successfully parse Vorbis ID header");
        ASSERT(streams[12345].channels == 2, "Should extract correct channel count");
        ASSERT(streams[12345].sample_rate == 44100, "Should extract correct sample rate");
        
        // Test comment header parsing
        auto comment_header = MockCodecHeaders::createVorbisCommentHeader();
        OggPacket comment_packet;
        comment_packet.stream_id = 12345;
        comment_packet.data = comment_header;
        comment_packet.granule_position = 0;
        comment_packet.is_first_packet = false;
        comment_packet.is_last_packet = false;
        
        result = demuxer.parseVorbisHeaders(streams[12345], comment_packet);
        ASSERT(result, "Should successfully parse Vorbis comment header");
        ASSERT(streams[12345].artist == "Test Artist", "Should extract artist metadata");
        ASSERT(streams[12345].title == "Test Title", "Should extract title metadata");
        ASSERT(streams[12345].album == "Test Album", "Should extract album metadata");
        
        // Test setup header parsing
        auto setup_header = MockCodecHeaders::createVorbisSetupHeader();
        OggPacket setup_packet;
        setup_packet.stream_id = 12345;
        setup_packet.data = setup_header;
        setup_packet.granule_position = 0;
        setup_packet.is_first_packet = false;
        setup_packet.is_last_packet = false;
        
        result = demuxer.parseVorbisHeaders(streams[12345], setup_packet);
        ASSERT(result, "Should successfully parse Vorbis setup header");
        ASSERT(!streams[12345].codec_setup_data.empty(), "Should store setup data");
        
        std::remove("test_dummy.ogg");
        std::cout << "  ✓ Vorbis header parsing test passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Vorbis header parsing test failed: " << e.what() << std::endl;
        std::remove("test_dummy.ogg");
        return false;
    }
}

bool testOpusHeaderParsing() {
    std::cout << "Testing Opus header parsing..." << std::endl;
    
    try {
        // Create a dummy file for IOHandler
        std::ofstream dummy_file("test_dummy.ogg", std::ios::binary);
        dummy_file.write("dummy", 5);
        dummy_file.close();
        
        auto handler = std::make_unique<FileIOHandler>("test_dummy.ogg");
        OggDemuxer demuxer(std::move(handler));
        
        // Get access to streams for testing
        auto& streams = demuxer.getStreamsForTesting();
        
        // Create a test stream
        OggStream stream;
        stream.serial_number = 54321;
        stream.codec_name = "opus";
        streams[54321] = stream;
        
        // Test OpusHead header parsing
        auto id_header = MockCodecHeaders::createOpusIdHeader();
        OggPacket id_packet;
        id_packet.stream_id = 54321;
        id_packet.data = id_header;
        id_packet.granule_position = 0;
        id_packet.is_first_packet = true;
        id_packet.is_last_packet = false;
        
        bool result = demuxer.parseOpusHeaders(streams[54321], id_packet);
        ASSERT(result, "Should successfully parse OpusHead header");
        ASSERT(streams[54321].channels == 2, "Should extract correct channel count");
        ASSERT(streams[54321].sample_rate == 48000, "Should extract correct sample rate");
        ASSERT(streams[54321].pre_skip == 312, "Should extract correct pre-skip");
        
        // Test OpusTags header parsing
        auto comment_header = MockCodecHeaders::createOpusCommentHeader();
        OggPacket comment_packet;
        comment_packet.stream_id = 54321;
        comment_packet.data = comment_header;
        comment_packet.granule_position = 0;
        comment_packet.is_first_packet = false;
        comment_packet.is_last_packet = false;
        
        result = demuxer.parseOpusHeaders(streams[54321], comment_packet);
        ASSERT(result, "Should successfully parse OpusTags header");
        ASSERT(streams[54321].artist == "Opus Test Artist", "Should extract artist metadata");
        ASSERT(streams[54321].title == "Opus Test Title", "Should extract title metadata");
        
        std::remove("test_dummy.ogg");
        std::cout << "  ✓ Opus header parsing test passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Opus header parsing test failed: " << e.what() << std::endl;
        std::remove("test_dummy.ogg");
        return false;
    }
}

bool testFLACHeaderParsing() {
    std::cout << "Testing FLAC header parsing..." << std::endl;
    
    try {
        // Create a dummy file for IOHandler
        std::ofstream dummy_file("test_dummy.ogg", std::ios::binary);
        dummy_file.write("dummy", 5);
        dummy_file.close();
        
        auto handler = std::make_unique<FileIOHandler>("test_dummy.ogg");
        OggDemuxer demuxer(std::move(handler));
        
        // Get access to streams for testing
        auto& streams = demuxer.getStreamsForTesting();
        
        // Create a test stream
        OggStream stream;
        stream.serial_number = 98765;
        stream.codec_name = "flac";
        streams[98765] = stream;
        
        // Test FLAC identification header parsing
        auto id_header = MockCodecHeaders::createFLACIdHeader();
        OggPacket id_packet;
        id_packet.stream_id = 98765;
        id_packet.data = id_header;
        id_packet.granule_position = 0;
        id_packet.is_first_packet = true;
        id_packet.is_last_packet = false;
        
        bool result = demuxer.parseFLACHeaders(streams[98765], id_packet);
        ASSERT(result, "Should successfully parse FLAC header");
        ASSERT(streams[98765].channels == 2, "Should extract correct channel count");
        ASSERT(streams[98765].sample_rate == 44100, "Should extract correct sample rate");
        ASSERT(streams[98765].total_samples == 1000000, "Should extract correct total samples");
        
        std::remove("test_dummy.ogg");
        std::cout << "  ✓ FLAC header parsing test passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ FLAC header parsing test failed: " << e.what() << std::endl;
        std::remove("test_dummy.ogg");
        return false;
    }
}

bool testSpeexHeaderParsing() {
    std::cout << "Testing Speex header parsing..." << std::endl;
    
    try {
        // Create a dummy file for IOHandler
        std::ofstream dummy_file("test_dummy.ogg", std::ios::binary);
        dummy_file.write("dummy", 5);
        dummy_file.close();
        
        auto handler = std::make_unique<FileIOHandler>("test_dummy.ogg");
        OggDemuxer demuxer(std::move(handler));
        
        // Get access to streams for testing
        auto& streams = demuxer.getStreamsForTesting();
        
        // Create a test stream
        OggStream stream;
        stream.serial_number = 11111;
        stream.codec_name = "speex";
        streams[11111] = stream;
        
        // Test Speex identification header parsing
        auto id_header = MockCodecHeaders::createSpeexIdHeader();
        OggPacket id_packet;
        id_packet.stream_id = 11111;
        id_packet.data = id_header;
        id_packet.granule_position = 0;
        id_packet.is_first_packet = true;
        id_packet.is_last_packet = false;
        
        bool result = demuxer.parseSpeexHeaders(streams[11111], id_packet);
        ASSERT(result, "Should successfully parse Speex header");
        ASSERT(streams[11111].channels == 1, "Should extract correct channel count");
        ASSERT(streams[11111].sample_rate == 16000, "Should extract correct sample rate");
        
        std::remove("test_dummy.ogg");
        std::cout << "  ✓ Speex header parsing test passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Speex header parsing test failed: " << e.what() << std::endl;
        std::remove("test_dummy.ogg");
        return false;
    }
}

// Test error handling
bool testInvalidHeaderHandling() {
    std::cout << "Testing invalid header handling..." << std::endl;
    
    try {
        // Create a dummy file for IOHandler
        std::ofstream dummy_file("test_dummy.ogg", std::ios::binary);
        dummy_file.write("dummy", 5);
        dummy_file.close();
        
        auto handler = std::make_unique<FileIOHandler>("test_dummy.ogg");
        OggDemuxer demuxer(std::move(handler));
        
        // Get access to streams for testing
        auto& streams = demuxer.getStreamsForTesting();
        
        // Create a test stream
        OggStream stream;
        stream.serial_number = 99999;
        stream.codec_name = "vorbis";
        streams[99999] = stream;
        
        // Test with too small packet
        std::vector<uint8_t> small_packet = {0x01, 0x02, 0x03};
        OggPacket small_ogg_packet;
        small_ogg_packet.stream_id = 99999;
        small_ogg_packet.data = small_packet;
        small_ogg_packet.granule_position = 0;
        small_ogg_packet.is_first_packet = true;
        small_ogg_packet.is_last_packet = false;
        
        bool result = demuxer.parseVorbisHeaders(streams[99999], small_ogg_packet);
        ASSERT(!result, "Should reject too small packet");
        
        // Test with invalid signature
        std::vector<uint8_t> invalid_packet = {0x01, 'i', 'n', 'v', 'a', 'l', 'i', 'd'};
        invalid_packet.resize(30, 0x00); // Make it large enough
        OggPacket invalid_ogg_packet;
        invalid_ogg_packet.stream_id = 99999;
        invalid_ogg_packet.data = invalid_packet;
        invalid_ogg_packet.granule_position = 0;
        invalid_ogg_packet.is_first_packet = true;
        invalid_ogg_packet.is_last_packet = false;
        
        result = demuxer.parseVorbisHeaders(streams[99999], invalid_ogg_packet);
        ASSERT(!result, "Should reject invalid signature");
        
        std::remove("test_dummy.ogg");
        std::cout << "  ✓ Invalid header handling test passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Invalid header handling test failed: " << e.what() << std::endl;
        std::remove("test_dummy.ogg");
        return false;
    }
}

bool testCorruptedMetadataHandling() {
    std::cout << "Testing corrupted metadata handling..." << std::endl;
    
    try {
        // Create a dummy file for IOHandler
        std::ofstream dummy_file("test_dummy.ogg", std::ios::binary);
        dummy_file.write("dummy", 5);
        dummy_file.close();
        
        auto handler = std::make_unique<FileIOHandler>("test_dummy.ogg");
        OggDemuxer demuxer(std::move(handler));
        
        // Get access to streams for testing
        auto& streams = demuxer.getStreamsForTesting();
        
        // Create a test stream
        OggStream stream;
        stream.serial_number = 88888;
        stream.codec_name = "vorbis";
        streams[88888] = stream;
        
        // Create corrupted comment header (invalid length)
        std::vector<uint8_t> corrupted_comment;
        corrupted_comment.push_back(0x03); // Comment packet type
        corrupted_comment.insert(corrupted_comment.end(), {'v', 'o', 'r', 'b', 'i', 's'});
        
        // Invalid vendor length (too large)
        corrupted_comment.insert(corrupted_comment.end(), {0xFF, 0xFF, 0xFF, 0xFF});
        
        OggPacket corrupted_packet;
        corrupted_packet.stream_id = 88888;
        corrupted_packet.data = corrupted_comment;
        corrupted_packet.granule_position = 0;
        corrupted_packet.is_first_packet = false;
        corrupted_packet.is_last_packet = false;
        
        // Should handle corruption gracefully (not crash)
        bool result = demuxer.parseVorbisHeaders(streams[88888], corrupted_packet);
        // Result can be true or false, but it shouldn't crash
        
        std::remove("test_dummy.ogg");
        std::cout << "  ✓ Corrupted metadata handling test passed (no crash)" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Corrupted metadata handling test failed: " << e.what() << std::endl;
        std::remove("test_dummy.ogg");
        return false;
    }
}

// Main test runner
int main() {
    std::cout << "Running OggDemuxer Codec Detection and Header Processing Tests..." << std::endl;
    std::cout << "=================================================================" << std::endl;
    
    int passed = 0;
    int total = 0;
    
    // Test codec identification
    if (testVorbisCodecIdentification()) passed++;
    total++;
    
    if (testOpusCodecIdentification()) passed++;
    total++;
    
    if (testFLACCodecIdentification()) passed++;
    total++;
    
    if (testSpeexCodecIdentification()) passed++;
    total++;
    
    if (testUnknownCodecIdentification()) passed++;
    total++;
    
    if (testEmptyPacketIdentification()) passed++;
    total++;
    
    // Test header parsing
    if (testVorbisHeaderParsing()) passed++;
    total++;
    
    if (testOpusHeaderParsing()) passed++;
    total++;
    
    if (testFLACHeaderParsing()) passed++;
    total++;
    
    if (testSpeexHeaderParsing()) passed++;
    total++;
    
    // Test error handling
    if (testInvalidHeaderHandling()) passed++;
    total++;
    
    if (testCorruptedMetadataHandling()) passed++;
    total++;
    
    // Print results
    std::cout << "=================================================================" << std::endl;
    std::cout << "Test Results: " << passed << "/" << total << " passed" << std::endl;
    
    if (passed == total) {
        std::cout << "All tests PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << (total - passed) << " tests FAILED!" << std::endl;
        return 1;
    }
}

#else // !HAVE_OGGDEMUXER

int main() {
    std::cout << "OggDemuxer not available - skipping codec detection tests" << std::endl;
    return 0;
}

#endif // HAVE_OGGDEMUXER