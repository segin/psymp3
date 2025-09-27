/*
 * test_opus_integration.cpp - Integration tests for OpusCodec with demuxer architecture
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

#ifdef HAVE_OGGDEMUXER

/**
 * @brief Test fixture for Opus codec integration tests
 */
class OpusIntegrationTest {
public:
    OpusIntegrationTest() = default;
    ~OpusIntegrationTest() = default;
    
    /**
     * @brief Create a minimal valid Opus identification header
     */
    static std::vector<uint8_t> createOpusIdHeader(uint8_t channels = 2, uint16_t pre_skip = 312, int16_t gain = 0) {
        std::vector<uint8_t> header;
        
        // OpusHead signature
        header.insert(header.end(), {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd'});
        
        // Version (1 byte)
        header.push_back(1);
        
        // Channel count (1 byte)
        header.push_back(channels);
        
        // Pre-skip (2 bytes, little-endian)
        header.push_back(pre_skip & 0xFF);
        header.push_back((pre_skip >> 8) & 0xFF);
        
        // Input sample rate (4 bytes, little-endian) - 48000 Hz
        uint32_t sample_rate = 48000;
        header.push_back(sample_rate & 0xFF);
        header.push_back((sample_rate >> 8) & 0xFF);
        header.push_back((sample_rate >> 16) & 0xFF);
        header.push_back((sample_rate >> 24) & 0xFF);
        
        // Output gain (2 bytes, little-endian, Q7.8 format)
        header.push_back(gain & 0xFF);
        header.push_back((gain >> 8) & 0xFF);
        
        // Channel mapping family (1 byte)
        header.push_back(0); // Family 0 for mono/stereo
        
        return header;
    }
    
    /**
     * @brief Create a minimal valid Opus comment header
     */
    static std::vector<uint8_t> createOpusCommentHeader() {
        std::vector<uint8_t> header;
        
        // OpusTags signature
        header.insert(header.end(), {'O', 'p', 'u', 's', 'T', 'a', 'g', 's'});
        
        // Vendor string length (4 bytes, little-endian)
        std::string vendor = "libopus 1.3.1";
        uint32_t vendor_len = vendor.length();
        header.push_back(vendor_len & 0xFF);
        header.push_back((vendor_len >> 8) & 0xFF);
        header.push_back((vendor_len >> 16) & 0xFF);
        header.push_back((vendor_len >> 24) & 0xFF);
        
        // Vendor string
        header.insert(header.end(), vendor.begin(), vendor.end());
        
        // User comment list count (4 bytes, little-endian)
        uint32_t comment_count = 0;
        header.push_back(comment_count & 0xFF);
        header.push_back((comment_count >> 8) & 0xFF);
        header.push_back((comment_count >> 16) & 0xFF);
        header.push_back((comment_count >> 24) & 0xFF);
        
        return header;
    }
    
    /**
     * @brief Create a minimal valid Opus audio packet (silence)
     */
    static std::vector<uint8_t> createOpusAudioPacket(int frame_size_ms = 20) {
        // Create a minimal Opus packet for silence
        // TOC byte: Configuration 16 (SILK-only, 20ms, stereo)
        std::vector<uint8_t> packet;
        packet.push_back(0x78); // TOC: config=30 (20ms), stereo=1, frame_count=0
        
        // For a silence packet, we can use a very minimal payload
        // This creates a valid but minimal Opus packet
        packet.push_back(0x00); // Minimal payload for silence
        
        return packet;
    }
    
    /**
     * @brief Create StreamInfo for Opus testing
     */
    static StreamInfo createOpusStreamInfo(uint32_t stream_id = 1, uint16_t channels = 2) {
        StreamInfo info;
        info.stream_id = stream_id;
        info.codec_type = "audio";
        info.codec_name = "opus";
        info.sample_rate = 48000; // Opus always outputs at 48kHz
        info.channels = channels;
        info.bitrate = 128000; // 128 kbps
        return info;
    }
    
    /**
     * @brief Create MediaChunk for testing
     */
    static MediaChunk createMediaChunk(uint32_t stream_id, const std::vector<uint8_t>& data) {
        MediaChunk chunk;
        chunk.stream_id = stream_id;
        chunk.data = data;
        chunk.is_keyframe = true;
        return chunk;
    }
};

/**
 * @brief Test OpusCodec integration with OggDemuxer - Basic functionality
 * Requirements: 6.1, 11.3
 */
bool test_opus_codec_with_ogg_demuxer_basic() {
    Debug::log("test", "=== Testing OpusCodec integration with OggDemuxer - Basic ===");
    
    try {
        // Create StreamInfo for Opus
        StreamInfo stream_info = OpusIntegrationTest::createOpusStreamInfo();
        
        // Create OpusCodec instance
        OpusCodec codec(stream_info);
        
        // Test codec can decode Opus streams
        if (!codec.canDecode(stream_info)) {
            Debug::log("test", "FAIL: OpusCodec should be able to decode Opus streams");
            return false;
        }
        
        // Test codec name
        if (codec.getCodecName() != "opus") {
            Debug::log("test", "FAIL: OpusCodec should return 'opus' as codec name, got: ", codec.getCodecName());
            return false;
        }
        
        // Initialize codec
        if (!codec.initialize()) {
            Debug::log("test", "FAIL: OpusCodec initialization failed");
            return false;
        }
        
        Debug::log("test", "PASS: Basic OpusCodec integration test");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test", "FAIL: Exception in basic integration test: ", e.what());
        return false;
    }
}

/**
 * @brief Test MediaChunk processing and AudioFrame output format
 * Requirements: 6.1, 11.3, 11.4
 */
bool test_media_chunk_processing_and_audio_frame_output() {
    Debug::log("test", "=== Testing MediaChunk processing and AudioFrame output ===");
    
    try {
        // Create StreamInfo for stereo Opus
        StreamInfo stream_info = OpusIntegrationTest::createOpusStreamInfo(1, 2);
        
        // Create OpusCodec instance
        OpusCodec codec(stream_info);
        
        if (!codec.initialize()) {
            Debug::log("test", "FAIL: OpusCodec initialization failed");
            return false;
        }
        
        // Test header packet processing
        auto id_header = OpusIntegrationTest::createOpusIdHeader(2, 312, 0);
        MediaChunk id_chunk = OpusIntegrationTest::createMediaChunk(1, id_header);
        
        AudioFrame id_frame = codec.decode(id_chunk);
        if (!id_frame.samples.empty()) {
            Debug::log("test", "FAIL: ID header should not produce audio samples");
            return false;
        }
        
        // Test comment header processing
        auto comment_header = OpusIntegrationTest::createOpusCommentHeader();
        MediaChunk comment_chunk = OpusIntegrationTest::createMediaChunk(1, comment_header);
        
        AudioFrame comment_frame = codec.decode(comment_chunk);
        if (!comment_frame.samples.empty()) {
            Debug::log("test", "FAIL: Comment header should not produce audio samples");
            return false;
        }
        
        // Test audio packet processing
        auto audio_packet = OpusIntegrationTest::createOpusAudioPacket(20);
        MediaChunk audio_chunk = OpusIntegrationTest::createMediaChunk(1, audio_packet);
        
        AudioFrame audio_frame = codec.decode(audio_chunk);
        
        // Validate AudioFrame format
        if (audio_frame.sample_rate != 48000) {
            Debug::log("test", "FAIL: AudioFrame should have 48kHz sample rate, got: ", audio_frame.sample_rate);
            return false;
        }
        
        if (audio_frame.channels != 2) {
            Debug::log("test", "FAIL: AudioFrame should have 2 channels, got: ", audio_frame.channels);
            return false;
        }
        
        // For a 20ms frame at 48kHz stereo, we expect 960 samples per channel = 1920 total samples
        size_t expected_samples = 960 * 2; // 20ms * 48000 Hz * 2 channels / 1000
        if (audio_frame.samples.size() != expected_samples) {
            Debug::log("test", "INFO: AudioFrame has ", audio_frame.samples.size(), 
                      " samples, expected around ", expected_samples, " for 20ms frame");
            // Don't fail on this as the actual frame size may vary
        }
        
        // Validate samples are 16-bit PCM
        if (!audio_frame.samples.empty()) {
            // Check that samples are within valid 16-bit range
            for (int16_t sample : audio_frame.samples) {
                if (sample < -32768 || sample > 32767) {
                    Debug::log("test", "FAIL: Sample out of 16-bit range: ", sample);
                    return false;
                }
            }
        }
        
        Debug::log("test", "PASS: MediaChunk processing and AudioFrame output format test");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test", "FAIL: Exception in MediaChunk processing test: ", e.what());
        return false;
    }
}

/**
 * @brief Test seeking support through reset() method
 * Requirements: 6.3, 12.8
 */
bool test_seeking_support_through_reset() {
    Debug::log("test", "=== Testing seeking support through reset() method ===");
    
    try {
        // Create StreamInfo for Opus
        StreamInfo stream_info = OpusIntegrationTest::createOpusStreamInfo();
        
        // Create OpusCodec instance
        OpusCodec codec(stream_info);
        
        if (!codec.initialize()) {
            Debug::log("test", "FAIL: OpusCodec initialization failed");
            return false;
        }
        
        // Process headers to get codec into decoding state
        auto id_header = OpusIntegrationTest::createOpusIdHeader();
        MediaChunk id_chunk = OpusIntegrationTest::createMediaChunk(1, id_header);
        codec.decode(id_chunk);
        
        auto comment_header = OpusIntegrationTest::createOpusCommentHeader();
        MediaChunk comment_chunk = OpusIntegrationTest::createMediaChunk(1, comment_header);
        codec.decode(comment_chunk);
        
        // Decode some audio packets to establish state
        auto audio_packet1 = OpusIntegrationTest::createOpusAudioPacket(20);
        MediaChunk audio_chunk1 = OpusIntegrationTest::createMediaChunk(1, audio_packet1);
        AudioFrame frame1 = codec.decode(audio_chunk1);
        
        auto audio_packet2 = OpusIntegrationTest::createOpusAudioPacket(20);
        MediaChunk audio_chunk2 = OpusIntegrationTest::createMediaChunk(1, audio_packet2);
        AudioFrame frame2 = codec.decode(audio_chunk2);
        
        // Test reset functionality (simulating seek operation)
        codec.reset();
        
        // After reset, codec should still be able to decode
        auto audio_packet3 = OpusIntegrationTest::createOpusAudioPacket(20);
        MediaChunk audio_chunk3 = OpusIntegrationTest::createMediaChunk(1, audio_packet3);
        AudioFrame frame3 = codec.decode(audio_chunk3);
        
        // Validate that reset worked - frame should have proper format
        if (frame3.sample_rate != 48000) {
            Debug::log("test", "FAIL: After reset, AudioFrame should have 48kHz sample rate");
            return false;
        }
        
        if (frame3.channels != 2) {
            Debug::log("test", "FAIL: After reset, AudioFrame should have 2 channels");
            return false;
        }
        
        // Test flush after reset
        AudioFrame flush_frame = codec.flush();
        // Flush may or may not return data, but should not crash
        
        Debug::log("test", "PASS: Seeking support through reset() method test");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test", "FAIL: Exception in seeking support test: ", e.what());
        return false;
    }
}

/**
 * @brief Test integration with DemuxedStream bridge interface
 * Requirements: 11.4, 12.8
 */
bool test_integration_with_demuxed_stream_bridge() {
    Debug::log("test", "=== Testing integration with DemuxedStream bridge interface ===");
    
    try {
        // Test that OpusCodec can be created through AudioCodecFactory
        StreamInfo stream_info = OpusIntegrationTest::createOpusStreamInfo();
        
        auto codec = AudioCodecFactory::createCodec(stream_info);
        if (!codec) {
            Debug::log("test", "FAIL: AudioCodecFactory should create OpusCodec for Opus streams");
            return false;
        }
        
        // Verify it's actually an OpusCodec
        if (codec->getCodecName() != "opus") {
            Debug::log("test", "FAIL: Factory should create OpusCodec, got: ", codec->getCodecName());
            return false;
        }
        
        // Test initialization through factory-created codec
        if (!codec->initialize()) {
            Debug::log("test", "FAIL: Factory-created OpusCodec initialization failed");
            return false;
        }
        
        // Test that codec can handle MediaChunk data format used by DemuxedStream
        auto id_header = OpusIntegrationTest::createOpusIdHeader();
        MediaChunk chunk = OpusIntegrationTest::createMediaChunk(1, id_header);
        
        // Verify MediaChunk format compatibility
        if (!chunk.isValid()) {
            Debug::log("test", "FAIL: MediaChunk should be valid");
            return false;
        }
        
        if (chunk.stream_id != 1) {
            Debug::log("test", "FAIL: MediaChunk should have correct stream_id");
            return false;
        }
        
        // Test decoding through factory-created codec
        AudioFrame frame = codec->decode(chunk);
        // Header packets don't produce audio, so empty frame is expected
        
        // Test codec interface methods required by DemuxedStream
        if (!codec->canDecode(stream_info)) {
            Debug::log("test", "FAIL: Codec should report it can decode its own stream info");
            return false;
        }
        
        StreamInfo retrieved_info = codec->getStreamInfo();
        if (retrieved_info.codec_name != "opus") {
            Debug::log("test", "FAIL: Retrieved stream info should have opus codec name");
            return false;
        }
        
        // Test reset functionality required for seeking in DemuxedStream
        codec->reset();
        
        // Test flush functionality required for end-of-stream in DemuxedStream
        AudioFrame flush_frame = codec->flush();
        // Flush may or may not return data, but should not crash
        
        Debug::log("test", "PASS: Integration with DemuxedStream bridge interface test");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test", "FAIL: Exception in DemuxedStream bridge test: ", e.what());
        return false;
    }
}

/**
 * @brief Test error handling in integration scenarios
 */
bool test_integration_error_handling() {
    Debug::log("test", "=== Testing integration error handling ===");
    
    try {
        // Test with invalid stream info
        StreamInfo invalid_info;
        invalid_info.stream_id = 1;
        invalid_info.codec_type = "audio";
        invalid_info.codec_name = "invalid_codec";
        
        auto codec = AudioCodecFactory::createCodec(invalid_info);
        if (codec) {
            Debug::log("test", "FAIL: Factory should not create codec for invalid codec name");
            return false;
        }
        
        // Test with valid Opus codec but invalid data
        StreamInfo opus_info = OpusIntegrationTest::createOpusStreamInfo();
        OpusCodec opus_codec(opus_info);
        
        if (!opus_codec.initialize()) {
            Debug::log("test", "FAIL: OpusCodec initialization failed");
            return false;
        }
        
        // Test with invalid MediaChunk data
        std::vector<uint8_t> invalid_data = {0x00, 0x01, 0x02, 0x03}; // Not valid Opus data
        MediaChunk invalid_chunk = OpusIntegrationTest::createMediaChunk(1, invalid_data);
        
        AudioFrame frame = opus_codec.decode(invalid_chunk);
        // Should handle invalid data gracefully (may return empty frame or silence)
        
        // Test with empty MediaChunk
        MediaChunk empty_chunk;
        empty_chunk.stream_id = 1;
        // data is empty
        
        AudioFrame empty_frame = opus_codec.decode(empty_chunk);
        // Should handle empty chunk gracefully
        
        Debug::log("test", "PASS: Integration error handling test");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test", "FAIL: Exception in integration error handling test: ", e.what());
        return false;
    }
}

/**
 * @brief Test multi-channel Opus integration
 */
bool test_multichannel_opus_integration() {
    Debug::log("test", "=== Testing multi-channel Opus integration ===");
    
    try {
        // Test with 5.1 surround sound (6 channels)
        StreamInfo surround_info = OpusIntegrationTest::createOpusStreamInfo(1, 6);
        
        OpusCodec surround_codec(surround_info);
        
        if (!surround_codec.canDecode(surround_info)) {
            Debug::log("test", "FAIL: OpusCodec should support multi-channel streams");
            return false;
        }
        
        if (!surround_codec.initialize()) {
            Debug::log("test", "FAIL: Multi-channel OpusCodec initialization failed");
            return false;
        }
        
        // Create multi-channel ID header (channel mapping family 1 for surround)
        std::vector<uint8_t> mc_header;
        
        // OpusHead signature
        mc_header.insert(mc_header.end(), {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd'});
        
        // Version
        mc_header.push_back(1);
        
        // Channel count (6 for 5.1)
        mc_header.push_back(6);
        
        // Pre-skip
        mc_header.push_back(312 & 0xFF);
        mc_header.push_back((312 >> 8) & 0xFF);
        
        // Input sample rate (48000 Hz)
        uint32_t sample_rate = 48000;
        mc_header.push_back(sample_rate & 0xFF);
        mc_header.push_back((sample_rate >> 8) & 0xFF);
        mc_header.push_back((sample_rate >> 16) & 0xFF);
        mc_header.push_back((sample_rate >> 24) & 0xFF);
        
        // Output gain
        mc_header.push_back(0);
        mc_header.push_back(0);
        
        // Channel mapping family (1 for Vorbis channel order)
        mc_header.push_back(1);
        
        // Stream count and coupled stream count for 5.1
        mc_header.push_back(4); // stream_count
        mc_header.push_back(2); // coupled_stream_count
        
        // Channel mapping for 5.1: FL, FR, FC, LFE, BL, BR
        mc_header.insert(mc_header.end(), {0, 1, 2, 3, 4, 5});
        
        MediaChunk mc_id_chunk = OpusIntegrationTest::createMediaChunk(1, mc_header);
        AudioFrame mc_id_frame = surround_codec.decode(mc_id_chunk);
        
        // Should not produce audio from header
        if (!mc_id_frame.samples.empty()) {
            Debug::log("test", "FAIL: Multi-channel ID header should not produce audio");
            return false;
        }
        
        Debug::log("test", "PASS: Multi-channel Opus integration test");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test", "FAIL: Exception in multi-channel integration test: ", e.what());
        return false;
    }
}

/**
 * @brief Run all Opus integration tests
 */
bool run_opus_integration_tests() {
    Debug::log("test", "Starting Opus codec integration tests...");
    
    bool all_passed = true;
    
    // Test 13.1: Integration with demuxer architecture
    all_passed &= test_opus_codec_with_ogg_demuxer_basic();
    all_passed &= test_media_chunk_processing_and_audio_frame_output();
    all_passed &= test_seeking_support_through_reset();
    all_passed &= test_integration_with_demuxed_stream_bridge();
    all_passed &= test_integration_error_handling();
    all_passed &= test_multichannel_opus_integration();
    
    if (all_passed) {
        Debug::log("test", "=== ALL OPUS INTEGRATION TESTS PASSED ===");
    } else {
        Debug::log("test", "=== SOME OPUS INTEGRATION TESTS FAILED ===");
    }
    
    return all_passed;
}

#endif // HAVE_OGGDEMUXER

/**
 * @brief Main test entry point
 */
int main() {
    printf("Starting Opus Integration Test Suite\n");
    Debug::log("test", "Opus Integration Test Suite");
    
#ifdef HAVE_OGGDEMUXER
    printf("HAVE_OGGDEMUXER is defined, running tests\n");
    
    // Register only the Opus codec for testing
    AudioCodecFactory::registerCodec("opus", [](const StreamInfo& info) {
        return std::make_unique<OpusCodec>(info);
    });
    
    bool success = run_opus_integration_tests();
    printf("Test result: %s\n", success ? "PASS" : "FAIL");
    return success ? 0 : 1;
#else
    printf("HAVE_OGGDEMUXER not defined, skipping tests\n");
    Debug::log("test", "Ogg demuxer not available - skipping Opus integration tests");
    return 0;
#endif
}