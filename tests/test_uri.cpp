/*
 * test_uri.cpp - Unit tests for URI class
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include "io/URI.h"

using namespace TestFramework;
using namespace PsyMP3::IO;

class URI_LocalFileThreeSlashes : public TestCase {
public:
    URI_LocalFileThreeSlashes() : TestCase("URI_LocalFileThreeSlashes") {}
protected:
    void runTest() override {
        URI uri(TagLib::String("file:///home/user/music/song.mp3"));
        ASSERT_EQUALS(std::string("file"), uri.scheme().to8Bit(true), "Scheme should be file");
        ASSERT_EQUALS(std::string("/home/user/music/song.mp3"), uri.path().to8Bit(true), "Path should match");
    }
};

class URI_LocalFileOneSlash : public TestCase {
public:
    URI_LocalFileOneSlash() : TestCase("URI_LocalFileOneSlash") {}
protected:
    void runTest() override {
        URI uri(TagLib::String("file:/home/user/music/song.mp3"));
        ASSERT_EQUALS(std::string("file"), uri.scheme().to8Bit(true), "Scheme should be file");
        ASSERT_EQUALS(std::string("/home/user/music/song.mp3"), uri.path().to8Bit(true), "Path should match");
    }
};

class URI_NoScheme : public TestCase {
public:
    URI_NoScheme() : TestCase("URI_NoScheme") {}
protected:
    void runTest() override {
        URI uri(TagLib::String("/home/user/music/song.mp3"));
        ASSERT_EQUALS(std::string("file"), uri.scheme().to8Bit(true), "Scheme should default to file");
        ASSERT_EQUALS(std::string("/home/user/music/song.mp3"), uri.path().to8Bit(true), "Path should match input");
    }
};

class URI_RelativePath : public TestCase {
public:
    URI_RelativePath() : TestCase("URI_RelativePath") {}
protected:
    void runTest() override {
        URI uri(TagLib::String("music/song.mp3"));
        ASSERT_EQUALS(std::string("file"), uri.scheme().to8Bit(true), "Scheme should default to file");
        ASSERT_EQUALS(std::string("music/song.mp3"), uri.path().to8Bit(true), "Path should match input");
    }
};

class URI_HTTPScheme : public TestCase {
public:
    URI_HTTPScheme() : TestCase("URI_HTTPScheme") {}
protected:
    void runTest() override {
        URI uri(TagLib::String("http://example.com/stream.mp3"));
        ASSERT_EQUALS(std::string("http"), uri.scheme().to8Bit(true), "Scheme should be http");
        ASSERT_EQUALS(std::string("example.com/stream.mp3"), uri.path().to8Bit(true), "Path should exclude scheme://");
    }
};

class URI_HTTPSScheme : public TestCase {
public:
    URI_HTTPSScheme() : TestCase("URI_HTTPSScheme") {}
protected:
    void runTest() override {
        URI uri(TagLib::String("https://example.com/stream.mp3"));
        ASSERT_EQUALS(std::string("https"), uri.scheme().to8Bit(true), "Scheme should be https");
        ASSERT_EQUALS(std::string("example.com/stream.mp3"), uri.path().to8Bit(true), "Path should exclude scheme://");
    }
};

class URI_FTPScheme : public TestCase {
public:
    URI_FTPScheme() : TestCase("URI_FTPScheme") {}
protected:
    void runTest() override {
        URI uri(TagLib::String("ftp://ftp.example.com/pub/song.mp3"));
        ASSERT_EQUALS(std::string("ftp"), uri.scheme().to8Bit(true), "Scheme should be ftp");
        ASSERT_EQUALS(std::string("ftp.example.com/pub/song.mp3"), uri.path().to8Bit(true), "Path should exclude scheme://");
    }
};

class URI_CustomScheme : public TestCase {
public:
    URI_CustomScheme() : TestCase("URI_CustomScheme") {}
protected:
    void runTest() override {
        URI uri(TagLib::String("myscheme://data"));
        ASSERT_EQUALS(std::string("myscheme"), uri.scheme().to8Bit(true), "Scheme should be myscheme");
        ASSERT_EQUALS(std::string("data"), uri.path().to8Bit(true), "Path should be data");
    }
};

class URI_EmptyString : public TestCase {
public:
    URI_EmptyString() : TestCase("URI_EmptyString") {}
protected:
    void runTest() override {
        URI uri(TagLib::String(""));
        ASSERT_EQUALS(std::string("file"), uri.scheme().to8Bit(true), "Empty string should default to file scheme");
        ASSERT_EQUALS(std::string(""), uri.path().to8Bit(true), "Path should be empty");
    }
};

class URI_OnlySeparator : public TestCase {
public:
    URI_OnlySeparator() : TestCase("URI_OnlySeparator") {}
protected:
    void runTest() override {
        URI uri(TagLib::String("://"));
        // Based on implementation logic:
        // s.find("://") returns 0.
        // scheme = substr(0, 0) -> ""
        // path = substr(0 + 3) -> ""
        ASSERT_EQUALS(std::string(""), uri.scheme().to8Bit(true), "Scheme should be empty");
        ASSERT_EQUALS(std::string(""), uri.path().to8Bit(true), "Path should be empty");
    }
};

class URI_SpecialCharacters : public TestCase {
public:
    URI_SpecialCharacters() : TestCase("URI_SpecialCharacters") {}
protected:
    void runTest() override {
        URI uri(TagLib::String("file:///path/with spaces/and+symbols.mp3"));
        ASSERT_EQUALS(std::string("file"), uri.scheme().to8Bit(true), "Scheme should be file");
        ASSERT_EQUALS(std::string("/path/with spaces/and+symbols.mp3"), uri.path().to8Bit(true), "Path should preserve characters");
    }
};

int main() {
    TestSuite suite("URI Class Tests");

    suite.addTest(std::make_unique<URI_LocalFileThreeSlashes>());
    suite.addTest(std::make_unique<URI_LocalFileOneSlash>());
    suite.addTest(std::make_unique<URI_NoScheme>());
    suite.addTest(std::make_unique<URI_RelativePath>());
    suite.addTest(std::make_unique<URI_HTTPScheme>());
    suite.addTest(std::make_unique<URI_HTTPSScheme>());
    suite.addTest(std::make_unique<URI_FTPScheme>());
    suite.addTest(std::make_unique<URI_CustomScheme>());
    suite.addTest(std::make_unique<URI_EmptyString>());
    suite.addTest(std::make_unique<URI_OnlySeparator>());
    suite.addTest(std::make_unique<URI_SpecialCharacters>());

    auto results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results) == 0 ? 0 : 1;
}
