/*
 * generate_picture_corpus.cpp - Generate seed corpus for picture fuzzer
 * 
 * This utility generates seed data for fuzzing picture parsing in both
 * ID3v2 APIC frames and VorbisComment METADATA_BLOCK_PICTURE fields.
 * 
 * Compile with:
 *   g++ -o generate_picture_corpus generate_picture_corpus.cpp
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

void writeFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot create file: " << path << "\n";
        return;
    }
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    std::cout << "Created: " << path << " (" << data.size() << " bytes)\n";
}

// Minimal JPEG header
std::vector<uint8_t> minimalJPEG() {
    return {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 'J', 'F', 'I', 'F', 0x00,
            0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
            0xFF, 0xD9};  // EOI marker
}

// Minimal PNG header
std::vector<uint8_t> minimalPNG() {
    return {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A,  // PNG signature
            0x00, 0x00, 0x00, 0x0D,  // IHDR length
            'I', 'H', 'D', 'R',      // IHDR type
            0x00, 0x00, 0x00, 0x01,  // width = 1
            0x00, 0x00, 0x00, 0x01,  // height = 1
            0x08, 0x02,              // bit depth, color type
            0x00, 0x00, 0x00,        // compression, filter, interlace
            0x90, 0x77, 0x53, 0xDE}; // CRC
}

// Minimal GIF header
std::vector<uint8_t> minimalGIF() {
    return {'G', 'I', 'F', '8', '9', 'a',  // GIF89a
            0x01, 0x00,  // width = 1
            0x01, 0x00,  // height = 1
            0x00, 0x00, 0x00,  // flags, bgcolor, aspect
            0x3B};  // trailer
}

int main() {
    const std::string corpus_dir = "tests/data/fuzz_corpus/picture/";
    
    // ========================================================================
    // Valid picture data seeds
    // ========================================================================
    
    // 1. Valid JPEG-like data
    {
        auto data = minimalJPEG();
        writeFile(corpus_dir + "valid_jpeg.bin", data);
    }
    
    // 2. Valid PNG-like data
    {
        auto data = minimalPNG();
        writeFile(corpus_dir + "valid_png.bin", data);
    }
    
    // 3. Valid GIF-like data
    {
        auto data = minimalGIF();
        writeFile(corpus_dir + "valid_gif.bin", data);
    }
    
    // 4. Picture with all metadata fields
    {
        std::vector<uint8_t> data;
        // Picture type (front cover = 3)
        data.push_back(0); data.push_back(0);
        data.push_back(0); data.push_back(3);
        // MIME type length
        data.push_back(10);
        // MIME type
        for (char c : "image/jpeg") data.push_back(c);
        // Description length
        data.push_back(11);
        // Description
        for (char c : "Front Cover") data.push_back(c);
        // Dimensions
        data.push_back(0); data.push_back(0); data.push_back(2); data.push_back(0);  // width=512
        data.push_back(0); data.push_back(0); data.push_back(2); data.push_back(0);  // height=512
        data.push_back(0); data.push_back(0); data.push_back(0); data.push_back(24); // depth=24
        data.push_back(0); data.push_back(0); data.push_back(0); data.push_back(0);  // colors=0
        // Image data
        auto jpeg = minimalJPEG();
        for (uint8_t b : jpeg) data.push_back(b);
        
        writeFile(corpus_dir + "valid_full_metadata.bin", data);
    }
    
    // ========================================================================
    // Edge cases
    // ========================================================================
    
    // 5. Empty image data
    {
        std::vector<uint8_t> data;
        data.push_back(0); data.push_back(0); data.push_back(0); data.push_back(3);
        data.push_back(10);
        for (char c : "image/jpeg") data.push_back(c);
        data.push_back(0);  // empty description
        // Zero dimensions
        for (int i = 0; i < 16; ++i) data.push_back(0);
        // No image data
        writeFile(corpus_dir + "edge_empty_image.bin", data);
    }
    
    // 6. Very long MIME type
    {
        std::vector<uint8_t> data;
        data.push_back(0); data.push_back(0); data.push_back(0); data.push_back(3);
        data.push_back(200);  // Long MIME
        for (int i = 0; i < 200; ++i) data.push_back('x');
        data.push_back(0);
        for (int i = 0; i < 16; ++i) data.push_back(0);
        writeFile(corpus_dir + "edge_long_mime.bin", data);
    }
    
    // 7. Very long description
    {
        std::vector<uint8_t> data;
        data.push_back(0); data.push_back(0); data.push_back(0); data.push_back(3);
        data.push_back(10);
        for (char c : "image/jpeg") data.push_back(c);
        data.push_back(255);  // Long description
        for (int i = 0; i < 255; ++i) data.push_back('D');
        for (int i = 0; i < 16; ++i) data.push_back(0);
        writeFile(corpus_dir + "edge_long_description.bin", data);
    }
    
    // 8. All picture types (0-20)
    for (uint8_t type = 0; type <= 20; ++type) {
        std::vector<uint8_t> data;
        data.push_back(0); data.push_back(0); data.push_back(0); data.push_back(type);
        data.push_back(10);
        for (char c : "image/jpeg") data.push_back(c);
        data.push_back(0);
        for (int i = 0; i < 16; ++i) data.push_back(0);
        auto jpeg = minimalJPEG();
        for (uint8_t b : jpeg) data.push_back(b);
        writeFile(corpus_dir + "valid_type_" + std::to_string(type) + ".bin", data);
    }
    
    // ========================================================================
    // Malformed data
    // ========================================================================
    
    // 9. Invalid picture type (> 20)
    {
        std::vector<uint8_t> data;
        data.push_back(0); data.push_back(0); data.push_back(0); data.push_back(0xFF);
        data.push_back(10);
        for (char c : "image/jpeg") data.push_back(c);
        data.push_back(0);
        for (int i = 0; i < 16; ++i) data.push_back(0);
        writeFile(corpus_dir + "malformed_invalid_type.bin", data);
    }
    
    // 10. Truncated data
    {
        std::vector<uint8_t> data = {0, 0, 0, 3, 10};  // Only partial header
        writeFile(corpus_dir + "malformed_truncated.bin", data);
    }
    
    // 11. MIME type claiming more than available
    {
        std::vector<uint8_t> data;
        data.push_back(0); data.push_back(0); data.push_back(0); data.push_back(3);
        data.push_back(100);  // Claim 100 bytes
        data.push_back('x'); data.push_back('y'); data.push_back('z');  // Only 3
        writeFile(corpus_dir + "malformed_mime_overflow.bin", data);
    }
    
    // 12. Huge dimensions
    {
        std::vector<uint8_t> data;
        data.push_back(0); data.push_back(0); data.push_back(0); data.push_back(3);
        data.push_back(10);
        for (char c : "image/jpeg") data.push_back(c);
        data.push_back(0);
        // Huge width/height
        data.push_back(0xFF); data.push_back(0xFF); data.push_back(0xFF); data.push_back(0xFF);
        data.push_back(0xFF); data.push_back(0xFF); data.push_back(0xFF); data.push_back(0xFF);
        data.push_back(0); data.push_back(0); data.push_back(0); data.push_back(24);
        data.push_back(0); data.push_back(0); data.push_back(0); data.push_back(0);
        writeFile(corpus_dir + "malformed_huge_dimensions.bin", data);
    }
    
    // 13. Binary garbage
    {
        std::vector<uint8_t> data;
        for (int i = 0; i < 256; ++i) {
            data.push_back(static_cast<uint8_t>(i));
        }
        writeFile(corpus_dir + "malformed_garbage.bin", data);
    }
    
    // 14. All zeros
    {
        std::vector<uint8_t> data(128, 0);
        writeFile(corpus_dir + "edge_all_zeros.bin", data);
    }
    
    // 15. All 0xFF
    {
        std::vector<uint8_t> data(128, 0xFF);
        writeFile(corpus_dir + "edge_all_ff.bin", data);
    }
    
    std::cout << "\nPicture seed corpus generation complete.\n";
    return 0;
}

