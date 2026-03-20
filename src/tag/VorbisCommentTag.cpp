/*
 * VorbisCommentTag.cpp - VorbisComment metadata tag implementation
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

// ============================================================================
// Helper functions
// ============================================================================

/**
 * @brief Read a 32-bit little-endian unsigned integer
 */
static uint32_t readLE32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}



// ============================================================================
// VorbisCommentTag implementation
// ============================================================================

std::string VorbisCommentTag::normalizeFieldName(const std::string& name) {
    std::string normalized;
    normalized.reserve(name.size());
    
    for (char c : name) {
        normalized += std::toupper(static_cast<unsigned char>(c));
    }
    
    return normalized;
}

VorbisCommentTag::VorbisCommentTag(const std::string& vendor,
                                   const std::map<std::string, std::vector<std::string>>& fields)
    : m_vendor_string(vendor), m_fields(fields) {
}

VorbisCommentTag::VorbisCommentTag(const std::string& vendor,
                                   const std::map<std::string, std::vector<std::string>>& fields,
                                   const std::vector<Picture>& pictures)
    : m_vendor_string(vendor), m_fields(fields), m_pictures(pictures) {
}

VorbisCommentTag::~VorbisCommentTag() = default;

std::unique_ptr<VorbisCommentTag> VorbisCommentTag::parse(const uint8_t* data, size_t size) {
    if (!data || size < 8) {
        Debug::log("tag", "VorbisComment: Insufficient data for header (need 8, got ", size, ")");
        return nullptr;
    }
    
    size_t offset = 0;
    
    // Parse vendor string length (little-endian)
    uint32_t vendor_len = readLE32(data + offset);
    offset += 4;
    
    // Sanity check vendor length (prevent huge allocations)
    if (vendor_len > TagConstants::MAX_VENDOR_STRING_SIZE) { 
        Debug::log("tag", "VorbisComment: Vendor string length ", vendor_len, " exceeds maximum");
        return nullptr;
    }
    
    if (vendor_len > size - offset) {
        Debug::log("tag", "VorbisComment: Vendor string length ", vendor_len, 
                   " exceeds remaining data ", (size - offset));
        return nullptr;
    }
    
    // Parse vendor string (UTF-8)
    std::string vendor;
    if (vendor_len > 0) {
        vendor = ID3v2Utils::decodeUTF8Safe(data + offset, vendor_len);
    }
    offset += vendor_len;
    
    // Parse field count
    if (offset + 4 > size) {
        Debug::log("tag", "VorbisComment: Insufficient data for field count");
        return nullptr;
    }
    
    uint32_t field_count = readLE32(data + offset);
    offset += 4;
    
    // Sanity check field count (prevent huge allocations)
    if (field_count > TagConstants::MAX_FIELD_COUNT) {
        Debug::log("tag", "VorbisComment: Field count ", field_count, " exceeds maximum");
        return nullptr;
    }
    
    Debug::log("tag", "VorbisComment: vendor='", vendor, "', field_count=", field_count);
    
    // Parse all fields
    std::map<std::string, std::vector<std::string>> fields;
    std::vector<Picture> pictures;
    
    for (uint32_t i = 0; i < field_count; i++) {
        if (offset + 4 > size) {
            Debug::log("tag", "VorbisComment: Insufficient data for field ", i, " length");
            // Return partial data instead of failing completely
            break;
        }
        
        uint32_t field_len = readLE32(data + offset);
        offset += 4;
        
        // Sanity check field length (prevent huge allocations)
        if (field_len > TagConstants::MAX_STRING_FIELD_SIZE) {
            Debug::log("tag", "VorbisComment: Field ", i, " length ", field_len, " exceeds maximum");
            // Skip this field but continue parsing
            offset += std::min(field_len, static_cast<uint32_t>(size - offset));
            continue;
        }
        
        if (field_len > size - offset) {
            Debug::log("tag", "VorbisComment: Field ", i, " length ", field_len,
                       " exceeds remaining data ", (size - offset));
            // Return partial data instead of failing completely
            break;
        }
        
        // Parse field as UTF-8 string with safe decoding
        std::string field_str = ID3v2Utils::decodeUTF8Safe(data + offset, field_len);
        offset += field_len;
        
        // Split on '=' to get name and value
        size_t eq_pos = field_str.find('=');
        if (eq_pos == std::string::npos) {
            Debug::log("tag", "VorbisComment: Field ", i, " missing '=' separator");
            continue;
        }
        
        std::string name = field_str.substr(0, eq_pos);
        std::string value = field_str.substr(eq_pos + 1);
        
        // Validate field name (printable ASCII, no '=')
        bool valid_name = true;
        for (char c : name) {
            if (c < 0x20 || c > 0x7E || c == '=') {
                valid_name = false;
                break;
            }
        }
        
        if (!valid_name) {
            Debug::log("tag", "VorbisComment: Invalid field name, skipping");
            continue;
        }
        
        // Normalize field name to uppercase for case-insensitive storage
        std::string normalized_name = normalizeFieldName(name);
        
        // Check for METADATA_BLOCK_PICTURE
        if (normalized_name == "METADATA_BLOCK_PICTURE") {
            auto picture = parsePictureField(value);
            if (picture) {
                pictures.push_back(*picture);
            }
        } else {
            // Add to multi-valued field map
            fields[normalized_name].push_back(value);
        }
        
        Debug::log("tag", "VorbisComment: ", name, "=", value);
    }
    
    return std::make_unique<VorbisCommentTag>(vendor, fields, pictures);
}

std::optional<Picture> VorbisCommentTag::parsePictureField(const std::string& base64_data) {
    // Decode base64
    std::vector<uint8_t> data = Core::Utility::Base64::decode(base64_data);
    
    if (data.size() < 32) {
        Debug::log("tag", "VorbisComment: METADATA_BLOCK_PICTURE too small (need 32, got ", data.size(), ")");
        return std::nullopt;
    }
    
    size_t offset = 0;
    Picture pic;
    
    // Picture type (32-bit big-endian)
    uint32_t type = (static_cast<uint32_t>(data[offset]) << 24) |
                    (static_cast<uint32_t>(data[offset + 1]) << 16) |
                    (static_cast<uint32_t>(data[offset + 2]) << 8) |
                    static_cast<uint32_t>(data[offset + 3]);
    pic.type = static_cast<PictureType>(type);
    offset += 4;
    
    // MIME type length (32-bit big-endian)
    uint32_t mime_len = (static_cast<uint32_t>(data[offset]) << 24) |
                        (static_cast<uint32_t>(data[offset + 1]) << 16) |
                        (static_cast<uint32_t>(data[offset + 2]) << 8) |
                        static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    
    // Sanity check MIME type length
    if (mime_len > TagConstants::MAX_MIME_TYPE_LENGTH) {
        Debug::log("tag", "VorbisComment: MIME type length ", mime_len, " exceeds maximum");
        return std::nullopt;
    }
    
    if (mime_len > data.size() - offset) {
        Debug::log("tag", "VorbisComment: MIME type length exceeds data");
        return std::nullopt;
    }
    
    // MIME type (UTF-8)
    pic.mime_type = ID3v2Utils::decodeUTF8Safe(data.data() + offset, mime_len);
    offset += mime_len;
    
    if (offset + 4 > data.size()) {
        Debug::log("tag", "VorbisComment: Insufficient data for description length");
        return std::nullopt;
    }
    
    // Description length (32-bit big-endian)
    uint32_t desc_len = (static_cast<uint32_t>(data[offset]) << 24) |
                        (static_cast<uint32_t>(data[offset + 1]) << 16) |
                        (static_cast<uint32_t>(data[offset + 2]) << 8) |
                        static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    
    // Sanity check description length
    if (desc_len > TagConstants::MAX_STRING_FIELD_SIZE) { 
        Debug::log("tag", "VorbisComment: Description length ", desc_len, " exceeds maximum");
        return std::nullopt;
    }
    
    if (desc_len > data.size() - offset) {
        Debug::log("tag", "VorbisComment: Description length exceeds data");
        return std::nullopt;
    }
    
    // Description (UTF-8)
    pic.description = ID3v2Utils::decodeUTF8Safe(data.data() + offset, desc_len);
    offset += desc_len;
    
    if (offset + 20 > data.size()) {
        Debug::log("tag", "VorbisComment: Insufficient data for picture metadata");
        return std::nullopt;
    }
    
    // Width (32-bit big-endian)
    pic.width = (static_cast<uint32_t>(data[offset]) << 24) |
                (static_cast<uint32_t>(data[offset + 1]) << 16) |
                (static_cast<uint32_t>(data[offset + 2]) << 8) |
                static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    
    // Height (32-bit big-endian)
    pic.height = (static_cast<uint32_t>(data[offset]) << 24) |
                 (static_cast<uint32_t>(data[offset + 1]) << 16) |
                 (static_cast<uint32_t>(data[offset + 2]) << 8) |
                 static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    
    // Color depth (32-bit big-endian)
    pic.color_depth = (static_cast<uint32_t>(data[offset]) << 24) |
                      (static_cast<uint32_t>(data[offset + 1]) << 16) |
                      (static_cast<uint32_t>(data[offset + 2]) << 8) |
                      static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    
    // Colors used (32-bit big-endian)
    pic.colors_used = (static_cast<uint32_t>(data[offset]) << 24) |
                      (static_cast<uint32_t>(data[offset + 1]) << 16) |
                      (static_cast<uint32_t>(data[offset + 2]) << 8) |
                      static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    
    // Picture data length (32-bit big-endian)
    uint32_t pic_len = (static_cast<uint32_t>(data[offset]) << 24) |
                       (static_cast<uint32_t>(data[offset + 1]) << 16) |
                       (static_cast<uint32_t>(data[offset + 2]) << 8) |
                       static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    
    // Sanity check picture data length
    if (pic_len > TagConstants::MAX_PICTURE_SIZE) { 
        Debug::log("tag", "VorbisComment: Picture data length ", pic_len, " exceeds maximum");
        return std::nullopt;
    }
    
    if (pic_len > data.size() - offset) {
        Debug::log("tag", "VorbisComment: Picture data length exceeds data");
        return std::nullopt;
    }
    
    // Picture data
    pic.data.assign(data.begin() + offset, data.begin() + offset + pic_len);

    // Validate or fallback dimensions
    if (pic.width == 0 || pic.height == 0) {
        ImageUtils::extractDimensions(pic);
        if (pic.width != 0 && pic.height != 0) {
             Debug::log("tag", "VorbisComment: Extracted dimensions from image data: ", pic.width, "x", pic.height);
        }
    }
    
    Debug::log("tag", "VorbisComment: Parsed picture: type=", static_cast<int>(pic.type),
               ", mime=", pic.mime_type, ", ", pic.width, "x", pic.height, ", ", pic_len, " bytes");
    
    return pic;
}

// ============================================================================
// Tag interface implementation
// ============================================================================

std::string VorbisCommentTag::getFirstValue(const std::string& key) const {
    std::string normalized = normalizeFieldName(key);
    auto it = m_fields.find(normalized);
    if (it != m_fields.end() && !it->second.empty()) {
        return it->second[0];
    }
    return "";
}

uint32_t VorbisCommentTag::getNumberValue(const std::string& key) const {
    std::string value = getFirstValue(key);
    if (value.empty()) {
        return 0;
    }
    
    try {
        return static_cast<uint32_t>(std::stoul(value));
    } catch (...) {
        return 0;
    }
}

std::string VorbisCommentTag::title() const {
    return getFirstValue("TITLE");
}

std::string VorbisCommentTag::artist() const {
    return getFirstValue("ARTIST");
}

std::string VorbisCommentTag::album() const {
    return getFirstValue("ALBUM");
}

std::string VorbisCommentTag::albumArtist() const {
    return getFirstValue("ALBUMARTIST");
}

std::string VorbisCommentTag::genre() const {
    return getFirstValue("GENRE");
}

uint32_t VorbisCommentTag::year() const {
    // Try DATE first, then YEAR
    uint32_t year = getNumberValue("DATE");
    if (year == 0) {
        year = getNumberValue("YEAR");
    }
    return year;
}

uint32_t VorbisCommentTag::track() const {
    return getNumberValue("TRACKNUMBER");
}

uint32_t VorbisCommentTag::trackTotal() const {
    // Try TRACKTOTAL first, then TOTALTRACKS
    uint32_t total = getNumberValue("TRACKTOTAL");
    if (total == 0) {
        total = getNumberValue("TOTALTRACKS");
    }
    return total;
}

uint32_t VorbisCommentTag::disc() const {
    return getNumberValue("DISCNUMBER");
}

uint32_t VorbisCommentTag::discTotal() const {
    // Try DISCTOTAL first, then TOTALDISCS
    uint32_t total = getNumberValue("DISCTOTAL");
    if (total == 0) {
        total = getNumberValue("TOTALDISCS");
    }
    return total;
}

std::string VorbisCommentTag::comment() const {
    // Try COMMENT first, then DESCRIPTION
    std::string comment = getFirstValue("COMMENT");
    if (comment.empty()) {
        comment = getFirstValue("DESCRIPTION");
    }
    return comment;
}

std::string VorbisCommentTag::composer() const {
    return getFirstValue("COMPOSER");
}

std::string VorbisCommentTag::getTag(const std::string& key) const {
    return getFirstValue(key);
}

std::vector<std::string> VorbisCommentTag::getTagValues(const std::string& key) const {
    std::string normalized = normalizeFieldName(key);
    auto it = m_fields.find(normalized);
    if (it != m_fields.end()) {
        return it->second;
    }
    return {};
}

std::map<std::string, std::string> VorbisCommentTag::getAllTags() const {
    std::map<std::string, std::string> result;
    for (const auto& [key, values] : m_fields) {
        if (!values.empty()) {
            result[key] = values[0];
        }
    }
    return result;
}

bool VorbisCommentTag::hasTag(const std::string& key) const {
    std::string normalized = normalizeFieldName(key);
    return m_fields.find(normalized) != m_fields.end();
}

size_t VorbisCommentTag::pictureCount() const {
    return m_pictures.size();
}

std::optional<Picture> VorbisCommentTag::getPicture(size_t index) const {
    if (index < m_pictures.size()) {
        return m_pictures[index];
    }
    return std::nullopt;
}

std::optional<Picture> VorbisCommentTag::getFrontCover() const {
    // Look for front cover picture type
    for (const auto& pic : m_pictures) {
        if (pic.type == PictureType::FrontCover) {
            return pic;
        }
    }
    
    // If no front cover, return first picture if available
    if (!m_pictures.empty()) {
        return m_pictures[0];
    }
    
    return std::nullopt;
}

bool VorbisCommentTag::isEmpty() const {
    return m_fields.empty() && m_pictures.empty();
}

std::string VorbisCommentTag::formatName() const {
    return "Vorbis Comments";
}

std::string VorbisCommentTag::vendorString() const {
    return m_vendor_string;
}

} // namespace Tag
} // namespace PsyMP3
