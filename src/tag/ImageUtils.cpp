/*
 * ImageUtils.cpp - Image utility functions implementation
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#include "tag/ImageUtils.h"
#endif // !FINAL_BUILD
#include <algorithm>
#include <string>

namespace PsyMP3 {
namespace Tag {
namespace ImageUtils {

void extractDimensions(Picture& picture) {
    if (picture.data.size() < 16) {
        return; // Too small for any image header
    }
    
    const uint8_t* data = picture.data.data();
    
    // Normalize MIME type for checking
    std::string mime = picture.mime_type;
    std::transform(mime.begin(), mime.end(), mime.begin(), ::tolower);
    
    // Try to extract dimensions based on MIME type
    if (mime == "image/jpeg" || mime == "image/jpg") {
        // JPEG: Look for SOF0 (0xFFC0) marker
        // Simple scan for markers
        for (size_t i = 0; i + 8 < picture.data.size(); i++) {
            if (data[i] == 0xFF && data[i + 1] == 0xC0) {
                // SOF0 marker found
                if (i + 8 < picture.data.size()) {
                    picture.height = (static_cast<uint32_t>(data[i + 5]) << 8) | data[i + 6];
                    picture.width = (static_cast<uint32_t>(data[i + 7]) << 8) | data[i + 8];
                    picture.color_depth = data[i + 4] * 8; // Components * 8 bits
                    return;
                }
            }
        }
    } else if (mime == "image/png") {
        // PNG: Check IHDR chunk (should be at offset 8)
        if (picture.data.size() >= 24 && 
            data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47) {
            // PNG signature verified, read IHDR
            picture.width = (static_cast<uint32_t>(data[16]) << 24) |
                           (static_cast<uint32_t>(data[17]) << 16) |
                           (static_cast<uint32_t>(data[18]) << 8) |
                           static_cast<uint32_t>(data[19]);
            picture.height = (static_cast<uint32_t>(data[20]) << 24) |
                            (static_cast<uint32_t>(data[21]) << 16) |
                            (static_cast<uint32_t>(data[22]) << 8) |
                            static_cast<uint32_t>(data[23]);
            picture.color_depth = data[24]; // Bit depth
            return;
        }
    } else if (mime == "image/gif") {
        // GIF: Check header
        if (picture.data.size() >= 10 &&
            ((data[0] == 'G' && data[1] == 'I' && data[2] == 'F' && data[3] == '8' && data[4] == '7' && data[5] == 'a') ||
             (data[0] == 'G' && data[1] == 'I' && data[2] == 'F' && data[3] == '8' && data[4] == '9' && data[5] == 'a'))) {
            // GIF signature verified
            picture.width = static_cast<uint32_t>(data[6]) | (static_cast<uint32_t>(data[7]) << 8);
            picture.height = static_cast<uint32_t>(data[8]) | (static_cast<uint32_t>(data[9]) << 8);
            // GIF color depth is in global color table flag
            if (picture.data.size() >= 11) {
                uint8_t packed = data[10];
                if (packed & 0x80) { // Global color table flag
                    picture.color_depth = ((packed & 0x07) + 1); // Bits per pixel
                }
            }
            return;
        }
    } else if (mime == "image/bmp") {
        // BMP: Check header
        if (picture.data.size() >= 26 && data[0] == 'B' && data[1] == 'M') {
            // BMP signature verified, read DIB header
            picture.width = static_cast<uint32_t>(data[18]) |
                           (static_cast<uint32_t>(data[19]) << 8) |
                           (static_cast<uint32_t>(data[20]) << 16) |
                           (static_cast<uint32_t>(data[21]) << 24);
            picture.height = static_cast<uint32_t>(data[22]) |
                            (static_cast<uint32_t>(data[23]) << 8) |
                            (static_cast<uint32_t>(data[24]) << 16) |
                            (static_cast<uint32_t>(data[25]) << 24);
            if (picture.data.size() >= 30) {
                 // Bit count is at offset 28 (2 bytes)
                picture.color_depth = static_cast<uint32_t>(data[28]) | (static_cast<uint32_t>(data[29]) << 8);
            }
            return;
        }
    }
}

} // namespace ImageUtils
} // namespace Tag
} // namespace PsyMP3
