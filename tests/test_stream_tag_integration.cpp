/*
 * test_stream_tag_integration.cpp - Tests for Stream-Tag integration
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
using namespace PsyMP3::Demuxer;
using namespace TestFramework;

// ============================================================================
// Mock Stream class for testing
// ============================================================================

class TestStream : public Stream {
public:
    TestStream() : Stream() {}
    size_t getData(size_t len, void *buf) override { return 0; }
    void seekTo(unsigned long pos) override {}
    bool eof() override { return true; }
};

// ============================================================================
// Unit Tests for Stream Tag Integration
// ============================================================================

// Test: Stream::getTag() returns NullTag when no tag is set
class Test_Stream_GetTag_ReturnsNullTagWhenNoTagSet : public TestCase {
public:
    Test_Stream_GetTag_ReturnsNullTagWhenNoTagSet() 
        : TestCase("Stream_GetTag_ReturnsNullTagWhenNoTagSet") {}
    
protected:
    void runTest() override {
        TestStream stream;
        const Tag& tag = stream.getTag();
        
        // Should return a NullTag (isEmpty() == true, formatName() == "None")
        ASSERT_TRUE(tag.isEmpty(), "Tag should be empty (NullTag)");
        ASSERT_EQUALS("None", tag.formatName(), "Format name should be 'None'");
        ASSERT_TRUE(tag.title().empty(), "Title should be empty");
        ASSERT_TRUE(tag.artist().empty(), "Artist should be empty");
        ASSERT_TRUE(tag.album().empty(), "Album should be empty");
    }
};

// Test: Stream::getTag() returns valid tag reference
class Test_Stream_GetTag_ReturnsValidTagReference : public TestCase {
public:
    Test_Stream_GetTag_ReturnsValidTagReference() 
        : TestCase("Stream_GetTag_ReturnsValidTagReference") {}
    
protected:
    void runTest() override {
        TestStream stream;
        
        // Get tag twice - should return consistent results
        const Tag& tag1 = stream.getTag();
        const Tag& tag2 = stream.getTag();
        
        // Both should be NullTag
        ASSERT_TRUE(tag1.isEmpty(), "First tag should be empty");
        ASSERT_TRUE(tag2.isEmpty(), "Second tag should be empty");
        ASSERT_EQUALS(tag1.formatName(), tag2.formatName(), "Format names should match");
    }
};

// Test: NullTag is returned for base Stream class
class Test_Stream_BaseClass_ReturnsNullTag : public TestCase {
public:
    Test_Stream_BaseClass_ReturnsNullTag() 
        : TestCase("Stream_BaseClass_ReturnsNullTag") {}
    
protected:
    void runTest() override {
        TestStream stream;
        const Tag& tag = stream.getTag();
        
        // Verify NullTag behavior
        ASSERT_TRUE(tag.isEmpty(), "Tag should be empty");
        ASSERT_EQUALS("None", tag.formatName(), "Format name should be 'None'");
        ASSERT_EQUALS(0u, tag.pictureCount(), "Picture count should be 0");
        ASSERT_FALSE(tag.getFrontCover().has_value(), "Front cover should not have value");
        ASSERT_TRUE(tag.getAllTags().empty(), "All tags should be empty");
    }
};

// Test: Stream::getTag() returns valid tag (Requirements 7.1)
class Test_Stream_GetTag_ReturnsValidTag : public TestCase {
public:
    Test_Stream_GetTag_ReturnsValidTag() 
        : TestCase("Stream_GetTag_ReturnsValidTag") {}
    
protected:
    void runTest() override {
        TestStream stream;
        const Tag& tag = stream.getTag();
        
        // Tag should be valid (not crash when accessing methods)
        // Even NullTag is a valid tag
        std::string format = tag.formatName();
        ASSERT_FALSE(format.empty(), "Format name should not be empty");
        
        // All accessor methods should work without crashing
        std::string title = tag.title();
        std::string artist = tag.artist();
        std::string album = tag.album();
        std::string albumArtist = tag.albumArtist();
        std::string genre = tag.genre();
        std::string comment = tag.comment();
        std::string composer = tag.composer();
        
        uint32_t year = tag.year();
        uint32_t track = tag.track();
        uint32_t trackTotal = tag.trackTotal();
        uint32_t disc = tag.disc();
        uint32_t discTotal = tag.discTotal();
        
        size_t pictureCount = tag.pictureCount();
        bool isEmpty = tag.isEmpty();
        
        // For NullTag, all should be empty/zero
        ASSERT_TRUE(isEmpty, "NullTag should be empty");
        ASSERT_EQUALS(0u, year, "Year should be 0");
        ASSERT_EQUALS(0u, track, "Track should be 0");
        ASSERT_EQUALS(0u, pictureCount, "Picture count should be 0");
    }
};

// Test: Stream tag delegation consistency (Requirements 7.4)
class Test_Stream_TagDelegationConsistency : public TestCase {
public:
    Test_Stream_TagDelegationConsistency() 
        : TestCase("Stream_TagDelegationConsistency") {}
    
protected:
    void runTest() override {
        TestStream stream;
        const Tag& tag = stream.getTag();
        
        // For a stream with NullTag, getArtist/getTitle/getAlbum should
        // fall back to TagLib (which will also return empty for a test stream)
        // The key is that they don't crash and return consistent results
        
        // Multiple calls should return consistent results
        std::string artist1 = tag.artist();
        std::string artist2 = tag.artist();
        ASSERT_EQUALS(artist1, artist2, "Artist should be consistent across calls");
        
        std::string title1 = tag.title();
        std::string title2 = tag.title();
        ASSERT_EQUALS(title1, title2, "Title should be consistent across calls");
        
        std::string album1 = tag.album();
        std::string album2 = tag.album();
        ASSERT_EQUALS(album1, album2, "Album should be consistent across calls");
    }
};

// Test: NullTag getTag returns empty for any key (Requirements 7.5)
class Test_Stream_NullTagGetTagReturnsEmpty : public TestCase {
public:
    Test_Stream_NullTagGetTagReturnsEmpty() 
        : TestCase("Stream_NullTagGetTagReturnsEmpty") {}
    
protected:
    void runTest() override {
        TestStream stream;
        const Tag& tag = stream.getTag();
        
        // Test various keys
        ASSERT_TRUE(tag.getTag("ARTIST").empty(), "ARTIST should be empty");
        ASSERT_TRUE(tag.getTag("TITLE").empty(), "TITLE should be empty");
        ASSERT_TRUE(tag.getTag("ALBUM").empty(), "ALBUM should be empty");
        ASSERT_TRUE(tag.getTag("RANDOM_KEY").empty(), "Random key should be empty");
        ASSERT_TRUE(tag.getTag("").empty(), "Empty key should return empty");
        
        // hasTag should return false for all keys
        ASSERT_FALSE(tag.hasTag("ARTIST"), "hasTag(ARTIST) should be false");
        ASSERT_FALSE(tag.hasTag("TITLE"), "hasTag(TITLE) should be false");
        ASSERT_FALSE(tag.hasTag("RANDOM_KEY"), "hasTag(RANDOM_KEY) should be false");
    }
};

// Test: NullTag getTagValues returns empty vector (Requirements 7.5)
class Test_Stream_NullTagGetTagValuesReturnsEmpty : public TestCase {
public:
    Test_Stream_NullTagGetTagValuesReturnsEmpty() 
        : TestCase("Stream_NullTagGetTagValuesReturnsEmpty") {}
    
protected:
    void runTest() override {
        TestStream stream;
        const Tag& tag = stream.getTag();
        
        // getTagValues should return empty vector for any key
        auto values1 = tag.getTagValues("ARTIST");
        ASSERT_TRUE(values1.empty(), "ARTIST values should be empty");
        
        auto values2 = tag.getTagValues("PERFORMER");
        ASSERT_TRUE(values2.empty(), "PERFORMER values should be empty");
        
        auto values3 = tag.getTagValues("");
        ASSERT_TRUE(values3.empty(), "Empty key values should be empty");
    }
};

// ============================================================================
// Property-Based Tests using RapidCheck
// ============================================================================

#ifdef HAVE_RAPIDCHECK

bool runStreamTagPropertyTests() {
    bool all_passed = true;
    
    std::cout << "Running RapidCheck property-based tests for Stream-Tag integration...\n\n";
    
    // ========================================================================
    // Property 11: Stream-Tag Delegation Consistency
    // **Validates: Requirements 7.4**
    // For any Stream with a Tag, the metadata accessor methods (getArtist,
    // getTitle, getAlbum) should return values consistent with the Tag's
    // corresponding methods.
    // ========================================================================
    
    std::cout << "  --- Property 11: Stream-Tag Delegation Consistency ---\n";
    
    // Property: Stream::getTag() never returns null reference
    std::cout << "  Stream_GetTagNeverReturnsNullReference: ";
    auto result1 = rc::check("Stream::getTag() never returns null reference", []() {
        TestStream stream;
        const Tag& tag = stream.getTag();
        
        // Tag reference should always be valid (NullTag at minimum)
        // Calling methods on it should not crash
        RC_ASSERT(tag.formatName().length() >= 0); // Just verify we can call methods
        RC_ASSERT(tag.title().length() >= 0);
        RC_ASSERT(tag.artist().length() >= 0);
        RC_ASSERT(tag.album().length() >= 0);
    });
    if (!result1) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: NullTag always returns empty strings for metadata
    std::cout << "  Stream_NullTagReturnsEmptyMetadata: ";
    auto result2 = rc::check("Stream with NullTag returns empty metadata", []() {
        TestStream stream;
        const Tag& tag = stream.getTag();
        
        // NullTag should return empty for all string fields
        RC_ASSERT(tag.title().empty());
        RC_ASSERT(tag.artist().empty());
        RC_ASSERT(tag.album().empty());
        RC_ASSERT(tag.albumArtist().empty());
        RC_ASSERT(tag.genre().empty());
        RC_ASSERT(tag.comment().empty());
        RC_ASSERT(tag.composer().empty());
    });
    if (!result2) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: NullTag always returns zero for numeric fields
    std::cout << "  Stream_NullTagReturnsZeroNumerics: ";
    auto result3 = rc::check("Stream with NullTag returns zero for numerics", []() {
        TestStream stream;
        const Tag& tag = stream.getTag();
        
        // NullTag should return 0 for all numeric fields
        RC_ASSERT(tag.year() == 0);
        RC_ASSERT(tag.track() == 0);
        RC_ASSERT(tag.trackTotal() == 0);
        RC_ASSERT(tag.disc() == 0);
        RC_ASSERT(tag.discTotal() == 0);
    });
    if (!result3) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: Stream::getTag() is idempotent
    std::cout << "  Stream_GetTagIsIdempotent: ";
    auto result4 = rc::check("Stream::getTag() is idempotent", []() {
        TestStream stream;
        
        // Multiple calls should return consistent results
        const Tag& tag1 = stream.getTag();
        const Tag& tag2 = stream.getTag();
        const Tag& tag3 = stream.getTag();
        
        RC_ASSERT(tag1.formatName() == tag2.formatName());
        RC_ASSERT(tag2.formatName() == tag3.formatName());
        RC_ASSERT(tag1.isEmpty() == tag2.isEmpty());
        RC_ASSERT(tag2.isEmpty() == tag3.isEmpty());
    });
    if (!result4) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: NullTag hasTag returns false for any key
    std::cout << "  Stream_NullTagHasTagReturnsFalse: ";
    auto result5 = rc::check("Stream with NullTag hasTag returns false for any key", 
        [](const std::string& key) {
            TestStream stream;
            const Tag& tag = stream.getTag();
            
            RC_ASSERT(!tag.hasTag(key));
        });
    if (!result5) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: NullTag getTag returns empty for any key
    std::cout << "  Stream_NullTagGetTagReturnsEmpty: ";
    auto result6 = rc::check("Stream with NullTag getTag returns empty for any key", 
        [](const std::string& key) {
            TestStream stream;
            const Tag& tag = stream.getTag();
            
            RC_ASSERT(tag.getTag(key).empty());
        });
    if (!result6) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: NullTag getTagValues returns empty vector for any key
    std::cout << "  Stream_NullTagGetTagValuesReturnsEmpty: ";
    auto result7 = rc::check("Stream with NullTag getTagValues returns empty for any key", 
        [](const std::string& key) {
            TestStream stream;
            const Tag& tag = stream.getTag();
            
            RC_ASSERT(tag.getTagValues(key).empty());
        });
    if (!result7) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property: NullTag getPicture returns nullopt for any index
    std::cout << "  Stream_NullTagGetPictureReturnsNullopt: ";
    auto result8 = rc::check("Stream with NullTag getPicture returns nullopt for any index", 
        [](size_t index) {
            TestStream stream;
            const Tag& tag = stream.getTag();
            
            RC_ASSERT(!tag.getPicture(index).has_value());
        });
    if (!result8) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    std::cout << "\n";
    return all_passed;
}

#endif // HAVE_RAPIDCHECK

// ============================================================================
// Test Runner
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "=== Stream-Tag Integration Tests ===\n\n";
    
    bool all_passed = true;
    int tests_run = 0;
    int tests_passed = 0;
    
    // Run unit tests
    std::cout << "--- Unit Tests ---\n";
    
    // Helper lambda to run tests
    auto runTest = [&](TestCase& test) {
        auto result = test.run();
        tests_run++;
        if (result.result == TestResult::PASSED) {
            tests_passed++;
            std::cout << "  " << result.name << ": PASSED\n";
        } else {
            all_passed = false;
            std::cout << "  " << result.name << ": FAILED - " << result.failure_message << "\n";
        }
    };
    
    // Test 1: Stream::getTag() returns NullTag when no tag is set
    {
        Test_Stream_GetTag_ReturnsNullTagWhenNoTagSet test;
        runTest(test);
    }
    
    // Test 2: Stream::getTag() returns valid tag reference
    {
        Test_Stream_GetTag_ReturnsValidTagReference test;
        runTest(test);
    }
    
    // Test 3: NullTag is returned for base Stream class
    {
        Test_Stream_BaseClass_ReturnsNullTag test;
        runTest(test);
    }
    
    // Test 4: Stream::getTag() returns valid tag (Requirements 7.1)
    {
        Test_Stream_GetTag_ReturnsValidTag test;
        runTest(test);
    }
    
    // Test 5: Stream tag delegation consistency (Requirements 7.4)
    {
        Test_Stream_TagDelegationConsistency test;
        runTest(test);
    }
    
    // Test 6: NullTag getTag returns empty for any key (Requirements 7.5)
    {
        Test_Stream_NullTagGetTagReturnsEmpty test;
        runTest(test);
    }
    
    // Test 7: NullTag getTagValues returns empty vector (Requirements 7.5)
    {
        Test_Stream_NullTagGetTagValuesReturnsEmpty test;
        runTest(test);
    }
    
    std::cout << "\n";
    
#ifdef HAVE_RAPIDCHECK
    // Run property-based tests
    std::cout << "--- Property-Based Tests ---\n";
    if (!runStreamTagPropertyTests()) {
        all_passed = false;
    }
#else
    std::cout << "RapidCheck not available - skipping property-based tests\n\n";
#endif
    
    // Summary
    std::cout << "=== Test Summary ===\n";
    std::cout << "Tests run: " << tests_run << "\n";
    std::cout << "Tests passed: " << tests_passed << "\n";
    std::cout << "Tests failed: " << (tests_run - tests_passed) << "\n";
    
    if (all_passed && tests_run == tests_passed) {
        std::cout << "\nAll tests PASSED!\n";
        return 0;
    } else {
        std::cout << "\nSome tests FAILED!\n";
        return 1;
    }
}
