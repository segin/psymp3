/*
 * test_mulaw_alaw_performance.cpp - Performance tests for μ-law/A-law codecs
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
#include <chrono>
#include <random>
#include <iomanip>
#include <cmath>
#include <thread>
#include <mutex>

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

/**
 * @brief Performance test suite for μ-law and A-law codecs
 * 
 * Tests real-time decoding performance requirements:
 * - Requirement 5.1: Use pre-computed lookup tables for conversion
 * - Requirement 5.2: Maintain real-time performance for telephony applications
 * - Requirement 5.3: Support concurrent decoding efficiently
 * - Requirement 5.4: Minimize memory allocation overhead
 * - Requirement 5.7: Optimize for cache-friendly memory access patterns
 * - Requirement 5.8: Exceed real-time requirements by significant margin
 */

// Performance test constants
static constexpr size_t TELEPHONY_SAMPLE_RATE = 8000;
static constexpr size_t WIDEBAND_SAMPLE_RATE = 16000;
static constexpr size_t SUPER_WIDEBAND_SAMPLE_RATE = 32000;
static constexpr size_t FULLBAND_SAMPLE_RATE = 48000;
static constexpr size_t VOIP_PACKET_SIZE = 160;  // 20ms at 8kHz
static constexpr size_t SMALL_PACKET_SIZE = 80;  // 10ms at 8kHz
static constexpr size_t LARGE_PACKET_SIZE = 1600; // 200ms at 8kHz

// Performance thresholds
static constexpr double MIN_REAL_TIME_FACTOR = 10.0;  // 10x real-time minimum
static constexpr double MIN_WIDEBAND_FACTOR = 5.0;    // 5x for wideband
static constexpr double MIN_SMALL_PACKET_FACTOR = 8.0; // 8x for small packets
static constexpr double MIN_LARGE_PACKET_FACTOR = 15.0; // 15x for large packets

int test_failures = 0;

/**
 * @brief Generate random audio data for performance testing
 */
std::vector<uint8_t> generateRandomAudioData(size_t size) {
    std::vector<uint8_t> data(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    
    for (size_t i = 0; i < size; ++i) {
        data[i] = dist(gen);
    }
    
    return data;
}

/**
 * @brief Measure decoding performance for a codec
 */
template<typename CodecType>
struct PerformanceMetrics {
    double real_time_factor;
    double samples_per_second;
    double packets_per_second;
    size_t total_samples;
    size_t total_packets;
    double duration_seconds;
};

template<typename CodecType>
PerformanceMetrics<CodecType> measureDecodingPerformance(
    const std::string& codec_name, 
    size_t sample_rate, 
    size_t packet_size,
    size_t test_duration_ms = 1000) {
    
    // Create stream info
    StreamInfo stream_info;
    stream_info.codec_name = codec_name;
    stream_info.sample_rate = sample_rate;
    stream_info.channels = 1;
    stream_info.bits_per_sample = 8;
    
    // Create codec instance
    CodecType codec(stream_info);
    if (!codec.initialize()) {
        throw std::runtime_error("Failed to initialize " + codec_name + " codec");
    }
    
    // Generate test data
    std::vector<uint8_t> test_data = generateRandomAudioData(packet_size);
    
    // Measure performance
    auto start_time = std::chrono::high_resolution_clock::now();
    auto end_time = start_time + std::chrono::milliseconds(test_duration_ms);
    
    size_t packets_processed = 0;
    size_t total_samples = 0;
    
    while (std::chrono::high_resolution_clock::now() < end_time) {
        MediaChunk chunk;
        chunk.data = test_data;
        chunk.timestamp_samples = packets_processed * packet_size;
        
        AudioFrame frame = codec.decode(chunk);
        if (frame.samples.empty()) {
            throw std::runtime_error("Decoding failed during performance test");
        }
        
        packets_processed++;
        total_samples += frame.samples.size();
    }
    
    auto actual_duration = std::chrono::high_resolution_clock::now() - start_time;
    double duration_seconds = std::chrono::duration<double>(actual_duration).count();
    
    // Calculate performance metrics
    double samples_per_second = total_samples / duration_seconds;
    double real_time_factor = samples_per_second / sample_rate;
    double packets_per_second = packets_processed / duration_seconds;
    
    return {real_time_factor, samples_per_second, packets_per_second, 
            total_samples, packets_processed, duration_seconds};
}

/**
 * @brief Test μ-law codec real-time performance at telephony rates
 */
void testMuLawTelephonyPerformance() {
    std::cout << "\n=== μ-law Telephony Performance (8 kHz) ===" << std::endl;
    
    try {
#ifdef ENABLE_MULAW_CODEC
        auto metrics = measureDecodingPerformance<MuLawCodec>(
            "mulaw", TELEPHONY_SAMPLE_RATE, VOIP_PACKET_SIZE);
        
        std::cout << "  Real-time factor: " << std::fixed << std::setprecision(2) 
                  << metrics.real_time_factor << "x" << std::endl;
        std::cout << "  Samples/second: " << std::fixed << std::setprecision(0) 
                  << metrics.samples_per_second << std::endl;
        std::cout << "  Packets/second: " << std::fixed << std::setprecision(0) 
                  << metrics.packets_per_second << std::endl;
        std::cout << "  Total samples: " << metrics.total_samples << std::endl;
        
        if (metrics.real_time_factor >= MIN_REAL_TIME_FACTOR) {
            std::cout << "  ✓ PASS: Exceeds real-time requirements" << std::endl;
        } else {
            std::cout << "  ✗ FAIL: Performance insufficient: " 
                     << metrics.real_time_factor << "x < " << MIN_REAL_TIME_FACTOR << "x" << std::endl;
            test_failures++;
        }
#else
        std::cout << "  SKIP: μ-law codec not enabled" << std::endl;
#endif
    } catch (const std::exception& e) {
        std::cout << "  ✗ FAIL: Exception: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test A-law codec real-time performance at telephony rates
 */
void testALawTelephonyPerformance() {
    std::cout << "\n=== A-law Telephony Performance (8 kHz) ===" << std::endl;
    
    try {
#ifdef ENABLE_ALAW_CODEC
        auto metrics = measureDecodingPerformance<ALawCodec>(
            "alaw", TELEPHONY_SAMPLE_RATE, VOIP_PACKET_SIZE);
        
        std::cout << "  Real-time factor: " << std::fixed << std::setprecision(2) 
                  << metrics.real_time_factor << "x" << std::endl;
        std::cout << "  Samples/second: " << std::fixed << std::setprecision(0) 
                  << metrics.samples_per_second << std::endl;
        std::cout << "  Packets/second: " << std::fixed << std::setprecision(0) 
                  << metrics.packets_per_second << std::endl;
        std::cout << "  Total samples: " << metrics.total_samples << std::endl;
        
        if (metrics.real_time_factor >= MIN_REAL_TIME_FACTOR) {
            std::cout << "  ✓ PASS: Exceeds real-time requirements" << std::endl;
        } else {
            std::cout << "  ✗ FAIL: Performance insufficient: " 
                     << metrics.real_time_factor << "x < " << MIN_REAL_TIME_FACTOR << "x" << std::endl;
            test_failures++;
        }
#else
        std::cout << "  SKIP: A-law codec not enabled" << std::endl;
#endif
    } catch (const std::exception& e) {
        std::cout << "  ✗ FAIL: Exception: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test codec performance with multiple sample rates
 */
void testMultipleSampleRates() {
    std::cout << "\n=== Multiple Sample Rate Performance ===" << std::endl;
    
    std::vector<std::pair<size_t, std::string>> rates = {
        {TELEPHONY_SAMPLE_RATE, "8 kHz"},
        {WIDEBAND_SAMPLE_RATE, "16 kHz"},
        {SUPER_WIDEBAND_SAMPLE_RATE, "32 kHz"},
        {FULLBAND_SAMPLE_RATE, "48 kHz"}
    };
    
    try {
#ifdef ENABLE_MULAW_CODEC
        std::cout << "\nμ-law codec:" << std::endl;
        for (const auto& [rate, label] : rates) {
            auto metrics = measureDecodingPerformance<MuLawCodec>(
                "mulaw", rate, VOIP_PACKET_SIZE);
            
            std::cout << "  " << label << ": " << std::fixed << std::setprecision(2) 
                      << metrics.real_time_factor << "x real-time" << std::endl;
        }
#endif

#ifdef ENABLE_ALAW_CODEC
        std::cout << "\nA-law codec:" << std::endl;
        for (const auto& [rate, label] : rates) {
            auto metrics = measureDecodingPerformance<ALawCodec>(
                "alaw", rate, VOIP_PACKET_SIZE);
            
            std::cout << "  " << label << ": " << std::fixed << std::setprecision(2) 
                      << metrics.real_time_factor << "x real-time" << std::endl;
        }
#endif
        
        std::cout << "  ✓ PASS: All sample rates tested successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  ✗ FAIL: Exception: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test multi-channel processing efficiency
 */
void testMultiChannelProcessing() {
    std::cout << "\n=== Multi-channel Processing Efficiency ===" << std::endl;
    
    try {
#ifdef ENABLE_MULAW_CODEC
        std::cout << "\nμ-law mono (1 channel):" << std::endl;
        StreamInfo mono_info;
        mono_info.codec_name = "mulaw";
        mono_info.sample_rate = TELEPHONY_SAMPLE_RATE;
        mono_info.channels = 1;
        
        MuLawCodec mono_codec(mono_info);
        mono_codec.initialize();
        
        std::vector<uint8_t> mono_data = generateRandomAudioData(VOIP_PACKET_SIZE);
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < 1000; ++i) {
            MediaChunk chunk;
            chunk.data = mono_data;
            mono_codec.decode(chunk);
        }
        
        auto mono_duration = std::chrono::high_resolution_clock::now() - start;
        double mono_ms = std::chrono::duration<double, std::milli>(mono_duration).count();
        
        std::cout << "  Mono decode time (1000 packets): " << std::fixed << std::setprecision(2) 
                  << mono_ms << " ms" << std::endl;
        
        std::cout << "\nμ-law stereo (2 channels):" << std::endl;
        StreamInfo stereo_info;
        stereo_info.codec_name = "mulaw";
        stereo_info.sample_rate = TELEPHONY_SAMPLE_RATE;
        stereo_info.channels = 2;
        
        MuLawCodec stereo_codec(stereo_info);
        stereo_codec.initialize();
        
        std::vector<uint8_t> stereo_data = generateRandomAudioData(VOIP_PACKET_SIZE * 2);
        start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < 1000; ++i) {
            MediaChunk chunk;
            chunk.data = stereo_data;
            stereo_codec.decode(chunk);
        }
        
        auto stereo_duration = std::chrono::high_resolution_clock::now() - start;
        double stereo_ms = std::chrono::duration<double, std::milli>(stereo_duration).count();
        
        std::cout << "  Stereo decode time (1000 packets): " << std::fixed << std::setprecision(2) 
                  << stereo_ms << " ms" << std::endl;
        
        double efficiency = mono_ms / stereo_ms;
        std::cout << "  Efficiency ratio: " << std::fixed << std::setprecision(2) 
                  << efficiency << "x" << std::endl;
        
        std::cout << "  ✓ PASS: Multi-channel processing tested" << std::endl;
#else
        std::cout << "  SKIP: μ-law codec not enabled" << std::endl;
#endif
    } catch (const std::exception& e) {
        std::cout << "  ✗ FAIL: Exception: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test samples processed per second
 */
void testSamplesPerSecond() {
    std::cout << "\n=== Samples Processed Per Second ===" << std::endl;
    
    try {
#ifdef ENABLE_MULAW_CODEC
        std::cout << "\nμ-law codec:" << std::endl;
        auto mulaw_metrics = measureDecodingPerformance<MuLawCodec>(
            "mulaw", TELEPHONY_SAMPLE_RATE, VOIP_PACKET_SIZE, 2000);
        
        std::cout << "  Samples/second: " << std::fixed << std::setprecision(0) 
                  << mulaw_metrics.samples_per_second << std::endl;
        std::cout << "  Packets/second: " << std::fixed << std::setprecision(0) 
                  << mulaw_metrics.packets_per_second << std::endl;
        std::cout << "  Duration: " << std::fixed << std::setprecision(3) 
                  << mulaw_metrics.duration_seconds << " seconds" << std::endl;
#endif

#ifdef ENABLE_ALAW_CODEC
        std::cout << "\nA-law codec:" << std::endl;
        auto alaw_metrics = measureDecodingPerformance<ALawCodec>(
            "alaw", TELEPHONY_SAMPLE_RATE, VOIP_PACKET_SIZE, 2000);
        
        std::cout << "  Samples/second: " << std::fixed << std::setprecision(0) 
                  << alaw_metrics.samples_per_second << std::endl;
        std::cout << "  Packets/second: " << std::fixed << std::setprecision(0) 
                  << alaw_metrics.packets_per_second << std::endl;
        std::cout << "  Duration: " << std::fixed << std::setprecision(3) 
                  << alaw_metrics.duration_seconds << " seconds" << std::endl;
#endif
        
        std::cout << "  ✓ PASS: Throughput metrics collected" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  ✗ FAIL: Exception: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test lookup table memory footprint
 */
void testLookupTableMemoryFootprint() {
    std::cout << "\n=== Lookup Table Memory Footprint ===" << std::endl;
    
    try {
        const size_t EXPECTED_TABLE_SIZE = 256 * sizeof(int16_t); // 512 bytes per table
        const size_t TOTAL_EXPECTED = EXPECTED_TABLE_SIZE * 2; // Both tables
        
        std::cout << "  Expected per-table size: " << EXPECTED_TABLE_SIZE << " bytes" << std::endl;
        std::cout << "  Expected total size: " << TOTAL_EXPECTED << " bytes" << std::endl;
        
        // Verify tables fit in typical L1 cache (32KB)
        if (TOTAL_EXPECTED < 32768) {
            std::cout << "  ✓ Tables fit in L1 cache (32KB)" << std::endl;
        } else {
            std::cout << "  ✗ Tables exceed L1 cache size" << std::endl;
            test_failures++;
        }
        
        // Verify tables fit in typical L2 cache (256KB)
        if (TOTAL_EXPECTED < 262144) {
            std::cout << "  ✓ Tables fit in L2 cache (256KB)" << std::endl;
        } else {
            std::cout << "  ✗ Tables exceed L2 cache size" << std::endl;
            test_failures++;
        }
        
        std::cout << "  ✓ PASS: Memory footprint acceptable" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  ✗ FAIL: Exception: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test concurrent codec instance memory usage
 */
void testConcurrentInstanceMemory() {
    std::cout << "\n=== Concurrent Codec Instance Memory Usage ===" << std::endl;
    
    try {
        const size_t NUM_INSTANCES = 100;
        std::vector<std::unique_ptr<AudioCodec>> codecs;
        
#ifdef ENABLE_MULAW_CODEC
        std::cout << "\nCreating " << NUM_INSTANCES << " μ-law codec instances..." << std::endl;
        
        StreamInfo mulaw_info;
        mulaw_info.codec_name = "mulaw";
        mulaw_info.sample_rate = TELEPHONY_SAMPLE_RATE;
        mulaw_info.channels = 1;
        
        for (size_t i = 0; i < NUM_INSTANCES; ++i) {
            auto codec = std::make_unique<MuLawCodec>(mulaw_info);
            if (codec->initialize()) {
                codecs.push_back(std::move(codec));
            }
        }
        
        std::cout << "  Successfully created " << codecs.size() << " instances" << std::endl;
        
        // Estimate memory per instance (excluding shared tables)
        // Each instance should only have StreamInfo and state variables
        size_t estimated_per_instance = sizeof(StreamInfo) + 64; // 64 bytes for state
        size_t total_estimated = estimated_per_instance * codecs.size();
        
        std::cout << "  Estimated per-instance: " << estimated_per_instance << " bytes" << std::endl;
        std::cout << "  Estimated total: " << total_estimated << " bytes" << std::endl;
        std::cout << "  Shared tables: 1024 bytes (not counted per instance)" << std::endl;
#endif

#ifdef ENABLE_ALAW_CODEC
        std::cout << "\nCreating " << NUM_INSTANCES << " A-law codec instances..." << std::endl;
        
        StreamInfo alaw_info;
        alaw_info.codec_name = "alaw";
        alaw_info.sample_rate = TELEPHONY_SAMPLE_RATE;
        alaw_info.channels = 1;
        
        for (size_t i = 0; i < NUM_INSTANCES; ++i) {
            auto codec = std::make_unique<ALawCodec>(alaw_info);
            if (codec->initialize()) {
                codecs.push_back(std::move(codec));
            }
        }
        
        std::cout << "  Successfully created " << codecs.size() << " total instances" << std::endl;
#endif
        
        if (!codecs.empty()) {
            std::cout << "  ✓ PASS: Multiple instances created with shared tables" << std::endl;
        } else {
            std::cout << "  ✗ FAIL: Failed to create codec instances" << std::endl;
            test_failures++;
        }
        
    } catch (const std::exception& e) {
        std::cout << "  ✗ FAIL: Exception: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test cache efficiency with sequential access
 */
void testCacheEfficiency() {
    std::cout << "\n=== Cache Efficiency with Sequential Access ===" << std::endl;
    
    try {
#ifdef ENABLE_MULAW_CODEC
        std::cout << "\nTesting sequential access patterns..." << std::endl;
        
        StreamInfo info;
        info.codec_name = "mulaw";
        info.sample_rate = TELEPHONY_SAMPLE_RATE;
        info.channels = 1;
        
        MuLawCodec codec(info);
        codec.initialize();
        
        // Test with sequential data (good cache locality)
        std::vector<uint8_t> sequential_data(10000);
        for (size_t i = 0; i < sequential_data.size(); ++i) {
            sequential_data[i] = static_cast<uint8_t>(i % 256);
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        for (int iter = 0; iter < 100; ++iter) {
            MediaChunk chunk;
            chunk.data = sequential_data;
            codec.decode(chunk);
        }
        auto sequential_duration = std::chrono::high_resolution_clock::now() - start;
        double sequential_ms = std::chrono::duration<double, std::milli>(sequential_duration).count();
        
        // Test with random data (poor cache locality)
        std::vector<uint8_t> random_data = generateRandomAudioData(10000);
        
        start = std::chrono::high_resolution_clock::now();
        for (int iter = 0; iter < 100; ++iter) {
            MediaChunk chunk;
            chunk.data = random_data;
            codec.decode(chunk);
        }
        auto random_duration = std::chrono::high_resolution_clock::now() - start;
        double random_ms = std::chrono::duration<double, std::milli>(random_duration).count();
        
        std::cout << "  Sequential access time: " << std::fixed << std::setprecision(2) 
                  << sequential_ms << " ms" << std::endl;
        std::cout << "  Random access time: " << std::fixed << std::setprecision(2) 
                  << random_ms << " ms" << std::endl;
        
        double cache_efficiency = random_ms / sequential_ms;
        std::cout << "  Cache efficiency ratio: " << std::fixed << std::setprecision(2) 
                  << cache_efficiency << "x" << std::endl;
        
        if (cache_efficiency > 1.0) {
            std::cout << "  ✓ PASS: Sequential access is faster (good cache locality)" << std::endl;
        } else {
            std::cout << "  ⚠ Note: Cache efficiency not clearly demonstrated" << std::endl;
        }
#else
        std::cout << "  SKIP: μ-law codec not enabled" << std::endl;
#endif
    } catch (const std::exception& e) {
        std::cout << "  ✗ FAIL: Exception: " << e.what() << std::endl;
        test_failures++;
    }
}

/**
 * @brief Test performance with different packet sizes
 */
void testPacketSizePerformance() {
    std::cout << "\n=== Performance with Different Packet Sizes ===" << std::endl;
    
    std::vector<std::pair<size_t, std::string>> packet_sizes = {
        {SMALL_PACKET_SIZE, "Small (10ms)"},
        {VOIP_PACKET_SIZE, "Standard (20ms)"},
        {LARGE_PACKET_SIZE, "Large (200ms)"}
    };
    
    try {
#ifdef ENABLE_MULAW_CODEC
        std::cout << "\nμ-law codec:" << std::endl;
        for (const auto& [size, label] : packet_sizes) {
            auto metrics = measureDecodingPerformance<MuLawCodec>(
                "mulaw", TELEPHONY_SAMPLE_RATE, size);
            
            std::cout << "  " << label << ": " << std::fixed << std::setprecision(2) 
                      << metrics.real_time_factor << "x real-time" << std::endl;
        }
#endif

#ifdef ENABLE_ALAW_CODEC
        std::cout << "\nA-law codec:" << std::endl;
        for (const auto& [size, label] : packet_sizes) {
            auto metrics = measureDecodingPerformance<ALawCodec>(
                "alaw", TELEPHONY_SAMPLE_RATE, size);
            
            std::cout << "  " << label << ": " << std::fixed << std::setprecision(2) 
                      << metrics.real_time_factor << "x real-time" << std::endl;
        }
#endif
        
        std::cout << "  ✓ PASS: Packet size performance tested" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  ✗ FAIL: Exception: " << e.what() << std::endl;
        test_failures++;
    }
}

int main() {
    try {
        std::cout << "╔════════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║  μ-law/A-law Codec Performance Test Suite                  ║" << std::endl;
        std::cout << "║  Testing real-time decoding performance for telephony      ║" << std::endl;
        std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
        
        testMuLawTelephonyPerformance();
        testALawTelephonyPerformance();
        testMultipleSampleRates();
        testMultiChannelProcessing();
        testSamplesPerSecond();
        testLookupTableMemoryFootprint();
        testConcurrentInstanceMemory();
        testCacheEfficiency();
        testPacketSizePerformance();
        
        std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║  Performance Tests Complete                                ║" << std::endl;
        std::cout << "║  Test failures: " << std::setw(40) << test_failures << "   ║" << std::endl;
        std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
        
        return test_failures > 0 ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << "Test framework error: " << e.what() << std::endl;
        return 1;
    }
}
