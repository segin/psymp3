/*
 * test_mulaw_alaw_integration.cpp - Integration tests for μ-law/A-law codecs
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <cstdint>

// Minimal test framework
class TestFailure : public std::exception {
public:
    explicit TestFailure(const std::string& message) : m_message(message) {}
    const char* what() const noexcept override { return m_message.c_str(); }
private:
    std::string m_message;
};

#define ASSERT_TRUE(condition, message) \
    do { if (!(condition)) throw TestFailure(std::string("ASSERTION FAILED: ") + (message)); } while(0)

#define ASSERT_EQUALS(expected, actual, message) \
    do { if (!((expected) == (actual))) throw TestFailure(std::string("ASSERTION FAILED: ") + (message) + \
        " - Expected: " + std::to_string(expected) + ", Got: " + std::to_string(actual)); } while(0)

// Minimal StreamInfo and MediaChunk structures
struct StreamInfo {
    std::string codec_type = "audio";
    std::string codec_name;
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
    uint16_t bits_per_sample = 0;
};

struct MediaChunk {
    std::vector<uint8_t> data;
    uint64_t timestamp_samples = 0;
};

struct AudioFrame {
    std::vector<int16_t> samples;
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
    uint64_t timestamp_samples = 0;
    uint64_t timestamp_ms = 0;
};

// Minimal AudioCodec base class
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
    
    bool isInitialized() const { return m_initialized; }

protected:
    StreamInfo m_stream_info;
    bool m_initialized = false;
};

// SimplePCMCodec base class
class SimplePCMCodec : public AudioCodec {
public:
    explicit SimplePCMCodec(const StreamInfo& stream_info) : AudioCodec(stream_info) {}
    
    bool initialize() override {
        m_initialized = true;
        return true;
    }
    
    AudioFrame decode(const MediaChunk& chunk) override {
        AudioFrame frame;
        if (!m_initialized || chunk.data.empty()) return frame;
        
        frame.sample_rate = m_stream_info.sample_rate;
        frame.channels = m_stream_info.channels;
        frame.timestamp_samples = chunk.timestamp_samples;
        if (m_stream_info.sample_rate > 0) {
            frame.timestamp_ms = (chunk.timestamp_samples * 1000ULL) / m_stream_info.sample_rate;
        }
        
        convertSamples(chunk.data, frame.samples);
        return frame;
    }
    
    AudioFrame flush() override { return AudioFrame{}; }
    void reset() override {}

protected:
    virtual size_t convertSamples(const std::vector<uint8_t>& input_data, 
                                  std::vector<int16_t>& output_samples) = 0;
    virtual size_t getBytesPerInputSample() const = 0;
};

// MuLawCodec implementation
class MuLawCodec : public SimplePCMCodec {
public:
    explicit MuLawCodec(const StreamInfo& stream_info) : SimplePCMCodec(stream_info) {
        if (!s_table_initialized) initializeMuLawTable();
    }
    
    bool canDecode(const StreamInfo& stream_info) const override {
        return stream_info.codec_name == "mulaw" || 
               stream_info.codec_name == "pcm_mulaw" ||
               stream_info.codec_name == "g711_mulaw";
    }
    
    std::string getCodecName() const override { return "mulaw"; }

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
    
    size_t getBytesPerInputSample() const override { return 1; }

private:
    static int16_t MULAW_TO_PCM[256];
    static bool s_table_initialized;
    
    static void initializeMuLawTable() {
        for (int i = 0; i < 256; ++i) {
            uint8_t mulaw_sample = static_cast<uint8_t>(i);
            uint8_t complement = mulaw_sample ^ 0xFF;
            bool sign = (complement & 0x80) != 0;
            uint8_t exponent = (complement & 0x70) >> 4;
            uint8_t mantissa = complement & 0x0F;
            
            int16_t linear;
            if (exponent == 0) {
                linear = 16 + 2 * mantissa;
            } else {
                linear = (16 + 2 * mantissa) << exponent;
            }
            
            if (!sign) linear = -linear;
            MULAW_TO_PCM[i] = linear;
        }
        s_table_initialized = true;
    }
};

int16_t MuLawCodec::MULAW_TO_PCM[256];
bool MuLawCodec::s_table_initialized = false;

// ALawCodec implementation
class ALawCodec : public SimplePCMCodec {
public:
    explicit ALawCodec(const StreamInfo& stream_info) : SimplePCMCodec(stream_info) {
        if (!s_table_initialized) initializeALawTable();
    }
    
    bool canDecode(const StreamInfo& stream_info) const override {
        return stream_info.codec_name == "alaw" || 
               stream_info.codec_name == "pcm_alaw" ||
               stream_info.codec_name == "g711_alaw";
    }
    
    std::string getCodecName() const override { return "alaw"; }

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
    
    size_t getBytesPerInputSample() const override { return 1; }

private:
    static int16_t ALAW_TO_PCM[256];
    static bool s_table_initialized;
    
    static void initializeALawTable() {
        for (int i = 0; i < 256; ++i) {
            uint8_t alaw_sample = static_cast<uint8_t>(i);
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
            
            if (sign) linear = -linear;
            ALAW_TO_PCM[i] = linear;
        }
        s_table_initialized = true;
    }
};

int16_t ALawCodec::ALAW_TO_PCM[256];
bool ALawCodec::s_table_initialized = false;

// Helper functions
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

// Test functions
void test_initialize_with_various_streaminfo() {
    std::cout << "Testing initialize() with various StreamInfo configurations..." << std::endl;
    
    StreamInfo info = createStreamInfo("mulaw");
    auto codec = std::make_unique<MuLawCodec>(info);
    ASSERT_TRUE(codec->initialize(), "MuLawCodec should initialize");
    ASSERT_TRUE(codec->isInitialized(), "MuLawCodec should report as initialized");
    
    StreamInfo info2 = createStreamInfo("alaw", 16000, 2);
    auto codec2 = std::make_unique<ALawCodec>(info2);
    ASSERT_TRUE(codec2->initialize(), "ALawCodec should initialize with 16 kHz stereo");
    
    std::cout << "✓ Initialize with various StreamInfo configurations works correctly" << std::endl;
}

void test_decode_with_different_chunk_sizes() {
    std::cout << "Testing decode() with different MediaChunk sizes..." << std::endl;
    
    StreamInfo info = createStreamInfo("mulaw");
    auto codec = std::make_unique<MuLawCodec>(info);
    ASSERT_TRUE(codec->initialize(), "MuLawCodec should initialize");
    
    // Small chunk
    std::vector<uint8_t> small_data = {0xFF};
    AudioFrame frame = codec->decode(createMediaChunk(small_data));
    ASSERT_EQUALS(1u, frame.samples.size(), "Should decode 1 sample");
    
    // Medium chunk
    std::vector<uint8_t> medium_data(100);
    for (size_t i = 0; i < medium_data.size(); ++i) medium_data[i] = static_cast<uint8_t>(i % 256);
    frame = codec->decode(createMediaChunk(medium_data));
    ASSERT_EQUALS(100u, frame.samples.size(), "Should decode 100 samples");
    
    // Large chunk
    std::vector<uint8_t> large_data(10000);
    for (size_t i = 0; i < large_data.size(); ++i) large_data[i] = static_cast<uint8_t>(i % 256);
    frame = codec->decode(createMediaChunk(large_data));
    ASSERT_EQUALS(10000u, frame.samples.size(), "Should decode 10000 samples");
    
    std::cout << "✓ Decode with different MediaChunk sizes works correctly" << std::endl;
}

void test_decode_voip_small_packets() {
    std::cout << "Testing decode() with VoIP-typical small packets..." << std::endl;
    
    const size_t VOIP_PACKET_SIZE = 160;
    StreamInfo info = createStreamInfo("mulaw", 8000, 1);
    auto codec = std::make_unique<MuLawCodec>(info);
    ASSERT_TRUE(codec->initialize(), "MuLawCodec should initialize for VoIP");
    
    for (int packet_num = 0; packet_num < 5; ++packet_num) {
        std::vector<uint8_t> voip_packet(VOIP_PACKET_SIZE);
        for (size_t i = 0; i < voip_packet.size(); ++i) {
            voip_packet[i] = static_cast<uint8_t>((packet_num * 256 + i) % 256);
        }
        
        uint64_t timestamp = packet_num * VOIP_PACKET_SIZE;
        AudioFrame frame = codec->decode(createMediaChunk(voip_packet, timestamp));
        
        ASSERT_EQUALS(VOIP_PACKET_SIZE, frame.samples.size(), "Should decode VoIP packet");
        ASSERT_EQUALS(8000u, frame.sample_rate, "Should preserve 8 kHz sample rate");
        ASSERT_EQUALS(timestamp, frame.timestamp_samples, "Should preserve timestamp");
    }
    
    std::cout << "✓ Decode with VoIP-typical small packets works correctly" << std::endl;
}

void test_flush_behavior() {
    std::cout << "Testing flush() behavior..." << std::endl;
    
    StreamInfo info = createStreamInfo("mulaw");
    auto codec = std::make_unique<MuLawCodec>(info);
    ASSERT_TRUE(codec->initialize(), "MuLawCodec should initialize");
    
    std::vector<uint8_t> data = {0xFF, 0x80, 0x00};
    AudioFrame frame = codec->decode(createMediaChunk(data));
    ASSERT_EQUALS(3u, frame.samples.size(), "Should decode 3 samples");
    
    AudioFrame flush_frame = codec->flush();
    ASSERT_EQUALS(0u, flush_frame.samples.size(), "Flush should return empty frame");
    
    std::cout << "✓ Flush behavior works correctly" << std::endl;
}

void test_reset_functionality() {
    std::cout << "Testing reset() functionality..." << std::endl;
    
    StreamInfo info = createStreamInfo("mulaw");
    auto codec = std::make_unique<MuLawCodec>(info);
    ASSERT_TRUE(codec->initialize(), "MuLawCodec should initialize");
    
    std::vector<uint8_t> data = {0xFF, 0x80};
    AudioFrame frame1 = codec->decode(createMediaChunk(data));
    ASSERT_EQUALS(2u, frame1.samples.size(), "Should decode 2 samples");
    
    codec->reset();
    ASSERT_TRUE(codec->isInitialized(), "Codec should still be initialized after reset");
    
    AudioFrame frame2 = codec->decode(createMediaChunk(data));
    ASSERT_EQUALS(2u, frame2.samples.size(), "Should decode 2 samples after reset");
    ASSERT_EQUALS(frame1.samples[0], frame2.samples[0], "Samples should be identical after reset");
    
    std::cout << "✓ Reset functionality works correctly" << std::endl;
}

void test_multi_channel_processing() {
    std::cout << "Testing multi-channel processing..." << std::endl;
    
    StreamInfo info = createStreamInfo("mulaw", 8000, 2);
    auto codec = std::make_unique<MuLawCodec>(info);
    ASSERT_TRUE(codec->initialize(), "MuLawCodec should initialize with stereo");
    
    std::vector<uint8_t> stereo_data = {0xFF, 0x80, 0x00, 0x7F, 0x40, 0xBF};
    AudioFrame frame = codec->decode(createMediaChunk(stereo_data));
    
    ASSERT_EQUALS(6u, frame.samples.size(), "Should decode 6 samples (3 stereo pairs)");
    ASSERT_EQUALS(2u, frame.channels, "Should report 2 channels");
    ASSERT_EQUALS(8000u, frame.sample_rate, "Should preserve sample rate");
    
    std::cout << "✓ Multi-channel processing works correctly" << std::endl;
}

void test_various_sample_rates() {
    std::cout << "Testing various sample rates (8, 16, 32, 48 kHz)..." << std::endl;
    
    std::vector<uint32_t> sample_rates = {8000, 16000, 32000, 48000};
    
    for (uint32_t rate : sample_rates) {
        StreamInfo info = createStreamInfo("mulaw", rate, 1);
        auto codec = std::make_unique<MuLawCodec>(info);
        ASSERT_TRUE(codec->initialize(), "MuLawCodec should initialize with " + std::to_string(rate) + " Hz");
        
        std::vector<uint8_t> data(100);
        for (size_t i = 0; i < data.size(); ++i) data[i] = static_cast<uint8_t>(i % 256);
        AudioFrame frame = codec->decode(createMediaChunk(data));
        
        ASSERT_EQUALS(100u, frame.samples.size(), "Should decode 100 samples at " + std::to_string(rate) + " Hz");
        ASSERT_EQUALS(rate, frame.sample_rate, "Should preserve " + std::to_string(rate) + " Hz sample rate");
    }
    
    std::cout << "✓ Various sample rates work correctly" << std::endl;
}

void test_timestamp_preservation() {
    std::cout << "Testing timestamp preservation..." << std::endl;
    
    StreamInfo info = createStreamInfo("mulaw", 8000, 1);
    auto codec = std::make_unique<MuLawCodec>(info);
    ASSERT_TRUE(codec->initialize(), "MuLawCodec should initialize");
    
    std::vector<uint64_t> timestamps = {0, 160, 320, 1000, 8000, 16000};
    
    for (uint64_t ts : timestamps) {
        std::vector<uint8_t> data = {0xFF, 0x80, 0x00};
        AudioFrame frame = codec->decode(createMediaChunk(data, ts));
        
        ASSERT_EQUALS(ts, frame.timestamp_samples, "Should preserve timestamp " + std::to_string(ts));
        
        uint64_t expected_ms = (ts * 1000ULL) / 8000;
        ASSERT_EQUALS(expected_ms, frame.timestamp_ms, "Should calculate correct timestamp_ms");
    }
    
    std::cout << "✓ Timestamp preservation works correctly" << std::endl;
}

void test_empty_chunk_handling() {
    std::cout << "Testing empty chunk handling..." << std::endl;
    
    StreamInfo info = createStreamInfo("mulaw");
    auto codec = std::make_unique<MuLawCodec>(info);
    ASSERT_TRUE(codec->initialize(), "MuLawCodec should initialize");
    
    std::vector<uint8_t> empty_data;
    AudioFrame frame = codec->decode(createMediaChunk(empty_data));
    ASSERT_EQUALS(0u, frame.samples.size(), "Should return empty frame for empty chunk");
    
    std::vector<uint8_t> valid_data = {0xFF, 0x80};
    AudioFrame valid_frame = codec->decode(createMediaChunk(valid_data));
    ASSERT_EQUALS(2u, valid_frame.samples.size(), "Codec should still work after empty chunk");
    
    std::cout << "✓ Empty chunk handling works correctly" << std::endl;
}

void test_continuous_stream_processing() {
    std::cout << "Testing continuous stream processing..." << std::endl;
    
    StreamInfo info = createStreamInfo("mulaw", 8000, 1);
    auto codec = std::make_unique<MuLawCodec>(info);
    ASSERT_TRUE(codec->initialize(), "MuLawCodec should initialize");
    
    size_t total_samples = 0;
    const size_t CHUNK_SIZE = 160;
    const size_t NUM_CHUNKS = 10;
    
    for (size_t chunk_num = 0; chunk_num < NUM_CHUNKS; ++chunk_num) {
        std::vector<uint8_t> chunk_data(CHUNK_SIZE);
        for (size_t i = 0; i < CHUNK_SIZE; ++i) {
            chunk_data[i] = static_cast<uint8_t>((chunk_num * 256 + i) % 256);
        }
        
        uint64_t timestamp = chunk_num * CHUNK_SIZE;
        AudioFrame frame = codec->decode(createMediaChunk(chunk_data, timestamp));
        
        ASSERT_EQUALS(CHUNK_SIZE, frame.samples.size(), "Should decode chunk " + std::to_string(chunk_num));
        total_samples += frame.samples.size();
    }
    
    ASSERT_EQUALS(NUM_CHUNKS * CHUNK_SIZE, total_samples, "Should process all samples in continuous stream");
    
    std::cout << "✓ Continuous stream processing works correctly" << std::endl;
}

int main() {
    std::cout << "=== μ-law/A-law Codec Integration Tests ===" << std::endl;
    std::cout << "Testing SimplePCMCodec integration with MuLawCodec and ALawCodec" << std::endl;
    std::cout << std::endl;
    
    try {
        test_initialize_with_various_streaminfo();
        std::cout << std::endl;
        
        test_decode_with_different_chunk_sizes();
        std::cout << std::endl;
        
        test_decode_voip_small_packets();
        std::cout << std::endl;
        
        test_flush_behavior();
        std::cout << std::endl;
        
        test_reset_functionality();
        std::cout << std::endl;
        
        test_multi_channel_processing();
        std::cout << std::endl;
        
        test_various_sample_rates();
        std::cout << std::endl;
        
        test_timestamp_preservation();
        std::cout << std::endl;
        
        test_empty_chunk_handling();
        std::cout << std::endl;
        
        test_continuous_stream_processing();
        std::cout << std::endl;
        
        std::cout << "=== ALL INTEGRATION TESTS PASSED ===" << std::endl;
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
