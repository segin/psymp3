# Implementation Plan

## Overview

This implementation plan converts the FLAC RFC 9639 compliance design into a series of incremental coding tasks. Each task builds on previous work to systematically rebuild the FLAC demuxer with full RFC compliance, robust error recovery, and optimal performance while maintaining thread safety and API compatibility.

## Task List

- [x] 1. Implement Core Frame Boundary Detection
  - Create RFC 9639 compliant sync pattern validation
  - Implement robust frame size estimation and boundary detection
  - Add configurable search windows to prevent infinite loops
  - _Requirements: 2.1, 2.2, 2.3, 2.4_

- [x] 1.1 Create sync pattern validation methods
  - Implement `validateFrameSync_unlocked()` with 15-bit pattern (0b111111111111100)
  - Add `searchSyncPattern_unlocked()` with efficient byte-by-byte search
  - Create sync pattern validation with proper bit masking
  - _Requirements: 2.1_

- [x] 1.2 Implement frame boundary detection algorithm
  - Create `findNextFrame_unlocked()` to prevent infinite loops
  - Add conservative frame size estimation using STREAMINFO hints
  - Implement frame end detection using next sync pattern search
  - Add fallback strategies when frame boundaries cannot be determined
  - _Requirements: 2.2, 2.4_

- [x] 1.3 Add frame size calculation and validation
  - Implement `calculateFrameSize_unlocked()` using STREAMINFO constraints
  - Create frame size estimation based on audio format parameters
  - Add frame size validation against min/max constraints
  - Implement fallback size estimation for missing STREAMINFO
  - _Requirements: 2.3, 2.4_

- [x] 2. Fix STREAMINFO Metadata Parsing
  - Correct bit-field extraction errors in STREAMINFO parsing
  - Implement robust metadata block header parsing
  - Add STREAMINFO recovery mechanisms for corrupted blocks
  - _Requirements: 1.1, 1.2, 1.3, 1.4_

- [x] 2.1 Fix STREAMINFO bit-field extraction
  - Correct sample rate extraction (20 bits) in `parseStreamInfoBlock_unlocked()`
  - Fix channel count extraction (3 bits + 1) with proper bit masking
  - Fix bits per sample extraction (5 bits + 1) from packed fields
  - Fix total samples extraction (36 bits) across multiple bytes
  - Validate extracted values against RFC 9639 constraints
  - _Requirements: 1.3_

- [x] 2.2 Implement metadata block header parsing
  - Create `parseMetadataBlockHeader_unlocked()` for all block types
  - Add validation of block length fields and last-block flags
  - Implement metadata block size limits to prevent memory exhaustion
  - Add error recovery for corrupted block headers
  - _Requirements: 1.1, 1.2_

- [x] 2.3 Add STREAMINFO recovery mechanisms
  - Implement `attemptStreamInfoRecovery_unlocked()` to derive from first frame
  - Add validation of recovered STREAMINFO parameters
  - Create fallback STREAMINFO values for missing blocks
  - Implement STREAMINFO consistency checking across frames
  - _Requirements: 1.4_

- [x] 3. Implement RFC 9639 Frame Header Parsing
  - Fix frame header parsing for all RFC 9639 field combinations
  - Add UTF-8 decoding for frame/sample numbers
  - Implement complete frame header validation including reserved bits
  - Add support for variable vs. fixed block size strategies
  - _Requirements: 2.2, 6.2, 6.4, 7.4_

- [x] 3.1 Fix frame header field parsing
  - Correct blocking strategy parsing in `parseFrameHeader_unlocked()`
  - Fix block size parsing for all encoding modes (direct, 8-bit, 16-bit)
  - Fix sample rate parsing for all encoding modes including custom rates
  - Fix channel assignment parsing and validation
  - Fix bit depth parsing for all valid bit depths
  - _Requirements: 2.2_

- [x] 3.2 Implement UTF-8 frame number decoding
  - Create `decodeUTF8Number_unlocked()` for frame/sample number parsing
  - Add UTF-8 sequence validation and error handling
  - Support 1-7 byte UTF-8 sequences per FLAC specification
  - Add bounds checking for decoded frame numbers
  - _Requirements: 2.2_

- [x] 3.3 Add comprehensive frame header validation
  - Implement `validateFrameHeader_unlocked()` with complete checks
  - Add reserved bit validation (must be zero)
  - Validate frame header consistency with STREAMINFO
  - Add frame header CRC-8 validation per RFC 9639 Section 9.1.8
  - _Requirements: 7.1, 7.4_

- [x] 4. Implement RFC 9639 CRC Validation
  - Add CRC-8 validation for frame headers
  - Implement CRC-16 validation for frame footers
  - Create configurable CRC validation modes
  - Add CRC calculation methods with correct polynomials
  - _Requirements: 7.1, 7.2, 7.5_

- [x] 4.1 Implement CRC-8 header validation
  - Create `calculateHeaderCRC8_unlocked()` with polynomial 0x07
  - Add CRC-8 validation in frame header parsing
  - Implement CRC-8 error logging and recovery options
  - Add performance optimization for CRC calculation
  - _Requirements: 7.1_

- [x] 4.2 Implement CRC-16 frame validation
  - Create `calculateFrameCRC16_unlocked()` with polynomial 0x8005
  - Add CRC-16 validation for complete frames
  - Implement CRC-16 error handling and recovery strategies
  - Add frame CRC validation during frame reading
  - _Requirements: 7.2_

- [x] 4.3 Add configurable CRC validation modes
  - Create CRC validation configuration (disabled/enabled/strict)
  - Implement CRC error counting and threshold-based disabling
  - Add CRC validation statistics and reporting
  - Create CRC error recovery strategies based on configuration
  - _Requirements: 7.5_

- [x] 5. Implement Seeking and Position Tracking
  - Create frame indexing system for efficient seeking
  - Fix SEEKTABLE-based seeking implementation
  - Implement accurate sample position tracking
  - Add seeking strategy coordination with fallback mechanisms
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 6.2, 6.4_

- [x] 5.1 Create frame indexing system
  - Implement `FLACFrameIndex` class with efficient storage
  - Add frame index building during parsing
  - Implement frame index queries for seeking operations
  - Add memory management and limits for frame index
  - _Requirements: 3.2_

- [x] 5.2 Fix SEEKTABLE-based seeking
  - Implement `seekWithTable_unlocked()` with proper validation
  - Add SEEKTABLE entry validation and bounds checking
  - Implement interpolation between seek points for accuracy
  - Add SEEKTABLE optimization and sorting
  - _Requirements: 3.1, 5.2_

- [x] 5.3 Implement sample position tracking
  - Create `updatePositionTracking_unlocked()` for accurate counting
  - Add sample position validation against STREAMINFO total samples
  - Implement position tracking for variable block size streams
  - Create position recovery mechanisms after seeking
  - _Requirements: 3.5, 6.2, 6.4_

- [x] 6. Implement Error Recovery Framework
  - Create centralized error recovery management
  - Add sync loss recovery mechanisms
  - Implement corruption detection and recovery
  - Add comprehensive error logging with RFC references
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5, 9.1, 9.2, 9.3, 9.4_

- [x] 6.1 Create error recovery manager
  - Implement `ErrorRecoveryManager` class for centralized handling
  - Add error classification system (IO, format, RFC violation, resource, logic)
  - Create recovery strategy selection based on error type and severity
  - Implement error recovery attempt limits and fallback mechanisms
  - _Requirements: 4.1, 4.2, 4.3_

- [x] 6.2 Add sync loss recovery
  - Implement `recoverFromLostSync_unlocked()` with configurable limits
  - Add sync pattern search with progressive window expansion
  - Create sync recovery validation and position correction
  - Implement sync loss statistics and monitoring
  - _Requirements: 4.2_

- [x] 6.3 Implement corruption detection and recovery
  - Add corruption detection using CRC validation and format checks
  - Create corrupted frame skipping with next frame search
  - Implement metadata corruption recovery with block skipping
  - Add corruption statistics and reporting
  - _Requirements: 4.1, 4.4_

- [x] 7. Implement Streamable Subset Validation
  - Add streamable subset constraint validation per RFC 9639 Section 7
  - Implement frame header independence validation
  - Add block size and sample rate constraints for streamable subset
  - Create configurable streamable subset enforcement
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5_

- [x] 7.1 Add streamable subset frame validation
  - Implement frame header independence validation (no STREAMINFO references)
  - Add block size constraint validation (max 16384, max 4608 for ≤48kHz)
  - Validate sample rate encoding in frame headers
  - Add bit depth encoding validation in frame headers
  - _Requirements: 8.1, 8.2, 8.3, 8.4_

- [x] 7.2 Create streamable subset configuration
  - Add streamable subset enforcement configuration options
  - Implement streamable subset violation reporting
  - Create streamable subset statistics and monitoring
  - Add streamable subset validation during parsing and seeking
  - _Requirements: 8.5_

- [x] 8. Add Performance Optimization and Thread Safety
  - Optimize frame parsing and seeking performance
  - Implement memory limits and efficient buffer management
  - Validate thread safety using public/private lock pattern
  - Add performance monitoring and statistics
  - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.5_

- [x] 8.1 Optimize frame processing performance
  - Add frame parsing performance monitoring
  - Implement efficient buffer reuse and memory management
  - Optimize sync pattern search algorithms
  - Add frame processing caching where appropriate
  - _Requirements: 10.2_
  
- [x] 8.2 Implement memory management
  - Add memory usage tracking and limits throughout demuxer
  - Implement efficient metadata storage with memory limits
  - Create buffer management for frame processing
  - Add memory usage optimization and cleanup
  - _Requirements: 10.3_

- [x] 8.3 Validate thread safety implementation
  - Ensure all public methods use proper locking (public/private pattern)
  - Implement proper lock ordering to prevent deadlocks
  - Add thread safety validation and testing
  - Create thread safety documentation
  - _Requirements: 10.1_

- [x] 9. Integration and API Compatibility
  - Integrate enhanced demuxer with existing codec architecture
  - Ensure API compatibility with existing code
  - Add configuration interfaces for new features
  - Create comprehensive documentation
  - _Requirements: 10.4, 10.5_

- [x] 9.1 Update demuxer integration
  - Ensure enhanced demuxer works with existing FLACCodec
  - Update MediaFactory integration for conditional compilation
  - Test demuxer/codec integration with various FLAC files
  - Validate API compatibility with existing code
  - _Requirements: 10.4_

- [x] 9.2 Add configuration interfaces
  - Create public methods for RFC 9639 compliance configuration
  - Add streamable subset mode configuration
  - Implement CRC validation mode configuration
  - Add error recovery configuration options
  - _Requirements: 7.5, 8.5, 10.5_

- [x] 9.3 Create comprehensive documentation
  - Document all new RFC 9639 compliance features
  - Create troubleshooting guide for FLAC parsing issues
  - Document error recovery strategies and configuration
  - Add performance tuning and optimization guide
  - _Requirements: 9.1, 9.2, 9.3, 9.4_