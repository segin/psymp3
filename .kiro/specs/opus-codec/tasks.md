# Implementation Plan

- [x] 1. Create OpusCodec Class Structure
  - Implement OpusCodec class inheriting from AudioCodec base class
  - Add private member variables for libopus decoder state and configuration
  - Implement constructor accepting StreamInfo parameter
  - Add destructor with proper libopus cleanup
  - _Requirements: 11.1, 11.2_

- [x] 2. Implement Core AudioCodec Interface Methods
  - [x] 2.1 Implement initialize() method
    - Extract Opus parameters from StreamInfo (channels, sample rate, codec data)
    - Validate parameters against Opus specification limits
    - Set up internal buffers and state variables
    - Return success/failure status
    - _Requirements: 11.2, 6.2_

  - [x] 2.2 Implement canDecode() method
    - Check if StreamInfo contains "opus" codec name
    - Validate basic Opus stream parameters
    - Return boolean indicating decode capability
    - _Requirements: 11.6_

  - [x] 2.3 Implement getCodecName() method
    - Return "opus" string identifier
    - _Requirements: 11.6_

- [ ] 3. Implement Header Processing System
  - [ ] 3.1 Create header packet detection and routing
    - Implement processHeaderPacket() method to route header types
    - Add header packet counter and state tracking
    - Validate header packet sequence (ID header first, then comments)
    - _Requirements: 1.1, 1.2, 1.3_

  - [ ] 3.2 Implement Opus identification header parsing
    - Create processIdentificationHeader() method
    - Parse OpusHead header format (version, channels, pre-skip, gain, etc.)
    - Extract and validate channel count, pre-skip, and output gain
    - Store configuration parameters for decoder initialization
    - _Requirements: 1.2, 5.1, 5.2, 5.3_

  - [ ] 3.3 Implement comment header processing
    - Create processCommentHeader() method for OpusTags header
    - Validate comment header structure without processing metadata
    - Make header data available to demuxer for metadata extraction
    - _Requirements: 1.3, 14.1, 14.2, 14.4_

- [x] 4. Implement libopus Decoder Integration
  - [x] 4.1 Create libopus decoder initialization
    - Implement initializeOpusDecoder() method using opus_decoder_create()
    - Configure decoder with proper channel count and sample rate (48kHz)
    - Handle multi-stream decoder setup for surround sound configurations
    - Add proper error handling for decoder creation failures
    - _Requirements: 2.1, 2.4, 4.1, 4.2_

  - [x] 4.2 Implement decoder state management
    - Add proper cleanup in destructor using opus_decoder_destroy()
    - Implement reset() method using opus_decoder_ctl() for seeking support
    - Handle decoder state validation and error recovery
    - _Requirements: 2.5, 2.7, 8.7_

- [ ] 5. Implement Audio Packet Decoding
  - [ ] 5.1 Create main decode() method implementation
    - Route header packets to header processing system
    - Route audio packets to audio decoding system
    - Handle empty packets and end-of-stream conditions
    - Return appropriate AudioFrame or empty frame
    - _Requirements: 11.3, 1.5, 6.1_

  - [ ] 5.2 Implement audio packet decoding core
    - Create decodeAudioPacket() method using opus_decode()
    - Handle variable frame sizes (2.5ms to 60ms) efficiently
    - Process SILK, CELT, and hybrid mode packets correctly
    - Convert libopus output to 16-bit PCM format
    - _Requirements: 1.5, 3.1, 3.2, 3.3, 3.6_

  - [ ] 5.3 Add packet validation and error handling
    - Implement validateOpusPacket() for basic packet validation
    - Handle corrupted packets by skipping and continuing
    - Implement handleDecoderError() for libopus error code processing
    - Output silence for failed frames while maintaining stream continuity
    - _Requirements: 8.2, 8.3, 8.4, 1.8_

- [x] 6. Implement Pre-skip and Gain Processing
  - [x] 6.1 Create pre-skip sample removal system
    - Implement applyPreSkip() method to remove initial samples
    - Track samples to skip using atomic counter
    - Handle pre-skip across multiple frames correctly
    - Ensure proper sample alignment after pre-skip application
    - _Requirements: 5.1, 5.3, 5.5, 5.8_

  - [x] 6.2 Implement output gain adjustment
    - Create applyOutputGain() method for Q7.8 format gain
    - Apply gain to decoded samples with proper clamping
    - Handle zero gain case efficiently (no processing)
    - Ensure no audio artifacts from gain application
    - _Requirements: 5.2, 5.4, 5.7_

- [ ] 7. Implement Multi-Channel Support
  - [ ] 7.1 Add channel configuration handling
    - Support mono and stereo configurations (channel mapping family 0)
    - Handle surround sound configurations (channel mapping family 1)
    - Validate channel count against Opus specification limits
    - Process channel coupling for stereo streams correctly
    - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5_

  - [ ] 7.2 Implement channel mapping and ordering
    - Follow Opus channel ordering conventions for multi-channel output
    - Provide properly interleaved channel data in AudioFrame
    - Handle unsupported channel configurations with appropriate errors
    - _Requirements: 4.6, 4.7, 4.8_

- [ ] 8. Implement Buffering and Flow Control
  - [ ] 8.1 Create output buffer management
    - Implement bounded output buffers to prevent memory exhaustion
    - Handle buffer overflow with appropriate backpressure
    - Process packets incrementally without requiring complete file access
    - _Requirements: 7.1, 7.2, 7.4_

  - [ ] 8.2 Add streaming support and buffer flushing
    - Implement flush() method to output remaining decoded samples
    - Handle partial packet data gracefully
    - Maintain consistent latency and throughput for continuous streaming
    - _Requirements: 7.3, 7.5, 7.8_

- [ ] 9. Implement Error Handling and Recovery
  - [ ] 9.1 Create comprehensive error handling system
    - Handle invalid header packets with proper error reporting
    - Implement graceful handling of memory allocation failures
    - Validate input parameters and reject invalid data appropriately
    - _Requirements: 8.1, 8.5, 8.6_

  - [ ] 9.2 Add error recovery and state management
    - Reset decoder state when encountering unrecoverable errors
    - Maintain thread-local error state for concurrent operation
    - Provide clear error reporting through PsyMP3's error mechanisms
    - _Requirements: 8.7, 8.8, 11.7_

- [ ] 10. Implement Performance Optimizations
  - [ ] 10.1 Optimize memory usage and CPU efficiency
    - Use appropriately sized buffers for maximum Opus frame size
    - Leverage libopus built-in optimizations and SIMD support
    - Minimize allocation overhead and memory fragmentation
    - _Requirements: 9.1, 9.2, 9.3, 9.6_

  - [ ] 10.2 Add efficient processing for common cases
    - Optimize for mono and stereo configurations
    - Handle variable frame sizes and bitrate changes efficiently
    - Optimize memory access patterns for cache efficiency
    - _Requirements: 9.4, 9.5, 9.7, 9.8_

- [ ] 11. Implement Thread Safety
  - [ ] 11.1 Ensure thread-safe codec operation
    - Maintain independent libopus state per codec instance
    - Use appropriate synchronization for shared resources
    - Handle concurrent initialization and cleanup safely
    - _Requirements: 10.1, 10.2, 10.3, 10.5, 10.6_

  - [ ] 11.2 Add thread-safe error handling and logging
    - Implement thread-safe error state management
    - Use PsyMP3's thread-safe Debug logging system
    - Ensure proper cleanup before codec destruction
    - _Requirements: 10.7, 10.8, 11.8_

- [ ] 12. Create Comprehensive Unit Tests
  - [ ] 12.1 Test core decoding functionality
    - Write tests for Opus header processing (ID and comment headers)
    - Test audio packet decoding with various frame sizes and modes
    - Verify pre-skip and gain processing correctness
    - Test multi-channel configurations and channel mapping
    - _Requirements: 1.1, 1.2, 1.3, 1.5, 5.1, 5.2, 4.1, 4.2_

  - [ ] 12.2 Test error handling and edge cases
    - Test handling of corrupted packets and invalid headers
    - Verify error recovery and decoder state reset functionality
    - Test memory allocation failure scenarios
    - Verify thread safety with concurrent codec instances
    - _Requirements: 8.1, 8.2, 8.7, 10.1, 10.2_

- [ ] 13. Integration Testing and Validation
  - [ ] 13.1 Test integration with demuxer architecture
    - Verify codec works with OggDemuxer for Ogg Opus files
    - Test MediaChunk processing and AudioFrame output format
    - Validate seeking support through reset() method
    - Test integration with DemuxedStream bridge interface
    - _Requirements: 6.1, 6.3, 11.3, 11.4, 12.8_

  - [ ] 13.2 Validate compatibility and performance
    - Test with various Opus files from different encoders
    - Verify equivalent or better performance than existing implementation
    - Test all quality levels and encoding modes (SILK, CELT, hybrid)
    - Validate output quality and accuracy against reference implementation
    - _Requirements: 12.1, 12.2, 12.4, 13.1, 13.2, 13.8_

- [ ] 14. Register Codec with Factory System
  - Register OpusCodec with AudioCodecFactory for "opus" codec name
  - Ensure proper codec selection and instantiation
  - Test factory integration with demuxer system
  - _Requirements: 11.6_