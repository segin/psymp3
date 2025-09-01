/*
 * FLACPerformanceBenchmark.h - Performance benchmarking and validation for FLAC codec
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

#ifndef FLACPERFORMANCEBENCHMARK_H
#define FLACPERFORMANCEBENCHMARK_H

#ifdef HAVE_FLAC

/**
 * @brief Performance metrics for FLAC codec benchmarking
 * 
 * This structure contains detailed performance measurements for validating
 * real-time performance requirements per RFC 9639 compliance validation.
 */
struct FLACPerformanceMetrics {
    // Timing measurements (in microseconds)
    uint64_t frame_decode_time_us = 0;      ///< Time to decode single frame
    uint64_t total_decode_time_us = 0;      ///< Total decoding time
    uint64_t max_frame_time_us = 0;         ///< Maximum single frame decode time
    uint64_t min_frame_time_us = UINT64_MAX; ///< Minimum single frame decode time
    double average_frame_time_us = 0.0;     ///< Average frame decode time
    
    // CPU usage measurements
    double cpu_usage_percent = 0.0;         ///< CPU usage percentage
    double peak_cpu_usage_percent = 0.0;    ///< Peak CPU usage
    double average_cpu_usage_percent = 0.0; ///< Average CPU usage
    
    // Memory allocation tracking
    size_t allocations_during_decode = 0;   ///< Memory allocations during steady-state
    size_t peak_memory_usage_bytes = 0;     ///< Peak memory usage
    size_t current_memory_usage_bytes = 0;  ///< Current memory usage
    size_t total_allocated_bytes = 0;       ///< Total bytes allocated
    size_t total_deallocated_bytes = 0;     ///< Total bytes deallocated
    
    // Performance validation flags
    bool meets_realtime_requirements = false; ///< True if meets real-time requirements
    bool meets_cpu_requirements = false;      ///< True if CPU usage <1% for 44.1kHz/16-bit
    bool meets_memory_requirements = false;   ///< True if zero allocations in steady-state
    bool meets_latency_requirements = false;  ///< True if frame processing <100μs
    
    // Stream characteristics
    uint32_t sample_rate = 0;               ///< Sample rate being tested
    uint16_t channels = 0;                  ///< Number of channels
    uint16_t bits_per_sample = 0;           ///< Bits per sample
    uint32_t frames_processed = 0;          ///< Number of frames processed
    uint64_t samples_processed = 0;         ///< Total samples processed
    
    /**
     * @brief Check if performance meets real-time requirements
     */
    bool meetsRealTimeRequirements() const {
        // Real-time requirement: frame processing must complete faster than playback
        double samples_per_second = static_cast<double>(sample_rate);
        double max_allowed_time_us = (1000000.0 / samples_per_second) * 1024.0; // Assume 1024 sample frames
        
        return average_frame_time_us < max_allowed_time_us && 
               max_frame_time_us < (max_allowed_time_us * 2.0); // Allow 2x headroom for worst case
    }
    
    /**
     * @brief Check if CPU usage meets efficiency requirements
     */
    bool meetsCPURequirements() const {
        // For 44.1kHz/16-bit: <1% CPU usage
        // For higher resolutions: scale proportionally
        double base_sample_rate = 44100.0;
        double base_bit_depth = 16.0;
        double cpu_scaling_factor = (static_cast<double>(sample_rate) / base_sample_rate) * 
                                   (static_cast<double>(bits_per_sample) / base_bit_depth);
        
        double max_allowed_cpu = 1.0 * cpu_scaling_factor;
        return average_cpu_usage_percent < max_allowed_cpu;
    }
    
    /**
     * @brief Check if memory allocation meets steady-state requirements
     */
    bool meetsMemoryRequirements() const {
        // Zero allocations during steady-state decoding
        return allocations_during_decode == 0;
    }
    
    /**
     * @brief Check if latency meets real-time requirements
     */
    bool meetsLatencyRequirements() const {
        // Frame processing should complete in <100μs for real-time
        return max_frame_time_us < 100 && average_frame_time_us < 50;
    }
    
    /**
     * @brief Update overall validation flags
     */
    void updateValidationFlags() {
        meets_realtime_requirements = meetsRealTimeRequirements();
        meets_cpu_requirements = meetsCPURequirements();
        meets_memory_requirements = meetsMemoryRequirements();
        meets_latency_requirements = meetsLatencyRequirements();
    }
};

/**
 * @brief Performance benchmark configuration for different test scenarios
 */
struct FLACBenchmarkConfig {
    uint32_t sample_rate = 44100;           ///< Target sample rate for testing
    uint16_t channels = 2;                  ///< Number of channels
    uint16_t bits_per_sample = 16;          ///< Bits per sample
    uint32_t test_duration_seconds = 10;    ///< Duration of benchmark test
    uint32_t warmup_frames = 100;           ///< Number of warmup frames before measurement
    bool enable_cpu_monitoring = true;      ///< Enable CPU usage monitoring
    bool enable_memory_tracking = true;     ///< Enable memory allocation tracking
    bool enable_latency_measurement = true; ///< Enable frame latency measurement
    
    /**
     * @brief Get test description string
     */
    std::string getDescription() const {
        return std::to_string(sample_rate) + "Hz/" + 
               std::to_string(bits_per_sample) + "-bit/" + 
               std::to_string(channels) + "ch";
    }
    
    /**
     * @brief Calculate expected frame count for test duration
     */
    uint32_t getExpectedFrameCount(uint32_t block_size = 1152) const {
        uint64_t total_samples = static_cast<uint64_t>(sample_rate) * test_duration_seconds;
        return static_cast<uint32_t>(total_samples / block_size);
    }
};

/**
 * @brief FLAC codec performance benchmark and validation system
 * 
 * This class provides comprehensive performance benchmarking and validation
 * for FLAC codec implementations to ensure they meet real-time requirements
 * as specified in the RFC 9639 compliance validation tasks.
 * 
 * @thread_safety This class is NOT thread-safe. Use from single thread only.
 */
class FLACPerformanceBenchmark {
public:
    FLACPerformanceBenchmark();
    ~FLACPerformanceBenchmark();
    
    // Benchmark execution methods
    FLACPerformanceMetrics runBenchmark(FLACCodec& codec, const FLACBenchmarkConfig& config);
    FLACPerformanceMetrics runStandardBenchmarks(FLACCodec& codec);
    FLACPerformanceMetrics runHighResolutionBenchmark(FLACCodec& codec);
    FLACPerformanceMetrics runRegressionTest(FLACCodec& codec, const FLACPerformanceMetrics& baseline);
    
    // Real-time validation methods
    bool validateRealTimePerformance(FLACCodec& codec, uint32_t sample_rate, uint16_t bits_per_sample);
    bool validateCPUUsage(FLACCodec& codec, const FLACBenchmarkConfig& config);
    bool validateMemoryAllocation(FLACCodec& codec, const FLACBenchmarkConfig& config);
    bool validateFrameProcessingTime(FLACCodec& codec, const FLACBenchmarkConfig& config);
    
    // Performance regression detection
    bool detectPerformanceRegression(const FLACPerformanceMetrics& current, 
                                   const FLACPerformanceMetrics& baseline,
                                   double tolerance_percent = 10.0);
    
    // Benchmark result analysis
    void generatePerformanceReport(const FLACPerformanceMetrics& metrics, 
                                 const FLACBenchmarkConfig& config);
    void logPerformanceMetrics(const FLACPerformanceMetrics& metrics);
    bool validatePerformanceRequirements(const FLACPerformanceMetrics& metrics);
    
    // Memory allocation monitoring
    void startMemoryTracking();
    void stopMemoryTracking();
    size_t getCurrentMemoryUsage() const;
    size_t getAllocationCount() const;
    
    // CPU usage monitoring
    void startCPUMonitoring();
    void stopCPUMonitoring();
    double getCurrentCPUUsage() const;
    double getPeakCPUUsage() const;
    
private:
    // Performance measurement state
    bool m_memory_tracking_active = false;
    bool m_cpu_monitoring_active = false;
    
    // Memory tracking
    size_t m_baseline_memory_usage = 0;
    size_t m_peak_memory_usage = 0;
    size_t m_allocation_count = 0;
    size_t m_deallocation_count = 0;
    
    // CPU monitoring
    std::chrono::high_resolution_clock::time_point m_cpu_monitor_start;
    double m_peak_cpu_usage = 0.0;
    std::vector<double> m_cpu_samples;
    
    // Timing measurement
    std::chrono::high_resolution_clock::time_point m_benchmark_start;
    std::vector<uint64_t> m_frame_times;
    
    // Internal benchmark methods
    FLACPerformanceMetrics measureFramePerformance(FLACCodec& codec, 
                                                  const std::vector<MediaChunk>& test_chunks,
                                                  const FLACBenchmarkConfig& config);
    
    std::vector<MediaChunk> generateTestChunks(const FLACBenchmarkConfig& config);
    MediaChunk generateFLACFrame(uint32_t sample_rate, uint16_t channels, 
                                uint16_t bits_per_sample, uint32_t block_size);
    
    // Performance measurement utilities
    uint64_t measureFrameDecodeTime(FLACCodec& codec, const MediaChunk& chunk);
    double measureCPUUsageForDuration(std::chrono::milliseconds duration);
    size_t measureMemoryUsageDelta();
    
    // System resource monitoring
    double getCurrentSystemCPUUsage() const;
    size_t getCurrentSystemMemoryUsage() const;
    
    // Validation helpers
    bool validateLatencyRequirement(uint64_t frame_time_us, uint32_t sample_rate, uint32_t block_size);
    bool validateThroughputRequirement(uint64_t total_time_us, uint64_t samples_processed, uint32_t sample_rate);
    bool validateMemoryEfficiency(size_t allocations, size_t peak_memory);
    
    // Test data generation
    std::vector<uint8_t> generateFLACFrameData(uint32_t sample_rate, uint16_t channels,
                                              uint16_t bits_per_sample, uint32_t block_size,
                                              const std::vector<int32_t>& samples);
    
    std::vector<int32_t> generateTestSamples(uint32_t block_size, uint16_t channels, 
                                           uint16_t bits_per_sample);
    
    // Performance analysis
    void analyzeFrameTimingDistribution(const std::vector<uint64_t>& frame_times,
                                      FLACPerformanceMetrics& metrics);
    void analyzeCPUUsagePattern(const std::vector<double>& cpu_samples,
                              FLACPerformanceMetrics& metrics);
    void analyzeMemoryUsagePattern(FLACPerformanceMetrics& metrics);
    
    // Reporting utilities
    void printPerformanceHeader(const FLACBenchmarkConfig& config);
    void printPerformanceResults(const FLACPerformanceMetrics& metrics);
    void printValidationResults(const FLACPerformanceMetrics& metrics);
    void printRegressionAnalysis(const FLACPerformanceMetrics& current,
                               const FLACPerformanceMetrics& baseline);
};

/**
 * @brief Standard benchmark configurations for common test scenarios
 */
namespace FLACBenchmarkConfigs {
    
    /**
     * @brief Standard CD quality benchmark (44.1kHz/16-bit stereo)
     */
    inline FLACBenchmarkConfig standardCDQuality() {
        FLACBenchmarkConfig config;
        config.sample_rate = 44100;
        config.channels = 2;
        config.bits_per_sample = 16;
        config.test_duration_seconds = 10;
        return config;
    }
    
    /**
     * @brief High resolution audio benchmark (96kHz/24-bit stereo)
     */
    inline FLACBenchmarkConfig highResolution96k24() {
        FLACBenchmarkConfig config;
        config.sample_rate = 96000;
        config.channels = 2;
        config.bits_per_sample = 24;
        config.test_duration_seconds = 10;
        return config;
    }
    
    /**
     * @brief Ultra high resolution benchmark (192kHz/32-bit stereo)
     */
    inline FLACBenchmarkConfig ultraHighRes192k32() {
        FLACBenchmarkConfig config;
        config.sample_rate = 192000;
        config.channels = 2;
        config.bits_per_sample = 32;
        config.test_duration_seconds = 5; // Shorter test for very high data rates
        return config;
    }
    
    /**
     * @brief Mono benchmark for efficiency testing
     */
    inline FLACBenchmarkConfig monoEfficiency() {
        FLACBenchmarkConfig config;
        config.sample_rate = 44100;
        config.channels = 1;
        config.bits_per_sample = 16;
        config.test_duration_seconds = 15;
        return config;
    }
    
    /**
     * @brief Multi-channel benchmark (5.1 surround)
     */
    inline FLACBenchmarkConfig multiChannel51() {
        FLACBenchmarkConfig config;
        config.sample_rate = 48000;
        config.channels = 6;
        config.bits_per_sample = 24;
        config.test_duration_seconds = 8;
        return config;
    }
    
    /**
     * @brief Stress test configuration for maximum load
     */
    inline FLACBenchmarkConfig stressTest() {
        FLACBenchmarkConfig config;
        config.sample_rate = 192000;
        config.channels = 8;
        config.bits_per_sample = 32;
        config.test_duration_seconds = 3;
        config.warmup_frames = 50; // Reduced warmup for stress test
        return config;
    }
}

#endif // HAVE_FLAC

#endif // FLACPERFORMANCEBENCHMARK_H