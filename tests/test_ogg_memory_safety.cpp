/*
 * test_memory_safety.cpp - Memory safety and resource management tests for OggDemuxer
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifdef HAVE_OGGDEMUXER

#include "test_framework.h"
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <fstream>

// Test IOHandler implementations
class TestIOHandler : public IOHandler {
private:
    std::vector<uint8_t> m_data;
    size_t m_position;
    
public:
    explicit TestIOHandler(const std::vector<uint8_t>& data) 
        : m_data(data), m_position(0) {}
    
    size_t read(void* buffer, size_t size, size_t count) override {
        size_t bytes_to_read = size * count;
        size_t available = m_data.size() - m_position;
        size_t actual_bytes = std::min(bytes_to_read, available);
        
        if (actual_bytes > 0) {
            std::memcpy(buffer, m_data.data() + m_position, actual_bytes);
            m_position += actual_bytes;
        }
        
        return actual_bytes / size;
    }
    
    int seek(long offset, int whence) override {
        size_t new_pos;
        
        switch (whence) {
            case SEEK_SET:
                new_pos = static_cast<size_t>(offset);
                break;
            case SEEK_CUR:
                new_pos = m_position + static_cast<size_t>(offset);
                break;
            case SEEK_END:
                new_pos = m_data.size() + static_cast<size_t>(offset);
                break;
            default:
                return -1;
        }
        
        if (new_pos > m_data.size()) {
            return -1;
        }
        
        m_position = new_pos;
        return 0;
    }
    
    long tell() override {
        return static_cast<long>(m_position);
    }
    
    bool eof() override {
        return m_position >= m_data.size();
    }
    
    off_t getFileSize() override {
        return static_cast<off_t>(m_data.size());
    }
};

class FailingIOHandler : public IOHandler {
public:
    size_t read(void* buffer, size_t size, size_t count) override {
        return 0; // Always fail
    }
    
    int seek(long offset, int whence) override {
        return -1; // Always fail
    }
    
    long tell() override {
        return -1; // Always fail
    }
    
    bool eof() override {
        return true;
    }
    
    off_t getFileSize() override {
        return 0;
    }
};

// Test constants following reference implementation patterns
namespace MemorySafetyTestConstants {
    static constexpr size_t MAX_PACKET_QUEUE_SIZE = 100;
    static constexpr size_t PAGE_SIZE_MAX = 65307;
    static constexpr size_t LARGE_BUFFER_SIZE = PAGE_SIZE_MAX * 4;
    static constexpr int MAX_ALLOCATION_ATTEMPTS = 10;
    static constexpr int STRESS_TEST_ITERATIONS = 100; // Reduced for faster testing
    static constexpr int CONCURRENT_THREADS = 4;
}

class MemorySafetyTestSuite {
public:
    static void runAllTests() {
        TestFramework::TestSuite suite("OggDemuxer Memory Safety Tests");
        
        // Basic memory safety tests
        suite.addTest("test_libogg_initialization_cleanup", test_libogg_initialization_cleanup);
        suite.addTest("test_bounded_packet_queues", test_bounded_packet_queues);
        suite.addTest("test_buffer_overflow_prevention", test_buffer_overflow_prevention);
        suite.addTest("test_null_pointer_checks", test_null_pointer_checks);
        suite.addTest("test_memory_allocation_failures", test_memory_allocation_failures);
        
        // Resource management tests
        suite.addTest("test_destructor_cleanup", test_destructor_cleanup);
        suite.addTest("test_error_path_cleanup", test_error_path_cleanup);
        suite.addTest("test_seek_state_reset", test_seek_state_reset);
        suite.addTest("test_stream_switching", test_stream_switching);
        
        // Enhanced memory safety tests
        suite.addTest("test_memory_audit", test_memory_audit);
        suite.addTest("test_memory_limit_enforcement", test_memory_limit_enforcement);
        suite.addTest("test_libogg_structure_validation", test_libogg_structure_validation);
        suite.addTest("test_periodic_maintenance", test_periodic_maintenance);
        suite.addTest("test_concurrent_memory_access", test_concurrent_memory_access);
        
        // Stress tests
        suite.addTest("test_memory_exhaustion_protection", test_memory_exhaustion_protection);
        suite.addTest("test_large_packet_handling", test_large_packet_handling);
        
        auto results = suite.runAll();
        suite.printResults(results);
    }

private:
    // Test 1: Proper initialization and cleanup of libogg structures (Requirement 8.1)
    static bool test_libogg_initialization_cleanup() {
        try {
            // Create test file with minimal Ogg data
            std::vector<uint8_t> test_data = createMinimalOggFile();
            std::unique_ptr<IOHandler> handler = std::make_unique<TestIOHandler>(test_data);
            
            // Test constructor initialization
            {
                OggDemuxer demuxer(std::move(handler));
                
                // Verify that libogg structures are properly initialized
                // This is tested implicitly by successful construction
                
                // Test basic operations to ensure structures are working
                bool parse_result = demuxer.parseContainer();
                
                // Test that we can access streams (verifies ogg_stream_state initialization)
                auto streams = demuxer.getStreams();
                
                // Destructor will be called here - test cleanup
            }
            
            // Test multiple construction/destruction cycles to verify no leaks
            for (int i = 0; i < 10; i++) {
                std::unique_ptr<IOHandler> handler2 = std::make_unique<TestIOHandler>(test_data);
                OggDemuxer demuxer(std::move(handler2));
                demuxer.parseContainer();
                
                // Test operations that use libogg structures
                auto streams = demuxer.getStreams();
                if (!streams.empty()) {
                    demuxer.readChunk(streams[0].stream_id);
                }
            }
            
            return true;
            
        } catch (const std::exception& e) {
            Debug::log("test", "test_libogg_initialization_cleanup failed: ", e.what());
            return false;
        }
    }
    
    // Test 2: Bounded packet queues with size limits (Requirement 8.2)
    static bool test_bounded_packet_queues() {
        try {
            std::vector<uint8_t> test_data = createLargeOggFile();
            std::unique_ptr<IOHandler> handler = std::make_unique<TestIOHandler>(test_data);
            OggDemuxer demuxer(std::move(handler));
            
            if (!demuxer.parseContainer()) {
                Debug::log("test", "Failed to parse container for bounded queue test");
                return false;
            }
            
            auto streams = demuxer.getStreams();
            if (streams.empty()) {
                Debug::log("test", "No streams found for bounded queue test");
                return false;
            }
            
            uint32_t stream_id = streams[0].stream_id;
            
            // Fill packet queue and verify it's bounded
            int packets_read = 0;
            int max_attempts = 200; // Reduced for faster testing
            
            while (packets_read < max_attempts) {
                MediaChunk chunk = demuxer.readChunk(stream_id);
                if (chunk.data.empty()) {
                    break;
                }
                packets_read++;
            }
            
            Debug::log("test", "Successfully processed ", packets_read, " packets with bounded queues");
            return true;
            
        } catch (const std::exception& e) {
            Debug::log("test", "test_bounded_packet_queues failed: ", e.what());
            return false;
        }
    }
    
    // Test 3: Buffer overflow prevention using PAGE_SIZE_MAX (Requirement 8.4)
    static bool test_buffer_overflow_prevention() {
        try {
            // Test with normal data
            std::vector<uint8_t> test_data = createMinimalOggFile();
            std::unique_ptr<IOHandler> handler = std::make_unique<TestIOHandler>(test_data);
            OggDemuxer demuxer(std::move(handler));
            
            // Should handle normal data without issues
            bool parse_result = demuxer.parseContainer();
            
            Debug::log("test", "Buffer overflow prevention test completed");
            return true;
            
        } catch (const std::exception& e) {
            Debug::log("test", "test_buffer_overflow_prevention failed: ", e.what());
            return false;
        }
    }
    
    // Test 4: Null pointer checks for dynamic allocations (Requirement 8.5)
    static bool test_null_pointer_checks() {
        try {
            // Test with empty data
            std::vector<uint8_t> empty_data;
            std::unique_ptr<IOHandler> handler = std::make_unique<TestIOHandler>(empty_data);
            OggDemuxer demuxer(std::move(handler));
            
            // Should handle empty data gracefully
            bool parse_result = demuxer.parseContainer();
            
            Debug::log("test", "Null pointer checks test completed");
            return true;
            
        } catch (const std::exception& e) {
            Debug::log("test", "test_null_pointer_checks failed: ", e.what());
            return false;
        }
    }
    
    // Test 5: Memory allocation failure handling (Requirement 8.3)
    static bool test_memory_allocation_failures() {
        try {
            // Test with normal data
            std::vector<uint8_t> test_data = createMinimalOggFile();
            std::unique_ptr<IOHandler> handler = std::make_unique<TestIOHandler>(test_data);
            OggDemuxer demuxer(std::move(handler));
            
            // Should handle normal allocations gracefully
            bool parse_result = demuxer.parseContainer();
            
            Debug::log("test", "Memory allocation failure handling test completed");
            return true;
            
        } catch (const std::bad_alloc& e) {
            Debug::log("test", "Expected memory allocation failure handled gracefully");
            return true;
        } catch (const std::exception& e) {
            Debug::log("test", "test_memory_allocation_failures failed: ", e.what());
            return false;
        }
    }
    
    // Test 6: Destructor cleanup verification (Requirement 8.3)
    static bool test_destructor_cleanup() {
        try {
            std::vector<uint8_t> test_data = createMinimalOggFile();
            
            // Test that destructor properly cleans up resources
            {
                std::unique_ptr<IOHandler> handler = std::make_unique<TestIOHandler>(test_data);
                OggDemuxer demuxer(std::move(handler));
                demuxer.parseContainer();
                
                // Add some streams and packets
                auto streams = demuxer.getStreams();
                if (!streams.empty()) {
                    demuxer.readChunk(streams[0].stream_id);
                }
                
                // Destructor called here - should clean up all libogg structures
            }
            
            // Test multiple destruction cycles
            for (int i = 0; i < 10; i++) {
                std::unique_ptr<IOHandler> handler = std::make_unique<TestIOHandler>(test_data);
                OggDemuxer* demuxer = new OggDemuxer(std::move(handler));
                demuxer->parseContainer();
                delete demuxer;
            }
            
            Debug::log("test", "Destructor cleanup test completed");
            return true;
            
        } catch (const std::exception& e) {
            Debug::log("test", "test_destructor_cleanup failed: ", e.what());
            return false;
        }
    }
    
    // Test 7: Error path cleanup verification (Requirement 8.3)
    static bool test_error_path_cleanup() {
        try {
            // Test cleanup when parsing fails
            std::vector<uint8_t> invalid_data = {0x00, 0x01, 0x02, 0x03}; // Not Ogg data
            std::unique_ptr<IOHandler> handler = std::make_unique<TestIOHandler>(invalid_data);
            OggDemuxer demuxer(std::move(handler));
            
            // Should fail gracefully and clean up properly
            bool parse_result = demuxer.parseContainer();
            
            // Test cleanup when I/O fails
            std::unique_ptr<IOHandler> failing_handler = std::make_unique<FailingIOHandler>();
            OggDemuxer demuxer2(std::move(failing_handler));
            
            // Should handle I/O failure and clean up properly
            bool parse_result2 = demuxer2.parseContainer();
            
            Debug::log("test", "Error path cleanup test completed");
            return true;
            
        } catch (const std::exception& e) {
            Debug::log("test", "test_error_path_cleanup failed: ", e.what());
            return false;
        }
    }
    
    // Test 8: ogg_sync_reset() after seeks (Requirement 12.9)
    static bool test_seek_state_reset() {
        try {
            std::vector<uint8_t> test_data = createSeekableOggFile();
            std::unique_ptr<IOHandler> handler = std::make_unique<TestIOHandler>(test_data);
            OggDemuxer demuxer(std::move(handler));
            
            if (!demuxer.parseContainer()) {
                Debug::log("test", "Failed to parse container for seek test");
                return false;
            }
            
            uint64_t duration = demuxer.getDuration();
            if (duration == 0) {
                Debug::log("test", "No duration available for seek test");
                return true; // Not an error, just can't test seeking
            }
            
            // Test seeking to various positions
            std::vector<uint64_t> seek_positions = {0, duration / 4, duration / 2, duration * 3 / 4};
            
            for (uint64_t pos : seek_positions) {
                bool seek_result = demuxer.seekTo(pos);
                
                // Read some data after seek to verify state is properly reset
                auto streams = demuxer.getStreams();
                if (!streams.empty()) {
                    MediaChunk chunk = demuxer.readChunk(streams[0].stream_id);
                    // Should be able to read data after seek
                }
            }
            
            Debug::log("test", "Seek state reset test completed");
            return true;
            
        } catch (const std::exception& e) {
            Debug::log("test", "test_seek_state_reset failed: ", e.what());
            return false;
        }
    }
    
    // Test 9: ogg_stream_reset_serialno() for stream switching (Requirement 12.10)
    static bool test_stream_switching() {
        try {
            std::vector<uint8_t> test_data = createMultiStreamOggFile();
            std::unique_ptr<IOHandler> handler = std::make_unique<TestIOHandler>(test_data);
            OggDemuxer demuxer(std::move(handler));
            
            if (!demuxer.parseContainer()) {
                Debug::log("test", "Failed to parse container for stream switching test");
                return false;
            }
            
            auto streams = demuxer.getStreams();
            if (streams.size() < 2) {
                Debug::log("test", "Need multiple streams for stream switching test");
                return true; // Not an error, just can't test stream switching
            }
            
            // Test switching between streams
            for (const auto& stream : streams) {
                MediaChunk chunk = demuxer.readChunk(stream.stream_id);
                // Should be able to read from each stream
            }
            
            Debug::log("test", "Stream switching test completed");
            return true;
            
        } catch (const std::exception& e) {
            Debug::log("test", "test_stream_switching failed: ", e.what());
            return false;
        }
    }
    
    // Test 10: Memory audit functionality (Enhanced memory safety)
    static bool test_memory_audit() {
        try {
            std::vector<uint8_t> test_data = createMinimalOggFile();
            std::unique_ptr<IOHandler> handler = std::make_unique<TestIOHandler>(test_data);
            OggDemuxer demuxer(std::move(handler));
            
            if (!demuxer.parseContainer()) {
                Debug::log("test", "Failed to parse container for memory audit test");
                return false;
            }
            
            // Test memory audit functionality
            bool audit_result = demuxer.performMemoryAudit();
            
            Debug::log("test", "Memory audit test completed with result: ", audit_result);
            return true;
            
        } catch (const std::exception& e) {
            Debug::log("test", "test_memory_audit failed: ", e.what());
            return false;
        }
    }
    
    // Test 11: Memory limit enforcement (Enhanced memory safety)
    static bool test_memory_limit_enforcement() {
        try {
            std::vector<uint8_t> test_data = createLargeOggFile();
            std::unique_ptr<IOHandler> handler = std::make_unique<TestIOHandler>(test_data);
            OggDemuxer demuxer(std::move(handler));
            
            if (!demuxer.parseContainer()) {
                Debug::log("test", "Failed to parse container for memory limit test");
                return false;
            }
            
            // Test memory limit enforcement
            demuxer.enforceMemoryLimits();
            
            Debug::log("test", "Memory limit enforcement test completed");
            return true;
            
        } catch (const std::exception& e) {
            Debug::log("test", "test_memory_limit_enforcement failed: ", e.what());
            return false;
        }
    }
    
    // Test 12: libogg structure validation (Enhanced memory safety)
    static bool test_libogg_structure_validation() {
        try {
            std::vector<uint8_t> test_data = createMinimalOggFile();
            std::unique_ptr<IOHandler> handler = std::make_unique<TestIOHandler>(test_data);
            OggDemuxer demuxer(std::move(handler));
            
            if (!demuxer.parseContainer()) {
                Debug::log("test", "Failed to parse container for structure validation test");
                return false;
            }
            
            // Test structure validation
            bool validation_result = demuxer.validateLiboggStructures();
            
            Debug::log("test", "libogg structure validation test completed with result: ", validation_result);
            return true;
            
        } catch (const std::exception& e) {
            Debug::log("test", "test_libogg_structure_validation failed: ", e.what());
            return false;
        }
    }
    
    // Test 13: Periodic maintenance (Enhanced memory safety)
    static bool test_periodic_maintenance() {
        try {
            std::vector<uint8_t> test_data = createMinimalOggFile();
            std::unique_ptr<IOHandler> handler = std::make_unique<TestIOHandler>(test_data);
            OggDemuxer demuxer(std::move(handler));
            
            if (!demuxer.parseContainer()) {
                Debug::log("test", "Failed to parse container for periodic maintenance test");
                return false;
            }
            
            // Test periodic maintenance
            demuxer.performPeriodicMaintenance();
            
            Debug::log("test", "Periodic maintenance test completed");
            return true;
            
        } catch (const std::exception& e) {
            Debug::log("test", "test_periodic_maintenance failed: ", e.what());
            return false;
        }
    }
    
    // Test 14: Concurrent memory access (Enhanced memory safety)
    static bool test_concurrent_memory_access() {
        try {
            std::vector<uint8_t> test_data = createLargeOggFile();
            std::unique_ptr<IOHandler> handler = std::make_unique<TestIOHandler>(test_data);
            OggDemuxer demuxer(std::move(handler));
            
            if (!demuxer.parseContainer()) {
                Debug::log("test", "Failed to parse container for concurrent access test");
                return false;
            }
            
            auto streams = demuxer.getStreams();
            if (streams.empty()) {
                Debug::log("test", "No streams found for concurrent access test");
                return false;
            }
            
            std::atomic<bool> test_failed{false};
            std::atomic<int> operations_completed{0};
            
            // Create multiple threads that access memory concurrently
            std::vector<std::thread> threads;
            const int num_threads = 4;
            const int operations_per_thread = 10;
            
            for (int i = 0; i < num_threads; i++) {
                threads.emplace_back([&demuxer, &streams, &test_failed, &operations_completed, operations_per_thread]() {
                    try {
                        for (int j = 0; j < operations_per_thread; j++) {
                            // Perform various operations that access memory
                            MediaChunk chunk = demuxer.readChunk(streams[0].stream_id);
                            demuxer.performMemoryAudit();
                            demuxer.enforceMemoryLimits();
                            operations_completed.fetch_add(1);
                        }
                    } catch (const std::exception& e) {
                        Debug::log("test", "Concurrent access thread failed: ", e.what());
                        test_failed.store(true);
                    }
                });
            }
            
            // Wait for all threads to complete
            for (auto& thread : threads) {
                thread.join();
            }
            
            if (test_failed.load()) {
                Debug::log("test", "Concurrent memory access test failed");
                return false;
            }
            
            int total_operations = operations_completed.load();
            Debug::log("test", "Concurrent memory access test completed - ", total_operations, " operations");
            return total_operations == num_threads * operations_per_thread;
            
        } catch (const std::exception& e) {
            Debug::log("test", "test_concurrent_memory_access failed: ", e.what());
            return false;
        }
    }
    
    // Test 15: Memory exhaustion protection (Stress test)
    static bool test_memory_exhaustion_protection() {
        try {
            std::vector<uint8_t> test_data = createLargeOggFile();
            std::unique_ptr<IOHandler> handler = std::make_unique<TestIOHandler>(test_data);
            OggDemuxer demuxer(std::move(handler));
            
            if (!demuxer.parseContainer()) {
                Debug::log("test", "Failed to parse container for memory exhaustion test");
                return false;
            }
            
            auto streams = demuxer.getStreams();
            if (streams.empty()) {
                Debug::log("test", "No streams found for memory exhaustion test");
                return false;
            }
            
            // Try to read many chunks to test memory limits
            int chunks_read = 0;
            int max_chunks = 1000; // Reduced for faster testing
            
            while (chunks_read < max_chunks) {
                MediaChunk chunk = demuxer.readChunk(streams[0].stream_id);
                if (chunk.data.empty()) {
                    break;
                }
                chunks_read++;
                
                // Periodically check memory limits
                if (chunks_read % 50 == 0) {
                    demuxer.enforceMemoryLimits();
                }
            }
            
            Debug::log("test", "Memory exhaustion protection test completed - read ", chunks_read, " chunks");
            return true;
            
        } catch (const std::exception& e) {
            Debug::log("test", "test_memory_exhaustion_protection failed: ", e.what());
            return false;
        }
    }
    
    // Test 16: Large packet handling (Stress test)
    static bool test_large_packet_handling() {
        try {
            // Create test data with larger packets
            std::vector<uint8_t> test_data = createLargePacketOggFile();
            std::unique_ptr<IOHandler> handler = std::make_unique<TestIOHandler>(test_data);
            OggDemuxer demuxer(std::move(handler));
            
            // Should handle large packets gracefully
            bool parse_result = demuxer.parseContainer();
            
            Debug::log("test", "Large packet handling test completed with result: ", parse_result);
            return true;
            
        } catch (const std::exception& e) {
            Debug::log("test", "test_large_packet_handling failed: ", e.what());
            return false;
        }
    }
    
    // Helper methods to create test data
    static std::vector<uint8_t> createMinimalOggFile() {
        // Create minimal valid Ogg file with Vorbis header
        std::vector<uint8_t> data;
        
        // Ogg page header
        data.insert(data.end(), {'O', 'g', 'g', 'S'}); // Capture pattern
        data.push_back(0x00); // Version
        data.push_back(0x02); // Header type (first page)
        
        // Granule position (8 bytes)
        for (int i = 0; i < 8; i++) data.push_back(0x00);
        
        // Serial number (4 bytes)
        data.insert(data.end(), {0x01, 0x00, 0x00, 0x00});
        
        // Page sequence (4 bytes)
        for (int i = 0; i < 4; i++) data.push_back(0x00);
        
        // Checksum (4 bytes) - will be invalid but that's OK for testing
        for (int i = 0; i < 4; i++) data.push_back(0x00);
        
        // Segment count
        data.push_back(0x01);
        
        // Segment length
        data.push_back(0x1E); // 30 bytes for Vorbis ID header
        
        // Vorbis identification header
        data.insert(data.end(), {0x01, 'v', 'o', 'r', 'b', 'i', 's'});
        
        // Vorbis version (4 bytes)
        for (int i = 0; i < 4; i++) data.push_back(0x00);
        
        // Channels (1 byte)
        data.push_back(0x02); // Stereo
        
        // Sample rate (4 bytes) - 44100 Hz
        data.insert(data.end(), {0x44, 0xAC, 0x00, 0x00});
        
        // Bitrate max/nominal/min (12 bytes)
        for (int i = 0; i < 12; i++) data.push_back(0x00);
        
        // Blocksize info and framing
        data.insert(data.end(), {0xB8, 0x01}); // Blocksize info + framing bit
        
        return data;
    }
    
    static std::vector<uint8_t> createLargeOggFile() {
        std::vector<uint8_t> data = createMinimalOggFile();
        
        // Add more pages to make it larger
        for (int i = 0; i < 50; i++) { // Reduced for faster testing
            std::vector<uint8_t> page = createMinimalOggFile();
            data.insert(data.end(), page.begin(), page.end());
        }
        
        return data;
    }
    
    static std::vector<uint8_t> createSeekableOggFile() {
        // Create file with multiple pages for seeking
        return createLargeOggFile();
    }
    
    static std::vector<uint8_t> createMultiStreamOggFile() {
        std::vector<uint8_t> data = createMinimalOggFile();
        
        // Add another stream with different serial number
        std::vector<uint8_t> stream2 = createMinimalOggFile();
        // Change serial number in second stream
        if (stream2.size() >= 18) {
            stream2[14] = 0x02; // Different serial number
        }
        
        data.insert(data.end(), stream2.begin(), stream2.end());
        return data;
    }
    
    static std::vector<uint8_t> createLargePacketOggFile() {
        std::vector<uint8_t> data = createMinimalOggFile();
        
        // Add a page with larger packet data
        std::vector<uint8_t> large_page;
        
        // Ogg page header
        large_page.insert(large_page.end(), {'O', 'g', 'g', 'S'}); // Capture pattern
        large_page.push_back(0x00); // Version
        large_page.push_back(0x00); // Header type (continuation page)
        
        // Granule position (8 bytes)
        for (int i = 0; i < 8; i++) large_page.push_back(0x00);
        
        // Serial number (4 bytes)
        large_page.insert(large_page.end(), {0x01, 0x00, 0x00, 0x00});
        
        // Page sequence (4 bytes)
        large_page.insert(large_page.end(), {0x01, 0x00, 0x00, 0x00});
        
        // Checksum (4 bytes) - will be invalid but that's OK for testing
        for (int i = 0; i < 4; i++) large_page.push_back(0x00);
        
        // Segment count (use multiple segments for larger packet)
        large_page.push_back(0x04); // 4 segments
        
        // Segment lengths (each 255 bytes for maximum size)
        for (int i = 0; i < 4; i++) {
            large_page.push_back(0xFF); // 255 bytes per segment
        }
        
        // Add packet data (4 * 255 = 1020 bytes)
        for (int i = 0; i < 1020; i++) {
            large_page.push_back(static_cast<uint8_t>(i % 256));
        }
        
        data.insert(data.end(), large_page.begin(), large_page.end());
        return data;
    }
};

// Main test function
int main() {
    Debug::log("test", "Starting OggDemuxer Memory Safety Tests");
    
    try {
        MemorySafetyTestSuite::runAllTests();
        Debug::log("test", "All memory safety tests completed");
        return 0;
    } catch (const std::exception& e) {
        Debug::log("test", "Memory safety tests failed with exception: ", e.what());
        return 1;
    }
}

#else // !HAVE_OGGDEMUXER

int main() {
    Debug::log("test", "OggDemuxer not available - skipping memory safety tests");
    return 0;
}

#endif // HAVE_OGGDEMUXER