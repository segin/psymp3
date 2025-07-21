# **OGG DEMUXER IMPLEMENTATION PLAN**

## **Implementation Tasks**

- [x] 1. Fix Critical Seeking Issues
  - Fix segmentation fault in seekToPage function by properly validating ogg_page structures
  - Remove incorrect use of ogg_sync_pageseek and use ogg_sync_pageout exclusively
  - Implement proper bisection search algorithm with valid page detection
  - Add comprehensive error checking for all libogg API calls
  - _Requirements: 5.1, 5.2, 5.3, 5.7, 7.1, 7.2_

- [x] 1.1 Implement Safe Page Extraction
  - Replace ogg_sync_pageseek usage with ogg_sync_pageout for reliable page extraction
  - Add validation that ogg_page structure is properly initialized before calling ogg_page_* functions
  - Implement proper buffer management for libogg sync state
  - Add bounds checking for all page access operations
  - _Requirements: 1.6, 1.7, 7.1, 7.4_

- [x] 1.2 Fix Bisection Search Algorithm
  - Rewrite seekToPage to use only ogg_sync_pageout for page extraction
  - Implement proper file position tracking during bisection search
  - Add stream ID validation and granule position checking
  - Remove non-existent ogg_page_bytes function call and use alternative position calculation
  - _Requirements: 5.1, 5.2, 5.3, 5.8_

- [x] 1.3 Correct Header Handling After Seeks
  - Remove incorrect header resending logic after seek operations
  - Ensure decoder state is maintained across seeks (headers sent only once)
  - Update position tracking without reinitializing codec state
  - Follow reference implementation patterns from libvorbisfile and opusfile
  - _Requirements: 5.6, 5.7, 3.7, 10.6_

- [ ] 2. Implement Robust Granule Position Handling
  - Create codec-specific granule-to-millisecond conversion functions
  - Fix Opus granule position calculation to account for 48kHz rate and pre-skip
  - Implement proper Vorbis granule position handling
  - Add FLAC granule position support for Ogg FLAC streams
  - _Requirements: 1.4, 5.4, 5.5, 6.5, 6.6_

- [ ] 2.1 Implement Codec-Specific Time Conversion
  - Write granuleToMs function with codec-specific logic for Vorbis, Opus, FLAC
  - Write msToGranule function with proper pre-skip handling for Opus
  - Add validation for granule position ranges and invalid values
  - Test time conversion accuracy across different sample rates and codecs
  - _Requirements: 5.4, 5.5, 6.5, 6.6_

- [ ] 2.2 Fix Duration Calculation Logic
  - Implement getLastGranulePosition with robust backward scanning
  - Add fallback duration calculation methods (header info, tracked max granule)
  - Fix manual OggS signature scanning for corrupted or unusual files
  - Ensure duration calculation works for all supported Ogg codecs
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.7_

- [ ] 3. Enhance Error Handling and Robustness
  - Add comprehensive validation for all libogg API return values
  - Implement graceful handling of corrupted pages and invalid data
  - Add proper resource cleanup in all error paths
  - Create bounded parsing loops to prevent infinite hangs
  - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7, 7.8_

- [ ] 3.1 Implement Safe Container Parsing
  - Add maximum iteration limits to header parsing loops
  - Validate packet sizes and header structures before processing
  - Handle incomplete or truncated files gracefully
  - Add proper error reporting with specific failure reasons
  - _Requirements: 1.1, 1.2, 7.1, 7.2, 7.4_

- [ ] 3.2 Add Memory Safety Measures
  - Implement bounded packet queues to prevent memory exhaustion
  - Add proper cleanup of libogg structures in destructor and error paths
  - Validate buffer sizes and prevent buffer overflows
  - Add null pointer checks for all dynamic allocations
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6_

- [ ] 4. Optimize Performance and Memory Usage
  - Implement efficient packet buffering with bounded queues
  - Optimize I/O operations for both local files and HTTP streams
  - Add read-ahead buffering for network sources
  - Minimize memory copying in packet processing
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 8.8_

- [ ] 4.1 Implement Efficient Buffering Strategy
  - Design packet queue size limits based on memory constraints
  - Add adaptive buffering for different stream types and bitrates
  - Implement buffer reuse to minimize allocation overhead
  - Add metrics for buffer usage and performance monitoring
  - _Requirements: 8.2, 8.4, 8.6, 8.7_

- [ ] 4.2 Optimize Seeking Performance
  - Implement page caching for recently accessed pages
  - Add seek position hints to reduce bisection search iterations
  - Optimize file I/O patterns for seeking operations
  - Add performance metrics for seek operations
  - _Requirements: 8.1, 8.3, 8.7, 8.8_

- [ ] 5. Enhance Codec Support and Metadata Handling
  - Complete Opus header parsing with proper OpusTags support
  - Enhance Vorbis comment parsing with full UTF-8 support
  - Add comprehensive FLAC-in-Ogg support with STREAMINFO parsing
  - Implement Speex header parsing for completeness
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 3.1, 3.2, 3.3, 3.4, 3.5_

- [ ] 5.1 Complete Opus Integration
  - Fix OpusHead header parsing with all required fields
  - Implement complete OpusTags metadata extraction
  - Add proper pre-skip sample handling in timing calculations
  - Test with various Opus encoder configurations
  - _Requirements: 2.4, 3.4, 3.5, 5.4, 6.5_

- [ ] 5.2 Enhance Vorbis Support
  - Complete Vorbis comment parsing with proper UTF-8 handling
  - Add support for all three Vorbis header types
  - Implement proper Vorbis granule position interpretation
  - Test with various Vorbis encoder versions and settings
  - _Requirements: 2.1, 3.1, 3.2, 5.5, 6.6_

- [ ] 5.3 Complete FLAC-in-Ogg Support
  - Implement parseFLACHeaders with proper Ogg FLAC identification header parsing
  - Extract STREAMINFO metadata block from Ogg FLAC identification header
  - Add support for FLAC granule position handling (sample-based like Vorbis)
  - Ensure FLAC-in-Ogg streams are properly detected and configured
  - Test with various FLAC-in-Ogg files and compression levels
  - _Requirements: 2.3, 3.3, 5.5, 6.4_

- [ ] 6. Add Comprehensive Testing and Validation
  - Create unit tests for all codec detection and header parsing functions
  - Add integration tests with various Ogg file types and encoders
  - Implement regression tests for previously failing files
  - Add performance benchmarks for seeking and streaming operations
  - _Requirements: All requirements validation_

- [ ] 6.1 Create Codec-Specific Test Suite
  - Test Vorbis files with various quality settings and metadata
  - Test Opus files with different channel configurations and pre-skip values
  - Test FLAC-in-Ogg files with various compression levels and verify lossless decoding
  - Test Speex files for completeness of codec support
  - Test multiplexed streams and chained bitstreams with mixed codec types
  - Verify that FLAC-in-Ogg files are properly distinguished from native FLAC files
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7_

- [ ] 6.2 Implement Seeking Accuracy Tests
  - Test seeking precision across different file sizes and durations
  - Validate seek accuracy at various positions (start, middle, end)
  - Test seeking in files with variable bitrates
  - Compare seeking behavior with reference implementations
  - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.8_

- [x] 7. Integrate with PsyMP3 Architecture
  - Ensure proper integration with IOHandler subsystem
  - Add comprehensive debug logging with appropriate categories
  - Integrate with PsyMP3's error reporting and exception system
  - Update MediaFactory registration for improved Ogg format detection
  - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.5, 10.6, 10.7_

- [ ] 7.1 Complete IOHandler Integration
  - Ensure all I/O operations use IOHandler interface exclusively
  - Test with both FileIOHandler and HTTPIOHandler implementations
  - Add proper error propagation from IOHandler to demuxer
  - Validate large file support and network streaming capabilities
  - _Requirements: 10.1, 10.4, 8.1, 8.7_

- [x] 7.2 Add Debug Logging and Monitoring
  - Add comprehensive debug logging for all major operations
  - Include timing information for performance analysis
  - Add error condition logging with detailed context
  - Implement configurable logging levels for different components
  - _Requirements: 10.3, 8.8, 7.8_

- [ ] 8. Documentation and Code Quality
  - Add comprehensive inline documentation for all public methods
  - Create developer documentation for extending codec support
  - Add code comments explaining complex algorithms (bisection search, granule conversion)
  - Ensure code follows PsyMP3 style guidelines and conventions
  - _Requirements: 10.8, 10.9, 10.10_

- [ ] 8.1 Document Public API
  - Add detailed documentation for all OggDemuxer public methods
  - Document expected behavior for edge cases and error conditions
  - Add usage examples for common operations
  - Document thread safety considerations and limitations
  - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 9.6, 9.7_

- [ ] 8.2 Create Developer Guide
  - Document how to add support for new Ogg-encapsulated codecs
  - Explain the relationship between granule positions and timing
  - Document the seeking algorithm and optimization opportunities
  - Provide troubleshooting guide for common issues
  - _Requirements: 10.8, 10.9, 10.10_