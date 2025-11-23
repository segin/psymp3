/*
 * ISODemuxerStreamManager.cpp - Stream manager for ISO demuxer audio track management
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

void ISODemuxerStreamManager::AddAudioTrack(const AudioTrackInfo& track) {
    tracks.push_back(track);
}

std::vector<StreamInfo> ISODemuxerStreamManager::GetStreamInfos() const {
    std::vector<StreamInfo> streams;
    for (const auto& track : tracks) {
        StreamInfo info;
        info.stream_id = track.trackId;
        info.codec_type = "audio";
        info.codec_name = track.codecType;
        info.sample_rate = track.sampleRate;
        info.channels = track.channelCount;
        info.bits_per_sample = track.bitsPerSample;
        info.bitrate = track.avgBitrate;
        info.codec_data = track.codecConfig;
        
        // Calculate duration with special handling for telephony codecs
        if (track.codecType == "ulaw" || track.codecType == "alaw") {
            // For telephony codecs, use precise sample-based timing
            if (track.sampleRate > 0 && !track.sampleTableInfo.sampleTimes.empty()) {
                // Calculate total samples from sample table
                size_t totalSamples = track.sampleTableInfo.sampleTimes.size();
                info.duration_samples = totalSamples;
                info.duration_ms = (totalSamples * 1000ULL) / track.sampleRate;
            } else {
                // Fallback to timescale-based calculation
                info.duration_ms = track.timescale > 0 ? (track.duration * 1000ULL) / track.timescale : 0;
                info.duration_samples = info.sample_rate > 0 ? (info.duration_ms * info.sample_rate) / 1000ULL : 0;
            }
            
            // Validate telephony codec parameters
            if (info.sample_rate != 8000 && info.sample_rate != 16000 && info.sample_rate != 11025 && info.sample_rate != 22050) {
                // Log warning about non-standard sample rate
            }
            if (info.channels != 1) {
                // Log warning about non-mono telephony audio
            }
            if (info.bits_per_sample != 8) {
                // Log warning about non-8-bit companded audio
            }
        } else {
            // Standard duration calculation for other codecs
            info.duration_ms = track.timescale > 0 ? (track.duration * 1000ULL) / track.timescale : 0;
            info.duration_samples = info.sample_rate > 0 ? (info.duration_ms * info.sample_rate) / 1000ULL : 0;
        }
        
        streams.push_back(info);
    }
    return streams;
}

AudioTrackInfo* ISODemuxerStreamManager::GetTrack(uint32_t trackId) {
    for (auto& track : tracks) {
        if (track.trackId == trackId) {
            return &track;
        }
    }
    return nullptr;
}

std::vector<AudioTrackInfo> ISODemuxerStreamManager::GetAudioTracks() const {
    return tracks;
}
// Streaming functionality (merged from StreamingManager)

bool ISODemuxerStreamManager::isStreaming() const {
    return m_is_streaming;
}

bool ISODemuxerStreamManager::isMovieBoxAtEnd() const {
    return m_movie_box_at_end;
}

uint64_t ISODemuxerStreamManager::findMovieBox() {
    // For now, return the cached movie box offset
    // This would need to be implemented with actual box searching logic
    return m_movie_box_offset;
}

bool ISODemuxerStreamManager::isDataAvailable(uint64_t offset, size_t size) const {
    // For non-streaming sources, all data is always available
    if (!m_is_streaming) {
        return true;
    }
    
    // For streaming sources, this would need to check the actual data availability
    // For now, assume data is available (stub implementation)
    return true;
}

void ISODemuxerStreamManager::requestByteRange(uint64_t offset, size_t size) {
    // For non-streaming sources, no action needed
    if (!m_is_streaming) {
        return;
    }
    
    // For streaming sources, this would initiate a byte range request
    // Stub implementation for now
}

bool ISODemuxerStreamManager::waitForData(uint64_t offset, size_t size, uint32_t timeout_ms) {
    // For non-streaming sources, data is always immediately available
    if (!m_is_streaming) {
        return true;
    }
    
    // For streaming sources, this would wait for the data to become available
    // Stub implementation for now - assume data becomes available
    return true;
}

void ISODemuxerStreamManager::prefetchSample(uint64_t offset, size_t size) {
    // For non-streaming sources, no prefetching needed
    if (!m_is_streaming) {
        return;
    }
    
    // For streaming sources, this would initiate prefetching
    // Stub implementation for now
}
} // namespace ISO
} // namespace Demuxer
} // namespace PsyMP3
