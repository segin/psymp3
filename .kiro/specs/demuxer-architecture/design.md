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

// Content detection result with confidence scoring
struct ContentInfo {
    std::string format_id;           // Detected format identifier
    std::string mime_type;           // Associated MIME type
    std::vector<std::string> codecs; // Detected codec types
    int confidence;                  // Confidence score (0-100)
    bool is_streamable;              // Whether format supports streaming
    bool is_seekable;                // Whether format supports seeking
    std::string detection_method;    // "magic_bytes", "extension", "mime_type", "content_analysis"
    std::map<std::string, std::string> metadata; // Additional format-specific metadata
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
- **Purpose**: Handle Ogg container format (RFC 3533 compliant)
- **Supported Codecs**: Vorbis, Opus, FLAC-in-Ogg, Speex
- **Key Features**: 
  - libogg integration for page and packet handling
  - Multiple logical bitstream support with stream multiplexing
  - Granule-based seeking using bisection search
  - Codec identification via magic signatures in header packets
  - Vorbis comment and OpusTags metadata extraction
  - Page boundary handling and packet reconstruction
  - Pre-skip sample handling for Opus streams
- **Complexity**: High (multiple codecs, complex seeking, stream multiplexing)
- **Codec Detection**:
  - Vorbis: "\x01vorbis" signature in identification header
  - Opus: "OpusHead" signature in identification header
  - FLAC: "\x7FFLAC" signature in identification header
  - Speex: "Speex   " signature in identification header
- **Seeking Strategy**: 
  - Use bisection search on file to locate target granule position
  - Parse pages to find exact packet boundaries
  - Reset codec state without resending header packets
  - Handle variable-rate codecs with granule position interpolation
- **Design Rationale**: Ogg is a complex container supporting multiple codecs with different timing models. The demuxer must handle codec-specific granule position interpretations and maintain packet ordering across page boundaries.

#### **ChunkDemuxer**
- **Purpose**: Handle chunk-based formats (RIFF/WAV, AIFF)
- **Key Features**: 
  - Universal chunk parsing for IFF-style containers
  - Automatic endianness detection (RIFF=little-endian, FORM=big-endian)
  - WAVE format tag support (PCM, IEEE float, compressed formats)
  - AIFF compression type support (NONE, sowt, fl32, etc.)
  - IEEE 80-bit extended float conversion for AIFF sample rates
  - Block alignment and frame boundary handling
  - Metadata extraction from INFO chunks (RIFF) and NAME chunks (AIFF)
- **Complexity**: Medium (chunk navigation, format variants, endianness)
- **Chunk Structure**:
```cpp
struct ChunkHeader {
    uint32_t fourcc;      // Chunk identifier (e.g., "fmt ", "data")
    uint32_t size;        // Chunk data size
    // Followed by chunk data
};
```
- **Format Detection**:
  - RIFF/WAV: "RIFF" signature, "WAVE" form type
  - AIFF: "FORM" signature, "AIFF" or "AIFC" form type
- **Seeking Strategy**: 
  - Calculate byte offset from sample position using format parameters
  - Respect block alignment for compressed formats
  - Direct positioning for uncompressed PCM
- **Design Rationale**: Chunk-based formats share a common structure but differ in endianness and format-specific details. A unified ChunkDemuxer handles both RIFF and AIFF variants while abstracting endianness differences.

#### **ISODemuxer**
- **Purpose**: Handle ISO Base Media format (MP4, M4A, MOV, 3GP)
- **Key Features**: 
  - Hierarchical box structure parsing (atoms/boxes)
  - Support for both 32-bit and 64-bit box sizes
  - Sample table processing (stts, stsc, stsz, stco/co64)
  - Multiple track support with audio track prioritization
  - Timescale-based duration calculation
  - FourCC to codec name mapping (mp4a, alac, flac, etc.)
  - Precise sample-based seeking using chunk offset tables
  - Basic fragmented MP4 support (future extension)
- **Complexity**: High (complex structure, sample-based seeking, multiple codecs)
- **Box Structure**:
```cpp
struct BoxHeader {
    uint32_t size;        // Box size (1 = 64-bit size follows)
    uint32_t type;        // Box type FourCC
    uint64_t largesize;   // Optional 64-bit size for large boxes
};
```
- **Key Boxes**:
  - `ftyp`: File type and compatibility
  - `moov`: Movie metadata container
  - `trak`: Track container (one per track)
  - `mdia`: Media information
  - `stbl`: Sample table (timing and offset information)
  - `stsd`: Sample description (codec information)
- **Sample Table Components**:
  - `stts`: Time-to-sample (decode timestamps)
  - `stsc`: Sample-to-chunk mapping
  - `stsz`: Sample sizes
  - `stco`/`co64`: Chunk offsets (32-bit/64-bit)
- **Seeking Strategy**: 
  - Convert timestamp to sample number using time-to-sample table
  - Map sample to chunk using sample-to-chunk table
  - Calculate byte offset using chunk offset table
  - Extract sample data using sample size table
- **Design Rationale**: ISO Base Media format uses a complex hierarchical structure with sample tables for efficient seeking. The demuxer must parse the box hierarchy, build sample tables, and use them for precise positioning.

#### **RawAudioDemuxer**
- **Purpose**: Handle raw audio formats (PCM, A-law, μ-law)
- **Supported Codecs**: PCM (various bit depths), G.711 A-law, G.711 μ-law
- **Key Features**: 
  - Extension-based format detection (.pcm, .alaw, .ulaw, .raw)
  - Explicit RawAudioConfig support for manual configuration
  - Default telephony parameters (8kHz, mono) for A-law/μ-law
  - Default CD quality parameters (44.1kHz, stereo, 16-bit) for PCM
  - Simple streaming with calculated seeking
  - Support for various PCM bit depths (8, 16, 24, 32-bit)
  - Support for both little-endian and big-endian PCM
  - Automatic duration calculation based on file size
- **Complexity**: Low (no container structure, simple calculations)
- **Configuration Structure**:
```cpp
struct RawAudioConfig {
    std::string codec_name;   // "pcm", "alaw", "ulaw"
    uint32_t sample_rate;     // Default: 8000 for telephony, 44100 for PCM
    uint16_t channels;        // Default: 1 (mono) for telephony, 2 (stereo) for PCM
    uint16_t bits_per_sample; // 8, 16, 24, 32 (only for PCM)
    bool is_big_endian;       // Default: false (little-endian)
};
```
- **Extension Detection**:
  - `.pcm`, `.raw`: PCM format (44.1kHz, stereo, 16-bit, little-endian)
  - `.alaw`: G.711 A-law (8kHz, mono, 8-bit)
  - `.ulaw`, `.mulaw`: G.711 μ-law (8kHz, mono, 8-bit)
- **Seeking Strategy**: 
  - Calculate byte offset from sample position using format parameters
  - Respect sample alignment (frame boundaries)
  - Direct positioning for all raw formats
- **Duration Calculation**:
  - `duration_samples = file_size / (channels * bytes_per_sample)`
  - `duration_ms = (duration_samples * 1000) / sample_rate`
- **Design Rationale**: Raw audio files lack container metadata, requiring either extension-based heuristics or explicit configuration. Telephony formats (A-law/μ-law) have standard parameters (8kHz mono) that can be safely assumed per ITU-T G.711 specification. PCM formats default to CD quality parameters but can be overridden via RawAudioConfig for non-standard configurations.

#### **FLACDemuxer**
- **Purpose**: Handle native FLAC container format (RFC 9639 compliant)
- **Supported Codecs**: Native FLAC audio
- **Key Features**: 
  - Metadata block parsing (STREAMINFO, VORBIS_COMMENT, SEEKTABLE, PICTURE, etc.)
  - Frame sync detection using 0xFFF8-0xFFFF sync patterns (14-bit sync code)
  - Variable block size support (streamable subset and non-streamable)
  - CRC-8 validation for frame headers and CRC-16 for frame footers
  - Seek table optimization for efficient seeking when available
  - ID3v2 tag skipping at file start (common in practice)
  - Comprehensive metadata extraction from VORBIS_COMMENT blocks
- **Complexity**: Medium (metadata parsing, frame sync detection, RFC compliance)
- **Metadata Block Structure**:
```cpp
struct MetadataBlockHeader {
    bool is_last;           // Last metadata block flag
    uint8_t block_type;     // 0=STREAMINFO, 1=PADDING, 2=APPLICATION, etc.
    uint32_t length;        // Block data length (24-bit)
};
```
- **STREAMINFO Block** (mandatory, must be first):
  - Minimum/maximum block size
  - Minimum/maximum frame size (may be 0 if unknown)
  - Sample rate (20-bit, 1-655350 Hz)
  - Number of channels (3-bit, 1-8 channels)
  - Bits per sample (5-bit, 4-32 bits)
  - Total samples (36-bit, 0 if unknown)
  - MD5 signature of unencoded audio
- **Seeking Strategy**: 
  - Use SEEKTABLE metadata block if present for fast seeking
  - Fall back to binary search through file for target sample
  - Validate frame sync patterns during seek to ensure accuracy
  - Handle variable block sizes during seek positioning
- **Design Rationale**: Native FLAC files require specialized handling distinct from FLAC-in-Ogg. The demuxer must parse metadata blocks before audio frames and maintain frame boundaries for proper seeking. RFC 9639 compliance ensures interoperability with all FLAC encoders and players.

### **4. Integration Bridge Component**

**Purpose**: Bridge new demuxer architecture with legacy Stream interface (Requirements 9.1-9.10)

```cpp
class DemuxedStream : public Stream {
public:
    // Constructor accepts TagLib::String for compatibility (Requirement 9.1)
    explicit DemuxedStream(const TagLib::String& path, uint32_t preferred_stream_id = 0);
    
    // Stream interface implementation (Requirement 9.1)
    size_t getData(size_t len, void *buf) override;      // Requirement 9.4: Decode to PCM
    void seekTo(unsigned long pos) override;             // Requirement 9.5: Translate seeks
    bool eof() override;                                 // EOF detection
    unsigned int getLength() override;                   // Duration reporting
    
    // Extended functionality
    std::vector<StreamInfo> getAvailableStreams() const;
    bool switchToStream(uint32_t stream_id);             // Requirement 9.8: Stream switching
    StreamInfo getCurrentStreamInfo() const;
    
private:
    std::unique_ptr<Demuxer> m_demuxer;                  // Requirement 9.2: Demuxer instance
    std::unique_ptr<AudioCodec> m_codec;                 // Requirement 9.2: Codec instance
    uint32_t m_current_stream_id = 0;
    
    // Buffering system (Requirement 9.6)
    std::queue<MediaChunk> m_chunk_buffer;
    AudioFrame m_current_frame;
    size_t m_current_frame_offset = 0;
    
    // Position tracking
    uint64_t m_samples_consumed = 0;
    bool m_eof_reached = false;
    
    // Initialization helpers (Requirement 9.2, 9.3)
    bool initializeDemuxer(const TagLib::String& path);
    bool initializeCodec(const StreamInfo& stream_info);
    void selectBestAudioStream();                        // Requirement 9.3: Auto-select stream
};
```

**Key Design Decisions**:
- **Automatic Stream Selection** (Requirement 9.3): Constructor automatically selects the best audio stream (first audio stream, or highest quality if multiple)
- **Seamless Decoding** (Requirement 9.4): getData() transparently decodes compressed data to PCM samples
- **Position Translation** (Requirement 9.5): seekTo() converts sample positions to millisecond timestamps for demuxer
- **Efficient Buffering** (Requirement 9.6): Maintains chunk and frame buffers to minimize overhead
- **Metadata Preference** (Requirement 9.7): Prefers container metadata over file-based tags
- **Dynamic Switching** (Requirement 9.8): Supports switching between streams at runtime
- **Property Updates** (Requirement 9.9): Updates Stream base class with current format information
- **Graceful Degradation** (Requirement 9.10): Handles errors gracefully and reports issues

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

## **Correctness Properties**

*A property is a characteristic or behavior that should hold true across all valid executions of a system—essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### **Property 1: Demuxer Interface Completeness**
*For any* concrete demuxer implementation, all pure virtual methods from the base Demuxer class must be implemented and callable without compilation errors.
**Validates: Requirements 1.1**

### **Property 2: Container Parsing Idempotence**
*For any* valid media container file, calling parseContainer() multiple times should return the same result and populate identical stream information.
**Validates: Requirements 1.3**

### **Property 3: Stream Metadata Completeness**
*For any* successfully parsed container, getStreams() must return StreamInfo objects containing all required fields (stream_id, codec_type, codec_name, sample_rate, channels) with valid non-zero values for audio streams.
**Validates: Requirements 1.4, 2.1, 2.2**

### **Property 4: Chunk Reading Consistency**
*For any* demuxer in a valid state, consecutive calls to readChunk() should return MediaChunk objects with monotonically increasing timestamps until EOF is reached.
**Validates: Requirements 1.5, 2.6**

### **Property 5: Seeking Boundary Clamping**
*For any* seek request with timestamp beyond the file duration, seekTo() should clamp to the valid range [0, getDuration()] and return success, positioning at the nearest valid location.
**Validates: Requirements 1.6, 11.4**

### **Property 6: EOF Consistency**
*For any* demuxer, when isEOF() returns true, subsequent calls to readChunk() should return empty MediaChunk objects, and isEOF() should continue returning true.
**Validates: Requirements 1.7**

### **Property 7: Position Tracking Accuracy**
*For any* demuxer after seeking to timestamp T, getPosition() should return a value within acceptable tolerance (±100ms) of T.
**Validates: Requirements 1.9**

### **Property 8: Format Detection Determinism**
*For any* given file content, DemuxerFactory::probeFormat() should always return the same format identifier across multiple invocations.
**Validates: Requirements 7.2, 7.9**

### **Property 9: Factory Creation Safety**
*For any* unsupported or corrupted file format, DemuxerFactory::createDemuxer() should return nullptr rather than throwing an exception or returning an invalid demuxer.
**Validates: Requirements 7.8, 11.7**

### **Property 10: IOHandler Exclusivity**
*For any* demuxer implementation, all I/O operations must go through the IOHandler interface—no direct file system or network calls should be made.
**Validates: Requirements 14.1**

### **Property 11: Stream Interface Compatibility**
*For any* media file supported by the demuxer architecture, DemuxedStream should provide the same functionality as legacy Stream implementations, with getData() returning valid PCM samples.
**Validates: Requirements 9.1, 9.4**

### **Property 12: Metadata Preference**
*For any* media file with both container metadata and file-based tags, DemuxedStream should prefer container metadata when both are available.
**Validates: Requirements 9.7**

### **Property 13: Memory Bounded Buffering**
*For any* demuxer processing a file, the total memory used for buffering should not exceed a configured maximum (e.g., 10MB), regardless of file size.
**Validates: Requirements 10.3**

### **Property 14: Sequential Read Optimization**
*For any* demuxer reading sequentially, the number of I/O operations should be proportional to the number of chunks read, not the file size.
**Validates: Requirements 10.7**

### **Property 15: Error Recovery Continuation**
*For any* demuxer encountering a corrupted section, it should skip the damaged data and continue processing subsequent valid sections without crashing.
**Validates: Requirements 11.1**

### **Property 16: Resource Cleanup Guarantee**
*For any* demuxer instance, when the destructor is called, all allocated resources (file handles, memory buffers, IOHandler) must be properly released.
**Validates: Requirements 11.10**

### **Property 17: Thread-Safe Factory Registration**
*For any* sequence of concurrent MediaFactory::registerFormat() calls from multiple threads, all formats should be successfully registered without data corruption or race conditions.
**Validates: Requirements 12.6**

### **Property 18: Concurrent Seek Safety**
*For any* demuxer with proper synchronization, concurrent seekTo() calls from multiple threads should not cause data corruption or crashes, though only one seek position will be active.
**Validates: Requirements 12.2**

### **Property 19: Plugin Format Registration**
*For any* custom demuxer implementation following the Demuxer interface, it should be registrable with MediaFactory at runtime and usable for file playback.
**Validates: Requirements 13.1, 13.2**

### **Property 20: Debug Logging Thread Safety**
*For any* demuxer with debug logging enabled, concurrent logging calls from multiple threads should produce complete, non-interleaved log messages.
**Validates: Requirements 12.10, 14.3**

## **Error Handling**

### **Error Reporting Strategy**

The demuxer architecture uses a **return-value-based error handling** approach rather than exceptions for most operations:

- **Factory Methods**: Return `nullptr` for unsupported formats or initialization failures
- **Parsing Methods**: Return `false` from `parseContainer()` with error logging
- **Seeking Methods**: Return `false` for invalid seeks, `true` for clamped seeks
- **Data Reading**: Return empty `MediaChunk` objects at EOF or on errors

**Integration with PsyMP3 Error System**:
```cpp
// Use PsyMP3's Debug logging for error reporting
Debug::log("demuxer", "Failed to parse container: %s", error_message);
Debug::log("demuxer", Debug::ERROR, "Critical demuxer failure: %s", error_message);
```

### **Initialization Errors**

| Error Condition | Handling Strategy | Recovery | Requirement |
|----------------|-------------------|----------|-------------|
| **Format Detection Failure** | Return nullptr from factory | Try fallback detection methods (extension-based) | 7.8, 11.7 |
| **Container Parsing Failure** | Return false from parseContainer() | Log specific error, allow retry with different demuxer | 11.2 |
| **IOHandler Errors** | Propagate through return values | Handle network timeouts, retry logic in caller | 11.3 |
| **Memory Allocation** | Use RAII, return nullptr/false | Cleanup via destructors, log allocation failure | 11.6 |
| **Invalid Parameters** | Validate inputs, return error | Provide clear error messages about invalid parameters | 11.5 |

### **Runtime Errors**

| Error Condition | Handling Strategy | Recovery | Requirement |
|----------------|-------------------|----------|-------------|
| **Corrupted Data** | Skip damaged sections | Continue processing from next valid sync point | 11.1 |
| **Seeking Errors** | Clamp to valid ranges | Position at nearest valid location, return success | 11.4 |
| **Stream Errors** | Isolate per-stream failures | Continue with other streams, mark failed stream | 11.9 |
| **EOF Conditions** | Return empty chunks | Set EOF flag, allow reset via seeking | 11.3 |
| **Threading Issues** | Use proper synchronization | Detect and log race conditions, maintain state consistency | 12.1-12.8 |
| **Codec Initialization Failure** | Return error from DemuxedStream | Fall back gracefully or report error to user | 11.8 |
| **Stream Switching Failure** | Maintain current stream | Report failure, keep playback on current stream | 11.9 |

### **Recovery Strategies**

**Graceful Degradation**:
- Continue operation with reduced functionality when possible
- Example: If seeking fails, continue sequential playback
- Example: If one stream fails, continue with other available streams

**Error Propagation**:
- Clear error reporting through return values and logging
- Use PsyMP3's Debug logging system with appropriate severity levels
- Provide context in error messages (file name, position, operation)

**State Consistency**:
- Maintain valid state even after errors
- Use RAII to ensure cleanup in all paths
- Reset to known-good state after recoverable errors

**Resource Cleanup** (Requirement 11.10):
- Ensure proper cleanup in all error paths using RAII
- Smart pointers automatically release resources (unique_ptr for ownership)
- Destructors handle cleanup even during exceptions
- IOHandler resources released via unique_ptr
- File handles closed automatically through IOHandler destructors
- Memory buffers freed via vector/unique_ptr destructors
- No manual resource management required in error paths
- All demuxer instances guarantee resource release on destruction

### **Error Handling Examples**

```cpp
// Factory error handling
std::unique_ptr<Demuxer> demuxer = DemuxerFactory::createDemuxer(std::move(handler));
if (!demuxer) {
    Debug::log("demuxer", Debug::ERROR, "Failed to create demuxer for file");
    return nullptr; // Propagate error to caller
}

// Parsing error handling
if (!demuxer->parseContainer()) {
    Debug::log("demuxer", Debug::ERROR, "Container parsing failed");
    return false; // Allow caller to try alternative approach
}

// Seeking error handling with clamping
bool seekResult = demuxer->seekTo(requested_timestamp);
if (!seekResult) {
    Debug::log("demuxer", Debug::WARNING, "Seek failed, clamped to valid range");
    // Continue anyway - position is still valid
}

// Corrupted data handling
MediaChunk chunk = demuxer->readChunk();
if (chunk.data.empty() && !demuxer->isEOF()) {
    Debug::log("demuxer", Debug::WARNING, "Skipping corrupted section");
    // Try next chunk - demuxer has recovered to next valid position
}
```

## **Testing Strategy**

### **Dual Testing Approach**

The demuxer architecture requires both unit testing and property-based testing for comprehensive validation:

#### **Unit Testing**
Unit tests verify specific examples, edge cases, and integration points:

- **Specific Format Examples**: Test known-good files for each container format
- **Edge Cases**: Empty files, single-frame files, maximum-size files
- **Error Conditions**: Corrupted headers, truncated files, invalid metadata
- **Integration Points**: IOHandler integration, codec selection, metadata extraction
- **Boundary Conditions**: Seeking to start/end, reading at EOF, zero-duration files

**Example Unit Tests**:
```cpp
TEST(DemuxerFactory, DetectsOggFormat) {
    auto handler = createTestIOHandler("test.ogg");
    auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
    ASSERT_NE(demuxer, nullptr);
    ASSERT_TRUE(dynamic_cast<OggDemuxer*>(demuxer.get()) != nullptr);
}

TEST(ChunkDemuxer, HandlesEmptyWAVFile) {
    auto handler = createTestIOHandler("empty.wav");
    auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
    ASSERT_TRUE(demuxer->parseContainer());
    ASSERT_EQ(demuxer->getDuration(), 0);
}
```

#### **Property-Based Testing**

Property-based tests verify universal properties across all inputs using a PBT library:

**Library Selection**: Use **Catch2 with Generators** or **RapidCheck** for C++ property-based testing

**Configuration**: Each property-based test should run a minimum of 100 iterations

**Tagging Convention**: Each PBT test must be tagged with:
```cpp
// **Feature: demuxer-architecture, Property 1: Demuxer Interface Completeness**
```

**Property Test Examples**:

```cpp
// **Feature: demuxer-architecture, Property 2: Container Parsing Idempotence**
TEST_CASE("Container parsing is idempotent", "[demuxer][property]") {
    auto gen = GENERATE(take(100, validMediaFiles()));
    
    auto handler1 = createIOHandler(gen);
    auto demuxer = DemuxerFactory::createDemuxer(std::move(handler1));
    
    auto result1 = demuxer->parseContainer();
    auto streams1 = demuxer->getStreams();
    
    auto result2 = demuxer->parseContainer();
    auto streams2 = demuxer->getStreams();
    
    REQUIRE(result1 == result2);
    REQUIRE(streams1.size() == streams2.size());
    REQUIRE(streamsEqual(streams1, streams2));
}

// **Feature: demuxer-architecture, Property 5: Seeking Boundary Clamping**
TEST_CASE("Seeking clamps to valid range", "[demuxer][property]") {
    auto file = GENERATE(take(100, validMediaFiles()));
    auto timestamp = GENERATE(take(10, random(0ULL, UINT64_MAX)));
    
    auto handler = createIOHandler(file);
    auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
    demuxer->parseContainer();
    
    auto duration = demuxer->getDuration();
    bool result = demuxer->seekTo(timestamp);
    auto position = demuxer->getPosition();
    
    REQUIRE(result == true);
    REQUIRE(position <= duration);
    REQUIRE(position >= 0);
}

// **Feature: demuxer-architecture, Property 13: Memory Bounded Buffering**
TEST_CASE("Buffering respects memory limits", "[demuxer][property]") {
    auto file = GENERATE(take(100, largeMediaFiles()));
    
    auto handler = createIOHandler(file);
    auto demuxer = DemuxerFactory::createDemuxer(std::move(handler));
    demuxer->parseContainer();
    
    size_t maxMemory = 0;
    while (!demuxer->isEOF()) {
        auto chunk = demuxer->readChunk();
        size_t currentMemory = measureDemuxerMemory(demuxer.get());
        maxMemory = std::max(maxMemory, currentMemory);
    }
    
    REQUIRE(maxMemory <= 10 * 1024 * 1024); // 10MB limit
}
```

**Generator Strategies**:
- **Valid Media Files**: Generate or use corpus of known-good files in various formats
- **Random Timestamps**: Generate random seek positions to test boundary handling
- **Corrupted Data**: Generate files with intentional corruption for error handling tests
- **Large Files**: Generate or use large files to test memory and performance properties
- **Edge Cases**: Generate minimal files, empty files, single-frame files

### **Test Coverage Requirements**

- **Unit Tests**: Cover all public methods, error paths, and integration points
- **Property Tests**: Implement tests for all 20 correctness properties
- **Integration Tests**: Test complete pipelines from file to PCM output
- **Performance Tests**: Benchmark seeking, parsing, and memory usage
- **Thread Safety Tests**: Verify concurrent access patterns don't cause corruption

### **Test Organization**

```
tests/
├── test_demuxer_unit.cpp              # Base demuxer unit tests
├── test_demuxer_factory_unit.cpp      # Factory unit tests
├── test_oggdemuxer_unit.cpp           # Ogg-specific unit tests
├── test_chunkdemuxer_unit.cpp         # Chunk-specific unit tests
├── test_isodemuxer_unit.cpp           # ISO-specific unit tests
├── test_demuxed_stream_unit.cpp       # Bridge unit tests
├── test_demuxer_properties.cpp        # Property-based tests
├── test_demuxer_integration.cpp       # Integration tests
├── test_demuxer_performance.cpp       # Performance benchmarks
└── test_demuxer_thread_safety.cpp     # Threading tests
```

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

**Public/Private Lock Pattern**: All classes using synchronization primitives MUST follow this mandatory pattern:

```cpp
class ThreadSafeDemuxer {
public:
    // Public API - acquires locks and calls private implementation
    bool seekTo(uint64_t timestamp_ms) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return seekTo_unlocked(timestamp_ms);
    }
    
    MediaChunk readChunk() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return readChunk_unlocked();
    }
    
private:
    // Private implementation - assumes locks are already held
    bool seekTo_unlocked(uint64_t timestamp_ms) {
        // Actual implementation
        // Can safely call other _unlocked methods
        return true;
    }
    
    MediaChunk readChunk_unlocked() {
        // Actual implementation
        return MediaChunk{};
    }
    
    mutable std::mutex m_mutex;
};
```

**Key Threading Principles**:
- **Per-Instance State**: Each demuxer instance maintains independent state
- **Shared Resources**: Protect factory registrations and global state with appropriate locks
- **I/O Operations**: Coordinate access to IOHandler instances to prevent race conditions
- **Buffer Management**: Thread-safe buffer operations using bounded queues
- **Lock Acquisition Order**: Document and enforce consistent lock ordering to prevent deadlocks
- **RAII Lock Guards**: Always use `std::lock_guard` or `std::unique_lock` for exception safety
- **Callback Safety**: Never invoke callbacks while holding internal locks

### **Concurrency Model**
- **Single-Threaded Demuxers** (Requirement 12.1): Individual demuxer instances are not inherently thread-safe; callers must synchronize access
- **Factory Thread Safety** (Requirement 12.6): Format registration and creation are thread-safe using internal synchronization
- **Bridge Synchronization** (Requirement 12.2): DemuxedStream handles threading for legacy compatibility using the public/private lock pattern
- **Concurrent Seeking** (Requirement 12.2): Multiple threads can call seekTo() safely with proper synchronization, though only one seek position will be active
- **I/O Coordination** (Requirement 12.3): IOHandler access is synchronized to prevent race conditions
- **Packet Queue Safety** (Requirement 12.4): Thread-safe queue operations for buffering
- **Error Propagation** (Requirement 12.8): Thread-safe error state management using atomic operations where appropriate
- **Logging** (Requirement 12.10): All debug logging operations are thread-safe

### **Lock Acquisition Order**
To prevent deadlocks when multiple locks are involved:
```
1. MediaFactory::s_formats_mutex (global format registry)
2. DemuxedStream::m_demuxer_mutex (demuxer access)
3. DemuxedStream::m_buffer_mutex (buffer operations)
4. IOHandler instance locks (I/O operations)
```

## **Integration with PsyMP3 Systems**

### **IOHandler Integration**

All demuxers use the IOHandler interface exclusively for I/O operations:

```cpp
class Demuxer {
protected:
    std::unique_ptr<IOHandler> m_handler;
    
    // All I/O goes through IOHandler
    std::vector<uint8_t> readBytes(size_t count) {
        std::vector<uint8_t> buffer(count);
        size_t read = m_handler->read(buffer.data(), count);
        buffer.resize(read);
        return buffer;
    }
    
    bool seek(uint64_t position) {
        return m_handler->seek(position, SEEK_SET);
    }
};
```

**Supported IOHandler Implementations**:
- `FileIOHandler`: Local file access with buffering
- `HTTPIOHandler`: Network streaming with range request support
- Custom implementations: Plugin-provided I/O handlers

### **Debug Logging Integration**

Use PsyMP3's Debug logging system with appropriate categories:

```cpp
// Available debug channels for demuxer architecture
Debug::log("demuxer", "Creating demuxer for format: %s", format_id.c_str());
Debug::log("demux", "Parsing container, found %zu streams", streams.size());
Debug::log("iso", "Processing MP4 box: %s, size: %llu", boxType, boxSize);
Debug::log("ogg", "Seeking to granule position: %lld", target_granule);
Debug::log("flac", "Parsed STREAMINFO: %u Hz, %u channels", sample_rate, channels);
```

**Debug Categories**:
- `demuxer`: Factory and registration operations
- `demux`: General demuxer operations
- `iso`: ISO/MP4 specific operations
- `ogg`: Ogg container operations
- `flac`: FLAC format operations
- `compliance`: Format compliance validation

### **URI and Path Handling**

Integration with PsyMP3's URI parsing:

```cpp
// MediaFactory handles URI parsing and IOHandler creation
auto stream = MediaFactory::createStream("file:///path/to/audio.mp4");
auto stream = MediaFactory::createStream("http://example.com/audio.ogg");

// DemuxedStream accepts TagLib::String for compatibility
DemuxedStream stream(TagLib::String("/path/to/audio.flac"));
```

### **Metadata Integration** (Requirement 14.8)

Container metadata integrates with existing metadata display:

```cpp
// StreamInfo provides metadata that integrates with Song/Track classes
struct StreamInfo {
    std::string artist;   // Maps to Song::getArtist()
    std::string title;    // Maps to Song::getTitle()
    std::string album;    // Maps to Song::getAlbum()
    // ... other metadata fields
};

// DemuxedStream prefers container metadata over file tags (Requirement 9.7)
// This ensures accurate metadata from MP4/M4A files
```

**Metadata Priority** (Requirement 9.7):
1. **Container Metadata** (highest priority): Extracted from container format (MP4 atoms, Ogg comments, FLAC VORBIS_COMMENT)
2. **File-Based Tags** (fallback): TagLib-extracted ID3v2, APE, etc.
3. **Filename Heuristics** (last resort): Parse artist/title from filename

**Metadata Extraction by Format**:
- **MP4/M4A**: iTunes-style metadata from `udta` atom
- **Ogg Vorbis/Opus**: Vorbis comments from header packets
- **FLAC**: VORBIS_COMMENT metadata blocks
- **WAV/AIFF**: INFO/NAME chunks (limited metadata)

**Integration with Display System**:
```cpp
class DemuxedStream : public Stream {
public:
    DemuxedStream(const TagLib::String& path) {
        // Parse container and extract metadata
        m_demuxer->parseContainer();
        auto streams = m_demuxer->getStreams();
        
        // Populate Stream base class with container metadata
        if (!streams.empty()) {
            auto& info = streams[0];
            // Update base class metadata fields
            // These integrate with existing Song/Track display
        }
    }
};
```

### **Memory Management**

Follow PsyMP3's RAII and smart pointer conventions:

```cpp
// Use unique_ptr for ownership transfer
std::unique_ptr<IOHandler> handler = std::make_unique<FileIOHandler>(path);
std::unique_ptr<Demuxer> demuxer = DemuxerFactory::createDemuxer(std::move(handler));

// Use shared_ptr for shared ownership (rare in demuxer architecture)
// Use RAII for resource management
class DemuxedStream : public Stream {
    std::unique_ptr<Demuxer> m_demuxer;  // Automatic cleanup
    std::unique_ptr<AudioCodec> m_codec;  // Automatic cleanup
    // Destructor automatically releases resources
};
```

### **Configuration System** (Requirement 14.7)

Respect PsyMP3's configuration for demuxer behavior:

```cpp
// Configuration options integrated with PsyMP3 config system
struct DemuxerConfiguration {
    // Buffer management
    size_t stream_buffer_size = 1024 * 1024;  // 1MB default
    size_t chunk_buffer_size = 256 * 1024;    // 256KB default
    
    // Seeking behavior
    int seek_precision_ms = 100;              // 100ms tolerance
    bool prefer_seektables = true;            // Use seek tables when available
    
    // Debug logging
    bool enable_debug_logging = false;
    std::vector<std::string> debug_channels;  // Specific channels to enable
    
    // Format detection
    std::map<std::string, int> format_priorities;  // Override default priorities
    bool enable_extension_fallback = true;    // Use extensions when magic bytes fail
};

// Access through PsyMP3 configuration system
auto config = Config::getDemuxerConfig();
```

### **Audio Pipeline Integration** (Requirements 14.9, 14.10)

DemuxedStream bridges demuxer architecture with audio output:

```
File/URL → IOHandler → Demuxer → MediaChunk → AudioCodec → AudioFrame → PCM → Audio Output
                                                                                      ↓
                                                                              Player/Playlist
```

**Integration Points**:
- `Stream::getData()` (Requirement 14.10): Provides PCM samples to audio system
- `Stream::seekTo()` (Requirement 14.9): Coordinates seeking with player position tracking
- `Stream::getLength()`: Reports duration for UI display
- `Stream::eof()`: Signals end of playback for playlist advancement

**Position Tracking Coordination** (Requirement 14.9):
```cpp
class DemuxedStream : public Stream {
    void seekTo(unsigned long pos) override {
        // Convert Stream position to demuxer timestamp
        uint64_t timestamp_ms = (pos * 1000ULL) / m_sample_rate;
        
        // Coordinate with demuxer
        if (m_demuxer->seekTo(timestamp_ms)) {
            // Reset codec state
            m_codec->reset();
            
            // Update position tracking
            m_samples_consumed = pos;
            m_eof_reached = false;
        }
    }
};
```

**Buffering System Integration** (Requirement 14.10):
```cpp
// Efficient buffering between demuxer and audio output
class DemuxedStream : public Stream {
private:
    std::queue<MediaChunk> m_chunk_buffer;    // Compressed data buffer
    AudioFrame m_current_frame;               // Decoded PCM buffer
    size_t m_current_frame_offset = 0;        // Position in current frame
    
    size_t getData(size_t len, void* buf) override {
        // Fill from current frame first
        // Decode more chunks as needed
        // Return PCM samples to audio system
    }
};
```

## **Extensibility**

### **Plugin Architecture** (Requirements 13.1-13.10)

The demuxer architecture is designed for extensibility through multiple mechanisms:

#### **Format Registration** (Requirement 13.2)
```cpp
// Register new format at runtime
MediaFormat custom_format{
    .format_id = "custom",
    .display_name = "Custom Audio Format",
    .extensions = {".caf"},
    .mime_types = {"audio/x-custom"},
    .magic_signatures = {"\x43\x41\x46\x00"},  // "CAF\0"
    .priority = 100,
    .supports_streaming = true,
    .supports_seeking = true,
    .is_container = true,
    .description = "Custom audio container format"
};

MediaFactory::registerFormat(custom_format, customStreamFactory);
```

#### **Custom Demuxer Implementation** (Requirement 13.1)
```cpp
// Implement custom demuxer by inheriting from base class
class CustomDemuxer : public Demuxer {
public:
    explicit CustomDemuxer(std::unique_ptr<IOHandler> handler)
        : Demuxer(std::move(handler)) {}
    
    bool parseContainer() override { /* ... */ }
    std::vector<StreamInfo> getStreams() const override { /* ... */ }
    MediaChunk readChunk() override { /* ... */ }
    bool seekTo(uint64_t timestamp_ms) override { /* ... */ }
    // ... implement other required methods
};
```

#### **Content Detection** (Requirement 13.3)
```cpp
// Register custom content detector
auto customDetector = [](IOHandler* handler) -> ContentInfo {
    // Custom detection logic
    auto magic = handler->peek(4);
    if (magic == std::vector<uint8_t>{0x43, 0x41, 0x46, 0x00}) {
        return ContentInfo{
            .format_id = "custom",
            .confidence = 100,
            .detection_method = "magic_bytes"
        };
    }
    return ContentInfo{.confidence = 0};
};

MediaFactory::registerContentDetector("custom", customDetector);
```

#### **Custom IOHandler** (Requirement 13.5)
```cpp
// Implement custom I/O handler for new protocols
class CustomIOHandler : public IOHandler {
public:
    size_t read(void* buffer, size_t size) override { /* ... */ }
    bool seek(int64_t offset, int whence) override { /* ... */ }
    int64_t tell() const override { /* ... */ }
    int64_t size() const override { /* ... */ }
    bool eof() const override { /* ... */ }
};
```

#### **Extensible Metadata** (Requirement 13.6)
```cpp
// StreamInfo supports format-specific metadata via codec_data
struct StreamInfo {
    // Standard fields
    uint32_t stream_id;
    std::string codec_name;
    // ...
    
    // Format-specific codec initialization data
    std::vector<uint8_t> codec_data;  // Can contain any format-specific info
    
    // Standard metadata fields
    std::string artist;
    std::string title;
    std::string album;
    // Additional metadata can be added to this structure
};
```

#### **Codec Integration** (Requirement 13.7)
The demuxer architecture integrates seamlessly with the AudioCodec architecture:
- Demuxers provide `codec_name` and `codec_data` in StreamInfo
- DemuxedStream automatically selects appropriate codec based on StreamInfo
- New codecs can be added independently and will work with all demuxers

#### **Stable ABI** (Requirement 13.8)
For external plugin modules:
- Pure virtual interface ensures ABI stability
- No inline methods in base class that could cause ABI breaks
- Version detection through format metadata
- Plugin compatibility checking at load time

#### **Runtime Configuration** (Requirement 13.10)
```cpp
// Configuration options for demuxer behavior
struct DemuxerConfig {
    size_t buffer_size = 1024 * 1024;      // 1MB default
    bool enable_seeking = true;
    bool validate_checksums = true;
    int seek_precision_ms = 100;
    bool prefer_seektables = true;
};

// Apply configuration to demuxer instances
demuxer->setConfig(config);
```

### **Future Enhancements**
- **Video Support**: Architecture ready for video demuxing extensions
- **Subtitle Support**: Framework for subtitle stream handling
- **Network Protocols**: Support for additional streaming protocols (RTSP, RTMP, etc.)
- **Metadata Standards**: Extensible metadata handling system (ID3v2, APE tags, etc.)
- **Format Versioning** (Requirement 13.9): Support for detecting and handling different format versions

This architecture provides a solid foundation for media container handling that is both powerful and maintainable, with clear separation of concerns and excellent extensibility for future enhancements.