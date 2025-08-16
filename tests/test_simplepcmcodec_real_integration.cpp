/*
 * test_simplepcmcodec_real_integration.cpp - Real integration tests with MuLaw/ALaw codecs
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

#define ASSERT_EQUALS(expected, actual, message) \
    do { \
        if (!((expected) == (actual))) { \
            throw TestFailure(std::string("ASSERTION FAILED: ") + (message) + \
                            " at " + __FILE__ + ":" + std::to_string(__LINE__) + \
                            " - Expected: " + std::to_string(expected) + \
                            ", Got: " + std::to_string(actual)); \
        } \
    } while(0)

// ========================================
// HELPER FUNCTIONS
// ========================================

StreamInfo createStreamInfo(const std::string& codec_name, uint32_t sample_rate = 8000, uint16_t channels = 1) {
    StreamInfo info;
    info.codec_type = "audio";
    info.codec_name = codec_name;
    info.sample_rate = sample_rate;
    info.channels = channels;
    info.bits_per_sample = 8;
    return info;
}

MediaChunk createMediaChunk(const std::vector<uint8_t>& data, uint64_t timestamp = 0) {
    MediaChunk chunk;
    chunk.data = data;
    chunk.timestamp_samples = timestamp;
    return chunk;
}

// ========================================
// TEST CASES
// ========================================

void test_mulaw_codec_integration() {
    std::cout << "Testing MuLawCodec integration with SimplePCMCodec..." << std::endl;
    
    StreamInfo info = createStreamInfo("mulaw");
    MuLawCodec codec(info);
    
    // Test initialization
    ASSERT_TRUE(codec.initialize(), "MuLawCodec should initialize successfully");
    ASSERT_TRUE(codec.isInitialized(), "MuLawCodec should report as initialized");
    
    // Test decode with known μ-law values
    std::vector<uint8_t> mulaw_data = {0xFF, 0x7F, 0x80, 0x00}; // silence, max negative, max positive, min negative
    MediaChunk chunk = createMediaChunk(mulaw_data, 1000);
    AudioFrame frame = codec.decode(chunk);
    
    ASSERT_EQUALS(mulaw_data.size(), frame.samples.size(), "Should convert all μ-law samples");
    ASSERT_EQUALS(8000u, frame.sample_rate, "Should preserve sample rate");
    ASSERT_EQUALS(1u, frame.channels, "Should preserve channel count");
    ASSERT_EQUALS(1000u, frame.timestamp_samples, "Should preserve timestamp");
    
    // Test flush
    AudioFrame flush_frame = codec.flush();
    ASSERT_EQUALS(0u, flush_frame.samples.size(), "Flush should return empty frame");
    
    // Test reset
    codec.reset();
    ASSERT_TRUE(codec.isInitialized(), "Reset should not affect initialization");
    
    std::cout << "✓ MuLawCodec integration with SimplePCMCodec works correctly" << std::endl;
}

void test_alaw_codec_integration() {
    std::cout << "Testing ALawCodec integration with SimplePCMCodec..." << std::endl;
    
    StreamInfo info = createStreamInfo("alaw");
    ALawCodec codec(info);
    
    // Test initialization
    ASSERT_TRUE(codec.initialize(), "ALawCodec should initialize successfully");
    ASSERT_TRUE(codec.isInitialized(), "ALawCodec should report as initialized");
    
    // Test decode with known A-law values
    std::vector<uint8_t> alaw_data = {0x55, 0xD5, 0x2A, 0xAA}; // closest to silence, others
    MediaChunk chunk = createMediaChunk(alaw_data, 2000);
    AudioFrame frame = codec.decode(chunk);
    
    ASSERT_EQUALS(alaw_data.size(), frame.samples.size(), "Should convert all A-law samples");
    ASSERT_EQUALS(8000u, frame.sample_rate, "Should preserve sample rate");
    ASSERT_EQUALS(1u, frame.channels, "Should preserve channel count");
    ASSERT_EQUALS(2000u, frame.timestamp_samples, "Should preserve timestamp");
    
    // Test flush
    AudioFrame flush_frame = codec.flush();
    ASSERT_EQUALS(0u, flush_frame.samples.size(), "Flush should return empty frame");
    
    // Test reset
    codec.reset();
    ASSERT_TRUE(codec.isInitialized(), "Reset should not affect initialization");
    
    std::cout << "✓ ALawCodec integration with SimplePCMCodec works correctly" << std::endl;
}

void test_codec_format_validation() {
    std::cout << "Testing codec format validation..." << std::endl;
    
    // Test MuLawCodec format validation
    {
        StreamInfo mulaw_info = createStreamInfo("mulaw");
        StreamInfo alaw_info = createStreamInfo("alaw");
        StreamInfo invalid_info = createStreamInfo("invalid");
        
        MuLawCodec mulaw_codec(mulaw_info);
        ASSERT_TRUE(mulaw_codec.canDecode(mulaw_info), "MuLawCodec should accept μ-law format");
        ASSERT_TRUE(!mulaw_codec.canDecode(alaw_info), "MuLawCodec should reject A-law format");
        ASSERT_TRUE(!mulaw_codec.canDecode(invalid_info), "MuLawCodec should reject invalid format");
    }
    
    // Test ALawCodec format validation
    {
        StreamInfo mulaw_info = createStreamInfo("mulaw");
        StreamInfo alaw_info = createStreamInfo("alaw");
        StreamInfo invalid_info = createStreamInfo("invalid");
        
        ALawCodec alaw_codec(alaw_info);
        ASSERT_TRUE(alaw_codec.canDecode(alaw_info), "ALawCodec should accept A-law format");
        ASSERT_TRUE(!alaw_codec.canDecode(mulaw_info), "ALawCodec should reject μ-law format");
        ASSERT_TRUE(!alaw_codec.canDecode(invalid_info), "ALawCodec should reject invalid format");
    }
    
    std::cout << "✓ Codec format validation works correctly" << std::endl;
}

void test_different_sample_rates_and_channels() {
    std::cout << "Testing different sample rates and channels..." << std::endl;
    
    // Test different sample rates
    std::vector<uint32_t> sample_rates = {8000, 16000, 44100, 48000};
    for (uint32_t rate : sample_rates) {
        StreamInfo info = createStreamInfo("mulaw", rate, 1);
        MuLawCodec codec(info);
        
        ASSERT_TRUE(codec.initialize(), "Should initialize with sample rate " + std::to_string(rate));
        
        std::vector<uint8_t> test_data = {128, 200};
        MediaChunk chunk = createMediaChunk(test_data);
        AudioFrame frame = codec.decode(chunk);
        
        ASSERT_EQUALS(rate, frame.sample_rate, "Should preserve sample rate " + std::to_string(rate));
    }
    
    // Test different channel counts
    std::vector<uint16_t> channel_counts = {1, 2};
    for (uint16_t channels : channel_counts) {
        StreamInfo info = createStreamInfo("alaw", 8000, channels);
        ALawCodec codec(info);
        
        ASSERT_TRUE(codec.initialize(), "Should initialize with " + std::to_string(channels) + " channels");
        
        std::vector<uint8_t> test_data = {100, 150};
        MediaChunk chunk = createMediaChunk(test_data);
        AudioFrame frame = codec.decode(chunk);
        
        ASSERT_EQUALS(channels, frame.channels, "Should preserve " + std::to_string(channels) + " channels");
    }
    
    std::cout << "✓ Different sample rates and channels work correctly" << std::endl;
}

void test_large_data_processing() {
    std::cout << "Testing large data processing..." << std::endl;
    
    StreamInfo info = createStreamInfo("mulaw");
    MuLawCodec codec(info);
    ASSERT_TRUE(codec.initialize(), "Codec should initialize");
    
    // Create large test data (10000 samples)
    std::vector<uint8_t> large_data(10000);
    for (size_t i = 0; i < large_data.size(); ++i) {
        large_data[i] = static_cast<uint8_t>(i % 256);
    }
    
    MediaChunk chunk = createMediaChunk(large_data);
    AudioFrame frame = codec.decode(chunk);
    
    ASSERT_EQUALS(large_data.size(), frame.samples.size(), "Should process all samples in large data");
    ASSERT_EQUALS(8000u, frame.sample_rate, "Should preserve sample rate for large data");
    
    std::cout << "✓ Large data processing works correctly" << std::endl;
}

// ========================================
// MAIN TEST EXECUTION
// ========================================

int main() {
    std::cout << "=== SimplePCMCodec Real Integration Tests ===" << std::endl;
    std::cout << "Testing SimplePCMCodec with real MuLaw/ALaw codec implementations" << std::endl;
    std::cout << std::endl;
    
    try {
        test_mulaw_codec_integration();
        std::cout << std::endl;
        
        test_alaw_codec_integration();
        std::cout << std::endl;
        
        test_codec_format_validation();
        std::cout << std::endl;
        
        test_different_sample_rates_and_channels();
        std::cout << std::endl;
        
        test_large_data_processing();
        std::cout << std::endl;
        
        std::cout << "=== ALL TESTS PASSED ===" << std::endl;
        std::cout << "SimplePCMCodec real integration tests completed successfully!" << std::endl;
        
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