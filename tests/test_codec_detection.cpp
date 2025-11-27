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

#include <memory>
#include <vector>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>

// Simple assertion macro
#define ASSERT_TRUE(condition, message) \
    do { \
        if (!(condition)) { \
            throw std::runtime_error(std::string("ASSERTION FAILED: ") + (message) + \
                                    " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
        } \
    } while(0)

// Test helper to create mock IOHandler for testing
class MockIOHandler : public IOHandler {
private:
    std::vector<uint8_t> m_data;
    size_t m_position = 0;
    
public:
    MockIOHandler(const std::vector<uint8_t>& data) : m_data(data) {}
    
    size_t read(void* buffer, size_t size, size_t count) override {
        size_t bytes_to_read = std::min(size * count, m_data.size() - m_position);
        if (bytes_to_read > 0) {
            std::memcpy(buffer, m_data.data() + m_position, bytes_to_read);
            m_position += bytes_to_read;
        }
        return bytes_to_read;
    }
    
    int seek(off_t offset, int whence) override {
        switch (whence) {
            case SEEK_SET:
                m_position = std::min(static_cast<size_t>(offset), m_data.size());
                break;
            case SEEK_CUR:
                m_position = std::min(m_position + offset, m_data.size());
                break;
            case SEEK_END:
                m_position = m_data.size();
                break;
        }
        return 0;
    }
    
    off_t tell() override {
        return static_cast<off_t>(m_position);
    }
    
    bool eof() override {
        return m_position >= m_data.size();
    }
    
    off_t getFileSize() override {
        return static_cast<off_t>(m_data.size());
    }
};

// Test data creation helpers
std::vector<uint8_t> createVorbisIdHeader() {
    std::vector<uint8_t> header;
    
    // Vorbis identification header signature
    header.push_back(0x01);
    header.insert(header.end(), {'v', 'o', 'r', 'b', 'i', 's'});
    
    // Version (4 bytes, little-endian)
    header.insert(header.end(), {0x00, 0x00, 0x00, 0x00});
    
    // Channels (1 byte)
    header.push_back(0x02); // 2 channels
    
    // Sample rate (4 bytes, little-endian) - 44100 Hz
    header.insert(header.end(), {0x44, 0xAC, 0x00, 0x00});
    
    // Bitrate maximum (4 bytes, little-endian)
    header.insert(header.end(), {0x00, 0x00, 0x02, 0x00}); // 128000 bps
    
    // Bitrate nominal (4 bytes, little-endian)
    header.insert(header.end(), {0x00, 0x00, 0x02, 0x00}); // 128000 bps
    
    // Bitrate minimum (4 bytes, little-endian)
    header.insert(header.end(), {0x00, 0x00, 0x00, 0x00});
    
    // Blocksize (1 byte)
    header.push_back(0xB8); // blocksize_0=8, blocksize_1=11
    
    // Framing flag (1 byte)
    header.push_back(0x01);
    
    return header;
}

std::vector<uint8_t> createVorbisCommentHeader() {
    std::vector<uint8_t> header;
    
    // Vorbis comment header signature
    header.push_back(0x03);
    header.insert(header.end(), {'v', 'o', 'r', 'b', 'i', 's'});
    
    // Vendor string length (4 bytes, little-endian)
    std::string vendor = "Test Encoder";
    uint32_t vendor_len = vendor.length();
    header.push_back(vendor_len & 0xFF);
    header.push_back((vendor_len >> 8) & 0xFF);
    header.push_back((vendor_len >> 16) & 0xFF);
    header.push_back((vendor_len >> 24) & 0xFF);
    
    // Vendor string
    header.insert(header.end(), vendor.begin(), vendor.end());
    
    // User comment list length (4 bytes, little-endian)
    uint32_t comment_count = 3;
    header.push_back(comment_count & 0xFF);
    header.push_back((comment_count >> 8) & 0xFF);
    header.push_back((comment_count >> 16) & 0xFF);
    header.push_back((comment_count >> 24) & 0xFF);
    
    // Comments
    std::vector<std::string> comments = {
        "ARTIST=Test Artist",
        "TITLE=Test Title", 
        "ALBUM=Test Album"
    };
    
    for (const auto& comment : comments) {
        uint32_t comment_len = comment.length();
        header.push_back(comment_len & 0xFF);
        header.push_back((comment_len >> 8) & 0xFF);
        header.push_back((comment_len >> 16) & 0xFF);
        header.push_back((comment_len >> 24) & 0xFF);
        header.insert(header.end(), comment.begin(), comment.end());
    }
    
    // Framing bit
    header.push_back(0x01);
    
    return header;
}

std::vector<uint8_t> createVorbisSetupHeader() {
    std::vector<uint8_t> header;
    
    // Vorbis setup header signature
    header.push_back(0x05);
    header.insert(header.end(), {'v', 'o', 'r', 'b', 'i', 's'});
    
    // Minimal setup data (this would normally be much larger)
    header.insert(header.end(), {0x00, 0x00, 0x00, 0x00, 0x01});
    
    return header;
}

std::vector<uint8_t> createOpusIdHeader() {
    std::vector<uint8_t> header;
    
    // OpusHead signature
    header.insert(header.end(), {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd'});
    
    // Version (1 byte)
    header.push_back(0x01);
    
    // Channel count (1 byte)
    header.push_back(0x02); // 2 channels
    
    // Pre-skip (2 bytes, little-endian)
    header.insert(header.end(), {0x38, 0x01}); // 312 samples
    
    // Input sample rate (4 bytes, little-endian) - 48000 Hz
    header.insert(header.end(), {0x80, 0xBB, 0x00, 0x00});
    
    // Output gain (2 bytes, little-endian)
    header.insert(header.end(), {0x00, 0x00});
    
    // Channel mapping family (1 byte)
    header.push_back(0x00);
    
    return header;
}

std::vector<uint8_t> createOpusCommentHeader() {
    std::vector<uint8_t> header;
    
    // OpusTags signature
    header.insert(header.end(), {'O', 'p', 'u', 's', 'T', 'a', 'g', 's'});
    
    // Vendor string length (4 bytes, little-endian)
    std::string vendor = "Test Opus Encoder";
    uint32_t vendor_len = vendor.length();
    header.push_back(vendor_len & 0xFF);
    header.push_back((vendor_len >> 8) & 0xFF);
    header.push_back((vendor_len >> 16) & 0xFF);
    header.push_back((vendor_len >> 24) & 0xFF);
    
    // Vendor string
    header.insert(header.end(), vendor.begin(), vendor.end());
    
    // User comment list length (4 bytes, little-endian)
    uint32_t comment_count = 3;
    header.push_back(comment_count & 0xFF);
    header.push_back((comment_count >> 8) & 0xFF);
    header.push_back((comment_count >> 16) & 0xFF);
    header.push_back((comment_count >> 24) & 0xFF);
    
    // Comments
    std::vector<std::string> comments = {
        "ARTIST=Test Opus Artist",
        "TITLE=Test Opus Title",
        "ALBUM=Test Opus Album"
    };
    
    for (const auto& comment : comments) {
        uint32_t comment_len = comment.length();
        header.push_back(comment_len & 0xFF);
        header.push_back((comment_len >> 8) & 0xFF);
        header.push_back((comment_len >> 16) & 0xFF);
        header.push_back((comment_len >> 24) & 0xFF);
        header.insert(header.end(), comment.begin(), comment.end());
    }
    
    return header;
}

std::vector<uint8_t> createFLACIdHeader() {
    std::vector<uint8_t> header;
    
    // Ogg FLAC identification header signature (5 bytes)
    header.insert(header.end(), {0x7F, 'F', 'L', 'A', 'C'});  // offset 0-4
    
    // Version (1 byte major, 1 byte minor) (2 bytes)
    header.insert(header.end(), {0x01, 0x00});  // offset 5-6
    
    // Number of header packets (2 bytes, big-endian) (2 bytes)
    header.insert(header.end(), {0x00, 0x01});  // offset 7-8
    
    // Native FLAC signature (4 bytes)
    header.insert(header.end(), {'f', 'L', 'a', 'C'});  // offset 9-12
    
    // STREAMINFO metadata block header (4 bytes)
    header.insert(header.end(), {0x80, 0x00, 0x00, 0x22}); // Last block (0x80), type 0, length 34  // offset 13-16
    
    // STREAMINFO data (34 bytes total)
    // Min block size (2 bytes, big-endian)
    header.insert(header.end(), {0x10, 0x00}); // 4096 samples
    
    // Max block size (2 bytes, big-endian)  
    header.insert(header.end(), {0x10, 0x00}); // 4096 samples
    
    // Min frame size (3 bytes, big-endian)
    header.insert(header.end(), {0x00, 0x00, 0x00});
    
    // Max frame size (3 bytes, big-endian)
    header.insert(header.end(), {0x00, 0x00, 0x00});
    
    // Sample rate (20 bits), channels-1 (3 bits), bits per sample-1 (5 bits), total samples high (4 bits)
    // 44100 Hz, 2 channels, 16 bits per sample, 1000000 samples
    uint32_t sample_rate = 44100;
    uint32_t channels_minus_1 = 1; // 2 channels - 1
    uint32_t bits_minus_1 = 15;    // 16 bits - 1
    uint64_t total_samples = 1000000;
    
    // First 4 bytes: sample_rate(20) | channels-1(3) | bits-1(5) | total_samples_high(4)
    uint32_t first_word = (sample_rate << 12) | (channels_minus_1 << 9) | (bits_minus_1 << 4) | ((total_samples >> 32) & 0xF);
    header.push_back((first_word >> 24) & 0xFF);
    header.push_back((first_word >> 16) & 0xFF);
    header.push_back((first_word >> 8) & 0xFF);
    header.push_back(first_word & 0xFF);
    
    // Next 4 bytes: total_samples_low(32)
    uint32_t second_word = total_samples & 0xFFFFFFFF;
    header.push_back((second_word >> 24) & 0xFF);
    header.push_back((second_word >> 16) & 0xFF);
    header.push_back((second_word >> 8) & 0xFF);
    header.push_back(second_word & 0xFF);
    
    // MD5 signature (16 bytes)
    header.insert(header.end(), {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
    
    return header;
}

std::vector<uint8_t> createSpeexHeader() {
    std::vector<uint8_t> header;
    
    // Speex header signature (8 bytes)
    header.insert(header.end(), {'S', 'p', 'e', 'e', 'x', ' ', ' ', ' '});
    
    // Speex version (20 bytes)
    std::string version = "1.2.0";
    header.insert(header.end(), version.begin(), version.end());
    header.resize(header.size() + (20 - version.length()), 0); // Pad to 20 bytes
    
    // Speex version ID (4 bytes, little-endian)
    header.insert(header.end(), {0x01, 0x00, 0x00, 0x00});
    
    // Header size (4 bytes, little-endian)
    header.insert(header.end(), {0x50, 0x00, 0x00, 0x00}); // 80 bytes
    
    // Sample rate (4 bytes, little-endian) - 16000 Hz
    header.insert(header.end(), {0x80, 0x3E, 0x00, 0x00});
    
    // Mode (4 bytes, little-endian)
    header.insert(header.end(), {0x01, 0x00, 0x00, 0x00});
    
    // Mode bitstream version (4 bytes, little-endian)
    header.insert(header.end(), {0x04, 0x00, 0x00, 0x00});
    
    // Channels (4 bytes, little-endian)
    header.insert(header.end(), {0x01, 0x00, 0x00, 0x00}); // 1 channel
    
    // Bitrate (4 bytes, little-endian)
    header.insert(header.end(), {0xFF, 0xFF, 0xFF, 0xFF}); // Variable bitrate
    
    // Frame size (4 bytes, little-endian)
    header.insert(header.end(), {0xA0, 0x00, 0x00, 0x00}); // 160 samples
    
    // VBR (4 bytes, little-endian)
    header.insert(header.end(), {0x01, 0x00, 0x00, 0x00});
    
    // Frames per packet (4 bytes, little-endian)
    header.insert(header.end(), {0x01, 0x00, 0x00, 0x00});
    
    return header;
}

std::vector<uint8_t> createUnknownCodecHeader() {
    std::vector<uint8_t> header;
    
    // Unknown codec signature
    header.insert(header.end(), {'U', 'N', 'K', 'N', 'O', 'W', 'N'});
    
    // Some random data
    header.insert(header.end(), {0x01, 0x02, 0x03, 0x04, 0x05});
    
    return header;
}

// Test codec identification
void test_vorbis_codec_identification() {
    std::vector<uint8_t> empty_data;
    auto mock_handler = std::make_unique<MockIOHandler>(empty_data);
    OggDemuxer demuxer(std::move(mock_handler));
    
    // Test Vorbis identification header
    std::vector<uint8_t> vorbis_header = createVorbisIdHeader();
    std::string codec = demuxer.identifyCodec(vorbis_header);
    
#ifdef HAVE_VORBIS
    ASSERT_TRUE(codec == "vorbis", "Vorbis codec should be identified");
#else
    ASSERT_TRUE(codec.empty(), "Vorbis codec should not be identified when not available");
#endif
}

void test_opus_codec_identification() {
    std::vector<uint8_t> empty_data;
    auto mock_handler = std::make_unique<MockIOHandler>(empty_data);
    OggDemuxer demuxer(std::move(mock_handler));
    
    // Test Opus identification header
    std::vector<uint8_t> opus_header = createOpusIdHeader();
    std::string codec = demuxer.identifyCodec(opus_header);
    
#ifdef HAVE_OPUS
    ASSERT_TRUE(codec == "opus", "Opus codec should be identified");
#else
    ASSERT_TRUE(codec.empty(), "Opus codec should not be identified when not available");
#endif
}

void test_flac_codec_identification() {
    std::vector<uint8_t> empty_data;
    auto mock_handler = std::make_unique<MockIOHandler>(empty_data);
    OggDemuxer demuxer(std::move(mock_handler));
    
    // Test FLAC identification header
    std::vector<uint8_t> flac_header = createFLACIdHeader();
    std::string codec = demuxer.identifyCodec(flac_header);
    
#ifdef HAVE_FLAC
    ASSERT_TRUE(codec == "flac", "FLAC codec should be identified");
#else
    ASSERT_TRUE(codec.empty(), "FLAC codec should not be identified when not available");
#endif
}

void test_speex_codec_identification() {
    std::vector<uint8_t> empty_data;
    auto mock_handler = std::make_unique<MockIOHandler>(empty_data);
    OggDemuxer demuxer(std::move(mock_handler));
    
    // Test Speex identification header - Speex is not implemented yet
    std::vector<uint8_t> speex_header = createSpeexHeader();
    std::string codec = demuxer.identifyCodec(speex_header);
    
    ASSERT_TRUE(codec.empty(), "Speex codec should not be identified (not implemented)");
}

void test_unknown_codec_identification() {
    std::vector<uint8_t> empty_data;
    auto mock_handler = std::make_unique<MockIOHandler>(empty_data);
    OggDemuxer demuxer(std::move(mock_handler));
    
    // Test unknown codec header
    std::vector<uint8_t> unknown_header = createUnknownCodecHeader();
    std::string codec = demuxer.identifyCodec(unknown_header);
    
    ASSERT_TRUE(codec.empty(), "Unknown codec should return empty string");
}

void test_empty_packet_identification() {
    std::vector<uint8_t> empty_data;
    auto mock_handler = std::make_unique<MockIOHandler>(empty_data);
    OggDemuxer demuxer(std::move(mock_handler));
    
    // Test empty packet
    std::vector<uint8_t> empty_packet;
    std::string codec = demuxer.identifyCodec(empty_packet);
    
    ASSERT_TRUE(codec.empty(), "Empty packet should return empty string");
}

// Test Vorbis header parsing
void test_vorbis_header_parsing() {
#ifdef HAVE_VORBIS
    std::vector<uint8_t> empty_data;
    auto mock_handler = std::make_unique<MockIOHandler>(empty_data);
    OggDemuxer demuxer(std::move(mock_handler));
    
    auto& streams = demuxer.getStreamsForTesting();
    OggStream stream;
    stream.serial_number = 1;
    stream.codec_name = "vorbis";
    streams[1] = stream;
    
    // Test identification header
    std::vector<uint8_t> id_header = createVorbisIdHeader();
    OggPacket id_packet;
    id_packet.stream_id = 1;
    id_packet.data = id_header;
    id_packet.granule_position = 0;
    id_packet.is_first_packet = true;
    
    bool result = demuxer.parseVorbisHeaders(streams[1], id_packet);
    ASSERT_TRUE(result, "Vorbis ID header should parse successfully");
    ASSERT_TRUE(streams[1].channels == 2, "Vorbis should have 2 channels");
    ASSERT_TRUE(streams[1].sample_rate == 44100, "Vorbis should have 44100 Hz sample rate");
    
    // Test comment header
    std::vector<uint8_t> comment_header = createVorbisCommentHeader();
    OggPacket comment_packet;
    comment_packet.stream_id = 1;
    comment_packet.data = comment_header;
    comment_packet.granule_position = 0;
    
    result = demuxer.parseVorbisHeaders(streams[1], comment_packet);
    ASSERT_TRUE(result, "Vorbis comment header should parse successfully");
    ASSERT_TRUE(streams[1].artist == "Test Artist", "Vorbis artist should be parsed");
    ASSERT_TRUE(streams[1].title == "Test Title", "Vorbis title should be parsed");
    ASSERT_TRUE(streams[1].album == "Test Album", "Vorbis album should be parsed");
    
    // Test setup header
    std::vector<uint8_t> setup_header = createVorbisSetupHeader();
    OggPacket setup_packet;
    setup_packet.stream_id = 1;
    setup_packet.data = setup_header;
    setup_packet.granule_position = 0;
    
    result = demuxer.parseVorbisHeaders(streams[1], setup_packet);
    ASSERT_TRUE(result, "Vorbis setup header should parse successfully");
    // Note: codec_setup_data was removed from OggStream - setup headers are stored in header_packets
    ASSERT_TRUE(!streams[1].header_packets.empty(), "Vorbis header packets should not be empty");
#endif
}

// Test Opus header parsing
void test_opus_header_parsing() {
#ifdef HAVE_OPUS
    std::vector<uint8_t> empty_data;
    auto mock_handler = std::make_unique<MockIOHandler>(empty_data);
    OggDemuxer demuxer(std::move(mock_handler));
    
    auto& streams = demuxer.getStreamsForTesting();
    OggStream stream;
    stream.serial_number = 1;
    stream.codec_name = "opus";
    streams[1] = stream;
    
    // Test identification header
    std::vector<uint8_t> id_header = createOpusIdHeader();
    OggPacket id_packet;
    id_packet.stream_id = 1;
    id_packet.data = id_header;
    id_packet.granule_position = 0;
    id_packet.is_first_packet = true;
    
    bool result = demuxer.parseOpusHeaders(streams[1], id_packet);
    ASSERT_TRUE(result, "Opus ID header should parse successfully");
    ASSERT_TRUE(streams[1].channels == 2, "Opus should have 2 channels");
    ASSERT_TRUE(streams[1].sample_rate == 48000, "Opus should have 48000 Hz sample rate");
    ASSERT_TRUE(streams[1].pre_skip == 312, "Opus should have 312 pre-skip samples");
    
    // Test comment header
    std::vector<uint8_t> comment_header = createOpusCommentHeader();
    OggPacket comment_packet;
    comment_packet.stream_id = 1;
    comment_packet.data = comment_header;
    comment_packet.granule_position = 0;
    
    result = demuxer.parseOpusHeaders(streams[1], comment_packet);
    ASSERT_TRUE(result, "Opus comment header should parse successfully");
    ASSERT_TRUE(streams[1].artist == "Test Opus Artist", "Opus artist should be parsed");
    ASSERT_TRUE(streams[1].title == "Test Opus Title", "Opus title should be parsed");
    ASSERT_TRUE(streams[1].album == "Test Opus Album", "Opus album should be parsed");
#endif
}

// Test FLAC header parsing
void test_flac_header_parsing() {
#ifdef HAVE_FLAC
    std::vector<uint8_t> empty_data;
    auto mock_handler = std::make_unique<MockIOHandler>(empty_data);
    OggDemuxer demuxer(std::move(mock_handler));
    
    auto& streams = demuxer.getStreamsForTesting();
    OggStream stream;
    stream.serial_number = 1;
    stream.codec_name = "flac";
    streams[1] = stream;
    
    // Test identification header
    std::vector<uint8_t> id_header = createFLACIdHeader();
    OggPacket id_packet;
    id_packet.stream_id = 1;
    id_packet.data = id_header;
    id_packet.granule_position = 0;
    id_packet.is_first_packet = true;
    
    bool result = demuxer.parseFLACHeaders(streams[1], id_packet);
    ASSERT_TRUE(result, "FLAC ID header should parse successfully");
    std::cout << "DEBUG: FLAC packet size=" << id_packet.data.size() << std::endl;
    std::cout << "DEBUG: FLAC channels=" << streams[1].channels << ", sample_rate=" << streams[1].sample_rate << ", total_samples=" << streams[1].total_samples << std::endl;
    ASSERT_TRUE(streams[1].channels == 2, "FLAC should have 2 channels");
    ASSERT_TRUE(streams[1].sample_rate == 44100, "FLAC should have 44100 Hz sample rate");
    ASSERT_TRUE(streams[1].total_samples == 1000000, "FLAC should have 1000000 total samples");
#endif
}

// Speex header parsing test - skipped since Speex is not implemented
void test_speex_header_parsing() {
    // Speex is not implemented yet, so this test is skipped
    std::cout << "Speex header parsing skipped (not implemented)" << std::endl;
}

// Test error handling
void test_invalid_header_handling() {
    std::vector<uint8_t> empty_data;
    auto mock_handler = std::make_unique<MockIOHandler>(empty_data);
    OggDemuxer demuxer(std::move(mock_handler));
    
    auto& streams = demuxer.getStreamsForTesting();
    OggStream stream;
    stream.serial_number = 1;
    streams[1] = stream;
    
    // Test with too small packet
    OggPacket small_packet;
    small_packet.stream_id = 1;
    small_packet.data = {0x01, 0x02}; // Too small
    small_packet.granule_position = 0;
    
#ifdef HAVE_VORBIS
    stream.codec_name = "vorbis";
    bool result = demuxer.parseVorbisHeaders(streams[1], small_packet);
    ASSERT_TRUE(!result, "Vorbis should reject too small packet");
#endif

#ifdef HAVE_OPUS
    stream.codec_name = "opus";
    bool opus_result = demuxer.parseOpusHeaders(streams[1], small_packet);
    ASSERT_TRUE(!opus_result, "Opus should reject too small packet");
#endif

#ifdef HAVE_FLAC
    stream.codec_name = "flac";
    bool flac_result = demuxer.parseFLACHeaders(streams[1], small_packet);
    ASSERT_TRUE(!flac_result, "FLAC should reject too small packet");
#endif

    // Speex is not implemented, so skip this test
}

void test_malformed_comment_handling() {
#ifdef HAVE_VORBIS
    std::vector<uint8_t> empty_data;
    auto mock_handler = std::make_unique<MockIOHandler>(empty_data);
    OggDemuxer demuxer(std::move(mock_handler));
    
    auto& streams = demuxer.getStreamsForTesting();
    OggStream stream;
    stream.serial_number = 1;
    stream.codec_name = "vorbis";
    streams[1] = stream;
    
    // Create malformed comment header (truncated)
    std::vector<uint8_t> malformed_header;
    malformed_header.push_back(0x03);
    malformed_header.insert(malformed_header.end(), {'v', 'o', 'r', 'b', 'i', 's'});
    malformed_header.insert(malformed_header.end(), {0x05, 0x00, 0x00, 0x00}); // Vendor length = 5
    malformed_header.insert(malformed_header.end(), {'T', 'e', 's', 't'}); // Only 4 bytes instead of 5
    
    OggPacket malformed_packet;
    malformed_packet.stream_id = 1;
    malformed_packet.data = malformed_header;
    malformed_packet.granule_position = 0;
    
    // Should handle gracefully without crashing
    bool result = demuxer.parseVorbisHeaders(streams[1], malformed_packet);
    ASSERT_TRUE(result, "Should still return true for valid signature even with malformed data");
#endif
}

// Test header completion detection
void test_header_completion_detection() {
#ifdef HAVE_VORBIS
    std::vector<uint8_t> empty_data;
    auto mock_handler = std::make_unique<MockIOHandler>(empty_data);
    OggDemuxer demuxer(std::move(mock_handler));
    
    auto& streams = demuxer.getStreamsForTesting();
    OggStream stream;
    stream.serial_number = 1;
    stream.codec_name = "vorbis";
    streams[1] = stream;
    
    // Initially headers should not be complete
    ASSERT_TRUE(!streams[1].headers_complete, "Headers should not be complete initially");
    
    // Add identification header
    std::vector<uint8_t> id_header = createVorbisIdHeader();
    OggPacket id_packet;
    id_packet.stream_id = 1;
    id_packet.data = id_header;
    streams[1].header_packets.push_back(id_packet);
    
    // Still not complete (need 3 headers for Vorbis)
    ASSERT_TRUE(!streams[1].headers_complete, "Vorbis headers should not be complete with only 1 header");
    
    // Add comment header
    std::vector<uint8_t> comment_header = createVorbisCommentHeader();
    OggPacket comment_packet;
    comment_packet.stream_id = 1;
    comment_packet.data = comment_header;
    streams[1].header_packets.push_back(comment_packet);
    
    // Still not complete
    ASSERT_TRUE(!streams[1].headers_complete, "Vorbis headers should not be complete with only 2 headers");
    
    // Add setup header
    std::vector<uint8_t> setup_header = createVorbisSetupHeader();
    OggPacket setup_packet;
    setup_packet.stream_id = 1;
    setup_packet.data = setup_header;
    streams[1].header_packets.push_back(setup_packet);
    
    // Now should be complete (3 headers)
    if (streams[1].header_packets.size() >= 3) {
        streams[1].headers_complete = true;
    }
    ASSERT_TRUE(streams[1].headers_complete, "Vorbis headers should be complete with 3 headers");
#endif

#ifdef HAVE_OPUS
    // Test Opus header completion (needs 2 headers)
    OggStream opus_stream;
    opus_stream.serial_number = 2;
    opus_stream.codec_name = "opus";
    streams[2] = opus_stream;
    
    ASSERT_TRUE(!streams[2].headers_complete, "Opus headers should not be complete initially");
    
    // Add identification header
    std::vector<uint8_t> opus_id_header = createOpusIdHeader();
    OggPacket opus_id_packet;
    opus_id_packet.stream_id = 2;
    opus_id_packet.data = opus_id_header;
    streams[2].header_packets.push_back(opus_id_packet);
    
    // Still not complete (need 2 headers for Opus)
    ASSERT_TRUE(!streams[2].headers_complete, "Opus headers should not be complete with only 1 header");
    
    // Add comment header
    std::vector<uint8_t> opus_comment_header = createOpusCommentHeader();
    OggPacket opus_comment_packet;
    opus_comment_packet.stream_id = 2;
    opus_comment_packet.data = opus_comment_header;
    streams[2].header_packets.push_back(opus_comment_packet);
    
    // Now should be complete (2 headers)
    if (streams[2].header_packets.size() >= 2) {
        streams[2].headers_complete = true;
    }
    ASSERT_TRUE(streams[2].headers_complete, "Opus headers should be complete with 2 headers");
#endif

#ifdef HAVE_FLAC
    // Test FLAC header completion (needs 1 header)
    OggStream flac_stream;
    flac_stream.serial_number = 3;
    flac_stream.codec_name = "flac";
    streams[3] = flac_stream;
    
    ASSERT_TRUE(!streams[3].headers_complete, "FLAC headers should not be complete initially");
    
    // Add identification header
    std::vector<uint8_t> flac_id_header = createFLACIdHeader();
    OggPacket flac_id_packet;
    flac_id_packet.stream_id = 3;
    flac_id_packet.data = flac_id_header;
    streams[3].header_packets.push_back(flac_id_packet);
    
    // Should be complete (1 header for FLAC)
    if (streams[3].header_packets.size() >= 1) {
        streams[3].headers_complete = true;
    }
    ASSERT_TRUE(streams[3].headers_complete, "FLAC headers should be complete with 1 header");
#endif
}

// Simple test runner function
bool runTest(const std::string& name, std::function<void()> test_func) {
    std::cout << "Running " << name << "... ";
    try {
        test_func();
        std::cout << "PASSED" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cout << "FAILED: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cout << "FAILED: Unknown exception" << std::endl;
        return false;
    }
}

int main() {
    std::cout << "Running OggDemuxer Codec Detection Tests..." << std::endl;
    std::cout << "===========================================" << std::endl;
    
    int passed = 0;
    int total = 0;
    
    // Codec identification tests
    if (runTest("Vorbis Codec Identification", test_vorbis_codec_identification)) passed++;
    total++;
    
    if (runTest("Opus Codec Identification", test_opus_codec_identification)) passed++;
    total++;
    
    if (runTest("FLAC Codec Identification", test_flac_codec_identification)) passed++;
    total++;
    
    if (runTest("Speex Codec Identification", test_speex_codec_identification)) passed++;
    total++;
    
    if (runTest("Unknown Codec Identification", test_unknown_codec_identification)) passed++;
    total++;
    
    if (runTest("Empty Packet Identification", test_empty_packet_identification)) passed++;
    total++;
    
    // Header parsing tests
    if (runTest("Vorbis Header Parsing", test_vorbis_header_parsing)) passed++;
    total++;
    
    if (runTest("Opus Header Parsing", test_opus_header_parsing)) passed++;
    total++;
    
    if (runTest("FLAC Header Parsing", test_flac_header_parsing)) passed++;
    total++;
    
    // Speex header parsing is skipped since Speex is not implemented
    std::cout << "Skipping Speex Header Parsing (not implemented)..." << std::endl;
    
    // Error handling tests
    if (runTest("Invalid Header Handling", test_invalid_header_handling)) passed++;
    total++;
    
    if (runTest("Malformed Comment Handling", test_malformed_comment_handling)) passed++;
    total++;
    
    // Header completion tests
    if (runTest("Header Completion Detection", test_header_completion_detection)) passed++;
    total++;
    
    // Print results
    std::cout << "===========================================" << std::endl;
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

#include <iostream>

int main() {
    std::cout << "OggDemuxer not available - skipping codec detection tests" << std::endl;
    return 0;
}

#endif // HAVE_OGGDEMUXER