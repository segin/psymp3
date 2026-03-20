/*
 * fuzz_tag_picture.cpp - libFuzzer harness for picture/artwork parsing
 * 
 * This file implements a libFuzzer target for fuzzing the picture parsing
 * code in both ID3v2 APIC frames and VorbisComment METADATA_BLOCK_PICTURE fields.
 * 
 * Picture parsing is particularly security-sensitive because:
 * - It handles binary image data of arbitrary size
 * - It parses MIME types and descriptions as strings
 * - METADATA_BLOCK_PICTURE uses base64 encoding
 * - Image dimension extraction parses binary headers
 * 
 * Compile with:
 *   clang++ -fsanitize=fuzzer,address -I./include fuzz_tag_picture.cpp \
 *     src/tag/ID3v2Tag.cpp src/tag/ID3v2Utils.cpp src/tag/VorbisCommentTag.cpp \
 *     src/tag/Tag.cpp src/tag/NullTag.cpp src/core/utility/UTF8Util.cpp \
 *     src/debug.cpp -o fuzz_tag_picture
 * 
 * Run with:
 *   ./fuzz_tag_picture corpus/ -max_len=10000000
 * 
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include <cstdint>
#include <cstring>
#include <vector>
#include <memory>
#include <string>

// Include tag headers
#include "tag/ID3v2Tag.h"
#include "tag/VorbisCommentTag.h"
#include "tag/Tag.h"

using namespace PsyMP3::Tag;

/*
 * Helper: Create an ID3v2 tag with APIC frame from fuzzer data
 * 
 * Uses the fuzzer data to construct an APIC frame with:
 * - Variable MIME type
 * - Variable picture type
 * - Variable description
 * - Variable image data
 */
std::vector<uint8_t> createAPICTag(const uint8_t* data, size_t size) {
    std::vector<uint8_t> tag;
    
    // ID3v2 header
    tag.push_back('I');
    tag.push_back('D');
    tag.push_back('3');
    tag.push_back(3);  // v2.3
    tag.push_back(0);  // minor version
    tag.push_back(0);  // flags
    
    // We'll fill in the size later
    size_t size_offset = tag.size();
    tag.push_back(0); tag.push_back(0);
    tag.push_back(0); tag.push_back(0);
    
    // APIC frame
    tag.push_back('A');
    tag.push_back('P');
    tag.push_back('I');
    tag.push_back('C');
    
    // Frame size placeholder
    size_t frame_size_offset = tag.size();
    tag.push_back(0); tag.push_back(0);
    tag.push_back(0); tag.push_back(0);
    
    // Frame flags
    tag.push_back(0);
    tag.push_back(0);
    
    size_t frame_data_start = tag.size();
    
    // Use fuzzer data to populate APIC content
    if (size > 0) {
        // Encoding byte
        tag.push_back(data[0] % 4);
        
        // MIME type (use some bytes as MIME, null-terminated)
        size_t mime_len = (size > 1) ? (data[1] % 32) : 0;
        for (size_t i = 0; i < mime_len && i + 2 < size; ++i) {
            tag.push_back(data[i + 2]);
        }
        tag.push_back(0);  // null terminator
        
        // Picture type
        if (size > mime_len + 2) {
            tag.push_back(data[mime_len + 2]);
        } else {
            tag.push_back(3);  // Front cover
        }
        
        // Description (null-terminated)
        size_t desc_start = mime_len + 3;
        size_t desc_len = (size > desc_start) ? (data[desc_start] % 64) : 0;
        for (size_t i = 0; i < desc_len && desc_start + 1 + i < size; ++i) {
            tag.push_back(data[desc_start + 1 + i]);
        }
        tag.push_back(0);  // null terminator
        
        // Image data (rest of fuzzer input)
        size_t img_start = desc_start + 1 + desc_len;
        for (size_t i = img_start; i < size; ++i) {
            tag.push_back(data[i]);
        }
    }
    
    // Calculate and fill in frame size
    uint32_t frame_size = static_cast<uint32_t>(tag.size() - frame_data_start);
    tag[frame_size_offset] = static_cast<uint8_t>((frame_size >> 24) & 0xFF);
    tag[frame_size_offset + 1] = static_cast<uint8_t>((frame_size >> 16) & 0xFF);
    tag[frame_size_offset + 2] = static_cast<uint8_t>((frame_size >> 8) & 0xFF);
    tag[frame_size_offset + 3] = static_cast<uint8_t>(frame_size & 0xFF);
    
    // Calculate and fill in tag size (synchsafe)
    uint32_t tag_size = static_cast<uint32_t>(tag.size() - 10);
    tag[size_offset] = static_cast<uint8_t>((tag_size >> 21) & 0x7F);
    tag[size_offset + 1] = static_cast<uint8_t>((tag_size >> 14) & 0x7F);
    tag[size_offset + 2] = static_cast<uint8_t>((tag_size >> 7) & 0x7F);
    tag[size_offset + 3] = static_cast<uint8_t>(tag_size & 0x7F);
    
    return tag;
}

/*
 * Helper: Create a VorbisComment with METADATA_BLOCK_PICTURE from fuzzer data
 * 
 * METADATA_BLOCK_PICTURE format (binary, then base64 encoded):
 *   4 bytes: picture type (big-endian)
 *   4 bytes: MIME type length
 *   N bytes: MIME type
 *   4 bytes: description length
 *   N bytes: description (UTF-8)
 *   4 bytes: width
 *   4 bytes: height
 *   4 bytes: color depth
 *   4 bytes: colors used (for indexed images)
 *   4 bytes: image data length
 *   N bytes: image data
 */
std::vector<uint8_t> createVorbisCommentWithPicture(const uint8_t* data, size_t size) {
    std::vector<uint8_t> vorbis;
    
    // Vendor string length (little-endian)
    vorbis.push_back(4); vorbis.push_back(0);
    vorbis.push_back(0); vorbis.push_back(0);
    // Vendor string
    vorbis.push_back('t'); vorbis.push_back('e');
    vorbis.push_back('s'); vorbis.push_back('t');
    
    // Field count (1 field)
    vorbis.push_back(1); vorbis.push_back(0);
    vorbis.push_back(0); vorbis.push_back(0);
    
    // Build METADATA_BLOCK_PICTURE binary data
    std::vector<uint8_t> picture_binary;
    
    if (size >= 4) {
        // Picture type (big-endian)
        picture_binary.push_back(data[0]);
        picture_binary.push_back(data[1]);
        picture_binary.push_back(data[2]);
        picture_binary.push_back(data[3]);
    } else {
        picture_binary.push_back(0); picture_binary.push_back(0);
        picture_binary.push_back(0); picture_binary.push_back(3);
    }
    
    // MIME type
    size_t mime_len = (size > 4) ? (data[4] % 32) : 10;
    picture_binary.push_back(0); picture_binary.push_back(0);
    picture_binary.push_back(0); picture_binary.push_back(static_cast<uint8_t>(mime_len));
    for (size_t i = 0; i < mime_len; ++i) {
        if (5 + i < size) {
            picture_binary.push_back(data[5 + i]);
        } else {
            picture_binary.push_back('x');
        }
    }
    
    // Description
    size_t desc_offset = 5 + mime_len;
    size_t desc_len = (size > desc_offset) ? (data[desc_offset] % 64) : 0;
    picture_binary.push_back(0); picture_binary.push_back(0);
    picture_binary.push_back(0); picture_binary.push_back(static_cast<uint8_t>(desc_len));
    for (size_t i = 0; i < desc_len; ++i) {
        if (desc_offset + 1 + i < size) {
            picture_binary.push_back(data[desc_offset + 1 + i]);
        } else {
            picture_binary.push_back('?');
        }
    }
    
    // Dimensions (use fuzzer data if available)
    size_t dim_offset = desc_offset + 1 + desc_len;
    for (int field = 0; field < 4; ++field) {  // width, height, depth, colors
        for (int byte = 0; byte < 4; ++byte) {
            if (dim_offset + field * 4 + byte < size) {
                picture_binary.push_back(data[dim_offset + field * 4 + byte]);
            } else {
                picture_binary.push_back(0);
            }
        }
    }
    
    // Image data length and data
    size_t img_offset = dim_offset + 16;
    size_t img_len = (size > img_offset) ? (size - img_offset) : 0;
    picture_binary.push_back(static_cast<uint8_t>((img_len >> 24) & 0xFF));
    picture_binary.push_back(static_cast<uint8_t>((img_len >> 16) & 0xFF));
    picture_binary.push_back(static_cast<uint8_t>((img_len >> 8) & 0xFF));
    picture_binary.push_back(static_cast<uint8_t>(img_len & 0xFF));
    for (size_t i = img_offset; i < size; ++i) {
        picture_binary.push_back(data[i]);
    }
    
    // Base64 encode the picture data
    static const char base64_chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string base64;
    size_t i = 0;
    while (i < picture_binary.size()) {
        uint32_t octet_a = i < picture_binary.size() ? picture_binary[i++] : 0;
        uint32_t octet_b = i < picture_binary.size() ? picture_binary[i++] : 0;
        uint32_t octet_c = i < picture_binary.size() ? picture_binary[i++] : 0;
        
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;
        
        base64 += base64_chars[(triple >> 18) & 0x3F];
        base64 += base64_chars[(triple >> 12) & 0x3F];
        base64 += (i > picture_binary.size() + 1) ? '=' : base64_chars[(triple >> 6) & 0x3F];
        base64 += (i > picture_binary.size()) ? '=' : base64_chars[triple & 0x3F];
    }
    
    // Build field: METADATA_BLOCK_PICTURE=<base64>
    std::string field = "METADATA_BLOCK_PICTURE=" + base64;
    
    // Field length (little-endian)
    uint32_t field_len = static_cast<uint32_t>(field.size());
    vorbis.push_back(static_cast<uint8_t>(field_len & 0xFF));
    vorbis.push_back(static_cast<uint8_t>((field_len >> 8) & 0xFF));
    vorbis.push_back(static_cast<uint8_t>((field_len >> 16) & 0xFF));
    vorbis.push_back(static_cast<uint8_t>((field_len >> 24) & 0xFF));
    
    // Field data
    for (char c : field) {
        vorbis.push_back(static_cast<uint8_t>(c));
    }
    
    return vorbis;
}

/*
 * libFuzzer entry point
 * 
 * This function is called by libFuzzer with random input data.
 * It tests picture parsing in multiple ways:
 * 1. Direct APIC frame parsing via ID3v2Tag
 * 2. METADATA_BLOCK_PICTURE parsing via VorbisCommentTag
 * 3. Raw picture data handling
 */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) return 0;
    
    // ========================================================================
    // Test 1: Parse as ID3v2 tag with APIC frame
    // ========================================================================
    {
        auto apic_tag_data = createAPICTag(data, size);
        auto tag = ID3v2Tag::parse(apic_tag_data.data(), apic_tag_data.size());
        
        if (tag) {
            // Exercise picture accessors
            size_t pic_count = tag->pictureCount();
            (void)pic_count;
            
            for (size_t i = 0; i < pic_count && i < 10; ++i) {
                auto pic = tag->getPicture(i);
                if (pic) {
                    (void)pic->type;
                    (void)pic->mime_type;
                    (void)pic->description;
                    (void)pic->width;
                    (void)pic->height;
                    (void)pic->color_depth;
                    (void)pic->colors_used;
                    (void)pic->data.size();
                    (void)pic->isEmpty();
                }
            }
            
            (void)tag->getFrontCover();
            (void)tag->getPicture(0);
            (void)tag->getPicture(SIZE_MAX);
        }
    }
    
    // ========================================================================
    // Test 2: Parse as VorbisComment with METADATA_BLOCK_PICTURE
    // ========================================================================
    {
        auto vorbis_data = createVorbisCommentWithPicture(data, size);
        auto tag = VorbisCommentTag::parse(vorbis_data.data(), vorbis_data.size());
        
        if (tag) {
            // Exercise picture accessors
            size_t pic_count = tag->pictureCount();
            (void)pic_count;
            
            for (size_t i = 0; i < pic_count && i < 10; ++i) {
                auto pic = tag->getPicture(i);
                if (pic) {
                    (void)pic->type;
                    (void)pic->mime_type;
                    (void)pic->description;
                    (void)pic->width;
                    (void)pic->height;
                    (void)pic->color_depth;
                    (void)pic->colors_used;
                    (void)pic->data.size();
                    (void)pic->isEmpty();
                }
            }
            
            (void)tag->getFrontCover();
            (void)tag->getPicture(0);
            (void)tag->getPicture(SIZE_MAX);
            
            // Also check the raw METADATA_BLOCK_PICTURE field
            (void)tag->getTag("METADATA_BLOCK_PICTURE");
            (void)tag->getTagValues("METADATA_BLOCK_PICTURE");
        }
    }
    
    // ========================================================================
    // Test 3: Direct ID3v2 tag parsing with raw fuzzer data
    // ========================================================================
    {
        // Try parsing raw fuzzer data as ID3v2 (might have APIC frames)
        auto tag = ID3v2Tag::parse(data, size);
        if (tag) {
            size_t pic_count = tag->pictureCount();
            for (size_t i = 0; i < pic_count && i < 10; ++i) {
                auto pic = tag->getPicture(i);
                if (pic) {
                    // Verify picture data is bounded
                    if (pic->data.size() > size) {
                        // Picture data larger than input - bug!
                        __builtin_trap();
                    }
                }
            }
        }
    }
    
    // ========================================================================
    // Test 4: Direct VorbisComment parsing with raw fuzzer data
    // ========================================================================
    {
        // Try parsing raw fuzzer data as VorbisComment
        auto tag = VorbisCommentTag::parse(data, size);
        if (tag) {
            size_t pic_count = tag->pictureCount();
            for (size_t i = 0; i < pic_count && i < 10; ++i) {
                auto pic = tag->getPicture(i);
                if (pic) {
                    // Verify picture data is bounded
                    if (pic->data.size() > size * 2) {  // Allow for base64 expansion
                        // Picture data unreasonably large - bug!
                        __builtin_trap();
                    }
                }
            }
        }
    }
    
    return 0;
}

/*
 * AFL++ entry point
 */
#ifdef __AFL_COMPILER
#include <cstdio>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    std::vector<uint8_t> input;
    int byte;
    while ((byte = getchar()) != EOF) {
        input.push_back(static_cast<uint8_t>(byte));
    }

    if (!input.empty()) {
        LLVMFuzzerTestOneInput(input.data(), input.size());
    }

    return 0;
}
#endif

/*
 * Standalone test mode
 */
#ifndef __AFL_COMPILER
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file>\n";
        std::cerr << "  Reads input file and runs fuzzer target once.\n";
        return 1;
    }
    
    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot open file: " << argv[1] << "\n";
        return 1;
    }
    
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    
    std::cout << "Running fuzzer target with " << data.size() << " bytes...\n";
    int result = LLVMFuzzerTestOneInput(data.data(), data.size());
    std::cout << "Fuzzer target returned: " << result << "\n";
    
    return result;
}
#endif
#endif

