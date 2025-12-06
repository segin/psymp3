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
#include <cstring>

using namespace TestFramework::Threading;

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
void testMemoryManagementStress() {
    std::cout << "\n=== Memory Management Stress Test ===" << std::endl;
    
    auto& pool_manager = PsyMP3::IO::MemoryPoolManager::getInstance();
    
    std::vector<std::pair<uint8_t*, size_t>> allocated_buffers;
    std::mutex buffers_mutex;
    std::atomic<int> error_count{0};
    
    ThreadSafetyTester::Config config;
    config.num_threads = 8;
    config.operations_per_thread = 100;
    
    ThreadSafetyTester tester(config);
    
    std::map<std::string, ThreadSafetyTester::TestFunction> operations;
    
    operations["allocate"] = [&]() {
        std::random_device rd;
        std::mt19937 rng(rd());
        std::uniform_int_distribution<size_t> size_dist(64, 4096);
        
        size_t size = size_dist(rng);
        uint8_t* buffer = pool_manager.allocateBuffer(size, "stress_test");
        
        if (buffer) {
            std::memset(buffer, 0x42, size);
            std::lock_guard<std::mutex> lock(buffers_mutex);
            allocated_buffers.emplace_back(buffer, size);
        }
        return true;
    };
    
    operations["release"] = [&]() {
        uint8_t* buffer = nullptr;
        size_t size = 0;
        
        {
            std::lock_guard<std::mutex> lock(buffers_mutex);
            if (!allocated_buffers.empty()) {
                auto& back = allocated_buffers.back();
                buffer = back.first;
                size = back.second;
                allocated_buffers.pop_back();
            }
        }
        
        if (buffer) {
            pool_manager.releaseBuffer(buffer, size, "stress_test");
        }
        return true;
    };
    
    auto results = tester.runStressTest(operations, "Memory management stress");
    
    // Cleanup remaining buffers
    for (auto& [buffer, size] : allocated_buffers) {
        pool_manager.releaseBuffer(buffer, size, "cleanup");
    }
    allocated_buffers.clear();
    
    std::cout << "Memory stress test: " 
              << (results.failed_operations == 0 ? "PASSED" : "FAILED") << std::endl;
    std::cout << "Operations: " << results.total_operations 
              << ", Errors: " << results.failed_operations << std::endl;
}

/**
 * Test scenario: Surface operations with concurrent memory management
 */
void testSurfaceMemoryIntegration() {
    std::cout << "\n=== Surface + Memory Integration Test ===" << std::endl;
    
    // Try to create a test surface
    std::unique_ptr<Surface> surface;
    try {
        surface = std::make_unique<Surface>(320, 240, 32);
    } catch (...) {
        std::cout << "SKIPPED: Could not create Surface (SDL not available)" << std::endl;
        return;
    }
    
    auto& pool_manager = PsyMP3::IO::MemoryPoolManager::getInstance();
    
    ThreadSafetyTester::Config config;
    config.num_threads = 4;
    config.operations_per_thread = 50;
    
    ThreadSafetyTester tester(config);
    
    std::map<std::string, ThreadSafetyTester::TestFunction> operations;
    
    operations["surface_ops"] = [&surface]() {
        if (surface) {
            surface->pixel(10, 10, 0xFF0000);
            surface->hline(0, 50, 100, 0x00FF00);
            surface->vline(50, 0, 100, 0x0000FF);
            surface->FillRect(0x808080);
        }
        return true;
    };
    
    operations["memory_ops"] = [&pool_manager]() {
        size_t buffer_size = 2048;
        uint8_t* buffer = pool_manager.allocateBuffer(buffer_size, "surface_test");
        
        if (buffer) {
            std::memset(buffer, 0xAA, buffer_size);
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            pool_manager.releaseBuffer(buffer, buffer_size, "surface_test");
        }
        return true;
    };
    
    auto results = tester.runStressTest(operations, "Surface + Memory integration");
    
    std::cout << "Surface + Memory test: " 
              << (results.failed_operations == 0 ? "PASSED" : "FAILED") << std::endl;
    std::cout << "Operations: " << results.total_operations 
              << ", Errors: " << results.failed_operations << std::endl;
}

/**
 * Long-running stress test to detect race conditions and deadlocks
 */
void testLongRunningStress() {
    std::cout << "\n=== Long-Running Stress Test ===" << std::endl;
    
    auto& pool_manager = PsyMP3::IO::MemoryPoolManager::getInstance();
    
    std::atomic<bool> test_running{true};
    std::atomic<int> error_count{0};
    std::atomic<size_t> total_operations{0};
    
    const int num_threads = 8;
    const int duration_seconds = 5; // Short for CI
    
    std::cout << "Running stress test for " << duration_seconds << " seconds..." << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    auto end_time = start_time + std::chrono::seconds(duration_seconds);
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            std::random_device rd;
            std::mt19937 rng(rd() + i);
            std::uniform_int_distribution<int> op_dist(0, 2);
            
            while (test_running.load() && 
                   std::chrono::steady_clock::now() < end_time && 
                   error_count.load() == 0) {
                
                try {
                    int op = op_dist(rng);
                    
                    if (op == 0 || op == 1) {
                        // Memory operation
                        size_t size = 512;
                        uint8_t* buffer = pool_manager.allocateBuffer(size, "stress_worker");
                        if (buffer) {
                            std::memset(buffer, 0x33, size);
                            pool_manager.releaseBuffer(buffer, size, "stress_worker");
                        }
                    } else {
                        // Simulate I/O work
                        std::this_thread::sleep_for(std::chrono::microseconds(10));
                    }
                    
                    total_operations++;
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                    
                } catch (const std::exception& e) {
                    error_count++;
                    std::cerr << "Worker " << i << " error: " << e.what() << std::endl;
                    break;
                }
            }
        });
    }
    
    // Monitor progress
    while (std::chrono::steady_clock::now() < end_time && error_count.load() == 0) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time);
        std::cout << "Progress: " << elapsed.count() << "/" << duration_seconds 
                  << " seconds, ops: " << total_operations.load() << std::endl;
    }
    
    test_running = false;
    
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    
    std::cout << "Long-running stress test: " 
              << (error_count.load() == 0 ? "PASSED" : "FAILED") << std::endl;
    std::cout << "Total operations: " << total_operations.load() 
              << ", Errors: " << error_count.load() << std::endl;
}

/**
 * Main test function
 */
int main() {
    std::cout << "=== System-Wide Threading Safety Integration Tests ===" << std::endl;
    
    int failures = 0;
    
    try {
        testMemoryManagementStress();
        testSurfaceMemoryIntegration();
        testLongRunningStress();
        
        std::cout << "\n=== Summary ===" << std::endl;
        std::cout << "System-wide threading integration tests completed." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Test execution failed: " << e.what() << std::endl;
        return 1;
    }
    
    return failures;
}
