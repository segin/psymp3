/*
 * ID3v2Tag.cpp - ID3v2 tag implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

namespace PsyMP3 {
namespace Tag {

// ============================================================================
// Static factory methods
// ============================================================================

std::unique_ptr<ID3v2Tag> ID3v2Tag::parse(const uint8_t* data, size_t size) {
    if (!isValid(data, size)) {
        Debug::log("tag", "ID3v2Tag::parse: Invalid ID3v2 header");
        return nullptr;
    }
    
    auto tag = std::make_unique<ID3v2Tag>();
    
    // Parse header
    if (!tag->parseHeader(data, size)) {
        Debug::log("tag", "ID3v2Tag::parse: Failed to parse header");
        return nullptr;
    }
    
    // Get tag size from header
    size_t tag_size = getTagSize(data);
    if (tag_size > size) {
        Debug::log("tag", "ID3v2Tag::parse: Tag size (", tag_size, ") exceeds available data (", size, ")");
        return nullptr;
    }
    
    // Skip extended header if present
    size_t frame_data_offset = HEADER_SIZE;
    if (tag->hasExtendedHeader()) {
        size_t ext_header_size = tag->skipExtendedHeader(data + HEADER_SIZE, tag_size - HEADER_SIZE);
        if (ext_header_size == 0) {
            Debug::log("tag", "ID3v2Tag::parse: Failed to skip extended header");
            return nullptr;
        }
        frame_data_offset += ext_header_size;
    }
    
    // Parse frames
    size_t frame_data_size = tag_size - frame_data_offset;
    if (tag->hasFooter()) {
        frame_data_size -= 10; // Footer is 10 bytes
    }
    
    if (frame_data_size > 0) {
        if (!tag->parseFrames(data + frame_data_offset, frame_data_size)) {
            Debug::log("tag", "ID3v2Tag::parse: Failed to parse frames");
            // Don't return nullptr - partial parsing is acceptable
        }
    }
    
    // Extract pictures from APIC/PIC frames
    tag->extractPictures();
    
    Debug::log("tag", "ID3v2Tag::parse: Successfully parsed ID3v", 
               static_cast<int>(tag->m_major_version), ".", static_cast<int>(tag->m_minor_version),
               " tag with ", tag->m_frames.size(), " frame types");
    
    return tag;
}

bool ID3v2Tag::isValid(const uint8_t* data, size_t size) {
    if (!data || size < HEADER_SIZE) {
        return false;
    }
    
    // Check magic bytes "ID3"
    if (data[0] != 'I' || data[1] != 'D' || data[2] != '3') {
        return false;
    }
    
    // Check version (support 2.2, 2.3, 2.4)
    uint8_t major_version = data[3];
    uint8_t minor_version = data[4];
    
    if (major_version < 2 || major_version > 4) {
        return false;
    }
    
    // Minor version should be 0 for standard versions
    if (minor_version != 0) {
        Debug::log("tag", "ID3v2Tag::isValid: Non-standard minor version ", static_cast<int>(minor_version));
        // Don't reject - just log warning
    }
    
    // Check that synchsafe size bytes don't have high bit set
    for (int i = 6; i < 10; ++i) {
        if (data[i] & 0x80) {
            return false; // Invalid synchsafe integer
        }
    }
    
    return true;
}

size_t ID3v2Tag::getTagSize(const uint8_t* header) {
    if (!header) {
        return 0;
    }
    
    // Extract synchsafe size (bytes 6-9)
    uint32_t synchsafe_size = ID3v2Utils::decodeSynchsafeBytes(header + 6);
    
    // Add header size (10 bytes)
    size_t total_size = HEADER_SIZE + synchsafe_size;
    
    // Sanity check - prevent integer overflow
    if (synchsafe_size > MAX_TAG_SIZE - HEADER_SIZE) {
        Debug::log("tag", "ID3v2Tag::getTagSize: Synchsafe size ", synchsafe_size, " would overflow");
        return 0;
    }
    
    // Sanity check
    if (total_size > MAX_TAG_SIZE) {
        Debug::log("tag", "ID3v2Tag::getTagSize: Tag size ", total_size, " exceeds maximum ", MAX_TAG_SIZE);
        return 0;
    }
    
    return total_size;
}

// ============================================================================
// Header parsing implementation
// ============================================================================

bool ID3v2Tag::parseHeader(const uint8_t* data, size_t size) {
    if (!data || size < HEADER_SIZE) {
        return false;
    }
    
    // Magic bytes already validated by isValid()
    
    // Extract version
    m_major_version = data[3];
    m_minor_version = data[4];
    
    // Extract flags
    m_flags = data[5];
    
    // Validate flags based on version
    if (m_major_version == 2) {
        // ID3v2.2 only supports unsync flag (bit 7)
        if (m_flags & 0x7F) {
            Debug::log("tag", "ID3v2Tag::parseHeader: Invalid flags for v2.2: 0x", 
                      std::hex, static_cast<int>(m_flags));
            // Clear invalid flags but continue
            m_flags &= 0x80;
        }
    } else if (m_major_version == 3) {
        // ID3v2.3 supports unsync (bit 7), extended header (bit 6), experimental (bit 5)
        if (m_flags & 0x1F) {
            Debug::log("tag", "ID3v2Tag::parseHeader: Invalid flags for v2.3: 0x", 
                      std::hex, static_cast<int>(m_flags));
            m_flags &= 0xE0;
        }
    } else if (m_major_version == 4) {
        // ID3v2.4 supports unsync (bit 7), extended header (bit 6), experimental (bit 5), footer (bit 4)
        if (m_flags & 0x0F) {
            Debug::log("tag", "ID3v2Tag::parseHeader: Invalid flags for v2.4: 0x", 
                      std::hex, static_cast<int>(m_flags));
            m_flags &= 0xF0;
        }
    }
    
    Debug::log("tag", "ID3v2Tag::parseHeader: ID3v", static_cast<int>(m_major_version), 
               ".", static_cast<int>(m_minor_version), " flags=0x", std::hex, static_cast<int>(m_flags));
    
    return true;
}

size_t ID3v2Tag::skipExtendedHeader(const uint8_t* data, size_t size) {
    if (!data || size < 4) {
        return 0;
    }
    
    uint32_t ext_header_size;
    
    if (m_major_version == 3) {
        // ID3v2.3: 4-byte size (not synchsafe)
        ext_header_size = (static_cast<uint32_t>(data[0]) << 24) |
                         (static_cast<uint32_t>(data[1]) << 16) |
                         (static_cast<uint32_t>(data[2]) << 8) |
                         static_cast<uint32_t>(data[3]);
        
        // Size includes the 4-byte size field itself
        if (ext_header_size < 4 || ext_header_size > size) {
            Debug::log("tag", "ID3v2Tag::skipExtendedHeader: Invalid v2.3 extended header size: ", ext_header_size);
            return 0;
        }
        
        Debug::log("tag", "ID3v2Tag::skipExtendedHeader: Skipping v2.3 extended header (", ext_header_size, " bytes)");
        return ext_header_size;
        
    } else if (m_major_version == 4) {
        // ID3v2.4: 4-byte synchsafe size (excludes size field itself)
        ext_header_size = ID3v2Utils::decodeSynchsafeBytes(data);
        
        if (ext_header_size == 0 || ext_header_size + 4 > size) {
            Debug::log("tag", "ID3v2Tag::skipExtendedHeader: Invalid v2.4 extended header size: ", ext_header_size);
            return 0;
        }
        
        Debug::log("tag", "ID3v2Tag::skipExtendedHeader: Skipping v2.4 extended header (", ext_header_size + 4, " bytes)");
        return ext_header_size + 4; // Add 4 bytes for the size field
        
    } else {
        // ID3v2.2 doesn't support extended headers
        Debug::log("tag", "ID3v2Tag::skipExtendedHeader: Extended header flag set but not supported in v2.2");
        return 0;
    }
}

// ============================================================================
// Frame ID normalization
// ============================================================================

std::string ID3v2Tag::normalizeFrameId(const std::string& id, uint8_t version) {
    if (version >= 3) {
        // Already 4-character format
        return id;
    }
    
    // ID3v2.2 uses 3-character frame IDs - map to v2.3+ equivalents
    static const std::map<std::string, std::string> v22_to_v23_map = {
        // Text frames
        {"TT1", "TIT1"},  // Content group description
        {"TT2", "TIT2"},  // Title
        {"TT3", "TIT3"},  // Subtitle/Description refinement
        {"TP1", "TPE1"},  // Artist
        {"TP2", "TPE2"},  // Album artist
        {"TP3", "TPE3"},  // Conductor
        {"TP4", "TPE4"},  // Interpreted/remixed by
        {"TCM", "TCOM"},  // Composer
        {"TXT", "TEXT"},  // Lyricist
        {"TLA", "TLAN"},  // Language
        {"TCO", "TCON"},  // Genre
        {"TAL", "TALB"},  // Album
        {"TPA", "TPOS"},  // Part of set (disc)
        {"TRK", "TRCK"},  // Track number
        {"TRC", "TSRC"},  // ISRC
        {"TYE", "TYER"},  // Year (v2.3) / TDRC (v2.4)
        {"TDA", "TDAT"},  // Date
        {"TIM", "TIME"},  // Time
        {"TRD", "TRDA"},  // Recording dates
        {"TMT", "TMED"},  // Media type
        {"TFT", "TFLT"},  // File type
        {"TBP", "TBPM"},  // BPM
        {"TCR", "TCOP"},  // Copyright
        {"TPB", "TPUB"},  // Publisher
        {"TEN", "TENC"},  // Encoded by
        {"TSS", "TSSE"},  // Software/hardware settings
        {"TOF", "TOFN"},  // Original filename
        {"TLE", "TLEN"},  // Length
        {"TSI", "TSIZ"},  // Size
        {"TDY", "TDLY"},  // Playlist delay
        {"TKE", "TKEY"},  // Initial key
        {"TOT", "TOAL"},  // Original album
        {"TOA", "TOPE"},  // Original artist
        {"TOL", "TOLY"},  // Original lyricist
        {"TOR", "TORY"},  // Original release year
        
        // URL frames
        {"WAF", "WOAF"},  // Official audio file webpage
        {"WAR", "WOAR"},  // Official artist webpage
        {"WAS", "WOAS"},  // Official audio source webpage
        {"WCM", "WCOM"},  // Commercial information
        {"WCP", "WCOP"},  // Copyright information
        {"WPB", "WPUB"},  // Publishers official webpage
        
        // Comment/text frames
        {"COM", "COMM"},  // Comment
        {"ULT", "USLT"},  // Unsynchronized lyrics
        
        // Picture frame
        {"PIC", "APIC"},  // Attached picture
        
        // Other frames
        {"CNT", "PCNT"},  // Play counter
        {"POP", "POPM"},  // Popularimeter
        {"BUF", "RBUF"},  // Recommended buffer size
        {"CRA", "AENC"},  // Audio encryption
        {"CRM", "COMR"},  // Commercial frame
        {"ETC", "ETCO"},  // Event timing codes
        {"EQU", "EQUA"},  // Equalization
        {"IPL", "IPLS"},  // Involved people list
        {"LNK", "LINK"},  // Linked information
        {"MCI", "MCDI"},  // Music CD identifier
        {"MLL", "MLLT"},  // MPEG location lookup table
        {"REV", "RVRB"},  // Reverb
        {"SLT", "SYLT"},  // Synchronized lyrics
        {"STC", "SYTC"},  // Synchronized tempo codes
        {"UFI", "UFID"},  // Unique file identifier
        {"GEO", "GEOB"},  // General encapsulated object
    };
    
    auto it = v22_to_v23_map.find(id);
    if (it != v22_to_v23_map.end()) {
        return it->second;
    }
    
    // Unknown frame ID - return as-is
    Debug::log("tag", "ID3v2Tag::normalizeFrameId: Unknown v2.2 frame ID: ", id);
    return id;
}

// ============================================================================
// Frame parsing implementation
// ============================================================================

bool ID3v2Tag::parseFrames(const uint8_t* data, size_t size) {
    if (!data || size == 0) {
        return true; // Empty frame data is valid
    }
    
    // Apply unsynchronization if needed
    std::vector<uint8_t> frame_data;
    const uint8_t* parse_data = data;
    size_t parse_size = size;
    
    if (hasUnsynchronization()) {
        frame_data = ID3v2Utils::decodeUnsync(data, size);
        parse_data = frame_data.data();
        parse_size = frame_data.size();
        Debug::log("tag", "ID3v2Tag::parseFrames: Decoded unsynchronization, size ", size, " -> ", parse_size);
    }
    
    size_t offset = 0;
    int frame_count = 0;
    
    while (offset < parse_size) {
        // Check for padding (all zeros)
        if (parse_data[offset] == 0x00) {
            Debug::log("tag", "ID3v2Tag::parseFrames: Reached padding at offset ", offset);
            break;
        }
        
        size_t bytes_consumed = 0;
        ID3v2Frame frame = parseFrame(parse_data + offset, parse_size - offset, bytes_consumed);
        
        if (bytes_consumed == 0) {
            Debug::log("tag", "ID3v2Tag::parseFrames: Failed to parse frame at offset ", offset);
            break;
        }
        
        if (!frame.isEmpty()) {
            // Normalize frame ID
            frame.id = normalizeFrameId(frame.id, m_major_version);
            
            // Store frame
            m_frames[frame.id].push_back(std::move(frame));
            frame_count++;
            
            Debug::log("tag", "ID3v2Tag::parseFrames: Parsed frame ", frame.id, " (", frame.size(), " bytes)");
        }
        
        offset += bytes_consumed;
    }
    
    Debug::log("tag", "ID3v2Tag::parseFrames: Parsed ", frame_count, " frames");
    return true;
}

ID3v2Frame ID3v2Tag::parseFrame(const uint8_t* data, size_t size, size_t& bytes_consumed) {
    bytes_consumed = 0;
    
    if (!data || size == 0) {
        return {};
    }
    
    std::string frame_id;
    uint32_t frame_size;
    uint16_t frame_flags;
    
    // Parse frame header
    size_t header_size = parseFrameHeader(data, size, frame_id, frame_size, frame_flags);
    if (header_size == 0) {
        return {};
    }
    
    // Sanity check frame size (prevent huge allocations)
    if (frame_size > 100 * 1024 * 1024) { // 100MB max frame
        Debug::log("tag", "ID3v2Tag::parseFrame: Frame ", frame_id, " size ", frame_size, 
                  " exceeds maximum (100MB)");
        bytes_consumed = header_size + std::min(frame_size, static_cast<uint32_t>(size - header_size));
        return {};
    }
    
    // Validate frame size
    if (header_size + frame_size > size) {
        Debug::log("tag", "ID3v2Tag::parseFrame: Frame ", frame_id, " size ", frame_size, 
                  " exceeds available data (", size - header_size, ")");
        return {};
    }
    
    // Skip frames with invalid IDs
    if (frame_id.empty() || frame_id.find('\0') != std::string::npos) {
        Debug::log("tag", "ID3v2Tag::parseFrame: Invalid frame ID");
        bytes_consumed = header_size + frame_size;
        return {};
    }
    
    ID3v2Frame frame;
    frame.id = frame_id;
    frame.flags = frame_flags;
    
    // Extract frame data
    const uint8_t* frame_data = data + header_size;
    size_t data_size = frame_size;
    
    // Handle frame-level unsynchronization (v2.4 only)
    std::vector<uint8_t> unsync_data;
    if (m_major_version == 4 && (frame_flags & 0x0002)) {
        unsync_data = ID3v2Utils::decodeUnsync(frame_data, data_size);
        frame_data = unsync_data.data();
        data_size = unsync_data.size();
        Debug::log("tag", "ID3v2Tag::parseFrame: Decoded frame-level unsync for ", frame_id);
    }
    
    // Handle data length indicator (v2.4 only)
    if (m_major_version == 4 && (frame_flags & 0x0001)) {
        if (data_size < 4) {
            Debug::log("tag", "ID3v2Tag::parseFrame: Frame ", frame_id, " has data length indicator but insufficient data");
            bytes_consumed = header_size + frame_size;
            return {};
        }
        
        // Skip 4-byte data length indicator
        frame_data += 4;
        data_size -= 4;
        Debug::log("tag", "ID3v2Tag::parseFrame: Skipped data length indicator for ", frame_id);
    }
    
    // Handle compression (not commonly used, log warning)
    if ((m_major_version == 3 && (frame_flags & 0x0080)) ||
        (m_major_version == 4 && (frame_flags & 0x0008))) {
        Debug::log("tag", "ID3v2Tag::parseFrame: Frame ", frame_id, " is compressed (not supported)");
        bytes_consumed = header_size + frame_size;
        return {};
    }
    
    // Handle encryption (not supported)
    if ((m_major_version == 3 && (frame_flags & 0x0040)) ||
        (m_major_version == 4 && (frame_flags & 0x0004))) {
        Debug::log("tag", "ID3v2Tag::parseFrame: Frame ", frame_id, " is encrypted (not supported)");
        bytes_consumed = header_size + frame_size;
        return {};
    }
    
    // Copy frame data
    if (data_size > 0) {
        frame.data.assign(frame_data, frame_data + data_size);
    }
    
    bytes_consumed = header_size + frame_size;
    return frame;
}

size_t ID3v2Tag::parseFrameHeader(const uint8_t* data, size_t size, 
                                 std::string& frame_id, uint32_t& frame_size, uint16_t& frame_flags) {
    frame_id.clear();
    frame_size = 0;
    frame_flags = 0;
    
    if (!data) {
        return 0;
    }
    
    if (m_major_version == 2) {
        // ID3v2.2: 3-byte frame ID + 3-byte size (no flags)
        if (size < 6) {
            return 0;
        }
        
        // Extract frame ID (3 characters)
        frame_id.assign(reinterpret_cast<const char*>(data), 3);
        
        // Extract frame size (24-bit big-endian)
        frame_size = (static_cast<uint32_t>(data[3]) << 16) |
                    (static_cast<uint32_t>(data[4]) << 8) |
                    static_cast<uint32_t>(data[5]);
        
        // No flags in v2.2
        frame_flags = 0;
        
        return 6;
        
    } else {
        // ID3v2.3/2.4: 4-byte frame ID + 4-byte size + 2-byte flags
        if (size < 10) {
            return 0;
        }
        
        // Extract frame ID (4 characters)
        frame_id.assign(reinterpret_cast<const char*>(data), 4);
        
        // Extract frame size
        if (m_major_version == 3) {
            // v2.3: Regular 32-bit big-endian
            frame_size = (static_cast<uint32_t>(data[4]) << 24) |
                        (static_cast<uint32_t>(data[5]) << 16) |
                        (static_cast<uint32_t>(data[6]) << 8) |
                        static_cast<uint32_t>(data[7]);
        } else {
            // v2.4: Synchsafe integer
            frame_size = ID3v2Utils::decodeSynchsafeBytes(data + 4);
        }
        
        // Extract frame flags
        frame_flags = (static_cast<uint16_t>(data[8]) << 8) | static_cast<uint16_t>(data[9]);
        
        return 10;
    }
}

// ============================================================================
// Text frame helpers
// ============================================================================

std::string ID3v2Tag::getTextFrame(const std::string& frame_id) const {
    auto it = m_frames.find(frame_id);
    if (it == m_frames.end() || it->second.empty()) {
        return "";
    }
    
    const ID3v2Frame& frame = it->second[0]; // Use first frame
    if (frame.data.empty()) {
        return "";
    }
    
    // Text frames start with encoding byte
    return ID3v2Utils::decodeTextWithEncoding(frame.data.data(), frame.data.size());
}

std::vector<std::string> ID3v2Tag::getTextFrameValues(const std::string& frame_id) const {
    std::vector<std::string> values;
    
    auto it = m_frames.find(frame_id);
    if (it == m_frames.end()) {
        return values;
    }
    
    for (const auto& frame : it->second) {
        if (!frame.data.empty()) {
            std::string text = ID3v2Utils::decodeTextWithEncoding(frame.data.data(), frame.data.size());
            if (!text.empty()) {
                values.push_back(text);
            }
        }
    }
    
    return values;
}

std::pair<uint32_t, uint32_t> ID3v2Tag::parseNumberPair(const std::string& text) {
    if (text.empty()) {
        return {0, 0};
    }
    
    size_t slash_pos = text.find('/');
    if (slash_pos == std::string::npos) {
        // No slash - just a number
        try {
            uint32_t number = static_cast<uint32_t>(std::stoul(text));
            return {number, 0};
        } catch (const std::exception&) {
            return {0, 0};
        }
    }
    
    // Parse "number/total" format
    try {
        uint32_t number = static_cast<uint32_t>(std::stoul(text.substr(0, slash_pos)));
        uint32_t total = static_cast<uint32_t>(std::stoul(text.substr(slash_pos + 1)));
        return {number, total};
    } catch (const std::exception&) {
        return {0, 0};
    }
}

uint32_t ID3v2Tag::parseYear(const std::string& text) {
    if (text.empty()) {
        return 0;
    }
    
    // Extract first 4 digits
    std::string year_str;
    for (char c : text) {
        if (std::isdigit(c)) {
            year_str += c;
            if (year_str.length() == 4) {
                break;
            }
        } else if (!year_str.empty()) {
            // Non-digit after digits - stop
            break;
        }
    }
    
    if (year_str.length() == 4) {
        try {
            uint32_t year = static_cast<uint32_t>(std::stoul(year_str));
            // Sanity check (reasonable year range)
            if (year >= 1900 && year <= 2100) {
                return year;
            }
        } catch (const std::exception&) {
            // Fall through
        }
    }
    
    return 0;
}

std::string ID3v2Tag::normalizeKey(const std::string& key) {
    std::string normalized;
    normalized.reserve(key.size());
    
    for (char c : key) {
        normalized += static_cast<char>(std::toupper(c));
    }
    
    return normalized;
}

std::string ID3v2Tag::mapKeyToFrameId(const std::string& key) {
    std::string norm_key = normalizeKey(key);
    
    static const std::map<std::string, std::string> key_to_frame_map = {
        {"TITLE", "TIT2"},
        {"ARTIST", "TPE1"},
        {"ALBUM", "TALB"},
        {"ALBUMARTIST", "TPE2"},
        {"GENRE", "TCON"},
        {"YEAR", "TYER"},  // v2.3
        {"DATE", "TDRC"},  // v2.4
        {"TRACK", "TRCK"},
        {"TRACKNUMBER", "TRCK"},
        {"DISC", "TPOS"},
        {"DISCNUMBER", "TPOS"},
        {"COMMENT", "COMM"},
        {"COMPOSER", "TCOM"},
        {"CONDUCTOR", "TPE3"},
        {"LYRICIST", "TEXT"},
        {"PUBLISHER", "TPUB"},
        {"COPYRIGHT", "TCOP"},
        {"ENCODER", "TENC"},
        {"BPM", "TBPM"},
        {"KEY", "TKEY"},
        {"LANGUAGE", "TLAN"},
        {"LENGTH", "TLEN"},
        {"MEDIATYPE", "TMED"},
        {"ORIGINALARTIST", "TOPE"},
        {"ORIGINALALBUM", "TOAL"},
        {"ORIGINALYEAR", "TORY"},
        {"PLAYLISTDELAY", "TDLY"},
        {"SUBTITLE", "TIT3"},
        {"GROUPING", "TIT1"},
    };
    
    auto it = key_to_frame_map.find(norm_key);
    if (it != key_to_frame_map.end()) {
        return it->second;
    }
    
    return "";
}
// ============================================================================
// Picture parsing implementation
// ============================================================================

void ID3v2Tag::extractPictures() {
    m_pictures.clear();
    
    // Extract from APIC frames (v2.3+)
    auto apic_it = m_frames.find("APIC");
    if (apic_it != m_frames.end()) {
        for (const auto& frame : apic_it->second) {
            auto picture = parseAPIC(frame);
            if (picture.has_value()) {
                m_pictures.push_back(std::move(picture.value()));
            }
        }
    }
    
    // Extract from PIC frames (v2.2)
    auto pic_it = m_frames.find("PIC");
    if (pic_it != m_frames.end()) {
        for (const auto& frame : pic_it->second) {
            auto picture = parsePIC(frame);
            if (picture.has_value()) {
                m_pictures.push_back(std::move(picture.value()));
            }
        }
    }
    
    Debug::log("tag", "ID3v2Tag::extractPictures: Extracted ", m_pictures.size(), " pictures");
}

std::optional<Picture> ID3v2Tag::parseAPIC(const ID3v2Frame& frame) const {
    if (frame.data.empty()) {
        return std::nullopt;
    }
    
    const uint8_t* data = frame.data.data();
    size_t size = frame.data.size();
    size_t offset = 0;
    
    // Parse text encoding
    if (offset >= size) {
        Debug::log("tag", "ID3v2Tag::parseAPIC: No encoding byte");
        return std::nullopt;
    }
    
    ID3v2Utils::TextEncoding encoding = static_cast<ID3v2Utils::TextEncoding>(data[offset]);
    if (data[offset] > 3) {
        Debug::log("tag", "ID3v2Tag::parseAPIC: Invalid encoding: ", static_cast<int>(data[offset]));
        encoding = ID3v2Utils::TextEncoding::ISO_8859_1;
    }
    offset++;
    
    // Parse MIME type (null-terminated)
    if (offset >= size) {
        Debug::log("tag", "ID3v2Tag::parseAPIC: No MIME type");
        return std::nullopt;
    }
    
    size_t mime_end = offset;
    while (mime_end < size && data[mime_end] != 0x00) {
        mime_end++;
    }
    
    if (mime_end >= size) {
        Debug::log("tag", "ID3v2Tag::parseAPIC: MIME type not null-terminated");
        return std::nullopt;
    }
    
    // Sanity check MIME type length
    if (mime_end - offset > 256) {
        Debug::log("tag", "ID3v2Tag::parseAPIC: MIME type too long (", mime_end - offset, " bytes)");
        return std::nullopt;
    }
    
    std::string mime_type = ID3v2Utils::decodeUTF8Safe(data + offset, mime_end - offset);
    offset = mime_end + 1; // Skip null terminator
    
    // Parse picture type
    if (offset >= size) {
        Debug::log("tag", "ID3v2Tag::parseAPIC: No picture type");
        return std::nullopt;
    }
    
    uint8_t pic_type = data[offset];
    offset++;
    
    // Parse description (null-terminated, encoding-dependent)
    if (offset >= size) {
        Debug::log("tag", "ID3v2Tag::parseAPIC: No description");
        return std::nullopt;
    }
    
    size_t desc_start = offset;
    size_t null_size = ID3v2Utils::getNullTerminatorSize(encoding);
    size_t desc_end = ID3v2Utils::findNullTerminator(data + offset, size - offset, encoding);
    
    if (desc_end == size - offset) {
        Debug::log("tag", "ID3v2Tag::parseAPIC: Description not null-terminated");
        return std::nullopt;
    }
    
    // Sanity check description length
    if (desc_end > 1024 * 1024) { // 1MB max description
        Debug::log("tag", "ID3v2Tag::parseAPIC: Description too long (", desc_end, " bytes)");
        return std::nullopt;
    }
    
    std::string description;
    if (desc_end > 0) {
        description = ID3v2Utils::decodeText(data + desc_start, desc_end, encoding);
    }
    offset = desc_start + desc_end + null_size;
    
    // Remaining data is the picture
    if (offset >= size) {
        Debug::log("tag", "ID3v2Tag::parseAPIC: No picture data");
        return std::nullopt;
    }
    
    // Sanity check picture data size
    size_t pic_size = size - offset;
    if (pic_size > 100 * 1024 * 1024) { // 100MB max picture
        Debug::log("tag", "ID3v2Tag::parseAPIC: Picture data too large (", pic_size, " bytes)");
        return std::nullopt;
    }
    
    Picture picture;
    picture.type = static_cast<PictureType>(pic_type);
    picture.mime_type = mime_type;
    picture.description = description;
    picture.data.assign(data + offset, data + size);
    
    // Try to extract dimensions from image headers
    extractImageDimensions(picture);
    
    Debug::log("tag", "ID3v2Tag::parseAPIC: Parsed APIC frame - type=", static_cast<int>(pic_type),
               ", MIME=", mime_type, ", desc='", description, "', size=", picture.data.size());
    
    return picture;
}

std::optional<Picture> ID3v2Tag::parsePIC(const ID3v2Frame& frame) const {
    if (frame.data.empty()) {
        return std::nullopt;
    }
    
    const uint8_t* data = frame.data.data();
    size_t size = frame.data.size();
    size_t offset = 0;
    
    // Parse text encoding
    if (offset >= size) {
        Debug::log("tag", "ID3v2Tag::parsePIC: No encoding byte");
        return std::nullopt;
    }
    
    ID3v2Utils::TextEncoding encoding = static_cast<ID3v2Utils::TextEncoding>(data[offset]);
    if (data[offset] > 3) {
        Debug::log("tag", "ID3v2Tag::parsePIC: Invalid encoding: ", static_cast<int>(data[offset]));
        encoding = ID3v2Utils::TextEncoding::ISO_8859_1;
    }
    offset++;
    
    // Parse image format (3 bytes: "JPG", "PNG", etc.)
    if (offset + 3 > size) {
        Debug::log("tag", "ID3v2Tag::parsePIC: No image format");
        return std::nullopt;
    }
    
    std::string image_format = ID3v2Utils::decodeUTF8Safe(data + offset, 3);
    offset += 3;
    
    // Convert image format to MIME type
    std::string mime_type;
    if (image_format == "JPG") {
        mime_type = "image/jpeg";
    } else if (image_format == "PNG") {
        mime_type = "image/png";
    } else if (image_format == "GIF") {
        mime_type = "image/gif";
    } else if (image_format == "BMP") {
        mime_type = "image/bmp";
    } else {
        mime_type = "image/" + image_format;
        std::transform(mime_type.begin() + 6, mime_type.end(), mime_type.begin() + 6, ::tolower);
    }
    
    // Parse picture type
    if (offset >= size) {
        Debug::log("tag", "ID3v2Tag::parsePIC: No picture type");
        return std::nullopt;
    }
    
    uint8_t pic_type = data[offset];
    offset++;
    
    // Parse description (null-terminated, encoding-dependent)
    if (offset >= size) {
        Debug::log("tag", "ID3v2Tag::parsePIC: No description");
        return std::nullopt;
    }
    
    size_t desc_start = offset;
    size_t null_size = ID3v2Utils::getNullTerminatorSize(encoding);
    size_t desc_end = ID3v2Utils::findNullTerminator(data + offset, size - offset, encoding);
    
    if (desc_end == size - offset) {
        Debug::log("tag", "ID3v2Tag::parsePIC: Description not null-terminated");
        return std::nullopt;
    }
    
    // Sanity check description length
    if (desc_end > 1024 * 1024) { // 1MB max description
        Debug::log("tag", "ID3v2Tag::parsePIC: Description too long (", desc_end, " bytes)");
        return std::nullopt;
    }
    
    std::string description;
    if (desc_end > 0) {
        description = ID3v2Utils::decodeText(data + desc_start, desc_end, encoding);
    }
    offset = desc_start + desc_end + null_size;
    
    // Remaining data is the picture
    if (offset >= size) {
        Debug::log("tag", "ID3v2Tag::parsePIC: No picture data");
        return std::nullopt;
    }
    
    // Sanity check picture data size
    size_t pic_size = size - offset;
    if (pic_size > 100 * 1024 * 1024) { // 100MB max picture
        Debug::log("tag", "ID3v2Tag::parsePIC: Picture data too large (", pic_size, " bytes)");
        return std::nullopt;
    }
    
    Picture picture;
    picture.type = static_cast<PictureType>(pic_type);
    picture.mime_type = mime_type;
    picture.description = description;
    picture.data.assign(data + offset, data + size);
    
    // Try to extract dimensions from image headers
    extractImageDimensions(picture);
    
    Debug::log("tag", "ID3v2Tag::parsePIC: Parsed PIC frame - type=", static_cast<int>(pic_type),
               ", format=", image_format, ", desc='", description, "', size=", picture.data.size());
    
    return picture;
}

void ID3v2Tag::extractImageDimensions(Picture& picture) const {
    if (picture.data.size() < 16) {
        return; // Too small for any image header
    }
    
    const uint8_t* data = picture.data.data();
    
    // Try to extract dimensions based on MIME type
    if (picture.mime_type == "image/jpeg") {
        // JPEG: Look for SOF0 (0xFFC0) marker
        for (size_t i = 0; i + 8 < picture.data.size(); i++) {
            if (data[i] == 0xFF && data[i + 1] == 0xC0) {
                // SOF0 marker found
                if (i + 7 < picture.data.size()) {
                    picture.height = (static_cast<uint32_t>(data[i + 5]) << 8) | data[i + 6];
                    picture.width = (static_cast<uint32_t>(data[i + 7]) << 8) | data[i + 8];
                    picture.color_depth = data[i + 4] * 8; // Components * 8 bits
                    return;
                }
            }
        }
    } else if (picture.mime_type == "image/png") {
        // PNG: Check IHDR chunk (should be at offset 8)
        if (picture.data.size() >= 24 && 
            data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47) {
            // PNG signature verified, read IHDR
            picture.width = (static_cast<uint32_t>(data[16]) << 24) |
                           (static_cast<uint32_t>(data[17]) << 16) |
                           (static_cast<uint32_t>(data[18]) << 8) |
                           static_cast<uint32_t>(data[19]);
            picture.height = (static_cast<uint32_t>(data[20]) << 24) |
                            (static_cast<uint32_t>(data[21]) << 16) |
                            (static_cast<uint32_t>(data[22]) << 8) |
                            static_cast<uint32_t>(data[23]);
            picture.color_depth = data[24]; // Bit depth
            return;
        }
    } else if (picture.mime_type == "image/gif") {
        // GIF: Check header
        if (picture.data.size() >= 10 &&
            ((data[0] == 'G' && data[1] == 'I' && data[2] == 'F' && data[3] == '8' && data[4] == '7' && data[5] == 'a') ||
             (data[0] == 'G' && data[1] == 'I' && data[2] == 'F' && data[3] == '8' && data[4] == '9' && data[5] == 'a'))) {
            // GIF signature verified
            picture.width = static_cast<uint32_t>(data[6]) | (static_cast<uint32_t>(data[7]) << 8);
            picture.height = static_cast<uint32_t>(data[8]) | (static_cast<uint32_t>(data[9]) << 8);
            // GIF color depth is in global color table flag
            if (picture.data.size() >= 11) {
                uint8_t packed = data[10];
                if (packed & 0x80) { // Global color table flag
                    picture.color_depth = ((packed & 0x07) + 1); // Bits per pixel
                }
            }
            return;
        }
    } else if (picture.mime_type == "image/bmp") {
        // BMP: Check header
        if (picture.data.size() >= 26 && data[0] == 'B' && data[1] == 'M') {
            // BMP signature verified, read DIB header
            picture.width = static_cast<uint32_t>(data[18]) |
                           (static_cast<uint32_t>(data[19]) << 8) |
                           (static_cast<uint32_t>(data[20]) << 16) |
                           (static_cast<uint32_t>(data[21]) << 24);
            picture.height = static_cast<uint32_t>(data[22]) |
                            (static_cast<uint32_t>(data[23]) << 8) |
                            (static_cast<uint32_t>(data[24]) << 16) |
                            (static_cast<uint32_t>(data[25]) << 24);
            if (picture.data.size() >= 28) {
                picture.color_depth = static_cast<uint32_t>(data[28]) | (static_cast<uint32_t>(data[29]) << 8);
            }
            return;
        }
    }
}
// ============================================================================
// Tag interface implementation
// ============================================================================

std::string ID3v2Tag::title() const {
    return getTextFrame("TIT2");
}

std::string ID3v2Tag::artist() const {
    return getTextFrame("TPE1");
}

std::string ID3v2Tag::album() const {
    return getTextFrame("TALB");
}

std::string ID3v2Tag::albumArtist() const {
    return getTextFrame("TPE2");
}

std::string ID3v2Tag::genre() const {
    std::string genre = getTextFrame("TCON");
    
    // Handle ID3v1-style genre numbers in parentheses
    if (!genre.empty() && genre[0] == '(' && genre.find(')') != std::string::npos) {
        size_t close_paren = genre.find(')');
        std::string number_str = genre.substr(1, close_paren - 1);
        
        try {
            uint8_t genre_index = static_cast<uint8_t>(std::stoul(number_str));
            std::string id3v1_genre = ID3v1Tag::genreFromIndex(genre_index);
            if (!id3v1_genre.empty()) {
                // Check if there's additional text after the parentheses
                if (close_paren + 1 < genre.length()) {
                    return id3v1_genre + genre.substr(close_paren + 1);
                }
                return id3v1_genre;
            }
        } catch (const std::exception&) {
            // Not a valid number - return as-is
        }
    }
    
    return genre;
}

uint32_t ID3v2Tag::year() const {
    // Try TDRC first (v2.4 recording time)
    std::string date = getTextFrame("TDRC");
    if (!date.empty()) {
        return parseYear(date);
    }
    
    // Try TYER (v2.3 year)
    date = getTextFrame("TYER");
    if (!date.empty()) {
        return parseYear(date);
    }
    
    // Try TDAT + TYER combination (v2.3)
    std::string year_str = getTextFrame("TYER");
    if (!year_str.empty()) {
        return parseYear(year_str);
    }
    
    return 0;
}

uint32_t ID3v2Tag::track() const {
    std::string track_str = getTextFrame("TRCK");
    std::pair<uint32_t, uint32_t> result = parseNumberPair(track_str);
    return result.first;
}

uint32_t ID3v2Tag::trackTotal() const {
    std::string track_str = getTextFrame("TRCK");
    std::pair<uint32_t, uint32_t> result = parseNumberPair(track_str);
    return result.second;
}

uint32_t ID3v2Tag::disc() const {
    std::string disc_str = getTextFrame("TPOS");
    std::pair<uint32_t, uint32_t> result = parseNumberPair(disc_str);
    return result.first;
}

uint32_t ID3v2Tag::discTotal() const {
    std::string disc_str = getTextFrame("TPOS");
    std::pair<uint32_t, uint32_t> result = parseNumberPair(disc_str);
    return result.second;
}

std::string ID3v2Tag::comment() const {
    // COMM frames have a more complex structure than text frames
    auto it = m_frames.find("COMM");
    if (it == m_frames.end() || it->second.empty()) {
        return "";
    }
    
    // Use first COMM frame
    const ID3v2Frame& frame = it->second[0];
    if (frame.data.size() < 4) {
        return "";
    }
    
    const uint8_t* data = frame.data.data();
    size_t size = frame.data.size();
    size_t offset = 0;
    
    // Parse encoding
    ID3v2Utils::TextEncoding encoding = static_cast<ID3v2Utils::TextEncoding>(data[offset]);
    if (data[offset] > 3) {
        encoding = ID3v2Utils::TextEncoding::ISO_8859_1;
    }
    offset++;
    
    // Skip language (3 bytes)
    if (offset + 3 > size) {
        return "";
    }
    offset += 3;
    
    // Skip short description (null-terminated)
    size_t null_size = ID3v2Utils::getNullTerminatorSize(encoding);
    size_t desc_end = ID3v2Utils::findNullTerminator(data + offset, size - offset, encoding);
    if (desc_end == size - offset) {
        return ""; // No null terminator found
    }
    offset += desc_end + null_size;
    
    // Remaining data is the comment text
    if (offset >= size) {
        return "";
    }
    
    return ID3v2Utils::decodeText(data + offset, size - offset, encoding);
}

std::string ID3v2Tag::composer() const {
    return getTextFrame("TCOM");
}

std::string ID3v2Tag::getTag(const std::string& key) const {
    // Try direct frame ID lookup first
    std::string frame_id = mapKeyToFrameId(key);
    if (!frame_id.empty()) {
        return getTextFrame(frame_id);
    }
    
    // Try key as frame ID directly
    std::string upper_key = normalizeKey(key);
    return getTextFrame(upper_key);
}

std::vector<std::string> ID3v2Tag::getTagValues(const std::string& key) const {
    // Try direct frame ID lookup first
    std::string frame_id = mapKeyToFrameId(key);
    if (!frame_id.empty()) {
        return getTextFrameValues(frame_id);
    }
    
    // Try key as frame ID directly
    std::string upper_key = normalizeKey(key);
    return getTextFrameValues(upper_key);
}

std::map<std::string, std::string> ID3v2Tag::getAllTags() const {
    std::map<std::string, std::string> tags;
    
    // Add standard tags
    if (!title().empty()) tags["TITLE"] = title();
    if (!artist().empty()) tags["ARTIST"] = artist();
    if (!album().empty()) tags["ALBUM"] = album();
    if (!albumArtist().empty()) tags["ALBUMARTIST"] = albumArtist();
    if (!genre().empty()) tags["GENRE"] = genre();
    if (year() != 0) tags["YEAR"] = std::to_string(year());
    if (track() != 0) tags["TRACK"] = std::to_string(track());
    if (trackTotal() != 0) tags["TRACKTOTAL"] = std::to_string(trackTotal());
    if (disc() != 0) tags["DISC"] = std::to_string(disc());
    if (discTotal() != 0) tags["DISCTOTAL"] = std::to_string(discTotal());
    if (!comment().empty()) tags["COMMENT"] = comment();
    if (!composer().empty()) tags["COMPOSER"] = composer();
    
    // Add all text frames
    for (const auto& pair : m_frames) {
        if (!pair.second.empty() && pair.first.length() == 4 && pair.first[0] == 'T') {
            // Text frame
            std::string value = getTextFrame(pair.first);
            if (!value.empty()) {
                tags[pair.first] = value;
            }
        }
    }
    
    return tags;
}

bool ID3v2Tag::hasTag(const std::string& key) const {
    // Try direct frame ID lookup first
    std::string frame_id = mapKeyToFrameId(key);
    if (!frame_id.empty()) {
        auto it = m_frames.find(frame_id);
        return it != m_frames.end() && !it->second.empty();
    }
    
    // Try key as frame ID directly
    std::string upper_key = normalizeKey(key);
    auto it = m_frames.find(upper_key);
    return it != m_frames.end() && !it->second.empty();
}

size_t ID3v2Tag::pictureCount() const {
    return m_pictures.size();
}

std::optional<Picture> ID3v2Tag::getPicture(size_t index) const {
    if (index >= m_pictures.size()) {
        return std::nullopt;
    }
    
    return m_pictures[index];
}

std::optional<Picture> ID3v2Tag::getFrontCover() const {
    // Look for front cover picture type first
    for (const auto& picture : m_pictures) {
        if (picture.type == PictureType::FrontCover) {
            return picture;
        }
    }
    
    // If no front cover, return first picture
    if (!m_pictures.empty()) {
        return m_pictures[0];
    }
    
    return std::nullopt;
}

bool ID3v2Tag::isEmpty() const {
    return m_frames.empty();
}

std::string ID3v2Tag::formatName() const {
    return "ID3v2." + std::to_string(m_major_version);
}

// ============================================================================
// ID3v2-specific methods
// ============================================================================

std::vector<ID3v2Frame> ID3v2Tag::getFrames(const std::string& frame_id) const {
    auto it = m_frames.find(frame_id);
    if (it != m_frames.end()) {
        return it->second;
    }
    return {};
}

const ID3v2Frame* ID3v2Tag::getFrame(const std::string& frame_id) const {
    auto it = m_frames.find(frame_id);
    if (it != m_frames.end() && !it->second.empty()) {
        return &it->second[0];
    }
    return nullptr;
}

std::vector<std::string> ID3v2Tag::getFrameIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_frames.size());
    
    for (const auto& pair : m_frames) {
        if (!pair.second.empty()) {
            ids.push_back(pair.first);
        }
    }
    
    return ids;
}

} // namespace Tag
} // namespace PsyMP3
