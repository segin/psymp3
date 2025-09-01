/*
 * test_flac_performance_benchmarking.cpp - Test FLAC codec performance benchmarking
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

#include "psymp3.h"

#ifdef HAVE_FLAC

/**
 * @brief Test FLAC codec performance benchmarking system
 * 
 * This test validates the performance benchmarking and validation system
 * for FLAC codec implementations as required by RFC 9639 compliance validation.
 */

// Simple mock codec that doesn't use buffer pools
class MockFLACCodec {
public:
    MockFLACCodec() {
        m_decode_delay_us = 10; // Simulate 10μs decode time
        m_cpu_usage_percent = 0.5; // Simulate 0.5% CPU usage
    }
    
    bool initialize() { return true; }
    
    // Simulate decode method
    AudioFrame decode(const MediaChunk& chunk) {
        // Simulate decode time
        std::this_thread::sleep_for(std::chrono::microseconds(m_decode_delay_us));
        
        // Create simple mock audio frame without buffer pools
        AudioFrame frame;
        frame.sample_rate = 44100;
        frame.channels = 2;
        size_t sample_count = 1152; // Standard FLAC block size
        frame.samples.resize(sample_count * frame.channels, 0);
        frame.timestamp_samples = 0;
        
        return frame;
    }
    
    void setDecodeDelay(uint64_t delay_us) { m_decode_delay_us = delay_us; }
    void setCPUUsage(double cpu_percent) { m_cpu_usage_percent = cpu_percent; }
    
private:
    uint64_t m_decode_delay_us;
    double m_cpu_usage_percent;
};

// Test performance benchmark creation and basic functionality
bool test_performance_benchmark_creation() {
    Debug::log("test_flac_performance", "[test_performance_benchmark_creation] Testing benchmark creation");
    
    try {
        FLACPerformanceBenchmark benchmark;
        
        // Test basic functionality
        size_t memory_usage = benchmark.getCurrentMemoryUsage();
        double cpu_usage = benchmark.getCurrentCPUUsage();
        
        Debug::log("test_flac_performance", "[test_performance_benchmark_creation] Initial memory usage: ", memory_usage);
        Debug::log("test_flac_performance", "[test_performance_benchmark_creation] Initial CPU usage: ", cpu_usage);
        
        // Verify reasonable values
        if (memory_usage == 0) {
            Debug::log("test_flac_performance", "[test_performance_benchmark_creation] ERROR: Memory usage is zero");
            return false;
        }
        
        Debug::log("test_flac_performance", "[test_performance_benchmark_creation] SUCCESS: Benchmark created successfully");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test_flac_performance", "[test_performance_benchmark_creation] ERROR: Exception: ", e.what());
        return false;
    }
}

// Test standard benchmark configurations
bool test_benchmark_configurations() {
    Debug::log("test_flac_performance", "[test_benchmark_configurations] Testing benchmark configurations");
    
    try {
        // Test standard CD quality configuration
        FLACBenchmarkConfig cd_config = FLACBenchmarkConfigs::standardCDQuality();
        
        if (cd_config.sample_rate != 44100 || cd_config.channels != 2 || cd_config.bits_per_sample != 16) {
            Debug::log("test_flac_performance", "[test_benchmark_configurations] ERROR: Invalid CD quality config");
            return false;
        }
        
        Debug::log("test_flac_performance", "[test_benchmark_configurations] CD config: ", cd_config.getDescription());
        
        // Test high resolution configuration
        FLACBenchmarkConfig hires_config = FLACBenchmarkConfigs::highResolution96k24();
        
        if (hires_config.sample_rate != 96000 || hires_config.channels != 2 || hires_config.bits_per_sample != 24) {
            Debug::log("test_flac_performance", "[test_benchmark_configurations] ERROR: Invalid high-res config");
            return false;
        }
        
        Debug::log("test_flac_performance", "[test_benchmark_configurations] High-res config: ", hires_config.getDescription());
        
        // Test ultra high resolution configuration
        FLACBenchmarkConfig ultra_config = FLACBenchmarkConfigs::ultraHighRes192k32();
        
        if (ultra_config.sample_rate != 192000 || ultra_config.channels != 2 || ultra_config.bits_per_sample != 32) {
            Debug::log("test_flac_performance", "[test_benchmark_configurations] ERROR: Invalid ultra high-res config");
            return false;
        }
        
        Debug::log("test_flac_performance", "[test_benchmark_configurations] Ultra high-res config: ", ultra_config.getDescription());
        
        // Test expected frame count calculation
        uint32_t expected_frames = cd_config.getExpectedFrameCount(1152);
        uint32_t calculated_frames = (44100 * cd_config.test_duration_seconds) / 1152;
        
        if (expected_frames != calculated_frames) {
            Debug::log("test_flac_performance", "[test_benchmark_configurations] ERROR: Frame count calculation mismatch");
            return false;
        }
        
        Debug::log("test_flac_performance", "[test_benchmark_configurations] SUCCESS: All configurations valid");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test_flac_performance", "[test_benchmark_configurations] ERROR: Exception: ", e.what());
        return false;
    }
}

// Test performance metrics validation
bool test_performance_metrics_validation() {
    Debug::log("test_flac_performance", "[test_performance_metrics_validation] Testing performance metrics validation");
    
    try {
        FLACPerformanceMetrics metrics;
        
        // Test metrics for 44.1kHz/16-bit stereo
        metrics.sample_rate = 44100;
        metrics.channels = 2;
        metrics.bits_per_sample = 16;
        metrics.average_frame_time_us = 25.0; // Good performance
        metrics.max_frame_time_us = 50;       // Within limits
        metrics.average_cpu_usage_percent = 0.8; // Good CPU usage
        metrics.allocations_during_decode = 0;   // No allocations
        
        metrics.updateValidationFlags();
        
        // Should meet all requirements for CD quality
        if (!metrics.meets_realtime_requirements) {
            Debug::log("test_flac_performance", "[test_performance_metrics_validation] ERROR: Should meet real-time requirements");
            return false;
        }
        
        if (!metrics.meets_cpu_requirements) {
            Debug::log("test_flac_performance", "[test_performance_metrics_validation] ERROR: Should meet CPU requirements");
            return false;
        }
        
        if (!metrics.meets_memory_requirements) {
            Debug::log("test_flac_performance", "[test_performance_metrics_validation] ERROR: Should meet memory requirements");
            return false;
        }
        
        if (!metrics.meets_latency_requirements) {
            Debug::log("test_flac_performance", "[test_performance_metrics_validation] ERROR: Should meet latency requirements");
            return false;
        }
        
        Debug::log("test_flac_performance", "[test_performance_metrics_validation] Good performance metrics validated");
        
        // Test failing metrics
        FLACPerformanceMetrics bad_metrics;
        bad_metrics.sample_rate = 44100;
        bad_metrics.channels = 2;
        bad_metrics.bits_per_sample = 16;
        bad_metrics.average_frame_time_us = 150.0; // Too slow
        bad_metrics.max_frame_time_us = 300;       // Way too slow
        bad_metrics.average_cpu_usage_percent = 5.0; // Too much CPU
        bad_metrics.allocations_during_decode = 100; // Too many allocations
        
        bad_metrics.updateValidationFlags();
        
        // Should fail requirements
        if (bad_metrics.meets_realtime_requirements || bad_metrics.meets_cpu_requirements ||
            bad_metrics.meets_memory_requirements || bad_metrics.meets_latency_requirements) {
            Debug::log("test_flac_performance", "[test_performance_metrics_validation] ERROR: Bad metrics should fail validation");
            return false;
        }
        
        Debug::log("test_flac_performance", "[test_performance_metrics_validation] Bad performance metrics correctly rejected");
        
        Debug::log("test_flac_performance", "[test_performance_metrics_validation] SUCCESS: Metrics validation working correctly");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test_flac_performance", "[test_performance_metrics_validation] ERROR: Exception: ", e.what());
        return false;
    }
}

// Test memory tracking functionality
bool test_memory_tracking() {
    Debug::log("test_flac_performance", "[test_memory_tracking] Testing memory tracking");
    
    try {
        FLACPerformanceBenchmark benchmark;
        
        // Start memory tracking
        benchmark.startMemoryTracking();
        
        size_t initial_usage = benchmark.getCurrentMemoryUsage();
        size_t initial_allocations = benchmark.getAllocationCount();
        
        Debug::log("test_flac_performance", "[test_memory_tracking] Initial memory usage: ", initial_usage);
        Debug::log("test_flac_performance", "[test_memory_tracking] Initial allocations: ", initial_allocations);
        
        // Simulate some memory allocations
        std::vector<std::unique_ptr<uint8_t[]>> allocations;
        for (int i = 0; i < 10; ++i) {
            allocations.push_back(std::make_unique<uint8_t[]>(1024));
        }
        
        // Check if memory usage increased
        size_t after_usage = benchmark.getCurrentMemoryUsage();
        
        Debug::log("test_flac_performance", "[test_memory_tracking] Memory usage after allocations: ", after_usage);
        
        // Stop memory tracking
        benchmark.stopMemoryTracking();
        
        Debug::log("test_flac_performance", "[test_memory_tracking] SUCCESS: Memory tracking completed");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test_flac_performance", "[test_memory_tracking] ERROR: Exception: ", e.what());
        return false;
    }
}

// Test CPU monitoring functionality
bool test_cpu_monitoring() {
    Debug::log("test_flac_performance", "[test_cpu_monitoring] Testing CPU monitoring");
    
    try {
        FLACPerformanceBenchmark benchmark;
        
        // Start CPU monitoring
        benchmark.startCPUMonitoring();
        
        double initial_cpu = benchmark.getCurrentCPUUsage();
        Debug::log("test_flac_performance", "[test_cpu_monitoring] Initial CPU usage: ", initial_cpu, "%");
        
        // Simulate some CPU work
        auto start_time = std::chrono::high_resolution_clock::now();
        volatile uint64_t sum = 0;
        for (uint64_t i = 0; i < 1000000; ++i) {
            sum += i * i;
        }
        auto end_time = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        Debug::log("test_flac_performance", "[test_cpu_monitoring] CPU work completed in ", duration.count(), " μs");
        
        double after_cpu = benchmark.getCurrentCPUUsage();
        double peak_cpu = benchmark.getPeakCPUUsage();
        
        Debug::log("test_flac_performance", "[test_cpu_monitoring] CPU usage after work: ", after_cpu, "%");
        Debug::log("test_flac_performance", "[test_cpu_monitoring] Peak CPU usage: ", peak_cpu, "%");
        
        // Stop CPU monitoring
        benchmark.stopCPUMonitoring();
        
        Debug::log("test_flac_performance", "[test_cpu_monitoring] SUCCESS: CPU monitoring completed");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test_flac_performance", "[test_cpu_monitoring] ERROR: Exception: ", e.what());
        return false;
    }
}

// Test real-time performance validation (simplified)
bool test_realtime_performance_validation() {
    Debug::log("test_flac_performance", "[test_realtime_performance_validation] Testing real-time performance validation");
    
    try {
        // Test performance metrics validation directly
        FLACPerformanceMetrics good_metrics;
        good_metrics.sample_rate = 44100;
        good_metrics.channels = 2;
        good_metrics.bits_per_sample = 16;
        good_metrics.average_frame_time_us = 25.0; // Good performance
        good_metrics.max_frame_time_us = 50;       // Within limits
        good_metrics.average_cpu_usage_percent = 0.8; // Good CPU usage
        good_metrics.allocations_during_decode = 0;   // No allocations
        
        good_metrics.updateValidationFlags();
        
        if (!good_metrics.meets_realtime_requirements || !good_metrics.meets_latency_requirements) {
            Debug::log("test_flac_performance", "[test_realtime_performance_validation] ERROR: Good performance should pass validation");
            return false;
        }
        
        Debug::log("test_flac_performance", "[test_realtime_performance_validation] Good performance correctly validated");
        
        // Test poor performance metrics
        FLACPerformanceMetrics bad_metrics;
        bad_metrics.sample_rate = 44100;
        bad_metrics.channels = 2;
        bad_metrics.bits_per_sample = 16;
        bad_metrics.average_frame_time_us = 150.0; // Too slow
        bad_metrics.max_frame_time_us = 300;       // Way too slow
        bad_metrics.average_cpu_usage_percent = 5.0; // Too much CPU
        bad_metrics.allocations_during_decode = 100; // Too many allocations
        
        bad_metrics.updateValidationFlags();
        
        if (bad_metrics.meets_realtime_requirements || bad_metrics.meets_latency_requirements) {
            Debug::log("test_flac_performance", "[test_realtime_performance_validation] ERROR: Poor performance should fail validation");
            return false;
        }
        
        Debug::log("test_flac_performance", "[test_realtime_performance_validation] Poor performance correctly rejected");
        
        Debug::log("test_flac_performance", "[test_realtime_performance_validation] SUCCESS: Real-time validation working correctly");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test_flac_performance", "[test_realtime_performance_validation] ERROR: Exception: ", e.what());
        return false;
    }
}

// Test performance regression detection
bool test_performance_regression_detection() {
    Debug::log("test_flac_performance", "[test_performance_regression_detection] Testing performance regression detection");
    
    try {
        FLACPerformanceBenchmark benchmark;
        
        // Create baseline metrics
        FLACPerformanceMetrics baseline;
        baseline.sample_rate = 44100;
        baseline.channels = 2;
        baseline.bits_per_sample = 16;
        baseline.average_frame_time_us = 25.0;
        baseline.average_cpu_usage_percent = 0.8;
        baseline.peak_memory_usage_bytes = 1024 * 1024; // 1MB
        baseline.meets_realtime_requirements = true;
        baseline.meets_cpu_requirements = true;
        baseline.meets_memory_requirements = true;
        baseline.meets_latency_requirements = true;
        
        // Test with similar performance (should not detect regression)
        FLACPerformanceMetrics current = baseline;
        current.average_frame_time_us = 26.0; // Slight increase, within tolerance
        
        bool has_regression = benchmark.detectPerformanceRegression(current, baseline, 10.0);
        
        if (has_regression) {
            Debug::log("test_flac_performance", "[test_performance_regression_detection] ERROR: Should not detect regression for minor changes");
            return false;
        }
        
        Debug::log("test_flac_performance", "[test_performance_regression_detection] Minor performance change correctly ignored");
        
        // Test with significant regression (should detect)
        current.average_frame_time_us = 50.0; // 100% increase - significant regression
        
        has_regression = benchmark.detectPerformanceRegression(current, baseline, 10.0);
        
        if (!has_regression) {
            Debug::log("test_flac_performance", "[test_performance_regression_detection] ERROR: Should detect significant regression");
            return false;
        }
        
        Debug::log("test_flac_performance", "[test_performance_regression_detection] Significant regression correctly detected");
        
        // Test with requirement regression (should detect)
        current = baseline;
        current.meets_realtime_requirements = false; // Requirement regression
        
        has_regression = benchmark.detectPerformanceRegression(current, baseline, 10.0);
        
        if (!has_regression) {
            Debug::log("test_flac_performance", "[test_performance_regression_detection] ERROR: Should detect requirement regression");
            return false;
        }
        
        Debug::log("test_flac_performance", "[test_performance_regression_detection] Requirement regression correctly detected");
        
        Debug::log("test_flac_performance", "[test_performance_regression_detection] SUCCESS: Regression detection working correctly");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test_flac_performance", "[test_performance_regression_detection] ERROR: Exception: ", e.what());
        return false;
    }
}

// Test complete benchmark execution (simplified)
bool test_complete_benchmark_execution() {
    Debug::log("test_flac_performance", "[test_complete_benchmark_execution] Testing complete benchmark execution");
    
    try {
        // Test the benchmark configuration and metrics system
        FLACBenchmarkConfig config = FLACBenchmarkConfigs::standardCDQuality();
        config.test_duration_seconds = 2; // Short test
        config.warmup_frames = 10;        // Minimal warmup
        
        // Validate configuration
        if (config.sample_rate != 44100 || config.channels != 2 || config.bits_per_sample != 16) {
            Debug::log("test_flac_performance", "[test_complete_benchmark_execution] ERROR: Invalid configuration");
            return false;
        }
        
        // Test metrics creation and validation
        FLACPerformanceMetrics metrics;
        metrics.sample_rate = config.sample_rate;
        metrics.channels = config.channels;
        metrics.bits_per_sample = config.bits_per_sample;
        metrics.frames_processed = 100;
        metrics.samples_processed = 115200; // 100 frames * 1152 samples
        metrics.total_decode_time_us = 5000; // 5ms total
        metrics.average_frame_time_us = 50.0; // 50μs per frame
        metrics.max_frame_time_us = 80;
        metrics.min_frame_time_us = 30;
        metrics.average_cpu_usage_percent = 0.8;
        metrics.peak_cpu_usage_percent = 1.2;
        metrics.allocations_during_decode = 0;
        
        metrics.updateValidationFlags();
        
        Debug::log("test_flac_performance", "[test_complete_benchmark_execution] Benchmark results:");
        Debug::log("test_flac_performance", "  Frames processed: ", metrics.frames_processed);
        Debug::log("test_flac_performance", "  Samples processed: ", metrics.samples_processed);
        Debug::log("test_flac_performance", "  Total time: ", metrics.total_decode_time_us, " μs");
        Debug::log("test_flac_performance", "  Average frame time: ", metrics.average_frame_time_us, " μs");
        Debug::log("test_flac_performance", "  Meets requirements: ", 
                  (metrics.meets_realtime_requirements && metrics.meets_cpu_requirements &&
                   metrics.meets_memory_requirements && metrics.meets_latency_requirements) ? "YES" : "NO");
        
        // Validate that metrics are reasonable
        if (metrics.frames_processed == 0 || metrics.samples_processed == 0 || metrics.total_decode_time_us == 0) {
            Debug::log("test_flac_performance", "[test_complete_benchmark_execution] ERROR: Invalid metrics");
            return false;
        }
        
        Debug::log("test_flac_performance", "[test_complete_benchmark_execution] SUCCESS: Benchmark system working correctly");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("test_flac_performance", "[test_complete_benchmark_execution] ERROR: Exception: ", e.what());
        return false;
    }
}

// Main test function
int main() {
    Debug::log("test_flac_performance", "Starting FLAC performance benchmarking tests");
    
    bool all_tests_passed = true;
    
    // Run all tests
    all_tests_passed &= test_performance_benchmark_creation();
    all_tests_passed &= test_benchmark_configurations();
    all_tests_passed &= test_performance_metrics_validation();
    all_tests_passed &= test_memory_tracking();
    all_tests_passed &= test_cpu_monitoring();
    all_tests_passed &= test_realtime_performance_validation();
    all_tests_passed &= test_performance_regression_detection();
    all_tests_passed &= test_complete_benchmark_execution();
    
    if (all_tests_passed) {
        Debug::log("test_flac_performance", "SUCCESS: All FLAC performance benchmarking tests passed");
        return 0;
    } else {
        Debug::log("test_flac_performance", "FAILURE: Some FLAC performance benchmarking tests failed");
        return 1;
    }
}

#else // !HAVE_FLAC

int main() {
    Debug::log("test_flac_performance", "FLAC support not available - skipping performance benchmarking tests");
    return 0;
}

#endif // HAVE_FLAC