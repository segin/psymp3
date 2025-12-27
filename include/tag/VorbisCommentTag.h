/*
 * VorbisCommentTag.h - VorbisComment metadata tag implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef PSYMP3_TAG_VORBISCOMMENTTAG_H
#define PSYMP3_TAG_VORBISCOMMENTTAG_H

#include "tag/Tag.h"
#include <map>
#include <vector>
#include <string>
#include <memory>
#include <cstdint>

namespace PsyMP3 {
namespace Tag {

/**
 * @brief VorbisComment metadata tag implementation
 * 
 * Implements the Tag interface for VorbisComment metadata found in
 * Ogg and FLAC containers. VorbisComment uses UTF-8 encoded key=value
 * pairs with case-insensitive field names and support for multi-valued fields.
 * 
 * ## Thread Safety
 * 
 * This class is thread-safe for concurrent read operations:
 * - All public accessor methods are const
 * - No mutable state is modified after construction
 * - Multiple threads can safely call any method simultaneously
 * - Data is fully parsed and stored during parse() or construction
 * 
 * @see Tag class documentation for complete thread safety guarantees
 */
class VorbisCommentTag : public Tag {
public:
    /**
     * @brief Parse VorbisComment data from raw bytes
     * 
     * Parses a VorbisComment block according to the Vorbis specification.
     * The data should start with the vendor string length (little-endian).
     * 
     * @param data Pointer to VorbisComment data
     * @param size Size of data in bytes
     * @return Unique pointer to VorbisCommentTag, or nullptr on parse failure
     */
    static std::unique_ptr<VorbisCommentTag> parse(const uint8_t* data, size_t size);
    
    /**
     * @brief Construct from pre-parsed fields
     * 
     * @param vendor Vendor string
     * @param fields Map of field names to values (case-normalized keys)
     */
    VorbisCommentTag(const std::string& vendor,
                     const std::map<std::string, std::vector<std::string>>& fields);
    
    /**
     * @brief Construct with vendor, fields, and pictures
     * 
     * @param vendor Vendor string
     * @param fields Map of field names to values
     * @param pictures Vector of embedded pictures
     */
    VorbisCommentTag(const std::string& vendor,
                     const std::map<std::string, std::vector<std::string>>& fields,
                     const std::vector<Picture>& pictures);
    
    ~VorbisCommentTag() override;
    
    // Non-copyable but movable
    VorbisCommentTag(const VorbisCommentTag&) = delete;
    VorbisCommentTag& operator=(const VorbisCommentTag&) = delete;
    VorbisCommentTag(VorbisCommentTag&&) = default;
    VorbisCommentTag& operator=(VorbisCommentTag&&) = default;
    
    // ========================================================================
    // Tag interface implementation
    // ========================================================================
    
    std::string title() const override;
    std::string artist() const override;
    std::string album() const override;
    std::string albumArtist() const override;
    std::string genre() const override;
    uint32_t year() const override;
    uint32_t track() const override;
    uint32_t trackTotal() const override;
    uint32_t disc() const override;
    uint32_t discTotal() const override;
    std::string comment() const override;
    std::string composer() const override;
    
    std::string getTag(const std::string& key) const override;
    std::vector<std::string> getTagValues(const std::string& key) const override;
    std::map<std::string, std::string> getAllTags() const override;
    bool hasTag(const std::string& key) const override;
    
    size_t pictureCount() const override;
    std::optional<Picture> getPicture(size_t index) const override;
    std::optional<Picture> getFrontCover() const override;
    
    bool isEmpty() const override;
    std::string formatName() const override;
    
    // ========================================================================
    // VorbisComment-specific accessors
    // ========================================================================
    
    /**
     * @brief Get the vendor string
     * @return Vendor string (e.g., "libvorbis 1.3.7")
     */
    std::string vendorString() const;
    
private:
    std::string m_vendor_string;
    std::map<std::string, std::vector<std::string>> m_fields; // Case-normalized keys
    std::vector<Picture> m_pictures;
    
    /**
     * @brief Normalize field name to uppercase for case-insensitive storage
     * @param name Field name
     * @return Uppercase field name
     */
    static std::string normalizeFieldName(const std::string& name);
    
    /**
     * @brief Parse METADATA_BLOCK_PICTURE field
     * 
     * Parses a base64-encoded FLAC picture block from a VorbisComment field.
     * 
     * @param base64_data Base64-encoded picture data
     * @return Picture structure or nullopt on parse failure
     */
    static std::optional<Picture> parsePictureField(const std::string& base64_data);
    
    /**
     * @brief Get first value for a field (case-insensitive)
     * @param key Field name
     * @return First value or empty string if not present
     */
    std::string getFirstValue(const std::string& key) const;
    
    /**
     * @brief Parse a number from a string field
     * @param key Field name
     * @return Parsed number or 0 if not present/invalid
     */
    uint32_t getNumberValue(const std::string& key) const;
};

} // namespace Tag
} // namespace PsyMP3

#endif // PSYMP3_TAG_VORBISCOMMENTTAG_H
