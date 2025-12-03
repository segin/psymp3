# FLAC DEMUXER IMPLEMENTATION PLAN

## Implementation Tasks Based on RFC 9639 Compliance

**Key Implementation Priorities:**
1. RFC 9639 compliance with strict validation of forbidden patterns and reserved fields
2. Accurate frame size estimation using STREAMINFO minimum frame size
3. Efficient frame boundary detection with limited search scope (512 bytes max)
4. Thread-safe public/private method patterns to prevent deadlocks
5. Comprehensive debug logging with method identification tokens
6. Robust error recovery for corrupted or incomplete streams

---

## Phase 1: Core Infrastructure

- [-] 1. Create FLACDemuxer Class Structure
  - [x] 1.1 Define FLACDemuxer class inheriting from Demuxer base class
    - Add private member variables for FLAC container state and metadata
    - Implement constructor accepting unique_ptr of IOHandler
    - Add destructor with proper resource cleanup
    - _Requirements: 26.1, 26.5_
  - [x] 1.2 Define FLAC Data Structures
    - Create FLACMetadataBlock structure with type, is_last, length, data_offset
    - Define FLACFrame structure with sample_offset, file_offset, block_size, frame_size
    - Add FLACMetadataType enum for block types 0-6
    - _Requirements: 2.6-2.12_
  - [x] 1.3 Implement Basic Demuxer Interface
    - Implement all pure virtual methods from Demuxer base class
    - Add placeholder implementations returning appropriate defaults
    - _Requirements: 26.1, 26.2, 26.3_

- [x] 1.4 Write property test for stream marker validation
  - **Property 1: Stream Marker Validation**
  - **Validates: Requirements 1.2, 1.3**

---

## Phase 2: Container Parsing

- [x] 2. Implement Stream Marker Validation (RFC 9639 Section 6)
  - [x] 2.1 Implement validateStreamMarker method
    - Read first 4 bytes of file
    - Verify bytes equal 0x66 0x4C 0x61 0x43
    - Reject invalid markers without crashing
    - Log exact byte values found versus expected
    - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5_

- [x] 3. Implement Metadata Block Header Parsing (RFC 9639 Section sudo apt install nsolid -yl
  - [x] 3.1 Implement parseMetadataBlockHeader method
    - Read 4-byte header
    - Extract bit 7 as is_last flag
    - Extract bits 0-6 as block type
    - Extract bytes 1-3 as 24-bit big-endian block length
    - _Requirements: 2.1, 2.2, 2.3, 2.5_
  - [x] 3.2 Implement forbidden block type detection
    - Reject block type 127 as forbidden pattern
    - Handle reserved block types 7-126 by skipping
    - _Requirements: 2.4, 2.13, 18.1_

- [x] 3.3 Write property test for metadata block header parsing
  - **Property 2: Metadata Block Header Bit Extraction**
  - **Validates: Requirements 2.2, 2.3**

- [x] 3.4 Write property test for forbidden block type
  - **Property 3: Forbidden Block Type Detection**
  - **Validates: Requirements 2.4, 18.1**

- [x] 4. Implement STREAMINFO Block Parsing (RFC 9639 Section 8.2)
  - [x] 4.1 Implement parseStreamInfo method
    - Verify STREAMINFO is first metadata block
    - Read exactly 34 bytes of data
    - Extract u16 min and max block size big-endian
    - Extract u24 min and max frame size big-endian
    - Extract u20 sample rate, u3 channels-1, u5 bit depth-1
    - Extract u36 total samples, u128 MD5 checksum
    - _Requirements: 3.1-3.5, 3.9, 3.10, 3.13, 3.15-3.17, 3.19_
  - [x] 4.2 Implement STREAMINFO validation
    - Reject if min or max block size less than 16
    - Reject if min block size exceeds max block size
    - Reject if sample rate is 0 for audio streams
    - Handle unknown values for frame size 0, total samples 0, MD5 all zeros
    - _Requirements: 3.6-3.8, 3.11, 3.12, 3.14, 3.18, 3.20_

- [x] 4.3 Write property test for STREAMINFO block size validation
  - **Property 4: STREAMINFO Block Size Validation**
  - **Validates: Requirements 3.6, 3.7**

- [x] 4.4 Write property test for STREAMINFO block size ordering
  - **Property 5: STREAMINFO Block Size Ordering**
  - **Validates: Requirements 3.8**

- [x] 5. Checkpoint
  - Ensure all tests pass, ask the user if questions arise.

---

## Phase 3: Additional Metadata Blocks

- [x] 6. Implement PADDING Block Handling (RFC 9639 Section 8.3)
  - [x] 6.1 Implement parsePadding method
    - Read block length from header
    - Skip exactly block_length bytes
    - Handle multiple PADDING blocks
    - _Requirements: 14.1-14.4_

- [x] 7. Implement APPLICATION Block Handling (RFC 9639 Section 8.4)
  - [x] 7.1 Implement parseApplication method
    - Read u32 application ID
    - Skip remaining block_length minus 4 bytes
    - Handle unrecognized application blocks gracefully
    - _Requirements: 15.1-15.4_

- [x] 8. Implement SEEKTABLE Block Parsing (RFC 9639 Section 8.5)
  - [x] 8.1 Implement parseSeekTable method
    - Calculate seek point count as block_length divided by 18
    - Parse u64 sample number, u64 byte offset, u16 frame samples per point
    - Detect placeholder seek points with value 0xFFFFFFFFFFFFFFFF
    - Validate seek points are sorted in ascending order
    - _Requirements: 12.1-12.8_

- [x] 8.2 Write property test for seek point placeholder detection
  - **Property 15: Seek Point Placeholder Detection**
  - **Validates: Requirements 12.5**

- [x] 9. Implement VORBIS_COMMENT Block Parsing (RFC 9639 Section 8.6)
  - [x] 9.1 Implement parseVorbisComment method
    - Parse u32 little-endian vendor string length
    - Parse UTF-8 vendor string
    - Parse u32 little-endian field count
    - Parse each field with u32 little-endian length
    - Split fields on first equals sign into name and value
    - Validate field names use printable ASCII 0x20-0x7E except 0x3D
    - _Requirements: 13.1-13.8_

- [x] 9.2 Write property test for endianness handling
  - **Property 16: Endianness Handling**
  - **Validates: Requirements 19.1, 19.4, 13.1**

- [x] 10. Implement CUESHEET Block Parsing (RFC 9639 Section 8.7)
  - [x] 10.1 Implement parseCuesheet method
    - Read u128 media catalog number
    - Read u64 lead-in samples and u1 CD-DA flag
    - Skip reserved bits and read u8 number of tracks
    - Validate track count is at least 1
    - Validate CD-DA track count is at most 100
    - _Requirements: 16.1-16.8_

- [x] 10.2 Write property test for CUESHEET track count validation
  - **Property 17: CUESHEET Track Count Validation**
  - **Validates: Requirements 16.6**

- [x] 11. Implement PICTURE Block Parsing (RFC 9639 Section 8.8)
  - [x] 11.1 Implement parsePicture method
    - Parse u32 picture type
    - Parse media type length and ASCII string
    - Parse description length and UTF-8 string
    - Parse dimensions, color depth, indexed colors
    - Parse picture data length and binary data
    - Handle URI format when media type is arrow
    - _Requirements: 17.1-17.12_

- [x] 12. Checkpoint
  - Ensure all tests pass, ask the user if questions arise.

---

## Phase 4: Frame Header Parsing

- [x] 13. Implement Frame Sync Code Detection (RFC 9639 Section 9.1)
  - [x] 13.1 Implement findNextFrame method
    - Search for 15-bit sync pattern 0b111111111111100
    - Verify byte alignment of sync code
    - Limit search scope to 512 bytes maximum
    - _Requirements: 4.1, 4.2, 21.3_
  - [x] 13.2 Implement blocking strategy validation
    - Extract blocking strategy bit where 0 is fixed and 1 is variable
    - Distinguish 0xFFF8 fixed from 0xFFF9 variable
    - Reject if blocking strategy changes mid-stream
    - _Requirements: 4.3-4.8_

- [x] 13.3 Write property test for frame sync code detection
  - **Property 6: Frame Sync Code Detection**
  - **Validates: Requirements 4.1, 4.2**

- [x] 13.4 Write property test for blocking strategy consistency
  - **Property 7: Blocking Strategy Consistency**
  - **Validates: Requirements 4.8**

- [x] 14. Implement Block Size Bits Parser (RFC 9639 Table 14)
  - [x] 14.1 Implement parseBlockSizeBits method
    - Extract bits 4-7 of frame byte 2
    - Implement all 16 lookup table values
    - Handle uncommon block sizes 8-bit and 16-bit
    - Reject reserved pattern 0b0000
    - Reject forbidden uncommon block size 65536
    - _Requirements: 5.1-5.18_

- [x] 14.2 Write property test for reserved block size pattern
  - **Property 8: Reserved Block Size Pattern Detection**
  - **Validates: Requirements 5.2**

- [x] 14.3 Write property test for forbidden block size
  - **Property 9: Forbidden Block Size Detection**
  - **Validates: Requirements 5.18**

- [x] 15. Implement Sample Rate Bits Parser (RFC 9639 Section 9.1.2)
  - [x] 15.1 Implement parseSampleRateBits method
    - Extract bits 0-3 of frame byte 2
    - Implement all 16 lookup table values
    - Handle uncommon sample rates in kHz, Hz, and tens of Hz
    - Reject forbidden pattern 0b1111
    - _Requirements: 6.1-6.17_

- [x] 15.2 Write property test for forbidden sample rate
  - **Property 10: Forbidden Sample Rate Detection**
  - **Validates: Requirements 6.17**

- [x] 16. Implement Channel Assignment Parser (RFC 9639 Section 9.1.3)
  - [x] 16.1 Implement parseChannelBits method
    - Extract bits 4-7 of frame byte 3
    - Support independent channels 0b0000-0b0111
    - Support stereo modes left-side, right-side, mid-side
    - Reject reserved patterns 0b1011-0b1111
    - _Requirements: 7.1-7.7_

- [x] 16.2 Write property test for reserved channel bits
  - **Property 11: Reserved Channel Bits Detection**
  - **Validates: Requirements 7.7**

- [x] 17. Implement Bit Depth Parser (RFC 9639 Section 9.1.4)
  - [x] 17.1 Implement parseBitDepthBits method
    - Extract bits 1-3 of frame byte 3
    - Implement all 8 lookup table values
    - Reject reserved pattern 0b011
    - Validate reserved bit at bit 0 is zero
    - _Requirements: 8.1-8.11_

- [x] 17.2 Write property test for reserved bit depth
  - **Property 12: Reserved Bit Depth Detection**
  - **Validates: Requirements 8.5**

- [x] 18. Implement Coded Number Parser (RFC 9639 Section 9.1.5)
  - [x] 18.1 Implement parseCodedNumber method
    - Parse UTF-8-like variable-length encoding 1-7 bytes
    - Support all byte length patterns
    - Interpret as frame number for fixed or sample number for variable
    - _Requirements: 9.1-9.10_

- [x] 19. Checkpoint
  - Ensure all tests pass, ask the user if questions arise.

---

## Phase 5: CRC Validation

- [x] 20. Implement CRC-8 Validation (RFC 9639 Section 9.1.8)
  - [x] 20.1 Implement calculateCRC8 method
    - Use polynomial 0x07
    - Initialize CRC to 0
    - Cover all frame header bytes except CRC itself
    - _Requirements: 10.1-10.3_
  - [x] 20.2 Implement frame header CRC validation
    - Validate CRC-8 after parsing header
    - Log CRC mismatches with frame position
    - Attempt resynchronization on failure
    - Support strict mode rejection
    - _Requirements: 10.4-10.6_

- [x] 20.3 Write property test for CRC-8 calculation
  - **Property 13: CRC-8 Calculation Correctness**
  - **Validates: Requirements 10.2**

- [x] 21. Implement CRC-16 Validation (RFC 9639 Section 9.3)
  - [x] 21.1 Implement calculateCRC16 method
    - Use polynomial 0x8005
    - Initialize CRC to 0
    - Cover entire frame from sync code to end of subframes
    - _Requirements: 11.2-11.5_
  - [x] 21.2 Implement frame footer CRC validation
    - Ensure byte alignment with zero padding
    - Read 16-bit CRC from footer
    - Log CRC mismatches and attempt to continue
    - Support strict mode rejection
    - _Requirements: 11.1, 11.6-11.8_

- [x] 21.3 Write property test for CRC-16 calculation
  - **Property 14: CRC-16 Calculation Correctness**
  - **Validates: Requirements 11.3**
  - **Status: PASSED** (724/724 tests)

- [x] 22. Checkpoint
  - Ensure all tests pass, ask the user if questions arise.
  - **Status: PASSED** - CRC-8 (722 tests) and CRC-16 (724 tests) all pass

---

## Phase 6: Frame Size Estimation and Streaming

- [x] 23. Implement Frame Size Estimation
  - [x] 23.1 Implement calculateFrameSize method
    - Use STREAMINFO minimum frame size as primary estimate
    - For fixed block size streams use minimum directly without scaling
    - Avoid complex theoretical calculations
    - Handle highly compressed streams with frames as small as 14 bytes
    - Add debug logging with calculateFrameSize token
    - _Requirements: 21.1, 21.2, 21.5, 25.1, 25.4_

- [x] 23.2 Write property test for frame size estimation
  - **Property 18: Frame Size Estimation**
  - **Validates: Requirements 21.1**

- [x] 23.3 Write property test for frame boundary search limit
  - **Property 19: Frame Boundary Search Limit**
  - **Validates: Requirements 21.3**

- [x] 24. Implement Data Streaming
  - [x] 24.1 Implement readChunk methods
    - Locate next frame sync code
    - Parse frame header to determine size
    - Read complete frame including CRC
    - Return frame as MediaChunk with proper timing
    - _Requirements: 21.7, 21.8, 26.3_
  - [x] 24.2 Implement position tracking
    - Maintain current sample position during reading
    - Update position based on frame block sizes
    - Provide accurate position reporting
    - _Requirements: 23.3, 23.6_

- [x] 25. Checkpoint
  - Ensure all tests pass, ask the user if questions arise.

---

## Phase 7: Seeking and Duration

- [x] 26. Implement Duration Calculation
  - [x] 26.1 Implement getDuration method
    - Use total samples from STREAMINFO
    - Handle unknown duration when total samples is 0
    - Convert samples to milliseconds using sample rate
    - Use 64-bit integers for large files
    - _Requirements: 23.1, 23.2, 23.4, 23.8_

- [x] 26.2 Write property test for duration calculation
  - **Property 20: Duration Calculation**
  - **Validates: Requirements 23.1, 23.4**

- [x] 27. Implement Seeking Operations
  - [x] 27.1 Implement seekTo method
    - Reset to first audio frame for seek to beginning
    - Clamp seeks beyond stream end to last valid position
    - Maintain current position on seek failure
    - _Requirements: 22.1, 22.5, 22.6_
  - [x] 27.2 Implement SEEKTABLE-based seeking
    - Find closest seek point not exceeding target sample
    - Add byte offset to first frame header position
    - Parse frames forward to exact position
    - _Requirements: 22.2, 22.3, 22.8_
  - [x] 27.3 Implement frame indexing for seeking
    - Build frame index during initial parsing
    - Cache discovered frame positions
    - Provide sample-accurate seeking using index
    - _Requirements: 22.4, 22.7_

- [x] 28. Checkpoint
  - Ensure all tests pass, ask the user if questions arise.

---

## Phase 8: Error Handling and Recovery

- [x] 29. Implement Error Handling
  - [x] 29.1 Implement container-level error handling
    - Reject invalid stream markers without crashing
    - Skip corrupted metadata blocks and continue
    - Derive parameters from frame headers if STREAMINFO missing
    - _Requirements: 24.1-24.3_
  - [x] 29.2 Implement frame-level error recovery
    - Resynchronize to next valid sync code on sync loss
    - Log CRC errors but attempt to continue
    - Handle truncated files gracefully
    - Return appropriate error codes on allocation failure
    - _Requirements: 24.4-24.8_

- [x] 29.3 Write property test for sync resynchronization
  - **Property 21: Error Recovery - Sync Resynchronization**
  - **Validates: Requirements 24.4**

- [x] 30. Checkpoint
  - Ensure all tests pass, ask the user if questions arise.

---

## Phase 9: Thread Safety

- [x] 31. Implement Thread Safety
  - [x] 31.1 Implement public/private lock pattern
    - Create public methods that acquire locks
    - Create private _unlocked implementations
    - Ensure internal calls use _unlocked versions
    - Document lock acquisition order
    - Use RAII lock guards for exception safety
    - _Requirements: 28.1-28.4_
  - [x] 31.2 Implement callback safety
    - Release locks before callback invocation
    - Use atomic operations for sample counters
    - Use atomic error state flags
    - _Requirements: 28.5-28.7_
  - [x] 31.3 Write property test for thread safety lock pattern
    - **Property 22: Thread Safety - Lock Pattern**
    - **Validates: Requirements 28.1, 28.2**

- [x] 32. Checkpoint
  - Ensure all tests pass, ask the user if questions arise.

---

## Phase 10: Integration and Debug Logging

- [x] 33. Implement Debug Logging
  - [x] 33.1 Add method identification tokens
    - Include tokens like parseStreamInfo and calculateFrameSize in all debug messages
    - Log frame size estimation method selection and results
    - Log frame boundary detection scope and outcomes
    - Log seeking strategy selection and outcomes
    - _Requirements: 29.1-29.8_

- [x] 33.2 Write property test for debug logging format
  - **Property 23: Debug Logging Format**
  - **Validates: Requirements 29.1**

- [x] 34. Implement Demuxer Architecture Integration
  - [x] 34.1 Complete IOHandler integration
    - Use IOHandler exclusively for all I/O
    - Support FileIOHandler and HTTPIOHandler
    - Handle 64-bit offsets for large files
    - Propagate I/O errors appropriately
    - _Requirements: 26.5, 26.7_
  - [x] 34.2 Complete MediaFactory integration
    - Register for .flac extension
    - Register for audio/flac MIME type
    - Implement format detection based on fLaC marker
    - _Requirements: 26.1, 26.2_
  - [x] 34.3 Populate StreamInfo correctly
    - Include accurate FLAC parameters
    - Provide compatible frame data format for FLACCodec
    - Return timestamps in milliseconds consistently
    - _Requirements: 26.2, 26.4, 26.8_

- [x] 35. Implement Streamable Subset Detection (RFC 9639 Section 7)
  - [x] 35.1 Implement streamable subset validation
    - Mark non-streamable if sample rate bits equal 0b0000
    - Mark non-streamable if bit depth bits equal 0b000
    - Mark non-streamable if max block size exceeds 16384
    - Mark non-streamable if sample rate 48kHz or less and block size exceeds 4608
    - Mark non-streamable if WAVEFORMATEXTENSIBLE_CHANNEL_MASK present
    - _Requirements: 20.1-20.5_

- [x] 36. Ensure Compatibility
  - [x] 36.1 Verify backward compatibility
    - Support all previously supported FLAC variants
    - Extract same metadata fields as current implementation
    - Provide equivalent seeking accuracy
    - Handle all currently working FLAC files
    - Work through DemuxedStream bridge
    - _Requirements: 27.1-27.8_

- [x] 37. Final Checkpoint
  - Ensure all tests pass, ask the user if questions arise.
  - **Status: ALL TESTS PASSED** - All 23 property tests and backward compatibility tests pass

