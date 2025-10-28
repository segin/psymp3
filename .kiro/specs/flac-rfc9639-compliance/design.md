# Design Document

## Overview

This design document outlines the architecture for implementing full RFC 9639 compliance in the PsyMP3 FLAC codec and demuxer. The design focuses on rebuilding critical components that are currently broken or non-compliant, while maintaining thread safety, performance, and API compatibility.

The implementation follows a layered approach: fixing core demuxer functionality first, then adding comprehensive RFC 9639 validation, and finally integrating error recovery and optimization features.

## Architecture

### High-Level Component Structure

```
┌─────────────────────────────────────────────────────────────┐
│                    FLAC Implementation                       │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐  │
│  │   FLACDemuxer   │  │   FLACCodec     │  │ RFC9639     │  │
│  │                 │  │                 │  │ Validator   │  │
│  │ • Frame Parsing │  │ • Audio Decode  │  │ • CRC Check │  │
│  │ • Metadata      │  │ • Sample Output │  │ • Format    │  │
│  │ • Seeking       │  │ • Error Handle  │  │ • Compliance│  │
│  └─────────────────┘  └─────────────────┘  └─────────────┘  │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐  │
│  │ Error Recovery  │  │ Frame Index     │  │ Performance │  │
│  │ Manager         │  │ System          │  │ Monitor     │  │
│  └─────────────────┘  └─────────────────┘  └─────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                    IOHandler Interface                      │
└─────────────────────────────────────────────────────────────┘
```

### Threading Architecture

The implementation follows the established public/private lock pattern:

- **Public Methods**: Acquire locks and call private `_unlocked` implementations
- **Private Methods**: Perform actual work without lock acquisition
- **Lock Ordering**: Documented consistent ordering to prevent deadlocks
- **Exception Safety**: RAII lock guards for automatic cleanup

## Components and Interfaces

### 1. Enhanced FLACDemuxer

The demuxer is rebuilt with robust frame boundary detection and metadata parsing:

#### Core Methods

```cpp
class FLACDemuxer {
public:
    // Public API with locking
    bool parseMetadata();
    bool findNextFrame(FLACFrame& frame);
    bool seekToSample(uint64_t sample);
    
private:
    // Private implementations
    bool parseMetadata_unlocked();
    bool findNextFrame_unlocked(FLACFrame& frame);
    bool seekToSample_unlocked(uint64_t sample);
    
    // Frame boundary detection
    bool validateFrameSync_unlocked(const uint8_t* data, size_t size);
    bool searchSyncPattern_unlocked(uint64_t start_pos, uint64_t& found_pos);
    size_t calculateFrameSize_unlocked(const FLACFrame& frame);
    
    // Metadata parsing
    bool parseStreamInfoBlock_unlocked(const uint8_t* data, size_t size);
    bool parseMetadataBlockHeader_unlocked(MetadataBlockHeader& header);
    bool attemptStreamInfoRecovery_unlocked();
    
    // Thread safety
    mutable std::mutex m_demux_mutex;
};
```

#### Frame Boundary Detection Algorithm

1. **Sync Pattern Search**: Efficient byte-by-byte search for 15-bit pattern
2. **Frame Size Estimation**: Conservative estimation using STREAMINFO hints
3. **Boundary Validation**: Verify frame parameters and structure
4. **Fallback Strategies**: Multiple approaches when primary detection fails

### 2. RFC 9639 Compliance Validator

A dedicated component for format validation and compliance checking:

#### Validation Categories

```cpp
class RFC9639Validator {
public:
    // Frame validation
    bool validateFrameSync(const uint8_t* data, size_t size);
    bool validateFrameHeader(const FLACFrameHeader& header);
    bool validateBlockSize(uint32_t block_size, bool is_last_frame);
    
    // CRC validation
    bool validateHeaderCRC8(const uint8_t* data, size_t size);
    bool validateFrameCRC16(const uint8_t* data, size_t size);
    
    // Streamable subset validation
    bool validateStreamableSubset(const FLACFrame& frame);
    
    // Channel assignment validation
    bool validateChannelAssignment(uint8_t assignment, uint8_t channels);
    bool validateStereoDecorrelation(const FLACFrame& frame);
    
private:
    uint8_t calculateHeaderCRC8_unlocked(const uint8_t* data, size_t size);
    uint16_t calculateFrameCRC16_unlocked(const uint8_t* data, size_t size);
    
    // Configuration
    bool m_strict_mode = false;
    bool m_streamable_subset_mode = false;
    bool m_crc_validation_enabled = true;
};
```

#### CRC Calculation Implementation

- **CRC-8 Header**: Polynomial x^8 + x^2 + x^1 + x^0 (0x07)
- **CRC-16 Frame**: Polynomial x^16 + x^15 + x^2 + x^0 (0x8005)
- **Performance Optimized**: Lookup tables for common cases
- **Configurable**: Multiple validation modes (disabled/enabled/strict)

### 3. Error Recovery Manager

Centralized error handling with classification and recovery strategies:

#### Error Classification

```cpp
enum class FLACErrorType {
    IO_ERROR,           // File reading/seeking failures
    FORMAT_ERROR,       // Invalid FLAC format structure
    RFC_VIOLATION,      // Non-compliance with RFC 9639
    RESOURCE_ERROR,     // Memory/resource exhaustion
    LOGIC_ERROR         // Internal consistency failures
};

enum class ErrorSeverity {
    RECOVERABLE,        // Can continue processing
    FRAME_SKIP,         // Skip current frame, continue
    STREAM_RESET,       // Reset to known good state
    FATAL               // Cannot continue processing
};
```

#### Recovery Strategies

```cpp
class ErrorRecoveryManager {
public:
    bool handleError(FLACErrorType type, ErrorSeverity severity, 
                    const std::string& context);
    
    // Specific recovery methods
    bool recoverFromLostSync(const uint8_t* data, size_t size);
    bool recoverFromCorruptedMetadata();
    bool recoverFromCRCMismatch(bool strict_mode);
    
    // Statistics and monitoring
    ErrorStatistics getErrorStats() const;
    void resetErrorStats();
    
private:
    std::vector<ErrorRecord> m_error_history;
    ErrorStatistics m_stats;
    mutable std::mutex m_error_mutex;
};
```

### 4. Frame Index System

Efficient seeking support with memory management:

#### Index Structure

```cpp
struct FrameIndexEntry {
    uint64_t file_offset;      // Byte position in file
    uint64_t sample_offset;    // Sample position in stream
    uint32_t frame_size;       // Frame size in bytes
    uint32_t block_size;       // Samples in this frame
};

class FLACFrameIndex {
public:
    void addFrame(const FrameIndexEntry& entry);
    bool findFrameForSample(uint64_t sample, FrameIndexEntry& entry);
    void clear();
    size_t getMemoryUsage() const;
    
private:
    std::vector<FrameIndexEntry> m_entries;
    size_t m_memory_limit = 1024 * 1024; // 1MB default
    mutable std::mutex m_index_mutex;
};
```

#### Seeking Strategies

1. **SEEKTABLE-based**: Use metadata seek points when available
2. **Frame Index**: Use built frame index for accurate positioning
3. **Binary Search**: Bisection search for approximate positioning
4. **Linear Scan**: Fallback for corrupted or missing index data

### 5. Performance Monitor

Resource usage tracking and optimization:

```cpp
class PerformanceMonitor {
public:
    void recordFrameParseTime(std::chrono::microseconds duration);
    void recordMemoryUsage(size_t bytes);
    void recordSeekOperation(std::chrono::microseconds duration, bool success);
    
    PerformanceStats getStats() const;
    void resetStats();
    
private:
    struct PerformanceStats {
        std::chrono::microseconds avg_frame_parse_time{0};
        size_t peak_memory_usage = 0;
        size_t total_frames_parsed = 0;
        size_t successful_seeks = 0;
        size_t failed_seeks = 0;
    } m_stats;
    
    mutable std::mutex m_perf_mutex;
};
```

## Data Models

### FLAC Frame Structure

```cpp
struct FLACFrame {
    // File positioning
    uint64_t file_offset;           // Byte position in file
    uint64_t sample_offset;         // Sample position in stream
    
    // Frame parameters
    uint32_t block_size;            // Samples in this frame
    uint32_t sample_rate;           // Samples per second
    uint8_t channels;               // Number of audio channels
    uint8_t bits_per_sample;        // Bits per sample
    uint8_t channel_assignment;     // Channel assignment mode
    
    // Frame structure
    bool variable_block_size;       // Blocking strategy
    size_t header_size;             // Header size in bytes
    size_t frame_size;              // Total frame size in bytes
    
    // Validation status
    bool header_crc_valid;          // CRC-8 validation result
    bool frame_crc_valid;           // CRC-16 validation result
    bool rfc_compliant;             // Overall RFC compliance
};
```

### STREAMINFO Metadata

```cpp
struct StreamInfo {
    uint32_t min_block_size;        // Minimum block size
    uint32_t max_block_size;        // Maximum block size
    uint32_t min_frame_size;        // Minimum frame size (0 if unknown)
    uint32_t max_frame_size;        // Maximum frame size (0 if unknown)
    uint32_t sample_rate;           // Sample rate in Hz
    uint8_t channels;               // Number of channels
    uint8_t bits_per_sample;        // Bits per sample
    uint64_t total_samples;         // Total samples (0 if unknown)
    uint8_t md5_signature[16];      // MD5 signature of audio data
    
    bool isValid() const;
    bool isComplete() const;
};
```

### Error Context Information

```cpp
struct ErrorContext {
    FLACErrorType type;
    ErrorSeverity severity;
    std::string message;
    std::string rfc_section;        // RFC 9639 section reference
    uint64_t file_position;         // Byte position where error occurred
    uint64_t sample_position;       // Sample position where error occurred
    std::chrono::steady_clock::time_point timestamp;
    
    // Additional context
    std::map<std::string, std::string> details;
};
```

## Error Handling

### Error Classification Strategy

1. **IO Errors**: File access, network issues, resource exhaustion
2. **Format Errors**: Invalid FLAC structure, malformed headers
3. **RFC Violations**: Non-compliance with specification requirements
4. **Resource Errors**: Memory limits, buffer overflows
5. **Logic Errors**: Internal state inconsistencies

### Recovery Mechanisms

#### Sync Loss Recovery

```cpp
bool FLACDemuxer::recoverFromLostSync_unlocked(const uint8_t* data, size_t size) {
    const size_t MAX_SEARCH_DISTANCE = 64 * 1024; // 64KB search limit
    
    for (size_t offset = 1; offset < std::min(size, MAX_SEARCH_DISTANCE); ++offset) {
        if (validateFrameSync_unlocked(data + offset, size - offset)) {
            // Found potential sync pattern
            m_current_offset += offset;
            return true;
        }
    }
    
    return false; // No sync pattern found within search limit
}
```

#### CRC Error Handling

```cpp
bool FLACCodec::handleCRCError_unlocked(const FLACFrame& frame, bool is_header_crc) {
    if (m_strict_crc_mode) {
        return false; // Reject frame in strict mode
    }
    
    // Log error for monitoring
    m_error_stats.crc_errors++;
    
    // Disable CRC validation if too many errors
    if (m_error_stats.crc_errors > m_crc_error_threshold) {
        m_crc_validation_enabled = false;
    }
    
    return true; // Continue processing despite CRC error
}
```

### Error Reporting

All errors include:
- **RFC Section Reference**: Specific section of RFC 9639 violated
- **File Position**: Byte offset where error occurred
- **Sample Position**: Audio sample position for temporal context
- **Error Context**: Additional debugging information
- **Recovery Action**: What recovery strategy was attempted

## Testing Strategy

### Unit Testing

1. **Frame Boundary Detection**: Test sync pattern validation with various bit patterns
2. **Metadata Parsing**: Test STREAMINFO extraction with different field combinations
3. **CRC Validation**: Test with known test vectors and corrupted data
4. **Error Recovery**: Test recovery mechanisms with intentionally corrupted data

### Integration Testing

1. **Real FLAC Files**: Test with files from various encoders
2. **Seeking Accuracy**: Validate seeking precision across different file types
3. **Performance**: Benchmark against baseline implementation
4. **Thread Safety**: Concurrent access testing

### Compliance Testing

1. **RFC 9639 Examples**: Test with official specification examples
2. **Streamable Subset**: Validate streamable subset constraints
3. **Format Validation**: Test with edge cases and boundary conditions
4. **Error Injection**: Test error recovery with systematic corruption

### Performance Testing

1. **Parsing Speed**: Measure frame parsing performance
2. **Memory Usage**: Monitor memory consumption patterns
3. **Seeking Performance**: Benchmark seeking operations
4. **Thread Contention**: Measure lock contention under load

## Configuration Options

### Validation Modes

```cpp
enum class ValidationMode {
    DISABLED,           // No validation (maximum performance)
    BASIC,              // Essential validation only
    STANDARD,           // Full RFC 9639 compliance
    STRICT              // Strict compliance with no error tolerance
};
```

### Error Recovery Options

```cpp
struct ErrorRecoveryConfig {
    bool enable_sync_recovery = true;
    bool enable_crc_recovery = true;
    bool enable_metadata_recovery = true;
    size_t max_recovery_attempts = 3;
    size_t sync_search_limit = 64 * 1024;
    size_t crc_error_threshold = 10;
};
```

### Performance Tuning

```cpp
struct PerformanceConfig {
    size_t frame_index_memory_limit = 1024 * 1024;  // 1MB
    size_t metadata_memory_limit = 256 * 1024;      // 256KB
    bool enable_frame_caching = true;
    bool enable_performance_monitoring = false;
};
```

This design provides a comprehensive foundation for implementing RFC 9639 compliance while maintaining the performance, thread safety, and API compatibility requirements of the PsyMP3 codebase.