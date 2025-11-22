/*
 * ISODemuxerSeekingEngine.h - Seeking engine for sample-accurate positioning
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef ISODEMUXERSEEKINGENGINE_H
#define ISODEMUXERSEEKINGENGINE_H

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Seeking engine for sample-accurate positioning
 */
class ISODemuxerSeekingEngine {
public:
    ISODemuxerSeekingEngine() = default;
    ~ISODemuxerSeekingEngine() = default;
    
    bool SeekToTimestamp(double timestamp, AudioTrackInfo& track, ISODemuxerSampleTableManager& sampleTables);
    
private:
    uint64_t BinarySearchTimeToSample(double timestamp, const std::vector<ISODemuxerSampleTableManager::SampleInfo>& samples);
    uint64_t FindNearestSyncSample(uint64_t targetSampleIndex, ISODemuxerSampleTableManager& sampleTables);
    bool ValidateSeekPosition(uint64_t sampleIndex, const AudioTrackInfo& track, ISODemuxerSampleTableManager& sampleTables);
};

#endif // ISODEMUXERSEEKINGENGINE_H