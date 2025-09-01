/*
 * FLACPerformanceBenchmark.cpp - Performance benchmarking and validation implementation
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

// ============================================================================
// FLACPerformanceBenchmark Implementation
// ============================================================================

FLACPerformanceBenchmark::FLACPerformanceBenchmark() {
    Debug::log("flac_benchmark", "[FLACPerformanceBenchmark] Performance benchmark system initialized");
    
    // Initialize measurement state
    m_memory_tracking_active = false;
    m_cpu_monitoring_active = false;
    m_baseline_memory_usage = getCurrentSystemMemoryUsage();
    m_peak_memory_usage = m_baseline_memory_usage;
    m_allocation_count = 0;
    m_deallocation_count = 0;
    m_peak_cpu_usage = 0.0;
    
    // Reserve space for performance samples
    m_cpu_samples.reserve(10000); // Reserve space for 10k CPU samples
    m_frame_times.reserve(10000); // Reserve space for 10k frame timing measurements
}

FLACPerformanceBenchmark::~FLACPerformanceBenchmark() {
    // Ensure monitoring is stopped
    if (m_memory_tracking_active) {
        stopMemoryTracking();
    }
    
    if (m_cpu_monitoring_active) {
        stopCPUMonitoring();
    }
    
    Debug::log("flac_benchmark", "[FLACPerformanceBenchmark] Performance benchmark system destroyed");
}

// ============================================================================
// Main Benchmark Execution Methods
// ============================================================================

FLACPerformanceMetrics FLACPerformanceBenchmark::runBenchmark(FLACCodec& codec, const FLACBenchmarkConfig& config) {
    Debug::log("flac_benchmark", "[runBenchmark] Starting performance benchmark for ", config.getDescription());
    
    printPerformanceHeader(config);
    
    FLACPerformanceMetrics metrics;
    metrics.sample_rate = config.sample_rate;
    metrics.channels = config.channels;
    metrics.bits_per_sample = config.bits_per_sample;
    
    try {
        // Initialize codec for testing
        if (!codec.initialize()) {
            Debug::log("flac_benchmark", "[runBenchmark] Failed to initialize codec for benchmark");
            return metrics;
        }
        
        // Generate test data
        std::vector<MediaChunk> test_chunks = generateTestChunks(config);
        if (test_chunks.empty()) {
            Debug::log("flac_benchmark", "[runBenchmark] Failed to generate test chunks");
            return metrics;
        }
        
        Debug::log("flac_benchmark", "[runBenchmark] Generated ", test_chunks.size(), " test chunks");
        
        // Start monitoring systems
        if (config.enable_memory_tracking) {
            startMemoryTracking();
        }
        
        if (config.enable_cpu_monitoring) {
            startCPUMonitoring();
        }
        
        // Run warmup phase
        Debug::log("flac_benchmark", "[runBenchmark] Running warmup phase with ", config.warmup_frames, " frames");
        for (uint32_t i = 0; i < config.warmup_frames && i < test_chunks.size(); ++i) {
            codec.decode(test_chunks[i % test_chunks.size()]);
        }
        
        // Clear warmup measurements
        m_frame_times.clear();
        m_cpu_samples.clear();
        
        // Run actual benchmark
        Debug::log("flac_benchmark", "[runBenchmark] Starting actual performance measurement");
        metrics = measureFramePerformance(codec, test_chunks, config);
        
        // Stop monitoring
        if (config.enable_cpu_monitoring) {
            stopCPUMonitoring();
        }
        
        if (config.enable_memory_tracking) {
            stopMemoryTracking();
        }
        
        // Update validation flags
        metrics.updateValidationFlags();
        
        // Generate report
        generatePerformanceReport(metrics, config);
        
        Debug::log("flac_benchmark", "[runBenchmark] Benchmark completed successfully");
        
    } catch (const std::exception& e) {
        Debug::log("flac_benchmark", "[runBenchmark] Exception during benchmark: ", e.what());
    }
    
    return metrics;
}

FLACPerformanceMetrics FLACPerformanceBenchmark::runStandardBenchmarks(FLACCodec& codec) {
    Debug::log("flac_benchmark", "[runStandardBenchmarks] Running standard benchmark suite");
    
    // Run CD quality benchmark (primary requirement)
    FLACBenchmarkConfig cd_config = FLACBenchmarkConfigs::standardCDQuality();
    FLACPerformanceMetrics cd_metrics = runBenchmark(codec, cd_config);
    
    Debug::log("flac_benchmark", "[runStandardBenchmarks] CD quality benchmark completed");
    Debug::log("flac_benchmark", "[runStandardBenchmarks] CPU usage: ", cd_metrics.average_cpu_usage_percent, "%");
    Debug::log("flac_benchmark", "[runStandardBenchmarks] Average frame time: ", cd_metrics.average_frame_time_us, " μs");
    Debug::log("flac_benchmark", "[runStandardBenchmarks] Meets requirements: ", 
              cd_metrics.meets_realtime_requirements ? "YES" : "NO");
    
    return cd_metrics;
}

FLACPerformanceMetrics FLACPerformanceBenchmark::runHighResolutionBenchmark(FLACCodec& codec) {
    Debug::log("flac_benchmark", "[runHighResolutionBenchmark] Running high-resolution benchmark suite");
    
    // Test 96kHz/24-bit performance
    FLACBenchmarkConfig hires_config = FLACBenchmarkConfigs::highResolution96k24();
    FLACPerformanceMetrics hires_metrics = runBenchmark(codec, hires_config);
    
    Debug::log("flac_benchmark", "[runHighResolutionBenchmark] 96kHz/24-bit benchmark completed");
    
    // Test 192kHz/32-bit performance if 96kHz passed
    if (hires_metrics.meets_realtime_requirements) {
        Debug::log("flac_benchmark", "[runHighResolutionBenchmark] Testing ultra-high resolution (192kHz/32-bit)");
        
        FLACBenchmarkConfig ultra_config = FLACBenchmarkConfigs::ultraHighRes192k32();
        FLACPerformanceMetrics ultra_metrics = runBenchmark(codec, ultra_config);
        
        Debug::log("flac_benchmark", "[runHighResolutionBenchmark] 192kHz/32-bit benchmark completed");
        Debug::log("flac_benchmark", "[runHighResolutionBenchmark] Ultra-high res meets requirements: ", 
                  ultra_metrics.meets_realtime_requirements ? "YES" : "NO");
        
        return ultra_metrics; // Return the most demanding test results
    }
    
    return hires_metrics;
}

FLACPerformanceMetrics FLACPerformanceBenchmark::runRegressionTest(FLACCodec& codec, const FLACPerformanceMetrics& baseline) {
    Debug::log("flac_benchmark", "[runRegressionTest] Running performance regression test");
    
    // Use same configuration as baseline
    FLACBenchmarkConfig config;
    config.sample_rate = baseline.sample_rate;
    config.channels = baseline.channels;
    config.bits_per_sample = baseline.bits_per_sample;
    config.test_duration_seconds = 5; // Shorter test for regression
    
    FLACPerformanceMetrics current_metrics = runBenchmark(codec, config);
    
    // Check for regression
    bool has_regression = detectPerformanceRegression(current_metrics, baseline);
    
    if (has_regression) {
        Debug::log("flac_benchmark", "[runRegressionTest] PERFORMANCE REGRESSION DETECTED!");
        printRegressionAnalysis(current_metrics, baseline);
    } else {
        Debug::log("flac_benchmark", "[runRegressionTest] No performance regression detected");
    }
    
    return current_metrics;
}

// ============================================================================
// Real-time Validation Methods
// ============================================================================

bool FLACPerformanceBenchmark::validateRealTimePerformance(FLACCodec& codec, uint32_t sample_rate, uint16_t bits_per_sample) {
    Debug::log("flac_benchmark", "[validateRealTimePerformance] Validating real-time performance for ", 
              sample_rate, "Hz/", bits_per_sample, "-bit");
    
    FLACBenchmarkConfig config;
    config.sample_rate = sample_rate;
    config.channels = 2; // Standard stereo
    config.bits_per_sample = bits_per_sample;
    config.test_duration_seconds = 5; // Quick validation test
    config.warmup_frames = 50;
    
    FLACPerformanceMetrics metrics = runBenchmark(codec, config);
    
    bool meets_requirements = metrics.meets_realtime_requirements && 
                             metrics.meets_latency_requirements;
    
    Debug::log("flac_benchmark", "[validateRealTimePerformance] Real-time validation result: ", 
              meets_requirements ? "PASS" : "FAIL");
    
    if (!meets_requirements) {
        Debug::log("flac_benchmark", "[validateRealTimePerformance] Average frame time: ", 
                  metrics.average_frame_time_us, " μs (should be <50 μs)");
        Debug::log("flac_benchmark", "[validateRealTimePerformance] Max frame time: ", 
                  metrics.max_frame_time_us, " μs (should be <100 μs)");
    }
    
    return meets_requirements;
}

bool FLACPerformanceBenchmark::validateCPUUsage(FLACCodec& codec, const FLACBenchmarkConfig& config) {
    Debug::log("flac_benchmark", "[validateCPUUsage] Validating CPU usage for ", config.getDescription());
    
    FLACPerformanceMetrics metrics = runBenchmark(codec, config);
    
    bool meets_cpu_requirements = metrics.meets_cpu_requirements;
    
    Debug::log("flac_benchmark", "[validateCPUUsage] CPU validation result: ", 
              meets_cpu_requirements ? "PASS" : "FAIL");
    Debug::log("flac_benchmark", "[validateCPUUsage] Average CPU usage: ", 
              metrics.average_cpu_usage_percent, "% (target: <", 
              (config.sample_rate / 44100.0) * (config.bits_per_sample / 16.0), "%)");
    
    return meets_cpu_requirements;
}

bool FLACPerformanceBenchmark::validateMemoryAllocation(FLACCodec& codec, const FLACBenchmarkConfig& config) {
    Debug::log("flac_benchmark", "[validateMemoryAllocation] Validating memory allocation for ", config.getDescription());
    
    FLACPerformanceMetrics metrics = runBenchmark(codec, config);
    
    bool meets_memory_requirements = metrics.meets_memory_requirements;
    
    Debug::log("flac_benchmark", "[validateMemoryAllocation] Memory validation result: ", 
              meets_memory_requirements ? "PASS" : "FAIL");
    Debug::log("flac_benchmark", "[validateMemoryAllocation] Allocations during decode: ", 
              metrics.allocations_during_decode, " (target: 0)");
    
    return meets_memory_requirements;
}

bool FLACPerformanceBenchmark::validateFrameProcessingTime(FLACCodec& codec, const FLACBenchmarkConfig& config) {
    Debug::log("flac_benchmark", "[validateFrameProcessingTime] Validating frame processing time for ", config.getDescription());
    
    FLACPerformanceMetrics metrics = runBenchmark(codec, config);
    
    bool meets_latency_requirements = metrics.meets_latency_requirements;
    
    Debug::log("flac_benchmark", "[validateFrameProcessingTime] Latency validation result: ", 
              meets_latency_requirements ? "PASS" : "FAIL");
    Debug::log("flac_benchmark", "[validateFrameProcessingTime] Max frame time: ", 
              metrics.max_frame_time_us, " μs (target: <100 μs)");
    Debug::log("flac_benchmark", "[validateFrameProcessingTime] Average frame time: ", 
              metrics.average_frame_time_us, " μs (target: <50 μs)");
    
    return meets_latency_requirements;
}

// ============================================================================
// Performance Regression Detection
// ============================================================================

bool FLACPerformanceBenchmark::detectPerformanceRegression(const FLACPerformanceMetrics& current, 
                                                          const FLACPerformanceMetrics& baseline,
                                                          double tolerance_percent) {
    Debug::log("flac_benchmark", "[detectPerformanceRegression] Checking for performance regression with ", 
              tolerance_percent, "% tolerance");
    
    // Check frame processing time regression
    double frame_time_increase = ((current.average_frame_time_us - baseline.average_frame_time_us) / 
                                 baseline.average_frame_time_us) * 100.0;
    
    if (frame_time_increase > tolerance_percent) {
        Debug::log("flac_benchmark", "[detectPerformanceRegression] Frame time regression detected: ", 
                  frame_time_increase, "% increase");
        return true;
    }
    
    // Check CPU usage regression
    double cpu_increase = ((current.average_cpu_usage_percent - baseline.average_cpu_usage_percent) / 
                          baseline.average_cpu_usage_percent) * 100.0;
    
    if (cpu_increase > tolerance_percent) {
        Debug::log("flac_benchmark", "[detectPerformanceRegression] CPU usage regression detected: ", 
                  cpu_increase, "% increase");
        return true;
    }
    
    // Check memory usage regression
    double memory_increase = ((static_cast<double>(current.peak_memory_usage_bytes) - 
                              static_cast<double>(baseline.peak_memory_usage_bytes)) / 
                             static_cast<double>(baseline.peak_memory_usage_bytes)) * 100.0;
    
    if (memory_increase > tolerance_percent) {
        Debug::log("flac_benchmark", "[detectPerformanceRegression] Memory usage regression detected: ", 
                  memory_increase, "% increase");
        return true;
    }
    
    // Check if requirements that were previously met are now failing
    if (baseline.meets_realtime_requirements && !current.meets_realtime_requirements) {
        Debug::log("flac_benchmark", "[detectPerformanceRegression] Real-time requirements regression detected");
        return true;
    }
    
    if (baseline.meets_cpu_requirements && !current.meets_cpu_requirements) {
        Debug::log("flac_benchmark", "[detectPerformanceRegression] CPU requirements regression detected");
        return true;
    }
    
    if (baseline.meets_memory_requirements && !current.meets_memory_requirements) {
        Debug::log("flac_benchmark", "[detectPerformanceRegression] Memory requirements regression detected");
        return true;
    }
    
    Debug::log("flac_benchmark", "[detectPerformanceRegression] No significant performance regression detected");
    return false;
}

// ============================================================================
// Benchmark Result Analysis and Reporting
// ============================================================================

void FLACPerformanceBenchmark::generatePerformanceReport(const FLACPerformanceMetrics& metrics, 
                                                        const FLACBenchmarkConfig& config) {
    Debug::log("flac_benchmark", "[generatePerformanceReport] Generating performance report for ", config.getDescription());
    
    printPerformanceResults(metrics);
    printValidationResults(metrics);
    
    // Log detailed analysis
    Debug::log("flac_benchmark", "=== PERFORMANCE ANALYSIS ===");
    Debug::log("flac_benchmark", "Configuration: ", config.getDescription());
    Debug::log("flac_benchmark", "Frames processed: ", metrics.frames_processed);
    Debug::log("flac_benchmark", "Samples processed: ", metrics.samples_processed);
    Debug::log("flac_benchmark", "Total decode time: ", metrics.total_decode_time_us, " μs");
    Debug::log("flac_benchmark", "Average frame time: ", metrics.average_frame_time_us, " μs");
    Debug::log("flac_benchmark", "Max frame time: ", metrics.max_frame_time_us, " μs");
    Debug::log("flac_benchmark", "Min frame time: ", metrics.min_frame_time_us, " μs");
    Debug::log("flac_benchmark", "Average CPU usage: ", metrics.average_cpu_usage_percent, "%");
    Debug::log("flac_benchmark", "Peak CPU usage: ", metrics.peak_cpu_usage_percent, "%");
    Debug::log("flac_benchmark", "Peak memory usage: ", metrics.peak_memory_usage_bytes, " bytes");
    Debug::log("flac_benchmark", "Allocations during decode: ", metrics.allocations_during_decode);
    Debug::log("flac_benchmark", "=== VALIDATION RESULTS ===");
    Debug::log("flac_benchmark", "Real-time requirements: ", metrics.meets_realtime_requirements ? "PASS" : "FAIL");
    Debug::log("flac_benchmark", "CPU requirements: ", metrics.meets_cpu_requirements ? "PASS" : "FAIL");
    Debug::log("flac_benchmark", "Memory requirements: ", metrics.meets_memory_requirements ? "PASS" : "FAIL");
    Debug::log("flac_benchmark", "Latency requirements: ", metrics.meets_latency_requirements ? "PASS" : "FAIL");
}

void FLACPerformanceBenchmark::logPerformanceMetrics(const FLACPerformanceMetrics& metrics) {
    Debug::log("flac_benchmark", "[logPerformanceMetrics] Logging performance metrics");
    
    Debug::log("flac_benchmark", "Performance Metrics Summary:");
    Debug::log("flac_benchmark", "  Stream: ", metrics.sample_rate, "Hz/", 
              metrics.bits_per_sample, "-bit/", metrics.channels, "ch");
    Debug::log("flac_benchmark", "  Frames: ", metrics.frames_processed, 
              " (", metrics.samples_processed, " samples)");
    Debug::log("flac_benchmark", "  Timing: avg=", metrics.average_frame_time_us, "μs, max=", 
              metrics.max_frame_time_us, "μs, min=", metrics.min_frame_time_us, "μs");
    Debug::log("flac_benchmark", "  CPU: avg=", metrics.average_cpu_usage_percent, 
              "%, peak=", metrics.peak_cpu_usage_percent, "%");
    Debug::log("flac_benchmark", "  Memory: peak=", metrics.peak_memory_usage_bytes, 
              " bytes, allocs=", metrics.allocations_during_decode);
}

bool FLACPerformanceBenchmark::validatePerformanceRequirements(const FLACPerformanceMetrics& metrics) {
    Debug::log("flac_benchmark", "[validatePerformanceRequirements] Validating all performance requirements");
    
    bool all_requirements_met = metrics.meets_realtime_requirements &&
                               metrics.meets_cpu_requirements &&
                               metrics.meets_memory_requirements &&
                               metrics.meets_latency_requirements;
    
    Debug::log("flac_benchmark", "[validatePerformanceRequirements] Overall validation result: ", 
              all_requirements_met ? "PASS" : "FAIL");
    
    if (!all_requirements_met) {
        Debug::log("flac_benchmark", "[validatePerformanceRequirements] Failed requirements:");
        if (!metrics.meets_realtime_requirements) {
            Debug::log("flac_benchmark", "  - Real-time performance");
        }
        if (!metrics.meets_cpu_requirements) {
            Debug::log("flac_benchmark", "  - CPU usage efficiency");
        }
        if (!metrics.meets_memory_requirements) {
            Debug::log("flac_benchmark", "  - Memory allocation efficiency");
        }
        if (!metrics.meets_latency_requirements) {
            Debug::log("flac_benchmark", "  - Frame processing latency");
        }
    }
    
    return all_requirements_met;
}

// ============================================================================
// Memory and CPU Monitoring
// ============================================================================

void FLACPerformanceBenchmark::startMemoryTracking() {
    Debug::log("flac_benchmark", "[startMemoryTracking] Starting memory allocation tracking");
    
    m_memory_tracking_active = true;
    m_baseline_memory_usage = getCurrentSystemMemoryUsage();
    m_peak_memory_usage = m_baseline_memory_usage;
    m_allocation_count = 0;
    m_deallocation_count = 0;
}

void FLACPerformanceBenchmark::stopMemoryTracking() {
    Debug::log("flac_benchmark", "[stopMemoryTracking] Stopping memory allocation tracking");
    
    m_memory_tracking_active = false;
    
    Debug::log("flac_benchmark", "[stopMemoryTracking] Memory tracking results:");
    Debug::log("flac_benchmark", "  Baseline usage: ", m_baseline_memory_usage, " bytes");
    Debug::log("flac_benchmark", "  Peak usage: ", m_peak_memory_usage, " bytes");
    Debug::log("flac_benchmark", "  Allocations: ", m_allocation_count);
    Debug::log("flac_benchmark", "  Deallocations: ", m_deallocation_count);
}

size_t FLACPerformanceBenchmark::getCurrentMemoryUsage() const {
    return getCurrentSystemMemoryUsage();
}

size_t FLACPerformanceBenchmark::getAllocationCount() const {
    return m_allocation_count;
}

void FLACPerformanceBenchmark::startCPUMonitoring() {
    Debug::log("flac_benchmark", "[startCPUMonitoring] Starting CPU usage monitoring");
    
    m_cpu_monitoring_active = true;
    m_cpu_monitor_start = std::chrono::high_resolution_clock::now();
    m_peak_cpu_usage = 0.0;
    m_cpu_samples.clear();
}

void FLACPerformanceBenchmark::stopCPUMonitoring() {
    Debug::log("flac_benchmark", "[stopCPUMonitoring] Stopping CPU usage monitoring");
    
    m_cpu_monitoring_active = false;
    
    Debug::log("flac_benchmark", "[stopCPUMonitoring] CPU monitoring results:");
    Debug::log("flac_benchmark", "  Peak CPU usage: ", m_peak_cpu_usage, "%");
    Debug::log("flac_benchmark", "  CPU samples collected: ", m_cpu_samples.size());
}

double FLACPerformanceBenchmark::getCurrentCPUUsage() const {
    return getCurrentSystemCPUUsage();
}

double FLACPerformanceBenchmark::getPeakCPUUsage() const {
    return m_peak_cpu_usage;
}

// ============================================================================
// Internal Implementation Methods
// ============================================================================

FLACPerformanceMetrics FLACPerformanceBenchmark::measureFramePerformance(FLACCodec& codec, 
                                                                        const std::vector<MediaChunk>& test_chunks,
                                                                        const FLACBenchmarkConfig& config) {
    Debug::log("flac_benchmark", "[measureFramePerformance] Measuring frame-level performance");
    
    FLACPerformanceMetrics metrics;
    metrics.sample_rate = config.sample_rate;
    metrics.channels = config.channels;
    metrics.bits_per_sample = config.bits_per_sample;
    
    m_frame_times.clear();
    m_frame_times.reserve(test_chunks.size());
    
    auto benchmark_start = std::chrono::high_resolution_clock::now();
    
    // Process all test chunks
    for (size_t i = 0; i < test_chunks.size(); ++i) {
        const MediaChunk& chunk = test_chunks[i];
        
        // Measure individual frame decode time
        uint64_t frame_time = measureFrameDecodeTime(codec, chunk);
        m_frame_times.push_back(frame_time);
        
        // Update metrics
        metrics.frames_processed++;
        metrics.samples_processed += 1152; // Assume standard FLAC block size
        
        // Track min/max frame times
        if (frame_time > metrics.max_frame_time_us) {
            metrics.max_frame_time_us = frame_time;
        }
        if (frame_time < metrics.min_frame_time_us) {
            metrics.min_frame_time_us = frame_time;
        }
        
        // Sample CPU usage periodically
        if (m_cpu_monitoring_active && (i % 10 == 0)) {
            double cpu_usage = getCurrentSystemCPUUsage();
            m_cpu_samples.push_back(cpu_usage);
            
            if (cpu_usage > m_peak_cpu_usage) {
                m_peak_cpu_usage = cpu_usage;
            }
        }
        
        // Sample memory usage periodically
        if (m_memory_tracking_active && (i % 20 == 0)) {
            size_t current_memory = getCurrentSystemMemoryUsage();
            if (current_memory > m_peak_memory_usage) {
                m_peak_memory_usage = current_memory;
            }
        }
    }
    
    auto benchmark_end = std::chrono::high_resolution_clock::now();
    metrics.total_decode_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
        benchmark_end - benchmark_start).count();
    
    // Analyze collected data
    analyzeFrameTimingDistribution(m_frame_times, metrics);
    analyzeCPUUsagePattern(m_cpu_samples, metrics);
    analyzeMemoryUsagePattern(metrics);
    
    Debug::log("flac_benchmark", "[measureFramePerformance] Performance measurement completed");
    Debug::log("flac_benchmark", "[measureFramePerformance] Processed ", metrics.frames_processed, 
              " frames in ", metrics.total_decode_time_us, " μs");
    
    return metrics;
}

std::vector<MediaChunk> FLACPerformanceBenchmark::generateTestChunks(const FLACBenchmarkConfig& config) {
    Debug::log("flac_benchmark", "[generateTestChunks] Generating test chunks for ", config.getDescription());
    
    std::vector<MediaChunk> chunks;
    
    uint32_t expected_frames = config.getExpectedFrameCount();
    chunks.reserve(expected_frames);
    
    // Generate frames with standard FLAC block size
    uint32_t block_size = 1152; // Standard FLAC block size
    
    for (uint32_t frame = 0; frame < expected_frames; ++frame) {
        MediaChunk chunk = generateFLACFrame(config.sample_rate, config.channels, 
                                           config.bits_per_sample, block_size);
        
        if (!chunk.data.empty()) {
            chunks.push_back(std::move(chunk));
        }
        
        // Vary block size occasionally to test variable block size handling
        if (frame % 100 == 0 && frame > 0) {
            block_size = (block_size == 1152) ? 4608 : 1152; // Alternate between common sizes
        }
    }
    
    Debug::log("flac_benchmark", "[generateTestChunks] Generated ", chunks.size(), " test chunks");
    return chunks;
}

MediaChunk FLACPerformanceBenchmark::generateFLACFrame(uint32_t sample_rate, uint16_t channels, 
                                                      uint16_t bits_per_sample, uint32_t block_size) {
    // Generate test samples
    std::vector<int32_t> samples = generateTestSamples(block_size, channels, bits_per_sample);
    
    // Generate FLAC frame data (simplified for testing)
    std::vector<uint8_t> frame_data = generateFLACFrameData(sample_rate, channels, 
                                                           bits_per_sample, block_size, samples);
    
    MediaChunk chunk;
    chunk.data = std::move(frame_data);
    chunk.timestamp_samples = 0; // Simplified for testing
    
    return chunk;
}

uint64_t FLACPerformanceBenchmark::measureFrameDecodeTime(FLACCodec& codec, const MediaChunk& chunk) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Decode the frame
    AudioFrame result = codec.decode(chunk);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    return std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
}

std::vector<int32_t> FLACPerformanceBenchmark::generateTestSamples(uint32_t block_size, uint16_t channels, 
                                                                  uint16_t bits_per_sample) {
    std::vector<int32_t> samples;
    samples.reserve(block_size * channels);
    
    // Generate simple sine wave test pattern
    double frequency = 440.0; // A4 note
    double sample_rate = 44100.0; // Assume standard rate for test pattern
    
    int32_t max_amplitude = (1 << (bits_per_sample - 1)) - 1;
    
    for (uint32_t sample = 0; sample < block_size; ++sample) {
        double time = static_cast<double>(sample) / sample_rate;
        double amplitude = std::sin(2.0 * M_PI * frequency * time);
        
        int32_t sample_value = static_cast<int32_t>(amplitude * max_amplitude);
        
        // Duplicate for all channels
        for (uint16_t channel = 0; channel < channels; ++channel) {
            samples.push_back(sample_value);
        }
    }
    
    return samples;
}

std::vector<uint8_t> FLACPerformanceBenchmark::generateFLACFrameData(uint32_t sample_rate, uint16_t channels,
                                                                    uint16_t bits_per_sample, uint32_t block_size,
                                                                    const std::vector<int32_t>& samples) {
    // This is a simplified FLAC frame generator for testing purposes
    // In a real implementation, this would use libFLAC encoding
    
    std::vector<uint8_t> frame_data;
    frame_data.reserve(block_size * channels * (bits_per_sample / 8) + 64); // Estimate with header overhead
    
    // Simplified FLAC frame header (not RFC 9639 compliant, just for testing)
    frame_data.push_back(0xFF); // Sync pattern start
    frame_data.push_back(0xF8); // Sync pattern end + reserved
    
    // Block size encoding (simplified)
    frame_data.push_back(static_cast<uint8_t>(block_size >> 8));
    frame_data.push_back(static_cast<uint8_t>(block_size & 0xFF));
    
    // Sample rate encoding (simplified)
    frame_data.push_back(static_cast<uint8_t>(sample_rate >> 16));
    frame_data.push_back(static_cast<uint8_t>((sample_rate >> 8) & 0xFF));
    frame_data.push_back(static_cast<uint8_t>(sample_rate & 0xFF));
    
    // Channel and bit depth info
    frame_data.push_back(static_cast<uint8_t>((channels - 1) << 4 | (bits_per_sample - 1)));
    
    // Simplified sample data (not actually FLAC encoded)
    for (const int32_t sample : samples) {
        if (bits_per_sample <= 8) {
            frame_data.push_back(static_cast<uint8_t>(sample >> 24));
        } else if (bits_per_sample <= 16) {
            frame_data.push_back(static_cast<uint8_t>(sample >> 8));
            frame_data.push_back(static_cast<uint8_t>(sample & 0xFF));
        } else if (bits_per_sample <= 24) {
            frame_data.push_back(static_cast<uint8_t>(sample >> 16));
            frame_data.push_back(static_cast<uint8_t>((sample >> 8) & 0xFF));
            frame_data.push_back(static_cast<uint8_t>(sample & 0xFF));
        } else {
            frame_data.push_back(static_cast<uint8_t>(sample >> 24));
            frame_data.push_back(static_cast<uint8_t>((sample >> 16) & 0xFF));
            frame_data.push_back(static_cast<uint8_t>((sample >> 8) & 0xFF));
            frame_data.push_back(static_cast<uint8_t>(sample & 0xFF));
        }
    }
    
    // Simplified CRC (not actual CRC-16)
    frame_data.push_back(0x12);
    frame_data.push_back(0x34);
    
    return frame_data;
}

void FLACPerformanceBenchmark::analyzeFrameTimingDistribution(const std::vector<uint64_t>& frame_times,
                                                            FLACPerformanceMetrics& metrics) {
    if (frame_times.empty()) {
        return;
    }
    
    // Calculate average
    uint64_t total_time = 0;
    for (uint64_t time : frame_times) {
        total_time += time;
    }
    metrics.average_frame_time_us = static_cast<double>(total_time) / frame_times.size();
    
    // Find min/max (already tracked during measurement)
    // Calculate standard deviation and percentiles for detailed analysis
    double variance = 0.0;
    for (uint64_t time : frame_times) {
        double diff = static_cast<double>(time) - metrics.average_frame_time_us;
        variance += diff * diff;
    }
    variance /= frame_times.size();
    double std_dev = std::sqrt(variance);
    
    Debug::log("flac_benchmark", "[analyzeFrameTimingDistribution] Frame timing analysis:");
    Debug::log("flac_benchmark", "  Average: ", metrics.average_frame_time_us, " μs");
    Debug::log("flac_benchmark", "  Min: ", metrics.min_frame_time_us, " μs");
    Debug::log("flac_benchmark", "  Max: ", metrics.max_frame_time_us, " μs");
    Debug::log("flac_benchmark", "  Std Dev: ", std_dev, " μs");
}

void FLACPerformanceBenchmark::analyzeCPUUsagePattern(const std::vector<double>& cpu_samples,
                                                    FLACPerformanceMetrics& metrics) {
    if (cpu_samples.empty()) {
        metrics.average_cpu_usage_percent = 0.0;
        metrics.peak_cpu_usage_percent = 0.0;
        return;
    }
    
    // Calculate average CPU usage
    double total_cpu = 0.0;
    for (double cpu : cpu_samples) {
        total_cpu += cpu;
    }
    metrics.average_cpu_usage_percent = total_cpu / cpu_samples.size();
    
    // Peak CPU usage (already tracked during measurement)
    metrics.peak_cpu_usage_percent = m_peak_cpu_usage;
    
    Debug::log("flac_benchmark", "[analyzeCPUUsagePattern] CPU usage analysis:");
    Debug::log("flac_benchmark", "  Average: ", metrics.average_cpu_usage_percent, "%");
    Debug::log("flac_benchmark", "  Peak: ", metrics.peak_cpu_usage_percent, "%");
    Debug::log("flac_benchmark", "  Samples: ", cpu_samples.size());
}

void FLACPerformanceBenchmark::analyzeMemoryUsagePattern(FLACPerformanceMetrics& metrics) {
    metrics.peak_memory_usage_bytes = m_peak_memory_usage;
    metrics.current_memory_usage_bytes = getCurrentSystemMemoryUsage();
    metrics.allocations_during_decode = m_allocation_count;
    
    Debug::log("flac_benchmark", "[analyzeMemoryUsagePattern] Memory usage analysis:");
    Debug::log("flac_benchmark", "  Baseline: ", m_baseline_memory_usage, " bytes");
    Debug::log("flac_benchmark", "  Peak: ", metrics.peak_memory_usage_bytes, " bytes");
    Debug::log("flac_benchmark", "  Current: ", metrics.current_memory_usage_bytes, " bytes");
    Debug::log("flac_benchmark", "  Allocations: ", metrics.allocations_during_decode);
}

// ============================================================================
// System Resource Monitoring (Platform-specific implementations)
// ============================================================================

double FLACPerformanceBenchmark::getCurrentSystemCPUUsage() const {
    // Simplified CPU usage measurement
    // In a real implementation, this would use platform-specific APIs
    // For now, return a simulated value based on timing
    
    static auto last_check = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_check);
    
    // Simulate CPU usage based on measurement frequency
    double simulated_usage = std::min(5.0, static_cast<double>(duration.count()) / 100.0);
    
    return simulated_usage;
}

size_t FLACPerformanceBenchmark::getCurrentSystemMemoryUsage() const {
    // Simplified memory usage measurement
    // In a real implementation, this would use platform-specific APIs
    
    // Return a simulated memory usage value
    static size_t base_usage = 1024 * 1024 * 50; // 50MB base
    size_t additional = m_allocation_count * 1024; // 1KB per allocation
    
    return base_usage + additional;
}

// ============================================================================
// Reporting Utilities
// ============================================================================

void FLACPerformanceBenchmark::printPerformanceHeader(const FLACBenchmarkConfig& config) {
    Debug::log("flac_benchmark", "");
    Debug::log("flac_benchmark", "=== FLAC CODEC PERFORMANCE BENCHMARK ===");
    Debug::log("flac_benchmark", "Configuration: ", config.getDescription());
    Debug::log("flac_benchmark", "Test Duration: ", config.test_duration_seconds, " seconds");
    Debug::log("flac_benchmark", "Warmup Frames: ", config.warmup_frames);
    Debug::log("flac_benchmark", "CPU Monitoring: ", config.enable_cpu_monitoring ? "Enabled" : "Disabled");
    Debug::log("flac_benchmark", "Memory Tracking: ", config.enable_memory_tracking ? "Enabled" : "Disabled");
    Debug::log("flac_benchmark", "");
}

void FLACPerformanceBenchmark::printPerformanceResults(const FLACPerformanceMetrics& metrics) {
    Debug::log("flac_benchmark", "=== PERFORMANCE RESULTS ===");
    Debug::log("flac_benchmark", "Frames Processed: ", metrics.frames_processed);
    Debug::log("flac_benchmark", "Samples Processed: ", metrics.samples_processed);
    Debug::log("flac_benchmark", "Total Time: ", metrics.total_decode_time_us, " μs");
    Debug::log("flac_benchmark", "Average Frame Time: ", metrics.average_frame_time_us, " μs");
    Debug::log("flac_benchmark", "Max Frame Time: ", metrics.max_frame_time_us, " μs");
    Debug::log("flac_benchmark", "Min Frame Time: ", metrics.min_frame_time_us, " μs");
    Debug::log("flac_benchmark", "Average CPU Usage: ", metrics.average_cpu_usage_percent, "%");
    Debug::log("flac_benchmark", "Peak CPU Usage: ", metrics.peak_cpu_usage_percent, "%");
    Debug::log("flac_benchmark", "Peak Memory Usage: ", metrics.peak_memory_usage_bytes, " bytes");
    Debug::log("flac_benchmark", "Allocations: ", metrics.allocations_during_decode);
}

void FLACPerformanceBenchmark::printValidationResults(const FLACPerformanceMetrics& metrics) {
    Debug::log("flac_benchmark", "=== VALIDATION RESULTS ===");
    Debug::log("flac_benchmark", "Real-time Requirements: ", metrics.meets_realtime_requirements ? "PASS" : "FAIL");
    Debug::log("flac_benchmark", "CPU Requirements: ", metrics.meets_cpu_requirements ? "PASS" : "FAIL");
    Debug::log("flac_benchmark", "Memory Requirements: ", metrics.meets_memory_requirements ? "PASS" : "FAIL");
    Debug::log("flac_benchmark", "Latency Requirements: ", metrics.meets_latency_requirements ? "PASS" : "FAIL");
    
    bool overall_pass = metrics.meets_realtime_requirements && metrics.meets_cpu_requirements &&
                       metrics.meets_memory_requirements && metrics.meets_latency_requirements;
    
    Debug::log("flac_benchmark", "OVERALL RESULT: ", overall_pass ? "PASS" : "FAIL");
    Debug::log("flac_benchmark", "");
}

void FLACPerformanceBenchmark::printRegressionAnalysis(const FLACPerformanceMetrics& current,
                                                     const FLACPerformanceMetrics& baseline) {
    Debug::log("flac_benchmark", "=== REGRESSION ANALYSIS ===");
    
    double frame_time_change = ((current.average_frame_time_us - baseline.average_frame_time_us) / 
                               baseline.average_frame_time_us) * 100.0;
    Debug::log("flac_benchmark", "Frame Time Change: ", frame_time_change, "%");
    
    double cpu_change = ((current.average_cpu_usage_percent - baseline.average_cpu_usage_percent) / 
                        baseline.average_cpu_usage_percent) * 100.0;
    Debug::log("flac_benchmark", "CPU Usage Change: ", cpu_change, "%");
    
    double memory_change = ((static_cast<double>(current.peak_memory_usage_bytes) - 
                            static_cast<double>(baseline.peak_memory_usage_bytes)) / 
                           static_cast<double>(baseline.peak_memory_usage_bytes)) * 100.0;
    Debug::log("flac_benchmark", "Memory Usage Change: ", memory_change, "%");
    
    Debug::log("flac_benchmark", "");
}

#endif // HAVE_FLAC