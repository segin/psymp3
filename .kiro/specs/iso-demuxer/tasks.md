# ISO Demuxer Implementation Plan

- [x] 1. Set up core ISO demuxer structure and interfaces
  - Create ISODemuxer class inheriting from Demuxer base class
  - Implement basic constructor and destructor with proper cleanup
  - Add member variables for core components (BoxParser, SampleTableManager, etc.)
  - _Requirements: 10.1, 10.2_

- [x] 2. Implement basic box parsing infrastructure
  - Create BoxParser class with box header reading functionality
  - Implement FOURCC constants for all ISO box types
  - Add recursive box parsing with proper size validation
  - Write unit tests for box header parsing and validation
  - _Requirements: 1.1, 1.2, 1.8_

- [x] 3. Implement file type and movie box parsing
  - Add ftyp box parsing to identify container variants (MP4, M4A, MOV, 3GP)
  - Implement moov box parsing to extract movie header information
  - Create track enumeration from trak boxes within moov
  - _Requirements: 1.1, 1.3, 9.1, 9.2, 9.3, 9.4_

- [x] 4. Create audio track identification and codec detection
  - Implement track box parsing to identify audio tracks by handler type 'soun'
  - Add sample description box parsing for codec identification
  - Create codec-specific configuration extraction (AAC, ALAC, mulaw, alaw, PCM)
  - _Requirements: 1.6, 1.7, 2.1, 2.2, 2.3, 2.4, 2.5_

- [x] 5. Implement sample table parsing and management
  - Create SampleTableManager class for efficient sample lookups
  - Implement parsing of stts (time-to-sample), stsc (sample-to-chunk), stsz (sample sizes), stco/co64 (chunk offsets)
  - Add sample table validation and consistency checking
  - _Requirements: 1.4, 5.1, 5.2, 5.3, 5.4, 5.5_

- [x] 6. Create sample extraction and media chunk generation
  - Implement sample data extraction from mdat boxes using sample tables
  - Create MediaChunk objects with proper timing and codec information
  - Add support for variable sample sizes and chunk-based storage
  - _Requirements: 1.5, 10.4, 5.6, 5.7_

- [x] 7. Implement seeking functionality with sample table navigation
  - Create SeekingEngine for timestamp-to-sample conversion
  - Implement binary search for efficient time-to-sample lookups
  - Add keyframe-aware seeking using sync sample tables when available
  - _Requirements: 5.1, 5.2, 5.6, 5.7, 10.5_

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
  - Implement traf (track fragment) and trun (track run) parsing
  - Add fragment-based sample table updates and navigation
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7, 3.8_

- [x] 11. Implement progressive download and streaming support
  - Add support for incomplete files with movie box at end
  - Implement byte range request handling for streaming scenarios
  - Create buffering logic for samples not yet available
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 6.7, 6.8_

- [x] 12. Add comprehensive error handling and recovery mechanisms
  - Implement graceful handling of corrupted boxes and invalid sizes
  - Add sample table repair and consistency validation
  - Create error recovery for missing codec configuration
  - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7, 7.8_

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

- [x] 15. Integrate with PsyMP3 demuxer architecture
  - Implement all required Demuxer interface methods
  - Add proper StreamInfo population with codec and format details
  - Integrate with PsyMP3 error reporting and logging systems
  - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.5, 10.6, 10.7, 10.8_

- [-] 16. Add standards compliance validation and quality assurance
  - Implement ISO/IEC 14496-12 specification compliance checking
  - Add support for both 32-bit and 64-bit box sizes
  - Validate timestamp handling and timescale configurations
  - _Requirements: 12.1, 12.2, 12.3, 12.4, 12.5, 12.6, 12.7, 12.8_

- [ ] 17. Integrate compliance validator with main demuxer
  - Add ISODemuxerComplianceValidator to psymp3.h header inclusion
  - Initialize compliance validator in ISODemuxer constructor
  - Add compliance validation calls during box parsing and track processing
  - Implement compliance reporting in demuxer error handling
  - _Requirements: 12.1, 12.8_

- [ ] 18. Add comprehensive test coverage for compliance validation
  - Create unit tests for box structure validation (32-bit and 64-bit sizes)
  - Add tests for timestamp and timescale validation
  - Implement sample table consistency validation tests
  - Create codec-specific data integrity tests
  - Add container format compliance tests
  - _Requirements: 12.1, 12.2, 12.3, 12.4, 12.5, 12.6, 12.7, 12.8_

- [ ] 19. Implement performance optimization and memory management
  - Add lazy loading for large sample size tables in SampleTableManager
  - Implement compressed sample-to-chunk mappings for memory efficiency
  - Optimize binary search structures for time-to-sample lookups
  - Add memory usage profiling and optimization for large files
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 8.8_

- [ ] 20. Add FLAC-in-MP4 codec support
  - Implement FLAC codec detection and configuration extraction for ISO containers
  - Add support for FLAC sample description box parsing (fLaC codec type)
  - Create FLAC-specific configuration data extraction from sample description
  - Implement FLAC frame boundary detection within MP4 sample data
  - Add FLAC metadata block handling within ISO container context
  - Integrate with existing FLACCodec for decoding FLAC samples from MP4
  - _Requirements: 2.1, 2.2, 2.7, 10.4_

- [ ] 21. Add comprehensive integration testing
  - Create tests with real-world MP4/M4A files from various encoders
  - Add fragmented MP4 streaming scenario tests
  - Implement seeking accuracy validation across different codecs
  - Add telephony codec (mulaw/alaw) integration tests
  - Add FLAC-in-MP4 integration tests with various FLAC configurations
  - Create error handling and recovery scenario tests
  - _Requirements: All requirements validation_
