/*
 * test_iso_telephony_codecs.cpp - Test telephony codec support in ISO demuxer
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include <memory>
#include <vector>

// Test telephony codec detection and configuration
TEST(ISODemuxer, TelephonyCodecDetection) {
    // Test μ-law codec configuration
    AudioTrackInfo ulawTrack;
    ulawTrack.codecType = "ulaw";
    ulawTrack.sampleRate = 0;  // Should be set to 8000
    ulawTrack.channelCount = 0; // Should be set to 1
    ulawTrack.bitsPerSample = 0; // Should be set to 8
    
    // Create a BoxParser to test telephony configuration
    std::shared_ptr<IOHandler> dummyHandler;
    BoxParser parser(dummyHandler);
    
    // Test μ-law configuration
    bool result = parser.ConfigureTelephonyCodec(ulawTrack, "ulaw");
    ASSERT_TRUE(result);
    ASSERT_EQ(ulawTrack.sampleRate, 8000u);
    ASSERT_EQ(ulawTrack.channelCount, 1u);
    ASSERT_EQ(ulawTrack.bitsPerSample, 8u);
    ASSERT_TRUE(ulawTrack.codecConfig.empty()); // Raw format needs no config
    
    // Test A-law codec configuration
    AudioTrackInfo alawTrack;
    alawTrack.codecType = "alaw";
    alawTrack.sampleRate = 0;  // Should be set to 8000
    alawTrack.channelCount = 0; // Should be set to 1
    alawTrack.bitsPerSample = 0; // Should be set to 8
    
    result = parser.ConfigureTelephonyCodec(alawTrack, "alaw");
    ASSERT_TRUE(result);
    ASSERT_EQ(alawTrack.sampleRate, 8000u);
    ASSERT_EQ(alawTrack.channelCount, 1u);
    ASSERT_EQ(alawTrack.bitsPerSample, 8u);
    ASSERT_TRUE(alawTrack.codecConfig.empty()); // Raw format needs no config
}

TEST(ISODemuxer, TelephonyParameterValidation) {
    std::shared_ptr<IOHandler> dummyHandler;
    BoxParser parser(dummyHandler);
    
    // Test valid telephony parameters
    AudioTrackInfo validTrack;
    validTrack.codecType = "ulaw";
    validTrack.sampleRate = 8000;
    validTrack.channelCount = 1;
    validTrack.bitsPerSample = 8;
    
    bool result = parser.ValidateTelephonyParameters(validTrack);
    ASSERT_TRUE(result);
    
    // Test 16kHz sample rate (also valid)
    validTrack.sampleRate = 16000;
    result = parser.ValidateTelephonyParameters(validTrack);
    ASSERT_TRUE(result);
    
    // Test invalid sample rate (should be corrected)
    AudioTrackInfo invalidTrack;
    invalidTrack.codecType = "alaw";
    invalidTrack.sampleRate = 44100; // Invalid for telephony
    invalidTrack.channelCount = 2;    // Invalid for telephony
    invalidTrack.bitsPerSample = 16;  // Invalid for telephony
    
    result = parser.ValidateTelephonyParameters(invalidTrack);
    ASSERT_TRUE(result); // Should still return true after correction
    ASSERT_EQ(invalidTrack.sampleRate, 8000u);  // Corrected to 8kHz
    ASSERT_EQ(invalidTrack.channelCount, 1u);   // Corrected to mono
    ASSERT_EQ(invalidTrack.bitsPerSample, 8u);  // Corrected to 8-bit
}

TEST(ISODemuxer, TelephonyTimingCalculation) {
    // Create a dummy ISO demuxer for testing
    std::unique_ptr<IOHandler> dummyHandler;
    ISODemuxer demuxer(std::move(dummyHandler));
    
    // Test μ-law timing calculation
    AudioTrackInfo ulawTrack;
    ulawTrack.codecType = "ulaw";
    ulawTrack.sampleRate = 8000;
    ulawTrack.timescale = 8000;
    
    // Test timing for various sample indices
    uint64_t timing0 = demuxer.CalculateTelephonyTiming(ulawTrack, 0);
    ASSERT_EQ(timing0, 0u);
    
    uint64_t timing8000 = demuxer.CalculateTelephonyTiming(ulawTrack, 8000);
    ASSERT_EQ(timing8000, 1000u); // 1 second = 1000ms
    
    uint64_t timing4000 = demuxer.CalculateTelephonyTiming(ulawTrack, 4000);
    ASSERT_EQ(timing4000, 500u); // 0.5 seconds = 500ms
    
    // Test A-law timing calculation at 16kHz
    AudioTrackInfo alawTrack;
    alawTrack.codecType = "alaw";
    alawTrack.sampleRate = 16000;
    alawTrack.timescale = 16000;
    
    uint64_t timing16000 = demuxer.CalculateTelephonyTiming(alawTrack, 16000);
    ASSERT_EQ(timing16000, 1000u); // 1 second = 1000ms
}

TEST(ISODemuxer, TelephonyCodecValidation) {
    std::unique_ptr<IOHandler> dummyHandler;
    ISODemuxer demuxer(std::move(dummyHandler));
    
    // Test valid μ-law configuration
    AudioTrackInfo validUlaw;
    validUlaw.codecType = "ulaw";
    validUlaw.sampleRate = 8000;
    validUlaw.channelCount = 1;
    validUlaw.bitsPerSample = 8;
    
    bool result = demuxer.ValidateTelephonyCodecConfiguration(validUlaw);
    ASSERT_TRUE(result);
    
    // Test valid A-law configuration
    AudioTrackInfo validAlaw;
    validAlaw.codecType = "alaw";
    validAlaw.sampleRate = 16000;
    validAlaw.channelCount = 1;
    validAlaw.bitsPerSample = 8;
    
    result = demuxer.ValidateTelephonyCodecConfiguration(validAlaw);
    ASSERT_TRUE(result);
    
    // Test invalid configuration
    AudioTrackInfo invalidConfig;
    invalidConfig.codecType = "ulaw";
    invalidConfig.sampleRate = 44100; // Invalid
    invalidConfig.channelCount = 2;    // Invalid
    invalidConfig.bitsPerSample = 16;  // Invalid
    
    result = demuxer.ValidateTelephonyCodecConfiguration(invalidConfig);
    ASSERT_FALSE(result);
    
    // Test non-telephony codec (should return true)
    AudioTrackInfo nonTelephony;
    nonTelephony.codecType = "aac";
    nonTelephony.sampleRate = 44100;
    nonTelephony.channelCount = 2;
    nonTelephony.bitsPerSample = 16;
    
    result = demuxer.ValidateTelephonyCodecConfiguration(nonTelephony);
    ASSERT_TRUE(result); // Not a telephony codec, so validation passes
}

int main() {
    return run_all_tests();
}