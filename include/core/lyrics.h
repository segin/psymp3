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

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace Core {

/**
 * @brief Represents a single line of lyrics with optional timing information.
 */
struct LyricLine {
  std::chrono::milliseconds timestamp{0}; ///< When to show this line
  std::string text;                       ///< The lyric text
  bool is_synced{false};                  ///< Whether this line has timing

  LyricLine() = default;

  LyricLine(std::chrono::milliseconds time, std::string lyric_text,
            bool synced = true)
      : timestamp(time), text(std::move(lyric_text)), is_synced(synced) {}

  // Legacy constructor for backward compatibility
  LyricLine(unsigned int time_ms, const std::string &lyric_text,
            bool synced = true)
      : timestamp(std::chrono::milliseconds(time_ms)), text(lyric_text),
        is_synced(synced) {}

  // Convenience accessor for legacy code
  [[nodiscard]] unsigned int timestamp_ms() const noexcept {
    return static_cast<unsigned int>(timestamp.count());
  }
};

/**
 * @brief Container for parsed lyrics from various file formats.
 *
 * Supports LRC (synchronized) and plain text formats.
 * Thread-safe for concurrent reads after loading.
 */
class LyricsFile {
public:
  /// Maximum file size for lyrics files (10 MB)
  static constexpr size_t MAX_FILE_SIZE = 10 * 1024 * 1024;

  LyricsFile() = default;
  ~LyricsFile() = default;

  // Non-copyable but movable
  LyricsFile(const LyricsFile &) = delete;
  LyricsFile &operator=(const LyricsFile &) = delete;
  LyricsFile(LyricsFile &&) = default;
  LyricsFile &operator=(LyricsFile &&) = default;

  /**
   * @brief Loads lyrics from a file path.
   * @param file_path Path to the lyrics file (LRC or TXT)
   * @return true if lyrics were loaded successfully
   */
  [[nodiscard]] bool loadFromFile(std::string_view file_path);

  /**
   * @brief Gets the lyric line for the given playback time.
   * @param current_time Current playback position
   * @return Pointer to current line, or nullptr if none
   */
  [[nodiscard]] const LyricLine *
  getCurrentLine(std::chrono::milliseconds current_time) const noexcept;

  // Legacy overload
  [[nodiscard]] const LyricLine *
  getCurrentLine(unsigned int current_time_ms) const noexcept {
    return getCurrentLine(std::chrono::milliseconds(current_time_ms));
  }

  /**
   * @brief Gets upcoming lyric lines for preview.
   * @param current_time Current playback position
   * @param count Maximum number of lines to return
   * @return Vector of pointers to upcoming lines
   */
  [[nodiscard]] std::vector<const LyricLine *>
  getUpcomingLines(std::chrono::milliseconds current_time,
                   size_t count = 3) const;

  // Legacy overload
  [[nodiscard]] std::vector<const LyricLine *>
  getUpcomingLines(unsigned int current_time_ms, size_t count = 3) const {
    return getUpcomingLines(std::chrono::milliseconds(current_time_ms), count);
  }

  /**
   * @brief Checks if lyrics have timing information.
   */
  [[nodiscard]] bool hasTiming() const noexcept { return m_has_timing; }

  /**
   * @brief Checks if any lyrics are loaded.
   */
  [[nodiscard]] bool hasLyrics() const noexcept { return !m_lines.empty(); }

  /**
   * @brief Gets the number of lyric lines.
   */
  [[nodiscard]] size_t lineCount() const noexcept { return m_lines.size(); }

  /**
   * @brief Gets all lyric lines.
   */
  [[nodiscard]] const std::vector<LyricLine> &getLines() const noexcept {
    return m_lines;
  }

  /**
   * @brief Gets the song title from lyrics metadata.
   */
  [[nodiscard]] std::string_view title() const noexcept { return m_title; }

  /**
   * @brief Gets the artist from lyrics metadata.
   */
  [[nodiscard]] std::string_view artist() const noexcept { return m_artist; }

  /**
   * @brief Clears all loaded lyrics and metadata.
   */
  void clear() noexcept;

private:
  std::vector<LyricLine> m_lines;
  bool m_has_timing{false};
  std::string m_title;
  std::string m_artist;

  [[nodiscard]] bool parseLRC(std::string_view content);
  [[nodiscard]] bool parsePlainText(std::string_view content);
  [[nodiscard]] std::ptrdiff_t
  findCurrentLineIndex(std::chrono::milliseconds current_time) const noexcept;
};

/**
 * @brief Utility functions for lyrics file discovery.
 */
namespace LyricsUtils {

/// Supported lyrics file extensions
inline constexpr std::array<std::string_view, 3> LYRICS_EXTENSIONS = {
    ".lrc", ".srt", ".txt"};

/**
 * @brief Attempts to find a lyrics file for the given audio file.
 * @param audio_file_path Path to the audio file
 * @return Path to lyrics file if found, empty string otherwise
 */
[[nodiscard]] std::string findLyricsFile(std::string_view audio_file_path);

/**
 * @brief Checks if a file path has a lyrics file extension.
 * @param file_path Path to check
 * @return true if the extension indicates a lyrics file
 */
[[nodiscard]] bool isLyricsFile(std::string_view file_path) noexcept;

/**
 * @brief Trims whitespace from both ends of a string view.
 * @param str String to trim
 * @return Trimmed string view
 */
[[nodiscard]] std::string_view trim(std::string_view str) noexcept;

} // namespace LyricsUtils

} // namespace Core
} // namespace PsyMP3

#endif // LYRICS_H