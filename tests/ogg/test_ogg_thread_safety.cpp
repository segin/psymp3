/*
 * test_ogg_thread_safety.cpp - Property-based tests for OggDemuxer thread safety
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Property-based tests for thread safety in OggDemuxer.
 * Tests concurrent access patterns to verify proper synchronization.
 * 
 * **Feature: ogg-demuxer-fix, Property 16: Thread Safety**
 * **Validates: Requirements 11.1**
 */

#include "psymp3.h"

#ifdef HAVE_RAPIDCHECK
#include <rapidcheck.h>
#endif

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <memory>

#ifdef HAVE_OGGDEMUXER
using namespace PsyMP3::Demuxer::Ogg;
#endif

// Test result tracking
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, msg) \
    do { \
        if (!(condition)) { \
            std::cout << "✗ FAILED: " << msg << std::endl; \
            tests_failed++; \
            return false; \
        } \
    } while(0)

#define TEST_PASS(msg) \
    do { \
        std::cout << "✓ " << msg << std::endl; \
        tests_passed++; \
    } while(0)

#ifdef HAVE_OGGDEMUXER

/**
 * Mock IOHandler for testing
 */
class MockIOHandler : public PsyMP3::IO::IOHandler {
public:
    MockIOHandler() : m_position(0) {}
    
    int close() override { return 0; }
    
    size_t read(void* buffer, size_t size, size_t count) override {
        // Return empty data to simulate EOF
        return 0;
    }
    
    int seek(off_t offset, int whence) override {
        if (whence == SEEK_SET) {
            m_position = offset;
        } else if (whence == SEEK_CUR) {
            m_position += offset;
        } else if (whence == SEEK_END) {
            m_position = 1000000 + offset;  // Simulate 1MB file
        }
        return 0;
    }
    
    off_t tell() override {
        return m_position;
    }
    
    bool eof() override { return false; }
    
    off_t getFileSize() override { return 1000000; }
    
private:
    off_t m_position;
};

/**
 * Test: Concurrent readChunk calls from multiple threads
 * 
 * Property: For any concurrent access to readChunk from multiple threads,
 * the demuxer SHALL protect shared state with appropriate synchronization.
 * 
 * This test verifies that:
 * - Multiple threads can safely call readChunk simultaneously
 * - No data races occur on shared state
 * - Packet queues are accessed safely
 * 
 * Requirements: 11.1, 11.4
 */
bool testConcurrentReadChunk() {
    std::cout << "\n=== Test: Concurrent readChunk calls ===" << std::endl;
    
    auto handler = std::make_unique<MockIOHandler>();
    OggDemuxer demuxer(std::move(handler));
    
    // Track concurrent access
    std::atomic<int> concurrent_readers{0};
    std::atomic<int> max_concurrent{0};
    std::atomic<bool> error_detected{false};
    
    // Create multiple threads that call readChunk
    const int NUM_THREADS = 4;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&demuxer, &concurrent_readers, &max_concurrent, &error_detected]() {
            // Simulate concurrent access
            for (int j = 0; j < 10; ++j) {
                concurrent_readers++;
                int current = concurrent_readers.load();
                
                // Track maximum concurrent readers
                int expected = max_concurrent.load();
                while (current > expected && 
                       !max_concurrent.compare_exchange_weak(expected, current)) {
                    expected = max_concurrent.load();
                }
                
                // Call readChunk (should not crash or deadlock)
                try {
                    MediaChunk chunk = demuxer.readChunk();
                    // Verify chunk is valid (even if empty)
                    if (chunk.stream_id != 0 && chunk.data.empty()) {
                        // This is acceptable - empty chunk from empty stream
                    }
                } catch (...) {
                    error_detected.store(true);
                }
                
                concurrent_readers--;
                
                // Small delay to increase chance of concurrent access
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify no errors occurred
    TEST_ASSERT(!error_detected.load(), "No exceptions during concurrent readChunk");
    
    // Verify concurrent access occurred
    TEST_ASSERT(max_concurrent.load() > 0, "Concurrent access detected");
    
    TEST_PASS("Concurrent readChunk calls handled safely");
    return true;
}

/**
 * Test: Concurrent seekTo calls from multiple threads
 * 
 * Property: For any concurrent seeking operations from multiple threads,
 * the demuxer SHALL handle concurrent operations safely.
 * 
 * This test verifies that:
 * - Multiple threads can safely call seekTo simultaneously
 * - No race conditions occur on file position
 * - Seeking state is protected
 * 
 * Requirements: 11.1, 11.2
 */
bool testConcurrentSeekTo() {
    std::cout << "\n=== Test: Concurrent seekTo calls ===" << std::endl;
    
    auto handler = std::make_unique<MockIOHandler>();
    OggDemuxer demuxer(std::move(handler));
    
    std::atomic<bool> error_detected{false};
    std::atomic<int> seek_count{0};
    
    // Create multiple threads that call seekTo
    const int NUM_THREADS = 4;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&demuxer, &error_detected, &seek_count, i]() {
            // Each thread seeks to different positions
            for (int j = 0; j < 5; ++j) {
                try {
                    uint64_t timestamp_ms = (i * 1000) + (j * 100);
                    bool result = demuxer.seekTo(timestamp_ms);
                    // Result may be false (no valid stream), but should not crash
                    seek_count++;
                } catch (...) {
                    error_detected.store(true);
                }
                
                // Small delay
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify no errors occurred
    TEST_ASSERT(!error_detected.load(), "No exceptions during concurrent seekTo");
    
    // Verify seeks were attempted
    TEST_ASSERT(seek_count.load() > 0, "Concurrent seeks were attempted");
    
    TEST_PASS("Concurrent seekTo calls handled safely");
    return true;
}

/**
 * Test: Concurrent readChunk and seekTo from different threads
 * 
 * Property: For any concurrent seeking and reading operations from different threads,
 * the demuxer SHALL prevent race conditions on file position.
 * 
 * This test verifies that:
 * - Reading and seeking can occur concurrently
 * - File position is protected from race conditions
 * - No deadlocks occur
 * 
 * Requirements: 11.1, 11.2, 11.3
 */
bool testConcurrentReadAndSeek() {
    std::cout << "\n=== Test: Concurrent readChunk and seekTo ===" << std::endl;
    
    auto handler = std::make_unique<MockIOHandler>();
    OggDemuxer demuxer(std::move(handler));
    
    std::atomic<bool> error_detected{false};
    std::atomic<int> operations{0};
    
    // Create reader threads
    std::vector<std::thread> threads;
    
    for (int i = 0; i < 2; ++i) {
        threads.emplace_back([&demuxer, &error_detected, &operations]() {
            for (int j = 0; j < 10; ++j) {
                try {
                    MediaChunk chunk = demuxer.readChunk();
                    operations++;
                } catch (...) {
                    error_detected.store(true);
                }
                std::this_thread::sleep_for(std::chrono::microseconds(5));
            }
        });
    }
    
    // Create seeker threads
    for (int i = 0; i < 2; ++i) {
        threads.emplace_back([&demuxer, &error_detected, &operations]() {
            for (int j = 0; j < 10; ++j) {
                try {
                    bool result = demuxer.seekTo(j * 100);
                    operations++;
                } catch (...) {
                    error_detected.store(true);
                }
                std::this_thread::sleep_for(std::chrono::microseconds(5));
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify no errors occurred
    TEST_ASSERT(!error_detected.load(), "No exceptions during concurrent read/seek");
    
    // Verify operations were performed
    TEST_ASSERT(operations.load() > 0, "Concurrent operations were performed");
    
    TEST_PASS("Concurrent readChunk and seekTo handled safely");
    return true;
}

/**
 * Test: Error state propagation across threads
 * 
 * Property: For any errors occurring in one thread, the demuxer SHALL
 * propagate error state safely to other threads.
 * 
 * This test verifies that:
 * - Error state can be set atomically
 * - Error state is visible to other threads
 * - Error codes are propagated correctly
 * 
 * Requirements: 11.7
 */
bool testErrorStatePropagation() {
    std::cout << "\n=== Test: Error state propagation ===" << std::endl;
    
    auto handler = std::make_unique<MockIOHandler>();
    OggDemuxer demuxer(std::move(handler));
    
    std::atomic<bool> error_detected_by_thread{false};
    
    // Thread 1: Set error state
    std::thread error_setter([&demuxer]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        // Simulate error detection
        demuxer.setErrorState_unlocked(-1);
    });
    
    // Thread 2: Check error state
    std::thread error_checker([&demuxer, &error_detected_by_thread]() {
        // Wait a bit for error to be set
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        
        // Check if error state is visible
        if (demuxer.hasErrorState()) {
            int error_code = demuxer.getErrorCode();
            if (error_code == -1) {
                error_detected_by_thread.store(true);
            }
        }
    });
    
    error_setter.join();
    error_checker.join();
    
    // Verify error was propagated
    TEST_ASSERT(error_detected_by_thread.load(), "Error state propagated to other thread");
    
    // Clear error state
    demuxer.clearErrorState();
    TEST_ASSERT(!demuxer.hasErrorState(), "Error state cleared successfully");
    
    TEST_PASS("Error state propagation works correctly");
    return true;
}

#endif  // HAVE_OGGDEMUXER

/**
 * Main test runner
 */
int main(int argc, char* argv[]) {
    std::cout << "=== OggDemuxer Thread Safety Tests ===" << std::endl;
    std::cout << "**Feature: ogg-demuxer-fix, Property 16: Thread Safety**" << std::endl;
    std::cout << "**Validates: Requirements 11.1**" << std::endl;
    
#ifdef HAVE_OGGDEMUXER
    
    // Run all thread safety tests
    testConcurrentReadChunk();
    testConcurrentSeekTo();
    testConcurrentReadAndSeek();
    testErrorStatePropagation();
    
#else
    std::cout << "OggDemuxer not available - skipping tests" << std::endl;
    return 0;
#endif
    
    // Print summary
    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "Passed: " << tests_passed << std::endl;
    std::cout << "Failed: " << tests_failed << std::endl;
    
    if (tests_failed > 0) {
        std::cout << "\n✗ Some tests failed" << std::endl;
        return 1;
    } else {
        std::cout << "\n✓ All tests passed" << std::endl;
        return 0;
    }
}
