# **FLAC DEMUXER REQUIREMENTS - REVISED**

## **Introduction**

This specification defines the requirements for implementing a native FLAC container demuxer for PsyMP3, based on lessons learned from real-world implementation and debugging. The FLAC demuxer handles native FLAC files (.flac) by parsing the FLAC container format and extracting FLAC bitstream data for decoding.

**Key Implementation Insights:**
- **Frame size estimation** is critical for performance - must use STREAMINFO minimum frame size
- **Highly compressed FLAC** can have frames as small as 14 bytes, requiring accurate estimation
- **Binary search seeking** is fundamentally incompatible with compressed audio streams
- **Thread safety** requires careful public/private method patterns to prevent deadlocks
- **Debug logging** with method identification tokens is essential for troubleshooting

The implementation must support:
- **Native FLAC container format** as defined in RFC 9639 (FLAC specification)
- **Accurate frame size estimation** using STREAMINFO metadata for performance
- **Efficient frame boundary detection** with minimal I/O operations
- **Robust error recovery** for corrupted or incomplete streams
- **Thread-safe operations** using proper synchronization patterns

## **Requirements**

### **Requirement 1: FLAC Container Parsing with Robust Error Recovery**

**User Story:** As a media player, I want to parse native FLAC container files correctly with robust error recovery, so that I can extract FLAC bitstream data and metadata reliably even from damaged files.

#### **Acceptance Criteria**

1. **WHEN** a FLAC file is opened **THEN** the demuxer **SHALL** validate the "fLaC" stream marker per RFC 9639
2. **WHEN** parsing metadata blocks **THEN** the demuxer **SHALL** read block header with type, is_last flag, and 24-bit length
3. **WHEN** encountering STREAMINFO block **THEN** the demuxer **SHALL** extract minimum/maximum frame sizes for performance optimization
4. **WHEN** processing metadata blocks **THEN** the demuxer **SHALL** skip unknown blocks gracefully without failing
5. **WHEN** metadata parsing fails **THEN** the demuxer **SHALL** attempt STREAMINFO recovery from first frame header
6. **WHEN** reaching audio data **THEN** the demuxer **SHALL** identify the start of FLAC frames at calculated offset
7. **WHEN** handling large files **THEN** the demuxer **SHALL** support files larger than 4GB with 64-bit offsets
8. **WHEN** validating structure **THEN** the demuxer **SHALL** provide detailed error messages for debugging

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

### **Requirement 3: Accurate Frame Size Estimation and Efficient Boundary Detection**

**User Story:** As a FLAC codec, I want to receive properly formatted FLAC frame data with minimal I/O overhead, so that I can decode audio samples efficiently even from highly compressed streams.

#### **Acceptance Criteria**

1. **WHEN** estimating frame sizes **THEN** the demuxer **SHALL** use STREAMINFO minimum frame size as primary estimate
2. **WHEN** handling fixed block size streams **THEN** the demuxer **SHALL** use minimum frame size directly without scaling
3. **WHEN** detecting frame boundaries **THEN** the demuxer **SHALL** limit search scope to prevent excessive I/O operations
4. **WHEN** boundary detection fails **THEN** the demuxer **SHALL** use conservative STREAMINFO-based fallback sizes
5. **WHEN** reading highly compressed frames **THEN** the demuxer **SHALL** handle frames as small as 14 bytes efficiently
6. **WHEN** encountering frame errors **THEN** the demuxer **SHALL** provide silence output for unrecoverable frames
7. **WHEN** streaming frames **THEN** the demuxer **SHALL** maintain accurate sample position tracking
8. **WHEN** providing frame data **THEN** the demuxer **SHALL** include complete frames with proper MediaChunk formatting

### **Requirement 4: Seeking Operations with Architectural Limitations**

**User Story:** As a media player, I want to seek to specific timestamps in FLAC files, so that users can navigate through audio content, understanding that compressed audio has fundamental seeking limitations.

#### **Acceptance Criteria**

1. **WHEN** seeking to beginning **THEN** the demuxer **SHALL** reset to first audio frame efficiently
2. **WHEN** seeking with SEEKTABLE **THEN** the demuxer **SHALL** use seek points for approximate positioning
3. **WHEN** SEEKTABLE is available **THEN** the demuxer **SHALL** find closest seek point and parse forward
4. **WHEN** seeking without SEEKTABLE **THEN** the demuxer **SHALL** acknowledge binary search limitations with compressed data
5. **WHEN** binary search fails **THEN** the demuxer **SHALL** fall back to beginning position gracefully
6. **WHEN** seeking beyond stream end **THEN** the demuxer **SHALL** clamp to last valid position
7. **WHEN** seek operation fails **THEN** the demuxer **SHALL** maintain current position and provide clear error reporting
8. **WHEN** implementing future seeking **THEN** the demuxer **SHALL** support frame indexing during initial parsing

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

### **Requirement 7: Performance Optimization Based on Real-World Lessons**

**User Story:** As a media player, I want efficient FLAC processing with minimal I/O overhead, so that highly compressed files can be processed without performance degradation.

#### **Acceptance Criteria**

1. **WHEN** estimating frame sizes **THEN** the demuxer **SHALL** avoid complex theoretical calculations that cause inaccurate estimates
2. **WHEN** detecting frame boundaries **THEN** the demuxer **SHALL** limit search operations to 512 bytes maximum
3. **WHEN** processing highly compressed streams **THEN** the demuxer **SHALL** reduce I/O operations from hundreds to tens per frame
4. **WHEN** using STREAMINFO data **THEN** the demuxer **SHALL** prioritize minimum frame size over complex scaling algorithms
5. **WHEN** handling frame reading **THEN** the demuxer **SHALL** complete processing in milliseconds rather than seconds
6. **WHEN** managing memory **THEN** the demuxer **SHALL** use accurate frame size estimates to prevent buffer waste
7. **WHEN** debugging performance **THEN** the demuxer **SHALL** provide method-specific logging tokens for identification
8. **WHEN** processing sequential frames **THEN** the demuxer **SHALL** maintain consistent performance across frame types

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

### **Requirement 10: Thread Safety Using Public/Private Lock Pattern**

**User Story:** As a multi-threaded media player, I want thread-safe FLAC demuxing operations using proven patterns, so that seeking and reading can occur concurrently without deadlocks.

#### **Acceptance Criteria**

1. **WHEN** implementing public methods **THEN** the demuxer **SHALL** acquire locks and call private `_unlocked` implementations
2. **WHEN** calling internal methods **THEN** the demuxer **SHALL** use `_unlocked` versions to prevent deadlocks
3. **WHEN** multiple locks are needed **THEN** the demuxer **SHALL** acquire them in documented order to prevent deadlocks
4. **WHEN** using RAII lock guards **THEN** the demuxer **SHALL** ensure exception safety for all operations
5. **WHEN** invoking callbacks **THEN** the demuxer **SHALL** never call callbacks while holding internal locks
6. **WHEN** updating position tracking **THEN** the demuxer **SHALL** use atomic operations for sample counters
7. **WHEN** handling errors **THEN** the demuxer **SHALL** use atomic error state flags for thread-safe propagation
8. **WHEN** debugging threading issues **THEN** the demuxer **SHALL** provide clear method identification for lock analysis

### **Requirement 11: Debug Logging and Troubleshooting Support**

**User Story:** As a developer debugging FLAC issues, I want comprehensive logging with method identification, so that I can quickly identify which code paths are executing and causing problems.

#### **Acceptance Criteria**

1. **WHEN** logging debug messages **THEN** the demuxer **SHALL** include method-specific tokens like `[calculateFrameSize]`
2. **WHEN** multiple methods use similar messages **THEN** the demuxer **SHALL** use unique identifiers to distinguish them
3. **WHEN** frame size estimation occurs **THEN** the demuxer **SHALL** log which calculation method is being used
4. **WHEN** frame boundary detection runs **THEN** the demuxer **SHALL** log search scope and results
5. **WHEN** seeking operations execute **THEN** the demuxer **SHALL** log strategy selection and outcomes
6. **WHEN** error recovery activates **THEN** the demuxer **SHALL** log recovery attempts and success/failure
7. **WHEN** performance issues occur **THEN** the demuxer **SHALL** provide timing information for critical operations
8. **WHEN** troubleshooting **THEN** the demuxer **SHALL** log sufficient detail to reproduce and fix issues