# Implementation Plan

- [x] 1. Implement byte position estimation
  - [x] 1.1 Add `estimateBytePosition_unlocked()` method to FLACDemuxer
    - Calculate position using `audio_offset + (target/total) * audio_size`
    - Clamp to valid range `[audio_offset, file_size - 64]`
    - Handle edge case when total_samples is 0
    - _Requirements: 1.1, 1.2, 1.4, 1.5_
  - [x] 1.2 Write property test for byte estimation
    - **Property 1: Byte Position Estimation Correctness**
    - **Validates: Requirements 1.1, 1.2, 1.4, 1.5**

- [x] 2. Implement frame finder for bisection seeking
  - [x] 2.1 Add `findFrameAtPosition_unlocked()` method to FLACDemuxer
    - Seek to given position
    - Search forward for sync pattern (0xFFF8 or 0xFFF9)
    - Validate CRC-8 per RFC 9639 Section 9.1.8
    - Parse coded number per RFC 9639 Section 9.1.5
    - Return frame position, sample offset, and block size
    - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 2.9_
  - [x] 2.2 Write property test for frame sync detection
    - **Property 2: Frame Sync Detection (RFC 9639 Section 9.1)**
    - **Validates: Requirements 2.1, 2.2**
  - [x] 2.3 Write property test for CRC-8 validation
    - **Property 3: CRC-8 Validation (RFC 9639 Section 9.1.8)**
    - **Validates: Requirements 2.3, 2.8**


- [x] 3. Implement bisection controller
  - [x] 3.1 Add `seekWithByteEstimation_unlocked()` method to FLACDemuxer
    - Initialize search range [audio_offset, file_size]
    - Loop: estimate position, find frame, check tolerance
    - Adjust range based on actual vs target sample
    - Track best position found
    - Terminate on convergence or iteration limit
    - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6_
  - [x] 3.2 Write property test for bisection range adjustment
    - **Property 6: Bisection Range Adjustment**
    - **Validates: Requirements 3.1, 3.2**
  - [x] 3.3 Write property test for convergence termination
    - **Property 7: Convergence Termination**
    - **Validates: Requirements 3.3, 3.4, 3.5, 3.6**

- [x] 4. Implement time differential and best position logic
  - [x] 4.1 Add time differential calculation
    - Calculate `abs(actual - target) * 1000 / sample_rate`
    - Compare against 250ms tolerance
    - _Requirements: 4.1, 4.2_
  - [x] 4.2 Add best position tracking
    - Track position with minimum differential
    - Prefer positions before target when equal
    - _Requirements: 4.3, 4.4_
  - [x] 4.3 Write property test for time differential calculation
    - **Property 8: Time Differential Calculation**
    - **Validates: Requirements 4.1, 4.2**
  - [x] 4.4 Write property test for best position selection
    - **Property 9: Best Position Selection**
    - **Validates: Requirements 4.3, 4.4**

- [ ] 5. Integrate with existing seeking code
  - [ ] 5.1 Update `seekTo_unlocked()` to call bisection seeking
    - Call `seekWithByteEstimation_unlocked()` when SEEKTABLE and frame index unavailable
    - Maintain existing strategy priority order
    - _Requirements: 7.1, 7.2, 7.3, 7.5_
  - [ ] 5.2 Add discovered frames to frame index
    - Call `addFrameToIndex_unlocked()` for each frame found during bisection
    - _Requirements: 6.4_
  - [ ] 5.3 Write property test for strategy priority
    - **Property 10: Strategy Priority**
    - **Validates: Requirements 7.1, 7.2, 7.3, 7.5**

- [ ] 6. Checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 7. Test with real FLAC files
  - [ ] 7.1 Test seeking on FLAC files without SEEKTABLEs
    - Verify seeks land within 250ms of target
    - Test seeks to beginning, middle, and end of file
    - _Requirements: 4.2, 5.1, 5.2_
  - [ ] 7.2 Verify user can play "RADIO GA GA.flac" without issues
    - Confirm track loads and plays correctly
    - Confirm seeking works throughout the track
    - _Requirements: All_

- [ ] 8. Final Checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

