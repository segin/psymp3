/*
 * test_tag_properties.cpp - Property-based tests for Tag framework
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"

#ifdef HAVE_RAPIDCHECK
#include <rapidcheck.h>
#endif

using namespace PsyMP3::Tag;
using namespace TestFramework;

// ============================================================================
// Property-Based Tests using RapidCheck native API
// ============================================================================

#ifdef HAVE_RAPIDCHECK

bool runRapidCheckTests() {
    bool all_passed = true;
    
    std::cout << "Running RapidCheck property-based tests for Tag framework...\n\n";
    
    // Property: NullTag always returns empty strings for all string methods
    std::cout << "  NullTag_AlwaysReturnsEmptyStrings: ";
    auto result1 = rc::check("NullTag always returns empty strings", []() {
        NullTag tag;
        RC_ASSERT(tag.title().empty());
        RC_ASSERT(tag.artist().empty());
        RC_ASSERT(tag.album().empty());
        RC_ASSERT(tag.albumArtist().empty());
        RC_ASSERT(tag.genre().empty());
        RC_ASSERT(tag.comment().empty());
        RC_ASSERT(tag.composer().empty());
    });
    if (!result1) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: NullTag always returns zero for all numeric methods
    std::cout << "  NullTag_AlwaysReturnsZeroForNumerics: ";
    auto result2 = rc::check("NullTag always returns zero for numerics", []() {
        NullTag tag;
        RC_ASSERT(tag.year() == 0);
        RC_ASSERT(tag.track() == 0);
        RC_ASSERT(tag.trackTotal() == 0);
        RC_ASSERT(tag.disc() == 0);
        RC_ASSERT(tag.discTotal() == 0);
    });
    if (!result2) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: NullTag.getTag returns empty for any key
    std::cout << "  NullTag_GetTagReturnsEmptyForAnyKey: ";
    auto result3 = rc::check("NullTag.getTag returns empty for any key", [](const std::string& key) {
        NullTag tag;
        RC_ASSERT(tag.getTag(key).empty());
    });
    if (!result3) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: NullTag.hasTag returns false for any key
    std::cout << "  NullTag_HasTagReturnsFalseForAnyKey: ";
    auto result4 = rc::check("NullTag.hasTag returns false for any key", [](const std::string& key) {
        NullTag tag;
        RC_ASSERT(!tag.hasTag(key));
    });
    if (!result4) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: NullTag.getTagValues returns empty vector for any key
    std::cout << "  NullTag_GetTagValuesReturnsEmptyForAnyKey: ";
    auto result5 = rc::check("NullTag.getTagValues returns empty for any key", [](const std::string& key) {
        NullTag tag;
        RC_ASSERT(tag.getTagValues(key).empty());
    });
    if (!result5) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: NullTag.getPicture returns nullopt for any index
    std::cout << "  NullTag_GetPictureReturnsNulloptForAnyIndex: ";
    auto result6 = rc::check("NullTag.getPicture returns nullopt for any index", [](size_t index) {
        NullTag tag;
        RC_ASSERT(!tag.getPicture(index).has_value());
    });
    if (!result6) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: createTagReader never returns nullptr
    std::cout << "  TagFactory_CreateTagReaderNeverReturnsNull: ";
    auto result7 = rc::check("createTagReader never returns nullptr", [](const std::string& path) {
        auto tag = createTagReader(path);
        RC_ASSERT(tag != nullptr);
    });
    if (!result7) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: createTagReaderFromData never returns nullptr
    std::cout << "  TagFactory_CreateTagReaderFromDataNeverReturnsNull: ";
    auto result8 = rc::check("createTagReaderFromData never returns nullptr", 
        [](const std::vector<uint8_t>& data, const std::string& hint) {
            auto tag = createTagReaderFromData(data.data(), data.size(), hint);
            RC_ASSERT(tag != nullptr);
        });
    if (!result8) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: Picture.isEmpty() is true iff data is empty
    std::cout << "  Picture_IsEmptyIffDataEmpty: ";
    auto result9 = rc::check("Picture.isEmpty() is true iff data is empty", 
        [](const std::vector<uint8_t>& data) {
            Picture pic;
            pic.data = data;
            RC_ASSERT(pic.isEmpty() == data.empty());
        });
    if (!result9) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: NullTag is always empty
    std::cout << "  NullTag_AlwaysEmpty: ";
    auto result10 = rc::check("NullTag is always empty", []() {
        NullTag tag;
        RC_ASSERT(tag.isEmpty());
    });
    if (!result10) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: NullTag format name is always "None"
    std::cout << "  NullTag_FormatNameAlwaysNone: ";
    auto result11 = rc::check("NullTag format name is always 'None'", []() {
        NullTag tag;
        RC_ASSERT(tag.formatName() == "None");
    });
    if (!result11) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: NullTag picture count is always zero
    std::cout << "  NullTag_PictureCountAlwaysZero: ";
    auto result12 = rc::check("NullTag picture count is always zero", []() {
        NullTag tag;
        RC_ASSERT(tag.pictureCount() == 0);
    });
    if (!result12) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: NullTag front cover is always nullopt
    std::cout << "  NullTag_FrontCoverAlwaysNullopt: ";
    auto result13 = rc::check("NullTag front cover is always nullopt", []() {
        NullTag tag;
        RC_ASSERT(!tag.getFrontCover().has_value());
    });
    if (!result13) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: NullTag getAllTags returns empty map
    std::cout << "  NullTag_GetAllTagsAlwaysEmpty: ";
    auto result14 = rc::check("NullTag getAllTags returns empty map", []() {
        NullTag tag;
        RC_ASSERT(tag.getAllTags().empty());
    });
    if (!result14) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // ========================================================================
    // ID3v1 Property-Based Tests (Fuzzing)
    // ========================================================================
    
    std::cout << "\n  --- ID3v1 Fuzzing Tests ---\n";
    
    // Property: ID3v1Tag::parse never crashes on random 128-byte data
    std::cout << "  ID3v1_ParseNeverCrashesOnRandom128Bytes: ";
    auto result15 = rc::check("ID3v1Tag::parse never crashes on random 128-byte data", []() {
        // Generate exactly 128 random bytes
        auto data = *rc::gen::container<std::vector<uint8_t>>(128, rc::gen::arbitrary<uint8_t>());
        RC_ASSERT(data.size() == 128);
        
        // This should never crash, regardless of content
        auto tag = ID3v1Tag::parse(data.data());
        // Result can be nullptr (invalid tag) or valid tag - both are acceptable
        // The key property is that it doesn't crash
        RC_ASSERT(true); // If we get here, no crash occurred
    });
    if (!result15) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: ID3v1Tag::parse never crashes on truncated data
    std::cout << "  ID3v1_ParseNeverCrashesOnTruncatedData: ";
    auto result16 = rc::check("ID3v1Tag::parse never crashes on truncated data", []() {
        // Generate data of random size 0-200 bytes
        auto size = *rc::gen::inRange<size_t>(0, 200);
        auto data = *rc::gen::container<std::vector<uint8_t>>(size, rc::gen::arbitrary<uint8_t>());
        
        // This should never crash, even with wrong size
        auto tag = ID3v1Tag::parse(data.empty() ? nullptr : data.data());
        // Result should be nullptr for invalid/truncated data
        RC_ASSERT(true); // If we get here, no crash occurred
    });
    if (!result16) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: ID3v1Tag::parse returns nullptr for data without "TAG" header
    std::cout << "  ID3v1_ParseReturnsNullForInvalidHeader: ";
    auto result17 = rc::check("ID3v1Tag::parse returns nullptr for data without TAG header", []() {
        // Generate 128 bytes that definitely don't start with "TAG"
        auto data = *rc::gen::container<std::vector<uint8_t>>(128, rc::gen::arbitrary<uint8_t>());
        // Ensure first 3 bytes are NOT "TAG"
        if (data.size() >= 3) {
            // Make sure it's not "TAG" by setting first byte to something else
            if (data[0] == 'T' && data[1] == 'A' && data[2] == 'G') {
                data[0] = 'X'; // Corrupt the header
            }
        }
        
        auto tag = ID3v1Tag::parse(data.data());
        RC_ASSERT(tag == nullptr);
    });
    if (!result17) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: ID3v1Tag::parse returns valid tag for data with "TAG" header
    std::cout << "  ID3v1_ParseReturnsValidTagForValidHeader: ";
    auto result18 = rc::check("ID3v1Tag::parse returns valid tag for data with TAG header", []() {
        // Generate 128 bytes with valid "TAG" header
        auto data = *rc::gen::container<std::vector<uint8_t>>(128, rc::gen::arbitrary<uint8_t>());
        data[0] = 'T';
        data[1] = 'A';
        data[2] = 'G';
        
        auto tag = ID3v1Tag::parse(data.data());
        RC_ASSERT(tag != nullptr);
        RC_ASSERT(tag->formatName() == "ID3v1" || tag->formatName() == "ID3v1.1");
    });
    if (!result18) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: ID3v1 genre index mapping is consistent
    std::cout << "  ID3v1_GenreIndexMappingConsistent: ";
    auto result19 = rc::check("ID3v1 genre index mapping is consistent", []() {
        auto index = *rc::gen::inRange<uint8_t>(0, 255);
        
        std::string genre = ID3v1Tag::genreFromIndex(index);
        
        // Valid genres (0-191) should return non-empty strings
        if (index < ID3v1Tag::GENRE_COUNT) {
            RC_ASSERT(!genre.empty());
        } else {
            // Invalid genres (192-255) should return empty string
            RC_ASSERT(genre.empty());
        }
    });
    if (!result19) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: ID3v1.1 detection based on byte 125 and 126
    std::cout << "  ID3v1_1_DetectionBasedOnBytes125And126: ";
    auto result20 = rc::check("ID3v1.1 detection based on bytes 125 and 126", []() {
        // Generate valid ID3v1 tag
        std::vector<uint8_t> data(128, 0);
        data[0] = 'T';
        data[1] = 'A';
        data[2] = 'G';
        
        // Fill with random data for fields
        for (size_t i = 3; i < 125; ++i) {
            data[i] = *rc::gen::inRange<uint8_t>(32, 126); // Printable ASCII
        }
        
        // Test ID3v1.1 detection: byte 125 = 0, byte 126 = track number (non-zero)
        auto track_num = *rc::gen::inRange<uint8_t>(1, 255);
        data[125] = 0x00;
        data[126] = track_num;
        data[127] = *rc::gen::inRange<uint8_t>(0, 191); // Valid genre
        
        auto tag = ID3v1Tag::parse(data.data());
        RC_ASSERT(tag != nullptr);
        RC_ASSERT(tag->isID3v1_1());
        RC_ASSERT(tag->track() == track_num);
    });
    if (!result20) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: ID3v1.0 detection when byte 125 is non-zero
    std::cout << "  ID3v1_0_DetectionWhenByte125NonZero: ";
    auto result21 = rc::check("ID3v1.0 detection when byte 125 is non-zero", []() {
        // Generate valid ID3v1 tag
        std::vector<uint8_t> data(128, 0);
        data[0] = 'T';
        data[1] = 'A';
        data[2] = 'G';
        
        // Fill with random data for fields
        for (size_t i = 3; i < 127; ++i) {
            data[i] = *rc::gen::inRange<uint8_t>(32, 126); // Printable ASCII
        }
        
        // Ensure byte 125 is non-zero (ID3v1.0 format)
        data[125] = *rc::gen::inRange<uint8_t>(1, 255);
        data[127] = *rc::gen::inRange<uint8_t>(0, 191); // Valid genre
        
        auto tag = ID3v1Tag::parse(data.data());
        RC_ASSERT(tag != nullptr);
        RC_ASSERT(!tag->isID3v1_1());
        RC_ASSERT(tag->track() == 0); // No track in ID3v1.0
    });
    if (!result21) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: ID3v1 string fields are properly trimmed
    std::cout << "  ID3v1_StringFieldsProperlyTrimmed: ";
    auto result22 = rc::check("ID3v1 string fields are properly trimmed", []() {
        // Generate valid ID3v1 tag with trailing spaces/nulls
        std::vector<uint8_t> data(128, 0);
        data[0] = 'T';
        data[1] = 'A';
        data[2] = 'G';
        
        // Title field (bytes 3-32): "Test" followed by spaces
        data[3] = 'T'; data[4] = 'e'; data[5] = 's'; data[6] = 't';
        for (size_t i = 7; i < 33; ++i) data[i] = ' '; // Trailing spaces
        
        // Artist field (bytes 33-62): "Artist" followed by nulls
        data[33] = 'A'; data[34] = 'r'; data[35] = 't'; data[36] = 'i'; data[37] = 's'; data[38] = 't';
        for (size_t i = 39; i < 63; ++i) data[i] = '\0'; // Trailing nulls
        
        data[127] = 12; // Genre: "Other"
        
        auto tag = ID3v1Tag::parse(data.data());
        RC_ASSERT(tag != nullptr);
        RC_ASSERT(tag->title() == "Test");
        RC_ASSERT(tag->artist() == "Artist");
        // Verify no trailing spaces or nulls
        RC_ASSERT(tag->title().back() != ' ');
        RC_ASSERT(tag->title().back() != '\0');
        RC_ASSERT(tag->artist().back() != ' ');
        RC_ASSERT(tag->artist().back() != '\0');
    });
    if (!result22) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: ID3v1 year parsing handles non-numeric gracefully
    std::cout << "  ID3v1_YearParsingHandlesNonNumeric: ";
    auto result23 = rc::check("ID3v1 year parsing handles non-numeric gracefully", []() {
        std::vector<uint8_t> data(128, 0);
        data[0] = 'T';
        data[1] = 'A';
        data[2] = 'G';
        
        // Year field (bytes 93-96): random non-numeric data
        auto year_data = *rc::gen::container<std::vector<uint8_t>>(4, rc::gen::arbitrary<uint8_t>());
        for (size_t i = 0; i < 4; ++i) {
            data[93 + i] = year_data[i];
        }
        
        data[127] = 0; // Genre: "Blues"
        
        // Should not crash, year should be 0 or parsed value
        auto tag = ID3v1Tag::parse(data.data());
        RC_ASSERT(tag != nullptr);
        // Year is either 0 (invalid) or a valid parsed number
        RC_ASSERT(true); // No crash is the key property
    });
    if (!result23) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: ID3v1 isValid correctly identifies TAG header
    std::cout << "  ID3v1_IsValidCorrectlyIdentifiesTAGHeader: ";
    auto result24 = rc::check("ID3v1 isValid correctly identifies TAG header", []() {
        auto data = *rc::gen::container<std::vector<uint8_t>>(128, rc::gen::arbitrary<uint8_t>());
        
        bool has_tag_header = (data[0] == 'T' && data[1] == 'A' && data[2] == 'G');
        bool is_valid = ID3v1Tag::isValid(data.data());
        
        RC_ASSERT(is_valid == has_tag_header);
    });
    if (!result24) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: ID3v1 null pointer handling
    std::cout << "  ID3v1_NullPointerHandling: ";
    auto result25 = rc::check("ID3v1 null pointer handling", []() {
        // isValid should return false for nullptr
        RC_ASSERT(!ID3v1Tag::isValid(nullptr));
        
        // parse should return nullptr for nullptr input
        auto tag = ID3v1Tag::parse(nullptr);
        RC_ASSERT(tag == nullptr);
    });
    if (!result25) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // ========================================================================
    // Property 4: ID3v1 Round-Trip Parsing
    // **Validates: Requirements 4.1, 4.2, 4.3, 4.5**
    // For any valid 128-byte ID3v1 tag data, parsing the data should correctly
    // extract title, artist, album, year, comment, and (for ID3v1.1) track number,
    // with trailing nulls and spaces trimmed.
    // ========================================================================
    
    std::cout << "\n  --- Property 4: ID3v1 Round-Trip Parsing ---\n";
    std::cout << "  ID3v1_RoundTripParsing: ";
    auto result26 = rc::check("ID3v1 round-trip parsing preserves field values", []() {
        // Generate random printable strings (no trailing spaces)
        auto genPrintableChar = rc::gen::inRange<char>(33, 126); // Printable ASCII, no space
        
        // Generate test data with fixed reasonable lengths
        auto title = *rc::gen::container<std::string>(rc::gen::just<size_t>(10), genPrintableChar);
        auto artist = *rc::gen::container<std::string>(rc::gen::just<size_t>(10), genPrintableChar);
        auto album = *rc::gen::container<std::string>(rc::gen::just<size_t>(10), genPrintableChar);
        auto year = *rc::gen::inRange<uint32_t>(1900, 2100);
        auto comment = *rc::gen::container<std::string>(rc::gen::just<size_t>(10), genPrintableChar);
        auto track = *rc::gen::inRange<uint8_t>(1, 99);
        auto genre_idx = *rc::gen::inRange<uint8_t>(0, 191);
        
        // Build ID3v1.1 tag data
        std::vector<uint8_t> data(128, 0);
        data[0] = 'T'; data[1] = 'A'; data[2] = 'G';
        
        // Copy title (bytes 3-32)
        std::copy(title.begin(), title.end(), data.begin() + 3);
        
        // Copy artist (bytes 33-62)
        std::copy(artist.begin(), artist.end(), data.begin() + 33);
        
        // Copy album (bytes 63-92)
        std::copy(album.begin(), album.end(), data.begin() + 63);
        
        // Copy year (bytes 93-96)
        std::string year_str = std::to_string(year);
        std::copy(year_str.begin(), year_str.end(), data.begin() + 93);
        
        // Copy comment (bytes 97-124 for ID3v1.1)
        std::copy(comment.begin(), comment.end(), data.begin() + 97);
        
        // ID3v1.1: byte 125 = 0, byte 126 = track
        data[125] = 0x00;
        data[126] = track;
        
        // Genre (byte 127)
        data[127] = genre_idx;
        
        // Parse the tag
        auto tag = ID3v1Tag::parse(data.data());
        RC_ASSERT(tag != nullptr);
        
        // Verify round-trip: parsed values should match input
        RC_ASSERT(tag->title() == title);
        RC_ASSERT(tag->artist() == artist);
        RC_ASSERT(tag->album() == album);
        RC_ASSERT(tag->year() == year);
        RC_ASSERT(tag->comment() == comment);
        RC_ASSERT(tag->track() == track);
        RC_ASSERT(tag->genreIndex() == genre_idx);
        RC_ASSERT(tag->isID3v1_1());
        RC_ASSERT(tag->formatName() == "ID3v1.1");
    });
    if (!result26) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // ID3v1.0 round-trip (no track number)
    std::cout << "  ID3v1_0_RoundTripParsing: ";
    auto result27 = rc::check("ID3v1.0 round-trip parsing preserves field values", []() {
        auto genPrintableChar = rc::gen::inRange<char>(33, 126);
        
        auto title = *rc::gen::container<std::string>(rc::gen::just<size_t>(10), genPrintableChar);
        auto artist = *rc::gen::container<std::string>(rc::gen::just<size_t>(10), genPrintableChar);
        auto album = *rc::gen::container<std::string>(rc::gen::just<size_t>(10), genPrintableChar);
        auto year = *rc::gen::inRange<uint32_t>(1900, 2100);
        auto comment = *rc::gen::container<std::string>(rc::gen::just<size_t>(10), genPrintableChar);
        auto genre_idx = *rc::gen::inRange<uint8_t>(0, 191);
        
        std::vector<uint8_t> data(128, 0);
        data[0] = 'T'; data[1] = 'A'; data[2] = 'G';
        
        std::copy(title.begin(), title.end(), data.begin() + 3);
        std::copy(artist.begin(), artist.end(), data.begin() + 33);
        std::copy(album.begin(), album.end(), data.begin() + 63);
        
        std::string year_str = std::to_string(year);
        std::copy(year_str.begin(), year_str.end(), data.begin() + 93);
        
        std::copy(comment.begin(), comment.end(), data.begin() + 97);
        
        // ID3v1.0: byte 125 is non-zero (part of comment)
        data[125] = 'X'; // Non-zero byte to trigger ID3v1.0 detection
        
        data[127] = genre_idx;
        
        auto tag = ID3v1Tag::parse(data.data());
        RC_ASSERT(tag != nullptr);
        
        RC_ASSERT(tag->title() == title);
        RC_ASSERT(tag->artist() == artist);
        RC_ASSERT(tag->album() == album);
        RC_ASSERT(tag->year() == year);
        RC_ASSERT(tag->track() == 0); // No track in ID3v1.0
        RC_ASSERT(tag->genreIndex() == genre_idx);
        RC_ASSERT(!tag->isID3v1_1());
        RC_ASSERT(tag->formatName() == "ID3v1");
    });
    if (!result27) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    return all_passed;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    std::cout << "========================================\n";
    std::cout << "Tag Framework Property Tests (RapidCheck)\n";
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

class NullTag_Property_AlwaysReturnsEmptyStrings : public TestCase {
public:
    NullTag_Property_AlwaysReturnsEmptyStrings() 
        : TestCase("NullTag_Property_AlwaysReturnsEmptyStrings") {}
protected:
    void runTest() override {
        NullTag tag;
        ASSERT_TRUE(tag.title().empty(), "title() should be empty");
        ASSERT_TRUE(tag.artist().empty(), "artist() should be empty");
        ASSERT_TRUE(tag.album().empty(), "album() should be empty");
        ASSERT_TRUE(tag.albumArtist().empty(), "albumArtist() should be empty");
        ASSERT_TRUE(tag.genre().empty(), "genre() should be empty");
        ASSERT_TRUE(tag.comment().empty(), "comment() should be empty");
        ASSERT_TRUE(tag.composer().empty(), "composer() should be empty");
    }
};

class NullTag_Property_AlwaysReturnsZeroForNumerics : public TestCase {
public:
    NullTag_Property_AlwaysReturnsZeroForNumerics() 
        : TestCase("NullTag_Property_AlwaysReturnsZeroForNumerics") {}
protected:
    void runTest() override {
        NullTag tag;
        ASSERT_EQUALS(0u, tag.year(), "year() should be 0");
        ASSERT_EQUALS(0u, tag.track(), "track() should be 0");
        ASSERT_EQUALS(0u, tag.trackTotal(), "trackTotal() should be 0");
        ASSERT_EQUALS(0u, tag.disc(), "disc() should be 0");
        ASSERT_EQUALS(0u, tag.discTotal(), "discTotal() should be 0");
    }
};

class NullTag_Property_GetTagReturnsEmptyForVariousKeys : public TestCase {
public:
    NullTag_Property_GetTagReturnsEmptyForVariousKeys() 
        : TestCase("NullTag_Property_GetTagReturnsEmptyForVariousKeys") {}
protected:
    void runTest() override {
        NullTag tag;
        std::vector<std::string> keys = {"", "ARTIST", "TITLE", "ALBUM", 
                                          "very_long_key_name_that_is_unusual",
                                          "key with spaces", "KEY123"};
        for (const auto& key : keys) {
            ASSERT_TRUE(tag.getTag(key).empty(), "getTag() should return empty for key: " + key);
        }
    }
};

class NullTag_Property_HasTagReturnsFalseForVariousKeys : public TestCase {
public:
    NullTag_Property_HasTagReturnsFalseForVariousKeys() 
        : TestCase("NullTag_Property_HasTagReturnsFalseForVariousKeys") {}
protected:
    void runTest() override {
        NullTag tag;
        std::vector<std::string> keys = {"", "ARTIST", "TITLE", "ALBUM", 
                                          "NONEXISTENT", "random_key"};
        for (const auto& key : keys) {
            ASSERT_FALSE(tag.hasTag(key), "hasTag() should return false for key: " + key);
        }
    }
};

class NullTag_Property_GetPictureReturnsNulloptForVariousIndices : public TestCase {
public:
    NullTag_Property_GetPictureReturnsNulloptForVariousIndices() 
        : TestCase("NullTag_Property_GetPictureReturnsNulloptForVariousIndices") {}
protected:
    void runTest() override {
        NullTag tag;
        std::vector<size_t> indices = {0, 1, 10, 100, 1000, SIZE_MAX};
        for (size_t idx : indices) {
            ASSERT_FALSE(tag.getPicture(idx).has_value(), "getPicture() should return nullopt");
        }
    }
};

class TagFactory_Property_CreateTagReaderNeverReturnsNull : public TestCase {
public:
    TagFactory_Property_CreateTagReaderNeverReturnsNull() 
        : TestCase("TagFactory_Property_CreateTagReaderNeverReturnsNull") {}
protected:
    void runTest() override {
        std::vector<std::string> paths = {"", "/nonexistent", "file.mp3", 
                                           "/path/to/file.flac", "relative/path.ogg"};
        for (const auto& path : paths) {
            auto tag = createTagReader(path);
            ASSERT_NOT_NULL(tag.get(), "createTagReader should not return nullptr for: " + path);
        }
    }
};

class TagFactory_Property_CreateTagReaderFromDataNeverReturnsNull : public TestCase {
public:
    TagFactory_Property_CreateTagReaderFromDataNeverReturnsNull() 
        : TestCase("TagFactory_Property_CreateTagReaderFromDataNeverReturnsNull") {}
protected:
    void runTest() override {
        // Test with various data sizes
        std::vector<std::vector<uint8_t>> test_data = {
            {},
            {0x00},
            {0x49, 0x44, 0x33}, // "ID3"
            {0x66, 0x4C, 0x61, 0x43}, // "fLaC"
            {0x4F, 0x67, 0x67, 0x53}, // "OggS"
        };
        
        for (const auto& data : test_data) {
            auto tag = createTagReaderFromData(data.data(), data.size());
            ASSERT_NOT_NULL(tag.get(), "createTagReaderFromData should not return nullptr");
        }
    }
};

class Picture_Property_IsEmptyIffDataEmpty : public TestCase {
public:
    Picture_Property_IsEmptyIffDataEmpty() 
        : TestCase("Picture_Property_IsEmptyIffDataEmpty") {}
protected:
    void runTest() override {
        Picture pic1;
        ASSERT_TRUE(pic1.isEmpty(), "Empty picture should be empty");
        
        Picture pic2;
        pic2.data = {0x00};
        ASSERT_FALSE(pic2.isEmpty(), "Picture with data should not be empty");
        
        Picture pic3;
        pic3.data = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A}; // PNG header
        ASSERT_FALSE(pic3.isEmpty(), "Picture with PNG header should not be empty");
    }
};

// ============================================================================
// ID3v1 Fallback Tests
// ============================================================================

class ID3v1_ParseNeverCrashesOnRandom128Bytes : public TestCase {
public:
    ID3v1_ParseNeverCrashesOnRandom128Bytes() 
        : TestCase("ID3v1_ParseNeverCrashesOnRandom128Bytes") {}
protected:
    void runTest() override {
        // Test with various random-ish patterns
        std::vector<std::vector<uint8_t>> test_data;
        
        // All zeros
        test_data.push_back(std::vector<uint8_t>(128, 0x00));
        
        // All 0xFF
        test_data.push_back(std::vector<uint8_t>(128, 0xFF));
        
        // Alternating pattern
        std::vector<uint8_t> alt(128);
        for (size_t i = 0; i < 128; ++i) alt[i] = (i % 2) ? 0xFF : 0x00;
        test_data.push_back(alt);
        
        // Random-ish pattern using simple PRNG
        std::vector<uint8_t> prng(128);
        uint32_t seed = 12345;
        for (size_t i = 0; i < 128; ++i) {
            seed = seed * 1103515245 + 12345;
            prng[i] = static_cast<uint8_t>((seed >> 16) & 0xFF);
        }
        test_data.push_back(prng);
        
        for (const auto& data : test_data) {
            // Should not crash
            auto tag = ID3v1Tag::parse(data.data());
            // Result can be nullptr or valid - both acceptable
            (void)tag;
        }
        ASSERT_TRUE(true, "No crash occurred");
    }
};

class ID3v1_ParseReturnsNullForInvalidHeader : public TestCase {
public:
    ID3v1_ParseReturnsNullForInvalidHeader() 
        : TestCase("ID3v1_ParseReturnsNullForInvalidHeader") {}
protected:
    void runTest() override {
        std::vector<uint8_t> data(128, 'X');
        
        // Various invalid headers
        std::vector<std::array<uint8_t, 3>> invalid_headers = {
            {'X', 'X', 'X'},
            {'T', 'X', 'G'},
            {'T', 'A', 'X'},
            {'t', 'a', 'g'}, // lowercase
            {0x00, 0x00, 0x00},
            {0xFF, 0xFF, 0xFF},
        };
        
        for (const auto& header : invalid_headers) {
            data[0] = header[0];
            data[1] = header[1];
            data[2] = header[2];
            
            auto tag = ID3v1Tag::parse(data.data());
            ASSERT_NULL(tag.get(), "Should return nullptr for invalid header");
        }
    }
};

class ID3v1_ParseReturnsValidTagForValidHeader : public TestCase {
public:
    ID3v1_ParseReturnsValidTagForValidHeader() 
        : TestCase("ID3v1_ParseReturnsValidTagForValidHeader") {}
protected:
    void runTest() override {
        std::vector<uint8_t> data(128, 0);
        data[0] = 'T';
        data[1] = 'A';
        data[2] = 'G';
        
        auto tag = ID3v1Tag::parse(data.data());
        ASSERT_NOT_NULL(tag.get(), "Should return valid tag for TAG header");
        ASSERT_TRUE(tag->formatName() == "ID3v1" || tag->formatName() == "ID3v1.1",
                    "Format name should be ID3v1 or ID3v1.1");
    }
};

class ID3v1_GenreIndexMappingConsistent : public TestCase {
public:
    ID3v1_GenreIndexMappingConsistent() 
        : TestCase("ID3v1_GenreIndexMappingConsistent") {}
protected:
    void runTest() override {
        // Test all valid genres (0-191)
        for (uint8_t i = 0; i < 192; ++i) {
            std::string genre = ID3v1Tag::genreFromIndex(i);
            ASSERT_FALSE(genre.empty(), "Valid genre index should return non-empty string");
        }
        
        // Test invalid genres (192-255)
        for (int i = 192; i < 256; ++i) {
            std::string genre = ID3v1Tag::genreFromIndex(static_cast<uint8_t>(i));
            ASSERT_TRUE(genre.empty(), "Invalid genre index should return empty string");
        }
    }
};

class ID3v1_1_DetectionBasedOnBytes125And126 : public TestCase {
public:
    ID3v1_1_DetectionBasedOnBytes125And126() 
        : TestCase("ID3v1_1_DetectionBasedOnBytes125And126") {}
protected:
    void runTest() override {
        std::vector<uint8_t> data(128, ' ');
        data[0] = 'T';
        data[1] = 'A';
        data[2] = 'G';
        
        // ID3v1.1: byte 125 = 0, byte 126 = track number
        data[125] = 0x00;
        data[126] = 5; // Track 5
        data[127] = 0; // Genre: Blues
        
        auto tag = ID3v1Tag::parse(data.data());
        ASSERT_NOT_NULL(tag.get(), "Should parse valid ID3v1.1 tag");
        ASSERT_TRUE(tag->isID3v1_1(), "Should detect ID3v1.1 format");
        ASSERT_EQUALS(5u, tag->track(), "Track number should be 5");
    }
};

class ID3v1_0_DetectionWhenByte125NonZero : public TestCase {
public:
    ID3v1_0_DetectionWhenByte125NonZero() 
        : TestCase("ID3v1_0_DetectionWhenByte125NonZero") {}
protected:
    void runTest() override {
        std::vector<uint8_t> data(128, ' ');
        data[0] = 'T';
        data[1] = 'A';
        data[2] = 'G';
        
        // ID3v1.0: byte 125 is non-zero (part of comment)
        data[125] = 'X';
        data[126] = 'Y';
        data[127] = 0; // Genre: Blues
        
        auto tag = ID3v1Tag::parse(data.data());
        ASSERT_NOT_NULL(tag.get(), "Should parse valid ID3v1.0 tag");
        ASSERT_FALSE(tag->isID3v1_1(), "Should detect ID3v1.0 format");
        ASSERT_EQUALS(0u, tag->track(), "Track number should be 0 for ID3v1.0");
    }
};

class ID3v1_StringFieldsProperlyTrimmed : public TestCase {
public:
    ID3v1_StringFieldsProperlyTrimmed() 
        : TestCase("ID3v1_StringFieldsProperlyTrimmed") {}
protected:
    void runTest() override {
        std::vector<uint8_t> data(128, 0);
        data[0] = 'T';
        data[1] = 'A';
        data[2] = 'G';
        
        // Title: "Test" with trailing spaces
        data[3] = 'T'; data[4] = 'e'; data[5] = 's'; data[6] = 't';
        for (size_t i = 7; i < 33; ++i) data[i] = ' ';
        
        // Artist: "Artist" with trailing nulls
        data[33] = 'A'; data[34] = 'r'; data[35] = 't'; data[36] = 'i'; 
        data[37] = 's'; data[38] = 't';
        
        data[127] = 12; // Genre: Other
        
        auto tag = ID3v1Tag::parse(data.data());
        ASSERT_NOT_NULL(tag.get(), "Should parse tag");
        ASSERT_EQUALS(std::string("Test"), tag->title(), "Title should be trimmed");
        ASSERT_EQUALS(std::string("Artist"), tag->artist(), "Artist should be trimmed");
    }
};

class ID3v1_NullPointerHandling : public TestCase {
public:
    ID3v1_NullPointerHandling() 
        : TestCase("ID3v1_NullPointerHandling") {}
protected:
    void runTest() override {
        ASSERT_FALSE(ID3v1Tag::isValid(nullptr), "isValid should return false for nullptr");
        
        auto tag = ID3v1Tag::parse(nullptr);
        ASSERT_NULL(tag.get(), "parse should return nullptr for nullptr input");
    }
};

class ID3v1_YearParsingHandlesNonNumeric : public TestCase {
public:
    ID3v1_YearParsingHandlesNonNumeric() 
        : TestCase("ID3v1_YearParsingHandlesNonNumeric") {}
protected:
    void runTest() override {
        std::vector<uint8_t> data(128, 0);
        data[0] = 'T';
        data[1] = 'A';
        data[2] = 'G';
        
        // Year field with non-numeric data
        data[93] = 'A';
        data[94] = 'B';
        data[95] = 'C';
        data[96] = 'D';
        
        data[127] = 0; // Genre: Blues
        
        // Should not crash
        auto tag = ID3v1Tag::parse(data.data());
        ASSERT_NOT_NULL(tag.get(), "Should parse tag even with invalid year");
        // Year should be 0 for invalid data
        ASSERT_EQUALS(0u, tag->year(), "Year should be 0 for non-numeric data");
    }
};

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    TestSuite suite("Tag Framework Property Tests (Fallback)");
    
    suite.addTest(std::make_unique<NullTag_Property_AlwaysReturnsEmptyStrings>());
    suite.addTest(std::make_unique<NullTag_Property_AlwaysReturnsZeroForNumerics>());
    suite.addTest(std::make_unique<NullTag_Property_GetTagReturnsEmptyForVariousKeys>());
    suite.addTest(std::make_unique<NullTag_Property_HasTagReturnsFalseForVariousKeys>());
    suite.addTest(std::make_unique<NullTag_Property_GetPictureReturnsNulloptForVariousIndices>());
    suite.addTest(std::make_unique<TagFactory_Property_CreateTagReaderNeverReturnsNull>());
    suite.addTest(std::make_unique<TagFactory_Property_CreateTagReaderFromDataNeverReturnsNull>());
    suite.addTest(std::make_unique<Picture_Property_IsEmptyIffDataEmpty>());
    
    // ID3v1 fuzzing tests
    suite.addTest(std::make_unique<ID3v1_ParseNeverCrashesOnRandom128Bytes>());
    suite.addTest(std::make_unique<ID3v1_ParseReturnsNullForInvalidHeader>());
    suite.addTest(std::make_unique<ID3v1_ParseReturnsValidTagForValidHeader>());
    suite.addTest(std::make_unique<ID3v1_GenreIndexMappingConsistent>());
    suite.addTest(std::make_unique<ID3v1_1_DetectionBasedOnBytes125And126>());
    suite.addTest(std::make_unique<ID3v1_0_DetectionWhenByte125NonZero>());
    suite.addTest(std::make_unique<ID3v1_StringFieldsProperlyTrimmed>());
    suite.addTest(std::make_unique<ID3v1_NullPointerHandling>());
    suite.addTest(std::make_unique<ID3v1_YearParsingHandlesNonNumeric>());
    
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results) > 0 ? 1 : 0;
}

#endif // HAVE_RAPIDCHECK

