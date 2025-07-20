# **DEMUXER ARCHITECTURE DESIGN**

## **Overview**

This design document specifies the implementation of PsyMP3's modern demuxer architecture, which provides a modular, extensible system for handling various media container formats. The architecture separates container parsing (demuxing) from audio/video decoding (codecs), enabling support for multiple formats through a unified interface.

The design emphasizes modularity, extensibility, and performance while maintaining compatibility with existing PsyMP3 components through bridge interfaces.

## **Architecture**

### **Core Class Hierarchy**

```cpp
// Base demuxer interface
class Demuxer {
public:
    explicit Demuxer(std::unique_ptr<IOHandler> handler);
    virtual ~Demuxer() = default;
    
    // Core interface methods
    virtual bool parseContainer() = 0;
    virtual std::vector<StreamInfo> getStreams() const = 0;
    virtual StreamInfo getStreamInfo(uint32_t stream_id) const = 0;
    virtual MediaChunk readChunk() = 0;
    virtual MediaChunk readChunk(uint32_t stream_id) = 0;
    virtual bool seekTo(uint64_t timestamp_ms) = 0;
    virtual bool isEOF() const = 0;
    virtual uint64_t getDuration() const = 0;
    virtual uint64_t getPosition() const = 0;
    virtual uint64_t getGranulePosition(uint32_t stream_id) const;
    
protected:
    std::unique_ptr<IOHandler> m_handler;
    std::vector<StreamInfo> m_streams;
    uint64_t m_duration_ms = 0;
    uint64_t m_position_ms = 0;
    bool m_parsed = false;
    
    // Helper methods for endianness handling
    template<typename T> T readLE();
    template<typename T> T readBE();
    uint32_t readFourCC();
};

// Concrete demuxer implementations
class OggDemuxer : public Demuxer { /* ... */ };
class ChunkDemuxer : public Demuxer { /* ... */ };
class ISODemuxer : public Demuxer { /* ... */ };
class RawAudioDemuxer : public Demuxer { /* ... */ };
class FLACDemuxer : public Demuxer { /* ... */ };
```

### **Data Structures**

```cpp
// Stream information structure
struct StreamInfo {
    uint32_t stream_id;
    std::string codec_type;        // "audio", "video", "subtitle"
    std::string codec_name;        // "pcm", "mp3", "aac", "flac", etc.
    uint32_t codec_tag;            // Format-specific codec identifier
    
    // Audio-specific properties
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
    uint16_t bits_per_sample = 0;
    uint32_t bitrate = 0;
    
    // Additional codec data
    std::vector<uint8_t> codec_data;
    
    // Timing information
    uint64_t duration_samples = 0;
    uint64_t duration_ms = 0;
    
    // Metadata
    std::string artist;
    std::string title;
    std::string album;
};

// Media chunk structure
struct MediaChunk {
    uint32_t stream_id;
    std::vector<uint8_t> data;
    uint64_t granule_position = 0;     // For Ogg-based formats
    uint64_t timestamp_samples = 0;    // For other formats
    bool is_keyframe = true;           // For audio, usually always true
    uint64_t file_offset = 0;          // Original offset in file
};
```

### **Factory System**

```cpp
class DemuxerFactory {
public:
    static std::unique_ptr<Demuxer> createDemuxer(std::unique_ptr<IOHandler> handler);
    static std::unique_ptr<Demuxer> createDemuxer(std::unique_ptr<IOHandler> handler, 
                                                  const std::string& file_path);
    
private:
    static std::string probeFormat(IOHandler* handler);
    
    // Format detection signatures
    static const std::map<std::string, std::vector<uint8_t>> s_format_signatures;
};

class MediaFactory {
public:
    // Primary factory methods
    static std::unique_ptr<Stream> createStream(const std::string& uri);
    static std::unique_ptr<Stream> createStreamWithMimeType(const std::string& uri, 
                                                           const std::string& mime_type);
    static ContentInfo analyzeContent(const std::string& uri);
    
    // Format registration
    static void registerFormat(const MediaFormat& format, StreamFactory factory);
    static void registerContentDetector(const std::string& format_id, ContentDetector detector);
    
    // Format queries
    static std::vector<MediaFormat> getSupportedFormats();
    static bool supportsFormat(const std::string& format_id);
    static bool supportsExtension(const std::string& extension);
    
private:
    struct FormatRegistration {
        MediaFormat format;
        StreamFactory factory;
        ContentDetector detector;
    };
    
    static std::map<std::string, FormatRegistration> s_formats;
    static std::map<std::string, std::string> s_extension_to_format;
    static std::map<std::string, std::string> s_mime_to_format;
};
```

## **Components and Interfaces**

### **1. Base Demuxer Component**

**Purpose**: Provide unified interface for all container demuxers

**Key Design Decisions**:
- Pure virtual interface ensures consistent API across all demuxers
- IOHandler abstraction enables support for files, HTTP streams, and future protocols
- Template helpers for endianness handling reduce code duplication
- Protected members provide common functionality for derived classes

**Interface Design**:
- `parseContainer()`: One-time initialization and metadata extraction
- `getStreams()`: Query available streams after parsing
- `readChunk()`: Sequential data reading with automatic stream selection
- `seekTo()`: Timestamp-based seeking with millisecond precision
- Position tracking through `getPosition()` and `getDuration()`

### **2. Format Detection Component**

**Purpose**: Automatically identify container formats from file content

**Detection Methods**:
1. **Magic Byte Detection**: Check file headers for format signatures
2. **Extension-Based Detection**: Use file extensions as hints for raw formats
3. **Content Analysis**: Advanced detection for ambiguous cases
4. **MIME Type Hints**: Use HTTP Content-Type headers when available

**Format Signatures**:
```cpp
static const std::map<std::string, std::vector<uint8_t>> s_format_signatures = {
    {"riff", {0x52, 0x49, 0x46, 0x46}},  // "RIFF"
    {"aiff", {0x46, 0x4F, 0x52, 0x4D}},  // "FORM"
    {"ogg",  {0x4F, 0x67, 0x67, 0x53}},  // "OggS"
    {"mp4",  {0x00, 0x00, 0x00, 0x00, 0x66, 0x74, 0x79, 0x70}}, // ftyp box
    {"flac", {0x66, 0x4C, 0x61, 0x43}},  // "fLaC"
};
```

### **3. Concrete Demuxer Implementations**

#### **OggDemuxer**
- **Purpose**: Handle Ogg container format (Vorbis, Opus, FLAC-in-Ogg)
- **Key Features**: libogg integration, multiple logical streams, granule-based seeking
- **Complexity**: High (multiple codecs, complex seeking, stream multiplexing)

#### **ChunkDemuxer**
- **Purpose**: Handle chunk-based formats (RIFF/WAV, AIFF)
- **Key Features**: Universal chunk parsing, endianness handling, format detection
- **Complexity**: Medium (chunk navigation, format variants)

#### **ISODemuxer**
- **Purpose**: Handle ISO Base Media format (MP4, M4A, MOV)
- **Key Features**: Hierarchical box structure, sample tables, multiple tracks
- **Complexity**: High (complex structure, sample-based seeking, multiple codecs)

#### **RawAudioDemuxer**
- **Purpose**: Handle raw audio formats (PCM, A-law, μ-law)
- **Key Features**: Extension-based detection, simple streaming, calculated seeking
- **Complexity**: Low (no container structure, simple calculations)

#### **FLACDemuxer**
- **Purpose**: Handle native FLAC container format
- **Key Features**: Metadata block parsing, seek table support, frame detection
- **Complexity**: Medium (metadata parsing, frame sync detection)

### **4. Integration Bridge Component**

**Purpose**: Bridge new demuxer architecture with legacy Stream interface

```cpp
class DemuxedStream : public Stream {
public:
    explicit DemuxedStream(const TagLib::String& path, uint32_t preferred_stream_id = 0);
    
    // Stream interface implementation
    size_t getData(size_t len, void *buf) override;
    void seekTo(unsigned long pos) override;
    bool eof() override;
    unsigned int getLength() override;
    
    // Extended functionality
    std::vector<StreamInfo> getAvailableStreams() const;
    bool switchToStream(uint32_t stream_id);
    StreamInfo getCurrentStreamInfo() const;
    
private:
    std::unique_ptr<Demuxer> m_demuxer;
    std::unique_ptr<AudioCodec> m_codec;
    uint32_t m_current_stream_id = 0;
    
    // Buffering system
    std::queue<MediaChunk> m_chunk_buffer;
    AudioFrame m_current_frame;
    size_t m_current_frame_offset = 0;
    
    // Position tracking
    uint64_t m_samples_consumed = 0;
    bool m_eof_reached = false;
};
```

### **5. MediaFactory Integration Component**

**Purpose**: Provide comprehensive format detection and stream creation

**Format Registration System**:
```cpp
struct MediaFormat {
    std::string format_id;
    std::string display_name;
    std::vector<std::string> extensions;
    std::vector<std::string> mime_types;
    std::vector<std::string> magic_signatures;
    int priority = 100;
    bool supports_streaming = false;
    bool supports_seeking = true;
    bool is_container = false;
    std::string description;
};
```

**Content Detection Pipeline**:
1. **Extension Detection**: Quick format hints from file extensions
2. **MIME Type Detection**: HTTP Content-Type header analysis
3. **Magic Byte Detection**: Binary signature matching
4. **Content Analysis**: Advanced format-specific detection
5. **Confidence Scoring**: Weighted results for best match selection

## **Data Models**

### **Stream Processing Pipeline**

```
URI → IOHandler → Format Detection → Demuxer Creation → Container Parsing → Stream Selection → Chunk Reading → Codec Decoding → PCM Output
```

### **Seeking Flow**

```
Timestamp Request → Stream Selection → Demuxer Seeking → Position Update → Codec Reset → Resume Playback
```

### **Format Detection Flow**

```
File/URL → IOHandler → Magic Bytes → Format Matching → Demuxer Selection → Container Validation → Success/Failure
```

## **Error Handling**

### **Initialization Errors**
- **Format Detection Failure**: Return nullptr from factory, try fallback detection
- **Container Parsing Failure**: Return false from parseContainer(), log specific errors
- **IOHandler Errors**: Propagate I/O errors, handle network timeouts gracefully
- **Memory Allocation**: Use RAII, handle allocation failures with appropriate cleanup

### **Runtime Errors**
- **Corrupted Data**: Skip damaged sections, continue with valid data
- **Seeking Errors**: Clamp to valid ranges, provide approximate positioning
- **Stream Errors**: Handle individual stream failures without affecting others
- **Threading Issues**: Use proper synchronization, handle race conditions

### **Recovery Strategies**
- **Graceful Degradation**: Continue operation with reduced functionality
- **Error Propagation**: Clear error reporting through exception hierarchy
- **State Consistency**: Maintain valid state even after errors
- **Resource Cleanup**: Ensure proper cleanup in all error paths

## **Performance Considerations**

### **Memory Management**
- **Streaming Architecture**: Process data incrementally without loading entire files
- **Bounded Buffers**: Prevent memory exhaustion with configurable buffer limits
- **Resource Pooling**: Reuse buffers and objects where possible
- **Smart Pointers**: Use RAII for automatic resource management

### **I/O Optimization**
- **Sequential Access**: Optimize for forward reading patterns
- **Read-Ahead Buffering**: Minimize I/O operations for network streams
- **Seek Optimization**: Use format-specific seeking strategies
- **Cache Management**: Cache frequently accessed metadata and seek points

### **CPU Efficiency**
- **Format Detection**: Fast magic byte matching with early termination
- **Parsing Optimization**: Efficient parsing algorithms for common formats
- **Threading**: Asynchronous processing where beneficial
- **Code Paths**: Optimize common operations, handle edge cases separately

### **Scalability**
- **Large Files**: Support files >2GB with 64-bit addressing
- **Multiple Streams**: Handle reasonable numbers of concurrent streams
- **Network Streams**: Efficient handling of HTTP streams with range requests
- **Plugin Architecture**: Support for dynamically loaded format handlers

## **Thread Safety**

### **Synchronization Strategy**
- **Per-Instance State**: Each demuxer instance maintains independent state
- **Shared Resources**: Protect factory registrations and global state
- **I/O Operations**: Coordinate access to IOHandler instances
- **Buffer Management**: Thread-safe buffer operations where needed

### **Concurrency Model**
- **Single-Threaded Demuxers**: Individual demuxer instances are not thread-safe
- **Factory Thread Safety**: Format registration and creation are thread-safe
- **Bridge Synchronization**: DemuxedStream handles threading for legacy compatibility
- **Error Propagation**: Thread-safe error state management

## **Extensibility**

### **Plugin Architecture**
- **Dynamic Registration**: Runtime format registration through MediaFactory
- **Custom Demuxers**: Easy addition of new container format support
- **Content Detectors**: Pluggable format detection algorithms
- **Stream Factories**: Custom stream creation logic

### **Future Enhancements**
- **Video Support**: Architecture ready for video demuxing extensions
- **Subtitle Support**: Framework for subtitle stream handling
- **Network Protocols**: Support for additional streaming protocols
- **Metadata Standards**: Extensible metadata handling system

This architecture provides a solid foundation for media container handling that is both powerful and maintainable, with clear separation of concerns and excellent extensibility for future enhancements.