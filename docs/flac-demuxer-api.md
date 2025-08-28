# FLAC Demuxer API Documentation

## Overview

The FLACDemuxer class provides native FLAC container format parsing and streaming for PsyMP3. It handles the FLAC container structure, extracts metadata, and provides FLAC bitstream data to codecs for decoding.

## Key Features

- **Native FLAC Container Support**: Parses .flac files according to FLAC specification
- **Comprehensive Metadata Extraction**: Supports all standard FLAC metadata blocks
- **Efficient Seeking**: Multiple seeking strategies for optimal performance
- **Large File Support**: Handles files >2GB with 64-bit addressing
- **Thread Safety**: Concurrent access safe with internal synchronization
- **Memory Optimization**: Bounded memory usage for large files and embedded artwork
- **Network Stream Support**: Optimized for both local files and HTTP streams

## FLAC Container Format

### Container Structure

```
[fLaC] (4 bytes)                    - Stream marker
[METADATA_BLOCK_STREAMINFO]         - Mandatory first metadata block
[METADATA_BLOCK_*]                  - Optional metadata blocks
...
[METADATA_BLOCK_*] (is_last=1)      - Last metadata block
[FLAC_FRAME]                        - First audio frame
[FLAC_FRAME]                        - Subsequent audio frames
...
```

### Metadata Block Types

| Type | Name | Description |
|------|------|-------------|
| 0 | STREAMINFO | Stream parameters (mandatory, always first) |
| 1 | PADDING | Padding for future metadata |
| 2 | APPLICATION | Application-specific data |
| 3 | SEEKTABLE | Seek points for efficient seeking |
| 4 | VORBIS_COMMENT | Metadata in Vorbis comment format |
| 5 | CUESHEET | CD-like track information |
| 6 | PICTURE | Embedded artwork |

## Public API Reference

### Constructor and Destructor

#### `FLACDemuxer(std::unique_ptr<IOHandler> handler)`

Creates a new FLAC demuxer instance.

**Parameters:**
- `handler`: IOHandler for reading FLAC data (takes ownership)

**Usage Example:**
```cpp
// Create demuxer for local file
auto file_handler = std::make_unique<FileIOHandler>("music.flac");
auto demuxer = std::make_unique<FLACDemuxer>(std::move(file_handler));

// Create demuxer for HTTP stream
auto http_handler = std::make_unique<HTTPIOHandler>("http://example.com/stream.flac");
auto demuxer = std::make_unique<FLACDemuxer>(std::move(http_handler));
```

**Thread Safety:** Constructor is not thread-safe. Create instances from single thread.

#### `~FLACDemuxer()`

Destructor with proper resource cleanup.

**Thread Safety:** Destructor ensures no operations are in progress before cleanup.

### Container Parsing

#### `bool parseContainer()`

Parses FLAC container headers and identifies streams.

**Returns:** `true` if container was successfully parsed, `false` on error

**Usage Example:**
```cpp
auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
if (!demuxer->parseContainer()) {
    std::cerr << "Failed to parse FLAC container" << std::endl;
    return false;
}
```

**Error Conditions:**
- Invalid fLaC stream marker
- Corrupted metadata blocks
- Missing STREAMINFO block
- I/O errors during parsing

**Thread Safety:** Thread-safe. Can be called concurrently with other methods.

### Stream Information

#### `std::vector<StreamInfo> getStreams() const`

Gets information about all streams in the container.

**Returns:** Vector containing single StreamInfo for FLAC audio stream

**Usage Example:**
```cpp
auto streams = demuxer->getStreams();
if (streams.empty()) {
    std::cerr << "No streams found" << std::endl;
    return false;
}

const auto& stream = streams[0];
std::cout << "Sample Rate: " << stream.sample_rate << " Hz" << std::endl;
std::cout << "Channels: " << stream.channels << std::endl;
std::cout << "Bit Depth: " << stream.bits_per_sample << " bits" << std::endl;
```

**Thread Safety:** Thread-safe. Metadata access is synchronized.

#### `StreamInfo getStreamInfo(uint32_t stream_id) const`

Gets information about a specific stream.

**Parameters:**
- `stream_id`: Stream ID (should be 1 for FLAC)

**Returns:** StreamInfo for the requested stream

**Usage Example:**
```cpp
StreamInfo info = demuxer->getStreamInfo(1);
if (info.stream_id == 0) {
    std::cerr << "Invalid stream ID" << std::endl;
    return false;
}
```

**Thread Safety:** Thread-safe.

### Data Streaming

#### `MediaChunk readChunk()`

Reads the next chunk of FLAC frame data.

**Returns:** MediaChunk containing complete FLAC frame with headers

**Usage Example:**
```cpp
while (!demuxer->isEOF()) {
    MediaChunk chunk = demuxer->readChunk();
    if (chunk.data.empty()) {
        std::cerr << "Failed to read chunk" << std::endl;
        break;
    }
    
    // Send to codec for decoding
    codec->decode(chunk);
}
```

**MediaChunk Contents:**
- `data`: Complete FLAC frame (header + subframes + CRC)
- `timestamp_samples`: Sample position of frame in stream
- `stream_id`: Always 1 for FLAC
- `is_keyframe`: Always true (all FLAC frames are independent)

**Thread Safety:** Thread-safe. Position tracking is synchronized.

#### `MediaChunk readChunk(uint32_t stream_id)`

Reads the next chunk from a specific stream.

**Parameters:**
- `stream_id`: Stream ID (should be 1 for FLAC)

**Returns:** MediaChunk containing FLAC frame data

**Usage Example:**
```cpp
MediaChunk chunk = demuxer->readChunk(1);
```

**Thread Safety:** Thread-safe.

### Seeking Operations

#### `bool seekTo(uint64_t timestamp_ms)`

Seeks to a specific time position.

**Parameters:**
- `timestamp_ms`: Target time position in milliseconds

**Returns:** `true` if seek was successful, `false` on error

**Usage Example:**
```cpp
// Seek to 2 minutes 30 seconds
uint64_t target_ms = 2 * 60 * 1000 + 30 * 1000;
if (!demuxer->seekTo(target_ms)) {
    std::cerr << "Seek failed" << std::endl;
}
```

**Seeking Strategies:**
1. **SEEKTABLE-based** (fastest): Uses seek points if available
2. **Binary search** (medium): Estimates position and refines
3. **Linear search** (slowest): Sample-accurate positioning

**Thread Safety:** Thread-safe. Seeking operations are synchronized.

### Position and Duration

#### `uint64_t getDuration() const`

Gets total duration of the FLAC file in milliseconds.

**Returns:** Duration in milliseconds, 0 if unknown

**Usage Example:**
```cpp
uint64_t duration_ms = demuxer->getDuration();
if (duration_ms > 0) {
    uint64_t minutes = duration_ms / (60 * 1000);
    uint64_t seconds = (duration_ms % (60 * 1000)) / 1000;
    std::cout << "Duration: " << minutes << ":" << seconds << std::endl;
}
```

**Thread Safety:** Thread-safe.

#### `uint64_t getPosition() const`

Gets current position in milliseconds.

**Returns:** Current playback position in milliseconds

**Usage Example:**
```cpp
uint64_t position_ms = demuxer->getPosition();
uint64_t duration_ms = demuxer->getDuration();
if (duration_ms > 0) {
    double progress = static_cast<double>(position_ms) / duration_ms;
    std::cout << "Progress: " << (progress * 100.0) << "%" << std::endl;
}
```

**Thread Safety:** Thread-safe. Uses atomic operations for fast access.

#### `uint64_t getCurrentSample() const`

Gets current position in samples.

**Returns:** Current playback position in samples

**Usage Example:**
```cpp
uint64_t current_sample = demuxer->getCurrentSample();
auto streams = demuxer->getStreams();
if (!streams.empty()) {
    uint64_t total_samples = streams[0].total_samples;
    if (total_samples > 0) {
        double progress = static_cast<double>(current_sample) / total_samples;
        std::cout << "Sample progress: " << (progress * 100.0) << "%" << std::endl;
    }
}
```

**Thread Safety:** Thread-safe.

#### `bool isEOF() const`

Checks if we've reached the end of the FLAC stream.

**Returns:** `true` if at end of file

**Usage Example:**
```cpp
while (!demuxer->isEOF()) {
    MediaChunk chunk = demuxer->readChunk();
    // Process chunk
}
```

**Thread Safety:** Thread-safe.

## Data Structures

### FLACStreamInfo

Contains STREAMINFO metadata block data:

```cpp
struct FLACStreamInfo {
    uint16_t min_block_size;     // Minimum block size in samples
    uint16_t max_block_size;     // Maximum block size in samples
    uint32_t min_frame_size;     // Minimum frame size in bytes (0 if unknown)
    uint32_t max_frame_size;     // Maximum frame size in bytes (0 if unknown)
    uint32_t sample_rate;        // Sample rate in Hz
    uint8_t channels;            // Number of channels (1-8)
    uint8_t bits_per_sample;     // Bits per sample (4-32)
    uint64_t total_samples;      // Total samples in stream (0 if unknown)
    uint8_t md5_signature[16];   // MD5 signature of uncompressed audio data
    
    bool isValid() const;        // Check if STREAMINFO contains valid data
    uint64_t getDurationMs() const; // Calculate duration in milliseconds
};
```

### FLACSeekPoint

Represents a seek table entry:

```cpp
struct FLACSeekPoint {
    uint64_t sample_number;      // Sample number of first sample in target frame
    uint64_t stream_offset;      // Offset from first frame to target frame
    uint16_t frame_samples;      // Number of samples in target frame
    
    bool isPlaceholder() const;  // Check if this is a placeholder seek point
    bool isValid() const;        // Check if this seek point is valid
};
```

### FLACFrame

Contains frame header information:

```cpp
struct FLACFrame {
    uint64_t sample_offset;      // Sample position of this frame in the stream
    uint64_t file_offset;        // File position where frame starts
    uint32_t block_size;         // Number of samples in this frame
    uint32_t frame_size;         // Size of frame in bytes
    uint32_t sample_rate;        // Sample rate for this frame (may vary)
    uint8_t channels;            // Channel assignment for this frame
    uint8_t bits_per_sample;     // Bits per sample for this frame
    bool variable_block_size;    // True if using variable block size strategy
    
    bool isValid() const;        // Check if frame header contains valid data
    uint64_t getDurationMs() const; // Calculate frame duration in milliseconds
};
```

### FLACPicture

Embedded picture metadata (memory-optimized):

```cpp
struct FLACPicture {
    uint32_t picture_type;       // Picture type (0=Other, 3=Cover front, etc.)
    std::string mime_type;       // MIME type (e.g., "image/jpeg")
    std::string description;     // Picture description
    uint32_t width;              // Picture width in pixels
    uint32_t height;             // Picture height in pixels
    uint32_t color_depth;        // Color depth in bits per pixel
    uint32_t colors_used;        // Number of colors used (0 for non-indexed)
    
    // Memory optimization: Picture data loaded on demand
    uint64_t data_offset;        // File offset where picture data starts
    uint32_t data_size;          // Size of picture data in bytes
    
    bool isValid() const;        // Check if picture metadata is valid
    const std::vector<uint8_t>& getData(IOHandler* handler) const; // Get picture data
    void clearCache() const;     // Clear cached picture data to free memory
};
```

## Performance Characteristics

### Seeking Performance

| Strategy | Speed | Accuracy | Requirements |
|----------|-------|----------|--------------|
| SEEKTABLE | Fastest | Good | SEEKTABLE metadata block |
| Binary Search | Medium | Good | None |
| Linear Search | Slowest | Perfect | None |

### Memory Usage

- **Metadata**: ~1-10KB per file (bounded)
- **Seek Table**: ~80 bytes per seek point (max 10,000 points)
- **Pictures**: Loaded on demand, cached temporarily
- **Frame Buffers**: ~64KB for frame reading
- **Total**: Typically <1MB per demuxer instance

### I/O Optimization

- **Sequential Reading**: Optimized for forward playback
- **Network Streams**: Read-ahead buffering for HTTP streams
- **Large Files**: 64-bit addressing for files >2GB
- **Buffer Management**: Reusable buffers to minimize allocations

## Error Handling

### Container-Level Errors

- **Invalid fLaC marker**: File rejected as non-FLAC
- **Corrupted metadata**: Damaged blocks skipped, playback continues
- **Missing STREAMINFO**: Parameters derived from first frame
- **I/O errors**: Propagated from IOHandler

### Frame-Level Errors

- **Lost frame sync**: Search for next valid frame
- **Invalid headers**: Frame skipped, playback continues
- **CRC mismatches**: Error logged, frame data used anyway
- **Truncated frames**: EOF handled gracefully

### Recovery Mechanisms

- **Metadata recovery**: Reasonable defaults provided
- **Frame resynchronization**: Automatic sync recovery
- **Error state tracking**: Thread-safe error propagation
- **Graceful degradation**: Playback continues when possible

## Integration Examples

### Basic Playback

```cpp
// Create and initialize demuxer
auto handler = std::make_unique<FileIOHandler>("music.flac");
auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));

if (!demuxer->parseContainer()) {
    std::cerr << "Failed to parse FLAC file" << std::endl;
    return false;
}

// Get stream information
auto streams = demuxer->getStreams();
if (streams.empty()) {
    std::cerr << "No audio streams found" << std::endl;
    return false;
}

const auto& stream = streams[0];
std::cout << "FLAC: " << stream.sample_rate << "Hz, " 
          << stream.channels << " channels, " 
          << stream.bits_per_sample << " bits" << std::endl;

// Create codec for decoding
auto codec = createFLACCodec(stream);

// Playback loop
while (!demuxer->isEOF()) {
    MediaChunk chunk = demuxer->readChunk();
    if (chunk.data.empty()) break;
    
    // Decode and play audio
    auto audio_data = codec->decode(chunk);
    audio_output->play(audio_data);
}
```

### Seeking Example

```cpp
// Seek to 25% through the file
uint64_t duration = demuxer->getDuration();
uint64_t target_position = duration / 4;

if (demuxer->seekTo(target_position)) {
    std::cout << "Seeked to " << target_position << "ms" << std::endl;
    
    // Continue playback from new position
    while (!demuxer->isEOF()) {
        MediaChunk chunk = demuxer->readChunk();
        // Process chunk
    }
} else {
    std::cerr << "Seek failed" << std::endl;
}
```

### Metadata Extraction

```cpp
// Parse container to extract metadata
if (!demuxer->parseContainer()) {
    return false;
}

auto streams = demuxer->getStreams();
if (!streams.empty()) {
    const auto& stream = streams[0];
    
    // Display basic stream information
    std::cout << "Title: " << stream.title << std::endl;
    std::cout << "Artist: " << stream.artist << std::endl;
    std::cout << "Album: " << stream.album << std::endl;
    std::cout << "Duration: " << (stream.duration_ms / 1000) << " seconds" << std::endl;
    
    // Access additional metadata through demuxer
    // (Implementation would provide access to m_vorbis_comments)
}
```

## Troubleshooting Guide

### Common Issues

#### "Failed to parse FLAC container"

**Causes:**
- File is not a valid FLAC file
- File is corrupted or truncated
- Network connection issues for HTTP streams

**Solutions:**
- Verify file integrity with FLAC tools
- Check file extension and MIME type
- Test with different FLAC files
- For HTTP streams, check network connectivity

#### "No streams found"

**Causes:**
- STREAMINFO metadata block is missing or corrupted
- File parsing failed before reaching audio data

**Solutions:**
- Check if `parseContainer()` returned true
- Verify file is not empty or truncated
- Try with a known-good FLAC file

#### "Seek failed"

**Causes:**
- Target position is beyond file duration
- File doesn't support seeking (some network streams)
- Corrupted seek table or frame structure

**Solutions:**
- Verify target position is within file duration
- Check if file has SEEKTABLE metadata block
- Try seeking to different positions
- For network streams, ensure seeking is supported

#### Poor seeking performance

**Causes:**
- File lacks SEEKTABLE metadata block
- Very large file with complex frame structure
- Network latency for HTTP streams

**Solutions:**
- Re-encode file with seek table generation
- Use FLAC tools to add seek table to existing file
- Consider local caching for network streams

#### High memory usage

**Causes:**
- Large embedded pictures in PICTURE metadata blocks
- Very large seek table
- Multiple demuxer instances

**Solutions:**
- Clear picture cache after extracting artwork
- Limit number of concurrent demuxer instances
- Monitor memory usage in application

### Debug Information

Enable debug logging to get detailed information about:
- Container parsing progress
- Metadata block processing
- Frame sync detection
- Seeking operations
- Error conditions

### Performance Monitoring

Monitor these metrics for optimal performance:
- Seek time (should be <100ms for local files)
- Memory usage (should be <1MB per instance)
- I/O operations during seeking
- Frame parsing success rate

### Compatibility Notes

- Supports all FLAC specification versions
- Compatible with files from all major FLAC encoders
- Handles both fixed and variable block size strategies
- Works with files containing unusual metadata blocks
- Supports embedded cue sheets and multiple pictures