/*
 * NullTag.h - Null object pattern implementation for Tag interface
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

#ifndef PSYMP3_TAG_NULLTAG_H
#define PSYMP3_TAG_NULLTAG_H

#include "tag/Tag.h"

namespace PsyMP3 {
namespace Tag {

/**
 * @brief Null object implementation of Tag interface
 * 
 * This class implements the Null Object pattern for the Tag interface.
 * It returns empty/default values for all tag queries, allowing code
 * to safely work with files that have no metadata without null checks.
 * 
 * ## Thread Safety
 * 
 * This class is thread-safe for concurrent read operations:
 * - All public accessor methods are const
 * - No mutable state exists (all methods return constant values)
 * - Multiple threads can safely call any method simultaneously
 * - Stateless implementation guarantees no data races
 * 
 * @see Tag class documentation for complete thread safety guarantees
 */
class NullTag : public Tag {
public:
    NullTag() = default;
    ~NullTag() override = default;
    
    // Non-copyable but movable
    NullTag(const NullTag&) = delete;
    NullTag& operator=(const NullTag&) = delete;
    NullTag(NullTag&&) = default;
    NullTag& operator=(NullTag&&) = default;
    
    // ========================================================================
    // Core metadata fields - all return empty/zero values
    // ========================================================================
    
    std::string title() const override { return ""; }
    std::string artist() const override { return ""; }
    std::string album() const override { return ""; }
    std::string albumArtist() const override { return ""; }
    std::string genre() const override { return ""; }
    uint32_t year() const override { return 0; }
    uint32_t track() const override { return 0; }
    uint32_t trackTotal() const override { return 0; }
    uint32_t disc() const override { return 0; }
    uint32_t discTotal() const override { return 0; }
    std::string comment() const override { return ""; }
    std::string composer() const override { return ""; }
    
    // ========================================================================
    // Extended metadata access - all return empty values
    // ========================================================================
    
    std::string getTag(const std::string& key) const override { 
        (void)key; // Suppress unused parameter warning
        return ""; 
    }
    
    std::vector<std::string> getTagValues(const std::string& key) const override { 
        (void)key;
        return {}; 
    }
    
    std::map<std::string, std::string> getAllTags() const override { 
        return {}; 
    }
    
    bool hasTag(const std::string& key) const override { 
        (void)key;
        return false; 
    }
    
    // ========================================================================
    // Picture/artwork access - no pictures available
    // ========================================================================
    
    size_t pictureCount() const override { return 0; }
    
    std::optional<Picture> getPicture(size_t index) const override { 
        (void)index;
        return std::nullopt; 
    }
    
    std::optional<Picture> getFrontCover() const override { 
        return std::nullopt; 
    }
    
    // ========================================================================
    // Metadata state
    // ========================================================================
    
    bool isEmpty() const override { return true; }
    
    std::string formatName() const override { return "None"; }
};

} // namespace Tag
} // namespace PsyMP3

#endif // PSYMP3_TAG_NULLTAG_H

