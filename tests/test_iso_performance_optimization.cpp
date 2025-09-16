/*
 * test_iso_performance_optimization.cpp - Test ISO demuxer performance optimizations
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

// Test data structures
struct TestSampleTableInfo {
    std::vector<uint64_t> chunkOffsets;
    std::vector<SampleToChunkEntry> sampleToChunkEntries;
    std::vector<uint32_t> sampleSizes;
    std::vector<uint64_t> sampleTimes;
    std::vector<uint64_t> syncSamples;
};

// Create test sample table data
TestSampleTableInfo createLargeSampleTable(size_t sampleCount) {
    TestSampleTableInfo testData;
    
    // Create chunk offsets (assume 100 samples per chunk)
    size_t chunkCount = (sampleCount + 99) / 100;
    testData.chunkOffsets.reserve(chunkCount);
    for (size_t i = 0; i < chunkCount; i++) {
        testData.chunkOffsets.push_back(i * 100 * 1024); // 100KB per chunk
    }
    
    // Create sample-to-chunk entries
    SampleToChunkEntry entry;
    entry.firstChunk = 0;
    entry.samplesPerChunk = 100;
    entry.sampleDescIndex = 1;
    testData.sampleToChunkEntries.push_back(entry);
    
    // Create sample sizes (mix of fixed and variable)
    testData.sampleSizes.reserve(sampleCount);
    for (size_t i = 0; i < sampleCount; i++) {
        if (i % 10 == 0) {
            testData.sampleSizes.push_back(2048); // Larger keyframe
        } else {
            testData.sampleSizes.push_back(1024); // Regular frame
        }
    }
    
    // Create sample times (44.1kHz, ~23ms per frame)
    testData.sampleTimes.reserve(sampleCount);
    for (size_t i = 0; i < sampleCount; i++) {
        testData.sampleTimes.push_back(i * 1024); // 1024 time units per sample
    }
    
    // Create sync samples (every 10th sample is a keyframe)
    for (size_t i = 0; i < sampleCount; i += 10) {
        testData.syncSamples.push_back(i);
    }
    
    return testData;
}

// Convert test data to SampleTableInfo
SampleTableInfo convertToSampleTableInfo(const TestSampleTableInfo& testData) {
    SampleTableInfo info;
    info.chunkOffsets = testData.chunkOffsets;
    info.sampleToChunkEntries = testData.sampleToChunkEntries;
    info.sampleSizes = testData.sampleSizes;
    info.sampleTimes = testData.sampleTimes;
    info.syncSamples = testData.syncSamples;
    return info;
}

// Test lazy loading performance
void testLazyLoadingPerformance() {
    std::cout << "Testing lazy loading performance..." << std::endl;
    
    // Create large sample table (100,000 samples)
    auto testData = createLargeSampleTable(100000);
    auto sampleTableInfo = convertToSampleTableInfo(testData);
    
    // Test with lazy loading enabled
    auto sampleTableManager = std::make_unique<ISODemuxerSampleTableManager>();
    sampleTableManager->EnableLazyLoading(true);
    
    auto start = std::chrono::high_resolution_clock::now();
    bool success = sampleTableManager->BuildSampleTables(sampleTableInfo);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "  Lazy loading build time: " << duration.count() << " microseconds" << std::endl;
    std::cout << "  Build success: " << (success ? "Yes" : "No") << std::endl;
    std::cout << "  Memory footprint: " << sampleTableManager->GetMemoryFootprint() << " bytes" << std::endl;
    
    // Test sample access performance
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        auto sampleInfo = sampleTableManager->GetSampleInfo(i * 100); // Access every 100th sample
    }
    end = std::chrono::high_resolution_clock::now();
    
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "  1000 sample access time: " << duration.count() << " microseconds" << std::endl;
}

// Test compressed chunk mapping performance
void testCompressedChunkMappingPerformance() {
    std::cout << "Testing compressed chunk mapping performance..." << std::endl;
    
    // Create sample table with many chunks
    auto testData = createLargeSampleTable(50000);
    auto sampleTableInfo = convertToSampleTableInfo(testData);
    
    auto sampleTableManager = std::make_unique<ISODemuxerSampleTableManager>();
    
    auto start = std::chrono::high_resolution_clock::now();
    bool success = sampleTableManager->BuildSampleTables(sampleTableInfo);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "  Compressed mapping build time: " << duration.count() << " microseconds" << std::endl;
    std::cout << "  Build success: " << (success ? "Yes" : "No") << std::endl;
    std::cout << "  Memory footprint: " << sampleTableManager->GetMemoryFootprint() << " bytes" << std::endl;
    
    // Test chunk lookup performance
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; i++) {
        auto sampleInfo = sampleTableManager->GetSampleInfo(i * 5); // Access every 5th sample
    }
    end = std::chrono::high_resolution_clock::now();
    
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "  10000 chunk lookups time: " << duration.count() << " microseconds" << std::endl;
}

// Test binary search optimization
void testBinarySearchOptimization() {
    std::cout << "Testing binary search optimization..." << std::endl;
    
    // Create large time table
    auto testData = createLargeSampleTable(200000); // Large table to trigger hierarchical index
    auto sampleTableInfo = convertToSampleTableInfo(testData);
    
    auto sampleTableManager = std::make_unique<ISODemuxerSampleTableManager>();
    
    auto start = std::chrono::high_resolution_clock::now();
    bool success = sampleTableManager->BuildSampleTables(sampleTableInfo);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "  Time table build time: " << duration.count() << " microseconds" << std::endl;
    std::cout << "  Build success: " << (success ? "Yes" : "No") << std::endl;
    std::cout << "  Memory footprint: " << sampleTableManager->GetMemoryFootprint() << " bytes" << std::endl;
    
    // Test time-to-sample lookup performance
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; i++) {
        double timestamp = i * 0.023; // 23ms intervals
        uint64_t sampleIndex = sampleTableManager->TimeToSample(timestamp);
        (void)sampleIndex; // Suppress unused variable warning
    }
    end = std::chrono::high_resolution_clock::now();
    
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "  10000 time-to-sample lookups: " << duration.count() << " microseconds" << std::endl;
    
    // Test sample-to-time lookup performance
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; i++) {
        double timestamp = sampleTableManager->SampleToTime(i * 20);
        (void)timestamp; // Suppress unused variable warning
    }
    end = std::chrono::high_resolution_clock::now();
    
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "  10000 sample-to-time lookups: " << duration.count() << " microseconds" << std::endl;
}

// Test memory optimization
void testMemoryOptimization() {
    std::cout << "Testing memory optimization..." << std::endl;
    
    auto testData = createLargeSampleTable(75000);
    auto sampleTableInfo = convertToSampleTableInfo(testData);
    
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
}

int main() {
    std::cout << "ISO Demuxer Performance Optimization Tests" << std::endl;
    std::cout << "===========================================" << std::endl;
    
    try {
        testLazyLoadingPerformance();
        std::cout << std::endl;
        
        testCompressedChunkMappingPerformance();
        std::cout << std::endl;
        
        testBinarySearchOptimization();
        std::cout << std::endl;
        
        testMemoryOptimization();
        std::cout << std::endl;
        
        std::cout << "All performance optimization tests completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}