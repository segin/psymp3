/*
 * test_framework_threading.cpp - Threading safety test utilities implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework_threading.h"

namespace TestFramework {
namespace Threading {

// ThreadSafetyTester implementation

ThreadSafetyTester::ThreadSafetyTester(const Config& config) 
    : m_config(config), m_random_generator(std::random_device{}()) {
}

ThreadSafetyTester::Results ThreadSafetyTester::runTest(TestFunction test_func, const std::string& test_name) {
    Results results;
    results.total_duration = m_config.test_duration;
    
    std::atomic<size_t> operations_count{0};
    std::atomic<size_t> success_count{0};
    std::atomic<size_t> failure_count{0};
    std::atomic<bool> should_stop{false};
    
    std::vector<std::vector<std::chrono::microseconds>> thread_times(m_config.num_threads);
    std::vector<std::mutex> times_mutexes(m_config.num_threads);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Start worker threads
    std::vector<std::thread> threads;
    for (size_t i = 0; i < m_config.num_threads; ++i) {
        threads.emplace_back([this, &test_func, &operations_count, &success_count, 
                             &failure_count, &should_stop, &thread_times, &times_mutexes, i]() {
            workerThread(test_func, operations_count, success_count, failure_count, 
                        should_stop, thread_times[i], times_mutexes[i]);
        });
    }
    
    // Let test run for specified duration
    std::this_thread::sleep_for(m_config.test_duration);
    should_stop.store(true);
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    results.total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Collect results
    results.total_operations = operations_count.load();
    results.successful_operations = success_count.load();
    results.failed_operations = failure_count.load();
    
    // Calculate timing statistics
    std::vector<std::chrono::microseconds> all_times;
    for (size_t i = 0; i < m_config.num_threads; ++i) {
        std::lock_guard<std::mutex> lock(times_mutexes[i]);
        all_times.insert(all_times.end(), thread_times[i].begin(), thread_times[i].end());
    }
    
    if (!all_times.empty()) {
        auto total_time = std::chrono::microseconds{0};
        for (const auto& time : all_times) {
            total_time += time;
            results.max_operation_time = std::max(results.max_operation_time, 
                                                 std::chrono::duration_cast<std::chrono::milliseconds>(time));
            results.min_operation_time = std::min(results.min_operation_time, 
                                                 std::chrono::duration_cast<std::chrono::milliseconds>(time));
        }
        results.average_operation_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            total_time / all_times.size());
    }
    
    return results;
}

ThreadSafetyTester::Results ThreadSafetyTester::runStressTest(
    const std::map<std::string, TestFunction>& operations, const std::string& test_name) {
    
    Results results;
    
    // Create a combined test function that randomly selects operations
    std::vector<std::pair<std::string, TestFunction>> operation_list;
    for (const auto& op : operations) {
        operation_list.push_back(op);
    }
    
    auto combined_test = [this, &operation_list]() -> bool {
        if (operation_list.empty()) return false;
        
        std::uniform_int_distribution<size_t> dist(0, operation_list.size() - 1);
        size_t index = dist(m_random_generator);
        
        return operation_list[index].second();
    };
    
    return runTest(combined_test, test_name);
}

bool ThreadSafetyTester::testForDeadlock(std::function<void()> setup_func, 
                                        std::chrono::milliseconds timeout) {
    std::atomic<bool> deadlock_detected{false};
    std::atomic<bool> test_completed{false};
    
    // Run the setup function in a separate thread
    std::thread test_thread([&setup_func, &test_completed, &deadlock_detected]() {
        try {
            setup_func();
            test_completed.store(true);
        } catch (...) {
            // Exception occurred, but not necessarily a deadlock
            test_completed.store(true);
        }
    });
    
    // Wait for completion or timeout
    auto start = std::chrono::high_resolution_clock::now();
    while (!test_completed.load()) {
        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        if (elapsed > timeout) {
            deadlock_detected.store(true);
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Clean up
    if (test_thread.joinable()) {
        if (deadlock_detected.load()) {
            // Force terminate the thread (not ideal, but necessary for deadlock detection)
            test_thread.detach();
        } else {
            test_thread.join();
        }
    }
    
    return deadlock_detected.load();
}

std::chrono::microseconds ThreadSafetyTester::measureLockContention(
    std::function<void()> lock_func, size_t contention_threads) {
    
    std::vector<std::chrono::microseconds> acquisition_times;
    std::mutex times_mutex;
    std::atomic<bool> should_stop{false};
    
    // Start contention threads
    std::vector<std::thread> threads;
    for (size_t i = 0; i < contention_threads; ++i) {
        threads.emplace_back([&lock_func, &acquisition_times, &times_mutex, &should_stop]() {
            while (!should_stop.load()) {
                auto start = std::chrono::high_resolution_clock::now();
                lock_func();
                auto end = std::chrono::high_resolution_clock::now();
                
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                
                {
                    std::lock_guard<std::mutex> lock(times_mutex);
                    acquisition_times.push_back(duration);
                }
            }
        });
    }
    
    // Let contention run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    should_stop.store(true);
    
    // Wait for threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Calculate average acquisition time
    if (acquisition_times.empty()) {
        return std::chrono::microseconds{0};
    }
    
    auto total_time = std::chrono::microseconds{0};
    for (const auto& time : acquisition_times) {
        total_time += time;
    }
    
    return total_time / acquisition_times.size();
}

void ThreadSafetyTester::workerThread(TestFunction test_func, 
                                    std::atomic<size_t>& operations_count,
                                    std::atomic<size_t>& success_count,
                                    std::atomic<size_t>& failure_count,
                                    std::atomic<bool>& should_stop,
                                    std::vector<std::chrono::microseconds>& operation_times,
                                    std::mutex& times_mutex) {
    size_t local_operations = 0;
    
    while (!should_stop.load() && local_operations < m_config.operations_per_thread) {
        auto start = std::chrono::high_resolution_clock::now();
        
        bool success = false;
        try {
            success = test_func();
        } catch (...) {
            success = false;
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        {
            std::lock_guard<std::mutex> lock(times_mutex);
            operation_times.push_back(duration);
        }
        
        operations_count.fetch_add(1);
        if (success) {
            success_count.fetch_add(1);
        } else {
            failure_count.fetch_add(1);
        }
        
        local_operations++;
        
        if (m_config.enable_random_delays) {
            addRandomDelay();
        }
    }
}

void ThreadSafetyTester::addRandomDelay() {
    std::uniform_int_distribution<int> dist(
        static_cast<int>(m_config.min_delay.count()),
        static_cast<int>(m_config.max_delay.count())
    );
    
    auto delay = std::chrono::microseconds(dist(m_random_generator));
    std::this_thread::sleep_for(delay);
}

// TestBarrier implementation

TestBarrier::TestBarrier(size_t thread_count) 
    : m_thread_count(thread_count), m_waiting_count(0), m_generation(0) {
}

void TestBarrier::wait() {
    std::unique_lock<std::mutex> lock(m_mutex);
    size_t current_generation = m_generation;
    
    if (++m_waiting_count == m_thread_count) {
        // Last thread to arrive - wake everyone up
        m_waiting_count = 0;
        m_generation++;
        m_cv.notify_all();
    } else {
        // Wait for all threads to arrive
        m_cv.wait(lock, [this, current_generation]() {
            return m_generation != current_generation;
        });
    }
}

void TestBarrier::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_waiting_count = 0;
    m_generation = 0;
}

} // namespace Threading
} // namespace TestFramework