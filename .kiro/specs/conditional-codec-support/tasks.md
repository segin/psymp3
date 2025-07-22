# Implementation Plan

- [x] 1. Create registry infrastructure
  - Create CodecRegistry and DemuxerRegistry classes with static registration methods
  - Implement registry lookup and factory creation methods
  - Add error handling for unsupported codecs and formats
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5_

- [x] 2. Implement centralized registration functions
  - Create registerAllCodecs() function with conditional compilation for each codec
  - Create registerAllDemuxers() function with conditional compilation for demuxers
  - Add OggDemuxer registration logic based on available Ogg codecs
  - Ensure FLAC codec and demuxer are always registered together
  - _Requirements: 1.1, 1.2, 5.1, 5.2, 5.3, 5.4, 5.5_

- [x] 3. Add conditional compilation blocks to OggDemuxer
  - Add #ifdef HAVE_VORBIS blocks around Vorbis-specific code in OggDemuxer
  - Add #ifdef HAVE_OPUS blocks around Opus-specific code in OggDemuxer
  - Add #ifdef HAVE_FLAC blocks around FLAC-specific code in OggDemuxer
  - Update identifyCodec() method to conditionally detect codecs
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5_

- [x] 4. Update MediaFactory to use registries
  - Modify MediaFactory::initializeDefaultFormats() to call registration functions
  - Replace hardcoded codec creation with registry lookups
  - Update format detection to use enhanced priority-based resolution
  - Add standardized extension mappings for all container formats
  - Add MP2 files as part of MP3 by simply including their extensions.
  - Add the `.bit` and `.mpga` extensions for MP3 for historical reasons.
  - _Requirements: 2.1, 2.2, 2.3, 14.1, 14.2, 14.3, 15.1, 15.2, 15.3, 15.4, 15.5_

- [x] 5. Enhance format detection and routing
  - Implement improved magic byte detection with codec-specific probing for Ogg
  - Add comprehensive MIME type support for web-served files
  - Update extension mappings to include .opus for Ogg and complete MP3 extensions
  - Fix format routing issues that cause Opus files to be misidentified as MP3
  - _Requirements: 14.4, 14.5, 16.1, 16.2, 16.3, 16.4, 16.5, 17.1, 17.2, 17.3, 17.4, 17.5_

- [ ] 6. Optimize build system for conditional compilation
  - Update src/Makefile.am to conditionally include codec source files
  - Add conditional compilation variables for each codec's source files
  - Ensure disabled codec files are not compiled into the binary
  - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.5_

- [ ] 7. Improve configure.ac dependency checking
  - Replace combined PKG_CHECK_MODULES with individual dependency checks
  - Add codec dependency validation to prevent invalid combinations
  - Implement comprehensive build configuration reporting
  - _Requirements: 11.1, 11.2, 11.3, 11.4, 11.5, 12.1, 12.2, 12.3, 12.4, 12.5, 13.1, 13.2, 13.3, 13.4, 13.5_

- [ ] 8. Refactor codec implementations for conditional compilation
  - Refactor VorbisCodec and VorbisPassthroughCodec to handle conditional compilation
  - Refactor OpusCodec and OpusPassthroughCodec to handle conditional compilation  
  - Refactor FLACCodec to handle conditional compilation and ensure demuxer coupling
  - Update MP3 codec to work with registry system while maintaining legacy architecture
  - Add conditional compilation guards around codec class definitions
  - _Requirements: 1.1, 1.2, 5.1, 5.2, 5.3, 9.1, 9.3, 9.4, 9.5_

- [ ] 9. Update existing codec specs for registry support
  - Update FLAC codec spec to use registry system and ensure demuxer coupling
  - Update A-law/mu-law codec spec to use registry system
  - Update Vorbis codec spec to use registry system with conditional compilation
  - Update Opus codec spec to use registry system with conditional compilation
  - Update OggDemuxer spec to use registry system with conditional compilation
  - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5_

- [ ] 10. Add comprehensive error handling
  - Implement UnsupportedCodecException and UnsupportedFormatException classes
  - Add FormatDetectionException for format detection failures
  - Update MediaFactory error messages to be more descriptive
  - Add graceful degradation when codecs are unavailable
  - _Requirements: 2.5, 14.4, 16.4_
  - [x] 10.1. Update steering documents.
    - Make sure project steering documents include information about the conditional compilation refactor so that future codecs adhere to this architecture and design.
    - Ensure that in the future, all tasks must build cleanly before being considered complete. This is before `git commit` and `git ignore`.

- [ ] 11. Create comprehensive test suite
  - Write unit tests for CodecRegistry and DemuxerRegistry functionality
  - Create integration tests for MediaFactory with different codec combinations
  - Add tests for OggDemuxer conditional compilation scenarios
  - Test format detection accuracy and routing fixes
  - Test build configurations with various codec combinations disabled
  - Test refactored codec implementations with conditional compilation
  - _Requirements: All requirements validation_