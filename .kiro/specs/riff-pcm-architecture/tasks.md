# RIFF/PCM Architecture Implementation Plan

- [ ] 1. Set up core RIFF demuxer structure and interfaces
  - Create RIFFDemuxer class inheriting from Demuxer base class
  - Implement basic constructor and destructor with proper IOHandler management
  - Add member variables for RIFF parsing state and audio format information
  - Write unit tests for basic RIFFDemuxer instantiation and cleanup
  - _Requirements: 1.1, 10.1, 10.2_

- [ ] 2. Implement RIFF container parsing infrastructure
  - Create RIFF header parsing with signature validation and size checking
  - Implement chunk header reading with FourCC and size extraction
  - Add WAVE format type validation and chunk enumeration
  - Write unit tests for RIFF header parsing and chunk navigation
  - _Requirements: 1.1, 1.2, 1.3, 8.1, 8.2_

- [ ] 3. Create fmt chunk parsing for audio format detection
  - Implement WAVE format chunk parsing with all PCM format variants
  - Add support for format tags (PCM, IEEE float, A-law, μ-law, extensible)
  - Create format parameter validation and consistency checking
  - Write unit tests for format chunk parsing with various PCM types
  - _Requirements: 1.4, 5.1, 5.2, 5.3, 5.4, 5.5, 8.3, 8.4_

- [ ] 4. Implement data chunk location and audio stream setup
  - Add data chunk parsing to locate audio sample region
  - Create StreamInfo population with PCM format parameters
  - Implement duration calculation from data size and format parameters
  - Write unit tests for data chunk parsing and stream info creation
  - _Requirements: 1.5, 6.1, 6.2, 6.3, 6.4, 10.3, 10.4_

- [ ] 5. Add RIFF metadata extraction support
  - Implement INFO chunk parsing for RIFF metadata (IART, INAM, IALB, ICMT)
  - Add ID3v2 tag support from id3 chunks
  - Create metadata field mapping to StreamInfo artist/title/album fields
  - Write unit tests for metadata extraction from various RIFF files
  - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7, 7.8_

- [ ] 6. Implement RIFF seeking and position management
  - Create sample-accurate seeking using direct byte offset calculation
  - Add position tracking in both sample and time units
  - Implement boundary checking and EOF detection
  - Write unit tests for seeking accuracy and position tracking
  - _Requirements: 6.5, 6.6, 6.7, 6.8, 9.1, 9.2_

- [ ] 7. Set up core AIFF demuxer structure and interfaces
  - Create AIFFDemuxer class inheriting from Demuxer base class
  - Implement basic constructor with big-endian byte order handling
  - Add member variables for AIFF parsing state and format information
  - Write unit tests for basic AIFFDemuxer instantiation and endian handling
  - _Requirements: 2.1, 10.1, 10.2_

- [ ] 8. Implement AIFF container parsing with big-endian support
  - Create FORM header parsing with signature validation and big-endian size
  - Implement IFF chunk header reading with proper byte order conversion
  - Add AIFF/AIFC format type detection and chunk enumeration
  - Write unit tests for AIFF header parsing and big-endian chunk navigation
  - _Requirements: 2.1, 2.2, 2.3, 8.1, 8.2_

- [ ] 9. Create COMM chunk parsing for AIFF audio format detection
  - Implement Common chunk parsing with IEEE 80-bit float sample rate conversion
  - Add support for compression types (NONE, sowt, fl32, fl64, alaw, ulaw)
  - Create format parameter extraction and validation for AIFF variants
  - Write unit tests for COMM chunk parsing and IEEE float conversion
  - _Requirements: 2.4, 2.5, 2.6, 2.7, 2.8_

- [ ] 10. Implement SSND chunk parsing and audio data location
  - Add Sound Data chunk parsing with offset and block size handling
  - Create audio data region mapping with proper offset calculations
  - Implement StreamInfo population with AIFF-specific parameters
  - Write unit tests for SSND chunk parsing and data offset handling
  - _Requirements: 2.4, 2.5, 6.1, 6.2, 10.3, 10.4_

- [ ] 11. Add AIFF metadata extraction support
  - Implement NAME, ANNO, AUTH, COPY chunk parsing for AIFF metadata
  - Create text encoding handling for various AIFF metadata formats
  - Add metadata field mapping to StreamInfo with proper encoding conversion
  - Write unit tests for AIFF metadata extraction and encoding handling
  - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7, 7.8_

- [ ] 12. Implement AIFF seeking with big-endian considerations
  - Create sample-accurate seeking with proper endian-aware calculations
  - Add position tracking compatible with AIFF data layout
  - Implement boundary checking with SSND offset and size validation
  - Write unit tests for AIFF seeking accuracy and position management
  - _Requirements: 6.5, 6.6, 6.7, 6.8, 9.1, 9.2_

- [ ] 13. Set up container-agnostic PCM codec structure
  - Create PCMCodec class inheriting from AudioCodec base class
  - Implement PCM format enumeration for all supported variants
  - Add format detection logic from StreamInfo parameters
  - Write unit tests for PCM codec instantiation and format detection
  - _Requirements: 3.1, 3.2, 5.1, 5.2, 10.1, 10.2_

- [ ] 14. Implement PCM format detection and configuration
  - Create determinePCMFormat method with comprehensive format detection
  - Add endianness detection from container type and codec parameters
  - Implement format validation and unsupported format reporting
  - Write unit tests for PCM format detection across all variants
  - _Requirements: 3.1, 3.2, 5.1, 5.2, 5.3, 5.4, 8.3, 8.4_

- [ ] 15. Create 8-bit PCM conversion functions
  - Implement convertU8ToPCM for unsigned 8-bit to 16-bit conversion
  - Add convertS8ToPCM for signed 8-bit to 16-bit conversion
  - Create proper scaling and offset handling for 8-bit variants
  - Write unit tests for 8-bit PCM conversion accuracy
  - _Requirements: 5.1, 5.2, 3.1, 3.2_

- [ ] 16. Implement 16-bit PCM conversion with endian support
  - Create convertU16ToPCM and convertS16ToPCM with endian parameters
  - Add byte order conversion for big-endian 16-bit PCM
  - Implement direct copy optimization for native 16-bit signed PCM
  - Write unit tests for 16-bit PCM conversion and endian handling
  - _Requirements: 5.3, 3.3, 3.4_

- [ ] 17. Add 24-bit PCM conversion with downscaling
  - Implement convertU24ToPCM and convertS24ToPCM with proper scaling
  - Add support for both packed and padded 24-bit formats
  - Create optional dithering for 24-bit to 16-bit downscaling
  - Write unit tests for 24-bit PCM conversion accuracy
  - _Requirements: 5.4, 3.5, 3.6_

- [ ] 18. Create 32-bit PCM conversion functions
  - Implement convertU32ToPCM and convertS32ToPCM with range mapping
  - Add convertF32ToPCM for 32-bit float to 16-bit integer conversion
  - Create proper clipping protection and range validation
  - Write unit tests for 32-bit PCM and float conversion
  - _Requirements: 5.5, 5.6, 3.7, 3.8_

- [ ] 19. Implement 64-bit float PCM conversion
  - Create convertF64ToPCM for 64-bit double to 16-bit conversion
  - Add proper range checking and clipping for double precision
  - Implement endian-aware double precision handling
  - Write unit tests for 64-bit float conversion accuracy
  - _Requirements: 5.6, 3.7, 3.8_

- [ ] 20. Add A-law and μ-law decompression support
  - Create convertALawToPCM with standard A-law lookup table
  - Implement convertMuLawToPCM with standard μ-law lookup table
  - Add precomputed lookup tables for efficient expansion
  - Write unit tests for A-law and μ-law decompression accuracy
  - _Requirements: 5.7, 5.8, 3.8_

- [ ] 21. Implement PCM codec decode and flush methods
  - Create decode method with MediaChunk processing and format conversion
  - Add flush method for any remaining buffered data
  - Implement proper AudioFrame creation with timing information
  - Write unit tests for PCM decoding pipeline and AudioFrame generation
  - _Requirements: 3.1, 3.2, 10.4, 10.5_

- [ ] 22. Add comprehensive error handling for container parsing
  - Implement graceful handling of invalid RIFF and AIFF signatures
  - Add recovery from truncated headers and corrupted chunks
  - Create validation for format parameters and data consistency
  - Write unit tests for error handling with corrupted files
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 8.8_

- [ ] 23. Implement PCM codec error handling and recovery
  - Add format detection error handling with fallback strategies
  - Create buffer overflow protection and input validation
  - Implement range error handling with proper clipping
  - Write unit tests for PCM codec error scenarios and recovery
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 8.8_

- [ ] 24. Optimize memory usage and performance
  - Implement streaming approach with minimal buffering
  - Add reusable buffer management for conversion operations
  - Create efficient seeking with direct position calculation
  - Write performance tests and memory usage profiling
  - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 9.6, 9.7, 9.8_

- [ ] 25. Integrate RIFF/PCM architecture with DemuxerFactory
  - Add RIFF and AIFF format signatures to DemuxerFactory
  - Implement createDemuxerForFormat integration for riff and aiff formats
  - Create proper format detection priority handling
  - Write unit tests for factory integration and format detection
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 10.1, 10.2_

- [ ] 26. Integrate PCM codec with AudioCodecFactory
  - Register PCM codec factory function with AudioCodecFactory
  - Add codec capability detection for PCM variants
  - Implement proper codec name and format matching
  - Write unit tests for codec factory integration
  - _Requirements: 4.5, 4.6, 4.7, 4.8, 10.1, 10.2_

- [ ] 27. Ensure backward compatibility with existing functionality
  - Validate that all currently working WAV files continue to function
  - Test that all currently working AIFF files maintain compatibility
  - Verify metadata extraction matches existing implementation behavior
  - Write compatibility tests with existing file collection
  - _Requirements: 11.1, 11.2, 11.3, 11.4, 11.5, 11.6, 11.7, 11.8_

- [ ] 28. Add thread safety and concurrency support
  - Implement proper synchronization for shared demuxer state
  - Add thread-safe seeking and reading operations
  - Create thread-local codec state management
  - Write unit tests for concurrent access and thread safety
  - _Requirements: 12.1, 12.2, 12.3, 12.4, 12.5, 12.6, 12.7, 12.8_

- [ ] 29. Create comprehensive test suite and validation
  - Write unit tests for all RIFF/AIFF parsing components
  - Add integration tests with real-world files from various sources
  - Create performance benchmarks for large files and seeking operations
  - Implement compatibility tests with existing PsyMP3 functionality
  - _Requirements: All requirements validation_

- [ ] 30. Finalize integration and documentation
  - Complete integration with existing PsyMP3 demuxer/codec architecture
  - Add proper error reporting and logging integration
  - Create comprehensive documentation for new components
  - Perform final validation against all requirements
  - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.5, 10.6, 10.7, 10.8_