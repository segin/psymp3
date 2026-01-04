/*
 * ImageUtils.h - Image utility functions
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

#ifndef PSYMP3_TAG_IMAGEUTILS_H
#define PSYMP3_TAG_IMAGEUTILS_H

#include <vector>
#include <cstdint>
#include <string>
#include "tag/Tag.h" // For Picture struct

namespace PsyMP3 {
namespace Tag {
namespace ImageUtils {

/**
 * @brief Extract dimensions from raw image data
 * 
 * Inspects the image header (JPEG, PNG, GIF, BMP) to determine
 * width, height, and color depth. Updates the Picture structure.
 * 
 * @param picture Picture structure to update (reads data/mime_type, writes width/height/depth)
 */
void extractDimensions(Picture& picture);

} // namespace ImageUtils
} // namespace Tag
} // namespace PsyMP3

#endif // PSYMP3_TAG_IMAGEUTILS_H
