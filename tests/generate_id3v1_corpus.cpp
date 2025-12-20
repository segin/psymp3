/*
 * generate_id3v1_corpus.cpp - Generate seed corpus for ID3v1 fuzzer
 * 
 * This utility generates a set of seed files for fuzzing the ID3v1 tag parser.
 * The generated files cover various valid and edge-case ID3v1 tags.
 * 
 * Compile with:
 *   g++ -o generate_id3v1_corpus generate_id3v1_corpus.cpp
 * 
 * Run with:
 *   ./generate_id3v1_corpus output_directory/
 * 
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// ID3v1 tag size
constexpr size_t TAG_SIZE = 128;

// Helper to write a seed file
void writeSeedFile(const std::string& dir, const std::string& name, 
                   const uint8_t* data, size_t size) {
    std::string path = dir + "/" + name;
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot create file: " << path << "\n";
        return;
    }
    file.write(reinterpret_cast<const char*>(data), size);
    std::cout << "Created: " << path << " (" << size << " bytes)\n";
}

// Create a basic ID3v1 tag
void createBasicTag(uint8_t* tag, const char* title, const char* artist,
                    const char* album, const char* year, const char* comment,
                    uint8_t genre) {
    memset(tag, 0, TAG_SIZE);
    
    // Magic header
    tag[0] = 'T';
    tag[1] = 'A';
    tag[2] = 'G';
    
    // Title (30 bytes at offset 3)
    if (title) {
        size_t len = strlen(title);
        if (len > 30) len = 30;
        memcpy(tag + 3, title, len);
    }
    
    // Artist (30 bytes at offset 33)
    if (artist) {
        size_t len = strlen(artist);
        if (len > 30) len = 30;
        memcpy(tag + 33, artist, len);
    }
    
    // Album (30 bytes at offset 63)
    if (album) {
        size_t len = strlen(album);
        if (len > 30) len = 30;
        memcpy(tag + 63, album, len);
    }
    
    // Year (4 bytes at offset 93)
    if (year) {
        size_t len = strlen(year);
        if (len > 4) len = 4;
        memcpy(tag + 93, year, len);
    }
    
    // Comment (30 bytes at offset 97)
    if (comment) {
        size_t len = strlen(comment);
        if (len > 30) len = 30;
        memcpy(tag + 97, comment, len);
    }
    
    // Genre (1 byte at offset 127)
    tag[127] = genre;
}

// Create an ID3v1.1 tag with track number
void createID3v1_1Tag(uint8_t* tag, const char* title, const char* artist,
                      const char* album, const char* year, const char* comment,
                      uint8_t track, uint8_t genre) {
    createBasicTag(tag, title, artist, album, year, nullptr, genre);
    
    // Comment (28 bytes at offset 97)
    if (comment) {
        size_t len = strlen(comment);
        if (len > 28) len = 28;
        memcpy(tag + 97, comment, len);
    }
    
    // ID3v1.1 marker and track
    tag[125] = 0x00;  // Null byte marker
    tag[126] = track; // Track number
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <output_directory>\n";
        return 1;
    }
    
    std::string outdir = argv[1];
    uint8_t tag[TAG_SIZE];
    
    std::cout << "Generating ID3v1 fuzzer seed corpus in: " << outdir << "\n\n";
    
    // 1. Valid ID3v1 tag with typical content
    createBasicTag(tag, "Test Song Title", "Test Artist", "Test Album", 
                   "2024", "This is a comment", 17); // Rock
    writeSeedFile(outdir, "valid_id3v1_basic.bin", tag, TAG_SIZE);
    
    // 2. Valid ID3v1.1 tag with track number
    createID3v1_1Tag(tag, "Track Five", "The Band", "Greatest Hits",
                     "2023", "Track comment", 5, 13); // Pop
    writeSeedFile(outdir, "valid_id3v1_1_track5.bin", tag, TAG_SIZE);
    
    // 3. ID3v1.1 with track 1
    createID3v1_1Tag(tag, "First Track", "Artist Name", "Album Name",
                     "2020", "First song", 1, 0); // Blues
    writeSeedFile(outdir, "valid_id3v1_1_track1.bin", tag, TAG_SIZE);
    
    // 4. ID3v1.1 with track 99
    createID3v1_1Tag(tag, "Track 99", "Various", "Compilation",
                     "2019", "", 99, 12); // Other
    writeSeedFile(outdir, "valid_id3v1_1_track99.bin", tag, TAG_SIZE);
    
    // 5. Empty fields (all zeros except header)
    memset(tag, 0, TAG_SIZE);
    tag[0] = 'T'; tag[1] = 'A'; tag[2] = 'G';
    tag[127] = 255; // Unknown genre
    writeSeedFile(outdir, "empty_fields.bin", tag, TAG_SIZE);
    
    // 6. Maximum length strings (30 chars each)
    createBasicTag(tag, 
                   "123456789012345678901234567890",  // 30 chars
                   "ABCDEFGHIJKLMNOPQRSTUVWXYZABCD",  // 30 chars
                   "abcdefghijklmnopqrstuvwxyzabcd",  // 30 chars
                   "9999",
                   "Comment that is exactly 30 ch!",  // 30 chars
                   79); // Hard Rock
    writeSeedFile(outdir, "max_length_strings.bin", tag, TAG_SIZE);
    
    // 7. Strings with trailing spaces
    createBasicTag(tag, "Title   ", "Artist   ", "Album   ",
                   "2000", "Comment   ", 32); // Classical
    writeSeedFile(outdir, "trailing_spaces.bin", tag, TAG_SIZE);
    
    // 8. All spaces
    memset(tag, ' ', TAG_SIZE);
    tag[0] = 'T'; tag[1] = 'A'; tag[2] = 'G';
    tag[127] = 0;
    writeSeedFile(outdir, "all_spaces.bin", tag, TAG_SIZE);
    
    // 9. Genre 0 (Blues)
    createBasicTag(tag, "Blues Song", "Blues Artist", "Blues Album",
                   "1990", "Blues comment", 0);
    writeSeedFile(outdir, "genre_0_blues.bin", tag, TAG_SIZE);
    
    // 10. Genre 191 (last valid Winamp extension)
    createBasicTag(tag, "Psybient Track", "Psybient Artist", "Psybient Album",
                   "2022", "Psybient comment", 191);
    writeSeedFile(outdir, "genre_191_psybient.bin", tag, TAG_SIZE);
    
    // 11. Genre 192 (first invalid)
    createBasicTag(tag, "Unknown Genre", "Artist", "Album",
                   "2021", "Comment", 192);
    writeSeedFile(outdir, "genre_192_invalid.bin", tag, TAG_SIZE);
    
    // 12. Genre 255 (unknown)
    createBasicTag(tag, "Unknown", "Unknown", "Unknown",
                   "0000", "Unknown", 255);
    writeSeedFile(outdir, "genre_255_unknown.bin", tag, TAG_SIZE);
    
    // 13. Year with non-numeric characters
    createBasicTag(tag, "Bad Year", "Artist", "Album",
                   "ABCD", "Comment", 17);
    writeSeedFile(outdir, "year_non_numeric.bin", tag, TAG_SIZE);
    
    // 14. Year partially numeric
    createBasicTag(tag, "Partial Year", "Artist", "Album",
                   "20AB", "Comment", 17);
    writeSeedFile(outdir, "year_partial_numeric.bin", tag, TAG_SIZE);
    
    // 15. Special characters in strings
    createBasicTag(tag, "Title!@#$%^&*()", "Artist<>?:\"{}|",
                   "Album[]\\;',./", "2024", "Comment`~", 17);
    writeSeedFile(outdir, "special_characters.bin", tag, TAG_SIZE);
    
    // 16. High-bit characters (extended ASCII)
    memset(tag, 0, TAG_SIZE);
    tag[0] = 'T'; tag[1] = 'A'; tag[2] = 'G';
    for (int i = 3; i < 33; ++i) tag[i] = 0x80 + (i % 128);
    for (int i = 33; i < 63; ++i) tag[i] = 0xC0 + (i % 64);
    for (int i = 63; i < 93; ++i) tag[i] = 0xE0 + (i % 32);
    tag[127] = 17;
    writeSeedFile(outdir, "high_bit_chars.bin", tag, TAG_SIZE);
    
    // 17. Null bytes in middle of strings
    createBasicTag(tag, "Title", "Artist", "Album", "2024", "Comment", 17);
    tag[10] = 0;  // Null in middle of title
    tag[40] = 0;  // Null in middle of artist
    writeSeedFile(outdir, "embedded_nulls.bin", tag, TAG_SIZE);
    
    // 18. ID3v1.0 with byte 125 non-zero (not ID3v1.1)
    createBasicTag(tag, "Not v1.1", "Artist", "Album", "2024",
                   "Comment with byte 125 set!", 17);
    // Ensure byte 125 is non-zero (part of comment)
    tag[125] = 'X';
    tag[126] = 5;  // This is NOT a track number in ID3v1.0
    writeSeedFile(outdir, "id3v1_0_not_v1_1.bin", tag, TAG_SIZE);
    
    // 19. Truncated tag (less than 128 bytes) - invalid
    memset(tag, 0, TAG_SIZE);
    tag[0] = 'T'; tag[1] = 'A'; tag[2] = 'G';
    writeSeedFile(outdir, "truncated_64bytes.bin", tag, 64);
    
    // 20. Just the header
    writeSeedFile(outdir, "header_only.bin", tag, 3);
    
    // 21. Invalid header
    memset(tag, 0, TAG_SIZE);
    tag[0] = 'X'; tag[1] = 'Y'; tag[2] = 'Z';
    writeSeedFile(outdir, "invalid_header.bin", tag, TAG_SIZE);
    
    // 22. Almost valid header
    memset(tag, 0, TAG_SIZE);
    tag[0] = 'T'; tag[1] = 'A'; tag[2] = 'X';  // TAX instead of TAG
    writeSeedFile(outdir, "almost_valid_header.bin", tag, TAG_SIZE);
    
    // 23. Lowercase header
    memset(tag, 0, TAG_SIZE);
    tag[0] = 't'; tag[1] = 'a'; tag[2] = 'g';
    writeSeedFile(outdir, "lowercase_header.bin", tag, TAG_SIZE);
    
    // 24. All 0xFF bytes except header
    memset(tag, 0xFF, TAG_SIZE);
    tag[0] = 'T'; tag[1] = 'A'; tag[2] = 'G';
    writeSeedFile(outdir, "all_0xff.bin", tag, TAG_SIZE);
    
    // 25. Empty file
    writeSeedFile(outdir, "empty.bin", tag, 0);
    
    // 26. Single byte
    tag[0] = 'T';
    writeSeedFile(outdir, "single_byte.bin", tag, 1);
    
    // 27. Two bytes
    tag[0] = 'T'; tag[1] = 'A';
    writeSeedFile(outdir, "two_bytes.bin", tag, 2);
    
    // 28. Oversized (more than 128 bytes)
    uint8_t oversized[256];
    memset(oversized, 0, 256);
    oversized[0] = 'T'; oversized[1] = 'A'; oversized[2] = 'G';
    memcpy(oversized + 3, "Oversized Tag", 13);
    oversized[127] = 17;
    writeSeedFile(outdir, "oversized_256bytes.bin", oversized, 256);
    
    std::cout << "\nSeed corpus generation complete.\n";
    return 0;
}
