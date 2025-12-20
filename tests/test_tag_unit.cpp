/*
 * test_tag_unit.cpp - Unit tests for Tag framework
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
// NullTag Unit Tests
// ============================================================================

class NullTag_ReturnsEmptyTitle : public TestCase {
public:
    NullTag_ReturnsEmptyTitle() : TestCase("NullTag_ReturnsEmptyTitle") {}
protected:
    void runTest() override {
        NullTag tag;
        ASSERT_EQUALS("", tag.title(), "title() should return empty string");
    }
};

class NullTag_ReturnsEmptyArtist : public TestCase {
public:
    NullTag_ReturnsEmptyArtist() : TestCase("NullTag_ReturnsEmptyArtist") {}
protected:
    void runTest() override {
        NullTag tag;
        ASSERT_EQUALS("", tag.artist(), "artist() should return empty string");
    }
};

class NullTag_ReturnsEmptyAlbum : public TestCase {
public:
    NullTag_ReturnsEmptyAlbum() : TestCase("NullTag_ReturnsEmptyAlbum") {}
protected:
    void runTest() override {
        NullTag tag;
        ASSERT_EQUALS("", tag.album(), "album() should return empty string");
    }
};

class NullTag_ReturnsZeroYear : public TestCase {
public:
    NullTag_ReturnsZeroYear() : TestCase("NullTag_ReturnsZeroYear") {}
protected:
    void runTest() override {
        NullTag tag;
        ASSERT_EQUALS(0u, tag.year(), "year() should return 0");
    }
};

class NullTag_ReturnsZeroTrack : public TestCase {
public:
    NullTag_ReturnsZeroTrack() : TestCase("NullTag_ReturnsZeroTrack") {}
protected:
    void runTest() override {
        NullTag tag;
        ASSERT_EQUALS(0u, tag.track(), "track() should return 0");
    }
};

class NullTag_GetTagReturnsEmpty : public TestCase {
public:
    NullTag_GetTagReturnsEmpty() : TestCase("NullTag_GetTagReturnsEmpty") {}
protected:
    void runTest() override {
        NullTag tag;
        ASSERT_EQUALS("", tag.getTag("ARTIST"), "getTag(ARTIST) should return empty");
        ASSERT_EQUALS("", tag.getTag("TITLE"), "getTag(TITLE) should return empty");
        ASSERT_EQUALS("", tag.getTag("CUSTOM"), "getTag(CUSTOM) should return empty");
    }
};

class NullTag_GetTagValuesReturnsEmpty : public TestCase {
public:
    NullTag_GetTagValuesReturnsEmpty() : TestCase("NullTag_GetTagValuesReturnsEmpty") {}
protected:
    void runTest() override {
        NullTag tag;
        auto values = tag.getTagValues("ARTIST");
        ASSERT_TRUE(values.empty(), "getTagValues() should return empty vector");
    }
};

class NullTag_GetAllTagsReturnsEmpty : public TestCase {
public:
    NullTag_GetAllTagsReturnsEmpty() : TestCase("NullTag_GetAllTagsReturnsEmpty") {}
protected:
    void runTest() override {
        NullTag tag;
        auto tags = tag.getAllTags();
        ASSERT_TRUE(tags.empty(), "getAllTags() should return empty map");
    }
};

class NullTag_HasTagReturnsFalse : public TestCase {
public:
    NullTag_HasTagReturnsFalse() : TestCase("NullTag_HasTagReturnsFalse") {}
protected:
    void runTest() override {
        NullTag tag;
        ASSERT_FALSE(tag.hasTag("ARTIST"), "hasTag(ARTIST) should return false");
        ASSERT_FALSE(tag.hasTag("TITLE"), "hasTag(TITLE) should return false");
        ASSERT_FALSE(tag.hasTag("NONEXISTENT"), "hasTag(NONEXISTENT) should return false");
    }
};

class NullTag_PictureCountReturnsZero : public TestCase {
public:
    NullTag_PictureCountReturnsZero() : TestCase("NullTag_PictureCountReturnsZero") {}
protected:
    void runTest() override {
        NullTag tag;
        ASSERT_EQUALS(0u, tag.pictureCount(), "pictureCount() should return 0");
    }
};

class NullTag_GetPictureReturnsNullopt : public TestCase {
public:
    NullTag_GetPictureReturnsNullopt() : TestCase("NullTag_GetPictureReturnsNullopt") {}
protected:
    void runTest() override {
        NullTag tag;
        ASSERT_FALSE(tag.getPicture(0).has_value(), "getPicture(0) should return nullopt");
        ASSERT_FALSE(tag.getPicture(1).has_value(), "getPicture(1) should return nullopt");
        ASSERT_FALSE(tag.getPicture(100).has_value(), "getPicture(100) should return nullopt");
    }
};

class NullTag_GetFrontCoverReturnsNullopt : public TestCase {
public:
    NullTag_GetFrontCoverReturnsNullopt() : TestCase("NullTag_GetFrontCoverReturnsNullopt") {}
protected:
    void runTest() override {
        NullTag tag;
        ASSERT_FALSE(tag.getFrontCover().has_value(), "getFrontCover() should return nullopt");
    }
};

class NullTag_IsEmptyReturnsTrue : public TestCase {
public:
    NullTag_IsEmptyReturnsTrue() : TestCase("NullTag_IsEmptyReturnsTrue") {}
protected:
    void runTest() override {
        NullTag tag;
        ASSERT_TRUE(tag.isEmpty(), "isEmpty() should return true");
    }
};

class NullTag_FormatNameReturnsNone : public TestCase {
public:
    NullTag_FormatNameReturnsNone() : TestCase("NullTag_FormatNameReturnsNone") {}
protected:
    void runTest() override {
        NullTag tag;
        ASSERT_EQUALS("None", tag.formatName(), "formatName() should return 'None'");
    }
};

// ============================================================================
// Factory Function Tests
// ============================================================================

class CreateTagReader_EmptyPath_ReturnsNullTag : public TestCase {
public:
    CreateTagReader_EmptyPath_ReturnsNullTag() : TestCase("CreateTagReader_EmptyPath_ReturnsNullTag") {}
protected:
    void runTest() override {
        auto tag = createTagReader("");
        ASSERT_NOT_NULL(tag.get(), "createTagReader should not return nullptr");
        ASSERT_TRUE(tag->isEmpty(), "Tag should be empty");
        ASSERT_EQUALS("None", tag->formatName(), "formatName should be 'None'");
    }
};

class CreateTagReader_NonexistentFile_ReturnsNullTag : public TestCase {
public:
    CreateTagReader_NonexistentFile_ReturnsNullTag() : TestCase("CreateTagReader_NonexistentFile_ReturnsNullTag") {}
protected:
    void runTest() override {
        auto tag = createTagReader("/nonexistent/path/to/file.mp3");
        ASSERT_NOT_NULL(tag.get(), "createTagReader should not return nullptr");
        ASSERT_TRUE(tag->isEmpty(), "Tag should be empty");
        ASSERT_EQUALS("None", tag->formatName(), "formatName should be 'None'");
    }
};

class CreateTagReaderFromData_NullData_ReturnsNullTag : public TestCase {
public:
    CreateTagReaderFromData_NullData_ReturnsNullTag() : TestCase("CreateTagReaderFromData_NullData_ReturnsNullTag") {}
protected:
    void runTest() override {
        auto tag = createTagReaderFromData(nullptr, 0);
        ASSERT_NOT_NULL(tag.get(), "createTagReaderFromData should not return nullptr");
        ASSERT_TRUE(tag->isEmpty(), "Tag should be empty");
        ASSERT_EQUALS("None", tag->formatName(), "formatName should be 'None'");
    }
};

class CreateTagReaderFromData_ZeroSize_ReturnsNullTag : public TestCase {
public:
    CreateTagReaderFromData_ZeroSize_ReturnsNullTag() : TestCase("CreateTagReaderFromData_ZeroSize_ReturnsNullTag") {}
protected:
    void runTest() override {
        uint8_t data[] = {0x00};
        auto tag = createTagReaderFromData(data, 0);
        ASSERT_NOT_NULL(tag.get(), "createTagReaderFromData should not return nullptr");
        ASSERT_TRUE(tag->isEmpty(), "Tag should be empty");
        ASSERT_EQUALS("None", tag->formatName(), "formatName should be 'None'");
    }
};

// ============================================================================
// Picture Structure Tests
// ============================================================================

class Picture_DefaultConstruction : public TestCase {
public:
    Picture_DefaultConstruction() : TestCase("Picture_DefaultConstruction") {}
protected:
    void runTest() override {
        Picture pic;
        ASSERT_EQUALS(static_cast<uint8_t>(PictureType::Other), static_cast<uint8_t>(pic.type), "type should be Other");
        ASSERT_EQUALS("", pic.mime_type, "mime_type should be empty");
        ASSERT_EQUALS("", pic.description, "description should be empty");
        ASSERT_EQUALS(0u, pic.width, "width should be 0");
        ASSERT_EQUALS(0u, pic.height, "height should be 0");
        ASSERT_TRUE(pic.data.empty(), "data should be empty");
        ASSERT_TRUE(pic.isEmpty(), "isEmpty() should return true");
    }
};

class Picture_IsEmptyWithData : public TestCase {
public:
    Picture_IsEmptyWithData() : TestCase("Picture_IsEmptyWithData") {}
protected:
    void runTest() override {
        Picture pic;
        pic.data = {0x89, 0x50, 0x4E, 0x47}; // PNG magic
        ASSERT_FALSE(pic.isEmpty(), "isEmpty() should return false when data is present");
    }
};

// ============================================================================
// PictureType Enumeration Tests
// ============================================================================

class PictureType_Values : public TestCase {
public:
    PictureType_Values() : TestCase("PictureType_Values") {}
protected:
    void runTest() override {
        ASSERT_EQUALS(0, static_cast<int>(PictureType::Other), "Other should be 0");
        ASSERT_EQUALS(1, static_cast<int>(PictureType::FileIcon), "FileIcon should be 1");
        ASSERT_EQUALS(3, static_cast<int>(PictureType::FrontCover), "FrontCover should be 3");
        ASSERT_EQUALS(4, static_cast<int>(PictureType::BackCover), "BackCover should be 4");
        ASSERT_EQUALS(20, static_cast<int>(PictureType::PublisherLogotype), "PublisherLogotype should be 20");
    }
};

// ============================================================================
// Tag Interface Polymorphism Tests
// ============================================================================

class Tag_PolymorphicAccess : public TestCase {
public:
    Tag_PolymorphicAccess() : TestCase("Tag_PolymorphicAccess") {}
protected:
    void runTest() override {
        std::unique_ptr<Tag> tag = std::make_unique<NullTag>();
        
        // Access through base class pointer
        ASSERT_EQUALS("", tag->title(), "title() through base pointer should return empty");
        ASSERT_EQUALS("", tag->artist(), "artist() through base pointer should return empty");
        ASSERT_TRUE(tag->isEmpty(), "isEmpty() through base pointer should return true");
        ASSERT_EQUALS("None", tag->formatName(), "formatName() through base pointer should return 'None'");
    }
};

class Tag_MoveSemantics : public TestCase {
public:
    Tag_MoveSemantics() : TestCase("Tag_MoveSemantics") {}
protected:
    void runTest() override {
        NullTag tag1;
        NullTag tag2 = std::move(tag1);
        
        // tag2 should work correctly after move
        ASSERT_TRUE(tag2.isEmpty(), "isEmpty() should return true after move");
        ASSERT_EQUALS("None", tag2.formatName(), "formatName() should return 'None' after move");
    }
};

// ============================================================================
// ID3v1Tag Unit Tests
// ============================================================================

// Helper function to create a valid ID3v1 tag
static std::vector<uint8_t> createID3v1Tag(const char* title, const char* artist,
                                            const char* album, const char* year,
                                            const char* comment, uint8_t genre) {
    std::vector<uint8_t> data(128, 0);
    data[0] = 'T'; data[1] = 'A'; data[2] = 'G';
    
    if (title) {
        size_t len = std::min(strlen(title), static_cast<size_t>(30));
        memcpy(data.data() + 3, title, len);
    }
    if (artist) {
        size_t len = std::min(strlen(artist), static_cast<size_t>(30));
        memcpy(data.data() + 33, artist, len);
    }
    if (album) {
        size_t len = std::min(strlen(album), static_cast<size_t>(30));
        memcpy(data.data() + 63, album, len);
    }
    if (year) {
        size_t len = std::min(strlen(year), static_cast<size_t>(4));
        memcpy(data.data() + 93, year, len);
    }
    if (comment) {
        size_t len = std::min(strlen(comment), static_cast<size_t>(30));
        memcpy(data.data() + 97, comment, len);
    }
    data[127] = genre;
    return data;
}

// Helper function to create a valid ID3v1.1 tag with track number
static std::vector<uint8_t> createID3v1_1Tag(const char* title, const char* artist,
                                              const char* album, const char* year,
                                              const char* comment, uint8_t track,
                                              uint8_t genre) {
    auto data = createID3v1Tag(title, artist, album, year, nullptr, genre);
    
    if (comment) {
        size_t len = std::min(strlen(comment), static_cast<size_t>(28));
        memcpy(data.data() + 97, comment, len);
    }
    data[125] = 0x00;  // ID3v1.1 marker
    data[126] = track; // Track number
    return data;
}

class ID3v1Tag_IsValid_ValidHeader : public TestCase {
public:
    ID3v1Tag_IsValid_ValidHeader() : TestCase("ID3v1Tag_IsValid_ValidHeader") {}
protected:
    void runTest() override {
        auto data = createID3v1Tag("Title", "Artist", "Album", "2024", "Comment", 17);
        ASSERT_TRUE(ID3v1Tag::isValid(data.data()), "isValid should return true for valid TAG header");
    }
};

class ID3v1Tag_IsValid_InvalidHeader : public TestCase {
public:
    ID3v1Tag_IsValid_InvalidHeader() : TestCase("ID3v1Tag_IsValid_InvalidHeader") {}
protected:
    void runTest() override {
        std::vector<uint8_t> data(128, 0);
        data[0] = 'X'; data[1] = 'Y'; data[2] = 'Z';
        ASSERT_FALSE(ID3v1Tag::isValid(data.data()), "isValid should return false for invalid header");
    }
};

class ID3v1Tag_IsValid_NullPointer : public TestCase {
public:
    ID3v1Tag_IsValid_NullPointer() : TestCase("ID3v1Tag_IsValid_NullPointer") {}
protected:
    void runTest() override {
        ASSERT_FALSE(ID3v1Tag::isValid(nullptr), "isValid should return false for nullptr");
    }
};

class ID3v1Tag_Parse_ValidTag : public TestCase {
public:
    ID3v1Tag_Parse_ValidTag() : TestCase("ID3v1Tag_Parse_ValidTag") {}
protected:
    void runTest() override {
        auto data = createID3v1Tag("Test Title", "Test Artist", "Test Album", 
                                    "2024", "Test Comment", 17);
        auto tag = ID3v1Tag::parse(data.data());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_EQUALS(std::string("Test Title"), tag->title(), "title should match");
        ASSERT_EQUALS(std::string("Test Artist"), tag->artist(), "artist should match");
        ASSERT_EQUALS(std::string("Test Album"), tag->album(), "album should match");
        ASSERT_EQUALS(2024u, tag->year(), "year should match");
        ASSERT_EQUALS(std::string("Test Comment"), tag->comment(), "comment should match");
        ASSERT_EQUALS(std::string("Rock"), tag->genre(), "genre should be Rock (17)");
    }
};

class ID3v1Tag_Parse_NullPointer : public TestCase {
public:
    ID3v1Tag_Parse_NullPointer() : TestCase("ID3v1Tag_Parse_NullPointer") {}
protected:
    void runTest() override {
        auto tag = ID3v1Tag::parse(nullptr);
        ASSERT_NULL(tag.get(), "parse should return nullptr for null input");
    }
};

class ID3v1Tag_Parse_InvalidHeader : public TestCase {
public:
    ID3v1Tag_Parse_InvalidHeader() : TestCase("ID3v1Tag_Parse_InvalidHeader") {}
protected:
    void runTest() override {
        std::vector<uint8_t> data(128, 0);
        data[0] = 'X'; data[1] = 'Y'; data[2] = 'Z';
        auto tag = ID3v1Tag::parse(data.data());
        ASSERT_NULL(tag.get(), "parse should return nullptr for invalid header");
    }
};

class ID3v1Tag_DetectsID3v1_0 : public TestCase {
public:
    ID3v1Tag_DetectsID3v1_0() : TestCase("ID3v1Tag_DetectsID3v1_0") {}
protected:
    void runTest() override {
        // ID3v1.0: byte 125 is non-zero (part of comment)
        auto data = createID3v1Tag("Title", "Artist", "Album", "2024", 
                                    "This is a 30 character comment", 17);
        auto tag = ID3v1Tag::parse(data.data());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_FALSE(tag->isID3v1_1(), "should detect ID3v1.0 format");
        ASSERT_EQUALS(0u, tag->track(), "track should be 0 for ID3v1.0");
        ASSERT_EQUALS(std::string("ID3v1"), tag->formatName(), "formatName should be ID3v1");
    }
};

class ID3v1Tag_DetectsID3v1_1 : public TestCase {
public:
    ID3v1Tag_DetectsID3v1_1() : TestCase("ID3v1Tag_DetectsID3v1_1") {}
protected:
    void runTest() override {
        auto data = createID3v1_1Tag("Title", "Artist", "Album", "2024", 
                                      "Comment", 5, 17);
        auto tag = ID3v1Tag::parse(data.data());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_TRUE(tag->isID3v1_1(), "should detect ID3v1.1 format");
        ASSERT_EQUALS(5u, tag->track(), "track should be 5");
        ASSERT_EQUALS(std::string("ID3v1.1"), tag->formatName(), "formatName should be ID3v1.1");
    }
};

class ID3v1Tag_TrimsTrailingSpaces : public TestCase {
public:
    ID3v1Tag_TrimsTrailingSpaces() : TestCase("ID3v1Tag_TrimsTrailingSpaces") {}
protected:
    void runTest() override {
        std::vector<uint8_t> data(128, ' ');
        data[0] = 'T'; data[1] = 'A'; data[2] = 'G';
        // Title: "Test" followed by spaces
        data[3] = 'T'; data[4] = 'e'; data[5] = 's'; data[6] = 't';
        data[127] = 17;
        
        auto tag = ID3v1Tag::parse(data.data());
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_EQUALS(std::string("Test"), tag->title(), "title should be trimmed");
    }
};

class ID3v1Tag_TrimsTrailingNulls : public TestCase {
public:
    ID3v1Tag_TrimsTrailingNulls() : TestCase("ID3v1Tag_TrimsTrailingNulls") {}
protected:
    void runTest() override {
        std::vector<uint8_t> data(128, 0);
        data[0] = 'T'; data[1] = 'A'; data[2] = 'G';
        // Artist: "Artist" followed by nulls
        data[33] = 'A'; data[34] = 'r'; data[35] = 't'; 
        data[36] = 'i'; data[37] = 's'; data[38] = 't';
        data[127] = 17;
        
        auto tag = ID3v1Tag::parse(data.data());
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_EQUALS(std::string("Artist"), tag->artist(), "artist should be trimmed");
    }
};

class ID3v1Tag_GenreMapping_ValidGenres : public TestCase {
public:
    ID3v1Tag_GenreMapping_ValidGenres() : TestCase("ID3v1Tag_GenreMapping_ValidGenres") {}
protected:
    void runTest() override {
        // Test a few specific genres
        ASSERT_EQUALS(std::string("Blues"), ID3v1Tag::genreFromIndex(0), "Genre 0 should be Blues");
        ASSERT_EQUALS(std::string("Classic Rock"), ID3v1Tag::genreFromIndex(1), "Genre 1 should be Classic Rock");
        ASSERT_EQUALS(std::string("Rock"), ID3v1Tag::genreFromIndex(17), "Genre 17 should be Rock");
        ASSERT_EQUALS(std::string("Pop"), ID3v1Tag::genreFromIndex(13), "Genre 13 should be Pop");
        ASSERT_EQUALS(std::string("Hard Rock"), ID3v1Tag::genreFromIndex(79), "Genre 79 should be Hard Rock");
        ASSERT_EQUALS(std::string("Folk"), ID3v1Tag::genreFromIndex(80), "Genre 80 should be Folk");
        ASSERT_EQUALS(std::string("Psybient"), ID3v1Tag::genreFromIndex(191), "Genre 191 should be Psybient");
    }
};

class ID3v1Tag_GenreMapping_InvalidGenres : public TestCase {
public:
    ID3v1Tag_GenreMapping_InvalidGenres() : TestCase("ID3v1Tag_GenreMapping_InvalidGenres") {}
protected:
    void runTest() override {
        ASSERT_EQUALS(std::string(""), ID3v1Tag::genreFromIndex(192), "Genre 192 should be empty");
        ASSERT_EQUALS(std::string(""), ID3v1Tag::genreFromIndex(200), "Genre 200 should be empty");
        ASSERT_EQUALS(std::string(""), ID3v1Tag::genreFromIndex(255), "Genre 255 should be empty");
    }
};

class ID3v1Tag_GenreMapping_AllGenresNonEmpty : public TestCase {
public:
    ID3v1Tag_GenreMapping_AllGenresNonEmpty() : TestCase("ID3v1Tag_GenreMapping_AllGenresNonEmpty") {}
protected:
    void runTest() override {
        // Verify all 192 genres return non-empty strings
        for (uint8_t i = 0; i < 192; ++i) {
            std::string genre = ID3v1Tag::genreFromIndex(i);
            ASSERT_FALSE(genre.empty(), "Genre " + std::to_string(i) + " should not be empty");
        }
    }
};

class ID3v1Tag_GenreList_HasCorrectSize : public TestCase {
public:
    ID3v1Tag_GenreList_HasCorrectSize() : TestCase("ID3v1Tag_GenreList_HasCorrectSize") {}
protected:
    void runTest() override {
        const auto& genres = ID3v1Tag::genreList();
        ASSERT_EQUALS(static_cast<size_t>(192), genres.size(), "Genre list should have 192 entries");
    }
};

class ID3v1Tag_GetTag_StandardKeys : public TestCase {
public:
    ID3v1Tag_GetTag_StandardKeys() : TestCase("ID3v1Tag_GetTag_StandardKeys") {}
protected:
    void runTest() override {
        auto data = createID3v1_1Tag("Title", "Artist", "Album", "2024", "Comment", 5, 17);
        auto tag = ID3v1Tag::parse(data.data());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_EQUALS(std::string("Title"), tag->getTag("TITLE"), "getTag(TITLE) should work");
        ASSERT_EQUALS(std::string("Artist"), tag->getTag("ARTIST"), "getTag(ARTIST) should work");
        ASSERT_EQUALS(std::string("Album"), tag->getTag("ALBUM"), "getTag(ALBUM) should work");
        ASSERT_EQUALS(std::string("2024"), tag->getTag("YEAR"), "getTag(YEAR) should work");
        ASSERT_EQUALS(std::string("Comment"), tag->getTag("COMMENT"), "getTag(COMMENT) should work");
        ASSERT_EQUALS(std::string("Rock"), tag->getTag("GENRE"), "getTag(GENRE) should work");
        ASSERT_EQUALS(std::string("5"), tag->getTag("TRACK"), "getTag(TRACK) should work");
    }
};

class ID3v1Tag_GetTag_CaseInsensitive : public TestCase {
public:
    ID3v1Tag_GetTag_CaseInsensitive() : TestCase("ID3v1Tag_GetTag_CaseInsensitive") {}
protected:
    void runTest() override {
        auto data = createID3v1Tag("Title", "Artist", "Album", "2024", "Comment", 17);
        auto tag = ID3v1Tag::parse(data.data());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_EQUALS(std::string("Title"), tag->getTag("title"), "lowercase key should work");
        ASSERT_EQUALS(std::string("Title"), tag->getTag("TITLE"), "uppercase key should work");
        ASSERT_EQUALS(std::string("Title"), tag->getTag("Title"), "mixed case key should work");
    }
};

class ID3v1Tag_GetTag_ID3v2FrameNames : public TestCase {
public:
    ID3v1Tag_GetTag_ID3v2FrameNames() : TestCase("ID3v1Tag_GetTag_ID3v2FrameNames") {}
protected:
    void runTest() override {
        auto data = createID3v1Tag("Title", "Artist", "Album", "2024", "Comment", 17);
        auto tag = ID3v1Tag::parse(data.data());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        // ID3v2 frame names should also work
        ASSERT_EQUALS(std::string("Title"), tag->getTag("TIT2"), "TIT2 should map to title");
        ASSERT_EQUALS(std::string("Artist"), tag->getTag("TPE1"), "TPE1 should map to artist");
        ASSERT_EQUALS(std::string("Album"), tag->getTag("TALB"), "TALB should map to album");
    }
};

class ID3v1Tag_HasTag_ExistingFields : public TestCase {
public:
    ID3v1Tag_HasTag_ExistingFields() : TestCase("ID3v1Tag_HasTag_ExistingFields") {}
protected:
    void runTest() override {
        auto data = createID3v1Tag("Title", "Artist", "Album", "2024", "Comment", 17);
        auto tag = ID3v1Tag::parse(data.data());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_TRUE(tag->hasTag("TITLE"), "hasTag(TITLE) should return true");
        ASSERT_TRUE(tag->hasTag("ARTIST"), "hasTag(ARTIST) should return true");
        ASSERT_TRUE(tag->hasTag("GENRE"), "hasTag(GENRE) should return true");
    }
};

class ID3v1Tag_HasTag_NonexistentFields : public TestCase {
public:
    ID3v1Tag_HasTag_NonexistentFields() : TestCase("ID3v1Tag_HasTag_NonexistentFields") {}
protected:
    void runTest() override {
        auto data = createID3v1Tag("Title", "", "", "", "", 255);
        auto tag = ID3v1Tag::parse(data.data());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_TRUE(tag->hasTag("TITLE"), "hasTag(TITLE) should return true");
        ASSERT_FALSE(tag->hasTag("ARTIST"), "hasTag(ARTIST) should return false for empty");
        ASSERT_FALSE(tag->hasTag("ALBUMARTIST"), "hasTag(ALBUMARTIST) should return false");
        ASSERT_FALSE(tag->hasTag("COMPOSER"), "hasTag(COMPOSER) should return false");
    }
};

class ID3v1Tag_GetAllTags_ReturnsPopulatedMap : public TestCase {
public:
    ID3v1Tag_GetAllTags_ReturnsPopulatedMap() : TestCase("ID3v1Tag_GetAllTags_ReturnsPopulatedMap") {}
protected:
    void runTest() override {
        auto data = createID3v1_1Tag("Title", "Artist", "Album", "2024", "Comment", 5, 17);
        auto tag = ID3v1Tag::parse(data.data());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        auto allTags = tag->getAllTags();
        
        ASSERT_FALSE(allTags.empty(), "getAllTags should not return empty map");
        ASSERT_EQUALS(std::string("Title"), allTags["TITLE"], "TITLE should be in map");
        ASSERT_EQUALS(std::string("Artist"), allTags["ARTIST"], "ARTIST should be in map");
        ASSERT_EQUALS(std::string("Album"), allTags["ALBUM"], "ALBUM should be in map");
        ASSERT_EQUALS(std::string("2024"), allTags["YEAR"], "YEAR should be in map");
        ASSERT_EQUALS(std::string("Comment"), allTags["COMMENT"], "COMMENT should be in map");
        ASSERT_EQUALS(std::string("Rock"), allTags["GENRE"], "GENRE should be in map");
        ASSERT_EQUALS(std::string("5"), allTags["TRACK"], "TRACK should be in map");
    }
};

class ID3v1Tag_IsEmpty_WithContent : public TestCase {
public:
    ID3v1Tag_IsEmpty_WithContent() : TestCase("ID3v1Tag_IsEmpty_WithContent") {}
protected:
    void runTest() override {
        auto data = createID3v1Tag("Title", "", "", "", "", 255);
        auto tag = ID3v1Tag::parse(data.data());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_FALSE(tag->isEmpty(), "isEmpty should return false when title is set");
    }
};

class ID3v1Tag_IsEmpty_AllEmpty : public TestCase {
public:
    ID3v1Tag_IsEmpty_AllEmpty() : TestCase("ID3v1Tag_IsEmpty_AllEmpty") {}
protected:
    void runTest() override {
        std::vector<uint8_t> data(128, 0);
        data[0] = 'T'; data[1] = 'A'; data[2] = 'G';
        data[127] = 255; // Unknown genre
        
        auto tag = ID3v1Tag::parse(data.data());
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_TRUE(tag->isEmpty(), "isEmpty should return true when all fields empty");
    }
};

class ID3v1Tag_YearParsing_ValidYear : public TestCase {
public:
    ID3v1Tag_YearParsing_ValidYear() : TestCase("ID3v1Tag_YearParsing_ValidYear") {}
protected:
    void runTest() override {
        auto data = createID3v1Tag("", "", "", "2024", "", 0);
        auto tag = ID3v1Tag::parse(data.data());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_EQUALS(2024u, tag->year(), "year should be 2024");
    }
};

class ID3v1Tag_YearParsing_InvalidYear : public TestCase {
public:
    ID3v1Tag_YearParsing_InvalidYear() : TestCase("ID3v1Tag_YearParsing_InvalidYear") {}
protected:
    void runTest() override {
        auto data = createID3v1Tag("", "", "", "ABCD", "", 0);
        auto tag = ID3v1Tag::parse(data.data());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_EQUALS(0u, tag->year(), "year should be 0 for invalid input");
    }
};

class ID3v1Tag_NoPictures : public TestCase {
public:
    ID3v1Tag_NoPictures() : TestCase("ID3v1Tag_NoPictures") {}
protected:
    void runTest() override {
        auto data = createID3v1Tag("Title", "Artist", "Album", "2024", "Comment", 17);
        auto tag = ID3v1Tag::parse(data.data());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        ASSERT_EQUALS(0u, tag->pictureCount(), "pictureCount should be 0");
        ASSERT_FALSE(tag->getPicture(0).has_value(), "getPicture should return nullopt");
        ASSERT_FALSE(tag->getFrontCover().has_value(), "getFrontCover should return nullopt");
    }
};

class ID3v1Tag_UnsupportedFields : public TestCase {
public:
    ID3v1Tag_UnsupportedFields() : TestCase("ID3v1Tag_UnsupportedFields") {}
protected:
    void runTest() override {
        auto data = createID3v1Tag("Title", "Artist", "Album", "2024", "Comment", 17);
        auto tag = ID3v1Tag::parse(data.data());
        
        ASSERT_NOT_NULL(tag.get(), "parse should return valid tag");
        // Fields not supported by ID3v1
        ASSERT_EQUALS(std::string(""), tag->albumArtist(), "albumArtist should be empty");
        ASSERT_EQUALS(std::string(""), tag->composer(), "composer should be empty");
        ASSERT_EQUALS(0u, tag->trackTotal(), "trackTotal should be 0");
        ASSERT_EQUALS(0u, tag->disc(), "disc should be 0");
        ASSERT_EQUALS(0u, tag->discTotal(), "discTotal should be 0");
    }
};

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    TestSuite suite("Tag Framework Unit Tests");
    
    // NullTag tests
    suite.addTest(std::make_unique<NullTag_ReturnsEmptyTitle>());
    suite.addTest(std::make_unique<NullTag_ReturnsEmptyArtist>());
    suite.addTest(std::make_unique<NullTag_ReturnsEmptyAlbum>());
    suite.addTest(std::make_unique<NullTag_ReturnsZeroYear>());
    suite.addTest(std::make_unique<NullTag_ReturnsZeroTrack>());
    suite.addTest(std::make_unique<NullTag_GetTagReturnsEmpty>());
    suite.addTest(std::make_unique<NullTag_GetTagValuesReturnsEmpty>());
    suite.addTest(std::make_unique<NullTag_GetAllTagsReturnsEmpty>());
    suite.addTest(std::make_unique<NullTag_HasTagReturnsFalse>());
    suite.addTest(std::make_unique<NullTag_PictureCountReturnsZero>());
    suite.addTest(std::make_unique<NullTag_GetPictureReturnsNullopt>());
    suite.addTest(std::make_unique<NullTag_GetFrontCoverReturnsNullopt>());
    suite.addTest(std::make_unique<NullTag_IsEmptyReturnsTrue>());
    suite.addTest(std::make_unique<NullTag_FormatNameReturnsNone>());
    
    // Factory function tests
    suite.addTest(std::make_unique<CreateTagReader_EmptyPath_ReturnsNullTag>());
    suite.addTest(std::make_unique<CreateTagReader_NonexistentFile_ReturnsNullTag>());
    suite.addTest(std::make_unique<CreateTagReaderFromData_NullData_ReturnsNullTag>());
    suite.addTest(std::make_unique<CreateTagReaderFromData_ZeroSize_ReturnsNullTag>());
    
    // Picture tests
    suite.addTest(std::make_unique<Picture_DefaultConstruction>());
    suite.addTest(std::make_unique<Picture_IsEmptyWithData>());
    
    // PictureType tests
    suite.addTest(std::make_unique<PictureType_Values>());
    
    // Polymorphism tests
    suite.addTest(std::make_unique<Tag_PolymorphicAccess>());
    suite.addTest(std::make_unique<Tag_MoveSemantics>());
    
    // ID3v1Tag tests
    suite.addTest(std::make_unique<ID3v1Tag_IsValid_ValidHeader>());
    suite.addTest(std::make_unique<ID3v1Tag_IsValid_InvalidHeader>());
    suite.addTest(std::make_unique<ID3v1Tag_IsValid_NullPointer>());
    suite.addTest(std::make_unique<ID3v1Tag_Parse_ValidTag>());
    suite.addTest(std::make_unique<ID3v1Tag_Parse_NullPointer>());
    suite.addTest(std::make_unique<ID3v1Tag_Parse_InvalidHeader>());
    suite.addTest(std::make_unique<ID3v1Tag_DetectsID3v1_0>());
    suite.addTest(std::make_unique<ID3v1Tag_DetectsID3v1_1>());
    suite.addTest(std::make_unique<ID3v1Tag_TrimsTrailingSpaces>());
    suite.addTest(std::make_unique<ID3v1Tag_TrimsTrailingNulls>());
    suite.addTest(std::make_unique<ID3v1Tag_GenreMapping_ValidGenres>());
    suite.addTest(std::make_unique<ID3v1Tag_GenreMapping_InvalidGenres>());
    suite.addTest(std::make_unique<ID3v1Tag_GenreMapping_AllGenresNonEmpty>());
    suite.addTest(std::make_unique<ID3v1Tag_GenreList_HasCorrectSize>());
    suite.addTest(std::make_unique<ID3v1Tag_GetTag_StandardKeys>());
    suite.addTest(std::make_unique<ID3v1Tag_GetTag_CaseInsensitive>());
    suite.addTest(std::make_unique<ID3v1Tag_GetTag_ID3v2FrameNames>());
    suite.addTest(std::make_unique<ID3v1Tag_HasTag_ExistingFields>());
    suite.addTest(std::make_unique<ID3v1Tag_HasTag_NonexistentFields>());
    suite.addTest(std::make_unique<ID3v1Tag_GetAllTags_ReturnsPopulatedMap>());
    suite.addTest(std::make_unique<ID3v1Tag_IsEmpty_WithContent>());
    suite.addTest(std::make_unique<ID3v1Tag_IsEmpty_AllEmpty>());
    suite.addTest(std::make_unique<ID3v1Tag_YearParsing_ValidYear>());
    suite.addTest(std::make_unique<ID3v1Tag_YearParsing_InvalidYear>());
    suite.addTest(std::make_unique<ID3v1Tag_NoPictures>());
    suite.addTest(std::make_unique<ID3v1Tag_UnsupportedFields>());
    
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
