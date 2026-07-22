/*
 * ISODemuxerMetadataExtractor.cpp - Metadata extraction for ISO demuxer
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include <cstring>
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

        uint64_t boxDataOffset;
        uint64_t boxDataSize;
        uint64_t actualBoxSize;

        // Handle extended (64-bit) size FIRST: boxSize == 1 signals a largesize
        // box. The previous "boxSize < 8 -> break" ran before this branch, so
        // (1 < 8) aborted parsing at any largesize child and silently dropped
        // every metadata box after it.
        if (boxSize == 1) {
            if (currentOffset + 16 > endOffset) {
                break;
            }
            uint64_t extendedSize = ReadUInt64BE(io, currentOffset + 8);
            // A 64-bit size below the 16-byte header is malformed; without this
            // guard extendedSize - 16 underflows to ~2^64.
            if (extendedSize < 16) {
                break;
            }
            boxDataOffset = currentOffset + 16;
            boxDataSize = extendedSize - 16;
            actualBoxSize = extendedSize;
        } else if (boxSize == 0 || boxSize < 8) {
            break;
        } else {
            boxDataOffset = currentOffset + 8;
            boxDataSize = boxSize - 8;
            actualBoxSize = boxSize;
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
        
        currentOffset += actualBoxSize;
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
                    // Track/disk number - iTunes binary format: [0-1] padding,
                    // [2-3] number, [4-5] total. Read the 16-bit number from
                    // bytes [2-3], which lie within the guaranteed 4-byte
                    // minimum. The old ReadUInt32BE(dataOffset+2) spanned the
                    // number AND the total, yielding (number<<16)|total (e.g.
                    // "3/12" -> 196620), and read 2 bytes past the guarantee.
                    if (dataSize >= 4) {
                        uint32_t trackNum = ReadUInt32BE(io, dataOffset) & 0xFFFF;
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
    
    io->seek(static_cast<off_t>(offset), SEEK_SET);
    std::vector<uint8_t> buffer(size);
    size_t bytesRead = io->read(buffer.data(), 1, size);
    
    if (bytesRead == 0) {
        return "";
    }
    
    // Convert to string, handling UTF-8 and null termination
    const uint8_t* start_ptr = buffer.data();
    const uint8_t* end_ptr = static_cast<const uint8_t*>(std::memchr(start_ptr, 0, bytesRead));
    size_t len = end_ptr ? static_cast<size_t>(end_ptr - start_ptr) : bytesRead;
    
    // Find first non-whitespace
    size_t start = 0;
    while (start < len && (start_ptr[start] == ' ' || start_ptr[start] == '\t' || start_ptr[start] == '\r' || start_ptr[start] == '\n')) {
        start++;
    }
    
    if (start == len) {
        return "";
    }

    // Find last non-whitespace
    size_t end = len - 1;
    while (end > start && (start_ptr[end] == ' ' || start_ptr[end] == '\t' || start_ptr[end] == '\r' || start_ptr[end] == '\n')) {
        end--;
    }

    return std::string(reinterpret_cast<const char*>(start_ptr + start), end - start + 1);
}

uint32_t MetadataExtractor::ReadUInt32BE(std::shared_ptr<IOHandler> io, uint64_t offset) {
    if (!io) {
        return 0;
    }
    
    io->seek(static_cast<off_t>(offset), SEEK_SET);
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
    
    io->seek(static_cast<off_t>(offset), SEEK_SET);
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
