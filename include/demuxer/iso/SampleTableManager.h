/*
 * SampleTableManager.h - Sample table manager for efficient sample lookups
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef SAMPLETABLEMANAGER_H
#define SAMPLETABLEMANAGER_H

namespace PsyMP3 {
namespace Demuxer {
namespace ISO {

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Sample table manager for efficient sample lookups with performance optimizations
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
    
    // Memory management and optimization methods
    void OptimizeMemoryUsage();
    void EnableLazyLoading(bool enable) { lazyLoadingEnabled = enable; }
    size_t GetMemoryFootprint() const;
    
    // Memory pressure specific optimizations (Requirement 8.8)
    void OptimizeForCriticalMemoryPressure();
    void OptimizeForHighMemoryPressure();
    void OptimizeForNormalMemoryPressure();
    
private:
    // Legacy chunk info structure for compatibility
    struct ChunkInfo {
        uint64_t offset;
        uint32_t sampleCount;
        uint32_t firstSample;
    };
    
    // Optimized compressed sample-to-chunk mapping (Requirement 8.2)
    struct CompressedChunkInfo {
        uint64_t baseOffset;        // Base offset for chunk range
        uint32_t chunkCount;        // Number of chunks in this range
        uint32_t samplesPerChunk;   // Samples per chunk (constant within range)
        uint32_t firstSample;       // First sample index in this range
        uint32_t totalSamples;      // Total samples in this range
        uint32_t averageChunkSize = 0; // Average chunk size for memory optimization
    };
    std::vector<CompressedChunkInfo> compressedChunkTable;
    
    // Original chunk table for fallback (lazy loaded)
    mutable std::vector<ChunkInfo> chunkTable;
    mutable bool chunkTableLoaded = false;
    
    // Legacy time-to-sample structure for compatibility
    struct TimeToSampleEntry {
        uint64_t sampleIndex;
        uint64_t timestamp;
        uint32_t duration;
    };
    std::vector<TimeToSampleEntry> timeTable;
    
    // Optimized time-to-sample lookup with binary search tree structure (Requirement 8.3)
    struct OptimizedTimeEntry {
        uint64_t sampleIndex;
        uint64_t timestamp;
        uint32_t duration;
        uint32_t sampleRange;       // Number of samples this entry covers
    };
    std::vector<OptimizedTimeEntry> optimizedTimeTable;
    
    // Hierarchical index for large time tables (Requirement 8.3)
    struct HierarchicalTimeIndex {
        size_t entryIndex;          // Index into optimizedTimeTable
        uint64_t timestamp;         // Timestamp at this index
        uint64_t sampleIndex;       // Sample index at this index
    };
    std::vector<HierarchicalTimeIndex> hierarchicalTimeIndex;
    
    // Lazy-loaded sample size table with chunked loading (Requirement 8.1)
    struct LazyLoadedSampleSizes {
        mutable bool isLoaded = false;
        bool isCompressed = false;
        uint32_t fixedSize = 0;                    // For compressed (all same size)
        mutable std::vector<uint32_t> variableSizes; // For variable sizes (lazy loaded)
        
        // Chunked loading for memory optimization
        mutable bool chunkedMode = false;
        mutable size_t chunkSize = 256;            // Samples per chunk
        
        struct SampleChunk {
            std::vector<uint32_t> samples;
            std::chrono::steady_clock::time_point lastAccess;
        };
        mutable std::map<size_t, SampleChunk> sampleChunks; // Chunk cache
        
        // Lazy loading parameters
        std::shared_ptr<IOHandler> io;
        uint64_t tableOffset = 0;
        uint32_t sampleCount = 0;
        
        void LoadIfNeeded() const;
        void LoadChunkedSampleSizes() const;
        void LoadChunk(size_t chunkIndex) const;
        void EvictOldestChunk() const;
        uint32_t GetSize(uint64_t sampleIndex) const;
    };
    mutable LazyLoadedSampleSizes sampleSizes;
    
    // Optimized sync sample table with binary search
    std::vector<uint64_t> syncSamples;
    
    // Performance optimization flags
    bool lazyLoadingEnabled = true;
    bool memoryOptimizationEnabled = true;
    
    // Memory usage tracking
    mutable size_t estimatedMemoryUsage = 0;
    
    // Private helper methods for building and managing sample tables
    bool BuildOptimizedChunkTable(const SampleTableInfo& rawTables);
    bool BuildOptimizedTimeTable(const SampleTableInfo& rawTables);
    bool BuildLazyLoadedSampleSizeTable(const SampleTableInfo& rawTables);
    void BuildHierarchicalTimeIndex();
    bool ValidateTableConsistency();
    bool ValidateTableConsistencyDetailed();
    
    // Optimized lookup methods
    const CompressedChunkInfo* FindCompressedChunkForSample(uint64_t sampleIndex) const;
    ChunkInfo* FindChunkForSample(uint64_t sampleIndex) const;
    uint32_t GetSampleSize(uint64_t sampleIndex) const;
    uint32_t GetSampleDuration(uint64_t sampleIndex) const;
    bool IsSyncSample(uint64_t sampleIndex) const;
    
    // Memory optimization helpers
    void CompressSampleToChunkMapping(const SampleTableInfo& rawTables);
    void OptimizeTimeToSampleTable(const SampleTableInfo& rawTables);
    void CalculateMemoryFootprint() const;
    
    // Lazy loading helpers
    void EnsureChunkTableLoaded() const;
    void LoadSampleSizesIfNeeded() const;
    
    // Legacy methods for compatibility
    bool BuildChunkTable(const SampleTableInfo& rawTables);
    bool BuildTimeTable(const SampleTableInfo& rawTables);
    bool BuildSampleSizeTable(const SampleTableInfo& rawTables);
    bool BuildExpandedSampleToChunkMapping(const SampleTableInfo& rawTables, 
                                          std::vector<uint32_t>& expandedMapping);
    uint32_t GetSamplesPerChunkForIndex(size_t chunkIndex, const std::vector<uint32_t>& samplesPerChunk);
};


} // namespace ISO
} // namespace Demuxer
} // namespace PsyMP3
#endif // SAMPLETABLEMANAGER_H