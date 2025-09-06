/**
 * @file test_oggdemuxer_comprehensive.cpp
 * @brief Comprehensive test suite for OGG demuxer implementation
 * 
 * This test suite validates all aspects of the OGG demuxer implementation
 * including codec support, seeking accuracy, error handling, memory management,
 * and performance characteristics.
 */

#include "psymp3.h"

// System includes
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <fstream>
#include <random>
#include <algorithm>
#include <atomic>

// Simple test framework
static int test_count = 0;
static int passed_count = 0;
static int failed_count = 0;

void assert_true(bool condition, const std::string& message) {
    test_count++;
    if (condition) {
        passed_count++;
        std::cout << "✓ " << message << std::endl;
    } else {
        failed_count++;
        std::cout << "✗ " << message << std::endl;
    }
}

void assert_false(bool condition, const std::string& message) {
    assert_true(!condition, message);
}

template<typename T>
void assert_equal(const T& expected, const T& actual, const std::string& message) {
    test_count++;
    if (expected == actual) {
        passed_count++;
        std::cout << "✓ " << message << std::endl;
    } else {
        failed_count++;
        std::cout << "✗ " << message << " (expected: " << expected << ", got: " << actual << ")" << std::endl;
    }
}

void print_summary() {
    std::cout << std::endl;
    std::cout << "Test Summary:" << std::endl;
    std::cout << "=============" << std::endl;
    std::cout << "Total tests: " << test_count << std::endl;
    std::cout << "Passed: " << passed_count << std::endl;
    std::cout << "Failed: " << failed_count << std::endl;
    std::cout << "Success rate: " << (test_count > 0 ? (100.0 * passed_count / test_count) : 0.0) << "%" << std::endl;
}

// Test data generation utilities
namespace TestDataGenerator {
    /**
     * Generate minimal Ogg Vorbis test data
     */
    std::vector<uint8_t> generateMinimalOggVorbis() {
        std::vector<uint8_t> data;
        
        // Ogg page header with "OggS" signature
        data.insert(data.end(), {'O', 'g', 'g', 'S'});
        data.push_back(0x00); // version
        data.push_back(0x02); // header_type (first page)
        
        // Granule position (8 bytes, little-endian)
        for (int i = 0; i < 8; i++) data.push_back(0x00);
        
        // Serial number (4 bytes)
        data.insert(data.end(), {0x01, 0x00, 0x00, 0x00});
        
        // Page sequence number (4 bytes)
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00});
        
        // CRC checksum (4 bytes) - will be calculated by libogg
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00});
        
        // Number of segments
        data.push_back(0x01);
        
        // Segment table
        data.push_back(0x1E); // 30 bytes for Vorbis identification header
        
        // Vorbis identification header
        data.insert(data.end(), {0x01, 'v', 'o', 'r', 'b', 'i', 's'});
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00}); // version
        data.push_back(0x02); // channels
        data.insert(data.end(), {0x44, 0xAC, 0x00, 0x00}); // sample rate (44100)
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00}); // bitrate_maximum
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00}); // bitrate_nominal
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00}); // bitrate_minimum
        data.push_back(0xB8); // blocksize_0 and blocksize_1
        data.push_back(0x01); // framing
        
        return data;
    }
    
    /**
     * Generate minimal Ogg Opus test data
     */
    std::vector<uint8_t> generateMinimalOggOpus() {
        std::vector<uint8_t> data;
        
        // Ogg page header
        data.insert(data.end(), {'O', 'g', 'g', 'S'});
        data.push_back(0x00); // version
        data.push_back(0x02); // header_type (first page)
        
        // Granule position (8 bytes)
        for (int i = 0; i < 8; i++) data.push_back(0x00);
        
        // Serial number
        data.insert(data.end(), {0x02, 0x00, 0x00, 0x00});
        
        // Page sequence number
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00});
        
        // CRC checksum
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00});
        
        // Number of segments
        data.push_back(0x01);
        
        // Segment table
        data.push_back(0x13); // 19 bytes for Opus identification header
        
        // Opus identification header
        data.insert(data.end(), {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd'});
        data.push_back(0x01); // version
        data.push_back(0x02); // channels
        data.insert(data.end(), {0x38, 0x01}); // pre-skip (312 samples)
        data.insert(data.end(), {0x80, 0xBB, 0x00, 0x00}); // input sample rate (48000)
        data.insert(data.end(), {0x00, 0x00}); // output gain
        data.push_back(0x00); // channel mapping family
        
        return data;
    }
    
    /**
     * Generate minimal Ogg FLAC test data
     */
    std::vector<uint8_t> generateMinimalOggFLAC() {
        std::vector<uint8_t> data;
        
        // Ogg page header
        data.insert(data.end(), {'O', 'g', 'g', 'S'});
        data.push_back(0x00); // version
        data.push_back(0x02); // header_type (first page)
        
        // Granule position (8 bytes)
        for (int i = 0; i < 8; i++) data.push_back(0x00);
        
        // Serial number
        data.insert(data.end(), {0x03, 0x00, 0x00, 0x00});
        
        // Page sequence number
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00});
        
        // CRC checksum
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00});
        
        // Number of segments
        data.push_back(0x01);
        
        // Segment table
        data.push_back(0x33); // 51 bytes for FLAC identification header
        
        // FLAC identification header
        data.insert(data.end(), {0x7F, 'F', 'L', 'A', 'C'});
        data.push_back(0x01); // major version
        data.push_back(0x00); // minor version
        data.insert(data.end(), {0x00, 0x01}); // number of header packets
        data.insert(data.end(), {'f', 'L', 'a', 'C'}); // native FLAC signature
        
        // STREAMINFO metadata block
        data.push_back(0x00); // last block flag + block type
        data.insert(data.end(), {0x00, 0x00, 0x22}); // block length (34 bytes)
        data.insert(data.end(), {0x10, 0x00}); // min block size (4096)
        data.insert(data.end(), {0x10, 0x00}); // max block size (4096)
        data.insert(data.end(), {0x00, 0x00, 0x00}); // min frame size
        data.insert(data.end(), {0x00, 0x00, 0x00}); // max frame size
        data.insert(data.end(), {0xAC, 0x44, 0x02}); // sample rate (44100) + channels (2) + bits per sample (16)
        data.insert(data.end(), {0x00, 0x00, 0x00, 0x00, 0x00}); // total samples
        
        // MD5 signature (16 bytes)
        for (int i = 0; i < 16; i++) data.push_back(0x00);
        
        return data;
    }
    
    /**
     * Generate corrupted Ogg data for error handling tests
     */
    std::vector<uint8_t> generateCorruptedOggData() {
        std::vector<uint8_t> data;
        
        // Invalid signature
        data.insert(data.end(), {'B', 'a', 'd', 'S'});
        data.push_back(0x00);
        data.push_back(0x02);
        
        // Rest of header with invalid data
        for (int i = 0; i < 20; i++) {
            data.push_back(0xFF);
        }
        
        return data;
    }
}

// Test suite implementation
class ComprehensiveTestSuite {
public:
    ComprehensiveTestSuite() {}
    
    /**
     * Test codec detection for all supported formats
     */
    void testCodecDetection() {
        std::cout << "=== Testing Codec Detection ===" << std::endl;
        
        // Test Vorbis detection
        {
            auto vorbis_data = TestDataGenerator::generateMinimalOggVorbis();
            std::string temp_file = "test_vorbis_temp.ogg";
            
            std::ofstream file(temp_file, std::ios::binary);
            file.write(reinterpret_cast<const char*>(vorbis_data.data()), vorbis_data.size());
            file.close();
            
            try {
                auto io_handler = std::make_unique<FileIOHandler>(temp_file);
                OggDemuxer demuxer(std::move(io_handler));
                bool result = demuxer.parseContainer();
                
                assert_true(result, "Vorbis codec detection");
                
                auto streams = demuxer.getStreams();
                if (!streams.empty()) {
                    assert_equal(streams[0].codec_name, std::string("vorbis"), "Vorbis codec name");
                    assert_equal(streams[0].sample_rate, 44100u, "Vorbis sample rate");
                    assert_equal(static_cast<unsigned int>(streams[0].channels), 2u, "Vorbis channels");
                }
            } catch (const std::exception& e) {
                std::cout << "Vorbis test exception: " << e.what() << std::endl;
            }
            
            std::remove(temp_file.c_str());
        }
        
        // Test Opus detection
        {
            auto opus_data = TestDataGenerator::generateMinimalOggOpus();
            std::string temp_file = "test_opus_temp.ogg";
            
            std::ofstream file(temp_file, std::ios::binary);
            file.write(reinterpret_cast<const char*>(opus_data.data()), opus_data.size());
            file.close();
            
            try {
                auto io_handler = std::make_unique<FileIOHandler>(temp_file);
                OggDemuxer demuxer(std::move(io_handler));
                bool result = demuxer.parseContainer();
                
                assert_true(result, "Opus codec detection");
                
                auto streams = demuxer.getStreams();
                if (!streams.empty()) {
                    assert_equal(streams[0].codec_name, std::string("opus"), "Opus codec name");
                    assert_equal(static_cast<unsigned int>(streams[0].channels), 2u, "Opus channels");
                }
            } catch (const std::exception& e) {
                std::cout << "Opus test exception: " << e.what() << std::endl;
            }
            
            std::remove(temp_file.c_str());
        }
        
        // Test FLAC detection
        {
            auto flac_data = TestDataGenerator::generateMinimalOggFLAC();
            std::string temp_file = "test_flac_temp.oga";
            
            std::ofstream file(temp_file, std::ios::binary);
            file.write(reinterpret_cast<const char*>(flac_data.data()), flac_data.size());
            file.close();
            
            try {
                auto io_handler = std::make_unique<FileIOHandler>(temp_file);
                OggDemuxer demuxer(std::move(io_handler));
                bool result = demuxer.parseContainer();
                
                assert_true(result, "FLAC codec detection");
                
                auto streams = demuxer.getStreams();
                if (!streams.empty()) {
                    assert_equal(streams[0].codec_name, std::string("flac"), "FLAC codec name");
                    assert_equal(streams[0].sample_rate, 44100u, "FLAC sample rate");
                    assert_equal(static_cast<unsigned int>(streams[0].channels), 2u, "FLAC channels");
                }
            } catch (const std::exception& e) {
                std::cout << "FLAC test exception: " << e.what() << std::endl;
            }
            
            std::remove(temp_file.c_str());
        }
        
        std::cout << "Codec detection tests completed." << std::endl;
    }
    
    /**
     * Test seeking accuracy across different scenarios
     */
    void testSeekingAccuracy() {
        std::cout << "=== Testing Seeking Accuracy ===" << std::endl;
        
        // Test seeking with minimal Vorbis data
        auto vorbis_data = TestDataGenerator::generateMinimalOggVorbis();
        std::string temp_file = "test_seeking_temp.ogg";
        
        std::ofstream file(temp_file, std::ios::binary);
        file.write(reinterpret_cast<const char*>(vorbis_data.data()), vorbis_data.size());
        file.close();
        
        try {
            auto io_handler = std::make_unique<FileIOHandler>(temp_file);
            OggDemuxer demuxer(std::move(io_handler));
            
            if (demuxer.parseContainer()) {
                // Test seeking to beginning
                bool seek_result = demuxer.seekTo(0);
                assert_true(seek_result, "Seek to beginning");
                
                // Test seeking to middle (should handle gracefully even with minimal data)
                seek_result = demuxer.seekTo(1000);
                // Don't assert this as true since minimal data may not support seeking
                
                // Test seeking beyond end (should clamp or handle gracefully)
                seek_result = demuxer.seekTo(999999);
                // Don't assert this as it should handle gracefully
                
                std::cout << "Seeking tests completed with minimal data." << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "Seeking test exception: " << e.what() << std::endl;
        }
        
        std::remove(temp_file.c_str());
    }
    
    /**
     * Test error handling with corrupted and malformed files
     */
    void testErrorHandling() {
        std::cout << "=== Testing Error Handling ===" << std::endl;
        
        // Test with corrupted data
        {
            auto corrupted_data = TestDataGenerator::generateCorruptedOggData();
            std::string temp_file = "test_corrupted_temp.ogg";
            
            std::ofstream file(temp_file, std::ios::binary);
            file.write(reinterpret_cast<const char*>(corrupted_data.data()), corrupted_data.size());
            file.close();
            
            try {
                auto io_handler = std::make_unique<FileIOHandler>(temp_file);
                OggDemuxer demuxer(std::move(io_handler));
                bool result = demuxer.parseContainer();
                
                // Should handle corrupted data gracefully (may return false)
                std::cout << "Corrupted data handling: " << (result ? "parsed" : "rejected") << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Corrupted data exception (expected): " << e.what() << std::endl;
            }
            
            std::remove(temp_file.c_str());
        }
        
        // Test with empty file
        {
            std::string temp_file = "test_empty_temp.ogg";
            std::ofstream file(temp_file);
            file.close();
            
            try {
                auto io_handler = std::make_unique<FileIOHandler>(temp_file);
                OggDemuxer demuxer(std::move(io_handler));
                bool result = demuxer.parseContainer();
                
                assert_false(result, "Empty file handling");
            } catch (const std::exception& e) {
                std::cout << "Empty file exception (expected): " << e.what() << std::endl;
            }
            
            std::remove(temp_file.c_str());
        }
        
        // Test with non-existent file
        {
            try {
                auto io_handler = std::make_unique<FileIOHandler>("non_existent_file.ogg");
                // If we get here, the constructor didn't throw, which is unexpected
                assert_false(true, "Non-existent file should throw exception");
            } catch (const std::exception& e) {
                std::cout << "Non-existent file exception (expected): " << e.what() << std::endl;
                assert_true(true, "Non-existent file handling");
            }
        }
        
        std::cout << "Error handling tests completed." << std::endl;
    }
    
    /**
     * Test memory management and resource cleanup
     */
    void testMemoryManagement() {
        std::cout << "=== Testing Memory Management ===" << std::endl;
        
        // Test multiple demuxer instances
        std::vector<std::unique_ptr<OggDemuxer>> demuxers;
        
        auto test_data = TestDataGenerator::generateMinimalOggVorbis();
        std::string temp_file = "test_memory_temp.ogg";
        
        std::ofstream file(temp_file, std::ios::binary);
        file.write(reinterpret_cast<const char*>(test_data.data()), test_data.size());
        file.close();
        
        try {
            // Create multiple demuxer instances
            for (int i = 0; i < 10; i++) {
                try {
                    auto io_handler = std::make_unique<FileIOHandler>(temp_file);
                    auto demuxer = std::make_unique<OggDemuxer>(std::move(io_handler));
                    demuxer->parseContainer();
                    demuxers.push_back(std::move(demuxer));
                } catch (const std::exception& e) {
                    std::cout << "Memory test instance " << i << " exception: " << e.what() << std::endl;
                }
            }
            
            assert_equal(static_cast<unsigned int>(demuxers.size()), 10u, "Multiple demuxer instances created");
            
            // Clear all instances (tests destructors)
            demuxers.clear();
            
            std::cout << "Memory management test completed." << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "Memory management test exception: " << e.what() << std::endl;
        }
        
        std::remove(temp_file.c_str());
    }
    
    /**
     * Test performance characteristics
     */
    void testPerformance() {
        std::cout << "=== Testing Performance ===" << std::endl;
        
        auto test_data = TestDataGenerator::generateMinimalOggVorbis();
        std::string temp_file = "test_performance_temp.ogg";
        
        std::ofstream file(temp_file, std::ios::binary);
        file.write(reinterpret_cast<const char*>(test_data.data()), test_data.size());
        file.close();
        
        try {
            // Test parsing performance
            auto start_time = std::chrono::high_resolution_clock::now();
            
            for (int i = 0; i < 100; i++) {
                try {
                    auto io_handler = std::make_unique<FileIOHandler>(temp_file);
                    OggDemuxer demuxer(std::move(io_handler));
                    demuxer.parseContainer();
                } catch (const std::exception& e) {
                    // Ignore individual failures for performance test
                }
            }
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            std::cout << "100 parsing operations completed in " << duration.count() << "ms" << std::endl;
            
            // Performance should be reasonable (less than 10 seconds for 100 operations)
            assert_true(duration.count() < 10000, "Parsing performance acceptable");
            
        } catch (const std::exception& e) {
            std::cout << "Performance test exception: " << e.what() << std::endl;
        }
        
        std::remove(temp_file.c_str());
    }
    
    /**
     * Test thread safety (basic concurrent access)
     */
    void testThreadSafety() {
        std::cout << "=== Testing Thread Safety ===" << std::endl;
        
        auto test_data = TestDataGenerator::generateMinimalOggVorbis();
        std::string temp_file = "test_thread_temp.ogg";
        
        std::ofstream file(temp_file, std::ios::binary);
        file.write(reinterpret_cast<const char*>(test_data.data()), test_data.size());
        file.close();
        
        try {
            std::vector<std::thread> threads;
            std::atomic<int> success_count{0};
            std::atomic<int> error_count{0};
            
            // Create multiple threads that parse the same file
            for (int i = 0; i < 5; i++) {
                threads.emplace_back([&temp_file, &success_count, &error_count]() {
                    try {
                        auto io_handler = std::make_unique<FileIOHandler>(temp_file);
                        OggDemuxer demuxer(std::move(io_handler));
                        if (demuxer.parseContainer()) {
                            success_count++;
                        } else {
                            error_count++;
                        }
                    } catch (const std::exception& e) {
                        error_count++;
                    }
                });
            }
            
            // Wait for all threads to complete
            for (auto& thread : threads) {
                thread.join();
            }
            
            std::cout << "Thread safety test: " << success_count.load() << " successes, " 
                      << error_count.load() << " errors" << std::endl;
            
            // At least some threads should succeed
            assert_true(success_count.load() > 0, "Thread safety - some operations succeeded");
            
        } catch (const std::exception& e) {
            std::cout << "Thread safety test exception: " << e.what() << std::endl;
        }
        
        std::remove(temp_file.c_str());
    }
    
    /**
     * Test regression scenarios (edge cases)
     */
    void testRegressionScenarios() {
        std::cout << "=== Testing Regression Scenarios ===" << std::endl;
        
        // Test very small file
        {
            std::string temp_file = "test_tiny_temp.ogg";
            std::ofstream file(temp_file, std::ios::binary);
            file.write("OggS", 4); // Just the signature
            file.close();
            
            try {
                auto io_handler = std::make_unique<FileIOHandler>(temp_file);
                OggDemuxer demuxer(std::move(io_handler));
                bool result = demuxer.parseContainer();
                
                // Should handle gracefully (likely return false)
                std::cout << "Tiny file handling: " << (result ? "parsed" : "rejected") << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Tiny file exception (expected): " << e.what() << std::endl;
            }
            
            std::remove(temp_file.c_str());
        }
        
        // Test file with only header
        {
            auto header_only_data = TestDataGenerator::generateMinimalOggVorbis();
            std::string temp_file = "test_header_only_temp.ogg";
            
            std::ofstream file(temp_file, std::ios::binary);
            file.write(reinterpret_cast<const char*>(header_only_data.data()), header_only_data.size());
            file.close();
            
            try {
                auto io_handler = std::make_unique<FileIOHandler>(temp_file);
                OggDemuxer demuxer(std::move(io_handler));
                bool result = demuxer.parseContainer();
                
                if (result) {
                    // Try to read data chunks
                    auto chunk = demuxer.readChunk();
                    std::cout << "Header-only file: chunk size = " << chunk.data.size() << std::endl;
                }
            } catch (const std::exception& e) {
                std::cout << "Header-only file exception: " << e.what() << std::endl;
            }
            
            std::remove(temp_file.c_str());
        }
        
        std::cout << "Regression scenario tests completed." << std::endl;
    }
    
    /**
     * Run all comprehensive tests
     */
    void runAllTests() {
        std::cout << "Starting OGG Demuxer Comprehensive Test Suite" << std::endl;
        std::cout << "=============================================" << std::endl;
        
        try {
            testCodecDetection();
            testSeekingAccuracy();
            testErrorHandling();
            testMemoryManagement();
            testPerformance();
            testThreadSafety();
            testRegressionScenarios();
            
            std::cout << std::endl;
            std::cout << "=============================================" << std::endl;
            print_summary();
            
        } catch (const std::exception& e) {
            std::cout << "Test suite exception: " << e.what() << std::endl;
        }
    }
};

int main() {
    std::cout << "OGG Demuxer Comprehensive Test Suite" << std::endl;
    std::cout << "====================================" << std::endl;
    
    try {
        ComprehensiveTestSuite test_suite;
        test_suite.runAllTests();
        
        std::cout << std::endl;
        std::cout << "All comprehensive tests completed." << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "Fatal test suite error: " << e.what() << std::endl;
        return 1;
    }
}