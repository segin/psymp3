/*
 * test_utf8util_unit.cpp - Unit tests for UTF8Util
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif
#include "test_framework.h"
#include "core/utility/UTF8Util.h"
#include <cstring>

using namespace PsyMP3::Core::Utility;
using namespace TestFramework;

// ============================================================================
// UTF-8 Validation Tests
// ============================================================================

class UTF8ValidationTest : public TestCase {
public:
    UTF8ValidationTest() : TestCase("UTF8Util::isValid") {}
    
protected:
    void runTest() override {
        // Valid ASCII
        ASSERT_TRUE(UTF8Util::isValid("Hello, World!"), "ASCII should be valid UTF-8");
        ASSERT_TRUE(UTF8Util::isValid(""), "Empty string should be valid UTF-8");
        
        // Valid multi-byte sequences
        ASSERT_TRUE(UTF8Util::isValid("café"), "2-byte UTF-8 should be valid");
        ASSERT_TRUE(UTF8Util::isValid("日本語"), "3-byte UTF-8 should be valid");
        ASSERT_TRUE(UTF8Util::isValid("🎵🎶"), "4-byte UTF-8 (emoji) should be valid");
        ASSERT_TRUE(UTF8Util::isValid("Mixed: café 日本語 🎵"), "Mixed UTF-8 should be valid");
        
        // Invalid sequences
        std::string invalid1 = "\x80"; // Continuation byte without start
        ASSERT_FALSE(UTF8Util::isValid(invalid1), "Orphan continuation byte should be invalid");
        
        std::string invalid2 = "\xC0\x80"; // Overlong encoding of NUL
        ASSERT_FALSE(UTF8Util::isValid(invalid2), "Overlong encoding should be invalid");
        
        std::string invalid3 = "\xED\xA0\x80"; // Surrogate (U+D800)
        ASSERT_FALSE(UTF8Util::isValid(invalid3), "Surrogate codepoint should be invalid");
        
        std::string invalid4 = "\xF5\x80\x80\x80"; // Beyond U+10FFFF
        ASSERT_FALSE(UTF8Util::isValid(invalid4), "Codepoint > U+10FFFF should be invalid");
    }
};

// ============================================================================
// UTF-8 Repair Tests
// ============================================================================

class UTF8RepairTest : public TestCase {
public:
    UTF8RepairTest() : TestCase("UTF8Util::repair") {}
    
protected:
    void runTest() override {
        // Valid strings should pass through unchanged
        ASSERT_EQUALS("Hello", UTF8Util::repair("Hello"), "Valid ASCII unchanged");
        ASSERT_EQUALS("café", UTF8Util::repair("café"), "Valid UTF-8 unchanged");
        
        // Invalid sequences should be replaced with U+FFFD
        std::string invalid = "Hello\x80World";
        std::string repaired = UTF8Util::repair(invalid);
        ASSERT_TRUE(repaired.find("\xEF\xBF\xBD") != std::string::npos, 
                   "Invalid byte should be replaced with U+FFFD");
        ASSERT_TRUE(repaired.find("Hello") != std::string::npos, "Valid part preserved");
        ASSERT_TRUE(repaired.find("World") != std::string::npos, "Valid part preserved");
    }
};

// ============================================================================
// Latin-1 Conversion Tests
// ============================================================================

class Latin1ConversionTest : public TestCase {
public:
    Latin1ConversionTest() : TestCase("UTF8Util::Latin1 conversion") {}
    
protected:
    void runTest() override {
        // ASCII passthrough
        uint8_t ascii[] = {'H', 'e', 'l', 'l', 'o'};
        ASSERT_EQUALS("Hello", UTF8Util::fromLatin1(ascii, 5), "ASCII passthrough");
        
        // Latin-1 extended characters
        uint8_t latin1[] = {0xE9}; // é in Latin-1
        std::string utf8 = UTF8Util::fromLatin1(latin1, 1);
        ASSERT_EQUALS("é", utf8, "Latin-1 é converts to UTF-8");
        
        // Round-trip for Latin-1 compatible characters
        std::string original = "café";
        auto encoded = UTF8Util::toLatin1(original);
        std::string decoded = UTF8Util::fromLatin1(encoded.data(), encoded.size());
        ASSERT_EQUALS(original, decoded, "Latin-1 round-trip for compatible chars");
        
        // Non-Latin-1 characters become '?'
        std::string japanese = "日本語";
        auto latin1Encoded = UTF8Util::toLatin1(japanese);
        ASSERT_TRUE(latin1Encoded.size() == 3, "3 chars become 3 '?'");
        ASSERT_TRUE(latin1Encoded[0] == '?', "Non-Latin-1 becomes '?'");
    }
};


// ============================================================================
// UTF-16 Conversion Tests
// ============================================================================

class UTF16ConversionTest : public TestCase {
public:
    UTF16ConversionTest() : TestCase("UTF8Util::UTF16 conversion") {}
    
protected:
    void runTest() override {
        // UTF-16LE BMP character
        uint8_t utf16le_a[] = {0x41, 0x00}; // 'A' in UTF-16LE
        ASSERT_EQUALS("A", UTF8Util::fromUTF16LE(utf16le_a, 2), "UTF-16LE ASCII");
        
        // UTF-16BE BMP character
        uint8_t utf16be_a[] = {0x00, 0x41}; // 'A' in UTF-16BE
        ASSERT_EQUALS("A", UTF8Util::fromUTF16BE(utf16be_a, 2), "UTF-16BE ASCII");
        
        // UTF-16LE with BOM
        uint8_t utf16le_bom[] = {0xFF, 0xFE, 0x41, 0x00}; // BOM + 'A'
        ASSERT_EQUALS("A", UTF8Util::fromUTF16BOM(utf16le_bom, 4), "UTF-16LE with BOM");
        
        // UTF-16BE with BOM
        uint8_t utf16be_bom[] = {0xFE, 0xFF, 0x00, 0x41}; // BOM + 'A'
        ASSERT_EQUALS("A", UTF8Util::fromUTF16BOM(utf16be_bom, 4), "UTF-16BE with BOM");
        
        // Surrogate pair (emoji U+1F3B5 = 🎵)
        uint8_t utf16le_emoji[] = {0x3C, 0xD8, 0xB5, 0xDF}; // U+1F3B5 in UTF-16LE
        std::string emoji = UTF8Util::fromUTF16LE(utf16le_emoji, 4);
        ASSERT_EQUALS("🎵", emoji, "UTF-16LE surrogate pair");
        
        // Round-trip test
        std::string original = "Hello 世界 🎵";
        auto utf16 = UTF8Util::toUTF16LE(original);
        std::string decoded = UTF8Util::fromUTF16LE(utf16.data(), utf16.size());
        ASSERT_EQUALS(original, decoded, "UTF-16LE round-trip");
        
        // UTF-16BE round-trip
        auto utf16be = UTF8Util::toUTF16BE(original);
        std::string decodedBE = UTF8Util::fromUTF16BE(utf16be.data(), utf16be.size());
        ASSERT_EQUALS(original, decodedBE, "UTF-16BE round-trip");
    }
};

// ============================================================================
// UTF-32 Conversion Tests
// ============================================================================

class UTF32ConversionTest : public TestCase {
public:
    UTF32ConversionTest() : TestCase("UTF8Util::UTF32 conversion") {}
    
protected:
    void runTest() override {
        // UTF-32LE ASCII
        uint8_t utf32le_a[] = {0x41, 0x00, 0x00, 0x00}; // 'A' in UTF-32LE
        ASSERT_EQUALS("A", UTF8Util::fromUTF32LE(utf32le_a, 4), "UTF-32LE ASCII");
        
        // UTF-32BE ASCII
        uint8_t utf32be_a[] = {0x00, 0x00, 0x00, 0x41}; // 'A' in UTF-32BE
        ASSERT_EQUALS("A", UTF8Util::fromUTF32BE(utf32be_a, 4), "UTF-32BE ASCII");
        
        // UTF-32LE emoji (U+1F3B5)
        uint8_t utf32le_emoji[] = {0xB5, 0xF3, 0x01, 0x00}; // U+1F3B5 in UTF-32LE
        ASSERT_EQUALS("🎵", UTF8Util::fromUTF32LE(utf32le_emoji, 4), "UTF-32LE emoji");
        
        // Round-trip test
        std::string original = "Test 日本語 🎶";
        auto utf32 = UTF8Util::toUTF32LE(original);
        std::string decoded = UTF8Util::fromUTF32LE(utf32.data(), utf32.size());
        ASSERT_EQUALS(original, decoded, "UTF-32LE round-trip");
    }
};

// ============================================================================
// Codepoint Operations Tests
// ============================================================================

class CodepointOperationsTest : public TestCase {
public:
    CodepointOperationsTest() : TestCase("UTF8Util::codepoint operations") {}
    
protected:
    void runTest() override {
        // Encode single codepoints
        ASSERT_EQUALS("A", UTF8Util::encodeCodepoint(0x41), "Encode ASCII");
        ASSERT_EQUALS("é", UTF8Util::encodeCodepoint(0xE9), "Encode 2-byte");
        ASSERT_EQUALS("日", UTF8Util::encodeCodepoint(0x65E5), "Encode 3-byte");
        ASSERT_EQUALS("🎵", UTF8Util::encodeCodepoint(0x1F3B5), "Encode 4-byte");
        
        // Decode codepoints
        size_t consumed;
        ASSERT_EQUALS(0x41u, UTF8Util::decodeCodepoint("ABC", consumed), "Decode ASCII");
        ASSERT_EQUALS(1u, consumed, "ASCII consumes 1 byte");
        
        ASSERT_EQUALS(0xE9u, UTF8Util::decodeCodepoint("é", consumed), "Decode 2-byte");
        ASSERT_EQUALS(2u, consumed, "2-byte consumes 2 bytes");
        
        ASSERT_EQUALS(0x65E5u, UTF8Util::decodeCodepoint("日", consumed), "Decode 3-byte");
        ASSERT_EQUALS(3u, consumed, "3-byte consumes 3 bytes");
        
        ASSERT_EQUALS(0x1F3B5u, UTF8Util::decodeCodepoint("🎵", consumed), "Decode 4-byte");
        ASSERT_EQUALS(4u, consumed, "4-byte consumes 4 bytes");
        
        // toCodepoints / fromCodepoints round-trip
        std::string original = "Hello 世界";
        auto codepoints = UTF8Util::toCodepoints(original);
        std::string reconstructed = UTF8Util::fromCodepoints(codepoints);
        ASSERT_EQUALS(original, reconstructed, "Codepoints round-trip");
        
        // Verify codepoint count
        ASSERT_EQUALS(8u, codepoints.size(), "8 codepoints in 'Hello 世界'");
    }
};

// ============================================================================
// String Utilities Tests
// ============================================================================

class StringUtilitiesTest : public TestCase {
public:
    StringUtilitiesTest() : TestCase("UTF8Util::string utilities") {}
    
protected:
    void runTest() override {
        // Character length (not byte length)
        ASSERT_EQUALS(5u, UTF8Util::length("Hello"), "ASCII length");
        ASSERT_EQUALS(4u, UTF8Util::length("café"), "Mixed length");
        ASSERT_EQUALS(3u, UTF8Util::length("日本語"), "CJK length");
        ASSERT_EQUALS(2u, UTF8Util::length("🎵🎶"), "Emoji length");
        
        // Null terminator finding
        uint8_t data1[] = {'H', 'e', 'l', 'l', 'o', 0, 'X'};
        ASSERT_EQUALS(5u, UTF8Util::findNullTerminator(data1, 7, 1), "Find single-byte null");
        
        uint8_t data2[] = {'H', 0, 'e', 0, 0, 0, 'X', 0};
        ASSERT_EQUALS(4u, UTF8Util::findNullTerminator(data2, 8, 2), "Find double-byte null");
        
        // Valid codepoint check
        ASSERT_TRUE(UTF8Util::isValidCodepoint(0x41), "ASCII is valid");
        ASSERT_TRUE(UTF8Util::isValidCodepoint(0x10FFFF), "Max codepoint is valid");
        ASSERT_FALSE(UTF8Util::isValidCodepoint(0xD800), "Surrogate is invalid");
        ASSERT_FALSE(UTF8Util::isValidCodepoint(0x110000), "Beyond max is invalid");
        
        // Replacement character
        ASSERT_EQUALS("\xEF\xBF\xBD", UTF8Util::replacementCharacter(), "Replacement char is U+FFFD");
    }
};

// ============================================================================
// UTF-8 DecodeSafe Tests
// ============================================================================

class UTF8DecodeSafeTest : public TestCase {
public:
    UTF8DecodeSafeTest() : TestCase("UTF8Util::decodeSafe") {}

protected:
    void runTest() override {
        // Null and empty input
        ASSERT_EQUALS("", UTF8Util::decodeSafe(nullptr, 0), "Null input with size 0");
        ASSERT_EQUALS("", UTF8Util::decodeSafe(nullptr, 10), "Null input with non-zero size");
        uint8_t dummy = 'A';
        ASSERT_EQUALS("", UTF8Util::decodeSafe(&dummy, 0), "Non-null input with size 0");

        // Valid ASCII
        const uint8_t ascii[] = {'H', 'e', 'l', 'l', 'o'};
        ASSERT_EQUALS("Hello", UTF8Util::decodeSafe(ascii, 5), "Valid ASCII without null terminator");
        const uint8_t ascii_null[] = {'H', 'e', 'l', 'l', 'o', 0}; // Includes null terminator
        ASSERT_EQUALS("Hello", UTF8Util::decodeSafe(ascii_null, 6), "Valid ASCII with null terminator");

        // Valid UTF-8
        const uint8_t utf8[] = {'c', 'a', 'f', static_cast<uint8_t>('\xC3'), static_cast<uint8_t>('\xA9')}; // café
        ASSERT_EQUALS("café", UTF8Util::decodeSafe(utf8, 5), "Valid UTF-8");

        // Null terminator before end of size
        const uint8_t with_null[] = {'H', 'i', 0, 'X', 'Y'};
        ASSERT_EQUALS("Hi", UTF8Util::decodeSafe(with_null, 5), "Null terminator respected within buffer");

        // Invalid UTF-8 should be repaired
        // 0x80 is an invalid start byte
        const uint8_t invalid[] = {'H', 0x80, 'W', 0};
        std::string repaired = UTF8Util::decodeSafe(invalid, 4);
        ASSERT_TRUE(repaired.find("\xEF\xBF\xBD") != std::string::npos, "Invalid byte replaced with replacement char");
        ASSERT_TRUE(repaired.find("H") == 0, "Valid prefix preserved");
        ASSERT_TRUE(repaired.find("W") != std::string::npos, "Valid suffix preserved");

        // Overlong encoding (invalid)
        const uint8_t overlong[] = {0xC0, 0x80, 0}; // Overlong NUL
        std::string repaired2 = UTF8Util::decodeSafe(overlong, 3);
        ASSERT_EQUALS(UTF8Util::replacementCharacter(), repaired2, "Overlong NUL repaired to single replacement char");
    }
};

// ============================================================================
// Edge Cases Tests
// ============================================================================

class EdgeCasesTest : public TestCase {
public:
    EdgeCasesTest() : TestCase("UTF8Util::edge cases") {}
    
protected:
    void runTest() override {
        // Empty inputs
        ASSERT_TRUE(UTF8Util::isValid(""), "Empty string is valid");
        ASSERT_EQUALS("", UTF8Util::fromLatin1(nullptr, 0), "Null Latin-1 input");
        ASSERT_EQUALS("", UTF8Util::fromUTF16LE(nullptr, 0), "Null UTF-16 input");
        ASSERT_EQUALS("", UTF8Util::fromUTF32LE(nullptr, 0), "Null UTF-32 input");
        
        // Boundary codepoints
        ASSERT_EQUALS("\x7F", UTF8Util::encodeCodepoint(0x7F), "Max 1-byte");
        ASSERT_EQUALS("\xC2\x80", UTF8Util::encodeCodepoint(0x80), "Min 2-byte");
        ASSERT_EQUALS("\xDF\xBF", UTF8Util::encodeCodepoint(0x7FF), "Max 2-byte");
        ASSERT_EQUALS("\xE0\xA0\x80", UTF8Util::encodeCodepoint(0x800), "Min 3-byte");
        ASSERT_EQUALS("\xEF\xBF\xBF", UTF8Util::encodeCodepoint(0xFFFF), "Max 3-byte");
        ASSERT_EQUALS("\xF0\x90\x80\x80", UTF8Util::encodeCodepoint(0x10000), "Min 4-byte");
        ASSERT_EQUALS("\xF4\x8F\xBF\xBF", UTF8Util::encodeCodepoint(0x10FFFF), "Max 4-byte");
        
        // Invalid codepoint encoding
        std::string invalid = UTF8Util::encodeCodepoint(0x200000); // Beyond max
        ASSERT_EQUALS(UTF8Util::replacementCharacter(), invalid, "Invalid codepoint becomes U+FFFD");
    }
};

// ============================================================================
// UTF-8 DecodeSafe Comprehensive Tests
// ============================================================================

class UTF8DecodeSafeComprehensiveTest : public TestCase {
public:
    UTF8DecodeSafeComprehensiveTest() : TestCase("UTF8Util::decodeSafe (Comprehensive)") {}

protected:
    void runTest() override {
        // 1. Truncated Multi-byte Sequences
        // ---------------------------------

        // Truncated 2-byte sequence (first byte only)
        // U+00E9 (é) is 0xC3 0xA9
        const uint8_t trunc2[] = {0xC3};
        ASSERT_EQUALS("\xEF\xBF\xBD", UTF8Util::decodeSafe(trunc2, 1), "Truncated 2-byte sequence");

        // Truncated 3-byte sequence (1 and 2 bytes)
        // U+20AC (€) is 0xE2 0x82 0xAC
        const uint8_t trunc3_1[] = {0xE2};
        ASSERT_EQUALS("\xEF\xBF\xBD", UTF8Util::decodeSafe(trunc3_1, 1), "Truncated 3-byte sequence (1 byte)");

        const uint8_t trunc3_2[] = {0xE2, 0x82};
        ASSERT_EQUALS("\xEF\xBF\xBD\xEF\xBF\xBD", UTF8Util::decodeSafe(trunc3_2, 2), "Truncated 3-byte sequence (2 bytes)");

        // Truncated 4-byte sequence (1, 2, and 3 bytes)
        // U+1F0A0 (🂠) is 0xF0 0x9F 0x82 0xA0
        const uint8_t trunc4_1[] = {0xF0};
        ASSERT_EQUALS("\xEF\xBF\xBD", UTF8Util::decodeSafe(trunc4_1, 1), "Truncated 4-byte sequence (1 byte)");

        const uint8_t trunc4_2[] = {0xF0, 0x9F};
        ASSERT_EQUALS("\xEF\xBF\xBD\xEF\xBF\xBD", UTF8Util::decodeSafe(trunc4_2, 2), "Truncated 4-byte sequence (2 bytes)");

        const uint8_t trunc4_3[] = {0xF0, 0x9F, 0x82};
        ASSERT_EQUALS("\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD", UTF8Util::decodeSafe(trunc4_3, 3), "Truncated 4-byte sequence (3 bytes)");

        // 2. Invalid Continuation Bytes
        // -----------------------------

        // Valid start followed by invalid continuation (ASCII)
        // 0xC3 0x41 ('A') -> Replacement char + 'A'
        const uint8_t bad_cont[] = {0xC3, 0x41};
        ASSERT_EQUALS("\xEF\xBF\xBD" "A", UTF8Util::decodeSafe(bad_cont, 2), "Invalid continuation byte (ASCII)");

        // Valid start followed by another start byte
        const uint8_t double_start[] = {0xC3, 0xC3};
        ASSERT_EQUALS("\xEF\xBF\xBD\xEF\xBF\xBD", UTF8Util::decodeSafe(double_start, 2), "Invalid continuation byte (Start byte)");

        // 3. Overlong Encodings
        // ---------------------
        // These are well-formed but non-shortest forms, forbidden by RFC 3629

        // Overlong ASCII '/' (0x2F). Encoded as 2 bytes: 0xC0 0xAF
        // Consumed=2, Result=0xFFFD (one replacement char)
        const uint8_t overlong2[] = {0xC0, 0xAF};
        ASSERT_EQUALS("\xEF\xBF\xBD", UTF8Util::decodeSafe(overlong2, 2), "Overlong 2-byte sequence");

        // Overlong 3-byte sequence (for U+00E9 'é' which should be 2 bytes)
        // 0xE0 0x83 0xA9
        // Consumed=3, Result=0xFFFD
        const uint8_t overlong3[] = {0xE0, 0x83, 0xA9};
        ASSERT_EQUALS("\xEF\xBF\xBD", UTF8Util::decodeSafe(overlong3, 3), "Overlong 3-byte sequence");

        // 4. Surrogate Pairs (Invalid in UTF-8)
        // -------------------------------------
        // High surrogate U+D800: 0xED 0xA0 0x80
        // Consumed=3, Result=0xFFFD
        const uint8_t surrogate[] = {0xED, 0xA0, 0x80};
        ASSERT_EQUALS("\xEF\xBF\xBD", UTF8Util::decodeSafe(surrogate, 3), "Surrogate pair (invalid in UTF-8)");

        // 5. Max Codepoint Boundary
        // -------------------------
        // U+10FFFF (Max Valid): 0xF4 0x8F 0xBF 0xBF
        const uint8_t max_valid[] = {0xF4, 0x8F, 0xBF, 0xBF};
        std::string max_valid_str(reinterpret_cast<const char*>(max_valid), 4);
        ASSERT_EQUALS(max_valid_str, UTF8Util::decodeSafe(max_valid, 4), "Max valid codepoint U+10FFFF");

        // U+110000 (First Invalid): 0xF4 0x90 0x80 0x80
        // Consumed=4, Result=0xFFFD
        const uint8_t first_invalid[] = {0xF4, 0x90, 0x80, 0x80};
        ASSERT_EQUALS("\xEF\xBF\xBD", UTF8Util::decodeSafe(first_invalid, 4), "Codepoint U+110000 (invalid)");

        // 6. Null Terminator Variants
        // ---------------------------
        // Null terminator early
        const uint8_t null_early[] = {'A', 0, 'B'};
        ASSERT_EQUALS("A", UTF8Util::decodeSafe(null_early, 3), "Null terminator early");

        // Null terminator at end
        const uint8_t null_end[] = {'A', 'B', 0};
        ASSERT_EQUALS("AB", UTF8Util::decodeSafe(null_end, 3), "Null terminator at end");

        // No null terminator
        const uint8_t no_null[] = {'A', 'B', 'C'};
        ASSERT_EQUALS("ABC", UTF8Util::decodeSafe(no_null, 3), "No null terminator");
    }
};

// ============================================================================
// Test Registration
// ============================================================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    TestSuite suite("UTF8Util Unit Tests");
    
    suite.addTest(std::make_unique<UTF8ValidationTest>());
    suite.addTest(std::make_unique<UTF8RepairTest>());
    suite.addTest(std::make_unique<Latin1ConversionTest>());
    suite.addTest(std::make_unique<UTF16ConversionTest>());
    suite.addTest(std::make_unique<UTF32ConversionTest>());
    suite.addTest(std::make_unique<CodepointOperationsTest>());
    suite.addTest(std::make_unique<StringUtilitiesTest>());
    suite.addTest(std::make_unique<UTF8DecodeSafeTest>());
    suite.addTest(std::make_unique<EdgeCasesTest>());
    suite.addTest(std::make_unique<UTF8DecodeSafeComprehensiveTest>());
    
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results) > 0 ? 1 : 0;
}

