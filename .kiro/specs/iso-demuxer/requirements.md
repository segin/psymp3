# ISO Demuxer Requirements

## Introduction

This specification defines the requirements for implementing an ISO Base Media File Format demuxer for PsyMP3. The ISO demuxer will handle MP4, M4A, MOV, and other ISO-based container formats, extracting audio streams and metadata for playback. This demuxer works with the Demuxer architecture and provides audio data to appropriate codecs including AAC, ALAC, mulaw, alaw, and PCM variants.

The implementation must support:
- **ISO Base Media File Format** (ISO/IEC 14496-12) compliance
- **MP4, M4A, MOV, and 3GP** container variants
- **Multiple audio codecs** including AAC, ALAC, mulaw, alaw, PCM
- **Fragmented MP4** support for streaming scenarios
- **Metadata extraction** from various atom types
- **Sample-accurate seeking** through sample table navigation
- **Efficient streaming** with progressive download support

## Requirements

### Requirement 1: ISO Container Parsing

**User Story:** As a media player, I want to parse ISO-based container files, so that I can extract audio streams from MP4, M4A, and MOV files.

#### Acceptance Criteria

1. WHEN opening ISO container files THEN the demuxer SHALL parse the file type box (ftyp) to identify container variant
2. WHEN processing movie box (moov) THEN the demuxer SHALL extract track information and sample tables
3. WHEN handling track boxes (trak) THEN the demuxer SHALL identify audio tracks and extract codec information
4. WHEN processing sample table boxes THEN the demuxer SHALL build sample-to-chunk and time-to-sample mappings
5. WHEN encountering media data boxes (mdat) THEN the demuxer SHALL locate and extract audio sample data
6. WHEN parsing handler reference boxes THEN the demuxer SHALL identify audio tracks by handler type 'soun'
7. WHEN processing sample description boxes THEN the demuxer SHALL extract codec-specific configuration data
8. WHEN handling nested boxes THEN the demuxer SHALL recursively parse container structures

### Requirement 2: Audio Codec Support

**User Story:** As an audio codec, I want to receive properly formatted audio data from ISO containers, so that I can decode various audio formats contained within.

#### Acceptance Criteria

1. WHEN encountering AAC audio THEN the demuxer SHALL extract AAC configuration from AudioSpecificConfig
2. WHEN processing ALAC audio THEN the demuxer SHALL provide ALAC magic cookie data to codec
3. WHEN handling mulaw audio THEN the demuxer SHALL configure 8kHz/16kHz sample rates and provide raw samples
4. WHEN processing alaw audio THEN the demuxer SHALL configure telephony sample rates and provide raw samples
5. WHEN encountering PCM audio THEN the demuxer SHALL extract bit depth, sample rate, and channel configuration
6. WHEN handling unknown codecs THEN the demuxer SHALL report unsupported format gracefully
7. WHEN processing codec configuration THEN the demuxer SHALL validate configuration data integrity
8. WHEN multiple audio tracks exist THEN the demuxer SHALL allow track selection

### Requirement 3: Fragmented MP4 Support

**User Story:** As a streaming media player, I want to play fragmented MP4 files, so that I can handle live streams and progressive downloads.

#### Acceptance Criteria

1. WHEN processing movie fragment boxes (moof) THEN the demuxer SHALL extract fragment-specific sample information
2. WHEN handling track fragment boxes (traf) THEN the demuxer SHALL update sample tables for current fragment
3. WHEN processing track run boxes (trun) THEN the demuxer SHALL extract sample durations and sizes
4. WHEN encountering fragment random access boxes THEN the demuxer SHALL enable seeking within fragments
5. WHEN handling segment index boxes (sidx) THEN the demuxer SHALL support segment-based navigation
6. WHEN processing live streams THEN the demuxer SHALL handle incomplete fragments gracefully
7. WHEN fragments arrive out of order THEN the demuxer SHALL buffer and reorder as needed
8. WHEN fragment headers are missing THEN the demuxer SHALL use default values from movie header

### Requirement 4: Metadata Extraction

**User Story:** As a media player, I want to display metadata from ISO container files, so that users can see track information, artwork, and other details.

#### Acceptance Criteria

1. WHEN processing user data boxes (udta) THEN the demuxer SHALL extract iTunes-style metadata
2. WHEN handling meta boxes THEN the demuxer SHALL parse item location and information tables
3. WHEN encountering copyright boxes THEN the demuxer SHALL extract copyright information
4. WHEN processing title boxes THEN the demuxer SHALL extract track and album titles
5. WHEN handling artwork data THEN the demuxer SHALL extract cover art images
6. WHEN processing genre information THEN the demuxer SHALL extract genre tags
7. WHEN encountering custom metadata THEN the demuxer SHALL preserve unknown metadata types
8. WHEN metadata encoding is specified THEN the demuxer SHALL handle UTF-8 and UTF-16 text

### Requirement 5: Sample Table Navigation

**User Story:** As a seeking mechanism, I want efficient sample table navigation, so that I can provide accurate seeking to any position in the audio stream.

#### Acceptance Criteria

1. WHEN building sample tables THEN the demuxer SHALL create efficient lookup structures
2. WHEN seeking to timestamp THEN the demuxer SHALL use time-to-sample table for accurate positioning
3. WHEN locating samples THEN the demuxer SHALL use sample-to-chunk table for data location
4. WHEN handling variable sample sizes THEN the demuxer SHALL use sample size table for precise extraction
5. WHEN processing chunk offsets THEN the demuxer SHALL use chunk offset table for file positioning
6. WHEN seeking to keyframes THEN the demuxer SHALL use sync sample table when available
7. WHEN handling edit lists THEN the demuxer SHALL apply timeline modifications correctly
8. WHEN sample tables are large THEN the demuxer SHALL optimize memory usage and lookup performance

### Requirement 6: Streaming and Progressive Download

**User Story:** As a streaming media player, I want to handle incomplete ISO files, so that I can start playback before the entire file is downloaded.

#### Acceptance Criteria

1. WHEN movie box is at file end THEN the demuxer SHALL handle progressive download scenarios
2. WHEN sample data is not yet available THEN the demuxer SHALL request specific byte ranges
3. WHEN handling incomplete files THEN the demuxer SHALL buffer samples as they become available
4. WHEN network interruptions occur THEN the demuxer SHALL resume from last known position
5. WHEN seeking in incomplete files THEN the demuxer SHALL determine if target data is available
6. WHEN handling fast-start files THEN the demuxer SHALL prioritize movie box parsing
7. WHEN processing streaming protocols THEN the demuxer SHALL work with HTTP range requests
8. WHEN bandwidth is limited THEN the demuxer SHALL minimize redundant data requests

### Requirement 7: Error Handling and Recovery

**User Story:** As a media player, I want robust error handling for corrupted ISO files, so that playback continues when possible or fails gracefully.

#### Acceptance Criteria

1. WHEN encountering corrupted boxes THEN the demuxer SHALL skip damaged sections and continue
2. WHEN box sizes are invalid THEN the demuxer SHALL validate and handle size mismatches
3. WHEN sample tables are inconsistent THEN the demuxer SHALL use available data and report warnings
4. WHEN codec configuration is missing THEN the demuxer SHALL attempt to infer from sample data
5. WHEN file structure is damaged THEN the demuxer SHALL recover usable portions when possible
6. WHEN memory allocation fails THEN the demuxer SHALL handle resource constraints gracefully
7. WHEN I/O errors occur THEN the demuxer SHALL retry operations with exponential backoff
8. WHEN unrecoverable errors occur THEN the demuxer SHALL report clear error messages

### Requirement 8: Performance and Memory Management

**User Story:** As a media player, I want efficient ISO demuxing that doesn't impact system performance, so that large files and multiple streams can be handled smoothly.

#### Acceptance Criteria

1. WHEN processing large files THEN the demuxer SHALL minimize memory footprint for sample tables
2. WHEN handling multiple tracks THEN the demuxer SHALL efficiently manage per-track state
3. WHEN seeking frequently THEN the demuxer SHALL cache lookup structures for performance
4. WHEN processing sample data THEN the demuxer SHALL minimize data copying and allocation
5. WHEN handling fragmented files THEN the demuxer SHALL efficiently manage fragment metadata
6. WHEN parsing nested boxes THEN the demuxer SHALL use iterative parsing to prevent stack overflow
7. WHEN processing metadata THEN the demuxer SHALL lazy-load non-essential information
8. WHEN memory pressure occurs THEN the demuxer SHALL release cached data intelligently

### Requirement 9: Container Format Variants

**User Story:** As a media player, I want to support various ISO-based container formats, so that I can play files from different sources and applications.

#### Acceptance Criteria

1. WHEN processing MP4 files THEN the demuxer SHALL handle standard MP4 container structure
2. WHEN handling M4A files THEN the demuxer SHALL process audio-only MP4 variants
3. WHEN processing MOV files THEN the demuxer SHALL handle QuickTime-specific extensions
4. WHEN encountering 3GP files THEN the demuxer SHALL support mobile container variants
5. WHEN handling F4A files THEN the demuxer SHALL process Flash audio container format
6. WHEN processing brand-specific features THEN the demuxer SHALL adapt to container requirements
7. WHEN encountering unknown brands THEN the demuxer SHALL attempt generic ISO parsing
8. WHEN handling legacy formats THEN the demuxer SHALL support backward compatibility

### Requirement 10: Integration with Demuxer Architecture

**User Story:** As a PsyMP3 component, I want the ISO demuxer to integrate seamlessly with the demuxer architecture, so that it works consistently with other demuxers.

#### Acceptance Criteria

1. WHEN implementing Demuxer interface THEN ISODemuxer SHALL provide all required virtual methods
2. WHEN initializing THEN the demuxer SHALL configure itself from IOHandler and file information
3. WHEN extracting streams THEN the demuxer SHALL populate StreamInfo with codec and format details
4. WHEN providing samples THEN the demuxer SHALL output MediaChunk data for codec consumption
5. WHEN seeking THEN the demuxer SHALL update internal state and provide accurate positioning
6. WHEN handling metadata THEN the demuxer SHALL populate metadata structures for UI display
7. WHEN reporting capabilities THEN the demuxer SHALL indicate support for ISO-based formats
8. WHEN errors occur THEN the demuxer SHALL use PsyMP3's error reporting mechanisms

### Requirement 11: Telephony Audio Support

**User Story:** As a telephony application, I want to play mulaw and alaw audio from ISO containers, so that I can handle voice recordings and communications data.

#### Acceptance Criteria

1. WHEN processing mulaw samples THEN the demuxer SHALL configure 8kHz sample rate by default
2. WHEN handling alaw samples THEN the demuxer SHALL support European telephony standards
3. WHEN extracting telephony audio THEN the demuxer SHALL provide raw sample data to codecs
4. WHEN processing sample rates THEN the demuxer SHALL support 8kHz, 16kHz, and other telephony rates
5. WHEN handling channel configuration THEN the demuxer SHALL support mono telephony audio
6. WHEN processing sample sizes THEN the demuxer SHALL handle 8-bit companded audio samples
7. WHEN extracting timing information THEN the demuxer SHALL provide accurate sample timing
8. WHEN handling codec configuration THEN the demuxer SHALL pass minimal configuration for raw formats

### Requirement 12: Quality and Standards Compliance

**User Story:** As a standards-compliant media player, I want accurate ISO format parsing, so that files from various sources play correctly.

#### Acceptance Criteria

1. WHEN parsing box structures THEN the demuxer SHALL follow ISO/IEC 14496-12 specifications
2. WHEN handling atom types THEN the demuxer SHALL support both 32-bit and 64-bit sizes
3. WHEN processing timestamps THEN the demuxer SHALL handle various timescale configurations
4. WHEN extracting durations THEN the demuxer SHALL provide accurate timing information
5. WHEN handling sample descriptions THEN the demuxer SHALL validate codec-specific data
6. WHEN processing edit lists THEN the demuxer SHALL apply timeline modifications per specification
7. WHEN handling track references THEN the demuxer SHALL maintain proper track relationships
8. WHEN validating structures THEN the demuxer SHALL ensure data integrity and consistency