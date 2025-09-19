/*
 * test_iso_codec_compliance.cpp - Codec-specific compliance tests for ISO demuxer
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
#include <cstring>

using namespace TestFramework;

// Mock IOHandler for codec testing
class CodecMockIOHandler : public IOHandler {
private:
    std::vector<uint8_t> data;
    size_t position = 0;
    
public:
    CodecMockIOHandler(const std::vector<uint8_t>& testData) : data(testData) {}
    
    size_t read(void* buffer, size_t size, size_t count) override {
        size_t totalBytes = size * count;
        size_t availableBytes = data.size() - position;
        size_t bytesToRead = std::min(totalBytes, availableBytes);
        
        if (bytesToRead > 0) {
            std::memcpy(buffer, data.data() + position, bytesToRead);
            position += bytesToRead;
        }
        
        return bytesToRead / size;
    }
    
    int seek(off_t offset, int whence) override {
        switch (whence) {
            case SEEK_SET:
                position = static_cast<size_t>(offset);
                break;
            case SEEK_CUR:
                position += static_cast<size_t>(offset);
                break;
            case SEEK_END:
                position = data.size() + static_cast<size_t>(offset);
                break;
        }
        
        if (position > data.size()) {
            position = data.size();
        }
        
        return 0;
    }
    
    off_t tell() override {
        return static_cast<off_t>(position);
    }
    
    bool eof() override {
        return position >= data.size();
    }
};

// Test class for AAC codec compliance
class AACCodecComplianceTest : public TestCase {
public:
    AACCodecComplianceTest() : TestCase("AACCodecCompliance") {}
    
protected:
    void runTest() override {
        testValidAACConfigurations();
        testInvalidAACConfigurations();
        testAACProfileValidation();
        testAACSampleRateValidation();
        testAACChannelConfigValidation();
        testAACConfigurationMismatch();
    }
    
private:
    void testValidAACConfigurations() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<CodecMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        AudioTrackInfo track;
        track.codecType = "aac";
        track.sampleRate = 44100;
        track.channelCount = 2;
        track.bitsPerSample = 16;
        
        // Test LC profile, 44.1kHz, stereo
        std::vector<uint8_t> lcConfig = {0x12, 0x10}; // LC profile, 44.1kHz, stereo
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("aac", lcConfig, track), 
                   "Valid LC AAC configuration should pass");
        
        // Test HE-AAC profile
        std::vector<uint8_t> heConfig = {0x16, 0x10}; // HE-AAC profile, 44.1kHz, stereo
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("aac", heConfig, track), 
                   "Valid HE-AAC configuration should pass");
        
        // Test different sample rates
        track.sampleRate = 48000;
        std::vector<uint8_t> config48k = {0x11, 0x90}; // LC profile, 48kHz, stereo
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("aac", config48k, track), 
                   "Valid 48kHz AAC configuration should pass");
        
        // Test mono configuration
        track.channelCount = 1;
        std::vector<uint8_t> monoConfig = {0x11, 0x88}; // LC profile, 48kHz, mono
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("aac", monoConfig, track), 
                   "Valid mono AAC configuration should pass");
    }
    
    void testInvalidAACConfigurations() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<CodecMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        AudioTrackInfo track;
        track.codecType = "aac";
        track.sampleRate = 44100;
        track.channelCount = 2;
        track.bitsPerSample = 16;
        
        // Test empty configuration
        std::vector<uint8_t> emptyConfig;
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("aac", emptyConfig, track), 
                    "Empty AAC configuration should fail");
        
        // Test too short configuration
        std::vector<uint8_t> shortConfig = {0x12};
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("aac", shortConfig, track), 
                    "Too short AAC configuration should fail");
        
        // Test invalid profile
        std::vector<uint8_t> invalidProfile = {0x00, 0x10}; // Invalid profile
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("aac", invalidProfile, track), 
                    "Invalid AAC profile should fail");
        
        // Test reserved values
        std::vector<uint8_t> reservedConfig = {0xFF, 0xFF}; // All bits set (reserved)
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("aac", reservedConfig, track), 
                    "Reserved AAC configuration values should fail");
    }
    
    void testAACProfileValidation() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<CodecMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        AudioTrackInfo track;
        track.codecType = "aac";
        track.sampleRate = 44100;
        track.channelCount = 2;
        track.bitsPerSample = 16;
        
        // Test Main profile (profile 1)
        std::vector<uint8_t> mainProfile = {0x0A, 0x10}; // Main profile, 44.1kHz, stereo
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("aac", mainProfile, track), 
                   "Main AAC profile should be valid");
        
        // Test LC profile (profile 2) - most common
        std::vector<uint8_t> lcProfile = {0x12, 0x10}; // LC profile, 44.1kHz, stereo
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("aac", lcProfile, track), 
                   "LC AAC profile should be valid");
        
        // Test SSR profile (profile 3)
        std::vector<uint8_t> ssrProfile = {0x1A, 0x10}; // SSR profile, 44.1kHz, stereo
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("aac", ssrProfile, track), 
                   "SSR AAC profile should be valid");
        
        // Test LTP profile (profile 4)
        std::vector<uint8_t> ltpProfile = {0x22, 0x10}; // LTP profile, 44.1kHz, stereo
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("aac", ltpProfile, track), 
                   "LTP AAC profile should be valid");
    }
    
    void testAACSampleRateValidation() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<CodecMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        AudioTrackInfo track;
        track.codecType = "aac";
        track.channelCount = 2;
        track.bitsPerSample = 16;
        
        // Test standard sample rates
        std::vector<std::pair<uint32_t, uint8_t>> standardRates = {
            {96000, 0x0}, {88200, 0x1}, {64000, 0x2}, {48000, 0x3},
            {44100, 0x4}, {32000, 0x5}, {24000, 0x6}, {22050, 0x7},
            {16000, 0x8}, {12000, 0x9}, {11025, 0xA}, {8000, 0xB}
        };
        
        for (const auto& rate : standardRates) {
            track.sampleRate = rate.first;
            uint8_t configByte1 = 0x10 | (rate.second >> 1); // LC profile + sample rate high bits
            uint8_t configByte2 = ((rate.second & 0x1) << 7) | 0x10; // Sample rate low bit + stereo
            std::vector<uint8_t> config = {configByte1, configByte2};
            
            ASSERT_TRUE(validator.ValidateCodecDataIntegrity("aac", config, track), 
                       "Standard AAC sample rate " + std::to_string(rate.first) + " should be valid");
        }
        
        // Test invalid sample rate
        track.sampleRate = 12345; // Non-standard rate
        std::vector<uint8_t> invalidRateConfig = {0x12, 0x10}; // Config says 44.1kHz but track says 12345
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("aac", invalidRateConfig, track), 
                    "Mismatched AAC sample rate should fail");
    }
    
    void testAACChannelConfigValidation() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<CodecMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        AudioTrackInfo track;
        track.codecType = "aac";
        track.sampleRate = 44100;
        track.bitsPerSample = 16;
        
        // Test valid channel configurations
        std::vector<std::pair<uint16_t, uint8_t>> channelConfigs = {
            {1, 0x1}, // Mono
            {2, 0x2}, // Stereo
            {3, 0x3}, // 3 channels
            {4, 0x4}, // 4 channels
            {5, 0x5}, // 5 channels
            {6, 0x6}, // 5.1 channels
            {8, 0x7}  // 7.1 channels
        };
        
        for (const auto& config : channelConfigs) {
            track.channelCount = config.first;
            uint8_t configByte2 = 0x10 | (config.second << 3); // 44.1kHz + channel config
            std::vector<uint8_t> aacConfig = {0x12, configByte2}; // LC profile
            
            ASSERT_TRUE(validator.ValidateCodecDataIntegrity("aac", aacConfig, track), 
                       "AAC " + std::to_string(config.first) + " channel configuration should be valid");
        }
        
        // Test invalid channel configuration
        track.channelCount = 2;
        std::vector<uint8_t> invalidChannelConfig = {0x12, 0x18}; // Config says mono but track says stereo
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("aac", invalidChannelConfig, track), 
                    "Mismatched AAC channel configuration should fail");
    }
    
    void testAACConfigurationMismatch() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<CodecMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        AudioTrackInfo track;
        track.codecType = "aac";
        track.sampleRate = 44100;
        track.channelCount = 2;
        track.bitsPerSample = 16;
        
        // Test sample rate mismatch
        std::vector<uint8_t> mismatchedRate = {0x11, 0x90}; // Config says 48kHz but track says 44.1kHz
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("aac", mismatchedRate, track), 
                    "Sample rate mismatch should fail validation");
        
        // Test channel count mismatch
        std::vector<uint8_t> mismatchedChannels = {0x12, 0x08}; // Config says mono but track says stereo
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("aac", mismatchedChannels, track), 
                    "Channel count mismatch should fail validation");
        
        // Test correct configuration for comparison
        std::vector<uint8_t> correctConfig = {0x12, 0x10}; // LC profile, 44.1kHz, stereo
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("aac", correctConfig, track), 
                   "Correct AAC configuration should pass validation");
    }
};

// Test class for ALAC codec compliance
class ALACCodecComplianceTest : public TestCase {
public:
    ALACCodecComplianceTest() : TestCase("ALACCodecCompliance") {}
    
protected:
    void runTest() override {
        testValidALACConfigurations();
        testInvalidALACConfigurations();
        testALACMagicCookieValidation();
        testALACParameterValidation();
    }
    
private:
    void testValidALACConfigurations() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<CodecMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        AudioTrackInfo track;
        track.codecType = "alac";
        track.sampleRate = 44100;
        track.channelCount = 2;
        track.bitsPerSample = 16;
        
        // Create valid ALAC magic cookie
        std::vector<uint8_t> validALACConfig = {
            0x00, 0x00, 0x00, 0x24, // Size (36 bytes)
            'a', 'l', 'a', 'c',     // Type
            0x00, 0x00, 0x00, 0x00, // Version/flags
            0x00, 0x00, 0x10, 0x00, // Frame length (4096)
            0x10,                   // Bit depth (16)
            0x28,                   // History mult (40)
            0x0A,                   // Initial history (10)
            0x0E,                   // K modifier (14)
            0x02,                   // Channels (2)
            0xFF, 0xFF,             // Max run (65535)
            0x00, 0x00, 0xAC, 0x44, // Sample rate (44100)
            0x00, 0x00, 0xAC, 0x44  // Sample rate again
        };
        validALACConfig.resize(36); // Pad to full size
        
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("alac", validALACConfig, track), 
                   "Valid ALAC configuration should pass");
    }
    
    void testInvalidALACConfigurations() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<CodecMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        AudioTrackInfo track;
        track.codecType = "alac";
        track.sampleRate = 44100;
        track.channelCount = 2;
        track.bitsPerSample = 16;
        
        // Test empty configuration
        std::vector<uint8_t> emptyConfig;
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("alac", emptyConfig, track), 
                    "Empty ALAC configuration should fail");
        
        // Test too short configuration
        std::vector<uint8_t> shortConfig = {0x00, 0x00, 0x00, 0x10, 'a', 'l', 'a', 'c'};
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("alac", shortConfig, track), 
                    "Too short ALAC configuration should fail");
        
        // Test wrong magic number
        std::vector<uint8_t> wrongMagic = {
            0x00, 0x00, 0x00, 0x24,
            'w', 'r', 'o', 'n',     // Wrong magic
            0x00, 0x00, 0x00, 0x00
        };
        wrongMagic.resize(36);
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("alac", wrongMagic, track), 
                    "Wrong ALAC magic number should fail");
    }
    
    void testALACMagicCookieValidation() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<CodecMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        AudioTrackInfo track;
        track.codecType = "alac";
        track.sampleRate = 44100;
        track.channelCount = 2;
        track.bitsPerSample = 16;
        
        // Test various magic cookie sizes
        std::vector<uint8_t> config24 = {
            0x00, 0x00, 0x00, 0x18, // Size (24 bytes)
            'a', 'l', 'a', 'c',
            0x00, 0x00, 0x00, 0x00
        };
        config24.resize(24);
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("alac", config24, track), 
                   "24-byte ALAC magic cookie should be valid");
        
        std::vector<uint8_t> config36 = {
            0x00, 0x00, 0x00, 0x24, // Size (36 bytes)
            'a', 'l', 'a', 'c',
            0x00, 0x00, 0x00, 0x00
        };
        config36.resize(36);
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("alac", config36, track), 
                   "36-byte ALAC magic cookie should be valid");
        
        // Test invalid size
        std::vector<uint8_t> invalidSize = {
            0x00, 0x00, 0x00, 0x08, // Size too small
            'a', 'l', 'a', 'c'
        };
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("alac", invalidSize, track), 
                    "Invalid ALAC magic cookie size should fail");
    }
    
    void testALACParameterValidation() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<CodecMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        AudioTrackInfo track;
        track.codecType = "alac";
        track.sampleRate = 44100;
        track.channelCount = 2;
        track.bitsPerSample = 24; // Test 24-bit
        
        // Create ALAC config with 24-bit depth
        std::vector<uint8_t> alac24Config = {
            0x00, 0x00, 0x00, 0x24,
            'a', 'l', 'a', 'c',
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x10, 0x00, // Frame length
            0x18,                   // Bit depth (24)
            0x28, 0x0A, 0x0E,      // History parameters
            0x02,                   // Channels (2)
            0xFF, 0xFF,             // Max run
            0x00, 0x00, 0xAC, 0x44, // Sample rate
            0x00, 0x00, 0xAC, 0x44
        };
        alac24Config.resize(36);
        
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("alac", alac24Config, track), 
                   "24-bit ALAC configuration should be valid");
        
        // Test parameter mismatch
        track.bitsPerSample = 16; // Track says 16-bit but config says 24-bit
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("alac", alac24Config, track), 
                    "ALAC bit depth mismatch should fail");
    }
};

// Test class for telephony codec compliance (mulaw/alaw)
class TelephonyCodecComplianceTest : public TestCase {
public:
    TelephonyCodecComplianceTest() : TestCase("TelephonyCodecCompliance") {}
    
protected:
    void runTest() override {
        testValidTelephonyConfigurations();
        testInvalidTelephonyConfigurations();
        testTelephonySampleRateValidation();
        testTelephonyChannelValidation();
        testTelephonyBitDepthValidation();
    }
    
private:
    void testValidTelephonyConfigurations() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<CodecMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        // Test valid mulaw configuration
        AudioTrackInfo mulawTrack;
        mulawTrack.codecType = "ulaw";
        mulawTrack.sampleRate = 8000;
        mulawTrack.channelCount = 1;
        mulawTrack.bitsPerSample = 8;
        
        std::vector<uint8_t> emptyConfig; // Telephony codecs don't need config data
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("ulaw", emptyConfig, mulawTrack), 
                   "Valid mulaw configuration should pass");
        
        // Test valid alaw configuration
        AudioTrackInfo alawTrack;
        alawTrack.codecType = "alaw";
        alawTrack.sampleRate = 8000;
        alawTrack.channelCount = 1;
        alawTrack.bitsPerSample = 8;
        
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("alaw", emptyConfig, alawTrack), 
                   "Valid alaw configuration should pass");
        
        // Test 16kHz telephony (also valid)
        mulawTrack.sampleRate = 16000;
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("ulaw", emptyConfig, mulawTrack), 
                   "16kHz mulaw configuration should pass");
    }
    
    void testInvalidTelephonyConfigurations() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<CodecMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        std::vector<uint8_t> emptyConfig;
        
        // Test invalid bit depth
        AudioTrackInfo invalidBitDepth;
        invalidBitDepth.codecType = "ulaw";
        invalidBitDepth.sampleRate = 8000;
        invalidBitDepth.channelCount = 1;
        invalidBitDepth.bitsPerSample = 16; // Should be 8
        
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("ulaw", emptyConfig, invalidBitDepth), 
                    "Invalid mulaw bit depth should fail");
        
        // Test invalid sample rate
        AudioTrackInfo invalidSampleRate;
        invalidSampleRate.codecType = "ulaw";
        invalidSampleRate.sampleRate = 44100; // Should be 8000 or 16000
        invalidSampleRate.channelCount = 1;
        invalidSampleRate.bitsPerSample = 8;
        
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("ulaw", emptyConfig, invalidSampleRate), 
                    "Invalid mulaw sample rate should fail");
        
        // Test invalid channel count
        AudioTrackInfo invalidChannels;
        invalidChannels.codecType = "ulaw";
        invalidChannels.sampleRate = 8000;
        invalidChannels.channelCount = 2; // Should be 1
        invalidChannels.bitsPerSample = 8;
        
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("ulaw", emptyConfig, invalidChannels), 
                    "Invalid mulaw channel count should fail");
    }
    
    void testTelephonySampleRateValidation() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<CodecMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        AudioTrackInfo track;
        track.codecType = "ulaw";
        track.channelCount = 1;
        track.bitsPerSample = 8;
        
        std::vector<uint8_t> emptyConfig;
        
        // Test standard telephony rates
        std::vector<uint32_t> validRates = {8000, 16000};
        for (uint32_t rate : validRates) {
            track.sampleRate = rate;
            ASSERT_TRUE(validator.ValidateCodecDataIntegrity("ulaw", emptyConfig, track), 
                       "Telephony rate " + std::to_string(rate) + " should be valid");
        }
        
        // Test invalid rates
        std::vector<uint32_t> invalidRates = {4000, 11025, 22050, 44100, 48000};
        for (uint32_t rate : invalidRates) {
            track.sampleRate = rate;
            ASSERT_FALSE(validator.ValidateCodecDataIntegrity("ulaw", emptyConfig, track), 
                        "Non-telephony rate " + std::to_string(rate) + " should be invalid");
        }
    }
    
    void testTelephonyChannelValidation() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<CodecMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        AudioTrackInfo track;
        track.codecType = "alaw";
        track.sampleRate = 8000;
        track.bitsPerSample = 8;
        
        std::vector<uint8_t> emptyConfig;
        
        // Test valid channel count (mono only)
        track.channelCount = 1;
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("alaw", emptyConfig, track), 
                   "Mono telephony should be valid");
        
        // Test invalid channel counts
        std::vector<uint16_t> invalidChannels = {0, 2, 3, 4, 5, 6, 8};
        for (uint16_t channels : invalidChannels) {
            track.channelCount = channels;
            ASSERT_FALSE(validator.ValidateCodecDataIntegrity("alaw", emptyConfig, track), 
                        "Telephony with " + std::to_string(channels) + " channels should be invalid");
        }
    }
    
    void testTelephonyBitDepthValidation() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<CodecMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        AudioTrackInfo track;
        track.codecType = "ulaw";
        track.sampleRate = 8000;
        track.channelCount = 1;
        
        std::vector<uint8_t> emptyConfig;
        
        // Test valid bit depth (8-bit only)
        track.bitsPerSample = 8;
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("ulaw", emptyConfig, track), 
                   "8-bit telephony should be valid");
        
        // Test invalid bit depths
        std::vector<uint16_t> invalidBitDepths = {1, 4, 12, 16, 24, 32};
        for (uint16_t bitDepth : invalidBitDepths) {
            track.bitsPerSample = bitDepth;
            ASSERT_FALSE(validator.ValidateCodecDataIntegrity("ulaw", emptyConfig, track), 
                        "Telephony with " + std::to_string(bitDepth) + " bits should be invalid");
        }
    }
};

// Test class for PCM codec compliance
class PCMCodecComplianceTest : public TestCase {
public:
    PCMCodecComplianceTest() : TestCase("PCMCodecCompliance") {}
    
protected:
    void runTest() override {
        testValidPCMConfigurations();
        testInvalidPCMConfigurations();
        testPCMBitDepthValidation();
        testPCMSampleRateValidation();
        testPCMChannelValidation();
    }
    
private:
    void testValidPCMConfigurations() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<CodecMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        std::vector<uint8_t> emptyConfig; // PCM doesn't need config data
        
        // Test standard PCM configurations
        std::vector<std::tuple<uint32_t, uint16_t, uint16_t>> validConfigs = {
            {44100, 2, 16}, // CD quality
            {48000, 2, 16}, // DAT quality
            {96000, 2, 24}, // High-res audio
            {192000, 2, 24}, // Ultra high-res
            {44100, 1, 16}, // Mono CD
            {48000, 6, 24}, // 5.1 surround
            {48000, 8, 32}  // 7.1 surround, 32-bit
        };
        
        for (const auto& config : validConfigs) {
            AudioTrackInfo track;
            track.codecType = "lpcm";
            track.sampleRate = std::get<0>(config);
            track.channelCount = std::get<1>(config);
            track.bitsPerSample = std::get<2>(config);
            
            ASSERT_TRUE(validator.ValidateCodecDataIntegrity("lpcm", emptyConfig, track), 
                       "PCM " + std::to_string(track.sampleRate) + "Hz/" + 
                       std::to_string(track.bitsPerSample) + "bit/" + 
                       std::to_string(track.channelCount) + "ch should be valid");
        }
    }
    
    void testInvalidPCMConfigurations() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<CodecMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        std::vector<uint8_t> emptyConfig;
        
        // Test invalid bit depths
        std::vector<uint16_t> invalidBitDepths = {0, 1, 7, 9, 15, 17, 23, 25, 31, 33};
        for (uint16_t bitDepth : invalidBitDepths) {
            AudioTrackInfo track;
            track.codecType = "lpcm";
            track.sampleRate = 44100;
            track.channelCount = 2;
            track.bitsPerSample = bitDepth;
            
            ASSERT_FALSE(validator.ValidateCodecDataIntegrity("lpcm", emptyConfig, track), 
                        "PCM with " + std::to_string(bitDepth) + " bits should be invalid");
        }
        
        // Test invalid channel counts
        AudioTrackInfo track;
        track.codecType = "lpcm";
        track.sampleRate = 44100;
        track.bitsPerSample = 16;
        track.channelCount = 0;
        
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("lpcm", emptyConfig, track), 
                    "PCM with 0 channels should be invalid");
    }
    
    void testPCMBitDepthValidation() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<CodecMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        AudioTrackInfo track;
        track.codecType = "lpcm";
        track.sampleRate = 44100;
        track.channelCount = 2;
        
        std::vector<uint8_t> emptyConfig;
        
        // Test valid bit depths
        std::vector<uint16_t> validBitDepths = {8, 16, 24, 32};
        for (uint16_t bitDepth : validBitDepths) {
            track.bitsPerSample = bitDepth;
            ASSERT_TRUE(validator.ValidateCodecDataIntegrity("lpcm", emptyConfig, track), 
                       "PCM " + std::to_string(bitDepth) + "-bit should be valid");
        }
    }
    
    void testPCMSampleRateValidation() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<CodecMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        AudioTrackInfo track;
        track.codecType = "lpcm";
        track.channelCount = 2;
        track.bitsPerSample = 16;
        
        std::vector<uint8_t> emptyConfig;
        
        // Test standard sample rates
        std::vector<uint32_t> validRates = {
            8000, 11025, 16000, 22050, 32000, 44100, 48000, 
            88200, 96000, 176400, 192000, 352800, 384000
        };
        
        for (uint32_t rate : validRates) {
            track.sampleRate = rate;
            ASSERT_TRUE(validator.ValidateCodecDataIntegrity("lpcm", emptyConfig, track), 
                       "PCM " + std::to_string(rate) + "Hz should be valid");
        }
        
        // Test edge case rates
        track.sampleRate = 1; // Extremely low
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("lpcm", emptyConfig, track), 
                    "PCM 1Hz should be invalid");
        
        track.sampleRate = 1000000; // Extremely high
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("lpcm", emptyConfig, track), 
                    "PCM 1MHz should be invalid");
    }
    
    void testPCMChannelValidation() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<CodecMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        AudioTrackInfo track;
        track.codecType = "lpcm";
        track.sampleRate = 44100;
        track.bitsPerSample = 16;
        
        std::vector<uint8_t> emptyConfig;
        
        // Test valid channel counts
        std::vector<uint16_t> validChannels = {1, 2, 3, 4, 5, 6, 7, 8};
        for (uint16_t channels : validChannels) {
            track.channelCount = channels;
            ASSERT_TRUE(validator.ValidateCodecDataIntegrity("lpcm", emptyConfig, track), 
                       "PCM " + std::to_string(channels) + " channels should be valid");
        }
        
        // Test extreme channel counts
        track.channelCount = 32; // Very high but potentially valid
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("lpcm", emptyConfig, track), 
                   "PCM 32 channels should be valid");
        
        track.channelCount = 256; // Extremely high
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("lpcm", emptyConfig, track), 
                    "PCM 256 channels should be invalid");
    }
};

int main() {
    TestSuite suite("ISO Demuxer Codec-Specific Compliance Tests");
    
    // Add all codec-specific test classes
    suite.addTest(std::make_unique<AACCodecComplianceTest>());
    suite.addTest(std::make_unique<ALACCodecComplianceTest>());
    suite.addTest(std::make_unique<TelephonyCodecComplianceTest>());
    suite.addTest(std::make_unique<PCMCodecComplianceTest>());
    
    // Run all tests
    std::vector<TestCaseInfo> results = suite.runAll();
    
    // Print results
    suite.printResults(results);
    
    // Return appropriate exit code
    return suite.getFailureCount(results) == 0 ? 0 : 1;
}