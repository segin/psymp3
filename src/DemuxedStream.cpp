/*
 * DemuxedStream.cpp - Stream implementation using demuxer/codec architecture
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "DemuxedStream.h"
#include "Demuxer.h"
#include "AudioCodec.h"
#include "FileIOHandler.h"
#include "URI.h"
#include "exceptions.h"
#include <algorithm>

DemuxedStream::DemuxedStream(const TagLib::String& path, uint32_t preferred_stream_id) 
    : Stream(path), m_current_stream_id(preferred_stream_id) {
    
    if (!initialize()) {
        throw InvalidMediaException("Failed to initialize demuxed stream for: " + path);
    }
}

bool DemuxedStream::initialize() {
    try {
        // Create IOHandler
        URI uri(m_path);
        std::unique_ptr<IOHandler> handler;
        
        if (uri.scheme() == "file" || uri.scheme().empty()) {
            handler = std::make_unique<FileIOHandler>(uri.path());
        } else {
            // Could add support for other schemes later
            return false;
        }
        
        // Create demuxer with file path hint for raw format detection
        m_demuxer = DemuxerFactory::createDemuxer(std::move(handler), uri.path());
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
    m_length = static_cast<int>(stream_info.duration_ms);
    m_slength = stream_info.duration_samples;
    m_position = 0;
    m_sposition = 0;
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
            continue;
        }
        
        // Need a new frame
        m_current_frame = getNextFrame();
        m_current_frame_offset = 0;
        
        if (m_current_frame.samples.empty()) {
            // No more data available
            m_eof_reached = true;
            m_eof = true;
            break;
        }
    }
    
    // Update position based on bytes consumed
    if (bytes_written > 0 && m_channels > 0) {
        size_t samples_consumed = bytes_written / (sizeof(int16_t) * m_channels);
        m_sposition += samples_consumed;
        m_position = static_cast<int>((m_sposition * 1000) / m_rate);
    }
    
    return bytes_written;
}

AudioFrame DemuxedStream::getNextFrame() {
    // Check if we have buffered frames
    if (!m_frame_buffer.empty()) {
        AudioFrame frame = m_frame_buffer.front();
        m_frame_buffer.pop();
        return frame;
    }
    
    // Fill the buffer
    fillFrameBuffer();
    
    // Try again
    if (!m_frame_buffer.empty()) {
        AudioFrame frame = m_frame_buffer.front();
        m_frame_buffer.pop();
        return frame;
    }
    
    return AudioFrame{}; // Empty frame
}

void DemuxedStream::fillFrameBuffer() {
    constexpr size_t MAX_BUFFER_FRAMES = 10; // Limit buffering
    
    while (m_frame_buffer.size() < MAX_BUFFER_FRAMES && !m_demuxer->isEOF()) {
        MediaChunk chunk = m_demuxer->readChunk(m_current_stream_id);
        
        if (chunk.data.empty()) {
            break; // No more data
        }
        
        AudioFrame frame = m_codec->decode(chunk);
        if (!frame.samples.empty()) {
            m_frame_buffer.push(frame);
        }
    }
    
    // Try to flush codec if demuxer is at EOF
    if (m_demuxer->isEOF()) {
        AudioFrame frame = m_codec->flush();
        if (!frame.samples.empty()) {
            m_frame_buffer.push(frame);
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
    
    // Clear buffers
    while (!m_frame_buffer.empty()) {
        m_frame_buffer.pop();
    }
    m_current_frame = AudioFrame{};
    m_current_frame_offset = 0;
    
    // Seek demuxer
    m_demuxer->seekTo(pos);
    
    // Reset codec state
    if (m_codec) {
        m_codec->reset();
    }
    
    // Update position
    m_position = static_cast<int>(pos);
    m_sposition = (static_cast<long long>(pos) * m_rate) / 1000;
    m_eof = false;
    m_eof_reached = false;
}

bool DemuxedStream::eof() {
    return m_eof_reached;
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
    while (!m_frame_buffer.empty()) {
        m_frame_buffer.pop();
    }
    m_current_frame = AudioFrame{};
    m_current_frame_offset = 0;
    
    // Update stream ID
    m_current_stream_id = stream_id;
    
    // Setup new codec
    if (!setupCodec()) {
        return false;
    }
    
    // Update properties
    updateStreamProperties();
    
    return true;
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