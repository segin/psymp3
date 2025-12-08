/*
 * DemuxedStream.cpp - Stream implementation using demuxer/codec architecture
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace Demuxer {

DemuxedStream::DemuxedStream(const TagLib::String& path, uint32_t preferred_stream_id) 
    : Stream(), m_current_stream_id(preferred_stream_id) {
    m_path = path;
    
    if (!initialize()) {
        throw InvalidMediaException("Failed to initialize demuxed stream for: " + path);
    }
}

DemuxedStream::DemuxedStream(std::unique_ptr<IOHandler> handler, const TagLib::String& path, uint32_t preferred_stream_id) 
    : Stream(), m_current_stream_id(preferred_stream_id) {
    m_path = path;
    
    if (!initializeWithHandler(std::move(handler))) {
        throw InvalidMediaException("Failed to initialize demuxed stream for: " + path);
    }
}

bool DemuxedStream::initialize() {
    try {
        Debug::log("demux", "DemuxedStream::initialize() starting for path: ", m_path);
        
        // Create IOHandler
        URI uri(m_path);
        std::unique_ptr<IOHandler> handler;
        
        if (uri.scheme() == "file" || uri.scheme().isEmpty()) {
            handler = std::make_unique<FileIOHandler>(uri.path());
        } else {
            // Could add support for other schemes later
            Debug::log("demux", "DemuxedStream::initialize() unsupported URI scheme");
            return false;
        }
        
        // Create demuxer with file path hint for raw format detection
        m_demuxer = DemuxerFactory::createDemuxer(std::move(handler), uri.path().to8Bit(true));
        if (!m_demuxer) {
            Debug::log("demux", "DemuxedStream::initialize() createDemuxer returned nullptr");
            return false;
        }
        
        // Parse container
        if (!m_demuxer->parseContainer()) {
            Debug::log("demux", "DemuxedStream::initialize() parseContainer returned false");
            return false;
        }
        Debug::log("demux", "DemuxedStream::initialize() parseContainer succeeded");
        
        // Select audio stream
        if (m_current_stream_id == 0) {
            m_current_stream_id = selectBestAudioStream();
            Debug::log("demux", "DemuxedStream::initialize() selectBestAudioStream returned: ", m_current_stream_id);
        }
        
        if (m_current_stream_id == 0) {
            Debug::log("demux", "DemuxedStream::initialize() no suitable audio stream found");
            return false; // No suitable audio stream found
        }
        
        // Setup codec
        if (!setupCodec()) {
            Debug::log("demux", "DemuxedStream::initialize() setupCodec returned false");
            return false;
        }
        Debug::log("demux", "DemuxedStream::initialize() setupCodec succeeded");
        
        // Update stream properties
        updateStreamProperties();
        
        Debug::log("demux", "DemuxedStream::initialize() complete - rate=", m_rate, " channels=", m_channels);
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("demux", "DemuxedStream::initialize() exception: ", e.what());
        return false;
    }
}

uint32_t DemuxedStream::selectBestAudioStream() const {
    auto streams = m_demuxer->getStreams();
    
    // Find first audio stream
    for (const auto& stream : streams) {
        if (stream.codec_type == "audio") {
            return stream.stream_id;
        }
    }
    
    return 0; // No audio stream found
}

bool DemuxedStream::setupCodec() {
    Debug::log("demux", "DemuxedStream::setupCodec() starting for stream_id: ", m_current_stream_id);
    
    StreamInfo stream_info = m_demuxer->getStreamInfo(m_current_stream_id);
    Debug::log("demux", "DemuxedStream::setupCodec() getStreamInfo returned stream_id=", stream_info.stream_id, 
               " codec_name=", stream_info.codec_name, " codec_type=", stream_info.codec_type,
               " channels=", stream_info.channels, " sample_rate=", stream_info.sample_rate);
    
    if (stream_info.stream_id == 0) {
        Debug::log("demux", "DemuxedStream::setupCodec() failed: stream_id is 0");
        return false;
    }
    
    m_codec = AudioCodecFactory::createCodec(stream_info);
    if (!m_codec) {
        Debug::log("demux", "DemuxedStream::setupCodec() failed: createCodec returned nullptr for codec_name=", stream_info.codec_name);
        return false;
    }
    Debug::log("demux", "DemuxedStream::setupCodec() codec created, type=", m_codec->getCodecName());
    
    bool init_result = m_codec->initialize();
    Debug::log("demux", "DemuxedStream::setupCodec() codec->initialize() returned ", init_result);
    return init_result;
}

void DemuxedStream::updateStreamProperties() {
    StreamInfo stream_info = m_demuxer->getStreamInfo(m_current_stream_id);
    
    m_rate = stream_info.sample_rate;
    m_channels = stream_info.channels;
    m_bitrate = stream_info.bitrate;
    Debug::log("demux", "DemuxedStream::updateStreamProperties: duration_ms from demuxer=", stream_info.duration_ms);
    m_length = static_cast<int>(stream_info.duration_ms);
    m_slength = stream_info.duration_samples;
    m_position = 0;
    m_sposition = 0;
    m_samples_consumed = 0;
    m_eof = false;
}

size_t DemuxedStream::getData(size_t len, void *buf) {
    if (m_eof_reached || !m_codec) {
        return 0;
    }
    
    uint8_t* output_buf = static_cast<uint8_t*>(buf);
    size_t bytes_written = 0;
    
    while (bytes_written < len && !m_eof_reached) {
        // If we have a current frame with remaining data, use it
        if (m_current_frame_offset < m_current_frame.getByteCount()) {
            size_t bytes_copied = copyFrameData(m_current_frame, m_current_frame_offset,
                                               output_buf + bytes_written, len - bytes_written);
            bytes_written += bytes_copied;
            m_current_frame_offset += bytes_copied;
            
            // Note: m_samples_consumed is now updated when we get new frames,
            // not when we copy data to output buffer
            continue;
        }
        
        // Need a new frame - decode on-demand from buffered chunks
        m_current_frame = std::move(getNextFrame());
        m_current_frame_offset = 0;
        
        // Note: m_samples_consumed is now updated inside getNextFrame() 
        // based on granule positions or incremental counting
        
        if (m_current_frame.samples.empty()) {
            // Empty frame could be from header processing or actual EOF
            // Check if we have more chunks to process before declaring EOF
            size_t buffer_size = 0;
            {
                std::lock_guard<std::mutex> lock(m_buffer_mutex);
                buffer_size = m_chunk_buffer.size();
            }
            Debug::log("demux", "DemuxedStream::getData: Empty frame - chunk_buffer.size()=", buffer_size, 
                               ", demuxer.isEOF()=", m_demuxer ? m_demuxer->isEOF() : true);
            
            // Try to get more chunks before declaring EOF
            fillChunkBuffer();
            
            bool buffer_empty = false;
            {
                std::lock_guard<std::mutex> lock(m_buffer_mutex);
                buffer_empty = m_chunk_buffer.empty();
            }
            
            if (buffer_empty && m_demuxer && m_demuxer->isEOF()) {
                // Truly at EOF - no more chunks and demuxer is done
                // Use current position from frame timestamps, not sample counter
                uint64_t current_time_ms = static_cast<uint64_t>(m_position);
                Debug::log("demux", "DemuxedStream::getData: Natural EOF reached at position ", 
                                   current_time_ms, "ms (frame-based position)");
                Debug::log("demux", "DemuxedStream::getData: Setting EOF - position=", current_time_ms, "ms");
                m_eof_reached = true;
                m_eof = true;
                break;
            } else if (buffer_empty && m_demuxer && !m_demuxer->isEOF()) {
                // No chunks buffered but demuxer has more data - this shouldn't happen in normal operation
                Debug::log("demux", "DemuxedStream::getData: Buffer empty but demuxer has more data - unexpected condition");
                continue;
            } else {
                // Empty frame but more data might be available (could be header processing)
                // Continue the loop to try getting more chunks
                Debug::log("demux", "DemuxedStream::getData: Got empty frame, but more chunks available - continuing");
                continue;
            }
        }
    }
    
    // Position is updated when we process new frames (see above)
    // Don't recalculate based on sample counting as it can drift
    
    return bytes_written;
}

AudioFrame DemuxedStream::getNextFrame() {
    // Ensure we have buffered chunks to decode from
    fillChunkBuffer();
    
    size_t buffer_size_after_fill = 0;
    {
        std::lock_guard<std::mutex> lock(m_buffer_mutex);
        buffer_size_after_fill = m_chunk_buffer.size();
    }
    Debug::log("demux", "DemuxedStream::getNextFrame: After fillChunkBuffer, buffer size=", buffer_size_after_fill);
    
    Debug::log("demux", "DemuxedStream::getNextFrame: chunk_buffer size=", buffer_size_after_fill, 
                   ", demuxer EOF=", m_demuxer ? m_demuxer->isEOF() : true);
    
    // Thread-safe access to chunk buffer
    MediaChunk chunk;
    size_t chunk_size = 0;
    bool has_chunk = false;
    
    {
        std::lock_guard<std::mutex> lock(m_buffer_mutex);
        if (!m_chunk_buffer.empty()) {
            chunk = std::move(m_chunk_buffer.front());
            chunk_size = chunk.data.size();
            m_chunk_buffer.pop();
            
            // Update memory tracking
            m_current_buffer_bytes -= chunk_size;
            has_chunk = true;
        }
    }
    
    // If we have chunks, decode one on-demand
    if (has_chunk) {
        
        // Special handling for Opus: if codec is initialized, skip any header packets
        // that the demuxer may have included (e.g., after seeking to a page containing headers).
        // We detect headers by their magic signatures, not by is_keyframe flag.
        if (m_codec && m_codec->getCodecName() == "opus" && m_codec->isInitialized()) {
            // Check if this is an Opus header packet (OpusHead or OpusTags)
            if (chunk_size >= 8) {
                const uint8_t* data = chunk.data.data();
                if (std::memcmp(data, "OpusHead", 8) == 0 || 
                    std::memcmp(data, "OpusTags", 8) == 0) {
                    Debug::log("demux", "DemuxedStream: Skipping redundant Opus header chunk (size=", chunk_size, ")");
                    return AudioFrame{}; // Return empty frame, effectively discarding this header
                }
            }
        }
        
        AudioFrame frame = m_codec->decode(chunk);
        if (!frame.samples.empty()) {
            // Correct timestamp calculation for Ogg
            // For Ogg Vorbis, granule position is only valid on the last packet of each page
            // Most packets have granule -1, so we need to track samples incrementally
            if (chunk.granule_position != 0 && chunk.granule_position != static_cast<uint64_t>(-1)) {
                // Valid granule position - this is the sample count at END of this packet
                // So timestamp at start of packet = granule - samples_in_this_frame
                frame.timestamp_samples = chunk.granule_position - frame.getSampleFrameCount();
                // Update our tracking to match the authoritative granule position
                m_samples_consumed = chunk.granule_position;
            } else {
                // No valid granule - use incremental tracking
                // timestamp_samples is where this frame STARTS
                frame.timestamp_samples = m_samples_consumed;
                // Update consumed count by adding samples from this frame
                m_samples_consumed += frame.getSampleFrameCount();
            }
            frame.timestamp_ms = (frame.timestamp_samples * 1000) / m_rate;
            
            Debug::log("demux", "DemuxedStream: On-demand decoded frame with ", frame.samples.size(), " samples. Timestamp: ", frame.timestamp_ms, "ms");
            return frame;
        } else {
            Debug::log("demux", "DemuxedStream: Codec returned empty frame for chunk size=", chunk_size);
        }
    } else {
        Debug::log("demux", "DemuxedStream: No chunks available in buffer");
    }
    
    // If we reach here and demuxer is at EOF, flush codec
    if (m_demuxer && m_demuxer->isEOF() && m_codec) {
        Debug::log("demux", "DemuxedStream: Attempting to flush codec");
        AudioFrame frame = m_codec->flush();
        if (!frame.samples.empty()) {
            Debug::log("demux", "DemuxedStream: Flushed frame with ", frame.samples.size(), " samples");
            return frame;
        }
    }
    
    Debug::log("demux", "DemuxedStream: Returning empty frame");
    return AudioFrame{}; // Empty frame
}

void DemuxedStream::fillChunkBuffer() {
    // Use simple bounded buffering with memory pressure awareness
    static std::queue<MediaChunk> temp_buffer;
    static size_t temp_buffer_bytes = 0;
    static size_t max_chunks = MAX_CHUNK_BUFFER_SIZE;
    static size_t max_bytes = MAX_CHUNK_BUFFER_BYTES;
    
    // Check if we need to update memory pressure
    int memory_pressure = MemoryTracker::getInstance().getMemoryPressureLevel();
    if (memory_pressure > 70) {
        // Under high memory pressure, reduce buffer size
        max_chunks = MAX_CHUNK_BUFFER_SIZE / 2;
        max_bytes = MAX_CHUNK_BUFFER_BYTES / 2;
    } else {
        // Normal memory pressure, use default sizes
        max_chunks = MAX_CHUNK_BUFFER_SIZE;
        max_bytes = MAX_CHUNK_BUFFER_BYTES;
    }
    
    // Fill buffer until full or EOF
    // IMPORTANT: Check buffer bounds BEFORE reading to avoid dropping chunks
    while (!m_demuxer->isEOF()) {
        // Check if we have room for another chunk (estimate size)
        // Use max_frame_size from FLAC as estimate, or a reasonable default
        static constexpr size_t ESTIMATED_CHUNK_OVERHEAD = sizeof(MediaChunk) + 8192;
        if (temp_buffer.size() >= max_chunks || temp_buffer_bytes + ESTIMATED_CHUNK_OVERHEAD > max_bytes) {
            Debug::log("demux", "DemuxedStream: Bounded buffer full, will refill later");
            break; // Buffer is full - don't read more, we'll refill when there's space
        }
        
        MediaChunk chunk = m_demuxer->readChunk(m_current_stream_id);
        
        if (chunk.data.empty()) {
            Debug::log("demux", "DemuxedStream: Got empty chunk, stopping chunk buffering");
            break; // No more data
        }
        
        // Add chunk to temp buffer - we already checked bounds above
        size_t chunk_size = sizeof(MediaChunk) + chunk.data.capacity() * sizeof(uint8_t);
        temp_buffer_bytes += chunk_size;
        temp_buffer.push(std::move(chunk));
        
        // Log buffer stats
        Debug::log("demux", "DemuxedStream: Buffer stats - items: ", temp_buffer.size(),
                  ", bytes: ", temp_buffer_bytes,
                  ", fullness: ", (float)temp_buffer.size() / max_chunks * 100, "%");
    }
    
    // Transfer chunks from temp buffer to our queue (thread-safe)
    {
        std::lock_guard<std::mutex> lock(m_buffer_mutex);
        while (!temp_buffer.empty()) {
            MediaChunk chunk = std::move(temp_buffer.front());
            temp_buffer.pop();
            
            size_t chunk_size = chunk.data.size();
            temp_buffer_bytes -= (sizeof(MediaChunk) + chunk.data.capacity() * sizeof(uint8_t));
            m_current_buffer_bytes += chunk_size;
            m_chunk_buffer.push(std::move(chunk));
        }
    }
}

size_t DemuxedStream::copyFrameData(const AudioFrame& frame, size_t frame_offset, 
                                   void* output_buf, size_t output_len) {
    size_t frame_bytes = frame.getByteCount();
    if (frame_offset >= frame_bytes) {
        return 0;
    }
    
    size_t bytes_available = frame_bytes - frame_offset;
    size_t bytes_to_copy = std::min(bytes_available, output_len);
    
    const uint8_t* frame_data = reinterpret_cast<const uint8_t*>(frame.samples.data());
    std::memcpy(output_buf, frame_data + frame_offset, bytes_to_copy);
    
    return bytes_to_copy;
}

void DemuxedStream::seekTo(unsigned long pos) {
    if (!m_demuxer) {
        return;
    }
    
    // Thread-safe clearing of chunk buffer and current frame
    {
        std::lock_guard<std::mutex> lock(m_buffer_mutex);
        while (!m_chunk_buffer.empty()) {
            m_chunk_buffer.pop();
        }
        m_current_buffer_bytes = 0; // Reset memory tracking
        m_current_frame = AudioFrame{};
        m_current_frame_offset = 0;
    }
    
    // Seek demuxer
    if (!m_demuxer->seekTo(pos)) {
        // Handle seek failure if necessary
        return;
    }
    
    // Reset codec state
    if (m_codec) {
        m_codec->reset();
    }
    
    // Update position tracking
    m_position = static_cast<int>(pos);
    m_sposition = (static_cast<uint64_t>(pos) * m_rate) / 1000;
    
    // CRITICAL: Sync sample counter with demuxer's granule position after seek
    m_samples_consumed = m_demuxer->getGranulePosition(m_current_stream_id);
    
    m_eof = false;
    m_eof_reached = false;
}

bool DemuxedStream::eof() {
    if (m_eof_reached) {
        Debug::log("demux", "DemuxedStream::eof() returning true - position=", m_position, "ms (frame-based)");
    }
    return m_eof_reached;
}

unsigned int DemuxedStream::getLength() {
    if (m_demuxer) {
        return static_cast<unsigned int>(m_demuxer->getDuration());
    }
    return 0;
}

std::vector<StreamInfo> DemuxedStream::getAvailableStreams() const {
    if (!m_demuxer) {
        return {};
    }
    return m_demuxer->getStreams();
}

bool DemuxedStream::switchToStream(uint32_t stream_id) {
    if (!m_demuxer) {
        return false;
    }
    
    // Verify stream exists and is audio
    StreamInfo stream_info = m_demuxer->getStreamInfo(stream_id);
    if (stream_info.stream_id == 0 || stream_info.codec_type != "audio") {
        return false;
    }
    
    // Clear current state (thread-safe)
    {
        std::lock_guard<std::mutex> lock(m_buffer_mutex);
        while (!m_chunk_buffer.empty()) {
            m_chunk_buffer.pop();
        }
        m_current_buffer_bytes = 0; // Reset memory tracking
    }
    m_current_frame = AudioFrame{};
    m_current_frame_offset = 0;
    
    // Update stream ID
    m_current_stream_id = stream_id;
    
    // Setup new codec
    if (!setupCodec()) {
        return false;
    }
    
    // Update properties and reset consumption tracking
    updateStreamProperties();
    m_samples_consumed = 0;
    
    return true;
}

bool DemuxedStream::initializeWithHandler(std::unique_ptr<IOHandler> handler) {
    try {
        // Reset handler to beginning for audio processing
        handler->seek(0, SEEK_SET);
        
        // Create demuxer with shared handler
        m_demuxer = DemuxerFactory::createDemuxer(std::move(handler), m_path.to8Bit(true));
        if (!m_demuxer) {
            return false;
        }
        
        // Parse container
        if (!m_demuxer->parseContainer()) {
            return false;
        }
        
        // Select audio stream
        if (m_current_stream_id == 0) {
            m_current_stream_id = selectBestAudioStream();
        }
        
        if (m_current_stream_id == 0) {
            return false; // No suitable audio stream found
        }
        
        // Setup codec
        if (!setupCodec()) {
            return false;
        }
        
        // Update stream properties
        updateStreamProperties();
        
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

StreamInfo DemuxedStream::getCurrentStreamInfo() const {
    if (!m_demuxer) {
        return StreamInfo{};
    }
    return m_demuxer->getStreamInfo(m_current_stream_id);
}

std::string DemuxedStream::getDemuxerType() const {
    // This would need to be implemented in the demuxer base class
    // For now, return a placeholder
    return "unknown";
}

std::string DemuxedStream::getCodecType() const {
    if (!m_codec) {
        return "unknown";
    }
    return m_codec->getCodecName();
}

TagLib::String DemuxedStream::getArtist() {
    if (!m_demuxer) {
        return TagLib::String();
    }
    
    // Try to get metadata from the demuxer (for Ogg files)
    StreamInfo stream_info = m_demuxer->getStreamInfo(m_current_stream_id);
    if (!stream_info.artist.empty()) {
        return TagLib::String(stream_info.artist, TagLib::String::UTF8);
    }
    
    // Fall back to base class implementation (TagLib)
    return Stream::getArtist();
}

TagLib::String DemuxedStream::getTitle() {
    if (!m_demuxer) {
        return TagLib::String();
    }
    
    // Try to get metadata from the demuxer (for Ogg files)
    StreamInfo stream_info = m_demuxer->getStreamInfo(m_current_stream_id);
    if (!stream_info.title.empty()) {
        return TagLib::String(stream_info.title, TagLib::String::UTF8);
    }
    
    // Fall back to base class implementation (TagLib)
    return Stream::getTitle();
}

TagLib::String DemuxedStream::getAlbum() {
    if (!m_demuxer) {
        return TagLib::String();
    }
    
    // Try to get metadata from the demuxer (for Ogg files)
    StreamInfo stream_info = m_demuxer->getStreamInfo(m_current_stream_id);
    if (!stream_info.album.empty()) {
        return TagLib::String(stream_info.album, TagLib::String::UTF8);
    }
    
    // Fall back to base class implementation (TagLib)
    return Stream::getAlbum();
}

} // namespace Demuxer
} // namespace PsyMP3
