/*
 * ISODemuxerFragmentHandler.h - Fragment handler for fragmented MP4 support
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef ISODEMUXERFRAGMENTHANDLER_H
#define ISODEMUXERFRAGMENTHANDLER_H

#include "Demuxer.h"
#include <memory>
#include <vector>

/**
 * @brief Track fragment information structure
 */
struct TrackFragmentInfo {
    uint32_t trackId;
    uint64_t baseDataOffset;
    uint32_t sampleDescriptionIndex;
    uint32_t defaultSampleDuration;
    uint32_t defaultSampleSize;
    uint32_t defaultSampleFlags;
    
    // Track run information
    struct TrackRunInfo {
        uint32_t sampleCount;
        uint32_t dataOffset;
        uint32_t firstSampleFlags;
        std::vector<uint32_t> sampleDurations;
        std::vector<uint32_t> sampleSizes;
        std::vector<uint32_t> sampleFlags;
        std::vector<uint32_t> sampleCompositionTimeOffsets;
    };
    
    std::vector<TrackRunInfo> trackRuns;
    uint64_t tfdt = 0; // Track fragment decode time
};

/**
 * @brief Movie fragment header information
 */
struct MovieFragmentInfo {
    uint32_t sequenceNumber;
    uint64_t moofOffset;
    uint64_t mdatOffset;
    uint64_t mdatSize;
    std::vector<TrackFragmentInfo> trackFragments;
    bool isComplete = false;
};

/**
 * @brief Fragment handler for fragmented MP4 support
 */
class ISODemuxerFragmentHandler {
public:
    ISODemuxerFragmentHandler() = default;
    ~ISODemuxerFragmentHandler() = default;
    
    // Core fragment processing
    bool ProcessMovieFragment(uint64_t moofOffset, std::shared_ptr<IOHandler> io);
    bool UpdateSampleTables(const TrackFragmentInfo& traf, AudioTrackInfo& track);
    bool IsFragmented() const { return hasFragments; }
    
    // Fragment navigation and management
    bool SeekToFragment(uint32_t sequenceNumber);
    MovieFragmentInfo* GetCurrentFragment();
    MovieFragmentInfo* GetFragment(uint32_t sequenceNumber);
    uint32_t GetFragmentCount() const { return static_cast<uint32_t>(fragments.size()); }
    
    // Fragment ordering and buffering
    bool AddFragment(const MovieFragmentInfo& fragment);
    bool ReorderFragments();
    bool IsFragmentComplete(uint32_t sequenceNumber) const;
    
    // Sample extraction from fragments
    bool ExtractFragmentSample(uint32_t trackId, uint64_t sampleIndex, 
                              uint64_t& offset, uint32_t& size);
    
    // Default value handling
    void SetDefaultValues(const AudioTrackInfo& movieHeaderDefaults);
    
private:
    bool hasFragments = false;
    std::vector<MovieFragmentInfo> fragments;
    uint32_t currentFragmentIndex = 0;
    
    // Default values from movie header for missing fragment headers
    struct DefaultValues {
        uint32_t defaultSampleDuration = 0;
        uint32_t defaultSampleSize = 0;
        uint32_t defaultSampleFlags = 0;
    } defaults;
    
    // Fragment parsing methods
    bool ParseMovieFragmentBox(uint64_t offset, uint64_t size, std::shared_ptr<IOHandler> io, 
                              MovieFragmentInfo& fragment);
    bool ParseMovieFragmentHeader(uint64_t offset, uint64_t size, std::shared_ptr<IOHandler> io,
                                 MovieFragmentInfo& fragment);
    bool ParseTrackFragmentBox(uint64_t offset, uint64_t size, std::shared_ptr<IOHandler> io,
                              TrackFragmentInfo& traf);
    bool ParseTrackFragmentHeader(uint64_t offset, uint64_t size, std::shared_ptr<IOHandler> io,
                                 TrackFragmentInfo& traf);
    bool ParseTrackFragmentRun(uint64_t offset, uint64_t size, std::shared_ptr<IOHandler> io,
                              TrackFragmentInfo::TrackRunInfo& trun);
    bool ParseTrackFragmentDecodeTime(uint64_t offset, uint64_t size, std::shared_ptr<IOHandler> io,
                                     TrackFragmentInfo& traf);
    
    // Fragment validation and consistency checking
    bool ValidateFragment(const MovieFragmentInfo& fragment) const;
    bool ValidateTrackFragment(const TrackFragmentInfo& traf) const;
    
    // Helper methods
    uint32_t ReadUInt32BE(std::shared_ptr<IOHandler> io, uint64_t offset);
    uint64_t ReadUInt64BE(std::shared_ptr<IOHandler> io, uint64_t offset);
    uint64_t FindMediaDataBox(uint64_t moofOffset, std::shared_ptr<IOHandler> io);
    
    // Fragment ordering helpers
    static bool CompareFragmentsBySequence(const MovieFragmentInfo& a, const MovieFragmentInfo& b);
    bool HasMissingFragments() const;
    void FillMissingFragmentGaps();
};

#endif // ISODEMUXERFRAGMENTHANDLER_H