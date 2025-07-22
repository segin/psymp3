/*
 * ErrorRecovery.cpp - Error handling and recovery for ISO demuxer
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include <algorithm>
#include <chrono>
#include <thread>
#include <cstring>
#include <sstream>

ErrorRecovery::ErrorRecovery(std::shared_ptr<IOHandler> io) : io(io) {
    ResetErrorStats();
}

BoxHeader ErrorRecovery::RecoverCorruptedBox(const BoxHeader& header, uint64_t containerSize, uint64_t fileSize) {
    BoxHeader recoveredHeader = header;
    
    // Log the error
    std::stringstream ss;
    ss << "Corrupted box of type '" << BoxTypeToString(header.type) << "' with size " << header.size;
    LogError("CorruptedBox", ss.str(), header.type);
    
    // Check if box type is valid
    if (!IsKnownBoxType(header.type)) {
        // Unknown box type, likely corruption
        // Return empty header to indicate unrecoverable
        recoveredHeader.type = 0;
        recoveredHeader.size = 0;
        return recoveredHeader;
    }
    
    // Check if size is reasonable
    if (header.size < MIN_VALID_BOX_SIZE || header.size > MAX_REASONABLE_BOX_SIZE) {
        // Size is unreasonable, try to estimate a better size
        uint32_t estimatedSize = EstimateReasonableBoxSize(header.type, containerSize);
        
        // Make sure estimated size doesn't exceed container size
        if (estimatedSize > 0 && estimatedSize <= containerSize) {
            recoveredHeader.size = estimatedSize;
            
            // Log the recovery
            std::stringstream ss;
            ss << "Recovered box size from " << header.size << " to " << estimatedSize;
            LogError("BoxSizeRecovery", ss.str(), header.type);
        } else {
            // Can't estimate size, use container size as fallback
            recoveredHeader.size = containerSize;
            
            // Log the fallback
            std::stringstream ss;
            ss << "Using container size " << containerSize << " as fallback for corrupted box";
            LogError("BoxSizeFallback", ss.str(), header.type);
        }
    }
    
    // Check if box extends beyond file
    uint64_t boxStart = header.dataOffset - (header.isExtendedSize() ? 16 : 8);
    if (boxStart + recoveredHeader.size > fileSize) {
        // Box extends beyond file, truncate to file size
        recoveredHeader.size = fileSize - boxStart;
        
        // Log the truncation
        std::stringstream ss;
        ss << "Truncated box size to " << recoveredHeader.size << " to fit within file";
        LogError("BoxSizeTruncation", ss.str(), header.type);
    }
    
    return recoveredHeader;
}

bool ErrorRecovery::RepairSampleTables(SampleTableInfo& tables) {
    bool success = true;
    
    // Check if we have any sample tables to repair
    if (tables.chunkOffsets.empty() && tables.sampleToChunkEntries.empty() && 
        tables.sampleSizes.empty() && tables.sampleTimes.empty()) {
        LogError("SampleTableRepair", "No sample tables to repair");
        return false;
    }
    
    // Try to repair each table
    if (!RepairTimeToSampleTable(tables)) {
        LogError("SampleTableRepair", "Failed to repair time-to-sample table");
        success = false;
    }
    
    if (!RepairSampleToChunkTable(tables)) {
        LogError("SampleTableRepair", "Failed to repair sample-to-chunk table");
        success = false;
    }
    
    if (!RepairSampleSizeTable(tables)) {
        LogError("SampleTableRepair", "Failed to repair sample size table");
        success = false;
    }
    
    if (!RepairChunkOffsetTable(tables)) {
        LogError("SampleTableRepair", "Failed to repair chunk offset table");
        success = false;
    }
    
    // Validate consistency between tables
    if (!ValidateTableConsistency(tables)) {
        LogError("SampleTableRepair", "Sample tables are inconsistent after repair");
        success = false;
    }
    
    return success;
}

bool ErrorRecovery::RepairTimeToSampleTable(SampleTableInfo& tables) {
    // Check if time-to-sample table is empty
    if (tables.sampleTimes.empty()) {
        // Try to create a time-to-sample table from other information
        
        // Estimate total samples from sample size table
        size_t totalSamples = 0;
        if (!tables.sampleSizes.empty()) {
            if (tables.sampleSizes.size() == 1) {
                // Fixed sample size, need to estimate count from chunk information
                size_t totalChunks = tables.chunkOffsets.size();
                if (totalChunks > 0 && !tables.sampleToChunkEntries.empty()) {
                    // Use sample-to-chunk entries to estimate total samples
                    uint32_t samplesPerChunk = tables.sampleToChunkEntries[0].samplesPerChunk;
                    totalSamples = totalChunks * samplesPerChunk;
                }
            } else {
                // Variable sample sizes, count is size of array
                totalSamples = tables.sampleSizes.size();
            }
        }
        
        if (totalSamples > 0) {
            // Create a simple time-to-sample table with constant duration
            // Assume 1024 samples per frame (common for audio)
            uint32_t sampleDuration = 1024;
            tables.sampleTimes.reserve(totalSamples);
            
            uint64_t currentTime = 0;
            for (size_t i = 0; i < totalSamples; i++) {
                tables.sampleTimes.push_back(currentTime);
                currentTime += sampleDuration;
            }
            
            LogError("TimeTableRepair", "Created synthetic time-to-sample table with " + 
                    std::to_string(totalSamples) + " entries");
            return true;
        }
        
        return false;
    }
    
    // Check for discontinuities in time-to-sample table
    for (size_t i = 1; i < tables.sampleTimes.size(); i++) {
        if (tables.sampleTimes[i] <= tables.sampleTimes[i-1]) {
            // Found a discontinuity, fix it
            uint64_t expectedTime = tables.sampleTimes[i-1] + 
                                   (i > 1 ? (tables.sampleTimes[i-1] - tables.sampleTimes[i-2]) : 1024);
            
            tables.sampleTimes[i] = expectedTime;
            
            LogError("TimeTableRepair", "Fixed discontinuity at sample " + std::to_string(i));
        }
    }
    
    return true;
}

bool ErrorRecovery::RepairSampleToChunkTable(SampleTableInfo& tables) {
    // Check if sample-to-chunk table is empty
    if (tables.sampleToChunkEntries.empty()) {
        // Try to create a sample-to-chunk table from other information
        
        // Check if we have chunk offsets
        if (tables.chunkOffsets.empty()) {
            return false;
        }
        
        // Create a simple sample-to-chunk table with one entry
        // Assume each chunk contains one sample (conservative)
        SampleToChunkEntry entry;
        entry.firstChunk = 0; // 0-based indexing
        entry.samplesPerChunk = 1;
        entry.sampleDescIndex = 1;
        
        tables.sampleToChunkEntries.push_back(entry);
        
        LogError("ChunkTableRepair", "Created synthetic sample-to-chunk table with 1 entry");
        return true;
    }
    
    // Check for invalid entries in sample-to-chunk table
    for (auto it = tables.sampleToChunkEntries.begin(); it != tables.sampleToChunkEntries.end(); ) {
        if (it->samplesPerChunk == 0 || it->sampleDescIndex == 0) {
            // Invalid entry, remove it
            it = tables.sampleToChunkEntries.erase(it);
            LogError("ChunkTableRepair", "Removed invalid sample-to-chunk entry");
        } else {
            ++it;
        }
    }
    
    // If all entries were invalid, create a default entry
    if (tables.sampleToChunkEntries.empty()) {
        SampleToChunkEntry entry;
        entry.firstChunk = 0; // 0-based indexing
        entry.samplesPerChunk = 1;
        entry.sampleDescIndex = 1;
        
        tables.sampleToChunkEntries.push_back(entry);
        
        LogError("ChunkTableRepair", "Created default sample-to-chunk entry after removing all invalid entries");
    }
    
    return true;
}

bool ErrorRecovery::RepairSampleSizeTable(SampleTableInfo& tables) {
    // Check if sample size table is empty
    if (tables.sampleSizes.empty()) {
        // Try to create a sample size table from other information
        
        // Estimate total samples from time-to-sample table
        size_t totalSamples = tables.sampleTimes.size();
        
        if (totalSamples == 0) {
            // Try to estimate from chunk information
            size_t totalChunks = tables.chunkOffsets.size();
            if (totalChunks > 0 && !tables.sampleToChunkEntries.empty()) {
                // Use sample-to-chunk entries to estimate total samples
                uint32_t samplesPerChunk = tables.sampleToChunkEntries[0].samplesPerChunk;
                totalSamples = totalChunks * samplesPerChunk;
            }
        }
        
        if (totalSamples > 0) {
            // Create a simple sample size table with constant size
            // Assume 1024 bytes per sample (reasonable for compressed audio)
            uint32_t sampleSize = 1024;
            
            // Use a single entry for fixed sample size
            tables.sampleSizes.push_back(sampleSize);
            
            LogError("SizeTableRepair", "Created synthetic sample size table with fixed size " + 
                    std::to_string(sampleSize));
            return true;
        }
        
        return false;
    }
    
    // Check for invalid sample sizes
    if (tables.sampleSizes.size() == 1) {
        // Fixed sample size
        if (tables.sampleSizes[0] == 0) {
            // Invalid fixed size, use a reasonable default
            tables.sampleSizes[0] = 1024;
            LogError("SizeTableRepair", "Fixed invalid sample size from 0 to 1024");
        }
    } else {
        // Variable sample sizes
        bool hasInvalid = false;
        for (size_t i = 0; i < tables.sampleSizes.size(); i++) {
            if (tables.sampleSizes[i] == 0) {
                // Invalid size, use previous size or default
                uint32_t newSize = (i > 0) ? tables.sampleSizes[i-1] : 1024;
                tables.sampleSizes[i] = newSize;
                hasInvalid = true;
            }
        }
        
        if (hasInvalid) {
            LogError("SizeTableRepair", "Fixed invalid sample sizes in variable size table");
        }
    }
    
    return true;
}

bool ErrorRecovery::RepairChunkOffsetTable(SampleTableInfo& tables) {
    // Check if chunk offset table is empty
    if (tables.chunkOffsets.empty()) {
        // Can't repair chunk offset table without file information
        return false;
    }
    
    // Check for invalid chunk offsets
    bool hasInvalid = false;
    uint64_t lastValidOffset = 0;
    uint32_t avgChunkSize = 0;
    
    // Calculate average chunk size if we have multiple chunks
    if (tables.chunkOffsets.size() > 1) {
        uint64_t totalSize = 0;
        for (size_t i = 1; i < tables.chunkOffsets.size(); i++) {
            if (tables.chunkOffsets[i] > tables.chunkOffsets[i-1]) {
                totalSize += (tables.chunkOffsets[i] - tables.chunkOffsets[i-1]);
            }
        }
        
        avgChunkSize = static_cast<uint32_t>(totalSize / (tables.chunkOffsets.size() - 1));
        if (avgChunkSize == 0) avgChunkSize = 4096; // Fallback
    } else {
        avgChunkSize = 4096; // Default chunk size
    }
    
    // Get file size for validation
    uint64_t fileSize = 0;
    if (io) {
        io->seek(0, SEEK_END);
        fileSize = static_cast<uint64_t>(io->tell());
    }
    
    // Fix invalid offsets
    for (size_t i = 0; i < tables.chunkOffsets.size(); i++) {
        if (tables.chunkOffsets[i] == 0 || (fileSize > 0 && tables.chunkOffsets[i] >= fileSize)) {
            // Invalid offset
            if (i > 0 && lastValidOffset > 0) {
                // Use previous offset plus average chunk size
                tables.chunkOffsets[i] = lastValidOffset + avgChunkSize;
            } else {
                // Can't repair this offset
                tables.chunkOffsets[i] = 0;
            }
            
            hasInvalid = true;
        } else {
            lastValidOffset = tables.chunkOffsets[i];
        }
    }
    
    if (hasInvalid) {
        LogError("OffsetTableRepair", "Fixed invalid chunk offsets");
    }
    
    return true;
}

bool ErrorRecovery::ValidateTableConsistency(const SampleTableInfo& tables) {
    // Check if we have the minimum required tables
    if (tables.chunkOffsets.empty() || tables.sampleToChunkEntries.empty() || 
        tables.sampleSizes.empty() || tables.sampleTimes.empty()) {
        LogError("TableConsistency", "Missing required sample tables");
        return false;
    }
    
    // Calculate total samples from different tables
    size_t sampleCountFromTimes = tables.sampleTimes.size();
    
    size_t sampleCountFromSizes = 0;
    if (tables.sampleSizes.size() == 1) {
        // Fixed sample size, need to calculate from chunks
        size_t totalChunks = tables.chunkOffsets.size();
        size_t totalSamples = 0;
        
        // Calculate samples based on sample-to-chunk entries
        for (size_t i = 0; i < totalChunks; i++) {
            // Find applicable entry
            const SampleToChunkEntry* entry = nullptr;
            for (auto it = tables.sampleToChunkEntries.rbegin(); it != tables.sampleToChunkEntries.rend(); ++it) {
                if (it->firstChunk <= i) {
                    entry = &(*it);
                    break;
                }
            }
            
            if (entry) {
                totalSamples += entry->samplesPerChunk;
            } else {
                // No applicable entry, use first entry as fallback
                totalSamples += tables.sampleToChunkEntries[0].samplesPerChunk;
            }
        }
        
        sampleCountFromSizes = totalSamples;
    } else {
        // Variable sample sizes
        sampleCountFromSizes = tables.sampleSizes.size();
    }
    
    // Check if sample counts are consistent
    if (sampleCountFromTimes != sampleCountFromSizes) {
        LogError("TableConsistency", "Inconsistent sample counts: " + 
                std::to_string(sampleCountFromTimes) + " from times, " + 
                std::to_string(sampleCountFromSizes) + " from sizes");
        
        // Tables are inconsistent, but we'll still try to use them
        return false;
    }
    
    return true;
}

bool ErrorRecovery::InferCodecConfig(AudioTrackInfo& track, const std::vector<uint8_t>& sampleData) {
    // Check if we already have codec configuration
    if (!track.codecConfig.empty()) {
        return true;
    }
    
    // Check if we have sample data to analyze
    if (sampleData.empty()) {
        LogError("CodecConfig", "No sample data available for codec configuration inference");
        return false;
    }
    
    // Infer codec configuration based on codec type
    if (track.codecType == "aac") {
        return InferAACConfig(track, sampleData);
    } else if (track.codecType == "alac") {
        return InferALACConfig(track, sampleData);
    } else if (track.codecType == "pcm") {
        return InferPCMConfig(track, sampleData);
    } else if (track.codecType == "ulaw" || track.codecType == "alaw") {
        // Telephony codecs don't need complex configuration
        // Just ensure we have sample rate and channel count
        if (track.sampleRate == 0) {
            track.sampleRate = 8000; // Default for telephony
            LogError("CodecConfig", "Using default 8kHz sample rate for " + track.codecType);
        }
        
        if (track.channelCount == 0) {
            track.channelCount = 1; // Default mono for telephony
            LogError("CodecConfig", "Using default mono channel configuration for " + track.codecType);
        }
        
        return true;
    }
    
    LogError("CodecConfig", "Unsupported codec type for configuration inference: " + track.codecType);
    return false;
}

bool ErrorRecovery::InferAACConfig(AudioTrackInfo& track, const std::vector<uint8_t>& sampleData) {
    // AAC requires AudioSpecificConfig
    // This is a simplified version that creates a basic AAC-LC configuration
    
    // Check if we have sample rate and channel count
    if (track.sampleRate == 0 || track.channelCount == 0) {
        LogError("AACConfig", "Missing sample rate or channel count for AAC configuration");
        return false;
    }
    
    // Create a minimal AudioSpecificConfig
    // Format: [Audio Object Type (5 bits) | Sampling Frequency Index (4 bits) | Channel Configuration (4 bits) | ...]
    
    // Determine sampling frequency index
    uint8_t samplingFrequencyIndex = 0;
    if (track.sampleRate == 96000) samplingFrequencyIndex = 0;
    else if (track.sampleRate == 88200) samplingFrequencyIndex = 1;
    else if (track.sampleRate == 64000) samplingFrequencyIndex = 2;
    else if (track.sampleRate == 48000) samplingFrequencyIndex = 3;
    else if (track.sampleRate == 44100) samplingFrequencyIndex = 4;
    else if (track.sampleRate == 32000) samplingFrequencyIndex = 5;
    else if (track.sampleRate == 24000) samplingFrequencyIndex = 6;
    else if (track.sampleRate == 22050) samplingFrequencyIndex = 7;
    else if (track.sampleRate == 16000) samplingFrequencyIndex = 8;
    else if (track.sampleRate == 12000) samplingFrequencyIndex = 9;
    else if (track.sampleRate == 11025) samplingFrequencyIndex = 10;
    else if (track.sampleRate == 8000) samplingFrequencyIndex = 11;
    else if (track.sampleRate == 7350) samplingFrequencyIndex = 12;
    else samplingFrequencyIndex = 15; // Escape value
    
    // Create the configuration
    std::vector<uint8_t> config;
    
    // First byte: Audio Object Type (2 = AAC-LC) and top 3 bits of samplingFrequencyIndex
    config.push_back((2 << 3) | ((samplingFrequencyIndex & 0x0E) >> 1));
    
    // Second byte: Bottom bit of samplingFrequencyIndex, channel configuration, and frame length flag
    config.push_back(((samplingFrequencyIndex & 0x01) << 7) | (track.channelCount << 3) | 0);
    
    // If we used escape value for sampling frequency index, add the actual frequency
    if (samplingFrequencyIndex == 15) {
        config.push_back((track.sampleRate >> 16) & 0xFF);
        config.push_back((track.sampleRate >> 8) & 0xFF);
        config.push_back(track.sampleRate & 0xFF);
    }
    
    // Store the configuration
    track.codecConfig = config;
    
    LogError("AACConfig", "Created synthetic AAC configuration with " + 
            std::to_string(config.size()) + " bytes");
    
    return true;
}

bool ErrorRecovery::InferALACConfig(AudioTrackInfo& track, const std::vector<uint8_t>& sampleData) {
    // ALAC requires a magic cookie
    // This is a simplified version that creates a basic ALAC configuration
    
    // Check if we have sample rate, channel count, and bit depth
    if (track.sampleRate == 0 || track.channelCount == 0 || track.bitsPerSample == 0) {
        LogError("ALACConfig", "Missing sample rate, channel count, or bit depth for ALAC configuration");
        return false;
    }
    
    // Create a minimal ALAC magic cookie (24 bytes)
    std::vector<uint8_t> config(24, 0);
    
    // Fill in the magic cookie
    // Bytes 0-3: Frame length (default 4096)
    config[3] = 0x10;
    config[2] = 0x00;
    
    // Bytes 4-7: Compatible version (0)
    
    // Bytes 8-11: Max samples per frame (4096)
    config[11] = 0x10;
    config[10] = 0x00;
    
    // Bytes 12-15: Bits per sample
    config[15] = static_cast<uint8_t>(track.bitsPerSample);
    
    // Bytes 16-17: History multiplier (40)
    config[17] = 40;
    
    // Bytes 18-19: Initial history (10)
    config[19] = 10;
    
    // Bytes 20-21: Maximum K (14)
    config[21] = 14;
    
    // Bytes 22-23: Channels and flags
    config[23] = static_cast<uint8_t>(track.channelCount);
    
    // Store the configuration
    track.codecConfig = config;
    
    LogError("ALACConfig", "Created synthetic ALAC configuration with " + 
            std::to_string(config.size()) + " bytes");
    
    return true;
}

bool ErrorRecovery::InferPCMConfig(AudioTrackInfo& track, const std::vector<uint8_t>& sampleData) {
    // PCM doesn't need complex configuration
    // Just ensure we have sample rate, channel count, and bit depth
    
    if (track.sampleRate == 0) {
        // Try to infer sample rate from context or use default
        track.sampleRate = 44100; // Common default
        LogError("PCMConfig", "Using default 44.1kHz sample rate for PCM");
    }
    
    if (track.channelCount == 0) {
        // Try to infer channel count from context or use default
        track.channelCount = 2; // Stereo default
        LogError("PCMConfig", "Using default stereo channel configuration for PCM");
    }
    
    if (track.bitsPerSample == 0) {
        // Try to infer bit depth from context or use default
        track.bitsPerSample = 16; // Common default
        LogError("PCMConfig", "Using default 16-bit depth for PCM");
    }
    
    return true;
}

bool ErrorRecovery::RetryIOOperation(std::function<bool()> operation, int maxRetries) {
    int attempts = 0;
    bool success = false;
    
    while (attempts < maxRetries) {
        success = operation();
        if (success) {
            break;
        }
        
        // Log retry attempt
        LogError("IORetry", "Retry attempt " + std::to_string(attempts + 1) + 
                " of " + std::to_string(maxRetries));
        
        // Exponential backoff
        int delayMs = static_cast<int>(100 * std::pow(BACKOFF_MULTIPLIER, attempts));
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        
        attempts++;
    }
    
    if (!success) {
        LogError("IORetry", "Operation failed after " + std::to_string(maxRetries) + " attempts");
    }
    
    return success;
}

void ErrorRecovery::LogError(const std::string& errorType, const std::string& message, uint32_t boxType) {
    // Increment error count for this type
    errorStats[errorType]++;
    
    // In a real implementation, this would log to a file or debug output
    // For now, we'll just track the statistics
    
    // Debug output could be added here
    // fprintf(stderr, "[%s] %s%s%s\n", errorType.c_str(), message.c_str(),
    //         boxType ? " (box: " : "", boxType ? BoxTypeToString(boxType).c_str() : "");
}

std::map<std::string, int> ErrorRecovery::GetErrorStats() const {
    return errorStats;
}

void ErrorRecovery::ResetErrorStats() {
    errorStats.clear();
}

uint32_t ErrorRecovery::EstimateReasonableBoxSize(uint32_t boxType, uint64_t containerSize) {
    // Estimate a reasonable size for common box types
    switch (boxType) {
        case BOX_FTYP:
            return 32; // Typically small
            
        case BOX_MOOV:
            return std::min<uint64_t>(containerSize, 10 * 1024 * 1024); // Up to 10 MB
            
        case BOX_MDAT:
            return std::min<uint64_t>(containerSize, containerSize - 16); // Most of container
            
        case BOX_TRAK:
            return std::min<uint64_t>(containerSize, 1 * 1024 * 1024); // Up to 1 MB
            
        case BOX_STBL:
            return std::min<uint64_t>(containerSize, 1 * 1024 * 1024); // Up to 1 MB
            
        case BOX_STSD:
            return std::min<uint64_t>(containerSize, 1024); // Typically small
            
        case BOX_STTS:
        case BOX_STSC:
        case BOX_STSZ:
        case BOX_STCO:
        case BOX_CO64:
            return std::min<uint64_t>(containerSize, 100 * 1024); // Up to 100 KB
            
        default:
            // For unknown box types, use a conservative estimate
            return std::min<uint64_t>(containerSize, 4096); // 4 KB default
    }
}

bool ErrorRecovery::IsKnownBoxType(uint32_t boxType) {
    // Check if this is a known box type
    switch (boxType) {
        // Core structure
        case BOX_FTYP:
        case BOX_MOOV:
        case BOX_MDAT:
        case BOX_FREE:
        case BOX_SKIP:
        case BOX_WIDE:
        case BOX_PNOT:
            
        // Movie box children
        case BOX_MVHD:
        case BOX_TRAK:
        case BOX_UDTA:
        case BOX_META:
        case BOX_IODS:
            
        // Track box children
        case BOX_TKHD:
        case BOX_TREF:
        case BOX_EDTS:
        case BOX_MDIA:
            
        // Edit box children
        case BOX_ELST:
            
        // Media box children
        case BOX_MDHD:
        case BOX_HDLR:
        case BOX_MINF:
            
        // Media information box children
        case BOX_VMHD:
        case BOX_SMHD:
        case BOX_HMHD:
        case BOX_NMHD:
        case BOX_DINF:
        case BOX_STBL:
            
        // Data information box children
        case BOX_DREF:
        case BOX_URL:
        case BOX_URN:
            
        // Sample table box children
        case BOX_STSD:
        case BOX_STTS:
        case BOX_CTTS:
        case BOX_STSC:
        case BOX_STSZ:
        case BOX_STZ2:
        case BOX_STCO:
        case BOX_CO64:
        case BOX_STSS:
        case BOX_STSH:
        case BOX_PADB:
        case BOX_STDP:
            
        // Fragmented MP4 boxes
        case BOX_MOOF:
        case BOX_MFHD:
        case BOX_TRAF:
        case BOX_TFHD:
        case BOX_TRUN:
        case BOX_TFDT:
        case BOX_MFRA:
        case BOX_TFRA:
        case BOX_MFRO:
        case BOX_SIDX:
            
        // Metadata boxes
        case BOX_ILST:
        case BOX_KEYS:
        case BOX_DATA:
        case BOX_MEAN:
        case BOX_NAME:
            
        // iTunes metadata atoms
        case BOX_TITLE:
        case BOX_ARTIST:
        case BOX_ALBUM:
        case BOX_DATE:
        case BOX_GENRE:
        case BOX_TRACK:
        case BOX_DISK:
        case BOX_COVR:
            return true;
            
        default:
            // Check if it's a valid 4CC (printable ASCII)
            char c1 = (boxType >> 24) & 0xFF;
            char c2 = (boxType >> 16) & 0xFF;
            char c3 = (boxType >> 8) & 0xFF;
            char c4 = boxType & 0xFF;
            
            bool isPrintable = 
                (c1 >= 32 && c1 <= 126) &&
                (c2 >= 32 && c2 <= 126) &&
                (c3 >= 32 && c3 <= 126) &&
                (c4 >= 32 && c4 <= 126);
            
            return isPrintable;
    }
}

std::string ErrorRecovery::BoxTypeToString(uint32_t boxType) {
    std::string result(4, ' ');
    result[0] = (boxType >> 24) & 0xFF;
    result[1] = (boxType >> 16) & 0xFF;
    result[2] = (boxType >> 8) & 0xFF;
    result[3] = boxType & 0xFF;
    
    // Replace non-printable characters with '?'
    for (int i = 0; i < 4; i++) {
        if (result[i] < 32 || result[i] > 126) {
            result[i] = '?';
        }
    }
    
    return result;
}