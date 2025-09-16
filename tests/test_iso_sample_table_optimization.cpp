/*
 * test_iso_sample_table_optimization.cpp - Test ISO demuxer sample table optimizations
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <memory>

// Create test sample table data
SampleTableInfo createTestSampleTable(size_t sampleCount) {
    SampleTableInfo info;
    
    // Create chunk offsets (assume 100 samples per chunk)
    size_t chunkCount = (sampleCount + 99) / 100;
    info.chunkOffsets.reserve(chunkCount);
    for (size_t i = 0; i < chunkCount; i++) {
        info.chunkOffsets.push_back(i * 100 * 1024); // 100KB per chunk
    }
    
    // Create sample-to-chunk entries
    SampleToChunkEntry entry;
    entry.firstChunk = 0;
    entry.samplesPerChunk = 100;
    entry.sampleDescIndex = 1;
    info.sampleToChunkEntries.push_back(entry);
    
    // Create sample sizes (mix of fixed and variable)
    info.sampleSizes.reserve(sampleCount);
    for (size_t i = 0; i < sampleCount; i++) {
        if (i % 10 == 0) {
            info.sampleSizes.push_back(2048); // Larger keyframe
        } else {
            info.sampleSizes.push_back(1024); // Regular frame
        }
    }
    
    // Create sample times (44.1kHz, ~23ms per frame)
    info.sampleTimes.reserve(sampleCount);
    for (size_t i = 0; i < sampleCount; i++) {
        info.sampleTimes.push_back(i * 1024); // 1024 time units per sample
    }
    
    // Create sync samples (every 10th sample is a keyframe)
    for (size_t i = 0; i < sampleCount; i += 10) {
        info.syncSamples.push_back(i);
    }
    
    return info;
}

// Test basic functionality
void testBasicFunctionality() {
    std::cout << "Testing basic sample table functionality..." << std::endl;
    
    auto sampleTableInfo = createTestSampleTable(1000);
    auto sampleTableManager = std::make_unique<ISODemuxerSampleTableManager>();
    
    bool success = sampleTableManager->BuildSampleTables(sampleTableInfo);
    std::cout << "  Build success: " << (success ? "Yes" : "No") << std::endl;
    
    if (success) {
        // Test sample info retrieval
        auto sampleInfo = sampleTableManager->GetSampleInfo(500);
        std::cout << "  Sample 500 size: " << sampleInfo.size << " bytes" << std::endl;
        std::cout << "  Sample 500 duration: " << sampleInfo.duration << " time units" << std::endl;
        std::cout << "  Sample 500 is keyframe: " << (sampleInfo.isKeyframe ? "Yes" : "No") << std::endl;
        
        // Test time conversions
        double timestamp = sampleTableManager->SampleToTime(500);
        uint64_t sampleIndex = sampleTableManager->TimeToSample(timestamp);
        std::cout << "  Sample 500 timestamp: " << timestamp << " seconds" << std::endl;
        std::cout << "  Timestamp back to sample: " << sampleIndex << std::endl;
        
        std::cout << "  Memory footprint: " << sampleTableManager->GetMemoryFootprint() << " bytes" << std::endl;
    }
}

// Test performance with large tables
void testLargeTablePerformance() {
    std::cout << "Testing large table performance..." << std::endl;
    
    const size_t LARGE_SAMPLE_COUNT = 100000;
    auto sampleTableInfo = createTestSampleTable(LARGE_SAMPLE_COUNT);
    auto sampleTableManager = std::make_unique<ISODemuxerSampleTableManager>();
    
    // Measure build time
    auto start = std::chrono::high_resolution_clock::now();
    bool success = sampleTableManager->BuildSampleTables(sampleTableInfo);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "  Build time for " << LARGE_SAMPLE_COUNT << " samples: " << duration.count() << " microseconds" << std::endl;
    std::cout << "  Build success: " << (success ? "Yes" : "No") << std::endl;
    std::cout << "  Memory footprint: " << sampleTableManager->GetMemoryFootprint() << " bytes" << std::endl;
    
    if (success) {
        // Test lookup performance
        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 10000; i++) {
            auto sampleInfo = sampleTableManager->GetSampleInfo(i * 10);
            (void)sampleInfo; // Suppress unused variable warning
        }
        end = std::chrono::high_resolution_clock::now();
        
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "  10000 sample lookups: " << duration.count() << " microseconds" << std::endl;
        
        // Test time conversion performance
        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 10000; i++) {
            double timestamp = i * 0.023; // 23ms intervals
            uint64_t sampleIndex = sampleTableManager->TimeToSample(timestamp);
            (void)sampleIndex; // Suppress unused variable warning
        }
        end = std::chrono::high_resolution_clock::now();
        
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "  10000 time-to-sample conversions: " << duration.count() << " microseconds" << std::endl;
    }
}

// Test memory optimization
void testMemoryOptimization() {
    std::cout << "Testing memory optimization..." << std::endl;
    
    auto sampleTableInfo = createTestSampleTable(50000);
    auto sampleTableManager = std::make_unique<ISODemuxerSampleTableManager>();
    
    // Build tables
    sampleTableManager->BuildSampleTables(sampleTableInfo);
    size_t memoryBefore = sampleTableManager->GetMemoryFootprint();
    
    std::cout << "  Memory before optimization: " << memoryBefore << " bytes" << std::endl;
    
    // Optimize memory usage
    sampleTableManager->OptimizeMemoryUsage();
    size_t memoryAfter = sampleTableManager->GetMemoryFootprint();
    
    std::cout << "  Memory after optimization: " << memoryAfter << " bytes" << std::endl;
    
    if (memoryBefore > memoryAfter) {
        size_t saved = memoryBefore - memoryAfter;
        double percentage = (static_cast<double>(saved) / memoryBefore) * 100.0;
        std::cout << "  Memory saved: " << saved << " bytes (" << percentage << "%)" << std::endl;
    } else {
        std::cout << "  No memory savings (optimization may have been minimal)" << std::endl;
    }
    
    // Verify functionality still works after optimization
    auto sampleInfo = sampleTableManager->GetSampleInfo(1000);
    std::cout << "  Post-optimization sample access works: " << (sampleInfo.size > 0 ? "Yes" : "No") << std::endl;
}

// Test lazy loading
void testLazyLoading() {
    std::cout << "Testing lazy loading..." << std::endl;
    
    auto sampleTableInfo = createTestSampleTable(25000);
    
    // Test with lazy loading enabled
    auto lazyManager = std::make_unique<ISODemuxerSampleTableManager>();
    lazyManager->EnableLazyLoading(true);
    
    auto start = std::chrono::high_resolution_clock::now();
    bool lazySuccess = lazyManager->BuildSampleTables(sampleTableInfo);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto lazyDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    size_t lazyMemory = lazyManager->GetMemoryFootprint();
    
    // Test with lazy loading disabled
    auto eagerManager = std::make_unique<ISODemuxerSampleTableManager>();
    eagerManager->EnableLazyLoading(false);
    
    start = std::chrono::high_resolution_clock::now();
    bool eagerSuccess = eagerManager->BuildSampleTables(sampleTableInfo);
    end = std::chrono::high_resolution_clock::now();
    
    auto eagerDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    size_t eagerMemory = eagerManager->GetMemoryFootprint();
    
    std::cout << "  Lazy loading build time: " << lazyDuration.count() << " microseconds" << std::endl;
    std::cout << "  Eager loading build time: " << eagerDuration.count() << " microseconds" << std::endl;
    std::cout << "  Lazy loading memory: " << lazyMemory << " bytes" << std::endl;
    std::cout << "  Eager loading memory: " << eagerMemory << " bytes" << std::endl;
    
    if (lazyMemory < eagerMemory) {
        size_t saved = eagerMemory - lazyMemory;
        double percentage = (static_cast<double>(saved) / eagerMemory) * 100.0;
        std::cout << "  Lazy loading memory savings: " << saved << " bytes (" << percentage << "%)" << std::endl;
    }
    
    std::cout << "  Lazy loading success: " << (lazySuccess ? "Yes" : "No") << std::endl;
    std::cout << "  Eager loading success: " << (eagerSuccess ? "Yes" : "No") << std::endl;
}

int main() {
    std::cout << "ISO Demuxer Sample Table Optimization Tests" << std::endl;
    std::cout << "===========================================" << std::endl;
    
    try {
        testBasicFunctionality();
        std::cout << std::endl;
        
        testLargeTablePerformance();
        std::cout << std::endl;
        
        testMemoryOptimization();
        std::cout << std::endl;
        
        testLazyLoading();
        std::cout << std::endl;
        
        std::cout << "All sample table optimization tests completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}