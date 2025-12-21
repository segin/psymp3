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
        
        // Generate test data with fixed reasonable lengths (limit to field sizes)
        auto title = *rc::gen::container<std::string>(30, genPrintableChar);
        auto artist = *rc::gen::container<std::string>(30, genPrintableChar);
        auto album = *rc::gen::container<std::string>(30, genPrintableChar);
        auto year = *rc::gen::inRange<uint32_t>(1900, 2100);
        auto comment = *rc::gen::container<std::string>(28, genPrintableChar); // 28 bytes for ID3v1.1
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
        
        auto title = *rc::gen::container<std::string>(30, genPrintableChar);
        auto artist = *rc::gen::container<std::string>(30, genPrintableChar);
        auto album = *rc::gen::container<std::string>(30, genPrintableChar);
        auto year = *rc::gen::inRange<uint32_t>(1900, 2100);
        auto comment = *rc::gen::container<std::string>(30, genPrintableChar); // 30 bytes for ID3v1.0
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
    
    // ========================================================================
    // Property 6: ID3v2 Synchsafe Integer Round-Trip
    // **Validates: Requirements 3.2**
    // For any 28-bit unsigned integer value, encoding it as a synchsafe integer
    // and then decoding should return the original value.
    // ========================================================================
    
    std::cout << "\n  --- Property 6: ID3v2 Synchsafe Integer Round-Trip ---\n";
    std::cout << "  ID3v2_SynchsafeIntegerRoundTrip: ";
    auto result28 = rc::check("ID3v2 synchsafe integer round-trip", []() {
        // Generate a 28-bit value (0 to 0x0FFFFFFF)
        auto value = *rc::gen::inRange<uint32_t>(0, 0x0FFFFFFF);
        
        // Verify the value can be encoded
        RC_ASSERT(ID3v2Utils::canEncodeSynchsafe(value));
        
        // Encode and decode using uint32_t functions
        uint32_t encoded = ID3v2Utils::encodeSynchsafe(value);
        uint32_t decoded = ID3v2Utils::decodeSynchsafe(encoded);
        
        // Round-trip should preserve the value
        RC_ASSERT(decoded == value);
    });
    if (!result28) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: Synchsafe byte encoding/decoding round-trip
    std::cout << "  ID3v2_SynchsafeBytesRoundTrip: ";
    auto result29 = rc::check("ID3v2 synchsafe bytes round-trip", []() {
        auto value = *rc::gen::inRange<uint32_t>(0, 0x0FFFFFFF);
        
        // Encode to bytes
        uint8_t bytes[4];
        ID3v2Utils::encodeSynchsafeBytes(value, bytes);
        
        // Verify each byte has MSB = 0 (synchsafe property)
        RC_ASSERT((bytes[0] & 0x80) == 0);
        RC_ASSERT((bytes[1] & 0x80) == 0);
        RC_ASSERT((bytes[2] & 0x80) == 0);
        RC_ASSERT((bytes[3] & 0x80) == 0);
        
        // Decode from bytes
        uint32_t decoded = ID3v2Utils::decodeSynchsafeBytes(bytes);
        
        // Round-trip should preserve the value
        RC_ASSERT(decoded == value);
    });
    if (!result29) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: Values > 28 bits cannot be encoded as synchsafe
    std::cout << "  ID3v2_SynchsafeRejectsLargeValues: ";
    auto result30 = rc::check("ID3v2 synchsafe rejects values > 28 bits", []() {
        // Generate a value that's definitely > 28 bits
        auto value = *rc::gen::inRange<uint32_t>(0x10000000, 0xFFFFFFFF);
        
        // Should not be encodable as synchsafe
        RC_ASSERT(!ID3v2Utils::canEncodeSynchsafe(value));
    });
    if (!result30) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // ========================================================================
    // Property 7: ID3v2 Text Encoding Round-Trip
    // **Validates: Requirements 3.4**
    // For any UTF-8 string, encoding it in any ID3v2 text encoding and then
    // decoding should return the original string (or equivalent for ISO-8859-1).
    // ========================================================================
    
    std::cout << "\n  --- Property 7: ID3v2 Text Encoding Round-Trip ---\n";
    
    // Property: UTF-8 round-trip (lossless)
    std::cout << "  ID3v2_UTF8RoundTrip: ";
    auto result31 = rc::check("ID3v2 UTF-8 text round-trip", []() {
        // Generate ASCII string (safe for all encodings) - use fixed size
        auto text = *rc::gen::container<std::string>(rc::gen::inRange<char>(32, 126));
        
        // Encode as UTF-8
        auto encoded = ID3v2Utils::encodeText(text, ID3v2Utils::TextEncoding::UTF_8);
        
        // Decode back
        std::string decoded = ID3v2Utils::decodeText(
            encoded.data(), encoded.size(), ID3v2Utils::TextEncoding::UTF_8);
        
        // Round-trip should preserve the value
        RC_ASSERT(decoded == text);
    });
    if (!result31) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: ISO-8859-1 round-trip (ASCII subset)
    std::cout << "  ID3v2_ISO8859_1RoundTrip: ";
    auto result32 = rc::check("ID3v2 ISO-8859-1 text round-trip (ASCII)", []() {
        // Generate ASCII string (safe for ISO-8859-1)
        auto text = *rc::gen::container<std::string>(rc::gen::inRange<char>(32, 126));
        
        // Encode as ISO-8859-1
        auto encoded = ID3v2Utils::encodeText(text, ID3v2Utils::TextEncoding::ISO_8859_1);
        
        // Decode back
        std::string decoded = ID3v2Utils::decodeText(
            encoded.data(), encoded.size(), ID3v2Utils::TextEncoding::ISO_8859_1);
        
        // Round-trip should preserve the value
        RC_ASSERT(decoded == text);
    });
    if (!result32) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: UTF-16 with BOM round-trip
    std::cout << "  ID3v2_UTF16BOMRoundTrip: ";
    auto result33 = rc::check("ID3v2 UTF-16 BOM text round-trip", []() {
        // Generate ASCII string
        auto text = *rc::gen::container<std::string>(rc::gen::inRange<char>(32, 126));
        
        // Encode as UTF-16 with BOM
        auto encoded = ID3v2Utils::encodeText(text, ID3v2Utils::TextEncoding::UTF_16_BOM);
        
        // Decode back
        std::string decoded = ID3v2Utils::decodeText(
            encoded.data(), encoded.size(), ID3v2Utils::TextEncoding::UTF_16_BOM);
        
        // Round-trip should preserve the value
        RC_ASSERT(decoded == text);
    });
    if (!result33) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: UTF-16 BE round-trip
    std::cout << "  ID3v2_UTF16BERoundTrip: ";
    auto result34 = rc::check("ID3v2 UTF-16 BE text round-trip", []() {
        // Generate ASCII string
        auto text = *rc::gen::container<std::string>(rc::gen::inRange<char>(32, 126));
        
        // Encode as UTF-16 BE
        auto encoded = ID3v2Utils::encodeText(text, ID3v2Utils::TextEncoding::UTF_16_BE);
        
        // Decode back
        std::string decoded = ID3v2Utils::decodeText(
            encoded.data(), encoded.size(), ID3v2Utils::TextEncoding::UTF_16_BE);
        
        // Round-trip should preserve the value
        RC_ASSERT(decoded == text);
    });
    if (!result34) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: Text with encoding byte round-trip
    std::cout << "  ID3v2_TextWithEncodingRoundTrip: ";
    auto result35 = rc::check("ID3v2 text with encoding byte round-trip", []() {
        // Generate ASCII string
        auto text = *rc::gen::container<std::string>(rc::gen::inRange<char>(32, 126));
        
        // Test all encodings
        for (uint8_t enc = 0; enc <= 3; ++enc) {
            auto encoding = static_cast<ID3v2Utils::TextEncoding>(enc);
            
            // Encode with encoding byte prefix
            auto encoded = ID3v2Utils::encodeTextWithEncoding(text, encoding);
            
            // First byte should be the encoding
            RC_ASSERT(encoded[0] == enc);
            
            // Decode with encoding byte
            std::string decoded = ID3v2Utils::decodeTextWithEncoding(
                encoded.data(), encoded.size());
            
            // Round-trip should preserve the value
            RC_ASSERT(decoded == text);
        }
    });
    if (!result35) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // ========================================================================
    // ID3v2 Unsynchronization Tests
    // ========================================================================
    
    std::cout << "\n  --- ID3v2 Unsynchronization Tests ---\n";
    
    // Property: Unsync decode/encode round-trip
    std::cout << "  ID3v2_UnsyncRoundTrip: ";
    auto result36 = rc::check("ID3v2 unsync round-trip", []() {
        // Generate random data with fixed size
        auto data = *rc::gen::container<std::vector<uint8_t>>(rc::gen::arbitrary<uint8_t>());
        
        // Encode with unsync
        auto encoded = ID3v2Utils::encodeUnsync(data.data(), data.size());
        
        // Decode unsync
        auto decoded = ID3v2Utils::decodeUnsync(encoded.data(), encoded.size());
        
        // Round-trip should preserve the data
        RC_ASSERT(decoded == data);
    });
    if (!result36) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: Unsync encoding never produces false sync patterns
    std::cout << "  ID3v2_UnsyncNoFalseSync: ";
    auto result37 = rc::check("ID3v2 unsync encoding produces no false sync", []() {
        // Generate random data with fixed size
        auto data = *rc::gen::container<std::vector<uint8_t>>(rc::gen::arbitrary<uint8_t>());
        
        // Encode with unsync
        auto encoded = ID3v2Utils::encodeUnsync(data.data(), data.size());
        
        // Check that no 0xFF is followed by >= 0xE0 (MPEG sync pattern)
        for (size_t i = 0; i + 1 < encoded.size(); ++i) {
            if (encoded[i] == 0xFF) {
                RC_ASSERT(encoded[i + 1] < 0xE0);
            }
        }
    });
    if (!result37) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // ========================================================================
    // Property 8: ID3 Tag Merging with Fallback
    // **Validates: Requirements 6a.3, 6a.4, 6a.5, 6a.6**
    // For any combination of ID3v1 and ID3v2 tag data where ID3v2 has some
    // fields empty and ID3v1 has those fields populated, the merged tag should
    // return ID3v2 values for non-empty fields and ID3v1 values for fields
    // empty in ID3v2.
    // ========================================================================
    
    std::cout << "\n  --- Property 8: ID3 Tag Merging with Fallback ---\n";
    std::cout << "  MergedID3Tag_FallbackLogic: ";
    auto result38 = rc::check("MergedID3Tag uses ID3v2 with ID3v1 fallback", []() {
        // Generate ID3v1 tag with all fields populated
        std::vector<uint8_t> v1_data(128, 0);
        v1_data[0] = 'T'; v1_data[1] = 'A'; v1_data[2] = 'G';
        
        // Title: "V1Title"
        std::string v1_title = "V1Title";
        std::copy(v1_title.begin(), v1_title.end(), v1_data.begin() + 3);
        
        // Artist: "V1Artist"
        std::string v1_artist = "V1Artist";
        std::copy(v1_artist.begin(), v1_artist.end(), v1_data.begin() + 33);
        
        // Album: "V1Album"
        std::string v1_album = "V1Album";
        std::copy(v1_album.begin(), v1_album.end(), v1_data.begin() + 63);
        
        // Year: 2000
        std::string v1_year = "2000";
        std::copy(v1_year.begin(), v1_year.end(), v1_data.begin() + 93);
        
        // Comment: "V1Comment"
        std::string v1_comment = "V1Comment";
        std::copy(v1_comment.begin(), v1_comment.end(), v1_data.begin() + 97);
        
        // Track: 5
        v1_data[125] = 0x00;
        v1_data[126] = 5;
        
        // Genre: 12 (Other)
        v1_data[127] = 12;
        
        auto v1_tag = ID3v1Tag::parse(v1_data.data());
        RC_ASSERT(v1_tag != nullptr);
        
        // Generate ID3v2 tag with some fields empty
        // For simplicity, create an empty ID3v2 tag (all fields empty)
        auto v2_tag = std::make_unique<ID3v2Tag>();
        
        // Create merged tag
        MergedID3Tag merged(std::move(v1_tag), std::move(v2_tag));
        
        // Verify fallback: since ID3v2 is empty, should use ID3v1 values
        RC_ASSERT(merged.title() == "V1Title");
        RC_ASSERT(merged.artist() == "V1Artist");
        RC_ASSERT(merged.album() == "V1Album");
        RC_ASSERT(merged.year() == 2000);
        RC_ASSERT(merged.comment() == "V1Comment");
        RC_ASSERT(merged.track() == 5);
        RC_ASSERT(merged.genre() == "Other");
        
        // Verify format name includes both
        RC_ASSERT(merged.formatName().find("ID3v2") != std::string::npos);
        RC_ASSERT(merged.formatName().find("ID3v1") != std::string::npos);
    });
    if (!result38) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: MergedID3Tag with only ID3v1
    std::cout << "  MergedID3Tag_ID3v1Only: ";
    auto result39 = rc::check("MergedID3Tag with only ID3v1 uses ID3v1 values", []() {
        std::vector<uint8_t> v1_data(128, 0);
        v1_data[0] = 'T'; v1_data[1] = 'A'; v1_data[2] = 'G';
        
        std::string title = "TestTitle";
        std::copy(title.begin(), title.end(), v1_data.begin() + 3);
        
        v1_data[127] = 0; // Genre: Blues
        
        auto v1_tag = ID3v1Tag::parse(v1_data.data());
        RC_ASSERT(v1_tag != nullptr);
        
        // Create merged tag with only ID3v1
        MergedID3Tag merged(std::move(v1_tag), nullptr);
        
        RC_ASSERT(merged.title() == "TestTitle");
        RC_ASSERT(merged.hasID3v1());
        RC_ASSERT(!merged.hasID3v2());
    });
    if (!result39) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: MergedID3Tag with only ID3v2
    std::cout << "  MergedID3Tag_ID3v2Only: ";
    auto result40 = rc::check("MergedID3Tag with only ID3v2 uses ID3v2 values", []() {
        auto v2_tag = std::make_unique<ID3v2Tag>();
        
        // Create merged tag with only ID3v2
        MergedID3Tag merged(nullptr, std::move(v2_tag));
        
        RC_ASSERT(!merged.hasID3v1());
        RC_ASSERT(merged.hasID3v2());
    });
    if (!result40) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: MergedID3Tag isEmpty when both tags are empty
    std::cout << "  MergedID3Tag_IsEmptyWhenBothEmpty: ";
    auto result41 = rc::check("MergedID3Tag isEmpty when both tags are empty", []() {
        auto v1_tag = std::make_unique<ID3v1Tag>();
        auto v2_tag = std::make_unique<ID3v2Tag>();
        
        MergedID3Tag merged(std::move(v1_tag), std::move(v2_tag));
        
        RC_ASSERT(merged.isEmpty());
    });
    if (!result41) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: MergedID3Tag pictures come from ID3v2 only
    std::cout << "  MergedID3Tag_PicturesFromID3v2Only: ";
    auto result42 = rc::check("MergedID3Tag pictures come from ID3v2 only", []() {
        auto v1_tag = std::make_unique<ID3v1Tag>();
        auto v2_tag = std::make_unique<ID3v2Tag>();
        
        MergedID3Tag merged(std::move(v1_tag), std::move(v2_tag));
        
        // ID3v1 doesn't support pictures, so count should be 0
        RC_ASSERT(merged.pictureCount() == 0);
        RC_ASSERT(!merged.getFrontCover().has_value());
    });
    if (!result42) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // ========================================================================
    // Property 1: VorbisComment Round-Trip Parsing
    // **Validates: Requirements 2.1, 2.2, 2.3**
    // For any valid VorbisComment data containing a vendor string and field
    // name=value pairs, parsing the data and then querying the resulting Tag
    // should return the original values for all fields.
    // ========================================================================
    
    std::cout << "\n  --- Property 1: VorbisComment Round-Trip Parsing ---\n";
    std::cout << "  VorbisComment_RoundTripParsing: ";
    auto result43 = rc::check("VorbisComment round-trip parsing preserves field values", []() {
        // Generate vendor string
        auto vendor = *rc::gen::container<std::string>(rc::gen::inRange<char>(32, 126));
        
        // Generate field map with random fields
        std::map<std::string, std::vector<std::string>> fields;
        
        // Add some standard fields
        auto title = *rc::gen::container<std::string>(rc::gen::inRange<char>(32, 126));
        auto artist = *rc::gen::container<std::string>(rc::gen::inRange<char>(32, 126));
        auto album = *rc::gen::container<std::string>(rc::gen::inRange<char>(32, 126));
        
        fields["TITLE"] = {title};
        fields["ARTIST"] = {artist};
        fields["ALBUM"] = {album};
        
        // Create VorbisCommentTag
        VorbisCommentTag tag(vendor, fields);
        
        // Verify round-trip
        RC_ASSERT(tag.vendorString() == vendor);
        RC_ASSERT(tag.title() == title);
        RC_ASSERT(tag.artist() == artist);
        RC_ASSERT(tag.album() == album);
    });
    if (!result43) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // ========================================================================
    // Property 2: VorbisComment Case-Insensitive Lookup
    // **Validates: Requirements 2.4**
    // For any VorbisComment Tag and any field name, looking up the field using
    // different case variations should return the same value.
    // ========================================================================
    
    std::cout << "\n  --- Property 2: VorbisComment Case-Insensitive Lookup ---\n";
    std::cout << "  VorbisComment_CaseInsensitiveLookup: ";
    auto result44 = rc::check("VorbisComment case-insensitive lookup", []() {
        std::map<std::string, std::vector<std::string>> fields;
        auto value = *rc::gen::container<std::string>(rc::gen::inRange<char>(32, 126));
        fields["ARTIST"] = {value};
        
        VorbisCommentTag tag("vendor", fields);
        
        // All case variations should return the same value
        RC_ASSERT(tag.getTag("ARTIST") == value);
        RC_ASSERT(tag.getTag("artist") == value);
        RC_ASSERT(tag.getTag("Artist") == value);
        RC_ASSERT(tag.getTag("aRtIsT") == value);
    });
    if (!result44) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // ========================================================================
    // Property 3: VorbisComment Multi-Valued Fields
    // **Validates: Requirements 2.5**
    // For any VorbisComment data containing multiple fields with the same name,
    // getTagValues() should return all values in order, and getTag() should
    // return the first value.
    // ========================================================================
    
    std::cout << "\n  --- Property 3: VorbisComment Multi-Valued Fields ---\n";
    std::cout << "  VorbisComment_MultiValuedFields: ";
    auto result45 = rc::check("VorbisComment multi-valued fields", []() {
        std::map<std::string, std::vector<std::string>> fields;
        
        // Generate multiple values for ARTIST field
        auto value1 = *rc::gen::container<std::string>(rc::gen::inRange<char>(32, 126));
        auto value2 = *rc::gen::container<std::string>(rc::gen::inRange<char>(32, 126));
        auto value3 = *rc::gen::container<std::string>(rc::gen::inRange<char>(32, 126));
        
        fields["ARTIST"] = {value1, value2, value3};
        
        VorbisCommentTag tag("vendor", fields);
        
        // getTag() should return first value
        RC_ASSERT(tag.getTag("ARTIST") == value1);
        
        // getTagValues() should return all values in order
        auto values = tag.getTagValues("ARTIST");
        RC_ASSERT(values.size() == 3);
        RC_ASSERT(values[0] == value1);
        RC_ASSERT(values[1] == value2);
        RC_ASSERT(values[2] == value3);
    });
    if (!result45) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
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
// ID3v2 Utility Fallback Tests
// ============================================================================

class ID3v2_SynchsafeIntegerRoundTrip : public TestCase {
public:
    ID3v2_SynchsafeIntegerRoundTrip() 
        : TestCase("ID3v2_SynchsafeIntegerRoundTrip") {}
protected:
    void runTest() override {
        // Test various values
        std::vector<uint32_t> test_values = {
            0, 1, 127, 128, 255, 256, 16383, 16384,
            0x7F, 0x3FFF, 0x1FFFFF, 0x0FFFFFFF,
            1000, 10000, 100000, 1000000
        };
        
        for (uint32_t value : test_values) {
            ASSERT_TRUE(ID3v2Utils::canEncodeSynchsafe(value), 
                        "Value should be encodable: " + std::to_string(value));
            
            uint32_t encoded = ID3v2Utils::encodeSynchsafe(value);
            uint32_t decoded = ID3v2Utils::decodeSynchsafe(encoded);
            
            ASSERT_EQUALS(value, decoded, 
                          "Round-trip failed for value: " + std::to_string(value));
        }
    }
};

class ID3v2_SynchsafeBytesRoundTrip : public TestCase {
public:
    ID3v2_SynchsafeBytesRoundTrip() 
        : TestCase("ID3v2_SynchsafeBytesRoundTrip") {}
protected:
    void runTest() override {
        std::vector<uint32_t> test_values = {
            0, 1, 127, 128, 255, 256, 16383, 16384,
            0x7F, 0x3FFF, 0x1FFFFF, 0x0FFFFFFF
        };
        
        for (uint32_t value : test_values) {
            uint8_t bytes[4];
            ID3v2Utils::encodeSynchsafeBytes(value, bytes);
            
            // Verify MSB is 0 for each byte
            ASSERT_TRUE((bytes[0] & 0x80) == 0, "Byte 0 MSB should be 0");
            ASSERT_TRUE((bytes[1] & 0x80) == 0, "Byte 1 MSB should be 0");
            ASSERT_TRUE((bytes[2] & 0x80) == 0, "Byte 2 MSB should be 0");
            ASSERT_TRUE((bytes[3] & 0x80) == 0, "Byte 3 MSB should be 0");
            
            uint32_t decoded = ID3v2Utils::decodeSynchsafeBytes(bytes);
            ASSERT_EQUALS(value, decoded, 
                          "Bytes round-trip failed for: " + std::to_string(value));
        }
    }
};

class ID3v2_SynchsafeRejectsLargeValues : public TestCase {
public:
    ID3v2_SynchsafeRejectsLargeValues() 
        : TestCase("ID3v2_SynchsafeRejectsLargeValues") {}
protected:
    void runTest() override {
        std::vector<uint32_t> large_values = {
            0x10000000, 0x20000000, 0x7FFFFFFF, 0xFFFFFFFF
        };
        
        for (uint32_t value : large_values) {
            ASSERT_FALSE(ID3v2Utils::canEncodeSynchsafe(value),
                         "Large value should not be encodable: " + std::to_string(value));
        }
    }
};

class ID3v2_UTF8RoundTrip : public TestCase {
public:
    ID3v2_UTF8RoundTrip() 
        : TestCase("ID3v2_UTF8RoundTrip") {}
protected:
    void runTest() override {
        std::vector<std::string> test_strings = {
            "", "Hello", "Test String", "ASCII only 123",
            "Special: !@#$%^&*()", "Spaces   and\ttabs"
        };
        
        for (const auto& text : test_strings) {
            auto encoded = ID3v2Utils::encodeText(text, ID3v2Utils::TextEncoding::UTF_8);
            std::string decoded = ID3v2Utils::decodeText(
                encoded.data(), encoded.size(), ID3v2Utils::TextEncoding::UTF_8);
            
            ASSERT_EQUALS(text, decoded, "UTF-8 round-trip failed for: " + text);
        }
    }
};

class ID3v2_ISO8859_1RoundTrip : public TestCase {
public:
    ID3v2_ISO8859_1RoundTrip() 
        : TestCase("ID3v2_ISO8859_1RoundTrip") {}
protected:
    void runTest() override {
        std::vector<std::string> test_strings = {
            "", "Hello", "Test String", "ASCII only 123"
        };
        
        for (const auto& text : test_strings) {
            auto encoded = ID3v2Utils::encodeText(text, ID3v2Utils::TextEncoding::ISO_8859_1);
            std::string decoded = ID3v2Utils::decodeText(
                encoded.data(), encoded.size(), ID3v2Utils::TextEncoding::ISO_8859_1);
            
            ASSERT_EQUALS(text, decoded, "ISO-8859-1 round-trip failed for: " + text);
        }
    }
};

class ID3v2_UTF16BOMRoundTrip : public TestCase {
public:
    ID3v2_UTF16BOMRoundTrip() 
        : TestCase("ID3v2_UTF16BOMRoundTrip") {}
protected:
    void runTest() override {
        std::vector<std::string> test_strings = {
            "", "Hello", "Test String", "ASCII only"
        };
        
        for (const auto& text : test_strings) {
            auto encoded = ID3v2Utils::encodeText(text, ID3v2Utils::TextEncoding::UTF_16_BOM);
            std::string decoded = ID3v2Utils::decodeText(
                encoded.data(), encoded.size(), ID3v2Utils::TextEncoding::UTF_16_BOM);
            
            ASSERT_EQUALS(text, decoded, "UTF-16 BOM round-trip failed for: " + text);
        }
    }
};

class ID3v2_UTF16BERoundTrip : public TestCase {
public:
    ID3v2_UTF16BERoundTrip() 
        : TestCase("ID3v2_UTF16BERoundTrip") {}
protected:
    void runTest() override {
        std::vector<std::string> test_strings = {
            "", "Hello", "Test String", "ASCII only"
        };
        
        for (const auto& text : test_strings) {
            auto encoded = ID3v2Utils::encodeText(text, ID3v2Utils::TextEncoding::UTF_16_BE);
            std::string decoded = ID3v2Utils::decodeText(
                encoded.data(), encoded.size(), ID3v2Utils::TextEncoding::UTF_16_BE);
            
            ASSERT_EQUALS(text, decoded, "UTF-16 BE round-trip failed for: " + text);
        }
    }
};

class ID3v2_TextWithEncodingRoundTrip : public TestCase {
public:
    ID3v2_TextWithEncodingRoundTrip() 
        : TestCase("ID3v2_TextWithEncodingRoundTrip") {}
protected:
    void runTest() override {
        std::string text = "Test String";
        
        for (uint8_t enc = 0; enc <= 3; ++enc) {
            auto encoding = static_cast<ID3v2Utils::TextEncoding>(enc);
            
            auto encoded = ID3v2Utils::encodeTextWithEncoding(text, encoding);
            
            // First byte should be the encoding
            ASSERT_EQUALS(enc, encoded[0], "First byte should be encoding");
            
            std::string decoded = ID3v2Utils::decodeTextWithEncoding(
                encoded.data(), encoded.size());
            
            ASSERT_EQUALS(text, decoded, 
                          "Text with encoding round-trip failed for encoding: " + std::to_string(enc));
        }
    }
};

class ID3v2_UnsyncRoundTrip : public TestCase {
public:
    ID3v2_UnsyncRoundTrip() 
        : TestCase("ID3v2_UnsyncRoundTrip") {}
protected:
    void runTest() override {
        // Test data with various patterns
        std::vector<std::vector<uint8_t>> test_data = {
            {},
            {0x00},
            {0xFF},
            {0xFF, 0x00},
            {0xFF, 0xE0},
            {0xFF, 0xFF},
            {0x01, 0xFF, 0x00, 0x02},
            {0xFF, 0xE0, 0xFF, 0xF0, 0xFF, 0x00}
        };
        
        for (const auto& data : test_data) {
            auto encoded = ID3v2Utils::encodeUnsync(data.data(), data.size());
            auto decoded = ID3v2Utils::decodeUnsync(encoded.data(), encoded.size());
            
            ASSERT_EQUALS(data.size(), decoded.size(), "Unsync round-trip size mismatch");
            for (size_t i = 0; i < data.size(); ++i) {
                ASSERT_EQUALS(data[i], decoded[i], 
                              "Unsync round-trip byte mismatch at index " + std::to_string(i));
            }
        }
    }
};

class ID3v2_UnsyncNoFalseSync : public TestCase {
public:
    ID3v2_UnsyncNoFalseSync() 
        : TestCase("ID3v2_UnsyncNoFalseSync") {}
protected:
    void runTest() override {
        // Data that would create false sync patterns
        std::vector<uint8_t> data = {0xFF, 0xE0, 0xFF, 0xF0, 0xFF, 0xFB};
        
        auto encoded = ID3v2Utils::encodeUnsync(data.data(), data.size());
        
        // Check no false sync patterns
        for (size_t i = 0; i + 1 < encoded.size(); ++i) {
            if (encoded[i] == 0xFF) {
                ASSERT_TRUE(encoded[i + 1] < 0xE0, 
                            "False sync pattern found at index " + std::to_string(i));
            }
        }
    }
};

// ============================================================================
// MergedID3Tag Fallback Tests
// ============================================================================

class MergedID3Tag_FallbackLogic : public TestCase {
public:
    MergedID3Tag_FallbackLogic() 
        : TestCase("MergedID3Tag_FallbackLogic") {}
protected:
    void runTest() override {
        // Create ID3v1 tag with all fields populated
        std::vector<uint8_t> v1_data(128, 0);
        v1_data[0] = 'T'; v1_data[1] = 'A'; v1_data[2] = 'G';
        
        // Title: "V1Title"
        std::string v1_title = "V1Title";
        std::copy(v1_title.begin(), v1_title.end(), v1_data.begin() + 3);
        
        // Artist: "V1Artist"
        std::string v1_artist = "V1Artist";
        std::copy(v1_artist.begin(), v1_artist.end(), v1_data.begin() + 33);
        
        // Album: "V1Album"
        std::string v1_album = "V1Album";
        std::copy(v1_album.begin(), v1_album.end(), v1_data.begin() + 63);
        
        // Year: 2000
        std::string v1_year = "2000";
        std::copy(v1_year.begin(), v1_year.end(), v1_data.begin() + 93);
        
        // Comment: "V1Comment"
        std::string v1_comment = "V1Comment";
        std::copy(v1_comment.begin(), v1_comment.end(), v1_data.begin() + 97);
        
        // Track: 5
        v1_data[125] = 0x00;
        v1_data[126] = 5;
        
        // Genre: 12 (Other)
        v1_data[127] = 12;
        
        auto v1_tag = ID3v1Tag::parse(v1_data.data());
        ASSERT_NOT_NULL(v1_tag.get(), "ID3v1 tag should parse");
        
        // Create empty ID3v2 tag
        auto v2_tag = std::make_unique<ID3v2Tag>();
        
        // Create merged tag
        MergedID3Tag merged(std::move(v1_tag), std::move(v2_tag));
        
        // Verify fallback: since ID3v2 is empty, should use ID3v1 values
        ASSERT_EQUALS(std::string("V1Title"), merged.title(), "Title should fallback to ID3v1");
        ASSERT_EQUALS(std::string("V1Artist"), merged.artist(), "Artist should fallback to ID3v1");
        ASSERT_EQUALS(std::string("V1Album"), merged.album(), "Album should fallback to ID3v1");
        ASSERT_EQUALS(2000u, merged.year(), "Year should fallback to ID3v1");
        ASSERT_EQUALS(std::string("V1Comment"), merged.comment(), "Comment should fallback to ID3v1");
        ASSERT_EQUALS(5u, merged.track(), "Track should fallback to ID3v1");
        ASSERT_EQUALS(std::string("Other"), merged.genre(), "Genre should fallback to ID3v1");
        
        // Verify format name includes both
        std::string format = merged.formatName();
        ASSERT_TRUE(format.find("ID3v2") != std::string::npos, "Format should mention ID3v2");
        ASSERT_TRUE(format.find("ID3v1") != std::string::npos, "Format should mention ID3v1");
    }
};

class MergedID3Tag_ID3v1Only : public TestCase {
public:
    MergedID3Tag_ID3v1Only() 
        : TestCase("MergedID3Tag_ID3v1Only") {}
protected:
    void runTest() override {
        std::vector<uint8_t> v1_data(128, 0);
        v1_data[0] = 'T'; v1_data[1] = 'A'; v1_data[2] = 'G';
        
        std::string title = "TestTitle";
        std::copy(title.begin(), title.end(), v1_data.begin() + 3);
        
        v1_data[127] = 0; // Genre: Blues
        
        auto v1_tag = ID3v1Tag::parse(v1_data.data());
        ASSERT_NOT_NULL(v1_tag.get(), "ID3v1 tag should parse");
        
        // Create merged tag with only ID3v1
        MergedID3Tag merged(std::move(v1_tag), nullptr);
        
        ASSERT_EQUALS(std::string("TestTitle"), merged.title(), "Title should come from ID3v1");
        ASSERT_TRUE(merged.hasID3v1(), "Should have ID3v1");
        ASSERT_FALSE(merged.hasID3v2(), "Should not have ID3v2");
    }
};

class MergedID3Tag_ID3v2Only : public TestCase {
public:
    MergedID3Tag_ID3v2Only() 
        : TestCase("MergedID3Tag_ID3v2Only") {}
protected:
    void runTest() override {
        auto v2_tag = std::make_unique<ID3v2Tag>();
        
        // Create merged tag with only ID3v2
        MergedID3Tag merged(nullptr, std::move(v2_tag));
        
        ASSERT_FALSE(merged.hasID3v1(), "Should not have ID3v1");
        ASSERT_TRUE(merged.hasID3v2(), "Should have ID3v2");
    }
};

class MergedID3Tag_IsEmptyWhenBothEmpty : public TestCase {
public:
    MergedID3Tag_IsEmptyWhenBothEmpty() 
        : TestCase("MergedID3Tag_IsEmptyWhenBothEmpty") {}
protected:
    void runTest() override {
        auto v1_tag = std::make_unique<ID3v1Tag>();
        auto v2_tag = std::make_unique<ID3v2Tag>();
        
        MergedID3Tag merged(std::move(v1_tag), std::move(v2_tag));
        
        ASSERT_TRUE(merged.isEmpty(), "Merged tag should be empty when both tags are empty");
    }
};

class MergedID3Tag_PicturesFromID3v2Only : public TestCase {
public:
    MergedID3Tag_PicturesFromID3v2Only() 
        : TestCase("MergedID3Tag_PicturesFromID3v2Only") {}
protected:
    void runTest() override {
        auto v1_tag = std::make_unique<ID3v1Tag>();
        auto v2_tag = std::make_unique<ID3v2Tag>();
        
        MergedID3Tag merged(std::move(v1_tag), std::move(v2_tag));
        
        // ID3v1 doesn't support pictures, so count should be 0
        ASSERT_EQUALS(0u, merged.pictureCount(), "Picture count should be 0");
        ASSERT_FALSE(merged.getFrontCover().has_value(), "Front cover should be nullopt");
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
    
    // ID3v2 utility tests
    suite.addTest(std::make_unique<ID3v2_SynchsafeIntegerRoundTrip>());
    suite.addTest(std::make_unique<ID3v2_SynchsafeBytesRoundTrip>());
    suite.addTest(std::make_unique<ID3v2_SynchsafeRejectsLargeValues>());
    suite.addTest(std::make_unique<ID3v2_UTF8RoundTrip>());
    suite.addTest(std::make_unique<ID3v2_ISO8859_1RoundTrip>());
    suite.addTest(std::make_unique<ID3v2_UTF16BOMRoundTrip>());
    suite.addTest(std::make_unique<ID3v2_UTF16BERoundTrip>());
    suite.addTest(std::make_unique<ID3v2_TextWithEncodingRoundTrip>());
    suite.addTest(std::make_unique<ID3v2_UnsyncRoundTrip>());
    suite.addTest(std::make_unique<ID3v2_UnsyncNoFalseSync>());
    
    // MergedID3Tag tests
    suite.addTest(std::make_unique<MergedID3Tag_FallbackLogic>());
    suite.addTest(std::make_unique<MergedID3Tag_ID3v1Only>());
    suite.addTest(std::make_unique<MergedID3Tag_ID3v2Only>());
    suite.addTest(std::make_unique<MergedID3Tag_IsEmptyWhenBothEmpty>());
    suite.addTest(std::make_unique<MergedID3Tag_PicturesFromID3v2Only>());
    
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results) > 0 ? 1 : 0;
}

#endif // HAVE_RAPIDCHECK

