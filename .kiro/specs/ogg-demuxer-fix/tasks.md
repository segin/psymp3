# **OGG DEMUXER IMPLEMENTATION PLAN**

## **Implementation Tasks**

- [x] 1. Clean Slate - Remove Existing Problematic Tests
  - Remove existing OGG-related unit tests that don't follow reference implementation patterns
  - Delete tests/test_ogg_granule_conversion.cpp and tests/test_ogg_duration_calculation.cpp
  - Remove any OGG test entries from tests/Makefile.am
  - Clean up any test artifacts and ensure clean build state
  - Verify clean build with `make clean && make -j$(nproc)` before proceeding
  - _Requirements: All - Clean foundation for proper implementation_

- [x] 2. Implement Reference-Pattern Page Extraction (Following libvorbisfile) ✅ **COMPLETED**
  - ✅ Implement _get_next_page() equivalent using ogg_sync_pageseek() patterns from libvorbisfile
  - ✅ Add proper boundary checking and data fetching logic like _get_data()
  - ✅ Implement _get_prev_page() equivalent for backward scanning with CHUNKSIZE increments
  - ✅ Add _get_prev_page_serial() equivalent for serial-number-aware backward scanning
  - ✅ Write comprehensive unit tests for all page extraction functions
  - ✅ Verify unit tests pass with `make test_page_extraction && ./test_page_extraction`
  - ✅ Ensure clean build with `make -j$(nproc)` before proceeding
  - _Requirements: 1.7, 1.8, 1.9, 5.9, 7.1_
  - **Implementation Notes:** All page extraction functions were already implemented following libvorbisfile patterns. Fixed critical deadlock issue in mutex lock ordering (io_lock before ogg_lock). Updated comprehensive unit tests to handle threading issues properly. All tests now pass successfully (11/11). Clean build verified.

- [x] 3. Implement Bisection Search Algorithm (Following ov_pcm_seek_page/op_pcm_seek_page)
  - Implement seekToPage() using exact bisection algorithm from ov_pcm_seek_page()
  - Use ogg_sync_pageseek() for page discovery (NOT ogg_sync_pageout())
  - Implement proper interval management and boundary conditions
  - Add linear scanning fallback when bisection interval becomes small
  - Use ogg_stream_packetpeek() to examine packets without consuming them
  - Write comprehensive unit tests for bisection search algorithm
  - Verify unit tests pass with `make test_bisection_search && ./test_bisection_search`
  - Ensure clean build with `make -j$(nproc)` before proceeding
  - _Requirements: 5.1, 5.2, 5.11, 7.11_

- [x] 4. Implement Safe Granule Position Arithmetic (Following libopusfile)
  - Implement granposAdd() following op_granpos_add() overflow detection patterns
  - Implement granposDiff() following op_granpos_diff() wraparound handling
  - Implement granposCmp() following op_granpos_cmp() ordering logic
  - Handle invalid granule position (-1) like reference implementations
  - Add proper overflow and underflow detection for all arithmetic operations
  - Write comprehensive unit tests for granule position arithmetic
  - Verify unit tests pass with `make test_granule_arithmetic && ./test_granule_arithmetic`
  - Ensure clean build with `make -j$(nproc)` before proceeding
  - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.5, 10.6_

- [x] 5. Implement Codec Detection and Header Processing (Following Reference Patterns)
  - Implement Vorbis codec detection using vorbis_synthesis_idheader() equivalent logic
  - Implement Opus codec detection using opus_head_parse() equivalent logic  
  - Implement FLAC codec detection using "\x7fFLAC" signature validation
  - Add proper header processing for all three header types (ID, comment, setup/tags)
  - Handle unknown codecs by returning appropriate error codes and continuing
  - Write comprehensive unit tests for codec detection and header processing
  - Verify unit tests pass with `make test_codec_detection && ./test_codec_detection`
  - Ensure clean build with `make -j$(nproc)` before proceeding
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 3.1, 3.2, 3.3, 3.4, 3.5, 3.8, 3.9, 3.10, 3.11_

- [x] 6. Implement Duration Calculation (Following op_get_last_page Patterns)
  - Implement getLastGranulePosition() using op_get_last_page() backward scanning patterns
  - Use chunk-based backward scanning with exponentially increasing chunk sizes
  - Implement proper serial number preference and boundary condition handling
  - Add fallback duration calculation methods (header info, tracked max granule)
  - Implement codec-specific granule-to-time conversion following reference patterns
  - Write comprehensive unit tests for duration calculation
  - Verify unit tests pass with `make test_duration_calculation && ./test_duration_calculation`
  - Add unit tests to build system `Makefile.am`
  - Ensure clean build with `make -j$(nproc)` before proceeding
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 6.7, 6.8, 6.9, 6.10, 6.11_

- [x] 7. Implement Time Conversion Functions (Following Reference Implementation Logic)
  - Write granuleToMs() with codec-specific logic following reference implementations
  - Write msToGranule() with proper pre-skip handling for Opus
  - Implement Opus time conversion using opus_granule_sample() equivalent logic
  - Implement Vorbis time conversion using direct sample count mapping
  - Implement FLAC time conversion using sample count mapping like Vorbis
  - Add validation for granule position ranges and invalid values (-1)
  - Write comprehensive unit tests for time conversion accuracy
  - Verify unit tests pass with `make test_time_conversion && ./test_time_conversion`
  - Add unit tests to build system `Makefile.am`
  - Ensure clean build with `make -j$(nproc)` before proceeding
  - _Requirements: 5.4, 5.5, 6.5, 6.6, 6.8, 10.7, 10.8, 10.9_

- [ ] 8. Implement Data Streaming Component (Following _fetch_and_process_packet Patterns)
  - Implement packet streaming using ogg_stream_packetout() patterns from libvorbisfile
  - Add proper header packet handling (send once per stream, never resend after seeks)
  - Implement bounded packet queues to prevent memory exhaustion
  - Handle packet holes by returning OV_HOLE/OP_HOLE like reference implementations
  - Add proper page boundary handling using ogg_stream_pagein() patterns
  - Update position tracking from granule positions using safe arithmetic
  - Write comprehensive unit tests for data streaming functionality
  - Verify unit tests pass with `make test_data_streaming && ./test_data_streaming`
  - Add unit tests to build system `Makefile.am`
  - Ensure clean build with `make -j$(nproc)` before proceeding
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 4.7, 11.1, 11.2, 11.3, 11.4, 11.5, 11.6, 11.7_

- [ ] 9. Implement Error Handling and Robustness (Following Reference Implementation Patterns)
  - Add comprehensive error handling following libvorbisfile/libopusfile patterns
  - Implement proper error code returns (OP_EREAD, OP_EBADHEADER, OP_ENOTFORMAT, etc.)
  - Add graceful handling of corrupted pages using ogg_sync_pageseek() negative returns
  - Handle memory allocation failures with proper cleanup and error codes
  - Add proper resource cleanup in all error paths following reference patterns
  - Implement bounded parsing loops to prevent infinite hangs
  - Write comprehensive unit tests for error handling scenarios
  - Verify unit tests pass with `make test_error_handling && ./test_error_handling`
  - Add unit tests to build system `Makefile.am`
  - Ensure clean build with `make -j$(nproc)` before proceeding
  - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7, 7.8, 7.9, 7.10, 7.11, 7.12_

- [ ] 10. Implement Memory Safety and Resource Management
  - Add proper initialization and cleanup of libogg structures (ogg_sync_state, ogg_stream_state)
  - Implement bounded packet queues with size limits to prevent memory exhaustion
  - Add proper cleanup of libogg structures in destructor and error paths
  - Validate buffer sizes and prevent buffer overflows using OP_PAGE_SIZE_MAX
  - Add null pointer checks for all dynamic allocations
  - Use ogg_sync_reset() after seeks like reference implementations
  - Use ogg_stream_reset_serialno() for stream switching like reference implementations
  - Write comprehensive unit tests for memory safety and resource management
  - Verify unit tests pass with `make test_memory_safety && ./test_memory_safety`
  - Add unit tests to build system `Makefile.am`
  - Ensure clean build with `make -j$(nproc)` before proceeding
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 12.8, 12.9, 12.10_

- [ ] 11. Integrate Complete Seeking System (Combining All Components)
  - Integrate bisection search, granule arithmetic, and page extraction into complete seeking system
  - Implement main seekTo() interface following ov_pcm_seek/op_pcm_seek patterns
  - Add proper state management and stream reset after seeks
  - Ensure headers are never resent after seeks (sent only once per stream)
  - Add comprehensive integration testing for seeking across different file types
  - Test seeking accuracy with various Vorbis, Opus, and FLAC files
  - Write comprehensive unit tests for complete seeking system
  - Verify unit tests pass with `make test_seeking_integration && ./test_seeking_integration`
  - Add unit tests to build system `Makefile.am`
  - Ensure clean build with `make -j$(nproc)` before proceeding
  - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7, 5.8, 5.9, 5.10, 5.11_

- [ ] 12. Implement Complete OggDemuxer Class Integration
  - Integrate all components into complete OggDemuxer class following PsyMP3 Demuxer interface
  - Implement proper IOHandler integration for file and HTTP sources
  - Add comprehensive debug logging using PsyMP3 Debug system
  - Ensure proper error code mapping to PsyMP3 conventions
  - Add MediaChunk creation and StreamInfo population
  - Implement proper cleanup and resource management
  - Write comprehensive integration tests for complete demuxer functionality
  - Verify unit tests pass with `make test_ogg_demuxer_integration && ./test_ogg_demuxer_integration`
  - Add unit tests to build system `Makefile.am`
  - Ensure clean build with `make -j$(nproc)` before proceeding
  - _Requirements: 12.1, 12.2, 12.3, 12.4, 12.5, 12.6, 12.7_

- [ ] 13. Performance Optimization and Buffering Strategy
  - Implement efficient packet buffering with bounded queues following reference patterns
  - Optimize I/O operations for both local files and HTTP streams
  - Add read-ahead buffering for network sources using appropriate buffer sizes
  - Minimize memory copying in packet processing following reference implementation patterns
  - Implement page caching for recently accessed pages to improve seeking performance
  - Add seek position hints to reduce bisection search iterations
  - Write comprehensive performance tests and benchmarks
  - Verify unit tests pass with `make test_performance && ./test_performance`
  - Add unit tests to build system `Makefile.am`
  - Ensure clean build with `make -j$(nproc)` before proceeding
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 8.8_

- [ ] 14. Comprehensive Testing and Validation
  - Create comprehensive test suite covering all codec types (Vorbis, Opus, FLAC)
  - Test with various file types, encoders, and quality settings
  - Add regression tests for previously failing files and edge cases
  - Test seeking accuracy across different file sizes and durations
  - Test error handling with corrupted and malformed files
  - Test memory management and resource cleanup
  - Test performance with large files and network streams
  - Verify all unit tests pass with `make test_comprehensive && ./test_comprehensive`
  - Add all unit tests to build system `Makefile.am`
  - Ensure clean build with `make -j$(nproc)` before proceeding
  - _Requirements: All requirements validation_

- [ ] 15. Final Integration and Documentation
  - Complete integration with PsyMP3 MediaFactory and DemuxerFactory
  - Add comprehensive inline documentation for all public methods
  - Create developer documentation for extending codec support
  - Add code comments explaining complex algorithms (bisection search, granule conversion)
  - Ensure code follows PsyMP3 style guidelines and conventions
  - Perform final testing with real-world Ogg files
  - Verify final clean build with `make clean && make -j$(nproc)`
  - Commit all changes with proper git commit messages
  - _Requirements: 12.1, 12.2, 12.3, 12.4, 12.5, 12.6, 12.7_

