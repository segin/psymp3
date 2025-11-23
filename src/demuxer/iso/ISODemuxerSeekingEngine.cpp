/*
 * ISODemuxerSeekingEngine.cpp - Seeking engine for ISO demuxer sample-accurate positioning
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
namespace PsyMP3 {
namespace Demuxer {
namespace ISO {

bool ISODemuxerSeekingEngine::SeekToTimestamp(double timestamp, AudioTrackInfo& track, ISODemuxerSampleTableManager& sampleTables) {
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

uint64_t ISODemuxerSeekingEngine::BinarySearchTimeToSample(double timestamp, const std::vector<ISODemuxerSampleTableManager::SampleInfo>& samples) {
    if (samples.empty()) {
        return 0;
    }
    
    // Convert timestamp to comparable units (assuming samples have timestamp info)
    // This is a simplified binary search - in practice, we'd use the time table
    size_t left = 0;
    size_t right = samples.size() - 1;
    
    while (left <= right) {
        size_t mid = left + (right - left) / 2;
        
        // For this implementation, we'll use sample index as a proxy for time
        // In a real implementation, we'd compare against actual timestamps
        double sampleTime = static_cast<double>(mid) / samples.size();
        
        if (sampleTime <= timestamp) {
            if (mid == samples.size() - 1 || static_cast<double>(mid + 1) / samples.size() > timestamp) {
                return mid;
            }
            left = mid + 1;
        } else {
            if (mid == 0) break;
            right = mid - 1;
        }
    }
    
    return left;
}

uint64_t ISODemuxerSeekingEngine::FindNearestSyncSample(uint64_t targetSampleIndex, ISODemuxerSampleTableManager& sampleTables) {
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

bool ISODemuxerSeekingEngine::ValidateSeekPosition(uint64_t sampleIndex, const AudioTrackInfo& track, ISODemuxerSampleTableManager& sampleTables) {
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
