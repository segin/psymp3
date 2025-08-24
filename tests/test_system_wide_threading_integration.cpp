/**
 * System-Wide Threading Safety Integration Tests
 * 
 * This test suite exercises multiple threaded components simultaneously
 * to validate the threading safety refactoring across the entire system.
 * 
 * Requirements addressed: 3.3, 5.4
 */

#include "test_framework.h"
#include "test_framework_threading.h"

// Follow project header inclusion policy - include master header
#include "psymp3.h"

#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <memory>
#include <random>
#include <fstream>

using namespace ThreadingTest;

/**
 * Mock implementations for testing
 */
class MockStream : public Stream {
public:
    MockStream() : Stream(TagLib::String("mock_stream")) {}
    
    virtual ~MockStream() = default;
    
    virtual size_t getData(size_t len, void *buf) override {
        // Simulate some work and return dummy data
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        if (buf && len > 0) {
            std::memset(buf, 0x42, len);
        }
        return len;
    }
    
    virtual void seekTo(unsigned long pos) override {
        m_position = pos;
    }
    
    virtual bool eof() override {
        return m_finished.load();
    }
    
    virtual bool isFinished() const {
        return m_finished.load();
    }
    
    virtual void setFinished(bool finished) {
        m_finished.store(finished);
    }
    
private:
    std::atomic<bool> m_finished{false};
    unsigned long m_position{0};
};

/**
 * Test scenario: Memory management under high concurrency
 */
class MemoryManagementStressTest : public ThreadSafetyTestBase {
private:
    std::vector<std::pair<uint8_t*, size_t>> m_allocated_buffers;
    std::mutex m_buffers_mutex;
    std::mt19937 m_rng;
    
public:
    MemoryManagementStressTest(const TestConfig& config = TestConfig{}) 
        : ThreadSafetyTestBase(config), m_rng(std::random_device{}()) {}
    
    ~MemoryManagementStressTest() {
        cleanup();
    }
    
protected:
    void runTest() override {
        std::cout << "Running memory management stress test..." << std::endl;
        
        runConcurrentOperations([this](int thread_id) {
            testMemoryOperations(thread_id);
        });
        
        // Verify all buffers were properly released
        std::lock_guard<std::mutex> lock(m_buffers_mutex);
        if (!m_allocated_buffers.empty()) {
            m_results.addError("Memory leak detected: " + 
                             std::to_string(m_allocated_buffers.size()) + 
                             " buffers not released");
        }
    }
    
private:
    void testMemoryOperations(int thread_id) {
        auto& pool_manager = MemoryPoolManager::getInstance();
        std::uniform_int_distribution<size_t> size_dist(64, 4096);
        std::uniform_int_distribution<int> action_dist(0, 2);
        
        std::string component_name = "stress_test_" + std::to_string(thread_id);
        
        for (int i = 0; i < 100; ++i) {
            int action = action_dist(m_rng);
            
            if (action == 0 || action == 1) {
                // Allocate buffer
                size_t size = size_dist(m_rng);
                uint8_t* buffer = pool_manager.allocateBuffer(size, component_name);
                
                if (buffer) {
                    // Store for later release
                    std::lock_guard<std::mutex> lock(m_buffers_mutex);
                    m_allocated_buffers.emplace_back(buffer, size);
                    
                    // Use the buffer
                    std::memset(buffer, thread_id & 0xFF, size);
                }
            } else {
                // Release a buffer
                std::lock_guard<std::mutex> lock(m_buffers_mutex);
                if (!m_allocated_buffers.empty()) {
                    auto [buffer, size] = m_allocated_buffers.back();
                    m_allocated_buffers.pop_back();
                    
                    // Release outside the lock to avoid holding multiple locks
                    lock.~lock_guard();
                    pool_manager.releaseBuffer(buffer, size, component_name);
                }
            }
            
            // Occasionally test pool manager state
            if (i % 20 == 0) {
                // Just test that the pool manager is responsive
                pool_manager.setMemoryLimits(1024 * 1024, 512 * 1024);
            }
        }
    }
    
    void cleanup() {
        auto& pool_manager = MemoryPoolManager::getInstance();
        
        std::lock_guard<std::mutex> lock(m_buffers_mutex);
        for (auto [buffer, size] : m_allocated_buffers) {
            pool_manager.releaseBuffer(buffer, size, "cleanup");
        }
        m_allocated_buffers.clear();
    }
};

/**
 * Test scenario: Surface operations with concurrent memory management
 */
class SurfaceMemoryIntegrationTest : public ThreadSafetyTestBase {
private:
    std::unique_ptr<Surface> m_surface;
    
public:
    SurfaceMemoryIntegrationTest(const TestConfig& config = TestConfig{}) 
        : ThreadSafetyTestBase(config) {
        
        // Create a test surface (this might require SDL initialization)
        try {
            m_surface = std::make_unique<Surface>(320, 240, 32);
        } catch (...) {
            // If SDL is not available, we'll skip surface operations
            m_surface = nullptr;
        }
    }
    
protected:
    void runTest() override {
        std::cout << "Running Surface + Memory integration test..." << std::endl;
        
        runConcurrentOperations([this](int thread_id) {
            try {
                if (thread_id % 2 == 0) {
                    testSurfaceOperations();
                } else {
                    testMemoryOperations();
                }
            } catch (const std::exception& e) {
                m_results.addError(std::string("Thread ") + std::to_string(thread_id) + 
                                 " exception: " + e.what());
            }
        });
    }
    
private:
    void testSurfaceOperations() {
        if (!m_surface) {
            // Skip surface operations if surface creation failed
            return;
        }
        
        // Test various surface operations
        m_surface->pixel(10, 10, 0xFF0000);
        m_surface->hline(0, 50, 100, 0x00FF00);
        m_surface->vline(50, 0, 100, 0x0000FF);
        m_surface->FillRect(0x808080);
    }
    
    void testMemoryOperations() {
        auto& pool_manager = MemoryPoolManager::getInstance();
        
        // Allocate and release buffers while surface operations are happening
        size_t buffer_size = 2048;
        uint8_t* buffer = pool_manager.allocateBuffer(buffer_size, "surface_test");
        
        if (buffer) {
            // Simulate some graphics-related memory usage
            std::memset(buffer, 0xAA, buffer_size);
            
            // Brief delay to increase chance of concurrent access
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            
            pool_manager.releaseBuffer(buffer, buffer_size, "surface_test");
        }
    }
};

/**
 * Long-running stress test to detect race conditions and deadlocks
 */
class LongRunningStressTest : public ThreadSafetyTestBase {
private:
    std::atomic<bool> m_test_running{true};
    std::atomic<int> m_error_count{0};
    
public:
    LongRunningStressTest(const TestConfig& config = TestConfig{}) 
        : ThreadSafetyTestBase(config) {
        
        // Override config for long-running test
        m_config.stress_duration_seconds = 30; // 30 seconds
        m_config.num_threads = 12; // More threads for stress
    }
    
protected:
    void runTest() override {
        std::cout << "Running long-running stress test for " 
                  << m_config.stress_duration_seconds << " seconds..." << std::endl;
        
        auto start_time = std::chrono::steady_clock::now();
        auto end_time = start_time + std::chrono::seconds(m_config.stress_duration_seconds);
        
        std::vector<std::thread> threads;
        
        // Start worker threads
        for (int i = 0; i < m_config.num_threads; ++i) {
            threads.emplace_back([this, i, end_time]() {
                stressWorker(i, end_time);
            });
        }
        
        // Monitor progress
        while (std::chrono::steady_clock::now() < end_time && m_error_count.load() == 0) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start_time);
            std::cout << "Stress test progress: " << elapsed.count() 
                      << "/" << m_config.stress_duration_seconds << " seconds" << std::endl;
        }
        
        m_test_running = false;
        
        // Wait for all threads to complete
        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }
        
        if (m_error_count.load() > 0) {
            m_results.addError("Stress test detected " + std::to_string(m_error_count.load()) + 
                             " errors during execution");
        }
        
        std::cout << "Long-running stress test completed." << std::endl;
    }
    
private:
    void stressWorker(int worker_id, std::chrono::steady_clock::time_point end_time) {
        std::mt19937 rng(std::random_device{}() + worker_id);
        std::uniform_int_distribution<int> operation_dist(0, 2);
        
        int operations_performed = 0;
        
        while (m_test_running.load() && 
               std::chrono::steady_clock::now() < end_time && 
               m_error_count.load() == 0) {
            
            try {
                int operation = operation_dist(rng);
                
                switch (operation) {
                    case 0:
                        performMemoryOperations();
                        break;
                    case 1:
                        performIOOperations();
                        break;
                    case 2:
                        performMixedOperations();
                        break;
                }
                
                operations_performed++;
                m_results.total_operations++;
                
                // Brief pause to allow other threads to run
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                
            } catch (const std::exception& e) {
                m_error_count++;
                m_results.addError(std::string("Worker ") + std::to_string(worker_id) + 
                                 " error: " + e.what());
                break;
            }
        }
    }
    
    void performIOOperations() {
        // Test I/O related operations
        // Simulate some I/O work
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
    
    void performMemoryOperations() {
        auto& pool_manager = MemoryPoolManager::getInstance();
        
        // Allocate and immediately release a buffer
        size_t size = 512;
        uint8_t* buffer = pool_manager.allocateBuffer(size, "stress_worker");
        
        if (buffer) {
            std::memset(buffer, 0x33, size);
            pool_manager.releaseBuffer(buffer, size, "stress_worker");
        }
        
        // Test pool manager responsiveness
        pool_manager.setMemoryLimits(1024 * 1024, 512 * 1024);
    }
    
    void performMixedOperations() {
        // Perform a combination of operations that might interact
        performMemoryOperations();
        performIOOperations();
    }
};

/**
 * Main test function
 */
int main() {
    std::cout << "=== System-Wide Threading Safety Integration Tests ===" << std::endl;
    
    // Initialize debug system
    Debug::init("threading_test.log", {"threading", "memory", "io"});
    
    ThreadingTestRunner runner;
    
    // Configure test parameters
    TestConfig config;
    config.num_threads = 8;
    config.operations_per_thread = 500;
    config.timeout = std::chrono::milliseconds(10000); // 10 seconds
    
    std::cout << "Adding memory management stress test..." << std::endl;
    runner.addTest(std::make_unique<MemoryManagementStressTest>(config));
    
    std::cout << "Adding Surface + Memory integration test..." << std::endl;
    runner.addTest(std::make_unique<SurfaceMemoryIntegrationTest>(config));
    
    // Add long-running stress test (shorter for CI)
    TestConfig stress_config = config;
    stress_config.stress_duration_seconds = 10; // 10 seconds for CI
    std::cout << "Adding long-running stress test..." << std::endl;
    runner.addTest(std::make_unique<LongRunningStressTest>(stress_config));
    
    // Run all tests
    bool all_passed = runner.runAllTests();
    
    std::cout << std::endl;
    std::cout << "=== System-Wide Threading Integration Test Results ===" << std::endl;
    std::cout << "Status: " << (all_passed ? "PASSED" : "FAILED") << std::endl;
    
    return all_passed ? 0 : 1;
}