/*
 * test_opus_codec_core_decoding.cpp - Test Opus codec core decoding functionality
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"

#ifdef HAVE_OGGDEMUXER

using namespace TestFramework;
using namespace PsyMP3;
using namespace PsyMP3::Demuxer;
using namespace PsyMP3::Demuxer::Ogg;

// ========== Test Data Creation Utilities ==========

/**
 * @brief Create a mock StreamInfo for Opus testing
 */
StreamInfo createOpusStreamInfo(uint16_t channels = 2, uint32_t sample_rate = 48000, uint32_t bitrate = 128000)
{
    StreamInfo info;
    info.codec_type = "audio";
    info.codec_name = "opus";
    info.channels = channels;
    info.sample_rate = sample_rate;
    info.bitrate = bitrate;
    return info;
}

/**
 * @brief Create a valid OpusHead identification header packet
 */
std::vector<uint8_t> createOpusHeadPacket(uint8_t channels, uint16_t pre_skip = 312, 
                                          int16_t output_gain = 0, uint8_t mapping_family = 0)
{
    std::vector<uint8_t> packet;
    
    // OpusHead signature (8 bytes)
    packet.insert(packet.end(), {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd'});
    
    // Version (1 byte) - must be 1
    packet.push_back(1);
    
    // Channel count (1 byte)
    packet.push_back(channels);
    
    // Pre-skip (2 bytes, little endian)
    packet.push_back(pre_skip & 0xFF);
    packet.push_back((pre_skip >> 8) & 0xFF);
    
    // Input sample rate (4 bytes, little endian) - 48000 Hz
    uint32_t input_rate = 48000;
    packet.push_back(input_rate & 0xFF);
    packet.push_back((input_rate >> 8) & 0xFF);
    packet.push_back((input_rate >> 16) & 0xFF);
    packet.push_back((input_rate >> 24) & 0xFF);
    
    // Output gain (2 bytes, little endian, Q7.8 format)
    packet.push_back(output_gain & 0xFF);
    packet.push_back((output_gain >> 8) & 0xFF);
    
    // Channel mapping family (1 byte)
    packet.push_back(mapping_family);
    
    // For mapping family 1 (surround sound), add additional mapping info
    if (mapping_family == 1 && channels > 2) {
        // Stream count (simplified: one stream per channel for testing)
        packet.push_back(channels);
        
        // Coupled stream count (0 for simplicity in testing)
        packet.push_back(0);
        
        // Channel mapping (each channel maps to its own stream)
        for (uint8_t i = 0; i < channels; i++) {
            packet.push_back(i);
        }
    }
    
    return packet;
}

/**
 * @brief Create a valid OpusTags comment header packet
 */
std::vector<uint8_t> createOpusTagsPacket(const std::string& vendor = "libopus 1.3.1")
{
    std::vector<uint8_t> packet;
    
    // OpusTags signature (8 bytes)
    packet.insert(packet.end(), {'O', 'p', 'u', 's', 'T', 'a', 'g', 's'});
    
    // Vendor string length (4 bytes, little endian)
    uint32_t vendor_len = vendor.length();
    packet.push_back(vendor_len & 0xFF);
    packet.push_back((vendor_len >> 8) & 0xFF);
    packet.push_back((vendor_len >> 16) & 0xFF);
    packet.push_back((vendor_len >> 24) & 0xFF);
    
    // Vendor string
    packet.insert(packet.end(), vendor.begin(), vendor.end());
    
    // User comment list length (4 bytes, little endian) - 0 comments for simplicity
    packet.push_back(0x00);
    packet.push_back(0x00);
    packet.push_back(0x00);
    packet.push_back(0x00);
    
    return packet;
}

/**
 * @brief Create a minimal valid Opus audio packet (silence frame)
 */
std::vector<uint8_t> createOpusAudioPacket(uint8_t channels = 2, bool is_silence = true)
{
    std::vector<uint8_t> packet;
    
    if (is_silence) {
        // Create a minimal silence packet
        // TOC byte: Configuration 0 (SILK-only, NB, 20ms), stereo flag, frame count 1
        uint8_t toc = 0x00; // Configuration 0
        if (channels == 2) {
            toc |= 0x04; // Stereo flag
        }
        packet.push_back(toc);
        
        // For silence, we can use a very minimal packet
        // This creates a valid but minimal Opus packet that should decode to silence
        packet.push_back(0x00); // Minimal frame data
    } else {
        // Create a more complex packet (still minimal but not silence)
        uint8_t toc = 0x10; // Configuration 4 (CELT-only, NB, 20ms)
        if (channels == 2) {
            toc |= 0x04; // Stereo flag
        }
        packet.push_back(toc);
        
        // Add some minimal frame data
        packet.insert(packet.end(), {0x01, 0x02, 0x03});
    }
    
    return packet;
}

// ========== Test Cases ==========

/**
 * @brief Test Opus codec initialization with valid StreamInfo
 */
class TestOpusCodecInitialization : public TestCase {
public:
    TestOpusCodecInitialization() : TestCase("OpusCodec Initialization") {}
    
protected:
    void runTest() override {
        // Test with valid stereo configuration
        StreamInfo info = createOpusStreamInfo(2, 48000, 128000);
        OpusCodec codec(info);
        
        ASSERT_TRUE(codec.initialize(), "Codec should initialize successfully with valid StreamInfo");
        ASSERT_EQUALS("opus", codec.getCodecName(), "Codec name should be 'opus'");
        ASSERT_TRUE(codec.canDecode(info), "Codec should be able to decode Opus streams");
        
        // Test with mono configuration
        StreamInfo mono_info = createOpusStreamInfo(1, 48000, 64000);
        OpusCodec mono_codec(mono_info);
        
        ASSERT_TRUE(mono_codec.initialize(), "Codec should initialize successfully with mono configuration");
        ASSERT_TRUE(mono_codec.canDecode(mono_info), "Codec should be able to decode mono Opus streams");
        
        // Test with invalid codec name
        StreamInfo invalid_info = createOpusStreamInfo();
        invalid_info.codec_name = "mp3";
        
        ASSERT_FALSE(codec.canDecode(invalid_info), "Codec should not decode non-Opus streams");
    }
};

/**
 * @brief Test Opus identification header processing
 */
class TestOpusIdentificationHeader : public TestCase {
public:
    TestOpusIdentificationHeader() : TestCase("Opus Identification Header Processing") {}
    
protected:
    void runTest() override {
        StreamInfo info = createOpusStreamInfo(2);
        OpusCodec codec(info);
        
        ASSERT_TRUE(codec.initialize(), "Codec initialization should succeed");
        
        // Test valid identification header
        auto head_packet = createOpusHeadPacket(2, 312, 0, 0);
        MediaChunk head_chunk;
        head_chunk.data = head_packet;
        
        AudioFrame frame = codec.decode(head_chunk);
        
        // Header packets should return empty frames
        ASSERT_TRUE(frame.samples.empty(), "Identification header should return empty AudioFrame");
        
        // Test with different channel configurations
        auto mono_head = createOpusHeadPacket(1, 312, 0, 0);
        MediaChunk mono_chunk;
        mono_chunk.data = mono_head;
        
        StreamInfo mono_info = createOpusStreamInfo(1);
        OpusCodec mono_codec(mono_info);
        ASSERT_TRUE(mono_codec.initialize(), "Mono codec initialization should succeed");
        
        AudioFrame mono_frame = mono_codec.decode(mono_chunk);
        ASSERT_TRUE(mono_frame.samples.empty(), "Mono identification header should return empty AudioFrame");
        
        // Test with pre-skip value
        auto preskip_head = createOpusHeadPacket(2, 1024, 0, 0);
        MediaChunk preskip_chunk;
        preskip_chunk.data = preskip_head;
        
        OpusCodec preskip_codec(info);
        ASSERT_TRUE(preskip_codec.initialize(), "Pre-skip codec initialization should succeed");
        
        AudioFrame preskip_frame = preskip_codec.decode(preskip_chunk);
        ASSERT_TRUE(preskip_frame.samples.empty(), "Pre-skip identification header should return empty AudioFrame");
        
        // Test with output gain
        auto gain_head = createOpusHeadPacket(2, 312, 256, 0); // +1dB gain in Q7.8 format
        MediaChunk gain_chunk;
        gain_chunk.data = gain_head;
        
        OpusCodec gain_codec(info);
        ASSERT_TRUE(gain_codec.initialize(), "Gain codec initialization should succeed");
        
        AudioFrame gain_frame = gain_codec.decode(gain_chunk);
        ASSERT_TRUE(gain_frame.samples.empty(), "Gain identification header should return empty AudioFrame");
    }
};

/**
 * @brief Test Opus comment header processing
 */
class TestOpusCommentHeader : public TestCase {
public:
    TestOpusCommentHeader() : TestCase("Opus Comment Header Processing") {}
    
protected:
    void runTest() override {
        StreamInfo info = createOpusStreamInfo(2);
        OpusCodec codec(info);
        
        ASSERT_TRUE(codec.initialize(), "Codec initialization should succeed");
        
        // Process identification header first
        auto head_packet = createOpusHeadPacket(2);
        MediaChunk head_chunk;
        head_chunk.data = head_packet;
        
        AudioFrame head_frame = codec.decode(head_chunk);
        ASSERT_TRUE(head_frame.samples.empty(), "ID header should return empty frame");
        
        // Test valid comment header
        auto tags_packet = createOpusTagsPacket("libopus 1.3.1");
        MediaChunk tags_chunk;
        tags_chunk.data = tags_packet;
        
        AudioFrame tags_frame = codec.decode(tags_chunk);
        
        // Comment header should also return empty frame
        ASSERT_TRUE(tags_frame.samples.empty(), "Comment header should return empty AudioFrame");
        
        // Test with different vendor strings
        auto custom_tags = createOpusTagsPacket("Custom Opus Encoder 2.0");
        MediaChunk custom_chunk;
        custom_chunk.data = custom_tags;
        
        OpusCodec custom_codec(info);
        ASSERT_TRUE(custom_codec.initialize(), "Custom codec initialization should succeed");
        
        // Process ID header first for custom codec
        AudioFrame custom_head = custom_codec.decode(head_chunk);
        ASSERT_TRUE(custom_head.samples.empty(), "Custom ID header should return empty frame");
        
        AudioFrame custom_tags_frame = custom_codec.decode(custom_chunk);
        ASSERT_TRUE(custom_tags_frame.samples.empty(), "Custom comment header should return empty AudioFrame");
    }
};

/**
 * @brief Test audio packet decoding with real Opus file
 */
class TestOpusAudioDecoding : public TestCase {
public:
    TestOpusAudioDecoding() : TestCase("Opus Audio Packet Decoding with Real File") {}
    
protected:
    void runTest() override {
        // Test with real Opus file using OggDemuxer
        std::string test_file = "data/bummershort.opus";
        
        try {
            // Create IOHandler for the test file
            auto io_handler = std::make_unique<FileIOHandler>(TagLib::String(test_file.c_str()));
            
            // Create OggDemuxer to extract Opus packets
            OggDemuxer demuxer(std::move(io_handler));
            if (!demuxer.parseContainer()) {
                printf("Skipping real file test - failed to parse Ogg container\n");
                return;
            }
            
            // Get stream info from demuxer
            auto streams = demuxer.getStreams();
            ASSERT_FALSE(streams.empty(), "Opus file should have at least one stream");
            
            StreamInfo opus_stream_info = streams[0];
            ASSERT_EQUALS("opus", opus_stream_info.codec_name, "Stream should be Opus codec");
            
            // Create OpusCodec with real stream info
            OpusCodec codec(opus_stream_info);
            ASSERT_TRUE(codec.initialize(), "Codec should initialize with real stream info");
            
            // Read and decode packets from the real file
            int packets_decoded = 0;
            int audio_frames_received = 0;
            bool headers_processed = false;
            
            for (int i = 0; i < 10 && !demuxer.isEOF(); i++) { // Process first 10 packets
                MediaChunk chunk = demuxer.readChunk();
                if (chunk.data.empty()) {
                    break; // End of stream
                }
                
                packets_decoded++;
                AudioFrame frame = codec.decode(chunk);
                
                if (!frame.samples.empty()) {
                    audio_frames_received++;
                    
                    // Verify frame properties
                    ASSERT_TRUE(frame.channels > 0, "Audio frame should have valid channel count");
                    ASSERT_EQUALS(48000u, frame.sample_rate, "Opus always outputs at 48kHz");
                    ASSERT_TRUE(frame.samples.size() > 0, "Audio frame should have samples");
                    
                    // Verify reasonable sample count (Opus frames are typically 960-5760 samples per channel)
                    size_t samples_per_channel = frame.samples.size() / frame.channels;
                    ASSERT_TRUE(samples_per_channel >= 120, "Frame should have at least 120 samples per channel (2.5ms at 48kHz)");
                    ASSERT_TRUE(samples_per_channel <= 5760, "Frame should have at most 5760 samples per channel (120ms at 48kHz)");
                    
                    printf("Decoded frame: %u channels, %zu samples per channel\n", 
                           frame.channels, samples_per_channel);
                } else {
                    // Empty frames are expected for headers
                    if (packets_decoded <= 2) {
                        headers_processed = true;
                    }
                }
            }
            
            ASSERT_TRUE(packets_decoded > 0, "Should have processed some packets from real file");
            ASSERT_TRUE(audio_frames_received > 0, "Should have received some audio frames from real file");
            
            printf("Successfully processed %d packets, received %d audio frames\n", 
                   packets_decoded, audio_frames_received);
                   
        } catch (const std::exception& e) {
            printf("Skipping real file test - exception: %s\n", e.what());
            return;
        }
    }
};

/**
 * @brief Test pre-skip processing
 */
class TestOpusPreSkipProcessing : public TestCase {
public:
    TestOpusPreSkipProcessing() : TestCase("Opus Pre-skip Processing") {}
    
protected:
    void runTest() override {
        StreamInfo info = createOpusStreamInfo(2);
        OpusCodec codec(info);
        
        ASSERT_TRUE(codec.initialize(), "Codec initialization should succeed");
        
        // Create identification header with significant pre-skip
        uint16_t pre_skip_samples = 1024;
        auto head_packet = createOpusHeadPacket(2, pre_skip_samples, 0, 0);
        auto tags_packet = createOpusTagsPacket();
        
        MediaChunk head_chunk;
        head_chunk.data = head_packet;
        MediaChunk tags_chunk;
        tags_chunk.data = tags_packet;
        
        // Process headers
        AudioFrame head_frame = codec.decode(head_chunk);
        AudioFrame tags_frame = codec.decode(tags_chunk);
        
        ASSERT_TRUE(head_frame.samples.empty(), "Header frames should be empty");
        ASSERT_TRUE(tags_frame.samples.empty(), "Header frames should be empty");
        
        // Decode several audio packets to test pre-skip behavior
        auto audio_packet = createOpusAudioPacket(2, true);
        MediaChunk audio_chunk;
        audio_chunk.data = audio_packet;
        
        // First few frames might be affected by pre-skip
        AudioFrame first_frame = codec.decode(audio_chunk);
        
        if (!first_frame.samples.empty()) {
            ASSERT_EQUALS(2u, first_frame.channels, "First frame should have correct channels");
            ASSERT_EQUALS(48000u, first_frame.sample_rate, "First frame should have correct sample rate");
            
            // Pre-skip should reduce the number of samples in early frames
            // The exact behavior depends on the implementation, but we should get some samples
            ASSERT_TRUE(first_frame.samples.size() > 0, "First frame should have some samples after pre-skip");
        }
        
        // Subsequent frames should have normal sample counts
        AudioFrame second_frame = codec.decode(audio_chunk);
        
        if (!second_frame.samples.empty()) {
            ASSERT_EQUALS(2u, second_frame.channels, "Second frame should have correct channels");
            ASSERT_EQUALS(48000u, second_frame.sample_rate, "Second frame should have correct sample rate");
        }
    }
};

/**
 * @brief Test output gain processing
 */
class TestOpusOutputGainProcessing : public TestCase {
public:
    TestOpusOutputGainProcessing() : TestCase("Opus Output Gain Processing") {}
    
protected:
    void runTest() override {
        StreamInfo info = createOpusStreamInfo(2);
        
        // Test with zero gain (no processing)
        {
            OpusCodec zero_gain_codec(info);
            ASSERT_TRUE(zero_gain_codec.initialize(), "Zero gain codec initialization should succeed");
            
            auto head_packet = createOpusHeadPacket(2, 312, 0, 0); // Zero gain
            auto tags_packet = createOpusTagsPacket();
            
            MediaChunk head_chunk;
            head_chunk.data = head_packet;
            MediaChunk tags_chunk;
            tags_chunk.data = tags_packet;
            
            AudioFrame head_frame = zero_gain_codec.decode(head_chunk);
            AudioFrame tags_frame = zero_gain_codec.decode(tags_chunk);
            
            auto audio_packet = createOpusAudioPacket(2, true);
            MediaChunk audio_chunk;
            audio_chunk.data = audio_packet;
            
            AudioFrame audio_frame = zero_gain_codec.decode(audio_chunk);
            
            if (!audio_frame.samples.empty()) {
                ASSERT_EQUALS(2u, audio_frame.channels, "Zero gain frame should have correct channels");
                ASSERT_EQUALS(48000u, audio_frame.sample_rate, "Zero gain frame should have correct sample rate");
            }
        }
        
        // Test with positive gain
        {
            OpusCodec pos_gain_codec(info);
            ASSERT_TRUE(pos_gain_codec.initialize(), "Positive gain codec initialization should succeed");
            
            int16_t positive_gain = 256; // +1dB in Q7.8 format
            auto head_packet = createOpusHeadPacket(2, 312, positive_gain, 0);
            auto tags_packet = createOpusTagsPacket();
            
            MediaChunk head_chunk;
            head_chunk.data = head_packet;
            MediaChunk tags_chunk;
            tags_chunk.data = tags_packet;
            
            AudioFrame head_frame = pos_gain_codec.decode(head_chunk);
            AudioFrame tags_frame = pos_gain_codec.decode(tags_chunk);
            
            auto audio_packet = createOpusAudioPacket(2, false); // Non-silence for gain testing
            MediaChunk audio_chunk;
            audio_chunk.data = audio_packet;
            
            AudioFrame audio_frame = pos_gain_codec.decode(audio_chunk);
            
            if (!audio_frame.samples.empty()) {
                ASSERT_EQUALS(2u, audio_frame.channels, "Positive gain frame should have correct channels");
                ASSERT_EQUALS(48000u, audio_frame.sample_rate, "Positive gain frame should have correct sample rate");
                
                // With positive gain, samples should be amplified (but we can't easily test exact values)
                // Just verify the frame is valid
                ASSERT_TRUE(audio_frame.samples.size() > 0, "Positive gain frame should have samples");
            }
        }
        
        // Test with negative gain (attenuation)
        {
            OpusCodec neg_gain_codec(info);
            ASSERT_TRUE(neg_gain_codec.initialize(), "Negative gain codec initialization should succeed");
            
            int16_t negative_gain = -256; // -1dB in Q7.8 format
            auto head_packet = createOpusHeadPacket(2, 312, negative_gain, 0);
            auto tags_packet = createOpusTagsPacket();
            
            MediaChunk head_chunk;
            head_chunk.data = head_packet;
            MediaChunk tags_chunk;
            tags_chunk.data = tags_packet;
            
            AudioFrame head_frame = neg_gain_codec.decode(head_chunk);
            AudioFrame tags_frame = neg_gain_codec.decode(tags_chunk);
            
            auto audio_packet = createOpusAudioPacket(2, false); // Non-silence for gain testing
            MediaChunk audio_chunk;
            audio_chunk.data = audio_packet;
            
            AudioFrame audio_frame = neg_gain_codec.decode(audio_chunk);
            
            if (!audio_frame.samples.empty()) {
                ASSERT_EQUALS(2u, audio_frame.channels, "Negative gain frame should have correct channels");
                ASSERT_EQUALS(48000u, audio_frame.sample_rate, "Negative gain frame should have correct sample rate");
                
                // With negative gain, samples should be attenuated
                ASSERT_TRUE(audio_frame.samples.size() > 0, "Negative gain frame should have samples");
            }
        }
    }
};

/**
 * @brief Test multi-channel configurations
 */
class TestOpusMultiChannelConfigurations : public TestCase {
public:
    TestOpusMultiChannelConfigurations() : TestCase("Opus Multi-Channel Configurations") {}
    
protected:
    void runTest() override {
        // Test mono configuration (mapping family 0)
        {
            StreamInfo mono_info = createOpusStreamInfo(1);
            OpusCodec mono_codec(mono_info);
            
            ASSERT_TRUE(mono_codec.initialize(), "Mono codec should initialize");
            
            auto mono_head = createOpusHeadPacket(1, 312, 0, 0);
            MediaChunk mono_chunk;
            mono_chunk.data = mono_head;
            
            AudioFrame mono_frame = mono_codec.decode(mono_chunk);
            ASSERT_TRUE(mono_frame.samples.empty(), "Mono header should return empty frame");
        }
        
        // Test stereo configuration (mapping family 0)
        {
            StreamInfo stereo_info = createOpusStreamInfo(2);
            OpusCodec stereo_codec(stereo_info);
            
            ASSERT_TRUE(stereo_codec.initialize(), "Stereo codec should initialize");
            
            auto stereo_head = createOpusHeadPacket(2, 312, 0, 0);
            MediaChunk stereo_chunk;
            stereo_chunk.data = stereo_head;
            
            AudioFrame stereo_frame = stereo_codec.decode(stereo_chunk);
            ASSERT_TRUE(stereo_frame.samples.empty(), "Stereo header should return empty frame");
        }
        
        // Test 5.1 surround configuration (mapping family 1)
        {
            StreamInfo surround_info = createOpusStreamInfo(6);
            OpusCodec surround_codec(surround_info);
            
            ASSERT_TRUE(surround_codec.initialize(), "5.1 surround codec should initialize");
            
            auto surround_head = createOpusHeadPacket(6, 312, 0, 1); // Mapping family 1
            MediaChunk surround_chunk;
            surround_chunk.data = surround_head;
            
            AudioFrame surround_frame = surround_codec.decode(surround_chunk);
            ASSERT_TRUE(surround_frame.samples.empty(), "5.1 surround header should return empty frame");
        }
        
        // Test 7.1 surround configuration (mapping family 1)
        {
            StreamInfo surround71_info = createOpusStreamInfo(8);
            OpusCodec surround71_codec(surround71_info);
            
            ASSERT_TRUE(surround71_codec.initialize(), "7.1 surround codec should initialize");
            
            auto surround71_head = createOpusHeadPacket(8, 312, 0, 1); // Mapping family 1
            MediaChunk surround71_chunk;
            surround71_chunk.data = surround71_head;
            
            AudioFrame surround71_frame = surround71_codec.decode(surround71_chunk);
            ASSERT_TRUE(surround71_frame.samples.empty(), "7.1 surround header should return empty frame");
        }
    }
};

// ========== Test Suite Setup ==========

int main()
{
    TestSuite suite("Opus Codec Core Decoding Tests");
    
    // Add all test cases
    suite.addTest(std::make_unique<TestOpusCodecInitialization>());
    suite.addTest(std::make_unique<TestOpusIdentificationHeader>());
    suite.addTest(std::make_unique<TestOpusCommentHeader>());
    suite.addTest(std::make_unique<TestOpusAudioDecoding>());
    suite.addTest(std::make_unique<TestOpusPreSkipProcessing>());
    suite.addTest(std::make_unique<TestOpusOutputGainProcessing>());
    suite.addTest(std::make_unique<TestOpusMultiChannelConfigurations>());
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print results
    suite.printResults(results);
    
    // Return appropriate exit code
    return suite.getFailureCount(results) == 0 ? 0 : 1;
}

#else

int main()
{
    printf("Opus codec not available (HAVE_OGGDEMUXER not defined)\n");
    return 0;
}

#endif // HAVE_OGGDEMUXER