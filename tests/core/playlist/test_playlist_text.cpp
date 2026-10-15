/*
 * test_playlist_text.cpp - PlaylistText: M3U, PLS, XSPF and ASX parsing
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * The samples are in the shapes radio sites and other players write:
 * SHOUTcast .pls downloads, VLC-style XSPF with file:// URIs and percent
 * escapes, Windows Media .asx with mixed-case tags, and M3U with URL lines.
 */

#include "psymp3.h"
#include "test_framework.h"

using PsyMP3::Core::Utility::PlaylistFormat;
using PsyMP3::Core::Utility::PlaylistItem;
using PsyMP3::Core::Utility::PlaylistText;

namespace {

/** @brief One test per format, and one for the helpers. */
class PlaylistTextTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    PlaylistTextTest() : TestCase("PlaylistText: M3U, PLS, XSPF, ASX and helpers") {}
protected:
    /** @brief Run every part. */
    void runTest() override {
        testM3U();
        testPLS();
        testXSPF();
        testASX();
        testDetection();
        testLooseASX();
        testHelpers();
    }

private:
    /** @brief EXTINF titles, URL and path lines, comments, a BOM, CRLF. */
    void testM3U() {
        const auto items = PlaylistText::parse(
            "\xEF\xBB\xBF#EXTM3U\r\n#EXTINF:-1,181.FM - Rock 181\r\nhttps://listen.181fm.com/181-rock_128k.mp3\r\n"
            "# a comment\r\n#EXTINF:215,Band - Song\r\nmusic/song.flac\r\n", PlaylistFormat::M3U);
        ASSERT_EQUALS(size_t(2), items.size(), "M3U: two entries");
        ASSERT_EQUALS(std::string("https://listen.181fm.com/181-rock_128k.mp3"), items[0].location, "M3U: URL line");
        ASSERT_EQUALS(std::string("Rock 181"), items[0].title, "M3U: title");
        ASSERT_EQUALS(0L, items[0].duration_seconds, "M3U: -1 (a stream) is no duration");
        ASSERT_EQUALS(std::string("music/song.flac"), items[1].location, "M3U: relative path as written");
        ASSERT_EQUALS(215L, items[1].duration_seconds, "M3U: duration");
    }

    /** @brief Numbered keys in any case and order, Length -1 for a stream. */
    void testPLS() {
        const auto items = PlaylistText::parse(
            "[playlist]\nNumberOfEntries=2\n"
            "File2=http://backup.example.com:8000/stream\nTitle2=Backup\n"
            "file1=http://ice1.example.com/radio\ntitle1=(#1 - 300/2000) Example FM\nLength1=-1\n"
            "Version=2\n", PlaylistFormat::PLS);
        ASSERT_EQUALS(size_t(2), items.size(), "PLS: two entries");
        ASSERT_EQUALS(std::string("http://ice1.example.com/radio"), items[0].location, "PLS: File1 first, in any case");
        ASSERT_EQUALS(std::string("http://backup.example.com:8000/stream"), items[1].location, "PLS: then File2");
        ASSERT_EQUALS(std::string("Backup"), items[1].title, "PLS: title");
        ASSERT_EQUALS(0L, items[0].duration_seconds, "PLS: Length -1 is no duration");
    }

    /** @brief Tracks with location, title, creator and duration; namespaces. */
    void testXSPF() {
        const auto items = PlaylistText::parse(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<playlist version=\"1\" xmlns=\"http://xspf.org/ns/0/\">\n"
            " <trackList>\n"
            "  <track><location>file:///home/me/Music/a%20b.flac</location>"
            "<title>A &amp; B</title><creator>Someone</creator><duration>183000</duration></track>\n"
            "  <track><location>http://radio.example.com/live</location></track>\n"
            "  <track><title>no location</title></track>\n"
            " </trackList>\n</playlist>\n", PlaylistFormat::XSPF);
        ASSERT_EQUALS(size_t(2), items.size(), "XSPF: tracks with a location");
        ASSERT_EQUALS(std::string("file:///home/me/Music/a%20b.flac"), items[0].location, "XSPF: location as written");
        ASSERT_EQUALS(std::string("A & B"), items[0].title, "XSPF: entities decoded");
        ASSERT_EQUALS(std::string("Someone"), items[0].artist, "XSPF: creator is the artist");
        ASSERT_EQUALS(183L, items[0].duration_seconds, "XSPF: milliseconds to seconds");
        ASSERT_EQUALS(std::string("http://radio.example.com/live"), items[1].location, "XSPF: URL");
    }

    /** @brief Mixed-case elements and attributes, entryref. */
    void testASX() {
        const auto items = PlaylistText::parse(
            "<ASX VERSION=\"3.0\">\n<TITLE>Station</TITLE>\n"
            "<Entry><Title>Live</Title><Author>DJ</Author><Ref HREF=\"mms://stream.example.com/live\"/>"
            "<ref href=\"http://stream.example.com/fallback\"/></Entry>\n"
            "<ENTRYREF HREF=\"http://example.com/more.asx\"/>\n</ASX>\n", PlaylistFormat::ASX);
        ASSERT_EQUALS(size_t(2), items.size(), "ASX: an entry and an entryref");
        ASSERT_EQUALS(std::string("http://stream.example.com/fallback"), items[0].location,
                      "ASX: the first ref that can be opened, not the mms:// before it");
        ASSERT_EQUALS(std::string("Live"), items[0].title, "ASX: title");
        ASSERT_EQUALS(std::string("DJ"), items[0].artist, "ASX: author is the artist");
        ASSERT_EQUALS(std::string("http://example.com/more.asx"), items[1].location, "ASX: entryref");
    }

    /** @brief Formats from text, extensions and media types. */
    void testDetection() {
        ASSERT_TRUE(PlaylistText::detect("[playlist]\nFile1=x", PlaylistFormat::Unknown) == PlaylistFormat::PLS,
                    "detect PLS");
        ASSERT_TRUE(PlaylistText::detect("<?xml version=\"1.0\"?><playlist xmlns=\"http://xspf.org/ns/0/\">",
                                         PlaylistFormat::Unknown) == PlaylistFormat::XSPF, "detect XSPF");
        ASSERT_TRUE(PlaylistText::detect("  <asx version=\"3\">", PlaylistFormat::Unknown) == PlaylistFormat::ASX,
                    "detect ASX");
        ASSERT_TRUE(PlaylistText::detect("#EXTM3U\n", PlaylistFormat::Unknown) == PlaylistFormat::M3U, "detect M3U");
        ASSERT_TRUE(PlaylistText::detect("song.mp3\n", PlaylistFormat::PLS) == PlaylistFormat::PLS,
                    "fall back on what the name said");
        ASSERT_TRUE(PlaylistText::formatForExtension("PLS") == PlaylistFormat::PLS, "extension, any case");
        ASSERT_TRUE(PlaylistText::formatForExtension("wax") == PlaylistFormat::ASX, "wax is ASX");
        ASSERT_TRUE(PlaylistText::formatForExtension("mp3") == PlaylistFormat::Unknown, "media is no playlist");
        ASSERT_TRUE(PlaylistText::formatForMimeType("audio/x-scpls") == PlaylistFormat::PLS, "PLS type");
        ASSERT_TRUE(PlaylistText::formatForMimeType("application/xspf+xml") == PlaylistFormat::XSPF, "XSPF type");
        ASSERT_TRUE(PlaylistText::formatForMimeType("audio/mpeg") == PlaylistFormat::Unknown, "audio is no playlist");
        // A tag name in an M3U's title or path is not its root element.
        ASSERT_TRUE(PlaylistText::detect("#EXTM3U\n#EXTINF:1,The <asx> Song\na.mp3\n", PlaylistFormat::Unknown) ==
                    PlaylistFormat::M3U, "#EXTM3U wins over a tag in a title");
        ASSERT_TRUE(PlaylistText::detect("music/<asx>/a.mp3\n", PlaylistFormat::M3U) == PlaylistFormat::M3U,
                    "a tag in a path is not ASX");
        ASSERT_TRUE(PlaylistText::detect("<?xml version=\"1.0\"?>\n<!-- made by hand -->\n<ASX version=\"3\">",
                                         PlaylistFormat::Unknown) == PlaylistFormat::ASX,
                    "ASX after a declaration and a comment");
    }

    /** @brief ASX as old players wrote it: not XML, href values unquoted. */
    void testLooseASX() {
        const auto items = PlaylistText::parse(
            "<asx version=3.0>\n<entry><ref href=mms://h/live /><ref href=http://h/live /></entry>\n"
            "<entry><ref href='http://h/two'></entry>\n<entryref href=http://h/more.asx>\n</asx>\n",
            PlaylistFormat::ASX);
        ASSERT_EQUALS(size_t(3), items.size(), "two entries and an entryref");
        ASSERT_EQUALS(std::string("http://h/live"), items[0].location, "unquoted, the openable ref");
        ASSERT_EQUALS(std::string("http://h/two"), items[1].location, "single-quoted");
        ASSERT_EQUALS(std::string("http://h/more.asx"), items[2].location, "entryref");
    }

    /** @brief URL recognition and file:// and %XX decoding. */
    void testHelpers() {
        ASSERT_TRUE(PlaylistText::isUrl("http://a/b"), "http URL");
        ASSERT_TRUE(PlaylistText::isUrl("mms://a/b"), "mms URL");
        ASSERT_FALSE(PlaylistText::isUrl("music/a.mp3"), "a relative path");
        ASSERT_FALSE(PlaylistText::isUrl("C://x.mp3"), "a drive letter is not a scheme");
        ASSERT_EQUALS(std::string("/home/me/a b.flac"), PlaylistText::fileUriToPath("file:///home/me/a%20b.flac"),
                      "file URI to path");
        ASSERT_EQUALS(std::string("/x.mp3"), PlaylistText::fileUriToPath("file://localhost/x.mp3"), "localhost");
        ASSERT_EQUALS(std::string(), PlaylistText::fileUriToPath("http://a/b"), "not a file URI");
#ifdef _WIN32
        ASSERT_EQUALS(std::string("//nas/music/a b.mp3"), PlaylistText::fileUriToPath("file://nas/music/a%20b.mp3"),
                      "another machine's file is a UNC path");
#else
        ASSERT_EQUALS(std::string(), PlaylistText::fileUriToPath("file://nas/music/a.mp3"),
                      "another machine's file has no path here");
#endif
        ASSERT_EQUALS(std::string("caf\xC3\xA9 50%"), PlaylistText::percentDecode("caf%C3%A9 50%"),
                      "escapes decoded, a stray % kept");
    }
};

} // namespace

/**
 * @brief Run the PlaylistText tests; the entry test_playlist_suite calls.
 * @return Number of failed tests.
 */
int test_playlist_text_main()
{
    TestFramework::TestSuite suite("PlaylistText Tests");
    suite.addTest(std::make_unique<PlaylistTextTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
