/*
 * generate_vorbis_corpus.cpp - Generate seed corpus for VorbisComment fuzzer
 * 
 * This utility generates valid VorbisComment binary data for use as
 * seed corpus in fuzzing tests.
 * 
 * Compile with:
 *   g++ -o generate_vorbis_corpus generate_vorbis_corpus.cpp
 * 
 * Run with:
 *   ./generate_vorbis_corpus
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

// Write little-endian uint32
void writeLE32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

// Write string with length prefix
void writeString(std::vector<uint8_t>& out, const std::string& str) {
    writeLE32(out, static_cast<uint32_t>(str.size()));
    for (char c : str) {
        out.push_back(static_cast<uint8_t>(c));
    }
}

// Create VorbisComment block
std::vector<uint8_t> createVorbisComment(
    const std::string& vendor,
    const std::vector<std::string>& fields) {
    
    std::vector<uint8_t> data;
    
    // Vendor string
    writeString(data, vendor);
    
    // Field count
    writeLE32(data, static_cast<uint32_t>(fields.size()));
    
    // Fields
    for (const auto& field : fields) {
        writeString(data, field);
    }
    
    return data;
}

// Write binary file
void writeFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot create file: " << path << "\n";
        return;
    }
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    std::cout << "Created: " << path << " (" << data.size() << " bytes)\n";
}

int main() {
    const std::string corpus_dir = "tests/data/fuzz_corpus/vorbis/";
    
    // 1. Minimal valid VorbisComment (empty vendor, no fields)
    {
        auto data = createVorbisComment("", {});
        writeFile(corpus_dir + "valid_minimal.bin", data);
    }
    
    // 2. Basic VorbisComment with common fields
    {
        auto data = createVorbisComment("libvorbis 1.3.7", {
            "TITLE=Test Track",
            "ARTIST=Test Artist",
            "ALBUM=Test Album",
            "DATE=2025",
            "TRACKNUMBER=1",
            "GENRE=Rock"
        });
        writeFile(corpus_dir + "valid_basic.bin", data);
    }
    
    // 3. VorbisComment with multi-valued fields
    {
        auto data = createVorbisComment("Xiph.Org libVorbis I 20200704", {
            "TITLE=Multi-Artist Track",
            "ARTIST=Artist One",
            "ARTIST=Artist Two",
            "ARTIST=Artist Three",
            "GENRE=Rock",
            "GENRE=Alternative",
            "ALBUM=Compilation Album"
        });
        writeFile(corpus_dir + "valid_multivalue.bin", data);
    }
    
    // 4. VorbisComment with all standard fields
    {
        auto data = createVorbisComment("reference libFLAC 1.4.3", {
            "TITLE=Complete Metadata Test",
            "ARTIST=Test Artist",
            "ALBUM=Test Album",
            "ALBUMARTIST=Various Artists",
            "DATE=2025-06-15",
            "TRACKNUMBER=5",
            "TRACKTOTAL=12",
            "DISCNUMBER=1",
            "DISCTOTAL=2",
            "GENRE=Electronic",
            "COMMENT=This is a test comment",
            "DESCRIPTION=Extended description field",
            "COMPOSER=Test Composer",
            "PERFORMER=Test Performer",
            "COPYRIGHT=2025 Test Copyright",
            "LICENSE=CC-BY-4.0",
            "ORGANIZATION=Test Organization",
            "LOCATION=Test Location",
            "CONTACT=test@example.com",
            "ISRC=USRC12345678"
        });
        writeFile(corpus_dir + "valid_complete.bin", data);
    }
    
    // 5. Edge case: empty vendor string
    {
        auto data = createVorbisComment("", {
            "TITLE=No Vendor",
            "ARTIST=Unknown"
        });
        writeFile(corpus_dir + "edge_empty_vendor.bin", data);
    }
    
    // 6. Edge case: empty field values
    {
        auto data = createVorbisComment("test", {
            "TITLE=",
            "ARTIST=",
            "ALBUM=Has Value",
            "GENRE="
        });
        writeFile(corpus_dir + "edge_empty_values.bin", data);
    }
    
    // 7. Edge case: Unicode in fields
    {
        auto data = createVorbisComment("libvorbis", {
            "TITLE=日本語タイトル",
            "ARTIST=アーティスト名",
            "ALBUM=Ümläüts Ëvërÿwhërë",
            "COMMENT=Emoji: 🎵🎶🎸",
            "DESCRIPTION=Mixed: Hello 世界 مرحبا"
        });
        writeFile(corpus_dir + "edge_unicode.bin", data);
    }
    
    // 8. Edge case: very long field
    {
        std::string long_value(1000, 'A');
        auto data = createVorbisComment("test", {
            "TITLE=" + long_value,
            "ARTIST=Normal"
        });
        writeFile(corpus_dir + "edge_long_field.bin", data);
    }
    
    // 9. Edge case: many fields
    {
        std::vector<std::string> fields;
        for (int i = 0; i < 100; ++i) {
            fields.push_back("CUSTOM" + std::to_string(i) + "=Value" + std::to_string(i));
        }
        auto data = createVorbisComment("test", fields);
        writeFile(corpus_dir + "edge_many_fields.bin", data);
    }
    
    // 10. Edge case: field with equals sign in value
    {
        auto data = createVorbisComment("test", {
            "TITLE=A=B=C",
            "COMMENT=x=y=z=w",
            "DESCRIPTION=key=value pairs: a=1, b=2"
        });
        writeFile(corpus_dir + "edge_equals_in_value.bin", data);
    }
    
    // 11. Edge case: case variations in field names
    {
        auto data = createVorbisComment("test", {
            "title=lowercase",
            "TITLE=UPPERCASE",
            "Title=MixedCase",
            "TiTlE=AlTeRnAtInG"
        });
        writeFile(corpus_dir + "edge_case_variations.bin", data);
    }
    
    // 12. Malformed: truncated vendor length
    {
        std::vector<uint8_t> data = {0x10, 0x00}; // Only 2 bytes of length
        writeFile(corpus_dir + "malformed_truncated_vendor_len.bin", data);
    }
    
    // 13. Malformed: vendor length exceeds data
    {
        std::vector<uint8_t> data;
        writeLE32(data, 1000); // Claim 1000 bytes
        data.push_back('X');   // But only provide 1
        writeFile(corpus_dir + "malformed_vendor_overflow.bin", data);
    }
    
    // 14. Malformed: field without equals sign
    {
        auto data = createVorbisComment("test", {
            "TITLE=Valid",
            "INVALIDFIELD",  // No equals sign
            "ARTIST=Also Valid"
        });
        writeFile(corpus_dir + "malformed_no_equals.bin", data);
    }
    
    // 15. Malformed: field count exceeds actual fields
    {
        std::vector<uint8_t> data;
        writeString(data, "test");
        writeLE32(data, 100); // Claim 100 fields
        writeString(data, "TITLE=Only One"); // But only provide 1
        writeFile(corpus_dir + "malformed_field_count.bin", data);
    }
    
    std::cout << "\nSeed corpus generation complete.\n";
    return 0;
}

