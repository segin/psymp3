# Implementation Plan

- [x] 1. Set up PCM codec module structure
  - Create `src/codecs/pcm/` and `include/codecs/pcm/` directories
  - Create `src/codecs/pcm/Makefile.am` to build `libpcmcodecs.a` convenience library
  - Update `src/codecs/Makefile.am` to include pcm subdirectory in SUBDIRS
  - Update `src/Makefile.am` to link `libpcmcodecs.a`
  - Update `configure.ac` to include `src/codecs/pcm/Makefile` in AC_CONFIG_FILES
  - Add conditional compilation support with ENABLE_MULAW_CODEC and ENABLE_ALAW_CODEC flags
  - _Requirements: 9.1, 10.8, 12.7_

- [x] 2. Implement SimplePCMCodec base class
  - Create `include/codecs/pcm/SimplePCMCodec.h` with base class interface
  - Create `src/codecs/pcm/SimplePCMCodec.cpp` with common PCM codec functionality
  - Implement AudioCodec interface methods (initialize, decode, flush, reset)
  - Add protected virtual methods for format-specific conversion (convertSamples, getBytesPerInputSample)
  - Implement common multi-channel processing and AudioFrame generation logic
  - Add namespace `PsyMP3::Codec::PCM::`
  - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5_

- [x] 3. Implement MuLawCodec class structure
  - Create `include/codecs/pcm/MuLawCodec.h` with MuLawCodec class declaration
  - Create `src/codecs/pcm/MuLawCodec.cpp` with class implementation
  - Inherit from SimplePCMCodec base class
  - Implement constructor accepting StreamInfo parameter
  - Override getCodecName() to return "mulaw"
  - Override getBytesPerInputSample() to return 1
  - Add static const MULAW_TO_PCM[256] lookup table declaration
  - _Requirements: 1.1, 9.1, 9.2, 10.1_

- [x] 4. Implement ALawCodec class structure
  - Create `include/codecs/pcm/ALawCodec.h` with ALawCodec class declaration
  - Create `src/codecs/pcm/ALawCodec.cpp` with class implementation
  - Inherit from SimplePCMCodec base class
  - Implement constructor accepting StreamInfo parameter
  - Override getCodecName() to return "alaw"
  - Override getBytesPerInputSample() to return 1
  - Add static const ALAW_TO_PCM[256] lookup table declaration
  - _Requirements: 2.1, 9.1, 9.2, 10.2_

- [x] 5. Generate ITU-T G.711 compliant μ-law lookup table
  - Implement initializeMuLawTable() static method
  - Generate all 256 μ-law to PCM conversion values according to ITU-T G.711 specification
  - Ensure μ-law silence value 0xFF maps to PCM value 0
  - Verify bit-perfect accuracy for all input values
  - Initialize static const MULAW_TO_PCM[256] array
  - _Requirements: 1.1, 1.3, 1.6, 6.1, 6.4_

- [x] 5.1 Write property test for μ-law lookup table completeness
  - **Property 2: Lookup Table Completeness**
  - **Validates: Requirements 1.7**

- [x] 5.2 Write property test for μ-law ITU-T G.711 conversion accuracy
  - **Property 1: ITU-T G.711 Conversion Accuracy**
  - **Validates: Requirements 1.1, 6.1, 6.4**

- [x] 5.3 Write property test for μ-law silence value handling
  - **Property 3: Silence Value Handling**
  - **Validates: Requirements 1.6, 6.6**

- [ ] 6. Generate ITU-T G.711 compliant A-law lookup table
  - Implement initializeALawTable() static method
  - Generate all 256 A-law to PCM conversion values according to ITU-T G.711 specification
  - Ensure A-law closest-to-silence value 0x55 maps to PCM value -8
  - Verify bit-perfect accuracy for all input values
  - Initialize static const ALAW_TO_PCM[256] array
  - _Requirements: 2.1, 2.3, 2.6, 6.2, 6.4_

- [ ] 6.1 Write property test for A-law lookup table completeness
  - **Property 2: Lookup Table Completeness**
  - **Validates: Requirements 2.7**

- [ ] 6.2 Write property test for A-law ITU-T G.711 conversion accuracy
  - **Property 1: ITU-T G.711 Conversion Accuracy**
  - **Validates: Requirements 2.1, 6.2, 6.4**

- [ ] 6.3 Write property test for A-law silence value handling
  - **Property 3: Silence Value Handling**
  - **Validates: Requirements 2.6, 6.6**

- [x] 7. Implement MuLawCodec canDecode validation
  - Implement canDecode() method to check StreamInfo codec_name
  - Return true for "mulaw", "pcm_mulaw", and "g711_mulaw" codec names
  - Return false for all other codec names including A-law variants
  - Add validation for μ-law compatible parameters
  - _Requirements: 9.6, 10.1, 10.5, 10.7_

- [-] 8. Implement ALawCodec canDecode validation
  - Implement canDecode() method to check StreamInfo codec_name
  - Return true for "alaw", "pcm_alaw", and "g711_alaw" codec names
  - Return false for all other codec names including μ-law variants
  - Add validation for A-law compatible parameters
  - _Requirements: 9.6, 10.2, 10.6, 10.7_

- [ ] 8.1 Write property test for codec selection exclusivity
  - **Property 4: Codec Selection Exclusivity**
  - **Validates: Requirements 9.6, 10.5, 10.6**

- [-] 9. Implement MuLawCodec sample conversion
  - Override convertSamples() method in MuLawCodec
  - Use MULAW_TO_PCM lookup table for 8-bit to 16-bit conversion
  - Process samples sequentially for optimal cache utilization
  - Handle multi-channel data with proper interleaving
  - Generate 16-bit signed PCM output in host byte order
  - Support variable chunk sizes for VoIP packet processing
  - _Requirements: 1.2, 1.8, 3.4, 3.7, 5.1, 5.5, 7.6_

- [ ] 9.1 Write property test for μ-law sample count preservation
  - **Property 5: Sample Count Preservation**
  - **Validates: Requirements 1.2**

- [ ] 9.2 Write property test for μ-law multi-channel interleaving
  - **Property 6: Multi-channel Interleaving Consistency**
  - **Validates: Requirements 7.6**

- [ ] 10. Implement ALawCodec sample conversion
  - Override convertSamples() method in ALawCodec
  - Use ALAW_TO_PCM lookup table for 8-bit to 16-bit conversion
  - Process samples sequentially for optimal cache utilization
  - Handle multi-channel data with proper interleaving
  - Generate 16-bit signed PCM output in host byte order
  - Support variable chunk sizes for VoIP packet processing
  - _Requirements: 2.2, 2.8, 3.4, 3.7, 5.1, 5.5, 7.6_

- [ ] 10.1 Write property test for A-law sample count preservation
  - **Property 5: Sample Count Preservation**
  - **Validates: Requirements 2.2**

- [ ] 10.2 Write property test for A-law multi-channel interleaving
  - **Property 6: Multi-channel Interleaving Consistency**
  - **Validates: Requirements 7.6**

- [ ] 11. Implement parameter handling and defaults
  - Add logic to extract sample rate from StreamInfo (default to 8 kHz for raw streams)
  - Add logic to extract channel count from StreamInfo (default to mono for raw streams)
  - Support sample rates: 8 kHz, 16 kHz, 32 kHz, 48 kHz
  - Support mono and stereo channel configurations
  - Validate parameter ranges and reject unsupported configurations
  - _Requirements: 1.4, 1.5, 2.4, 2.5, 3.2, 3.3, 3.5, 7.1, 7.2, 7.3, 7.4, 7.5, 7.7, 7.8_

- [ ] 11.1 Write property test for container parameter preservation
  - **Property 9: Container Parameter Preservation**
  - **Validates: Requirements 4.8, 7.5**

- [ ] 11.2 Write property test for raw stream default parameters
  - **Property 10: Raw Stream Default Parameters**
  - **Validates: Requirements 3.2, 3.5, 7.7**

- [ ] 12. Implement comprehensive error handling
  - Add initialization error handling for unsupported formats
  - Add validation for invalid StreamInfo parameters
  - Implement runtime error recovery for corrupted data
  - Handle truncated streams gracefully
  - Handle null or zero-size MediaChunk data
  - Maintain decoder state consistency after errors
  - Return appropriate error codes for memory allocation failures
  - Add Debug::log() calls for all error conditions
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 8.8, 9.7_

- [ ] 12.1 Write property test for error state consistency
  - **Property 8: Error State Consistency**
  - **Validates: Requirements 8.8**

- [ ] 13. Implement MediaFactory registration
  - Create registerMuLawCodec() function with ENABLE_MULAW_CODEC guards
  - Register MuLawCodec for "mulaw" and "pcm_mulaw" codec names
  - Create registerALawCodec() function with ENABLE_ALAW_CODEC guards
  - Register ALawCodec for "alaw" and "pcm_alaw" codec names
  - Add registration calls to codec initialization system
  - Ensure conditional compilation allows independent codec enabling
  - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.8_

- [ ] 14. Add Debug logging integration
  - Add Debug::log() calls for codec initialization status
  - Add Debug::log() calls for format detection and validation
  - Add Debug::log() calls for parameter extraction
  - Add Debug::log() calls for error conditions
  - Add optional performance metrics logging when debug enabled
  - Ensure all logging uses PsyMP3's Debug system
  - Verify thread-safe logging operations
  - _Requirements: 9.7, 9.8, 11.7_

- [ ] 15. Update psymp3.h master header
  - Add conditional includes for PCM codec headers
  - Add using declarations for MuLawCodec and ALawCodec in global namespace
  - Ensure includes use full path: "codecs/pcm/MuLawCodec.h"
  - Add conditional compilation guards matching codec availability
  - _Requirements: 9.1_

- [ ] 16. Create unit tests for conversion accuracy
  - Write test verifying all 256 μ-law values convert correctly
  - Write test verifying all 256 A-law values convert correctly
  - Write test comparing against ITU-T reference implementations
  - Write test for μ-law silence value (0xFF → 0)
  - Write test for A-law silence value (0x55 → -8)
  - Write tests for specific known input/output pairs from ITU-T test vectors
  - _Requirements: 6.3, 6.4, 6.5_

- [ ] 17. Create unit tests for codec selection
  - Write tests for MuLawCodec canDecode() with various StreamInfo
  - Write tests for ALawCodec canDecode() with various StreamInfo
  - Write tests verifying codec_name variants (mulaw, pcm_mulaw, g711_mulaw)
  - Write tests verifying rejection of incompatible formats
  - Write tests for MediaFactory codec selection
  - _Requirements: 10.5, 10.6, 10.7_

- [ ] 18. Create unit tests for error handling
  - Write tests for null chunk data handling
  - Write tests for zero-size chunk handling
  - Write tests for invalid StreamInfo parameters
  - Write tests for unsupported codec_name values
  - Write tests for unsupported sample rates
  - Write tests for unsupported channel counts
  - Write tests verifying error recovery and state consistency
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.8_

- [ ] 19. Create integration tests with SimplePCMCodec
  - Write tests for initialize() with various StreamInfo configurations
  - Write tests for decode() with different MediaChunk sizes
  - Write tests for decode() with VoIP-typical small packets
  - Write tests for flush() behavior
  - Write tests for reset() functionality
  - Write tests for multi-channel processing
  - Write tests for various sample rates (8, 16, 32, 48 kHz)
  - _Requirements: 9.2, 9.3, 9.4, 9.5, 7.1, 7.2, 7.3, 7.4_

- [ ] 20. Create thread safety tests
  - Write tests for concurrent codec instance creation
  - Write tests for concurrent decode operations on different instances
  - Write tests for concurrent access to shared lookup tables
  - Write tests verifying instance independence
  - Write tests for thread-safe logging operations
  - Write tests for cleanup during concurrent operations
  - _Requirements: 11.1, 11.2, 11.3, 11.4, 11.5, 11.7_

- [ ] 20.1 Write property test for thread safety independence
  - **Property 7: Thread Safety Independence**
  - **Validates: Requirements 11.1, 11.2**

- [ ] 21. Create performance tests
  - Write tests measuring decoding performance vs real-time requirements
  - Write tests for 8 kHz, 16 kHz, 32 kHz, and 48 kHz sample rates
  - Write tests for multi-channel processing efficiency
  - Write tests measuring samples processed per second
  - Write tests for lookup table memory footprint verification
  - Write tests for concurrent codec instance memory usage
  - Write tests for cache efficiency with sequential access
  - Benchmark against reference implementations
  - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.7, 5.8_

- [ ] 22. Checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.
