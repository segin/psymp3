/*
 * test_demuxer_performance.cpp - Performance and regression tests for demuxer architecture
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include <chrono>
#include <random>

using namespace TestFramework;
using namespace PsyMP3::IO;
using namespace PsyMP3::Demuxer;

/**
 * @brief Performance measurement utilities
 */
class PerformanceTimer {
private:
    std::chrono::high_resolution_clock::time_point m_start;
    
public:
    void start() {
        m_start = std::chrono::high_resolution_clock::now();
    }
    
    std::chrono::milliseconds elapsed() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - m_start);
    }
    
    double elapsedSeconds() const {
        return elapsed().count() / 1000.0;
    }
};

/**
 * @brief Generate large test data for performance testing
 */
class LargeTestDataGenerator {
public:
    /**
     * @brief Generate large RIFF/WAV file (configurable size)
     */
    static std::vector<uint8_t> generateLargeRIFFWAV(size_t duration_seconds = 10) {
        std::vector<uint8_t> data;
        
        // Calculate data size for given duration
        uint32_t sample_rate = 44100;
        uint16_t channels = 2;
        uint16_t bits_per_sample = 16;
        size_t bytes_per_second = sample_rate * channels * (bits_per_sample / 8);
        size_t audio_data_size = bytes_per_second * duration_seconds;
        
        // RIFF header
        data.insert(data.end(), {'R', 'I', 'F', 'F'});
        
        // File size (little-endian)
        uint32_t file_size = static_cast<uint32_t>(audio_data_size + 36);
        data.push_back(file_size & 0xFF);
        data.push_back((file_size >> 8) & 0xFF);
        data.push_back((file_size >> 16) & 0xFF);
        data.push_back((file_size >> 24) & 0xFF);
        
        // WAVE format
        data.insert(data.end(), {'W', 'A', 'V', 'E'});
        
        // fmt chunk
        data.insert(data.end(), {'f', 'm', 't', ' '});
        data.insert(data.end(), {0x10, 0x00, 0x00, 0x00}); // fmt chunk size
        data.insert(data.end(), {0x01, 0x00}); // PCM format
        data.push_back(channels & 0xFF);
        data.push_back((channels >> 8) & 0xFF);
        data.push_back(sample_rate & 0xFF);
        data.push_back((sample_rate >> 8) & 0xFF);
        data.push_back((sample_rate >> 16) & 0xFF);
        data.push_back((sample_rate >> 24) & 0xFF);
        
        uint32_t byte_rate = sample_rate * channels * (bits_per_sample / 8);
        data.push_back(byte_rate & 0xFF);
        data.push_back((byte_rate >> 8) & 0xFF);
        data.push_back((byte_rate >> 16) & 0xFF);
        data.push_back((byte_rate >> 24) & 0xFF);
        
        uint16_t block_align = channels * (bits_per_sample / 8);
        data.push_back(block_align & 0xFF);
        data.push_back((block_align >> 8) & 0xFF);
        data.push_back(bits_per_sample & 0xFF);
        data.push_back((bits_per_sample >> 8) & 0xFF);
        
        // data chunk
        data.insert(data.end(), {'d', 'a', 't', 'a'});
        data.push_back(audio_data_size & 0xFF);
        data.push_back((audio_data_size >> 8) & 0xFF);
        data.push_back((audio_data_size >> 16) & 0xFF);
        data.push_back((audio_data_size >> 24) & 0xFF);
        
        // Generate audio data with pseudo-random pattern for realistic size
        std::mt19937 rng(42); // Fixed seed for reproducible tests
        std::uniform_int_distribution<uint8_t> dist(0, 255);
        
        data.reserve(data.size() + audio_data_size);
        for (size_t i = 0; i < audio_data_size; ++i) {
            data.push_back(dist(rng));
        }
        
        return data;
    }
    
    /**
     * @brief Generate large Ogg file with multiple pages
     */
    static std::vector<uint8_t> generateLargeOgg(size_t num_pages = 100) {
        std::vector<uint8_t> data;
        
        for (size_t page = 0; page < num_pages; ++page) {
            // Ogg page header
            data.insert(data.end(), {'O', 'g', 'g', 'S'}); // capture pattern
            data.push_back(0x00); // version
            data.push_back(page == 0 ? 0x02 : 0x00); // header type
            
            // Granule position (8 bytes)
            uint64_t granule = page * 1024;
            for (int i = 0; i < 8; ++i) {
                data.push_back((granule >> (i * 8)) & 0xFF);
            }
            
            // Serial number
            data.insert(data.end(), {0x01, 0x00, 0x00, 0x00});
            
            // Page sequence number
            uint32_t seq = static_cast<uint32_t>(page);
            data.push_back(seq & 0xFF);
            data.push_back((seq >> 8) & 0xFF);
            data.push_back((seq >> 16) & 0xFF);
            data.push_back((seq >> 24) & 0xFF);
            
            // CRC checksum (simplified)
            data.insert(data.end(), {0x12, 0x34, 0x56, 0x78});
            
            // Number of segments (1-255)
            uint8_t num_segments = static_cast<uint8_t>(1 + (page % 10));
            data.push_back(num_segments);
            
            // Segment table
            size_t total_payload = 0;
            for (uint8_t seg = 0; seg < num_segments; ++seg) {
                uint8_t seg_size = static_cast<uint8_t>(200 + (seg * 5));
                data.push_back(seg_size);
                total_payload += seg_size;
            }
            
            // Payload data
            std::mt19937 rng(static_cast<uint32_t>(page + 1000));
            std::uniform_int_distribution<uint8_t> dist(0, 255);
            
            for (size_t i = 0; i < total_payload; ++i) {
                data.push_back(dist(rng));
            }
        }
        
        return data;
    }
};

/**
 * @brief Mock IOHandler for performance testing
 */
class PerformanceIOHandler : public IOHandler {
private:
    std::vector<uint8_t> m_data;
    size_t m_position = 0;
    mutable std::atomic<size_t> m_read_count{0};
    mutable std::atomic<size_t> m_seek_count{0};
    
public:
    explicit PerformanceIOHandler(const std::vector<uint8_t>& data) : m_data(data) {}
    
    size_t getReadCount() const { return m_read_count.load(); }
    size_t getSeekCount() const { return m_seek_count.load(); }
    void resetCounters() { m_read_count = 0; m_seek_count = 0; }
    
    size_t read(void* buffer, size_t size, size_t count) override {
        m_read_count++;
        size_t bytes_to_read = std::min(size * count, m_data.size() - m_position);
        if (bytes_to_read > 0) {
            std::memcpy(buffer, m_data.data() + m_position, bytes_to_read);
            m_position += bytes_to_read;
        }
        return bytes_to_read / size;
    }
    
    int seek(long offset, int whence) override {
        m_seek_count++;
        long new_pos;
        switch (whence) {
            case SEEK_SET: new_pos = offset; break;
            case SEEK_CUR: new_pos = static_cast<long>(m_position) + offset; break;
            case SEEK_END: new_pos = static_cast<long>(m_data.size()) + offset; break;
            default: return -1;
        }
        
        if (new_pos < 0 || new_pos > static_cast<long>(m_data.size())) {
            return -1;
        }
        
        m_position = static_cast<size_t>(new_pos);
        return 0;
    }
    
    long tell() override {
        return static_cast<long>(m_position);
    }
    
    bool eof() override {
        return m_position >= m_data.size();
    }
    
    int close() override { return 0; }
};

/**
 * @brief Test demuxer parsing performance with large files
 */
class DemuxerParsingPerformanceTest : public TestCase {
public:
    DemuxerParsingPerformanceTest() : TestCase("Demuxer Parsing Performance Test") {}
    
protected:
    void runTest() override {
        PerformanceTimer timer;
        
        // Test RIFF parsing performance
        std::cout << "Testing RIFF parsing performance..." << std::endl;
        auto large_wav = LargeTestDataGenerator::generateLargeRIFFWAV(30); // 30 seconds
        std::cout << "Generated " << large_wav.size() << " bytes of WAV data" << std::endl;
        
        auto wav_handler = std::make_unique<PerformanceIOHandler>(large_wav);
        auto wav_demuxer = DemuxerFactory::createDemuxer(std::move(wav_handler));
        ASSERT_NOT_NULL(wav_demuxer.get(), "RIFF demuxer should be created");
        
        timer.start();
        bool parse_result = wav_demuxer->parseContainer();
        auto parse_time = timer.elapsed();
        
        ASSERT_TRUE(parse_result, "Large RIFF file should parse successfully");
        std::cout << "RIFF parsing took " << parse_time.count() << "ms" << std::endl;
        
        // Performance benchmark: parsing should complete within reasonable time
        ASSERT_TRUE(parse_time.count() < 1000, "RIFF parsing should complete within 1 second");
        
        // Test Ogg parsing performance
        std::cout << "Testing Ogg parsing performance..." << std::endl;
        auto large_ogg = LargeTestDataGenerator::generateLargeOgg(500); // 500 pages
        std::cout << "Generated " << large_ogg.size() << " bytes of Ogg data" << std::endl;
        
        auto ogg_handler = std::make_unique<PerformanceIOHandler>(large_ogg);
        auto ogg_demuxer = DemuxerFactory::createDemuxer(std::move(ogg_handler));
        ASSERT_NOT_NULL(ogg_demuxer.get(), "Ogg demuxer should be created");
        
        timer.start();
        bool ogg_parse_result = ogg_demuxer->parseContainer();
        auto ogg_parse_time = timer.elapsed();
        
        ASSERT_TRUE(ogg_parse_result, "Large Ogg file should parse successfully");
        std::cout << "Ogg parsing took " << ogg_parse_time.count() << "ms" << std::endl;
        
        // Ogg parsing may be more complex due to page structure
        ASSERT_TRUE(ogg_parse_time.count() < 2000, "Ogg parsing should complete within 2 seconds");
    }
};

/**
 * @brief Test chunk reading performance
 */
class ChunkReadingPerformanceTest : public TestCase {
public:
    ChunkReadingPerformanceTest() : TestCase("Chunk Reading Performance Test") {}
    
protected:
    void runTest() override {
        // Generate test data
        auto wav_data = LargeTestDataGenerator::generateLargeRIFFWAV(10); // 10 seconds
        auto handler = std::make_unique<PerformanceIOHandler>(wav_data);
        auto perf_handler = handler.get(); // Keep reference for metrics
        
        auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
        ASSERT_NOT_NULL(demuxer.get(), "Demuxer should be created");
        ASSERT_TRUE(demuxer->parseContainer(), "Container should parse");
        
        PerformanceTimer timer;
        timer.start();
        
        size_t chunks_read = 0;
        size_t total_bytes = 0;
        
        // Read all chunks and measure performance
        while (!demuxer->isEOF() && chunks_read < 10000) { // Limit to prevent infinite loop
            auto chunk = demuxer->readChunk();
            if (chunk.isValid()) {
                chunks_read++;
                total_bytes += chunk.data.size();
            } else {
                break;
            }
        }
        
        auto read_time = timer.elapsed();
        
        std::cout << "Read " << chunks_read << " chunks (" << total_bytes << " bytes) in " 
                  << read_time.count() << "ms" << std::endl;
        std::cout << "I/O operations: " << perf_handler->getReadCount() << " reads, " 
                  << perf_handler->getSeekCount() << " seeks" << std::endl;
        
        ASSERT_TRUE(chunks_read > 0, "Should read some chunks");
        ASSERT_TRUE(total_bytes > 0, "Should read some data");
        
        // Performance benchmarks
        if (chunks_read > 0) {
            double chunks_per_second = (chunks_read * 1000.0) / read_time.count();
            double mbytes_per_second = (total_bytes / (1024.0 * 1024.0)) / (read_time.count() / 1000.0);
            
            std::cout << "Performance: " << chunks_per_second << " chunks/sec, " 
                      << mbytes_per_second << " MB/sec" << std::endl;
            
            ASSERT_TRUE(chunks_per_second > 100, "Should read at least 100 chunks per second");
            ASSERT_TRUE(mbytes_per_second > 1.0, "Should read at least 1 MB per second");
        }
    }
};

/**
 * @brief Test seeking performance
 */
class SeekingPerformanceTest : public TestCase {
public:
    SeekingPerformanceTest() : TestCase("Seeking Performance Test") {}
    
protected:
    void runTest() override {
        // Generate test data
        auto wav_data = LargeTestDataGenerator::generateLargeRIFFWAV(60); // 1 minute
        auto handler = std::make_unique<PerformanceIOHandler>(wav_data);
        auto perf_handler = handler.get();
        
        auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
        ASSERT_NOT_NULL(demuxer.get(), "Demuxer should be created");
        ASSERT_TRUE(demuxer->parseContainer(), "Container should parse");
        
        uint64_t duration = demuxer->getDuration();
        ASSERT_TRUE(duration > 0, "Should have valid duration");
        
        PerformanceTimer timer;
        timer.start();
        
        // Perform multiple seeks to different positions
        std::vector<uint64_t> seek_positions = {
            0,                    // Beginning
            duration / 4,         // 25%
            duration / 2,         // 50%
            duration * 3 / 4,     // 75%
            duration - 1000,      // Near end
            duration / 3,         // 33%
            duration * 2 / 3,     // 66%
            0                     // Back to beginning
        };
        
        size_t successful_seeks = 0;
        for (uint64_t pos : seek_positions) {
            if (demuxer->seekTo(pos)) {
                successful_seeks++;
                ASSERT_EQUALS(pos, demuxer->getPosition(), "Position should be updated correctly");
                
                // Verify we can read after seeking
                auto chunk = demuxer->readChunk();
                // Chunk may or may not be valid depending on position, but should not crash
            }
        }
        
        auto seek_time = timer.elapsed();
        
        std::cout << "Performed " << successful_seeks << "/" << seek_positions.size() 
                  << " seeks in " << seek_time.count() << "ms" << std::endl;
        std::cout << "I/O seeks: " << perf_handler->getSeekCount() << std::endl;
        
        ASSERT_TRUE(successful_seeks >= seek_positions.size() / 2, "Most seeks should succeed");
        
        // Performance benchmark: seeks should be fast
        if (successful_seeks > 0) {
            double avg_seek_time = static_cast<double>(seek_time.count()) / successful_seeks;
            std::cout << "Average seek time: " << avg_seek_time << "ms" << std::endl;
            
            ASSERT_TRUE(avg_seek_time < 50.0, "Average seek should be under 50ms");
        }
    }
};

/**
 * @brief Test memory usage during processing
 */
class MemoryUsageTest : public TestCase {
public:
    MemoryUsageTest() : TestCase("Memory Usage Test") {}
    
protected:
    void runTest() override {
        // Test buffer pool efficiency
        auto initial_stats = BufferPool::getInstance().getStats();
        
        // Generate large test data
        auto wav_data = LargeTestDataGenerator::generateLargeRIFFWAV(30);
        auto handler = std::make_unique<PerformanceIOHandler>(wav_data);
        
        auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
        ASSERT_NOT_NULL(demuxer.get(), "Demuxer should be created");
        ASSERT_TRUE(demuxer->parseContainer(), "Container should parse");
        
        // Read chunks and monitor memory usage
        std::vector<MediaChunk> chunks;
        size_t max_chunks_held = 100; // Limit memory usage
        
        for (size_t i = 0; i < 1000 && !demuxer->isEOF(); ++i) {
            auto chunk = demuxer->readChunk();
            if (chunk.isValid()) {
                chunks.push_back(std::move(chunk));
                
                // Periodically release old chunks to test memory management
                if (chunks.size() > max_chunks_held) {
                    chunks.erase(chunks.begin(), chunks.begin() + 50);
                }
            }
        }
        
        auto final_stats = BufferPool::getInstance().getStats();
        
        std::cout << "Buffer pool stats:" << std::endl;
        std::cout << "  Initial: " << initial_stats.total_buffers << " buffers, " 
                  << initial_stats.total_memory_bytes << " bytes" << std::endl;
        std::cout << "  Final: " << final_stats.total_buffers << " buffers, " 
                  << final_stats.total_memory_bytes << " bytes" << std::endl;
        
        // Memory usage should be reasonable
        ASSERT_TRUE(final_stats.total_memory_bytes < 10 * 1024 * 1024, "Should use less than 10MB");
        
        // Clear chunks to test cleanup
        chunks.clear();
        
        // Force buffer pool cleanup
        BufferPool::getInstance().clear();
        
        auto cleanup_stats = BufferPool::getInstance().getStats();
        std::cout << "  After cleanup: " << cleanup_stats.total_buffers << " buffers, " 
                  << cleanup_stats.total_memory_bytes << " bytes" << std::endl;
    }
};

/**
 * @brief Test concurrent access performance
 */
class ConcurrentAccessPerformanceTest : public TestCase {
public:
    ConcurrentAccessPerformanceTest() : TestCase("Concurrent Access Performance Test") {}
    
protected:
    void runTest() override {
        // Generate test data
        auto wav_data = LargeTestDataGenerator::generateLargeRIFFWAV(20);
        auto handler = std::make_unique<PerformanceIOHandler>(wav_data);
        
        auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
        ASSERT_NOT_NULL(demuxer.get(), "Demuxer should be created");
        ASSERT_TRUE(demuxer->parseContainer(), "Container should parse");
        
        std::atomic<size_t> total_chunks_read{0};
        std::atomic<size_t> total_seeks{0};
        std::atomic<bool> test_failed{false};
        
        PerformanceTimer timer;
        timer.start();
        
        // Worker function for concurrent access
        auto worker = [&](int worker_id) {
            try {
                size_t chunks_read = 0;
                size_t seeks_performed = 0;
                
                for (int i = 0; i < 100; ++i) {
                    // Mix of reading and seeking
                    if (i % 10 == 0) {
                        // Perform seek
                        uint64_t seek_pos = (demuxer->getDuration() * (i % 100)) / 100;
                        if (demuxer->seekTo(seek_pos)) {
                            seeks_performed++;
                        }
                    } else {
                        // Read chunk
                        auto chunk = demuxer->readChunk();
                        if (chunk.isValid()) {
                            chunks_read++;
                        }
                    }
                    
                    // Small delay to encourage thread interleaving
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
                
                total_chunks_read += chunks_read;
                total_seeks += seeks_performed;
                
            } catch (...) {
                test_failed = true;
            }
        };
        
        // Run multiple workers concurrently
        std::vector<std::thread> workers;
        for (int i = 0; i < 4; ++i) {
            workers.emplace_back(worker, i);
        }
        
        // Wait for all workers to complete
        for (auto& t : workers) {
            t.join();
        }
        
        auto concurrent_time = timer.elapsed();
        
        std::cout << "Concurrent test results:" << std::endl;
        std::cout << "  Time: " << concurrent_time.count() << "ms" << std::endl;
        std::cout << "  Total chunks read: " << total_chunks_read.load() << std::endl;
        std::cout << "  Total seeks: " << total_seeks.load() << std::endl;
        
        ASSERT_FALSE(test_failed.load(), "Concurrent access should not fail");
        ASSERT_TRUE(total_chunks_read.load() > 0, "Should read some chunks concurrently");
        
        // Performance should not degrade too much with concurrency
        ASSERT_TRUE(concurrent_time.count() < 30000, "Concurrent test should complete within 30 seconds");
    }
};

/**
 * @brief Regression test for known issues
 */
class RegressionTest : public TestCase {
public:
    RegressionTest() : TestCase("Regression Test") {}
    
protected:
    void runTest() override {
        // Test case 1: Empty file handling
        std::vector<uint8_t> empty_data;
        auto empty_handler = std::make_unique<PerformanceIOHandler>(empty_data);
        auto empty_demuxer = DemuxerFactory::createDemuxer(std::move(empty_handler));
        
        if (empty_demuxer) {
            ASSERT_FALSE(empty_demuxer->parseContainer(), "Empty file should not parse successfully");
            ASSERT_FALSE(empty_demuxer->isParsed(), "Empty file should not be marked as parsed");
        }
        
        // Test case 2: Malformed header handling
        std::vector<uint8_t> malformed_data = {0x52, 0x49, 0x46, 0x46}; // Just "RIFF"
        auto malformed_handler = std::make_unique<PerformanceIOHandler>(malformed_data);
        auto malformed_demuxer = DemuxerFactory::createDemuxer(std::move(malformed_handler));
        
        if (malformed_demuxer) {
            // Should handle gracefully without crashing
            bool parse_result = malformed_demuxer->parseContainer();
            // May succeed or fail, but should not crash
            
            if (!parse_result) {
                ASSERT_TRUE(malformed_demuxer->hasError(), "Should have error information");
            }
        }
        
        // Test case 3: Large seek position handling
        auto wav_data = LargeTestDataGenerator::generateLargeRIFFWAV(5);
        auto wav_handler = std::make_unique<PerformanceIOHandler>(wav_data);
        auto wav_demuxer = DemuxerFactory::createDemuxer(std::move(wav_handler));
        
        if (wav_demuxer && wav_demuxer->parseContainer()) {
            uint64_t duration = wav_demuxer->getDuration();
            
            // Test seeking beyond end
            bool seek_result = wav_demuxer->seekTo(duration * 2);
            if (seek_result) {
                ASSERT_TRUE(wav_demuxer->isEOF(), "Seeking beyond end should set EOF");
            }
            
            // Test seeking to maximum value
            seek_result = wav_demuxer->seekTo(UINT64_MAX);
            // Should handle gracefully without crashing
        }
        
        // Test case 4: Rapid seek operations
        if (wav_demuxer) {
            uint64_t duration = wav_demuxer->getDuration();
            
            // Perform rapid seeks
            for (int i = 0; i < 50; ++i) {
                uint64_t pos = (duration * i) / 50;
                wav_demuxer->seekTo(pos);
                // Should not crash or corrupt state
            }
            
            // Verify demuxer is still functional
            auto chunk = wav_demuxer->readChunk();
            // May or may not be valid, but should not crash
        }
        
        // Test case 5: Buffer pool stress test
        BufferPool::getInstance().clear();
        
        std::vector<std::vector<uint8_t>> buffers;
        for (int i = 0; i < 1000; ++i) {
            auto buffer = BufferPool::getInstance().getBuffer(1024);
            buffers.push_back(std::move(buffer));
        }
        
        // Return buffers
        for (auto& buffer : buffers) {
            BufferPool::getInstance().returnBuffer(std::move(buffer));
        }
        
        auto pool_stats = BufferPool::getInstance().getStats();
        std::cout << "Buffer pool after stress test: " << pool_stats.total_buffers 
                  << " buffers, " << pool_stats.total_memory_bytes << " bytes" << std::endl;
        
        // Should not have excessive memory usage
        ASSERT_TRUE(pool_stats.total_memory_bytes < 50 * 1024 * 1024, "Buffer pool should not use excessive memory");
    }
};

/**
 * @brief Scalability test with multiple streams
 */
class ScalabilityTest : public TestCase {
public:
    ScalabilityTest() : TestCase("Scalability Test") {}
    
protected:
    void runTest() override {
        PerformanceTimer timer;
        timer.start();
        
        // Create multiple demuxers simultaneously
        std::vector<std::unique_ptr<Demuxer>> demuxers;
        const size_t num_demuxers = 10;
        
        for (size_t i = 0; i < num_demuxers; ++i) {
            auto data = LargeTestDataGenerator::generateLargeRIFFWAV(5);
            auto handler = std::make_unique<PerformanceIOHandler>(data);
            auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
            
            if (demuxer) {
                ASSERT_TRUE(demuxer->parseContainer(), "Each demuxer should parse successfully");
                demuxers.push_back(std::move(demuxer));
            }
        }
        
        auto creation_time = timer.elapsed();
        std::cout << "Created " << demuxers.size() << " demuxers in " << creation_time.count() << "ms" << std::endl;
        
        // Test concurrent operations on all demuxers
        timer.start();
        
        std::atomic<size_t> total_operations{0};
        std::atomic<bool> test_failed{false};
        
        auto worker = [&](size_t start_idx, size_t end_idx) {
            try {
                size_t operations = 0;
                
                for (size_t i = start_idx; i < end_idx && i < demuxers.size(); ++i) {
                    auto& demuxer = demuxers[i];
                    
                    // Perform various operations
                    for (int op = 0; op < 20; ++op) {
                        switch (op % 4) {
                            case 0: {
                                auto chunk = demuxer->readChunk();
                                operations++;
                                break;
                            }
                            case 1: {
                                uint64_t pos = (demuxer->getDuration() * op) / 20;
                                demuxer->seekTo(pos);
                                operations++;
                                break;
                            }
                            case 2: {
                                demuxer->getPosition();
                                demuxer->isEOF();
                                operations++;
                                break;
                            }
                            case 3: {
                                auto streams = demuxer->getStreams();
                                operations++;
                                break;
                            }
                        }
                    }
                }
                
                total_operations += operations;
                
            } catch (...) {
                test_failed = true;
            }
        };
        
        // Run workers on different subsets of demuxers
        std::vector<std::thread> workers;
        size_t demuxers_per_worker = std::max(1UL, demuxers.size() / 4);
        
        for (size_t i = 0; i < demuxers.size(); i += demuxers_per_worker) {
            workers.emplace_back(worker, i, i + demuxers_per_worker);
        }
        
        for (auto& t : workers) {
            t.join();
        }
        
        auto operation_time = timer.elapsed();
        
        std::cout << "Performed " << total_operations.load() << " operations on " 
                  << demuxers.size() << " demuxers in " << operation_time.count() << "ms" << std::endl;
        
        ASSERT_FALSE(test_failed.load(), "Scalability test should not fail");
        ASSERT_TRUE(total_operations.load() > 0, "Should perform operations");
        
        // Performance should scale reasonably
        if (total_operations.load() > 0) {
            double ops_per_second = (total_operations.load() * 1000.0) / operation_time.count();
            std::cout << "Performance: " << ops_per_second << " operations/second" << std::endl;
            
            ASSERT_TRUE(ops_per_second > 100, "Should maintain reasonable performance with multiple demuxers");
        }
    }
};

int main() {
    TestSuite suite("Demuxer Performance and Regression Tests");
    
    // Add all test cases
    suite.addTest(std::make_unique<DemuxerParsingPerformanceTest>());
    suite.addTest(std::make_unique<ChunkReadingPerformanceTest>());
    suite.addTest(std::make_unique<SeekingPerformanceTest>());
    suite.addTest(std::make_unique<MemoryUsageTest>());
    suite.addTest(std::make_unique<ConcurrentAccessPerformanceTest>());
    suite.addTest(std::make_unique<RegressionTest>());
    suite.addTest(std::make_unique<ScalabilityTest>());
    
    // Run all tests
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results);
}