/*
 * test_fragment_handler.cpp - Test FragmentHandler implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include <stdio.h>

// Use namespace aliases for cleaner code
using namespace PsyMP3::Demuxer::ISO;

// Test FragmentHandler implementation
bool test_fragment_handler() {
    FragmentHandler handler;
    
    // Initially should not be fragmented
    if (handler.IsFragmented()) {
        printf("ERROR: Handler should not be fragmented initially\n");
        return false;
    }
    
    // Create a mock fragment
    MovieFragmentInfo fragment;
    fragment.sequenceNumber = 1;
    fragment.moofOffset = 1000;
    fragment.mdatOffset = 2000;
    fragment.isComplete = true;
    
    // Create a mock track fragment
    TrackFragmentInfo traf;
    traf.trackId = 1;
    traf.baseDataOffset = 2000;
    traf.defaultSampleDuration = 1024;
    traf.defaultSampleSize = 512;
    
    // Create a mock track run
    TrackFragmentInfo::TrackRunInfo trun;
    trun.sampleCount = 10;
    trun.dataOffset = 0;
    
    // Add sample durations and sizes
    for (uint32_t i = 0; i < 10; i++) {
        trun.sampleDurations.push_back(1024);
        trun.sampleSizes.push_back(512);
    }
    
    traf.trackRuns.push_back(trun);
    fragment.trackFragments.push_back(traf);
    
    // Add fragment to handler
    if (!handler.AddFragment(fragment)) {
        printf("ERROR: Failed to add fragment\n");
        return false;
    }
    
    // Now handler should be fragmented
    if (!handler.IsFragmented()) {
        printf("ERROR: Handler should be fragmented after adding fragment\n");
        return false;
    }
    
    // Should have one fragment
    if (handler.GetFragmentCount() != 1) {
        printf("ERROR: Expected 1 fragment, got %u\n", handler.GetFragmentCount());
        return false;
    }
    
    // Should be able to get current fragment
    MovieFragmentInfo* currentFragment = handler.GetCurrentFragment();
    if (!currentFragment) {
        printf("ERROR: Failed to get current fragment\n");
        return false;
    }
    
    // Current fragment should match what we added
    if (currentFragment->sequenceNumber != 1) {
        printf("ERROR: Expected sequence number 1, got %u\n", currentFragment->sequenceNumber);
        return false;
    }
    
    // Test fragment ordering
    MovieFragmentInfo fragment2;
    fragment2.sequenceNumber = 2;
    fragment2.isComplete = true;
    
    TrackFragmentInfo traf2;
    traf2.trackId = 1;
    traf2.trackRuns.push_back(trun);
    fragment2.trackFragments.push_back(traf2);
    
    MovieFragmentInfo fragment3;
    fragment3.sequenceNumber = 3;
    fragment3.isComplete = true;
    
    TrackFragmentInfo traf3;
    traf3.trackId = 1;
    traf3.trackRuns.push_back(trun);
    fragment3.trackFragments.push_back(traf3);
    
    // Add fragments out of order
    handler.AddFragment(fragment3);
    handler.AddFragment(fragment2);
    
    // Should have three fragments
    if (handler.GetFragmentCount() != 3) {
        printf("ERROR: Expected 3 fragments, got %u\n", handler.GetFragmentCount());
        return false;
    }
    
    // Test seeking to fragments
    if (!handler.SeekToFragment(2)) {
        printf("ERROR: Failed to seek to fragment 2\n");
        return false;
    }
    
    currentFragment = handler.GetCurrentFragment();
    if (!currentFragment || currentFragment->sequenceNumber != 2) {
        printf("ERROR: Current fragment should be sequence 2\n");
        return false;
    }
    
    if (!handler.SeekToFragment(3)) {
        printf("ERROR: Failed to seek to fragment 3\n");
        return false;
    }
    
    currentFragment = handler.GetCurrentFragment();
    if (!currentFragment || currentFragment->sequenceNumber != 3) {
        printf("ERROR: Current fragment should be sequence 3\n");
        return false;
    }
    
    // Test getting fragment by sequence number
    MovieFragmentInfo* fragment1 = handler.GetFragment(1);
    if (!fragment1 || fragment1->sequenceNumber != 1) {
        printf("ERROR: Failed to get fragment 1\n");
        return false;
    }
    
    // Test fragment completion status
    if (!handler.IsFragmentComplete(1)) {
        printf("ERROR: Fragment 1 should be complete\n");
        return false;
    }
    
    // Test default values
    AudioTrackInfo track;
    track.sampleRate = 48000;
    track.sampleTableInfo.sampleSizes.push_back(1024);
    
    handler.SetDefaultValues(track);
    
    // Test sample extraction
    uint64_t offset;
    uint32_t size;
    
    // Seek back to fragment 1
    handler.SeekToFragment(1);
    
    // Extract first sample
    if (!handler.ExtractFragmentSample(1, 0, offset, size)) {
        printf("ERROR: Failed to extract fragment sample\n");
        return false;
    }
    
    // Offset should be base data offset
    if (offset != 2000) {
        printf("ERROR: Expected offset 2000, got %lu\n", static_cast<unsigned long>(offset));
        return false;
    }
    
    // Size should be default sample size
    if (size != 512) {
        printf("ERROR: Expected size 512, got %u\n", size);
        return false;
    }
    
    printf("FragmentHandler tests passed\n");
    return true;
}

// Test sample table updates from fragments
bool test_sample_table_updates() {
    FragmentHandler handler;
    
    // Create a mock audio track
    AudioTrackInfo track;
    track.trackId = 1;
    track.sampleRate = 48000;
    track.timescale = 48000;
    
    // Create a mock fragment
    MovieFragmentInfo fragment;
    fragment.sequenceNumber = 1;
    fragment.moofOffset = 1000;
    fragment.mdatOffset = 2000;
    fragment.isComplete = true;
    
    // Create a mock track fragment
    TrackFragmentInfo traf;
    traf.trackId = 1;
    traf.baseDataOffset = 2000;
    traf.defaultSampleDuration = 1024;
    traf.defaultSampleSize = 512;
    traf.tfdt = 0; // Start at time 0
    
    // Create a mock track run
    TrackFragmentInfo::TrackRunInfo trun;
    trun.sampleCount = 10;
    trun.dataOffset = 0;
    
    // Add sample durations and sizes
    for (uint32_t i = 0; i < 10; i++) {
        trun.sampleDurations.push_back(1024);
        trun.sampleSizes.push_back(512);
    }
    
    traf.trackRuns.push_back(trun);
    fragment.trackFragments.push_back(traf);
    
    // Add fragment to handler
    handler.AddFragment(fragment);
    
    // Update sample tables
    if (!handler.UpdateSampleTables(traf, track)) {
        printf("ERROR: Failed to update sample tables\n");
        return false;
    }
    
    // Verify sample table updates
    if (track.sampleTableInfo.sampleSizes.size() != 10) {
        printf("ERROR: Expected 10 sample sizes, got %zu\n", track.sampleTableInfo.sampleSizes.size());
        return false;
    }
    
    if (track.sampleTableInfo.sampleTimes.size() != 10) {
        printf("ERROR: Expected 10 sample times, got %zu\n", track.sampleTableInfo.sampleTimes.size());
        return false;
    }
    
    if (track.sampleTableInfo.chunkOffsets.size() != 1) {
        printf("ERROR: Expected 1 chunk offset, got %zu\n", track.sampleTableInfo.chunkOffsets.size());
        return false;
    }
    
    if (track.sampleTableInfo.chunkOffsets[0] != 2000) {
        printf("ERROR: Expected chunk offset 2000, got %lu\n", 
               static_cast<unsigned long>(track.sampleTableInfo.chunkOffsets[0]));
        return false;
    }
    
    // Create a second fragment
    MovieFragmentInfo fragment2;
    fragment2.sequenceNumber = 2;
    fragment2.moofOffset = 10000;
    fragment2.mdatOffset = 11000;
    fragment2.isComplete = true;
    
    TrackFragmentInfo traf2;
    traf2.trackId = 1;
    traf2.baseDataOffset = 11000;
    traf2.defaultSampleDuration = 1024;
    traf2.defaultSampleSize = 512;
    traf2.tfdt = 10240; // Start at time 10240 (10 samples * 1024)
    
    TrackFragmentInfo::TrackRunInfo trun2;
    trun2.sampleCount = 10;
    trun2.dataOffset = 0;
    
    // Add sample durations and sizes
    for (uint32_t i = 0; i < 10; i++) {
        trun2.sampleDurations.push_back(1024);
        trun2.sampleSizes.push_back(512);
    }
    
    traf2.trackRuns.push_back(trun2);
    fragment2.trackFragments.push_back(traf2);
    
    // Add second fragment
    handler.AddFragment(fragment2);
    
    // Update sample tables with second fragment
    if (!handler.UpdateSampleTables(traf2, track)) {
        printf("ERROR: Failed to update sample tables with second fragment\n");
        return false;
    }
    
    // Verify sample table updates after second fragment
    if (track.sampleTableInfo.sampleSizes.size() != 20) {
        printf("ERROR: Expected 20 sample sizes, got %zu\n", track.sampleTableInfo.sampleSizes.size());
        return false;
    }
    
    if (track.sampleTableInfo.sampleTimes.size() != 20) {
        printf("ERROR: Expected 20 sample times, got %zu\n", track.sampleTableInfo.sampleTimes.size());
        return false;
    }
    
    if (track.sampleTableInfo.chunkOffsets.size() != 2) {
        printf("ERROR: Expected 2 chunk offsets, got %zu\n", track.sampleTableInfo.chunkOffsets.size());
        return false;
    }
    
    if (track.sampleTableInfo.chunkOffsets[1] != 11000) {
        printf("ERROR: Expected chunk offset 11000, got %lu\n", 
               static_cast<unsigned long>(track.sampleTableInfo.chunkOffsets[1]));
        return false;
    }
    
    // Verify sample times are continuous
    if (track.sampleTableInfo.sampleTimes[10] != 10240) {
        printf("ERROR: Expected sample time 10240, got %lu\n", 
               static_cast<unsigned long>(track.sampleTableInfo.sampleTimes[10]));
        return false;
    }
    
    printf("Sample table update tests passed\n");
    return true;
}

int main() {
    int passed = 0;
    int total = 0;
    
    // Run tests
    total++; if (test_fragment_handler()) passed++; else printf("FAIL: test_fragment_handler\n");
    total++; if (test_sample_table_updates()) passed++; else printf("FAIL: test_sample_table_updates\n");
    
    printf("Fragment handler tests: %d/%d passed\n", passed, total);
    
    return (passed == total) ? 0 : 1;
}
