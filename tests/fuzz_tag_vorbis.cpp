/*
 * fuzz_tag_vorbis.cpp - libFuzzer harness for VorbisComment tag parser
 * 
 * This file implements a libFuzzer target for fuzzing the VorbisComment tag parser.
 * It can be compiled with libFuzzer to perform continuous fuzzing and detect
 * crashes, hangs, and undefined behavior.
 * 
 * Compile with:
 *   clang++ -fsanitize=fuzzer,address -I./include fuzz_tag_vorbis.cpp \
 *     src/tag/VorbisCommentTag.cpp src/tag/Tag.cpp src/tag/NullTag.cpp \
 *     src/core/utility/UTF8Util.cpp src/debug.cpp \
 *     -o fuzz_tag_vorbis
 * 
 * Run with:
 *   ./fuzz_tag_vorbis corpus/ -max_len=1000000
 * 
 * Seed corpus should include:
 *   - Valid VorbisComment blocks with vendor string and fields
 *   - Comments with multi-valued fields (same key multiple times)
 *   - Comments with METADATA_BLOCK_PICTURE fields
 *   - Comments with various UTF-8 sequences
 *   - Edge cases: empty vendor, empty fields, max length strings
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

// Include VorbisCommentTag header
#include "tag/VorbisCommentTag.h"

using namespace PsyMP3::Tag;

/*
 * libFuzzer entry point
 * 
 * This function is called by libFuzzer with random input data.
 * It attempts to parse the input as a VorbisComment block and exercises all accessors.
 * 
 * The fuzzer tests:
 * - VorbisCommentTag::parse() with arbitrary data
 * - All accessor methods on parsed tags
 * - Case-insensitive field lookup
 * - Multi-valued field handling
 * - METADATA_BLOCK_PICTURE parsing
 * - UTF-8 handling with arbitrary byte sequences
 */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Test parse() with any size data
    if (size > 0) {
        auto tag = VorbisCommentTag::parse(data, size);
        
        // If parsing succeeded, exercise all accessors
        if (tag) {
            // Core metadata accessors - should never crash
            (void)tag->title();
            (void)tag->artist();
            (void)tag->album();
            (void)tag->albumArtist();
            (void)tag->genre();
            (void)tag->year();
            (void)tag->track();
            (void)tag->trackTotal();
            (void)tag->disc();
            (void)tag->discTotal();
            (void)tag->comment();
            (void)tag->composer();
            
            // VorbisComment-specific accessor
            (void)tag->vendorString();
            
            // Extended accessors with standard VorbisComment field names
            (void)tag->getTag("TITLE");
            (void)tag->getTag("ARTIST");
            (void)tag->getTag("ALBUM");
            (void)tag->getTag("ALBUMARTIST");
            (void)tag->getTag("GENRE");
            (void)tag->getTag("DATE");
            (void)tag->getTag("TRACKNUMBER");
            (void)tag->getTag("TRACKTOTAL");
            (void)tag->getTag("DISCNUMBER");
            (void)tag->getTag("DISCTOTAL");
            (void)tag->getTag("COMMENT");
            (void)tag->getTag("DESCRIPTION");
            (void)tag->getTag("COMPOSER");
            (void)tag->getTag("METADATA_BLOCK_PICTURE");
            (void)tag->getTag("NONEXISTENT");
            
            // Case-insensitive lookups
            (void)tag->getTag("title");
            (void)tag->getTag("Title");
            (void)tag->getTag("TITLE");
            (void)tag->getTag("TiTlE");
            
            // Multi-value accessors
            (void)tag->getTagValues("ARTIST");
            (void)tag->getTagValues("GENRE");
            (void)tag->getTagValues("NONEXISTENT");
            
            // All tags
            auto all_tags = tag->getAllTags();
            (void)all_tags.size();
            
            // Has tag checks
            (void)tag->hasTag("TITLE");
            (void)tag->hasTag("NONEXISTENT");
            (void)tag->hasTag("title");
            
            // Picture accessors
            size_t pic_count = tag->pictureCount();
            (void)pic_count;
            
            // Try to get pictures at various indices
            (void)tag->getPicture(0);
            (void)tag->getPicture(1);
            (void)tag->getPicture(SIZE_MAX);
            (void)tag->getFrontCover();
            
            // If we have pictures, exercise picture accessors
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
            
            // State accessors
            (void)tag->isEmpty();
            (void)tag->formatName();
        }
    }
    
    // Test parse() with nullptr
    (void)VorbisCommentTag::parse(nullptr, 0);
    (void)VorbisCommentTag::parse(nullptr, 100);
    
    // Test with minimum valid structure (vendor length + field count)
    // VorbisComment format: 
    //   4 bytes: vendor length (little-endian)
    //   N bytes: vendor string
    //   4 bytes: field count (little-endian)
    //   For each field:
    //     4 bytes: field length (little-endian)
    //     N bytes: field string (KEY=VALUE format)
    
    if (size >= 8) {
        // Try parsing with various interpretations
        auto tag = VorbisCommentTag::parse(data, size);
        if (tag) {
            // Verify vendor string doesn't exceed reasonable bounds
            std::string vendor = tag->vendorString();
            if (vendor.length() > 1024 * 1024) {
                // Vendor string should be bounded
                __builtin_trap();
            }
        }
    }
    
    // Test with crafted minimal valid VorbisComment
    if (size >= 4) {
        // Use first 4 bytes as vendor length
        uint32_t vendor_len = static_cast<uint32_t>(data[0]) |
                              (static_cast<uint32_t>(data[1]) << 8) |
                              (static_cast<uint32_t>(data[2]) << 16) |
                              (static_cast<uint32_t>(data[3]) << 24);
        
        // If vendor_len is reasonable, try parsing
        if (vendor_len < size) {
            auto tag = VorbisCommentTag::parse(data, size);
            (void)tag;
        }
    }
    
    return 0;
}

/*
 * AFL++ entry point (for AFL fuzzer compatibility)
 * 
 * This allows the same binary to be used with both libFuzzer and AFL++
 */
#ifdef __AFL_COMPILER
#include <cstdio>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    // Read input from stdin
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
 * Standalone test mode (for manual testing without fuzzer)
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

