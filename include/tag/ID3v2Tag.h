/*
 * ID3v2Tag.h - ID3v2 tag implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef PSYMP3_TAG_ID3V2TAG_H
#define PSYMP3_TAG_ID3V2TAG_H

#include "tag/Tag.h"
#include <map>
#include <tuple>

namespace PsyMP3 {
namespace Tag {

/**
 * @brief ID3v2 frame structure
 * 
 * Represents a single ID3v2 frame containing metadata.
 * Frame IDs are normalized to v2.3+ format (4 characters).
 */
struct ID3v2Frame {
    std::string id;                    ///< Frame ID (4-char, normalized to v2.3+)
    std::vector<uint8_t> data;         ///< Frame data (after header, flags processed)
    uint16_t flags = 0;                ///< Frame flags (v2.3/v2.4 only)
    
    /**
     * @brief Check if frame is empty
     * @return true if frame has no data
     */
    bool isEmpty() const { return data.empty(); }
    
    /**
     * @brief Get frame data size
     * @return Size of frame data in bytes
     */
    size_t size() const { return data.size(); }
};

/**
 * @brief ID3v2 tag implementation
 * 
 * Supports ID3v2 versions 2.2, 2.3, and 2.4 with the following features:
 * - Text frames with multiple encoding support (ISO-8859-1, UTF-16, UTF-8)
 * - APIC/PIC frames for embedded artwork
 * - Synchsafe integer handling for v2.4
 * - Unsynchronization support
 * - Frame ID normalization (v2.2 3-char to v2.3+ 4-char)
 * 
 * ## Thread Safety
 * 
 * This class is thread-safe for concurrent read operations:
 * - All public accessor methods are const
 * - No mutable state is modified after construction
 * - Multiple threads can safely call any method simultaneously
 * - Data is fully parsed and stored during parse()
 * - Pictures are extracted and cached during construction
 * 
 * @see Tag class documentation for complete thread safety guarantees
 */
class ID3v2Tag : public Tag {
public:
    /// ID3v2 header size in bytes
    static constexpr size_t HEADER_SIZE = 10;
    
    /// Maximum tag size (256MB - reasonable limit)
    static constexpr size_t MAX_TAG_SIZE = 256 * 1024 * 1024;
    
    /**
     * @brief Parse ID3v2 tag from raw data
     * 
     * Parses ID3v2 tag starting with "ID3" header. Supports versions 2.2, 2.3, and 2.4.
     * Handles unsynchronization, extended headers, and all standard frame types.
     * 
     * @param data Pointer to ID3v2 data (starting with "ID3" header)
     * @param size Size of available data
     * @return Unique pointer to ID3v2Tag, or nullptr if invalid/unsupported
     */
    static std::unique_ptr<ID3v2Tag> parse(const uint8_t* data, size_t size);
    
    /**
     * @brief Check if data contains a valid ID3v2 header
     * 
     * Validates magic bytes and version numbers without full parsing.
     * 
     * @param data Pointer to at least 10 bytes of data
     * @param size Size of available data
     * @return true if data starts with valid ID3v2 header
     */
    static bool isValid(const uint8_t* data, size_t size);
    
    /**
     * @brief Get total tag size from header
     * 
     * Extracts the tag size from ID3v2 header without parsing the entire tag.
     * Includes the 10-byte header in the returned size.
     * 
     * @param header Pointer to 10-byte ID3v2 header
     * @return Total tag size in bytes (including header), or 0 if invalid
     */
    static size_t getTagSize(const uint8_t* header);
    
    /**
     * @brief Normalize frame ID from v2.2 to v2.3+ format
     * 
     * Converts 3-character v2.2 frame IDs to 4-character v2.3+ equivalents.
     * Unknown frame IDs are returned unchanged.
     * 
     * @param id Frame ID to normalize
     * @param version Major version (2, 3, or 4)
     * @return Normalized 4-character frame ID
     */
    static std::string normalizeFrameId(const std::string& id, uint8_t version);
    
    // Default constructor (creates empty tag)
    ID3v2Tag() = default;
    ~ID3v2Tag() override = default;
    
    // Non-copyable but movable
    ID3v2Tag(const ID3v2Tag&) = delete;
    ID3v2Tag& operator=(const ID3v2Tag&) = delete;
    ID3v2Tag(ID3v2Tag&&) = default;
    ID3v2Tag& operator=(ID3v2Tag&&) = default;
    
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
    // ID3v2-specific methods
    // ========================================================================
    
    /**
     * @brief Get ID3v2 major version
     * @return Major version (2, 3, or 4)
     */
    uint8_t majorVersion() const { return m_major_version; }
    
    /**
     * @brief Get ID3v2 minor version
     * @return Minor version (typically 0)
     */
    uint8_t minorVersion() const { return m_minor_version; }
    
    /**
     * @brief Get ID3v2 header flags
     * @return Header flags byte
     */
    uint8_t headerFlags() const { return m_flags; }
    
    /**
     * @brief Check if unsynchronization is enabled
     * @return true if unsync flag is set in header
     */
    bool hasUnsynchronization() const { return (m_flags & 0x80) != 0; }
    
    /**
     * @brief Check if extended header is present
     * @return true if extended header flag is set
     */
    bool hasExtendedHeader() const { return (m_flags & 0x40) != 0; }
    
    /**
     * @brief Check if experimental indicator is set (v2.3+)
     * @return true if experimental flag is set
     */
    bool isExperimental() const { return (m_flags & 0x20) != 0; }
    
    /**
     * @brief Check if footer is present (v2.4 only)
     * @return true if footer flag is set
     */
    bool hasFooter() const { return (m_flags & 0x10) != 0; }
    
    /**
     * @brief Get all frames with a specific ID
     * 
     * @param frame_id Frame ID (4-character, normalized)
     * @return Vector of frames with matching ID
     */
    std::vector<ID3v2Frame> getFrames(const std::string& frame_id) const;
    
    /**
     * @brief Get first frame with a specific ID
     * 
     * @param frame_id Frame ID (4-character, normalized)
     * @return Pointer to first matching frame, or nullptr if not found
     */
    const ID3v2Frame* getFrame(const std::string& frame_id) const;
    
    /**
     * @brief Get all frame IDs present in the tag
     * @return Vector of unique frame IDs
     */
    std::vector<std::string> getFrameIds() const;
    
private:
    uint8_t m_major_version = 0;                                    ///< Major version (2, 3, or 4)
    uint8_t m_minor_version = 0;                                    ///< Minor version
    uint8_t m_flags = 0;                                            ///< Header flags
    std::map<std::string, std::vector<ID3v2Frame>> m_frames;       ///< Frames by ID
    std::vector<Picture> m_pictures;                                ///< Cached pictures from APIC/PIC frames
    
    // ========================================================================
    // Internal parsing methods
    // ========================================================================
    
    /**
     * @brief Parse ID3v2 header
     * 
     * @param data Pointer to header data
     * @param size Available data size
     * @return true if header is valid and parsed successfully
     */
    bool parseHeader(const uint8_t* data, size_t size);
    
    /**
     * @brief Parse all frames in the tag
     * 
     * @param data Pointer to frame data (after header and extended header)
     * @param size Size of frame data
     * @return true if parsing completed successfully
     */
    bool parseFrames(const uint8_t* data, size_t size);
    
    /**
     * @brief Parse a single frame
     * 
     * @param data Pointer to frame data
     * @param size Available data size
     * @param bytes_consumed Output: number of bytes consumed
     * @return Parsed frame, or empty frame if parsing failed
     */
    ID3v2Frame parseFrame(const uint8_t* data, size_t size, size_t& bytes_consumed);
    
    /**
     * @brief Parse frame header
     * 
     * @param data Pointer to frame header
     * @param size Available data size
     * @param frame_id Output: frame ID
     * @param frame_size Output: frame data size
     * @param frame_flags Output: frame flags
     * @return Size of frame header, or 0 if invalid
     */
    size_t parseFrameHeader(const uint8_t* data, size_t size, 
                           std::string& frame_id, uint32_t& frame_size, uint16_t& frame_flags);
    
    /**
     * @brief Skip extended header if present
     * 
     * @param data Pointer to data after main header
     * @param size Available data size
     * @return Number of bytes to skip, or 0 if no extended header
     */
    size_t skipExtendedHeader(const uint8_t* data, size_t size);
    
    /**
     * @brief Parse APIC frame for pictures
     * 
     * @param frame APIC frame to parse
     * @return Picture data, or nullopt if parsing failed
     */
    std::optional<Picture> parseAPIC(const ID3v2Frame& frame) const;
    
    /**
     * @brief Parse PIC frame (v2.2) for pictures
     * 
     * @param frame PIC frame to parse
     * @return Picture data, or nullopt if parsing failed
     */
    std::optional<Picture> parsePIC(const ID3v2Frame& frame) const;
    
    /**
     * @brief Extract and cache all pictures from APIC/PIC frames
     */
    void extractPictures();
    
    /**
     * @brief Extract image dimensions from picture data
     * 
     * @param picture Picture to analyze and update with dimensions
     */
    void extractImageDimensions(Picture& picture) const;
    
    // ========================================================================
    // Text frame helpers
    // ========================================================================
    
    /**
     * @brief Get text content from a text frame
     * 
     * @param frame_id Frame ID to look up
     * @return Decoded text content, or empty string if not found
     */
    std::string getTextFrame(const std::string& frame_id) const;
    
    /**
     * @brief Get all text values from frames with the same ID
     * 
     * @param frame_id Frame ID to look up
     * @return Vector of decoded text values
     */
    std::vector<std::string> getTextFrameValues(const std::string& frame_id) const;
    
    /**
     * @brief Parse track/disc number from text (handles "N/M" format)
     * 
     * @param text Text containing number or "number/total" format
     * @return Pair of (number, total), where total is 0 if not specified
     */
    static std::pair<uint32_t, uint32_t> parseNumberPair(const std::string& text);
    
    /**
     * @brief Parse year from date text (handles various formats)
     * 
     * @param text Date text (YYYY, YYYY-MM-DD, etc.)
     * @return Year as integer, or 0 if not parseable
     */
    static uint32_t parseYear(const std::string& text);
    
    /**
     * @brief Normalize key name for frame lookup
     * 
     * @param key Key name to normalize
     * @return Uppercase key name
     */
    static std::string normalizeKey(const std::string& key);
    
    /**
     * @brief Map common tag names to ID3v2 frame IDs
     * 
     * @param key Tag name (e.g., "title", "artist")
     * @return ID3v2 frame ID, or empty string if no mapping
     */
    static std::string mapKeyToFrameId(const std::string& key);
};

} // namespace Tag
} // namespace PsyMP3

#endif // PSYMP3_TAG_ID3V2TAG_H