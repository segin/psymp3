# Implementation Plan

- [x] 1. Create OpusCodec Class Structure
  - [x] 1.1 Implement OpusCodec class inheriting from AudioCodec base class
    - Add private member variables for libopus decoder state and configuration
    - Implement constructor accepting StreamInfo parameter
    - Add destructor with proper libopus cleanup
    - _Requirements: 11.1, 11.2_

  - [x] 1.2 Write property test for header parsing round-trip
    - **Property 1: Header Parsing Round-Trip Consistency**
    - **Validates: Requirements 1.2, 16.1, 16.2, 16.3**

- [x] 2. Implement Core AudioCodec Interface Methods
  - [x] 2.1 Implement initialize() method
    - Extract Opus parameters from StreamInfo (channels, sample rate, codec data)
    - Validate parameters against Opus specification limits
    - Set up internal buffers and state variables
    - Return success/failure status
    - _Requirements: 11.2, 6.2_

  - [x] 2.2 Implement canDecode() and getCodecName() methods
    - Check if StreamInfo contains "opus" codec name
    - Return "opus" string identifier for getCodecName()
    - _Requirements: 11.6_

- [x] 3. Implement Header Processing System
  - [x] 3.1 Create header packet detection and routing
    - Implement processHeaderPacket() method to route header types
    - Add header packet counter and state tracking
    - Validate header packet sequence (ID header first, then comments)
    - _Requirements: 1.1, 16.7_

  - [x] 3.2 Write property test for header sequence validation
    - **Property 4: Header Sequence Validation**
    - **Validates: Requirements 1.1, 16.7**

  - [x] 3.3 Implement Opus identification header parsing
    - Create processIdentificationHeader() method
    - Parse OpusHead header format (version, channels, pre-skip, gain, etc.)
    - Extract fields in correct byte order (little-endian) per RFC 7845
    - Store configuration parameters for decoder initialization
    - _Requirements: 1.2, 16.1, 16.2, 16.3_

  - [x] 3.4 Implement comment header processing
    - Create processCommentHeader() method for OpusTags header
    - Validate vendor string length and comment entry lengths
    - Make header data available to demuxer for metadata extraction
    - _Requirements: 1.3, 14.1, 14.2, 16.5, 16.6_

  - [x] 3.5 Write property test for comment header validation
    - **Property 13: Comment Header Structure Validation**
    - **Validates: Requirements 1.3, 14.2, 16.5, 16.6**

  - [x] 3.6 Write property test for invalid header rejection
    - **Property 6: Invalid Header Rejection**
    - **Validates: Requirements 8.1, 16.8**

- [x] 4. Checkpoint - Verify header processing
  - All header processing tests pass.

- [x] 5. Implement libopus Decoder Integration
  - [x] 5.1 Create libopus decoder initialization
    - Implement initializeOpusDecoder() method using opus_decoder_create()
    - Configure decoder with proper channel count and sample rate (48kHz)
    - Handle multi-stream decoder setup for surround sound configurations
    - Add proper error handling for decoder creation failures
    - _Requirements: 2.1, 2.4, 4.1, 4.2_

  - [x] 5.2 Implement decoder state management
    - Add proper cleanup in destructor using opus_decoder_destroy()
    - Implement reset() method using opus_decoder_ctl(OPUS_RESET_STATE)
    - Restore pre-skip counter to original header value on reset
    - Handle decoder state validation and error recovery
    - _Requirements: 2.5, 2.6, 2.7, 15.1, 15.3_

  - [x] 5.3 Write property test for reset state consistency
    - **Property 7: Reset State Consistency**
    - **Validates: Requirements 15.1, 15.4**

- [x] 6. Implement TOC Byte Parsing
  - [x] 6.1 Create TOC byte parser
    - Extract configuration bits (3-7) for mode identification
    - Extract stereo flag (bit 2) for channel configuration
    - Extract frame count code (bits 0-1) for packet structure
    - Identify SILK-only (0-15), Hybrid (16-19), and CELT-only (20-31) modes
    - _Requirements: 17.1, 17.2, 17.3, 17.4_

  - [x] 6.2 Write property test for TOC byte parsing
    - **Property 10: TOC Byte Parsing Correctness**
    - **Validates: Requirements 17.1, 17.2, 17.3, 17.4**

  - [x] 6.3 Implement TOC validation
    - Reject packets with invalid TOC configurations
    - Handle frame duration and bandwidth changes between packets
    - _Requirements: 17.5, 17.6, 17.7, 17.8_

- [x] 7. Implement Audio Packet Decoding
  - [x] 7.1 Create main decode() method implementation
    - Route header packets to header processing system
    - Route audio packets to audio decoding system
    - Handle empty packets and end-of-stream conditions
    - Return appropriate AudioFrame or empty frame
    - _Requirements: 11.3, 1.4, 6.1_

  - [x] 7.2 Implement audio packet decoding core
    - Create decodeAudioPacket() method using opus_decode()
    - Handle variable frame sizes (2.5ms to 60ms) efficiently
    - Process SILK, CELT, and hybrid mode packets correctly
    - Convert libopus output to 16-bit PCM format
    - _Requirements: 1.4, 1.5, 3.1, 3.2, 3.3, 3.6_

  - [x] 7.3 Write property test for variable frame size support
    - **Property 9: Variable Frame Size Support**
    - **Validates: Requirements 1.5, 3.6**

  - [x] 7.4 Add packet validation and error handling
    - Implement validateOpusPacket() for basic packet validation
    - Handle corrupted packets by skipping and continuing
    - Implement handleDecoderError() for libopus error code processing
    - Output silence for failed frames while maintaining stream continuity
    - _Requirements: 1.8, 8.2, 8.3, 8.4_

  - [x] 7.5 Write property test for corrupted packet resilience
    - **Property 5: Corrupted Packet Resilience**
    - **Validates: Requirements 1.8, 8.2, 8.3**

- [x] 8. Checkpoint - Verify core decoding
  - All core decoding tests pass.

- [x] 9. Implement Pre-skip and Gain Processing
  - [x] 9.1 Create pre-skip sample removal system
    - Implement applyPreSkip() method to remove initial samples
    - Track samples to skip using atomic counter
    - Handle pre-skip across multiple frames correctly
    - Ensure proper sample alignment after pre-skip application
    - _Requirements: 1.7, 5.1, 5.3, 5.5, 5.8_

  - [x] 9.2 Write property test for pre-skip accuracy
    - **Property 2: Pre-skip Sample Removal Accuracy**
    - **Validates: Requirements 1.7, 5.1**

  - [x] 9.3 Implement output gain adjustment
    - Create applyOutputGain() method for Q7.8 format gain
    - Apply gain using formula: sample *= pow(10, gain/(20.0*256))
    - Clamp output to valid 16-bit range
    - Handle zero gain case efficiently (no processing)
    - _Requirements: 5.2, 5.4, 5.7, 13.4_

  - [x] 9.4 Write property test for output gain correctness
    - **Property 3: Output Gain Application Correctness**
    - **Validates: Requirements 5.2, 13.4**

- [x] 10. Implement Multi-Channel Support
  - [x] 10.1 Add channel configuration handling
    - Support mono and stereo configurations (channel mapping family 0)
    - Handle surround sound configurations (channel mapping family 1)
    - Support custom mapping (channel mapping family 255)
    - Validate channel count against Opus specification limits (1-255)
    - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5_

  - [x] 10.2 Implement channel mapping and ordering
    - Follow Opus/Vorbis channel ordering conventions for multi-channel output
    - Provide properly interleaved channel data in AudioFrame
    - Handle unsupported channel configurations with appropriate errors
    - _Requirements: 4.6, 4.7, 4.8_

- [x] 11. Implement End Trimming Support
  - [x] 11.1 Create end trimming system
    - Accept end position from demuxer via MediaChunk metadata
    - Calculate exact sample count using granule position
    - Decode full final packet and trim excess samples
    - Ensure no audio artifacts at trim point
    - _Requirements: 18.1, 18.2, 18.3, 18.4, 18.5_

  - [x] 11.2 Write property test for end trimming accuracy
    - **Property 11: End Trimming Accuracy**
    - **Validates: Requirements 18.1, 18.2, 18.3**

  - [x] 11.3 Handle end trimming edge cases
    - Handle end position before pre-skip completion
    - Handle end position at exact frame boundary
    - _Requirements: 18.6_

- [x] 12. Implement Packet Loss Concealment
  - [x] 12.1 Create PLC system
    - Detect missing or corrupted packets
    - Call opus_decode with NULL packet data for PLC
    - Generate replacement audio with correct sample count
    - Maintain correct sample timing and position tracking
    - _Requirements: 20.1, 20.2, 20.3, 20.4_

  - [x] 12.2 Write property test for PLC sample count
    - **Property 12: PLC Sample Count Consistency**
    - **Validates: Requirements 20.1, 20.4**

  - [x] 12.3 Implement PLC recovery and limits
    - Seamlessly transition back to normal decoding after PLC
    - Apply same gain and channel processing to PLC audio
    - Output silence for excessive packet loss
    - _Requirements: 20.5, 20.6, 20.7_

- [x] 13. Implement Forward Error Correction Support
  - [x] 13.1 Create FEC decoding system
    - Detect FEC data availability in subsequent packets
    - Call opus_decode with FEC flag when recovering lost packets
    - Fall back to PLC when FEC data unavailable
    - Maintain correct timing and sample alignment
    - _Requirements: 21.1, 21.2, 21.3, 21.4, 21.5, 21.6_

- [x] 14. Checkpoint - Verify advanced features
  - All advanced feature tests pass.

- [x] 15. Implement Seeking Pre-roll Support
  - [x] 15.1 Create pre-roll handling system
    - Support decoding 3840 samples (80ms) pre-roll before seek target
    - Discard pre-roll samples, output only from seek target
    - Handle seek target within 80ms of stream start
    - Accept pre-roll packets without outputting audio
    - _Requirements: 19.1, 19.2, 19.3, 19.6_

  - [x] 15.2 Implement pre-roll edge cases
    - Clear decoder state on reset for proper convergence
    - Handle insufficient pre-roll gracefully
    - _Requirements: 19.4, 19.5_

- [x] 16. Implement Buffering and Flow Control
  - [x] 16.1 Create output buffer management
    - Implement bounded output buffers to prevent memory exhaustion
    - Handle buffer overflow with appropriate backpressure
    - Process packets incrementally without requiring complete file access
    - _Requirements: 7.1, 7.2, 7.4_

  - [x] 16.2 Add streaming support and buffer flushing
    - Implement flush() method to output remaining decoded samples
    - Handle partial packet data gracefully
    - Maintain consistent latency and throughput for continuous streaming
    - _Requirements: 7.3, 7.5, 7.8, 11.4_

- [x] 17. Implement Error Handling and Recovery
  - [x] 17.1 Create comprehensive error handling system
    - Handle invalid header packets with proper error reporting
    - Implement graceful handling of memory allocation failures
    - Validate input parameters and reject invalid data appropriately
    - _Requirements: 8.1, 8.5, 8.6_

  - [x] 17.2 Add error recovery and state management
    - Reset decoder state when encountering unrecoverable errors
    - Maintain thread-local error state for concurrent operation
    - Provide clear error reporting through PsyMP3's error mechanisms
    - _Requirements: 8.7, 8.8, 11.7, 15.8_

- [x] 18. Implement Thread Safety
  - [x] 18.1 Ensure thread-safe codec operation
    - Maintain independent libopus state per codec instance
    - Use appropriate synchronization (public/private lock pattern)
    - Handle concurrent initialization and cleanup safely
    - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.5, 10.6_

  - [x] 18.2 Write property test for thread instance independence
    - **Property 8: Thread Instance Independence**
    - **Validates: Requirements 10.1, 10.2**

  - [x] 18.3 Add thread-safe error handling and logging
    - Implement thread-safe error state management
    - Use PsyMP3's thread-safe Debug logging system
    - Ensure proper cleanup before codec destruction
    - _Requirements: 10.7, 10.8, 11.8_

- [x] 19. Implement Performance Optimizations
  - [x] 19.1 Optimize memory usage and CPU efficiency
    - Use appropriately sized buffers for maximum Opus frame size (5760 samples)
    - Leverage libopus built-in optimizations and SIMD support
    - Minimize allocation overhead and memory fragmentation
    - _Requirements: 9.1, 9.2, 9.3, 9.6_

  - [x] 19.2 Add efficient processing for common cases
    - Optimize for mono and stereo configurations
    - Handle variable frame sizes and bitrate changes efficiently
    - Optimize memory access patterns for cache efficiency
    - _Requirements: 9.4, 9.5, 9.7, 9.8_

- [x] 20. Checkpoint - Verify all core functionality
  - All core functionality tests pass.

- [x] 21. Create Comprehensive Unit Tests
  - [x] 21.1 Test core decoding functionality
    - Write tests for Opus header processing (ID and comment headers)
    - Test audio packet decoding with various frame sizes and modes
    - Verify pre-skip and gain processing correctness
    - Test multi-channel configurations and channel mapping
    - _Requirements: 1.1, 1.2, 1.3, 1.5, 5.1, 5.2, 4.1, 4.2_

  - [x] 21.2 Test error handling and edge cases
    - Test handling of corrupted packets and invalid headers
    - Verify error recovery and decoder state reset functionality
    - Test memory allocation failure scenarios
    - _Requirements: 8.1, 8.2, 8.7_

- [x] 22. Integration Testing and Validation
  - [x] 22.1 Test integration with demuxer architecture
    - Verify codec works with OggDemuxer for Ogg Opus files
    - Test MediaChunk processing and AudioFrame output format
    - Validate seeking support through reset() method
    - Test integration with DemuxedStream bridge interface
    - _Requirements: 6.1, 6.3, 11.3, 11.4, 12.8_

  - [x] 22.2 Validate compatibility and performance
    - Test with various Opus files from different encoders
    - Verify equivalent or better performance than existing implementation
    - Test all quality levels and encoding modes (SILK, CELT, hybrid)
    - Validate output quality and accuracy against reference implementation
    - _Requirements: 12.1, 12.2, 12.4, 13.1, 13.2, 13.8_

- [x] 23. Register Codec with Factory System
  - Register OpusCodec with AudioCodecFactory for "opus" codec name
  - Ensure proper codec selection and instantiation
  - Test factory integration with demuxer system
  - _Requirements: 11.6_

- [x] 24. Final Checkpoint - Verify complete implementation
  - All tests pass, implementation complete.