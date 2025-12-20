# Requirements Document

## Introduction

This document specifies the requirements for a comprehensive Tag framework in PsyMP3 that provides format-neutral metadata access for audio files. The framework will support reading metadata tags from various formats including VorbisComment (used in Ogg, FLAC), ID3v1/ID3v2 (used in MP3), and APE tags. The Tag framework integrates with the Stream and DemuxedStream classes to provide unified metadata access regardless of the underlying container format.

## Glossary

- **Tag**: A metadata container holding information about an audio file (title, artist, album, etc.)
- **Tag_Reader**: A component that parses and extracts metadata from a specific tag format
- **Tag_Factory**: A factory component that creates appropriate Tag_Reader instances based on format detection
- **VorbisComment**: A metadata format used in Ogg containers and FLAC files, consisting of UTF-8 key=value pairs
- **ID3v1**: A simple fixed-size metadata format appended to MP3 files (128 bytes)
- **ID3v2**: A flexible metadata format prepended to MP3 files with variable-length frames
- **APE_Tag**: Another metadata format sometimes found in MP3 and other files
- **Picture**: Embedded artwork/image data within metadata tags
- **Stream**: The base class for audio stream access in PsyMP3
- **DemuxedStream**: A Stream implementation using the demuxer/codec architecture
- **Demuxer**: A component that parses container formats and extracts stream data

## Requirements

### Requirement 1: Format-Neutral Tag Interface

**User Story:** As a developer, I want a unified interface for accessing metadata tags, so that I can retrieve track information without knowing the underlying tag format.

#### Acceptance Criteria

1. THE Tag_Interface SHALL provide methods to retrieve core metadata fields: title, artist, album, album_artist, genre, year, track, track_total, disc, disc_total, comment, and composer
2. THE Tag_Interface SHALL provide a method to retrieve arbitrary tags by key name
3. THE Tag_Interface SHALL provide a method to retrieve all values for a multi-valued tag
4. THE Tag_Interface SHALL provide a method to retrieve all tags as a key-value map
5. THE Tag_Interface SHALL provide a method to check if a specific tag exists
6. THE Tag_Interface SHALL provide a method to check if the tag container is empty
7. THE Tag_Interface SHALL provide a method to retrieve the underlying format name

### Requirement 2: VorbisComment Tag Reader

**User Story:** As a user, I want to read metadata from Ogg and FLAC files, so that I can see track information for my Vorbis, Opus, and FLAC audio files.

#### Acceptance Criteria

1. WHEN a VorbisComment block is provided, THE VorbisComment_Reader SHALL parse the vendor string
2. WHEN a VorbisComment block is provided, THE VorbisComment_Reader SHALL parse all field name=value pairs
3. THE VorbisComment_Reader SHALL handle UTF-8 encoded field values correctly
4. THE VorbisComment_Reader SHALL perform case-insensitive field name lookups
5. THE VorbisComment_Reader SHALL support multi-valued fields (same field name appearing multiple times)
6. WHEN a field name contains invalid characters, THE VorbisComment_Reader SHALL skip that field and continue parsing
7. THE VorbisComment_Reader SHALL map standard VorbisComment field names to the Tag_Interface methods (TITLE→title, ARTIST→artist, ALBUM→album, etc.)
8. THE VorbisComment_Reader SHALL support the METADATA_BLOCK_PICTURE field for embedded artwork

### Requirement 3: ID3v2 Tag Reader

**User Story:** As a user, I want to read metadata from MP3 files with ID3v2 tags, so that I can see track information for my MP3 audio files.

#### Acceptance Criteria

1. WHEN an ID3v2 header is detected, THE ID3v2_Reader SHALL parse the tag version (2.2, 2.3, or 2.4)
2. WHEN an ID3v2 header is detected, THE ID3v2_Reader SHALL parse the tag size using synchsafe integers
3. THE ID3v2_Reader SHALL parse text frames (TIT2, TPE1, TALB, etc.) and map them to Tag_Interface methods
4. THE ID3v2_Reader SHALL handle different text encodings (ISO-8859-1, UTF-16, UTF-16BE, UTF-8)
5. THE ID3v2_Reader SHALL parse APIC frames for embedded artwork
6. WHEN unsync is enabled in the tag header, THE ID3v2_Reader SHALL decode unsynchronized data
7. IF an extended header is present, THEN THE ID3v2_Reader SHALL skip it correctly
8. THE ID3v2_Reader SHALL support ID3v2.3 and ID3v2.4 frame formats

### Requirement 4: ID3v1 Tag Reader

**User Story:** As a user, I want to read metadata from older MP3 files with ID3v1 tags, so that I can see track information for legacy MP3 files.

#### Acceptance Criteria

1. WHEN an ID3v1 tag is detected at file end, THE ID3v1_Reader SHALL parse the fixed 128-byte structure
2. THE ID3v1_Reader SHALL extract title (30 bytes), artist (30 bytes), album (30 bytes), year (4 bytes), and comment (30 bytes)
3. THE ID3v1_Reader SHALL detect ID3v1.1 format and extract track number from comment field
4. THE ID3v1_Reader SHALL map genre byte to genre string using the standard ID3v1 genre list
5. THE ID3v1_Reader SHALL trim trailing null bytes and spaces from string fields

### Requirement 5: Picture/Artwork Support

**User Story:** As a user, I want to access embedded album artwork, so that I can display cover art for my audio files.

#### Acceptance Criteria

1. THE Tag_Interface SHALL provide a method to get the count of embedded pictures
2. THE Tag_Interface SHALL provide a method to retrieve a picture by index
3. THE Tag_Interface SHALL provide a convenience method to retrieve the front cover picture
4. THE Picture structure SHALL contain: type, MIME type, description, dimensions, color depth, and raw image data
5. THE Picture_Type enumeration SHALL be compatible with ID3v2 APIC and FLAC METADATA_BLOCK_PICTURE types

### Requirement 6: Tag Factory and Format Detection

**User Story:** As a developer, I want automatic tag format detection, so that I can read metadata without manually specifying the format.

#### Acceptance Criteria

1. WHEN a file path is provided, THE Tag_Factory SHALL detect the tag format by examining file contents
2. WHEN raw data is provided, THE Tag_Factory SHALL detect the tag format from magic bytes
3. THE Tag_Factory SHALL support format hints to optimize detection
4. IF no tags are detected, THEN THE Tag_Factory SHALL return a NullTag instance
5. THE Tag_Factory SHALL never return a null pointer

### Requirement 6a: ID3 Tag Merging

**User Story:** As a user, I want complete metadata from MP3 files, so that I can see all available track information even when tags are split between ID3v1 and ID3v2.

#### Acceptance Criteria

1. WHEN an MP3 file is opened, THE ID3_Reader SHALL always check for and load the ID3v1 tag (if present)
2. WHEN an MP3 file is opened, THE ID3_Reader SHALL check for and load the ID3v2 tag (if present)
3. WHEN both ID3v1 and ID3v2 tags are present, THE ID3_Reader SHALL use ID3v2 data as the primary source
4. WHEN a field is missing in ID3v2 but present in ID3v1, THE ID3_Reader SHALL fall back to the ID3v1 value for that field
5. WHEN only ID3v1 is present, THE ID3_Reader SHALL use ID3v1 data exclusively
6. WHEN only ID3v2 is present, THE ID3_Reader SHALL use ID3v2 data exclusively
7. THE ID3_Reader SHALL parse ID3v1 immediately upon detection to ensure data availability for fallback

### Requirement 7: Stream Integration

**User Story:** As a developer, I want the Stream and DemuxedStream classes to provide tag access, so that I can retrieve metadata through the existing stream interface.

#### Acceptance Criteria

1. THE Stream class SHALL provide a method to retrieve the associated Tag object
2. THE DemuxedStream class SHALL extract tags from the demuxer's parsed metadata
3. WHEN a DemuxedStream is created, THE DemuxedStream SHALL populate tag data from container metadata
4. THE Stream::getArtist, Stream::getTitle, and Stream::getAlbum methods SHALL delegate to the Tag object
5. IF no tags are available, THEN THE Stream SHALL return a NullTag instance

### Requirement 8: Demuxer Tag Extraction

**User Story:** As a developer, I want demuxers to extract tag data during container parsing, so that metadata is available without additional file reads.

#### Acceptance Criteria

1. THE Demuxer base class SHALL provide a method to retrieve extracted tags
2. WHEN parsing a FLAC container, THE FLAC_Demuxer SHALL extract VorbisComment metadata
3. WHEN parsing an Ogg container, THE Ogg_Demuxer SHALL extract VorbisComment metadata from the appropriate stream header
4. THE Demuxer SHALL store extracted tag data in a format-neutral representation
5. IF no metadata is found during parsing, THEN THE Demuxer SHALL return a NullTag instance

### Requirement 9: Thread Safety

**User Story:** As a developer, I want thread-safe tag access, so that I can read metadata from multiple threads without data races.

#### Acceptance Criteria

1. THE Tag implementations SHALL be thread-safe for concurrent read operations
2. THE Tag_Factory SHALL be thread-safe for concurrent tag creation
3. THE Tag implementations SHALL use const methods for all read operations
4. THE Tag implementations SHALL not modify internal state during read operations

### Requirement 10: Error Handling

**User Story:** As a developer, I want robust error handling in tag parsing, so that corrupted or malformed tags don't crash the application.

#### Acceptance Criteria

1. IF tag data is corrupted, THEN THE Tag_Reader SHALL return a NullTag or partial data rather than throwing exceptions
2. IF a required field is missing, THEN THE Tag_Reader SHALL return an empty string or zero value
3. THE Tag_Reader SHALL validate data sizes before reading to prevent buffer overflows
4. WHEN encountering invalid UTF-8 sequences, THE Tag_Reader SHALL replace them with replacement characters or skip the field

### Requirement 11: Memory Efficiency

**User Story:** As a developer, I want memory-efficient tag handling, so that large embedded artwork doesn't cause excessive memory usage.

#### Acceptance Criteria

1. THE Tag implementations SHALL support lazy loading of picture data
2. THE Picture data SHALL be stored efficiently without unnecessary copies
3. THE Tag implementations SHALL release picture data when no longer needed
4. WHEN multiple pictures are present, THE Tag_Reader SHALL allow selective loading by index

