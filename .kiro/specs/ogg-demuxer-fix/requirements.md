# **OGG DEMUXER IMPLEMENTATION REQUIREMENTS**

## **Introduction**

This specification defines the requirements for implementing a robust, RFC-compliant Ogg container demuxer for PsyMP3. The demuxer must handle Ogg Vorbis, Ogg Opus, Ogg FLAC, and other Ogg-encapsulated audio formats with proper seeking, streaming, and error handling capabilities.

The implementation must strictly follow:
- **RFC 3533**: The Ogg Encapsulation Format Version 0
- **RFC 5334**: Ogg Media Types  
- **RFC 7845**: Ogg Encapsulation for the Opus Audio Codec
- **Vorbis I Specification** (xiph.org)
- **FLAC Format Specification** (xiph.org)

## **Requirements**

### **Requirement 1: Ogg Container Parsing**

**User Story:** As a media player, I want to parse Ogg container files correctly, so that I can extract audio streams and metadata reliably.

#### **Acceptance Criteria**

1. **WHEN** an Ogg file is opened **THEN** the demuxer **SHALL** validate the "OggS" capture pattern per RFC 3533 Section 6
2. **WHEN** parsing page headers **THEN** the demuxer **SHALL** verify page structure version is 0 per RFC 3533 Section 6.1
3. **WHEN** encountering page header type flags **THEN** the demuxer **SHALL** correctly interpret continued packet (0x01), first page (0x02), and last page (0x04) flags per RFC 3533 Section 6.1.1
4. **WHEN** reading granule positions **THEN** the demuxer **SHALL** handle 64-bit granule positions as codec-specific timing information per RFC 3533 Section 6.1.2
5. **WHEN** processing serial numbers **THEN** the demuxer **SHALL** use 32-bit serial numbers to identify logical bitstreams per RFC 3533 Section 6.1.3
6. **WHEN** validating pages **THEN** the demuxer **SHALL** verify CRC32 checksums per RFC 3533 Section 6.1.5
7. **WHEN** parsing segment table **THEN** the demuxer **SHALL** correctly reconstruct packets from page segments per RFC 3533 Section 6.1.6

### **Requirement 2: Logical Bitstream Identification**

**User Story:** As a media player, I want to identify different codec types within Ogg containers, so that I can route packets to appropriate decoders.

#### **Acceptance Criteria**

1. **WHEN** encountering a BOS (Beginning of Stream) packet **THEN** the demuxer **SHALL** identify the codec type from packet signature
2. **WHEN** detecting Vorbis streams **THEN** the demuxer **SHALL** recognize "\x01vorbis" identification header signature
3. **WHEN** detecting Opus streams **THEN** the demuxer **SHALL** recognize "OpusHead" identification header signature per RFC 7845 Section 5.1
4. **WHEN** detecting FLAC streams **THEN** the demuxer **SHALL** recognize "\x7fFLAC" identification header signature
5. **WHEN** detecting Speex streams **THEN** the demuxer **SHALL** recognize "Speex   " identification header signature
6. **WHEN** multiple logical bitstreams exist **THEN** the demuxer **SHALL** handle multiplexed streams correctly per RFC 3533 Section 3
7. **WHEN** chained bitstreams exist **THEN** the demuxer **SHALL** handle sequential logical bitstreams per RFC 3533 Section 3

### **Requirement 3: Header Packet Processing**

**User Story:** As a media player, I want to extract codec configuration and metadata from header packets, so that I can initialize decoders and display track information.

#### **Acceptance Criteria**

1. **WHEN** processing Vorbis identification headers **THEN** the demuxer **SHALL** extract sample rate, channel count, and bitrate information per Vorbis I Specification Section 4.2.2
2. **WHEN** processing Vorbis comment headers **THEN** the demuxer **SHALL** parse UTF-8 metadata fields per Vorbis I Specification Section 4.2.3
3. **WHEN** processing Vorbis setup headers **THEN** the demuxer **SHALL** preserve codec setup data for decoder initialization per Vorbis I Specification Section 4.2.4
4. **WHEN** processing Opus identification headers **THEN** the demuxer **SHALL** extract channel count, pre-skip, and input sample rate per RFC 7845 Section 5.1
5. **WHEN** processing Opus comment headers **THEN** the demuxer **SHALL** parse OpusTags metadata per RFC 7845 Section 5.2
6. **WHEN** processing FLAC identification headers **THEN** the demuxer **SHALL** extract STREAMINFO metadata block information
7. **WHEN** header parsing is complete **THEN** the demuxer **SHALL** mark streams as ready for data packet processing

### **Requirement 4: Data Packet Streaming**

**User Story:** As a media player, I want to stream audio data packets sequentially, so that I can provide continuous audio playback.

#### **Acceptance Criteria**

1. **WHEN** reading data packets **THEN** the demuxer **SHALL** maintain packet order within each logical bitstream
2. **WHEN** packets span multiple pages **THEN** the demuxer **SHALL** correctly reconstruct complete packets per RFC 3533 Section 6.1.6
3. **WHEN** encountering continued packets **THEN** the demuxer **SHALL** handle packet continuation flags correctly
4. **WHEN** processing granule positions **THEN** the demuxer **SHALL** track timing information for position reporting
5. **WHEN** reaching end of stream **THEN** the demuxer **SHALL** detect EOS (End of Stream) markers per RFC 3533 Section 6.1.1
6. **WHEN** buffering data **THEN** the demuxer **SHALL** implement efficient packet queuing without excessive memory usage
7. **WHEN** multiple streams are present **THEN** the demuxer **SHALL** interleave packets correctly based on granule positions

### **Requirement 5: Seeking Operations**

**User Story:** As a media player, I want to seek to specific timestamps in Ogg files, so that users can navigate through audio content.

#### **Acceptance Criteria**

1. **WHEN** seeking to a timestamp **THEN** the demuxer **SHALL** use bisection search to locate target pages efficiently
2. **WHEN** performing bisection search **THEN** the demuxer **SHALL** use granule positions for accurate positioning
3. **WHEN** seeking in Opus streams **THEN** the demuxer **SHALL** account for pre-skip samples per RFC 7845 Section 4.2
4. **WHEN** seeking in Vorbis streams **THEN** the demuxer **SHALL** handle variable-rate granule position mapping
5. **WHEN** seeking to stream beginning **THEN** the demuxer **SHALL** reset to first data packet after headers
6. **WHEN** seek operation completes **THEN** the demuxer **SHALL** NOT resend header packets to decoder
7. **WHEN** seek fails **THEN** the demuxer **SHALL** maintain valid state and report error appropriately
8. **WHEN** seeking near stream boundaries **THEN** the demuxer **SHALL** handle edge cases gracefully

### **Requirement 6: Duration Calculation**

**User Story:** As a media player, I want to determine the total duration of Ogg files, so that I can display progress information and enable seeking.

#### **Acceptance Criteria**

1. **WHEN** calculating duration **THEN** the demuxer **SHALL** use the last valid granule position in the file
2. **WHEN** granule positions are available in headers **THEN** the demuxer **SHALL** prefer header-provided total sample counts
3. **WHEN** scanning for last granule **THEN** the demuxer **SHALL** search backwards from file end efficiently
4. **WHEN** multiple streams exist **THEN** the demuxer **SHALL** use the longest stream for duration calculation
5. **WHEN** granule-to-time conversion is needed **THEN** the demuxer **SHALL** use codec-specific sample rates
6. **WHEN** Opus duration is calculated **THEN** the demuxer **SHALL** use 48kHz granule rate per RFC 7845 Section 4.2
7. **WHEN** duration cannot be determined **THEN** the demuxer **SHALL** report unknown duration rather than incorrect values

### **Requirement 7: Error Handling and Robustness**

**User Story:** As a media player, I want the demuxer to handle corrupted or malformed Ogg files gracefully, so that playback doesn't crash or produce incorrect results.

#### **Acceptance Criteria**

1. **WHEN** encountering invalid page headers **THEN** the demuxer **SHALL** skip corrupted pages and continue processing
2. **WHEN** CRC validation fails **THEN** the demuxer **SHALL** log errors but attempt to continue playback
3. **WHEN** packet reconstruction fails **THEN** the demuxer **SHALL** discard incomplete packets gracefully
4. **WHEN** codec identification fails **THEN** the demuxer **SHALL** skip unknown streams without affecting other streams
5. **WHEN** memory allocation fails **THEN** the demuxer **SHALL** return appropriate error codes
6. **WHEN** I/O operations fail **THEN** the demuxer **SHALL** handle read errors and EOF conditions properly
7. **WHEN** seeking beyond file boundaries **THEN** the demuxer **SHALL** clamp to valid ranges
8. **WHEN** malformed metadata is encountered **THEN** the demuxer **SHALL** parse what's possible and continue

### **Requirement 8: Performance and Memory Management**

**User Story:** As a media player, I want efficient Ogg processing with minimal memory usage, so that large files can be handled without excessive resource consumption.

#### **Acceptance Criteria**

1. **WHEN** processing large files **THEN** the demuxer **SHALL** use streaming approach without loading entire file into memory
2. **WHEN** buffering packets **THEN** the demuxer **SHALL** implement bounded queues to prevent memory exhaustion
3. **WHEN** seeking operations occur **THEN** the demuxer **SHALL** minimize I/O operations through efficient bisection
4. **WHEN** parsing headers **THEN** the demuxer **SHALL** process headers incrementally without excessive buffering
5. **WHEN** handling multiple streams **THEN** the demuxer **SHALL** share resources efficiently between streams
6. **WHEN** cleaning up resources **THEN** the demuxer **SHALL** properly free all allocated memory and libogg structures
7. **WHEN** processing very long files **THEN** the demuxer **SHALL** maintain acceptable performance characteristics

### **Requirement 9: Thread Safety and Concurrency**

**User Story:** As a media player, I want thread-safe Ogg demuxing operations, so that seeking and reading can occur from different threads safely.

#### **Acceptance Criteria**

1. **WHEN** multiple threads access the demuxer **THEN** the demuxer **SHALL** protect shared state with appropriate synchronization
2. **WHEN** seeking occurs during playback **THEN** the demuxer **SHALL** handle concurrent operations safely
3. **WHEN** I/O operations are in progress **THEN** the demuxer **SHALL** prevent race conditions on file position
4. **WHEN** packet queues are accessed **THEN** the demuxer **SHALL** ensure thread-safe queue operations
5. **WHEN** stream state is modified **THEN** the demuxer **SHALL** use atomic operations where appropriate
6. **WHEN** cleanup occurs **THEN** the demuxer **SHALL** ensure no operations are in progress before destruction
7. **WHEN** errors occur in one thread **THEN** the demuxer **SHALL** propagate error state safely to other threads

### **Requirement 10: API Compliance and Integration**

**User Story:** As a PsyMP3 component, I want the Ogg demuxer to integrate seamlessly with the existing demuxer architecture, so that it works consistently with other format demuxers.

#### **Acceptance Criteria**

1. **WHEN** implementing the Demuxer interface **THEN** the OggDemuxer **SHALL** provide all required virtual methods
2. **WHEN** returning stream information **THEN** the demuxer **SHALL** populate StreamInfo structures with accurate codec data
3. **WHEN** providing media chunks **THEN** the demuxer **SHALL** return properly formatted MediaChunk objects
4. **WHEN** reporting position **THEN** the demuxer **SHALL** return timestamps in milliseconds consistently
5. **WHEN** handling IOHandler operations **THEN** the demuxer **SHALL** use the provided IOHandler interface exclusively
6. **WHEN** logging debug information **THEN** the demuxer **SHALL** use the PsyMP3 Debug logging system
7. **WHEN** reporting errors **THEN** the demuxer **SHALL** use consistent error codes and messages across the codebase