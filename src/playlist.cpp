/*
 * playlist.cpp - class implementation for playlist class
 * This file is part of PsyMP3.
 * Copyright © 2011-2024 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */


#include "psymp3.h"

Playlist::~Playlist()
{
    //dtor
}

bool Playlist::addFile(TagLib::String path)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    TagLib::FileRef *fileref;
    track *ntrk;
    try {
        std::cout << "Attempting open of " << path << std::endl;
        fileref = new TagLib::FileRef(path.toCString(true));
    } catch (const std::exception& e) { // Revert to catching std::exception, as TagLib::FileRef::NotAFile is not a standard TagLib exception.
        std::cerr << "Playlist::addFile(): Cannot add file " << path << ": " << e.what() << std::endl;
	    return false;
    }
    // Construct track directly in the vector, passing the raw pointer.
    // The track constructor will take ownership of 'fileref' via unique_ptr.
    tracks.emplace_back(path, fileref);
    return true;
}

bool Playlist::addFile(TagLib::String path, TagLib::String artist, TagLib::String title, long duration)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    // Construct track directly in the vector, passing EXTINF data.
    // The track constructor will handle creating TagLib::FileRef if needed.
    tracks.emplace_back(path, artist, title, duration);
    return true;
}





long Playlist::getPosition(void)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_position;
}

long Playlist::entries(void)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return tracks.size();
}

bool Playlist::setPosition(long position)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if(position < tracks.size()) {
        m_position = position;
        return true;
    } else {
        return false;
    }
}

TagLib::String Playlist::setPositionAndJump(long position)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    setPosition(position);
    return getTrack(position);
}

TagLib::String Playlist::getTrack(long position)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if(position < tracks.size()) {
        std::string path = tracks[position].GetFilePath().to8Bit(true);
        return tracks[position].GetFilePath();
    } else {
        return "";
    }
}

TagLib::String Playlist::next()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (tracks.empty()) return "";
    m_position++;
    if (m_position >= tracks.size()) {
        m_position = 0; // Wrap around to the beginning
    }
    return getTrack(m_position);
}

TagLib::String Playlist::prev()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (tracks.empty()) return "";
    if (m_position > 0) {
        m_position--;
    } else {
        m_position = tracks.size() - 1; // Wrap around to the end
    }
    return getTrack(m_position);
}

static TagLib::String joinPaths(const TagLib::String& base, const TagLib::String& relative) {
    if (base.isEmpty()) return relative;
    if (relative.isEmpty()) return base;

    TagLib::String result = base;
    // Check if base already ends with a separator or relative starts with one
    if (result.to8Bit(true).back() != '/' && result.to8Bit(true).back() != '\\' &&
        relative.to8Bit(true).front() != '/' && relative.to8Bit(true).front() != '\\') {
        result += "/"; // Default to Unix-style separator for joining
    }
    result += relative;
    return result;
}

std::unique_ptr<Playlist> Playlist::loadPlaylist(TagLib::String path)
{
    auto playlist = std::make_unique<Playlist>();
    std::ifstream file(path.toCString(true));
    if (!file.is_open()) {
        std::cerr << "Playlist::loadPlaylist(): Could not open playlist file: " << path << std::endl;
        return playlist; // Return empty playlist on failure
    }

    TagLib::String playlist_dir;
    std::string path_str = path.to8Bit(true);
    size_t last_slash = path_str.find_last_of("/\\"); // Handle both Unix and Windows separators
    if (last_slash != std::string::npos) {
        playlist_dir = TagLib::String(path_str.substr(0, last_slash + 1), TagLib::String::UTF8);
    } else {
        playlist_dir = TagLib::String("./", TagLib::String::UTF8); // Current directory if no path
    }

    std::string line;
    TagLib::String next_track_artist;
    TagLib::String next_track_title;
    long next_track_duration = 0;

    while (std::getline(file, line)) {
        // Trim whitespace from the line
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty()) {
            continue;
        }

        if (line.rfind("#EXTINF:", 0) == 0) { // Starts with #EXTINF:
            // Parse #EXTINF:duration,Artist - Title
            size_t comma_pos = line.find(',');
            if (comma_pos != std::string::npos) {
                std::string duration_str = line.substr(8, comma_pos - 8); // Skip "#EXTINF:"
                try {
                    next_track_duration = std::stol(duration_str);
                } catch (const std::exception& e) {
                    std::cerr << "Playlist::loadPlaylist(): Invalid duration in EXTINF: " << duration_str << std::endl;
                    next_track_duration = 0;
                }

                std::string title_artist_str = line.substr(comma_pos + 1);
                size_t dash_pos = title_artist_str.find(" - ");
                if (dash_pos != std::string::npos) {
                    next_track_artist = TagLib::String(title_artist_str.substr(0, dash_pos), TagLib::String::UTF8);
                    next_track_title = TagLib::String(title_artist_str.substr(dash_pos + 3), TagLib::String::UTF8);
                } else {
                    next_track_title = TagLib::String(title_artist_str, TagLib::String::UTF8);
                    next_track_artist = TagLib::String(); // No artist specified
                }
            }
        } else if (line[0] == '#') { // Other comments
            continue;
        } else { // It's a track path
            TagLib::String track_path(line, TagLib::String::UTF8);
            TagLib::String final_track_path;

            // Check if track_path is absolute (simple check for leading / or drive letter)
            if (track_path.to8Bit(true).length() > 0 && (track_path.to8Bit(true)[0] == '/' || (track_path.to8Bit(true).length() > 1 && track_path.to8Bit(true)[1] == ':'))) {
                final_track_path = track_path;
            } else {
                final_track_path = joinPaths(playlist_dir, track_path);
            }
            playlist->addFile(final_track_path, next_track_artist, next_track_title, next_track_duration);
            // Reset EXTINF data for the next track
            next_track_artist = TagLib::String();
            next_track_title = TagLib::String();
            next_track_duration = 0;
        }
    }

    file.close();
    return playlist;
}

void Playlist::savePlaylist(TagLib::String path)
{

}