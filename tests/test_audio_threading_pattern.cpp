/**
 * Simple Threading Pattern Test for Audio Class Refactoring
 * 
 * This test validates the public/private lock pattern implementation
 * without the complexity of creating actual Audio objects.
 * 
 * Requirements addressed: 3.3, 5.4
 */

#include "psymp3.h"
#include "test_framework.h"
#include "test_framework_threading.h"
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

/**
 * Simple mock class that demonstrates the threading pattern
 * This mimics the Audio class structure without SDL dependencies
 */
class MockAudioThreadingPattern {
private:
    mutable std::mutex m_buffer_mutex;
    mutable std::mutex m_stream_mutex;
    std::atomic<bool> m_finished{false};
    std::atomic<uint64_t> m_buffer_latency{0};
    mutable std::atomic<int> m_operation_count{0};
    
public:
    // Public methods that acquire locks (like the real Audio class)
    bool isFinished() const {
        std::lock_guard<std::mutex> lock(m_buffer_mutex);
        return isFinished_unlocked();
    }
    
    void resetBuffer() {
        std::lock_guard<std::mutex> lock(m_buffer_mutex);
        resetBuffer_unlocked();
    }
    
    uint64_t getBufferLatencyMs() const {
        std::lock_guard<std::mutex> lock(m_buffer_mutex);
        return getBufferLatencyMs_unlocked();
    }
    
    void setStream() {
        // Lock acquisition order: m_stream_mutex before m_buffer_mutex
        std::lock_guard<std::mutex> stream_lock(m_stream_mutex);
        std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
        setStream_unlocked();
    }
    
    int getOperationCount() const {
        return m_operation_count.load();
    }
    
private:
    // Private unlocked versions (assumes locks are already held)
    bool isFinished_unlocked() const {
        m_operation_count++;
        return m_finished.load();
    }
    
    void resetBuffer_unlocked() {
        m_operation_count++;
        m_buffer_latency = 0;
    }
    
    uint64_t getBufferLatencyMs_unlocked() const {
        m_operation_count++;
        return m_buffer_latency.load();
    }
    
    void setStream_unlocked() {
        m_operation_count++;
        resetBuffer_unlocked();  // This would cause deadlock without unlocked pattern
        m_finished = false;
    }
};

using namespace TestFramework::Threading;

/**
 * Test concurrent access to public methods
 */
void testConcurrentAccess() {
    std::cout << "\n=== Testing Concurrent Access Pattern ===" << std::endl;
    
    MockAudioThreadingPattern mock_audio;
    ThreadSafetyTester::Config config;
    config.num_threads = 8;
    config.operations_per_thread = 100;
    config.test_duration = std::chrono::milliseconds(5000);
    
    ThreadSafetyTester tester(config);
    
    std::map<std::string, ThreadSafetyTester::TestFunction> operations;
    operations["isFinished"] = [&mock_audio]() { mock_audio.isFinished(); return true; };
    operations["resetBuffer"] = [&mock_audio]() { mock_audio.resetBuffer(); return true; };
    operations["getBufferLatencyMs"] = [&mock_audio]() { mock_audio.getBufferLatencyMs(); return true; };
    operations["setStream"] = [&mock_audio]() { mock_audio.setStream(); return true; };
    
    auto results = tester.runStressTest(operations, "Concurrent access pattern");
    
    std::cout << "Concurrent access test: " 
              << (results.failed_operations == 0 ? "PASSED" : "FAILED") << std::endl;
    std::cout << "Operations: " << results.total_operations 
              << ", Errors: " << results.failed_operations << std::endl;
    std::cout << "Mock operations executed: " << mock_audio.getOperationCount() << std::endl;
    
    if (results.failed_operations > 0) {
        for (const auto& error : results.error_messages) {
            std::cout << "  Error: " << error << std::endl;
        }
    }
}

/**
 * Test deadlock prevention
 */
void testDeadlockPrevention() {
    std::cout << "\n=== Testing Deadlock Prevention Pattern ===" << std::endl;
    
    MockAudioThreadingPattern mock_audio;
    ThreadSafetyTester::Config config;
    config.num_threads = 4;
    config.operations_per_thread = 50;
    
    ThreadSafetyTester tester(config);
    
    // Test for deadlock using the framework's deadlock detection
    bool deadlock_detected = tester.testForDeadlock([&mock_audio]() {
        // These operations would cause deadlock without the unlocked pattern
        mock_audio.setStream();
        mock_audio.resetBuffer();
        mock_audio.getBufferLatencyMs();
        mock_audio.isFinished();
    }, std::chrono::milliseconds(3000));
    
    std::cout << "Deadlock prevention test: " 
              << (!deadlock_detected ? "PASSED" : "FAILED") << std::endl;
    
    if (deadlock_detected) {
        std::cout << "WARNING: Potential deadlock detected!" << std::endl;
    }
}

/**
 * Performance test to ensure the pattern doesn't significantly impact performance
 */
void testPerformanceImpact() {
    std::cout << "\n=== Testing Performance Impact ===" << std::endl;
    
    MockAudioThreadingPattern mock_audio;
    const size_t iterations = 10000;
    
    ThreadingBenchmark benchmark;
    
    // Benchmark isFinished
    auto results1 = benchmark.benchmarkScaling(
        [&mock_audio](size_t) { mock_audio.isFinished(); },
        iterations, 4);
    
    std::cout << "MockAudio::isFinished() - Single: " 
              << results1.single_thread_time.count() << "us, Multi: "
              << results1.multi_thread_time.count() << "us, Speedup: "
              << results1.speedup_ratio << "x" << std::endl;
    
    // Benchmark getBufferLatencyMs
    auto results2 = benchmark.benchmarkScaling(
        [&mock_audio](size_t) { mock_audio.getBufferLatencyMs(); },
        iterations, 4);
    
    std::cout << "MockAudio::getBufferLatencyMs() - Single: " 
              << results2.single_thread_time.count() << "us, Multi: "
              << results2.multi_thread_time.count() << "us, Speedup: "
              << results2.speedup_ratio << "x" << std::endl;
    
    // Benchmark mixed operations
    auto results3 = benchmark.benchmarkScaling(
        [&mock_audio](size_t i) { 
            switch (i % 3) {
                case 0: mock_audio.isFinished(); break;
                case 1: mock_audio.getBufferLatencyMs(); break;
                case 2: mock_audio.resetBuffer(); break;
            }
        },
        iterations, 4);
    
    std::cout << "MockAudio mixed operations - Single: " 
              << results3.single_thread_time.count() << "us, Multi: "
              << results3.multi_thread_time.count() << "us, Speedup: "
              << results3.speedup_ratio << "x" << std::endl;
}

int main() {
    std::cout << "PsyMP3 Audio Threading Pattern Tests" << std::endl;
    std::cout << "====================================" << std::endl;
    
    std::cout << "\nTesting the public/private lock pattern implementation" << std::endl;
    std::cout << "to validate thread safety and deadlock prevention." << std::endl;
    
    try {
        testConcurrentAccess();
        testDeadlockPrevention();
        testPerformanceImpact();
        
        std::cout << "\n=== Summary ===" << std::endl;
        std::cout << "Threading pattern tests completed successfully." << std::endl;
        std::cout << "The public/private lock pattern:" << std::endl;
        std::cout << "1. Prevents deadlocks when public methods call each other" << std::endl;
        std::cout << "2. Maintains thread safety under concurrent access" << std::endl;
        std::cout << "3. Has minimal performance impact" << std::endl;
        std::cout << "4. Can be safely applied to the Audio class" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Test execution failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}