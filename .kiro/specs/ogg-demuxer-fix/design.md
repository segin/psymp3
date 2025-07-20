# **OGG DEMUXER IMPLEMENTATION DESIGN**

## **Overview**

This design document specifies the implementation of a robust, RFC-compliant Ogg container demuxer for PsyMP3. The demuxer will handle Ogg Vorbis, Ogg Opus, Ogg FLAC, and other Ogg-encapsulated audio formats with proper seeking, streaming, and error handling capabilities.

The design follows a layered architecture:
- **Ogg Container Layer**: Uses libogg for page and packet parsing
- **Logical Stream Layer**: Manages multiple logical bitstreams within the container
- **Codec Detection Layer**: Identifies and configures codec-specific parameters
- **Seeking Layer**: Implements efficient bisection search for timestamp-based seeking
- **Integration Layer**: Bridges with PsyMP3's demuxer architecture

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
    std::string codec_name;        // "vorbis", "opus", "flac", "speex"
    std::string codec_type;        // "audio", "video", "subtitle"
    
    // Header management
    std::vector<OggPacket> header_packets;
    bool headers_complete;
    bool headers_sent;
    size_t next_header_index;
    
    // Audio properties
    uint32_t sample_rate;
    uint16_t channels;
    uint32_t bitrate;
    uint64_t total_samples;
    uint64_t pre_skip;  // Opus-specific
    
    // Metadata
    std::string artist, title, album;
    
    // Packet buffering
    std::deque<OggPacket> m_packet_queue;
    uint64_t total_samples_processed;
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

### **2. Codec Detection Component**

**Purpose**: Identify codec types and parse codec-specific headers

**Key Methods**:
- `std::string identifyCodec(const std::vector<uint8_t>& packet_data)`
- `bool parseVorbisHeaders(OggStream& stream, const OggPacket& packet)`
- `bool parseOpusHeaders(OggStream& stream, const OggPacket& packet)`
- `bool parseFLACHeaders(OggStream& stream, const OggPacket& packet)`

**Codec-Specific Logic**:

**Vorbis**:
- Identification header: `\x01vorbis` signature
- Comment header: `\x03vorbis` signature with Vorbis comments
- Setup header: `\x05vorbis` signature with codec setup data
- Requires all 3 headers for complete initialization

**Opus**:
- Identification header: `OpusHead` signature with channel count and pre-skip
- Comment header: `OpusTags` signature with metadata
- Requires 2 headers for complete initialization
- Uses 48kHz granule rate regardless of output sample rate

**FLAC**:
- Identification header: `\x7fFLAC` signature with STREAMINFO
- Single header contains all necessary information
- Embedded STREAMINFO provides sample rate, channels, total samples
- Granule positions are sample-based (like Vorbis)
- Must be distinguished from native FLAC files (which use different container)

### **3. Seeking Component**

**Purpose**: Implement efficient timestamp-based seeking

**Key Methods**:
- `bool seekTo(uint64_t timestamp_ms)`: Main seeking interface
- `bool seekToPage(uint64_t target_granule, uint32_t stream_id)`: Page-level seeking
- `uint64_t granuleToMs(uint64_t granule, uint32_t stream_id)`: Time conversion
- `uint64_t msToGranule(uint64_t timestamp_ms, uint32_t stream_id)`: Time conversion

**Seeking Algorithm**:
1. Convert timestamp to target granule position
2. Use bisection search to find appropriate page
3. Reset libogg state and stream positions
4. Update position tracking
5. **Do NOT resend headers** - decoder maintains state

**Granule Position Handling**:
- **Vorbis**: Granule = sample number at end of packet
- **Opus**: Granule = 48kHz sample number + pre-skip
- **FLAC**: Granule = sample number at end of packet (same as Vorbis)
- **Speex**: Granule = sample number at end of packet

### **4. Duration Calculation Component**

**Purpose**: Determine total file duration from various sources

**Key Methods**:
- `void calculateDuration()`: Main duration calculation
- `uint64_t getLastGranulePosition()`: Find last valid granule

**Duration Sources (in priority order)**:
1. Header-provided total sample counts
2. Tracked maximum granule position during parsing
3. Last page granule position (requires file scan)
4. Unknown duration (return 0)

**Implementation**:
- Scan backwards from file end for last valid page
- Use manual "OggS" signature detection for robustness
- Convert granule positions to milliseconds using codec-specific rates

### **5. Data Streaming Component**

**Purpose**: Provide continuous packet streaming for playback

**Key Methods**:
- `MediaChunk readChunk()`: Read from best audio stream
- `MediaChunk readChunk(uint32_t stream_id)`: Read from specific stream
- `void fillPacketQueue(uint32_t target_stream_id)`: Buffer management

**Streaming Logic**:
1. Send header packets first (once per stream)
2. Buffer data packets in per-stream queues
3. Maintain packet order within logical bitstreams
4. Handle page boundaries and packet continuation
5. Update position tracking from granule positions

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

## **Error Handling**

### **Container Level Errors**
- **Invalid Ogg signatures**: Skip corrupted pages, continue parsing
- **CRC failures**: Log warnings, attempt to continue
- **Truncated files**: Handle gracefully, report EOF
- **Memory allocation failures**: Return error codes, clean up resources

### **Stream Level Errors**
- **Unknown codecs**: Skip unknown streams, continue with known streams
- **Incomplete headers**: Mark stream as incomplete, exclude from playback
- **Packet reconstruction failures**: Skip corrupted packets, continue stream
- **Invalid granule positions**: Use previous valid position, continue

### **Seeking Errors**
- **Seek beyond file boundaries**: Clamp to valid range, report success
- **No valid pages found**: Seek to beginning, report success
- **Bisection failures**: Fall back to linear search or beginning
- **Stream reset failures**: Reinitialize streams, continue

### **I/O Errors**
- **Read failures**: Propagate IOHandler errors, set EOF state
- **Network timeouts**: Handle gracefully in HTTPIOHandler
- **File access errors**: Report through exception system
- **Buffer overflow**: Implement bounded buffers, prevent crashes

## **Testing Strategy**

### **Unit Tests**
- **Codec detection**: Test signature recognition for all supported codecs
- **Header parsing**: Verify correct extraction of audio parameters and metadata
- **Granule conversion**: Test time conversion accuracy for all codec types
- **Packet reconstruction**: Test handling of packets spanning multiple pages
- **Error conditions**: Test graceful handling of corrupted data

### **Integration Tests**
- **File format compatibility**: Test with various Ogg file types and encoders
- **Seeking accuracy**: Verify seek precision across different file sizes
- **Stream switching**: Test handling of multiplexed streams
- **Memory management**: Verify no leaks during normal and error conditions
- **Performance**: Test with large files and network streams

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