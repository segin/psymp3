# **RIFF/PCM ARCHITECTURE DESIGN**

## **Overview**

This design document specifies the implementation of a cleaner RIFF/PCM architecture for PsyMP3, separating RIFF container parsing from PCM audio decoding. The architecture follows the container-agnostic pattern established for FLAC and other codecs, enabling the same PCM codec to work with RIFF/WAV, AIFF, and potentially other containers.

The design emphasizes separation of concerns: demuxers handle container format parsing and metadata extraction, while codecs focus purely on audio data decoding. This approach provides better maintainability, testability, and extensibility while maintaining full backward compatibility with existing WAV and AIFF file support.

## **Architecture**

### **Core Components**

```
RIFF/PCM Architecture
├── RIFFDemuxer          # RIFF container format parser (WAV, AVI audio)
├── AIFFDemuxer          # AIFF container format parser  
├── PCMCodec             # Container-agnostic PCM decoder
├── PCMFormatHandler     # PCM format variant management
├── MetadataExtractor    # RIFF/AIFF metadata parsing
└── SeekingEngine        # Sample-accurate positioning
```
### **Class Structure**

```cpp
class RIFFDemuxer : public Demuxer {
private:
    // RIFF parsing state
    struct RIFFHeader {
        uint32_t riff_size;
        uint32_t format_type;  // 'WAVE', 'AVI ', etc.
    };
    
    struct ChunkHeader {
        uint32_t chunk_id;     // FourCC
        uint32_t chunk_size;
    };
    
    // Audio format information
    struct WAVEFormat {
        uint16_t format_tag;
        uint16_t channels;
        uint32_t sample_rate;
        uint32_t avg_bytes_per_sec;
        uint16_t block_align;
        uint16_t bits_per_sample;
        uint16_t extra_size;
        std::vector<uint8_t> extra_data;
    };
    
    // Stream state
    WAVEFormat m_format;
    uint64_t m_data_offset = 0;
    uint64_t m_data_size = 0;
    uint64_t m_current_position = 0;
    uint32_t m_fact_samples = 0;  // For compressed formats
    
    // Metadata
    std::map<std::string, std::string> m_metadata;
    
public:
    explicit RIFFDemuxer(std::unique_ptr<IOHandler> handler);
    
    // Demuxer interface implementation
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
    // RIFF parsing methods
    bool parseRIFFHeader();
    bool parseWAVEChunks();
    bool parseFmtChunk(uint32_t chunk_size);
    bool parseDataChunk(uint32_t chunk_size);
    bool parseFactChunk(uint32_t chunk_size);
    bool parseInfoChunk(uint32_t chunk_size);
    bool parseId3Chunk(uint32_t chunk_size);
    
    // Utility methods
    ChunkHeader readChunkHeader();
    bool skipChunk(uint32_t chunk_size);
    StreamInfo createStreamInfo() const;
    uint64_t samplesToBytes(uint64_t samples) const;
    uint64_t bytesToSamples(uint64_t bytes) const;
    uint64_t samplesToMs(uint64_t samples) const;
    uint64_t msToSamples(uint64_t ms) const;
};
```

```cpp
class AIFFDemuxer : public Demuxer {
private:
    // AIFF parsing state
    struct AIFFHeader {
        uint32_t form_size;
        uint32_t form_type;    // 'AIFF' or 'AIFC'
    };
    
    struct ChunkHeader {
        uint32_t chunk_id;     // FourCC (big-endian)
        uint32_t chunk_size;   // Big-endian
    };
    
    // Audio format information
    struct COMMChunk {
        uint16_t channels;
        uint32_t sample_frames;
        uint16_t sample_size;
        uint8_t sample_rate[10];  // IEEE 80-bit float
        uint32_t compression_type;
        std::string compression_name;
    };
    
    struct SSNDChunk {
        uint32_t offset;
        uint32_t block_size;
        uint64_t data_offset;
        uint64_t data_size;
    };
    
    // Stream state
    COMMChunk m_comm;
    SSNDChunk m_ssnd;
    uint32_t m_sample_rate = 0;  // Converted from IEEE float
    uint64_t m_current_position = 0;
    bool m_is_aifc = false;
    
    // Metadata
    std::map<std::string, std::string> m_metadata;
    
public:
    explicit AIFFDemuxer(std::unique_ptr<IOHandler> handler);
    
    // Demuxer interface implementation
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
    // AIFF parsing methods
    bool parseAIFFHeader();
    bool parseAIFFChunks();
    bool parseCOMMChunk(uint32_t chunk_size);
    bool parseSSNDChunk(uint32_t chunk_size);
    bool parseNAMEChunk(uint32_t chunk_size);
    bool parseANNOChunk(uint32_t chunk_size);
    bool parseAUTHChunk(uint32_t chunk_size);
    bool parseCOPYChunk(uint32_t chunk_size);
    
    // Utility methods
    ChunkHeader readChunkHeader();
    bool skipChunk(uint32_t chunk_size);
    uint32_t convertIEEE80ToUint32(const uint8_t ieee80[10]);
    StreamInfo createStreamInfo() const;
    uint64_t samplesToBytes(uint64_t samples) const;
    uint64_t bytesToSamples(uint64_t bytes) const;
    uint64_t samplesToMs(uint64_t samples) const;
    uint64_t msToSamples(uint64_t ms) const;
};
```

```cpp
class PCMCodec : public AudioCodec {
private:
    // PCM format configuration
    enum class PCMFormat {
        PCM_U8,          // 8-bit unsigned
        PCM_S8,          // 8-bit signed
        PCM_U16_LE,      // 16-bit unsigned little-endian
        PCM_S16_LE,      // 16-bit signed little-endian
        PCM_U16_BE,      // 16-bit unsigned big-endian
        PCM_S16_BE,      // 16-bit signed big-endian
        PCM_U24_LE,      // 24-bit unsigned little-endian
        PCM_S24_LE,      // 24-bit signed little-endian
        PCM_U24_BE,      // 24-bit unsigned big-endian
        PCM_S24_BE,      // 24-bit signed big-endian
        PCM_U32_LE,      // 32-bit unsigned little-endian
        PCM_S32_LE,      // 32-bit signed little-endian
        PCM_U32_BE,      // 32-bit unsigned big-endian
        PCM_S32_BE,      // 32-bit signed big-endian
        PCM_F32_LE,      // 32-bit float little-endian
        PCM_F32_BE,      // 32-bit float big-endian
        PCM_F64_LE,      // 64-bit float little-endian
        PCM_F64_BE,      // 64-bit float big-endian
        PCM_ALAW,        // A-law compressed
        PCM_MULAW        // μ-law compressed
    };
    
    PCMFormat m_format;
    uint32_t m_sample_rate;
    uint16_t m_channels;
    uint16_t m_bits_per_sample;
    uint16_t m_bytes_per_sample;
    uint16_t m_block_align;
    
    // Conversion buffers
    std::vector<int16_t> m_output_buffer;
    std::vector<uint8_t> m_temp_buffer;
    
    // A-law and μ-law lookup tables
    static const int16_t s_alaw_table[256];
    static const int16_t s_mulaw_table[256];
    
public:
    explicit PCMCodec(const StreamInfo& stream_info);
    
    // AudioCodec interface implementation
    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    std::string getCodecName() const override { return "pcm"; }
    bool canDecode(const StreamInfo& stream_info) const override;
    
private:
    // Format detection and configuration
    PCMFormat determinePCMFormat(const StreamInfo& stream_info);
    bool configurePCMFormat(PCMFormat format);
    
    // Sample conversion methods
    size_t convertU8ToPCM(const uint8_t* input, size_t input_size, int16_t* output);
    size_t convertS8ToPCM(const uint8_t* input, size_t input_size, int16_t* output);
    size_t convertU16ToPCM(const uint8_t* input, size_t input_size, int16_t* output, bool big_endian);
    size_t convertS16ToPCM(const uint8_t* input, size_t input_size, int16_t* output, bool big_endian);
    size_t convertU24ToPCM(const uint8_t* input, size_t input_size, int16_t* output, bool big_endian);
    size_t convertS24ToPCM(const uint8_t* input, size_t input_size, int16_t* output, bool big_endian);
    size_t convertU32ToPCM(const uint8_t* input, size_t input_size, int16_t* output, bool big_endian);
    size_t convertS32ToPCM(const uint8_t* input, size_t input_size, int16_t* output, bool big_endian);
    size_t convertF32ToPCM(const uint8_t* input, size_t input_size, int16_t* output, bool big_endian);
    size_t convertF64ToPCM(const uint8_t* input, size_t input_size, int16_t* output, bool big_endian);
    size_t convertALawToPCM(const uint8_t* input, size_t input_size, int16_t* output);
    size_t convertMuLawToPCM(const uint8_t* input, size_t input_size, int16_t* output);
    
    // Utility methods
    int16_t clampToInt16(int32_t value);
    int16_t clampToInt16(float value);
    int16_t clampToInt16(double value);
    uint16_t swapBytes16(uint16_t value);
    uint32_t swapBytes32(uint32_t value);
    uint64_t swapBytes64(uint64_t value);
};
```
## **Components and Interfaces**

### **1. RIFFDemuxer Component**

**Purpose**: Parse RIFF container format and extract PCM stream information

**Key Methods**:
- `bool parseContainer()`: Main parsing entry point
- `bool parseRIFFHeader()`: Validate RIFF signature and size
- `bool parseWAVEChunks()`: Process WAVE format chunks
- `bool parseFmtChunk()`: Extract audio format parameters
- `bool parseDataChunk()`: Locate audio data region
- `bool parseFactChunk()`: Extract sample count for compressed formats
- `bool parseInfoChunk()`: Extract RIFF INFO metadata
- `bool parseId3Chunk()`: Extract ID3v2 metadata

**RIFF Parsing Process**:
1. **Header Validation**: Check "RIFF" signature and file size
2. **Format Detection**: Verify "WAVE" format type
3. **Chunk Processing**: Parse fmt, data, fact, and metadata chunks
4. **Stream Creation**: Build StreamInfo with PCM parameters
5. **Position Setup**: Prepare for audio data reading

**Format Support**:
- **PCM Formats**: All standard PCM variants (8/16/24/32-bit, signed/unsigned)
- **Compressed PCM**: A-law, μ-law, ADPCM (basic support)
- **Float PCM**: 32-bit and 64-bit IEEE float
- **Extensible Format**: Support for WAVE_FORMAT_EXTENSIBLE

### **2. AIFFDemuxer Component**

**Purpose**: Parse AIFF container format and extract PCM stream information

**Key Methods**:
- `bool parseContainer()`: Main parsing entry point
- `bool parseAIFFHeader()`: Validate FORM signature and handle big-endian
- `bool parseAIFFChunks()`: Process AIFF format chunks
- `bool parseCOMMChunk()`: Extract audio format parameters
- `bool parseSSNDChunk()`: Locate audio data with offset/blocksize
- `bool parseNAMEChunk()`: Extract title metadata
- `bool parseANNOChunk()`: Extract annotation metadata
- `uint32_t convertIEEE80ToUint32()`: Convert AIFF sample rate format

**AIFF Parsing Process**:
1. **Header Validation**: Check "FORM" signature and handle big-endian byte order
2. **Format Detection**: Verify "AIFF" or "AIFC" format type
3. **COMM Processing**: Extract sample rate, channels, bit depth from Common chunk
4. **SSND Processing**: Locate Sound Data chunk with proper offset handling
5. **Metadata Extraction**: Process NAME, ANNO, AUTH, COPY chunks
6. **Stream Creation**: Build StreamInfo with converted parameters

**AIFF-Specific Handling**:
- **Big-Endian**: All multi-byte values in big-endian format
- **IEEE 80-bit Float**: Sample rate stored as extended precision float
- **Compression Types**: Support for 'NONE', 'sowt', 'fl32', 'fl64', 'alaw', 'ulaw'
- **Data Offset**: Handle SSND chunk offset and block size fields
#
## **3. PCMCodec Component**

**Purpose**: Container-agnostic PCM audio decoding for all PCM variants

**Key Methods**:
- `bool initialize()`: Configure PCM format from StreamInfo
- `AudioFrame decode(const MediaChunk& chunk)`: Convert PCM data to 16-bit output
- `PCMFormat determinePCMFormat()`: Detect PCM variant from parameters
- `size_t convertXXXToPCM()`: Format-specific conversion functions
- `bool canDecode()`: Check if codec supports the PCM variant

**PCM Format Detection Logic**:
```cpp
PCMFormat determinePCMFormat(const StreamInfo& stream_info) {
    uint16_t bits = stream_info.bits_per_sample;
    uint32_t codec_tag = stream_info.codec_tag;
    bool is_float = (codec_tag == WAVE_FORMAT_IEEE_FLOAT);
    bool is_alaw = (codec_tag == WAVE_FORMAT_ALAW);
    bool is_mulaw = (codec_tag == WAVE_FORMAT_MULAW);
    
    // Determine endianness from container type
    bool big_endian = (stream_info.codec_data.size() > 0 && 
                      stream_info.codec_data[0] == 'A'); // AIFF marker
    
    if (is_alaw) return PCMFormat::PCM_ALAW;
    if (is_mulaw) return PCMFormat::PCM_MULAW;
    if (is_float && bits == 32) return big_endian ? PCMFormat::PCM_F32_BE : PCMFormat::PCM_F32_LE;
    if (is_float && bits == 64) return big_endian ? PCMFormat::PCM_F64_BE : PCMFormat::PCM_F64_LE;
    
    // Integer PCM format detection
    bool is_unsigned = (bits == 8); // 8-bit is typically unsigned
    if (bits == 8) return is_unsigned ? PCMFormat::PCM_U8 : PCMFormat::PCM_S8;
    if (bits == 16) return big_endian ? PCMFormat::PCM_S16_BE : PCMFormat::PCM_S16_LE;
    if (bits == 24) return big_endian ? PCMFormat::PCM_S24_BE : PCMFormat::PCM_S24_LE;
    if (bits == 32) return big_endian ? PCMFormat::PCM_S32_BE : PCMFormat::PCM_S32_LE;
    
    return PCMFormat::PCM_S16_LE; // Default fallback
}
```

**Conversion Strategies**:
- **8-bit to 16-bit**: Scale and apply offset for unsigned variants
- **16-bit**: Direct copy with endian conversion if needed
- **24-bit to 16-bit**: Downscale with optional dithering
- **32-bit to 16-bit**: Downscale with proper range mapping
- **Float to 16-bit**: Convert with clipping protection
- **A-law/μ-law**: Use lookup tables for efficient expansion

### **4. Format Detection and Integration Component**

**Purpose**: Integrate RIFF/PCM architecture with DemuxerFactory and AudioCodecFactory

**DemuxerFactory Integration**:
```cpp
// In DemuxerFactory::createDemuxerForFormat()
if (format_id == "riff") {
    return std::make_unique<RIFFDemuxer>(std::move(handler));
} else if (format_id == "aiff") {
    return std::make_unique<AIFFDemuxer>(std::move(handler));
}
```

**AudioCodecFactory Integration**:
```cpp
// Register PCM codec factory
AudioCodecFactory::registerCodec("pcm", [](const StreamInfo& info) {
    return std::make_unique<PCMCodec>(info);
});
```

**Format Signatures**:
```cpp
// RIFF format detection
{FormatSignature("riff", {'R','I','F','F'}, 0, 100, "RIFF Container")},
{FormatSignature("riff", {'W','A','V','E'}, 8, 90, "WAVE Audio")},

// AIFF format detection  
{FormatSignature("aiff", {'F','O','R','M'}, 0, 100, "AIFF Container")},
{FormatSignature("aiff", {'A','I','F','F'}, 8, 90, "AIFF Audio")},
{FormatSignature("aiff", {'A','I','F','C'}, 8, 90, "AIFF-C Audio")},
```

## **Data Models**

### **RIFF Structure Definitions**

```cpp
// RIFF chunk identifiers
constexpr uint32_t RIFF_RIFF = FOURCC('R','I','F','F');
constexpr uint32_t RIFF_WAVE = FOURCC('W','A','V','E');
constexpr uint32_t RIFF_FMT  = FOURCC('f','m','t',' ');
constexpr uint32_t RIFF_DATA = FOURCC('d','a','t','a');
constexpr uint32_t RIFF_FACT = FOURCC('f','a','c','t');
constexpr uint32_t RIFF_INFO = FOURCC('I','N','F','O');
constexpr uint32_t RIFF_ID3  = FOURCC('i','d','3',' ');

// WAVE format tags
constexpr uint16_t WAVE_FORMAT_PCM        = 0x0001;
constexpr uint16_t WAVE_FORMAT_IEEE_FLOAT = 0x0003;
constexpr uint16_t WAVE_FORMAT_ALAW       = 0x0006;
constexpr uint16_t WAVE_FORMAT_MULAW      = 0x0007;
constexpr uint16_t WAVE_FORMAT_EXTENSIBLE = 0xFFFE;
```

### **AIFF Structure Definitions**

```cpp
// AIFF chunk identifiers (big-endian)
constexpr uint32_t AIFF_FORM = FOURCC('F','O','R','M');
constexpr uint32_t AIFF_AIFF = FOURCC('A','I','F','F');
constexpr uint32_t AIFF_AIFC = FOURCC('A','I','F','C');
constexpr uint32_t AIFF_COMM = FOURCC('C','O','M','M');
constexpr uint32_t AIFF_SSND = FOURCC('S','S','N','D');
constexpr uint32_t AIFF_NAME = FOURCC('N','A','M','E');
constexpr uint32_t AIFF_ANNO = FOURCC('A','N','N','O');
constexpr uint32_t AIFF_AUTH = FOURCC('A','U','T','H');
constexpr uint32_t AIFF_COPY = FOURCC('(','c',')',' ');

// AIFF compression types
constexpr uint32_t AIFF_NONE = FOURCC('N','O','N','E');
constexpr uint32_t AIFF_SOWT = FOURCC('s','o','w','t'); // Little-endian PCM
constexpr uint32_t AIFF_FL32 = FOURCC('f','l','3','2'); // 32-bit float
constexpr uint32_t AIFF_FL64 = FOURCC('f','l','6','4'); // 64-bit float
constexpr uint32_t AIFF_ALAW = FOURCC('a','l','a','w'); // A-law
constexpr uint32_t AIFF_ULAW = FOURCC('u','l','a','w'); // μ-law
```

### **PCM Processing Pipeline**

```
MediaChunk → Format Detection → Sample Conversion → Channel Interleaving → AudioFrame
     ↓              ↓                    ↓                    ↓              ↓
Raw PCM Data → PCMFormat Enum → 16-bit Samples → Proper Layout → Output Buffer
```

## **Error Handling**

### **Container Parsing Errors**

**RIFF Error Handling**:
- **Invalid Signature**: Reject non-RIFF files immediately
- **Truncated Headers**: Handle incomplete chunk headers gracefully
- **Missing fmt Chunk**: Reject files without format information
- **Invalid Format**: Validate format parameters and reject unsupported variants
- **Corrupted Data**: Skip corrupted chunks, continue with valid data

**AIFF Error Handling**:
- **Invalid FORM**: Reject non-AIFF files immediately
- **Big-Endian Issues**: Handle byte order conversion errors
- **Missing COMM**: Reject files without Common chunk
- **IEEE Float Conversion**: Handle invalid sample rate values
- **SSND Offset Issues**: Validate and correct data offset calculations

### **PCM Decoding Errors**

**Format Detection Errors**:
- **Unsupported Format**: Report specific unsupported PCM variant
- **Invalid Parameters**: Validate bit depth, sample rate, channel count
- **Inconsistent Data**: Handle mismatched format parameters

**Conversion Errors**:
- **Buffer Overflow**: Ensure adequate output buffer sizing
- **Invalid Input**: Handle corrupted or truncated PCM data
- **Range Errors**: Clamp values during bit depth conversion
- **Endian Issues**: Handle byte order conversion failures

### **Recovery Strategies**

```cpp
class PCMErrorRecovery {
public:
    // Attempt to repair invalid format parameters
    static bool repairFormatParameters(StreamInfo& stream_info);
    
    // Infer missing format information from data analysis
    static bool inferPCMFormat(const std::vector<uint8_t>& sample_data, 
                              StreamInfo& stream_info);
    
    // Handle partial or corrupted PCM data
    static AudioFrame handleCorruptedData(const MediaChunk& chunk, 
                                         const StreamInfo& stream_info);
    
private:
    static constexpr size_t MIN_SAMPLE_SIZE = 64;  // Minimum samples for analysis
    static constexpr double SILENCE_THRESHOLD = 0.001; // For detecting silence
};
```

## **Performance Considerations**

### **Memory Efficiency**

**Streaming Approach**:
- **No Full File Loading**: Process audio data in chunks
- **Minimal Buffering**: Use small, fixed-size buffers for conversion
- **Efficient Metadata**: Store only essential metadata in memory
- **Lazy Loading**: Load format information only when needed

**Buffer Management**:
- **Reusable Buffers**: Reuse conversion buffers across chunks
- **Optimal Sizing**: Size buffers based on typical chunk sizes
- **Memory Pooling**: Consider buffer pooling for high-frequency operations

### **CPU Optimization**

**Conversion Efficiency**:
- **Lookup Tables**: Use precomputed tables for A-law/μ-law
- **SIMD Opportunities**: Vectorize conversion loops where possible
- **Branch Prediction**: Optimize format detection branches
- **Cache Efficiency**: Minimize memory access patterns

**Seeking Performance**:
- **Direct Calculation**: Calculate seek positions without file scanning
- **Minimal I/O**: Single seek operation for position changes
- **Boundary Alignment**: Ensure sample frame alignment for efficiency

### **I/O Optimization**

**Sequential Access**:
- **Forward Reading**: Optimize for sequential playback
- **Chunk Sizing**: Use optimal chunk sizes for I/O efficiency
- **Prefetching**: Consider read-ahead for streaming scenarios

**Seeking Efficiency**:
- **Position Caching**: Cache frequently accessed positions
- **Minimal Seeks**: Reduce file system seek operations
- **Boundary Handling**: Efficient handling of data boundaries

## **Integration Points**

### **With DemuxerFactory**

**Format Detection Integration**:
```cpp
// Enhanced format detection with priority handling
static const std::vector<FormatSignature> s_format_signatures = {
    // RIFF formats
    FormatSignature("riff", {'R','I','F','F'}, 0, 100, "RIFF Container"),
    FormatSignature("riff", {'W','A','V','E'}, 8, 90, "WAVE Audio"),
    
    // AIFF formats
    FormatSignature("aiff", {'F','O','R','M'}, 0, 100, "AIFF Container"),
    FormatSignature("aiff", {'A','I','F','F'}, 8, 90, "AIFF Audio"),
    FormatSignature("aiff", {'A','I','F','C'}, 8, 85, "AIFF-C Audio"),
};
```

### **With AudioCodecFactory**

**PCM Codec Registration**:
```cpp
// Register PCM codec with capability detection
AudioCodecFactory::registerCodec("pcm", [](const StreamInfo& info) -> std::unique_ptr<AudioCodec> {
    if (info.codec_name == "pcm" || info.codec_name == "wav" || info.codec_name == "aiff") {
        return std::make_unique<PCMCodec>(info);
    }
    return nullptr;
});
```

### **With Existing Architecture**

**Backward Compatibility**:
- **Stream Interface**: Maintain existing stream interface compatibility
- **Metadata Format**: Preserve existing metadata field names and formats
- **Error Handling**: Consistent error reporting with existing codecs
- **Performance**: Match or exceed existing WAV/AIFF performance

**DemuxedStream Integration**:
- **Bridge Compatibility**: Work seamlessly with DemuxedStream bridge
- **Seeking Behavior**: Maintain consistent seeking behavior
- **Chunk Format**: Produce MediaChunks compatible with existing codecs

## **Testing Strategy**

### **Unit Testing Focus**

1. **Container Parsing**: Test RIFF and AIFF parsing with various file structures
2. **PCM Conversion**: Verify accuracy of all PCM format conversions
3. **Metadata Extraction**: Test metadata parsing from various sources
4. **Error Handling**: Test graceful handling of corrupted files
5. **Seeking Accuracy**: Verify sample-accurate seeking across formats

### **Integration Testing**

1. **Real-world Files**: Test with files from various encoders and sources
2. **Format Variants**: Test all supported PCM variants and bit depths
3. **Large Files**: Test performance and memory usage with large files
4. **Edge Cases**: Test boundary conditions and unusual file structures
5. **Compatibility**: Verify backward compatibility with existing files

This design provides a robust, efficient RIFF/PCM architecture that cleanly separates container parsing from audio decoding while maintaining full compatibility with existing functionality and providing a foundation for future PCM format extensions.