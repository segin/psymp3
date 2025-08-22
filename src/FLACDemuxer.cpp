/*
 * FLACDemuxer.cpp - FLAC container demuxer implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "psymp3.h"

FLACDemuxer::FLACDemuxer(std::unique_ptr<IOHandler> handler)
    : Demuxer(std::move(handler))
{
    Debug::log("flac", "FLACDemuxer constructor called");
    
    // Initialize state
    m_container_parsed = false;
    m_file_size = 0;
    m_audio_data_offset = 0;
    m_current_offset = 0;
    m_current_sample = 0;
    m_last_block_size = 0;
    
    // Get file size if possible
    if (m_handler) {
        m_file_size = static_cast<uint64_t>(m_handler->getFileSize());
        if (m_file_size == static_cast<uint64_t>(-1)) {
            m_file_size = 0;
        }
    }
}

FLACDemuxer::~FLACDemuxer()
{
    Debug::log("flac", "FLACDemuxer destructor called");
    
    // Clear metadata containers
    m_seektable.clear();
    m_vorbis_comments.clear();
    m_pictures.clear();
    
    // Base class destructor will handle IOHandler cleanup
}

bool FLACDemuxer::parseContainer()
{
    Debug::log("flac", "FLACDemuxer::parseContainer() - parsing FLAC container");
    
    if (!m_handler) {
        reportError("IO", "No IOHandler available for parsing");
        return false;
    }
    
    if (m_container_parsed) {
        Debug::log("flac", "Container already parsed");
        return true;
    }
    
    // Seek to beginning of file
    if (!m_handler->seek(0, SEEK_SET)) {
        reportError("IO", "Failed to seek to beginning of file");
        return false;
    }
    
    // Validate fLaC stream marker (4 bytes)
    uint8_t marker[4];
    if (m_handler->read(marker, 1, 4) != 4) {
        reportError("IO", "Failed to read FLAC stream marker");
        return false;
    }
    
    if (marker[0] != 'f' || marker[1] != 'L' || marker[2] != 'a' || marker[3] != 'C') {
        reportError("Format", "Invalid FLAC stream marker - not a FLAC file");
        return false;
    }
    
    Debug::log("flac", "Valid fLaC stream marker found");
    
    // Parse metadata blocks
    if (!parseMetadataBlocks()) {
        reportError("Format", "Failed to parse FLAC metadata blocks");
        return false;
    }
    
    // Verify we have valid STREAMINFO
    if (!m_streaminfo.isValid()) {
        reportError("Format", "Missing or invalid STREAMINFO block");
        return false;
    }
    
    // Container parsing successful
    m_container_parsed = true;
    
    Debug::log("flac", "FLAC container parsing completed successfully");
    Debug::log("flac", "Audio data starts at offset: ", m_audio_data_offset);
    
    return true;
}

std::vector<StreamInfo> FLACDemuxer::getStreams() const
{
    Debug::log("flac", "FLACDemuxer::getStreams() - returning FLAC stream info");
    
    if (!m_container_parsed) {
        Debug::log("flac", "Container not parsed, returning empty stream list");
        return {};
    }
    
    if (!m_streaminfo.isValid()) {
        Debug::log("flac", "Invalid STREAMINFO, returning empty stream list");
        return {};
    }
    
    // Create StreamInfo for the FLAC audio stream
    StreamInfo stream;
    stream.stream_id = 1;
    stream.codec_type = "audio";
    stream.codec_name = "flac";
    stream.sample_rate = m_streaminfo.sample_rate;
    stream.channels = m_streaminfo.channels;
    stream.bits_per_sample = m_streaminfo.bits_per_sample;
    stream.duration_ms = m_streaminfo.getDurationMs();
    
    // Add metadata if available
    auto artist_it = m_vorbis_comments.find("ARTIST");
    if (artist_it != m_vorbis_comments.end()) {
        stream.artist = artist_it->second;
    }
    
    auto title_it = m_vorbis_comments.find("TITLE");
    if (title_it != m_vorbis_comments.end()) {
        stream.title = title_it->second;
    }
    
    auto album_it = m_vorbis_comments.find("ALBUM");
    if (album_it != m_vorbis_comments.end()) {
        stream.album = album_it->second;
    }
    
    return {stream};
}

StreamInfo FLACDemuxer::getStreamInfo(uint32_t stream_id) const
{
    Debug::log("flac", "FLACDemuxer::getStreamInfo(", stream_id, ") - returning FLAC stream info");
    
    if (!m_container_parsed) {
        Debug::log("flac", "Container not parsed");
        return StreamInfo{};
    }
    
    if (stream_id != 1) {
        Debug::log("flac", "Invalid stream ID for FLAC: ", stream_id);
        return StreamInfo{};
    }
    
    auto streams = getStreams();
    if (streams.empty()) {
        return StreamInfo{};
    }
    
    return streams[0];
}

MediaChunk FLACDemuxer::readChunk()
{
    Debug::log("flac", "FLACDemuxer::readChunk() - placeholder implementation");
    
    if (!m_container_parsed) {
        reportError("State", "Container not parsed");
        return MediaChunk{};
    }
    
    if (isEOF()) {
        Debug::log("flac", "At end of file");
        return MediaChunk{};
    }
    
    // TODO: Implement frame reading
    // For now, return empty chunk
    return MediaChunk{};
}

MediaChunk FLACDemuxer::readChunk(uint32_t stream_id)
{
    Debug::log("flac", "FLACDemuxer::readChunk(", stream_id, ") - placeholder implementation");
    
    if (stream_id != 1) {
        reportError("Stream", "Invalid stream ID for FLAC: " + std::to_string(stream_id));
        return MediaChunk{};
    }
    
    return readChunk();
}

bool FLACDemuxer::seekTo(uint64_t timestamp_ms)
{
    Debug::log("flac", "FLACDemuxer::seekTo(", timestamp_ms, ") - placeholder implementation");
    
    if (!m_container_parsed) {
        reportError("State", "Container not parsed");
        return false;
    }
    
    // TODO: Implement seeking
    // For now, return false to indicate not implemented
    reportError("Feature", "FLAC seeking not yet implemented");
    return false;
}

bool FLACDemuxer::isEOF() const
{
    if (!m_handler) {
        return true;
    }
    
    return m_handler->eof() || (m_file_size > 0 && m_current_offset >= m_file_size);
}

uint64_t FLACDemuxer::getDuration() const
{
    Debug::log("flac", "FLACDemuxer::getDuration() - placeholder implementation");
    
    if (!m_container_parsed || !m_streaminfo.isValid()) {
        return 0;
    }
    
    return m_streaminfo.getDurationMs();
}

uint64_t FLACDemuxer::getPosition() const
{
    Debug::log("flac", "FLACDemuxer::getPosition() - placeholder implementation");
    
    if (!m_container_parsed || !m_streaminfo.isValid()) {
        return 0;
    }
    
    return samplesToMs(m_current_sample);
}

// Private helper methods - implementations

bool FLACDemuxer::parseMetadataBlockHeader(FLACMetadataBlock& block)
{
    Debug::log("flac", "FLACDemuxer::parseMetadataBlockHeader() - parsing block header");
    
    if (!m_handler) {
        return false;
    }
    
    // Read 4-byte metadata block header
    uint8_t header[4];
    if (m_handler->read(header, 1, 4) != 4) {
        Debug::log("flac", "Failed to read metadata block header");
        return false;
    }
    
    // Parse header fields
    // Bit 0: is_last flag
    // Bits 1-7: block type
    // Bits 8-31: block length (24-bit big-endian)
    
    block.is_last = (header[0] & 0x80) != 0;
    uint8_t block_type = header[0] & 0x7F;
    
    // Convert block type to enum
    if (block_type <= static_cast<uint8_t>(FLACMetadataType::PICTURE)) {
        block.type = static_cast<FLACMetadataType>(block_type);
    } else {
        block.type = FLACMetadataType::INVALID;
    }
    
    // Parse 24-bit big-endian length
    block.length = (static_cast<uint32_t>(header[1]) << 16) |
                   (static_cast<uint32_t>(header[2]) << 8) |
                   static_cast<uint32_t>(header[3]);
    
    // Store current position as data offset
    block.data_offset = static_cast<uint64_t>(m_handler->tell());
    
    Debug::log("flac", "Metadata block: type=", static_cast<int>(block.type), 
               " is_last=", block.is_last, " length=", block.length, 
               " data_offset=", block.data_offset);
    
    return true;
}

bool FLACDemuxer::parseMetadataBlocks()
{
    Debug::log("flac", "FLACDemuxer::parseMetadataBlocks() - parsing metadata blocks");
    
    if (!m_handler) {
        reportError("IO", "No IOHandler available for metadata parsing");
        return false;
    }
    
    bool found_streaminfo = false;
    bool is_last_block = false;
    
    while (!is_last_block && !m_handler->eof()) {
        FLACMetadataBlock block;
        
        // Parse metadata block header
        if (!parseMetadataBlockHeader(block)) {
            reportError("Format", "Failed to parse metadata block header");
            return false;
        }
        
        is_last_block = block.is_last;
        
        // Validate block structure and size limits
        if (!block.isValid()) {
            Debug::log("flac", "Invalid metadata block, skipping");
            if (!skipMetadataBlock(block)) {
                reportError("IO", "Failed to skip invalid metadata block");
                return false;
            }
            continue;
        }
        
        // Check for reasonable size limits (16MB max for any metadata block)
        if (block.length > 16 * 1024 * 1024) {
            Debug::log("flac", "Metadata block too large (", block.length, " bytes), skipping");
            if (!skipMetadataBlock(block)) {
                reportError("IO", "Failed to skip oversized metadata block");
                return false;
            }
            continue;
        }
        
        // Process block based on type
        bool parse_success = false;
        switch (block.type) {
            case FLACMetadataType::STREAMINFO:
                Debug::log("flac", "Processing STREAMINFO block");
                parse_success = parseStreamInfoBlock(block);
                found_streaminfo = true;
                break;
                
            case FLACMetadataType::SEEKTABLE:
                Debug::log("flac", "Processing SEEKTABLE block");
                parse_success = parseSeekTableBlock(block);
                break;
                
            case FLACMetadataType::VORBIS_COMMENT:
                Debug::log("flac", "Processing VORBIS_COMMENT block");
                parse_success = parseVorbisCommentBlock(block);
                break;
                
            case FLACMetadataType::PICTURE:
                Debug::log("flac", "Processing PICTURE block");
                parse_success = parsePictureBlock(block);
                break;
                
            case FLACMetadataType::PADDING:
                Debug::log("flac", "Skipping PADDING block");
                parse_success = skipMetadataBlock(block);
                break;
                
            case FLACMetadataType::APPLICATION:
                Debug::log("flac", "Skipping APPLICATION block");
                parse_success = skipMetadataBlock(block);
                break;
                
            case FLACMetadataType::CUESHEET:
                Debug::log("flac", "Skipping CUESHEET block");
                parse_success = skipMetadataBlock(block);
                break;
                
            default:
                Debug::log("flac", "Skipping unknown metadata block type: ", static_cast<int>(block.type));
                parse_success = skipMetadataBlock(block);
                break;
        }
        
        if (!parse_success) {
            Debug::log("flac", "Failed to process metadata block type: ", static_cast<int>(block.type));
            // Try to skip the block and continue
            if (!skipMetadataBlock(block)) {
                reportError("IO", "Failed to skip metadata block after parse error");
                return false;
            }
        }
    }
    
    // STREAMINFO is mandatory
    if (!found_streaminfo) {
        reportError("Format", "FLAC file missing mandatory STREAMINFO block");
        return false;
    }
    
    // Store current position as start of audio data
    m_audio_data_offset = static_cast<uint64_t>(m_handler->tell());
    m_current_offset = m_audio_data_offset;
    
    Debug::log("flac", "Metadata parsing complete, audio data starts at offset: ", m_audio_data_offset);
    return true;
}

bool FLACDemuxer::parseStreamInfoBlock(const FLACMetadataBlock& block)
{
    Debug::log("flac", "FLACDemuxer::parseStreamInfoBlock() - parsing STREAMINFO block");
    
    if (!m_handler) {
        return false;
    }
    
    // STREAMINFO block must be exactly 34 bytes
    if (block.length != 34) {
        reportError("Format", "Invalid STREAMINFO block length: " + std::to_string(block.length));
        return false;
    }
    
    // Read STREAMINFO data
    uint8_t data[34];
    if (m_handler->read(data, 1, 34) != 34) {
        reportError("IO", "Failed to read STREAMINFO block data");
        return false;
    }
    
    // Parse STREAMINFO fields (all big-endian)
    
    // Minimum block size (16 bits)
    m_streaminfo.min_block_size = (static_cast<uint16_t>(data[0]) << 8) | 
                                  static_cast<uint16_t>(data[1]);
    
    // Maximum block size (16 bits)
    m_streaminfo.max_block_size = (static_cast<uint16_t>(data[2]) << 8) | 
                                  static_cast<uint16_t>(data[3]);
    
    // Minimum frame size (24 bits)
    m_streaminfo.min_frame_size = (static_cast<uint32_t>(data[4]) << 16) |
                                  (static_cast<uint32_t>(data[5]) << 8) |
                                  static_cast<uint32_t>(data[6]);
    
    // Maximum frame size (24 bits)
    m_streaminfo.max_frame_size = (static_cast<uint32_t>(data[7]) << 16) |
                                  (static_cast<uint32_t>(data[8]) << 8) |
                                  static_cast<uint32_t>(data[9]);
    
    // Sample rate (20 bits), channels (3 bits), bits per sample (5 bits)
    uint32_t sample_rate_and_channels = (static_cast<uint32_t>(data[10]) << 12) |
                                        (static_cast<uint32_t>(data[11]) << 4) |
                                        (static_cast<uint32_t>(data[12]) >> 4);
    
    m_streaminfo.sample_rate = sample_rate_and_channels >> 4;  // Top 20 bits
    m_streaminfo.channels = ((sample_rate_and_channels & 0x0E) >> 1) + 1;  // Next 3 bits + 1
    m_streaminfo.bits_per_sample = ((data[12] & 0x01) << 4) | ((data[13] & 0xF0) >> 4);  // Next 5 bits
    m_streaminfo.bits_per_sample += 1;  // Add 1 to get actual bits per sample
    
    // Total samples (36 bits)
    m_streaminfo.total_samples = (static_cast<uint64_t>(data[13] & 0x0F) << 32) |
                                 (static_cast<uint64_t>(data[14]) << 24) |
                                 (static_cast<uint64_t>(data[15]) << 16) |
                                 (static_cast<uint64_t>(data[16]) << 8) |
                                 static_cast<uint64_t>(data[17]);
    
    // MD5 signature (16 bytes)
    std::memcpy(m_streaminfo.md5_signature, &data[18], 16);
    
    // Validate parsed data
    if (!m_streaminfo.isValid()) {
        reportError("Format", "Invalid STREAMINFO data");
        return false;
    }
    
    Debug::log("flac", "STREAMINFO parsed successfully:");
    Debug::log("flac", "  Sample rate: ", m_streaminfo.sample_rate, " Hz");
    Debug::log("flac", "  Channels: ", static_cast<int>(m_streaminfo.channels));
    Debug::log("flac", "  Bits per sample: ", static_cast<int>(m_streaminfo.bits_per_sample));
    Debug::log("flac", "  Total samples: ", m_streaminfo.total_samples);
    Debug::log("flac", "  Duration: ", m_streaminfo.getDurationMs(), " ms");
    Debug::log("flac", "  Block size range: ", m_streaminfo.min_block_size, "-", m_streaminfo.max_block_size);
    
    if (m_streaminfo.min_frame_size > 0 && m_streaminfo.max_frame_size > 0) {
        Debug::log("flac", "  Frame size range: ", m_streaminfo.min_frame_size, "-", m_streaminfo.max_frame_size);
    }
    
    return true;
}

bool FLACDemuxer::parseSeekTableBlock(const FLACMetadataBlock& block)
{
    Debug::log("flac", "FLACDemuxer::parseSeekTableBlock() - parsing SEEKTABLE block");
    
    if (!m_handler) {
        return false;
    }
    
    // Each seek point is 18 bytes (3 * 64-bit values, but packed)
    // Sample number (64 bits), stream offset (64 bits), frame samples (16 bits)
    const uint32_t seek_point_size = 18;
    
    if (block.length % seek_point_size != 0) {
        reportError("Format", "Invalid SEEKTABLE block length: " + std::to_string(block.length));
        return false;
    }
    
    uint32_t num_seek_points = block.length / seek_point_size;
    Debug::log("flac", "SEEKTABLE contains ", num_seek_points, " seek points");
    
    // Clear existing seek table
    m_seektable.clear();
    m_seektable.reserve(num_seek_points);
    
    // Read and parse each seek point
    for (uint32_t i = 0; i < num_seek_points; i++) {
        uint8_t seek_data[18];
        if (m_handler->read(seek_data, 1, 18) != 18) {
            reportError("IO", "Failed to read seek point " + std::to_string(i));
            return false;
        }
        
        FLACSeekPoint seek_point;
        
        // Parse sample number (64 bits, big-endian)
        seek_point.sample_number = (static_cast<uint64_t>(seek_data[0]) << 56) |
                                   (static_cast<uint64_t>(seek_data[1]) << 48) |
                                   (static_cast<uint64_t>(seek_data[2]) << 40) |
                                   (static_cast<uint64_t>(seek_data[3]) << 32) |
                                   (static_cast<uint64_t>(seek_data[4]) << 24) |
                                   (static_cast<uint64_t>(seek_data[5]) << 16) |
                                   (static_cast<uint64_t>(seek_data[6]) << 8) |
                                   static_cast<uint64_t>(seek_data[7]);
        
        // Parse stream offset (64 bits, big-endian)
        seek_point.stream_offset = (static_cast<uint64_t>(seek_data[8]) << 56) |
                                   (static_cast<uint64_t>(seek_data[9]) << 48) |
                                   (static_cast<uint64_t>(seek_data[10]) << 40) |
                                   (static_cast<uint64_t>(seek_data[11]) << 32) |
                                   (static_cast<uint64_t>(seek_data[12]) << 24) |
                                   (static_cast<uint64_t>(seek_data[13]) << 16) |
                                   (static_cast<uint64_t>(seek_data[14]) << 8) |
                                   static_cast<uint64_t>(seek_data[15]);
        
        // Parse frame samples (16 bits, big-endian)
        seek_point.frame_samples = (static_cast<uint16_t>(seek_data[16]) << 8) |
                                   static_cast<uint16_t>(seek_data[17]);
        
        // Handle placeholder seek points
        if (seek_point.isPlaceholder()) {
            Debug::log("flac", "Seek point ", i, " is a placeholder, skipping");
            continue;
        }
        
        // Validate seek point for consistency and reasonable values
        if (!seek_point.isValid()) {
            Debug::log("flac", "Invalid seek point ", i, ", skipping");
            continue;
        }
        
        // Additional validation against STREAMINFO
        if (m_streaminfo.isValid()) {
            // Check if sample number is within total samples
            if (m_streaminfo.total_samples > 0 && 
                seek_point.sample_number >= m_streaminfo.total_samples) {
                Debug::log("flac", "Seek point ", i, " sample number (", 
                          seek_point.sample_number, ") exceeds total samples (", 
                          m_streaminfo.total_samples, "), skipping");
                continue;
            }
            
            // Check if frame samples is reasonable
            if (seek_point.frame_samples > m_streaminfo.max_block_size) {
                Debug::log("flac", "Seek point ", i, " frame samples (", 
                          seek_point.frame_samples, ") exceeds max block size (", 
                          m_streaminfo.max_block_size, "), skipping");
                continue;
            }
        }
        
        // Check for reasonable stream offset (should be within file size)
        if (m_file_size > 0 && seek_point.stream_offset >= m_file_size) {
            Debug::log("flac", "Seek point ", i, " stream offset (", 
                      seek_point.stream_offset, ") exceeds file size (", 
                      m_file_size, "), skipping");
            continue;
        }
        
        // Add valid seek point to table
        m_seektable.push_back(seek_point);
        
        Debug::log("flac", "Added seek point: sample=", seek_point.sample_number, 
                  " offset=", seek_point.stream_offset, 
                  " frame_samples=", seek_point.frame_samples);
    }
    
    Debug::log("flac", "SEEKTABLE parsed successfully, ", m_seektable.size(), 
              " valid seek points out of ", num_seek_points, " total");
    
    // Sort seek table by sample number for efficient lookup
    std::sort(m_seektable.begin(), m_seektable.end(), 
              [](const FLACSeekPoint& a, const FLACSeekPoint& b) {
                  return a.sample_number < b.sample_number;
              });
    
    return true;
}

bool FLACDemuxer::parseVorbisCommentBlock(const FLACMetadataBlock& block)
{
    Debug::log("flac", "FLACDemuxer::parseVorbisCommentBlock() - parsing VORBIS_COMMENT block");
    
    if (!m_handler) {
        return false;
    }
    
    if (block.length < 8) {  // Minimum: 4 bytes vendor length + 4 bytes comment count
        reportError("Format", "VORBIS_COMMENT block too small: " + std::to_string(block.length));
        return false;
    }
    
    // Clear existing comments
    m_vorbis_comments.clear();
    
    uint32_t bytes_read = 0;
    
    // Read vendor string length (32-bit little-endian)
    uint8_t vendor_len_data[4];
    if (m_handler->read(vendor_len_data, 1, 4) != 4) {
        reportError("IO", "Failed to read vendor string length");
        return false;
    }
    bytes_read += 4;
    
    uint32_t vendor_length = static_cast<uint32_t>(vendor_len_data[0]) |
                            (static_cast<uint32_t>(vendor_len_data[1]) << 8) |
                            (static_cast<uint32_t>(vendor_len_data[2]) << 16) |
                            (static_cast<uint32_t>(vendor_len_data[3]) << 24);
    
    // Validate vendor string length
    if (vendor_length > block.length - 8) {  // Leave room for comment count
        reportError("Format", "Invalid vendor string length: " + std::to_string(vendor_length));
        return false;
    }
    
    // Read vendor string (UTF-8)
    std::string vendor_string;
    if (vendor_length > 0) {
        std::vector<uint8_t> vendor_data(vendor_length);
        if (m_handler->read(vendor_data.data(), 1, vendor_length) != vendor_length) {
            reportError("IO", "Failed to read vendor string");
            return false;
        }
        bytes_read += vendor_length;
        
        // Convert to string (assuming UTF-8)
        vendor_string.assign(vendor_data.begin(), vendor_data.end());
        Debug::log("flac", "Vendor string: ", vendor_string);
    }
    
    // Read user comment count (32-bit little-endian)
    uint8_t comment_count_data[4];
    if (m_handler->read(comment_count_data, 1, 4) != 4) {
        reportError("IO", "Failed to read comment count");
        return false;
    }
    bytes_read += 4;
    
    uint32_t comment_count = static_cast<uint32_t>(comment_count_data[0]) |
                            (static_cast<uint32_t>(comment_count_data[1]) << 8) |
                            (static_cast<uint32_t>(comment_count_data[2]) << 16) |
                            (static_cast<uint32_t>(comment_count_data[3]) << 24);
    
    Debug::log("flac", "Processing ", comment_count, " user comments");
    
    // Read each user comment
    for (uint32_t i = 0; i < comment_count; i++) {
        // Check if we have enough bytes left
        if (bytes_read + 4 > block.length) {
            Debug::log("flac", "Not enough data for comment ", i, " length field");
            break;
        }
        
        // Read comment length (32-bit little-endian)
        uint8_t comment_len_data[4];
        if (m_handler->read(comment_len_data, 1, 4) != 4) {
            Debug::log("flac", "Failed to read comment ", i, " length");
            break;
        }
        bytes_read += 4;
        
        uint32_t comment_length = static_cast<uint32_t>(comment_len_data[0]) |
                                 (static_cast<uint32_t>(comment_len_data[1]) << 8) |
                                 (static_cast<uint32_t>(comment_len_data[2]) << 16) |
                                 (static_cast<uint32_t>(comment_len_data[3]) << 24);
        
        // Validate comment length
        if (comment_length == 0) {
            Debug::log("flac", "Empty comment ", i, ", skipping");
            continue;
        }
        
        if (bytes_read + comment_length > block.length) {
            Debug::log("flac", "Comment ", i, " length (", comment_length, 
                      ") exceeds remaining block data");
            break;
        }
        
        // Reasonable size limit for comments (64KB)
        if (comment_length > 65536) {
            Debug::log("flac", "Comment ", i, " too large (", comment_length, " bytes), skipping");
            // Skip this comment
            if (!m_handler->seek(m_handler->tell() + comment_length, SEEK_SET)) {
                Debug::log("flac", "Failed to skip oversized comment");
                break;
            }
            bytes_read += comment_length;
            continue;
        }
        
        // Read comment data (UTF-8)
        std::vector<uint8_t> comment_data(comment_length);
        if (m_handler->read(comment_data.data(), 1, comment_length) != comment_length) {
            Debug::log("flac", "Failed to read comment ", i, " data");
            break;
        }
        bytes_read += comment_length;
        
        // Convert to string (assuming UTF-8)
        std::string comment_string(comment_data.begin(), comment_data.end());
        
        // Parse FIELD=value format
        size_t equals_pos = comment_string.find('=');
        if (equals_pos == std::string::npos) {
            Debug::log("flac", "Comment ", i, " missing '=' separator: ", comment_string);
            continue;
        }
        
        std::string field = comment_string.substr(0, equals_pos);
        std::string value = comment_string.substr(equals_pos + 1);
        
        // Convert field name to uppercase for consistency
        std::transform(field.begin(), field.end(), field.begin(), ::toupper);
        
        // Store the comment
        m_vorbis_comments[field] = value;
        
        Debug::log("flac", "Added comment: ", field, " = ", value);
    }
    
    // Skip any remaining bytes in the block
    if (bytes_read < block.length) {
        uint32_t remaining = block.length - bytes_read;
        Debug::log("flac", "Skipping ", remaining, " remaining bytes in VORBIS_COMMENT block");
        if (!m_handler->seek(m_handler->tell() + remaining, SEEK_SET)) {
            Debug::log("flac", "Failed to skip remaining VORBIS_COMMENT data");
            return false;
        }
    }
    
    Debug::log("flac", "VORBIS_COMMENT parsed successfully, ", m_vorbis_comments.size(), " comments");
    
    // Log standard metadata fields if present
    const std::vector<std::string> standard_fields = {
        "TITLE", "ARTIST", "ALBUM", "DATE", "TRACKNUMBER", "GENRE", "ALBUMARTIST"
    };
    
    for (const auto& field : standard_fields) {
        auto it = m_vorbis_comments.find(field);
        if (it != m_vorbis_comments.end()) {
            Debug::log("flac", "  ", field, ": ", it->second);
        }
    }
    
    return true;
}

bool FLACDemuxer::parsePictureBlock(const FLACMetadataBlock& block)
{
    Debug::log("flac", "FLACDemuxer::parsePictureBlock() - parsing PICTURE block");
    
    if (!m_handler) {
        return false;
    }
    
    // Minimum PICTURE block size: 4+4+4+4+4+4+4+4 = 32 bytes (without strings and data)
    if (block.length < 32) {
        reportError("Format", "PICTURE block too small: " + std::to_string(block.length));
        return false;
    }
    
    FLACPicture picture;
    uint32_t bytes_read = 0;
    
    // Read picture type (32-bit big-endian)
    uint8_t type_data[4];
    if (m_handler->read(type_data, 1, 4) != 4) {
        reportError("IO", "Failed to read picture type");
        return false;
    }
    bytes_read += 4;
    
    picture.picture_type = (static_cast<uint32_t>(type_data[0]) << 24) |
                          (static_cast<uint32_t>(type_data[1]) << 16) |
                          (static_cast<uint32_t>(type_data[2]) << 8) |
                          static_cast<uint32_t>(type_data[3]);
    
    // Read MIME type length (32-bit big-endian)
    uint8_t mime_len_data[4];
    if (m_handler->read(mime_len_data, 1, 4) != 4) {
        reportError("IO", "Failed to read MIME type length");
        return false;
    }
    bytes_read += 4;
    
    uint32_t mime_length = (static_cast<uint32_t>(mime_len_data[0]) << 24) |
                          (static_cast<uint32_t>(mime_len_data[1]) << 16) |
                          (static_cast<uint32_t>(mime_len_data[2]) << 8) |
                          static_cast<uint32_t>(mime_len_data[3]);
    
    // Validate MIME type length
    if (mime_length > 256) {  // Reasonable limit for MIME type
        reportError("Format", "MIME type too long: " + std::to_string(mime_length));
        return false;
    }
    
    if (bytes_read + mime_length > block.length) {
        reportError("Format", "MIME type length exceeds block size");
        return false;
    }
    
    // Read MIME type string
    if (mime_length > 0) {
        std::vector<uint8_t> mime_data(mime_length);
        if (m_handler->read(mime_data.data(), 1, mime_length) != mime_length) {
            reportError("IO", "Failed to read MIME type");
            return false;
        }
        bytes_read += mime_length;
        
        picture.mime_type.assign(mime_data.begin(), mime_data.end());
    }
    
    // Read description length (32-bit big-endian)
    uint8_t desc_len_data[4];
    if (m_handler->read(desc_len_data, 1, 4) != 4) {
        reportError("IO", "Failed to read description length");
        return false;
    }
    bytes_read += 4;
    
    uint32_t desc_length = (static_cast<uint32_t>(desc_len_data[0]) << 24) |
                          (static_cast<uint32_t>(desc_len_data[1]) << 16) |
                          (static_cast<uint32_t>(desc_len_data[2]) << 8) |
                          static_cast<uint32_t>(desc_len_data[3]);
    
    // Validate description length
    if (desc_length > 65536) {  // Reasonable limit for description
        Debug::log("flac", "Description too long (", desc_length, " bytes), truncating");
        desc_length = 65536;
    }
    
    if (bytes_read + desc_length > block.length) {
        reportError("Format", "Description length exceeds block size");
        return false;
    }
    
    // Read description string (UTF-8)
    if (desc_length > 0) {
        std::vector<uint8_t> desc_data(desc_length);
        if (m_handler->read(desc_data.data(), 1, desc_length) != desc_length) {
            reportError("IO", "Failed to read description");
            return false;
        }
        bytes_read += desc_length;
        
        picture.description.assign(desc_data.begin(), desc_data.end());
    }
    
    // Read picture width (32-bit big-endian)
    uint8_t width_data[4];
    if (m_handler->read(width_data, 1, 4) != 4) {
        reportError("IO", "Failed to read picture width");
        return false;
    }
    bytes_read += 4;
    
    picture.width = (static_cast<uint32_t>(width_data[0]) << 24) |
                   (static_cast<uint32_t>(width_data[1]) << 16) |
                   (static_cast<uint32_t>(width_data[2]) << 8) |
                   static_cast<uint32_t>(width_data[3]);
    
    // Read picture height (32-bit big-endian)
    uint8_t height_data[4];
    if (m_handler->read(height_data, 1, 4) != 4) {
        reportError("IO", "Failed to read picture height");
        return false;
    }
    bytes_read += 4;
    
    picture.height = (static_cast<uint32_t>(height_data[0]) << 24) |
                    (static_cast<uint32_t>(height_data[1]) << 16) |
                    (static_cast<uint32_t>(height_data[2]) << 8) |
                    static_cast<uint32_t>(height_data[3]);
    
    // Read color depth (32-bit big-endian)
    uint8_t depth_data[4];
    if (m_handler->read(depth_data, 1, 4) != 4) {
        reportError("IO", "Failed to read color depth");
        return false;
    }
    bytes_read += 4;
    
    picture.color_depth = (static_cast<uint32_t>(depth_data[0]) << 24) |
                         (static_cast<uint32_t>(depth_data[1]) << 16) |
                         (static_cast<uint32_t>(depth_data[2]) << 8) |
                         static_cast<uint32_t>(depth_data[3]);
    
    // Read colors used (32-bit big-endian)
    uint8_t colors_data[4];
    if (m_handler->read(colors_data, 1, 4) != 4) {
        reportError("IO", "Failed to read colors used");
        return false;
    }
    bytes_read += 4;
    
    picture.colors_used = (static_cast<uint32_t>(colors_data[0]) << 24) |
                         (static_cast<uint32_t>(colors_data[1]) << 16) |
                         (static_cast<uint32_t>(colors_data[2]) << 8) |
                         static_cast<uint32_t>(colors_data[3]);
    
    // Read picture data length (32-bit big-endian)
    uint8_t data_len_data[4];
    if (m_handler->read(data_len_data, 1, 4) != 4) {
        reportError("IO", "Failed to read picture data length");
        return false;
    }
    bytes_read += 4;
    
    uint32_t data_length = (static_cast<uint32_t>(data_len_data[0]) << 24) |
                          (static_cast<uint32_t>(data_len_data[1]) << 16) |
                          (static_cast<uint32_t>(data_len_data[2]) << 8) |
                          static_cast<uint32_t>(data_len_data[3]);
    
    // Validate picture data length
    if (bytes_read + data_length != block.length) {
        reportError("Format", "Picture data length doesn't match remaining block size");
        return false;
    }
    
    // Reasonable size limit for picture data (16MB)
    if (data_length > 16 * 1024 * 1024) {
        Debug::log("flac", "Picture data very large (", data_length, " bytes), storing metadata only");
        // Skip the actual image data but store the metadata
        if (!m_handler->seek(m_handler->tell() + data_length, SEEK_SET)) {
            reportError("IO", "Failed to skip large picture data");
            return false;
        }
        // Don't store the actual image data
        picture.data.clear();
    } else {
        // Read picture data
        if (data_length > 0) {
            picture.data.resize(data_length);
            if (m_handler->read(picture.data.data(), 1, data_length) != data_length) {
                reportError("IO", "Failed to read picture data");
                return false;
            }
        }
    }
    
    // Validate picture metadata
    if (!picture.isValid()) {
        Debug::log("flac", "Invalid picture metadata, skipping");
        return true;  // Not a fatal error, just skip this picture
    }
    
    // Add picture to collection
    m_pictures.push_back(std::move(picture));
    
    Debug::log("flac", "PICTURE parsed successfully:");
    Debug::log("flac", "  Type: ", picture.picture_type);
    Debug::log("flac", "  MIME type: ", picture.mime_type);
    Debug::log("flac", "  Description: ", picture.description);
    Debug::log("flac", "  Dimensions: ", picture.width, "x", picture.height);
    Debug::log("flac", "  Color depth: ", picture.color_depth, " bits");
    Debug::log("flac", "  Data size: ", data_length, " bytes");
    
    // Log picture type description
    const char* type_desc = "Unknown";
    switch (picture.picture_type) {
        case 0: type_desc = "Other"; break;
        case 1: type_desc = "32x32 pixels file icon"; break;
        case 2: type_desc = "Other file icon"; break;
        case 3: type_desc = "Cover (front)"; break;
        case 4: type_desc = "Cover (back)"; break;
        case 5: type_desc = "Leaflet page"; break;
        case 6: type_desc = "Media"; break;
        case 7: type_desc = "Lead artist/lead performer/soloist"; break;
        case 8: type_desc = "Artist/performer"; break;
        case 9: type_desc = "Conductor"; break;
        case 10: type_desc = "Band/Orchestra"; break;
        case 11: type_desc = "Composer"; break;
        case 12: type_desc = "Lyricist/text writer"; break;
        case 13: type_desc = "Recording Location"; break;
        case 14: type_desc = "During recording"; break;
        case 15: type_desc = "During performance"; break;
        case 16: type_desc = "Movie/video screen capture"; break;
        case 17: type_desc = "A bright coloured fish"; break;
        case 18: type_desc = "Illustration"; break;
        case 19: type_desc = "Band/artist logotype"; break;
        case 20: type_desc = "Publisher/Studio logotype"; break;
    }
    Debug::log("flac", "  Type description: ", type_desc);
    
    return true;
}

bool FLACDemuxer::skipMetadataBlock(const FLACMetadataBlock& block)
{
    Debug::log("flac", "FLACDemuxer::skipMetadataBlock() - skipping block of length: ", block.length);
    
    if (!m_handler) {
        return false;
    }
    
    // Skip the block data by seeking forward
    int64_t current_pos = m_handler->tell();
    int64_t target_pos = current_pos + static_cast<int64_t>(block.length);
    
    if (!m_handler->seek(target_pos, SEEK_SET)) {
        Debug::log("flac", "Failed to seek past metadata block");
        return false;
    }
    
    return true;
}

bool FLACDemuxer::findNextFrame(FLACFrame& frame)
{
    Debug::log("flac", "FLACDemuxer::findNextFrame() - placeholder implementation");
    // TODO: Implement frame sync detection
    return false;
}

bool FLACDemuxer::parseFrameHeader(FLACFrame& frame)
{
    Debug::log("flac", "FLACDemuxer::parseFrameHeader() - placeholder implementation");
    // TODO: Implement frame header parsing
    return false;
}

bool FLACDemuxer::validateFrameHeader(const FLACFrame& frame)
{
    Debug::log("flac", "FLACDemuxer::validateFrameHeader() - placeholder implementation");
    // TODO: Implement frame header validation
    return false;
}

uint32_t FLACDemuxer::calculateFrameSize(const FLACFrame& frame)
{
    Debug::log("flac", "FLACDemuxer::calculateFrameSize() - placeholder implementation");
    // TODO: Implement frame size calculation
    return 0;
}

bool FLACDemuxer::seekWithTable(uint64_t target_sample)
{
    Debug::log("flac", "FLACDemuxer::seekWithTable() - placeholder implementation");
    // TODO: Implement seek table based seeking
    return false;
}

bool FLACDemuxer::seekBinary(uint64_t target_sample)
{
    Debug::log("flac", "FLACDemuxer::seekBinary() - placeholder implementation");
    // TODO: Implement binary search seeking
    return false;
}

bool FLACDemuxer::seekLinear(uint64_t target_sample)
{
    Debug::log("flac", "FLACDemuxer::seekLinear() - placeholder implementation");
    // TODO: Implement linear seeking
    return false;
}

uint64_t FLACDemuxer::samplesToMs(uint64_t samples) const
{
    if (m_streaminfo.sample_rate == 0) {
        return 0;
    }
    return (samples * 1000) / m_streaminfo.sample_rate;
}

uint64_t FLACDemuxer::msToSamples(uint64_t ms) const
{
    if (m_streaminfo.sample_rate == 0) {
        return 0;
    }
    return (ms * m_streaminfo.sample_rate) / 1000;
}