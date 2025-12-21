/*
 * TagFactory.cpp - Tag format detection and factory implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
// Standalone build - include only what we need
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string>
#include <vector>
#include <fstream>
#include <cstring>
#include <mutex>

#include "debug.h"
#include "tag/TagFactory.h"
#include "tag/Tag.h"
#include "tag/NullTag.h"
#include "tag/ID3v1Tag.h"
#include "tag/ID3v2Tag.h"
#include "tag/MergedID3Tag.h"
#include "tag/VorbisCommentTag.h"
#endif // !FINAL_BUILD

namespace PsyMP3 {
namespace Tag {

// ============================================================================
// Format Detection
// ============================================================================

TagFormat TagFactory::detectFormat(const uint8_t* data, size_t size) {
    if (!data || size < 3) {
        return TagFormat::Unknown;
    }
    
    // Check for ID3v2 (starts with "ID3")
    if (size >= 10 && data[0] == 'I' && data[1] == 'D' && data[2] == '3') {
        Debug::log("tag", "TagFactory::detectFormat: Detected ID3v2");
        return TagFormat::ID3v2;
    }
    
    // Check for ID3v1 (starts with "TAG")
    if (size >= 128 && data[0] == 'T' && data[1] == 'A' && data[2] == 'G') {
        Debug::log("tag", "TagFactory::detectFormat: Detected ID3v1");
        return TagFormat::ID3v1;
    }
    
    // Check for VorbisComment (little-endian vendor string length followed by string)
    // This is a heuristic - VorbisComment doesn't have a magic byte
    if (size >= 8) {
        uint32_t vendor_len = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
        // Reasonable vendor string length (< 1KB)
        if (vendor_len > 0 && vendor_len < 1024 && size >= 4 + vendor_len) {
            Debug::log("tag", "TagFactory::detectFormat: Possibly VorbisComment");
            return TagFormat::VorbisComment;
        }
    }
    
    Debug::log("tag", "TagFactory::detectFormat: Unknown format");
    return TagFormat::Unknown;
}

// ============================================================================
// ID3 Detection Helpers
// ============================================================================

bool TagFactory::hasID3v1(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        Debug::log("tag", "TagFactory::hasID3v1: Failed to open file: ", filepath);
        return false;
    }
    
    // Seek to 128 bytes from end
    file.seekg(-128, std::ios::end);
    if (!file) {
        Debug::log("tag", "TagFactory::hasID3v1: File too small for ID3v1");
        return false;
    }
    
    // Read first 3 bytes
    uint8_t header[3];
    file.read(reinterpret_cast<char*>(header), 3);
    if (!file) {
        return false;
    }
    
    // Check for "TAG" magic
    bool has_id3v1 = (header[0] == 'T' && header[1] == 'A' && header[2] == 'G');
    Debug::log("tag", "TagFactory::hasID3v1: ", has_id3v1 ? "Found" : "Not found");
    return has_id3v1;
}

size_t TagFactory::getID3v2Size(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        Debug::log("tag", "TagFactory::getID3v2Size: Failed to open file: ", filepath);
        return 0;
    }
    
    // Read ID3v2 header (10 bytes)
    uint8_t header[10];
    file.read(reinterpret_cast<char*>(header), 10);
    if (!file) {
        Debug::log("tag", "TagFactory::getID3v2Size: Failed to read header");
        return 0;
    }
    
    // Check for "ID3" magic
    if (header[0] != 'I' || header[1] != 'D' || header[2] != '3') {
        Debug::log("tag", "TagFactory::getID3v2Size: No ID3v2 tag found");
        return 0;
    }
    
    // Get tag size from header (synchsafe integer)
    size_t tag_size = ID3v2Tag::getTagSize(header);
    Debug::log("tag", "TagFactory::getID3v2Size: Found ID3v2 tag, size: ", tag_size);
    return tag_size;
}

// ============================================================================
// MP3 Tag Parsing (ID3v1 + ID3v2 with merging)
// ============================================================================

std::unique_ptr<Tag> TagFactory::parseMP3Tags(const std::string& filepath) {
    Debug::log("tag", "TagFactory::parseMP3Tags: Parsing MP3 file: ", filepath);
    
    std::unique_ptr<ID3v1Tag> id3v1_tag;
    std::unique_ptr<ID3v2Tag> id3v2_tag;
    
    // Check for ID3v2 at start of file
    size_t id3v2_size = getID3v2Size(filepath);
    if (id3v2_size > 0) {
        std::ifstream file(filepath, std::ios::binary);
        if (file) {
            std::vector<uint8_t> id3v2_data(id3v2_size);
            file.read(reinterpret_cast<char*>(id3v2_data.data()), id3v2_size);
            if (file) {
                id3v2_tag = ID3v2Tag::parse(id3v2_data.data(), id3v2_size);
                if (id3v2_tag) {
                    Debug::log("tag", "TagFactory::parseMP3Tags: Successfully parsed ID3v2");
                }
            }
        }
    }
    
    // Check for ID3v1 at end of file
    if (hasID3v1(filepath)) {
        std::ifstream file(filepath, std::ios::binary);
        if (file) {
            file.seekg(-128, std::ios::end);
            uint8_t id3v1_data[128];
            file.read(reinterpret_cast<char*>(id3v1_data), 128);
            if (file) {
                id3v1_tag = ID3v1Tag::parse(id3v1_data);
                if (id3v1_tag) {
                    Debug::log("tag", "TagFactory::parseMP3Tags: Successfully parsed ID3v1");
                }
            }
        }
    }
    
    // Create appropriate tag based on what we found
    if (id3v1_tag && id3v2_tag) {
        Debug::log("tag", "TagFactory::parseMP3Tags: Creating MergedID3Tag");
        return std::make_unique<MergedID3Tag>(std::move(id3v1_tag), std::move(id3v2_tag));
    } else if (id3v2_tag) {
        Debug::log("tag", "TagFactory::parseMP3Tags: Returning ID3v2 tag only");
        return id3v2_tag;
    } else if (id3v1_tag) {
        Debug::log("tag", "TagFactory::parseMP3Tags: Returning ID3v1 tag only");
        return id3v1_tag;
    } else {
        Debug::log("tag", "TagFactory::parseMP3Tags: No tags found, returning NullTag");
        return std::make_unique<NullTag>();
    }
}

// ============================================================================
// Public Factory Methods
// ============================================================================

std::unique_ptr<Tag> TagFactory::createFromFile(const std::string& filepath) {
    if (filepath.empty()) {
        Debug::log("tag", "TagFactory::createFromFile: Empty filepath, returning NullTag");
        return std::make_unique<NullTag>();
    }
    
    Debug::log("tag", "TagFactory::createFromFile: Processing file: ", filepath);
    
    // Determine file type from extension (simple heuristic)
    std::string ext;
    size_t dot_pos = filepath.find_last_of('.');
    if (dot_pos != std::string::npos) {
        ext = filepath.substr(dot_pos + 1);
        // Convert to lowercase
        for (char& c : ext) {
            if (c >= 'A' && c <= 'Z') {
                c = c - 'A' + 'a';
            }
        }
    }
    
    Debug::log("tag", "TagFactory::createFromFile: File extension: ", ext);
    
    // MP3 files may have both ID3v1 and ID3v2
    if (ext == "mp3") {
        return parseMP3Tags(filepath);
    }
    
    // For other formats, read beginning of file and detect
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        Debug::log("tag", "TagFactory::createFromFile: Failed to open file");
        return std::make_unique<NullTag>();
    }
    
    // Read first 1KB for format detection
    std::vector<uint8_t> header(1024);
    file.read(reinterpret_cast<char*>(header.data()), header.size());
    size_t bytes_read = file.gcount();
    
    if (bytes_read == 0) {
        Debug::log("tag", "TagFactory::createFromFile: Empty file");
        return std::make_unique<NullTag>();
    }
    
    TagFormat format = detectFormat(header.data(), bytes_read);
    
    switch (format) {
        case TagFormat::ID3v2: {
            // Read full ID3v2 tag
            size_t tag_size = ID3v2Tag::getTagSize(header.data());
            if (tag_size > 0) {
                file.seekg(0);
                std::vector<uint8_t> tag_data(tag_size);
                file.read(reinterpret_cast<char*>(tag_data.data()), tag_size);
                if (file) {
                    auto tag = ID3v2Tag::parse(tag_data.data(), tag_size);
                    if (tag) {
                        return tag;
                    }
                }
            }
            break;
        }
        
        case TagFormat::VorbisComment: {
            // VorbisComment tags are typically embedded in container formats
            // This would need container-specific parsing
            Debug::log("tag", "TagFactory::createFromFile: VorbisComment detected but needs container parsing");
            break;
        }
        
        default:
            Debug::log("tag", "TagFactory::createFromFile: Unknown or unsupported format");
            break;
    }
    
    return std::make_unique<NullTag>();
}

std::unique_ptr<Tag> TagFactory::createFromData(
    const uint8_t* data, size_t size,
    const std::string& format_hint) {
    
    if (!data || size == 0) {
        Debug::log("tag", "TagFactory::createFromData: No data provided, returning NullTag");
        return std::make_unique<NullTag>();
    }
    
    Debug::log("tag", "TagFactory::createFromData: Processing ", size, " bytes, hint: ",
               format_hint.empty() ? "(none)" : format_hint);
    
    // Use format hint if provided
    TagFormat format = TagFormat::Unknown;
    if (!format_hint.empty()) {
        if (format_hint == "id3v2" || format_hint == "ID3v2") {
            format = TagFormat::ID3v2;
        } else if (format_hint == "id3v1" || format_hint == "ID3v1") {
            format = TagFormat::ID3v1;
        } else if (format_hint == "vorbis" || format_hint == "vorbiscomment") {
            format = TagFormat::VorbisComment;
        }
    }
    
    // If no hint or unknown hint, detect format
    if (format == TagFormat::Unknown) {
        format = detectFormat(data, size);
    }
    
    // Create appropriate tag reader
    switch (format) {
        case TagFormat::ID3v2: {
            auto tag = ID3v2Tag::parse(data, size);
            if (tag) {
                Debug::log("tag", "TagFactory::createFromData: Created ID3v2Tag");
                return tag;
            }
            break;
        }
        
        case TagFormat::ID3v1: {
            if (size >= 128) {
                auto tag = ID3v1Tag::parse(data);
                if (tag) {
                    Debug::log("tag", "TagFactory::createFromData: Created ID3v1Tag");
                    return tag;
                }
            }
            break;
        }
        
        case TagFormat::VorbisComment: {
            auto tag = VorbisCommentTag::parse(data, size);
            if (tag) {
                Debug::log("tag", "TagFactory::createFromData: Created VorbisCommentTag");
                return tag;
            }
            break;
        }
        
        default:
            Debug::log("tag", "TagFactory::createFromData: Unknown or unsupported format");
            break;
    }
    
    Debug::log("tag", "TagFactory::createFromData: Failed to parse, returning NullTag");
    return std::make_unique<NullTag>();
}

} // namespace Tag
} // namespace PsyMP3
