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

/**
 * @brief The image format an image's own header declares
 *
 * Tags carry a MIME type too, but taggers get it wrong often enough -- a JPEG
 * labelled image/png is common -- that the bytes are the better witness.
 *
 * @return image/jpeg, image/png, image/gif, image/webp or image/bmp; empty
 *         when the data is none of these
 */
std::string sniffMimeType(const std::vector<uint8_t>& data);

/// The largest image dataUri() will encode.
constexpr size_t kMaxDataUriImageBytes = 1024 * 1024;

/**
 * @brief A picture as an RFC 2397 data: URI, base64-encoded
 *
 * The MIME type comes from sniffMimeType(), not from the tag. Empty when the
 * data is not a recognised image, or is larger than kMaxDataUriImageBytes:
 * a URI like this is sent whole to everything that asks for it -- MPRIS
 * clients receive it with every metadata change -- so an oversized scan is
 * better left out than sent.
 */
std::string dataUri(const Picture& picture);

} // namespace ImageUtils
} // namespace Tag
} // namespace PsyMP3

#endif // PSYMP3_TAG_IMAGEUTILS_H
