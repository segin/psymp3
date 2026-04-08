/*
 * test_mediafile_split.cpp - Unit tests for MediaFile::split
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
#include "mediafile.h"
#include <vector>
#include <string>

using namespace TestFramework;

class MediaFileSplitTest : public TestCase {
public:
    MediaFileSplitTest() : TestCase("MediaFile::split") {}

protected:
    void runTest() override {
        // Test basic comma-separated split
        std::vector<std::string> r1 = MediaFile::split("a,b,c", ',');
        ASSERT_EQUALS(static_cast<size_t>(3), r1.size(), "Basic split size");
        ASSERT_EQUALS("a", r1[0], "First element");
        ASSERT_EQUALS("b", r1[1], "Second element");
        ASSERT_EQUALS("c", r1[2], "Third element");

        // Test empty string
        std::vector<std::string> r2 = MediaFile::split("", ',');
        ASSERT_EQUALS(static_cast<size_t>(0), r2.size(), "Empty string should return empty vector");

        // Test string with no delimiter
        std::vector<std::string> r3 = MediaFile::split("hello", ',');
        ASSERT_EQUALS(static_cast<size_t>(1), r3.size(), "No delimiter size");
        ASSERT_EQUALS("hello", r3[0], "No delimiter element");

        // Test consecutive delimiters
        std::vector<std::string> r4 = MediaFile::split("a,,b", ',');
        ASSERT_EQUALS(static_cast<size_t>(3), r4.size(), "Consecutive delimiters size");
        ASSERT_EQUALS("a", r4[0], "First element");
        ASSERT_EQUALS("", r4[1], "Middle empty element");
        ASSERT_EQUALS("b", r4[2], "Last element");

        // Test trailing delimiter (quirk: std::getline doesn't produce an empty element for trailing delimiter)
        std::vector<std::string> r5 = MediaFile::split("a,b,", ',');
        ASSERT_EQUALS(static_cast<size_t>(2), r5.size(), "Trailing delimiter size quirk");
        ASSERT_EQUALS("a", r5[0], "First element");
        ASSERT_EQUALS("b", r5[1], "Second element");

        // Test appending variant
        std::vector<std::string> append_vec = {"x"};
        MediaFile::split("y,z", ',', append_vec);
        ASSERT_EQUALS(static_cast<size_t>(3), append_vec.size(), "Appended split size");
        ASSERT_EQUALS("x", append_vec[0], "Original element");
        ASSERT_EQUALS("y", append_vec[1], "Appended element 1");
        ASSERT_EQUALS("z", append_vec[2], "Appended element 2");
    }
};

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    TestSuite suite("MediaFile Unit Tests");
    suite.addTest(std::make_unique<MediaFileSplitTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
