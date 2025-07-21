# Requirements Document

## Introduction

This feature implements conditional optional codec support for PsyMP3, allowing the application to be built with or without specific audio codecs while maintaining proper functionality. The system will use a registry-based architecture where codecs and demuxers register themselves at startup, eliminating the need for conditional compilation checks throughout the codebase except in the registration functions.

## Requirements

### Requirement 1

**User Story:** As a developer, I want to conditionally compile codec support so that I can create builds with only the codecs I need.

#### Acceptance Criteria

1. WHEN a codec is disabled at compile time THEN the codec SHALL NOT be included in the final binary
2. WHEN a codec is enabled at compile time THEN the codec SHALL register itself with the codec registry at startup
3. IF Vorbis codec is disabled AND Opus codec is disabled THEN OggDemuxer SHALL NOT be compiled unless FLAC support is explicitly requested for Ogg containers
4. WHEN FLAC codec is disabled THEN OggDemuxer SHALL NOT support FLAC streams
5. IF all Ogg-compatible codecs are disabled THEN OggDemuxer SHALL NOT be compiled at all

### Requirement 2

**User Story:** As a developer, I want a registry-based architecture so that the MediaFactory doesn't need conditional compilation logic.

#### Acceptance Criteria

1. WHEN the application starts THEN it SHALL call a codec registration function that registers all available codecs
2. WHEN the application starts THEN it SHALL call a demuxer registration function that registers all available demuxers
3. WHEN MediaFactory needs to create a codec THEN it SHALL query the codec registry instead of using conditional compilation
4. WHEN MediaFactory needs to create a demuxer THEN it SHALL query the demuxer registry instead of using conditional compilation
5. IF a codec is not registered THEN MediaFactory SHALL gracefully handle the missing codec

### Requirement 3

**User Story:** As a user, I want proper codec dependency handling so that demuxers only support the codecs that are available.

#### Acceptance Criteria

1. WHEN OggDemuxer is available AND Vorbis is available AND Opus is not available THEN OggDemuxer SHALL only handle Vorbis streams and reject Opus streams
2. WHEN OggDemuxer is available AND Opus is available AND Vorbis is not available THEN OggDemuxer SHALL only handle Opus streams and reject Vorbis streams
3. WHEN both Vorbis and Opus are available THEN OggDemuxer SHALL handle both stream types
4. WHEN FLAC is available THEN it SHALL be supported both in native FLAC containers and Ogg containers (if Ogg is enabled)
5. WHEN FLAC is disabled THEN Ogg containers SHALL NOT support FLAC streams

### Requirement 4

**User Story:** As a developer, I want MP3 codec to remain in the legacy architecture so that existing functionality is preserved.

#### Acceptance Criteria

1. WHEN implementing the new registry system THEN MP3 codec SHALL remain using the legacy stream architecture
2. WHEN MP3 codec is enabled THEN it SHALL NOT use the demux stream architecture
3. WHEN MP3 codec is disabled THEN legacy MP3 functionality SHALL be conditionally compiled out

### Requirement 5

**User Story:** As a developer, I want proper FLAC codec and demuxer coupling so that FLAC is always fully supported when enabled.

#### Acceptance Criteria

1. WHEN FLAC is enabled THEN both FLAC codec and FLAC demuxer SHALL be compiled
2. WHEN FLAC is enabled THEN it SHALL be supported in both native FLAC containers and Ogg containers (if Ogg is enabled)
3. IF FLAC codec is available THEN FLAC demuxer SHALL always be available
4. WHEN FLAC is disabled THEN neither FLAC codec nor FLAC demuxer SHALL be compiled
5. WHEN building the system THEN a configuration where FLAC codec exists without FLAC demuxer SHALL be prevented

### Requirement 6

**User Story:** As a developer, I want explicit conditional compilation in OggDemuxer so that unsupported codecs are properly excluded.

#### Acceptance Criteria

1. WHEN Vorbis codec is disabled THEN OggDemuxer SHALL have `#ifdef` blocks to remove Vorbis support code
2. WHEN Opus codec is disabled THEN OggDemuxer SHALL have `#ifdef` blocks to remove Opus support code  
3. WHEN FLAC codec is disabled THEN OggDemuxer SHALL have `#ifdef` blocks to remove FLAC support code
4. WHEN OggDemuxer encounters a disabled codec stream THEN it SHALL reject the stream at compile time, not runtime
5. WHEN all Ogg-compatible codecs are disabled THEN OggDemuxer SHALL not be compiled at all

### Requirement 7

**User Story:** As a developer, I want future-proof Ogg container support so that the system can handle codec changes.

#### Acceptance Criteria

1. WHEN Vorbis or Opus are decoupled from Ogg containers in the future THEN the system SHALL handle this gracefully
2. WHEN OggDemuxer is implemented THEN it SHALL only support the four known Ogg audio codecs (Vorbis, Opus, FLAC, and potentially Speex)
3. WHEN adding new Ogg codec support THEN OggDemuxer SHALL require explicit code changes for each new codec
4. WHEN implementing codec support THEN automatic codec discovery SHALL NOT be used for Ogg containers

### Requirement 8

**User Story:** As a developer, I want centralized conditional compilation so that codec availability checks are in one place.

#### Acceptance Criteria

1. WHEN implementing conditional compilation THEN codec availability checks SHALL only exist in registration functions
2. WHEN a codec is conditionally compiled THEN the conditional logic SHALL be isolated to the codec registration code
3. WHEN MediaFactory or other components need codec information THEN they SHALL query registries instead of using preprocessor conditionals

### Requirement 9

**User Story:** As a developer, I want codec implementations refactored for proper conditional compilation so that disabled codecs are completely excluded from builds.

#### Acceptance Criteria

1. WHEN a codec is disabled THEN its class definitions SHALL be conditionally compiled out
2. WHEN Vorbis is disabled THEN VorbisCodec and VorbisPassthroughCodec classes SHALL NOT be compiled
3. WHEN Opus is disabled THEN OpusCodec and OpusPassthroughCodec classes SHALL NOT be compiled
4. WHEN FLAC is disabled THEN FLACCodec class SHALL NOT be compiled
5. WHEN refactoring codecs THEN conditional compilation guards SHALL be added around all codec-specific code

### Requirement 18

**User Story:** As a developer, I want existing codec specs updated for the new architecture so that all codecs work with the registry system.

#### Acceptance Criteria

1. WHEN implementing the registry system THEN FLAC codec spec SHALL be updated for conditional compilation and registry support
2. WHEN implementing the registry system THEN A-law/mu-law codec spec SHALL be updated for registry support (but not conditional compilation)
3. WHEN implementing the registry system THEN OggDemuxer spec SHALL be updated for conditional compilation and registry support
4. WHEN implementing the registry system THEN Vorbis codec spec SHALL be updated for conditional compilation and registry support
5. WHEN implementing the registry system THEN Opus codec spec SHALL be updated for conditional compilation and registry support

### Requirement 10

**User Story:** As a developer, I want optimized build system so that disabled codec files are not compiled at all.

#### Acceptance Criteria

1. WHEN a codec is disabled THEN its source files SHALL NOT be compiled into the binary
2. WHEN implementing conditional compilation THEN configure.ac SHALL be updated to exclude disabled codec source files
3. WHEN implementing conditional compilation THEN src/Makefile.am SHALL be updated to conditionally include source files based on enabled codecs
4. WHEN a codec is disabled THEN no object files SHALL be generated for that codec's source files
5. WHEN building the project THEN only enabled codec source files SHALL be passed to the compiler

### Requirement 11

**User Story:** As a developer, I want improved dependency checking so that I know exactly which required library is missing.

#### Acceptance Criteria

1. WHEN configure script runs THEN each core dependency SHALL be checked individually
2. WHEN a core dependency is missing THEN the error message SHALL clearly identify which specific library is not found
3. WHEN multiple dependencies are missing THEN each missing dependency SHALL be reported separately
4. WHEN configure script fails THEN it SHALL NOT use combined dependency checks that obscure which library is missing
5. IF SDL is missing THEN the error SHALL specifically mention SDL and its version requirement

### Requirement 12

**User Story:** As a developer, I want proper codec dependency validation so that invalid codec combinations are prevented at build time.

#### Acceptance Criteria

1. WHEN FLAC codec is enabled THEN FLAC demuxer SHALL automatically be enabled
2. WHEN no Ogg-compatible codecs are enabled THEN OggDemuxer SHALL NOT be built
3. WHEN Vorbis or Opus is enabled without libogg THEN configure SHALL fail with a clear error message
4. WHEN FLAC is enabled for Ogg support without libogg THEN configure SHALL fail with a clear error message
5. WHEN configure runs THEN it SHALL validate all codec dependency combinations before proceeding

### Requirement 13

**User Story:** As a developer, I want comprehensive build configuration reporting so that I can verify which features are enabled.

#### Acceptance Criteria

1. WHEN configure completes successfully THEN it SHALL display a detailed summary of enabled/disabled codecs
2. WHEN configure completes THEN it SHALL show which demuxers will be built
3. WHEN configure completes THEN it SHALL indicate which codec combinations are active
4. WHEN a codec is disabled due to missing dependencies THEN the summary SHALL explain why it was disabled
5. WHEN OggDemuxer is built THEN the summary SHALL list which Ogg codecs it will support

### Requirement 14

**User Story:** As a user, I want proper file format detection so that audio files are routed to the correct codec and don't fail with wrong codec errors.

#### Acceptance Criteria

1. WHEN an Opus file is played THEN it SHALL be routed to OggDemuxer and NOT to MP3 codec
2. WHEN MediaFactory detects file format THEN it SHALL use both magic bytes and file extension for accurate detection
3. WHEN format detection is ambiguous THEN the system SHALL prioritize the most specific format match
4. WHEN a file has incorrect extension THEN magic byte detection SHALL override extension-based detection
5. WHEN registry system is implemented THEN file format routing SHALL be more accurate than current implementation

### Requirement 15

**User Story:** As a developer, I want standardized file extension mapping so that each container format uses the correct extensions.

#### Acceptance Criteria

1. WHEN Ogg container format is detected THEN it SHALL use extensions `.ogg`, `.oga`, and `.opus`
2. WHEN ISO container format is detected THEN it SHALL use extensions `.mov`, `.mp4`, `.m4a`, `.3gp`
3. WHEN FLAC format is detected THEN it SHALL use extension `.flac`
4. WHEN MP3 elementary stream is detected THEN it SHALL use extensions `.mp3`, `.mpa`, `.mpga`, `.bit`, `.mp2`, `.m2a`, `.mp2a`
5. WHEN MediaFactory registers formats THEN extension mappings SHALL follow these standardized assignments

### Requirement 16

**User Story:** As a user, I want reliable format detection priority so that files are correctly identified even with ambiguous signatures.

#### Acceptance Criteria

1. WHEN multiple format signatures match THEN the system SHALL use priority-based resolution
2. WHEN Ogg signature is detected THEN the system SHALL probe for specific codec signatures within the container
3. WHEN file extension conflicts with magic bytes THEN magic bytes SHALL take precedence
4. WHEN format detection fails THEN the system SHALL provide clear error messages indicating why detection failed
5. WHEN registry system is active THEN format detection SHALL be deterministic and repeatable

### Requirement 17

**User Story:** As a developer, I want comprehensive MIME type support so that web-served files with incorrect extensions are properly handled.

#### Acceptance Criteria

1. WHEN Ogg container is served THEN it SHALL recognize MIME types `application/ogg`, `audio/ogg`, `audio/opus`
2. WHEN ISO container is served THEN it SHALL recognize MIME types `audio/mp4`, `audio/m4a`, `video/mp4`, `video/quicktime`
3. WHEN FLAC is served THEN it SHALL recognize MIME types `audio/flac`, `audio/x-flac`
4. WHEN MP3 is served THEN it SHALL recognize MIME types `audio/mpeg`, `audio/mp3`, `audio/x-mp3`
5. WHEN web server provides incorrect file extension THEN MIME type detection SHALL override extension-based detection