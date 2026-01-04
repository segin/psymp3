/*
 * TagConstants.h - Constants for Tag module
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill II <segin2005@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
