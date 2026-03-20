/*
 * generate_id3v2_corpus.cpp - Generate seed corpus for ID3v2 tag fuzzer
 * 
 * This utility generates valid and "almost-right" ID3v2 binary data for use as
 * seed corpus in fuzzing tests. The "almost-right" approach creates inputs that
 * are structurally valid but contain subtle errors to maximize fuzzer coverage.
 * 
 * ID3v2 format:
 *   Header (10 bytes):
 *     3 bytes: "ID3" identifier
 *     1 byte:  Major version (2, 3, or 4)
 *     1 byte:  Minor version (typically 0)
 *     1 byte:  Flags
 *     4 bytes: Tag size (synchsafe integer, excludes header)
 *   
 *   Frames (variable):
 *     v2.2: 3-byte ID + 3-byte size
 *     v2.3: 4-byte ID + 4-byte size + 2-byte flags
 *     v2.4: 4-byte ID + 4-byte synchsafe size + 2-byte flags
 * 
 * Compile with:
 *   g++ -o generate_id3v2_corpus generate_id3v2_corpus.cpp
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

// Encode synchsafe integer (28-bit value in 4 bytes, MSB of each byte is 0)
void encodeSynchsafe(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 21) & 0x7F));
    out.push_back(static_cast<uint8_t>((value >> 14) & 0x7F));
    out.push_back(static_cast<uint8_t>((value >> 7) & 0x7F));
    out.push_back(static_cast<uint8_t>(value & 0x7F));
}

// Encode regular 32-bit big-endian integer
void encodeBE32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

// Encode 24-bit big-endian integer (for v2.2 frame sizes)
void encodeBE24(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

// Encode 16-bit big-endian integer
void encodeBE16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

// Create ID3v2 header
std::vector<uint8_t> createHeader(uint8_t major_version, uint8_t flags, uint32_t tag_size) {
    std::vector<uint8_t> header;
    header.push_back('I');
    header.push_back('D');
    header.push_back('3');
    header.push_back(major_version);
    header.push_back(0);  // minor version always 0
    header.push_back(flags);
    encodeSynchsafe(header, tag_size);
    return header;
}

// Create v2.3/v2.4 text frame
std::vector<uint8_t> createTextFrame(const std::string& frame_id,
                                      const std::string& text,
                                      uint8_t encoding = 0,
                                      bool synchsafe_size = false) {
    std::vector<uint8_t> frame;
    
    // Frame ID (4 bytes)
    for (char c : frame_id) {
        frame.push_back(static_cast<uint8_t>(c));
    }
    while (frame.size() < 4) frame.push_back(0);
    
    // Frame size (encoding byte + text)
    uint32_t data_size = 1 + text.size();
    if (synchsafe_size) {
        encodeSynchsafe(frame, data_size);
    } else {
        encodeBE32(frame, data_size);
    }
    
    // Frame flags (2 bytes)
    frame.push_back(0);
    frame.push_back(0);
    
    // Encoding byte
    frame.push_back(encoding);
    
    // Text data
    for (char c : text) {
        frame.push_back(static_cast<uint8_t>(c));
    }
    
    return frame;
}

// Create v2.2 text frame (3-byte ID, 3-byte size)
std::vector<uint8_t> createTextFrameV22(const std::string& frame_id,
                                         const std::string& text,
                                         uint8_t encoding = 0) {
    std::vector<uint8_t> frame;
    
    // Frame ID (3 bytes)
    for (size_t i = 0; i < 3 && i < frame_id.size(); ++i) {
        frame.push_back(static_cast<uint8_t>(frame_id[i]));
    }
    while (frame.size() < 3) frame.push_back(0);
    
    // Frame size (3 bytes, encoding byte + text)
    uint32_t data_size = 1 + text.size();
    encodeBE24(frame, data_size);
    
    // Encoding byte
    frame.push_back(encoding);
    
    // Text data
    for (char c : text) {
        frame.push_back(static_cast<uint8_t>(c));
    }
    
    return frame;
}

// Create APIC frame (picture)
std::vector<uint8_t> createAPICFrame(const std::string& mime_type,
                                      uint8_t picture_type,
                                      const std::string& description,
                                      const std::vector<uint8_t>& image_data,
                                      bool synchsafe_size = false) {
    std::vector<uint8_t> frame;
    
    // Frame ID
    frame.push_back('A');
    frame.push_back('P');
    frame.push_back('I');
    frame.push_back('C');
    
    // Calculate data size
    uint32_t data_size = 1 + mime_type.size() + 1 + 1 + description.size() + 1 + image_data.size();
    if (synchsafe_size) {
        encodeSynchsafe(frame, data_size);
    } else {
        encodeBE32(frame, data_size);
    }
    
    // Frame flags
    frame.push_back(0);
    frame.push_back(0);
    
    // Encoding (0 = ISO-8859-1)
    frame.push_back(0);
    
    // MIME type (null-terminated)
    for (char c : mime_type) {
        frame.push_back(static_cast<uint8_t>(c));
    }
    frame.push_back(0);
    
    // Picture type
    frame.push_back(picture_type);
    
    // Description (null-terminated)
    for (char c : description) {
        frame.push_back(static_cast<uint8_t>(c));
    }
    frame.push_back(0);
    
    // Image data
    for (uint8_t b : image_data) {
        frame.push_back(b);
    }
    
    return frame;
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

// Combine header and frames into complete tag
std::vector<uint8_t> buildTag(uint8_t major_version, uint8_t flags,
                               const std::vector<std::vector<uint8_t>>& frames) {
    // Calculate total frame size
    size_t frame_size = 0;
    for (const auto& frame : frames) {
        frame_size += frame.size();
    }
    
    // Create header
    auto tag = createHeader(major_version, flags, static_cast<uint32_t>(frame_size));
    
    // Append frames
    for (const auto& frame : frames) {
        tag.insert(tag.end(), frame.begin(), frame.end());
    }
    
    return tag;
}

int main() {
    const std::string corpus_dir = "tests/data/fuzz_corpus/id3v2/";
    
    // ========================================================================
    // VALID TAGS
    // ========================================================================
    
    // 1. Valid ID3v2.3 tag with basic frames
    {
        std::vector<std::vector<uint8_t>> frames = {
            createTextFrame("TIT2", "Test Title"),
            createTextFrame("TPE1", "Test Artist"),
            createTextFrame("TALB", "Test Album"),
            createTextFrame("TYER", "2025"),
            createTextFrame("TRCK", "5/12"),
            createTextFrame("TCON", "Rock")
        };
        auto tag = buildTag(3, 0, frames);
        writeFile(corpus_dir + "valid_v23_basic.bin", tag);
    }
    
    // 2. Valid ID3v2.4 tag with synchsafe frame sizes
    {
        std::vector<std::vector<uint8_t>> frames = {
            createTextFrame("TIT2", "Test Title", 0, true),
            createTextFrame("TPE1", "Test Artist", 0, true),
            createTextFrame("TALB", "Test Album", 0, true),
            createTextFrame("TDRC", "2025-06-15", 0, true)
        };
        auto tag = buildTag(4, 0, frames);
        writeFile(corpus_dir + "valid_v24_basic.bin", tag);
    }
    
    // 3. Valid ID3v2.2 tag with 3-byte frame IDs
    {
        std::vector<std::vector<uint8_t>> frames = {
            createTextFrameV22("TT2", "Test Title"),
            createTextFrameV22("TP1", "Test Artist"),
            createTextFrameV22("TAL", "Test Album"),
            createTextFrameV22("TYE", "2025")
        };
        
        size_t frame_size = 0;
        for (const auto& f : frames) frame_size += f.size();
        auto tag = createHeader(2, 0, static_cast<uint32_t>(frame_size));
        for (const auto& f : frames) tag.insert(tag.end(), f.begin(), f.end());
        writeFile(corpus_dir + "valid_v22_basic.bin", tag);
    }
    
    // 4. Valid tag with UTF-16 text
    {
        std::vector<uint8_t> utf16_frame;
        utf16_frame.push_back('T'); utf16_frame.push_back('I');
        utf16_frame.push_back('T'); utf16_frame.push_back('2');
        // Size: encoding(1) + BOM(2) + "Test"(8) + null(2) = 13
        encodeBE32(utf16_frame, 13);
        utf16_frame.push_back(0); utf16_frame.push_back(0); // flags
        utf16_frame.push_back(1); // UTF-16 with BOM
        utf16_frame.push_back(0xFF); utf16_frame.push_back(0xFE); // BOM (LE)
        // "Test" in UTF-16LE
        utf16_frame.push_back('T'); utf16_frame.push_back(0);
        utf16_frame.push_back('e'); utf16_frame.push_back(0);
        utf16_frame.push_back('s'); utf16_frame.push_back(0);
        utf16_frame.push_back('t'); utf16_frame.push_back(0);
        utf16_frame.push_back(0); utf16_frame.push_back(0); // null terminator
        
        auto tag = createHeader(3, 0, static_cast<uint32_t>(utf16_frame.size()));
        tag.insert(tag.end(), utf16_frame.begin(), utf16_frame.end());
        writeFile(corpus_dir + "valid_utf16.bin", tag);
    }
    
    // 5. Valid tag with APIC frame
    {
        std::vector<uint8_t> fake_jpeg = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10,
                                           'J', 'F', 'I', 'F', 0x00};
        auto apic = createAPICFrame("image/jpeg", 3, "Front Cover", fake_jpeg);
        auto title = createTextFrame("TIT2", "With Picture");
        
        std::vector<std::vector<uint8_t>> frames = {title, apic};
        auto tag = buildTag(3, 0, frames);
        writeFile(corpus_dir + "valid_with_apic.bin", tag);
    }
    
    // ========================================================================
    // HEADER MUTATIONS
    // ========================================================================
    
    // 6. Invalid version (0x05)
    {
        auto tag = createHeader(5, 0, 0);
        writeFile(corpus_dir + "mutate_invalid_version.bin", tag);
    }
    
    // 7. Invalid version (0xFF)
    {
        auto tag = createHeader(0xFF, 0, 0);
        writeFile(corpus_dir + "mutate_version_ff.bin", tag);
    }
    
    // 8. v2.4 header with v2.2 frame IDs
    {
        auto frames_v22 = createTextFrameV22("TT2", "Mixed Version");
        auto tag = createHeader(4, 0, static_cast<uint32_t>(frames_v22.size()));
        tag.insert(tag.end(), frames_v22.begin(), frames_v22.end());
        writeFile(corpus_dir + "mutate_v24_with_v22_frames.bin", tag);
    }
    
    // 9. Synchsafe size with high bit set (invalid)
    {
        std::vector<uint8_t> tag = {'I', 'D', '3', 3, 0, 0};
        tag.push_back(0x80); // Invalid: high bit set
        tag.push_back(0x00);
        tag.push_back(0x00);
        tag.push_back(0x10);
        writeFile(corpus_dir + "mutate_invalid_synchsafe.bin", tag);
    }
    
    // 10. Size claiming more data than available
    {
        auto tag = createHeader(3, 0, 1000000); // Claim 1MB
        // But provide no frames
        writeFile(corpus_dir + "mutate_size_overflow.bin", tag);
    }
    
    // 11. Size of 0 with frames following
    {
        auto frame = createTextFrame("TIT2", "Zero Size Tag");
        auto tag = createHeader(3, 0, 0); // Size = 0
        tag.insert(tag.end(), frame.begin(), frame.end());
        writeFile(corpus_dir + "mutate_zero_size_with_frames.bin", tag);
    }
    
    // 12. Extended header flag set but no extended header
    {
        auto frame = createTextFrame("TIT2", "Missing Extended Header");
        auto tag = createHeader(3, 0x40, static_cast<uint32_t>(frame.size())); // 0x40 = extended header
        tag.insert(tag.end(), frame.begin(), frame.end());
        writeFile(corpus_dir + "mutate_missing_extended_header.bin", tag);
    }

    
    // ========================================================================
    // FRAME SIZE ATTACKS
    // ========================================================================
    
    // 13. Frame size larger than remaining tag data
    {
        std::vector<uint8_t> frame;
        frame.push_back('T'); frame.push_back('I');
        frame.push_back('T'); frame.push_back('2');
        encodeBE32(frame, 10000); // Claim 10KB
        frame.push_back(0); frame.push_back(0);
        frame.push_back(0); // encoding
        frame.push_back('X'); // Only 1 byte of data
        
        auto tag = createHeader(3, 0, static_cast<uint32_t>(frame.size()));
        tag.insert(tag.end(), frame.begin(), frame.end());
        writeFile(corpus_dir + "mutate_frame_size_overflow.bin", tag);
    }
    
    // 14. Frame size of 0xFFFFFFFF
    {
        std::vector<uint8_t> frame;
        frame.push_back('T'); frame.push_back('I');
        frame.push_back('T'); frame.push_back('2');
        frame.push_back(0xFF); frame.push_back(0xFF);
        frame.push_back(0xFF); frame.push_back(0xFF);
        frame.push_back(0); frame.push_back(0);
        
        auto tag = createHeader(3, 0, static_cast<uint32_t>(frame.size()));
        tag.insert(tag.end(), frame.begin(), frame.end());
        writeFile(corpus_dir + "mutate_frame_size_max.bin", tag);
    }
    
    // 15. v2.4 non-synchsafe frame size (should be synchsafe)
    {
        std::vector<uint8_t> frame;
        frame.push_back('T'); frame.push_back('I');
        frame.push_back('T'); frame.push_back('2');
        // Non-synchsafe size in v2.4 tag
        frame.push_back(0x00); frame.push_back(0x00);
        frame.push_back(0x00); frame.push_back(0x85); // 0x85 has high bit set
        frame.push_back(0); frame.push_back(0);
        frame.push_back(0);
        for (int i = 0; i < 5; ++i) frame.push_back('X');
        
        auto tag = createHeader(4, 0, static_cast<uint32_t>(frame.size()));
        tag.insert(tag.end(), frame.begin(), frame.end());
        writeFile(corpus_dir + "mutate_v24_nonsynchsafe_frame.bin", tag);
    }
    
    // ========================================================================
    // TEXT FRAME EXPLOITS
    // ========================================================================
    
    // 16. Text encoding byte > 3 (invalid)
    {
        std::vector<uint8_t> frame;
        frame.push_back('T'); frame.push_back('I');
        frame.push_back('T'); frame.push_back('2');
        encodeBE32(frame, 6);
        frame.push_back(0); frame.push_back(0);
        frame.push_back(0xFF); // Invalid encoding
        frame.push_back('T'); frame.push_back('e');
        frame.push_back('s'); frame.push_back('t');
        frame.push_back(0);
        
        auto tag = createHeader(3, 0, static_cast<uint32_t>(frame.size()));
        tag.insert(tag.end(), frame.begin(), frame.end());
        writeFile(corpus_dir + "mutate_invalid_encoding.bin", tag);
    }
    
    // 17. UTF-16 without BOM
    {
        std::vector<uint8_t> frame;
        frame.push_back('T'); frame.push_back('I');
        frame.push_back('T'); frame.push_back('2');
        encodeBE32(frame, 9);
        frame.push_back(0); frame.push_back(0);
        frame.push_back(1); // UTF-16 with BOM
        // No BOM, just raw UTF-16LE
        frame.push_back('T'); frame.push_back(0);
        frame.push_back('e'); frame.push_back(0);
        frame.push_back('s'); frame.push_back(0);
        frame.push_back('t'); frame.push_back(0);
        
        auto tag = createHeader(3, 0, static_cast<uint32_t>(frame.size()));
        tag.insert(tag.end(), frame.begin(), frame.end());
        writeFile(corpus_dir + "mutate_utf16_no_bom.bin", tag);
    }
    
    // 18. UTF-16 with odd byte count
    {
        std::vector<uint8_t> frame;
        frame.push_back('T'); frame.push_back('I');
        frame.push_back('T'); frame.push_back('2');
        encodeBE32(frame, 6);
        frame.push_back(0); frame.push_back(0);
        frame.push_back(1); // UTF-16
        frame.push_back(0xFF); frame.push_back(0xFE); // BOM
        frame.push_back('T'); frame.push_back(0);
        frame.push_back('X'); // Odd byte - incomplete code unit
        
        auto tag = createHeader(3, 0, static_cast<uint32_t>(frame.size()));
        tag.insert(tag.end(), frame.begin(), frame.end());
        writeFile(corpus_dir + "mutate_utf16_odd_bytes.bin", tag);
    }
    
    // 19. UTF-16BE claimed but LE BOM present
    {
        std::vector<uint8_t> frame;
        frame.push_back('T'); frame.push_back('I');
        frame.push_back('T'); frame.push_back('2');
        encodeBE32(frame, 11);
        frame.push_back(0); frame.push_back(0);
        frame.push_back(2); // UTF-16BE (no BOM expected)
        frame.push_back(0xFF); frame.push_back(0xFE); // LE BOM (wrong!)
        frame.push_back(0); frame.push_back('T');
        frame.push_back(0); frame.push_back('e');
        frame.push_back(0); frame.push_back('s');
        frame.push_back(0); frame.push_back('t');
        
        auto tag = createHeader(3, 0, static_cast<uint32_t>(frame.size()));
        tag.insert(tag.end(), frame.begin(), frame.end());
        writeFile(corpus_dir + "mutate_utf16be_with_le_bom.bin", tag);
    }
    
    // 20. Text frame with encoding byte but no text
    {
        std::vector<uint8_t> frame;
        frame.push_back('T'); frame.push_back('I');
        frame.push_back('T'); frame.push_back('2');
        encodeBE32(frame, 1); // Just encoding byte
        frame.push_back(0); frame.push_back(0);
        frame.push_back(0); // ISO-8859-1
        // No text data
        
        auto tag = createHeader(3, 0, static_cast<uint32_t>(frame.size()));
        tag.insert(tag.end(), frame.begin(), frame.end());
        writeFile(corpus_dir + "mutate_empty_text_frame.bin", tag);
    }

    
    // ========================================================================
    // APIC FRAME ATTACKS
    // ========================================================================
    
    // 21. APIC with MIME type not null-terminated
    {
        std::vector<uint8_t> frame;
        frame.push_back('A'); frame.push_back('P');
        frame.push_back('I'); frame.push_back('C');
        encodeBE32(frame, 20);
        frame.push_back(0); frame.push_back(0);
        frame.push_back(0); // encoding
        // MIME type without null terminator
        frame.push_back('i'); frame.push_back('m');
        frame.push_back('a'); frame.push_back('g');
        frame.push_back('e'); frame.push_back('/');
        frame.push_back('j'); frame.push_back('p');
        frame.push_back('e'); frame.push_back('g');
        // No null - goes straight to picture type
        frame.push_back(3); // Front cover
        frame.push_back(0); // Empty description
        frame.push_back(0xFF); frame.push_back(0xD8); // JPEG magic
        
        auto tag = createHeader(3, 0, static_cast<uint32_t>(frame.size()));
        tag.insert(tag.end(), frame.begin(), frame.end());
        writeFile(corpus_dir + "mutate_apic_no_mime_null.bin", tag);
    }
    
    // 22. APIC with picture type > 20 (invalid)
    {
        std::vector<uint8_t> fake_img = {0xFF, 0xD8, 0xFF, 0xE0};
        auto frame = createAPICFrame("image/jpeg", 0xFF, "Invalid Type", fake_img);
        auto tag = createHeader(3, 0, static_cast<uint32_t>(frame.size()));
        tag.insert(tag.end(), frame.begin(), frame.end());
        writeFile(corpus_dir + "mutate_apic_invalid_type.bin", tag);
    }
    
    // 23. APIC with zero-length image data
    {
        std::vector<uint8_t> empty_img;
        auto frame = createAPICFrame("image/jpeg", 3, "Empty Image", empty_img);
        auto tag = createHeader(3, 0, static_cast<uint32_t>(frame.size()));
        tag.insert(tag.end(), frame.begin(), frame.end());
        writeFile(corpus_dir + "mutate_apic_empty_image.bin", tag);
    }
    
    // 24. APIC with URL link (-->)
    {
        std::vector<uint8_t> frame;
        frame.push_back('A'); frame.push_back('P');
        frame.push_back('I'); frame.push_back('C');
        encodeBE32(frame, 30);
        frame.push_back(0); frame.push_back(0);
        frame.push_back(0); // encoding
        // MIME type as URL indicator
        frame.push_back('-'); frame.push_back('-');
        frame.push_back('>'); frame.push_back(0);
        frame.push_back(3); // Front cover
        frame.push_back(0); // Empty description
        // URL instead of image data
        const char* url = "http://example.com/img.jpg";
        for (const char* p = url; *p; ++p) frame.push_back(*p);
        
        auto tag = createHeader(3, 0, static_cast<uint32_t>(frame.size()));
        tag.insert(tag.end(), frame.begin(), frame.end());
        writeFile(corpus_dir + "mutate_apic_url_link.bin", tag);
    }
    
    // ========================================================================
    // UNSYNC EDGE CASES
    // ========================================================================
    
    // 25. Unsync flag set but no 0xFF bytes
    {
        auto frame = createTextFrame("TIT2", "No FF Bytes");
        auto tag = createHeader(3, 0x80, static_cast<uint32_t>(frame.size())); // 0x80 = unsync
        tag.insert(tag.end(), frame.begin(), frame.end());
        writeFile(corpus_dir + "mutate_unsync_no_ff.bin", tag);
    }
    
    // 26. 0xFF at end of data (no following byte)
    {
        std::vector<uint8_t> frame;
        frame.push_back('T'); frame.push_back('I');
        frame.push_back('T'); frame.push_back('2');
        encodeBE32(frame, 5);
        frame.push_back(0); frame.push_back(0);
        frame.push_back(0);
        frame.push_back('T'); frame.push_back('e');
        frame.push_back('s'); frame.push_back(0xFF); // 0xFF at end
        
        auto tag = createHeader(3, 0x80, static_cast<uint32_t>(frame.size()));
        tag.insert(tag.end(), frame.begin(), frame.end());
        writeFile(corpus_dir + "mutate_unsync_ff_at_end.bin", tag);
    }
    
    // 27. 0xFF 0xFF sequence
    {
        std::vector<uint8_t> frame;
        frame.push_back('T'); frame.push_back('I');
        frame.push_back('T'); frame.push_back('2');
        encodeBE32(frame, 6);
        frame.push_back(0); frame.push_back(0);
        frame.push_back(0);
        frame.push_back('A');
        frame.push_back(0xFF); frame.push_back(0xFF); // Double 0xFF
        frame.push_back('B');
        frame.push_back(0);
        
        auto tag = createHeader(3, 0x80, static_cast<uint32_t>(frame.size()));
        tag.insert(tag.end(), frame.begin(), frame.end());
        writeFile(corpus_dir + "mutate_unsync_double_ff.bin", tag);
    }
    
    // ========================================================================
    // FRAME ID ATTACKS
    // ========================================================================
    
    // 28. Frame ID with null bytes
    {
        std::vector<uint8_t> frame;
        frame.push_back('T'); frame.push_back('I');
        frame.push_back('T'); frame.push_back(0); // Null in ID
        encodeBE32(frame, 5);
        frame.push_back(0); frame.push_back(0);
        frame.push_back(0);
        frame.push_back('T'); frame.push_back('e');
        frame.push_back('s'); frame.push_back('t');
        
        auto tag = createHeader(3, 0, static_cast<uint32_t>(frame.size()));
        tag.insert(tag.end(), frame.begin(), frame.end());
        writeFile(corpus_dir + "mutate_frame_id_null.bin", tag);
    }
    
    // 29. Frame ID with non-ASCII
    {
        std::vector<uint8_t> frame;
        frame.push_back(0xFF); frame.push_back(0xFE);
        frame.push_back(0xFD); frame.push_back(0xFC);
        encodeBE32(frame, 5);
        frame.push_back(0); frame.push_back(0);
        frame.push_back(0);
        frame.push_back('T'); frame.push_back('e');
        frame.push_back('s'); frame.push_back('t');
        
        auto tag = createHeader(3, 0, static_cast<uint32_t>(frame.size()));
        tag.insert(tag.end(), frame.begin(), frame.end());
        writeFile(corpus_dir + "mutate_frame_id_nonascii.bin", tag);
    }
    
    // 30. Unknown but valid format frame ID
    {
        auto frame = createTextFrame("XXXX", "Unknown Frame");
        auto tag = createHeader(3, 0, static_cast<uint32_t>(frame.size()));
        tag.insert(tag.end(), frame.begin(), frame.end());
        writeFile(corpus_dir + "valid_unknown_frame_id.bin", tag);
    }
    
    // ========================================================================
    // BOUNDARY CONDITIONS
    // ========================================================================
    
    // 31. Minimum valid tag (header only)
    {
        auto tag = createHeader(3, 0, 0);
        writeFile(corpus_dir + "edge_header_only.bin", tag);
    }
    
    // 32. Tag with padding (null bytes after frames)
    {
        auto frame = createTextFrame("TIT2", "With Padding");
        size_t padding = 100;
        auto tag = createHeader(3, 0, static_cast<uint32_t>(frame.size() + padding));
        tag.insert(tag.end(), frame.begin(), frame.end());
        for (size_t i = 0; i < padding; ++i) tag.push_back(0);
        writeFile(corpus_dir + "valid_with_padding.bin", tag);
    }
    
    // 33. Many small frames
    {
        std::vector<std::vector<uint8_t>> frames;
        for (int i = 0; i < 50; ++i) {
            frames.push_back(createTextFrame("TXXX", "Value" + std::to_string(i)));
        }
        auto tag = buildTag(3, 0, frames);
        writeFile(corpus_dir + "edge_many_frames.bin", tag);
    }
    
    // 34. Truncated header
    {
        std::vector<uint8_t> tag = {'I', 'D', '3', 3, 0}; // Only 5 bytes
        writeFile(corpus_dir + "malformed_truncated_header.bin", tag);
    }
    
    // 35. Wrong magic bytes
    {
        std::vector<uint8_t> tag = {'X', 'Y', 'Z', 3, 0, 0, 0, 0, 0, 0};
        writeFile(corpus_dir + "malformed_wrong_magic.bin", tag);
    }
    
    std::cout << "\nID3v2 seed corpus generation complete.\n";
    return 0;
}

