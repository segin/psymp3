# PsyMP3 Demuxer Architecture API Documentation

## Overview

The PsyMP3 Demuxer Architecture provides a modular, extensible system for handling various media container formats. It separates container parsing (demuxing) from audio/video decoding (codecs), enabling support for multiple formats through a unified interface.

## Core Components

### Base Demuxer Interface

The `Demuxer` class serves as the foundation for all container demuxers in PsyMP3.

#### Class: `Demuxer`

**Purpose**: Abstract base class providing a unified interface for all media container demuxers.

**Thread Safety**: Individual demuxer instances are **not** thread-safe. Use separate instances for concurrent access or implement external synchronization.

```cpp
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
    
    // Error handling
    const DemuxerError& getLastError() const;
    bool hasError() const;
    void clearError();
};
```

#### Core Methods

##### `parseContainer()`
**Purpose**: Parse the container headers and identify available streams.

**Returns**: `true` if container was successfully parsed, `false` on error.

**Usage**: Must be called once after construction before accessing streams.

**Error Handling**: Returns `false` on failure. Use `getLastError()` for details.

**Example**:
```cpp
auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
if (!demuxer->parseContainer()) {
    std::cerr << "Failed to parse container: " << demuxer->getLastError().message << std::endl;
    return;
}
```

##### `getStreams()`
**Purpose**: Get information about all streams in the container.

**Returns**: Vector of `StreamInfo` objects describing each stream.

**Preconditions**: `parseContainer()` must have been called successfully.

**Thread Safety**: Safe to call concurrently after parsing is complete.

##### `readChunk()` / `readChunk(uint32_t stream_id)`
**Purpose**: Read the next chunk of media data.

**Parameters**:
- `stream_id` (optional): Specific stream to read from. If omitted, reads from any available stream.

**Returns**: `MediaChunk` containing data, or empty chunk if EOF.

**Thread Safety**: **Not thread-safe**. Serialize calls or use separate demuxer instances.

**Example**:
```cpp
while (!demuxer->isEOF()) {
    MediaChunk chunk = demuxer->readChunk();
    if (chunk.isValid()) {
        // Process chunk data
        processAudioData(chunk.data, chunk.stream_id);
    }
}
```

##### `seekTo(uint64_t timestamp_ms)`
**Purpose**: Seek to a specific time position in the media.

**Parameters**:
- `timestamp_ms`: Target time in milliseconds

**Returns**: `true` if seek was successful, `false` on error.

**Behavior**: 
- Seeks to the nearest keyframe at or before the target time
- Resets internal state and prepares for reading from the new position
- May require codec reset in the calling code

**Error Handling**: Returns `false` if seeking fails or timestamp is out of range.

### Data Structures

#### `StreamInfo`
**Purpose**: Comprehensive metadata about a media stream within a container.

```cpp
struct StreamInfo {
    uint32_t stream_id;           // Unique stream identifier
    std::string codec_type;       // "audio", "video", "subtitle"
    std::string codec_name;       // "pcm", "mp3", "aac", "flac", etc.
    uint32_t codec_tag;           // Format-specific codec identifier
    
    // Audio-specific properties
    uint32_t sample_rate;         // Samples per second
    uint16_t channels;            // Number of audio channels
    uint16_t bits_per_sample;     // Bits per sample
    uint32_t bitrate;             // Average bitrate in bits/second
    
    // Codec configuration
    std::vector<uint8_t> codec_data; // Decoder initialization data
    
    // Timing information
    uint64_t duration_samples;    // Duration in samples
    uint64_t duration_ms;         // Duration in milliseconds
    
    // Metadata
    std::string artist;           // Track artist
    std::string title;            // Track title
    std::string album;            // Album name
    
    // Utility methods
    bool isValid() const;         // Check if stream info is complete
    bool isAudio() const;         // Check if this is an audio stream
    bool isVideo() const;         // Check if this is a video stream
    bool isSubtitle() const;      // Check if this is a subtitle stream
};
```

#### `MediaChunk`
**Purpose**: Container for media data with associated metadata and optimized memory management.

```cpp
struct MediaChunk {
    uint32_t stream_id;           // Stream this chunk belongs to
    std::vector<uint8_t> data;    // Raw media data
    uint64_t granule_position;    // Ogg-specific timing (0 for other formats)
    uint64_t timestamp_samples;   // Timestamp in samples
    bool is_keyframe;             // Whether this is a keyframe (usually true for audio)
    uint64_t file_offset;         // Original file offset (for seeking optimization)
    
    // Utility methods
    bool isValid() const;         // Check if chunk contains valid data
    size_t getDataSize() const;   // Get size of data in bytes
    bool isEmpty() const;         // Check if chunk is empty
    void clear();                 // Clear all chunk data
};
```

**Memory Management**: `MediaChunk` uses an internal buffer pool for efficient memory reuse. Large buffers are automatically returned to the pool when the chunk is destroyed.

### Factory System

#### `DemuxerFactory`
**Purpose**: Factory for creating appropriate demuxer instances based on format detection.

**Thread Safety**: Factory methods are thread-safe and can be called concurrently.

```cpp
class DemuxerFactory {
public:
    // Primary factory methods
    static std::unique_ptr<Demuxer> createDemuxer(std::unique_ptr<IOHandler> handler);
    static std::unique_ptr<Demuxer> createDemuxer(std::unique_ptr<IOHandler> handler, 
                                                  const std::string& file_path);
    
    // Format detection
    static std::string probeFormat(IOHandler* handler);
    static std::string probeFormat(IOHandler* handler, const std::string& file_path);
    
    // Plugin registration
    using DemuxerFactoryFunc = std::function<std::unique_ptr<Demuxer>(std::unique_ptr<IOHandler>)>;
    static void registerDemuxer(const std::string& format_id, DemuxerFactoryFunc factory_func);
    static void registerSignature(const FormatSignature& signature);
};
```

##### Format Detection Process
1. **Magic Byte Detection**: Examines file headers for format signatures
2. **Extension-Based Detection**: Uses file extensions as hints for raw formats
3. **Content Analysis**: Advanced detection for ambiguous cases

**Supported Formats**:
- **RIFF/WAV**: `"RIFF"` signature → `ChunkDemuxer`
- **AIFF**: `"FORM"` signature → `ChunkDemuxer`
- **Ogg**: `"OggS"` signature → `OggDemuxer`
- **MP4/M4A**: `ftyp` box → `ISODemuxer`
- **Raw Audio**: Extension-based → `RawAudioDemuxer`

**Example**:
```cpp
auto handler = std::make_unique<FileIOHandler>("audio.ogg");
auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
if (demuxer && demuxer->parseContainer()) {
    // Use demuxer
}
```

#### `MediaFactory`
**Purpose**: Comprehensive media factory with HTTP streaming support and plugin architecture.

**Thread Safety**: All factory methods are thread-safe.

```cpp
class MediaFactory {
public:
    // Stream creation
    static std::unique_ptr<Stream> createStream(const std::string& uri);
    static std::unique_ptr<Stream> createStreamWithMimeType(const std::string& uri, 
                                                           const std::string& mime_type);
    
    // Content analysis
    static ContentInfo analyzeContent(const std::string& uri);
    
    // Format registration (for plugins)
    static void registerFormat(const MediaFormat& format, StreamFactory factory);
    static void registerContentDetector(const std::string& format_id, ContentDetector detector);
    
    // Format queries
    static std::vector<MediaFormat> getSupportedFormats();
    static bool supportsFormat(const std::string& format_id);
    static bool supportsExtension(const std::string& extension);
    static bool supportsMimeType(const std::string& mime_type);
};
```

### Legacy Integration

#### `DemuxedStream`
**Purpose**: Bridge between the new demuxer architecture and the legacy `Stream` interface.

**Thread Safety**: Individual instances are **not** thread-safe.

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
    
    // Metadata (prefers container metadata over file tags)
    TagLib::String getArtist() override;
    TagLib::String getTitle() override;
    TagLib::String getAlbum() override;
};
```

**Key Features**:
- **Automatic Format Detection**: Uses `DemuxerFactory` for format detection
- **Codec Integration**: Automatically selects and initializes appropriate audio codecs
- **Buffering System**: Implements bounded buffers to prevent memory exhaustion
- **Position Tracking**: Tracks position based on consumed samples, not packet timestamps

## Concrete Demuxer Implementations

### `ChunkDemuxer`
**Purpose**: Universal chunk-based demuxer for RIFF/WAV and AIFF formats.

**Supported Formats**:
- Microsoft RIFF (little-endian): WAV files
- Apple AIFF (big-endian): Audio interchange format
- Various PCM and compressed audio formats within chunks

**Key Features**:
- Automatic endianness detection and handling
- Support for various WAVE format tags and AIFF compression types
- Metadata extraction from INFO chunks (WAV) and standard chunks (AIFF)
- IEEE 80-bit float conversion for AIFF sample rates

**Example**:
```cpp
auto handler = std::make_unique<FileIOHandler>("audio.wav");
auto demuxer = std::make_unique<ChunkDemuxer>(std::move(handler));
if (demuxer->parseContainer()) {
    auto streams = demuxer->getStreams();
    // Process WAV streams
}
```

### `OggDemuxer`
**Purpose**: Ogg container demuxer using libogg for proper packet handling.

**Supported Codecs**:
- Vorbis audio (.ogg)
- FLAC audio (.oga, .ogg)
- Opus audio (.opus, .oga)
- Speex audio (.spx)

**Key Features**:
- Multiple logical bitstream support
- Granule-based seeking with bisection search
- Codec-specific header parsing (Vorbis comments, OpusTags)
- Proper packet boundary handling across pages

**Conditional Compilation**: Only available when `HAVE_OGGDEMUXER` is defined.

### `ISODemuxer`
**Purpose**: ISO Base Media File Format demuxer for MP4, M4A, and related formats.

**Supported Formats**:
- MP4 files (.mp4, .m4v)
- M4A files (.m4a)
- 3GP files (.3gp)
- QuickTime MOV files (.mov)

**Supported Codecs**:
- AAC, ALAC, μ-law, A-law, various PCM formats

**Key Features**:
- Hierarchical box structure parsing
- Sample table processing for precise seeking
- Multiple track support with automatic audio track selection
- Fragmented MP4 support (basic)

### `RawAudioDemuxer`
**Purpose**: Demuxer for raw audio files with no container format.

**Supported Formats**:
- μ-law (.ulaw, .ul) - 8kHz, mono, μ-law
- A-law (.alaw, .al) - 8kHz, mono, A-law
- PCM (.pcm) - various sample rates and bit depths
- Signed 16-bit PCM (.s16le, .s16be)
- 32-bit float PCM (.f32le)

**Key Features**:
- Extension-based format detection
- Configurable format parameters
- Telephony format defaults (8kHz, mono)
- Simple streaming with calculated seeking

## Error Handling

### Error Recovery Strategies

The demuxer architecture implements comprehensive error handling with multiple recovery strategies:

#### `DemuxerErrorRecovery` Enum
```cpp
enum class DemuxerErrorRecovery {
    NONE,           // No recovery attempted
    SKIP_SECTION,   // Skip corrupted section and continue
    RESET_STATE,    // Reset internal state and retry
    FALLBACK_MODE   // Use fallback parsing mode
};
```

#### `DemuxerError` Structure
```cpp
struct DemuxerError {
    std::string category;         // Error category ("IO", "Format", "Memory")
    std::string message;          // Human-readable error message
    int error_code;               // Numeric error code
    uint64_t file_offset;         // File offset where error occurred
    DemuxerErrorRecovery recovery; // Recovery strategy used
};
```

### Error Handling Best Practices

1. **Check Return Values**: Always check return values from demuxer methods
2. **Use Error Information**: Call `getLastError()` for detailed error information
3. **Handle EOF Properly**: Use `isEOF()` to distinguish EOF from errors
4. **Resource Cleanup**: Demuxers automatically clean up resources in destructors

**Example**:
```cpp
auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
if (!demuxer) {
    std::cerr << "Failed to create demuxer" << std::endl;
    return;
}

if (!demuxer->parseContainer()) {
    const auto& error = demuxer->getLastError();
    std::cerr << "Parse error [" << error.category << "]: " 
              << error.message << " at offset " << error.file_offset << std::endl;
    return;
}

while (!demuxer->isEOF()) {
    MediaChunk chunk = demuxer->readChunk();
    if (!chunk.isValid()) {
        if (demuxer->hasError()) {
            const auto& error = demuxer->getLastError();
            std::cerr << "Read error: " << error.message << std::endl;
            demuxer->clearError(); // Clear error and continue
        }
        continue;
    }
    // Process valid chunk
}
```

## Performance Considerations

### Memory Management
- **Buffer Pool**: `MediaChunk` uses an internal buffer pool for efficient memory reuse
- **Bounded Buffers**: `DemuxedStream` implements bounded buffers to prevent memory exhaustion
- **RAII**: All resources are managed using RAII principles with smart pointers

### I/O Optimization
- **Sequential Access**: Optimized for forward reading patterns
- **Read-Ahead**: Network streams use read-ahead buffering
- **Seek Optimization**: Format-specific seeking strategies minimize I/O operations

### Threading Considerations
- **Per-Instance State**: Each demuxer instance maintains independent state
- **Factory Thread Safety**: Factory operations are thread-safe
- **No Internal Threading**: Demuxers do not create internal threads

## Integration Patterns

### Basic Usage Pattern
```cpp
// 1. Create IOHandler
auto handler = std::make_unique<FileIOHandler>("audio.ogg");

// 2. Create demuxer
auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
if (!demuxer) {
    // Handle creation failure
    return;
}

// 3. Parse container
if (!demuxer->parseContainer()) {
    // Handle parse failure
    return;
}

// 4. Get stream information
auto streams = demuxer->getStreams();
for (const auto& stream : streams) {
    if (stream.isAudio()) {
        std::cout << "Audio stream: " << stream.codec_name 
                  << " " << stream.sample_rate << "Hz" << std::endl;
    }
}

// 5. Read and process data
while (!demuxer->isEOF()) {
    MediaChunk chunk = demuxer->readChunk();
    if (chunk.isValid()) {
        // Process chunk with appropriate codec
        processWithCodec(chunk);
    }
}
```

### Legacy Integration Pattern
```cpp
// Use DemuxedStream for compatibility with existing Stream-based code
auto stream = std::make_unique<DemuxedStream>("audio.ogg");

// Existing Stream interface works unchanged
while (!stream->eof()) {
    std::vector<uint8_t> buffer(4096);
    size_t bytes_read = stream->getData(buffer.size(), buffer.data());
    if (bytes_read > 0) {
        // Process PCM data
        processPCMData(buffer.data(), bytes_read);
    }
}
```

### HTTP Streaming Pattern
```cpp
// MediaFactory handles HTTP URLs automatically
auto stream = MediaFactory::createStream("http://example.com/audio.ogg");
if (stream) {
    // Stream works the same as local files
    while (!stream->eof()) {
        // Process streaming data
    }
}
```

## Extension and Plugin Development

### Adding New Demuxer Types

1. **Inherit from Demuxer**: Create a new class inheriting from `Demuxer`
2. **Implement Interface**: Implement all pure virtual methods
3. **Register with Factory**: Register the new demuxer with `DemuxerFactory`

**Example**:
```cpp
class MyCustomDemuxer : public Demuxer {
public:
    explicit MyCustomDemuxer(std::unique_ptr<IOHandler> handler) 
        : Demuxer(std::move(handler)) {}
    
    bool parseContainer() override {
        // Implement container parsing
        return true;
    }
    
    // Implement other required methods...
};

// Register with factory
DemuxerFactory::registerDemuxer("mycustom", 
    [](std::unique_ptr<IOHandler> handler) {
        return std::make_unique<MyCustomDemuxer>(std::move(handler));
    });

// Register format signature
FormatSignature signature("mycustom", {0x4D, 0x59, 0x46, 0x4D}); // "MYFM"
DemuxerFactory::registerSignature(signature);
```

### Plugin Architecture

The `MediaFactory` supports runtime plugin registration:

```cpp
// Define format metadata
MediaFormat format;
format.format_id = "myformat";
format.display_name = "My Custom Format";
format.extensions = {".myf", ".custom"};
format.mime_types = {"audio/my-format"};
format.supports_streaming = true;

// Define stream factory
StreamFactory factory = [](const std::string& uri, const ContentInfo& info) {
    return std::make_unique<MyCustomStream>(uri);
};

// Register with MediaFactory
MediaFactory::registerFormat(format, factory);
```

This comprehensive API documentation covers all major aspects of the PsyMP3 Demuxer Architecture, providing developers with the information needed to use, extend, and integrate with the system effectively.