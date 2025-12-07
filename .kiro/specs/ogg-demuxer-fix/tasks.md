# **OGG DEMUXER IMPLEMENTATION PLAN**

## **Implementation Tasks**

- [ ] 1. Project Setup and Infrastructure
  - [ ] 1.1 Archive and remove existing OggDemuxer implementation
    - Move existing `src/demuxer/ogg/OggDemuxer.cpp` to `src/demuxer/ogg/OggDemuxer.cpp.old`
    - Move existing `include/demuxer/ogg/OggDemuxer.h` to `include/demuxer/ogg/OggDemuxer.h.old`
    - Keep existing Makefile.am structure intact
    - _Requirements: All - Clean foundation for rewrite_

  - [ ] 1.2 Create new header file structure
    - Create `include/demuxer/ogg/OggSyncManager.h`
    - Create `include/demuxer/ogg/OggStreamManager.h`
    - Create `include/demuxer/ogg/OggSeekingEngine.h`
    - Create `include/demuxer/ogg/CodecHeaderParser.h`
    - Create `include/demuxer/ogg/VorbisHeaderParser.h`
    - Create `include/demuxer/ogg/OpusHeaderParser.h`
    - Create `include/demuxer/ogg/FLACHeaderParser.h`
    - Create `include/demuxer/ogg/SpeexHeaderParser.h`
    - Create new `include/demuxer/ogg/OggDemuxer.h`
    - _Requirements: 14.1_

  - [ ] 1.3 Create new source file structure
    - Create `src/demuxer/ogg/OggSyncManager.cpp`
    - Create `src/demuxer/ogg/OggStreamManager.cpp`
    - Create `src/demuxer/ogg/OggSeekingEngine.cpp`
    - Create `src/demuxer/ogg/CodecHeaderParser.cpp`
    - Create `src/demuxer/ogg/VorbisHeaderParser.cpp`
    - Create `src/demuxer/ogg/OpusHeaderParser.cpp`
    - Create `src/demuxer/ogg/FLACHeaderParser.cpp`
    - Create `src/demuxer/ogg/SpeexHeaderParser.cpp`
    - Create new `src/demuxer/ogg/OggDemuxer.cpp`
    - _Requirements: 14.1_

  - [ ] 1.4 Update build system
    - Update `src/demuxer/ogg/Makefile.am` with new source files
    - Verify libogg linkage is correct
    - Run `./generate-configure.sh && ./configure && make -j$(nproc)`
    - _Requirements: 14.1_

- [ ] 2. Implement OggSyncManager (I/O and Page Extraction)
  - [ ] 2.1 Implement OggSyncManager class structure
    - Define class with ogg_sync_state member
    - Implement constructor with IOHandler parameter
    - Implement destructor with ogg_sync_clear()
    - Implement reset() wrapping ogg_sync_reset()
    - Implement tell() and seek() for file position
    - Implement isEOF() accessor
    - _Requirements: 14.5, 14.8, 14.9_

  - [ ] 2.2 Implement getData() following _get_data() pattern
    - Call ogg_sync_buffer() to get write buffer
    - Read from IOHandler into buffer
    - Call ogg_sync_wrote() to commit bytes
    - Track file offset and EOF state
    - Return bytes read or error code
    - _Requirements: 2.1, 9.6_

  - [ ] 2.3 Implement getNextPage() following _get_next_page() pattern
    - Use ogg_sync_pageseek() for page discovery (NOT ogg_sync_pageout())
    - Handle partial pages by calling getData()
    - Respect boundary parameter for bisection search
    - Return page size on success, negative on error
    - _Requirements: 2.1, 7.2, 9.1_

  - [ ] 2.4 Implement getPrevPage() following _get_prev_page() pattern
    - Seek backwards in CHUNKSIZE increments
    - Read forward to find pages
    - Return last complete page before end position
    - Handle file beginning boundary
    - _Requirements: 7.3, 8.3_

  - [ ] 2.5 Implement getPrevPageSerial() for serial-specific backward scan
    - Extend getPrevPage() to filter by serial number
    - Used for duration calculation and seeking
    - _Requirements: 7.9, 8.1_

  - [ ] 2.6 Write unit tests for OggSyncManager
    - Test getData() with various buffer sizes
    - Test getNextPage() page extraction
    - Test getPrevPage() backward scanning
    - Test EOF and error handling
    - _Requirements: 2.1, 7.2, 7.3_

- [ ] 3. Implement OggStreamManager (Logical Bitstream Management)
  - [ ] 3.1 Implement OggStreamManager class structure
    - Define class with map of serial -> ogg_stream_state
    - Implement constructor and destructor with proper cleanup
    - Implement hasStream() check
    - Implement getSerialNumbers() for iteration
    - Implement getStreamState() accessor
    - _Requirements: 3.9, 14.8_

  - [ ] 3.2 Implement stream lifecycle methods
    - Implement addStream() with ogg_stream_init()
    - Implement removeStream() with ogg_stream_clear()
    - Implement clearAllStreams() for cleanup
    - Handle duplicate serial number detection
    - _Requirements: 3.9, 4.14, 10.6_

  - [ ] 3.3 Implement page and packet operations
    - Implement submitPage() wrapping ogg_stream_pagein()
    - Implement getPacket() wrapping ogg_stream_packetout()
    - Implement peekPacket() wrapping ogg_stream_packetpeek()
    - Route pages by serial number
    - _Requirements: 2.2, 2.3, 3.10, 7.6_

  - [ ] 3.4 Implement stream reset methods
    - Implement resetStream() wrapping ogg_stream_reset()
    - Implement resetAllStreams() for seek operations
    - _Requirements: 14.9, 14.10_

  - [ ] 3.5 Write unit tests for OggStreamManager
    - Test stream creation and destruction
    - Test page routing by serial number
    - Test packet extraction
    - Test multiple stream handling
    - Test duplicate serial detection
    - _Requirements: 3.9, 3.10, 4.14_

- [ ] 4. Checkpoint - Verify Core libogg Integration
  - Ensure all tests pass, ask the user if questions arise.


- [ ] 5. Implement CodecHeaderParser Base and Factory
  - [ ] 5.1 Implement CodecHeaderParser base class
    - Define virtual interface: parseHeader(), headersComplete()
    - Implement static identifyCodec() for signature detection
    - Detect Vorbis: `\x01vorbis` (7 bytes)
    - Detect Opus: `OpusHead` (8 bytes)
    - Detect FLAC: `\x7fFLAC` (5 bytes)
    - Detect Speex: `Speex   ` (8 bytes)
    - Detect Theora: `\x80theora` (7 bytes)
    - Return "unknown" for unrecognized signatures
    - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6_

  - [ ] 5.2 Implement factory method
    - Implement static create() returning appropriate parser
    - Return nullptr for unknown codecs
    - _Requirements: 4.11_

  - [ ] 5.3 Write property test for codec signature detection
    - **Property 6: Codec Signature Detection**
    - Generate packets with known signatures, verify detection
    - Generate packets with random data, verify "unknown" result
    - **Validates: Requirements 3.2, 3.3, 3.4, 3.5, 3.6**

- [ ] 6. Implement VorbisHeaderParser
  - [ ] 6.1 Implement Vorbis identification header parsing
    - Validate `\x01vorbis` signature
    - Extract vorbis_version (must be 0)
    - Extract audio_channels (byte 11)
    - Extract audio_sample_rate (bytes 12-15, little-endian)
    - Extract bitrate fields (bytes 16-27)
    - Store as first header packet
    - _Requirements: 4.1_

  - [ ] 6.2 Implement Vorbis comment header parsing
    - Validate `\x03vorbis` signature
    - Parse vendor string (length + UTF-8 string)
    - Parse comment count and comments (TAG=value format)
    - Extract title, artist, album metadata
    - Store as second header packet
    - _Requirements: 4.2_

  - [ ] 6.3 Implement Vorbis setup header handling
    - Validate `\x05vorbis` signature
    - Store complete packet for decoder (no parsing needed)
    - Mark headers complete after third header
    - _Requirements: 4.3_

  - [ ] 6.4 Implement headersComplete() for Vorbis
    - Return true only when all 3 headers received
    - _Requirements: 4.3, 4.12_

  - [ ] 6.5 Write property test for Vorbis header count
    - **Property 7: Vorbis Header Count**
    - Verify headers_complete is false until 3 headers received
    - **Validates: Requirements 4.1, 4.2, 4.3**

- [ ] 7. Implement OpusHeaderParser
  - [ ] 7.1 Implement OpusHead parsing (RFC 7845 §5.1)
    - Validate `OpusHead` signature (8 bytes)
    - Extract version (byte 8, must be 1)
    - Extract channel_count (byte 9)
    - Extract pre_skip (bytes 10-11, little-endian)
    - Extract input_sample_rate (bytes 12-15, little-endian)
    - Extract output_gain (bytes 16-17)
    - Extract channel_mapping_family (byte 18)
    - Handle extended mapping if family != 0
    - Store as first header packet
    - _Requirements: 4.4_

  - [ ] 7.2 Implement OpusTags parsing (RFC 7845 §5.2)
    - Validate `OpusTags` signature (8 bytes)
    - Parse vendor string and comments (same format as Vorbis)
    - Extract metadata fields
    - Store as second header packet
    - Mark headers complete after second header
    - _Requirements: 4.5_

  - [ ] 7.3 Implement headersComplete() for Opus
    - Return true when both OpusHead and OpusTags received
    - _Requirements: 4.5, 4.12_

  - [ ] 7.4 Write property test for Opus header count
    - **Property 8: Opus Header Count**
    - Verify headers_complete is false until 2 headers received
    - **Validates: Requirements 4.4, 4.5**

- [ ] 8. Implement FLACHeaderParser (RFC 9639 §10.1)
  - [ ] 8.1 Implement FLAC-in-Ogg identification header parsing
    - Validate `\x7fFLAC` signature (5 bytes)
    - Extract mapping version (bytes 5-6, expect 1.0)
    - Extract header_packets count (bytes 7-8, big-endian)
    - Validate `fLaC` native signature (bytes 9-12)
    - Parse metadata block header (bytes 13-16)
    - Extract STREAMINFO (bytes 17-50, 34 bytes)
    - Verify total packet size is 51 bytes
    - _Requirements: 4.6, 4.7, 4.8, 5.1, 5.2_

  - [ ] 8.2 Implement STREAMINFO extraction
    - Extract min/max block size (bits 0-31)
    - Extract min/max frame size (bits 32-79)
    - Extract sample_rate (bits 80-99, 20 bits)
    - Extract channels (bits 100-102, 3 bits + 1)
    - Extract bits_per_sample (bits 103-107, 5 bits + 1)
    - Extract total_samples (bits 108-143, 36 bits)
    - _Requirements: 5.10_

  - [ ] 8.3 Implement FLAC metadata block parsing
    - Parse subsequent header packets as metadata blocks
    - First header packet SHOULD be Vorbis comment (type 4)
    - Detect last-metadata-block flag
    - Handle header_packets count (0 = unknown)
    - _Requirements: 4.10, 5.4, 5.12_

  - [ ] 8.4 Implement headersComplete() for FLAC
    - Return true when last-metadata-block flag seen
    - Or when expected header count reached
    - _Requirements: 4.12, 5.4_

  - [ ] 8.5 Implement FLAC version handling
    - Accept version 1.0 (0x01 0x00)
    - Log warning for other versions, attempt parsing
    - _Requirements: 5.3_

  - [ ] 8.6 Write property test for FLAC-in-Ogg identification header
    - **Property 9: FLAC-in-Ogg Identification Header**
    - Verify 51-byte structure parsing
    - **Validates: Requirements 5.2**

  - [ ] 8.7 Write property test for FLAC-in-Ogg first page size
    - **Property 10: FLAC-in-Ogg First Page Size**
    - Verify first page is exactly 79 bytes
    - **Validates: Requirements 4.9**

- [ ] 9. Implement SpeexHeaderParser
  - [ ] 9.1 Implement Speex identification header parsing
    - Validate `Speex   ` signature (8 bytes with spaces)
    - Extract version string
    - Extract sample_rate, channels, bitrate
    - Store as first header packet
    - _Requirements: 3.5_

  - [ ] 9.2 Implement Speex comment header parsing
    - Parse Vorbis-style comments
    - Mark headers complete after second header
    - _Requirements: 4.11_

- [ ] 10. Checkpoint - Verify Header Parsing
  - Ensure all tests pass, ask the user if questions arise.


- [ ] 11. Implement OggSeekingEngine - Granule Arithmetic
  - [ ] 11.1 Implement granule position arithmetic
    - Implement granposAdd() with overflow detection
    - Implement granposDiff() with wraparound handling
    - Implement granposCmp() for proper ordering
    - Handle -1 as invalid/unset granule position
    - _Requirements: 12.1, 12.2, 12.3, 12.4, 12.5, 12.6_

  - [ ] 11.2 Implement time conversion methods
    - Implement granuleToMs() with codec-specific logic
    - Opus: (granule - pre_skip) * 1000 / 48000
    - Vorbis/FLAC: granule * 1000 / sample_rate
    - Implement msToGranule() inverse conversion
    - _Requirements: 7.4, 7.5, 8.5, 8.6, 8.7, 8.8, 12.7, 12.8, 12.9_

  - [ ] 11.3 Write property test for granule arithmetic safety
    - **Property 15: Granule Arithmetic Overflow Safety**
    - Test overflow detection in addition
    - Test underflow detection in subtraction
    - **Validates: Requirements 12.1, 12.2, 12.6**

  - [ ] 11.4 Write property test for duration calculation
    - **Property 17: Duration Calculation Consistency**
    - Verify formula: (last_granule - pre_skip) * 1000 / rate
    - **Validates: Requirements 8.6, 8.7, 8.8**

- [ ] 12. Implement OggSeekingEngine - Duration Calculation
  - [ ] 12.1 Implement OggSeekingEngine class structure
    - Define class with OggSyncManager and OggStreamManager pointers
    - Implement constructor
    - _Requirements: 8.1_

  - [ ] 12.2 Implement calculateDuration()
    - Check for total_samples in STREAMINFO (FLAC)
    - Fall back to backward scanning for last granule
    - Convert granule to milliseconds
    - _Requirements: 8.1, 8.2_

  - [ ] 12.3 Implement getLastGranule() backward scanning
    - Start with CHUNKSIZE, grow exponentially
    - Seek backwards, scan forward for pages
    - Filter by serial number
    - Return best granule found
    - Handle file beginning boundary
    - _Requirements: 8.1, 8.3, 8.10_

- [ ] 13. Implement OggSeekingEngine - Bisection Search
  - [ ] 13.1 Implement seekToGranule() bisection search
    - Initialize search interval [begin, end]
    - Use bisectForward() to narrow interval
    - Switch to linear scan when interval small
    - Reset sync state after positioning
    - _Requirements: 7.1, 7.2, 7.11_

  - [ ] 13.2 Implement bisectForward() helper
    - Seek to midpoint
    - Find next page with matching serial using getNextPage()
    - Compare granule position to target
    - Adjust interval based on comparison
    - Handle granule position -1 (continue searching)
    - _Requirements: 7.1, 7.2, 7.10_

  - [ ] 13.3 Implement seekToTime() wrapper
    - Convert timestamp to granule using msToGranule()
    - Call seekToGranule()
    - Account for pre-skip in Opus streams
    - _Requirements: 7.4, 7.5_

  - [ ] 13.4 Write property test for seeking accuracy
    - **Property 16: Seeking Accuracy**
    - Seek to target, verify next packet granule >= target
    - Verify previous page granule < target
    - **Validates: Requirements 7.1**

- [ ] 14. Checkpoint - Verify Seeking and Duration
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 15. Implement OggDemuxer - Core Structure
  - [ ] 15.1 Implement OggDemuxer class structure
    - Define State enum: INIT, HEADERS, STREAMING, ERROR
    - Define member variables for components
    - Define OggStreamInfo map
    - Define mutex for thread safety
    - _Requirements: 11.1, 14.1_

  - [ ] 15.2 Implement constructor and destructor
    - Create OggSyncManager with IOHandler
    - Create OggStreamManager
    - Implement destructor with proper cleanup
    - _Requirements: 10.6, 14.8_

  - [ ] 15.3 Implement static registration
    - Implement registerDemuxer() for DemuxerFactory
    - Implement canHandle() checking for OggS signature
    - _Requirements: 14.1_

- [ ] 16. Implement OggDemuxer - Container Parsing
  - [ ] 16.1 Implement parseContainer()
    - Get file size from IOHandler
    - Call fetchHeaders_unlocked()
    - Create OggSeekingEngine
    - Calculate duration
    - Select primary audio stream
    - Seek to data_start
    - Transition to STREAMING state
    - _Requirements: 14.1, 14.5_

  - [ ] 16.2 Implement fetchHeaders_unlocked() following _fetch_headers()
    - Read pages until all BOS pages seen
    - For each BOS page: create stream, identify codec, create parser
    - Continue reading until all headers complete
    - Record data_start offset
    - Handle grouped streams (all BOS before data)
    - Handle unknown codecs (skip, continue)
    - _Requirements: 3.7, 4.11, 4.12, 4.13, 4.16_

  - [ ] 16.3 Implement processHeaderPacket_unlocked()
    - Get appropriate CodecHeaderParser
    - Call parseHeader() on parser
    - Update OggStreamInfo with parsed data
    - Check headersComplete()
    - _Requirements: 4.1-4.10, 4.15_

  - [ ] 16.4 Write property test for grouped stream ordering
    - **Property 11: Grouped Stream BOS Ordering**
    - Verify all BOS pages appear before non-BOS pages
    - **Validates: Requirements 3.7**

- [ ] 17. Implement OggDemuxer - Stream Information
  - [ ] 17.1 Implement getStreams()
    - Return vector of StreamInfo for complete streams
    - Populate codec_type, codec_name, sample_rate, channels
    - Include duration and metadata
    - _Requirements: 14.2_

  - [ ] 17.2 Implement getStreamInfo()
    - Return StreamInfo for specific stream_id
    - Return empty StreamInfo if not found
    - _Requirements: 14.2_

  - [ ] 17.3 Implement selectPrimaryStream()
    - Prefer first audio stream with complete headers
    - Store as m_primary_serial
    - _Requirements: 14.1_


- [ ] 18. Implement OggDemuxer - Packet Reading
  - [ ] 18.1 Implement readChunk() for primary stream
    - Acquire mutex lock
    - Call readChunk_unlocked(m_primary_serial)
    - _Requirements: 6.1, 11.1_

  - [ ] 18.2 Implement readChunk(stream_id)
    - Acquire mutex lock
    - Call readChunk_unlocked(stream_id)
    - _Requirements: 6.1, 11.1_

  - [ ] 18.3 Implement readChunk_unlocked()
    - First call: return header packets as MediaChunk
    - Get packet from OggStreamManager
    - If no packet: read more pages, submit to stream
    - Build MediaChunk from ogg_packet
    - Update current_granule and position
    - Detect page sequence gaps (page loss)
    - Handle EOS flag
    - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.8, 14.3_

  - [ ] 18.4 Write property test for page sequence continuity
    - **Property 13: Page Sequence Continuity**
    - Verify detection of non-consecutive sequence numbers
    - **Validates: Requirements 1.6, 6.8**

  - [ ] 18.5 Write property test for header packet preservation
    - **Property 19: Header Packet Preservation**
    - Verify all header packets preserved exactly
    - **Validates: Requirements 4.3, 4.12**

- [ ] 19. Implement OggDemuxer - Seeking
  - [ ] 19.1 Implement seekTo()
    - Acquire mutex lock
    - Call seekTo_unlocked()
    - _Requirements: 7.1, 11.2_

  - [ ] 19.2 Implement seekTo_unlocked()
    - Clamp timestamp to valid range
    - Call seeking engine seekToTime()
    - Reset all stream states (but keep headers)
    - Update current position
    - Do NOT resend headers (m_headers_sent stays true)
    - _Requirements: 7.1, 7.7, 7.8, 9.7_

  - [ ] 19.3 Implement position accessors
    - Implement getDuration() returning m_duration_ms
    - Implement getPosition() returning m_current_position_ms
    - Implement getGranulePosition() for specific stream
    - _Requirements: 14.4_

  - [ ] 19.4 Implement isEOF()
    - Return true when sync manager reports EOF
    - _Requirements: 6.5_

- [ ] 20. Checkpoint - Verify Basic Playback
  - Ensure all tests pass, ask the user if questions arise.
  - Test with real Ogg Vorbis file
  - Test with real Ogg Opus file
  - Verify audio plays correctly

- [ ] 21. Implement Page Structure Validation Properties
  - [ ] 21.1 Write property test for OggS capture pattern
    - **Property 1: OggS Capture Pattern Validation**
    - Generate byte sequences, verify only "OggS" accepted
    - **Validates: Requirements 1.1**

  - [ ] 21.2 Write property test for page version
    - **Property 2: Page Version Validation**
    - Generate pages with various versions, verify only 0 accepted
    - **Validates: Requirements 1.2**

  - [ ] 21.3 Write property test for page size calculation
    - **Property 3: Page Size Calculation**
    - Verify header_size = 27 + N, total = header + sum(lacing)
    - **Validates: Requirements 1.9, 1.10**

  - [ ] 21.4 Write property test for page size bounds
    - **Property 4: Page Size Bounds**
    - Verify pages > 65307 bytes rejected
    - **Validates: Requirements 1.11**

  - [ ] 21.5 Write property test for lacing value interpretation
    - **Property 5: Lacing Value Interpretation**
    - Verify 255 = continuation, <255 = termination
    - **Validates: Requirements 2.4, 2.5**

- [ ] 22. Implement Stream Multiplexing Properties
  - [ ] 22.1 Write property test for chained stream detection
    - **Property 12: Chained Stream Detection**
    - Verify BOS after data pages detected as chain boundary
    - **Validates: Requirements 3.8, 3.11**

  - [ ] 22.2 Write property test for granule position validity
    - **Property 14: Granule Position Validity**
    - Verify -1 treated as "no packets complete"
    - **Validates: Requirements 6.9, 7.10, 9.9**

- [ ] 23. Implement Packet Handling Properties
  - [ ] 23.1 Write property test for packet reconstruction
    - **Property 18: Packet Reconstruction Completeness**
    - Verify multi-page packets reconstructed correctly
    - **Validates: Requirements 2.7, 13.1**


- [ ] 24. Implement Thread Safety
  - [ ] 24.1 Implement public/private lock pattern
    - All public methods acquire m_mutex
    - All _unlocked methods assume lock held
    - Document lock requirements in comments
    - _Requirements: 11.1, 11.2, 11.3_

  - [ ] 24.2 Implement safe cleanup
    - Destructor acquires lock before cleanup
    - Ensure no operations in progress
    - _Requirements: 11.6_

  - [ ] 24.3 Write property test for thread safety
    - **Property 20: Thread Safety - No Data Races**
    - Concurrent read and seek operations
    - Verify no crashes or data corruption
    - **Validates: Requirements 11.1, 11.2**

- [ ] 25. Implement Error Handling
  - [ ] 25.1 Implement page-level error recovery
    - Skip corrupted pages (invalid capture pattern)
    - Handle CRC failures via libogg
    - Handle oversized pages
    - _Requirements: 9.1, 9.2, 9.12_

  - [ ] 25.2 Implement stream-level error recovery
    - Skip unknown codecs, continue with known
    - Handle incomplete headers
    - Return appropriate error codes
    - _Requirements: 9.3, 9.4, 9.10_

  - [ ] 25.3 Implement seeking error recovery
    - Clamp seeks to valid range
    - Fall back to linear scan on bisection failure
    - Maintain valid state on failure
    - _Requirements: 9.7, 9.11, 7.8_

  - [ ] 25.4 Implement I/O error handling
    - Propagate IOHandler errors
    - Handle EOF gracefully
    - _Requirements: 9.5, 9.6_


- [ ] 26. Checkpoint - Verify Error Handling
  - Ensure all tests pass, ask the user if questions arise.
  - Test with corrupted files
  - Test with truncated files
  - Verify graceful degradation

- [ ] 27. FLAC-in-Ogg Specific Handling
  - [ ] 27.1 Implement FLAC granule position handling
    - Interpret granule as interchannel sample count
    - Handle 0xFFFFFFFFFFFFFFFF (no completed packet)
    - Expect granule 0 for header pages
    - _Requirements: 5.6, 5.7, 5.8_

  - [ ] 27.2 Implement FLAC audio packet handling
    - Each packet is single FLAC frame
    - Verify first audio packet starts new page
    - _Requirements: 5.5, 5.13_

  - [ ] 27.3 Implement FLAC chaining detection
    - Detect audio property changes
    - Handle as new stream (EOS + BOS)
    - _Requirements: 5.9_

- [ ] 28. Integration Testing
  - [ ] 28.1 Test with Ogg Vorbis files
    - Various encoders (oggenc, ffmpeg, etc.)
    - Various quality settings
    - Verify playback, seeking, duration
    - _Requirements: All Vorbis requirements_

  - [ ] 28.2 Test with Ogg Opus files
    - Various encoders (opusenc, ffmpeg)
    - Various channel configurations
    - Verify pre-skip handling
    - _Requirements: All Opus requirements_

  - [ ] 28.3 Test with FLAC-in-Ogg files (.oga)
    - Various encoders (flac, ffmpeg)
    - Verify STREAMINFO extraction
    - Verify duration from total_samples
    - _Requirements: All FLAC-in-Ogg requirements_

  - [ ] 28.4 Test seeking accuracy
    - Seek to various positions
    - Verify audio resumes correctly
    - Test boundary conditions (start, end)
    - _Requirements: 7.1-7.11_

  - [ ] 28.5 Test with chained streams
    - Multiple concatenated Ogg streams
    - Verify chain boundary detection
    - _Requirements: 3.8, 3.11_

  - [ ] 28.6 Test with grouped/multiplexed streams
    - Audio + video Ogg files
    - Verify stream separation
    - _Requirements: 3.7, 3.9, 3.10_

- [ ] 29. Final Checkpoint - Verify All Tests Pass
  - Ensure all tests pass, ask the user if questions arise.
  - Run complete property test suite (20 properties)
  - Verify clean build

- [ ] 30. Cleanup and Documentation
  - [ ] 30.1 Remove archived old implementation
    - Delete `src/demuxer/ogg/OggDemuxer.cpp.old`
    - Delete `include/demuxer/ogg/OggDemuxer.h.old`
    - _Requirements: Cleanup_

  - [ ] 30.2 Update documentation
    - Update `docs/ogg-demuxer-developer-guide.md`
    - Add inline documentation for all public methods
    - Document RFC compliance (3533, 7845, 9639)
    - _Requirements: All_

  - [ ] 30.3 Final validation
    - Run `make clean && make -j$(nproc)`
    - Run complete test suite
    - Commit with proper git messages
    - _Requirements: All_
