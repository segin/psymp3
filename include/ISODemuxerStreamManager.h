/*
 * ISODemuxerStreamManager.h - Stream manager for audio track management
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef ISODEMUXERSTREAMMANAGER_H
#define ISODEMUXERSTREAMMANAGER_H

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Stream manager for audio track management
 */
class ISODemuxerStreamManager {
public:
    ISODemuxerStreamManager() = default;
    ~ISODemuxerStreamManager() = default;
    
    void AddAudioTrack(const AudioTrackInfo& track);
    std::vector<StreamInfo> GetStreamInfos() const;
    AudioTrackInfo* GetTrack(uint32_t trackId);
    std::vector<AudioTrackInfo> GetAudioTracks() const;
    
private:
    std::vector<AudioTrackInfo> tracks;
};

#endif // ISODEMUXERSTREAMMANAGER_H