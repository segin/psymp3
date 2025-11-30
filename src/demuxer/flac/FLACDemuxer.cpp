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
    bool is_last = false;
    
    while (!is_last) {
        FLACMetadataBlock block;
        
        if (!parseMetadataBlockHeader_unlocked(block)) {
            reportError("Format", "Failed to parse metadata block header");
            return false;
        }
        
        is_last = block.is_last;
        
        FLAC_DEBUG("Metadata block: type=", static_cast<int>(block.type), 
                   ", length=", block.length, ", is_last=", is_last);
        
        // Process block based on type
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
            case FLACMetadataType::APPLICATION:
            case FLACMetadataType::CUESHEET:
            case FLACMetadataType::PICTURE:
            default:
                // Skip unhandled blocks
                skipMetadataBlock_unlocked(block);
                break;
        }
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
    // RFC 9639 Section 8.1: 4-byte header
    uint8_t header[4];
    
    if (m_handler->read(header, 1, 4) != 4) {
        return false;
    }
    
    // Bit 7 of byte 0: is_last flag
    block.is_last = (header[0] & 0x80) != 0;
    
    // Bits 0-6 of byte 0: block type
    uint8_t type = header[0] & 0x7F;
    
    // RFC 9639 Table 1: Block type 127 is forbidden
    if (type == 127) {
        reportError("Format", "Forbidden metadata block type 127");
        return false;
    }
    
    block.type = static_cast<FLACMetadataType>(type);
    
    // Bytes 1-3: 24-bit big-endian block length
    block.length = (static_cast<uint32_t>(header[1]) << 16) |
                   (static_cast<uint32_t>(header[2]) << 8) |
                   static_cast<uint32_t>(header[3]);
    
    block.data_offset = static_cast<uint64_t>(m_handler->tell());
    
    return true;
}


// ============================================================================
// STREAMINFO Block Parsing (RFC 9639 Section 8.2)
// ============================================================================

bool FLACDemuxer::parseStreamInfoBlock_unlocked(const FLACMetadataBlock& block)
{
    FLAC_DEBUG("Parsing STREAMINFO block");
    
    // RFC 9639 Section 8.2: STREAMINFO is exactly 34 bytes
    if (block.length != 34) {
        reportError("Format", "Invalid STREAMINFO length: " + std::to_string(block.length));
        return false;
    }
    
    uint8_t data[34];
    if (m_handler->read(data, 1, 34) != 34) {
        reportError("IO", "Failed to read STREAMINFO data");
        return false;
    }
    
    // Parse STREAMINFO fields (all big-endian)
    // Bytes 0-1: minimum block size (u16)
    m_streaminfo.min_block_size = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    
    // Bytes 2-3: maximum block size (u16)
    m_streaminfo.max_block_size = (static_cast<uint16_t>(data[2]) << 8) | data[3];
    
    // Bytes 4-6: minimum frame size (u24)
    m_streaminfo.min_frame_size = (static_cast<uint32_t>(data[4]) << 16) |
                                   (static_cast<uint32_t>(data[5]) << 8) |
                                   data[6];
    
    // Bytes 7-9: maximum frame size (u24)
    m_streaminfo.max_frame_size = (static_cast<uint32_t>(data[7]) << 16) |
                                   (static_cast<uint32_t>(data[8]) << 8) |
                                   data[9];
    
    // Bytes 10-13: sample rate (u20), channels-1 (u3), bits_per_sample-1 (u5)
    // Byte 10: sample_rate[19:12]
    // Byte 11: sample_rate[11:4]
    // Byte 12: sample_rate[3:0], channels-1[2:0], bits_per_sample-1[4]
    // Byte 13: bits_per_sample-1[3:0], total_samples[35:32]
    
    m_streaminfo.sample_rate = (static_cast<uint32_t>(data[10]) << 12) |
                                (static_cast<uint32_t>(data[11]) << 4) |
                                (data[12] >> 4);
    
    m_streaminfo.channels = ((data[12] >> 1) & 0x07) + 1;
    
    m_streaminfo.bits_per_sample = (((data[12] & 0x01) << 4) | (data[13] >> 4)) + 1;
    
    // Bytes 13-17: total samples (u36)
    m_streaminfo.total_samples = (static_cast<uint64_t>(data[13] & 0x0F) << 32) |
                                  (static_cast<uint64_t>(data[14]) << 24) |
                                  (static_cast<uint64_t>(data[15]) << 16) |
                                  (static_cast<uint64_t>(data[16]) << 8) |
                                  data[17];
    
    // Bytes 18-33: MD5 signature (128 bits)
    std::memcpy(m_streaminfo.md5_signature, &data[18], 16);
    
    // RFC 9639 Table 1: min/max block size < 16 is forbidden
    if (m_streaminfo.min_block_size < 16) {
        reportError("Format", "Forbidden: min_block_size < 16");
        return false;
    }
    if (m_streaminfo.max_block_size < 16) {
        reportError("Format", "Forbidden: max_block_size < 16");
        return false;
    }
    
    // Validate min <= max
    if (m_streaminfo.min_block_size > m_streaminfo.max_block_size) {
        reportError("Format", "Invalid: min_block_size > max_block_size");
        return false;
    }
    
    // RFC 9639: sample rate must not be 0 for audio streams
    if (m_streaminfo.sample_rate == 0) {
        reportError("Format", "Invalid: sample_rate is 0");
        return false;
    }
    
    FLAC_DEBUG("STREAMINFO parsed successfully");
    return true;
}

// ============================================================================
// SEEKTABLE Block Parsing (RFC 9639 Section 8.5)
// ============================================================================

bool FLACDemuxer::parseSeekTableBlock_unlocked(const FLACMetadataBlock& block)
{
    FLAC_DEBUG("Parsing SEEKTABLE block");
    
    // Each seek point is 18 bytes
    if (block.length % 18 != 0) {
        FLAC_DEBUG("Invalid SEEKTABLE length, skipping");
        skipMetadataBlock_unlocked(block);
        return false;
    }
    
    size_t num_points = block.length / 18;
    FLAC_DEBUG("SEEKTABLE contains ", num_points, " seek points");
    
    m_seektable.clear();
    m_seektable.reserve(num_points);
    
    for (size_t i = 0; i < num_points; ++i) {
        uint8_t data[18];
        if (m_handler->read(data, 1, 18) != 18) {
            FLAC_DEBUG("Failed to read seek point ", i);
            break;
        }
        
        FLACSeekPoint point;
        
        // u64 sample number (big-endian)
        point.sample_number = (static_cast<uint64_t>(data[0]) << 56) |
                               (static_cast<uint64_t>(data[1]) << 48) |
                               (static_cast<uint64_t>(data[2]) << 40) |
                               (static_cast<uint64_t>(data[3]) << 32) |
                               (static_cast<uint64_t>(data[4]) << 24) |
                               (static_cast<uint64_t>(data[5]) << 16) |
                               (static_cast<uint64_t>(data[6]) << 8) |
                               data[7];
        
        // u64 stream offset (big-endian)
        point.stream_offset = (static_cast<uint64_t>(data[8]) << 56) |
                               (static_cast<uint64_t>(data[9]) << 48) |
                               (static_cast<uint64_t>(data[10]) << 40) |
                               (static_cast<uint64_t>(data[11]) << 32) |
                               (static_cast<uint64_t>(data[12]) << 24) |
                               (static_cast<uint64_t>(data[13]) << 16) |
                               (static_cast<uint64_t>(data[14]) << 8) |
                               data[15];
        
        // u16 frame samples (big-endian)
        point.frame_samples = (static_cast<uint16_t>(data[16]) << 8) | data[17];
        
        // Skip placeholder seek points (sample_number = 0xFFFFFFFFFFFFFFFF)
        if (!point.isPlaceholder()) {
            m_seektable.push_back(point);
        }
    }
    
    FLAC_DEBUG("Loaded ", m_seektable.size(), " valid seek points");
    return true;
}


// ============================================================================
// VORBIS_COMMENT Block Parsing (RFC 9639 Section 8.6)
// ============================================================================

bool FLACDemuxer::parseVorbisCommentBlock_unlocked(const FLACMetadataBlock& block)
{
    FLAC_DEBUG("Parsing VORBIS_COMMENT block");
    
    uint64_t block_end = block.data_offset + block.length;
    
    // Vendor string length (u32 little-endian - exception to big-endian rule)
    uint8_t len_buf[4];
    if (m_handler->read(len_buf, 1, 4) != 4) {
        FLAC_DEBUG("Failed to read vendor length");
        return false;
    }
    
    uint32_t vendor_len = len_buf[0] | (len_buf[1] << 8) | 
                          (len_buf[2] << 16) | (len_buf[3] << 24);
    
    // Skip vendor string
    if (vendor_len > 0) {
        m_handler->seek(static_cast<off_t>(vendor_len), SEEK_CUR);
    }
    
    // Field count (u32 little-endian)
    if (m_handler->read(len_buf, 1, 4) != 4) {
        FLAC_DEBUG("Failed to read field count");
        return false;
    }
    
    uint32_t field_count = len_buf[0] | (len_buf[1] << 8) | 
                           (len_buf[2] << 16) | (len_buf[3] << 24);
    
    FLAC_DEBUG("VORBIS_COMMENT has ", field_count, " fields");
    
    // Parse each field
    for (uint32_t i = 0; i < field_count; ++i) {
        // Check we haven't exceeded block bounds
        if (static_cast<uint64_t>(m_handler->tell()) >= block_end) {
            break;
        }
        
        // Field length (u32 little-endian)
        if (m_handler->read(len_buf, 1, 4) != 4) {
            break;
        }
        
        uint32_t field_len = len_buf[0] | (len_buf[1] << 8) | 
                             (len_buf[2] << 16) | (len_buf[3] << 24);
        
        if (field_len == 0 || field_len > 8192) {
            // Skip invalid or too-long fields
            if (field_len > 0) {
                m_handler->seek(static_cast<off_t>(field_len), SEEK_CUR);
            }
            continue;
        }
        
        // Read field content
        std::vector<char> field_data(field_len);
        if (m_handler->read(field_data.data(), 1, field_len) != field_len) {
            break;
        }
        
        std::string field(field_data.begin(), field_data.end());
        
        // Split on first '=' into name and value
        size_t eq_pos = field.find('=');
        if (eq_pos != std::string::npos && eq_pos > 0) {
            std::string name = field.substr(0, eq_pos);
            std::string value = field.substr(eq_pos + 1);
            
            // Convert name to uppercase for case-insensitive lookup
            for (char& c : name) {
                if (c >= 'a' && c <= 'z') c -= 32;
            }
            
            m_vorbis_comments[name] = value;
        }
    }
    
    FLAC_DEBUG("Parsed ", m_vorbis_comments.size(), " Vorbis comments");
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

bool FLACDemuxer::findNextFrame_unlocked(FLACFrame& frame)
{
    // Search for frame sync code (RFC 9639 Section 9.1)
    // Sync code is 15 bits: 0b111111111111100 (0xFFF8 or 0xFFF9)
    
    // Limit search to 512 bytes max per Requirement 21.3
    static constexpr size_t MAX_SEARCH_BYTES = 512;
    
    uint8_t buffer[MAX_SEARCH_BYTES];
    size_t search_start = m_current_offset;
    
    // Seek to current position
    if (m_handler->seek(static_cast<off_t>(m_current_offset), SEEK_SET) != 0) {
        return false;
    }
    
    size_t bytes_read = m_handler->read(buffer, 1, MAX_SEARCH_BYTES);
    if (bytes_read < 2) {
        return false;
    }
    
    // Search for sync pattern
    for (size_t i = 0; i < bytes_read - 1; ++i) {
        // Check for 0xFF followed by 0xF8 (fixed) or 0xF9 (variable)
        if (buffer[i] == 0xFF && (buffer[i + 1] == 0xF8 || buffer[i + 1] == 0xF9)) {
            // Found potential sync code
            frame.file_offset = search_start + i;
            frame.variable_block_size = (buffer[i + 1] == 0xF9);
            
            // Parse frame header
            if (parseFrameHeader_unlocked(frame, &buffer[i], bytes_read - i)) {
                FLAC_DEBUG("Found frame at offset ", frame.file_offset);
                return true;
            }
        }
    }
    
    FLAC_DEBUG("No frame sync found in ", bytes_read, " bytes");
    return false;
}

bool FLACDemuxer::parseFrameHeader_unlocked(FLACFrame& frame, const uint8_t* buffer, size_t size)
{
    if (size < 4) {
        return false;
    }
    
    // Byte 0-1: Sync code (already validated)
    // Byte 2: Block size bits (4) + Sample rate bits (4)
    // Byte 3: Channel bits (4) + Bit depth bits (3) + Reserved (1)
    
    uint8_t block_size_bits = (buffer[2] >> 4) & 0x0F;
    uint8_t sample_rate_bits = buffer[2] & 0x0F;
    uint8_t channel_bits = (buffer[3] >> 4) & 0x0F;
    uint8_t bit_depth_bits = (buffer[3] >> 1) & 0x07;
    
    // RFC 9639 Table 1: Reserved block size pattern 0b0000
    if (block_size_bits == 0) {
        return false;
    }
    
    // RFC 9639 Table 1: Forbidden sample rate pattern 0b1111
    if (sample_rate_bits == 0x0F) {
        return false;
    }
    
    // RFC 9639 Table 1: Reserved channel bits 0b1011-0b1111
    if (channel_bits >= 0x0B) {
        return false;
    }
    
    // RFC 9639 Table 1: Reserved bit depth pattern 0b011
    if (bit_depth_bits == 0x03) {
        return false;
    }
    
    // Parse block size (RFC 9639 Table 14)
    switch (block_size_bits) {
        case 0x01: frame.block_size = 192; break;
        case 0x02: frame.block_size = 576; break;
        case 0x03: frame.block_size = 1152; break;
        case 0x04: frame.block_size = 2304; break;
        case 0x05: frame.block_size = 4608; break;
        case 0x06: frame.block_size = 256; break;  // 8-bit uncommon - simplified
        case 0x07: frame.block_size = 256; break;  // 16-bit uncommon - simplified
        case 0x08: frame.block_size = 256; break;
        case 0x09: frame.block_size = 512; break;
        case 0x0A: frame.block_size = 1024; break;
        case 0x0B: frame.block_size = 2048; break;
        case 0x0C: frame.block_size = 4096; break;
        case 0x0D: frame.block_size = 8192; break;
        case 0x0E: frame.block_size = 16384; break;
        case 0x0F: frame.block_size = 32768; break;
        default: frame.block_size = m_streaminfo.max_block_size; break;
    }
    
    // Parse sample rate
    if (sample_rate_bits == 0x00) {
        frame.sample_rate = m_streaminfo.sample_rate;
    } else {
        // Use lookup table for common rates
        static const uint32_t sample_rates[] = {
            0, 88200, 176400, 192000, 8000, 16000, 22050, 24000,
            32000, 44100, 48000, 96000, 0, 0, 0, 0
        };
        frame.sample_rate = sample_rates[sample_rate_bits];
        if (frame.sample_rate == 0) {
            frame.sample_rate = m_streaminfo.sample_rate;
        }
    }
    
    // Parse channels
    if (channel_bits <= 0x07) {
        frame.channels = channel_bits + 1;
    } else {
        frame.channels = 2;  // Stereo modes (left-side, right-side, mid-side)
    }
    
    // Parse bit depth
    if (bit_depth_bits == 0x00) {
        frame.bits_per_sample = m_streaminfo.bits_per_sample;
    } else {
        static const uint8_t bit_depths[] = {0, 8, 12, 0, 16, 20, 24, 32};
        frame.bits_per_sample = bit_depths[bit_depth_bits];
        if (frame.bits_per_sample == 0) {
            frame.bits_per_sample = m_streaminfo.bits_per_sample;
        }
    }
    
    // Set sample offset based on current position
    frame.sample_offset = m_current_sample;
    
    return frame.isValid();
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
