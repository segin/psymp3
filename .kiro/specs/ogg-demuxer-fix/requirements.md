# **OGG DEMUXER IMPLEMENTATION REQUIREMENTS**

## **Introduction**

This specification defines the requirements for implementing a robust, RFC-compliant Ogg container demuxer for PsyMP3. The demuxer must handle Ogg Vorbis, Ogg Opus, Ogg FLAC, and other Ogg-encapsulated audio formats with proper seeking, streaming, and error handling capabilities.

The implementation must strictly follow the behavior patterns established by the reference implementations:
- **libvorbisfile** (xiph.org/vorbis) - Reference Vorbis implementation patterns
- **libopusfile** (xiph.org/opus) - Reference Opus implementation patterns
- **RFC 3533**: The Ogg Encapsulation Format Version 0
- **RFC 5334**: Ogg Media Types  
- **RFC 7845**: Ogg Encapsulation for the Opus Audio Codec
- **Vorbis I Specification** (xiph.org)
- **FLAC Format Specification** (xiph.org)

**CRITICAL**: All seeking, granule position handling, and error recovery behavior must match libvorbisfile and libopusfile exactly to prevent compatibility issues and bugs.

## **Requirements**

### **Requirement 1: Ogg Container Parsing (Following libogg Reference Patterns)**

**User Story:** As a media player, I want to parse Ogg container files correctly, so that I can extract audio streams and metadata reliably.

#### **Acceptance Criteria**

1. **WHEN** an Ogg file is opened **THEN** the demuxer **SHALL** validate the "OggS" capture pattern per RFC 3533 Section 6 using memcmp() like libopusfile
2. **WHEN** parsing page headers **THEN** the demuxer **SHALL** verify page structure version is 0 per RFC 3533 Section 6.1
3. **WHEN** encountering page header type flags **THEN** the demuxer **SHALL** correctly interpret continued packet (0x01), first page (0x02), and last page (0x04) flags per RFC 3533 Section 6.1.1
4. **WHEN** reading granule positions **THEN** the demuxer **SHALL** handle 64-bit granule positions as codec-specific timing information using ogg_page_granulepos()
5. **WHEN** processing serial numbers **THEN** the demuxer **SHALL** use ogg_page_serialno() to identify logical bitstreams per RFC 3533 Section 6.1.3
6. **WHEN** validating pages **THEN** the demuxer **SHALL** rely on libogg's internal CRC32 validation per RFC 3533 Section 6.1.5
7. **WHEN** parsing segment table **THEN** the demuxer **SHALL** use ogg_sync_pageseek() and ogg_sync_pageout() for page extraction like libvorbisfile
8. **WHEN** reconstructing packets **THEN** the demuxer **SHALL** use ogg_stream_packetout() for packet extraction from pages
9. **WHEN** handling page boundaries **THEN** the demuxer **SHALL** use ogg_stream_pagein() to submit pages to stream state
10. **WHEN** detecting BOS pages **THEN** the demuxer **SHALL** use ogg_page_bos() to identify beginning-of-stream pages
11. **WHEN** detecting EOS pages **THEN** the demuxer **SHALL** use ogg_page_eos() to identify end-of-stream pages

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

### **Requirement 3: Header Packet Processing (Following Reference Implementation Patterns)**

**User Story:** As a media player, I want to extract codec configuration and metadata from header packets, so that I can initialize decoders and display track information.

#### **Acceptance Criteria**

1. **WHEN** processing Vorbis identification headers **THEN** the demuxer **SHALL** use vorbis_synthesis_idheader() pattern to validate "\x01vorbis" signature
2. **WHEN** processing Vorbis comment headers **THEN** the demuxer **SHALL** parse UTF-8 metadata fields using vorbis_synthesis_headerin() patterns
3. **WHEN** processing Vorbis setup headers **THEN** the demuxer **SHALL** preserve all three header packets (ID, comment, setup) like libvorbisfile
4. **WHEN** processing Opus identification headers **THEN** the demuxer **SHALL** use opus_head_parse() equivalent logic for "OpusHead" signature validation
5. **WHEN** processing Opus comment headers **THEN** the demuxer **SHALL** use opus_tags_parse() equivalent logic for "OpusTags" metadata
6. **WHEN** processing FLAC identification headers **THEN** the demuxer **SHALL** validate "\x7fFLAC" signature and extract STREAMINFO block
7. **WHEN** header parsing encounters unknown codec **THEN** the demuxer **SHALL** skip unknown streams like libopusfile (continue with known streams)
8. **WHEN** header parsing is complete **THEN** the demuxer **SHALL** mark streams as ready using ready_state patterns from reference implementations
9. **WHEN** BOS pages are processed **THEN** the demuxer **SHALL** collect all serial numbers like _fetch_headers() in libvorbisfile
10. **WHEN** duplicate serial numbers are found **THEN** the demuxer **SHALL** return OP_EBADHEADER/OV_EBADHEADER like reference implementations
11. **WHEN** header packets span multiple pages **THEN** the demuxer **SHALL** handle continuation using ogg_stream_packetout() patterns

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

### **Requirement 5: Seeking Operations (Following libvorbisfile/libopusfile Patterns)**

**User Story:** As a media player, I want to seek to specific timestamps in Ogg files, so that users can navigate through audio content.

#### **Acceptance Criteria**

1. **WHEN** seeking to a timestamp **THEN** the demuxer **SHALL** use bisection search identical to ov_pcm_seek_page() and op_pcm_seek_page() algorithms
2. **WHEN** performing bisection search **THEN** the demuxer **SHALL** use ogg_sync_pageseek() for page discovery like libvorbisfile, NOT ogg_sync_pageout()
3. **WHEN** seeking backwards **THEN** the demuxer **SHALL** use chunk-based backward scanning with CHUNKSIZE (65536) increments like _get_prev_page()
4. **WHEN** seeking in Opus streams **THEN** the demuxer **SHALL** account for pre-skip samples and 48kHz granule rate per libopusfile patterns
5. **WHEN** seeking in Vorbis streams **THEN** the demuxer **SHALL** handle variable-rate granule position mapping per libvorbisfile patterns
6. **WHEN** bisection finds target page **THEN** the demuxer **SHALL** use ogg_stream_packetpeek() to examine packets without consuming them
7. **WHEN** seek operation completes **THEN** the demuxer **SHALL** NOT resend header packets to decoder (headers sent only once per stream)
8. **WHEN** seek fails **THEN** the demuxer **SHALL** maintain valid state and report error appropriately
9. **WHEN** seeking near stream boundaries **THEN** the demuxer **SHALL** handle edge cases gracefully using _get_prev_page_serial() patterns
10. **WHEN** granule position is invalid (-1) **THEN** the demuxer **SHALL** continue searching like libvorbisfile/libopusfile
11. **WHEN** bisection interval becomes too small **THEN** the demuxer **SHALL** switch to linear forward scanning like reference implementations

### **Requirement 6: Duration Calculation (Following libvorbisfile/libopusfile Patterns)**

**User Story:** As a media player, I want to determine the total duration of Ogg files, so that I can display progress information and enable seeking.

#### **Acceptance Criteria**

1. **WHEN** calculating duration **THEN** the demuxer **SHALL** use op_get_last_page() and _get_prev_page_serial() patterns to find last valid granule position
2. **WHEN** granule positions are available in headers **THEN** the demuxer **SHALL** prefer header-provided total sample counts like libopusfile
3. **WHEN** scanning for last granule **THEN** the demuxer **SHALL** search backwards using chunk-based scanning with exponentially increasing chunk sizes
4. **WHEN** multiple streams exist **THEN** the demuxer **SHALL** use the longest stream for duration calculation per libvorbisfile patterns
5. **WHEN** granule-to-time conversion is needed **THEN** the demuxer **SHALL** use codec-specific sample rates following reference implementations
6. **WHEN** Opus duration is calculated **THEN** the demuxer **SHALL** use opus_granule_sample() equivalent logic with 48kHz granule rate
7. **WHEN** Vorbis duration is calculated **THEN** the demuxer **SHALL** use granule position as sample count at codec sample rate
8. **WHEN** FLAC-in-Ogg duration is calculated **THEN** the demuxer **SHALL** use granule position as sample count like Vorbis
9. **WHEN** duration cannot be determined **THEN** the demuxer **SHALL** report unknown duration rather than incorrect values
10. **WHEN** scanning backwards hits beginning of file **THEN** the demuxer **SHALL** handle gracefully without infinite loops
11. **WHEN** file is truncated or corrupted **THEN** the demuxer **SHALL** use best available granule position information

### **Requirement 7: Error Handling and Robustness (Following Reference Implementation Patterns)**

**User Story:** As a media player, I want the demuxer to handle corrupted or malformed Ogg files gracefully, so that playback doesn't crash or produce incorrect results.

#### **Acceptance Criteria**

1. **WHEN** encountering invalid page headers **THEN** the demuxer **SHALL** skip corrupted pages using ogg_sync_pageseek() negative return handling
2. **WHEN** CRC validation fails **THEN** the demuxer **SHALL** rely on libogg's internal validation and continue like libvorbisfile
3. **WHEN** packet reconstruction fails **THEN** the demuxer **SHALL** return OV_HOLE/OP_HOLE for missing packets like reference implementations
4. **WHEN** codec identification fails **THEN** the demuxer **SHALL** return OP_ENOTFORMAT and continue scanning like libopusfile
5. **WHEN** memory allocation fails **THEN** the demuxer **SHALL** return OP_EFAULT/OV_EFAULT like reference implementations
6. **WHEN** I/O operations fail **THEN** the demuxer **SHALL** return OP_EREAD/OV_EREAD and handle EOF like _get_data() patterns
7. **WHEN** seeking beyond file boundaries **THEN** the demuxer **SHALL** clamp to valid ranges using boundary checking patterns
8. **WHEN** malformed metadata is encountered **THEN** the demuxer **SHALL** parse what's possible and continue like opus_tags_parse()
9. **WHEN** granule position is invalid (-1) **THEN** the demuxer **SHALL** handle like reference implementations (continue searching)
10. **WHEN** stream ends unexpectedly **THEN** the demuxer **SHALL** return OP_EBADLINK/OV_EBADLINK like reference implementations
11. **WHEN** bisection search fails **THEN** the demuxer **SHALL** fall back to linear search or return appropriate error codes
12. **WHEN** page size exceeds maximum **THEN** the demuxer **SHALL** handle using OP_PAGE_SIZE_MAX bounds checking

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

### **Requirement 10: Granule Position Arithmetic (Following libopusfile Patterns)**

**User Story:** As a media player, I want accurate granule position calculations that handle edge cases and overflow conditions properly.

#### **Acceptance Criteria**

1. **WHEN** adding to granule positions **THEN** the demuxer **SHALL** use op_granpos_add() equivalent logic to detect overflow
2. **WHEN** subtracting granule positions **THEN** the demuxer **SHALL** use op_granpos_diff() equivalent logic to handle wraparound
3. **WHEN** comparing granule positions **THEN** the demuxer **SHALL** use op_granpos_cmp() equivalent logic for proper ordering
4. **WHEN** granule position is -1 **THEN** the demuxer **SHALL** treat as invalid/unset like reference implementations
5. **WHEN** granule position wraps to negative **THEN** the demuxer **SHALL** handle correctly per libopusfile patterns
6. **WHEN** granule position arithmetic would overflow **THEN** the demuxer **SHALL** return appropriate error codes
7. **WHEN** Opus granule positions are processed **THEN** the demuxer **SHALL** account for pre-skip in all calculations
8. **WHEN** Vorbis granule positions are processed **THEN** the demuxer **SHALL** use sample-based interpretation
9. **WHEN** FLAC granule positions are processed **THEN** the demuxer **SHALL** use sample-based interpretation like Vorbis

### **Requirement 11: Page Boundary and Packet Continuation (Following libogg Patterns)**

**User Story:** As a media player, I want proper handling of packets that span multiple pages and page boundary conditions.

#### **Acceptance Criteria**

1. **WHEN** packets span multiple pages **THEN** the demuxer **SHALL** use ogg_stream_packetout() to reconstruct complete packets
2. **WHEN** page has continued packet flag **THEN** the demuxer **SHALL** handle using ogg_page_continued() patterns
3. **WHEN** packet is incomplete **THEN** the demuxer **SHALL** wait for continuation pages like reference implementations
4. **WHEN** page segment table is processed **THEN** the demuxer **SHALL** rely on libogg's internal segment handling
5. **WHEN** lacing values indicate packet boundaries **THEN** the demuxer **SHALL** use libogg's packet reconstruction
6. **WHEN** page ends with 255 lacing value **THEN** the demuxer **SHALL** expect continuation on next page
7. **WHEN** EOS page is encountered **THEN** the demuxer **SHALL** finalize any incomplete packets

### **Requirement 12: API Compliance and Integration**

**User Story:** As a PsyMP3 component, I want the Ogg demuxer to integrate seamlessly with the existing demuxer architecture, so that it works consistently with other format demuxers.

#### **Acceptance Criteria**

1. **WHEN** implementing the Demuxer interface **THEN** the OggDemuxer **SHALL** provide all required virtual methods
2. **WHEN** returning stream information **THEN** the demuxer **SHALL** populate StreamInfo structures with accurate codec data
3. **WHEN** providing media chunks **THEN** the demuxer **SHALL** return properly formatted MediaChunk objects
4. **WHEN** reporting position **THEN** the demuxer **SHALL** return timestamps in milliseconds consistently
5. **WHEN** handling IOHandler operations **THEN** the demuxer **SHALL** use the provided IOHandler interface exclusively
6. **WHEN** logging debug information **THEN** the demuxer **SHALL** use the PsyMP3 Debug logging system
7. **WHEN** reporting errors **THEN** the demuxer **SHALL** use consistent error codes and messages across the codebase
8. **WHEN** initializing libogg structures **THEN** the demuxer **SHALL** use proper initialization and cleanup patterns
9. **WHEN** managing ogg_sync_state **THEN** the demuxer **SHALL** use ogg_sync_reset() after seeks like reference implementations
10. **WHEN** managing ogg_stream_state **THEN** the demuxer **SHALL** use ogg_stream_reset_serialno() for stream switching