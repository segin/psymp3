/*
 * FragmentHandler.h - Fragment handler for fragmented MP4 support
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FRAGMENTHANDLER_H
#define FRAGMENTHANDLER_H

namespace PsyMP3 {
namespace Demuxer {
namespace ISO {

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Track fragment information structure
 */
struct TrackFragmentInfo {
    // Default-initialize every scalar: the tfhd/trun parsers only assign these
    // when the corresponding optional flag is present, and the standard
    // default-base-is-moof layout leaves baseDataOffset unwritten. Reading an
    // indeterminate value as a file offset (UpdateSampleTables/ExtractFragment-
    // Sample) is UB and seeks to a random position. Zero is the correct default.
    uint32_t trackId = 0;
    uint64_t baseDataOffset = 0;
    uint32_t sampleDescriptionIndex = 0;
    uint32_t defaultSampleDuration = 0;
    uint32_t defaultSampleSize = 0;
    uint32_t defaultSampleFlags = 0;

    // Track run information
    struct TrackRunInfo {
        uint32_t sampleCount = 0;
        // Signed per ISO/IEC 14496-12 §8.8.8 (data can precede the base offset);
        // a uint32_t zero-extends a negative value into a ~4 GB positive offset.
        int32_t dataOffset = 0;
        uint32_t firstSampleFlags = 0;
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
    uint32_t sequenceNumber = 0;
    uint64_t moofOffset = 0;
    uint64_t mdatOffset = 0;
    uint64_t mdatSize = 0;
    std::vector<TrackFragmentInfo> trackFragments;
    bool isComplete = false;
};

/**
 * @brief Fragment handler for fragmented MP4 support
 */
class FragmentHandler {
public:
    FragmentHandler() = default;
    ~FragmentHandler() = default;
    
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


} // namespace ISO
} // namespace Demuxer
} // namespace PsyMP3
#endif // FRAGMENTHANDLER_H