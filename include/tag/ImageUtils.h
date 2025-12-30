/*
 * ImageUtils.h - Image utility functions
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
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
