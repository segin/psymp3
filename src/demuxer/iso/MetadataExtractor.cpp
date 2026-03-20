/*
 * ISODemuxerMetadataExtractor.cpp - Metadata extraction for ISO demuxer
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

std::map<std::string, std::string> MetadataExtractor::ExtractMetadata(std::shared_ptr<IOHandler> io, uint64_t udtaOffset, uint64_t size) {
    std::map<std::string, std::string> metadata;
    if (!io || size < 8) {
        return metadata;
    }
    
    // Parse udta box recursively to find metadata
    ParseUdtaBox(io, udtaOffset, size, metadata, 0);
    return metadata;
}

bool MetadataExtractor::ParseUdtaBox(std::shared_ptr<IOHandler> io, uint64_t offset, uint64_t size, std::map<std::string, std::string>& metadata, int depth) {
    if (depth > MAX_RECURSION_DEPTH) {
        return false;
    }

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
                ParseMetaBox(io, boxDataOffset, boxDataSize, metadata, depth + 1);
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

bool MetadataExtractor::ParseMetaBox(std::shared_ptr<IOHandler> io, uint64_t offset, uint64_t size, std::map<std::string, std::string>& metadata, int depth) {
    if (size < 4) {
        return false;
    }
    
    // Skip version/flags (4 bytes)
    uint64_t dataOffset = offset + 4;
    uint64_t dataSize = size - 4;
    
    // Parse meta box contents recursively
    return ParseUdtaBox(io, dataOffset, dataSize, metadata, depth);
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
} // namespace ISO
} // namespace Demuxer
} // namespace PsyMP3
