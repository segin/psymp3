/*
 * test_playlist_load.cpp - Unit tests for Playlist loading functionality
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
#include <unistd.h>

using namespace TestFramework;

// Helper to get current working directory
std::string get_cwd() {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        return std::string(cwd);
    } else {
        return ".";
    }
}

void test_load_simple_m3u() {
    std::string filename = "test_simple.m3u";
    std::ofstream file(filename);
    ASSERT_TRUE(file.is_open(), "Failed to create temporary file");

    file << "/path/to/song1.mp3\n";
    file << "/path/to/song2.mp3\n";
    file.close();

    auto playlist = Playlist::loadPlaylist(TagLib::String(filename, TagLib::String::UTF8));

    ASSERT_NOT_NULL(playlist.get(), "Playlist should not be null");
    ASSERT_EQUALS(2, playlist->entries(), "Should have 2 entries");

    ASSERT_EQUALS(TagLib::String("/path/to/song1.mp3"), playlist->getTrack(0), "First track path mismatch");
    ASSERT_EQUALS(TagLib::String("/path/to/song2.mp3"), playlist->getTrack(1), "Second track path mismatch");

    std::remove(filename.c_str());
}

void test_load_extended_m3u() {
    std::string filename = "test_ext.m3u";
    std::ofstream file(filename);
    ASSERT_TRUE(file.is_open(), "Failed to create temporary file");

    file << "#EXTM3U\n";
    file << "#EXTINF:123,Artist1 - Title1\n";
    file << "/path/to/song1.mp3\n";
    file << "#EXTINF:456,Title2\n";
    file << "song2.mp3\n";
    file.close();

    auto playlist = Playlist::loadPlaylist(TagLib::String(filename, TagLib::String::UTF8));

    ASSERT_NOT_NULL(playlist.get(), "Playlist should not be null");
    ASSERT_EQUALS(2, playlist->entries(), "Should have 2 entries");

    // Check first track
    const track* t1 = playlist->getTrackInfo(0);
    ASSERT_NOT_NULL(t1, "Track 1 info should not be null");
    ASSERT_EQUALS(TagLib::String("Artist1"), t1->GetArtist(), "Track 1 artist mismatch");
    ASSERT_EQUALS(TagLib::String("Title1"), t1->GetTitle(), "Track 1 title mismatch");
    ASSERT_EQUALS(123, t1->GetLen(), "Track 1 length mismatch");
    ASSERT_EQUALS(TagLib::String("/path/to/song1.mp3"), t1->GetFilePath(), "Track 1 path mismatch");

    // Check second track (relative path resolution)
    const track* t2 = playlist->getTrackInfo(1);
    ASSERT_NOT_NULL(t2, "Track 2 info should not be null");
    // Artist might be empty or unknown depending on implementation when only title is given
    // But title should be "Title2"
    ASSERT_EQUALS(TagLib::String("Title2"), t2->GetTitle(), "Track 2 title mismatch");
    ASSERT_EQUALS(456, t2->GetLen(), "Track 2 length mismatch");

    // Construct expected absolute path for song2.mp3
    std::string cwd = get_cwd();
    std::string expected_path = cwd + "/song2.mp3";
    // Normalize slashes if needed, but let's assume simple concatenation for now
    // Actually Playlist::loadPlaylist uses joinPaths which handles separators.
    // We should check if it ends with /song2.mp3
    std::string actual_path = t2->GetFilePath().to8Bit(true);
    // On windows it might be different, but let's assume unix-like for now or check suffix
    bool ends_with = actual_path.length() >= 10 && actual_path.substr(actual_path.length() - 10) == "/song2.mp3";
#ifdef _WIN32
    if (!ends_with) ends_with = actual_path.length() >= 10 && actual_path.substr(actual_path.length() - 10) == "\\song2.mp3";
#endif
    ASSERT_TRUE(ends_with, "Track 2 path should end with /song2.mp3");

    std::remove(filename.c_str());
}

void test_load_nonexistent_file() {
    auto playlist = Playlist::loadPlaylist(TagLib::String("nonexistent.m3u", TagLib::String::UTF8));
    ASSERT_NOT_NULL(playlist.get(), "Playlist should not be null even on failure");
    ASSERT_EQUALS(0, playlist->entries(), "Should have 0 entries");
}

void test_load_empty_file() {
    std::string filename = "test_empty.m3u";
    std::ofstream file(filename);
    file.close();

    auto playlist = Playlist::loadPlaylist(TagLib::String(filename, TagLib::String::UTF8));
    ASSERT_NOT_NULL(playlist.get(), "Playlist should not be null");
    ASSERT_EQUALS(0, playlist->entries(), "Should have 0 entries");

    std::remove(filename.c_str());
}

int main() {
    TestSuite suite("Playlist Loading Tests");

    suite.addTest("Load Simple M3U", test_load_simple_m3u);
    suite.addTest("Load Extended M3U", test_load_extended_m3u);
    suite.addTest("Load Non-existent File", test_load_nonexistent_file);
    suite.addTest("Load Empty File", test_load_empty_file);

    auto results = suite.runAll();
    suite.printResults(results);

    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}
