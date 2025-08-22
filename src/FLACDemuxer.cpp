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
    Debug::log("flac", "FLACDemuxer::parseSeekTableBlock() - placeholder implementation");
    // TODO: Implement SEEKTABLE parsing
    return false;
}

bool FLACDemuxer::parseVorbisCommentBlock(const FLACMetadataBlock& block)
{
    Debug::log("flac", "FLACDemuxer::parseVorbisCommentBlock() - placeholder implementation");
    // TODO: Implement VORBIS_COMMENT parsing
    return false;
}

bool FLACDemuxer::parsePictureBlock(const FLACMetadataBlock& block)
{
    Debug::log("flac", "FLACDemuxer::parsePictureBlock() - placeholder implementation");
    // TODO: Implement PICTURE parsing
    return false;
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