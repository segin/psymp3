/*
 * lyrics.cpp - Implementation for lyrics file support
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

// GCC 13 has a known bug where it generates false-positive "may be used
// uninitialized" warnings in libstdc++ <regex> internals when -O2 is enabled.
// This is a compiler/library bug, not a code issue. Suppress it here.
// See: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105562
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 13
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include "psymp3.h"

#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 13
#pragma GCC diagnostic pop
#endif

namespace PsyMP3 {
namespace Core {

bool LyricsFile::loadFromFile(const std::string &file_path) {
  clear();

  try {
    // Use IOHandler to read the file - this may throw InvalidMediaException
    auto io_handler = std::make_unique<FileIOHandler>(
        TagLib::String(file_path, TagLib::String::UTF8));

    // Get file size using seek/tell
    io_handler->seek(0, SEEK_END);
    long file_size = io_handler->tell();
    io_handler->seek(0, SEEK_SET);

    if (file_size <= 0 ||
        file_size > 10 * 1024 * 1024) { // Sanity check: max 10MB for lyrics
      return false;
    }

    // Read entire file into buffer
    std::vector<char> buffer(static_cast<size_t>(file_size));
    size_t bytes_read =
        io_handler->read(buffer.data(), 1, static_cast<size_t>(file_size));

    if (bytes_read == 0) {
      throw IOException("Failed to read any data from lyrics file");
    }

    if (bytes_read != static_cast<size_t>(file_size)) {
      // Partial read - could be I/O error, but we'll try to continue
      std::cerr << "Warning: Only read " << bytes_read << " of " << file_size
                << " bytes from lyrics file '" << file_path << "'" << std::endl;
    }

    // Convert to string (handle partial reads)
    std::string content(buffer.data(), bytes_read);

    if (content.empty()) {
      return false;
    }

    // Determine file format and parse accordingly
    std::string extension = file_path.substr(file_path.find_last_of(".") + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   ::tolower);

    bool success = false;
    if (extension == "lrc") {
      success = parseLRC(content);
    } else if (extension == "txt") {
      success = parsePlainText(content);
    } else {
      // Try LRC format first, then fall back to plain text
      success = parseLRC(content);
      if (!success) {
        success = parsePlainText(content);
      }
    }

    if (success && !m_lines.empty()) {
      // Sort lines by timestamp for synced lyrics
      if (m_has_timing) {
        std::sort(m_lines.begin(), m_lines.end(),
                  [](const LyricLine &a, const LyricLine &b) {
                    return a.timestamp_ms < b.timestamp_ms;
                  });
      }
      return true;
    }

    return false;

  } catch (const InvalidMediaException &e) {
    // File doesn't exist, permission denied, etc. from FileIOHandler - this is
    // expected
    return false;
  } catch (const IOException &e) {
    // General I/O errors during lyrics processing
    std::cerr << "I/O error loading lyrics file '" << file_path
              << "': " << e.what() << std::endl;
    clear();
    return false;
  } catch (const std::exception &e) {
    // Catch any other exceptions, parsing errors, etc.
    std::cerr << "Error loading lyrics file '" << file_path << "': " << e.what()
              << std::endl;
    clear(); // Ensure we're in a clean state
    return false;
  } catch (...) {
    // Catch any other unexpected exceptions
    std::cerr << "Unknown error loading lyrics file '" << file_path << "'"
              << std::endl;
    clear();
    return false;
  }
}

// Helper to parse LRC timestamp: [mm:ss.xx] or [m:ss.xx]
// Returns true if valid, sets minutes, seconds, centiseconds, and end_pos
// (position after ']')
static bool parseLRCTimestamp(const std::string &line, size_t start,
                              int &minutes, int &seconds, int &centiseconds,
                              size_t &end_pos) {
  if (start >= line.size() || line[start] != '[')
    return false;

  size_t pos = start + 1;

  // Parse minutes (1-2 digits)
  size_t min_start = pos;
  while (pos < line.size() && std::isdigit(line[pos]))
    pos++;
  if (pos == min_start || pos - min_start > 2)
    return false;
  minutes = std::stoi(line.substr(min_start, pos - min_start));

  // Expect ':'
  if (pos >= line.size() || line[pos] != ':')
    return false;
  pos++;

  // Parse seconds (exactly 2 digits)
  if (pos + 2 > line.size() || !std::isdigit(line[pos]) ||
      !std::isdigit(line[pos + 1]))
    return false;
  seconds = (line[pos] - '0') * 10 + (line[pos + 1] - '0');
  pos += 2;

  // Expect '.'
  if (pos >= line.size() || line[pos] != '.')
    return false;
  pos++;

  // Parse centiseconds (exactly 2 digits)
  if (pos + 2 > line.size() || !std::isdigit(line[pos]) ||
      !std::isdigit(line[pos + 1]))
    return false;
  centiseconds = (line[pos] - '0') * 10 + (line[pos + 1] - '0');
  pos += 2;

  // Expect ']'
  if (pos >= line.size() || line[pos] != ']')
    return false;
  pos++;

  end_pos = pos;
  return true;
}

// Helper to parse LRC metadata: [xx:value] where xx is 2 lowercase letters
// Returns true if valid, sets tag, value
static bool parseLRCMetadata(const std::string &line, std::string &tag,
                             std::string &value) {
  if (line.size() < 5 || line[0] != '[')
    return false;

  // Check for 2-letter tag
  if (!std::isalpha(line[1]) || !std::isalpha(line[2]))
    return false;
  if (line[3] != ':')
    return false;

  // Find closing bracket
  size_t close = line.find(']', 4);
  if (close == std::string::npos)
    return false;

  // Make sure there's nothing after the closing bracket (metadata lines are
  // standalone)
  if (close != line.size() - 1)
    return false;

  tag = line.substr(1, 2);
  std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
  value = line.substr(4, close - 4);
  return true;
}

bool LyricsFile::parseLRC(const std::string &content) {
  // LRC format: [mm:ss.xx]Lyric text
  // Also supports metadata: [ti:Title], [ar:Artist], etc.

  std::istringstream stream(content);
  std::string line;
  bool found_lyrics = false;

  while (std::getline(stream, line)) {
    // Trim whitespace
    line.erase(0, line.find_first_not_of(" \t\r\n"));
    line.erase(line.find_last_not_of(" \t\r\n") + 1);

    if (line.empty())
      continue;

    int minutes, seconds, centiseconds;
    size_t end_pos;

    // Try to match timestamp format
    if (parseLRCTimestamp(line, 0, minutes, seconds, centiseconds, end_pos)) {
      std::string lyric_text = line.substr(end_pos);

      // Trim lyric text
      lyric_text.erase(0, lyric_text.find_first_not_of(" \t"));
      if (!lyric_text.empty()) {
        lyric_text.erase(lyric_text.find_last_not_of(" \t") + 1);
      }

      unsigned int timestamp_ms =
          (minutes * 60 + seconds) * 1000 + centiseconds * 10;

      // Add lyric line even if text is empty (for timing gaps)
      m_lines.emplace_back(timestamp_ms, lyric_text, true);
      found_lyrics = true;
      m_has_timing = true;
    }
    // Try to match metadata format
    else {
      std::string tag, value;
      if (parseLRCMetadata(line, tag, value)) {
        if (tag == "ti") {
          m_title = value;
        } else if (tag == "ar") {
          m_artist = value;
        }
        // Ignore other metadata for now
      }
    }
  }

  return found_lyrics;
}

bool LyricsFile::parsePlainText(const std::string &content) {
  std::istringstream stream(content);
  std::string line;
  bool found_lyrics = false;

  while (std::getline(stream, line)) {
    // Trim whitespace
    line.erase(0, line.find_first_not_of(" \t\r\n"));
    line.erase(line.find_last_not_of(" \t\r\n") + 1);

    if (!line.empty()) {
      // Plain text lyrics have no timing
      m_lines.emplace_back(0, line, false);
      found_lyrics = true;
    }
  }

  m_has_timing = false;
  return found_lyrics;
}

const LyricLine *
LyricsFile::getCurrentLine(unsigned int current_time_ms) const {
  if (m_lines.empty()) {
    return nullptr;
  }

  if (!m_has_timing) {
    // For unsynced lyrics, just return the first line
    return &m_lines[0];
  }

  int index = findCurrentLineIndex(current_time_ms);
  if (index >= 0 && index < static_cast<int>(m_lines.size())) {
    return &m_lines[index];
  }

  return nullptr;
}

std::vector<const LyricLine *>
LyricsFile::getUpcomingLines(unsigned int current_time_ms, size_t count) const {
  std::vector<const LyricLine *> upcoming;

  if (m_lines.empty()) {
    return upcoming;
  }

  if (!m_has_timing) {
    // For unsynced lyrics, return all lines
    for (size_t i = 0; i < std::min(count, m_lines.size()); ++i) {
      upcoming.push_back(&m_lines[i]);
    }
    return upcoming;
  }

  int current_index = findCurrentLineIndex(current_time_ms);
  if (current_index >= 0) {
    // Add current line and upcoming lines
    for (size_t i = 0; i < count && (current_index + i) < m_lines.size(); ++i) {
      upcoming.push_back(&m_lines[current_index + i]);
    }
  }

  return upcoming;
}

int LyricsFile::findCurrentLineIndex(unsigned int current_time_ms) const {
  if (m_lines.empty() || !m_has_timing) {
    return -1;
  }

  // Binary search for the most recent lyric line
  int left = 0;
  int right = static_cast<int>(m_lines.size()) - 1;
  int result = -1;

  while (left <= right) {
    int mid = left + (right - left) / 2;

    if (m_lines[mid].timestamp_ms <= current_time_ms) {
      result = mid;
      left = mid + 1;
    } else {
      right = mid - 1;
    }
  }

  return result;
}

void LyricsFile::clear() {
  m_lines.clear();
  m_has_timing = false;
  m_title.clear();
  m_artist.clear();
}

// Utility functions
namespace LyricsUtils {

std::string findLyricsFile(const std::string &audio_file_path) {
  try {
    // Extract base name without extension
    size_t last_dot = audio_file_path.find_last_of(".");
    size_t last_slash = audio_file_path.find_last_of("/\\");

    std::string base_path;
    if (last_dot != std::string::npos &&
        (last_slash == std::string::npos || last_dot > last_slash)) {
      base_path = audio_file_path.substr(0, last_dot);
    } else {
      base_path = audio_file_path;
    }

    // Try different lyrics file extensions
    std::vector<std::string> extensions = {".lrc", ".LRC", ".srt",
                                           ".SRT", ".txt", ".TXT"};

    for (const auto &ext : extensions) {
      try {
        std::string lyrics_path = base_path + ext;
        auto test_handler = std::make_unique<FileIOHandler>(
            TagLib::String(lyrics_path, TagLib::String::UTF8));
        if (test_handler) {
          // Check if file has content by seeking to end
          test_handler->seek(0, SEEK_END);
          long size = test_handler->tell();
          if (size > 0) {
            return lyrics_path;
          }
        }
      } catch (...) {
        // Ignore errors for individual file checks (ENOENT, EPERM, etc.)
        continue;
      }
    }

    // Try lyrics subdirectory
    if (last_slash != std::string::npos) {
      std::string dir = audio_file_path.substr(0, last_slash + 1);
      std::string filename = audio_file_path.substr(last_slash + 1);

      if (last_dot != std::string::npos && last_dot > last_slash) {
        filename = filename.substr(0, last_dot - last_slash - 1);
      }

      for (const auto &ext : extensions) {
        try {
          std::string lyrics_path = dir + "lyrics/" + filename + ext;
          auto test_handler = std::make_unique<FileIOHandler>(
              TagLib::String(lyrics_path, TagLib::String::UTF8));
          if (test_handler) {
            // Check if file has content by seeking to end
            test_handler->seek(0, SEEK_END);
            long size = test_handler->tell();
            if (size > 0) {
              return lyrics_path;
            }
          }
        } catch (...) {
          // Ignore errors for individual file checks (ENOENT, EPERM, etc.)
          continue;
        }
      }
    }

    return "";

  } catch (const std::exception &e) {
    std::cerr << "Error searching for lyrics file for '" << audio_file_path
              << "': " << e.what() << std::endl;
    return "";
  } catch (...) {
    std::cerr << "Unknown error searching for lyrics file for '"
              << audio_file_path << "'" << std::endl;
    return "";
  }
}

bool isLyricsFile(const std::string &file_path) {
  size_t last_dot = file_path.find_last_of(".");
  if (last_dot == std::string::npos) {
    return false;
  }

  std::string extension = file_path.substr(last_dot + 1);
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 ::tolower);

  return extension == "lrc" || extension == "srt" || extension == "txt";
}

} // namespace LyricsUtils

} // namespace Core
} // namespace PsyMP3