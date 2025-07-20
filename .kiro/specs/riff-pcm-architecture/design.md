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