/*
 * test_http_client_fallback.cpp - Unit tests for HTTPClient::urlEncode fallback (no-curl)
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#define HTTP_CLIENT_NO_CURL

// Include mock psymp3.h first
#include "psymp3.h"

// Then include the class definition (since mock psymp3.h might not include it)
#include "io/http/HTTPClient.h"

// We include the .cpp file directly to test internal logic with the macro defined
#include "../src/io/http/HTTPClient.cpp"

#include "test_framework.h"

using namespace PsyMP3::IO::HTTP;
using namespace TestFramework;

class HTTPClientFallbackTest : public TestCase {
public:
    HTTPClientFallbackTest() : TestCase("HTTPClient::urlEncode (Fallback)") {}

protected:
    void runTest() override {
        // 1. Alphanumeric characters (unchanged)
        ASSERT_EQUALS("hello", HTTPClient::urlEncode("hello"), "Simple alphanumeric string should remain unchanged");
        ASSERT_EQUALS("12345", HTTPClient::urlEncode("12345"), "Digits should remain unchanged");

        // 2. Unreserved characters (unchanged)
        ASSERT_EQUALS("-_.~", HTTPClient::urlEncode("-_.~"), "Unreserved characters should remain unchanged");

        // 3. Space encoding (%20)
        ASSERT_EQUALS("hello%20world", HTTPClient::urlEncode("hello world"), "Space should be encoded as %20");

        // 4. Reserved characters (encoded)
        // RFC 3986 reserved chars: ! * ' ( ) ; : @ & = + $ , / ? # [ ]
        ASSERT_EQUALS("%21", HTTPClient::urlEncode("!"), "! should be encoded");
        ASSERT_EQUALS("%2A", HTTPClient::urlEncode("*"), "* should be encoded");
        ASSERT_EQUALS("%27", HTTPClient::urlEncode("'"), "' should be encoded");
        ASSERT_EQUALS("%28", HTTPClient::urlEncode("("), "( should be encoded");
        ASSERT_EQUALS("%29", HTTPClient::urlEncode(")"), ") should be encoded");
        ASSERT_EQUALS("%3B", HTTPClient::urlEncode(";"), "; should be encoded");
        ASSERT_EQUALS("%3A", HTTPClient::urlEncode(":"), ": should be encoded");
        ASSERT_EQUALS("%40", HTTPClient::urlEncode("@"), "@ should be encoded");
        ASSERT_EQUALS("%26", HTTPClient::urlEncode("&"), "& should be encoded");
        ASSERT_EQUALS("%3D", HTTPClient::urlEncode("="), "= should be encoded");
        ASSERT_EQUALS("%2B", HTTPClient::urlEncode("+"), "+ should be encoded");
        ASSERT_EQUALS("%24", HTTPClient::urlEncode("$"), "$ should be encoded");
        ASSERT_EQUALS("%2C", HTTPClient::urlEncode(","), ", should be encoded");
        ASSERT_EQUALS("%2F", HTTPClient::urlEncode("/"), "/ should be encoded");
        ASSERT_EQUALS("%3F", HTTPClient::urlEncode("?"), "? should be encoded");
        ASSERT_EQUALS("%23", HTTPClient::urlEncode("#"), "# should be encoded");
        ASSERT_EQUALS("%5B", HTTPClient::urlEncode("["), "[ should be encoded");
        ASSERT_EQUALS("%5D", HTTPClient::urlEncode("]"), "] should be encoded");

        // 5. Mixed content
        ASSERT_EQUALS("a%2Fb%3Fc%3Dd%26e", HTTPClient::urlEncode("a/b?c=d&e"), "Mixed content should be encoded correctly");

        // 6. Empty strings
        ASSERT_EQUALS("", HTTPClient::urlEncode(""), "Empty string should return empty string");

        // 7. Extended characters (UTF-8 bytes should be percent encoded)
        // '€' is E2 82 AC in UTF-8
        ASSERT_EQUALS("%E2%82%AC", HTTPClient::urlEncode("€"), "Extended characters should be percent encoded");
    }
};

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    TestSuite suite("HTTPClient Fallback Tests");
    suite.addTest(std::make_unique<HTTPClientFallbackTest>());

    auto results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
