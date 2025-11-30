# Implementation Plan

- [x] 1. Create VorbisCodec Class Structure
  - Implement VorbisCodec class inheriting from AudioCodec base class
  - Add private member variables for libvorbis structures (info, comment, dsp_state, block)
  - Implement constructor accepting StreamInfo parameter
  - Add destructor with proper libvorbis cleanup (vorbis_block_clear, vorbis_dsp_clear, vorbis_comment_clear, vorbis_info_clear)
  - _Requirements: 11.1, 2.8_

- [x] 2. Implement Core AudioCodec Interface Methods
  - [x] 2.1 Implement initialize() method
    - Initialize libvorbis structures (vorbis_info_init, vorbis_comment_init)
    - Extract Vorbis parameters from StreamInfo (channels, sample rate, codec data)
    - Set up internal buffers and state variables
    - Return success/failure status
    - _Requirements: 2.1, 11.2, 6.2_

  - [x] 2.2 Implement canDecode() method
    - Check if StreamInfo contains "vorbis" codec name
    - Validate basic Vorbis stream parameters
    - Return boolean indicating decode capability
    - _Requirements: 11.6, 6.6_

  - [x] 2.3 Implement getCodecName() method
    - Return "vorbis" string identifier
    - _Requirements: 11.6_

- [x] 3. Implement Header Processing System
  - [x] 3.1 Create header packet detection and routing
    - Implement processHeaderPacket() method to route header types
    - Add header packet counter and state tracking (3 headers required)
    - Validate header packet sequence (identification \x01vorbis, comment \x03vorbis, setup \x05vorbis)
    - _Requirements: 1.1, 2.2_

  - [x] 3.2 Write property test for header sequence validation
    - **Property 1: Header Sequence Validation**
    - **Validates: Requirements 1.1**

  - [x] 3.3 Implement Vorbis identification header parsing
    - Create processIdentificationHeader() using vorbis_synthesis_headerin()
    - Extract version, channels, rate, bitrate_upper, bitrate_nominal, bitrate_lower from vorbis_info
    - Store configuration parameters for decoder initialization
    - Validate parameters against Vorbis specification limits
    - _Requirements: 1.2, 2.2, 4.1, 4.2_

  - [x] 3.4 Write property test for identification header field extraction
    - **Property 2: Identification Header Field Extraction**
    - **Validates: Requirements 1.2**

  - [x] 3.5 Implement comment header processing
    - Create processCommentHeader() using vorbis_synthesis_headerin()
    - Validate comment header structure without processing metadata
    - Make vorbis_comment data available to demuxer for metadata extraction
    - _Requirements: 1.3, 14.1, 14.2_

  - [x] 3.6 Implement setup header processing
    - Create processSetupHeader() using vorbis_synthesis_headerin()
    - Initialize decoder with codebook and floor/residue configurations
    - Complete libvorbis decoder initialization after all headers
    - _Requirements: 1.4, 2.3_

  - [x] 3.7 Write property test for block size constraint
    - **Property 6: Block Size Constraint**
    - **Validates: Requirements 4.1, 4.2**

- [x] 4. Implement libvorbis Decoder Integration
  - [x] 4.1 Create libvorbis decoder initialization
    - Initialize vorbis_dsp_state using vorbis_synthesis_init() after headers
    - Initialize vorbis_block using vorbis_block_init() for packet processing
    - Handle decoder initialization errors and cleanup
    - _Requirements: 2.3, 2.4, 2.5_

  - [x] 4.2 Implement decoder state management
    - Add proper cleanup in destructor (vorbis_block_clear, vorbis_dsp_clear, etc.)
    - Implement reset() method using vorbis_synthesis_restart() for seeking
    - Handle decoder state validation and error recovery
    - _Requirements: 2.7, 2.8, 8.7_

  - [x] 4.3 Write property test for reset preserves headers
    - **Property 5: Reset Preserves Headers**
    - **Validates: Requirements 2.7, 6.4**

- [x] 5. Checkpoint - Verify header processing works
  - Ensure all tests pass, ask the user if questions arise.

- [x] 6. Implement Audio Packet Decoding
  - [x] 6.1 Create main decode() method implementation
    - Route header packets to header processing system
    - Route audio packets to audio decoding system
    - Handle empty packets and end-of-stream conditions
    - Return appropriate AudioFrame or empty frame
    - _Requirements: 11.3, 1.5, 6.1_

  - [x] 6.2 Implement audio packet decoding core
    - Create decodeAudioPacket() using vorbis_synthesis() and vorbis_synthesis_blockin()
    - Extract PCM samples using vorbis_synthesis_pcmout() and vorbis_synthesis_read()
    - Handle variable block sizes via vorbis_info_blocksize()
    - Convert float samples to 16-bit PCM format
    - _Requirements: 1.5, 1.6, 2.4, 2.5_

  - [x] 6.3 Write property test for MediaChunk to AudioFrame conversion
    - **Property 15: MediaChunk to AudioFrame Conversion**
    - **Validates: Requirements 11.3**

  - [x] 6.4 Add packet validation and error handling
    - Implement validateVorbisPacket() for basic packet validation
    - Handle corrupted packets by skipping and continuing
    - Implement handleVorbisError() for libvorbis error code processing
    - _Requirements: 1.8, 8.1, 8.2, 8.3_

  - [x] 6.5 Write property test for corrupted packet recovery
    - **Property 3: Corrupted Packet Recovery**
    - **Validates: Requirements 1.8, 8.3**

  - [x] 6.6 Write property test for error code handling
    - **Property 4: Error Code Handling**
    - **Validates: Requirements 2.6**

- [x] 7. Implement Float to PCM Conversion
  - [x] 7.1 Create float to 16-bit PCM conversion system
    - Implement convertFloatToPCM() method for libvorbis float output
    - Handle proper clamping (-1.0 to 1.0 → -32768 to 32767) and scaling
    - Interleave channels correctly during conversion
    - _Requirements: 1.5, 5.1, 5.2_

  - [x] 7.2 Add channel interleaving and formatting
    - Process multi-channel float arrays into interleaved 16-bit output
    - Handle channel mapping according to Vorbis conventions
    - Ensure proper sample alignment and channel ordering
    - _Requirements: 5.5, 5.7_

  - [x] 7.3 Write property test for channel count consistency
    - **Property 8: Channel Count Consistency**
    - **Validates: Requirements 5.1, 5.2, 5.3, 5.5**

  - [x] 7.4 Write property test for channel interleaving correctness
    - **Property 9: Channel Interleaving Correctness**
    - **Validates: Requirements 5.5, 5.7**

- [x] 8. Checkpoint - Verify basic decoding works
  - Ensure all tests pass, ask the user if questions arise.

- [x] 9. Implement Streaming and Buffering
  - [x] 9.1 Create output buffer management
    - Implement bounded output buffers (max 2 seconds at 48kHz stereo)
    - Handle buffer overflow with appropriate backpressure
    - Process packets incrementally without requiring complete file access
    - _Requirements: 7.1, 7.2, 7.4_

  - [x] 9.2 Write property test for bounded buffer size
    - **Property 11: Bounded Buffer Size**
    - **Validates: Requirements 7.2, 7.4**

  - [x] 9.3 Write property test for incremental processing
    - **Property 12: Incremental Processing**
    - **Validates: Requirements 7.1**

  - [x] 9.4 Add streaming support and buffer flushing
    - Implement flush() method to output remaining decoded samples
    - Handle partial packet data gracefully
    - Clear all internal buffers during reset operations
    - _Requirements: 7.3, 7.5, 7.6_

  - [x] 9.5 Write property test for flush outputs remaining samples
    - **Property 7: Flush Outputs Remaining Samples**
    - **Validates: Requirements 4.8, 7.5, 11.4**

  - [x] 9.6 Write property test for reset clears state
    - **Property 16: Reset Clears State**
    - **Validates: Requirements 7.6, 11.5**

- [x] 10. Implement Container-Agnostic Operation
  - [x] 10.1 Ensure container-agnostic decoding
    - Decode based on packet data only, not container details
    - Work with MediaChunk data regardless of source
    - Reset state without requiring container-specific operations
    - _Requirements: 6.1, 6.3, 6.4_

  - [x] 10.2 Write property test for container-agnostic decoding
    - **Property 10: Container-Agnostic Decoding**
    - **Validates: Requirements 6.1, 6.3**

- [x] 11. Implement Error Handling and Recovery
  - [x] 11.1 Create comprehensive error handling system
    - Handle OV_ENOTVORBIS by rejecting packet as not Vorbis data
    - Handle OV_EBADHEADER by rejecting initialization
    - Handle OV_EFAULT by resetting decoder state
    - Throw BadFormatException on memory allocation failures
    - _Requirements: 8.1, 8.2, 8.5, 8.6_

  - [x] 11.2 Add error recovery and state management
    - Skip corrupted packets (vorbis_synthesis returns non-zero) and continue
    - Log errors via vorbis_synthesis_blockin failures
    - Call vorbis_synthesis_restart() on state inconsistency
    - Report errors through PsyMP3's Debug logging system
    - _Requirements: 8.3, 8.4, 8.7, 8.8_

- [x] 12. Checkpoint - Verify error handling works
  - Ensure all tests pass, ask the user if questions arise.

- [x] 13. Implement Thread Safety
  - [x] 13.1 Ensure thread-safe codec operation
    - Maintain independent libvorbis state per codec instance
    - Use appropriate synchronization for shared resources if any
    - Handle concurrent initialization and cleanup safely
    - _Requirements: 10.1, 10.2, 10.3, 10.5, 10.6_

  - [x] 13.2 Write property test for instance independence
    - **Property 13: Instance Independence**
    - **Validates: Requirements 10.1, 10.2**

  - [x] 13.3 Write property test for concurrent initialization safety
    - **Property 14: Concurrent Initialization Safety**
    - **Validates: Requirements 10.5**

  - [x] 13.4 Add thread-safe error handling and logging
    - Implement thread-safe error state management
    - Use PsyMP3's thread-safe Debug logging system
    - _Requirements: 10.7, 10.8_

- [x] 14. Register Codec with Factory System
  - Register VorbisCodec with AudioCodecFactory for "vorbis" codec name
  - Ensure proper codec selection and instantiation
  - Test factory integration with demuxer system
  - _Requirements: 11.6_

- [x] 15. Integration Testing and Validation
  - [x] 15.1 Test integration with demuxer architecture
    - Verify codec works with OggDemuxer for Ogg Vorbis files
    - Test MediaChunk processing and AudioFrame output format
    - Validate seeking support through reset() method
    - Test integration with DemuxedStream bridge interface
    - _Requirements: 6.1, 6.3, 11.3, 11.4, 12.8_

  - [x] 15.2 Validate compatibility and performance
    - Test with various Vorbis files from different encoders (oggenc, etc.)
    - Verify equivalent or better performance than existing implementation
    - Test all quality levels (-1 to 10) and encoding configurations
    - Validate output quality and accuracy against libvorbis reference
    - _Requirements: 12.1, 12.2, 12.4, 13.1, 13.2, 13.8_

- [ ] 16. Final Checkpoint - Make sure all tests are passing
  - Ensure all tests pass, ask the user if questions arise.
