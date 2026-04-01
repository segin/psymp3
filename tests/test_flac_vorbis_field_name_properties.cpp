/*
 * test_flac_vorbis_field_name_properties.cpp - Property-based tests for FLAC Vorbis field name validation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

#include "psymp3.h"

#include "demuxer/flac/FLACDemuxer.h"

using namespace PsyMP3::Demuxer::FLAC;

#define ASSERT_TEST(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "Assertion failed: " << message << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
            throw std::runtime_error(message); \
        } \
    } while (0)

void test_property_vorbis_field_name_validation() {
    std::cout << "\n=== Property: Vorbis Field Name Validation ===" << std::endl;
    std::cout << "Testing that field names adhere to RFC 9639 Section 8.6..." << std::endl;

    int tests_passed = 0;
    int tests_run = 0;

    // ----------------------------------------
    // Test 1: Empty field name
    // ----------------------------------------
    std::cout << "\n  Test 1: Empty field name..." << std::endl;
    {
        tests_run++;
        if (!FLACDemuxer::isValidVorbisFieldName("")) {
            tests_passed++;
            std::cout << "    Empty field name rejected correctly ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Empty field name was accepted" << std::endl;
            ASSERT_TEST(false, "Empty field name validation failed");
        }
    }

    // ----------------------------------------
    // Test 2: Valid characters (0x20 to 0x7D, excluding '=')
    // ----------------------------------------
    std::cout << "\n  Test 2: Valid characters (0x20 to 0x7D, excluding '=')..." << std::endl;
    {
        tests_run++;
        bool all_passed = true;

        for (char c = 0x20; c <= 0x7D; ++c) {
            if (c == '=') continue;

            std::string s(1, c);
            if (!FLACDemuxer::isValidVorbisFieldName(s)) {
                all_passed = false;
                std::cerr << "    FAILED: Valid character '" << c << "' (0x" << std::hex << (int)c << std::dec << ") was rejected" << std::endl;
            }
        }

        if (all_passed) {
            tests_passed++;
            std::cout << "    All valid characters accepted correctly ✓" << std::endl;
        } else {
            ASSERT_TEST(false, "Valid character validation failed");
        }
    }

    // ----------------------------------------
    // Test 3: Invalid character - Equals sign ('=')
    // ----------------------------------------
    std::cout << "\n  Test 3: Invalid character - Equals sign ('=')..." << std::endl;
    {
        tests_run++;
        if (!FLACDemuxer::isValidVorbisFieldName("=")) {
            tests_passed++;
            std::cout << "    Equals sign ('=') rejected correctly ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Equals sign ('=') was accepted" << std::endl;
            ASSERT_TEST(false, "Equals sign validation failed");
        }

        // Test equals sign embedded in string
        tests_run++;
        if (!FLACDemuxer::isValidVorbisFieldName("TIT=LE")) {
            tests_passed++;
            std::cout << "    Embedded equals sign rejected correctly ✓" << std::endl;
        } else {
            std::cerr << "    FAILED: Embedded equals sign was accepted" << std::endl;
            ASSERT_TEST(false, "Embedded equals sign validation failed");
        }
    }

    // ----------------------------------------
    // Test 4: Invalid characters - Control characters (< 0x20)
    // ----------------------------------------
    std::cout << "\n  Test 4: Invalid characters - Control characters (< 0x20)..." << std::endl;
    {
        tests_run++;
        bool all_passed = true;

        for (int i = 0x00; i < 0x20; ++i) {
            char c = static_cast<char>(i);
            std::string s(1, c);
            if (FLACDemuxer::isValidVorbisFieldName(s)) {
                all_passed = false;
                std::cerr << "    FAILED: Control character 0x" << std::hex << i << std::dec << " was accepted" << std::endl;
            }

            // Also test embedded control character
            std::string embedded = "TITLE" + s;
            if (FLACDemuxer::isValidVorbisFieldName(embedded)) {
                all_passed = false;
                std::cerr << "    FAILED: Embedded control character 0x" << std::hex << i << std::dec << " was accepted" << std::endl;
            }
        }

        if (all_passed) {
            tests_passed++;
            std::cout << "    All control characters rejected correctly ✓" << std::endl;
        } else {
            ASSERT_TEST(false, "Control character validation failed");
        }
    }

    // ----------------------------------------
    // Test 5: Invalid characters - Non-ASCII and Extended ASCII (> 0x7D)
    // ----------------------------------------
    std::cout << "\n  Test 5: Invalid characters - Extended characters (> 0x7D)..." << std::endl;
    {
        tests_run++;
        bool all_passed = true;

        for (int i = 0x7E; i <= 0xFF; ++i) {
            char c = static_cast<char>(i);
            std::string s(1, c);
            if (FLACDemuxer::isValidVorbisFieldName(s)) {
                all_passed = false;
                std::cerr << "    FAILED: Extended character 0x" << std::hex << i << std::dec << " was accepted" << std::endl;
            }

            // Also test embedded extended character
            std::string embedded = "TITLE" + s;
            if (FLACDemuxer::isValidVorbisFieldName(embedded)) {
                all_passed = false;
                std::cerr << "    FAILED: Embedded extended character 0x" << std::hex << i << std::dec << " was accepted" << std::endl;
            }
        }

        if (all_passed) {
            tests_passed++;
            std::cout << "    All extended characters rejected correctly ✓" << std::endl;
        } else {
            ASSERT_TEST(false, "Extended character validation failed");
        }
    }

    // ----------------------------------------
    // Summary
    // ----------------------------------------
    std::cout << "\n✓ Property: Vorbis Field Name Validation: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    ASSERT_TEST(tests_passed == tests_run, "Not all tests passed");
}


// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC VORBIS FIELD NAME VALIDATION PROPERTY-BASED TESTS" << std::endl;
    std::cout << std::string(70, '=') << std::endl;

    try {
        test_property_vorbis_field_name_validation();

        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "✅ ALL PROPERTY TESTS PASSED" << std::endl;
        std::cout << std::string(70, '=') << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(70, '=') << std::endl;
        std::cerr << "❌ PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Exception: " << e.what() << std::endl;
        std::cerr << std::string(70, '=') << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n" << std::string(70, '=') << std::endl;
        std::cerr << "❌ PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Unknown exception occurred" << std::endl;
        std::cerr << std::string(70, '=') << std::endl;
        return 1;
    }
}
