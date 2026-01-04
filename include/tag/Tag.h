/*
 * Tag.h - Format-neutral metadata tag interface
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

#ifndef PSYMP3_TAG_TAG_H
#define PSYMP3_TAG_TAG_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <cstdint>

namespace PsyMP3 {
namespace Tag {

/**
 * @brief Picture type enumeration (compatible with ID3v2 APIC and Vorbis METADATA_BLOCK_PICTURE)
 */
enum class PictureType : uint8_t {
    Other = 0,
    FileIcon = 1,
    OtherFileIcon = 2,
    FrontCover = 3,
    BackCover = 4,
    LeafletPage = 5,
    Media = 6,
    LeadArtist = 7,
    Artist = 8,
    Conductor = 9,
    Band = 10,
    Composer = 11,
    Lyricist = 12,
    RecordingLocation = 13,
    DuringRecording = 14,
    DuringPerformance = 15,
    MovieScreenCapture = 16,
    BrightColoredFish = 17,
    Illustration = 18,
    BandLogotype = 19,
    PublisherLogotype = 20
};

/**
 * @brief Embedded picture/artwork data
 */
struct Picture {
    PictureType type = PictureType::Other;
    std::string mime_type;
    std::string description;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t color_depth = 0;
    uint32_t colors_used = 0;
    std::vector<uint8_t> data;
    
    bool isEmpty() const { return data.empty(); }
};

/**
 * @brief Abstract base class for format-neutral metadata access
 * 
 * This interface provides a unified way to access metadata tags regardless
 * of the underlying format (ID3v1, ID3v2, Vorbis Comments, APE, etc.)
 * 
 * ## Thread Safety Guarantees
 * 
 * All Tag implementations provide the following thread safety guarantees:
 * 
 * 1. **Concurrent Read Safety**: Multiple threads can safely call any const
 *    method simultaneously without external synchronization. This includes
 *    all accessor methods (title(), artist(), getTag(), getPicture(), etc.)
 * 
 * 2. **Immutable After Construction**: Tag objects are immutable after
 *    construction. All data is parsed and stored during object creation,
 *    and no internal state is modified by any accessor method.
 * 
 * 3. **No Mutable State**: Tag implementations do not use mutable members
 *    or lazy initialization that could cause data races during reads.
 * 
 * 4. **Safe Sharing**: Tag objects can be safely shared between threads
 *    via const references or shared_ptr<const Tag> without synchronization.
 * 
 * ## Usage Example
 * 
 * @code
 * // Safe: Multiple threads reading the same tag
 * std::shared_ptr<const Tag> tag = TagFactory::createFromFile("song.mp3");
 * 
 * std::thread t1([&tag]() { std::cout << tag->title() << std::endl; });
 * std::thread t2([&tag]() { std::cout << tag->artist() << std::endl; });
 * std::thread t3([&tag]() { std::cout << tag->album() << std::endl; });
 * 
 * t1.join(); t2.join(); t3.join();
 * @endcode
 * 
 * @note Write operations are not supported. Tags are read-only after creation.
 * @see Requirements 9.1, 9.2, 9.3, 9.4 in tag-framework design document
 */
class Tag {
public:
    virtual ~Tag() = default;
    
    // ========================================================================
    // Core metadata fields (common across all formats)
    // ========================================================================
    
    /**
     * @brief Get the track title
     * @return Track title or empty string if not present
     */
    virtual std::string title() const = 0;
    
    /**
     * @brief Get the artist name
     * @return Artist name or empty string if not present
     */
    virtual std::string artist() const = 0;
    
    /**
     * @brief Get the album name
     * @return Album name or empty string if not present
     */
    virtual std::string album() const = 0;
    
    /**
     * @brief Get the album artist (may differ from track artist)
     * @return Album artist or empty string if not present
     */
    virtual std::string albumArtist() const = 0;
    
    /**
     * @brief Get the genre
     * @return Genre or empty string if not present
     */
    virtual std::string genre() const = 0;
    
    /**
     * @brief Get the year/date of release
     * @return Year as integer, or 0 if not present
     */
    virtual uint32_t year() const = 0;
    
    /**
     * @brief Get the track number
     * @return Track number, or 0 if not present
     */
    virtual uint32_t track() const = 0;
    
    /**
     * @brief Get the total number of tracks on the album
     * @return Total tracks, or 0 if not present
     */
    virtual uint32_t trackTotal() const = 0;
    
    /**
     * @brief Get the disc number
     * @return Disc number, or 0 if not present
     */
    virtual uint32_t disc() const = 0;
    
    /**
     * @brief Get the total number of discs
     * @return Total discs, or 0 if not present
     */
    virtual uint32_t discTotal() const = 0;
    
    /**
     * @brief Get the comment/description
     * @return Comment or empty string if not present
     */
    virtual std::string comment() const = 0;
    
    /**
     * @brief Get the composer
     * @return Composer or empty string if not present
     */
    virtual std::string composer() const = 0;
    
    // ========================================================================
    // Extended metadata access
    // ========================================================================
    
    /**
     * @brief Get a custom/extended tag by name
     * @param key Tag name (case-insensitive for most formats)
     * @return Tag value or empty string if not present
     */
    virtual std::string getTag(const std::string& key) const = 0;
    
    /**
     * @brief Get all values for a tag (some formats allow multiple values)
     * @param key Tag name
     * @return Vector of values (empty if tag not present)
     */
    virtual std::vector<std::string> getTagValues(const std::string& key) const = 0;
    
    /**
     * @brief Get all tags as a key-value map
     * @return Map of tag names to values
     */
    virtual std::map<std::string, std::string> getAllTags() const = 0;
    
    /**
     * @brief Check if a specific tag exists
     * @param key Tag name
     * @return true if tag exists
     */
    virtual bool hasTag(const std::string& key) const = 0;
    
    // ========================================================================
    // Picture/artwork access
    // ========================================================================
    
    /**
     * @brief Get the number of embedded pictures
     * @return Number of pictures
     */
    virtual size_t pictureCount() const = 0;
    
    /**
     * @brief Get a picture by index
     * @param index Picture index (0-based)
     * @return Picture data or nullopt if index out of range
     */
    virtual std::optional<Picture> getPicture(size_t index) const = 0;
    
    /**
     * @brief Get the front cover picture if available
     * @return Front cover or nullopt if not present
     */
    virtual std::optional<Picture> getFrontCover() const = 0;
    
    // ========================================================================
    // Metadata state
    // ========================================================================
    
    /**
     * @brief Check if any tags are present
     * @return true if at least one tag has a value
     */
    virtual bool isEmpty() const = 0;
    
    /**
     * @brief Get the underlying tag format name
     * @return Format name (e.g., "ID3v2.4", "Vorbis Comments", "APE", "None")
     */
    virtual std::string formatName() const = 0;
};

/**
 * @brief Factory function to create a Tag reader for a file
 * 
 * This function automatically detects the tag format and returns
 * an appropriate Tag implementation.
 * 
 * @param filepath Path to the audio file
 * @return Unique pointer to Tag implementation (never null - returns NullTag if no tags found)
 */
std::unique_ptr<Tag> createTagReader(const std::string& filepath);

/**
 * @brief Factory function to create a Tag reader from raw data
 * 
 * @param data Raw file data
 * @param size Size of data
 * @param format_hint Optional format hint (e.g., "flac", "mp3", "ogg")
 * @return Unique pointer to Tag implementation
 */
std::unique_ptr<Tag> createTagReaderFromData(const uint8_t* data, size_t size, 
                                              const std::string& format_hint = "");

} // namespace Tag
} // namespace PsyMP3

#endif // PSYMP3_TAG_TAG_H

