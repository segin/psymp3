/*
 * ISODemuxerSeekingEngine.cpp - Seeking engine for ISO demuxer sample-accurate positioning
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
namespace PsyMP3 {
namespace Demuxer {
namespace ISO {

bool SeekingEngine::SeekToTimestamp(double timestamp, AudioTrackInfo& track, SampleTableManager& sampleTables) {
    if (timestamp < 0.0) {
        return false;
    }
    
    // Convert timestamp to sample index
    uint64_t targetSampleIndex = sampleTables.TimeToSample(timestamp);
    
    // Find nearest sync sample for keyframe-accurate seeking
    uint64_t syncSampleIndex = FindNearestSyncSample(targetSampleIndex, sampleTables);
    
    // Validate the seek position
    if (!ValidateSeekPosition(syncSampleIndex, track, sampleTables)) {
        return false;
    }
    
    // Update track's current sample index
    track.currentSampleIndex = syncSampleIndex;
    
    return true;
}

uint64_t SeekingEngine::BinarySearchTimeToSample(double timestamp, const std::vector<SampleTableManager::SampleInfo>& samples) {
    if (samples.empty()) {
        return 0;
    }
    
    // Calculate target time in milliseconds (consistent with SampleTableManager)
    uint64_t targetTimeMs = static_cast<uint64_t>(timestamp * 1000.0);
    uint64_t currentTimeMs = 0;
    
    // Linear scan to find the sample containing the target timestamp
    // Note: This is O(N) because SampleInfo only contains duration, not absolute timestamp.
    // For O(log N) seeking, SampleTableManager::TimeToSample should be used with optimized tables.
    for (size_t i = 0; i < samples.size(); ++i) {
        uint32_t duration = samples[i].duration;
        
        // If target time falls within this sample's duration
        if (targetTimeMs >= currentTimeMs && targetTimeMs < currentTimeMs + duration) {
            return i;
        }

        currentTimeMs += duration;
    }
    
    // If timestamp is beyond total duration, return the last sample
    return samples.size() - 1;
}

uint64_t SeekingEngine::FindNearestSyncSample(uint64_t targetSampleIndex, SampleTableManager& sampleTables) {
    // Get sample info to check if target is already a sync sample
    auto sampleInfo = sampleTables.GetSampleInfo(targetSampleIndex);
    
    if (sampleInfo.isKeyframe) {
        return targetSampleIndex;
    }
    
    // Search backwards for the nearest sync sample
    for (uint64_t i = targetSampleIndex; i > 0; i--) {
        auto info = sampleTables.GetSampleInfo(i - 1);
        if (info.isKeyframe) {
            return i - 1;
        }
    }
    
    // If no sync sample found, return the first sample
    return 0;
}

bool SeekingEngine::ValidateSeekPosition(uint64_t sampleIndex, const AudioTrackInfo& track, SampleTableManager& sampleTables) {
    // Get sample info to validate the position
    auto sampleInfo = sampleTables.GetSampleInfo(sampleIndex);
    
    // Check if sample exists and has valid data
    if (sampleInfo.size == 0 || sampleInfo.offset == 0) {
        return false;
    }
    
    // Additional validation could be added here
    return true;
}
} // namespace ISO
} // namespace Demuxer
} // namespace PsyMP3
