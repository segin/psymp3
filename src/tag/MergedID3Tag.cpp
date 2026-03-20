/*
 * MergedID3Tag.cpp - Merged ID3v1 and ID3v2 tag implementation
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

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

namespace PsyMP3 {
namespace Tag {

MergedID3Tag::MergedID3Tag(std::unique_ptr<ID3v1Tag> v1, std::unique_ptr<ID3v2Tag> v2)
    : m_v1(std::move(v1))
    , m_v2(std::move(v2))
{
}

// ============================================================================
// Tag interface implementation with fallback logic
// ============================================================================

std::string MergedID3Tag::title() const {
    return getStringWithFallback(&Tag::title);
}

std::string MergedID3Tag::artist() const {
    return getStringWithFallback(&Tag::artist);
}

std::string MergedID3Tag::album() const {
    return getStringWithFallback(&Tag::album);
}

std::string MergedID3Tag::albumArtist() const {
    // ID3v1 doesn't have album artist, so only check ID3v2
    if (m_v2) {
        return m_v2->albumArtist();
    }
    return "";
}

std::string MergedID3Tag::genre() const {
    return getStringWithFallback(&Tag::genre);
}

uint32_t MergedID3Tag::year() const {
    return getUint32WithFallback(&Tag::year);
}

uint32_t MergedID3Tag::track() const {
    return getUint32WithFallback(&Tag::track);
}

uint32_t MergedID3Tag::trackTotal() const {
    // ID3v1 doesn't have track total, so only check ID3v2
    if (m_v2) {
        return m_v2->trackTotal();
    }
    return 0;
}

uint32_t MergedID3Tag::disc() const {
    // ID3v1 doesn't have disc number, so only check ID3v2
    if (m_v2) {
        return m_v2->disc();
    }
    return 0;
}

uint32_t MergedID3Tag::discTotal() const {
    // ID3v1 doesn't have disc total, so only check ID3v2
    if (m_v2) {
        return m_v2->discTotal();
    }
    return 0;
}

std::string MergedID3Tag::comment() const {
    return getStringWithFallback(&Tag::comment);
}

std::string MergedID3Tag::composer() const {
    // ID3v1 doesn't have composer, so only check ID3v2
    if (m_v2) {
        return m_v2->composer();
    }
    return "";
}

std::string MergedID3Tag::getTag(const std::string& key) const {
    // Try ID3v2 first
    if (m_v2) {
        std::string value = m_v2->getTag(key);
        if (!value.empty()) {
            return value;
        }
    }
    
    // Fall back to ID3v1
    if (m_v1) {
        return m_v1->getTag(key);
    }
    
    return "";
}

std::vector<std::string> MergedID3Tag::getTagValues(const std::string& key) const {
    // Try ID3v2 first (it supports multi-valued fields)
    if (m_v2) {
        std::vector<std::string> values = m_v2->getTagValues(key);
        if (!values.empty()) {
            return values;
        }
    }
    
    // Fall back to ID3v1
    if (m_v1) {
        return m_v1->getTagValues(key);
    }
    
    return {};
}

std::map<std::string, std::string> MergedID3Tag::getAllTags() const {
    std::map<std::string, std::string> result;
    
    // Start with ID3v1 tags (lower priority)
    if (m_v1) {
        result = m_v1->getAllTags();
    }
    
    // Overlay ID3v2 tags (higher priority)
    if (m_v2) {
        auto v2_tags = m_v2->getAllTags();
        for (const auto& [key, value] : v2_tags) {
            if (!value.empty()) {
                result[key] = value;
            }
        }
    }
    
    return result;
}

bool MergedID3Tag::hasTag(const std::string& key) const {
    // Check both tags
    if (m_v2 && m_v2->hasTag(key)) {
        return true;
    }
    if (m_v1 && m_v1->hasTag(key)) {
        return true;
    }
    return false;
}

size_t MergedID3Tag::pictureCount() const {
    // Only ID3v2 supports pictures
    if (m_v2) {
        return m_v2->pictureCount();
    }
    return 0;
}

std::optional<Picture> MergedID3Tag::getPicture(size_t index) const {
    // Only ID3v2 supports pictures
    if (m_v2) {
        return m_v2->getPicture(index);
    }
    return std::nullopt;
}

std::optional<Picture> MergedID3Tag::getFrontCover() const {
    // Only ID3v2 supports pictures
    if (m_v2) {
        return m_v2->getFrontCover();
    }
    return std::nullopt;
}

bool MergedID3Tag::isEmpty() const {
    // Empty if both tags are null or both are empty
    bool v1_empty = !m_v1 || m_v1->isEmpty();
    bool v2_empty = !m_v2 || m_v2->isEmpty();
    return v1_empty && v2_empty;
}

std::string MergedID3Tag::formatName() const {
    if (m_v1 && m_v2) {
        return "Merged ID3 (" + m_v2->formatName() + " + " + m_v1->formatName() + ")";
    } else if (m_v2) {
        return m_v2->formatName();
    } else if (m_v1) {
        return m_v1->formatName();
    } else {
        return "Empty Merged ID3";
    }
}

// ============================================================================
// Helper methods for fallback logic
// ============================================================================

std::string MergedID3Tag::getStringWithFallback(
    std::string (Tag::*getter)() const) const
{
    // Try ID3v2 first
    if (m_v2) {
        std::string value = (m_v2.get()->*getter)();
        if (!value.empty()) {
            return value;
        }
    }
    
    // Fall back to ID3v1
    if (m_v1) {
        return (m_v1.get()->*getter)();
    }
    
    return "";
}

uint32_t MergedID3Tag::getUint32WithFallback(
    uint32_t (Tag::*getter)() const) const
{
    // Try ID3v2 first
    if (m_v2) {
        uint32_t value = (m_v2.get()->*getter)();
        if (value != 0) {
            return value;
        }
    }
    
    // Fall back to ID3v1
    if (m_v1) {
        return (m_v1.get()->*getter)();
    }
    
    return 0;
}

} // namespace Tag
} // namespace PsyMP3
