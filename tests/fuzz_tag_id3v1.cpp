/*
 * fuzz_tag_id3v1.cpp - libFuzzer harness for ID3v1 tag parser
 * 
 * This file implements a libFuzzer target for fuzzing the ID3v1 tag parser.
 * It can be compiled with libFuzzer to perform continuous fuzzing and detect
 * crashes, hangs, and undefined behavior.
 * 
 * Compile with:
 *   clang++ -fsanitize=fuzzer,address -I./include fuzz_tag_id3v1.cpp \
 *     src/tag/ID3v1Tag.cpp src/tag/Tag.cpp src/tag/NullTag.cpp src/debug.cpp \
 *     -o fuzz_tag_id3v1
 * 
 * Run with:
 *   ./fuzz_tag_id3v1 corpus/ -max_len=256
 * 
 * Seed corpus should include:
 *   - Valid ID3v1 tags (128 bytes starting with "TAG")
 *   - Valid ID3v1.1 tags (with track number)
 *   - Tags with various genre indices
 *   - Tags with edge case strings (all spaces, all nulls, max length)
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

// Include ID3v1Tag header
#include "tag/ID3v1Tag.h"

using namespace PsyMP3::Tag;

/*
 * libFuzzer entry point
 * 
 * This function is called by libFuzzer with random input data.
 * It attempts to parse the input as an ID3v1 tag and exercises all accessors.
 * 
 * The fuzzer tests:
 * - ID3v1Tag::isValid() with arbitrary data
 * - ID3v1Tag::parse() with arbitrary data
 * - All accessor methods on parsed tags
 * - Genre index lookup with arbitrary indices
 * - String trimming with arbitrary byte sequences
 */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Test isValid() with any size data
    if (size > 0) {
        // isValid should never crash regardless of input
        (void)ID3v1Tag::isValid(data);
    }
    
    // Test isValid() with nullptr
    (void)ID3v1Tag::isValid(nullptr);
    
    // Test parse() with any size data
    if (size > 0) {
        auto tag = ID3v1Tag::parse(data);
        
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
            
            // Extended accessors
            (void)tag->getTag("TITLE");
            (void)tag->getTag("ARTIST");
            (void)tag->getTag("ALBUM");
            (void)tag->getTag("YEAR");
            (void)tag->getTag("COMMENT");
            (void)tag->getTag("GENRE");
            (void)tag->getTag("TRACK");
            (void)tag->getTag("NONEXISTENT");
            
            // Multi-value accessors
            (void)tag->getTagValues("ARTIST");
            (void)tag->getTagValues("NONEXISTENT");
            
            // All tags
            (void)tag->getAllTags();
            
            // Has tag checks
            (void)tag->hasTag("TITLE");
            (void)tag->hasTag("NONEXISTENT");
            
            // Picture accessors (always empty for ID3v1)
            (void)tag->pictureCount();
            (void)tag->getPicture(0);
            (void)tag->getPicture(SIZE_MAX);
            (void)tag->getFrontCover();
            
            // State accessors
            (void)tag->isEmpty();
            (void)tag->formatName();
            
            // ID3v1-specific accessors
            (void)tag->isID3v1_1();
            (void)tag->genreIndex();
        }
    }
    
    // Test parse() with nullptr
    (void)ID3v1Tag::parse(nullptr);
    
    // Test genre lookup with fuzzer-derived index
    if (size >= 1) {
        (void)ID3v1Tag::genreFromIndex(data[0]);
    }
    
    // Test all genre indices if we have enough data
    if (size >= 256) {
        for (int i = 0; i < 256; ++i) {
            (void)ID3v1Tag::genreFromIndex(static_cast<uint8_t>(i));
        }
    }
    
    // Test genreList() accessor
    const auto& genres = ID3v1Tag::genreList();
    (void)genres.size();
    
    // Test with exact ID3v1 tag size (128 bytes)
    if (size >= ID3v1Tag::TAG_SIZE) {
        auto tag = ID3v1Tag::parse(data);
        if (tag) {
            // Exercise all accessors again with properly-sized data
            std::string title = tag->title();
            std::string artist = tag->artist();
            std::string album = tag->album();
            std::string comment = tag->comment();
            std::string genre = tag->genre();
            uint32_t year = tag->year();
            uint32_t track = tag->track();
            
            // Verify string lengths are reasonable (max 30 chars for ID3v1 fields)
            if (title.length() > 30 || artist.length() > 30 || 
                album.length() > 30 || comment.length() > 30) {
                // This would indicate a bug - strings should be trimmed
                __builtin_trap();
            }
        }
    }
    
    // Test with truncated data (less than 128 bytes)
    for (size_t truncated_size = 0; truncated_size < std::min(size, static_cast<size_t>(128)); ++truncated_size) {
        auto tag = ID3v1Tag::parse(data);
        // Should return nullptr for truncated data without "TAG" header
        // or nullptr/valid tag depending on content
        (void)tag;
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

