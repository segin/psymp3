# ISO Demuxer Design

## Overview

The ISO demuxer implements support for ISO Base Media File Format containers (MP4, M4A, MOV, 3GP) within PsyMP3's demuxer architecture. It provides container-agnostic audio stream extraction with support for multiple codecs including AAC, ALAC, mulaw, alaw, and PCM variants.

The design emphasizes efficient parsing of complex nested box structures, sample-accurate seeking through optimized sample tables, and robust handling of both complete files and fragmented/streaming scenarios.

## Architecture

### Core Components

```
ISODemuxer
├── BoxParser          # Recursive box structure parsing
├── SampleTableManager # Sample-to-chunk and timing lookups  
├── FragmentHandler    # Fragmented MP4 support
├── MetadataExtractor  # iTunes/ISO metadata parsing
├── StreamManager      # Audio track management
└── SeekingEngine      # Sample-accurate positioning
```

### Box Parsing Strategy

The demuxer uses a streaming box parser that can handle:
- Nested container structures with arbitrary depth
- Large boxes with 64-bit sizes
- Unknown/custom box types (skip gracefully)
- Incomplete files for progressive download

### Sample Table Optimization

Sample tables are optimized for memory efficiency and lookup performance:
- Compressed sample-to-chunk mappings
- Binary search structures for time-to-sample lookups
- Lazy loading of large sample size tables
- Cached chunk offset calculations

## Components and Interfaces

### ISODemuxer Class

```cpp
class ISODemuxer : public Demuxer {
public:
    // Demuxer interface implementation
    bool Initialize(std::shared_ptr<IOHandler> io) override;
    std::vector<StreamInfo> GetStreams() override;
    bool SelectStream(int streamIndex) override;
    std::unique_ptr<MediaChunk> ReadChunk() override;
    bool Seek(double timestamp) override;
    std::map<std::string, std::string> GetMetadata() override;
    
private:
    std::unique_ptr<BoxParser> boxParser;
    std::unique_ptr<SampleTableManager> sampleTables;
    std::unique_ptr<FragmentHandler> fragmentHandler;
    std::unique_ptr<MetadataExtractor> metadataExtractor;
    std::vector<AudioTrackInfo> audioTracks;
    int selectedTrackIndex = -1;
    uint64_t currentSampleIndex = 0;
};
```

### BoxParser Component

Handles recursive parsing of ISO box structures:

```cpp
class BoxParser {
public:
    struct BoxHeader {
        uint32_t type;
        uint64_t size;
        uint64_t dataOffset;
    };
    
    bool ParseMovieBox(uint64_t offset, uint64_t size);
    bool ParseTrackBox(uint64_t offset, uint64_t size, AudioTrackInfo& track);
    bool ParseSampleTableBox(uint64_t offset, uint64_t size, SampleTableInfo& tables);
    bool ParseFragmentBox(uint64_t offset, uint64_t size);
    
private:
    std::shared_ptr<IOHandler> io;
    std::stack<BoxHeader> boxStack;
    
    BoxHeader ReadBoxHeader(uint64_t offset);
    bool SkipUnknownBox(const BoxHeader& header);
};
```

### SampleTableManager Component

Manages efficient sample table lookups:

```cpp
class SampleTableManager {
public:
    struct SampleInfo {
        uint64_t offset;
        uint32_t size;
        uint32_t duration;
        bool isKeyframe;
    };
    
    bool BuildSampleTables(const SampleTableInfo& rawTables);
    SampleInfo GetSampleInfo(uint64_t sampleIndex);
    uint64_t TimeToSample(double timestamp);
    double SampleToTime(uint64_t sampleIndex);
    
private:
    // Compressed sample-to-chunk mapping
    std::vector<ChunkInfo> chunkTable;
    
    // Time-to-sample lookup with binary search
    std::vector<TimeToSampleEntry> timeTable;
    
    // Sample size table (compressed for fixed sizes)
    std::variant<uint32_t, std::vector<uint32_t>> sampleSizes;
    
    // Sync sample table for keyframe seeking
    std::vector<uint64_t> syncSamples;
};
```

### FragmentHandler Component

Handles fragmented MP4 files and streaming:

```cpp
class FragmentHandler {
public:
    bool ProcessMovieFragment(uint64_t moofOffset);
    bool UpdateSampleTables(const TrackFragmentInfo& traf);
    bool IsFragmented() const { return hasFragments; }
    
private:
    bool hasFragments = false;
    std::vector<FragmentInfo> fragments;
    uint64_t currentFragmentIndex = 0;
    
    struct FragmentInfo {
        uint64_t moofOffset;
        uint64_t mdatOffset;
        uint32_t sampleCount;
        std::vector<uint32_t> sampleSizes;
        std::vector<uint32_t> sampleDurations;
    };
};
```

## Data Models

### AudioTrackInfo Structure

```cpp
struct AudioTrackInfo {
    uint32_t trackId;
    std::string codecType;        // "aac", "alac", "ulaw", "alaw", "lpcm"
    uint32_t sampleRate;
    uint16_t channelCount;
    uint16_t bitsPerSample;
    uint32_t avgBitrate;
    
    // Codec-specific configuration
    std::vector<uint8_t> codecConfig;  // AAC: AudioSpecificConfig, ALAC: magic cookie
    
    // Sample table information
    SampleTableInfo sampleTables;
    
    // Track timing
    uint64_t duration;           // in track timescale units
    uint32_t timescale;          // samples per second for timing
    
    // Edit list (timeline modifications)
    std::vector<EditListEntry> editList;
};
```

### Box Type Definitions

```cpp
// ISO box type constants
constexpr uint32_t BOX_FTYP = FOURCC('f','t','y','p');
constexpr uint32_t BOX_MOOV = FOURCC('m','o','o','v');
constexpr uint32_t BOX_MDAT = FOURCC('m','d','a','t');
constexpr uint32_t BOX_TRAK = FOURCC('t','r','a','k');
constexpr uint32_t BOX_MDIA = FOURCC('m','d','i','a');
constexpr uint32_t BOX_MINF = FOURCC('m','i','n','f');
constexpr uint32_t BOX_STBL = FOURCC('s','t','b','l');
constexpr uint32_t BOX_STSD = FOURCC('s','t','s','d');
constexpr uint32_t BOX_STTS = FOURCC('s','t','t','s');
constexpr uint32_t BOX_STSC = FOURCC('s','t','s','c');
constexpr uint32_t BOX_STSZ = FOURCC('s','t','s','z');
constexpr uint32_t BOX_STCO = FOURCC('s','t','c','o');
constexpr uint32_t BOX_CO64 = FOURCC('c','o','6','4');

// Audio codec types
constexpr uint32_t CODEC_AAC  = FOURCC('m','p','4','a');
constexpr uint32_t CODEC_ALAC = FOURCC('a','l','a','c');
constexpr uint32_t CODEC_ULAW = FOURCC('u','l','a','w');
constexpr uint32_t CODEC_ALAW = FOURCC('a','l','a','w');
constexpr uint32_t CODEC_LPCM = FOURCC('l','p','c','m');
```

## Error Handling

### Graceful Degradation Strategy

1. **Box Parsing Errors**: Skip unknown/corrupted boxes, continue with valid data
2. **Sample Table Inconsistencies**: Use available data, interpolate missing entries
3. **Codec Configuration Missing**: Attempt to infer from sample data
4. **Fragmented File Issues**: Fall back to available fragments
5. **I/O Errors**: Retry with exponential backoff, report partial success

### Error Recovery Mechanisms

```cpp
class ErrorRecovery {
public:
    // Attempt to recover from corrupted sample tables
    bool RepairSampleTables(SampleTableInfo& tables);
    
    // Infer codec configuration from sample data
    bool InferCodecConfig(const std::vector<uint8_t>& sampleData, 
                         AudioTrackInfo& track);
    
    // Handle incomplete fragmented files
    bool RecoverFragmentedStream(std::vector<FragmentInfo>& fragments);
    
private:
    static constexpr int MAX_REPAIR_ATTEMPTS = 3;
    static constexpr double BACKOFF_MULTIPLIER = 2.0;
};
```

## Testing Strategy

### Unit Testing Focus

1. **Box Parser**: Test nested structure parsing, large boxes, unknown types
2. **Sample Tables**: Verify lookup accuracy, memory efficiency, edge cases
3. **Fragment Handling**: Test fragment ordering, incomplete fragments
4. **Codec Support**: Verify configuration extraction for each supported codec
5. **Error Recovery**: Test graceful handling of various corruption scenarios

### Integration Testing

1. **Real-world Files**: Test with files from various encoders and sources
2. **Streaming Scenarios**: Test progressive download and fragmented playback
3. **Seeking Accuracy**: Verify sample-accurate seeking across different codecs
4. **Memory Usage**: Profile memory consumption with large files
5. **Performance**: Benchmark parsing and seeking performance

### Test File Coverage

- Standard MP4/M4A files (AAC, ALAC)
- Telephony files (mulaw/alaw in ISO containers)
- Fragmented MP4 streams
- Files with complex metadata
- Corrupted/incomplete files for error handling
- Large files for performance testing

## Performance Considerations

### Memory Optimization

- Lazy loading of sample size tables for large files
- Compressed sample-to-chunk mappings
- Streaming box parser with minimal buffering
- Efficient metadata caching strategies

### Seeking Performance

- Binary search for time-to-sample lookups
- Cached chunk offset calculations
- Keyframe-aware seeking for compressed codecs
- Optimized sample table indexing

### I/O Efficiency

- Minimize file seeks through sequential parsing
- Batch sample reads for improved throughput
- Efficient handling of fragmented files
- Smart prefetching for streaming scenarios

This design provides a robust, efficient ISO demuxer that handles the complexity of modern container formats while maintaining the simplicity and performance requirements of PsyMP3's architecture.