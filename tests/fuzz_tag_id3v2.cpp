/*
 * fuzz_tag_id3v2.cpp - libFuzzer harness for ID3v2 tag parser
 * 
 * This file implements a libFuzzer target for fuzzing the ID3v2 tag parser.
 * It can be compiled with libFuzzer to perform continuous fuzzing and detect
 * crashes, hangs, and undefined behavior.
 * 
 * The ID3v2 parser is particularly complex due to:
 * - Multiple versions (2.2, 2.3, 2.4) with different frame formats
 * - Synchsafe integer encoding
 * - Multiple text encodings (ISO-8859-1, UTF-16, UTF-16BE, UTF-8)
 * - Unsynchronization
 * - Extended headers
 * - APIC/PIC picture frames
 * 
 * Compile with:
 *   clang++ -fsanitize=fuzzer,address -I./include fuzz_tag_id3v2.cpp \
 *     src/tag/ID3v2Tag.cpp src/tag/ID3v2Utils.cpp src/tag/Tag.cpp \
 *     src/tag/NullTag.cpp src/core/utility/UTF8Util.cpp src/debug.cpp \
 *     -o fuzz_tag_id3v2
 * 
 * Run with:
 *   ./fuzz_tag_id3v2 corpus/ -max_len=10000000
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

// Include ID3v2 headers
#include "tag/ID3v2Tag.h"
#include "tag/ID3v2Utils.h"

using namespace PsyMP3::Tag;

/*
 * libFuzzer entry point
 * 
 * This function is called by libFuzzer with random input data.
 * It attempts to parse the input as an ID3v2 tag and exercises all accessors.
 * 
 * The fuzzer tests:
 * - ID3v2Tag::isValid() with arbitrary data
 * - ID3v2Tag::parse() with arbitrary data
 * - ID3v2Tag::getTagSize() with arbitrary headers
 * - All accessor methods on parsed tags
 * - Frame parsing with various encodings
 * - APIC frame parsing
 * - Synchsafe integer handling
 * - Unsynchronization decoding
 */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Test isValid() with any size data
    if (size > 0) {
        (void)ID3v2Tag::isValid(data, size);
    }
    
    // Test isValid() with nullptr
    (void)ID3v2Tag::isValid(nullptr, 0);
    (void)ID3v2Tag::isValid(nullptr, 100);
    
    // Test getTagSize() with header-sized data
    if (size >= ID3v2Tag::HEADER_SIZE) {
        (void)ID3v2Tag::getTagSize(data);
    }
    
    // Test parse() with any size data
    if (size > 0) {
        auto tag = ID3v2Tag::parse(data, size);
        
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
            
            // ID3v2-specific accessors
            (void)tag->majorVersion();
            (void)tag->minorVersion();
            (void)tag->headerFlags();
            (void)tag->hasUnsynchronization();
            (void)tag->hasExtendedHeader();
            (void)tag->isExperimental();
            (void)tag->hasFooter();
            
            // Extended accessors with ID3v2 frame IDs
            (void)tag->getTag("TIT2");
            (void)tag->getTag("TPE1");
            (void)tag->getTag("TALB");
            (void)tag->getTag("TPE2");
            (void)tag->getTag("TCON");
            (void)tag->getTag("TYER");
            (void)tag->getTag("TDRC");
            (void)tag->getTag("TRCK");
            (void)tag->getTag("TPOS");
            (void)tag->getTag("COMM");
            (void)tag->getTag("TCOM");
            (void)tag->getTag("APIC");
            (void)tag->getTag("NONEXISTENT");
            
            // Common tag name lookups
            (void)tag->getTag("title");
            (void)tag->getTag("artist");
            (void)tag->getTag("album");
            (void)tag->getTag("year");
            (void)tag->getTag("track");
            (void)tag->getTag("genre");
            
            // Multi-value accessors
            (void)tag->getTagValues("TPE1");
            (void)tag->getTagValues("TCON");
            (void)tag->getTagValues("NONEXISTENT");
            
            // All tags
            auto all_tags = tag->getAllTags();
            (void)all_tags.size();
            
            // Has tag checks
            (void)tag->hasTag("TIT2");
            (void)tag->hasTag("NONEXISTENT");
            (void)tag->hasTag("title");
            
            // Frame accessors
            auto frame_ids = tag->getFrameIds();
            (void)frame_ids.size();
            
            for (const auto& id : frame_ids) {
                auto frames = tag->getFrames(id);
                (void)frames.size();
                
                auto frame = tag->getFrame(id);
                if (frame) {
                    (void)frame->id;
                    (void)frame->data.size();
                    (void)frame->flags;
                    (void)frame->isEmpty();
                    (void)frame->size();
                }
            }
            
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
    (void)ID3v2Tag::parse(nullptr, 0);
    (void)ID3v2Tag::parse(nullptr, 100);
    
    // Test ID3v2Utils functions with fuzzer data
    if (size >= 4) {
        // Test synchsafe decoding
        uint32_t synchsafe = static_cast<uint32_t>(data[0]) << 24 |
                            static_cast<uint32_t>(data[1]) << 16 |
                            static_cast<uint32_t>(data[2]) << 8 |
                            static_cast<uint32_t>(data[3]);
        (void)ID3v2Utils::decodeSynchsafe(synchsafe);
        (void)ID3v2Utils::decodeSynchsafeBytes(data);
        
        // Test synchsafe encoding round-trip
        uint32_t decoded = ID3v2Utils::decodeSynchsafeBytes(data);
        if (ID3v2Utils::canEncodeSynchsafe(decoded)) {
            uint32_t encoded = ID3v2Utils::encodeSynchsafe(decoded);
            uint32_t redecoded = ID3v2Utils::decodeSynchsafe(encoded);
            // Round-trip should preserve value
            if (decoded != redecoded) {
                // This would indicate a bug
                __builtin_trap();
            }
        }
    }
    
    // Test text decoding with various encodings
    if (size >= 1) {
        uint8_t encoding = data[0] % 4;  // 0-3 are valid encodings
        
        if (size > 1) {
            (void)ID3v2Utils::decodeText(data + 1, size - 1, 
                static_cast<ID3v2Utils::TextEncoding>(encoding));
        }
        
        (void)ID3v2Utils::decodeTextWithEncoding(data, size);
    }
    
    // Test unsync decoding
    if (size > 0) {
        auto decoded = ID3v2Utils::decodeUnsync(data, size);
        (void)decoded.size();
        
        // Test needsUnsync
        (void)ID3v2Utils::needsUnsync(data, size);
    }
    
    // Test UTF-8 validation and repair
    if (size > 0) {
        std::string test_str(reinterpret_cast<const char*>(data), size);
        (void)ID3v2Utils::isValidUTF8(test_str);
        (void)ID3v2Utils::repairUTF8(test_str);
        (void)ID3v2Utils::decodeUTF8Safe(data, size);
    }
    
    // Test frame ID normalization
    if (size >= 3) {
        std::string frame_id_v22(reinterpret_cast<const char*>(data), 3);
        (void)ID3v2Tag::normalizeFrameId(frame_id_v22, 2);
    }
    if (size >= 4) {
        std::string frame_id_v23(reinterpret_cast<const char*>(data), 4);
        (void)ID3v2Tag::normalizeFrameId(frame_id_v23, 3);
        (void)ID3v2Tag::normalizeFrameId(frame_id_v23, 4);
    }
    
    return 0;
}

/*
 * AFL++ entry point (for AFL fuzzer compatibility)
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

