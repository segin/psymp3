/*
 * lyrics.h - Lyrics file support and data structures
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
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

#ifndef LYRICS_H
#define LYRICS_H

#include <vector>
#include <string>
#include <memory>

/**
 * @brief Represents a single line of lyrics with optional timing information.
 */
struct LyricLine {
    unsigned int timestamp_ms;  ///< When to show this line (0 for unsynced)
    std::string text;          ///< The lyric text
    bool is_synced;            ///< Whether this line has timing information
    
    LyricLine(unsigned int time_ms, const std::string& lyric_text, bool synced = true)
        : timestamp_ms(time_ms), text(lyric_text), is_synced(synced) {}
};

/**
 * @brief Container for parsed lyrics from various file formats.
 */
class LyricsFile {
public:
    LyricsFile() : m_has_timing(false) {}
    
    /**
     * @brief Loads lyrics from a file path.
     * Supports LRC, SRT, and plain text formats.
     * @param file_path Path to the lyrics file
     * @return true if lyrics were loaded successfully
     */
    bool loadFromFile(const std::string& file_path);
    
    /**
     * @brief Gets the lyric line that should be displayed at the given time.
     * @param current_time_ms Current playback position in milliseconds
     * @return Pointer to the current lyric line, or nullptr if no lyrics at this time
     */
    const LyricLine* getCurrentLine(unsigned int current_time_ms) const;
    
    /**
     * @brief Gets upcoming lyric lines for preview.
     * @param current_time_ms Current playback position in milliseconds
     * @param count Number of upcoming lines to return
     * @return Vector of upcoming lyric lines
     */
    std::vector<const LyricLine*> getUpcomingLines(unsigned int current_time_ms, size_t count = 3) const;
    
    /**
     * @brief Checks if this lyrics file has timing information.
     * @return true if lyrics are synchronized with timestamps
     */
    bool hasTiming() const { return m_has_timing; }
    
    /**
     * @brief Checks if lyrics are available.
     * @return true if there are any lyrics loaded
     */
    bool hasLyrics() const { return !m_lines.empty(); }
    
    /**
     * @brief Gets all lyric lines.
     * @return Reference to the vector of lyric lines
     */
    const std::vector<LyricLine>& getLines() const { return m_lines; }
    
    /**
     * @brief Clears all loaded lyrics.
     */
    void clear();

private:
    std::vector<LyricLine> m_lines;  ///< All lyric lines
    bool m_has_timing;               ///< Whether lyrics have timing info
    std::string m_title;             ///< Song title from lyrics metadata
    std::string m_artist;            ///< Artist from lyrics metadata
    
    /**
     * @brief Parses LRC format lyrics.
     * @param content File content as string
     * @return true if parsing succeeded
     */
    bool parseLRC(const std::string& content);
    
    /**
     * @brief Parses plain text lyrics.
     * @param content File content as string
     * @return true if parsing succeeded
     */
    bool parsePlainText(const std::string& content);
    
    /**
     * @brief Finds the index of the lyric line for the given time.
     * @param current_time_ms Current playback position
     * @return Index of the current line, or -1 if none found
     */
    int findCurrentLineIndex(unsigned int current_time_ms) const;
};

/**
 * @brief Utility functions for lyrics file discovery.
 */
namespace LyricsUtils {
    /**
     * @brief Attempts to find a lyrics file for the given audio file.
     * Checks for .lrc, .srt, and .txt files with the same base name.
     * @param audio_file_path Path to the audio file
     * @return Path to lyrics file if found, empty string otherwise
     */
    std::string findLyricsFile(const std::string& audio_file_path);
    
    /**
     * @brief Checks if a file extension indicates a lyrics file.
     * @param file_path Path to check
     * @return true if the file appears to be a lyrics file
     */
    bool isLyricsFile(const std::string& file_path);
}

#endif // LYRICS_H