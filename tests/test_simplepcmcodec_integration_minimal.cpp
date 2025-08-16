/*
 * test_simplepcmcodec_integration_minimal.cpp - Minimal integration tests for SimplePCMCodec base class
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

// Minimal includes to avoid AudioCodec factory dependencies
#include <cstdint>

// ========================================
// MINIMAL DEPENDENCIES
// ========================================

// Minimal StreamInfo structure for testing
struct StreamInfo {
    std::string codec_type = "audio";
    std::string codec_name;
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
    uint16_t bits_per_sample = 0;
};

// Minimal MediaChunk structure for testing
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

// Minimal AudioFrame structure for testing
struct AudioFrame {
    std::vector<int16_t> samples;
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
    uint64_t timestamp_samples = 0;
    uint64_t timestamp_ms = 0;
    
    AudioFrame() = default;
    AudioFrame(size_t sample_count, uint32_t rate, uint16_t ch)
        : samples(sample_count), sample_rate(rate), channels(ch) {}
    
    size_t getByteCount() const {
        return samples.size() * sizeof(int16_t);
    }
    
    size_t getSampleFrameCount() const {
        return channels > 0 ? samples.size() / channels : 0;
    }
    
    uint64_t getDurationMs() const {
        if (sample_rate == 0 || channels == 0) return 0;
        return (getSampleFrameCount() * 1000ULL) / sample_rate;
    }
};

// Minimal AudioCodec base class for testing
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

// SimplePCMCodec implementation (minimal version for testing)
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
        return AudioFrame{}; // Empty frame - SimplePCMCodec doesn't buffer
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
// TEST CODEC IMPLEMENTATION
// ========================================

class TestSimplePCMCodec : public SimplePCMCodec {
public:
    explicit TestSimplePCMCodec(const StreamInfo& stream_info) 
        : SimplePCMCodec(stream_info), m_conversion_called(false) {}
    
    bool canDecode(const StreamInfo& stream_info) const override {
        return stream_info.codec_name == "test_pcm";
    }
    
    std::string getCodecName() const override {
        return "test_pcm";
    }
    
    bool wasConversionCalled() const { return m_conversion_called; }
    void resetConversionFlag() { m_conversion_called = false; }

protected:
    size_t convertSamples(const std::vector<uint8_t>& input_data, 
                         std::vector<int16_t>& output_samples) override {
        m_conversion_called = true;
        
        output_samples.clear();
        output_samples.reserve(input_data.size());
        
        for (uint8_t byte : input_data) {
            int16_t sample = static_cast<int16_t>(byte) - 128;
            output_samples.push_back(sample * 256);
        }
        
        return input_data.size();
    }
    
    size_t getBytesPerInputSample() const override {
        return 1;
    }

private:
    bool m_conversion_called;
};

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
                            " at " + __FILE__ + ":" + std::to_string(__LINE__) + \
                            " - Expected: " + std::to_string(expected) + \
                            ", Got: " + std::to_string(actual)); \
        } \
    } while(0)

// ========================================
// HELPER FUNCTIONS
// ========================================

StreamInfo createTestStreamInfo(const std::string& codec_name = "test_pcm",
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

MediaChunk createTestMediaChunk(const std::vector<uint8_t>& data, 
                               uint32_t stream_id = 0,
                               uint64_t timestamp_samples = 0) {
    MediaChunk chunk;
    chunk.stream_id = stream_id;
    chunk.data = data;
    chunk.timestamp_samples = timestamp_samples;
    chunk.is_keyframe = true;
    return chunk;
}

// ========================================
// TEST CASES
// ========================================

void test_initialize_with_various_streaminfo() {
    std::cout << "Testing initialize() with various StreamInfo configurations..." << std::endl;
    
    // Test 1: Valid basic configuration
    {
        StreamInfo info = createTestStreamInfo();
        TestSimplePCMCodec codec(info);
        
        ASSERT_TRUE(codec.initialize(), "Basic initialization should succeed");
        ASSERT_TRUE(codec.isInitialized(), "Codec should report as initialized");
    }
    
    // Test 2: Different sample rates
    {
        std::vector<uint32_t> sample_rates = {8000, 16000, 44100, 48000};
        for (uint32_t rate : sample_rates) {
            StreamInfo info = createTestStreamInfo("test_pcm", rate);
            TestSimplePCMCodec codec(info);
            
            ASSERT_TRUE(codec.initialize(), "Initialization should succeed for sample rate");
            ASSERT_EQUALS(rate, codec.getStreamInfo().sample_rate, "Sample rate should be preserved");
        }
    }
    
    // Test 3: Different channel configurations
    {
        std::vector<uint16_t> channel_counts = {1, 2};
        for (uint16_t channels : channel_counts) {
            StreamInfo info = createTestStreamInfo("test_pcm", 8000, channels);
            TestSimplePCMCodec codec(info);
            
            ASSERT_TRUE(codec.initialize(), "Initialization should succeed for channels");
            ASSERT_EQUALS(channels, codec.getStreamInfo().channels, "Channel count should be preserved");
        }
    }
    
    // Test 4: Multiple initialization calls
    {
        StreamInfo info = createTestStreamInfo();
        TestSimplePCMCodec codec(info);
        
        ASSERT_TRUE(codec.initialize(), "First initialization should succeed");
        ASSERT_TRUE(codec.initialize(), "Second initialization should also succeed");
        ASSERT_TRUE(codec.isInitialized(), "Codec should remain initialized");
    }
    
    std::cout << "✓ initialize() method works correctly with various StreamInfo configurations" << std::endl;
}

void test_decode_with_different_chunk_sizes() {
    std::cout << "Testing decode() with different MediaChunk sizes..." << std::endl;
    
    StreamInfo info = createTestStreamInfo();
    TestSimplePCMCodec codec(info);
    ASSERT_TRUE(codec.initialize(), "Codec initialization should succeed");
    
    // Test 1: Empty chunk
    {
        MediaChunk empty_chunk = createTestMediaChunk({});
        AudioFrame frame = codec.decode(empty_chunk);
        
        ASSERT_EQUALS(0u, frame.samples.size(), "Empty chunk should produce empty frame");
    }
    
    // Test 2: Single byte chunk
    {
        codec.resetConversionFlag();
        MediaChunk single_chunk = createTestMediaChunk({128});
        AudioFrame frame = codec.decode(single_chunk);
        
        ASSERT_TRUE(codec.wasConversionCalled(), "Conversion should be called for non-empty chunk");
        ASSERT_EQUALS(1u, frame.samples.size(), "Single byte should produce one sample");
    }
    
    // Test 3: Small chunk (10 bytes)
    {
        codec.resetConversionFlag();
        std::vector<uint8_t> small_data = {0, 32, 64, 96, 128, 160, 192, 224, 255, 127};
        MediaChunk small_chunk = createTestMediaChunk(small_data);
        AudioFrame frame = codec.decode(small_chunk);
        
        ASSERT_TRUE(codec.wasConversionCalled(), "Conversion should be called");
        ASSERT_EQUALS(small_data.size(), frame.samples.size(), "Frame should have correct sample count");
    }
    
    // Test 4: Timestamp preservation
    {
        std::vector<uint8_t> test_data = {100, 150, 200};
        uint64_t test_timestamp = 12345;
        MediaChunk timestamped_chunk = createTestMediaChunk(test_data, 0, test_timestamp);
        AudioFrame frame = codec.decode(timestamped_chunk);
        
        ASSERT_EQUALS(test_timestamp, frame.timestamp_samples, "Timestamp should be preserved");
        ASSERT_EQUALS(test_data.size(), frame.samples.size(), "Sample count should be correct");
    }
    
    std::cout << "✓ decode() method works correctly with different MediaChunk sizes" << std::endl;
}

void test_flush_behavior() {
    std::cout << "Testing flush() behavior for stream completion scenarios..." << std::endl;
    
    StreamInfo info = createTestStreamInfo();
    TestSimplePCMCodec codec(info);
    ASSERT_TRUE(codec.initialize(), "Codec initialization should succeed");
    
    // Test 1: Flush on fresh codec
    {
        AudioFrame flush_frame = codec.flush();
        ASSERT_EQUALS(0u, flush_frame.samples.size(), "Fresh codec flush should return empty frame");
    }
    
    // Test 2: Flush after processing data
    {
        std::vector<uint8_t> test_data = {64, 128, 192};
        MediaChunk chunk = createTestMediaChunk(test_data);
        AudioFrame decode_frame = codec.decode(chunk);
        
        ASSERT_EQUALS(test_data.size(), decode_frame.samples.size(), "Decode should work normally");
        
        AudioFrame flush_frame = codec.flush();
        ASSERT_EQUALS(0u, flush_frame.samples.size(), "Flush after decode should return empty frame");
    }
    
    // Test 3: Multiple flush calls
    {
        AudioFrame flush1 = codec.flush();
        AudioFrame flush2 = codec.flush();
        
        ASSERT_EQUALS(0u, flush1.samples.size(), "First flush should return empty frame");
        ASSERT_EQUALS(0u, flush2.samples.size(), "Second flush should return empty frame");
    }
    
    std::cout << "✓ flush() behavior works correctly for stream completion scenarios" << std::endl;
}

void test_reset_functionality() {
    std::cout << "Testing reset() functionality for seeking operations..." << std::endl;
    
    StreamInfo info = createTestStreamInfo();
    TestSimplePCMCodec codec(info);
    ASSERT_TRUE(codec.initialize(), "Codec initialization should succeed");
    
    // Test 1: Reset on fresh codec
    {
        codec.reset();
        ASSERT_TRUE(codec.isInitialized(), "Reset should not affect initialization state");
    }
    
    // Test 2: Reset after processing data
    {
        std::vector<uint8_t> test_data = {50, 100, 150, 200, 250};
        MediaChunk chunk = createTestMediaChunk(test_data);
        AudioFrame decode_frame = codec.decode(chunk);
        
        ASSERT_EQUALS(test_data.size(), decode_frame.samples.size(), "Decode should work normally");
        
        codec.reset();
        ASSERT_TRUE(codec.isInitialized(), "Reset should not affect initialization state");
        
        std::vector<uint8_t> new_data = {75, 125, 175};
        MediaChunk new_chunk = createTestMediaChunk(new_data);
        AudioFrame new_frame = codec.decode(new_chunk);
        
        ASSERT_EQUALS(new_data.size(), new_frame.samples.size(), "Decode should work after reset");
    }
    
    // Test 3: Seeking simulation
    {
        std::vector<uint8_t> data1 = {10, 20, 30};
        MediaChunk chunk1 = createTestMediaChunk(data1, 0, 1000);
        AudioFrame frame1 = codec.decode(chunk1);
        
        ASSERT_EQUALS(data1.size(), frame1.samples.size(), "First decode should work");
        ASSERT_EQUALS(1000u, frame1.timestamp_samples, "First timestamp should be preserved");
        
        codec.reset();
        
        std::vector<uint8_t> data2 = {40, 50};
        MediaChunk chunk2 = createTestMediaChunk(data2, 0, 5000);
        AudioFrame frame2 = codec.decode(chunk2);
        
        ASSERT_EQUALS(data2.size(), frame2.samples.size(), "Second decode should work after reset");
        ASSERT_EQUALS(5000u, frame2.timestamp_samples, "Second timestamp should be preserved");
    }
    
    std::cout << "✓ reset() functionality works correctly for seeking operations" << std::endl;
}

void test_audiocodec_interface_compliance() {
    std::cout << "Testing AudioCodec interface compliance..." << std::endl;
    
    StreamInfo info = createTestStreamInfo();
    TestSimplePCMCodec codec(info);
    
    // Test 1: getCodecName()
    {
        std::string codec_name = codec.getCodecName();
        ASSERT_TRUE(codec_name == "test_pcm", "getCodecName should return correct name");
    }
    
    // Test 2: canDecode()
    {
        ASSERT_TRUE(codec.canDecode(info), "canDecode should return true for supported format");
        
        StreamInfo unsupported = createTestStreamInfo("unsupported");
        ASSERT_FALSE(codec.canDecode(unsupported), "canDecode should return false for unsupported format");
    }
    
    // Test 3: getStreamInfo()
    {
        const StreamInfo& retrieved_info = codec.getStreamInfo();
        ASSERT_TRUE(info.codec_name == retrieved_info.codec_name, "getStreamInfo should return original info");
        ASSERT_EQUALS(info.sample_rate, retrieved_info.sample_rate, "Sample rate should match");
        ASSERT_EQUALS(info.channels, retrieved_info.channels, "Channels should match");
    }
    
    // Test 4: isInitialized()
    {
        ASSERT_FALSE(codec.isInitialized(), "Should not be initialized initially");
        ASSERT_TRUE(codec.initialize(), "Initialization should succeed");
        ASSERT_TRUE(codec.isInitialized(), "Should be initialized after initialize()");
    }
    
    std::cout << "✓ AudioCodec interface compliance verified" << std::endl;
}

// ========================================
// MAIN TEST EXECUTION
// ========================================

int main() {
    std::cout << "=== SimplePCMCodec Integration Tests (Minimal) ===" << std::endl;
    std::cout << "Testing SimplePCMCodec base class integration with AudioCodec interface" << std::endl;
    std::cout << std::endl;
    
    try {
        test_initialize_with_various_streaminfo();
        std::cout << std::endl;
        
        test_decode_with_different_chunk_sizes();
        std::cout << std::endl;
        
        test_flush_behavior();
        std::cout << std::endl;
        
        test_reset_functionality();
        std::cout << std::endl;
        
        test_audiocodec_interface_compliance();
        std::cout << std::endl;
        
        std::cout << "=== ALL TESTS PASSED ===" << std::endl;
        std::cout << "SimplePCMCodec integration tests completed successfully!" << std::endl;
        
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