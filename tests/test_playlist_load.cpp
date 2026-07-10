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

int main() {
    TestSuite suite("Playlist Load Tests");
    suite.addTest(std::make_unique<PlaylistLoadTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
