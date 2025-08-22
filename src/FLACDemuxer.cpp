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
    
    // Initialize position tracking to start of stream
    resetPositionTracking();
    
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
    Debug::log("flac", "FLACDemuxer::readChunk() - reading next FLAC frame");
    
    if (!m_container_parsed) {
        reportError("State", "Container not parsed");
        return MediaChunk{};
    }
    
    if (isEOF()) {
        Debug::log("flac", "At end of file");
        return MediaChunk{};
    }
    
    // Find the next FLAC frame
    FLACFrame frame;
    if (!findNextFrame(frame)) {
        Debug::log("flac", "No more FLAC frames found");
        return MediaChunk{};
    }
    
    // Read the complete frame data
    std::vector<uint8_t> frame_data;
    if (!readFrameData(frame, frame_data)) {
        reportError("IO", "Failed to read FLAC frame data");
        return MediaChunk{};
    }
    
    // Create MediaChunk with proper timing information
    MediaChunk chunk(1, std::move(frame_data));  // stream_id = 1 for FLAC
    chunk.timestamp_samples = frame.sample_offset;
    chunk.is_keyframe = true;  // All FLAC frames are independent
    chunk.file_offset = frame.file_offset;
    
    // Update position tracking - advance to next frame position
    uint64_t next_sample_position = frame.sample_offset + frame.block_size;
    uint64_t next_file_offset = frame.file_offset + frame.frame_size;
    
    // Use updatePositionTracking for consistency and validation
    updatePositionTracking(next_sample_position, next_file_offset);
    m_last_block_size = frame.block_size;
    
    Debug::log("flac", "Read FLAC frame: samples=", frame.sample_offset, 
              " block_size=", frame.block_size, " data_size=", chunk.data.size());
    
    return chunk;
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
    Debug::log("flac", "FLACDemuxer::seekTo(", timestamp_ms, ") - seeking to timestamp");
    
    if (!m_container_parsed) {
        reportError("State", "Container not parsed");
        return false;
    }
    
    if (!m_streaminfo.isValid()) {
        reportError("State", "Invalid STREAMINFO for seeking");
        return false;
    }
    
    // Convert timestamp to sample position
    uint64_t target_sample = msToSamples(timestamp_ms);
    
    // Handle seek to beginning
    if (timestamp_ms == 0 || target_sample == 0) {
        Debug::log("flac", "Seeking to beginning of stream");
        resetPositionTracking();
        
        // Seek to start of audio data
        if (m_handler && !m_handler->seek(m_audio_data_offset, SEEK_SET)) {
            reportError("IO", "Failed to seek to start of audio data");
            return false;
        }
        
        return true;
    }
    
    // Validate target sample is within stream bounds
    if (m_streaminfo.total_samples > 0 && target_sample >= m_streaminfo.total_samples) {
        Debug::log("flac", "Seek target (", target_sample, ") beyond stream end (", 
                  m_streaminfo.total_samples, "), clamping");
        target_sample = m_streaminfo.total_samples - 1;
        timestamp_ms = samplesToMs(target_sample);
    }
    
    Debug::log("flac", "Seeking to sample ", target_sample, " (", timestamp_ms, " ms)");
    
    // Try different seeking strategies in order of preference
    bool seek_success = false;
    
    // Strategy 1: Use seek table if available
    if (!m_seektable.empty()) {
        Debug::log("flac", "Attempting seek table based seeking");
        seek_success = seekWithTable(target_sample);
    }
    
    // Strategy 2: Binary search through frames (not implemented yet)
    if (!seek_success) {
        Debug::log("flac", "Attempting binary search seeking");
        seek_success = seekBinary(target_sample);
    }
    
    // Strategy 3: Linear search from current or beginning (not implemented yet)
    if (!seek_success) {
        Debug::log("flac", "Attempting linear seeking");
        seek_success = seekLinear(target_sample);
    }
    
    if (seek_success) {
        Debug::log("flac", "Seek successful to sample ", m_current_sample, 
                  " (", samplesToMs(m_current_sample), " ms)");
        return true;
    } else {
        reportError("Seek", "All seeking strategies failed for timestamp " + std::to_string(timestamp_ms));
        return false;
    }
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
    Debug::log("flac", "FLACDemuxer::getDuration() - calculating duration");
    
    if (!m_container_parsed) {
        Debug::log("flac", "Container not parsed, cannot determine duration");
        return 0;
    }
    
    // Primary method: Use total samples from STREAMINFO
    if (m_streaminfo.isValid() && m_streaminfo.total_samples > 0 && m_streaminfo.sample_rate > 0) {
        // Use 64-bit arithmetic to prevent overflow for very long files
        // Calculate: (total_samples * 1000) / sample_rate
        // But do it safely to avoid overflow
        uint64_t duration_ms = (m_streaminfo.total_samples * 1000ULL) / m_streaminfo.sample_rate;
        
        Debug::log("flac", "Duration from STREAMINFO: ", duration_ms, " ms (", 
                  m_streaminfo.total_samples, " samples at ", m_streaminfo.sample_rate, " Hz)");
        return duration_ms;
    }
    
    // Fallback method: Estimate from file size and average bitrate
    if (m_file_size > 0 && m_streaminfo.isValid() && m_streaminfo.sample_rate > 0) {
        Debug::log("flac", "STREAMINFO incomplete, estimating duration from file size");
        
        // Calculate approximate bitrate based on format parameters
        // Uncompressed bitrate = sample_rate * channels * bits_per_sample
        uint64_t uncompressed_bitrate = static_cast<uint64_t>(m_streaminfo.sample_rate) * 
                                       m_streaminfo.channels * 
                                       m_streaminfo.bits_per_sample;
        
        if (uncompressed_bitrate == 0) {
            Debug::log("flac", "Cannot calculate bitrate, insufficient STREAMINFO");
            return 0;
        }
        
        // Estimate FLAC compression ratio (typically 0.5-0.7, use 0.6 as average)
        // This is a rough estimate since FLAC compression varies by content
        double compression_ratio = 0.6;
        uint64_t estimated_compressed_bitrate = static_cast<uint64_t>(uncompressed_bitrate * compression_ratio);
        
        // Account for metadata overhead (subtract audio data offset from file size)
        uint64_t audio_data_size = m_file_size;
        if (m_audio_data_offset > 0 && m_audio_data_offset < m_file_size) {
            audio_data_size = m_file_size - m_audio_data_offset;
        }
        
        // Calculate duration: (audio_data_size_in_bits * 1000) / bitrate
        // Convert bytes to bits: audio_data_size * 8
        // Use 64-bit arithmetic to prevent overflow
        if (estimated_compressed_bitrate > 0) {
            uint64_t duration_ms = (audio_data_size * 8ULL * 1000ULL) / estimated_compressed_bitrate;
            
            Debug::log("flac", "Estimated duration from file size: ", duration_ms, " ms");
            Debug::log("flac", "  File size: ", m_file_size, " bytes");
            Debug::log("flac", "  Audio data size: ", audio_data_size, " bytes");
            Debug::log("flac", "  Estimated bitrate: ", estimated_compressed_bitrate, " bps");
            
            return duration_ms;
        }
    }
    
    // No reliable way to determine duration
    Debug::log("flac", "Cannot determine duration - insufficient information");
    return 0;
}

uint64_t FLACDemuxer::getPosition() const
{
    Debug::log("flac", "FLACDemuxer::getPosition() - returning current position in milliseconds");
    
    if (!m_container_parsed || !m_streaminfo.isValid()) {
        Debug::log("flac", "Container not parsed or invalid STREAMINFO");
        return 0;
    }
    
    // Convert current sample position to milliseconds
    uint64_t position_ms = samplesToMs(m_current_sample);
    Debug::log("flac", "Current position: ", m_current_sample, " samples = ", position_ms, " ms");
    
    return position_ms;
}

uint64_t FLACDemuxer::getCurrentSample() const
{
    Debug::log("flac", "FLACDemuxer::getCurrentSample() - returning current position in samples");
    
    if (!m_container_parsed) {
        Debug::log("flac", "Container not parsed");
        return 0;
    }
    
    Debug::log("flac", "Current sample position: ", m_current_sample);
    return m_current_sample;
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
    Debug::log("flac", "FLACDemuxer::findNextFrame() - searching for FLAC frame sync");
    
    if (!m_handler) {
        reportError("IO", "No IOHandler available for frame sync detection");
        return false;
    }
    
    // Start searching from current position
    uint64_t search_start = m_current_offset;
    
    // Buffer for reading data during sync search
    const size_t buffer_size = 8192;  // 8KB buffer for efficient I/O
    std::vector<uint8_t> buffer(buffer_size);
    
    // Seek to current position
    if (!m_handler->seek(search_start, SEEK_SET)) {
        reportError("IO", "Failed to seek to search position");
        return false;
    }
    
    uint64_t bytes_searched = 0;
    const uint64_t max_search_distance = 1024 * 1024;  // Search up to 1MB for sync
    
    while (bytes_searched < max_search_distance && !m_handler->eof()) {
        // Read buffer of data
        size_t bytes_read = m_handler->read(buffer.data(), 1, buffer_size);
        if (bytes_read < 2) {
            // Need at least 2 bytes for sync code
            break;
        }
        
        // Search for FLAC sync code in buffer
        // FLAC sync code is 14 bits: 11111111111110xx (0xFFF8 to 0xFFFF)
        for (size_t i = 0; i < bytes_read - 1; i++) {
            uint16_t sync_candidate = (static_cast<uint16_t>(buffer[i]) << 8) | 
                                     static_cast<uint16_t>(buffer[i + 1]);
            
            // Check if this matches FLAC sync pattern (0xFFF8 to 0xFFFF)
            if ((sync_candidate & 0xFFFC) == 0xFFF8) {
                // Found potential sync code, validate frame header
                uint64_t sync_position = search_start + bytes_searched + i;
                
                Debug::log("flac", "Found potential sync code 0x", std::hex, sync_candidate, 
                          std::dec, " at position ", sync_position);
                
                // Seek to this position and try to parse frame header
                if (!m_handler->seek(sync_position, SEEK_SET)) {
                    Debug::log("flac", "Failed to seek to sync position");
                    continue;
                }
                
                // Store position in frame structure
                frame.file_offset = sync_position;
                
                // Try to parse frame header at this position
                if (parseFrameHeader(frame)) {
                    // Validate the parsed frame header
                    if (validateFrameHeader(frame)) {
                        Debug::log("flac", "Valid FLAC frame found at position ", sync_position);
                        m_current_offset = sync_position;
                        return true;
                    } else {
                        Debug::log("flac", "Frame header validation failed at position ", sync_position);
                    }
                } else {
                    Debug::log("flac", "Frame header parsing failed at position ", sync_position);
                }
                
                // Continue searching from next byte if this wasn't a valid frame
            }
        }
        
        bytes_searched += bytes_read;
        
        // If we read less than buffer_size, we're at EOF
        if (bytes_read < buffer_size) {
            break;
        }
        
        // Overlap search to avoid missing sync codes at buffer boundaries
        // Back up by 1 byte to ensure we don't miss a sync code split across buffers
        if (bytes_read > 1) {
            search_start = search_start + bytes_searched - 1;
            bytes_searched = 1;  // Account for the overlap
            
            if (!m_handler->seek(search_start, SEEK_SET)) {
                Debug::log("flac", "Failed to seek for overlapped search");
                break;
            }
        }
    }
    
    Debug::log("flac", "No valid FLAC frame found after searching ", bytes_searched, " bytes");
    return false;
}

bool FLACDemuxer::parseFrameHeader(FLACFrame& frame)
{
    Debug::log("flac", "FLACDemuxer::parseFrameHeader() - parsing FLAC frame header");
    
    if (!m_handler) {
        reportError("IO", "No IOHandler available for frame header parsing");
        return false;
    }
    
    // FLAC frame header is variable length, minimum 4 bytes
    // We need to read and parse it incrementally
    
    // Read first 4 bytes (sync + basic header info)
    uint8_t header_start[4];
    if (m_handler->read(header_start, 1, 4) != 4) {
        Debug::log("flac", "Failed to read frame header start");
        return false;
    }
    
    // Verify sync code (14 bits: 11111111111110xx)
    uint16_t sync_code = (static_cast<uint16_t>(header_start[0]) << 8) | 
                        static_cast<uint16_t>(header_start[1]);
    
    if ((sync_code & 0xFFFC) != 0xFFF8) {
        Debug::log("flac", "Invalid sync code in frame header: 0x", std::hex, sync_code, std::dec);
        return false;
    }
    
    // Parse frame header fields
    
    // Reserved bit (must be 0)
    if ((header_start[1] & 0x02) != 0) {
        Debug::log("flac", "Reserved bit set in frame header");
        return false;
    }
    
    // Blocking strategy (1 bit)
    frame.variable_block_size = (header_start[1] & 0x01) != 0;
    
    // Block size (4 bits)
    uint8_t block_size_code = (header_start[2] & 0xF0) >> 4;
    
    // Sample rate (4 bits)
    uint8_t sample_rate_code = header_start[2] & 0x0F;
    
    // Channel assignment (4 bits)
    uint8_t channel_assignment = (header_start[3] & 0xF0) >> 4;
    
    // Sample size (3 bits)
    uint8_t sample_size_code = (header_start[3] & 0x0E) >> 1;
    
    // Reserved bit (must be 0)
    if ((header_start[3] & 0x01) != 0) {
        Debug::log("flac", "Reserved bit set in frame header");
        return false;
    }
    
    // Parse block size
    uint32_t block_size = 0;
    switch (block_size_code) {
        case 0x00: // Reserved
            Debug::log("flac", "Reserved block size code");
            return false;
        case 0x01: block_size = 192; break;
        case 0x02: block_size = 576; break;
        case 0x03: block_size = 1152; break;
        case 0x04: block_size = 2304; break;
        case 0x05: block_size = 4608; break;
        case 0x06: // 8-bit block size follows
        case 0x07: // 16-bit block size follows
            // Will be read later
            break;
        case 0x08: block_size = 256; break;
        case 0x09: block_size = 512; break;
        case 0x0A: block_size = 1024; break;
        case 0x0B: block_size = 2048; break;
        case 0x0C: block_size = 4096; break;
        case 0x0D: block_size = 8192; break;
        case 0x0E: block_size = 16384; break;
        case 0x0F: block_size = 32768; break;
    }
    
    // Parse sample rate
    uint32_t sample_rate = 0;
    switch (sample_rate_code) {
        case 0x00: // Use STREAMINFO sample rate
            sample_rate = m_streaminfo.sample_rate;
            break;
        case 0x01: sample_rate = 88200; break;
        case 0x02: sample_rate = 176400; break;
        case 0x03: sample_rate = 192000; break;
        case 0x04: sample_rate = 8000; break;
        case 0x05: sample_rate = 16000; break;
        case 0x06: sample_rate = 22050; break;
        case 0x07: sample_rate = 24000; break;
        case 0x08: sample_rate = 32000; break;
        case 0x09: sample_rate = 44100; break;
        case 0x0A: sample_rate = 48000; break;
        case 0x0B: sample_rate = 96000; break;
        case 0x0C: // 8-bit sample rate follows (in kHz)
        case 0x0D: // 16-bit sample rate follows (in Hz)
        case 0x0E: // 16-bit sample rate follows (in 10Hz units)
            // Will be read later
            break;
        case 0x0F: // Invalid
            Debug::log("flac", "Invalid sample rate code");
            return false;
    }
    
    // Parse channel assignment
    uint8_t channels = 0;
    switch (channel_assignment) {
        case 0x0: case 0x1: case 0x2: case 0x3:
        case 0x4: case 0x5: case 0x6: case 0x7:
            channels = channel_assignment + 1;  // 1-8 channels
            break;
        case 0x8: // Left/side stereo
        case 0x9: // Right/side stereo  
        case 0xA: // Mid/side stereo
            channels = 2;
            break;
        default: // Reserved
            Debug::log("flac", "Reserved channel assignment: ", channel_assignment);
            return false;
    }
    
    // Parse sample size
    uint8_t bits_per_sample = 0;
    switch (sample_size_code) {
        case 0x0: // Use STREAMINFO bits per sample
            bits_per_sample = m_streaminfo.bits_per_sample;
            break;
        case 0x1: bits_per_sample = 8; break;
        case 0x2: bits_per_sample = 12; break;
        case 0x3: // Reserved
            Debug::log("flac", "Reserved sample size code");
            return false;
        case 0x4: bits_per_sample = 16; break;
        case 0x5: bits_per_sample = 20; break;
        case 0x6: bits_per_sample = 24; break;
        case 0x7: // Reserved
            Debug::log("flac", "Reserved sample size code");
            return false;
    }
    
    // Read frame/sample number (UTF-8 coded)
    uint64_t frame_sample_number = 0;
    uint8_t utf8_byte;
    if (m_handler->read(&utf8_byte, 1, 1) != 1) {
        Debug::log("flac", "Failed to read frame/sample number");
        return false;
    }
    
    // Decode UTF-8 coded number
    if ((utf8_byte & 0x80) == 0) {
        // 1 byte: 0xxxxxxx
        frame_sample_number = utf8_byte;
    } else if ((utf8_byte & 0xE0) == 0xC0) {
        // 2 bytes: 110xxxxx 10xxxxxx
        frame_sample_number = utf8_byte & 0x1F;
        uint8_t byte2;
        if (m_handler->read(&byte2, 1, 1) != 1 || (byte2 & 0xC0) != 0x80) {
            Debug::log("flac", "Invalid UTF-8 sequence in frame number");
            return false;
        }
        frame_sample_number = (frame_sample_number << 6) | (byte2 & 0x3F);
    } else if ((utf8_byte & 0xF0) == 0xE0) {
        // 3 bytes: 1110xxxx 10xxxxxx 10xxxxxx
        frame_sample_number = utf8_byte & 0x0F;
        for (int i = 0; i < 2; i++) {
            uint8_t byte;
            if (m_handler->read(&byte, 1, 1) != 1 || (byte & 0xC0) != 0x80) {
                Debug::log("flac", "Invalid UTF-8 sequence in frame number");
                return false;
            }
            frame_sample_number = (frame_sample_number << 6) | (byte & 0x3F);
        }
    } else if ((utf8_byte & 0xF8) == 0xF0) {
        // 4 bytes: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        frame_sample_number = utf8_byte & 0x07;
        for (int i = 0; i < 3; i++) {
            uint8_t byte;
            if (m_handler->read(&byte, 1, 1) != 1 || (byte & 0xC0) != 0x80) {
                Debug::log("flac", "Invalid UTF-8 sequence in frame number");
                return false;
            }
            frame_sample_number = (frame_sample_number << 6) | (byte & 0x3F);
        }
    } else if ((utf8_byte & 0xFC) == 0xF8) {
        // 5 bytes: 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
        frame_sample_number = utf8_byte & 0x03;
        for (int i = 0; i < 4; i++) {
            uint8_t byte;
            if (m_handler->read(&byte, 1, 1) != 1 || (byte & 0xC0) != 0x80) {
                Debug::log("flac", "Invalid UTF-8 sequence in frame number");
                return false;
            }
            frame_sample_number = (frame_sample_number << 6) | (byte & 0x3F);
        }
    } else if ((utf8_byte & 0xFE) == 0xFC) {
        // 6 bytes: 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
        frame_sample_number = utf8_byte & 0x01;
        for (int i = 0; i < 5; i++) {
            uint8_t byte;
            if (m_handler->read(&byte, 1, 1) != 1 || (byte & 0xC0) != 0x80) {
                Debug::log("flac", "Invalid UTF-8 sequence in frame number");
                return false;
            }
            frame_sample_number = (frame_sample_number << 6) | (byte & 0x3F);
        }
    } else {
        Debug::log("flac", "Invalid UTF-8 start byte in frame number: 0x", std::hex, utf8_byte, std::dec);
        return false;
    }
    
    // Read variable-length fields if needed
    
    // Block size (if encoded as variable length)
    if (block_size_code == 0x06) {
        // 8-bit block size follows
        uint8_t block_size_byte;
        if (m_handler->read(&block_size_byte, 1, 1) != 1) {
            Debug::log("flac", "Failed to read 8-bit block size");
            return false;
        }
        block_size = static_cast<uint32_t>(block_size_byte) + 1;
    } else if (block_size_code == 0x07) {
        // 16-bit block size follows
        uint8_t block_size_bytes[2];
        if (m_handler->read(block_size_bytes, 1, 2) != 2) {
            Debug::log("flac", "Failed to read 16-bit block size");
            return false;
        }
        block_size = (static_cast<uint32_t>(block_size_bytes[0]) << 8) | 
                     static_cast<uint32_t>(block_size_bytes[1]);
        block_size += 1;
    }
    
    // Sample rate (if encoded as variable length)
    if (sample_rate_code == 0x0C) {
        // 8-bit sample rate in kHz
        uint8_t sample_rate_byte;
        if (m_handler->read(&sample_rate_byte, 1, 1) != 1) {
            Debug::log("flac", "Failed to read 8-bit sample rate");
            return false;
        }
        sample_rate = static_cast<uint32_t>(sample_rate_byte) * 1000;
    } else if (sample_rate_code == 0x0D) {
        // 16-bit sample rate in Hz
        uint8_t sample_rate_bytes[2];
        if (m_handler->read(sample_rate_bytes, 1, 2) != 2) {
            Debug::log("flac", "Failed to read 16-bit sample rate");
            return false;
        }
        sample_rate = (static_cast<uint32_t>(sample_rate_bytes[0]) << 8) | 
                      static_cast<uint32_t>(sample_rate_bytes[1]);
    } else if (sample_rate_code == 0x0E) {
        // 16-bit sample rate in 10Hz units
        uint8_t sample_rate_bytes[2];
        if (m_handler->read(sample_rate_bytes, 1, 2) != 2) {
            Debug::log("flac", "Failed to read 16-bit sample rate (10Hz units)");
            return false;
        }
        sample_rate = ((static_cast<uint32_t>(sample_rate_bytes[0]) << 8) | 
                       static_cast<uint32_t>(sample_rate_bytes[1])) * 10;
    }
    
    // Read CRC-8 (header checksum)
    uint8_t crc8;
    if (m_handler->read(&crc8, 1, 1) != 1) {
        Debug::log("flac", "Failed to read frame header CRC-8");
        return false;
    }
    
    // Store parsed values in frame structure
    frame.block_size = block_size;
    frame.sample_rate = sample_rate;
    frame.channels = channels;
    frame.bits_per_sample = bits_per_sample;
    
    // Calculate sample offset based on blocking strategy
    if (frame.variable_block_size) {
        // Frame number represents sample number
        frame.sample_offset = frame_sample_number;
    } else {
        // Frame number represents frame number, calculate sample offset
        frame.sample_offset = frame_sample_number * block_size;
    }
    
    Debug::log("flac", "Frame header parsed successfully:");
    Debug::log("flac", "  Block size: ", frame.block_size, " samples");
    Debug::log("flac", "  Sample rate: ", frame.sample_rate, " Hz");
    Debug::log("flac", "  Channels: ", static_cast<int>(frame.channels));
    Debug::log("flac", "  Bits per sample: ", static_cast<int>(frame.bits_per_sample));
    Debug::log("flac", "  Sample offset: ", frame.sample_offset);
    Debug::log("flac", "  Variable block size: ", frame.variable_block_size);
    Debug::log("flac", "  CRC-8: 0x", std::hex, crc8, std::dec);
    
    return true;
}

bool FLACDemuxer::validateFrameHeader(const FLACFrame& frame)
{
    Debug::log("flac", "FLACDemuxer::validateFrameHeader() - validating frame header");
    
    // Check if frame contains valid data
    if (!frame.isValid()) {
        Debug::log("flac", "Frame header contains invalid data");
        return false;
    }
    
    // Validate against STREAMINFO if available
    if (m_streaminfo.isValid()) {
        // Check sample rate consistency
        if (frame.sample_rate != m_streaminfo.sample_rate) {
            Debug::log("flac", "Frame sample rate (", frame.sample_rate, 
                      ") doesn't match STREAMINFO (", m_streaminfo.sample_rate, ")");
            // This is allowed in FLAC, but log it
        }
        
        // Check channels consistency
        if (frame.channels != m_streaminfo.channels) {
            Debug::log("flac", "Frame channels (", static_cast<int>(frame.channels), 
                      ") doesn't match STREAMINFO (", static_cast<int>(m_streaminfo.channels), ")");
            return false;
        }
        
        // Check bits per sample consistency
        if (frame.bits_per_sample != m_streaminfo.bits_per_sample) {
            Debug::log("flac", "Frame bits per sample (", static_cast<int>(frame.bits_per_sample), 
                      ") doesn't match STREAMINFO (", static_cast<int>(m_streaminfo.bits_per_sample), ")");
            return false;
        }
        
        // Check block size is within valid range
        if (frame.block_size < m_streaminfo.min_block_size || 
            frame.block_size > m_streaminfo.max_block_size) {
            Debug::log("flac", "Frame block size (", frame.block_size, 
                      ") outside STREAMINFO range (", m_streaminfo.min_block_size, 
                      "-", m_streaminfo.max_block_size, ")");
            return false;
        }
        
        // Check sample offset is reasonable
        if (m_streaminfo.total_samples > 0 && 
            frame.sample_offset >= m_streaminfo.total_samples) {
            Debug::log("flac", "Frame sample offset (", frame.sample_offset, 
                      ") exceeds total samples (", m_streaminfo.total_samples, ")");
            return false;
        }
        
        // Check that frame doesn't extend beyond total samples
        if (m_streaminfo.total_samples > 0 && 
            frame.sample_offset + frame.block_size > m_streaminfo.total_samples) {
            // This might be the last frame, which can be shorter
            Debug::log("flac", "Frame extends beyond total samples (might be last frame)");
        }
    }
    
    // Additional sanity checks
    
    // Check reasonable block size limits (FLAC spec allows 1-65535)
    if (frame.block_size == 0 || frame.block_size > 65535) {
        Debug::log("flac", "Frame block size out of valid range: ", frame.block_size);
        return false;
    }
    
    // Check reasonable sample rate limits
    if (frame.sample_rate == 0 || frame.sample_rate > 655350) {
        Debug::log("flac", "Frame sample rate out of valid range: ", frame.sample_rate);
        return false;
    }
    
    // Check channel count (FLAC supports 1-8 channels)
    if (frame.channels == 0 || frame.channels > 8) {
        Debug::log("flac", "Frame channel count out of valid range: ", static_cast<int>(frame.channels));
        return false;
    }
    
    // Check bits per sample (FLAC supports 4-32 bits)
    if (frame.bits_per_sample < 4 || frame.bits_per_sample > 32) {
        Debug::log("flac", "Frame bits per sample out of valid range: ", static_cast<int>(frame.bits_per_sample));
        return false;
    }
    
    Debug::log("flac", "Frame header validation passed");
    return true;
}

uint32_t FLACDemuxer::calculateFrameSize(const FLACFrame& frame)
{
    Debug::log("flac", "FLACDemuxer::calculateFrameSize() - estimating frame size");
    
    if (!frame.isValid()) {
        Debug::log("flac", "Cannot calculate size for invalid frame");
        return 0;
    }
    
    // FLAC frame size estimation is complex due to variable compression
    // We'll use multiple approaches for estimation
    
    uint32_t estimated_size = 0;
    
    // Method 1: Use STREAMINFO frame size hints if available
    if (m_streaminfo.isValid() && 
        m_streaminfo.min_frame_size > 0 && 
        m_streaminfo.max_frame_size > 0) {
        
        // Use average of min/max frame sizes as baseline
        uint32_t avg_frame_size = (m_streaminfo.min_frame_size + m_streaminfo.max_frame_size) / 2;
        
        // Adjust based on block size ratio
        if (m_streaminfo.max_block_size > 0) {
            double block_ratio = static_cast<double>(frame.block_size) / m_streaminfo.max_block_size;
            estimated_size = static_cast<uint32_t>(avg_frame_size * block_ratio);
        } else {
            estimated_size = avg_frame_size;
        }
        
        Debug::log("flac", "Frame size estimate from STREAMINFO: ", estimated_size, " bytes");
    }
    
    // Method 2: Calculate theoretical minimum size
    // Frame header (variable, typically 4-16 bytes) + subframes + frame footer (2 bytes CRC-16)
    
    uint32_t header_size = 4;  // Minimum header size
    header_size += 2;  // UTF-8 frame/sample number (conservative estimate)
    header_size += 1;  // CRC-8
    
    // Estimate subframe sizes
    uint32_t subframe_size = 0;
    
    // Each subframe has a header (1-2 bytes) plus compressed audio data
    uint32_t subframe_header_size = 2;  // Conservative estimate
    
    // Estimate compressed audio data size
    // This is highly variable, but we can estimate based on bits per sample and compression ratio
    
    // Uncompressed size per subframe
    uint32_t uncompressed_bits = frame.block_size * frame.bits_per_sample;
    uint32_t uncompressed_bytes = (uncompressed_bits + 7) / 8;  // Round up to bytes
    
    // Assume compression ratio between 30-70% (FLAC typical range)
    // Use 50% as middle estimate
    uint32_t compressed_bytes = uncompressed_bytes / 2;
    
    // Add subframe header
    subframe_size = subframe_header_size + compressed_bytes;
    
    // Total for all channels
    uint32_t total_subframes_size = subframe_size * frame.channels;
    
    uint32_t frame_footer_size = 2;  // CRC-16
    
    uint32_t theoretical_size = header_size + total_subframes_size + frame_footer_size;
    
    Debug::log("flac", "Theoretical frame size estimate: ", theoretical_size, " bytes");
    Debug::log("flac", "  Header: ", header_size, " bytes");
    Debug::log("flac", "  Subframes: ", total_subframes_size, " bytes (", frame.channels, " channels)");
    Debug::log("flac", "  Footer: ", frame_footer_size, " bytes");
    
    // Method 3: Use previous frame size as hint (if available)
    uint32_t previous_frame_estimate = 0;
    if (m_last_block_size > 0) {
        // Scale previous frame size based on block size ratio
        double block_ratio = static_cast<double>(frame.block_size) / m_last_block_size;
        // Use a conservative scaling factor
        previous_frame_estimate = static_cast<uint32_t>(theoretical_size * block_ratio);
        Debug::log("flac", "Previous frame size estimate: ", previous_frame_estimate, " bytes");
    }
    
    // Choose the best estimate
    if (estimated_size > 0) {
        // Use STREAMINFO-based estimate if available
        estimated_size = std::max(estimated_size, theoretical_size);
    } else if (previous_frame_estimate > 0) {
        // Use previous frame estimate
        estimated_size = std::max(previous_frame_estimate, theoretical_size);
    } else {
        // Fall back to theoretical estimate
        estimated_size = theoretical_size;
    }
    
    // Apply reasonable bounds checking
    
    // Minimum frame size (header + minimal subframes + footer)
    uint32_t min_frame_size = header_size + frame.channels * 4 + frame_footer_size;
    
    // Maximum frame size (uncompressed data + overhead)
    // In worst case, FLAC might not compress at all and add some overhead
    uint32_t max_uncompressed = frame.block_size * frame.channels * ((frame.bits_per_sample + 7) / 8);
    uint32_t max_frame_size = header_size + max_uncompressed + frame.channels * 16 + frame_footer_size;
    
    // Clamp estimate to reasonable bounds
    estimated_size = std::max(estimated_size, min_frame_size);
    estimated_size = std::min(estimated_size, max_frame_size);
    
    // Additional safety margin for seeking (add 10% buffer)
    estimated_size = static_cast<uint32_t>(estimated_size * 1.1);
    
    Debug::log("flac", "Final frame size estimate: ", estimated_size, " bytes");
    Debug::log("flac", "  Bounds: ", min_frame_size, " - ", max_frame_size, " bytes");
    
    return estimated_size;
}

bool FLACDemuxer::readFrameData(const FLACFrame& frame, std::vector<uint8_t>& data)
{
    Debug::log("flac", "FLACDemuxer::readFrameData() - reading complete FLAC frame");
    
    if (!m_handler) {
        reportError("IO", "No IOHandler available for frame reading");
        return false;
    }
    
    if (!frame.isValid()) {
        reportError("Frame", "Invalid frame header for reading");
        return false;
    }
    
    // Seek to the frame position
    if (!m_handler->seek(frame.file_offset, SEEK_SET)) {
        reportError("IO", "Failed to seek to frame position: " + std::to_string(frame.file_offset));
        return false;
    }
    
    // Calculate or use known frame size
    uint32_t frame_size = frame.frame_size;
    if (frame_size == 0) {
        // Estimate frame size if not known
        frame_size = calculateFrameSize(frame);
        Debug::log("flac", "Using estimated frame size: ", frame_size, " bytes");
    }
    
    // Validate frame size is reasonable
    if (frame_size == 0 || frame_size > 16 * 1024 * 1024) {  // 16MB max
        reportError("Frame", "Invalid frame size: " + std::to_string(frame_size));
        return false;
    }
    
    // Check if we have enough data left in file
    if (m_file_size > 0) {
        uint64_t bytes_available = m_file_size - frame.file_offset;
        if (frame_size > bytes_available) {
            Debug::log("flac", "Frame size (", frame_size, ") exceeds available data (", 
                      bytes_available, "), adjusting");
            frame_size = static_cast<uint32_t>(bytes_available);
        }
    }
    
    // Allocate buffer for frame data
    data.clear();
    data.resize(frame_size);
    
    // Read the frame data
    size_t bytes_read = m_handler->read(data.data(), 1, frame_size);
    if (bytes_read == 0) {
        reportError("IO", "Failed to read any frame data");
        return false;
    }
    
    // Handle partial reads (common at end of file)
    if (bytes_read < frame_size) {
        Debug::log("flac", "Partial frame read: ", bytes_read, " of ", frame_size, " bytes");
        data.resize(bytes_read);
        
        // Check if this looks like a complete frame by looking for sync pattern
        if (bytes_read < 4) {
            reportError("Frame", "Frame data too small to be valid");
            return false;
        }
        
        // Verify we still have the sync pattern at the beginning
        if ((data[0] & 0xFF) != 0xFF || (data[1] & 0xF8) != 0xF8) {
            reportError("Frame", "Frame data missing sync pattern");
            return false;
        }
    }
    
    // For FLAC frames, we need to find the actual end of the frame
    // This is more complex because FLAC frames are variable length
    // For now, we'll try to find the CRC-16 at the end or next sync pattern
    
    if (bytes_read == frame_size && frame_size > 2) {
        // Try to find the actual frame boundary by looking for next sync or EOF
        uint64_t search_start = frame.file_offset + bytes_read;
        uint64_t original_pos = m_handler->tell();
        
        // Look ahead for next frame sync to validate our frame size
        bool found_next_sync = false;
        const uint32_t max_search = 1024;  // Don't search too far
        
        for (uint32_t i = 0; i < max_search && search_start + i < m_file_size; i++) {
            if (!m_handler->seek(search_start + i, SEEK_SET)) {
                break;
            }
            
            uint8_t sync_bytes[2];
            if (m_handler->read(sync_bytes, 1, 2) != 2) {
                break;
            }
            
            // Check for FLAC sync pattern
            if (sync_bytes[0] == 0xFF && (sync_bytes[1] & 0xF8) == 0xF8) {
                // Found potential next frame, adjust our frame size
                uint32_t actual_frame_size = static_cast<uint32_t>(search_start + i - frame.file_offset);
                if (actual_frame_size < frame_size && actual_frame_size >= 4) {
                    Debug::log("flac", "Adjusted frame size from ", frame_size, " to ", actual_frame_size);
                    data.resize(actual_frame_size);
                    found_next_sync = true;
                }
                break;
            }
        }
        
        // Restore file position
        m_handler->seek(original_pos, SEEK_SET);
        
        if (!found_next_sync && m_file_size > 0 && search_start >= m_file_size) {
            // We're at the last frame, adjust size to file end
            uint32_t actual_frame_size = static_cast<uint32_t>(m_file_size - frame.file_offset);
            if (actual_frame_size < frame_size && actual_frame_size >= 4) {
                Debug::log("flac", "Last frame, adjusted size from ", frame_size, " to ", actual_frame_size);
                data.resize(actual_frame_size);
            }
        }
    }
    
    // Update current file position
    m_current_offset = frame.file_offset + data.size();
    
    Debug::log("flac", "Successfully read FLAC frame: ", data.size(), " bytes at offset ", frame.file_offset);
    
    return true;
}

void FLACDemuxer::resetPositionTracking()
{
    Debug::log("flac", "FLACDemuxer::resetPositionTracking() - resetting position to start");
    
    // Reset sample position to beginning of stream
    m_current_sample = 0;
    m_last_block_size = 0;
    
    // Reset file position to start of audio data
    m_current_offset = m_audio_data_offset;
    
    Debug::log("flac", "Position tracking reset: sample=", m_current_sample, 
              " file_offset=", m_current_offset, " (", samplesToMs(m_current_sample), " ms)");
}

void FLACDemuxer::updatePositionTracking(uint64_t sample_position, uint64_t file_offset)
{
    Debug::log("flac", "FLACDemuxer::updatePositionTracking() - updating position");
    
    // Validate sample position against stream bounds if known
    if (m_streaminfo.isValid() && m_streaminfo.total_samples > 0) {
        if (sample_position > m_streaminfo.total_samples) {
            Debug::log("flac", "Warning: sample position (", sample_position, 
                      ") exceeds total samples (", m_streaminfo.total_samples, ")");
            // Clamp to valid range
            sample_position = m_streaminfo.total_samples;
        }
    }
    
    // Update tracking variables
    m_current_sample = sample_position;
    m_current_offset = file_offset;
    
    Debug::log("flac", "Position tracking updated: sample=", m_current_sample, 
              " file_offset=", m_current_offset, " (", samplesToMs(m_current_sample), " ms)");
}

bool FLACDemuxer::seekWithTable(uint64_t target_sample)
{
    Debug::log("flac", "FLACDemuxer::seekWithTable() - seeking to sample ", target_sample);
    
    if (!m_handler) {
        reportError("IO", "No IOHandler available for seeking");
        return false;
    }
    
    if (m_seektable.empty()) {
        Debug::log("flac", "No seek table available");
        return false;
    }
    
    if (!m_streaminfo.isValid()) {
        reportError("State", "Invalid STREAMINFO for seeking");
        return false;
    }
    
    // Find the closest seek point before or at the target sample
    FLACSeekPoint best_seek_point;
    bool found_seek_point = false;
    
    for (const auto& seek_point : m_seektable) {
        if (seek_point.sample_number <= target_sample) {
            // This seek point is before or at our target
            if (!found_seek_point || seek_point.sample_number > best_seek_point.sample_number) {
                best_seek_point = seek_point;
                found_seek_point = true;
            }
        } else {
            // We've passed our target, stop searching (table is sorted)
            break;
        }
    }
    
    if (!found_seek_point) {
        Debug::log("flac", "No suitable seek point found for sample ", target_sample);
        return false;
    }
    
    Debug::log("flac", "Found seek point: sample=", best_seek_point.sample_number, 
              " offset=", best_seek_point.stream_offset, 
              " frame_samples=", best_seek_point.frame_samples);
    
    // Calculate absolute file position from stream offset
    // Stream offset is relative to the first frame (start of audio data)
    uint64_t file_position = m_audio_data_offset + best_seek_point.stream_offset;
    
    // Validate file position is within bounds
    if (m_file_size > 0 && file_position >= m_file_size) {
        reportError("Seek", "Seek table entry points beyond file end");
        return false;
    }
    
    // Seek to the file position
    if (!m_handler->seek(file_position, SEEK_SET)) {
        reportError("IO", "Failed to seek to file position " + std::to_string(file_position));
        return false;
    }
    
    // Update position tracking to the seek point
    updatePositionTracking(best_seek_point.sample_number, file_position);
    
    Debug::log("flac", "Seeked to file position ", file_position, 
              " (sample ", best_seek_point.sample_number, ")");
    
    // If we're exactly at the target, we're done
    if (best_seek_point.sample_number == target_sample) {
        Debug::log("flac", "Exact seek point match found");
        return true;
    }
    
    // Parse frames forward from seek point to exact target
    Debug::log("flac", "Parsing frames forward from sample ", best_seek_point.sample_number, 
              " to target ", target_sample);
    
    uint64_t current_sample = best_seek_point.sample_number;
    const uint32_t max_frames_to_parse = 1000;  // Prevent infinite loops
    uint32_t frames_parsed = 0;
    
    while (current_sample < target_sample && frames_parsed < max_frames_to_parse) {
        FLACFrame frame;
        
        // Find the next frame
        if (!findNextFrame(frame)) {
            Debug::log("flac", "Failed to find next frame during seek refinement");
            break;
        }
        
        // Check if this frame contains our target sample
        uint64_t frame_end_sample = frame.sample_offset + frame.block_size;
        
        if (frame.sample_offset <= target_sample && target_sample < frame_end_sample) {
            // Target sample is within this frame
            Debug::log("flac", "Target sample ", target_sample, " found in frame at sample ", 
                      frame.sample_offset);
            
            // Seek back to the start of this frame
            if (!m_handler->seek(frame.file_offset, SEEK_SET)) {
                reportError("IO", "Failed to seek back to target frame");
                return false;
            }
            
            // Update position tracking to this frame
            updatePositionTracking(frame.sample_offset, frame.file_offset);
            
            return true;
        }
        
        // Skip to next frame
        if (frame.frame_size > 0) {
            uint64_t next_frame_offset = frame.file_offset + frame.frame_size;
            if (!m_handler->seek(next_frame_offset, SEEK_SET)) {
                Debug::log("flac", "Failed to skip to next frame");
                break;
            }
            updatePositionTracking(frame_end_sample, next_frame_offset);
        } else {
            // Frame size unknown, try to read the frame to advance position
            std::vector<uint8_t> frame_data;
            if (!readFrameData(frame, frame_data)) {
                Debug::log("flac", "Failed to read frame data during seek");
                break;
            }
            updatePositionTracking(frame_end_sample, m_current_offset);
        }
        
        current_sample = frame_end_sample;
        frames_parsed++;
    }
    
    if (frames_parsed >= max_frames_to_parse) {
        Debug::log("flac", "Reached maximum frame parse limit during seek refinement");
        return false;
    }
    
    if (current_sample < target_sample) {
        Debug::log("flac", "Could not reach target sample ", target_sample, 
                  ", stopped at sample ", current_sample);
        return false;
    }
    
    Debug::log("flac", "Seek table based seeking completed successfully");
    return true;
}

bool FLACDemuxer::seekBinary(uint64_t target_sample)
{
    Debug::log("flac", "FLACDemuxer::seekBinary() - seeking to sample ", target_sample, " using binary search");
    
    if (!m_handler) {
        reportError("IO", "No IOHandler available for seeking");
        return false;
    }
    
    if (!m_streaminfo.isValid()) {
        reportError("State", "Invalid STREAMINFO for seeking");
        return false;
    }
    
    if (m_file_size == 0) {
        Debug::log("flac", "Unknown file size, cannot perform binary search");
        return false;
    }
    
    // Calculate search bounds
    uint64_t search_start = m_audio_data_offset;
    uint64_t search_end = m_file_size;
    
    // Estimate average bitrate for initial position guess
    uint64_t total_samples = m_streaminfo.total_samples;
    if (total_samples == 0) {
        Debug::log("flac", "Unknown total samples, using file size estimation");
        // Rough estimation: assume average compression ratio
        uint32_t bytes_per_sample = (m_streaminfo.channels * m_streaminfo.bits_per_sample) / 8;
        total_samples = (m_file_size - m_audio_data_offset) / (bytes_per_sample / 2);  // Assume 2:1 compression
    }
    
    Debug::log("flac", "Binary search bounds: file offset ", search_start, " to ", search_end);
    Debug::log("flac", "Estimated total samples: ", total_samples);
    
    // Binary search parameters
    const uint32_t max_iterations = 32;  // Prevent infinite loops
    const uint64_t sample_tolerance = m_streaminfo.max_block_size;  // Accept frames within one block
    
    uint32_t iteration = 0;
    uint64_t best_sample = 0;
    uint64_t best_file_offset = search_start;
    
    while (iteration < max_iterations && search_start < search_end) {
        iteration++;
        
        // Calculate midpoint file position
        uint64_t mid_offset = search_start + (search_end - search_start) / 2;
        
        Debug::log("flac", "Binary search iteration ", iteration, ": trying offset ", mid_offset);
        
        // Seek to midpoint
        if (!m_handler->seek(mid_offset, SEEK_SET)) {
            Debug::log("flac", "Failed to seek to offset ", mid_offset);
            break;
        }
        
        // Find the next valid FLAC frame from this position
        FLACFrame frame;
        bool found_frame = false;
        
        // Search forward from midpoint for a valid frame (up to 64KB)
        const uint32_t max_search_distance = 65536;
        uint64_t search_offset = mid_offset;
        
        while (search_offset < search_end && (search_offset - mid_offset) < max_search_distance) {
            if (!m_handler->seek(search_offset, SEEK_SET)) {
                break;
            }
            
            // Look for FLAC sync pattern
            uint8_t sync_bytes[2];
            if (m_handler->read(sync_bytes, 1, 2) != 2) {
                break;
            }
            
            if (sync_bytes[0] == 0xFF && (sync_bytes[1] & 0xF8) == 0xF8) {
                // Found potential sync, seek back and try to parse frame
                if (!m_handler->seek(search_offset, SEEK_SET)) {
                    break;
                }
                
                if (findNextFrame(frame)) {
                    // Validate frame header for consistency
                    if (validateFrameHeader(frame)) {
                        found_frame = true;
                        Debug::log("flac", "Found valid frame at offset ", frame.file_offset, 
                                  " sample ", frame.sample_offset);
                        break;
                    }
                }
            }
            
            search_offset++;
        }
        
        if (!found_frame) {
            Debug::log("flac", "No valid frame found near offset ", mid_offset);
            // Adjust search to lower half
            search_end = mid_offset;
            continue;
        }
        
        // Update best position if this is closer to target
        uint64_t sample_distance = (frame.sample_offset > target_sample) ? 
                                  (frame.sample_offset - target_sample) : 
                                  (target_sample - frame.sample_offset);
        
        uint64_t best_distance = (best_sample > target_sample) ? 
                                (best_sample - target_sample) : 
                                (target_sample - best_sample);
        
        if (iteration == 1 || sample_distance < best_distance) {
            best_sample = frame.sample_offset;
            best_file_offset = frame.file_offset;
            Debug::log("flac", "New best position: sample ", best_sample, " at offset ", best_file_offset);
        }
        
        // Check if we're close enough to the target
        if (sample_distance <= sample_tolerance) {
            Debug::log("flac", "Found frame within tolerance (", sample_distance, " samples)");
            break;
        }
        
        // Adjust search bounds based on frame position
        if (frame.sample_offset < target_sample) {
            // Frame is before target, search upper half
            search_start = frame.file_offset + 1;
        } else {
            // Frame is after target, search lower half
            search_end = frame.file_offset;
        }
        
        Debug::log("flac", "Adjusted search bounds: ", search_start, " to ", search_end);
    }
    
    if (iteration >= max_iterations) {
        Debug::log("flac", "Binary search reached maximum iterations");
    }
    
    // Seek to the best position found
    if (best_file_offset > 0) {
        Debug::log("flac", "Seeking to best position: sample ", best_sample, " at offset ", best_file_offset);
        
        if (!m_handler->seek(best_file_offset, SEEK_SET)) {
            reportError("IO", "Failed to seek to best position");
            return false;
        }
        
        updatePositionTracking(best_sample, best_file_offset);
        
        // If we're not exactly at the target, we may need linear refinement
        uint64_t sample_distance = (best_sample > target_sample) ? 
                                  (best_sample - target_sample) : 
                                  (target_sample - best_sample);
        
        if (sample_distance <= sample_tolerance) {
            Debug::log("flac", "Binary search successful, within tolerance");
            return true;
        } else {
            Debug::log("flac", "Binary search found approximate position, distance: ", sample_distance, " samples");
            return true;  // Still consider this successful for approximate seeking
        }
    }
    
    Debug::log("flac", "Binary search failed to find suitable position");
    return false;
}

bool FLACDemuxer::seekLinear(uint64_t target_sample)
{
    Debug::log("flac", "FLACDemuxer::seekLinear() - seeking to sample ", target_sample, " using linear search");
    
    if (!m_handler) {
        reportError("IO", "No IOHandler available for seeking");
        return false;
    }
    
    if (!m_streaminfo.isValid()) {
        reportError("State", "Invalid STREAMINFO for seeking");
        return false;
    }
    
    // Determine starting position for linear search
    uint64_t start_sample = 0;
    uint64_t start_offset = m_audio_data_offset;
    
    // Optimize for short-distance seeks from current position
    uint64_t current_distance = (m_current_sample > target_sample) ? 
                               (m_current_sample - target_sample) : 
                               (target_sample - m_current_sample);
    
    // If target is close to current position and we're ahead, start from current position
    const uint64_t short_seek_threshold = m_streaminfo.max_block_size * 10;  // 10 blocks
    
    if (current_distance <= short_seek_threshold && m_current_sample <= target_sample) {
        start_sample = m_current_sample;
        start_offset = m_current_offset;
        Debug::log("flac", "Short-distance seek: starting from current position (sample ", 
                  start_sample, ", offset ", start_offset, ")");
    } else {
        Debug::log("flac", "Long-distance seek: starting from beginning (sample ", 
                  start_sample, ", offset ", start_offset, ")");
    }
    
    // Seek to starting position
    if (!m_handler->seek(start_offset, SEEK_SET)) {
        reportError("IO", "Failed to seek to starting position");
        return false;
    }
    
    updatePositionTracking(start_sample, start_offset);
    
    // Linear search parameters
    const uint32_t max_frames_to_parse = 10000;  // Prevent runaway parsing
    uint32_t frames_parsed = 0;
    uint64_t current_sample = start_sample;
    
    Debug::log("flac", "Starting linear search from sample ", current_sample, " to target ", target_sample);
    
    while (current_sample < target_sample && frames_parsed < max_frames_to_parse) {
        FLACFrame frame;
        
        // Find the next frame
        if (!findNextFrame(frame)) {
            Debug::log("flac", "Failed to find next frame during linear search at sample ", current_sample);
            break;
        }
        
        frames_parsed++;
        
        // Check if this frame contains our target sample
        uint64_t frame_end_sample = frame.sample_offset + frame.block_size;
        
        Debug::log("flac", "Frame ", frames_parsed, ": samples ", frame.sample_offset, 
                  " to ", frame_end_sample, " (target: ", target_sample, ")");
        
        if (frame.sample_offset <= target_sample && target_sample < frame_end_sample) {
            // Target sample is within this frame - perfect match
            Debug::log("flac", "Target sample ", target_sample, " found in frame at sample ", 
                      frame.sample_offset);
            
            // Seek back to the start of this frame
            if (!m_handler->seek(frame.file_offset, SEEK_SET)) {
                reportError("IO", "Failed to seek back to target frame");
                return false;
            }
            
            // Update position tracking to this frame
            updatePositionTracking(frame.sample_offset, frame.file_offset);
            
            Debug::log("flac", "Linear seeking successful: positioned at sample ", 
                      frame.sample_offset, " (target was ", target_sample, ")");
            return true;
        }
        
        // If we've passed the target, position at this frame (closest we can get)
        if (frame.sample_offset > target_sample) {
            Debug::log("flac", "Passed target sample ", target_sample, 
                      ", positioning at frame sample ", frame.sample_offset);
            
            // Seek back to the start of this frame
            if (!m_handler->seek(frame.file_offset, SEEK_SET)) {
                reportError("IO", "Failed to seek back to closest frame");
                return false;
            }
            
            // Update position tracking to this frame
            updatePositionTracking(frame.sample_offset, frame.file_offset);
            
            Debug::log("flac", "Linear seeking successful: positioned at sample ", 
                      frame.sample_offset, " (closest to target ", target_sample, ")");
            return true;
        }
        
        // Continue to next frame
        current_sample = frame_end_sample;
        
        // Skip to next frame position
        if (frame.frame_size > 0) {
            uint64_t next_frame_offset = frame.file_offset + frame.frame_size;
            if (!m_handler->seek(next_frame_offset, SEEK_SET)) {
                Debug::log("flac", "Failed to skip to next frame");
                break;
            }
            updatePositionTracking(current_sample, next_frame_offset);
        } else {
            // Frame size unknown, read the frame to advance position
            std::vector<uint8_t> frame_data;
            if (!readFrameData(frame, frame_data)) {
                Debug::log("flac", "Failed to read frame data during linear search");
                break;
            }
            updatePositionTracking(current_sample, m_current_offset);
        }
        
        // Progress logging for long searches
        if (frames_parsed % 100 == 0) {
            Debug::log("flac", "Linear search progress: parsed ", frames_parsed, 
                      " frames, at sample ", current_sample);
        }
    }
    
    if (frames_parsed >= max_frames_to_parse) {
        Debug::log("flac", "Linear search reached maximum frame limit (", max_frames_to_parse, ")");
        return false;
    }
    
    if (current_sample < target_sample) {
        Debug::log("flac", "Linear search reached end of stream at sample ", current_sample, 
                  " (target was ", target_sample, ")");
        
        // Position at the last valid position we found
        Debug::log("flac", "Positioning at end of stream");
        return true;  // Consider this successful - we're at the end
    }
    
    Debug::log("flac", "Linear search completed successfully");
    return true;
}

uint64_t FLACDemuxer::samplesToMs(uint64_t samples) const
{
    if (m_streaminfo.sample_rate == 0) {
        return 0;
    }
    
    // Use 64-bit arithmetic to prevent overflow for very long files
    // For extremely large sample counts, we need to be careful about overflow
    // when multiplying by 1000
    
    // Check if samples * 1000 would overflow
    if (samples > UINT64_MAX / 1000) {
        // Use division first to prevent overflow
        // This may lose some precision but prevents overflow
        uint64_t seconds = samples / m_streaminfo.sample_rate;
        uint64_t remaining_samples = samples % m_streaminfo.sample_rate;
        uint64_t remaining_ms = (remaining_samples * 1000) / m_streaminfo.sample_rate;
        return (seconds * 1000) + remaining_ms;
    }
    
    return (samples * 1000ULL) / m_streaminfo.sample_rate;
}

uint64_t FLACDemuxer::msToSamples(uint64_t ms) const
{
    if (m_streaminfo.sample_rate == 0) {
        return 0;
    }
    
    // Use 64-bit arithmetic to prevent overflow for very long durations
    // Check if ms * sample_rate would overflow
    if (ms > UINT64_MAX / m_streaminfo.sample_rate) {
        // Use division first to prevent overflow
        uint64_t seconds = ms / 1000;
        uint64_t remaining_ms = ms % 1000;
        uint64_t remaining_samples = (remaining_ms * m_streaminfo.sample_rate) / 1000;
        return (seconds * m_streaminfo.sample_rate) + remaining_samples;
    }
    
    return (ms * static_cast<uint64_t>(m_streaminfo.sample_rate)) / 1000ULL;
}