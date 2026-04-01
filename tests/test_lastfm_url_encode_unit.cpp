/*
 * test_lastfm_url_encode_unit.cpp - Unit tests for LastFM::urlEncode
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "lastfm/LastFM.h"
#include "test_framework.h"

using namespace PsyMP3::LastFM;
using namespace TestFramework;

class LastFMURLEncodeTest : public TestCase {
public:
    LastFMURLEncodeTest() : TestCase("LastFM::urlEncode") {}

protected:
    void runTest() override {
        // 1. Alphanumeric characters (unchanged)
        ASSERT_EQUALS(std::string("hello"), LastFM::urlEncode("hello"), "Simple alphanumeric string should remain unchanged");
        ASSERT_EQUALS(std::string("1234567890"), LastFM::urlEncode("1234567890"), "Digits should remain unchanged");

        // 2. Unreserved characters (unchanged)
        ASSERT_EQUALS(std::string("-_.~"), LastFM::urlEncode("-_.~"), "Unreserved characters should remain unchanged");

        // 3. Space encoding (%20)
        ASSERT_EQUALS(std::string("hello%20world"), LastFM::urlEncode("hello world"), "Space should be encoded as %20");

        // 4. Reserved characters (encoded)
        // RFC 3986 reserved chars: ! * ' ( ) ; : @ & = + $ , / ? # [ ]
        ASSERT_EQUALS(std::string("%21"), LastFM::urlEncode("!"), "! should be encoded");
        ASSERT_EQUALS(std::string("%2A"), LastFM::urlEncode("*"), "* should be encoded");
        ASSERT_EQUALS(std::string("%27"), LastFM::urlEncode("'"), "' should be encoded");
        ASSERT_EQUALS(std::string("%28"), LastFM::urlEncode("("), "( should be encoded");
        ASSERT_EQUALS(std::string("%29"), LastFM::urlEncode(")"), ") should be encoded");
        ASSERT_EQUALS(std::string("%3B"), LastFM::urlEncode(";"), "; should be encoded");
        ASSERT_EQUALS(std::string("%3A"), LastFM::urlEncode(":"), ": should be encoded");
        ASSERT_EQUALS(std::string("%40"), LastFM::urlEncode("@"), "@ should be encoded");
        ASSERT_EQUALS(std::string("%26"), LastFM::urlEncode("&"), "& should be encoded");
        ASSERT_EQUALS(std::string("%3D"), LastFM::urlEncode("="), "= should be encoded");
        ASSERT_EQUALS(std::string("%2B"), LastFM::urlEncode("+"), "+ should be encoded");
        ASSERT_EQUALS(std::string("%24"), LastFM::urlEncode("$"), "$ should be encoded");
        ASSERT_EQUALS(std::string("%2C"), LastFM::urlEncode(","), ", should be encoded");
        ASSERT_EQUALS(std::string("%2F"), LastFM::urlEncode("/"), "/ should be encoded");
        ASSERT_EQUALS(std::string("%3F"), LastFM::urlEncode("?"), "? should be encoded");
        ASSERT_EQUALS(std::string("%23"), LastFM::urlEncode("#"), "# should be encoded");
        ASSERT_EQUALS(std::string("%5B"), LastFM::urlEncode("["), "[ should be encoded");
        ASSERT_EQUALS(std::string("%5D"), LastFM::urlEncode("]"), "] should be encoded");

        // 5. Mixed content
        ASSERT_EQUALS(std::string("artist%3DThe%20Beatles%26track%3DHey%20Jude"), LastFM::urlEncode("artist=The Beatles&track=Hey Jude"), "Mixed content should be encoded correctly");

        // 6. Empty strings
        ASSERT_EQUALS(std::string(""), LastFM::urlEncode(""), "Empty string should return empty string");

        // 7. Extended characters (UTF-8 bytes should be percent encoded)
        // '€' is E2 82 AC in UTF-8
        ASSERT_EQUALS(std::string("%E2%82%AC"), LastFM::urlEncode("€"), "Extended characters should be percent encoded");
    }
};

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    TestSuite suite("LastFM URL Encode Unit Tests");
    suite.addTest(std::make_unique<LastFMURLEncodeTest>());

    auto results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
