# Implementation Plan

- [ ] 1. Create VorbisCodec Class Structure
  - Implement VorbisCodec class inheriting from AudioCodec base class
  - Add private member variables for libvorbis structures (info, comment, dsp_state, block)
  - Implement constructor accepting StreamInfo parameter
  - Add destructor with proper libvorbis cleanup
  - _Requirements: 11.1, 11.2_

- [ ] 2. Implement Core AudioCodec Interface Methods
  - [ ] 2.1 Implement initialize() method
    - Initialize libvorbis structures (vorbis_info_init, vorbis_comment_init)
    - Extract Vorbis parameters from StreamInfo (channels, sample rate, codec data)
    - Set up internal buffers and state variables
    - Return success/failure status
    - _Requirements: 11.2, 6.2_

  - [ ] 2.2 Implement canDecode() method
    - Check if StreamInfo contains "vorbis" codec name
    - Validate basic Vorbis stream parameters
    - Return boolean indicating decode capability
    - _Requirements: 11.6_

  - [ ] 2.3 Implement getCodecName() method
    - Return "vorbis" string identifier
    - _Requirements: 11.6_

- [ ] 3. Implement Header Processing System
  - [ ] 3.1 Create header packet detection and routing
    - Implement processHeaderPacket() method to route header types
    - Add header packet counter and state tracking (3 headers required)
    - Validate header packet sequence (ID, comment, setup)
    - _Requirements: 1.1, 1.2, 1.3, 1.4_

  - [ ] 3.2 Implement Vorbis identification header parsing
    - Create processIdentificationHeader() using vorbis_synthesis_headerin()
    - Extract sample rate, channels, bitrate bounds, and block sizes
    - Store configuration parameters for decoder initialization
    - Validate parameters against Vorbis specification limits
    - _Requirements: 1.2, 2.1, 3.4, 4.1, 4.2_

  - [ ] 3.3 Implement comment header processing
    - Create processCommentHeader() using vorbis_synthesis_headerin()
    - Validate comment header structure without processing metadata
    - Make header data available to demuxer for metadata extraction
    - _Requirements: 1.3, 14.1, 14.2, 14.4_

  - [ ] 3.4 Implement setup header processing
    - Create processSetupHeader() using vorbis_synthesis_headerin()
    - Initialize decoder with codebook and floor/residue configurations
    - Complete libvorbis decoder initialization after all headers
    - _Requirements: 1.4, 2.1_

- [ ] 4. Implement libvorbis Decoder Integration
  - [ ] 4.1 Create libvorbis decoder initialization
    - Initialize vorbis_dsp_state using vorbis_synthesis_init() after headers
    - Initialize vorbis_block using vorbis_block_init() for packet processing
    - Handle decoder initialization errors and cleanup
    - _Requirements: 2.1, 2.4, 2.5_

  - [ ] 4.2 Implement decoder state management
    - Add proper cleanup in destructor (vorbis_block_clear, vorbis_dsp_clear, etc.)
    - Implement reset() method by reinitializing libvorbis state for seeking
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
    - Create decodeAudioPacket() using vorbis_synthesis() and vorbis_synthesis_blockin()
    - Extract PCM samples using vorbis_synthesis_pcmout()
    - Handle variable block sizes and overlap-add processing correctly
    - Convert float samples to 16-bit PCM format
    - _Requirements: 1.5, 1.7, 4.1, 4.2, 4.3, 4.4_

  - [ ] 5.3 Add packet validation and error handling
    - Implement validateVorbisPacket() for basic packet validation
    - Handle corrupted packets by skipping and continuing
    - Implement handleVorbisError() for libvorbis error code processing
    - Output silence for failed frames while maintaining stream continuity
    - _Requirements: 8.2, 8.3, 8.4, 1.8_

- [ ] 6. Implement Float to PCM Conversion
  - [ ] 6.1 Create float to 16-bit PCM conversion system
    - Implement convertFloatToPCM() method for libvorbis float output
    - Handle proper clamping and scaling for 16-bit range
    - Interleave channels correctly during conversion
    - Optimize conversion for performance
    - _Requirements: 1.5, 5.1, 5.2_

  - [ ] 6.2 Add channel interleaving and formatting
    - Process multi-channel float arrays into interleaved 16-bit output
    - Handle channel mapping according to Vorbis conventions
    - Ensure proper sample alignment and channel ordering
    - _Requirements: 5.1, 5.2, 5.5, 5.7_

- [ ] 7. Implement Variable Bitrate and Quality Handling
  - [ ] 7.1 Add support for different bitrate modes
    - Handle constant bitrate Vorbis streams efficiently
    - Support variable bitrate streams with changing bitrates
    - Process quality-based encoding (quality levels -1 to 10)
    - Adapt to bitrate changes seamlessly within streams
    - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.8_

  - [ ] 7.2 Implement quality level and bitrate bound handling
    - Support all Vorbis quality levels from -1 to 10
    - Handle minimum, nominal, and maximum bitrate settings
    - Maintain audio fidelity for high-quality streams
    - Handle bandwidth limitations gracefully for low-bitrate streams
    - _Requirements: 3.5, 3.6, 3.7_

- [ ] 8. Implement Block Size and Windowing Support
  - [ ] 8.1 Add variable block size handling
    - Support short blocks (64-2048 samples) and long blocks (128-8192 samples)
    - Handle block size transitions (short-to-long and long-to-short)
    - Let libvorbis handle windowing and overlap-add processing
    - Manage decoder delay and latency correctly
    - _Requirements: 4.1, 4.2, 4.5, 4.6_

  - [ ] 8.2 Implement proper overlap-add processing
    - Ensure libvorbis handles windowing functions correctly
    - Process overlapping portions of adjacent blocks properly
    - Handle initial and final block overlap correctly
    - Output remaining samples from final overlap during flush
    - _Requirements: 4.3, 4.4, 4.7, 4.8_

- [ ] 9. Implement Multi-Channel Support
  - [ ] 9.1 Add channel configuration handling
    - Support mono and stereo Vorbis streams
    - Handle multi-channel Vorbis (up to 255 channels as per specification)
    - Process channel coupling (magnitude/angle pairs) correctly
    - Validate channel configurations and report errors for unsupported setups
    - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.8_

  - [ ] 9.2 Implement channel mapping and ordering
    - Follow Vorbis channel ordering conventions for multi-channel output
    - Provide properly interleaved channel data in AudioFrame
    - Handle channel count changes appropriately (shouldn't happen in practice)
    - _Requirements: 5.5, 5.6, 5.7_

- [ ] 10. Implement Streaming and Buffering
  - [ ] 10.1 Create output buffer management
    - Implement bounded output buffers to prevent memory exhaustion
    - Handle buffer overflow with appropriate backpressure
    - Process packets incrementally without requiring complete file access
    - _Requirements: 7.1, 7.2, 7.4_

  - [ ] 10.2 Add streaming support and buffer flushing
    - Implement flush() method to output remaining decoded samples
    - Handle partial packet data gracefully
    - Maintain consistent latency and throughput for continuous streaming
    - Clear all internal buffers during reset operations
    - _Requirements: 7.3, 7.5, 7.6, 7.8_

- [ ] 11. Implement Error Handling and Recovery
  - [ ] 11.1 Create comprehensive error handling system
    - Handle invalid header packets with proper error reporting
    - Implement graceful handling of memory allocation failures
    - Validate input parameters and reject invalid data appropriately
    - Interpret libvorbis error codes and handle appropriately
    - _Requirements: 8.1, 8.4, 8.5, 8.6_

  - [ ] 11.2 Add error recovery and state management
    - Reset decoder state when encountering unrecoverable errors
    - Skip corrupted packets and continue processing
    - Output silence for synthesis failures
    - Provide clear error reporting through PsyMP3's error mechanisms
    - _Requirements: 8.2, 8.3, 8.7, 8.8, 11.7_

- [ ] 12. Implement Performance Optimizations
  - [ ] 12.1 Optimize memory usage and CPU efficiency
    - Use appropriately sized buffers for maximum block size
    - Leverage libvorbis optimizations and SIMD support
    - Minimize allocation overhead and memory fragmentation
    - Optimize float to PCM conversion performance
    - _Requirements: 9.1, 9.2, 9.3, 9.6_

  - [ ] 12.2 Add efficient processing for common cases
    - Optimize for standard quality levels and bitrates
    - Handle variable bitrate changes efficiently
    - Optimize memory access patterns for cache efficiency
    - Process overlap-add efficiently through libvorbis
    - _Requirements: 9.4, 9.5, 9.7, 9.8_

- [ ] 13. Implement Thread Safety
  - [ ] 13.1 Ensure thread-safe codec operation
    - Maintain independent libvorbis state per codec instance
    - Use appropriate synchronization for shared resources
    - Handle concurrent initialization and cleanup safely
    - Ensure thread-safe usage of libvorbis functions
    - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.5, 10.6_

  - [ ] 13.2 Add thread-safe error handling and logging
    - Implement thread-safe error state management
    - Use PsyMP3's thread-safe Debug logging system
    - Ensure proper cleanup before codec destruction
    - _Requirements: 10.7, 10.8, 11.8_

- [ ] 14. Create Comprehensive Unit Tests
  - [ ] 14.1 Test core decoding functionality
    - Write tests for Vorbis header processing (ID, comment, setup headers)
    - Test audio packet decoding with various quality levels and bitrates
    - Verify float to PCM conversion correctness
    - Test variable block size handling and overlap-add processing
    - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 3.1, 3.2, 4.1, 4.2_

  - [ ] 14.2 Test error handling and edge cases
    - Test handling of corrupted packets and invalid headers
    - Verify error recovery and decoder state reset functionality
    - Test memory allocation failure scenarios
    - Verify thread safety with concurrent codec instances
    - _Requirements: 8.1, 8.2, 8.7, 10.1, 10.2_

- [ ] 15. Integration Testing and Validation
  - [ ] 15.1 Test integration with demuxer architecture
    - Verify codec works with OggDemuxer for Ogg Vorbis files
    - Test MediaChunk processing and AudioFrame output format
    - Validate seeking support through reset() method
    - Test integration with DemuxedStream bridge interface
    - _Requirements: 6.1, 6.3, 11.3, 11.4, 12.8_

  - [ ] 15.2 Validate compatibility and performance
    - Test with various Vorbis files from different encoders (oggenc, etc.)
    - Verify equivalent or better performance than existing implementation
    - Test all quality levels and encoding configurations
    - Validate output quality and accuracy against libvorbis reference
    - _Requirements: 12.1, 12.2, 12.4, 13.1, 13.2, 13.8_

- [ ] 16. Register Codec with Factory System
  - Register VorbisCodec with AudioCodecFactory for "vorbis" codec name
  - Ensure proper codec selection and instantiation
  - Test factory integration with demuxer system
  - _Requirements: 11.6_