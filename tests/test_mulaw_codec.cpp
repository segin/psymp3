/*
 * test_mulaw_codec.cpp - Tests for μ-law codec implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"

using namespace TestFramework;

/**
 * @brief Test μ-law silence value handling
 * 
 * ITU-T G.711 specifies that μ-law value 0xFF represents silence (0)
 */
void test_mulaw_silence_value() {
    StreamInfo stream_info;
    stream_info.codec_name = "mulaw";
    stream_info.sample_rate = 8000;
    stream_info.channels = 1;
    
    MuLawCodec codec(stream_info);
    ASSERT_TRUE(codec.initialize(), "Codec should initialize successfully");
    
    // Test silence value (0xFF) maps to 0
    MediaChunk chunk(0, std::vector<uint8_t>{0xFF});
    AudioFrame frame = codec.decode(chunk);
    
    ASSERT_EQUALS(1, frame.samples.size(), "Output should contain 1 sample");
    ASSERT_EQUALS(0, frame.samples[0], "μ-law silence value (0xFF) should map to 0");
    ASSERT_EQUALS(8000, frame.sample_rate, "Sample rate should be preserved");
    ASSERT_EQUALS(1, frame.channels, "Channel count should be preserved");
}

/**
 * @brief Test μ-law sign bit handling
 * 
 * ITU-T G.711 μ-law uses bit 7 as sign bit:
 * - Values 0x00-0x7F should be negative
 * - Values 0x80-0xFE should be positive
 * - Value 0xFF is special case (silence = 0)
 */
void test_mulaw_sign_bit_handling() {
    StreamInfo stream_info;
    stream_info.codec_name = "mulaw";
    stream_info.sample_rate = 8000;
    stream_info.channels = 1;
    
    MuLawCodec codec(stream_info);
    ASSERT_TRUE(codec.initialize(), "Codec should initialize successfully");
    
    // Test negative values (0x00-0x7F)
    MediaChunk negative_chunk(0, std::vector<uint8_t>{0x00, 0x01, 0x7E, 0x7F});
    AudioFrame negative_frame = codec.decode(negative_chunk);
    
    ASSERT_EQUALS(4, negative_frame.samples.size(), "Should decode 4 negative samples");
    for (size_t i = 0; i < negative_frame.samples.size(); ++i) {
        ASSERT_TRUE(negative_frame.samples[i] < 0, "Values 0x00-0x7F should be negative");
    }
    
    // Reset codec for next test
    codec.reset();
    
    // Test positive values (0x80-0xFE)
    MediaChunk positive_chunk(0, std::vector<uint8_t>{0x80, 0x81, 0xFD, 0xFE});
    AudioFrame positive_frame = codec.decode(positive_chunk);
    
    ASSERT_EQUALS(4, positive_frame.samples.size(), "Should decode 4 positive samples");
    for (size_t i = 0; i < positive_frame.samples.size(); ++i) {
        ASSERT_TRUE(positive_frame.samples[i] > 0, "Values 0x80-0xFE should be positive");
    }
}

/**
 * @brief Test μ-law maximum amplitude values
 * 
 * Test that maximum positive and negative μ-law values produce
 * expected high-amplitude PCM samples
 */
void test_mulaw_maximum_amplitudes() {
    StreamInfo stream_info;
    stream_info.codec_name = "mulaw";
    stream_info.sample_rate = 8000;
    stream_info.channels = 1;
    
    MuLawCodec codec(stream_info);
    ASSERT_TRUE(codec.initialize(), "Codec should initialize successfully");
    
    // Test maximum negative value (0x00)
    MediaChunk max_neg_chunk(0, std::vector<uint8_t>{0x00});
    AudioFrame max_neg_frame = codec.decode(max_neg_chunk);
    
    ASSERT_EQUALS(1, max_neg_frame.samples.size(), "Should decode 1 sample");
    ASSERT_TRUE(max_neg_frame.samples[0] < -30000, "Maximum negative μ-law should produce high negative amplitude");
    
    // Reset codec for next test
    codec.reset();
    
    // Test maximum positive value (0x80)
    MediaChunk max_pos_chunk(0, std::vector<uint8_t>{0x80});
    AudioFrame max_pos_frame = codec.decode(max_pos_chunk);
    
    ASSERT_EQUALS(1, max_pos_frame.samples.size(), "Should decode 1 sample");
    ASSERT_TRUE(max_pos_frame.samples[0] > 30000, "Maximum positive μ-law should produce high positive amplitude");
}

/**
 * @brief Test μ-law codec format detection
 * 
 * Verify that the codec correctly identifies μ-law formats
 */
void test_mulaw_format_detection() {
    StreamInfo mulaw_stream;
    mulaw_stream.codec_name = "mulaw";
    MuLawCodec codec(mulaw_stream);
    
    // Test accepted format identifiers
    StreamInfo test_stream;
    
    test_stream.codec_name = "mulaw";
    ASSERT_TRUE(codec.canDecode(test_stream), "Should accept 'mulaw' format");
    
    test_stream.codec_name = "pcm_mulaw";
    ASSERT_TRUE(codec.canDecode(test_stream), "Should accept 'pcm_mulaw' format");
    
    test_stream.codec_name = "g711_mulaw";
    ASSERT_TRUE(codec.canDecode(test_stream), "Should accept 'g711_mulaw' format");
    
    // Test rejected format identifiers
    test_stream.codec_name = "alaw";
    ASSERT_FALSE(codec.canDecode(test_stream), "Should reject 'alaw' format");
    
    test_stream.codec_name = "pcm";
    ASSERT_FALSE(codec.canDecode(test_stream), "Should reject 'pcm' format");
    
    test_stream.codec_name = "mp3";
    ASSERT_FALSE(codec.canDecode(test_stream), "Should reject 'mp3' format");
}

/**
 * @brief Test μ-law multi-channel processing
 * 
 * Verify that multi-channel μ-law data is processed correctly
 */
void test_mulaw_multichannel() {
    StreamInfo stream_info;
    stream_info.codec_name = "mulaw";
    stream_info.sample_rate = 8000;
    stream_info.channels = 2; // Stereo
    
    MuLawCodec codec(stream_info);
    ASSERT_TRUE(codec.initialize(), "Codec should initialize successfully");
    
    // Test stereo data: left=0x80 (max positive), right=0x00 (max negative)
    MediaChunk stereo_chunk(0, std::vector<uint8_t>{0x80, 0x00, 0x80, 0x00});
    AudioFrame stereo_frame = codec.decode(stereo_chunk);
    
    ASSERT_EQUALS(4, stereo_frame.samples.size(), "Output should contain 4 samples");
    ASSERT_EQUALS(8000, stereo_frame.sample_rate, "Sample rate should be preserved");
    ASSERT_EQUALS(2, stereo_frame.channels, "Channel count should be preserved");
    
    // Verify interleaved channel data
    ASSERT_TRUE(stereo_frame.samples[0] > 30000, "First sample (left) should be high positive");
    ASSERT_TRUE(stereo_frame.samples[1] < -30000, "Second sample (right) should be high negative");
    ASSERT_TRUE(stereo_frame.samples[2] > 30000, "Third sample (left) should be high positive");
    ASSERT_TRUE(stereo_frame.samples[3] < -30000, "Fourth sample (right) should be high negative");
}

int main() {
    TestSuite suite("μ-law Codec Tests");
    
    suite.addTest("test_mulaw_silence_value", test_mulaw_silence_value);
    suite.addTest("test_mulaw_sign_bit_handling", test_mulaw_sign_bit_handling);
    suite.addTest("test_mulaw_maximum_amplitudes", test_mulaw_maximum_amplitudes);
    suite.addTest("test_mulaw_format_detection", test_mulaw_format_detection);
    suite.addTest("test_mulaw_multichannel", test_mulaw_multichannel);
    
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results);
}