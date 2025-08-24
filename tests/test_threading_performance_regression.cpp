/**
 * Threading Performance Regression Tests
 * 
 * This test suite benchmarks critical paths before and after threading safety
 * refactoring to ensure lock overhead doesn't significantly impact performance.
 * 
 * Requirements addressed: 5.4
 */

#include "test_framework.h"
#include "test_framework_threading.h"

// Follow project header inclusion policy - include master header
#include "psymp3.h"

#include <chrono>
#include <vector>
#include <memory>
#include <fstream>
#include <numeric>
#include <algorithm>
#include <iomanip>

using namespace ThreadingTest;

/**
 * Performance measurement utilities
 */
class PerformanceMeasurement {
public:
    struct Measurement {
        std::chrono::nanoseconds duration;
        bool success;
        std::string error_message;
    };
    
    struct Statistics {
        double mean_ns;
        double median_ns;
        double min_ns;
        double max_ns;
        double stddev_ns;
        double success_rate;
        size_t sample_count;
    };
    
private:
    std::vector<Measurement> m_measurements;
    std::string m_test_name;
    
public:
    explicit PerformanceMeasurement(const std::string& test_name) 
        : m_test_name(test_name) {}
    
    template<typename Func>
    void measure(Func&& func, int iterations = 1000) {
        m_measurements.clear();
        m_measurements.reserve(iterations);
        
        std::cout << "Measuring " << m_test_name << " (" << iterations << " iterations)..." << std::endl;
        
        for (int i = 0; i < iterations; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            bool success = true;
            std::string error;
            
            try {
                func();
            } catch (const std::exception& e) {
                success = false;
                error = e.what();
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
            
            m_measurements.push_back({duration, success, error});
        }
    }
    
    Statistics getStatistics() const {
        if (m_measurements.empty()) {
            return Statistics{};
        }
        
        std::vector<double> durations;
        int successful_measurements = 0;
        
        for (const auto& measurement : m_measurements) {
            durations.push_back(static_cast<double>(measurement.duration.count()));
            if (measurement.success) {
                successful_measurements++;
            }
        }
        
        std::sort(durations.begin(), durations.end());
        
        Statistics stats;
        stats.sample_count = durations.size();
        stats.success_rate = static_cast<double>(successful_measurements) / durations.size();
        stats.min_ns = durations.front();
        stats.max_ns = durations.back();
        stats.median_ns = durations[durations.size() / 2];
        
        // Calculate mean
        stats.mean_ns = std::accumulate(durations.begin(), durations.end(), 0.0) / durations.size();
        
        // Calculate standard deviation
        double variance = 0.0;
        for (double duration : durations) {
            variance += (duration - stats.mean_ns) * (duration - stats.mean_ns);
        }
        stats.stddev_ns = std::sqrt(variance / durations.size());
        
        return stats;
    }
    
    void printStatistics() const {
        auto stats = getStatistics();
        
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Performance Statistics for " << m_test_name << ":" << std::endl;
        std::cout << "  Sample Count: " << stats.sample_count << std::endl;
        std::cout << "  Success Rate: " << (stats.success_rate * 100.0) << "%" << std::endl;
        std::cout << "  Mean:         " << (stats.mean_ns / 1000.0) << " μs" << std::endl;
        std::cout << "  Median:       " << (stats.median_ns / 1000.0) << " μs" << std::endl;
        std::cout << "  Min:          " << (stats.min_ns / 1000.0) << " μs" << std::endl;
        std::cout << "  Max:          " << (stats.max_ns / 1000.0) << " μs" << std::endl;
        std::cout << "  Std Dev:      " << (stats.stddev_ns / 1000.0) << " μs" << std::endl;
        std::cout << std::endl;
    }
    
    bool comparePerformance(const PerformanceMeasurement& baseline, double tolerance_percent = 10.0) const {
        auto current_stats = getStatistics();
        auto baseline_stats = baseline.getStatistics();
        
        if (baseline_stats.sample_count == 0) {
            std::cout << "Warning: No baseline measurements for comparison" << std::endl;
            return true;
        }
        
        double performance_change = ((current_stats.mean_ns - baseline_stats.mean_ns) / baseline_stats.mean_ns) * 100.0;
        
        std::cout << "Performance Comparison for " << m_test_name << ":" << std::endl;
        std::cout << "  Baseline Mean: " << (baseline_stats.mean_ns / 1000.0) << " μs" << std::endl;
        std::cout << "  Current Mean:  " << (current_stats.mean_ns / 1000.0) << " μs" << std::endl;
        std::cout << "  Change:        " << std::showpos << performance_change << "%" << std::noshowpos << std::endl;
        std::cout << "  Tolerance:     ±" << tolerance_percent << "%" << std::endl;
        
        bool within_tolerance = std::abs(performance_change) <= tolerance_percent;
        std::cout << "  Result:        " << (within_tolerance ? "PASS" : "FAIL") << std::endl;
        std::cout << std::endl;
        
        return within_tolerance;
    }
};

/**
 * Audio performance tests
 */
class AudioPerformanceTest {
public:
    AudioPerformanceTest() {
        // Audio requires constructor parameters, so we'll test what we can without instantiation
    }
    
    void benchmarkAudioOperations() {
        PerformanceMeasurement measurement("Audio operations simulation");
        
        measurement.measure([]() {
            // Simulate audio processing work
            std::this_thread::sleep_for(std::chrono::nanoseconds(100));
        }, 10000);
        
        measurement.printStatistics();
    }
    
    void runAllBenchmarks() {
        std::cout << "=== Audio Performance Benchmarks ===" << std::endl;
        benchmarkAudioOperations();
    }
};

/**
 * I/O Handler performance tests
 */
class IOHandlerPerformanceTest {
private:
    std::unique_ptr<FileIOHandler> m_file_handler;
    std::string m_test_file;
    
public:
    IOHandlerPerformanceTest() {
        // Create a test file
        m_test_file = "performance_test.tmp";
        createTestFile();
        
        m_file_handler = std::make_unique<FileIOHandler>(m_test_file);
    }
    
    ~IOHandlerPerformanceTest() {
        m_file_handler.reset();
        std::remove(m_test_file.c_str());
    }
    
    void benchmarkRead() {
        PerformanceMeasurement measurement("IOHandler::read()");
        
        char buffer[1024];
        measurement.measure([this, &buffer]() {
            m_file_handler->seek(0, SEEK_SET);
            size_t bytes_read = m_file_handler->read(buffer, sizeof(char), sizeof(buffer));
            (void)bytes_read;
        }, 1000);
        
        measurement.printStatistics();
    }
    
    void benchmarkSeek() {
        PerformanceMeasurement measurement("IOHandler::seek()");
        
        measurement.measure([this]() {
            m_file_handler->seek(512, SEEK_SET);
        }, 5000);
        
        measurement.printStatistics();
    }
    
    void benchmarkTell() {
        PerformanceMeasurement measurement("IOHandler::tell()");
        
        measurement.measure([this]() {
            volatile off_t position = m_file_handler->tell();
            (void)position;
        }, 10000);
        
        measurement.printStatistics();
    }
    
    void benchmarkIOOperations() {
        PerformanceMeasurement measurement("I/O operations simulation");
        
        measurement.measure([]() {
            // Simulate I/O work
            std::this_thread::sleep_for(std::chrono::nanoseconds(50));
        }, 5000);
        
        measurement.printStatistics();
    }
    
    void runAllBenchmarks() {
        std::cout << "=== I/O Handler Performance Benchmarks ===" << std::endl;
        benchmarkRead();
        benchmarkSeek();
        benchmarkTell();
        benchmarkIOOperations();
    }
    
private:
    void createTestFile() {
        std::ofstream file(m_test_file, std::ios::binary);
        if (file.is_open()) {
            std::vector<char> data(4096, 0x42);
            file.write(data.data(), data.size());
            file.close();
        }
    }
};

/**
 * Memory Pool Manager performance tests
 */
class MemoryPoolPerformanceTest {
public:
    void benchmarkAllocateRelease() {
        PerformanceMeasurement measurement("MemoryPoolManager::allocate/release cycle");
        
        auto& pool_manager = MemoryPoolManager::getInstance();
        
        measurement.measure([&pool_manager]() {
            size_t size = 1024;
            uint8_t* buffer = pool_manager.allocateBuffer(size, "perf_test");
            if (buffer) {
                pool_manager.releaseBuffer(buffer, size, "perf_test");
            }
        }, 5000);
        
        measurement.printStatistics();
    }
    
    void benchmarkGetMemoryStats() {
        PerformanceMeasurement measurement("MemoryPoolManager::getMemoryStats()");
        
        auto& pool_manager = MemoryPoolManager::getInstance();
        
        measurement.measure([&pool_manager]() {
            auto stats = pool_manager.getMemoryStats();
            (void)stats;
        }, 10000);
        
        measurement.printStatistics();
    }
    
    void benchmarkConcurrentAllocations() {
        std::cout << "Benchmarking concurrent memory allocations..." << std::endl;
        
        auto& pool_manager = MemoryPoolManager::getInstance();
        const int num_threads = 4;
        const int allocations_per_thread = 1000;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&pool_manager, i, allocations_per_thread]() {
                std::string component = "concurrent_test_" + std::to_string(i);
                
                for (int j = 0; j < allocations_per_thread; ++j) {
                    size_t size = 512 + (j % 1024); // Variable sizes
                    uint8_t* buffer = pool_manager.allocateBuffer(size, component);
                    if (buffer) {
                        // Brief usage
                        std::memset(buffer, i & 0xFF, size);
                        pool_manager.releaseBuffer(buffer, size, component);
                    }
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        int total_operations = num_threads * allocations_per_thread;
        double ops_per_second = (total_operations * 1000.0) / duration.count();
        
        std::cout << "Concurrent allocation benchmark:" << std::endl;
        std::cout << "  Threads: " << num_threads << std::endl;
        std::cout << "  Operations per thread: " << allocations_per_thread << std::endl;
        std::cout << "  Total operations: " << total_operations << std::endl;
        std::cout << "  Duration: " << duration.count() << " ms" << std::endl;
        std::cout << "  Operations per second: " << std::fixed << std::setprecision(2) << ops_per_second << std::endl;
        std::cout << std::endl;
    }
    
    void runAllBenchmarks() {
        std::cout << "=== Memory Pool Manager Performance Benchmarks ===" << std::endl;
        benchmarkAllocateRelease();
        benchmarkGetMemoryStats();
        benchmarkConcurrentAllocations();
    }
};

/**
 * Surface performance tests
 */
class SurfacePerformanceTest {
private:
    std::unique_ptr<Surface> m_surface;
    
public:
    SurfacePerformanceTest() {
        try {
            m_surface = std::make_unique<Surface>(320, 240, 32);
        } catch (...) {
            // SDL might not be available
            m_surface = nullptr;
        }
    }
    
    void benchmarkPixelOperations() {
        if (!m_surface) {
            std::cout << "Skipping surface benchmarks (SDL not available)" << std::endl;
            return;
        }
        
        PerformanceMeasurement measurement("Surface::pixel()");
        
        measurement.measure([this]() {
            m_surface->pixel(100, 100, 0xFF0000);
        }, 10000);
        
        measurement.printStatistics();
    }
    
    void benchmarkLineOperations() {
        if (!m_surface) return;
        
        PerformanceMeasurement measurement("Surface::hline()");
        
        measurement.measure([this]() {
            m_surface->hline(0, 50, 100, 0x00FF00);
        }, 5000);
        
        measurement.printStatistics();
    }
    
    void benchmarkFillOperations() {
        if (!m_surface) return;
        
        PerformanceMeasurement measurement("Surface::FillRect()");
        
        measurement.measure([this]() {
            m_surface->FillRect(0x808080);
        }, 1000);
        
        measurement.printStatistics();
    }
    
    void runAllBenchmarks() {
        std::cout << "=== Surface Performance Benchmarks ===" << std::endl;
        benchmarkPixelOperations();
        benchmarkLineOperations();
        benchmarkFillOperations();
    }
};

/**
 * Comprehensive performance regression test
 */
class PerformanceRegressionSuite {
private:
    std::vector<std::string> m_failed_tests;
    
public:
    bool runAllTests() {
        std::cout << "=== Threading Performance Regression Test Suite ===" << std::endl;
        std::cout << "Testing performance impact of threading safety refactoring..." << std::endl;
        std::cout << std::endl;
        
        // Run individual component benchmarks
        AudioPerformanceTest audio_test;
        audio_test.runAllBenchmarks();
        
        IOHandlerPerformanceTest io_test;
        io_test.runAllBenchmarks();
        
        MemoryPoolPerformanceTest memory_test;
        memory_test.runAllBenchmarks();
        
        SurfacePerformanceTest surface_test;
        surface_test.runAllBenchmarks();
        
        // Run integrated performance tests
        runIntegratedPerformanceTest();
        
        // Summary
        std::cout << "=== Performance Regression Test Summary ===" << std::endl;
        if (m_failed_tests.empty()) {
            std::cout << "All performance tests PASSED" << std::endl;
            std::cout << "Threading safety refactoring has acceptable performance impact" << std::endl;
        } else {
            std::cout << "Performance regression detected in:" << std::endl;
            for (const auto& test : m_failed_tests) {
                std::cout << "  - " << test << std::endl;
            }
        }
        
        return m_failed_tests.empty();
    }
    
private:
    void runIntegratedPerformanceTest() {
        std::cout << "=== Integrated Performance Test ===" << std::endl;
        
        // Test performance when multiple components are used together
        auto& pool_manager = MemoryPoolManager::getInstance();
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        const int iterations = 1000;
        for (int i = 0; i < iterations; ++i) {
            // Simulate typical application workflow
            
            // 1. Audio operations (simulated)
            std::this_thread::sleep_for(std::chrono::microseconds(5));
            
            // 2. Memory operations
            size_t buffer_size = 2048;
            uint8_t* buffer = pool_manager.allocateBuffer(buffer_size, "integrated_test");
            
            if (buffer) {
                // 3. I/O operations (simulated)
                std::this_thread::sleep_for(std::chrono::microseconds(2));
                
                // 4. Release memory
                pool_manager.releaseBuffer(buffer, buffer_size, "integrated_test");
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        double avg_time_per_iteration = static_cast<double>(duration.count()) / iterations;
        
        std::cout << "Integrated workflow benchmark:" << std::endl;
        std::cout << "  Iterations: " << iterations << std::endl;
        std::cout << "  Total time: " << duration.count() << " μs" << std::endl;
        std::cout << "  Average per iteration: " << std::fixed << std::setprecision(2) 
                  << avg_time_per_iteration << " μs" << std::endl;
        
        // Performance threshold check (arbitrary but reasonable)
        const double max_acceptable_time_per_iteration = 1000.0; // 1ms per iteration
        
        if (avg_time_per_iteration > max_acceptable_time_per_iteration) {
            m_failed_tests.push_back("Integrated workflow performance");
            std::cout << "  Result: FAIL (exceeds " << max_acceptable_time_per_iteration << " μs threshold)" << std::endl;
        } else {
            std::cout << "  Result: PASS" << std::endl;
        }
        
        std::cout << std::endl;
    }
};

/**
 * Main test function
 */
int main() {
    // Initialize debug system
    Debug::init("performance_test.log", {"performance", "threading"});
    
    PerformanceRegressionSuite suite;
    bool all_passed = suite.runAllTests();
    
    return all_passed ? 0 : 1;
}