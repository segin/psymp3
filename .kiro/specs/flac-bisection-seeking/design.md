# FLAC Bisection Seeking Design

## Overview

This design document specifies the implementation of bisection-based seeking for FLAC files without SEEKTABLE metadata, with full RFC 9639 compliance. The algorithm estimates byte positions based on time ratios, then iteratively refines using binary search until within 250ms tolerance.

**RFC 9639 Compliance:**
- Frame sync detection per Section 9.1 (15-bit pattern 0b111111111111100)
- CRC-8 validation per Section 9.1.8 (polynomial 0x07)
- Coded number parsing per Section 9.1.5 (UTF-8-like encoding)
- Blocking strategy interpretation per Section 9.1 (fixed vs variable)

## Architecture

### Algorithm Flow

```
1. Calculate initial byte estimate: pos = audio_offset + (target/total) * audio_size
2. Seek to estimated position
3. Search forward for valid frame (sync + CRC-8)
4. Parse coded number to get actual sample position
5. Calculate time differential
6. If within 250ms: DONE
7. If actual < target: search upper half
8. If actual > target: search lower half
9. Repeat from step 2 (max 10 iterations)
```


### Bisection State

```cpp
struct BisectionState {
    uint64_t target_sample;      // Target sample position
    uint64_t low_pos;            // Lower bound (byte position)
    uint64_t high_pos;           // Upper bound (byte position)
    uint64_t best_pos;           // Best position found so far
    uint64_t best_sample;        // Sample at best position
    int64_t best_diff;           // Best time differential (ms)
    int iteration;               // Current iteration count
    static constexpr int MAX_ITERATIONS = 10;
    static constexpr int64_t TOLERANCE_MS = 250;
};
```

## Components and Interfaces

### 1. Byte Position Estimator

**Purpose:** Calculate initial byte position from sample ratio

```cpp
uint64_t estimateBytePosition(uint64_t target_sample, uint64_t total_samples,
                              uint64_t audio_offset, uint64_t file_size) {
    if (total_samples == 0) return audio_offset;
    
    uint64_t audio_size = file_size - audio_offset;
    double ratio = static_cast<double>(target_sample) / total_samples;
    uint64_t estimated = audio_offset + static_cast<uint64_t>(ratio * audio_size);
    
    // Clamp to valid range
    if (estimated < audio_offset) return audio_offset;
    if (estimated >= file_size) return file_size - 64; // Leave room for frame
    return estimated;
}
```

### 2. Frame Finder (RFC 9639 Section 9.1)

**Purpose:** Find valid FLAC frame at or after given position with CRC-8 validation

**RFC Compliance:**
- Sync pattern: 15-bit 0b111111111111100 (bytes 0xFF followed by 0xF8 or 0xF9)
- CRC-8: Polynomial 0x07, covers header bytes excluding CRC byte itself
- Blocking strategy: Bit 0 of second sync byte (0=fixed, 1=variable)

```cpp
bool findFrameAtPosition(uint64_t start_pos, uint64_t& frame_pos, 
                         uint64_t& frame_sample, uint32_t& block_size) {
    // Seek to start position
    // Read buffer (e.g., 64KB)
    // Search for sync pattern 0xFF followed by 0xF8 or 0xF9
    // For each candidate:
    //   1. Parse frame header
    //   2. Calculate CRC-8 over header bytes
    //   3. Compare with CRC-8 byte in header
    //   4. If match: parse coded number, return true
    //   5. If mismatch: continue searching (false positive)
    // Return false if no valid frame found
}
```

### 3. Coded Number Parser (RFC 9639 Section 9.1.5)

**Purpose:** Extract frame/sample number from UTF-8-like encoding

**RFC Compliance:**
- 1-7 byte encoding based on leading bits
- Fixed block size: coded number = frame number
- Variable block size: coded number = sample number

```cpp
bool parseCodedNumber(const uint8_t* data, size_t size, 
                      uint64_t& number, size_t& bytes_consumed) {
    // Determine byte count from leading bits of first byte
    // Extract value bits from each byte
    // Combine into final number
}
```


### 4. Bisection Controller

**Purpose:** Orchestrate iterative refinement until convergence

```cpp
bool seekWithByteEstimation_unlocked(uint64_t target_sample) {
    BisectionState state;
    state.target_sample = target_sample;
    state.low_pos = m_audio_data_offset;
    state.high_pos = m_file_size;
    state.best_diff = INT64_MAX;
    state.iteration = 0;
    
    while (state.iteration < BisectionState::MAX_ITERATIONS) {
        // Calculate midpoint estimate
        uint64_t mid_pos = estimateBytePosition(...);
        
        // Find frame at position
        uint64_t frame_pos, frame_sample;
        uint32_t block_size;
        if (!findFrameAtPosition(mid_pos, frame_pos, frame_sample, block_size)) {
            // Narrow search range and retry
            continue;
        }
        
        // Calculate time differential
        int64_t sample_diff = static_cast<int64_t>(frame_sample) - 
                              static_cast<int64_t>(target_sample);
        int64_t time_diff_ms = (std::abs(sample_diff) * 1000) / m_streaminfo.sample_rate;
        
        // Update best if closer
        if (time_diff_ms < std::abs(state.best_diff)) {
            state.best_pos = frame_pos;
            state.best_sample = frame_sample;
            state.best_diff = time_diff_ms;
        }
        
        // Check convergence
        if (time_diff_ms <= BisectionState::TOLERANCE_MS) {
            break; // Within tolerance
        }
        
        // Adjust search range
        if (frame_sample < target_sample) {
            state.low_pos = frame_pos + block_size; // Search upper half
        } else {
            state.high_pos = frame_pos; // Search lower half
        }
        
        // Check if range collapsed
        if (state.high_pos <= state.low_pos + 64) {
            break; // Can't refine further
        }
        
        state.iteration++;
    }
    
    // Seek to best position found
    return seekToFramePosition(state.best_pos, state.best_sample);
}
```

## Data Models

### Search Range Tracking

```cpp
struct SearchRange {
    uint64_t low;   // Lower bound (inclusive)
    uint64_t high;  // Upper bound (exclusive)
    
    uint64_t midpoint() const { return low + (high - low) / 2; }
    uint64_t size() const { return high - low; }
    bool collapsed() const { return size() < 64; }
};
```


## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system-essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Property 1: Byte Position Estimation Correctness
*For any* valid target sample, total samples, audio offset, and file size, the estimated byte position SHALL equal `audio_offset + (target_sample / total_samples) * (file_size - audio_offset)`, clamped to the valid range `[audio_offset, file_size - 64]`.
**Validates: Requirements 1.1, 1.2, 1.4, 1.5**

### Property 2: Frame Sync Detection (RFC 9639 Section 9.1)
*For any* byte buffer containing a valid FLAC frame sync pattern (0xFF followed by 0xF8 or 0xF9), the frame finder SHALL locate the sync pattern and return its position.
**Validates: Requirements 2.1, 2.2**

### Property 3: CRC-8 Validation (RFC 9639 Section 9.1.8)
*For any* FLAC frame header, the CRC-8 calculated using polynomial 0x07 over header bytes (excluding CRC byte) SHALL match the CRC byte in the header for valid frames, and SHALL NOT match for corrupted frames.
**Validates: Requirements 2.3, 2.8**

### Property 4: Coded Number Round-Trip (RFC 9639 Section 9.1.5)
*For any* valid frame/sample number, encoding it using UTF-8-like encoding and then decoding SHALL produce the original number.
**Validates: Requirements 2.4**

### Property 5: Sample Position Calculation
*For any* fixed block size stream with frame number N and block size B, the sample position SHALL equal N * B. *For any* variable block size stream, the sample position SHALL equal the coded number directly.
**Validates: Requirements 2.5, 2.6**

### Property 6: Bisection Range Adjustment
*For any* bisection iteration where actual sample < target sample, the new search range SHALL have low bound >= current position. *For any* iteration where actual sample > target sample, the new search range SHALL have high bound <= current position.
**Validates: Requirements 3.1, 3.2**

### Property 7: Convergence Termination
*For any* bisection search, the algorithm SHALL terminate when: (a) time differential <= 250ms, OR (b) iteration count > 10, OR (c) search range < 64 bytes, OR (d) same position found twice consecutively.
**Validates: Requirements 3.3, 3.4, 3.5, 3.6**

### Property 8: Time Differential Calculation
*For any* actual sample and target sample at sample rate R, the time differential in milliseconds SHALL equal `abs(actual - target) * 1000 / R`.
**Validates: Requirements 4.1, 4.2**

### Property 9: Best Position Selection
*For any* bisection search that exceeds tolerance, the final position SHALL be the one with minimum time differential found during all iterations. When two positions have equal differential, prefer the one before target.
**Validates: Requirements 4.3, 4.4**

### Property 10: Strategy Priority
*For any* seek operation, the FLAC Demuxer SHALL use strategies in order: (1) SEEKTABLE if available, (2) frame index if target is indexed, (3) bisection estimation, (4) fallback to beginning.
**Validates: Requirements 7.1, 7.2, 7.3, 7.5**


## Error Handling

### I/O Errors
- Save current position before seeking
- On I/O error, restore saved position
- Report error through existing error reporting mechanism

### Frame Discovery Failures
- If no frame found within 64KB, narrow search range
- After 3 consecutive failures, fall back to beginning
- Log each failure with position for debugging

### CRC Validation Failures
- Continue searching past false positive sync patterns
- Track number of CRC failures for diagnostics
- Do not count CRC failures against iteration limit

## Testing Strategy

### Property-Based Testing
- Use fast-check or similar PBT library
- Generate random FLAC-like data structures
- Verify all correctness properties hold

### Unit Tests
- Test byte estimation formula with known values
- Test CRC-8 calculation against RFC 9639 test vectors
- Test coded number parsing with edge cases (1-byte, 7-byte)

### Integration Tests
- Test with real FLAC files without SEEKTABLEs
- Verify seeks land within 250ms of target
- Measure iteration counts for various file types

## Performance Considerations

- Buffer 64KB at a time to minimize I/O operations
- Cache discovered frame positions in frame index
- Use frame index entries to narrow initial search range
- Typical convergence: 3-5 iterations for most files

## Integration Points

### With Existing Seeking Code
- `seekTo_unlocked()` calls `seekWithByteEstimation_unlocked()` when SEEKTABLE and frame index unavailable
- `seekWithByteEstimation_unlocked()` uses existing `findNextFrame_unlocked()` for frame discovery
- Discovered frames added to `m_frame_index` via `addFrameToIndex_unlocked()`

### With Frame Parsing
- Reuse existing `parseFrameHeader_unlocked()` for header parsing
- Reuse existing `validateFrameHeaderCRC_unlocked()` for CRC validation
- Reuse existing `parseCodedNumber_unlocked()` for sample position extraction

