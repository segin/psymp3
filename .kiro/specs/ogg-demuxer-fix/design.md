# **OGG DEMUXER IMPLEMENTATION DESIGN**

## **Overview**

This design document specifies the implementation of a robust, RFC-compliant Ogg container demuxer for PsyMP3 that **exactly follows the behavior patterns of libvorbisfile and libopusfile reference implementations**. The demuxer will handle Ogg Vorbis, Ogg Opus, Ogg FLAC, and other Ogg-encapsulated audio formats with proper seeking, streaming, and error handling capabilities.

The design follows a layered architecture based on reference implementation patterns:
- **Ogg Container Layer**: Uses libogg API patterns identical to libvorbisfile/libopusfile
- **Logical Stream Layer**: Manages multiple logical bitstreams using _fetch_headers() patterns
- **Codec Detection Layer**: Identifies codecs using opus_head_parse()/vorbis_synthesis_idheader() patterns
- **Seeking Layer**: Implements bisection search identical to ov_pcm_seek_page()/op_pcm_seek_page()
- **Granule Position Layer**: Handles arithmetic using libopusfile's overflow-safe patterns
- **Integration Layer**: Bridges with PsyMP3's demuxer architecture

**CRITICAL DESIGN PRINCIPLE**: All algorithms, error handling, and API usage must match the reference implementations exactly to prevent compatibility issues and bugs that have already been solved in the mature reference code.

**Note**: This OggDemuxer handles FLAC-in-Ogg streams (`.oga` files), which are different from native FLAC files (`.flac` files). The existing `Flac` class in `src/flac.cpp` handles native FLAC container format and should be refactored into separate `FLACDemuxer` and `FLACCodec` components to support both native FLAC and FLAC-in-Ogg through a unified codec interface.

## **Architecture**

### **Class Structure**

```cpp
class OggDemuxer : public Demuxer {
private:
    // Core libogg structures
    ogg_sync_state m_sync_state;
    std::map<uint32_t, ogg_stream_state> m_ogg_streams;
    
    // Stream management
    std::map<uint32_t, OggStream> m_streams;
    uint64_t m_file_size;
    uint64_t m_duration_ms;
    uint64_t m_position_ms;
    bool m_eof;
    
    // Seeking optimization
    uint64_t m_max_granule_seen;
    
public:
    // Demuxer interface implementation
    bool parseContainer() override;
    std::vector<StreamInfo> getStreams() const override;
    MediaChunk readChunk() override;
    MediaChunk readChunk(uint32_t stream_id) override;
    bool seekTo(uint64_t timestamp_ms) override;
    // ... other interface methods
};
```

### **Supporting Data Structures**

```cpp
struct OggStream {
    uint32_t serial_number;
    std::string codec_name;        // "vorbis", "opus", "flac", "speex", "theora"
    std::string codec_type;        // "audio", "video", "subtitle"
    
    // Header management
    std::vector<OggPacket> header_packets;
    bool headers_complete;
    bool headers_sent;
    size_t next_header_index;
    uint16_t expected_header_count; // FLAC-in-Ogg: from identification header (0 = unknown)
    
    // Audio properties
    uint32_t sample_rate;
    uint16_t channels;
    uint32_t bitrate;
    uint64_t total_samples;
    uint64_t pre_skip;              // Opus-specific: samples to discard at start
    uint8_t bits_per_sample;        // FLAC-specific: bit depth
    
    // FLAC-in-Ogg specific
    uint8_t flac_mapping_version_major;  // Should be 1
    uint8_t flac_mapping_version_minor;  // Should be 0
    uint16_t flac_min_block_size;
    uint16_t flac_max_block_size;
    uint32_t flac_min_frame_size;
    uint32_t flac_max_frame_size;
    
    // Metadata
    std::string artist, title, album;
    
    // Packet buffering
    std::deque<OggPacket> m_packet_queue;
    uint64_t total_samples_processed;
    
    // Page sequence tracking
    uint32_t last_page_sequence;    // For detecting page loss
    bool page_sequence_initialized;
};

struct OggPacket {
    uint32_t stream_id;
    std::vector<uint8_t> data;
    uint64_t granule_position;
    bool is_first_packet;
    bool is_last_packet;
};
```

## **Components and Interfaces**

### **1. Container Parsing Component**

**Purpose**: Parse Ogg container structure using libogg

**Key Methods**:
- `bool parseContainer()`: Main parsing entry point
- `bool readIntoSyncBuffer(size_t bytes)`: Buffer management
- `bool processPages()`: Page extraction and processing

**Design Decisions**:
- Use libogg's `ogg_sync_state` for robust page parsing
- Separate header parsing from data streaming phases
- Implement bounded header parsing to prevent infinite loops
- Reset sync state after header parsing for sequential reading

**Error Handling**:
- Validate page headers and checksums
- Skip corrupted pages and continue processing
- Handle incomplete packets gracefully
- Limit header parsing iterations to prevent hangs

### **2. Codec Detection Component (Following Reference Implementation Patterns)**

**Purpose**: Identify codec types and parse codec-specific headers using reference implementation logic

**Key Methods**:
- `std::string identifyCodec(const std::vector<uint8_t>& packet_data)`: Codec signature detection
- `bool parseVorbisHeaders(OggStream& stream, const OggPacket& packet)`: Vorbis header parsing
- `bool parseOpusHeaders(OggStream& stream, const OggPacket& packet)`: Opus header parsing  
- `bool parseFLACHeaders(OggStream& stream, const OggPacket& packet)`: FLAC header parsing

**Codec Detection Logic (Following Reference Patterns)**:

**Vorbis (Following libvorbisfile patterns)**:
- Use vorbis_synthesis_idheader() equivalent logic for `\x01vorbis` signature validation
- Use vorbis_synthesis_headerin() equivalent logic for header processing
- Identification header: Extract sample rate, channels, bitrate from packet
- Comment header: `\x03vorbis` signature with UTF-8 metadata parsing
- Setup header: `\x05vorbis` signature with codec setup data preservation
- **Requires all 3 headers for complete initialization** (libvorbisfile pattern)
- Handle header parsing errors like libvorbisfile (return OV_EBADHEADER)

**Opus (Following libopusfile patterns)**:
- Use opus_head_parse() equivalent logic for "OpusHead" signature validation
- Use opus_tags_parse() equivalent logic for metadata extraction
- Identification header: Extract channels, pre-skip, input_sample_rate, channel_mapping
- Comment header: "OpusTags" signature with metadata parsing
- **Requires 2 headers for complete initialization** (libopusfile pattern)
- Uses 48kHz granule rate regardless of output sample rate
- Handle header parsing errors like libopusfile (return OP_EBADHEADER)

**FLAC-in-Ogg (Following RFC 9639 Section 10.1)**:
- Identification header: `\x7fFLAC` (0x7F 0x46 0x4C 0x41 0x43) 5-byte signature validation
- **First packet structure (51 bytes total)**:
  - 5 bytes: Signature `\x7fFLAC`
  - 2 bytes: Mapping version (0x01 0x00 for version 1.0)
  - 2 bytes: Header packet count (big-endian, 0 = unknown)
  - 4 bytes: fLaC signature (0x664C6143)
  - 4 bytes: Metadata block header for STREAMINFO
  - 34 bytes: STREAMINFO metadata block
- **First page is always exactly 79 bytes** (27-byte header + 1 lacing value + 51-byte packet)
- **Header packets**: One or more metadata blocks following identification
- **First header packet SHOULD be Vorbis comment** for historic compatibility
- **Audio packets**: Each packet contains exactly one FLAC frame
- **Granule positions**: Sample count (interchannel samples), 0 for header pages
- **Special granule value**: 0xFFFFFFFFFFFFFFFF when no packet completes on page
- **Chaining**: Audio property changes require new stream (EOS followed by BOS)
- Must be distinguished from native FLAC files (different container format)

**Unknown Codecs**:
- Return OP_ENOTFORMAT and continue scanning like libopusfile
- Skip unknown streams without affecting other streams
- Log codec signature for debugging purposes

### **3. FLAC-in-Ogg Handler Component (Following RFC 9639 Section 10.1)**

**Purpose**: Handle FLAC audio encapsulated in Ogg containers per RFC 9639 Section 10.1

**Key Methods**:
- `bool parseFLACInOggHeader(OggStream& stream, const OggPacket& packet)`: Parse identification header
- `bool extractFLACStreamInfo(OggStream& stream, const uint8_t* data, size_t size)`: Extract STREAMINFO
- `uint64_t flacGranuleToSamples(uint64_t granule)`: Granule position interpretation

**FLAC-in-Ogg Identification Header Structure**:
```cpp
struct FLACInOggHeader {
    uint8_t signature[5];      // "\x7fFLAC" (0x7F 0x46 0x4C 0x41 0x43)
    uint8_t version_major;     // 0x01 for version 1.0
    uint8_t version_minor;     // 0x00 for version 1.0
    uint16_t header_count;     // Big-endian, 0 = unknown
    uint8_t flac_signature[4]; // "fLaC" (0x66 0x4C 0x61 0x43)
    uint8_t metadata_header[4]; // Metadata block header
    uint8_t streaminfo[34];    // STREAMINFO metadata block
};
// Total: 51 bytes, first page always 79 bytes
```

**STREAMINFO Extraction**:
- Minimum block size (16 bits)
- Maximum block size (16 bits)
- Minimum frame size (24 bits)
- Maximum frame size (24 bits)
- Sample rate (20 bits)
- Number of channels minus 1 (3 bits)
- Bits per sample minus 1 (5 bits)
- Total samples (36 bits)
- MD5 signature (128 bits)

**Granule Position Handling**:
- Header pages: Granule position MUST be 0
- Audio pages with completed packet: Sample count at end of last completed packet
- Audio pages without completed packet: 0xFFFFFFFFFFFFFFFF
- Sample numbering: Interchannel samples (not per-channel)

**Version Handling**:
- Version 1.0 (0x01 0x00): Fully supported
- Unknown versions: Log warning, attempt parsing, may fail gracefully

**Error Conditions**:
- Invalid signature: Return OP_ENOTFORMAT
- Unsupported version: Return OP_EVERSION (or equivalent)
- Malformed STREAMINFO: Return OP_EBADHEADER
- First page not 79 bytes: Log warning, continue parsing

### **4. Seeking Component (Following ov_pcm_seek_page/op_pcm_seek_page Patterns)**

**Purpose**: Implement efficient timestamp-based seeking identical to reference implementations

**Key Methods**:
- `bool seekTo(uint64_t timestamp_ms)`: Main seeking interface (follows ov_pcm_seek/op_pcm_seek)
- `bool seekToPage(uint64_t target_granule, uint32_t stream_id)`: Page-level seeking (follows ov_pcm_seek_page/op_pcm_seek_page)
- `uint64_t granuleToMs(uint64_t granule, uint32_t stream_id)`: Time conversion
- `uint64_t msToGranule(uint64_t timestamp_ms, uint32_t stream_id)`: Time conversion

**Bisection Search Algorithm (Identical to Reference Implementations)**:
1. Convert timestamp to target granule position using codec-specific logic
2. Initialize bisection interval (begin, end) like libvorbisfile/libopusfile
3. **Use ogg_sync_pageseek() for page discovery** (NOT ogg_sync_pageout())
4. Bisect interval using (begin + end) / 2 calculation
5. Read pages forward using _get_next_page() patterns
6. Compare granule positions using op_granpos_cmp() equivalent logic
7. Adjust interval based on comparison results
8. **Switch to linear scanning when interval becomes small**
9. Use ogg_stream_packetpeek() to examine packets without consuming
10. Reset ogg_sync_state and stream positions after seek
11. **Do NOT resend headers** - decoder maintains state

**Backward Scanning (Following _get_prev_page Patterns)**:
- Use chunk-based backward scanning with CHUNKSIZE (65536) increments
- Exponentially increase chunk size for efficiency (like libopusfile)
- Handle file beginning boundary conditions
- Prefer pages with matching serial numbers

**Granule Position Handling (Reference Implementation Patterns)**:
- **Vorbis**: Granule = sample number at end of packet (direct mapping)
- **Opus**: Granule = 48kHz sample number, account for pre-skip using opus_granule_sample() logic
- **FLAC**: Granule = sample number at end of packet (same as Vorbis)
- **Invalid (-1)**: Handle like reference implementations (continue searching)

### **5. Duration Calculation Component (Following op_get_last_page Patterns)**

**Purpose**: Determine total file duration using reference implementation patterns

**Key Methods**:
- `void calculateDuration()`: Main duration calculation
- `uint64_t getLastGranulePosition()`: Find last valid granule (follows op_get_last_page)
- `int64_t getPrevPageSerial()`: Backward page scanning (follows _get_prev_page_serial)

**Duration Sources (Priority Order from Reference Implementations)**:
1. Header-provided total sample counts (libopusfile pattern)
2. Tracked maximum granule position during parsing
3. Last page granule position using backward scanning
4. Unknown duration (return 0)

**Backward Scanning Implementation (Following Reference Patterns)**:
- Use chunk-based backward scanning with exponentially increasing chunk sizes
- Start with OP_CHUNK_SIZE (65536), increase up to OP_CHUNK_SIZE_MAX
- Scan forward from each chunk start to find valid pages
- Prefer pages with matching serial numbers
- Handle file beginning boundary conditions gracefully
- Use ogg_page_granulepos() to extract granule positions
- Continue until valid granule position found or file beginning reached

**Granule-to-Time Conversion**:
- **Vorbis**: Direct sample count / sample_rate conversion
- **Opus**: Use opus_granule_sample() equivalent logic (48kHz granule rate, account for pre-skip)
- **FLAC-in-Ogg**: Direct sample count / sample_rate conversion (like Vorbis)
  - Sample rate from STREAMINFO metadata block
  - Total samples from STREAMINFO if available (36-bit field)
  - Granule position represents interchannel sample count

### **6. Granule Position Arithmetic Component (Following libopusfile Patterns)**

**Purpose**: Handle granule position calculations with overflow protection

**Key Methods**:
- `int granposAdd(int64_t* dst_gp, int64_t src_gp, int32_t delta)`: Safe addition (follows op_granpos_add)
- `int granposDiff(int64_t* delta, int64_t gp_a, int64_t gp_b)`: Safe subtraction (follows op_granpos_diff)
- `int granposCmp(int64_t gp_a, int64_t gp_b)`: Safe comparison (follows op_granpos_cmp)

**Granule Position Arithmetic (libopusfile Patterns)**:
- **Invalid Value**: -1 in two's complement indicates invalid/unset granule position
- **Overflow Detection**: Check for wraparound past -1 value in all operations
- **Wraparound Handling**: Handle negative granule positions correctly (larger than positive)
- **Range Organization**: [ -1 (invalid) ][ 0 ... INT64_MAX ][ INT64_MIN ... -2 ][ -1 (invalid) ]

**Safe Addition Logic**:
- Detect overflow that would wrap past -1
- Handle positive and negative granule position ranges
- Return appropriate error codes on overflow

**Safe Subtraction Logic**:
- Handle wraparound from positive to negative values
- Detect underflow conditions
- Maintain proper ordering semantics

### **7. Data Streaming Component (Following Reference Implementation Patterns)**

**Purpose**: Provide continuous packet streaming for playback using reference patterns

**Key Methods**:
- `MediaChunk readChunk()`: Read from best audio stream
- `MediaChunk readChunk(uint32_t stream_id)`: Read from specific stream
- `void fillPacketQueue(uint32_t target_stream_id)`: Buffer management
- `int fetchAndProcessPacket()`: Packet processing (follows _fetch_and_process_packet)

**Streaming Logic (Following libvorbisfile/libopusfile Patterns)**:
1. Send header packets first (once per stream, never resend after seeks)
2. Use ogg_stream_packetout() for packet extraction from pages
3. Buffer data packets in per-stream queues with bounded sizes
4. Maintain packet order within logical bitstreams
5. Handle page boundaries using ogg_stream_pagein() patterns
6. Update position tracking from granule positions using safe arithmetic
7. Handle packet holes (return OV_HOLE/OP_HOLE) like reference implementations
8. Use ogg_sync_pageseek() for page discovery during streaming

## **Data Models**

### **Stream State Machine**

```
[Uninitialized] → [Headers Parsing] → [Headers Complete] → [Streaming Data]
                                   ↓
                              [Headers Sent] → [Active Streaming]
                                   ↑
                              [After Seek] (no header resend)
```

### **Packet Flow**

```
File → IOHandler → ogg_sync_state → ogg_page → ogg_stream_state → ogg_packet → OggPacket → MediaChunk
```

### **Seeking Flow**

```
Timestamp → Target Granule → Bisection Search → Page Position → Stream Reset → Continue Streaming
```

## **Correctness Properties**

*A property is a characteristic or behavior that should hold true across all valid executions of a system-essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Property 1: OggS Capture Pattern Validation
*For any* byte sequence, the demuxer SHALL accept it as a valid Ogg page start if and only if the first 4 bytes are exactly "OggS" (0x4f 0x67 0x67 0x53).
**Validates: Requirements 1.1**

### Property 2: Page Version Validation
*For any* Ogg page header, the demuxer SHALL accept it as valid if and only if the stream_structure_version byte is 0.
**Validates: Requirements 1.2**

### Property 3: Page Size Bounds
*For any* Ogg page, the demuxer SHALL reject it as invalid if the total page size exceeds 65,307 bytes.
**Validates: Requirements 1.11**

### Property 4: Lacing Value Interpretation
*For any* segment table, the demuxer SHALL interpret a lacing value of 255 as packet continuation and a lacing value less than 255 as packet termination.
**Validates: Requirements 2.4, 2.5, 13.6**

### Property 5: Codec Signature Detection
*For any* BOS packet, the demuxer SHALL correctly identify the codec type based on the magic bytes: "\x01vorbis" for Vorbis, "OpusHead" for Opus, "\x7fFLAC" for FLAC, "Speex   " for Speex, "\x80theora" for Theora.
**Validates: Requirements 3.1, 3.2, 3.3, 3.4, 3.5, 3.6**

### Property 6: FLAC-in-Ogg Header Structure
*For any* valid FLAC-in-Ogg stream, the first page SHALL be exactly 79 bytes and the identification header SHALL contain the 5-byte signature, 2-byte version, 2-byte header count, 4-byte fLaC signature, 4-byte metadata header, and 34-byte STREAMINFO.
**Validates: Requirements 4.9, 5.2**

### Property 7: Page Sequence Tracking
*For any* logical bitstream, the demuxer SHALL detect and report when page sequence numbers are non-consecutive (indicating page loss).
**Validates: Requirements 1.6, 6.8**

### Property 8: Grouped Stream Ordering
*For any* grouped Ogg bitstream, all BOS pages SHALL appear before any data pages.
**Validates: Requirements 3.7**

### Property 9: Chained Stream Detection
*For any* chained Ogg bitstream, the demuxer SHALL detect stream boundaries where an EOS page is immediately followed by a BOS page.
**Validates: Requirements 3.8**

### Property 10: Granule Position Arithmetic Safety
*For any* granule position operations, the demuxer SHALL:
- Detect overflow when adding to granule positions
- Handle wraparound correctly when subtracting granule positions
- Maintain proper ordering when comparing granule positions
- Treat -1 as invalid/unset
**Validates: Requirements 12.1, 12.2, 12.3, 12.4**

### Property 11: Invalid Granule Handling
*For any* page with granule position -1, the demuxer SHALL continue searching for valid granule positions rather than treating -1 as a valid position.
**Validates: Requirements 7.10, 9.9**

### Property 12: Multi-Page Packet Reconstruction
*For any* packet spanning multiple pages, the demuxer SHALL correctly reconstruct the complete packet by accumulating segments across pages using continuation flags.
**Validates: Requirements 13.1, 2.7**

### Property 13: Seeking Accuracy
*For any* seek operation to a target timestamp, the demuxer SHALL land on a page whose granule position is at or before the target, and the next page's granule position is after the target.
**Validates: Requirements 7.1**

### Property 14: Duration Calculation Consistency
*For any* Ogg stream, the calculated duration SHALL equal (last_granule_position - pre_skip) / sample_rate for Opus, or last_granule_position / sample_rate for Vorbis and FLAC-in-Ogg.
**Validates: Requirements 8.6, 8.7, 8.8**

### Property 15: Bounded Queue Memory
*For any* packet buffering operation, the demuxer SHALL enforce queue size limits to prevent unbounded memory growth.
**Validates: Requirements 10.2**

### Property 16: Thread Safety
*For any* concurrent access to the demuxer from multiple threads, shared state modifications SHALL be protected by appropriate synchronization primitives.
**Validates: Requirements 11.1**

### Property 17: Position Reporting Consistency
*For any* position query, the demuxer SHALL return timestamps in milliseconds, calculated consistently from granule positions using codec-specific sample rates.
**Validates: Requirements 14.4**

## **Error Handling (Following Reference Implementation Patterns)**

### **Container Level Errors (libvorbisfile/libopusfile Patterns)**
- **Invalid Ogg signatures**: Use memcmp() validation, return OP_ENOTFORMAT like libopusfile
- **CRC failures**: Rely on libogg's internal validation, continue like reference implementations
- **Truncated files**: Return OP_EOF/OV_EOF, handle gracefully
- **Memory allocation failures**: Return OP_EFAULT/OV_EFAULT, clean up resources

### **Stream Level Errors (Reference Implementation Patterns)**
- **Unknown codecs**: Return OP_ENOTFORMAT, continue scanning like libopusfile
- **Incomplete headers**: Return OP_EBADHEADER/OV_EBADHEADER like reference implementations
- **Packet reconstruction failures**: Return OV_HOLE/OP_HOLE for missing packets
- **Invalid granule positions (-1)**: Continue searching like reference implementations
- **Duplicate serial numbers**: Return OP_EBADHEADER/OV_EBADHEADER

### **Seeking Errors (Reference Implementation Patterns)**
- **Seek beyond file boundaries**: Clamp to valid range using boundary checking
- **No valid pages found**: Return OP_EBADLINK/OV_EBADLINK like reference implementations
- **Bisection failures**: Fall back to linear search like ov_pcm_seek_page
- **Stream reset failures**: Return appropriate error codes, maintain state
- **Granule position overflow**: Return OP_EINVAL using safe arithmetic

### **I/O Errors (Reference Implementation Patterns)**
- **Read failures**: Return OP_EREAD/OV_EREAD, propagate IOHandler errors
- **Network timeouts**: Handle gracefully in HTTPIOHandler
- **File access errors**: Return OP_EREAD/OV_EREAD through exception system
- **Buffer overflow**: Use bounded buffers with OP_PAGE_SIZE_MAX limits
- **EOF conditions**: Return OP_FALSE/OV_EOF like _get_data() patterns

### **Page Processing Errors (libogg Patterns)**
- **ogg_sync_pageseek() negative return**: Handle as skipped bytes like libvorbisfile
- **ogg_sync_pageout() failure**: Request more data like reference implementations
- **ogg_stream_packetout() holes**: Return OV_HOLE/OP_HOLE like reference implementations
- **Page size exceeds maximum**: Use OP_PAGE_SIZE_MAX bounds checking

## **Testing Strategy**

### **Dual Testing Approach**
This implementation uses both unit tests and property-based tests:
- **Unit tests** verify specific examples, edge cases, and error conditions
- **Property-based tests** verify universal properties that should hold across all inputs
- Together they provide comprehensive coverage: unit tests catch concrete bugs, property tests verify general correctness

### **Property-Based Testing Framework**
- **Library**: RapidCheck (C++ property-based testing library)
- **Minimum iterations**: 100 per property test
- **Test annotation format**: `// **Feature: ogg-demuxer-fix, Property N: <property_text>**`

### **Unit Tests**
- **Codec detection**: Test signature recognition for all supported codecs (Vorbis, Opus, FLAC, Speex, Theora)
- **Header parsing**: Verify correct extraction of audio parameters and metadata
- **Granule conversion**: Test time conversion accuracy for all codec types
- **Packet reconstruction**: Test handling of packets spanning multiple pages
- **Error conditions**: Test graceful handling of corrupted data
- **FLAC-in-Ogg specifics**: Test 79-byte first page, STREAMINFO extraction, version validation

### **Property-Based Tests**
Each correctness property from the design document SHALL be implemented as a property-based test:
- **Property 1-3**: Page structure validation properties
- **Property 4**: Lacing value interpretation property
- **Property 5-6**: Codec detection and FLAC-in-Ogg header properties
- **Property 7-9**: Stream multiplexing properties
- **Property 10-11**: Granule position arithmetic properties
- **Property 12-13**: Packet reconstruction and seeking properties
- **Property 14-17**: Duration, memory, threading, and position properties

### **Integration Tests**
- **File format compatibility**: Test with various Ogg file types and encoders
- **Seeking accuracy**: Verify seek precision across different file sizes
- **Stream switching**: Test handling of multiplexed streams
- **Memory management**: Verify no leaks during normal and error conditions
- **Performance**: Test with large files and network streams
- **FLAC-in-Ogg integration**: Test with real .oga files containing FLAC audio

### **Regression Tests**
- **Known problematic files**: Maintain test suite of previously failing files
- **Edge cases**: Test very short files, single-packet files, empty files
- **Boundary conditions**: Test seeking to start, end, and invalid positions
- **Codec variations**: Test different encoder settings and versions

### **Compatibility Tests**
- **Reference implementations**: Compare behavior with libvorbisfile and opusfile
- **Cross-platform**: Test on Windows, Linux, macOS with different compilers
- **Endianness**: Test on big-endian systems if available
- **Threading**: Test concurrent access patterns

## **Performance Considerations**

### **Memory Usage**
- **Bounded packet queues**: Limit queue sizes to prevent memory exhaustion
- **Header caching**: Store parsed headers efficiently
- **Buffer reuse**: Minimize allocation/deallocation in hot paths
- **Stream cleanup**: Properly release libogg resources

### **I/O Efficiency**
- **Read-ahead buffering**: Use appropriate buffer sizes for network streams
- **Seek optimization**: Minimize I/O operations during bisection search
- **Page caching**: Cache recently accessed pages for repeated seeks
- **Sequential optimization**: Optimize for forward playback patterns

### **CPU Efficiency**
- **Minimal copying**: Avoid unnecessary data copying in packet handling
- **Efficient parsing**: Use optimized parsing for common header types
- **Lazy evaluation**: Parse metadata only when requested
- **Branch prediction**: Structure code for common cases

### **Scalability**
- **Large files**: Handle files >2GB efficiently
- **Many streams**: Support reasonable numbers of multiplexed streams
- **Long duration**: Handle very long audio files without overflow
- **High bitrates**: Support high-bitrate streams without buffering issues

## **Security Considerations**

### **Input Validation**
- **Header size limits**: Validate header sizes to prevent buffer overflows
- **Packet size limits**: Limit packet sizes to reasonable bounds
- **Loop prevention**: Prevent infinite loops in parsing logic
- **Integer overflow**: Check for overflow in size calculations

### **Memory Safety**
- **Buffer bounds**: Always check buffer boundaries before access
- **Null pointer checks**: Validate pointers before dereferencing
- **Resource cleanup**: Ensure cleanup in all error paths
- **Exception safety**: Maintain consistent state during exceptions

### **Denial of Service Prevention**
- **Parse time limits**: Limit time spent parsing headers
- **Memory limits**: Prevent excessive memory allocation
- **Recursion limits**: Limit recursion depth in parsing
- **Rate limiting**: Consider rate limiting for network streams

This design provides a robust, efficient, and maintainable implementation of Ogg demuxing that addresses the current seeking issues while providing a solid foundation for future enhancements.