# FLAC RFC 9639 Compliance Guide

## Overview

This guide documents the RFC 9639 compliance features implemented in PsyMP3's FLAC codec and demuxer. RFC 9639 defines the Free Lossless Audio Codec (FLAC) format and its streamable subset, providing the authoritative specification for FLAC implementation.

## Key Features

### 1. Enhanced Frame Boundary Detection

The FLAC demuxer implements robust frame boundary detection per RFC 9639 Section 9.1:

- **15-bit Sync Pattern Validation**: Validates the sync pattern `0b111111111111100` (0x3FFE)
- **Conservative Frame Size Estimation**: Uses STREAMINFO hints to prevent infinite loops
- **Configurable Search Windows**: Prevents excessive I/O operations during sync recovery
- **Fallback Strategies**: Multiple approaches when primary detection fails

#### Usage Example:
```cpp
FLACDemuxer demuxer("audio.flac");
demuxer.parseContainer(); // Automatic frame boundary detection
```

### 2. STREAMINFO Metadata Parsing

Correct bit-field extraction per RFC 9639 Section 6:

- **Sample Rate**: 20-bit field extraction with proper byte order
- **Channel Count**: 3-bit field + 1 with correct bit masking
- **Bits Per Sample**: 5-bit field + 1 from packed fields
- **Total Samples**: 36-bit field across multiple bytes
- **Recovery Mechanisms**: Fallback STREAMINFO derivation from first frame

#### Configuration:
```cpp
// STREAMINFO recovery is automatic, but can be monitored
auto stats = demuxer.getCorruptionRecoveryStats();
if (stats.metadata_recovery_successes > 0) {
    std::cout << "STREAMINFO was recovered from corruption" << std::endl;
}
```

### 3. RFC 9639 Frame Header Parsing

Complete frame header parsing per RFC 9639 Section 9.1:

- **Blocking Strategy**: Variable vs. fixed block size detection
- **Block Size Parsing**: All encoding modes (direct, 8-bit, 16-bit)
- **Sample Rate Parsing**: All encoding modes including custom rates
- **Channel Assignment**: All stereo modes and independent channels
- **Bit Depth Parsing**: All valid bit depths (4-32 bits)
- **UTF-8 Frame Numbers**: 1-7 byte UTF-8 sequence support
- **Reserved Bit Validation**: Ensures reserved bits are zero

#### Advanced Usage:
```cpp
FLACFrame frame;
if (demuxer.findNextFrame(frame)) {
    std::cout << "Block size: " << frame.block_size << std::endl;
    std::cout << "Sample rate: " << frame.sample_rate << std::endl;
    std::cout << "Channels: " << frame.channels << std::endl;
    std::cout << "Bit depth: " << frame.bits_per_sample << std::endl;
}
```

### 4. CRC Validation

RFC 9639 compliant CRC validation per Sections 9.1.8 and 9.3:

- **CRC-8 Header Validation**: Polynomial x^8 + x^2 + x^1 + x^0 (0x07)
- **CRC-16 Frame Validation**: Polynomial x^16 + x^15 + x^2 + x^0 (0x8005)
- **Configurable Modes**: Disabled, enabled, or strict validation
- **Performance Optimization**: Lookup tables for common cases

#### Configuration:
```cpp
// Configure CRC validation mode
demuxer.setCRCValidationMode(FLACDemuxer::CRCValidationMode::STRICT);

// Set error threshold for automatic disabling
demuxer.setCRCErrorThreshold(5); // Disable after 5 errors

// Monitor CRC validation statistics
auto crc_stats = demuxer.getCRCValidationStats();
std::cout << "CRC-8 errors: " << crc_stats.crc8_errors << std::endl;
std::cout << "CRC-16 errors: " << crc_stats.crc16_errors << std::endl;
```

### 5. Streamable Subset Validation

RFC 9639 Section 7 streamable subset compliance:

- **Frame Header Independence**: No STREAMINFO references for sample rate/bit depth
- **Block Size Constraints**: Maximum 16384 samples, 4608 for ≤48kHz streams
- **Sample Rate Encoding**: Must be encoded in frame headers
- **Bit Depth Encoding**: Must be encoded in frame headers
- **Configurable Enforcement**: Disabled, enabled, or strict modes

#### Configuration:
```cpp
// Enable streamable subset validation
demuxer.setStreamableSubsetMode(FLACDemuxer::StreamableSubsetMode::ENABLED);

// Monitor streamable subset compliance
auto subset_stats = demuxer.getStreamableSubsetStats();
std::cout << "Frames validated: " << subset_stats.frames_validated << std::endl;
std::cout << "Violations detected: " << subset_stats.violations_detected << std::endl;
std::cout << "Violation rate: " << subset_stats.getViolationRate() << "%" << std::endl;
```

### 6. Error Recovery Framework

Comprehensive error recovery per RFC 9639 error handling guidelines:

- **Sync Loss Recovery**: Configurable search limits for sync pattern recovery
- **Corruption Detection**: CRC validation and format checks
- **Frame Skipping**: Skip corrupted frames and continue processing
- **Metadata Recovery**: Block-level recovery with non-critical block skipping
- **Statistics and Monitoring**: Detailed error tracking and reporting

#### Configuration:
```cpp
// Configure error recovery options
FLACDemuxer::ErrorRecoveryConfig config;
config.enable_sync_recovery = true;
config.enable_crc_recovery = true;
config.enable_metadata_recovery = true;
config.max_recovery_attempts = 3;
config.sync_search_limit_bytes = 64 * 1024; // 64KB search limit
config.error_rate_threshold = 10.0; // 10% error rate threshold

demuxer.setErrorRecoveryConfig(config);

// Monitor error recovery
auto recovery_stats = demuxer.getSyncRecoveryStats();
std::cout << "Sync recovery success rate: " << recovery_stats.getSuccessRate() << "%" << std::endl;
```

## Performance Tuning

### CRC Validation Performance

CRC validation adds ~5-10% CPU overhead. For performance-critical applications:

```cpp
// Disable CRC validation for trusted sources
demuxer.setCRCValidationMode(FLACDemuxer::CRCValidationMode::DISABLED);

// Or use automatic disabling with threshold
demuxer.setCRCErrorThreshold(10); // Auto-disable after 10 errors
```

### Memory Usage Optimization

Configure memory limits for large files:

```cpp
// Set memory limit for frame indexing (default 64MB)
demuxer.setFrameIndexingEnabled(true);
// Memory usage is automatically managed within limits
```

### Error Recovery Tuning

Adjust error recovery for different use cases:

```cpp
FLACDemuxer::ErrorRecoveryConfig config;

// For high-quality sources (reduce recovery overhead)
config.max_recovery_attempts = 1;
config.sync_search_limit_bytes = 16 * 1024; // 16KB limit

// For corrupted sources (increase recovery capability)
config.max_recovery_attempts = 5;
config.sync_search_limit_bytes = 128 * 1024; // 128KB limit
config.error_rate_threshold = 20.0; // Higher tolerance

demuxer.setErrorRecoveryConfig(config);
```

## Troubleshooting

### Common Issues and Solutions

#### 1. CRC Validation Failures

**Symptoms**: High CRC error counts, automatic validation disabling

**Causes**:
- Corrupted FLAC files
- Non-compliant FLAC encoders
- I/O errors during reading
- Network transmission errors

**Solutions**:
```cpp
// Check CRC validation statistics
auto crc_stats = demuxer.getCRCValidationStats();
if (crc_stats.validation_disabled_due_to_errors) {
    std::cout << "CRC validation disabled due to errors" << std::endl;
    std::cout << "Total errors: " << crc_stats.total_errors << std::endl;
    
    // Reset and re-enable if needed
    demuxer.resetCRCValidationStats();
}

// Use permissive mode for corrupted files
demuxer.setCRCValidationMode(FLACDemuxer::CRCValidationMode::ENABLED);
```

#### 2. Frame Boundary Detection Issues

**Symptoms**: Parsing failures, infinite loops, incorrect frame sizes

**Causes**:
- Corrupted sync patterns
- Invalid frame headers
- Truncated files

**Solutions**:
```cpp
// Monitor sync recovery
auto sync_stats = demuxer.getSyncRecoveryStats();
if (sync_stats.sync_loss_count > 0) {
    std::cout << "Sync losses detected: " << sync_stats.sync_loss_count << std::endl;
    std::cout << "Recovery success rate: " << sync_stats.getSuccessRate() << "%" << std::endl;
}

// Adjust sync search limits
FLACDemuxer::ErrorRecoveryConfig config = demuxer.getErrorRecoveryConfig();
config.sync_search_limit_bytes = 128 * 1024; // Increase search limit
demuxer.setErrorRecoveryConfig(config);
```

#### 3. Streamable Subset Violations

**Symptoms**: Streamable subset validation failures

**Causes**:
- Non-streamable FLAC files
- Large block sizes
- Frame header dependencies on STREAMINFO

**Solutions**:
```cpp
// Check streamable subset compliance
auto subset_stats = demuxer.getStreamableSubsetStats();
if (subset_stats.violations_detected > 0) {
    std::cout << "Streamable subset violations:" << std::endl;
    std::cout << "  Block size violations: " << subset_stats.block_size_violations << std::endl;
    std::cout << "  Header dependency violations: " << subset_stats.frame_header_dependency_violations << std::endl;
    
    // Use permissive mode for non-streamable files
    demuxer.setStreamableSubsetMode(FLACDemuxer::StreamableSubsetMode::ENABLED);
}
```

#### 4. Performance Issues

**Symptoms**: High CPU usage, slow parsing, memory usage

**Causes**:
- CRC validation overhead
- Excessive error recovery attempts
- Large frame indexes

**Solutions**:
```cpp
// Optimize for performance
demuxer.setCRCValidationMode(FLACDemuxer::CRCValidationMode::DISABLED);

FLACDemuxer::ErrorRecoveryConfig config;
config.max_recovery_attempts = 1; // Reduce recovery attempts
config.log_recovery_attempts = false; // Reduce logging overhead
demuxer.setErrorRecoveryConfig(config);

// Monitor performance impact
auto perf_stats = demuxer.getPerformanceStats();
std::cout << "Average parse time: " << perf_stats.getAverageParseTimeUs() << " μs" << std::endl;
```

## Integration Examples

### Basic FLAC Playback

```cpp
#include "FLACDemuxer.h"
#include "FLACCodec.h"

// Create demuxer with RFC 9639 compliance
FLACDemuxer demuxer("audio.flac");
if (!demuxer.parseContainer()) {
    std::cerr << "Failed to parse FLAC container" << std::endl;
    return false;
}

// Get stream information
auto streams = demuxer.getStreams();
if (streams.empty()) {
    std::cerr << "No audio streams found" << std::endl;
    return false;
}

// Create codec
FLACCodec codec(streams[0]);
if (!codec.initialize()) {
    std::cerr << "Failed to initialize FLAC codec" << std::endl;
    return false;
}

// Configure RFC 9639 compliance
demuxer.setCRCValidationMode(FLACDemuxer::CRCValidationMode::ENABLED);
codec.setCRCValidationEnabled(true);

// Decode audio
while (!demuxer.isEOF()) {
    MediaChunk chunk = demuxer.readChunk();
    if (chunk.data.empty()) break;
    
    AudioFrame frame = codec.decode(chunk);
    if (frame.getSampleFrameCount() > 0) {
        // Process decoded audio samples
        processAudio(frame.getSamples());
    }
}

// Get final statistics
auto crc_stats = demuxer.getCRCValidationStats();
auto codec_stats = codec.getStats();
std::cout << "Decoding completed:" << std::endl;
std::cout << "  CRC errors: " << crc_stats.total_errors << std::endl;
std::cout << "  Frames decoded: " << codec_stats.frames_decoded << std::endl;
std::cout << "  Samples decoded: " << codec_stats.samples_decoded << std::endl;
```

### Streaming FLAC with Streamable Subset

```cpp
// Configure for streaming with streamable subset validation
FLACDemuxer demuxer("stream.flac");
demuxer.setStreamableSubsetMode(FLACDemuxer::StreamableSubsetMode::STRICT);
demuxer.setCRCValidationMode(FLACDemuxer::CRCValidationMode::ENABLED);

// Configure error recovery for streaming
FLACDemuxer::ErrorRecoveryConfig config;
config.enable_sync_recovery = true;
config.max_recovery_attempts = 2; // Quick recovery for real-time
config.sync_search_limit_bytes = 32 * 1024; // Smaller search window
config.error_rate_threshold = 5.0; // Lower tolerance for streaming
demuxer.setErrorRecoveryConfig(config);

if (!demuxer.parseContainer()) {
    std::cerr << "Failed to parse FLAC container" << std::endl;
    return false;
}

// Verify streamable subset compliance
auto subset_stats = demuxer.getStreamableSubsetStats();
if (subset_stats.violations_detected > 0) {
    std::cerr << "Warning: FLAC file is not streamable subset compliant" << std::endl;
    std::cerr << "Violations: " << subset_stats.violations_detected << std::endl;
}

// Continue with streaming playback...
```

## API Reference

### FLACDemuxer Configuration Methods

#### CRC Validation
- `setCRCValidationMode(CRCValidationMode mode)` - Set CRC validation mode
- `getCRCValidationMode()` - Get current CRC validation mode
- `setCRCErrorThreshold(size_t threshold)` - Set error threshold for auto-disable
- `getCRCValidationStats()` - Get CRC validation statistics
- `resetCRCValidationStats()` - Reset CRC statistics and re-enable validation

#### Streamable Subset Validation
- `setStreamableSubsetMode(StreamableSubsetMode mode)` - Set streamable subset mode
- `getStreamableSubsetMode()` - Get current streamable subset mode
- `getStreamableSubsetStats()` - Get streamable subset statistics
- `resetStreamableSubsetStats()` - Reset streamable subset statistics

#### Error Recovery Configuration
- `setErrorRecoveryConfig(const ErrorRecoveryConfig& config)` - Set error recovery options
- `getErrorRecoveryConfig()` - Get current error recovery configuration
- `resetErrorRecoveryConfig()` - Reset error recovery to defaults
- `getSyncRecoveryStats()` - Get sync recovery statistics
- `getCorruptionRecoveryStats()` - Get corruption recovery statistics

### FLACCodec Configuration Methods

#### CRC Validation
- `setCRCValidationEnabled(bool enabled)` - Enable/disable CRC validation
- `getCRCValidationEnabled()` - Check if CRC validation is enabled
- `setCRCValidationStrict(bool strict)` - Set strict/permissive CRC mode
- `getCRCValidationStrict()` - Check if strict CRC mode is enabled
- `setCRCErrorThreshold(size_t threshold)` - Set CRC error threshold
- `getCRCErrorCount()` - Get current CRC error count

#### Statistics and Monitoring
- `getStats()` - Get comprehensive codec statistics
- `getCurrentSample()` - Get current sample position
- `supportsSeekReset()` - Check if codec supports seek reset

## Best Practices

### 1. Production Deployment

```cpp
// Recommended settings for production
demuxer.setCRCValidationMode(FLACDemuxer::CRCValidationMode::ENABLED);
demuxer.setCRCErrorThreshold(10); // Auto-disable after 10 errors
demuxer.setStreamableSubsetMode(FLACDemuxer::StreamableSubsetMode::ENABLED);

FLACDemuxer::ErrorRecoveryConfig config;
config.enable_sync_recovery = true;
config.enable_crc_recovery = true;
config.enable_metadata_recovery = true;
config.max_recovery_attempts = 3;
config.log_recovery_attempts = false; // Reduce log noise in production
demuxer.setErrorRecoveryConfig(config);
```

### 2. Development and Testing

```cpp
// Recommended settings for development
demuxer.setCRCValidationMode(FLACDemuxer::CRCValidationMode::STRICT);
demuxer.setStreamableSubsetMode(FLACDemuxer::StreamableSubsetMode::STRICT);

FLACDemuxer::ErrorRecoveryConfig config;
config.log_recovery_attempts = true; // Enable detailed logging
config.strict_rfc_compliance = true; // Strict RFC compliance checking
demuxer.setErrorRecoveryConfig(config);
```

### 3. Performance-Critical Applications

```cpp
// Optimized settings for performance
demuxer.setCRCValidationMode(FLACDemuxer::CRCValidationMode::DISABLED);
demuxer.setStreamableSubsetMode(FLACDemuxer::StreamableSubsetMode::DISABLED);

FLACDemuxer::ErrorRecoveryConfig config;
config.max_recovery_attempts = 1;
config.log_recovery_attempts = false;
config.sync_search_limit_bytes = 16 * 1024; // Smaller search window
demuxer.setErrorRecoveryConfig(config);
```

## Conclusion

The RFC 9639 compliance implementation in PsyMP3 provides comprehensive FLAC format support with robust error recovery, configurable validation, and performance optimization options. The implementation follows the official FLAC specification closely while providing practical features for real-world deployment scenarios.

For additional information, consult:
- RFC 9639 specification (`docs/rfc9639.txt`)
- RFC 9639 summary (`docs/RFC9639_FLAC_SUMMARY.md`)
- FLAC demuxer API documentation (`docs/flac-demuxer-api.md`)
- FLAC codec developer guide (`docs/flac-codec-developer-guide.md`)