/*
 * ISODemuxer.h - ISO Base Media File Format demuxer (MP4, M4A, etc.)
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef ISODEMUXER_H
#define ISODEMUXER_H

#include "Demuxer.h"
#include <map>
#include <vector>
#include <memory>
#include <stack>
#include <variant>
#include <functional>

/**
 * @brief ISO box header structure
 */
struct BoxHeader {
    uint32_t type;
    uint64_t size;
    uint64_t dataOffset;
    
    bool isExtendedSize() const { return size == 1; }
};

/**
 * @brief Sample table information structure
 */
struct SampleTableInfo {
    std::vector<uint64_t> chunkOffsets;     // stco/co64
    std::vector<uint32_t> samplesPerChunk;  // stsc
    std::vector<uint32_t> sampleSizes;      // stsz
    std::vector<uint64_t> sampleTimes;      // stts (decoded to absolute times)
    std::vector<uint64_t> syncSamples;      // stss (keyframes)
};

/**
 * @brief Audio track information
 */
struct AudioTrackInfo {
    uint32_t trackId;
    std::string codecType;        // "aac", "alac", "ulaw", "alaw", "lpcm"
    uint32_t sampleRate;
    uint16_t channelCount;
    uint16_t bitsPerSample;
    uint32_t avgBitrate;
    
    // Codec-specific configuration
    std::vector<uint8_t> codecConfig;  // AAC: AudioSpecificConfig, ALAC: magic cookie
    
    // Track timing
    uint64_t duration;           // in track timescale units
    uint32_t timescale;          // samples per second for timing
    
    // Current playback state
    uint64_t currentSampleIndex = 0;
    
    // Sample table information
    SampleTableInfo sampleTableInfo;
};

// Helper macro for FOURCC constants
#define FOURCC(a, b, c, d) ((uint32_t)(((uint8_t)(a) << 24) | ((uint8_t)(b) << 16) | ((uint8_t)(c) << 8) | (uint8_t)(d)))

// ISO box type constants - Core structure
constexpr uint32_t BOX_FTYP = FOURCC('f','t','y','p'); // File type box
constexpr uint32_t BOX_MOOV = FOURCC('m','o','o','v'); // Movie box
constexpr uint32_t BOX_MDAT = FOURCC('m','d','a','t'); // Media data box
constexpr uint32_t BOX_FREE = FOURCC('f','r','e','e'); // Free space box
constexpr uint32_t BOX_SKIP = FOURCC('s','k','i','p'); // Skip box
constexpr uint32_t BOX_WIDE = FOURCC('w','i','d','e'); // Wide box
constexpr uint32_t BOX_PNOT = FOURCC('p','n','o','t'); // Preview box

// Movie box children
constexpr uint32_t BOX_MVHD = FOURCC('m','v','h','d'); // Movie header
constexpr uint32_t BOX_TRAK = FOURCC('t','r','a','k'); // Track box
constexpr uint32_t BOX_UDTA = FOURCC('u','d','t','a'); // User data box
constexpr uint32_t BOX_META = FOURCC('m','e','t','a'); // Metadata box
constexpr uint32_t BOX_IODS = FOURCC('i','o','d','s'); // Initial object descriptor

// Track box children
constexpr uint32_t BOX_TKHD = FOURCC('t','k','h','d'); // Track header
constexpr uint32_t BOX_TREF = FOURCC('t','r','e','f'); // Track reference
constexpr uint32_t BOX_EDTS = FOURCC('e','d','t','s'); // Edit box
constexpr uint32_t BOX_MDIA = FOURCC('m','d','i','a'); // Media box

// Edit box children
constexpr uint32_t BOX_ELST = FOURCC('e','l','s','t'); // Edit list

// Media box children
constexpr uint32_t BOX_MDHD = FOURCC('m','d','h','d'); // Media header
constexpr uint32_t BOX_HDLR = FOURCC('h','d','l','r'); // Handler reference
constexpr uint32_t BOX_MINF = FOURCC('m','i','n','f'); // Media information

// Media information box children
constexpr uint32_t BOX_VMHD = FOURCC('v','m','h','d'); // Video media header
constexpr uint32_t BOX_SMHD = FOURCC('s','m','h','d'); // Sound media header
constexpr uint32_t BOX_HMHD = FOURCC('h','m','h','d'); // Hint media header
constexpr uint32_t BOX_NMHD = FOURCC('n','m','h','d'); // Null media header
constexpr uint32_t BOX_DINF = FOURCC('d','i','n','f'); // Data information
constexpr uint32_t BOX_STBL = FOURCC('s','t','b','l'); // Sample table

// Data information box children
constexpr uint32_t BOX_DREF = FOURCC('d','r','e','f'); // Data reference
constexpr uint32_t BOX_URL  = FOURCC('u','r','l',' '); // Data entry URL
constexpr uint32_t BOX_URN  = FOURCC('u','r','n',' '); // Data entry URN

// Sample table box children
constexpr uint32_t BOX_STSD = FOURCC('s','t','s','d'); // Sample description
constexpr uint32_t BOX_STTS = FOURCC('s','t','t','s'); // Time-to-sample
constexpr uint32_t BOX_CTTS = FOURCC('c','t','t','s'); // Composition time-to-sample
constexpr uint32_t BOX_STSC = FOURCC('s','t','s','c'); // Sample-to-chunk
constexpr uint32_t BOX_STSZ = FOURCC('s','t','s','z'); // Sample size
constexpr uint32_t BOX_STZ2 = FOURCC('s','t','z','2'); // Compact sample size
constexpr uint32_t BOX_STCO = FOURCC('s','t','c','o'); // Chunk offset (32-bit)
constexpr uint32_t BOX_CO64 = FOURCC('c','o','6','4'); // Chunk offset (64-bit)
constexpr uint32_t BOX_STSS = FOURCC('s','t','s','s'); // Sync sample (keyframes)
constexpr uint32_t BOX_STSH = FOURCC('s','t','s','h'); // Shadow sync sample
constexpr uint32_t BOX_PADB = FOURCC('p','a','d','b'); // Padding bits
constexpr uint32_t BOX_STDP = FOURCC('s','t','d','p'); // Sample degradation priority

// Fragmented MP4 boxes
constexpr uint32_t BOX_MOOF = FOURCC('m','o','o','f'); // Movie fragment
constexpr uint32_t BOX_MFHD = FOURCC('m','f','h','d'); // Movie fragment header
constexpr uint32_t BOX_TRAF = FOURCC('t','r','a','f'); // Track fragment
constexpr uint32_t BOX_TFHD = FOURCC('t','f','h','d'); // Track fragment header
constexpr uint32_t BOX_TRUN = FOURCC('t','r','u','n'); // Track fragment run
constexpr uint32_t BOX_TFDT = FOURCC('t','f','d','t'); // Track fragment decode time
constexpr uint32_t BOX_MFRA = FOURCC('m','f','r','a'); // Movie fragment random access
constexpr uint32_t BOX_TFRA = FOURCC('t','f','r','a'); // Track fragment random access
constexpr uint32_t BOX_MFRO = FOURCC('m','f','r','o'); // Movie fragment random access offset
constexpr uint32_t BOX_SIDX = FOURCC('s','i','d','x'); // Segment index

// Metadata boxes
constexpr uint32_t BOX_ILST = FOURCC('i','l','s','t'); // Item list
constexpr uint32_t BOX_KEYS = FOURCC('k','e','y','s'); // Keys
constexpr uint32_t BOX_DATA = FOURCC('d','a','t','a'); // Data
constexpr uint32_t BOX_MEAN = FOURCC('m','e','a','n'); // Mean
constexpr uint32_t BOX_NAME = FOURCC('n','a','m','e'); // Name

// iTunes metadata atoms
constexpr uint32_t BOX_TITLE = FOURCC(0xA9,'n','a','m'); // Title
constexpr uint32_t BOX_ARTIST = FOURCC(0xA9,'A','R','T'); // Artist
constexpr uint32_t BOX_ALBUM = FOURCC(0xA9,'a','l','b'); // Album
constexpr uint32_t BOX_DATE = FOURCC(0xA9,'d','a','y'); // Date
constexpr uint32_t BOX_GENRE = FOURCC(0xA9,'g','e','n'); // Genre
constexpr uint32_t BOX_TRACK = FOURCC('t','r','k','n'); // Track number
constexpr uint32_t BOX_DISK = FOURCC('d','i','s','k'); // Disk number
constexpr uint32_t BOX_COVR = FOURCC('c','o','v','r'); // Cover art

// Audio codec types
constexpr uint32_t CODEC_AAC  = FOURCC('m','p','4','a'); // AAC audio
constexpr uint32_t CODEC_ALAC = FOURCC('a','l','a','c'); // Apple Lossless
constexpr uint32_t CODEC_ULAW = FOURCC('u','l','a','w'); // μ-law
constexpr uint32_t CODEC_ALAW = FOURCC('a','l','a','w'); // A-law
constexpr uint32_t CODEC_LPCM = FOURCC('l','p','c','m'); // Linear PCM
constexpr uint32_t CODEC_SOWT = FOURCC('s','o','w','t'); // Little-endian PCM
constexpr uint32_t CODEC_TWOS = FOURCC('t','w','o','s'); // Big-endian PCM
constexpr uint32_t CODEC_FL32 = FOURCC('f','l','3','2'); // 32-bit float
constexpr uint32_t CODEC_FL64 = FOURCC('f','l','6','4'); // 64-bit float
constexpr uint32_t CODEC_IN24 = FOURCC('i','n','2','4'); // 24-bit integer
constexpr uint32_t CODEC_IN32 = FOURCC('i','n','3','2'); // 32-bit integer

// Handler types
constexpr uint32_t HANDLER_SOUN = FOURCC('s','o','u','n'); // Sound handler
constexpr uint32_t HANDLER_VIDE = FOURCC('v','i','d','e'); // Video handler
constexpr uint32_t HANDLER_HINT = FOURCC('h','i','n','t'); // Hint handler
constexpr uint32_t HANDLER_META = FOURCC('m','e','t','a'); // Metadata handler

// File type brands
constexpr uint32_t BRAND_ISOM = FOURCC('i','s','o','m'); // ISO Base Media
constexpr uint32_t BRAND_MP41 = FOURCC('m','p','4','1'); // MP4 version 1
constexpr uint32_t BRAND_MP42 = FOURCC('m','p','4','2'); // MP4 version 2
constexpr uint32_t BRAND_M4A  = FOURCC('M','4','A',' '); // iTunes M4A
constexpr uint32_t BRAND_M4V  = FOURCC('M','4','V',' '); // iTunes M4V
constexpr uint32_t BRAND_QT   = FOURCC('q','t',' ',' '); // QuickTime
constexpr uint32_t BRAND_3GP4 = FOURCC('3','g','p','4'); // 3GPP Release 4
constexpr uint32_t BRAND_3GP5 = FOURCC('3','g','p','5'); // 3GPP Release 5
constexpr uint32_t BRAND_3GP6 = FOURCC('3','g','p','6'); // 3GPP Release 6
constexpr uint32_t BRAND_3G2A = FOURCC('3','g','2','a'); // 3GPP2

// Forward declarations
class BoxParser;
class SampleTableManager;
class FragmentHandler;
class MetadataExtractor;
class StreamManager;
class SeekingEngine;

/**
 * @brief Box parser component for recursive ISO box structure parsing
 */
class BoxParser {
public:
    explicit BoxParser(std::shared_ptr<IOHandler> io);
    ~BoxParser() = default;
    
    bool ParseMovieBox(uint64_t offset, uint64_t size);
    bool ParseTrackBox(uint64_t offset, uint64_t size, AudioTrackInfo& track);
    bool ParseSampleTableBox(uint64_t offset, uint64_t size, SampleTableInfo& tables);
    bool ParseFragmentBox(uint64_t offset, uint64_t size);
    
    // Core box parsing functionality
    BoxHeader ReadBoxHeader(uint64_t offset);
    bool ValidateBoxSize(const BoxHeader& header, uint64_t containerSize);
    bool ParseBoxRecursively(uint64_t offset, uint64_t size, 
                            std::function<bool(const BoxHeader&, uint64_t)> handler);
    
    // Additional parsing methods for file type and movie box parsing
    bool ParseFileTypeBox(uint64_t offset, uint64_t size, std::string& containerType);
    bool ParseMediaBox(uint64_t offset, uint64_t size, AudioTrackInfo& track, bool& foundAudio);
    bool ParseMediaBoxWithSampleTables(uint64_t offset, uint64_t size, AudioTrackInfo& track, bool& foundAudio, SampleTableInfo& sampleTables);
    bool ParseHandlerBox(uint64_t offset, uint64_t size, std::string& handlerType);
    bool ParseSampleDescriptionBox(uint64_t offset, uint64_t size, AudioTrackInfo& track);
    
    // Codec-specific configuration parsing
    bool ParseAACConfiguration(uint64_t offset, uint64_t size, AudioTrackInfo& track);
    bool ParseALACConfiguration(uint64_t offset, uint64_t size, AudioTrackInfo& track);
    
    // Sample table parsing methods
    bool ParseTimeToSampleBox(uint64_t offset, uint64_t size, SampleTableInfo& tables);
    bool ParseSampleToChunkBox(uint64_t offset, uint64_t size, SampleTableInfo& tables);
    bool ParseSampleSizeBox(uint64_t offset, uint64_t size, SampleTableInfo& tables);
    bool ParseChunkOffsetBox(uint64_t offset, uint64_t size, SampleTableInfo& tables, bool is64Bit);
    bool ParseSyncSampleBox(uint64_t offset, uint64_t size, SampleTableInfo& tables);
    
    uint32_t ReadUInt32BE(uint64_t offset);
    uint64_t ReadUInt64BE(uint64_t offset);
    std::string BoxTypeToString(uint32_t boxType);
    bool SkipUnknownBox(const BoxHeader& header);
    
private:
    std::shared_ptr<IOHandler> io;
    std::stack<BoxHeader> boxStack;
    uint64_t fileSize;
    
    bool IsContainerBox(uint32_t boxType);
};

/**
 * @brief Sample table manager for efficient sample lookups
 */
class SampleTableManager {
public:
    struct SampleInfo {
        uint64_t offset;
        uint32_t size;
        uint32_t duration;
        bool isKeyframe;
    };
    
    SampleTableManager() = default;
    ~SampleTableManager() = default;
    
    bool BuildSampleTables(const SampleTableInfo& rawTables);
    SampleInfo GetSampleInfo(uint64_t sampleIndex);
    uint64_t TimeToSample(double timestamp);
    double SampleToTime(uint64_t sampleIndex);
    
private:
    // Compressed sample-to-chunk mapping
    struct ChunkInfo {
        uint64_t offset;
        uint32_t sampleCount;
        uint32_t firstSample;
    };
    std::vector<ChunkInfo> chunkTable;
    
    // Time-to-sample lookup with binary search
    struct TimeToSampleEntry {
        uint64_t sampleIndex;
        uint64_t timestamp;
        uint32_t duration;
    };
    std::vector<TimeToSampleEntry> timeTable;
    
    // Sample size table (compressed for fixed sizes)
    std::variant<uint32_t, std::vector<uint32_t>> sampleSizes;
    
    // Sync sample table for keyframe seeking
    std::vector<uint64_t> syncSamples;
    
    // Private helper methods for building and managing sample tables
    bool BuildChunkTable(const SampleTableInfo& rawTables);
    bool BuildTimeTable(const SampleTableInfo& rawTables);
    bool BuildSampleSizeTable(const SampleTableInfo& rawTables);
    bool ValidateTableConsistency();
    
    uint32_t GetSamplesPerChunkForIndex(size_t chunkIndex, const std::vector<uint32_t>& samplesPerChunk);
    ChunkInfo* FindChunkForSample(uint64_t sampleIndex);
    uint32_t GetSampleSize(uint64_t sampleIndex);
    uint32_t GetSampleDuration(uint64_t sampleIndex);
    bool IsSyncSample(uint64_t sampleIndex);
};

/**
 * @brief Fragment handler for fragmented MP4 support
 */
class FragmentHandler {
public:
    FragmentHandler() = default;
    ~FragmentHandler() = default;
    
    bool ProcessMovieFragment(uint64_t moofOffset);
    bool UpdateSampleTables(const SampleTableInfo& traf);
    bool IsFragmented() const { return hasFragments; }
    
private:
    bool hasFragments = false;
    
    struct FragmentInfo {
        uint64_t moofOffset;
        uint64_t mdatOffset;
        uint32_t sampleCount;
        std::vector<uint32_t> sampleSizes;
        std::vector<uint32_t> sampleDurations;
    };
    
    std::vector<FragmentInfo> fragments;
    uint64_t currentFragmentIndex = 0;
};

/**
 * @brief Metadata extractor for iTunes/ISO metadata parsing
 */
class MetadataExtractor {
public:
    MetadataExtractor() = default;
    ~MetadataExtractor() = default;
    
    std::map<std::string, std::string> ExtractMetadata(uint64_t udtaOffset, uint64_t size);
    
private:
    std::string ExtractTextMetadata(const std::vector<uint8_t>& data);
};

/**
 * @brief Stream manager for audio track management
 */
class StreamManager {
public:
    StreamManager() = default;
    ~StreamManager() = default;
    
    void AddAudioTrack(const AudioTrackInfo& track);
    std::vector<StreamInfo> GetStreamInfos() const;
    AudioTrackInfo* GetTrack(uint32_t trackId);
    
private:
    std::vector<AudioTrackInfo> tracks;
};

/**
 * @brief Seeking engine for sample-accurate positioning
 */
class SeekingEngine {
public:
    SeekingEngine() = default;
    ~SeekingEngine() = default;
    
    bool SeekToTimestamp(double timestamp, AudioTrackInfo& track, SampleTableManager& sampleTables);
    
private:
    uint64_t BinarySearchTimeToSample(double timestamp, const std::vector<SampleTableManager::SampleInfo>& samples);
};

/**
 * @brief ISO Base Media File Format demuxer
 * 
 * This demuxer handles the ISO container format family:
 * - MP4 files (.mp4, .m4v)
 * - M4A files (.m4a)
 * - 3GP files (.3gp)
 * - MOV files (.mov) - QuickTime variant
 * 
 * The format can contain various audio codecs:
 * - AAC, ALAC, mulaw, alaw, PCM variants
 */
class ISODemuxer : public Demuxer {
public:
    explicit ISODemuxer(std::unique_ptr<IOHandler> handler);
    virtual ~ISODemuxer();
    
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
    // Core components as per design
    std::unique_ptr<BoxParser> boxParser;
    std::unique_ptr<SampleTableManager> sampleTables;
    std::unique_ptr<FragmentHandler> fragmentHandler;
    std::unique_ptr<MetadataExtractor> metadataExtractor;
    std::unique_ptr<StreamManager> streamManager;
    std::unique_ptr<SeekingEngine> seekingEngine;
    
    // Audio track management
    std::vector<AudioTrackInfo> audioTracks;
    int selectedTrackIndex = -1;
    uint64_t currentSampleIndex = 0;
    
    // State management
    bool m_eof = false;
    
    /**
     * @brief Initialize core components
     */
    void initializeComponents();
    
    /**
     * @brief Clean up resources
     */
    void cleanup();
    
    /**
     * @brief Parse movie box and extract audio tracks
     */
    bool ParseMovieBoxWithTracks(uint64_t offset, uint64_t size);
};

#endif // ISODEMUXER_H