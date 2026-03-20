/*
 * test_uri.cpp - Unit tests for URI class
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif

// Include TagLib header before URI.h because URI.h depends on it
// and does not include it itself (relying on psymp3.h usually).
#include <taglib/tstring.h>
#include "io/URI.h"
#include "test_framework.h"

using namespace PsyMP3::IO;
using namespace TestFramework;

class URIParsingTest : public TestCase {
public:
    URIParsingTest() : TestCase("URI::URI parsing") {}

protected:
    void runTest() override {
        // Test file:///
        URI uri1("file:///home/user/music/song.mp3");
        ASSERT_EQUALS("file", uri1.scheme().to8Bit(true), "Scheme should be file");
        ASSERT_EQUALS("/home/user/music/song.mp3", uri1.path().to8Bit(true), "Path should be extracted correctly");

        // Test file:/
        URI uri2("file:/home/user/music/song.mp3");
        ASSERT_EQUALS("file", uri2.scheme().to8Bit(true), "Scheme should be file");
        ASSERT_EQUALS("/home/user/music/song.mp3", uri2.path().to8Bit(true), "Path should be extracted correctly");

        // Test http://
        URI uri3("http://example.com/stream");
        ASSERT_EQUALS("http", uri3.scheme().to8Bit(true), "Scheme should be http");
        ASSERT_EQUALS("example.com/stream", uri3.path().to8Bit(true), "Path should be extracted correctly");

        // Test https://
        URI uri4("https://example.com/secure/stream");
        ASSERT_EQUALS("https", uri4.scheme().to8Bit(true), "Scheme should be https");
        ASSERT_EQUALS("example.com/secure/stream", uri4.path().to8Bit(true), "Path should be extracted correctly");

        // Test no scheme (default to file)
        URI uri5("/local/path/to/file.mp3");
        ASSERT_EQUALS("file", uri5.scheme().to8Bit(true), "Scheme should be file");
        ASSERT_EQUALS("/local/path/to/file.mp3", uri5.path().to8Bit(true), "Path should be extracted correctly");

        // Test custom scheme
        URI uri6("custom://resource");
        ASSERT_EQUALS("custom", uri6.scheme().to8Bit(true), "Scheme should be custom");
        ASSERT_EQUALS("resource", uri6.path().to8Bit(true), "Path should be extracted correctly");

        // Test empty
        URI uri7("");
        ASSERT_EQUALS("file", uri7.scheme().to8Bit(true), "Empty URI defaults to file scheme");
        ASSERT_EQUALS("", uri7.path().to8Bit(true), "Empty URI has empty path");

        // Test Windows path (file:///)
        // file:///C:/path/to/file
        URI uri8("file:///C:/path/to/file");
        ASSERT_EQUALS("file", uri8.scheme().to8Bit(true), "Scheme should be file");
        ASSERT_EQUALS("/C:/path/to/file", uri8.path().to8Bit(true), "Path should be extracted correctly including drive letter");

        // Test generic scheme with authority
        // smb://server/share/file
        URI uri9("smb://server/share/file");
        ASSERT_EQUALS("smb", uri9.scheme().to8Bit(true), "Scheme should be smb");
        ASSERT_EQUALS("server/share/file", uri9.path().to8Bit(true), "Path should be extracted correctly");
    }
};

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    TestSuite suite("URI Unit Tests");
    suite.addTest(std::make_unique<URIParsingTest>());

    auto results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
