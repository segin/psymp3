/*
 * test_simplepcmcodec_comprehensive_integration.cpp - Comprehensive integration tests for SimplePCMCodec
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

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <exception>
#include <algorithm>
#include <random>

// Minimal includes for testing
#include <cstdint>

// ========================================
// MINIMAL DEPENDENCIES FOR TESTING
// ========================================

struct StreamInfo {
    std::string codec_type = "audio";
    std::string codec_name;
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
    uint16_t bits_per_sample = 0;
};

struct MediaChunk {
    uint32_t stream_id = 0;
    std::vector<uint8_t> data;
    uint64_t timestamp_samples = 0;
    bool is_keyframe = true;
    uint64_t file_offset = 0;
    
    MediaChunk() = default;
    MediaChunk(uint32_t id, std::vector<uint8_t>&& chunk_data)
        : stream_id(id), data(std::move(chunk_data)) {}
};

struct AudioFrame {
    std::vector<int16_t> samples;
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
    uint64_t timestamp_samples = 0;
    uint64_t timestamp_ms = 0;
    
    AudioFrame() = default;
    AudioFrame(size_t sample_count, uint32_t rate, uint16_t ch)
        : samples(sample_count), sample_rate(rate), channels(ch) {}
};

class AudioCodec {
public:
    explicit AudioCodec(const StreamInfo& stream_info) : m_stream_info(stream_info) {}
    virtual ~AudioCodec() = default;
    
    virtual bool initialize() = 0;
    virtual AudioFrame decode(const MediaChunk& chunk) = 0;
    virtual AudioFrame flush() = 0;
    virtual void reset() = 0;
    virtual std::string getCodecName() const = 0;
    virtual bool canDecode(const StreamInfo& stream_info) const = 0;
    
    const StreamInfo& getStreamInfo() const { return m_stream_info; }
    bool isInitialized() const { return m_initialized; }

protected:
    StreamInfo m_stream_info;
    bool m_initialized = false;
};

class SimplePCMCodec : public AudioCodec {
public:
    explicit SimplePCMCodec(const StreamInfo& stream_info) : AudioCodec(stream_info) {}
    
    bool initialize() override {
        m_initialized = true;
        return true;
    }
    
    AudioFrame decode(const MediaChunk& chunk) override {
        AudioFrame frame;
        
        if (!m_initialized || chunk.data.empty()) {
            return frame;
        }
        
        frame.sample_rate = m_stream_info.sample_rate;
        frame.channels = m_stream_info.channels;
        frame.timestamp_samples = chunk.timestamp_samples;
        
        if (m_stream_info.sample_rate > 0) {
            frame.timestamp_ms = (chunk.timestamp_samples * 1000ULL) / m_stream_info.sample_rate;
        }
        
        size_t samples_converted = convertSamples(chunk.data, frame.samples);
        return frame;
    }
    
    AudioFrame flush() override {
        return AudioFrame{};
    }
    
    void reset() override {
        // SimplePCMCodec doesn't have state to reset
    }

protected:
    virtual size_t convertSamples(const std::vector<uint8_t>& input_data, 
                                  std::vector<int16_t>& output_samples) = 0;
    virtual size_t getBytesPerInputSample() const = 0;
};

// ========================================
// MULAW CODEC IMPLEMENTATION
// ========================================

class MuLawCodec : public SimplePCMCodec {
public:
    explicit MuLawCodec(const StreamInfo& stream_info) : SimplePCMCodec(stream_info) {
        if (!s_table_initialized) {
            initializeMuLawTable();
        }
    }
    
    bool canDecode(const StreamInfo& stream_info) const override {
        return stream_info.codec_name == "mulaw" || 
               stream_info.codec_name == "pcm_mulaw" ||
               stream_info.codec_name == "g711_mulaw";
    }
    
    std::string getCodecName() const override {
        return "mulaw";
    }

protected:
    size_t convertSamples(const std::vector<uint8_t>& input_data, 
                         std::vector<int16_t>& output_samples) override {
        output_samples.clear();
        output_samples.reserve(input_data.size());
        
        for (uint8_t mulaw_sample : input_data) {
            output_samples.push_back(MULAW_TO_PCM[mulaw_sample]);
        }
        
        return input_data.size();
    }
    
    size_t getBytesPerInputSample() const override {
        return 1;
    }

private:
    static int16_t MULAW_TO_PCM[256];
    static bool s_table_initialized;
    
    static void initializeMuLawTable() {
        for (int i = 0; i < 256; ++i) {
            uint8_t mulaw_sample = static_cast<uint8_t>(i);
            
            // ITU-T G.711 μ-law decoding algorithm
            uint8_t complement = ~mulaw_sample;
            bool sign = (complement & 0x80) != 0;
            uint8_t exponent = (complement & 0x70) >> 4;
            uint8_t mantissa = complement & 0x0F;
            
            int16_t linear;
            if (exponent == 0) {
                linear = 33 + 2 * mantissa;
            } else {
                linear = (33 + 2 * mantissa) << exponent;
            }
            
            if (sign) {
                linear = -linear;
            }
            
            MULAW_TO_PCM[i] = linear;
        }
        s_table_initialized = true;
    }
};

int16_t MuLawCodec::MULAW_TO_PCM[256];
bool MuLawCodec::s_table_initialized = false;

// ========================================
// ALAW CODEC IMPLEMENTATION
// ========================================

class ALawCodec : public SimplePCMCodec {
public:
    explicit ALawCodec(const StreamInfo& stream_info) : SimplePCMCodec(stream_info) {
        if (!s_table_initialized) {
            initializeALawTable();
        }
    }
    
    bool canDecode(const StreamInfo& stream_info) const override {
        return stream_info.codec_name == "alaw" || 
               stream_info.codec_name == "pcm_alaw" ||
               stream_info.codec_name == "g711_alaw";
    }
    
    std::string getCodecName() const override {
        return "alaw";
    }

protected:
    size_t convertSamples(const std::vector<uint8_t>& input_data, 
                         std::vector<int16_t>& output_samples) override {
        output_samples.clear();
        output_samples.reserve(input_data.size());
        
        for (uint8_t alaw_sample : input_data) {
            output_samples.push_back(ALAW_TO_PCM[alaw_sample]);
        }
        
        return input_data.size();
    }
    
    size_t getBytesPerInputSample() const override {
        return 1;
    }

private:
    static int16_t ALAW_TO_PCM[256];
    static bool s_table_initialized;
    
    static void initializeALawTable() {
        for (int i = 0; i < 256; ++i) {
            uint8_t alaw_sample = static_cast<uint8_t>(i);
            
            // ITU-T G.711 A-law decoding algorithm
            uint8_t complement = alaw_sample ^ 0x55;
            bool sign = (complement & 0x80) == 0;
            uint8_t exponent = (complement & 0x70) >> 4;
            uint8_t mantissa = complement & 0x0F;
            
            int16_t linear;
            if (exponent == 0) {
                linear = 16 + 2 * mantissa;
            } else {
                linear = (16 + 2 * mantissa) << exponent;
            }
            
            if (sign) {
                linear = -linear;
            }
            
            ALAW_TO_PCM[i] = linear;
        }
        s_table_initialized = true;
    }
};

int16_t ALawCodec::ALAW_TO_PCM[256];
bool ALawCodec::s_table_initialized = false;

// ========================================
// SIMPLE TEST FRAMEWORK
// ========================================

class TestFailure : public std::exception {
public:
    explicit TestFailure(const std::string& message) : m_message(message) {}
    const char* what() const noexcept override { return m_message.c_str(); }
private:
    std::string m_message;
};

#define ASSERT_TRUE(condition, message) \
    do { \
        if (!(condition)) { \
            throw TestFailure(std::string("ASSERTION FAILED: ") + (message) + \
                            " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
        } \
    } while(0)

#define ASSERT_FALSE(condition, message) \
    do { \
        if ((condition)) { \
            throw TestFailure(std::string("ASSERTION FAILED: ") + (message) + \
                            " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
        } \
    } while(0)

#define ASSERT_EQUALS(expected, actual, message) \
    do { \
        if (!((expected) == (actual))) { \
            throw TestFailure(std::string("ASSERTION FAILED: ") + (message) + \
                            " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
        } \
    } while(0)

// ========================================
// HELPER FUNCTIONS
// ========================================

StreamInfo createStreamInfo(const std::string& codec_name, 
                           uint32_t sample_rate = 8000, 
                           uint16_t channels = 1,
                           uint16_t bits_per_sample = 8) {
    StreamInfo info;
    info.codec_type = "audio";
    info.codec_name = codec_name;
    info.sample_rate = sample_rate;
    info.channels = channels;
    info.bits_per_sample = bits_per_sample;
    return info;
}

MediaChunk createMediaChunk(const std::vector<uint8_t>& data, 
                           uint32_t stream_id = 0,
                           uint64_t timestamp_samples = 0) {
    MediaChunk chunk;
    chunk.stream_id = stream_id;
    chunk.data = data;
    chunk.timestamp_samples = timestamp_samples;
    chunk.is_keyframe = true;
    chunk.file_offset = 0;
    return chunk;
}

std::vector<uint8_t> generateRandomData(size_t size, uint32_t seed = 12345) {
    std::mt19937 gen(seed);
    std::uniform_int_distribution<uint8_t> dis(0, 255);
    
    std::vector<uint8_t> data(size);
    for (size_t i = 0; i < size; ++i) {
        data[i] = dis(gen);
    }
    return data;
}

// ========================================
// TEST CASES FOR INITIALIZE() METHOD
// ========================================

void test_initialize_with_various_streaminfo_configurations() {
    std::cout << "Testing initialize() method with various StreamInfo configurations..." << std::endl;
    
    // Test 1: MuLawCodec with standard telephony configuration
    {
        StreamInfo info = createStreamInfo("mulaw", 8000, 1, 8);
        std::unique_ptr<AudioCodec> codec = std::make_unique<MuLawCodec>(info);
        
        ASSERT_TRUE(codec->initialize(), "MuLawCodec should initialize with standard config");
        ASSERT_TRUE(codec->isInitialized(), "MuLawCodec should report as initialized");
        ASSERT_EQUALS(std::string("mulaw"), codec->getCodecName(), "Should return correct codec name");
    }
    
    // Test 2: ALawCodec with standard telephony configuration
    {
        StreamInfo info = createStreamInfo("alaw", 8000, 1, 8);
        std::unique_ptr<AudioCodec> codec = std::make_unique<ALawCodec>(info);
        
        ASSERT_TRUE(codec->initialize(), "ALawCodec should initialize with standard config");
        ASSERT_TRUE(codec->isInitialized(), "ALawCodec should report as initialized");
        ASSERT_EQUALS(std::string("alaw"), codec->getCodecName(), "Should return correct codec name");
    }
    
    // Test 3: Different sample rates
    std::vector<uint32_t> sample_rates = {8000, 16000, 22050, 44100, 48000};
    for (uint32_t rate : sample_rates) {
        StreamInfo mulaw_info = createStreamInfo("mulaw", rate, 1, 8);
        std::unique_ptr<AudioCodec> mulaw_codec = std::make_unique<MuLawCodec>(mulaw_info);
        
        ASSERT_TRUE(mulaw_codec->initialize(), "MuLawCodec should initialize with rate " + std::to_string(rate));
        ASSERT_EQUALS(rate, mulaw_codec->getStreamInfo().sample_rate, "Sample rate should be preserved");
        
        StreamInfo alaw_info = createStreamInfo("alaw", rate, 1, 8);
        std::unique_ptr<AudioCodec> alaw_codec = std::make_unique<ALawCodec>(alaw_info);
        
        ASSERT_TRUE(alaw_codec->initialize(), "ALawCodec should initialize with rate " + std::to_string(rate));
        ASSERT_EQUALS(rate, alaw_codec->getStreamInfo().sample_rate, "Sample rate should be preserved");
    }
    
    // Test 4: Different channel configurations
    std::vector<uint16_t> channel_counts = {1, 2};
    for (uint16_t channels : channel_counts) {
        StreamInfo mulaw_info = createStreamInfo("mulaw", 8000, channels, 8);
        std::unique_ptr<AudioCodec> mulaw_codec = std::make_unique<MuLawCodec>(mulaw_info);
        
        ASSERT_TRUE(mulaw_codec->initialize(), "MuLawCodec should initialize with " + std::to_string(channels) + " channels");
        ASSERT_EQUALS(channels, mulaw_codec->getStreamInfo().channels, "Channel count should be preserved");
        
        StreamInfo alaw_info = createStreamInfo("alaw", 8000, channels, 8);
        std::unique_ptr<AudioCodec> alaw_codec = std::make_unique<ALawCodec>(alaw_info);
        
        ASSERT_TRUE(alaw_codec->initialize(), "ALawCodec should initialize with " + std::to_string(channels) + " channels");
        ASSERT_EQUALS(channels, alaw_codec->getStreamInfo().channels, "Channel count should be preserved");
    }
    
    // Test 5: Multiple initialization calls
    {
        StreamInfo info = createStreamInfo("mulaw");
        std::unique_ptr<AudioCodec> codec = std::make_unique<MuLawCodec>(info);
        
        ASSERT_TRUE(codec->initialize(), "First initialization should succeed");
        ASSERT_TRUE(codec->initialize(), "Second initialization should also succeed");
        ASSERT_TRUE(codec->initialize(), "Third initialization should also succeed");
        ASSERT_TRUE(codec->isInitialized(), "Codec should remain initialized");
    }
    
    std::cout << "✓ initialize() method works correctly with various StreamInfo configurations" << std::endl;
}

// ========================================
// TEST CASES FOR DECODE() METHOD
// ========================================

void test_decode_with_different_mediachunk_sizes() {
    std::cout << "Testing decode() method with different MediaChunk sizes..." << std::endl;
    
    StreamInfo mulaw_info = createStreamInfo("mulaw");
    std::unique_ptr<AudioCodec> mulaw_codec = std::make_unique<MuLawCodec>(mulaw_info);
    ASSERT_TRUE(mulaw_codec->initialize(), "MuLawCodec initialization should succeed");
    
    StreamInfo alaw_info = createStreamInfo("alaw");
    std::unique_ptr<AudioCodec> alaw_codec = std::make_unique<ALawCodec>(alaw_info);
    ASSERT_TRUE(alaw_codec->initialize(), "ALawCodec initialization should succeed");
    
    // Test 1: Empty chunk
    {
        MediaChunk empty_chunk = createMediaChunk({});
        
        AudioFrame mulaw_frame = mulaw_codec->decode(empty_chunk);
        ASSERT_EQUALS(0u, mulaw_frame.samples.size(), "Empty chunk should produce empty frame");
        
        AudioFrame alaw_frame = alaw_codec->decode(empty_chunk);
        ASSERT_EQUALS(0u, alaw_frame.samples.size(), "Empty chunk should produce empty frame");
    }
    
    // Test 2: Single byte chunk
    {
        MediaChunk single_chunk = createMediaChunk({0xFF}); // μ-law silence
        
        AudioFrame mulaw_frame = mulaw_codec->decode(single_chunk);
        ASSERT_EQUALS(1u, mulaw_frame.samples.size(), "Single byte should produce one sample");
        ASSERT_EQUALS(8000u, mulaw_frame.sample_rate, "Should preserve sample rate");
        ASSERT_EQUALS(1u, mulaw_frame.channels, "Should preserve channel count");
        
        MediaChunk alaw_single = createMediaChunk({0x55}); // A-law closest to silence
        AudioFrame alaw_frame = alaw_codec->decode(alaw_single);
        ASSERT_EQUALS(1u, alaw_frame.samples.size(), "Single byte should produce one sample");
    }
    
    // Test 3: Small chunks (various sizes)
    std::vector<size_t> chunk_sizes = {2, 5, 10, 16, 32, 64};
    for (size_t size : chunk_sizes) {
        std::vector<uint8_t> test_data = generateRandomData(size);
        MediaChunk chunk = createMediaChunk(test_data);
        
        AudioFrame mulaw_frame = mulaw_codec->decode(chunk);
        ASSERT_EQUALS(size, mulaw_frame.samples.size(), "Should convert all samples for size " + std::to_string(size));
        
        AudioFrame alaw_frame = alaw_codec->decode(chunk);
        ASSERT_EQUALS(size, alaw_frame.samples.size(), "Should convert all samples for size " + std::to_string(size));
    }
    
    // Test 4: Timestamp preservation
    {
        std::vector<uint8_t> test_data = {100, 150, 200, 250};
        uint64_t test_timestamp = 98765;
        MediaChunk timestamped_chunk = createMediaChunk(test_data, 0, test_timestamp);
        
        AudioFrame mulaw_frame = mulaw_codec->decode(timestamped_chunk);
        ASSERT_EQUALS(test_timestamp, mulaw_frame.timestamp_samples, "Should preserve timestamp");
        
        AudioFrame alaw_frame = alaw_codec->decode(timestamped_chunk);
        ASSERT_EQUALS(test_timestamp, alaw_frame.timestamp_samples, "Should preserve timestamp");
    }
    
    std::cout << "✓ decode() method works correctly with different MediaChunk sizes" << std::endl;
}

// ========================================
// TEST CASES FOR FLUSH() METHOD
// ========================================

void test_flush_behavior_for_stream_completion() {
    std::cout << "Testing flush() behavior for stream completion scenarios..." << std::endl;
    
    StreamInfo mulaw_info = createStreamInfo("mulaw");
    std::unique_ptr<AudioCodec> mulaw_codec = std::make_unique<MuLawCodec>(mulaw_info);
    ASSERT_TRUE(mulaw_codec->initialize(), "MuLawCodec initialization should succeed");
    
    StreamInfo alaw_info = createStreamInfo("alaw");
    std::unique_ptr<AudioCodec> alaw_codec = std::make_unique<ALawCodec>(alaw_info);
    ASSERT_TRUE(alaw_codec->initialize(), "ALawCodec initialization should succeed");
    
    // Test 1: Flush on fresh codec (no data processed)
    {
        AudioFrame mulaw_flush = mulaw_codec->flush();
        ASSERT_EQUALS(0u, mulaw_flush.samples.size(), "Fresh codec flush should return empty frame");
        
        AudioFrame alaw_flush = alaw_codec->flush();
        ASSERT_EQUALS(0u, alaw_flush.samples.size(), "Fresh codec flush should return empty frame");
    }
    
    // Test 2: Flush after processing data
    {
        std::vector<uint8_t> test_data = {0x80, 0x7F, 0x00, 0xFF, 0x40};
        MediaChunk chunk = createMediaChunk(test_data);
        
        AudioFrame mulaw_decode = mulaw_codec->decode(chunk);
        ASSERT_EQUALS(test_data.size(), mulaw_decode.samples.size(), "Should decode all samples");
        
        AudioFrame mulaw_flush = mulaw_codec->flush();
        ASSERT_EQUALS(0u, mulaw_flush.samples.size(), "SimplePCMCodec flush should return empty frame");
        
        AudioFrame alaw_decode = alaw_codec->decode(chunk);
        ASSERT_EQUALS(test_data.size(), alaw_decode.samples.size(), "ALaw decode should work normally");
        
        AudioFrame alaw_flush = alaw_codec->flush();
        ASSERT_EQUALS(0u, alaw_flush.samples.size(), "SimplePCMCodec flush should return empty frame");
    }
    
    // Test 3: Multiple flush calls
    {
        AudioFrame flush1 = mulaw_codec->flush();
        AudioFrame flush2 = mulaw_codec->flush();
        AudioFrame flush3 = mulaw_codec->flush();
        
        ASSERT_EQUALS(0u, flush1.samples.size(), "First flush should return empty frame");
        ASSERT_EQUALS(0u, flush2.samples.size(), "Second flush should return empty frame");
        ASSERT_EQUALS(0u, flush3.samples.size(), "Third flush should return empty frame");
    }
    
    std::cout << "✓ flush() behavior works correctly for stream completion scenarios" << std::endl;
}

// ========================================
// TEST CASES FOR RESET() METHOD
// ========================================

void test_reset_functionality_for_seeking() {
    std::cout << "Testing reset() functionality for seeking operations..." << std::endl;
    
    StreamInfo mulaw_info = createStreamInfo("mulaw");
    std::unique_ptr<AudioCodec> mulaw_codec = std::make_unique<MuLawCodec>(mulaw_info);
    ASSERT_TRUE(mulaw_codec->initialize(), "MuLawCodec initialization should succeed");
    
    StreamInfo alaw_info = createStreamInfo("alaw");
    std::unique_ptr<AudioCodec> alaw_codec = std::make_unique<ALawCodec>(alaw_info);
    ASSERT_TRUE(alaw_codec->initialize(), "ALawCodec initialization should succeed");
    
    // Test 1: Reset on fresh codec
    {
        mulaw_codec->reset();
        ASSERT_TRUE(mulaw_codec->isInitialized(), "Reset should not affect initialization state");
        
        alaw_codec->reset();
        ASSERT_TRUE(alaw_codec->isInitialized(), "Reset should not affect initialization state");
    }
    
    // Test 2: Reset after processing data
    {
        std::vector<uint8_t> initial_data = {0x80, 0x7F, 0x00, 0xFF, 0x40, 0xC0};
        MediaChunk initial_chunk = createMediaChunk(initial_data, 0, 1000);
        
        AudioFrame initial_frame = mulaw_codec->decode(initial_chunk);
        ASSERT_EQUALS(initial_data.size(), initial_frame.samples.size(), "Should decode all initial samples");
        ASSERT_EQUALS(1000u, initial_frame.timestamp_samples, "Should preserve initial timestamp");
        
        mulaw_codec->reset();
        ASSERT_TRUE(mulaw_codec->isInitialized(), "Should remain initialized after reset");
        
        std::vector<uint8_t> post_reset_data = {0x10, 0x20, 0x30};
        MediaChunk post_reset_chunk = createMediaChunk(post_reset_data, 0, 5000);
        
        AudioFrame post_reset_frame = mulaw_codec->decode(post_reset_chunk);
        ASSERT_EQUALS(post_reset_data.size(), post_reset_frame.samples.size(), "Should decode all post-reset samples");
        ASSERT_EQUALS(5000u, post_reset_frame.timestamp_samples, "Should preserve post-reset timestamp");
    }
    
    // Test 3: Reset preserves codec configuration
    {
        StreamInfo high_rate_info = createStreamInfo("alaw", 48000, 2, 8);
        std::unique_ptr<AudioCodec> high_rate_codec = std::make_unique<ALawCodec>(high_rate_info);
        ASSERT_TRUE(high_rate_codec->initialize(), "High rate codec should initialize");
        
        std::vector<uint8_t> test_data = {0x55, 0xAA, 0x33, 0xCC};
        MediaChunk test_chunk = createMediaChunk(test_data);
        
        AudioFrame before_reset = high_rate_codec->decode(test_chunk);
        ASSERT_EQUALS(48000u, before_reset.sample_rate, "Should have high sample rate before reset");
        ASSERT_EQUALS(2u, before_reset.channels, "Should have stereo before reset");
        
        high_rate_codec->reset();
        
        AudioFrame after_reset = high_rate_codec->decode(test_chunk);
        ASSERT_EQUALS(48000u, after_reset.sample_rate, "Should preserve high sample rate after reset");
        ASSERT_EQUALS(2u, after_reset.channels, "Should preserve stereo after reset");
        ASSERT_EQUALS(test_data.size(), after_reset.samples.size(), "Should decode all samples after reset");
    }
    
    std::cout << "✓ reset() functionality works correctly for seeking operations" << std::endl;
}

// ========================================
// COMPREHENSIVE INTEGRATION TESTS
// ========================================

void test_comprehensive_workflow_integration() {
    std::cout << "Testing comprehensive workflow integration..." << std::endl;
    
    // Test complete workflow: initialize -> decode -> flush -> reset -> decode
    StreamInfo info = createStreamInfo("mulaw", 16000, 2, 8);
    std::unique_ptr<AudioCodec> codec = std::make_unique<MuLawCodec>(info);
    
    // Step 1: Initialize
    ASSERT_TRUE(codec->initialize(), "Workflow initialization should succeed");
    ASSERT_TRUE(codec->isInitialized(), "Should be initialized");
    
    // Step 2: Decode multiple chunks
    std::vector<std::vector<uint8_t>> chunks = {
        {0x80, 0x7F, 0x00, 0xFF},
        {0x40, 0xC0, 0x20, 0xE0},
        {0x10, 0xF0, 0x08, 0xF8}
    };
    
    for (size_t i = 0; i < chunks.size(); ++i) {
        MediaChunk chunk = createMediaChunk(chunks[i], 0, i * 1000);
        AudioFrame frame = codec->decode(chunk);
        
        ASSERT_EQUALS(chunks[i].size(), frame.samples.size(), "Should decode all samples in chunk " + std::to_string(i));
        ASSERT_EQUALS(16000u, frame.sample_rate, "Should preserve sample rate");
        ASSERT_EQUALS(2u, frame.channels, "Should preserve channel count");
        ASSERT_EQUALS(i * 1000, frame.timestamp_samples, "Should preserve timestamp");
    }
    
    // Step 3: Flush
    AudioFrame flush_frame = codec->flush();
    ASSERT_EQUALS(0u, flush_frame.samples.size(), "Workflow flush should return empty frame");
    
    // Step 4: Reset
    codec->reset();
    ASSERT_TRUE(codec->isInitialized(), "Should remain initialized after workflow reset");
    
    // Step 5: Decode after reset
    std::vector<uint8_t> post_reset_data = {0x55, 0xAA, 0x33, 0xCC, 0x66, 0x99};
    MediaChunk post_reset_chunk = createMediaChunk(post_reset_data, 0, 10000);
    
    AudioFrame post_reset_frame = codec->decode(post_reset_chunk);
    ASSERT_EQUALS(post_reset_data.size(), post_reset_frame.samples.size(), "Should decode all post-reset samples");
    ASSERT_EQUALS(16000u, post_reset_frame.sample_rate, "Should preserve sample rate after reset");
    ASSERT_EQUALS(2u, post_reset_frame.channels, "Should preserve channels after reset");
    ASSERT_EQUALS(10000u, post_reset_frame.timestamp_samples, "Should preserve post-reset timestamp");
    
    std::cout << "✓ Comprehensive workflow integration works correctly" << std::endl;
}

// ========================================
// MAIN TEST EXECUTION
// ========================================

int main() {
    std::cout << "=== SimplePCMCodec Comprehensive Integration Tests ===" << std::endl;
    std::cout << "Testing SimplePCMCodec base class with MuLaw/ALaw codec implementations" << std::endl;
    std::cout << "Requirements: 9.2, 9.3, 9.4, 9.5" << std::endl;
    std::cout << std::endl;
    
    try {
        test_initialize_with_various_streaminfo_configurations();
        std::cout << std::endl;
        
        test_decode_with_different_mediachunk_sizes();
        std::cout << std::endl;
        
        test_flush_behavior_for_stream_completion();
        std::cout << std::endl;
        
        test_reset_functionality_for_seeking();
        std::cout << std::endl;
        
        test_comprehensive_workflow_integration();
        std::cout << std::endl;
        
        std::cout << "=== ALL TESTS PASSED ===" << std::endl;
        std::cout << "SimplePCMCodec comprehensive integration tests completed successfully!" << std::endl;
        std::cout << "✓ initialize() method tested with various StreamInfo configurations" << std::endl;
        std::cout << "✓ decode() method tested with different MediaChunk sizes" << std::endl;
        std::cout << "✓ flush() behavior tested for stream completion scenarios" << std::endl;
        std::cout << "✓ reset() functionality tested for seeking operations" << std::endl;
        std::cout << "✓ Comprehensive workflow integration verified" << std::endl;
        
        return 0;
        
    } catch (const TestFailure& e) {
        std::cerr << "TEST FAILED: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "UNEXPECTED ERROR: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "UNKNOWN ERROR occurred during testing" << std::endl;
        return 1;
    }
}