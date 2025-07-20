# **FLAC DEMUXER REQUIREMENTS**

## **Introduction**

This specification defines the requirements for implementing a native FLAC container demuxer for PsyMP3. The FLAC demuxer will handle native FLAC files (.flac) by parsing the FLAC container format and extracting FLAC bitstream data for decoding. This demuxer works in conjunction with a separate FLACCodec to provide container-agnostic FLAC decoding.

The implementation must support:
- **Native FLAC container format** as defined in the FLAC specification
- **FLAC metadata blocks** including STREAMINFO, VORBIS_COMMENT, and others
- **Efficient seeking** using FLAC's built-in seek table or sample-accurate seeking
- **Large file support** for high-resolution and long-duration FLAC files
- **Integration** with the modern demuxer/codec architecture

## **Requirements**

### **Requirement 1: FLAC Container Parsing**

**User Story:** As a media player, I want to parse native FLAC container files correctly, so that I can extract FLAC bitstream data and metadata reliably.

#### **Acceptance Criteria**

1. **WHEN** a FLAC file is opened **THEN** the demuxer **SHALL** validate the "fLaC" stream marker per FLAC specification
2. **WHEN** parsing metadata blocks **THEN** the demuxer **SHALL** read block header with type and size information
3. **WHEN** encountering STREAMINFO block **THEN** the demuxer **SHALL** extract sample rate, channels, bit depth, and total samples
4. **WHEN** processing metadata blocks **THEN** the demuxer **SHALL** handle both standard and non-standard block types
5. **WHEN** reaching audio data **THEN** the demuxer **SHALL** identify the start of FLAC frames
6. **WHEN** parsing is complete **THEN** the demuxer **SHALL** be ready to provide FLAC bitstream chunks
7. **WHEN** handling large files **THEN** the demuxer **SHALL** support files larger than 2GB
8. **WHEN** validating structure **THEN** the demuxer **SHALL** verify metadata block structure integrity

### **Requirement 2: Metadata Block Processing**

**User Story:** As a media player, I want to extract comprehensive metadata from FLAC files, so that I can display track information and configure playback properly.

#### **Acceptance Criteria**

1. **WHEN** processing STREAMINFO **THEN** the demuxer **SHALL** extract minimum/maximum block size, frame size, sample rate, channels, bit depth, and total samples
2. **WHEN** processing VORBIS_COMMENT **THEN** the demuxer **SHALL** parse UTF-8 metadata fields (ARTIST, TITLE, ALBUM, etc.)
3. **WHEN** processing SEEKTABLE **THEN** the demuxer **SHALL** store seek points for efficient seeking
4. **WHEN** processing APPLICATION blocks **THEN** the demuxer **SHALL** handle application-specific data appropriately
5. **WHEN** processing PADDING blocks **THEN** the demuxer **SHALL** skip padding data efficiently
6. **WHEN** processing PICTURE blocks **THEN** the demuxer **SHALL** extract embedded artwork metadata
7. **WHEN** encountering unknown blocks **THEN** the demuxer **SHALL** skip unknown metadata blocks gracefully
8. **WHEN** metadata is incomplete **THEN** the demuxer **SHALL** provide reasonable defaults for missing information

### **Requirement 3: FLAC Frame Identification and Streaming**

**User Story:** As a FLAC codec, I want to receive properly formatted FLAC frame data, so that I can decode audio samples correctly.

#### **Acceptance Criteria**

1. **WHEN** identifying FLAC frames **THEN** the demuxer **SHALL** locate frame sync codes (0xFFF8-0xFFFF)
2. **WHEN** parsing frame headers **THEN** the demuxer **SHALL** extract block size, sample rate, channel assignment, and bit depth
3. **WHEN** reading frame data **THEN** the demuxer **SHALL** include complete frames with headers and CRC
4. **WHEN** streaming frames **THEN** the demuxer **SHALL** provide frames in sequential order
5. **WHEN** handling variable block sizes **THEN** the demuxer **SHALL** adapt to different frame sizes within the stream
6. **WHEN** encountering frame errors **THEN** the demuxer **SHALL** skip corrupted frames and continue
7. **WHEN** reaching end of stream **THEN** the demuxer **SHALL** detect EOF condition properly
8. **WHEN** providing frame data **THEN** the demuxer **SHALL** include sample position information for timing

### **Requirement 4: Seeking Operations**

**User Story:** As a media player, I want to seek to specific timestamps in FLAC files, so that users can navigate through audio content efficiently.

#### **Acceptance Criteria**

1. **WHEN** seeking with SEEKTABLE **THEN** the demuxer **SHALL** use seek points for fast approximate positioning
2. **WHEN** seeking without SEEKTABLE **THEN** the demuxer **SHALL** perform binary search through frames
3. **WHEN** seeking to exact positions **THEN** the demuxer **SHALL** provide sample-accurate positioning
4. **WHEN** seeking to stream beginning **THEN** the demuxer **SHALL** reset to first audio frame
5. **WHEN** seeking beyond stream end **THEN** the demuxer **SHALL** clamp to last valid position
6. **WHEN** seek operation completes **THEN** the demuxer **SHALL** update position tracking accurately
7. **WHEN** seeking fails **THEN** the demuxer **SHALL** maintain current position and report error
8. **WHEN** seeking in large files **THEN** the demuxer **SHALL** maintain efficient performance

### **Requirement 5: Duration and Position Tracking**

**User Story:** As a media player, I want accurate duration and position information for FLAC files, so that I can display progress and enable seeking.

#### **Acceptance Criteria**

1. **WHEN** calculating duration **THEN** the demuxer **SHALL** use total samples from STREAMINFO block
2. **WHEN** STREAMINFO is incomplete **THEN** the demuxer **SHALL** estimate duration from file size and bitrate
3. **WHEN** tracking position **THEN** the demuxer **SHALL** maintain current sample position accurately
4. **WHEN** converting to time **THEN** the demuxer **SHALL** use sample rate for millisecond conversion
5. **WHEN** handling variable bitrate **THEN** the demuxer **SHALL** account for compression ratio variations
6. **WHEN** reporting position **THEN** the demuxer **SHALL** provide both sample and time-based positions
7. **WHEN** position is unknown **THEN** the demuxer **SHALL** return appropriate default values
8. **WHEN** handling very long files **THEN** the demuxer **SHALL** prevent integer overflow in calculations

### **Requirement 6: Error Handling and Robustness**

**User Story:** As a media player, I want robust error handling in FLAC parsing, so that corrupted or unusual files don't crash the application.

#### **Acceptance Criteria**

1. **WHEN** encountering invalid stream markers **THEN** the demuxer **SHALL** reject non-FLAC files gracefully
2. **WHEN** metadata blocks are corrupted **THEN** the demuxer **SHALL** skip damaged blocks and continue
3. **WHEN** STREAMINFO is missing **THEN** the demuxer **SHALL** attempt to derive parameters from frame headers
4. **WHEN** frame sync is lost **THEN** the demuxer **SHALL** resynchronize to next valid frame
5. **WHEN** CRC errors occur **THEN** the demuxer **SHALL** log errors but attempt to continue playback
6. **WHEN** file truncation occurs **THEN** the demuxer **SHALL** handle incomplete files gracefully
7. **WHEN** memory allocation fails **THEN** the demuxer **SHALL** return appropriate error codes
8. **WHEN** I/O operations fail **THEN** the demuxer **SHALL** handle read errors and EOF conditions properly

### **Requirement 7: Performance and Memory Management**

**User Story:** As a media player, I want efficient FLAC processing with minimal memory usage, so that large files can be handled without excessive resource consumption.

#### **Acceptance Criteria**

1. **WHEN** processing large files **THEN** the demuxer **SHALL** use streaming approach without loading entire file
2. **WHEN** buffering frames **THEN** the demuxer **SHALL** implement bounded buffers to prevent memory exhaustion
3. **WHEN** seeking operations occur **THEN** the demuxer **SHALL** minimize I/O operations through efficient algorithms
4. **WHEN** parsing metadata **THEN** the demuxer **SHALL** process blocks incrementally without excessive buffering
5. **WHEN** handling seek tables **THEN** the demuxer **SHALL** store seek points efficiently in memory
6. **WHEN** cleaning up resources **THEN** the demuxer **SHALL** properly free all allocated memory
7. **WHEN** processing very long files **THEN** the demuxer **SHALL** maintain acceptable performance characteristics
8. **WHEN** caching data **THEN** the demuxer **SHALL** implement bounded caches to prevent unbounded growth

### **Requirement 8: Integration with Demuxer Architecture**

**User Story:** As a PsyMP3 component, I want the FLAC demuxer to integrate seamlessly with the existing demuxer architecture, so that it works consistently with other format demuxers.

#### **Acceptance Criteria**

1. **WHEN** implementing the Demuxer interface **THEN** FLACDemuxer **SHALL** provide all required virtual methods
2. **WHEN** returning stream information **THEN** the demuxer **SHALL** populate StreamInfo with accurate FLAC parameters
3. **WHEN** providing media chunks **THEN** the demuxer **SHALL** return properly formatted MediaChunk objects with FLAC frames
4. **WHEN** reporting position **THEN** the demuxer **SHALL** return timestamps in milliseconds consistently
5. **WHEN** handling IOHandler operations **THEN** the demuxer **SHALL** use the provided IOHandler interface exclusively
6. **WHEN** logging debug information **THEN** the demuxer **SHALL** use the PsyMP3 Debug logging system
7. **WHEN** reporting errors **THEN** the demuxer **SHALL** use consistent error codes and messages
8. **WHEN** working with FLACCodec **THEN** the demuxer **SHALL** provide compatible frame data format

### **Requirement 9: Compatibility with Existing FLAC Implementation**

**User Story:** As a PsyMP3 user, I want the new FLAC demuxer to maintain compatibility with existing FLAC playback, so that my FLAC files continue to work without issues.

#### **Acceptance Criteria**

1. **WHEN** replacing existing FLAC code **THEN** the new implementation **SHALL** support all previously supported FLAC variants
2. **WHEN** handling metadata **THEN** the demuxer **SHALL** extract the same metadata fields as the current implementation
3. **WHEN** seeking **THEN** the demuxer **SHALL** provide at least the same seeking accuracy as current implementation
4. **WHEN** processing files **THEN** the demuxer **SHALL** handle all FLAC files that currently work
5. **WHEN** reporting duration **THEN** the demuxer **SHALL** provide consistent duration calculations
6. **WHEN** handling errors **THEN** the demuxer **SHALL** provide equivalent error handling behavior
7. **WHEN** integrating with Stream interface **THEN** the demuxer **SHALL** work through DemuxedStream bridge
8. **WHEN** performance is measured **THEN** the demuxer **SHALL** provide comparable or better performance

### **Requirement 10: Thread Safety and Concurrency**

**User Story:** As a multi-threaded media player, I want thread-safe FLAC demuxing operations, so that seeking and reading can occur concurrently without corruption.

#### **Acceptance Criteria**

1. **WHEN** multiple threads access the demuxer **THEN** it **SHALL** protect shared state with appropriate synchronization
2. **WHEN** seeking occurs during playback **THEN** the demuxer **SHALL** handle concurrent operations safely
3. **WHEN** I/O operations are in progress **THEN** the demuxer **SHALL** prevent race conditions on IOHandler
4. **WHEN** metadata is accessed **THEN** the demuxer **SHALL** ensure thread-safe metadata access
5. **WHEN** position tracking is updated **THEN** the demuxer **SHALL** use atomic operations where appropriate
6. **WHEN** cleanup occurs **THEN** the demuxer **SHALL** ensure no operations are in progress before destruction
7. **WHEN** errors occur in one thread **THEN** the demuxer **SHALL** propagate error state safely to other threads
8. **WHEN** caching data **THEN** the demuxer **SHALL** implement thread-safe caching mechanisms