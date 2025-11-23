# **DEMUXER ARCHITECTURE IMPLEMENTATION PLAN**

## **Overview**

This implementation plan focuses on completing the remaining work for the demuxer architecture. Most of the core infrastructure is already in place. The primary remaining task is completing the ISODemuxer implementation for MP4/M4A container support.

## **Completed Work**

The following components have been successfully implemented:
- ✅ Base Demuxer interface and data structures (StreamInfo, MediaChunk)
- ✅ DemuxerFactory with format detection and demuxer creation
- ✅ OggDemuxer with full RFC 3533 compliance
- ✅ ChunkDemuxer for RIFF/WAV and AIFF formats
- ✅ RawAudioDemuxer for PCM, A-law, μ-law formats
- ✅ MediaFactory with extensible format registration
- ✅ DemuxedStream bridge for legacy Stream interface compatibility
- ✅ Performance optimizations (memory management, I/O, CPU efficiency)
- ✅ Error handling and robustness mechanisms
- ✅ Thread safety with public/private lock pattern
- ✅ Extensibility and plugin support architecture
- ✅ PsyMP3 system integration (IOHandler, Debug logging, URI handling)
- ✅ Comprehensive testing suite
- ✅ API documentation and developer guides
- ✅ Backward compatibility validation

## **Remaining Implementation Tasks**

- [ ] 1. Complete ISODemuxer Implementation
  - Implement full ISO Base Media File Format (MP4/M4A) support
  - Add hierarchical box structure parsing
  - Implement sample table processing for seeking
  - Support multiple tracks and codec detection
  - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7, 5.8, 5.9, 5.10_

- [ ] 1.1 Implement Box Hierarchy Parser
  - Create box header parsing (32-bit and 64-bit sizes)
  - Implement recursive box tree navigation
  - Add support for essential boxes (ftyp, moov, trak, mdia, stbl)
  - Handle unknown boxes gracefully by skipping
  - _Requirements: 5.1, 5.2_

- [ ] 1.2 Parse Movie and Track Metadata
  - Extract movie header (mvhd) with timescale and duration
  - Parse track headers (tkhd) for track information
  - Extract media headers (mdhd) for media-specific timescales
  - Identify and prioritize audio tracks over video tracks
  - _Requirements: 5.3, 5.5_

- [ ] 1.3 Build Sample Tables
  - Parse time-to-sample table (stts) for decode timestamps
  - Parse sample-to-chunk table (stsc) for chunk mapping
  - Parse sample size table (stsz/stz2) for sample sizes
  - Parse chunk offset table (stco/co64) for file positions
  - Build efficient lookup structures for seeking
  - _Requirements: 5.4, 5.6_

- [ ] 1.4 Implement Codec Detection
  - Parse sample description box (stsd) for codec information
  - Map FourCC codes to codec names (mp4a→AAC, alac→ALAC, flac→FLAC, etc.)
  - Extract codec-specific initialization data (esds, alac, dfLa boxes)
  - Populate StreamInfo with complete codec information
  - _Requirements: 5.8_

- [ ] 1.5 Implement Sample-Based Seeking
  - Convert timestamp to sample number using stts table
  - Map sample to chunk using stsc table
  - Calculate byte offset using stco/co64 table
  - Extract sample data using stsz table
  - Handle sync samples (stss) for video tracks
  - _Requirements: 5.6, 5.7_

- [ ] 1.6 Add Duration Calculation
  - Calculate duration from track duration and timescale
  - Handle multiple tracks with different timescales
  - Convert to milliseconds for unified interface
  - Support files with unknown duration (live streams)
  - _Requirements: 5.9_

- [ ] 1.7 Implement Fragmented MP4 Support (Basic)
  - Detect fragmented MP4 files (moof boxes)
  - Parse movie fragment boxes for basic playback
  - Handle fragment-based seeking (limited support)
  - Provide foundation for future full fragmentation support
  - _Requirements: 5.10_

- [ ] 1.8 Create ISODemuxer Unit Tests
  - Test box parsing with various MP4/M4A files
  - Verify sample table construction and lookup
  - Test seeking accuracy with different codecs
  - Validate multi-track handling
  - Test fragmented MP4 basic support
  - _Requirements: 5.1-5.10_

- [ ] 1.9 Add ISODemuxer Integration Tests
  - Test with real-world MP4/M4A files from various encoders
  - Verify codec detection for AAC, ALAC, FLAC in MP4
  - Test large file support (>2GB with 64-bit offsets)
  - Validate metadata extraction from iTunes-style atoms
  - Test error handling with corrupted MP4 files
  - _Requirements: 5.1-5.10, 11.1-11.8_

- [ ] 2. Integrate ISODemuxer with Factory System
  - Register ISODemuxer with DemuxerFactory
  - Add MP4 format signatures to detection system
  - Test automatic format detection for MP4/M4A/MOV/3GP files
  - Verify integration with DemuxedStream bridge
  - _Requirements: 7.6, 8.1-8.10_

- [ ] 2.1 Register MP4 Format Signatures
  - Add ftyp box detection (various brand codes: isom, mp42, M4A, etc.)
  - Handle different MP4 variants (MP4, M4A, MOV, 3GP)
  - Set appropriate detection priority
  - Test with ambiguous files
  - _Requirements: 7.6_

- [ ] 2.2 Test End-to-End MP4 Playback
  - Verify MP4/M4A files play through DemuxedStream
  - Test seeking accuracy in MP4 files
  - Validate metadata extraction from MP4 containers
  - Ensure performance meets requirements
  - _Requirements: 9.1-9.10, 14.1-14.10_

- [ ] 3. Final Validation and Documentation
  - Update API documentation with ISODemuxer details
  - Add MP4 format examples to developer guide
  - Run full test suite to ensure no regressions
  - Validate all requirements are met
  - _Requirements: All requirements_

- [ ] 3.1 Update Documentation
  - Document ISODemuxer public API
  - Add MP4 box structure reference
  - Provide examples of MP4 demuxing
  - Document limitations and future enhancements
  - _Requirements: 14.8, 14.9, 14.10_

- [ ] 3.2 Run Comprehensive Test Suite
  - Execute all unit tests
  - Run integration tests with all formats
  - Perform performance benchmarks
  - Validate thread safety
  - Test backward compatibility
  - _Requirements: All requirements_

- [ ] 3.3 Final Requirements Validation
  - Verify all 14 requirement sections are satisfied
  - Confirm all 20 correctness properties are testable
  - Validate error handling meets specifications
  - Ensure thread safety compliance
  - Document any known limitations
  - _Requirements: All requirements_