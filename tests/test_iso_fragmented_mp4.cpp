/*
 * test_iso_fragmented_mp4.cpp - Test fragmented MP4 support in ISO demuxer
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

// Test ISODemuxerFragmentHandler basic functionality
bool test_fragment_handler_creation() {
    ISODemuxerFragmentHandler handler;
    
    // Initially should not be fragmented
    if (handler.IsFragmented()) {
        return false;
    }
    
    // Should have zero fragments
    if (handler.GetFragmentCount() != 0) {
        return false;
    }
    
    return true;
}

// Test MovieFragmentInfo structure
bool test_movie_fragment_info() {
    MovieFragmentInfo fragment;
    
    // Test that structure can be created and initialized
    fragment.sequenceNumber = 1;
    fragment.isComplete = true;
    
    if (fragment.sequenceNumber != 1) {
        return false;
    }
    
    if (fragment.isComplete != true) {
        return false;
    }
    
    return true;
}

// Test TrackFragmentInfo structure
bool test_track_fragment_info() {
    TrackFragmentInfo traf;
    
    // Test that structure can be created and used
    traf.trackId = 1;
    traf.tfdt = 1000;
    
    if (traf.trackId != 1) {
        return false;
    }
    
    if (traf.tfdt != 1000) {
        return false;
    }
    
    return true;
}

// Test TrackRunInfo structure
bool test_track_run_info() {
    TrackFragmentInfo::TrackRunInfo trun;
    
    // Test that structure can be created and used
    trun.sampleCount = 10;
    trun.dataOffset = 1024;
    trun.sampleDurations.push_back(512);
    trun.sampleSizes.push_back(256);
    
    if (trun.sampleCount != 10) {
        return false;
    }
    
    if (trun.dataOffset != 1024) {
        return false;
    }
    
    if (trun.sampleDurations.size() != 1 || trun.sampleDurations[0] != 512) {
        return false;
    }
    
    if (trun.sampleSizes.size() != 1 || trun.sampleSizes[0] != 256) {
        return false;
    }
    
    return true;
}

// Test fragment validation
bool test_fragment_validation() {
    ISODemuxerFragmentHandler handler;
    
    // Create a valid fragment
    MovieFragmentInfo fragment;
    fragment.sequenceNumber = 1;
    fragment.isComplete = true;
    
    // Add a valid track fragment
    TrackFragmentInfo traf;
    traf.trackId = 1;
    traf.defaultSampleDuration = 1024;
    traf.defaultSampleSize = 512;
    
    // Add a track run
    TrackFragmentInfo::TrackRunInfo trun;
    trun.sampleCount = 10;
    trun.dataOffset = 0;
    traf.trackRuns.push_back(trun);
    
    fragment.trackFragments.push_back(traf);
    
    // This should succeed
    bool addResult = handler.AddFragment(fragment);
    if (!addResult) {
        printf("DEBUG: AddFragment failed\n");
        return false;
    }
    
    // Now handler should be fragmented
    if (!handler.IsFragmented()) {
        printf("DEBUG: Handler not fragmented after adding fragment\n");
        return false;
    }
    
    // Should have one fragment
    uint32_t fragmentCount = handler.GetFragmentCount();
    if (fragmentCount != 1) {
        printf("DEBUG: Expected 1 fragment, got %u\n", fragmentCount);
        return false;
    }
    
    return true;
}

// Test fragment ordering
bool test_fragment_ordering() {
    ISODemuxerFragmentHandler handler;
    
    // Add fragments out of order
    MovieFragmentInfo fragment3;
    fragment3.sequenceNumber = 3;
    fragment3.isComplete = true;
    
    MovieFragmentInfo fragment1;
    fragment1.sequenceNumber = 1;
    fragment1.isComplete = true;
    
    MovieFragmentInfo fragment2;
    fragment2.sequenceNumber = 2;
    fragment2.isComplete = true;
    
    // Add track fragments to make them valid
    for (auto* frag : {&fragment1, &fragment2, &fragment3}) {
        TrackFragmentInfo traf;
        traf.trackId = 1;
        frag->trackFragments.push_back(traf);
    }
    
    // Add in wrong order
    handler.AddFragment(fragment3);
    handler.AddFragment(fragment1);
    handler.AddFragment(fragment2);
    
    // Should have 3 fragments
    if (handler.GetFragmentCount() != 3) {
        return false;
    }
    
    // Should be able to seek to specific fragments
    if (!handler.SeekToFragment(1)) {
        return false;
    }
    
    if (!handler.SeekToFragment(2)) {
        return false;
    }
    
    if (!handler.SeekToFragment(3)) {
        return false;
    }
    
    // Should fail to seek to non-existent fragment
    if (handler.SeekToFragment(4)) {
        return false;
    }
    
    return true;
}

// Test default values handling
bool test_default_values() {
    ISODemuxerFragmentHandler handler;
    
    // Create a mock audio track for default values
    AudioTrackInfo track;
    track.duration = 48000; // 1 second at 48kHz
    track.sampleRate = 48000;
    track.sampleTableInfo.sampleTimes.resize(48000); // 1 second worth of samples
    track.sampleTableInfo.sampleSizes.push_back(1024); // Fixed size samples
    
    // Set default values
    handler.SetDefaultValues(track);
    
    // This should not crash and should set internal defaults
    // We can't directly test the private defaults, but we can test
    // that the method completes successfully
    return true;
}

int main() {
    int passed = 0;
    int total = 0;
    
    // Run tests
    total++; if (test_fragment_handler_creation()) passed++; else printf("FAIL: test_fragment_handler_creation\n");
    total++; if (test_movie_fragment_info()) passed++; else printf("FAIL: test_movie_fragment_info\n");
    total++; if (test_track_fragment_info()) passed++; else printf("FAIL: test_track_fragment_info\n");
    total++; if (test_track_run_info()) passed++; else printf("FAIL: test_track_run_info\n");
    total++; if (test_fragment_validation()) passed++; else printf("FAIL: test_fragment_validation\n");
    total++; if (test_fragment_ordering()) passed++; else printf("FAIL: test_fragment_ordering\n");
    total++; if (test_default_values()) passed++; else printf("FAIL: test_default_values\n");
    
    printf("Fragmented MP4 tests: %d/%d passed\n", passed, total);
    
    return (passed == total) ? 0 : 1;
}