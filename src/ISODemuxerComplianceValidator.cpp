/*
 * ISODemuxerComplianceValidator.cpp - ISO/IEC 14496-12 standards compliance validation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

ISODemuxerComplianceValidator::ISODemuxerComplianceValidator(std::shared_ptr<IOHandler> ioHandler)
    : m_ioHandler(ioHandler), m_complianceLevel("strict"), 
      m_hasFileTypeBox(false), m_hasMovieBox(false), m_hasMediaDataBox(false) {
    // Initialize compliance tracking
    m_warnings.clear();
    m_errors.clear();
    m_boxCounts.clear();
    m_encounteredBoxTypes.clear();
}

ISODemuxerComplianceValidator::~ISODemuxerComplianceValidator() {
    // Cleanup handled by smart pointers
}

BoxSizeValidationResult ISODemuxerComplianceValidator::ValidateBoxStructure(uint32_t boxType, 
                                                                           uint64_t boxSize, 
                                                                           uint64_t boxOffset, 
                                                                           uint64_t containerSize) {
    BoxSizeValidationResult result;
    result.actualSize = boxSize;
    result.is64BitSize = false;
    result.isValid = true;
    
    // Track box type occurrence
    m_boxCounts[boxType]++;
    m_encounteredBoxTypes.insert(boxType);
    
    // Check for 64-bit size indicator (size == 1 in 32-bit header)
    if (boxSize == 1) {
        result.is64BitSize = true;
        
        // For 64-bit sizes, we need to read the actual size from the extended header
        if (!m_ioHandler) {
            result.isValid = false;
            result.errorMessage = "No IOHandler available for 64-bit size validation";
            AddError("Cannot validate 64-bit box size: " + BoxTypeToString(boxType));
            return result;
        }
        
        // Read the 64-bit size from offset + 8
        uint8_t sizeBytes[8];
        if (m_ioHandler->seek(static_cast<long>(boxOffset + 8), SEEK_SET) != 0 ||
            m_ioHandler->read(sizeBytes, 1, 8) != 8) {
            result.isValid = false;
            result.errorMessage = "Failed to read 64-bit box size";
            AddError("Cannot read 64-bit size for box: " + BoxTypeToString(boxType));
            return result;
        }
        
        // Convert to 64-bit size (big-endian)
        result.actualSize = (static_cast<uint64_t>(sizeBytes[0]) << 56) |
                           (static_cast<uint64_t>(sizeBytes[1]) << 48) |
                           (static_cast<uint64_t>(sizeBytes[2]) << 40) |
                           (static_cast<uint64_t>(sizeBytes[3]) << 32) |
                           (static_cast<uint64_t>(sizeBytes[4]) << 24) |
                           (static_cast<uint64_t>(sizeBytes[5]) << 16) |
                           (static_cast<uint64_t>(sizeBytes[6]) << 8) |
                           static_cast<uint64_t>(sizeBytes[7]);
        
        // Validate 64-bit size
        if (!Validate64BitBoxSize(result.actualSize, boxOffset, containerSize)) {
            result.isValid = false;
            result.errorMessage = "Invalid 64-bit box size";
            return result;
        }
    } else {
        // Standard 32-bit size
        if (!Validate32BitBoxSize(static_cast<uint32_t>(boxSize), boxOffset, containerSize)) {
            result.isValid = false;
            result.errorMessage = "Invalid 32-bit box size";
            return result;
        }
    }
    
    // Validate against specification limits
    if (!ValidateBoxSizeLimits(result.actualSize, boxType)) {
        result.isValid = false;
        result.errorMessage = "Box size exceeds specification limits";
        AddError("Box size exceeds limits for " + BoxTypeToString(boxType) + 
                ": " + std::to_string(result.actualSize) + " bytes");
    }
    
    // Check if 64-bit size is necessary
    if (result.is64BitSize && result.actualSize < 0xFFFFFFFF) {
        if (m_complianceLevel == "strict") {
            AddWarning("Unnecessary 64-bit size for box " + BoxTypeToString(boxType) + 
                      " (size: " + std::to_string(result.actualSize) + " bytes)");
        }
    }
    
    return result;
}

bool ISODemuxerComplianceValidator::Validate32BitBoxSize(uint32_t boxSize, 
                                                        uint64_t boxOffset, 
                                                        uint64_t containerSize) {
    // Requirement 12.2: Support for 32-bit box sizes
    
    // Minimum box size is 8 bytes (4-byte size + 4-byte type)
    if (boxSize < 8 && boxSize != 0 && boxSize != 1) {
        AddError("Invalid 32-bit box size: " + std::to_string(boxSize) + " (minimum is 8 bytes)");
        return false;
    }
    
    // Size of 0 means box extends to end of container
    if (boxSize == 0) {
        if (containerSize == 0) {
            AddWarning("Box with size 0 in container with unknown size");
        }
        return true; // Valid according to ISO specification
    }
    
    // Size of 1 indicates 64-bit size follows
    if (boxSize == 1) {
        return true; // Will be validated as 64-bit size
    }
    
    // Check if box fits within container
    if (containerSize > 0 && boxOffset + boxSize > containerSize) {
        AddError("32-bit box extends beyond container boundary");
        return false;
    }
    
    return true;
}

bool ISODemuxerComplianceValidator::Validate64BitBoxSize(uint64_t boxSize, 
                                                        uint64_t boxOffset, 
                                                        uint64_t containerSize) {
    // Requirement 12.2: Support for 64-bit box sizes
    
    // Minimum 64-bit box size is 16 bytes (4-byte size=1 + 4-byte type + 8-byte extended size)
    if (boxSize < 16) {
        AddError("Invalid 64-bit box size: " + std::to_string(boxSize) + " (minimum is 16 bytes)");
        return false;
    }
    
    // 64-bit sizes should only be used when necessary (> 4GB - 1)
    if (boxSize <= 0xFFFFFFFE && m_complianceLevel == "strict") {
        AddWarning("64-bit size used unnecessarily for size: " + std::to_string(boxSize));
    }
    
    // Check if box fits within container
    if (containerSize > 0 && boxOffset + boxSize > containerSize) {
        AddError("64-bit box extends beyond container boundary");
        return false;
    }
    
    // Check for reasonable maximum size (avoid potential overflow issues)
    const uint64_t MAX_REASONABLE_SIZE = 1ULL << 50; // 1 PB
    if (boxSize > MAX_REASONABLE_SIZE) {
        AddError("64-bit box size unreasonably large: " + std::to_string(boxSize) + " bytes");
        return false;
    }
    
    return true;
}

TimestampValidationResult ISODemuxerComplianceValidator::ValidateTimestampConfiguration(uint64_t timestamp, 
                                                                                       uint32_t timescale, 
                                                                                       uint64_t duration) {
    // Requirement 12.3: Validate timestamp handling and timescale configurations
    
    TimestampValidationResult result;
    result.isValid = true;
    result.hasValidTimescale = ValidateTimescaleValue(timescale);
    result.normalizedTimestamp = timestamp;
    
    if (!result.hasValidTimescale) {
        result.isValid = false;
        result.errorMessage = "Invalid timescale value: " + std::to_string(timescale);
        AddError("Invalid timescale: " + std::to_string(timescale));
        return result;
    }
    
    // Validate timestamp is within duration bounds
    if (duration > 0 && timestamp > duration) {
        result.isValid = false;
        result.errorMessage = "Timestamp exceeds track duration";
        AddError("Timestamp " + std::to_string(timestamp) + " exceeds duration " + std::to_string(duration));
        return result;
    }
    
    // Check for potential overflow in timestamp calculations
    if (timescale > 0) {
        // Calculate maximum safe timestamp to avoid overflow in millisecond conversion
        const uint64_t MAX_SAFE_TIMESTAMP = UINT64_MAX / 1000;
        if (timestamp > MAX_SAFE_TIMESTAMP) {
            AddWarning("Timestamp may cause overflow in time calculations: " + std::to_string(timestamp));
        }
        
        // Normalize timestamp to milliseconds for validation
        result.normalizedTimestamp = (timestamp * 1000ULL) / timescale;
    }
    
    // Validate reasonable timestamp range (not more than 1000 years)
    const uint64_t MAX_REASONABLE_TIME_MS = 1000ULL * 365 * 24 * 60 * 60 * 1000; // 1000 years in ms
    if (result.normalizedTimestamp > MAX_REASONABLE_TIME_MS) {
        AddWarning("Timestamp represents unreasonably long duration: " + 
                  std::to_string(result.normalizedTimestamp / 1000) + " seconds");
    }
    
    return result;
}

bool ISODemuxerComplianceValidator::ValidateSampleTableConsistency(const SampleTableInfo& sampleTableInfo) {
    // Requirement 12.8: Validate data integrity and consistency
    
    bool isConsistent = true;
    
    // Check that we have the minimum required tables
    if (sampleTableInfo.chunkOffsets.empty()) {
        AddError("Missing chunk offset table (stco/co64)");
        isConsistent = false;
    }
    
    if (sampleTableInfo.sampleToChunkEntries.empty()) {
        AddError("Missing sample-to-chunk table (stsc)");
        isConsistent = false;
    }
    
    if (sampleTableInfo.sampleSizes.empty()) {
        AddError("Missing sample size table (stsz)");
        isConsistent = false;
    }
    
    if (sampleTableInfo.sampleTimes.empty()) {
        AddError("Missing time-to-sample table (stts)");
        isConsistent = false;
    }
    
    if (!isConsistent) {
        return false; // Cannot validate consistency without required tables
    }
    
    // Validate sample-to-chunk table consistency
    for (size_t i = 0; i < sampleTableInfo.sampleToChunkEntries.size(); i++) {
        const auto& entry = sampleTableInfo.sampleToChunkEntries[i];
        
        // Check that first chunk is valid (1-based indexing)
        if (entry.firstChunk == 0) {
            AddError("Invalid first chunk index in sample-to-chunk table: 0 (should be 1-based)");
            isConsistent = false;
        }
        
        // Check that first chunk doesn't exceed available chunks
        if (entry.firstChunk > sampleTableInfo.chunkOffsets.size()) {
            AddError("First chunk index exceeds available chunks: " + std::to_string(entry.firstChunk) + 
                    " > " + std::to_string(sampleTableInfo.chunkOffsets.size()));
            isConsistent = false;
        }
        
        // Check that samples per chunk is reasonable
        if (entry.samplesPerChunk == 0) {
            AddError("Invalid samples per chunk: 0");
            isConsistent = false;
        }
        
        if (entry.samplesPerChunk > 10000) { // Reasonable upper limit
            AddWarning("Very large samples per chunk: " + std::to_string(entry.samplesPerChunk));
        }
        
        // Check ordering of first chunk values
        if (i > 0 && entry.firstChunk <= sampleTableInfo.sampleToChunkEntries[i-1].firstChunk) {
            AddError("Sample-to-chunk entries not properly ordered");
            isConsistent = false;
        }
    }
    
    // Validate that total samples calculated from sample-to-chunk matches sample size table
    uint64_t calculatedSamples = 0;
    for (size_t i = 0; i < sampleTableInfo.sampleToChunkEntries.size(); i++) {
        const auto& entry = sampleTableInfo.sampleToChunkEntries[i];
        
        uint32_t chunkCount;
        if (i + 1 < sampleTableInfo.sampleToChunkEntries.size()) {
            chunkCount = sampleTableInfo.sampleToChunkEntries[i + 1].firstChunk - entry.firstChunk;
        } else {
            chunkCount = static_cast<uint32_t>(sampleTableInfo.chunkOffsets.size()) - entry.firstChunk + 1;
        }
        
        calculatedSamples += static_cast<uint64_t>(chunkCount) * entry.samplesPerChunk;
    }
    
    if (calculatedSamples != sampleTableInfo.sampleSizes.size()) {
        AddError("Sample count mismatch: sample-to-chunk indicates " + std::to_string(calculatedSamples) + 
                " samples, but sample size table has " + std::to_string(sampleTableInfo.sampleSizes.size()));
        isConsistent = false;
    }
    
    // Validate time-to-sample table
    if (sampleTableInfo.sampleTimes.size() != sampleTableInfo.sampleSizes.size()) {
        AddError("Time table size mismatch: " + std::to_string(sampleTableInfo.sampleTimes.size()) + 
                " time entries vs " + std::to_string(sampleTableInfo.sampleSizes.size()) + " samples");
        isConsistent = false;
    }
    
    // Validate sync sample table if present
    if (!sampleTableInfo.syncSamples.empty()) {
        for (uint64_t syncSample : sampleTableInfo.syncSamples) {
            if (syncSample == 0 || syncSample > sampleTableInfo.sampleSizes.size()) {
                AddError("Invalid sync sample index: " + std::to_string(syncSample));
                isConsistent = false;
            }
        }
        
        // Check that sync samples are in ascending order
        for (size_t i = 1; i < sampleTableInfo.syncSamples.size(); i++) {
            if (sampleTableInfo.syncSamples[i] <= sampleTableInfo.syncSamples[i-1]) {
                AddError("Sync samples not in ascending order");
                isConsistent = false;
                break;
            }
        }
    }
    
    return isConsistent;
}

bool ISODemuxerComplianceValidator::ValidateCodecDataIntegrity(const std::string& codecType, 
                                                              const std::vector<uint8_t>& codecConfig,
                                                              const AudioTrackInfo& track) {
    // Requirement 12.5: Validate codec-specific data
    
    bool isValid = true;
    
    if (codecType == "aac") {
        // Validate AAC AudioSpecificConfig
        if (codecConfig.empty()) {
            AddError("Missing AAC AudioSpecificConfig");
            return false;
        }
        
        if (codecConfig.size() < 2) {
            AddError("AAC AudioSpecificConfig too short: " + std::to_string(codecConfig.size()) + " bytes");
            isValid = false;
        }
        
        // Basic validation of AudioSpecificConfig structure
        if (codecConfig.size() >= 2) {
            uint8_t audioObjectType = (codecConfig[0] >> 3) & 0x1F;
            uint8_t samplingFreqIndex = ((codecConfig[0] & 0x07) << 1) | ((codecConfig[1] >> 7) & 0x01);
            uint8_t channelConfig = (codecConfig[1] >> 3) & 0x0F;
            
            // Validate audio object type
            if (audioObjectType == 0 || audioObjectType > 31) {
                AddError("Invalid AAC audio object type: " + std::to_string(audioObjectType));
                isValid = false;
            }
            
            // Validate sampling frequency index
            if (samplingFreqIndex > 12 && samplingFreqIndex != 15) {
                AddError("Invalid AAC sampling frequency index: " + std::to_string(samplingFreqIndex));
                isValid = false;
            }
            
            // Validate channel configuration
            if (channelConfig > 7) {
                AddError("Invalid AAC channel configuration: " + std::to_string(channelConfig));
                isValid = false;
            }
        }
        
    } else if (codecType == "alac") {
        // Validate ALAC magic cookie
        if (codecConfig.empty()) {
            AddError("Missing ALAC magic cookie");
            return false;
        }
        
        if (codecConfig.size() < 24) {
            AddError("ALAC magic cookie too short: " + std::to_string(codecConfig.size()) + " bytes");
            isValid = false;
        }
        
    } else if (codecType == "ulaw" || codecType == "alaw") {
        // Validate telephony codec compliance
        if (!ValidateTelephonyCodecCompliance(track)) {
            isValid = false;
        }
        
    } else if (codecType == "lpcm") {
        // Validate PCM configuration
        if (track.bitsPerSample == 0 || track.bitsPerSample > 32) {
            AddError("Invalid PCM bits per sample: " + std::to_string(track.bitsPerSample));
            isValid = false;
        }
        
        if (track.channelCount == 0 || track.channelCount > 8) {
            AddError("Invalid PCM channel count: " + std::to_string(track.channelCount));
            isValid = false;
        }
    }
    
    // Validate sample rate consistency
    if (track.sampleRate == 0) {
        AddError("Invalid sample rate: 0");
        isValid = false;
    } else if (track.sampleRate > 192000) {
        AddWarning("Very high sample rate: " + std::to_string(track.sampleRate) + " Hz");
    }
    
    return isValid;
}

ComplianceValidationResult ISODemuxerComplianceValidator::ValidateContainerCompliance(
    const std::vector<uint8_t>& fileTypeBox,
    const std::string& containerBrand) {
    
    // Requirement 12.1: Follow ISO/IEC 14496-12 specifications
    
    ComplianceValidationResult result;
    result.isCompliant = true;
    result.complianceLevel = "compliant";
    
    if (fileTypeBox.empty()) {
        AddError("Missing file type box (ftyp)");
        result.isCompliant = false;
        result.complianceLevel = "non-compliant";
    } else {
        m_hasFileTypeBox = true;
        
        // Validate file type box structure
        if (fileTypeBox.size() < 8) {
            AddError("File type box too short: " + std::to_string(fileTypeBox.size()) + " bytes");
            result.isCompliant = false;
        } else {
            // Extract major brand
            uint32_t majorBrand = (static_cast<uint32_t>(fileTypeBox[0]) << 24) |
                                 (static_cast<uint32_t>(fileTypeBox[1]) << 16) |
                                 (static_cast<uint32_t>(fileTypeBox[2]) << 8) |
                                 static_cast<uint32_t>(fileTypeBox[3]);
            
            // Validate major brand
            std::set<uint32_t> validBrands = {
                BRAND_ISOM, BRAND_MP41, BRAND_MP42, BRAND_M4A,
                BRAND_QT, BRAND_3GP4, BRAND_3GP5, BRAND_3GP6, BRAND_3G2A
            };
            
            if (validBrands.find(majorBrand) == validBrands.end()) {
                AddWarning("Unknown major brand: " + BoxTypeToString(majorBrand));
                result.complianceLevel = "relaxed";
            }
        }
    }
    
    // Check for required top-level boxes
    if (!m_hasMovieBox) {
        AddError("Missing movie box (moov)");
        result.isCompliant = false;
        result.complianceLevel = "non-compliant";
    }
    
    // Media data box is not strictly required (can be in fragments)
    if (!m_hasMediaDataBox && m_encounteredBoxTypes.find(BOX_MOOF) == m_encounteredBoxTypes.end()) {
        AddWarning("No media data box (mdat) or fragments found");
        result.complianceLevel = "relaxed";
    }
    
    result.warnings = m_warnings;
    result.errors = m_errors;
    
    return result;
}

ComplianceValidationResult ISODemuxerComplianceValidator::ValidateTrackCompliance(const AudioTrackInfo& track) {
    ComplianceValidationResult result;
    result.isCompliant = true;
    result.complianceLevel = "compliant";
    
    // Validate track ID
    if (track.trackId == 0) {
        AddError("Invalid track ID: 0 (track IDs must be non-zero)");
        result.isCompliant = false;
    }
    
    // Validate timescale
    if (!ValidateTimescaleValue(track.timescale)) {
        result.isCompliant = false;
    }
    
    // Validate sample table consistency
    if (!ValidateSampleTableConsistency(track.sampleTableInfo)) {
        result.isCompliant = false;
    }
    
    // Validate codec data integrity
    if (!ValidateCodecDataIntegrity(track.codecType, track.codecConfig, track)) {
        result.isCompliant = false;
    }
    
    if (!result.isCompliant) {
        result.complianceLevel = "non-compliant";
    } else if (!m_warnings.empty()) {
        result.complianceLevel = "relaxed";
    }
    
    result.warnings = m_warnings;
    result.errors = m_errors;
    
    return result;
}

bool ISODemuxerComplianceValidator::ValidateEditListCompliance(const std::vector<uint8_t>& editList,
                                                              uint64_t trackDuration,
                                                              uint32_t timescale) {
    // Requirement 12.6: Apply timeline modifications per specification
    
    if (editList.empty()) {
        return true; // No edit list is valid
    }
    
    if (editList.size() < 8) {
        AddError("Edit list too short: " + std::to_string(editList.size()) + " bytes");
        return false;
    }
    
    // Parse edit list header
    uint32_t entryCount = (static_cast<uint32_t>(editList[4]) << 24) |
                         (static_cast<uint32_t>(editList[5]) << 16) |
                         (static_cast<uint32_t>(editList[6]) << 8) |
                         static_cast<uint32_t>(editList[7]);
    
    if (entryCount == 0) {
        AddWarning("Edit list with zero entries");
        return true;
    }
    
    // Each entry is 12 bytes (32-bit version) or 20 bytes (64-bit version)
    uint8_t version = editList[0];
    size_t entrySize = (version == 1) ? 20 : 12;
    size_t expectedSize = 8 + (entryCount * entrySize);
    
    if (editList.size() < expectedSize) {
        AddError("Edit list size mismatch: expected " + std::to_string(expectedSize) + 
                " bytes, got " + std::to_string(editList.size()));
        return false;
    }
    
    // Validate each edit list entry
    for (uint32_t i = 0; i < entryCount; i++) {
        size_t entryOffset = 8 + (i * entrySize);
        
        uint64_t segmentDuration, mediaTime;
        if (version == 1) {
            // 64-bit version
            segmentDuration = (static_cast<uint64_t>(editList[entryOffset]) << 56) |
                             (static_cast<uint64_t>(editList[entryOffset + 1]) << 48) |
                             (static_cast<uint64_t>(editList[entryOffset + 2]) << 40) |
                             (static_cast<uint64_t>(editList[entryOffset + 3]) << 32) |
                             (static_cast<uint64_t>(editList[entryOffset + 4]) << 24) |
                             (static_cast<uint64_t>(editList[entryOffset + 5]) << 16) |
                             (static_cast<uint64_t>(editList[entryOffset + 6]) << 8) |
                             static_cast<uint64_t>(editList[entryOffset + 7]);
            
            mediaTime = (static_cast<uint64_t>(editList[entryOffset + 8]) << 56) |
                       (static_cast<uint64_t>(editList[entryOffset + 9]) << 48) |
                       (static_cast<uint64_t>(editList[entryOffset + 10]) << 40) |
                       (static_cast<uint64_t>(editList[entryOffset + 11]) << 32) |
                       (static_cast<uint64_t>(editList[entryOffset + 12]) << 24) |
                       (static_cast<uint64_t>(editList[entryOffset + 13]) << 16) |
                       (static_cast<uint64_t>(editList[entryOffset + 14]) << 8) |
                       static_cast<uint64_t>(editList[entryOffset + 15]);
        } else {
            // 32-bit version
            segmentDuration = (static_cast<uint32_t>(editList[entryOffset]) << 24) |
                             (static_cast<uint32_t>(editList[entryOffset + 1]) << 16) |
                             (static_cast<uint32_t>(editList[entryOffset + 2]) << 8) |
                             static_cast<uint32_t>(editList[entryOffset + 3]);
            
            mediaTime = (static_cast<uint32_t>(editList[entryOffset + 4]) << 24) |
                       (static_cast<uint32_t>(editList[entryOffset + 5]) << 16) |
                       (static_cast<uint32_t>(editList[entryOffset + 6]) << 8) |
                       static_cast<uint32_t>(editList[entryOffset + 7]);
        }
        
        // Validate segment duration
        if (segmentDuration == 0) {
            AddWarning("Edit list entry " + std::to_string(i) + " has zero segment duration");
        }
        
        // Validate media time (0xFFFFFFFF means empty edit)
        if (mediaTime != 0xFFFFFFFFULL && mediaTime > trackDuration) {
            AddError("Edit list entry " + std::to_string(i) + " media time exceeds track duration");
            return false;
        }
    }
    
    return true;
}

bool ISODemuxerComplianceValidator::ValidateFragmentCompliance(const std::vector<uint8_t>& fragmentData,
                                                              uint32_t fragmentType) {
    if (fragmentData.empty()) {
        AddError("Empty fragment data for type: " + BoxTypeToString(fragmentType));
        return false;
    }
    
    // Basic fragment validation based on type
    switch (fragmentType) {
        case BOX_MOOF:
            // Movie fragment should contain at least mfhd
            if (fragmentData.size() < 16) {
                AddError("Movie fragment too short");
                return false;
            }
            break;
            
        case BOX_TRAF:
            // Track fragment should contain at least tfhd
            if (fragmentData.size() < 16) {
                AddError("Track fragment too short");
                return false;
            }
            break;
            
        case BOX_TRUN:
            // Track run should have valid sample count
            if (fragmentData.size() < 8) {
                AddError("Track run too short");
                return false;
            }
            break;
            
        default:
            AddWarning("Unknown fragment type: " + BoxTypeToString(fragmentType));
            break;
    }
    
    return true;
}

ComplianceValidationResult ISODemuxerComplianceValidator::GetComplianceReport() const {
    ComplianceValidationResult result;
    result.warnings = m_warnings;
    result.errors = m_errors;
    result.isCompliant = m_errors.empty();
    
    if (result.isCompliant) {
        result.complianceLevel = m_warnings.empty() ? "strict" : "relaxed";
    } else {
        result.complianceLevel = "non-compliant";
    }
    
    return result;
}

void ISODemuxerComplianceValidator::SetComplianceLevel(const std::string& level) {
    if (level == "strict" || level == "relaxed" || level == "permissive") {
        m_complianceLevel = level;
    } else {
        AddWarning("Unknown compliance level: " + level + ", using 'strict'");
        m_complianceLevel = "strict";
    }
}

bool ISODemuxerComplianceValidator::IsRequiredBox(uint32_t boxType, uint32_t containerType) {
    // Define required boxes for different container types
    switch (containerType) {
        case BOX_MOOV:
            return (boxType == BOX_MVHD); // Movie header is required in movie box
            
        case BOX_TRAK:
            return (boxType == BOX_TKHD || boxType == BOX_MDIA); // Track header and media box required
            
        case BOX_MDIA:
            return (boxType == BOX_MDHD || boxType == BOX_HDLR || boxType == BOX_MINF); // Media header, handler, media info required
            
        case BOX_MINF:
            return (boxType == BOX_STBL); // Sample table required in media info
            
        case BOX_STBL:
            return (boxType == BOX_STSD || boxType == BOX_STTS || boxType == BOX_STSC || 
                   boxType == BOX_STSZ || boxType == BOX_STCO || boxType == BOX_CO64); // Sample tables required
            
        default:
            return false;
    }
}

bool ISODemuxerComplianceValidator::ValidateBoxNesting(uint32_t childBoxType, uint32_t parentBoxType) {
    // Define allowed box nesting according to ISO specification
    std::map<uint32_t, std::set<uint32_t>> allowedChildren = {
        {BOX_MOOV, {BOX_MVHD, BOX_TRAK, BOX_UDTA, BOX_META, BOX_IODS}},
        {BOX_TRAK, {BOX_TKHD, BOX_TREF, BOX_EDTS, BOX_MDIA}},
        {BOX_MDIA, {BOX_MDHD, BOX_HDLR, BOX_MINF}},
        {BOX_MINF, {BOX_VMHD, BOX_SMHD, BOX_HMHD, BOX_NMHD, BOX_DINF, BOX_STBL}},
        {BOX_STBL, {BOX_STSD, BOX_STTS, BOX_CTTS, BOX_STSC, BOX_STSZ, BOX_STZ2, BOX_STCO, BOX_CO64, BOX_STSS}},
        {BOX_EDTS, {BOX_ELST}},
        {BOX_DINF, {BOX_DREF}},
        {BOX_UDTA, {BOX_META}},
        {BOX_META, {BOX_HDLR, BOX_DINF, BOX_ILST, BOX_KEYS}}
    };
    
    auto it = allowedChildren.find(parentBoxType);
    if (it != allowedChildren.end()) {
        return it->second.find(childBoxType) != it->second.end();
    }
    
    // If parent type not in our list, allow nesting (permissive for unknown types)
    return true;
}

void ISODemuxerComplianceValidator::AddWarning(const std::string& message) {
    m_warnings.push_back(message);
    Debug::log("iso_compliance", "WARNING: ", message);
}

void ISODemuxerComplianceValidator::AddError(const std::string& message) {
    m_errors.push_back(message);
    Debug::log("iso_compliance", "ERROR: ", message);
}

bool ISODemuxerComplianceValidator::ValidateBoxSizeLimits(uint64_t boxSize, uint32_t boxType) {
    uint64_t maxSize = GetMaxAllowedBoxSize(boxType);
    if (maxSize > 0 && boxSize > maxSize) {
        return false;
    }
    return true;
}

bool ISODemuxerComplianceValidator::ValidateTimescaleValue(uint32_t timescale) {
    if (timescale == 0) {
        AddError("Invalid timescale: 0");
        return false;
    }
    
    // Reasonable upper limit for timescale
    if (timescale > 1000000) {
        AddWarning("Very high timescale: " + std::to_string(timescale));
    }
    
    return true;
}

bool ISODemuxerComplianceValidator::ValidateTelephonyCodecCompliance(const AudioTrackInfo& track) {
    bool isCompliant = true;
    
    // Validate sample rate for telephony codecs
    if (track.codecType == "ulaw" || track.codecType == "alaw") {
        // Standard telephony sample rates
        std::set<uint32_t> validTelephonyRates = {8000, 16000, 32000, 48000};
        if (validTelephonyRates.find(track.sampleRate) == validTelephonyRates.end()) {
            AddWarning("Non-standard sample rate for telephony codec " + track.codecType + 
                      ": " + std::to_string(track.sampleRate) + " Hz");
        }
        
        // Validate bits per sample (should be 8 for companded audio)
        if (track.bitsPerSample != 8) {
            AddError("Invalid bits per sample for " + track.codecType + 
                    ": " + std::to_string(track.bitsPerSample) + " (should be 8)");
            isCompliant = false;
        }
        
        // Validate channel count (typically mono for telephony)
        if (track.channelCount != 1) {
            AddWarning("Non-standard channel count for telephony codec: " + 
                      std::to_string(track.channelCount) + " (typically mono)");
        }
    }
    
    return isCompliant;
}

std::string ISODemuxerComplianceValidator::BoxTypeToString(uint32_t boxType) {
    char str[5];
    str[0] = static_cast<char>((boxType >> 24) & 0xFF);
    str[1] = static_cast<char>((boxType >> 16) & 0xFF);
    str[2] = static_cast<char>((boxType >> 8) & 0xFF);
    str[3] = static_cast<char>(boxType & 0xFF);
    str[4] = '\0';
    
    // Handle non-printable characters
    for (int i = 0; i < 4; i++) {
        if (str[i] < 32 || str[i] > 126) {
            str[i] = '?';
        }
    }
    
    return std::string(str);
}

uint64_t ISODemuxerComplianceValidator::GetMaxAllowedBoxSize(uint32_t boxType) {
    // Define reasonable maximum sizes for different box types
    switch (boxType) {
        case BOX_MDAT:
            return UINT64_MAX; // Media data can be very large
        case BOX_STSZ:
        case BOX_STTS:
        case BOX_STSC:
            return 100 * 1024 * 1024; // Sample tables can be large but reasonable
        default:
            return 0; // No specific limit
    }
}

bool ISODemuxerComplianceValidator::Supports64BitSize(uint32_t boxType) {
    // All box types support 64-bit sizes according to ISO specification
    // Some boxes are more likely to need them
    switch (boxType) {
        case BOX_MDAT:
        case BOX_STCO: // When using co64 variant
        case BOX_STSZ:
        case BOX_STTS:
        case BOX_STSC:
            return true;
        default:
            return true; // ISO spec allows 64-bit for all boxes
    }
}

bool ISODemuxerComplianceValidator::ValidateSampleDescriptionCompliance(
    const std::vector<uint8_t>& sampleDescription,
    const std::string& codecType) {
    
    if (sampleDescription.empty()) {
        AddError("Empty sample description for codec: " + codecType);
        return false;
    }
    
    // Minimum sample description size is 16 bytes (sample entry header)
    if (sampleDescription.size() < 16) {
        AddError("Sample description too short: " + std::to_string(sampleDescription.size()) + " bytes");
        return false;
    }
    
    // Validate sample entry header structure
    // Bytes 0-5: reserved (should be 0)
    for (int i = 0; i < 6; i++) {
        if (sampleDescription[i] != 0) {
            AddWarning("Non-zero reserved byte in sample description at position " + std::to_string(i));
        }
    }
    
    // Bytes 6-7: data reference index (should be 1 or higher)
    uint16_t dataRefIndex = (static_cast<uint16_t>(sampleDescription[6]) << 8) | 
                           static_cast<uint16_t>(sampleDescription[7]);
    if (dataRefIndex == 0) {
        AddError("Invalid data reference index: 0");
        return false;
    }
    
    // Audio sample entry specific validation (bytes 8-15 are audio-specific)
    if (sampleDescription.size() >= 16) {
        // Bytes 8-9: version (should be 0 for basic audio)
        uint16_t version = (static_cast<uint16_t>(sampleDescription[8]) << 8) | 
                          static_cast<uint16_t>(sampleDescription[9]);
        if (version > 2) {
            AddWarning("High audio sample entry version: " + std::to_string(version));
        }
        
        // Bytes 10-15: reserved (should be 0)
        for (int i = 10; i < 16; i++) {
            if (sampleDescription[i] != 0) {
                AddWarning("Non-zero reserved byte in audio sample entry at position " + std::to_string(i));
            }
        }
    }
    
    return true;
}

bool ISODemuxerComplianceValidator::CheckRequiredBoxes(uint32_t containerType, 
                                                      const std::set<uint32_t>& childBoxTypes) {
    bool hasAllRequired = true;
    
    switch (containerType) {
        case BOX_MOOV:
            if (childBoxTypes.find(BOX_MVHD) == childBoxTypes.end()) {
                AddError("Missing required movie header (mvhd) in movie box");
                hasAllRequired = false;
            }
            break;
            
        case BOX_TRAK:
            if (childBoxTypes.find(BOX_TKHD) == childBoxTypes.end()) {
                AddError("Missing required track header (tkhd) in track box");
                hasAllRequired = false;
            }
            if (childBoxTypes.find(BOX_MDIA) == childBoxTypes.end()) {
                AddError("Missing required media box (mdia) in track box");
                hasAllRequired = false;
            }
            break;
            
        case BOX_MDIA:
            if (childBoxTypes.find(BOX_MDHD) == childBoxTypes.end()) {
                AddError("Missing required media header (mdhd) in media box");
                hasAllRequired = false;
            }
            if (childBoxTypes.find(BOX_HDLR) == childBoxTypes.end()) {
                AddError("Missing required handler reference (hdlr) in media box");
                hasAllRequired = false;
            }
            if (childBoxTypes.find(BOX_MINF) == childBoxTypes.end()) {
                AddError("Missing required media information (minf) in media box");
                hasAllRequired = false;
            }
            break;
            
        case BOX_MINF:
            if (childBoxTypes.find(BOX_STBL) == childBoxTypes.end()) {
                AddError("Missing required sample table (stbl) in media information box");
                hasAllRequired = false;
            }
            // Should have one of the media header types
            if (childBoxTypes.find(BOX_SMHD) == childBoxTypes.end() &&
                childBoxTypes.find(BOX_VMHD) == childBoxTypes.end() &&
                childBoxTypes.find(BOX_HMHD) == childBoxTypes.end() &&
                childBoxTypes.find(BOX_NMHD) == childBoxTypes.end()) {
                AddError("Missing required media header in media information box");
                hasAllRequired = false;
            }
            break;
            
        case BOX_STBL:
            if (childBoxTypes.find(BOX_STSD) == childBoxTypes.end()) {
                AddError("Missing required sample description (stsd) in sample table");
                hasAllRequired = false;
            }
            if (childBoxTypes.find(BOX_STTS) == childBoxTypes.end()) {
                AddError("Missing required time-to-sample (stts) in sample table");
                hasAllRequired = false;
            }
            if (childBoxTypes.find(BOX_STSC) == childBoxTypes.end()) {
                AddError("Missing required sample-to-chunk (stsc) in sample table");
                hasAllRequired = false;
            }
            if (childBoxTypes.find(BOX_STSZ) == childBoxTypes.end() &&
                childBoxTypes.find(BOX_STZ2) == childBoxTypes.end()) {
                AddError("Missing required sample size table (stsz or stz2) in sample table");
                hasAllRequired = false;
            }
            if (childBoxTypes.find(BOX_STCO) == childBoxTypes.end() &&
                childBoxTypes.find(BOX_CO64) == childBoxTypes.end()) {
                AddError("Missing required chunk offset table (stco or co64) in sample table");
                hasAllRequired = false;
            }
            break;
            
        default:
            // No specific requirements for other container types
            break;
    }
    
    return hasAllRequired;
}