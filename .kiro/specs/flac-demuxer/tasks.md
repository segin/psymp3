# **FLAC DEMUXER IMPLEMENTATION PLAN**

## **Implementation Tasks**

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

- [ ] 4. Implement FLAC Frame Detection and Parsing
  - Create findNextFrame() method to locate FLAC frame sync codes
  - Implement parseFrameHeader() to extract frame parameters
  - Add validateFrameHeader() for frame header consistency checking
  - Create calculateFrameSize() for frame size estimation
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7, 3.8_

- [ ] 4.1 Implement Frame Sync Detection
  - Search for FLAC sync codes (0xFFF8 to 0xFFFF) in bitstream
  - Validate frame header structure after sync code detection
  - Handle false sync codes and continue searching
  - Optimize sync detection for performance with large files
  - _Requirements: 3.1, 3.6, 6.4, 7.3_

- [ ] 4.2 Add Frame Header Parsing
  - Parse blocking strategy, block size, sample rate, channel assignment
  - Extract sample size, frame/sample number, and CRC
  - Handle variable block sizes and sample rates within stream
  - Validate frame parameters against STREAMINFO for consistency
  - _Requirements: 3.2, 3.4, 3.5, 3.8_

- [ ] 4.3 Implement Frame Size Calculation
  - Estimate frame size based on block size, channels, and bit depth
  - Handle variable frame sizes due to compression efficiency
  - Use frame size estimates for seeking optimization
  - Add bounds checking to prevent reading beyond file boundaries
  - _Requirements: 3.4, 3.5, 7.1, 7.2_

- [ ] 5. Implement Data Streaming Component
  - Create readChunk() methods for sequential frame reading
  - Implement readFrameData() to read complete FLAC frames
  - Add proper MediaChunk creation with frame data and timing
  - Handle EOF detection and stream completion
  - _Requirements: 3.3, 3.4, 3.7, 3.8, 8.3, 8.4_

- [ ] 5.1 Implement Frame Data Reading
  - Read complete FLAC frames including headers and CRC
  - Create MediaChunk objects with proper stream_id and timing
  - Set timestamp_samples based on frame sample position
  - Handle frame reading errors and corrupted data gracefully
  - _Requirements: 3.3, 3.4, 3.8, 6.4, 6.6_

- [ ] 5.2 Add Position Tracking
  - Maintain current sample position during sequential reading
  - Update position based on frame block sizes
  - Provide accurate position reporting through getPosition()
  - Handle position tracking across seeks and resets
  - _Requirements: 5.5, 5.6, 5.7, 5.8_

- [ ] 6. Implement Seeking Operations
  - Create seekTo() method for timestamp-based seeking
  - Implement seekWithTable() for SEEKTABLE-based seeking
  - Add seekBinary() for binary search seeking without seek table
  - Create seekLinear() as fallback for sample-accurate positioning
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 4.7, 4.8_

- [ ] 6.1 Implement SEEKTABLE-Based Seeking
  - Find closest seek point before target sample position
  - Seek to file position specified in seek table
  - Parse frames forward from seek point to exact target
  - Handle seek table errors and missing seek points
  - _Requirements: 4.1, 4.6, 4.8, 6.3_

- [ ] 6.2 Add Binary Search Seeking
  - Estimate initial file position based on target sample and file size
  - Use binary search to refine position by parsing frame headers
  - Handle variable bitrate and compression ratio variations
  - Provide reasonable seeking performance for files without seek tables
  - _Requirements: 4.2, 4.8, 7.3, 7.7_

- [ ] 6.3 Implement Linear Seeking
  - Parse frames sequentially from current or seek table position
  - Provide sample-accurate positioning for precise seeking
  - Use as fallback when other seeking methods fail
  - Optimize for short-distance seeks from current position
  - _Requirements: 4.3, 4.4, 4.6, 4.7_

- [ ] 7. Add Duration and Position Calculation
  - Implement getDuration() using total samples from STREAMINFO
  - Create getPosition() for current playback position reporting
  - Add sample-to-millisecond conversion utilities
  - Handle edge cases with missing or invalid duration information
  - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7, 5.8_

- [ ] 7.1 Implement Duration Calculation
  - Use total samples and sample rate from STREAMINFO for primary duration
  - Fall back to file size estimation when STREAMINFO is incomplete
  - Handle very long files without integer overflow
  - Provide duration in milliseconds for consistency with other demuxers
  - _Requirements: 5.1, 5.2, 5.8, 8.4_

- [ ] 7.2 Add Position Tracking and Reporting
  - Maintain accurate current sample position during playback
  - Convert sample positions to milliseconds for position reporting
  - Handle position updates during seeking operations
  - Provide both sample-based and time-based position information
  - _Requirements: 5.3, 5.4, 5.5, 5.6, 5.7_

- [ ] 8. Implement Error Handling and Robustness
  - Add comprehensive error checking for all parsing operations
  - Implement graceful handling of corrupted metadata and frames
  - Create recovery mechanisms for lost frame sync
  - Add proper resource cleanup in all error paths
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 6.7, 6.8_

- [ ] 8.1 Add Container-Level Error Handling
  - Validate fLaC stream marker and reject non-FLAC files
  - Handle corrupted metadata blocks by skipping and continuing
  - Provide reasonable defaults when STREAMINFO is missing or invalid
  - Log errors appropriately without stopping playback when possible
  - _Requirements: 6.1, 6.2, 6.3, 6.8_

- [ ] 8.2 Implement Frame-Level Error Recovery
  - Handle lost frame sync by searching for next valid frame
  - Skip corrupted frames and continue with subsequent frames
  - Log CRC mismatches but attempt to use frame data
  - Provide silence output for completely unrecoverable frames
  - _Requirements: 6.4, 6.5, 6.6, 6.7_

- [ ] 9. Optimize Performance and Memory Usage
  - Implement efficient metadata parsing with minimal memory usage
  - Add bounded buffering to prevent memory exhaustion
  - Optimize I/O operations for both local files and network streams
  - Create efficient seek table lookup and frame sync detection
  - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7, 7.8_

- [ ] 9.1 Implement Memory Management
  - Use streaming approach for large file processing
  - Implement bounded buffers for metadata and frame data
  - Optimize seek table storage for memory efficiency
  - Add proper cleanup of all allocated resources
  - _Requirements: 7.1, 7.2, 7.6, 7.8_

- [ ] 9.2 Add Performance Optimizations
  - Optimize frame sync detection with efficient bit manipulation
  - Use binary search for large seek table lookups
  - Minimize I/O operations during seeking
  - Add read-ahead buffering for network streams
  - _Requirements: 7.3, 7.4, 7.5, 7.7_

- [ ] 10. Integrate with Demuxer Architecture
  - Ensure proper integration with IOHandler subsystem
  - Implement StreamInfo population with accurate FLAC parameters
  - Add MediaFactory registration for .flac files
  - Create proper error reporting using PsyMP3 conventions
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 8.8_

- [ ] 10.1 Complete IOHandler Integration
  - Use IOHandler interface exclusively for all file I/O operations
  - Support both FileIOHandler and HTTPIOHandler implementations
  - Handle large file operations with proper 64-bit offset support
  - Propagate I/O errors appropriately through demuxer interface
  - _Requirements: 8.5, 8.6, 7.1, 7.7_

- [ ] 10.2 Add MediaFactory Integration
  - Register FLACDemuxer for .flac file extension detection
  - Add audio/flac MIME type support
  - Implement format detection based on fLaC stream marker
  - Integrate with content analysis and format probing system
  - _Requirements: 8.1, 8.2, 8.7_

- [ ] 11. Ensure Compatibility with Existing Implementation
  - Test with all FLAC files that work with current implementation
  - Maintain equivalent metadata extraction capabilities
  - Provide comparable or better seeking accuracy and performance
  - Ensure seamless integration with DemuxedStream bridge
  - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 9.6, 9.7, 9.8_

- [ ] 11.1 Implement Compatibility Testing
  - Create test suite with various FLAC file types and encoders
  - Test metadata extraction against current implementation results
  - Verify seeking accuracy and performance benchmarks
  - Ensure all previously working files continue to function
  - _Requirements: 9.1, 9.3, 9.4, 9.8_

- [ ] 11.2 Add Performance Validation
  - Benchmark parsing and seeking performance against current code
  - Measure memory usage and ensure reasonable resource consumption
  - Test with large files and various compression levels
  - Validate thread safety and concurrent access patterns
  - _Requirements: 9.5, 9.6, 9.7, 10.1, 10.2, 10.3, 10.4_

- [ ] 12. Add Thread Safety and Concurrency Support
  - Implement proper synchronization for shared state access
  - Add thread-safe metadata and position tracking
  - Handle concurrent seeking and reading operations safely
  - Ensure proper cleanup during multi-threaded destruction
  - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.5, 10.6, 10.7, 10.8_

- [ ] 12.1 Implement Thread Synchronization
  - Add mutex protection for shared demuxer state
  - Use atomic operations for position tracking where appropriate
  - Implement thread-safe error state propagation
  - Add proper synchronization for seek operations
  - _Requirements: 10.1, 10.2, 10.5, 10.7_

- [ ] 12.2 Add Concurrent Access Support
  - Handle multiple threads accessing metadata safely
  - Implement thread-safe I/O operations through IOHandler
  - Add proper cleanup coordination during destruction
  - Test concurrent seeking and reading scenarios
  - _Requirements: 10.3, 10.4, 10.6, 10.8_

- [ ] 13. Create Comprehensive Testing Suite
  - Implement unit tests for all major parsing and seeking functions
  - Add integration tests with various FLAC file types
  - Create performance benchmarks and regression tests
  - Test error handling and recovery scenarios
  - _Requirements: All requirements validation_

- [ ] 13.1 Implement Unit Tests
  - Test metadata block parsing with various block types and sizes
  - Verify frame detection and header parsing accuracy
  - Test seeking algorithms with and without seek tables
  - Validate error handling and recovery mechanisms
  - _Requirements: 1.1-1.8, 2.1-2.8, 3.1-3.8, 4.1-4.8_

- [ ] 13.2 Add Integration and Performance Tests
  - Test with FLAC files from various encoders and versions
  - Benchmark seeking performance and memory usage
  - Test integration with IOHandler implementations
  - Validate compatibility with existing FLAC playback
  - _Requirements: 5.1-5.8, 6.1-6.8, 7.1-7.8, 8.1-8.8, 9.1-9.8_

- [ ] 14. Documentation and Code Quality
  - Add comprehensive inline documentation for all public methods
  - Create developer documentation for FLAC container format handling
  - Document seeking algorithms and optimization opportunities
  - Ensure code follows PsyMP3 style guidelines and conventions
  - _Requirements: 8.7, 8.8, 9.7, 9.8_

- [ ] 14.1 Create API Documentation
  - Document all FLACDemuxer public methods with usage examples
  - Explain FLAC container format and metadata block structure
  - Document seeking strategies and performance characteristics
  - Add troubleshooting guide for common issues
  - _Requirements: 8.7, 8.8, 10.1-10.8_

- [ ] 14.2 Add Developer Guide
  - Document integration with FLACCodec and other components
  - Explain relationship between container parsing and bitstream decoding
  - Provide guidance for extending FLAC support
  - Document thread safety considerations and best practices
  - _Requirements: 9.7, 9.8, 10.1-10.8_