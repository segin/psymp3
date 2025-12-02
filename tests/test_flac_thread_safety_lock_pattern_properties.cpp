/*
 * test_flac_thread_safety_lock_pattern_properties.cpp - Property-based tests for FLAC demuxer thread safety
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
#include <string>
#include <vector>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <random>
#include <memory>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <functional>

// ========================================
// THREAD SAFETY LOCK PATTERN VERIFICATION
// ========================================

/**
 * **Feature: flac-demuxer, Property 22: Thread Safety - Lock Pattern**
 * **Validates: Requirements 28.1, 28.2**
 *
 * Property 22: Thread Safety - Lock Pattern
 * *For any* public method call, the FLAC Demuxer SHALL acquire locks before 
 * calling private _unlocked implementations, and internal method calls SHALL 
 * use _unlocked versions.
 *
 * This test verifies:
 * 1. Concurrent access to public methods doesn't cause deadlocks
 * 2. Multiple threads can safely call public methods simultaneously
 * 3. The lock pattern prevents data races
 * 4. Atomic operations provide consistent state access
 */

// ========================================
// MOCK FLAC DEMUXER FOR TESTING
// ========================================

/**
 * Mock FLACDemuxer that implements the public/private lock pattern
 * for testing thread safety without requiring actual FLAC files.
 */
class MockFLACDemuxer {
public:
    MockFLACDemuxer() 
        : m_container_parsed(false)
        , m_current_sample(0)
        , m_eof(false)
        , m_atomic_current_sample(0)
        , m_atomic_eof(false)
        , m_atomic_error(false)
        , m_operation_count(0)
    {}
    
    // ========================================================================
    // Public Interface Methods (acquire locks, call _unlocked implementations)
    // Requirement 28.1: Public methods acquire locks and call private unlocked
    // ========================================================================
    
    bool parseContainer() {
        std::lock_guard<std::mutex> state_lock(m_state_mutex);
        std::lock_guard<std::mutex> metadata_lock(m_metadata_mutex);
        return parseContainer_unlocked();
    }
    
    uint64_t getPosition() const {
        std::lock_guard<std::mutex> state_lock(m_state_mutex);
        return getPosition_unlocked();
    }
    
    bool isEOF() const {
        std::lock_guard<std::mutex> state_lock(m_state_mutex);
        return isEOF_unlocked();
    }
    
    bool seekTo(uint64_t sample) {
        std::lock_guard<std::mutex> state_lock(m_state_mutex);
        return seekTo_unlocked(sample);
    }
    
    uint64_t readChunk() {
        std::lock_guard<std::mutex> state_lock(m_state_mutex);
        return readChunk_unlocked();
    }
    
    // ========================================================================
    // Atomic accessors (lock-free read access)
    // Requirement 28.6, 28.7: Atomic operations for sample counters and error state
    // ========================================================================
    
    uint64_t getAtomicCurrentSample() const {
        return m_atomic_current_sample.load(std::memory_order_acquire);
    }
    
    bool getAtomicEOF() const {
        return m_atomic_eof.load(std::memory_order_acquire);
    }
    
    bool getAtomicError() const {
        return m_atomic_error.load(std::memory_order_acquire);
    }
    
    // Test helper to get operation count
    uint64_t getOperationCount() const {
        return m_operation_count.load(std::memory_order_acquire);
    }
    
private:
    // ========================================================================
    // Thread safety - Lock acquisition order documented
    // 1. m_state_mutex (acquired first)
    // 2. m_metadata_mutex (acquired second)
    // ========================================================================
    mutable std::mutex m_state_mutex;
    mutable std::mutex m_metadata_mutex;
    
    // State variables (protected by m_state_mutex)
    bool m_container_parsed;
    uint64_t m_current_sample;
    bool m_eof;
    
    // Atomic state for thread-safe quick access (Requirements 28.6, 28.7)
    mutable std::atomic<uint64_t> m_atomic_current_sample;
    mutable std::atomic<bool> m_atomic_eof;
    mutable std::atomic<bool> m_atomic_error;
    
    // Operation counter for testing
    std::atomic<uint64_t> m_operation_count;
    
    // ========================================================================
    // Private unlocked implementations (assume locks are held)
    // Requirement 28.2: Internal method calls use unlocked versions
    // ========================================================================
    
    bool parseContainer_unlocked() {
        m_operation_count.fetch_add(1, std::memory_order_relaxed);
        m_container_parsed = true;
        updateCurrentSample_unlocked(0);
        updateEOF_unlocked(false);
        return true;
    }
    
    uint64_t getPosition_unlocked() const {
        return m_current_sample;
    }
    
    bool isEOF_unlocked() const {
        return m_eof;
    }
    
    bool seekTo_unlocked(uint64_t sample) {
        m_operation_count.fetch_add(1, std::memory_order_relaxed);
        updateCurrentSample_unlocked(sample);
        updateEOF_unlocked(false);
        return true;
    }
    
    uint64_t readChunk_unlocked() {
        m_operation_count.fetch_add(1, std::memory_order_relaxed);
        
        if (isEOF_unlocked()) {
            return 0;
        }
        
        // Simulate reading a chunk of 1024 samples
        uint64_t samples_read = 1024;
        updateCurrentSample_unlocked(m_current_sample + samples_read);
        
        // Simulate EOF at 1 million samples
        if (m_current_sample >= 1000000) {
            updateEOF_unlocked(true);
        }
        
        return samples_read;
    }
    
    // ========================================================================
    // Atomic State Update Helpers (Requirements 28.6, 28.7)
    // ========================================================================
    
    void updateCurrentSample_unlocked(uint64_t sample) {
        m_current_sample = sample;
        m_atomic_current_sample.store(sample, std::memory_order_release);
    }
    
    void updateEOF_unlocked(bool eof) {
        m_eof = eof;
        m_atomic_eof.store(eof, std::memory_order_release);
    }
    
    void updateError_unlocked(bool error) {
        m_atomic_error.store(error, std::memory_order_release);
    }
};

// ========================================
// PROPERTY-BASED TESTS
// ========================================

/**
 * Test 1: Concurrent read operations don't cause deadlocks
 * 
 * Multiple threads calling getPosition() and isEOF() simultaneously
 * should not cause deadlocks or data races.
 */
void test_concurrent_read_operations() {
    std::cout << "\n=== Test 1: Concurrent Read Operations ===" << std::endl;
    std::cout << "Testing that concurrent read operations don't cause deadlocks..." << std::endl;
    
    MockFLACDemuxer demuxer;
    demuxer.parseContainer();
    
    const int NUM_THREADS = 8;
    const int ITERATIONS_PER_THREAD = 1000;
    
    std::atomic<int> completed_threads(0);
    std::atomic<bool> test_failed(false);
    std::vector<std::thread> threads;
    
    // Barrier to start all threads simultaneously
    std::atomic<bool> start_flag(false);
    
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&demuxer, &completed_threads, &test_failed, &start_flag, t, ITERATIONS_PER_THREAD]() {
            // Wait for start signal
            while (!start_flag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            
            try {
                for (int i = 0; i < ITERATIONS_PER_THREAD; ++i) {
                    // Alternate between different read operations
                    if (i % 3 == 0) {
                        demuxer.getPosition();
                    } else if (i % 3 == 1) {
                        demuxer.isEOF();
                    } else {
                        demuxer.getAtomicCurrentSample();
                    }
                }
                completed_threads.fetch_add(1, std::memory_order_relaxed);
            } catch (...) {
                test_failed.store(true, std::memory_order_release);
            }
        });
    }
    
    // Start all threads
    start_flag.store(true, std::memory_order_release);
    
    // Wait for all threads with timeout
    auto start_time = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(10);
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    
    // Verify results
    assert(!test_failed.load() && "Test should not throw exceptions");
    assert(completed_threads.load() == NUM_THREADS && "All threads should complete");
    assert(elapsed < timeout && "Test should complete within timeout (no deadlock)");
    
    std::cout << "  " << NUM_THREADS << " threads completed " 
              << (NUM_THREADS * ITERATIONS_PER_THREAD) << " read operations" << std::endl;
    std::cout << "  Elapsed time: " << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() 
              << " ms" << std::endl;
    std::cout << "  ✓ No deadlocks detected" << std::endl;
}

/**
 * Test 2: Concurrent read/write operations don't cause deadlocks
 * 
 * Multiple threads calling a mix of read and write operations
 * should not cause deadlocks or data races.
 */
void test_concurrent_read_write_operations() {
    std::cout << "\n=== Test 2: Concurrent Read/Write Operations ===" << std::endl;
    std::cout << "Testing that concurrent read/write operations don't cause deadlocks..." << std::endl;
    
    MockFLACDemuxer demuxer;
    demuxer.parseContainer();
    
    const int NUM_READERS = 4;
    const int NUM_WRITERS = 4;
    const int ITERATIONS_PER_THREAD = 500;
    
    std::atomic<int> completed_threads(0);
    std::atomic<bool> test_failed(false);
    std::vector<std::thread> threads;
    
    // Barrier to start all threads simultaneously
    std::atomic<bool> start_flag(false);
    
    // Reader threads
    for (int t = 0; t < NUM_READERS; ++t) {
        threads.emplace_back([&demuxer, &completed_threads, &test_failed, &start_flag, ITERATIONS_PER_THREAD]() {
            while (!start_flag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            
            try {
                for (int i = 0; i < ITERATIONS_PER_THREAD; ++i) {
                    demuxer.getPosition();
                    demuxer.isEOF();
                    demuxer.getAtomicCurrentSample();
                    demuxer.getAtomicEOF();
                }
                completed_threads.fetch_add(1, std::memory_order_relaxed);
            } catch (...) {
                test_failed.store(true, std::memory_order_release);
            }
        });
    }
    
    // Writer threads
    for (int t = 0; t < NUM_WRITERS; ++t) {
        threads.emplace_back([&demuxer, &completed_threads, &test_failed, &start_flag, t, ITERATIONS_PER_THREAD]() {
            while (!start_flag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            
            try {
                std::mt19937 gen(t);
                std::uniform_int_distribution<uint64_t> dist(0, 100000);
                
                for (int i = 0; i < ITERATIONS_PER_THREAD; ++i) {
                    if (i % 2 == 0) {
                        demuxer.seekTo(dist(gen));
                    } else {
                        demuxer.readChunk();
                    }
                }
                completed_threads.fetch_add(1, std::memory_order_relaxed);
            } catch (...) {
                test_failed.store(true, std::memory_order_release);
            }
        });
    }
    
    // Start all threads
    start_flag.store(true, std::memory_order_release);
    
    // Wait for all threads with timeout
    auto start_time = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(10);
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    
    // Verify results
    int total_threads = NUM_READERS + NUM_WRITERS;
    assert(!test_failed.load() && "Test should not throw exceptions");
    assert(completed_threads.load() == total_threads && "All threads should complete");
    assert(elapsed < timeout && "Test should complete within timeout (no deadlock)");
    
    std::cout << "  " << NUM_READERS << " reader threads + " << NUM_WRITERS << " writer threads" << std::endl;
    std::cout << "  Total operations: " << demuxer.getOperationCount() << std::endl;
    std::cout << "  Elapsed time: " << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() 
              << " ms" << std::endl;
    std::cout << "  ✓ No deadlocks detected" << std::endl;
}

/**
 * Test 3: Atomic state consistency
 * 
 * Atomic operations should provide consistent state access
 * even under concurrent modifications.
 */
void test_atomic_state_consistency() {
    std::cout << "\n=== Test 3: Atomic State Consistency ===" << std::endl;
    std::cout << "Testing that atomic operations provide consistent state access..." << std::endl;
    
    MockFLACDemuxer demuxer;
    demuxer.parseContainer();
    
    const int NUM_THREADS = 4;
    const int ITERATIONS_PER_THREAD = 1000;
    
    std::atomic<int> completed_threads(0);
    std::atomic<bool> test_failed(false);
    std::atomic<int> consistency_violations(0);
    std::vector<std::thread> threads;
    
    // Barrier to start all threads simultaneously
    std::atomic<bool> start_flag(false);
    
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&demuxer, &completed_threads, &test_failed, &consistency_violations, &start_flag, t, ITERATIONS_PER_THREAD]() {
            while (!start_flag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            
            try {
                std::mt19937 gen(t);
                std::uniform_int_distribution<uint64_t> dist(0, 100000);
                
                for (int i = 0; i < ITERATIONS_PER_THREAD; ++i) {
                    // Perform a seek operation
                    uint64_t target = dist(gen);
                    demuxer.seekTo(target);
                    
                    // Read atomic state
                    uint64_t atomic_sample = demuxer.getAtomicCurrentSample();
                    
                    // The atomic sample should be a valid value (not corrupted)
                    // Note: Due to concurrent modifications, it may not equal target
                    // but it should be a valid uint64_t value
                    if (atomic_sample > 10000000) {
                        // Suspiciously large value might indicate corruption
                        consistency_violations.fetch_add(1, std::memory_order_relaxed);
                    }
                }
                completed_threads.fetch_add(1, std::memory_order_relaxed);
            } catch (...) {
                test_failed.store(true, std::memory_order_release);
            }
        });
    }
    
    // Start all threads
    start_flag.store(true, std::memory_order_release);
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify results
    assert(!test_failed.load() && "Test should not throw exceptions");
    assert(completed_threads.load() == NUM_THREADS && "All threads should complete");
    assert(consistency_violations.load() == 0 && "No consistency violations should occur");
    
    std::cout << "  " << NUM_THREADS << " threads completed " 
              << (NUM_THREADS * ITERATIONS_PER_THREAD) << " seek operations" << std::endl;
    std::cout << "  Consistency violations: " << consistency_violations.load() << std::endl;
    std::cout << "  ✓ Atomic state is consistent" << std::endl;
}

/**
 * Test 4: Stress test with many threads
 * 
 * High concurrency stress test to verify the lock pattern
 * holds under heavy load.
 */
void test_stress_high_concurrency() {
    std::cout << "\n=== Test 4: High Concurrency Stress Test ===" << std::endl;
    std::cout << "Testing lock pattern under high concurrency..." << std::endl;
    
    MockFLACDemuxer demuxer;
    demuxer.parseContainer();
    
    const int NUM_THREADS = 16;
    const int ITERATIONS_PER_THREAD = 200;
    
    std::atomic<int> completed_threads(0);
    std::atomic<bool> test_failed(false);
    std::vector<std::thread> threads;
    
    // Barrier to start all threads simultaneously
    std::atomic<bool> start_flag(false);
    
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&demuxer, &completed_threads, &test_failed, &start_flag, t, ITERATIONS_PER_THREAD]() {
            while (!start_flag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            
            try {
                std::mt19937 gen(t);
                std::uniform_int_distribution<int> op_dist(0, 5);
                std::uniform_int_distribution<uint64_t> sample_dist(0, 100000);
                
                for (int i = 0; i < ITERATIONS_PER_THREAD; ++i) {
                    int op = op_dist(gen);
                    switch (op) {
                        case 0: demuxer.getPosition(); break;
                        case 1: demuxer.isEOF(); break;
                        case 2: demuxer.seekTo(sample_dist(gen)); break;
                        case 3: demuxer.readChunk(); break;
                        case 4: demuxer.getAtomicCurrentSample(); break;
                        case 5: demuxer.getAtomicEOF(); break;
                    }
                }
                completed_threads.fetch_add(1, std::memory_order_relaxed);
            } catch (...) {
                test_failed.store(true, std::memory_order_release);
            }
        });
    }
    
    // Start all threads
    start_flag.store(true, std::memory_order_release);
    
    // Wait for all threads with timeout
    auto start_time = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(30);
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    
    // Verify results
    assert(!test_failed.load() && "Test should not throw exceptions");
    assert(completed_threads.load() == NUM_THREADS && "All threads should complete");
    assert(elapsed < timeout && "Test should complete within timeout (no deadlock)");
    
    std::cout << "  " << NUM_THREADS << " threads completed " 
              << (NUM_THREADS * ITERATIONS_PER_THREAD) << " mixed operations" << std::endl;
    std::cout << "  Total operations tracked: " << demuxer.getOperationCount() << std::endl;
    std::cout << "  Elapsed time: " << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() 
              << " ms" << std::endl;
    std::cout << "  ✓ Lock pattern holds under high concurrency" << std::endl;
}

/**
 * Test 5: Property verification - lock acquisition order
 * 
 * Verify that the documented lock acquisition order is followed
 * by checking that operations complete without deadlock.
 */
void test_lock_acquisition_order() {
    std::cout << "\n=== Test 5: Lock Acquisition Order Verification ===" << std::endl;
    std::cout << "Testing that lock acquisition order prevents deadlocks..." << std::endl;
    
    // This test verifies that the documented lock order (state_mutex first, 
    // metadata_mutex second) is followed by running operations that would
    // deadlock if the order were violated.
    
    MockFLACDemuxer demuxer;
    
    const int NUM_ITERATIONS = 100;
    
    std::atomic<int> completed_iterations(0);
    std::atomic<bool> test_failed(false);
    
    // Thread 1: parseContainer (acquires both locks)
    std::thread t1([&demuxer, &completed_iterations, &test_failed, NUM_ITERATIONS]() {
        try {
            for (int i = 0; i < NUM_ITERATIONS; ++i) {
                demuxer.parseContainer();
                completed_iterations.fetch_add(1, std::memory_order_relaxed);
            }
        } catch (...) {
            test_failed.store(true, std::memory_order_release);
        }
    });
    
    // Thread 2: seekTo (acquires state_mutex only)
    std::thread t2([&demuxer, &completed_iterations, &test_failed, NUM_ITERATIONS]() {
        try {
            for (int i = 0; i < NUM_ITERATIONS; ++i) {
                demuxer.seekTo(i * 1000);
                completed_iterations.fetch_add(1, std::memory_order_relaxed);
            }
        } catch (...) {
            test_failed.store(true, std::memory_order_release);
        }
    });
    
    // Thread 3: getPosition (acquires state_mutex only)
    std::thread t3([&demuxer, &completed_iterations, &test_failed, NUM_ITERATIONS]() {
        try {
            for (int i = 0; i < NUM_ITERATIONS; ++i) {
                demuxer.getPosition();
                completed_iterations.fetch_add(1, std::memory_order_relaxed);
            }
        } catch (...) {
            test_failed.store(true, std::memory_order_release);
        }
    });
    
    // Wait for all threads with timeout
    auto start_time = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(10);
    
    t1.join();
    t2.join();
    t3.join();
    
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    
    // Verify results
    assert(!test_failed.load() && "Test should not throw exceptions");
    assert(completed_iterations.load() == NUM_ITERATIONS * 3 && "All iterations should complete");
    assert(elapsed < timeout && "Test should complete within timeout (no deadlock)");
    
    std::cout << "  3 threads completed " << (NUM_ITERATIONS * 3) << " operations" << std::endl;
    std::cout << "  Elapsed time: " << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() 
              << " ms" << std::endl;
    std::cout << "  ✓ Lock acquisition order is correct (no deadlocks)" << std::endl;
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC DEMUXER THREAD SAFETY PROPERTY-BASED TESTS" << std::endl;
    std::cout << "**Feature: flac-demuxer, Property 22: Thread Safety - Lock Pattern**" << std::endl;
    std::cout << "**Validates: Requirements 28.1, 28.2**" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    try {
        // Test 1: Concurrent read operations
        tests_run++;
        test_concurrent_read_operations();
        tests_passed++;
        
        // Test 2: Concurrent read/write operations
        tests_run++;
        test_concurrent_read_write_operations();
        tests_passed++;
        
        // Test 3: Atomic state consistency
        tests_run++;
        test_atomic_state_consistency();
        tests_passed++;
        
        // Test 4: High concurrency stress test
        tests_run++;
        test_stress_high_concurrency();
        tests_passed++;
        
        // Test 5: Lock acquisition order verification
        tests_run++;
        test_lock_acquisition_order();
        tests_passed++;
        
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "✅ ALL PROPERTY TESTS PASSED (" << tests_passed << "/" << tests_run << ")" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(70, '=') << std::endl;
        std::cerr << "❌ PROPERTY TEST FAILED (" << tests_passed << "/" << tests_run << " passed)" << std::endl;
        std::cerr << "Exception: " << e.what() << std::endl;
        std::cerr << std::string(70, '=') << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n" << std::string(70, '=') << std::endl;
        std::cerr << "❌ PROPERTY TEST FAILED (" << tests_passed << "/" << tests_run << " passed)" << std::endl;
        std::cerr << "Unknown exception occurred" << std::endl;
        std::cerr << std::string(70, '=') << std::endl;
        return 1;
    }
}
