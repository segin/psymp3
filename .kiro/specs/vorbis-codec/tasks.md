# Implementation Plan

- [x] 1. Create VorbisCodec Class Structure and Build System
  - [x] 1.1 Create directory structure and Makefile.am
    - Create `src/codecs/vorbis/` and `include/codecs/vorbis/` directories
    - Create `src/codecs/vorbis/Makefile.am` to build `libvorbiscodec.a`
    - Update `src/codecs/Makefile.am` to include vorbis subdirectory
    - Update `configure.ac` to add vorbis codec Makefile
    - _Requirements: 17.1_

  - [x] 1.2 Create VorbisCodec header file
    - Create `include/codecs/vorbis/VorbisCodec.h` with class declaration
    - Add libvorbis structure members (vorbis_info, vorbis_comment, vorbis_dsp_state, vorbis_block)
    - Add initialization state tracking flags
    - Add stream configuration members (sample_rate, channels, bitrates, block sizes)
    - Add header processing state and position tracking
    - Add threading mutex following public/private lock pattern
    - _Requirements: 1.1-1.6, 16.1_

  - [x] 1.3 Create VorbisCodec implementation skeleton
    - Create `src/codecs/vorbis/VorbisCodec.cpp` with constructor and destructor
    - Implement constructor with vorbis_info_init() and vorbis_comment_init()
    - Implement destructor with cleanup in reverse order (block, dsp, comment, info)
    - Add initialization state tracking to ensure proper cleanup
    - _Requirements: 1.1-1.6, 2.1-2.6_

  - [ ] 1.4 Write property test for cleanup order
    - **Property 14: Cleanup Order**
    - **Validates: Requirements 2.1-2.5**

- [x] 2. Implement Core AudioCodec Interface Methods
  - [x] 2.1 Implement initialize() method
    - Initialize libvorbis structures (vorbis_info_init, vorbis_comment_init)
    - Extract Vorbis parameters from StreamInfo if available
    - Set up internal buffers and state variables
    - Return success/failure status
    - _Requirements: 1.1-1.2, 17.2_

  - [x] 2.2 Implement canDecode() method
    - Check if StreamInfo contains "vorbis" codec name (case-insensitive)
    - Return boolean indicating decode capability
    - _Requirements: 17.7_

  - [x] 2.3 Implement getCodecName() method
    - Return "vorbis" string identifier
    - _Requirements: 17.6_

- [x] 3. Implement Header Processing System
  - [ ] 3.1 Create header packet detection and routing
    - Implement isHeaderPacket_unlocked() to detect 0x01/0x03/0x05 + "vorbis"
    - Implement processHeaderPacket_unlocked() to route header types
    - Add header packet counter and state tracking (3 headers required)
    - Create ogg_packet structure for libvorbis
    - _Requirements: 3.1-3.3_

  - [ ] 3.2 Write property test for header sequence validation
    - **Property 1: Header Sequence Validation**
    - **Validates: Requirements 3.1, 3.7**

  - [ ] 3.3 Implement identification header processing
    - Call vorbis_synthesis_headerin() for ID header
    - Extract version, channels, rate, bitrate_upper, bitrate_nominal, bitrate_lower
    - Extract block sizes using vorbis_info_blocksize()
    - Validate vorbis_info.version is 0
    - _Requirements: 4.1-4.8_

  - [ ] 3.4 Write property test for identification header field extraction
    - **Property 2: Identification Header Field Extraction**
    - **Validates: Requirements 4.1-4.7**

  - [ ] 3.5 Implement block size validation
    - Verify blocksize_0 is power of 2 in range [64, 8192]
    - Verify blocksize_1 is power of 2 in range [64, 8192]
    - Verify blocksize_1 >= blocksize_0
    - Reject stream if validation fails
    - _Requirements: 5.1-5.4_

  - [ ] 3.6 Write property test for block size constraint
    - **Property 3: Block Size Constraint**
    - **Validates: Requirements 5.1-5.3**

  - [ ] 3.7 Implement comment and setup header processing
    - Call vorbis_synthesis_headerin() for comment header
    - Call vorbis_synthesis_headerin() for setup header
    - After all 3 headers: call vorbis_synthesis_init() and vorbis_block_init()
    - Handle initialization errors and cleanup
    - _Requirements: 3.2, 1.3-1.6_

- [ ] 4. Checkpoint - Verify header processing works
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 5. Implement Audio Packet Decoding
  - [ ] 5.1 Create main decode() method implementation
    - Route header packets to header processing system
    - Route audio packets to audio decoding system
    - Handle empty packets and end-of-stream conditions
    - Return appropriate AudioFrame or empty frame
    - _Requirements: 17.3, 6.1-6.3_

  - [ ] 5.2 Implement audio packet decoding core
    - Create ogg_packet structure from MediaChunk data
    - Call vorbis_synthesis() to decode packet into block
    - Call vorbis_synthesis_blockin() to submit block to DSP
    - Handle first block producing no output (overlap-add)
    - _Requirements: 6.1-6.7_

  - [ ] 5.3 Write property test for MediaChunk to AudioFrame conversion
    - **Property 13: MediaChunk to AudioFrame Conversion**
    - **Validates: Requirements 17.3**

  - [ ] 5.4 Implement PCM sample extraction
    - Call vorbis_synthesis_pcmout() to get float samples
    - Process all available samples in loop
    - Call vorbis_synthesis_read() with consumed sample count
    - Update samples_decoded counter
    - _Requirements: 7.1-7.6_

  - [ ] 5.5 Add packet validation and error handling
    - Handle OV_ENOTAUDIO by skipping packet
    - Handle OV_EBADPACKET by skipping and continuing
    - Handle vorbis_synthesis_blockin() errors
    - _Requirements: 6.4-6.6_

  - [ ] 5.6 Write property test for corrupted packet recovery
    - **Property 5: Corrupted Packet Recovery**
    - **Validates: Requirements 13.1-13.4**

- [ ] 6. Implement Float to PCM Conversion
  - [ ] 6.1 Create float to 16-bit PCM conversion
    - Scale float [-1.0, 1.0] to int16 [-32768, 32767]
    - Clamp values exceeding range
    - Interleave channels in Vorbis order
    - _Requirements: 8.1-8.5_

  - [ ] 6.2 Write property test for float to PCM clamping
    - **Property 9: Float to PCM Clamping**
    - **Validates: Requirements 8.2-8.3**

  - [ ] 6.3 Implement channel interleaving
    - Handle mono (1 channel) output
    - Handle stereo (2 channels) interleaved output
    - Handle multi-channel (3-255) interleaved output
    - Follow Vorbis channel ordering conventions
    - _Requirements: 9.1-9.5_

  - [ ] 6.4 Write property test for channel count consistency
    - **Property 8: Channel Count Consistency**
    - **Validates: Requirements 9.1-9.3**

- [ ] 7. Checkpoint - Verify basic decoding works
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 8. Implement State Management
  - [ ] 8.1 Implement reset() method for seeking
    - Call vorbis_synthesis_restart() to reset synthesis state
    - Preserve header information (vorbis_info, vorbis_comment)
    - Clear buffered PCM samples
    - Reset position tracking to zero
    - Clear error state
    - _Requirements: 10.1-10.6_

  - [ ] 8.2 Write property test for reset preserves headers
    - **Property 6: Reset Preserves Headers**
    - **Validates: Requirements 10.1-10.3**

  - [ ] 8.3 Implement flush() method
    - Extract all remaining samples from vorbis_synthesis_pcmout()
    - Return AudioFrame with remaining samples
    - Return empty frame if no samples remain
    - Handle flush before headers processed
    - _Requirements: 11.1-11.5_

  - [ ] 8.4 Write property test for flush outputs remaining samples
    - **Property 7: Flush Outputs Remaining Samples**
    - **Validates: Requirements 11.1-11.3**

- [ ] 9. Implement Error Handling
  - [ ] 9.1 Create error code handling system
    - Handle OV_EREAD, OV_EFAULT, OV_EIMPL, OV_EINVAL
    - Handle OV_ENOTVORBIS, OV_EBADHEADER, OV_EVERSION
    - Handle OV_ENOTAUDIO, OV_EBADPACKET, OV_EBADLINK
    - Set appropriate error state and message
    - _Requirements: 12.1-12.10_

  - [ ] 9.2 Write property test for error code handling
    - **Property 4: Error Code Handling**
    - **Validates: Requirements 12.1-12.10**

  - [ ] 9.3 Implement error recovery
    - Skip corrupted packets and continue
    - Call vorbis_synthesis_restart() on state inconsistency
    - Resume decoding without re-sending headers
    - Log errors through Debug system
    - _Requirements: 13.1-13.5_

- [ ] 10. Implement Buffer Management
  - [ ] 10.1 Add bounded output buffer
    - Limit internal buffer to MAX_BUFFER_SAMPLES
    - Provide backpressure when buffer full
    - Clear buffers on reset
    - _Requirements: 15.1-15.4_

  - [ ] 10.2 Write property test for bounded buffer size
    - **Property 12: Bounded Buffer Size**
    - **Validates: Requirements 15.2-15.3**

- [ ] 11. Checkpoint - Verify error handling and state management
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 12. Implement Container-Agnostic Operation
  - [ ] 12.1 Ensure container-agnostic decoding
    - Decode based on packet data only
    - Work with MediaChunk regardless of source
    - Reset without container-specific operations
    - _Requirements: 14.1-14.6_

  - [ ] 12.2 Write property test for container-agnostic decoding
    - **Property 10: Container-Agnostic Decoding**
    - **Validates: Requirements 14.1-14.3**

- [ ] 13. Implement Thread Safety
  - [ ] 13.1 Ensure thread-safe codec operation
    - Maintain independent libvorbis state per instance
    - Use mutex with public/private lock pattern
    - Handle concurrent initialization safely
    - Ensure safe cleanup with no operations in progress
    - _Requirements: 16.1-16.6_

  - [ ] 13.2 Write property test for instance independence
    - **Property 11: Instance Independence**
    - **Validates: Requirements 16.1-16.2**

- [ ] 14. Register Codec with Factory System
  - [ ] 14.1 Create codec registration
    - Add VorbisCodecSupport namespace with registerCodec()
    - Create factory function for VorbisCodec instantiation
    - Add isVorbisStream() helper function
    - Register with AudioCodecFactory for "vorbis" codec
    - _Requirements: 17.7_

  - [ ] 14.2 Update psymp3.h includes
    - Add conditional include for VorbisCodec.h
    - Add using declaration for backward compatibility
    - Guard with HAVE_OGGDEMUXER or similar
    - _Requirements: 17.1_

- [ ] 15. Checkpoint - Verify factory registration works
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 16. Integration Testing
  - [ ] 16.1 Test integration with OggDemuxer
    - Verify codec works with OggDemuxer for Ogg Vorbis files
    - Test MediaChunk processing and AudioFrame output
    - Validate seeking support through reset()
    - Test with DemuxedStream bridge interface
    - _Requirements: 14.1, 17.3, 20.4_

  - [ ] 16.2 Test compatibility with various encoders
    - Test with oggenc-encoded files
    - Test with ffmpeg-encoded files
    - Test various quality levels (-1 to 10)
    - Verify output quality against reference
    - _Requirements: 20.1-20.5_

- [ ] 17. Final Checkpoint - Make sure all tests are passing
  - Ensure all tests pass, ask the user if questions arise.

