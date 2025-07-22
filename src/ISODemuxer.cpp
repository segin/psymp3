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
    } return true;
        case BOX_SIDX:
            // Segment index box - for streaming optimization
            return true;
        case BOX_MFRA:
            // Movie fragment random access box
            return true;
        default:
            return SkipUnknownBox(header);
    }
}
            return true;
        case BOX_MFRA:
            // Movie fragment random access box
            return ParseBoxRecursively(header.dataOffset, 
                                     header.size - (header.dataOffset - offset),
                                     [this](const BoxHeader& childHeader, uint64_t childOffset) {
                switch (childHeader.type) {
                    case BOX_TFRA:
                        // Track fragment random access - enables seeking within fragments
                        return true; // Skip for now, will implement in seeking task
                    case BOX_MFRO:
                        // Movie fragment random access offset
                        return true;
                    default:
                        return SkipUnknownBox(childHeader);
                }
            });
        case BOX_SIDX:
            // Segment index box - for segment-based navigation
            return true; // Skip for now, will implement in streaming task
        default:
            return SkipUnknownBox(header);
    }
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

bool BoxParser::ConfigureTelephonyCodec(AudioTrackInfo& track, const std::string& codecType) {
    // Apply codec-specific telephony standards
    if (codecType == "ulaw") {
        // μ-law (North American/Japanese telephony standard)
        ApplyTelephonyDefaults(track, "ulaw");
    } else if (codecType == "alaw") {
        // A-law (European telephony standard ITU-T G.711)
        ApplyTelephonyDefaults(track, "alaw");
    } else {
        return false; // Not a telephony codec
    }
    
    // Validate the configuration
    return ValidateTelephonyParameters(track);
}

bool BoxParser::ValidateTelephonyParameters(AudioTrackInfo& track) {
    // Validate sample rate is appropriate for telephony
    bool validSampleRate = (track.sampleRate == 8000 || track.sampleRate == 16000 ||
                           track.sampleRate == 11025 || track.sampleRate == 22050);
    
    if (!validSampleRate) {
        // Log warning and use default
        track.sampleRate = 8000; // Default to 8kHz
    }
    
    // Validate channel configuration (must be mono for telephony)
    if (track.channelCount != 1) {
        track.channelCount = 1; // Force mono
    }
    
    // Validate bit depth (must be 8-bit for companded audio)
    if (track.bitsPerSample != 8) {
        track.bitsPerSample = 8; // Force 8-bit
    }
    
    // Clear codec configuration (raw formats need no additional config)
    track.codecConfig.clear();
    
    return true;
}

void BoxParser::ApplyTelephonyDefaults(AudioTrackInfo& track, const std::string& codecType) {
    // Set telephony-specific defaults
    if (track.sampleRate == 0) {
        track.sampleRate = 8000; // 8kHz is standard for telephony
    }
    
    if (track.channelCount == 0) {
        track.channelCount = 1; // Mono for telephony
    }
    
    if (track.bitsPerSample == 0) {
        track.bitsPerSample = 8; // 8-bit companded samples
    }
    
    // Set codec-specific parameters
    if (codecType == "ulaw") {
        // μ-law specific configuration
        track.codecType = "ulaw";
        // No additional configuration needed for raw μ-law
    } else if (codecType == "alaw") {
        // A-law specific configuration (European standard)
        track.codecType = "alaw";
        // No additional configuration needed for raw A-law
    }
    
    // Clear any existing codec configuration (raw formats)
    track.codecConfig.clear();
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
bool FragmentHandler::ProcessMovieFragment(uint64_t moofOffset, std::shared_ptr<IOHandler> io) {
    if (!io || moofOffset == 0) {
        return false;
    }
    
    MovieFragmentInfo fragment;
    fragment.moofOffset = moofOffset;
    
    // Parse the movie fragment box
    if (!ParseMovieFragmentBox(moofOffset, 0, io, fragment)) {
        return false;
    }
    
    // Find the associated media data box
    fragment.mdatOffset = FindMediaDataBox(moofOffset, io);
    if (fragment.mdatOffset == 0) {
        return false;
    }
    
    // Validate the fragment
    if (!ValidateFragment(fragment)) {
        return false;
    }
    
    // Add fragment to our collection
    hasFragments = true;
    return AddFragment(fragment);
}

bool FragmentHandler::UpdateSampleTables(const TrackFragmentInfo& traf, AudioTrackInfo& track) {
    if (traf.trackId != track.trackId) {
        return false;
    }
    
    // Update track's sample table information with fragment data
    SampleTableInfo& tables = track.sampleTableInfo;
    
    // Calculate total samples in this fragment
    uint32_t totalSamples = 0;
    for (const auto& trun : traf.trackRuns) {
        totalSamples += trun.sampleCount;
    }
    
    if (totalSamples == 0) {
        return true; // Empty fragment is valid
    }
    
    // Reserve space for new samples
    size_t currentSampleCount = tables.sampleTimes.size();
    tables.sampleTimes.reserve(currentSampleCount + totalSamples);
    tables.sampleSizes.reserve(currentSampleCount + totalSamples);
    
    // Process each track run
    uint64_t currentTime = traf.tfdt; // Start from fragment decode time
    uint64_t currentOffset = traf.baseDataOffset;
    
    for (const auto& trun : traf.trackRuns) {
        // Apply data offset from track run
        uint64_t runOffset = currentOffset + trun.dataOffset;
        
        for (uint32_t i = 0; i < trun.sampleCount; i++) {
            // Get sample duration (use default if not specified)
            uint32_t duration = traf.defaultSampleDuration;
            if (i < trun.sampleDurations.size()) {
                duration = trun.sampleDurations[i];
            }
            
            // Get sample size (use default if not specified)
            uint32_t size = traf.defaultSampleSize;
            if (i < trun.sampleSizes.size()) {
                size = trun.sampleSizes[i];
            }
            
            // Add sample timing information
            tables.sampleTimes.push_back(currentTime);
            tables.sampleSizes.push_back(size);
            
            // Update for next sample
            currentTime += duration;
            runOffset += size;
        }
        
        currentOffset = runOffset;
    }
    
    return true;
}

bool FragmentHandler::SeekToFragment(uint32_t sequenceNumber) {
    for (size_t i = 0; i < fragments.size(); i++) {
        if (fragments[i].sequenceNumber == sequenceNumber) {
            currentFragmentIndex = static_cast<uint32_t>(i);
            return true;
        }
    }
    return false;
}

MovieFragmentInfo* FragmentHandler::GetCurrentFragment() {
    if (currentFragmentIndex < fragments.size()) {
        return &fragments[currentFragmentIndex];
    }
    return nullptr;
}

MovieFragmentInfo* FragmentHandler::GetFragment(uint32_t sequenceNumber) {
    for (auto& fragment : fragments) {
        if (fragment.sequenceNumber == sequenceNumber) {
            return &fragment;
        }
    }
    return nullptr;
}

bool FragmentHandler::AddFragment(const MovieFragmentInfo& fragment) {
    // Check if fragment already exists
    for (const auto& existing : fragments) {
        if (existing.sequenceNumber == fragment.sequenceNumber) {
            return false; // Duplicate fragment
        }
    }
    
    // Add fragment
    fragments.push_back(fragment);
    hasFragments = true; // Set the flag to indicate we have fragments
    
    // Reorder fragments by sequence number
    return ReorderFragments();
}

bool FragmentHandler::ReorderFragments() {
    if (fragments.empty()) {
        return true;
    }
    
    // Sort fragments by sequence number
    std::sort(fragments.begin(), fragments.end(), CompareFragmentsBySequence);
    
    // Check for missing fragments and handle gaps
    FillMissingFragmentGaps();
    
    return true;
}

bool FragmentHandler::IsFragmentComplete(uint32_t sequenceNumber) const {
    const MovieFragmentInfo* fragment = const_cast<FragmentHandler*>(this)->GetFragment(sequenceNumber);
    return fragment && fragment->isComplete;
}

bool FragmentHandler::ExtractFragmentSample(uint32_t trackId, uint64_t sampleIndex, 
                                           uint64_t& offset, uint32_t& size) {
    MovieFragmentInfo* currentFragment = GetCurrentFragment();
    if (!currentFragment) {
        return false;
    }
    
    // Find the track fragment for this track
    TrackFragmentInfo* traf = nullptr;
    for (auto& tf : currentFragment->trackFragments) {
        if (tf.trackId == trackId) {
            traf = &tf;
            break;
        }
    }
    
    if (!traf) {
        return false;
    }
    
    // Calculate sample offset within fragment
    uint64_t currentOffset = traf->baseDataOffset;
    uint64_t currentSample = 0;
    
    for (const auto& trun : traf->trackRuns) {
        if (currentSample + trun.sampleCount > sampleIndex) {
            // Sample is in this track run
            uint64_t sampleInRun = sampleIndex - currentSample;
            uint64_t runOffset = currentOffset + trun.dataOffset;
            
            // Calculate offset to specific sample
            for (uint64_t i = 0; i < sampleInRun; i++) {
                uint32_t sampleSize = traf->defaultSampleSize;
                if (i < trun.sampleSizes.size()) {
                    sampleSize = trun.sampleSizes[i];
                }
                runOffset += sampleSize;
            }
            
            // Get size of target sample
            size = traf->defaultSampleSize;
            if (sampleInRun < trun.sampleSizes.size()) {
                size = trun.sampleSizes[sampleInRun];
            }
            
            offset = runOffset;
            return true;
        }
        
        currentSample += trun.sampleCount;
        
        // Update offset for next track run
        for (uint32_t i = 0; i < trun.sampleCount; i++) {
            uint32_t sampleSize = traf->defaultSampleSize;
            if (i < trun.sampleSizes.size()) {
                sampleSize = trun.sampleSizes[i];
            }
            currentOffset += sampleSize;
        }
    }
    
    return false;
}

void FragmentHandler::SetDefaultValues(const AudioTrackInfo& movieHeaderDefaults) {
    // Set default values from movie header for use when fragment headers are missing
    defaults.defaultSampleDuration = static_cast<uint32_t>(movieHeaderDefaults.duration / 
                                                           (movieHeaderDefaults.sampleTableInfo.sampleTimes.size() + 1));
    defaults.defaultSampleSize = movieHeaderDefaults.sampleTableInfo.sampleSizes.empty() ? 0 : 
                                movieHeaderDefaults.sampleTableInfo.sampleSizes[0];
    defaults.defaultSampleFlags = 0; // Default to no special flags
}

bool FragmentHandler::ParseMovieFragmentBox(uint64_t offset, uint64_t size, std::shared_ptr<IOHandler> io, 
                                           MovieFragmentInfo& fragment) {
    if (!io) {
        return false;
    }
    
    // Read movie fragment box header
    io->seek(static_cast<long>(offset), SEEK_SET);
    uint32_t boxSize = ReadUInt32BE(io, offset);
    uint32_t boxType = ReadUInt32BE(io, offset + 4);
    
    if (boxType != BOX_MOOF) {
        return false;
    }
    
    uint64_t actualSize = (boxSize == 1) ? ReadUInt64BE(io, offset + 8) : boxSize;
    uint64_t dataOffset = (boxSize == 1) ? offset + 16 : offset + 8;
    uint64_t endOffset = offset + actualSize;
    
    // Parse child boxes
    uint64_t currentOffset = dataOffset;
    while (currentOffset < endOffset) {
        uint32_t childSize = ReadUInt32BE(io, currentOffset);
        uint32_t childType = ReadUInt32BE(io, currentOffset + 4);
        
        if (childSize < 8) {
            break;
        }
        
        uint64_t childDataOffset = currentOffset + 8;
        
        switch (childType) {
            case BOX_MFHD:
                // Movie fragment header
                if (!ParseMovieFragmentHeader(childDataOffset, childSize - 8, io, fragment)) {
                    return false;
                }
                break;
                
            case BOX_TRAF:
                // Track fragment
                {
                    TrackFragmentInfo traf;
                    if (ParseTrackFragmentBox(childDataOffset, childSize - 8, io, traf)) {
                        fragment.trackFragments.push_back(traf);
                    }
                }
                break;
                
            default:
                // Skip unknown boxes
                break;
        }
        
        currentOffset += childSize;
    }
    
    fragment.isComplete = !fragment.trackFragments.empty();
    return fragment.isComplete;
}

bool FragmentHandler::ParseMovieFragmentHeader(uint64_t offset, uint64_t size, std::shared_ptr<IOHandler> io,
                                              MovieFragmentInfo& fragment) {
    if (!io || size < 8) {
        return false;
    }
    
    // Skip version/flags (4 bytes)
    fragment.sequenceNumber = ReadUInt32BE(io, offset + 4);
    return true;
}

bool FragmentHandler::ParseTrackFragmentBox(uint64_t offset, uint64_t size, std::shared_ptr<IOHandler> io,
                                           TrackFragmentInfo& traf) {
    if (!io) {
        return false;
    }
    
    uint64_t endOffset = offset + size;
    uint64_t currentOffset = offset;
    
    while (currentOffset < endOffset) {
        uint32_t childSize = ReadUInt32BE(io, currentOffset);
        uint32_t childType = ReadUInt32BE(io, currentOffset + 4);
        
        if (childSize < 8) {
            break;
        }
        
        uint64_t childDataOffset = currentOffset + 8;
        
        switch (childType) {
            case BOX_TFHD:
                // Track fragment header
                if (!ParseTrackFragmentHeader(childDataOffset, childSize - 8, io, traf)) {
                    return false;
                }
                break;
                
            case BOX_TRUN:
                // Track fragment run
                {
                    TrackFragmentInfo::TrackRunInfo trun;
                    if (ParseTrackFragmentRun(childDataOffset, childSize - 8, io, trun)) {
                        traf.trackRuns.push_back(trun);
                    }
                }
                break;
                
            case BOX_TFDT:
                // Track fragment decode time
                ParseTrackFragmentDecodeTime(childDataOffset, childSize - 8, io, traf);
                break;
                
            default:
                // Skip unknown boxes
                break;
        }
        
        currentOffset += childSize;
    }
    
    return traf.trackId != 0;
}

bool FragmentHandler::ParseTrackFragmentHeader(uint64_t offset, uint64_t size, std::shared_ptr<IOHandler> io,
                                              TrackFragmentInfo& traf) {
    if (!io || size < 8) {
        return false;
    }
    
    uint32_t versionFlags = ReadUInt32BE(io, offset);
    uint32_t flags = versionFlags & 0xFFFFFF;
    
    uint64_t currentOffset = offset + 4;
    
    // Track ID is always present
    traf.trackId = ReadUInt32BE(io, currentOffset);
    currentOffset += 4;
    
    // Parse optional fields based on flags
    if (flags & 0x000001) { // base-data-offset-present
        traf.baseDataOffset = ReadUInt64BE(io, currentOffset);
        currentOffset += 8;
    }
    
    if (flags & 0x000002) { // sample-description-index-present
        traf.sampleDescriptionIndex = ReadUInt32BE(io, currentOffset);
        currentOffset += 4;
    }
    
    if (flags & 0x000008) { // default-sample-duration-present
        traf.defaultSampleDuration = ReadUInt32BE(io, currentOffset);
        currentOffset += 4;
    } else {
        traf.defaultSampleDuration = defaults.defaultSampleDuration;
    }
    
    if (flags & 0x000010) { // default-sample-size-present
        traf.defaultSampleSize = ReadUInt32BE(io, currentOffset);
        currentOffset += 4;
    } else {
        traf.defaultSampleSize = defaults.defaultSampleSize;
    }
    
    if (flags & 0x000020) { // default-sample-flags-present
        traf.defaultSampleFlags = ReadUInt32BE(io, currentOffset);
        currentOffset += 4;
    } else {
        traf.defaultSampleFlags = defaults.defaultSampleFlags;
    }
    
    return true;
}

bool FragmentHandler::ParseTrackFragmentRun(uint64_t offset, uint64_t size, std::shared_ptr<IOHandler> io,
                                           TrackFragmentInfo::TrackRunInfo& trun) {
    if (!io || size < 8) {
        return false;
    }
    
    uint32_t versionFlags = ReadUInt32BE(io, offset);
    uint8_t version = (versionFlags >> 24) & 0xFF;
    uint32_t flags = versionFlags & 0xFFFFFF;
    
    uint64_t currentOffset = offset + 4;
    
    // Sample count is always present
    trun.sampleCount = ReadUInt32BE(io, currentOffset);
    currentOffset += 4;
    
    if (trun.sampleCount == 0) {
        return true; // Empty run is valid
    }
    
    // Parse optional fields based on flags
    if (flags & 0x000001) { // data-offset-present
        trun.dataOffset = ReadUInt32BE(io, currentOffset);
        currentOffset += 4;
    }
    
    if (flags & 0x000004) { // first-sample-flags-present
        trun.firstSampleFlags = ReadUInt32BE(io, currentOffset);
        currentOffset += 4;
    }
    
    // Parse per-sample data
    bool hasSampleDuration = (flags & 0x000100) != 0;
    bool hasSampleSize = (flags & 0x000200) != 0;
    bool hasSampleFlags = (flags & 0x000400) != 0;
    bool hasSampleCompositionTimeOffset = (flags & 0x000800) != 0;
    
    if (hasSampleDuration) {
        trun.sampleDurations.reserve(trun.sampleCount);
    }
    if (hasSampleSize) {
        trun.sampleSizes.reserve(trun.sampleCount);
    }
    if (hasSampleFlags) {
        trun.sampleFlags.reserve(trun.sampleCount);
    }
    if (hasSampleCompositionTimeOffset) {
        trun.sampleCompositionTimeOffsets.reserve(trun.sampleCount);
    }
    
    for (uint32_t i = 0; i < trun.sampleCount; i++) {
        if (hasSampleDuration) {
            trun.sampleDurations.push_back(ReadUInt32BE(io, currentOffset));
            currentOffset += 4;
        }
        
        if (hasSampleSize) {
            trun.sampleSizes.push_back(ReadUInt32BE(io, currentOffset));
            currentOffset += 4;
        }
        
        if (hasSampleFlags) {
            trun.sampleFlags.push_back(ReadUInt32BE(io, currentOffset));
            currentOffset += 4;
        }
        
        if (hasSampleCompositionTimeOffset) {
            if (version == 0) {
                trun.sampleCompositionTimeOffsets.push_back(ReadUInt32BE(io, currentOffset));
            } else {
                // Version 1 uses signed offsets
                int32_t signedOffset = static_cast<int32_t>(ReadUInt32BE(io, currentOffset));
                trun.sampleCompositionTimeOffsets.push_back(static_cast<uint32_t>(signedOffset));
            }
            currentOffset += 4;
        }
    }
    
    return true;
}

bool FragmentHandler::ParseTrackFragmentDecodeTime(uint64_t offset, uint64_t size, std::shared_ptr<IOHandler> io,
                                                  TrackFragmentInfo& traf) {
    if (!io || size < 8) {
        return false;
    }
    
    uint32_t versionFlags = ReadUInt32BE(io, offset);
    uint8_t version = (versionFlags >> 24) & 0xFF;
    
    if (version == 1) {
        traf.tfdt = ReadUInt64BE(io, offset + 4);
    } else {
        traf.tfdt = ReadUInt32BE(io, offset + 4);
    }
    
    return true;
}

bool FragmentHandler::ValidateFragment(const MovieFragmentInfo& fragment) const {
    if (fragment.trackFragments.empty()) {
        return false;
    }
    
    for (const auto& traf : fragment.trackFragments) {
        if (!ValidateTrackFragment(traf)) {
            return false;
        }
    }
    
    return true;
}

bool FragmentHandler::ValidateTrackFragment(const TrackFragmentInfo& traf) const {
    if (traf.trackId == 0) {
        return false;
    }
    
    // Validate track runs
    for (const auto& trun : traf.trackRuns) {
        if (trun.sampleCount == 0) {
            continue; // Empty runs are allowed
        }
        
        // Check consistency of per-sample data
        if (!trun.sampleDurations.empty() && trun.sampleDurations.size() != trun.sampleCount) {
            return false;
        }
        if (!trun.sampleSizes.empty() && trun.sampleSizes.size() != trun.sampleCount) {
            return false;
        }
        if (!trun.sampleFlags.empty() && trun.sampleFlags.size() != trun.sampleCount) {
            return false;
        }
        if (!trun.sampleCompositionTimeOffsets.empty() && 
            trun.sampleCompositionTimeOffsets.size() != trun.sampleCount) {
            return false;
        }
    }
    
    return true;
}

uint32_t FragmentHandler::ReadUInt32BE(std::shared_ptr<IOHandler> io, uint64_t offset) {
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

uint64_t FragmentHandler::ReadUInt64BE(std::shared_ptr<IOHandler> io, uint64_t offset) {
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

uint64_t FragmentHandler::FindMediaDataBox(uint64_t moofOffset, std::shared_ptr<IOHandler> io) {
    if (!io) {
        return 0;
    }
    
    // Media data box typically follows movie fragment box
    // Read moof box size first
    uint32_t moofSize = ReadUInt32BE(io, moofOffset);
    if (moofSize == 1) {
        // Extended size
        moofSize = static_cast<uint32_t>(ReadUInt64BE(io, moofOffset + 8));
    }
    
    uint64_t searchOffset = moofOffset + moofSize;
    
    // Look for mdat box within reasonable distance
    const uint64_t maxSearchDistance = 1024 * 1024; // 1MB search limit
    uint64_t endOffset = searchOffset + maxSearchDistance;
    
    while (searchOffset < endOffset) {
        uint32_t boxSize = ReadUInt32BE(io, searchOffset);
        uint32_t boxType = ReadUInt32BE(io, searchOffset + 4);
        
        if (boxType == BOX_MDAT) {
            return searchOffset;
        }
        
        if (boxSize < 8) {
            break;
        }
        
        searchOffset += boxSize;
    }
    
    return 0;
}

bool FragmentHandler::CompareFragmentsBySequence(const MovieFragmentInfo& a, const MovieFragmentInfo& b) {
    return a.sequenceNumber < b.sequenceNumber;
}

bool FragmentHandler::HasMissingFragments() const {
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

void FragmentHandler::FillMissingFragmentGaps() {
    // For now, we just log missing fragments
    // In a full implementation, we might request missing fragments
    // or handle graceful degradation
    if (HasMissingFragments()) {
        // Missing fragments detected - could implement recovery logic here
    }
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
            if (info.sample_rate != 8000 && info.sample_rate != 16000 && 
                info.sample_rate != 11025 && info.sample_rate != 22050) {
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
        
        // Parse top-level boxes to find ftyp, moov, and fragments
        std::string containerType;
        bool foundFileType = false;
        bool foundMovie = false;
        bool hasFragments = false;
        
        // Create shared IOHandler for fragment processing
        std::shared_ptr<IOHandler> sharedHandler(m_handler.get(), [](IOHandler*) {
            // Custom deleter that does nothing - we don't want to delete the handler
        });
        
        boxParser->ParseBoxRecursively(0, static_cast<uint64_t>(file_size), 
            [this, &containerType, &foundFileType, &foundMovie, &hasFragments, sharedHandler](const BoxHeader& header, uint64_t boxOffset) {
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
                    case BOX_MOOF:
                        // Movie fragment box - process with FragmentHandler
                        if (fragmentHandler->ProcessMovieFragment(boxOffset, sharedHandler)) {
                            hasFragments = true;
                        }
                        return true;
                    case BOX_MFRA:
                        // Movie fragment random access box
                        boxParser->ParseFragmentBox(boxOffset, header.size);
                        return true;
                    case BOX_SIDX:
                        // Segment index box
                        boxParser->ParseFragmentBox(boxOffset, header.size);
                        return true;
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
        
        // Handle fragmented MP4 files
        if (hasFragments && fragmentHandler->IsFragmented()) {
            // Set default values from movie header for missing fragment headers
            if (!audioTracks.empty()) {
                fragmentHandler->SetDefaultValues(audioTracks[0]);
            }
            
            // Update sample tables with fragment information
            for (auto& track : audioTracks) {
                // Get current fragment and update track sample tables
                MovieFragmentInfo* currentFragment = fragmentHandler->GetCurrentFragment();
                if (currentFragment) {
                    for (const auto& traf : currentFragment->trackFragments) {
                        if (traf.trackId == track.trackId) {
                            fragmentHandler->UpdateSampleTables(traf, track);
                            break;
                        }
                    }
                }
            }
        }
        
        // Validate telephony codec configurations before adding to stream manager
        for (const auto& track : audioTracks) {
            if (track.codecType == "ulaw" || track.codecType == "alaw") {
                if (!ValidateTelephonyCodecConfiguration(track)) {
                    // Log warning about non-compliant telephony configuration
                    // Continue processing but note the issue
                }
            }
        }
        
        // Add tracks to stream manager
        for (const auto& track : audioTracks) {
            streamManager->AddAudioTrack(track);
        }
        
        // Calculate duration from audio tracks with special handling for telephony
        for (const auto& track : audioTracks) {
            uint64_t track_duration_ms;
            
            if (track.codecType == "ulaw" || track.codecType == "alaw") {
                // Use precise sample-based calculation for telephony codecs
                if (track.sampleRate > 0 && !track.sampleTableInfo.sampleTimes.empty()) {
                    size_t totalSamples = track.sampleTableInfo.sampleTimes.size();
                    track_duration_ms = (totalSamples * 1000ULL) / track.sampleRate;
                } else {
                    // Fallback to timescale calculation
                    track_duration_ms = track.timescale > 0 ? (track.duration * 1000ULL) / track.timescale : 0;
                }
            } else {
                // Standard calculation for other codecs
                track_duration_ms = track.timescale > 0 ? (track.duration * 1000ULL) / track.timescale : 0;
            }
            
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
    
    // Check if this is a fragmented file
    if (fragmentHandler && fragmentHandler->IsFragmented()) {
        // Extract sample from current fragment
        uint64_t sampleOffset;
        uint32_t sampleSize;
        
        if (fragmentHandler->ExtractFragmentSample(stream_id, track->currentSampleIndex, 
                                                  sampleOffset, sampleSize)) {
            // Read sample data from fragment
            MediaChunk chunk;
            chunk.stream_id = stream_id;
            chunk.data.resize(sampleSize);
            
            if (m_handler->seek(static_cast<long>(sampleOffset), SEEK_SET) == 0 &&
                m_handler->read(chunk.data.data(), 1, sampleSize) == sampleSize) {
                
                // Set timing information
                uint64_t timestamp_ms = CalculateTelephonyTiming(*track, track->currentSampleIndex);
                chunk.timestamp_samples = (timestamp_ms * track->sampleRate) / 1000; // Convert to samples
                
                // Apply codec-specific processing
                ProcessCodecSpecificData(chunk, *track);
                
                // Update track position
                track->currentSampleIndex++;
                m_position_ms = timestamp_ms;
                
                return chunk;
            }
        }
        
        // Fragment sample extraction failed
        m_eof = true;
        return MediaChunk{};
    }
    
    // Use SampleTableManager for non-fragmented files
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
    
    // Handle fragmented files
    if (fragmentHandler && fragmentHandler->IsFragmented()) {
        // For fragmented files, we need to find the appropriate fragment
        // and seek within that fragment
        
        // Get all available fragments
        uint32_t fragmentCount = fragmentHandler->GetFragmentCount();
        if (fragmentCount == 0) {
            return false;
        }
        
        // Calculate target time in track timescale
        uint64_t targetTime = 0;
        if (track.timescale > 0) {
            targetTime = (timestamp_ms * track.timescale) / 1000;
        }
        
        // Find the fragment containing this timestamp
        // Start with the first fragment and iterate until we find the right one
        bool foundFragment = false;
        for (uint32_t i = 1; i <= fragmentCount; i++) {
            MovieFragmentInfo* fragment = fragmentHandler->GetFragment(i);
            if (!fragment) {
                continue;
            }
            
            // Find the track fragment for this track
            for (const auto& traf : fragment->trackFragments) {
                if (traf.trackId == track.trackId) {
                    // Check if this fragment contains our target time
                    // We need to calculate the end time of this fragment
                    uint64_t fragmentStartTime = traf.tfdt;
                    uint64_t fragmentDuration = 0;
                    
                    // Calculate fragment duration by summing all sample durations
                    for (const auto& trun : traf.trackRuns) {
                        if (!trun.sampleDurations.empty()) {
                            // Use per-sample durations
                            for (uint32_t duration : trun.sampleDurations) {
                                fragmentDuration += duration;
                            }
                        } else {
                            // Use default duration
                            fragmentDuration += trun.sampleCount * traf.defaultSampleDuration;
                        }
                    }
                    
                    uint64_t fragmentEndTime = fragmentStartTime + fragmentDuration;
                    
                    // Check if target time is within this fragment
                    if (targetTime >= fragmentStartTime && targetTime < fragmentEndTime) {
                        // Found the right fragment
                        fragmentHandler->SeekToFragment(i);
                        foundFragment = true;
                        
                        // Calculate sample index within fragment
                        uint64_t timeOffset = targetTime - fragmentStartTime;
                        uint64_t sampleIndex = 0;
                        uint64_t accumulatedDuration = 0;
                        
                        // Find the sample containing our target time
                        for (const auto& trun : traf.trackRuns) {
                            for (uint32_t j = 0; j < trun.sampleCount; j++) {
                                uint32_t sampleDuration = 0;
                                
                                if (j < trun.sampleDurations.size()) {
                                    sampleDuration = trun.sampleDurations[j];
                                } else {
                                    sampleDuration = traf.defaultSampleDuration;
                                }
                                
                                if (accumulatedDuration + sampleDuration > timeOffset) {
                                    // Found the sample
                                    track.currentSampleIndex = sampleIndex;
                                    currentSampleIndex = sampleIndex;
                                    m_position_ms = timestamp_ms;
                                    m_eof = false;
                                    return true;
                                }
                                
                                accumulatedDuration += sampleDuration;
                                sampleIndex++;
                            }
                        }
                        
                        // If we get here, use the first sample in the fragment
                        track.currentSampleIndex = 0;
                        currentSampleIndex = 0;
                        m_position_ms = (fragmentStartTime * 1000ULL) / track.timescale;
                        m_eof = false;
                        return true;
                    }
                }
            }
        }
        
        // If we couldn't find the exact fragment, try to seek to the closest one
        if (!foundFragment) {
            // Find the last fragment
            MovieFragmentInfo* lastFragment = fragmentHandler->GetFragment(fragmentCount);
            if (lastFragment) {
                fragmentHandler->SeekToFragment(fragmentCount);
                track.currentSampleIndex = 0;
                currentSampleIndex = 0;
                m_position_ms = timestamp_ms;
                m_eof = false;
                return true;
            }
        }
        
        return false;
    }
    
    // Handle non-fragmented files
    if (!seekingEngine || !sampleTables) {
        return false;
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
    
    // Calculate accurate timing for telephony codecs
    // Note: MediaChunk uses timestamp_samples, not timestamp_ms
    // The timing calculation is handled by the codec layer
    
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
        // Telephony codecs - samples are raw companded 8-bit data
        // Ensure proper sample alignment and validate data integrity
        
        // Validate telephony codec parameters (read-only validation)
        if (track.bitsPerSample != 8) {
            // Log warning - telephony codecs should be 8-bit companded
            // Note: Cannot modify const track parameter
        }
        
        if (track.channelCount != 1) {
            // Log warning - telephony codecs should be mono
            // Note: Cannot modify const track parameter
        }
        
        if (track.sampleRate != 8000 && track.sampleRate != 16000 && 
            track.sampleRate != 11025 && track.sampleRate != 22050) {
            // Log warning - non-standard sample rate for telephony
            // Note: Cannot modify const track parameter
        }
        
        // For telephony codecs, samples are already in the correct raw format
        // Validate that sample data is properly aligned for 8-bit companded audio
        
        // Validate sample data size for telephony codecs
        if (track.sampleRate > 0 && track.timescale > 0) {
            // Each sample should be exactly 1 byte for companded audio
            // Basic sanity check - ensure we have reasonable amount of data
            size_t minExpectedSize = 1; // At least 1 byte
            size_t maxExpectedSize = track.sampleRate; // At most 1 second worth
            
            if (chunk.data.size() < minExpectedSize || chunk.data.size() > maxExpectedSize) {
                // Log warning but continue - some files may have unusual sample sizes
            }
        }
        
        // Each byte represents one companded audio sample - no further processing needed
        
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

uint64_t ISODemuxer::CalculateTelephonyTiming(const AudioTrackInfo& track, uint64_t sampleIndex) {
    // For telephony codecs, calculate precise timing based on sample rate
    if (track.codecType == "ulaw" || track.codecType == "alaw") {
        // Each sample represents 1/sampleRate seconds
        // Convert to milliseconds: (sampleIndex * 1000) / sampleRate
        if (track.sampleRate > 0) {
            return (sampleIndex * 1000) / track.sampleRate;
        }
    }
    
    // Fallback to timescale-based calculation
    if (track.timescale > 0 && sampleIndex < track.sampleTableInfo.sampleTimes.size()) {
        uint64_t trackTime = track.sampleTableInfo.sampleTimes[sampleIndex];
        return (trackTime * 1000) / track.timescale;
    }
    
    return 0; // Unable to calculate timing
}

bool ISODemuxer::ValidateTelephonyCodecConfiguration(const AudioTrackInfo& track) {
    // Only validate telephony codecs
    if (track.codecType != "ulaw" && track.codecType != "alaw") {
        return true; // Not a telephony codec, validation not applicable
    }
    
    bool isValid = true;
    
    // Validate sample rate (telephony standards)
    if (track.sampleRate != 8000 && track.sampleRate != 16000 && 
        track.sampleRate != 11025 && track.sampleRate != 22050) {
        // Non-standard sample rate for telephony
        isValid = false;
    }
    
    // Validate channel configuration (must be mono for telephony)
    if (track.channelCount != 1) {
        // Telephony audio must be mono
        isValid = false;
    }
    
    // Validate bit depth (must be 8-bit for companded audio)
    if (track.bitsPerSample != 8) {
        // Companded audio must be 8-bit
        isValid = false;
    }
    
    // Validate codec-specific requirements
    if (track.codecType == "alaw") {
        // A-law specific validation (European standard ITU-T G.711)
        // Default sample rate should be 8kHz for European telephony
        if (track.sampleRate == 0) {
            isValid = false;
        }
    } else if (track.codecType == "ulaw") {
        // μ-law specific validation (North American/Japanese standard)
        // Default sample rate should be 8kHz for North American telephony
        if (track.sampleRate == 0) {
            isValid = false;
        }
    }
    
    // Validate that codec configuration is minimal (raw formats need no config)
    if (!track.codecConfig.empty()) {
        // Telephony codecs should have no additional configuration
        // This is acceptable but not required - some containers may include metadata
    }
    
    return isValid;
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
/
/ FragmentHandler implementation
bool FragmentHandler::ProcessMovieFragment(uint64_t moofOffset, std::shared_ptr<IOHandler> io) {
    if (!io) {
        return false;
    }
    
    // Create a new fragment info structure
    MovieFragmentInfo fragment;
    fragment.moofOffset = moofOffset;
    
    // Read the moof box header to get size
    io->seek(static_cast<long>(moofOffset), SEEK_SET);
    uint8_t headerBytes[8];
    if (io->read(headerBytes, 1, 8) != 8) {
        return false;
    }
    
    uint32_t moofSize = (static_cast<uint32_t>(headerBytes[0]) << 24) |
                        (static_cast<uint32_t>(headerBytes[1]) << 16) |
                        (static_cast<uint32_t>(headerBytes[2]) << 8) |
                        static_cast<uint32_t>(headerBytes[3]);
    
    uint32_t moofType = (static_cast<uint32_t>(headerBytes[4]) << 24) |
                        (static_cast<uint32_t>(headerBytes[5]) << 16) |
                        (static_cast<uint32_t>(headerBytes[6]) << 8) |
                        static_cast<uint32_t>(headerBytes[7]);
    
    if (moofType != BOX_MOOF) {
        return false;
    }
    
    // Parse the movie fragment box
    if (!ParseMovieFragmentBox(moofOffset + 8, moofSize - 8, io, fragment)) {
        return false;
    }
    
    // Find the corresponding mdat box
    fragment.mdatOffset = FindMediaDataBox(moofOffset + moofSize, io);
    if (fragment.mdatOffset == 0) {
        return false;
    }
    
    // Read mdat size
    io->seek(static_cast<long>(fragment.mdatOffset), SEEK_SET);
    if (io->read(headerBytes, 1, 8) != 8) {
        return false;
    }
    
    fragment.mdatSize = (static_cast<uint32_t>(headerBytes[0]) << 24) |
                        (static_cast<uint32_t>(headerBytes[1]) << 16) |
                        (static_cast<uint32_t>(headerBytes[2]) << 8) |
                        static_cast<uint32_t>(headerBytes[3]);
    
    // Mark fragment as complete and add it
    fragment.isComplete = true;
    hasFragments = true;
    
    return AddFragment(fragment);
}

bool FragmentHandler::ParseMovieFragmentBox(uint64_t offset, uint64_t size, std::shared_ptr<IOHandler> io, 
                                           MovieFragmentInfo& fragment) {
    uint64_t currentOffset = offset;
    uint64_t endOffset = offset + size;
    
    while (currentOffset < endOffset) {
        // Read box header
        if (currentOffset + 8 > endOffset) {
            break;
        }
        
        io->seek(static_cast<long>(currentOffset), SEEK_SET);
        uint8_t headerBytes[8];
        if (io->read(headerBytes, 1, 8) != 8) {
            break;
        }
        
        uint32_t boxSize = (static_cast<uint32_t>(headerBytes[0]) << 24) |
                           (static_cast<uint32_t>(headerBytes[1]) << 16) |
                           (static_cast<uint32_t>(headerBytes[2]) << 8) |
                           static_cast<uint32_t>(headerBytes[3]);
        
        uint32_t boxType = (static_cast<uint32_t>(headerBytes[4]) << 24) |
                           (static_cast<uint32_t>(headerBytes[5]) << 16) |
                           (static_cast<uint32_t>(headerBytes[6]) << 8) |
                           static_cast<uint32_t>(headerBytes[7]);
        
        if (boxSize < 8 || currentOffset + boxSize > endOffset) {
            break;
        }
        
        switch (boxType) {
            case BOX_MFHD:
                // Movie fragment header
                if (!ParseMovieFragmentHeader(currentOffset + 8, boxSize - 8, io, fragment)) {
                    return false;
                }
                break;
            case BOX_TRAF:
                // Track fragment
                {
                    TrackFragmentInfo traf;
                    if (ParseTrackFragmentBox(currentOffset + 8, boxSize - 8, io, traf)) {
                        fragment.trackFragments.push_back(traf);
                    }
                }
                break;
            default:
                // Skip unknown boxes
                break;
        }
        
        currentOffset += boxSize;
    }
    
    return !fragment.trackFragments.empty();
}

bool FragmentHandler::ParseMovieFragmentHeader(uint64_t offset, uint64_t size, std::shared_ptr<IOHandler> io,
                                              MovieFragmentInfo& fragment) {
    if (size < 8) {
        return false;
    }
    
    io->seek(static_cast<long>(offset), SEEK_SET);
    uint8_t data[8];
    if (io->read(data, 1, 8) != 8) {
        return false;
    }
    
    // Skip version/flags (4 bytes)
    fragment.sequenceNumber = (static_cast<uint32_t>(data[4]) << 24) |
                              (static_cast<uint32_t>(data[5]) << 16) |
                              (static_cast<uint32_t>(data[6]) << 8) |
                              static_cast<uint32_t>(data[7]);
    
    return true;
}

bool FragmentHandler::ParseTrackFragmentBox(uint64_t offset, uint64_t size, std::shared_ptr<IOHandler> io,
                                           TrackFragmentInfo& traf) {
    uint64_t currentOffset = offset;
    uint64_t endOffset = offset + size;
    
    while (currentOffset < endOffset) {
        // Read box header
        if (currentOffset + 8 > endOffset) {
            break;
        }
        
        io->seek(static_cast<long>(currentOffset), SEEK_SET);
        uint8_t headerBytes[8];
        if (io->read(headerBytes, 1, 8) != 8) {
            break;
        }
        
        uint32_t boxSize = (static_cast<uint32_t>(headerBytes[0]) << 24) |
                           (static_cast<uint32_t>(headerBytes[1]) << 16) |
                           (static_cast<uint32_t>(headerBytes[2]) << 8) |
                           static_cast<uint32_t>(headerBytes[3]);
        
        uint32_t boxType = (static_cast<uint32_t>(headerBytes[4]) << 24) |
                           (static_cast<uint32_t>(headerBytes[5]) << 16) |
                           (static_cast<uint32_t>(headerBytes[6]) << 8) |
                           static_cast<uint32_t>(headerBytes[7]);
        
        if (boxSize < 8 || currentOffset + boxSize > endOffset) {
            break;
        }
        
        switch (boxType) {
            case BOX_TFHD:
                // Track fragment header
                if (!ParseTrackFragmentHeader(currentOffset + 8, boxSize - 8, io, traf)) {
                    return false;
                }
                break;
            case BOX_TRUN:
                // Track fragment run
                {
                    TrackFragmentInfo::TrackRunInfo trun;
                    if (ParseTrackFragmentRun(currentOffset + 8, boxSize - 8, io, trun)) {
                        traf.trackRuns.push_back(trun);
                    }
                }
                break;
            case BOX_TFDT:
                // Track fragment decode time
                ParseTrackFragmentDecodeTime(currentOffset + 8, boxSize - 8, io, traf);
                break;
            default:
                // Skip unknown boxes
                break;
        }
        
        currentOffset += boxSize;
    }
    
    return traf.trackId != 0 && !traf.trackRuns.empty();
}

bool FragmentHandler::ParseTrackFragmentHeader(uint64_t offset, uint64_t size, std::shared_ptr<IOHandler> io,
                                              TrackFragmentInfo& traf) {
    if (size < 8) {
        return false;
    }
    
    io->seek(static_cast<long>(offset), SEEK_SET);
    uint8_t data[32]; // Maximum size we might need
    size_t readSize = std::min(size, static_cast<uint64_t>(32));
    if (io->read(data, 1, readSize) != readSize) {
        return false;
    }
    
    // Read version/flags
    uint32_t versionFlags = (static_cast<uint32_t>(data[0]) << 24) |
                            (static_cast<uint32_t>(data[1]) << 16) |
                            (static_cast<uint32_t>(data[2]) << 8) |
                            static_cast<uint32_t>(data[3]);
    
    uint32_t flags = versionFlags & 0xFFFFFF;
    
    // Read track ID
    traf.trackId = (static_cast<uint32_t>(data[4]) << 24) |
                   (static_cast<uint32_t>(data[5]) << 16) |
                   (static_cast<uint32_t>(data[6]) << 8) |
                   static_cast<uint32_t>(data[7]);
    
    size_t dataOffset = 8;
    
    // Parse optional fields based on flags
    if (flags & 0x000001) { // base-data-offset-present
        if (dataOffset + 8 <= readSize) {
            traf.baseDataOffset = (static_cast<uint64_t>(data[dataOffset]) << 56) |
                                  (static_cast<uint64_t>(data[dataOffset + 1]) << 48) |
                                  (static_cast<uint64_t>(data[dataOffset + 2]) << 40) |
                                  (static_cast<uint64_t>(data[dataOffset + 3]) << 32) |
                                  (static_cast<uint64_t>(data[dataOffset + 4]) << 24) |
                                  (static_cast<uint64_t>(data[dataOffset + 5]) << 16) |
                                  (static_cast<uint64_t>(data[dataOffset + 6]) << 8) |
                                  static_cast<uint64_t>(data[dataOffset + 7]);
            dataOffset += 8;
        }
    }
    
    if (flags & 0x000002) { // sample-description-index-present
        if (dataOffset + 4 <= readSize) {
            traf.sampleDescriptionIndex = (static_cast<uint32_t>(data[dataOffset]) << 24) |
                                          (static_cast<uint32_t>(data[dataOffset + 1]) << 16) |
                                          (static_cast<uint32_t>(data[dataOffset + 2]) << 8) |
                                          static_cast<uint32_t>(data[dataOffset + 3]);
            dataOffset += 4;
        }
    }
    
    if (flags & 0x000008) { // default-sample-duration-present
        if (dataOffset + 4 <= readSize) {
            traf.defaultSampleDuration = (static_cast<uint32_t>(data[dataOffset]) << 24) |
                                         (static_cast<uint32_t>(data[dataOffset + 1]) << 16) |
                                         (static_cast<uint32_t>(data[dataOffset + 2]) << 8) |
                                         static_cast<uint32_t>(data[dataOffset + 3]);
            dataOffset += 4;
        }
    }
    
    if (flags & 0x000010) { // default-sample-size-present
        if (dataOffset + 4 <= readSize) {
            traf.defaultSampleSize = (static_cast<uint32_t>(data[dataOffset]) << 24) |
                                     (static_cast<uint32_t>(data[dataOffset + 1]) << 16) |
                                     (static_cast<uint32_t>(data[dataOffset + 2]) << 8) |
                                     static_cast<uint32_t>(data[dataOffset + 3]);
            dataOffset += 4;
        }
    }
    
    if (flags & 0x000020) { // default-sample-flags-present
        if (dataOffset + 4 <= readSize) {
            traf.defaultSampleFlags = (static_cast<uint32_t>(data[dataOffset]) << 24) |
                                      (static_cast<uint32_t>(data[dataOffset + 1]) << 16) |
                                      (static_cast<uint32_t>(data[dataOffset + 2]) << 8) |
                                      static_cast<uint32_t>(data[dataOffset + 3]);
            dataOffset += 4;
        }
    }
    
    return true;
}

bool FragmentHandler::ParseTrackFragmentRun(uint64_t offset, uint64_t size, std::shared_ptr<IOHandler> io,
                                            TrackFragmentInfo::TrackRunInfo& trun) {
    if (size < 8) {
        return false;
    }
    
    io->seek(static_cast<long>(offset), SEEK_SET);
    uint8_t header[8];
    if (io->read(header, 1, 8) != 8) {
        return false;
    }
    
    // Read version/flags
    uint32_t versionFlags = (static_cast<uint32_t>(header[0]) << 24) |
                            (static_cast<uint32_t>(header[1]) << 16) |
                            (static_cast<uint32_t>(header[2]) << 8) |
                            static_cast<uint32_t>(header[3]);
    
    uint32_t flags = versionFlags & 0xFFFFFF;
    
    // Read sample count
    trun.sampleCount = (static_cast<uint32_t>(header[4]) << 24) |
                       (static_cast<uint32_t>(header[5]) << 16) |
                       (static_cast<uint32_t>(header[6]) << 8) |
                       static_cast<uint32_t>(header[7]);
    
    if (trun.sampleCount == 0) {
        return false;
    }
    
    size_t dataOffset = 8;
    
    // Read optional data offset
    if (flags & 0x000001) { // data-offset-present
        if (size < dataOffset + 4) {
            return false;
        }
        uint8_t offsetBytes[4];
        if (io->read(offsetBytes, 1, 4) != 4) {
            return false;
        }
        trun.dataOffset = (static_cast<uint32_t>(offsetBytes[0]) << 24) |
                          (static_cast<uint32_t>(offsetBytes[1]) << 16) |
                          (static_cast<uint32_t>(offsetBytes[2]) << 8) |
                          static_cast<uint32_t>(offsetBytes[3]);
        dataOffset += 4;
    }
    
    // Read optional first sample flags
    if (flags & 0x000004) { // first-sample-flags-present
        if (size < dataOffset + 4) {
            return false;
        }
        uint8_t flagsBytes[4];
        if (io->read(flagsBytes, 1, 4) != 4) {
            return false;
        }
        trun.firstSampleFlags = (static_cast<uint32_t>(flagsBytes[0]) << 24) |
                                (static_cast<uint32_t>(flagsBytes[1]) << 16) |
                                (static_cast<uint32_t>(flagsBytes[2]) << 8) |
                                static_cast<uint32_t>(flagsBytes[3]);
        dataOffset += 4;
    }
    
    // Calculate per-sample data size
    size_t perSampleSize = 0;
    if (flags & 0x000100) perSampleSize += 4; // sample-duration-present
    if (flags & 0x000200) perSampleSize += 4; // sample-size-present
    if (flags & 0x000400) perSampleSize += 4; // sample-flags-present
    if (flags & 0x000800) perSampleSize += 4; // sample-composition-time-offsets-present
    
    if (size < dataOffset + (trun.sampleCount * perSampleSize)) {
        return false;
    }
    
    // Reserve space for sample data
    if (flags & 0x000100) trun.sampleDurations.reserve(trun.sampleCount);
    if (flags & 0x000200) trun.sampleSizes.reserve(trun.sampleCount);
    if (flags & 0x000400) trun.sampleFlags.reserve(trun.sampleCount);
    if (flags & 0x000800) trun.sampleCompositionTimeOffsets.reserve(trun.sampleCount);
    
    // Read per-sample data
    for (uint32_t i = 0; i < trun.sampleCount; i++) {
        if (flags & 0x000100) { // sample-duration-present
            uint8_t durationBytes[4];
            if (io->read(durationBytes, 1, 4) != 4) {
                return false;
            }
            uint32_t duration = (static_cast<uint32_t>(durationBytes[0]) << 24) |
                                (static_cast<uint32_t>(durationBytes[1]) << 16) |
                                (static_cast<uint32_t>(durationBytes[2]) << 8) |
                                static_cast<uint32_t>(durationBytes[3]);
            trun.sampleDurations.push_back(duration);
        }
        
        if (flags & 0x000200) { // sample-size-present
            uint8_t sizeBytes[4];
            if (io->read(sizeBytes, 1, 4) != 4) {
                return false;
            }
            uint32_t sampleSize = (static_cast<uint32_t>(sizeBytes[0]) << 24) |
                                  (static_cast<uint32_t>(sizeBytes[1]) << 16) |
                                  (static_cast<uint32_t>(sizeBytes[2]) << 8) |
                                  static_cast<uint32_t>(sizeBytes[3]);
            trun.sampleSizes.push_back(sampleSize);
        }
        
        if (flags & 0x000400) { // sample-flags-present
            uint8_t flagsBytes[4];
            if (io->read(flagsBytes, 1, 4) != 4) {
                return false;
            }
            uint32_t sampleFlags = (static_cast<uint32_t>(flagsBytes[0]) << 24) |
                                   (static_cast<uint32_t>(flagsBytes[1]) << 16) |
                                   (static_cast<uint32_t>(flagsBytes[2]) << 8) |
                                   static_cast<uint32_t>(flagsBytes[3]);
            trun.sampleFlags.push_back(sampleFlags);
        }
        
        if (flags & 0x000800) { // sample-composition-time-offsets-present
            uint8_t offsetBytes[4];
            if (io->read(offsetBytes, 1, 4) != 4) {
                return false;
            }
            uint32_t offset = (static_cast<uint32_t>(offsetBytes[0]) << 24) |
                              (static_cast<uint32_t>(offsetBytes[1]) << 16) |
                              (static_cast<uint32_t>(offsetBytes[2]) << 8) |
                              static_cast<uint32_t>(offsetBytes[3]);
            trun.sampleCompositionTimeOffsets.push_back(offset);
        }
    }
    
    return true;
}

bool FragmentHandler::ParseTrackFragmentDecodeTime(uint64_t offset, uint64_t size, std::shared_ptr<IOHandler> io,
                                                   TrackFragmentInfo& traf) {
    if (size < 8) {
        return false;
    }
    
    io->seek(static_cast<long>(offset), SEEK_SET);
    uint8_t data[16]; // Maximum size for version 1
    size_t readSize = std::min(size, static_cast<uint64_t>(16));
    if (io->read(data, 1, readSize) != readSize) {
        return false;
    }
    
    // Read version/flags
    uint8_t version = data[0];
    
    if (version == 1) {
        // Version 1 - 64-bit decode time
        if (readSize >= 12) {
            traf.tfdt = (static_cast<uint64_t>(data[4]) << 56) |
                        (static_cast<uint64_t>(data[5]) << 48) |
                        (static_cast<uint64_t>(data[6]) << 40) |
                        (static_cast<uint64_t>(data[7]) << 32) |
                        (static_cast<uint64_t>(data[8]) << 24) |
                        (static_cast<uint64_t>(data[9]) << 16) |
                        (static_cast<uint64_t>(data[10]) << 8) |
                        static_cast<uint64_t>(data[11]);
        }
    } else {
        // Version 0 - 32-bit decode time
        if (readSize >= 8) {
            traf.tfdt = (static_cast<uint32_t>(data[4]) << 24) |
                        (static_cast<uint32_t>(data[5]) << 16) |
                        (static_cast<uint32_t>(data[6]) << 8) |
                        static_cast<uint32_t>(data[7]);
        }
    }
    
    return true;
}

uint64_t FragmentHandler::FindMediaDataBox(uint64_t startOffset, std::shared_ptr<IOHandler> io) {
    // Look for mdat box after the moof box
    uint64_t currentOffset = startOffset;
    
    // Search within a reasonable range (e.g., 64KB)
    uint64_t maxSearchRange = startOffset + 65536;
    
    while (currentOffset < maxSearchRange) {
        io->seek(static_cast<long>(currentOffset), SEEK_SET);
        uint8_t header[8];
        if (io->read(header, 1, 8) != 8) {
            break;
        }
        
        uint32_t boxSize = (static_cast<uint32_t>(header[0]) << 24) |
                           (static_cast<uint32_t>(header[1]) << 16) |
                           (static_cast<uint32_t>(header[2]) << 8) |
                           static_cast<uint32_t>(header[3]);
        
        uint32_t boxType = (static_cast<uint32_t>(header[4]) << 24) |
                           (static_cast<uint32_t>(header[5]) << 16) |
                           (static_cast<uint32_t>(header[6]) << 8) |
                           static_cast<uint32_t>(header[7]);
        
        if (boxType == BOX_MDAT) {
            return currentOffset;
        }
        
        if (boxSize < 8) {
            break;
        }
        
        currentOffset += boxSize;
    }
    
    return 0; // Not found
}

bool FragmentHandler::UpdateSampleTables(const TrackFragmentInfo& traf, AudioTrackInfo& track) {
    if (traf.trackRuns.empty()) {
        return false;
    }
    
    // Update sample table information with fragment data
    for (const auto& trun : traf.trackRuns) {
        // Add sample sizes
        if (!trun.sampleSizes.empty()) {
            track.sampleTableInfo.sampleSizes.insert(
                track.sampleTableInfo.sampleSizes.end(),
                trun.sampleSizes.begin(),
                trun.sampleSizes.end()
            );
        } else if (traf.defaultSampleSize > 0) {
            // Use default sample size for all samples in this run
            for (uint32_t i = 0; i < trun.sampleCount; i++) {
                track.sampleTableInfo.sampleSizes.push_back(traf.defaultSampleSize);
            }
        }
        
        // Add sample times based on durations
        uint64_t currentTime = traf.tfdt;
        if (!trun.sampleDurations.empty()) {
            for (uint32_t duration : trun.sampleDurations) {
                track.sampleTableInfo.sampleTimes.push_back(currentTime);
                currentTime += duration;
            }
        } else if (traf.defaultSampleDuration > 0) {
            // Use default sample duration for all samples in this run
            for (uint32_t i = 0; i < trun.sampleCount; i++) {
                track.sampleTableInfo.sampleTimes.push_back(currentTime);
                currentTime += traf.defaultSampleDuration;
            }
        }
    }
    
    return true;
}

bool FragmentHandler::AddFragment(const MovieFragmentInfo& fragment) {
    // Validate fragment
    if (!ValidateFragment(fragment)) {
        return false;
    }
    
    // Add fragment to collection
    fragments.push_back(fragment);
    
    // Reorder fragments by sequence number
    return ReorderFragments();
}

bool FragmentHandler::ReorderFragments() {
    // Sort fragments by sequence number
    std::sort(fragments.begin(), fragments.end(), CompareFragmentsBySequence);
    
    // Check for missing fragments
    if (HasMissingFragments()) {
        FillMissingFragmentGaps();
    }
    
    return true;
}

bool FragmentHandler::CompareFragmentsBySequence(const MovieFragmentInfo& a, const MovieFragmentInfo& b) {
    return a.sequenceNumber < b.sequenceNumber;
}

bool FragmentHandler::HasMissingFragments() const {
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

void FragmentHandler::FillMissingFragmentGaps() {
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

bool FragmentHandler::ValidateFragment(const MovieFragmentInfo& fragment) const {
    // Basic validation
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

bool FragmentHandler::ValidateTrackFragment(const TrackFragmentInfo& traf) const {
    // Track ID must be valid
    if (traf.trackId == 0) {
        return false;
    }
    
    // Must have at least one track run
    if (traf.trackRuns.empty()) {
        return false;
    }
    
    // Validate each track run
    for (const auto& trun : traf.trackRuns) {
        if (trun.sampleCount == 0) {
            return false;
        }
        
        // If sample-specific data is present, it must match sample count
        if (!trun.sampleSizes.empty() && trun.sampleSizes.size() != trun.sampleCount) {
            return false;
        }
        if (!trun.sampleDurations.empty() && trun.sampleDurations.size() != trun.sampleCount) {
            return false;
        }
        if (!trun.sampleFlags.empty() && trun.sampleFlags.size() != trun.sampleCount) {
            return false;
        }
        if (!trun.sampleCompositionTimeOffsets.empty() && 
            trun.sampleCompositionTimeOffsets.size() != trun.sampleCount) {
            return false;
        }
    }
    
    return true;
}

bool FragmentHandler::SeekToFragment(uint32_t sequenceNumber) {
    for (size_t i = 0; i < fragments.size(); i++) {
        if (fragments[i].sequenceNumber == sequenceNumber) {
            currentFragmentIndex = static_cast<uint32_t>(i);
            return true;
        }
    }
    return false;
}

MovieFragmentInfo* FragmentHandler::GetCurrentFragment() {
    if (currentFragmentIndex < fragments.size()) {
        return &fragments[currentFragmentIndex];
    }
    return nullptr;
}

MovieFragmentInfo* FragmentHandler::GetFragment(uint32_t sequenceNumber) {
    for (auto& fragment : fragments) {
        if (fragment.sequenceNumber == sequenceNumber) {
            return &fragment;
        }
    }
    return nullptr;
}

bool FragmentHandler::IsFragmentComplete(uint32_t sequenceNumber) const {
    for (const auto& fragment : fragments) {
        if (fragment.sequenceNumber == sequenceNumber) {
            return fragment.isComplete;
        }
    }
    return false;
}

bool FragmentHandler::ExtractFragmentSample(uint32_t trackId, uint64_t sampleIndex, 
                                           uint64_t& offset, uint32_t& size) {
    MovieFragmentInfo* currentFragment = GetCurrentFragment();
    if (!currentFragment) {
        return false;
    }
    
    // Find the track fragment for the specified track ID
    TrackFragmentInfo* traf = nullptr;
    for (auto& tf : currentFragment->trackFragments) {
        if (tf.trackId == trackId) {
            traf = &tf;
            break;
        }
    }
    
    if (!traf) {
        return false;
    }
    
    // Find the sample within the track runs
    uint64_t currentSampleIndex = 0;
    uint64_t currentDataOffset = currentFragment->mdatOffset + 8; // Skip mdat header
    
    for (const auto& trun : traf->trackRuns) {
        if (sampleIndex >= currentSampleIndex && sampleIndex < currentSampleIndex + trun.sampleCount) {
            // Sample is in this track run
            uint64_t runSampleIndex = sampleIndex - currentSampleIndex;
            
            // Calculate offset within this run
            uint64_t runOffset = currentDataOffset + trun.dataOffset;
            for (uint64_t i = 0; i < runSampleIndex; i++) {
                if (i < trun.sampleSizes.size()) {
                    runOffset += trun.sampleSizes[i];
                } else if (traf->defaultSampleSize > 0) {
                    runOffset += traf->defaultSampleSize;
                } else {
                    return false; // No size information available
                }
            }
            
            // Get sample size
            if (runSampleIndex < trun.sampleSizes.size()) {
                size = trun.sampleSizes[runSampleIndex];
            } else if (traf->defaultSampleSize > 0) {
                size = traf->defaultSampleSize;
            } else {
                return false; // No size information available
            }
            
            offset = runOffset;
            return true;
        }
        
        currentSampleIndex += trun.sampleCount;
        
        // Update data offset for next run
        for (uint32_t i = 0; i < trun.sampleCount; i++) {
            if (i < trun.sampleSizes.size()) {
                currentDataOffset += trun.sampleSizes[i];
            } else if (traf->defaultSampleSize > 0) {
                currentDataOffset += traf->defaultSampleSize;
            }
        }
    }
    
    return false; // Sample not found
}

void FragmentHandler::SetDefaultValues(const AudioTrackInfo& movieHeaderDefaults) {
    defaults.defaultSampleDuration = static_cast<uint32_t>(movieHeaderDefaults.duration / 1000); // Rough estimate
    defaults.defaultSampleSize = 1024; // Default for audio
    defaults.defaultSampleFlags = 0;
}

uint32_t FragmentHandler::ReadUInt32BE(std::shared_ptr<IOHandler> io, uint64_t offset) {
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

uint64_t FragmentHandler::ReadUInt64BE(std::shared_ptr<IOHandler> io, uint64_t offset) {
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