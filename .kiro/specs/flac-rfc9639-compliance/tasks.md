# Implementation Plan

## Overview

This implementation plan focuses on rebuilding the broken FLAC demuxer to achieve RFC 9639 compliance and restore basic functionality. The tasks are organized to address the most critical demuxer failures first, then build up the RFC compliance validation layer.

## Task List

- [ ] 1. Fix Core FLAC Demuxer Frame Boundary Detection
  - Implement RFC 9639 compliant 15-bit sync pattern detection (0b1111 1111 1111 1110)
  - Fix the broken `findNextFrame()` method that causes infinite loops
  - Add robust frame size estimation and boundary detection
  - Implement proper sync pattern search with configurable search windows
  - _Requirements: 2.1, 2.2, 2.3, 2.4_

- [x] 1.1 Implement RFC 9639 sync pattern validation
  - Create `validateFrameSync_unlocked()` method with correct 15-bit pattern
  - Fix sync code validation in `parseFrameHeader()` to use (sync_code & 0xFFFC) != 0xFFF8
  - Add sync pattern search in `searchSyncPattern()` with proper bit masking
  - _Requirements: 2.1_

- [ ] 1.2 Fix frame boundary detection algorithm
  - Rewrite `findNextFrame_unlocked()` to prevent infinite loops
  - Implement conservative frame size estimation using STREAMINFO min_frame_size
  - Add frame end detection using next sync pattern search
  - Implement fallback strategies when frame end cannot be determined
  - _Requirements: 2.2, 2.4_

- [ ] 1.3 Implement robust sync pattern search
  - Create `searchSyncPattern_unlocked()` with efficient byte-by-byte search
  - Add configurable search window limits to prevent excessive I/O
  - Implement sync pattern validation at candidate positions
  - Add performance optimization for common frame sizes
  - _Requirements: 2.3_

- [ ] 1.4 Add frame size calculation and validation
  - Fix `calculateFrameSize_unlocked()` to use STREAMINFO hints properly
  - Implement frame size estimation based on audio format parameters
  - Add frame size validation against STREAMINFO min/max constraints
  - Create fallback size estimation for files without STREAMINFO hints
  - _Requirements: 2.4_

- [ ] 2. Fix FLAC Demuxer Metadata Parsing
  - Correct STREAMINFO bit-field extraction errors that cause invalid parameters
  - Fix metadata block header parsing and validation
  - Implement proper error recovery for corrupted metadata blocks
  - Add comprehensive metadata block size and structure validation
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5_

- [ ] 2.1 Fix STREAMINFO parsing bit-field extraction
  - Correct sample rate extraction (20 bits) in `parseStreamInfoBlock_unlocked()`
  - Fix channel count extraction (3 bits + 1) with proper bit masking
  - Correct bits per sample extraction (5 bits + 1) from packed fields
  - Fix total samples extraction (36 bits) across multiple bytes
  - Validate all extracted values against RFC 9639 constraints
  - _Requirements: 1.3_

- [ ] 2.2 Implement robust metadata block header parsing
  - Fix `parseMetadataBlockHeader_unlocked()` to handle all block types correctly
  - Add proper validation of block length fields and last-block flags
  - Implement metadata block size limits to prevent memory exhaustion
  - Add error recovery for corrupted block headers
  - _Requirements: 1.1, 1.2_

- [ ] 2.3 Add STREAMINFO recovery mechanisms
  - Implement `attemptStreamInfoRecovery_unlocked()` to derive STREAMINFO from first frame
  - Add validation of recovered STREAMINFO parameters
  - Create fallback STREAMINFO values for completely missing blocks
  - Implement STREAMINFO consistency checking across frames
  - _Requirements: 1.4_

- [ ] 2.4 Fix SEEKTABLE and VORBIS_COMMENT parsing
  - Correct SEEKTABLE entry parsing with proper big-endian byte order
  - Add SEEKTABLE validation against STREAMINFO total samples
  - Fix VORBIS_COMMENT UTF-8 handling and field parsing
  - Implement memory limits for large comment blocks
  - _Requirements: 5.2, 5.1_

- [ ] 2.5 Implement metadata error recovery
  - Create `recoverFromCorruptedMetadata_unlocked()` for block-level recovery
  - Add metadata block skipping for non-critical corrupted blocks
  - Implement audio data start detection when metadata parsing fails
  - Add comprehensive error logging for metadata parsing failures
  - _Requirements: 4.1, 4.4_

- [ ] 3. Implement RFC 9639 Frame Header Parsing
  - Fix frame header parsing to handle all RFC 9639 field combinations correctly
  - Add proper UTF-8 decoding for frame/sample numbers
  - Implement complete frame header validation including reserved bits
  - Add support for variable vs. fixed block size strategies
  - _Requirements: 2.2, 7.4_

- [ ] 3.1 Fix frame header field parsing
  - Correct blocking strategy parsing in `parseFrameHeader_unlocked()`
  - Fix block size parsing for all encoding modes (direct, 8-bit, 16-bit)
  - Correct sample rate parsing for all encoding modes including custom rates
  - Fix channel assignment parsing and validation
  - Correct bit depth parsing for all valid bit depths
  - _Requirements: 2.2_

- [ ] 3.2 Implement UTF-8 frame number decoding
  - Create `decodeUTF8Number_unlocked()` for frame/sample number parsing
  - Add proper UTF-8 sequence validation and error handling
  - Support 1-7 byte UTF-8 sequences as per FLAC specification
  - Add bounds checking for decoded frame numbers
  - _Requirements: 2.2_

- [ ] 3.3 Add frame header validation
  - Implement `validateFrameHeader_unlocked()` with comprehensive checks
  - Add reserved bit validation (must be zero)
  - Validate frame header consistency with STREAMINFO
  - Add frame header CRC-8 validation per RFC 9639 Section 9.1.8
  - _Requirements: 7.4, 7.1_

- [ ] 3.4 Support variable and fixed block size strategies
  - Implement proper sample position tracking for variable block size
  - Add frame number vs. sample number handling based on blocking strategy
  - Validate block size constraints for each strategy type
  - Update position tracking logic to handle both strategies correctly
  - _Requirements: 6.4_

- [ ] 4. Implement RFC 9639 CRC Validation
  - Add CRC-8 validation for frame headers per RFC 9639 Section 9.1.8
  - Implement CRC-16 validation for frame footers per RFC 9639 Section 9.3
  - Create configurable CRC validation modes (disabled, enabled, strict)
  - Add CRC calculation methods with correct polynomials
  - _Requirements: 7.1, 7.2, 7.5_

- [ ] 4.1 Implement CRC-8 header validation
  - Create `calculateHeaderCRC8_unlocked()` with polynomial x^8 + x^2 + x^1 + x^0 (0x07)
  - Add CRC-8 validation in frame header parsing
  - Implement CRC-8 error logging and recovery options
  - Add performance optimization for CRC calculation
  - _Requirements: 7.1_

- [ ] 4.2 Implement CRC-16 frame validation
  - Create `calculateFrameCRC16_unlocked()` with polynomial x^16 + x^15 + x^2 + x^0 (0x8005)
  - Add CRC-16 validation for complete frames
  - Implement CRC-16 error handling and recovery strategies
  - Add frame CRC validation during frame reading
  - _Requirements: 7.2_

- [ ] 4.3 Add configurable CRC validation modes
  - Create CRC validation configuration options (disabled/enabled/strict)
  - Implement CRC error counting and threshold-based disabling
  - Add CRC validation statistics and reporting
  - Create CRC error recovery strategies based on configuration
  - _Requirements: 7.5_

- [ ] 5. Fix FLAC Demuxer Seeking Architecture
  - Rebuild the broken seeking system with frame indexing support
  - Fix sample position tracking throughout the stream
  - Implement multiple seeking strategies with fallback mechanisms
  - Add proper seeking validation and error handling
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5_

- [ ] 5.1 Implement frame indexing system
  - Create `FLACFrameIndex` class with efficient frame position storage
  - Add frame index building during initial parsing (fix infinite loop issues)
  - Implement frame index queries for seeking operations
  - Add memory management and limits for frame index
  - _Requirements: 3.2_

- [ ] 5.2 Fix SEEKTABLE-based seeking
  - Correct `seekWithTable_unlocked()` implementation
  - Add SEEKTABLE entry validation and bounds checking
  - Implement interpolation between seek points for better accuracy
  - Add SEEKTABLE optimization and sorting
  - _Requirements: 3.1_

- [ ] 5.3 Implement sample position tracking
  - Fix `updatePositionTracking_unlocked()` for accurate sample counting
  - Add sample position validation against STREAMINFO total samples
  - Implement position tracking for variable block size streams
  - Create position recovery mechanisms after seeking
  - _Requirements: 6.2, 3.5_

- [ ] 5.4 Add seeking strategy coordination
  - Create `SeekingCoordinator` to select optimal seeking method
  - Implement fallback strategies when primary seeking fails
  - Add seeking performance monitoring and optimization
  - Create seeking validation and error recovery
  - _Requirements: 3.3, 3.4_

- [ ] 6. Implement Comprehensive Error Recovery
  - Add robust error detection and recovery throughout the demuxer
  - Implement fallback strategies for all major failure modes
  - Create detailed error logging with RFC 9639 section references
  - Add error statistics and monitoring capabilities
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5_

- [ ] 6.1 Create error recovery framework
  - Implement `ErrorRecoveryManager` class for centralized error handling
  - Add error classification system (IO, format, RFC violation, resource, logic)
  - Create recovery strategy selection based on error type and severity
  - Implement error recovery attempt limits and fallback mechanisms
  - _Requirements: 4.1, 4.2, 4.3_

- [ ] 6.2 Add sync loss recovery
  - Implement `recoverFromLostSync_unlocked()` with configurable search limits
  - Add sync pattern search with progressive window expansion
  - Create sync recovery validation and position correction
  - Implement sync loss statistics and monitoring
  - _Requirements: 4.2_

- [ ] 6.3 Implement corruption detection and recovery
  - Add data corruption detection using CRC validation and format checks
  - Create corrupted frame skipping with next frame search
  - Implement metadata corruption recovery with block skipping
  - Add corruption statistics and reporting
  - _Requirements: 4.1, 4.4_

- [ ] 6.4 Add comprehensive error logging
  - Create detailed error messages with file positions and RFC section references
  - Implement error statistics collection and reporting
  - Add configurable logging levels (debug, info, warning, error, critical)
  - Create error context information for debugging
  - _Requirements: 9.1, 9.2, 9.3, 9.4_

- [ ] 7. Implement RFC 9639 Streamable Subset Validation
  - Add streamable subset constraint validation per RFC 9639 Section 7
  - Implement frame header independence validation
  - Add block size and sample rate constraints for streamable subset
  - Create configurable streamable subset enforcement modes
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5_

- [ ] 7.1 Add streamable subset frame validation
  - Implement frame header independence validation (no STREAMINFO references)
  - Add block size constraint validation (max 16384, max 4608 for ≤48kHz)
  - Validate sample rate encoding in frame headers
  - Add bit depth encoding validation in frame headers
  - _Requirements: 8.1, 8.2, 8.3, 8.4_

- [ ] 7.2 Create streamable subset configuration
  - Add streamable subset enforcement configuration options
  - Implement streamable subset violation reporting
  - Create streamable subset statistics and monitoring
  - Add streamable subset validation during parsing and seeking
  - _Requirements: 8.5_

- [ ] 8. Add Performance Optimization and Memory Management
  - Optimize frame parsing and seeking performance
  - Implement memory limits and efficient buffer management
  - Add performance monitoring and statistics
  - Create memory usage optimization strategies
  - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.5_

- [ ] 8.1 Optimize frame processing performance
  - Add frame parsing performance monitoring
  - Implement efficient buffer reuse and memory management
  - Optimize sync pattern search algorithms
  - Add frame processing caching where appropriate
  - _Requirements: 10.2_

- [ ] 8.2 Implement memory management
  - Add memory usage tracking and limits throughout the demuxer
  - Implement efficient metadata storage with memory limits
  - Create buffer management for frame processing
  - Add memory usage optimization and cleanup
  - _Requirements: 10.3_

- [ ] 8.3 Add thread safety validation
  - Validate all public methods use proper locking (public/private pattern)
  - Add thread safety testing for concurrent access
  - Implement proper lock ordering to prevent deadlocks
  - Create thread safety documentation and validation
  - _Requirements: 10.1_

- [ ] 9. Create Comprehensive Testing Suite
  - Implement unit tests for all major demuxer components
  - Add integration tests with real FLAC files
  - Create RFC 9639 compliance test suite
  - Add performance and stress testing
  - _Requirements: All requirements validation_

- [ ]* 9.1 Create unit tests for frame processing
  - Test sync pattern detection with various bit patterns and corrupted data
  - Test frame header parsing with all valid and invalid field combinations
  - Test frame boundary detection with edge cases and corrupted frames
  - Test CRC validation with known test vectors
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 7.1, 7.2_

- [ ]* 9.2 Create metadata parsing tests
  - Test STREAMINFO parsing with various bit-field combinations
  - Test metadata block parsing with corrupted and oversized blocks
  - Test SEEKTABLE and VORBIS_COMMENT parsing with edge cases
  - Test metadata error recovery scenarios
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5_

- [ ]* 9.3 Create seeking and positioning tests
  - Test frame indexing with various file sizes and block sizes
  - Test SEEKTABLE-based seeking accuracy and edge cases
  - Test sample position tracking with variable block sizes
  - Test seeking error recovery and fallback strategies
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5_

- [ ]* 9.4 Create RFC 9639 compliance tests
  - Test with official FLAC specification examples
  - Test streamable subset validation with compliant and non-compliant files
  - Test format validation with various FLAC encoders
  - Test error recovery with intentionally corrupted files
  - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 8.1, 8.2, 8.3, 8.4_

- [ ]* 9.5 Create performance and stress tests
  - Benchmark parsing performance against baseline implementation
  - Test memory usage with large files and extensive metadata
  - Test thread safety with concurrent access patterns
  - Test error recovery performance and resource usage
  - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.5_

- [ ] 10. Integration and Documentation
  - Integrate fixed demuxer with existing codec architecture
  - Update API documentation and usage examples
  - Create RFC 9639 compliance documentation
  - Add configuration and debugging guides
  - _Requirements: All requirements integration_

- [ ] 10.1 Update demuxer integration
  - Ensure fixed demuxer works correctly with existing FLACCodec
  - Update MediaFactory integration for conditional compilation
  - Test demuxer/codec integration with various FLAC files
  - Validate API compatibility with existing code
  - _Requirements: 10.4_

- [ ] 10.2 Create comprehensive documentation
  - Document all new RFC 9639 compliance features and configuration options
  - Create troubleshooting guide for common FLAC parsing issues
  - Document error recovery strategies and configuration
  - Add performance tuning and optimization guide
  - _Requirements: 9.5_

- [ ] 10.3 Add configuration examples
  - Create configuration examples for different use cases (strict compliance, performance, compatibility)
  - Document all validation and error recovery options
  - Add examples for debugging and monitoring FLAC parsing
  - Create integration examples with the existing codebase
  - _Requirements: 8.5, 9.4, 10.5_