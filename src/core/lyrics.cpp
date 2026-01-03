/*
 * lyrics.cpp - Implementation for lyrics file support
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
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

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

namespace PsyMP3 {
namespace Core {

namespace {

// Helper to convert string_view to lowercase string
[[nodiscard]] std::string toLower(std::string_view str) {
  std::string result(str);
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return result;
}

// Helper to get file extension (lowercase, without dot)
[[nodiscard]] std::string_view getExtension(std::string_view path) noexcept {
  if (const auto pos = path.rfind('.'); pos != std::string_view::npos) {
    return path.substr(pos + 1);
  }
  return {};
}

// LRC timestamp parser result
struct TimestampResult {
  std::chrono::milliseconds time{0};
  size_t end_pos{0};
  bool valid{false};
};

// Parse LRC timestamp: [mm:ss.xx] or [m:ss.xx]
[[nodiscard]] TimestampResult parseLRCTimestamp(std::string_view line,
                                                size_t start) noexcept {
  TimestampResult result;

  if (start >= line.size() || line[start] != '[') {
    return result;
  }

  size_t pos = start + 1;

  // Parse minutes (1-2 digits)
  const size_t min_start = pos;
  while (pos < line.size() &&
         std::isdigit(static_cast<unsigned char>(line[pos]))) {
    ++pos;
  }

  if (pos == min_start || pos - min_start > 2) {
    return result;
  }

  int minutes = 0;
  for (size_t i = min_start; i < pos; ++i) {
    minutes = minutes * 10 + (line[i] - '0');
  }

  // Expect ':'
  if (pos >= line.size() || line[pos] != ':') {
    return result;
  }
  ++pos;

  // Parse seconds (exactly 2 digits)
  if (pos + 2 > line.size() ||
      !std::isdigit(static_cast<unsigned char>(line[pos])) ||
      !std::isdigit(static_cast<unsigned char>(line[pos + 1]))) {
    return result;
  }
  const int seconds = (line[pos] - '0') * 10 + (line[pos + 1] - '0');
  pos += 2;

  // Expect '.'
  if (pos >= line.size() || line[pos] != '.') {
    return result;
  }
  ++pos;

  // Parse centiseconds (exactly 2 digits)
  if (pos + 2 > line.size() ||
      !std::isdigit(static_cast<unsigned char>(line[pos])) ||
      !std::isdigit(static_cast<unsigned char>(line[pos + 1]))) {
    return result;
  }
  const int centiseconds = (line[pos] - '0') * 10 + (line[pos + 1] - '0');
  pos += 2;

  // Expect ']'
  if (pos >= line.size() || line[pos] != ']') {
    return result;
  }
  ++pos;

  result.time = std::chrono::milliseconds((minutes * 60 + seconds) * 1000 +
                                          centiseconds * 10);
  result.end_pos = pos;
  result.valid = true;
  return result;
}

// LRC metadata parser result
struct MetadataResult {
  std::string_view tag;
  std::string_view value;
  bool valid{false};
};

// Parse LRC metadata: [xx:value] where xx is 2 letters
[[nodiscard]] MetadataResult parseLRCMetadata(std::string_view line) noexcept {
  MetadataResult result;

  if (line.size() < 5 || line[0] != '[') {
    return result;
  }

  // Check for 2-letter tag
  if (!std::isalpha(static_cast<unsigned char>(line[1])) ||
      !std::isalpha(static_cast<unsigned char>(line[2]))) {
    return result;
  }

  if (line[3] != ':') {
    return result;
  }

  // Find closing bracket
  const auto close = line.find(']', 4);
  if (close == std::string_view::npos || close != line.size() - 1) {
    return result;
  }

  result.tag = line.substr(1, 2);
  result.value = line.substr(4, close - 4);
  result.valid = true;
  return result;
}

} // anonymous namespace

// --- LyricsUtils implementation ---

namespace LyricsUtils {

std::string_view trim(std::string_view str) noexcept {
  constexpr std::string_view whitespace = " \t\r\n";

  const auto start = str.find_first_not_of(whitespace);
  if (start == std::string_view::npos) {
    return {};
  }

  const auto end = str.find_last_not_of(whitespace);
  return str.substr(start, end - start + 1);
}

bool isLyricsFile(std::string_view file_path) noexcept {
  const auto ext = getExtension(file_path);
  if (ext.empty()) {
    return false;
  }

  const auto lower_ext = toLower(ext);
  return lower_ext == "lrc" || lower_ext == "srt" || lower_ext == "txt";
}

std::string findLyricsFile(std::string_view audio_file_path) {
  // Extract base path without extension
  auto dotPos = audio_file_path.rfind('.');
  auto slashPos = audio_file_path.rfind('/');

#ifdef _WIN32
  if (auto bslash = audio_file_path.rfind('\\');
      bslash != std::string_view::npos) {
    slashPos = (slashPos == std::string_view::npos)
                   ? bslash
                   : std::max(slashPos, bslash);
  }
#endif

  std::string base_path;
  if (dotPos != std::string_view::npos &&
      (slashPos == std::string_view::npos || dotPos > slashPos)) {
    base_path = std::string(audio_file_path.substr(0, dotPos));
  } else {
    base_path = std::string(audio_file_path);
  }

  // Try each extension (both lower and upper case)
  constexpr std::array<std::string_view, 6> extensions = {
      ".lrc", ".LRC", ".srt", ".SRT", ".txt", ".TXT"};

  for (const auto &ext : extensions) {
    std::string lyrics_path = base_path + std::string(ext);
    try {
      auto handler = std::make_unique<FileIOHandler>(
          TagLib::String(lyrics_path, TagLib::String::UTF8));
      handler->seek(0, SEEK_END);
      if (handler->tell() > 0) {
        return lyrics_path;
      }
    } catch (...) {
      // File doesn't exist or can't be opened - continue
    }
  }

  // Try lyrics subdirectory
  if (slashPos != std::string_view::npos) {
    const auto dir = audio_file_path.substr(0, slashPos + 1);
    auto filename = audio_file_path.substr(slashPos + 1);

    // Remove extension from filename
    if (dotPos != std::string_view::npos && dotPos > slashPos) {
      filename = filename.substr(0, dotPos - slashPos - 1);
    }

    for (const auto &ext : extensions) {
      std::string lyrics_path = std::string(dir) + "lyrics/" +
                                std::string(filename) + std::string(ext);
      try {
        auto handler = std::make_unique<FileIOHandler>(
            TagLib::String(lyrics_path, TagLib::String::UTF8));
        handler->seek(0, SEEK_END);
        if (handler->tell() > 0) {
          return lyrics_path;
        }
      } catch (...) {
        // Continue searching
      }
    }
  }

  return {};
}

} // namespace LyricsUtils

// --- LyricsFile implementation ---

void LyricsFile::clear() noexcept {
  m_lines.clear();
  m_has_timing = false;
  m_title.clear();
  m_artist.clear();
}

bool LyricsFile::loadFromFile(std::string_view file_path) {
  clear();

  try {
    // Create IO handler - throws on failure
    auto io_handler = std::make_unique<FileIOHandler>(
        TagLib::String(std::string(file_path), TagLib::String::UTF8));

    // Get file size
    io_handler->seek(0, SEEK_END);
    const auto file_size = io_handler->tell();
    io_handler->seek(0, SEEK_SET);

    if (file_size <= 0) {
      Debug::log("lyrics", "Empty lyrics file: {}", file_path);
      return false;
    }

    if (static_cast<size_t>(file_size) > MAX_FILE_SIZE) {
      Debug::log("lyrics", "Lyrics file too large (", file_size,
                 " bytes): ", file_path);
      return false;
    }

    // Read file content
    std::vector<char> buffer(static_cast<size_t>(file_size));
    const auto bytes_read = io_handler->read(buffer.data(), 1, buffer.size());

    if (bytes_read == 0) {
      Debug::log("lyrics", "Failed to read lyrics file: ", file_path);
      return false;
    }

    if (bytes_read != buffer.size()) {
      Debug::log("lyrics", "Partial read (", bytes_read, "/", file_size,
                 " bytes): ", file_path);
    }

    const std::string_view content(buffer.data(), bytes_read);

    // Determine format by extension
    const auto ext = toLower(getExtension(file_path));

    bool success = false;
    if (ext == "lrc") {
      success = parseLRC(content);
    } else if (ext == "txt") {
      success = parsePlainText(content);
    } else {
      // Unknown extension - try LRC first, then plain text
      success = parseLRC(content);
      if (!success) {
        success = parsePlainText(content);
      }
    }

    if (success && !m_lines.empty()) {
      // Sort synchronized lyrics by timestamp
      if (m_has_timing) {
        std::sort(m_lines.begin(), m_lines.end(),
                  [](const LyricLine &a, const LyricLine &b) {
                    return a.timestamp < b.timestamp;
                  });
      }

      Debug::log("lyrics", "Loaded {} {} lyrics from: {}", m_lines.size(),
                 m_has_timing ? "synced" : "unsynced", file_path);
      return true;
    }

    return false;

  } catch (const InvalidMediaException &) {
    // File not found or inaccessible - expected, don't log
    return false;
  } catch (const IOException &e) {
    Debug::log("lyrics", "I/O error loading lyrics '", file_path,
               "': ", e.what());
    clear();
    return false;
  } catch (const std::exception &e) {
    Debug::log("lyrics", "Error loading lyrics '{}': {}", file_path, e.what());
    clear();
    return false;
  }
}

bool LyricsFile::parseLRC(std::string_view content) {
  bool found_lyrics = false;
  size_t line_start = 0;

  while (line_start < content.size()) {
    // Find end of line
    auto line_end = content.find('\n', line_start);
    if (line_end == std::string_view::npos) {
      line_end = content.size();
    }

    // Extract and trim line
    auto line =
        LyricsUtils::trim(content.substr(line_start, line_end - line_start));
    line_start = line_end + 1;

    if (line.empty()) {
      continue;
    }

    // Try timestamp format
    if (auto ts = parseLRCTimestamp(line, 0); ts.valid) {
      auto lyric_text = LyricsUtils::trim(line.substr(ts.end_pos));
      m_lines.emplace_back(ts.time, std::string(lyric_text), true);
      found_lyrics = true;
      m_has_timing = true;
    }
    // Try metadata format
    else if (auto meta = parseLRCMetadata(line); meta.valid) {
      const auto tag = toLower(meta.tag);
      if (tag == "ti") {
        m_title = std::string(meta.value);
      } else if (tag == "ar") {
        m_artist = std::string(meta.value);
      }
      // Other metadata tags are ignored
    }
  }

  return found_lyrics;
}

bool LyricsFile::parsePlainText(std::string_view content) {
  bool found_lyrics = false;
  size_t line_start = 0;

  while (line_start < content.size()) {
    auto line_end = content.find('\n', line_start);
    if (line_end == std::string_view::npos) {
      line_end = content.size();
    }

    auto line =
        LyricsUtils::trim(content.substr(line_start, line_end - line_start));
    line_start = line_end + 1;

    if (!line.empty()) {
      m_lines.emplace_back(std::chrono::milliseconds(0), std::string(line),
                           false);
      found_lyrics = true;
    }
  }

  m_has_timing = false;
  return found_lyrics;
}

const LyricLine *LyricsFile::getCurrentLine(
    std::chrono::milliseconds current_time) const noexcept {
  if (m_lines.empty()) {
    return nullptr;
  }

  if (!m_has_timing) {
    return &m_lines[0];
  }

  const auto index = findCurrentLineIndex(current_time);
  if (index >= 0 && static_cast<size_t>(index) < m_lines.size()) {
    return &m_lines[static_cast<size_t>(index)];
  }

  return nullptr;
}

std::vector<const LyricLine *>
LyricsFile::getUpcomingLines(std::chrono::milliseconds current_time,
                             size_t count) const {

  std::vector<const LyricLine *> upcoming;
  upcoming.reserve(count);

  if (m_lines.empty()) {
    return upcoming;
  }

  if (!m_has_timing) {
    // For unsynced lyrics, return first N lines
    const auto n = std::min(count, m_lines.size());
    for (size_t i = 0; i < n; ++i) {
      upcoming.push_back(&m_lines[i]);
    }
    return upcoming;
  }

  const auto current_index = findCurrentLineIndex(current_time);
  if (current_index >= 0) {
    const auto start = static_cast<size_t>(current_index);
    const auto end = std::min(start + count, m_lines.size());
    for (size_t i = start; i < end; ++i) {
      upcoming.push_back(&m_lines[i]);
    }
  }

  return upcoming;
}

std::ptrdiff_t LyricsFile::findCurrentLineIndex(
    std::chrono::milliseconds current_time) const noexcept {
  if (m_lines.empty() || !m_has_timing) {
    return -1;
  }

  // Binary search for the most recent lyric line
  std::ptrdiff_t left = 0;
  std::ptrdiff_t right = static_cast<std::ptrdiff_t>(m_lines.size()) - 1;
  std::ptrdiff_t result = -1;

  while (left <= right) {
    const auto mid = left + (right - left) / 2;

    if (m_lines[static_cast<size_t>(mid)].timestamp <= current_time) {
      result = mid;
      left = mid + 1;
    } else {
      right = mid - 1;
    }
  }

  return result;
}

} // namespace Core
} // namespace PsyMP3