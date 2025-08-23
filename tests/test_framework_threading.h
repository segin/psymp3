#ifndef TEST_FRAMEWORK_THREADING_H
#define TEST_FRAMEWORK_THREADING_H

/**
 * Threading Safety Test Framework for PsyMP3
 * 
 * This framework provides utilities for testing thread safety patterns
 * and validating the public/private lock pattern implementation.
 * 
 * Requirements addressed: 1.1, 1.3, 5.1
 */

#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <random>
#include <cassert>
#include <iostream>

namespace ThreadingTest {

/**
 * Configuration for threading tests
 */
struct TestConfig {
    int num_threads = 8;
    int operations_per_thread = 1000;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(5000);
    bool enable_stress_testing = false;
    int stress_duration_seconds = 10;
};

/**
 * Results from a threading test
 */
struct TestResults {
    bool success = false;
    int total_operations = 0;
    int failed_operations = 0;
    std::chrono::milliseconds duration{0};
    std::vector<std::string> errors;
    
    void addError(const std::string& error) {
        errors.push_back(error);
        failed_operations++;
    }
    
    double getSuccessRate() const {
        if (total_operations == 0) return 0.0;
        return (double)(total_operations - failed_operations) / total_operations;
    }
};

/**
 * Barrier for synchronizing thread starts
 */
class ThreadBarrier {
private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    int m_count;
    int m_waiting;
    
public:
    explicit ThreadBarrier(int count) : m_count(count), m_waiting(0) {}
    
    void wait() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_waiting++;
        if (m_waiting == m_count) {
            m_cv.notify_all();
        } else {
            m_cv.wait(lock, [this] { return m_waiting == m_count; });
        }
    }
};

/**
 * Base class for threading safety tests
 */
class ThreadSafetyTestBase {
protected:
    TestConfig m_config;
    TestResults m_results;
    std::atomic<bool> m_should_stop{false};
    
public:
    explicit ThreadSafetyTestBase(const TestConfig& config = TestConfig{}) 
        : m_config(config) {}
    
    virtual ~ThreadSafetyTestBase() = default;
    
    /**
     * Run the threading test
     */
    TestResults run() {
        m_results = TestResults{};
        m_should_stop = false;
        
        auto start_time = std::chrono::steady_clock::now();
        
        try {
            runTest();
            m_results.success = (m_results.failed_operations == 0);
        } catch (const std::exception& e) {
            m_results.addError(std::string("Exception: ") + e.what());
            m_results.success = false;
        }
        
        auto end_time = std::chrono::steady_clock::now();
        m_results.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        return m_results;
    }
    
    /**
     * Get the test results
     */
    const TestResults& getResults() const { return m_results; }
    
protected:
    /**
     * Override this method to implement the actual test
     */
    virtual void runTest() = 0;
    
    /**
     * Helper method to run operations concurrently
     */
    void runConcurrentOperations(std::function<void(int thread_id)> operation) {
        std::vector<std::thread> threads;
        std::atomic<int> error_count{0};
        ThreadBarrier barrier(m_config.num_threads);
        
        // Start threads
        for (int i = 0; i < m_config.num_threads; ++i) {
            threads.emplace_back([this, &operation, &error_count, &barrier, i]() {
                try {
                    barrier.wait(); // Synchronize thread starts
                    
                    for (int j = 0; j < m_config.operations_per_thread && !m_should_stop; ++j) {
                        operation(i);
                        m_results.total_operations++;
                    }
                } catch (const std::exception& e) {
                    error_count++;
                    m_results.addError(std::string("Thread ") + std::to_string(i) + 
                                     " error: " + e.what());
                }
            });
        }
        
        // Wait for completion or timeout
        auto timeout_thread = std::thread([this]() {
            std::this_thread::sleep_for(m_config.timeout);
            m_should_stop = true;
        });
        
        // Join all threads
        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }
        
        m_should_stop = true;
        if (timeout_thread.joinable()) {
            timeout_thread.join();
        }
        
        if (error_count > 0) {
            m_results.addError("Concurrent operations failed");
        }
    }
};

/**
 * Test for basic concurrent access to public methods
 */
template<typename T>
class ConcurrentAccessTest : public ThreadSafetyTestBase {
private:
    T* m_test_object;
    std::function<void(T*, int)> m_operation;
    
public:
    ConcurrentAccessTest(T* test_object, 
                        std::function<void(T*, int)> operation,
                        const TestConfig& config = TestConfig{})
        : ThreadSafetyTestBase(config), m_test_object(test_object), m_operation(operation) {}
    
protected:
    void runTest() override {
        runConcurrentOperations([this](int thread_id) {
            m_operation(m_test_object, thread_id);
        });
    }
};

/**
 * Test for deadlock detection
 */
template<typename T>
class DeadlockDetectionTest : public ThreadSafetyTestBase {
private:
    T* m_test_object;
    std::function<void(T*, int)> m_deadlock_operation;
    
public:
    DeadlockDetectionTest(T* test_object,
                         std::function<void(T*, int)> deadlock_operation,
                         const TestConfig& config = TestConfig{})
        : ThreadSafetyTestBase(config), m_test_object(test_object), 
          m_deadlock_operation(deadlock_operation) {}
    
protected:
    void runTest() override {
        // This test should complete without hanging
        // If it hangs, it indicates a deadlock
        runConcurrentOperations([this](int thread_id) {
            m_deadlock_operation(m_test_object, thread_id);
        });
    }
};

/**
 * Stress test for high-concurrency scenarios
 */
template<typename T>
class StressTest : public ThreadSafetyTestBase {
private:
    T* m_test_object;
    std::vector<std::function<void(T*, int)>> m_operations;
    std::mt19937 m_rng;
    
public:
    StressTest(T* test_object,
               std::vector<std::function<void(T*, int)>> operations,
               const TestConfig& config = TestConfig{})
        : ThreadSafetyTestBase(config), m_test_object(test_object), 
          m_operations(operations), m_rng(std::random_device{}()) {}
    
protected:
    void runTest() override {
        if (m_operations.empty()) {
            m_results.addError("No operations provided for stress test");
            return;
        }
        
        runConcurrentOperations([this](int thread_id) {
            // Randomly select an operation to perform
            std::uniform_int_distribution<size_t> dist(0, m_operations.size() - 1);
            size_t op_index = dist(m_rng);
            m_operations[op_index](m_test_object, thread_id);
        });
    }
};

/**
 * Utility class for measuring performance impact of threading changes
 */
class PerformanceBenchmark {
private:
    std::string m_test_name;
    std::chrono::steady_clock::time_point m_start_time;
    
public:
    explicit PerformanceBenchmark(const std::string& test_name) 
        : m_test_name(test_name) {
        m_start_time = std::chrono::steady_clock::now();
    }
    
    ~PerformanceBenchmark() {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - m_start_time);
        
        std::cout << "Benchmark [" << m_test_name << "]: " 
                  << duration.count() << " microseconds" << std::endl;
    }
};

/**
 * Macro for easy performance benchmarking
 */
#define BENCHMARK(name) PerformanceBenchmark _bench(name)

/**
 * Test runner for executing multiple threading tests
 */
class ThreadingTestRunner {
private:
    std::vector<std::unique_ptr<ThreadSafetyTestBase>> m_tests;
    
public:
    void addTest(std::unique_ptr<ThreadSafetyTestBase> test) {
        m_tests.push_back(std::move(test));
    }
    
    bool runAllTests() {
        bool all_passed = true;
        int test_number = 1;
        
        std::cout << "Running " << m_tests.size() << " threading safety tests..." << std::endl;
        
        for (auto& test : m_tests) {
            std::cout << "Test " << test_number << "/" << m_tests.size() << "... ";
            
            auto results = test->run();
            
            if (results.success) {
                std::cout << "PASSED";
            } else {
                std::cout << "FAILED";
                all_passed = false;
            }
            
            std::cout << " (" << results.duration.count() << "ms, "
                      << results.total_operations << " ops, "
                      << (results.getSuccessRate() * 100.0) << "% success)" << std::endl;
            
            if (!results.errors.empty()) {
                for (const auto& error : results.errors) {
                    std::cout << "  Error: " << error << std::endl;
                }
            }
            
            test_number++;
        }
        
        std::cout << std::endl;
        std::cout << "Threading safety tests: " 
                  << (all_passed ? "ALL PASSED" : "SOME FAILED") << std::endl;
        
        return all_passed;
    }
};

} // namespace ThreadingTest

// Convenience macros for common test patterns
#define THREADING_TEST_CONCURRENT_ACCESS(object, operation) \
    ThreadingTest::ConcurrentAccessTest<decltype(object)>(&object, operation)

#define THREADING_TEST_DEADLOCK_DETECTION(object, operation) \
    ThreadingTest::DeadlockDetectionTest<decltype(object)>(&object, operation)

#define THREADING_TEST_STRESS(object, operations) \
    ThreadingTest::StressTest<decltype(object)>(&object, operations)

#endif // TEST_FRAMEWORK_THREADING_H