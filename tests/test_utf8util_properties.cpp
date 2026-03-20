/*
 * test_utf8util_properties.cpp - Property-based tests for UTF8Util
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifdef HAVE_RAPIDCHECK
#include <rapidcheck.h>
#endif

#include <iostream>
#include <cassert>
#include <random>

using namespace PsyMP3::Core::Utility;

// ============================================================================
// Property 1: UTF-8 Round-Trip Preservation
// ============================================================================
// For any valid UTF-8 string, encoding to UTF-16 and back should produce
// the original string.

void test_property_utf16_roundtrip() {
    std::cout << "\n=== Property 1: UTF-16 Round-Trip ===" << std::endl;
    std::cout << "Testing that UTF-8 -> UTF-16 -> UTF-8 preserves data..." << std::endl;
    
    // Test vectors
    std::vector<std::string> test_cases = {
        "",
        "Hello",
        "café",
        "日本語",
        "🎵🎶🎸",
        "Mixed: Hello 世界 🎵 café",
        "Emoji sequence: 👨‍👩‍👧‍👦",
        std::string(1000, 'A'), // Long ASCII
    };
    
    int passed = 0;
    for (const auto& original : test_cases) {
        // UTF-16LE round-trip
        auto utf16le = UTF8Util::toUTF16LE(original);
        std::string decoded_le = UTF8Util::fromUTF16LE(utf16le.data(), utf16le.size());
        
        // UTF-16BE round-trip
        auto utf16be = UTF8Util::toUTF16BE(original);
        std::string decoded_be = UTF8Util::fromUTF16BE(utf16be.data(), utf16be.size());
        
        // UTF-16 with BOM round-trip
        auto utf16bom = UTF8Util::toUTF16BOM(original);
        std::string decoded_bom = UTF8Util::fromUTF16BOM(utf16bom.data(), utf16bom.size());
        
        if (decoded_le == original && decoded_be == original && decoded_bom == original) {
            passed++;
        } else {
            std::cerr << "FAIL: Round-trip failed for string of length " << original.size() << std::endl;
        }
    }
    
    std::cout << "Passed " << passed << "/" << test_cases.size() << " test cases" << std::endl;
    assert(passed == static_cast<int>(test_cases.size()) && "All UTF-16 round-trips should pass");
    std::cout << "✓ Property 1: UTF-16 Round-Trip - PASSED" << std::endl;
}


// ============================================================================
// Property 2: UTF-32 Round-Trip Preservation
// ============================================================================

void test_property_utf32_roundtrip() {
    std::cout << "\n=== Property 2: UTF-32 Round-Trip ===" << std::endl;
    std::cout << "Testing that UTF-8 -> UTF-32 -> UTF-8 preserves data..." << std::endl;
    
    std::vector<std::string> test_cases = {
        "",
        "Hello",
        "café",
        "日本語",
        "🎵🎶🎸",
        "Mixed content: ABC 日本 🎵",
    };
    
    int passed = 0;
    for (const auto& original : test_cases) {
        auto utf32le = UTF8Util::toUTF32LE(original);
        std::string decoded_le = UTF8Util::fromUTF32LE(utf32le.data(), utf32le.size());
        
        auto utf32be = UTF8Util::toUTF32BE(original);
        std::string decoded_be = UTF8Util::fromUTF32BE(utf32be.data(), utf32be.size());
        
        if (decoded_le == original && decoded_be == original) {
            passed++;
        } else {
            std::cerr << "FAIL: UTF-32 round-trip failed" << std::endl;
        }
    }
    
    std::cout << "Passed " << passed << "/" << test_cases.size() << " test cases" << std::endl;
    assert(passed == static_cast<int>(test_cases.size()) && "All UTF-32 round-trips should pass");
    std::cout << "✓ Property 2: UTF-32 Round-Trip - PASSED" << std::endl;
}

// ============================================================================
// Property 3: Codepoint Round-Trip
// ============================================================================

void test_property_codepoint_roundtrip() {
    std::cout << "\n=== Property 3: Codepoint Round-Trip ===" << std::endl;
    std::cout << "Testing that encode -> decode preserves codepoints..." << std::endl;
    
    // Test specific codepoints across the Unicode range
    std::vector<uint32_t> test_codepoints = {
        0x00,      // NUL
        0x41,      // 'A'
        0x7F,      // DEL (max 1-byte)
        0x80,      // Min 2-byte
        0xFF,      // Latin-1 max
        0x7FF,     // Max 2-byte
        0x800,     // Min 3-byte
        0xFFFF,    // Max BMP
        0x10000,   // Min supplementary
        0x1F3B5,   // Musical note emoji
        0x10FFFF,  // Max Unicode
    };
    
    int passed = 0;
    for (uint32_t cp : test_codepoints) {
        if (!UTF8Util::isValidCodepoint(cp)) continue;
        
        std::string encoded = UTF8Util::encodeCodepoint(cp);
        size_t consumed;
        uint32_t decoded = UTF8Util::decodeCodepoint(encoded, consumed);
        
        if (decoded == cp && consumed == encoded.size()) {
            passed++;
        } else {
            std::cerr << "FAIL: Codepoint U+" << std::hex << cp << " round-trip failed" << std::endl;
        }
    }
    
    std::cout << "Passed " << passed << "/" << test_codepoints.size() << " codepoints" << std::endl;
    assert(passed == static_cast<int>(test_codepoints.size()) && "All codepoint round-trips should pass");
    std::cout << "✓ Property 3: Codepoint Round-Trip - PASSED" << std::endl;
}

// ============================================================================
// Property 4: Latin-1 Subset Preservation
// ============================================================================

void test_property_latin1_subset() {
    std::cout << "\n=== Property 4: Latin-1 Subset Preservation ===" << std::endl;
    std::cout << "Testing that Latin-1 compatible UTF-8 round-trips through Latin-1..." << std::endl;
    
    // Generate all Latin-1 characters (0x00-0xFF)
    int passed = 0;
    int total = 0;
    
    for (uint32_t cp = 0x01; cp <= 0xFF; ++cp) {
        total++;
        std::string utf8 = UTF8Util::encodeCodepoint(cp);
        auto latin1 = UTF8Util::toLatin1(utf8);
        std::string decoded = UTF8Util::fromLatin1(latin1.data(), latin1.size());
        
        if (decoded == utf8) {
            passed++;
        } else {
            std::cerr << "FAIL: Latin-1 round-trip failed for U+" << std::hex << cp << std::endl;
        }
    }
    
    std::cout << "Passed " << passed << "/" << total << " Latin-1 characters" << std::endl;
    assert(passed == total && "All Latin-1 characters should round-trip");
    std::cout << "✓ Property 4: Latin-1 Subset Preservation - PASSED" << std::endl;
}

// ============================================================================
// Property 5: Validation Consistency
// ============================================================================

void test_property_validation_consistency() {
    std::cout << "\n=== Property 5: Validation Consistency ===" << std::endl;
    std::cout << "Testing that repair produces valid UTF-8..." << std::endl;
    
    // Generate some invalid sequences
    std::vector<std::string> invalid_inputs = {
        std::string("\x80"),           // Orphan continuation
        std::string("\xC0\x80"),       // Overlong NUL
        std::string("\xED\xA0\x80"),   // Surrogate
        std::string("\xF5\x80\x80\x80"), // Beyond max
        std::string("Hello\x80World"), // Invalid in middle
        std::string("\xFF\xFE"),       // Invalid start bytes
    };
    
    int passed = 0;
    for (const auto& invalid : invalid_inputs) {
        std::string repaired = UTF8Util::repair(invalid);
        if (UTF8Util::isValid(repaired)) {
            passed++;
        } else {
            std::cerr << "FAIL: Repaired string is still invalid" << std::endl;
        }
    }
    
    std::cout << "Passed " << passed << "/" << invalid_inputs.size() << " repair tests" << std::endl;
    assert(passed == static_cast<int>(invalid_inputs.size()) && "All repaired strings should be valid");
    std::cout << "✓ Property 5: Validation Consistency - PASSED" << std::endl;
}

// ============================================================================
// Property 6: Length Consistency
// ============================================================================

void test_property_length_consistency() {
    std::cout << "\n=== Property 6: Length Consistency ===" << std::endl;
    std::cout << "Testing that length() equals toCodepoints().size()..." << std::endl;
    
    std::vector<std::string> test_cases = {
        "",
        "Hello",
        "café",
        "日本語",
        "🎵🎶",
        "Mixed: A é 日 🎵",
    };
    
    int passed = 0;
    for (const auto& str : test_cases) {
        size_t len = UTF8Util::length(str);
        size_t cpCount = UTF8Util::toCodepoints(str).size();
        
        if (len == cpCount) {
            passed++;
        } else {
            std::cerr << "FAIL: length() = " << len << " but toCodepoints().size() = " << cpCount << std::endl;
        }
    }
    
    std::cout << "Passed " << passed << "/" << test_cases.size() << " length tests" << std::endl;
    assert(passed == static_cast<int>(test_cases.size()) && "Length should match codepoint count");
    std::cout << "✓ Property 6: Length Consistency - PASSED" << std::endl;
}

#ifdef HAVE_RAPIDCHECK
// ============================================================================
// RapidCheck Property Tests
// ============================================================================

void test_rapidcheck_properties() {
    std::cout << "\n=== RapidCheck Property Tests ===" << std::endl;
    
    // Property: Valid UTF-8 strings remain valid after repair
    rc::check("repair(valid) == valid", []() {
        std::string s = *rc::gen::string<std::string>();
        // Generate valid UTF-8 by encoding random codepoints
        std::vector<uint32_t> cps;
        for (char c : s) {
            uint32_t cp = static_cast<uint8_t>(c) % 0x80; // ASCII only for simplicity
            cps.push_back(cp);
        }
        std::string valid = UTF8Util::fromCodepoints(cps);
        RC_ASSERT(UTF8Util::repair(valid) == valid);
    });
    
    // Property: UTF-16 round-trip preserves ASCII
    rc::check("UTF-16 round-trip preserves ASCII", []() {
        std::string ascii = *rc::gen::container<std::string>(rc::gen::inRange('!', '~'));
        auto utf16 = UTF8Util::toUTF16LE(ascii);
        std::string decoded = UTF8Util::fromUTF16LE(utf16.data(), utf16.size());
        RC_ASSERT(decoded == ascii);
    });
    
    std::cout << "✓ RapidCheck properties passed" << std::endl;
}
#endif

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "UTF8Util Property-Based Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    
    test_property_utf16_roundtrip();
    test_property_utf32_roundtrip();
    test_property_codepoint_roundtrip();
    test_property_latin1_subset();
    test_property_validation_consistency();
    test_property_length_consistency();
    
#ifdef HAVE_RAPIDCHECK
    test_rapidcheck_properties();
#else
    std::cout << "\n[SKIP] RapidCheck not available - skipping randomized tests" << std::endl;
#endif
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "All UTF8Util property tests PASSED!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}

