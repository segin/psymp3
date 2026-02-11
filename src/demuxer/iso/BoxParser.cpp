/*
 * BoxParser.cpp - ISO box structure parser implementation
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

static const uint32_t MAX_SAMPLES_PER_TRACK = 10000000; // 10 million samples

BoxParser::BoxParser(std::shared_ptr<IOHandler> io) : io(io), fileSize(0) {
    // Get file size for validation
    if (io) {
        io->seek(0, SEEK_END);
        fileSize = static_cast<uint64_t>(io->tell());
        io->seek(0, SEEK_SET);
    }
}

BoxHeader BoxParser::ReadBoxHeader(uint64_t offset) {
    BoxHeader header = {};
    
    if (!io || offset >= fileSize) {
        return header;
    }
    
    // Seek to box position with error handling
    if (io->seek(static_cast<long>(offset), SEEK_SET) != 0) {
        // I/O error during seek
        return header;
    }
    
    // Read basic box header (8 bytes minimum)
    if (offset + 8 > fileSize) {
        return header;
    }
    
    // Read size (4 bytes, big-endian) with error handling
    uint32_t size32 = ReadUInt32BE(offset);
    if (size32 == 0 && offset + 8 < fileSize) {
        // Potential corruption - size should not be 0 unless at end of file
        // This will be handled by validation later
    }
    
    // Read type (4 bytes, big-endian)
    header.type = ReadUInt32BE(offset + 4);
    
    // Handle extended size
    if (size32 == 1) {
        // Extended size - read 8 more bytes
        if (offset + 16 > fileSize) {
            return header;
        }
        header.size = ReadUInt64BE(offset + 8);
        header.dataOffset = offset + 16;
        header.extendedSize = true; // Mark as extended size
        
        // Validate extended size
        if (header.size < 16) {
            // Invalid extended size - must be at least 16 bytes
            header.size = 0; // Mark as invalid
            return header;
        }
    } else if (size32 == 0) {
        // Size extends to end of file
        header.size = fileSize - offset;
        header.dataOffset = offset + 8;
        header.extendedSize = false;
    } else {
        // Normal size
        header.size = size32;
        header.dataOffset = offset + 8;
        header.extendedSize = false;
        
        // Validate normal size
        if (header.size < 8) {
            // Invalid size - must be at least 8 bytes
            header.size = 0; // Mark as invalid
            return header;
        }
    }
    
    return header;
}

bool BoxParser::ValidateBoxSize(const BoxHeader& header, uint64_t containerSize) {
    // Check if box size is valid
    if (header.size == 0) {
        return false;
    }
    
    // Check minimum header size
    uint64_t headerSize = header.isExtendedSize() ? 16 : 8;
    if (header.size < headerSize) {
        return false;
    }
    
    // Check if box fits within container
    if (header.size > containerSize) {
        return false;
    }
    
    // Check if box extends beyond file
    uint64_t boxStart = header.dataOffset - headerSize;
    if (boxStart + header.size > fileSize) {
        return false;
    }
    
    // Additional validation for reasonable box sizes
    if (header.size > fileSize) {
        return false;
    }
    
    // Check for extremely large boxes that might indicate corruption
    if (header.size > 1024 * 1024 * 1024) { // 1GB limit
        // Very large box - could be corruption
        // Allow it but flag for potential recovery
        return true; // Let higher-level code handle this
    }
    
    return true;
}

bool BoxParser::ParseBoxRecursively(uint64_t offset, uint64_t size, 
                                   std::function<bool(const BoxHeader&, uint64_t)> handler) {
    uint64_t currentOffset = offset;
    uint64_t endOffset = offset + size;
    uint32_t boxCount = 0;
    const uint32_t MAX_BOXES_PER_CONTAINER = 10000; // Prevent runaway parsing
    
    while (currentOffset < endOffset && boxCount < MAX_BOXES_PER_CONTAINER) {
        // Safety check for offset alignment
        if (currentOffset >= fileSize) {
            break;
        }
        
        // Read box header with error handling
        BoxHeader header = ReadBoxHeader(currentOffset);
        
        if (header.type == 0 || header.size == 0) {
            // Invalid box - attempt to skip to next potential box
            // Look for next valid box signature
            bool foundNextBox = false;
            for (uint64_t searchOffset = currentOffset + 4; 
                 searchOffset < endOffset - 8 && searchOffset < currentOffset + 1024; 
                 searchOffset += 4) {
                
                BoxHeader testHeader = ReadBoxHeader(searchOffset);
                if (testHeader.type != 0 && testHeader.size >= 8 && 
                    ValidateBoxSize(testHeader, endOffset - searchOffset)) {
                    currentOffset = searchOffset;
                    foundNextBox = true;
                    break;
                }
            }
            
            if (!foundNextBox) {
                // No valid box found, skip remaining data
                break;
            }
            continue;
        }
        
        // Validate box size with error recovery (Requirement 7.2)
        if (!ValidateBoxSize(header, endOffset - currentOffset)) {
            // Invalid box size - attempt recovery (Requirement 7.2)
            uint64_t containerSize = endOffset - currentOffset;
            
            // Try to estimate a reasonable size
            if (header.size > containerSize) {
                // Box claims to be larger than container - truncate (Requirement 7.2)
                header.size = containerSize;
            } else if (header.size < 8) {
                // Box too small - skip it (Requirement 7.1)
                currentOffset += 8;
                boxCount++;
                continue;
            }
            
            // If still invalid after recovery, skip (Requirement 7.1)
            if (!ValidateBoxSize(header, containerSize)) {
                currentOffset += 8; // Skip at least the basic header
                boxCount++;
                continue;
            }
        }
        
        // Call handler for this box with exception handling
        bool handlerResult = false;
        try {
            handlerResult = handler(header, currentOffset);
        } catch (const std::exception& e) {
            // Handle exceptions during box parsing (Requirement 7.8)
            handlerResult = false;
            std::string errorMsg = "Exception during box parsing: ";
            errorMsg += e.what();
            // Log error through error recovery system if available
            // This will be handled by the demuxer's error recovery component
        } catch (...) {
            // Handle any other exceptions (Requirement 7.8)
            handlerResult = false;
            // Log unknown exception
        }
        
        if (!handlerResult) {
            // Handler failed, but continue parsing other boxes
            // This implements graceful degradation (Requirement 7.1)
            // Skip damaged sections and continue (Requirement 7.1)
        }
        
        // Move to next box
        uint64_t nextOffset = currentOffset + header.size;
        
        // Prevent infinite loops and validate progression
        if (nextOffset <= currentOffset || header.size < 8) {
            // Invalid progression - skip ahead
            currentOffset += 8;
        } else {
            currentOffset = nextOffset;
        }
        
        boxCount++;
        
        // Safety check to prevent runaway parsing
        if (currentOffset > endOffset) {
            break;
        }
    }
    
    if (boxCount >= MAX_BOXES_PER_CONTAINER) {
        // Too many boxes - possible corruption or infinite loop
        // This is a safety measure
    }
    
    return true;
}

bool BoxParser::IsContainerBox(uint32_t boxType) {
    switch (boxType) {
        case BOX_MOOV:
        case BOX_TRAK:
        case BOX_MDIA:
        case BOX_MINF:
        case BOX_STBL:
        case BOX_EDTS:
        case BOX_DINF:
        case BOX_UDTA:
        case BOX_META:
        case BOX_ILST:
        case BOX_MOOF:
        case BOX_TRAF:
        case BOX_MFRA:
            return true;
        default:
            return false;
    }
}

uint32_t BoxParser::ReadUInt32BE(uint64_t offset) {
    if (!io || offset + 4 > fileSize) {
        return 0;
    }
    
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

uint64_t BoxParser::ReadUInt64BE(uint64_t offset) {
    if (!io || offset + 8 > fileSize) {
        return 0;
    }
    
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

std::string BoxParser::BoxTypeToString(uint32_t boxType) {
    std::string result(4, '\0');
    result[0] = static_cast<char>((boxType >> 24) & 0xFF);
    result[1] = static_cast<char>((boxType >> 16) & 0xFF);
    result[2] = static_cast<char>((boxType >> 8) & 0xFF);
    result[3] = static_cast<char>(boxType & 0xFF);
    return result;
}

bool BoxParser::SkipUnknownBox(const BoxHeader& header) {
    // For unknown boxes, we simply skip them by returning true
    // The recursive parser will automatically move to the next box
    return true;
}

bool BoxParser::ParseMovieBox(uint64_t offset, uint64_t size) {
    // Parse movie box recursively to find tracks
    return ParseBoxRecursively(offset, size, [this](const BoxHeader& header, uint64_t boxOffset) {
        switch (header.type) {
            case BOX_MVHD:
                // Movie header - contains duration and timescale
                return true; // Skip for now, will implement in later tasks
            case BOX_TRAK:
                // Track box - parse for audio tracks
                return true; // Will be implemented when ParseTrackBox is expanded
            case BOX_UDTA:
                // User data - metadata
                return true; // Skip for now
            default:
                return SkipUnknownBox(header);
        }
    });
}

bool BoxParser::ParseTrackBox(uint64_t offset, uint64_t size, AudioTrackInfo& track) {
    // Parse track box recursively to extract audio track information
    bool foundAudio = false;
    SampleTableInfo sampleTables;
    
    ParseBoxRecursively(offset, size, [this, &track, &foundAudio, &sampleTables](const BoxHeader& header, uint64_t boxOffset) {
        switch (header.type) {
            case BOX_TKHD:
                // Track header - contains track ID
                if (header.size >= 32) {
                    // Skip version/flags (4 bytes) and creation/modification times (8 bytes each)
                    track.trackId = ReadUInt32BE(header.dataOffset + 20);
                }
                return true;
            case BOX_MDIA:
                // Media box - contains handler and media info
                return ParseMediaBoxWithSampleTables(boxOffset + (header.dataOffset - boxOffset), 
                                                   header.size - (header.dataOffset - boxOffset), 
                                                   track, foundAudio, sampleTables);
            default:
                return SkipUnknownBox(header);
        }
    });
    
    // If we found audio and have sample tables, store them in the track
    if (foundAudio && !sampleTables.chunkOffsets.empty()) {
        // Store sample table info in track for later use
        // This will be used by SampleTableManager::BuildSampleTables
        track.sampleTableInfo = sampleTables;
    }
    
    return foundAudio;
}

bool BoxParser::ParseSampleTableBox(uint64_t offset, uint64_t size, SampleTableInfo& tables) {
    // Parse sample table box recursively to extract all sample table atoms
    bool hasRequiredTables = false;
    bool hasStts = false, hasStsc = false, hasStsz = false, hasStco = false;
    
    ParseBoxRecursively(offset, size, [this, &tables, &hasStts, &hasStsc, &hasStsz, &hasStco](const BoxHeader& header, uint64_t boxOffset) {
        switch (header.type) {
            case BOX_STSD:
                // Sample description - already handled in track parsing
                return true;
            case BOX_STTS:
                // Time-to-sample table
                hasStts = ParseTimeToSampleBox(header.dataOffset, header.size - (header.dataOffset - boxOffset), tables);
                return hasStts;
            case BOX_STSC:
                // Sample-to-chunk table
                hasStsc = ParseSampleToChunkBox(header.dataOffset, header.size - (header.dataOffset - boxOffset), tables);
                return hasStsc;
            case BOX_STSZ:
                // Sample size table
                hasStsz = ParseSampleSizeBox(header.dataOffset, header.size - (header.dataOffset - boxOffset), tables);
                return hasStsz;
            case BOX_STCO:
                // Chunk offset table (32-bit)
                hasStco = ParseChunkOffsetBox(header.dataOffset, header.size - (header.dataOffset - boxOffset), tables, false);
                return hasStco;
            case BOX_CO64:
                // Chunk offset table (64-bit)
                hasStco = ParseChunkOffsetBox(header.dataOffset, header.size - (header.dataOffset - boxOffset), tables, true);
                return hasStco;
            case BOX_STSS:
                // Sync sample table (keyframes)
                return ParseSyncSampleBox(header.dataOffset, header.size - (header.dataOffset - boxOffset), tables);
            case BOX_CTTS:
                // Composition time-to-sample (for video, skip for audio)
                return true;
            default:
                return SkipUnknownBox(header);
        }
    });
    
    // Validate that we have all required sample tables
    hasRequiredTables = hasStts && hasStsc && hasStsz && hasStco;
    
    if (!hasRequiredTables) {
        // Log missing tables for debugging
        if (!hasStts) { /* Missing time-to-sample table */ }
        if (!hasStsc) { /* Missing sample-to-chunk table */ }
        if (!hasStsz) { /* Missing sample size table */ }
        if (!hasStco) { /* Missing chunk offset table */ }
    }
    
    return hasRequiredTables;
}

bool BoxParser::ParseFileTypeBox(uint64_t offset, uint64_t size, std::string& containerType) {
    if (size < 8) {
        return false;
    }
    
    // Read major brand (4 bytes)
    uint32_t majorBrand = ReadUInt32BE(offset);
    
    // Identify container type based on major brand
    switch (majorBrand) {
        case BRAND_ISOM:
            containerType = "MP4";
            break;
        case BRAND_MP41:
        case BRAND_MP42:
            containerType = "MP4";
            break;
        case BRAND_M4A:
            containerType = "M4A";
            break;
        case BRAND_QT:
            containerType = "MOV";
            break;
        case BRAND_3GP4:
        case BRAND_3GP5:
        case BRAND_3GP6:
            containerType = "3GP";
            break;
        case BRAND_3G2A:
            containerType = "3G2";
            break;
        default:
            // Try to identify from compatible brands
            containerType = "MP4"; // Default fallback
            break;
    }
    
    return true;
}

bool BoxParser::ParseMediaBox(uint64_t offset, uint64_t size, AudioTrackInfo& track, bool& foundAudio) {
    SampleTableInfo dummyTables; // Not used in this version
    return ParseMediaBoxWithSampleTables(offset, size, track, foundAudio, dummyTables);
}

bool BoxParser::ParseMediaBoxWithSampleTables(uint64_t offset, uint64_t size, AudioTrackInfo& track, bool& foundAudio, SampleTableInfo& sampleTables) {
    std::string handlerType;
    bool handlerParsed = false;
    
    return ParseBoxRecursively(offset, size, [this, &track, &foundAudio, &handlerType, &handlerParsed, &sampleTables](const BoxHeader& header, uint64_t boxOffset) {
        switch (header.type) {
            case BOX_MDHD:
                // Media header - contains timescale and duration
                if (header.size >= 32) {
                    // Read version to determine header format
                    uint8_t version = 0;
                    io->seek(header.dataOffset, SEEK_SET);
                    if (io->read(&version, 1, 1) == 1) {
                        if (version == 1) {
                            // Version 1 - 64-bit times
                            if (header.size >= 44) {
                                track.timescale = ReadUInt32BE(header.dataOffset + 28);
                                track.duration = ReadUInt64BE(header.dataOffset + 32);
                            }
                        } else {
                            // Version 0 - 32-bit times
                            track.timescale = ReadUInt32BE(header.dataOffset + 20);
                            track.duration = ReadUInt32BE(header.dataOffset + 24);
                        }
                    }
                }
                return true;
            case BOX_HDLR:
                // Handler reference - identifies track type
                handlerParsed = ParseHandlerBox(header.dataOffset, header.size - (header.dataOffset - boxOffset), handlerType);
                return handlerParsed;
            case BOX_MINF:
                // Media information - only process if this is an audio track
                if (handlerParsed && handlerType == "soun") {
                    foundAudio = true;
                    return ParseBoxRecursively(header.dataOffset, header.size - (header.dataOffset - boxOffset),
                        [this, &track, &sampleTables](const BoxHeader& minfHeader, uint64_t minfOffset) {
                            switch (minfHeader.type) {
                                case BOX_SMHD:
                                    // Sound media header - confirms this is audio
                                    return true;
                                case BOX_DINF:
                                    // Data information - skip for now
                                    return true;
                                case BOX_STBL:
                                    // Sample table box - parse for codec information and sample tables
                                    return ParseBoxRecursively(minfHeader.dataOffset, minfHeader.size - (minfHeader.dataOffset - minfOffset),
                                        [this, &track, &sampleTables](const BoxHeader& stblHeader, uint64_t stblOffset) {
                                            if (stblHeader.type == BOX_STSD) {
                                                // Sample description - contains codec information
                                                return ParseSampleDescriptionBox(stblHeader.dataOffset,
                                                                                stblHeader.size - (stblHeader.dataOffset - stblOffset),
                                                                                track);
                                            } else {
                                                // Parse sample table boxes
                                                return ParseSampleTableBox(stblHeader.dataOffset,
                                                                          stblHeader.size - (stblHeader.dataOffset - stblOffset),
                                                                          sampleTables);
                                            }
                                        });
                                default:
                                    return SkipUnknownBox(minfHeader);
                            }
                        });
                } else {
                    // Not an audio track, skip
                    return SkipUnknownBox(header);
                }
            default:
                return SkipUnknownBox(header);
        }
    });
}

bool BoxParser::ParseHandlerBox(uint64_t offset, uint64_t size, std::string& handlerType) {
    if (size < 24) {
        return false;
    }
    
    // Read version/flags to ensure proper parsing (currently not used but reserved for future)
    // uint32_t versionFlags = ReadUInt32BE(offset);
    // uint8_t version = (versionFlags >> 24) & 0xFF;
    
    // Skip version/flags (4 bytes) and pre_defined (4 bytes)
    uint32_t handler = ReadUInt32BE(offset + 8);
    
    // Identify handler type - this determines if track contains audio
    switch (handler) {
        case HANDLER_SOUN:
            handlerType = "soun";  // Audio track
            break;
        case HANDLER_VIDE:
            handlerType = "vide";  // Video track
            break;
        case HANDLER_HINT:
            handlerType = "hint";  // Hint track
            break;
        case HANDLER_META:
            handlerType = "meta";  // Metadata track
            break;
        default:
            // Convert handler to string for debugging
            handlerType = "unknown";
            char handlerStr[5] = {0};
            handlerStr[0] = (handler >> 24) & 0xFF;
            handlerStr[1] = (handler >> 16) & 0xFF;
            handlerStr[2] = (handler >> 8) & 0xFF;
            handlerStr[3] = handler & 0xFF;
            
            // Check if it's a valid ASCII string
            bool isValidAscii = true;
            for (int i = 0; i < 4; i++) {
                if (handlerStr[i] < 32 || handlerStr[i] > 126) {
                    isValidAscii = false;
                    break;
                }
            }
            
            if (isValidAscii) {
                handlerType = std::string(handlerStr);
            }
            break;
    }
    
    return true;
}

bool BoxParser::ParseSampleDescriptionBox(uint64_t offset, uint64_t size, AudioTrackInfo& track) {
    if (size < 16) {
        return false;
    }
    
    // Skip version/flags (4 bytes) and entry count (4 bytes)
    uint64_t entryOffset = offset + 8;
    uint32_t entryCount = ReadUInt32BE(offset + 4);
    
    if (entryCount == 0) {
        return false;
    }
    
    // Read first sample description entry
    if (size >= 36) { // Minimum size for audio sample entry
        uint32_t entrySize = ReadUInt32BE(entryOffset);
        uint32_t codecType = ReadUInt32BE(entryOffset + 4);
        
        // Skip reserved fields (6 bytes) and data reference index (2 bytes)
        uint64_t audioEntryOffset = entryOffset + 16;
        
        // Read audio sample entry fields (version 0)
        if (entrySize >= 36) {
            // Skip version (2 bytes) and revision level (2 bytes) and vendor (4 bytes)
            uint16_t channelCount = (ReadUInt32BE(audioEntryOffset + 8) >> 16) & 0xFFFF;
            uint16_t sampleSize = ReadUInt32BE(audioEntryOffset + 8) & 0xFFFF;
            // Skip compression ID (2 bytes) and packet size (2 bytes)
            uint32_t sampleRate = ReadUInt32BE(audioEntryOffset + 16) >> 16; // Fixed-point 16.16
            
            track.channelCount = channelCount;
            track.bitsPerSample = sampleSize;
            track.sampleRate = sampleRate;
        }
        
        // Identify codec and set track information
        switch (codecType) {
            case CODEC_AAC:
                track.codecType = "aac";
                // Look for esds box for AAC configuration
                if (entrySize > 36) {
                    // Parse additional boxes within the sample entry
                    ParseBoxRecursively(audioEntryOffset + 20, entrySize - 36,
                        [this, &track](const BoxHeader& header, uint64_t boxOffset) {
                            if (header.type == FOURCC('e','s','d','s')) {
                                // Elementary stream descriptor - contains AAC config
                                return ParseAACConfiguration(header.dataOffset, header.size - (header.dataOffset - boxOffset), track);
                            }
                            return true;
                        });
                }
                break;
            case CODEC_ALAC:
                track.codecType = "alac";
                // Look for alac box for ALAC magic cookie
                if (entrySize > 36) {
                    ParseBoxRecursively(audioEntryOffset + 20, entrySize - 36,
                        [this, &track](const BoxHeader& header, uint64_t boxOffset) {
                            if (header.type == CODEC_ALAC) {
                                // ALAC magic cookie
                                return ParseALACConfiguration(header.dataOffset,
                                                            header.size - (header.dataOffset - boxOffset),
                                                            track);
                            }
                            return true;
                        });
                }
                break;
            case CODEC_FLAC:
                track.codecType = "flac";
                // Look for dfLa box for FLAC configuration
                if (entrySize > 36) {
                    ParseBoxRecursively(audioEntryOffset + 20, entrySize - 36,
                        [this, &track](const BoxHeader& header, uint64_t boxOffset) {
                            if (header.type == FOURCC('d','f','L','a')) {
                                // FLAC configuration box
                                return ParseFLACConfiguration(header.dataOffset,
                                                            header.size - (header.dataOffset - boxOffset),
                                                            track);
                            }
                            return true;
                        });
                }
                break;
            case CODEC_ULAW:
                // Configure μ-law telephony codec (North American/Japanese standard)
                if (!ConfigureTelephonyCodec(track, "ulaw")) {
                    return false; // Configuration failed
                }
                break;
            case CODEC_ALAW:
                // Configure A-law telephony codec (European standard ITU-T G.711)
                if (!ConfigureTelephonyCodec(track, "alaw")) {
                    return false; // Configuration failed
                }
                break;
            case CODEC_LPCM:
            case CODEC_SOWT:
            case CODEC_TWOS:
            case CODEC_FL32:
            case CODEC_FL64:
            case CODEC_IN24:
            case CODEC_IN32:
                track.codecType = "pcm";
                // PCM variants - sample rate, channels, and bit depth already parsed
                // Set specific PCM subtype based on codec
                if (codecType == CODEC_FL32) {
                    track.bitsPerSample = 32;
                } else if (codecType == CODEC_FL64) {
                    track.bitsPerSample = 64;
                } else if (codecType == CODEC_IN24) {
                    track.bitsPerSample = 24;
                } else if (codecType == CODEC_IN32) {
                    track.bitsPerSample = 32;
                }
                break;
            default:
                track.codecType = "unknown";
                return false;
        }
    }
    
    return true;
}

bool BoxParser::ParseTimeToSampleBox(uint64_t offset, uint64_t size, SampleTableInfo& tables) {
    if (size < 8) {
        return false;
    }
    
    // Skip version/flags (4 bytes)
    uint32_t entryCount = ReadUInt32BE(offset + 4);
    
    if (entryCount == 0 || size < 8 + (entryCount * 8)) {
        return false;
    }
    
    // Clear existing time data
    tables.sampleTimes.clear();
    tables.sampleTimes.reserve(entryCount * 2); // Rough estimate
    
    uint64_t currentTime = 0;
    uint64_t entryOffset = offset + 8;
    
    for (uint32_t i = 0; i < entryCount; i++) {
        uint32_t sampleCount = ReadUInt32BE(entryOffset);
        uint32_t sampleDelta = ReadUInt32BE(entryOffset + 4);
        
        // Validate entry
        if (sampleCount == 0) {
            return false;
        }
        
        // SEC-01: Prevent OOM by validating total samples against limit
        if (sampleCount > MAX_SAMPLES_PER_TRACK || 
            tables.sampleTimes.size() + sampleCount > MAX_SAMPLES_PER_TRACK) {
            Debug::log("BoxParser", "Too many samples in stts box, rejecting to prevent OOM");
            return false;
        }
        
        // Add sample times for this entry
        for (uint32_t j = 0; j < sampleCount; j++) {
            tables.sampleTimes.push_back(currentTime);
            currentTime += sampleDelta;
        }
        
        entryOffset += 8;
    }
    
    return true;
}

bool BoxParser::ParseSampleToChunkBox(uint64_t offset, uint64_t size, SampleTableInfo& tables) {
    if (size < 8) {
        return false;
    }
    
    // Skip version/flags (4 bytes)
    uint32_t entryCount = ReadUInt32BE(offset + 4);
    
    if (entryCount == 0 || size < 8 + (entryCount * 12)) {
        return false;
    }
    
    // Clear existing chunk data and store raw entries
    tables.sampleToChunkEntries.clear();
    tables.sampleToChunkEntries.reserve(entryCount);
    
    uint64_t entryOffset = offset + 8;
    
    for (uint32_t i = 0; i < entryCount; i++) {
        uint32_t firstChunk = ReadUInt32BE(entryOffset);
        uint32_t samplesPerChunk = ReadUInt32BE(entryOffset + 4);
        uint32_t sampleDescIndex = ReadUInt32BE(entryOffset + 8);
        
        // Validate entry
        if (firstChunk == 0 || samplesPerChunk == 0 || sampleDescIndex == 0) {
            return false;
        }
        
        // Store the raw sample-to-chunk entry
        SampleToChunkEntry entry;
        entry.firstChunk = firstChunk - 1; // Convert to 0-based indexing
        entry.samplesPerChunk = samplesPerChunk;
        entry.sampleDescIndex = sampleDescIndex;
        tables.sampleToChunkEntries.push_back(entry);
        
        entryOffset += 12;
    }
    
    return true;
}

bool BoxParser::ParseSampleSizeBox(uint64_t offset, uint64_t size, SampleTableInfo& tables) {
    if (size < 12) {
        return false;
    }
    
    // Skip version/flags (4 bytes)
    uint32_t sampleSize = ReadUInt32BE(offset + 4);
    uint32_t sampleCount = ReadUInt32BE(offset + 8);
    
    if (sampleCount == 0) {
        return false;
    }
    
    // Clear existing size data
    tables.sampleSizes.clear();
    
    if (sampleSize != 0) {
        // All samples have the same size
        tables.sampleSizes.resize(1);
        tables.sampleSizes[0] = sampleSize;
    } else {
        // Variable sample sizes
        if (size < 12 + (sampleCount * 4)) {
            return false;
        }
        
        tables.sampleSizes.reserve(sampleCount);
        uint64_t entryOffset = offset + 12;
        
        for (uint32_t i = 0; i < sampleCount; i++) {
            uint32_t size = ReadUInt32BE(entryOffset);
            tables.sampleSizes.push_back(size);
            entryOffset += 4;
        }
    }
    
    return true;
}

bool BoxParser::ParseChunkOffsetBox(uint64_t offset, uint64_t size, SampleTableInfo& tables, bool is64Bit) {
    if (size < 8) {
        return false;
    }
    
    // Skip version/flags (4 bytes)
    uint32_t entryCount = ReadUInt32BE(offset + 4);
    
    if (entryCount == 0) {
        return false;
    }
    
    uint32_t entrySize = is64Bit ? 8 : 4;
    if (size < 8 + (entryCount * entrySize)) {
        return false;
    }
    
    // Clear existing offset data
    tables.chunkOffsets.clear();
    tables.chunkOffsets.reserve(entryCount);
    
    uint64_t entryOffset = offset + 8;
    
    for (uint32_t i = 0; i < entryCount; i++) {
        uint64_t chunkOffset;
        if (is64Bit) {
            chunkOffset = ReadUInt64BE(entryOffset);
            entryOffset += 8;
        } else {
            chunkOffset = ReadUInt32BE(entryOffset);
            entryOffset += 4;
        }
        
        // Validate offset
        if (chunkOffset >= fileSize) {
            return false;
        }
        
        tables.chunkOffsets.push_back(chunkOffset);
    }
    
    return true;
}

bool BoxParser::ParseSyncSampleBox(uint64_t offset, uint64_t size, SampleTableInfo& tables) {
    if (size < 8) {
        return false;
    }
    
    // Skip version/flags (4 bytes)
    uint32_t entryCount = ReadUInt32BE(offset + 4);
    
    if (entryCount == 0) {
        // No sync samples specified - all samples are sync samples (common for audio)
        return true;
    }
    
    if (size < 8 + (entryCount * 4)) {
        return false;
    }
    
    // Clear existing sync sample data
    tables.syncSamples.clear();
    tables.syncSamples.reserve(entryCount);
    
    uint64_t entryOffset = offset + 8;
    
    for (uint32_t i = 0; i < entryCount; i++) {
        uint32_t sampleNumber = ReadUInt32BE(entryOffset);
        
        // Validate sample number (1-based)
        if (sampleNumber == 0) {
            return false;
        }
        
        // Convert to 0-based index
        tables.syncSamples.push_back(sampleNumber - 1);
        entryOffset += 4;
    }
    
    return true;
}

bool BoxParser::ParseFragmentBox(uint64_t offset, uint64_t size) {
    // Parse fragmented MP4 boxes (moof, mfra, sidx)
    BoxHeader header = ReadBoxHeader(offset);
    
    switch (header.type) {
        case BOX_MOOF:
            // Movie fragment box - will be handled by FragmentHandler
            return true;
        case BOX_MFRA:
            // Movie fragment random access box - for seeking in fragmented files
            return true;
        case BOX_SIDX:
            // Segment index box - for streaming
            return true;
        default:
            return SkipUnknownBox(header);
    }
}

bool BoxParser::ParseAACConfiguration(uint64_t offset, uint64_t size, AudioTrackInfo& track) {
    // Placeholder for AAC configuration parsing
    // This would parse the Elementary Stream Descriptor (ESDS) box
    // to extract AAC-specific configuration data
    return true;
}

bool BoxParser::ParseALACConfiguration(uint64_t offset, uint64_t size, AudioTrackInfo& track) {
    // Placeholder for ALAC configuration parsing
    // This would parse the ALAC magic cookie to extract
    // ALAC-specific configuration data
    return true;
}

bool BoxParser::ParseFLACConfiguration(uint64_t offset, uint64_t size, AudioTrackInfo& track) {
    // Parse FLAC configuration from dfLa box (FLAC-in-MP4 specification)
    // The dfLa box contains FLAC metadata blocks without the 'fLaC' signature
    
    if (size < 4) {
        Debug::log("iso", "ISODemuxerBoxParser: dfLa box too small: ", size, " bytes");
        return false;
    }
    
    try {
        // Read dfLa box header
        io->seek(static_cast<long>(offset), SEEK_SET);
        
        // dfLa box format:
        // - version (1 byte)
        // - flags (3 bytes) 
        // - FLAC metadata blocks (remaining bytes)
        
        uint8_t version = 0;
        if (io->read(&version, 1, 1) != 1) {
            Debug::log("iso", "ISODemuxerBoxParser: Failed to read dfLa version");
            return false;
        }
        
        // Skip flags (3 bytes)
        uint8_t flags[3];
        if (io->read(flags, 1, 3) != 3) {
            Debug::log("iso", "ISODemuxerBoxParser: Failed to read dfLa flags");
            return false;
        }
        
        // Remaining data contains FLAC metadata blocks
        size_t metadataSize = size - 4; // Subtract version + flags
        if (metadataSize == 0) {
            Debug::log("iso", "ISODemuxerBoxParser: No FLAC metadata in dfLa box");
            return false;
        }
        
        // Read FLAC metadata blocks
        std::vector<uint8_t> metadataBlocks(metadataSize);
        if (io->read(metadataBlocks.data(), 1, metadataSize) != metadataSize) {
            Debug::log("iso", "ISODemuxerBoxParser: Failed to read FLAC metadata blocks");
            return false;
        }
        
        // Parse STREAMINFO block (should be first)
        if (metadataSize >= 34) { // Minimum STREAMINFO size
            // FLAC metadata block header (4 bytes)
            uint8_t blockType = metadataBlocks[0] & 0x7F; // Remove last-metadata-block flag
            // bool isLastBlock = (metadataBlocks[0] & 0x80) != 0; // Not used in current implementation
            
            // Block length (24-bit big-endian)
            uint32_t blockLength = (static_cast<uint32_t>(metadataBlocks[1]) << 16) |
                                  (static_cast<uint32_t>(metadataBlocks[2]) << 8) |
                                  static_cast<uint32_t>(metadataBlocks[3]);
            
            if (blockType == 0 && blockLength >= 34) { // STREAMINFO block
                // Parse STREAMINFO data (starts at offset 4)
                const uint8_t* streamInfo = metadataBlocks.data() + 4;
                
                // Extract key parameters from STREAMINFO
                // Minimum block size (16-bit) - not used but part of STREAMINFO structure
                // uint16_t minBlockSize = (static_cast<uint16_t>(streamInfo[0]) << 8) | streamInfo[1];
                
                // Maximum block size (16-bit) - not used but part of STREAMINFO structure
                // uint16_t maxBlockSize = (static_cast<uint16_t>(streamInfo[2]) << 8) | streamInfo[3];
                
                // Sample rate (20-bit, bits 96-115)
                uint32_t sampleRate = (static_cast<uint32_t>(streamInfo[10]) << 12) |
                                     (static_cast<uint32_t>(streamInfo[11]) << 4) |
                                     ((streamInfo[12] & 0xF0) >> 4);
                
                // Channels (3-bit, bits 116-118) + 1
                uint8_t channels = ((streamInfo[12] & 0x0E) >> 1) + 1;
                
                // Bits per sample (5-bit, bits 119-123) + 1
                uint8_t bitsPerSample = (((streamInfo[12] & 0x01) << 4) | ((streamInfo[13] & 0xF0) >> 4)) + 1;
                
                // Total samples (36-bit, bits 124-159)
                uint64_t totalSamples = (static_cast<uint64_t>(streamInfo[13] & 0x0F) << 32) |
                                       (static_cast<uint64_t>(streamInfo[14]) << 24) |
                                       (static_cast<uint64_t>(streamInfo[15]) << 16) |
                                       (static_cast<uint64_t>(streamInfo[16]) << 8) |
                                       static_cast<uint64_t>(streamInfo[17]);
                
                // Update track information with FLAC parameters
                track.sampleRate = sampleRate;
                track.channelCount = channels;
                track.bitsPerSample = bitsPerSample;
                
                // Calculate duration from total samples
                if (sampleRate > 0) {
                    track.duration = totalSamples; // Duration in samples
                    track.timescale = sampleRate;  // Timescale matches sample rate for FLAC
                }
                
                // Store complete metadata blocks as codec configuration
                track.codecConfig = std::move(metadataBlocks);
                
                Debug::log("iso", "ISODemuxerBoxParser: FLAC configuration - ",
                          sampleRate, "Hz, ", static_cast<int>(channels), " channels, ",
                          static_cast<int>(bitsPerSample), " bits, ", totalSamples, " samples");
                
                return true;
            } else {
                Debug::log("iso", "ISODemuxerBoxParser: Invalid or missing STREAMINFO block in dfLa");
            }
        } else {
            Debug::log("iso", "ISODemuxerBoxParser: dfLa metadata too small for STREAMINFO");
        }
        
    } catch (const std::exception& e) {
        Debug::log("iso", "ISODemuxerBoxParser: Exception parsing FLAC configuration: ", e.what());
    }
    
    return false;
}

bool BoxParser::ConfigureTelephonyCodec(AudioTrackInfo& track, const std::string& codecType) {
    // Set codec type
    track.codecType = codecType;
    
    // Apply default telephony parameters if not already set
    ApplyTelephonyDefaults(track, codecType);
    
    // Validate the configuration
    return ValidateTelephonyParameters(track);
}

bool BoxParser::ValidateTelephonyParameters(AudioTrackInfo& track) {
    // Validate sample rate (telephony codecs typically use 8kHz)
    if (track.sampleRate != 8000) {
        // Allow some flexibility but warn about non-standard rates
        if (track.sampleRate < 4000 || track.sampleRate > 48000) {
            return false;
        }
    }
    
    // Validate channel count (telephony is typically mono)
    if (track.channelCount != 1) {
        // Some telephony applications may use stereo
        if (track.channelCount > 2) {
            return false;
        }
    }
    
    // Validate bit depth (telephony codecs are typically 8-bit)
    if (track.bitsPerSample != 8) {
        // Allow 16-bit for higher quality telephony
        if (track.bitsPerSample != 16) {
            return false;
        }
    }
    
    return true;
}

void BoxParser::ApplyTelephonyDefaults(AudioTrackInfo& track, const std::string& codecType) {
    // Apply standard telephony defaults if parameters are not set
    if (track.sampleRate == 0) {
        track.sampleRate = 8000; // Standard telephony sample rate
    }
    
    if (track.channelCount == 0) {
        track.channelCount = 1; // Mono audio for telephony
    }
    
    if (track.bitsPerSample == 0) {
        track.bitsPerSample = 8; // Standard telephony bit depth
    }
    
    // Set codec-specific defaults
    if (codecType == "ulaw" || codecType == "alaw") {
        // Both μ-law and A-law use 8-bit samples
        track.bitsPerSample = 8;
    }
}

} // namespace ISO
} // namespace Demuxer
} // namespace PsyMP3
