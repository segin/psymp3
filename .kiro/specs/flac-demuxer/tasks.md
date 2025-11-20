# **FLAC DEMUXER IMPLEMENTATION PLAN - REVISED**

## **Implementation Tasks Based on RFC 9639 Compliance and Real-World Lessons**

**Key Implementation Priorities:**
1. **RFC 9639 compliance** with strict validation of forbidden patterns and reserved fields
2. **Accurate frame size estimation** using STREAMINFO minimum frame size
3. **Efficient frame boundary detection** with limited search scope
4. **Thread-safe public/private method patterns** to prevent deadlocks
5. **Comprehensive debug logging** with method identification tokens
6. **Robust error recovery** for corrupted or incomplete streams

- [x] 0. Implement RFC 9639 Compliance Foundation
  - Create forbidden pattern detection system per RFC 9639 Table 1
  - Implement big-endian integer parsing utilities (except Vorbis comments)
  - Add CRC-8 and CRC-16 validation functions per RFC 9639 Sections 9.1.8, 9.3
  - Create UTF-8-like coded number parser per RFC 9639 Section 9.1.5
  - _Requirements: 1.1, 1.2, 2.1, 2.2, 15.1, 15.2, 16.1_

Co  - Validate exact byte sequence 0x66 0x4C 0x61 0x43 (fLaC)
  - Reject files with invalid stream markers immediately
  - Log exact byte values found versus expected for debugging
  - _Requirements: 1.1, 1.2, 1.3, 1.4_

- [x] 0.2 Implement Metadata Block Header Parser (RFC 9639 Section 8.1)
  - Parse 4-byte header: is_last flag (bit 7), block type (bits 0-6), length (24-bit big-endian)
  - Detect and reject forbidden block type 127
  - Handle reserved block types 7-126 by skipping gracefully
  - Validate block length against file size
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 15.1_

- [x] 0.3 Implement STREAMINFO Parser (RFC 9639 Section 8.2)
  - Parse all 34 bytes with correct bit field extraction
  - Extract u(16) min/max block size, u(24) min/max frame size
  - Extract u(20) sample rate, u(3) channels-1, u(5) bit depth-1
  - Extract u(36) total samples, u(128) MD5 checksum
  - Validate min/max block size >= 16 (forbidden pattern)
  - Validate min block size <= max block size
  - Validate sample rate != 0 for audio streams
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7, 3.8, 3.9, 3.10, 3.11, 3.12, 3.13, 3.14, 3.15, 3.16, 3.17, 3.18, 3.19, 3.20_

- [x] 0.4 Implement Frame Sync Code Detector (RFC 9639 Section 9.1)
  - Search for 15-bit sync pattern 0b111111111111100
  - Verify byte alignment of sync code
  - Extract and validate blocking strategy bit (must not change mid-stream)
  - Distinguish fixed block size (0xFFF8) from variable (0xFFF9)
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 4.7, 4.8_

- [x] 0.5 Implement Block Size Bits Parser (RFC 9639 Section 9.1.1, Table 14)
  - Parse 4-bit block size encoding
  - Implement all 16 lookup table values (0b0000-0b1111)
  - Handle uncommon block sizes (8-bit and 16-bit)
  - Reject reserved pattern 0b0000
  - Reject forbidden uncommon block size 65536
  - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7, 5.8, 5.9, 5.10, 5.11, 5.12, 5.13, 5.14, 5.15, 5.16, 5.17, 5.18_

- [x] 0.6 Implement Sample Rate Bits Parser (RFC 9639 Section 9.1.2)
  - Parse 4-bit sample rate encoding
  - Implement all 16 lookup table values
  - Handle uncommon sample rates (kHz, Hz, tens of Hz)
  - Reject forbidden pattern 0b1111
  - Support STREAMINFO reference (0b0000) for non-streamable files
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 6.7, 6.8, 6.9, 6.10, 6.11, 6.12, 6.13, 6.14, 6.15, 6.16, 6.17_

- [x] 0.7 Implement Channel Assignment Parser (RFC 9639 Section 9.1.3)
  - Parse 4-bit channel encoding
  - Support independent channels (0b0000-0b0111)
  - Support left-side stereo (0b1000)
  - Support right-side stereo (0b1001)
  - Support mid-side stereo (0b1010)
  - Reject reserved patterns (0b1011-0b1111)
  - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7_

- [x] 0.8 Implement Bit Depth Parser (RFC 9639 Section 9.1.4)
  - Parse 3-bit bit depth encoding
  - Implement all 8 lookup table values
  - Support STREAMINFO reference (0b000) for non-streamable files
  - Reject reserved pattern 0b011
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 8.8, 8.9_

- [x] 0.9 Implement Coded Number Parser (RFC 9639 Section 9.1.5)
  - Parse UTF-8-like variable-length encoding (1-7 bytes)
  - Support all byte length patterns (0b0xxxxxxx through 0b11111110)
  - Interpret as frame number for fixed block size streams
  - Interpret as sample number for variable block size streams
  - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 9.6, 9.7, 9.8, 9.9, 9.10_

- [x] 0.10 Implement CRC Validation (RFC 9639 Sections 9.1.8, 9.3)
  - Implement CRC-8 with polynomial 0x07 for frame headers
  - Implement CRC-16 with polynomial 0x8005 for frame footers
  - Validate frame header CRC-8 after parsing
  - Validate frame footer CRC-16 after reading complete frame
  - Log CRC mismatches with frame position
  - Support strict mode that rejects on CRC failure
  - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.5, 10.6, 11.1, 11.2, 11.3, 11.4, 11.5, 11.6, 11.7, 11.8_

- [x] 0.11 Implement SEEKTABLE Parser (RFC 9639 Section 8.5)
  - Calculate seek point count as block_length / 18
  - Parse u(64) sample number, u(64) byte offset, u(16) frame samples
  - Detect placeholder seek points (0xFFFFFFFFFFFFFFFF)
  - Validate seek points are sorted in ascending order
  - Validate seek points are unique (except placeholders)
  - _Requirements: 12.1, 12.2, 12.3, 12.4, 12.5, 12.6, 12.7, 12.8_

- [x] 0.12 Implement VORBIS_COMMENT Parser (RFC 9639 Section 8.6)
  - Parse u(32) little-endian vendor string length
  - Parse UTF-8 vendor string
  - Parse u(32) little-endian field count
  - Parse each field with u(32) little-endian length
  - Split fields on first equals sign into name=value
  - Validate field names (printable ASCII 0x20-0x7E except 0x3D)
  - Implement case-insensitive field name comparison
  - _Requirements: 13.1, 13.2, 13.3, 13.4, 13.5, 13.6, 13.7, 13.8_

- [x] 0.13 Implement PICTURE Parser (RFC 9639 Section 8.8)
  - Parse u(32) picture type
  - Parse u(32) media type length and ASCII string
  - Parse u(32) description length and UTF-8 string
  - Parse u(32) width, height, color depth, indexed colors
  - Parse u(32) picture data length and binary data
  - Handle URI format (media type "-->")
  - _Requirements: 14.1, 14.2, 14.3, 14.4, 14.5, 14.6, 14.7, 14.8, 14.9, 14.10, 14.11, 14.12_

- [x] 0.14 Implement Forbidden Pattern Detection (RFC 9639 Section 5, Table 1)
  - Reject metadata block type 127
  - Reject STREAMINFO min/max block size < 16
  - Reject sample rate bits 0b1111
  - Reject uncommon block size 65536
  - Log all forbidden pattern detections with context
  - _Requirements: 15.1, 15.2, 15.3, 15.4, 15.5_

- [x] 0.15 Implement Big-Endian Parsing (RFC 9639 Section 5)
  - Create big-endian u(16), u(24), u(32), u(64) parsers
  - Use big-endian for all metadata block fields
  - Use big-endian for all STREAMINFO fields
  - Use big-endian for all frame header fields
  - Use little-endian ONLY for VORBIS_COMMENT lengths
  - _Requirements: 16.1, 16.2, 16.3, 16.4, 16.5_

- [x] 0.16 Implement Streamable Subset Detection (RFC 9639 Section 7)
  - Mark stream as non-streamable if sample rate bits = 0b0000
  - Mark stream as non-streamable if bit depth bits = 0b000
  - Mark stream as non-streamable if max block size > 16384
  - Mark stream as non-streamable if sample rate <= 48kHz and block size > 4608
  - Mark stream as non-streamable if WAVEFORMATEXTENSIBLE_CHANNEL_MASK present
  - _Requirements: 17.1, 17.2, 17.3, 17.4, 17.5_

- [x] 1. Create FLACDemuxer Class Structure
  - Implement FLACDemuxer class inheriting from Demuxer base class
  - Add private member variables for FLAC container state and metadata
  - Implement constructor accepting `unique_ptr<IOHandler>`
  - Add destructor with proper resource cleanup
  - _Requirements: 8.1, 8.2, 8.7_

- [x] 1.1 Define FLAC Data Structures
  - Create FLACMetadataBlock structure for metadata block information
  - Define FLACFrame structure for frame positioning and size data
  - Add enums for FLAC metadata block types
  - Implement helper structures for seek table and stream info
  - _Requirements: 1.1, 1.2, 1.3, 2.1_

- [x] 1.2 Implement Basic Demuxer Interface
  - Implement all pure virtual methods from Demuxer base class
  - Add placeholder implementations that return appropriate defaults
  - Ensure proper error handling and return codes
  - Add basic logging for debugging and development
  - _Requirements: 8.1, 8.2, 8.3, 8.4_

- [x] 2. Implement FLAC Container Parsing
  - Create parseContainer() method to parse FLAC file structure
  - Implement fLaC stream marker validation
  - Add metadata block parsing loop with proper error handling
  - Locate start of audio data after metadata blocks
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8_

- [x] 2.1 Implement Metadata Block Parsing
  - Create parseMetadataBlocks() method to process all metadata
  - Add parseMetadataBlockHeader() for block header parsing
  - Implement proper handling of is_last flag and block sizes
  - Add validation for block structure and reasonable size limits
  - _Requirements: 1.2, 1.3, 1.4, 2.4, 2.7, 2.8_

- [x] 2.2 Add STREAMINFO Block Processing
  - Implement parseStreamInfo() method for STREAMINFO block
  - Extract minimum/maximum block size, frame size, sample rate, channels, bit depth
  - Parse total samples field for duration calculation
  - Store MD5 signature for potential integrity verification
  - _Requirements: 2.1, 5.1, 5.2, 5.3, 5.4_

- [x] 3. Implement Metadata Block Processors
  - Create parseSeekTable() method for SEEKTABLE block processing
  - Implement parseVorbisComment() method for metadata extraction
  - Add parsePicture() method for embedded artwork handling
  - Create generic metadata block skipper for unknown types
  - _Requirements: 2.2, 2.3, 2.5, 2.6, 2.7, 2.8_

- [x] 3.1 Implement SEEKTABLE Processing
  - Parse seek points with sample number, byte offset, and frame samples
  - Handle placeholder seek points (0xFFFFFFFFFFFFFFFF) appropriately
  - Build internal seek table data structure for efficient lookup
  - Validate seek points for consistency and reasonable values
  - _Requirements: 2.3, 4.1, 4.2, 4.6, 4.8_

- [x] 3.2 Implement VORBIS_COMMENT Processing
  - Parse vendor string and user comment count
  - Extract individual comments in FIELD=value format
  - Handle UTF-8 encoding properly for international metadata
  - Store standard fields (ARTIST, TITLE, ALBUM) in StreamInfo
  - _Requirements: 2.2, 8.5, 8.6_

- [x] 3.3 Add PICTURE Block Support
  - Parse picture type, MIME type, description, and image data
  - Extract embedded artwork for potential display
  - Handle multiple picture blocks appropriately
  - Store picture metadata without loading large image data unnecessarily
  - _Requirements: 2.6, 7.1, 7.2, 7.7_

- [x] 4. Implement FLAC Frame Detection and Parsing
  - Create findNextFrame() method to locate FLAC frame sync codes
  - Implement parseFrameHeader() to extract frame parameters
  - Add validateFrameHeader() for frame header consistency checking
  - Create calculateFrameSize() for frame size estimation
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7, 3.8_

- [x] 4.1 Implement Efficient Frame Boundary Detection (CRITICAL)
  - **Priority 1**: Limit frame boundary search scope to 512 bytes maximum to prevent excessive I/O
  - **Priority 2**: Use 16-byte search increments instead of 2-byte for faster detection
  - **Priority 3**: Implement conservative fallback using STREAMINFO minimum frame size when detection fails
  - **Priority 4**: Add debug logging showing search scope and results
  - **Priority 5**: Optimize for highly compressed streams where frames may be very small
  - _Requirements: 3.1, 3.4, 7.1, 7.2, 7.3, 11.4_

- [x] 4.2 Add Frame Header Parsing
  - Parse blocking strategy, block size, sample rate, channel assignment
  - Extract sample size, frame/sample number, and CRC
  - Handle variable block sizes and sample rates within stream
  - Validate frame parameters against STREAMINFO for consistency
  - _Requirements: 3.2, 3.4, 3.5, 3.8_

- [x] 4.3 Implement Accurate Frame Size Calculation (CRITICAL)
  - **Priority 1**: Use STREAMINFO minimum frame size as primary estimate
  - **Priority 2**: For fixed block size streams, return minimum frame size directly without scaling
  - **Priority 3**: Avoid complex theoretical calculations that produce inaccurate estimates
  - **Priority 4**: Add debug logging with `[calculateFrameSize]` token for troubleshooting
  - **Priority 5**: Handle highly compressed streams with frames as small as 14 bytes
  - _Requirements: 3.1, 3.2, 7.1, 7.4, 11.1, 11.3_

- [x] 5. Implement Data Streaming Component
  - Create readChunk() methods for sequential frame reading
  - Implement readFrameData() to read complete FLAC frames
  - Add proper MediaChunk creation with frame data and timing
  - Handle EOF detection and stream completion
  - _Requirements: 3.3, 3.4, 3.7, 3.8, 8.3, 8.4_

- [x] 5.1 Implement Frame Data Reading
  - Read complete FLAC frames including headers and CRC
  - Create MediaChunk objects with proper stream_id and timing
  - Set timestamp_samples based on frame sample position
  - Handle frame reading errors and corrupted data gracefully
  - _Requirements: 3.3, 3.4, 3.8, 6.4, 6.6_

- [x] 5.2 Add Position Tracking
  - Maintain current sample position during sequential reading
  - Update position based on frame block sizes
  - Provide accurate position reporting through getPosition()
  - Handle position tracking across seeks and resets
  - _Requirements: 5.5, 5.6, 5.7, 5.8_

- [x] 6. Implement Seeking Operations
  - Create seekTo() method for timestamp-based seekingz
  - Implement seekWithTable() for SEEKTABLE-based seeking
  - Add seekBinary() for binary search seeking without seek table
  - Create seekLinear() as fallback for sample-accurate positioning
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 4.7, 4.8_

- [x] 6.1 Implement SEEKTABLE-Based Seeking
  - Find closest seek point before target sample position
  - Seek to file position specified in seek table
  - Parse frames forward from seek point to exact target
  - Handle seek table errors and missing seek points
  - _Requirements: 4.1, 4.6, 4.8, 6.3_

- [x] 6.2 Acknowledge Binary Search Limitations (ARCHITECTURAL)
  - **Reality Check**: Binary search is fundamentally incompatible with compressed audio streams
  - **Problem**: Cannot predict frame positions in variable-length compressed data
  - **Current Approach**: Implement binary search but expect failures with compressed streams
  - **Fallback Strategy**: Return to beginning position when binary search fails
  - **Future Solution**: Implement frame indexing during initial parsing for accurate seeking
  - _Requirements: 4.2, 4.5, 4.7, 4.8_

- [x] 6.3 Implement Linear Seeking
  - Parse frames sequentially from current or seek table position
  - Provide sample-accurate positioning for precise seeking
  - Use as fallback when other seeking methods fail
  - Optimize for short-distance seeks from current position
  - _Requirements: 4.3, 4.4, 4.6, 4.7_

- [x] 7. Add Duration and Position Calculation
  - Implement getDuration() using total samples from STREAMINFO
  - Create getPosition() for current playback position reporting
  - Add sample-to-millisecond conversion utilities
  - Handle edge cases with missing or invalid duration information
  - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7, 5.8_

- [x] 7.1 Implement Duration Calculation
  - Use total samples and sample rate from STREAMINFO for primary duration
  - Fall back to file size estimation when STREAMINFO is incomplete
  - Handle very long files without integer overflow
  - Provide duration in milliseconds for consistency with other demuxers
  - _Requirements: 5.1, 5.2, 5.8, 8.4_

- [x] 7.2 Add Position Tracking and Reporting
  - Maintain accurate current sample position during playback
  - Convert sample positions to milliseconds for position reporting
  - Handle position updates during seeking operations
  - Provide both sample-based and time-based position information
  - _Requirements: 5.3, 5.4, 5.5, 5.6, 5.7_

- [x] 8. Implement Error Handling and Robustness
  - Add comprehensive error checking for all parsing operations
  - Implement graceful handling of corrupted metadata and frames
  - Create recovery mechanisms for lost frame sync
  - Add proper resource cleanup in all error paths
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 6.7, 6.8_

- [x] 8.1 Add Container-Level Error Handling
  - Validate fLaC stream marker and reject non-FLAC files
  - Handle corrupted metadata blocks by skipping and continuing
  - Provide reasonable defaults when STREAMINFO is missing or invalid
  - Log errors appropriately without stopping playback when possible
  - _Requirements: 6.1, 6.2, 6.3, 6.8_

- [x] 8.2 Implement Frame-Level Error Recovery
  - Handle lost frame sync by searching for next valid frame
  - Skip corrupted frames and continue with subsequent frames
  - Log CRC mismatches but attempt to use frame data
  - Provide silence output for completely unrecoverable frames
  - _Requirements: 6.4, 6.5, 6.6, 6.7_

- [x] 9. Optimize Performance and Memory Usage
  - Implement efficient metadata parsing with minimal memory usage
  - Add bounded buffering to prevent memory exhaustion
  - Optimize I/O operations for both local files and network streams
  - Create efficient seek table lookup and frame sync detection
  - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7, 7.8_

- [x] 9.1 Implement Memory Management
  - Use streaming approach for large file processing
  - Implement bounded buffers for metadata and frame data
  - Optimize seek table storage for memory efficiency
  - Add proper cleanup of all allocated resources
  - _Requirements: 7.1, 7.2, 7.6, 7.8_

- [x] 9.2 Add Performance Optimizations
  - Optimize frame sync detection with efficient bit manipulation
  - Use binary search for large seek table lookups
  - Minimize I/O operations during seeking
  - Add read-ahead buffering for network streams
  - _Requirements: 7.3, 7.4, 7.5, 7.7_

- [x] 10. Integrate with Demuxer Architecture
  - Ensure proper integration with IOHandler subsystem
  - Implement StreamInfo population with accurate FLAC parameters
  - Add MediaFactory registration for .flac files
  - Create proper error reporting using PsyMP3 conventions
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 8.8_

- [x] 10.1 Complete IOHandler Integration
  - Use IOHandler interface exclusively for all file I/O operations
  - Support both FileIOHandler and HTTPIOHandler implementations
  - Handle large file operations with proper 64-bit offset support
  - Propagate I/O errors appropriately through demuxer interface
  - _Requirements: 8.5, 8.6, 7.1, 7.7_

- [x] 10.2 Add MediaFactory Integration
  - Register FLACDemuxer for .flac file extension detection
  - Add audio/flac MIME type support
  - Implement format detection based on fLaC stream marker
  - Integrate with content analysis and format probing system
  - _Requirements: 8.1, 8.2, 8.7_

- [x] 11. Ensure Compatibility with Existing Implementation
  - Test with all FLAC files that work with current implementation
  - Maintain equivalent metadata extraction capabilities
  - Provide comparable or better seeking accuracy and performance
  - Ensure seamless integration with DemuxedStream bridge
  - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 9.6, 9.7, 9.8_

- [x] 11.1 Implement Compatibility Testing
  - Create test suite with various FLAC file types and encoders
  - Test metadata extraction against current implementation results
  - Verify seeking accuracy and performance benchmarks
  - Ensure all previously working files continue to function
  - _Requirements: 9.1, 9.3, 9.4, 9.8_

- [x] 11.2 Add Performance Validation
  - Benchmark parsing and seeking performance against current code
  - Measure memory usage and ensure reasonable resource consumption
  - Test with large files and various compression levels
  - Validate thread safety and concurrent access patterns
  - _Requirements: 9.5, 9.6, 9.7, 10.1, 10.2, 10.3, 10.4_

- [x] 12. Add Thread Safety and Concurrency Support
  - Implement proper synchronization for shared state access
  - Add thread-safe metadata and position tracking
  - Handle concurrent seeking and reading operations safely
  - Ensure proper cleanup during multi-threaded destruction
  - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.5, 10.6, 10.7, 10.8_

- [x] 12.1 Implement Public/Private Lock Pattern (CRITICAL)
  - **Priority 1**: Create public methods that acquire locks and call private `_unlocked` implementations
  - **Priority 2**: Ensure all internal method calls use `_unlocked` versions to prevent deadlocks
  - **Priority 3**: Document lock acquisition order to prevent deadlock scenarios
  - **Priority 4**: Use RAII lock guards for exception safety
  - **Priority 5**: Never invoke callbacks while holding internal locks
  - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.8_

- [x] 12.2 Add Concurrent Access Support
  - Handle multiple threads accessing metadata safely
  - Implement thread-safe I/O operations through IOHandler
  - Add proper cleanup coordination during destruction
  - Test concurrent seeking and reading scenarios
  - _Requirements: 10.3, 10.4, 10.6, 10.8_

- [x] 13. Create Comprehensive Testing Suite
  - Implement unit tests for all major parsing and seeking functions
  - Add integration tests with various FLAC file types
  - Create performance benchmarks and regression tests
  - Test error handling and recovery scenarios
  - _Requirements: All requirements validation_

- [x] 13.1 Implement Unit Tests
  - Test metadata block parsing with various block types and sizes
  - Verify frame detection and header parsing accuracy
  - Test seeking algorithms with and without seek tables
  - Validate error handling and recovery mechanisms
  - _Requirements: 1.1-1.8, 2.1-2.8, 3.1-3.8, 4.1-4.8_

- [x] 13.2 Add Integration and Performance Tests
  - Test with FLAC files from various encoders and versions
  - Benchmark seeking performance and memory usage
  - Test integration with IOHandler implementations
  - Validate compatibility with existing FLAC playback
  - _Requirements: 5.1-5.8, 6.1-6.8, 7.1-7.8, 8.1-8.8, 9.1-9.8_

- [x] 13.3 Fix IOHandler Buffering Issue
  - ❌ CRITICAL: FileIOHandler jumps from position 4 to 131072 (128KB)
  - ✅ Identified that bit field extraction logic is correct
  - ✅ Problem is IOHandler reading from wrong file position
  - Need to investigate FileIOHandler buffering behavior
  - May need to disable buffering or fix buffer position tracking

- [x] 14. Documentation and Code Quality
  - Add comprehensive inline documentation for all public methods
  - Create developer documentation for FLAC container format handling
  - Document seeking algorithms and optimization opportunities
  - Ensure code follows PsyMP3 style guidelines and conventions
  - _Requirements: 8.7, 8.8, 9.7, 9.8_

- [x] 14.1 Create API Documentation
  - Document all FLACDemuxer public methods with usage examples
  - Explain FLAC container format and metadata block structure
  - Document seeking strategies and performance characteristics
  - Add troubleshooting guide for common issues
  - _Requirements: 8.7, 8.8, 10.1-10.8_

- [x] 14.2 Add Developer Guide
  - Document integration with FLACCodec and other components
  - Explain relationship between container parsing and bitstream decoding
  - Provide guidance for extending FLAC support
  - Document thread safety considerations and best practices
  - _Requirements: 9.7, 9.8, 10.1-10.8_

- [x] 15. Implement Debug Logging with Method Identification (NEW)
  - **Priority 1**: Add unique method tokens to all debug messages (e.g., `[calculateFrameSize]`)
  - **Priority 2**: Distinguish between similar messages from different methods
  - **Priority 3**: Log frame size estimation method selection and results
  - **Priority 4**: Log frame boundary detection scope and outcomes
  - **Priority 5**: Provide sufficient detail for performance troubleshooting
  - _Requirements: 11.1, 11.2, 11.3, 11.4, 11.8_

- [x] 16. Fix Multiple Frame Size Estimation Methods (NEW)
  - **Priority 1**: Identify all locations where frame size estimation occurs
  - **Priority 2**: Ensure consistent use of STREAMINFO-based estimation across all methods
  - **Priority 3**: Fix `skipCorruptedFrame()` method to use proper STREAMINFO minimum
  - **Priority 4**: Remove or fix complex theoretical calculations that produce wrong estimates
  - **Priority 5**: Add method-specific debug tokens to distinguish estimation sources
  - _Requirements: 3.1, 3.2, 7.1, 7.4, 11.1, 11.3_

- [x] 17. Optimize Frame Processing Performance (NEW)
  - **Priority 1**: Reduce I/O operations from hundreds to tens per frame
  - **Priority 2**: Complete frame processing in milliseconds rather than seconds
  - **Priority 3**: Use accurate frame size estimates to prevent buffer waste
  - **Priority 4**: Implement efficient frame boundary detection with limited search scope
  - **Priority 5**: Handle highly compressed streams (14-byte frames) efficiently
  - _Requirements: 7.1, 7.2, 7.3, 7.5, 7.8_

- [x] 18. Implement Future Seeking Architecture (COMPLETED)
  - **Priority 1**: ✅ Design frame indexing system during initial parsing
  - **Priority 2**: ✅ Cache discovered frame positions for accurate seeking
  - **Priority 3**: ✅ Implement sample-accurate seeking using frame index
  - **Priority 4**: ✅ Provide efficient seeking for files without SEEKTABLE
  - **Priority 5**: ✅ Replace binary search with index-based seeking
  - _Requirements: 4.1, 4.2, 4.3, 4.8_