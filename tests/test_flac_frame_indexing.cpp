/*
 * test_flac_frame_indexing.cpp - Test FLAC frame indexing for efficient seeking
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

/*
 * @TEST_METADATA_BEGIN
 * @TEST_NAME: FLAC Frame Indexing Tests
 * @TEST_DESCRIPTION: Tests frame indexing functionality for efficient seeking in FLAC streams
 * @TEST_REQUIREMENTS: 4.1, 4.2, 4.3, 4.8
 * @TEST_AUTHOR: Kirn Gill <segin2005@gmail.com>
 * @TEST_CREATED: 2025-01-27
 * @TEST_TIMEOUT: 5000
 * @TEST_PARALLEL_SAFE: true
 * @TEST_DEPENDENCIES: FLACDemuxer.o, IOHandler.o, FileIOHandler.o
 * @TEST_TAGS: flac, indexing, seeking, performance
 * @TEST_METADATA_END
 */

#include "test_framework.h"
#include "../include/psymp3.h"
#include <iostream>

using namespace TestFramework;

/**
 * Test frame indexing functionality
 */
void test_frame_indexing_basic()
{
    // Create a simple FLAC demuxer with frame indexing enabled
    auto handler = std::make_unique<FileIOHandler>("simple_test.txt");
    ASSERT_NOT_NULL(handler.get(), "Failed to create test IOHandler");
    
    FLACDemuxer demuxer(std::move(handler));
    
    // Verify frame indexing is enabled by default
    ASSERT_TRUE(demuxer.isFrameIndexingEnabled(), "Frame indexing should be enabled by default");
    
    // Test enabling/disabling frame indexing
    demuxer.setFrameIndexingEnabled(false);
    ASSERT_FALSE(demuxer.isFrameIndexingEnabled(), "Frame indexing should be disabled after setFrameIndexingEnabled(false)");
    
    demuxer.setFrameIndexingEnabled(true);
    ASSERT_TRUE(demuxer.isFrameIndexingEnabled(), "Frame indexing should be enabled after setFrameIndexingEnabled(true)");
    
    // Get initial frame index stats
    auto stats = demuxer.getFrameIndexStats();
    ASSERT_EQUALS(0u, stats.entry_count, "Initial frame index should be empty");
}

/**
 * Test FLACFrameIndex class functionality
 */
void test_flac_frame_index_class()
{
    FLACFrameIndex index;
    
    // Test empty index
    ASSERT_TRUE(index.empty(), "New index should be empty");
    ASSERT_EQUALS(0u, index.size(), "New index should have size 0");
    
    // Add some test entries with proper granularity spacing (44100 samples = 1 second at 44.1kHz)
    FLACFrameIndexEntry entry1(0, 1000, 4096, 256);         // Sample 0, offset 1000, 4096 samples, 256 bytes
    FLACFrameIndexEntry entry2(44100, 1256, 4096, 240);     // Sample 44100, offset 1256, 4096 samples, 240 bytes
    FLACFrameIndexEntry entry3(88200, 1496, 4096, 248);     // Sample 88200, offset 1496, 4096 samples, 248 bytes
    
    ASSERT_TRUE(index.addFrame(entry1), "Failed to add first frame to index");
    ASSERT_TRUE(index.addFrame(entry2), "Failed to add second frame to index");
    ASSERT_TRUE(index.addFrame(entry3), "Failed to add third frame to index");
    
    // Test index size
    ASSERT_EQUALS(3u, index.size(), "Index should have 3 entries");
    ASSERT_FALSE(index.empty(), "Index should not be empty after adding entries");
    
    // Test finding best entry
    const FLACFrameIndexEntry* best = index.findBestEntry(50000);  // Target sample 50000
    ASSERT_NOT_NULL(best, "Should find best entry for sample 50000");
    ASSERT_EQUALS(44100u, best->sample_offset, "Best entry for sample 50000 should be at sample 44100");
    
    // Test finding containing entry
    const FLACFrameIndexEntry* containing = index.findContainingEntry(45000);  // Sample 45000 is in frame starting at 44100
    ASSERT_NOT_NULL(containing, "Should find containing entry for sample 45000");
    ASSERT_EQUALS(44100u, containing->sample_offset, "Sample 45000 should be contained in frame starting at sample 44100");
    
    // Test finding entry for sample at exact frame boundary
    containing = index.findContainingEntry(44100);  // Exact start of second frame
    ASSERT_NOT_NULL(containing, "Should find containing entry for sample 44100");
    ASSERT_EQUALS(44100u, containing->sample_offset, "Sample 44100 should be contained in frame starting at sample 44100");
    
    // Test finding entry for sample not in any frame
    containing = index.findContainingEntry(20000);  // Beyond all frames
    ASSERT_NULL(containing, "Sample 20000 should not be contained in any frame");
    
    // Test index statistics
    auto stats = index.getStats();
    ASSERT_EQUALS(3u, stats.entry_count, "Stats should show 3 entries");
    ASSERT_EQUALS(0u, stats.first_sample, "Stats should show first sample as 0");
    ASSERT_EQUALS(92296u, stats.last_sample, "Stats should show last sample as 92296");  // 88200 + 4096
    
    // Test clearing index
    index.clear();
    ASSERT_TRUE(index.empty(), "Index should be empty after clear()");
    ASSERT_EQUALS(0u, index.size(), "Index should have size 0 after clear()");
}

/**
 * Test frame index granularity and memory limits
 */
void test_frame_index_limits()
{
    FLACFrameIndex index;
    
    // Test granularity - add entries that are too close together
    FLACFrameIndexEntry entry1(0, 1000, 1024, 100);
    FLACFrameIndexEntry entry2(1024, 1100, 1024, 100);  // Only 1024 samples apart (less than granularity)
    
    ASSERT_TRUE(index.addFrame(entry1), "Failed to add first frame");
    
    // Second entry should be rejected due to granularity
    ASSERT_FALSE(index.addFrame(entry2), "Second frame should be rejected due to granularity");
    ASSERT_EQUALS(1u, index.size(), "Index should still have only 1 entry after granularity rejection");
    
    // Add an entry that meets granularity requirements
    FLACFrameIndexEntry entry3(44100, 10000, 4096, 256);  // 44100 samples apart (meets granularity)
    
    ASSERT_TRUE(index.addFrame(entry3), "Third frame should be accepted (meets granularity)");
    ASSERT_EQUALS(2u, index.size(), "Index should have 2 entries after adding valid entry");
    
    // Test memory usage reporting
    size_t memory_usage = index.getMemoryUsage();
    ASSERT_TRUE(memory_usage > 0, "Memory usage should be non-zero for non-empty index");
}

/**
 * Test seeking strategy priority with frame indexing
 */
void test_seeking_strategy_priority()
{
    // This test verifies that frame indexing is preferred over other seeking methods
    // We can't easily test with real FLAC files in unit tests, but we can verify
    // the API behavior and method availability
    
    auto handler = std::make_unique<FileIOHandler>("simple_test.txt");
    ASSERT_NOT_NULL(handler.get(), "Failed to create test IOHandler");
    
    FLACDemuxer demuxer(std::move(handler));
    
    // Verify frame indexing is enabled
    ASSERT_TRUE(demuxer.isFrameIndexingEnabled(), "Frame indexing should be enabled by default");
    
    // Test building frame index (should not crash even with invalid file)
    bool build_result = demuxer.buildFrameIndex();
    // Result may be false due to invalid test file, but method should not crash
    
    // Get frame index stats
    auto stats = demuxer.getFrameIndexStats();
    // Stats should be valid even if empty
}

int main()
{
    std::cout << "Running FLAC Frame Indexing Tests..." << std::endl;
    
    try {
        test_frame_indexing_basic();
        std::cout << "✓ Frame indexing basic functionality test passed" << std::endl;
        
        test_flac_frame_index_class();
        std::cout << "✓ FLACFrameIndex class functionality test passed" << std::endl;
        
        test_frame_index_limits();
        std::cout << "✓ Frame index limits and granularity test passed" << std::endl;
        
        test_seeking_strategy_priority();
        std::cout << "✓ Seeking strategy priority test passed" << std::endl;
        
        std::cout << "All FLAC Frame Indexing tests passed!" << std::endl;
        return 0;
        
    } catch (const AssertionFailure& e) {
        std::cout << "✗ Test failed: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cout << "✗ Unexpected exception: " << e.what() << std::endl;
        return 1;
    }
}