/*
 * tests/test_mediafile_split.cpp - Unit tests for MediaFile::split
 * This file is part of PsyMP3.
 */

#ifndef FINAL_BUILD
#define FINAL_BUILD
#endif

#include <vector>
#include <string>
#include <memory>
#include "psymp3.h"
#include "test_framework.h"

using namespace TestFramework;

class MediaFileSplitTest : public TestCase {
public:
    MediaFileSplitTest() : TestCase("MediaFile::split") {}

protected:
    void runTest() override {
        // Test 1: Standard comma-separated string
        {
            std::string s = "apple,banana,cherry";
            std::vector<std::string> result = MediaFile::split(s, ',');
            ASSERT_EQUALS(static_cast<size_t>(3), result.size(), "Standard split should return 3 elements");
            if (result.size() >= 3) {
                ASSERT_EQUALS(std::string("apple"), result[0], "First element mismatch");
                ASSERT_EQUALS(std::string("banana"), result[1], "Second element mismatch");
                ASSERT_EQUALS(std::string("cherry"), result[2], "Third element mismatch");
            }
        }

        // Test 2: Empty input string
        {
            std::string s = "";
            std::vector<std::string> result = MediaFile::split(s, ',');
            ASSERT_EQUALS(static_cast<size_t>(0), result.size(), "Empty string should return 0 elements");
        }

        // Test 3: No delimiter present
        {
            std::string s = "single_element";
            std::vector<std::string> result = MediaFile::split(s, ',');
            ASSERT_EQUALS(static_cast<size_t>(1), result.size(), "String without delimiter should return 1 element");
            if (result.size() >= 1) {
                ASSERT_EQUALS(std::string("single_element"), result[0], "Element mismatch when no delimiter present");
            }
        }

        // Test 4: Leading delimiter
        {
            std::string s = ",leading";
            std::vector<std::string> result = MediaFile::split(s, ',');
            ASSERT_EQUALS(static_cast<size_t>(2), result.size(), "Leading delimiter should result in an empty first element");
            if (result.size() >= 2) {
                ASSERT_EQUALS(std::string(""), result[0], "First element should be empty");
                ASSERT_EQUALS(std::string("leading"), result[1], "Second element mismatch");
            }
        }

        // Test 5: Trailing delimiter
        {
            std::string s = "trailing,";
            std::vector<std::string> result = MediaFile::split(s, ',');
            // std::getline behavior results in 1 element here
            ASSERT_EQUALS(static_cast<size_t>(1), result.size(), "Trailing delimiter currently returns only preceding elements");
            if (result.size() >= 1) {
                ASSERT_EQUALS(std::string("trailing"), result[0], "Element mismatch");
            }
        }

        // Test 6: Consecutive delimiters
        {
            std::string s = "a,,b";
            std::vector<std::string> result = MediaFile::split(s, ',');
            ASSERT_EQUALS(static_cast<size_t>(3), result.size(), "Consecutive delimiters should result in empty element in between");
            if (result.size() >= 3) {
                ASSERT_EQUALS(std::string("a"), result[0], "First element mismatch");
                ASSERT_EQUALS(std::string(""), result[1], "Middle element should be empty");
                ASSERT_EQUALS(std::string("b"), result[2], "Third element mismatch");
            }
        }

        // Test 7: Only delimiters
        {
            std::string s = ",,";
            std::vector<std::string> result = MediaFile::split(s, ',');
            // 2 commas -> 2 successful getlines returning empty strings
            ASSERT_EQUALS(static_cast<size_t>(2), result.size(), "N delimiters currently return N elements if they are at start/middle");
            if (result.size() >= 2) {
                ASSERT_EQUALS(std::string(""), result[0], "First element mismatch");
                ASSERT_EQUALS(std::string(""), result[1], "Second element mismatch");
            }
        }

        // Test 8: Different delimiter
        {
            std::string s = "path/to/file";
            std::vector<std::string> result = MediaFile::split(s, '/');
            ASSERT_EQUALS(static_cast<size_t>(3), result.size(), "Should work with different delimiters");
            if (result.size() >= 3) {
                ASSERT_EQUALS(std::string("path"), result[0], "First element mismatch");
                ASSERT_EQUALS(std::string("to"), result[1], "Second element mismatch");
                ASSERT_EQUALS(std::string("file"), result[2], "Third element mismatch");
            }
        }

        // Test 9: Appending to existing vector
        {
            std::vector<std::string> elems = {"initial"};
            std::string s = "extra1,extra2";
            MediaFile::split(s, ',', elems);
            ASSERT_EQUALS(static_cast<size_t>(3), elems.size(), "Should append to existing vector");
            if (elems.size() >= 3) {
                ASSERT_EQUALS(std::string("initial"), elems[0], "Original element should be preserved");
                ASSERT_EQUALS(std::string("extra1"), elems[1], "First extra element mismatch");
                ASSERT_EQUALS(std::string("extra2"), elems[2], "Second extra element mismatch");
            }
        }
    }
};

int main() {
    TestSuite suite("MediaFile Split Tests");
    suite.addTest(std::make_unique<MediaFileSplitTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
