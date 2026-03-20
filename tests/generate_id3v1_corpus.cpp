/*
 * generate_id3v1_corpus.cpp - Generate seed corpus for ID3v1 tag fuzzer
 * 
 * This utility generates valid ID3v1/ID3v1.1 binary data for use as
 * seed corpus in fuzzing tests.
 * 
 * ID3v1 format (128 bytes):
 *   Offset  Size  Description
 *   0       3     "TAG" identifier
 *   3       30    Title
 *   33      30    Artist
 *   63      30    Album
 *   93      4     Year
 *   97      30    Comment (28 bytes + null + track for ID3v1.1)
 *   127     1     Genre index
 * 
 * Compile with:
 *   g++ -o generate_id3v1_corpus generate_id3v1_corpus.cpp
 * 
 * Run with:
 *   ./generate_id3v1_corpus
 * 
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <array>

// ID3v1 tag size
constexpr size_t ID3V1_SIZE = 128;

// Create ID3v1 tag
std::array<uint8_t, ID3V1_SIZE> createID3v1(
    const std::string& title,
    const std::string& artist,
    const std::string& album,
    const std::string& year,
    const std::string& comment,
    uint8_t genre,
    int track = -1)  // -1 = ID3v1, >= 0 = ID3v1.1
{
    std::array<uint8_t, ID3V1_SIZE> tag;
    tag.fill(0);
    
    // "TAG" identifier
    tag[0] = 'T';
    tag[1] = 'A';
    tag[2] = 'G';
    
    // Title (30 bytes at offset 3)
    size_t len = std::min(title.size(), static_cast<size_t>(30));
    std::memcpy(&tag[3], title.c_str(), len);
    
    // Artist (30 bytes at offset 33)
    len = std::min(artist.size(), static_cast<size_t>(30));
    std::memcpy(&tag[33], artist.c_str(), len);
    
    // Album (30 bytes at offset 63)
    len = std::min(album.size(), static_cast<size_t>(30));
    std::memcpy(&tag[63], album.c_str(), len);
    
    // Year (4 bytes at offset 93)
    len = std::min(year.size(), static_cast<size_t>(4));
    std::memcpy(&tag[93], year.c_str(), len);
    
    // Comment (30 bytes at offset 97)
    if (track >= 0) {
        // ID3v1.1: 28 bytes comment + null + track
        len = std::min(comment.size(), static_cast<size_t>(28));
        std::memcpy(&tag[97], comment.c_str(), len);
        tag[125] = 0;  // Null byte indicates ID3v1.1
        tag[126] = static_cast<uint8_t>(track);
    } else {
        // ID3v1: 30 bytes comment
        len = std::min(comment.size(), static_cast<size_t>(30));
        std::memcpy(&tag[97], comment.c_str(), len);
    }
    
    // Genre (1 byte at offset 127)
    tag[127] = genre;
    
    return tag;
}

// Write binary file
void writeFile(const std::string& path, const uint8_t* data, size_t size) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot create file: " << path << "\n";
        return;
    }
    file.write(reinterpret_cast<const char*>(data), size);
    std::cout << "Created: " << path << " (" << size << " bytes)\n";
}

template<size_t N>
void writeFile(const std::string& path, const std::array<uint8_t, N>& data) {
    writeFile(path, data.data(), data.size());
}

int main() {
    const std::string corpus_dir = "tests/data/fuzz_corpus/id3v1/";
    
    // 1. Valid ID3v1 tag with all fields
    {
        auto tag = createID3v1(
            "Test Title",
            "Test Artist",
            "Test Album",
            "2025",
            "This is a test comment",
            17  // Rock genre
        );
        writeFile(corpus_dir + "valid_id3v1.bin", tag);
    }
    
    // 2. Valid ID3v1.1 tag with track number
    {
        auto tag = createID3v1(
            "Track Title",
            "Track Artist",
            "Track Album",
            "2024",
            "Comment with track",
            13,  // Pop genre
            5    // Track 5
        );
        writeFile(corpus_dir + "valid_id3v1_1.bin", tag);
    }
    
    // 3. ID3v1 with maximum length fields
    {
        auto tag = createID3v1(
            "123456789012345678901234567890",  // 30 chars
            "123456789012345678901234567890",
            "123456789012345678901234567890",
            "2025",
            "123456789012345678901234567890",
            0  // Blues
        );
        writeFile(corpus_dir + "valid_max_length.bin", tag);
    }
    
    // 4. ID3v1 with empty fields
    {
        auto tag = createID3v1("", "", "", "", "", 255);  // 255 = unknown genre
        writeFile(corpus_dir + "valid_empty_fields.bin", tag);
    }
    
    // 5. ID3v1 with spaces (should be trimmed)
    {
        auto tag = createID3v1(
            "Title with spaces     ",
            "Artist with spaces    ",
            "Album with spaces     ",
            "2025",
            "Comment with spaces   ",
            12
        );
        writeFile(corpus_dir + "valid_trailing_spaces.bin", tag);
    }
    
    // 6. ID3v1 with various genre indices
    for (uint8_t genre : {0, 1, 17, 79, 80, 147, 191, 192, 254, 255}) {
        auto tag = createID3v1(
            "Genre Test",
            "Artist",
            "Album",
            "2025",
            "Testing genre " + std::to_string(genre),
            genre
        );
        writeFile(corpus_dir + "valid_genre_" + std::to_string(genre) + ".bin", tag);
    }
    
    // 7. ID3v1.1 with track 0 (edge case)
    {
        auto tag = createID3v1(
            "Track Zero",
            "Artist",
            "Album",
            "2025",
            "Track number is zero",
            17,
            0  // Track 0
        );
        writeFile(corpus_dir + "edge_track_zero.bin", tag);
    }
    
    // 8. ID3v1.1 with track 255 (max)
    {
        auto tag = createID3v1(
            "Track Max",
            "Artist",
            "Album",
            "2025",
            "Track number is 255",
            17,
            255  // Track 255
        );
        writeFile(corpus_dir + "edge_track_max.bin", tag);
    }
    
    // 9. ID3v1 with non-ASCII characters (Latin-1)
    {
        auto tag = createID3v1(
            "Caf\xe9 M\xfcsic",      // Café Müsic in Latin-1
            "\xc9ric Cl\xe4pton",    // Éric Cläpton
            "Gr\xf6\xdftes Album",   // Größtes Album
            "2025",
            "Sch\xf6ne Musik",       // Schöne Musik
            17
        );
        writeFile(corpus_dir + "valid_latin1.bin", tag);
    }
    
    // 10. Malformed: wrong magic bytes
    {
        std::array<uint8_t, ID3V1_SIZE> tag;
        tag.fill(0);
        tag[0] = 'X';
        tag[1] = 'Y';
        tag[2] = 'Z';
        writeFile(corpus_dir + "malformed_wrong_magic.bin", tag);
    }
    
    // 11. Malformed: partial magic
    {
        std::array<uint8_t, ID3V1_SIZE> tag;
        tag.fill(0);
        tag[0] = 'T';
        tag[1] = 'A';
        tag[2] = 'X';  // Wrong third byte
        writeFile(corpus_dir + "malformed_partial_magic.bin", tag);
    }
    
    // 12. Truncated: less than 128 bytes
    {
        auto tag = createID3v1("Title", "Artist", "Album", "2025", "Comment", 17);
        writeFile(corpus_dir + "malformed_truncated_64.bin", tag.data(), 64);
        writeFile(corpus_dir + "malformed_truncated_100.bin", tag.data(), 100);
        writeFile(corpus_dir + "malformed_truncated_127.bin", tag.data(), 127);
    }
    
    // 13. Extended: more than 128 bytes (should only read first 128)
    {
        std::vector<uint8_t> extended(256);
        auto tag = createID3v1("Extended", "Artist", "Album", "2025", "Comment", 17);
        std::memcpy(extended.data(), tag.data(), ID3V1_SIZE);
        // Fill rest with garbage
        for (size_t i = ID3V1_SIZE; i < extended.size(); ++i) {
            extended[i] = static_cast<uint8_t>(i & 0xFF);
        }
        writeFile(corpus_dir + "edge_extended.bin", extended.data(), extended.size());
    }
    
    // 14. All nulls (except TAG)
    {
        std::array<uint8_t, ID3V1_SIZE> tag;
        tag.fill(0);
        tag[0] = 'T';
        tag[1] = 'A';
        tag[2] = 'G';
        writeFile(corpus_dir + "edge_all_nulls.bin", tag);
    }
    
    // 15. All 0xFF (except TAG)
    {
        std::array<uint8_t, ID3V1_SIZE> tag;
        tag.fill(0xFF);
        tag[0] = 'T';
        tag[1] = 'A';
        tag[2] = 'G';
        writeFile(corpus_dir + "edge_all_ff.bin", tag);
    }
    
    // 16. Year edge cases
    {
        auto tag1 = createID3v1("Title", "Artist", "Album", "0000", "Comment", 17);
        writeFile(corpus_dir + "edge_year_0000.bin", tag1);
        
        auto tag2 = createID3v1("Title", "Artist", "Album", "9999", "Comment", 17);
        writeFile(corpus_dir + "edge_year_9999.bin", tag2);
        
        auto tag3 = createID3v1("Title", "Artist", "Album", "XXXX", "Comment", 17);
        writeFile(corpus_dir + "edge_year_invalid.bin", tag3);
    }
    
    std::cout << "\nID3v1 seed corpus generation complete.\n";
    return 0;
}

