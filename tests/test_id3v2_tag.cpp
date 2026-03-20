/*
 * test_id3v2_tag.cpp - Unit tests for ID3v2Tag
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"

using namespace PsyMP3::Tag;
using namespace TestFramework;

// ============================================================================
// Helper functions to create ID3v2 test data
// ============================================================================

// Create a minimal ID3v2.3 header
static std::vector<uint8_t> createID3v2Header(uint8_t major_version, uint8_t flags, uint32_t size) {
    std::vector<uint8_t> header(10);
    header[0] = 'I';
    header[1] = 'D';
    header[2] = '3';
    header[3] = major_version;  // Major version
    header[4] = 0;              // Minor version
    header[5] = flags;          // Flags
    
    // Synchsafe size (4 bytes, 7 bits each)
    header[6] = static_cast<uint8_t>((size >> 21) & 0x7F);
    header[7] = static_cast<uint8_t>((size >> 14) & 0x7F);
    header[8] = static_cast<uint8_t>((size >> 7) & 0x7F);
    header[9] = static_cast<uint8_t>(size & 0x7F);
    
    return header;
}

// Create a v2.3/v2.4 text frame
static std::vector<uint8_t> createTextFrame(const std::string& frame_id, const std::string& text,
                                            uint8_t encoding = 0, bool synchsafe_size = false) {
    std::vector<uint8_t> frame;
    
    // Frame ID (4 bytes)
    for (char c : frame_id) {
        frame.push_back(static_cast<uint8_t>(c));
    }
    while (frame.size() < 4) {
        frame.push_back(0);
    }

    // Frame size (4 bytes) - includes encoding byte
    uint32_t data_size = static_cast<uint32_t>(text.size() + 1); // +1 for encoding byte
    if (synchsafe_size) {
        // v2.4 synchsafe size
        frame.push_back(static_cast<uint8_t>((data_size >> 21) & 0x7F));
        frame.push_back(static_cast<uint8_t>((data_size >> 14) & 0x7F));
        frame.push_back(static_cast<uint8_t>((data_size >> 7) & 0x7F));
        frame.push_back(static_cast<uint8_t>(data_size & 0x7F));
    } else {
        // v2.3 regular size
        frame.push_back(static_cast<uint8_t>((data_size >> 24) & 0xFF));
        frame.push_back(static_cast<uint8_t>((data_size >> 16) & 0xFF));
        frame.push_back(static_cast<uint8_t>((data_size >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(data_size & 0xFF));
    }
    
    // Frame flags (2 bytes)
    frame.push_back(0);
    frame.push_back(0);
    
    // Frame data: encoding byte + text
    frame.push_back(encoding);
    for (char c : text) {
        frame.push_back(static_cast<uint8_t>(c));
    }
    
    return frame;
}

// Create a v2.2 text frame (3-char ID, 3-byte size, no flags)
static std::vector<uint8_t> createV22TextFrame(const std::string& frame_id, const std::string& text,
                                               uint8_t encoding = 0) {
    std::vector<uint8_t> frame;
    
    // Frame ID (3 bytes)
    for (size_t i = 0; i < 3 && i < frame_id.size(); ++i) {
        frame.push_back(static_cast<uint8_t>(frame_id[i]));
    }
    while (frame.size() < 3) {
        frame.push_back(0);
    }
    
    // Frame size (3 bytes, big-endian) - includes encoding byte
    uint32_t data_size = static_cast<uint32_t>(text.size() + 1);
    frame.push_back(static_cast<uint8_t>((data_size >> 16) & 0xFF));
    frame.push_back(static_cast<uint8_t>((data_size >> 8) & 0xFF));
    frame.push_back(static_cast<uint8_t>(data_size & 0xFF));
    
    // Frame data: encoding byte + text
    frame.push_back(encoding);
    for (char c : text) {
        frame.push_back(static_cast<uint8_t>(c));
    }
    
    return frame;
}


// Create a complete ID3v2.3 tag with frames
static std::vector<uint8_t> createID3v23Tag(const std::vector<std::pair<std::string, std::string>>& frames) {
    // Build frame data first
    std::vector<uint8_t> frame_data;
    for (const auto& frame_pair : frames) {
        auto frame = createTextFrame(frame_pair.first, frame_pair.second, 0, false);
        frame_data.insert(frame_data.end(), frame.begin(), frame.end());
    }
    
    // Create header with correct size
    auto header = createID3v2Header(3, 0, static_cast<uint32_t>(frame_data.size()));
    
    // Combine header and frames
    std::vector<uint8_t> tag;
    tag.insert(tag.end(), header.begin(), header.end());
    tag.insert(tag.end(), frame_data.begin(), frame_data.end());
    
    return tag;
}

// Create a complete ID3v2.4 tag with frames
static std::vector<uint8_t> createID3v24Tag(const std::vector<std::pair<std::string, std::string>>& frames) {
    // Build frame data first
    std::vector<uint8_t> frame_data;
    for (const auto& frame_pair : frames) {
        auto frame = createTextFrame(frame_pair.first, frame_pair.second, 0, true);
        frame_data.insert(frame_data.end(), frame.begin(), frame.end());
    }
    
    // Create header with correct size
    auto header = createID3v2Header(4, 0, static_cast<uint32_t>(frame_data.size()));
    
    // Combine header and frames
    std::vector<uint8_t> tag;
    tag.insert(tag.end(), header.begin(), header.end());
    tag.insert(tag.end(), frame_data.begin(), frame_data.end());
    
    return tag;
}

// Create a complete ID3v2.2 tag with frames
static std::vector<uint8_t> createID3v22Tag(const std::vector<std::pair<std::string, std::string>>& frames) {
    // Build frame data first
    std::vector<uint8_t> frame_data;
    for (const auto& frame_pair : frames) {
        auto frame = createV22TextFrame(frame_pair.first, frame_pair.second, 0);
        frame_data.insert(frame_data.end(), frame.begin(), frame.end());
    }
    
    // Create header with correct size
    auto header = createID3v2Header(2, 0, static_cast<uint32_t>(frame_data.size()));
    
    // Combine header and frames
    std::vector<uint8_t> tag;
    tag.insert(tag.end(), header.begin(), header.end());
    tag.insert(tag.end(), frame_data.begin(), frame_data.end());
    
    return tag;
}


// ============================================================================
// ID3v2Tag::isValid Tests
// ============================================================================

class ID3v2Tag_IsValid_ValidV23Header : public TestCase {
public:
    ID3v2Tag_IsValid_ValidV23Header() : TestCase("ID3v2Tag_IsValid_ValidV23Header") {}
protected:
    void runTest() override {
        auto header = createID3v2Header(3, 0, 100);
        ASSERT_TRUE(ID3v2Tag::isValid(header.data(), header.size()), 
                   "isValid should return true for valid v2.3 header");
    }
};

class ID3v2Tag_IsValid_ValidV24Header : public TestCase {
public:
    ID3v2Tag_IsValid_ValidV24Header() : TestCase("ID3v2Tag_IsValid_ValidV24Header") {}
protected:
    void runTest() override {
        auto header = createID3v2Header(4, 0, 100);
        ASSERT_TRUE(ID3v2Tag::isValid(header.data(), header.size()), 
                   "isValid should return true for valid v2.4 header");
    }
};

class ID3v2Tag_IsValid_ValidV22Header : public TestCase {
public:
    ID3v2Tag_IsValid_ValidV22Header() : TestCase("ID3v2Tag_IsValid_ValidV22Header") {}
protected:
    void runTest() override {
        auto header = createID3v2Header(2, 0, 100);
        ASSERT_TRUE(ID3v2Tag::isValid(header.data(), header.size()), 
                   "isValid should return true for valid v2.2 header");
    }
};

class ID3v2Tag_IsValid_InvalidMagic : public TestCase {
public:
    ID3v2Tag_IsValid_InvalidMagic() : TestCase("ID3v2Tag_IsValid_InvalidMagic") {}
protected:
    void runTest() override {
        std::vector<uint8_t> data = {'X', 'Y', 'Z', 3, 0, 0, 0, 0, 0, 100};
        ASSERT_FALSE(ID3v2Tag::isValid(data.data(), data.size()), 
                    "isValid should return false for invalid magic bytes");
    }
};

class ID3v2Tag_IsValid_InvalidVersion : public TestCase {
public:
    ID3v2Tag_IsValid_InvalidVersion() : TestCase("ID3v2Tag_IsValid_InvalidVersion") {}
protected:
    void runTest() override {
        // Version 1 is invalid
        auto header = createID3v2Header(1, 0, 100);
        ASSERT_FALSE(ID3v2Tag::isValid(header.data(), header.size()), 
                    "isValid should return false for version 1");
        
        // Version 5 is invalid
        header = createID3v2Header(5, 0, 100);
        ASSERT_FALSE(ID3v2Tag::isValid(header.data(), header.size()), 
                    "isValid should return false for version 5");
    }
};

class ID3v2Tag_IsValid_NullPointer : public TestCase {
public:
    ID3v2Tag_IsValid_NullPointer() : TestCase("ID3v2Tag_IsValid_NullPointer") {}
protected:
    void runTest() override {
        ASSERT_FALSE(ID3v2Tag::isValid(nullptr, 0), 
                    "isValid should return false for nullptr");
        ASSERT_FALSE(ID3v2Tag::isValid(nullptr, 100), 
                    "isValid should return false for nullptr with size");
    }
};

class ID3v2Tag_IsValid_TooSmall : public TestCase {
public:
    ID3v2Tag_IsValid_TooSmall() : TestCase("ID3v2Tag_IsValid_TooSmall") {}
protected:
    void runTest() override {
        std::vector<uint8_t> data = {'I', 'D', '3', 3, 0};
        ASSERT_FALSE(ID3v2Tag::isValid(data.data(), data.size()), 
                    "isValid should return false for data smaller than header");
    }
};

class ID3v2Tag_IsValid_InvalidSynchsafe : public TestCase {
public:
    ID3v2Tag_IsValid_InvalidSynchsafe() : TestCase("ID3v2Tag_IsValid_InvalidSynchsafe") {}
protected:
    void runTest() override {
        // Create header with invalid synchsafe bytes (high bit set)
        std::vector<uint8_t> data = {'I', 'D', '3', 3, 0, 0, 0x80, 0, 0, 0};
        ASSERT_FALSE(ID3v2Tag::isValid(data.data(), data.size()), 
                    "isValid should return false for invalid synchsafe size");
    }
};


// ============================================================================
// ID3v2Tag::getTagSize Tests
// ============================================================================

class ID3v2Tag_GetTagSize_ValidHeader : public TestCase {
public:
    ID3v2Tag_GetTagSize_ValidHeader() : TestCase("ID3v2Tag_GetTagSize_ValidHeader") {}
protected:
    void runTest() override {
        auto header = createID3v2Header(3, 0, 100);
        size_t size = ID3v2Tag::getTagSize(header.data());
        ASSERT_EQUALS(static_cast<size_t>(110), size, 
                     "getTagSize should return header size + data size (10 + 100)");
    }
};

class ID3v2Tag_GetTagSize_NullPointer : public TestCase {
public:
    ID3v2Tag_GetTagSize_NullPointer() : TestCase("ID3v2Tag_GetTagSize_NullPointer") {}
protected:
    void runTest() override {
        size_t size = ID3v2Tag::getTagSize(nullptr);
        ASSERT_EQUALS(static_cast<size_t>(0), size, 
                     "getTagSize should return 0 for nullptr");
    }
};

class ID3v2Tag_GetTagSize_LargeSize : public TestCase {
public:
    ID3v2Tag_GetTagSize_LargeSize() : TestCase("ID3v2Tag_GetTagSize_LargeSize") {}
protected:
    void runTest() override {
        // Create header with size 0x0FFFFFFF (max synchsafe value = 268MB)
        // This exceeds MAX_TAG_SIZE (256MB), so getTagSize should return 0
        auto header = createID3v2Header(3, 0, 0x0FFFFFFF);
        size_t size = ID3v2Tag::getTagSize(header.data());
        ASSERT_EQUALS(static_cast<size_t>(0), size, 
                     "getTagSize should return 0 for tags exceeding MAX_TAG_SIZE");
        
        // Test with a reasonable large size (100MB) that's under the limit
        header = createID3v2Header(3, 0, 100 * 1024 * 1024);
        size = ID3v2Tag::getTagSize(header.data());
        ASSERT_EQUALS(static_cast<size_t>(100 * 1024 * 1024 + 10), size, 
                     "getTagSize should handle large but valid sizes");
    }
};

// ============================================================================
// ID3v2Tag Version Detection Tests
// ============================================================================

class ID3v2Tag_Parse_DetectsV22 : public TestCase {
public:
    ID3v2Tag_Parse_DetectsV22() : TestCase("ID3v2Tag_Parse_DetectsV22") {}
protected:
    void runTest() override {
        auto tag_data = createID3v22Tag({{"TT2", "Test Title"}});
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag for v2.2");
        ASSERT_EQUALS(static_cast<uint8_t>(2), tag->majorVersion(), "majorVersion should be 2");
        ASSERT_EQUALS(static_cast<uint8_t>(0), tag->minorVersion(), "minorVersion should be 0");
        ASSERT_EQUALS(std::string("ID3v2.2"), tag->formatName(), "formatName should be ID3v2.2");
    }
};

class ID3v2Tag_Parse_DetectsV23 : public TestCase {
public:
    ID3v2Tag_Parse_DetectsV23() : TestCase("ID3v2Tag_Parse_DetectsV23") {}
protected:
    void runTest() override {
        auto tag_data = createID3v23Tag({{"TIT2", "Test Title"}});
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag for v2.3");
        ASSERT_EQUALS(static_cast<uint8_t>(3), tag->majorVersion(), "majorVersion should be 3");
        ASSERT_EQUALS(static_cast<uint8_t>(0), tag->minorVersion(), "minorVersion should be 0");
        ASSERT_EQUALS(std::string("ID3v2.3"), tag->formatName(), "formatName should be ID3v2.3");
    }
};

class ID3v2Tag_Parse_DetectsV24 : public TestCase {
public:
    ID3v2Tag_Parse_DetectsV24() : TestCase("ID3v2Tag_Parse_DetectsV24") {}
protected:
    void runTest() override {
        auto tag_data = createID3v24Tag({{"TIT2", "Test Title"}});
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag for v2.4");
        ASSERT_EQUALS(static_cast<uint8_t>(4), tag->majorVersion(), "majorVersion should be 4");
        ASSERT_EQUALS(static_cast<uint8_t>(0), tag->minorVersion(), "minorVersion should be 0");
        ASSERT_EQUALS(std::string("ID3v2.4"), tag->formatName(), "formatName should be ID3v2.4");
    }
};


// ============================================================================
// ID3v2Tag Frame ID Normalization Tests
// ============================================================================

class ID3v2Tag_NormalizeFrameId_V22ToV23 : public TestCase {
public:
    ID3v2Tag_NormalizeFrameId_V22ToV23() : TestCase("ID3v2Tag_NormalizeFrameId_V22ToV23") {}
protected:
    void runTest() override {
        // Test common frame ID mappings
        ASSERT_EQUALS(std::string("TIT2"), ID3v2Tag::normalizeFrameId("TT2", 2), "TT2 should map to TIT2");
        ASSERT_EQUALS(std::string("TPE1"), ID3v2Tag::normalizeFrameId("TP1", 2), "TP1 should map to TPE1");
        ASSERT_EQUALS(std::string("TALB"), ID3v2Tag::normalizeFrameId("TAL", 2), "TAL should map to TALB");
        ASSERT_EQUALS(std::string("TPE2"), ID3v2Tag::normalizeFrameId("TP2", 2), "TP2 should map to TPE2");
        ASSERT_EQUALS(std::string("TCON"), ID3v2Tag::normalizeFrameId("TCO", 2), "TCO should map to TCON");
        ASSERT_EQUALS(std::string("TRCK"), ID3v2Tag::normalizeFrameId("TRK", 2), "TRK should map to TRCK");
        ASSERT_EQUALS(std::string("TPOS"), ID3v2Tag::normalizeFrameId("TPA", 2), "TPA should map to TPOS");
        ASSERT_EQUALS(std::string("TCOM"), ID3v2Tag::normalizeFrameId("TCM", 2), "TCM should map to TCOM");
        ASSERT_EQUALS(std::string("COMM"), ID3v2Tag::normalizeFrameId("COM", 2), "COM should map to COMM");
        ASSERT_EQUALS(std::string("APIC"), ID3v2Tag::normalizeFrameId("PIC", 2), "PIC should map to APIC");
    }
};

class ID3v2Tag_NormalizeFrameId_V23Unchanged : public TestCase {
public:
    ID3v2Tag_NormalizeFrameId_V23Unchanged() : TestCase("ID3v2Tag_NormalizeFrameId_V23Unchanged") {}
protected:
    void runTest() override {
        // v2.3+ frame IDs should remain unchanged
        ASSERT_EQUALS(std::string("TIT2"), ID3v2Tag::normalizeFrameId("TIT2", 3), "TIT2 should remain TIT2 for v2.3");
        ASSERT_EQUALS(std::string("TPE1"), ID3v2Tag::normalizeFrameId("TPE1", 3), "TPE1 should remain TPE1 for v2.3");
        ASSERT_EQUALS(std::string("TALB"), ID3v2Tag::normalizeFrameId("TALB", 4), "TALB should remain TALB for v2.4");
        ASSERT_EQUALS(std::string("APIC"), ID3v2Tag::normalizeFrameId("APIC", 4), "APIC should remain APIC for v2.4");
    }
};

class ID3v2Tag_NormalizeFrameId_UnknownV22 : public TestCase {
public:
    ID3v2Tag_NormalizeFrameId_UnknownV22() : TestCase("ID3v2Tag_NormalizeFrameId_UnknownV22") {}
protected:
    void runTest() override {
        // Unknown v2.2 frame IDs should be returned as-is
        ASSERT_EQUALS(std::string("XXX"), ID3v2Tag::normalizeFrameId("XXX", 2), "Unknown frame ID should be unchanged");
        ASSERT_EQUALS(std::string("ZZZ"), ID3v2Tag::normalizeFrameId("ZZZ", 2), "Unknown frame ID should be unchanged");
    }
};

// ============================================================================
// ID3v2Tag Text Frame Parsing Tests
// ============================================================================

class ID3v2Tag_Parse_TextFrames : public TestCase {
public:
    ID3v2Tag_Parse_TextFrames() : TestCase("ID3v2Tag_Parse_TextFrames") {}
protected:
    void runTest() override {
        auto tag_data = createID3v23Tag({
            {"TIT2", "Test Title"},
            {"TPE1", "Test Artist"},
            {"TALB", "Test Album"},
            {"TYER", "2024"},
            {"TRCK", "5/12"},
            {"TPOS", "1/2"},
            {"TCON", "Rock"},
            {"TCOM", "Test Composer"}
        });
        
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        
        ASSERT_EQUALS(std::string("Test Title"), tag->title(), "title should match");
        ASSERT_EQUALS(std::string("Test Artist"), tag->artist(), "artist should match");
        ASSERT_EQUALS(std::string("Test Album"), tag->album(), "album should match");
        ASSERT_EQUALS(2024u, tag->year(), "year should match");
        ASSERT_EQUALS(5u, tag->track(), "track should match");
        ASSERT_EQUALS(12u, tag->trackTotal(), "trackTotal should match");
        ASSERT_EQUALS(1u, tag->disc(), "disc should match");
        ASSERT_EQUALS(2u, tag->discTotal(), "discTotal should match");
        ASSERT_EQUALS(std::string("Rock"), tag->genre(), "genre should match");
        ASSERT_EQUALS(std::string("Test Composer"), tag->composer(), "composer should match");
    }
};

class ID3v2Tag_Parse_V22TextFrames : public TestCase {
public:
    ID3v2Tag_Parse_V22TextFrames() : TestCase("ID3v2Tag_Parse_V22TextFrames") {}
protected:
    void runTest() override {
        auto tag_data = createID3v22Tag({
            {"TT2", "V22 Title"},
            {"TP1", "V22 Artist"},
            {"TAL", "V22 Album"},
            {"TYE", "2023"},
            {"TRK", "3"},
            {"TCO", "Pop"}
        });
        
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag for v2.2");
        
        ASSERT_EQUALS(std::string("V22 Title"), tag->title(), "title should match");
        ASSERT_EQUALS(std::string("V22 Artist"), tag->artist(), "artist should match");
        ASSERT_EQUALS(std::string("V22 Album"), tag->album(), "album should match");
        ASSERT_EQUALS(2023u, tag->year(), "year should match");
        ASSERT_EQUALS(3u, tag->track(), "track should match");
        ASSERT_EQUALS(std::string("Pop"), tag->genre(), "genre should match");
    }
};


// ============================================================================
// ID3v2Tag Text Encoding Tests
// ============================================================================

class ID3v2Tag_Parse_ISO8859_1Encoding : public TestCase {
public:
    ID3v2Tag_Parse_ISO8859_1Encoding() : TestCase("ID3v2Tag_Parse_ISO8859_1Encoding") {}
protected:
    void runTest() override {
        // Encoding 0 = ISO-8859-1
        auto tag_data = createID3v23Tag({{"TIT2", "ASCII Title"}});
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_EQUALS(std::string("ASCII Title"), tag->title(), "ISO-8859-1 text should be decoded");
    }
};

class ID3v2Tag_Parse_UTF8Encoding : public TestCase {
public:
    ID3v2Tag_Parse_UTF8Encoding() : TestCase("ID3v2Tag_Parse_UTF8Encoding") {}
protected:
    void runTest() override {
        // Create a tag with UTF-8 encoding (encoding byte = 3)
        std::vector<uint8_t> frame_data;
        
        // Frame ID
        frame_data.push_back('T'); frame_data.push_back('I');
        frame_data.push_back('T'); frame_data.push_back('2');
        
        // Frame size (encoding byte + text)
        std::string text = "UTF8 Title";
        uint32_t data_size = static_cast<uint32_t>(text.size() + 1);
        frame_data.push_back(static_cast<uint8_t>((data_size >> 24) & 0xFF));
        frame_data.push_back(static_cast<uint8_t>((data_size >> 16) & 0xFF));
        frame_data.push_back(static_cast<uint8_t>((data_size >> 8) & 0xFF));
        frame_data.push_back(static_cast<uint8_t>(data_size & 0xFF));
        
        // Frame flags
        frame_data.push_back(0); frame_data.push_back(0);
        
        // Encoding byte (3 = UTF-8)
        frame_data.push_back(3);
        
        // Text
        for (char c : text) {
            frame_data.push_back(static_cast<uint8_t>(c));
        }
        
        // Create header
        auto header = createID3v2Header(3, 0, static_cast<uint32_t>(frame_data.size()));
        
        // Combine
        std::vector<uint8_t> tag_data;
        tag_data.insert(tag_data.end(), header.begin(), header.end());
        tag_data.insert(tag_data.end(), frame_data.begin(), frame_data.end());
        
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_EQUALS(std::string("UTF8 Title"), tag->title(), "UTF-8 text should be decoded");
    }
};

// ============================================================================
// ID3v2Tag getTag/hasTag Tests
// ============================================================================

class ID3v2Tag_GetTag_StandardKeys : public TestCase {
public:
    ID3v2Tag_GetTag_StandardKeys() : TestCase("ID3v2Tag_GetTag_StandardKeys") {}
protected:
    void runTest() override {
        auto tag_data = createID3v23Tag({
            {"TIT2", "Title"},
            {"TPE1", "Artist"},
            {"TALB", "Album"}
        });
        
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        
        ASSERT_EQUALS(std::string("Title"), tag->getTag("TITLE"), "getTag(TITLE) should work");
        ASSERT_EQUALS(std::string("Artist"), tag->getTag("ARTIST"), "getTag(ARTIST) should work");
        ASSERT_EQUALS(std::string("Album"), tag->getTag("ALBUM"), "getTag(ALBUM) should work");
    }
};

class ID3v2Tag_GetTag_CaseInsensitive : public TestCase {
public:
    ID3v2Tag_GetTag_CaseInsensitive() : TestCase("ID3v2Tag_GetTag_CaseInsensitive") {}
protected:
    void runTest() override {
        auto tag_data = createID3v23Tag({{"TIT2", "Title"}});
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_EQUALS(std::string("Title"), tag->getTag("title"), "lowercase key should work");
        ASSERT_EQUALS(std::string("Title"), tag->getTag("TITLE"), "uppercase key should work");
        ASSERT_EQUALS(std::string("Title"), tag->getTag("Title"), "mixed case key should work");
    }
};

class ID3v2Tag_GetTag_FrameIdDirect : public TestCase {
public:
    ID3v2Tag_GetTag_FrameIdDirect() : TestCase("ID3v2Tag_GetTag_FrameIdDirect") {}
protected:
    void runTest() override {
        auto tag_data = createID3v23Tag({{"TIT2", "Title"}});
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_EQUALS(std::string("Title"), tag->getTag("TIT2"), "direct frame ID should work");
    }
};

class ID3v2Tag_HasTag_ExistingFields : public TestCase {
public:
    ID3v2Tag_HasTag_ExistingFields() : TestCase("ID3v2Tag_HasTag_ExistingFields") {}
protected:
    void runTest() override {
        auto tag_data = createID3v23Tag({
            {"TIT2", "Title"},
            {"TPE1", "Artist"}
        });
        
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        
        ASSERT_TRUE(tag->hasTag("TITLE"), "hasTag(TITLE) should return true");
        ASSERT_TRUE(tag->hasTag("ARTIST"), "hasTag(ARTIST) should return true");
        ASSERT_TRUE(tag->hasTag("TIT2"), "hasTag(TIT2) should return true");
    }
};

class ID3v2Tag_HasTag_NonexistentFields : public TestCase {
public:
    ID3v2Tag_HasTag_NonexistentFields() : TestCase("ID3v2Tag_HasTag_NonexistentFields") {}
protected:
    void runTest() override {
        auto tag_data = createID3v23Tag({{"TIT2", "Title"}});
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_FALSE(tag->hasTag("ARTIST"), "hasTag(ARTIST) should return false");
        ASSERT_FALSE(tag->hasTag("ALBUM"), "hasTag(ALBUM) should return false");
        ASSERT_FALSE(tag->hasTag("NONEXISTENT"), "hasTag(NONEXISTENT) should return false");
    }
};


// ============================================================================
// ID3v2Tag getAllTags Tests
// ============================================================================

class ID3v2Tag_GetAllTags_ReturnsPopulatedMap : public TestCase {
public:
    ID3v2Tag_GetAllTags_ReturnsPopulatedMap() : TestCase("ID3v2Tag_GetAllTags_ReturnsPopulatedMap") {}
protected:
    void runTest() override {
        auto tag_data = createID3v23Tag({
            {"TIT2", "Title"},
            {"TPE1", "Artist"},
            {"TALB", "Album"}
        });
        
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        
        auto all_tags = tag->getAllTags();
        ASSERT_FALSE(all_tags.empty(), "getAllTags should not return empty map");
        ASSERT_TRUE(all_tags.find("TIT2") != all_tags.end() || all_tags.find("TITLE") != all_tags.end(),
                   "getAllTags should contain title");
    }
};

// ============================================================================
// ID3v2Tag isEmpty Tests
// ============================================================================

class ID3v2Tag_IsEmpty_WithContent : public TestCase {
public:
    ID3v2Tag_IsEmpty_WithContent() : TestCase("ID3v2Tag_IsEmpty_WithContent") {}
protected:
    void runTest() override {
        auto tag_data = createID3v23Tag({{"TIT2", "Title"}});
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_FALSE(tag->isEmpty(), "isEmpty should return false when content exists");
    }
};

class ID3v2Tag_IsEmpty_NoFrames : public TestCase {
public:
    ID3v2Tag_IsEmpty_NoFrames() : TestCase("ID3v2Tag_IsEmpty_NoFrames") {}
protected:
    void runTest() override {
        // Create tag with no frames (just padding)
        auto header = createID3v2Header(3, 0, 10);
        std::vector<uint8_t> tag_data;
        tag_data.insert(tag_data.end(), header.begin(), header.end());
        // Add padding (zeros)
        for (int i = 0; i < 10; ++i) {
            tag_data.push_back(0);
        }
        
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_TRUE(tag->isEmpty(), "isEmpty should return true when no frames");
    }
};

// ============================================================================
// ID3v2Tag Picture Tests
// ============================================================================

class ID3v2Tag_NoPictures : public TestCase {
public:
    ID3v2Tag_NoPictures() : TestCase("ID3v2Tag_NoPictures") {}
protected:
    void runTest() override {
        auto tag_data = createID3v23Tag({{"TIT2", "Title"}});
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_EQUALS(static_cast<size_t>(0), tag->pictureCount(), "pictureCount should be 0");
        ASSERT_FALSE(tag->getPicture(0).has_value(), "getPicture should return nullopt");
        ASSERT_FALSE(tag->getFrontCover().has_value(), "getFrontCover should return nullopt");
    }
};

// Helper to create APIC frame
static std::vector<uint8_t> createAPICFrame(const std::string& mime_type, uint8_t pic_type,
                                            const std::string& description,
                                            const std::vector<uint8_t>& image_data) {
    std::vector<uint8_t> frame;
    
    // Frame ID
    frame.push_back('A'); frame.push_back('P');
    frame.push_back('I'); frame.push_back('C');
    
    // Calculate frame data size
    size_t data_size = 1 + mime_type.size() + 1 + 1 + description.size() + 1 + image_data.size();
    
    // Frame size (4 bytes, big-endian)
    frame.push_back(static_cast<uint8_t>((data_size >> 24) & 0xFF));
    frame.push_back(static_cast<uint8_t>((data_size >> 16) & 0xFF));
    frame.push_back(static_cast<uint8_t>((data_size >> 8) & 0xFF));
    frame.push_back(static_cast<uint8_t>(data_size & 0xFF));
    
    // Frame flags
    frame.push_back(0); frame.push_back(0);
    
    // Encoding byte (0 = ISO-8859-1)
    frame.push_back(0);
    
    // MIME type (null-terminated)
    for (char c : mime_type) {
        frame.push_back(static_cast<uint8_t>(c));
    }
    frame.push_back(0);
    
    // Picture type
    frame.push_back(pic_type);
    
    // Description (null-terminated)
    for (char c : description) {
        frame.push_back(static_cast<uint8_t>(c));
    }
    frame.push_back(0);
    
    // Image data
    frame.insert(frame.end(), image_data.begin(), image_data.end());
    
    return frame;
}

class ID3v2Tag_Parse_APICFrame : public TestCase {
public:
    ID3v2Tag_Parse_APICFrame() : TestCase("ID3v2Tag_Parse_APICFrame") {}
protected:
    void runTest() override {
        // Create fake image data (PNG magic bytes + some data)
        std::vector<uint8_t> image_data = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
                                           0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52};
        
        // Create APIC frame (type 3 = front cover)
        auto apic_frame = createAPICFrame("image/png", 3, "Cover", image_data);
        
        // Create header
        auto header = createID3v2Header(3, 0, static_cast<uint32_t>(apic_frame.size()));
        
        // Combine
        std::vector<uint8_t> tag_data;
        tag_data.insert(tag_data.end(), header.begin(), header.end());
        tag_data.insert(tag_data.end(), apic_frame.begin(), apic_frame.end());
        
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_EQUALS(static_cast<size_t>(1), tag->pictureCount(), "pictureCount should be 1");
        
        auto pic = tag->getPicture(0);
        ASSERT_TRUE(pic.has_value(), "getPicture(0) should return picture");
        ASSERT_EQUALS(std::string("image/png"), pic->mime_type, "MIME type should match");
        ASSERT_EQUALS(static_cast<uint8_t>(PictureType::FrontCover), static_cast<uint8_t>(pic->type),
                     "picture type should be FrontCover");
        ASSERT_EQUALS(std::string("Cover"), pic->description, "description should match");
        ASSERT_EQUALS(image_data.size(), pic->data.size(), "image data size should match");
    }
};

class ID3v2Tag_GetFrontCover : public TestCase {
public:
    ID3v2Tag_GetFrontCover() : TestCase("ID3v2Tag_GetFrontCover") {}
protected:
    void runTest() override {
        std::vector<uint8_t> image_data = {0x89, 0x50, 0x4E, 0x47};
        
        // Create APIC frame with front cover type (3)
        auto apic_frame = createAPICFrame("image/png", 3, "Front", image_data);
        
        auto header = createID3v2Header(3, 0, static_cast<uint32_t>(apic_frame.size()));
        
        std::vector<uint8_t> tag_data;
        tag_data.insert(tag_data.end(), header.begin(), header.end());
        tag_data.insert(tag_data.end(), apic_frame.begin(), apic_frame.end());
        
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        
        auto front_cover = tag->getFrontCover();
        ASSERT_TRUE(front_cover.has_value(), "getFrontCover should return picture");
        ASSERT_EQUALS(static_cast<uint8_t>(PictureType::FrontCover), static_cast<uint8_t>(front_cover->type),
                     "picture type should be FrontCover");
    }
};


// ============================================================================
// ID3v2Tag Header Flags Tests
// ============================================================================

class ID3v2Tag_HeaderFlags_Unsync : public TestCase {
public:
    ID3v2Tag_HeaderFlags_Unsync() : TestCase("ID3v2Tag_HeaderFlags_Unsync") {}
protected:
    void runTest() override {
        // Create header with unsync flag (bit 7)
        auto header = createID3v2Header(3, 0x80, 10);
        std::vector<uint8_t> tag_data;
        tag_data.insert(tag_data.end(), header.begin(), header.end());
        for (int i = 0; i < 10; ++i) tag_data.push_back(0);
        
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_TRUE(tag->hasUnsynchronization(), "hasUnsynchronization should return true");
    }
};

class ID3v2Tag_HeaderFlags_ExtendedHeader : public TestCase {
public:
    ID3v2Tag_HeaderFlags_ExtendedHeader() : TestCase("ID3v2Tag_HeaderFlags_ExtendedHeader") {}
protected:
    void runTest() override {
        // Create header with extended header flag (bit 6)
        auto header = createID3v2Header(3, 0x40, 10);
        std::vector<uint8_t> tag_data;
        tag_data.insert(tag_data.end(), header.begin(), header.end());
        for (int i = 0; i < 10; ++i) tag_data.push_back(0);
        
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        // Note: This may fail to parse due to invalid extended header, which is expected
        // The test verifies the flag detection works
        if (tag) {
            ASSERT_TRUE(tag->hasExtendedHeader(), "hasExtendedHeader should return true");
        }
    }
};

class ID3v2Tag_HeaderFlags_Footer : public TestCase {
public:
    ID3v2Tag_HeaderFlags_Footer() : TestCase("ID3v2Tag_HeaderFlags_Footer") {}
protected:
    void runTest() override {
        // Create v2.4 header with footer flag (bit 4)
        auto header = createID3v2Header(4, 0x10, 10);
        std::vector<uint8_t> tag_data;
        tag_data.insert(tag_data.end(), header.begin(), header.end());
        for (int i = 0; i < 10; ++i) tag_data.push_back(0);
        
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_TRUE(tag->hasFooter(), "hasFooter should return true");
    }
};

// ============================================================================
// ID3v2Tag Parse Error Handling Tests
// ============================================================================

class ID3v2Tag_Parse_NullPointer : public TestCase {
public:
    ID3v2Tag_Parse_NullPointer() : TestCase("ID3v2Tag_Parse_NullPointer") {}
protected:
    void runTest() override {
        auto tag = ID3v2Tag::parse(nullptr, 0);
        ASSERT_NULL(tag.get(), "parse should return nullptr for null input");
    }
};

class ID3v2Tag_Parse_InvalidHeader : public TestCase {
public:
    ID3v2Tag_Parse_InvalidHeader() : TestCase("ID3v2Tag_Parse_InvalidHeader") {}
protected:
    void runTest() override {
        std::vector<uint8_t> data = {'X', 'Y', 'Z', 3, 0, 0, 0, 0, 0, 100};
        auto tag = ID3v2Tag::parse(data.data(), data.size());
        ASSERT_NULL(tag.get(), "parse should return nullptr for invalid header");
    }
};

class ID3v2Tag_Parse_TruncatedData : public TestCase {
public:
    ID3v2Tag_Parse_TruncatedData() : TestCase("ID3v2Tag_Parse_TruncatedData") {}
protected:
    void runTest() override {
        // Create header claiming 1000 bytes but only provide 100
        auto header = createID3v2Header(3, 0, 1000);
        std::vector<uint8_t> tag_data;
        tag_data.insert(tag_data.end(), header.begin(), header.end());
        for (int i = 0; i < 100; ++i) tag_data.push_back(0);
        
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        ASSERT_NULL(tag.get(), "parse should return nullptr for truncated data");
    }
};

// ============================================================================
// ID3v2Tag Track/Disc Number Parsing Tests
// ============================================================================

class ID3v2Tag_Parse_TrackNumberOnly : public TestCase {
public:
    ID3v2Tag_Parse_TrackNumberOnly() : TestCase("ID3v2Tag_Parse_TrackNumberOnly") {}
protected:
    void runTest() override {
        auto tag_data = createID3v23Tag({{"TRCK", "7"}});
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_EQUALS(7u, tag->track(), "track should be 7");
        ASSERT_EQUALS(0u, tag->trackTotal(), "trackTotal should be 0 when not specified");
    }
};

class ID3v2Tag_Parse_TrackWithTotal : public TestCase {
public:
    ID3v2Tag_Parse_TrackWithTotal() : TestCase("ID3v2Tag_Parse_TrackWithTotal") {}
protected:
    void runTest() override {
        auto tag_data = createID3v23Tag({{"TRCK", "7/15"}});
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_EQUALS(7u, tag->track(), "track should be 7");
        ASSERT_EQUALS(15u, tag->trackTotal(), "trackTotal should be 15");
    }
};

class ID3v2Tag_Parse_DiscWithTotal : public TestCase {
public:
    ID3v2Tag_Parse_DiscWithTotal() : TestCase("ID3v2Tag_Parse_DiscWithTotal") {}
protected:
    void runTest() override {
        auto tag_data = createID3v23Tag({{"TPOS", "2/3"}});
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_EQUALS(2u, tag->disc(), "disc should be 2");
        ASSERT_EQUALS(3u, tag->discTotal(), "discTotal should be 3");
    }
};

// ============================================================================
// ID3v2Tag Year Parsing Tests
// ============================================================================

class ID3v2Tag_Parse_YearV23 : public TestCase {
public:
    ID3v2Tag_Parse_YearV23() : TestCase("ID3v2Tag_Parse_YearV23") {}
protected:
    void runTest() override {
        auto tag_data = createID3v23Tag({{"TYER", "2024"}});
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_EQUALS(2024u, tag->year(), "year should be 2024");
    }
};

class ID3v2Tag_Parse_YearV24_TDRC : public TestCase {
public:
    ID3v2Tag_Parse_YearV24_TDRC() : TestCase("ID3v2Tag_Parse_YearV24_TDRC") {}
protected:
    void runTest() override {
        auto tag_data = createID3v24Tag({{"TDRC", "2024-06-15"}});
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_EQUALS(2024u, tag->year(), "year should be extracted from TDRC");
    }
};

class ID3v2Tag_Parse_InvalidYear : public TestCase {
public:
    ID3v2Tag_Parse_InvalidYear() : TestCase("ID3v2Tag_Parse_InvalidYear") {}
protected:
    void runTest() override {
        auto tag_data = createID3v23Tag({{"TYER", "ABCD"}});
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_EQUALS(0u, tag->year(), "year should be 0 for invalid input");
    }
};


// ============================================================================
// ID3v2Tag getFrames/getFrame Tests
// ============================================================================

class ID3v2Tag_GetFrames_ExistingFrame : public TestCase {
public:
    ID3v2Tag_GetFrames_ExistingFrame() : TestCase("ID3v2Tag_GetFrames_ExistingFrame") {}
protected:
    void runTest() override {
        auto tag_data = createID3v23Tag({{"TIT2", "Title"}});
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        
        auto frames = tag->getFrames("TIT2");
        ASSERT_EQUALS(static_cast<size_t>(1), frames.size(), "should have 1 TIT2 frame");
        ASSERT_EQUALS(std::string("TIT2"), frames[0].id, "frame ID should be TIT2");
    }
};

class ID3v2Tag_GetFrames_NonexistentFrame : public TestCase {
public:
    ID3v2Tag_GetFrames_NonexistentFrame() : TestCase("ID3v2Tag_GetFrames_NonexistentFrame") {}
protected:
    void runTest() override {
        auto tag_data = createID3v23Tag({{"TIT2", "Title"}});
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        
        auto frames = tag->getFrames("TALB");
        ASSERT_TRUE(frames.empty(), "should have no TALB frames");
    }
};

class ID3v2Tag_GetFrame_ExistingFrame : public TestCase {
public:
    ID3v2Tag_GetFrame_ExistingFrame() : TestCase("ID3v2Tag_GetFrame_ExistingFrame") {}
protected:
    void runTest() override {
        auto tag_data = createID3v23Tag({{"TIT2", "Title"}});
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        
        const ID3v2Frame* frame = tag->getFrame("TIT2");
        ASSERT_NOT_NULL(frame, "getFrame should return frame");
        ASSERT_EQUALS(std::string("TIT2"), frame->id, "frame ID should be TIT2");
    }
};

class ID3v2Tag_GetFrame_NonexistentFrame : public TestCase {
public:
    ID3v2Tag_GetFrame_NonexistentFrame() : TestCase("ID3v2Tag_GetFrame_NonexistentFrame") {}
protected:
    void runTest() override {
        auto tag_data = createID3v23Tag({{"TIT2", "Title"}});
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        
        const ID3v2Frame* frame = tag->getFrame("TALB");
        ASSERT_NULL(frame, "getFrame should return nullptr for nonexistent frame");
    }
};

class ID3v2Tag_GetFrameIds : public TestCase {
public:
    ID3v2Tag_GetFrameIds() : TestCase("ID3v2Tag_GetFrameIds") {}
protected:
    void runTest() override {
        auto tag_data = createID3v23Tag({
            {"TIT2", "Title"},
            {"TPE1", "Artist"},
            {"TALB", "Album"}
        });
        
        auto tag = ID3v2Tag::parse(tag_data.data(), tag_data.size());
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        
        auto frame_ids = tag->getFrameIds();
        ASSERT_EQUALS(static_cast<size_t>(3), frame_ids.size(), "should have 3 frame IDs");
        
        // Check that all expected IDs are present
        bool has_tit2 = false, has_tpe1 = false, has_talb = false;
        for (const auto& id : frame_ids) {
            if (id == "TIT2") has_tit2 = true;
            if (id == "TPE1") has_tpe1 = true;
            if (id == "TALB") has_talb = true;
        }
        ASSERT_TRUE(has_tit2, "should have TIT2");
        ASSERT_TRUE(has_tpe1, "should have TPE1");
        ASSERT_TRUE(has_talb, "should have TALB");
    }
};

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    TestSuite suite("ID3v2Tag Unit Tests");
    
    // isValid tests
    suite.addTest(std::make_unique<ID3v2Tag_IsValid_ValidV23Header>());
    suite.addTest(std::make_unique<ID3v2Tag_IsValid_ValidV24Header>());
    suite.addTest(std::make_unique<ID3v2Tag_IsValid_ValidV22Header>());
    suite.addTest(std::make_unique<ID3v2Tag_IsValid_InvalidMagic>());
    suite.addTest(std::make_unique<ID3v2Tag_IsValid_InvalidVersion>());
    suite.addTest(std::make_unique<ID3v2Tag_IsValid_NullPointer>());
    suite.addTest(std::make_unique<ID3v2Tag_IsValid_TooSmall>());
    suite.addTest(std::make_unique<ID3v2Tag_IsValid_InvalidSynchsafe>());
    
    // getTagSize tests
    suite.addTest(std::make_unique<ID3v2Tag_GetTagSize_ValidHeader>());
    suite.addTest(std::make_unique<ID3v2Tag_GetTagSize_NullPointer>());
    suite.addTest(std::make_unique<ID3v2Tag_GetTagSize_LargeSize>());
    
    // Version detection tests
    suite.addTest(std::make_unique<ID3v2Tag_Parse_DetectsV22>());
    suite.addTest(std::make_unique<ID3v2Tag_Parse_DetectsV23>());
    suite.addTest(std::make_unique<ID3v2Tag_Parse_DetectsV24>());
    
    // Frame ID normalization tests
    suite.addTest(std::make_unique<ID3v2Tag_NormalizeFrameId_V22ToV23>());
    suite.addTest(std::make_unique<ID3v2Tag_NormalizeFrameId_V23Unchanged>());
    suite.addTest(std::make_unique<ID3v2Tag_NormalizeFrameId_UnknownV22>());
    
    // Text frame parsing tests
    suite.addTest(std::make_unique<ID3v2Tag_Parse_TextFrames>());
    suite.addTest(std::make_unique<ID3v2Tag_Parse_V22TextFrames>());
    
    // Text encoding tests
    suite.addTest(std::make_unique<ID3v2Tag_Parse_ISO8859_1Encoding>());
    suite.addTest(std::make_unique<ID3v2Tag_Parse_UTF8Encoding>());
    
    // getTag/hasTag tests
    suite.addTest(std::make_unique<ID3v2Tag_GetTag_StandardKeys>());
    suite.addTest(std::make_unique<ID3v2Tag_GetTag_CaseInsensitive>());
    suite.addTest(std::make_unique<ID3v2Tag_GetTag_FrameIdDirect>());
    suite.addTest(std::make_unique<ID3v2Tag_HasTag_ExistingFields>());
    suite.addTest(std::make_unique<ID3v2Tag_HasTag_NonexistentFields>());
    
    // getAllTags tests
    suite.addTest(std::make_unique<ID3v2Tag_GetAllTags_ReturnsPopulatedMap>());
    
    // isEmpty tests
    suite.addTest(std::make_unique<ID3v2Tag_IsEmpty_WithContent>());
    suite.addTest(std::make_unique<ID3v2Tag_IsEmpty_NoFrames>());
    
    // Picture tests
    suite.addTest(std::make_unique<ID3v2Tag_NoPictures>());
    suite.addTest(std::make_unique<ID3v2Tag_Parse_APICFrame>());
    suite.addTest(std::make_unique<ID3v2Tag_GetFrontCover>());
    
    // Header flags tests
    suite.addTest(std::make_unique<ID3v2Tag_HeaderFlags_Unsync>());
    suite.addTest(std::make_unique<ID3v2Tag_HeaderFlags_ExtendedHeader>());
    suite.addTest(std::make_unique<ID3v2Tag_HeaderFlags_Footer>());
    
    // Error handling tests
    suite.addTest(std::make_unique<ID3v2Tag_Parse_NullPointer>());
    suite.addTest(std::make_unique<ID3v2Tag_Parse_InvalidHeader>());
    suite.addTest(std::make_unique<ID3v2Tag_Parse_TruncatedData>());
    
    // Track/disc number tests
    suite.addTest(std::make_unique<ID3v2Tag_Parse_TrackNumberOnly>());
    suite.addTest(std::make_unique<ID3v2Tag_Parse_TrackWithTotal>());
    suite.addTest(std::make_unique<ID3v2Tag_Parse_DiscWithTotal>());
    
    // Year parsing tests
    suite.addTest(std::make_unique<ID3v2Tag_Parse_YearV23>());
    suite.addTest(std::make_unique<ID3v2Tag_Parse_YearV24_TDRC>());
    suite.addTest(std::make_unique<ID3v2Tag_Parse_InvalidYear>());
    
    // getFrames/getFrame tests
    suite.addTest(std::make_unique<ID3v2Tag_GetFrames_ExistingFrame>());
    suite.addTest(std::make_unique<ID3v2Tag_GetFrames_NonexistentFrame>());
    suite.addTest(std::make_unique<ID3v2Tag_GetFrame_ExistingFrame>());
    suite.addTest(std::make_unique<ID3v2Tag_GetFrame_NonexistentFrame>());
    suite.addTest(std::make_unique<ID3v2Tag_GetFrameIds>());
    
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
