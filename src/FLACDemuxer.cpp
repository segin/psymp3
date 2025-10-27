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
    Debug::log("flac", "[FLACDemuxer] Constructor called");
    
    // Initialize state (no locks needed during construction)
    m_container_parsed = false;
    m_file_size = 0;
    m_audio_data_offset = 0;
    m_current_offset = 0;
    m_current_sample.store(0);
    m_last_block_size = 0;
    m_memory_usage_bytes = 0;
    m_error_state.store(false);
    
    // Initialize performance optimization state
    m_seek_table_sorted = false;
    m_last_seek_position = 0;
    m_is_network_stream = false;
    
    // Initialize frame indexing system
    m_frame_indexing_enabled = true;
    m_initial_indexing_complete = false;
    m_frames_indexed_during_parsing = 0;
    m_frames_indexed_during_playback = 0;
    
    // Initialize memory-efficient buffers
    initializeBuffers();
    
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
    Debug::log("flac", "[~FLACDemuxer] Destructor called");
    
    // Set error state to prevent new operations during destruction
    m_error_state.store(true);
    
    // Acquire locks to ensure no operations are in progress
    std::lock_guard<std::mutex> state_lock(m_state_mutex);
    std::lock_guard<std::mutex> metadata_lock(m_metadata_mutex);
    
    // Free all allocated memory
    freeUnusedMemory();
    
    // Clear metadata containers
    m_seektable.clear();
    m_vorbis_comments.clear();
    m_pictures.clear();
    
    // Clear reusable buffers
    m_frame_buffer.clear();
    m_frame_buffer.shrink_to_fit();
    m_sync_buffer.clear();
    m_sync_buffer.shrink_to_fit();
    
    // Base class destructor will handle IOHandler cleanup
}

// ============================================================================
// FLACFrameIndex Implementation
// ============================================================================

bool FLACFrameIndex::addFrame(const FLACFrameIndexEntry& entry)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!entry.isValid()) {
        return false;
    }
    
    // Check memory limits
    if (m_entries.size() >= MAX_INDEX_ENTRIES) {
        return false;
    }
    
    if (getMemoryUsage() >= MEMORY_LIMIT_BYTES) {
        return false;
    }
    
    // Check if we should add this entry based on granularity
    if (!shouldAddEntry(entry)) {
        return false;
    }
    
    // Add the entry
    m_entries.push_back(entry);
    
    // Keep entries sorted by sample offset
    if (m_entries.size() > 1 && 
        m_entries[m_entries.size() - 1].sample_offset < m_entries[m_entries.size() - 2].sample_offset) {
        ensureSorted();
    }
    
    return true;
}

const FLACFrameIndexEntry* FLACFrameIndex::findBestEntry(uint64_t target_sample) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_entries.empty()) {
        return nullptr;
    }
    
    // Binary search for the best entry (closest but not exceeding target)
    auto it = std::upper_bound(m_entries.begin(), m_entries.end(), target_sample,
        [](uint64_t sample, const FLACFrameIndexEntry& entry) {
            return sample < entry.sample_offset;
        });
    
    if (it == m_entries.begin()) {
        // Target is before first entry, return first entry
        return &m_entries[0];
    }
    
    // Return the entry just before the upper bound
    --it;
    return &(*it);
}

const FLACFrameIndexEntry* FLACFrameIndex::findContainingEntry(uint64_t target_sample) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_entries.empty()) {
        return nullptr;
    }
    
    // Linear search for containing entry (could be optimized with binary search)
    for (const auto& entry : m_entries) {
        if (entry.containsSample(target_sample)) {
            return &entry;
        }
    }
    
    return nullptr;
}

void FLACFrameIndex::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
    m_entries.shrink_to_fit();
}

size_t FLACFrameIndex::getMemoryUsage() const
{
    return m_entries.size() * sizeof(FLACFrameIndexEntry) + 
           m_entries.capacity() * sizeof(FLACFrameIndexEntry);
}

bool FLACFrameIndex::shouldAddEntry(const FLACFrameIndexEntry& entry) const
{
    if (m_entries.empty()) {
        return true;  // Always add first entry
    }
    
    // Check granularity - don't add entries too close together
    return checkGranularity(entry);
}

FLACFrameIndex::IndexStats FLACFrameIndex::getStats() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    IndexStats stats;
    stats.entry_count = m_entries.size();
    stats.memory_usage = getMemoryUsage();
    
    if (m_entries.empty()) {
        return stats;
    }
    
    stats.first_sample = m_entries.front().sample_offset;
    stats.last_sample = m_entries.back().sample_offset + m_entries.back().block_size;
    stats.total_samples_covered = stats.last_sample - stats.first_sample;
    
    // Calculate coverage percentage (rough estimate)
    uint64_t indexed_samples = 0;
    for (const auto& entry : m_entries) {
        indexed_samples += entry.block_size;
    }
    
    if (stats.total_samples_covered > 0) {
        stats.coverage_percentage = (static_cast<double>(indexed_samples) / stats.total_samples_covered) * 100.0;
    }
    
    return stats;
}

void FLACFrameIndex::ensureSorted()
{
    std::sort(m_entries.begin(), m_entries.end(),
        [](const FLACFrameIndexEntry& a, const FLACFrameIndexEntry& b) {
            return a.sample_offset < b.sample_offset;
        });
}

bool FLACFrameIndex::checkGranularity(const FLACFrameIndexEntry& entry) const
{
    if (m_entries.empty()) {
        return true;
    }
    
    // Find the closest existing entry
    uint64_t min_distance = UINT64_MAX;
    for (const auto& existing : m_entries) {
        uint64_t distance = (entry.sample_offset > existing.sample_offset) ?
                           (entry.sample_offset - existing.sample_offset) :
                           (existing.sample_offset - entry.sample_offset);
        min_distance = std::min(min_distance, distance);
    }
    
    // Only add if far enough from existing entries
    return min_distance >= INDEX_GRANULARITY_SAMPLES;
}

bool FLACDemuxer::parseContainer()
{
    std::lock_guard<std::mutex> state_lock(m_state_mutex);
    std::lock_guard<std::mutex> metadata_lock(m_metadata_mutex);
    return parseContainer_unlocked();
}

bool FLACDemuxer::parseContainer_unlocked()
{
    Debug::log("flac", "[parseContainer_unlocked] Starting FLAC container parsing");
    
    if (m_error_state.load()) {
        Debug::log("flac", "[parseContainer_unlocked] Demuxer in error state, cannot parse container");
        return false;
    }
    
    if (!m_handler) {
        reportError("IO", "No IOHandler available for parsing");
        setErrorState(true);
        return false;
    }
    
    if (m_container_parsed) {
        Debug::log("flac", "[parseContainer_unlocked] Container already parsed");
        return true;
    }
    
    // Seek to beginning of file with error recovery
    if (m_handler->seek(0, SEEK_SET) != 0) {
        int io_error = m_handler->getLastError();
        reportError("IO", "Failed to seek to beginning of file (error: " + std::to_string(io_error) + ")");
        return false;
    }
    
    // Validate fLaC stream marker (4 bytes) with enhanced error handling
    uint8_t marker[4];
    size_t bytes_read = m_handler->read(marker, 1, 4);
    
    if (bytes_read == 0) {
        int io_error = m_handler->getLastError();
        if (io_error != 0) {
            reportError("IO", "Read error while reading FLAC stream marker (error: " + std::to_string(io_error) + ")");
        } else {
            reportError("IO", "Empty file - cannot read FLAC stream marker");
        }
        return false;
    }
    
    if (bytes_read < 4) {
        reportError("Format", "File too small to be a valid FLAC file (only " + 
                   std::to_string(bytes_read) + " bytes available)");
        return false;
    }
    
    // Enhanced fLaC marker validation with detailed error reporting
    if (marker[0] != 'f' || marker[1] != 'L' || marker[2] != 'a' || marker[3] != 'C') {
        // Provide helpful error message based on what we found
        std::string found_marker(reinterpret_cast<char*>(marker), 4);
        
        // Check for common misidentifications
        if (marker[0] == 'I' && marker[1] == 'D' && marker[2] == '3') {
            reportError("Format", "File appears to be MP3 with ID3 tag, not FLAC");
        } else if (marker[0] == 'O' && marker[1] == 'g' && marker[2] == 'g' && marker[3] == 'S') {
            reportError("Format", "File appears to be Ogg container, not native FLAC");
        } else if (marker[0] == 'R' && marker[1] == 'I' && marker[2] == 'F' && marker[3] == 'F') {
            reportError("Format", "File appears to be RIFF/WAV container, not FLAC");
        } else if (std::isprint(marker[0]) && std::isprint(marker[1]) && 
                   std::isprint(marker[2]) && std::isprint(marker[3])) {
            reportError("Format", "Invalid FLAC stream marker '" + found_marker + 
                       "' - not a FLAC file");
        } else {
            reportError("Format", "Invalid FLAC stream marker (binary data) - not a FLAC file");
        }
        return false;
    }
    
    Debug::log("flac", "[parseContainer_unlocked] Valid fLaC stream marker found");
    
    // Parse metadata blocks with enhanced error recovery
    bool metadata_parse_success = false;
    try {
        metadata_parse_success = parseMetadataBlocks();
    } catch (const std::exception& e) {
        reportError("Format", "Exception during metadata parsing: " + std::string(e.what()));
        metadata_parse_success = false;
    } catch (...) {
        reportError("Format", "Unknown exception during metadata parsing");
        metadata_parse_success = false;
    }
    
    if (!metadata_parse_success) {
        // Try to recover by providing reasonable defaults
        Debug::log("flac", "[parseContainer_unlocked] Metadata parsing failed, attempting recovery with defaults");
        
        if (!m_streaminfo.isValid()) {
            // Try to derive STREAMINFO from first frame if possible
            if (attemptStreamInfoRecovery()) {
                Debug::log("flac", "[parseContainer_unlocked] Successfully recovered STREAMINFO from first frame");
            } else {
                reportError("Format", "Failed to parse FLAC metadata blocks and cannot recover");
                return false;
            }
        }
    }
    
    // Verify we have valid STREAMINFO (either parsed or recovered)
    if (!m_streaminfo.isValid()) {
        reportError("Format", "Missing or invalid STREAMINFO block - cannot proceed");
        return false;
    }
    
    // Validate STREAMINFO parameters for reasonableness
    if (!validateStreamInfoParameters()) {
        reportError("Format", "STREAMINFO contains invalid parameters");
        return false;
    }
    
    // Container parsing successful
    m_container_parsed = true;
    
    // Apply memory optimizations after parsing
    optimizeSeekTable();
    limitVorbisComments();
    limitPictureStorage();
    
    // Apply performance optimizations
    optimizeForNetworkStreaming();
    optimizeFrameProcessingPerformance();
    
    // Calculate and log memory usage
    m_memory_usage_bytes = calculateMemoryUsage();
    Debug::log("flac", "[parseContainer_unlocked] Memory usage after parsing: ", m_memory_usage_bytes, " bytes");
    
    // Initialize position tracking to start of stream
    resetPositionTracking();
    
    // TEMPORARILY DISABLED: Initial frame indexing causes infinite loop
    // TODO: Fix frame boundary detection before re-enabling
    if (false && m_frame_indexing_enabled) {
        Debug::log("flac", "[parseContainer_unlocked] Starting initial frame indexing");
        if (performInitialFrameIndexing()) {
            Debug::log("flac", "[parseContainer_unlocked] Initial frame indexing completed successfully");
            auto stats = m_frame_index.getStats();
            Debug::log("flac", "[parseContainer_unlocked] Frame index stats: ", stats.entry_count, 
                      " entries, ", stats.memory_usage, " bytes, covering ", stats.total_samples_covered, " samples");
        } else {
            Debug::log("flac", "[parseContainer_unlocked] Initial frame indexing failed, but continuing");
        }
    } else {
        Debug::log("flac", "[parseContainer_unlocked] Initial frame indexing disabled to prevent infinite loop");
    }
    
    Debug::log("flac", "[parseContainer_unlocked] FLAC container parsing completed successfully");
    Debug::log("flac", "[parseContainer_unlocked] Audio data starts at offset: ", m_audio_data_offset);
    
    return true;
}

std::vector<StreamInfo> FLACDemuxer::getStreams() const
{
    std::lock_guard<std::mutex> state_lock(m_state_mutex);
    std::lock_guard<std::mutex> metadata_lock(m_metadata_mutex);
    return getStreams_unlocked();
}

std::vector<StreamInfo> FLACDemuxer::getStreams_unlocked() const
{
    Debug::log("flac", "[getStreams_unlocked] Returning FLAC stream info");
    
    if (m_error_state.load()) {
        Debug::log("flac", "[getStreams_unlocked] Demuxer in error state, returning empty stream list");
        return {};
    }
    
    if (!m_container_parsed) {
        Debug::log("flac", "[getStreams_unlocked] Container not parsed, returning empty stream list");
        return {};
    }
    
    if (!m_streaminfo.isValid()) {
        Debug::log("flac", "[getStreams_unlocked] Invalid STREAMINFO, returning empty stream list");
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
    std::lock_guard<std::mutex> state_lock(m_state_mutex);
    std::lock_guard<std::mutex> metadata_lock(m_metadata_mutex);
    return getStreamInfo_unlocked(stream_id);
}

StreamInfo FLACDemuxer::getStreamInfo_unlocked(uint32_t stream_id) const
{
    Debug::log("flac", "[getStreamInfo_unlocked] Returning FLAC stream info for stream_id: ", stream_id);
    
    if (m_error_state.load()) {
        Debug::log("flac", "[getStreamInfo_unlocked] Demuxer in error state");
        return StreamInfo{};
    }
    
    if (!m_container_parsed) {
        Debug::log("flac", "[getStreamInfo_unlocked] Container not parsed");
        return StreamInfo{};
    }
    
    if (stream_id != 1) {
        Debug::log("flac", "[getStreamInfo_unlocked] Invalid stream ID for FLAC: ", stream_id);
        return StreamInfo{};
    }
    
    auto streams = getStreams_unlocked();
    if (streams.empty()) {
        return StreamInfo{};
    }
    
    return streams[0];
}

MediaChunk FLACDemuxer::readChunk()
{
    std::lock_guard<std::mutex> state_lock(m_state_mutex);
    return readChunk_unlocked();
}

MediaChunk FLACDemuxer::readChunk_unlocked()
{
    Debug::log("flac", "[readChunk_unlocked] Reading next FLAC frame");
    
    if (m_error_state.load()) {
        Debug::log("flac", "[readChunk_unlocked] Demuxer in error state");
        return MediaChunk{};
    }
    
    if (!m_container_parsed) {
        Debug::log("flac", "[readChunk_unlocked] Container not parsed");
        reportError("State", "Container not parsed");
        setErrorState(true);
        return MediaChunk{};
    }
    
    if (isEOF_unlocked()) {
        Debug::log("flac", "[readChunk_unlocked] At end of file");
        return MediaChunk{};
    }
    
    // Stream-based approach: provide sequential FLAC data to libFLAC (like fread)
    // This matches how libFLAC expects to receive data in file_read_callback_
    
    Debug::log("flac", "[readChunk_unlocked] Reading sequential FLAC data from offset ", m_current_offset);
    
    // Read a reasonable chunk size - libFLAC will handle frame parsing
    uint32_t chunk_size = 8192;  // 8KB chunks work well
    
    // Check if we have enough data left in file
    if (m_file_size > 0) {
        uint64_t bytes_available = m_file_size - m_current_offset;
        if (bytes_available == 0) {
            Debug::log("flac", "[readChunk_unlocked] Reached end of file");
            return MediaChunk{};
        }
        if (chunk_size > bytes_available) {
            chunk_size = static_cast<uint32_t>(bytes_available);
            Debug::log("flac", "[readChunk_unlocked] Limited chunk size to available data: ", chunk_size, " bytes");
        }
    }
    
    if (chunk_size < 10) {
        Debug::log("flac", "[readChunk_unlocked] Insufficient data available");
        return MediaChunk{};
    }
    
    // Seek to current position
    if (m_handler->seek(static_cast<off_t>(m_current_offset), SEEK_SET) != 0) {
        reportError("IO", "Failed to seek to position: " + std::to_string(m_current_offset));
        return MediaChunk{};
    }
    
    // Ensure buffer capacity
    if (!ensureBufferCapacity(m_frame_buffer, chunk_size)) {
        reportError("Memory", "Failed to allocate buffer of size " + std::to_string(chunk_size));
        return MediaChunk{};
    }
    
    // Read the chunk data (like fread in libFLAC)
    size_t bytes_read = m_handler->read(m_frame_buffer.data(), 1, chunk_size);
    if (bytes_read == 0) {
        Debug::log("flac", "[readChunk_unlocked] No data read - likely end of file");
        return MediaChunk{};
    }
    
    Debug::log("flac", "[readChunk_unlocked] Read ", bytes_read, " bytes from position ", m_current_offset);
    
    // Copy the chunk data
    std::vector<uint8_t> chunk_data(bytes_read);
    std::memcpy(chunk_data.data(), m_frame_buffer.data(), bytes_read);
    
    // Create MediaChunk with sequential data
    MediaChunk chunk(1, std::move(chunk_data));  // stream_id = 1 for FLAC
    chunk.timestamp_samples = m_current_sample.load();  // Use current sample position
    chunk.is_keyframe = true;  // All FLAC data is independent
    chunk.file_offset = m_current_offset;
    
    // Advance position by the amount we actually read (like fread)
    m_current_offset += bytes_read;
    
    Debug::log("flac", "[readChunk_unlocked] Successfully read FLAC chunk: ", bytes_read, " bytes, next position: ", m_current_offset);
    
    return chunk;
}

MediaChunk FLACDemuxer::readChunk(uint32_t stream_id)
{
    std::lock_guard<std::mutex> state_lock(m_state_mutex);
    return readChunk_unlocked(stream_id);
}

MediaChunk FLACDemuxer::readChunk_unlocked(uint32_t stream_id)
{
    Debug::log("flac", "[readChunk_unlocked] Reading chunk for stream_id: ", stream_id);
    
    if (m_error_state.load()) {
        Debug::log("flac", "[readChunk_unlocked] Demuxer in error state");
        return MediaChunk{};
    }
    
    if (stream_id != 1) {
        reportError("Stream", "Invalid stream ID for FLAC: " + std::to_string(stream_id));
        setErrorState(true);
        return MediaChunk{};
    }
    
    return readChunk_unlocked();
}

bool FLACDemuxer::seekTo(uint64_t timestamp_ms)
{
    std::lock_guard<std::mutex> state_lock(m_state_mutex);
    return seekTo_unlocked(timestamp_ms);
}

bool FLACDemuxer::seekTo_unlocked(uint64_t timestamp_ms)
{
    Debug::log("flac", "[seekTo_unlocked] Seeking to timestamp: ", timestamp_ms, " ms");
    
    if (m_error_state.load()) {
        Debug::log("flac", "[seekTo_unlocked] Demuxer in error state");
        return false;
    }
    
    if (!m_container_parsed) {
        reportError("State", "Container not parsed");
        setErrorState(true);
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
        Debug::log("flac", "[seekTo_unlocked] Seeking to beginning of stream");
        resetPositionTracking();
        
        // Seek to beginning of audio data
        if (m_handler->seek(static_cast<off_t>(m_audio_data_offset), SEEK_SET) != 0) {
            reportError("IO", "Failed to seek to beginning of audio data");
            return false;
        }
        
        m_current_offset = m_audio_data_offset;
        m_current_sample.store(0);
        
        return true;
    }
    
    // Validate target sample is within stream bounds
    if (m_streaminfo.total_samples > 0 && target_sample >= m_streaminfo.total_samples) {
        Debug::log("flac", "[seekTo_unlocked] Seek target (", target_sample, ") beyond stream end (", 
                  m_streaminfo.total_samples, "), clamping");
        target_sample = m_streaminfo.total_samples - 1;
        timestamp_ms = samplesToMs(target_sample);
    }
    
    Debug::log("flac", "[seekTo_unlocked] Seeking to sample ", target_sample, " (", timestamp_ms, " ms)");
    
    // Choose seeking strategy based on available metadata and indexing
    // Priority 1: Frame index (most accurate for compressed streams)
    if (m_frame_indexing_enabled && !m_frame_index.empty()) {
        Debug::log("flac", "[seekTo_unlocked] Using frame index for seeking (preferred method)");
        if (seekWithIndex(target_sample)) {
            return true;
        }
        Debug::log("flac", "[seekTo_unlocked] Frame index seeking failed, trying fallback methods");
    }
    
    // Priority 2: SEEKTABLE (fast but less accurate)
    if (!m_seektable.empty()) {
        Debug::log("flac", "[seekTo_unlocked] Using SEEKTABLE for seeking");
        if (seekWithTable(target_sample)) {
            return true;
        }
        Debug::log("flac", "[seekTo_unlocked] SEEKTABLE seeking failed, trying fallback methods");
    }
    
    // Priority 3: Binary search (limited effectiveness with compressed streams)
    Debug::log("flac", "[seekTo_unlocked] Using binary search for seeking (limited effectiveness expected)");
    if (seekBinary(target_sample)) {
        return true;
    }
    
    // Priority 4: Linear search (most reliable but slowest)
    Debug::log("flac", "[seekTo_unlocked] Using linear search for seeking (fallback method)");
    return seekLinear(target_sample);
    m_current_offset = m_audio_data_offset;  // Keep at start of audio data
    
    bool seek_success = true;
    
    // Try different seeking strategies in order of preference (commented out for now)
    /*
    bool seek_success = false;
    
    // Strategy 1: Use seek table if available
    if (!m_seektable.empty()) {
        Debug::log("flac", "[seekTo_unlocked] Attempting seek table based seeking");
        seek_success = seekWithTable(target_sample);
    }
    
    // Strategy 2: Binary search through frames (not implemented yet)
    if (!seek_success) {
        Debug::log("flac", "[seekTo_unlocked] Attempting binary search seeking");
        seek_success = seekBinary(target_sample);
    }
    
    // Strategy 3: Linear search from current or beginning (not implemented yet)
    if (!seek_success) {
        Debug::log("flac", "[seekTo_unlocked] Attempting linear seeking");
        seek_success = seekLinear(target_sample);
    }
    */
    
    if (seek_success) {
        // Track successful seek position for optimization
        m_last_seek_position = target_sample;
        
        uint64_t current_sample = m_current_sample.load();
        Debug::log("flac", "[seekTo_unlocked] Seek successful to sample ", current_sample, 
                  " (", samplesToMs(current_sample), " ms)");
        return true;
    } else {
        reportError("Seek", "All seeking strategies failed for timestamp " + std::to_string(timestamp_ms));
        return false;
    }
}

bool FLACDemuxer::isEOF() const
{
    std::lock_guard<std::mutex> state_lock(m_state_mutex);
    return isEOF_unlocked();
}

bool FLACDemuxer::isEOF_unlocked() const
{
    if (m_error_state.load()) {
        return true;
    }
    
    if (!m_handler) {
        return true;
    }
    
    return m_handler->eof() || (m_file_size > 0 && m_current_offset >= m_file_size);
}

uint64_t FLACDemuxer::getDuration() const
{
    std::lock_guard<std::mutex> metadata_lock(m_metadata_mutex);
    return getDuration_unlocked();
}

uint64_t FLACDemuxer::getDuration_unlocked() const
{
    Debug::log("flac", "[getDuration_unlocked] Calculating duration");
    
    if (m_error_state.load()) {
        Debug::log("flac", "[getDuration_unlocked] Demuxer in error state, cannot determine duration");
        return 0;
    }
    
    if (!m_container_parsed) {
        Debug::log("flac", "[getDuration_unlocked] Container not parsed, cannot determine duration");
        return 0;
    }
    
    // Primary method: Use total samples from STREAMINFO
    if (m_streaminfo.isValid() && m_streaminfo.total_samples > 0 && m_streaminfo.sample_rate > 0) {
        // Use 64-bit arithmetic to prevent overflow for very long files
        // Calculate: (total_samples * 1000) / sample_rate
        // But do it safely to avoid overflow
        uint64_t duration_ms = (m_streaminfo.total_samples * 1000ULL) / m_streaminfo.sample_rate;
        
        Debug::log("flac", "[getDuration_unlocked] Duration from STREAMINFO: ", duration_ms, " ms (", 
                  m_streaminfo.total_samples, " samples at ", m_streaminfo.sample_rate, " Hz)");
        return duration_ms;
    }
    
    // Fallback method: Estimate from file size and average bitrate
    if (m_file_size > 0 && m_streaminfo.isValid() && m_streaminfo.sample_rate > 0) {
        Debug::log("flac", "[getDuration_unlocked] STREAMINFO incomplete, estimating duration from file size");
        
        // Calculate approximate bitrate based on format parameters
        // Uncompressed bitrate = sample_rate * channels * bits_per_sample
        uint64_t uncompressed_bitrate = static_cast<uint64_t>(m_streaminfo.sample_rate) * 
                                       m_streaminfo.channels * 
                                       m_streaminfo.bits_per_sample;
        
        if (uncompressed_bitrate == 0) {
            Debug::log("flac", "[getDuration_unlocked] Cannot calculate bitrate, insufficient STREAMINFO");
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
            
            Debug::log("flac", "[getDuration_unlocked] Estimated duration from file size: ", duration_ms, " ms");
            Debug::log("flac", "[getDuration_unlocked] File size: ", m_file_size, " bytes");
            Debug::log("flac", "[getDuration_unlocked] Audio data size: ", audio_data_size, " bytes");
            Debug::log("flac", "[getDuration_unlocked] Estimated bitrate: ", estimated_compressed_bitrate, " bps");
            
            return duration_ms;
        }
    }
    
    // No reliable way to determine duration
    Debug::log("flac", "[getDuration_unlocked] Cannot determine duration - insufficient information");
    return 0;
}

uint64_t FLACDemuxer::getPosition() const
{
    std::lock_guard<std::mutex> metadata_lock(m_metadata_mutex);
    return getPosition_unlocked();
}

uint64_t FLACDemuxer::getPosition_unlocked() const
{
    Debug::log("flac", "[getPosition_unlocked] Returning current position in milliseconds");
    
    if (m_error_state.load()) {
        Debug::log("flac", "Demuxer in error state");
        return 0;
    }
    
    if (!m_container_parsed || !m_streaminfo.isValid()) {
        Debug::log("flac", "[getPosition_unlocked] Container not parsed or invalid STREAMINFO");
        return 0;
    }
    
    // Convert current sample position to milliseconds (atomic read)
    uint64_t current_sample = m_current_sample.load();
    uint64_t position_ms = samplesToMs(current_sample);
    Debug::log("flac", "[getPosition_unlocked] Current position: ", current_sample, " samples = ", position_ms, " ms");
    
    return position_ms;
}

uint64_t FLACDemuxer::getCurrentSample() const
{
    std::lock_guard<std::mutex> state_lock(m_state_mutex);
    return getCurrentSample_unlocked();
}

uint64_t FLACDemuxer::getCurrentSample_unlocked() const
{
    Debug::log("flac", "[getCurrentSample_unlocked] Returning current position in samples");
    
    if (m_error_state.load()) {
        Debug::log("flac", "[getCurrentSample_unlocked] Demuxer in error state");
        return 0;
    }
    
    if (!m_container_parsed) {
        Debug::log("flac", "[getCurrentSample_unlocked] Container not parsed");
        return 0;
    }
    
    uint64_t current_sample = m_current_sample.load();
    Debug::log("flac", "[getCurrentSample_unlocked] Current sample position: ", current_sample);
    return current_sample;
}

// Private helper methods - implementations

uint32_t FLACDemuxer::calculateFrameSize(const FLACFrame& frame)
{
    std::lock_guard<std::mutex> metadata_lock(m_metadata_mutex);
    return calculateFrameSize_unlocked(frame);
}

uint32_t FLACDemuxer::calculateFrameSize_unlocked(const FLACFrame& frame)
{
    Debug::log("flac", "[calculateFrameSize_unlocked] Calculating frame size for ", frame.block_size, 
              " samples, ", frame.channels, " channels, ", frame.bits_per_sample, " bits per sample");
    
    // Priority 1: Use STREAMINFO minimum frame size if available
    if (m_streaminfo.isValid() && m_streaminfo.min_frame_size > 0) {
        uint32_t streaminfo_min = m_streaminfo.min_frame_size;
        
        // Validate against STREAMINFO max frame size if available
        if (m_streaminfo.max_frame_size > 0 && streaminfo_min > m_streaminfo.max_frame_size) {
            Debug::log("flac", "[calculateFrameSize_unlocked] Warning: min_frame_size > max_frame_size, using max");
            streaminfo_min = m_streaminfo.max_frame_size;
        }
        
        Debug::log("flac", "[calculateFrameSize_unlocked] Using STREAMINFO minimum frame size: ", streaminfo_min, " bytes");
        return streaminfo_min;
    }
    
    // Priority 2: Estimate based on audio format parameters
    if (frame.isValid()) {
        // Calculate theoretical minimum size based on frame structure
        // FLAC frame header: ~4-16 bytes (variable due to UTF-8 encoding)
        // Subframe headers: ~1-4 bytes per channel
        // Compressed audio data: highly variable, use conservative estimate
        
        uint32_t header_size = 8;  // Conservative frame header estimate
        uint32_t subframe_headers = frame.channels * 2;  // Conservative subframe header estimate
        
        // Estimate compressed data size based on compression efficiency
        // For very small block sizes, compression is less effective
        uint32_t samples_per_channel = frame.block_size;
        uint32_t bytes_per_sample = (frame.bits_per_sample + 7) / 8;  // Round up to byte boundary
        
        // Estimate compression ratio based on block size
        double compression_ratio;
        if (samples_per_channel <= 256) {
            compression_ratio = 0.8;  // Less compression for small blocks
        } else if (samples_per_channel <= 1024) {
            compression_ratio = 0.6;  // Moderate compression
        } else {
            compression_ratio = 0.4;  // Better compression for larger blocks
        }
        
        uint32_t uncompressed_size = samples_per_channel * frame.channels * bytes_per_sample;
        uint32_t estimated_compressed_size = static_cast<uint32_t>(uncompressed_size * compression_ratio);
        
        uint32_t total_estimate = header_size + subframe_headers + estimated_compressed_size;
        
        // Apply minimum and maximum bounds for safety
        uint32_t min_bound = 32;   // Absolute minimum for any FLAC frame
        uint32_t max_bound = 64 * 1024;  // Conservative maximum (64KB)
        
        total_estimate = std::max(min_bound, std::min(max_bound, total_estimate));
        
        Debug::log("flac", "[calculateFrameSize_unlocked] Estimated frame size: ", total_estimate, 
                  " bytes (header: ", header_size, ", subframes: ", subframe_headers, 
                  ", compressed data: ", estimated_compressed_size, ")");
        
        return total_estimate;
    }
    
    // Priority 3: Conservative fallback for invalid frame data
    uint32_t conservative_estimate = 64;  // Safe minimum
    
    Debug::log("flac", "[calculateFrameSize_unlocked] Using conservative fallback estimate: ", conservative_estimate, " bytes");
    
    return conservative_estimate;
}


bool FLACDemuxer::getNextFrameFromSeekTable(FLACFrame& frame)
{
    Debug::log("flac", "[getNextFrameFromSeekTable] Getting next frame from SEEKTABLE");
    
    if (m_seektable.empty()) {
        Debug::log("flac", "[getNextFrameFromSeekTable] No SEEKTABLE available, falling back to findNextFrame_unlocked");
        return findNextFrame_unlocked(frame);
    }
    
    // Find the next seek point based on current sample position
    uint64_t current_sample = m_current_sample.load();
    
    for (const auto& seek_point : m_seektable) {
        if (seek_point.sample_number >= current_sample) {
            // Found the next frame
            frame.file_offset = m_audio_data_offset + seek_point.stream_offset;
            frame.sample_offset = seek_point.sample_number;
            frame.block_size = seek_point.frame_samples;
            frame.frame_size = 0;  // Will be determined when reading
            
            // Set other frame properties from STREAMINFO
            if (m_streaminfo.isValid()) {
                frame.sample_rate = m_streaminfo.sample_rate;
                frame.channels = m_streaminfo.channels;
                frame.bits_per_sample = m_streaminfo.bits_per_sample;
                frame.variable_block_size = (m_streaminfo.min_block_size != m_streaminfo.max_block_size);
            }
            
            Debug::log("flac", "[getNextFrameFromSeekTable] Found frame: sample=", frame.sample_offset, 
                      " offset=", frame.file_offset, " block_size=", frame.block_size);
            
            return true;
        }
    }
    
    Debug::log("flac", "[getNextFrameFromSeekTable] No more frames in SEEKTABLE");
    return false;
}

uint32_t FLACDemuxer::findFrameEnd(const uint8_t* buffer, uint32_t buffer_size)
{
    Debug::log("flac", "[findFrameEnd] Searching for frame end in buffer of size ", buffer_size);
    
    if (!buffer || buffer_size < 4) {
        Debug::log("flac", "[findFrameEnd] Invalid buffer or insufficient size");
        return 0;
    }
    
    // Search for the next sync pattern starting from offset 4 (skip current frame's sync)
    // We need to find the next frame's sync pattern to determine where current frame ends
    for (uint32_t i = 4; i < buffer_size - 1; ++i) {
        if (validateFrameSync_unlocked(buffer + i, buffer_size - i)) {
            Debug::log("flac", "[findFrameEnd] Found next frame sync at offset ", i);
            return i;  // This is where the current frame ends
        }
    }
    
    Debug::log("flac", "[findFrameEnd] No frame end found in buffer");
    return 0;
}

uint32_t FLACDemuxer::findFrameEndFromFile(uint64_t frame_start_offset)
{
    std::lock_guard<std::mutex> state_lock(m_state_mutex);
    return findFrameEndFromFile_unlocked(frame_start_offset);
}

uint32_t FLACDemuxer::findFrameEndFromFile_unlocked(uint64_t frame_start_offset)
{
    Debug::log("flac", "[findFrameEndFromFile_unlocked] Finding frame end starting from offset ", frame_start_offset);
    
    if (!m_handler) {
        Debug::log("flac", "[findFrameEndFromFile_unlocked] No IOHandler available");
        return 0;
    }
    
    // Read a reasonable chunk to search for the next frame
    const uint32_t search_buffer_size = 32768;  // 32KB should cover most frame sizes
    
    // Ensure buffer capacity
    if (!ensureBufferCapacity(m_frame_buffer, search_buffer_size)) {
        Debug::log("flac", "[findFrameEndFromFile_unlocked] Failed to allocate search buffer");
        return 0;
    }
    
    // Seek to frame start
    if (m_handler->seek(static_cast<off_t>(frame_start_offset), SEEK_SET) != 0) {
        Debug::log("flac", "[findFrameEndFromFile_unlocked] Failed to seek to frame start");
        return 0;
    }
    
    // Read data for searching
    size_t bytes_read = m_handler->read(m_frame_buffer.data(), 1, search_buffer_size);
    if (bytes_read < 20) {  // Need at least 20 bytes to find next frame
        Debug::log("flac", "[findFrameEndFromFile_unlocked] Insufficient data for search: ", bytes_read, " bytes");
        return 0;
    }
    
    // Use the existing findFrameEnd method to search within the buffer
    uint32_t frame_end_offset = findFrameEnd(m_frame_buffer.data(), static_cast<uint32_t>(bytes_read));
    
    if (frame_end_offset > 0) {
        Debug::log("flac", "[findFrameEndFromFile_unlocked] Found frame end at relative offset ", frame_end_offset);
        return frame_end_offset;  // This is the frame size
    }
    
    // If we couldn't find the end, use a conservative estimate
    if (m_streaminfo.isValid() && m_streaminfo.max_frame_size > 0) {
        uint32_t estimated_size = m_streaminfo.max_frame_size;
        Debug::log("flac", "[findFrameEndFromFile_unlocked] Using max frame size estimate: ", estimated_size, " bytes");
        return estimated_size;
    }
    
    Debug::log("flac", "[findFrameEndFromFile_unlocked] Could not determine frame size");
    return 0;
}

bool FLACDemuxer::parseMetadataBlockHeader(FLACMetadataBlock& block)
{
    Debug::log("flac", "[parseMetadataBlockHeader] Parsing block header");
    
    if (!m_handler) {
        return false;
    }
    
    // Read 4-byte metadata block header
    uint8_t header[4];
    if (m_handler->read(header, 1, 4) != 4) {
        Debug::log("flac", "[parseMetadataBlockHeader] Failed to read metadata block header");
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
    
    Debug::log("flac", "[parseMetadataBlockHeader] Metadata block: type=", static_cast<int>(block.type), 
               " is_last=", block.is_last, " length=", block.length, 
               " data_offset=", block.data_offset);
    
    return true;
}

bool FLACDemuxer::parseMetadataBlocks()
{
    Debug::log("flac", "[parseMetadataBlocks] Starting metadata blocks parsing");
    
    if (!m_handler) {
        reportError("IO", "No IOHandler available for metadata parsing");
        return false;
    }
    
    bool found_streaminfo = false;
    bool is_last_block = false;
    int blocks_parsed = 0;
    int blocks_skipped = 0;
    const int max_metadata_blocks = 1000;  // Prevent infinite loops
    
    while (!is_last_block && !m_handler->eof() && blocks_parsed < max_metadata_blocks) {
        FLACMetadataBlock block;
        
        // Parse metadata block header with error recovery
        if (!parseMetadataBlockHeader(block)) {
            Debug::log("flac", "[parseMetadataBlocks] Failed to parse metadata block header at block ", blocks_parsed);
            
            // Try to recover by searching for next valid block or audio data
            if (recoverFromCorruptedMetadata()) {
                Debug::log("flac", "[parseMetadataBlocks] Recovered from corrupted metadata, stopping metadata parsing");
                break;
            } else {
                reportError("Format", "Failed to parse metadata block header and cannot recover");
                return false;
            }
        }
        
        blocks_parsed++;
        is_last_block = block.is_last;
        
        // Validate block structure and size limits
        if (!block.isValid()) {
            Debug::log("flac", "[parseMetadataBlocks] Invalid metadata block ", blocks_parsed, ", attempting to skip");
            blocks_skipped++;
            
            if (!skipMetadataBlock(block)) {
                Debug::log("flac", "[parseMetadataBlocks] Failed to skip invalid metadata block, attempting recovery");
                
                // Try to find next valid block or audio data
                if (recoverFromCorruptedMetadata()) {
                    Debug::log("flac", "[parseMetadataBlocks] Recovered from invalid metadata block");
                    break;
                } else {
                    reportError("IO", "Failed to skip invalid metadata block and cannot recover");
                    return false;
                }
            }
            continue;
        }
        
        // Check for reasonable size limits (16MB max for any metadata block)
        if (block.length > 16 * 1024 * 1024) {
            Debug::log("flac", "[parseMetadataBlocks] Metadata block ", blocks_parsed, " too large (", 
                      block.length, " bytes), skipping");
            blocks_skipped++;
            
            if (!skipMetadataBlock(block)) {
                Debug::log("flac", "[parseMetadataBlocks] Failed to skip oversized metadata block, attempting recovery");
                
                if (recoverFromCorruptedMetadata()) {
                    Debug::log("flac", "[parseMetadataBlocks] Recovered from oversized metadata block");
                    break;
                } else {
                    reportError("IO", "Failed to skip oversized metadata block and cannot recover");
                    return false;
                }
            }
            continue;
        }
        
        // Process block based on type with enhanced error handling
        bool parse_success = false;
        const char* block_type_name = "Unknown";
        
        switch (block.type) {
            case FLACMetadataType::STREAMINFO:
                block_type_name = "STREAMINFO";
                Debug::log("flac", "[parseMetadataBlocks] Processing STREAMINFO block");
                parse_success = parseStreamInfoBlock_unlocked(block);
                if (parse_success) {
                    found_streaminfo = true;
                } else {
                    Debug::log("flac", "[parseMetadataBlocks] STREAMINFO parsing failed, this is critical");
                }
                break;
                
            case FLACMetadataType::SEEKTABLE:
                block_type_name = "SEEKTABLE";
                Debug::log("flac", "[parseMetadataBlocks] Processing SEEKTABLE block");
                parse_success = parseSeekTableBlock(block);
                if (!parse_success) {
                    Debug::log("flac", "[parseMetadataBlocks] SEEKTABLE parsing failed, seeking will be less efficient");
                }
                break;
                
            case FLACMetadataType::VORBIS_COMMENT:
                block_type_name = "VORBIS_COMMENT";
                Debug::log("flac", "[parseMetadataBlocks] Processing VORBIS_COMMENT block");
                parse_success = parseVorbisCommentBlock(block);
                if (!parse_success) {
                    Debug::log("flac", "[parseMetadataBlocks] VORBIS_COMMENT parsing failed, metadata will be unavailable");
                }
                break;
                
            case FLACMetadataType::PICTURE:
                block_type_name = "PICTURE";
                Debug::log("flac", "[parseMetadataBlocks] Processing PICTURE block");
                parse_success = parsePictureBlock(block);
                if (!parse_success) {
                    Debug::log("flac", "[parseMetadataBlocks] PICTURE parsing failed, artwork will be unavailable");
                    // Skip the failed PICTURE block by seeking to its end
                    off_t block_end = static_cast<off_t>(block.data_offset + block.length);
                    if (m_handler->seek(block_end, SEEK_SET)) {
                        Debug::log("flac", "[parseMetadataBlocks] Successfully skipped failed PICTURE block");
                        parse_success = true; // Continue parsing other blocks
                    } else {
                        Debug::log("flac", "[parseMetadataBlocks] Failed to skip PICTURE block, continuing anyway");
                        parse_success = true; // Non-fatal error, continue parsing
                    }
                }
                break;
                
            case FLACMetadataType::PADDING:
                block_type_name = "PADDING";
                Debug::log("flac", "[parseMetadataBlocks] Skipping PADDING block");
                parse_success = skipMetadataBlock(block);
                break;
                
            case FLACMetadataType::APPLICATION:
                block_type_name = "APPLICATION";
                Debug::log("flac", "[parseMetadataBlocks] Skipping APPLICATION block");
                parse_success = skipMetadataBlock(block);
                break;
                
            case FLACMetadataType::CUESHEET:
                block_type_name = "CUESHEET";
                Debug::log("flac", "[parseMetadataBlocks] Skipping CUESHEET block");
                parse_success = skipMetadataBlock(block);
                break;
                
            default:
                block_type_name = "Unknown";
                Debug::log("flac", "[parseMetadataBlocks] Skipping unknown metadata block type: ", static_cast<int>(block.type));
                parse_success = skipMetadataBlock(block);
                break;
        }
        
        if (!parse_success) {
            Debug::log("flac", "[parseMetadataBlocks] Failed to process ", block_type_name, " metadata block");
            blocks_skipped++;
            
            // For critical blocks (STREAMINFO), this is a serious error
            if (block.type == FLACMetadataType::STREAMINFO) {
                Debug::log("flac", "[parseMetadataBlocks] STREAMINFO block parsing failed, attempting recovery");
                
                // Try to skip this block and continue, we'll attempt recovery later
                if (!skipMetadataBlock(block)) {
                    Debug::log("flac", "[parseMetadataBlocks] Failed to skip corrupted STREAMINFO block");
                    
                    // Try to recover by finding audio data
                    if (recoverFromCorruptedMetadata()) {
                        Debug::log("flac", "[parseMetadataBlocks] Recovered from corrupted STREAMINFO");
                        break;
                    } else {
                        reportError("Format", "Failed to process STREAMINFO block and cannot recover");
                        return false;
                    }
                }
            } else {
                // For non-critical blocks, try to skip and continue
                Debug::log("flac", "[parseMetadataBlocks] Attempting to skip failed ", block_type_name, " block");
                
                if (!skipMetadataBlock(block)) {
                    Debug::log("flac", "[parseMetadataBlocks] Failed to skip ", block_type_name, " block after parse error");
                    
                    // Try to recover by finding next valid block or audio data
                    if (recoverFromCorruptedMetadata()) {
                        Debug::log("flac", "[parseMetadataBlocks] Recovered from corrupted ", block_type_name, " block");
                        break;
                    } else {
                        // For non-critical blocks, we can continue without them
                        Debug::log("flac", "[parseMetadataBlocks] Cannot recover from ", block_type_name, 
                                  " block error, but continuing anyway");
                        break;
                    }
                }
            }
        }
    }
    
    // Check if we hit the maximum block limit
    if (blocks_parsed >= max_metadata_blocks) {
        Debug::log("flac", "[parseMetadataBlocks] Reached maximum metadata block limit (", max_metadata_blocks, 
                  "), stopping parsing");
        
        // Try to find audio data start
        if (recoverFromCorruptedMetadata()) {
            Debug::log("flac", "[parseMetadataBlocks] Found audio data after hitting block limit");
        } else {
            reportError("Format", "Too many metadata blocks and cannot find audio data");
            return false;
        }
    }
    
    // STREAMINFO is mandatory - if we didn't find it, try to recover
    if (!found_streaminfo) {
        Debug::log("flac", "[parseMetadataBlocks] STREAMINFO block not found, attempting recovery");
        
        if (attemptStreamInfoRecovery()) {
            Debug::log("flac", "[parseMetadataBlocks] Successfully recovered STREAMINFO from first frame");
        } else {
            reportError("Format", "FLAC file missing mandatory STREAMINFO block and cannot recover");
            return false;
        }
    }
    
    // Store current position as start of audio data
    m_audio_data_offset = static_cast<uint64_t>(m_handler->tell());
    m_current_offset = m_audio_data_offset;
    
    Debug::log("flac", "[parseMetadataBlocks] Metadata parsing complete:");
    Debug::log("flac", "[parseMetadataBlocks] Blocks parsed: ", blocks_parsed);
    Debug::log("flac", "[parseMetadataBlocks] Blocks skipped: ", blocks_skipped);
    Debug::log("flac", "[parseMetadataBlocks] Audio data starts at offset: ", m_audio_data_offset);
    
    return true;
}

bool FLACDemuxer::parseStreamInfoBlock(const FLACMetadataBlock& block)
{
    std::lock_guard<std::mutex> metadata_lock(m_metadata_mutex);
    return parseStreamInfoBlock_unlocked(block);
}

bool FLACDemuxer::parseStreamInfoBlock_unlocked(const FLACMetadataBlock& block)
{
    Debug::log("flac", "[parseStreamInfoBlock_unlocked] Parsing STREAMINFO block");
    
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
    
    // Parse STREAMINFO fields according to RFC 9639 bit layout
    // All fields are in big-endian format
    
    // Minimum block size (16 bits) - bytes 0-1
    m_streaminfo.min_block_size = (static_cast<uint16_t>(data[0]) << 8) | 
                                  static_cast<uint16_t>(data[1]);
    
    // Maximum block size (16 bits) - bytes 2-3
    m_streaminfo.max_block_size = (static_cast<uint16_t>(data[2]) << 8) | 
                                  static_cast<uint16_t>(data[3]);
    
    // Minimum frame size (24 bits) - bytes 4-6
    m_streaminfo.min_frame_size = (static_cast<uint32_t>(data[4]) << 16) |
                                  (static_cast<uint32_t>(data[5]) << 8) |
                                  static_cast<uint32_t>(data[6]);
    
    // Maximum frame size (24 bits) - bytes 7-9
    m_streaminfo.max_frame_size = (static_cast<uint32_t>(data[7]) << 16) |
                                  (static_cast<uint32_t>(data[8]) << 8) |
                                  static_cast<uint32_t>(data[9]);
    
    // RFC 9639 Section 8.2: Packed fields in bytes 10-13
    // Sample rate (20 bits), channels (3 bits), bits per sample (5 bits)
    // Total samples (36 bits) spans bytes 13-17
    
    // Sample rate (20 bits) - bytes 10-12 + upper 4 bits of byte 13
    // Correct bit extraction: 20 bits total
    m_streaminfo.sample_rate = (static_cast<uint32_t>(data[10]) << 12) |
                               (static_cast<uint32_t>(data[11]) << 4) |
                               ((static_cast<uint32_t>(data[12]) >> 4) & 0x0F);
    
    // Channels (3 bits) - bits 1-3 of byte 12 (after sample rate), then add 1
    // Correct bit extraction: 3 bits from positions 1-3 of byte 12
    m_streaminfo.channels = ((data[12] >> 1) & 0x07) + 1;
    
    // Bits per sample (5 bits) - bit 0 of byte 12 + upper 4 bits of byte 13, then add 1
    // Correct bit extraction: 1 bit from byte 12 + 4 bits from byte 13
    m_streaminfo.bits_per_sample = (((data[12] & 0x01) << 4) | ((data[13] >> 4) & 0x0F)) + 1;
    
    // Total samples (36 bits) - lower 4 bits of byte 13 + bytes 14-17
    // Correct bit extraction: 4 bits from byte 13 + 32 bits from bytes 14-17
    m_streaminfo.total_samples = (static_cast<uint64_t>(data[13] & 0x0F) << 32) |
                                 (static_cast<uint64_t>(data[14]) << 24) |
                                 (static_cast<uint64_t>(data[15]) << 16) |
                                 (static_cast<uint64_t>(data[16]) << 8) |
                                 static_cast<uint64_t>(data[17]);
    
    // MD5 signature (16 bytes) - bytes 18-33
    std::memcpy(m_streaminfo.md5_signature, &data[18], 16);
    
    // Validate parsed data against RFC 9639 constraints
    if (!validateStreamInfoParameters_unlocked()) {
        reportError("Format", "STREAMINFO contains invalid parameters per RFC 9639");
        return false;
    }
    
    Debug::log("flac", "[parseStreamInfoBlock_unlocked] STREAMINFO parsed successfully:");
    Debug::log("flac", "[parseStreamInfoBlock_unlocked] Sample rate: ", m_streaminfo.sample_rate, " Hz");
    Debug::log("flac", "[parseStreamInfoBlock_unlocked] Channels: ", static_cast<int>(m_streaminfo.channels));
    Debug::log("flac", "[parseStreamInfoBlock_unlocked] Bits per sample: ", static_cast<int>(m_streaminfo.bits_per_sample));
    Debug::log("flac", "[parseStreamInfoBlock_unlocked] Total samples: ", m_streaminfo.total_samples);
    Debug::log("flac", "[parseStreamInfoBlock_unlocked] Duration: ", m_streaminfo.getDurationMs(), " ms");
    Debug::log("flac", "[parseStreamInfoBlock_unlocked] Block size range: ", m_streaminfo.min_block_size, "-", m_streaminfo.max_block_size);
    
    if (m_streaminfo.min_frame_size > 0 && m_streaminfo.max_frame_size > 0) {
        Debug::log("flac", "[parseStreamInfoBlock_unlocked] Frame size range: ", m_streaminfo.min_frame_size, "-", m_streaminfo.max_frame_size);
    }
    
    return true;
}

bool FLACDemuxer::parseSeekTableBlock(const FLACMetadataBlock& block)
{
    Debug::log("flac", "[parseSeekTableBlock] Parsing SEEKTABLE block");
    
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
    Debug::log("flac", "[parseSeekTableBlock] SEEKTABLE contains ", num_seek_points, " seek points");
    
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
            Debug::log("flac", "[parseSeekTableBlock] Seek point ", i, " is a placeholder, skipping");
            continue;
        }
        
        // Validate seek point for consistency and reasonable values
        if (!seek_point.isValid()) {
            Debug::log("flac", "[parseSeekTableBlock] Invalid seek point ", i, ", skipping");
            continue;
        }
        
        // Additional validation against STREAMINFO
        if (m_streaminfo.isValid()) {
            // Check if sample number is within total samples
            if (m_streaminfo.total_samples > 0 && 
                seek_point.sample_number >= m_streaminfo.total_samples) {
                Debug::log("flac", "[parseSeekTableBlock] Seek point ", i, " sample number (", 
                          seek_point.sample_number, ") exceeds total samples (", 
                          m_streaminfo.total_samples, "), skipping");
                continue;
            }
            
            // Check if frame samples is reasonable
            if (seek_point.frame_samples > m_streaminfo.max_block_size) {
                Debug::log("flac", "[parseSeekTableBlock] Seek point ", i, " frame samples (", 
                          seek_point.frame_samples, ") exceeds max block size (", 
                          m_streaminfo.max_block_size, "), skipping");
                continue;
            }
        }
        
        // Check for reasonable stream offset (should be within file size)
        if (m_file_size > 0 && seek_point.stream_offset >= m_file_size) {
            Debug::log("flac", "[parseSeekTableBlock] Seek point ", i, " stream offset (", 
                      seek_point.stream_offset, ") exceeds file size (", 
                      m_file_size, "), skipping");
            continue;
        }
        
        // Add valid seek point to table
        m_seektable.push_back(seek_point);
        
        Debug::log("flac", "[parseSeekTableBlock] Added seek point: sample=", seek_point.sample_number, 
                  " offset=", seek_point.stream_offset, 
                  " frame_samples=", seek_point.frame_samples);
    }
    
    Debug::log("flac", "[parseSeekTableBlock] SEEKTABLE parsed successfully, ", m_seektable.size(), 
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
    Debug::log("flac", "[parseVorbisCommentBlock] Parsing VORBIS_COMMENT block");
    
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
        Debug::log("flac", "[parseVorbisCommentBlock] Vendor string: ", vendor_string);
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
    
    Debug::log("flac", "[parseVorbisCommentBlock] Processing ", comment_count, " user comments");
    
    // Read each user comment
    for (uint32_t i = 0; i < comment_count; i++) {
        // Check if we have enough bytes left
        if (bytes_read + 4 > block.length) {
            Debug::log("flac", "[parseVorbisCommentBlock] Not enough data for comment ", i, " length field");
            break;
        }
        
        // Read comment length (32-bit little-endian)
        uint8_t comment_len_data[4];
        if (m_handler->read(comment_len_data, 1, 4) != 4) {
            Debug::log("flac", "[parseVorbisCommentBlock] Failed to read comment ", i, " length");
            break;
        }
        bytes_read += 4;
        
        uint32_t comment_length = static_cast<uint32_t>(comment_len_data[0]) |
                                 (static_cast<uint32_t>(comment_len_data[1]) << 8) |
                                 (static_cast<uint32_t>(comment_len_data[2]) << 16) |
                                 (static_cast<uint32_t>(comment_len_data[3]) << 24);
        
        // Validate comment length
        if (comment_length == 0) {
            Debug::log("flac", "[parseVorbisCommentBlock] Empty comment ", i, ", skipping");
            continue;
        }
        
        if (bytes_read + comment_length > block.length) {
            Debug::log("flac", "[parseVorbisCommentBlock] Comment ", i, " length (", comment_length, 
                      ") exceeds remaining block data");
            break;
        }
        
        // Reasonable size limit for comments (64KB)
        if (comment_length > 65536) {
            Debug::log("flac", "[parseVorbisCommentBlock] Comment ", i, " too large (", comment_length, " bytes), skipping");
            // Skip this comment
            off_t current_pos = m_handler->tell();
            if (current_pos < 0 || m_handler->seek(current_pos + static_cast<off_t>(comment_length), SEEK_SET) != 0) {
                Debug::log("flac", "[parseVorbisCommentBlock] Failed to skip oversized comment");
                break;
            }
            bytes_read += comment_length;
            continue;
        }
        
        // Read comment data (UTF-8)
        std::vector<uint8_t> comment_data(comment_length);
        if (m_handler->read(comment_data.data(), 1, comment_length) != comment_length) {
            Debug::log("flac", "[parseVorbisCommentBlock] Failed to read comment ", i, " data");
            break;
        }
        bytes_read += comment_length;
        
        // Convert to string (assuming UTF-8)
        std::string comment_string(comment_data.begin(), comment_data.end());
        
        // Parse FIELD=value format
        size_t equals_pos = comment_string.find('=');
        if (equals_pos == std::string::npos) {
            Debug::log("flac", "[parseVorbisCommentBlock] Comment ", i, " missing '=' separator: ", comment_string);
            continue;
        }
        
        std::string field = comment_string.substr(0, equals_pos);
        std::string value = comment_string.substr(equals_pos + 1);
        
        // Convert field name to uppercase for consistency
        std::transform(field.begin(), field.end(), field.begin(), ::toupper);
        
        // Store the comment
        m_vorbis_comments[field] = value;
        
        Debug::log("flac", "[parseVorbisCommentBlock] Added comment: ", field, " = ", value);
    }
    
    // Skip any remaining bytes in the block
    if (bytes_read < block.length) {
        uint32_t remaining = block.length - bytes_read;
        Debug::log("flac", "[parseVorbisCommentBlock] Skipping ", remaining, " remaining bytes in VORBIS_COMMENT block");
        off_t current_pos = m_handler->tell();
        if (current_pos < 0 || m_handler->seek(current_pos + static_cast<off_t>(remaining), SEEK_SET) != 0) {
            Debug::log("flac", "[parseVorbisCommentBlock] Failed to skip remaining VORBIS_COMMENT data");
            return false;
        }
    }
    
    Debug::log("flac", "[parseVorbisCommentBlock] VORBIS_COMMENT parsed successfully, ", m_vorbis_comments.size(), " comments");
    
    // Log standard metadata fields if present
    const std::vector<std::string> standard_fields = {
        "TITLE", "ARTIST", "ALBUM", "DATE", "TRACKNUMBER", "GENRE", "ALBUMARTIST"
    };
    
    for (const auto& field : standard_fields) {
        auto it = m_vorbis_comments.find(field);
        if (it != m_vorbis_comments.end()) {
            Debug::log("flac", "[parseVorbisCommentBlock] ", field, ": ", it->second);
        }
    }
    
    return true;
}

bool FLACDemuxer::parsePictureBlock(const FLACMetadataBlock& block)
{
    Debug::log("flac", "[parsePictureBlock] Parsing PICTURE block");
    
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
    
    // Memory-optimized picture storage: store location instead of loading data
    picture.data_size = data_length;
    picture.data_offset = static_cast<uint64_t>(m_handler->tell());
    
    // Apply memory management limits
    if (data_length > MAX_PICTURE_SIZE) {
        Debug::log("flac", "Picture data too large (", data_length, " bytes), skipping");
        // Skip the picture entirely if it's too large
        off_t block_end = static_cast<off_t>(block.data_offset + block.length);
        if (m_handler->seek(block_end, SEEK_SET) != 0) {
            reportError("IO", "Failed to skip oversized picture data");
            return false;
        }
        return true;  // Skip this picture but continue parsing
    }
    
    // Check if we already have too many pictures
    if (m_pictures.size() >= MAX_PICTURES) {
        Debug::log("flac", "Too many pictures already stored, skipping additional picture");
        // Skip the picture data
        off_t block_end = static_cast<off_t>(block.data_offset + block.length);
        if (m_handler->seek(block_end, SEEK_SET) != 0) {
            reportError("IO", "Failed to skip excess picture data");
            return false;
        }
        return true;  // Skip this picture but continue parsing
    }
    
    // Skip the actual image data for now (will be loaded on demand)
    // Seek to the end of this metadata block
    // Note: block.data_offset points to the start of block data, 
    // and block.length is the size of the data portion
    off_t block_end = static_cast<off_t>(block.data_offset + block.length);
    Debug::log("flac", "Seeking to end of PICTURE block: data_offset=", block.data_offset, 
               " length=", block.length, " target=", block_end);
    
    if (m_handler->seek(block_end, SEEK_SET) != 0) {
        reportError("IO", "Failed to skip picture data at offset " + std::to_string(block_end));
        return false;
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
    
    // Skip the block data by seeking to the end of the block
    // Use block.data_offset + block.length to get the correct end position
    off_t target_pos = static_cast<off_t>(block.data_offset + block.length);
    
    Debug::log("flac", "Seeking to end of block: data_offset=", block.data_offset, 
              " length=", block.length, " target=", target_pos);
    
    if (m_handler->seek(target_pos, SEEK_SET) != 0) {
        Debug::log("flac", "Failed to seek past metadata block to position ", target_pos);
        return false;
    }
    
    return true;
}

bool FLACDemuxer::findNextFrame(FLACFrame& frame)
{
    std::lock_guard<std::mutex> state_lock(m_state_mutex);
    return findNextFrame_unlocked(frame);
}

bool FLACDemuxer::findNextFrame_unlocked(FLACFrame& frame)
{
    Debug::log("flac", "[findNextFrame_unlocked] Starting RFC 9639 compliant frame boundary detection");
    
    if (!m_handler) {
        reportError("IO", "No IOHandler available for frame sync detection");
        return false;
    }
    
    // Start searching from current position
    uint64_t search_start = m_current_offset;
    
    Debug::log("flac", "[findNextFrame_unlocked] Starting frame search from offset: ", search_start);
    
    // Conservative frame size estimation using STREAMINFO min_frame_size
    uint32_t conservative_frame_size = 64;  // Fallback minimum for safety
    if (m_streaminfo.isValid() && m_streaminfo.min_frame_size > 0) {
        conservative_frame_size = m_streaminfo.min_frame_size;
        Debug::log("flac", "[findNextFrame_unlocked] Using STREAMINFO min_frame_size: ", conservative_frame_size, " bytes");
    } else {
        Debug::log("flac", "[findNextFrame_unlocked] No STREAMINFO min_frame_size, using conservative fallback: ", conservative_frame_size, " bytes");
    }
    
    // INFINITE LOOP PREVENTION: Strict search window limits
    const uint64_t MAX_SEARCH_SCOPE = std::min(static_cast<uint64_t>(4096), 
                                               static_cast<uint64_t>(conservative_frame_size * 2));
    
    Debug::log("flac", "[findNextFrame_unlocked] Using limited search scope: ", MAX_SEARCH_SCOPE, " bytes");
    
    // Seek to search position
    if (m_handler->seek(static_cast<off_t>(search_start), SEEK_SET) != 0) {
        reportError("IO", "Failed to seek to search position");
        return false;
    }
    
    // Try to parse frame header at current position first (most common case)
    frame.file_offset = search_start;
    if (parseFrameHeader(frame) && validateFrameHeader(frame)) {
        Debug::log("flac", "[findNextFrame_unlocked] Valid FLAC frame found at current position ", search_start);
        m_current_offset = search_start;
        
        // Calculate frame size for boundary detection
        uint32_t frame_size = calculateFrameSize_unlocked(frame);
        if (frame_size > 0) {
            frame.frame_size = frame_size;
        }
        
        return true;
    }
    
    // Frame not at current position - perform limited sync pattern search
    Debug::log("flac", "[findNextFrame_unlocked] Frame not at current position, starting limited sync search");
    
    // Allocate search buffer
    const size_t SEARCH_BUFFER_SIZE = static_cast<size_t>(MAX_SEARCH_SCOPE);
    if (!ensureBufferCapacity(m_sync_buffer, SEARCH_BUFFER_SIZE)) {
        reportError("Memory", "Failed to allocate sync search buffer");
        return false;
    }
    
    // Single read operation to minimize I/O
    size_t bytes_read = m_handler->read(m_sync_buffer.data(), 1, SEARCH_BUFFER_SIZE);
    
    if (bytes_read < 2) {
        Debug::log("flac", "[findNextFrame_unlocked] Insufficient data for sync search (", bytes_read, " bytes)");
        return false;
    }
    
    // RFC 9639 compliant sync pattern search with validation
    size_t sync_offset;
    if (searchSyncPattern_unlocked(m_sync_buffer.data(), bytes_read, sync_offset)) {
        uint64_t sync_position = search_start + sync_offset;
        
        Debug::log("flac", "[findNextFrame_unlocked] Found RFC 9639 sync pattern at position ", sync_position);
        
        // Seek to sync position and validate frame header
        if (m_handler->seek(static_cast<off_t>(sync_position), SEEK_SET) == 0) {
            frame.file_offset = sync_position;
            
            if (parseFrameHeader(frame) && validateFrameHeader(frame)) {
                Debug::log("flac", "[findNextFrame_unlocked] Valid FLAC frame found at position ", sync_position);
                m_current_offset = sync_position;
                
                // Calculate frame size for boundary detection
                uint32_t frame_size = calculateFrameSize_unlocked(frame);
                if (frame_size > 0) {
                    frame.frame_size = frame_size;
                }
                
                return true;
            } else {
                Debug::log("flac", "[findNextFrame_unlocked] Frame header validation failed at sync position ", sync_position);
            }
        }
    }
    
    // Fallback strategy: Use conservative frame size estimation
    Debug::log("flac", "[findNextFrame_unlocked] Sync search failed, using conservative frame size fallback");
    
    if (m_handler->seek(static_cast<off_t>(search_start), SEEK_SET) == 0) {
        frame.file_offset = search_start;
        frame.frame_size = conservative_frame_size;
        
        // Try to parse header with conservative size assumption
        if (parseFrameHeader(frame)) {
            Debug::log("flac", "[findNextFrame_unlocked] Frame found using conservative size fallback: ", conservative_frame_size, " bytes");
            m_current_offset = search_start;
            return true;
        }
    }
    
    Debug::log("flac", "[findNextFrame_unlocked] All frame boundary detection strategies failed");
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
    
    // Read first 4 bytes (sync + basic header info) with error checking
    uint8_t header_start[4];
    size_t bytes_read = m_handler->read(header_start, 1, 4);
    
    if (bytes_read == 0) {
        Debug::log("flac", "End of file reached while reading frame header");
        return false;
    }
    
    if (bytes_read < 4) {
        Debug::log("flac", "Incomplete frame header read: only ", bytes_read, " bytes available");
        return false;
    }
    
    // Verify sync code (15 bits: 111111111111100 per RFC 9639 Section 9.1)
    uint16_t sync_code = (static_cast<uint16_t>(header_start[0]) << 8) | 
                        static_cast<uint16_t>(header_start[1]);
    
    // RFC 9639: 15-bit sync code 0b111111111111100 followed by blocking strategy bit
    // Valid patterns: 0xFFF8 (fixed block) or 0xFFF9 (variable block)
    // Both patterns have the same upper 14 bits when masked with 0xFFFC
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
    
    // Read frame/sample number (UTF-8 coded) with error handling
    uint64_t frame_sample_number = 0;
    uint8_t utf8_byte;
    size_t utf8_bytes_read = m_handler->read(&utf8_byte, 1, 1);
    
    if (utf8_bytes_read != 1) {
        Debug::log("flac", "Failed to read frame/sample number (EOF or read error)");
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
                      ") exceeds total samples (", m_streaminfo.total_samples, ") - reached EOF");
            // This is EOF, not a validation error - set EOF flag
            setEOF(true);
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

bool FLACDemuxer::validateFrameHeaderAt(uint64_t file_offset)
{
    Debug::log("flac", "FLACDemuxer::validateFrameHeaderAt() - validating frame header at offset ", file_offset);
    
    // Save current position
    off_t original_pos = m_handler->tell();
    
    // Seek to the potential frame start
    if (m_handler->seek(static_cast<off_t>(file_offset), SEEK_SET) != 0) {
        Debug::log("flac", "Failed to seek to offset ", file_offset);
        return false;
    }
    
    // Read enough bytes for a minimal frame header (at least 7 bytes)
    uint8_t header_bytes[16];
    size_t bytes_read = m_handler->read(header_bytes, 1, sizeof(header_bytes));
    
    // Restore original position
    m_handler->seek(original_pos, SEEK_SET);
    
    if (bytes_read < 7) {
        Debug::log("flac", "Not enough bytes for frame header at offset ", file_offset);
        return false;
    }
    
    // Validate sync pattern (15-bit sync code 0b111111111111100 = 0xFFF8)
    // The sync pattern is exactly 0xFFF8 (15 bits), so we need:
    // - First byte: 0xFF (8 bits)
    // - Second byte: 0xF8 with the blocking strategy bit (can be 0xF8 or 0xF9)
    if (header_bytes[0] != 0xFF || (header_bytes[1] & 0xFE) != 0xF8) {
        Debug::log("flac", "Invalid sync pattern at offset ", file_offset, 
                  " (got ", std::hex, static_cast<int>(header_bytes[0]), " ", 
                  static_cast<int>(header_bytes[1]), std::dec, ")");
        return false;
    }
    
    // Parse and validate frame header structure
    size_t bit_offset = 15; // After sync code
    
    // Blocking strategy (1 bit)
    // bool variable_block_size = (header_bytes[1] & 0x01) != 0;  // Not used in validation
    bit_offset++;
    
    // Block size bits (4 bits)
    uint8_t block_size_bits = (header_bytes[2] >> 4) & 0x0F;
    if (block_size_bits == 0x00) {
        Debug::log("flac", "Reserved block size bits at offset ", file_offset);
        return false;
    }
    bit_offset += 4;
    
    // Sample rate bits (4 bits)
    uint8_t sample_rate_bits = header_bytes[2] & 0x0F;
    if (sample_rate_bits == 0x0F) {
        Debug::log("flac", "Invalid sample rate bits at offset ", file_offset);
        return false;
    }
    bit_offset += 4;
    
    // Channel assignment (4 bits)
    uint8_t channel_bits = (header_bytes[3] >> 4) & 0x0F;
    if (channel_bits >= 0x0B && channel_bits <= 0x0F) {
        Debug::log("flac", "Reserved channel assignment at offset ", file_offset);
        return false;
    }
    bit_offset += 4;
    
    // Sample size bits (3 bits)
    uint8_t sample_size_bits = (header_bytes[3] >> 1) & 0x07;
    if (sample_size_bits == 0x03 || sample_size_bits == 0x07) {
        Debug::log("flac", "Reserved sample size bits at offset ", file_offset);
        return false;
    }
    bit_offset += 3;
    
    // Reserved bit (1 bit) - must be 0
    if ((header_bytes[3] & 0x01) != 0) {
        Debug::log("flac", "Reserved bit not zero at offset ", file_offset);
        return false;
    }
    bit_offset++;
    
    // Parse frame/sample number (UTF-8 encoded)
    size_t byte_offset = 4;
    
    // Simple UTF-8 validation - check first byte
    uint8_t first_byte = header_bytes[byte_offset];
    if (first_byte < 0x80) {
        // Single byte (0xxxxxxx)
        byte_offset++;
    } else if ((first_byte & 0xE0) == 0xC0) {
        // Two bytes (110xxxxx 10xxxxxx)
        if (byte_offset + 1 >= bytes_read) return false;
        if ((header_bytes[byte_offset + 1] & 0xC0) != 0x80) return false;
        byte_offset += 2;
    } else if ((first_byte & 0xF0) == 0xE0) {
        // Three bytes (1110xxxx 10xxxxxx 10xxxxxx)
        if (byte_offset + 2 >= bytes_read) return false;
        if ((header_bytes[byte_offset + 1] & 0xC0) != 0x80) return false;
        if ((header_bytes[byte_offset + 2] & 0xC0) != 0x80) return false;
        byte_offset += 3;
    } else if ((first_byte & 0xF8) == 0xF0) {
        // Four bytes or more - we'll accept it for now
        byte_offset += 4;
    } else {
        Debug::log("flac", "Invalid UTF-8 encoding at offset ", file_offset);
        return false;
    }
    
    // Check if we have enough bytes for the rest of the header
    if (byte_offset >= bytes_read) {
        Debug::log("flac", "Frame header extends beyond available data at offset ", file_offset);
        return false;
    }
    
    // Additional validation: check for uncommon block size or sample rate
    if (block_size_bits == 0x06 || block_size_bits == 0x07) {
        // Uncommon block size - need additional bytes
        if (byte_offset >= bytes_read) return false;
        byte_offset++;
        if (block_size_bits == 0x07 && byte_offset >= bytes_read) return false;
        if (block_size_bits == 0x07) byte_offset++;
    }
    
    if (sample_rate_bits == 0x0C || sample_rate_bits == 0x0D || sample_rate_bits == 0x0E) {
        // Uncommon sample rate - need additional bytes
        if (byte_offset >= bytes_read) return false;
        byte_offset++;
        if (sample_rate_bits == 0x0E && byte_offset >= bytes_read) return false;
        if (sample_rate_bits == 0x0E) byte_offset++;
    }
    
    // Frame header CRC-8 should be at byte_offset
    if (byte_offset >= bytes_read) {
        Debug::log("flac", "Missing frame header CRC at offset ", file_offset);
        return false;
    }
    
    // TODO: Validate CRC-8 if needed for stricter validation
    
    Debug::log("flac", "Frame header validation passed at offset ", file_offset);
    return true;
}

bool FLACDemuxer::readFrameData(const FLACFrame& frame, std::vector<uint8_t>& data)
{
    // This method is no longer used with the stream-based approach
    // readChunk_unlocked now provides sequential data directly to libFLAC
    Debug::log("flac", "[readFrameData] Method not used in stream-based approach");
    return false;
}

void FLACDemuxer::resetPositionTracking()
{
    Debug::log("flac", "FLACDemuxer::resetPositionTracking() - resetting position to start");
    
    // Reset sample position to beginning of stream (atomic update)
    m_current_sample.store(0);
    m_last_block_size = 0;
    
    // Reset file position to start of audio data
    m_current_offset = m_audio_data_offset;
    
    Debug::log("flac", "Position tracking reset: sample=0", 
              " file_offset=", m_current_offset, " (0 ms)");
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
    
    // Update tracking variables (atomic update for sample position)
    m_current_sample.store(sample_position);
    m_current_offset = file_offset;
    
    Debug::log("flac", "Position tracking updated: sample=", sample_position, 
              " file_offset=", file_offset, " (", samplesToMs(sample_position), " ms)");
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
    
    // Use optimized binary search to find the best seek point
    size_t seek_index = findSeekPointIndex(target_sample);
    if (seek_index == SIZE_MAX) {
        Debug::log("flac", "No suitable seek point found for sample ", target_sample);
        return false;
    }
    
    const FLACSeekPoint& best_seek_point = m_seektable[seek_index];
    
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
    if (!m_handler->seek(static_cast<off_t>(file_position), SEEK_SET)) {
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
        if (!findNextFrame_unlocked(frame)) {
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
            if (!m_handler->seek(static_cast<off_t>(frame.file_offset), SEEK_SET)) {
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
            if (!m_handler->seek(static_cast<off_t>(next_frame_offset), SEEK_SET)) {
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
    Debug::log("flac", "[seekBinary] Seeking to sample ", target_sample, " using binary search");
    
    // ARCHITECTURAL LIMITATION ACKNOWLEDGMENT:
    // Binary search is fundamentally incompatible with compressed audio streams.
    // 
    // PROBLEM: Cannot predict frame positions in variable-length compressed data.
    // - FLAC frames have variable sizes depending on audio content and compression
    // - Frame boundaries are unpredictable without parsing the entire stream
    // - Estimating positions based on file offsets often leads to incorrect locations
    // 
    // CURRENT APPROACH: Implement binary search but expect failures with compressed streams.
    // This method attempts binary search but acknowledges it may fail frequently.
    // 
    // FALLBACK STRATEGY: Return to beginning position when binary search fails.
    // This ensures the demuxer remains in a valid state even when seeking fails.
    // 
    // FUTURE SOLUTION: Implement frame indexing during initial parsing for accurate seeking.
    // A proper solution would build a frame index during container parsing, caching
    // discovered frame positions to enable sample-accurate seeking without guesswork.
    
    Debug::log("flac", "[seekBinary] WARNING: Binary search has fundamental limitations with compressed audio");
    
    if (!m_handler) {
        reportError("IO", "No IOHandler available for seeking");
        return false;
    }
    
    if (!m_streaminfo.isValid()) {
        reportError("State", "Invalid STREAMINFO for seeking");
        return false;
    }
    
    if (m_file_size == 0) {
        Debug::log("flac", "[seekBinary] Unknown file size, cannot perform binary search");
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
    
    Debug::log("flac", "[seekBinary] Binary search bounds: file offset ", search_start, " to ", search_end);
    Debug::log("flac", "[seekBinary] Estimated total samples: ", total_samples);
    Debug::log("flac", "[seekBinary] NOTE: Position estimates may be inaccurate due to variable compression");
    
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
        
        Debug::log("flac", "[seekBinary] Iteration ", iteration, ": trying offset ", mid_offset, " (may not align with frame boundaries)");
        
        // Seek to midpoint
        if (!m_handler->seek(static_cast<off_t>(mid_offset), SEEK_SET)) {
            Debug::log("flac", "Failed to seek to offset ", mid_offset);
            break;
        }
        
        // Find the next valid FLAC frame from this position
        FLACFrame frame;
        bool found_frame = false;
        
        // Search forward from midpoint for a valid frame (limited distance for efficiency)
        const uint32_t max_search_distance = 8192;  // Reduced from 64KB for efficiency
        uint64_t search_offset = mid_offset;
        
        while (search_offset < search_end && (search_offset - mid_offset) < max_search_distance) {
            if (!m_handler->seek(static_cast<off_t>(search_offset), SEEK_SET)) {
                break;
            }
            
            // Look for FLAC sync pattern
            uint8_t sync_bytes[2];
            if (m_handler->read(sync_bytes, 1, 2) != 2) {
                break;
            }
            
            if (sync_bytes[0] == 0xFF && (sync_bytes[1] & 0xF8) == 0xF8) {
                // Found potential sync, seek back and try to parse frame
                if (!m_handler->seek(static_cast<off_t>(search_offset), SEEK_SET)) {
                    break;
                }
                
                if (findNextFrame_unlocked(frame)) {
                    // Validate frame header for consistency
                    if (validateFrameHeader(frame)) {
                        found_frame = true;
                        Debug::log("flac", "[seekBinary] Found valid frame at offset ", frame.file_offset, 
                                  " sample ", frame.sample_offset, " (frame boundary discovered by parsing)");
                        break;
                    }
                }
            }
            
            search_offset++;
        }
        
        if (!found_frame) {
            Debug::log("flac", "[seekBinary] No valid frame found near offset ", mid_offset, " - compressed stream boundary mismatch");
            // Adjust search to lower half (this is often ineffective with compressed data)
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
            Debug::log("flac", "[seekBinary] New best position: sample ", best_sample, " at offset ", best_file_offset, 
                      " (distance: ", sample_distance, " samples)");
        }
        
        // Check if we're close enough to the target
        if (sample_distance <= sample_tolerance) {
            Debug::log("flac", "[seekBinary] Found frame within tolerance (", sample_distance, " samples) - acceptable for compressed stream");
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
        Debug::log("flac", "[seekBinary] Binary search reached maximum iterations - compressed stream complexity exceeded search capability");
    }
    
    // Seek to the best position found (if any)
    if (best_file_offset > 0) {
        Debug::log("flac", "[seekBinary] Seeking to best position found: sample ", best_sample, " at offset ", best_file_offset);
        
        if (m_handler->seek(static_cast<off_t>(best_file_offset), SEEK_SET) != 0) {
            reportError("IO", "Failed to seek to best position at offset " + std::to_string(best_file_offset));
            // Fall through to fallback strategy below
        } else {
            updatePositionTracking(best_sample, best_file_offset);
            
            // Calculate final distance from target
            uint64_t sample_distance = (best_sample > target_sample) ? 
                                      (best_sample - target_sample) : 
                                      (target_sample - best_sample);
            
            if (sample_distance <= sample_tolerance) {
                Debug::log("flac", "[seekBinary] Binary search successful within tolerance (", sample_distance, " samples)");
                return true;
            } else {
                Debug::log("flac", "[seekBinary] Binary search found approximate position, distance: ", sample_distance, " samples");
                Debug::log("flac", "[seekBinary] Compressed stream prevents exact positioning - this is expected behavior");
                return true;  // Still consider this successful for approximate seeking
            }
        }
    }
    
    // FALLBACK STRATEGY: Binary search failed - return to beginning position
    // This is the expected behavior due to the architectural limitations of binary search
    // with compressed audio streams. The fallback ensures we remain in a valid state.
    Debug::log("flac", "[seekBinary] Binary search failed due to compressed stream limitations");
    Debug::log("flac", "[seekBinary] Implementing fallback strategy: returning to beginning position");
    
    if (m_handler->seek(static_cast<off_t>(m_audio_data_offset), SEEK_SET) != 0) {
        reportError("IO", "Failed to seek to audio data start during fallback");
        return false;
    }
    
    updatePositionTracking(0, m_audio_data_offset);
    
    // Binary search failure is expected with compressed streams
    // Return success only if we were seeking to the beginning anyway
    bool fallback_success = (target_sample == 0);
    
    if (fallback_success) {
        Debug::log("flac", "[seekBinary] Fallback successful - was seeking to beginning");
    } else {
        Debug::log("flac", "[seekBinary] Fallback to beginning - binary search cannot handle compressed streams");
        Debug::log("flac", "[seekBinary] FUTURE: Frame indexing during parsing would enable accurate seeking");
    }
    
    return fallback_success;
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
    uint64_t current_sample = m_current_sample.load();
    uint64_t current_distance = (current_sample > target_sample) ? 
                               (current_sample - target_sample) : 
                               (target_sample - current_sample);
    
    // If target is close to current position and we're ahead, start from current position
    const uint64_t short_seek_threshold = m_streaminfo.max_block_size * 10;  // 10 blocks
    
    if (current_distance <= short_seek_threshold && current_sample <= target_sample) {
        start_sample = current_sample;
        start_offset = m_current_offset;
        Debug::log("flac", "Short-distance seek: starting from current position (sample ", 
                  start_sample, ", offset ", start_offset, ")");
    } else {
        Debug::log("flac", "Long-distance seek: starting from beginning (sample ", 
                  start_sample, ", offset ", start_offset, ")");
    }
    
    // Seek to starting position
    if (!m_handler->seek(static_cast<off_t>(start_offset), SEEK_SET)) {
        reportError("IO", "Failed to seek to starting position");
        return false;
    }
    
    updatePositionTracking(start_sample, start_offset);
    
    // Linear search parameters
    const uint32_t max_frames_to_parse = 10000;  // Prevent runaway parsing
    uint32_t frames_parsed = 0;
    uint64_t linear_current_sample = start_sample;
    
    Debug::log("flac", "Starting linear search from sample ", linear_current_sample, " to target ", target_sample);
    
    while (linear_current_sample < target_sample && frames_parsed < max_frames_to_parse) {
        FLACFrame frame;
        
        // Find the next frame
        if (!findNextFrame_unlocked(frame)) {
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
        linear_current_sample = frame_end_sample;
        
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
    
    if (linear_current_sample < target_sample) {
        Debug::log("flac", "Linear search reached end of stream at sample ", linear_current_sample, 
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

// ============================================================================
// Frame Indexing Methods
// ============================================================================

bool FLACDemuxer::seekWithIndex(uint64_t target_sample)
{
    Debug::log("flac", "[seekWithIndex] Seeking to sample ", target_sample, " using frame index");
    
    if (!m_handler) {
        Debug::log("flac", "[seekWithIndex] No IOHandler available for seeking");
        return false;
    }
    
    if (!m_frame_indexing_enabled) {
        Debug::log("flac", "[seekWithIndex] Frame indexing is disabled");
        return false;
    }
    
    // First try to find an exact match (frame containing the target sample)
    const FLACFrameIndexEntry* containing_entry = m_frame_index.findContainingEntry(target_sample);
    if (containing_entry) {
        Debug::log("flac", "[seekWithIndex] Found exact frame containing sample ", target_sample,
                  " at file offset ", containing_entry->file_offset, 
                  " (frame samples ", containing_entry->sample_offset, " to ", 
                  containing_entry->sample_offset + containing_entry->block_size, ")");
        
        // Seek directly to the frame
        if (m_handler->seek(static_cast<off_t>(containing_entry->file_offset), SEEK_SET) != 0) {
            Debug::log("flac", "[seekWithIndex] Failed to seek to indexed frame position");
            return false;
        }
        
        updatePositionTracking(containing_entry->sample_offset, containing_entry->file_offset);
        Debug::log("flac", "[seekWithIndex] Index-based seeking successful (exact match)");
        return true;
    }
    
    // If no exact match, find the best entry (closest but not exceeding target)
    const FLACFrameIndexEntry* best_entry = m_frame_index.findBestEntry(target_sample);
    if (!best_entry) {
        Debug::log("flac", "[seekWithIndex] No suitable index entry found for sample ", target_sample);
        return false;
    }
    
    Debug::log("flac", "[seekWithIndex] Found best index entry at sample ", best_entry->sample_offset,
              " (target: ", target_sample, ", distance: ", 
              (target_sample > best_entry->sample_offset ? target_sample - best_entry->sample_offset : 0), " samples)");
    
    // Seek to the best entry position
    if (m_handler->seek(static_cast<off_t>(best_entry->file_offset), SEEK_SET) != 0) {
        Debug::log("flac", "[seekWithIndex] Failed to seek to best index entry position");
        return false;
    }
    
    updatePositionTracking(best_entry->sample_offset, best_entry->file_offset);
    
    // If the best entry is close enough, consider it successful
    uint64_t distance = (target_sample > best_entry->sample_offset) ? 
                       (target_sample - best_entry->sample_offset) : 0;
    
    if (distance <= best_entry->block_size) {
        Debug::log("flac", "[seekWithIndex] Index-based seeking successful (close approximation, distance: ", distance, " samples)");
        return true;
    }
    
    // If we need to get closer, use linear search from this position
    Debug::log("flac", "[seekWithIndex] Using linear search from index position to reach exact target");
    return seekLinear(target_sample);
}

bool FLACDemuxer::performInitialFrameIndexing()
{
    Debug::log("flac", "[performInitialFrameIndexing] Starting initial frame indexing");
    
    if (!m_handler) {
        Debug::log("flac", "[performInitialFrameIndexing] No IOHandler available");
        return false;
    }
    
    if (!m_streaminfo.isValid()) {
        Debug::log("flac", "[performInitialFrameIndexing] Invalid STREAMINFO, cannot perform indexing");
        return false;
    }
    
    if (m_initial_indexing_complete) {
        Debug::log("flac", "[performInitialFrameIndexing] Initial indexing already complete");
        return true;
    }
    
    // Save current position
    int64_t saved_position = m_handler->tell();
    
    // Start from beginning of audio data
    if (m_handler->seek(static_cast<off_t>(m_audio_data_offset), SEEK_SET) != 0) {
        Debug::log("flac", "[performInitialFrameIndexing] Failed to seek to audio data start");
        return false;
    }
    
    Debug::log("flac", "[performInitialFrameIndexing] Indexing frames from offset ", m_audio_data_offset);
    
    // Indexing parameters
    const size_t max_frames_to_index = 1000;  // Limit initial indexing to prevent long delays
    const uint64_t max_samples_to_index = m_streaminfo.sample_rate * 300; // 5 minutes max
    size_t frames_indexed = 0;
    uint64_t samples_indexed = 0;
    
    while (frames_indexed < max_frames_to_index && samples_indexed < max_samples_to_index) {
        FLACFrame frame;
        
        // Find the next frame
        if (!findNextFrame_unlocked(frame)) {
            Debug::log("flac", "[performInitialFrameIndexing] No more frames found after indexing ", frames_indexed, " frames");
            break;
        }
        
        // Add frame to index
        addFrameToIndex(frame);
        frames_indexed++;
        samples_indexed = frame.sample_offset + frame.block_size;
        
        // Progress logging
        if (frames_indexed % 100 == 0) {
            Debug::log("flac", "[performInitialFrameIndexing] Indexed ", frames_indexed, 
                      " frames, reached sample ", samples_indexed);
        }
        
        // Skip to next frame
        if (frame.frame_size > 0) {
            uint64_t next_offset = frame.file_offset + frame.frame_size;
            if (m_handler->seek(static_cast<off_t>(next_offset), SEEK_SET) != 0) {
                Debug::log("flac", "[performInitialFrameIndexing] Failed to skip to next frame");
                break;
            }
        } else {
            // Read frame data to advance position
            std::vector<uint8_t> frame_data;
            if (!readFrameData(frame, frame_data)) {
                Debug::log("flac", "[performInitialFrameIndexing] Failed to read frame data");
                break;
            }
        }
    }
    
    m_frames_indexed_during_parsing = frames_indexed;
    m_initial_indexing_complete = true;
    
    Debug::log("flac", "[performInitialFrameIndexing] Initial indexing complete: ", frames_indexed, 
              " frames indexed, covering ", samples_indexed, " samples");
    
    // Restore original position
    if (saved_position >= 0) {
        m_handler->seek(saved_position, SEEK_SET);
    }
    
    return frames_indexed > 0;
}

void FLACDemuxer::addFrameToIndex(const FLACFrame& frame)
{
    if (!m_frame_indexing_enabled || !frame.isValid()) {
        return;
    }
    
    FLACFrameIndexEntry entry(frame.sample_offset, frame.file_offset, frame.block_size, frame.frame_size);
    
    if (m_frame_index.addFrame(entry)) {
        Debug::log("flac", "[addFrameToIndex] Added frame to index: sample ", frame.sample_offset,
                  " at offset ", frame.file_offset, " (", frame.block_size, " samples)");
    }
}

void FLACDemuxer::addFrameToIndex(uint64_t sample_offset, uint64_t file_offset, uint32_t block_size, uint32_t frame_size)
{
    if (!m_frame_indexing_enabled) {
        return;
    }
    
    FLACFrameIndexEntry entry(sample_offset, file_offset, block_size, frame_size);
    
    if (m_frame_index.addFrame(entry)) {
        Debug::log("flac", "[addFrameToIndex] Added frame to index: sample ", sample_offset,
                  " at offset ", file_offset, " (", block_size, " samples)");
    }
}

bool FLACDemuxer::isFrameIndexingEnabled() const
{
    std::lock_guard<std::mutex> lock(m_state_mutex);
    return m_frame_indexing_enabled;
}

void FLACDemuxer::enableFrameIndexing(bool enable)
{
    std::lock_guard<std::mutex> lock(m_state_mutex);
    m_frame_indexing_enabled = enable;
    
    if (!enable) {
        m_frame_index.clear();
        m_initial_indexing_complete = false;
        m_frames_indexed_during_parsing = 0;
        m_frames_indexed_during_playback = 0;
        Debug::log("flac", "[enableFrameIndexing] Frame indexing disabled and index cleared");
    } else {
        Debug::log("flac", "[enableFrameIndexing] Frame indexing enabled");
    }
}

void FLACDemuxer::clearFrameIndex()
{
    std::lock_guard<std::mutex> lock(m_state_mutex);
    m_frame_index.clear();
    m_initial_indexing_complete = false;
    m_frames_indexed_during_parsing = 0;
    m_frames_indexed_during_playback = 0;
    Debug::log("flac", "[clearFrameIndex] Frame index cleared");
}

FLACFrameIndex::IndexStats FLACDemuxer::getFrameIndexStats() const
{
    std::lock_guard<std::mutex> lock(m_state_mutex);
    return m_frame_index.getStats();
}

// Public API methods for frame indexing

void FLACDemuxer::setFrameIndexingEnabled(bool enable)
{
    enableFrameIndexing(enable);
}

bool FLACDemuxer::buildFrameIndex()
{
    std::lock_guard<std::mutex> state_lock(m_state_mutex);
    
    if (!m_container_parsed) {
        Debug::log("flac", "[buildFrameIndex] Container not parsed yet");
        return false;
    }
    
    return performInitialFrameIndexing();
}

// Error handling and recovery methods

bool FLACDemuxer::attemptStreamInfoRecovery()
{
    Debug::log("flac", "FLACDemuxer::attemptStreamInfoRecovery() - attempting to recover STREAMINFO from first frame");
    
    if (!m_handler) {
        Debug::log("flac", "No IOHandler available for STREAMINFO recovery");
        return false;
    }
    
    // Save current position
    int64_t saved_position = m_handler->tell();
    
    // Try to find the first FLAC frame after metadata
    if (m_audio_data_offset > 0) {
        if (!m_handler->seek(m_audio_data_offset, SEEK_SET)) {
            Debug::log("flac", "Failed to seek to audio data offset for recovery");
            return false;
        }
    } else {
        // If we don't know where audio starts, search from current position
        Debug::log("flac", "Audio data offset unknown, searching from current position");
    }
    
    // Try to find and parse the first frame
    FLACFrame first_frame;
    if (!findNextFrame_unlocked(first_frame)) {
        Debug::log("flac", "Could not find first FLAC frame for STREAMINFO recovery");
        // Restore position
        m_handler->seek(saved_position, SEEK_SET);
        return false;
    }
    
    // Extract parameters from the first frame to create minimal STREAMINFO
    if (!first_frame.isValid()) {
        Debug::log("flac", "First frame is invalid, cannot recover STREAMINFO");
        // Restore position
        m_handler->seek(saved_position, SEEK_SET);
        return false;
    }
    
    Debug::log("flac", "Recovering STREAMINFO from first frame:");
    Debug::log("flac", "  Sample rate: ", first_frame.sample_rate, " Hz");
    Debug::log("flac", "  Channels: ", static_cast<int>(first_frame.channels));
    Debug::log("flac", "  Bits per sample: ", static_cast<int>(first_frame.bits_per_sample));
    Debug::log("flac", "  Block size: ", first_frame.block_size);
    
    // Create minimal STREAMINFO from first frame
    m_streaminfo.sample_rate = first_frame.sample_rate;
    m_streaminfo.channels = first_frame.channels;
    m_streaminfo.bits_per_sample = first_frame.bits_per_sample;
    
    // Set reasonable defaults for block sizes
    m_streaminfo.min_block_size = first_frame.block_size;
    m_streaminfo.max_block_size = first_frame.block_size;
    
    // Frame sizes are unknown
    m_streaminfo.min_frame_size = 0;
    m_streaminfo.max_frame_size = 0;
    
    // Total samples unknown (will be 0)
    m_streaminfo.total_samples = 0;
    
    // Clear MD5 signature (unknown)
    std::memset(m_streaminfo.md5_signature, 0, sizeof(m_streaminfo.md5_signature));
    
    // Restore position
    m_handler->seek(saved_position, SEEK_SET);
    
    Debug::log("flac", "STREAMINFO recovery successful with minimal parameters");
    return true;
}

bool FLACDemuxer::validateStreamInfoParameters() const
{
    std::lock_guard<std::mutex> metadata_lock(m_metadata_mutex);
    return validateStreamInfoParameters_unlocked();
}

bool FLACDemuxer::validateStreamInfoParameters_unlocked() const
{
    Debug::log("flac", "[validateStreamInfoParameters_unlocked] Validating STREAMINFO per RFC 9639");
    
    // Check basic validity first
    if (!m_streaminfo.isValid()) {
        Debug::log("flac", "[validateStreamInfoParameters_unlocked] STREAMINFO basic validation failed");
        return false;
    }
    
    // RFC 9639 Section 8.2 validation requirements
    
    // Sample rate validation (RFC 9639: must not be 0 for audio)
    if (m_streaminfo.sample_rate == 0) {
        Debug::log("flac", "[validateStreamInfoParameters_unlocked] Invalid sample rate: 0 Hz (RFC 9639 violation)");
        return false;
    }
    
    // Sample rate should be reasonable (1 Hz to 655350 Hz per 20-bit field)
    if (m_streaminfo.sample_rate > 1048575) { // 2^20 - 1
        Debug::log("flac", "[validateStreamInfoParameters_unlocked] Sample rate exceeds 20-bit limit: ", m_streaminfo.sample_rate, " Hz");
        return false;
    }
    
    // Channels validation (RFC 9639: 1-8 channels, stored as channels-1 in 3 bits)
    if (m_streaminfo.channels < 1 || m_streaminfo.channels > 8) {
        Debug::log("flac", "[validateStreamInfoParameters_unlocked] Invalid channel count: ", static_cast<int>(m_streaminfo.channels), " (RFC 9639: 1-8 channels)");
        return false;
    }
    
    // Bits per sample validation (RFC 9639: 4-32 bits, stored as bits-1 in 5 bits)
    if (m_streaminfo.bits_per_sample < 4 || m_streaminfo.bits_per_sample > 32) {
        Debug::log("flac", "[validateStreamInfoParameters_unlocked] Invalid bits per sample: ", static_cast<int>(m_streaminfo.bits_per_sample), " (RFC 9639: 4-32 bits)");
        return false;
    }
    
    // Block size validation (RFC 9639: 16-65535 samples)
    if (m_streaminfo.min_block_size < 16 || m_streaminfo.min_block_size > 65535) {
        Debug::log("flac", "[validateStreamInfoParameters_unlocked] Invalid minimum block size: ", m_streaminfo.min_block_size, " (RFC 9639: 16-65535)");
        return false;
    }
    
    if (m_streaminfo.max_block_size < 16 || m_streaminfo.max_block_size > 65535) {
        Debug::log("flac", "[validateStreamInfoParameters_unlocked] Invalid maximum block size: ", m_streaminfo.max_block_size, " (RFC 9639: 16-65535)");
        return false;
    }
    
    // Max block size should be >= min block size (RFC 9639)
    if (m_streaminfo.max_block_size < m_streaminfo.min_block_size) {
        Debug::log("flac", "[validateStreamInfoParameters_unlocked] Maximum block size (", m_streaminfo.max_block_size, 
                  ") is less than minimum block size (", m_streaminfo.min_block_size, ") - RFC 9639 violation");
        return false;
    }
    
    // Frame sizes validation (if specified)
    if (m_streaminfo.min_frame_size > 0 && m_streaminfo.max_frame_size > 0) {
        if (m_streaminfo.max_frame_size < m_streaminfo.min_frame_size) {
            Debug::log("flac", "[validateStreamInfoParameters_unlocked] Maximum frame size (", m_streaminfo.max_frame_size, 
                      ") is less than minimum frame size (", m_streaminfo.min_frame_size, ")");
            return false;
        }
        
        // Frame sizes should not exceed 24-bit field limit (16MB)
        const uint32_t max_frame_size_limit = (1U << 24) - 1; // 24-bit field
        if (m_streaminfo.max_frame_size > max_frame_size_limit) {
            Debug::log("flac", "[validateStreamInfoParameters_unlocked] Maximum frame size exceeds 24-bit limit: ", m_streaminfo.max_frame_size, " bytes");
            return false;
        }
    }
    
    // Total samples validation (36-bit field allows up to 2^36-1 samples)
    const uint64_t max_total_samples = (1ULL << 36) - 1;
    if (m_streaminfo.total_samples > max_total_samples) {
        Debug::log("flac", "[validateStreamInfoParameters_unlocked] Total samples exceeds 36-bit limit: ", m_streaminfo.total_samples);
        return false;
    }
    
    // Additional reasonableness check for total samples
    if (m_streaminfo.total_samples > 0) {
        // Check for reasonable duration (up to 24 hours at any sample rate)
        uint64_t max_reasonable_samples = static_cast<uint64_t>(m_streaminfo.sample_rate) * 24 * 3600;
        if (m_streaminfo.total_samples > max_reasonable_samples) {
            Debug::log("flac", "[validateStreamInfoParameters_unlocked] Warning: Total samples seems unreasonably large: ", m_streaminfo.total_samples, " (>24 hours)");
            // This is just a warning, not a fatal error for RFC compliance
        }
    }
    
    Debug::log("flac", "[validateStreamInfoParameters_unlocked] STREAMINFO parameters validation passed (RFC 9639 compliant)");
    return true;
}

bool FLACDemuxer::recoverFromCorruptedMetadata()
{
    Debug::log("flac", "FLACDemuxer::recoverFromCorruptedMetadata() - attempting metadata recovery");
    
    if (!m_handler) {
        return false;
    }
    
    // Clear any partially parsed metadata
    m_seektable.clear();
    m_vorbis_comments.clear();
    m_pictures.clear();
    
    // If we don't have valid STREAMINFO, try to recover it
    if (!m_streaminfo.isValid()) {
        if (!attemptStreamInfoRecovery()) {
            Debug::log("flac", "Failed to recover STREAMINFO");
            return false;
        }
    }
    
    // Try to find the start of audio data by searching for frame sync
    Debug::log("flac", "Searching for audio data start after corrupted metadata");
    
    // Start searching from after the fLaC marker (position 4)
    if (!m_handler->seek(4, SEEK_SET)) {
        Debug::log("flac", "Failed to seek to start metadata search");
        return false;
    }
    
    // Search for the first valid FLAC frame
    FLACFrame first_frame;
    if (findNextFrame_unlocked(first_frame)) {
        m_audio_data_offset = first_frame.file_offset;
        Debug::log("flac", "Found audio data start at offset: ", m_audio_data_offset);
        return true;
    }
    
    Debug::log("flac", "Could not find valid audio data after corrupted metadata");
    return false;
}

bool FLACDemuxer::resynchronizeToNextFrame()
{
    Debug::log("flac", "FLACDemuxer::resynchronizeToNextFrame() - attempting frame resynchronization");
    
    if (!m_handler) {
        return false;
    }
    
    // Save current position for logging
    int64_t start_position = m_handler->tell();
    
    // Try to find the next valid frame
    FLACFrame frame;
    if (findNextFrame_unlocked(frame)) {
        Debug::log("flac", "Resynchronized to frame at offset ", frame.file_offset, 
                  " (searched from ", start_position, ")");
        
        // Update position tracking to the found frame
        updatePositionTracking(frame.sample_offset, frame.file_offset);
        return true;
    }
    
    Debug::log("flac", "Failed to resynchronize - no valid frame found");
    return false;
}

void FLACDemuxer::provideDefaultStreamInfo()
{
    Debug::log("flac", "FLACDemuxer::provideDefaultStreamInfo() - providing default STREAMINFO");
    
    // Provide reasonable defaults for a FLAC file
    m_streaminfo.sample_rate = 44100;      // CD quality default
    m_streaminfo.channels = 2;             // Stereo default
    m_streaminfo.bits_per_sample = 16;     // CD quality default
    m_streaminfo.min_block_size = 4096;    // Common FLAC block size
    m_streaminfo.max_block_size = 4096;    // Same as min for simplicity
    m_streaminfo.min_frame_size = 0;       // Unknown
    m_streaminfo.max_frame_size = 0;       // Unknown
    m_streaminfo.total_samples = 0;        // Unknown
    
    // Clear MD5 signature
    std::memset(m_streaminfo.md5_signature, 0, sizeof(m_streaminfo.md5_signature));
    
    Debug::log("flac", "Default STREAMINFO provided:");
    Debug::log("flac", "  Sample rate: ", m_streaminfo.sample_rate, " Hz");
    Debug::log("flac", "  Channels: ", static_cast<int>(m_streaminfo.channels));
    Debug::log("flac", "  Bits per sample: ", static_cast<int>(m_streaminfo.bits_per_sample));
    Debug::log("flac", "  Block size: ", m_streaminfo.min_block_size);
}

// Frame-level error recovery methods

bool FLACDemuxer::handleLostFrameSync()
{
    Debug::log("flac", "FLACDemuxer::handleLostFrameSync() - attempting to recover from lost frame sync");
    
    if (!m_handler) {
        return false;
    }
    
    // Save current position
    int64_t start_position = m_handler->tell();
    
    // Try to resynchronize to the next valid frame
    if (resynchronizeToNextFrame()) {
        Debug::log("flac", "Successfully recovered from lost frame sync");
        return true;
    }
    
    // If resynchronization failed, try a more aggressive search
    Debug::log("flac", "Standard resynchronization failed, trying aggressive search");
    
    // Search further ahead for frame sync
    const size_t search_buffer_size = 64 * 1024;  // 64KB search buffer
    const uint64_t max_search_distance = 1024 * 1024;  // Search up to 1MB
    std::vector<uint8_t> search_buffer(search_buffer_size);
    
    uint64_t bytes_searched = 0;
    
    while (bytes_searched < max_search_distance && !m_handler->eof()) {
        size_t bytes_read = m_handler->read(search_buffer.data(), 1, search_buffer_size);
        if (bytes_read < 2) {
            break;
        }
        
        // Look for potential FLAC sync codes using RFC 9639 compliant validation
        for (size_t i = 0; i < bytes_read - 1; i++) {
            // Use RFC 9639 compliant sync pattern validation
            if (validateFrameSync_unlocked(search_buffer.data() + i, bytes_read - i)) {
                // Found potential sync, try to validate
                uint64_t sync_position = start_position + bytes_searched + i;
                
                if (m_handler->seek(sync_position, SEEK_SET)) {
                    FLACFrame test_frame;
                    test_frame.file_offset = sync_position;
                    
                    if (parseFrameHeader(test_frame) && validateFrameHeader(test_frame)) {
                        Debug::log("flac", "Found valid frame sync at position ", sync_position, 
                                  " after searching ", bytes_searched + i, " bytes");
                        
                        // Update position tracking
                        updatePositionTracking(test_frame.sample_offset, sync_position);
                        return true;
                    }
                }
            }
        }
        
        bytes_searched += bytes_read;
        
        // Overlap search to avoid missing sync codes at buffer boundaries
        if (bytes_read == search_buffer_size && bytes_searched < max_search_distance) {
            if (!m_handler->seek(static_cast<off_t>(start_position + bytes_searched - 1), SEEK_SET)) {
                break;
            }
            bytes_searched -= 1;  // Account for overlap
        }
    }
    
    Debug::log("flac", "Failed to recover frame sync after searching ", bytes_searched, " bytes");
    
    // Restore original position if recovery failed
    m_handler->seek(start_position, SEEK_SET);
    return false;
}

bool FLACDemuxer::skipCorruptedFrame()
{
    Debug::log("flac", "[skipCorruptedFrame] Attempting to skip corrupted frame");
    
    if (!m_handler) {
        return false;
    }
    
    // Save current position
    int64_t start_position = m_handler->tell();
    
    // PRIORITY 3: Use consistent STREAMINFO-based estimation (same as calculateFrameSize)
    uint32_t estimated_frame_size = 0;
    
    if (m_streaminfo.isValid() && m_streaminfo.min_frame_size > 0) {
        // Use STREAMINFO minimum frame size directly - consistent with calculateFrameSize
        estimated_frame_size = m_streaminfo.min_frame_size;
        
        Debug::log("flac", "[skipCorruptedFrame] Using STREAMINFO minimum frame size: ", estimated_frame_size, " bytes");
        
        // For fixed block size streams, this is the most accurate estimate
        if (m_streaminfo.min_block_size == m_streaminfo.max_block_size) {
            Debug::log("flac", "[skipCorruptedFrame] Fixed block size stream - using minimum directly");
        }
    } else {
        // Conservative fallback consistent with calculateFrameSize
        estimated_frame_size = 64;  // Conservative minimum that handles highly compressed frames
        Debug::log("flac", "[skipCorruptedFrame] No STREAMINFO available - using conservative fallback: ", estimated_frame_size, " bytes");
    }
    
    // Try skipping by estimated frame size and look for next sync
    const int skip_attempts = 3;
    uint32_t skip_distance = estimated_frame_size / 4;  // Start with smaller skips to avoid overshooting
    
    Debug::log("flac", "[skipCorruptedFrame] Starting skip attempts with initial distance: ", skip_distance, " bytes");
    
    for (int attempt = 0; attempt < skip_attempts; attempt++) {
        uint64_t skip_position = start_position + skip_distance;
        
        Debug::log("flac", "[skipCorruptedFrame] Attempt ", attempt + 1, " - seeking to position: ", skip_position);
        
        if (!m_handler->seek(skip_position, SEEK_SET)) {
            Debug::log("flac", "[skipCorruptedFrame] Failed to seek to skip position ", skip_position);
            break;
        }
        
        // Try to find a valid frame from this position
        FLACFrame test_frame;
        if (findNextFrame_unlocked(test_frame)) {
            Debug::log("flac", "[skipCorruptedFrame] Successfully skipped corrupted frame, found next frame at ", 
                      test_frame.file_offset);
            
            // Update position tracking
            updatePositionTracking(test_frame.sample_offset, test_frame.file_offset);
            return true;
        }
        
        // Increase skip distance for next attempt, but keep it reasonable
        skip_distance = std::min(skip_distance * 2, estimated_frame_size * 2);
        Debug::log("flac", "[skipCorruptedFrame] Frame not found, increasing skip distance to: ", skip_distance, " bytes");
    }
    
    Debug::log("flac", "[skipCorruptedFrame] Failed to skip corrupted frame after ", skip_attempts, " attempts");
    
    // Restore original position
    m_handler->seek(start_position, SEEK_SET);
    return false;
}

bool FLACDemuxer::validateFrameCRC(const FLACFrame& frame, const std::vector<uint8_t>& frame_data)
{
    Debug::log("flac", "FLACDemuxer::validateFrameCRC() - validating frame CRC");
    
    // FLAC frames have a CRC-16 at the end
    if (frame_data.size() < 2) {
        Debug::log("flac", "Frame data too small for CRC validation");
        return false;
    }
    
    // For now, we'll do a basic validation
    // A full CRC-16 implementation would require the FLAC CRC polynomial
    // This is a simplified check that looks for obvious corruption
    
    // Check for reasonable frame structure
    if (frame_data.size() < 4) {
        Debug::log("flac", "Frame data suspiciously small: ", frame_data.size(), " bytes");
        return false;
    }
    
    // Verify sync code at start of frame using RFC 9639 compliant validation
    if (frame_data.size() >= 2) {
        if (!validateFrameSync_unlocked(frame_data.data(), frame_data.size())) {
            uint16_t sync_code = (static_cast<uint16_t>(frame_data[0]) << 8) | 
                                static_cast<uint16_t>(frame_data[1]);
            Debug::log("flac", "Invalid sync code in frame data: 0x", std::hex, sync_code, std::dec);
            return false;
        }
    }
    
    // Check for reasonable frame size vs. expected size
    if (frame.frame_size > 0 && frame_data.size() != frame.frame_size) {
        Debug::log("flac", "Frame data size (", frame_data.size(), 
                  ") doesn't match expected size (", frame.frame_size, ")");
        // This might not be an error if frame_size was estimated
    }
    
    // Additional heuristic checks
    
    // Check for excessive runs of identical bytes (likely corruption)
    if (frame_data.size() >= 16) {
        int max_identical_run = 0;
        int current_run = 1;
        
        for (size_t i = 1; i < std::min(frame_data.size(), size_t(64)); i++) {
            if (frame_data[i] == frame_data[i-1]) {
                current_run++;
            } else {
                max_identical_run = std::max(max_identical_run, current_run);
                current_run = 1;
            }
        }
        max_identical_run = std::max(max_identical_run, current_run);
        
        if (max_identical_run > 32) {
            Debug::log("flac", "Suspicious identical byte run of ", max_identical_run, 
                      " bytes, possible corruption");
            return false;
        }
    }
    
    Debug::log("flac", "Frame CRC validation passed (basic checks)");
    return true;
}

MediaChunk FLACDemuxer::createSilenceChunk(uint32_t block_size)
{
    Debug::log("flac", "FLACDemuxer::createSilenceChunk() - creating silence chunk with ", 
              block_size, " samples");
    
    if (!m_streaminfo.isValid() || block_size == 0) {
        Debug::log("flac", "Cannot create silence chunk - invalid parameters");
        return MediaChunk{};
    }
    
    // Create a minimal FLAC frame containing silence
    // This is a simplified approach - we create a frame header followed by minimal subframes
    
    std::vector<uint8_t> silence_frame;
    
    // FLAC frame header (simplified)
    // Sync code (14 bits) + reserved (1 bit) + blocking strategy (1 bit)
    silence_frame.push_back(0xFF);  // Sync code high byte
    silence_frame.push_back(0xF8);  // Sync code low byte + reserved + fixed blocking
    
    // Block size code (4 bits) + sample rate code (4 bits)
    // Use "get 8-bit block size from end of header" (0x6) and "use streaminfo sample rate" (0x0)
    silence_frame.push_back(0x60);
    
    // Channel assignment (4 bits) + sample size code (3 bits) + reserved (1 bit)
    uint8_t channel_assignment = 0;  // Independent channels
    if (m_streaminfo.channels == 2) {
        channel_assignment = 1;  // Left-right stereo
    } else if (m_streaminfo.channels > 2) {
        channel_assignment = m_streaminfo.channels - 1;
    }
    
    uint8_t sample_size_code = 0;  // Use streaminfo sample size
    switch (m_streaminfo.bits_per_sample) {
        case 8: sample_size_code = 1; break;
        case 12: sample_size_code = 2; break;
        case 16: sample_size_code = 4; break;
        case 20: sample_size_code = 5; break;
        case 24: sample_size_code = 6; break;
        default: sample_size_code = 0; break;  // Use streaminfo
    }
    
    silence_frame.push_back((channel_assignment << 4) | (sample_size_code << 1));
    
    // Frame number (UTF-8 coded) - use current sample position
    uint64_t frame_number = m_current_sample;
    if (frame_number < 128) {
        silence_frame.push_back(static_cast<uint8_t>(frame_number));
    } else {
        // Simplified UTF-8 encoding for larger numbers
        silence_frame.push_back(0xC0 | static_cast<uint8_t>(frame_number >> 6));
        silence_frame.push_back(0x80 | static_cast<uint8_t>(frame_number & 0x3F));
    }
    
    // Block size (8-bit, since we used code 0x6)
    if (block_size <= 256) {
        silence_frame.push_back(static_cast<uint8_t>(block_size - 1));
    } else {
        silence_frame.push_back(255);  // Maximum 8-bit value
        block_size = 256;  // Adjust block size to match
    }
    
    // Header CRC-8 (simplified - just use 0)
    silence_frame.push_back(0x00);
    
    // Subframes (one per channel) - each subframe is a constant value (silence)
    for (int ch = 0; ch < m_streaminfo.channels; ch++) {
        // Subframe header: type (1 bit) + wasted bits (6 bits) + type-specific (1 bit)
        // Type 0 = CONSTANT, no wasted bits
        silence_frame.push_back(0x00);
        
        // Constant value (silence = 0, encoded as signed integer)
        // For simplicity, just add zero bytes for the sample size
        int bytes_per_sample = (m_streaminfo.bits_per_sample + 7) / 8;
        for (int b = 0; b < bytes_per_sample; b++) {
            silence_frame.push_back(0x00);
        }
    }
    
    // Frame CRC-16 (simplified - just use 0x0000)
    silence_frame.push_back(0x00);
    silence_frame.push_back(0x00);
    
    // Create MediaChunk
    MediaChunk chunk(1, std::move(silence_frame));
    chunk.timestamp_samples = m_current_sample;
    chunk.is_keyframe = true;
    chunk.file_offset = m_current_offset;
    
    Debug::log("flac", "Created silence chunk: ", block_size, " samples, ", 
              chunk.data.size(), " bytes");
    
    return chunk;
}

// Memory management method implementations

const std::vector<uint8_t>& FLACPicture::getData(IOHandler* handler) const
{
    // Return cached data if available
    if (!cached_data.empty()) {
        return cached_data;
    }
    
    // Load data from file if handler is available
    if (handler && data_size > 0 && data_offset > 0) {
        // Save current position
        long current_pos = handler->tell();
        
        // Seek to picture data
        if (handler->seek(data_offset, SEEK_SET)) {
            cached_data.resize(data_size);
            size_t bytes_read = handler->read(cached_data.data(), 1, data_size);
            
            if (bytes_read != data_size) {
                // Partial read, resize to actual data
                cached_data.resize(bytes_read);
            }
            
            // Restore original position
            handler->seek(current_pos, SEEK_SET);
        }
    }
    
    return cached_data;
}

void FLACDemuxer::initializeBuffers()
{
    Debug::log("flac", "FLACDemuxer::initializeBuffers() - initializing reusable buffers");
    
    // Pre-allocate reusable buffers to avoid frequent allocations
    m_frame_buffer.reserve(FRAME_BUFFER_SIZE);
    m_sync_buffer.reserve(SYNC_SEARCH_BUFFER_SIZE);
    
    Debug::log("flac", "Initialized buffers: frame=", FRAME_BUFFER_SIZE, 
              " bytes, sync=", SYNC_SEARCH_BUFFER_SIZE, " bytes");
}

void FLACDemuxer::optimizeSeekTable()
{
    Debug::log("flac", "FLACDemuxer::optimizeSeekTable() - optimizing seek table memory usage");
    
    if (m_seektable.empty()) {
        return;
    }
    
    size_t original_size = m_seektable.size();
    
    // Remove placeholder entries (they don't provide useful seek information)
    m_seektable.erase(
        std::remove_if(m_seektable.begin(), m_seektable.end(),
                      [](const FLACSeekPoint& point) { return point.isPlaceholder(); }),
        m_seektable.end()
    );
    
    // Limit seek table size to prevent memory exhaustion
    if (m_seektable.size() > MAX_SEEK_TABLE_ENTRIES) {
        Debug::log("flac", "Seek table too large (", m_seektable.size(), 
                  " entries), reducing to ", MAX_SEEK_TABLE_ENTRIES);
        
        // Keep evenly distributed entries
        std::vector<FLACSeekPoint> optimized_table;
        optimized_table.reserve(MAX_SEEK_TABLE_ENTRIES);
        
        double step = static_cast<double>(m_seektable.size()) / MAX_SEEK_TABLE_ENTRIES;
        for (size_t i = 0; i < MAX_SEEK_TABLE_ENTRIES; ++i) {
            size_t index = static_cast<size_t>(i * step);
            if (index < m_seektable.size()) {
                optimized_table.push_back(m_seektable[index]);
            }
        }
        
        m_seektable = std::move(optimized_table);
    }
    
    // Shrink to fit to free unused memory
    m_seektable.shrink_to_fit();
    
    Debug::log("flac", "Seek table optimized: ", original_size, " -> ", 
              m_seektable.size(), " entries");
}

void FLACDemuxer::limitVorbisComments()
{
    Debug::log("flac", "FLACDemuxer::limitVorbisComments() - limiting Vorbis comments");
    
    if (m_vorbis_comments.empty()) {
        return;
    }
    
    size_t original_count = m_vorbis_comments.size();
    
    // Remove excessively long comments
    auto it = m_vorbis_comments.begin();
    while (it != m_vorbis_comments.end()) {
        if (it->first.length() + it->second.length() > MAX_COMMENT_LENGTH) {
            Debug::log("flac", "Removing oversized comment: ", it->first);
            it = m_vorbis_comments.erase(it);
        } else {
            ++it;
        }
    }
    
    // Limit total number of comments
    if (m_vorbis_comments.size() > MAX_VORBIS_COMMENTS) {
        Debug::log("flac", "Too many Vorbis comments (", m_vorbis_comments.size(), 
                  "), keeping only first ", MAX_VORBIS_COMMENTS);
        
        // Keep only the first MAX_VORBIS_COMMENTS entries
        // Priority order: standard fields first, then others
        std::vector<std::string> priority_fields = {
            "TITLE", "ARTIST", "ALBUM", "DATE", "TRACKNUMBER", "GENRE", "ALBUMARTIST"
        };
        
        std::map<std::string, std::string> limited_comments;
        
        // Add priority fields first
        for (const auto& field : priority_fields) {
            auto found = m_vorbis_comments.find(field);
            if (found != m_vorbis_comments.end()) {
                limited_comments[found->first] = found->second;
                if (limited_comments.size() >= MAX_VORBIS_COMMENTS) break;
            }
        }
        
        // Add remaining fields if space available
        if (limited_comments.size() < MAX_VORBIS_COMMENTS) {
            for (const auto& comment : m_vorbis_comments) {
                if (limited_comments.find(comment.first) == limited_comments.end()) {
                    limited_comments[comment.first] = comment.second;
                    if (limited_comments.size() >= MAX_VORBIS_COMMENTS) break;
                }
            }
        }
        
        m_vorbis_comments = std::move(limited_comments);
    }
    
    Debug::log("flac", "Vorbis comments limited: ", original_count, " -> ", 
              m_vorbis_comments.size(), " entries");
}

void FLACDemuxer::limitPictureStorage()
{
    Debug::log("flac", "FLACDemuxer::limitPictureStorage() - limiting picture storage");
    
    if (m_pictures.empty()) {
        return;
    }
    
    size_t original_count = m_pictures.size();
    
    // Remove pictures that are too large
    m_pictures.erase(
        std::remove_if(m_pictures.begin(), m_pictures.end(),
                      [](const FLACPicture& pic) { 
                          return pic.data_size > MAX_PICTURE_SIZE; 
                      }),
        m_pictures.end()
    );
    
    // Limit total number of pictures
    if (m_pictures.size() > MAX_PICTURES) {
        Debug::log("flac", "Too many pictures (", m_pictures.size(), 
                  "), keeping only first ", MAX_PICTURES);
        m_pictures.resize(MAX_PICTURES);
    }
    
    // Clear any cached picture data to save memory
    for (auto& picture : m_pictures) {
        picture.clearCache();
    }
    
    Debug::log("flac", "Picture storage limited: ", original_count, " -> ", 
              m_pictures.size(), " pictures");
}

size_t FLACDemuxer::calculateMemoryUsage() const
{
    size_t total_usage = 0;
    
    // Seek table memory
    total_usage += m_seektable.size() * sizeof(FLACSeekPoint);
    
    // Vorbis comments memory
    for (const auto& comment : m_vorbis_comments) {
        total_usage += comment.first.size() + comment.second.size() + 
                      sizeof(std::pair<std::string, std::string>);
    }
    
    // Picture metadata memory (not including cached data)
    total_usage += m_pictures.size() * sizeof(FLACPicture);
    for (const auto& picture : m_pictures) {
        total_usage += picture.mime_type.size() + picture.description.size();
        total_usage += picture.cached_data.size(); // Include cached data if present
    }
    
    // Buffer memory
    total_usage += m_frame_buffer.capacity();
    total_usage += m_sync_buffer.capacity();
    
    return total_usage;
}

void FLACDemuxer::freeUnusedMemory()
{
    Debug::log("flac", "FLACDemuxer::freeUnusedMemory() - freeing unused memory");
    
    size_t before_usage = calculateMemoryUsage();
    
    // Optimize all metadata containers
    optimizeSeekTable();
    limitVorbisComments();
    limitPictureStorage();
    
    // Clear cached picture data
    for (auto& picture : m_pictures) {
        picture.clearCache();
    }
    
    // Shrink buffers if they're oversized
    if (m_frame_buffer.capacity() > FRAME_BUFFER_SIZE * 2) {
        m_frame_buffer.clear();
        m_frame_buffer.shrink_to_fit();
        m_frame_buffer.reserve(FRAME_BUFFER_SIZE);
    }
    
    if (m_sync_buffer.capacity() > SYNC_SEARCH_BUFFER_SIZE * 2) {
        m_sync_buffer.clear();
        m_sync_buffer.shrink_to_fit();
        m_sync_buffer.reserve(SYNC_SEARCH_BUFFER_SIZE);
    }
    
    size_t after_usage = calculateMemoryUsage();
    m_memory_usage_bytes = after_usage;
    
    Debug::log("flac", "Memory usage: ", before_usage, " -> ", after_usage, 
              " bytes (freed ", (before_usage - after_usage), " bytes)");
}

bool FLACDemuxer::ensureBufferCapacity(std::vector<uint8_t>& buffer, size_t required_size) const
{
    // Prevent excessive memory allocation
    if (required_size > MAX_FRAME_SIZE) {
        Debug::log("flac", "Requested buffer size too large: ", required_size, 
                  " bytes (max: ", MAX_FRAME_SIZE, ")");
        return false;
    }
    
    // Resize buffer if necessary
    if (buffer.size() < required_size) {
        try {
            buffer.resize(required_size);
            return true;
        } catch (const std::bad_alloc& e) {
            Debug::log("flac", "Failed to allocate buffer of size ", required_size, 
                      " bytes: ", e.what());
            return false;
        }
    }
    
    return true;
}

bool FLACDemuxer::recoverFromFrameError()
{
    Debug::log("flac", "FLACDemuxer::recoverFromFrameError() - attempting general frame error recovery");
    
    if (!m_handler) {
        return false;
    }
    
    // Try multiple recovery strategies
    
    // Strategy 1: Try to resynchronize to next frame
    if (resynchronizeToNextFrame()) {
        Debug::log("flac", "Recovered using frame resynchronization");
        return true;
    }
    
    // Strategy 2: Try to handle lost frame sync
    if (handleLostFrameSync()) {
        Debug::log("flac", "Recovered using lost sync recovery");
        return true;
    }
    
    // Strategy 3: Skip ahead by a reasonable amount and try again
    int64_t current_pos = m_handler->tell();
    const uint64_t skip_amounts[] = {1024, 4096, 16384, 65536};  // Progressive skip sizes
    
    for (size_t i = 0; i < sizeof(skip_amounts) / sizeof(skip_amounts[0]); i++) {
        uint64_t skip_pos = current_pos + skip_amounts[i];
        
        if (m_handler->seek(skip_pos, SEEK_SET)) {
            if (resynchronizeToNextFrame()) {
                Debug::log("flac", "Recovered by skipping ", skip_amounts[i], " bytes");
                return true;
            }
        }
    }
    
    // Strategy 4: Try to find any valid FLAC frame in the remaining file
    if (m_handler->seek(current_pos, SEEK_SET)) {
        FLACFrame recovery_frame;
        if (findNextFrame_unlocked(recovery_frame)) {
            Debug::log("flac", "Found valid frame during general recovery");
            updatePositionTracking(recovery_frame.sample_offset, recovery_frame.file_offset);
            return true;
        }
    }
    
    Debug::log("flac", "All frame error recovery strategies failed");
    return false;
}

// Performance optimization method implementations

size_t FLACDemuxer::findSeekPointIndex(uint64_t target_sample) const
{
    if (m_seektable.empty()) {
        return SIZE_MAX;  // No seek table available
    }
    
    // Ensure seek table is sorted for binary search
    if (!m_seek_table_sorted) {
        // Sort seek table by sample number for efficient binary search
        std::sort(const_cast<std::vector<FLACSeekPoint>&>(m_seektable).begin(),
                 const_cast<std::vector<FLACSeekPoint>&>(m_seektable).end(),
                 [](const FLACSeekPoint& a, const FLACSeekPoint& b) {
                     return a.sample_number < b.sample_number;
                 });
        const_cast<FLACDemuxer*>(this)->m_seek_table_sorted = true;
        Debug::log("flac", "Sorted seek table for binary search optimization");
    }
    
    // Use binary search to find the best seek point
    // Find the largest seek point that is <= target_sample
    auto it = std::upper_bound(m_seektable.begin(), m_seektable.end(), target_sample,
                              [](uint64_t sample, const FLACSeekPoint& point) {
                                  return sample < point.sample_number;
                              });
    
    if (it == m_seektable.begin()) {
        // Target is before first seek point, use first point
        return 0;
    }
    
    // Use the previous seek point (largest one <= target)
    --it;
    size_t index = static_cast<size_t>(it - m_seektable.begin());
    
    Debug::log("flac", "Binary search found seek point ", index, " for sample ", target_sample,
              " (seek point at sample ", it->sample_number, ")");
    
    return index;
}

bool FLACDemuxer::optimizedFrameSync(uint64_t start_offset, FLACFrame& frame)
{
    Debug::log("flac", "FLACDemuxer::optimizedFrameSync() - optimized frame sync search");
    
    if (!m_handler) {
        return false;
    }
    
    // Seek to start position
    if (!m_handler->seek(start_offset, SEEK_SET)) {
        return false;
    }
    
    // Use bit manipulation optimization for sync detection
    // Read larger chunks and use efficient bit operations
    const size_t chunk_size = std::min(SYNC_SEARCH_BUFFER_SIZE, static_cast<size_t>(4096));
    if (!ensureBufferCapacity(m_sync_buffer, chunk_size)) {
        return false;
    }
    
    uint64_t bytes_searched = 0;
    const uint64_t max_search = 64 * 1024;  // Limit search distance for performance
    
    // Optimization: Use 32-bit reads for faster sync detection
    while (bytes_searched < max_search && !m_handler->eof()) {
        size_t bytes_read = m_handler->read(m_sync_buffer.data(), 1, chunk_size);
        if (bytes_read < 4) break;  // Need at least 4 bytes for frame header
        
        // Optimized sync search using 32-bit operations
        for (size_t i = 0; i <= bytes_read - 4; i += 2) {  // Step by 2 for efficiency
            // Read 32 bits at once for faster processing
            uint32_t word = (static_cast<uint32_t>(m_sync_buffer[i]) << 24) |
                           (static_cast<uint32_t>(m_sync_buffer[i + 1]) << 16) |
                           (static_cast<uint32_t>(m_sync_buffer[i + 2]) << 8) |
                           static_cast<uint32_t>(m_sync_buffer[i + 3]);
            
            // Check for FLAC sync pattern in the upper 16 bits using RFC 9639 validation
            uint8_t sync_bytes[2] = {
                static_cast<uint8_t>((word >> 24) & 0xFF),
                static_cast<uint8_t>((word >> 16) & 0xFF)
            };
            if (validateFrameSync_unlocked(sync_bytes, 2)) {
                // Found potential sync, validate frame
                uint64_t sync_position = start_offset + bytes_searched + i;
                
                if (!m_handler->seek(sync_position, SEEK_SET)) {
                    continue;
                }
                
                frame.file_offset = sync_position;
                if (parseFrameHeader(frame) && validateFrameHeader(frame)) {
                    Debug::log("flac", "Optimized sync found frame at position ", sync_position);
                    return true;
                }
            }
            
            // Also check the next 16 bits (overlapping search)
            sync_bytes[0] = static_cast<uint8_t>((word >> 16) & 0xFF);
            sync_bytes[1] = static_cast<uint8_t>((word >> 8) & 0xFF);
            if (validateFrameSync_unlocked(sync_bytes, 2)) {
                uint64_t sync_position = start_offset + bytes_searched + i + 2;
                
                if (!m_handler->seek(sync_position, SEEK_SET)) {
                    continue;
                }
                
                frame.file_offset = sync_position;
                if (parseFrameHeader(frame) && validateFrameHeader(frame)) {
                    Debug::log("flac", "Optimized sync found frame at position ", sync_position);
                    return true;
                }
            }
        }
        
        bytes_searched += bytes_read;
        
        // Overlap to avoid missing sync codes at boundaries
        if (bytes_read >= 4) {
            start_offset = start_offset + bytes_searched - 2;
            bytes_searched = 2;
            if (!m_handler->seek(start_offset, SEEK_SET)) {
                break;
            }
        }
    }
    
    return false;
}

void FLACDemuxer::prefetchNextFrame()
{
    // PERFORMANCE OPTIMIZATION: Intelligent prefetching for network streams
    if (!m_is_network_stream || !m_handler) {
        return;
    }
    
    Debug::log("flac", "[prefetchNextFrame] Optimized prefetching for network stream");
    
    // Save current position
    long current_pos = m_handler->tell();
    
    // PERFORMANCE OPTIMIZATION: Use adaptive prefetch size based on frame size estimates
    size_t prefetch_size;
    if (m_streaminfo.isValid() && m_streaminfo.min_frame_size > 0) {
        // Prefetch enough for several frames based on STREAMINFO
        prefetch_size = m_streaminfo.min_frame_size * 8;  // 8 frames worth
        prefetch_size = std::min(prefetch_size, static_cast<size_t>(16384));  // Cap at 16KB
        prefetch_size = std::max(prefetch_size, static_cast<size_t>(1024));   // Minimum 1KB
    } else {
        // Conservative prefetch for unknown streams
        prefetch_size = 4096;  // 4KB default
    }
    
    if (!ensureBufferCapacity(m_readahead_buffer, prefetch_size)) {
        Debug::log("flac", "[prefetchNextFrame] Failed to allocate prefetch buffer");
        return;
    }
    
    // PERFORMANCE OPTIMIZATION: Non-blocking prefetch read
    size_t bytes_read = m_handler->read(m_readahead_buffer.data(), 1, prefetch_size);
    
    // Restore position efficiently
    if (m_handler->seek(current_pos, SEEK_SET) != 0) {
        Debug::log("flac", "[prefetchNextFrame] Warning: Failed to restore position after prefetch");
    }
    
    if (bytes_read > 0) {
        Debug::log("flac", "[prefetchNextFrame] Prefetched ", bytes_read, " bytes (", 
                  bytes_read / (m_streaminfo.min_frame_size > 0 ? m_streaminfo.min_frame_size : 64), 
                  " estimated frames)");
    }
}

bool FLACDemuxer::isNetworkStream() const
{
    // Detect if this is likely a network stream based on IOHandler type
    // This is a heuristic - we could check the IOHandler type more specifically
    if (!m_handler) {
        return false;
    }
    
    // Check if file size is unknown (common for network streams)
    if (m_file_size == 0) {
        return true;
    }
    
    // Check if seeking is slow (another indicator of network streams)
    // This is a simple heuristic - in practice we might time seek operations
    return false;  // Default to false for now
}

void FLACDemuxer::optimizeForNetworkStreaming()
{
    Debug::log("flac", "[optimizeForNetworkStreaming] Optimizing for network performance");
    
    m_is_network_stream = isNetworkStream();
    
    if (m_is_network_stream) {
        Debug::log("flac", "[optimizeForNetworkStreaming] Network stream detected, enabling optimizations");
        
        // PERFORMANCE OPTIMIZATION: Pre-allocate optimized read-ahead buffer
        size_t readahead_size = 32 * 1024;  // 32KB for network efficiency
        if (m_streaminfo.isValid() && m_streaminfo.min_frame_size > 0) {
            // Size buffer to hold multiple frames
            readahead_size = std::max(readahead_size, static_cast<size_t>(m_streaminfo.min_frame_size * 16));
        }
        m_readahead_buffer.reserve(readahead_size);
        
        Debug::log("flac", "[optimizeForNetworkStreaming] Network optimizations enabled with ", 
                  readahead_size, " byte buffer");
    } else {
        Debug::log("flac", "Local file detected, using standard optimizations");
    }
}

// Thread safety helper methods

void FLACDemuxer::setErrorState(bool error_state)
{
    m_error_state.store(error_state);
    if (error_state) {
        Debug::log("flac", "FLACDemuxer error state set to true");
    }
}

bool FLACDemuxer::getErrorState() const
{
    return m_error_state.load();
}

// RFC 9639 Sync Pattern Validation Methods

bool FLACDemuxer::validateFrameSync_unlocked(const uint8_t* data, size_t size) const
{
    if (size < 2) {
        return false;
    }
    
    // RFC 9639 Section 9.1: 15-bit frame sync code 0b111111111111100 followed by blocking strategy bit
    // Breaking down the 15-bit sync code: 0b111111111111100
    // - 13 ones: 1111111111111
    // - 2 zeros: 00
    // Total: 15 bits
    //
    // With blocking strategy bit, the 16-bit patterns are:
    // - Fixed block (strategy=0):    1111111111111000 = 0xFFF8  
    // - Variable block (strategy=1): 1111111111111001 = 0xFFF9
    
    uint16_t sync_word = (static_cast<uint16_t>(data[0]) << 8) | static_cast<uint16_t>(data[1]);
    
    // Check for exact sync patterns (RFC 9639 compliant)
    return (sync_word == 0xFFF8 || sync_word == 0xFFF9);
}
bool FLACDemuxer::searchSyncPattern_unlocked(const uint8_t* buffer, size_t buffer_size, size_t& sync_offset) const
{
    if (buffer_size < 2) {
        return false;
    }
    
    // Performance optimization: Search for 0xFF first, then validate second byte
    // This is much faster than checking every 16-bit combination
    for (size_t i = 0; i <= buffer_size - 2; ++i) {
        // Quick check: first byte must be 0xFF for FLAC sync pattern
        if (buffer[i] == 0xFF) {
            uint8_t second_byte = buffer[i + 1];
            
            // RFC 9639: Second byte upper 7 bits should be 0x7C (0b1111100)
            // Valid patterns: 0xF8 (fixed block) or 0xF9 (variable block)
            if ((second_byte & 0xFE) == 0xF8) {
                // Additional validation: check if this looks like a valid frame header
                if (validateFrameSync_unlocked(buffer + i, buffer_size - i)) {
                    sync_offset = i;
                    return true;
                }
            }
        }
    }
    
    return false;
}

// Performance Optimization Methods

void FLACDemuxer::optimizeFrameProcessingPerformance()
{
    Debug::log("flac", "[optimizeFrameProcessingPerformance] Applying frame processing optimizations");
    
    // PERFORMANCE OPTIMIZATION 1: Pre-allocate reusable buffers to avoid allocations
    if (m_streaminfo.isValid() && m_streaminfo.min_frame_size > 0) {
        size_t optimal_buffer_size = m_streaminfo.min_frame_size * 2;  // Double for safety
        optimal_buffer_size = std::max(optimal_buffer_size, static_cast<size_t>(1024));
        optimal_buffer_size = std::min(optimal_buffer_size, static_cast<size_t>(8192));
        
        if (!ensureBufferCapacity(m_frame_buffer, optimal_buffer_size)) {
            Debug::log("flac", "[optimizeFrameProcessingPerformance] Warning: Failed to pre-allocate frame buffer");
        } else {
            Debug::log("flac", "[optimizeFrameProcessingPerformance] Pre-allocated frame buffer: ", optimal_buffer_size, " bytes");
        }
        
        // Pre-allocate sync search buffer
        size_t sync_buffer_size = 256;  // Optimized search scope
        if (!ensureBufferCapacity(m_sync_buffer, sync_buffer_size)) {
            Debug::log("flac", "[optimizeFrameProcessingPerformance] Warning: Failed to pre-allocate sync buffer");
        } else {
            Debug::log("flac", "[optimizeFrameProcessingPerformance] Pre-allocated sync buffer: ", sync_buffer_size, " bytes");
        }
    }
    
    // PERFORMANCE OPTIMIZATION 2: Initialize frame indexing for faster seeking
    if (m_frame_indexing_enabled && m_frame_index.empty()) {
        Debug::log("flac", "[optimizeFrameProcessingPerformance] Initializing frame indexing for performance");
        // Frame indexing will be populated during playback
    }
    
    // PERFORMANCE OPTIMIZATION 3: Optimize for stream characteristics
    if (m_streaminfo.isValid()) {
        bool is_fixed_block_size = (m_streaminfo.min_block_size == m_streaminfo.max_block_size);
        bool is_highly_compressed = (m_streaminfo.min_frame_size > 0 && m_streaminfo.min_frame_size < 64);
        
        Debug::log("flac", "[optimizeFrameProcessingPerformance] Stream characteristics:");
        Debug::log("flac", "  Fixed block size: ", is_fixed_block_size);
        Debug::log("flac", "  Highly compressed: ", is_highly_compressed, " (min frame size: ", m_streaminfo.min_frame_size, " bytes)");
        
        if (is_highly_compressed) {
            Debug::log("flac", "[optimizeFrameProcessingPerformance] Enabling optimizations for highly compressed stream");
            // Highly compressed streams benefit from accurate frame size estimation
        }
    }
    
    Debug::log("flac", "[optimizeFrameProcessingPerformance] Frame processing optimizations applied");
}

bool FLACDemuxer::validatePerformanceOptimizations()
{
    Debug::log("flac", "[validatePerformanceOptimizations] Validating performance optimizations");
    
    bool optimizations_valid = true;
    
    // Check buffer allocations
    if (m_frame_buffer.capacity() == 0) {
        Debug::log("flac", "[validatePerformanceOptimizations] Warning: Frame buffer not pre-allocated");
        optimizations_valid = false;
    }
    
    if (m_sync_buffer.capacity() == 0) {
        Debug::log("flac", "[validatePerformanceOptimizations] Warning: Sync buffer not pre-allocated");
        optimizations_valid = false;
    }
    
    // Check STREAMINFO availability for accurate frame size estimation
    if (!m_streaminfo.isValid() || m_streaminfo.min_frame_size == 0) {
        Debug::log("flac", "[validatePerformanceOptimizations] Warning: STREAMINFO not available for optimal frame size estimation");
        optimizations_valid = false;
    }
    
    // Check frame indexing status
    if (m_frame_indexing_enabled) {
        auto stats = m_frame_index.getStats();
        Debug::log("flac", "[validatePerformanceOptimizations] Frame index stats: ", stats.entry_count, " entries");
    }
    
    Debug::log("flac", "[validatePerformanceOptimizations] Performance optimizations ", 
              (optimizations_valid ? "validated successfully" : "have issues"));
    
    return optimizations_valid;
}

void FLACDemuxer::logPerformanceMetrics()
{
    Debug::log("flac", "[logPerformanceMetrics] Performance metrics summary:");
    
    if (m_streaminfo.isValid()) {
        Debug::log("flac", "  Stream info: ", m_streaminfo.sample_rate, " Hz, ", 
                  static_cast<int>(m_streaminfo.channels), " channels, ", 
                  static_cast<int>(m_streaminfo.bits_per_sample), " bits");
        Debug::log("flac", "  Block size range: ", m_streaminfo.min_block_size, "-", m_streaminfo.max_block_size);
        Debug::log("flac", "  Frame size range: ", m_streaminfo.min_frame_size, "-", m_streaminfo.max_frame_size, " bytes");
    }
    
    Debug::log("flac", "  Buffer capacities: frame=", m_frame_buffer.capacity(), 
              " sync=", m_sync_buffer.capacity(), " bytes");
    
    if (m_frame_indexing_enabled) {
        auto stats = m_frame_index.getStats();
        Debug::log("flac", "  Frame index: ", stats.entry_count, " entries, ", 
                  stats.memory_usage, " bytes, ", stats.coverage_percentage, "% coverage");
    }
    
    Debug::log("flac", "  Network stream: ", m_is_network_stream);
    Debug::log("flac", "  Memory usage: ", m_memory_usage_bytes, " bytes");
}
