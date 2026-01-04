/*
 * TagFactory.h - Tag format detection and factory
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill II <segin2005@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef PSYMP3_TAG_TAGFACTORY_H
#define PSYMP3_TAG_TAGFACTORY_H

#include <string>
#include <memory>
#include <cstdint>
#include <cstddef>

namespace PsyMP3 {
namespace Tag {

// Forward declarations
class Tag;

/**
 * @brief Tag format detection result
 */
enum class TagFormat {
    Unknown,
    VorbisComment,
    ID3v1,
    ID3v2,
    ID3Combined,  // Both ID3v1 and ID3v2 present
    APE           // Future support
};

/**
 * @brief Factory class for tag format detection and creation
 */
class TagFactory {
public:
    /**
     * @brief Create tag reader from file path
     * 
     * Automatically detects the tag format and returns an appropriate
     * Tag implementation. For MP3 files, checks for both ID3v1 and ID3v2
     * and creates a MergedID3Tag if both are present.
     * 
     * @param filepath Path to the audio file
     * @return Unique pointer to Tag implementation (never null - returns NullTag if no tags found)
     */
    static std::unique_ptr<Tag> createFromFile(const std::string& filepath);
    
    /**
     * @brief Create tag reader from raw data with optional format hint
     * 
     * @param data Raw tag data
     * @param size Size of data
     * @param format_hint Optional format hint (e.g., "flac", "mp3", "ogg")
     * @return Unique pointer to Tag implementation (never null)
     */
    static std::unique_ptr<Tag> createFromData(
        const uint8_t* data, size_t size,
        const std::string& format_hint = "");
    
    /**
     * @brief Detect tag format from raw data
     * 
     * Examines magic bytes and structure to determine tag format.
     * 
     * @param data Raw data to examine
     * @param size Size of data
     * @return Detected format
     */
    static TagFormat detectFormat(const uint8_t* data, size_t size);
    
    /**
     * @brief Check if file has ID3v1 tag at end
     * 
     * @param filepath Path to file
     * @return true if ID3v1 tag is present
     */
    static bool hasID3v1(const std::string& filepath);
    
    /**
     * @brief Get ID3v2 tag size from file
     * 
     * @param filepath Path to file
     * @return Size of ID3v2 tag in bytes, or 0 if no ID3v2 tag present
     */
    static size_t getID3v2Size(const std::string& filepath);

private:
    /**
     * @brief Parse MP3 file with potential ID3v1 and ID3v2 tags
     * 
     * Checks for both ID3v1 (at end) and ID3v2 (at start) and creates
     * appropriate tag implementation (single or merged).
     * 
     * @param filepath Path to MP3 file
     * @return Unique pointer to Tag implementation
     */
    static std::unique_ptr<Tag> parseMP3Tags(const std::string& filepath);
};

} // namespace Tag
} // namespace PsyMP3

#endif // PSYMP3_TAG_TAGFACTORY_H
