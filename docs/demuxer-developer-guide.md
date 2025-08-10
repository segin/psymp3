# PsyMP3 Demuxer Architecture Developer Guide

## Table of Contents
- [Architecture Overview](#architecture-overview)
- [Implementing New Demuxer Types](#implementing-new-demuxer-types)
- [Plugin Development](#plugin-development)
- [Format Registration](#format-registration)
- [Design Decisions and Trade-offs](#design-decisions-and-trade-offs)
- [Integration Patterns](#integration-patterns)
- [Troubleshooting Guide](#troubleshooting-guide)
- [Performance Optimization](#performance-optimization)
- [Testing and Validation](#testing-and-validation)

## Architecture Overview

### Core Design Principles

The PsyMP3 Demuxer Architecture is built on several key principles:

1. **Separation of Concerns**: Demuxing (container parsing) is completely separate from decoding (codec processing)
2. **Streaming Architecture**: Data is processed incrementally without loading entire files into memory
3. **Extensibility**: New formats can be added through plugins without modifying core code
4. **Error Recovery**: Comprehensive error handling with multiple recovery strategies
5. **Memory Efficiency**: Bounded buffers and buffer pooling prevent memory exhaustion
6. **Thread Safety**: Clear thread safety boundaries with safe factory operations

### Component Hierarchy

```
MediaFactory (Top-level factory)
├── DemuxerFactory (Container format detection and creation)
├── Concrete Demuxers (Format-specific implementations)
│   ├── OggDemuxer (Ogg container: Vorbis, Opus, FLAC)
│   ├── ChunkDemuxer (RIFF/WAV, AIFF)
│   ├── ISODemuxer (MP4, M4A, 3GP)
│   ├── RawAudioDemuxer (Raw PCM, μ-law, A-law)
│   └── Custom Demuxers (Plugin-provided)
├── DemuxedStream (Legacy Stream interface bridge)
└── Support Classes
    ├── StreamInfo (Stream metadata)
    ├── MediaChunk (Data container with buffer pooling)
    ├── BufferPool (Memory management)
    └── Error handling classes
```

### Data Flow

```
Input Source → IOHandler → Format Detection → Demuxer Creation → Container Parsing → Stream Selection → Chunk Reading → Codec Processing → PCM Output
```

## Implementing New Demuxer Types

### Step 1: Define Your Format

Before implementing a demuxer, clearly define your format:

- **Container Structure**: How is data organized in the file?
- **Header Format**: What metadata is available and where?
- **Stream Organization**: How are multiple streams handled?
- **Seeking Strategy**: How can you efficiently seek to arbitrary positions?
- **Error Recovery**: How should corrupted data be handled?

### Step 2: Create the Demuxer Class

All demuxers must inherit from the `Demuxer` base class:

```cpp
#include "psymp3.h"

class MyFormatDemuxer : public Demuxer {
public:
    explicit MyFormatDemuxer(std::unique_ptr<IOHandler> handler);
    
    // Required interface implementation
    bool parseContainer() override;
    std::vector<StreamInfo> getStreams() const override;
    StreamInfo getStreamInfo(uint32_t stream_id) const override;
    MediaChunk readChunk() override;
    MediaChunk readChunk(uint32_t stream_id) override;
    bool seekTo(uint64_t timestamp_ms) override;
    bool isEOF() const override;
    uint64_t getDuration() const override;
    uint64_t getPosition() const override;
    
private:
    // Format-specific data structures
    struct MyFormatHeader {
        // Define your header structure
    };
    
    struct MyFormatStream {
        // Define per-stream data
    };
    
    // Private implementation
    MyFormatHeader m_header;
    std::vector<MyFormatStream> m_format_streams;
    // Add other format-specific state
};
```

### Step 3: Implement Core Methods

#### parseContainer() Implementation

This is the most critical method. It should:

1. **Read and validate the file header**
2. **Extract stream information**
3. **Build internal data structures for efficient access**
4. **Calculate duration if possible**
5. **Handle errors gracefully**

```cpp
bool MyFormatDemuxer::parseContainer() {
    try {
        // Read main header
        if (m_handler->read(&m_header, sizeof(m_header), 1) != 1) {
            reportError("IO", "Failed to read format header");
            return false;
        }
        
        // Validate magic signature
        if (m_header.magic != EXPECTED_MAGIC) {
            reportError("Format", "Invalid format signature");
            return false;
        }
        
        // Parse stream headers
        for (uint32_t i = 0; i < m_header.stream_count; ++i) {
            MyFormatStream stream;
            if (!parseStreamHeader(stream, i)) {
                reportError("Format", "Failed to parse stream " + std::to_string(i));
                return false;
            }
            m_format_streams.push_back(stream);
            
            // Convert to StreamInfo for the base class
            StreamInfo info = convertToStreamInfo(stream, i);
            m_streams.push_back(info);
        }
        
        // Calculate total duration
        calculateDuration();
        
        setParsed(true);
        return true;
        
    } catch (const std::exception& e) {
        reportError("Exception", "Parse error: " + std::string(e.what()));
        return false;
    }
}

bool MyFormatDemuxer::parseStreamHeader(MyFormatStream& stream, uint32_t index) {
    // Read stream-specific header
    // Validate stream data
    // Set up stream for reading
    return true;
}

StreamInfo MyFormatDemuxer::convertToStreamInfo(const MyFormatStream& stream, uint32_t index) {
    StreamInfo info;
    info.stream_id = index + 1; // 1-based IDs
    info.codec_type = "audio"; // or determine from stream data
    info.codec_name = determineCodecName(stream);
    info.sample_rate = stream.sample_rate;
    info.channels = stream.channels;
    // Fill in other fields...
    return info;
}
```

#### readChunk() Implementation

The read methods should:

1. **Return compressed data chunks**
2. **Include proper timing information**
3. **Handle interleaved streams correctly**
4. **Update position tracking**
5. **Detect EOF conditions**

```cpp
MediaChunk MyFormatDemuxer::readChunk() {
    // Read from the next available stream
    // This implementation depends on your format's interleaving
    
    if (isEOF()) {
        return MediaChunk{};
    }
    
    try {
        // Determine which stream to read from
        uint32_t next_stream = determineNextStream();
        return readChunk(next_stream);
        
    } catch (const std::exception& e) {
        reportError("Exception", "Read error: " + std::string(e.what()));
        return MediaChunk{};
    }
}

MediaChunk MyFormatDemuxer::readChunk(uint32_t stream_id) {
    if (stream_id == 0 || stream_id > m_format_streams.size()) {
        return MediaChunk{};
    }
    
    try {
        auto& stream = m_format_streams[stream_id - 1];
        
        // Check if stream has more data
        if (stream.current_position >= stream.data_end) {
            return MediaChunk{};
        }
        
        // Calculate chunk size (format-specific)
        size_t chunk_size = calculateChunkSize(stream);
        
        // Seek to current position
        if (m_handler->seek(stream.current_position, SEEK_SET) != 0) {
            reportError("IO", "Seek failed for stream " + std::to_string(stream_id));
            return MediaChunk{};
        }
        
        // Read chunk data
        MediaChunk chunk(stream_id, chunk_size);
        size_t bytes_read = m_handler->read(chunk.data.data(), 1, chunk_size);
        
        if (bytes_read == 0) {
            return MediaChunk{};
        }
        
        chunk.data.resize(bytes_read);
        chunk.timestamp_samples = calculateTimestamp(stream);
        chunk.file_offset = stream.current_position;
        
        // Update stream position
        stream.current_position += bytes_read;
        updatePosition(calculateCurrentTimeMs());
        
        return chunk;
        
    } catch (const std::exception& e) {
        reportError("Exception", "Read chunk error: " + std::string(e.what()));
        return MediaChunk{};
    }
}
```

#### seekTo() Implementation

Seeking strategies vary by format:

- **Sample-based formats** (PCM): Calculate exact byte offset
- **Packet-based formats** (Ogg): Use bisection search with granule positions
- **Index-based formats** (MP4): Use sample tables for precise seeking

```cpp
bool MyFormatDemuxer::seekTo(uint64_t timestamp_ms) {
    try {
        // Clamp to valid range
        if (timestamp_ms > getDuration()) {
            timestamp_ms = getDuration();
        }
        
        // Format-specific seeking logic
        for (auto& stream : m_format_streams) {
            uint64_t target_position = calculateSeekPosition(stream, timestamp_ms);
            stream.current_position = target_position;
        }
        
        // Update demuxer state
        updatePosition(timestamp_ms);
        setEOF(false);
        
        return true;
        
    } catch (const std::exception& e) {
        reportError("Exception", "Seek error: " + std::string(e.what()));
        return false;
    }
}

uint64_t MyFormatDemuxer::calculateSeekPosition(const MyFormatStream& stream, uint64_t timestamp_ms) {
    // This is highly format-specific
    // Examples:
    
    // For constant bitrate formats:
    // return stream.data_start + (timestamp_ms * stream.bytes_per_ms);
    
    // For variable bitrate formats, you might need:
    // - Index tables
    // - Bisection search
    // - Approximation with validation
    
    return stream.data_start; // Placeholder
}
```

### Step 4: Error Recovery Implementation

Override the base class error recovery methods:

```cpp
bool MyFormatDemuxer::skipToNextValidSection() const override {
    // Format-specific logic to skip corrupted data
    // For example, search for next sync pattern
    
    const uint8_t sync_pattern[] = {0x12, 0x34, 0x56, 0x78}; // Example
    
    try {
        std::vector<uint8_t> buffer(4096);
        while (true) {
            size_t bytes_read = m_handler->read(buffer.data(), 1, buffer.size());
            if (bytes_read == 0) break;
            
            // Search for sync pattern
            for (size_t i = 0; i <= bytes_read - sizeof(sync_pattern); ++i) {
                if (std::memcmp(buffer.data() + i, sync_pattern, sizeof(sync_pattern)) == 0) {
                    // Found sync pattern, seek to it
                    long current_pos = m_handler->tell();
                    m_handler->seek(current_pos - bytes_read + i, SEEK_SET);
                    return true;
                }
            }
        }
    } catch (const std::exception&) {
        // Recovery failed
    }
    
    return false;
}

bool MyFormatDemuxer::resetInternalState() const override {
    // Reset parsing state to a known good state
    // This might involve:
    // - Clearing internal buffers
    // - Resetting position counters
    // - Re-reading headers
    
    try {
        // Seek back to beginning of data
        m_handler->seek(m_header.data_start_offset, SEEK_SET);
        
        // Reset stream positions
        for (auto& stream : m_format_streams) {
            stream.current_position = stream.data_start;
        }
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
```

### Step 5: Testing Your Demuxer

Create comprehensive tests:

```cpp
#include "test_framework.h"

class MyFormatDemuxerTest {
public:
    void testBasicParsing() {
        auto handler = std::make_unique<FileIOHandler>("test.myformat");
        auto demuxer = std::make_unique<MyFormatDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer());
        
        auto streams = demuxer->getStreams();
        ASSERT_FALSE(streams.empty());
        
        const auto& stream = streams[0];
        ASSERT_TRUE(stream.isValid());
        ASSERT_EQ(stream.codec_type, "audio");
    }
    
    void testDataReading() {
        auto handler = std::make_unique<FileIOHandler>("test.myformat");
        auto demuxer = std::make_unique<MyFormatDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer());
        
        size_t chunk_count = 0;
        while (!demuxer->isEOF()) {
            MediaChunk chunk = demuxer->readChunk();
            if (chunk.isValid()) {
                chunk_count++;
                ASSERT_GT(chunk.getDataSize(), 0);
            }
        }
        
        ASSERT_GT(chunk_count, 0);
    }
    
    void testSeeking() {
        auto handler = std::make_unique<FileIOHandler>("test.myformat");
        auto demuxer = std::make_unique<MyFormatDemuxer>(std::move(handler));
        
        ASSERT_TRUE(demuxer->parseContainer());
        
        uint64_t duration = demuxer->getDuration();
        ASSERT_GT(duration, 0);
        
        // Test seeking to middle
        uint64_t target = duration / 2;
        ASSERT_TRUE(demuxer->seekTo(target));
        
        uint64_t position = demuxer->getPosition();
        ASSERT_NEAR(position, target, 1000); // Within 1 second tolerance
    }
    
    void testErrorHandling() {
        // Test with corrupted file
        auto handler = std::make_unique<FileIOHandler>("corrupted.myformat");
        auto demuxer = std::make_unique<MyFormatDemuxer>(std::move(handler));
        
        // Should fail gracefully
        ASSERT_FALSE(demuxer->parseContainer());
        ASSERT_TRUE(demuxer->hasError());
        
        const auto& error = demuxer->getLastError();
        ASSERT_FALSE(error.message.empty());
    }
};
```

## Plugin Development

### Creating a Plugin

Plugins allow you to add new format support without modifying the core PsyMP3 code:

```cpp
// MyFormatPlugin.cpp
#include "psymp3.h"
#include "MyFormatDemuxer.h"

extern "C" {
    // Plugin entry point
    bool initializePlugin() {
        try {
            registerMyFormatSupport();
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Plugin initialization failed: " << e.what() << std::endl;
            return false;
        }
    }
    
    void shutdownPlugin() {
        unregisterMyFormatSupport();
    }
    
    const char* getPluginName() {
        return "MyFormat Demuxer Plugin";
    }
    
    const char* getPluginVersion() {
        return "1.0.0";
    }
}

void registerMyFormatSupport() {
    // Register demuxer factory
    DemuxerFactory::registerDemuxer("myformat", 
        [](std::unique_ptr<IOHandler> handler) -> std::unique_ptr<Demuxer> {
            return std::make_unique<MyFormatDemuxer>(std::move(handler));
        });
    
    // Register format signature
    FormatSignature signature("myformat", {0x4D, 0x59, 0x46, 0x4D}, 0, 90); // "MYFM"
    DemuxerFactory::registerSignature(signature);
    
    // Register with MediaFactory
    MediaFormat format;
    format.format_id = "myformat";
    format.display_name = "My Custom Format";
    format.extensions = {".myf", ".myfmt"};
    format.mime_types = {"audio/my-format"};
    format.magic_signatures = {"MYFM"};
    format.priority = 90;
    format.supports_streaming = true;
    format.supports_seeking = true;
    format.is_container = true;
    format.description = "Custom audio container format";
    
    StreamFactory factory = [](const std::string& uri, const ContentInfo& info) -> std::unique_ptr<Stream> {
        return std::make_unique<DemuxedStream>(TagLib::String(uri.c_str()));
    };
    
    MediaFactory::registerFormat(format, factory);
}

void unregisterMyFormatSupport() {
    MediaFactory::unregisterFormat("myformat");
    // Note: DemuxerFactory doesn't currently support unregistration
    // This would need to be added for full plugin support
}
```

### Plugin Build System

Create a CMakeLists.txt for your plugin:

```cmake
cmake_minimum_required(VERSION 3.10)
project(MyFormatPlugin)

# Find PsyMP3 headers
find_path(PSYMP3_INCLUDE_DIR psymp3.h)
if(NOT PSYMP3_INCLUDE_DIR)
    message(FATAL_ERROR "PsyMP3 headers not found")
endif()

# Create plugin library
add_library(myformat_plugin SHARED
    MyFormatDemuxer.cpp
    MyFormatPlugin.cpp
)

target_include_directories(myformat_plugin PRIVATE ${PSYMP3_INCLUDE_DIR})
target_compile_features(myformat_plugin PRIVATE cxx_std_17)

# Set plugin properties
set_target_properties(myformat_plugin PROPERTIES
    PREFIX ""  # No lib prefix
    SUFFIX ".psymp3plugin"  # Custom extension
)

# Install plugin
install(TARGETS myformat_plugin
    LIBRARY DESTINATION lib/psymp3/plugins
)
```

## Format Registration

### Signature-Based Detection

Register format signatures for automatic detection:

```cpp
// Simple signature at file start
FormatSignature simple_sig("myformat", {0x4D, 0x59, 0x46, 0x4D}, 0, 100);
DemuxerFactory::registerSignature(simple_sig);

// Signature at specific offset
FormatSignature offset_sig("myformat2", {0x66, 0x74, 0x79, 0x70}, 4, 95);
DemuxerFactory::registerSignature(offset_sig);

// Multiple possible signatures
std::vector<FormatSignature> signatures = {
    FormatSignature("myformat", {0x4D, 0x59, 0x46, 0x4D}, 0, 100),
    FormatSignature("myformat", {0x4D, 0x59, 0x46, 0x32}, 0, 90),  // Version 2
};

for (const auto& sig : signatures) {
    DemuxerFactory::registerSignature(sig);
}
```

### Content Detection

For more complex detection logic:

```cpp
ContentDetector myDetector = [](std::unique_ptr<IOHandler>& handler) -> std::optional<ContentInfo> {
    // Read header
    uint8_t header[16];
    if (handler->read(header, 1, sizeof(header)) != sizeof(header)) {
        return std::nullopt;
    }
    
    // Reset position
    handler->seek(0, SEEK_SET);
    
    // Check for format-specific patterns
    if (isMyFormat(header)) {
        ContentInfo info;
        info.detected_format = "myformat";
        info.confidence = 0.95f;
        info.metadata["version"] = std::to_string(getFormatVersion(header));
        return info;
    }
    
    return std::nullopt;
};

MediaFactory::registerContentDetector("myformat", myDetector);
```

## Design Decisions and Trade-offs

### Memory Management Strategy

**Decision**: Use buffer pooling with bounded buffers
**Rationale**: 
- Prevents memory exhaustion during streaming
- Reduces allocation overhead
- Maintains predictable memory usage

**Trade-offs**:
- Slightly more complex implementation
- Small overhead for buffer management
- Benefits outweigh costs for streaming scenarios

### Thread Safety Model

**Decision**: Per-instance thread isolation, thread-safe factories
**Rationale**:
- Simpler implementation than full thread safety
- Allows concurrent processing with separate instances
- Factory thread safety enables safe plugin registration

**Trade-offs**:
- Requires external synchronization for shared instances
- Multiple instances use more memory
- Clear model is easier to understand and debug

### Error Recovery Philosophy

**Decision**: Graceful degradation with multiple recovery strategies
**Rationale**:
- Real-world media files are often corrupted
- Users prefer partial playback to complete failure
- Different error types need different recovery approaches

**Trade-offs**:
- More complex error handling code
- Potential for unexpected behavior with severely corrupted files
- Better user experience justifies complexity

### Streaming vs. Random Access

**Decision**: Optimize for streaming with optional seeking
**Rationale**:
- Most use cases involve sequential playback
- Streaming is memory-efficient
- Seeking can be approximated when exact seeking isn't available

**Trade-offs**:
- Some formats don't support precise seeking
- Index-based seeking requires additional memory
- Streaming-first design handles most use cases well

## Integration Patterns

### With Existing PsyMP3 Components

#### Audio Output Integration

```cpp
class AudioOutputIntegration {
private:
    std::unique_ptr<DemuxedStream> stream;
    AudioOutput* output;
    
public:
    bool initialize(const std::string& filename, AudioOutput* audio_output) {
        stream = std::make_unique<DemuxedStream>(TagLib::String(filename.c_str()));
        output = audio_output;
        
        if (stream->eof()) {
            return false;
        }
        
        // Configure audio output based on stream properties
        auto stream_info = stream->getCurrentStreamInfo();
        output->configure(stream_info.sample_rate, stream_info.channels, 
                         stream_info.bits_per_sample);
        
        return true;
    }
    
    void playback() {
        const size_t buffer_size = 4096;
        std::vector<uint8_t> buffer(buffer_size);
        
        while (!stream->eof()) {
            size_t bytes_read = stream->getData(buffer_size, buffer.data());
            if (bytes_read > 0) {
                output->write(buffer.data(), bytes_read);
            }
        }
    }
};
```

#### Metadata Integration

```cpp
class MetadataIntegration {
public:
    static void extractMetadata(const std::string& filename, Song& song) {
        auto stream = std::make_unique<DemuxedStream>(TagLib::String(filename.c_str()));
        
        if (!stream->eof()) {
            // Prefer container metadata over file-based tags
            song.setArtist(stream->getArtist());
            song.setTitle(stream->getTitle());
            song.setAlbum(stream->getAlbum());
            
            // Get technical information
            auto stream_info = stream->getCurrentStreamInfo();
            song.setDuration(stream_info.duration_ms / 1000);
            song.setBitrate(stream_info.bitrate);
            song.setSampleRate(stream_info.sample_rate);
        }
    }
};
```

### With External Libraries

#### TagLib Integration

```cpp
class TagLibIntegration {
public:
    static void combineMetadata(const std::string& filename, StreamInfo& stream_info) {
        // Get container metadata from demuxer
        auto stream = std::make_unique<DemuxedStream>(TagLib::String(filename.c_str()));
        auto container_info = stream->getCurrentStreamInfo();
        
        // Get file-based metadata from TagLib
        TagLib::FileRef file(filename.c_str());
        if (!file.isNull() && file.tag()) {
            auto tag = file.tag();
            
            // Prefer container metadata, fall back to file tags
            if (container_info.artist.empty() && !tag->artist().isEmpty()) {
                stream_info.artist = tag->artist().toCString();
            }
            if (container_info.title.empty() && !tag->title().isEmpty()) {
                stream_info.title = tag->title().toCString();
            }
            if (container_info.album.empty() && !tag->album().isEmpty()) {
                stream_info.album = tag->album().toCString();
            }
        }
    }
};
```

## Troubleshooting Guide

### Common Issues and Solutions

#### Issue: "Unsupported file format" Error

**Symptoms**: DemuxerFactory::createDemuxer() returns nullptr

**Possible Causes**:
1. File format not registered
2. Incorrect magic signature
3. File corruption
4. Missing plugin

**Solutions**:
```cpp
// Debug format detection
std::string detected_format = DemuxerFactory::probeFormat(handler.get());
if (detected_format.empty()) {
    // Check file header manually
    uint8_t header[16];
    handler->read(header, 1, sizeof(header));
    handler->seek(0, SEEK_SET);
    
    std::cout << "File header: ";
    for (int i = 0; i < 16; ++i) {
        std::cout << std::hex << static_cast<int>(header[i]) << " ";
    }
    std::cout << std::endl;
}
```

#### Issue: Parse Container Fails

**Symptoms**: parseContainer() returns false

**Debugging Steps**:
```cpp
auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
if (demuxer && !demuxer->parseContainer()) {
    const auto& error = demuxer->getLastError();
    std::cout << "Parse error [" << error.category << "]: " 
              << error.message << " at offset " << error.file_offset << std::endl;
    
    // Check if recovery is possible
    if (error.recovery != DemuxerErrorRecovery::NONE) {
        std::cout << "Recovery strategy available: " << static_cast<int>(error.recovery) << std::endl;
    }
}
```

#### Issue: No Audio Data

**Symptoms**: readChunk() returns empty chunks immediately

**Possible Causes**:
1. No audio streams in container
2. Seeking to end of file
3. Stream selection issue

**Debugging**:
```cpp
auto streams = demuxer->getStreams();
std::cout << "Found " << streams.size() << " streams:" << std::endl;

for (const auto& stream : streams) {
    std::cout << "Stream " << stream.stream_id << ": "
              << stream.codec_type << "/" << stream.codec_name << std::endl;
}

// Check if we're at EOF
if (demuxer->isEOF()) {
    std::cout << "Demuxer is at EOF" << std::endl;
    std::cout << "Duration: " << demuxer->getDuration() << "ms" << std::endl;
    std::cout << "Position: " << demuxer->getPosition() << "ms" << std::endl;
}
```

#### Issue: Seeking Doesn't Work

**Symptoms**: seekTo() returns false or seeking is inaccurate

**Debugging**:
```cpp
uint64_t duration = demuxer->getDuration();
std::cout << "File duration: " << duration << "ms" << std::endl;

// Test seeking to various positions
std::vector<uint64_t> test_positions = {0, duration/4, duration/2, 3*duration/4};

for (uint64_t pos : test_positions) {
    bool success = demuxer->seekTo(pos);
    uint64_t actual_pos = demuxer->getPosition();
    
    std::cout << "Seek to " << pos << "ms: " 
              << (success ? "SUCCESS" : "FAILED")
              << ", actual position: " << actual_pos << "ms" << std::endl;
}
```

#### Issue: Memory Usage Growing

**Symptoms**: Memory usage increases during playback

**Possible Causes**:
1. Buffer pool not working correctly
2. Chunks not being released
3. Unbounded buffering

**Debugging**:
```cpp
// Monitor buffer pool statistics
auto stats = BufferPool::getInstance().getStats();
std::cout << "Buffer pool: " << stats.total_buffers << " buffers, "
          << stats.total_memory_bytes << " bytes" << std::endl;

// Check for memory leaks in chunk processing
size_t chunk_count = 0;
while (!demuxer->isEOF() && chunk_count < 100) {
    MediaChunk chunk = demuxer->readChunk();
    if (chunk.isValid()) {
        chunk_count++;
        // Process chunk immediately and let it go out of scope
        processChunk(chunk);
    }
    
    if (chunk_count % 10 == 0) {
        auto current_stats = BufferPool::getInstance().getStats();
        std::cout << "After " << chunk_count << " chunks: "
                  << current_stats.total_memory_bytes << " bytes pooled" << std::endl;
    }
}
```

### Performance Debugging

#### Profiling Demuxer Performance

```cpp
class DemuxerProfiler {
private:
    std::chrono::high_resolution_clock::time_point start_time;
    std::map<std::string, std::chrono::nanoseconds> timings;
    
public:
    void startTiming(const std::string& operation) {
        start_time = std::chrono::high_resolution_clock::now();
    }
    
    void endTiming(const std::string& operation) {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = end_time - start_time;
        timings[operation] += duration;
    }
    
    void printReport() {
        std::cout << "Demuxer Performance Report:" << std::endl;
        for (const auto& [operation, total_time] : timings) {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(total_time);
            std::cout << "  " << operation << ": " << ms.count() << "ms" << std::endl;
        }
    }
};

void profileDemuxer(const std::string& filename) {
    DemuxerProfiler profiler;
    
    auto handler = std::make_unique<FileIOHandler>(filename);
    auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
    
    profiler.startTiming("parseContainer");
    demuxer->parseContainer();
    profiler.endTiming("parseContainer");
    
    size_t chunk_count = 0;
    profiler.startTiming("readChunks");
    while (!demuxer->isEOF() && chunk_count < 1000) {
        MediaChunk chunk = demuxer->readChunk();
        if (chunk.isValid()) {
            chunk_count++;
        }
    }
    profiler.endTiming("readChunks");
    
    profiler.printReport();
    std::cout << "Processed " << chunk_count << " chunks" << std::endl;
}
```

## Performance Optimization

### Memory Optimization Techniques

1. **Use Buffer Pooling**: Leverage the built-in BufferPool for MediaChunk data
2. **Bounded Buffers**: Implement limits on internal buffering
3. **Lazy Loading**: Only load data when needed
4. **Efficient Data Structures**: Use appropriate containers for your use case

```cpp
class OptimizedDemuxer : public Demuxer {
private:
    // Use bounded queues for internal buffering
    static constexpr size_t MAX_BUFFERED_CHUNKS = 8;
    std::queue<MediaChunk> chunk_buffer;
    
    // Cache frequently accessed data
    mutable std::optional<uint64_t> cached_duration;
    
public:
    uint64_t getDuration() const override {
        if (!cached_duration) {
            cached_duration = calculateDuration();
        }
        return *cached_duration;
    }
    
    MediaChunk readChunk() override {
        // Fill buffer if needed
        while (chunk_buffer.size() < MAX_BUFFERED_CHUNKS && !isEOF()) {
            MediaChunk chunk = readNextChunkFromFile();
            if (chunk.isValid()) {
                chunk_buffer.push(std::move(chunk));
            } else {
                break;
            }
        }
        
        // Return buffered chunk
        if (!chunk_buffer.empty()) {
            MediaChunk chunk = std::move(chunk_buffer.front());
            chunk_buffer.pop();
            return chunk;
        }
        
        return MediaChunk{};
    }
};
```

### I/O Optimization

1. **Sequential Access**: Optimize for forward reading
2. **Read-Ahead**: Buffer data for network streams
3. **Minimize Seeks**: Cache position information

```cpp
class IOOptimizedDemuxer : public Demuxer {
private:
    // Read-ahead buffer for network streams
    std::vector<uint8_t> read_ahead_buffer;
    size_t buffer_position = 0;
    size_t buffer_valid_size = 0;
    
    static constexpr size_t READ_AHEAD_SIZE = 64 * 1024; // 64KB
    
public:
    size_t optimizedRead(void* buffer, size_t size) {
        uint8_t* output = static_cast<uint8_t*>(buffer);
        size_t bytes_copied = 0;
        
        while (bytes_copied < size) {
            // Fill read-ahead buffer if needed
            if (buffer_position >= buffer_valid_size) {
                fillReadAheadBuffer();
                if (buffer_valid_size == 0) {
                    break; // EOF
                }
            }
            
            // Copy from buffer
            size_t available = buffer_valid_size - buffer_position;
            size_t to_copy = std::min(size - bytes_copied, available);
            
            std::memcpy(output + bytes_copied, 
                       read_ahead_buffer.data() + buffer_position, 
                       to_copy);
            
            bytes_copied += to_copy;
            buffer_position += to_copy;
        }
        
        return bytes_copied;
    }
    
private:
    void fillReadAheadBuffer() {
        if (read_ahead_buffer.size() < READ_AHEAD_SIZE) {
            read_ahead_buffer.resize(READ_AHEAD_SIZE);
        }
        
        buffer_valid_size = m_handler->read(read_ahead_buffer.data(), 1, READ_AHEAD_SIZE);
        buffer_position = 0;
    }
};
```

## Testing and Validation

### Unit Test Framework

```cpp
class DemuxerTestSuite {
public:
    void runAllTests() {
        testBasicFunctionality();
        testErrorHandling();
        testPerformance();
        testMemoryUsage();
        testThreadSafety();
    }
    
private:
    void testBasicFunctionality() {
        // Test with known good files
        std::vector<std::string> test_files = {
            "test_mono_8khz.wav",
            "test_stereo_44khz.ogg",
            "test_compressed.m4a"
        };
        
        for (const auto& filename : test_files) {
            testFile(filename);
        }
    }
    
    void testFile(const std::string& filename) {
        auto handler = std::make_unique<FileIOHandler>(filename);
        auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
        
        ASSERT_NOT_NULL(demuxer);
        ASSERT_TRUE(demuxer->parseContainer());
        
        auto streams = demuxer->getStreams();
        ASSERT_FALSE(streams.empty());
        
        // Test reading all data
        size_t total_chunks = 0;
        while (!demuxer->isEOF()) {
            MediaChunk chunk = demuxer->readChunk();
            if (chunk.isValid()) {
                total_chunks++;
                ASSERT_GT(chunk.getDataSize(), 0);
            }
        }
        
        ASSERT_GT(total_chunks, 0);
        
        // Test seeking
        if (demuxer->getDuration() > 0) {
            ASSERT_TRUE(demuxer->seekTo(0));
            ASSERT_FALSE(demuxer->isEOF());
        }
    }
    
    void testErrorHandling() {
        // Test with corrupted files
        testCorruptedFile("corrupted_header.wav");
        testCorruptedFile("truncated.ogg");
        testCorruptedFile("invalid_signature.m4a");
    }
    
    void testCorruptedFile(const std::string& filename) {
        auto handler = std::make_unique<FileIOHandler>(filename);
        auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
        
        if (demuxer) {
            // Should fail gracefully
            bool parse_result = demuxer->parseContainer();
            if (!parse_result) {
                ASSERT_TRUE(demuxer->hasError());
                const auto& error = demuxer->getLastError();
                ASSERT_FALSE(error.message.empty());
            }
        }
    }
    
    void testPerformance() {
        // Performance benchmarks
        auto start = std::chrono::high_resolution_clock::now();
        
        testFile("large_test_file.wav");
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "Performance test completed in " << duration.count() << "ms" << std::endl;
        
        // Assert reasonable performance (adjust threshold as needed)
        ASSERT_LT(duration.count(), 5000); // Should complete within 5 seconds
    }
    
    void testMemoryUsage() {
        // Monitor memory usage during processing
        size_t initial_memory = getCurrentMemoryUsage();
        
        {
            auto handler = std::make_unique<FileIOHandler>("test_file.wav");
            auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
            
            demuxer->parseContainer();
            
            // Process entire file
            while (!demuxer->isEOF()) {
                MediaChunk chunk = demuxer->readChunk();
                // Let chunk go out of scope immediately
            }
        }
        
        // Force cleanup
        BufferPool::getInstance().clear();
        
        size_t final_memory = getCurrentMemoryUsage();
        size_t memory_increase = final_memory - initial_memory;
        
        std::cout << "Memory increase: " << memory_increase << " bytes" << std::endl;
        
        // Assert reasonable memory usage (adjust threshold as needed)
        ASSERT_LT(memory_increase, 1024 * 1024); // Less than 1MB increase
    }
};
```

This comprehensive developer guide provides all the information needed to understand, extend, and integrate with the PsyMP3 Demuxer Architecture. It covers implementation details, best practices, troubleshooting, and performance optimization techniques.