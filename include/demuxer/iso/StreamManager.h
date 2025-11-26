/*
 * StreamManager.h - Stream manager for audio track management
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef STREAMMANAGER_H
#define STREAMMANAGER_H

namespace PsyMP3 {
namespace Demuxer {
namespace ISO {

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Stream manager for audio track management
 */
class StreamManager {
public:
    StreamManager() = default;
    ~StreamManager() = default;
    
    // Audio track management
    void AddAudioTrack(const AudioTrackInfo& track);
    std::vector<StreamInfo> GetStreamInfos() const;
    AudioTrackInfo* GetTrack(uint32_t trackId);
    std::vector<AudioTrackInfo> GetAudioTracks() const;
    
    // Streaming functionality (merged from StreamingManager)
    bool isStreaming() const;
    bool isMovieBoxAtEnd() const;
    uint64_t findMovieBox();
    bool isDataAvailable(uint64_t offset, size_t size) const;
    void requestByteRange(uint64_t offset, size_t size);
    bool waitForData(uint64_t offset, size_t size, uint32_t timeout_ms);
    void prefetchSample(uint64_t offset, size_t size);
    
private:
    std::vector<AudioTrackInfo> tracks;
    
    // Streaming state
    bool m_is_streaming = false;
    bool m_movie_box_at_end = false;
    uint64_t m_movie_box_offset = 0;
};


} // namespace ISO
} // namespace Demuxer
} // namespace PsyMP3
#endif // STREAMMANAGER_H