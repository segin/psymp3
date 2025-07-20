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

// ISO box type constants
constexpr uint32_t BOX_FTYP = 0x66747970; // "ftyp"
constexpr uint32_t BOX_MOOV = 0x6D6F6F76; // "moov"
constexpr uint32_t BOX_MDAT = 0x6D646174; // "mdat"
constexpr uint32_t BOX_TRAK = 0x7472616B; // "trak"
constexpr uint32_t BOX_MDIA = 0x6D646961; // "mdia"
constexpr uint32_t BOX_MINF = 0x6D696E66; // "minf"
constexpr uint32_t BOX_STBL = 0x7374626C; // "stbl"
constexpr uint32_t BOX_STSD = 0x73747364; // "stsd"
constexpr uint32_t BOX_STTS = 0x73747473; // "stts"
constexpr uint32_t BOX_STSC = 0x73747363; // "stsc"
constexpr uint32_t BOX_STSZ = 0x7374737A; // "stsz"
constexpr uint32_t BOX_STCO = 0x7374636F; // "stco"
constexpr uint32_t BOX_CO64 = 0x636F3634; // "co64"

// Audio codec types
constexpr uint32_t CODEC_AAC  = 0x6D703461; // "mp4a"
constexpr uint32_t CODEC_ALAC = 0x616C6163; // "alac"
constexpr uint32_t CODEC_ULAW = 0x756C6177; // "ulaw"
constexpr uint32_t CODEC_ALAW = 0x616C6177; // "alaw"
constexpr uint32_t CODEC_LPCM = 0x6C70636D; // "lpcm"

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
    
private:
    std::shared_ptr<IOHandler> io;
    std::stack<BoxHeader> boxStack;
    
    BoxHeader ReadBoxHeader(uint64_t offset);
    bool SkipUnknownBox(const BoxHeader& header);
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
};

#endif // ISODEMUXER_H