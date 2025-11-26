/*
 * ISODemuxer.h - ISO Base Media File Format demuxer (MP4, M4A, etc.)
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef ISODEMUXER_H
#define ISODEMUXER_H

namespace PsyMP3 {
namespace Demuxer {
namespace ISO {

// All necessary headers are included via psymp3.h

/**
 * @brief Sample-to-chunk table entry
 */
struct SampleToChunkEntry {
    uint32_t firstChunk;        // First chunk (0-based)
    uint32_t samplesPerChunk;   // Samples per chunk
    uint32_t sampleDescIndex;   // Sample description index
};

/**
 * @brief Sample table information structure
 */
struct SampleTableInfo {
    std::vector<uint64_t> chunkOffsets;           // stco/co64
    std::vector<SampleToChunkEntry> sampleToChunkEntries; // stsc (raw entries)
    std::vector<uint32_t> samplesPerChunk;        // stsc (deprecated - for compatibility)
    std::vector<uint32_t> sampleSizes;            // stsz
    std::vector<uint64_t> sampleTimes;            // stts (decoded to absolute times)
    std::vector<uint64_t> syncSamples;            // stss (keyframes)
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
constexpr uint32_t CODEC_FLAC = FOURCC('f','L','a','C'); // FLAC lossless
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
    
    // Metadata extraction
    std::map<std::string, std::string> getMetadata() const;
    
    // Compliance validation
    ComplianceValidationResult getComplianceReport() const;
    
private:
    // Core components as per design
    std::unique_ptr<BoxParser> boxParser;
    std::unique_ptr<SampleTableManager> sampleTables;
    std::unique_ptr<FragmentHandler> fragmentHandler;
    std::unique_ptr<MetadataExtractor> metadataExtractor;
    std::unique_ptr<StreamManager> streamManager;
    std::unique_ptr<SeekingEngine> seekingEngine;
    std::unique_ptr<StreamManager> streamingManager;
    std::unique_ptr<ErrorRecovery> errorRecovery;
    std::unique_ptr<ComplianceValidator> complianceValidator;
    
    // Audio track management
    std::vector<AudioTrackInfo> audioTracks;
    int selectedTrackIndex = -1;
    uint64_t currentSampleIndex = 0;
    
    // State management is handled by base class
    
    // Metadata storage
    std::map<std::string, std::string> m_metadata;
    
    // Memory management
    int memoryPressureCallbackId = -1;
    
    /**
     * @brief Initialize core components
     */
    void initializeComponents();
    
    /**
     * @brief Clean up resources
     */
    void cleanup();
    
    /**
     * @brief Initialize memory management integration (Requirement 8.8)
     */
    void InitializeMemoryManagement();
    
    /**
     * @brief Handle memory pressure changes adaptively
     * @param pressureLevel Memory pressure level (0-100)
     */
    void HandleMemoryPressureChange(int pressureLevel);
    
    /**
     * @brief Optimize for critical memory pressure
     */
    void OptimizeForCriticalMemoryPressure();
    
    /**
     * @brief Log current memory usage breakdown
     */
    void LogMemoryUsage() const;
    
    /**
     * @brief Get total memory usage of the demuxer
     * @return Total memory usage in bytes
     */
    size_t GetMemoryUsage() const;
    
    /**
     * @brief Parse movie box and extract audio tracks
     */
    bool ParseMovieBoxWithTracks(uint64_t offset, uint64_t size);
    
    /**
     * @brief Extract sample data from mdat boxes using sample tables
     * @param stream_id Stream identifier
     * @param track Audio track information
     * @param sampleInfo Sample location and size information
     * @return MediaChunk with extracted sample data
     */
    MediaChunk ExtractSampleData(uint32_t stream_id, const AudioTrackInfo& track, 
                                const SampleTableManager::SampleInfo& sampleInfo);
    
    /**
     * @brief Apply codec-specific processing to extracted sample data
     * @param chunk MediaChunk to process
     * @param track Audio track information containing codec details
     */
    void ProcessCodecSpecificData(MediaChunk& chunk, const AudioTrackInfo& track);
    
    /**
     * @brief Calculate accurate timing information for telephony codecs
     * @param track Audio track information
     * @param sampleIndex Current sample index
     * @return Timestamp in milliseconds
     */
    uint64_t CalculateTelephonyTiming(const AudioTrackInfo& track, uint64_t sampleIndex);
    
    /**
     * @brief Validate telephony codec configuration meets standards compliance
     * @param track Audio track information to validate
     * @return True if configuration is valid for telephony use
     */
    bool ValidateTelephonyCodecConfiguration(const AudioTrackInfo& track);
    
    /**
     * @brief Validate FLAC codec configuration meets RFC 9639 compliance
     * @param track Audio track information to validate
     * @return True if configuration is valid for FLAC decoding
     */
    bool ValidateFLACCodecConfiguration(const AudioTrackInfo& track);
    
    /**
     * @brief Detect FLAC frame boundaries within MP4 sample data
     * @param sampleData Raw sample data from MP4 container
     * @param frameOffsets Output vector of frame start offsets
     * @return True if frame boundaries were successfully detected
     */
    bool DetectFLACFrameBoundaries(const std::vector<uint8_t>& sampleData, 
                                  std::vector<size_t>& frameOffsets);
    
    /**
     * @brief Validate FLAC frame header structure
     * @param data Sample data containing potential FLAC frame
     * @param offset Offset to start of potential frame header
     * @return True if frame header is valid
     */
    bool ValidateFLACFrameHeader(const std::vector<uint8_t>& data, size_t offset);
    
    /**
     * @brief Handle progressive download with movie box at end
     * @return true if movie box was found and parsed
     */
    bool HandleProgressiveDownload();
    
    /**
     * @brief Check if sample data is available for streaming
     * @param offset Sample data offset
     * @param size Sample data size
     * @return true if data is available or after waiting for it
     */
    bool EnsureSampleDataAvailable(uint64_t offset, size_t size);
    
    /**
     * @brief Prefetch upcoming samples for streaming
     * @param currentSample Current sample index
     * @param track Current audio track
     */
    void PrefetchUpcomingSamples(uint64_t currentSample, const AudioTrackInfo& track);
    
    /**
     * @brief Handle corrupted box structures with recovery
     * @param header Potentially corrupted box header
     * @param containerSize Size of the container box
     * @return Recovered box header or empty header if unrecoverable
     */
    BoxHeader HandleCorruptedBox(const BoxHeader& header, uint64_t containerSize);
    
    /**
     * @brief Validate and repair sample tables for consistency
     * @param track Audio track with potentially inconsistent sample tables
     * @return true if tables are valid or were successfully repaired
     */
    bool ValidateAndRepairSampleTables(AudioTrackInfo& track);
    
    /**
     * @brief Handle missing codec configuration by inference
     * @param track Audio track with missing configuration
     * @param sampleData Sample data to analyze for inference
     * @return true if configuration was successfully inferred or already present
     */
    bool HandleMissingCodecConfig(AudioTrackInfo& track, const std::vector<uint8_t>& sampleData);
    
    /**
     * @brief Perform I/O operation with retry and error recovery
     * @param operation Function to execute with retry logic
     * @param errorContext Description of the operation for error reporting
     * @return true if operation succeeded, false if all retries failed
     */
    bool PerformIOWithRetry(std::function<bool()> operation, const std::string& errorContext);
    
    /**
     * @brief Handle memory allocation failures gracefully
     * @param requestedSize Size of failed allocation
     * @param context Description of what was being allocated
     * @return true if recovery was possible, false if unrecoverable
     */
    bool HandleMemoryAllocationFailure(size_t requestedSize, const std::string& context);
    
    /**
     * @brief Report error with context and statistics
     * @param errorType Type of error for categorization
     * @param message Detailed error message
     * @param boxType Box type if applicable (0 if not applicable)
     */
    void ReportError(const std::string& errorType, const std::string& message, uint32_t boxType = 0);
    
    /**
     * @brief Attempt to recover from corrupted box with comprehensive error handling
     * @param header Potentially corrupted box header
     * @param containerSize Size of the container box
     * @return Recovered box header or empty header if unrecoverable
     */
    BoxHeader RecoverCorruptedBoxWithRetry(const BoxHeader& header, uint64_t containerSize);
    
    /**
     * @brief Validate sample tables and attempt repair if inconsistent
     * @param track Audio track with sample tables to validate
     * @return true if tables are valid or successfully repaired
     */
    bool ValidateAndRepairSampleTablesWithRecovery(AudioTrackInfo& track);
    
    /**
     * @brief Handle missing codec configuration with multiple inference strategies
     * @param track Audio track with missing configuration
     * @return true if configuration was successfully inferred or already present
     */
    bool HandleMissingCodecConfigWithInference(AudioTrackInfo& track);
    
    /**
     * @brief Perform I/O operation with comprehensive retry and error recovery
     * @param operation Function to execute with retry logic
     * @param errorContext Description of the operation for error reporting
     * @param maxRetries Maximum number of retry attempts
     * @return true if operation succeeded, false if all retries failed
     */
    bool PerformIOWithComprehensiveRetry(std::function<bool()> operation, 
                                        const std::string& errorContext, 
                                        int maxRetries = 3);
    
    /**
     * @brief Handle memory allocation failures with graceful degradation
     * @param requestedSize Size of failed allocation
     * @param context Description of what was being allocated
     * @param fallbackStrategy Function to attempt alternative allocation strategy
     * @return true if recovery was possible, false if unrecoverable
     */
    bool HandleMemoryAllocationFailureWithFallback(size_t requestedSize, 
                                                   const std::string& context,
                                                   std::function<bool()> fallbackStrategy = nullptr);
};


} // namespace ISO
} // namespace Demuxer
} // namespace PsyMP3
#endif // ISODEMUXER_H