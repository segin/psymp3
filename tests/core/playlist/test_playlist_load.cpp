/*
 * test_playlist_load.cpp - Unit tests for Playlist::loadPlaylist
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include <fstream>
#include <cstdio>
#include <string>

using namespace TestFramework;

class PlaylistLoadTest : public TestCase {
public:
    PlaylistLoadTest() : TestCase("Playlist Load Test") {}

protected:
    void runTest() override {
        testLoadSimplePlaylist();
        testLoadPlaylistWithMetadata();
        testLoadPlaylistWithRelativePaths();
        testLoadEmptyPlaylist();
        testLoadNonExistentPlaylist();
        testLoadPlaylistWithComments();
        testResolveInlineSourcesPreservesOrder();
        testResolveInlineSourcesRecursesNestedPlaylists();
        testResolveInlineSourcesKeepsStreamUrls();
        testLoadKeepsUrlLines();
        testLoadOtherFormats();
        testLoadPlaylistWithInvalidDuration();
    }

private:
    std::string createTempM3U(const std::string& content, const std::string& filename = "temp_playlist.m3u") {
        std::ofstream out(filename);
        if (out.is_open()) {
            out << content;
            out.close();
        }
        return filename;
    }

    void testLoadSimplePlaylist() {
        std::string content = "/path/to/song1.mp3\n/path/to/song2.mp3\n";
        std::string filename = createTempM3U(content);

        auto playlist = Playlist::loadPlaylist(TagLib::String(filename, TagLib::String::UTF8));

        ASSERT_NOT_NULL(playlist.get(), "Playlist should not be null");
        ASSERT_EQUALS(2, playlist->entries(), "Playlist should have 2 entries");

        std::string track1 = playlist->getTrack(0).to8Bit(true);
        std::string track2 = playlist->getTrack(1).to8Bit(true);

        ASSERT_TRUE(track1.find("song1.mp3") != std::string::npos, "First track should be song1.mp3");
        ASSERT_TRUE(track2.find("song2.mp3") != std::string::npos, "Second track should be song2.mp3");

        std::remove(filename.c_str());
    }

    void testLoadPlaylistWithMetadata() {
        std::string content = "#EXTM3U\n#EXTINF:123,Artist Name - Song Title\n/path/to/song.mp3\n";
        std::string filename = createTempM3U(content);

        auto playlist = Playlist::loadPlaylist(TagLib::String(filename, TagLib::String::UTF8));

        ASSERT_NOT_NULL(playlist.get(), "Playlist should not be null");
        ASSERT_EQUALS(1, playlist->entries(), "Playlist should have 1 entry");

        auto t = playlist->getTrackInfo(0);
        ASSERT_TRUE(t.has_value(), "Track info should not be null");

        ASSERT_EQUALS("Artist Name", t->artist.to8Bit(true), "Artist should be parsed correctly");
        ASSERT_EQUALS("Song Title", t->title.to8Bit(true), "Title should be parsed correctly");
        ASSERT_EQUALS(123u, t->length_seconds, "Duration should be parsed correctly");

        std::remove(filename.c_str());
    }

    void testLoadPlaylistWithRelativePaths() {
        std::string content = "song_relative.mp3\n./subdir/song_subdir.mp3\n";
        std::string filename = "temp_relative.m3u";
        createTempM3U(content, filename);

        auto playlist = Playlist::loadPlaylist(TagLib::String(filename, TagLib::String::UTF8));

        ASSERT_NOT_NULL(playlist.get(), "Playlist should not be null");
        ASSERT_EQUALS(2, playlist->entries(), "Playlist should have 2 entries");

        std::string track1 = playlist->getTrack(0).to8Bit(true);

        // "./" + "song_relative.mp3" -> "./song_relative.mp3"
        // The implementation joins the directory of the playlist with the relative path.
        // Since we are running in the current directory (presumably), the directory part is likely "./".

        ASSERT_TRUE(track1.find("song_relative.mp3") != std::string::npos, "Relative path should be preserved");

        std::remove(filename.c_str());
    }

    void testLoadEmptyPlaylist() {
        std::string filename = createTempM3U("");

        auto playlist = Playlist::loadPlaylist(TagLib::String(filename, TagLib::String::UTF8));

        ASSERT_NOT_NULL(playlist.get(), "Playlist should not be null");
        ASSERT_EQUALS(0, playlist->entries(), "Playlist should be empty");

        std::remove(filename.c_str());
    }

    void testLoadNonExistentPlaylist() {
        std::string filename = "non_existent_file_12345.m3u";

        auto playlist = Playlist::loadPlaylist(TagLib::String(filename, TagLib::String::UTF8));

        ASSERT_NOT_NULL(playlist.get(), "Playlist should not be null even on failure");
        ASSERT_EQUALS(0, playlist->entries(), "Playlist should be empty on failure");
    }

    void testLoadPlaylistWithComments() {
        std::string content = "# This is a comment\n#Another comment\n/path/to/song.mp3\n \n#End comment";
        std::string filename = createTempM3U(content);

        auto playlist = Playlist::loadPlaylist(TagLib::String(filename, TagLib::String::UTF8));

        ASSERT_NOT_NULL(playlist.get(), "Playlist should not be null");
        ASSERT_EQUALS(1, playlist->entries(), "Playlist should have 1 entry");

        std::string track = playlist->getTrack(0).to8Bit(true);
        ASSERT_TRUE(track.find("song.mp3") != std::string::npos, "Track should be loaded correctly ignoring comments");

        std::remove(filename.c_str());
    }

    void testResolveInlineSourcesPreservesOrder() {
        std::string inline_playlist = createTempM3U("song2.mp3\nsong3.m4a\n", "temp_inline_playlist.m3u");

        std::vector<TagLib::String> sources = {
            TagLib::String("file1.flac", TagLib::String::UTF8),
            TagLib::String(inline_playlist, TagLib::String::UTF8),
            TagLib::String("file4.flac", TagLib::String::UTF8)
        };

        auto resolved = Playlist::resolveInlineSources(sources);

        ASSERT_EQUALS(4, resolved.size(), "Inline playlist sources should expand in place");
        ASSERT_TRUE(resolved[0].path.to8Bit(true).find("file1.flac") != std::string::npos, "First entry should remain file1.flac");
        ASSERT_TRUE(resolved[1].path.to8Bit(true).find("song2.mp3") != std::string::npos, "Inline playlist should insert song2.mp3 second");
        ASSERT_TRUE(resolved[2].path.to8Bit(true).find("song3.m4a") != std::string::npos, "Inline playlist should insert song3.m4a third");
        ASSERT_TRUE(resolved[3].path.to8Bit(true).find("file4.flac") != std::string::npos, "Fourth entry should remain file4.flac");

        std::remove(inline_playlist.c_str());
    }

    void testResolveInlineSourcesRecursesNestedPlaylists() {
        std::string inner_playlist = createTempM3U("song2.mp3\nsong3.m4a\n", "temp_inner_playlist.m3u");
        std::string outer_content = inner_playlist + "\nfinal_track.flac\n";
        std::string outer_playlist = createTempM3U(outer_content, "temp_outer_playlist.m3u");

        std::vector<TagLib::String> sources = {
            TagLib::String("intro.flac", TagLib::String::UTF8),
            TagLib::String(outer_playlist, TagLib::String::UTF8)
        };

        auto resolved = Playlist::resolveInlineSources(sources);

        ASSERT_EQUALS(4, resolved.size(), "Nested playlists should resolve recursively");
        ASSERT_TRUE(resolved[0].path.to8Bit(true).find("intro.flac") != std::string::npos, "First entry should remain intro.flac");
        ASSERT_TRUE(resolved[1].path.to8Bit(true).find("song2.mp3") != std::string::npos, "Nested playlist should insert first inner track");
        ASSERT_TRUE(resolved[2].path.to8Bit(true).find("song3.m4a") != std::string::npos, "Nested playlist should insert second inner track");
        ASSERT_TRUE(resolved[3].path.to8Bit(true).find("final_track.flac") != std::string::npos, "Outer playlist entries should continue after nested expansion");

        std::remove(inner_playlist.c_str());
        std::remove(outer_playlist.c_str());
    }

    /**
     * @brief Stream URLs stay single entries whatever they are called: one
     *        ending .m3u8 (an HLS stream) is not read as a local playlist
     *        file, and one with no extension is kept as it is.
     */
    void testResolveInlineSourcesKeepsStreamUrls() {
        std::vector<TagLib::String> sources = {
            TagLib::String("http://stream.revma.ihrhls.com/zc7001/hls.m3u8", TagLib::String::UTF8),
            TagLib::String("HTTPS://example.com/list.m3u", TagLib::String::UTF8),
            TagLib::String("https://cloud.revma.ihrhls.com/zc7001", TagLib::String::UTF8),
        };

        auto resolved = Playlist::resolveInlineSources(sources);

        ASSERT_EQUALS(3, resolved.size(), "Each URL is one entry");
        ASSERT_EQUALS(std::string("http://stream.revma.ihrhls.com/zc7001/hls.m3u8"), resolved[0].path.to8Bit(true),
                      "An .m3u8 URL is kept, not loaded as a playlist file");
        ASSERT_EQUALS(std::string("HTTPS://example.com/list.m3u"), resolved[1].path.to8Bit(true),
                      "The scheme is matched without regard to case");
        ASSERT_EQUALS(std::string("https://cloud.revma.ihrhls.com/zc7001"), resolved[2].path.to8Bit(true),
                      "A URL without an extension is kept");
    }

    /**
     * @brief A URL line in a local .m3u stays the URL; it used to be glued
     *        onto the playlist's folder, giving "./http://...".
     */
    void testLoadKeepsUrlLines() {
        std::string filename = createTempM3U(
            "#EXTM3U\n#EXTINF:-1,Rock 181\nhttps://listen.181fm.com/181-rock_128k.mp3\nlocal.flac\n",
            "temp_url_lines.m3u");
        auto entries = Playlist::loadPlaylistEntries(TagLib::String(filename, TagLib::String::UTF8));
        ASSERT_EQUALS(2, entries.size(), "two entries");
        ASSERT_EQUALS(std::string("https://listen.181fm.com/181-rock_128k.mp3"), entries[0].path.to8Bit(true),
                      "the URL as written");
        ASSERT_EQUALS(std::string("./local.flac"), entries[1].path.to8Bit(true), "a relative path from the folder");
        std::remove(filename.c_str());
    }

    /**
     * @brief .pls, .xspf and .asx files load: URLs kept, file:// URIs made
     *        paths, XSPF's percent-escaped relative locations decoded.
     */
    void testLoadOtherFormats() {
        std::string pls = createTempM3U(
            "[playlist]\nFile1=http://ice.example.com/radio\nTitle1=Example FM\nLength1=-1\nNumberOfEntries=1\n",
            "temp_station.pls");
        auto entries = Playlist::loadPlaylistEntries(TagLib::String(pls, TagLib::String::UTF8));
        ASSERT_EQUALS(1, entries.size(), "PLS: one entry");
        ASSERT_EQUALS(std::string("http://ice.example.com/radio"), entries[0].path.to8Bit(true), "PLS: the URL");
        ASSERT_EQUALS(std::string("Example FM"), entries[0].title.to8Bit(true), "PLS: the title");
        std::remove(pls.c_str());

        std::string xspf = createTempM3U(
            "<?xml version=\"1.0\"?><playlist version=\"1\" xmlns=\"http://xspf.org/ns/0/\"><trackList>"
            "<track><location>file:///music/a%20b.flac</location></track>"
            "<track><location>sub/c%20d.mp3</location></track>"
            "</trackList></playlist>", "temp_list.xspf");
        entries = Playlist::loadPlaylistEntries(TagLib::String(xspf, TagLib::String::UTF8));
        ASSERT_EQUALS(2, entries.size(), "XSPF: two entries");
        ASSERT_EQUALS(std::string("/music/a b.flac"), entries[0].path.to8Bit(true), "XSPF: file URI to path");
        ASSERT_EQUALS(std::string("./sub/c d.mp3"), entries[1].path.to8Bit(true), "XSPF: relative, decoded");
        std::remove(xspf.c_str());

        // Locations that decode to no usable name are dropped, not opened as another.
        std::string broken = createTempM3U(
            "<?xml version=\"1.0\"?><playlist version=\"1\" xmlns=\"http://xspf.org/ns/0/\"><trackList>"
            "<track><location>song.mp3%00.txt</location></track>"
            "<track><location>bad%FFname.mp3</location></track>"
            "<track><location>file:///music/ok.flac</location></track>"
            "</trackList></playlist>", "temp_broken.xspf");
        entries = Playlist::loadPlaylistEntries(TagLib::String(broken, TagLib::String::UTF8));
        ASSERT_EQUALS(1, entries.size(), "XSPF: a NUL and a non-UTF-8 byte are dropped");
        ASSERT_EQUALS(std::string("/music/ok.flac"), entries[0].path.to8Bit(true), "the good entry stays");
        std::remove(broken.c_str());

        std::string asx = createTempM3U(
            "<asx version=\"3.0\"><entry><title>Live</title><ref href=\"http://stream.example.com/live\"/></entry></asx>",
            "temp_station.asx");
        entries = Playlist::loadPlaylistEntries(TagLib::String(asx, TagLib::String::UTF8));
        ASSERT_EQUALS(1, entries.size(), "ASX: one entry");
        ASSERT_EQUALS(std::string("http://stream.example.com/live"), entries[0].path.to8Bit(true), "ASX: the URL");
        std::remove(asx.c_str());
    }

    void testLoadPlaylistWithInvalidDuration() {
        std::string content = "#EXTM3U\n#EXTINF:invalid_duration,Artist Name - Song Title\n/path/to/song.mp3\n";
        std::string filename = createTempM3U(content, "temp_invalid_duration.m3u");

        auto playlist = Playlist::loadPlaylist(TagLib::String(filename, TagLib::String::UTF8));

        ASSERT_NOT_NULL(playlist.get(), "Playlist should not be null");
        ASSERT_EQUALS(1, playlist->entries(), "Playlist should have 1 entry");

        auto t = playlist->getTrackInfo(0);
        ASSERT_TRUE(t.has_value(), "Track info should not be null");

        ASSERT_EQUALS(0u, t->length_seconds, "Duration should be 0 for invalid EXTINF duration");

        std::remove(filename.c_str());
    }
};

int test_playlist_load_main() {
    TestSuite suite("Playlist Load Tests");
    suite.addTest(std::make_unique<PlaylistLoadTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
