/*
 * ISODemuxer.cpp - ISO Base Media File Format demuxer implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "ISODemuxer.h"
#include <algorithm>
#include <stdexcept>
#include <cstring>

ISODemuxer::ISODemuxer(std::unique_ptr<IOHandler> handler) 
    : Demuxer(std::move(handler)), selectedTrackIndex(-1), currentSampleIndex(0), m_eof(false) {
    initializeComponents();
}

ISODemuxer::~ISODemuxer() {
    cleanup();
}

void ISODemuxer::initializeComponents() {
    // Initialize core components with shared IOHandler
    std::shared_ptr<IOHandler> sharedHandler(m_handler.get(), [](IOHandler*) {
        // Custom deleter that does nothing - we don't want to delete the handler
        // since it's owned by the unique_ptr in the base class
    });
    
    boxParser = std::make_unique<BoxParser>(sharedHandler);
    sampleTables = std::make_unique<SampleTableManager>();
    fragmentHandler = std::make_unique<FragmentHandler>();
    metadataExtractor = std::make_unique<MetadataExtractor>();
    streamManager = std::make_unique<StreamManager>();
    seekingEngine = std::make_unique<SeekingEngine>();
}

void ISODemuxer::cleanup() {
    // Components will be automatically cleaned up by unique_ptr destructors
    audioTracks.clear();
    selectedTrackIndex = -1;
    currentSampleIndex = 0;
}

// BoxParser implementation
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
    
    // Seek to box position
    if (io->seek(static_cast<long>(offset), SEEK_SET) != 0) {
        return header;
    }
    
    // Read basic box header (8 bytes minimum)
    if (offset + 8 > fileSize) {
        return header;
    }
    
    // Read size (4 bytes, big-endian)
    uint32_t size32 = ReadUInt32BE(offset);
    
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
    } else if (size32 == 0) {
        // Size extends to end of file
        header.size = fileSize - offset;
        header.dataOffset = offset + 8;
    } else {
        // Normal size
        header.size = size32;
        header.dataOffset = offset + 8;
    }
    
    return header;
}

bool BoxParser::ValidateBoxSize(const BoxHeader& header, uint64_t containerSize) {
    // Check if box size is valid
    if (header.size == 0) {
        return false;
    }
    
    // Check if box fits within container
    if (header.size > containerSize) {
        return false;
    }
    
    // Check if box extends beyond file
    uint64_t boxStart = header.dataOffset - (header.isExtendedSize() ? 16 : 8);
    if (boxStart + header.size > fileSize) {
        return false;
    }
    
    // Check minimum header size
    uint64_t headerSize = header.isExtendedSize() ? 16 : 8;
    if (header.size < headerSize) {
        return false;
    }
    
    return true;
}

bool BoxParser::ParseBoxRecursively(uint64_t offset, uint64_t size, 
                                   std::function<bool(const BoxHeader&, uint64_t)> handler) {
    uint64_t currentOffset = offset;
    uint64_t endOffset = offset + size;
    
    while (currentOffset < endOffset) {
        // Read box header
        BoxHeader header = ReadBoxHeader(currentOffset);
        
        if (header.type == 0 || header.size == 0) {
            // Invalid box, skip remaining data
            break;
        }
        
        // Validate box size
        if (!ValidateBoxSize(header, endOffset - currentOffset)) {
            // Invalid box size, skip it
            currentOffset += 8; // Skip at least the basic header
            continue;
        }
        
        // Call handler for this box
        if (!handler(header, currentOffset)) {
            // Handler failed, but continue parsing other boxes
        }
        
        // Move to next box
        currentOffset += header.size;
        
        // Prevent infinite loops
        if (header.size < 8) {
            break;
        }
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
                                    return ParseBoxRecursively(minfHeader.dataOffset, 
                                                              minfHeader.size - (minfHeader.dataOffset - minfOffset),
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
                                return ParseAACConfiguration(header.dataOffset, 
                                                           header.size - (header.dataOffset - boxOffset), 
                                                           track);
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
            case CODEC_ULAW:
                track.codecType = "ulaw";
                // Configure telephony audio defaults
                if (track.sampleRate == 0) track.sampleRate = 8000;
                if (track.channelCount == 0) track.channelCount = 1;
                if (track.bitsPerSample == 0) track.bitsPerSample = 8;
                // Support both 8kHz and 16kHz for telephony
                if (track.sampleRate != 8000 && track.sampleRate != 16000) {
                    track.sampleRate = 8000; // Default to 8kHz
                }
                break;
            case CODEC_ALAW:
                track.codecType = "alaw";
                // Configure European telephony standard defaults
                if (track.sampleRate == 0) track.sampleRate = 8000;
                if (track.channelCount == 0) track.channelCount = 1;
                if (track.bitsPerSample == 0) track.bitsPerSample = 8;
                // Support both 8kHz and 16kHz for telephony
                if (track.sampleRate != 8000 && track.sampleRate != 16000) {
                    track.sampleRate = 8000; // Default to 8kHz
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
    // Basic implementation - will be expanded in later tasks
    return true;
}

bool BoxParser::ParseAACConfiguration(uint64_t offset, uint64_t size, AudioTrackInfo& track) {
    if (size < 12) { // Minimum ESDS size
        return false;
    }
    
    // Skip version/flags (4 bytes)
    uint64_t currentOffset = offset + 4;
    
    // Parse ES descriptor
    // This is a simplified parser for the most common AAC configuration
    // Full ESDS parsing is complex due to variable-length descriptors
    
    // Look for decoder config descriptor (tag 0x04)
    while (currentOffset < offset + size - 4) {
        uint8_t tag = 0;
        io->seek(currentOffset, SEEK_SET);
        if (io->read(&tag, 1, 1) != 1) break;
        
        if (tag == 0x04) { // DecoderConfigDescriptor
            // Skip tag and length encoding (variable)
            currentOffset += 2; // Simplified - assume 1-byte length
            
            // Skip object type (1 byte) and stream type (1 byte)
            currentOffset += 2;
            
            // Skip buffer size (3 bytes) and max bitrate (4 bytes) and avg bitrate (4 bytes)
            currentOffset += 11;
            
            // Look for decoder specific info (tag 0x05)
            io->seek(currentOffset, SEEK_SET);
            uint8_t dsiTag = 0;
            if (io->read(&dsiTag, 1, 1) == 1 && dsiTag == 0x05) {
                currentOffset += 1;
                
                // Read length (simplified - assume 1 byte)
                uint8_t dsiLength = 0;
                if (io->read(&dsiLength, 1, 1) == 1 && dsiLength > 0 && dsiLength <= 64) {
                    currentOffset += 1;
                    
                    // Read AudioSpecificConfig
                    track.codecConfig.resize(dsiLength);
                    if (io->read(track.codecConfig.data(), 1, dsiLength) == dsiLength) {
                        // Parse basic AAC configuration from first 2 bytes
                        if (dsiLength >= 2) {
                            uint16_t config = (track.codecConfig[0] << 8) | track.codecConfig[1];
                            
                            // Extract audio object type (5 bits) - not used currently
                            // uint8_t audioObjectType = (config >> 11) & 0x1F;
                            
                            // Extract sampling frequency index (4 bits)
                            uint8_t samplingFreqIndex = (config >> 7) & 0x0F;
                            
                            // Extract channel configuration (4 bits)
                            uint8_t channelConfig = (config >> 3) & 0x0F;
                            
                            // Map sampling frequency index to actual rate
                            static const uint32_t aacSampleRates[] = {
                                96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
                                16000, 12000, 11025, 8000, 7350, 0, 0, 0
                            };
                            
                            if (samplingFreqIndex < 16 && aacSampleRates[samplingFreqIndex] > 0) {
                                track.sampleRate = aacSampleRates[samplingFreqIndex];
                            }
                            
                            if (channelConfig > 0 && channelConfig <= 7) {
                                track.channelCount = channelConfig;
                            }
                        }
                        return true;
                    }
                }
            }
            break;
        }
        currentOffset++;
    }
    
    return false;
}

bool BoxParser::ParseALACConfiguration(uint64_t offset, uint64_t size, AudioTrackInfo& track) {
    if (size < 24) { // Minimum ALAC magic cookie size
        return false;
    }
    
    // ALAC magic cookie contains decoder configuration
    track.codecConfig.resize(size);
    
    io->seek(offset, SEEK_SET);
    if (io->read(track.codecConfig.data(), 1, size) != size) {
        track.codecConfig.clear();
        return false;
    }
    
    // Parse ALAC configuration from magic cookie
    if (size >= 24) {
        // ALAC magic cookie format (simplified parsing)
        // Bytes 0-3: frame length (big-endian)
        // Bytes 4: compatible version
        // Bytes 5: bit depth
        // Bytes 6: pb (rice modifier)
        // Bytes 7: mb (rice modifier)
        // Bytes 8: kb (rice modifier)  
        // Bytes 9: channels
        // Bytes 10-11: max run (big-endian)
        // Bytes 12-15: max frame bytes (big-endian)
        // Bytes 16-19: avg bit rate (big-endian)
        // Bytes 20-23: sample rate (big-endian)
        
        if (track.codecConfig.size() >= 24) {
            // Extract bit depth
            track.bitsPerSample = track.codecConfig[5];
            
            // Extract channel count
            track.channelCount = track.codecConfig[9];
            
            // Extract sample rate (big-endian)
            track.sampleRate = (static_cast<uint32_t>(track.codecConfig[20]) << 24) |
                              (static_cast<uint32_t>(track.codecConfig[21]) << 16) |
                              (static_cast<uint32_t>(track.codecConfig[22]) << 8) |
                              static_cast<uint32_t>(track.codecConfig[23]);
            
            // Extract average bitrate (big-endian)
            track.avgBitrate = (static_cast<uint32_t>(track.codecConfig[16]) << 24) |
                              (static_cast<uint32_t>(track.codecConfig[17]) << 16) |
                              (static_cast<uint32_t>(track.codecConfig[18]) << 8) |
                              static_cast<uint32_t>(track.codecConfig[19]);
        }
    }
    
    return true;
}

// SampleTableManager implementation
bool SampleTableManager::BuildSampleTables(const SampleTableInfo& rawTables) {
    // Validate input tables
    if (rawTables.chunkOffsets.empty() || rawTables.samplesPerChunk.empty() || 
        rawTables.sampleSizes.empty() || rawTables.sampleTimes.empty()) {
        return false;
    }
    
    // Clear existing tables
    chunkTable.clear();
    timeTable.clear();
    syncSamples.clear();
    
    // Build chunk table with sample-to-chunk mapping
    if (!BuildChunkTable(rawTables)) {
        return false;
    }
    
    // Build time table for efficient time-to-sample lookups
    if (!BuildTimeTable(rawTables)) {
        return false;
    }
    
    // Store sample sizes (compressed if all samples are the same size)
    if (!BuildSampleSizeTable(rawTables)) {
        return false;
    }
    
    // Store sync samples for keyframe seeking
    syncSamples = rawTables.syncSamples;
    
    // Validate consistency between tables
    return ValidateTableConsistency();
}

bool SampleTableManager::BuildChunkTable(const SampleTableInfo& rawTables) {
    chunkTable.clear();
    chunkTable.reserve(rawTables.chunkOffsets.size());
    
    // Build expanded sample-to-chunk mapping
    std::vector<uint32_t> expandedSamplesPerChunk;
    if (!BuildExpandedSampleToChunkMapping(rawTables, expandedSamplesPerChunk)) {
        return false;
    }
    
    uint64_t currentSampleIndex = 0;
    
    for (size_t chunkIndex = 0; chunkIndex < rawTables.chunkOffsets.size(); chunkIndex++) {
        ChunkInfo chunk;
        chunk.offset = rawTables.chunkOffsets[chunkIndex];
        chunk.firstSample = currentSampleIndex;
        
        // Get samples per chunk for this specific chunk
        if (chunkIndex < expandedSamplesPerChunk.size()) {
            chunk.sampleCount = expandedSamplesPerChunk[chunkIndex];
        } else {
            // Fallback to first entry if we don't have enough data
            chunk.sampleCount = !rawTables.samplesPerChunk.empty() ? rawTables.samplesPerChunk[0] : 1;
        }
        
        chunkTable.push_back(chunk);
        currentSampleIndex += chunk.sampleCount;
    }
    
    return true;
}

uint32_t SampleTableManager::GetSamplesPerChunkForIndex(size_t chunkIndex, const std::vector<uint32_t>& samplesPerChunk) {
    // For now, use a simplified approach - assume all chunks have the same sample count
    // This will be refined when we properly parse the sample-to-chunk table structure
    if (!samplesPerChunk.empty()) {
        return samplesPerChunk[0];
    }
    return 1;
}

bool SampleTableManager::BuildTimeTable(const SampleTableInfo& rawTables) {
    timeTable.clear();
    
    if (rawTables.sampleTimes.empty()) {
        return false;
    }
    
    // Build optimized time table for binary search
    // Use adaptive compression - more entries for areas with time changes
    timeTable.reserve(rawTables.sampleTimes.size() / 5); // Better estimate
    
    uint32_t lastDuration = 0;
    
    for (size_t i = 0; i < rawTables.sampleTimes.size(); i++) {
        bool shouldAddEntry = false;
        
        // Always add first and last entries
        if (i == 0 || i == rawTables.sampleTimes.size() - 1) {
            shouldAddEntry = true;
        }
        // Add entry every 10 samples for regular intervals
        else if (i % 10 == 0) {
            shouldAddEntry = true;
        }
        // Add entry if there's a significant time change (for variable bitrate content)
        else if (i > 0) {
            uint64_t currentDuration = (i + 1 < rawTables.sampleTimes.size()) ? 
                                     (rawTables.sampleTimes[i + 1] - rawTables.sampleTimes[i]) : lastDuration;
            
            // If duration changed significantly, add an entry
            if (lastDuration > 0 && currentDuration > 0) {
                double durationRatio = static_cast<double>(currentDuration) / lastDuration;
                if (durationRatio < 0.8 || durationRatio > 1.2) {
                    shouldAddEntry = true;
                }
            }
        }
        
        if (shouldAddEntry) {
            TimeToSampleEntry entry;
            entry.sampleIndex = i;
            entry.timestamp = rawTables.sampleTimes[i];
            entry.duration = (i + 1 < rawTables.sampleTimes.size()) ? 
                           (rawTables.sampleTimes[i + 1] - rawTables.sampleTimes[i]) : lastDuration;
            
            timeTable.push_back(entry);
            
            lastDuration = entry.duration;
        }
    }
    
    // Ensure we have at least one entry
    if (timeTable.empty()) {
        TimeToSampleEntry entry;
        entry.sampleIndex = 0;
        entry.timestamp = rawTables.sampleTimes[0];
        entry.duration = rawTables.sampleTimes.size() > 1 ? 
                        (rawTables.sampleTimes[1] - rawTables.sampleTimes[0]) : 1000;
        timeTable.push_back(entry);
    }
    
    return true;
}

bool SampleTableManager::BuildSampleSizeTable(const SampleTableInfo& rawTables) {
    if (rawTables.sampleSizes.empty()) {
        return false;
    }
    
    // Check if all samples have the same size (compression opportunity)
    if (rawTables.sampleSizes.size() == 1) {
        // All samples have the same size
        sampleSizes = rawTables.sampleSizes[0];
    } else {
        // Variable sample sizes
        bool allSame = true;
        uint32_t firstSize = rawTables.sampleSizes[0];
        
        for (size_t i = 1; i < rawTables.sampleSizes.size(); i++) {
            if (rawTables.sampleSizes[i] != firstSize) {
                allSame = false;
                break;
            }
        }
        
        if (allSame) {
            // All samples are the same size, compress
            sampleSizes = firstSize;
        } else {
            // Variable sizes, store full table
            sampleSizes = rawTables.sampleSizes;
        }
    }
    
    return true;
}

bool SampleTableManager::ValidateTableConsistency() {
    // Validate that chunk table and time table have consistent sample counts
    uint64_t totalSamplesFromChunks = 0;
    for (const auto& chunk : chunkTable) {
        totalSamplesFromChunks += chunk.sampleCount;
    }
    
    uint64_t totalSamplesFromTime = timeTable.empty() ? 0 : 
        (timeTable.back().sampleIndex + 1);
    
    // Allow some tolerance for compressed time table
    if (totalSamplesFromChunks > 0 && totalSamplesFromTime > 0) {
        double ratio = static_cast<double>(totalSamplesFromTime) / totalSamplesFromChunks;
        if (ratio < 0.8 || ratio > 1.2) {
            // Significant mismatch between tables
            return false;
        }
    }
    
    return true;
}

SampleTableManager::SampleInfo SampleTableManager::GetSampleInfo(uint64_t sampleIndex) {
    SampleInfo info = {};
    
    // Find the chunk containing this sample
    ChunkInfo* chunk = FindChunkForSample(sampleIndex);
    if (!chunk) {
        return info;
    }
    
    // Calculate sample offset within the chunk
    uint64_t sampleInChunk = sampleIndex - chunk->firstSample;
    uint64_t sampleOffset = chunk->offset;
    
    // Handle variable sample sizes - accumulate offsets of previous samples in this chunk
    if (std::holds_alternative<std::vector<uint32_t>>(sampleSizes)) {
        // Variable sample sizes - calculate exact offset
        const auto& sizes = std::get<std::vector<uint32_t>>(sampleSizes);
        for (uint64_t i = 0; i < sampleInChunk; i++) {
            uint64_t prevSampleIndex = chunk->firstSample + i;
            if (prevSampleIndex < sizes.size()) {
                sampleOffset += sizes[prevSampleIndex];
            } else {
                // Sample index out of range - use default size if available
                if (!sizes.empty()) {
                    sampleOffset += sizes[0];
                }
            }
        }
    } else {
        // Fixed sample size - simple calculation
        uint32_t fixedSize = std::get<uint32_t>(sampleSizes);
        sampleOffset += sampleInChunk * fixedSize;
    }
    
    info.offset = sampleOffset;
    info.size = GetSampleSize(sampleIndex);
    info.duration = GetSampleDuration(sampleIndex);
    info.isKeyframe = IsSyncSample(sampleIndex);
    
    return info;
}

SampleTableManager::ChunkInfo* SampleTableManager::FindChunkForSample(uint64_t sampleIndex) {
    for (auto& chunk : chunkTable) {
        if (sampleIndex >= chunk.firstSample && 
            sampleIndex < chunk.firstSample + chunk.sampleCount) {
            return &chunk;
        }
    }
    return nullptr;
}

uint32_t SampleTableManager::GetSampleSize(uint64_t sampleIndex) {
    if (std::holds_alternative<uint32_t>(sampleSizes)) {
        // All samples have the same size
        return std::get<uint32_t>(sampleSizes);
    } else {
        // Variable sample sizes
        const auto& sizes = std::get<std::vector<uint32_t>>(sampleSizes);
        if (sampleIndex < sizes.size()) {
            return sizes[sampleIndex];
        }
    }
    return 0;
}

uint32_t SampleTableManager::GetSampleDuration(uint64_t sampleIndex) {
    // Find the time table entry for this sample
    for (size_t i = 0; i < timeTable.size(); i++) {
        if (timeTable[i].sampleIndex <= sampleIndex && 
            (i + 1 >= timeTable.size() || timeTable[i + 1].sampleIndex > sampleIndex)) {
            return timeTable[i].duration;
        }
    }
    return 0;
}

bool SampleTableManager::IsSyncSample(uint64_t sampleIndex) {
    if (syncSamples.empty()) {
        // No sync sample table - all samples are sync samples (common for audio)
        return true;
    }
    
    // Binary search for sync sample
    return std::binary_search(syncSamples.begin(), syncSamples.end(), sampleIndex);
}

uint64_t SampleTableManager::TimeToSample(double timestamp) {
    if (timeTable.empty()) {
        return 0;
    }
    
    // Convert timestamp to timescale units (assuming timestamp is in seconds)
    uint64_t targetTime = static_cast<uint64_t>(timestamp * 1000); // Convert to milliseconds
    
    // Binary search in time table for efficient lookup
    size_t left = 0;
    size_t right = timeTable.size();
    
    while (left < right) {
        size_t mid = left + (right - left) / 2;
        
        if (timeTable[mid].timestamp <= targetTime) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    
    // left now points to the first entry with timestamp > targetTime
    // We want the entry at or before targetTime
    if (left > 0) {
        left--;
    }
    
    // Interpolate within the time table entry for more accuracy
    if (left < timeTable.size()) {
        const auto& entry = timeTable[left];
        
        // If we have a next entry, interpolate between them
        if (left + 1 < timeTable.size()) {
            const auto& nextEntry = timeTable[left + 1];
            
            // Calculate interpolation factor
            uint64_t entryDuration = nextEntry.timestamp - entry.timestamp;
            uint64_t timeOffset = targetTime - entry.timestamp;
            
            if (entryDuration > 0 && timeOffset <= entryDuration) {
                // Interpolate sample index
                uint64_t sampleRange = nextEntry.sampleIndex - entry.sampleIndex;
                uint64_t interpolatedOffset = (timeOffset * sampleRange) / entryDuration;
                
                return entry.sampleIndex + interpolatedOffset;
            }
        }
        
        return entry.sampleIndex;
    }
    
    return 0;
}

double SampleTableManager::SampleToTime(uint64_t sampleIndex) {
    if (timeTable.empty()) {
        return 0.0;
    }
    
    // Find the time table entry for this sample
    for (size_t i = 0; i < timeTable.size(); i++) {
        if (timeTable[i].sampleIndex <= sampleIndex && 
            (i + 1 >= timeTable.size() || timeTable[i + 1].sampleIndex > sampleIndex)) {
            
            // Interpolate time within the entry
            uint64_t sampleOffset = sampleIndex - timeTable[i].sampleIndex;
            uint64_t estimatedTime = timeTable[i].timestamp + (sampleOffset * timeTable[i].duration);
            
            return static_cast<double>(estimatedTime) / 1000.0; // Convert to seconds
        }
    }
    
    return 0.0;
}

// FragmentHandler implementation
bool FragmentHandler::ProcessMovieFragment(uint64_t moofOffset) {
    // Basic implementation - will be expanded in later tasks
    return true;
}

bool FragmentHandler::UpdateSampleTables(const SampleTableInfo& traf) {
    // Basic implementation - will be expanded in later tasks
    return true;
}

// MetadataExtractor implementation
std::map<std::string, std::string> MetadataExtractor::ExtractMetadata(std::shared_ptr<IOHandler> io, uint64_t udtaOffset, uint64_t size) {
    std::map<std::string, std::string> metadata;
    
    if (!io || size < 8) {
        return metadata;
    }
    
    // Parse udta box recursively to find metadata
    ParseUdtaBox(io, udtaOffset, size, metadata);
    
    return metadata;
}

bool MetadataExtractor::ParseUdtaBox(std::shared_ptr<IOHandler> io, uint64_t offset, uint64_t size, std::map<std::string, std::string>& metadata) {
    uint64_t currentOffset = offset;
    uint64_t endOffset = offset + size;
    
    while (currentOffset < endOffset) {
        // Read box header
        if (currentOffset + 8 > endOffset) {
            break;
        }
        
        uint32_t boxSize = ReadUInt32BE(io, currentOffset);
        uint32_t boxType = ReadUInt32BE(io, currentOffset + 4);
        
        if (boxSize == 0 || boxSize < 8) {
            break;
        }
        
        uint64_t boxDataOffset = currentOffset + 8;
        uint64_t boxDataSize = boxSize - 8;
        
        // Handle extended size
        if (boxSize == 1) {
            if (currentOffset + 16 > endOffset) {
                break;
            }
            uint64_t extendedSize = ReadUInt64BE(io, currentOffset + 8);
            boxDataOffset = currentOffset + 16;
            boxDataSize = extendedSize - 16;
        }
        
        // Parse specific metadata boxes
        switch (boxType) {
            case BOX_META:
                // Metadata box - contains ilst
                ParseMetaBox(io, boxDataOffset, boxDataSize, metadata);
                break;
            case BOX_ILST:
                // Item list - contains actual metadata items
                ParseIlstBox(io, boxDataOffset, boxDataSize, metadata);
                break;
            default:
                // Check for iTunes-style metadata atoms
                ParseiTunesMetadataAtom(io, boxType, boxDataOffset, boxDataSize, metadata);
                break;
        }
        
        currentOffset += boxSize;
    }
    
    return true;
}

bool MetadataExtractor::ParseMetaBox(std::shared_ptr<IOHandler> io, uint64_t offset, uint64_t size, std::map<std::string, std::string>& metadata) {
    if (size < 4) {
        return false;
    }
    
    // Skip version/flags (4 bytes)
    uint64_t dataOffset = offset + 4;
    uint64_t dataSize = size - 4;
    
    // Parse meta box contents recursively
    return ParseUdtaBox(io, dataOffset, dataSize, metadata);
}

bool MetadataExtractor::ParseIlstBox(std::shared_ptr<IOHandler> io, uint64_t offset, uint64_t size, std::map<std::string, std::string>& metadata) {
    uint64_t currentOffset = offset;
    uint64_t endOffset = offset + size;
    
    while (currentOffset < endOffset) {
        if (currentOffset + 8 > endOffset) {
            break;
        }
        
        uint32_t itemSize = ReadUInt32BE(io, currentOffset);
        uint32_t itemType = ReadUInt32BE(io, currentOffset + 4);
        
        if (itemSize == 0 || itemSize < 8) {
            break;
        }
        
        uint64_t itemDataOffset = currentOffset + 8;
        uint64_t itemDataSize = itemSize - 8;
        
        // Parse iTunes metadata item
        ParseiTunesMetadataAtom(io, itemType, itemDataOffset, itemDataSize, metadata);
        
        currentOffset += itemSize;
    }
    
    return true;
}

bool MetadataExtractor::ParseiTunesMetadataAtom(std::shared_ptr<IOHandler> io, uint32_t atomType, uint64_t offset, uint64_t size, std::map<std::string, std::string>& metadata) {
    std::string key;
    
    // Map atom types to metadata keys
    switch (atomType) {
        case BOX_TITLE:  // ©nam
            key = "title";
            break;
        case BOX_ARTIST: // ©ART
            key = "artist";
            break;
        case BOX_ALBUM:  // ©alb
            key = "album";
            break;
        case BOX_DATE:   // ©day
            key = "date";
            break;
        case BOX_GENRE:  // ©gen
            key = "genre";
            break;
        case BOX_TRACK:  // trkn
            key = "track";
            break;
        case BOX_DISK:   // disk
            key = "disk";
            break;
        case BOX_COVR:   // covr
            key = "artwork";
            break;
        default:
            // Unknown metadata atom, skip
            return true;
    }
    
    // Parse data box within the metadata atom
    uint64_t currentOffset = offset;
    uint64_t endOffset = offset + size;
    
    while (currentOffset < endOffset) {
        if (currentOffset + 8 > endOffset) {
            break;
        }
        
        uint32_t dataBoxSize = ReadUInt32BE(io, currentOffset);
        uint32_t dataBoxType = ReadUInt32BE(io, currentOffset + 4);
        
        if (dataBoxSize == 0 || dataBoxSize < 8) {
            break;
        }
        
        if (dataBoxType == BOX_DATA) {
            // Found data box - extract the actual metadata value
            uint64_t dataOffset = currentOffset + 8;
            uint64_t dataSize = dataBoxSize - 8;
            
            if (dataSize >= 8) {
                // Skip type indicator and locale (8 bytes)
                dataOffset += 8;
                dataSize -= 8;
                
                // Extract text or binary data
                std::string value;
                if (atomType == BOX_COVR) {
                    // Artwork - store as binary data indicator
                    value = "[ARTWORK_DATA]";
                } else if (atomType == BOX_TRACK || atomType == BOX_DISK) {
                    // Track/disk number - binary format
                    if (dataSize >= 4) {
                        uint32_t trackNum = ReadUInt32BE(io, dataOffset + 2); // Skip padding
                        value = std::to_string(trackNum);
                    }
                } else {
                    // Text metadata
                    value = ExtractTextMetadata(io, dataOffset, dataSize);
                }
                
                if (!value.empty()) {
                    metadata[key] = value;
                }
            }
            break;
        }
        
        currentOffset += dataBoxSize;
    }
    
    return true;
}

std::string MetadataExtractor::ExtractTextMetadata(std::shared_ptr<IOHandler> io, uint64_t offset, uint64_t size) {
    if (!io || size == 0 || size > 65536) { // Reasonable limit for text metadata
        return "";
    }
    
    io->seek(static_cast<long>(offset), SEEK_SET);
    
    std::vector<uint8_t> buffer(size);
    size_t bytesRead = io->read(buffer.data(), 1, size);
    
    if (bytesRead == 0) {
        return "";
    }
    
    // Convert to string, handling UTF-8 and null termination
    std::string result;
    result.reserve(bytesRead);
    
    for (size_t i = 0; i < bytesRead; i++) {
        if (buffer[i] == 0) {
            break; // Null terminator
        }
        result += static_cast<char>(buffer[i]);
    }
    
    // Trim whitespace
    size_t start = result.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    
    size_t end = result.find_last_not_of(" \t\r\n");
    return result.substr(start, end - start + 1);
}

uint32_t MetadataExtractor::ReadUInt32BE(std::shared_ptr<IOHandler> io, uint64_t offset) {
    if (!io) {
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

uint64_t MetadataExtractor::ReadUInt64BE(std::shared_ptr<IOHandler> io, uint64_t offset) {
    if (!io) {
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

// StreamManager implementation
void StreamManager::AddAudioTrack(const AudioTrackInfo& track) {
    tracks.push_back(track);
}

std::vector<StreamInfo> StreamManager::GetStreamInfos() const {
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
        info.duration_ms = track.timescale > 0 ? (track.duration * 1000ULL) / track.timescale : 0;
        info.duration_samples = info.sample_rate > 0 ? (info.duration_ms * info.sample_rate) / 1000ULL : 0;
        streams.push_back(info);
    }
    return streams;
}

AudioTrackInfo* StreamManager::GetTrack(uint32_t trackId) {
    for (auto& track : tracks) {
        if (track.trackId == trackId) {
            return &track;
        }
    }
    return nullptr;
}

// SeekingEngine implementation
bool SeekingEngine::SeekToTimestamp(double timestamp, AudioTrackInfo& track, SampleTableManager& sampleTables) {
    if (timestamp < 0.0) {
        return false;
    }
    
    // Use SampleTableManager to find the sample index for this timestamp
    uint64_t targetSampleIndex = sampleTables.TimeToSample(timestamp);
    
    // For keyframe-aware seeking, find the nearest sync sample at or before target
    uint64_t seekSampleIndex = FindNearestSyncSample(targetSampleIndex, sampleTables);
    
    // Validate the seek position
    if (!ValidateSeekPosition(seekSampleIndex, track, sampleTables)) {
        return false;
    }
    
    // Update track position
    track.currentSampleIndex = seekSampleIndex;
    
    return true;
}

uint64_t SeekingEngine::BinarySearchTimeToSample(double timestamp, const std::vector<SampleTableManager::SampleInfo>& samples) {
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
            if (mid == samples.size() - 1 || 
                static_cast<double>(mid + 1) / samples.size() > timestamp) {
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

uint64_t SeekingEngine::FindNearestSyncSample(uint64_t targetSampleIndex, SampleTableManager& sampleTables) {
    // Get sample info to check if target is already a sync sample
    auto sampleInfo = sampleTables.GetSampleInfo(targetSampleIndex);
    
    if (sampleInfo.isKeyframe) {
        // Target sample is already a keyframe
        return targetSampleIndex;
    }
    
    // Search backwards for the nearest sync sample
    // This ensures we don't seek to a position that requires previous frames for decoding
    for (uint64_t i = targetSampleIndex; i > 0; i--) {
        auto info = sampleTables.GetSampleInfo(i - 1);
        if (info.isKeyframe) {
            return i - 1;
        }
        
        // Limit backward search to avoid excessive searching
        if (targetSampleIndex - i > 100) {
            break;
        }
    }
    
    // If no sync sample found nearby, use the target sample
    // This is acceptable for audio where most samples are independent
    return targetSampleIndex;
}

bool SeekingEngine::ValidateSeekPosition(uint64_t sampleIndex, const AudioTrackInfo& track, SampleTableManager& sampleTables) {
    // Get sample info to validate the position
    auto sampleInfo = sampleTables.GetSampleInfo(sampleIndex);
    
    // Check if sample exists and has valid data
    if (sampleInfo.size == 0 || sampleInfo.offset == 0) {
        return false;
    }
    
    // Validate that the sample index is within reasonable bounds
    // We can't easily determine the total sample count without parsing all tables,
    // so we'll do a basic sanity check
    if (sampleIndex > 1000000) { // Arbitrary large number for sanity check
        return false;
    }
    
    return true;
}

bool ISODemuxer::parseContainer() {
    if (m_parsed) {
        return true;
    }
    
    try {
        // Get file size
        m_handler->seek(0, SEEK_END);
        off_t file_size = m_handler->tell();
        m_handler->seek(0, SEEK_SET);
        
        if (file_size <= 0) {
            return false;
        }
        
        // Parse top-level boxes to find ftyp and moov
        std::string containerType;
        bool foundFileType = false;
        bool foundMovie = false;
        
        boxParser->ParseBoxRecursively(0, static_cast<uint64_t>(file_size), 
            [this, &containerType, &foundFileType, &foundMovie](const BoxHeader& header, uint64_t boxOffset) {
                switch (header.type) {
                    case BOX_FTYP:
                        // File type box - identify container variant
                        foundFileType = boxParser->ParseFileTypeBox(header.dataOffset, 
                                                                   header.size - (header.dataOffset - boxOffset), 
                                                                   containerType);
                        return foundFileType;
                    case BOX_MOOV:
                        // Movie box - extract track information
                        foundMovie = ParseMovieBoxWithTracks(header.dataOffset, 
                                                           header.size - (header.dataOffset - boxOffset));
                        return foundMovie;
                    case BOX_MDAT:
                        // Media data box - skip for now
                        return true;
                    case BOX_FREE:
                    case BOX_SKIP:
                        // Free space - skip
                        return true;
                    default:
                        return boxParser->SkipUnknownBox(header);
                }
            });
        
        if (!foundFileType) {
            // No ftyp box found, assume generic MP4
            containerType = "MP4";
        }
        
        if (!foundMovie) {
            return false;
        }
        
        // Add tracks to stream manager
        for (const auto& track : audioTracks) {
            streamManager->AddAudioTrack(track);
        }
        
        // Calculate duration from audio tracks
        for (const auto& track : audioTracks) {
            uint64_t track_duration_ms = track.timescale > 0 ? (track.duration * 1000ULL) / track.timescale : 0;
            m_duration_ms = std::max(m_duration_ms, track_duration_ms);
        }
        
        m_parsed = true;
        return !audioTracks.empty();
        
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<StreamInfo> ISODemuxer::getStreams() const {
    if (streamManager) {
        return streamManager->GetStreamInfos();
    }
    return std::vector<StreamInfo>();
}

StreamInfo ISODemuxer::getStreamInfo(uint32_t stream_id) const {
    auto streams = getStreams();
    auto it = std::find_if(streams.begin(), streams.end(),
                          [stream_id](const StreamInfo& info) {
                              return info.stream_id == stream_id;
                          });
    
    if (it != streams.end()) {
        return *it;
    }
    
    return StreamInfo{};
}

MediaChunk ISODemuxer::readChunk() {
    // Find the first available audio track if none selected
    if (selectedTrackIndex == -1 && !audioTracks.empty()) {
        selectedTrackIndex = 0;
    }
    
    if (selectedTrackIndex >= 0 && selectedTrackIndex < static_cast<int>(audioTracks.size())) {
        return readChunk(audioTracks[selectedTrackIndex].trackId);
    }
    
    return MediaChunk{};
}

MediaChunk ISODemuxer::readChunk(uint32_t stream_id) {
    if (!streamManager) {
        return MediaChunk{};
    }
    
    AudioTrackInfo* track = streamManager->GetTrack(stream_id);
    if (!track) {
        return MediaChunk{};
    }
    
    // Use SampleTableManager to get sample info
    if (!sampleTables) {
        return MediaChunk{};
    }
    
    // Get sample information for current position
    auto sampleInfo = sampleTables->GetSampleInfo(track->currentSampleIndex);
    if (sampleInfo.size == 0) {
        m_eof = true;
        return MediaChunk{};
    }
    
    // Extract sample data from mdat box using sample tables
    MediaChunk chunk = ExtractSampleData(stream_id, *track, sampleInfo);
    
    if (chunk.data.empty()) {
        m_eof = true;
        return MediaChunk{};
    }
    
    // Update track position
    track->currentSampleIndex++;
    
    // Update global position based on track timing
    double timestamp = sampleTables->SampleToTime(track->currentSampleIndex);
    m_position_ms = static_cast<uint64_t>(timestamp * 1000.0);
    
    return chunk;
}

bool ISODemuxer::seekTo(uint64_t timestamp_ms) {
    if (!seekingEngine || !sampleTables) {
        return false;
    }
    
    // If no track is selected, select the first audio track
    if (selectedTrackIndex == -1 && !audioTracks.empty()) {
        selectedTrackIndex = 0;
    }
    
    if (selectedTrackIndex >= static_cast<int>(audioTracks.size())) {
        return false;
    }
    
    AudioTrackInfo& track = audioTracks[selectedTrackIndex];
    
    // Validate seek position against track duration
    uint64_t trackDurationMs = track.timescale > 0 ? (track.duration * 1000ULL) / track.timescale : 0;
    if (timestamp_ms > trackDurationMs && trackDurationMs > 0) {
        // Clamp to track duration
        timestamp_ms = trackDurationMs;
    }
    
    double timestamp_seconds = static_cast<double>(timestamp_ms) / 1000.0;
    
    bool success = seekingEngine->SeekToTimestamp(timestamp_seconds, track, *sampleTables);
    
    if (success) {
        currentSampleIndex = track.currentSampleIndex;
        
        // Update position to actual seek position (may be different due to keyframe alignment)
        double actualTimestamp = sampleTables->SampleToTime(track.currentSampleIndex);
        m_position_ms = static_cast<uint64_t>(actualTimestamp * 1000.0);
        
        m_eof = false;
        
        // Update all tracks to maintain synchronization (if multiple tracks exist)
        for (auto& otherTrack : audioTracks) {
            if (otherTrack.trackId != track.trackId) {
                // Seek other tracks to equivalent position
                seekingEngine->SeekToTimestamp(timestamp_seconds, otherTrack, *sampleTables);
            }
        }
    }
    
    return success;
}

bool ISODemuxer::isEOF() const {
    return m_eof;
}

uint64_t ISODemuxer::getDuration() const {
    return m_duration_ms;
}

uint64_t ISODemuxer::getPosition() const {
    return m_position_ms;
}

bool ISODemuxer::ParseMovieBoxWithTracks(uint64_t offset, uint64_t size) {
    bool success = boxParser->ParseBoxRecursively(offset, size, 
        [this](const BoxHeader& header, uint64_t boxOffset) {
            switch (header.type) {
                case BOX_MVHD:
                    // Movie header - contains global timescale and duration
                    return true; // Skip for now, will implement in later tasks
                case BOX_TRAK:
                    // Track box - parse for audio tracks
                    {
                        AudioTrackInfo track = {};
                        if (boxParser->ParseTrackBox(header.dataOffset, 
                                                   header.size - (header.dataOffset - boxOffset), 
                                                   track)) {
                            audioTracks.push_back(track);
                        }
                        return true;
                    }
                case BOX_UDTA:
                    // User data - extract metadata
                    {
                        std::shared_ptr<IOHandler> sharedHandler(m_handler.get(), [](IOHandler*) {
                            // Custom deleter that does nothing
                        });
                        auto extractedMetadata = metadataExtractor->ExtractMetadata(sharedHandler, 
                                                                                   header.dataOffset, 
                                                                                   header.size - (header.dataOffset - boxOffset));
                        // Merge extracted metadata with existing metadata
                        for (const auto& pair : extractedMetadata) {
                            m_metadata[pair.first] = pair.second;
                        }
                        return true;
                    }
                default:
                    return boxParser->SkipUnknownBox(header);
            }
        });
    
    // After parsing all tracks, build sample tables for the first audio track
    if (success && !audioTracks.empty()) {
        // Build sample tables for the first track (for now)
        const AudioTrackInfo& firstTrack = audioTracks[0];
        if (!firstTrack.sampleTableInfo.chunkOffsets.empty()) {
            if (!sampleTables->BuildSampleTables(firstTrack.sampleTableInfo)) {
                // Sample table validation failed
                return false;
            }
        }
    }
    
    return success;
}

MediaChunk ISODemuxer::ExtractSampleData(uint32_t stream_id, const AudioTrackInfo& track, 
                                         const SampleTableManager::SampleInfo& sampleInfo) {
    MediaChunk chunk;
    chunk.stream_id = stream_id;
    chunk.timestamp_samples = track.currentSampleIndex;
    chunk.is_keyframe = sampleInfo.isKeyframe;
    chunk.file_offset = sampleInfo.offset;
    
    // Validate sample information
    if (sampleInfo.size == 0 || sampleInfo.offset == 0) {
        return chunk; // Return empty chunk
    }
    
    // Validate file offset is within bounds
    m_handler->seek(0, SEEK_END);
    off_t file_size = m_handler->tell();
    
    if (sampleInfo.offset >= static_cast<uint64_t>(file_size) || 
        sampleInfo.offset + sampleInfo.size > static_cast<uint64_t>(file_size)) {
        // Sample extends beyond file - corrupted or incomplete file
        return chunk;
    }
    
    // Allocate buffer for sample data
    chunk.data.resize(sampleInfo.size);
    
    // Seek to sample location in mdat box
    if (m_handler->seek(static_cast<long>(sampleInfo.offset), SEEK_SET) != 0) {
        chunk.data.clear();
        return chunk;
    }
    
    // Read sample data from file
    size_t bytes_read = m_handler->read(chunk.data.data(), 1, sampleInfo.size);
    
    if (bytes_read != sampleInfo.size) {
        // Partial read - resize buffer to actual data read
        chunk.data.resize(bytes_read);
        
        // If we read significantly less than expected, this might be an error
        if (bytes_read < sampleInfo.size / 2) {
            chunk.data.clear();
            return chunk;
        }
    }
    
    // Apply codec-specific processing if needed
    ProcessCodecSpecificData(chunk, track);
    
    return chunk;
}

void ISODemuxer::ProcessCodecSpecificData(MediaChunk& chunk, const AudioTrackInfo& track) {
    // Apply codec-specific processing based on track codec type
    if (chunk.data.empty()) {
        return;
    }
    
    if (track.codecType == "aac") {
        // AAC samples are typically already properly formatted
        // No additional processing needed for raw AAC frames
        
    } else if (track.codecType == "alac") {
        // ALAC samples are already properly formatted
        // No additional processing needed
        
    } else if (track.codecType == "ulaw" || track.codecType == "alaw") {
        // Telephony codecs - samples are raw companded data
        // Ensure proper sample alignment for 8-bit samples
        // No additional processing needed as samples are already in correct format
        
    } else if (track.codecType == "pcm") {
        // PCM samples may need endianness correction or padding
        // For now, assume samples are correctly formatted in the container
        // Future enhancement: handle different PCM variants (little/big endian, etc.)
        
    }
    
    // For all codecs, ensure we have valid data
    if (chunk.data.empty()) {
        // If processing resulted in empty data, mark as invalid
        chunk.stream_id = 0;
    }
}

bool SampleTableManager::BuildExpandedSampleToChunkMapping(const SampleTableInfo& rawTables, 
                                                        std::vector<uint32_t>& expandedMapping) {
    expandedMapping.clear();
    
    if (rawTables.chunkOffsets.empty()) {
        return false;
    }
    
    size_t totalChunks = rawTables.chunkOffsets.size();
    expandedMapping.reserve(totalChunks);
    
    if (!rawTables.sampleToChunkEntries.empty()) {
        // Use the proper sample-to-chunk entries to build expanded mapping
        size_t entryIndex = 0;
        uint32_t currentSamplesPerChunk = rawTables.sampleToChunkEntries[0].samplesPerChunk;
        
        for (size_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
            // Check if we need to move to the next sample-to-chunk entry
            if (entryIndex + 1 < rawTables.sampleToChunkEntries.size()) {
                const auto& nextEntry = rawTables.sampleToChunkEntries[entryIndex + 1];
                if (chunkIndex >= nextEntry.firstChunk) {
                    entryIndex++;
                    currentSamplesPerChunk = nextEntry.samplesPerChunk;
                }
            }
            
            expandedMapping.push_back(currentSamplesPerChunk);
        }
    } else if (!rawTables.samplesPerChunk.empty()) {
        // Fallback to legacy samplesPerChunk array (deprecated)
        for (size_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
            if (chunkIndex < rawTables.samplesPerChunk.size()) {
                expandedMapping.push_back(rawTables.samplesPerChunk[chunkIndex]);
            } else {
                // Use the last known value
                expandedMapping.push_back(rawTables.samplesPerChunk.back());
            }
        }
    } else {
        // No sample-to-chunk information available - assume 1 sample per chunk
        expandedMapping.assign(totalChunks, 1);
    }
    
    return true;
}

std::map<std::string, std::string> ISODemuxer::getMetadata() const {
    return m_metadata;
}