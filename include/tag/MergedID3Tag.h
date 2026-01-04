/*
 * MergedID3Tag.h - Merged ID3v1 and ID3v2 tag implementation
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

#ifndef PSYMP3_TAG_MERGEDID3TAG_H
#define PSYMP3_TAG_MERGEDID3TAG_H

#include "tag/Tag.h"
#include "tag/ID3v1Tag.h"
#include "tag/ID3v2Tag.h"
#include <memory>

namespace PsyMP3 {
namespace Tag {

/**
 * @brief Merged ID3v1 and ID3v2 tag implementation
 * 
 * This class combines ID3v1 and ID3v2 tags with ID3v2 taking precedence
 * and ID3v1 as fallback. This is useful for MP3 files that have both
 * tag formats, ensuring complete metadata access.
 * 
 * Precedence rules:
 * - ID3v2 values are used when present and non-empty
 * - ID3v1 values are used as fallback when ID3v2 field is empty
 * - If only one tag type is present, it is used exclusively
 * 
 * ## Thread Safety
 * 
 * This class is thread-safe for concurrent read operations:
 * - All public accessor methods are const
 * - No mutable state is modified after construction
 * - Multiple threads can safely call any method simultaneously
 * - Underlying ID3v1 and ID3v2 tags are also thread-safe
 * 
 * @see Tag class documentation for complete thread safety guarantees
 */
class MergedID3Tag : public Tag {
public:
    /**
     * @brief Construct a merged tag from ID3v1 and ID3v2 tags
     * 
     * At least one of the tags should be non-null. If both are null,
     * the merged tag will behave as an empty tag.
     * 
     * @param v1 ID3v1 tag (can be nullptr)
     * @param v2 ID3v2 tag (can be nullptr)
     */
    MergedID3Tag(std::unique_ptr<ID3v1Tag> v1, std::unique_ptr<ID3v2Tag> v2);
    
    ~MergedID3Tag() override = default;
    
    // Non-copyable but movable
    MergedID3Tag(const MergedID3Tag&) = delete;
    MergedID3Tag& operator=(const MergedID3Tag&) = delete;
    MergedID3Tag(MergedID3Tag&&) = default;
    MergedID3Tag& operator=(MergedID3Tag&&) = default;
    
    // ========================================================================
    // Tag interface implementation (delegates to v2 first, falls back to v1)
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
    // MergedID3Tag-specific methods
    // ========================================================================
    
    /**
     * @brief Get the underlying ID3v1 tag
     * @return Pointer to ID3v1 tag, or nullptr if not present
     */
    const ID3v1Tag* id3v1() const { return m_v1.get(); }
    
    /**
     * @brief Get the underlying ID3v2 tag
     * @return Pointer to ID3v2 tag, or nullptr if not present
     */
    const ID3v2Tag* id3v2() const { return m_v2.get(); }
    
    /**
     * @brief Check if ID3v1 tag is present
     * @return true if ID3v1 tag exists
     */
    bool hasID3v1() const { return m_v1 != nullptr; }
    
    /**
     * @brief Check if ID3v2 tag is present
     * @return true if ID3v2 tag exists
     */
    bool hasID3v2() const { return m_v2 != nullptr; }
    
private:
    std::unique_ptr<ID3v1Tag> m_v1;  ///< ID3v1 tag (fallback)
    std::unique_ptr<ID3v2Tag> m_v2;  ///< ID3v2 tag (primary)
    
    /**
     * @brief Get string value with fallback logic
     * 
     * Tries ID3v2 first, falls back to ID3v1 if ID3v2 value is empty.
     * 
     * @param getter Member function pointer to string getter
     * @return String value from ID3v2 or ID3v1, or empty string
     */
    std::string getStringWithFallback(
        std::string (Tag::*getter)() const) const;
    
    /**
     * @brief Get uint32_t value with fallback logic
     * 
     * Tries ID3v2 first, falls back to ID3v1 if ID3v2 value is 0.
     * 
     * @param getter Member function pointer to uint32_t getter
     * @return Value from ID3v2 or ID3v1, or 0
     */
    uint32_t getUint32WithFallback(
        uint32_t (Tag::*getter)() const) const;
};

} // namespace Tag
} // namespace PsyMP3

#endif // PSYMP3_TAG_MERGEDID3TAG_H
