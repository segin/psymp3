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
#include "tag/TagFactory.h"
#endif // !FINAL_BUILD

namespace PsyMP3 {
namespace Tag {

std::unique_ptr<Tag> createTagReader(const std::string& filepath) {
    return TagFactory::createFromFile(filepath);
}

std::unique_ptr<Tag> createTagReaderFromData(const uint8_t* data, size_t size, 
                                              const std::string& format_hint) {
    return TagFactory::createFromData(data, size, format_hint);
}

} // namespace Tag
} // namespace PsyMP3

