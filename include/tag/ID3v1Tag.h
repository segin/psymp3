/*
 * ID3v1Tag.h - ID3v1/ID3v1.1 tag implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef PSYMP3_TAG_ID3V1TAG_H
#define PSYMP3_TAG_ID3V1TAG_H

#include "tag/Tag.h"
#include <array>

namespace PsyMP3 {
namespace Tag {

/**
 * @brief ID3v1/ID3v1.1 tag implementation
 * 
 * ID3v1 is a simple fixed-size metadata format appended to MP3 files.
 * The tag is exactly 128 bytes and contains:
 * - 3 bytes: "TAG" identifier
 * - 30 bytes: Title
 * - 30 bytes: Artist
 * - 30 bytes: Album
 * - 4 bytes: Year
 * - 30 bytes: Comment (28 bytes + null + track in ID3v1.1)
 * - 1 byte: Genre index
 * 
 * ID3v1.1 extends ID3v1 by using the last two bytes of the comment field
 * to store a track number (byte 28 = 0x00, byte 29 = track number).
 * 
 * ## Thread Safety
 * 
 * This class is thread-safe for concurrent read operations:
 * - All public accessor methods are const
 * - No mutable state is modified after construction
 * - Multiple threads can safely call any method simultaneously
 * - Data is fully parsed and stored during parse()
 * 
 * @see Tag class documentation for complete thread safety guarantees
 */
class ID3v1Tag : public Tag {
public:
    /// ID3v1 tag size in bytes
    static constexpr size_t TAG_SIZE = 128;
    
    /// Number of genres in the standard ID3v1 genre list (including Winamp extensions)
    static constexpr size_t GENRE_COUNT = 192;
    
    /**
     * @brief Parse ID3v1 tag from raw data
     * @param data Pointer to 128 bytes of ID3v1 data (starting with "TAG")
     * @return Unique pointer to ID3v1Tag, or nullptr if invalid
     */
    static std::unique_ptr<ID3v1Tag> parse(const uint8_t* data);
    
    /**
     * @brief Check if data contains a valid ID3v1 tag
     * @param data Pointer to at least 128 bytes of data
     * @return true if data starts with "TAG" magic bytes
     */
    static bool isValid(const uint8_t* data);
    
    /**
     * @brief Get genre string from genre index
     * @param index Genre index (0-191, 255 = unknown)
     * @return Genre string or empty string if index is invalid/unknown
     */
    static std::string genreFromIndex(uint8_t index);
    
    /**
     * @brief Get the complete genre list
     * @return Reference to the static genre list array
     */
    static const std::array<std::string, GENRE_COUNT>& genreList();
    
    // Default constructor (creates empty tag)
    ID3v1Tag() = default;
    ~ID3v1Tag() override = default;
    
    // Non-copyable but movable
    ID3v1Tag(const ID3v1Tag&) = delete;
    ID3v1Tag& operator=(const ID3v1Tag&) = delete;
    ID3v1Tag(ID3v1Tag&&) = default;
    ID3v1Tag& operator=(ID3v1Tag&&) = default;
    
    // ========================================================================
    // Tag interface implementation
    // ========================================================================
    
    std::string title() const override { return m_title; }
    std::string artist() const override { return m_artist; }
    std::string album() const override { return m_album; }
    std::string albumArtist() const override { return ""; } // Not in ID3v1
    std::string genre() const override;
    uint32_t year() const override { return m_year; }
    uint32_t track() const override { return m_track; }
    uint32_t trackTotal() const override { return 0; } // Not in ID3v1
    uint32_t disc() const override { return 0; } // Not in ID3v1
    uint32_t discTotal() const override { return 0; } // Not in ID3v1
    std::string comment() const override { return m_comment; }
    std::string composer() const override { return ""; } // Not in ID3v1
    
    std::string getTag(const std::string& key) const override;
    std::vector<std::string> getTagValues(const std::string& key) const override;
    std::map<std::string, std::string> getAllTags() const override;
    bool hasTag(const std::string& key) const override;
    
    // No pictures in ID3v1
    size_t pictureCount() const override { return 0; }
    std::optional<Picture> getPicture(size_t) const override { return std::nullopt; }
    std::optional<Picture> getFrontCover() const override { return std::nullopt; }
    
    bool isEmpty() const override;
    std::string formatName() const override;
    
    // ========================================================================
    // ID3v1-specific methods
    // ========================================================================
    
    /**
     * @brief Check if this is an ID3v1.1 tag (has track number)
     * @return true if ID3v1.1 format detected
     */
    bool isID3v1_1() const { return m_is_v1_1; }
    
    /**
     * @brief Get the raw genre index
     * @return Genre index (0-191), or 255 if unknown
     */
    uint8_t genreIndex() const { return m_genre_index; }
    
private:
    std::string m_title;
    std::string m_artist;
    std::string m_album;
    uint32_t m_year = 0;
    std::string m_comment;
    uint32_t m_track = 0;
    uint8_t m_genre_index = 255; // 255 = unknown
    bool m_is_v1_1 = false;
    
    /**
     * @brief Trim trailing null bytes and spaces from a string
     * @param data Pointer to character data
     * @param max_len Maximum length to read
     * @return Trimmed string
     */
    static std::string trimString(const char* data, size_t max_len);
    
    /**
     * @brief Normalize a key name for comparison
     * @param key Key name to normalize
     * @return Uppercase key name
     */
    static std::string normalizeKey(const std::string& key);
};

} // namespace Tag
} // namespace PsyMP3

#endif // PSYMP3_TAG_ID3V1TAG_H
