/*
 * test_tag_factory.cpp - Unit tests for TagFactory
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include <fstream>
#include <cstring>

using namespace PsyMP3::Tag;
using namespace TestFramework;

// ============================================================================
// Format Detection Tests
// ============================================================================

class TagFactory_DetectFormat_ID3v2 : public TestCase {
public:
    TagFactory_DetectFormat_ID3v2() : TestCase("TagFactory_DetectFormat_ID3v2") {}
protected:
    void runTest() override {
        // Create minimal ID3v2 header
        uint8_t data[10] = {'I', 'D', '3', 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        
        TagFormat format = TagFactory::detectFormat(data, sizeof(data));
        ASSERT_TRUE(format == TagFormat::ID3v2, "Should detect ID3v2 format");
    }
};

class TagFactory_DetectFormat_ID3v1 : public TestCase {
public:
    TagFactory_DetectFormat_ID3v1() : TestCase("TagFactory_DetectFormat_ID3v1") {}
protected:
    void runTest() override {
        // Create minimal ID3v1 tag
        uint8_t data[128];
        std::memset(data, 0, sizeof(data));
        data[0] = 'T';
        data[1] = 'A';
        data[2] = 'G';
        
        TagFormat format = TagFactory::detectFormat(data, sizeof(data));
        ASSERT_TRUE(format == TagFormat::ID3v1, "Should detect ID3v1 format");
    }
};

class TagFactory_DetectFormat_VorbisComment : public TestCase {
public:
    TagFactory_DetectFormat_VorbisComment() : TestCase("TagFactory_DetectFormat_VorbisComment") {}
protected:
    void runTest() override {
        // Create minimal VorbisComment structure
        // Little-endian vendor string length (10 bytes) followed by vendor string
        uint8_t data[14] = {
            0x0A, 0x00, 0x00, 0x00,  // vendor_len = 10
            'T', 'e', 's', 't', ' ', 'V', 'e', 'n', 'd', 'r'  // vendor string
        };
        
        TagFormat format = TagFactory::detectFormat(data, sizeof(data));
        ASSERT_TRUE(format == TagFormat::VorbisComment, "Should detect VorbisComment format");
    }
};

class TagFactory_DetectFormat_Unknown : public TestCase {
public:
    TagFactory_DetectFormat_Unknown() : TestCase("TagFactory_DetectFormat_Unknown") {}
protected:
    void runTest() override {
        // Random data that doesn't match any format
        uint8_t data[10] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        
        TagFormat format = TagFactory::detectFormat(data, sizeof(data));
        ASSERT_TRUE(format == TagFormat::Unknown, "Should detect Unknown format");
    }
};

class TagFactory_DetectFormat_EmptyData : public TestCase {
public:
    TagFactory_DetectFormat_EmptyData() : TestCase("TagFactory_DetectFormat_EmptyData") {}
protected:
    void runTest() override {
        TagFormat format = TagFactory::detectFormat(nullptr, 0);
        ASSERT_TRUE(format == TagFormat::Unknown, "Should return Unknown for empty data");
    }
};

class TagFactory_DetectFormat_TooSmall : public TestCase {
public:
    TagFactory_DetectFormat_TooSmall() : TestCase("TagFactory_DetectFormat_TooSmall") {}
protected:
    void runTest() override {
        uint8_t data[2] = {'I', 'D'};
        
        TagFormat format = TagFactory::detectFormat(data, sizeof(data));
        ASSERT_TRUE(format == TagFormat::Unknown, "Should return Unknown for too small data");
    }
};

// ============================================================================
// createFromData Tests
// ============================================================================

class TagFactory_CreateFromData_ID3v2 : public TestCase {
public:
    TagFactory_CreateFromData_ID3v2() : TestCase("TagFactory_CreateFromData_ID3v2") {}
protected:
    void runTest() override {
        // Create minimal valid ID3v2.4 tag
        std::vector<uint8_t> data = {
            'I', 'D', '3',           // ID3 magic
            0x04, 0x00,              // Version 2.4.0
            0x00,                    // Flags
            0x00, 0x00, 0x00, 0x00   // Size (synchsafe) = 0
        };
        
        auto tag = TagFactory::createFromData(data.data(), data.size());
        ASSERT_NOT_NULL(tag.get(), "Should create tag from ID3v2 data");
        ASSERT_TRUE(tag->formatName().find("ID3v2") != std::string::npos, 
                    "Format name should contain ID3v2");
    }
};

class TagFactory_CreateFromData_ID3v1 : public TestCase {
public:
    TagFactory_CreateFromData_ID3v1() : TestCase("TagFactory_CreateFromData_ID3v1") {}
protected:
    void runTest() override {
        // Create minimal valid ID3v1 tag
        std::vector<uint8_t> data(128, 0);
        data[0] = 'T';
        data[1] = 'A';
        data[2] = 'G';
        
        // Add some data
        std::string title = "Test";
        std::copy(title.begin(), title.end(), data.begin() + 3);
        
        auto tag = TagFactory::createFromData(data.data(), data.size());
        ASSERT_NOT_NULL(tag.get(), "Should create tag from ID3v1 data");
        ASSERT_TRUE(tag->formatName().find("ID3v1") != std::string::npos, 
                    "Format name should contain ID3v1");
        ASSERT_EQUALS(std::string("Test"), tag->title(), "Title should be parsed");
    }
};

class TagFactory_CreateFromData_WithHint : public TestCase {
public:
    TagFactory_CreateFromData_WithHint() : TestCase("TagFactory_CreateFromData_WithHint") {}
protected:
    void runTest() override {
        // Create ID3v2 data
        std::vector<uint8_t> data = {
            'I', 'D', '3',
            0x04, 0x00,
            0x00,
            0x00, 0x00, 0x00, 0x00
        };
        
        // Test with hint
        auto tag = TagFactory::createFromData(data.data(), data.size(), "id3v2");
        ASSERT_NOT_NULL(tag.get(), "Should create tag with hint");
        ASSERT_TRUE(tag->formatName().find("ID3v2") != std::string::npos, 
                    "Format name should contain ID3v2");
    }
};

class TagFactory_CreateFromData_InvalidData : public TestCase {
public:
    TagFactory_CreateFromData_InvalidData() : TestCase("TagFactory_CreateFromData_InvalidData") {}
protected:
    void runTest() override {
        // Random invalid data
        std::vector<uint8_t> data = {0xFF, 0xFF, 0xFF, 0xFF};
        
        auto tag = TagFactory::createFromData(data.data(), data.size());
        ASSERT_NOT_NULL(tag.get(), "Should return NullTag for invalid data");
        ASSERT_EQUALS(std::string("None"), tag->formatName(), "Should be NullTag");
    }
};

class TagFactory_CreateFromData_EmptyData : public TestCase {
public:
    TagFactory_CreateFromData_EmptyData() : TestCase("TagFactory_CreateFromData_EmptyData") {}
protected:
    void runTest() override {
        auto tag = TagFactory::createFromData(nullptr, 0);
        ASSERT_NOT_NULL(tag.get(), "Should return NullTag for empty data");
        ASSERT_EQUALS(std::string("None"), tag->formatName(), "Should be NullTag");
    }
};

// ============================================================================
// createFromFile Tests
// ============================================================================

class TagFactory_CreateFromFile_NonexistentFile : public TestCase {
public:
    TagFactory_CreateFromFile_NonexistentFile() : TestCase("TagFactory_CreateFromFile_NonexistentFile") {}
protected:
    void runTest() override {
        auto tag = TagFactory::createFromFile("/nonexistent/file.mp3");
        ASSERT_NOT_NULL(tag.get(), "Should return NullTag for nonexistent file");
        ASSERT_EQUALS(std::string("None"), tag->formatName(), "Should be NullTag");
    }
};

class TagFactory_CreateFromFile_EmptyPath : public TestCase {
public:
    TagFactory_CreateFromFile_EmptyPath() : TestCase("TagFactory_CreateFromFile_EmptyPath") {}
protected:
    void runTest() override {
        auto tag = TagFactory::createFromFile("");
        ASSERT_NOT_NULL(tag.get(), "Should return NullTag for empty path");
        ASSERT_EQUALS(std::string("None"), tag->formatName(), "Should be NullTag");
    }
};

class TagFactory_CreateFromFile_MP3WithID3v1 : public TestCase {
public:
    TagFactory_CreateFromFile_MP3WithID3v1() : TestCase("TagFactory_CreateFromFile_MP3WithID3v1") {}
protected:
    void runTest() override {
        // Create a temporary MP3 file with ID3v1 tag
        const char* filename = "/tmp/test_id3v1.mp3";
        std::ofstream file(filename, std::ios::binary);
        
        // Write some dummy MP3 data
        std::vector<uint8_t> mp3_data(1000, 0xFF);
        file.write(reinterpret_cast<const char*>(mp3_data.data()), mp3_data.size());
        
        // Write ID3v1 tag at end
        std::vector<uint8_t> id3v1(128, 0);
        id3v1[0] = 'T';
        id3v1[1] = 'A';
        id3v1[2] = 'G';
        
        std::string title = "TestTitle";
        std::copy(title.begin(), title.end(), id3v1.begin() + 3);
        
        file.write(reinterpret_cast<const char*>(id3v1.data()), id3v1.size());
        file.close();
        
        // Test
        auto tag = TagFactory::createFromFile(filename);
        ASSERT_NOT_NULL(tag.get(), "Should create tag from MP3 file");
        ASSERT_TRUE(tag->formatName().find("ID3v1") != std::string::npos, 
                    "Format name should contain ID3v1");
        ASSERT_EQUALS(std::string("TestTitle"), tag->title(), "Title should be parsed");
        
        // Cleanup
        std::remove(filename);
    }
};

class TagFactory_CreateFromFile_MP3WithID3v2 : public TestCase {
public:
    TagFactory_CreateFromFile_MP3WithID3v2() : TestCase("TagFactory_CreateFromFile_MP3WithID3v2") {}
protected:
    void runTest() override {
        // Create a temporary MP3 file with ID3v2 tag
        const char* filename = "/tmp/test_id3v2.mp3";
        std::ofstream file(filename, std::ios::binary);
        
        // Write ID3v2 tag at start
        std::vector<uint8_t> id3v2 = {
            'I', 'D', '3',           // ID3 magic
            0x04, 0x00,              // Version 2.4.0
            0x00,                    // Flags
            0x00, 0x00, 0x00, 0x00   // Size (synchsafe) = 0
        };
        file.write(reinterpret_cast<const char*>(id3v2.data()), id3v2.size());
        
        // Write some dummy MP3 data
        std::vector<uint8_t> mp3_data(1000, 0xFF);
        file.write(reinterpret_cast<const char*>(mp3_data.data()), mp3_data.size());
        file.close();
        
        // Test
        auto tag = TagFactory::createFromFile(filename);
        ASSERT_NOT_NULL(tag.get(), "Should create tag from MP3 file");
        ASSERT_TRUE(tag->formatName().find("ID3v2") != std::string::npos, 
                    "Format name should contain ID3v2");
        
        // Cleanup
        std::remove(filename);
    }
};

class TagFactory_CreateFromFile_MP3WithBothID3Tags : public TestCase {
public:
    TagFactory_CreateFromFile_MP3WithBothID3Tags() : TestCase("TagFactory_CreateFromFile_MP3WithBothID3Tags") {}
protected:
    void runTest() override {
        // Create a temporary MP3 file with both ID3v1 and ID3v2 tags
        const char* filename = "/tmp/test_both_id3.mp3";
        std::ofstream file(filename, std::ios::binary);
        
        // Write ID3v2 tag at start
        std::vector<uint8_t> id3v2 = {
            'I', 'D', '3',
            0x04, 0x00,
            0x00,
            0x00, 0x00, 0x00, 0x00
        };
        file.write(reinterpret_cast<const char*>(id3v2.data()), id3v2.size());
        
        // Write some dummy MP3 data
        std::vector<uint8_t> mp3_data(1000, 0xFF);
        file.write(reinterpret_cast<const char*>(mp3_data.data()), mp3_data.size());
        
        // Write ID3v1 tag at end
        std::vector<uint8_t> id3v1(128, 0);
        id3v1[0] = 'T';
        id3v1[1] = 'A';
        id3v1[2] = 'G';
        
        std::string title = "V1Title";
        std::copy(title.begin(), title.end(), id3v1.begin() + 3);
        
        file.write(reinterpret_cast<const char*>(id3v1.data()), id3v1.size());
        file.close();
        
        // Test
        auto tag = TagFactory::createFromFile(filename);
        ASSERT_NOT_NULL(tag.get(), "Should create merged tag from MP3 file");
        
        // Should be a MergedID3Tag
        std::string format = tag->formatName();
        ASSERT_TRUE(format.find("ID3v2") != std::string::npos && 
                    format.find("ID3v1") != std::string::npos, 
                    "Format name should contain both ID3v2 and ID3v1");
        
        // Cleanup
        std::remove(filename);
    }
};

// ============================================================================
// ID3 Detection Helper Tests
// ============================================================================

class TagFactory_HasID3v1_ValidTag : public TestCase {
public:
    TagFactory_HasID3v1_ValidTag() : TestCase("TagFactory_HasID3v1_ValidTag") {}
protected:
    void runTest() override {
        // Create a temporary file with ID3v1 tag
        const char* filename = "/tmp/test_has_id3v1.dat";
        std::ofstream file(filename, std::ios::binary);
        
        // Write some data
        std::vector<uint8_t> data(1000, 0x00);
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        
        // Write ID3v1 tag at end
        std::vector<uint8_t> id3v1(128, 0);
        id3v1[0] = 'T';
        id3v1[1] = 'A';
        id3v1[2] = 'G';
        file.write(reinterpret_cast<const char*>(id3v1.data()), id3v1.size());
        file.close();
        
        // Test
        bool has_id3v1 = TagFactory::hasID3v1(filename);
        ASSERT_TRUE(has_id3v1, "Should detect ID3v1 tag");
        
        // Cleanup
        std::remove(filename);
    }
};

class TagFactory_HasID3v1_NoTag : public TestCase {
public:
    TagFactory_HasID3v1_NoTag() : TestCase("TagFactory_HasID3v1_NoTag") {}
protected:
    void runTest() override {
        // Create a temporary file without ID3v1 tag
        const char* filename = "/tmp/test_no_id3v1.dat";
        std::ofstream file(filename, std::ios::binary);
        
        // Write some data (no ID3v1 tag)
        std::vector<uint8_t> data(1000, 0x00);
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        file.close();
        
        // Test
        bool has_id3v1 = TagFactory::hasID3v1(filename);
        ASSERT_FALSE(has_id3v1, "Should not detect ID3v1 tag");
        
        // Cleanup
        std::remove(filename);
    }
};

class TagFactory_HasID3v1_NonexistentFile : public TestCase {
public:
    TagFactory_HasID3v1_NonexistentFile() : TestCase("TagFactory_HasID3v1_NonexistentFile") {}
protected:
    void runTest() override {
        bool has_id3v1 = TagFactory::hasID3v1("/nonexistent/file.mp3");
        ASSERT_FALSE(has_id3v1, "Should return false for nonexistent file");
    }
};

class TagFactory_GetID3v2Size_ValidTag : public TestCase {
public:
    TagFactory_GetID3v2Size_ValidTag() : TestCase("TagFactory_GetID3v2Size_ValidTag") {}
protected:
    void runTest() override {
        // Create a temporary file with ID3v2 tag
        const char* filename = "/tmp/test_id3v2_size.dat";
        std::ofstream file(filename, std::ios::binary);
        
        // Write ID3v2 header with size = 100 bytes (synchsafe)
        std::vector<uint8_t> id3v2 = {
            'I', 'D', '3',
            0x04, 0x00,
            0x00,
            0x00, 0x00, 0x00, 0x64  // Size = 100 (synchsafe: 0x64 = 100)
        };
        file.write(reinterpret_cast<const char*>(id3v2.data()), id3v2.size());
        file.close();
        
        // Test
        size_t size = TagFactory::getID3v2Size(filename);
        ASSERT_TRUE(size == 110, "Should return correct ID3v2 size (header + data)");
        
        // Cleanup
        std::remove(filename);
    }
};

class TagFactory_GetID3v2Size_NoTag : public TestCase {
public:
    TagFactory_GetID3v2Size_NoTag() : TestCase("TagFactory_GetID3v2Size_NoTag") {}
protected:
    void runTest() override {
        // Create a temporary file without ID3v2 tag
        const char* filename = "/tmp/test_no_id3v2.dat";
        std::ofstream file(filename, std::ios::binary);
        
        // Write some data (no ID3v2 tag)
        std::vector<uint8_t> data(1000, 0x00);
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        file.close();
        
        // Test
        size_t size = TagFactory::getID3v2Size(filename);
        ASSERT_EQUALS(0u, size, "Should return 0 for no ID3v2 tag");
        
        // Cleanup
        std::remove(filename);
    }
};

class TagFactory_GetID3v2Size_NonexistentFile : public TestCase {
public:
    TagFactory_GetID3v2Size_NonexistentFile() : TestCase("TagFactory_GetID3v2Size_NonexistentFile") {}
protected:
    void runTest() override {
        size_t size = TagFactory::getID3v2Size("/nonexistent/file.mp3");
        ASSERT_EQUALS(0u, size, "Should return 0 for nonexistent file");
    }
};

// ============================================================================
// Test Suite Registration
// ============================================================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    TestSuite suite("TagFactory Unit Tests");
    
    // Format detection tests
    suite.addTest(std::make_unique<TagFactory_DetectFormat_ID3v2>());
    suite.addTest(std::make_unique<TagFactory_DetectFormat_ID3v1>());
    suite.addTest(std::make_unique<TagFactory_DetectFormat_VorbisComment>());
    suite.addTest(std::make_unique<TagFactory_DetectFormat_Unknown>());
    suite.addTest(std::make_unique<TagFactory_DetectFormat_EmptyData>());
    suite.addTest(std::make_unique<TagFactory_DetectFormat_TooSmall>());
    
    // createFromData tests
    suite.addTest(std::make_unique<TagFactory_CreateFromData_ID3v2>());
    suite.addTest(std::make_unique<TagFactory_CreateFromData_ID3v1>());
    suite.addTest(std::make_unique<TagFactory_CreateFromData_WithHint>());
    suite.addTest(std::make_unique<TagFactory_CreateFromData_InvalidData>());
    suite.addTest(std::make_unique<TagFactory_CreateFromData_EmptyData>());
    
    // createFromFile tests
    suite.addTest(std::make_unique<TagFactory_CreateFromFile_NonexistentFile>());
    suite.addTest(std::make_unique<TagFactory_CreateFromFile_EmptyPath>());
    suite.addTest(std::make_unique<TagFactory_CreateFromFile_MP3WithID3v1>());
    suite.addTest(std::make_unique<TagFactory_CreateFromFile_MP3WithID3v2>());
    suite.addTest(std::make_unique<TagFactory_CreateFromFile_MP3WithBothID3Tags>());
    
    // ID3 detection helper tests
    suite.addTest(std::make_unique<TagFactory_HasID3v1_ValidTag>());
    suite.addTest(std::make_unique<TagFactory_HasID3v1_NoTag>());
    suite.addTest(std::make_unique<TagFactory_HasID3v1_NonexistentFile>());
    suite.addTest(std::make_unique<TagFactory_GetID3v2Size_ValidTag>());
    suite.addTest(std::make_unique<TagFactory_GetID3v2Size_NoTag>());
    suite.addTest(std::make_unique<TagFactory_GetID3v2Size_NonexistentFile>());
    
    // Run all tests
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results) == 0 ? 0 : 1;
}
