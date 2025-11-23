# ISO Demuxer Requirements

## Introduction

This specification defines the requirements for implementing an ISO Base Media File Format demuxer for PsyMP3. The ISO demuxer will handle MP4, M4A, MOV, and other ISO-based container formats, extracting audio streams and metadata for playback. This demuxer works with the Demuxer architecture and provides audio data to appropriate codecs including AAC, ALAC, mulaw, alaw, and PCM variants.

The implementation must support:
- **ISO Base Media File Format** (ISO/IEC 14496-12) compliance
- **MP4 File Format** (ISO/IEC 14496-14) compliance
- **AVC File Format** (ISO/IEC 14496-15) for H.264/AVC video
- **MIME Type Registration** (RFC 4337) for proper content type identification
- **MP4, M4A, MOV, and 3GP** container variants
- **Multiple audio codecs** including AAC, ALAC, mulaw, alaw, PCM
- **H.264/AVC video codec** support for video-enabled containers
- **Fragmented MP4** support for streaming scenarios
- **Metadata extraction** from various atom types
- **Sample-accurate seeking** through sample table navigation
- **Efficient streaming** with progressive download support

## Reference Documents

- **ISO/IEC 14496-12:2015**: ISO Base Media File Format specification (stored in `docs/ISO_IEC_14496-12_2015.pdf` and `docs/ISO_IEC_14496-12_2015.txt`)
- **ISO/IEC 14496-14**: MP4 File Format specification
- **ISO/IEC 14496-15:2010**: AVC File Format specification (stored in `docs/ISO_IEC_14496-15-AVC-format-2012.pdf` and `docs/ISO_IEC_14496-15-AVC-format-2012.txt`)
- **RFC 4337**: MIME Type Registration for MPEG-4 (stored in `docs/rfc4337.txt`)

## Glossary

- **ISODemuxer**: The ISO Base Media File Format demuxer component that parses ISO-based container files
- **ISO Container**: A file conforming to ISO/IEC 14496-12 Base Media File Format specification
- **Box**: A structural element in ISO containers consisting of size, type, and data fields
- **ftyp Box**: File type box that identifies the container variant and compatibility brands
- **moov Box**: Movie box containing track information and sample tables
- **mdat Box**: Media data box containing actual audio and video sample data
- **trak Box**: Track box containing information about a single media track
- **Sample Table**: A collection of boxes (stts, stsc, stsz, stco) that map samples to file locations and timestamps
- **Fragment**: A self-contained portion of a fragmented MP4 file with its own sample information
- **moof Box**: Movie fragment box containing fragment-specific sample information
- **AudioSpecificConfig**: AAC codec configuration data structure
- **Magic Cookie**: ALAC codec-specific configuration data
- **Companded Audio**: Audio compressed using logarithmic encoding (mulaw/alaw)
- **Sync Sample**: A keyframe or independently decodable sample
- **Edit List**: Timeline modification instructions for track playback
- **Progressive Download**: Playback of a file while it is still being downloaded
- **Byte Range**: A specific range of bytes within a file, used for HTTP range requests
- **IOHandler**: PsyMP3 component that provides abstracted I/O operations
- **MediaChunk**: PsyMP3 data structure containing decoded audio samples with timing information
- **StreamInfo**: PsyMP3 data structure describing codec and format details of an audio stream
- **AVC**: Advanced Video Coding, also known as H.264 (ISO/IEC 14496-10)
- **avcC Box**: AVC configuration box containing H.264 decoder configuration record
- **NAL Unit**: Network Abstraction Layer unit, the basic data structure in H.264/AVC
- **SPS**: Sequence Parameter Set, H.264 configuration data for video sequence
- **PPS**: Picture Parameter Set, H.264 configuration data for picture decoding
- **Brand**: Four-character code in ftyp box identifying file format variant (e.g., 'isom', 'mp41', 'mp42', 'avc1')
- **Compatible Brand**: Additional brands listed in ftyp box indicating format compatibility
- **MIME Type**: Media type identifier for HTTP content negotiation (e.g., video/mp4, audio/mp4, application/mp4)
- **stsd Box**: Sample Description Box containing codec-specific configuration
- **stts Box**: Time-to-Sample Box mapping sample numbers to time
- **stsc Box**: Sample-to-Chunk Box mapping samples to file chunks
- **stsz Box**: Sample Size Box containing size of each sample
- **stco/co64 Box**: Chunk Offset Box containing file offsets for each chunk
- **stss Box**: Sync Sample Box listing keyframe positions
- **ctts Box**: Composition Time to Sample Box for decode/presentation time offsets
- **mvhd Box**: Movie Header Box with timescale and duration
- **tkhd Box**: Track Header Box with track-specific information
- **mdhd Box**: Media Header Box with media timescale
- **hdlr Box**: Handler Reference Box identifying track type ('soun' for audio, 'vide' for video)
- **minf Box**: Media Information Box containing sample table
- **dinf Box**: Data Information Box with data references
- **dref Box**: Data Reference Box listing data sources
- **edts Box**: Edit Box containing edit list
- **elst Box**: Edit List Box with timeline modifications
- **mvex Box**: Movie Extends Box for fragmented files
- **mehd Box**: Movie Extends Header Box with fragment duration
- **trex Box**: Track Extends Box with default fragment values
- **traf Box**: Track Fragment Box within movie fragment
- **tfhd Box**: Track Fragment Header Box with fragment defaults
- **trun Box**: Track Run Box with sample information in fragment
- **mfra Box**: Movie Fragment Random Access Box for seeking in fragments
- **tfra Box**: Track Fragment Random Access Box with fragment positions
- **sdtp Box**: Sample Dependency Type Box indicating sample dependencies
- **udta Box**: User Data Box containing metadata
- **meta Box**: Metadata Box with structured metadata
- **Timescale**: Number of time units per second for timing calculations
- **Duration**: Length of presentation in timescale units
- **Sample**: Single unit of media data (audio frame, video frame, etc.)
- **Chunk**: Contiguous set of samples stored together in file
- **Data Reference Index**: Index into data reference table indicating sample location
- **NAL Unit**: Network Abstraction Layer unit, the basic data structure in H.264/AVC
- **SPS**: Sequence Parameter Set, H.264 configuration data for video sequence
- **PPS**: Picture Parameter Set, H.264 configuration data for picture decoding
- **Brand**: Four-character code in ftyp box identifying file format variant (e.g., 'isom', 'mp41', 'mp42', 'avc1')
- **Compatible Brand**: Additional brands listed in ftyp box indicating format compatibility
- **MIME Type**: Media type identifier for HTTP content negotiation (e.g., video/mp4, audio/mp4, application/mp4)

## Requirements

### Requirement 1: ISO Container Parsing

**User Story:** As a media player, I want to parse ISO-based container files, so that I can extract audio streams from MP4, M4A, and MOV files.

#### Acceptance Criteria

1. WHEN opening ISO container files THEN ISODemuxer SHALL parse the file type box (ftyp) to identify container variant
2. WHEN processing movie box (moov) THEN ISODemuxer SHALL extract track information and sample tables
3. WHEN handling track boxes (trak) THEN ISODemuxer SHALL identify audio tracks and extract codec information
4. WHEN processing sample table boxes THEN ISODemuxer SHALL build sample-to-chunk and time-to-sample mappings
5. WHEN encountering media data boxes (mdat) THEN ISODemuxer SHALL locate and extract audio sample data
6. WHEN parsing handler reference boxes THEN ISODemuxer SHALL identify audio tracks by handler type 'soun'
7. WHEN processing sample description boxes THEN ISODemuxer SHALL extract codec-specific configuration data
8. WHEN handling nested boxes THEN ISODemuxer SHALL recursively parse container structures

### Requirement 2: Audio Codec Support

**User Story:** As an audio codec, I want to receive properly formatted audio data from ISO containers, so that I can decode various audio formats contained within.

#### Acceptance Criteria

1. WHEN encountering AAC audio THEN ISODemuxer SHALL extract AAC configuration from AudioSpecificConfig
2. WHEN processing ALAC audio THEN ISODemuxer SHALL provide ALAC magic cookie data to codec
3. WHEN handling mulaw audio THEN ISODemuxer SHALL configure 8kHz or 16kHz sample rates and provide raw samples
4. WHEN processing alaw audio THEN ISODemuxer SHALL configure telephony sample rates and provide raw samples
5. WHEN encountering PCM audio THEN ISODemuxer SHALL extract bit depth, sample rate, and channel configuration
6. WHEN handling unknown codecs THEN ISODemuxer SHALL report unsupported format error
7. WHEN processing codec configuration THEN ISODemuxer SHALL validate configuration data integrity
8. WHEN multiple audio tracks exist THEN ISODemuxer SHALL allow track selection

### Requirement 3: Fragmented MP4 Support

**User Story:** As a streaming media player, I want to play fragmented MP4 files, so that I can handle live streams and progressive downloads.

#### Acceptance Criteria

1. WHEN processing movie fragment boxes (moof) THEN ISODemuxer SHALL extract fragment-specific sample information
2. WHEN handling track fragment boxes (traf) THEN ISODemuxer SHALL update sample tables for current fragment
3. WHEN processing track run boxes (trun) THEN ISODemuxer SHALL extract sample durations and sizes
4. WHEN encountering fragment random access boxes THEN ISODemuxer SHALL enable seeking within fragments
5. WHEN handling segment index boxes (sidx) THEN ISODemuxer SHALL support segment-based navigation
6. WHEN processing live streams THEN ISODemuxer SHALL handle incomplete fragments without errors
7. WHEN fragments arrive out of order THEN ISODemuxer SHALL buffer and reorder fragments
8. WHEN fragment headers are missing THEN ISODemuxer SHALL use default values from movie header

### Requirement 4: Metadata Extraction

**User Story:** As a media player, I want to display metadata from ISO container files, so that users can see track information, artwork, and other details.

#### Acceptance Criteria

1. WHEN processing user data boxes (udta) THEN ISODemuxer SHALL extract iTunes-style metadata
2. WHEN handling meta boxes THEN ISODemuxer SHALL parse item location and information tables
3. WHEN encountering copyright boxes THEN ISODemuxer SHALL extract copyright information
4. WHEN processing title boxes THEN ISODemuxer SHALL extract track and album titles
5. WHEN handling artwork data THEN ISODemuxer SHALL extract cover art images
6. WHEN processing genre information THEN ISODemuxer SHALL extract genre tags
7. WHEN encountering custom metadata THEN ISODemuxer SHALL preserve unknown metadata types
8. WHEN metadata encoding is specified THEN ISODemuxer SHALL handle UTF-8 and UTF-16 text

### Requirement 5: Sample Table Navigation

**User Story:** As a seeking mechanism, I want efficient sample table navigation, so that I can provide accurate seeking to any position in the audio stream.

#### Acceptance Criteria

1. WHEN building sample tables THEN ISODemuxer SHALL create efficient lookup structures
2. WHEN seeking to timestamp THEN ISODemuxer SHALL use time-to-sample table for accurate positioning
3. WHEN locating samples THEN ISODemuxer SHALL use sample-to-chunk table for data location
4. WHEN handling variable sample sizes THEN ISODemuxer SHALL use sample size table for precise extraction
5. WHEN processing chunk offsets THEN ISODemuxer SHALL use chunk offset table for file positioning
6. WHEN seeking to keyframes THEN ISODemuxer SHALL use sync sample table when available
7. WHEN handling edit lists THEN ISODemuxer SHALL apply timeline modifications correctly
8. WHEN sample tables are large THEN ISODemuxer SHALL optimize memory usage and lookup performance

### Requirement 6: Streaming and Progressive Download

**User Story:** As a streaming media player, I want to handle incomplete ISO files, so that I can start playback before the entire file is downloaded.

#### Acceptance Criteria

1. WHEN movie box is at file end THEN ISODemuxer SHALL handle progressive download scenarios
2. WHEN sample data is not yet available THEN ISODemuxer SHALL request specific byte ranges
3. WHEN handling incomplete files THEN ISODemuxer SHALL buffer samples as they become available
4. WHEN network interruptions occur THEN ISODemuxer SHALL resume from last known position
5. WHEN seeking in incomplete files THEN ISODemuxer SHALL determine if target data is available
6. WHEN handling fast-start files THEN ISODemuxer SHALL prioritize movie box parsing
7. WHEN processing streaming protocols THEN ISODemuxer SHALL work with HTTP range requests
8. WHEN bandwidth is limited THEN ISODemuxer SHALL minimize redundant data requests

### Requirement 7: Error Handling and Recovery

**User Story:** As a media player, I want robust error handling for corrupted ISO files, so that playback continues when possible or fails gracefully.

#### Acceptance Criteria

1. WHEN encountering corrupted boxes THEN ISODemuxer SHALL skip damaged sections and continue parsing
2. WHEN box sizes are invalid THEN ISODemuxer SHALL validate and handle size mismatches
3. WHEN sample tables are inconsistent THEN ISODemuxer SHALL use available data and report warnings
4. WHEN codec configuration is missing THEN ISODemuxer SHALL attempt to infer configuration from sample data
5. WHEN file structure is damaged THEN ISODemuxer SHALL recover usable portions when possible
6. WHEN memory allocation fails THEN ISODemuxer SHALL handle resource constraints without crashing
7. WHEN I/O errors occur THEN ISODemuxer SHALL retry operations with exponential backoff
8. WHEN unrecoverable errors occur THEN ISODemuxer SHALL report clear error messages

### Requirement 8: Performance and Memory Management

**User Story:** As a media player, I want efficient ISO demuxing that doesn't impact system performance, so that large files and multiple streams can be handled smoothly.

#### Acceptance Criteria

1. WHEN processing large files THEN ISODemuxer SHALL minimize memory footprint for sample tables
2. WHEN handling multiple tracks THEN ISODemuxer SHALL efficiently manage per-track state
3. WHEN seeking frequently THEN ISODemuxer SHALL cache lookup structures for performance
4. WHEN processing sample data THEN ISODemuxer SHALL minimize data copying and allocation
5. WHEN handling fragmented files THEN ISODemuxer SHALL efficiently manage fragment metadata
6. WHEN parsing nested boxes THEN ISODemuxer SHALL use iterative parsing to prevent stack overflow
7. WHEN processing metadata THEN ISODemuxer SHALL lazy-load non-essential information
8. WHEN memory pressure occurs THEN ISODemuxer SHALL release cached data intelligently

### Requirement 9: Container Format Variants

**User Story:** As a media player, I want to support various ISO-based container formats, so that I can play files from different sources and applications.

#### Acceptance Criteria

1. WHEN processing MP4 files THEN ISODemuxer SHALL handle standard MP4 container structure
2. WHEN handling M4A files THEN ISODemuxer SHALL process audio-only MP4 variants
3. WHEN processing MOV files THEN ISODemuxer SHALL handle QuickTime-specific extensions
4. WHEN encountering 3GP files THEN ISODemuxer SHALL support mobile container variants
5. WHEN handling F4A files THEN ISODemuxer SHALL process Flash audio container format
6. WHEN processing brand-specific features THEN ISODemuxer SHALL adapt to container requirements
7. WHEN encountering unknown brands THEN ISODemuxer SHALL attempt generic ISO parsing
8. WHEN handling legacy formats THEN ISODemuxer SHALL support backward compatibility

### Requirement 10: Integration with Demuxer Architecture

**User Story:** As a PsyMP3 component, I want the ISO demuxer to integrate seamlessly with the demuxer architecture, so that it works consistently with other demuxers.

#### Acceptance Criteria

1. WHEN implementing Demuxer interface THEN ISODemuxer SHALL provide all required virtual methods
2. WHEN initializing THEN ISODemuxer SHALL configure itself from IOHandler and file information
3. WHEN extracting streams THEN ISODemuxer SHALL populate StreamInfo with codec and format details
4. WHEN providing samples THEN ISODemuxer SHALL output MediaChunk data for codec consumption
5. WHEN seeking THEN ISODemuxer SHALL update internal state and provide accurate positioning
6. WHEN handling metadata THEN ISODemuxer SHALL populate metadata structures for UI display
7. WHEN reporting capabilities THEN ISODemuxer SHALL indicate support for ISO-based formats
8. WHEN errors occur THEN ISODemuxer SHALL use PsyMP3 error reporting mechanisms

### Requirement 11: Telephony Audio Support

**User Story:** As a telephony application, I want to play mulaw and alaw audio from ISO containers, so that I can handle voice recordings and communications data.

#### Acceptance Criteria

1. WHEN processing mulaw samples THEN ISODemuxer SHALL configure 8kHz sample rate by default
2. WHEN handling alaw samples THEN ISODemuxer SHALL support European telephony standards
3. WHEN extracting telephony audio THEN ISODemuxer SHALL provide raw sample data to codecs
4. WHEN processing sample rates THEN ISODemuxer SHALL support 8kHz, 16kHz, and other telephony rates
5. WHEN handling channel configuration THEN ISODemuxer SHALL support mono telephony audio
6. WHEN processing sample sizes THEN ISODemuxer SHALL handle 8-bit companded audio samples
7. WHEN extracting timing information THEN ISODemuxer SHALL provide accurate sample timing
8. WHEN handling codec configuration THEN ISODemuxer SHALL pass minimal configuration for raw formats

### Requirement 12: Quality and Standards Compliance

**User Story:** As a standards-compliant media player, I want accurate ISO format parsing, so that files from various sources play correctly.

#### Acceptance Criteria

1. WHEN parsing box structures THEN ISODemuxer SHALL follow ISO/IEC 14496-12 specifications
2. WHEN handling atom types THEN ISODemuxer SHALL support both 32-bit and 64-bit sizes
3. WHEN processing timestamps THEN ISODemuxer SHALL handle various timescale configurations
4. WHEN extracting durations THEN ISODemuxer SHALL provide accurate timing information
5. WHEN handling sample descriptions THEN ISODemuxer SHALL validate codec-specific data
6. WHEN processing edit lists THEN ISODemuxer SHALL apply timeline modifications per specification
7. WHEN handling track references THEN ISODemuxer SHALL maintain proper track relationships
8. WHEN validating structures THEN ISODemuxer SHALL ensure data integrity and consistency

### Requirement 13: MIME Type Identification and Content Negotiation

**User Story:** As a web server or HTTP client, I want to identify MP4 file content types correctly, so that I can serve and request files with appropriate MIME types per RFC 4337.

#### Acceptance Criteria

1. WHEN file contains video and audio THEN ISODemuxer SHALL identify content as video/mp4 MIME type
2. WHEN file contains audio only THEN ISODemuxer SHALL identify content as audio/mp4 MIME type
3. WHEN file contains neither video nor audio THEN ISODemuxer SHALL identify content as application/mp4 MIME type
4. WHEN processing ftyp box THEN ISODemuxer SHALL extract major brand and compatible brands
5. WHEN identifying file extensions THEN ISODemuxer SHALL recognize mp4, mpg4, m4a, and m4v extensions
6. WHEN handling brand-specific content THEN ISODemuxer SHALL determine appropriate MIME type based on track types
7. WHEN serving files over HTTP THEN ISODemuxer SHALL provide MIME type information for content negotiation
8. WHEN processing MPEG-J or MPEG-7 content THEN ISODemuxer SHALL identify as application/mp4 if no audio or video present

### Requirement 14: AVC/H.264 Video Codec Support

**User Story:** As a video codec, I want to receive properly formatted H.264/AVC video data from ISO containers, so that I can decode video streams per ISO/IEC 14496-15.

#### Acceptance Criteria

1. WHEN encountering AVC video track THEN ISODemuxer SHALL parse avcC box for decoder configuration
2. WHEN processing avcC box THEN ISODemuxer SHALL extract SPS and PPS NAL units
3. WHEN handling AVC samples THEN ISODemuxer SHALL convert NAL unit length fields to start codes
4. WHEN processing NAL unit length size THEN ISODemuxer SHALL support 1, 2, and 4-byte length fields
5. WHEN extracting AVC configuration THEN ISODemuxer SHALL provide profile, level, and constraint flags
6. WHEN handling parameter sets THEN ISODemuxer SHALL pass SPS and PPS to video decoder
7. WHEN processing AVC samples THEN ISODemuxer SHALL maintain NAL unit boundaries
8. WHEN encountering multiple parameter sets THEN ISODemuxer SHALL handle all SPS and PPS entries

### Requirement 15: Brand and Compatibility Identification

**User Story:** As a format validator, I want to identify file format brands and compatibility, so that I can determine which features and codecs are supported.

#### Acceptance Criteria

1. WHEN parsing ftyp box THEN ISODemuxer SHALL extract major brand four-character code
2. WHEN processing compatible brands THEN ISODemuxer SHALL extract all compatible brand codes
3. WHEN encountering 'isom' brand THEN ISODemuxer SHALL recognize ISO Base Media File Format
4. WHEN encountering 'mp41' brand THEN ISODemuxer SHALL recognize MP4 version 1 format
5. WHEN encountering 'mp42' brand THEN ISODemuxer SHALL recognize MP4 version 2 format
6. WHEN encountering 'avc1' brand THEN ISODemuxer SHALL recognize AVC/H.264 video support
7. WHEN encountering 'M4A ' brand THEN ISODemuxer SHALL recognize audio-only MP4 format
8. WHEN encountering unknown brands THEN ISODemuxer SHALL attempt parsing as generic ISO format

### Requirement 16: File Extension and Format Association

**User Story:** As a file system handler, I want to associate file extensions with ISO container formats, so that files can be identified and opened correctly.

#### Acceptance Criteria

1. WHEN encountering .mp4 extension THEN ISODemuxer SHALL recognize as MP4 container format
2. WHEN encountering .m4a extension THEN ISODemuxer SHALL recognize as audio-only MP4 format
3. WHEN encountering .m4v extension THEN ISODemuxer SHALL recognize as video MP4 format
4. WHEN encountering .mpg4 extension THEN ISODemuxer SHALL recognize as legacy MP4 format
5. WHEN encountering .mov extension THEN ISODemuxer SHALL recognize as QuickTime format
6. WHEN encountering .3gp extension THEN ISODemuxer SHALL recognize as 3GPP mobile format
7. WHEN encountering .f4a extension THEN ISODemuxer SHALL recognize as Flash audio format
8. WHEN file extension is missing THEN ISODemuxer SHALL identify format from ftyp box brand

### Requirement 17: Security and Buffer Management

**User Story:** As a security-conscious media player, I want protection against malformed MP4 files, so that buffer overflows and decoder crashes are prevented per RFC 4337 security considerations.

#### Acceptance Criteria

1. WHEN processing sample sizes THEN ISODemuxer SHALL validate against maximum buffer sizes
2. WHEN handling box sizes THEN ISODemuxer SHALL prevent integer overflow in size calculations
3. WHEN allocating memory THEN ISODemuxer SHALL enforce maximum allocation limits
4. WHEN processing nested boxes THEN ISODemuxer SHALL limit recursion depth to prevent stack overflow
5. WHEN encountering malformed data THEN ISODemuxer SHALL reject non-compliant streams
6. WHEN handling codec configuration THEN ISODemuxer SHALL validate configuration data bounds
7. WHEN processing NAL units THEN ISODemuxer SHALL validate NAL unit sizes before processing
8. WHEN buffer models are violated THEN ISODemuxer SHALL report security violations and reject file

### Requirement 18: Mandatory Box Structure Compliance

**User Story:** As a standards-compliant demuxer, I want to enforce mandatory box requirements per ISO/IEC 14496-12, so that only valid ISO files are processed.

#### Acceptance Criteria

1. WHEN opening file THEN ISODemuxer SHALL require exactly one ftyp box at file start
2. WHEN processing file THEN ISODemuxer SHALL require exactly one moov box
3. WHEN processing moov box THEN ISODemuxer SHALL require exactly one mvhd box
4. WHEN processing track THEN ISODemuxer SHALL require exactly one trak box per track
5. WHEN processing trak box THEN ISODemuxer SHALL require exactly one tkhd box
6. WHEN processing track media THEN ISODemuxer SHALL require mdia box with mdhd and hdlr boxes
7. WHEN processing sample table THEN ISODemuxer SHALL require stsd, stsz, stsc, and stco boxes
8. WHEN sample description exists THEN ISODemuxer SHALL require at least one sample entry

### Requirement 19: Box Size and Version Handling

**User Story:** As a robust parser, I want to handle both 32-bit and 64-bit box sizes and multiple box versions, so that I can process files of any size per ISO/IEC 14496-12.

#### Acceptance Criteria

1. WHEN box size is 1 THEN ISODemuxer SHALL read 64-bit extended size field
2. WHEN box size is 0 THEN ISODemuxer SHALL treat box as extending to end of file
3. WHEN processing FullBox THEN ISODemuxer SHALL read version and flags fields
4. WHEN mvhd version is 0 THEN ISODemuxer SHALL use 32-bit time and duration fields
5. WHEN mvhd version is 1 THEN ISODemuxer SHALL use 64-bit time and duration fields
6. WHEN processing co64 box THEN ISODemuxer SHALL use 64-bit chunk offsets
7. WHEN box type is unrecognized THEN ISODemuxer SHALL skip box using size field
8. WHEN box extends beyond parent THEN ISODemuxer SHALL report error and reject file

### Requirement 20: Timescale and Duration Calculations

**User Story:** As a timing system, I want accurate timescale and duration handling per ISO/IEC 14496-12, so that playback timing is precise across all tracks.

#### Acceptance Criteria

1. WHEN reading mvhd THEN ISODemuxer SHALL extract movie timescale in time units per second
2. WHEN reading mdhd THEN ISODemuxer SHALL extract media timescale for each track
3. WHEN calculating duration THEN ISODemuxer SHALL use timescale to convert to seconds
4. WHEN timescales differ THEN ISODemuxer SHALL convert between movie and media timescales
5. WHEN duration is all 1s THEN ISODemuxer SHALL treat duration as unknown
6. WHEN processing stts THEN ISODemuxer SHALL map sample counts to time deltas
7. WHEN processing ctts THEN ISODemuxer SHALL apply composition time offsets for presentation order
8. WHEN calculating timestamps THEN ISODemuxer SHALL maintain 64-bit precision to prevent overflow

### Requirement 21: Sample Table Processing

**User Story:** As a sample locator, I want complete sample table processing per ISO/IEC 14496-12, so that I can locate any sample in the file accurately.

#### Acceptance Criteria

1. WHEN processing stsd THEN ISODemuxer SHALL extract data reference index for each sample description
2. WHEN processing stsz THEN ISODemuxer SHALL handle both per-sample sizes and constant sample size
3. WHEN processing stsc THEN ISODemuxer SHALL build sample-to-chunk mapping with first chunk and samples per chunk
4. WHEN processing stco THEN ISODemuxer SHALL extract 32-bit chunk offsets
5. WHEN processing co64 THEN ISODemuxer SHALL extract 64-bit chunk offsets for large files
6. WHEN processing stts THEN ISODemuxer SHALL build time-to-sample mapping with sample counts and deltas
7. WHEN processing stss THEN ISODemuxer SHALL extract sync sample numbers for keyframe seeking
8. WHEN stss is absent THEN ISODemuxer SHALL treat all samples as sync samples

### Requirement 22: Data Reference and Location

**User Story:** As a data locator, I want proper data reference handling per ISO/IEC 14496-12, so that samples can be located in local files or external URLs.

#### Acceptance Criteria

1. WHEN processing dinf THEN ISODemuxer SHALL extract data reference box
2. WHEN processing dref THEN ISODemuxer SHALL extract all data reference entries
3. WHEN data reference is url THEN ISODemuxer SHALL support URL-based sample location
4. WHEN data reference is urn THEN ISODemuxer SHALL support URN-based sample location
5. WHEN data reference flag is 1 THEN ISODemuxer SHALL use same file for sample data
6. WHEN locating sample THEN ISODemuxer SHALL use data reference index from sample description
7. WHEN external reference is used THEN ISODemuxer SHALL resolve URL through IOHandler
8. WHEN data reference is invalid THEN ISODemuxer SHALL report error and skip track

### Requirement 23: Handler Type Identification

**User Story:** As a track classifier, I want proper handler type identification per ISO/IEC 14496-12, so that audio and video tracks are correctly identified.

#### Acceptance Criteria

1. WHEN processing hdlr box THEN ISODemuxer SHALL extract handler type four-character code
2. WHEN handler type is 'soun' THEN ISODemuxer SHALL identify track as audio
3. WHEN handler type is 'vide' THEN ISODemuxer SHALL identify track as video
4. WHEN handler type is 'hint' THEN ISODemuxer SHALL identify track as hint track
5. WHEN handler type is 'meta' THEN ISODemuxer SHALL identify track as metadata
6. WHEN handler type is 'text' THEN ISODemuxer SHALL identify track as text/subtitle
7. WHEN handler type is unrecognized THEN ISODemuxer SHALL skip track
8. WHEN no audio tracks exist THEN ISODemuxer SHALL report error for audio-only playback

### Requirement 24: Edit List Support

**User Story:** As a timeline modifier, I want edit list support per ISO/IEC 14496-12, so that track timelines can be modified for synchronization and trimming.

#### Acceptance Criteria

1. WHEN edts box exists THEN ISODemuxer SHALL process edit list
2. WHEN processing elst THEN ISODemuxer SHALL extract segment duration and media time
3. WHEN media time is -1 THEN ISODemuxer SHALL treat segment as empty edit
4. WHEN applying edit list THEN ISODemuxer SHALL map presentation time to media time
5. WHEN edit rate is specified THEN ISODemuxer SHALL apply playback rate modification
6. WHEN multiple edit segments exist THEN ISODemuxer SHALL process segments in order
7. WHEN seeking with edit list THEN ISODemuxer SHALL account for timeline modifications
8. WHEN edit list is absent THEN ISODemuxer SHALL use identity mapping for timeline

### Requirement 25: Track Header and Flags

**User Story:** As a track manager, I want proper track header processing per ISO/IEC 14496-12, so that track properties and flags are correctly interpreted.

#### Acceptance Criteria

1. WHEN processing tkhd THEN ISODemuxer SHALL extract track ID
2. WHEN track enabled flag is 0 THEN ISODemuxer SHALL skip track for playback
3. WHEN track in movie flag is 0 THEN ISODemuxer SHALL exclude track from presentation
4. WHEN track in preview flag is set THEN ISODemuxer SHALL include track in preview mode
5. WHEN processing track dimensions THEN ISODemuxer SHALL extract width and height for video
6. WHEN processing track volume THEN ISODemuxer SHALL extract volume for audio tracks
7. WHEN processing track matrix THEN ISODemuxer SHALL extract transformation matrix
8. WHEN track ID is 0 THEN ISODemuxer SHALL reject file as invalid