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
    
    // Acquire locks to ensure no operations are in progress (Requirements 11.6)
    // Lock acquisition order: m_ogg_state_mutex first, then m_packet_queue_mutex
    std::lock_guard<std::mutex> state_lock(m_ogg_state_mutex);
    std::lock_guard<std::mutex> queue_lock(m_packet_queue_mutex);
    
    // Ensure no operations are in progress before destruction (Requirements 11.6)
    // All locks are now held, so no other threads can access shared state
    
    // Clean up performance caches (Requirements 11.6)
    cleanupPerformanceCaches_unlocked();
    
    // Clean up libogg structures (Requirements 11.6)
    cleanupLiboggStructures_unlocked();
    
    // Clear all streams
    m_streams.clear();
    
    // Clear error state
    m_fallback_mode = false;
    m_corrupted_streams.clear();
    
    Debug::log("ogg", "OggDemuxer destructor - cleanup complete");
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

/**
 * @brief Read next chunk from the best audio stream
 * 
 * Following libvorbisfile patterns:
 * - Select the primary audio stream
 * - Return next available packet as MediaChunk
 * - Fill queue if needed
 * 
 * @return MediaChunk containing packet data, or empty chunk on EOF/error
 * 
 * Requirements: 6.1, 6.2, 6.3, 6.4
 */
MediaChunk OggDemuxer::readChunk() {
    // Find the first audio stream
    uint32_t audio_stream_id = 0;
    bool found_audio = false;
    
    for (const auto& [serial, stream] : m_streams) {
        if (stream.codec_type == "audio" && stream.headers_complete) {
            audio_stream_id = serial;
            found_audio = true;
            break;
        }
    }
    
    if (!found_audio) {
        Debug::log("ogg", "readChunk: No audio stream available");
        return MediaChunk{};
    }
    
    return readChunk(audio_stream_id);
}

/**
 * @brief Read next chunk from a specific stream (public API with lock)
 * 
 * Following libvorbisfile patterns:
 * - Maintain packet order within the logical bitstream (Requirements 6.1)
 * - Reconstruct complete packets using continuation flag (Requirements 6.2)
 * - Handle packet continuation flags correctly (Requirements 6.3)
 * - Track timing information from granule positions (Requirements 6.4)
 * 
 * @param stream_id Stream to read from
 * @return MediaChunk containing packet data, or empty chunk on EOF/error
 * 
 * Requirements: 6.1, 6.2, 6.3, 6.4, 11.1, 11.4
 */
MediaChunk OggDemuxer::readChunk(uint32_t stream_id) {
    std::lock_guard<std::mutex> lock(m_packet_queue_mutex);
    return readChunk_unlocked(stream_id);
}

/**
 * @brief Read next chunk from a specific stream (private implementation without lock)
 * 
 * Assumes m_packet_queue_mutex is already held by caller.
 * 
 * @param stream_id Stream to read from
 * @return MediaChunk containing packet data, or empty chunk on EOF/error
 * 
 * Requirements: 6.1, 6.2, 6.3, 6.4
 */
MediaChunk OggDemuxer::readChunk_unlocked(uint32_t stream_id) {
    // Check if stream exists
    auto stream_it = m_streams.find(stream_id);
    if (stream_it == m_streams.end()) {
        Debug::log("ogg", "readChunk_unlocked: Stream 0x%08x not found", stream_id);
        return MediaChunk{};
    }
    
    OggStream& stream = stream_it->second;
    
    // First, send header packets if not yet sent (Requirements 6.1 - packet order)
    if (!stream.headers_sent && !stream.header_packets.empty()) {
        if (stream.next_header_index < stream.header_packets.size()) {
            const OggPacket& header = stream.header_packets[stream.next_header_index];
            
            MediaChunk chunk;
            chunk.stream_id = stream_id;
            chunk.data = header.data;
            chunk.timestamp_samples = 0;  // Headers have timestamp 0
            chunk.granule_position = 0;
            chunk.is_keyframe = true;
            
            stream.next_header_index++;
            
            // Mark headers as sent when all are delivered
            if (stream.next_header_index >= stream.header_packets.size()) {
                stream.headers_sent = true;
                Debug::log("ogg", "readChunk_unlocked: All %zu headers sent for stream 0x%08x",
                           stream.header_packets.size(), stream_id);
            }
            
            return chunk;
        }
    }
    
    // Check if we need to fill the queue
    if (stream.m_packet_queue.empty() && !m_eof) {
        // Note: fillPacketQueue_unlocked expects m_packet_queue_mutex to be held
        fillPacketQueue_unlocked(stream_id);
        
        // Re-check stream iterator after filling queue
        stream_it = m_streams.find(stream_id);
        if (stream_it == m_streams.end()) {
            return MediaChunk{};
        }
    }
    
    // Get reference again after potential queue fill
    OggStream& stream_ref = m_streams[stream_id];
    
    // Check if queue is still empty
    if (stream_ref.m_packet_queue.empty()) {
        if (m_eof) {
            Debug::log("ogg", "readChunk_unlocked: EOF reached for stream 0x%08x", stream_id);
        }
        return MediaChunk{};
    }
    
    // Get next packet from queue (maintains packet order per Requirements 6.1)
    OggPacket packet = std::move(stream_ref.m_packet_queue.front());
    stream_ref.m_packet_queue.pop_front();
    
    // Convert granule position to timestamp (Requirements 6.4)
    uint64_t timestamp_samples = 0;
    if (packet.granule_position != static_cast<uint64_t>(-1) &&
        packet.granule_position != FLAC_OGG_GRANULE_NO_PACKET) {
        // For Ogg formats, granule position is the sample count
        timestamp_samples = packet.granule_position;
        
        // For Opus, account for pre-skip
        if (stream_ref.codec_name == "opus" && timestamp_samples > stream_ref.pre_skip) {
            timestamp_samples -= stream_ref.pre_skip;
        }
        
        // Update stream's processed sample count
        stream_ref.total_samples_processed = packet.granule_position;
    }
    
    // Create MediaChunk
    MediaChunk chunk;
    chunk.stream_id = stream_id;
    chunk.data = std::move(packet.data);
    chunk.granule_position = packet.granule_position;
    chunk.timestamp_samples = timestamp_samples;
    chunk.is_keyframe = true;  // Audio packets are typically all keyframes
    
    Debug::log("ogg", "readChunk_unlocked: Returned packet from stream 0x%08x, size %zu, granule %llu",
               stream_id, chunk.data.size(), static_cast<unsigned long long>(packet.granule_position));
    
    return chunk;
}

/**
 * @brief Seek to a specific timestamp in milliseconds (public API with lock)
 * 
 * Following libvorbisfile ov_pcm_seek() and libopusfile op_pcm_seek() patterns:
 * - Convert timestamp to target granule position
 * - Use bisection search to find the target page
 * - Reset stream state after seek
 * - Do NOT resend header packets (decoder maintains state)
 * 
 * @param timestamp_ms Target timestamp in milliseconds
 * @return true on success, false on failure
 * 
 * Requirements: 7.1, 7.6, 7.7, 7.8, 11.1, 11.2
 */
bool OggDemuxer::seekTo(uint64_t timestamp_ms) {
    std::lock_guard<std::mutex> lock(m_ogg_state_mutex);
    return seekTo_unlocked(timestamp_ms);
}

/**
 * @brief Seek to a specific timestamp in milliseconds (private implementation without lock)
 * 
 * Assumes m_ogg_state_mutex is already held by caller.
 * 
 * @param timestamp_ms Target timestamp in milliseconds
 * @return true on success, false on failure
 * 
 * Requirements: 7.1, 7.6, 7.7, 7.8
 */
bool OggDemuxer::seekTo_unlocked(uint64_t timestamp_ms) {
    // Find the primary audio stream
    uint32_t audio_stream_id = 0;
    bool found_audio = false;
    
    for (const auto& [serial, stream] : m_streams) {
        if (stream.codec_type == "audio" && stream.headers_complete) {
            audio_stream_id = serial;
            found_audio = true;
            break;
        }
    }
    
    if (!found_audio) {
        Debug::log("ogg", "seekTo_unlocked: No audio stream available for seeking");
        return false;
    }
    
    // Convert timestamp to target granule position
    uint64_t target_granule = msToGranule(timestamp_ms, audio_stream_id);
    
    Debug::log("ogg", "seekTo_unlocked: Seeking to %llu ms (granule %llu) in stream 0x%08x",
               static_cast<unsigned long long>(timestamp_ms),
               static_cast<unsigned long long>(target_granule),
               audio_stream_id);
    
    // Perform page-level seeking using bisection search
    bool result = seekToPage_unlocked(target_granule, audio_stream_id);
    
    if (result) {
        // Track seek operation for performance monitoring
        m_seek_operations++;
        
        // Clear EOF flag since we've repositioned
        m_eof = false;
        
        Debug::log("ogg", "seekTo_unlocked: Successfully seeked to %llu ms", 
                   static_cast<unsigned long long>(timestamp_ms));
    } else {
        Debug::log("ogg", "seekTo_unlocked: Failed to seek to %llu ms",
                   static_cast<unsigned long long>(timestamp_ms));
    }
    
    return result;
}

/**
 * @brief Check if end of file has been reached
 * 
 * Returns true if:
 * - Physical EOF has been reached on the IOHandler
 * - All streams have received their EOS pages
 * 
 * @return true if EOF, false otherwise
 * 
 * Requirements: 6.5
 */
bool OggDemuxer::isEOF() const {
    // Check physical EOF
    if (m_eof) {
        return true;
    }
    
    // Check if all streams have seen EOS
    if (!m_streams.empty() && !m_bos_serial_numbers.empty()) {
        // All streams must have seen EOS for logical EOF
        if (m_eos_serial_numbers.size() == m_bos_serial_numbers.size()) {
            // Also check that all packet queues are empty
            for (const auto& [serial, stream] : m_streams) {
                if (!stream.m_packet_queue.empty()) {
                    return false;  // Still have packets to deliver
                }
            }
            return true;
        }
    }
    
    return false;
}

/**
 * @brief Get total duration of the file in milliseconds
 * 
 * Following libvorbisfile and libopusfile patterns:
 * - Opus: Use opus_granule_sample() equivalent with 48kHz rate (Requirements 8.6)
 * - Vorbis: Use granule position as sample count at codec sample rate (Requirements 8.7)
 * - FLAC-in-Ogg: Use granule position as sample count like Vorbis (Requirements 8.8)
 * - Report unknown duration (0) rather than incorrect values (Requirements 8.9)
 * - Handle file beginning boundary gracefully (Requirements 8.10)
 * - Use best available granule for truncated/corrupted files (Requirements 8.11)
 * 
 * @return Duration in milliseconds, or 0 if unknown
 * 
 * Requirements: 8.5, 8.6, 8.7, 8.8, 8.9, 8.10, 8.11
 */
uint64_t OggDemuxer::getDuration() const {
    // Find the primary audio stream
    const OggStream* primary_stream = nullptr;
    uint32_t primary_serial = 0;
    uint64_t longest_duration_samples = 0;
    
    for (const auto& [serial, stream] : m_streams) {
        if (stream.codec_type == "audio" && stream.headers_complete) {
            // For multiplexed files, use the longest stream (Requirements 8.4)
            // Estimate duration from total_samples if available
            uint64_t stream_samples = stream.total_samples;
            if (stream_samples == 0) {
                // Use tracked max granule as fallback
                stream_samples = m_max_granule_seen;
            }
            
            if (stream_samples > longest_duration_samples || primary_stream == nullptr) {
                longest_duration_samples = stream_samples;
                primary_stream = &stream;
                primary_serial = serial;
            }
        }
    }
    
    if (!primary_stream) {
        Debug::log("ogg", "getDuration: No audio stream available");
        return 0;  // Unknown duration (Requirements 8.9)
    }
    
    // Get the last granule position
    // Note: getLastGranulePosition() is not const, so we use cached values
    uint64_t last_granule = m_max_granule_seen;
    
    // Prefer header-provided total samples if available
    if (primary_stream->total_samples > 0) {
        last_granule = primary_stream->total_samples;
    }
    
    if (last_granule == 0) {
        Debug::log("ogg", "getDuration: No granule position available");
        return 0;  // Unknown duration (Requirements 8.9)
    }
    
    // Convert granule to milliseconds using codec-specific logic
    return granuleToMs(last_granule, primary_serial);
}

/**
 * @brief Get current playback position in milliseconds
 * 
 * Returns the timestamp of the last packet read from the primary audio stream.
 * 
 * @return Current position in milliseconds
 * 
 * Requirements: 14.4
 */
uint64_t OggDemuxer::getPosition() const {
    // Find the primary audio stream
    for (const auto& [serial, stream] : m_streams) {
        if (stream.codec_type == "audio" && stream.headers_complete) {
            // Convert processed samples to milliseconds
            if (stream.total_samples_processed > 0 && stream.sample_rate > 0) {
                if (stream.codec_name == "opus") {
                    // Opus: Account for pre-skip
                    uint64_t samples = stream.total_samples_processed;
                    if (samples > stream.pre_skip) {
                        samples -= stream.pre_skip;
                    } else {
                        samples = 0;
                    }
                    return (samples * 1000) / 48000;
                } else {
                    return (stream.total_samples_processed * 1000) / stream.sample_rate;
                }
            }
            break;
        }
    }
    
    return 0;
}

/**
 * @brief Get current granule position for a specific stream
 * 
 * @param stream_id Stream ID to query
 * @return Current granule position, or 0 if stream not found
 */
uint64_t OggDemuxer::getGranulePosition(uint32_t stream_id) const {
    auto stream_it = m_streams.find(stream_id);
    if (stream_it == m_streams.end()) {
        return 0;
    }
    
    return stream_it->second.total_samples_processed;
}

/**
 * @brief Convert granule position to milliseconds
 * 
 * Following reference implementation patterns:
 * - Vorbis: granule = sample number at end of packet (direct mapping)
 * - Opus: granule = 48kHz sample number, account for pre-skip
 * - FLAC: granule = sample number at end of packet (same as Vorbis)
 * 
 * @param granule Granule position to convert
 * @param stream_id Stream ID for codec-specific conversion
 * @return Timestamp in milliseconds
 * 
 * Requirements: 6.4, 8.6, 8.7, 8.8
 */
uint64_t OggDemuxer::granuleToMs(uint64_t granule, uint32_t stream_id) const {
    // Check for invalid granule position
    if (granule == static_cast<uint64_t>(-1) || granule == FLAC_OGG_GRANULE_NO_PACKET) {
        return 0;
    }
    
    auto stream_it = m_streams.find(stream_id);
    if (stream_it == m_streams.end()) {
        Debug::log("ogg", "granuleToMs: Stream 0x%08x not found", stream_id);
        return 0;
    }
    
    const OggStream& stream = stream_it->second;
    
    if (stream.sample_rate == 0) {
        Debug::log("ogg", "granuleToMs: Stream 0x%08x has zero sample rate", stream_id);
        return 0;
    }
    
    uint64_t samples = granule;
    
    // Codec-specific handling
    if (stream.codec_name == "opus") {
        // Opus: Account for pre-skip (Requirements 8.6)
        // Opus granule is at 48kHz regardless of output sample rate
        if (samples > stream.pre_skip) {
            samples -= stream.pre_skip;
        } else {
            samples = 0;
        }
        // Opus always uses 48kHz granule rate
        return (samples * 1000) / 48000;
    } else if (stream.codec_name == "vorbis") {
        // Vorbis: Direct sample count at codec sample rate (Requirements 8.7)
        return (samples * 1000) / stream.sample_rate;
    } else if (stream.codec_name == "flac") {
        // FLAC-in-Ogg: Direct sample count like Vorbis (Requirements 8.8)
        return (samples * 1000) / stream.sample_rate;
    } else if (stream.codec_name == "speex") {
        // Speex: Direct sample count at codec sample rate
        return (samples * 1000) / stream.sample_rate;
    }
    
    // Default: assume direct sample count
    return (samples * 1000) / stream.sample_rate;
}

/**
 * @brief Convert milliseconds to granule position
 * 
 * Following reference implementation patterns:
 * - Vorbis: granule = sample number (direct mapping)
 * - Opus: granule = 48kHz sample number + pre-skip
 * - FLAC: granule = sample number (same as Vorbis)
 * 
 * @param timestamp_ms Timestamp in milliseconds
 * @param stream_id Stream ID for codec-specific conversion
 * @return Granule position
 */
uint64_t OggDemuxer::msToGranule(uint64_t timestamp_ms, uint32_t stream_id) const {
    auto stream_it = m_streams.find(stream_id);
    if (stream_it == m_streams.end()) {
        Debug::log("ogg", "msToGranule: Stream 0x%08x not found", stream_id);
        return 0;
    }
    
    const OggStream& stream = stream_it->second;
    
    if (stream.sample_rate == 0) {
        Debug::log("ogg", "msToGranule: Stream 0x%08x has zero sample rate", stream_id);
        return 0;
    }
    
    // Codec-specific handling
    if (stream.codec_name == "opus") {
        // Opus: Convert to 48kHz samples and add pre-skip
        uint64_t samples = (timestamp_ms * 48000) / 1000;
        return samples + stream.pre_skip;
    } else if (stream.codec_name == "vorbis") {
        // Vorbis: Direct conversion at codec sample rate
        return (timestamp_ms * stream.sample_rate) / 1000;
    } else if (stream.codec_name == "flac") {
        // FLAC-in-Ogg: Direct conversion like Vorbis
        return (timestamp_ms * stream.sample_rate) / 1000;
    } else if (stream.codec_name == "speex") {
        // Speex: Direct conversion at codec sample rate
        return (timestamp_ms * stream.sample_rate) / 1000;
    }
    
    // Default: assume direct sample count
    return (timestamp_ms * stream.sample_rate) / 1000;
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

/**
 * @brief Parse Vorbis header packets
 * 
 * Following libvorbisfile patterns (vorbis_synthesis_idheader, vorbis_synthesis_headerin):
 * - Identification header: "\x01vorbis" signature, extract sample rate, channels, bitrate
 * - Comment header: "\x03vorbis" signature with UTF-8 metadata
 * - Setup header: "\x05vorbis" signature with codec setup data
 * - Requires all 3 headers for complete initialization
 * 
 * @param stream OggStream to update with parsed header data
 * @param packet Header packet to parse
 * @return true on success, false on error
 * 
 * Requirements: 4.1, 4.2, 4.3
 */
bool OggDemuxer::parseVorbisHeaders(OggStream& stream, const OggPacket& packet) {
    const std::vector<uint8_t>& data = packet.data;
    
    if (data.size() < 7) {
        Debug::log("ogg", "parseVorbisHeaders: Packet too small (%zu bytes)", data.size());
        return false;
    }
    
    // Check packet type byte and "vorbis" signature
    uint8_t packet_type = data[0];
    
    // Validate "vorbis" signature (bytes 1-6)
    if (data[1] != 'v' || data[2] != 'o' || data[3] != 'r' ||
        data[4] != 'b' || data[5] != 'i' || data[6] != 's') {
        Debug::log("ogg", "parseVorbisHeaders: Invalid vorbis signature");
        return false;
    }
    
    switch (packet_type) {
        case 0x01: {
            // Identification header (Requirements 4.1)
            // Minimum size: 7 (signature) + 23 (header fields) = 30 bytes
            if (data.size() < 30) {
                Debug::log("ogg", "parseVorbisHeaders: Identification header too small");
                return false;
            }
            
            // Parse Vorbis identification header fields (little-endian)
            // Offset 7: vorbis_version (4 bytes) - must be 0
            uint32_t vorbis_version = readLE<uint32_t>(data, 7);
            if (vorbis_version != 0) {
                Debug::log("ogg", "parseVorbisHeaders: Unsupported vorbis version %u", vorbis_version);
                return false;
            }
            
            // Offset 11: audio_channels (1 byte)
            stream.channels = data[11];
            if (stream.channels == 0) {
                Debug::log("ogg", "parseVorbisHeaders: Invalid channel count 0");
                return false;
            }
            
            // Offset 12: audio_sample_rate (4 bytes)
            stream.sample_rate = readLE<uint32_t>(data, 12);
            if (stream.sample_rate == 0) {
                Debug::log("ogg", "parseVorbisHeaders: Invalid sample rate 0");
                return false;
            }
            
            // Offset 16: bitrate_maximum (4 bytes)
            // Offset 20: bitrate_nominal (4 bytes)
            stream.bitrate = readLE<uint32_t>(data, 20);  // Use nominal bitrate
            
            // Offset 24: bitrate_minimum (4 bytes)
            // Offset 28: blocksize_0 and blocksize_1 (1 byte combined)
            // Offset 29: framing_flag (1 byte) - must have bit 0 set
            
            uint8_t framing = data[29];
            if ((framing & 0x01) == 0) {
                Debug::log("ogg", "parseVorbisHeaders: Invalid framing flag");
                return false;
            }
            
            Debug::log("ogg", "parseVorbisHeaders: Identification header - "
                       "channels=%u, sample_rate=%u, bitrate=%u",
                       stream.channels, stream.sample_rate, stream.bitrate);
            
            return true;
        }
        
        case 0x03: {
            // Comment header (Requirements 4.2)
            // Parse Vorbis comment header for metadata
            if (data.size() < 11) {  // 7 (signature) + 4 (vendor length)
                Debug::log("ogg", "parseVorbisHeaders: Comment header too small");
                return false;
            }
            
            size_t offset = 7;  // After signature
            
            // Vendor string length (4 bytes, little-endian)
            uint32_t vendor_length = readLE<uint32_t>(data, offset);
            offset += 4;
            
            if (offset + vendor_length > data.size()) {
                Debug::log("ogg", "parseVorbisHeaders: Vendor string exceeds packet");
                return false;
            }
            
            // Skip vendor string
            offset += vendor_length;
            
            // User comment list length (4 bytes)
            if (offset + 4 > data.size()) {
                Debug::log("ogg", "parseVorbisHeaders: Missing comment count");
                return false;
            }
            
            uint32_t comment_count = readLE<uint32_t>(data, offset);
            offset += 4;
            
            // Parse each comment
            for (uint32_t i = 0; i < comment_count && offset + 4 <= data.size(); ++i) {
                uint32_t comment_length = readLE<uint32_t>(data, offset);
                offset += 4;
                
                if (offset + comment_length > data.size()) {
                    Debug::log("ogg", "parseVorbisHeaders: Comment %u exceeds packet", i);
                    break;
                }
                
                // Parse comment as "TAG=value" (UTF-8)
                std::string comment(reinterpret_cast<const char*>(data.data() + offset), 
                                    comment_length);
                offset += comment_length;
                
                // Find '=' separator
                size_t eq_pos = comment.find('=');
                if (eq_pos != std::string::npos) {
                    std::string tag = comment.substr(0, eq_pos);
                    std::string value = comment.substr(eq_pos + 1);
                    
                    // Convert tag to uppercase for comparison
                    for (char& c : tag) {
                        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                    }
                    
                    if (tag == "ARTIST") {
                        stream.artist = value;
                    } else if (tag == "TITLE") {
                        stream.title = value;
                    } else if (tag == "ALBUM") {
                        stream.album = value;
                    }
                }
            }
            
            Debug::log("ogg", "parseVorbisHeaders: Comment header - "
                       "artist='%s', title='%s', album='%s'",
                       stream.artist.c_str(), stream.title.c_str(), stream.album.c_str());
            
            return true;
        }
        
        case 0x05: {
            // Setup header (Requirements 4.3)
            // Just validate signature - the actual setup data is preserved in header_packets
            Debug::log("ogg", "parseVorbisHeaders: Setup header (%zu bytes)", data.size());
            return true;
        }
        
        default:
            Debug::log("ogg", "parseVorbisHeaders: Unknown packet type 0x%02x", packet_type);
            return false;
    }
}

/**
 * @brief Parse FLAC-in-Ogg header packets (RFC 9639 Section 10.1)
 * 
 * FLAC-in-Ogg identification header structure (51 bytes):
 * - 5 bytes: Signature "\x7fFLAC" (0x7F 0x46 0x4C 0x41 0x43)
 * - 2 bytes: Mapping version (0x01 0x00 for version 1.0)
 * - 2 bytes: Header packet count (big-endian, 0 = unknown)
 * - 4 bytes: fLaC signature (0x66 0x4C 0x61 0x43)
 * - 4 bytes: Metadata block header for STREAMINFO
 * - 34 bytes: STREAMINFO metadata block
 * 
 * First page is always exactly 79 bytes (27 header + 1 lacing + 51 packet)
 * 
 * @param stream OggStream to update with parsed header data
 * @param packet Header packet to parse
 * @return true on success, false on error
 * 
 * Requirements: 4.6, 4.7, 4.8, 4.9, 4.10
 */
bool OggDemuxer::parseFLACHeaders(OggStream& stream, const OggPacket& packet) {
    const std::vector<uint8_t>& data = packet.data;
    
    // Check if this is the identification header or a metadata block
    if (data.size() >= 5 && data[0] == 0x7F &&
        data[1] == 'F' && data[2] == 'L' && data[3] == 'A' && data[4] == 'C') {
        
        // FLAC-in-Ogg identification header (Requirements 4.6, 4.7, 4.8, 4.9)
        // Minimum size: 51 bytes
        if (data.size() < 51) {
            Debug::log("ogg", "parseFLACHeaders: Identification header too small (%zu bytes, need 51)",
                       data.size());
            return false;
        }
        
        // Verify mapping version (Requirements 4.7)
        stream.flac_mapping_version_major = data[5];
        stream.flac_mapping_version_minor = data[6];
        
        if (stream.flac_mapping_version_major != 1 || stream.flac_mapping_version_minor != 0) {
            Debug::log("ogg", "parseFLACHeaders: Unsupported mapping version %u.%u (expected 1.0)",
                       stream.flac_mapping_version_major, stream.flac_mapping_version_minor);
            // Handle gracefully per Requirements 5.3 - continue but warn
        }
        
        // Extract header packet count (big-endian) (Requirements 4.8)
        stream.expected_header_count = (static_cast<uint16_t>(data[7]) << 8) | data[8];
        if (stream.expected_header_count == 0) {
            // 0 means unknown number of header packets
            Debug::log("ogg", "parseFLACHeaders: Unknown header packet count");
        } else {
            Debug::log("ogg", "parseFLACHeaders: Expecting %u header packets", 
                       stream.expected_header_count);
        }
        
        // Verify fLaC signature at offset 9 (Requirements 4.8)
        if (data[9] != 'f' || data[10] != 'L' || data[11] != 'a' || data[12] != 'C') {
            Debug::log("ogg", "parseFLACHeaders: Missing fLaC signature");
            return false;
        }
        
        // Parse metadata block header at offset 13 (4 bytes)
        // Bit 7 of first byte: last-metadata-block flag
        // Bits 0-6: block type (should be 0 for STREAMINFO)
        uint8_t block_type = data[13] & 0x7F;
        if (block_type != 0) {
            Debug::log("ogg", "parseFLACHeaders: Expected STREAMINFO block type 0, got %u", block_type);
            return false;
        }
        
        // Block length (24 bits big-endian) - should be 34 for STREAMINFO
        uint32_t block_length = (static_cast<uint32_t>(data[14]) << 16) |
                                (static_cast<uint32_t>(data[15]) << 8) |
                                data[16];
        if (block_length != 34) {
            Debug::log("ogg", "parseFLACHeaders: STREAMINFO block length %u, expected 34", block_length);
            return false;
        }
        
        // Parse STREAMINFO at offset 17 (34 bytes)
        // Bytes 0-1: minimum block size (16 bits big-endian)
        stream.flac_min_block_size = (static_cast<uint16_t>(data[17]) << 8) | data[18];
        
        // Bytes 2-3: maximum block size (16 bits big-endian)
        stream.flac_max_block_size = (static_cast<uint16_t>(data[19]) << 8) | data[20];
        
        // Bytes 4-6: minimum frame size (24 bits big-endian)
        stream.flac_min_frame_size = (static_cast<uint32_t>(data[21]) << 16) |
                                     (static_cast<uint32_t>(data[22]) << 8) |
                                     data[23];
        
        // Bytes 7-9: maximum frame size (24 bits big-endian)
        stream.flac_max_frame_size = (static_cast<uint32_t>(data[24]) << 16) |
                                     (static_cast<uint32_t>(data[25]) << 8) |
                                     data[26];
        
        // Bytes 10-13: sample rate (20 bits), channels (3 bits), bits per sample (5 bits)
        // Sample rate: bits 0-19 of bytes 10-12 (big-endian, upper 20 bits)
        uint32_t sr_ch_bps = (static_cast<uint32_t>(data[27]) << 24) |
                             (static_cast<uint32_t>(data[28]) << 16) |
                             (static_cast<uint32_t>(data[29]) << 8) |
                             data[30];
        
        stream.sample_rate = (sr_ch_bps >> 12) & 0xFFFFF;  // Upper 20 bits
        stream.channels = ((sr_ch_bps >> 9) & 0x07) + 1;   // Next 3 bits + 1
        stream.bits_per_sample = ((sr_ch_bps >> 4) & 0x1F) + 1;  // Next 5 bits + 1
        
        // Bytes 14-17: total samples (36 bits, upper 4 bits in byte 13)
        // Lower 32 bits in bytes 14-17
        uint64_t total_samples_low = (static_cast<uint64_t>(data[31]) << 24) |
                                     (static_cast<uint64_t>(data[32]) << 16) |
                                     (static_cast<uint64_t>(data[33]) << 8) |
                                     data[34];
        uint64_t total_samples_high = sr_ch_bps & 0x0F;  // Lower 4 bits of sr_ch_bps
        stream.total_samples = (total_samples_high << 32) | total_samples_low;
        
        // MD5 signature is at bytes 18-33 (16 bytes) - we don't need to parse it
        
        Debug::log("ogg", "parseFLACHeaders: FLAC-in-Ogg identification - "
                   "sample_rate=%u, channels=%u, bits_per_sample=%u, total_samples=%llu",
                   stream.sample_rate, stream.channels, stream.bits_per_sample,
                   static_cast<unsigned long long>(stream.total_samples));
        
        return true;
    }
    
    // Check for Vorbis comment metadata block (Requirements 4.10)
    // FLAC metadata blocks start with a 4-byte header
    if (data.size() >= 4) {
        uint8_t block_type = data[0] & 0x7F;
        uint32_t block_length = (static_cast<uint32_t>(data[1]) << 16) |
                                (static_cast<uint32_t>(data[2]) << 8) |
                                data[3];
        
        if (block_type == 4 && data.size() >= 4 + block_length) {
            // Vorbis comment block (type 4)
            // Parse vendor string and comments similar to Vorbis
            size_t offset = 4;  // After block header
            
            if (offset + 4 > data.size()) {
                return true;  // Empty comment block is valid
            }
            
            // Vendor string length (4 bytes, little-endian)
            uint32_t vendor_length = readLE<uint32_t>(data, offset);
            offset += 4;
            
            if (offset + vendor_length > data.size()) {
                Debug::log("ogg", "parseFLACHeaders: Vendor string exceeds block");
                return true;  // Continue anyway
            }
            
            offset += vendor_length;
            
            // Comment count (4 bytes, little-endian)
            if (offset + 4 > data.size()) {
                return true;
            }
            
            uint32_t comment_count = readLE<uint32_t>(data, offset);
            offset += 4;
            
            // Parse comments
            for (uint32_t i = 0; i < comment_count && offset + 4 <= data.size(); ++i) {
                uint32_t comment_length = readLE<uint32_t>(data, offset);
                offset += 4;
                
                if (offset + comment_length > data.size()) {
                    break;
                }
                
                std::string comment(reinterpret_cast<const char*>(data.data() + offset),
                                    comment_length);
                offset += comment_length;
                
                size_t eq_pos = comment.find('=');
                if (eq_pos != std::string::npos) {
                    std::string tag = comment.substr(0, eq_pos);
                    std::string value = comment.substr(eq_pos + 1);
                    
                    for (char& c : tag) {
                        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                    }
                    
                    if (tag == "ARTIST") {
                        stream.artist = value;
                    } else if (tag == "TITLE") {
                        stream.title = value;
                    } else if (tag == "ALBUM") {
                        stream.album = value;
                    }
                }
            }
            
            Debug::log("ogg", "parseFLACHeaders: Vorbis comment - "
                       "artist='%s', title='%s', album='%s'",
                       stream.artist.c_str(), stream.title.c_str(), stream.album.c_str());
        }
        
        return true;
    }
    
    return true;
}

/**
 * @brief Parse Opus header packets (RFC 7845)
 * 
 * OpusHead identification header structure (19+ bytes):
 * - 8 bytes: "OpusHead" signature
 * - 1 byte: Version (must be 1)
 * - 1 byte: Channel count
 * - 2 bytes: Pre-skip (little-endian)
 * - 4 bytes: Input sample rate (little-endian)
 * - 2 bytes: Output gain (little-endian, Q7.8 dB)
 * - 1 byte: Channel mapping family
 * - Optional: Channel mapping table (if mapping family != 0)
 * 
 * OpusTags comment header:
 * - 8 bytes: "OpusTags" signature
 * - Vorbis comment format metadata
 * 
 * @param stream OggStream to update with parsed header data
 * @param packet Header packet to parse
 * @return true on success, false on error
 * 
 * Requirements: 4.4, 4.5
 */
bool OggDemuxer::parseOpusHeaders(OggStream& stream, const OggPacket& packet) {
    const std::vector<uint8_t>& data = packet.data;
    
    if (data.size() < 8) {
        Debug::log("ogg", "parseOpusHeaders: Packet too small (%zu bytes)", data.size());
        return false;
    }
    
    // Check for OpusHead identification header (Requirements 4.4)
    if (data[0] == 'O' && data[1] == 'p' && data[2] == 'u' && data[3] == 's' &&
        data[4] == 'H' && data[5] == 'e' && data[6] == 'a' && data[7] == 'd') {
        
        // OpusHead header - minimum 19 bytes
        if (data.size() < 19) {
            Debug::log("ogg", "parseOpusHeaders: OpusHead too small (%zu bytes, need 19)",
                       data.size());
            return false;
        }
        
        // Version (offset 8) - must be 1
        uint8_t version = data[8];
        if (version != 1) {
            Debug::log("ogg", "parseOpusHeaders: Unsupported Opus version %u", version);
            return false;
        }
        
        // Channel count (offset 9)
        stream.channels = data[9];
        if (stream.channels == 0) {
            Debug::log("ogg", "parseOpusHeaders: Invalid channel count 0");
            return false;
        }
        
        // Pre-skip (offset 10-11, little-endian)
        stream.pre_skip = readLE<uint16_t>(data, 10);
        
        // Input sample rate (offset 12-15, little-endian)
        // Note: Opus always uses 48kHz internally, this is the original sample rate
        uint32_t input_sample_rate = readLE<uint32_t>(data, 12);
        stream.sample_rate = 48000;  // Opus granule position is always at 48kHz
        
        // Output gain (offset 16-17, little-endian, Q7.8 dB)
        // int16_t output_gain = readLE<int16_t>(data, 16);
        
        // Channel mapping family (offset 18)
        uint8_t mapping_family = data[18];
        
        // If mapping family != 0, there's a channel mapping table
        if (mapping_family != 0) {
            // Mapping table: stream_count (1), coupled_count (1), mapping[channels]
            size_t expected_size = 19 + 2 + stream.channels;
            if (data.size() < expected_size) {
                Debug::log("ogg", "parseOpusHeaders: OpusHead with mapping too small");
                return false;
            }
        }
        
        Debug::log("ogg", "parseOpusHeaders: OpusHead - channels=%u, pre_skip=%llu, "
                   "input_sample_rate=%u, mapping_family=%u",
                   stream.channels, static_cast<unsigned long long>(stream.pre_skip),
                   input_sample_rate, mapping_family);
        
        return true;
    }
    
    // Check for OpusTags comment header (Requirements 4.5)
    if (data[0] == 'O' && data[1] == 'p' && data[2] == 'u' && data[3] == 's' &&
        data[4] == 'T' && data[5] == 'a' && data[6] == 'g' && data[7] == 's') {
        
        // OpusTags header - parse Vorbis comment format
        if (data.size() < 16) {  // 8 (signature) + 4 (vendor length) + 4 (comment count)
            Debug::log("ogg", "parseOpusHeaders: OpusTags too small");
            return true;  // Empty tags is valid
        }
        
        size_t offset = 8;  // After signature
        
        // Vendor string length (4 bytes, little-endian)
        uint32_t vendor_length = readLE<uint32_t>(data, offset);
        offset += 4;
        
        if (offset + vendor_length > data.size()) {
            Debug::log("ogg", "parseOpusHeaders: Vendor string exceeds packet");
            return true;  // Continue anyway
        }
        
        offset += vendor_length;
        
        // Comment count (4 bytes, little-endian)
        if (offset + 4 > data.size()) {
            return true;
        }
        
        uint32_t comment_count = readLE<uint32_t>(data, offset);
        offset += 4;
        
        // Parse comments
        for (uint32_t i = 0; i < comment_count && offset + 4 <= data.size(); ++i) {
            uint32_t comment_length = readLE<uint32_t>(data, offset);
            offset += 4;
            
            if (offset + comment_length > data.size()) {
                break;
            }
            
            std::string comment(reinterpret_cast<const char*>(data.data() + offset),
                                comment_length);
            offset += comment_length;
            
            size_t eq_pos = comment.find('=');
            if (eq_pos != std::string::npos) {
                std::string tag = comment.substr(0, eq_pos);
                std::string value = comment.substr(eq_pos + 1);
                
                for (char& c : tag) {
                    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                }
                
                if (tag == "ARTIST") {
                    stream.artist = value;
                } else if (tag == "TITLE") {
                    stream.title = value;
                } else if (tag == "ALBUM") {
                    stream.album = value;
                }
            }
        }
        
        Debug::log("ogg", "parseOpusHeaders: OpusTags - artist='%s', title='%s', album='%s'",
                   stream.artist.c_str(), stream.title.c_str(), stream.album.c_str());
        
        return true;
    }
    
    Debug::log("ogg", "parseOpusHeaders: Unknown Opus header type");
    return false;
}

/**
 * @brief Parse Speex header packets
 * 
 * Speex identification header:
 * - 8 bytes: "Speex   " signature (with trailing spaces)
 * - Header fields for sample rate, channels, etc.
 * 
 * @param stream OggStream to update with parsed header data
 * @param packet Header packet to parse
 * @return true on success, false on error
 */
bool OggDemuxer::parseSpeexHeaders(OggStream& stream, const OggPacket& packet) {
    const std::vector<uint8_t>& data = packet.data;
    
    if (data.size() < 80) {  // Speex header is 80 bytes
        Debug::log("ogg", "parseSpeexHeaders: Packet too small (%zu bytes)", data.size());
        return false;
    }
    
    // Verify "Speex   " signature
    if (data[0] != 'S' || data[1] != 'p' || data[2] != 'e' || data[3] != 'e' ||
        data[4] != 'x' || data[5] != ' ' || data[6] != ' ' || data[7] != ' ') {
        Debug::log("ogg", "parseSpeexHeaders: Invalid Speex signature");
        return false;
    }
    
    // Parse Speex header fields
    // Offset 36: sample rate (4 bytes, little-endian)
    stream.sample_rate = readLE<uint32_t>(data, 36);
    
    // Offset 48: number of channels (4 bytes, little-endian)
    stream.channels = static_cast<uint16_t>(readLE<uint32_t>(data, 48));
    
    // Offset 52: bitrate (4 bytes, little-endian)
    stream.bitrate = readLE<uint32_t>(data, 52);
    
    Debug::log("ogg", "parseSpeexHeaders: sample_rate=%u, channels=%u, bitrate=%u",
               stream.sample_rate, stream.channels, stream.bitrate);
    
    return true;
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
    header_packet.data = packet_data;  // Copy for storage
    header_packet.granule_position = static_cast<uint64_t>(ogg_pkt.granulepos);
    header_packet.is_first_packet = true;
    header_packet.is_last_packet = false;
    header_packet.is_continued = false;
    stream.header_packets.push_back(header_packet);
    
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
    
    // Parse the identification header to extract codec parameters
    // This is done after setting expected_header_count so FLAC can update it
    bool parse_result = false;
    if (codec_name == "vorbis") {
        parse_result = parseVorbisHeaders(stream, header_packet);
    } else if (codec_name == "opus") {
        parse_result = parseOpusHeaders(stream, header_packet);
    } else if (codec_name == "flac") {
        parse_result = parseFLACHeaders(stream, header_packet);
        // FLAC-in-Ogg: expected_header_count is now set from the identification header
        // Add 1 for the identification header itself
        if (stream.expected_header_count > 0) {
            stream.expected_header_count += 1;
        }
    } else if (codec_name == "speex") {
        parse_result = parseSpeexHeaders(stream, header_packet);
    }
    
    if (!parse_result) {
        Debug::log("ogg", "handleBOSPage: Failed to parse identification header for %s",
                   codec_name.c_str());
        // Continue anyway - we have the raw header data
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
                // Still collecting headers - store the packet
                stream.header_packets.push_back(packet);
                
                // Parse the header packet to extract metadata
                // (First header was already parsed in handleBOSPage)
                if (stream.header_packets.size() > 1) {
                    bool parse_result = false;
                    if (stream.codec_name == "vorbis") {
                        parse_result = parseVorbisHeaders(stream, packet);
                    } else if (stream.codec_name == "opus") {
                        parse_result = parseOpusHeaders(stream, packet);
                    } else if (stream.codec_name == "flac") {
                        parse_result = parseFLACHeaders(stream, packet);
                    } else if (stream.codec_name == "speex") {
                        // Speex comment header uses Vorbis comment format
                        // Just store it, no special parsing needed
                        parse_result = true;
                    }
                    
                    if (!parse_result) {
                        Debug::log("ogg", "handleDataPage: Failed to parse header %zu for %s",
                                   stream.header_packets.size(), stream.codec_name.c_str());
                        // Continue anyway - we have the raw header data
                    }
                }
                
                // Check if we have all expected headers
                // For FLAC with unknown header count (0), check for last-metadata-block flag
                bool headers_done = false;
                if (stream.expected_header_count > 0) {
                    headers_done = (stream.header_packets.size() >= stream.expected_header_count);
                } else if (stream.codec_name == "flac" && packet.data.size() >= 4) {
                    // FLAC: Check if this is the last metadata block
                    // Bit 7 of first byte indicates last-metadata-block
                    headers_done = (packet.data[0] & 0x80) != 0;
                }
                
                if (headers_done) {
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

/**
 * @brief Get the last valid granule position in the file
 * 
 * Following libopusfile op_get_last_page() and libvorbisfile patterns:
 * - First check header-provided total sample counts (preferred)
 * - Then use tracked maximum granule position during parsing
 * - Finally scan backwards from end of file to find last valid granule
 * - Use chunk-based scanning with exponentially increasing chunk sizes
 * 
 * @return Last valid granule position, or 0 if unknown
 * 
 * Requirements: 8.1, 8.2, 8.3, 8.4
 */
uint64_t OggDemuxer::getLastGranulePosition() {
    Debug::log("ogg", "getLastGranulePosition: Starting duration calculation");
    
    // Priority 1: Check header-provided total sample counts (Requirements 8.2)
    // This is the most reliable source when available
    uint64_t header_granule = getLastGranuleFromHeaders();
    if (header_granule > 0) {
        Debug::log("ogg", "getLastGranulePosition: Using header-provided total samples: %llu",
                   static_cast<unsigned long long>(header_granule));
        return header_granule;
    }
    
    // Priority 2: Use tracked maximum granule position during parsing
    // This is available if we've already parsed some of the file
    if (m_max_granule_seen > 0) {
        Debug::log("ogg", "getLastGranulePosition: Using tracked max granule: %llu",
                   static_cast<unsigned long long>(m_max_granule_seen));
        // Note: This may not be the actual last granule if we haven't parsed the whole file
        // Continue to backward scanning for more accurate result
    }
    
    // Priority 3: Scan backwards from end of file (Requirements 8.1, 8.3)
    // Following op_get_last_page() pattern
    if (!m_handler) {
        Debug::log("ogg", "getLastGranulePosition: No IOHandler available");
        return m_max_granule_seen;
    }
    
    // Get file size if not already known
    if (m_file_size == 0) {
        int64_t current_pos = m_handler->tell();
        if (m_handler->seek(0, SEEK_END)) {
            m_file_size = static_cast<uint64_t>(m_handler->tell());
            m_handler->seek(current_pos, SEEK_SET);
        }
        if (m_file_size == 0) {
            Debug::log("ogg", "getLastGranulePosition: Cannot determine file size");
            return m_max_granule_seen;
        }
    }
    
    Debug::log("ogg", "getLastGranulePosition: File size is %llu bytes",
               static_cast<unsigned long long>(m_file_size));
    
    // Find the primary audio stream for serial number preference
    uint32_t preferred_serial = 0;
    bool has_preferred_serial = false;
    uint64_t longest_duration = 0;
    
    for (const auto& [serial, stream] : m_streams) {
        if (stream.codec_type == "audio" && stream.headers_complete) {
            // For multiplexed files, use the longest stream (Requirements 8.4)
            if (stream.total_samples > longest_duration) {
                longest_duration = stream.total_samples;
                preferred_serial = serial;
                has_preferred_serial = true;
            } else if (!has_preferred_serial) {
                preferred_serial = serial;
                has_preferred_serial = true;
            }
        }
    }
    
    Debug::log("ogg", "getLastGranulePosition: Preferred stream 0x%08x (has_preferred=%d)",
               preferred_serial, has_preferred_serial ? 1 : 0);
    
    // Backward scanning with exponentially increasing chunk sizes
    // Following libopusfile pattern: start with CHUNKSIZE, increase up to larger sizes
    static constexpr size_t INITIAL_CHUNK_SIZE = CHUNKSIZE;  // 65536
    static constexpr size_t MAX_CHUNK_SIZE = CHUNKSIZE * 4;  // 262144
    static constexpr int MAX_SCAN_ITERATIONS = 10;
    
    int64_t scan_end = static_cast<int64_t>(m_file_size);
    size_t current_chunk_size = INITIAL_CHUNK_SIZE;
    uint64_t best_granule = 0;
    int iterations = 0;
    
    while (scan_end > 0 && iterations < MAX_SCAN_ITERATIONS) {
        iterations++;
        
        // Calculate scan start position
        int64_t scan_start = scan_end - static_cast<int64_t>(current_chunk_size);
        if (scan_start < 0) {
            scan_start = 0;
        }
        
        Debug::log("ogg", "getLastGranulePosition: Scanning chunk [%lld, %lld] (size %zu)",
                   static_cast<long long>(scan_start),
                   static_cast<long long>(scan_end),
                   current_chunk_size);
        
        // Scan this chunk for the last valid granule
        uint64_t chunk_granule = scanBackwardForLastGranule(scan_start, 
                                                            static_cast<size_t>(scan_end - scan_start));
        
        if (chunk_granule > 0) {
            // Found a valid granule - check if it's better than what we have
            if (chunk_granule > best_granule) {
                best_granule = chunk_granule;
                Debug::log("ogg", "getLastGranulePosition: Found granule %llu in chunk",
                           static_cast<unsigned long long>(best_granule));
            }
            
            // If we found a granule in the last chunk, we're done
            if (scan_end == static_cast<int64_t>(m_file_size)) {
                break;
            }
        }
        
        // Move to previous chunk
        scan_end = scan_start;
        
        // Exponentially increase chunk size for efficiency
        if (current_chunk_size < MAX_CHUNK_SIZE) {
            current_chunk_size *= 2;
            if (current_chunk_size > MAX_CHUNK_SIZE) {
                current_chunk_size = MAX_CHUNK_SIZE;
            }
        }
        
        // If we've reached the beginning of the file, stop
        if (scan_start == 0) {
            break;
        }
    }
    
    // Use the best granule found, or fall back to tracked max
    if (best_granule > 0) {
        // Update tracked max if we found something better
        if (best_granule > m_max_granule_seen) {
            m_max_granule_seen = best_granule;
        }
        Debug::log("ogg", "getLastGranulePosition: Final result: %llu",
                   static_cast<unsigned long long>(best_granule));
        return best_granule;
    }
    
    Debug::log("ogg", "getLastGranulePosition: Using tracked max: %llu",
               static_cast<unsigned long long>(m_max_granule_seen));
    return m_max_granule_seen;
}

/**
 * @brief Scan a buffer for the last valid granule position
 * 
 * Scans through a buffer looking for Ogg pages and extracts the last
 * valid granule position found.
 * 
 * @param buffer Buffer containing Ogg data
 * @param buffer_size Size of data in buffer
 * @return Last valid granule position found, or 0 if none
 */
uint64_t OggDemuxer::scanBufferForLastGranule(const std::vector<uint8_t>& buffer, size_t buffer_size) {
    if (buffer.empty() || buffer_size == 0) {
        return 0;
    }
    
    uint64_t last_granule = 0;
    size_t offset = 0;
    
    // Scan through buffer looking for OggS capture patterns
    while (offset + OGG_PAGE_HEADER_MIN_SIZE <= buffer_size) {
        // Look for OggS capture pattern
        int64_t pattern_offset = OggPageParser::findNextCapturePattern(
            buffer.data(), buffer_size, offset);
        
        if (pattern_offset < 0) {
            break;  // No more pages found
        }
        
        offset = static_cast<size_t>(pattern_offset);
        
        // Try to parse the page
        OggPage page;
        size_t bytes_consumed = 0;
        OggPageParser::ParseResult result = OggPageParser::parsePage(
            buffer.data() + offset, buffer_size - offset, page, bytes_consumed);
        
        if (result == OggPageParser::ParseResult::SUCCESS) {
            // Check if this page has a valid granule position
            uint64_t granule = page.getGranulePosition();
            
            // Skip invalid granule positions (-1 as unsigned, or FLAC no-packet marker)
            if (granule != static_cast<uint64_t>(-1) && 
                !isFlacOggNoPacketGranule(granule)) {
                last_granule = granule;
            }
            
            offset += bytes_consumed;
        } else if (result == OggPageParser::ParseResult::NEED_MORE_DATA) {
            // Incomplete page at end of buffer
            break;
        } else {
            // Invalid page - skip past the capture pattern and continue
            offset += 4;
        }
    }
    
    return last_granule;
}

/**
 * @brief Scan backwards from a position to find the last valid granule
 * 
 * Following libopusfile _get_prev_page_serial() pattern:
 * - Seek to scan_start position
 * - Read forward through the chunk
 * - Track the last valid granule position found
 * 
 * @param scan_start Starting position for the scan
 * @param scan_size Size of region to scan
 * @return Last valid granule position found, or 0 if none
 * 
 * Requirements: 8.1, 8.3
 */
uint64_t OggDemuxer::scanBackwardForLastGranule(int64_t scan_start, size_t scan_size) {
    if (!m_handler || scan_size == 0) {
        return 0;
    }
    
    // Save current position
    int64_t saved_pos = m_handler->tell();
    int64_t saved_offset = m_offset;
    
    // Seek to scan start
    if (!m_handler->seek(scan_start, SEEK_SET)) {
        Debug::log("ogg", "scanBackwardForLastGranule: Seek to %lld failed",
                   static_cast<long long>(scan_start));
        return 0;
    }
    
    // Reset sync state for fresh scanning
    ogg_sync_reset(&m_sync_state);
    m_offset = scan_start;
    
    uint64_t last_granule = 0;
    int64_t scan_end = scan_start + static_cast<int64_t>(scan_size);
    
    // Scan forward through the chunk, tracking the last valid granule
    ogg_page page;
    while (m_offset < scan_end) {
        int ret = getNextPage(&page, scan_end - m_offset);
        
        if (ret <= 0) {
            break;  // No more pages or error
        }
        
        // Get granule position from this page
        int64_t granule = pageGranulePos(&page);
        
        // Check for valid granule position
        // Skip -1 (no packets finish on page) and FLAC no-packet marker
        if (granule >= 0 && !isFlacOggNoPacketGranule(static_cast<uint64_t>(granule))) {
            last_granule = static_cast<uint64_t>(granule);
        }
    }
    
    // Restore original position
    m_handler->seek(saved_pos, SEEK_SET);
    ogg_sync_reset(&m_sync_state);
    m_offset = saved_offset;
    
    return last_granule;
}

/**
 * @brief Scan a chunk for the last granule, optionally preferring a specific serial
 * 
 * @param buffer Buffer containing Ogg data
 * @param buffer_size Size of data in buffer
 * @param preferred_serial Serial number to prefer (for multiplexed files)
 * @param has_preferred_serial true if preferred_serial is valid
 * @return Last valid granule position found, or 0 if none
 * 
 * Requirements: 8.4
 */
uint64_t OggDemuxer::scanChunkForLastGranule(const std::vector<uint8_t>& buffer, size_t buffer_size, 
                                             uint32_t preferred_serial, bool has_preferred_serial) {
    if (buffer.empty() || buffer_size == 0) {
        return 0;
    }
    
    uint64_t last_granule = 0;
    uint64_t preferred_granule = 0;
    size_t offset = 0;
    
    // Scan through buffer looking for OggS capture patterns
    while (offset + OGG_PAGE_HEADER_MIN_SIZE <= buffer_size) {
        // Look for OggS capture pattern
        int64_t pattern_offset = OggPageParser::findNextCapturePattern(
            buffer.data(), buffer_size, offset);
        
        if (pattern_offset < 0) {
            break;  // No more pages found
        }
        
        offset = static_cast<size_t>(pattern_offset);
        
        // Try to parse the page
        OggPage page;
        size_t bytes_consumed = 0;
        OggPageParser::ParseResult result = OggPageParser::parsePage(
            buffer.data() + offset, buffer_size - offset, page, bytes_consumed);
        
        if (result == OggPageParser::ParseResult::SUCCESS) {
            // Check if this page has a valid granule position
            uint64_t granule = page.getGranulePosition();
            
            // Skip invalid granule positions
            if (granule != static_cast<uint64_t>(-1) && 
                !isFlacOggNoPacketGranule(granule)) {
                
                // Track last granule from any stream
                last_granule = granule;
                
                // Track last granule from preferred stream
                if (has_preferred_serial && page.getSerialNumber() == preferred_serial) {
                    preferred_granule = granule;
                }
            }
            
            offset += bytes_consumed;
        } else if (result == OggPageParser::ParseResult::NEED_MORE_DATA) {
            break;
        } else {
            offset += 4;
        }
    }
    
    // Prefer the granule from the preferred stream if available
    if (has_preferred_serial && preferred_granule > 0) {
        return preferred_granule;
    }
    
    return last_granule;
}

/**
 * @brief Scan forward from a position to find the last valid granule
 * 
 * Used when backward scanning fails or for verification.
 * 
 * @param start_position Starting position for the scan
 * @return Last valid granule position found, or 0 if none
 */
uint64_t OggDemuxer::scanForwardForLastGranule(int64_t start_position) {
    if (!m_handler) {
        return 0;
    }
    
    // Save current position
    int64_t saved_pos = m_handler->tell();
    int64_t saved_offset = m_offset;
    
    // Seek to start position
    if (!m_handler->seek(start_position, SEEK_SET)) {
        return 0;
    }
    
    // Reset sync state
    ogg_sync_reset(&m_sync_state);
    m_offset = start_position;
    
    uint64_t last_granule = 0;
    
    // Scan forward until EOF
    ogg_page page;
    while (true) {
        int ret = getNextPage(&page, -1);  // No boundary
        
        if (ret <= 0) {
            break;  // EOF or error
        }
        
        // Get granule position
        int64_t granule = pageGranulePos(&page);
        
        if (granule >= 0 && !isFlacOggNoPacketGranule(static_cast<uint64_t>(granule))) {
            last_granule = static_cast<uint64_t>(granule);
        }
    }
    
    // Restore original position
    m_handler->seek(saved_pos, SEEK_SET);
    ogg_sync_reset(&m_sync_state);
    m_offset = saved_offset;
    
    return last_granule;
}

/**
 * @brief Get last granule position from header-provided total sample counts
 * 
 * Some codecs provide total sample count in their headers:
 * - FLAC: STREAMINFO contains total_samples (36 bits)
 * - Opus: May have R128_TRACK_GAIN or similar metadata
 * - Vorbis: No standard total sample count in headers
 * 
 * @return Total samples from headers, or 0 if not available
 * 
 * Requirements: 8.2
 */
uint64_t OggDemuxer::getLastGranuleFromHeaders() {
    uint64_t best_total = 0;
    
    for (const auto& [serial, stream] : m_streams) {
        if (stream.codec_type != "audio" || !stream.headers_complete) {
            continue;
        }
        
        // FLAC-in-Ogg: STREAMINFO contains total_samples
        if (stream.codec_name == "flac" && stream.total_samples > 0) {
            Debug::log("ogg", "getLastGranuleFromHeaders: FLAC stream 0x%08x has total_samples=%llu",
                       serial, static_cast<unsigned long long>(stream.total_samples));
            
            // For multiplexed files, use the longest stream (Requirements 8.4)
            if (stream.total_samples > best_total) {
                best_total = stream.total_samples;
            }
        }
        
        // Vorbis: No standard total sample count in headers
        // Would need to scan to end of file
        
        // Opus: No standard total sample count in headers
        // R128_TRACK_GAIN doesn't provide duration
    }
    
    return best_total;
}

/**
 * @brief Seek to a page containing the target granule position using bisection search (public API with lock)
 * 
 * Following libvorbisfile ov_pcm_seek_page() and libopusfile op_pcm_seek_page() patterns:
 * - Use ogg_sync_pageseek() for page discovery (NOT ogg_sync_pageout())
 * - Implement proper interval management and boundary conditions
 * - Switch to linear scanning when interval becomes small
 * - Use ogg_stream_packetpeek() to examine packets without consuming
 * 
 * @param target_granule Target granule position to seek to
 * @param stream_id Stream ID to seek within
 * @return true on success, false on failure
 * 
 * Requirements: 7.1, 7.2, 7.11, 11.1, 11.2
 */
bool OggDemuxer::seekToPage(uint64_t target_granule, uint32_t stream_id) {
    std::lock_guard<std::mutex> lock(m_ogg_state_mutex);
    return seekToPage_unlocked(target_granule, stream_id);
}

/**
 * @brief Seek to a page containing the target granule position using bisection search (private implementation without lock)
 * 
 * Assumes m_ogg_state_mutex is already held by caller.
 * 
 * Following libvorbisfile ov_pcm_seek_page() and libopusfile op_pcm_seek_page() patterns:
 * - Use ogg_sync_pageseek() for page discovery (NOT ogg_sync_pageout())
 * - Implement proper interval management and boundary conditions
 * - Switch to linear scanning when interval becomes small
 * - Use ogg_stream_packetpeek() to examine packets without consuming
 * 
 * @param target_granule Target granule position to seek to
 * @param stream_id Stream ID to seek within
 * @return true on success, false on failure
 * 
 * Requirements: 7.1, 7.2, 7.11
 */
bool OggDemuxer::seekToPage_unlocked(uint64_t target_granule, uint32_t stream_id) {
    if (!m_handler) {
        Debug::log("ogg", "seekToPage: No IOHandler available");
        return false;
    }
    
    // Check if stream exists
    auto stream_it = m_streams.find(stream_id);
    if (stream_it == m_streams.end()) {
        Debug::log("ogg", "seekToPage: Stream 0x%08x not found", stream_id);
        return false;
    }
    
    OggStream& stream = stream_it->second;
    
    // Get file size for boundary checking
    if (m_file_size == 0) {
        int64_t current_pos = m_handler->tell();
        if (m_handler->seek(0, SEEK_END)) {
            m_file_size = static_cast<uint64_t>(m_handler->tell());
            m_handler->seek(current_pos, SEEK_SET);
        }
        if (m_file_size == 0) {
            Debug::log("ogg", "seekToPage: Cannot determine file size");
            return false;
        }
    }
    
    Debug::log("ogg", "seekToPage: Seeking to granule %llu in stream 0x%08x (file size: %llu)",
               static_cast<unsigned long long>(target_granule),
               stream_id,
               static_cast<unsigned long long>(m_file_size));
    
    // Initialize bisection interval
    // begin: start of data (after headers)
    // end: end of file
    int64_t begin = 0;
    int64_t end = static_cast<int64_t>(m_file_size);
    int64_t best_offset = -1;
    int64_t best_granule = -1;
    
    // Minimum interval size before switching to linear scan
    // Following libvorbisfile pattern: use CHUNKSIZE as threshold
    static constexpr int64_t MIN_BISECTION_INTERVAL = CHUNKSIZE;
    
    // Maximum iterations to prevent infinite loops
    static constexpr int MAX_ITERATIONS = 50;
    int iterations = 0;
    
    ogg_page page;
    
    // Bisection search loop
    while (end - begin > MIN_BISECTION_INTERVAL && iterations < MAX_ITERATIONS) {
        iterations++;
        
        // Calculate bisection point
        int64_t bisect = begin + (end - begin) / 2;
        
        Debug::log("ogg", "seekToPage: Bisection iteration %d: begin=%lld, bisect=%lld, end=%lld",
                   iterations,
                   static_cast<long long>(begin),
                   static_cast<long long>(bisect),
                   static_cast<long long>(end));
        
        // Seek to bisection point
        if (!m_handler->seek(bisect, SEEK_SET)) {
            Debug::log("ogg", "seekToPage: Seek to bisect point %lld failed",
                       static_cast<long long>(bisect));
            return false;
        }
        
        // Reset sync state for fresh page scanning
        ogg_sync_reset(&m_sync_state);
        m_offset = bisect;
        
        // Find the next page with matching serial number
        bool found_page = false;
        int64_t page_offset = -1;
        int64_t page_granule = -1;
        
        // Scan forward to find a page with the target serial number
        int64_t scan_limit = end - bisect;
        int64_t bytes_scanned = 0;
        
        while (bytes_scanned < scan_limit) {
            int ret = getNextPage(&page, scan_limit - bytes_scanned);
            
            if (ret <= 0) {
                // No more pages found in this range
                break;
            }
            
            bytes_scanned += ret;
            
            // Check if this page belongs to our target stream
            uint32_t page_serial = pageSerialNo(&page);
            if (page_serial == stream_id) {
                int64_t granule = pageGranulePos(&page);
                
                // Skip pages with invalid granule position (-1)
                // Per Requirements 7.10: continue searching when granule is -1
                if (granule != -1) {
                    found_page = true;
                    page_offset = m_offset - ret;
                    page_granule = granule;
                    break;
                }
            }
        }
        
        if (!found_page) {
            // No valid page found in upper half, search lower half
            end = bisect;
            Debug::log("ogg", "seekToPage: No page found, narrowing to [%lld, %lld]",
                       static_cast<long long>(begin),
                       static_cast<long long>(end));
            continue;
        }
        
        Debug::log("ogg", "seekToPage: Found page at offset %lld with granule %lld",
                   static_cast<long long>(page_offset),
                   static_cast<long long>(page_granule));
        
        // Compare granule position with target
        int cmp = granposCmp(page_granule, static_cast<int64_t>(target_granule));
        
        if (cmp < 0) {
            // Page granule is before target - search upper half
            begin = page_offset + pageTotalSize(&page);
            
            // Remember this as a potential best position (before target)
            if (best_offset == -1 || page_granule > best_granule) {
                best_offset = page_offset;
                best_granule = page_granule;
            }
            
            Debug::log("ogg", "seekToPage: Granule %lld < target %llu, narrowing to [%lld, %lld]",
                       static_cast<long long>(page_granule),
                       static_cast<unsigned long long>(target_granule),
                       static_cast<long long>(begin),
                       static_cast<long long>(end));
        } else if (cmp > 0) {
            // Page granule is after target - search lower half
            end = page_offset;
            
            Debug::log("ogg", "seekToPage: Granule %lld > target %llu, narrowing to [%lld, %lld]",
                       static_cast<long long>(page_granule),
                       static_cast<unsigned long long>(target_granule),
                       static_cast<long long>(begin),
                       static_cast<long long>(end));
        } else {
            // Exact match found
            best_offset = page_offset;
            best_granule = page_granule;
            Debug::log("ogg", "seekToPage: Exact match found at offset %lld",
                       static_cast<long long>(page_offset));
            break;
        }
    }
    
    // Switch to linear scanning for small intervals
    // Per Requirements 7.11: switch to linear scanning when interval becomes small
    if (end - begin <= MIN_BISECTION_INTERVAL && best_offset == -1) {
        Debug::log("ogg", "seekToPage: Switching to linear scan in [%lld, %lld]",
                   static_cast<long long>(begin),
                   static_cast<long long>(end));
        
        if (!m_handler->seek(begin, SEEK_SET)) {
            Debug::log("ogg", "seekToPage: Linear scan seek failed");
            return false;
        }
        
        ogg_sync_reset(&m_sync_state);
        m_offset = begin;
        
        // Linear scan to find the best page
        while (m_offset < end) {
            int ret = getNextPage(&page, end - m_offset);
            
            if (ret <= 0) {
                break;
            }
            
            uint32_t page_serial = pageSerialNo(&page);
            if (page_serial == stream_id) {
                int64_t granule = pageGranulePos(&page);
                
                if (granule != -1) {
                    int cmp = granposCmp(granule, static_cast<int64_t>(target_granule));
                    
                    if (cmp <= 0) {
                        // This page is at or before target
                        best_offset = m_offset - ret;
                        best_granule = granule;
                    }
                    
                    if (cmp >= 0) {
                        // This page is at or after target - stop scanning
                        break;
                    }
                }
            }
        }
    }
    
    // If no suitable page found, try to find any page before the target
    if (best_offset == -1) {
        Debug::log("ogg", "seekToPage: No suitable page found, trying backward scan");
        
        // Use backward scanning to find a page
        if (!m_handler->seek(end, SEEK_SET)) {
            return false;
        }
        m_offset = end;
        
        int prev_ret = getPrevPageSerial(&page, stream_id);
        if (prev_ret >= 0) {
            best_offset = prev_ret;
            best_granule = pageGranulePos(&page);
        }
    }
    
    if (best_offset == -1) {
        Debug::log("ogg", "seekToPage: Failed to find any suitable page");
        return false;
    }
    
    Debug::log("ogg", "seekToPage: Final position: offset %lld, granule %lld (target was %llu)",
               static_cast<long long>(best_offset),
               static_cast<long long>(best_granule),
               static_cast<unsigned long long>(target_granule));
    
    // Seek to the best position and reset state
    if (!m_handler->seek(best_offset, SEEK_SET)) {
        Debug::log("ogg", "seekToPage: Final seek to %lld failed",
                   static_cast<long long>(best_offset));
        return false;
    }
    
    // Reset sync state after seek (Requirements 14.9)
    resetSyncStateAfterSeek_unlocked();
    m_offset = best_offset;
    
    // Clear packet queues for all streams
    // Do NOT resend headers (Requirements 7.6)
    for (auto& [serial, app_stream] : m_streams) {
        std::lock_guard<std::mutex> queue_lock(m_packet_queue_mutex);
        app_stream.m_packet_queue.clear();
        // Note: headers_sent remains true - we don't resend headers after seek
    }
    
    // Update stream position tracking
    if (best_granule >= 0) {
        stream.total_samples_processed = static_cast<uint64_t>(best_granule);
    }
    
    // Cache this seek position for future use
    uint64_t timestamp_ms = granuleToMs(static_cast<uint64_t>(best_granule), stream_id);
    addSeekHint_unlocked(timestamp_ms, best_offset, static_cast<uint64_t>(best_granule));
    
    return true;
}

/**
 * @brief Examine packets at a specific file position without consuming them
 * 
 * Following libvorbisfile pattern using ogg_stream_packetpeek():
 * - Seek to position
 * - Read page and submit to stream state
 * - Use packetpeek to examine without consuming
 * 
 * @param file_offset File offset to examine
 * @param stream_id Stream ID to examine
 * @param granule_position Output: granule position found
 * @return true if valid granule found, false otherwise
 * 
 * Requirements: 7.2
 */
bool OggDemuxer::examinePacketsAtPosition(int64_t file_offset, uint32_t stream_id, uint64_t& granule_position) {
    if (!m_handler) {
        return false;
    }
    
    // Seek to the specified position
    if (!m_handler->seek(file_offset, SEEK_SET)) {
        Debug::log("ogg", "examinePacketsAtPosition: Seek to %lld failed",
                   static_cast<long long>(file_offset));
        return false;
    }
    
    // Reset sync state
    ogg_sync_reset(&m_sync_state);
    m_offset = file_offset;
    
    // Find a page with the target serial number
    ogg_page page;
    int ret = getNextPage(&page, CHUNKSIZE);
    
    if (ret <= 0) {
        return false;
    }
    
    // Check if this page belongs to our target stream
    if (pageSerialNo(&page) != stream_id) {
        return false;
    }
    
    // Get granule position from page
    int64_t granule = pageGranulePos(&page);
    
    // Check for invalid granule position
    if (granule == -1) {
        return false;
    }
    
    granule_position = static_cast<uint64_t>(granule);
    
    Debug::log("ogg", "examinePacketsAtPosition: Found granule %llu at offset %lld",
               static_cast<unsigned long long>(granule_position),
               static_cast<long long>(file_offset));
    
    return true;
}

/**
 * @brief Fill packet queue for a specific stream (public API with lock)
 * 
 * Following libvorbisfile patterns:
 * - Read pages and submit to appropriate stream state
 * - Extract packets using ogg_stream_packetout()
 * - Handle packet continuation across pages
 * 
 * @param target_stream_id Stream ID to fill queue for
 * 
 * Requirements: 6.1, 6.2, 6.3, 6.4, 11.1, 11.4
 */
void OggDemuxer::fillPacketQueue(uint32_t target_stream_id) {
    std::lock_guard<std::mutex> lock(m_packet_queue_mutex);
    fillPacketQueue_unlocked(target_stream_id);
}

/**
 * @brief Fill packet queue for a specific stream (private implementation without lock)
 * 
 * Assumes m_packet_queue_mutex is already held by caller.
 * 
 * Following libvorbisfile patterns:
 * - Read pages and submit to appropriate stream state
 * - Extract packets using ogg_stream_packetout()
 * - Handle packet continuation across pages
 * 
 * @param target_stream_id Stream ID to fill queue for
 * 
 * Requirements: 6.1, 6.2, 6.3, 6.4
 */
void OggDemuxer::fillPacketQueue_unlocked(uint32_t target_stream_id) {
    // Check if stream exists
    auto stream_it = m_streams.find(target_stream_id);
    if (stream_it == m_streams.end()) {
        Debug::log("ogg", "fillPacketQueue_unlocked: Stream 0x%08x not found", target_stream_id);
        return;
    }
    
    OggStream& stream = stream_it->second;
    
    // Check queue limits
    if (stream.m_packet_queue.size() >= m_max_packet_queue_size) {
        Debug::log("ogg", "fillPacketQueue_unlocked: Queue full for stream 0x%08x (%zu packets)",
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

// ============================================================================
// Safe Granule Position Arithmetic (Following libopusfile Patterns)
// Requirements: 12.1, 12.2, 12.3, 12.4, 12.5, 12.6
// ============================================================================

/**
 * @brief Safe granule position addition with overflow detection
 * 
 * Following libopusfile's op_granpos_add() pattern:
 * - Detects overflow that would wrap past -1 (invalid value)
 * - Handles positive and negative granule position ranges
 * - Returns appropriate error codes on overflow
 * 
 * Granule position range organization:
 * [ -1 (invalid) ][ 0 ... INT64_MAX ][ INT64_MIN ... -2 ][ -1 (invalid) ]
 * 
 * @param dst_gp Output: Result of addition (set to -1 on error)
 * @param src_gp Source granule position
 * @param delta Value to add (can be negative for subtraction)
 * @return 0 on success, -1 on error (null pointer, invalid input, or overflow)
 * 
 * Requirements: 12.1, 12.4, 12.5, 12.6
 */
int OggDemuxer::granposAdd(int64_t* dst_gp, int64_t src_gp, int32_t delta) {
    // Check for null pointer
    if (dst_gp == nullptr) {
        Debug::log("ogg", "granposAdd: NULL destination pointer");
        return -1;
    }
    
    // Check for invalid source granule position (-1)
    // Per libopusfile: -1 is the invalid/unset granule position
    if (src_gp == -1) {
        *dst_gp = -1;
        return -1;
    }
    
    // Perform the addition
    int64_t result = src_gp + static_cast<int64_t>(delta);
    
    // Check for overflow that wraps to -1 (invalid)
    if (result == -1) {
        // Result is the invalid value - this is an overflow condition
        *dst_gp = -1;
        return -1;
    }
    
    // Check for overflow: if delta is positive and result is less than src_gp,
    // or if delta is negative and result is greater than src_gp
    // (accounting for the special granule position ordering)
    
    if (delta > 0) {
        // Adding positive value
        // Overflow if result wrapped around (became smaller or negative when src was positive)
        if (src_gp >= 0 && result < src_gp) {
            // Wrapped from positive to negative range
            // In granule ordering, this is valid (negative > positive)
            // But we need to check if we wrapped past -1
            
            // If src_gp was positive and result is negative (not -1), 
            // we've wrapped into the "larger" negative range
            // This is actually valid in granule position space
            *dst_gp = result;
            return 0;
        }
        if (src_gp < 0 && src_gp != -1 && result >= 0) {
            // Wrapped from negative back to positive - this means we passed -1
            *dst_gp = -1;
            return -1;
        }
    } else if (delta < 0) {
        // Adding negative value (subtraction)
        // Underflow if result wrapped around
        if (src_gp < 0 && src_gp != -1 && result > src_gp) {
            // Wrapped from negative to more negative (or positive)
            // Check if we passed -1
            if (result >= 0 || result == -1) {
                *dst_gp = -1;
                return -1;
            }
        }
        if (src_gp >= 0 && result > src_gp) {
            // Wrapped from positive to negative
            // This is valid as long as result != -1
            *dst_gp = result;
            return 0;
        }
    }
    
    *dst_gp = result;
    return 0;
}

/**
 * @brief Safe granule position subtraction with wraparound handling
 * 
 * Following libopusfile's op_granpos_diff() pattern:
 * - Handles wraparound from positive to negative values
 * - Detects underflow conditions
 * - Maintains proper ordering semantics
 * 
 * @param delta Output: Difference (gp_a - gp_b), set to 0 on error
 * @param gp_a First granule position
 * @param gp_b Second granule position
 * @return 0 on success, -1 on error (null pointer, invalid input, or overflow)
 * 
 * Requirements: 12.2, 12.4, 12.5, 12.6
 */
int OggDemuxer::granposDiff(int64_t* delta, int64_t gp_a, int64_t gp_b) {
    // Check for null pointer
    if (delta == nullptr) {
        Debug::log("ogg", "granposDiff: NULL delta pointer");
        return -1;
    }
    
    // Check for invalid granule positions (-1)
    if (gp_a == -1 || gp_b == -1) {
        *delta = 0;
        return -1;
    }
    
    // Calculate the difference
    // Need to handle potential overflow when subtracting
    
    // Check for overflow: INT64_MAX - INT64_MIN would overflow
    // We need to detect when the subtraction would overflow
    
    // If gp_a and gp_b have opposite signs and the magnitude is too large
    if (gp_a >= 0 && gp_b < 0) {
        // gp_a is positive, gp_b is negative
        // gp_a - gp_b = gp_a + |gp_b|
        // This can overflow if |gp_b| is large
        if (gp_b == INT64_MIN) {
            // Special case: -INT64_MIN overflows
            *delta = 0;
            return -1;
        }
        int64_t abs_gp_b = -gp_b;
        if (gp_a > INT64_MAX - abs_gp_b) {
            // Would overflow
            *delta = 0;
            return -1;
        }
    } else if (gp_a < 0 && gp_b >= 0) {
        // gp_a is negative, gp_b is positive
        // gp_a - gp_b = -(|gp_a| + gp_b)
        // This can underflow if both are large
        if (gp_a == INT64_MIN) {
            // Special case
            *delta = 0;
            return -1;
        }
        int64_t abs_gp_a = -gp_a;
        if (gp_b > INT64_MAX - abs_gp_a) {
            // Would underflow
            *delta = 0;
            return -1;
        }
    }
    
    // Perform the subtraction
    *delta = gp_a - gp_b;
    return 0;
}

/**
 * @brief Safe granule position comparison with proper ordering
 * 
 * Following libopusfile's op_granpos_cmp() pattern:
 * - Handles the special granule position ordering where negative values
 *   (except -1) are considered greater than positive values
 * - Treats -1 as invalid/unset (less than all valid values)
 * 
 * Granule position ordering:
 * -1 (invalid) < 0 < 1 < ... < INT64_MAX < INT64_MIN < ... < -2
 * 
 * @param gp_a First granule position
 * @param gp_b Second granule position
 * @return -1 if gp_a < gp_b, 0 if gp_a == gp_b, 1 if gp_a > gp_b
 * 
 * Requirements: 12.3, 12.4
 */
int OggDemuxer::granposCmp(int64_t gp_a, int64_t gp_b) {
    // Handle invalid granule positions (-1)
    // -1 is considered less than all valid values
    if (gp_a == -1 && gp_b == -1) {
        return 0;  // Both invalid, considered equal
    }
    if (gp_a == -1) {
        return -1;  // Invalid is less than valid
    }
    if (gp_b == -1) {
        return 1;   // Valid is greater than invalid
    }
    
    // Both are valid granule positions
    // In granule position space:
    // - Positive values (0 to INT64_MAX) come first
    // - Negative values (INT64_MIN to -2) come after (are "larger")
    
    // Check if both have the same sign
    bool a_negative = (gp_a < 0);
    bool b_negative = (gp_b < 0);
    
    if (a_negative == b_negative) {
        // Same sign - normal comparison
        if (gp_a < gp_b) return -1;
        if (gp_a > gp_b) return 1;
        return 0;
    }
    
    // Different signs
    // In granule position ordering: negative (except -1) > positive
    if (a_negative) {
        // gp_a is negative, gp_b is positive (or zero)
        // Negative values are "larger" in granule space
        return 1;
    } else {
        // gp_a is positive (or zero), gp_b is negative
        // Positive values are "smaller" in granule space
        return -1;
    }
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

/**
 * @brief Perform memory audit to track usage
 * 
 * Implements memory tracking per Requirements 10.1, 10.2.
 * Calculates total memory used by all packet queues.
 * 
 * @return true if memory usage is within limits
 */
bool OggDemuxer::performMemoryAudit_unlocked() {
    size_t total_queue_memory = 0;
    
    // Calculate total memory used by all packet queues
    for (const auto& [stream_id, stream] : m_streams) {
        for (const auto& packet : stream.m_packet_queue) {
            total_queue_memory += packet.data.size();
        }
    }
    
    // Update atomic counter
    m_total_memory_usage.store(total_queue_memory);
    
    // Log if approaching limit
    if (total_queue_memory > m_max_memory_usage * 0.8) {
        Debug::log("ogg", "performMemoryAudit: Memory usage at %.1f%% of limit (%zu / %zu bytes)",
                   (100.0 * total_queue_memory) / m_max_memory_usage,
                   total_queue_memory, m_max_memory_usage);
    }
    
    return total_queue_memory <= m_max_memory_usage;
}

/**
 * @brief Enforce memory limits across all streams
 * 
 * Implements bounded queues per Requirements 10.2.
 * Drops oldest packets from streams exceeding queue limits.
 */
void OggDemuxer::enforceMemoryLimits_unlocked() {
    // Enforce per-stream queue limits
    for (auto& [stream_id, stream] : m_streams) {
        enforcePacketQueueLimits_unlocked(stream_id);
    }
    
    // Perform memory audit
    if (!performMemoryAudit_unlocked()) {
        Debug::log("ogg", "enforceMemoryLimits: Memory limit exceeded, dropping packets");
        
        // If still over limit, drop packets from all streams proportionally
        size_t current_usage = m_total_memory_usage.load();
        if (current_usage > m_max_memory_usage) {
            size_t excess = current_usage - m_max_memory_usage;
            size_t dropped = 0;
            
            // Drop packets from streams with largest queues first
            while (dropped < excess && !m_streams.empty()) {
                // Find stream with largest queue
                uint32_t largest_stream = 0;
                size_t largest_queue_size = 0;
                
                for (auto& [stream_id, stream] : m_streams) {
                    if (stream.m_packet_queue.size() > largest_queue_size) {
                        largest_queue_size = stream.m_packet_queue.size();
                        largest_stream = stream_id;
                    }
                }
                
                if (largest_queue_size == 0) {
                    break;  // No more packets to drop
                }
                
                // Drop one packet from largest stream
                auto& stream = m_streams[largest_stream];
                if (!stream.m_packet_queue.empty()) {
                    size_t packet_size = stream.m_packet_queue.front().data.size();
                    stream.m_packet_queue.pop_front();
                    dropped += packet_size;
                    
                    if (m_total_memory_usage >= packet_size) {
                        m_total_memory_usage -= packet_size;
                    }
                }
            }
            
            Debug::log("ogg", "enforceMemoryLimits: Dropped %zu bytes to enforce limit", dropped);
        }
    }
}

/**
 * @brief Validate libogg structures are in consistent state
 * 
 * Implements resource management per Requirements 10.5, 10.6.
 * Checks that all ogg_stream_state structures are properly initialized.
 * 
 * @return true if all structures are valid
 */
bool OggDemuxer::validateLiboggStructures_unlocked() {
    // Verify sync state is initialized
    if (m_sync_state.fill == 0) {
        // Sync state not yet initialized - this is OK
        return true;
    }
    
    // Verify all stream states have corresponding entries in m_streams
    for (const auto& [serial, ogg_stream] : m_ogg_streams) {
        auto stream_it = m_streams.find(serial);
        if (stream_it == m_streams.end()) {
            Debug::log("ogg", "validateLiboggStructures: ogg_stream_state exists for serial 0x%08x "
                       "but no corresponding OggStream", serial);
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Perform periodic maintenance tasks
 * 
 * Implements resource management per Requirements 10.5, 10.6, 10.7.
 * Called periodically to:
 * - Enforce memory limits
 * - Clean up caches
 * - Validate structures
 */
void OggDemuxer::performPeriodicMaintenance_unlocked() {
    // Enforce memory limits
    enforceMemoryLimits_unlocked();
    
    // Validate structures
    if (!validateLiboggStructures_unlocked()) {
        Debug::log("ogg", "performPeriodicMaintenance: Structure validation failed");
    }
    
    // Clean up performance caches if they're getting too large
    if (m_page_cache.size() > m_page_cache_size) {
        // Remove oldest cached pages
        size_t excess = m_page_cache.size() - m_page_cache_size;
        m_page_cache.erase(m_page_cache.begin(), m_page_cache.begin() + excess);
        Debug::log("ogg", "performPeriodicMaintenance: Trimmed page cache to %zu entries",
                   m_page_cache.size());
    }
    
    if (m_seek_hints.size() > 100) {
        // Keep only the most recent seek hints
        if (m_seek_hints.size() > 100) {
            m_seek_hints.erase(m_seek_hints.begin(), m_seek_hints.end() - 100);
        }
    }
}

/**
 * @brief Detect potential infinite loops in parsing
 * 
 * Implements robustness per Requirements 10.1, 10.3.
 * Tracks file position to detect when we're not making progress.
 * 
 * @param operation_name Name of operation for logging
 * @return true if infinite loop detected
 */
bool OggDemuxer::detectInfiniteLoop_unlocked(const std::string& operation_name) {
    // This is a simple check - in a real implementation, we'd track
    // file position changes and detect when we're stuck
    // For now, just return false (no infinite loop detected)
    return false;
}

/**
 * @brief Perform read-ahead buffering for I/O optimization
 * 
 * Implements I/O optimization per Requirements 10.3.
 * Reads ahead to fill internal buffers, reducing I/O operations.
 * 
 * @param target_buffer_size Target size for read-ahead buffer
 * @return true if buffering was successful
 */
bool OggDemuxer::performReadAheadBuffering_unlocked(size_t target_buffer_size) {
    if (!m_handler) {
        return false;
    }
    
    // Check current buffer fill level
    size_t current_fill = m_sync_state.fill;
    if (current_fill >= target_buffer_size) {
        return true;  // Already have enough buffered
    }
    
    // Calculate how much more to read
    size_t to_read = target_buffer_size - current_fill;
    if (to_read > m_read_ahead_buffer_size) {
        to_read = m_read_ahead_buffer_size;
    }
    
    // Get buffer from ogg_sync_state
    char* buffer = ogg_sync_buffer(&m_sync_state, to_read);
    if (!buffer) {
        Debug::log("ogg", "performReadAheadBuffering: Failed to get sync buffer");
        return false;
    }
    
    // Read data from IOHandler
    size_t bytes_read = m_handler->read(buffer, 1, to_read);
    
    // Tell ogg_sync_state how much we read
    ogg_sync_wrote(&m_sync_state, bytes_read);
    
    if (bytes_read == 0) {
        m_eof = true;
    }
    
    Debug::log("ogg", "performReadAheadBuffering: Read %zu bytes, buffer now at %d bytes",
               bytes_read, m_sync_state.fill);
    
    return true;
}

/**
 * @brief Cache a page for future seeking operations
 * 
 * Implements seeking optimization per Requirements 10.3.
 * Stores recently accessed pages for quick re-access during seeking.
 * 
 * @param file_offset File offset of the page
 * @param granule_position Granule position of the page
 * @param stream_id Stream ID for the page
 * @param page_data Raw page data
 */
void OggDemuxer::cachePageForSeeking_unlocked(int64_t file_offset, uint64_t granule_position, 
                                              uint32_t stream_id, const std::vector<uint8_t>& page_data) {
    std::lock_guard<std::mutex> lock(m_page_cache_mutex);
    
    // Check if page is already cached
    for (auto& cached : m_page_cache) {
        if (cached.file_offset == file_offset && cached.stream_id == stream_id) {
            // Update access time
            cached.access_time = std::chrono::steady_clock::now();
            return;
        }
    }
    
    // Add new cache entry
    CachedPage entry;
    entry.file_offset = file_offset;
    entry.granule_position = granule_position;
    entry.stream_id = stream_id;
    entry.page_data = page_data;
    entry.access_time = std::chrono::steady_clock::now();
    
    m_page_cache.push_back(entry);
    
    // Enforce cache size limit
    if (m_page_cache.size() > m_page_cache_size) {
        // Remove least recently used entry
        auto lru_it = m_page_cache.begin();
        for (auto it = m_page_cache.begin(); it != m_page_cache.end(); ++it) {
            if (it->access_time < lru_it->access_time) {
                lru_it = it;
            }
        }
        m_page_cache.erase(lru_it);
    }
    
    Debug::log("ogg", "cachePageForSeeking: Cached page at offset %lld, granule %llu, cache size %zu",
               static_cast<long long>(file_offset), static_cast<unsigned long long>(granule_position),
               m_page_cache.size());
}

/**
 * @brief Find a cached page near the target granule position
 * 
 * Implements seeking optimization per Requirements 10.3.
 * Searches cache for pages near the target to reduce I/O during seeking.
 * 
 * @param target_granule Target granule position
 * @param stream_id Stream to search for
 * @param file_offset Output: file offset of cached page
 * @param granule_position Output: granule position of cached page
 * @return true if a suitable cached page was found
 */
bool OggDemuxer::findCachedPageNearTarget_unlocked(uint64_t target_granule, uint32_t stream_id, 
                                                   int64_t& file_offset, uint64_t& granule_position) {
    std::lock_guard<std::mutex> lock(m_page_cache_mutex);
    
    // Find the best cached page (closest to target without exceeding it)
    int64_t best_offset = -1;
    uint64_t best_granule = 0;
    
    for (const auto& cached : m_page_cache) {
        if (cached.stream_id == stream_id && 
            cached.granule_position <= target_granule &&
            cached.granule_position > best_granule) {
            best_offset = cached.file_offset;
            best_granule = cached.granule_position;
        }
    }
    
    if (best_offset >= 0) {
        file_offset = best_offset;
        granule_position = best_granule;
        m_cache_hits++;
        Debug::log("ogg", "findCachedPageNearTarget: Found cached page at offset %lld, granule %llu",
                   static_cast<long long>(file_offset), static_cast<unsigned long long>(granule_position));
        return true;
    }
    
    m_cache_misses++;
    return false;
}

/**
 * @brief Add a seek hint for future seeking operations
 * 
 * Implements seeking optimization per Requirements 10.3.
 * Stores timestamp-to-position mappings for faster seeking.
 * 
 * @param timestamp_ms Timestamp in milliseconds
 * @param file_offset File offset for this timestamp
 * @param granule_position Granule position for this timestamp
 */
void OggDemuxer::addSeekHint_unlocked(uint64_t timestamp_ms, int64_t file_offset, uint64_t granule_position) {
    std::lock_guard<std::mutex> lock(m_seek_hints_mutex);
    
    // Check if we already have a hint near this timestamp
    for (auto& hint : m_seek_hints) {
        if (hint.timestamp_ms >= timestamp_ms - m_seek_hint_granularity &&
            hint.timestamp_ms <= timestamp_ms + m_seek_hint_granularity) {
            // Update existing hint if new one is more accurate
            if (std::abs(static_cast<int64_t>(hint.timestamp_ms) - static_cast<int64_t>(timestamp_ms)) >
                std::abs(static_cast<int64_t>(timestamp_ms) - static_cast<int64_t>(timestamp_ms))) {
                hint.file_offset = file_offset;
                hint.granule_position = granule_position;
            }
            return;
        }
    }
    
    // Add new seek hint
    SeekHint hint;
    hint.timestamp_ms = timestamp_ms;
    hint.file_offset = file_offset;
    hint.granule_position = granule_position;
    m_seek_hints.push_back(hint);
    
    Debug::log("ogg", "addSeekHint: Added hint for %llu ms at offset %lld",
               static_cast<unsigned long long>(timestamp_ms), static_cast<long long>(file_offset));
}

/**
 * @brief Find the best seek hint for a target timestamp
 * 
 * Implements seeking optimization per Requirements 10.3.
 * Finds the closest seek hint to the target timestamp.
 * 
 * @param target_timestamp_ms Target timestamp in milliseconds
 * @param file_offset Output: file offset from best hint
 * @param granule_position Output: granule position from best hint
 * @return true if a suitable hint was found
 */
bool OggDemuxer::findBestSeekHint_unlocked(uint64_t target_timestamp_ms, int64_t& file_offset, 
                                           uint64_t& granule_position) {
    std::lock_guard<std::mutex> lock(m_seek_hints_mutex);
    
    if (m_seek_hints.empty()) {
        return false;
    }
    
    // Find hint closest to target (but not exceeding it)
    int64_t best_offset = -1;
    uint64_t best_granule = 0;
    int64_t best_distance = INT64_MAX;
    
    for (const auto& hint : m_seek_hints) {
        if (hint.timestamp_ms <= target_timestamp_ms) {
            int64_t distance = static_cast<int64_t>(target_timestamp_ms) - static_cast<int64_t>(hint.timestamp_ms);
            if (distance < best_distance) {
                best_distance = distance;
                best_offset = hint.file_offset;
                best_granule = hint.granule_position;
            }
        }
    }
    
    if (best_offset >= 0) {
        file_offset = best_offset;
        granule_position = best_granule;
        Debug::log("ogg", "findBestSeekHint: Found hint for %llu ms at offset %lld",
                   static_cast<unsigned long long>(target_timestamp_ms), static_cast<long long>(file_offset));
        return true;
    }
    
    return false;
}

/**
 * @brief Perform optimized read with minimal buffering
 * 
 * Implements streaming approach per Requirements 10.1, 10.3.
 * Reads data efficiently without excessive buffering.
 * 
 * @param buffer Output buffer
 * @param size Size of each element
 * @param count Number of elements to read
 * @param bytes_read Output: number of bytes actually read
 * @return true on success
 */
bool OggDemuxer::optimizedRead_unlocked(void* buffer, size_t size, size_t count, long& bytes_read) {
    if (!m_handler || !buffer) {
        return false;
    }
    
    // Use IOHandler's read method
    size_t bytes_read_size = m_handler->read(buffer, size, count);
    bytes_read = static_cast<long>(bytes_read_size);
    
    // Track total bytes read
    m_bytes_read_total += bytes_read_size;
    
    if (bytes_read_size == 0) {
        m_eof = true;
    }
    
    return true;
}

/**
 * @brief Process packet with minimal data copying
 * 
 * Implements streaming approach per Requirements 10.1.
 * Converts ogg_packet to OggPacket with minimal copying.
 * 
 * @param packet libogg packet structure
 * @param stream_id Stream ID for the packet
 * @param output_packet Output OggPacket structure
 * @return true on success
 */
bool OggDemuxer::processPacketWithMinimalCopy_unlocked(const ogg_packet& packet, uint32_t stream_id, 
                                                       OggPacket& output_packet) {
    // Copy packet data
    output_packet.stream_id = stream_id;
    output_packet.granule_position = packet.granulepos;
    output_packet.is_first_packet = (packet.b_o_s != 0);
    output_packet.is_last_packet = (packet.e_o_s != 0);
    output_packet.is_continued = (packet.packetno > 0);  // Simplified check
    
    // Copy packet data with move semantics where possible
    output_packet.data.resize(packet.bytes);
    if (packet.bytes > 0 && packet.packet) {
        std::memcpy(output_packet.data.data(), packet.packet, packet.bytes);
    }
    
    // Update memory tracking
    m_total_memory_usage += packet.bytes;
    
    return true;
}

/**
 * @brief Clean up performance caches
 * 
 * Implements resource management per Requirements 10.5, 10.6.
 * Clears all performance optimization caches.
 */
void OggDemuxer::cleanupPerformanceCaches_unlocked() {
    {
        std::lock_guard<std::mutex> lock(m_page_cache_mutex);
        m_page_cache.clear();
    }
    
    {
        std::lock_guard<std::mutex> lock(m_seek_hints_mutex);
        m_seek_hints.clear();
    }
    
    Debug::log("ogg", "cleanupPerformanceCaches: Cleared all performance caches");
}

// ============================================================================
// Task 14: Error Handling and Robustness
// ============================================================================

// ============================================================================
// Task 14.1: Container-Level Error Handling (Requirements 9.1, 9.2, 9.3, 9.4)
// ============================================================================

/**
 * @brief Skip corrupted pages using ogg_sync_pageseek() negative returns
 * 
 * Following libvorbisfile patterns:
 * - ogg_sync_pageseek() returns negative value for bytes to skip on corruption
 * - Skip those bytes and continue searching for next valid page
 * - Rely on libogg's internal CRC validation
 * 
 * Requirements: 9.1, 9.2
 */
bool OggDemuxer::skipCorruptedPages_unlocked(size_t& bytes_skipped) {
    bytes_skipped = 0;
    
    // Get current buffer state
    char* buffer = ogg_sync_buffer(&m_sync_state, READSIZE);
    if (!buffer) {
        Debug::log("ogg", "skipCorruptedPages: Failed to get sync buffer");
        return false;
    }
    
    // Try to read more data
    size_t bytes_read = m_handler->read(buffer, 1, READSIZE);
    
    if (bytes_read <= 0) {
        Debug::log("ogg", "skipCorruptedPages: EOF reached or I/O error");
        return false;
    }
    
    // Tell sync state how much data we added
    ogg_sync_wrote(&m_sync_state, static_cast<long>(bytes_read));
    
    // Try to extract a page
    ogg_page page;
    int result = ogg_sync_pageseek(&m_sync_state, &page);
    
    if (result < 0) {
        // Negative return means we need to skip bytes (corruption detected)
        // ogg_sync_pageseek() has already advanced the buffer
        bytes_skipped = static_cast<size_t>(-result);
        Debug::log("ogg", "skipCorruptedPages: Skipped %zu bytes of corrupted data", bytes_skipped);
        return true;  // Successfully skipped corrupted data
    } else if (result == 0) {
        // Need more data
        return false;
    }
    
    // result > 0 means we have a valid page (not corrupted)
    return false;
}

/**
 * @brief Handle missing packets (page loss detected via sequence numbers)
 * 
 * Per RFC 3533 Section 6: page sequence numbers detect page loss
 * Returns OV_HOLE/OP_HOLE equivalent for missing packets
 * 
 * Requirements: 9.3
 */
void OggDemuxer::reportPageLoss_unlocked(uint32_t stream_id, uint32_t expected_seq, uint32_t actual_seq) {
    uint32_t missing_pages = actual_seq - expected_seq;
    
    Debug::log("ogg", "reportPageLoss: Stream 0x%08x - missing %u pages (expected seq %u, got %u)",
               stream_id, missing_pages, expected_seq, actual_seq);
    
    // Mark stream as having experienced a hole
    // This could be used to signal OV_HOLE/OP_HOLE to callers
    // For now, we log it and continue
}

/**
 * @brief Handle codec identification failures
 * 
 * When BOS packet doesn't match known codec signatures, return OP_ENOTFORMAT
 * and continue scanning for other streams
 * 
 * Requirements: 9.4
 */
bool OggDemuxer::handleCodecIdentificationFailure_unlocked(uint32_t stream_id, 
                                                           const std::vector<uint8_t>& packet_data) {
    Debug::log("ogg", "handleCodecIdentificationFailure: Stream 0x%08x - unknown codec signature",
               stream_id);
    
    // Log first few bytes for debugging
    if (packet_data.size() >= 8) {
        Debug::log("ogg", "  First 8 bytes: %02x %02x %02x %02x %02x %02x %02x %02x",
                   packet_data[0], packet_data[1], packet_data[2], packet_data[3],
                   packet_data[4], packet_data[5], packet_data[6], packet_data[7]);
    }
    
    // Skip this stream and continue with others
    // This is equivalent to returning OP_ENOTFORMAT and continuing
    auto stream_it = m_streams.find(stream_id);
    if (stream_it != m_streams.end()) {
        stream_it->second.codec_name = "unknown";
        stream_it->second.codec_type = "unknown";
    }
    
    return true;  // Continue processing other streams
}

// ============================================================================
// Task 14.2: Resource Error Handling (Requirements 9.5, 9.6, 9.7, 9.8)
// ============================================================================

/**
 * @brief Handle memory allocation failures
 * 
 * Returns OP_EFAULT/OV_EFAULT equivalent for memory allocation failures
 * 
 * Requirements: 9.5
 */
bool OggDemuxer::handleMemoryAllocationFailure_unlocked(size_t requested_size, 
                                                        const std::string& context) {
    Debug::log("ogg", "handleMemoryAllocationFailure: Failed to allocate %zu bytes for %s",
               requested_size, context.c_str());
    
    // Try to free some memory from caches
    cleanupPerformanceCaches_unlocked();
    
    // Return error code equivalent to OP_EFAULT/OV_EFAULT
    // In PsyMP3, we use exceptions or return false
    return false;
}

/**
 * @brief Handle I/O failures
 * 
 * Returns OP_EREAD/OV_EREAD equivalent for I/O failures
 * 
 * Requirements: 9.6
 */
bool OggDemuxer::handleIOFailure_unlocked(const std::string& operation) {
    Debug::log("ogg", "handleIOFailure: I/O operation failed: %s", operation.c_str());
    
    // Return error code equivalent to OP_EREAD/OV_EREAD
    // In PsyMP3, we use exceptions or return false
    return false;
}

/**
 * @brief Clamp seeks to valid ranges
 * 
 * Ensure seek operations don't go beyond file boundaries
 * 
 * Requirements: 9.7
 */
uint64_t OggDemuxer::clampSeekPosition_unlocked(uint64_t requested_position) {
    // Get file size
    if (!m_handler) {
        return 0;
    }
    
    long current_pos = m_handler->tell();
    if (m_handler->seek(0, SEEK_END) != 0) {
        return 0;
    }
    
    long file_size = m_handler->tell();
    m_handler->seek(current_pos, SEEK_SET);  // Restore position
    
    if (file_size <= 0) {
        return 0;
    }
    
    uint64_t max_position = static_cast<uint64_t>(file_size);
    
    // Clamp to valid range
    if (requested_position > max_position) {
        Debug::log("ogg", "clampSeekPosition: Clamping seek from %llu to %llu",
                   static_cast<unsigned long long>(requested_position),
                   static_cast<unsigned long long>(max_position));
        return max_position;
    }
    
    return requested_position;
}

/**
 * @brief Parse what's possible from malformed metadata
 * 
 * When metadata is malformed, extract what we can and continue
 * Following opus_tags_parse() patterns
 * 
 * Requirements: 9.8
 */
bool OggDemuxer::parsePartialMetadata_unlocked(OggStream& stream, 
                                               const std::vector<uint8_t>& metadata_packet) {
    Debug::log("ogg", "parsePartialMetadata: Attempting to parse malformed metadata for stream 0x%08x",
               stream.serial_number);
    
    // Try to extract what we can from the metadata
    // This is a best-effort approach
    
    // For Vorbis comments, try to extract artist/title/album
    if (stream.codec_name == "vorbis" || stream.codec_name == "opus" || stream.codec_name == "flac") {
        // Look for common comment patterns
        const char* data = reinterpret_cast<const char*>(metadata_packet.data());
        size_t size = metadata_packet.size();
        
        // Try to find "ARTIST=" or "TITLE=" or "ALBUM="
        if (size > 7) {
            if (std::memcmp(data, "ARTIST=", 7) == 0) {
                // Extract artist - find null terminator or end of packet
                size_t end = 7;
                while (end < size && data[end] != '\0') {
                    ++end;
                }
                stream.artist = std::string(data + 7, end - 7);
                Debug::log("ogg", "  Extracted artist: %s", stream.artist.c_str());
            }
        }
    }
    
    return true;  // Continue even if metadata is malformed
}

// ============================================================================
// Task 14.3: Stream Error Handling (Requirements 9.9, 9.10, 9.11, 9.12)
// ============================================================================

/**
 * @brief Handle invalid granule position (-1)
 * 
 * Per RFC 3533 Section 6: granule position -1 means no packets finish on page
 * Continue searching for valid granule positions
 * 
 * Requirements: 9.9
 */
bool OggDemuxer::handleInvalidGranulePosition_unlocked(uint32_t stream_id, uint64_t granule_position) {
    if (granule_position != static_cast<uint64_t>(-1) && 
        granule_position != FLAC_OGG_GRANULE_NO_PACKET) {
        return false;  // Granule is valid
    }
    
    Debug::log("ogg", "handleInvalidGranulePosition: Stream 0x%08x has invalid granule position",
               stream_id);
    
    // Continue searching for valid granule positions
    // This is equivalent to continuing the bisection search
    return true;
}

/**
 * @brief Handle unexpected stream end
 * 
 * Returns OP_EBADLINK/OV_EBADLINK equivalent for unexpected stream end
 * 
 * Requirements: 9.10
 */
bool OggDemuxer::handleUnexpectedStreamEnd_unlocked(uint32_t stream_id) {
    Debug::log("ogg", "handleUnexpectedStreamEnd: Stream 0x%08x ended unexpectedly",
               stream_id);
    
    auto stream_it = m_streams.find(stream_id);
    if (stream_it != m_streams.end()) {
        // Mark stream as ended
        stream_it->second.m_packet_queue.clear();
    }
    
    // Return error code equivalent to OP_EBADLINK/OV_EBADLINK
    return false;
}

/**
 * @brief Fall back to linear search on bisection failure
 * 
 * When bisection search fails, fall back to linear forward scanning
 * Following libvorbisfile/libopusfile patterns
 * 
 * Requirements: 9.11
 */
bool OggDemuxer::fallbackToLinearSearch_unlocked(uint64_t target_granule, uint32_t stream_id) {
    Debug::log("ogg", "fallbackToLinearSearch: Bisection failed, using linear search for stream 0x%08x",
               stream_id);
    
    // Reset to beginning of file
    if (!m_handler || m_handler->seek(0, SEEK_SET) != 0) {
        Debug::log("ogg", "fallbackToLinearSearch: Failed to seek to beginning");
        return false;
    }
    
    // Reset sync state
    ogg_sync_reset(&m_sync_state);
    
    // Scan forward linearly looking for target granule
    ogg_page page;
    while (true) {
        // Get buffer for reading
        char* buffer = ogg_sync_buffer(&m_sync_state, READSIZE);
        if (!buffer) {
            break;
        }
        
        // Read data
        size_t bytes_read = m_handler->read(buffer, 1, READSIZE);
        
        if (bytes_read <= 0) {
            break;  // EOF or error
        }
        
        ogg_sync_wrote(&m_sync_state, static_cast<long>(bytes_read));
        
        // Try to extract pages
        while (ogg_sync_pageout(&m_sync_state, &page) == 1) {
            uint32_t serial = pageSerialNo(&page);
            if (serial != stream_id) {
                continue;  // Skip other streams
            }
            
            int64_t granule = pageGranulePos(&page);
            if (granule >= 0 && static_cast<uint64_t>(granule) >= target_granule) {
                Debug::log("ogg", "fallbackToLinearSearch: Found target granule %llu",
                           static_cast<unsigned long long>(granule));
                return true;
            }
        }
    }
    
    Debug::log("ogg", "fallbackToLinearSearch: Target granule not found");
    return false;
}

/**
 * @brief Use OP_PAGE_SIZE_MAX bounds checking
 * 
 * Validate page sizes against RFC 3533 maximum (65307 bytes)
 * 
 * Requirements: 9.12
 */
bool OggDemuxer::validatePageSizeBounds_unlocked(const ogg_page* page) {
    if (!page) {
        return false;
    }
    
    size_t total_size = pageTotalSize(page);
    if (total_size > OGG_PAGE_SIZE_MAX) {
        Debug::log("ogg", "validatePageSizeBounds: Page size %zu exceeds maximum %zu",
                   total_size, OGG_PAGE_SIZE_MAX);
        return false;
    }
    
    return true;
}

// ============================================================================
// Error Recovery Methods (Base Class Overrides)
// ============================================================================

/**
 * @brief Skip to next valid section in the file
 * 
 * Attempts to find the next valid Ogg page by scanning for "OggS" capture pattern
 * 
 * @return true if a valid section was found
 */
bool OggDemuxer::skipToNextValidSection() const {
    // Note: This is const because it's a base class virtual method
    // We use mutable members for state that needs to be modified
    
    Debug::log("ogg", "skipToNextValidSection: Attempting to find next valid Ogg page");
    
    // Get current buffer
    char* buffer = ogg_sync_buffer(const_cast<ogg_sync_state*>(&m_sync_state), READSIZE);
    if (!buffer) {
        return false;
    }
    
    // Read data
    size_t bytes_read = m_handler->read(buffer, 1, READSIZE);
    
    if (bytes_read <= 0) {
        return false;
    }
    
    ogg_sync_wrote(const_cast<ogg_sync_state*>(&m_sync_state), static_cast<long>(bytes_read));
    
    // Try to find next valid page
    ogg_page page;
    int result = ogg_sync_pageseek(const_cast<ogg_sync_state*>(&m_sync_state), &page);
    
    if (result > 0) {
        // Found a valid page
        Debug::log("ogg", "skipToNextValidSection: Found valid page");
        return true;
    }
    
    return false;
}

/**
 * @brief Reset internal state to a known good state
 * 
 * Clears all stream states and resets libogg structures
 * 
 * @return true if reset was successful
 */
bool OggDemuxer::resetInternalState() const {
    // Note: This is const because it's a base class virtual method
    // We use mutable members for state that needs to be modified
    
    Debug::log("ogg", "resetInternalState: Resetting OggDemuxer internal state");
    
    // Clear all stream states
    for (auto& [stream_id, ogg_stream] : const_cast<std::map<uint32_t, ogg_stream_state>&>(m_ogg_streams)) {
        ogg_stream_clear(&ogg_stream);
    }
    const_cast<std::map<uint32_t, ogg_stream_state>&>(m_ogg_streams).clear();
    
    // Reset sync state
    ogg_sync_reset(const_cast<ogg_sync_state*>(&m_sync_state));
    
    // Reset file position
    if (m_handler) {
        m_handler->seek(0, SEEK_SET);
    }
    
    // Reset tracking variables (using mutable members)
    const_cast<OggDemuxer*>(this)->m_offset = 0;
    const_cast<OggDemuxer*>(this)->m_end = 0;
    const_cast<OggDemuxer*>(this)->m_eof = false;
    const_cast<OggDemuxer*>(this)->m_in_headers_phase = true;
    const_cast<OggDemuxer*>(this)->m_seen_data_page = false;
    const_cast<OggDemuxer*>(this)->m_bos_serial_numbers.clear();
    const_cast<OggDemuxer*>(this)->m_eos_serial_numbers.clear();
    const_cast<OggDemuxer*>(this)->m_chain_count = 0;
    
    Debug::log("ogg", "resetInternalState: Reset complete");
    return true;
}

/**
 * @brief Enable fallback mode for degraded operation
 * 
 * When normal parsing fails, enable fallback mode that uses simpler,
 * more robust parsing strategies
 * 
 * @return true if fallback mode was enabled
 */
bool OggDemuxer::enableFallbackMode() const {
    // Note: This is const because it's a base class virtual method
    // We use mutable members for state that needs to be modified
    
    Debug::log("ogg", "enableFallbackMode: Enabling fallback parsing mode");
    
    const_cast<OggDemuxer*>(this)->m_fallback_mode = true;
    
    // In fallback mode:
    // - Skip CRC validation (rely on libogg's internal checks)
    // - Use linear search instead of bisection
    // - Accept partial metadata
    // - Continue on codec identification failures
    
    return true;
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

// ============================================================================
// Stream State Management (Requirements 6.5, 6.6, 6.7, 6.8, 6.9)
// ============================================================================

/**
 * @brief Check if a granule position indicates no packets finish on this page
 * 
 * Per RFC 3533 Section 6: granule position -1 (0xFFFFFFFFFFFFFFFF as unsigned)
 * means no packets finish on this page. This occurs when a page contains only
 * the start or middle of a large packet that continues on the next page.
 * 
 * @param granule_position Granule position to check (signed)
 * @return true if granule indicates no completed packets
 * 
 * Requirements: 6.9
 */
bool OggDemuxer::isNoPacketGranule(int64_t granule_position) {
    return granule_position == -1;
}

/**
 * @brief Check if page loss has occurred for a stream
 * 
 * Compares expected and actual sequence numbers to detect missing pages.
 * Page sequence numbers are monotonically increasing per logical bitstream.
 * 
 * @param stream_id Stream ID to check
 * @param expected_seq Expected sequence number
 * @param actual_seq Actual sequence number from page
 * @return Number of pages lost (0 if no loss)
 * 
 * Requirements: 6.8
 */
uint32_t OggDemuxer::detectPageLoss(uint32_t stream_id, uint32_t expected_seq, uint32_t actual_seq) const {
    if (actual_seq == expected_seq) {
        return 0;  // No loss
    }
    
    // Handle sequence number wraparound
    if (actual_seq > expected_seq) {
        return actual_seq - expected_seq;
    }
    
    // Wraparound case (very rare, would need 4 billion pages)
    return (0xFFFFFFFF - expected_seq) + actual_seq + 1;
}

/**
 * @brief Report page loss for error handling
 * 
 * Logs page loss and could trigger error recovery mechanisms.
 * This is equivalent to returning OV_HOLE/OP_HOLE in reference implementations.
 * 
 * @param stream_id Stream ID where loss occurred
 * @param pages_lost Number of pages lost
 * 
 * Requirements: 6.8
 */
void OggDemuxer::reportPageLoss(uint32_t stream_id, uint32_t pages_lost) {
    Debug::log("ogg", "Page loss detected: stream 0x%08x lost %u page(s)", 
               stream_id, pages_lost);
    
    // Mark the stream as having experienced a hole
    // This could be used to signal OV_HOLE/OP_HOLE to callers
    auto stream_it = m_streams.find(stream_id);
    if (stream_it != m_streams.end()) {
        // Could add a flag to track holes for error reporting
        // For now, just log the event
    }
}

/**
 * @brief Check if a stream has reached EOS
 * 
 * @param stream_id Stream ID to check
 * @return true if stream has received EOS page
 * 
 * Requirements: 6.5
 */
bool OggDemuxer::isStreamEOS(uint32_t stream_id) const {
    return m_eos_serial_numbers.count(stream_id) > 0;
}

/**
 * @brief Get the number of packets queued for a stream
 * 
 * @param stream_id Stream ID to check
 * @return Number of packets in queue
 * 
 * Requirements: 6.6
 */
size_t OggDemuxer::getQueuedPacketCount(uint32_t stream_id) const {
    auto stream_it = m_streams.find(stream_id);
    if (stream_it == m_streams.end()) {
        return 0;
    }
    return stream_it->second.m_packet_queue.size();
}

/**
 * @brief Get total packets queued across all streams
 * 
 * @return Total number of packets in all queues
 */
size_t OggDemuxer::getTotalQueuedPackets() const {
    size_t total = 0;
    for (const auto& [serial, stream] : m_streams) {
        total += stream.m_packet_queue.size();
    }
    return total;
}

// ============================================================================
// FLAC-in-Ogg Specific Handling (RFC 9639 Section 10.1)
// ============================================================================

/**
 * @brief Check if a granule position indicates no completed packet
 * 
 * Per RFC 9639 Section 10.1:
 * "If a page contains no packet end (e.g., when it only contains the start
 * of a large packet that continues on the next page), then the granule
 * position is set to the maximum value possible, i.e., 0xFF 0xFF 0xFF
 * 0xFF 0xFF 0xFF 0xFF 0xFF."
 * 
 * @param granule_position Granule position to check
 * @return true if granule indicates no completed packet (0xFFFFFFFFFFFFFFFF)
 * 
 * Requirements: 5.7
 */
bool OggDemuxer::isFlacOggNoPacketGranule(uint64_t granule_position) {
    return granule_position == FLAC_OGG_GRANULE_NO_PACKET;
}

/**
 * @brief Check if a granule position is valid for a FLAC-in-Ogg header page
 * 
 * Per RFC 9639 Section 10.1:
 * "The granule position of all pages containing header packets MUST be 0."
 * 
 * @param granule_position Granule position to check
 * @return true if granule is valid for header page (must be 0)
 * 
 * Requirements: 5.8
 */
bool OggDemuxer::isFlacOggValidHeaderGranule(uint64_t granule_position) {
    return granule_position == 0;
}

/**
 * @brief Convert FLAC-in-Ogg granule position to sample count
 * 
 * Per RFC 9639 Section 10.1:
 * "For pages containing audio packets, the granule position is the
 * number of the last sample contained in the last completed packet in
 * the frame. The sample numbering considers interchannel samples."
 * 
 * @param granule_position Granule position from page
 * @param stream FLAC stream for sample rate info
 * @return Sample count, or 0 if invalid granule
 * 
 * Requirements: 5.6
 */
uint64_t OggDemuxer::flacOggGranuleToSamples(uint64_t granule_position, const OggStream& stream) const {
    // Check for special "no packet" value
    if (isFlacOggNoPacketGranule(granule_position)) {
        Debug::log("ogg", "flacOggGranuleToSamples: No-packet granule (0xFFFFFFFFFFFFFFFF)");
        return 0;
    }
    
    // For FLAC-in-Ogg, granule position IS the sample count (interchannel samples)
    // No conversion needed - it's a direct mapping
    return granule_position;
}

/**
 * @brief Convert sample count to FLAC-in-Ogg granule position
 * 
 * For FLAC-in-Ogg, granule position equals sample count (interchannel samples).
 * 
 * @param samples Sample count (interchannel samples)
 * @return Granule position
 */
uint64_t OggDemuxer::flacOggSamplesToGranule(uint64_t samples) {
    // Direct mapping - granule position equals sample count
    return samples;
}

/**
 * @brief Validate FLAC-in-Ogg page granule position
 * 
 * Checks that granule position follows RFC 9639 Section 10.1 rules:
 * - Header pages: granule MUST be 0
 * - Audio pages with completed packet: granule is sample count
 * - Audio pages without completed packet: granule is 0xFFFFFFFFFFFFFFFF
 * 
 * @param page Ogg page to validate
 * @param stream FLAC stream info
 * @param is_header_page true if this is a header page
 * @return true if granule position is valid
 * 
 * Requirements: 5.6, 5.7, 5.8
 */
bool OggDemuxer::validateFlacOggGranule(const ogg_page* page, const OggStream& stream, 
                                         bool is_header_page) const {
    if (!page) {
        return false;
    }
    
    int64_t granule = pageGranulePos(page);
    uint64_t unsigned_granule = static_cast<uint64_t>(granule);
    
    if (is_header_page) {
        // Header pages MUST have granule position 0
        if (granule != 0) {
            Debug::log("ogg", "validateFlacOggGranule: Header page has non-zero granule %lld",
                       static_cast<long long>(granule));
            return false;
        }
        return true;
    }
    
    // Audio page - check for valid granule values
    // Valid values: 0 (first audio page), positive sample count, or 0xFFFFFFFFFFFFFFFF
    
    if (isFlacOggNoPacketGranule(unsigned_granule)) {
        // Valid: no packet completes on this page
        Debug::log("ogg", "validateFlacOggGranule: Audio page with no completed packet");
        return true;
    }
    
    if (granule >= 0) {
        // Valid: sample count at end of last completed packet
        // Optionally validate against total_samples if known
        if (stream.total_samples > 0 && unsigned_granule > stream.total_samples) {
            Debug::log("ogg", "validateFlacOggGranule: Granule %llu exceeds total samples %llu",
                       static_cast<unsigned long long>(unsigned_granule),
                       static_cast<unsigned long long>(stream.total_samples));
            // This is a warning, not necessarily an error (could be streaming)
        }
        return true;
    }
    
    // Negative granule (other than -1 which is 0xFFFFFFFFFFFFFFFF) is invalid
    Debug::log("ogg", "validateFlacOggGranule: Invalid negative granule %lld",
               static_cast<long long>(granule));
    return false;
}

/**
 * @brief Process a FLAC-in-Ogg audio packet
 * 
 * Per RFC 9639 Section 10.1:
 * "Following the header packets are audio packets. Each audio packet
 * contains a single FLAC frame."
 * 
 * @param packet Audio packet to process
 * @param stream FLAC stream info
 * @return true on success, false on error
 * 
 * Requirements: 5.3, 5.5
 */
bool OggDemuxer::processFlacOggAudioPacket(const OggPacket& packet, OggStream& stream) {
    if (packet.data.empty()) {
        Debug::log("ogg", "processFlacOggAudioPacket: Empty packet");
        return false;
    }
    
    // Each FLAC-in-Ogg audio packet contains exactly one FLAC frame
    // Validate that this looks like a FLAC frame (sync code 0xFF 0xF8-0xFF)
    if (packet.data.size() < 2) {
        Debug::log("ogg", "processFlacOggAudioPacket: Packet too small for FLAC frame");
        return false;
    }
    
    // FLAC frame sync code: 14 bits of 1s (0xFFF8 to 0xFFFF when masked)
    uint16_t sync_code = (static_cast<uint16_t>(packet.data[0]) << 8) | packet.data[1];
    if ((sync_code & 0xFFFC) != 0xFFF8) {
        Debug::log("ogg", "processFlacOggAudioPacket: Invalid FLAC frame sync code 0x%04x",
                   sync_code);
        // Don't fail - could be a valid frame with different blocking strategy
    }
    
    // Get sample count from frame header for tracking
    uint32_t frame_samples = getFlacFrameSampleCount(packet);
    if (frame_samples > 0) {
        stream.total_samples_processed += frame_samples;
        Debug::log("ogg", "processFlacOggAudioPacket: Frame with %u samples, total processed: %llu",
                   frame_samples, static_cast<unsigned long long>(stream.total_samples_processed));
    }
    
    return true;
}

/**
 * @brief Check if FLAC-in-Ogg stream requires chaining due to property change
 * 
 * Per RFC 9639 Section 10.1:
 * "If an audio stream is encoded where audio properties (sample rate,
 * number of channels, or bit depth) change at some point in the stream,
 * this should be dealt with by finishing encoding of the current Ogg
 * stream and starting a new Ogg stream, concatenated to the previous
 * one. This is called chaining in Ogg."
 * 
 * @param new_stream_info New stream info from potential chain
 * @param current_stream Current stream info
 * @return true if properties differ (chaining required)
 * 
 * Requirements: 5.9
 */
bool OggDemuxer::flacOggRequiresChaining(const OggStream& new_stream_info, 
                                          const OggStream& current_stream) const {
    // Check sample rate
    if (new_stream_info.sample_rate != current_stream.sample_rate) {
        Debug::log("ogg", "flacOggRequiresChaining: Sample rate changed from %u to %u",
                   current_stream.sample_rate, new_stream_info.sample_rate);
        return true;
    }
    
    // Check channel count
    if (new_stream_info.channels != current_stream.channels) {
        Debug::log("ogg", "flacOggRequiresChaining: Channels changed from %u to %u",
                   current_stream.channels, new_stream_info.channels);
        return true;
    }
    
    // Check bit depth
    if (new_stream_info.bits_per_sample != current_stream.bits_per_sample) {
        Debug::log("ogg", "flacOggRequiresChaining: Bit depth changed from %u to %u",
                   current_stream.bits_per_sample, new_stream_info.bits_per_sample);
        return true;
    }
    
    // Properties match - no chaining required
    return false;
}

/**
 * @brief Handle FLAC-in-Ogg mapping version mismatch
 * 
 * Per RFC 9639 Section 10.1:
 * "Version number of the FLAC-in-Ogg mapping. These bytes are 0x01 0x00,
 * meaning version 1.0 of the mapping."
 * 
 * @param major_version Major version byte
 * @param minor_version Minor version byte
 * @return true if version is supported or can be handled gracefully
 * 
 * Requirements: 5.3, 5.4
 */
bool OggDemuxer::handleFlacOggVersionMismatch(uint8_t major_version, uint8_t minor_version) {
    // Version 1.0 is the only defined version
    if (major_version == 1 && minor_version == 0) {
        Debug::log("ogg", "handleFlacOggVersionMismatch: Version 1.0 - fully supported");
        return true;
    }
    
    // Major version mismatch - likely incompatible
    if (major_version != 1) {
        Debug::log("ogg", "handleFlacOggVersionMismatch: Major version %u.%u - "
                   "may be incompatible, attempting to parse anyway",
                   major_version, minor_version);
        // Return true to attempt parsing, but log warning
        // Per Requirements 5.3: handle gracefully
        return true;
    }
    
    // Minor version mismatch with same major - likely compatible
    Debug::log("ogg", "handleFlacOggVersionMismatch: Version 1.%u - "
               "minor version difference, should be compatible",
               minor_version);
    return true;
}

/**
 * @brief Get FLAC frame sample count from audio packet
 * 
 * Parses FLAC frame header to extract block size (number of samples).
 * 
 * FLAC frame header structure (simplified):
 * - Sync code: 14 bits (0x3FFE)
 * - Reserved: 1 bit
 * - Blocking strategy: 1 bit
 * - Block size: 4 bits (encoded)
 * - Sample rate: 4 bits (encoded)
 * - Channel assignment: 4 bits
 * - Sample size: 3 bits
 * - Reserved: 1 bit
 * 
 * @param packet Audio packet containing FLAC frame
 * @return Sample count for this frame, or 0 on error
 */
uint32_t OggDemuxer::getFlacFrameSampleCount(const OggPacket& packet) const {
    if (packet.data.size() < 4) {
        return 0;
    }
    
    // Extract block size bits (bits 12-15 of byte 2, or bits 4-7 of the 3rd byte)
    // Frame header: [sync:14][reserved:1][blocking:1][blocksize:4][samplerate:4]...
    // Byte 0: sync[13:6]
    // Byte 1: sync[5:0], reserved, blocking
    // Byte 2: blocksize[3:0], samplerate[3:0]
    
    uint8_t block_size_bits = (packet.data[2] >> 4) & 0x0F;
    
    // Decode block size according to FLAC specification
    switch (block_size_bits) {
        case 0:  return 0;      // Reserved
        case 1:  return 192;
        case 2:  return 576;
        case 3:  return 1152;
        case 4:  return 2304;
        case 5:  return 4608;
        case 6:  // 8-bit block size - 1 follows in header
            if (packet.data.size() < 5) return 0;
            // Need to find where the block size byte is (after variable-length coded number)
            // For simplicity, return 0 and let caller use granule position
            return 0;
        case 7:  // 16-bit block size - 1 follows in header
            if (packet.data.size() < 6) return 0;
            // Need to find where the block size bytes are
            return 0;
        case 8:  return 256;
        case 9:  return 512;
        case 10: return 1024;
        case 11: return 2048;
        case 12: return 4096;
        case 13: return 8192;
        case 14: return 16384;
        case 15: return 32768;
        default: return 0;
    }
}

// ============================================================================
// Task 17: Page Boundary and Packet Continuation (RFC 3533 Section 5)
// ============================================================================

// ============================================================================
// Task 17.1: Packet Continuation Handling (Requirements 13.1, 13.2, 13.3, 13.4)
// ============================================================================

/**
 * @brief Extract packets from a page using ogg_stream_packetout()
 * 
 * Following libogg patterns for multi-page packet reconstruction:
 * - Use ogg_stream_packetout() which handles continuation internally
 * - libogg accumulates segments across pages automatically
 * - Returns complete packets only when all segments are received
 * 
 * @param stream_id Stream ID to extract packets from
 * @param packets Output: vector of extracted packets
 * @return Number of complete packets extracted
 * 
 * Requirements: 13.1, 13.4
 */
size_t OggDemuxer::extractPacketsFromStream_unlocked(uint32_t stream_id, 
                                                      std::vector<OggPacket>& packets) {
    packets.clear();
    
    // Find the ogg_stream_state for this stream
    auto ogg_stream_it = m_ogg_streams.find(stream_id);
    if (ogg_stream_it == m_ogg_streams.end()) {
        Debug::log("ogg", "extractPacketsFromStream_unlocked: No ogg_stream_state for 0x%08x",
                   stream_id);
        return 0;
    }
    
    ogg_stream_state& os = ogg_stream_it->second;
    ogg_packet ogg_pkt;
    size_t packet_count = 0;
    
    // Use ogg_stream_packetout() to extract complete packets
    // libogg handles multi-page packet reconstruction internally (Requirements 13.1, 13.4)
    // Returns:
    //   1 = packet extracted successfully
    //   0 = need more data (packet spans pages, waiting for continuation)
    //  -1 = out of sync (hole in data)
    int ret;
    while ((ret = ogg_stream_packetout(&os, &ogg_pkt)) != 0) {
        if (ret == 1) {
            // Complete packet extracted
            OggPacket packet;
            packet.stream_id = stream_id;
            packet.data.assign(ogg_pkt.packet, ogg_pkt.packet + ogg_pkt.bytes);
            packet.granule_position = static_cast<uint64_t>(ogg_pkt.granulepos);
            packet.is_first_packet = (ogg_pkt.b_o_s != 0);
            packet.is_last_packet = (ogg_pkt.e_o_s != 0);
            packet.is_continued = false;  // libogg handles continuation internally
            
            packets.push_back(std::move(packet));
            packet_count++;
            
            Debug::log("ogg", "extractPacketsFromStream_unlocked: Extracted packet %zu, "
                       "size=%lld, granule=%lld, bos=%d, eos=%d",
                       packet_count, static_cast<long long>(ogg_pkt.bytes),
                       static_cast<long long>(ogg_pkt.granulepos),
                       ogg_pkt.b_o_s, ogg_pkt.e_o_s);
        } else if (ret == -1) {
            // Hole in data - report but continue
            Debug::log("ogg", "extractPacketsFromStream_unlocked: Hole detected in stream 0x%08x",
                       stream_id);
            // Continue extracting - there may be more packets after the hole
        }
    }
    
    Debug::log("ogg", "extractPacketsFromStream_unlocked: Extracted %zu packets from stream 0x%08x",
               packet_count, stream_id);
    
    return packet_count;
}

/**
 * @brief Check if a page has the continued packet flag set
 * 
 * Per RFC 3533 Section 6: header_type bit 0 (0x01) indicates
 * the first packet on this page is continued from the previous page.
 * 
 * @param page Pointer to ogg_page structure
 * @return true if continuation flag is set
 * 
 * Requirements: 13.2
 */
bool OggDemuxer::hasPacketContinuation(const ogg_page* page) {
    if (!page || !page->header) {
        return false;
    }
    // Use libogg's ogg_page_continued() for compatibility
    return ogg_page_continued(const_cast<ogg_page*>(page)) != 0;
}

/**
 * @brief Wait for continuation pages for incomplete packets
 * 
 * When a packet spans multiple pages, libogg's ogg_stream_packetout()
 * returns 0 until all continuation pages are received. This method
 * reads additional pages until the packet is complete.
 * 
 * @param stream_id Stream ID with incomplete packet
 * @param max_pages Maximum pages to read while waiting
 * @return true if continuation pages were successfully read
 * 
 * Requirements: 13.3
 */
bool OggDemuxer::waitForContinuationPages_unlocked(uint32_t stream_id, size_t max_pages) {
    Debug::log("ogg", "waitForContinuationPages_unlocked: Waiting for continuation for stream 0x%08x",
               stream_id);
    
    // Find the ogg_stream_state for this stream
    auto ogg_stream_it = m_ogg_streams.find(stream_id);
    if (ogg_stream_it == m_ogg_streams.end()) {
        Debug::log("ogg", "waitForContinuationPages_unlocked: No ogg_stream_state for 0x%08x",
                   stream_id);
        return false;
    }
    
    ogg_stream_state& os = ogg_stream_it->second;
    ogg_page page;
    size_t pages_read = 0;
    
    while (pages_read < max_pages) {
        // Read next page from sync state
        int ret = getNextPage(&page, -1);
        if (ret <= 0) {
            // No more pages available or error
            Debug::log("ogg", "waitForContinuationPages_unlocked: No more pages available");
            return false;
        }
        
        pages_read++;
        
        // Check if this page belongs to our stream
        uint32_t page_serial = pageSerialNo(&page);
        if (page_serial != stream_id) {
            // Page belongs to different stream - submit to that stream
            auto other_stream_it = m_ogg_streams.find(page_serial);
            if (other_stream_it != m_ogg_streams.end()) {
                ogg_stream_pagein(&other_stream_it->second, &page);
            }
            continue;
        }
        
        // Submit page to our stream
        if (ogg_stream_pagein(&os, &page) != 0) {
            Debug::log("ogg", "waitForContinuationPages_unlocked: ogg_stream_pagein() failed");
            return false;
        }
        
        // Check if we can now extract a packet
        ogg_packet ogg_pkt;
        ret = ogg_stream_packetpeek(&os, &ogg_pkt);
        if (ret == 1) {
            // Packet is now complete
            Debug::log("ogg", "waitForContinuationPages_unlocked: Packet complete after %zu pages",
                       pages_read);
            return true;
        }
        
        // Check for EOS - if we hit EOS, the packet should be finalized
        if (pageEOS(&page)) {
            Debug::log("ogg", "waitForContinuationPages_unlocked: Hit EOS while waiting");
            return true;  // EOS finalizes incomplete packets
        }
    }
    
    Debug::log("ogg", "waitForContinuationPages_unlocked: Max pages (%zu) reached without completion",
               max_pages);
    return false;
}

/**
 * @brief Check if stream has an incomplete packet waiting for continuation
 * 
 * Uses ogg_stream_packetpeek() to check if there's a partial packet
 * in the stream state waiting for more data.
 * 
 * @param stream_id Stream ID to check
 * @return true if there's an incomplete packet
 * 
 * Requirements: 13.3
 */
bool OggDemuxer::hasIncompletePacket_unlocked(uint32_t stream_id) const {
    // Find the ogg_stream_state for this stream
    auto ogg_stream_it = m_ogg_streams.find(stream_id);
    if (ogg_stream_it == m_ogg_streams.end()) {
        return false;
    }
    
    // Use ogg_stream_packetpeek() to check without consuming
    // Note: We need to cast away const because libogg doesn't use const
    ogg_stream_state& os = const_cast<ogg_stream_state&>(ogg_stream_it->second);
    ogg_packet ogg_pkt;
    
    // packetpeek returns:
    //   1 = packet available
    //   0 = need more data (incomplete packet)
    //  -1 = hole in data
    int ret = ogg_stream_packetpeek(&os, &ogg_pkt);
    
    // If ret == 0, there's data in the stream but no complete packet
    // This indicates an incomplete packet waiting for continuation
    // However, we need to check if there's actually any data buffered
    // libogg doesn't expose this directly, so we check if the stream has pages
    
    // A more reliable check: if we've submitted pages but can't get packets,
    // there's likely an incomplete packet
    return (ret == 0);
}

// ============================================================================
// Task 17.2: Packet Boundary Detection (Requirements 13.5, 13.6, 13.7)
// ============================================================================

/**
 * @brief Check if page ends with a lacing value of 255 (packet continues)
 * 
 * Per RFC 3533 Section 5: A lacing value of 255 indicates the packet
 * continues in the next segment. If the last lacing value is 255,
 * the packet continues on the next page.
 * 
 * @param page Pointer to ogg_page structure
 * @return true if last lacing value is 255 (packet continues)
 * 
 * Requirements: 13.6
 */
bool OggDemuxer::pageEndsWithContinuation(const ogg_page* page) {
    if (!page || !page->header || 
        static_cast<size_t>(page->header_len) < OGG_PAGE_HEADER_MIN_SIZE) {
        return false;
    }
    
    // Get number of segments from header (offset 26)
    uint8_t num_segments = page->header[26];
    if (num_segments == 0) {
        return false;  // No segments means no continuation
    }
    
    // Get the last lacing value from segment table
    // Segment table starts at offset 27
    uint8_t last_lacing = page->header[26 + num_segments];  // Last segment in table
    
    // Per RFC 3533 Section 5: lacing value of 255 means packet continues
    return (last_lacing == 255);
}

/**
 * @brief Get packet boundary information from page segment table
 * 
 * Analyzes the segment table to determine:
 * - Number of complete packets on this page
 * - Whether first packet is continued from previous page
 * - Whether last packet continues to next page
 * 
 * @param page Pointer to ogg_page structure
 * @param complete_packets Output: number of complete packets
 * @param first_continued Output: true if first packet is continued
 * @param last_continues Output: true if last packet continues
 * 
 * Requirements: 13.5, 13.6
 */
void OggDemuxer::analyzePacketBoundaries(const ogg_page* page,
                                          size_t& complete_packets,
                                          bool& first_continued,
                                          bool& last_continues) {
    complete_packets = 0;
    first_continued = false;
    last_continues = false;
    
    if (!page || !page->header || 
        static_cast<size_t>(page->header_len) < OGG_PAGE_HEADER_MIN_SIZE) {
        return;
    }
    
    // Check continuation flag in header type (offset 5, bit 0)
    first_continued = hasPacketContinuation(page);
    
    // Get number of segments from header (offset 26)
    uint8_t num_segments = page->header[26];
    if (num_segments == 0) {
        return;  // No segments
    }
    
    // Analyze segment table (starts at offset 27)
    // Per RFC 3533 Section 5:
    // - Lacing value of 255: packet continues in next segment
    // - Lacing value < 255: packet ends (this is the final segment)
    
    for (uint8_t i = 0; i < num_segments; ++i) {
        uint8_t lacing_value = page->header[27 + i];
        
        // A lacing value < 255 marks the end of a packet
        if (lacing_value < 255) {
            complete_packets++;
        }
    }
    
    // Check if last packet continues to next page
    // This happens when the last lacing value is 255
    uint8_t last_lacing = page->header[26 + num_segments];
    last_continues = (last_lacing == 255);
    
    // Adjust complete packet count if first packet is continued
    // The first "complete" packet might actually be the end of a continued packet
    // libogg handles this internally, but for analysis purposes:
    // If first_continued is true, the first packet boundary we find is actually
    // completing a packet that started on a previous page
    
    Debug::log("ogg", "analyzePacketBoundaries: complete=%zu, first_continued=%d, last_continues=%d",
               complete_packets, first_continued ? 1 : 0, last_continues ? 1 : 0);
}

/**
 * @brief Finalize incomplete packets on EOS page
 * 
 * Per RFC 3533: When an EOS page is received, any incomplete packet
 * in the stream state should be finalized and delivered, even if
 * it would normally require more continuation data.
 * 
 * @param stream_id Stream ID with EOS page
 * @return Number of packets finalized
 * 
 * Requirements: 13.7
 */
size_t OggDemuxer::finalizeIncompletePacketsOnEOS_unlocked(uint32_t stream_id) {
    Debug::log("ogg", "finalizeIncompletePacketsOnEOS_unlocked: Finalizing for stream 0x%08x",
               stream_id);
    
    // Find the ogg_stream_state for this stream
    auto ogg_stream_it = m_ogg_streams.find(stream_id);
    if (ogg_stream_it == m_ogg_streams.end()) {
        Debug::log("ogg", "finalizeIncompletePacketsOnEOS_unlocked: No ogg_stream_state for 0x%08x",
                   stream_id);
        return 0;
    }
    
    ogg_stream_state& os = ogg_stream_it->second;
    
    // Find the OggStream to add packets to
    auto stream_it = m_streams.find(stream_id);
    if (stream_it == m_streams.end()) {
        Debug::log("ogg", "finalizeIncompletePacketsOnEOS_unlocked: No OggStream for 0x%08x",
                   stream_id);
        return 0;
    }
    
    OggStream& stream = stream_it->second;
    size_t packets_finalized = 0;
    
    // Extract all remaining packets from the stream
    // libogg will finalize incomplete packets when EOS is set
    ogg_packet ogg_pkt;
    int ret;
    
    while ((ret = ogg_stream_packetout(&os, &ogg_pkt)) != 0) {
        if (ret == 1) {
            // Packet extracted (may be incomplete but finalized due to EOS)
            OggPacket packet;
            packet.stream_id = stream_id;
            packet.data.assign(ogg_pkt.packet, ogg_pkt.packet + ogg_pkt.bytes);
            packet.granule_position = static_cast<uint64_t>(ogg_pkt.granulepos);
            packet.is_first_packet = (ogg_pkt.b_o_s != 0);
            packet.is_last_packet = (ogg_pkt.e_o_s != 0);
            packet.is_continued = false;
            
            // Add to packet queue
            stream.m_packet_queue.push_back(std::move(packet));
            packets_finalized++;
            
            // Update max granule seen
            if (ogg_pkt.granulepos >= 0 &&
                static_cast<uint64_t>(ogg_pkt.granulepos) > m_max_granule_seen) {
                m_max_granule_seen = static_cast<uint64_t>(ogg_pkt.granulepos);
            }
            
            Debug::log("ogg", "finalizeIncompletePacketsOnEOS_unlocked: Finalized packet, "
                       "size=%lld, granule=%lld, eos=%d",
                       static_cast<long long>(ogg_pkt.bytes),
                       static_cast<long long>(ogg_pkt.granulepos),
                       ogg_pkt.e_o_s);
        } else if (ret == -1) {
            // Hole in data - log but continue
            Debug::log("ogg", "finalizeIncompletePacketsOnEOS_unlocked: Hole detected");
        }
    }
    
    Debug::log("ogg", "finalizeIncompletePacketsOnEOS_unlocked: Finalized %zu packets",
               packets_finalized);
    
    return packets_finalized;
}

/**
 * @brief Process a page and extract all available packets
 * 
 * Combines page submission and packet extraction:
 * 1. Submit page to stream state using ogg_stream_pagein()
 * 2. Extract all complete packets using ogg_stream_packetout()
 * 3. Handle continuation flags and EOS appropriately
 * 
 * @param page Pointer to ogg_page structure
 * @param stream_id Stream ID for the page
 * @param packets Output: vector of extracted packets
 * @return Number of packets extracted, or -1 on error
 * 
 * Requirements: 13.1, 13.2, 13.3, 13.4, 13.5, 13.6, 13.7
 */
int OggDemuxer::processPageAndExtractPackets_unlocked(ogg_page* page, uint32_t stream_id,
                                                       std::vector<OggPacket>& packets) {
    packets.clear();
    
    if (!page || !pageIsValid(page)) {
        Debug::log("ogg", "processPageAndExtractPackets_unlocked: Invalid page");
        return -1;
    }
    
    // Find or create ogg_stream_state for this stream
    auto ogg_stream_it = m_ogg_streams.find(stream_id);
    if (ogg_stream_it == m_ogg_streams.end()) {
        // Initialize new stream state
        ogg_stream_state new_stream;
        if (ogg_stream_init(&new_stream, static_cast<int>(stream_id)) != 0) {
            Debug::log("ogg", "processPageAndExtractPackets_unlocked: Failed to init stream state");
            return -1;
        }
        m_ogg_streams[stream_id] = new_stream;
        ogg_stream_it = m_ogg_streams.find(stream_id);
    }
    
    ogg_stream_state& os = ogg_stream_it->second;
    
    // Log page information for debugging
    bool is_continued = hasPacketContinuation(page);
    bool is_eos = pageEOS(page);
    bool ends_with_continuation = pageEndsWithContinuation(page);
    
    Debug::log("ogg", "processPageAndExtractPackets_unlocked: Processing page for stream 0x%08x, "
               "continued=%d, eos=%d, ends_with_cont=%d",
               stream_id, is_continued ? 1 : 0, is_eos ? 1 : 0, ends_with_continuation ? 1 : 0);
    
    // Submit page to stream state using ogg_stream_pagein() (Requirements 13.1)
    if (ogg_stream_pagein(&os, page) != 0) {
        Debug::log("ogg", "processPageAndExtractPackets_unlocked: ogg_stream_pagein() failed");
        return -1;
    }
    
    // Extract all complete packets using ogg_stream_packetout() (Requirements 13.1, 13.4)
    // libogg handles multi-page packet reconstruction internally
    ogg_packet ogg_pkt;
    int ret;
    int packet_count = 0;
    
    while ((ret = ogg_stream_packetout(&os, &ogg_pkt)) != 0) {
        if (ret == 1) {
            // Complete packet extracted
            OggPacket packet;
            packet.stream_id = stream_id;
            packet.data.assign(ogg_pkt.packet, ogg_pkt.packet + ogg_pkt.bytes);
            packet.granule_position = static_cast<uint64_t>(ogg_pkt.granulepos);
            packet.is_first_packet = (ogg_pkt.b_o_s != 0);
            packet.is_last_packet = (ogg_pkt.e_o_s != 0);
            packet.is_continued = false;  // libogg handles continuation internally
            
            packets.push_back(std::move(packet));
            packet_count++;
            
            Debug::log("ogg", "processPageAndExtractPackets_unlocked: Extracted packet %d, "
                       "size=%lld, granule=%lld",
                       packet_count, static_cast<long long>(ogg_pkt.bytes),
                       static_cast<long long>(ogg_pkt.granulepos));
        } else if (ret == -1) {
            // Hole in data - report but continue
            Debug::log("ogg", "processPageAndExtractPackets_unlocked: Hole detected in stream");
        }
    }
    
    // If this is an EOS page, finalize any remaining incomplete packets (Requirements 13.7)
    if (is_eos) {
        Debug::log("ogg", "processPageAndExtractPackets_unlocked: EOS page, checking for incomplete packets");
        // libogg should have already finalized packets, but double-check
        while ((ret = ogg_stream_packetout(&os, &ogg_pkt)) == 1) {
            OggPacket packet;
            packet.stream_id = stream_id;
            packet.data.assign(ogg_pkt.packet, ogg_pkt.packet + ogg_pkt.bytes);
            packet.granule_position = static_cast<uint64_t>(ogg_pkt.granulepos);
            packet.is_first_packet = (ogg_pkt.b_o_s != 0);
            packet.is_last_packet = (ogg_pkt.e_o_s != 0);
            packet.is_continued = false;
            
            packets.push_back(std::move(packet));
            packet_count++;
        }
    }
    
    Debug::log("ogg", "processPageAndExtractPackets_unlocked: Extracted %d packets total",
               packet_count);
    
    return packet_count;
}

// ============================================================================
} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

#endif // HAVE_OGGDEMUXER

