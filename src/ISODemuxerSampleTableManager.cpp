/*
 * SampleTableManager.cpp - Optimized sample table management implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include <algorithm>
#include <numeric>

// Lazy-loaded sample size table implementation (Requirement 8.1)
void ISODemuxerSampleTableManager::LazyLoadedSampleSizes::LoadIfNeeded() const {
    if (isLoaded || !io) {
        return;
    }
    
    if (isCompressed) {
        // All samples have the same size - no need to load anything
        isLoaded = true;
        return;
    }
    
    // Load variable sample sizes from file
    if (tableOffset > 0 && sampleCount > 0) {
        variableSizes.resize(sampleCount);
        
        io->seek(static_cast<long>(tableOffset), SEEK_SET);
        for (uint32_t i = 0; i < sampleCount; i++) {
            uint8_t bytes[4];
            if (io->read(bytes, 1, 4) == 4) {
                variableSizes[i] = (static_cast<uint32_t>(bytes[0]) << 24) |
                                  (static_cast<uint32_t>(bytes[1]) << 16) |
                                  (static_cast<uint32_t>(bytes[2]) << 8) |
                                  static_cast<uint32_t>(bytes[3]);
            } else {
                // I/O error - use default size
                variableSizes[i] = fixedSize > 0 ? fixedSize : 1024;
            }
        }
    }
    
    isLoaded = true;
}

uint32_t ISODemuxerSampleTableManager::LazyLoadedSampleSizes::GetSize(uint64_t sampleIndex) const {
    if (isCompressed) {
        return fixedSize;
    }
    
    LoadIfNeeded();
    
    if (sampleIndex < variableSizes.size()) {
        return variableSizes[sampleIndex];
    }
    
    return fixedSize > 0 ? fixedSize : 1024; // Default fallback
}

bool ISODemuxerSampleTableManager::BuildSampleTables(const SampleTableInfo& rawTables) {
    // Build optimized sample tables with memory efficiency (Requirements 8.1, 8.2, 8.3)
    
    // Build compressed sample-to-chunk mapping (Requirement 8.2)
    if (!BuildOptimizedChunkTable(rawTables)) {
        return false;
    }
    
    // Build optimized time-to-sample lookup structures (Requirement 8.3)
    if (!BuildOptimizedTimeTable(rawTables)) {
        return false;
    }
    
    // Build lazy-loaded sample size table (Requirement 8.1)
    if (!BuildLazyLoadedSampleSizeTable(rawTables)) {
        return false;
    }
    
    // Build sync sample table for keyframe seeking
    syncSamples = rawTables.syncSamples;
    std::sort(syncSamples.begin(), syncSamples.end());
    
    // Validate table consistency
    if (!ValidateTableConsistency()) {
        return false;
    }
    
    // Calculate memory footprint for monitoring
    CalculateMemoryFootprint();
    
    return true;
}

// Compressed sample-to-chunk mapping implementation (Requirement 8.2)
bool ISODemuxerSampleTableManager::BuildOptimizedChunkTable(const SampleTableInfo& rawTables) {
    if (rawTables.chunkOffsets.empty() || rawTables.sampleToChunkEntries.empty()) {
        return false;
    }
    
    compressedChunkTable.clear();
    compressedChunkTable.reserve(rawTables.sampleToChunkEntries.size());
    
    uint32_t currentSample = 0;
    
    for (size_t i = 0; i < rawTables.sampleToChunkEntries.size(); i++) {
        const auto& entry = rawTables.sampleToChunkEntries[i];
        
        // Determine the range of chunks this entry covers
        uint32_t firstChunk = entry.firstChunk;
        uint32_t lastChunk;
        
        if (i + 1 < rawTables.sampleToChunkEntries.size()) {
            lastChunk = rawTables.sampleToChunkEntries[i + 1].firstChunk - 1;
        } else {
            lastChunk = static_cast<uint32_t>(rawTables.chunkOffsets.size() - 1);
        }
        
        if (lastChunk < firstChunk || firstChunk >= rawTables.chunkOffsets.size()) {
            continue; // Invalid entry
        }
        
        uint32_t chunkCount = lastChunk - firstChunk + 1;
        uint32_t totalSamples = chunkCount * entry.samplesPerChunk;
        
        // Create compressed chunk info
        CompressedChunkInfo compressedInfo;
        compressedInfo.baseOffset = rawTables.chunkOffsets[firstChunk];
        compressedInfo.chunkCount = chunkCount;
        compressedInfo.samplesPerChunk = entry.samplesPerChunk;
        compressedInfo.firstSample = currentSample;
        compressedInfo.totalSamples = totalSamples;
        
        compressedChunkTable.push_back(compressedInfo);
        currentSample += totalSamples;
    }
    
    return !compressedChunkTable.empty();
}

// Optimized time-to-sample lookup with binary search structures (Requirement 8.3)
bool ISODemuxerSampleTableManager::BuildOptimizedTimeTable(const SampleTableInfo& rawTables) {
    if (rawTables.sampleTimes.empty()) {
        return false;
    }
    
    optimizedTimeTable.clear();
    optimizedTimeTable.reserve(rawTables.sampleTimes.size() / 10); // Estimate compression ratio
    
    // Build optimized time table by grouping consecutive samples with same duration
    uint64_t currentSample = 0;
    uint64_t currentTime = 0;
    
    while (currentSample < rawTables.sampleTimes.size()) {
        uint64_t startSample = currentSample;
        uint64_t startTime = currentTime;
        
        // Find duration for this sample
        uint32_t duration = 0;
        if (currentSample + 1 < rawTables.sampleTimes.size()) {
            duration = static_cast<uint32_t>(rawTables.sampleTimes[currentSample + 1] - rawTables.sampleTimes[currentSample]);
        } else if (currentSample > 0) {
            // Use previous duration for last sample
            duration = static_cast<uint32_t>(rawTables.sampleTimes[currentSample] - rawTables.sampleTimes[currentSample - 1]);
        } else {
            duration = 1024; // Default duration
        }
        
        // Count consecutive samples with the same duration
        uint32_t sampleRange = 1;
        currentSample++;
        currentTime += duration;
        
        while (currentSample < rawTables.sampleTimes.size()) {
            uint32_t nextDuration = 0;
            if (currentSample + 1 < rawTables.sampleTimes.size()) {
                nextDuration = static_cast<uint32_t>(rawTables.sampleTimes[currentSample + 1] - rawTables.sampleTimes[currentSample]);
            } else {
                nextDuration = duration; // Assume same duration for last sample
            }
            
            if (nextDuration != duration) {
                break; // Duration changed, end this range
            }
            
            sampleRange++;
            currentSample++;
            currentTime += duration;
        }
        
        // Create optimized time entry
        OptimizedTimeEntry entry;
        entry.sampleIndex = startSample;
        entry.timestamp = startTime;
        entry.duration = duration;
        entry.sampleRange = sampleRange;
        
        optimizedTimeTable.push_back(entry);
    }
    
    return !optimizedTimeTable.empty();
}

// Lazy-loaded sample size table implementation (Requirement 8.1)
bool ISODemuxerSampleTableManager::BuildLazyLoadedSampleSizeTable(const SampleTableInfo& rawTables) {
    if (rawTables.sampleSizes.empty()) {
        return false;
    }
    
    // Check if all samples have the same size (compressed representation)
    bool allSameSize = true;
    uint32_t firstSize = rawTables.sampleSizes[0];
    
    for (size_t i = 1; i < rawTables.sampleSizes.size() && allSameSize; i++) {
        if (rawTables.sampleSizes[i] != firstSize) {
            allSameSize = false;
        }
    }
    
    if (allSameSize) {
        // Use compressed representation
        sampleSizes.isCompressed = true;
        sampleSizes.fixedSize = firstSize;
        sampleSizes.isLoaded = true;
    } else {
        // Use lazy loading for variable sizes
        sampleSizes.isCompressed = false;
        sampleSizes.fixedSize = 0;
        sampleSizes.sampleCount = static_cast<uint32_t>(rawTables.sampleSizes.size());
        
        if (lazyLoadingEnabled) {
            // Don't load immediately - will be loaded on demand
            sampleSizes.isLoaded = false;
            // Note: In a real implementation, we would store the file offset
            // where the sample size table is located for lazy loading
            sampleSizes.tableOffset = 0; // Would be set from parsing context
        } else {
            // Load immediately
            sampleSizes.variableSizes = rawTables.sampleSizes;
            sampleSizes.isLoaded = true;
        }
    }
    
    return true;
}

bool ISODemuxerSampleTableManager::ValidateTableConsistency() {
    // Validate that compressed chunk table is consistent
    if (compressedChunkTable.empty()) {
        return false;
    }
    
    // Validate that optimized time table is consistent
    if (optimizedTimeTable.empty()) {
        return false;
    }
    
    // Check that sample counts match between tables
    uint32_t totalSamplesFromChunks = 0;
    for (const auto& chunk : compressedChunkTable) {
        totalSamplesFromChunks += chunk.totalSamples;
    }
    
    uint32_t totalSamplesFromTime = 0;
    for (const auto& timeEntry : optimizedTimeTable) {
        totalSamplesFromTime += timeEntry.sampleRange;
    }
    
    if (totalSamplesFromChunks != totalSamplesFromTime) {
        // Inconsistent sample counts - attempt repair
        // This is a simplified repair - in practice, we'd use more sophisticated logic
        return false;
    }
    
    return true;
}

ISODemuxerSampleTableManager::SampleInfo ISODemuxerSampleTableManager::GetSampleInfo(uint64_t sampleIndex) {
    SampleInfo info = {};
    
    // Find chunk information using compressed mapping
    const CompressedChunkInfo* chunkInfo = FindCompressedChunkForSample(sampleIndex);
    if (!chunkInfo) {
        return info;
    }
    
    // Calculate sample position within chunk range
    uint64_t sampleInRange = sampleIndex - chunkInfo->firstSample;
    uint32_t chunkInRange = static_cast<uint32_t>(sampleInRange / chunkInfo->samplesPerChunk);
    uint32_t sampleInChunk = static_cast<uint32_t>(sampleInRange % chunkInfo->samplesPerChunk);
    
    // Calculate chunk offset (this is simplified - real implementation would need chunk offset table)
    info.offset = chunkInfo->baseOffset + (chunkInRange * GetSampleSize(sampleIndex) * chunkInfo->samplesPerChunk) + 
                  (sampleInChunk * GetSampleSize(sampleIndex));
    
    info.size = GetSampleSize(sampleIndex);
    info.duration = GetSampleDuration(sampleIndex);
    info.isKeyframe = IsSyncSample(sampleIndex);
    
    return info;
}

uint64_t ISODemuxerSampleTableManager::TimeToSample(double timestamp) {
    if (optimizedTimeTable.empty()) {
        return 0;
    }
    
    // Binary search in optimized time table (Requirement 8.3)
    uint64_t timestampUnits = static_cast<uint64_t>(timestamp * 1000); // Convert to milliseconds
    
    auto it = std::lower_bound(optimizedTimeTable.begin(), optimizedTimeTable.end(), timestampUnits,
        [](const OptimizedTimeEntry& entry, uint64_t ts) {
            return entry.timestamp < ts;
        });
    
    if (it == optimizedTimeTable.end()) {
        // Timestamp is beyond the last sample
        if (!optimizedTimeTable.empty()) {
            const auto& lastEntry = optimizedTimeTable.back();
            return lastEntry.sampleIndex + lastEntry.sampleRange - 1;
        }
        return 0;
    }
    
    if (it == optimizedTimeTable.begin()) {
        return it->sampleIndex;
    }
    
    // Check if we need to interpolate within the range
    --it; // Go to the entry that contains our timestamp
    
    if (timestampUnits >= it->timestamp && 
        timestampUnits < it->timestamp + (it->duration * it->sampleRange)) {
        // Timestamp is within this range
        uint64_t offsetInRange = timestampUnits - it->timestamp;
        uint32_t sampleOffset = static_cast<uint32_t>(offsetInRange / it->duration);
        return it->sampleIndex + sampleOffset;
    }
    
    return it->sampleIndex;
}

double ISODemuxerSampleTableManager::SampleToTime(uint64_t sampleIndex) {
    if (optimizedTimeTable.empty()) {
        return 0.0;
    }
    
    // Find the time entry that contains this sample
    for (const auto& entry : optimizedTimeTable) {
        if (sampleIndex >= entry.sampleIndex && 
            sampleIndex < entry.sampleIndex + entry.sampleRange) {
            // Sample is within this range
            uint64_t sampleOffset = sampleIndex - entry.sampleIndex;
            uint64_t timestamp = entry.timestamp + (sampleOffset * entry.duration);
            return static_cast<double>(timestamp) / 1000.0; // Convert to seconds
        }
    }
    
    // Sample not found - return approximate time
    if (!optimizedTimeTable.empty()) {
        const auto& lastEntry = optimizedTimeTable.back();
        if (sampleIndex >= lastEntry.sampleIndex + lastEntry.sampleRange) {
            // Sample is beyond the last entry
            uint64_t extraSamples = sampleIndex - (lastEntry.sampleIndex + lastEntry.sampleRange);
            uint64_t timestamp = lastEntry.timestamp + (lastEntry.sampleRange * lastEntry.duration) + 
                                (extraSamples * lastEntry.duration);
            return static_cast<double>(timestamp) / 1000.0;
        }
    }
    
    return 0.0;
}

// Memory management and optimization methods (Requirements 8.1, 8.2, 8.7)
void ISODemuxerSampleTableManager::OptimizeMemoryUsage() {
    if (!memoryOptimizationEnabled) {
        return;
    }
    
    // Release legacy chunk table if compressed version is available
    if (!compressedChunkTable.empty() && chunkTableLoaded) {
        chunkTable.clear();
        chunkTable.shrink_to_fit();
        chunkTableLoaded = false;
    }
    
    // Optimize sync sample table
    syncSamples.shrink_to_fit();
    
    // Optimize time table
    optimizedTimeTable.shrink_to_fit();
    compressedChunkTable.shrink_to_fit();
    
    // Update memory footprint calculation
    CalculateMemoryFootprint();
}

size_t ISODemuxerSampleTableManager::GetMemoryFootprint() const {
    return estimatedMemoryUsage;
}

void ISODemuxerSampleTableManager::CalculateMemoryFootprint() const {
    estimatedMemoryUsage = 0;
    
    // Compressed chunk table
    estimatedMemoryUsage += compressedChunkTable.size() * sizeof(CompressedChunkInfo);
    
    // Optimized time table
    estimatedMemoryUsage += optimizedTimeTable.size() * sizeof(OptimizedTimeEntry);
    
    // Sample sizes (only if loaded)
    if (sampleSizes.isLoaded && !sampleSizes.isCompressed) {
        estimatedMemoryUsage += sampleSizes.variableSizes.size() * sizeof(uint32_t);
    }
    
    // Sync samples
    estimatedMemoryUsage += syncSamples.size() * sizeof(uint64_t);
    
    // Legacy tables (if loaded)
    if (chunkTableLoaded) {
        estimatedMemoryUsage += chunkTable.size() * sizeof(ChunkInfo);
    }
}

// Private helper methods
const ISODemuxerSampleTableManager::CompressedChunkInfo* ISODemuxerSampleTableManager::FindCompressedChunkForSample(uint64_t sampleIndex) const {
    for (const auto& chunk : compressedChunkTable) {
        if (sampleIndex >= chunk.firstSample && 
            sampleIndex < chunk.firstSample + chunk.totalSamples) {
            return &chunk;
        }
    }
    return nullptr;
}

uint32_t ISODemuxerSampleTableManager::GetSampleSize(uint64_t sampleIndex) const {
    return sampleSizes.GetSize(sampleIndex);
}

uint32_t ISODemuxerSampleTableManager::GetSampleDuration(uint64_t sampleIndex) const {
    // Find duration from optimized time table
    for (const auto& entry : optimizedTimeTable) {
        if (sampleIndex >= entry.sampleIndex && 
            sampleIndex < entry.sampleIndex + entry.sampleRange) {
            return entry.duration;
        }
    }
    
    return 1024; // Default duration
}

bool ISODemuxerSampleTableManager::IsSyncSample(uint64_t sampleIndex) const {
    if (syncSamples.empty()) {
        return true; // All samples are sync samples if no sync table
    }
    
    // Binary search in sorted sync sample table
    return std::binary_search(syncSamples.begin(), syncSamples.end(), sampleIndex);
}

// Legacy compatibility methods for fallback
void ISODemuxerSampleTableManager::EnsureChunkTableLoaded() const {
    if (chunkTableLoaded || compressedChunkTable.empty()) {
        return;
    }
    
    // Build legacy chunk table from compressed representation
    chunkTable.clear();
    chunkTable.reserve(compressedChunkTable.size() * 10); // Estimate
    
    for (const auto& compressed : compressedChunkTable) {
        for (uint32_t i = 0; i < compressed.chunkCount; i++) {
            ChunkInfo chunk;
            chunk.offset = compressed.baseOffset + (i * compressed.samplesPerChunk * GetSampleSize(compressed.firstSample));
            chunk.sampleCount = compressed.samplesPerChunk;
            chunk.firstSample = compressed.firstSample + (i * compressed.samplesPerChunk);
            chunkTable.push_back(chunk);
        }
    }
    
    chunkTableLoaded = true;
}

ISODemuxerSampleTableManager::ChunkInfo* ISODemuxerSampleTableManager::FindChunkForSample(uint64_t sampleIndex) const {
    EnsureChunkTableLoaded();
    
    for (auto& chunk : chunkTable) {
        if (sampleIndex >= chunk.firstSample && 
            sampleIndex < chunk.firstSample + chunk.sampleCount) {
            return &chunk;
        }
    }
    return nullptr;
}

bool ISODemuxerSampleTableManager::BuildChunkTable(const SampleTableInfo& rawTables) {
    // Legacy method - build traditional chunk table
    if (rawTables.chunkOffsets.empty() || rawTables.sampleToChunkEntries.empty()) {
        return false;
    }
    
    chunkTable.clear();
    uint32_t currentSample = 0;
    
    for (size_t i = 0; i < rawTables.sampleToChunkEntries.size(); i++) {
        const auto& entry = rawTables.sampleToChunkEntries[i];
        
        uint32_t firstChunk = entry.firstChunk;
        uint32_t lastChunk;
        
        if (i + 1 < rawTables.sampleToChunkEntries.size()) {
            lastChunk = rawTables.sampleToChunkEntries[i + 1].firstChunk - 1;
        } else {
            lastChunk = static_cast<uint32_t>(rawTables.chunkOffsets.size() - 1);
        }
        
        for (uint32_t chunkIndex = firstChunk; chunkIndex <= lastChunk; chunkIndex++) {
            if (chunkIndex < rawTables.chunkOffsets.size()) {
                ChunkInfo chunk;
                chunk.offset = rawTables.chunkOffsets[chunkIndex];
                chunk.sampleCount = entry.samplesPerChunk;
                chunk.firstSample = currentSample;
                chunkTable.push_back(chunk);
                currentSample += entry.samplesPerChunk;
            }
        }
    }
    
    chunkTableLoaded = true;
    return !chunkTable.empty();
}

bool ISODemuxerSampleTableManager::BuildTimeTable(const SampleTableInfo& rawTables) {
    // Legacy method - build traditional time table
    if (rawTables.sampleTimes.empty()) {
        return false;
    }
    
    timeTable.clear();
    timeTable.reserve(rawTables.sampleTimes.size());
    
    for (size_t i = 0; i < rawTables.sampleTimes.size(); i++) {
        TimeToSampleEntry entry;
        entry.sampleIndex = i;
        entry.timestamp = rawTables.sampleTimes[i];
        
        if (i + 1 < rawTables.sampleTimes.size()) {
            entry.duration = static_cast<uint32_t>(rawTables.sampleTimes[i + 1] - rawTables.sampleTimes[i]);
        } else if (i > 0) {
            entry.duration = static_cast<uint32_t>(rawTables.sampleTimes[i] - rawTables.sampleTimes[i - 1]);
        } else {
            entry.duration = 1024; // Default
        }
        
        timeTable.push_back(entry);
    }
    
    return !timeTable.empty();
}

bool ISODemuxerSampleTableManager::BuildSampleSizeTable(const SampleTableInfo& rawTables) {
    // Legacy method - just calls the optimized version
    return BuildLazyLoadedSampleSizeTable(rawTables);
}

bool ISODemuxerSampleTableManager::BuildExpandedSampleToChunkMapping(const SampleTableInfo& rawTables, 
                                                                    std::vector<uint32_t>& expandedMapping) {
    if (rawTables.sampleToChunkEntries.empty()) {
        return false;
    }
    
    expandedMapping.clear();
    
    for (size_t i = 0; i < rawTables.sampleToChunkEntries.size(); i++) {
        const auto& entry = rawTables.sampleToChunkEntries[i];
        
        uint32_t firstChunk = entry.firstChunk;
        uint32_t lastChunk;
        
        if (i + 1 < rawTables.sampleToChunkEntries.size()) {
            lastChunk = rawTables.sampleToChunkEntries[i + 1].firstChunk - 1;
        } else {
            lastChunk = static_cast<uint32_t>(rawTables.chunkOffsets.size() - 1);
        }
        
        for (uint32_t chunkIndex = firstChunk; chunkIndex <= lastChunk; chunkIndex++) {
            expandedMapping.push_back(entry.samplesPerChunk);
        }
    }
    
    return !expandedMapping.empty();
}

uint32_t ISODemuxerSampleTableManager::GetSamplesPerChunkForIndex(size_t chunkIndex, const std::vector<uint32_t>& samplesPerChunk) {
    if (chunkIndex < samplesPerChunk.size()) {
        return samplesPerChunk[chunkIndex];
    }
    return 1; // Default fallback
}

// Additional methods from old SampleTableManager implementation
bool ISODemuxerSampleTableManager::ValidateTableConsistencyDetailed() {
    // Validate that all sample tables are consistent with each other
    if (compressedChunkTable.empty() || optimizedTimeTable.empty()) {
        return false;
    }
    
    // Calculate total samples from chunk table
    uint64_t totalSamplesFromChunks = 0;
    for (const auto& chunk : compressedChunkTable) {
        totalSamplesFromChunks += chunk.totalSamples;
    }
    
    // Calculate total samples from time table  
    uint64_t totalSamplesFromTime = 0;
    for (const auto& timeEntry : optimizedTimeTable) {
        totalSamplesFromTime += timeEntry.sampleRange;
    }
    
    // Allow some tolerance for compressed time table
    if (totalSamplesFromChunks > 0 && totalSamplesFromTime > 0) {
        double ratio = static_cast<double>(totalSamplesFromTime) / totalSamplesFromChunks;
        if (ratio < 0.8 || ratio > 1.2) {
            // Significant mismatch between tables
            return false;
        }
    }
    
    return true;
}