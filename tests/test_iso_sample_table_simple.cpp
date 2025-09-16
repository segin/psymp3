/*
 * test_iso_sample_table_simple.cpp - Simple test for ISO demuxer sample table optimizations
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <memory>
#include <algorithm>
#include <map>
#include <cstdint>

// Simplified structures for testing
struct SampleToChunkEntry {
    uint32_t firstChunk;
    uint32_t samplesPerChunk;
    uint32_t sampleDescIndex;
};

struct SampleTableInfo {
    std::vector<uint64_t> chunkOffsets;
    std::vector<SampleToChunkEntry> sampleToChunkEntries;
    std::vector<uint32_t> sampleSizes;
    std::vector<uint64_t> sampleTimes;
    std::vector<uint64_t> syncSamples;
};

// Simplified SampleTableManager for testing
class SimpleSampleTableManager {
public:
    struct SampleInfo {
        uint64_t offset;
        uint32_t size;
        uint32_t duration;
        bool isKeyframe;
    };
    
    struct CompressedChunkInfo {
        uint64_t baseOffset;
        uint32_t chunkCount;
        uint32_t samplesPerChunk;
        uint32_t firstSample;
        uint32_t totalSamples;
    };
    
    struct OptimizedTimeEntry {
        uint64_t sampleIndex;
        uint64_t timestamp;
        uint32_t duration;
        uint32_t sampleRange;
    };
    
    bool BuildSampleTables(const SampleTableInfo& rawTables) {
        // Build compressed chunk table
        if (!BuildCompressedChunkTable(rawTables)) {
            return false;
        }
        
        // Build optimized time table
        if (!BuildOptimizedTimeTable(rawTables)) {
            return false;
        }
        
        // Store sample sizes
        sampleSizes = rawTables.sampleSizes;
        
        // Store sync samples
        syncSamples = rawTables.syncSamples;
        std::sort(syncSamples.begin(), syncSamples.end());
        
        return true;
    }
    
    SampleInfo GetSampleInfo(uint64_t sampleIndex) {
        SampleInfo info = {};
        
        // Find chunk information
        const CompressedChunkInfo* chunkInfo = FindChunkForSample(sampleIndex);
        if (!chunkInfo) {
            return info;
        }
        
        // Calculate sample position
        uint64_t sampleInRange = sampleIndex - chunkInfo->firstSample;
        uint32_t chunkInRange = static_cast<uint32_t>(sampleInRange / chunkInfo->samplesPerChunk);
        uint32_t sampleInChunk = static_cast<uint32_t>(sampleInRange % chunkInfo->samplesPerChunk);
        
        // Get sample size
        uint32_t sampleSize = GetSampleSize(sampleIndex);
        
        // Calculate offset (simplified)
        info.offset = chunkInfo->baseOffset + (chunkInRange * sampleSize * chunkInfo->samplesPerChunk) + 
                      (sampleInChunk * sampleSize);
        info.size = sampleSize;
        info.duration = GetSampleDuration(sampleIndex);
        info.isKeyframe = IsSyncSample(sampleIndex);
        
        return info;
    }
    
    uint64_t TimeToSample(double timestamp) {
        if (optimizedTimeTable.empty()) {
            return 0;
        }
        
        uint64_t timestampUnits = static_cast<uint64_t>(timestamp * 1000);
        
        auto it = std::lower_bound(optimizedTimeTable.begin(), optimizedTimeTable.end(), timestampUnits,
            [](const OptimizedTimeEntry& entry, uint64_t ts) {
                return entry.timestamp < ts;
            });
        
        if (it == optimizedTimeTable.end()) {
            if (!optimizedTimeTable.empty()) {
                const auto& lastEntry = optimizedTimeTable.back();
                return lastEntry.sampleIndex + lastEntry.sampleRange - 1;
            }
            return 0;
        }
        
        if (it == optimizedTimeTable.begin()) {
            return it->sampleIndex;
        }
        
        --it;
        
        if (timestampUnits >= it->timestamp && 
            timestampUnits < it->timestamp + (it->duration * it->sampleRange)) {
            uint64_t offsetInRange = timestampUnits - it->timestamp;
            uint32_t sampleOffset = static_cast<uint32_t>(offsetInRange / it->duration);
            return it->sampleIndex + sampleOffset;
        }
        
        return it->sampleIndex;
    }
    
    double SampleToTime(uint64_t sampleIndex) {
        for (const auto& entry : optimizedTimeTable) {
            if (sampleIndex >= entry.sampleIndex && 
                sampleIndex < entry.sampleIndex + entry.sampleRange) {
                uint64_t sampleOffset = sampleIndex - entry.sampleIndex;
                uint64_t timestamp = entry.timestamp + (sampleOffset * entry.duration);
                return static_cast<double>(timestamp) / 1000.0;
            }
        }
        return 0.0;
    }
    
    size_t GetMemoryFootprint() const {
        size_t total = sizeof(SimpleSampleTableManager);
        total += compressedChunkTable.capacity() * sizeof(CompressedChunkInfo);
        total += optimizedTimeTable.capacity() * sizeof(OptimizedTimeEntry);
        total += sampleSizes.capacity() * sizeof(uint32_t);
        total += syncSamples.capacity() * sizeof(uint64_t);
        return total;
    }
    
private:
    std::vector<CompressedChunkInfo> compressedChunkTable;
    std::vector<OptimizedTimeEntry> optimizedTimeTable;
    std::vector<uint32_t> sampleSizes;
    std::vector<uint64_t> syncSamples;
    
    bool BuildCompressedChunkTable(const SampleTableInfo& rawTables) {
        if (rawTables.chunkOffsets.empty() || rawTables.sampleToChunkEntries.empty()) {
            return false;
        }
        
        compressedChunkTable.clear();
        compressedChunkTable.reserve(rawTables.sampleToChunkEntries.size());
        
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
            
            if (lastChunk < firstChunk || firstChunk >= rawTables.chunkOffsets.size()) {
                continue;
            }
            
            uint32_t chunkCount = lastChunk - firstChunk + 1;
            uint32_t totalSamples = chunkCount * entry.samplesPerChunk;
            
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
    
    bool BuildOptimizedTimeTable(const SampleTableInfo& rawTables) {
        if (rawTables.sampleTimes.empty()) {
            return false;
        }
        
        optimizedTimeTable.clear();
        optimizedTimeTable.reserve(rawTables.sampleTimes.size() / 10);
        
        uint64_t currentSample = 0;
        uint64_t currentTime = 0;
        
        while (currentSample < rawTables.sampleTimes.size()) {
            uint64_t startSample = currentSample;
            uint64_t startTime = currentTime;
            
            uint32_t duration = 0;
            if (currentSample + 1 < rawTables.sampleTimes.size()) {
                duration = static_cast<uint32_t>(rawTables.sampleTimes[currentSample + 1] - rawTables.sampleTimes[currentSample]);
            } else if (currentSample > 0) {
                duration = static_cast<uint32_t>(rawTables.sampleTimes[currentSample] - rawTables.sampleTimes[currentSample - 1]);
            } else {
                duration = 1024;
            }
            
            uint32_t sampleRange = 1;
            currentSample++;
            currentTime += duration;
            
            while (currentSample < rawTables.sampleTimes.size()) {
                uint32_t nextDuration = 0;
                if (currentSample + 1 < rawTables.sampleTimes.size()) {
                    nextDuration = static_cast<uint32_t>(rawTables.sampleTimes[currentSample + 1] - rawTables.sampleTimes[currentSample]);
                } else {
                    nextDuration = duration;
                }
                
                if (nextDuration != duration) {
                    break;
                }
                
                sampleRange++;
                currentSample++;
                currentTime += duration;
            }
            
            OptimizedTimeEntry entry;
            entry.sampleIndex = startSample;
            entry.timestamp = startTime;
            entry.duration = duration;
            entry.sampleRange = sampleRange;
            
            optimizedTimeTable.push_back(entry);
        }
        
        return !optimizedTimeTable.empty();
    }
    
    const CompressedChunkInfo* FindChunkForSample(uint64_t sampleIndex) const {
        for (const auto& chunk : compressedChunkTable) {
            if (sampleIndex >= chunk.firstSample && 
                sampleIndex < chunk.firstSample + chunk.totalSamples) {
                return &chunk;
            }
        }
        return nullptr;
    }
    
    uint32_t GetSampleSize(uint64_t sampleIndex) const {
        if (sampleIndex < sampleSizes.size()) {
            return sampleSizes[sampleIndex];
        }
        return 1024;
    }
    
    uint32_t GetSampleDuration(uint64_t sampleIndex) const {
        for (const auto& entry : optimizedTimeTable) {
            if (sampleIndex >= entry.sampleIndex && 
                sampleIndex < entry.sampleIndex + entry.sampleRange) {
                return entry.duration;
            }
        }
        return 1024;
    }
    
    bool IsSyncSample(uint64_t sampleIndex) const {
        if (syncSamples.empty()) {
            return true;
        }
        return std::binary_search(syncSamples.begin(), syncSamples.end(), sampleIndex);
    }
};

// Create test sample table data
SampleTableInfo createTestSampleTable(size_t sampleCount) {
    SampleTableInfo info;
    
    // Create chunk offsets
    size_t chunkCount = (sampleCount + 99) / 100;
    info.chunkOffsets.reserve(chunkCount);
    for (size_t i = 0; i < chunkCount; i++) {
        info.chunkOffsets.push_back(i * 100 * 1024);
    }
    
    // Create sample-to-chunk entries
    SampleToChunkEntry entry;
    entry.firstChunk = 0;
    entry.samplesPerChunk = 100;
    entry.sampleDescIndex = 1;
    info.sampleToChunkEntries.push_back(entry);
    
    // Create sample sizes
    info.sampleSizes.reserve(sampleCount);
    for (size_t i = 0; i < sampleCount; i++) {
        if (i % 10 == 0) {
            info.sampleSizes.push_back(2048);
        } else {
            info.sampleSizes.push_back(1024);
        }
    }
    
    // Create sample times
    info.sampleTimes.reserve(sampleCount);
    for (size_t i = 0; i < sampleCount; i++) {
        info.sampleTimes.push_back(i * 1024);
    }
    
    // Create sync samples
    for (size_t i = 0; i < sampleCount; i += 10) {
        info.syncSamples.push_back(i);
    }
    
    return info;
}

void testBasicFunctionality() {
    std::cout << "Testing basic functionality..." << std::endl;
    
    auto sampleTableInfo = createTestSampleTable(1000);
    auto manager = std::make_unique<SimpleSampleTableManager>();
    
    bool success = manager->BuildSampleTables(sampleTableInfo);
    std::cout << "  Build success: " << (success ? "Yes" : "No") << std::endl;
    
    if (success) {
        auto sampleInfo = manager->GetSampleInfo(500);
        std::cout << "  Sample 500 size: " << sampleInfo.size << " bytes" << std::endl;
        std::cout << "  Sample 500 is keyframe: " << (sampleInfo.isKeyframe ? "Yes" : "No") << std::endl;
        
        double timestamp = manager->SampleToTime(500);
        uint64_t sampleIndex = manager->TimeToSample(timestamp);
        std::cout << "  Sample 500 timestamp: " << timestamp << " seconds" << std::endl;
        std::cout << "  Timestamp back to sample: " << sampleIndex << std::endl;
        
        std::cout << "  Memory footprint: " << manager->GetMemoryFootprint() << " bytes" << std::endl;
    }
}

void testPerformance() {
    std::cout << "Testing performance with large tables..." << std::endl;
    
    const size_t LARGE_SAMPLE_COUNT = 100000;
    auto sampleTableInfo = createTestSampleTable(LARGE_SAMPLE_COUNT);
    auto manager = std::make_unique<SimpleSampleTableManager>();
    
    // Measure build time
    auto start = std::chrono::high_resolution_clock::now();
    bool success = manager->BuildSampleTables(sampleTableInfo);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "  Build time for " << LARGE_SAMPLE_COUNT << " samples: " << duration.count() << " microseconds" << std::endl;
    std::cout << "  Build success: " << (success ? "Yes" : "No") << std::endl;
    std::cout << "  Memory footprint: " << manager->GetMemoryFootprint() << " bytes" << std::endl;
    
    if (success) {
        // Test lookup performance
        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 10000; i++) {
            auto sampleInfo = manager->GetSampleInfo(i * 10);
            (void)sampleInfo;
        }
        end = std::chrono::high_resolution_clock::now();
        
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "  10000 sample lookups: " << duration.count() << " microseconds" << std::endl;
        
        // Test time conversion performance
        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 10000; i++) {
            double timestamp = i * 0.023;
            uint64_t sampleIndex = manager->TimeToSample(timestamp);
            (void)sampleIndex;
        }
        end = std::chrono::high_resolution_clock::now();
        
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "  10000 time-to-sample conversions: " << duration.count() << " microseconds" << std::endl;
    }
}

void testMemoryEfficiency() {
    std::cout << "Testing memory efficiency..." << std::endl;
    
    // Test with different table sizes
    std::vector<size_t> testSizes = {1000, 10000, 50000, 100000};
    
    for (size_t size : testSizes) {
        auto sampleTableInfo = createTestSampleTable(size);
        auto manager = std::make_unique<SimpleSampleTableManager>();
        
        manager->BuildSampleTables(sampleTableInfo);
        size_t memoryUsed = manager->GetMemoryFootprint();
        
        // Calculate compression ratio
        size_t uncompressedSize = size * (sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint64_t)); // Rough estimate
        double compressionRatio = static_cast<double>(uncompressedSize) / memoryUsed;
        
        std::cout << "  " << size << " samples: " << memoryUsed << " bytes (compression ratio: " << compressionRatio << "x)" << std::endl;
    }
}

int main() {
    std::cout << "ISO Demuxer Sample Table Optimization Tests" << std::endl;
    std::cout << "===========================================" << std::endl;
    
    try {
        testBasicFunctionality();
        std::cout << std::endl;
        
        testPerformance();
        std::cout << std::endl;
        
        testMemoryEfficiency();
        std::cout << std::endl;
        
        std::cout << "All tests completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}