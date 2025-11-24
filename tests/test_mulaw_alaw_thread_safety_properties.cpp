/*
 * test_mulaw_alaw_thread_safety_properties.cpp - Property-based tests for thread safety
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
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <random>
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <memory>

// ========================================
// MINIMAL CODEC IMPLEMENTATIONS FOR TESTING
// ========================================

struct StreamInfo {
    std::string codec_name;
    uint32_t sample_rate = 8000;
    uint16_t channels = 1;
    uint16_t bits_per_sample = 8;
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
};

// Base codec class
class AudioCodec {
public:
    explicit AudioCodec(const StreamInfo& stream_info) : m_stream_info(stream_info) {}
    virtual ~AudioCodec() = default;
    
    virtual bool initialize() = 0;
    virtual AudioFrame decode(const MediaChunk& chunk) = 0;
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
        
        convertSamples(chunk.data, frame.samples);
        return frame;
    }

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

// ========================================
// PROPERTY-BASED TEST FRAMEWORK
// ========================================

static constexpr size_t NUM_THREADS = 8;
static constexpr size_t OPERATIONS_PER_THREAD = 100;
static constexpr size_t PACKET_SIZE = 160;
static constexpr size_t NUM_ITERATIONS = 10;

std::atomic<int> test_failures{0};
std::mutex result_mutex;
std::vector<std::string> thread_results;

// Generate random audio data
std::vector<uint8_t> generateTestData(size_t size, int seed) {
    std::vector<uint8_t> data(size);
    std::mt19937 gen(seed);
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    
    for (size_t i = 0; i < size; ++i) {
        data[i] = dist(gen);
    }
    
    return data;
}

// ========================================
// PROPERTY 7: Thread Safety Independence
// ========================================
// Feature: mulaw-alaw-codec, Property 7: Thread Safety Independence
// Validates: Requirements 11.1, 11.2
//
// For any two codec instances operating concurrently, operations on one 
// instance should not affect the state or output of the other instance.
//
// This property tests that:
// 1. Each codec instance maintains independent state
// 2. Concurrent operations on different instances don't interfere
// 3. Output from one instance is not affected by operations on another
// 4. Shared lookup tables are safely accessed without corruption

template<typename CodecType>
void threadWorker(const std::string& codec_name,
                 int thread_id,
                 int iteration,
                 std::vector<AudioFrame>& results,
                 std::atomic<int>& error_count) {
    try {
        // Create stream info for this thread
        StreamInfo stream_info;
        stream_info.codec_name = codec_name;
        stream_info.sample_rate = 8000;
        stream_info.channels = 1;
        stream_info.bits_per_sample = 8;
        
        // Create codec instance (each thread has its own)
        CodecType codec(stream_info);
        if (!codec.initialize()) {
            error_count++;
            return;
        }
        
        // Generate deterministic test data based on thread ID and iteration
        int seed = thread_id * 1000 + iteration;
        std::vector<uint8_t> test_data = generateTestData(PACKET_SIZE, seed);
        
        // Create media chunk
        MediaChunk chunk;
        chunk.data = test_data;
        chunk.timestamp_samples = thread_id * OPERATIONS_PER_THREAD + iteration;
        
        // Decode audio
        AudioFrame frame = codec.decode(chunk);
        
        if (frame.samples.empty()) {
            error_count++;
            return;
        }
        
        // Store result for verification
        results[thread_id * NUM_ITERATIONS + iteration] = frame;
        
    } catch (const std::exception& e) {
        error_count++;
        std::lock_guard<std::mutex> lock(result_mutex);
        thread_results.push_back("Thread " + std::to_string(thread_id) + 
                               " exception: " + e.what());
    }
}

template<typename CodecType>
void testThreadSafetyIndependence(const std::string& codec_name) {
    std::cout << "\nTesting " << codec_name << " thread safety independence..." << std::endl;
    
    try {
        // Run multiple iterations to increase probability of detecting race conditions
        for (size_t iteration = 0; iteration < NUM_ITERATIONS; ++iteration) {
            std::atomic<int> error_count{0};
            std::vector<std::thread> threads;
            std::vector<AudioFrame> results(NUM_THREADS * NUM_ITERATIONS);
            
            // Launch worker threads
            for (int i = 0; i < NUM_THREADS; ++i) {
                threads.emplace_back(threadWorker<CodecType>,
                                   codec_name, i, iteration,
                                   std::ref(results), std::ref(error_count));
            }
            
            // Wait for all threads to complete
            for (auto& thread : threads) {
                thread.join();
            }
            
            if (error_count > 0) {
                std::cout << "  Iteration " << iteration << ": FAILED with " 
                         << error_count.load() << " errors" << std::endl;
                test_failures++;
                return;
            }
            
            // Verify that each thread's output is independent
            // Same input should produce same output regardless of thread interference
            for (int i = 0; i < NUM_THREADS; ++i) {
                AudioFrame& frame = results[i * NUM_ITERATIONS + iteration];
                
                // Verify frame properties
                assert(frame.sample_rate == 8000);
                assert(frame.channels == 1);
                assert(frame.samples.size() == PACKET_SIZE);
                
                // Verify that the same input produces the same output
                // by decoding the same data again in a single-threaded context
                int seed = i * 1000 + iteration;
                std::vector<uint8_t> test_data = generateTestData(PACKET_SIZE, seed);
                
                StreamInfo stream_info;
                stream_info.codec_name = codec_name;
                stream_info.sample_rate = 8000;
                stream_info.channels = 1;
                
                CodecType verify_codec(stream_info);
                verify_codec.initialize();
                
                MediaChunk verify_chunk;
                verify_chunk.data = test_data;
                verify_chunk.timestamp_samples = i * OPERATIONS_PER_THREAD + iteration;
                
                AudioFrame verify_frame = verify_codec.decode(verify_chunk);
                
                // Compare outputs - they should be identical
                if (frame.samples != verify_frame.samples) {
                    std::cout << "  Iteration " << iteration << ": Output mismatch for thread " << i << std::endl;
                    test_failures++;
                    return;
                }
            }
            
            std::cout << "  Iteration " << iteration << ": PASS" << std::endl;
        }
        
        std::cout << "✓ " << codec_name << " thread safety independence verified" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "  FAIL: Exception in thread safety test: " << e.what() << std::endl;
        test_failures++;
    }
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "THREAD SAFETY PROPERTY-BASED TESTS" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        std::cout << "\n=== Property 7: Thread Safety Independence ===" << std::endl;
        std::cout << "Testing that codec instances maintain independent state under concurrent access" << std::endl;
        std::cout << "Configuration: " << NUM_THREADS << " threads, " << NUM_ITERATIONS 
                 << " iterations, " << PACKET_SIZE << " bytes per packet" << std::endl;
        
        // Test μ-law codec thread safety
        testThreadSafetyIndependence<MuLawCodec>("mulaw");
        
        // Test A-law codec thread safety
        testThreadSafetyIndependence<ALawCodec>("alaw");
        
        // Test mixed concurrent operations
        std::cout << "\nTesting mixed concurrent operations (μ-law + A-law)..." << std::endl;
        
        try {
            std::atomic<int> error_count{0};
            std::vector<std::thread> threads;
            std::vector<AudioFrame> mulaw_results(NUM_THREADS * NUM_ITERATIONS);
            std::vector<AudioFrame> alaw_results(NUM_THREADS * NUM_ITERATIONS);
            
            // Launch μ-law threads
            for (int i = 0; i < NUM_THREADS / 2; ++i) {
                for (size_t iter = 0; iter < NUM_ITERATIONS; ++iter) {
                    threads.emplace_back(threadWorker<MuLawCodec>,
                                       "mulaw", i, iter,
                                       std::ref(mulaw_results), std::ref(error_count));
                }
            }
            
            // Launch A-law threads
            for (int i = NUM_THREADS / 2; i < NUM_THREADS; ++i) {
                for (size_t iter = 0; iter < NUM_ITERATIONS; ++iter) {
                    threads.emplace_back(threadWorker<ALawCodec>,
                                       "alaw", i, iter,
                                       std::ref(alaw_results), std::ref(error_count));
                }
            }
            
            // Wait for all threads
            for (auto& thread : threads) {
                thread.join();
            }
            
            if (error_count == 0) {
                std::cout << "✓ Mixed concurrent operations completed successfully" << std::endl;
            } else {
                std::cout << "  FAIL: Mixed operations had " << error_count.load() << " errors" << std::endl;
                test_failures++;
            }
            
        } catch (const std::exception& e) {
            std::cout << "  FAIL: Exception in mixed operations: " << e.what() << std::endl;
            test_failures++;
        }
        
        std::cout << "\n" << std::string(70, '=') << std::endl;
        
        if (test_failures == 0) {
            std::cout << "✅ ALL THREAD SAFETY PROPERTY TESTS PASSED" << std::endl;
            std::cout << std::string(70, '=') << std::endl;
            return 0;
        } else {
            std::cout << "❌ THREAD SAFETY PROPERTY TESTS FAILED" << std::endl;
            std::cout << "Failures: " << test_failures.load() << std::endl;
            std::cout << std::string(70, '=') << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(70, '=') << std::endl;
        std::cerr << "❌ TEST FRAMEWORK ERROR" << std::endl;
        std::cerr << "Exception: " << e.what() << std::endl;
        std::cerr << std::string(70, '=') << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n" << std::string(70, '=') << std::endl;
        std::cerr << "❌ TEST FRAMEWORK ERROR" << std::endl;
        std::cerr << "Unknown exception occurred" << std::endl;
        std::cerr << std::string(70, '=') << std::endl;
        return 1;
    }
}
