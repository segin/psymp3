# ISO Demuxer Implementation Plan

- [x] 1. Set up core ISO demuxer structure and interfaces
  - Create ISODemuxer class inheriting from Demuxer base class
  - Implement basic constructor and destructor with proper cleanup
  - Add member variables for core components (BoxParser, SampleTableManager, FragmentHandler, MetadataExtractor, StreamManager, SeekingEngine, ComplianceValidator, SecurityValidator)
  - Add file type information members (majorBrand, compatibleBrands, movieTimescale, movieDuration)
  - _Requirements: 10.1, 10.2, 15.1, 15.2_

- [x] 2. Implement basic box parsing infrastructure
  - Create BoxParser class with box header reading functionality
  - Implement FOURCC constants for all ISO box types (including fragmented MP4, handler types, brands, track flags)
  - Add support for both 32-bit and 64-bit box sizes (extended size when size==1, extends to EOF when size==0)
  - Add FullBox header parsing with version and flags fields
  - Implement iterative box parsing with recursion depth limiting (MAX_RECURSION_DEPTH=32)
  - Add box size validation against parent size
  - Write unit tests for box header parsing, size validation, and version handling
  - _Requirements: 1.1, 1.2, 1.8, 18.1-18.8, 19.1-19.8_

- [x] 3. Implement file type and movie box parsing
  - Add ftyp box parsing to extract major brand and compatible brands
  - Implement brand identification for isom, mp41, mp42, avc1, M4A, qt, 3gp5, 3gp6
  - Add MIME type determination based on track content (video/mp4, audio/mp4, application/mp4)
  - Implement file extension association (.mp4, .m4a, .m4v, .mpg4, .mov, .3gp, .f4a)
  - Add moov box parsing to extract movie header information (mvhd)
  - Parse movie header with support for both version 0 (32-bit) and version 1 (64-bit) fields
  - Extract movie timescale, duration, rate, volume, matrix, nextTrackId
  - Create track enumeration from trak boxes within moov
  - _Requirements: 1.1, 1.3, 9.1-9.8, 13.1-13.8, 15.1-15.8, 16.1-16.8, 20.1-20.8_

- [x] 4. Create track identification and codec detection
  - Implement track header box (tkhd) parsing with version 0/1 support
  - Extract track ID, flags (enabled, in movie, in preview), duration, layer, alternate group
  - Extract track dimensions (width, height), volume, and transformation matrix
  - Validate track ID is not 0 (reject file if invalid)
  - Implement media box (mdia) parsing with media header (mdhd) and handler (hdlr)
  - Parse media header with version 0/1 support for timescale, duration, language
  - Identify track type by handler type ('soun' for audio, 'vide' for video, 'hint', 'meta', 'text')
  - Add sample description box parsing for codec identification
  - Create codec-specific configuration extraction (AAC, ALAC, mulaw, alaw, PCM, FLAC, AVC)
  - Implement AVC configuration parsing (avcC box) with SPS/PPS extraction
  - _Requirements: 1.6, 1.7, 2.1-2.8, 14.1-14.8, 23.1-23.8, 25.1-25.8_

- [x] 5. Implement sample table parsing and management
  - Create SampleTableManager class for efficient sample lookups
  - Implement parsing of stsd (sample description) with data reference index extraction
  - Implement parsing of stsz (sample sizes) with support for both per-sample and constant sizes
  - Implement parsing of stsc (sample-to-chunk) with first chunk and samples per chunk mapping
  - Implement parsing of stco (32-bit chunk offsets) and co64 (64-bit chunk offsets for large files)
  - Implement parsing of stts (time-to-sample) with sample counts and deltas
  - Implement parsing of ctts (composition time-to-sample) for decode/presentation time offsets
  - Implement parsing of stss (sync samples) for keyframe identification (treat all as sync if absent)
  - Add binary search structures for efficient time-to-sample lookups
  - Implement compressed sample-to-chunk mappings for memory efficiency
  - Add sample table validation and consistency checking
  - Support lazy loading of large sample size tables
  - _Requirements: 1.4, 5.1-5.8, 21.1-21.8_

- [x] 6. Create sample extraction and media chunk generation
  - Implement sample data extraction from mdat boxes using sample tables
  - Create MediaChunk objects with proper timing and codec information
  - Add support for variable sample sizes and chunk-based storage
  - _Requirements: 1.5, 10.4, 5.6, 5.7_

- [x] 7. Implement seeking functionality with sample table navigation and edit list support
  - Create SeekingEngine for timestamp-to-sample conversion
  - Implement binary search for efficient time-to-sample lookups
  - Add keyframe-aware seeking using sync sample tables when available
  - Implement edit list (edts/elst) parsing with version 0/1 support
  - Add edit list application for timeline modifications
  - Implement presentation time to media time mapping
  - Handle empty edits (media time = -1)
  - Support media rate modifications in edit segments
  - Add identity mapping when edit list is absent
  - _Requirements: 5.1, 5.2, 5.6, 5.7, 10.5, 24.1-24.8_

- [x] 8. Add telephony audio codec support (mulaw/alaw)
  - Implement mulaw codec detection and configuration for 8kHz/16kHz sample rates
  - Add alaw codec support with European telephony standard compliance
  - Create raw sample data extraction for companded audio formats
  - _Requirements: 11.1, 11.2, 11.3, 11.4, 11.5, 11.6, 11.7, 11.8_
-
- [x] 9. Implement metadata extraction from various atom types
  - Create MetadataExtractor class for iTunes-style metadata parsing
  - Add support for udta (user data) and meta boxes
  - Implement extraction of title, artist, album, genre, and artwork
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 4.7, 4.8_

- [x] 10. Add fragmented MP4 support for streaming scenarios
  - Create FragmentHandler class for moof (movie fragment) processing
  - Implement mvex (movie extends) box parsing with mehd and trex boxes
  - Parse track extends (trex) for default fragment values per track
  - Implement moof (movie fragment) and mfhd (movie fragment header) parsing
  - Implement traf (track fragment) and tfhd (track fragment header) parsing
  - Implement trun (track run) parsing with sample durations, sizes, flags, and composition offsets
  - Add fragment-based sample table updates and navigation
  - Implement sidx (segment index) parsing for segment-based navigation
  - Add mfra (movie fragment random access) and tfra parsing for seeking within fragments
  - Handle incomplete fragments without errors
  - Implement fragment buffering and reordering for out-of-order arrival
  - Use default values from movie header when fragment headers are missing
  - _Requirements: 3.1-3.8_

- [x] 11. Implement progressive download and streaming support
  - Add support for incomplete files with movie box at end
  - Implement byte range request handling for streaming scenarios
  - Create buffering logic for samples not yet available
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 6.7, 6.8_

- [x] 12. Add comprehensive error handling and recovery mechanisms
  - Implement graceful handling of corrupted boxes and invalid sizes
  - Add sample table repair and consistency validation
  - Create error recovery for missing codec configuration
  - Implement box size validation and repair
  - Add handling for missing mandatory boxes
  - Create recovery log for tracking repair attempts
  - _Requirements: 7.1-7.8_

- [ ] 12.1. Implement security validation component
  - Create SecurityValidator class for buffer overflow protection
  - Implement sample size validation (MAX_SAMPLE_SIZE = 100MB)
  - Add allocation size validation (MAX_ALLOCATION_SIZE = 1GB)
  - Implement buffer access validation
  - Add integer overflow protection for box size calculations
  - Implement recursion depth limiting (MAX_RECURSION_DEPTH = 32)
  - Add NAL unit size validation for AVC (MAX_NAL_UNIT_SIZE = 50MB)
  - Implement codec configuration data validation
  - Add decoder buffer model validation per RFC 4337
  - Create SecurityErrorHandler for violation reporting
  - _Requirements: 17.1-17.8_

- [ ] 12.2. Implement compliance validation component
  - Create ComplianceValidator class for ISO/IEC 14496-12 compliance
  - Implement mandatory box structure validation (ftyp, moov, mvhd, trak, tkhd, mdia, mdhd, hdlr, minf, stbl)
  - Add box hierarchy validation
  - Implement file type box validation
  - Add movie header, track header, media header validation
  - Implement sample table box validation
  - Add timescale and duration validation
  - Implement box version and size validation
  - Create ComplianceErrorHandler with severity levels (INFO, WARNING, ERROR, FATAL)
  - _Requirements: 18.1-18.8, 19.1-19.8, 20.1-20.8, 21.1-21.8_

- [x] 13. Optimize performance and memory usage
  - Implement lazy loading for large sample size tables
  - Add compressed sample-to-chunk mappings for memory efficiency
  - Optimize binary search structures for time-to-sample lookups  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 8.8_

- [x] 14. Fix header inclusion policy compliance
  - Replace individual header includes with master header psymp3.h
  - Move ISODemuxer.h, algorithm, stdexcept, and cstring includes to psymp3.h
  - Ensure ISODemuxer.cpp only includes psymp3.h as per project standards
  - Remove duplicate function definitions that belong in separate component files
  - _Requirements: Project header inclusion policy_

- [ ] 14.1. Implement data reference handling
  - Create DataReference structure for URL, URN, and self-reference types
  - Implement dinf (data information) box parsing
  - Implement dref (data reference) box parsing with all entries
  - Add support for url and urn data references
  - Handle self-reference flag (flag = 1) for same-file data
  - Implement data reference index usage from sample descriptions
  - Add external reference resolution through IOHandler
  - Validate data references and report errors for invalid references
  - _Requirements: 22.1-22.8_

- [x] 15. Integrate with PsyMP3 demuxer architecture
  - Implement all required Demuxer interface methods
  - Add proper StreamInfo population with codec and format details for both audio and video
  - Add GetMIMEType(), GetMajorBrand(), GetCompatibleBrands() methods
  - Implement track filtering based on flags (enabled, in movie, in preview)
  - Integrate with PsyMP3 error reporting and logging systems
  - Add security and compliance validation integration
  - _Requirements: 10.1-10.8, 13.1-13.8_

- [x] 16. Add standards compliance validation and quality assurance
  - Implement ISO/IEC 14496-12 specification compliance checking
  - Add support for both 32-bit and 64-bit box sizes
  - Validate timestamp handling and timescale configurations
  - _Requirements: 12.1, 12.2, 12.3, 12.4, 12.5, 12.6, 12.7, 12.8_

- [x] 17. Integrate compliance validator with main demuxer
  - Add ISODemuxerComplianceValidator to psymp3.h header inclusion
  - Initialize compliance validator in ISODemuxer constructor
  - Add compliance validation calls during box parsing and track processing
  - Implement compliance reporting in demuxer error handling
  - _Requirements: 12.1, 12.8_

- [x] 18. Add comprehensive test coverage for compliance validation
  - Create unit tests for box structure validation (32-bit and 64-bit sizes)
  - Add tests for timestamp and timescale validation
  - Implement sample table consistency validation tests
  - Create codec-specific data integrity tests
  - Add container format compliance tests
  - _Requirements: 12.1-12.8_

- [ ] 18.1. Add security validation test coverage
  - Create tests for buffer overflow protection
  - Add tests for integer overflow detection
  - Implement recursion depth limit tests
  - Create NAL unit size validation tests
  - Add allocation limit tests
  - Implement decoder buffer model validation tests
  - Create tests with malformed files triggering security violations
  - _Requirements: 17.1-17.8_

- [ ] 18.2. Add compliance validation test coverage
  - Create tests for mandatory box presence validation
  - Add tests for box hierarchy validation
  - Implement version handling tests (version 0 and 1)
  - Create timescale and duration validation tests
  - Add track flag validation tests
  - Implement data reference validation tests
  - Create edit list validation tests
  - Add tests with non-compliant files
  - _Requirements: 18.1-18.8, 19.1-19.8, 20.1-20.8, 21.1-21.8, 22.1-22.8, 23.1-23.8, 24.1-24.8, 25.1-25.8_

- [x] 19. Implement performance optimization and memory management
  - Add lazy loading for large sample size tables in SampleTableManager
  - Implement compressed sample-to-chunk mappings for memory efficiency
  - Optimize binary search structures for time-to-sample lookups
  - Add memory usage profiling and optimization for large files
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 8.8_

- [x] 20. Add FLAC-in-MP4 codec support
  - Implement FLAC codec detection and configuration extraction for ISO containers
  - Add support for FLAC sample description box parsing (fLaC codec type)
  - Create FLAC-specific configuration data extraction from sample description
  - Implement FLAC frame boundary detection within MP4 sample data
  - Add FLAC metadata block handling within ISO container context
  - Integrate with existing FLACCodec for decoding FLAC samples from MP4
  - _Requirements: 2.1, 2.2, 2.7, 10.4_

- [x] 21. Add comprehensive integration testing
  - Create tests with real-world MP4/M4A files from various encoders
  - Add fragmented MP4 streaming scenario tests (DASH, HLS)
  - Implement seeking accuracy validation across different codecs
  - Add telephony codec (mulaw/alaw) integration tests
  - Add FLAC-in-MP4 integration tests with various FLAC configurations
  - Create error handling and recovery scenario tests
  - _Requirements: All requirements validation_

- [ ] 21.1. Add AVC/H.264 video integration testing
  - Create tests with AVC video files (MP4, M4V)
  - Add tests for avcC box parsing and SPS/PPS extraction
  - Implement NAL unit length to start code conversion tests
  - Test various NAL unit length sizes (1, 2, 4 bytes)
  - Add tests for profile, level, and constraint flags
  - Test multiple parameter sets handling
  - Create tests with various AVC brands (avc1, avc2, avc3, avc4)
  - _Requirements: 14.1-14.8_

- [ ] 21.2. Add multi-track and brand compatibility testing
  - Create tests with multiple audio tracks
  - Add tests with multiple video tracks
  - Implement tests with mixed audio/video tracks
  - Test various brand combinations (isom, mp41, mp42, avc1, M4A, qt, 3gp)
  - Add MIME type determination tests (video/mp4, audio/mp4, application/mp4)
  - Test file extension association for various brands
  - Create tests with track flags (enabled, in movie, in preview)
  - _Requirements: 9.1-9.8, 13.1-13.8, 15.1-15.8, 16.1-16.8, 25.1-25.8_

- [ ] 21.3. Add edit list and timeline modification testing
  - Create tests with simple edit lists
  - Add tests with empty edits (media time = -1)
  - Implement tests with media rate modifications
  - Test multiple edit segments
  - Add seeking tests with edit lists applied
  - Test presentation time to media time mapping
  - Create tests with identity mapping (no edit list)
  - _Requirements: 24.1-24.8_

- [ ] 21.4. Add large file and performance testing
  - Create tests with files >4GB requiring 64-bit offsets (co64)
  - Add tests with millions of samples
  - Implement memory usage profiling tests
  - Test lazy loading of large sample tables
  - Add performance benchmarking for parsing and seeking
  - Test with files from various encoders and sources
  - _Requirements: 8.1-8.8, 19.1-19.8_

- [x] 22. Fix critical test unit failures and segmentation faults
  - Fixed segmentation fault in ISO demuxer parseContainer method
  - Corrected ComplianceValidationResult structure usage and field access
  - Fixed performIOWithRetry lambda expressions and error handling
  - Resolved test_seek_deadlock threading safety issues and added to build system
  - Improved file size handling and position validation in threading tests
  - _Requirements: Test stability and reliability_

- [x] 23. Fix missing Makefile.am configurations for ISO test files
  - Added proper build configurations for all missing ISO demuxer test files
  - Fixed corrupted Makefile.am content and formatting issues
  - Added configurations for compliance, integration, FLAC, performance, and additional tests
  - All ISO test files now build successfully with proper dependency linking
  - Regenerated configure script and verified build system functionality
  - _Requirements: Complete test coverage and build system integrity_

## Test Status Summary

### Completed Tasks ✅
Tasks 1-23 have been completed, covering:
- Core ISO demuxer structure and interfaces
- Box parsing infrastructure with 32-bit/64-bit support
- File type, brand identification, and MIME type determination
- Track identification for audio and video
- Sample table parsing and management
- Sample extraction and media chunk generation
- Seeking with edit list support
- Telephony codec support (mulaw/alaw)
- Metadata extraction
- Fragmented MP4 support
- Progressive download and streaming
- Error handling and recovery
- Performance optimization
- Header inclusion policy compliance
- PsyMP3 architecture integration
- Standards compliance validation
- Compliance validator integration
- Initial test coverage
- Performance optimization
- FLAC-in-MP4 support
- Comprehensive integration testing
- Critical bug fixes
- Build system configuration

### Remaining Tasks 📋
The following tasks remain to complete full ISO/IEC 14496-12, ISO/IEC 14496-15, and RFC 4337 compliance:

**Security and Compliance (Tasks 12.1-12.2):**
- Security validation component with buffer overflow protection
- Compliance validation component with mandatory box checking

**Data References (Task 14.1):**
- Data reference handling for URL, URN, and self-reference types

**Testing (Tasks 18.1-18.2, 21.1-21.4):**
- Security validation test coverage
- Compliance validation test coverage
- AVC/H.264 video integration testing
- Multi-track and brand compatibility testing
- Edit list and timeline modification testing
- Large file and performance testing

### Fixed Critical Issues ✅
- **Segmentation fault in ISO demuxer**: Fixed memory corruption in parseContainer method
- **Threading safety test**: Fixed test_seek_deadlock and added to build system
- **Compilation errors**: Resolved ComplianceValidationResult structure issues
- **Missing Makefile.am configurations**: All ISO test files now properly configured

### Current Status
The core ISO demuxer implementation is stable and functional with support for:
- ✅ Audio codecs: AAC, ALAC, mulaw, alaw, PCM, FLAC
- ✅ Container variants: MP4, M4A, MOV, 3GP
- ✅ Fragmented MP4 streaming
- ✅ Sample-accurate seeking
- ✅ Metadata extraction
- ✅ Edit list support
- ✅ Brand identification and MIME type determination

Remaining work focuses on:
- 🔄 Security validation and buffer overflow protection
- 🔄 Full compliance validation
- 🔄 AVC/H.264 video codec support
- 🔄 Data reference handling
- 🔄 Comprehensive test coverage for new features