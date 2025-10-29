# FLAC Troubleshooting Guide

## Overview

This guide provides comprehensive troubleshooting information for FLAC parsing and decoding issues in PsyMP3. It covers common problems, diagnostic techniques, and solutions for RFC 9639 compliance issues.

## Diagnostic Tools

### 1. Debug Logging

Enable FLAC-specific debug channels for detailed diagnostics:

```bash
# Enable FLAC-specific logging
./psymp3 --debug=flac,flac_codec,flac_rfc_validator,demux "problem.flac"

# Enable comprehensive logging
./psymp3 --debug=all --logfile=flac_debug.log "problem.flac"

# Focus on specific issues
./psymp3 --debug=flac_rfc_validator,compliance "problem.flac"
```

### 2. Statistics Monitoring

Use built-in statistics to identify issues:

```cpp
// Monitor CRC validation
auto crc_stats = demuxer.getCRCValidationStats();
std::cout << "CRC-8 errors: " << crc_stats.crc8_errors << std::endl;
std::cout << "CRC-16 errors: " << crc_stats.crc16_errors << std::endl;
std::cout << "Validation disabled: " << crc_stats.validation_disabled_due_to_errors << std::endl;

// Monitor sync recovery
auto sync_stats = demuxer.getSyncRecoveryStats();
std::cout << "Sync losses: " << sync_stats.sync_loss_count << std::endl;
std::cout << "Recovery success rate: " << sync_stats.getSuccessRate() << "%" << std::endl;

// Monitor streamable subset compliance
auto subset_stats = demuxer.getStreamableSubsetStats();
std::cout << "Violation rate: " << subset_stats.getViolationRate() << "%" << std::endl;
```

### 3. Performance Analysis

Monitor performance characteristics:

```cpp
auto codec_stats = codec.getStats();
std::cout << "Average decode time: " << codec_stats.getAverageDecodeTimeUs() << " μs" << std::endl;
std::cout << "Decode efficiency: " << codec_stats.getDecodeEfficiency() << " samples/sec" << std::endl;
std::cout << "Error rate: " << codec_stats.getErrorRate() << "%" << std::endl;
```

## Common Issues and Solutions

### 1. File Won't Parse

#### Symptoms
- `parseContainer()` returns false
- "Failed to parse FLAC container" errors
- No streams detected

#### Diagnostic Steps
```cpp
FLACDemuxer demuxer("problem.flac");

// Check if file exists and is readable
if (!demuxer.getIOHandler()) {
    std::cerr << "Cannot open file or file doesn't exist" << std::endl;
    return;
}

// Enable detailed logging
demuxer.setCRCValidationMode(FLACDemuxer::CRCValidationMode::ENABLED);

// Try parsing with error recovery
FLACDemuxer::ErrorRecoveryConfig config;
config.enable_metadata_recovery = true;
config.log_recovery_attempts = true;
demuxer.setErrorRecoveryConfig(config);

if (!demuxer.parseContainer()) {
    auto recovery_stats = demuxer.getCorruptionRecoveryStats();
    std::cout << "Metadata corruption count: " << recovery_stats.metadata_corruption_count << std::endl;
    std::cout << "Recovery attempts: " << recovery_stats.metadata_recovery_successes + recovery_stats.metadata_recovery_failures << std::endl;
}
```

#### Common Causes and Solutions

**Invalid FLAC Signature**
```
Error: Invalid FLAC stream signature
Solution: Verify file is actually FLAC format, not misnamed
```

**Corrupted STREAMINFO Block**
```
Error: Failed to parse STREAMINFO metadata block
Solution: Enable metadata recovery
```
```cpp
FLACDemuxer::ErrorRecoveryConfig config;
config.enable_metadata_recovery = true;
demuxer.setErrorRecoveryConfig(config);
```

**Truncated File**
```
Error: Unexpected end of file during metadata parsing
Solution: Check file integrity, re-download if necessary
```

### 2. CRC Validation Failures

#### Symptoms
- High CRC error counts
- CRC validation automatically disabled
- "CRC mismatch" warnings in logs

#### Diagnostic Steps
```cpp
// Check CRC validation status
auto crc_stats = demuxer.getCRCValidationStats();
if (crc_stats.validation_disabled_due_to_errors) {
    std::cout << "CRC validation disabled due to " << crc_stats.total_errors << " errors" << std::endl;
    
    // Analyze error distribution
    std::cout << "CRC-8 (header) errors: " << crc_stats.crc8_errors << std::endl;
    std::cout << "CRC-16 (frame) errors: " << crc_stats.crc16_errors << std::endl;
}
```

#### Solutions by Error Type

**Systematic CRC-8 Errors (Header Corruption)**
```cpp
// Indicates frame header corruption
// Solution: Use permissive mode and enable sync recovery
demuxer.setCRCValidationMode(FLACDemuxer::CRCValidationMode::ENABLED);

FLACDemuxer::ErrorRecoveryConfig config;
config.enable_sync_recovery = true;
config.max_recovery_attempts = 5;
demuxer.setErrorRecoveryConfig(config);
```

**Systematic CRC-16 Errors (Frame Corruption)**
```cpp
// Indicates frame data corruption
// Solution: Enable frame skipping
FLACDemuxer::ErrorRecoveryConfig config;
config.enable_frame_skipping = true;
config.enable_crc_recovery = true;
demuxer.setErrorRecoveryConfig(config);
```

**Occasional CRC Errors**
```cpp
// Normal for some files, increase threshold
demuxer.setCRCErrorThreshold(50); // Allow more errors before disabling
```

**All Frames Have CRC Errors**
```cpp
// Likely non-compliant encoder or file corruption
// Solution: Disable CRC validation
demuxer.setCRCValidationMode(FLACDemuxer::CRCValidationMode::DISABLED);
```

### 3. Frame Boundary Detection Issues

#### Symptoms
- Parsing hangs or takes very long
- "Lost sync" errors
- Incorrect frame sizes
- Audio dropouts

#### Diagnostic Steps
```cpp
// Monitor sync recovery
auto sync_stats = demuxer.getSyncRecoveryStats();
if (sync_stats.sync_loss_count > 0) {
    std::cout << "Sync losses detected: " << sync_stats.sync_loss_count << std::endl;
    std::cout << "Total search bytes: " << sync_stats.total_sync_search_bytes << std::endl;
    std::cout << "Last recovery offset: 0x" << std::hex << sync_stats.last_sync_recovery_offset << std::endl;
}
```

#### Solutions

**Frequent Sync Loss**
```cpp
// Increase sync search limit
FLACDemuxer::ErrorRecoveryConfig config;
config.sync_search_limit_bytes = 128 * 1024; // 128KB search window
config.max_recovery_attempts = 5;
demuxer.setErrorRecoveryConfig(config);
```

**Infinite Loop During Parsing**
```cpp
// Reduce search limit to prevent infinite loops
FLACDemuxer::ErrorRecoveryConfig config;
config.sync_search_limit_bytes = 32 * 1024; // 32KB limit
config.max_recovery_attempts = 2;
demuxer.setErrorRecoveryConfig(config);
```

**Corrupted Frame Headers**
```cpp
// Enable comprehensive error recovery
FLACDemuxer::ErrorRecoveryConfig config;
config.enable_sync_recovery = true;
config.enable_frame_skipping = true;
config.enable_crc_recovery = true;
demuxer.setErrorRecoveryConfig(config);
```

### 4. Streamable Subset Violations

#### Symptoms
- Streamable subset validation failures
- "Block size exceeds streamable subset limit" warnings
- "Frame header references STREAMINFO" errors

#### Diagnostic Steps
```cpp
auto subset_stats = demuxer.getStreamableSubsetStats();
std::cout << "Streamable subset analysis:" << std::endl;
std::cout << "  Frames validated: " << subset_stats.frames_validated << std::endl;
std::cout << "  Total violations: " << subset_stats.violations_detected << std::endl;
std::cout << "  Block size violations: " << subset_stats.block_size_violations << std::endl;
std::cout << "  Header dependency violations: " << subset_stats.frame_header_dependency_violations << std::endl;
std::cout << "  Sample rate violations: " << subset_stats.sample_rate_encoding_violations << std::endl;
std::cout << "  Bit depth violations: " << subset_stats.bit_depth_encoding_violations << std::endl;
```

#### Solutions by Violation Type

**Block Size Violations**
```
Cause: Block sizes > 16384 samples or > 4608 for ≤48kHz streams
Solution: File is not streamable subset compliant
```
```cpp
// Use permissive mode for non-streamable files
demuxer.setStreamableSubsetMode(FLACDemuxer::StreamableSubsetMode::ENABLED);
```

**Frame Header Dependencies**
```
Cause: Frame headers reference STREAMINFO for sample rate or bit depth
Solution: File violates streamable subset independence requirement
```
```cpp
// Disable streamable subset validation for such files
demuxer.setStreamableSubsetMode(FLACDemuxer::StreamableSubsetMode::DISABLED);
```

**Sample Rate/Bit Depth Encoding Violations**
```
Cause: Sample rate or bit depth not encoded in frame headers
Solution: File requires STREAMINFO for decoding (not streamable)
```

### 5. Performance Issues

#### Symptoms
- Slow parsing or decoding
- High CPU usage
- Memory usage growth
- Audio dropouts in real-time playback

#### Diagnostic Steps
```cpp
auto codec_stats = codec.getStats();
std::cout << "Performance analysis:" << std::endl;
std::cout << "  Average decode time: " << codec_stats.getAverageDecodeTimeUs() << " μs" << std::endl;
std::cout << "  Max decode time: " << codec_stats.max_frame_decode_time_us << " μs" << std::endl;
std::cout << "  Decode efficiency: " << codec_stats.getDecodeEfficiency() << " samples/sec" << std::endl;
std::cout << "  Memory usage: " << codec_stats.memory_usage_bytes << " bytes" << std::endl;
```

#### Solutions

**High CRC Validation Overhead**
```cpp
// Disable CRC validation for performance
demuxer.setCRCValidationMode(FLACDemuxer::CRCValidationMode::DISABLED);
codec.setCRCValidationEnabled(false);
```

**Excessive Error Recovery**
```cpp
// Reduce error recovery overhead
FLACDemuxer::ErrorRecoveryConfig config;
config.max_recovery_attempts = 1;
config.log_recovery_attempts = false;
config.sync_search_limit_bytes = 16 * 1024; // Smaller search window
demuxer.setErrorRecoveryConfig(config);
```

**Memory Usage Issues**
```cpp
// Monitor and limit memory usage
auto codec_stats = codec.getStats();
if (codec_stats.memory_usage_bytes > 100 * 1024 * 1024) { // 100MB
    std::cout << "High memory usage detected" << std::endl;
    // Consider resetting codec periodically
    codec.reset();
}
```

### 6. Seeking Issues

#### Symptoms
- Seeking fails or produces incorrect results
- Audio artifacts after seeking
- "Seek failed" errors

#### Diagnostic Steps
```cpp
// Check if seeking is supported
if (!codec.supportsSeekReset()) {
    std::cout << "Codec doesn't support seeking" << std::endl;
}

// Monitor seeking accuracy
uint64_t target_sample = 44100 * 30; // 30 seconds at 44.1kHz
if (demuxer.seekTo(target_sample * 1000 / 44100)) { // Convert to milliseconds
    uint64_t actual_sample = demuxer.getCurrentSample();
    int64_t error_samples = static_cast<int64_t>(actual_sample) - static_cast<int64_t>(target_sample);
    std::cout << "Seek error: " << error_samples << " samples" << std::endl;
}
```

#### Solutions

**Inaccurate Seeking**
```cpp
// Enable frame indexing for better accuracy
demuxer.setFrameIndexingEnabled(true);
demuxer.buildFrameIndex(); // Build index upfront
```

**Seeking Artifacts**
```cpp
// Reset codec after seeking to clear state
demuxer.seekTo(target_ms);
codec.reset(); // Clear decoder state
```

**No SEEKTABLE Available**
```cpp
// Check if SEEKTABLE is available
auto streams = demuxer.getStreams();
if (!streams.empty()) {
    auto stream_info = streams[0];
    if (stream_info.has_seektable) {
        std::cout << "SEEKTABLE available for seeking" << std::endl;
    } else {
        std::cout << "No SEEKTABLE - seeking will be approximate" << std::endl;
    }
}
```

## Error Code Reference

### FLACDemuxer Error Codes

| Error | Description | Solution |
|-------|-------------|----------|
| `INVALID_FLAC_SIGNATURE` | File doesn't start with "fLaC" | Verify file format |
| `STREAMINFO_PARSE_ERROR` | Cannot parse STREAMINFO block | Enable metadata recovery |
| `FRAME_SYNC_LOST` | Cannot find frame sync pattern | Enable sync recovery |
| `CRC_VALIDATION_FAILED` | CRC mismatch detected | Use permissive CRC mode |
| `STREAMABLE_SUBSET_VIOLATION` | Violates streamable subset rules | Disable streamable subset validation |
| `METADATA_CORRUPTION` | Corrupted metadata block | Enable metadata recovery |
| `FRAME_CORRUPTION` | Corrupted frame data | Enable frame skipping |
| `SEEK_FAILED` | Seeking operation failed | Check SEEKTABLE availability |

### FLACCodec Error Codes

| Error | Description | Solution |
|-------|-------------|----------|
| `INITIALIZATION_FAILED` | Codec initialization failed | Check StreamInfo parameters |
| `DECODE_ERROR` | Frame decoding failed | Enable error recovery |
| `MEMORY_ALLOCATION_ERROR` | Memory allocation failed | Check available memory |
| `INVALID_FRAME_DATA` | Invalid frame data received | Check demuxer output |
| `CRC_MISMATCH` | Frame CRC validation failed | Use permissive CRC mode |
| `UNSUPPORTED_FORMAT` | Unsupported FLAC variant | Check RFC 9639 compliance |

## Advanced Troubleshooting

### 1. Binary Analysis

For severe corruption, analyze the file at binary level:

```bash
# Check FLAC signature
hexdump -C problem.flac | head -1
# Should show: 66 4c 61 43 (fLaC)

# Find frame sync patterns
hexdump -C problem.flac | grep "ff f8\|ff f9\|ff fa\|ff fb"
```

### 2. Comparison with Reference Implementation

Compare behavior with reference libFLAC:

```bash
# Test with flac command-line tool
flac -t problem.flac  # Test integrity
flac -d problem.flac  # Decode to WAV
```

### 3. Network Stream Issues

For network streams, consider:

```cpp
// Increase buffer sizes for network streams
FLACDemuxer::ErrorRecoveryConfig config;
config.sync_search_limit_bytes = 256 * 1024; // Larger buffer for network
config.max_recovery_attempts = 10; // More attempts for network issues
demuxer.setErrorRecoveryConfig(config);
```

### 4. Multi-threaded Issues

For multi-threaded applications:

```cpp
// Verify thread safety
auto thread_validation = demuxer.validateThreadSafety();
if (!thread_validation.isValid()) {
    std::cout << "Thread safety issues detected:" << std::endl;
    for (const auto& violation : thread_validation.violations) {
        std::cout << "  " << violation << std::endl;
    }
}
```

## Prevention Strategies

### 1. Input Validation

Always validate inputs before processing:

```cpp
// Validate file before processing
if (!std::filesystem::exists(filename)) {
    std::cerr << "File does not exist: " << filename << std::endl;
    return false;
}

auto file_size = std::filesystem::file_size(filename);
if (file_size < 42) { // Minimum FLAC file size
    std::cerr << "File too small to be valid FLAC: " << file_size << " bytes" << std::endl;
    return false;
}
```

### 2. Graceful Degradation

Implement fallback strategies:

```cpp
// Try strict mode first, fall back to permissive
demuxer.setCRCValidationMode(FLACDemuxer::CRCValidationMode::STRICT);
demuxer.setStreamableSubsetMode(FLACDemuxer::StreamableSubsetMode::STRICT);

if (!demuxer.parseContainer()) {
    // Fall back to permissive mode
    demuxer.setCRCValidationMode(FLACDemuxer::CRCValidationMode::ENABLED);
    demuxer.setStreamableSubsetMode(FLACDemuxer::StreamableSubsetMode::ENABLED);
    
    if (!demuxer.parseContainer()) {
        // Final fallback - disable validation
        demuxer.setCRCValidationMode(FLACDemuxer::CRCValidationMode::DISABLED);
        demuxer.setStreamableSubsetMode(FLACDemuxer::StreamableSubsetMode::DISABLED);
        
        return demuxer.parseContainer();
    }
}
```

### 3. Monitoring and Alerting

Implement monitoring for production systems:

```cpp
class FLACHealthMonitor {
public:
    void checkHealth(const FLACDemuxer& demuxer, const FLACCodec& codec) {
        auto crc_stats = demuxer.getCRCValidationStats();
        auto codec_stats = codec.getStats();
        
        // Alert on high error rates
        if (codec_stats.getErrorRate() > 5.0) {
            alert("High FLAC error rate: " + std::to_string(codec_stats.getErrorRate()) + "%");
        }
        
        // Alert on CRC validation disabled
        if (crc_stats.validation_disabled_due_to_errors) {
            alert("CRC validation disabled due to errors");
        }
        
        // Alert on poor performance
        if (codec_stats.getAverageDecodeTimeUs() > 1000) { // >1ms per frame
            alert("Poor FLAC decode performance: " + std::to_string(codec_stats.getAverageDecodeTimeUs()) + " μs/frame");
        }
    }
    
private:
    void alert(const std::string& message) {
        // Implement alerting mechanism
        std::cerr << "ALERT: " << message << std::endl;
    }
};
```

## Getting Help

### 1. Debug Information Collection

When reporting issues, collect:

```cpp
// Collect comprehensive debug information
std::ostringstream debug_info;
debug_info << "FLAC Debug Information\n";
debug_info << "=====================\n";

// File information
debug_info << "File: " << filename << "\n";
debug_info << "Size: " << std::filesystem::file_size(filename) << " bytes\n";

// Demuxer statistics
auto crc_stats = demuxer.getCRCValidationStats();
debug_info << "CRC-8 errors: " << crc_stats.crc8_errors << "\n";
debug_info << "CRC-16 errors: " << crc_stats.crc16_errors << "\n";

auto sync_stats = demuxer.getSyncRecoveryStats();
debug_info << "Sync losses: " << sync_stats.sync_loss_count << "\n";
debug_info << "Sync recovery rate: " << sync_stats.getSuccessRate() << "%\n";

// Codec statistics
auto codec_stats = codec.getStats();
debug_info << "Frames decoded: " << codec_stats.frames_decoded << "\n";
debug_info << "Average decode time: " << codec_stats.getAverageDecodeTimeUs() << " μs\n";
debug_info << "Error rate: " << codec_stats.getErrorRate() << "%\n";

std::cout << debug_info.str() << std::endl;
```

### 2. Log File Analysis

Enable comprehensive logging:

```bash
# Generate detailed log file
./psymp3 --debug=all --logfile=flac_issue.log "problem.flac" 2>&1

# Analyze log for patterns
grep -i "error\|fail\|corrupt" flac_issue.log
grep -i "crc\|sync\|recovery" flac_issue.log
```

### 3. Minimal Reproduction Case

Create minimal test case:

```cpp
// Minimal reproduction case
int main() {
    try {
        FLACDemuxer demuxer("problem.flac");
        
        // Enable all debugging
        demuxer.setCRCValidationMode(FLACDemuxer::CRCValidationMode::STRICT);
        demuxer.setStreamableSubsetMode(FLACDemuxer::StreamableSubsetMode::STRICT);
        
        FLACDemuxer::ErrorRecoveryConfig config;
        config.log_recovery_attempts = true;
        demuxer.setErrorRecoveryConfig(config);
        
        if (!demuxer.parseContainer()) {
            std::cerr << "Parse failed" << std::endl;
            return 1;
        }
        
        // Report success
        std::cout << "Parse succeeded" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
```

This troubleshooting guide should help identify and resolve most FLAC-related issues in PsyMP3. For additional support, consult the RFC 9639 specification and the comprehensive API documentation.