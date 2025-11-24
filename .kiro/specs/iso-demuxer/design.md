# ISO Demuxer Design

## Overview

The ISO demuxer implements support for ISO Base Media File Format containers (MP4, M4A, MOV, 3GP) within PsyMP3's demuxer architecture. It provides container-agnostic audio and video stream extraction with support for multiple codecs including AAC, ALAC, mulaw, alaw, PCM variants, and H.264/AVC video.

The design emphasizes:
- **Standards Compliance**: Full compliance with ISO/IEC 14496-12 (Base Media File Format), ISO/IEC 14496-14 (MP4), ISO/IEC 14496-15 (AVC), and RFC 4337 (MIME types)
- **Efficient Parsing**: Optimized parsing of complex nested box structures with support for both 32-bit and 64-bit sizes
- **Sample-Accurate Seeking**: Precise navigation through sample tables with edit list support
- **Robust Streaming**: Handling of both complete files and fragmented/streaming scenarios
- **Security**: Buffer overflow protection and validation against malformed files
- **Format Identification**: Proper brand detection and MIME type identification for content negotiation

## Architecture

### Core Components

```
ISODemuxer
├── BoxParser          # Recursive box structure parsing
├── SampleTableManager # Sample-to-chunk and timing lookups  
├── FragmentHandler    # Fragmented MP4 support
├── MetadataExtractor  # iTunes/ISO metadata parsing
├── StreamManager      # Audio/video track management
├── SeekingEngine      # Sample-accurate positioning
├── ComplianceValidator # ISO/IEC 14496-12 compliance checking
└── SecurityValidator  # Buffer overflow and malformed file protection
```

### Box Parsing Strategy

The demuxer uses a streaming box parser that can handle:
- Nested container structures with arbitrary depth (with recursion limits for security)
- Large boxes with 64-bit extended sizes
- Unknown/custom box types (skip gracefully)
- Incomplete files for progressive download
- Both 32-bit and 64-bit box sizes per ISO/IEC 14496-12
- FullBox structures with version and flags fields

### Sample Table Optimization

Sample tables are optimized for memory efficiency and lookup performance:
- Compressed sample-to-chunk mappings
- Binary search structures for time-to-sample lookups
- Lazy loading of large sample size tables
- Cached chunk offset calculations
- Support for both constant and variable sample sizes

### Security and Validation

The demuxer implements comprehensive security measures:
- Buffer size validation to prevent overflows
- Integer overflow protection in size calculations
- Maximum allocation limits enforcement
- Recursion depth limiting to prevent stack overflow
- Mandatory box structure validation per ISO/IEC 14496-12
- Rejection of non-compliant streams

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
    
    // Format identification
    std::string GetMIMEType() const;
    std::string GetMajorBrand() const;
    std::vector<std::string> GetCompatibleBrands() const;
    
private:
    std::unique_ptr<BoxParser> boxParser;
    std::unique_ptr<SampleTableManager> sampleTables;
    std::unique_ptr<FragmentHandler> fragmentHandler;
    std::unique_ptr<MetadataExtractor> metadataExtractor;
    std::unique_ptr<StreamManager> streamManager;
    std::unique_ptr<SeekingEngine> seekingEngine;
    std::unique_ptr<ComplianceValidator> complianceValidator;
    std::unique_ptr<SecurityValidator> securityValidator;
    
    std::vector<TrackInfo> tracks;
    int selectedTrackIndex = -1;
    uint64_t currentSampleIndex = 0;
    
    // File type information
    std::string majorBrand;
    std::vector<std::string> compatibleBrands;
    uint32_t movieTimescale = 0;
    uint64_t movieDuration = 0;
};
```

### BoxParser Component

Handles recursive parsing of ISO box structures with full ISO/IEC 14496-12 compliance:

```cpp
class BoxParser {
public:
    struct BoxHeader {
        uint32_t type;
        uint64_t size;          // Supports both 32-bit and 64-bit sizes
        uint64_t dataOffset;
        bool isExtendedSize;    // True if size == 1 (64-bit extended size)
        bool extendsToEOF;      // True if size == 0 (extends to end of file)
    };
    
    struct FullBoxHeader : public BoxHeader {
        uint8_t version;
        uint32_t flags;         // 24-bit flags field
    };
    
    // File type box parsing
    bool ParseFileTypeBox(uint64_t offset, uint64_t size, 
                         std::string& majorBrand, 
                         std::vector<std::string>& compatibleBrands);
    
    // Movie structure parsing
    bool ParseMovieBox(uint64_t offset, uint64_t size);
    bool ParseMovieHeaderBox(uint64_t offset, uint64_t size, MovieHeaderInfo& mvhd);
    bool ParseTrackBox(uint64_t offset, uint64_t size, TrackInfo& track);
    bool ParseTrackHeaderBox(uint64_t offset, uint64_t size, TrackHeaderInfo& tkhd);
    bool ParseMediaBox(uint64_t offset, uint64_t size, TrackInfo& track);
    bool ParseMediaHeaderBox(uint64_t offset, uint64_t size, MediaHeaderInfo& mdhd);
    bool ParseHandlerBox(uint64_t offset, uint64_t size, HandlerInfo& hdlr);
    bool ParseMediaInformationBox(uint64_t offset, uint64_t size, TrackInfo& track);
    
    // Sample table parsing
    bool ParseSampleTableBox(uint64_t offset, uint64_t size, SampleTableInfo& tables);
    bool ParseSampleDescriptionBox(uint64_t offset, uint64_t size, SampleTableInfo& tables);
    bool ParseTimeToSampleBox(uint64_t offset, uint64_t size, SampleTableInfo& tables);
    bool ParseSampleToChunkBox(uint64_t offset, uint64_t size, SampleTableInfo& tables);
    bool ParseSampleSizeBox(uint64_t offset, uint64_t size, SampleTableInfo& tables);
    bool ParseChunkOffsetBox(uint64_t offset, uint64_t size, SampleTableInfo& tables);
    bool ParseSyncSampleBox(uint64_t offset, uint64_t size, SampleTableInfo& tables);
    bool ParseCompositionOffsetBox(uint64_t offset, uint64_t size, SampleTableInfo& tables);
    
    // Data reference parsing
    bool ParseDataInformationBox(uint64_t offset, uint64_t size, TrackInfo& track);
    bool ParseDataReferenceBox(uint64_t offset, uint64_t size, std::vector<DataReference>& refs);
    
    // Edit list parsing
    bool ParseEditBox(uint64_t offset, uint64_t size, TrackInfo& track);
    bool ParseEditListBox(uint64_t offset, uint64_t size, std::vector<EditListEntry>& edits);
    
    // Fragment parsing
    bool ParseMovieFragmentBox(uint64_t offset, uint64_t size);
    bool ParseTrackFragmentBox(uint64_t offset, uint64_t size, FragmentInfo& fragment);
    
    // Codec-specific parsing
    bool ParseAACConfig(const std::vector<uint8_t>& data, AACConfig& config);
    bool ParseALACConfig(const std::vector<uint8_t>& data, ALACConfig& config);
    bool ParseAVCConfig(const std::vector<uint8_t>& data, AVCConfig& config);
    
private:
    std::shared_ptr<IOHandler> io;
    std::stack<BoxHeader> boxStack;
    uint32_t recursionDepth = 0;
    static constexpr uint32_t MAX_RECURSION_DEPTH = 32;
    
    BoxHeader ReadBoxHeader(uint64_t offset);
    FullBoxHeader ReadFullBoxHeader(uint64_t offset);
    bool SkipUnknownBox(const BoxHeader& header);
    bool ValidateBoxSize(const BoxHeader& header, uint64_t parentSize);
    uint32_t ReadFourCC();
    std::string FourCCToString(uint32_t fourcc);
};
```

### SampleTableManager Component

Manages efficient sample table lookups with full ISO/IEC 14496-12 compliance:

```cpp
class SampleTableManager {
public:
    struct SampleInfo {
        uint64_t offset;
        uint32_t size;
        uint32_t duration;
        uint32_t compositionOffset;  // For decode/presentation time difference
        bool isKeyframe;
        uint32_t dataReferenceIndex;
    };
    
    bool BuildSampleTables(const SampleTableInfo& rawTables, uint32_t timescale);
    SampleInfo GetSampleInfo(uint64_t sampleIndex);
    uint64_t TimeToSample(double timestamp, bool seekToKeyframe = true);
    double SampleToTime(uint64_t sampleIndex);
    uint64_t GetTotalSampleCount() const;
    bool ValidateSampleTables();
    
private:
    // Compressed sample-to-chunk mapping
    struct ChunkInfo {
        uint32_t firstChunk;
        uint32_t samplesPerChunk;
        uint32_t sampleDescriptionIndex;
    };
    std::vector<ChunkInfo> chunkTable;
    
    // Time-to-sample lookup with binary search
    struct TimeToSampleEntry {
        uint32_t sampleCount;
        uint32_t sampleDelta;
    };
    std::vector<TimeToSampleEntry> timeTable;
    
    // Composition time offsets (for decode/presentation time difference)
    struct CompositionOffsetEntry {
        uint32_t sampleCount;
        int32_t sampleOffset;  // Can be negative in version 1
    };
    std::vector<CompositionOffsetEntry> compositionOffsets;
    
    // Sample size table (compressed for fixed sizes)
    std::variant<uint32_t, std::vector<uint32_t>> sampleSizes;
    
    // Chunk offset table (supports both 32-bit and 64-bit)
    std::vector<uint64_t> chunkOffsets;
    
    // Sync sample table for keyframe seeking
    std::vector<uint64_t> syncSamples;
    bool allSamplesAreSync = false;  // True when stss box is absent
    
    uint32_t timescale;
    uint64_t totalSamples = 0;
    
    // Helper methods
    uint64_t FindChunkForSample(uint64_t sampleIndex);
    uint64_t FindNearestSyncSample(uint64_t targetSample);
    bool IsSyncSample(uint64_t sampleIndex);
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
    bool ProcessMovieExtendsBox(uint64_t mvexOffset, uint64_t size);
    bool ProcessSegmentIndexBox(uint64_t sidxOffset, uint64_t size);
    
    // Fragment random access support
    bool BuildFragmentIndex();
    bool SeekToFragment(uint64_t targetTime);
    
private:
    bool hasFragments = false;
    std::vector<FragmentInfo> fragments;
    uint64_t currentFragmentIndex = 0;
    
    struct FragmentInfo {
        uint64_t moofOffset;
        uint64_t mdatOffset;
        uint32_t trackId;
        uint32_t sampleCount;
        std::vector<uint32_t> sampleSizes;
        std::vector<uint32_t> sampleDurations;
        std::vector<uint32_t> sampleFlags;
        std::vector<int32_t> compositionOffsets;
        uint64_t baseDataOffset;
        uint64_t decodeTime;
    };
    
    struct TrackExtendsInfo {
        uint32_t trackId;
        uint32_t defaultSampleDescriptionIndex;
        uint32_t defaultSampleDuration;
        uint32_t defaultSampleSize;
        uint32_t defaultSampleFlags;
    };
    std::map<uint32_t, TrackExtendsInfo> trackExtends;
    
    struct SegmentInfo {
        uint64_t referenceTime;
        uint64_t duration;
        uint64_t offset;
        uint64_t size;
    };
    std::vector<SegmentInfo> segments;
    
    bool HandleIncompleteFragment(FragmentInfo& fragment);
    bool ReorderFragments();
};
```

### MetadataExtractor Component

Handles extraction of metadata from iTunes and ISO metadata boxes:

```cpp
class MetadataExtractor {
public:
    std::map<std::string, std::string> ExtractMetadata(uint64_t metaBoxOffset, uint64_t size);
    bool ParseUserDataBox(uint64_t udtaOffset, uint64_t size);
    bool ParseiTunesMetadata(uint64_t ilstOffset, uint64_t size);
    bool ParseCopyrightBox(uint64_t cprtOffset, uint64_t size);
    
    // Artwork extraction
    std::vector<uint8_t> ExtractCoverArt();
    
private:
    std::map<std::string, std::string> metadata;
    std::vector<uint8_t> coverArt;
    
    std::string ParseMetadataValue(uint32_t atomType, const std::vector<uint8_t>& data);
    std::string DecodeText(const std::vector<uint8_t>& data, const std::string& encoding);
    bool IsUTF8(const std::vector<uint8_t>& data);
    bool IsUTF16(const std::vector<uint8_t>& data);
};
```

### StreamManager Component

Manages audio and video track information and stream selection:

```cpp
class StreamManager {
public:
    void AddTrack(const TrackInfo& track);
    std::vector<StreamInfo> GetStreamInfos() const;
    TrackInfo* GetTrack(uint32_t trackId);
    std::vector<TrackInfo*> GetAudioTracks();
    std::vector<TrackInfo*> GetVideoTracks();
    
    // Track filtering based on flags
    bool IsTrackEnabled(uint32_t trackId) const;
    bool IsTrackInMovie(uint32_t trackId) const;
    bool IsTrackInPreview(uint32_t trackId) const;
    
    // MIME type determination
    std::string DetermineMIMEType() const;
    
private:
    std::vector<TrackInfo> tracks;
    std::map<uint32_t, size_t> trackIdToIndex;
    
    bool HasVideoTracks() const;
    bool HasAudioTracks() const;
};
```

### SeekingEngine Component

Provides sample-accurate seeking capabilities with edit list support:

```cpp
class SeekingEngine {
public:
    bool SeekToTime(double timestamp, TrackInfo& track);
    bool SeekToSample(uint64_t sampleIndex, TrackInfo& track);
    
    // Edit list support
    bool ApplyEditList(double& timestamp, TrackInfo& track);
    double MapPresentationToMediaTime(double presentationTime, const TrackInfo& track);
    double MapMediaToPresentationTime(double mediaTime, const TrackInfo& track);
    
private:
    uint64_t FindNearestKeyframe(uint64_t targetSample, const TrackInfo& track);
    bool ValidateSeekPosition(uint64_t sampleIndex, const TrackInfo& track);
    
    // Edit list helpers
    struct EditSegment {
        uint64_t segmentDuration;    // In movie timescale
        int64_t mediaTime;            // -1 for empty edit
        double mediaRate;             // Typically 1.0
    };
    
    bool FindEditSegment(double presentationTime, const TrackInfo& track, 
                        EditSegment& segment, size_t& segmentIndex);
};
```

### ComplianceValidator Component

Validates ISO/IEC 14496-12 compliance and mandatory box requirements:

```cpp
class ComplianceValidator {
public:
    struct ValidationResult {
        bool isValid;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };
    
    // Mandatory box structure validation
    ValidationResult ValidateFileStructure();
    bool ValidateMandatoryBoxes();
    bool ValidateBoxHierarchy();
    
    // Box-specific validation
    bool ValidateFileTypeBox(const std::string& majorBrand, 
                            const std::vector<std::string>& compatibleBrands);
    bool ValidateMovieHeaderBox(const MovieHeaderInfo& mvhd);
    bool ValidateTrackHeaderBox(const TrackHeaderInfo& tkhd);
    bool ValidateMediaHeaderBox(const MediaHeaderInfo& mdhd);
    bool ValidateSampleTableBox(const SampleTableInfo& stbl);
    
    // Timescale and duration validation
    bool ValidateTimescale(uint32_t timescale);
    bool ValidateDuration(uint64_t duration, uint32_t timescale);
    
    // Version and size validation
    bool ValidateBoxVersion(uint8_t version, uint32_t boxType);
    bool ValidateBoxSize(uint64_t size, uint32_t boxType);
    
private:
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    
    bool hasFileTypeBox = false;
    bool hasMovieBox = false;
    bool hasMovieHeaderBox = false;
    std::set<uint32_t> foundTracks;
    
    void AddError(const std::string& message);
    void AddWarning(const std::string& message);
};
```

### SecurityValidator Component

Provides security validation and buffer overflow protection per RFC 4337:

```cpp
class SecurityValidator {
public:
    // Buffer size validation
    bool ValidateSampleSize(uint32_t size);
    bool ValidateAllocationSize(uint64_t size);
    bool ValidateBufferAccess(uint64_t offset, uint64_t size, uint64_t bufferSize);
    
    // Integer overflow protection
    bool ValidateBoxSizeCalculation(uint64_t size, uint64_t offset);
    bool ValidateSampleCount(uint64_t count);
    bool ValidateChunkCount(uint64_t count);
    
    // Recursion depth limiting
    bool CheckRecursionDepth(uint32_t depth);
    
    // NAL unit validation (for AVC)
    bool ValidateNALUnitSize(uint32_t size, uint32_t lengthSize);
    
    // Configuration data validation
    bool ValidateCodecConfig(const std::vector<uint8_t>& config, const std::string& codecType);
    
    // Decoder buffer model validation
    bool ValidateDecoderBufferModel(uint32_t bufferSize, uint32_t bitrate);
    
private:
    static constexpr uint64_t MAX_SAMPLE_SIZE = 100 * 1024 * 1024;  // 100MB
    static constexpr uint64_t MAX_ALLOCATION_SIZE = 1024 * 1024 * 1024;  // 1GB
    static constexpr uint32_t MAX_RECURSION_DEPTH = 32;
    static constexpr uint64_t MAX_SAMPLE_COUNT = 10000000;  // 10 million samples
    static constexpr uint32_t MAX_NAL_UNIT_SIZE = 50 * 1024 * 1024;  // 50MB
    
    bool CheckIntegerOverflow(uint64_t a, uint64_t b);
};
```

## Data Models

### TrackInfo Structure

Unified structure for both audio and video tracks:

```cpp
struct TrackInfo {
    uint32_t trackId;
    std::string handlerType;      // "soun", "vide", "hint", "meta", "text"
    std::string codecType;        // "aac", "alac", "ulaw", "alaw", "lpcm", "avc1"
    
    // Track header flags
    bool isEnabled;               // Track enabled flag from tkhd
    bool isInMovie;               // Track in movie flag from tkhd
    bool isInPreview;             // Track in preview flag from tkhd
    
    // Audio-specific properties
    uint32_t sampleRate;
    uint16_t channelCount;
    uint16_t bitsPerSample;
    uint32_t avgBitrate;
    float volume;                 // Audio volume from tkhd
    
    // Video-specific properties
    uint32_t width;
    uint32_t height;
    std::array<int32_t, 9> transformMatrix;  // Transformation matrix from tkhd
    
    // Codec-specific configuration
    std::vector<uint8_t> codecConfig;  // AAC: AudioSpecificConfig, ALAC: magic cookie, AVC: avcC
    
    // Sample table information
    SampleTableInfo sampleTables;
    
    // Track timing
    uint64_t duration;           // in track timescale units
    uint32_t timescale;          // time units per second
    
    // Edit list (timeline modifications)
    std::vector<EditListEntry> editList;
    
    // Data references
    std::vector<DataReference> dataReferences;
    uint32_t currentDataRefIndex = 1;
};
```

### MovieHeaderInfo Structure

```cpp
struct MovieHeaderInfo {
    uint8_t version;              // 0 or 1
    uint32_t timescale;           // Time units per second
    uint64_t duration;            // Duration in timescale units
    int32_t rate;                 // Playback rate (typically 0x00010000 = 1.0)
    int16_t volume;               // Audio volume (typically 0x0100 = 1.0)
    std::array<int32_t, 9> matrix; // Transformation matrix
    uint32_t nextTrackId;         // Next available track ID
    uint64_t creationTime;
    uint64_t modificationTime;
};
```

### TrackHeaderInfo Structure

```cpp
struct TrackHeaderInfo {
    uint8_t version;              // 0 or 1
    uint32_t flags;               // Track flags (enabled, in movie, in preview)
    uint32_t trackId;
    uint64_t duration;            // Duration in movie timescale units
    int16_t layer;                // Visual ordering of tracks
    int16_t alternateGroup;       // Group ID for alternate tracks
    int16_t volume;               // Audio volume
    std::array<int32_t, 9> matrix; // Transformation matrix
    uint32_t width;               // Visual width (16.16 fixed point)
    uint32_t height;              // Visual height (16.16 fixed point)
    uint64_t creationTime;
    uint64_t modificationTime;
};
```

### MediaHeaderInfo Structure

```cpp
struct MediaHeaderInfo {
    uint8_t version;              // 0 or 1
    uint32_t timescale;           // Media timescale
    uint64_t duration;            // Duration in media timescale units
    uint16_t language;            // ISO 639-2/T language code
    uint64_t creationTime;
    uint64_t modificationTime;
};
```

### HandlerInfo Structure

```cpp
struct HandlerInfo {
    uint32_t handlerType;         // 'soun', 'vide', 'hint', 'meta', 'text'
    std::string name;             // Handler name
};
```

### DataReference Structure

```cpp
struct DataReference {
    enum Type {
        URL,                      // url box
        URN,                      // urn box
        SELF                      // Self-reference (flag = 1)
    };
    
    Type type;
    std::string location;         // URL or URN string
    bool isSelfReference;         // True if data is in same file
};
```

### EditListEntry Structure

```cpp
struct EditListEntry {
    uint64_t segmentDuration;     // Duration in movie timescale
    int64_t mediaTime;            // Start time in media timescale (-1 = empty edit)
    int16_t mediaRate;            // Playback rate (16.16 fixed point)
};
```

### AACConfig Structure

```cpp
struct AACConfig {
    uint8_t audioObjectType;
    uint8_t samplingFrequencyIndex;
    uint32_t samplingFrequency;
    uint8_t channelConfiguration;
    bool frameLengthFlag;
    bool dependsOnCoreCoder;
    bool extensionFlag;
};
```

### ALACConfig Structure

```cpp
struct ALACConfig {
    uint32_t frameLength;
    uint8_t compatibleVersion;
    uint8_t bitDepth;
    uint8_t pb;
    uint8_t mb;
    uint8_t kb;
    uint8_t numChannels;
    uint16_t maxRun;
    uint32_t maxFrameBytes;
    uint32_t avgBitRate;
    uint32_t sampleRate;
};
```

### AVCConfig Structure

```cpp
struct AVCConfig {
    uint8_t configurationVersion;
    uint8_t avcProfileIndication;
    uint8_t profileCompatibility;
    uint8_t avcLevelIndication;
    uint8_t lengthSizeMinusOne;   // NAL unit length field size (0-3)
    
    // Sequence Parameter Sets
    std::vector<std::vector<uint8_t>> sequenceParameterSets;
    
    // Picture Parameter Sets
    std::vector<std::vector<uint8_t>> pictureParameterSets;
    
    // High profile fields (if profile > 66)
    uint8_t chromaFormat;
    uint8_t bitDepthLumaMinus8;
    uint8_t bitDepthChromaMinus8;
    std::vector<std::vector<uint8_t>> sequenceParameterSetExt;
};
```

### Box Type Definitions

```cpp
// ISO box type constants
constexpr uint32_t BOX_FTYP = FOURCC('f','t','y','p');  // File type
constexpr uint32_t BOX_MOOV = FOURCC('m','o','o','v');  // Movie
constexpr uint32_t BOX_MDAT = FOURCC('m','d','a','t');  // Media data
constexpr uint32_t BOX_MVHD = FOURCC('m','v','h','d');  // Movie header
constexpr uint32_t BOX_TRAK = FOURCC('t','r','a','k');  // Track
constexpr uint32_t BOX_TKHD = FOURCC('t','k','h','d');  // Track header
constexpr uint32_t BOX_MDIA = FOURCC('m','d','i','a');  // Media
constexpr uint32_t BOX_MDHD = FOURCC('m','d','h','d');  // Media header
constexpr uint32_t BOX_HDLR = FOURCC('h','d','l','r');  // Handler reference
constexpr uint32_t BOX_MINF = FOURCC('m','i','n','f');  // Media information
constexpr uint32_t BOX_DINF = FOURCC('d','i','n','f');  // Data information
constexpr uint32_t BOX_DREF = FOURCC('d','r','e','f');  // Data reference
constexpr uint32_t BOX_STBL = FOURCC('s','t','b','l');  // Sample table
constexpr uint32_t BOX_STSD = FOURCC('s','t','s','d');  // Sample description
constexpr uint32_t BOX_STTS = FOURCC('s','t','t','s');  // Time-to-sample
constexpr uint32_t BOX_CTTS = FOURCC('c','t','t','s');  // Composition time-to-sample
constexpr uint32_t BOX_STSC = FOURCC('s','t','s','c');  // Sample-to-chunk
constexpr uint32_t BOX_STSZ = FOURCC('s','t','s','z');  // Sample size
constexpr uint32_t BOX_STZ2 = FOURCC('s','t','z','2');  // Compact sample size
constexpr uint32_t BOX_STCO = FOURCC('s','t','c','o');  // Chunk offset (32-bit)
constexpr uint32_t BOX_CO64 = FOURCC('c','o','6','4');  // Chunk offset (64-bit)
constexpr uint32_t BOX_STSS = FOURCC('s','t','s','s');  // Sync sample
constexpr uint32_t BOX_EDTS = FOURCC('e','d','t','s');  // Edit
constexpr uint32_t BOX_ELST = FOURCC('e','l','s','t');  // Edit list
constexpr uint32_t BOX_UDTA = FOURCC('u','d','t','a');  // User data
constexpr uint32_t BOX_META = FOURCC('m','e','t','a');  // Metadata
constexpr uint32_t BOX_CPRT = FOURCC('c','p','r','t');  // Copyright

// Fragmented MP4 boxes
constexpr uint32_t BOX_MVEX = FOURCC('m','v','e','x');  // Movie extends
constexpr uint32_t BOX_MEHD = FOURCC('m','e','h','d');  // Movie extends header
constexpr uint32_t BOX_TREX = FOURCC('t','r','e','x');  // Track extends
constexpr uint32_t BOX_MOOF = FOURCC('m','o','o','f');  // Movie fragment
constexpr uint32_t BOX_MFHD = FOURCC('m','f','h','d');  // Movie fragment header
constexpr uint32_t BOX_TRAF = FOURCC('t','r','a','f');  // Track fragment
constexpr uint32_t BOX_TFHD = FOURCC('t','f','h','d');  // Track fragment header
constexpr uint32_t BOX_TRUN = FOURCC('t','r','u','n');  // Track run
constexpr uint32_t BOX_MFRA = FOURCC('m','f','r','a');  // Movie fragment random access
constexpr uint32_t BOX_TFRA = FOURCC('t','f','r','a');  // Track fragment random access
constexpr uint32_t BOX_SIDX = FOURCC('s','i','d','x');  // Segment index

// Audio codec types
constexpr uint32_t CODEC_AAC  = FOURCC('m','p','4','a');  // AAC
constexpr uint32_t CODEC_ALAC = FOURCC('a','l','a','c');  // Apple Lossless
constexpr uint32_t CODEC_ULAW = FOURCC('u','l','a','w');  // μ-law
constexpr uint32_t CODEC_ALAW = FOURCC('a','l','a','w');  // A-law
constexpr uint32_t CODEC_LPCM = FOURCC('l','p','c','m');  // Linear PCM
constexpr uint32_t CODEC_FLAC = FOURCC('f','L','a','C');  // FLAC

// Video codec types
constexpr uint32_t CODEC_AVC1 = FOURCC('a','v','c','1');  // H.264/AVC
constexpr uint32_t CODEC_AVC2 = FOURCC('a','v','c','2');  // H.264/AVC (no parameter sets)
constexpr uint32_t CODEC_AVC3 = FOURCC('a','v','c','3');  // H.264/AVC (parameter sets in-band)
constexpr uint32_t CODEC_AVC4 = FOURCC('a','v','c','4');  // H.264/AVC (no parameter sets, in-band)

// Handler types
constexpr uint32_t HANDLER_SOUN = FOURCC('s','o','u','n');  // Audio track
constexpr uint32_t HANDLER_VIDE = FOURCC('v','i','d','e');  // Video track
constexpr uint32_t HANDLER_HINT = FOURCC('h','i','n','t');  // Hint track
constexpr uint32_t HANDLER_META = FOURCC('m','e','t','a');  // Metadata track
constexpr uint32_t HANDLER_TEXT = FOURCC('t','e','x','t');  // Text/subtitle track

// Brand identifiers
constexpr uint32_t BRAND_ISOM = FOURCC('i','s','o','m');  // ISO Base Media
constexpr uint32_t BRAND_MP41 = FOURCC('m','p','4','1');  // MP4 version 1
constexpr uint32_t BRAND_MP42 = FOURCC('m','p','4','2');  // MP4 version 2
constexpr uint32_t BRAND_AVC1 = FOURCC('a','v','c','1');  // AVC/H.264
constexpr uint32_t BRAND_M4A  = FOURCC('M','4','A',' ');  // Audio-only MP4
constexpr uint32_t BRAND_QT   = FOURCC('q','t',' ',' ');  // QuickTime
constexpr uint32_t BRAND_3GP5 = FOURCC('3','g','p','5');  // 3GPP Release 5
constexpr uint32_t BRAND_3GP6 = FOURCC('3','g','p','6');  // 3GPP Release 6

// Track header flags
constexpr uint32_t TRACK_ENABLED    = 0x000001;  // Track is enabled
constexpr uint32_t TRACK_IN_MOVIE   = 0x000002;  // Track is in movie
constexpr uint32_t TRACK_IN_PREVIEW = 0x000004;  // Track is in preview
```

## Error Handling

### Graceful Degradation Strategy

1. **Box Parsing Errors**: Skip unknown/corrupted boxes, continue with valid data
2. **Sample Table Inconsistencies**: Use available data, interpolate missing entries
3. **Codec Configuration Missing**: Attempt to infer from sample data
4. **Fragmented File Issues**: Fall back to available fragments
5. **I/O Errors**: Retry with exponential backoff, report partial success
6. **Security Violations**: Reject file immediately, report security error
7. **Compliance Violations**: Report warnings for non-critical issues, reject for critical violations

### Error Recovery Mechanisms

```cpp
class ErrorRecovery {
public:
    // Attempt to recover from corrupted sample tables
    bool RepairSampleTables(SampleTableInfo& tables);
    
    // Infer codec configuration from sample data
    bool InferCodecConfig(const std::vector<uint8_t>& sampleData, 
                         TrackInfo& track);
    
    // Handle incomplete fragmented files
    bool RecoverFragmentedStream(std::vector<FragmentInfo>& fragments);
    
    // Validate and repair box sizes
    bool ValidateAndRepairBoxSize(uint64_t& size, uint64_t parentSize, uint64_t offset);
    
    // Handle missing mandatory boxes
    bool HandleMissingMandatoryBox(uint32_t boxType, const std::string& context);
    
private:
    static constexpr int MAX_REPAIR_ATTEMPTS = 3;
    static constexpr double BACKOFF_MULTIPLIER = 2.0;
    
    std::vector<std::string> recoveryLog;
};
```

### Security Error Handling

Security violations are treated as critical errors and result in immediate rejection:

```cpp
enum class SecurityViolation {
    BUFFER_OVERFLOW,
    INTEGER_OVERFLOW,
    EXCESSIVE_ALLOCATION,
    EXCESSIVE_RECURSION,
    INVALID_NAL_UNIT_SIZE,
    MALFORMED_CODEC_CONFIG,
    DECODER_BUFFER_VIOLATION
};

class SecurityErrorHandler {
public:
    void ReportViolation(SecurityViolation type, const std::string& details);
    bool ShouldRejectFile() const { return hasSecurityViolation; }
    std::string GetSecurityReport() const;
    
private:
    bool hasSecurityViolation = false;
    std::vector<std::pair<SecurityViolation, std::string>> violations;
};
```

### Compliance Error Handling

Compliance violations are categorized by severity:

```cpp
enum class ComplianceSeverity {
    INFO,       // Informational, no action needed
    WARNING,    // Non-critical deviation from spec
    ERROR,      // Critical violation, file should be rejected
    FATAL       // Unrecoverable error
};

class ComplianceErrorHandler {
public:
    void ReportIssue(ComplianceSeverity severity, const std::string& message);
    bool ShouldRejectFile() const;
    std::string GetComplianceReport() const;
    
private:
    std::vector<std::tuple<ComplianceSeverity, std::string, std::string>> issues;
    bool hasFatalError = false;
};
```

## MIME Type and Brand Identification

### MIME Type Determination

Per RFC 4337, MIME type is determined based on track content:

```cpp
class MIMETypeResolver {
public:
    std::string DetermineMIMEType(const std::vector<TrackInfo>& tracks);
    
private:
    bool HasVideoTracks(const std::vector<TrackInfo>& tracks);
    bool HasAudioTracks(const std::vector<TrackInfo>& tracks);
    bool HasMPEGJContent(const std::vector<TrackInfo>& tracks);
    bool HasMPEG7Content(const std::vector<TrackInfo>& tracks);
};
```

**MIME Type Rules:**
- `video/mp4`: File contains video tracks (with or without audio)
- `audio/mp4`: File contains only audio tracks
- `application/mp4`: File contains neither video nor audio (MPEG-J, MPEG-7, metadata)

### Brand Identification

Brand identification determines file format variant and compatibility:

```cpp
class BrandResolver {
public:
    struct BrandInfo {
        std::string majorBrand;
        std::vector<std::string> compatibleBrands;
        std::string formatDescription;
    };
    
    BrandInfo ParseFileTypeBox(const std::vector<uint8_t>& ftypData);
    bool IsBrandSupported(const std::string& brand);
    std::string GetFormatDescription(const std::string& majorBrand);
    
private:
    static const std::map<std::string, std::string> BRAND_DESCRIPTIONS;
};
```

**Supported Brands:**
- `isom`: ISO Base Media File Format
- `mp41`: MP4 version 1
- `mp42`: MP4 version 2
- `avc1`: AVC/H.264 video support
- `M4A `: Audio-only MP4
- `qt  `: QuickTime format
- `3gp5`, `3gp6`: 3GPP mobile formats

### File Extension Association

```cpp
class FileExtensionResolver {
public:
    std::string DetermineExtension(const std::string& majorBrand, 
                                   const std::vector<TrackInfo>& tracks);
    bool ValidateExtension(const std::string& filename, const std::string& majorBrand);
    
private:
    static const std::map<std::string, std::vector<std::string>> BRAND_TO_EXTENSIONS;
};
```

**Extension Mapping:**
- `.mp4`: General MP4 files
- `.m4a`: Audio-only MP4 files
- `.m4v`: Video MP4 files
- `.mpg4`: Legacy MP4 files
- `.mov`: QuickTime files
- `.3gp`: 3GPP mobile files
- `.f4a`: Flash audio files

## Testing Strategy

### Unit Testing Focus

1. **BoxParser**: Test nested structure parsing, 32-bit/64-bit sizes, unknown types, version handling
2. **SampleTableManager**: Verify lookup accuracy, memory efficiency, edge cases, edit list support
3. **FragmentHandler**: Test fragment ordering, incomplete fragments, segment indexing
4. **Codec Support**: Verify configuration extraction for AAC, ALAC, mulaw, alaw, PCM, FLAC, AVC
5. **Error Recovery**: Test graceful handling of various corruption scenarios
6. **ComplianceValidator**: Verify ISO/IEC 14496-12 compliance checking
7. **SecurityValidator**: Test buffer overflow protection, integer overflow detection
8. **SeekingEngine**: Verify edit list application, timeline mapping
9. **MIMETypeResolver**: Test MIME type determination based on track content
10. **BrandResolver**: Verify brand identification and compatibility checking

### Integration Testing

1. **Real-world Files**: Test with files from various encoders and sources
2. **Streaming Scenarios**: Test progressive download and fragmented playback
3. **Seeking Accuracy**: Verify sample-accurate seeking across different codecs with edit lists
4. **Memory Usage**: Profile memory consumption with large files
5. **Performance**: Benchmark parsing and seeking performance
6. **AVC Video**: Test H.264/AVC video track parsing and NAL unit handling
7. **Multi-track Files**: Test files with multiple audio/video tracks
8. **Brand Compatibility**: Test various brand combinations and compatibility

### Test File Coverage

- Standard MP4/M4A files (AAC, ALAC)
- Telephony files (mulaw/alaw in ISO containers)
- AVC/H.264 video files (MP4, M4V)
- Fragmented MP4 streams (DASH, HLS)
- Files with complex metadata (iTunes, ISO metadata)
- Files with edit lists (timeline modifications)
- Multi-track files (multiple audio/video tracks)
- Files with various brands (isom, mp41, mp42, avc1, M4A, qt, 3gp)
- Corrupted/incomplete files for error handling
- Large files for performance testing (>4GB, requiring 64-bit offsets)
- Files with security issues (malformed sizes, excessive allocations)
- Non-compliant files for compliance validation

### Security Testing

1. **Buffer Overflow Protection**: Test with malformed sample sizes
2. **Integer Overflow**: Test with extreme box sizes and counts
3. **Recursion Limits**: Test with deeply nested box structures
4. **NAL Unit Validation**: Test with malformed AVC data
5. **Allocation Limits**: Test with files requesting excessive memory
6. **Decoder Buffer Model**: Verify buffer model compliance per RFC 4337

### Compliance Testing

1. **Mandatory Boxes**: Verify presence of required boxes (ftyp, moov, mvhd, trak, tkhd, mdia, mdhd, hdlr, minf, stbl)
2. **Box Hierarchy**: Verify correct parent-child relationships
3. **Version Handling**: Test both version 0 and version 1 of versioned boxes
4. **Timescale Validation**: Test various timescale configurations
5. **Duration Calculations**: Verify accurate duration across timescales
6. **Track Flags**: Test track enabled, in movie, in preview flags
7. **Data References**: Test local and external data references
8. **Edit Lists**: Verify edit list application and timeline mapping

## Performance Considerations

### Memory Optimization

- Lazy loading of sample size tables for large files
- Compressed sample-to-chunk mappings
- Streaming box parser with minimal buffering
- Efficient metadata caching strategies
- Variant storage for constant vs. variable sample sizes
- On-demand loading of edit lists and data references

### Seeking Performance

- Binary search for time-to-sample lookups (O(log n) complexity)
- Cached chunk offset calculations
- Keyframe-aware seeking for compressed codecs
- Optimized sample table indexing
- Edit list caching for repeated timeline mapping
- Fragment index for fast seeking in fragmented files

### I/O Efficiency

- Minimize file seeks through sequential parsing
- Batch sample reads for improved throughput
- Efficient handling of fragmented files
- Smart prefetching for streaming scenarios
- Byte range request optimization for HTTP streaming
- Progressive parsing for incomplete files

### Parsing Optimization

- Iterative box parsing to prevent stack overflow (vs. recursive)
- Early termination for unknown/skippable boxes
- Efficient FourCC comparison using integer operations
- Minimal memory allocation during parsing
- Reuse of parser state across multiple files

### Codec Configuration Caching

- Parse codec configuration once during initialization
- Cache AudioSpecificConfig, ALAC magic cookie, avcC data
- Avoid repeated parsing of sample description boxes
- Efficient storage of NAL unit parameter sets

### Scalability

- Support for files >4GB using 64-bit offsets
- Efficient handling of millions of samples
- Graceful degradation with limited memory
- Parallel track processing where possible

## Design Rationale

### Component Separation

The design separates concerns into distinct components:
- **BoxParser**: Handles low-level box parsing and validation
- **SampleTableManager**: Manages sample lookup and timing
- **FragmentHandler**: Isolates fragmented MP4 complexity
- **MetadataExtractor**: Separates metadata from core demuxing
- **StreamManager**: Manages track selection and filtering
- **SeekingEngine**: Centralizes seeking logic with edit list support
- **ComplianceValidator**: Ensures standards compliance
- **SecurityValidator**: Provides security layer

This separation allows:
- Independent testing of each component
- Easy maintenance and bug fixes
- Clear responsibility boundaries
- Potential for future optimization of individual components

### Standards Compliance

Full compliance with ISO/IEC 14496-12, ISO/IEC 14496-14, ISO/IEC 14496-15, and RFC 4337 ensures:
- Interoperability with files from various encoders
- Correct handling of edge cases defined in specifications
- Future-proof design as standards evolve
- Proper MIME type identification for web delivery

### Security First

Security validation is integrated throughout:
- Buffer overflow protection prevents crashes and exploits
- Integer overflow detection prevents memory corruption
- Recursion limits prevent stack overflow attacks
- Allocation limits prevent denial-of-service
- Compliance with RFC 4337 security considerations

### Extensibility

The design allows for future extensions:
- Additional codec support (HEVC, VP9, AV1)
- New box types can be added without major refactoring
- Fragment handling can be extended for new streaming protocols
- Metadata extraction can support additional formats

This design provides a robust, efficient, secure, and standards-compliant ISO demuxer that handles the complexity of modern container formats while maintaining the simplicity and performance requirements of PsyMP3's architecture.