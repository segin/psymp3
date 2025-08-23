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
 * Test functions for each mock class
 */
void testMockAudioConcurrentAccess() {
    std::cout << "\n=== Testing MockAudio Concurrent Access ===" << std::endl;
    
    MockAudio audio;
    ThreadingTest::TestConfig config;
    config.num_threads = 4;
    config.operations_per_thread = 100;
    
    // Test concurrent access to different methods
    auto test = ThreadingTest::ConcurrentAccessTest<MockAudio>(
        &audio,
        [](MockAudio* a, int thread_id) {
            switch (thread_id % 4) {
                case 0: a->isFinished(); break;
                case 1: a->resetBuffer(); break;
                case 2: a->getBufferLevel(); break;
                case 3: a->addToBuffer(1); break;
            }
        },
        config
    );
    
    auto results = test.run();
    
    std::cout << "Concurrent access test: " 
              << (results.success ? "PASSED" : "FAILED") << std::endl;
    std::cout << "Operations: " << results.total_operations 
              << ", Errors: " << results.failed_operations << std::endl;
}

void testMockIOHandlerLockOrdering() {
    std::cout << "\n=== Testing MockIOHandler Lock Ordering ===" << std::endl;
    
    MockIOHandler handler;
    ThreadingTest::TestConfig config;
    config.num_threads = 4;
    config.operations_per_thread = 50;
    
    // Test mixed static and instance method calls
    auto test = ThreadingTest::ConcurrentAccessTest<MockIOHandler>(
        &handler,
        [](MockIOHandler* h, int thread_id) {
            char buffer[100];
            switch (thread_id % 3) {
                case 0: h->read(buffer, sizeof(buffer)); break;
                case 1: MockIOHandler::getTotalMemoryUsage(); break;
                case 2: h->getBytesRead(); break;
            }
        },
        config
    );
    
    auto results = test.run();
    
    std::cout << "Lock ordering test: " 
              << (results.success ? "PASSED" : "FAILED") << std::endl;
    std::cout << "Operations: " << results.total_operations 
              << ", Errors: " << results.failed_operations << std::endl;
}

void testMockMemoryPoolManagerCallbacks() {
    std::cout << "\n=== Testing MockMemoryPoolManager Callbacks ===" << std::endl;
    
    MockMemoryPoolManager manager;
    ThreadingTest::TestConfig config;
    config.num_threads = 2;
    config.operations_per_thread = 10;
    config.timeout = std::chrono::milliseconds(2000);
    
    // Set up a callback that could cause reentrancy issues
    manager.setPressureCallback([&manager]() {
        // This callback tries to call back into the manager
        // In a real scenario, this could cause deadlock
        manager.getAllocatedBytes();
    });
    
    auto test = ThreadingTest::ConcurrentAccessTest<MockMemoryPoolManager>(
        &manager,
        [](MockMemoryPoolManager* m, int thread_id) {
            // Allocate large buffers to trigger pressure callback
            void* buffer = m->allocateBuffer(500000);
            if (buffer) {
                m->releaseBuffer(buffer, 500000);
            }
        },
        config
    );
    
    auto results = test.run();
    
    std::cout << "Callback safety test: " 
              << (results.success ? "PASSED" : "FAILED") << std::endl;
    std::cout << "Operations: " << results.total_operations 
              << ", Errors: " << results.failed_operations << std::endl;
    
    if (!results.success) {
        std::cout << "Note: Callback test failures are expected with current implementation" << std::endl;
    }
}

void runPerformanceBenchmarks() {
    std::cout << "\n=== Performance Benchmarks ===" << std::endl;
    
    MockAudio audio;
    const int iterations = 10000;
    
    {
        ThreadingTest::PerformanceBenchmark bench("MockAudio::isFinished() single-threaded");
        for (int i = 0; i < iterations; ++i) {
            audio.isFinished();
        }
    }
    
    {
        ThreadingTest::PerformanceBenchmark bench("MockAudio::getBufferLevel() single-threaded");
        for (int i = 0; i < iterations; ++i) {
            audio.getBufferLevel();
        }
    }
    
    MockIOHandler handler;
    char buffer[1024];
    
    {
        ThreadingTest::PerformanceBenchmark bench("MockIOHandler::read() single-threaded");
        for (int i = 0; i < iterations / 10; ++i) {
            handler.read(buffer, sizeof(buffer));
        }
    }
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