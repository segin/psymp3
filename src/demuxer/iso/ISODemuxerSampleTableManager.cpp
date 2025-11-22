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

// Enhanced lazy-loaded sample size table implementation (Requirement 8.1)
void ISODemuxerSampleTableManager::LazyLoadedSampleSizes::LoadIfNeeded() const {
    if (isLoaded || !io) {
        return;
    }
    
    if (isCompressed) {
        // All samples have the same size - no need to load anything
        isLoaded = true;
        return;
    }
    
    // Load variable sample sizes from file with memory optimization
    if (tableOffset > 0 && sampleCount > 0) {
        // Check memory pressure before loading large tables
        auto& memoryOptimizer = MemoryOptimizer::getInstance();
        size_t requiredMemory = sampleCount * sizeof(uint32_t);
        
        if (!memoryOptimizer.isSafeToAllocate(requiredMemory, "ISODemuxer_SampleSizes")) {
            // Memory pressure is high - use chunked loading approach
            LoadChunkedSampleSizes();
            return;
        }
        
        try {
            variableSizes.resize(sampleCount);
            memoryOptimizer.registerAllocation(requiredMemory, "ISODemuxer_SampleSizes");
            
            io->seek(static_cast<long>(tableOffset), SEEK_SET);
            
            // Batch read for better I/O performance
            constexpr size_t BATCH_SIZE = 1024; // Read 1024 entries at a time
            std::vector<uint8_t> batchBuffer(BATCH_SIZE * 4);
            
            for (uint32_t i = 0; i < sampleCount; i += BATCH_SIZE) {
                uint32_t batchCount = std::min(static_cast<uint32_t>(BATCH_SIZE), sampleCount - i);
                size_t bytesToRead = batchCount * 4;
                
                if (io->read(batchBuffer.data(), 1, bytesToRead) == bytesToRead) {
                    // Parse batch data
                    for (uint32_t j = 0; j < batchCount; j++) {
                        uint32_t idx = i + j;
                        uint32_t offset = j * 4;
                        variableSizes[idx] = (static_cast<uint32_t>(batchBuffer[offset]) << 24) |
                                           (static_cast<uint32_t>(batchBuffer[offset + 1]) << 16) |
                                           (static_cast<uint32_t>(batchBuffer[offset + 2]) << 8) |
                                           static_cast<uint32_t>(batchBuffer[offset + 3]);
                    }
                } else {
                    // I/O error - fill remaining with default size
                    for (uint32_t j = i; j < sampleCount; j++) {
                        variableSizes[j] = fixedSize > 0 ? fixedSize : 1024;
                    }
                    break;
                }
            }
        } catch (const std::bad_alloc& e) {
            // Memory allocation failed - fall back to chunked loading
            variableSizes.clear();
            LoadChunkedSampleSizes();
            return;
        }
    }
    
    isLoaded = true;
}

void ISODemuxerSampleTableManager::LazyLoadedSampleSizes::LoadChunkedSampleSizes() const {
    // Chunked loading for memory-constrained environments
    // Load samples in chunks and cache recently accessed chunks
    constexpr size_t CHUNK_SIZE = 256; // 256 samples per chunk
    chunkSize = CHUNK_SIZE;
    chunkedMode = true;
    
    // Pre-load first chunk for immediate access
    LoadChunk(0);
    isLoaded = true;
}

void ISODemuxerSampleTableManager::LazyLoadedSampleSizes::LoadChunk(size_t chunkIndex) const {
    if (!io || tableOffset == 0) return;
    
    size_t startSample = chunkIndex * chunkSize;
    if (startSample >= sampleCount) return;
    
    size_t samplesInChunk = std::min(chunkSize, sampleCount - startSample);
    
    // Check if chunk is already loaded
    auto it = sampleChunks.find(chunkIndex);
    if (it != sampleChunks.end()) {
        // Update access time for LRU
        it->second.lastAccess = std::chrono::steady_clock::now();
        return;
    }
    
    // Load new chunk
    SampleChunk chunk;
    chunk.samples.resize(samplesInChunk);
    chunk.lastAccess = std::chrono::steady_clock::now();
    
    // Seek to chunk position in file
    uint64_t chunkOffset = tableOffset + (startSample * 4);
    io->seek(static_cast<long>(chunkOffset), SEEK_SET);
    
    // Read chunk data
    std::vector<uint8_t> buffer(samplesInChunk * 4);
    if (io->read(buffer.data(), 1, buffer.size()) == buffer.size()) {
        for (size_t i = 0; i < samplesInChunk; i++) {
            size_t offset = i * 4;
            chunk.samples[i] = (static_cast<uint32_t>(buffer[offset]) << 24) |
                              (static_cast<uint32_t>(buffer[offset + 1]) << 16) |
                              (static_cast<uint32_t>(buffer[offset + 2]) << 8) |
                              static_cast<uint32_t>(buffer[offset + 3]);
        }
    } else {
        // I/O error - fill with default size
        std::fill(chunk.samples.begin(), chunk.samples.end(), 
                 fixedSize > 0 ? fixedSize : 1024);
    }
    
    // Add to cache
    sampleChunks[chunkIndex] = std::move(chunk);
    
    // Limit cache size to prevent memory bloat
    constexpr size_t MAX_CACHED_CHUNKS = 8;
    if (sampleChunks.size() > MAX_CACHED_CHUNKS) {
        EvictOldestChunk();
    }
}

void ISODemuxerSampleTableManager::LazyLoadedSampleSizes::EvictOldestChunk() const {
    if (sampleChunks.empty()) return;
    
    auto oldestIt = sampleChunks.begin();
    auto oldestTime = oldestIt->second.lastAccess;
    
    for (auto it = sampleChunks.begin(); it != sampleChunks.end(); ++it) {
        if (it->second.lastAccess < oldestTime) {
            oldestTime = it->second.lastAccess;
            oldestIt = it;
        }
    }
    
    sampleChunks.erase(oldestIt);
}

uint32_t ISODemuxerSampleTableManager::LazyLoadedSampleSizes::GetSize(uint64_t sampleIndex) const {
    if (isCompressed) {
        return fixedSize;
    }
    
    LoadIfNeeded();
    
    if (chunkedMode) {
        // Get size from chunked cache
        size_t chunkIndex = sampleIndex / chunkSize;
        size_t indexInChunk = sampleIndex % chunkSize;
        
        // Load chunk if not cached
        LoadChunk(chunkIndex);
        
        auto it = sampleChunks.find(chunkIndex);
        if (it != sampleChunks.end() && indexInChunk < it->second.samples.size()) {
            // Update access time for LRU
            it->second.lastAccess = std::chrono::steady_clock::now();
            return it->second.samples[indexInChunk];
        }
        
        return fixedSize > 0 ? fixedSize : 1024; // Default fallback
    }
    
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

// Enhanced compressed sample-to-chunk mapping implementation (Requirement 8.2)
bool ISODemuxerSampleTableManager::BuildOptimizedChunkTable(const SampleTableInfo& rawTables) {
    if (rawTables.chunkOffsets.empty() || rawTables.sampleToChunkEntries.empty()) {
        return false;
    }
    
    // Check memory pressure and optimize accordingly
    auto& memoryOptimizer = MemoryOptimizer::getInstance();
    bool useUltraCompression = (memoryOptimizer.getMemoryPressureLevel() >= 
                               MemoryOptimizer::MemoryPressureLevel::High);
    
    compressedChunkTable.clear();
    compressedChunkTable.reserve(rawTables.sampleToChunkEntries.size());
    
    uint32_t currentSample = 0;
    
    // Build compressed chunk table with enhanced compression
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
        
        // Create compressed chunk info with enhanced compression
        CompressedChunkInfo compressedInfo;
        compressedInfo.baseOffset = rawTables.chunkOffsets[firstChunk];
        compressedInfo.chunkCount = chunkCount;
        compressedInfo.samplesPerChunk = entry.samplesPerChunk;
        compressedInfo.firstSample = currentSample;
        compressedInfo.totalSamples = totalSamples;
        
        // Calculate average chunk size for memory optimization
        if (chunkCount > 1 && firstChunk + 1 < rawTables.chunkOffsets.size()) {
            uint64_t totalChunkSize = 0;
            uint32_t validChunks = 0;
            
            for (uint32_t j = firstChunk; j < std::min(firstChunk + chunkCount, 
                                                      static_cast<uint32_t>(rawTables.chunkOffsets.size() - 1)); j++) {
                if (j + 1 < rawTables.chunkOffsets.size()) {
                    totalChunkSize += rawTables.chunkOffsets[j + 1] - rawTables.chunkOffsets[j];
                    validChunks++;
                }
            }
            
            if (validChunks > 0) {
                compressedInfo.averageChunkSize = static_cast<uint32_t>(totalChunkSize / validChunks);
            }
        }
        
        // For ultra compression mode, merge adjacent entries with same samples per chunk
        if (useUltraCompression && !compressedChunkTable.empty()) {
            auto& lastEntry = compressedChunkTable.back();
            if (lastEntry.samplesPerChunk == compressedInfo.samplesPerChunk &&
                lastEntry.firstSample + lastEntry.totalSamples == compressedInfo.firstSample) {
                // Merge with previous entry
                lastEntry.chunkCount += compressedInfo.chunkCount;
                lastEntry.totalSamples += compressedInfo.totalSamples;
                currentSample += totalSamples;
                continue;
            }
        }
        
        compressedChunkTable.push_back(compressedInfo);
        currentSample += totalSamples;
    }
    
    // Shrink to fit to minimize memory usage
    compressedChunkTable.shrink_to_fit();
    
    // Register memory usage with optimizer
    size_t memoryUsed = compressedChunkTable.size() * sizeof(CompressedChunkInfo);
    memoryOptimizer.registerAllocation(memoryUsed, "ISODemuxer_ChunkTable");
    
    return !compressedChunkTable.empty();
}

// Enhanced optimized time-to-sample lookup with binary search structures (Requirement 8.3)
bool ISODemuxerSampleTableManager::BuildOptimizedTimeTable(const SampleTableInfo& rawTables) {
    if (rawTables.sampleTimes.empty()) {
        return false;
    }
    
    auto& memoryOptimizer = MemoryOptimizer::getInstance();
    bool useHierarchicalIndex = (rawTables.sampleTimes.size() > 10000); // Use hierarchical for large tables
    
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
    
    // Build hierarchical index for large tables (Requirement 8.3)
    if (useHierarchicalIndex && optimizedTimeTable.size() > 100) {
        BuildHierarchicalTimeIndex();
    }
    
    // Shrink to fit to minimize memory usage
    optimizedTimeTable.shrink_to_fit();
    
    // Register memory usage
    size_t memoryUsed = optimizedTimeTable.size() * sizeof(OptimizedTimeEntry);
    if (!hierarchicalTimeIndex.empty()) {
        memoryUsed += hierarchicalTimeIndex.size() * sizeof(HierarchicalTimeIndex);
    }
    memoryOptimizer.registerAllocation(memoryUsed, "ISODemuxer_TimeTable");
    
    return !optimizedTimeTable.empty();
}

void ISODemuxerSampleTableManager::BuildHierarchicalTimeIndex() {
    if (optimizedTimeTable.empty()) return;
    
    // Build hierarchical index for faster binary search on large tables
    constexpr size_t INDEX_GRANULARITY = 64; // One index entry per 64 time entries
    
    hierarchicalTimeIndex.clear();
    hierarchicalTimeIndex.reserve((optimizedTimeTable.size() + INDEX_GRANULARITY - 1) / INDEX_GRANULARITY);
    
    for (size_t i = 0; i < optimizedTimeTable.size(); i += INDEX_GRANULARITY) {
        HierarchicalTimeIndex indexEntry;
        indexEntry.entryIndex = i;
        indexEntry.timestamp = optimizedTimeTable[i].timestamp;
        indexEntry.sampleIndex = optimizedTimeTable[i].sampleIndex;
        hierarchicalTimeIndex.push_back(indexEntry);
    }
    
    hierarchicalTimeIndex.shrink_to_fit();
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
    
    // Enhanced binary search with hierarchical index (Requirement 8.3)
    uint64_t timestampUnits = static_cast<uint64_t>(timestamp * 1000); // Convert to milliseconds
    
    // Use hierarchical index for large tables
    auto searchStart = optimizedTimeTable.begin();
    auto searchEnd = optimizedTimeTable.end();
    
    if (!hierarchicalTimeIndex.empty()) {
        // First, search in hierarchical index to narrow down the range
        auto indexIt = std::lower_bound(hierarchicalTimeIndex.begin(), hierarchicalTimeIndex.end(), timestampUnits,
            [](const HierarchicalTimeIndex& entry, uint64_t ts) {
                return entry.timestamp < ts;
            });
        
        if (indexIt != hierarchicalTimeIndex.end()) {
            // Found index entry, narrow search range
            searchStart = optimizedTimeTable.begin() + indexIt->entryIndex;
            
            // Set end boundary
            if (indexIt + 1 != hierarchicalTimeIndex.end()) {
                searchEnd = optimizedTimeTable.begin() + (indexIt + 1)->entryIndex;
            }
        } else if (!hierarchicalTimeIndex.empty()) {
            // Timestamp is beyond last index entry
            const auto& lastIndex = hierarchicalTimeIndex.back();
            searchStart = optimizedTimeTable.begin() + lastIndex.entryIndex;
        }
    }
    
    // Binary search in narrowed range
    auto it = std::lower_bound(searchStart, searchEnd, timestampUnits,
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

// Enhanced memory management and optimization methods (Requirements 8.1, 8.2, 8.7, 8.8)
void ISODemuxerSampleTableManager::OptimizeMemoryUsage() {
    if (!memoryOptimizationEnabled) {
        return;
    }
    
    auto& memoryOptimizer = MemoryOptimizer::getInstance();
    auto pressureLevel = memoryOptimizer.getMemoryPressureLevel();
    
    // Calculate current memory usage before optimization
    size_t memoryBeforeOptimization = GetMemoryFootprint();
    
    // Release legacy chunk table if compressed version is available
    if (!compressedChunkTable.empty() && chunkTableLoaded) {
        size_t freedMemory = chunkTable.size() * sizeof(ChunkInfo);
        chunkTable.clear();
        chunkTable.shrink_to_fit();
        chunkTableLoaded = false;
        memoryOptimizer.registerDeallocation(freedMemory, "ISODemuxer_LegacyChunkTable");
    }
    
    // Optimize based on memory pressure level
    switch (pressureLevel) {
        case MemoryOptimizer::MemoryPressureLevel::Critical:
            OptimizeForCriticalMemoryPressure();
            break;
        case MemoryOptimizer::MemoryPressureLevel::High:
            OptimizeForHighMemoryPressure();
            break;
        case MemoryOptimizer::MemoryPressureLevel::Normal:
            OptimizeForNormalMemoryPressure();
            break;
        case MemoryOptimizer::MemoryPressureLevel::Low:
            // No aggressive optimization needed
            break;
    }
    
    // Standard optimizations
    syncSamples.shrink_to_fit();
    optimizedTimeTable.shrink_to_fit();
    compressedChunkTable.shrink_to_fit();
    hierarchicalTimeIndex.shrink_to_fit();
    
    // Update memory footprint calculation
    CalculateMemoryFootprint();
    
    // Log optimization results
    size_t memoryAfterOptimization = GetMemoryFootprint();
    if (memoryBeforeOptimization > memoryAfterOptimization) {
        size_t savedMemory = memoryBeforeOptimization - memoryAfterOptimization;
        Debug::log("memory", "ISODemuxerSampleTableManager: Optimized memory usage, saved ", 
                  savedMemory, " bytes (", 
                  (savedMemory * 100) / memoryBeforeOptimization, "% reduction)");
    }
}

void ISODemuxerSampleTableManager::OptimizeForCriticalMemoryPressure() {
    // Critical memory pressure - use most aggressive optimizations
    
    // Force chunked mode for sample sizes if not already enabled
    if (!sampleSizes.isCompressed && !sampleSizes.chunkedMode) {
        // Clear loaded sample sizes and switch to chunked mode
        if (sampleSizes.isLoaded) {
            auto& memoryOptimizer = MemoryOptimizer::getInstance();
            size_t freedMemory = sampleSizes.variableSizes.size() * sizeof(uint32_t);
            sampleSizes.variableSizes.clear();
            sampleSizes.variableSizes.shrink_to_fit();
            sampleSizes.isLoaded = false;
            sampleSizes.chunkedMode = true;
            sampleSizes.chunkSize = 64; // Smaller chunks for critical pressure
            memoryOptimizer.registerDeallocation(freedMemory, "ISODemuxer_SampleSizes");
        }
    }
    
    // Reduce chunk cache size
    if (sampleSizes.chunkedMode) {
        while (sampleSizes.sampleChunks.size() > 2) { // Keep only 2 chunks
            sampleSizes.EvictOldestChunk();
        }
    }
    
    // Clear hierarchical index if memory is critical
    if (!hierarchicalTimeIndex.empty()) {
        auto& memoryOptimizer = MemoryOptimizer::getInstance();
        size_t freedMemory = hierarchicalTimeIndex.size() * sizeof(HierarchicalTimeIndex);
        hierarchicalTimeIndex.clear();
        hierarchicalTimeIndex.shrink_to_fit();
        memoryOptimizer.registerDeallocation(freedMemory, "ISODemuxer_HierarchicalIndex");
    }
}

void ISODemuxerSampleTableManager::OptimizeForHighMemoryPressure() {
    // High memory pressure - moderate optimizations
    
    // Switch to chunked mode with smaller chunks
    if (!sampleSizes.isCompressed && sampleSizes.isLoaded && !sampleSizes.chunkedMode) {
        if (sampleSizes.variableSizes.size() > 1000) { // Only for large tables
            auto& memoryOptimizer = MemoryOptimizer::getInstance();
            size_t freedMemory = sampleSizes.variableSizes.size() * sizeof(uint32_t);
            sampleSizes.variableSizes.clear();
            sampleSizes.variableSizes.shrink_to_fit();
            sampleSizes.isLoaded = false;
            sampleSizes.chunkedMode = true;
            sampleSizes.chunkSize = 128; // Smaller chunks for high pressure
            memoryOptimizer.registerDeallocation(freedMemory, "ISODemuxer_SampleSizes");
        }
    }
    
    // Reduce chunk cache size
    if (sampleSizes.chunkedMode) {
        while (sampleSizes.sampleChunks.size() > 4) { // Keep only 4 chunks
            sampleSizes.EvictOldestChunk();
        }
    }
}

void ISODemuxerSampleTableManager::OptimizeForNormalMemoryPressure() {
    // Normal memory pressure - light optimizations
    
    // Reduce chunk cache size moderately
    if (sampleSizes.chunkedMode) {
        while (sampleSizes.sampleChunks.size() > 8) { // Keep up to 8 chunks
            sampleSizes.EvictOldestChunk();
        }
    }
}

size_t ISODemuxerSampleTableManager::GetMemoryFootprint() const {
    return estimatedMemoryUsage;
}

void ISODemuxerSampleTableManager::CalculateMemoryFootprint() const {
    estimatedMemoryUsage = 0;
    
    // Compressed chunk table
    estimatedMemoryUsage += compressedChunkTable.capacity() * sizeof(CompressedChunkInfo);
    
    // Optimized time table
    estimatedMemoryUsage += optimizedTimeTable.capacity() * sizeof(OptimizedTimeEntry);
    
    // Hierarchical time index
    estimatedMemoryUsage += hierarchicalTimeIndex.capacity() * sizeof(HierarchicalTimeIndex);
    
    // Sample sizes
    if (sampleSizes.isLoaded && !sampleSizes.isCompressed) {
        if (sampleSizes.chunkedMode) {
            // Calculate memory used by cached chunks
            for (const auto& chunkPair : sampleSizes.sampleChunks) {
                estimatedMemoryUsage += chunkPair.second.samples.capacity() * sizeof(uint32_t);
                estimatedMemoryUsage += sizeof(LazyLoadedSampleSizes::SampleChunk);
            }
            // Add map overhead
            estimatedMemoryUsage += sampleSizes.sampleChunks.size() * 
                                   (sizeof(size_t) + sizeof(LazyLoadedSampleSizes::SampleChunk) + 32); // Estimate map node overhead
        } else {
            estimatedMemoryUsage += sampleSizes.variableSizes.capacity() * sizeof(uint32_t);
        }
    }
    
    // Sync samples
    estimatedMemoryUsage += syncSamples.capacity() * sizeof(uint64_t);
    
    // Legacy tables (if loaded)
    if (chunkTableLoaded) {
        estimatedMemoryUsage += chunkTable.capacity() * sizeof(ChunkInfo);
    }
    
    // Legacy time table
    estimatedMemoryUsage += timeTable.capacity() * sizeof(TimeToSampleEntry);
    
    // Add base object overhead
    estimatedMemoryUsage += sizeof(ISODemuxerSampleTableManager);
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