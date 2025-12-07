# ISO Demuxer Design

## Overview

This document specifies the complete redesign of the ISO Base Media File Format demuxer for PsyMP3. The demuxer provides full compliance with ISO/IEC 14496-12:2015, ISO/IEC 14496-15 (AVC), and RFC 4337 (MIME types).

### Design Goals

- **Complete ISO/IEC 14496-12:2015 Coverage**: Support all box types defined in the specification
- **Modular Architecture**: Clean separation of concerns with single-responsibility components
- **Thread Safety**: Follow PsyMP3's public/private lock pattern for all shared state
- **Memory Efficiency**: Lazy loading, compressed tables, streaming parsing
- **Security First**: Comprehensive validation against malformed files
- **Streaming Support**: Fragmented MP4, DASH, progressive download

## Architecture

### Component Hierarchy

```
ISODemuxer (Demuxer interface implementation)
│
├── ISOBoxReader                    # Low-level box I/O and header parsing
│   └── BoxHeader, FullBoxHeader    # Box header structures
│
├── ISOContainerParser              # High-level container structure parsing
│   ├── FileTypeParser              # ftyp, styp parsing
│   ├── MovieParser                 # moov structure parsing
│   ├── TrackParser                 # trak structure parsing
│   └── FragmentParser              # moof, mfra parsing
│
├── ISOSampleTableManager           # Sample table operations
│   ├── TimeToSampleIndex           # stts, ctts lookups
│   ├── SampleToChunkIndex          # stsc lookups
│   ├── SampleSizeIndex             # stsz, stz2 lookups
│   ├── ChunkOffsetIndex            # stco, co64 lookups
│   ├── SyncSampleIndex             # stss, stsh lookups
│   └── SampleGroupIndex            # sbgp, sgpd lookups
│
├── ISOSeekingEngine                # Seeking with edit list support
│   ├── EditListProcessor           # elst timeline mapping
│   └── CompositionTimeMapper       # ctts, cslg handling
│
├── ISOFragmentManager              # Fragmented MP4 support
│   ├── MovieExtendsHandler         # mvex, trex defaults
│   ├── FragmentIndexBuilder        # mfra, tfra indexing
│   └── SegmentIndexHandler         # sidx, ssix navigation
│
├── ISOMetadataExtractor            # Metadata extraction
│   ├── iTunesMetadataParser        # ilst parsing
│   ├── ISOMetadataParser           # meta, udta parsing
│   └── ItemLocationResolver        # iloc, iinf, iref handling
│
├── ISOCodecConfigExtractor         # Codec configuration
│   ├── AACConfigParser             # esds, AudioSpecificConfig
│   ├── ALACConfigParser            # alac magic cookie
│   ├── AVCConfigParser             # avcC, SPS/PPS
│   ├── FLACConfigParser            # dfLa configuration
│   └── TelephonyConfigParser       # ulaw, alaw configuration
│
├── ISOComplianceValidator          # ISO/IEC 14496-12 compliance
│   ├── MandatoryBoxValidator       # Required box presence
│   ├── BoxHierarchyValidator       # Parent-child relationships
│   └── TimingValidator             # Timescale, duration validation
│
└── ISOSecurityValidator            # Security and buffer protection
    ├── SizeValidator               # Buffer overflow prevention
    ├── DepthValidator              # Recursion limiting
    └── AllocationValidator         # Memory limit enforcement
```

### Namespace Organization

All components reside in `PsyMP3::Demuxer::ISO` namespace:

```cpp
namespace PsyMP3 {
namespace Demuxer {
namespace ISO {
    // Forward declarations
    class ISODemuxer;
    class ISOBoxReader;
    class ISOContainerParser;
    class ISOSampleTableManager;
    class ISOSeekingEngine;
    class ISOFragmentManager;
    class ISOMetadataExtractor;
    class ISOCodecConfigExtractor;
    class ISOComplianceValidator;
    class ISOSecurityValidator;
}}}
```

## Core Data Structures

### Box Header Structures

```cpp
// Basic box header (8 or 16 bytes)
struct BoxHeader {
    uint32_t type;              // Four-character code
    uint64_t size;              // Total box size including header
    uint64_t dataOffset;        // Offset to box data (after header)
    uint64_t dataSize;          // Size of box data (size - header)
    bool isExtendedSize;        // True if original size field was 1
    bool extendsToEOF;          // True if original size field was 0
};

// FullBox header (adds version and flags)
struct FullBoxHeader : BoxHeader {
    uint8_t version;            // Box version (0 or 1 typically)
    uint32_t flags;             // 24-bit flags field
};

// UUID extended type box
struct UUIDBoxHeader : BoxHeader {
    std::array<uint8_t, 16> uuid;  // 128-bit UUID
};
```

### File Type Information

```cpp
struct FileTypeInfo {
    uint32_t majorBrand;                    // Primary brand (e.g., 'isom', 'mp42')
    uint32_t minorVersion;                  // Minor version number
    std::vector<uint32_t> compatibleBrands; // List of compatible brands
    
    bool HasBrand(uint32_t brand) const;
    std::string GetMIMEType(bool hasVideo, bool hasAudio) const;
};
```

### Movie Header Information

```cpp
struct MovieHeaderInfo {
    uint8_t version;                        // 0 = 32-bit times, 1 = 64-bit times
    uint64_t creationTime;                  // Seconds since 1904-01-01 00:00:00 UTC
    uint64_t modificationTime;              // Seconds since 1904-01-01 00:00:00 UTC
    uint32_t timescale;                     // Time units per second
    uint64_t duration;                      // Duration in timescale units
    int32_t rate;                           // Playback rate (16.16 fixed-point, 0x10000 = 1.0)
    int16_t volume;                         // Audio volume (8.8 fixed-point, 0x100 = 1.0)
    std::array<int32_t, 9> matrix;          // Transformation matrix
    uint32_t nextTrackId;                   // Next available track ID
    
    double GetDurationSeconds() const { return static_cast<double>(duration) / timescale; }
    float GetRate() const { return static_cast<float>(rate) / 65536.0f; }
    float GetVolume() const { return static_cast<float>(volume) / 256.0f; }
};
```

### Track Header Information

```cpp
struct TrackHeaderInfo {
    uint8_t version;                        // 0 = 32-bit times, 1 = 64-bit times
    uint32_t flags;                         // Track flags
    uint32_t trackId;                       // Unique track identifier (never 0)
    uint64_t creationTime;
    uint64_t modificationTime;
    uint64_t duration;                      // Duration in movie timescale
    int16_t layer;                          // Visual stacking order (0 = front)
    int16_t alternateGroup;                 // Alternate track group (0 = none)
    int16_t volume;                         // Audio volume (8.8 fixed-point)
    std::array<int32_t, 9> matrix;          // Transformation matrix
    uint32_t width;                         // Visual width (16.16 fixed-point)
    uint32_t height;                        // Visual height (16.16 fixed-point)
    
    // Flag accessors
    bool IsEnabled() const { return (flags & 0x000001) != 0; }
    bool IsInMovie() const { return (flags & 0x000002) != 0; }
    bool IsInPreview() const { return (flags & 0x000004) != 0; }
    
    float GetWidth() const { return static_cast<float>(width) / 65536.0f; }
    float GetHeight() const { return static_cast<float>(height) / 65536.0f; }
};
```

### Media Header Information

```cpp
struct MediaHeaderInfo {
    uint8_t version;
    uint64_t creationTime;
    uint64_t modificationTime;
    uint32_t timescale;                     // Media time units per second
    uint64_t duration;                      // Duration in media timescale
    uint16_t language;                      // ISO 639-2/T packed language code
    
    std::string GetLanguageCode() const;    // Unpack to 3-character string
    double GetDurationSeconds() const { return static_cast<double>(duration) / timescale; }
};
```

### Extended Language Tag

```cpp
struct ExtendedLanguageTag {
    std::string languageTag;                // BCP 47 language tag (e.g., "en-US")
    
    std::string GetPrimaryLanguage() const;
    std::string GetRegion() const;
    std::string GetScript() const;
};
```

### Handler Information

```cpp
struct HandlerInfo {
    uint32_t handlerType;                   // 'soun', 'vide', 'hint', 'meta', 'text', 'subt'
    std::string name;                       // Human-readable handler name
    
    bool IsAudio() const { return handlerType == FOURCC('s','o','u','n'); }
    bool IsVideo() const { return handlerType == FOURCC('v','i','d','e'); }
    bool IsHint() const { return handlerType == FOURCC('h','i','n','t'); }
    bool IsMetadata() const { return handlerType == FOURCC('m','e','t','a'); }
    bool IsText() const { return handlerType == FOURCC('t','e','x','t'); }
    bool IsSubtitle() const { return handlerType == FOURCC('s','u','b','t'); }
};
```

### Sample Description

```cpp
struct SampleDescription {
    uint32_t format;                        // Codec four-character code
    uint16_t dataReferenceIndex;            // Index into dref (1-based)
    std::vector<uint8_t> codecConfig;       // Codec-specific configuration
    
    // Audio-specific fields
    uint16_t channelCount;
    uint16_t sampleSize;                    // Bits per sample
    uint32_t sampleRate;                    // Sample rate (16.16 fixed-point in file)
    
    // Video-specific fields
    uint16_t width;
    uint16_t height;
    uint32_t horizResolution;               // 16.16 fixed-point (72.0 dpi typical)
    uint32_t vertResolution;
    std::string compressorName;
    uint16_t depth;                         // Bit depth
};
```

### Data Reference

```cpp
struct DataReference {
    enum class Type { URL, URN, SELF };
    
    Type type;
    std::string location;                   // URL or URN string (empty for SELF)
    bool isSelfContained;                   // True if data is in same file
};
```

### Edit List Entry

```cpp
struct EditListEntry {
    uint64_t segmentDuration;               // Duration in movie timescale
    int64_t mediaTime;                      // Start time in media timescale (-1 = empty edit)
    int32_t mediaRateInteger;               // Integer part of rate
    int16_t mediaRateFraction;              // Fraction part of rate
    
    bool IsEmptyEdit() const { return mediaTime == -1; }
    double GetMediaRate() const { 
        return static_cast<double>(mediaRateInteger) + 
               static_cast<double>(mediaRateFraction) / 65536.0; 
    }
};
```

### Sample Table Structures

```cpp
// Time-to-sample entry (stts)
struct TimeToSampleEntry {
    uint32_t sampleCount;
    uint32_t sampleDelta;                   // Duration in media timescale
};

// Composition offset entry (ctts)
struct CompositionOffsetEntry {
    uint32_t sampleCount;
    int32_t sampleOffset;                   // Signed in version 1, unsigned in version 0
};

// Sample-to-chunk entry (stsc)
struct SampleToChunkEntry {
    uint32_t firstChunk;                    // 1-based chunk number
    uint32_t samplesPerChunk;
    uint32_t sampleDescriptionIndex;        // 1-based index into stsd
};

// Sync sample entry (stss) - just sample numbers
// Shadow sync entry (stsh)
struct ShadowSyncEntry {
    uint32_t shadowedSampleNumber;
    uint32_t syncSampleNumber;
};

// Sample dependency type (sdtp)
struct SampleDependencyType {
    uint8_t isLeading : 2;
    uint8_t sampleDependsOn : 2;
    uint8_t sampleIsDependedOn : 2;
    uint8_t sampleHasRedundancy : 2;
};

// Composition to decode timeline (cslg)
struct CompositionShiftInfo {
    int64_t compositionToDTSShift;
    int64_t leastDecodeToDisplayDelta;
    int64_t greatestDecodeToDisplayDelta;
    int64_t compositionStartTime;
    int64_t compositionEndTime;
};
```

### Sample Group Structures

```cpp
// Sample-to-group mapping (sbgp)
struct SampleToGroupEntry {
    uint32_t sampleCount;
    uint32_t groupDescriptionIndex;         // 0 = no group, 1-based otherwise
};

// Sample group description (sgpd) - base structure
struct SampleGroupDescription {
    uint32_t groupingType;                  // Four-character code
    uint32_t defaultLength;
    std::vector<std::vector<uint8_t>> entries;
};
```

### Sub-Sample Information

```cpp
struct SubSampleEntry {
    uint32_t subsampleSize;
    uint8_t subsamplePriority;
    uint8_t discardable;
    uint32_t codecSpecificParameters;
};

struct SampleSubSampleInfo {
    uint32_t sampleDelta;                   // Samples since last entry with sub-samples
    std::vector<SubSampleEntry> subsamples;
};
```

### Sample Auxiliary Information

```cpp
struct SampleAuxiliaryInfo {
    uint32_t auxInfoType;                   // Four-character code (optional)
    uint32_t auxInfoTypeParameter;          // Type-specific parameter
    std::vector<uint8_t> defaultSize;       // Default size or empty
    std::vector<uint32_t> sizes;            // Per-sample sizes if not default
    std::vector<uint64_t> offsets;          // Offsets to auxiliary data
};
```

### Track Information (Complete)

```cpp
struct TrackInfo {
    // Identity
    uint32_t trackId;
    
    // Headers
    TrackHeaderInfo trackHeader;
    MediaHeaderInfo mediaHeader;
    HandlerInfo handler;
    std::optional<ExtendedLanguageTag> extendedLanguage;
    
    // Sample descriptions
    std::vector<SampleDescription> sampleDescriptions;
    
    // Data references
    std::vector<DataReference> dataReferences;
    
    // Sample tables (managed by ISOSampleTableManager)
    std::vector<TimeToSampleEntry> timeToSample;
    std::vector<CompositionOffsetEntry> compositionOffsets;
    std::optional<CompositionShiftInfo> compositionShift;
    std::vector<SampleToChunkEntry> sampleToChunk;
    std::variant<uint32_t, std::vector<uint32_t>> sampleSizes;  // Constant or per-sample
    std::vector<uint64_t> chunkOffsets;
    std::vector<uint32_t> syncSamples;                          // Empty = all sync
    std::vector<ShadowSyncEntry> shadowSyncSamples;
    std::vector<uint16_t> degradationPriorities;
    std::vector<SampleDependencyType> sampleDependencies;
    std::vector<uint8_t> paddingBits;
    
    // Sample groups
    std::vector<std::pair<uint32_t, std::vector<SampleToGroupEntry>>> sampleGroups;
    std::vector<SampleGroupDescription> groupDescriptions;
    
    // Sub-sample information
    std::vector<SampleSubSampleInfo> subSamples;
    
    // Sample auxiliary information
    std::vector<SampleAuxiliaryInfo> auxiliaryInfo;
    
    // Edit list
    std::vector<EditListEntry> editList;
    
    // Track references
    std::map<uint32_t, std::vector<uint32_t>> trackReferences;  // type -> track IDs
    
    // Track groups
    std::vector<std::pair<uint32_t, uint32_t>> trackGroups;     // type, id pairs
    
    // Track selection
    uint32_t switchGroup = 0;
    std::vector<uint32_t> selectionAttributes;
    
    // Computed values
    uint64_t totalSamples = 0;
    uint64_t totalDuration = 0;
    
    // Convenience methods
    bool IsAudio() const { return handler.IsAudio(); }
    bool IsVideo() const { return handler.IsVideo(); }
    bool IsEnabled() const { return trackHeader.IsEnabled(); }
    bool HasConstantSampleSize() const { return std::holds_alternative<uint32_t>(sampleSizes); }
    bool AllSamplesAreSync() const { return syncSamples.empty(); }
};
```



### Fragment Structures

```cpp
// Track extends defaults (trex)
struct TrackExtendsDefaults {
    uint32_t trackId;
    uint32_t defaultSampleDescriptionIndex;
    uint32_t defaultSampleDuration;
    uint32_t defaultSampleSize;
    uint32_t defaultSampleFlags;
};

// Track fragment header (tfhd)
struct TrackFragmentHeader {
    uint32_t trackId;
    uint32_t flags;
    
    // Optional fields based on flags
    std::optional<uint64_t> baseDataOffset;
    std::optional<uint32_t> sampleDescriptionIndex;
    std::optional<uint32_t> defaultSampleDuration;
    std::optional<uint32_t> defaultSampleSize;
    std::optional<uint32_t> defaultSampleFlags;
    
    // Flag accessors
    bool HasBaseDataOffset() const { return (flags & 0x000001) != 0; }
    bool HasSampleDescriptionIndex() const { return (flags & 0x000002) != 0; }
    bool HasDefaultSampleDuration() const { return (flags & 0x000008) != 0; }
    bool HasDefaultSampleSize() const { return (flags & 0x000010) != 0; }
    bool HasDefaultSampleFlags() const { return (flags & 0x000020) != 0; }
    bool DurationIsEmpty() const { return (flags & 0x010000) != 0; }
    bool DefaultBaseIsMoof() const { return (flags & 0x020000) != 0; }
};

// Track run sample (trun)
struct TrackRunSample {
    std::optional<uint32_t> duration;
    std::optional<uint32_t> size;
    std::optional<uint32_t> flags;
    std::optional<int32_t> compositionTimeOffset;
};

// Track run (trun)
struct TrackRun {
    uint32_t flags;
    std::optional<int32_t> dataOffset;
    std::optional<uint32_t> firstSampleFlags;
    std::vector<TrackRunSample> samples;
    
    // Flag accessors
    bool HasDataOffset() const { return (flags & 0x000001) != 0; }
    bool HasFirstSampleFlags() const { return (flags & 0x000004) != 0; }
    bool HasSampleDuration() const { return (flags & 0x000100) != 0; }
    bool HasSampleSize() const { return (flags & 0x000200) != 0; }
    bool HasSampleFlags() const { return (flags & 0x000400) != 0; }
    bool HasSampleCompositionTimeOffset() const { return (flags & 0x000800) != 0; }
};

// Track fragment decode time (tfdt)
struct TrackFragmentDecodeTime {
    uint8_t version;
    uint64_t baseMediaDecodeTime;
};

// Track fragment (complete)
struct TrackFragment {
    TrackFragmentHeader header;
    std::optional<TrackFragmentDecodeTime> decodeTime;
    std::vector<TrackRun> runs;
    std::vector<std::pair<uint32_t, std::vector<SampleToGroupEntry>>> sampleGroups;
    std::vector<SampleGroupDescription> groupDescriptions;
    std::vector<SampleSubSampleInfo> subSamples;
    std::vector<SampleAuxiliaryInfo> auxiliaryInfo;
};

// Movie fragment
struct MovieFragment {
    uint32_t sequenceNumber;
    std::vector<TrackFragment> trackFragments;
    uint64_t moofOffset;
    uint64_t mdatOffset;
};

// Track fragment random access entry (tfra)
struct TrackFragmentRandomAccessEntry {
    uint64_t time;
    uint64_t moofOffset;
    uint32_t trafNumber;
    uint32_t trunNumber;
    uint32_t sampleNumber;
};

// Track fragment random access (tfra)
struct TrackFragmentRandomAccess {
    uint32_t trackId;
    std::vector<TrackFragmentRandomAccessEntry> entries;
};

// Segment index reference (sidx)
struct SegmentIndexReference {
    bool referenceType;                     // 0 = media, 1 = index
    uint32_t referencedSize;
    uint32_t subsegmentDuration;
    bool startsWithSAP;
    uint8_t sapType;
    uint32_t sapDeltaTime;
};

// Segment index (sidx)
struct SegmentIndex {
    uint32_t referenceId;
    uint32_t timescale;
    uint64_t earliestPresentationTime;
    uint64_t firstOffset;
    std::vector<SegmentIndexReference> references;
};

// Subsegment index (ssix)
struct SubsegmentIndexRange {
    uint8_t level;
    uint32_t rangeSize;
};

struct SubsegmentIndex {
    uint32_t subsegmentCount;
    std::vector<std::vector<SubsegmentIndexRange>> ranges;
};

// Producer reference time (prft)
struct ProducerReferenceTime {
    uint32_t referenceTrackId;
    uint64_t ntpTimestamp;
    uint64_t mediaTime;
};
```

### Metadata Structures

```cpp
// Item location extent
struct ItemLocationExtent {
    uint64_t extentIndex;
    uint64_t extentOffset;
    uint64_t extentLength;
};

// Item location entry (iloc)
struct ItemLocationEntry {
    uint32_t itemId;
    uint8_t constructionMethod;             // 0 = file, 1 = idat, 2 = item
    uint16_t dataReferenceIndex;
    uint64_t baseOffset;
    std::vector<ItemLocationExtent> extents;
};

// Item information entry (iinf/infe)
struct ItemInfoEntry {
    uint32_t itemId;
    uint16_t itemProtectionIndex;
    std::string itemName;
    std::string contentType;
    std::string contentEncoding;
    uint32_t itemType;                      // Four-character code (version >= 2)
    std::string itemUriType;
    bool isHidden;
};

// Item reference (iref)
struct ItemReference {
    uint32_t referenceType;                 // 'dimg', 'thmb', 'cdsc', 'auxl', etc.
    uint32_t fromItemId;
    std::vector<uint32_t> toItemIds;
};

// Protection scheme info (sinf)
struct ProtectionSchemeInfo {
    uint32_t originalFormat;                // Original codec type
    uint32_t schemeType;                    // 'cenc', 'cbcs', etc.
    uint32_t schemeVersion;
    std::vector<uint8_t> schemeInfo;        // Scheme-specific data
};
```

### Codec Configuration Structures

```cpp
// AAC AudioSpecificConfig
struct AACConfig {
    uint8_t audioObjectType;
    uint8_t samplingFrequencyIndex;
    uint32_t samplingFrequency;             // Explicit if index == 15
    uint8_t channelConfiguration;
    bool frameLengthFlag;
    bool dependsOnCoreCoder;
    bool extensionFlag;
    
    // SBR/PS extension
    bool hasSBR;
    bool hasPS;
    uint8_t extensionAudioObjectType;
    uint32_t extensionSamplingFrequency;
};

// ALAC magic cookie
struct ALACConfig {
    uint32_t frameLength;
    uint8_t compatibleVersion;
    uint8_t bitDepth;
    uint8_t pb;                             // Rice parameter
    uint8_t mb;                             // Rice parameter
    uint8_t kb;                             // Rice parameter
    uint8_t numChannels;
    uint16_t maxRun;
    uint32_t maxFrameBytes;
    uint32_t avgBitRate;
    uint32_t sampleRate;
};

// AVC decoder configuration (avcC)
struct AVCDecoderConfig {
    uint8_t configurationVersion;
    uint8_t avcProfileIndication;
    uint8_t profileCompatibility;
    uint8_t avcLevelIndication;
    uint8_t lengthSizeMinusOne;             // NAL unit length field size - 1
    
    std::vector<std::vector<uint8_t>> sequenceParameterSets;
    std::vector<std::vector<uint8_t>> pictureParameterSets;
    
    // High profile extensions (profile_idc >= 100)
    std::optional<uint8_t> chromaFormat;
    std::optional<uint8_t> bitDepthLumaMinus8;
    std::optional<uint8_t> bitDepthChromaMinus8;
    std::vector<std::vector<uint8_t>> sequenceParameterSetExt;
    
    uint8_t GetNALUnitLengthSize() const { return lengthSizeMinusOne + 1; }
};

// FLAC decoder configuration (dfLa)
struct FLACConfig {
    uint16_t minBlockSize;
    uint16_t maxBlockSize;
    uint32_t minFrameSize;
    uint32_t maxFrameSize;
    uint32_t sampleRate;
    uint8_t channels;
    uint8_t bitsPerSample;
    uint64_t totalSamples;
    std::array<uint8_t, 16> md5Signature;
};

// Channel layout (chnl)
struct ChannelLayout {
    uint8_t streamStructure;
    uint8_t definedLayout;
    std::vector<uint8_t> speakerPositions;  // If definedLayout == 0
    uint64_t omittedChannelsMap;
    uint8_t objectCount;
};
```

## Component Interfaces

### ISOBoxReader

Low-level box I/O operations:

```cpp
class ISOBoxReader {
public:
    explicit ISOBoxReader(std::shared_ptr<IOHandler> io);
    
    // Box header reading
    std::optional<BoxHeader> ReadBoxHeader(uint64_t offset);
    std::optional<FullBoxHeader> ReadFullBoxHeader(uint64_t offset);
    std::optional<UUIDBoxHeader> ReadUUIDBoxHeader(uint64_t offset);
    
    // Primitive reading at current position
    uint8_t ReadUInt8();
    uint16_t ReadUInt16BE();
    uint32_t ReadUInt32BE();
    uint64_t ReadUInt64BE();
    int16_t ReadInt16BE();
    int32_t ReadInt32BE();
    int64_t ReadInt64BE();
    uint32_t ReadFourCC();
    std::string ReadString(size_t length);
    std::string ReadNullTerminatedString();
    std::vector<uint8_t> ReadBytes(size_t count);
    
    // Fixed-point reading
    float ReadFixed16_16();
    float ReadFixed8_8();
    float ReadFixed2_30();
    
    // Matrix reading
    std::array<int32_t, 9> ReadMatrix();
    
    // Position management
    uint64_t GetPosition() const;
    bool Seek(uint64_t offset);
    bool Skip(uint64_t bytes);
    uint64_t GetFileSize() const;
    
    // Validation
    bool ValidateBoxBounds(const BoxHeader& header, uint64_t parentEnd);
    
private:
    std::shared_ptr<IOHandler> io_;
    uint64_t position_ = 0;
    uint64_t fileSize_ = 0;
    
    mutable std::mutex mutex_;
};
```

### ISOContainerParser

High-level container structure parsing:

```cpp
class ISOContainerParser {
public:
    ISOContainerParser(std::shared_ptr<ISOBoxReader> reader,
                       std::shared_ptr<ISOSecurityValidator> security);
    
    // Top-level parsing
    bool ParseFile();
    
    // Accessors
    const FileTypeInfo& GetFileType() const;
    const MovieHeaderInfo& GetMovieHeader() const;
    const std::vector<TrackInfo>& GetTracks() const;
    bool IsFragmented() const;
    
    // Fragment access
    const std::vector<TrackExtendsDefaults>& GetTrackExtends() const;
    std::optional<uint64_t> GetFragmentDuration() const;
    
private:
    // Box parsing methods (organized by container)
    bool ParseFileTypeBox(const BoxHeader& header);
    bool ParseSegmentTypeBox(const BoxHeader& header);
    bool ParseMovieBox(const BoxHeader& header);
    bool ParseMovieHeaderBox(const FullBoxHeader& header);
    bool ParseTrackBox(const BoxHeader& header);
    bool ParseTrackHeaderBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParseTrackReferenceBox(const BoxHeader& header, TrackInfo& track);
    bool ParseTrackGroupBox(const BoxHeader& header, TrackInfo& track);
    bool ParseEditBox(const BoxHeader& header, TrackInfo& track);
    bool ParseEditListBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParseMediaBox(const BoxHeader& header, TrackInfo& track);
    bool ParseMediaHeaderBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParseHandlerBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParseExtendedLanguageBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParseMediaInformationBox(const BoxHeader& header, TrackInfo& track);
    bool ParseDataInformationBox(const BoxHeader& header, TrackInfo& track);
    bool ParseDataReferenceBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParseSampleTableBox(const BoxHeader& header, TrackInfo& track);
    
    // Sample table parsing
    bool ParseSampleDescriptionBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParseTimeToSampleBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParseCompositionOffsetBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParseCompositionShiftBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParseSampleToChunkBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParseSampleSizeBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParseCompactSampleSizeBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParseChunkOffsetBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParseChunkOffset64Box(const FullBoxHeader& header, TrackInfo& track);
    bool ParseSyncSampleBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParseShadowSyncBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParseSampleDependencyTypeBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParseDegradationPriorityBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParsePaddingBitsBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParseSampleToGroupBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParseSampleGroupDescriptionBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParseSubSampleInformationBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParseSampleAuxInfoSizesBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParseSampleAuxInfoOffsetsBox(const FullBoxHeader& header, TrackInfo& track);
    
    // Fragment parsing
    bool ParseMovieExtendsBox(const BoxHeader& header);
    bool ParseMovieExtendsHeaderBox(const FullBoxHeader& header);
    bool ParseTrackExtendsBox(const FullBoxHeader& header);
    bool ParseMovieFragmentBox(const BoxHeader& header);
    bool ParseMovieFragmentHeaderBox(const FullBoxHeader& header, MovieFragment& fragment);
    bool ParseTrackFragmentBox(const BoxHeader& header, MovieFragment& fragment);
    bool ParseTrackFragmentHeaderBox(const FullBoxHeader& header, TrackFragment& traf);
    bool ParseTrackRunBox(const FullBoxHeader& header, TrackFragment& traf);
    bool ParseTrackFragmentDecodeTimeBox(const FullBoxHeader& header, TrackFragment& traf);
    
    // Random access parsing
    bool ParseMovieFragmentRandomAccessBox(const BoxHeader& header);
    bool ParseTrackFragmentRandomAccessBox(const FullBoxHeader& header);
    bool ParseMovieFragmentRandomAccessOffsetBox(const FullBoxHeader& header);
    
    // Segment index parsing
    bool ParseSegmentIndexBox(const FullBoxHeader& header);
    bool ParseSubsegmentIndexBox(const FullBoxHeader& header);
    bool ParseProducerReferenceTimeBox(const FullBoxHeader& header);
    
    // Media header parsing
    bool ParseVideoMediaHeaderBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParseSoundMediaHeaderBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParseHintMediaHeaderBox(const FullBoxHeader& header, TrackInfo& track);
    bool ParseNullMediaHeaderBox(const FullBoxHeader& header, TrackInfo& track);
    
    // Utility
    bool SkipBox(const BoxHeader& header);
    bool IterateContainerBox(const BoxHeader& container, 
                             std::function<bool(const BoxHeader&)> handler);
    
    std::shared_ptr<ISOBoxReader> reader_;
    std::shared_ptr<ISOSecurityValidator> security_;
    
    FileTypeInfo fileType_;
    MovieHeaderInfo movieHeader_;
    std::vector<TrackInfo> tracks_;
    
    // Fragment data
    bool isFragmented_ = false;
    std::optional<uint64_t> fragmentDuration_;
    std::vector<TrackExtendsDefaults> trackExtends_;
    std::vector<MovieFragment> fragments_;
    std::vector<TrackFragmentRandomAccess> fragmentRandomAccess_;
    std::vector<SegmentIndex> segmentIndices_;
    
    uint32_t recursionDepth_ = 0;
    
    mutable std::mutex mutex_;
};
```


### ISOSampleTableManager

Sample table operations and lookups:

```cpp
class ISOSampleTableManager {
public:
    explicit ISOSampleTableManager(const TrackInfo& track);
    
    // Sample count and duration
    uint64_t GetTotalSamples() const;
    uint64_t GetTotalDuration() const;
    double GetDurationSeconds() const;
    
    // Sample lookup by number (1-based)
    uint64_t GetSampleOffset(uint32_t sampleNumber) const;
    uint32_t GetSampleSize(uint32_t sampleNumber) const;
    uint64_t GetSampleTime(uint32_t sampleNumber) const;
    uint64_t GetSampleCompositionTime(uint32_t sampleNumber) const;
    uint32_t GetSampleDescriptionIndex(uint32_t sampleNumber) const;
    
    // Time-based lookup
    uint32_t GetSampleAtTime(uint64_t mediaTime) const;
    uint32_t GetSampleAtCompositionTime(uint64_t compositionTime) const;
    
    // Sync sample operations
    bool IsSyncSample(uint32_t sampleNumber) const;
    uint32_t GetNearestSyncSample(uint32_t sampleNumber, bool forward = true) const;
    uint32_t GetSyncSampleAtOrBefore(uint64_t mediaTime) const;
    
    // Chunk operations
    uint32_t GetChunkForSample(uint32_t sampleNumber) const;
    uint64_t GetChunkOffset(uint32_t chunkNumber) const;
    uint32_t GetFirstSampleInChunk(uint32_t chunkNumber) const;
    uint32_t GetSamplesInChunk(uint32_t chunkNumber) const;
    
    // Sample dependency
    SampleDependencyType GetSampleDependency(uint32_t sampleNumber) const;
    bool SampleDependsOnOthers(uint32_t sampleNumber) const;
    bool SampleIsDependedOn(uint32_t sampleNumber) const;
    
    // Sample groups
    uint32_t GetSampleGroupIndex(uint32_t sampleNumber, uint32_t groupingType) const;
    const std::vector<uint8_t>* GetSampleGroupDescription(uint32_t groupingType, 
                                                           uint32_t descriptionIndex) const;
    
    // Sub-sample information
    bool HasSubSamples(uint32_t sampleNumber) const;
    std::vector<SubSampleEntry> GetSubSamples(uint32_t sampleNumber) const;
    
    // Batch operations for efficiency
    struct SampleInfo {
        uint64_t offset;
        uint32_t size;
        uint64_t decodeTime;
        uint64_t compositionTime;
        uint32_t descriptionIndex;
        bool isSync;
    };
    
    SampleInfo GetSampleInfo(uint32_t sampleNumber) const;
    std::vector<SampleInfo> GetSampleRange(uint32_t firstSample, uint32_t count) const;
    
private:
    // Optimized index structures (built on construction)
    struct ChunkIndex {
        uint32_t firstSample;
        uint32_t sampleCount;
        uint32_t descriptionIndex;
        uint64_t offset;
    };
    
    struct TimeIndex {
        uint32_t firstSample;
        uint64_t startTime;
        uint32_t sampleCount;
        uint32_t delta;
    };
    
    void BuildIndices();
    void BuildChunkIndex();
    void BuildTimeIndex();
    void BuildSyncIndex();
    
    const TrackInfo& track_;
    
    std::vector<ChunkIndex> chunkIndex_;
    std::vector<TimeIndex> timeIndex_;
    std::set<uint32_t> syncSampleSet_;      // For O(1) sync sample lookup
    
    uint64_t totalSamples_ = 0;
    uint64_t totalDuration_ = 0;
    
    mutable std::mutex mutex_;
};
```

### ISOSeekingEngine

Seeking with edit list and composition time support:

```cpp
class ISOSeekingEngine {
public:
    ISOSeekingEngine(std::shared_ptr<ISOSampleTableManager> sampleTable,
                     const TrackInfo& track,
                     uint32_t movieTimescale);
    
    // Presentation time seeking (accounts for edit list)
    struct SeekResult {
        uint32_t sampleNumber;              // Target sample (1-based)
        uint64_t mediaTime;                 // Media time of sample
        uint64_t presentationTime;          // Presentation time after edit list
        bool isKeyframe;                    // True if sync sample
        int64_t prerollSamples;             // Samples to decode before target
    };
    
    SeekResult SeekToPresentationTime(double seconds) const;
    SeekResult SeekToMediaTime(uint64_t mediaTime) const;
    SeekResult SeekToSample(uint32_t sampleNumber) const;
    SeekResult SeekToKeyframe(double seconds, bool forward = true) const;
    
    // Edit list operations
    bool HasEditList() const;
    double GetPresentationDuration() const;
    
    // Time conversion
    uint64_t PresentationToMediaTime(uint64_t presentationTime) const;
    uint64_t MediaToPresentationTime(uint64_t mediaTime) const;
    double MediaTimeToSeconds(uint64_t mediaTime) const;
    uint64_t SecondsToMediaTime(double seconds) const;
    
    // Composition time operations
    uint64_t GetCompositionTime(uint32_t sampleNumber) const;
    uint64_t GetDecodeTime(uint32_t sampleNumber) const;
    int64_t GetCompositionOffset(uint32_t sampleNumber) const;
    
    // Edit segment information
    struct EditSegment {
        double startTime;                   // Presentation start time (seconds)
        double duration;                    // Segment duration (seconds)
        double mediaStartTime;              // Media start time (seconds), -1 for empty
        double rate;                        // Playback rate
        bool isEmpty;                       // True if empty edit (dwell)
    };
    
    std::vector<EditSegment> GetEditSegments() const;
    
private:
    // Edit list processing
    struct ProcessedEdit {
        uint64_t presentationStart;         // In movie timescale
        uint64_t presentationDuration;      // In movie timescale
        int64_t mediaStart;                 // In media timescale, -1 for empty
        double rate;
    };
    
    void ProcessEditList();
    const ProcessedEdit* FindEditForPresentationTime(uint64_t presentationTime) const;
    
    std::shared_ptr<ISOSampleTableManager> sampleTable_;
    const TrackInfo& track_;
    uint32_t movieTimescale_;
    uint32_t mediaTimescale_;
    
    std::vector<ProcessedEdit> processedEdits_;
    uint64_t presentationDuration_ = 0;
    
    mutable std::mutex mutex_;
};
```

### ISOFragmentManager

Fragmented MP4 support:

```cpp
class ISOFragmentManager {
public:
    ISOFragmentManager(std::shared_ptr<ISOBoxReader> reader,
                       const std::vector<TrackExtendsDefaults>& trackExtends);
    
    // Fragment discovery
    bool ScanForFragments();
    bool HasFragments() const;
    size_t GetFragmentCount() const;
    
    // Fragment access
    const MovieFragment* GetFragment(size_t index) const;
    const MovieFragment* GetFragmentAtTime(uint32_t trackId, uint64_t mediaTime) const;
    const MovieFragment* GetFragmentAtOffset(uint64_t fileOffset) const;
    
    // Random access
    bool HasRandomAccessInfo() const;
    std::vector<uint64_t> GetRandomAccessPoints(uint32_t trackId) const;
    uint64_t GetNearestRandomAccessPoint(uint32_t trackId, uint64_t mediaTime) const;
    
    // Segment index support
    bool HasSegmentIndex() const;
    const SegmentIndex* GetSegmentIndex(size_t index) const;
    size_t GetSegmentCount() const;
    
    // Sample access within fragments
    struct FragmentSampleInfo {
        uint32_t fragmentIndex;
        uint32_t trackFragmentIndex;
        uint32_t runIndex;
        uint32_t sampleIndexInRun;
        uint64_t offset;
        uint32_t size;
        uint64_t decodeTime;
        int32_t compositionOffset;
        uint32_t flags;
        bool isSync;
    };
    
    std::optional<FragmentSampleInfo> GetFragmentSample(uint32_t trackId, 
                                                         uint64_t mediaTime) const;
    std::vector<FragmentSampleInfo> GetFragmentSampleRange(uint32_t trackId,
                                                            uint64_t startTime,
                                                            uint64_t endTime) const;
    
    // Default value resolution
    uint32_t GetDefaultSampleDuration(uint32_t trackId, 
                                       const TrackFragmentHeader& tfhd) const;
    uint32_t GetDefaultSampleSize(uint32_t trackId, 
                                   const TrackFragmentHeader& tfhd) const;
    uint32_t GetDefaultSampleFlags(uint32_t trackId, 
                                    const TrackFragmentHeader& tfhd) const;
    
    // Live stream support
    bool IsLiveStream() const;
    bool RefreshFragments();
    
private:
    void BuildFragmentIndex();
    void ProcessTrackFragment(const MovieFragment& fragment, 
                              const TrackFragment& traf,
                              uint64_t baseOffset);
    
    std::shared_ptr<ISOBoxReader> reader_;
    std::vector<TrackExtendsDefaults> trackExtends_;
    
    std::vector<MovieFragment> fragments_;
    std::vector<TrackFragmentRandomAccess> randomAccess_;
    std::vector<SegmentIndex> segmentIndices_;
    
    // Per-track fragment sample index
    struct TrackFragmentIndex {
        std::vector<std::pair<uint64_t, size_t>> timeToFragment;  // mediaTime -> fragmentIndex
    };
    std::map<uint32_t, TrackFragmentIndex> trackIndices_;
    
    bool isLive_ = false;
    uint64_t lastScannedOffset_ = 0;
    
    mutable std::mutex mutex_;
};
```

### ISOMetadataExtractor

Metadata extraction:

```cpp
class ISOMetadataExtractor {
public:
    explicit ISOMetadataExtractor(std::shared_ptr<ISOBoxReader> reader);
    
    // iTunes-style metadata (ilst)
    struct iTunesMetadata {
        std::optional<std::string> title;
        std::optional<std::string> artist;
        std::optional<std::string> albumArtist;
        std::optional<std::string> album;
        std::optional<std::string> composer;
        std::optional<std::string> genre;
        std::optional<uint32_t> year;
        std::optional<std::pair<uint16_t, uint16_t>> trackNumber;  // track, total
        std::optional<std::pair<uint16_t, uint16_t>> discNumber;   // disc, total
        std::optional<std::string> comment;
        std::optional<std::string> lyrics;
        std::optional<std::string> copyright;
        std::optional<std::string> encoder;
        std::optional<uint8_t> compilation;
        std::optional<uint8_t> rating;
        std::optional<uint32_t> bpm;
        std::optional<std::string> grouping;
        std::optional<std::string> sortTitle;
        std::optional<std::string> sortArtist;
        std::optional<std::string> sortAlbum;
        std::vector<std::vector<uint8_t>> artwork;  // Cover art images
        std::map<std::string, std::string> custom;  // Custom tags
    };
    
    bool ParseMetadata(const BoxHeader& metaBox);
    const iTunesMetadata& GetMetadata() const;
    
    // Item-based metadata (iloc, iinf)
    bool HasItemMetadata() const;
    std::vector<ItemInfoEntry> GetItemInfo() const;
    std::optional<std::vector<uint8_t>> GetItemData(uint32_t itemId) const;
    std::vector<ItemReference> GetItemReferences(uint32_t itemId) const;
    
    // XML metadata
    bool HasXMLMetadata() const;
    std::string GetXMLMetadata() const;
    
    // User data (udta)
    bool ParseUserData(const BoxHeader& udtaBox);
    std::optional<std::string> GetCopyright() const;
    
private:
    bool ParseiTunesListBox(const BoxHeader& ilstBox);
    bool ParseItemLocationBox(const FullBoxHeader& ilocBox);
    bool ParseItemInfoBox(const FullBoxHeader& iinfBox);
    bool ParseItemReferenceBox(const FullBoxHeader& irefBox);
    bool ParsePrimaryItemBox(const FullBoxHeader& pitmBox);
    
    std::string ParseDataBox(const BoxHeader& dataBox);
    std::vector<uint8_t> ParseBinaryDataBox(const BoxHeader& dataBox);
    
    std::shared_ptr<ISOBoxReader> reader_;
    
    iTunesMetadata metadata_;
    std::vector<ItemLocationEntry> itemLocations_;
    std::vector<ItemInfoEntry> itemInfo_;
    std::vector<ItemReference> itemReferences_;
    std::optional<uint32_t> primaryItemId_;
    std::optional<std::string> xmlMetadata_;
    
    mutable std::mutex mutex_;
};
```

### ISOCodecConfigExtractor

Codec configuration extraction:

```cpp
class ISOCodecConfigExtractor {
public:
    explicit ISOCodecConfigExtractor(std::shared_ptr<ISOBoxReader> reader);
    
    // AAC configuration
    std::optional<AACConfig> ParseAACConfig(const std::vector<uint8_t>& esds);
    std::optional<AACConfig> ParseAudioSpecificConfig(const std::vector<uint8_t>& asc);
    
    // ALAC configuration
    std::optional<ALACConfig> ParseALACConfig(const std::vector<uint8_t>& alac);
    
    // AVC/H.264 configuration
    std::optional<AVCDecoderConfig> ParseAVCConfig(const std::vector<uint8_t>& avcC);
    
    // FLAC configuration
    std::optional<FLACConfig> ParseFLACConfig(const std::vector<uint8_t>& dfLa);
    
    // Channel layout
    std::optional<ChannelLayout> ParseChannelLayout(const std::vector<uint8_t>& chnl);
    
    // Elementary stream descriptor (esds)
    struct ESDescriptor {
        uint16_t esId;
        uint8_t streamPriority;
        std::optional<uint16_t> dependsOnEsId;
        std::optional<std::string> url;
        std::optional<uint16_t> ocrEsId;
        
        // Decoder config
        uint8_t objectTypeIndication;
        uint8_t streamType;
        uint32_t bufferSizeDB;
        uint32_t maxBitrate;
        uint32_t avgBitrate;
        std::vector<uint8_t> decoderSpecificInfo;
    };
    
    std::optional<ESDescriptor> ParseESDescriptor(const std::vector<uint8_t>& esds);
    
    // Sample entry parsing
    bool ParseAudioSampleEntry(const BoxHeader& header, SampleDescription& desc);
    bool ParseVideoSampleEntry(const BoxHeader& header, SampleDescription& desc);
    
private:
    // Descriptor tag parsing
    uint8_t ReadDescriptorTag(const uint8_t* data, size_t& offset);
    uint32_t ReadDescriptorLength(const uint8_t* data, size_t& offset);
    
    std::shared_ptr<ISOBoxReader> reader_;
    
    mutable std::mutex mutex_;
};
```

### ISOComplianceValidator

ISO/IEC 14496-12 compliance validation:

```cpp
class ISOComplianceValidator {
public:
    enum class Severity { Info, Warning, Error, Fatal };
    
    struct ValidationIssue {
        Severity severity;
        std::string code;
        std::string message;
        std::optional<uint64_t> offset;
        std::optional<uint32_t> boxType;
    };
    
    // Validation modes
    enum class Mode {
        Strict,         // All violations are errors
        Permissive,     // Only fatal issues are errors
        ReportOnly      // Collect issues but don't fail
    };
    
    explicit ISOComplianceValidator(Mode mode = Mode::Permissive);
    
    // File-level validation
    bool ValidateFileStructure(const FileTypeInfo& ftyp,
                               bool hasMovieBox,
                               bool hasMediaData);
    
    // Movie box validation
    bool ValidateMovieBox(const MovieHeaderInfo& mvhd,
                          const std::vector<TrackInfo>& tracks);
    
    // Track validation
    bool ValidateTrack(const TrackInfo& track, uint32_t movieTimescale);
    
    // Sample table validation
    bool ValidateSampleTable(const TrackInfo& track);
    
    // Fragment validation
    bool ValidateFragment(const MovieFragment& fragment,
                          const std::vector<TrackExtendsDefaults>& defaults);
    
    // Box hierarchy validation
    bool ValidateBoxHierarchy(uint32_t parentType, uint32_t childType);
    bool ValidateBoxCount(uint32_t parentType, uint32_t childType, uint32_t count);
    
    // Results
    bool HasErrors() const;
    bool HasFatalErrors() const;
    const std::vector<ValidationIssue>& GetIssues() const;
    std::string GetReport() const;
    void ClearIssues();
    
private:
    void AddIssue(Severity severity, const std::string& code, 
                  const std::string& message,
                  std::optional<uint64_t> offset = std::nullopt,
                  std::optional<uint32_t> boxType = std::nullopt);
    
    Mode mode_;
    std::vector<ValidationIssue> issues_;
    
    // Box hierarchy rules
    static const std::map<uint32_t, std::set<uint32_t>> allowedChildren_;
    static const std::map<uint32_t, std::pair<uint32_t, uint32_t>> boxCountRules_;
    
    mutable std::mutex mutex_;
};
```

### ISOSecurityValidator

Security validation and resource limiting:

```cpp
class ISOSecurityValidator {
public:
    struct Limits {
        uint64_t maxFileSize = 16ULL * 1024 * 1024 * 1024;  // 16 GB
        uint64_t maxBoxSize = 4ULL * 1024 * 1024 * 1024;    // 4 GB
        uint32_t maxRecursionDepth = 32;
        uint32_t maxTrackCount = 256;
        uint64_t maxSampleCount = 100000000;                 // 100 million
        uint64_t maxSampleSize = 256 * 1024 * 1024;          // 256 MB
        uint64_t maxAllocationSize = 1024 * 1024 * 1024;     // 1 GB total
        uint32_t maxStringLength = 65536;
        uint32_t maxArrayElements = 10000000;
    };
    
    explicit ISOSecurityValidator(const Limits& limits = Limits{});
    
    // Size validation
    bool ValidateBoxSize(uint64_t size, uint64_t parentRemaining);
    bool ValidateSampleSize(uint32_t size);
    bool ValidateAllocation(uint64_t size);
    
    // Recursion tracking
    bool EnterBox();
    void LeaveBox();
    uint32_t GetCurrentDepth() const;
    
    // Count validation
    bool ValidateTrackCount(uint32_t count);
    bool ValidateSampleCount(uint64_t count);
    bool ValidateArraySize(uint64_t count);
    bool ValidateStringLength(uint64_t length);
    
    // Offset validation
    bool ValidateOffset(uint64_t offset, uint64_t fileSize);
    bool ValidateRange(uint64_t offset, uint64_t size, uint64_t fileSize);
    
    // Allocation tracking
    void TrackAllocation(uint64_t size);
    void ReleaseAllocation(uint64_t size);
    uint64_t GetTotalAllocated() const;
    bool CanAllocate(uint64_t size) const;
    
    // Reset state
    void Reset();
    
    // Get current limits
    const Limits& GetLimits() const;
    
private:
    Limits limits_;
    uint32_t currentDepth_ = 0;
    uint64_t totalAllocated_ = 0;
    
    mutable std::mutex mutex_;
};
```

### ISODemuxer

Main demuxer class implementing the Demuxer interface:

```cpp
class ISODemuxer : public Demuxer {
public:
    ISODemuxer();
    ~ISODemuxer() override;
    
    // Demuxer interface implementation
    bool Open(std::shared_ptr<IOHandler> io) override;
    void Close() override;
    bool IsOpen() const override;
    
    // Stream information
    size_t GetStreamCount() const override;
    const StreamInfo& GetStreamInfo(size_t index) const override;
    size_t GetPrimaryAudioStream() const override;
    size_t GetPrimaryVideoStream() const override;
    
    // Sample reading
    bool ReadSample(size_t streamIndex, MediaChunk& chunk) override;
    bool ReadSampleAt(size_t streamIndex, uint64_t sampleNumber, MediaChunk& chunk) override;
    
    // Seeking
    bool Seek(double seconds) override;
    bool SeekToSample(size_t streamIndex, uint64_t sampleNumber) override;
    double GetPosition() const override;
    double GetDuration() const override;
    
    // Metadata
    const Metadata& GetMetadata() const override;
    
    // Format identification
    static bool CanHandle(std::shared_ptr<IOHandler> io);
    static std::string GetFormatName();
    static std::vector<std::string> GetSupportedExtensions();
    static std::vector<std::string> GetSupportedMimeTypes();
    
    // ISO-specific accessors
    const FileTypeInfo& GetFileType() const;
    bool IsFragmented() const;
    const std::vector<TrackInfo>& GetTracks() const;
    
private:
    // Threading: Public/Private Lock Pattern
    // All public methods acquire mutex and call _unlocked versions
    
    bool Open_unlocked(std::shared_ptr<IOHandler> io);
    void Close_unlocked();
    bool IsOpen_unlocked() const;
    
    size_t GetStreamCount_unlocked() const;
    const StreamInfo& GetStreamInfo_unlocked(size_t index) const;
    
    bool ReadSample_unlocked(size_t streamIndex, MediaChunk& chunk);
    bool Seek_unlocked(double seconds);
    double GetPosition_unlocked() const;
    
    // Internal helpers
    bool ParseFile();
    bool BuildStreamInfo();
    bool SelectPrimaryStreams();
    StreamInfo CreateStreamInfo(const TrackInfo& track);
    
    // Sample reading helpers
    bool ReadSampleFromTrack(const TrackInfo& track, uint32_t sampleNumber, 
                              MediaChunk& chunk);
    bool ReadSampleFromFragment(uint32_t trackId, uint64_t mediaTime,
                                 MediaChunk& chunk);
    
    // Components
    std::shared_ptr<IOHandler> io_;
    std::shared_ptr<ISOBoxReader> boxReader_;
    std::shared_ptr<ISOContainerParser> containerParser_;
    std::shared_ptr<ISOSecurityValidator> securityValidator_;
    std::shared_ptr<ISOComplianceValidator> complianceValidator_;
    std::shared_ptr<ISOMetadataExtractor> metadataExtractor_;
    std::shared_ptr<ISOCodecConfigExtractor> codecConfigExtractor_;
    std::shared_ptr<ISOFragmentManager> fragmentManager_;
    
    // Per-track managers
    std::map<uint32_t, std::shared_ptr<ISOSampleTableManager>> sampleTableManagers_;
    std::map<uint32_t, std::shared_ptr<ISOSeekingEngine>> seekingEngines_;
    
    // Stream mapping
    std::vector<StreamInfo> streamInfo_;
    std::map<size_t, uint32_t> streamToTrackId_;
    size_t primaryAudioStream_ = SIZE_MAX;
    size_t primaryVideoStream_ = SIZE_MAX;
    
    // Playback state
    struct TrackState {
        uint32_t currentSample = 1;
        uint64_t currentTime = 0;
        bool endOfTrack = false;
    };
    std::map<uint32_t, TrackState> trackStates_;
    
    // Metadata
    Metadata metadata_;
    
    bool isOpen_ = false;
    
    // Lock acquisition order:
    // 1. mutex_ (main demuxer lock)
    // 2. Component-specific locks (acquired by components internally)
    mutable std::mutex mutex_;
};
```

## Error Handling

### Error Categories

```cpp
enum class ISOError {
    // File structure errors
    InvalidFileType,
    MissingMandatoryBox,
    InvalidBoxSize,
    InvalidBoxHierarchy,
    CorruptedBox,
    
    // Sample table errors
    InvalidSampleTable,
    SampleNotFound,
    ChunkNotFound,
    InvalidTimeMapping,
    
    // Codec errors
    UnsupportedCodec,
    InvalidCodecConfig,
    MissingCodecConfig,
    
    // Fragment errors
    InvalidFragment,
    FragmentNotFound,
    IncompleteFragment,
    
    // Seeking errors
    SeekFailed,
    InvalidSeekPosition,
    NoSyncSample,
    
    // Security errors
    FileTooLarge,
    BoxTooLarge,
    RecursionLimitExceeded,
    AllocationLimitExceeded,
    InvalidOffset,
    
    // I/O errors
    ReadError,
    EndOfFile,
    IONotAvailable
};

class ISOException : public std::exception {
public:
    ISOException(ISOError error, const std::string& message,
                 std::optional<uint64_t> offset = std::nullopt);
    
    ISOError GetError() const;
    const std::string& GetMessage() const;
    std::optional<uint64_t> GetOffset() const;
    const char* what() const noexcept override;
    
private:
    ISOError error_;
    std::string message_;
    std::optional<uint64_t> offset_;
    std::string whatMessage_;
};
```

### Error Recovery Strategy

1. **Box-level recovery**: Skip corrupted boxes and continue parsing
2. **Track-level recovery**: Mark damaged tracks as unavailable
3. **Sample-level recovery**: Skip unreadable samples, report gaps
4. **Fragment-level recovery**: Skip incomplete fragments in live streams

## Threading Safety

### Lock Acquisition Order

To prevent deadlocks, locks must be acquired in this order:

1. `ISODemuxer::mutex_` (main demuxer lock)
2. `ISOContainerParser::mutex_`
3. `ISOSampleTableManager::mutex_`
4. `ISOSeekingEngine::mutex_`
5. `ISOFragmentManager::mutex_`
6. `ISOMetadataExtractor::mutex_`
7. `ISOBoxReader::mutex_` (I/O lock)

### Public/Private Lock Pattern

All classes follow the public/private lock pattern:

```cpp
// Public method acquires lock
bool ISODemuxer::Seek(double seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    return Seek_unlocked(seconds);
}

// Private method does work without acquiring lock
bool ISODemuxer::Seek_unlocked(double seconds) {
    // Can safely call other _unlocked methods
    auto& engine = seekingEngines_[primaryAudioStream_];
    auto result = engine->SeekToPresentationTime(seconds);
    // ...
}
```

### Thread-Safe Operations

- Multiple threads can read samples from different tracks concurrently
- Seeking is serialized per-demuxer
- Metadata access is read-only after Open()
- Fragment refresh (live streams) is serialized

## Testing Strategy

### Unit Tests

1. **Box parsing tests**: Verify correct parsing of all box types
2. **Sample table tests**: Verify sample lookup accuracy
3. **Seeking tests**: Verify edit list and composition time handling
4. **Fragment tests**: Verify fragmented MP4 support
5. **Security tests**: Verify limits and validation

### Integration Tests

1. **Format compatibility**: Test with MP4, M4A, MOV, 3GP files
2. **Codec compatibility**: Test AAC, ALAC, mulaw, alaw, PCM, AVC
3. **Streaming tests**: Test progressive download and live streams
4. **Error recovery**: Test with corrupted and malformed files

### Compliance Tests

1. **ISO/IEC 14496-12 compliance**: Validate against specification
2. **Brand compatibility**: Test all supported brands
3. **MIME type accuracy**: Verify correct type identification

## Performance Considerations

### Memory Optimization

1. **Lazy loading**: Load sample tables on demand
2. **Index compression**: Use run-length encoding for repetitive data
3. **Cache management**: LRU cache for frequently accessed data
4. **Streaming parsing**: Process boxes without full file buffering

### I/O Optimization

1. **Batch reading**: Read multiple samples in single I/O operation
2. **Prefetching**: Anticipate sequential access patterns
3. **Seek minimization**: Optimize chunk ordering for sequential access
4. **Buffer pooling**: Reuse buffers for sample data

### Computational Optimization

1. **Binary search**: O(log n) sample lookup
2. **Precomputed indices**: Build optimized lookup structures
3. **Incremental parsing**: Parse only required portions
4. **Parallel processing**: Concurrent track processing where safe
