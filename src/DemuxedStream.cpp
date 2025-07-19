/*
 * DemuxedStream.cpp - Stream implementation using demuxer/codec architecture
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

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
        // Create IOHandler
        URI uri(m_path);
        std::unique_ptr<IOHandler> handler;
        
        if (uri.scheme() == "file" || uri.scheme().isEmpty()) {
            handler = std::make_unique<FileIOHandler>(uri.path());
        } else {
            // Could add support for other schemes later
            return false;
        }
        
        // Create demuxer with file path hint for raw format detection
        m_demuxer = DemuxerFactory::createDemuxer(std::move(handler), uri.path().to8Bit(true));
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
    StreamInfo stream_info = m_demuxer->getStreamInfo(m_current_stream_id);
    if (stream_info.stream_id == 0) {
        return false;
    }
    
    m_codec = AudioCodecFactory::createCodec(stream_info);
    if (!m_codec) {
        return false;
    }
    
    return m_codec->initialize();
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
            
            // Update samples consumed based on actual audio consumption
            size_t samples_copied = bytes_copied / (sizeof(int16_t) * m_channels);
            m_samples_consumed += samples_copied;
            continue;
        }
        
        // Need a new frame - decode on-demand from buffered chunks
        m_current_frame = std::move(getNextFrame());
        m_current_frame_offset = 0;
        
        if (m_current_frame.samples.empty()) {
            // Empty frame could be from header processing or actual EOF
            // Check if we have more chunks to process before declaring EOF
            Debug::log("demux", "DemuxedStream::getData: Empty frame - chunk_buffer.size()=", m_chunk_buffer.size(), 
                               ", demuxer.isEOF()=", m_demuxer ? m_demuxer->isEOF() : true);
            
            if (m_chunk_buffer.empty() && m_demuxer && m_demuxer->isEOF()) {
                // Truly at EOF - no more chunks and demuxer is done
                Debug::log("demux", "DemuxedStream::getData: Natural EOF reached after consuming ", 
                                   m_samples_consumed, " samples (", 
                                   (m_samples_consumed * 1000) / m_rate, "ms)");
                Debug::log("demux", "DemuxedStream::getData: Setting EOF - consumed_samples=", m_samples_consumed, 
                                   ", time=", (m_samples_consumed * 1000) / m_rate, "ms");
                m_eof_reached = true;
                m_eof = true;
                break;
            } else if (m_chunk_buffer.empty() && m_demuxer && !m_demuxer->isEOF()) {
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
    
    // Update position based on actual audio consumption, not packet timestamps
    if (m_channels > 0 && m_rate > 0) {
        m_sposition = m_samples_consumed;
        m_position = static_cast<int>((m_samples_consumed * 1000) / m_rate);
    }
    
    return bytes_written;
}

AudioFrame DemuxedStream::getNextFrame() {
    // Ensure we have buffered chunks to decode from
    fillChunkBuffer();
    
    Debug::log("demux", "DemuxedStream::getNextFrame: After fillChunkBuffer, buffer size=", m_chunk_buffer.size());
    
    Debug::log("demux", "DemuxedStream::getNextFrame: chunk_buffer size=", m_chunk_buffer.size(), 
                   ", demuxer EOF=", m_demuxer ? m_demuxer->isEOF() : true);
    
    // If we have chunks, decode one on-demand
    if (!m_chunk_buffer.empty()) {
        MediaChunk chunk = std::move(m_chunk_buffer.front());
        m_chunk_buffer.pop();
        
        // Special handling for Opus: if codec is initialized, and we get a header chunk,
        // it means it's a redundant header from seeking or re-initialization. Skip it.
        if (m_codec && m_codec->getCodecName() == "opus" && m_codec->isInitialized() && chunk.is_keyframe) {
            Debug::log("demux", "DemuxedStream: Skipping redundant Opus header chunk (size=", chunk.data.size(), ")");
            return AudioFrame{}; // Return empty frame, effectively discarding this chunk
        }
        
        AudioFrame frame = m_codec->decode(chunk);
        if (!frame.samples.empty()) {
            // Correct timestamp calculation for Ogg
            if (chunk.granule_position != 0 && chunk.granule_position != static_cast<uint64_t>(-1)) {
                frame.timestamp_samples = chunk.granule_position - frame.getSampleFrameCount();
            } else {
                // Fallback for non-Ogg or header packets
                frame.timestamp_samples = m_samples_consumed;
            }
            frame.timestamp_ms = (frame.timestamp_samples * 1000) / m_rate;
            
            Debug::log("demux", "DemuxedStream: On-demand decoded frame with ", frame.samples.size(), " samples. Timestamp: ", frame.timestamp_ms, "ms");
            return frame;
        } else {
            Debug::log("demux", "DemuxedStream: Codec returned empty frame for chunk size=", chunk.data.size());
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
    // Keep it simple: Buffer only a few chunks at a time
    constexpr size_t MAX_BUFFER_CHUNKS = 5; // Keep buffer very small
    
    while (m_chunk_buffer.size() < MAX_BUFFER_CHUNKS && !m_demuxer->isEOF()) {
        MediaChunk chunk = m_demuxer->readChunk(m_current_stream_id);
        
        if (chunk.data.empty()) {
            Debug::log("demux", "DemuxedStream: Got empty chunk, stopping chunk buffering");
            break; // No more data
        }
        
        Debug::log("demux", "DemuxedStream: Buffering chunk size=", chunk.data.size(), " bytes (buffer size=", m_chunk_buffer.size() + 1, ")");
        
        // Buffer the compressed chunk - no decoding yet!
        m_chunk_buffer.push(std::move(chunk));
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
    
    // Clear chunk buffer
    while (!m_chunk_buffer.empty()) {
        m_chunk_buffer.pop();
    }
    m_current_frame = AudioFrame{};
    m_current_frame_offset = 0;
    
    // Seek demuxer
    m_demuxer->seekTo(pos);
    
    // Reset codec state
    if (m_codec) {
        m_codec->reset();
    }
    
    // Update position tracking
    m_position = static_cast<int>(pos);
    m_sposition = (static_cast<long long>(pos) * m_rate) / 1000;
    m_samples_consumed = m_sposition;  // Reset consumption tracking
    m_eof = false;
    m_eof_reached = false;
}

bool DemuxedStream::eof() {
    if (m_eof_reached) {
        Debug::log("demux", "DemuxedStream::eof() returning true - consumed_samples=", m_samples_consumed, 
                       ", time=", (m_samples_consumed * 1000) / m_rate, "ms");
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
    
    // Clear current state
    while (!m_chunk_buffer.empty()) {
        m_chunk_buffer.pop();
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