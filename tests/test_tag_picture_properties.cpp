/*
 * test_tag_picture_properties.cpp - Property-based tests for Tag picture access
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Property 13: Picture Index Access
 * **Validates: Requirements 5.1, 5.2, 11.4**
 *
 * For any Tag with N embedded pictures, getPicture(i) should return a valid
 * Picture for 0 ≤ i < N and nullopt for i ≥ N, and pictureCount() should equal N.
 */

#include "psymp3.h"
#include "test_framework.h"

#ifdef HAVE_RAPIDCHECK
#include <rapidcheck.h>
#endif

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
 * @brief Create a VorbisCommentTag with specified number of pictures
 */
static std::unique_ptr<VorbisCommentTag> createTagWithPictures(size_t picture_count) {
    std::map<std::string, std::vector<std::string>> fields;
    fields["TITLE"] = {"Test Title"};
    fields["ARTIST"] = {"Test Artist"};
    
    std::vector<Picture> pictures;
    for (size_t i = 0; i < picture_count; ++i) {
        PictureType type = (i == 0) ? PictureType::FrontCover : 
                           static_cast<PictureType>(i % 21);
        pictures.push_back(createTestPicture(
            type,
            "image/jpeg",
            "Picture " + std::to_string(i),
            100 + i * 10
        ));
    }
    
    return std::make_unique<VorbisCommentTag>("test vendor", fields, pictures);
}

// ============================================================================
// Property-Based Tests using RapidCheck native API
// ============================================================================

#ifdef HAVE_RAPIDCHECK

bool runRapidCheckTests() {
    bool all_passed = true;
    
    std::cout << "Running RapidCheck property-based tests for Tag picture access...\n\n";
    
    // ========================================================================
    // Property 13: Picture Index Access
    // **Validates: Requirements 5.1, 5.2, 11.4**
    // For any Tag with N embedded pictures, getPicture(i) should return a valid
    // Picture for 0 ≤ i < N and nullopt for i ≥ N, and pictureCount() should equal N.
    // ========================================================================
    
    std::cout << "  --- Property 13: Picture Index Access ---\n";
    
    // Property: pictureCount() returns correct count
    std::cout << "  PictureCount_ReturnsCorrectCount: ";
    auto result1 = rc::check("pictureCount() returns correct count", []() {
        // Generate random number of pictures (0-20)
        auto count = *rc::gen::inRange<size_t>(0, 20);
        
        auto tag = createTagWithPictures(count);
        RC_ASSERT(tag->pictureCount() == count);
    });
    if (!result1) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: getPicture(i) returns valid Picture for 0 ≤ i < N
    std::cout << "  GetPicture_ValidIndexReturnsValidPicture: ";
    auto result2 = rc::check("getPicture(i) returns valid Picture for valid index", []() {
        // Generate random number of pictures (1-20, at least 1)
        auto count = *rc::gen::inRange<size_t>(1, 20);
        auto tag = createTagWithPictures(count);
        
        // Generate random valid index
        auto index = *rc::gen::inRange<size_t>(0, count);
        
        auto picture = tag->getPicture(index);
        RC_ASSERT(picture.has_value());
        RC_ASSERT(!picture->isEmpty());
        RC_ASSERT(!picture->data.empty());
    });
    if (!result2) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: getPicture(i) returns nullopt for i ≥ N
    std::cout << "  GetPicture_InvalidIndexReturnsNullopt: ";
    auto result3 = rc::check("getPicture(i) returns nullopt for invalid index", []() {
        // Generate random number of pictures (0-10)
        auto count = *rc::gen::inRange<size_t>(0, 10);
        auto tag = createTagWithPictures(count);
        
        // Generate random invalid index (>= count)
        auto offset = *rc::gen::inRange<size_t>(0, 100);
        auto invalid_index = count + offset;
        
        auto picture = tag->getPicture(invalid_index);
        RC_ASSERT(!picture.has_value());
    });
    if (!result3) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: All valid indices return pictures
    std::cout << "  AllValidIndices_ReturnPictures: ";
    auto result4 = rc::check("All valid indices return pictures", []() {
        auto count = *rc::gen::inRange<size_t>(0, 15);
        auto tag = createTagWithPictures(count);
        
        // Verify all valid indices return pictures
        for (size_t i = 0; i < count; ++i) {
            auto picture = tag->getPicture(i);
            RC_ASSERT(picture.has_value());
        }
        
        // Verify pictureCount matches
        RC_ASSERT(tag->pictureCount() == count);
    });
    if (!result4) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: NullTag always returns 0 pictures and nullopt
    std::cout << "  NullTag_ZeroPicturesAndNullopt: ";
    auto result5 = rc::check("NullTag always returns 0 pictures and nullopt", []() {
        NullTag tag;
        
        RC_ASSERT(tag.pictureCount() == 0);
        
        // Any index should return nullopt
        auto index = *rc::gen::arbitrary<size_t>();
        RC_ASSERT(!tag.getPicture(index).has_value());
        RC_ASSERT(!tag.getFrontCover().has_value());
    });
    if (!result5) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: getFrontCover returns FrontCover type if present
    std::cout << "  GetFrontCover_ReturnsFrontCoverType: ";
    auto result6 = rc::check("getFrontCover returns FrontCover type if present", []() {
        // Create tag with at least one picture (first is always FrontCover)
        auto count = *rc::gen::inRange<size_t>(1, 10);
        auto tag = createTagWithPictures(count);
        
        auto frontCover = tag->getFrontCover();
        RC_ASSERT(frontCover.has_value());
        RC_ASSERT(frontCover->type == PictureType::FrontCover);
    });
    if (!result6) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: Picture data integrity preserved
    std::cout << "  PictureData_IntegrityPreserved: ";
    auto result7 = rc::check("Picture data integrity is preserved", []() {
        auto count = *rc::gen::inRange<size_t>(1, 5);
        auto tag = createTagWithPictures(count);
        
        for (size_t i = 0; i < count; ++i) {
            auto picture = tag->getPicture(i);
            RC_ASSERT(picture.has_value());
            
            // Verify data size matches expected
            size_t expected_size = 100 + i * 10;
            RC_ASSERT(picture->data.size() == expected_size);
            
            // Verify data content
            for (size_t j = 0; j < expected_size; ++j) {
                RC_ASSERT(picture->data[j] == static_cast<uint8_t>(j & 0xFF));
            }
        }
    });
    if (!result7) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: Picture metadata preserved
    std::cout << "  PictureMetadata_Preserved: ";
    auto result8 = rc::check("Picture metadata is preserved", []() {
        auto count = *rc::gen::inRange<size_t>(1, 5);
        auto tag = createTagWithPictures(count);
        
        for (size_t i = 0; i < count; ++i) {
            auto picture = tag->getPicture(i);
            RC_ASSERT(picture.has_value());
            
            // Verify metadata
            RC_ASSERT(picture->mime_type == "image/jpeg");
            RC_ASSERT(picture->description == "Picture " + std::to_string(i));
            RC_ASSERT(picture->width == 100);
            RC_ASSERT(picture->height == 100);
            RC_ASSERT(picture->color_depth == 24);
        }
    });
    if (!result8) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: Empty tag has zero pictures
    std::cout << "  EmptyTag_ZeroPictures: ";
    auto result9 = rc::check("Empty tag has zero pictures", []() {
        auto tag = createTagWithPictures(0);
        
        RC_ASSERT(tag->pictureCount() == 0);
        RC_ASSERT(!tag->getPicture(0).has_value());
        RC_ASSERT(!tag->getFrontCover().has_value());
    });
    if (!result9) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: Large index values don't crash
    std::cout << "  LargeIndex_DoesNotCrash: ";
    auto result10 = rc::check("Large index values don't crash", []() {
        auto count = *rc::gen::inRange<size_t>(0, 5);
        auto tag = createTagWithPictures(count);
        
        // Test with very large indices
        std::vector<size_t> large_indices = {
            SIZE_MAX,
            SIZE_MAX - 1,
            1000000,
            static_cast<size_t>(-1)
        };
        
        for (auto idx : large_indices) {
            auto picture = tag->getPicture(idx);
            RC_ASSERT(!picture.has_value());
        }
    });
    if (!result10) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    return all_passed;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    std::cout << "========================================\n";
    std::cout << "Tag Picture Access Property Tests (RapidCheck)\n";
    std::cout << "Property 13: Picture Index Access\n";
    std::cout << "Validates: Requirements 5.1, 5.2, 11.4\n";
    std::cout << "========================================\n\n";
    
    bool passed = runRapidCheckTests();
    
    std::cout << "\n========================================\n";
    if (passed) {
        std::cout << "All property tests PASSED\n";
    } else {
        std::cout << "Some property tests FAILED\n";
    }
    std::cout << "========================================\n";
    
    return passed ? 0 : 1;
}

#else // !HAVE_RAPIDCHECK

// ============================================================================
// Fallback Tests (when RapidCheck is not available)
// Uses TestFramework's TestCase class pattern
// ============================================================================

class PictureCount_ReturnsCorrectCount : public TestCase {
public:
    PictureCount_ReturnsCorrectCount() 
        : TestCase("PictureCount_ReturnsCorrectCount") {}
protected:
    void runTest() override {
        // Test with various picture counts
        for (size_t count = 0; count <= 10; ++count) {
            auto tag = createTagWithPictures(count);
            ASSERT_EQUALS(count, tag->pictureCount(), 
                "pictureCount() should return " + std::to_string(count));
        }
    }
};

class GetPicture_ValidIndexReturnsValidPicture : public TestCase {
public:
    GetPicture_ValidIndexReturnsValidPicture() 
        : TestCase("GetPicture_ValidIndexReturnsValidPicture") {}
protected:
    void runTest() override {
        auto tag = createTagWithPictures(5);
        
        for (size_t i = 0; i < 5; ++i) {
            auto picture = tag->getPicture(i);
            ASSERT_TRUE(picture.has_value(), 
                "getPicture(" + std::to_string(i) + ") should return valid picture");
            ASSERT_FALSE(picture->isEmpty(), 
                "Picture " + std::to_string(i) + " should not be empty");
        }
    }
};

class GetPicture_InvalidIndexReturnsNullopt : public TestCase {
public:
    GetPicture_InvalidIndexReturnsNullopt() 
        : TestCase("GetPicture_InvalidIndexReturnsNullopt") {}
protected:
    void runTest() override {
        auto tag = createTagWithPictures(3);
        
        // Test indices >= count
        ASSERT_FALSE(tag->getPicture(3).has_value(), 
            "getPicture(3) should return nullopt for tag with 3 pictures");
        ASSERT_FALSE(tag->getPicture(10).has_value(), 
            "getPicture(10) should return nullopt");
        ASSERT_FALSE(tag->getPicture(100).has_value(), 
            "getPicture(100) should return nullopt");
        ASSERT_FALSE(tag->getPicture(SIZE_MAX).has_value(), 
            "getPicture(SIZE_MAX) should return nullopt");
    }
};

class NullTag_ZeroPicturesAndNullopt : public TestCase {
public:
    NullTag_ZeroPicturesAndNullopt() 
        : TestCase("NullTag_ZeroPicturesAndNullopt") {}
protected:
    void runTest() override {
        NullTag tag;
        
        ASSERT_EQUALS(static_cast<size_t>(0), tag.pictureCount(), 
            "NullTag pictureCount() should be 0");
        ASSERT_FALSE(tag.getPicture(0).has_value(), 
            "NullTag getPicture(0) should return nullopt");
        ASSERT_FALSE(tag.getFrontCover().has_value(), 
            "NullTag getFrontCover() should return nullopt");
    }
};

class GetFrontCover_ReturnsFrontCoverType : public TestCase {
public:
    GetFrontCover_ReturnsFrontCoverType() 
        : TestCase("GetFrontCover_ReturnsFrontCoverType") {}
protected:
    void runTest() override {
        auto tag = createTagWithPictures(3);
        
        auto frontCover = tag->getFrontCover();
        ASSERT_TRUE(frontCover.has_value(), "getFrontCover() should return a picture");
        ASSERT_EQUALS(static_cast<uint8_t>(PictureType::FrontCover), 
                      static_cast<uint8_t>(frontCover->type),
                      "getFrontCover() should return FrontCover type");
    }
};

class PictureData_IntegrityPreserved : public TestCase {
public:
    PictureData_IntegrityPreserved() 
        : TestCase("PictureData_IntegrityPreserved") {}
protected:
    void runTest() override {
        auto tag = createTagWithPictures(3);
        
        for (size_t i = 0; i < 3; ++i) {
            auto picture = tag->getPicture(i);
            ASSERT_TRUE(picture.has_value(), "Picture should exist");
            
            size_t expected_size = 100 + i * 10;
            ASSERT_EQUALS(expected_size, picture->data.size(), 
                "Picture data size should match");
            
            // Verify first few bytes
            for (size_t j = 0; j < std::min(expected_size, static_cast<size_t>(10)); ++j) {
                ASSERT_EQUALS(static_cast<uint8_t>(j & 0xFF), picture->data[j],
                    "Picture data byte " + std::to_string(j) + " should match");
            }
        }
    }
};

class EmptyTag_ZeroPictures : public TestCase {
public:
    EmptyTag_ZeroPictures() 
        : TestCase("EmptyTag_ZeroPictures") {}
protected:
    void runTest() override {
        auto tag = createTagWithPictures(0);
        
        ASSERT_EQUALS(static_cast<size_t>(0), tag->pictureCount(), 
            "Empty tag pictureCount() should be 0");
        ASSERT_FALSE(tag->getPicture(0).has_value(), 
            "Empty tag getPicture(0) should return nullopt");
        ASSERT_FALSE(tag->getFrontCover().has_value(), 
            "Empty tag getFrontCover() should return nullopt");
    }
};

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    std::cout << "========================================\n";
    std::cout << "Tag Picture Access Property Tests (Fallback)\n";
    std::cout << "Property 13: Picture Index Access\n";
    std::cout << "Validates: Requirements 5.1, 5.2, 11.4\n";
    std::cout << "========================================\n\n";
    
    int passed = 0;
    int failed = 0;
    
    auto runTest = [&](TestCase& test) {
        try {
            test.run();
            if (test.passed()) {
                passed++;
                std::cout << "  " << test.name() << ": PASSED\n";
            } else {
                failed++;
                std::cout << "  " << test.name() << ": FAILED - " << test.failureMessage() << "\n";
            }
        } catch (const std::exception& e) {
            failed++;
            std::cout << "  " << test.name() << ": EXCEPTION - " << e.what() << "\n";
        }
    };
    
    {
        PictureCount_ReturnsCorrectCount test;
        runTest(test);
    }
    {
        GetPicture_ValidIndexReturnsValidPicture test;
        runTest(test);
    }
    {
        GetPicture_InvalidIndexReturnsNullopt test;
        runTest(test);
    }
    {
        NullTag_ZeroPicturesAndNullopt test;
        runTest(test);
    }
    {
        GetFrontCover_ReturnsFrontCoverType test;
        runTest(test);
    }
    {
        PictureData_IntegrityPreserved test;
        runTest(test);
    }
    {
        EmptyTag_ZeroPictures test;
        runTest(test);
    }
    
    std::cout << "\n========================================\n";
    std::cout << "Results: " << passed << " passed, " << failed << " failed\n";
    std::cout << "========================================\n";
    
    return (failed == 0) ? 0 : 1;
}

#endif // HAVE_RAPIDCHECK

