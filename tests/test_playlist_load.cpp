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

        const track* t = playlist->getTrackInfo(0);
        ASSERT_NOT_NULL(t, "Track info should not be null");

        ASSERT_EQUALS("Artist Name", t->GetArtist().to8Bit(true), "Artist should be parsed correctly");
        ASSERT_EQUALS("Song Title", t->GetTitle().to8Bit(true), "Title should be parsed correctly");
        ASSERT_EQUALS(123u, t->GetLen(), "Duration should be parsed correctly");

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
};

int main() {
    TestSuite suite("Playlist Load Tests");
    suite.addTest(std::make_unique<PlaylistLoadTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
