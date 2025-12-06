/*
 * test_framework_threading.h - Threading safety test utilities for PsyMP3
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef TEST_FRAMEWORK_THREADING_H
#define TEST_FRAMEWORK_THREADING_H

#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <future>
#include <random>
#include <memory>
#include <map>
#include <string>

namespace TestFramework {
namespace Threading {

/**
 * @brief Utility class for testing thread safety and lock contention
 */
class ThreadSafetyTester {
public:
    /**
     * @brief Configuration for thread safety tests
     */
    struct Config {
        size_t num_threads;
        std::chrono::milliseconds test_duration;
        size_t operations_per_thread;
        bool enable_random_delays;
        std::chrono::microseconds min_delay;
        std::chrono::microseconds max_delay;
        
        Config() : num_threads(4), test_duration(1000), operations_per_thread(100), 
                   enable_random_delays(true), min_delay(1), max_delay(100) {}
    };
    
    /**
     * @brief Results from thread safety testing
     */
    struct Results {
        size_t total_operations = 0;
        size_t successful_operations = 0;
        size_t failed_operations = 0;
        std::chrono::milliseconds total_duration{0};
        std::chrono::milliseconds average_operation_time{0};
        std::chrono::milliseconds max_operation_time{0};
        std::chrono::milliseconds min_operation_time{std::chrono::milliseconds::max()};
        size_t lock_contentions = 0;
        bool deadlock_detected = false;
        std::vector<std::string> error_messages;
    };
    
    /**
     * @brief Test function type - should return true on success, false on failure
     */
    using TestFunction = std::function<bool()>;
    
    /**
     * @brief Constructor
     * @param config Test configuration
     */
    explicit ThreadSafetyTester(const Config& config = Config());
    
    /**
     * @brief Run thread safety test with given function
     * @param test_func Function to test for thread safety
     * @param test_name Name of the test for reporting
     * @return Test results
     */
    Results runTest(TestFunction test_func, const std::string& test_name = "");
    
    /**
     * @brief Run stress test with multiple different operations
     * @param operations Map of operation name to test function
     * @param test_name Name of the test for reporting
     * @return Test results
     */
    Results runStressTest(const std::map<std::string, TestFunction>& operations, 
                         const std::string& test_name = "");
    
    /**
     * @brief Test for deadlock detection
     * @param setup_func Function to set up the deadlock scenario
     * @param timeout Maximum time to wait for deadlock
     * @return true if deadlock was detected, false otherwise
     */
    bool testForDeadlock(std::function<void()> setup_func, 
                        std::chrono::milliseconds timeout = std::chrono::milliseconds{5000});
    
    /**
     * @brief Test lock contention measurement
     * @param lock_func Function that acquires and releases a lock
     * @param contention_threads Number of threads to create contention
     * @return Average lock acquisition time
     */
    std::chrono::microseconds measureLockContention(std::function<void()> lock_func, 
                                                   size_t contention_threads = 8);

private:
    Config m_config;
    std::mt19937 m_random_generator;
    
    void workerThread(TestFunction test_func, 
                     std::atomic<size_t>& operations_count,
                     std::atomic<size_t>& success_count,
                     std::atomic<size_t>& failure_count,
                     std::atomic<bool>& should_stop,
                     std::vector<std::chrono::microseconds>& operation_times,
                     std::mutex& times_mutex);
    
    void addRandomDelay();
};

/**
 * @brief Barrier synchronization primitive for coordinating test threads
 */
class TestBarrier {
public:
    explicit TestBarrier(size_t thread_count);
    
    /**
     * @brief Wait for all threads to reach the barrier
     */
    void wait();
    
    /**
     * @brief Reset the barrier for reuse
     */
    void reset();

private:
    size_t m_thread_count;
    size_t m_waiting_count;
    size_t m_generation;
    std::mutex m_mutex;
    std::condition_variable m_cv;
};

/**
 * @brief Lock contention analyzer for measuring performance impact
 */
class LockContentionAnalyzer {
public:
    struct ContentionMetrics {
        std::chrono::microseconds average_acquisition_time{0};
        std::chrono::microseconds max_acquisition_time{0};
        std::chrono::microseconds min_acquisition_time{std::chrono::microseconds::max()};
        size_t total_acquisitions = 0;
        size_t contentions_detected = 0;
        double contention_ratio = 0.0; // contentions / total_acquisitions
    };
    
    /**
     * @brief Measure lock contention for a given mutex
     * @param mutex Mutex to analyze
     * @param test_duration How long to run the test
     * @param num_threads Number of threads to create contention
     * @return Contention metrics
     */
    template<typename MutexType>
    ContentionMetrics analyzeLockContention(MutexType& mutex,
                                          std::chrono::milliseconds test_duration,
                                          size_t num_threads = 4) {
        ContentionMetrics metrics;
        std::atomic<bool> should_stop{false};
        std::vector<std::thread> threads;
        std::vector<std::chrono::microseconds> acquisition_times;
        std::mutex times_mutex;
        
        // Start worker threads
        for (size_t i = 0; i < num_threads; ++i) {
            threads.emplace_back([&]() {
                while (!should_stop.load()) {
                    auto start = std::chrono::high_resolution_clock::now();
                    
                    {
                        std::lock_guard<MutexType> lock(mutex);
                        // Simulate some work
                        std::this_thread::sleep_for(std::chrono::microseconds(10));
                    }
                    
                    auto end = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                    
                    {
                        std::lock_guard<std::mutex> times_lock(times_mutex);
                        acquisition_times.push_back(duration);
                    }
                }
            });
        }
        
        // Let test run for specified duration
        std::this_thread::sleep_for(test_duration);
        should_stop.store(true);
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Calculate metrics
        if (!acquisition_times.empty()) {
            metrics.total_acquisitions = acquisition_times.size();
            
            auto total_time = std::chrono::microseconds{0};
            for (const auto& time : acquisition_times) {
                total_time += time;
                metrics.max_acquisition_time = std::max(metrics.max_acquisition_time, time);
                metrics.min_acquisition_time = std::min(metrics.min_acquisition_time, time);
                
                // Consider contention if acquisition took longer than expected
                if (time > std::chrono::microseconds{50}) {
                    metrics.contentions_detected++;
                }
            }
            
            metrics.average_acquisition_time = total_time / metrics.total_acquisitions;
            metrics.contention_ratio = static_cast<double>(metrics.contentions_detected) / metrics.total_acquisitions;
        }
        
        return metrics;
    }
};

/**
 * @brief Race condition detector for finding threading bugs
 */
class RaceConditionDetector {
public:
    /**
     * @brief Test for race conditions in shared data access
     * @param setup_func Function to set up shared data
     * @param test_func Function that accesses shared data
     * @param verify_func Function to verify data consistency
     * @param num_threads Number of threads to use
     * @param iterations Number of iterations per thread
     * @return true if race condition detected, false otherwise
     */
    template<typename SetupFunc, typename TestFunc, typename VerifyFunc>
    bool detectRaceCondition(SetupFunc setup_func,
                           TestFunc test_func,
                           VerifyFunc verify_func,
                           size_t num_threads = 4,
                           size_t iterations = 1000) {
        bool race_detected = false;
        
        for (size_t test_run = 0; test_run < 10 && !race_detected; ++test_run) {
            // Set up test data
            setup_func();
            
            // Create barrier for synchronized start
            TestBarrier barrier(num_threads);
            std::vector<std::thread> threads;
            std::atomic<bool> start_flag{false};
            
            // Start worker threads
            for (size_t i = 0; i < num_threads; ++i) {
                threads.emplace_back([&, i]() {
                    // Wait for synchronized start
                    barrier.wait();
                    
                    // Run test iterations
                    for (size_t iter = 0; iter < iterations; ++iter) {
                        test_func(i, iter);
                    }
                });
            }
            
            // Wait for all threads to complete
            for (auto& thread : threads) {
                thread.join();
            }
            
            // Verify data consistency
            if (!verify_func()) {
                race_detected = true;
            }
        }
        
        return race_detected;
    }
};

/**
 * @brief Performance benchmarking utilities for threading code
 */
class ThreadingBenchmark {
public:
    struct BenchmarkResults {
        std::chrono::microseconds single_thread_time{0};
        std::chrono::microseconds multi_thread_time{0};
        double speedup_ratio = 0.0;
        double efficiency = 0.0; // speedup / num_threads
        size_t operations_per_second = 0;
    };
    
    /**
     * @brief Benchmark single-threaded vs multi-threaded performance
     * @param operation Function to benchmark
     * @param num_operations Total number of operations
     * @param num_threads Number of threads for multi-threaded test
     * @return Benchmark results
     */
    template<typename OperationFunc>
    BenchmarkResults benchmarkScaling(OperationFunc operation,
                                    size_t num_operations,
                                    size_t num_threads = 4) {
        BenchmarkResults results;
        
        // Single-threaded benchmark
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < num_operations; ++i) {
            operation(i);
        }
        auto end = std::chrono::high_resolution_clock::now();
        results.single_thread_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        // Multi-threaded benchmark
        start = std::chrono::high_resolution_clock::now();
        std::vector<std::thread> threads;
        size_t operations_per_thread = num_operations / num_threads;
        
        for (size_t t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                size_t start_op = t * operations_per_thread;
                size_t end_op = (t == num_threads - 1) ? num_operations : (t + 1) * operations_per_thread;
                
                for (size_t i = start_op; i < end_op; ++i) {
                    operation(i);
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        end = std::chrono::high_resolution_clock::now();
        results.multi_thread_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        // Calculate metrics
        if (results.multi_thread_time.count() > 0) {
            results.speedup_ratio = static_cast<double>(results.single_thread_time.count()) / 
                                   results.multi_thread_time.count();
            results.efficiency = results.speedup_ratio / num_threads;
        }
        
        if (results.multi_thread_time.count() > 0) {
            results.operations_per_second = (num_operations * 1000000) / results.multi_thread_time.count();
        }
        
        return results;
    }
};

} // namespace Threading
} // namespace TestFramework

// Namespace alias for backward compatibility
namespace ThreadingTest = TestFramework::Threading;

#endif // TEST_FRAMEWORK_THREADING_H