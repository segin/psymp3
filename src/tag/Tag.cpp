/*
 * Tag.cpp - Format-neutral metadata tag factory implementation
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
#include <mutex>

#include "debug.h"
#include "tag/Tag.h"
#include "tag/NullTag.h"
#endif // !FINAL_BUILD

namespace PsyMP3 {
namespace Tag {

std::unique_ptr<Tag> createTagReader(const std::string& filepath) {
    if (filepath.empty()) {
        Debug::log("tag", "createTagReader: Empty filepath, returning NullTag");
        return std::make_unique<NullTag>();
    }
    
    Debug::log("tag", "createTagReader: Opening file: ", filepath);
    
    // TODO: Implement format detection and appropriate tag reader creation
    // For now, return NullTag as a placeholder
    // Future implementations will detect ID3, Vorbis Comments, APE, etc.
    
    Debug::log("tag", "createTagReader: No tag reader implemented yet, returning NullTag");
    return std::make_unique<NullTag>();
}

std::unique_ptr<Tag> createTagReaderFromData(const uint8_t* data, size_t size, 
                                              const std::string& format_hint) {
    if (!data || size == 0) {
        Debug::log("tag", "createTagReaderFromData: No data provided, returning NullTag");
        return std::make_unique<NullTag>();
    }
    
    Debug::log("tag", "createTagReaderFromData: Processing ", size, " bytes, hint: ", 
               format_hint.empty() ? "(none)" : format_hint);
    
    // TODO: Implement format detection from raw data
    // For now, return NullTag as a placeholder
    
    Debug::log("tag", "createTagReaderFromData: No tag reader implemented yet, returning NullTag");
    return std::make_unique<NullTag>();
}

} // namespace Tag
} // namespace PsyMP3

