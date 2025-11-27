/*
 * OggDemuxer.cpp - Ogg container demuxer implementation using libogg
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * RFC 3533 compliant Ogg container demuxer implementation.
 * The implementation follows the ogg-demuxer-fix spec which requires:
 * - Exact behavior patterns from libvorbisfile and libopusfile reference implementations
 * - RFC 3533 compliance for Ogg container format
 * - RFC 7845 compliance for Opus encapsulation
 * - RFC 9639 Section 10.1 compliance for FLAC-in-Ogg
 * - Property-based testing with RapidCheck
 */

#include "psymp3.h"

// OggDemuxer is built if HAVE_OGGDEMUXER is defined
#ifdef HAVE_OGGDEMUXER

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

// ============================================================================
// OggPageParser Implementation (RFC 3533 Section 6)
// ============================================================================

// CRC32 lookup table initialization flag
bool OggPageParser::s_crc_table_initialized = false;

// CRC32 lookup table (polynomial 0x04c11db7)
// This is the same CRC polynomial used by libogg
const uint32_t OggPageParser::s_crc_lookup[256] = {
    0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9,
    0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
    0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
    0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
    0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9,
    0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
    0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011,
    0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
    0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
    0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
    0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81,
    0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
    0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49,
    0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
    0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
    0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
    0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae,
    0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
    0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16,
    0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
    0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
    0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
    0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066,
    0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
    0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e,
    0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
    0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
    0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
    0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e,
    0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
    0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686,
    0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
    0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
    0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
    0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f,
    0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
    0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47,
    0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
    0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
    0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
    0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7,
    0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
    0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f,
    0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
    0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
    0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
    0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f,
    0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
    0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640,
    0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
    0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
    0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
    0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30,
    0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
    0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088,
    0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
    0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
    0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
    0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18,
    0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
    0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0,
    0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
    0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
    0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

void OggPageParser::initCRCTable() {
    // Table is pre-computed, nothing to do
    s_crc_table_initialized = true;
}

bool OggPageParser::validateCapturePattern(const uint8_t* data, size_t size, size_t offset) {
    if (!data || size < offset + 4) {
        return false;
    }
    
    // Check for "OggS" capture pattern (0x4f 0x67 0x67 0x53)
    return data[offset + 0] == 0x4F &&  // 'O'
           data[offset + 1] == 0x67 &&  // 'g'
           data[offset + 2] == 0x67 &&  // 'g'
           data[offset + 3] == 0x53;    // 'S'
}

bool OggPageParser::validateVersion(uint8_t version) {
    // RFC 3533 Section 6: stream_structure_version must be 0
    return version == 0;
}

void OggPageParser::parseHeaderFlags(uint8_t flags, bool& is_continued, bool& is_bos, bool& is_eos) {
    // RFC 3533 Section 6: Header type flags
    is_continued = (flags & OggPageHeader::CONTINUED_PACKET) != 0;  // 0x01
    is_bos = (flags & OggPageHeader::FIRST_PAGE) != 0;              // 0x02
    is_eos = (flags & OggPageHeader::LAST_PAGE) != 0;               // 0x04
}

OggPageParser::ParseResult OggPageParser::parsePage(const uint8_t* data, size_t size, 
                                                     OggPage& page, size_t& bytes_consumed) {
    bytes_consumed = 0;
    page.clear();
    
    // Need at least minimum header size
    if (size < OGG_PAGE_HEADER_MIN_SIZE) {
        return ParseResult::NEED_MORE_DATA;
    }
    
    // Validate capture pattern "OggS"
    if (!validateCapturePattern(data, size, 0)) {
        return ParseResult::INVALID_CAPTURE;
    }
    
    // Copy capture pattern
    std::memcpy(page.header.capture_pattern, data, 4);
    
    // Parse version (offset 4)
    page.header.version = data[4];
    if (!validateVersion(page.header.version)) {
        return ParseResult::INVALID_VERSION;
    }
    
    // Parse header type flags (offset 5)
    page.header.header_type = data[5];
    
    // Parse granule position (offset 6-13, 64-bit little-endian)
    page.header.granule_position = 
        static_cast<uint64_t>(data[6]) |
        (static_cast<uint64_t>(data[7]) << 8) |
        (static_cast<uint64_t>(data[8]) << 16) |
        (static_cast<uint64_t>(data[9]) << 24) |
        (static_cast<uint64_t>(data[10]) << 32) |
        (static_cast<uint64_t>(data[11]) << 40) |
        (static_cast<uint64_t>(data[12]) << 48) |
        (static_cast<uint64_t>(data[13]) << 56);
    
    // Parse serial number (offset 14-17, 32-bit little-endian)
    page.header.serial_number = 
        static_cast<uint32_t>(data[14]) |
        (static_cast<uint32_t>(data[15]) << 8) |
        (static_cast<uint32_t>(data[16]) << 16) |
        (static_cast<uint32_t>(data[17]) << 24);
    
    // Parse page sequence number (offset 18-21, 32-bit little-endian)
    page.header.page_sequence = 
        static_cast<uint32_t>(data[18]) |
        (static_cast<uint32_t>(data[19]) << 8) |
        (static_cast<uint32_t>(data[20]) << 16) |
        (static_cast<uint32_t>(data[21]) << 24);
    
    // Parse CRC32 checksum (offset 22-25, 32-bit little-endian)
    page.header.checksum = 
        static_cast<uint32_t>(data[22]) |
        (static_cast<uint32_t>(data[23]) << 8) |
        (static_cast<uint32_t>(data[24]) << 16) |
        (static_cast<uint32_t>(data[25]) << 24);
    
    // Parse number of page segments (offset 26)
    page.header.page_segments = data[26];
    
    // Calculate header size
    page.header_size = OGG_PAGE_HEADER_MIN_SIZE + page.header.page_segments;
    
    // Check if we have enough data for segment table
    if (size < page.header_size) {
        return ParseResult::NEED_MORE_DATA;
    }
    
    // Parse segment table (lacing values)
    page.segment_table.resize(page.header.page_segments);
    page.body_size = 0;
    for (size_t i = 0; i < page.header.page_segments; ++i) {
        page.segment_table[i] = data[OGG_PAGE_HEADER_MIN_SIZE + i];
        page.body_size += page.segment_table[i];
    }
    
    // Calculate total page size
    page.total_size = page.header_size + page.body_size;
    
    // Validate page size against RFC 3533 maximum
    if (page.total_size > OGG_PAGE_SIZE_MAX) {
        return ParseResult::INVALID_SIZE;
    }
    
    // Check if we have enough data for body
    if (size < page.total_size) {
        return ParseResult::NEED_MORE_DATA;
    }
    
    // Copy body data
    page.body.resize(page.body_size);
    if (page.body_size > 0) {
        std::memcpy(page.body.data(), data + page.header_size, page.body_size);
    }
    
    // Validate CRC32 checksum
    if (!validateCRC32(data, page.total_size)) {
        Debug::log("ogg", "CRC32 validation failed for page at offset, expected: 0x%08x", 
                   page.header.checksum);
        return ParseResult::CRC_MISMATCH;
    }
    
    bytes_consumed = page.total_size;
    return ParseResult::SUCCESS;
}

bool OggPageParser::calculatePageSize(const uint8_t* data, size_t size, size_t& page_size) {
    // Need at least minimum header to read segment count
    if (size < OGG_PAGE_HEADER_MIN_SIZE) {
        return false;
    }
    
    // Validate capture pattern first
    if (!validateCapturePattern(data, size, 0)) {
        return false;
    }
    
    // Get segment count
    uint8_t num_segments = data[26];
    size_t header_size = OGG_PAGE_HEADER_MIN_SIZE + num_segments;
    
    // Need segment table to calculate body size
    if (size < header_size) {
        return false;
    }
    
    // Sum lacing values to get body size
    size_t body_size = 0;
    for (size_t i = 0; i < num_segments; ++i) {
        body_size += data[OGG_PAGE_HEADER_MIN_SIZE + i];
    }
    
    page_size = header_size + body_size;
    return true;
}

uint32_t OggPageParser::calculateCRC32(const uint8_t* data, size_t size) {
    uint32_t crc = 0;
    
    for (size_t i = 0; i < size; ++i) {
        // For CRC calculation, the checksum field (bytes 22-25) should be treated as zeros
        uint8_t byte;
        if (i >= 22 && i <= 25) {
            byte = 0;
        } else {
            byte = data[i];
        }
        crc = (crc << 8) ^ s_crc_lookup[((crc >> 24) & 0xFF) ^ byte];
    }
    
    return crc;
}

bool OggPageParser::validateCRC32(const uint8_t* data, size_t size) {
    if (size < OGG_PAGE_HEADER_MIN_SIZE) {
        return false;
    }
    
    // Extract stored checksum (offset 22-25, little-endian)
    uint32_t stored_crc = 
        static_cast<uint32_t>(data[22]) |
        (static_cast<uint32_t>(data[23]) << 8) |
        (static_cast<uint32_t>(data[24]) << 16) |
        (static_cast<uint32_t>(data[25]) << 24);
    
    // Calculate CRC with checksum field zeroed
    uint32_t calculated_crc = calculateCRC32(data, size);
    
    return stored_crc == calculated_crc;
}

int64_t OggPageParser::findNextCapturePattern(const uint8_t* data, size_t size, size_t start_offset) {
    if (!data || size < 4) {
        return -1;
    }
    
    for (size_t i = start_offset; i <= size - 4; ++i) {
        if (validateCapturePattern(data, size, i)) {
            return static_cast<int64_t>(i);
        }
    }
    
    return -1;
}

void OggPageParser::parseSegmentTable(const std::vector<uint8_t>& segment_table,
                                      std::vector<size_t>& packet_offsets,
                                      std::vector<size_t>& packet_sizes,
                                      std::vector<bool>& packet_complete) {
    packet_offsets.clear();
    packet_sizes.clear();
    packet_complete.clear();
    
    if (segment_table.empty()) {
        return;
    }
    
    size_t current_offset = 0;
    size_t current_packet_size = 0;
    bool packet_started = false;
    
    for (size_t i = 0; i < segment_table.size(); ++i) {
        uint8_t lacing_value = segment_table[i];
        
        // Start a new packet if we haven't started one
        if (!packet_started) {
            packet_offsets.push_back(current_offset);
            current_packet_size = 0;
            packet_started = true;
        }
        
        // Add this segment's size to current packet
        current_packet_size += lacing_value;
        current_offset += lacing_value;
        
        // RFC 3533 Section 5: Lacing value < 255 indicates packet termination
        if (lacing_value < 255) {
            // Packet is complete
            packet_sizes.push_back(current_packet_size);
            packet_complete.push_back(true);
            packet_started = false;
        }
    }
    
    // Handle incomplete packet at end of page (last lacing value was 255)
    if (packet_started) {
        packet_sizes.push_back(current_packet_size);
        packet_complete.push_back(false);  // Packet continues on next page
    }
}

size_t OggPageParser::countCompletePackets(const std::vector<uint8_t>& segment_table) {
    size_t count = 0;
    
    for (uint8_t lacing_value : segment_table) {
        // Each lacing value < 255 marks end of a packet
        if (lacing_value < 255) {
            ++count;
        }
    }
    
    return count;
}

bool OggPageParser::isLastPacketComplete(const std::vector<uint8_t>& segment_table) {
    if (segment_table.empty()) {
        return true;  // No packets means nothing incomplete
    }
    
    // Last packet is complete if last lacing value < 255
    return segment_table.back() < 255;
}

// ============================================================================
// OggDemuxer Page Header Field Extraction (RFC 3533 Section 6)
// These methods wrap libogg functions for compatibility
// ============================================================================

int64_t OggDemuxer::pageGranulePos(const ogg_page* page) {
    if (!page || !page->header) {
        return -1;
    }
    // Use libogg's ogg_page_granulepos() for compatibility
    return ogg_page_granulepos(const_cast<ogg_page*>(page));
}

uint32_t OggDemuxer::pageSerialNo(const ogg_page* page) {
    if (!page || !page->header) {
        return 0;
    }
    // Use libogg's ogg_page_serialno() for compatibility
    return static_cast<uint32_t>(ogg_page_serialno(const_cast<ogg_page*>(page)));
}

uint32_t OggDemuxer::pageSequenceNo(const ogg_page* page) {
    if (!page || !page->header || page->header_len < 23) {
        return 0;
    }
    // Page sequence number is at offset 18-21 (little-endian)
    const uint8_t* header = page->header;
    return static_cast<uint32_t>(header[18]) |
           (static_cast<uint32_t>(header[19]) << 8) |
           (static_cast<uint32_t>(header[20]) << 16) |
           (static_cast<uint32_t>(header[21]) << 24);
}

bool OggDemuxer::pageBOS(const ogg_page* page) {
    if (!page || !page->header) {
        return false;
    }
    // Use libogg's ogg_page_bos() for compatibility
    return ogg_page_bos(const_cast<ogg_page*>(page)) != 0;
}

bool OggDemuxer::pageEOS(const ogg_page* page) {
    if (!page || !page->header) {
        return false;
    }
    // Use libogg's ogg_page_eos() for compatibility
    return ogg_page_eos(const_cast<ogg_page*>(page)) != 0;
}

bool OggDemuxer::pageContinued(const ogg_page* page) {
    if (!page || !page->header) {
        return false;
    }
    // Use libogg's ogg_page_continued() for compatibility
    return ogg_page_continued(const_cast<ogg_page*>(page)) != 0;
}

uint8_t OggDemuxer::pageSegments(const ogg_page* page) {
    if (!page || !page->header || page->header_len < 27) {
        return 0;
    }
    // Number of segments is at offset 26
    return page->header[26];
}

size_t OggDemuxer::pageHeaderSize(const ogg_page* page) {
    if (!page) {
        return 0;
    }
    // Header size = 27 (fixed) + number_of_segments
    return static_cast<size_t>(page->header_len);
}

size_t OggDemuxer::pageBodySize(const ogg_page* page) {
    if (!page) {
        return 0;
    }
    return static_cast<size_t>(page->body_len);
}

size_t OggDemuxer::pageTotalSize(const ogg_page* page) {
    if (!page) {
        return 0;
    }
    return pageHeaderSize(page) + pageBodySize(page);
}

bool OggDemuxer::pageValidateCRC(const ogg_page* page) {
    if (!page || !page->header || !page->body) {
        return false;
    }
    
    // Build complete page data for CRC validation
    size_t total_size = page->header_len + page->body_len;
    std::vector<uint8_t> page_data(total_size);
    std::memcpy(page_data.data(), page->header, page->header_len);
    std::memcpy(page_data.data() + page->header_len, page->body, page->body_len);
    
    return OggPageParser::validateCRC32(page_data.data(), total_size);
}

bool OggDemuxer::pageIsValid(const ogg_page* page) {
    if (!page || !page->header || 
        static_cast<size_t>(page->header_len) < OGG_PAGE_HEADER_MIN_SIZE) {
        return false;
    }
    
    // Validate capture pattern
    if (!OggPageParser::validateCapturePattern(page->header, page->header_len, 0)) {
        return false;
    }
    
    // Validate version
    if (!OggPageParser::validateVersion(page->header[4])) {
        return false;
    }
    
    // Validate page size
    size_t total_size = pageTotalSize(page);
    if (total_size > OGG_PAGE_SIZE_MAX) {
        return false;
    }
    
    return true;
}

// ============================================================================
// OggDemuxer Implementation
// ============================================================================

// Implementation follows the ogg-demuxer-fix spec
// See .kiro/specs/ogg-demuxer-fix/ for requirements, design, and tasks

OggDemuxer::OggDemuxer(std::unique_ptr<IOHandler> handler) 
    : Demuxer(std::move(handler)) {
    Debug::log("ogg", "OggDemuxer constructor - placeholder implementation");
    
    // Initialize libogg sync state
    ogg_sync_init(&m_sync_state);
}

OggDemuxer::~OggDemuxer() {
    Debug::log("ogg", "OggDemuxer destructor - cleaning up resources");
    
    // Clean up libogg structures
    for (auto& [stream_id, ogg_stream] : m_ogg_streams) {
        ogg_stream_clear(&ogg_stream);
    }
    ogg_sync_clear(&m_sync_state);
}

bool OggDemuxer::parseContainer() {
    Debug::log("ogg", "OggDemuxer::parseContainer - placeholder implementation");
    // TODO: Implement RFC 3533 compliant page parsing
    return false;
}

std::vector<StreamInfo> OggDemuxer::getStreams() const {
    return {};
}

StreamInfo OggDemuxer::getStreamInfo(uint32_t stream_id) const {
    return StreamInfo{};
}

MediaChunk OggDemuxer::readChunk() {
    return MediaChunk{};
}

MediaChunk OggDemuxer::readChunk(uint32_t stream_id) {
    return MediaChunk{};
}

bool OggDemuxer::seekTo(uint64_t timestamp_ms) {
    return false;
}

bool OggDemuxer::isEOF() const {
    return m_eof;
}

uint64_t OggDemuxer::getDuration() const {
    return 0;
}

uint64_t OggDemuxer::getPosition() const {
    return 0;
}

uint64_t OggDemuxer::getGranulePosition(uint32_t stream_id) const {
    return 0;
}

uint64_t OggDemuxer::granuleToMs(uint64_t granule, uint32_t stream_id) const {
    return 0;
}

uint64_t OggDemuxer::msToGranule(uint64_t timestamp_ms, uint32_t stream_id) const {
    return 0;
}

/**
 * @brief Identify codec type from BOS packet magic bytes
 * 
 * Following RFC 3533 Section 4 and reference implementation patterns:
 * - Vorbis: "\x01vorbis" (7 bytes) - vorbis_synthesis_idheader() pattern
 * - Opus: "OpusHead" (8 bytes) - opus_head_parse() pattern per RFC 7845
 * - FLAC: "\x7fFLAC" (5 bytes) - RFC 9639 Section 10.1
 * - Speex: "Speex   " (8 bytes with trailing spaces)
 * - Theora: "\x80theora" (7 bytes)
 * 
 * @param packet_data First packet data from BOS page
 * @return Codec name string ("vorbis", "opus", "flac", "speex", "theora") or empty if unknown
 * 
 * Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6
 */
std::string OggDemuxer::identifyCodec(const std::vector<uint8_t>& packet_data) {
    if (packet_data.empty()) {
        Debug::log("ogg", "identifyCodec: Empty packet data");
        return "";
    }
    
    // Vorbis identification: "\x01vorbis" (7 bytes)
    // First byte is packet type (0x01 = identification header)
    // Followed by "vorbis" magic string
    if (packet_data.size() >= 7) {
        if (packet_data[0] == 0x01 &&
            packet_data[1] == 'v' &&
            packet_data[2] == 'o' &&
            packet_data[3] == 'r' &&
            packet_data[4] == 'b' &&
            packet_data[5] == 'i' &&
            packet_data[6] == 's') {
            Debug::log("ogg", "identifyCodec: Detected Vorbis stream");
            return "vorbis";
        }
    }
    
    // Opus identification: "OpusHead" (8 bytes) per RFC 7845 Section 5.1
    if (packet_data.size() >= 8) {
        if (packet_data[0] == 'O' &&
            packet_data[1] == 'p' &&
            packet_data[2] == 'u' &&
            packet_data[3] == 's' &&
            packet_data[4] == 'H' &&
            packet_data[5] == 'e' &&
            packet_data[6] == 'a' &&
            packet_data[7] == 'd') {
            Debug::log("ogg", "identifyCodec: Detected Opus stream");
            return "opus";
        }
    }
    
    // FLAC-in-Ogg identification: "\x7fFLAC" (5 bytes) per RFC 9639 Section 10.1
    // 0x7F followed by "FLAC"
    if (packet_data.size() >= 5) {
        if (packet_data[0] == 0x7F &&
            packet_data[1] == 'F' &&
            packet_data[2] == 'L' &&
            packet_data[3] == 'A' &&
            packet_data[4] == 'C') {
            Debug::log("ogg", "identifyCodec: Detected FLAC-in-Ogg stream");
            return "flac";
        }
    }
    
    // Speex identification: "Speex   " (8 bytes with trailing spaces)
    if (packet_data.size() >= 8) {
        if (packet_data[0] == 'S' &&
            packet_data[1] == 'p' &&
            packet_data[2] == 'e' &&
            packet_data[3] == 'e' &&
            packet_data[4] == 'x' &&
            packet_data[5] == ' ' &&
            packet_data[6] == ' ' &&
            packet_data[7] == ' ') {
            Debug::log("ogg", "identifyCodec: Detected Speex stream");
            return "speex";
        }
    }
    
    // Theora identification: "\x80theora" (7 bytes)
    // First byte is packet type (0x80 = identification header)
    // Followed by "theora" magic string
    if (packet_data.size() >= 7) {
        if (packet_data[0] == 0x80 &&
            packet_data[1] == 't' &&
            packet_data[2] == 'h' &&
            packet_data[3] == 'e' &&
            packet_data[4] == 'o' &&
            packet_data[5] == 'r' &&
            packet_data[6] == 'a') {
            Debug::log("ogg", "identifyCodec: Detected Theora stream");
            return "theora";
        }
    }
    
    // Unknown codec - log first few bytes for debugging
    if (packet_data.size() >= 4) {
        Debug::log("ogg", "identifyCodec: Unknown codec, first bytes: 0x%02x 0x%02x 0x%02x 0x%02x",
                   packet_data[0], packet_data[1], packet_data[2], packet_data[3]);
    } else {
        Debug::log("ogg", "identifyCodec: Unknown codec, packet too small (%zu bytes)",
                   packet_data.size());
    }
    
    return "";
}

bool OggDemuxer::parseVorbisHeaders(OggStream& stream, const OggPacket& packet) {
    return false;
}

bool OggDemuxer::parseFLACHeaders(OggStream& stream, const OggPacket& packet) {
    return false;
}

bool OggDemuxer::parseOpusHeaders(OggStream& stream, const OggPacket& packet) {
    return false;
}

bool OggDemuxer::parseSpeexHeaders(OggStream& stream, const OggPacket& packet) {
    return false;
}

// ============================================================================
// Stream Multiplexing Handling (RFC 3533 Section 4)
// ============================================================================

/**
 * @brief Process an Ogg page and route to appropriate handler
 * 
 * Following RFC 3533 Section 4:
 * - Grouped bitstreams: all BOS pages appear before any data pages
 * - Chained bitstreams: EOS immediately followed by BOS
 * 
 * @param page Pointer to ogg_page structure
 * @return true on success, false on error
 * 
 * Requirements: 3.7, 3.8, 3.9, 3.10, 3.11
 */
bool OggDemuxer::processPage(ogg_page* page) {
    if (!page || !pageIsValid(page)) {
        Debug::log("ogg", "processPage: Invalid page");
        return false;
    }
    
    uint32_t serial_number = pageSerialNo(page);
    bool is_bos = pageBOS(page);
    bool is_eos = pageEOS(page);
    
    Debug::log("ogg", "processPage: serial=0x%08x, BOS=%d, EOS=%d, granule=%lld",
               serial_number, is_bos, is_eos,
               static_cast<long long>(pageGranulePos(page)));
    
    // Handle BOS (Beginning of Stream) pages
    if (is_bos) {
        return handleBOSPage(page, serial_number);
    }
    
    // Handle EOS (End of Stream) pages
    if (is_eos) {
        return handleEOSPage(page, serial_number);
    }
    
    // Handle regular data pages
    return handleDataPage(page, serial_number);
}

/**
 * @brief Handle a BOS (Beginning of Stream) page
 * 
 * Following RFC 3533 Section 4:
 * - BOS pages contain codec identification in first packet
 * - For grouped streams, all BOS pages appear before any data pages
 * - For chained streams, BOS appears after EOS of previous chain
 * 
 * @param page Pointer to ogg_page structure
 * @param serial_number Serial number of the stream
 * @return true on success, false on error
 * 
 * Requirements: 3.1-3.6, 3.7, 3.8, 3.11
 */
bool OggDemuxer::handleBOSPage(ogg_page* page, uint32_t serial_number) {
    Debug::log("ogg", "handleBOSPage: Processing BOS page for serial 0x%08x", serial_number);
    
    // Check for duplicate serial numbers (error condition per Requirements 4.14)
    if (m_bos_serial_numbers.count(serial_number) > 0) {
        Debug::log("ogg", "handleBOSPage: Duplicate serial number 0x%08x detected", serial_number);
        // This is an error condition - return OP_EBADHEADER equivalent
        return false;
    }
    
    // Check if this is a chained stream boundary
    if (isChainedStreamBoundary(page, serial_number)) {
        Debug::log("ogg", "handleBOSPage: Detected chained stream boundary, chain %u",
                   m_chain_count + 1);
        
        // Reset state for new chain
        m_chain_count++;
        m_bos_serial_numbers.clear();
        m_eos_serial_numbers.clear();
        m_in_headers_phase = true;
        m_seen_data_page = false;
        
        // Clean up old stream states for the previous chain
        // Keep the ogg_stream_state structures but reset them
        for (auto& [old_serial, ogg_stream] : m_ogg_streams) {
            ogg_stream_reset(&ogg_stream);
        }
    }
    
    // Record this BOS serial number
    m_bos_serial_numbers.insert(serial_number);
    
    // Initialize ogg_stream_state for this serial number if not exists
    auto ogg_stream_it = m_ogg_streams.find(serial_number);
    if (ogg_stream_it == m_ogg_streams.end()) {
        ogg_stream_state new_stream;
        if (ogg_stream_init(&new_stream, static_cast<int>(serial_number)) != 0) {
            Debug::log("ogg", "handleBOSPage: Failed to init ogg_stream_state for 0x%08x",
                       serial_number);
            return false;
        }
        m_ogg_streams[serial_number] = new_stream;
        ogg_stream_it = m_ogg_streams.find(serial_number);
    }
    
    // Submit page to stream state using ogg_stream_pagein()
    if (ogg_stream_pagein(&ogg_stream_it->second, page) != 0) {
        Debug::log("ogg", "handleBOSPage: ogg_stream_pagein() failed for serial 0x%08x",
                   serial_number);
        return false;
    }
    
    // Extract the first packet to identify codec
    ogg_packet ogg_pkt;
    int ret = ogg_stream_packetout(&ogg_stream_it->second, &ogg_pkt);
    
    if (ret != 1) {
        Debug::log("ogg", "handleBOSPage: Failed to extract identification packet");
        return false;
    }
    
    // Create packet data vector for codec identification
    std::vector<uint8_t> packet_data(ogg_pkt.packet, ogg_pkt.packet + ogg_pkt.bytes);
    
    // Identify codec from magic bytes
    std::string codec_name = identifyCodec(packet_data);
    
    if (codec_name.empty()) {
        Debug::log("ogg", "handleBOSPage: Unknown codec for serial 0x%08x", serial_number);
        // Skip unknown streams per Requirements 4.11
        // Don't fail, just don't create an OggStream for it
        return true;
    }
    
    // Create or update OggStream for this serial number
    OggStream& stream = m_streams[serial_number];
    stream.serial_number = serial_number;
    stream.codec_name = codec_name;
    stream.codec_type = "audio";  // Default to audio for now
    stream.headers_complete = false;
    stream.headers_sent = false;
    stream.next_header_index = 0;
    stream.page_sequence_initialized = false;
    stream.last_page_sequence = pageSequenceNo(page);
    stream.page_sequence_initialized = true;
    
    // Store the identification packet as first header packet
    OggPacket header_packet;
    header_packet.stream_id = serial_number;
    header_packet.data = std::move(packet_data);
    header_packet.granule_position = static_cast<uint64_t>(ogg_pkt.granulepos);
    header_packet.is_first_packet = true;
    header_packet.is_last_packet = false;
    header_packet.is_continued = false;
    stream.header_packets.push_back(std::move(header_packet));
    
    // Set expected header count based on codec
    if (codec_name == "vorbis") {
        stream.expected_header_count = 3;  // Identification, comment, setup
    } else if (codec_name == "opus") {
        stream.expected_header_count = 2;  // OpusHead, OpusTags
    } else if (codec_name == "flac") {
        // FLAC-in-Ogg: header count is in the identification header
        // Will be parsed in parseFLACHeaders()
        stream.expected_header_count = 0;  // Unknown until parsed
    } else if (codec_name == "speex") {
        stream.expected_header_count = 2;  // Header, comment
    } else if (codec_name == "theora") {
        stream.expected_header_count = 3;  // Identification, comment, setup
        stream.codec_type = "video";
    }
    
    Debug::log("ogg", "handleBOSPage: Created stream 0x%08x, codec=%s, expected_headers=%u",
               serial_number, codec_name.c_str(), stream.expected_header_count);
    
    return true;
}

/**
 * @brief Handle an EOS (End of Stream) page
 * 
 * Following RFC 3533 Section 4:
 * - EOS marks the end of a logical bitstream
 * - May be a nil page (header only, no content)
 * - For chained streams, EOS is immediately followed by BOS
 * 
 * @param page Pointer to ogg_page structure
 * @param serial_number Serial number of the stream
 * @return true on success, false on error
 * 
 * Requirements: 1.13, 1.14, 3.8
 */
bool OggDemuxer::handleEOSPage(ogg_page* page, uint32_t serial_number) {
    Debug::log("ogg", "handleEOSPage: Processing EOS page for serial 0x%08x", serial_number);
    
    // Record that this stream has seen EOS
    m_eos_serial_numbers.insert(serial_number);
    
    // Check if this is a nil EOS page (header only, no content)
    bool is_nil_eos = (pageBodySize(page) == 0);
    if (is_nil_eos) {
        Debug::log("ogg", "handleEOSPage: Nil EOS page (header only) for serial 0x%08x",
                   serial_number);
    }
    
    // Submit page to stream state
    auto ogg_stream_it = m_ogg_streams.find(serial_number);
    if (ogg_stream_it != m_ogg_streams.end()) {
        if (ogg_stream_pagein(&ogg_stream_it->second, page) != 0) {
            Debug::log("ogg", "handleEOSPage: ogg_stream_pagein() failed for serial 0x%08x",
                       serial_number);
            // Continue anyway - EOS is important to record
        }
        
        // Extract any remaining packets
        ogg_packet ogg_pkt;
        while (ogg_stream_packetout(&ogg_stream_it->second, &ogg_pkt) == 1) {
            auto stream_it = m_streams.find(serial_number);
            if (stream_it != m_streams.end()) {
                OggPacket packet;
                packet.stream_id = serial_number;
                packet.data.assign(ogg_pkt.packet, ogg_pkt.packet + ogg_pkt.bytes);
                packet.granule_position = static_cast<uint64_t>(ogg_pkt.granulepos);
                packet.is_first_packet = false;
                packet.is_last_packet = (ogg_pkt.e_o_s != 0);
                packet.is_continued = false;
                
                stream_it->second.m_packet_queue.push_back(std::move(packet));
                
                // Update max granule seen
                if (ogg_pkt.granulepos >= 0 &&
                    static_cast<uint64_t>(ogg_pkt.granulepos) > m_max_granule_seen) {
                    m_max_granule_seen = static_cast<uint64_t>(ogg_pkt.granulepos);
                }
            }
        }
    }
    
    // Update page sequence tracking
    auto stream_it = m_streams.find(serial_number);
    if (stream_it != m_streams.end()) {
        uint32_t page_seq = pageSequenceNo(page);
        if (stream_it->second.page_sequence_initialized) {
            if (page_seq != stream_it->second.last_page_sequence + 1) {
                Debug::log("ogg", "handleEOSPage: Page loss detected for serial 0x%08x "
                           "(expected %u, got %u)", serial_number,
                           stream_it->second.last_page_sequence + 1, page_seq);
            }
        }
        stream_it->second.last_page_sequence = page_seq;
    }
    
    // Check if all streams in the group have seen EOS
    if (m_eos_serial_numbers.size() == m_bos_serial_numbers.size()) {
        Debug::log("ogg", "handleEOSPage: All streams in group have reached EOS");
    }
    
    return true;
}

/**
 * @brief Handle a regular data page (not BOS or EOS)
 * 
 * Following RFC 3533 Section 4:
 * - Route page to correct stream state by serial number
 * - Track page sequence for loss detection
 * - Extract packets using ogg_stream_packetout()
 * 
 * @param page Pointer to ogg_page structure
 * @param serial_number Serial number of the stream
 * @return true on success, false on error
 * 
 * Requirements: 3.9, 3.10, 6.1, 6.8
 */
bool OggDemuxer::handleDataPage(ogg_page* page, uint32_t serial_number) {
    // Mark that we've seen a data page (no longer in headers phase)
    if (!m_seen_data_page) {
        m_seen_data_page = true;
        m_in_headers_phase = false;
        Debug::log("ogg", "handleDataPage: First data page seen, exiting headers phase");
    }
    
    // Check if we have a stream state for this serial number
    auto ogg_stream_it = m_ogg_streams.find(serial_number);
    if (ogg_stream_it == m_ogg_streams.end()) {
        Debug::log("ogg", "handleDataPage: Unknown serial 0x%08x, skipping page", serial_number);
        return true;  // Skip unknown streams
    }
    
    // Submit page to stream state using ogg_stream_pagein()
    if (ogg_stream_pagein(&ogg_stream_it->second, page) != 0) {
        Debug::log("ogg", "handleDataPage: ogg_stream_pagein() failed for serial 0x%08x",
                   serial_number);
        return false;
    }
    
    // Track page sequence for loss detection (Requirements 1.6, 6.8)
    auto stream_it = m_streams.find(serial_number);
    if (stream_it != m_streams.end()) {
        OggStream& stream = stream_it->second;
        uint32_t page_seq = pageSequenceNo(page);
        
        if (stream.page_sequence_initialized) {
            if (page_seq != stream.last_page_sequence + 1) {
                Debug::log("ogg", "handleDataPage: Page loss detected for serial 0x%08x "
                           "(expected %u, got %u)", serial_number,
                           stream.last_page_sequence + 1, page_seq);
            }
        }
        stream.last_page_sequence = page_seq;
        stream.page_sequence_initialized = true;
        
        // Extract packets using ogg_stream_packetout()
        ogg_packet ogg_pkt;
        while (ogg_stream_packetout(&ogg_stream_it->second, &ogg_pkt) == 1) {
            OggPacket packet;
            packet.stream_id = serial_number;
            packet.data.assign(ogg_pkt.packet, ogg_pkt.packet + ogg_pkt.bytes);
            packet.granule_position = static_cast<uint64_t>(ogg_pkt.granulepos);
            packet.is_first_packet = (ogg_pkt.b_o_s != 0);
            packet.is_last_packet = (ogg_pkt.e_o_s != 0);
            packet.is_continued = false;  // libogg handles continuation internally
            
            // Check if this is a header packet or data packet
            if (!stream.headers_complete) {
                // Still collecting headers
                stream.header_packets.push_back(std::move(packet));
                
                // Check if we have all expected headers
                if (stream.expected_header_count > 0 &&
                    stream.header_packets.size() >= stream.expected_header_count) {
                    stream.headers_complete = true;
                    Debug::log("ogg", "handleDataPage: Headers complete for serial 0x%08x "
                               "(%zu headers)", serial_number, stream.header_packets.size());
                }
            } else {
                // Data packet - add to queue
                stream.m_packet_queue.push_back(std::move(packet));
                
                // Update max granule seen
                if (ogg_pkt.granulepos >= 0 &&
                    static_cast<uint64_t>(ogg_pkt.granulepos) > m_max_granule_seen) {
                    m_max_granule_seen = static_cast<uint64_t>(ogg_pkt.granulepos);
                }
            }
        }
    }
    
    return true;
}

/**
 * @brief Check if a BOS page indicates a chained stream boundary
 * 
 * Following RFC 3533 Section 4:
 * - Chained streams: EOS immediately followed by BOS
 * - A new BOS after data pages indicates a chained stream boundary
 * 
 * @param page Pointer to ogg_page structure (must be BOS)
 * @param serial_number Serial number of the stream
 * @return true if this is a chained stream boundary
 * 
 * Requirements: 3.8, 3.11
 */
bool OggDemuxer::isChainedStreamBoundary(ogg_page* page, uint32_t serial_number) const {
    // A BOS page is a chained stream boundary if:
    // 1. We've already seen data pages (not in initial headers phase)
    // 2. OR all streams in the current group have seen EOS
    
    if (m_seen_data_page) {
        // We've seen data pages, so a new BOS indicates a chain
        Debug::log("ogg", "isChainedStreamBoundary: BOS after data pages - chain detected");
        return true;
    }
    
    if (!m_eos_serial_numbers.empty() && 
        m_eos_serial_numbers.size() == m_bos_serial_numbers.size()) {
        // All streams have seen EOS, new BOS starts a chain
        Debug::log("ogg", "isChainedStreamBoundary: BOS after all EOS - chain detected");
        return true;
    }
    
    return false;
}

/**
 * @brief Reset multiplexing state for a new file or chain
 * 
 * Clears all multiplexing tracking state to prepare for parsing
 * a new file or handling a new chain in a chained stream.
 */
void OggDemuxer::resetMultiplexingState() {
    m_in_headers_phase = true;
    m_seen_data_page = false;
    m_bos_serial_numbers.clear();
    m_eos_serial_numbers.clear();
    // Don't reset m_chain_count - it tracks total chains seen
    
    Debug::log("ogg", "resetMultiplexingState: Multiplexing state reset");
}

uint64_t OggDemuxer::getLastGranulePosition() {
    return 0;
}

uint64_t OggDemuxer::scanBufferForLastGranule(const std::vector<uint8_t>& buffer, size_t buffer_size) {
    return 0;
}

uint64_t OggDemuxer::scanBackwardForLastGranule(int64_t scan_start, size_t scan_size) {
    return 0;
}

uint64_t OggDemuxer::scanChunkForLastGranule(const std::vector<uint8_t>& buffer, size_t buffer_size, 
                                             uint32_t preferred_serial, bool has_preferred_serial) {
    return 0;
}

uint64_t OggDemuxer::scanForwardForLastGranule(int64_t start_position) {
    return 0;
}

uint64_t OggDemuxer::getLastGranuleFromHeaders() {
    return 0;
}

bool OggDemuxer::seekToPage(uint64_t target_granule, uint32_t stream_id) {
    return false;
}

bool OggDemuxer::examinePacketsAtPosition(int64_t file_offset, uint32_t stream_id, uint64_t& granule_position) {
    return false;
}

/**
 * @brief Fill packet queue for a specific stream
 * 
 * Following libvorbisfile patterns:
 * - Read pages and submit to appropriate stream state
 * - Extract packets using ogg_stream_packetout()
 * - Handle packet continuation across pages
 * 
 * @param target_stream_id Stream ID to fill queue for
 */
void OggDemuxer::fillPacketQueue(uint32_t target_stream_id) {
    std::lock_guard<std::mutex> lock(m_packet_queue_mutex);
    
    // Check if stream exists
    auto stream_it = m_streams.find(target_stream_id);
    if (stream_it == m_streams.end()) {
        Debug::log("ogg", "fillPacketQueue: Stream 0x%08x not found", target_stream_id);
        return;
    }
    
    OggStream& stream = stream_it->second;
    
    // Check queue limits
    if (stream.m_packet_queue.size() >= m_max_packet_queue_size) {
        Debug::log("ogg", "fillPacketQueue: Queue full for stream 0x%08x (%zu packets)",
                   target_stream_id, stream.m_packet_queue.size());
        return;
    }
    
    // Read pages until we have enough packets or hit EOF
    ogg_page page;
    while (stream.m_packet_queue.size() < m_max_packet_queue_size && !m_eof) {
        int ret = getNextPage(&page, -1);
        
        if (ret <= 0) {
            if (ret == 0) {
                m_eof = true;
            }
            break;
        }
        
        // Get serial number of this page
        uint32_t page_serial = pageSerialNo(&page);
        
        // Check if we have a stream state for this serial
        auto ogg_stream_it = m_ogg_streams.find(page_serial);
        if (ogg_stream_it == m_ogg_streams.end()) {
            // New stream - initialize stream state
            ogg_stream_state new_stream;
            if (ogg_stream_init(&new_stream, static_cast<int>(page_serial)) != 0) {
                Debug::log("ogg", "fillPacketQueue: Failed to init stream state for 0x%08x",
                           page_serial);
                continue;
            }
            m_ogg_streams[page_serial] = new_stream;
            ogg_stream_it = m_ogg_streams.find(page_serial);
        }
        
        // Submit page to stream state using ogg_stream_pagein()
        if (ogg_stream_pagein(&ogg_stream_it->second, &page) != 0) {
            Debug::log("ogg", "fillPacketQueue: ogg_stream_pagein() failed for serial 0x%08x",
                       page_serial);
            continue;
        }
        
        // Track page sequence for loss detection
        auto app_stream_it = m_streams.find(page_serial);
        if (app_stream_it != m_streams.end()) {
            OggStream& app_stream = app_stream_it->second;
            uint32_t page_seq = pageSequenceNo(&page);
            
            if (app_stream.page_sequence_initialized) {
                if (page_seq != app_stream.last_page_sequence + 1) {
                    Debug::log("ogg", "Page loss detected: expected seq %u, got %u for stream 0x%08x",
                               app_stream.last_page_sequence + 1, page_seq, page_serial);
                }
            }
            app_stream.last_page_sequence = page_seq;
            app_stream.page_sequence_initialized = true;
        }
        
        // Extract packets from this page using ogg_stream_packetout()
        ogg_packet ogg_pkt;
        while (ogg_stream_packetout(&ogg_stream_it->second, &ogg_pkt) == 1) {
            // Only queue packets for the target stream
            if (page_serial == target_stream_id) {
                OggPacket packet;
                packet.stream_id = page_serial;
                packet.data.assign(ogg_pkt.packet, ogg_pkt.packet + ogg_pkt.bytes);
                packet.granule_position = static_cast<uint64_t>(ogg_pkt.granulepos);
                packet.is_first_packet = (ogg_pkt.b_o_s != 0);
                packet.is_last_packet = (ogg_pkt.e_o_s != 0);
                packet.is_continued = false;  // libogg handles continuation internally
                
                stream.m_packet_queue.push_back(std::move(packet));
                
                Debug::log("ogg", "Queued packet: stream 0x%08x, size %zu, granule %lld",
                           page_serial, ogg_pkt.bytes,
                           static_cast<long long>(ogg_pkt.granulepos));
                
                // Update max granule seen
                if (ogg_pkt.granulepos >= 0 && 
                    static_cast<uint64_t>(ogg_pkt.granulepos) > m_max_granule_seen) {
                    m_max_granule_seen = static_cast<uint64_t>(ogg_pkt.granulepos);
                }
            }
        }
    }
}

/**
 * @brief Fetch and process a single packet
 * 
 * Following libvorbisfile _fetch_and_process_packet() pattern:
 * - Get next page if needed
 * - Submit to stream state
 * - Extract packet
 * 
 * @return 1 on success, 0 on EOF, -1 on error, -2 on hole (missing data)
 */
int OggDemuxer::fetchAndProcessPacket() {
    ogg_page page;
    ogg_packet packet;
    
    while (true) {
        // Try to get a packet from any active stream
        for (auto& [serial, ogg_stream] : m_ogg_streams) {
            int ret = ogg_stream_packetout(&ogg_stream, &packet);
            
            if (ret == 1) {
                // Got a packet
                auto stream_it = m_streams.find(serial);
                if (stream_it != m_streams.end()) {
                    OggPacket ogg_pkt;
                    ogg_pkt.stream_id = serial;
                    ogg_pkt.data.assign(packet.packet, packet.packet + packet.bytes);
                    ogg_pkt.granule_position = static_cast<uint64_t>(packet.granulepos);
                    ogg_pkt.is_first_packet = (packet.b_o_s != 0);
                    ogg_pkt.is_last_packet = (packet.e_o_s != 0);
                    ogg_pkt.is_continued = false;
                    
                    std::lock_guard<std::mutex> lock(m_packet_queue_mutex);
                    stream_it->second.m_packet_queue.push_back(std::move(ogg_pkt));
                }
                return 1;
            }
            
            if (ret == -1) {
                // Hole in data (missing packets)
                Debug::log("ogg", "fetchAndProcessPacket: Hole detected in stream 0x%08x", serial);
                return -2;
            }
            
            // ret == 0: Need more data
        }
        
        // Need to read another page
        int page_ret = getNextPage(&page, -1);
        
        if (page_ret == 0) {
            // EOF
            return 0;
        }
        
        if (page_ret < 0) {
            // Error
            return -1;
        }
        
        // Submit page to appropriate stream
        uint32_t page_serial = pageSerialNo(&page);
        auto ogg_stream_it = m_ogg_streams.find(page_serial);
        
        if (ogg_stream_it == m_ogg_streams.end()) {
            // New stream - initialize
            ogg_stream_state new_stream;
            if (ogg_stream_init(&new_stream, static_cast<int>(page_serial)) != 0) {
                Debug::log("ogg", "fetchAndProcessPacket: Failed to init stream 0x%08x",
                           page_serial);
                continue;
            }
            m_ogg_streams[page_serial] = new_stream;
            ogg_stream_it = m_ogg_streams.find(page_serial);
        }
        
        // Submit page using ogg_stream_pagein()
        if (ogg_stream_pagein(&ogg_stream_it->second, &page) != 0) {
            Debug::log("ogg", "fetchAndProcessPacket: ogg_stream_pagein() failed");
            continue;
        }
    }
}

int OggDemuxer::granposAdd(int64_t* dst_gp, int64_t src_gp, int32_t delta) {
    return 0;
}

int OggDemuxer::granposDiff(int64_t* delta, int64_t gp_a, int64_t gp_b) {
    return 0;
}

int OggDemuxer::granposCmp(int64_t gp_a, int64_t gp_b) {
    return 0;
}

/**
 * @brief Get data from IOHandler into ogg_sync_state buffer
 * 
 * Following libvorbisfile _get_data() pattern:
 * - Request buffer from ogg_sync_buffer()
 * - Read data from IOHandler
 * - Report bytes read via ogg_sync_wrote()
 * 
 * @param bytes_requested Number of bytes to request (0 = use READSIZE)
 * @return Number of bytes read, 0 on EOF, -1 on error
 */
int OggDemuxer::getData(size_t bytes_requested) {
    if (bytes_requested == 0) {
        bytes_requested = READSIZE;
    }
    
    // Request buffer from libogg sync state
    char* buffer = ogg_sync_buffer(&m_sync_state, static_cast<long>(bytes_requested));
    if (!buffer) {
        Debug::log("ogg", "ogg_sync_buffer() failed to allocate %zu bytes", bytes_requested);
        return -1;
    }
    
    // Read data from IOHandler
    if (!m_handler) {
        Debug::log("ogg", "getData: No IOHandler available");
        return -1;
    }
    
    size_t bytes_read = m_handler->read(buffer, 1, bytes_requested);
    
    // Report bytes read to libogg
    if (ogg_sync_wrote(&m_sync_state, static_cast<long>(bytes_read)) != 0) {
        Debug::log("ogg", "ogg_sync_wrote() failed for %zu bytes", bytes_read);
        return -1;
    }
    
    // Track total bytes read for performance monitoring
    m_bytes_read_total += bytes_read;
    
    if (bytes_read == 0) {
        // EOF reached
        m_eof = true;
        return 0;
    }
    
    return static_cast<int>(bytes_read);
}

/**
 * @brief Get next Ogg page from stream using ogg_sync_pageseek()
 * 
 * Following libvorbisfile _get_next_page() pattern:
 * - Use ogg_sync_pageseek() for page discovery (NOT ogg_sync_pageout())
 * - Handle negative returns (skipped bytes) for error recovery
 * - Request more data when needed
 * 
 * @param page Output: Pointer to ogg_page structure to fill
 * @param boundary Maximum bytes to search (-1 = no limit)
 * @return Bytes consumed on success, 0 on EOF, -1 on error, -2 on boundary reached
 */
int OggDemuxer::getNextPage(ogg_page* page, int64_t boundary) {
    if (!page) {
        return -1;
    }
    
    int64_t bytes_searched = 0;
    
    while (boundary < 0 || bytes_searched < boundary) {
        // Use ogg_sync_pageseek() for page discovery
        // This is the correct function per libvorbisfile patterns
        long ret = ogg_sync_pageseek(&m_sync_state, page);
        
        if (ret > 0) {
            // Page found successfully
            // ret is the number of bytes consumed (page size)
            m_offset += ret;
            
            Debug::log("ogg", "getNextPage: Found page at offset %lld, size %ld, "
                       "serial 0x%08x, granule %lld",
                       static_cast<long long>(m_offset - ret),
                       ret,
                       pageSerialNo(page),
                       static_cast<long long>(pageGranulePos(page)));
            
            return static_cast<int>(ret);
        }
        
        if (ret < 0) {
            // Negative return: skipped bytes (corrupted data or sync loss)
            // The absolute value is the number of bytes skipped
            long skipped = -ret;
            m_offset += skipped;
            bytes_searched += skipped;
            
            Debug::log("ogg", "getNextPage: Skipped %ld bytes (sync loss/corruption)", skipped);
            
            // Continue searching for next valid page
            continue;
        }
        
        // ret == 0: Need more data
        // Check if we've hit the boundary
        if (boundary >= 0 && bytes_searched >= boundary) {
            Debug::log("ogg", "getNextPage: Boundary reached after %lld bytes",
                       static_cast<long long>(bytes_searched));
            return -2;  // Boundary reached
        }
        
        // Request more data from IOHandler
        int data_ret = getData(READSIZE);
        
        if (data_ret < 0) {
            Debug::log("ogg", "getNextPage: getData() failed");
            return -1;  // Error
        }
        
        if (data_ret == 0) {
            Debug::log("ogg", "getNextPage: EOF reached");
            return 0;  // EOF
        }
        
        bytes_searched += data_ret;
    }
    
    // Boundary reached without finding page
    return -2;
}

/**
 * @brief Get previous page by scanning backwards
 * 
 * Following libvorbisfile _get_prev_page() pattern:
 * - Use chunk-based backward scanning with CHUNKSIZE increments
 * - Seek backwards, read forward to find pages
 * - Return the last complete page found in the chunk
 * 
 * @param page Output: Pointer to ogg_page structure to fill
 * @return Offset of page found, -1 on error
 */
int OggDemuxer::getPrevPage(ogg_page* page) {
    if (!page || !m_handler) {
        return -1;
    }
    
    int64_t begin = m_offset;
    int64_t end = begin;
    int64_t ret = -1;
    int64_t offset = -1;
    
    while (offset == -1) {
        // Move begin backwards by CHUNKSIZE
        begin -= CHUNKSIZE;
        if (begin < 0) {
            begin = 0;
        }
        
        // Seek to begin position
        if (!m_handler->seek(begin, SEEK_SET)) {
            Debug::log("ogg", "getPrevPage: Seek to %lld failed", 
                       static_cast<long long>(begin));
            return -1;
        }
        
        // Reset sync state for fresh scanning
        ogg_sync_reset(&m_sync_state);
        m_offset = begin;
        
        // Scan forward to find pages
        while (m_offset < end) {
            ret = getNextPage(page, end - m_offset);
            
            if (ret < 0) {
                // Error or boundary reached
                break;
            }
            
            if (ret == 0) {
                // EOF - shouldn't happen in middle of file
                break;
            }
            
            // Found a page - remember its offset
            offset = m_offset - ret;
        }
        
        // If we've searched back to beginning and found nothing, fail
        if (begin == 0 && offset == -1) {
            Debug::log("ogg", "getPrevPage: No page found searching back to beginning");
            return -1;
        }
    }
    
    // Seek back to the found page and re-read it
    if (!m_handler->seek(offset, SEEK_SET)) {
        Debug::log("ogg", "getPrevPage: Final seek to %lld failed",
                   static_cast<long long>(offset));
        return -1;
    }
    
    ogg_sync_reset(&m_sync_state);
    m_offset = offset;
    
    ret = getNextPage(page, CHUNKSIZE);
    if (ret <= 0) {
        Debug::log("ogg", "getPrevPage: Failed to re-read page at offset %lld",
                   static_cast<long long>(offset));
        return -1;
    }
    
    Debug::log("ogg", "getPrevPage: Found page at offset %lld",
               static_cast<long long>(offset));
    
    return static_cast<int>(offset);
}

/**
 * @brief Get previous page with specific serial number
 * 
 * Following libopusfile _get_prev_page_serial() pattern:
 * - Scan backwards looking for pages with matching serial number
 * - Skip pages from other streams
 * 
 * @param page Output: Pointer to ogg_page structure to fill
 * @param serial_number Serial number to match
 * @return Offset of page found, -1 on error
 */
int OggDemuxer::getPrevPageSerial(ogg_page* page, uint32_t serial_number) {
    if (!page || !m_handler) {
        return -1;
    }
    
    int64_t begin = m_offset;
    int64_t end = begin;
    int64_t best_offset = -1;
    
    while (best_offset == -1) {
        // Move begin backwards by CHUNKSIZE
        begin -= CHUNKSIZE;
        if (begin < 0) {
            begin = 0;
        }
        
        // Seek to begin position
        if (!m_handler->seek(begin, SEEK_SET)) {
            Debug::log("ogg", "getPrevPageSerial: Seek to %lld failed",
                       static_cast<long long>(begin));
            return -1;
        }
        
        // Reset sync state for fresh scanning
        ogg_sync_reset(&m_sync_state);
        m_offset = begin;
        
        // Scan forward to find pages with matching serial
        while (m_offset < end) {
            int ret = getNextPage(page, end - m_offset);
            
            if (ret <= 0) {
                break;
            }
            
            // Check if this page has the serial number we want
            if (pageSerialNo(page) == serial_number) {
                best_offset = m_offset - ret;
            }
        }
        
        // If we've searched back to beginning and found nothing, fail
        if (begin == 0 && best_offset == -1) {
            Debug::log("ogg", "getPrevPageSerial: No page with serial 0x%08x found",
                       serial_number);
            return -1;
        }
    }
    
    // Seek back to the found page and re-read it
    if (!m_handler->seek(best_offset, SEEK_SET)) {
        Debug::log("ogg", "getPrevPageSerial: Final seek to %lld failed",
                   static_cast<long long>(best_offset));
        return -1;
    }
    
    ogg_sync_reset(&m_sync_state);
    m_offset = best_offset;
    
    int ret = getNextPage(page, CHUNKSIZE);
    if (ret <= 0) {
        Debug::log("ogg", "getPrevPageSerial: Failed to re-read page at offset %lld",
                   static_cast<long long>(best_offset));
        return -1;
    }
    
    Debug::log("ogg", "getPrevPageSerial: Found page with serial 0x%08x at offset %lld",
               serial_number, static_cast<long long>(best_offset));
    
    return static_cast<int>(best_offset);
}

/**
 * @brief Clean up all libogg structures
 * 
 * Must be called with appropriate locks held.
 */
void OggDemuxer::cleanupLiboggStructures_unlocked() {
    // Clear all stream states
    for (auto& [serial, ogg_stream] : m_ogg_streams) {
        ogg_stream_clear(&ogg_stream);
    }
    m_ogg_streams.clear();
    
    // Reset sync state
    ogg_sync_reset(&m_sync_state);
    
    Debug::log("ogg", "cleanupLiboggStructures_unlocked: Cleaned up all libogg structures");
}

/**
 * @brief Validate buffer size against memory limits
 * 
 * Implements bounds checking to prevent memory exhaustion per Requirements 2.9.
 * 
 * @param requested_size Size being requested
 * @param operation_name Name of operation for logging
 * @return true if size is acceptable, false if it would exceed limits
 */
bool OggDemuxer::validateBufferSize(size_t requested_size, const char* operation_name) {
    // Check against maximum page size (RFC 3533 limit)
    if (requested_size > OGG_PAGE_SIZE_MAX) {
        Debug::log("ogg", "%s: Requested size %zu exceeds max page size %zu",
                   operation_name, requested_size, OGG_PAGE_SIZE_MAX);
        return false;
    }
    
    // Check against total memory limit
    size_t current_usage = m_total_memory_usage.load();
    if (current_usage + requested_size > m_max_memory_usage) {
        Debug::log("ogg", "%s: Would exceed memory limit (current: %zu, requested: %zu, max: %zu)",
                   operation_name, current_usage, requested_size, m_max_memory_usage);
        return false;
    }
    
    return true;
}

/**
 * @brief Enforce packet queue size limits
 * 
 * Implements bounded queues to prevent memory exhaustion per Requirements 10.2.
 * Removes oldest packets when queue exceeds limit.
 * 
 * @param stream_id Stream to enforce limits on
 * @return true if queue is within limits, false if packets were dropped
 */
bool OggDemuxer::enforcePacketQueueLimits_unlocked(uint32_t stream_id) {
    auto stream_it = m_streams.find(stream_id);
    if (stream_it == m_streams.end()) {
        return true;
    }
    
    OggStream& stream = stream_it->second;
    bool dropped = false;
    
    while (stream.m_packet_queue.size() > m_max_packet_queue_size) {
        // Remove oldest packet
        if (!stream.m_packet_queue.empty()) {
            size_t packet_size = stream.m_packet_queue.front().data.size();
            stream.m_packet_queue.pop_front();
            
            // Update memory tracking
            if (m_total_memory_usage >= packet_size) {
                m_total_memory_usage -= packet_size;
            }
            
            dropped = true;
        }
    }
    
    if (dropped) {
        Debug::log("ogg", "enforcePacketQueueLimits: Dropped packets from stream 0x%08x "
                   "(queue size now %zu)", stream_id, stream.m_packet_queue.size());
    }
    
    return !dropped;
}

/**
 * @brief Reset sync state after a seek operation
 * 
 * Following libvorbisfile patterns:
 * - Use ogg_sync_reset() to clear buffered data
 * - Reset internal offset tracking
 */
void OggDemuxer::resetSyncStateAfterSeek_unlocked() {
    ogg_sync_reset(&m_sync_state);
    
    // Clear any partial packet data in stream states
    for (auto& [serial, ogg_stream] : m_ogg_streams) {
        ogg_stream_reset(&ogg_stream);
    }
    
    Debug::log("ogg", "resetSyncStateAfterSeek_unlocked: Reset sync and stream states");
}

/**
 * @brief Reset stream state for a specific stream
 * 
 * Following libvorbisfile patterns:
 * - Use ogg_stream_reset_serialno() for stream switching
 * - Clear packet queue
 * 
 * @param stream_id Stream to reset
 * @param new_serial_number New serial number (or same to just reset)
 */
void OggDemuxer::resetStreamState_unlocked(uint32_t stream_id, uint32_t new_serial_number) {
    auto ogg_stream_it = m_ogg_streams.find(stream_id);
    if (ogg_stream_it != m_ogg_streams.end()) {
        // Use ogg_stream_reset_serialno() per Requirements 14.10
        ogg_stream_reset_serialno(&ogg_stream_it->second, static_cast<int>(new_serial_number));
    }
    
    // Clear application-level stream state
    auto stream_it = m_streams.find(stream_id);
    if (stream_it != m_streams.end()) {
        OggStream& stream = stream_it->second;
        
        // Update memory tracking before clearing queue
        for (const auto& packet : stream.m_packet_queue) {
            if (m_total_memory_usage >= packet.data.size()) {
                m_total_memory_usage -= packet.data.size();
            }
        }
        
        stream.m_packet_queue.clear();
        stream.page_sequence_initialized = false;
    }
    
    Debug::log("ogg", "resetStreamState_unlocked: Reset stream 0x%08x with serial 0x%08x",
               stream_id, new_serial_number);
}

bool OggDemuxer::performMemoryAudit_unlocked() {
    return true;
}

void OggDemuxer::enforceMemoryLimits_unlocked() {
}

bool OggDemuxer::validateLiboggStructures_unlocked() {
    return true;
}

void OggDemuxer::performPeriodicMaintenance_unlocked() {
}

bool OggDemuxer::detectInfiniteLoop_unlocked(const std::string& operation_name) {
    return false;
}

bool OggDemuxer::performReadAheadBuffering_unlocked(size_t target_buffer_size) {
    return true;
}

void OggDemuxer::cachePageForSeeking_unlocked(int64_t file_offset, uint64_t granule_position, 
                                              uint32_t stream_id, const std::vector<uint8_t>& page_data) {
}

bool OggDemuxer::findCachedPageNearTarget_unlocked(uint64_t target_granule, uint32_t stream_id, 
                                                   int64_t& file_offset, uint64_t& granule_position) {
    return false;
}

void OggDemuxer::addSeekHint_unlocked(uint64_t timestamp_ms, int64_t file_offset, uint64_t granule_position) {
}

bool OggDemuxer::findBestSeekHint_unlocked(uint64_t target_timestamp_ms, int64_t& file_offset, 
                                           uint64_t& granule_position) {
    return false;
}

bool OggDemuxer::optimizedRead_unlocked(void* buffer, size_t size, size_t count, long& bytes_read) {
    return false;
}

bool OggDemuxer::processPacketWithMinimalCopy_unlocked(const ogg_packet& packet, uint32_t stream_id, 
                                                       OggPacket& output_packet) {
    return false;
}

void OggDemuxer::cleanupPerformanceCaches_unlocked() {
}

bool OggDemuxer::skipToNextValidSection() const {
    return false;
}

bool OggDemuxer::resetInternalState() const {
    return false;
}

bool OggDemuxer::enableFallbackMode() const {
    return false;
}

/**
 * @brief Check if data starts with a specific signature
 * 
 * @param data Data buffer to check
 * @param signature Null-terminated signature string to match
 * @return true if data starts with signature
 */
bool OggDemuxer::hasSignature(const std::vector<uint8_t>& data, const char* signature) {
    if (!signature || data.empty()) {
        return false;
    }
    
    size_t sig_len = std::strlen(signature);
    if (data.size() < sig_len) {
        return false;
    }
    
    return std::memcmp(data.data(), signature, sig_len) == 0;
}

bool OggDemuxer::validateOggPage(const ogg_page* page) {
    return page != nullptr && page->header != nullptr && page->body != nullptr;
}

bool OggDemuxer::validateOggPacket(const ogg_packet* packet, uint32_t stream_id) {
    return packet != nullptr && packet->packet != nullptr && packet->bytes >= 0;
}

int OggDemuxer::getOpusPacketSampleCount(const OggPacket& packet) {
    return 0;
}

int OggDemuxer::getVorbisPacketSampleCount(const OggPacket& packet) {
    return 0;
}

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

#endif // HAVE_OGGDEMUXER
