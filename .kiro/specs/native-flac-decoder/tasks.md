# Implementation Plan

## Task Overview

This implementation plan breaks down the native FLAC decoder into discrete, manageable coding tasks. Each task builds incrementally on previous tasks, following the migration path outlined in the design document. Tasks are organized into phases for systematic development.

## Phase 1: Foundation and Core Infrastructure

- [x] 1. Set up project structure and build system integration
  - Create NativeFLACDecoder.h and NativeFLACDecoder.cpp in src/
  - Add conditional compilation guards (#ifdef HAVE_NATIVE_FLAC)
  - Update configure.ac with --enable-native-flac option
  - Update src/Makefile.am to conditionally compile native decoder
  - Create typedef/using declaration for FLACCodec selection
  - _Requirements: 17_

- [x] 2. Implement BitstreamReader class
  - [x] 2.1 Create BitstreamReader.h with class interface
    - Define bit buffer cache (64-bit)
    - Define position tracking members
    - Declare public interface methods
    - _Requirements: 1, 39_
  - [x] 2.2 Implement basic bit reading operations
    - Implement readBits() for unsigned values
    - Implement readBitsSigned() for signed values
    - Implement cache refill mechanism
    - Handle big-endian bit ordering
    - _Requirements: 1, 39_
  - [x] 2.3 Implement special encoding readers
    - Implement readUnary() for unary-coded values
    - Implement readUTF8() for UTF-8 coded numbers
    - Implement readRiceCode() for Rice-coded values
    - _Requirements: 1, 21, 32, 40_
  - [x] 2.4 Implement buffer management
    - Implement feedData() for input buffering
    - Implement clearBuffer() for reset
    - Implement byte alignment functions
    - Track bit position for seeking
    - _Requirements: 1, 55_

- [x] 3. Implement CRCValidator class
  - [x] 3.1 Create CRC lookup tables
    - Generate CRC-8 table with polynomial 0x07
    - Generate CRC-16 table with polynomial 0x8005
    - Implement static table initialization
    - _Requirements: 8, 56_
  - [x] 3.2 Implement CRC computation
    - Implement computeCRC8() for frame headers
    - Implement computeCRC16() for complete frames
    - Implement incremental CRC update methods
    - _Requirements: 8, 56_

## Phase 2: Frame Parsing and Structure

- [x] 4. Implement FrameParser class
  - [x] 4.1 Create FrameParser.h with frame structures
    - Define FrameHeader struct
    - Define FrameFooter struct
    - Define ChannelAssignment enum
    - _Requirements: 2, 33_
  - [x] 4.2 Implement frame sync detection
    - Implement findSync() to locate 0xFFF8-0xFFFF pattern
    - Validate sync pattern on byte boundary
    - Handle sync loss and recovery
    - _Requirements: 2, 33_
  - [x] 4.3 Implement frame header parsing
    - Parse blocking strategy bit
    - Parse block size bits with lookup table
    - Parse sample rate bits with lookup table
    - Parse channel assignment bits
    - Parse bit depth bits with lookup table
    - Parse reserved bit (must be 0)
    - _Requirements: 2, 22, 26, 27, 34_
  - [x] 4.4 Implement coded number parsing
    - Parse UTF-8 coded frame/sample number
    - Handle 1-7 byte UTF-8 encoding
    - Validate coded number range
    - _Requirements: 2, 21_
  - [x] 4.5 Implement uncommon parameter handling
    - Parse 8-bit or 16-bit uncommon block size
    - Parse uncommon sample rate (kHz, Hz, Hz/10)
    - Validate against forbidden values
    - _Requirements: 2, 22_
  - [x] 4.6 Implement frame header CRC validation
    - Compute CRC-8 over frame header
    - Validate against header CRC byte
    - Reject frame on CRC mismatch
    - _Requirements: 2, 8, 56_
  - [x] 4.7 Implement frame footer parsing
    - Align to byte boundary with zero padding
    - Parse 16-bit CRC-16
    - Validate complete frame CRC
    - _Requirements: 2, 8, 55, 56_

- [x] 5. Implement forbidden pattern detection
  - Validate metadata block type != 127
  - Validate sample rate bits != 0b1111
  - Validate uncommon block size != 65536
  - Validate predictor coefficient precision != 0b1111
  - Reject negative predictor right shift
  - _Requirements: 23_

## Phase 3: Subframe Decoding

- [x] 6. Implement SubframeDecoder class
  - [x] 6.1 Create SubframeDecoder.h with structures
    - Define SubframeType enum
    - Define SubframeHeader struct
    - Declare decoder interface
    - _Requirements: 3_
  - [x] 6.2 Implement subframe header parsing
    - Parse mandatory 0 bit
    - Parse 6-bit subframe type
    - Parse wasted bits flag and count
    - Calculate subframe bit depth
    - _Requirements: 3, 20, 36_
  - [x] 6.3 Implement CONSTANT subframe decoder
    - Read single unencoded sample
    - Replicate to all block samples
    - Apply wasted bits padding
    - _Requirements: 3_
  - [x] 6.4 Implement VERBATIM subframe decoder
    - Read unencoded samples sequentially
    - Handle subframe bit depth
    - Apply wasted bits padding
    - _Requirements: 3_
  - [x] 6.5 Implement FIXED predictor decoder
    - Read warm-up samples (order 0-4)
    - Decode residual samples
    - Apply fixed predictor formulas
    - Reconstruct samples sequentially
    - _Requirements: 3, 4, 35, 54_
  - [x] 6.6 Implement LPC predictor decoder
    - Read warm-up samples
    - Parse coefficient precision (4-bit)
    - Parse prediction right shift (5-bit signed)
    - Read predictor coefficients
    - Decode residual samples
    - Apply LPC prediction with 64-bit arithmetic
    - _Requirements: 3, 5, 28, 51, 52, 54_

## Phase 4: Residual Decoding

- [x] 7. Implement ResidualDecoder class
  - [x] 7.1 Create ResidualDecoder.h with structures
    - Define CodingMethod enum (4-bit/5-bit Rice)
    - Define PartitionInfo struct
    - Declare decoder interface
    - _Requirements: 6, 29_
  - [x] 7.2 Implement residual header parsing
    - Parse 2-bit coding method
    - Parse 4-bit partition order
    - Calculate partition sample counts
    - Validate partition order constraints
    - _Requirements: 6, 29, 53_
  - [x] 7.3 Implement Rice code decoder
    - Decode unary quotient (count zeros)
    - Decode binary remainder
    - Compute folded residual value
    - Apply zigzag decoding to unfold
    - _Requirements: 6, 31, 32_
  - [x] 7.4 Implement partition decoder
    - Parse Rice parameter (4-bit or 5-bit)
    - Detect escape code (0b1111 or 0b11111)
    - Decode Rice-coded partition
    - Handle escaped partition with unencoded samples
    - _Requirements: 6, 29, 30_
  - [x] 7.5 Implement residual validation
    - Verify residuals fit in 32-bit signed range
    - Reject most negative 32-bit value
    - Validate total residual count matches block size
    - _Requirements: 6, 37_

## Phase 5: Channel Processing

- [x] 8. Implement ChannelDecorrelator class
  - [x] 8.1 Create ChannelDecorrelator.h
    - Define ChannelAssignment enum
    - Declare decorrelation methods
    - _Requirements: 7_
  - [x] 8.2 Implement stereo decorrelation
    - Implement left-side decorrelation (right = left - side)
    - Implement right-side decorrelation (left = right + side)
    - Implement mid-side decorrelation with proper rounding
    - Handle mid-side odd sample reconstruction
    - _Requirements: 7_
  - [x] 8.3 Implement multi-channel support
    - Handle independent channel assignment
    - Support 1-8 channels per RFC 9639
    - Validate channel count
    - _Requirements: 7, 26_

- [x] 9. Implement SampleReconstructor class
  - [x] 9.1 Create SampleReconstructor.h
    - Declare bit depth conversion methods
    - Define output format (16-bit PCM)
    - _Requirements: 9, 10_
  - [x] 9.2 Implement bit depth conversion
    - Implement 16-bit passthrough (no conversion)
    - Implement 8-bit to 16-bit upscaling
    - Implement 24-bit to 16-bit downscaling with rounding
    - Implement 32-bit to 16-bit downscaling
    - Handle 4-12 bit and 20-bit depths
    - _Requirements: 9_
  - [x] 9.3 Implement sample interleaving
    - Interleave channels for stereo output
    - Interleave multi-channel output
    - Ensure proper channel ordering
    - _Requirements: 10_
  - [x] 9.4 Implement output validation
    - Verify samples within 16-bit range
    - Prevent clipping during conversion
    - Validate sample count
    - _Requirements: 10, 57_

## Phase 6: Metadata Parsing

- [ ] 10. Implement MetadataParser class
  - [ ] 10.1 Create MetadataParser.h with structures
    - Define MetadataType enum
    - Define StreamInfoMetadata struct
    - Define SeekPoint struct
    - Define VorbisComment struct
    - _Requirements: 41, 42_
  - [ ] 10.2 Implement metadata block header parsing
    - Parse 1-bit last-block flag
    - Parse 7-bit block type
    - Parse 24-bit block length
    - Reject forbidden type 127
    - _Requirements: 41_
  - [ ] 10.3 Implement STREAMINFO parser
    - Parse minimum/maximum block size
    - Parse minimum/maximum frame size
    - Parse 20-bit sample rate
    - Parse 3-bit channel count
    - Parse 5-bit bits per sample
    - Parse 36-bit total samples
    - Parse 128-bit MD5 checksum
    - Validate constraints
    - _Requirements: 42, 58_
  - [ ] 10.4 Implement seek table parser
    - Parse seek point count from block length
    - Parse 64-bit sample number per point
    - Parse 64-bit byte offset per point
    - Parse 16-bit frame sample count per point
    - Handle placeholder points (0xFFFFFFFFFFFFFFFF)
    - Validate ascending sample number order
    - _Requirements: 43_
  - [ ] 10.5 Implement Vorbis comment parser
    - Parse 32-bit vendor string length (little-endian)
    - Parse UTF-8 vendor string
    - Parse 32-bit field count (little-endian)
    - Parse field length and content for each field
    - Validate field name format (printable ASCII except =)
    - Support case-insensitive field name comparison
    - _Requirements: 44_
  - [ ] 10.6 Implement padding and application block handling
    - Skip padding blocks (verify zeros)
    - Parse application ID (32-bit)
    - Skip or parse application data
    - _Requirements: 45_
  - [ ] 10.7 Implement picture metadata parser
    - Parse picture type, MIME type, description
    - Parse width, height, depth, color count
    - Parse picture data length and data
    - _Requirements: 46_
  - [ ] 10.8 Implement cuesheet metadata parser
    - Parse media catalog number, lead-in
    - Parse CD flag and track count
    - Parse track offsets, numbers, ISRC
    - Parse index points
    - _Requirements: 47_

## Phase 7: Core Decoder Integration

- [ ] 11. Implement NativeFLACDecoder main class
  - [ ] 11.1 Create NativeFLACDecoder class structure
    - Define member variables for all components
    - Define state management members
    - Define buffer members
    - Define threading mutexes
    - _Requirements: 14, 15, 64_
  - [ ] 11.2 Implement AudioCodec interface methods
    - Implement initialize() with lock acquisition
    - Implement decode() with lock acquisition
    - Implement flush() with lock acquisition
    - Implement reset() with lock acquisition
    - Implement canDecode() with lock acquisition
    - _Requirements: 14_
  - [ ] 11.3 Implement unlocked internal methods
    - Implement initialize_unlocked()
    - Implement decode_unlocked()
    - Implement flush_unlocked()
    - Implement reset_unlocked()
    - Implement canDecode_unlocked()
    - _Requirements: 13, 15_
  - [ ] 11.4 Implement component initialization
    - Create BitstreamReader instance
    - Create FrameParser instance
    - Create SubframeDecoder instance
    - Create ResidualDecoder instance
    - Create ChannelDecorrelator instance
    - Create SampleReconstructor instance
    - Create CRCValidator instance
    - Create MetadataParser instance
    - _Requirements: 14_
  - [ ] 11.5 Implement buffer allocation
    - Allocate input buffer (64KB)
    - Allocate decode buffers per channel
    - Allocate output buffer
    - Base sizes on maximum block size
    - Handle allocation failures gracefully
    - _Requirements: 65_
  - [ ] 11.6 Implement decode pipeline
    - Feed input data to bitstream reader
    - Find and parse frame sync
    - Parse frame header
    - Decode subframes for each channel
    - Apply channel decorrelation
    - Reconstruct samples with bit depth conversion
    - Validate frame footer CRC
    - Return AudioFrame with decoded samples
    - _Requirements: 1-10, 14_

## Phase 8: Advanced Features

- [ ] 12. Implement seeking support
  - Use seek table for fast seeking
  - Reset decoder state for seek
  - Support sample-accurate positioning
  - _Requirements: 43_

- [ ] 13. Implement MD5 validation
  - Compute MD5 of decoded samples
  - Interleave channels for MD5 input
  - Use little-endian byte order
  - Sign-extend non-byte-aligned samples
  - Compare with STREAMINFO MD5
  - _Requirements: 25_

- [ ] 14. Implement streamable subset support
  - Handle frames without STREAMINFO references
  - Extract sample rate from frame header
  - Extract bit depth from frame header
  - Support mid-stream synchronization
  - _Requirements: 19_

- [ ] 15. Implement Ogg FLAC support
  - Parse Ogg FLAC header packet
  - Verify 0x7F 0x46 0x4C 0x41 0x43 signature
  - Parse version number and header count
  - Parse metadata from header packets
  - Decode FLAC frames from audio packets
  - Handle granule position for sample numbering
  - Support chained Ogg streams
  - _Requirements: 49_

- [ ] 16. Implement ID3v2 tag skipping
  - Detect "ID3" signature at file start
  - Parse ID3v2 tag header for size
  - Skip tag bytes
  - Search for fLaC marker after tag
  - _Requirements: 63_

## Phase 9: Error Handling and Validation

- [ ] 17. Implement error handling infrastructure
  - [ ] 17.1 Define error types and exceptions
    - Create FLACError enum
    - Create FLACException class
    - Define error messages
    - _Requirements: 11_
  - [ ] 17.2 Implement error recovery
    - Handle sync loss with frame search
    - Handle header CRC failure with skip
    - Handle frame CRC failure with warning
    - Handle subframe error with silence output
    - Handle memory errors with cleanup
    - _Requirements: 11_
  - [ ] 17.3 Implement state management
    - Track decoder state transitions
    - Handle error state
    - Support reset from error state
    - _Requirements: 64_

- [ ] 18. Implement validation and security
  - [ ] 18.1 Implement bounds checking
    - Validate all array accesses
    - Check buffer sizes before operations
    - Prevent integer overflow
    - _Requirements: 48_
  - [ ] 18.2 Implement resource limits
    - Enforce maximum block size
    - Limit partition order
    - Timeout sync search
    - Limit memory allocation
    - _Requirements: 48_
  - [ ] 18.3 Implement input validation
    - Validate all header fields
    - Reject forbidden patterns
    - Check sample value ranges
    - Validate block size constraints
    - _Requirements: 23, 57, 58_

## Phase 10: Testing and Validation

- [ ] 19. Create unit tests
  - [ ] 19.1 Test BitstreamReader
    - Test bit reading accuracy
    - Test unary decoding
    - Test UTF-8 number decoding
    - Test Rice code decoding
    - Test buffer management
  - [ ] 19.2 Test FrameParser
    - Test sync detection
    - Test header parsing
    - Test CRC validation
    - Test uncommon parameters
  - [ ] 19.3 Test SubframeDecoder
    - Test CONSTANT subframe
    - Test VERBATIM subframe
    - Test FIXED predictors (orders 0-4)
    - Test LPC predictor
    - Test wasted bits handling
  - [ ] 19.4 Test ResidualDecoder
    - Test Rice code decoding
    - Test partition handling
    - Test escaped partitions
    - Test zigzag encoding
  - [ ] 19.5 Test ChannelDecorrelator
    - Test left-side stereo
    - Test right-side stereo
    - Test mid-side stereo
    - Test independent channels
  - [ ] 19.6 Test SampleReconstructor
    - Test bit depth conversions
    - Test channel interleaving
    - Test sample validation

- [ ] 20. Create integration tests
  - [ ] 20.1 Test RFC example files
    - Decode example 1 from RFC Appendix D.1
    - Decode example 2 from RFC Appendix D.2
    - Decode example 3 from RFC Appendix D.3
    - Verify MD5 checksums match
  - [ ] 20.2 Test real FLAC files
    - Test files from tests/data directory
    - Compare output with libFLAC
    - Verify bit-perfect decoding
  - [ ] 20.3 Test container formats
    - Test native FLAC files
    - Test Ogg FLAC streams
    - Test files from various encoders
  - [ ] 20.4 Test edge cases
    - Test variable block sizes
    - Test uncommon bit depths
    - Test multi-channel audio
    - Test highly compressed frames

- [ ] 21. Create performance tests
  - [ ] 21.1 Benchmark against libFLAC
    - Test 44.1kHz/16-bit decoding speed
    - Test 96kHz/24-bit decoding speed
    - Measure CPU usage
  - [ ] 21.2 Test memory usage
    - Measure peak memory consumption
    - Profile allocation patterns
  - [ ] 21.3 Test threading
    - Test concurrent decoder instances
    - Measure lock contention

- [ ] 22. Security testing
  - [ ] 22.1 Fuzz testing
    - Run AFL fuzzer on decoder
    - Run libFuzzer on decoder
    - Test with malformed files
  - [ ] 22.2 Sanitizer testing
    - Build with AddressSanitizer
    - Build with UndefinedBehaviorSanitizer
    - Build with ThreadSanitizer
  - [ ] 22.3 Security audit
    - Review bounds checking
    - Review resource limits
    - Review error handling

## Phase 11: Documentation and Polish

- [ ] 23. Create documentation
  - Document public API
  - Document build options
  - Document performance characteristics
  - Create usage examples

- [ ] 24. Code review and cleanup
  - Review code for clarity
  - Add inline comments
  - Optimize hot paths
  - Remove debug code

- [ ] 25. Final validation
  - Run complete test suite
  - Verify all requirements met
  - Benchmark performance
  - Test on multiple platforms

## Notes

- All tasks are required for comprehensive implementation
- Each task should be completed and tested before moving to the next
- Commit code after completing each major task
- Update this task list as implementation progresses
- Refer to requirements.md and design.md for detailed specifications
