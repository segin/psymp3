# FLAC Bisection Seeking Requirements

## Introduction

This specification defines the requirements for implementing a bisection-based seeking strategy for FLAC files that lack SEEKTABLE metadata blocks, with full compliance to RFC 9639 (FLAC specification). The current implementation falls back to seeking to the beginning of the file when no SEEKTABLE is available, which provides a poor user experience compared to other media players like VLC, foobar2000, and ffmpeg-based players.

The bisection seeking strategy estimates byte positions based on the ratio of target time to total duration, then iteratively refines the position using binary search until the actual position is within 250ms of the desired target. This approach provides acceptable seeking accuracy for most use cases while maintaining reasonable performance.

**RFC 9639 Compliance Requirements:**
- Frame sync detection MUST use the 15-bit pattern per RFC 9639 Section 9.1 (0xFFF8 for fixed block size, 0xFFF9 for variable block size)
- Sample position extraction MUST use UTF-8-like coded number parsing per RFC 9639 Section 9.1.5
- Frame validation MUST include CRC-8 header checksum verification per RFC 9639 Section 9.1.8
- Blocking strategy interpretation MUST follow RFC 9639 Section 9.1 (fixed = frame number, variable = sample number)

**Key Implementation Insights:**
- FLAC frames are variable-length compressed data, making exact byte-to-sample mapping impossible without parsing
- Bisection search with iterative refinement converges quickly (typically 3-5 iterations)
- 250ms tolerance is acceptable for user-initiated seeks and matches industry standards
- The algorithm must handle edge cases like seeking near the beginning or end of the file

## Glossary

- **Bisection Seeking**: A seeking strategy that estimates byte position based on time ratio, then iteratively refines using binary search
- **Target Sample**: The sample number corresponding to the desired seek timestamp
- **Actual Sample**: The sample number of the frame found at the estimated byte position
- **Sample Differential**: The difference between target sample and actual sample
- **Time Differential**: The sample differential converted to milliseconds
- **Tolerance**: The maximum acceptable time differential (250ms)
- **Audio Data Region**: The portion of the FLAC file containing audio frames (after metadata blocks)
- **Iteration Limit**: Maximum number of refinement passes before accepting current position
- **Convergence**: When the time differential is within tolerance

## Requirements

### Requirement 1: Byte Position Estimation

**User Story:** As a media player, I want to estimate byte positions based on time ratios, so that I can quickly approximate seek targets without parsing the entire file.

#### Acceptance Criteria

1. WHEN seeking without SEEKTABLE, THE FLAC Demuxer SHALL calculate initial byte position using: `estimated_pos = audio_data_offset + (target_sample / total_samples) * audio_data_size`
2. WHEN calculating audio data size, THE FLAC Demuxer SHALL use `file_size - audio_data_offset`
3. WHEN total samples is unknown (zero), THE FLAC Demuxer SHALL fall back to beginning-of-file seeking
4. WHEN estimated position exceeds file size, THE FLAC Demuxer SHALL clamp to `file_size - minimum_frame_size`
5. WHEN estimated position is before audio data offset, THE FLAC Demuxer SHALL clamp to audio data offset

### Requirement 2: Frame Discovery at Estimated Position (RFC 9639 Section 9.1)

**User Story:** As a seeking algorithm, I want to find valid FLAC frames near estimated positions per RFC 9639, so that I can determine actual sample positions with format compliance.

#### Acceptance Criteria

1. WHEN seeking to estimated position, THE FLAC Demuxer SHALL search forward for the 15-bit sync pattern 0b111111111111100 per RFC 9639 Section 9.1
2. WHEN sync pattern is found, THE FLAC Demuxer SHALL verify blocking strategy bit matches stream's established strategy per RFC 9639 Section 9.1
3. WHEN frame sync is found, THE FLAC Demuxer SHALL validate CRC-8 checksum per RFC 9639 Section 9.1.8 before accepting frame
4. WHEN frame header CRC-8 passes, THE FLAC Demuxer SHALL parse coded number per RFC 9639 Section 9.1.5 to extract sample position
5. WHEN blocking strategy is fixed (0xFFF8), THE FLAC Demuxer SHALL interpret coded number as frame number and multiply by block size per RFC 9639 Section 9.1.5
6. WHEN blocking strategy is variable (0xFFF9), THE FLAC Demuxer SHALL interpret coded number directly as sample number per RFC 9639 Section 9.1.5
7. WHEN no valid frame is found within 64KB, THE FLAC Demuxer SHALL report frame discovery failure
8. WHEN frame header CRC-8 fails, THE FLAC Demuxer SHALL continue searching for next sync code (false positive detection)
9. WHEN frame is discovered and validated, THE FLAC Demuxer SHALL record file position, sample offset, and block size

### Requirement 3: Iterative Refinement with Bisection

**User Story:** As a seeking algorithm, I want to iteratively refine position estimates using bisection, so that I can converge on the target within acceptable tolerance.

#### Acceptance Criteria

1. WHEN actual sample is before target sample, THE FLAC Demuxer SHALL adjust search to upper half of remaining range
2. WHEN actual sample is after target sample, THE FLAC Demuxer SHALL adjust search to lower half of remaining range
3. WHEN time differential is within 250ms, THE FLAC Demuxer SHALL accept current position as final
4. WHEN iteration count exceeds 10, THE FLAC Demuxer SHALL accept current best position
5. WHEN search range becomes smaller than minimum frame size, THE FLAC Demuxer SHALL accept current position
6. WHEN bisection produces same position twice consecutively, THE FLAC Demuxer SHALL accept current position

### Requirement 4: Tolerance and Acceptance Criteria

**User Story:** As a media player, I want seeks to land within 250ms of the target, so that users experience smooth navigation without noticeable jumps.

#### Acceptance Criteria

1. WHEN calculating time differential, THE FLAC Demuxer SHALL use `abs(actual_sample - target_sample) * 1000 / sample_rate`
2. WHEN time differential is 250ms or less, THE FLAC Demuxer SHALL consider the seek successful
3. WHEN time differential exceeds 250ms after maximum iterations, THE FLAC Demuxer SHALL use the closest position found
4. WHEN actual position is before target, THE FLAC Demuxer SHALL prefer this over positions after target (for gapless playback)
5. WHEN logging seek results, THE FLAC Demuxer SHALL report final time differential in milliseconds

### Requirement 5: Edge Case Handling

**User Story:** As a seeking algorithm, I want to handle edge cases gracefully, so that seeking works reliably across all valid FLAC files.

#### Acceptance Criteria

1. WHEN seeking to first 500ms of file, THE FLAC Demuxer SHALL seek directly to audio data offset
2. WHEN seeking to last 500ms of file, THE FLAC Demuxer SHALL estimate position conservatively to avoid overshooting
3. WHEN file has highly variable compression, THE FLAC Demuxer SHALL allow more iterations before accepting
4. WHEN I/O errors occur during seeking, THE FLAC Demuxer SHALL restore previous position and report failure
5. WHEN frame parsing repeatedly fails, THE FLAC Demuxer SHALL fall back to beginning-of-file seeking

### Requirement 6: Performance Requirements

**User Story:** As a media player, I want seeking to complete quickly, so that users don't experience noticeable delays.

#### Acceptance Criteria

1. WHEN performing bisection seeking, THE FLAC Demuxer SHALL complete within 100ms for typical files
2. WHEN searching for frame sync, THE FLAC Demuxer SHALL limit search to 64KB per iteration
3. WHEN performing I/O operations, THE FLAC Demuxer SHALL minimize seeks by reading larger buffers
4. WHEN caching frame positions, THE FLAC Demuxer SHALL add discovered frames to the frame index for future seeks
5. WHEN frame index contains nearby entries, THE FLAC Demuxer SHALL use them to narrow initial search range

### Requirement 7: Integration with Existing Seeking Strategies

**User Story:** As a FLAC demuxer, I want bisection seeking to integrate with existing strategies, so that the best available method is always used.

#### Acceptance Criteria

1. WHEN SEEKTABLE is available, THE FLAC Demuxer SHALL prefer SEEKTABLE-based seeking over bisection
2. WHEN frame index contains target position, THE FLAC Demuxer SHALL prefer frame index over bisection
3. WHEN bisection seeking fails, THE FLAC Demuxer SHALL fall back to beginning-of-file seeking
4. WHEN bisection succeeds, THE FLAC Demuxer SHALL update frame index with discovered positions
5. WHEN seeking to position 0, THE FLAC Demuxer SHALL bypass bisection and seek directly to audio data offset

