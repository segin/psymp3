/*
 * playlist.cpp - class implementation for playlist class
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
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

Playlist::Playlist() { }

/**
 * @brief Destroys the Playlist object.
 *
 * The default destructor is sufficient as the std::vector of tracks will handle its own cleanup.
 */
Playlist::~Playlist()
{
    //dtor
}

/**
 * @brief Adds a single media file to the end of the playlist.
 *
 * This method creates a `track` object from the given file path, which triggers
 * TagLib to read its metadata.
 * @param path The full file path of the media file to add.
 * @return `true` if the file was added successfully, `false` otherwise.
 */
bool Playlist::addFile(TagLib::String path)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    Debug::log("playlist", "Playlist::addFile(): Attempting to add file: ", path.to8Bit(true));
    try {
        // Construct track directly in the vector. The track constructor will handle tag loading.
        tracks.emplace_back(path);
        Debug::log("playlist", "Playlist::addFile(): Successfully added file: ", path.to8Bit(true));
        return true;
    } catch (const std::exception& e) {
        Debug::log("playlist", "Playlist::addFile(): Failed to create track for ", path.to8Bit(true), ": ", e.what());
        std::cerr << "Playlist::addFile(): Could not create track for " << path << ": " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Adds a media file to the playlist using pre-parsed metadata.
 *
 * This version is used when loading playlists (like M3U) that already contain
 * metadata, avoiding the need to re-read every file's tags.
 * @param path The full file path of the media file.
 * @param artist The artist name from the playlist metadata.
 * @param title The track title from the playlist metadata.
 * @param duration The track duration in seconds from the playlist metadata.
 * @return `true` if the file was added successfully.
 */
bool Playlist::addFile(TagLib::String path, TagLib::String artist, TagLib::String title, long duration)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    // Construct track directly in the vector, passing EXTINF data.
    // The track constructor will handle creating TagLib::FileRef if needed.
    tracks.emplace_back(path, artist, title, duration);
    return true;
}




/**
 * @brief Gets the current playback position (index) in the playlist.
 * @return The zero-based index of the currently playing or selected track.
 */
long Playlist::getPosition(void)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_position;
}

/**
 * @brief Gets the total number of tracks in the playlist.
 * @return The total number of entries.
 */
long Playlist::entries(void)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return tracks.size();
}

/**
 * @brief Sets the current playback position to a specific index.
 *
 * This method does not trigger playback, it only updates the internal cursor.
 * @param position The new zero-based index to set.
 * @return `true` if the position was valid and set successfully, `false` otherwise.
 */
bool Playlist::setPosition(long position)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if(position >= 0 && static_cast<size_t>(position) < tracks.size()) {
        m_position = position;
        return true;
    } else {
        return false;
    }
}

/**
 * @brief Sets the playback position and returns the file path at that new position.
 * @param position The new zero-based index to jump to.
 * @return The file path of the track at the new position, or an empty string if invalid.
 */
TagLib::String Playlist::setPositionAndJump(long position)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    setPosition(position);
    return getTrack(position);
}

/**
 * @brief Gets the file path of the track at a specific index.
 * @param position The zero-based index of the desired track.
 * @return The file path of the track, or an empty string if the index is out of bounds.
 */
TagLib::String Playlist::getTrack(long position) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if(position >= 0 && static_cast<size_t>(position) < tracks.size()) {
        return tracks[position].GetFilePath();
    } else {
        return "";
    }
}

/**
 * @brief Advances the playback position to the next track.
 *
 * If the end of the playlist is reached, it wraps around to the beginning.
 * @return The file path of the next track.
 */
TagLib::String Playlist::next()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (tracks.empty()) return "";
    m_position++;
    if (static_cast<size_t>(m_position) >= tracks.size()) {
        m_position = 0; // Wrap around to the beginning
    }
    return getTrack(m_position);
}

/**
 * @brief Moves the playback position to the previous track.
 *
 * If the beginning of the playlist is reached, it wraps around to the end.
 * @return The file path of the previous track.
 */
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

/**
 * @brief Gets the file path of the next track without advancing the playback position.
 *
 * This is useful for pre-loading. It wraps around if the current track is the last one.
 * @return The file path of the next track in the sequence.
 */
TagLib::String Playlist::peekNext() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (tracks.empty()) {
        return "";
    }

    long next_pos = m_position + 1;
    if (static_cast<size_t>(next_pos) >= tracks.size()) {
        next_pos = 0; // Wrap around
    }
    
    return getTrack(next_pos);
}

/**
 * @brief Gets a pointer to the full track object at a specific index.
 *
 * This provides access to all metadata (artist, title, length, etc.).
 * @param position The zero-based index of the desired track.
 * @return A const pointer to the `track` object, or `nullptr` if the index is out of bounds.
 */
const track* Playlist::getTrackInfo(long position) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (position >= 0 && static_cast<size_t>(position) < tracks.size()) {
        return &tracks[position];
    }
    return nullptr;
}

/**
 * @brief Checks if a given path is absolute.
 * @param path The path to check.
 * @return True if the path is absolute, false otherwise.
 */
static bool isAbsolutePath(const TagLib::String& path) {
    std::string p = path.to8Bit(true);
    if (p.empty()) {
        return false;
    }
#ifdef _WIN32
    // e.g., "C:\..." or "C:/"
    if (p.length() >= 3 && isalpha(p[0]) && p[1] == ':' && (p[2] == '\\' || p[2] == '/')) {
        return true;
    }
    // e.g., "\\server\share" or "//server/share" (UNC)
    if (p.length() >= 2 && (p[0] == '\\' || p[0] == '/') && (p[1] == '\\' || p[1] == '/')) {
        return true;
    }
#endif
    // e.g., "/path/to/file" on Unix, or "\path\to\file" on Windows (absolute to current drive root)
    if (p[0] == '/' || p[0] == '\\') {
        return true;
    }
    return false;
}

/**
 * @brief Joins a base directory path and a relative path.
 * @param base The base directory path.
 * @param relative The relative path to append.
 * @return The combined path.
 */
static TagLib::String joinPaths(const TagLib::String& base, const TagLib::String& relative) {
    if (base.isEmpty()) return relative;
    if (relative.isEmpty()) return base;

    std::string base_str = base.to8Bit(true);
    if (base_str.back() != '/' && base_str.back() != '\\') {
        base_str += '/'; // Use forward slash as a universal separator
    }
    return TagLib::String(base_str, TagLib::String::UTF8) + relative;
}

/**
 * @brief Loads a playlist from a file (M3U or M3U8 format).
 *
 * This static factory method parses the specified file, resolving relative paths
 * and reading EXTINF metadata.
 * @param path The file path of the playlist to load.
 * @return A `std::unique_ptr<Playlist>` containing the loaded tracks. Returns a pointer to an empty playlist on failure.
 */
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

            if (isAbsolutePath(track_path)) {
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

/**
 * @brief Saves the current playlist to a file in extended M3U format.
 *
 * The saved file will include #EXTINF metadata for each track.
 * @param path The destination file path for the new playlist file.
 */
void Playlist::savePlaylist(TagLib::String path)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::ofstream file(path.toCString(true));
    if (!file.is_open()) {
        std::cerr << "Playlist::savePlaylist(): Unable to open " << path.to8Bit(true) << " for writing!" << std::endl;
        return;
    }

    file << "#EXTM3U\n";

    for (const auto& track : tracks) {
        long duration = track.GetLen();
        std::string artist = track.GetArtist().to8Bit(true);
        std::string title = track.GetTitle().to8Bit(true);
        std::string track_path = track.GetFilePath().to8Bit(true);

        file << "#EXTINF:" << duration << "," << artist << " - " << title << "\n";
        file << track_path << "\n";
    }

    file.close();
    std::cout << "Playlist saved to " << path.to8Bit(true) << std::endl;
}

