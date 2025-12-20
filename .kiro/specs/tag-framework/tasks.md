# Implementation Plan: Tag Framework

## Overview

This implementation plan breaks down the Tag framework into discrete coding tasks. The framework provides format-neutral metadata access for audio files, supporting VorbisComment, ID3v1, ID3v2, and merged ID3 tags. Implementation follows a bottom-up approach: core utilities first, then format-specific parsers, then integration with Stream/Demuxer classes.

## Tasks

- [x] 1. Implement ID3v1 Tag Reader
  - [x] 1.1 Create ID3v1Tag header and class structure
    - Define ID3v1Tag class in `include/tag/ID3v1Tag.h`
    - Implement static `parse()` and `isValid()` methods
    - Implement all Tag interface methods
    - Add genre lookup table (192 genres)
    - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5_

  - [x] 1.2 Implement ID3v1 parsing logic
    - Parse fixed 128-byte structure
    - Detect ID3v1.1 format (track number in comment)
    - Implement string trimming for trailing nulls/spaces
    - Implement genre index to string mapping
    - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5_

  - [x] 1.3 Write property test for ID3v1 round-trip parsing
    - **Property 4: ID3v1 Round-Trip Parsing**
    - **Validates: Requirements 4.1, 4.2, 4.3, 4.5**

  - [x] 1.4 Write property test for ID3v1 genre mapping
    - **Property 5: ID3v1 Genre Mapping**
    - **Validates: Requirements 4.4**

  - [x] 1.5 Write unit tests for ID3v1Tag
    - Test ID3v1 vs ID3v1.1 detection
    - Test string trimming edge cases
    - Test all 192 genre mappings
    - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5_

- [x] 2. Implement ID3v2 Core Utilities
  - [x] 2.1 Create ID3v2 utility functions
    - Implement synchsafe integer encode/decode
    - Implement text encoding detection and conversion
    - Implement unsynchronization decode
    - _Requirements: 3.2, 3.4, 3.6_

  - [x] 2.2 Write property test for synchsafe integer round-trip
    - **Property 6: ID3v2 Synchsafe Integer Round-Trip**
    - **Validates: Requirements 3.2**

  - [x] 2.3 Write property test for text encoding round-trip
    - **Property 7: ID3v2 Text Encoding Round-Trip**
    - **Validates: Requirements 3.4**

- [ ] 3. Implement ID3v2 Tag Reader
  - [ ] 3.1 Create ID3v2Tag header and class structure
    - Define ID3v2Tag class in `include/tag/ID3v2Tag.h`
    - Define ID3v2Frame structure
    - Implement static `parse()`, `isValid()`, `getTagSize()` methods
    - _Requirements: 3.1, 3.2, 3.3_

  - [ ] 3.2 Implement ID3v2 header parsing
    - Parse version (2.2, 2.3, 2.4)
    - Parse flags (unsync, extended header, footer)
    - Parse synchsafe tag size
    - Handle extended header skipping
    - _Requirements: 3.1, 3.2, 3.7_

  - [ ] 3.3 Implement ID3v2 frame parsing
    - Parse frame headers (v2.2 3-char, v2.3/v2.4 4-char)
    - Handle frame flags
    - Implement frame ID normalization (v2.2 to v2.3+)
    - Parse text frames with encoding handling
    - _Requirements: 3.3, 3.4, 3.8_

  - [ ] 3.4 Implement APIC frame parsing for pictures
    - Parse MIME type, picture type, description
    - Extract image data
    - Handle v2.2 PIC vs v2.3+ APIC differences
    - _Requirements: 3.5, 5.1, 5.2, 5.3_

  - [ ] 3.5 Write unit tests for ID3v2Tag
    - Test version detection (2.2, 2.3, 2.4)
    - Test frame ID mapping
    - Test text encoding handling
    - Test APIC parsing
    - _Requirements: 3.1, 3.3, 3.4, 3.5, 3.8_

- [ ] 4. Checkpoint - ID3 Parsers Complete
  - Ensure all ID3v1 and ID3v2 tests pass
  - Verify build compiles cleanly
  - Ask user if questions arise

- [ ] 5. Implement MergedID3Tag
  - [ ] 5.1 Create MergedID3Tag class
    - Define MergedID3Tag in `include/tag/MergedID3Tag.h`
    - Implement constructor taking ID3v1Tag and ID3v2Tag
    - Implement all Tag interface methods with fallback logic
    - _Requirements: 6a.1, 6a.2, 6a.3, 6a.4, 6a.5, 6a.6_

  - [ ] 5.2 Write property test for ID3 tag merging
    - **Property 8: ID3 Tag Merging with Fallback**
    - **Validates: Requirements 6a.3, 6a.4, 6a.5, 6a.6**

  - [ ] 5.3 Write unit tests for MergedID3Tag
    - Test ID3v2 precedence over ID3v1
    - Test fallback when ID3v2 field is empty
    - Test ID3v1-only and ID3v2-only cases
    - _Requirements: 6a.3, 6a.4, 6a.5, 6a.6_

- [ ] 6. Implement VorbisComment Tag Reader
  - [ ] 6.1 Create VorbisCommentTag header and class structure
    - Define VorbisCommentTag in `include/tag/VorbisCommentTag.h`
    - Implement static `parse()` method
    - Implement field name normalization (uppercase)
    - _Requirements: 2.1, 2.2, 2.4_

  - [ ] 6.2 Implement VorbisComment parsing logic
    - Parse vendor string (little-endian length)
    - Parse field count and all fields
    - Handle UTF-8 encoding
    - Implement case-insensitive storage
    - Support multi-valued fields
    - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5_

  - [ ] 6.3 Implement field name validation and mapping
    - Validate field names (printable ASCII, no '=')
    - Skip invalid fields gracefully
    - Map standard fields to Tag interface
    - _Requirements: 2.6, 2.7_

  - [ ] 6.4 Implement METADATA_BLOCK_PICTURE parsing
    - Parse base64-encoded picture data
    - Extract picture type, MIME, description, dimensions
    - _Requirements: 2.8, 5.1, 5.2, 5.3, 5.4_

  - [ ] 6.5 Write property test for VorbisComment round-trip
    - **Property 1: VorbisComment Round-Trip Parsing**
    - **Validates: Requirements 2.1, 2.2, 2.3**

  - [ ] 6.6 Write property test for case-insensitive lookup
    - **Property 2: VorbisComment Case-Insensitive Lookup**
    - **Validates: Requirements 2.4**

  - [ ] 6.7 Write property test for multi-valued fields
    - **Property 3: VorbisComment Multi-Valued Fields**
    - **Validates: Requirements 2.5**

  - [ ] 6.8 Write unit tests for VorbisCommentTag
    - Test vendor string extraction
    - Test field parsing
    - Test invalid field handling
    - Test METADATA_BLOCK_PICTURE parsing
    - _Requirements: 2.1, 2.2, 2.6, 2.7, 2.8_

- [ ] 7. Checkpoint - All Tag Parsers Complete
  - Ensure all VorbisComment tests pass
  - Ensure all ID3 tests pass
  - Verify build compiles cleanly
  - Ask user if questions arise

- [ ] 8. Implement Tag Factory
  - [ ] 8.1 Enhance TagFactory with format detection
    - Implement `detectFormat()` for magic byte detection
    - Implement `createFromFile()` with ID3v1/ID3v2 handling
    - Implement `createFromData()` with format hints
    - _Requirements: 6.1, 6.2, 6.3_

  - [ ] 8.2 Implement MP3 tag loading with merging
    - Check for ID3v1 at file end
    - Check for ID3v2 at file start
    - Create MergedID3Tag when both present
    - _Requirements: 6a.1, 6a.2, 6a.3_

  - [ ] 8.3 Write property test for factory null safety
    - **Property 9: Tag Factory Never Returns Null**
    - **Validates: Requirements 6.4, 6.5**

  - [ ] 8.4 Write unit tests for TagFactory
    - Test format detection
    - Test file-based tag loading
    - Test data-based tag loading
    - Test format hints
    - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5_

- [ ] 9. Implement Error Handling
  - [ ] 9.1 Add robust error handling to all parsers
    - Add size validation before all reads
    - Handle truncated data gracefully
    - Return NullTag or partial data on errors
    - _Requirements: 10.1, 10.2, 10.3_

  - [ ] 9.2 Implement UTF-8 error handling
    - Add safe UTF-8 decoding with replacement characters
    - Handle invalid sequences gracefully
    - _Requirements: 10.4_

  - [ ] 9.3 Write property test for corrupted data handling
    - **Property 10: Corrupted Data Handling**
    - **Validates: Requirements 10.1, 10.2, 10.3**

- [ ] 10. Checkpoint - Tag Framework Core Complete
  - Ensure all error handling tests pass
  - Verify no crashes on malformed input
  - Ask user if questions arise

- [ ] 11. Integrate with Demuxer
  - [ ] 11.1 Add tag support to Demuxer base class
    - Add `m_tag` member to Demuxer
    - Add `getTag()` method returning const reference
    - Initialize with NullTag by default
    - _Requirements: 8.1, 8.5_

  - [ ] 11.2 Update FLACDemuxer to extract VorbisComment
    - Create VorbisCommentTag from parsed metadata
    - Store in m_tag during parseContainer()
    - _Requirements: 8.2, 8.4_

  - [ ] 11.3 Update OggDemuxer to extract VorbisComment
    - Extract VorbisComment from stream headers
    - Create VorbisCommentTag from parsed data
    - _Requirements: 8.3, 8.4_

  - [ ] 11.4 Write unit tests for demuxer tag extraction
    - Test FLAC VorbisComment extraction
    - Test Ogg VorbisComment extraction
    - Test tagless containers return NullTag
    - _Requirements: 8.2, 8.3, 8.5_

- [ ] 12. Integrate with Stream Classes
  - [ ] 12.1 Add tag support to Stream base class
    - Add `m_tag` member to Stream
    - Add `getTag()` method
    - Update getArtist/getTitle/getAlbum to delegate to tag
    - _Requirements: 7.1, 7.4, 7.5_

  - [ ] 12.2 Update DemuxedStream to use demuxer tags
    - Get tag from demuxer after parsing
    - Override getTag() to return demuxer's tag
    - _Requirements: 7.2, 7.3_

  - [ ] 12.3 Write property test for Stream-Tag delegation
    - **Property 11: Stream-Tag Delegation Consistency**
    - **Validates: Requirements 7.4**

  - [ ] 12.4 Write unit tests for Stream tag integration
    - Test Stream::getTag() returns valid tag
    - Test DemuxedStream tag extraction
    - Test delegation consistency
    - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5_

- [ ] 13. Implement Thread Safety
  - [ ] 13.1 Ensure thread-safe read operations
    - Verify all Tag methods are const
    - Verify no mutable state modified during reads
    - Add thread safety documentation
    - _Requirements: 9.1, 9.3, 9.4_

  - [ ] 13.2 Write property test for concurrent reads
    - **Property 12: Thread-Safe Concurrent Reads**
    - **Validates: Requirements 9.1, 9.2**

- [ ] 14. Implement Picture Access
  - [ ] 14.1 Write property test for picture index access
    - **Property 13: Picture Index Access**
    - **Validates: Requirements 5.1, 5.2, 11.4**

  - [ ] 14.2 Write unit tests for picture access
    - Test pictureCount() accuracy
    - Test getPicture() bounds checking
    - Test getFrontCover() convenience method
    - _Requirements: 5.1, 5.2, 5.3_

- [ ] 15. Checkpoint - Integration Complete
  - Ensure all integration tests pass
  - Verify Stream/Demuxer tag access works
  - Ask user if questions arise

- [ ] 16. Implement Fuzzing Tests
  - [ ] 16.1 Create VorbisComment fuzzer
    - Implement fuzzer target in `tests/fuzz_tag_vorbis.cpp`
    - Create seed corpus with valid VorbisComment data
    - _Requirements: 10.1, 10.3_

  - [ ] 16.2 Create ID3v1 fuzzer
    - Implement fuzzer target in `tests/fuzz_tag_id3v1.cpp`
    - Create seed corpus with valid ID3v1 data
    - _Requirements: 10.1, 10.3_

  - [ ] 16.3 Create ID3v2 fuzzer with "almost-right" corpus
    - Implement fuzzer target in `tests/fuzz_tag_id3v2.cpp`
    - Create comprehensive seed corpus with mutation strategies
    - Include header mutations, frame size attacks, text exploits
    - Include APIC attacks, unsync edge cases, boundary conditions
    - _Requirements: 10.1, 10.3_

  - [ ] 16.4 Create picture data fuzzer
    - Implement fuzzer target in `tests/fuzz_tag_picture.cpp`
    - Test APIC and METADATA_BLOCK_PICTURE parsing
    - _Requirements: 10.1, 10.3_

- [ ] 17. Update Build System
  - [ ] 17.1 Update src/tag/Makefile.am
    - Add new source files to build
    - Ensure proper dependencies
    - _Requirements: All_

  - [ ] 17.2 Update tests/Makefile.am
    - Add new test files
    - Add fuzzer targets (conditional on fuzzer availability)
    - _Requirements: All_

  - [ ] 17.3 Update psymp3.final.cpp
    - Include new tag source files
    - _Requirements: All_

- [ ] 18. Final Checkpoint - All Tests Pass
  - Run full test suite
  - Run property tests with 100+ iterations
  - Run fuzzers for minimum duration
  - Verify clean build
  - Ask user to verify functionality

## Notes

- All tasks are required for comprehensive implementation
- Each task references specific requirements for traceability
- Checkpoints ensure incremental validation
- Property tests validate universal correctness properties
- Unit tests validate specific examples and edge cases
- Fuzzing tests validate security and robustness

