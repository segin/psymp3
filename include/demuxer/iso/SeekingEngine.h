/*
 * SeekingEngine.h - Seeking engine for sample-accurate positioning
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef SEEKINGENGINE_H
#define SEEKINGENGINE_H

namespace PsyMP3 {
namespace Demuxer {
namespace ISO {

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Seeking engine for sample-accurate positioning
 */
class SeekingEngine {
public:
    SeekingEngine() = default;
    ~SeekingEngine() = default;
    
    bool SeekToTimestamp(double timestamp, AudioTrackInfo& track, SampleTableManager& sampleTables);
    
private:
    uint64_t BinarySearchTimeToSample(double timestamp, const std::vector<SampleTableManager::SampleInfo>& samples);
    uint64_t FindNearestSyncSample(uint64_t targetSampleIndex, SampleTableManager& sampleTables);
    bool ValidateSeekPosition(uint64_t sampleIndex, const AudioTrackInfo& track, SampleTableManager& sampleTables);
};


} // namespace ISO
} // namespace Demuxer
} // namespace PsyMP3
#endif // SEEKINGENGINE_H