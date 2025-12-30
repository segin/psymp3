/*
 * TagConstants.h - Constants for Tag module
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef PSYMP3_TAG_TAGCONSTANTS_H
#define PSYMP3_TAG_TAGCONSTANTS_H

#include <cstdint>
#include <cstddef>

namespace PsyMP3 {
namespace Tag {

namespace TagConstants {

// Global limits to prevent excessive memory usage / DoS
constexpr size_t MAX_FRAME_SIZE = 100 * 1024 * 1024;       // 100 MB
constexpr size_t MAX_STRING_FIELD_SIZE = 1024 * 1024;      // 1 MB
constexpr size_t MAX_VENDOR_STRING_SIZE = 1024 * 1024;     // 1 MB
constexpr size_t MAX_FIELD_COUNT = 100000;                 // 100k fields
constexpr size_t MAX_PICTURE_SIZE = 100 * 1024 * 1024;     // 100 MB
constexpr size_t MAX_MIME_TYPE_LENGTH = 256;               // 256 bytes

} // namespace TagConstants

} // namespace Tag
} // namespace PsyMP3

#endif // PSYMP3_TAG_TAGCONSTANTS_H
