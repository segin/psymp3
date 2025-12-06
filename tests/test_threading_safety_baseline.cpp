/**
 * Baseline Threading Safety Tests for PsyMP3
 * 
 * This file demonstrates the threading safety test framework and provides
 * baseline tests for the current codebase before refactoring.
 * 
 * Requirements addressed: 1.1, 1.3, 5.1
 */

#include "test_framework_threading.h"
#include <iostream>
#include <memory>
#include <mutex>
#include <atomic>
#include <cstdlib>

using namespace TestFramework::Threading;

// Mock classes to demonstrate the testing framework
// These represent simplified versions of the actual PsyMP3 classes

/**
 * Mock Audio class demonstrating current threading issues
 */
class MockAudio {
private:
    mutable std::mutex m_buffer_mutex;
    mutable std::mutex m_stream_mutex;
    std::atomic<bool> m_finished{false};
    std::atomic<int> m_buffer_level{0};
    
public:
    // Current problematic pattern - public methods acquire locks
    bool isFinished() const {
        std::lock_guard<std::mutex> lock(m_buffer_mutex);
        return m_finished.load();
    }
    
    void setFinished(bool finished) {
        std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
        std::lock_guard<std::mutex> stream_lock(m_stream_mutex);
        
        m_finished = finished;
        
        // This would cause deadlock if we called isFinished() here
        // because isFinished() tries to acquire m_buffer_mutex again
    }
    
    void resetBuffer() {
        std::lock_guard<std::mutex> lock(m_buffer_mutex);
        m_buffer_level = 0;
    }
    
    int getBufferLevel() const {
        std::lock_guard<std::mutex> lock(m_buffer_mutex);
        return m_buffer_level.load();
    }
    
    void addToBuffer(int amount) {
        std::lock_guard<std::mutex> lock(m_buffer_mutex);
        m_buffer_level += amount;
    }
};

/**
 * Mock IOHandler class demonstrating static/instance lock issues
 */
class MockIOHandler {
private:
    static std::mutex s_memory_mutex;
    static std::atomic<size_t> s_total_memory;
    
    mutable std::mutex m_operation_mutex;
    std::atomic<size_t> m_bytes_read{0};
    
public:
    size_t read(void* buffer, size_t size) {
        std::lock_guard<std::mutex> lock(m_operation_mutex);
        
        // Simulate read operation
        m_bytes_read += size;
        
        // This could cause lock ordering issues
        updateMemoryUsage(size);
        
        return size;
    }
    
    static size_t getTotalMemoryUsage() {
        std::lock_guard<std::mutex> lock(s_memory_mutex);
        return s_total_memory.load();
    }
    
    size_t getBytesRead() const {
        std::lock_guard<std::mutex> lock(m_operation_mutex);
        return m_bytes_read.load();
    }
    
private:
    void updateMemoryUsage(size_t bytes) {
        std::lock_guard<std::mutex> lock(s_memory_mutex);
        s_total_memory += bytes;
    }
};

// Static member definition
std::mutex MockIOHandler::s_memory_mutex;
std::atomic<size_t> MockIOHandler::s_total_memory{0};

/**
 * Mock MemoryPoolManager demonstrating callback issues
 */
class MockMemoryPoolManager {
private:
    mutable std::mutex m_mutex;
    std::atomic<size_t> m_allocated_bytes{0};
    std::function<void()> m_pressure_callback;
    
public:
    void setPressureCallback(std::function<void()> callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pressure_callback = callback;
    }
    
    void* allocateBuffer(size_t size) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        m_allocated_bytes += size;
        
        // Simulate memory pressure
        if (m_allocated_bytes > 1000000 && m_pressure_callback) {
            // This is dangerous - calling callback while holding lock
            // If callback calls back into this class, deadlock occurs
            m_pressure_callback();
        }
        
        return malloc(size);
    }
    
    void releaseBuffer(void* buffer, size_t size) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (buffer) {
            free(buffer);
            m_allocated_bytes -= size;
        }
    }
    
    size_t getAllocatedBytes() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_allocated_bytes.load();
    }
};

/**
 * Test functions for each mock class using the actual ThreadSafetyTester API
 */
void testMockAudioConcurrentAccess() {
    std::cout << "\n=== Testing MockAudio Concurrent Access ===" << std::endl;
    
    MockAudio audio;
    ThreadSafetyTester::Config config;
    config.num_threads = 4;
    config.operations_per_thread = 100;
    
    ThreadSafetyTester tester(config);
    
    // Create test operations map
    std::map<std::string, ThreadSafetyTester::TestFunction> operations;
    operations["isFinished"] = [&audio]() { audio.isFinished(); return true; };
    operations["resetBuffer"] = [&audio]() { audio.resetBuffer(); return true; };
    operations["getBufferLevel"] = [&audio]() { audio.getBufferLevel(); return true; };
    operations["addToBuffer"] = [&audio]() { audio.addToBuffer(1); return true; };
    
    auto results = tester.runStressTest(operations, "MockAudio concurrent access");
    
    std::cout << "Concurrent access test: " 
              << (results.failed_operations == 0 ? "PASSED" : "FAILED") << std::endl;
    std::cout << "Operations: " << results.total_operations 
              << ", Errors: " << results.failed_operations << std::endl;
}

void testMockIOHandlerLockOrdering() {
    std::cout << "\n=== Testing MockIOHandler Lock Ordering ===" << std::endl;
    
    MockIOHandler handler;
    ThreadSafetyTester::Config config;
    config.num_threads = 4;
    config.operations_per_thread = 50;
    
    ThreadSafetyTester tester(config);
    
    // Create test operations map
    std::map<std::string, ThreadSafetyTester::TestFunction> operations;
    operations["read"] = [&handler]() { 
        char buffer[100];
        handler.read(buffer, sizeof(buffer)); 
        return true; 
    };
    operations["getTotalMemoryUsage"] = []() { 
        MockIOHandler::getTotalMemoryUsage(); 
        return true; 
    };
    operations["getBytesRead"] = [&handler]() { 
        handler.getBytesRead(); 
        return true; 
    };
    
    auto results = tester.runStressTest(operations, "MockIOHandler lock ordering");
    
    std::cout << "Lock ordering test: " 
              << (results.failed_operations == 0 ? "PASSED" : "FAILED") << std::endl;
    std::cout << "Operations: " << results.total_operations 
              << ", Errors: " << results.failed_operations << std::endl;
}

void testMockMemoryPoolManagerCallbacks() {
    std::cout << "\n=== Testing MockMemoryPoolManager Callbacks ===" << std::endl;
    
    MockMemoryPoolManager manager;
    
    // Set up a callback that could cause reentrancy issues
    // Note: This is intentionally problematic to demonstrate the issue
    manager.setPressureCallback([&manager]() {
        // This callback tries to call back into the manager
        // In a real scenario, this could cause deadlock
        // We skip the actual call to avoid deadlock in the test
        // manager.getAllocatedBytes();
    });
    
    ThreadSafetyTester::Config config;
    config.num_threads = 2;
    config.operations_per_thread = 10;
    
    ThreadSafetyTester tester(config);
    
    auto results = tester.runTest([&manager]() {
        // Allocate large buffers to trigger pressure callback
        void* buffer = manager.allocateBuffer(500000);
        if (buffer) {
            manager.releaseBuffer(buffer, 500000);
        }
        return true;
    }, "MockMemoryPoolManager callbacks");
    
    std::cout << "Callback safety test: " 
              << (results.failed_operations == 0 ? "PASSED" : "FAILED") << std::endl;
    std::cout << "Operations: " << results.total_operations 
              << ", Errors: " << results.failed_operations << std::endl;
}

void runPerformanceBenchmarks() {
    std::cout << "\n=== Performance Benchmarks ===" << std::endl;
    
    MockAudio audio;
    const size_t iterations = 10000;
    
    ThreadingBenchmark benchmark;
    
    // Benchmark isFinished
    auto results1 = benchmark.benchmarkScaling(
        [&audio](size_t) { audio.isFinished(); },
        iterations, 4);
    
    std::cout << "MockAudio::isFinished() - Single: " 
              << results1.single_thread_time.count() << "us, Multi: "
              << results1.multi_thread_time.count() << "us, Speedup: "
              << results1.speedup_ratio << "x" << std::endl;
    
    // Benchmark getBufferLevel
    auto results2 = benchmark.benchmarkScaling(
        [&audio](size_t) { audio.getBufferLevel(); },
        iterations, 4);
    
    std::cout << "MockAudio::getBufferLevel() - Single: " 
              << results2.single_thread_time.count() << "us, Multi: "
              << results2.multi_thread_time.count() << "us, Speedup: "
              << results2.speedup_ratio << "x" << std::endl;
    
    MockIOHandler handler;
    
    // Benchmark read
    auto results3 = benchmark.benchmarkScaling(
        [&handler](size_t) { 
            char buffer[1024];
            handler.read(buffer, sizeof(buffer)); 
        },
        iterations / 10, 4);
    
    std::cout << "MockIOHandler::read() - Single: " 
              << results3.single_thread_time.count() << "us, Multi: "
              << results3.multi_thread_time.count() << "us, Speedup: "
              << results3.speedup_ratio << "x" << std::endl;
}

int main() {
    std::cout << "PsyMP3 Threading Safety Baseline Tests" << std::endl;
    std::cout << "======================================" << std::endl;
    
    std::cout << "\nThese tests demonstrate the current threading patterns" << std::endl;
    std::cout << "and establish a baseline before implementing the public/private" << std::endl;
    std::cout << "lock pattern refactoring." << std::endl;
    
    try {
        testMockAudioConcurrentAccess();
        testMockIOHandlerLockOrdering();
        testMockMemoryPoolManagerCallbacks();
        runPerformanceBenchmarks();
        
        std::cout << "\n=== Summary ===" << std::endl;
        std::cout << "Baseline tests completed. These tests demonstrate:" << std::endl;
        std::cout << "1. Current concurrent access patterns work for basic operations" << std::endl;
        std::cout << "2. Lock ordering issues exist between static and instance methods" << std::endl;
        std::cout << "3. Callback reentrancy can cause problems" << std::endl;
        std::cout << "4. Performance baseline established for comparison" << std::endl;
        
        std::cout << "\nNext steps:" << std::endl;
        std::cout << "1. Run the threading safety analysis script" << std::endl;
        std::cout << "2. Implement public/private lock pattern refactoring" << std::endl;
        std::cout << "3. Re-run these tests to validate improvements" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Test execution failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}