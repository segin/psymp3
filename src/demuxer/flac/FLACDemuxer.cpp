/*
 * FLACDemuxer.cpp - FLAC container demuxer implementation (RFC 9639 compliant)
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace Demuxer {
namespace FLAC {

// Debug logging macro with method identification token
#define FLAC_DEBUG(...) Debug::log("flac", "[", __FUNCTION__, "] ", __VA_ARGS__)

// ============================================================================
// Constructor and Destructor
// ============================================================================

FLACDemuxer::FLACDemuxer(std::unique_ptr<IOHandler> handler)
    : Demuxer(std::move(handler))
{
    FLAC_DEBUG("Constructor called");
    
    // Get file size if available
    if (m_handler) {
        long size = m_handler->getFileSize();
        m_file_size = (size > 0) ? static_cast<uint64_t>(size) : 0;
        FLAC_DEBUG("File size: ", m_file_size, " bytes");
    }
}

FLACDemuxer::~FLACDemuxer()
{
    FLAC_DEBUG("Destructor called");
    
    // Acquire locks to ensure no operations in progress
    std::lock_guard<std::mutex> state_lock(m_state_mutex);
    std::lock_guard<std::mutex> metadata_lock(m_metadata_mutex);
    
    // Clear metadata
    m_seektable.clear();
    m_vorbis_comments.clear();
    m_pictures.clear();
}


// ============================================================================
// Public Interface Methods (acquire locks, call _unlocked implementations)
// ============================================================================

bool FLACDemuxer::parseContainer()
{
    std::lock_guard<std::mutex> state_lock(m_state_mutex);
    std::lock_guard<std::mutex> metadata_lock(m_metadata_mutex);
    return parseContainer_unlocked();
}

std::vector<StreamInfo> FLACDemuxer::getStreams() const
{
    std::lock_guard<std::mutex> state_lock(m_state_mutex);
    std::lock_guard<std::mutex> metadata_lock(m_metadata_mutex);
    return getStreams_unlocked();
}

StreamInfo FLACDemuxer::getStreamInfo(uint32_t stream_id) const
{
    std::lock_guard<std::mutex> state_lock(m_state_mutex);
    std::lock_guard<std::mutex> metadata_lock(m_metadata_mutex);
    return getStreamInfo_unlocked(stream_id);
}

MediaChunk FLACDemuxer::readChunk()
{
    std::lock_guard<std::mutex> state_lock(m_state_mutex);
    return readChunk_unlocked();
}

MediaChunk FLACDemuxer::readChunk(uint32_t stream_id)
{
    std::lock_guard<std::mutex> state_lock(m_state_mutex);
    
    // FLAC has only one stream (stream_id = 1)
    if (stream_id != 1) {
        FLAC_DEBUG("Invalid stream_id: ", stream_id, " (FLAC only has stream 1)");
        return MediaChunk{};
    }
    
    return readChunk_unlocked();
}

bool FLACDemuxer::seekTo(uint64_t timestamp_ms)
{
    std::lock_guard<std::mutex> state_lock(m_state_mutex);
    return seekTo_unlocked(timestamp_ms);
}

bool FLACDemuxer::isEOF() const
{
    std::lock_guard<std::mutex> state_lock(m_state_mutex);
    return isEOF_unlocked();
}

uint64_t FLACDemuxer::getDuration() const
{
    std::lock_guard<std::mutex> metadata_lock(m_metadata_mutex);
    return getDuration_unlocked();
}

uint64_t FLACDemuxer::getPosition() const
{
    std::lock_guard<std::mutex> state_lock(m_state_mutex);
    return getPosition_unlocked();
}


// ============================================================================
// Private Unlocked Implementations (assume locks are held)
// ============================================================================

bool FLACDemuxer::parseContainer_unlocked()
{
    FLAC_DEBUG("Starting FLAC container parsing");
    
    if (m_container_parsed) {
        FLAC_DEBUG("Container already parsed");
        return true;
    }
    
    if (!m_handler) {
        reportError("IO", "No IOHandler available");
        return false;
    }
    
    // Seek to beginning
    if (m_handler->seek(0, SEEK_SET) != 0) {
        reportError("IO", "Failed to seek to beginning of file");
        return false;
    }
    
    // Validate fLaC stream marker (RFC 9639 Section 6)
    if (!validateStreamMarker_unlocked()) {
        return false;
    }
    
    // Parse metadata blocks (RFC 9639 Section 8)
    if (!parseMetadataBlocks_unlocked()) {
        return false;
    }
    
    // Verify STREAMINFO is valid
    if (!m_streaminfo.isValid()) {
        reportError("Format", "Invalid or missing STREAMINFO block");
        return false;
    }
    
    m_container_parsed = true;
    m_current_offset = m_audio_data_offset;
    m_current_sample = 0;
    m_eof = false;
    
    FLAC_DEBUG("Container parsing complete");
    FLAC_DEBUG("  Sample rate: ", m_streaminfo.sample_rate, " Hz");
    FLAC_DEBUG("  Channels: ", static_cast<int>(m_streaminfo.channels));
    FLAC_DEBUG("  Bits per sample: ", static_cast<int>(m_streaminfo.bits_per_sample));
    FLAC_DEBUG("  Total samples: ", m_streaminfo.total_samples);
    FLAC_DEBUG("  Audio data offset: ", m_audio_data_offset);
    
    return true;
}

std::vector<StreamInfo> FLACDemuxer::getStreams_unlocked() const
{
    if (!m_container_parsed || !m_streaminfo.isValid()) {
        return {};
    }
    
    StreamInfo stream;
    stream.stream_id = 1;
    stream.codec_type = "audio";
    stream.codec_name = "flac";
    stream.sample_rate = m_streaminfo.sample_rate;
    stream.channels = m_streaminfo.channels;
    stream.bits_per_sample = m_streaminfo.bits_per_sample;
    stream.duration_samples = m_streaminfo.total_samples;
    stream.duration_ms = m_streaminfo.getDurationMs();
    
    // Add metadata from Vorbis comments
    auto it = m_vorbis_comments.find("ARTIST");
    if (it != m_vorbis_comments.end()) stream.artist = it->second;
    
    it = m_vorbis_comments.find("TITLE");
    if (it != m_vorbis_comments.end()) stream.title = it->second;
    
    it = m_vorbis_comments.find("ALBUM");
    if (it != m_vorbis_comments.end()) stream.album = it->second;
    
    return {stream};
}

StreamInfo FLACDemuxer::getStreamInfo_unlocked(uint32_t stream_id) const
{
    if (stream_id != 1) {
        return StreamInfo{};
    }
    
    auto streams = getStreams_unlocked();
    return streams.empty() ? StreamInfo{} : streams[0];
}


MediaChunk FLACDemuxer::readChunk_unlocked()
{
    if (!m_container_parsed) {
        FLAC_DEBUG("Container not parsed");
        return MediaChunk{};
    }
    
    if (isEOF_unlocked()) {
        FLAC_DEBUG("At end of file");
        return MediaChunk{};
    }
    
    // Find next FLAC frame
    FLACFrame frame;
    if (!findNextFrame_unlocked(frame)) {
        FLAC_DEBUG("No more frames found");
        m_eof = true;
        return MediaChunk{};
    }
    
    // Calculate frame size using STREAMINFO minimum frame size
    uint32_t frame_size = calculateFrameSize_unlocked(frame);
    
    // Validate frame size
    if (frame_size == 0 || frame_size > 1024 * 1024) {
        FLAC_DEBUG("Invalid frame size: ", frame_size);
        m_eof = true;
        return MediaChunk{};
    }
    
    // Check available data
    if (m_file_size > 0 && frame.file_offset + frame_size > m_file_size) {
        frame_size = static_cast<uint32_t>(m_file_size - frame.file_offset);
    }
    
    // Seek to frame position
    if (m_handler->seek(static_cast<off_t>(frame.file_offset), SEEK_SET) != 0) {
        reportError("IO", "Failed to seek to frame position");
        return MediaChunk{};
    }
    
    // Read frame data
    std::vector<uint8_t> data(frame_size);
    size_t bytes_read = m_handler->read(data.data(), 1, frame_size);
    
    if (bytes_read == 0) {
        FLAC_DEBUG("No data read - end of file");
        m_eof = true;
        return MediaChunk{};
    }
    
    data.resize(bytes_read);
    
    // Create MediaChunk
    MediaChunk chunk(1, std::move(data));
    chunk.timestamp_samples = frame.sample_offset;
    chunk.is_keyframe = true;  // All FLAC frames are keyframes
    chunk.file_offset = frame.file_offset;
    
    // Update position
    m_current_sample = frame.sample_offset + frame.block_size;
    m_current_offset = frame.file_offset + bytes_read;
    
    FLAC_DEBUG("Read frame: ", bytes_read, " bytes, samples ", 
               frame.sample_offset, "-", m_current_sample);
    
    return chunk;
}

bool FLACDemuxer::seekTo_unlocked(uint64_t timestamp_ms)
{
    FLAC_DEBUG("Seeking to ", timestamp_ms, " ms");
    
    if (!m_container_parsed) {
        reportError("State", "Container not parsed");
        return false;
    }
    
    // Seek to beginning
    if (timestamp_ms == 0) {
        m_current_offset = m_audio_data_offset;
        m_current_sample = 0;
        m_eof = false;
        
        if (m_handler->seek(static_cast<off_t>(m_audio_data_offset), SEEK_SET) != 0) {
            reportError("IO", "Failed to seek to beginning");
            return false;
        }
        
        FLAC_DEBUG("Seeked to beginning");
        return true;
    }
    
    // Convert to samples
    uint64_t target_sample = msToSamples(timestamp_ms);
    
    // Clamp to stream bounds
    if (m_streaminfo.total_samples > 0 && target_sample >= m_streaminfo.total_samples) {
        target_sample = m_streaminfo.total_samples - 1;
    }
    
    // Use SEEKTABLE if available
    if (!m_seektable.empty()) {
        // Find closest seek point not exceeding target
        const FLACSeekPoint* best = nullptr;
        for (const auto& point : m_seektable) {
            if (point.isPlaceholder()) continue;
            if (point.sample_number <= target_sample) {
                if (!best || point.sample_number > best->sample_number) {
                    best = &point;
                }
            }
        }
        
        if (best) {
            uint64_t seek_offset = m_audio_data_offset + best->stream_offset;
            
            if (m_handler->seek(static_cast<off_t>(seek_offset), SEEK_SET) != 0) {
                reportError("IO", "Failed to seek using SEEKTABLE");
                return false;
            }
            
            m_current_offset = seek_offset;
            m_current_sample = best->sample_number;
            m_eof = false;
            
            FLAC_DEBUG("Seeked using SEEKTABLE to sample ", m_current_sample);
            return true;
        }
    }
    
    // Fallback: seek to beginning
    FLAC_DEBUG("No SEEKTABLE, seeking to beginning");
    m_current_offset = m_audio_data_offset;
    m_current_sample = 0;
    m_eof = false;
    
    return m_handler->seek(static_cast<off_t>(m_audio_data_offset), SEEK_SET) == 0;
}

bool FLACDemuxer::isEOF_unlocked() const
{
    if (m_eof) return true;
    if (!m_handler) return true;
    if (m_file_size > 0 && m_current_offset >= m_file_size) return true;
    return m_handler->eof();
}

uint64_t FLACDemuxer::getDuration_unlocked() const
{
    return m_streaminfo.getDurationMs();
}

uint64_t FLACDemuxer::getPosition_unlocked() const
{
    return samplesToMs(m_current_sample);
}


// ============================================================================
// Stream Marker Validation (RFC 9639 Section 6)
// ============================================================================

bool FLACDemuxer::validateStreamMarker_unlocked()
{
    FLAC_DEBUG("Validating fLaC stream marker (RFC 9639 Section 6)");
    
    // Requirement 1.1: Read first 4 bytes of file
    uint8_t marker[4];
    size_t bytes_read = m_handler->read(marker, 1, 4);
    
    if (bytes_read < 4) {
        FLAC_DEBUG("File too small - only read ", bytes_read, " bytes, expected 4");
        reportError("Format", "File too small - cannot read stream marker");
        return false;
    }
    
    // Requirement 1.2: Verify bytes equal 0x66 0x4C 0x61 0x43 ("fLaC")
    // RFC 9639 Section 6: Stream marker must be exactly these 4 bytes
    static constexpr uint8_t EXPECTED_MARKER[4] = {0x66, 0x4C, 0x61, 0x43};
    
    if (marker[0] != EXPECTED_MARKER[0] || marker[1] != EXPECTED_MARKER[1] || 
        marker[2] != EXPECTED_MARKER[2] || marker[3] != EXPECTED_MARKER[3]) {
        
        // Requirement 1.5: Log exact byte values found versus expected
        FLAC_DEBUG("Stream marker validation FAILED");
        FLAC_DEBUG("  Expected: 0x66 0x4C 0x61 0x43 ('f' 'L' 'a' 'C')");
        FLAC_DEBUG("  Found:    0x", std::hex, 
                   static_cast<int>(marker[0]), " 0x",
                   static_cast<int>(marker[1]), " 0x",
                   static_cast<int>(marker[2]), " 0x",
                   static_cast<int>(marker[3]), std::dec,
                   " ('", static_cast<char>(marker[0] >= 32 && marker[0] < 127 ? marker[0] : '.'),
                   "' '", static_cast<char>(marker[1] >= 32 && marker[1] < 127 ? marker[1] : '.'),
                   "' '", static_cast<char>(marker[2] >= 32 && marker[2] < 127 ? marker[2] : '.'),
                   "' '", static_cast<char>(marker[3] >= 32 && marker[3] < 127 ? marker[3] : '.'), "')");
        
        // Requirement 1.3: Reject invalid markers without crashing
        reportError("Format", "Invalid FLAC stream marker - not a FLAC file");
        return false;
    }
    
    // Requirement 1.4: When stream marker is valid, proceed to metadata block parsing
    FLAC_DEBUG("Valid fLaC stream marker found at file offset 0");
    return true;
}

// ============================================================================
// Metadata Block Parsing (RFC 9639 Section 8)
// ============================================================================

bool FLACDemuxer::parseMetadataBlocks_unlocked()
{
    FLAC_DEBUG("Parsing metadata blocks");
    
    bool found_streaminfo = false;
    bool is_first_block = true;  // Track if this is the first metadata block
    bool is_last = false;
    
    while (!is_last) {
        FLACMetadataBlock block;
        
        if (!parseMetadataBlockHeader_unlocked(block)) {
            reportError("Format", "Failed to parse metadata block header");
            return false;
        }
        
        is_last = block.is_last;
        
        FLAC_DEBUG("Metadata block: type=", static_cast<int>(block.type), 
                   ", length=", block.length, ", is_last=", is_last,
                   ", is_first=", is_first_block);
        
        // Process block based on type
        // RFC 9639 Section 8.1: Block types 0-6 are defined, 7-126 are reserved, 127 is forbidden
        uint8_t raw_type = static_cast<uint8_t>(block.type);
        
        // Requirement 3.1, 3.2: STREAMINFO must be the first metadata block
        // RFC 9639 Section 8.2: "The STREAMINFO block MUST be the first metadata block"
        if (is_first_block) {
            if (block.type != FLACMetadataType::STREAMINFO) {
                FLAC_DEBUG("First metadata block is not STREAMINFO (type=", 
                           static_cast<int>(raw_type), ") - rejecting stream");
                reportError("Format", "STREAMINFO must be the first metadata block (RFC 9639 Section 8.2)");
                return false;
            }
        }
        
        // Requirement 2.4, 18.1: Block type 127 is forbidden (already checked in parseMetadataBlockHeader_unlocked)
        // Requirement 2.13: Reserved block types 7-126 should be skipped
        if (raw_type >= 7 && raw_type <= 126) {
            FLAC_DEBUG("Skipping reserved metadata block type ", static_cast<int>(raw_type), 
                       " (length=", block.length, " bytes)");
            skipMetadataBlock_unlocked(block);
            is_first_block = false;
            continue;
        }
        
        switch (block.type) {
            case FLACMetadataType::STREAMINFO:
                if (found_streaminfo) {
                    reportError("Format", "Multiple STREAMINFO blocks found");
                    return false;
                }
                if (!parseStreamInfoBlock_unlocked(block)) {
                    return false;
                }
                found_streaminfo = true;
                break;
                
            case FLACMetadataType::SEEKTABLE:
                parseSeekTableBlock_unlocked(block);
                break;
                
            case FLACMetadataType::VORBIS_COMMENT:
                parseVorbisCommentBlock_unlocked(block);
                break;
                
            case FLACMetadataType::PADDING:
                // Requirement 14.1-14.4: Handle PADDING blocks per RFC 9639 Section 8.3
                parsePaddingBlock_unlocked(block);
                break;
                
            case FLACMetadataType::APPLICATION:
                // Requirement 15.1-15.4: Handle APPLICATION blocks per RFC 9639 Section 8.4
                parseApplicationBlock_unlocked(block);
                break;
                
            case FLACMetadataType::CUESHEET:
                // Requirement 16.1-16.8: Handle CUESHEET blocks per RFC 9639 Section 8.7
                parseCuesheetBlock_unlocked(block);
                break;
                
            case FLACMetadataType::PICTURE:
                // Requirement 17.1-17.12: Handle PICTURE blocks per RFC 9639 Section 8.8
                parsePictureBlock_unlocked(block);
                break;
                
            default:
                // Should not reach here due to reserved type check above
                FLAC_DEBUG("Unexpected metadata block type ", static_cast<int>(raw_type));
                skipMetadataBlock_unlocked(block);
                break;
        }
        
        is_first_block = false;  // After processing any block, it's no longer the first
    }
    
    // RFC 9639: STREAMINFO must be first metadata block
    if (!found_streaminfo) {
        reportError("Format", "Missing STREAMINFO block");
        return false;
    }
    
    // Record where audio data starts
    m_audio_data_offset = static_cast<uint64_t>(m_handler->tell());
    FLAC_DEBUG("Audio data starts at offset ", m_audio_data_offset);
    
    return true;
}

bool FLACDemuxer::parseMetadataBlockHeader_unlocked(FLACMetadataBlock& block)
{
    // RFC 9639 Section 8.1: 4-byte metadata block header
    // Requirement 2.1: Read 4 bytes
    uint8_t header[4];
    
    if (m_handler->read(header, 1, 4) != 4) {
        FLAC_DEBUG("Failed to read 4-byte metadata block header");
        return false;
    }
    
    // Requirement 2.2: Extract bit 7 as is_last flag
    block.is_last = (header[0] & 0x80) != 0;
    
    // Requirement 2.3: Extract bits 0-6 as block type
    uint8_t type = header[0] & 0x7F;
    
    // Requirement 2.4, 18.1: RFC 9639 Table 1 - Block type 127 is forbidden
    if (type == 127) {
        FLAC_DEBUG("Forbidden metadata block type 127 detected - rejecting stream");
        reportError("Format", "Forbidden metadata block type 127 (RFC 9639 Table 1)");
        return false;
    }
    
    block.type = static_cast<FLACMetadataType>(type);
    
    // Requirement 2.5: Extract bytes 1-3 as 24-bit big-endian block length
    block.length = (static_cast<uint32_t>(header[1]) << 16) |
                   (static_cast<uint32_t>(header[2]) << 8) |
                   static_cast<uint32_t>(header[3]);
    
    block.data_offset = static_cast<uint64_t>(m_handler->tell());
    
    FLAC_DEBUG("Parsed metadata block header: type=", static_cast<int>(type),
               ", is_last=", block.is_last, ", length=", block.length,
               ", data_offset=", block.data_offset);
    
    return true;
}


// ============================================================================
// STREAMINFO Block Parsing (RFC 9639 Section 8.2)
// ============================================================================

bool FLACDemuxer::parseStreamInfoBlock_unlocked(const FLACMetadataBlock& block)
{
    FLAC_DEBUG("[parseStreamInfo] Parsing STREAMINFO block (RFC 9639 Section 8.2)");
    
    // Requirement 3.3: Read exactly 34 bytes of data
    // RFC 9639 Section 8.2: STREAMINFO is exactly 34 bytes
    if (block.length != 34) {
        FLAC_DEBUG("[parseStreamInfo] Invalid STREAMINFO length: ", block.length, " (expected 34)");
        reportError("Format", "Invalid STREAMINFO length: " + std::to_string(block.length) + " (expected 34)");
        return false;
    }
    
    uint8_t data[34];
    if (m_handler->read(data, 1, 34) != 34) {
        FLAC_DEBUG("[parseStreamInfo] Failed to read 34 bytes of STREAMINFO data");
        reportError("IO", "Failed to read STREAMINFO data");
        return false;
    }
    
    // Parse STREAMINFO fields (all big-endian per RFC 9639 Section 5)
    
    // Requirement 3.4: Extract u(16) minimum block size big-endian
    // Bytes 0-1: minimum block size (u16)
    m_streaminfo.min_block_size = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    FLAC_DEBUG("[parseStreamInfo] min_block_size: ", m_streaminfo.min_block_size);
    
    // Requirement 3.5: Extract u(16) maximum block size big-endian
    // Bytes 2-3: maximum block size (u16)
    m_streaminfo.max_block_size = (static_cast<uint16_t>(data[2]) << 8) | data[3];
    FLAC_DEBUG("[parseStreamInfo] max_block_size: ", m_streaminfo.max_block_size);
    
    // Requirement 3.9: Extract u(24) minimum frame size big-endian
    // Bytes 4-6: minimum frame size (u24)
    m_streaminfo.min_frame_size = (static_cast<uint32_t>(data[4]) << 16) |
                                   (static_cast<uint32_t>(data[5]) << 8) |
                                   data[6];
    // Requirement 3.11: If minimum frame size is 0, treat as unknown
    if (m_streaminfo.min_frame_size == 0) {
        FLAC_DEBUG("[parseStreamInfo] min_frame_size: 0 (unknown)");
    } else {
        FLAC_DEBUG("[parseStreamInfo] min_frame_size: ", m_streaminfo.min_frame_size);
    }
    
    // Requirement 3.10: Extract u(24) maximum frame size big-endian
    // Bytes 7-9: maximum frame size (u24)
    m_streaminfo.max_frame_size = (static_cast<uint32_t>(data[7]) << 16) |
                                   (static_cast<uint32_t>(data[8]) << 8) |
                                   data[9];
    // Requirement 3.12: If maximum frame size is 0, treat as unknown
    if (m_streaminfo.max_frame_size == 0) {
        FLAC_DEBUG("[parseStreamInfo] max_frame_size: 0 (unknown)");
    } else {
        FLAC_DEBUG("[parseStreamInfo] max_frame_size: ", m_streaminfo.max_frame_size);
    }
    
    // Requirement 3.13: Extract u(20) sample rate big-endian
    // Bytes 10-13: sample rate (u20), channels-1 (u3), bits_per_sample-1 (u5)
    // Byte 10: sample_rate[19:12]
    // Byte 11: sample_rate[11:4]
    // Byte 12: sample_rate[3:0], channels-1[2:0], bits_per_sample-1[4]
    // Byte 13: bits_per_sample-1[3:0], total_samples[35:32]
    
    m_streaminfo.sample_rate = (static_cast<uint32_t>(data[10]) << 12) |
                                (static_cast<uint32_t>(data[11]) << 4) |
                                (data[12] >> 4);
    FLAC_DEBUG("[parseStreamInfo] sample_rate: ", m_streaminfo.sample_rate, " Hz");
    
    // Requirement 3.15: Extract u(3) channels-1 and add 1
    m_streaminfo.channels = ((data[12] >> 1) & 0x07) + 1;
    FLAC_DEBUG("[parseStreamInfo] channels: ", static_cast<int>(m_streaminfo.channels));
    
    // Requirement 3.16: Extract u(5) bits_per_sample-1 and add 1
    m_streaminfo.bits_per_sample = (((data[12] & 0x01) << 4) | (data[13] >> 4)) + 1;
    FLAC_DEBUG("[parseStreamInfo] bits_per_sample: ", static_cast<int>(m_streaminfo.bits_per_sample));
    
    // Requirement 3.17: Extract u(36) total samples big-endian
    // Bytes 13-17: total samples (u36)
    m_streaminfo.total_samples = (static_cast<uint64_t>(data[13] & 0x0F) << 32) |
                                  (static_cast<uint64_t>(data[14]) << 24) |
                                  (static_cast<uint64_t>(data[15]) << 16) |
                                  (static_cast<uint64_t>(data[16]) << 8) |
                                  data[17];
    // Requirement 3.18: If total samples is 0, treat stream length as unknown
    if (m_streaminfo.total_samples == 0) {
        FLAC_DEBUG("[parseStreamInfo] total_samples: 0 (unknown duration)");
    } else {
        FLAC_DEBUG("[parseStreamInfo] total_samples: ", m_streaminfo.total_samples);
    }
    
    // Requirement 3.19: Extract 128-bit MD5 checksum
    // Bytes 18-33: MD5 signature (128 bits)
    std::memcpy(m_streaminfo.md5_signature, &data[18], 16);
    
    // Requirement 3.20: If MD5 checksum is all zeros, treat as unavailable
    bool md5_available = false;
    for (int i = 0; i < 16; ++i) {
        if (m_streaminfo.md5_signature[i] != 0) {
            md5_available = true;
            break;
        }
    }
    if (md5_available) {
        FLAC_DEBUG("[parseStreamInfo] MD5 signature present");
    } else {
        FLAC_DEBUG("[parseStreamInfo] MD5 signature: all zeros (unavailable)");
    }
    
    // ========================================================================
    // STREAMINFO Validation (Requirements 3.6-3.8, 3.14)
    // ========================================================================
    
    // Requirement 3.6: Reject if minimum block size < 16 (RFC 9639 Table 1 forbidden pattern)
    if (m_streaminfo.min_block_size < 16) {
        FLAC_DEBUG("[parseStreamInfo] REJECTED: min_block_size (", m_streaminfo.min_block_size, 
                   ") < 16 - forbidden pattern per RFC 9639 Table 1");
        reportError("Format", "Forbidden: min_block_size < 16 (RFC 9639 Table 1)");
        return false;
    }
    
    // Requirement 3.7: Reject if maximum block size < 16 (RFC 9639 Table 1 forbidden pattern)
    if (m_streaminfo.max_block_size < 16) {
        FLAC_DEBUG("[parseStreamInfo] REJECTED: max_block_size (", m_streaminfo.max_block_size, 
                   ") < 16 - forbidden pattern per RFC 9639 Table 1");
        reportError("Format", "Forbidden: max_block_size < 16 (RFC 9639 Table 1)");
        return false;
    }
    
    // Requirement 3.8: Reject if minimum block size exceeds maximum block size
    if (m_streaminfo.min_block_size > m_streaminfo.max_block_size) {
        FLAC_DEBUG("[parseStreamInfo] REJECTED: min_block_size (", m_streaminfo.min_block_size, 
                   ") > max_block_size (", m_streaminfo.max_block_size, ")");
        reportError("Format", "Invalid: min_block_size > max_block_size");
        return false;
    }
    
    // Requirement 3.14: Reject if sample rate is 0 for audio streams
    // RFC 9639: sample rate must not be 0 for audio streams
    if (m_streaminfo.sample_rate == 0) {
        FLAC_DEBUG("[parseStreamInfo] REJECTED: sample_rate is 0 - invalid for audio streams");
        reportError("Format", "Invalid: sample_rate is 0 (RFC 9639 Section 8.2)");
        return false;
    }
    
    FLAC_DEBUG("[parseStreamInfo] STREAMINFO parsed and validated successfully");
    return true;
}

// ============================================================================
// SEEKTABLE Block Parsing (RFC 9639 Section 8.5)
// ============================================================================

bool FLACDemuxer::parseSeekTableBlock_unlocked(const FLACMetadataBlock& block)
{
    FLAC_DEBUG("[parseSeekTable] Parsing SEEKTABLE block (RFC 9639 Section 8.5)");
    
    // Requirement 12.1: Calculate seek point count as block_length divided by 18
    // RFC 9639 Section 8.5: Each seek point is exactly 18 bytes
    if (block.length % 18 != 0) {
        FLAC_DEBUG("[parseSeekTable] Invalid SEEKTABLE length: ", block.length, 
                   " bytes (not divisible by 18)");
        skipMetadataBlock_unlocked(block);
        return false;
    }
    
    size_t num_points = block.length / 18;
    FLAC_DEBUG("[parseSeekTable] SEEKTABLE contains ", num_points, " seek points");
    
    m_seektable.clear();
    m_seektable.reserve(num_points);
    
    // Track for sorting validation (Requirement 12.6)
    uint64_t last_sample_number = 0;
    bool is_sorted = true;
    bool found_placeholder = false;
    size_t placeholder_count = 0;
    
    for (size_t i = 0; i < num_points; ++i) {
        uint8_t data[18];
        if (m_handler->read(data, 1, 18) != 18) {
            FLAC_DEBUG("[parseSeekTable] Failed to read seek point ", i);
            break;
        }
        
        FLACSeekPoint point;
        
        // Requirement 12.2: Parse u64 sample number (big-endian per RFC 9639 Section 5)
        point.sample_number = (static_cast<uint64_t>(data[0]) << 56) |
                               (static_cast<uint64_t>(data[1]) << 48) |
                               (static_cast<uint64_t>(data[2]) << 40) |
                               (static_cast<uint64_t>(data[3]) << 32) |
                               (static_cast<uint64_t>(data[4]) << 24) |
                               (static_cast<uint64_t>(data[5]) << 16) |
                               (static_cast<uint64_t>(data[6]) << 8) |
                               data[7];
        
        // Requirement 12.3: Parse u64 byte offset from first frame header (big-endian)
        point.stream_offset = (static_cast<uint64_t>(data[8]) << 56) |
                               (static_cast<uint64_t>(data[9]) << 48) |
                               (static_cast<uint64_t>(data[10]) << 40) |
                               (static_cast<uint64_t>(data[11]) << 32) |
                               (static_cast<uint64_t>(data[12]) << 24) |
                               (static_cast<uint64_t>(data[13]) << 16) |
                               (static_cast<uint64_t>(data[14]) << 8) |
                               data[15];
        
        // Requirement 12.4: Parse u16 number of samples in target frame (big-endian)
        point.frame_samples = (static_cast<uint16_t>(data[16]) << 8) | data[17];
        
        // Requirement 12.5: Detect placeholder seek points with value 0xFFFFFFFFFFFFFFFF
        // RFC 9639 Section 8.5: "A placeholder point MUST have the sample number value 
        // 0xFFFFFFFFFFFFFFFF"
        if (point.isPlaceholder()) {
            found_placeholder = true;
            placeholder_count++;
            FLAC_DEBUG("[parseSeekTable] Seek point ", i, ": placeholder (0xFFFFFFFFFFFFFFFF)");
            // Don't add placeholders to the usable seektable
            continue;
        }
        
        // RFC 9639 Section 8.5: Placeholders must all occur at end of table
        // If we find a non-placeholder after a placeholder, log warning
        if (found_placeholder) {
            FLAC_DEBUG("[parseSeekTable] WARNING: Non-placeholder seek point found after placeholder at index ", i);
        }
        
        // Requirement 12.6: Validate seek points are sorted in ascending order by sample number
        // RFC 9639 Section 8.5: "Seek points within a table MUST be sorted in ascending order 
        // by sample number"
        if (!m_seektable.empty() && point.sample_number <= last_sample_number) {
            is_sorted = false;
            FLAC_DEBUG("[parseSeekTable] WARNING: Seek point ", i, " (sample ", point.sample_number,
                       ") is not in ascending order (previous: ", last_sample_number, ")");
        }
        
        last_sample_number = point.sample_number;
        
        FLAC_DEBUG("[parseSeekTable] Seek point ", i, ": sample=", point.sample_number,
                   ", offset=", point.stream_offset, ", frame_samples=", point.frame_samples);
        
        m_seektable.push_back(point);
    }
    
    // Requirement 12.7: If seek points are not sorted, log warning but continue processing
    // RFC 9639 allows decoders to continue even with unsorted seek tables
    if (!is_sorted) {
        FLAC_DEBUG("[parseSeekTable] WARNING: SEEKTABLE seek points are not sorted in ascending order");
        FLAC_DEBUG("[parseSeekTable] Continuing with unsorted seek table (RFC 9639 allows this)");
    }
    
    FLAC_DEBUG("[parseSeekTable] Loaded ", m_seektable.size(), " valid seek points");
    if (placeholder_count > 0) {
        FLAC_DEBUG("[parseSeekTable] Skipped ", placeholder_count, " placeholder seek points");
    }
    
    return true;
}


// ============================================================================
// VORBIS_COMMENT Block Parsing (RFC 9639 Section 8.6)
// ============================================================================

/**
 * @brief Validate a Vorbis comment field name per RFC 9639 Section 8.6
 * 
 * Field names must contain only printable ASCII characters 0x20-0x7E,
 * excluding the equals sign (0x3D).
 * 
 * @param name The field name to validate
 * @return true if the field name is valid, false otherwise
 */
static bool isValidVorbisFieldName(const std::string& name)
{
    if (name.empty()) {
        return false;
    }
    
    // Requirement 13.8: Validate field names use printable ASCII 0x20-0x7E except 0x3D
    // RFC 9639 Section 8.6: "The field name is ASCII 0x20 through 0x7D, 0x3D ('=') excluded"
    // Note: RFC says 0x7D but this appears to be a typo - should be 0x7E (tilde)
    for (unsigned char c : name) {
        // Must be in range 0x20-0x7E (space through tilde)
        if (c < 0x20 || c > 0x7E) {
            return false;
        }
        // Must not be equals sign (0x3D)
        if (c == 0x3D) {
            return false;
        }
    }
    
    return true;
}

bool FLACDemuxer::parseVorbisCommentBlock_unlocked(const FLACMetadataBlock& block)
{
    FLAC_DEBUG("[parseVorbisComment] Parsing VORBIS_COMMENT block (RFC 9639 Section 8.6)");
    
    // Track remaining bytes to prevent reading past block end
    uint64_t bytes_remaining = block.length;
    
    // Clear any existing comments
    m_vorbis_comments.clear();
    
    // ========================================================================
    // Requirement 13.1: Parse u32 little-endian vendor string length
    // RFC 9639 Section 8.6: "A 32-bit unsigned integer indicating the length 
    // of the vendor string in bytes"
    // Note: This is little-endian, which is an exception to FLAC's big-endian rule
    // (for Vorbis compatibility per Requirement 19.4)
    // ========================================================================
    
    if (bytes_remaining < 4) {
        FLAC_DEBUG("[parseVorbisComment] Block too small for vendor length field");
        return false;
    }
    
    uint8_t len_buf[4];
    if (m_handler->read(len_buf, 1, 4) != 4) {
        FLAC_DEBUG("[parseVorbisComment] Failed to read vendor string length");
        return false;
    }
    bytes_remaining -= 4;
    
    // Little-endian u32 per Requirement 19.4
    uint32_t vendor_len = static_cast<uint32_t>(len_buf[0]) |
                          (static_cast<uint32_t>(len_buf[1]) << 8) |
                          (static_cast<uint32_t>(len_buf[2]) << 16) |
                          (static_cast<uint32_t>(len_buf[3]) << 24);
    
    FLAC_DEBUG("[parseVorbisComment] Vendor string length: ", vendor_len, " bytes");
    
    // Validate vendor length doesn't exceed remaining block data
    if (vendor_len > bytes_remaining) {
        FLAC_DEBUG("[parseVorbisComment] Vendor string length (", vendor_len, 
                   ") exceeds remaining block data (", bytes_remaining, ")");
        return false;
    }
    
    // ========================================================================
    // Requirement 13.2: Parse UTF-8 vendor string
    // RFC 9639 Section 8.6: "The vendor string, a UTF-8 encoded string"
    // ========================================================================
    
    std::string vendor_string;
    if (vendor_len > 0) {
        // Sanity check: limit vendor string to reasonable size
        if (vendor_len > 65536) {
            FLAC_DEBUG("[parseVorbisComment] Vendor string too long: ", vendor_len, " bytes");
            // Skip the oversized vendor string
            if (m_handler->seek(static_cast<off_t>(vendor_len), SEEK_CUR) != 0) {
                return false;
            }
            bytes_remaining -= vendor_len;
        } else {
            std::vector<char> vendor_data(vendor_len);
            if (m_handler->read(vendor_data.data(), 1, vendor_len) != vendor_len) {
                FLAC_DEBUG("[parseVorbisComment] Failed to read vendor string");
                return false;
            }
            bytes_remaining -= vendor_len;
            
            vendor_string.assign(vendor_data.begin(), vendor_data.end());
            FLAC_DEBUG("[parseVorbisComment] Vendor string: '", vendor_string, "'");
        }
    } else {
        FLAC_DEBUG("[parseVorbisComment] Empty vendor string");
    }
    
    // ========================================================================
    // Requirement 13.3: Parse u32 little-endian field count
    // RFC 9639 Section 8.6: "A 32-bit unsigned integer indicating the number 
    // of user comment fields"
    // ========================================================================
    
    if (bytes_remaining < 4) {
        FLAC_DEBUG("[parseVorbisComment] Block too small for field count");
        return false;
    }
    
    if (m_handler->read(len_buf, 1, 4) != 4) {
        FLAC_DEBUG("[parseVorbisComment] Failed to read field count");
        return false;
    }
    bytes_remaining -= 4;
    
    // Little-endian u32 per Requirement 19.4
    uint32_t field_count = static_cast<uint32_t>(len_buf[0]) |
                           (static_cast<uint32_t>(len_buf[1]) << 8) |
                           (static_cast<uint32_t>(len_buf[2]) << 16) |
                           (static_cast<uint32_t>(len_buf[3]) << 24);
    
    FLAC_DEBUG("[parseVorbisComment] Field count: ", field_count);
    
    // Sanity check: limit field count to prevent excessive memory allocation
    if (field_count > 65536) {
        FLAC_DEBUG("[parseVorbisComment] Excessive field count: ", field_count, 
                   " - limiting to 65536");
        field_count = 65536;
    }
    
    // ========================================================================
    // Requirement 13.4, 13.5: Parse each field with u32 little-endian length
    // RFC 9639 Section 8.6: "Each user comment field consists of a 32-bit 
    // unsigned integer indicating the length of the field in bytes, followed 
    // by the field itself"
    // ========================================================================
    
    uint32_t valid_fields = 0;
    uint32_t invalid_fields = 0;
    
    for (uint32_t i = 0; i < field_count; ++i) {
        // Check we haven't exceeded block bounds
        if (bytes_remaining < 4) {
            FLAC_DEBUG("[parseVorbisComment] Insufficient data for field ", i, " length");
            break;
        }
        
        // Requirement 13.4: Parse u32 little-endian field length
        if (m_handler->read(len_buf, 1, 4) != 4) {
            FLAC_DEBUG("[parseVorbisComment] Failed to read field ", i, " length");
            break;
        }
        bytes_remaining -= 4;
        
        // Little-endian u32 per Requirement 19.4
        uint32_t field_len = static_cast<uint32_t>(len_buf[0]) |
                             (static_cast<uint32_t>(len_buf[1]) << 8) |
                             (static_cast<uint32_t>(len_buf[2]) << 16) |
                             (static_cast<uint32_t>(len_buf[3]) << 24);
        
        // Validate field length
        if (field_len > bytes_remaining) {
            FLAC_DEBUG("[parseVorbisComment] Field ", i, " length (", field_len, 
                       ") exceeds remaining data (", bytes_remaining, ")");
            break;
        }
        
        // Handle empty fields
        if (field_len == 0) {
            FLAC_DEBUG("[parseVorbisComment] Field ", i, " is empty - skipping");
            invalid_fields++;
            continue;
        }
        
        // Sanity check: limit individual field size
        if (field_len > 1048576) {  // 1 MB limit per field
            FLAC_DEBUG("[parseVorbisComment] Field ", i, " too large: ", field_len, 
                       " bytes - skipping");
            if (m_handler->seek(static_cast<off_t>(field_len), SEEK_CUR) != 0) {
                break;
            }
            bytes_remaining -= field_len;
            invalid_fields++;
            continue;
        }
        
        // Requirement 13.5: Read field content (UTF-8 string)
        std::vector<char> field_data(field_len);
        if (m_handler->read(field_data.data(), 1, field_len) != field_len) {
            FLAC_DEBUG("[parseVorbisComment] Failed to read field ", i, " content");
            break;
        }
        bytes_remaining -= field_len;
        
        std::string field(field_data.begin(), field_data.end());
        
        // ====================================================================
        // Requirement 13.6: Split fields on first equals sign into name and value
        // RFC 9639 Section 8.6: "The field is a UTF-8 encoded string of the 
        // form NAME=value"
        // ====================================================================
        
        size_t eq_pos = field.find('=');
        if (eq_pos == std::string::npos) {
            FLAC_DEBUG("[parseVorbisComment] Field ", i, " has no '=' separator - skipping");
            invalid_fields++;
            continue;
        }
        
        if (eq_pos == 0) {
            FLAC_DEBUG("[parseVorbisComment] Field ", i, " has empty name - skipping");
            invalid_fields++;
            continue;
        }
        
        std::string name = field.substr(0, eq_pos);
        std::string value = field.substr(eq_pos + 1);
        
        // ====================================================================
        // Requirement 13.8: Validate field names use printable ASCII 0x20-0x7E except 0x3D
        // RFC 9639 Section 8.6: "The field name is ASCII 0x20 through 0x7D, 
        // 0x3D ('=') excluded"
        // ====================================================================
        
        if (!isValidVorbisFieldName(name)) {
            FLAC_DEBUG("[parseVorbisComment] Field ", i, " has invalid name characters - rejecting");
            invalid_fields++;
            continue;
        }
        
        // ====================================================================
        // Requirement 13.7: Use case-insensitive comparison for field names
        // RFC 9639 Section 8.6: "Field names are case-insensitive"
        // Convert to uppercase for consistent storage and lookup
        // ====================================================================
        
        std::string normalized_name;
        normalized_name.reserve(name.size());
        for (char c : name) {
            if (c >= 'a' && c <= 'z') {
                normalized_name += static_cast<char>(c - 32);  // Convert to uppercase
            } else {
                normalized_name += c;
            }
        }
        
        // Store the comment (later values overwrite earlier ones for same key)
        m_vorbis_comments[normalized_name] = value;
        valid_fields++;
        
        FLAC_DEBUG("[parseVorbisComment] Field ", i, ": ", normalized_name, "=", 
                   (value.length() > 50 ? value.substr(0, 50) + "..." : value));
    }
    
    FLAC_DEBUG("[parseVorbisComment] Parsed ", valid_fields, " valid fields, ", 
               invalid_fields, " invalid/skipped fields");
    FLAC_DEBUG("[parseVorbisComment] Total stored comments: ", m_vorbis_comments.size());
    
    // Log common metadata fields if present
    auto it = m_vorbis_comments.find("ARTIST");
    if (it != m_vorbis_comments.end()) {
        FLAC_DEBUG("[parseVorbisComment] ARTIST: ", it->second);
    }
    
    it = m_vorbis_comments.find("TITLE");
    if (it != m_vorbis_comments.end()) {
        FLAC_DEBUG("[parseVorbisComment] TITLE: ", it->second);
    }
    
    it = m_vorbis_comments.find("ALBUM");
    if (it != m_vorbis_comments.end()) {
        FLAC_DEBUG("[parseVorbisComment] ALBUM: ", it->second);
    }
    
    FLAC_DEBUG("[parseVorbisComment] VORBIS_COMMENT block parsed successfully");
    return true;
}

// ============================================================================
// PADDING Block Handling (RFC 9639 Section 8.3)
// ============================================================================

bool FLACDemuxer::parsePaddingBlock_unlocked(const FLACMetadataBlock& block)
{
    FLAC_DEBUG("[parsePadding] Parsing PADDING block (RFC 9639 Section 8.3)");
    
    // Requirement 14.1: Read block length from header
    // The block length is already available in block.length from parseMetadataBlockHeader_unlocked
    FLAC_DEBUG("[parsePadding] PADDING block length: ", block.length, " bytes");
    
    // Requirement 14.2: Skip exactly block_length bytes
    // RFC 9639 Section 8.3: "The padding, n bits set to 0"
    // Note: Per Requirement 14.4, we ignore content even if non-zero bytes are present
    if (block.length > 0) {
        if (m_handler->seek(static_cast<off_t>(block.length), SEEK_CUR) != 0) {
            FLAC_DEBUG("[parsePadding] Failed to skip ", block.length, " bytes of padding");
            reportError("IO", "Failed to skip PADDING block data");
            return false;
        }
        FLAC_DEBUG("[parsePadding] Skipped ", block.length, " bytes of padding");
    } else {
        FLAC_DEBUG("[parsePadding] Zero-length PADDING block (valid but unusual)");
    }
    
    // Requirement 14.3: Handle multiple PADDING blocks
    // This is handled by the caller (parseMetadataBlocks_unlocked) which calls this method
    // for each PADDING block encountered in the stream
    
    // Requirement 14.4: When PADDING Block contains non-zero bytes, ignore content and continue
    // We don't read the content at all - we just skip it, so non-zero bytes are automatically ignored
    
    FLAC_DEBUG("[parsePadding] PADDING block processed successfully");
    return true;
}

// ============================================================================
// APPLICATION Block Handling (RFC 9639 Section 8.4)
// ============================================================================

bool FLACDemuxer::parseApplicationBlock_unlocked(const FLACMetadataBlock& block)
{
    FLAC_DEBUG("[parseApplication] Parsing APPLICATION block (RFC 9639 Section 8.4)");
    
    // Requirement 15.1: Read u32 application ID
    // RFC 9639 Section 8.4: "A 32-bit identifier. Registered application IDs can be found
    // at https://xiph.org/flac/id.html"
    if (block.length < 4) {
        FLAC_DEBUG("[parseApplication] APPLICATION block too small: ", block.length, 
                   " bytes (minimum 4 bytes for application ID)");
        // Still skip whatever data exists
        if (block.length > 0) {
            m_handler->seek(static_cast<off_t>(block.length), SEEK_CUR);
        }
        return true;  // Gracefully handle malformed blocks
    }
    
    uint8_t app_id_bytes[4];
    if (m_handler->read(app_id_bytes, 1, 4) != 4) {
        FLAC_DEBUG("[parseApplication] Failed to read application ID");
        reportError("IO", "Failed to read APPLICATION block ID");
        return false;
    }
    
    // Application ID is 4 bytes, typically ASCII characters
    // Log it as both hex and ASCII for debugging
    uint32_t app_id = (static_cast<uint32_t>(app_id_bytes[0]) << 24) |
                      (static_cast<uint32_t>(app_id_bytes[1]) << 16) |
                      (static_cast<uint32_t>(app_id_bytes[2]) << 8) |
                      static_cast<uint32_t>(app_id_bytes[3]);
    
    // Convert to printable ASCII for logging (replace non-printable with '.')
    char app_id_str[5] = {0};
    for (int i = 0; i < 4; ++i) {
        app_id_str[i] = (app_id_bytes[i] >= 32 && app_id_bytes[i] < 127) 
                        ? static_cast<char>(app_id_bytes[i]) : '.';
    }
    
    FLAC_DEBUG("[parseApplication] Application ID: 0x", std::hex, app_id, std::dec,
               " ('", app_id_str, "')");
    
    // Requirement 15.2: Skip remaining block_length minus 4 bytes
    // RFC 9639 Section 8.4: "Application data whose contents are defined by the application"
    uint32_t remaining_bytes = block.length - 4;
    
    if (remaining_bytes > 0) {
        FLAC_DEBUG("[parseApplication] Skipping ", remaining_bytes, " bytes of application data");
        if (m_handler->seek(static_cast<off_t>(remaining_bytes), SEEK_CUR) != 0) {
            FLAC_DEBUG("[parseApplication] Failed to skip application data");
            reportError("IO", "Failed to skip APPLICATION block data");
            return false;
        }
    } else {
        FLAC_DEBUG("[parseApplication] No application data (ID only)");
    }
    
    // Requirement 15.3: Handle unrecognized application blocks gracefully
    // We don't attempt to interpret the application data - we just log and skip it
    // This allows the demuxer to work with any APPLICATION block regardless of the ID
    
    // Requirement 15.4: Handle multiple APPLICATION blocks
    // This is handled by the caller (parseMetadataBlocks_unlocked) which calls this method
    // for each APPLICATION block encountered in the stream
    
    FLAC_DEBUG("[parseApplication] APPLICATION block processed successfully");
    return true;
}

// ============================================================================
// CUESHEET Block Parsing (RFC 9639 Section 8.7)
// ============================================================================

bool FLACDemuxer::parseCuesheetBlock_unlocked(const FLACMetadataBlock& block)
{
    FLAC_DEBUG("[parseCuesheet] Parsing CUESHEET block (RFC 9639 Section 8.7)");
    
    // Clear any existing cuesheet data
    m_cuesheet = FLACCuesheet();
    
    // Minimum CUESHEET block size:
    // - Media catalog number: 128 bytes
    // - Lead-in samples: 8 bytes
    // - CD-DA flag + reserved: 1 + 258 bytes = 259 bytes (but stored as bits)
    // - Actually: 128 + 8 + (1 + 7 + 258*8)/8 = 128 + 8 + 259 = 395 bytes minimum header
    // - Plus number of tracks: 1 byte
    // Total minimum: 128 + 8 + 259 + 1 = 396 bytes
    // But the reserved bits are: u(7+258*8) = 7 + 2064 = 2071 bits = 258.875 bytes
    // So: 128 + 8 + 1 bit + 2071 bits + 8 bits = 128 + 8 + 2080 bits = 128 + 8 + 260 bytes = 396 bytes
    
    // Minimum size check: 128 (catalog) + 8 (lead-in) + 259 (flags+reserved) + 1 (track count) = 396 bytes
    // But we need at least one track, so minimum is actually larger
    static constexpr size_t MIN_CUESHEET_HEADER_SIZE = 396;
    
    if (block.length < MIN_CUESHEET_HEADER_SIZE) {
        FLAC_DEBUG("[parseCuesheet] CUESHEET block too small: ", block.length, 
                   " bytes (minimum ", MIN_CUESHEET_HEADER_SIZE, " bytes)");
        skipMetadataBlock_unlocked(block);
        return false;
    }
    
    // Track remaining bytes
    uint64_t bytes_remaining = block.length;
    
    // ========================================================================
    // Requirement 16.1: Read u(128*8) media catalog number
    // RFC 9639 Section 8.7: "Media catalog number in ASCII printable characters 0x20-0x7E"
    // ========================================================================
    
    uint8_t catalog_data[128];
    if (m_handler->read(catalog_data, 1, 128) != 128) {
        FLAC_DEBUG("[parseCuesheet] Failed to read media catalog number");
        return false;
    }
    bytes_remaining -= 128;
    
    // Copy to cuesheet structure (null-terminate)
    std::memcpy(m_cuesheet.media_catalog_number, catalog_data, 128);
    m_cuesheet.media_catalog_number[128] = '\0';
    
    // Log catalog number (trim trailing nulls for display)
    std::string catalog_str;
    for (int i = 0; i < 128 && catalog_data[i] != 0; ++i) {
        if (catalog_data[i] >= 0x20 && catalog_data[i] <= 0x7E) {
            catalog_str += static_cast<char>(catalog_data[i]);
        }
    }
    if (!catalog_str.empty()) {
        FLAC_DEBUG("[parseCuesheet] Media catalog number: '", catalog_str, "'");
    } else {
        FLAC_DEBUG("[parseCuesheet] Media catalog number: (empty)");
    }
    
    // ========================================================================
    // Requirement 16.2: Read u(64) number of lead-in samples
    // RFC 9639 Section 8.7: "Number of lead-in samples"
    // ========================================================================
    
    uint8_t lead_in_data[8];
    if (m_handler->read(lead_in_data, 1, 8) != 8) {
        FLAC_DEBUG("[parseCuesheet] Failed to read lead-in samples");
        return false;
    }
    bytes_remaining -= 8;
    
    // Big-endian u64 per RFC 9639 Section 5
    m_cuesheet.lead_in_samples = (static_cast<uint64_t>(lead_in_data[0]) << 56) |
                                  (static_cast<uint64_t>(lead_in_data[1]) << 48) |
                                  (static_cast<uint64_t>(lead_in_data[2]) << 40) |
                                  (static_cast<uint64_t>(lead_in_data[3]) << 32) |
                                  (static_cast<uint64_t>(lead_in_data[4]) << 24) |
                                  (static_cast<uint64_t>(lead_in_data[5]) << 16) |
                                  (static_cast<uint64_t>(lead_in_data[6]) << 8) |
                                  lead_in_data[7];
    
    FLAC_DEBUG("[parseCuesheet] Lead-in samples: ", m_cuesheet.lead_in_samples);
    
    // ========================================================================
    // Requirement 16.3: Read u(1) CD-DA flag
    // Requirement 16.4: Skip u(7+258*8) reserved bits
    // RFC 9639 Section 8.7: "1 if the cuesheet corresponds to a CD-DA; else 0"
    // Reserved bits: 7 + 258*8 = 2071 bits = 258.875 bytes, rounded to 259 bytes
    // ========================================================================
    
    // Read the byte containing the CD-DA flag and first 7 reserved bits
    uint8_t flags_and_reserved[259];
    if (m_handler->read(flags_and_reserved, 1, 259) != 259) {
        FLAC_DEBUG("[parseCuesheet] Failed to read CD-DA flag and reserved bits");
        return false;
    }
    bytes_remaining -= 259;
    
    // CD-DA flag is bit 7 (MSB) of the first byte
    m_cuesheet.is_cd_da = (flags_and_reserved[0] & 0x80) != 0;
    
    FLAC_DEBUG("[parseCuesheet] CD-DA flag: ", m_cuesheet.is_cd_da ? "true" : "false");
    
    // ========================================================================
    // Requirement 16.5: Read u(8) number of tracks
    // RFC 9639 Section 8.7: "Number of tracks in this cuesheet"
    // ========================================================================
    
    uint8_t num_tracks;
    if (m_handler->read(&num_tracks, 1, 1) != 1) {
        FLAC_DEBUG("[parseCuesheet] Failed to read number of tracks");
        return false;
    }
    bytes_remaining -= 1;
    
    FLAC_DEBUG("[parseCuesheet] Number of tracks: ", static_cast<int>(num_tracks));
    
    // ========================================================================
    // Requirement 16.6: Validate track count is at least 1
    // RFC 9639 Section 8.7: "The number of tracks MUST be at least 1"
    // ========================================================================
    
    if (num_tracks < 1) {
        FLAC_DEBUG("[parseCuesheet] REJECTED: Number of tracks (", static_cast<int>(num_tracks), 
                   ") < 1 - invalid per RFC 9639 Section 8.7");
        reportError("Format", "Invalid CUESHEET: number of tracks must be at least 1");
        return false;
    }
    
    // ========================================================================
    // Requirement 16.7: Validate CD-DA track count is at most 100
    // RFC 9639 Section 8.7: "For CD-DA, this number MUST be no more than 100"
    // ========================================================================
    
    if (m_cuesheet.is_cd_da && num_tracks > 100) {
        FLAC_DEBUG("[parseCuesheet] REJECTED: CD-DA track count (", static_cast<int>(num_tracks), 
                   ") > 100 - invalid per RFC 9639 Section 8.7");
        reportError("Format", "Invalid CD-DA CUESHEET: number of tracks must be at most 100");
        return false;
    }
    
    // ========================================================================
    // Parse cuesheet tracks (RFC 9639 Section 8.7.1)
    // ========================================================================
    
    m_cuesheet.tracks.reserve(num_tracks);
    
    for (uint8_t t = 0; t < num_tracks; ++t) {
        FLACCuesheetTrack track;
        
        // Each track has a fixed header size:
        // - Track offset: 8 bytes
        // - Track number: 1 byte
        // - Track ISRC: 12 bytes
        // - Track type + pre-emphasis + reserved: (1 + 1 + 6 + 13*8) bits = 112 bits = 14 bytes
        // - Number of index points: 1 byte
        // Total: 8 + 1 + 12 + 14 + 1 = 36 bytes per track header
        static constexpr size_t TRACK_HEADER_SIZE = 36;
        
        if (bytes_remaining < TRACK_HEADER_SIZE) {
            FLAC_DEBUG("[parseCuesheet] Insufficient data for track ", static_cast<int>(t), 
                       " header (need ", TRACK_HEADER_SIZE, " bytes, have ", bytes_remaining, ")");
            break;
        }
        
        // Read track offset (u64, big-endian)
        uint8_t track_data[36];
        if (m_handler->read(track_data, 1, TRACK_HEADER_SIZE) != TRACK_HEADER_SIZE) {
            FLAC_DEBUG("[parseCuesheet] Failed to read track ", static_cast<int>(t), " header");
            break;
        }
        bytes_remaining -= TRACK_HEADER_SIZE;
        
        // Parse track offset (bytes 0-7)
        track.offset = (static_cast<uint64_t>(track_data[0]) << 56) |
                       (static_cast<uint64_t>(track_data[1]) << 48) |
                       (static_cast<uint64_t>(track_data[2]) << 40) |
                       (static_cast<uint64_t>(track_data[3]) << 32) |
                       (static_cast<uint64_t>(track_data[4]) << 24) |
                       (static_cast<uint64_t>(track_data[5]) << 16) |
                       (static_cast<uint64_t>(track_data[6]) << 8) |
                       track_data[7];
        
        // Parse track number (byte 8)
        track.number = track_data[8];
        
        // Parse track ISRC (bytes 9-20)
        std::memcpy(track.isrc, &track_data[9], 12);
        track.isrc[12] = '\0';
        
        // Parse track type and pre-emphasis (byte 21)
        // Bit 7: track type (0=audio, 1=non-audio)
        // Bit 6: pre-emphasis flag
        // Bits 0-5: reserved (part of the 6+13*8 reserved bits)
        track.is_audio = (track_data[21] & 0x80) == 0;
        track.pre_emphasis = (track_data[21] & 0x40) != 0;
        
        // Bytes 22-34: remaining reserved bits (13 bytes)
        // Byte 35: number of index points
        uint8_t num_index_points = track_data[35];
        
        FLAC_DEBUG("[parseCuesheet] Track ", static_cast<int>(t), ": number=", 
                   static_cast<int>(track.number), ", offset=", track.offset,
                   ", is_audio=", track.is_audio, ", pre_emphasis=", track.pre_emphasis,
                   ", index_points=", static_cast<int>(num_index_points));
        
        // Parse index points (RFC 9639 Section 8.7.1.1)
        // Each index point is 12 bytes:
        // - Offset: 8 bytes
        // - Index point number: 1 byte
        // - Reserved: 3 bytes
        static constexpr size_t INDEX_POINT_SIZE = 12;
        
        // Lead-out track must have zero index points
        bool is_lead_out = track.isLeadOut();
        
        if (is_lead_out && num_index_points != 0) {
            FLAC_DEBUG("[parseCuesheet] WARNING: Lead-out track has ", 
                       static_cast<int>(num_index_points), " index points (should be 0)");
        }
        
        track.index_points.reserve(num_index_points);
        
        for (uint8_t i = 0; i < num_index_points; ++i) {
            if (bytes_remaining < INDEX_POINT_SIZE) {
                FLAC_DEBUG("[parseCuesheet] Insufficient data for index point ", 
                           static_cast<int>(i), " of track ", static_cast<int>(t));
                break;
            }
            
            uint8_t index_data[INDEX_POINT_SIZE];
            if (m_handler->read(index_data, 1, INDEX_POINT_SIZE) != INDEX_POINT_SIZE) {
                FLAC_DEBUG("[parseCuesheet] Failed to read index point ", 
                           static_cast<int>(i), " of track ", static_cast<int>(t));
                break;
            }
            bytes_remaining -= INDEX_POINT_SIZE;
            
            FLACCuesheetIndexPoint index_point;
            
            // Parse index point offset (bytes 0-7, big-endian)
            index_point.offset = (static_cast<uint64_t>(index_data[0]) << 56) |
                                 (static_cast<uint64_t>(index_data[1]) << 48) |
                                 (static_cast<uint64_t>(index_data[2]) << 40) |
                                 (static_cast<uint64_t>(index_data[3]) << 32) |
                                 (static_cast<uint64_t>(index_data[4]) << 24) |
                                 (static_cast<uint64_t>(index_data[5]) << 16) |
                                 (static_cast<uint64_t>(index_data[6]) << 8) |
                                 index_data[7];
            
            // Parse index point number (byte 8)
            index_point.number = index_data[8];
            
            // Bytes 9-11: reserved (ignored)
            
            track.index_points.push_back(index_point);
            
            FLAC_DEBUG("[parseCuesheet]   Index point ", static_cast<int>(i), 
                       ": number=", static_cast<int>(index_point.number),
                       ", offset=", index_point.offset);
        }
        
        m_cuesheet.tracks.push_back(std::move(track));
    }
    
    // ========================================================================
    // Requirement 16.8: Store track information for potential use
    // ========================================================================
    
    FLAC_DEBUG("[parseCuesheet] Parsed ", m_cuesheet.tracks.size(), " tracks");
    FLAC_DEBUG("[parseCuesheet] CUESHEET block parsed successfully");
    
    return true;
}

// ============================================================================
// PICTURE Block Parsing (RFC 9639 Section 8.8)
// ============================================================================

bool FLACDemuxer::parsePictureBlock_unlocked(const FLACMetadataBlock& block)
{
    FLAC_DEBUG("[parsePicture] Parsing PICTURE block (RFC 9639 Section 8.8)");
    
    // Minimum PICTURE block size:
    // - Picture type: 4 bytes
    // - Media type length: 4 bytes
    // - Description length: 4 bytes
    // - Width: 4 bytes
    // - Height: 4 bytes
    // - Color depth: 4 bytes
    // - Indexed colors: 4 bytes
    // - Picture data length: 4 bytes
    // Total minimum: 32 bytes (with empty strings and no data)
    static constexpr size_t MIN_PICTURE_HEADER_SIZE = 32;
    
    if (block.length < MIN_PICTURE_HEADER_SIZE) {
        FLAC_DEBUG("[parsePicture] PICTURE block too small: ", block.length, 
                   " bytes (minimum ", MIN_PICTURE_HEADER_SIZE, " bytes)");
        skipMetadataBlock_unlocked(block);
        return false;
    }
    
    // Track remaining bytes
    uint64_t bytes_remaining = block.length;
    
    FLACPicture picture;
    
    // ========================================================================
    // Requirement 17.1: Parse u32 picture type
    // RFC 9639 Section 8.8: "The picture type according to the ID3v2 APIC frame"
    // ========================================================================
    
    uint8_t type_data[4];
    if (m_handler->read(type_data, 1, 4) != 4) {
        FLAC_DEBUG("[parsePicture] Failed to read picture type");
        return false;
    }
    bytes_remaining -= 4;
    
    // Big-endian u32 per RFC 9639 Section 5
    picture.type = (static_cast<uint32_t>(type_data[0]) << 24) |
                   (static_cast<uint32_t>(type_data[1]) << 16) |
                   (static_cast<uint32_t>(type_data[2]) << 8) |
                   static_cast<uint32_t>(type_data[3]);
    
    FLAC_DEBUG("[parsePicture] Picture type: ", picture.type, " (", picture.getTypeName(), ")");
    
    // ========================================================================
    // Requirement 17.2: Parse media type length (u32)
    // RFC 9639 Section 8.8: "The length of the MIME type string in bytes"
    // ========================================================================
    
    if (bytes_remaining < 4) {
        FLAC_DEBUG("[parsePicture] Insufficient data for media type length");
        return false;
    }
    
    uint8_t len_data[4];
    if (m_handler->read(len_data, 1, 4) != 4) {
        FLAC_DEBUG("[parsePicture] Failed to read media type length");
        return false;
    }
    bytes_remaining -= 4;
    
    // Big-endian u32
    uint32_t media_type_len = (static_cast<uint32_t>(len_data[0]) << 24) |
                              (static_cast<uint32_t>(len_data[1]) << 16) |
                              (static_cast<uint32_t>(len_data[2]) << 8) |
                              static_cast<uint32_t>(len_data[3]);
    
    FLAC_DEBUG("[parsePicture] Media type length: ", media_type_len, " bytes");
    
    // Validate media type length
    if (media_type_len > bytes_remaining) {
        FLAC_DEBUG("[parsePicture] Media type length (", media_type_len, 
                   ") exceeds remaining data (", bytes_remaining, ")");
        return false;
    }
    
    // ========================================================================
    // Requirement 17.3: Parse media type (ASCII string)
    // RFC 9639 Section 8.8: "The MIME type string, in printable ASCII characters 0x20-0x7E"
    // ========================================================================
    
    if (media_type_len > 0) {
        // Sanity check: limit media type to reasonable size
        if (media_type_len > 256) {
            FLAC_DEBUG("[parsePicture] Media type too long: ", media_type_len, " bytes - skipping block");
            if (m_handler->seek(static_cast<off_t>(bytes_remaining), SEEK_CUR) != 0) {
                return false;
            }
            return true;  // Gracefully skip malformed block
        }
        
        std::vector<char> media_type_data(media_type_len);
        if (m_handler->read(media_type_data.data(), 1, media_type_len) != media_type_len) {
            FLAC_DEBUG("[parsePicture] Failed to read media type string");
            return false;
        }
        bytes_remaining -= media_type_len;
        
        picture.media_type.assign(media_type_data.begin(), media_type_data.end());
        FLAC_DEBUG("[parsePicture] Media type: '", picture.media_type, "'");
        
        // ====================================================================
        // Requirement 17.12: Handle URI format when media type is "-->"
        // RFC 9639 Section 8.8: "If the MIME type string is '-->', then the 
        // picture data is a URL"
        // ====================================================================
        
        if (picture.media_type == "-->") {
            picture.is_uri = true;
            FLAC_DEBUG("[parsePicture] Picture data is a URI reference");
        }
    } else {
        FLAC_DEBUG("[parsePicture] Empty media type");
    }
    
    // ========================================================================
    // Requirement 17.4: Parse description length (u32)
    // RFC 9639 Section 8.8: "The length of the description string in bytes"
    // ========================================================================
    
    if (bytes_remaining < 4) {
        FLAC_DEBUG("[parsePicture] Insufficient data for description length");
        return false;
    }
    
    if (m_handler->read(len_data, 1, 4) != 4) {
        FLAC_DEBUG("[parsePicture] Failed to read description length");
        return false;
    }
    bytes_remaining -= 4;
    
    // Big-endian u32
    uint32_t description_len = (static_cast<uint32_t>(len_data[0]) << 24) |
                               (static_cast<uint32_t>(len_data[1]) << 16) |
                               (static_cast<uint32_t>(len_data[2]) << 8) |
                               static_cast<uint32_t>(len_data[3]);
    
    FLAC_DEBUG("[parsePicture] Description length: ", description_len, " bytes");
    
    // Validate description length
    if (description_len > bytes_remaining) {
        FLAC_DEBUG("[parsePicture] Description length (", description_len, 
                   ") exceeds remaining data (", bytes_remaining, ")");
        return false;
    }
    
    // ========================================================================
    // Requirement 17.5: Parse description (UTF-8 string)
    // RFC 9639 Section 8.8: "The description of the picture, in UTF-8"
    // ========================================================================
    
    if (description_len > 0) {
        // Sanity check: limit description to reasonable size
        if (description_len > 65536) {
            FLAC_DEBUG("[parsePicture] Description too long: ", description_len, 
                       " bytes - truncating to 65536");
            // Read only first 65536 bytes
            std::vector<char> desc_data(65536);
            if (m_handler->read(desc_data.data(), 1, 65536) != 65536) {
                FLAC_DEBUG("[parsePicture] Failed to read description string");
                return false;
            }
            // Skip remaining description bytes
            if (m_handler->seek(static_cast<off_t>(description_len - 65536), SEEK_CUR) != 0) {
                return false;
            }
            bytes_remaining -= description_len;
            picture.description.assign(desc_data.begin(), desc_data.end());
        } else {
            std::vector<char> desc_data(description_len);
            if (m_handler->read(desc_data.data(), 1, description_len) != description_len) {
                FLAC_DEBUG("[parsePicture] Failed to read description string");
                return false;
            }
            bytes_remaining -= description_len;
            picture.description.assign(desc_data.begin(), desc_data.end());
        }
        
        FLAC_DEBUG("[parsePicture] Description: '", 
                   (picture.description.length() > 50 
                    ? picture.description.substr(0, 50) + "..." 
                    : picture.description), "'");
    } else {
        FLAC_DEBUG("[parsePicture] Empty description");
    }
    
    // ========================================================================
    // Requirement 17.6, 17.7: Parse dimensions (u32 width, u32 height)
    // RFC 9639 Section 8.8: "The width of the picture in pixels"
    // RFC 9639 Section 8.8: "The height of the picture in pixels"
    // ========================================================================
    
    if (bytes_remaining < 16) {  // 4 bytes each for width, height, color_depth, indexed_colors
        FLAC_DEBUG("[parsePicture] Insufficient data for picture dimensions");
        return false;
    }
    
    uint8_t dim_data[16];
    if (m_handler->read(dim_data, 1, 16) != 16) {
        FLAC_DEBUG("[parsePicture] Failed to read picture dimensions");
        return false;
    }
    bytes_remaining -= 16;
    
    // Big-endian u32 for each field
    picture.width = (static_cast<uint32_t>(dim_data[0]) << 24) |
                    (static_cast<uint32_t>(dim_data[1]) << 16) |
                    (static_cast<uint32_t>(dim_data[2]) << 8) |
                    static_cast<uint32_t>(dim_data[3]);
    
    picture.height = (static_cast<uint32_t>(dim_data[4]) << 24) |
                     (static_cast<uint32_t>(dim_data[5]) << 16) |
                     (static_cast<uint32_t>(dim_data[6]) << 8) |
                     static_cast<uint32_t>(dim_data[7]);
    
    FLAC_DEBUG("[parsePicture] Dimensions: ", picture.width, "x", picture.height, " pixels");
    
    // ========================================================================
    // Requirement 17.8: Parse color depth (u32)
    // RFC 9639 Section 8.8: "The color depth of the picture in bits-per-pixel"
    // ========================================================================
    
    picture.color_depth = (static_cast<uint32_t>(dim_data[8]) << 24) |
                          (static_cast<uint32_t>(dim_data[9]) << 16) |
                          (static_cast<uint32_t>(dim_data[10]) << 8) |
                          static_cast<uint32_t>(dim_data[11]);
    
    FLAC_DEBUG("[parsePicture] Color depth: ", picture.color_depth, " bits per pixel");
    
    // ========================================================================
    // Requirement 17.9: Parse indexed colors (u32)
    // RFC 9639 Section 8.8: "For indexed-color pictures (e.g. GIF), the number 
    // of colors used, or 0 for non-indexed-color pictures"
    // ========================================================================
    
    picture.indexed_colors = (static_cast<uint32_t>(dim_data[12]) << 24) |
                             (static_cast<uint32_t>(dim_data[13]) << 16) |
                             (static_cast<uint32_t>(dim_data[14]) << 8) |
                             static_cast<uint32_t>(dim_data[15]);
    
    if (picture.indexed_colors > 0) {
        FLAC_DEBUG("[parsePicture] Indexed colors: ", picture.indexed_colors);
    } else {
        FLAC_DEBUG("[parsePicture] Non-indexed color picture");
    }
    
    // ========================================================================
    // Requirement 17.10: Parse picture data length (u32)
    // RFC 9639 Section 8.8: "The length of the picture data in bytes"
    // ========================================================================
    
    if (bytes_remaining < 4) {
        FLAC_DEBUG("[parsePicture] Insufficient data for picture data length");
        return false;
    }
    
    if (m_handler->read(len_data, 1, 4) != 4) {
        FLAC_DEBUG("[parsePicture] Failed to read picture data length");
        return false;
    }
    bytes_remaining -= 4;
    
    // Big-endian u32
    uint32_t picture_data_len = (static_cast<uint32_t>(len_data[0]) << 24) |
                                (static_cast<uint32_t>(len_data[1]) << 16) |
                                (static_cast<uint32_t>(len_data[2]) << 8) |
                                static_cast<uint32_t>(len_data[3]);
    
    FLAC_DEBUG("[parsePicture] Picture data length: ", picture_data_len, " bytes");
    
    // Validate picture data length
    if (picture_data_len > bytes_remaining) {
        FLAC_DEBUG("[parsePicture] Picture data length (", picture_data_len, 
                   ") exceeds remaining data (", bytes_remaining, ")");
        return false;
    }
    
    // ========================================================================
    // Requirement 17.11: Parse picture data (binary data)
    // RFC 9639 Section 8.8: "The binary picture data"
    // ========================================================================
    
    if (picture_data_len > 0) {
        // Sanity check: limit picture data to reasonable size (16 MB)
        static constexpr uint32_t MAX_PICTURE_SIZE = 16 * 1024 * 1024;
        
        if (picture_data_len > MAX_PICTURE_SIZE) {
            FLAC_DEBUG("[parsePicture] Picture data too large: ", picture_data_len, 
                       " bytes (max ", MAX_PICTURE_SIZE, ") - skipping data");
            // Skip the oversized picture data
            if (m_handler->seek(static_cast<off_t>(picture_data_len), SEEK_CUR) != 0) {
                return false;
            }
            bytes_remaining -= picture_data_len;
        } else {
            picture.data.resize(picture_data_len);
            if (m_handler->read(picture.data.data(), 1, picture_data_len) != picture_data_len) {
                FLAC_DEBUG("[parsePicture] Failed to read picture data");
                return false;
            }
            bytes_remaining -= picture_data_len;
            
            FLAC_DEBUG("[parsePicture] Read ", picture.data.size(), " bytes of picture data");
            
            // If this is a URI, log it
            if (picture.is_uri) {
                std::string uri(picture.data.begin(), picture.data.end());
                FLAC_DEBUG("[parsePicture] URI: '", 
                           (uri.length() > 100 ? uri.substr(0, 100) + "..." : uri), "'");
            }
        }
    } else {
        FLAC_DEBUG("[parsePicture] No picture data");
    }
    
    // Skip any remaining bytes in the block (shouldn't happen with valid data)
    if (bytes_remaining > 0) {
        FLAC_DEBUG("[parsePicture] Skipping ", bytes_remaining, " trailing bytes");
        if (m_handler->seek(static_cast<off_t>(bytes_remaining), SEEK_CUR) != 0) {
            return false;
        }
    }
    
    // Store the picture
    m_pictures.push_back(std::move(picture));
    
    FLAC_DEBUG("[parsePicture] PICTURE block parsed successfully (total pictures: ", 
               m_pictures.size(), ")");
    
    return true;
}

bool FLACDemuxer::skipMetadataBlock_unlocked(const FLACMetadataBlock& block)
{
    if (block.length > 0) {
        return m_handler->seek(static_cast<off_t>(block.length), SEEK_CUR) == 0;
    }
    return true;
}


// ============================================================================
// Frame Detection and Parsing (RFC 9639 Section 9)
// ============================================================================

/**
 * @brief Find the next FLAC frame in the stream
 * 
 * Implements RFC 9639 Section 9.1 frame sync code detection:
 * - Requirement 4.1: Search for 15-bit sync pattern 0b111111111111100
 * - Requirement 4.2: Verify byte alignment of sync code
 * - Requirement 21.3: Limit search scope to 512 bytes maximum
 * 
 * @param frame Output frame structure to populate
 * @return true if a valid frame was found, false otherwise
 */
bool FLACDemuxer::findNextFrame_unlocked(FLACFrame& frame)
{
    FLAC_DEBUG("[findNextFrame] Searching for frame sync code (RFC 9639 Section 9.1)");
    FLAC_DEBUG("[findNextFrame] Starting search at offset ", m_current_offset);
    
    // Requirement 21.3: Limit search scope to 512 bytes maximum
    // This prevents excessive I/O operations when searching for frame boundaries
    static constexpr size_t MAX_SEARCH_BYTES = 512;
    
    uint8_t buffer[MAX_SEARCH_BYTES];
    uint64_t search_start = m_current_offset;
    
    // Seek to current position
    if (m_handler->seek(static_cast<off_t>(m_current_offset), SEEK_SET) != 0) {
        FLAC_DEBUG("[findNextFrame] Failed to seek to offset ", m_current_offset);
        return false;
    }
    
    size_t bytes_read = m_handler->read(buffer, 1, MAX_SEARCH_BYTES);
    if (bytes_read < 2) {
        FLAC_DEBUG("[findNextFrame] Insufficient data: only ", bytes_read, " bytes read");
        return false;
    }
    
    FLAC_DEBUG("[findNextFrame] Read ", bytes_read, " bytes for sync search");
    
    // Requirement 4.1: Search for 15-bit sync pattern 0b111111111111100
    // RFC 9639 Section 9.1: The sync code is 15 bits: 0b111111111111100
    // This appears as:
    //   - 0xFF 0xF8 for fixed block size (blocking strategy bit = 0)
    //   - 0xFF 0xF9 for variable block size (blocking strategy bit = 1)
    // 
    // Requirement 4.2: Verify byte alignment of sync code
    // The sync code must start on a byte boundary (which it does since we're
    // searching byte-by-byte from the start of the buffer)
    
    for (size_t i = 0; i < bytes_read - 1; ++i) {
        // Check for first byte of sync pattern (0xFF)
        if (buffer[i] != 0xFF) {
            continue;
        }
        
        // Check for second byte: 0xF8 (fixed) or 0xF9 (variable)
        // RFC 9639 Section 9.1: The 15-bit sync code is followed by a 1-bit
        // blocking strategy flag:
        //   - 0 = fixed block size (0xFFF8)
        //   - 1 = variable block size (0xFFF9)
        uint8_t second_byte = buffer[i + 1];
        
        if (second_byte != 0xF8 && second_byte != 0xF9) {
            continue;
        }
        
        // Found potential sync code at byte-aligned position
        uint64_t frame_offset = search_start + i;
        bool is_variable_block_size = (second_byte == 0xF9);
        
        FLAC_DEBUG("[findNextFrame] Potential sync code at offset ", frame_offset,
                   " (", is_variable_block_size ? "variable" : "fixed", " block size)");
        
        // Requirement 4.3-4.7: Extract and validate blocking strategy
        // Requirement 4.6: Fixed block size expects 0xFF 0xF8
        // Requirement 4.7: Variable block size expects 0xFF 0xF9
        
        // Requirement 4.8: Reject if blocking strategy changes mid-stream
        // RFC 9639 Section 9.1: "The blocking strategy MUST NOT change within a stream"
        if (m_blocking_strategy_set) {
            if (is_variable_block_size != m_variable_block_size) {
                FLAC_DEBUG("[findNextFrame] REJECTED: Blocking strategy changed mid-stream!");
                FLAC_DEBUG("[findNextFrame]   Expected: ", m_variable_block_size ? "variable (0xFFF9)" : "fixed (0xFFF8)");
                FLAC_DEBUG("[findNextFrame]   Found: ", is_variable_block_size ? "variable (0xFFF9)" : "fixed (0xFFF8)");
                // Skip this potential sync code and continue searching
                // (it might be a false positive in the audio data)
                continue;
            }
        }
        
        // Populate frame structure for header parsing
        frame.file_offset = frame_offset;
        frame.variable_block_size = is_variable_block_size;
        
        // Validate the frame header to confirm this is a real sync code
        // (not just 0xFFF8/0xFFF9 appearing in audio data)
        if (parseFrameHeader_unlocked(frame, &buffer[i], bytes_read - i)) {
            // Valid frame found - update blocking strategy tracking
            if (!m_blocking_strategy_set) {
                m_blocking_strategy_set = true;
                m_variable_block_size = is_variable_block_size;
                FLAC_DEBUG("[findNextFrame] Blocking strategy set to: ", 
                           m_variable_block_size ? "variable (0xFFF9)" : "fixed (0xFFF8)");
            }
            
            FLAC_DEBUG("[findNextFrame] Valid frame found at offset ", frame.file_offset);
            FLAC_DEBUG("[findNextFrame]   Block size: ", frame.block_size, " samples");
            FLAC_DEBUG("[findNextFrame]   Sample rate: ", frame.sample_rate, " Hz");
            FLAC_DEBUG("[findNextFrame]   Channels: ", static_cast<int>(frame.channels));
            FLAC_DEBUG("[findNextFrame]   Bits per sample: ", static_cast<int>(frame.bits_per_sample));
            
            return true;
        }
        
        // Header validation failed - this was a false sync code
        FLAC_DEBUG("[findNextFrame] False sync code at offset ", frame_offset, " - continuing search");
    }
    
    FLAC_DEBUG("[findNextFrame] No valid frame sync found in ", bytes_read, " bytes");
    FLAC_DEBUG("[findNextFrame] Search limit (", MAX_SEARCH_BYTES, " bytes) reached per Requirement 21.3");
    return false;
}

/**
 * @brief Parse and validate a FLAC frame header
 * 
 * Implements RFC 9639 Section 9.1 frame header parsing with validation
 * of all forbidden and reserved patterns per RFC 9639 Table 1.
 * 
 * Frame Header Structure (RFC 9639 Section 9.1):
 *   Byte 0-1: Sync code (15 bits) + Blocking strategy (1 bit)
 *   Byte 2:   Block size bits (4) + Sample rate bits (4)
 *   Byte 3:   Channel bits (4) + Bit depth bits (3) + Reserved (1)
 *   Byte 4+:  Coded number (1-7 bytes, UTF-8-like encoding)
 *   Optional: Uncommon block size (0, 1, or 2 bytes)
 *   Optional: Uncommon sample rate (0, 1, or 2 bytes)
 *   Final:    CRC-8 (1 byte)
 * 
 * @param frame Output frame structure to populate
 * @param buffer Pointer to frame data starting at sync code
 * @param size Number of bytes available in buffer
 * @return true if header is valid, false otherwise
 */
bool FLACDemuxer::parseFrameHeader_unlocked(FLACFrame& frame, const uint8_t* buffer, size_t size)
{
    // Minimum frame header size: sync(2) + header(2) = 4 bytes
    // (coded number and CRC-8 follow, but we validate basic structure first)
    if (size < 4) {
        FLAC_DEBUG("[parseFrameHeader] Buffer too small: ", size, " bytes (minimum 4)");
        return false;
    }
    
    // Byte 0-1: Sync code (already validated in findNextFrame_unlocked)
    // The sync code is 0xFFF8 (fixed) or 0xFFF9 (variable)
    
    // Byte 2: Block size bits (4) + Sample rate bits (4)
    // Requirement 5.1: Extract bits 4-7 of frame byte 2 for block size
    uint8_t block_size_bits = (buffer[2] >> 4) & 0x0F;
    
    // Requirement 6.1: Extract bits 0-3 of frame byte 2 for sample rate
    uint8_t sample_rate_bits = buffer[2] & 0x0F;
    
    // Byte 3: Channel bits (4) + Bit depth bits (3) + Reserved (1)
    // Requirement 7.1: Extract bits 4-7 of frame byte 3 for channel assignment
    uint8_t channel_bits = (buffer[3] >> 4) & 0x0F;
    
    // Requirement 8.1: Extract bits 1-3 of frame byte 3 for bit depth
    uint8_t bit_depth_bits = (buffer[3] >> 1) & 0x07;
    
    // Requirement 8.10, 8.11: Extract reserved bit (bit 0 of byte 3)
    uint8_t reserved_bit = buffer[3] & 0x01;
    
    // ========================================================================
    // Validate forbidden and reserved patterns (RFC 9639 Table 1)
    // ========================================================================
    
    // Requirement 5.2: Reserved block size pattern 0b0000
    // RFC 9639 Table 1: Block size bits 0b0000 is reserved
    if (block_size_bits == 0x00) {
        FLAC_DEBUG("[parseFrameHeader] REJECTED: Reserved block size pattern 0b0000");
        return false;
    }
    
    // Requirement 6.17: Forbidden sample rate pattern 0b1111
    // RFC 9639 Table 1: Sample rate bits 0b1111 is forbidden
    if (sample_rate_bits == 0x0F) {
        FLAC_DEBUG("[parseFrameHeader] REJECTED: Forbidden sample rate pattern 0b1111");
        return false;
    }
    
    // Requirement 7.7: Reserved channel bits 0b1011-0b1111
    // RFC 9639 Table 1: Channel bits 0b1011-0b1111 are reserved
    if (channel_bits >= 0x0B) {
        FLAC_DEBUG("[parseFrameHeader] REJECTED: Reserved channel bits 0b", 
                   std::hex, static_cast<int>(channel_bits), std::dec);
        return false;
    }
    
    // Requirement 8.5: Reserved bit depth pattern 0b011
    // RFC 9639 Table 1: Bit depth bits 0b011 is reserved
    if (bit_depth_bits == 0x03) {
        FLAC_DEBUG("[parseFrameHeader] REJECTED: Reserved bit depth pattern 0b011");
        return false;
    }
    
    // Requirement 8.10, 8.11: Reserved bit at bit 0 of byte 3 must be 0
    // RFC 9639 Section 9.1.4: "A reserved bit. It MUST have value 0"
    if (reserved_bit != 0) {
        FLAC_DEBUG("[parseFrameHeader] WARNING: Reserved bit is non-zero (", 
                   static_cast<int>(reserved_bit), ") - continuing per Requirement 8.11");
        // Per Requirement 8.11: Log warning and continue processing
    }
    
    // ========================================================================
    // Parse block size (RFC 9639 Section 9.1.1, Table 14)
    // Requirements 5.3-5.17
    // ========================================================================
    
    switch (block_size_bits) {
        case 0x01: frame.block_size = 192; break;    // Requirement 5.3
        case 0x02: frame.block_size = 576; break;    // Requirement 5.4
        case 0x03: frame.block_size = 1152; break;   // Requirement 5.5
        case 0x04: frame.block_size = 2304; break;   // Requirement 5.6
        case 0x05: frame.block_size = 4608; break;   // Requirement 5.7
        case 0x06:                                    // Requirement 5.8: 8-bit uncommon
            // For now, use STREAMINFO max_block_size as we don't parse the extra byte
            frame.block_size = m_streaminfo.max_block_size > 0 ? m_streaminfo.max_block_size : 4096;
            break;
        case 0x07:                                    // Requirement 5.9: 16-bit uncommon
            // For now, use STREAMINFO max_block_size as we don't parse the extra bytes
            frame.block_size = m_streaminfo.max_block_size > 0 ? m_streaminfo.max_block_size : 4096;
            break;
        case 0x08: frame.block_size = 256; break;    // Requirement 5.10
        case 0x09: frame.block_size = 512; break;    // Requirement 5.11
        case 0x0A: frame.block_size = 1024; break;   // Requirement 5.12
        case 0x0B: frame.block_size = 2048; break;   // Requirement 5.13
        case 0x0C: frame.block_size = 4096; break;   // Requirement 5.14
        case 0x0D: frame.block_size = 8192; break;   // Requirement 5.15
        case 0x0E: frame.block_size = 16384; break;  // Requirement 5.16
        case 0x0F: frame.block_size = 32768; break;  // Requirement 5.17
        default:
            // Should not reach here due to 0b0000 check above
            frame.block_size = m_streaminfo.max_block_size > 0 ? m_streaminfo.max_block_size : 4096;
            break;
    }
    
    // ========================================================================
    // Parse sample rate (RFC 9639 Section 9.1.2)
    // Requirements 6.2-6.16
    // ========================================================================
    
    switch (sample_rate_bits) {
        case 0x00:                                    // Requirement 6.2: Get from STREAMINFO
            frame.sample_rate = m_streaminfo.sample_rate;
            break;
        case 0x01: frame.sample_rate = 88200; break;  // Requirement 6.3
        case 0x02: frame.sample_rate = 176400; break; // Requirement 6.4
        case 0x03: frame.sample_rate = 192000; break; // Requirement 6.5
        case 0x04: frame.sample_rate = 8000; break;   // Requirement 6.6
        case 0x05: frame.sample_rate = 16000; break;  // Requirement 6.7
        case 0x06: frame.sample_rate = 22050; break;  // Requirement 6.8
        case 0x07: frame.sample_rate = 24000; break;  // Requirement 6.9
        case 0x08: frame.sample_rate = 32000; break;  // Requirement 6.10
        case 0x09: frame.sample_rate = 44100; break;  // Requirement 6.11
        case 0x0A: frame.sample_rate = 48000; break;  // Requirement 6.12
        case 0x0B: frame.sample_rate = 96000; break;  // Requirement 6.13
        case 0x0C:                                    // Requirement 6.14: 8-bit uncommon (kHz)
        case 0x0D:                                    // Requirement 6.15: 16-bit uncommon (Hz)
        case 0x0E:                                    // Requirement 6.16: 16-bit uncommon (tens of Hz)
            // For now, use STREAMINFO sample rate as we don't parse the extra bytes
            frame.sample_rate = m_streaminfo.sample_rate;
            break;
        // 0x0F is forbidden and already rejected above
        default:
            frame.sample_rate = m_streaminfo.sample_rate;
            break;
    }
    
    // ========================================================================
    // Parse channel assignment (RFC 9639 Section 9.1.3)
    // Requirements 7.1-7.7 - Using dedicated parseChannelBits_unlocked method
    // ========================================================================
    
    // Note: Reserved patterns 0b1011-0b1111 are already rejected above in the
    // validation section. The parseChannelBits_unlocked method provides the
    // complete implementation with proper mode tracking.
    if (!parseChannelBits_unlocked(channel_bits, frame.channels, frame.channel_mode)) {
        // This should not happen since we already validated above, but handle it
        FLAC_DEBUG("[parseFrameHeader] parseChannelBits_unlocked failed for bits 0x",
                   std::hex, static_cast<int>(channel_bits), std::dec);
        return false;
    }
    
    // ========================================================================
    // Parse bit depth (RFC 9639 Section 9.1.4)
    // Requirements 8.1-8.11 - Using dedicated parseBitDepthBits_unlocked method
    // ========================================================================
    
    // Note: Reserved pattern 0b011 is already rejected above in the validation section.
    // The parseBitDepthBits_unlocked method provides the complete implementation with
    // proper reserved bit validation and logging.
    if (!parseBitDepthBits_unlocked(bit_depth_bits, reserved_bit, frame.bits_per_sample)) {
        // This should not happen since we already validated above, but handle it
        FLAC_DEBUG("[parseFrameHeader] parseBitDepthBits_unlocked failed for bits 0x",
                   std::hex, static_cast<int>(bit_depth_bits), std::dec);
        return false;
    }
    
    // Set sample offset based on current position
    frame.sample_offset = m_current_sample;
    
    FLAC_DEBUG("[parseFrameHeader] Parsed header: block_size=", frame.block_size,
               ", sample_rate=", frame.sample_rate, ", channels=", static_cast<int>(frame.channels),
               ", bits_per_sample=", static_cast<int>(frame.bits_per_sample));
    
    return frame.isValid();
}


// ============================================================================
// Block Size Bits Parser (RFC 9639 Section 9.1.1, Table 14)
// ============================================================================

/**
 * @brief Parse block size bits from frame header per RFC 9639 Table 14
 * 
 * Implements Requirements 5.1-5.18 for block size decoding.
 * 
 * RFC 9639 Table 14 - Block Size Encoding:
 *   0b0000: Reserved (reject)
 *   0b0001: 192 samples
 *   0b0010: 576 samples
 *   0b0011: 1152 samples
 *   0b0100: 2304 samples
 *   0b0101: 4608 samples
 *   0b0110: 8-bit uncommon block size minus 1 follows
 *   0b0111: 16-bit uncommon block size minus 1 follows
 *   0b1000: 256 samples
 *   0b1001: 512 samples
 *   0b1010: 1024 samples
 *   0b1011: 2048 samples
 *   0b1100: 4096 samples
 *   0b1101: 8192 samples
 *   0b1110: 16384 samples
 *   0b1111: 32768 samples
 * 
 * @param bits The 4-bit block size code (bits 4-7 of frame byte 2)
 * @param buffer Pointer to frame data (for reading uncommon block sizes)
 * @param buffer_size Size of available buffer
 * @param header_offset Current offset within header (updated if uncommon bytes read)
 * @param block_size Output: the decoded block size in samples
 * @return true if block size is valid, false if reserved/forbidden pattern detected
 */
bool FLACDemuxer::parseBlockSizeBits_unlocked(uint8_t bits, const uint8_t* buffer, size_t buffer_size,
                                              size_t& header_offset, uint32_t& block_size)
{
    FLAC_DEBUG("[parseBlockSizeBits] Parsing block size bits: 0b",
               ((bits >> 3) & 1), ((bits >> 2) & 1), ((bits >> 1) & 1), (bits & 1),
               " (0x", std::hex, static_cast<int>(bits), std::dec, ")");
    
    // Requirement 5.2: Reserved block size pattern 0b0000
    // RFC 9639 Table 1: Block size bits 0b0000 is reserved
    if (bits == 0x00) {
        FLAC_DEBUG("[parseBlockSizeBits] REJECTED: Reserved block size pattern 0b0000 (Requirement 5.2)");
        return false;
    }
    
    switch (bits) {
        // Requirement 5.3: 0b0001 = 192 samples
        case 0x01:
            block_size = 192;
            FLAC_DEBUG("[parseBlockSizeBits] Block size: 192 samples (0b0001)");
            break;
            
        // Requirement 5.4: 0b0010 = 576 samples
        case 0x02:
            block_size = 576;
            FLAC_DEBUG("[parseBlockSizeBits] Block size: 576 samples (0b0010)");
            break;
            
        // Requirement 5.5: 0b0011 = 1152 samples
        case 0x03:
            block_size = 1152;
            FLAC_DEBUG("[parseBlockSizeBits] Block size: 1152 samples (0b0011)");
            break;
            
        // Requirement 5.6: 0b0100 = 2304 samples
        case 0x04:
            block_size = 2304;
            FLAC_DEBUG("[parseBlockSizeBits] Block size: 2304 samples (0b0100)");
            break;
            
        // Requirement 5.7: 0b0101 = 4608 samples
        case 0x05:
            block_size = 4608;
            FLAC_DEBUG("[parseBlockSizeBits] Block size: 4608 samples (0b0101)");
            break;
            
        // Requirement 5.8: 0b0110 = 8-bit uncommon block size minus 1 follows
        case 0x06: {
            // Need at least 1 additional byte for the uncommon block size
            // The uncommon block size byte comes after the coded number and before CRC-8
            // For now, we need to read it from the buffer at the appropriate offset
            FLAC_DEBUG("[parseBlockSizeBits] 8-bit uncommon block size (0b0110)");
            
            // Check if we have enough buffer space
            // The uncommon block size byte position depends on the coded number length
            // For simplicity, we'll mark that we need to read it later
            // and use a placeholder for now
            if (header_offset >= buffer_size) {
                FLAC_DEBUG("[parseBlockSizeBits] Insufficient buffer for 8-bit uncommon block size");
                // Fall back to STREAMINFO max_block_size
                block_size = m_streaminfo.max_block_size > 0 ? m_streaminfo.max_block_size : 4096;
                FLAC_DEBUG("[parseBlockSizeBits] Using fallback block size: ", block_size);
                break;
            }
            
            // Read the 8-bit uncommon block size value
            uint8_t uncommon_value = buffer[header_offset];
            header_offset += 1;
            
            // RFC 9639 Section 9.1.1: "8-bit (blocksize-1), i.e., blocksize is 1-256"
            // The stored value is (block_size - 1), so we add 1
            block_size = static_cast<uint32_t>(uncommon_value) + 1;
            
            FLAC_DEBUG("[parseBlockSizeBits] 8-bit uncommon value: ", static_cast<int>(uncommon_value),
                       " -> block size: ", block_size, " samples");
            break;
        }
            
        // Requirement 5.9: 0b0111 = 16-bit uncommon block size minus 1 follows
        case 0x07: {
            FLAC_DEBUG("[parseBlockSizeBits] 16-bit uncommon block size (0b0111)");
            
            // Need at least 2 additional bytes for the uncommon block size
            if (header_offset + 1 >= buffer_size) {
                FLAC_DEBUG("[parseBlockSizeBits] Insufficient buffer for 16-bit uncommon block size");
                // Fall back to STREAMINFO max_block_size
                block_size = m_streaminfo.max_block_size > 0 ? m_streaminfo.max_block_size : 4096;
                FLAC_DEBUG("[parseBlockSizeBits] Using fallback block size: ", block_size);
                break;
            }
            
            // Read the 16-bit uncommon block size value (big-endian per RFC 9639 Section 5)
            uint16_t uncommon_value = (static_cast<uint16_t>(buffer[header_offset]) << 8) |
                                       static_cast<uint16_t>(buffer[header_offset + 1]);
            header_offset += 2;
            
            // RFC 9639 Section 9.1.1: "16-bit (blocksize-1), i.e., blocksize is 1-65536"
            // The stored value is (block_size - 1), so we add 1
            uint32_t decoded_block_size = static_cast<uint32_t>(uncommon_value) + 1;
            
            FLAC_DEBUG("[parseBlockSizeBits] 16-bit uncommon value: ", uncommon_value,
                       " -> decoded block size: ", decoded_block_size, " samples");
            
            // Requirement 5.18: Reject forbidden uncommon block size 65536
            // RFC 9639 Table 1: "A frame header with an uncommon block size 
            // (see Section 9.1.1) equal to 65536"
            if (decoded_block_size == 65536) {
                FLAC_DEBUG("[parseBlockSizeBits] REJECTED: Forbidden uncommon block size 65536 (Requirement 5.18)");
                return false;
            }
            
            block_size = decoded_block_size;
            FLAC_DEBUG("[parseBlockSizeBits] Block size: ", block_size, " samples");
            break;
        }
            
        // Requirement 5.10: 0b1000 = 256 samples
        case 0x08:
            block_size = 256;
            FLAC_DEBUG("[parseBlockSizeBits] Block size: 256 samples (0b1000)");
            break;
            
        // Requirement 5.11: 0b1001 = 512 samples
        case 0x09:
            block_size = 512;
            FLAC_DEBUG("[parseBlockSizeBits] Block size: 512 samples (0b1001)");
            break;
            
        // Requirement 5.12: 0b1010 = 1024 samples
        case 0x0A:
            block_size = 1024;
            FLAC_DEBUG("[parseBlockSizeBits] Block size: 1024 samples (0b1010)");
            break;
            
        // Requirement 5.13: 0b1011 = 2048 samples
        case 0x0B:
            block_size = 2048;
            FLAC_DEBUG("[parseBlockSizeBits] Block size: 2048 samples (0b1011)");
            break;
            
        // Requirement 5.14: 0b1100 = 4096 samples
        case 0x0C:
            block_size = 4096;
            FLAC_DEBUG("[parseBlockSizeBits] Block size: 4096 samples (0b1100)");
            break;
            
        // Requirement 5.15: 0b1101 = 8192 samples
        case 0x0D:
            block_size = 8192;
            FLAC_DEBUG("[parseBlockSizeBits] Block size: 8192 samples (0b1101)");
            break;
            
        // Requirement 5.16: 0b1110 = 16384 samples
        case 0x0E:
            block_size = 16384;
            FLAC_DEBUG("[parseBlockSizeBits] Block size: 16384 samples (0b1110)");
            break;
            
        // Requirement 5.17: 0b1111 = 32768 samples
        case 0x0F:
            block_size = 32768;
            FLAC_DEBUG("[parseBlockSizeBits] Block size: 32768 samples (0b1111)");
            break;
            
        default:
            // Should never reach here since we handle all 16 values
            FLAC_DEBUG("[parseBlockSizeBits] Unexpected block size bits: 0x", 
                       std::hex, static_cast<int>(bits), std::dec);
            block_size = m_streaminfo.max_block_size > 0 ? m_streaminfo.max_block_size : 4096;
            break;
    }
    
    return true;
}


// ============================================================================
// Sample Rate Bits Parser (RFC 9639 Section 9.1.2)
// ============================================================================

/**
 * @brief Parse sample rate bits from frame header per RFC 9639 Section 9.1.2
 * 
 * Implements Requirements 6.1-6.17 for sample rate decoding.
 * 
 * RFC 9639 Sample Rate Encoding:
 *   0b0000: Get from STREAMINFO (non-streamable subset)
 *   0b0001: 88200 Hz
 *   0b0010: 176400 Hz
 *   0b0011: 192000 Hz
 *   0b0100: 8000 Hz
 *   0b0101: 16000 Hz
 *   0b0110: 22050 Hz
 *   0b0111: 24000 Hz
 *   0b1000: 32000 Hz
 *   0b1001: 44100 Hz
 *   0b1010: 48000 Hz
 *   0b1011: 96000 Hz
 *   0b1100: 8-bit uncommon sample rate in kHz follows
 *   0b1101: 16-bit uncommon sample rate in Hz follows
 *   0b1110: 16-bit uncommon sample rate in tens of Hz follows
 *   0b1111: Forbidden (reject)
 * 
 * @param bits The 4-bit sample rate code (bits 0-3 of frame byte 2)
 * @param buffer Pointer to frame data (for reading uncommon sample rates)
 * @param buffer_size Size of available buffer
 * @param header_offset Current offset within header (updated if uncommon bytes read)
 * @param sample_rate Output: the decoded sample rate in Hz
 * @return true if sample rate is valid, false if forbidden pattern detected
 */
bool FLACDemuxer::parseSampleRateBits_unlocked(uint8_t bits, const uint8_t* buffer, size_t buffer_size,
                                               size_t& header_offset, uint32_t& sample_rate)
{
    FLAC_DEBUG("[parseSampleRateBits] Parsing sample rate bits: 0b",
               ((bits >> 3) & 1), ((bits >> 2) & 1), ((bits >> 1) & 1), (bits & 1),
               " (0x", std::hex, static_cast<int>(bits), std::dec, ")");
    
    // Requirement 6.17: Forbidden sample rate pattern 0b1111
    // RFC 9639 Table 1: Sample rate bits 0b1111 is forbidden
    if (bits == 0x0F) {
        FLAC_DEBUG("[parseSampleRateBits] REJECTED: Forbidden sample rate pattern 0b1111 (Requirement 6.17)");
        return false;
    }
    
    switch (bits) {
        // Requirement 6.2: 0b0000 = Get from STREAMINFO
        // RFC 9639 Section 9.1.2: "Get sample rate from STREAMINFO metadata block"
        // Note: This makes the stream non-streamable subset compliant
        case 0x00:
            sample_rate = m_streaminfo.sample_rate;
            FLAC_DEBUG("[parseSampleRateBits] Sample rate from STREAMINFO: ", sample_rate, " Hz (0b0000)");
            break;
            
        // Requirement 6.3: 0b0001 = 88200 Hz
        case 0x01:
            sample_rate = 88200;
            FLAC_DEBUG("[parseSampleRateBits] Sample rate: 88200 Hz (0b0001)");
            break;
            
        // Requirement 6.4: 0b0010 = 176400 Hz
        case 0x02:
            sample_rate = 176400;
            FLAC_DEBUG("[parseSampleRateBits] Sample rate: 176400 Hz (0b0010)");
            break;
            
        // Requirement 6.5: 0b0011 = 192000 Hz
        case 0x03:
            sample_rate = 192000;
            FLAC_DEBUG("[parseSampleRateBits] Sample rate: 192000 Hz (0b0011)");
            break;
            
        // Requirement 6.6: 0b0100 = 8000 Hz
        case 0x04:
            sample_rate = 8000;
            FLAC_DEBUG("[parseSampleRateBits] Sample rate: 8000 Hz (0b0100)");
            break;
            
        // Requirement 6.7: 0b0101 = 16000 Hz
        case 0x05:
            sample_rate = 16000;
            FLAC_DEBUG("[parseSampleRateBits] Sample rate: 16000 Hz (0b0101)");
            break;
            
        // Requirement 6.8: 0b0110 = 22050 Hz
        case 0x06:
            sample_rate = 22050;
            FLAC_DEBUG("[parseSampleRateBits] Sample rate: 22050 Hz (0b0110)");
            break;
            
        // Requirement 6.9: 0b0111 = 24000 Hz
        case 0x07:
            sample_rate = 24000;
            FLAC_DEBUG("[parseSampleRateBits] Sample rate: 24000 Hz (0b0111)");
            break;
            
        // Requirement 6.10: 0b1000 = 32000 Hz
        case 0x08:
            sample_rate = 32000;
            FLAC_DEBUG("[parseSampleRateBits] Sample rate: 32000 Hz (0b1000)");
            break;
            
        // Requirement 6.11: 0b1001 = 44100 Hz
        case 0x09:
            sample_rate = 44100;
            FLAC_DEBUG("[parseSampleRateBits] Sample rate: 44100 Hz (0b1001)");
            break;
            
        // Requirement 6.12: 0b1010 = 48000 Hz
        case 0x0A:
            sample_rate = 48000;
            FLAC_DEBUG("[parseSampleRateBits] Sample rate: 48000 Hz (0b1010)");
            break;
            
        // Requirement 6.13: 0b1011 = 96000 Hz
        case 0x0B:
            sample_rate = 96000;
            FLAC_DEBUG("[parseSampleRateBits] Sample rate: 96000 Hz (0b1011)");
            break;
            
        // Requirement 6.14: 0b1100 = 8-bit uncommon sample rate in kHz follows
        case 0x0C: {
            FLAC_DEBUG("[parseSampleRateBits] 8-bit uncommon sample rate in kHz (0b1100)");
            
            // Check if we have enough buffer space for the 8-bit value
            if (header_offset >= buffer_size) {
                FLAC_DEBUG("[parseSampleRateBits] Insufficient buffer for 8-bit uncommon sample rate");
                // Fall back to STREAMINFO sample rate
                sample_rate = m_streaminfo.sample_rate;
                FLAC_DEBUG("[parseSampleRateBits] Using fallback sample rate: ", sample_rate, " Hz");
                break;
            }
            
            // Read the 8-bit uncommon sample rate value (in kHz)
            uint8_t uncommon_value = buffer[header_offset];
            header_offset += 1;
            
            // RFC 9639 Section 9.1.2: "8-bit sample rate (in kHz)"
            // The stored value is the sample rate in kHz, so multiply by 1000
            sample_rate = static_cast<uint32_t>(uncommon_value) * 1000;
            
            FLAC_DEBUG("[parseSampleRateBits] 8-bit uncommon value: ", static_cast<int>(uncommon_value),
                       " kHz -> sample rate: ", sample_rate, " Hz");
            break;
        }
            
        // Requirement 6.15: 0b1101 = 16-bit uncommon sample rate in Hz follows
        case 0x0D: {
            FLAC_DEBUG("[parseSampleRateBits] 16-bit uncommon sample rate in Hz (0b1101)");
            
            // Check if we have enough buffer space for the 16-bit value
            if (header_offset + 1 >= buffer_size) {
                FLAC_DEBUG("[parseSampleRateBits] Insufficient buffer for 16-bit uncommon sample rate");
                // Fall back to STREAMINFO sample rate
                sample_rate = m_streaminfo.sample_rate;
                FLAC_DEBUG("[parseSampleRateBits] Using fallback sample rate: ", sample_rate, " Hz");
                break;
            }
            
            // Read the 16-bit uncommon sample rate value (big-endian per RFC 9639 Section 5)
            uint16_t uncommon_value = (static_cast<uint16_t>(buffer[header_offset]) << 8) |
                                       static_cast<uint16_t>(buffer[header_offset + 1]);
            header_offset += 2;
            
            // RFC 9639 Section 9.1.2: "16-bit sample rate (in Hz)"
            // The stored value is the sample rate in Hz directly
            sample_rate = static_cast<uint32_t>(uncommon_value);
            
            FLAC_DEBUG("[parseSampleRateBits] 16-bit uncommon value: ", uncommon_value,
                       " Hz -> sample rate: ", sample_rate, " Hz");
            break;
        }
            
        // Requirement 6.16: 0b1110 = 16-bit uncommon sample rate in tens of Hz follows
        case 0x0E: {
            FLAC_DEBUG("[parseSampleRateBits] 16-bit uncommon sample rate in tens of Hz (0b1110)");
            
            // Check if we have enough buffer space for the 16-bit value
            if (header_offset + 1 >= buffer_size) {
                FLAC_DEBUG("[parseSampleRateBits] Insufficient buffer for 16-bit uncommon sample rate");
                // Fall back to STREAMINFO sample rate
                sample_rate = m_streaminfo.sample_rate;
                FLAC_DEBUG("[parseSampleRateBits] Using fallback sample rate: ", sample_rate, " Hz");
                break;
            }
            
            // Read the 16-bit uncommon sample rate value (big-endian per RFC 9639 Section 5)
            uint16_t uncommon_value = (static_cast<uint16_t>(buffer[header_offset]) << 8) |
                                       static_cast<uint16_t>(buffer[header_offset + 1]);
            header_offset += 2;
            
            // RFC 9639 Section 9.1.2: "16-bit sample rate (in units of 10 Hz)"
            // The stored value is the sample rate in tens of Hz, so multiply by 10
            sample_rate = static_cast<uint32_t>(uncommon_value) * 10;
            
            FLAC_DEBUG("[parseSampleRateBits] 16-bit uncommon value: ", uncommon_value,
                       " (x10 Hz) -> sample rate: ", sample_rate, " Hz");
            break;
        }
            
        // 0x0F is forbidden and already rejected above
        default:
            // Should never reach here since we handle all 16 values
            FLAC_DEBUG("[parseSampleRateBits] Unexpected sample rate bits: 0x", 
                       std::hex, static_cast<int>(bits), std::dec);
            sample_rate = m_streaminfo.sample_rate;
            break;
    }
    
    return true;
}


// ============================================================================
// Channel Assignment Bits Parser (RFC 9639 Section 9.1.3)
// ============================================================================

/**
 * @brief Parse channel assignment bits from frame header per RFC 9639 Section 9.1.3
 * 
 * Implements Requirements 7.1-7.7 for channel assignment decoding.
 * 
 * RFC 9639 Channel Assignment Encoding:
 *   0b0000-0b0111: 1-8 independent channels (value + 1)
 *   0b1000: Left-side stereo (left + side)
 *   0b1001: Right-side stereo (side + right)
 *   0b1010: Mid-side stereo (mid + side)
 *   0b1011-0b1111: Reserved (reject)
 * 
 * @param bits The 4-bit channel assignment code (bits 4-7 of frame byte 3)
 * @param channels Output: the number of channels (1-8)
 * @param mode Output: the channel assignment mode (independent or stereo variant)
 * @return true if channel assignment is valid, false if reserved pattern detected
 */
bool FLACDemuxer::parseChannelBits_unlocked(uint8_t bits, uint8_t& channels, FLACChannelMode& mode)
{
    FLAC_DEBUG("[parseChannelBits] Parsing channel bits: 0b",
               ((bits >> 3) & 1), ((bits >> 2) & 1), ((bits >> 1) & 1), (bits & 1),
               " (0x", std::hex, static_cast<int>(bits), std::dec, ")");
    
    // Requirement 7.7: Reserved channel bits 0b1011-0b1111
    // RFC 9639 Table 1: Channel bits 0b1011-0b1111 are reserved
    if (bits >= 0x0B) {
        FLAC_DEBUG("[parseChannelBits] REJECTED: Reserved channel bits 0b",
                   ((bits >> 3) & 1), ((bits >> 2) & 1), ((bits >> 1) & 1), (bits & 1),
                   " (Requirement 7.7)");
        return false;
    }
    
    // Requirement 7.1: Extract bits 4-7 of frame byte 3 for channel assignment
    // (Already done by caller - bits parameter contains the extracted value)
    
    if (bits <= 0x07) {
        // Requirement 7.2: Independent channels (1-8)
        // RFC 9639 Section 9.1.3: "n channels, where n is the value plus 1"
        channels = bits + 1;
        
        // Requirement 7.3: Mode is independent
        mode = FLACChannelMode::INDEPENDENT;
        
        FLAC_DEBUG("[parseChannelBits] Independent mode: ", static_cast<int>(channels), 
                   " channel(s) (0b", ((bits >> 3) & 1), ((bits >> 2) & 1), 
                   ((bits >> 1) & 1), (bits & 1), ")");
    } else if (bits == 0x08) {
        // Requirement 7.4: Left-side stereo
        // RFC 9639 Section 9.1.3: "left/side stereo: channel 0 is the left channel, 
        // channel 1 is the side (difference) channel"
        channels = 2;
        mode = FLACChannelMode::LEFT_SIDE;
        
        FLAC_DEBUG("[parseChannelBits] Left-side stereo: 2 channels (0b1000)");
    } else if (bits == 0x09) {
        // Requirement 7.5: Right-side stereo
        // RFC 9639 Section 9.1.3: "side/right stereo: channel 0 is the side (difference) 
        // channel, channel 1 is the right channel"
        channels = 2;
        mode = FLACChannelMode::RIGHT_SIDE;
        
        FLAC_DEBUG("[parseChannelBits] Right-side stereo: 2 channels (0b1001)");
    } else if (bits == 0x0A) {
        // Requirement 7.6: Mid-side stereo
        // RFC 9639 Section 9.1.3: "mid/side stereo: channel 0 is the mid (average) 
        // channel, channel 1 is the side (difference) channel"
        channels = 2;
        mode = FLACChannelMode::MID_SIDE;
        
        FLAC_DEBUG("[parseChannelBits] Mid-side stereo: 2 channels (0b1010)");
    }
    // 0x0B-0x0F are reserved and already rejected above
    
    return true;
}


// ============================================================================
// Bit Depth Bits Parser (RFC 9639 Section 9.1.4)
// ============================================================================

/**
 * @brief Parse bit depth bits from frame header per RFC 9639 Section 9.1.4
 * 
 * Implements Requirements 8.1-8.11 for bit depth decoding.
 * 
 * RFC 9639 Bit Depth Encoding:
 *   0b000: Get from STREAMINFO (non-streamable subset)
 *   0b001: 8 bits per sample
 *   0b010: 12 bits per sample
 *   0b011: Reserved (reject)
 *   0b100: 16 bits per sample
 *   0b101: 20 bits per sample
 *   0b110: 24 bits per sample
 *   0b111: 32 bits per sample
 * 
 * @param bits The 3-bit bit depth code (bits 1-3 of frame byte 3)
 * @param reserved_bit The reserved bit (bit 0 of frame byte 3) - must be 0
 * @param bit_depth Output: the decoded bit depth (4-32 bits per sample)
 * @return true if bit depth is valid, false if reserved pattern detected
 */
bool FLACDemuxer::parseBitDepthBits_unlocked(uint8_t bits, uint8_t reserved_bit, uint8_t& bit_depth)
{
    FLAC_DEBUG("[parseBitDepthBits] Parsing bit depth bits: 0b",
               ((bits >> 2) & 1), ((bits >> 1) & 1), (bits & 1),
               " (0x", std::hex, static_cast<int>(bits), std::dec, ")");
    
    // Requirement 8.10, 8.11: Validate reserved bit at bit 0 of frame byte 3
    // RFC 9639 Section 9.1.4: "A reserved bit. It MUST have value 0"
    if (reserved_bit != 0) {
        FLAC_DEBUG("[parseBitDepthBits] WARNING: Reserved bit is non-zero (",
                   static_cast<int>(reserved_bit), ") - continuing per Requirement 8.11");
        // Per Requirement 8.11: Log warning and continue processing
    }
    
    // Requirement 8.5: Reserved bit depth pattern 0b011
    // RFC 9639 Table 1: Bit depth bits 0b011 is reserved
    if (bits == 0x03) {
        FLAC_DEBUG("[parseBitDepthBits] REJECTED: Reserved bit depth pattern 0b011 (Requirement 8.5)");
        return false;
    }
    
    // Requirement 8.1: Extract bits 1-3 of frame byte 3 for bit depth
    // (Already done by caller - bits parameter contains the extracted value)
    
    switch (bits) {
        // Requirement 8.2: 0b000 = Get from STREAMINFO
        // RFC 9639 Section 9.1.4: "Get sample size in bits from STREAMINFO metadata block"
        // Note: This makes the stream non-streamable subset compliant
        case 0x00:
            bit_depth = m_streaminfo.bits_per_sample;
            FLAC_DEBUG("[parseBitDepthBits] Bit depth from STREAMINFO: ", 
                       static_cast<int>(bit_depth), " bits (0b000)");
            break;
            
        // Requirement 8.3: 0b001 = 8 bits per sample
        case 0x01:
            bit_depth = 8;
            FLAC_DEBUG("[parseBitDepthBits] Bit depth: 8 bits (0b001)");
            break;
            
        // Requirement 8.4: 0b010 = 12 bits per sample
        case 0x02:
            bit_depth = 12;
            FLAC_DEBUG("[parseBitDepthBits] Bit depth: 12 bits (0b010)");
            break;
            
        // 0b011 is reserved and already rejected above (Requirement 8.5)
            
        // Requirement 8.6: 0b100 = 16 bits per sample
        case 0x04:
            bit_depth = 16;
            FLAC_DEBUG("[parseBitDepthBits] Bit depth: 16 bits (0b100)");
            break;
            
        // Requirement 8.7: 0b101 = 20 bits per sample
        case 0x05:
            bit_depth = 20;
            FLAC_DEBUG("[parseBitDepthBits] Bit depth: 20 bits (0b101)");
            break;
            
        // Requirement 8.8: 0b110 = 24 bits per sample
        case 0x06:
            bit_depth = 24;
            FLAC_DEBUG("[parseBitDepthBits] Bit depth: 24 bits (0b110)");
            break;
            
        // Requirement 8.9: 0b111 = 32 bits per sample
        case 0x07:
            bit_depth = 32;
            FLAC_DEBUG("[parseBitDepthBits] Bit depth: 32 bits (0b111)");
            break;
            
        default:
            // Should never reach here since we handle all 8 values (0-7)
            // and 0b011 is already rejected
            FLAC_DEBUG("[parseBitDepthBits] Unexpected bit depth bits: 0x",
                       std::hex, static_cast<int>(bits), std::dec);
            bit_depth = m_streaminfo.bits_per_sample;
            break;
    }
    
    return true;
}


// ============================================================================
// Coded Number Parser (RFC 9639 Section 9.1.5)
// ============================================================================

/**
 * @brief Parse coded number from frame header per RFC 9639 Section 9.1.5
 * 
 * Implements Requirements 9.1-9.10 for coded number decoding using
 * UTF-8-like variable-length encoding (1-7 bytes).
 * 
 * RFC 9639 Table 18 - Coded Number Encoding (extended UTF-8):
 *   Number Range (Hex)         | Octet Sequence (Binary)
 *   0000 0000 0000 - 007F      | 0xxxxxxx                                           (1 byte)
 *   0000 0000 0080 - 07FF      | 110xxxxx 10xxxxxx                                  (2 bytes)
 *   0000 0000 0800 - FFFF      | 1110xxxx 10xxxxxx 10xxxxxx                         (3 bytes)
 *   0000 0001 0000 - 1F FFFF   | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx                (4 bytes)
 *   0000 0020 0000 - 3FF FFFF  | 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx       (5 bytes)
 *   0000 0400 0000 - 7FFF FFFF | 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx (6 bytes)
 *   0000 8000 0000 - F FFFF FFFF | 11111110 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx (7 bytes)
 * 
 * For fixed block size streams: coded number is frame number (max 31 bits, 6 bytes)
 * For variable block size streams: coded number is sample number (max 36 bits, 7 bytes)
 * 
 * @param buffer Pointer to frame data starting at the coded number position
 * @param buffer_size Size of available buffer
 * @param bytes_consumed Output: number of bytes consumed by the coded number
 * @param coded_number Output: the decoded frame/sample number
 * @param is_variable_block_size True if variable block size (sample number), false if fixed (frame number)
 * @return true if coded number is valid, false if encoding is invalid
 */
bool FLACDemuxer::parseCodedNumber_unlocked(const uint8_t* buffer, size_t buffer_size,
                                            size_t& bytes_consumed, uint64_t& coded_number,
                                            bool is_variable_block_size)
{
    FLAC_DEBUG("[parseCodedNumber] Parsing coded number (", 
               is_variable_block_size ? "sample number" : "frame number", ")");
    
    // Initialize outputs
    bytes_consumed = 0;
    coded_number = 0;
    
    // Need at least 1 byte
    if (buffer_size < 1) {
        FLAC_DEBUG("[parseCodedNumber] Buffer too small: ", buffer_size, " bytes");
        return false;
    }
    
    uint8_t first_byte = buffer[0];
    
    // Determine the number of bytes in the coded number based on the first byte pattern
    // RFC 9639 Section 9.1.5 / Table 18 (extended UTF-8 encoding)
    size_t num_bytes = 0;
    uint64_t value = 0;
    
    // Requirement 9.2: If first byte is 0b0xxxxxxx, read 1-byte coded number
    if ((first_byte & 0x80) == 0x00) {
        // 1-byte encoding: 0xxxxxxx (7 bits of data)
        num_bytes = 1;
        value = first_byte & 0x7F;
        FLAC_DEBUG("[parseCodedNumber] 1-byte encoding: 0x", std::hex, static_cast<int>(first_byte), 
                   std::dec, " -> value: ", value);
    }
    // Requirement 9.3: If first byte is 0b110xxxxx, read 2-byte coded number
    else if ((first_byte & 0xE0) == 0xC0) {
        // 2-byte encoding: 110xxxxx 10xxxxxx (11 bits of data)
        num_bytes = 2;
        value = first_byte & 0x1F;
        FLAC_DEBUG("[parseCodedNumber] 2-byte encoding detected");
    }
    // Requirement 9.4: If first byte is 0b1110xxxx, read 3-byte coded number
    else if ((first_byte & 0xF0) == 0xE0) {
        // 3-byte encoding: 1110xxxx 10xxxxxx 10xxxxxx (16 bits of data)
        num_bytes = 3;
        value = first_byte & 0x0F;
        FLAC_DEBUG("[parseCodedNumber] 3-byte encoding detected");
    }
    // Requirement 9.5: If first byte is 0b11110xxx, read 4-byte coded number
    else if ((first_byte & 0xF8) == 0xF0) {
        // 4-byte encoding: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx (21 bits of data)
        num_bytes = 4;
        value = first_byte & 0x07;
        FLAC_DEBUG("[parseCodedNumber] 4-byte encoding detected");
    }
    // Requirement 9.6: If first byte is 0b111110xx, read 5-byte coded number
    else if ((first_byte & 0xFC) == 0xF8) {
        // 5-byte encoding: 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx (26 bits of data)
        num_bytes = 5;
        value = first_byte & 0x03;
        FLAC_DEBUG("[parseCodedNumber] 5-byte encoding detected");
    }
    // Requirement 9.7: If first byte is 0b1111110x, read 6-byte coded number
    else if ((first_byte & 0xFE) == 0xFC) {
        // 6-byte encoding: 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx (31 bits of data)
        num_bytes = 6;
        value = first_byte & 0x01;
        FLAC_DEBUG("[parseCodedNumber] 6-byte encoding detected");
    }
    // Requirement 9.8: If first byte is 0b11111110, read 7-byte coded number
    else if (first_byte == 0xFE) {
        // 7-byte encoding: 11111110 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx (36 bits of data)
        num_bytes = 7;
        value = 0;  // No data bits in first byte for 7-byte encoding
        FLAC_DEBUG("[parseCodedNumber] 7-byte encoding detected");
    }
    else {
        // Invalid first byte pattern (0xFF or other invalid patterns)
        FLAC_DEBUG("[parseCodedNumber] REJECTED: Invalid first byte pattern 0x", 
                   std::hex, static_cast<int>(first_byte), std::dec);
        return false;
    }
    
    // Validate that we have enough bytes in the buffer
    if (buffer_size < num_bytes) {
        FLAC_DEBUG("[parseCodedNumber] Insufficient buffer: need ", num_bytes, 
                   " bytes, have ", buffer_size);
        return false;
    }
    
    // For fixed block size streams (frame number), max is 6 bytes (31 bits)
    // RFC 9639: "When a frame number is encoded, the value MUST NOT be larger than
    // what fits a value of 31 bits unencoded or 6 bytes encoded."
    if (!is_variable_block_size && num_bytes > 6) {
        FLAC_DEBUG("[parseCodedNumber] REJECTED: Frame number encoding exceeds 6 bytes (", 
                   num_bytes, " bytes) - invalid for fixed block size stream");
        return false;
    }
    
    // Read continuation bytes (each has pattern 10xxxxxx)
    for (size_t i = 1; i < num_bytes; ++i) {
        uint8_t cont_byte = buffer[i];
        
        // Validate continuation byte pattern: must be 10xxxxxx
        if ((cont_byte & 0xC0) != 0x80) {
            FLAC_DEBUG("[parseCodedNumber] REJECTED: Invalid continuation byte ", i, 
                       ": 0x", std::hex, static_cast<int>(cont_byte), std::dec,
                       " (expected 10xxxxxx pattern)");
            return false;
        }
        
        // Extract 6 bits of data from continuation byte and add to value
        value = (value << 6) | (cont_byte & 0x3F);
    }
    
    // Requirement 9.9: For fixed block size stream, interpret as frame number
    // Requirement 9.10: For variable block size stream, interpret as sample number
    coded_number = value;
    bytes_consumed = num_bytes;
    
    FLAC_DEBUG("[parseCodedNumber] Decoded ", num_bytes, "-byte coded number: ", coded_number,
               " (", is_variable_block_size ? "sample number" : "frame number", ")");
    
    return true;
}


// ============================================================================
// Frame Size Estimation (Requirement 21)
// ============================================================================

uint32_t FLACDemuxer::calculateFrameSize_unlocked(const FLACFrame& frame) const
{
    FLAC_DEBUG("Calculating frame size");
    
    // Requirement 21.1: Use STREAMINFO minimum frame size as primary estimate
    if (m_streaminfo.min_frame_size > 0) {
        FLAC_DEBUG("Using STREAMINFO min_frame_size: ", m_streaminfo.min_frame_size);
        return m_streaminfo.min_frame_size;
    }
    
    // Requirement 21.4: Conservative fallback
    // Minimum FLAC frame: sync(2) + header(~4) + subframe(~1) + crc(2) = ~9 bytes
    // Use 64 bytes as safe minimum for unknown streams
    FLAC_DEBUG("Using conservative fallback: 64 bytes");
    return 64;
}

// ============================================================================
// Utility Methods
// ============================================================================

uint64_t FLACDemuxer::samplesToMs(uint64_t samples) const
{
    if (m_streaminfo.sample_rate == 0) return 0;
    return (samples * 1000) / m_streaminfo.sample_rate;
}

uint64_t FLACDemuxer::msToSamples(uint64_t ms) const
{
    if (m_streaminfo.sample_rate == 0) return 0;
    return (ms * m_streaminfo.sample_rate) / 1000;
}

} // namespace FLAC
} // namespace Demuxer
} // namespace PsyMP3
