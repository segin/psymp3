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

// Debug logging macro with method identification token and line number
#define FLAC_DEBUG(...) Debug::log("flac", "[", __FUNCTION__, ":", __LINE__, "] ", __VA_ARGS__)

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
    m_frame_index.clear();
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
        // Requirement 24.8: Handle I/O errors gracefully
        reportError("IO", "No IOHandler available");
        return false;
    }
    
    // Seek to beginning
    // Requirement 24.8: Handle I/O errors gracefully
    if (m_handler->seek(0, SEEK_SET) != 0) {
        return handleIOError_unlocked("seek to beginning of file");
    }
    
    // Check for ID3v2 tag and skip past it if present
    // ID3 tags can be prepended to FLAC files by tagging software
    uint8_t id3_header[10];
    size_t id3_bytes = m_handler->read(id3_header, 1, 10);
    
    if (id3_bytes >= 10 && id3_header[0] == 'I' && id3_header[1] == 'D' && id3_header[2] == '3') {
        // ID3v2 tag found - calculate size and skip past it
        // Size is stored as syncsafe integer (4 bytes, 7 bits each)
        size_t id3_size = ((id3_header[6] & 0x7F) << 21) |
                          ((id3_header[7] & 0x7F) << 14) |
                          ((id3_header[8] & 0x7F) << 7) |
                          (id3_header[9] & 0x7F);
        
        // Total size = header (10 bytes) + tag size
        size_t total_id3_size = 10 + id3_size;
        
        FLAC_DEBUG("Found ID3v2 tag at file start, size: ", total_id3_size, " bytes");
        FLAC_DEBUG("Skipping ID3 tag to find FLAC stream marker");
        
        // Seek past the ID3 tag
        if (m_handler->seek(total_id3_size, SEEK_SET) != 0) {
            return handleIOError_unlocked("seek past ID3 tag");
        }
        
        // Store the offset where FLAC data actually starts
        m_id3_tag_offset = total_id3_size;
    } else {
        // No ID3 tag - seek back to beginning
        if (m_handler->seek(0, SEEK_SET) != 0) {
            return handleIOError_unlocked("seek back to beginning after ID3 check");
        }
        m_id3_tag_offset = 0;
    }
    
    // Validate fLaC stream marker (RFC 9639 Section 6)
    // Requirement 24.1: Reject invalid stream markers without crashing
    if (!validateStreamMarker_unlocked()) {
        FLAC_DEBUG("[parseContainer] Requirement 24.1: Invalid stream marker, rejecting file");
        return false;
    }
    
    // Parse metadata blocks (RFC 9639 Section 8)
    // Requirement 24.2: Skip corrupted metadata blocks and continue
    bool metadata_ok = parseMetadataBlocks_unlocked();
    
    // Verify STREAMINFO is valid
    if (!m_streaminfo.isValid()) {
        // Requirement 24.3: If STREAMINFO is missing, derive parameters from frame headers
        FLAC_DEBUG("[parseContainer] Requirement 24.3: STREAMINFO invalid or missing, attempting frame header derivation");
        
        if (!metadata_ok) {
            // Metadata parsing failed completely, try to find audio data
            // Scan forward from current position to find first frame
            FLAC_DEBUG("[parseContainer] Metadata parsing failed, scanning for first frame");
        }
        
        if (!deriveParametersFromFrameHeaders_unlocked()) {
            reportError("Format", "Invalid or missing STREAMINFO block and unable to derive parameters from frame headers");
            return false;
        }
        
        FLAC_DEBUG("[parseContainer] Successfully derived parameters from frame headers");
    }
    
    // Validate streamable subset compliance (RFC 9639 Section 7)
    // Requirements 20.1-20.5: Check streamable subset constraints
    // This must be called after STREAMINFO and VORBIS_COMMENT are parsed
    validateStreamableSubset_unlocked();
    
    // Create VorbisCommentTag from parsed metadata
    // Requirements 8.2, 8.4: Extract VorbisComment metadata and store in m_tag
    createTagFromMetadata_unlocked();
    
    m_container_parsed = true;
    m_current_offset = m_audio_data_offset;
    updateCurrentSample_unlocked(0);
    updateEOF_unlocked(false);
    
    FLAC_DEBUG("Container parsing complete");
    FLAC_DEBUG("  Sample rate: ", m_streaminfo.sample_rate, " Hz");
    FLAC_DEBUG("  Channels: ", static_cast<int>(m_streaminfo.channels));
    FLAC_DEBUG("  Bits per sample: ", static_cast<int>(m_streaminfo.bits_per_sample));
    FLAC_DEBUG("  Total samples: ", m_streaminfo.total_samples);
    FLAC_DEBUG("  Audio data offset: ", m_audio_data_offset);
    FLAC_DEBUG("  Streamable subset: ", (m_is_streamable_subset ? "yes" : "no"));
    
    return true;
}

std::vector<StreamInfo> FLACDemuxer::getStreams_unlocked() const
{
    FLAC_DEBUG("[getStreams] Building StreamInfo for FLAC stream");
    
    if (!m_container_parsed || !m_streaminfo.isValid()) {
        FLAC_DEBUG("[getStreams] Container not parsed or STREAMINFO invalid");
        return {};
    }
    
    StreamInfo stream;
    stream.stream_id = 1;
    stream.codec_type = "audio";
    stream.codec_name = "flac";
    stream.codec_tag = 0x43614C66; // 'fLaC' in big-endian - identifies native FLAC
    stream.sample_rate = m_streaminfo.sample_rate;
    stream.channels = m_streaminfo.channels;
    stream.bits_per_sample = m_streaminfo.bits_per_sample;
    stream.duration_samples = m_streaminfo.total_samples;
    stream.duration_ms = m_streaminfo.getDurationMs();
    
    // Requirement 26.2, 26.8: Populate codec_data with FLAC-specific parameters
    // Format: 8 bytes - 4 bytes min_block_size (big-endian), 4 bytes max_block_size (big-endian)
    // This provides FLACCodec with block size information for proper decoding
    stream.codec_data.resize(8);
    // Min block size (big-endian)
    stream.codec_data[0] = static_cast<uint8_t>((m_streaminfo.min_block_size >> 24) & 0xFF);
    stream.codec_data[1] = static_cast<uint8_t>((m_streaminfo.min_block_size >> 16) & 0xFF);
    stream.codec_data[2] = static_cast<uint8_t>((m_streaminfo.min_block_size >> 8) & 0xFF);
    stream.codec_data[3] = static_cast<uint8_t>(m_streaminfo.min_block_size & 0xFF);
    // Max block size (big-endian)
    stream.codec_data[4] = static_cast<uint8_t>((m_streaminfo.max_block_size >> 24) & 0xFF);
    stream.codec_data[5] = static_cast<uint8_t>((m_streaminfo.max_block_size >> 16) & 0xFF);
    stream.codec_data[6] = static_cast<uint8_t>((m_streaminfo.max_block_size >> 8) & 0xFF);
    stream.codec_data[7] = static_cast<uint8_t>(m_streaminfo.max_block_size & 0xFF);
    
    FLAC_DEBUG("[getStreams] codec_data populated with block sizes: min=", 
               m_streaminfo.min_block_size, ", max=", m_streaminfo.max_block_size);
    
    // Requirement 26.4: Calculate bitrate if frame size information is available
    // Bitrate = (average_frame_size * 8 * sample_rate) / average_block_size
    if (m_streaminfo.min_frame_size > 0 && m_streaminfo.max_frame_size > 0 &&
        m_streaminfo.min_block_size > 0 && m_streaminfo.sample_rate > 0) {
        // Use average of min and max frame sizes for estimation
        uint32_t avg_frame_size = (m_streaminfo.min_frame_size + m_streaminfo.max_frame_size) / 2;
        uint32_t avg_block_size = (m_streaminfo.min_block_size + m_streaminfo.max_block_size) / 2;
        if (avg_block_size > 0) {
            // Calculate bitrate in bits per second
            stream.bitrate = (static_cast<uint64_t>(avg_frame_size) * 8 * m_streaminfo.sample_rate) / avg_block_size;
            FLAC_DEBUG("[getStreams] Estimated bitrate: ", stream.bitrate, " bps");
        }
    }
    
    // Add metadata from Vorbis comments
    auto it = m_vorbis_comments.find("ARTIST");
    if (it != m_vorbis_comments.end()) stream.artist = it->second;
    
    it = m_vorbis_comments.find("TITLE");
    if (it != m_vorbis_comments.end()) stream.title = it->second;
    
    it = m_vorbis_comments.find("ALBUM");
    if (it != m_vorbis_comments.end()) stream.album = it->second;
    
    FLAC_DEBUG("[getStreams] StreamInfo complete: ", stream.sample_rate, "Hz, ",
               static_cast<int>(stream.channels), "ch, ", 
               static_cast<int>(stream.bits_per_sample), "bit, ",
               stream.duration_ms, "ms");
    
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


/**
 * @brief Read the next FLAC frame as a MediaChunk
 * 
 * Implements Requirements 21.7, 21.8, 26.3 and Error Handling 24.4-24.8:
 * - Requirement 21.7: Maintain accurate sample position tracking during reading
 * - Requirement 21.8: Include complete frames with proper MediaChunk formatting
 * - Requirement 26.3: Return frame as MediaChunk with proper timing
 * - Requirement 24.4: Resynchronize to next valid sync code on sync loss
 * - Requirement 24.5: Log CRC errors but attempt to continue
 * - Requirement 24.6: Handle truncated files gracefully
 * - Requirement 24.7: Return appropriate error codes on allocation failure
 * - Requirement 24.8: Handle I/O errors without crashing
 * 
 * This method:
 * 1. Locates the next frame sync code (0xFFF8 or 0xFFF9)
 * 2. Parses the frame header to determine block size and other parameters
 * 3. Estimates frame size using STREAMINFO minimum frame size
 * 4. Reads the complete frame data including CRC-16 footer
 * 5. Returns the frame as a MediaChunk with accurate timing information
 * 
 * @return MediaChunk containing the frame data, or empty chunk on EOF/error
 */
MediaChunk FLACDemuxer::readChunk_unlocked()
{
    FLAC_DEBUG("[readChunk] Starting frame read");
    
    if (!m_container_parsed) {
        FLAC_DEBUG("[readChunk] Container not parsed");
        return MediaChunk{};
    }
    
    if (isEOF_unlocked()) {
        FLAC_DEBUG("[readChunk] At end of file");
        return MediaChunk{};
    }
    
    // ========================================================================
    // Step 1: Locate next frame sync code
    // Requirement 21.7: Maintain accurate sample position tracking
    // Requirement 24.4: Resynchronize to next valid sync code on sync loss
    // ========================================================================
    
    FLACFrame frame;
    int resync_attempts = 0;
    static constexpr int MAX_RESYNC_ATTEMPTS = 3;
    
    while (!findNextFrame_unlocked(frame)) {
        // Requirement 24.4: Attempt resynchronization on sync loss
        FLAC_DEBUG("[readChunk] Requirement 24.4: Frame sync not found, attempting resync");
        
        if (resync_attempts >= MAX_RESYNC_ATTEMPTS) {
            FLAC_DEBUG("[readChunk] Max resync attempts (", MAX_RESYNC_ATTEMPTS, ") reached");
            updateEOF_unlocked(true);
            return MediaChunk{};
        }
        
        // Move forward and try to resync
        m_current_offset += 1;  // Skip one byte and try again
        
        if (!resyncToNextFrame_unlocked()) {
            FLAC_DEBUG("[readChunk] Resync failed, no more frames found");
            updateEOF_unlocked(true);
            return MediaChunk{};
        }
        
        resync_attempts++;
        FLAC_DEBUG("[readChunk] Resync attempt ", resync_attempts, " - trying again");
    }
    
    FLAC_DEBUG("[readChunk] Found frame at offset ", frame.file_offset);
    FLAC_DEBUG("[readChunk]   Block size: ", frame.block_size, " samples");
    FLAC_DEBUG("[readChunk]   Sample offset: ", frame.sample_offset);
    
    // ========================================================================
    // Step 2: Calculate frame size using STREAMINFO minimum frame size
    // Requirement 21.1: Use STREAMINFO minimum frame size as primary estimate
    // ========================================================================
    
    uint32_t estimated_frame_size = calculateFrameSize_unlocked(frame);
    
    // Validate frame size
    if (estimated_frame_size == 0) {
        FLAC_DEBUG("[readChunk] Invalid frame size estimate: 0");
        // Requirement 24.4: Try to skip this frame and continue
        if (skipCorruptedFrame_unlocked(frame.file_offset)) {
            FLAC_DEBUG("[readChunk] Skipped corrupted frame, retrying");
            return readChunk_unlocked();  // Recursive retry (limited by resync attempts)
        }
        updateEOF_unlocked(true);
        return MediaChunk{};
    }
    
    // Cap at reasonable maximum (1 MB) to prevent excessive memory allocation
    // Requirement 24.7: Handle allocation failures gracefully
    static constexpr uint32_t MAX_FRAME_SIZE = 1024 * 1024;
    if (estimated_frame_size > MAX_FRAME_SIZE) {
        FLAC_DEBUG("[readChunk] Frame size estimate (", estimated_frame_size, 
                   ") exceeds maximum, capping at ", MAX_FRAME_SIZE);
        estimated_frame_size = MAX_FRAME_SIZE;
    }
    
    // Check available data in file
    // Requirement 24.6: Handle truncated files gracefully
    uint32_t available_bytes = estimated_frame_size;
    if (m_file_size > 0 && frame.file_offset + estimated_frame_size > m_file_size) {
        available_bytes = static_cast<uint32_t>(m_file_size - frame.file_offset);
        FLAC_DEBUG("[readChunk] Requirement 24.6: Adjusted frame size to available bytes: ", available_bytes);
        
        // If very little data available, we're likely at EOF
        if (available_bytes < 9) {  // Minimum frame size
            FLAC_DEBUG("[readChunk] Requirement 24.6: Insufficient data for frame, treating as EOF");
            updateEOF_unlocked(true);
            return MediaChunk{};
        }
    }
    
    // ========================================================================
    // Step 3: Read frame data
    // Requirement 21.8: Include complete frames with proper MediaChunk formatting
    // Requirement 24.7: Handle allocation failures gracefully
    // Requirement 24.8: Handle I/O errors without crashing
    // ========================================================================
    
    // Seek to frame position
    if (m_handler->seek(static_cast<off_t>(frame.file_offset), SEEK_SET) != 0) {
        // Requirement 24.8: Handle I/O errors gracefully
        return handleIOError_unlocked("seek to frame position") ? MediaChunk{} : MediaChunk{};
    }
    
    // Allocate buffer with exception handling
    // Requirement 24.7: Return appropriate error codes on allocation failure
    std::vector<uint8_t> data;
    try {
        data.resize(available_bytes);
    } catch (const std::bad_alloc&) {
        handleAllocationFailure_unlocked("frame data buffer", available_bytes);
        return MediaChunk{};
    } catch (const std::length_error&) {
        handleAllocationFailure_unlocked("frame data buffer (length error)", available_bytes);
        return MediaChunk{};
    }
    
    size_t bytes_read = m_handler->read(data.data(), 1, available_bytes);
    
    if (bytes_read == 0) {
        // Requirement 24.8: Handle EOF condition gracefully
        FLAC_DEBUG("[readChunk] Requirement 24.8: No data read - end of file");
        updateEOF_unlocked(true);
        return MediaChunk{};
    }
    
    FLAC_DEBUG("[readChunk] Read ", bytes_read, " bytes of frame data");
    
    // ========================================================================
    // Step 4: Find actual frame end by searching for next sync code
    // This ensures we read the complete frame including CRC-16 footer
    // ========================================================================
    
    uint32_t actual_frame_size = static_cast<uint32_t>(bytes_read);
    
    // Search for next sync code starting after the minimum frame header size
    // Minimum frame: sync(2) + header(2) + coded_number(1) + CRC-8(1) + subframe(1) + CRC-16(2) = 9 bytes
    static constexpr size_t MIN_FRAME_SIZE = 9;
    
    // Requirement 21.1: Use STREAMINFO min_frame_size to avoid false positives
    // Sync codes found before min_frame_size are likely data patterns, not frame boundaries
    size_t search_start = MIN_FRAME_SIZE;
    if (m_streaminfo.isValid() && m_streaminfo.min_frame_size > MIN_FRAME_SIZE) {
        search_start = m_streaminfo.min_frame_size;
    }
    
    if (bytes_read > search_start) {
        // Search for next sync code (0xFF followed by 0xF8 or 0xF9)
        // Ensure we have enough bytes to check data[i+2]
        for (size_t i = search_start; i < bytes_read - 16; ++i) { // Ensure enough buffer for header max size
            if (data[i] == 0xFF && (data[i + 1] == 0xF8 || data[i + 1] == 0xF9)) {
                // Validate third byte to avoid simple false positives
                // data[i+2] contains Block Size (high nibble) and Sample Rate (low nibble)
                // 0xFF would mean invalid/reserved values for both
                if (data[i + 2] == 0xFF) {
                    continue;
                }
                
                // Decode UTF-8 coded number length (byte 4) to find header size
                // RFC 9639 Section 8.1.1: UTF-8 coding
                size_t coded_num_len = 0;
                uint8_t v = data[i + 4];
                
                if ((v & 0x80) == 0) coded_num_len = 1;
                else if ((v & 0xE0) == 0xC0) coded_num_len = 2;
                else if ((v & 0xF0) == 0xE0) coded_num_len = 3;
                else if ((v & 0xF8) == 0xF0) coded_num_len = 4;
                else if ((v & 0xFC) == 0xF8) coded_num_len = 5;
                else if ((v & 0xFE) == 0xFC) coded_num_len = 6;
                else if ((v & 0xFF) == 0xFE) coded_num_len = 7;
                else continue; // Invalid UTF-8 start byte
                
                // Calculate total header length excluding CRC-8
                // Sync(2) + Header(2) + Coded Number(L)
                size_t header_len_no_crc = 4 + coded_num_len;
                
                // Ensure we have enough data for the full header + CRC byte
                if (i + header_len_no_crc + 1 > bytes_read) {
                    continue;
                }
                
                // Validate CRC-8
                uint8_t expected_crc = data[i + header_len_no_crc];
                uint8_t calculated_crc = calculateCRC8(data.data() + i, header_len_no_crc);
                
                if (calculated_crc != expected_crc) {
                    // CRC mismatch - false positive sync code
                    continue;
                }
                
                // For fixed-block streams, validate that block size matches STREAMINFO
                // This provides additional protection against CRC-8 collisions (1/256 chance)
                if (m_streaminfo.isValid() && 
                    m_streaminfo.min_block_size == m_streaminfo.max_block_size) {
                    // Extract block size bits (high nibble of byte 2)
                    uint8_t block_size_bits = (data[i + 2] >> 4) & 0x0F;
                    
                    // Lookup expected block size from bits (RFC 9639 Table)
                    // 0000 = reserved, 0001 = 192, 0010 = 576, 0011 = 1152
                    // 0100 = 2304, 0101 = 4608, 0110 = 256, etc.
                    uint32_t expected_block_size = 0;
                    switch (block_size_bits) {
                        case 0x01: expected_block_size = 192; break;
                        case 0x02: expected_block_size = 576; break;
                        case 0x03: expected_block_size = 1152; break;
                        case 0x04: expected_block_size = 2304; break;
                        case 0x05: expected_block_size = 4608; break;
                        case 0x06: case 0x07: 
                            // These require reading extra bytes - skip validation
                            expected_block_size = m_streaminfo.min_block_size;
                            break;
                        case 0x08: expected_block_size = 256; break;
                        case 0x09: expected_block_size = 512; break;
                        case 0x0A: expected_block_size = 1024; break;
                        case 0x0B: expected_block_size = 2048; break;
                        case 0x0C: expected_block_size = 4096; break;
                        case 0x0D: expected_block_size = 8192; break;
                        case 0x0E: expected_block_size = 16384; break;
                        case 0x0F: expected_block_size = 32768; break;
                        default: expected_block_size = 0; break;
                    }
                    
                    if (expected_block_size != 0 && 
                        expected_block_size != m_streaminfo.min_block_size) {
                        // Block size mismatch - false positive
                        continue;
                    }
                    
                    // Also validate sample rate if STREAMINFO has a known rate
                    if (m_streaminfo.sample_rate > 0) {
                        uint8_t sample_rate_bits = data[i + 2] & 0x0F;
                        uint32_t expected_sample_rate = 0;
                        
                        switch (sample_rate_bits) {
                            case 0x01: expected_sample_rate = 88200; break;
                            case 0x02: expected_sample_rate = 176400; break;
                            case 0x03: expected_sample_rate = 192000; break;
                            case 0x04: expected_sample_rate = 8000; break;
                            case 0x05: expected_sample_rate = 16000; break;
                            case 0x06: expected_sample_rate = 22050; break;
                            case 0x07: expected_sample_rate = 24000; break;
                            case 0x08: expected_sample_rate = 32000; break;
                            case 0x09: expected_sample_rate = 44100; break;
                            case 0x0A: expected_sample_rate = 48000; break;
                            case 0x0B: expected_sample_rate = 96000; break;
                            case 0x0C: case 0x0D: case 0x0E:
                                // These require reading extra bytes - skip validation
                                expected_sample_rate = m_streaminfo.sample_rate;
                                break;
                            case 0x0F: expected_sample_rate = 0; break; // Invalid
                            default: expected_sample_rate = 0; break; // Reserved
                        }
                        
                        if (expected_sample_rate != 0 && 
                            expected_sample_rate != m_streaminfo.sample_rate) {
                            // Sample rate mismatch - false positive
                            continue;
                        }
                    }
                }
                
                // Found potential next frame sync code with valid CRC
                actual_frame_size = static_cast<uint32_t>(i);
                FLAC_DEBUG("[readChunk] Found next sync code at offset ", i, 
                           " - actual frame size: ", actual_frame_size, " bytes");
                break;
            }
        }
    }
    
    // Resize data to actual frame size
    if (actual_frame_size < bytes_read) {
        data.resize(actual_frame_size);
    }
    
    // Store the actual frame size in the frame structure
    frame.frame_size = actual_frame_size;
    
    // ========================================================================
    // Step 4.5: Add frame to index for future seeking
    // Requirement 22.4: Build frame index during initial parsing
    // ========================================================================
    
    addFrameToIndex_unlocked(frame);
    
    // ========================================================================
    // Step 5: Create MediaChunk with proper timing
    // Requirement 26.3: Return frame as MediaChunk with proper timing
    // ========================================================================
    
    MediaChunk chunk(1, std::move(data));
    chunk.timestamp_samples = frame.sample_offset;
    chunk.is_keyframe = true;  // All FLAC frames are keyframes (independent)
    chunk.file_offset = frame.file_offset;
    
    // ========================================================================
    // Step 6: Update position tracking
    // Requirement 21.7: Maintain accurate sample position tracking
    // Requirement 23.3: Maintain current sample position during reading
    // Requirement 23.6: Provide accurate position reporting
    // ========================================================================
    
    // Update current sample position based on frame block size
    // Requirement 28.6: Use atomic operations for sample counters
    updateCurrentSample_unlocked(frame.sample_offset + frame.block_size);
    
    // Update current file offset to point to the next frame
    m_current_offset = frame.file_offset + actual_frame_size;
    
    FLAC_DEBUG("[readChunk] Frame read complete:");
    FLAC_DEBUG("[readChunk]   Frame size: ", actual_frame_size, " bytes");
    FLAC_DEBUG("[readChunk]   Sample range: ", frame.sample_offset, " - ", m_current_sample);
    FLAC_DEBUG("[readChunk]   Next offset: ", m_current_offset);
    
    return chunk;
}

/**
 * @brief Seek to a specific timestamp in the FLAC stream
 * 
 * Implements Requirements 22.1, 22.5, 22.6:
 * - Requirement 22.1: Reset to first audio frame for seek to beginning
 * - Requirement 22.5: Maintain current position on seek failure
 * - Requirement 22.6: Clamp seeks beyond stream end to last valid position
 * 
 * Seeking strategy priority:
 * 1. Frame index (if available) - most accurate
 * 2. SEEKTABLE (if available) - RFC 9639 standard
 * 3. Fallback to beginning - always works
 * 
 * @param timestamp_ms Target timestamp in milliseconds
 * @return true if seek succeeded, false otherwise
 */
bool FLACDemuxer::seekTo_unlocked(uint64_t timestamp_ms)
{
    FLAC_DEBUG("[seekTo] Seeking to ", timestamp_ms, " ms");
    
    if (!m_container_parsed) {
        reportError("State", "Container not parsed");
        return false;
    }
    
    // Requirement 22.5: Save current position in case of seek failure
    uint64_t saved_offset = m_current_offset;
    uint64_t saved_sample = m_current_sample;
    bool saved_eof = m_eof;
    
    // Requirement 22.1: Reset to first audio frame for seek to beginning
    if (timestamp_ms == 0) {
        FLAC_DEBUG("[seekTo] Seeking to beginning (timestamp_ms == 0)");
        
        if (m_handler->seek(static_cast<off_t>(m_audio_data_offset), SEEK_SET) != 0) {
            // Requirement 22.5: Maintain current position on seek failure
            FLAC_DEBUG("[seekTo] Failed to seek to beginning, restoring position");
            m_current_offset = saved_offset;
            updateCurrentSample_unlocked(saved_sample);
            updateEOF_unlocked(saved_eof);
            reportError("IO", "Failed to seek to beginning");
            return false;
        }
        
        m_current_offset = m_audio_data_offset;
        updateCurrentSample_unlocked(0);
        updateEOF_unlocked(false);
        
        FLAC_DEBUG("[seekTo] Successfully seeked to beginning");
        return true;
    }
    
    // Convert timestamp to samples
    uint64_t target_sample = msToSamples(timestamp_ms);
    
    FLAC_DEBUG("[seekTo] Target sample: ", target_sample, 
               " (from ", timestamp_ms, " ms at ", m_streaminfo.sample_rate, " Hz)");
    
    // Requirement 22.6: Clamp seeks beyond stream end to last valid position
    if (m_streaminfo.total_samples > 0 && target_sample >= m_streaminfo.total_samples) {
        uint64_t clamped_sample = m_streaminfo.total_samples > 0 ? m_streaminfo.total_samples - 1 : 0;
        FLAC_DEBUG("[seekTo] Clamping target sample from ", target_sample, 
                   " to ", clamped_sample, " (stream end)");
        target_sample = clamped_sample;
    }
    
    // Strategy 1: Try frame index first (most accurate)
    // Requirement 22.4, 22.7: Use cached frame positions for sample-accurate seeking
    if (!m_frame_index.empty()) {
        FLAC_DEBUG("[seekTo] Attempting seek using frame index (", m_frame_index.size(), " entries)");
        
        if (seekWithFrameIndex_unlocked(target_sample)) {
            FLAC_DEBUG("[seekTo] Frame index seek successful");
            return true;
        }
        
        FLAC_DEBUG("[seekTo] Frame index seek failed, trying SEEKTABLE");
    }
    
    // Strategy 2: Try SEEKTABLE (RFC 9639 standard)
    // Requirements 22.2, 22.3, 22.8
    if (!m_seektable.empty()) {
        FLAC_DEBUG("[seekTo] Attempting seek using SEEKTABLE (", m_seektable.size(), " entries)");
        
        if (seekWithSeekTable_unlocked(target_sample)) {
            FLAC_DEBUG("[seekTo] SEEKTABLE seek successful");
            return true;
        }
        
        FLAC_DEBUG("[seekTo] SEEKTABLE seek failed, trying byte estimation");
    }
    
    // Strategy 3: Byte-position estimation (like VLC does)
    // Estimate byte position based on ratio of target time to total duration
    if (m_streaminfo.total_samples > 0 && m_file_size > m_audio_data_offset) {
        FLAC_DEBUG("[seekTo] Attempting byte-position estimation seek");
        
        if (seekWithByteEstimation_unlocked(target_sample)) {
            FLAC_DEBUG("[seekTo] Byte estimation seek successful");
            return true;
        }
        
        FLAC_DEBUG("[seekTo] Byte estimation seek failed, falling back to beginning");
    }
    
    // Strategy 4: Fallback to beginning
    // This is the last resort when all other strategies fail
    FLAC_DEBUG("[seekTo] All seek strategies failed, seeking to beginning");
    
    if (m_handler->seek(static_cast<off_t>(m_audio_data_offset), SEEK_SET) != 0) {
        // Requirement 22.5: Maintain current position on seek failure
        FLAC_DEBUG("[seekTo] Failed to seek to beginning, restoring position");
        m_current_offset = saved_offset;
        updateCurrentSample_unlocked(saved_sample);
        updateEOF_unlocked(saved_eof);
        reportError("IO", "Failed to seek to beginning for fallback");
        return false;
    }
    
    m_current_offset = m_audio_data_offset;
    updateCurrentSample_unlocked(0);
    updateEOF_unlocked(false);
    
    FLAC_DEBUG("[seekTo] Seek complete, now at sample ", m_current_sample, 
               " (offset ", m_current_offset, ")");
    return true;
}

bool FLACDemuxer::isEOF_unlocked() const
{
    if (m_eof) return true;
    if (!m_handler) return true;
    // Use file offset tracking rather than feof() since feof() returns true
    // when the file buffer is exhausted, not when all frames are demuxed
    if (m_file_size > 0 && m_current_offset >= m_file_size) return true;
    // Don't use m_handler->eof() as fallback - it can return true prematurely
    // when the IOHandler's internal buffer hits EOF but there's still data
    // to be demuxed from the buffer
    return false;
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
    
    // Requirement 1.1: Read first 4 bytes (after any ID3 tag)
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
    FLAC_DEBUG("Valid fLaC stream marker found at file offset ", m_id3_tag_offset);
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
    int corrupted_blocks_skipped = 0;  // Track corrupted blocks for Requirement 24.2
    
    while (!is_last) {
        FLACMetadataBlock block;
        
        if (!parseMetadataBlockHeader_unlocked(block)) {
            // Requirement 24.2: Skip corrupted metadata blocks and continue
            FLAC_DEBUG("[parseMetadataBlocks] Requirement 24.2: Failed to parse metadata block header");
            
            if (is_first_block) {
                // First block must be STREAMINFO - can't continue without it
                reportError("Format", "Failed to parse first metadata block header");
                return false;
            }
            
            // Try to recover by scanning for next valid block or audio data
            FLAC_DEBUG("[parseMetadataBlocks] Attempting to recover from corrupted metadata");
            corrupted_blocks_skipped++;
            
            // If we've skipped too many blocks, give up
            if (corrupted_blocks_skipped > 10) {
                FLAC_DEBUG("[parseMetadataBlocks] Too many corrupted blocks, giving up");
                reportError("Format", "Too many corrupted metadata blocks");
                return false;
            }
            
            // Try to find audio data by scanning for frame sync
            // This will set m_audio_data_offset if successful
            break;  // Exit metadata parsing loop, try to find audio data
        }
        
        is_last = block.is_last;
        
        FLAC_DEBUG("Metadata block: type=", static_cast<int>(block.type), 
                   ", length=", block.length, ", is_last=", is_last,
                   ", is_first=", is_first_block);
        
        // Validate block length to prevent reading past EOF
        // Requirement 24.6: Handle truncated files gracefully
        if (m_file_size > 0 && block.data_offset + block.length > m_file_size) {
            FLAC_DEBUG("[parseMetadataBlocks] Requirement 24.6: Block extends past EOF");
            FLAC_DEBUG("[parseMetadataBlocks]   Block end: ", block.data_offset + block.length);
            FLAC_DEBUG("[parseMetadataBlocks]   File size: ", m_file_size);
            
            if (is_first_block) {
                reportError("Format", "First metadata block extends past end of file");
                return false;
            }
            
            // Skip this corrupted block
            FLAC_DEBUG("[parseMetadataBlocks] Skipping truncated metadata block");
            corrupted_blocks_skipped++;
            is_first_block = false;
            break;  // Exit loop, try to find audio data
        }
        
        // Process block based on type
        // RFC 9639 Section 8.1: Block types 0-6 are defined, 7-126 are reserved, 127 is forbidden
        uint8_t raw_type = static_cast<uint8_t>(block.type);
        
        // Requirement 3.1, 3.2: STREAMINFO must be the first metadata block
        // RFC 9639 Section 8.2: "The STREAMINFO block MUST be the first metadata block"
        if (is_first_block) {
            if (block.type != FLACMetadataType::STREAMINFO) {
                FLAC_DEBUG("First metadata block is not STREAMINFO (type=", 
                           static_cast<int>(raw_type), ")");
                // Requirement 24.2: Don't reject immediately, try to continue
                // We'll derive parameters from frame headers later if needed
                FLAC_DEBUG("[parseMetadataBlocks] Requirement 24.2: Continuing despite missing STREAMINFO");
                skipMetadataBlock_unlocked(block);
                is_first_block = false;
                continue;
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
        
        // Requirement 24.2: Wrap block parsing in try-catch for robustness
        bool block_parsed_ok = true;
        
        switch (block.type) {
            case FLACMetadataType::STREAMINFO:
                if (found_streaminfo) {
                    // Requirement 24.2: Skip duplicate STREAMINFO instead of failing
                    FLAC_DEBUG("[parseMetadataBlocks] Requirement 24.2: Skipping duplicate STREAMINFO");
                    skipMetadataBlock_unlocked(block);
                    block_parsed_ok = true;
                } else {
                    block_parsed_ok = parseStreamInfoBlock_unlocked(block);
                    if (block_parsed_ok) {
                        found_streaminfo = true;
                    } else {
                        // Requirement 24.2: STREAMINFO parsing failed, skip and continue
                        FLAC_DEBUG("[parseMetadataBlocks] Requirement 24.2: STREAMINFO parsing failed, skipping");
                        skipMetadataBlock_unlocked(block);
                        corrupted_blocks_skipped++;
                    }
                }
                break;
                
            case FLACMetadataType::SEEKTABLE:
                // Non-critical block - failure is not fatal
                if (!parseSeekTableBlock_unlocked(block)) {
                    FLAC_DEBUG("[parseMetadataBlocks] SEEKTABLE parsing failed, skipping");
                    skipMetadataBlock_unlocked(block);
                }
                break;
                
            case FLACMetadataType::VORBIS_COMMENT:
                // Non-critical block - failure is not fatal
                if (!parseVorbisCommentBlock_unlocked(block)) {
                    FLAC_DEBUG("[parseMetadataBlocks] VORBIS_COMMENT parsing failed, skipping");
                    skipMetadataBlock_unlocked(block);
                }
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
                // Non-critical block - failure is not fatal
                if (!parseCuesheetBlock_unlocked(block)) {
                    FLAC_DEBUG("[parseMetadataBlocks] CUESHEET parsing failed, skipping");
                    skipMetadataBlock_unlocked(block);
                }
                break;
                
            case FLACMetadataType::PICTURE:
                // Requirement 17.1-17.12: Handle PICTURE blocks per RFC 9639 Section 8.8
                // Non-critical block - failure is not fatal
                if (!parsePictureBlock_unlocked(block)) {
                    FLAC_DEBUG("[parseMetadataBlocks] PICTURE parsing failed, skipping");
                    skipMetadataBlock_unlocked(block);
                }
                break;
                
            default:
                // Should not reach here due to reserved type check above
                FLAC_DEBUG("Unexpected metadata block type ", static_cast<int>(raw_type));
                skipMetadataBlock_unlocked(block);
                break;
        }
        
        is_first_block = false;  // After processing any block, it's no longer the first
    }
    
    // Log if we skipped any corrupted blocks
    if (corrupted_blocks_skipped > 0) {
        FLAC_DEBUG("[parseMetadataBlocks] Requirement 24.2: Skipped ", corrupted_blocks_skipped, " corrupted metadata blocks");
    }
    
    // RFC 9639: STREAMINFO should be first metadata block
    // But per Requirement 24.3, we can derive parameters from frame headers if missing
    if (!found_streaminfo) {
        FLAC_DEBUG("[parseMetadataBlocks] STREAMINFO not found, will attempt frame header derivation");
        // Don't return false here - let parseContainer_unlocked handle the fallback
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
// Streamable Subset Validation (RFC 9639 Section 7)
// ============================================================================

void FLACDemuxer::validateStreamableSubset_unlocked()
{
    FLAC_DEBUG("[validateStreamableSubset] Validating streamable subset compliance (RFC 9639 Section 7)");
    
    // Start assuming stream is streamable
    m_is_streamable_subset = true;
    
    // Requirement 20.3: Mark non-streamable if max block size exceeds 16384
    // RFC 9639 Section 7: Streamable subset requires max block size <= 16384
    if (m_streaminfo.max_block_size > 16384) {
        FLAC_DEBUG("[validateStreamableSubset] Requirement 20.3: max_block_size (", 
                   m_streaminfo.max_block_size, ") > 16384 - marking as non-streamable");
        m_is_streamable_subset = false;
    }
    
    // Requirement 20.4: Mark non-streamable if sample rate 48kHz or less and block size exceeds 4608
    // RFC 9639 Section 7: For sample rates <= 48000 Hz, block size must be <= 4608
    if (m_streaminfo.sample_rate > 0 && m_streaminfo.sample_rate <= 48000) {
        if (m_streaminfo.max_block_size > 4608) {
            FLAC_DEBUG("[validateStreamableSubset] Requirement 20.4: sample_rate (", 
                       m_streaminfo.sample_rate, " Hz) <= 48000 and max_block_size (", 
                       m_streaminfo.max_block_size, ") > 4608 - marking as non-streamable");
            m_is_streamable_subset = false;
        }
    }
    
    // Requirement 20.5: Mark non-streamable if WAVEFORMATEXTENSIBLE_CHANNEL_MASK present
    // RFC 9639 Section 7: WAVEFORMATEXTENSIBLE_CHANNEL_MASK indicates non-default channel ordering
    auto it = m_vorbis_comments.find("WAVEFORMATEXTENSIBLE_CHANNEL_MASK");
    if (it != m_vorbis_comments.end()) {
        FLAC_DEBUG("[validateStreamableSubset] Requirement 20.5: WAVEFORMATEXTENSIBLE_CHANNEL_MASK present ('", 
                   it->second, "') - marking as non-streamable");
        m_is_streamable_subset = false;
    }
    
    // Note: Requirements 20.1 and 20.2 (sample rate bits 0b0000 and bit depth bits 0b000)
    // are checked during frame header parsing in parseFrameHeader_unlocked().
    // These indicate that the frame relies on STREAMINFO for sample rate/bit depth,
    // which violates the streamable subset requirement.
    // The m_is_streamable_subset flag may be updated during frame parsing if these
    // patterns are encountered.
    
    if (m_is_streamable_subset) {
        FLAC_DEBUG("[validateStreamableSubset] Stream conforms to streamable subset");
    } else {
        FLAC_DEBUG("[validateStreamableSubset] Stream does NOT conform to streamable subset");
        FLAC_DEBUG("[validateStreamableSubset] Note: Non-streamable streams are still playable, "
                   "but may not be suitable for streaming without seeking capability");
    }
}

// ============================================================================
// Tag Creation from Parsed Metadata (Requirements 8.2, 8.4)
// ============================================================================

void FLACDemuxer::createTagFromMetadata_unlocked()
{
    FLAC_DEBUG("[createTagFromMetadata] Creating VorbisCommentTag from parsed metadata");
    
    // Check if we have any metadata to create a tag from
    if (m_vorbis_comments.empty() && m_pictures.empty()) {
        FLAC_DEBUG("[createTagFromMetadata] No VorbisComment metadata or pictures found, m_tag remains null");
        return;
    }
    
    // Convert m_vorbis_comments (single-valued) to multi-valued format for VorbisCommentTag
    std::map<std::string, std::vector<std::string>> fields;
    for (const auto& [key, value] : m_vorbis_comments) {
        fields[key].push_back(value);
    }
    
    // Convert FLACPicture to Tag::Picture
    std::vector<PsyMP3::Tag::Picture> pictures;
    pictures.reserve(m_pictures.size());
    
    for (const auto& flac_pic : m_pictures) {
        PsyMP3::Tag::Picture pic;
        pic.type = static_cast<PsyMP3::Tag::PictureType>(flac_pic.type);
        pic.mime_type = flac_pic.media_type;
        pic.description = flac_pic.description;
        pic.width = flac_pic.width;
        pic.height = flac_pic.height;
        pic.color_depth = flac_pic.color_depth;
        pic.colors_used = flac_pic.indexed_colors;
        pic.data = flac_pic.data;
        pictures.push_back(std::move(pic));
    }
    
    // Create the VorbisCommentTag
    // Note: FLAC doesn't store a vendor string in the same way as Ogg VorbisComment,
    // so we use a default vendor string
    std::string vendor = "PsyMP3 FLAC Demuxer";
    
    m_tag = std::make_unique<PsyMP3::Tag::VorbisCommentTag>(vendor, fields, pictures);
    
    FLAC_DEBUG("[createTagFromMetadata] Created VorbisCommentTag with ", fields.size(), 
               " fields and ", pictures.size(), " pictures");
    
    // Log some common metadata fields if present
    if (m_tag) {
        if (!m_tag->title().empty()) {
            FLAC_DEBUG("[createTagFromMetadata] Title: ", m_tag->title());
        }
        if (!m_tag->artist().empty()) {
            FLAC_DEBUG("[createTagFromMetadata] Artist: ", m_tag->artist());
        }
        if (!m_tag->album().empty()) {
            FLAC_DEBUG("[createTagFromMetadata] Album: ", m_tag->album());
        }
    }
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
    
    // ========================================================================
    // Parse coded number (RFC 9639 Section 9.1.5)
    // Requirements 9.1-9.10 - Determine header length for CRC validation
    // ========================================================================
    
    // The coded number starts at byte 4 (after sync code and header bytes)
    size_t header_offset = 4;  // Position after sync(2) + header(2)
    
    // Parse the coded number to determine its length
    uint64_t coded_number = 0;
    size_t coded_number_bytes = 0;
    
    if (header_offset < size) {
        if (!parseCodedNumber_unlocked(&buffer[header_offset], size - header_offset,
                                       coded_number_bytes, coded_number, frame.variable_block_size)) {
            FLAC_DEBUG("[parseFrameHeader] Failed to parse coded number");
            // Continue anyway - we can still use the frame with estimated values
        } else {
            header_offset += coded_number_bytes;
            FLAC_DEBUG("[parseFrameHeader] Coded number: ", coded_number, 
                       " (", coded_number_bytes, " bytes)");
        }
    }
    
    // ========================================================================
    // Parse uncommon block size if needed (RFC 9639 Section 9.1.1)
    // ========================================================================
    
    if (block_size_bits == 0x06 && header_offset < size) {
        // 8-bit uncommon block size
        frame.block_size = static_cast<uint32_t>(buffer[header_offset]) + 1;
        header_offset += 1;
        FLAC_DEBUG("[parseFrameHeader] Uncommon block size (8-bit): ", frame.block_size);
    } else if (block_size_bits == 0x07 && header_offset + 1 < size) {
        // 16-bit uncommon block size (big-endian)
        uint32_t uncommon_value = (static_cast<uint32_t>(buffer[header_offset]) << 8) |
                                   static_cast<uint32_t>(buffer[header_offset + 1]);
        frame.block_size = uncommon_value + 1;
        header_offset += 2;
        FLAC_DEBUG("[parseFrameHeader] Uncommon block size (16-bit): ", frame.block_size);
        
        // Requirement 5.18: Reject forbidden uncommon block size 65536
        if (frame.block_size == 65536) {
            FLAC_DEBUG("[parseFrameHeader] REJECTED: Forbidden uncommon block size 65536");
            return false;
        }
    }
    
    // ========================================================================
    // Parse uncommon sample rate if needed (RFC 9639 Section 9.1.2)
    // ========================================================================
    
    if (sample_rate_bits == 0x0C && header_offset < size) {
        // 8-bit uncommon sample rate in kHz
        frame.sample_rate = static_cast<uint32_t>(buffer[header_offset]) * 1000;
        header_offset += 1;
        FLAC_DEBUG("[parseFrameHeader] Uncommon sample rate (8-bit kHz): ", frame.sample_rate, " Hz");
    } else if (sample_rate_bits == 0x0D && header_offset + 1 < size) {
        // 16-bit uncommon sample rate in Hz (big-endian)
        frame.sample_rate = (static_cast<uint32_t>(buffer[header_offset]) << 8) |
                             static_cast<uint32_t>(buffer[header_offset + 1]);
        header_offset += 2;
        FLAC_DEBUG("[parseFrameHeader] Uncommon sample rate (16-bit Hz): ", frame.sample_rate, " Hz");
    } else if (sample_rate_bits == 0x0E && header_offset + 1 < size) {
        // 16-bit uncommon sample rate in tens of Hz (big-endian)
        uint32_t tens_of_hz = (static_cast<uint32_t>(buffer[header_offset]) << 8) |
                               static_cast<uint32_t>(buffer[header_offset + 1]);
        frame.sample_rate = tens_of_hz * 10;
        header_offset += 2;
        FLAC_DEBUG("[parseFrameHeader] Uncommon sample rate (16-bit x10 Hz): ", frame.sample_rate, " Hz");
    }
    
    // ========================================================================
    // CRC-8 Validation (RFC 9639 Section 9.1.8)
    // Requirements 10.1-10.6
    // ========================================================================
    
    // The CRC-8 byte is at header_offset (after all header fields)
    if (header_offset < size) {
        // Total header length including CRC-8 byte
        size_t total_header_length = header_offset + 1;
        
        // Requirement 10.4: Validate CRC-8 after parsing header
        // Requirement 2.3 (bisection seeking): Validate CRC-8 checksum per RFC 9639 Section 9.1.8
        // Requirement 2.8 (bisection seeking): Continue searching past false positive sync patterns (CRC failures)
        if (!validateFrameHeaderCRC_unlocked(buffer, total_header_length, frame.file_offset)) {
            // Requirement 10.5: Log CRC mismatches with frame position (done in validateFrameHeaderCRC_unlocked)
            // Requirement 10.6: Attempt resynchronization on failure
            FLAC_DEBUG("[parseFrameHeader] CRC-8 validation failed - rejecting frame as false sync");
            // CRC-8 failure means this is likely a false sync code in the audio data
            // Return false to continue searching for a valid frame
            return false;
        }
    } else {
        FLAC_DEBUG("[parseFrameHeader] Insufficient buffer for CRC-8 validation");
        // Cannot validate CRC-8, reject this potential frame
        return false;
    }
    
    // Calculate sample offset from coded number
    // Requirement 9.9: For fixed block size, coded number is frame number
    // Requirement 9.10: For variable block size, coded number is sample number
    if (coded_number_bytes > 0) {
        if (frame.variable_block_size) {
            // Variable block size: coded number is the sample number directly
            frame.sample_offset = coded_number;
            FLAC_DEBUG("[parseFrameHeader] Sample offset from coded number (variable): ", frame.sample_offset);
        } else {
            // Fixed block size: coded number is frame number, multiply by block size
            frame.sample_offset = coded_number * frame.block_size;
            FLAC_DEBUG("[parseFrameHeader] Sample offset from coded number (fixed): frame ", 
                       coded_number, " * ", frame.block_size, " = ", frame.sample_offset);
        }
    } else {
        // Fallback to current position if coded number parsing failed
        frame.sample_offset = m_current_sample;
        FLAC_DEBUG("[parseFrameHeader] Sample offset from current position (fallback): ", frame.sample_offset);
    }
    
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
        // Requirement 20.1: Mark stream as non-streamable if sample rate bits equal 0b0000
        case 0x00:
            sample_rate = m_streaminfo.sample_rate;
            FLAC_DEBUG("[parseSampleRateBits] Sample rate from STREAMINFO: ", sample_rate, " Hz (0b0000)");
            // Requirement 20.1: This violates streamable subset
            if (m_is_streamable_subset) {
                FLAC_DEBUG("[parseSampleRateBits] Requirement 20.1: sample rate bits 0b0000 - marking as non-streamable");
                m_is_streamable_subset = false;
            }
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
        // Requirement 20.2: Mark stream as non-streamable if bit depth bits equal 0b000
        case 0x00:
            bit_depth = m_streaminfo.bits_per_sample;
            FLAC_DEBUG("[parseBitDepthBits] Bit depth from STREAMINFO: ", 
                       static_cast<int>(bit_depth), " bits (0b000)");
            // Requirement 20.2: This violates streamable subset
            if (m_is_streamable_subset) {
                FLAC_DEBUG("[parseBitDepthBits] Requirement 20.2: bit depth bits 0b000 - marking as non-streamable");
                m_is_streamable_subset = false;
            }
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
// Frame Size Estimation (RFC 9639 compliant)
// ============================================================================

/**
 * @brief Calculate estimated frame size using STREAMINFO metadata
 * 
 * Implements Requirements 21.1, 21.2, 21.5, 25.1, 25.4:
 * - Requirement 21.1: Use STREAMINFO minimum frame size as primary estimate
 * - Requirement 21.2: For fixed block size streams, use minimum directly without scaling
 * - Requirement 21.5: Handle highly compressed streams with frames as small as 14 bytes
 * - Requirement 25.1: Avoid complex theoretical calculations that produce inaccurate estimates
 * - Requirement 25.4: Prioritize minimum frame size over complex scaling algorithms
 * 
 * Critical Design Insight (from real-world debugging):
 * - STREAMINFO minimum frame size is the most accurate estimate for highly compressed streams
 * - Fixed block size streams should use minimum frame size directly without scaling
 * - Complex theoretical calculations often produce inaccurate estimates (e.g., 6942 bytes vs actual 14 bytes)
 * - Conservative fallbacks prevent excessive I/O when estimation fails
 * 
 * @param frame The frame structure with parsed header information
 * @return Estimated frame size in bytes
 */
uint32_t FLACDemuxer::calculateFrameSize_unlocked(const FLACFrame& frame) const
{
    FLAC_DEBUG("[calculateFrameSize] Estimating frame size for frame at offset ", frame.file_offset);
    
    // ========================================================================
    // Method 1: Use STREAMINFO minimum frame size (preferred)
    // Requirement 21.1: Use STREAMINFO minimum frame size as primary estimate
    // Requirement 25.4: Prioritize minimum frame size over complex scaling algorithms
    // ========================================================================
    
    if (m_streaminfo.isValid() && m_streaminfo.min_frame_size > 0) {
        uint32_t estimated_size = m_streaminfo.min_frame_size;
        
        FLAC_DEBUG("[calculateFrameSize] STREAMINFO min_frame_size: ", m_streaminfo.min_frame_size, " bytes");
        FLAC_DEBUG("[calculateFrameSize] STREAMINFO max_frame_size: ", m_streaminfo.max_frame_size, " bytes");
        FLAC_DEBUG("[calculateFrameSize] STREAMINFO min_block_size: ", m_streaminfo.min_block_size, " samples");
        FLAC_DEBUG("[calculateFrameSize] STREAMINFO max_block_size: ", m_streaminfo.max_block_size, " samples");
        
        // ====================================================================
        // Requirement 21.2: For fixed block size streams, use MAXIMUM frame size
        // Fixed block size streams have min_block_size == max_block_size, but
        // individual FRAME sizes can still vary based on compression efficiency.
        // Using max_frame_size ensures we always read complete frame data.
        // ====================================================================
        
        if (m_streaminfo.min_block_size == m_streaminfo.max_block_size) {
            // Fixed block size stream - use maximum frame size to avoid truncation
            // Even though block size is fixed, frame size can vary due to compression
            estimated_size = m_streaminfo.max_frame_size > 0 ? m_streaminfo.max_frame_size : m_streaminfo.min_frame_size;
            FLAC_DEBUG("[calculateFrameSize] Fixed block size detected (", m_streaminfo.min_block_size, 
                       " samples), using maximum frame size for safety: ", estimated_size, " bytes");
            
            // Requirement 21.5: Handle highly compressed streams with frames as small as 14 bytes
            // The minimum valid FLAC frame is approximately:
            // - Sync code: 2 bytes
            // - Frame header: 4-7 bytes (varies with coded number length)
            // - CRC-8: 1 byte
            // - Minimal subframe: ~2 bytes per channel
            // - CRC-16: 2 bytes
            // Total minimum: ~14 bytes for mono, ~16 bytes for stereo
            
            if (estimated_size < 14) {
                FLAC_DEBUG("[calculateFrameSize] WARNING: min_frame_size (", estimated_size, 
                           ") is below theoretical minimum (14 bytes), using 14 bytes");
                estimated_size = 14;
            }
            
            return estimated_size;
        }
        
        // ====================================================================
        // Variable block size stream - use conservative estimate
        // Requirement 25.1: Avoid complex theoretical calculations
        // ====================================================================
        
        FLAC_DEBUG("[calculateFrameSize] Variable block size stream detected");
        FLAC_DEBUG("[calculateFrameSize] Frame block_size: ", frame.block_size, " samples");
        
        // For variable block size, we use a simple approach:
        // - If the frame's block size matches min_block_size, use min_frame_size
        // - If the frame's block size matches max_block_size, use max_frame_size
        // - Otherwise, use a linear interpolation between min and max
        
        if (frame.block_size > 0 && m_streaminfo.max_frame_size > 0) {
            if (frame.block_size <= m_streaminfo.min_block_size) {
                // Frame is at or below minimum block size - use minimum frame size
                FLAC_DEBUG("[calculateFrameSize] Frame block_size <= min_block_size, using min_frame_size: ", 
                           m_streaminfo.min_frame_size, " bytes");
                return m_streaminfo.min_frame_size;
            }
            
            if (frame.block_size >= m_streaminfo.max_block_size) {
                // Frame is at or above maximum block size - use maximum frame size
                FLAC_DEBUG("[calculateFrameSize] Frame block_size >= max_block_size, using max_frame_size: ", 
                           m_streaminfo.max_frame_size, " bytes");
                return m_streaminfo.max_frame_size;
            }
            
            // Linear interpolation between min and max frame sizes
            // This is a simple, conservative approach that avoids complex calculations
            uint32_t block_range = m_streaminfo.max_block_size - m_streaminfo.min_block_size;
            uint32_t frame_range = m_streaminfo.max_frame_size - m_streaminfo.min_frame_size;
            
            if (block_range > 0) {
                uint32_t block_offset = frame.block_size - m_streaminfo.min_block_size;
                uint32_t frame_offset_estimate = (block_offset * frame_range) / block_range;
                estimated_size = m_streaminfo.min_frame_size + frame_offset_estimate;
                
                FLAC_DEBUG("[calculateFrameSize] Linear interpolation: ", estimated_size, " bytes");
                FLAC_DEBUG("[calculateFrameSize]   block_offset: ", block_offset, 
                           ", frame_range: ", frame_range, ", block_range: ", block_range);
            }
        }
        
        // Ensure minimum valid frame size
        if (estimated_size < 14) {
            FLAC_DEBUG("[calculateFrameSize] Estimated size (", estimated_size, 
                       ") below minimum, using 14 bytes");
            estimated_size = 14;
        }
        
        FLAC_DEBUG("[calculateFrameSize] Final estimate from STREAMINFO: ", estimated_size, " bytes");
        return estimated_size;
    }
    
    // ========================================================================
    // Method 2: STREAMINFO not available or min_frame_size is 0 (unknown)
    // Use conservative fallback based on frame parameters
    // ========================================================================
    
    FLAC_DEBUG("[calculateFrameSize] STREAMINFO min_frame_size unavailable, using fallback estimation");
    
    // Conservative fallback calculation:
    // Frame overhead: sync(2) + header(4-7) + CRC-8(1) + CRC-16(2) = ~12 bytes
    // Audio data: block_size * channels * (bits_per_sample / 8) * compression_ratio
    // 
    // For highly compressed FLAC, compression ratio can be as low as 0.1 (10%)
    // For typical FLAC, compression ratio is around 0.5-0.7 (50-70%)
    // 
    // We use a conservative estimate assuming moderate compression
    
    uint32_t frame_overhead = 16;  // Conservative overhead estimate
    uint32_t audio_data_estimate = 0;
    
    if (frame.block_size > 0 && frame.channels > 0 && frame.bits_per_sample > 0) {
        // Calculate uncompressed size
        uint32_t uncompressed_size = frame.block_size * frame.channels * (frame.bits_per_sample / 8);
        
        // Assume 50% compression ratio as a conservative estimate
        // This may overestimate for highly compressed streams, but that's safer
        // than underestimating and missing frame data
        audio_data_estimate = uncompressed_size / 2;
        
        FLAC_DEBUG("[calculateFrameSize] Fallback calculation:");
        FLAC_DEBUG("[calculateFrameSize]   block_size: ", frame.block_size);
        FLAC_DEBUG("[calculateFrameSize]   channels: ", static_cast<int>(frame.channels));
        FLAC_DEBUG("[calculateFrameSize]   bits_per_sample: ", static_cast<int>(frame.bits_per_sample));
        FLAC_DEBUG("[calculateFrameSize]   uncompressed_size: ", uncompressed_size, " bytes");
        FLAC_DEBUG("[calculateFrameSize]   audio_data_estimate (50% compression): ", audio_data_estimate, " bytes");
    } else {
        // No frame parameters available - use a very conservative default
        // This handles the case where we're searching for the first frame
        audio_data_estimate = 4096;  // Conservative default
        FLAC_DEBUG("[calculateFrameSize] No frame parameters, using default audio_data_estimate: ", 
                   audio_data_estimate, " bytes");
    }
    
    uint32_t fallback_estimate = frame_overhead + audio_data_estimate;
    
    // Ensure minimum valid frame size
    if (fallback_estimate < 14) {
        fallback_estimate = 14;
    }
    
    // Cap at reasonable maximum to prevent excessive reads
    // FLAC frames are typically under 64KB even for high-resolution audio
    static constexpr uint32_t MAX_REASONABLE_FRAME_SIZE = 65536;
    if (fallback_estimate > MAX_REASONABLE_FRAME_SIZE) {
        FLAC_DEBUG("[calculateFrameSize] Fallback estimate (", fallback_estimate, 
                   ") exceeds maximum, capping at ", MAX_REASONABLE_FRAME_SIZE, " bytes");
        fallback_estimate = MAX_REASONABLE_FRAME_SIZE;
    }
    
    FLAC_DEBUG("[calculateFrameSize] Final fallback estimate: ", fallback_estimate, " bytes");
    return fallback_estimate;
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


// ============================================================================
// CRC-8 Validation (RFC 9639 Section 9.1.8)
// ============================================================================

/**
 * CRC-8 lookup table for frame header validation
 * 
 * RFC 9639 Section 9.1.8 specifies:
 * - Polynomial: x^8 + x^2 + x + 1 (0x07)
 * - Initial value: 0
 * - No final XOR
 * 
 * This table is pre-computed for the polynomial 0x07 to enable fast
 * byte-at-a-time CRC calculation.
 */
const uint8_t FLACDemuxer::s_crc8_table[256] = {
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
    0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
    0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
    0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
    0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
    0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
    0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
    0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
    0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
    0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
    0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
    0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
    0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
    0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
    0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
    0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
    0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
    0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
    0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
    0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
    0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
    0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
    0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
    0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
    0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
    0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
    0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

/**
 * @brief Calculate CRC-8 checksum for frame header per RFC 9639 Section 9.1.8
 * 
 * Implements Requirements 10.1-10.3:
 * - Requirement 10.1: Use polynomial 0x07 (x^8 + x^2 + x + 1)
 * - Requirement 10.2: Initialize CRC to 0
 * - Requirement 10.3: Cover all frame header bytes except CRC itself
 * 
 * The CRC-8 is calculated over all frame header bytes from the sync code
 * up to but not including the CRC-8 byte itself.
 * 
 * @param data Pointer to frame header data (starting from sync code)
 * @param length Number of bytes to include in CRC calculation (excluding CRC byte)
 * @return The calculated 8-bit CRC value
 */
uint8_t FLACDemuxer::calculateCRC8(const uint8_t* data, size_t length)
{
    // Requirement 10.2: Initialize CRC to 0
    uint8_t crc = 0;
    
    // Requirement 10.1: Use polynomial 0x07 via lookup table
    // Requirement 10.3: Cover all frame header bytes except CRC itself
    for (size_t i = 0; i < length; ++i) {
        crc = s_crc8_table[crc ^ data[i]];
    }
    
    return crc;
}

/**
 * @brief Validate frame header CRC-8 per RFC 9639 Section 9.1.8
 * 
 * Implements Requirements 10.4-10.6:
 * - Requirement 10.4: Validate CRC-8 after parsing header
 * - Requirement 10.5: Log CRC mismatches with frame position
 * - Requirement 10.6: Support strict mode rejection
 * 
 * The frame header CRC-8 is the last byte of the frame header, calculated
 * over all preceding header bytes starting from the sync code.
 * 
 * @param header_data Pointer to complete frame header data (including CRC byte)
 * @param header_length Total length of frame header including CRC byte
 * @param frame_offset File offset of the frame (for logging)
 * @return true if CRC is valid, false if CRC fails
 */
bool FLACDemuxer::validateFrameHeaderCRC_unlocked(const uint8_t* header_data, size_t header_length,
                                                   uint64_t frame_offset)
{
    FLAC_DEBUG("[validateFrameHeaderCRC] Validating CRC-8 for frame at offset ", frame_offset);
    
    // Need at least sync code (2 bytes) + minimal header (2 bytes) + CRC (1 byte) = 5 bytes
    if (header_length < 5) {
        FLAC_DEBUG("[validateFrameHeaderCRC] Header too short for CRC validation: ", 
                   header_length, " bytes");
        return false;
    }
    
    // The CRC-8 byte is the last byte of the frame header
    // Calculate CRC over all bytes except the CRC byte itself
    size_t crc_data_length = header_length - 1;
    uint8_t expected_crc = header_data[header_length - 1];
    
    // Requirement 10.4: Validate CRC-8 after parsing header
    uint8_t calculated_crc = calculateCRC8(header_data, crc_data_length);
    
    if (calculated_crc != expected_crc) {
        // Requirement 10.5: Log CRC mismatches with frame position
        FLAC_DEBUG("[validateFrameHeaderCRC] CRC-8 MISMATCH at frame offset ", frame_offset);
        FLAC_DEBUG("[validateFrameHeaderCRC]   Expected CRC: 0x", std::hex, 
                   static_cast<int>(expected_crc), std::dec);
        FLAC_DEBUG("[validateFrameHeaderCRC]   Calculated CRC: 0x", std::hex, 
                   static_cast<int>(calculated_crc), std::dec);
        FLAC_DEBUG("[validateFrameHeaderCRC]   Header length: ", header_length, " bytes");
        
        // Log first few bytes of header for debugging
        if (header_length >= 4) {
            FLAC_DEBUG("[validateFrameHeaderCRC]   Header bytes: 0x", std::hex,
                       static_cast<int>(header_data[0]), " 0x",
                       static_cast<int>(header_data[1]), " 0x",
                       static_cast<int>(header_data[2]), " 0x",
                       static_cast<int>(header_data[3]), std::dec, " ...");
        }
        
        // Requirement 10.6: In strict mode, reject stream on CRC failure
        // For now, we return false to indicate CRC failure
        // The caller can decide whether to attempt resynchronization
        return false;
    }
    
    FLAC_DEBUG("[validateFrameHeaderCRC] CRC-8 valid: 0x", std::hex, 
               static_cast<int>(calculated_crc), std::dec);
    return true;
}

// ============================================================================
// CRC-16 Implementation (RFC 9639 Section 9.3)
// ============================================================================

/**
 * @brief CRC-16 lookup table for polynomial 0x8005 (x^16 + x^15 + x^2 + x^0)
 * 
 * Per RFC 9639 Section 9.3:
 * - Polynomial: x^16 + x^15 + x^2 + x^0 = 0x8005
 * - Initial value: 0
 * - No final XOR
 * 
 * This table is pre-computed for the polynomial 0x8005 to enable fast
 * byte-at-a-time CRC calculation.
 * 
 * Note: The CRC-16 used by FLAC is a bit-reversed version of the standard
 * CRC-16-IBM. The polynomial 0x8005 when processed MSB-first produces
 * this lookup table.
 */
const uint16_t FLACDemuxer::s_crc16_table[256] = {
    0x0000, 0x8005, 0x800F, 0x000A, 0x801B, 0x001E, 0x0014, 0x8011,
    0x8033, 0x0036, 0x003C, 0x8039, 0x0028, 0x802D, 0x8027, 0x0022,
    0x8063, 0x0066, 0x006C, 0x8069, 0x0078, 0x807D, 0x8077, 0x0072,
    0x0050, 0x8055, 0x805F, 0x005A, 0x804B, 0x004E, 0x0044, 0x8041,
    0x80C3, 0x00C6, 0x00CC, 0x80C9, 0x00D8, 0x80DD, 0x80D7, 0x00D2,
    0x00F0, 0x80F5, 0x80FF, 0x00FA, 0x80EB, 0x00EE, 0x00E4, 0x80E1,
    0x00A0, 0x80A5, 0x80AF, 0x00AA, 0x80BB, 0x00BE, 0x00B4, 0x80B1,
    0x8093, 0x0096, 0x009C, 0x8099, 0x0088, 0x808D, 0x8087, 0x0082,
    0x8183, 0x0186, 0x018C, 0x8189, 0x0198, 0x819D, 0x8197, 0x0192,
    0x01B0, 0x81B5, 0x81BF, 0x01BA, 0x81AB, 0x01AE, 0x01A4, 0x81A1,
    0x01E0, 0x81E5, 0x81EF, 0x01EA, 0x81FB, 0x01FE, 0x01F4, 0x81F1,
    0x81D3, 0x01D6, 0x01DC, 0x81D9, 0x01C8, 0x81CD, 0x81C7, 0x01C2,
    0x0140, 0x8145, 0x814F, 0x014A, 0x815B, 0x015E, 0x0154, 0x8151,
    0x8173, 0x0176, 0x017C, 0x8179, 0x0168, 0x816D, 0x8167, 0x0162,
    0x8123, 0x0126, 0x012C, 0x8129, 0x0138, 0x813D, 0x8137, 0x0132,
    0x0110, 0x8115, 0x811F, 0x011A, 0x810B, 0x010E, 0x0104, 0x8101,
    0x8303, 0x0306, 0x030C, 0x8309, 0x0318, 0x831D, 0x8317, 0x0312,
    0x0330, 0x8335, 0x833F, 0x033A, 0x832B, 0x032E, 0x0324, 0x8321,
    0x0360, 0x8365, 0x836F, 0x036A, 0x837B, 0x037E, 0x0374, 0x8371,
    0x8353, 0x0356, 0x035C, 0x8359, 0x0348, 0x834D, 0x8347, 0x0342,
    0x03C0, 0x83C5, 0x83CF, 0x03CA, 0x83DB, 0x03DE, 0x03D4, 0x83D1,
    0x83F3, 0x03F6, 0x03FC, 0x83F9, 0x03E8, 0x83ED, 0x83E7, 0x03E2,
    0x83A3, 0x03A6, 0x03AC, 0x83A9, 0x03B8, 0x83BD, 0x83B7, 0x03B2,
    0x0390, 0x8395, 0x839F, 0x039A, 0x838B, 0x038E, 0x0384, 0x8381,
    0x0280, 0x8285, 0x828F, 0x028A, 0x829B, 0x029E, 0x0294, 0x8291,
    0x82B3, 0x02B6, 0x02BC, 0x82B9, 0x02A8, 0x82AD, 0x82A7, 0x02A2,
    0x82E3, 0x02E6, 0x02EC, 0x82E9, 0x02F8, 0x82FD, 0x82F7, 0x02F2,
    0x02D0, 0x82D5, 0x82DF, 0x02DA, 0x82CB, 0x02CE, 0x02C4, 0x82C1,
    0x8243, 0x0246, 0x024C, 0x8249, 0x0258, 0x825D, 0x8257, 0x0252,
    0x0270, 0x8275, 0x827F, 0x027A, 0x826B, 0x026E, 0x0264, 0x8261,
    0x0220, 0x8225, 0x822F, 0x022A, 0x823B, 0x023E, 0x0234, 0x8231,
    0x8213, 0x0216, 0x021C, 0x8219, 0x0208, 0x820D, 0x8207, 0x0202
};

/**
 * @brief Calculate CRC-16 checksum for frame data per RFC 9639 Section 9.3
 * 
 * Implements Requirements 11.2-11.5:
 * - Requirement 11.2: Use polynomial 0x8005 (x^16 + x^15 + x^2 + x^0)
 * - Requirement 11.3: Initialize CRC to 0
 * - Requirement 11.4: Cover entire frame from sync code to end of subframes
 * - Requirement 11.5: Exclude the 16-bit CRC itself from calculation
 * 
 * The CRC-16 is calculated over all frame bytes from the sync code
 * up to but not including the CRC-16 bytes themselves.
 * 
 * @param data Pointer to frame data (starting from sync code)
 * @param length Number of bytes to include in CRC calculation (excluding CRC bytes)
 * @return The calculated 16-bit CRC value
 */
uint16_t FLACDemuxer::calculateCRC16(const uint8_t* data, size_t length)
{
    // Requirement 11.3: Initialize CRC to 0
    uint16_t crc = 0;
    
    // Requirement 11.2: Use polynomial 0x8005 via lookup table
    // Requirement 11.4: Cover entire frame from sync code to end of subframes
    for (size_t i = 0; i < length; ++i) {
        crc = (crc << 8) ^ s_crc16_table[(crc >> 8) ^ data[i]];
    }
    
    return crc;
}

/**
 * @brief Validate frame footer CRC-16 per RFC 9639 Section 9.3
 * 
 * Implements Requirements 11.1, 11.6-11.8:
 * - Requirement 11.1: Ensure byte alignment with zero padding
 * - Requirement 11.6: Read 16-bit CRC from footer (big-endian)
 * - Requirement 11.7: Log CRC mismatches and attempt to continue
 * - Requirement 11.8: Support strict mode rejection
 * 
 * The frame footer CRC-16 is the last 2 bytes of the frame, calculated
 * over all preceding frame bytes starting from the sync code.
 * 
 * @param frame_data Pointer to complete frame data (including CRC bytes)
 * @param frame_length Total length of frame including CRC bytes
 * @param frame_offset File offset of the frame (for logging)
 * @return true if CRC is valid, false if CRC fails
 */
bool FLACDemuxer::validateFrameFooterCRC_unlocked(const uint8_t* frame_data, size_t frame_length,
                                                   uint64_t frame_offset)
{
    FLAC_DEBUG("[validateFrameFooterCRC] Validating CRC-16 for frame at offset ", frame_offset);
    
    // Need at least sync code (2 bytes) + minimal header (3 bytes) + CRC-8 (1 byte) + CRC-16 (2 bytes) = 8 bytes
    if (frame_length < 8) {
        FLAC_DEBUG("[validateFrameFooterCRC] Frame too short for CRC-16 validation: ", 
                   frame_length, " bytes");
        return false;
    }
    
    // Requirement 11.6: Read 16-bit CRC from footer (big-endian)
    // The CRC-16 is the last 2 bytes of the frame
    size_t crc_data_length = frame_length - 2;
    uint16_t expected_crc = (static_cast<uint16_t>(frame_data[frame_length - 2]) << 8) |
                            static_cast<uint16_t>(frame_data[frame_length - 1]);
    
    // Requirement 11.4, 11.5: Calculate CRC over all bytes except the CRC itself
    uint16_t calculated_crc = calculateCRC16(frame_data, crc_data_length);
    
    if (calculated_crc != expected_crc) {
        // Requirement 11.7: Log CRC mismatches
        FLAC_DEBUG("[validateFrameFooterCRC] CRC-16 MISMATCH at frame offset ", frame_offset);
        FLAC_DEBUG("[validateFrameFooterCRC]   Expected CRC: 0x", std::hex, 
                   expected_crc, std::dec);
        FLAC_DEBUG("[validateFrameFooterCRC]   Calculated CRC: 0x", std::hex, 
                   calculated_crc, std::dec);
        FLAC_DEBUG("[validateFrameFooterCRC]   Frame length: ", frame_length, " bytes");
        
        // Log first few bytes of frame for debugging
        if (frame_length >= 4) {
            FLAC_DEBUG("[validateFrameFooterCRC]   Frame bytes: 0x", std::hex,
                       static_cast<int>(frame_data[0]), " 0x",
                       static_cast<int>(frame_data[1]), " 0x",
                       static_cast<int>(frame_data[2]), " 0x",
                       static_cast<int>(frame_data[3]), std::dec, " ...");
        }
        
        // Requirement 11.7: Attempt to continue (caller decides on strict mode)
        // Requirement 11.8: In strict mode, reject frame on CRC failure
        // For now, we return false to indicate CRC failure
        // The caller can decide whether to skip this frame or abort
        return false;
    }
    
    FLAC_DEBUG("[validateFrameFooterCRC] CRC-16 valid: 0x", std::hex, 
               calculated_crc, std::dec);
    return true;
}

// ============================================================================
// Seeking Helper Methods (Requirements 22.1-22.8)
// ============================================================================

/**
 * @brief Seek using SEEKTABLE entries per RFC 9639 Section 8.5
 * 
 * Implements Requirements 22.2, 22.3, 22.8:
 * - Requirement 22.2: Use seek points for approximate positioning
 * - Requirement 22.3: Find closest seek point not exceeding target sample
 * - Requirement 22.8: Add byte offset to first frame header position
 * 
 * @param target_sample Target sample position to seek to
 * @return true if seek succeeded, false otherwise
 */
bool FLACDemuxer::seekWithSeekTable_unlocked(uint64_t target_sample)
{
    FLAC_DEBUG("[seekWithSeekTable] Seeking to sample ", target_sample, " using SEEKTABLE");
    
    if (m_seektable.empty()) {
        FLAC_DEBUG("[seekWithSeekTable] No SEEKTABLE available");
        return false;
    }
    
    // Requirement 22.3: Find closest seek point not exceeding target sample
    const FLACSeekPoint* best = nullptr;
    for (const auto& point : m_seektable) {
        // Skip placeholder seek points (sample_number == 0xFFFFFFFFFFFFFFFF)
        if (point.isPlaceholder()) {
            continue;
        }
        
        // Find the closest seek point that doesn't exceed the target
        if (point.sample_number <= target_sample) {
            if (!best || point.sample_number > best->sample_number) {
                best = &point;
            }
        }
    }
    
    if (!best) {
        FLAC_DEBUG("[seekWithSeekTable] No suitable seek point found for sample ", target_sample);
        return false;
    }
    
    FLAC_DEBUG("[seekWithSeekTable] Found seek point: sample=", best->sample_number,
               ", offset=", best->stream_offset, ", frame_samples=", best->frame_samples);
    
    // Requirement 22.8: Add byte offset to first frame header position
    uint64_t seek_offset = m_audio_data_offset + best->stream_offset;
    
    FLAC_DEBUG("[seekWithSeekTable] Seeking to file offset ", seek_offset,
               " (audio_data_offset=", m_audio_data_offset, " + stream_offset=", best->stream_offset, ")");
    
    // Perform the seek
    if (m_handler->seek(static_cast<off_t>(seek_offset), SEEK_SET) != 0) {
        FLAC_DEBUG("[seekWithSeekTable] Failed to seek to file offset ", seek_offset);
        return false;
    }
    
    // Update position tracking
    // Requirement 28.6: Use atomic operations for sample counters
    m_current_offset = seek_offset;
    updateCurrentSample_unlocked(best->sample_number);
    updateEOF_unlocked(false);
    
    FLAC_DEBUG("[seekWithSeekTable] Seek successful, now at sample ", m_current_sample);
    
    // If we need to get closer to the target, parse frames forward
    if (target_sample > m_current_sample) {
        FLAC_DEBUG("[seekWithSeekTable] Parsing forward from sample ", m_current_sample,
                   " to target ", target_sample);
        return parseFramesToSample_unlocked(target_sample);
    }
    
    return true;
}

/**
 * @brief Seek using frame index for sample-accurate positioning
 * 
 * Implements Requirements 22.4, 22.7:
 * - Requirement 22.4: Use cached frame positions for seeking
 * - Requirement 22.7: Provide sample-accurate seeking using index
 * 
 * @param target_sample Target sample position to seek to
 * @return true if seek succeeded, false otherwise
 */
bool FLACDemuxer::seekWithFrameIndex_unlocked(uint64_t target_sample)
{
    FLAC_DEBUG("[seekWithFrameIndex] Seeking to sample ", target_sample, " using frame index");
    
    if (m_frame_index.empty()) {
        FLAC_DEBUG("[seekWithFrameIndex] Frame index is empty");
        return false;
    }
    
    // Binary search for the closest frame not exceeding target sample
    // Requirement 22.7: Provide sample-accurate seeking using index
    size_t left = 0;
    size_t right = m_frame_index.size();
    size_t best_idx = 0;
    
    while (left < right) {
        size_t mid = left + (right - left) / 2;
        
        if (m_frame_index[mid].sample_offset <= target_sample) {
            best_idx = mid;
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    
    const FLACFrameIndexEntry& entry = m_frame_index[best_idx];
    
    FLAC_DEBUG("[seekWithFrameIndex] Found frame index entry: sample=", entry.sample_offset,
               ", file_offset=", entry.file_offset, ", block_size=", entry.block_size);
    
    // Check if the found entry is within acceptable tolerance (250ms)
    // If the entry is too far from the target, fall back to other seeking strategies
    // This prevents using stale/incomplete frame index entries that would result in
    // seeks landing far from the target position
    static constexpr int64_t TOLERANCE_MS = 250;
    uint64_t tolerance_samples = (static_cast<uint64_t>(m_streaminfo.sample_rate) * TOLERANCE_MS) / 1000;
    
    int64_t sample_diff = static_cast<int64_t>(target_sample) - static_cast<int64_t>(entry.sample_offset);
    
    // The entry should be at or before the target (sample_diff >= 0)
    // and within tolerance + one block size (to account for the frame containing the target)
    uint64_t max_acceptable_diff = tolerance_samples + entry.block_size;
    
    if (sample_diff < 0 || static_cast<uint64_t>(sample_diff) > max_acceptable_diff) {
        FLAC_DEBUG("[seekWithFrameIndex] Entry too far from target: diff=", sample_diff,
                   " samples, max_acceptable=", max_acceptable_diff,
                   " - falling back to other strategies");
        return false;
    }
    
    // Seek to the frame position
    if (m_handler->seek(static_cast<off_t>(entry.file_offset), SEEK_SET) != 0) {
        FLAC_DEBUG("[seekWithFrameIndex] Failed to seek to file offset ", entry.file_offset);
        return false;
    }
    
    // Update position tracking
    // Requirement 28.6: Use atomic operations for sample counters
    m_current_offset = entry.file_offset;
    updateCurrentSample_unlocked(entry.sample_offset);
    updateEOF_unlocked(false);
    
    FLAC_DEBUG("[seekWithFrameIndex] Seek successful, now at sample ", m_current_sample);
    return true;
}

/**
 * @brief Estimate byte position for a target sample using linear interpolation
 * 
 * Implements Requirements 1.1, 1.2, 1.4, 1.5 for bisection seeking:
 * - Requirement 1.1: Calculate position using audio_offset + (target/total) * audio_size
 * - Requirement 1.2: Audio data size is file_size - audio_data_offset
 * - Requirement 1.4: Clamp to file_size - 64 if estimated position exceeds file size
 * - Requirement 1.5: Clamp to audio_data_offset if estimated position is before audio data
 * 
 * @param target_sample Target sample position to estimate byte position for
 * @return Estimated byte position in the file, clamped to valid range
 */
uint64_t FLACDemuxer::estimateBytePosition_unlocked(uint64_t target_sample) const
{
    FLAC_DEBUG("[estimateBytePosition] Estimating byte position for sample ", target_sample);
    
    // Requirement 1.3: Handle edge case when total_samples is 0
    // Fall back to audio data offset (beginning of audio)
    if (m_streaminfo.total_samples == 0) {
        FLAC_DEBUG("[estimateBytePosition] total_samples is 0, returning audio_data_offset: ", m_audio_data_offset);
        return m_audio_data_offset;
    }
    
    // Requirement 1.2: Calculate audio data size
    uint64_t audio_data_size = m_file_size - m_audio_data_offset;
    
    if (audio_data_size == 0) {
        FLAC_DEBUG("[estimateBytePosition] audio_data_size is 0, returning audio_data_offset: ", m_audio_data_offset);
        return m_audio_data_offset;
    }
    
    // Requirement 1.1: Calculate initial byte position using linear interpolation
    // estimated_pos = audio_data_offset + (target_sample / total_samples) * audio_data_size
    // Use double precision to avoid integer overflow for large files
    double ratio = static_cast<double>(target_sample) / static_cast<double>(m_streaminfo.total_samples);
    uint64_t estimated_offset = static_cast<uint64_t>(ratio * static_cast<double>(audio_data_size));
    uint64_t estimated_pos = m_audio_data_offset + estimated_offset;
    
    FLAC_DEBUG("[estimateBytePosition] ratio=", ratio, 
               ", audio_offset=", m_audio_data_offset,
               ", audio_size=", audio_data_size,
               ", raw_estimate=", estimated_pos);
    
    // Requirement 1.5: Clamp to audio_data_offset if before audio data
    if (estimated_pos < m_audio_data_offset) {
        FLAC_DEBUG("[estimateBytePosition] Clamping to audio_data_offset (was below)");
        return m_audio_data_offset;
    }
    
    // Requirement 1.4: Clamp to file_size - 64 if exceeds file size
    // Leave room for at least a minimal frame (64 bytes is conservative)
    static constexpr uint64_t MIN_FRAME_ROOM = 64;
    if (m_file_size > MIN_FRAME_ROOM && estimated_pos >= m_file_size - MIN_FRAME_ROOM) {
        uint64_t clamped = m_file_size - MIN_FRAME_ROOM;
        FLAC_DEBUG("[estimateBytePosition] Clamping to file_size - 64: ", clamped);
        return clamped;
    }
    
    FLAC_DEBUG("[estimateBytePosition] Estimated position: ", estimated_pos);
    return estimated_pos;
}

/**
 * @brief Seek using byte-position estimation with iterative refinement
 * 
 * This is the strategy used by VLC and other players when no SEEKTABLE is available.
 * It estimates the byte position based on the ratio of target sample to total samples,
 * then iteratively refines the position until within 250ms of the target.
 * 
 * Implements Requirements 3.1-3.6 for bisection seeking:
 * - Requirement 3.1: Adjust search to upper half when actual < target
 * - Requirement 3.2: Adjust search to lower half when actual > target
 * - Requirement 3.3: Accept position when time differential <= 250ms
 * - Requirement 3.4: Accept best position when iteration count > 10
 * - Requirement 3.5: Accept position when search range < minimum frame size
 * - Requirement 3.6: Accept position when same position found twice consecutively
 * 
 * @param target_sample Target sample position to seek to
 * @return true if seek succeeded, false otherwise
 */
bool FLACDemuxer::seekWithByteEstimation_unlocked(uint64_t target_sample)
{
    FLAC_DEBUG("[seekWithByteEstimation] Seeking to sample ", target_sample, " using byte estimation");
    
    // Need total samples and file size for estimation
    // Requirement 1.3: Fall back to beginning when total_samples is 0
    if (m_streaminfo.total_samples == 0) {
        FLAC_DEBUG("[seekWithByteEstimation] Cannot estimate: total_samples is 0");
        return false;
    }
    
    uint64_t audio_data_size = m_file_size - m_audio_data_offset;
    if (audio_data_size == 0) {
        FLAC_DEBUG("[seekWithByteEstimation] Cannot estimate: audio data size is 0");
        return false;
    }
    
    // ========================================================================
    // Bisection State Initialization
    // Requirement 3.1, 3.2: Initialize search range [audio_offset, file_size]
    // ========================================================================
    
    // Calculate tolerance in samples (250ms) - Requirement 4.1, 4.2
    static constexpr int64_t TOLERANCE_MS = 250;
    uint64_t tolerance_samples = (static_cast<uint64_t>(m_streaminfo.sample_rate) * TOLERANCE_MS) / 1000;
    
    // Binary search bounds - Requirement 3.1, 3.2
    uint64_t low_pos = m_audio_data_offset;
    uint64_t high_pos = m_file_size;
    
    // Best position tracking - Requirement 4.3, 4.4
    uint64_t best_pos = m_audio_data_offset;
    uint64_t best_sample = 0;
    uint32_t best_block_size = 0;  // Track block size for frame index - Requirement 6.4
    int64_t best_diff_ms = INT64_MAX;
    bool best_is_before_target = true;
    
    // Iteration tracking - Requirement 3.4
    static constexpr int MAX_ITERATIONS = 10;
    
    // Consecutive same position detection - Requirement 3.6
    uint64_t last_frame_pos = UINT64_MAX;
    
    // Minimum search range - Requirement 3.5
    static constexpr uint64_t MIN_SEARCH_RANGE = 64;
    
    FLAC_DEBUG("[seekWithByteEstimation] Starting bisection search:");
    FLAC_DEBUG("[seekWithByteEstimation]   Target sample: ", target_sample);
    FLAC_DEBUG("[seekWithByteEstimation]   Tolerance: ", tolerance_samples, " samples (", TOLERANCE_MS, "ms)");
    FLAC_DEBUG("[seekWithByteEstimation]   Search range: [", low_pos, ", ", high_pos, "]");
    
    for (int iteration = 0; iteration < MAX_ITERATIONS; iteration++) {
        // ====================================================================
        // Requirement 3.5: Check if search range collapsed
        // ====================================================================
        if (high_pos <= low_pos + MIN_SEARCH_RANGE) {
            FLAC_DEBUG("[seekWithByteEstimation] Requirement 3.5: Search range collapsed (",
                       high_pos - low_pos, " bytes < ", MIN_SEARCH_RANGE, ")");
            break;
        }
        
        // ====================================================================
        // Calculate position within current search range
        // For first iteration, use global estimate; for subsequent iterations,
        // use midpoint of current search range (true bisection)
        // ====================================================================
        uint64_t estimated_pos;
        if (iteration == 0) {
            // First iteration: use linear interpolation based on sample ratio
            estimated_pos = estimateBytePosition_unlocked(target_sample);
            // Clamp to current search bounds
            if (estimated_pos < low_pos) estimated_pos = low_pos;
            if (estimated_pos >= high_pos) {
                estimated_pos = (high_pos > MIN_SEARCH_RANGE) ? high_pos - MIN_SEARCH_RANGE : low_pos;
            }
        } else {
            // Subsequent iterations: use midpoint of current search range (bisection)
            estimated_pos = low_pos + (high_pos - low_pos) / 2;
        }
        
        FLAC_DEBUG("[seekWithByteEstimation] Iteration ", iteration, 
                   ": estimated position ", estimated_pos,
                   " (range: [", low_pos, ", ", high_pos, "])");
        
        // ====================================================================
        // Find a frame at or after this position
        // ====================================================================
        uint64_t frame_pos = 0;
        uint64_t frame_sample = 0;
        uint32_t frame_block_size = 0;
        
        if (!findFrameAtPosition_unlocked(estimated_pos, frame_pos, frame_sample, frame_block_size)) {
            FLAC_DEBUG("[seekWithByteEstimation] No frame found at position ", estimated_pos);
            // Narrow search range and retry
            if (estimated_pos > low_pos + MIN_SEARCH_RANGE) {
                high_pos = estimated_pos;
                continue;
            }
            // Can't find any frames in remaining range
            break;
        }
        
        // ====================================================================
        // Requirement 3.6: Check for consecutive same position
        // ====================================================================
        if (frame_pos == last_frame_pos) {
            FLAC_DEBUG("[seekWithByteEstimation] Requirement 3.6: Same position found twice consecutively");
            break;
        }
        last_frame_pos = frame_pos;
        
        // ====================================================================
        // Calculate time differential - Requirement 4.1
        // time_diff_ms = abs(actual_sample - target_sample) * 1000 / sample_rate
        // ====================================================================
        int64_t sample_diff = static_cast<int64_t>(frame_sample) - static_cast<int64_t>(target_sample);
        int64_t time_diff_ms = (std::abs(sample_diff) * 1000) / static_cast<int64_t>(m_streaminfo.sample_rate);
        bool is_before_target = (frame_sample <= target_sample);
        
        FLAC_DEBUG("[seekWithByteEstimation] Found frame at ", frame_pos, 
                   " with sample ", frame_sample, ", block_size ", frame_block_size,
                   " (target: ", target_sample, ", diff: ", time_diff_ms, "ms, ",
                   is_before_target ? "before" : "after", " target)");
        
        // ====================================================================
        // Update best position - Requirement 4.3, 4.4
        // Track position with minimum differential
        // Prefer positions before target when equal (for gapless playback)
        // ====================================================================
        bool update_best = false;
        if (time_diff_ms < best_diff_ms) {
            update_best = true;
        } else if (time_diff_ms == best_diff_ms && is_before_target && !best_is_before_target) {
            // Requirement 4.4: Prefer positions before target when equal
            update_best = true;
        }
        
        if (update_best) {
            best_pos = frame_pos;
            best_sample = frame_sample;
            best_block_size = frame_block_size;  // Track block size for frame index - Requirement 6.4
            best_diff_ms = time_diff_ms;
            best_is_before_target = is_before_target;
            FLAC_DEBUG("[seekWithByteEstimation] Updated best position: ", best_pos,
                       " (sample ", best_sample, ", block_size ", best_block_size, ", diff ", best_diff_ms, "ms)");
        }
        
        // ====================================================================
        // Requirement 3.3: Check convergence (within 250ms tolerance)
        // ====================================================================
        if (time_diff_ms <= TOLERANCE_MS) {
            FLAC_DEBUG("[seekWithByteEstimation] Requirement 3.3: Within tolerance (",
                       time_diff_ms, "ms <= ", TOLERANCE_MS, "ms)");
            
            // Position at this frame
            if (m_handler->seek(static_cast<off_t>(frame_pos), SEEK_SET) != 0) {
                FLAC_DEBUG("[seekWithByteEstimation] Failed to seek to frame position");
                return false;
            }
            
            m_current_offset = frame_pos;
            updateCurrentSample_unlocked(frame_sample);
            updateEOF_unlocked(false);
            
            // Add discovered frame to index - Requirement 6.4
            FLACFrame discovered_frame;
            discovered_frame.file_offset = frame_pos;
            discovered_frame.sample_offset = frame_sample;
            discovered_frame.block_size = frame_block_size;
            discovered_frame.sample_rate = m_streaminfo.sample_rate;
            discovered_frame.channels = m_streaminfo.channels;
            discovered_frame.bits_per_sample = m_streaminfo.bits_per_sample;
            addFrameToIndex_unlocked(discovered_frame);
            
            FLAC_DEBUG("[seekWithByteEstimation] Seek successful after ", iteration + 1,
                       " iterations, now at sample ", frame_sample, 
                       " (target was ", target_sample, ", diff: ", time_diff_ms, "ms)");
            return true;
        }
        
        // ====================================================================
        // Adjust search range - Requirement 3.1, 3.2
        // ====================================================================
        if (frame_sample < target_sample) {
            // Requirement 3.1: Actual < target, search upper half
            low_pos = frame_pos + frame_block_size;  // Move past this frame
            FLAC_DEBUG("[seekWithByteEstimation] Requirement 3.1: Searching upper half, new low_pos=", low_pos);
        } else {
            // Requirement 3.2: Actual > target, search lower half
            high_pos = frame_pos;
            FLAC_DEBUG("[seekWithByteEstimation] Requirement 3.2: Searching lower half, new high_pos=", high_pos);
        }
    }
    
    // ========================================================================
    // Requirement 3.4: Accept best position after max iterations
    // Requirement 4.3: Use closest position found during all iterations
    // ========================================================================
    
    if (best_diff_ms < INT64_MAX) {
        FLAC_DEBUG("[seekWithByteEstimation] Requirement 3.4/4.3: Using best position found");
        FLAC_DEBUG("[seekWithByteEstimation]   Best position: ", best_pos);
        FLAC_DEBUG("[seekWithByteEstimation]   Best sample: ", best_sample);
        FLAC_DEBUG("[seekWithByteEstimation]   Best block size: ", best_block_size);
        FLAC_DEBUG("[seekWithByteEstimation]   Best diff: ", best_diff_ms, "ms");
        
        if (m_handler->seek(static_cast<off_t>(best_pos), SEEK_SET) != 0) {
            FLAC_DEBUG("[seekWithByteEstimation] Failed to seek to best position");
            return false;
        }
        
        m_current_offset = best_pos;
        updateCurrentSample_unlocked(best_sample);
        updateEOF_unlocked(false);
        
        // Add discovered frame to index - Requirement 6.4
        // Cache frame positions for future seeks
        if (best_block_size > 0) {
            FLACFrame discovered_frame;
            discovered_frame.file_offset = best_pos;
            discovered_frame.sample_offset = best_sample;
            discovered_frame.block_size = best_block_size;
            discovered_frame.sample_rate = m_streaminfo.sample_rate;
            discovered_frame.channels = m_streaminfo.channels;
            discovered_frame.bits_per_sample = m_streaminfo.bits_per_sample;
            addFrameToIndex_unlocked(discovered_frame);
            FLAC_DEBUG("[seekWithByteEstimation] Requirement 6.4: Added best frame to index");
        }
        
        // Requirement 4.5: Log final time differential
        FLAC_DEBUG("[seekWithByteEstimation] Seek completed with final diff: ", best_diff_ms, "ms");
        
        // Even if not within tolerance, we found a position
        return true;
    }
    
    FLAC_DEBUG("[seekWithByteEstimation] Failed to find any valid frame position");
    return false;
}

/**
 * @brief Find a valid frame at or after the given file position
 * 
 * Implements RFC 9639 Section 9.1 frame discovery for bisection seeking:
 * - Requirement 2.1: Search forward for 15-bit sync pattern (0xFFF8 or 0xFFF9)
 * - Requirement 2.2: Verify blocking strategy bit matches stream's established strategy
 * - Requirement 2.3: Validate CRC-8 checksum per RFC 9639 Section 9.1.8
 * - Requirement 2.4: Parse coded number per RFC 9639 Section 9.1.5
 * - Requirement 2.5: For fixed block size (0xFFF8), interpret coded number as frame number
 * - Requirement 2.6: For variable block size (0xFFF9), interpret coded number as sample number
 * - Requirement 2.7: Report failure if no valid frame found within 64KB
 * - Requirement 2.8: Continue searching past false positive sync patterns (CRC failures)
 * - Requirement 2.9: Record file position, sample offset, and block size
 * 
 * @param start_pos File position to start searching from
 * @param frame_pos Output: file position of found frame
 * @param frame_sample Output: sample offset of found frame
 * @param block_size Output: block size of found frame in samples
 * @return true if a frame was found, false otherwise
 */
bool FLACDemuxer::findFrameAtPosition_unlocked(uint64_t start_pos, uint64_t& frame_pos, 
                                               uint64_t& frame_sample, uint32_t& block_size)
{
    FLAC_DEBUG("[findFrameAtPosition] Searching for frame at position ", start_pos);
    FLAC_DEBUG("[findFrameAtPosition] Requirement 2.1: Searching for 15-bit sync pattern");
    
    // Requirement 2.7: Search up to 64KB for a valid frame
    const size_t MAX_SEARCH = 65536;
    const size_t BUFFER_SIZE = 8192;
    std::vector<uint8_t> buffer(BUFFER_SIZE);
    
    uint64_t search_pos = start_pos;
    size_t total_searched = 0;
    
    while (total_searched < MAX_SEARCH && search_pos < m_file_size) {
        if (m_handler->seek(static_cast<off_t>(search_pos), SEEK_SET) != 0) {
            FLAC_DEBUG("[findFrameAtPosition] Seek failed at position ", search_pos);
            return false;
        }
        
        size_t bytes_read = m_handler->read(buffer.data(), 1, BUFFER_SIZE);
        if (bytes_read < 4) {
            FLAC_DEBUG("[findFrameAtPosition] Read failed, only got ", bytes_read, " bytes");
            return false;
        }
        
        // Requirement 2.1: Search for frame sync pattern (0xFFF8 or 0xFFF9)
        // RFC 9639 Section 9.1: 15-bit sync code 0b111111111111100
        for (size_t i = 0; i < bytes_read - 1; i++) {
            if (buffer[i] == 0xFF && (buffer[i + 1] == 0xF8 || buffer[i + 1] == 0xF9)) {
                uint64_t sync_pos = search_pos + i;
                bool is_variable = (buffer[i + 1] == 0xF9);
                
                FLAC_DEBUG("[findFrameAtPosition] Found sync pattern at ", sync_pos,
                           " (", is_variable ? "variable" : "fixed", " block size)");
                
                // Requirement 2.2: Verify blocking strategy matches stream's established strategy
                if (m_blocking_strategy_set && is_variable != m_variable_block_size) {
                    FLAC_DEBUG("[findFrameAtPosition] Requirement 2.2: Blocking strategy mismatch, skipping");
                    // Requirement 2.8: Continue searching past false positives
                    continue;
                }
                
                // Try to parse frame header from buffer
                // parseFrameHeader_unlocked validates CRC-8 (Requirement 2.3)
                // and parses coded number (Requirements 2.4, 2.5, 2.6)
                if (i + 16 <= bytes_read) {
                    FLACFrame frame;
                    frame.file_offset = sync_pos;
                    frame.variable_block_size = is_variable;
                    if (parseFrameHeader_unlocked(frame, &buffer[i], bytes_read - i)) {
                        // Requirement 2.9: Record file position, sample offset, and block size
                        frame_pos = sync_pos;
                        frame_sample = frame.sample_offset;
                        block_size = frame.block_size;
                        FLAC_DEBUG("[findFrameAtPosition] Requirement 2.9: Valid frame at ", sync_pos, 
                                   ", sample ", frame_sample, ", block_size ", block_size);
                        return true;
                    }
                    // Requirement 2.8: CRC-8 failed or invalid header, continue searching
                    FLAC_DEBUG("[findFrameAtPosition] Requirement 2.8: Header validation failed, continuing search");
                }
                
                // Need to read more data for header parsing
                if (m_handler->seek(static_cast<off_t>(sync_pos), SEEK_SET) != 0) {
                    continue;
                }
                
                uint8_t header_buf[32];  // Buffer for frame header
                size_t header_read = m_handler->read(header_buf, 1, 32);
                if (header_read >= 4) {
                    FLACFrame frame;
                    frame.file_offset = sync_pos;
                    frame.variable_block_size = is_variable;
                    if (parseFrameHeader_unlocked(frame, header_buf, header_read)) {
                        // Requirement 2.9: Record file position, sample offset, and block size
                        frame_pos = sync_pos;
                        frame_sample = frame.sample_offset;
                        block_size = frame.block_size;
                        FLAC_DEBUG("[findFrameAtPosition] Requirement 2.9: Valid frame at ", sync_pos, 
                                   ", sample ", frame_sample, ", block_size ", block_size);
                        return true;
                    }
                }
                
                // Requirement 2.8: Continue searching past false positive sync patterns
                FLAC_DEBUG("[findFrameAtPosition] Requirement 2.8: Sync at ", sync_pos, " was false positive");
            }
        }
        
        // Move to next buffer, overlapping by 1 byte to catch sync at boundary
        search_pos += bytes_read - 1;
        total_searched += bytes_read - 1;
    }
    
    // Requirement 2.7: Report failure if no valid frame found within 64KB
    FLAC_DEBUG("[findFrameAtPosition] Requirement 2.7: No valid frame found after searching ", 
               total_searched, " bytes");
    return false;
}

/**
 * @brief Add a frame to the frame index
 * 
 * Implements Requirement 22.4: Build frame index during initial parsing
 * 
 * @param frame Frame information to add to index
 */
void FLACDemuxer::addFrameToIndex_unlocked(const FLACFrame& frame)
{
    // Only add valid frames
    if (!frame.isValid()) {
        return;
    }
    
    // Check if this frame is already in the index (avoid duplicates)
    if (!m_frame_index.empty()) {
        const auto& last = m_frame_index.back();
        if (last.file_offset == frame.file_offset) {
            return;  // Already indexed
        }
        
        // Ensure frames are added in order
        if (frame.sample_offset < last.sample_offset) {
            FLAC_DEBUG("[addFrameToIndex] Warning: Frame at sample ", frame.sample_offset,
                       " is out of order (last indexed: ", last.sample_offset, ")");
            return;
        }
    }
    
    // Add to index
    m_frame_index.emplace_back(frame.sample_offset, frame.file_offset, frame.block_size);
    
    FLAC_DEBUG("[addFrameToIndex] Added frame to index: sample=", frame.sample_offset,
               ", file_offset=", frame.file_offset, ", block_size=", frame.block_size,
               " (total indexed: ", m_frame_index.size(), ")");
}

/**
 * @brief Parse frames forward from current position to target sample
 * 
 * Implements Requirement 22.3: Parse frames forward to exact position
 * 
 * This method reads frames sequentially from the current position until
 * reaching a frame that contains or exceeds the target sample. Each
 * discovered frame is added to the frame index for future seeking.
 * 
 * @param target_sample Target sample position to reach
 * @return true if target was reached, false otherwise
 */
bool FLACDemuxer::parseFramesToSample_unlocked(uint64_t target_sample)
{
    FLAC_DEBUG("[parseFramesToSample] Parsing forward from sample ", m_current_sample,
               " to target ", target_sample);
    
    // Limit the number of frames we'll parse to prevent infinite loops
    static constexpr size_t MAX_FRAMES_TO_PARSE = 10000;
    size_t frames_parsed = 0;
    
    while (m_current_sample < target_sample && frames_parsed < MAX_FRAMES_TO_PARSE) {
        // Check for EOF
        if (isEOF_unlocked()) {
            FLAC_DEBUG("[parseFramesToSample] Reached EOF before target sample");
            return false;
        }
        
        // Find and parse the next frame
        FLACFrame frame;
        if (!findNextFrame_unlocked(frame)) {
            FLAC_DEBUG("[parseFramesToSample] Failed to find next frame");
            return false;
        }
        
        // Add frame to index for future seeking
        // Requirement 22.4: Build frame index during parsing
        addFrameToIndex_unlocked(frame);
        
        // Check if this frame contains or exceeds the target
        uint64_t frame_end_sample = frame.sample_offset + frame.block_size;
        
        if (frame.sample_offset <= target_sample && target_sample < frame_end_sample) {
            // Target is within this frame - we're done
            FLAC_DEBUG("[parseFramesToSample] Target sample ", target_sample,
                       " is within frame at sample ", frame.sample_offset,
                       " (block_size=", frame.block_size, ")");
            
            // Position at the start of this frame
            if (m_handler->seek(static_cast<off_t>(frame.file_offset), SEEK_SET) != 0) {
                FLAC_DEBUG("[parseFramesToSample] Failed to seek back to frame start");
                return false;
            }
            
            m_current_offset = frame.file_offset;
            updateCurrentSample_unlocked(frame.sample_offset);
            return true;
        }
        
        // Calculate frame size and skip to next frame
        uint32_t frame_size = calculateFrameSize_unlocked(frame);
        if (frame_size == 0) {
            frame_size = 64;  // Conservative fallback
        }
        
        // Update position to end of this frame
        // Requirement 28.6: Use atomic operations for sample counters
        m_current_offset = frame.file_offset + frame_size;
        updateCurrentSample_unlocked(frame_end_sample);
        
        // Seek to next frame position
        if (m_handler->seek(static_cast<off_t>(m_current_offset), SEEK_SET) != 0) {
            FLAC_DEBUG("[parseFramesToSample] Failed to seek to next frame");
            return false;
        }
        
        frames_parsed++;
    }
    
    if (frames_parsed >= MAX_FRAMES_TO_PARSE) {
        FLAC_DEBUG("[parseFramesToSample] Reached maximum frame parse limit (", MAX_FRAMES_TO_PARSE, ")");
        return false;
    }
    
    FLAC_DEBUG("[parseFramesToSample] Reached target sample ", m_current_sample);
    return true;
}


// ============================================================================
// Error Handling and Recovery (Requirements 24.1-24.8)
// ============================================================================

/**
 * @brief Attempt to derive stream parameters from frame headers
 * 
 * Implements Requirement 24.3: If STREAMINFO is missing, derive parameters
 * from frame headers. This is a fallback mechanism for corrupted files.
 * 
 * This method scans for the first valid frame and extracts:
 * - Sample rate
 * - Number of channels
 * - Bits per sample
 * - Block size (used for min/max block size estimates)
 * 
 * @return true if parameters were successfully derived, false otherwise
 */
bool FLACDemuxer::deriveParametersFromFrameHeaders_unlocked()
{
    FLAC_DEBUG("[deriveParametersFromFrameHeaders] Attempting to derive stream parameters from frame headers");
    FLAC_DEBUG("[deriveParametersFromFrameHeaders] Requirement 24.3: STREAMINFO missing, using frame header fallback");
    
    // Save current position
    uint64_t saved_offset = m_current_offset;
    
    // Start searching from the audio data offset (after metadata blocks)
    // If we don't know where audio data starts, start from current position
    uint64_t search_start = (m_audio_data_offset > 0) ? m_audio_data_offset : m_current_offset;
    
    if (m_handler->seek(static_cast<off_t>(search_start), SEEK_SET) != 0) {
        FLAC_DEBUG("[deriveParametersFromFrameHeaders] Failed to seek to search start position");
        return false;
    }
    
    m_current_offset = search_start;
    
    // Try to find and parse a valid frame
    FLACFrame frame;
    if (!findNextFrame_unlocked(frame)) {
        FLAC_DEBUG("[deriveParametersFromFrameHeaders] Failed to find any valid frame");
        // Restore position
        m_handler->seek(static_cast<off_t>(saved_offset), SEEK_SET);
        m_current_offset = saved_offset;
        return false;
    }
    
    // Validate that we got useful parameters
    if (frame.sample_rate == 0 || frame.channels == 0 || frame.bits_per_sample == 0) {
        FLAC_DEBUG("[deriveParametersFromFrameHeaders] Frame header has invalid parameters");
        FLAC_DEBUG("[deriveParametersFromFrameHeaders]   sample_rate=", frame.sample_rate);
        FLAC_DEBUG("[deriveParametersFromFrameHeaders]   channels=", static_cast<int>(frame.channels));
        FLAC_DEBUG("[deriveParametersFromFrameHeaders]   bits_per_sample=", static_cast<int>(frame.bits_per_sample));
        // Restore position
        m_handler->seek(static_cast<off_t>(saved_offset), SEEK_SET);
        m_current_offset = saved_offset;
        return false;
    }
    
    // Populate STREAMINFO from frame header
    FLAC_DEBUG("[deriveParametersFromFrameHeaders] Deriving STREAMINFO from frame header:");
    FLAC_DEBUG("[deriveParametersFromFrameHeaders]   sample_rate: ", frame.sample_rate, " Hz");
    FLAC_DEBUG("[deriveParametersFromFrameHeaders]   channels: ", static_cast<int>(frame.channels));
    FLAC_DEBUG("[deriveParametersFromFrameHeaders]   bits_per_sample: ", static_cast<int>(frame.bits_per_sample));
    FLAC_DEBUG("[deriveParametersFromFrameHeaders]   block_size: ", frame.block_size, " samples");
    
    m_streaminfo.sample_rate = frame.sample_rate;
    m_streaminfo.channels = frame.channels;
    m_streaminfo.bits_per_sample = frame.bits_per_sample;
    
    // Use block size for min/max estimates (we only have one frame to work with)
    // Per RFC 9639, block size must be >= 16
    m_streaminfo.min_block_size = (frame.block_size >= 16) ? frame.block_size : 16;
    m_streaminfo.max_block_size = (frame.block_size >= 16) ? frame.block_size : 4096;
    
    // Frame size is unknown without scanning the entire file
    m_streaminfo.min_frame_size = 0;  // Unknown
    m_streaminfo.max_frame_size = 0;  // Unknown
    
    // Total samples is unknown without STREAMINFO
    m_streaminfo.total_samples = 0;  // Unknown
    
    // MD5 is unavailable
    std::memset(m_streaminfo.md5_signature, 0, sizeof(m_streaminfo.md5_signature));
    
    // Update audio data offset if not already set
    if (m_audio_data_offset == 0) {
        m_audio_data_offset = frame.file_offset;
        FLAC_DEBUG("[deriveParametersFromFrameHeaders] Set audio_data_offset to ", m_audio_data_offset);
    }
    
    // Restore position to the found frame (so playback can start from here)
    if (m_handler->seek(static_cast<off_t>(frame.file_offset), SEEK_SET) != 0) {
        FLAC_DEBUG("[deriveParametersFromFrameHeaders] Failed to seek back to frame position");
        return false;
    }
    m_current_offset = frame.file_offset;
    // Requirement 28.6: Use atomic operations for sample counters
    updateCurrentSample_unlocked(frame.sample_offset);
    
    FLAC_DEBUG("[deriveParametersFromFrameHeaders] Successfully derived parameters from frame header");
    return true;
}

/**
 * @brief Resynchronize to the next valid frame sync code after sync loss
 * 
 * Implements Requirement 24.4: If frame sync is lost, resynchronize to
 * the next valid sync code. This enables recovery from corrupted frames.
 * 
 * The method searches for the 15-bit sync pattern (0xFFF8 or 0xFFF9) and
 * validates the frame header to confirm it's a real sync code.
 * 
 * @param max_search_bytes Maximum bytes to search (default 4096)
 * @return true if resynchronization succeeded, false if no sync found
 */
bool FLACDemuxer::resyncToNextFrame_unlocked(size_t max_search_bytes)
{
    FLAC_DEBUG("[resyncToNextFrame] Attempting to resynchronize to next valid frame");
    FLAC_DEBUG("[resyncToNextFrame] Requirement 24.4: Resynchronizing after sync loss");
    FLAC_DEBUG("[resyncToNextFrame] Starting search at offset ", m_current_offset);
    FLAC_DEBUG("[resyncToNextFrame] Maximum search range: ", max_search_bytes, " bytes");
    
    // Cap search range to prevent excessive I/O
    static constexpr size_t ABSOLUTE_MAX_SEARCH = 65536;  // 64KB absolute maximum
    if (max_search_bytes > ABSOLUTE_MAX_SEARCH) {
        max_search_bytes = ABSOLUTE_MAX_SEARCH;
    }
    
    // Read buffer for searching
    static constexpr size_t BUFFER_SIZE = 4096;
    uint8_t buffer[BUFFER_SIZE];
    
    size_t total_searched = 0;
    uint64_t search_start = m_current_offset;
    
    while (total_searched < max_search_bytes) {
        // Calculate how much to read this iteration
        size_t to_read = std::min(BUFFER_SIZE, max_search_bytes - total_searched);
        
        // Seek to current search position
        if (m_handler->seek(static_cast<off_t>(search_start + total_searched), SEEK_SET) != 0) {
            FLAC_DEBUG("[resyncToNextFrame] Failed to seek to search position");
            return false;
        }
        
        // Read buffer
        size_t bytes_read = m_handler->read(buffer, 1, to_read);
        if (bytes_read < 2) {
            FLAC_DEBUG("[resyncToNextFrame] Insufficient data read: ", bytes_read, " bytes");
            return false;
        }
        
        // Search for sync pattern in buffer
        for (size_t i = 0; i < bytes_read - 1; ++i) {
            // Check for sync pattern: 0xFF followed by 0xF8 or 0xF9
            if (buffer[i] == 0xFF && (buffer[i + 1] == 0xF8 || buffer[i + 1] == 0xF9)) {
                uint64_t potential_frame_offset = search_start + total_searched + i;
                bool is_variable = (buffer[i + 1] == 0xF9);
                
                FLAC_DEBUG("[resyncToNextFrame] Potential sync at offset ", potential_frame_offset,
                           " (", is_variable ? "variable" : "fixed", " block size)");
                
                // Validate blocking strategy consistency
                if (m_blocking_strategy_set && is_variable != m_variable_block_size) {
                    FLAC_DEBUG("[resyncToNextFrame] Blocking strategy mismatch, skipping");
                    continue;
                }
                
                // Try to validate the frame header
                FLACFrame frame;
                frame.file_offset = potential_frame_offset;
                frame.variable_block_size = is_variable;
                
                // Need to read more data for header validation
                if (m_handler->seek(static_cast<off_t>(potential_frame_offset), SEEK_SET) != 0) {
                    continue;
                }
                
                uint8_t header_buffer[32];  // Enough for frame header
                size_t header_read = m_handler->read(header_buffer, 1, sizeof(header_buffer));
                
                if (header_read >= 4 && parseFrameHeader_unlocked(frame, header_buffer, header_read)) {
                    // Valid frame found!
                    FLAC_DEBUG("[resyncToNextFrame] Successfully resynchronized at offset ", potential_frame_offset);
                    FLAC_DEBUG("[resyncToNextFrame]   Block size: ", frame.block_size, " samples");
                    FLAC_DEBUG("[resyncToNextFrame]   Sample rate: ", frame.sample_rate, " Hz");
                    
                    // Update position
                    // Requirement 28.6: Use atomic operations for sample counters
                    m_current_offset = potential_frame_offset;
                    updateCurrentSample_unlocked(frame.sample_offset);
                    
                    // Seek back to frame start for reading
                    m_handler->seek(static_cast<off_t>(potential_frame_offset), SEEK_SET);
                    
                    return true;
                }
                
                FLAC_DEBUG("[resyncToNextFrame] False sync at offset ", potential_frame_offset);
            }
        }
        
        // Move to next buffer, overlapping by 1 byte to catch sync codes at boundaries
        total_searched += bytes_read - 1;
    }
    
    FLAC_DEBUG("[resyncToNextFrame] Failed to find valid sync code in ", total_searched, " bytes");
    return false;
}

/**
 * @brief Skip a corrupted frame and attempt to continue playback
 * 
 * Implements Requirements 24.5, 24.6: Log CRC errors but attempt to
 * continue playback. Handle truncated files gracefully.
 * 
 * @param frame_offset File offset of the corrupted frame
 * @return true if successfully skipped to next frame, false otherwise
 */
bool FLACDemuxer::skipCorruptedFrame_unlocked(uint64_t frame_offset)
{
    FLAC_DEBUG("[skipCorruptedFrame] Skipping corrupted frame at offset ", frame_offset);
    FLAC_DEBUG("[skipCorruptedFrame] Requirement 24.5: Logging error and attempting to continue");
    
    // Log the error
    reportError("Frame", "Corrupted frame detected at offset " + std::to_string(frame_offset) + ", attempting recovery");
    
    // Move past the current position to avoid re-detecting the same sync code
    // Use STREAMINFO min_frame_size if available, otherwise use a conservative estimate
    uint32_t skip_distance = 16;  // Minimum skip to get past sync code and basic header
    
    if (m_streaminfo.min_frame_size > 0) {
        skip_distance = m_streaminfo.min_frame_size;
        FLAC_DEBUG("[skipCorruptedFrame] Using STREAMINFO min_frame_size: ", skip_distance, " bytes");
    }
    
    // Update current offset to skip past the corrupted frame
    m_current_offset = frame_offset + skip_distance;
    
    // Check if we've gone past EOF
    if (m_file_size > 0 && m_current_offset >= m_file_size) {
        FLAC_DEBUG("[skipCorruptedFrame] Requirement 24.6: Reached EOF while skipping corrupted frame");
        updateEOF_unlocked(true);
        return false;
    }
    
    // Try to resynchronize to the next valid frame
    if (!resyncToNextFrame_unlocked()) {
        FLAC_DEBUG("[skipCorruptedFrame] Failed to resynchronize after corrupted frame");
        updateEOF_unlocked(true);
        return false;
    }
    
    FLAC_DEBUG("[skipCorruptedFrame] Successfully recovered, now at offset ", m_current_offset);
    return true;
}

/**
 * @brief Handle memory allocation failure gracefully
 * 
 * Implements Requirement 24.7: Return appropriate error codes on
 * allocation failure without crashing.
 * 
 * @param operation Description of the operation that failed
 * @param requested_size Size of the failed allocation
 */
void FLACDemuxer::handleAllocationFailure_unlocked(const char* operation, size_t requested_size)
{
    FLAC_DEBUG("[handleAllocationFailure] Requirement 24.7: Memory allocation failed");
    FLAC_DEBUG("[handleAllocationFailure] Operation: ", operation);
    FLAC_DEBUG("[handleAllocationFailure] Requested size: ", requested_size, " bytes");
    
    // Report the error
    std::string error_msg = "Memory allocation failed for ";
    error_msg += operation;
    error_msg += " (";
    error_msg += std::to_string(requested_size);
    error_msg += " bytes)";
    
    reportError("Memory", error_msg);
    
    // Set EOF to prevent further operations
    // Requirement 28.7: Use atomic error state flags
    updateEOF_unlocked(true);
    updateError_unlocked(true);
}

/**
 * @brief Handle I/O error gracefully
 * 
 * Implements Requirement 24.8: Handle read errors and EOF conditions
 * without crashing.
 * 
 * @param operation Description of the I/O operation that failed
 * @return false always (to indicate error to caller)
 */
bool FLACDemuxer::handleIOError_unlocked(const char* operation)
{
    FLAC_DEBUG("[handleIOError] Requirement 24.8: I/O operation failed");
    FLAC_DEBUG("[handleIOError] Operation: ", operation);
    FLAC_DEBUG("[handleIOError] Current offset: ", m_current_offset);
    
    // Check if this is an EOF condition
    if (m_handler && m_handler->eof()) {
        FLAC_DEBUG("[handleIOError] EOF condition detected");
        updateEOF_unlocked(true);
        return false;
    }
    
    // Report the error
    std::string error_msg = "I/O error during ";
    error_msg += operation;
    
    reportError("IO", error_msg);
    
    return false;
}


// ============================================================================
// Atomic State Update Helpers (Requirements 28.6, 28.7)
// ============================================================================

/**
 * @brief Update current sample position (both regular and atomic)
 * 
 * Implements Requirement 28.6: Use atomic operations for sample counters.
 * Updates both m_current_sample and m_atomic_current_sample atomically.
 * Must be called while holding m_state_mutex.
 * 
 * @param sample New sample position
 */
void FLACDemuxer::updateCurrentSample_unlocked(uint64_t sample)
{
    m_current_sample = sample;
    m_atomic_current_sample.store(sample, std::memory_order_release);
}

/**
 * @brief Update EOF state (both regular and atomic)
 * 
 * Implements Requirement 28.7: Use atomic error state flags.
 * Updates both m_eof and m_atomic_eof atomically.
 * Must be called while holding m_state_mutex.
 * 
 * @param eof New EOF state
 */
void FLACDemuxer::updateEOF_unlocked(bool eof)
{
    m_eof = eof;
    m_atomic_eof.store(eof, std::memory_order_release);
}

/**
 * @brief Update error state (atomic only)
 * 
 * Implements Requirement 28.7: Use atomic error state flags.
 * Updates m_atomic_error for thread-safe error propagation.
 * Must be called while holding m_state_mutex.
 * 
 * @param error New error state
 */
void FLACDemuxer::updateError_unlocked(bool error)
{
    m_atomic_error.store(error, std::memory_order_release);
}

/**
 * @brief Get current sample position atomically (lock-free read)
 * 
 * Implements Requirement 28.6: Provides lock-free read access to
 * the current sample position for quick queries.
 * 
 * @return Current sample position
 */
uint64_t FLACDemuxer::getAtomicCurrentSample() const
{
    return m_atomic_current_sample.load(std::memory_order_acquire);
}

/**
 * @brief Get EOF state atomically (lock-free read)
 * 
 * Implements Requirement 28.7: Provides lock-free read access to
 * the EOF state for quick queries.
 * 
 * @return Current EOF state
 */
bool FLACDemuxer::getAtomicEOF() const
{
    return m_atomic_eof.load(std::memory_order_acquire);
}

/**
 * @brief Get error state atomically (lock-free read)
 * 
 * Implements Requirement 28.7: Provides lock-free read access to
 * the error state for quick queries.
 * 
 * @return Current error state
 */
bool FLACDemuxer::getAtomicError() const
{
    return m_atomic_error.load(std::memory_order_acquire);
}

} // namespace FLAC
} // namespace Demuxer
} // namespace PsyMP3
