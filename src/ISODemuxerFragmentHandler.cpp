/*
 * FragmentHandler.cpp - Fragmented MP4 support for ISO demuxer
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

// FragmentHandler implementation

bool ISODemuxerFragmentHandler::ProcessMovieFragment(uint64_t moofOffset, std::shared_ptr<IOHandler> io) {
    if (!io) {
        return false;
    }
    
    // Create a new fragment info structure
    MovieFragmentInfo fragment;
    fragment.moofOffset = moofOffset;
    
    // Parse the movie fragment box
    if (!ParseMovieFragmentBox(moofOffset, 0, io, fragment)) {
        return false;
    }
    
    // Find the associated mdat box
    fragment.mdatOffset = FindMediaDataBox(moofOffset, io);
    if (fragment.mdatOffset == 0) {
        // No mdat box found, fragment is incomplete
        fragment.isComplete = false;
        return false;
    }
    
    // Mark fragment as complete
    fragment.isComplete = true;
    
    // Add fragment to our collection
    if (!AddFragment(fragment)) {
        return false;
    }
    
    // Set fragmented flag
    hasFragments = true;
    
    return true;
}

bool ISODemuxerFragmentHandler::ParseMovieFragmentBox(uint64_t offset, uint64_t size, std::shared_ptr<IOHandler> io, 
                                           MovieFragmentInfo& fragment) {
    // Get file size for validation
    io->seek(0, SEEK_END);
    uint64_t fileSize = static_cast<uint64_t>(io->tell());
    
    // If size is 0, read the box header to get the actual size
    if (size == 0) {
        io->seek(static_cast<long>(offset), SEEK_SET);
        
        uint8_t headerBytes[8];
        if (io->read(headerBytes, 1, 8) != 8) {
            return false;
        }
        
        size = (static_cast<uint32_t>(headerBytes[0]) << 24) |
               (static_cast<uint32_t>(headerBytes[1]) << 16) |
               (static_cast<uint32_t>(headerBytes[2]) << 8) |
               static_cast<uint32_t>(headerBytes[3]);
        
        // Validate box type is 'moof'
        uint32_t boxType = (static_cast<uint32_t>(headerBytes[4]) << 24) |
                          (static_cast<uint32_t>(headerBytes[5]) << 16) |
                          (static_cast<uint32_t>(headerBytes[6]) << 8) |
                          static_cast<uint32_t>(headerBytes[7]);
        
        if (boxType != BOX_MOOF) {
            return false;
        }
        
        // Handle extended size
        if (size == 1) {
            if (offset + 16 > fileSize) {
                return false;
            }
            
            uint8_t extSizeBytes[8];
            if (io->read(extSizeBytes, 1, 8) != 8) {
                return false;
            }
            
            size = (static_cast<uint64_t>(extSizeBytes[0]) << 56) |
                   (static_cast<uint64_t>(extSizeBytes[1]) << 48) |
                   (static_cast<uint64_t>(extSizeBytes[2]) << 40) |
                   (static_cast<uint64_t>(extSizeBytes[3]) << 32) |
                   (static_cast<uint64_t>(extSizeBytes[4]) << 24) |
                   (static_cast<uint64_t>(extSizeBytes[5]) << 16) |
                   (static_cast<uint64_t>(extSizeBytes[6]) << 8) |
                   static_cast<uint64_t>(extSizeBytes[7]);
        }
    }
    
    // Validate size
    if (offset + size > fileSize) {
        return false;
    }
    
    // Parse moof box children
    uint64_t currentOffset = offset + 8; // Skip box header
    uint64_t endOffset = offset + size;
    
    while (currentOffset < endOffset) {
        // Read box header
        io->seek(static_cast<long>(currentOffset), SEEK_SET);
        
        uint8_t headerBytes[8];
        if (io->read(headerBytes, 1, 8) != 8) {
            return false;
        }
        
        uint32_t boxSize = (static_cast<uint32_t>(headerBytes[0]) << 24) |
                          (static_cast<uint32_t>(headerBytes[1]) << 16) |
                          (static_cast<uint32_t>(headerBytes[2]) << 8) |
                          static_cast<uint32_t>(headerBytes[3]);
        
        uint32_t boxType = (static_cast<uint32_t>(headerBytes[4]) << 24) |
                          (static_cast<uint32_t>(headerBytes[5]) << 16) |
                          (static_cast<uint32_t>(headerBytes[6]) << 8) |
                          static_cast<uint32_t>(headerBytes[7]);
        
        // Handle extended size
        uint64_t boxDataOffset = currentOffset + 8;
        if (boxSize == 1) {
            if (currentOffset + 16 > endOffset) {
                return false;
            }
            
            uint8_t extSizeBytes[8];
            if (io->read(extSizeBytes, 1, 8) != 8) {
                return false;
            }
            
            boxSize = (static_cast<uint64_t>(extSizeBytes[0]) << 56) |
                     (static_cast<uint64_t>(extSizeBytes[1]) << 48) |
                     (static_cast<uint64_t>(extSizeBytes[2]) << 40) |
                     (static_cast<uint64_t>(extSizeBytes[3]) << 32) |
                     (static_cast<uint64_t>(extSizeBytes[4]) << 24) |
                     (static_cast<uint64_t>(extSizeBytes[5]) << 16) |
                     (static_cast<uint64_t>(extSizeBytes[6]) << 8) |
                     static_cast<uint64_t>(extSizeBytes[7]);
            
            boxDataOffset += 8;
        }
        
        // Validate box size
        if (boxSize < 8 || currentOffset + boxSize > endOffset) {
            return false;
        }
        
        // Process box based on type
        switch (boxType) {
            case BOX_MFHD: // Movie fragment header
                if (!ParseMovieFragmentHeader(boxDataOffset, boxSize - (boxDataOffset - currentOffset), io, fragment)) {
                    return false;
                }
                break;
                
            case BOX_TRAF: // Track fragment
                {
                    TrackFragmentInfo traf;
                    if (ParseTrackFragmentBox(boxDataOffset, boxSize - (boxDataOffset - currentOffset), io, traf)) {
                        fragment.trackFragments.push_back(traf);
                    }
                }
                break;
                
            default:
                // Skip unknown box
                break;
        }
        
        // Move to next box
        currentOffset += boxSize;
    }
    
    // Validate fragment
    return ValidateFragment(fragment);
}

bool ISODemuxerFragmentHandler::ParseMovieFragmentHeader(uint64_t offset, uint64_t size, std::shared_ptr<IOHandler> io,
                                             MovieFragmentInfo& fragment) {
    if (size < 8) {
        return false;
    }
    
    // Skip version/flags (4 bytes)
    uint32_t sequenceNumber = ReadUInt32BE(io, offset + 4);
    
    // Validate sequence number
    if (sequenceNumber == 0) {
        return false;
    }
    
    fragment.sequenceNumber = sequenceNumber;
    return true;
}

bool ISODemuxerFragmentHandler::ParseTrackFragmentBox(uint64_t offset, uint64_t size, std::shared_ptr<IOHandler> io,
                                          TrackFragmentInfo& traf) {
    // Parse traf box children
    uint64_t currentOffset = offset;
    uint64_t endOffset = offset + size;
    
    while (currentOffset < endOffset) {
        // Read box header
        io->seek(static_cast<long>(currentOffset), SEEK_SET);
        
        uint8_t headerBytes[8];
        if (io->read(headerBytes, 1, 8) != 8) {
            return false;
        }
        
        uint32_t boxSize = (static_cast<uint32_t>(headerBytes[0]) << 24) |
                          (static_cast<uint32_t>(headerBytes[1]) << 16) |
                          (static_cast<uint32_t>(headerBytes[2]) << 8) |
                          static_cast<uint32_t>(headerBytes[3]);
        
        uint32_t boxType = (static_cast<uint32_t>(headerBytes[4]) << 24) |
                          (static_cast<uint32_t>(headerBytes[5]) << 16) |
                          (static_cast<uint32_t>(headerBytes[6]) << 8) |
                          static_cast<uint32_t>(headerBytes[7]);
        
        // Handle extended size
        uint64_t boxDataOffset = currentOffset + 8;
        if (boxSize == 1) {
            if (currentOffset + 16 > endOffset) {
                return false;
            }
            
            uint8_t extSizeBytes[8];
            if (io->read(extSizeBytes, 1, 8) != 8) {
                return false;
            }
            
            boxSize = (static_cast<uint64_t>(extSizeBytes[0]) << 56) |
                     (static_cast<uint64_t>(extSizeBytes[1]) << 48) |
                     (static_cast<uint64_t>(extSizeBytes[2]) << 40) |
                     (static_cast<uint64_t>(extSizeBytes[3]) << 32) |
                     (static_cast<uint64_t>(extSizeBytes[4]) << 24) |
                     (static_cast<uint64_t>(extSizeBytes[5]) << 16) |
                     (static_cast<uint64_t>(extSizeBytes[6]) << 8) |
                     static_cast<uint64_t>(extSizeBytes[7]);
            
            boxDataOffset += 8;
        }
        
        // Validate box size
        if (boxSize < 8 || currentOffset + boxSize > endOffset) {
            return false;
        }
        
        // Process box based on type
        switch (boxType) {
            case BOX_TFHD: // Track fragment header
                if (!ParseTrackFragmentHeader(boxDataOffset, boxSize - (boxDataOffset - currentOffset), io, traf)) {
                    return false;
                }
                break;
                
            case BOX_TRUN: // Track fragment run
                {
                    TrackFragmentInfo::TrackRunInfo trun;
                    if (ParseTrackFragmentRun(boxDataOffset, boxSize - (boxDataOffset - currentOffset), io, trun)) {
                        traf.trackRuns.push_back(trun);
                    }
                }
                break;
                
            case BOX_TFDT: // Track fragment decode time
                if (!ParseTrackFragmentDecodeTime(boxDataOffset, boxSize - (boxDataOffset - currentOffset), io, traf)) {
                    // TFDT is optional, so continue even if parsing fails
                }
                break;
                
            default:
                // Skip unknown box
                break;
        }
        
        // Move to next box
        currentOffset += boxSize;
    }
    
    // Validate track fragment
    return ValidateTrackFragment(traf);
}

bool ISODemuxerFragmentHandler::ParseTrackFragmentHeader(uint64_t offset, uint64_t size, std::shared_ptr<IOHandler> io,
                                             TrackFragmentInfo& traf) {
    if (size < 8) {
        return false;
    }
    
    // Read version and flags
    uint32_t versionFlags = ReadUInt32BE(io, offset);
    // uint8_t version = (versionFlags >> 24) & 0xFF; // Not used currently
    uint32_t flags = versionFlags & 0x00FFFFFF;
    
    // Read track ID (required)
    uint32_t trackId = ReadUInt32BE(io, offset + 4);
    
    // Validate track ID
    if (trackId == 0) {
        return false;
    }
    
    traf.trackId = trackId;
    
    // Process optional fields based on flags
    uint64_t fieldOffset = offset + 8;
    
    // Base data offset present
    if (flags & 0x000001) {
        if (fieldOffset + 8 > offset + size) {
            return false;
        }
        traf.baseDataOffset = ReadUInt64BE(io, fieldOffset);
        fieldOffset += 8;
    }
    
    // Sample description index present
    if (flags & 0x000002) {
        if (fieldOffset + 4 > offset + size) {
            return false;
        }
        traf.sampleDescriptionIndex = ReadUInt32BE(io, fieldOffset);
        fieldOffset += 4;
    }
    
    // Default sample duration present
    if (flags & 0x000008) {
        if (fieldOffset + 4 > offset + size) {
            return false;
        }
        traf.defaultSampleDuration = ReadUInt32BE(io, fieldOffset);
        fieldOffset += 4;
    } else {
        // Use default from movie header if available
        traf.defaultSampleDuration = defaults.defaultSampleDuration;
    }
    
    // Default sample size present
    if (flags & 0x000010) {
        if (fieldOffset + 4 > offset + size) {
            return false;
        }
        traf.defaultSampleSize = ReadUInt32BE(io, fieldOffset);
        fieldOffset += 4;
    } else {
        // Use default from movie header if available
        traf.defaultSampleSize = defaults.defaultSampleSize;
    }
    
    // Default sample flags present
    if (flags & 0x000020) {
        if (fieldOffset + 4 > offset + size) {
            return false;
        }
        traf.defaultSampleFlags = ReadUInt32BE(io, fieldOffset);
        fieldOffset += 4;
    } else {
        // Use default from movie header if available
        traf.defaultSampleFlags = defaults.defaultSampleFlags;
    }
    
    return true;
}

bool ISODemuxerFragmentHandler::ParseTrackFragmentRun(uint64_t offset, uint64_t size, std::shared_ptr<IOHandler> io,
                                          TrackFragmentInfo::TrackRunInfo& trun) {
    if (size < 8) {
        return false;
    }
    
    // Read version and flags
    uint32_t versionFlags = ReadUInt32BE(io, offset);
    uint8_t version = (versionFlags >> 24) & 0xFF;
    uint32_t flags = versionFlags & 0x00FFFFFF;
    
    // Read sample count (required)
    uint32_t sampleCount = ReadUInt32BE(io, offset + 4);
    
    // Validate sample count
    if (sampleCount == 0) {
        return false;
    }
    
    trun.sampleCount = sampleCount;
    
    // Process optional fields based on flags
    uint64_t fieldOffset = offset + 8;
    
    // Data offset present
    if (flags & 0x000001) {
        if (fieldOffset + 4 > offset + size) {
            return false;
        }
        trun.dataOffset = ReadUInt32BE(io, fieldOffset);
        fieldOffset += 4;
    }
    
    // First sample flags present
    if (flags & 0x000004) {
        if (fieldOffset + 4 > offset + size) {
            return false;
        }
        trun.firstSampleFlags = ReadUInt32BE(io, fieldOffset);
        fieldOffset += 4;
    }
    
    // Prepare arrays for per-sample data
    if (flags & 0x000100) { // Sample duration present
        trun.sampleDurations.reserve(sampleCount);
    }
    
    if (flags & 0x000200) { // Sample size present
        trun.sampleSizes.reserve(sampleCount);
    }
    
    if (flags & 0x000400) { // Sample flags present
        trun.sampleFlags.reserve(sampleCount);
    }
    
    if (flags & 0x000800) { // Sample composition time offsets present
        trun.sampleCompositionTimeOffsets.reserve(sampleCount);
    }
    
    // Read per-sample data
    for (uint32_t i = 0; i < sampleCount; i++) {
        // Sample duration present
        if (flags & 0x000100) {
            if (fieldOffset + 4 > offset + size) {
                return false;
            }
            trun.sampleDurations.push_back(ReadUInt32BE(io, fieldOffset));
            fieldOffset += 4;
        }
        
        // Sample size present
        if (flags & 0x000200) {
            if (fieldOffset + 4 > offset + size) {
                return false;
            }
            trun.sampleSizes.push_back(ReadUInt32BE(io, fieldOffset));
            fieldOffset += 4;
        }
        
        // Sample flags present
        if (flags & 0x000400) {
            if (fieldOffset + 4 > offset + size) {
                return false;
            }
            trun.sampleFlags.push_back(ReadUInt32BE(io, fieldOffset));
            fieldOffset += 4;
        }
        
        // Sample composition time offsets present
        if (flags & 0x000800) {
            if (fieldOffset + 4 > offset + size) {
                return false;
            }
            
            if (version == 0) {
                // Unsigned 32-bit value
                trun.sampleCompositionTimeOffsets.push_back(ReadUInt32BE(io, fieldOffset));
            } else {
                // Signed 32-bit value (version 1)
                int32_t offset = static_cast<int32_t>(ReadUInt32BE(io, fieldOffset));
                trun.sampleCompositionTimeOffsets.push_back(static_cast<uint32_t>(offset));
            }
            
            fieldOffset += 4;
        }
    }
    
    return true;
}

bool ISODemuxerFragmentHandler::ParseTrackFragmentDecodeTime(uint64_t offset, uint64_t size, std::shared_ptr<IOHandler> io,
                                                 TrackFragmentInfo& traf) {
    if (size < 8) {
        return false;
    }
    
    // Read version and flags
    uint32_t versionFlags = ReadUInt32BE(io, offset);
    uint8_t version = (versionFlags >> 24) & 0xFF;
    
    // Read base media decode time
    if (version == 1) {
        // 64-bit decode time
        if (size < 12) {
            return false;
        }
        traf.tfdt = ReadUInt64BE(io, offset + 4);
    } else {
        // 32-bit decode time
        traf.tfdt = ReadUInt32BE(io, offset + 4);
    }
    
    return true;
}

bool ISODemuxerFragmentHandler::UpdateSampleTables(const TrackFragmentInfo& traf, AudioTrackInfo& track) {
    // Check if this track fragment matches our track
    if (traf.trackId != track.trackId) {
        return false;
    }
    
    // Process each track run
    for (const auto& trun : traf.trackRuns) {
        uint32_t sampleCount = trun.sampleCount;
        
        // Calculate base data offset for this run
        uint64_t baseDataOffset = 0;
        
        // If base data offset is specified in tfhd, use it
        if (traf.baseDataOffset != 0) {
            baseDataOffset = traf.baseDataOffset;
        } else {
            // Otherwise, use the moof offset as base
            MovieFragmentInfo* currentFragment = GetCurrentFragment();
            if (currentFragment) {
                baseDataOffset = currentFragment->moofOffset;
            } else {
                return false;
            }
        }
        
        // Add data offset from trun if present
        if (trun.dataOffset != 0) {
            baseDataOffset += trun.dataOffset;
        }
        
        // Update sample tables with new samples
        for (uint32_t i = 0; i < sampleCount; i++) {
            // Get sample size
            uint32_t sampleSize = 0;
            if (i < trun.sampleSizes.size()) {
                // Use per-sample size from trun
                sampleSize = trun.sampleSizes[i];
            } else {
                // Use default sample size from tfhd
                sampleSize = traf.defaultSampleSize;
            }
            
            // Get sample duration
            uint32_t sampleDuration = 0;
            if (i < trun.sampleDurations.size()) {
                // Use per-sample duration from trun
                sampleDuration = trun.sampleDurations[i];
            } else {
                // Use default sample duration from tfhd
                sampleDuration = traf.defaultSampleDuration;
            }
            
            // Add sample size to sample table
            track.sampleTableInfo.sampleSizes.push_back(sampleSize);
            
            // Add sample time to sample table
            if (track.sampleTableInfo.sampleTimes.empty()) {
                // First sample in fragment, use tfdt as base time
                track.sampleTableInfo.sampleTimes.push_back(traf.tfdt);
            } else {
                // Calculate time based on previous sample
                uint64_t prevTime = track.sampleTableInfo.sampleTimes.back();
                track.sampleTableInfo.sampleTimes.push_back(prevTime + sampleDuration);
            }
            
            // Add chunk offset for this sample
            if (i == 0) {
                // First sample in run, add chunk offset
                track.sampleTableInfo.chunkOffsets.push_back(baseDataOffset);
            }
            
            // Update base data offset for next sample
            baseDataOffset += sampleSize;
        }
        
        // Update sample-to-chunk entries if needed
        if (sampleCount > 0 && !track.sampleTableInfo.chunkOffsets.empty()) {
            // Add a new sample-to-chunk entry for this run
            SampleToChunkEntry entry;
            entry.firstChunk = static_cast<uint32_t>(track.sampleTableInfo.chunkOffsets.size() - 1);
            entry.samplesPerChunk = sampleCount;
            entry.sampleDescIndex = traf.sampleDescriptionIndex > 0 ? traf.sampleDescriptionIndex : 1;
            
            track.sampleTableInfo.sampleToChunkEntries.push_back(entry);
        }
    }
    
    return true;
}

bool ISODemuxerFragmentHandler::AddFragment(const MovieFragmentInfo& fragment) {
    // Check if fragment already exists
    for (const auto& existingFragment : fragments) {
        if (existingFragment.sequenceNumber == fragment.sequenceNumber) {
            // Fragment already exists, skip
            return true;
        }
    }
    
    // Add fragment
    fragments.push_back(fragment);
    
    // Sort fragments by sequence number
    ReorderFragments();
    
    return true;
}

bool ISODemuxerFragmentHandler::ReorderFragments() {
    // Sort fragments by sequence number
    std::sort(fragments.begin(), fragments.end(), CompareFragmentsBySequence);
    
    // Check for missing fragments
    if (HasMissingFragments()) {
        FillMissingFragmentGaps();
    }
    
    return true;
}

bool ISODemuxerFragmentHandler::CompareFragmentsBySequence(const MovieFragmentInfo& a, const MovieFragmentInfo& b) {
    return a.sequenceNumber < b.sequenceNumber;
}

bool ISODemuxerFragmentHandler::SeekToFragment(uint32_t sequenceNumber) {
    // Find fragment with matching sequence number
    for (size_t i = 0; i < fragments.size(); i++) {
        if (fragments[i].sequenceNumber == sequenceNumber) {
            currentFragmentIndex = static_cast<uint32_t>(i);
            return true;
        }
    }
    
    return false;
}

MovieFragmentInfo* ISODemuxerFragmentHandler::GetCurrentFragment() {
    if (fragments.empty() || currentFragmentIndex >= fragments.size()) {
        return nullptr;
    }
    
    return &fragments[currentFragmentIndex];
}

MovieFragmentInfo* ISODemuxerFragmentHandler::GetFragment(uint32_t sequenceNumber) {
    // Find fragment with matching sequence number
    for (auto& fragment : fragments) {
        if (fragment.sequenceNumber == sequenceNumber) {
            return &fragment;
        }
    }
    
    return nullptr;
}

bool ISODemuxerFragmentHandler::IsFragmentComplete(uint32_t sequenceNumber) const {
    // Find fragment with matching sequence number
    for (const auto& fragment : fragments) {
        if (fragment.sequenceNumber == sequenceNumber) {
            return fragment.isComplete;
        }
    }
    
    return false;
}

bool ISODemuxerFragmentHandler::ValidateFragment(const MovieFragmentInfo& fragment) const {
    // Check if fragment has a valid sequence number
    if (fragment.sequenceNumber == 0) {
        return false;
    }
    
    // Check if fragment has at least one track fragment
    if (fragment.trackFragments.empty()) {
        return false;
    }
    
    // Validate each track fragment
    for (const auto& traf : fragment.trackFragments) {
        if (!ValidateTrackFragment(traf)) {
            return false;
        }
    }
    
    return true;
}

bool ISODemuxerFragmentHandler::ValidateTrackFragment(const TrackFragmentInfo& traf) const {
    // Check if track fragment has a valid track ID
    if (traf.trackId == 0) {
        return false;
    }
    
    // Check if track fragment has at least one track run
    if (traf.trackRuns.empty()) {
        return false;
    }
    
    // Validate each track run
    for (const auto& trun : traf.trackRuns) {
        if (trun.sampleCount == 0) {
            return false;
        }
    }
    
    return true;
}

bool ISODemuxerFragmentHandler::ExtractFragmentSample(uint32_t trackId, uint64_t sampleIndex, 
                                          uint64_t& offset, uint32_t& size) {
    // Find the fragment containing this sample
    // This is a simplified implementation - in a real-world scenario,
    // we would need to map global sample indices to fragment-specific indices
    
    MovieFragmentInfo* fragment = GetCurrentFragment();
    if (!fragment) {
        return false;
    }
    
    // Find the track fragment for this track
    TrackFragmentInfo* traf = nullptr;
    for (auto& trackFragment : fragment->trackFragments) {
        if (trackFragment.trackId == trackId) {
            traf = &trackFragment;
            break;
        }
    }
    
    if (!traf) {
        return false;
    }
    
    // Find the track run containing this sample
    uint64_t runStartSample = 0;
    
    for (const auto& trun : traf->trackRuns) {
        if (sampleIndex >= runStartSample && sampleIndex < runStartSample + trun.sampleCount) {
            // Found the run containing this sample
            uint64_t sampleIndexInRun = sampleIndex - runStartSample;
            
            // Calculate base data offset
            uint64_t baseDataOffset = traf->baseDataOffset;
            if (baseDataOffset == 0) {
                baseDataOffset = fragment->moofOffset;
            }
            
            // Add data offset from trun
            baseDataOffset += trun.dataOffset;
            
            // Calculate sample offset within run
            for (uint64_t i = 0; i < sampleIndexInRun; i++) {
                if (i < trun.sampleSizes.size()) {
                    baseDataOffset += trun.sampleSizes[i];
                } else {
                    baseDataOffset += traf->defaultSampleSize;
                }
            }
            
            // Set output parameters
            offset = baseDataOffset;
            if (sampleIndexInRun < trun.sampleSizes.size()) {
                size = trun.sampleSizes[sampleIndexInRun];
            } else {
                size = traf->defaultSampleSize;
            }
            
            return true;
        }
        
        runStartSample += trun.sampleCount;
    }
    
    return false;
}

void ISODemuxerFragmentHandler::SetDefaultValues(const AudioTrackInfo& movieHeaderDefaults) {
    // Set default values from movie header
    defaults.defaultSampleDuration = 1024; // Default for audio
    defaults.defaultSampleSize = 0; // Variable size
    defaults.defaultSampleFlags = 0; // No flags
    
    // If we have valid sample tables from movie header, use them to derive defaults
    if (!movieHeaderDefaults.sampleTableInfo.sampleSizes.empty()) {
        // Use most common sample size as default
        std::map<uint32_t, uint32_t> sizeCounts;
        for (uint32_t size : movieHeaderDefaults.sampleTableInfo.sampleSizes) {
            sizeCounts[size]++;
        }
        
        uint32_t mostCommonSize = 0;
        uint32_t maxCount = 0;
        for (const auto& pair : sizeCounts) {
            if (pair.second > maxCount) {
                maxCount = pair.second;
                mostCommonSize = pair.first;
            }
        }
        
        if (mostCommonSize > 0) {
            defaults.defaultSampleSize = mostCommonSize;
        }
    }
    
    // If we have valid sample times, calculate average duration
    if (movieHeaderDefaults.sampleTableInfo.sampleTimes.size() > 1) {
        uint64_t totalDuration = 0;
        for (size_t i = 1; i < movieHeaderDefaults.sampleTableInfo.sampleTimes.size(); i++) {
            totalDuration += movieHeaderDefaults.sampleTableInfo.sampleTimes[i] - 
                            movieHeaderDefaults.sampleTableInfo.sampleTimes[i-1];
        }
        
        uint32_t avgDuration = static_cast<uint32_t>(totalDuration / (movieHeaderDefaults.sampleTableInfo.sampleTimes.size() - 1));
        if (avgDuration > 0) {
            defaults.defaultSampleDuration = avgDuration;
        }
    }
}

uint64_t ISODemuxerFragmentHandler::FindMediaDataBox(uint64_t moofOffset, std::shared_ptr<IOHandler> io) {
    // Get file size for validation
    io->seek(0, SEEK_END);
    uint64_t fileSize = static_cast<uint64_t>(io->tell());
    
    // Start searching after the moof box
    io->seek(static_cast<long>(moofOffset), SEEK_SET);
    
    uint8_t headerBytes[8];
    if (io->read(headerBytes, 1, 8) != 8) {
        return 0;
    }
    
    uint32_t moofSize = (static_cast<uint32_t>(headerBytes[0]) << 24) |
                       (static_cast<uint32_t>(headerBytes[1]) << 16) |
                       (static_cast<uint32_t>(headerBytes[2]) << 8) |
                       static_cast<uint32_t>(headerBytes[3]);
    
    // Handle extended size
    if (moofSize == 1) {
        if (moofOffset + 16 > fileSize) {
            return 0;
        }
        
        uint8_t extSizeBytes[8];
        if (io->read(extSizeBytes, 1, 8) != 8) {
            return 0;
        }
        
        moofSize = (static_cast<uint64_t>(extSizeBytes[0]) << 56) |
                  (static_cast<uint64_t>(extSizeBytes[1]) << 48) |
                  (static_cast<uint64_t>(extSizeBytes[2]) << 40) |
                  (static_cast<uint64_t>(extSizeBytes[3]) << 32) |
                  (static_cast<uint64_t>(extSizeBytes[4]) << 24) |
                  (static_cast<uint64_t>(extSizeBytes[5]) << 16) |
                  (static_cast<uint64_t>(extSizeBytes[6]) << 8) |
                  static_cast<uint64_t>(extSizeBytes[7]);
    }
    
    // Look for mdat box after moof
    uint64_t currentOffset = moofOffset + moofSize;
    
    while (currentOffset + 8 <= fileSize) {
        io->seek(static_cast<long>(currentOffset), SEEK_SET);
        
        if (io->read(headerBytes, 1, 8) != 8) {
            return 0;
        }
        
        uint32_t boxSize = (static_cast<uint32_t>(headerBytes[0]) << 24) |
                          (static_cast<uint32_t>(headerBytes[1]) << 16) |
                          (static_cast<uint32_t>(headerBytes[2]) << 8) |
                          static_cast<uint32_t>(headerBytes[3]);
        
        uint32_t boxType = (static_cast<uint32_t>(headerBytes[4]) << 24) |
                          (static_cast<uint32_t>(headerBytes[5]) << 16) |
                          (static_cast<uint32_t>(headerBytes[6]) << 8) |
                          static_cast<uint32_t>(headerBytes[7]);
        
        if (boxType == BOX_MDAT) {
            // Found mdat box
            return currentOffset;
        }
        
        // Handle extended size
        uint64_t actualBoxSize = boxSize;
        if (boxSize == 1) {
            if (currentOffset + 16 > fileSize) {
                return 0;
            }
            
            uint8_t extSizeBytes[8];
            if (io->read(extSizeBytes, 1, 8) != 8) {
                return 0;
            }
            
            actualBoxSize = (static_cast<uint64_t>(extSizeBytes[0]) << 56) |
                           (static_cast<uint64_t>(extSizeBytes[1]) << 48) |
                           (static_cast<uint64_t>(extSizeBytes[2]) << 40) |
                           (static_cast<uint64_t>(extSizeBytes[3]) << 32) |
                           (static_cast<uint64_t>(extSizeBytes[4]) << 24) |
                           (static_cast<uint64_t>(extSizeBytes[5]) << 16) |
                           (static_cast<uint64_t>(extSizeBytes[6]) << 8) |
                           static_cast<uint64_t>(extSizeBytes[7]);
        } else if (boxSize == 0) {
            // Box extends to end of file
            actualBoxSize = fileSize - currentOffset;
        }
        
        // Move to next box
        if (actualBoxSize < 8) {
            // Invalid box size
            return 0;
        }
        
        currentOffset += actualBoxSize;
    }
    
    return 0;
}

uint32_t ISODemuxerFragmentHandler::ReadUInt32BE(std::shared_ptr<IOHandler> io, uint64_t offset) {
    io->seek(static_cast<long>(offset), SEEK_SET);
    
    uint8_t bytes[4];
    if (io->read(bytes, 1, 4) != 4) {
        return 0;
    }
    
    return (static_cast<uint32_t>(bytes[0]) << 24) |
           (static_cast<uint32_t>(bytes[1]) << 16) |
           (static_cast<uint32_t>(bytes[2]) << 8) |
           static_cast<uint32_t>(bytes[3]);
}

uint64_t ISODemuxerFragmentHandler::ReadUInt64BE(std::shared_ptr<IOHandler> io, uint64_t offset) {
    io->seek(static_cast<long>(offset), SEEK_SET);
    
    uint8_t bytes[8];
    if (io->read(bytes, 1, 8) != 8) {
        return 0;
    }
    
    return (static_cast<uint64_t>(bytes[0]) << 56) |
           (static_cast<uint64_t>(bytes[1]) << 48) |
           (static_cast<uint64_t>(bytes[2]) << 40) |
           (static_cast<uint64_t>(bytes[3]) << 32) |
           (static_cast<uint64_t>(bytes[4]) << 24) |
           (static_cast<uint64_t>(bytes[5]) << 16) |
           (static_cast<uint64_t>(bytes[6]) << 8) |
           static_cast<uint64_t>(bytes[7]);
}

bool ISODemuxerFragmentHandler::HasMissingFragments() const {
    if (fragments.size() <= 1) {
        return false;
    }
    
    for (size_t i = 1; i < fragments.size(); i++) {
        if (fragments[i].sequenceNumber != fragments[i-1].sequenceNumber + 1) {
            return true;
        }
    }
    
    return false;
}

void ISODemuxerFragmentHandler::FillMissingFragmentGaps() {
    // Create placeholder fragments for missing sequence numbers
    // This is a simplified implementation - in practice, you might want to
    // handle missing fragments differently (e.g., request them from server)
    std::vector<MovieFragmentInfo> completeFragments;
    
    if (!fragments.empty()) {
        uint32_t expectedSequence = fragments[0].sequenceNumber;
        
        for (const auto& fragment : fragments) {
            // Fill gaps with placeholder fragments
            while (expectedSequence < fragment.sequenceNumber) {
                MovieFragmentInfo placeholder;
                placeholder.sequenceNumber = expectedSequence;
                placeholder.moofOffset = 0;
                placeholder.mdatOffset = 0;
                placeholder.mdatSize = 0;
                placeholder.isComplete = false;
                completeFragments.push_back(placeholder);
                expectedSequence++;
            }
            
            completeFragments.push_back(fragment);
            expectedSequence = fragment.sequenceNumber + 1;
        }
    }
    
    fragments = std::move(completeFragments);
}