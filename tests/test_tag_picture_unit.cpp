/*
 * test_tag_picture_unit.cpp - Unit tests for Tag picture access
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Unit tests for:
 * - pictureCount() accuracy
 * - getPicture() bounds checking
 * - getFrontCover() convenience method
 *
 * _Requirements: 5.1, 5.2, 5.3_
 */

#include "psymp3.h"
#include "test_framework.h"

using namespace PsyMP3::Tag;
using namespace TestFramework;

// ============================================================================
// Helper functions to create test pictures
// ============================================================================

/**
 * @brief Create a test picture with specified type and data
 */
static Picture createTestPicture(PictureType type, const std::string& mime,
                                  const std::string& desc, size_t data_size) {
    Picture pic;
    pic.type = type;
    pic.mime_type = mime;
    pic.description = desc;
    pic.width = 100;
    pic.height = 100;
    pic.color_depth = 24;
    pic.colors_used = 0;
    
    // Generate some test data
    pic.data.resize(data_size);
    for (size_t i = 0; i < data_size; ++i) {
        pic.data[i] = static_cast<uint8_t>(i & 0xFF);
    }
    
    return pic;
}

/**
 * @brief Create a VorbisCommentTag with specified pictures
 */
static std::unique_ptr<VorbisCommentTag> createTagWithPictures(
    const std::vector<Picture>& pictures) {
    std::map<std::string, std::vector<std::string>> fields;
    fields["TITLE"] = {"Test Title"};
    fields["ARTIST"] = {"Test Artist"};
    
    return std::make_unique<VorbisCommentTag>("test vendor", fields, pictures);
}

// ============================================================================
// pictureCount() Accuracy Tests
// ============================================================================

class PictureCount_ZeroPictures : public TestCase {
public:
    PictureCount_ZeroPictures() : TestCase("PictureCount_ZeroPictures") {}
protected:
    void runTest() override {
        auto tag = createTagWithPictures({});
        ASSERT_EQUALS(static_cast<size_t>(0), tag->pictureCount(), 
            "pictureCount() should return 0 for tag with no pictures");
    }
};

class PictureCount_OnePicture : public TestCase {
public:
    PictureCount_OnePicture() : TestCase("PictureCount_OnePicture") {}
protected:
    void runTest() override {
        std::vector<Picture> pictures = {
            createTestPicture(PictureType::FrontCover, "image/jpeg", "Cover", 100)
        };
        auto tag = createTagWithPictures(pictures);
        ASSERT_EQUALS(static_cast<size_t>(1), tag->pictureCount(), 
            "pictureCount() should return 1 for tag with one picture");
    }
};

class PictureCount_MultiplePictures : public TestCase {
public:
    PictureCount_MultiplePictures() : TestCase("PictureCount_MultiplePictures") {}
protected:
    void runTest() override {
        std::vector<Picture> pictures = {
            createTestPicture(PictureType::FrontCover, "image/jpeg", "Front", 100),
            createTestPicture(PictureType::BackCover, "image/jpeg", "Back", 150),
            createTestPicture(PictureType::Artist, "image/png", "Artist", 200)
        };
        auto tag = createTagWithPictures(pictures);
        ASSERT_EQUALS(static_cast<size_t>(3), tag->pictureCount(), 
            "pictureCount() should return 3 for tag with three pictures");
    }
};

class PictureCount_NullTag : public TestCase {
public:
    PictureCount_NullTag() : TestCase("PictureCount_NullTag") {}
protected:
    void runTest() override {
        NullTag tag;
        ASSERT_EQUALS(static_cast<size_t>(0), tag.pictureCount(), 
            "NullTag pictureCount() should always return 0");
    }
};

class PictureCount_ID3v1Tag : public TestCase {
public:
    PictureCount_ID3v1Tag() : TestCase("PictureCount_ID3v1Tag") {}
protected:
    void runTest() override {
        // Create a valid ID3v1 tag
        std::vector<uint8_t> data(128, 0);
        data[0] = 'T'; data[1] = 'A'; data[2] = 'G';
        data[3] = 'T'; data[4] = 'i'; data[5] = 't'; data[6] = 'l'; data[7] = 'e';
        data[127] = 17; // Rock genre
        
        auto tag = ID3v1Tag::parse(data.data());
        ASSERT_NOT_NULL(tag.get(), "ID3v1Tag should parse successfully");
        ASSERT_EQUALS(static_cast<size_t>(0), tag->pictureCount(), 
            "ID3v1Tag pictureCount() should always return 0 (no picture support)");
    }
};

// ============================================================================
// getPicture() Bounds Checking Tests
// ============================================================================

class GetPicture_ValidIndex_ReturnsValidPicture : public TestCase {
public:
    GetPicture_ValidIndex_ReturnsValidPicture() 
        : TestCase("GetPicture_ValidIndex_ReturnsValidPicture") {}
protected:
    void runTest() override {
        std::vector<Picture> pictures = {
            createTestPicture(PictureType::FrontCover, "image/jpeg", "Cover", 100)
        };
        auto tag = createTagWithPictures(pictures);
        
        auto picture = tag->getPicture(0);
        ASSERT_TRUE(picture.has_value(), "getPicture(0) should return valid picture");
        ASSERT_FALSE(picture->isEmpty(), "Picture should not be empty");
    }
};

class GetPicture_IndexEqualsCount_ReturnsNullopt : public TestCase {
public:
    GetPicture_IndexEqualsCount_ReturnsNullopt() 
        : TestCase("GetPicture_IndexEqualsCount_ReturnsNullopt") {}
protected:
    void runTest() override {
        std::vector<Picture> pictures = {
            createTestPicture(PictureType::FrontCover, "image/jpeg", "Cover", 100),
            createTestPicture(PictureType::BackCover, "image/jpeg", "Back", 150)
        };
        auto tag = createTagWithPictures(pictures);
        
        // Index 2 is out of bounds (count is 2, valid indices are 0 and 1)
        auto picture = tag->getPicture(2);
        ASSERT_FALSE(picture.has_value(), 
            "getPicture(count) should return nullopt");
    }
};

class GetPicture_IndexGreaterThanCount_ReturnsNullopt : public TestCase {
public:
    GetPicture_IndexGreaterThanCount_ReturnsNullopt() 
        : TestCase("GetPicture_IndexGreaterThanCount_ReturnsNullopt") {}
protected:
    void runTest() override {
        std::vector<Picture> pictures = {
            createTestPicture(PictureType::FrontCover, "image/jpeg", "Cover", 100)
        };
        auto tag = createTagWithPictures(pictures);
        
        ASSERT_FALSE(tag->getPicture(1).has_value(), 
            "getPicture(1) should return nullopt for tag with 1 picture");
        ASSERT_FALSE(tag->getPicture(10).has_value(), 
            "getPicture(10) should return nullopt");
        ASSERT_FALSE(tag->getPicture(100).has_value(), 
            "getPicture(100) should return nullopt");
    }
};

class GetPicture_EmptyTag_ReturnsNullopt : public TestCase {
public:
    GetPicture_EmptyTag_ReturnsNullopt() 
        : TestCase("GetPicture_EmptyTag_ReturnsNullopt") {}
protected:
    void runTest() override {
        auto tag = createTagWithPictures({});
        
        ASSERT_FALSE(tag->getPicture(0).has_value(), 
            "getPicture(0) should return nullopt for empty tag");
    }
};

class GetPicture_MaxIndex_ReturnsNullopt : public TestCase {
public:
    GetPicture_MaxIndex_ReturnsNullopt() 
        : TestCase("GetPicture_MaxIndex_ReturnsNullopt") {}
protected:
    void runTest() override {
        std::vector<Picture> pictures = {
            createTestPicture(PictureType::FrontCover, "image/jpeg", "Cover", 100)
        };
        auto tag = createTagWithPictures(pictures);
        
        // SIZE_MAX should not crash and should return nullopt
        ASSERT_FALSE(tag->getPicture(SIZE_MAX).has_value(), 
            "getPicture(SIZE_MAX) should return nullopt");
    }
};

class GetPicture_NullTag_ReturnsNullopt : public TestCase {
public:
    GetPicture_NullTag_ReturnsNullopt() 
        : TestCase("GetPicture_NullTag_ReturnsNullopt") {}
protected:
    void runTest() override {
        NullTag tag;
        
        ASSERT_FALSE(tag.getPicture(0).has_value(), 
            "NullTag getPicture(0) should return nullopt");
        ASSERT_FALSE(tag.getPicture(1).has_value(), 
            "NullTag getPicture(1) should return nullopt");
        ASSERT_FALSE(tag.getPicture(SIZE_MAX).has_value(), 
            "NullTag getPicture(SIZE_MAX) should return nullopt");
    }
};

class GetPicture_AllValidIndices_ReturnValidPictures : public TestCase {
public:
    GetPicture_AllValidIndices_ReturnValidPictures() 
        : TestCase("GetPicture_AllValidIndices_ReturnValidPictures") {}
protected:
    void runTest() override {
        std::vector<Picture> pictures = {
            createTestPicture(PictureType::FrontCover, "image/jpeg", "Front", 100),
            createTestPicture(PictureType::BackCover, "image/png", "Back", 150),
            createTestPicture(PictureType::Artist, "image/gif", "Artist", 200),
            createTestPicture(PictureType::Media, "image/bmp", "Media", 250),
            createTestPicture(PictureType::Other, "image/webp", "Other", 300)
        };
        auto tag = createTagWithPictures(pictures);
        
        for (size_t i = 0; i < pictures.size(); ++i) {
            auto picture = tag->getPicture(i);
            ASSERT_TRUE(picture.has_value(), 
                "getPicture(" + std::to_string(i) + ") should return valid picture");
            ASSERT_EQUALS(pictures[i].mime_type, picture->mime_type,
                "Picture " + std::to_string(i) + " MIME type should match");
            ASSERT_EQUALS(pictures[i].description, picture->description,
                "Picture " + std::to_string(i) + " description should match");
        }
    }
};

// ============================================================================
// getFrontCover() Convenience Method Tests
// ============================================================================

class GetFrontCover_WithFrontCover_ReturnsFrontCover : public TestCase {
public:
    GetFrontCover_WithFrontCover_ReturnsFrontCover() 
        : TestCase("GetFrontCover_WithFrontCover_ReturnsFrontCover") {}
protected:
    void runTest() override {
        std::vector<Picture> pictures = {
            createTestPicture(PictureType::BackCover, "image/jpeg", "Back", 100),
            createTestPicture(PictureType::FrontCover, "image/jpeg", "Front", 150),
            createTestPicture(PictureType::Artist, "image/png", "Artist", 200)
        };
        auto tag = createTagWithPictures(pictures);
        
        auto frontCover = tag->getFrontCover();
        ASSERT_TRUE(frontCover.has_value(), "getFrontCover() should return a picture");
        ASSERT_EQUALS(static_cast<uint8_t>(PictureType::FrontCover), 
                      static_cast<uint8_t>(frontCover->type),
                      "getFrontCover() should return FrontCover type");
        ASSERT_EQUALS(std::string("Front"), frontCover->description,
                      "getFrontCover() should return the correct picture");
    }
};

class GetFrontCover_WithoutFrontCover_ReturnsFirstPicture : public TestCase {
public:
    GetFrontCover_WithoutFrontCover_ReturnsFirstPicture() 
        : TestCase("GetFrontCover_WithoutFrontCover_ReturnsFirstPicture") {}
protected:
    void runTest() override {
        std::vector<Picture> pictures = {
            createTestPicture(PictureType::BackCover, "image/jpeg", "Back", 100),
            createTestPicture(PictureType::Artist, "image/png", "Artist", 150)
        };
        auto tag = createTagWithPictures(pictures);
        
        auto frontCover = tag->getFrontCover();
        ASSERT_TRUE(frontCover.has_value(), 
            "getFrontCover() should return first picture when no FrontCover");
        ASSERT_EQUALS(static_cast<uint8_t>(PictureType::BackCover), 
                      static_cast<uint8_t>(frontCover->type),
                      "getFrontCover() should return first picture type");
        ASSERT_EQUALS(std::string("Back"), frontCover->description,
                      "getFrontCover() should return first picture");
    }
};

class GetFrontCover_EmptyTag_ReturnsNullopt : public TestCase {
public:
    GetFrontCover_EmptyTag_ReturnsNullopt() 
        : TestCase("GetFrontCover_EmptyTag_ReturnsNullopt") {}
protected:
    void runTest() override {
        auto tag = createTagWithPictures({});
        
        auto frontCover = tag->getFrontCover();
        ASSERT_FALSE(frontCover.has_value(), 
            "getFrontCover() should return nullopt for empty tag");
    }
};

class GetFrontCover_NullTag_ReturnsNullopt : public TestCase {
public:
    GetFrontCover_NullTag_ReturnsNullopt() 
        : TestCase("GetFrontCover_NullTag_ReturnsNullopt") {}
protected:
    void runTest() override {
        NullTag tag;
        
        auto frontCover = tag.getFrontCover();
        ASSERT_FALSE(frontCover.has_value(), 
            "NullTag getFrontCover() should return nullopt");
    }
};

class GetFrontCover_ID3v1Tag_ReturnsNullopt : public TestCase {
public:
    GetFrontCover_ID3v1Tag_ReturnsNullopt() 
        : TestCase("GetFrontCover_ID3v1Tag_ReturnsNullopt") {}
protected:
    void runTest() override {
        // Create a valid ID3v1 tag
        std::vector<uint8_t> data(128, 0);
        data[0] = 'T'; data[1] = 'A'; data[2] = 'G';
        data[3] = 'T'; data[4] = 'i'; data[5] = 't'; data[6] = 'l'; data[7] = 'e';
        data[127] = 17; // Rock genre
        
        auto tag = ID3v1Tag::parse(data.data());
        ASSERT_NOT_NULL(tag.get(), "ID3v1Tag should parse successfully");
        
        auto frontCover = tag->getFrontCover();
        ASSERT_FALSE(frontCover.has_value(), 
            "ID3v1Tag getFrontCover() should return nullopt (no picture support)");
    }
};

class GetFrontCover_MultipleFrontCovers_ReturnsFirst : public TestCase {
public:
    GetFrontCover_MultipleFrontCovers_ReturnsFirst() 
        : TestCase("GetFrontCover_MultipleFrontCovers_ReturnsFirst") {}
protected:
    void runTest() override {
        std::vector<Picture> pictures = {
            createTestPicture(PictureType::FrontCover, "image/jpeg", "First Front", 100),
            createTestPicture(PictureType::FrontCover, "image/png", "Second Front", 150)
        };
        auto tag = createTagWithPictures(pictures);
        
        auto frontCover = tag->getFrontCover();
        ASSERT_TRUE(frontCover.has_value(), "getFrontCover() should return a picture");
        ASSERT_EQUALS(std::string("First Front"), frontCover->description,
                      "getFrontCover() should return the first FrontCover");
    }
};

// ============================================================================
// Picture Data Integrity Tests
// ============================================================================

class Picture_DataIntegrity : public TestCase {
public:
    Picture_DataIntegrity() : TestCase("Picture_DataIntegrity") {}
protected:
    void runTest() override {
        std::vector<Picture> pictures = {
            createTestPicture(PictureType::FrontCover, "image/jpeg", "Cover", 256)
        };
        auto tag = createTagWithPictures(pictures);
        
        auto picture = tag->getPicture(0);
        ASSERT_TRUE(picture.has_value(), "Picture should exist");
        ASSERT_EQUALS(static_cast<size_t>(256), picture->data.size(), 
            "Picture data size should be 256");
        
        // Verify data content
        for (size_t i = 0; i < 256; ++i) {
            ASSERT_EQUALS(static_cast<uint8_t>(i & 0xFF), picture->data[i],
                "Picture data byte " + std::to_string(i) + " should match");
        }
    }
};

class Picture_MetadataIntegrity : public TestCase {
public:
    Picture_MetadataIntegrity() : TestCase("Picture_MetadataIntegrity") {}
protected:
    void runTest() override {
        Picture original;
        original.type = PictureType::BackCover;
        original.mime_type = "image/png";
        original.description = "Test Description";
        original.width = 640;
        original.height = 480;
        original.color_depth = 32;
        original.colors_used = 256;
        original.data = {0x89, 0x50, 0x4E, 0x47}; // PNG magic
        
        std::vector<Picture> pictures = {original};
        auto tag = createTagWithPictures(pictures);
        
        auto picture = tag->getPicture(0);
        ASSERT_TRUE(picture.has_value(), "Picture should exist");
        ASSERT_EQUALS(static_cast<uint8_t>(PictureType::BackCover), 
                      static_cast<uint8_t>(picture->type), "Type should match");
        ASSERT_EQUALS(std::string("image/png"), picture->mime_type, 
            "MIME type should match");
        ASSERT_EQUALS(std::string("Test Description"), picture->description, 
            "Description should match");
        ASSERT_EQUALS(640u, picture->width, "Width should match");
        ASSERT_EQUALS(480u, picture->height, "Height should match");
        ASSERT_EQUALS(32u, picture->color_depth, "Color depth should match");
        ASSERT_EQUALS(256u, picture->colors_used, "Colors used should match");
        ASSERT_EQUALS(static_cast<size_t>(4), picture->data.size(), 
            "Data size should match");
    }
};

// ============================================================================
// Picture isEmpty() Tests
// ============================================================================

class Picture_IsEmpty_EmptyData : public TestCase {
public:
    Picture_IsEmpty_EmptyData() : TestCase("Picture_IsEmpty_EmptyData") {}
protected:
    void runTest() override {
        Picture pic;
        ASSERT_TRUE(pic.isEmpty(), "Picture with empty data should be empty");
    }
};

class Picture_IsEmpty_WithData : public TestCase {
public:
    Picture_IsEmpty_WithData() : TestCase("Picture_IsEmpty_WithData") {}
protected:
    void runTest() override {
        Picture pic;
        pic.data = {0x00}; // Single byte
        ASSERT_FALSE(pic.isEmpty(), "Picture with data should not be empty");
    }
};

// ============================================================================
// Main function
// ============================================================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    TestSuite suite("Tag Picture Access Unit Tests (Requirements: 5.1, 5.2, 5.3)");
    
    // pictureCount() Accuracy Tests
    suite.addTest(std::make_unique<PictureCount_ZeroPictures>());
    suite.addTest(std::make_unique<PictureCount_OnePicture>());
    suite.addTest(std::make_unique<PictureCount_MultiplePictures>());
    suite.addTest(std::make_unique<PictureCount_NullTag>());
    suite.addTest(std::make_unique<PictureCount_ID3v1Tag>());
    
    // getPicture() Bounds Checking Tests
    suite.addTest(std::make_unique<GetPicture_ValidIndex_ReturnsValidPicture>());
    suite.addTest(std::make_unique<GetPicture_IndexEqualsCount_ReturnsNullopt>());
    suite.addTest(std::make_unique<GetPicture_IndexGreaterThanCount_ReturnsNullopt>());
    suite.addTest(std::make_unique<GetPicture_EmptyTag_ReturnsNullopt>());
    suite.addTest(std::make_unique<GetPicture_MaxIndex_ReturnsNullopt>());
    suite.addTest(std::make_unique<GetPicture_NullTag_ReturnsNullopt>());
    suite.addTest(std::make_unique<GetPicture_AllValidIndices_ReturnValidPictures>());
    
    // getFrontCover() Convenience Method Tests
    suite.addTest(std::make_unique<GetFrontCover_WithFrontCover_ReturnsFrontCover>());
    suite.addTest(std::make_unique<GetFrontCover_WithoutFrontCover_ReturnsFirstPicture>());
    suite.addTest(std::make_unique<GetFrontCover_EmptyTag_ReturnsNullopt>());
    suite.addTest(std::make_unique<GetFrontCover_NullTag_ReturnsNullopt>());
    suite.addTest(std::make_unique<GetFrontCover_ID3v1Tag_ReturnsNullopt>());
    suite.addTest(std::make_unique<GetFrontCover_MultipleFrontCovers_ReturnsFirst>());
    
    // Picture Data Integrity Tests
    suite.addTest(std::make_unique<Picture_DataIntegrity>());
    suite.addTest(std::make_unique<Picture_MetadataIntegrity>());
    
    // Picture isEmpty() Tests
    suite.addTest(std::make_unique<Picture_IsEmpty_EmptyData>());
    suite.addTest(std::make_unique<Picture_IsEmpty_WithData>());
    
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results) > 0 ? 1 : 0;
}

