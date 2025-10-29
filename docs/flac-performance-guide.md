# FLAC Performance Tuning and Optimization Guide

## Overview

This guide provides comprehensive performance tuning recommendations for PsyMP3's RFC 9639 compliant FLAC implementation. It covers optimization strategies for different use cases, from high-throughput batch processing to real-time streaming applications.

## Performance Characteristics

### Baseline Performance Targets

| Use Case | Target Performance | Configuration |
|----------|-------------------|---------------|
| Real-time Playback | <100μs per frame decode | CRC disabled, minimal recovery |
| High-Quality Archival | <500μs per frame decode | CRC enabled, full validation |
| Batch Processing | >10x real-time | Optimized for throughput |
| Streaming | <50μs per frame decode | Streamable subset, fast recovery |

### Performance Monitoring

Monitor key performance metrics:

```cpp
// Get comprehensive performance statistics
auto codec_stats = codec.getStats();
std::cout << "Performance Metrics:" << std::endl;
std::cout << "  Average decode time: " << codec_stats.getAverageDecodeTimeUs() << " μs" << std::endl;
std::cout << "  Max decode time: " << codec_stats.max_frame_decode_time_us << " μs" << std::endl;
std::cout << "  Min decode time: " << codec_stats.min_frame_decode_time_us << " μs" << std::endl;
std::cout << "  Decode efficiency: " << codec_stats.getDecodeEfficiency() << " samples/sec" << std::endl;
std::cout << "  Memory usage: " << codec_stats.memory_usage_bytes << " bytes" << std::endl;
std::cout << "  Error rate: " << codec_stats.getErrorRate() << "%" << std::endl;

// Check if performance targets are met
bool real_time_capable = codec_stats.getAverageDecodeTimeUs() < 100;
std::cout << "Real-time capable: " << (real_time_capable ? "YES" : "NO") << std::endl;
```

## Optimization Strategies

### 1. CRC Validation Optimization

CRC validation adds 5-10% CPU overhead. Optimize based on use case:

#### Disable for Performance-Critical Applications
```cpp
// Maximum performance - disable all CRC validation
demuxer.setCRCValidationMode(FLACDemuxer::CRCValidationMode::DISABLED);
codec.setCRCValidationEnabled(false);

// Expected performance gain: 5-10% CPU reduction
```

#### Selective CRC Validation
```cpp
// Enable only header CRC for basic integrity checking
demuxer.setCRCValidationMode(FLACDemuxer::CRCValidationMode::ENABLED);
codec.setCRCValidationEnabled(false); // Disable codec-level CRC

// Compromise between performance and integrity
```

#### Adaptive CRC Validation
```cpp
// Start with CRC enabled, disable if errors are excessive
demuxer.setCRCValidationMode(FLACDemuxer::CRCValidationMode::ENABLED);
demuxer.setCRCErrorThreshold(5); // Auto-disable after 5 errors

// Monitor and adjust threshold based on file quality
auto crc_stats = demuxer.getCRCValidationStats();
if (crc_stats.validation_disabled_due_to_errors) {
    std::cout << "CRC validation auto-disabled for performance" << std::endl;
}
```

### 2. Error Recovery Optimization

Error recovery can significantly impact performance with corrupted files:

#### Minimal Error Recovery (High Performance)
```cpp
FLACDemuxer::ErrorRecoveryConfig config;
config.enable_sync_recovery = true;
config.enable_crc_recovery = false;
config.enable_metadata_recovery = false;
config.enable_frame_skipping = true;
config.max_recovery_attempts = 1; // Single attempt only
config.sync_search_limit_bytes = 16 * 1024; // 16KB search limit
config.log_recovery_attempts = false; // Reduce logging overhead
demuxer.setErrorRecoveryConfig(config);

// Expected performance gain: 20-50% with corrupted files
```

#### Balanced Error Recovery
```cpp
FLACDemuxer::ErrorRecoveryConfig config;
config.enable_sync_recovery = true;
config.enable_crc_recovery = true;
config.enable_metadata_recovery = true;
config.max_recovery_attempts = 2; // Limited attempts
config.sync_search_limit_bytes = 32 * 1024; // 32KB search limit
config.error_rate_threshold = 5.0; // Disable recovery at 5% error rate
demuxer.setErrorRecoveryConfig(config);
```

#### Comprehensive Error Recovery (Quality Priority)
```cpp
FLACDemuxer::ErrorRecoveryConfig config;
config.enable_sync_recovery = true;
config.enable_crc_recovery = true;
config.enable_metadata_recovery = true;
config.enable_frame_skipping = true;
config.max_recovery_attempts = 5; // Multiple attempts
config.sync_search_limit_bytes = 128 * 1024; // 128KB search limit
config.strict_rfc_compliance = true; // Full RFC compliance
demuxer.setErrorRecoveryConfig(config);
```

### 3. Memory Management Optimization

Efficient memory usage improves cache performance and reduces allocation overhead:

#### Pre-allocation Strategy
```cpp
// Pre-allocate buffers for known stream characteristics
auto streams = demuxer.getStreams();
if (!streams.empty()) {
    auto stream_info = streams[0];
    
    // Calculate expected buffer sizes
    size_t max_frame_samples = stream_info.max_block_size * stream_info.channels;
    size_t buffer_size = max_frame_samples * sizeof(int16_t);
    
    std::cout << "Pre-allocating " << buffer_size << " bytes for audio buffers" << std::endl;
    
    // Codec will use this information for buffer pre-allocation
    FLACCodec codec(stream_info);
    codec.initialize();
}
```

#### Memory Pool Usage
```cpp
// Monitor memory usage patterns
auto codec_stats = codec.getStats();
std::cout << "Peak memory usage: " << codec_stats.memory_usage_bytes << " bytes" << std::endl;

// Reset codec periodically to prevent memory fragmentation
static size_t frame_count = 0;
if (++frame_count % 10000 == 0) { // Every 10,000 frames
    codec.reset(); // Clear internal buffers
    std::cout << "Codec reset for memory optimization" << std::endl;
}
```

### 4. Frame Indexing Optimization

Frame indexing improves seeking performance but uses memory:

#### Selective Frame Indexing
```cpp
// Enable indexing only for files that will be seeked
bool will_seek = true; // Based on application logic
demuxer.setFrameIndexingEnabled(will_seek);

if (will_seek) {
    // Build index upfront for better seeking performance
    demuxer.buildFrameIndex();
    
    auto index_stats = demuxer.getFrameIndexStats();
    std::cout << "Frame index built: " << index_stats.total_frames << " frames, "
              << index_stats.memory_usage_bytes << " bytes" << std::endl;
}
```

#### Lazy Frame Indexing
```cpp
// Build index during playback for memory efficiency
demuxer.setFrameIndexingEnabled(true);
// Don't call buildFrameIndex() - let it build during playback

// Index will be built incrementally as frames are parsed
```

### 5. Streamable Subset Optimization

Streamable subset validation adds overhead but ensures streaming compatibility:

#### Disable for Non-Streaming Applications
```cpp
// Disable streamable subset validation for local file playback
demuxer.setStreamableSubsetMode(FLACDemuxer::StreamableSubsetMode::DISABLED);

// Expected performance gain: 2-5% CPU reduction
```

#### Optimize for Streaming
```cpp
// Enable streamable subset validation for streaming applications
demuxer.setStreamableSubsetMode(FLACDemuxer::StreamableSubsetMode::ENABLED);

// Monitor compliance
auto subset_stats = demuxer.getStreamableSubsetStats();
if (subset_stats.getViolationRate() > 10.0) {
    std::cout << "Warning: High streamable subset violation rate: " 
              << subset_stats.getViolationRate() << "%" << std::endl;
}
```

## Use Case Specific Optimizations

### 1. Real-Time Audio Playback

Optimize for consistent low-latency performance:

```cpp
class RealTimeOptimizedFLAC {
public:
    RealTimeOptimizedFLAC(const std::string& filename) {
        // Initialize demuxer with performance settings
        demuxer = std::make_unique<FLACDemuxer>(filename);
        
        // Disable all validation for maximum performance
        demuxer->setCRCValidationMode(FLACDemuxer::CRCValidationMode::DISABLED);
        demuxer->setStreamableSubsetMode(FLACDemuxer::StreamableSubsetMode::DISABLED);
        
        // Minimal error recovery
        FLACDemuxer::ErrorRecoveryConfig config;
        config.enable_sync_recovery = true;
        config.enable_crc_recovery = false;
        config.enable_metadata_recovery = false;
        config.max_recovery_attempts = 1;
        config.sync_search_limit_bytes = 8 * 1024; // 8KB limit
        config.log_recovery_attempts = false;
        demuxer->setErrorRecoveryConfig(config);
        
        // Disable frame indexing for sequential playback
        demuxer->setFrameIndexingEnabled(false);
        
        // Parse container
        if (!demuxer->parseContainer()) {
            throw std::runtime_error("Failed to parse FLAC container");
        }
        
        // Initialize codec with performance settings
        auto streams = demuxer->getStreams();
        if (streams.empty()) {
            throw std::runtime_error("No audio streams found");
        }
        
        codec = std::make_unique<FLACCodec>(streams[0]);
        codec->setCRCValidationEnabled(false);
        codec->setCRCValidationStrict(false);
        
        if (!codec->initialize()) {
            throw std::runtime_error("Failed to initialize FLAC codec");
        }
    }
    
    AudioFrame decodeNextFrame() {
        MediaChunk chunk = demuxer->readChunk();
        if (chunk.data.empty()) {
            return AudioFrame(); // End of stream
        }
        
        return codec->decode(chunk);
    }
    
    void monitorPerformance() {
        auto stats = codec->getStats();
        if (stats.getAverageDecodeTimeUs() > 100) {
            std::cout << "Warning: Decode time exceeds real-time target: " 
                      << stats.getAverageDecodeTimeUs() << " μs" << std::endl;
        }
    }
    
private:
    std::unique_ptr<FLACDemuxer> demuxer;
    std::unique_ptr<FLACCodec> codec;
};
```

### 2. High-Quality Archival Processing

Optimize for accuracy and validation:

```cpp
class ArchivalQualityFLAC {
public:
    ArchivalQualityFLAC(const std::string& filename) {
        demuxer = std::make_unique<FLACDemuxer>(filename);
        
        // Enable all validation for maximum quality assurance
        demuxer->setCRCValidationMode(FLACDemuxer::CRCValidationMode::STRICT);
        demuxer->setStreamableSubsetMode(FLACDemuxer::StreamableSubsetMode::ENABLED);
        
        // Comprehensive error recovery
        FLACDemuxer::ErrorRecoveryConfig config;
        config.enable_sync_recovery = true;
        config.enable_crc_recovery = true;
        config.enable_metadata_recovery = true;
        config.enable_frame_skipping = false; // Don't skip frames
        config.max_recovery_attempts = 5;
        config.sync_search_limit_bytes = 256 * 1024; // 256KB search
        config.strict_rfc_compliance = true;
        config.log_recovery_attempts = true;
        demuxer->setErrorRecoveryConfig(config);
        
        // Enable frame indexing for seeking capability
        demuxer->setFrameIndexingEnabled(true);
        
        if (!demuxer->parseContainer()) {
            throw std::runtime_error("Failed to parse FLAC container");
        }
        
        auto streams = demuxer->getStreams();
        if (streams.empty()) {
            throw std::runtime_error("No audio streams found");
        }
        
        codec = std::make_unique<FLACCodec>(streams[0]);
        codec->setCRCValidationEnabled(true);
        codec->setCRCValidationStrict(true);
        codec->setCRCErrorThreshold(0); // Never auto-disable
        
        if (!codec->initialize()) {
            throw std::runtime_error("Failed to initialize FLAC codec");
        }
    }
    
    bool validateIntegrity() {
        // Decode entire file to validate integrity
        size_t total_frames = 0;
        size_t error_frames = 0;
        
        while (!demuxer->isEOF()) {
            MediaChunk chunk = demuxer->readChunk();
            if (chunk.data.empty()) break;
            
            AudioFrame frame = codec->decode(chunk);
            total_frames++;
            
            if (frame.getSampleFrameCount() == 0) {
                error_frames++;
            }
        }
        
        // Report validation results
        auto crc_stats = demuxer->getCRCValidationStats();
        auto codec_stats = codec->getStats();
        
        std::cout << "Integrity Validation Results:" << std::endl;
        std::cout << "  Total frames: " << total_frames << std::endl;
        std::cout << "  Error frames: " << error_frames << std::endl;
        std::cout << "  CRC-8 errors: " << crc_stats.crc8_errors << std::endl;
        std::cout << "  CRC-16 errors: " << crc_stats.crc16_errors << std::endl;
        std::cout << "  Codec error rate: " << codec_stats.getErrorRate() << "%" << std::endl;
        
        return error_frames == 0 && crc_stats.total_errors == 0;
    }
    
private:
    std::unique_ptr<FLACDemuxer> demuxer;
    std::unique_ptr<FLACCodec> codec;
};
```

### 3. Batch Processing

Optimize for maximum throughput:

```cpp
class BatchProcessingFLAC {
public:
    static void processFiles(const std::vector<std::string>& filenames) {
        for (const auto& filename : filenames) {
            auto start_time = std::chrono::high_resolution_clock::now();
            
            try {
                processFile(filename);
            } catch (const std::exception& e) {
                std::cerr << "Error processing " << filename << ": " << e.what() << std::endl;
                continue;
            }
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            std::cout << "Processed " << filename << " in " << duration.count() << " ms" << std::endl;
        }
    }
    
private:
    static void processFile(const std::string& filename) {
        FLACDemuxer demuxer(filename);
        
        // Optimize for throughput
        demuxer.setCRCValidationMode(FLACDemuxer::CRCValidationMode::DISABLED);
        demuxer.setStreamableSubsetMode(FLACDemuxer::StreamableSubsetMode::DISABLED);
        
        // Minimal error recovery for speed
        FLACDemuxer::ErrorRecoveryConfig config;
        config.enable_sync_recovery = true;
        config.enable_crc_recovery = false;
        config.enable_metadata_recovery = false;
        config.max_recovery_attempts = 1;
        config.log_recovery_attempts = false;
        demuxer.setErrorRecoveryConfig(config);
        
        // Disable frame indexing for sequential processing
        demuxer.setFrameIndexingEnabled(false);
        
        if (!demuxer.parseContainer()) {
            throw std::runtime_error("Failed to parse container");
        }
        
        auto streams = demuxer.getStreams();
        if (streams.empty()) {
            throw std::runtime_error("No audio streams");
        }
        
        FLACCodec codec(streams[0]);
        codec.setCRCValidationEnabled(false);
        
        if (!codec.initialize()) {
            throw std::runtime_error("Failed to initialize codec");
        }
        
        // Process all frames
        size_t total_samples = 0;
        while (!demuxer.isEOF()) {
            MediaChunk chunk = demuxer.readChunk();
            if (chunk.data.empty()) break;
            
            AudioFrame frame = codec.decode(chunk);
            total_samples += frame.getSampleFrameCount();
            
            // Process decoded audio (e.g., write to file, analyze, etc.)
            processAudioFrame(frame);
        }
        
        std::cout << "Processed " << total_samples << " samples" << std::endl;
    }
    
    static void processAudioFrame(const AudioFrame& frame) {
        // Placeholder for audio processing
        // In real implementation, this would write to file, analyze, etc.
    }
};
```

### 4. Network Streaming

Optimize for network streaming with error resilience:

```cpp
class NetworkStreamingFLAC {
public:
    NetworkStreamingFLAC(const std::string& stream_url) {
        // Initialize with network-optimized settings
        demuxer = std::make_unique<FLACDemuxer>(stream_url);
        
        // Enable streamable subset validation for streaming
        demuxer->setStreamableSubsetMode(FLACDemuxer::StreamableSubsetMode::ENABLED);
        
        // Balanced CRC validation (header only for performance)
        demuxer->setCRCValidationMode(FLACDemuxer::CRCValidationMode::ENABLED);
        
        // Network-optimized error recovery
        FLACDemuxer::ErrorRecoveryConfig config;
        config.enable_sync_recovery = true;
        config.enable_crc_recovery = true;
        config.enable_metadata_recovery = true;
        config.enable_frame_skipping = true; // Skip corrupted frames
        config.max_recovery_attempts = 3;
        config.sync_search_limit_bytes = 64 * 1024; // 64KB for network
        config.error_rate_threshold = 15.0; // Higher tolerance for network
        config.log_recovery_attempts = false; // Reduce logging for streaming
        demuxer->setErrorRecoveryConfig(config);
        
        // Disable frame indexing for streaming (sequential access)
        demuxer->setFrameIndexingEnabled(false);
        
        if (!demuxer->parseContainer()) {
            throw std::runtime_error("Failed to parse FLAC stream");
        }
        
        auto streams = demuxer->getStreams();
        if (streams.empty()) {
            throw std::runtime_error("No audio streams in stream");
        }
        
        codec = std::make_unique<FLACCodec>(streams[0]);
        codec->setCRCValidationEnabled(true);
        codec->setCRCValidationStrict(false); // Permissive for network
        codec->setCRCErrorThreshold(20); // Higher threshold for network
        
        if (!codec->initialize()) {
            throw std::runtime_error("Failed to initialize FLAC codec");
        }
    }
    
    AudioFrame getNextFrame() {
        try {
            MediaChunk chunk = demuxer->readChunk();
            if (chunk.data.empty()) {
                return AudioFrame(); // End of stream
            }
            
            return codec->decode(chunk);
            
        } catch (const std::exception& e) {
            // Handle network errors gracefully
            std::cerr << "Network error: " << e.what() << std::endl;
            
            // Return silence frame to maintain audio continuity
            return createSilenceFrame();
        }
    }
    
    void monitorNetworkHealth() {
        auto recovery_stats = demuxer->getSyncRecoveryStats();
        auto crc_stats = demuxer->getCRCValidationStats();
        
        // Monitor network-related issues
        if (recovery_stats.sync_loss_count > 10) {
            std::cout << "Warning: High sync loss count (network issues?): " 
                      << recovery_stats.sync_loss_count << std::endl;
        }
        
        if (crc_stats.total_errors > 50) {
            std::cout << "Warning: High CRC error count (network corruption?): " 
                      << crc_stats.total_errors << std::endl;
        }
        
        // Check streamable subset compliance
        auto subset_stats = demuxer->getStreamableSubsetStats();
        if (subset_stats.getViolationRate() > 5.0) {
            std::cout << "Warning: Stream may not be optimized for streaming: " 
                      << subset_stats.getViolationRate() << "% violations" << std::endl;
        }
    }
    
private:
    AudioFrame createSilenceFrame() {
        // Create silence frame for error recovery
        std::vector<int16_t> silence(1024 * 2, 0); // 1024 samples, stereo
        return AudioFrame(std::move(silence), 44100, 2);
    }
    
    std::unique_ptr<FLACDemuxer> demuxer;
    std::unique_ptr<FLACCodec> codec;
};
```

## Performance Measurement and Profiling

### 1. Built-in Performance Monitoring

Use built-in statistics for performance analysis:

```cpp
class PerformanceProfiler {
public:
    void profileDecoding(FLACDemuxer& demuxer, FLACCodec& codec) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        size_t frames_processed = 0;
        size_t samples_processed = 0;
        
        while (!demuxer.isEOF()) {
            auto frame_start = std::chrono::high_resolution_clock::now();
            
            MediaChunk chunk = demuxer.readChunk();
            if (chunk.data.empty()) break;
            
            AudioFrame frame = codec.decode(chunk);
            
            auto frame_end = std::chrono::high_resolution_clock::now();
            auto frame_duration = std::chrono::duration_cast<std::chrono::microseconds>(frame_end - frame_start);
            
            frames_processed++;
            samples_processed += frame.getSampleFrameCount();
            
            // Track worst-case performance
            if (frame_duration.count() > worst_frame_time_us) {
                worst_frame_time_us = frame_duration.count();
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // Calculate performance metrics
        double frames_per_second = static_cast<double>(frames_processed) * 1000.0 / total_duration.count();
        double samples_per_second = static_cast<double>(samples_processed) * 1000.0 / total_duration.count();
        
        // Get codec statistics
        auto codec_stats = codec.getStats();
        
        std::cout << "Performance Profile Results:" << std::endl;
        std::cout << "  Total time: " << total_duration.count() << " ms" << std::endl;
        std::cout << "  Frames processed: " << frames_processed << std::endl;
        std::cout << "  Samples processed: " << samples_processed << std::endl;
        std::cout << "  Frames per second: " << frames_per_second << std::endl;
        std::cout << "  Samples per second: " << samples_per_second << std::endl;
        std::cout << "  Average decode time: " << codec_stats.getAverageDecodeTimeUs() << " μs" << std::endl;
        std::cout << "  Worst frame time: " << worst_frame_time_us << " μs" << std::endl;
        std::cout << "  Real-time factor: " << calculateRealTimeFactor(samples_per_second, 44100) << "x" << std::endl;
    }
    
private:
    uint64_t worst_frame_time_us = 0;
    
    double calculateRealTimeFactor(double samples_per_second, uint32_t sample_rate) {
        return samples_per_second / sample_rate;
    }
};
```

### 2. External Profiling Tools

Use external tools for detailed analysis:

```bash
# CPU profiling with perf
perf record -g ./psymp3 "test.flac"
perf report

# Memory profiling with valgrind
valgrind --tool=massif ./psymp3 "test.flac"
ms_print massif.out.* > memory_profile.txt

# Cache analysis
perf stat -e cache-references,cache-misses,instructions,cycles ./psymp3 "test.flac"
```

### 3. Benchmark Suite

Create comprehensive benchmarks:

```cpp
class FLACBenchmarkSuite {
public:
    void runBenchmarks() {
        std::vector<std::string> test_files = {
            "test_44k_16bit_stereo.flac",
            "test_96k_24bit_stereo.flac",
            "test_192k_32bit_stereo.flac",
            "test_44k_16bit_mono.flac",
            "test_44k_16bit_5.1.flac"
        };
        
        for (const auto& file : test_files) {
            benchmarkFile(file);
        }
    }
    
private:
    void benchmarkFile(const std::string& filename) {
        std::cout << "Benchmarking: " << filename << std::endl;
        
        // Test different configurations
        benchmarkConfiguration(filename, "High Performance", createHighPerformanceConfig());
        benchmarkConfiguration(filename, "Balanced", createBalancedConfig());
        benchmarkConfiguration(filename, "High Quality", createHighQualityConfig());
        
        std::cout << std::endl;
    }
    
    void benchmarkConfiguration(const std::string& filename, const std::string& config_name, 
                               const BenchmarkConfig& config) {
        try {
            FLACDemuxer demuxer(filename);
            
            // Apply configuration
            demuxer.setCRCValidationMode(config.crc_mode);
            demuxer.setStreamableSubsetMode(config.subset_mode);
            demuxer.setErrorRecoveryConfig(config.recovery_config);
            
            if (!demuxer.parseContainer()) {
                std::cout << "  " << config_name << ": PARSE FAILED" << std::endl;
                return;
            }
            
            auto streams = demuxer.getStreams();
            if (streams.empty()) {
                std::cout << "  " << config_name << ": NO STREAMS" << std::endl;
                return;
            }
            
            FLACCodec codec(streams[0]);
            codec.setCRCValidationEnabled(config.codec_crc_enabled);
            
            if (!codec.initialize()) {
                std::cout << "  " << config_name << ": CODEC INIT FAILED" << std::endl;
                return;
            }
            
            // Benchmark decoding
            auto start_time = std::chrono::high_resolution_clock::now();
            
            size_t total_samples = 0;
            while (!demuxer.isEOF()) {
                MediaChunk chunk = demuxer.readChunk();
                if (chunk.data.empty()) break;
                
                AudioFrame frame = codec.decode(chunk);
                total_samples += frame.getSampleFrameCount();
            }
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            
            // Calculate metrics
            auto codec_stats = codec.getStats();
            double real_time_factor = calculateRealTimeFactor(total_samples, duration.count(), streams[0].sample_rate);
            
            std::cout << "  " << config_name << ":" << std::endl;
            std::cout << "    Time: " << duration.count() << " μs" << std::endl;
            std::cout << "    Samples: " << total_samples << std::endl;
            std::cout << "    Real-time factor: " << real_time_factor << "x" << std::endl;
            std::cout << "    Avg decode time: " << codec_stats.getAverageDecodeTimeUs() << " μs" << std::endl;
            std::cout << "    Error rate: " << codec_stats.getErrorRate() << "%" << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "  " << config_name << ": EXCEPTION - " << e.what() << std::endl;
        }
    }
    
    struct BenchmarkConfig {
        FLACDemuxer::CRCValidationMode crc_mode;
        FLACDemuxer::StreamableSubsetMode subset_mode;
        FLACDemuxer::ErrorRecoveryConfig recovery_config;
        bool codec_crc_enabled;
    };
    
    BenchmarkConfig createHighPerformanceConfig() {
        BenchmarkConfig config;
        config.crc_mode = FLACDemuxer::CRCValidationMode::DISABLED;
        config.subset_mode = FLACDemuxer::StreamableSubsetMode::DISABLED;
        config.codec_crc_enabled = false;
        
        config.recovery_config.enable_sync_recovery = true;
        config.recovery_config.enable_crc_recovery = false;
        config.recovery_config.enable_metadata_recovery = false;
        config.recovery_config.max_recovery_attempts = 1;
        config.recovery_config.log_recovery_attempts = false;
        
        return config;
    }
    
    BenchmarkConfig createBalancedConfig() {
        BenchmarkConfig config;
        config.crc_mode = FLACDemuxer::CRCValidationMode::ENABLED;
        config.subset_mode = FLACDemuxer::StreamableSubsetMode::ENABLED;
        config.codec_crc_enabled = true;
        
        config.recovery_config.enable_sync_recovery = true;
        config.recovery_config.enable_crc_recovery = true;
        config.recovery_config.enable_metadata_recovery = true;
        config.recovery_config.max_recovery_attempts = 2;
        config.recovery_config.log_recovery_attempts = false;
        
        return config;
    }
    
    BenchmarkConfig createHighQualityConfig() {
        BenchmarkConfig config;
        config.crc_mode = FLACDemuxer::CRCValidationMode::STRICT;
        config.subset_mode = FLACDemuxer::StreamableSubsetMode::STRICT;
        config.codec_crc_enabled = true;
        
        config.recovery_config.enable_sync_recovery = true;
        config.recovery_config.enable_crc_recovery = true;
        config.recovery_config.enable_metadata_recovery = true;
        config.recovery_config.enable_frame_skipping = false;
        config.recovery_config.max_recovery_attempts = 5;
        config.recovery_config.strict_rfc_compliance = true;
        config.recovery_config.log_recovery_attempts = true;
        
        return config;
    }
    
    double calculateRealTimeFactor(size_t samples, uint64_t time_us, uint32_t sample_rate) {
        double audio_duration_us = static_cast<double>(samples) * 1000000.0 / sample_rate;
        return audio_duration_us / time_us;
    }
};
```

## Conclusion

Performance optimization of FLAC decoding requires balancing multiple factors:

1. **Validation Overhead**: CRC validation and streamable subset checking add CPU overhead
2. **Error Recovery**: Comprehensive error recovery improves reliability but reduces performance
3. **Memory Usage**: Frame indexing and buffer management affect memory usage and cache performance
4. **Use Case Requirements**: Different applications have different performance vs. quality trade-offs

The key is to profile your specific use case and adjust the configuration accordingly. Use the built-in statistics and monitoring capabilities to identify bottlenecks and optimize performance while maintaining the required level of quality and reliability.

For most applications, the "Balanced" configuration provides a good compromise between performance and quality. For performance-critical real-time applications, consider the "High Performance" configuration. For archival and quality-critical applications, use the "High Quality" configuration.

Remember to validate your optimizations with real-world test files and usage patterns, as performance characteristics can vary significantly based on the specific FLAC files being processed.