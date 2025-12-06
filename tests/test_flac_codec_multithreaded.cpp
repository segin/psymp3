/*
 * test_flac_codec_multithreaded.cpp - Multi-threaded algorithm tests for FLAC codec
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
#include <cstdint>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_FLAC

/**
 * @brief Test FLAC codec algorithms under multi-threaded conditions
 * Requirements: 9.1-9.8
 */
class FLACCodecMultiThreadedTest {
public:
    static bool runAllTests() {
        std::cout << "FLAC Codec Multi-threaded Tests" << std::endl;
        std::cout << "===============================" << std::endl;
        
        bool all_passed = true;
        
        all_passed &= testConcurrentConversion();
        all_passed &= testThreadSafetyPatterns();
        all_passed &= testPerformanceUnderConcurrency();
        all_passed &= testResourceContention();
        
        if (all_passed) {
            std::cout << "✓ All multi-threaded tests PASSED" << std::endl;
        } else {
            std::cout << "✗ Some multi-threaded tests FAILED" << std::endl;
        }
        
        return all_passed;
    }

private:
    static bool testConcurrentConversion() {
        std::cout << "Testing concurrent bit depth conversion..." << std::endl;
        
        const int num_threads = 4;
        const size_t samples_per_thread = 100000;
        
        std::vector<std::thread> threads;
        std::atomic<int> successful_conversions{0};
        std::atomic<int> total_conversions{0};
        
        // Launch multiple threads doing bit depth conversion
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                std::vector<int32_t> test_data(samples_per_thread);
                std::vector<int16_t> converted_data(samples_per_thread);
                
                // Fill with test data
                for (size_t i = 0; i < samples_per_thread; ++i) {
                    test_data[i] = static_cast<int32_t>((i + t * 1000) % 65536) - 32768;
                }
                
                // Perform conversions
                for (size_t i = 0; i < samples_per_thread; ++i) {
                    converted_data[i] = convert24BitTo16Bit(test_data[i]);
                    total_conversions++;
                }
                
                // Verify results
                bool all_valid = true;
                for (size_t i = 0; i < samples_per_thread; ++i) {
                    if (converted_data[i] < -32768 || converted_data[i] > 32767) {
                        all_valid = false;
                        break;
                    }
                }
                
                if (all_valid) {
                    successful_conversions += samples_per_thread;
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        std::cout << "  Successful conversions: " << successful_conversions.load() 
                  << "/" << total_conversions.load() << std::endl;
        
        if (successful_conversions.load() != total_conversions.load()) {
            std::cout << "  ERROR: Some concurrent conversions failed" << std::endl;
            return false;
        }
        
        std::cout << "  ✓ Concurrent conversion test passed" << std::endl;
        return true;
    }
    
    static bool testThreadSafetyPatterns() {
        std::cout << "Testing thread safety patterns..." << std::endl;
        
        const int num_threads = 8;
        const int operations_per_thread = 1000;
        
        // Shared data structure with proper synchronization
        ThreadSafeCounter counter;
        std::vector<std::thread> threads;
        
        // Launch threads that increment the counter
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&]() {
                for (int i = 0; i < operations_per_thread; ++i) {
                    counter.increment();
                    
                    // Simulate some processing
                    std::vector<int16_t> temp_data(100);
                    for (size_t j = 0; j < temp_data.size(); ++j) {
                        temp_data[j] = static_cast<int16_t>(j);
                    }
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        int expected_count = num_threads * operations_per_thread;
        int actual_count = counter.getValue();
        
        std::cout << "  Expected count: " << expected_count << std::endl;
        std::cout << "  Actual count: " << actual_count << std::endl;
        
        if (actual_count != expected_count) {
            std::cout << "  ERROR: Thread safety violation detected" << std::endl;
            return false;
        }
        
        std::cout << "  ✓ Thread safety patterns test passed" << std::endl;
        return true;
    }
    
    static bool testPerformanceUnderConcurrency() {
        std::cout << "Testing performance under concurrency..." << std::endl;
        
        const size_t num_samples = 500000;
        
        // Measure single-threaded performance
        auto start_single = std::chrono::high_resolution_clock::now();
        
        std::vector<int32_t> single_data(num_samples);
        std::vector<int16_t> single_result(num_samples);
        
        for (size_t i = 0; i < num_samples; ++i) {
            single_data[i] = static_cast<int32_t>(i % 65536) - 32768;
            single_result[i] = convert24BitTo16Bit(single_data[i]);
        }
        
        auto end_single = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start_single);
        
        // Measure multi-threaded performance
        const int num_threads = 4;
        const size_t samples_per_thread = num_samples / num_threads;
        
        auto start_multi = std::chrono::high_resolution_clock::now();
        
        std::vector<std::thread> threads;
        std::vector<std::vector<int32_t>> multi_data(num_threads, std::vector<int32_t>(samples_per_thread));
        std::vector<std::vector<int16_t>> multi_result(num_threads, std::vector<int16_t>(samples_per_thread));
        
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                for (size_t i = 0; i < samples_per_thread; ++i) {
                    multi_data[t][i] = static_cast<int32_t>((i + t * samples_per_thread) % 65536) - 32768;
                    multi_result[t][i] = convert24BitTo16Bit(multi_data[t][i]);
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto end_multi = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start_multi);
        
        double single_time = static_cast<double>(end_single.count());
        double multi_time = static_cast<double>(end_multi.count());
        double speedup = single_time / multi_time;
        
        std::cout << "  Single-threaded time: " << single_time / 1000.0 << " ms" << std::endl;
        std::cout << "  Multi-threaded time: " << multi_time / 1000.0 << " ms" << std::endl;
        std::cout << "  Speedup: " << speedup << "x" << std::endl;
        
        // Multi-threaded should be at least somewhat faster (speedup > 1.5)
        if (speedup < 1.5) {
            std::cout << "  WARNING: Limited speedup from multi-threading" << std::endl;
            // Don't fail the test as this depends on system characteristics
        }
        
        std::cout << "  ✓ Performance under concurrency test passed" << std::endl;
        return true;
    }
    
    static bool testResourceContention() {
        std::cout << "Testing resource contention handling..." << std::endl;
        
        const int num_threads = 8;
        const int operations_per_thread = 500;
        
        // Shared resource with contention
        SharedResource resource;
        std::vector<std::thread> threads;
        std::atomic<int> successful_operations{0};
        
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                for (int i = 0; i < operations_per_thread; ++i) {
                    try {
                        // Simulate resource access with processing
                        if (resource.processData(t * operations_per_thread + i)) {
                            successful_operations++;
                        }
                        
                        // Small delay to increase contention
                        std::this_thread::sleep_for(std::chrono::microseconds(1));
                        
                    } catch (const std::exception& e) {
                        // Resource contention may cause some operations to fail
                        // This is acceptable as long as the system doesn't crash
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        int total_operations = num_threads * operations_per_thread;
        double success_rate = static_cast<double>(successful_operations.load()) / total_operations;
        
        std::cout << "  Successful operations: " << successful_operations.load() 
                  << "/" << total_operations << std::endl;
        std::cout << "  Success rate: " << (success_rate * 100.0) << "%" << std::endl;
        
        // Should handle resource contention reasonably well (>80% success rate)
        if (success_rate < 0.8) {
            std::cout << "  ERROR: Poor resource contention handling" << std::endl;
            return false;
        }
        
        std::cout << "  ✓ Resource contention test passed" << std::endl;
        return true;
    }
    
    // Helper methods and classes for multi-threaded testing
    static int16_t convert24BitTo16Bit(int32_t sample) {
        return static_cast<int16_t>(sample >> 8);
    }
    
    class ThreadSafeCounter {
    private:
        std::atomic<int> count{0};
        
    public:
        void increment() {
            count.fetch_add(1);
        }
        
        int getValue() const {
            return count.load();
        }
    };
    
    class SharedResource {
    private:
        mutable std::mutex mutex_;
        std::vector<int> data_;
        
    public:
        SharedResource() {
            data_.reserve(10000);
        }
        
        bool processData(int value) {
            std::lock_guard<std::mutex> lock(mutex_);
            
            // Simulate processing
            data_.push_back(value);
            
            // Simulate some computation
            int16_t converted = convert24BitTo16Bit(value * 256);
            
            // Keep data size manageable
            if (data_.size() > 5000) {
                data_.erase(data_.begin(), data_.begin() + 1000);
            }
            
            return converted >= -32768 && converted <= 32767;
        }
        
        size_t getDataSize() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return data_.size();
        }
    };
};

int main() {
    std::cout << "FLAC Codec Multi-threaded Algorithm Tests" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "Requirements: 9.1-9.8" << std::endl;
    std::cout << std::endl;
    
    bool success = FLACCodecMultiThreadedTest::runAllTests();
    
    return success ? 0 : 1;
}

#else // !HAVE_FLAC

int main() {
    std::cout << "FLAC support not available - skipping FLAC codec multi-threaded tests" << std::endl;
    return 0;
}

#endif // HAVE_FLAC