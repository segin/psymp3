/*
 * RawAudioDemuxer.cpp - Raw audio format demuxer implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

// Extension to configuration mapping
const std::map<std::string, RawAudioConfig> RawAudioDetector::s_extension_map = {
    // Telephony formats (8kHz mono by default)
    {".ulaw", RawAudioConfig("mulaw", 8000, 1, 8)},
    {".ul",   RawAudioConfig("mulaw", 8000, 1, 8)},
    {".mulaw", RawAudioConfig("mulaw", 8000, 1, 8)},
    {".alaw", RawAudioConfig("alaw", 8000, 1, 8)},
    {".al",   RawAudioConfig("alaw", 8000, 1, 8)},
    
    // PCM formats - common telephony and broadcast rates
    {".pcm",   RawAudioConfig("pcm", 8000, 1, 16, true)},  // Default 8kHz 16-bit
    {".s8",    RawAudioConfig("pcm", 8000, 1, 8, true)},   // 8-bit signed
    {".u8",    RawAudioConfig("pcm", 8000, 1, 8, true)},   // 8-bit unsigned
    {".s16le", RawAudioConfig("pcm", 44100, 2, 16, true)}, // 16-bit signed LE
    {".s16be", RawAudioConfig("pcm", 44100, 2, 16, false)},// 16-bit signed BE
    {".s24le", RawAudioConfig("pcm", 44100, 2, 24, true)}, // 24-bit signed LE
    {".s24be", RawAudioConfig("pcm", 44100, 2, 24, false)},// 24-bit signed BE
    {".s32le", RawAudioConfig("pcm", 44100, 2, 32, true)}, // 32-bit signed LE
    {".s32be", RawAudioConfig("pcm", 44100, 2, 32, false)},// 32-bit signed BE
    {".f32le", RawAudioConfig("pcm", 44100, 2, 32, true)}, // 32-bit float LE
    {".f32be", RawAudioConfig("pcm", 44100, 2, 32, false)},// 32-bit float BE
    {".f64le", RawAudioConfig("pcm", 44100, 2, 64, true)}, // 64-bit float LE
    {".f64be", RawAudioConfig("pcm", 44100, 2, 64, false)},// 64-bit float BE
};

RawAudioDemuxer::RawAudioDemuxer(std::unique_ptr<IOHandler> handler, const std::string& file_path)
    : Demuxer(std::move(handler)), m_file_path(file_path) {
    
    // Auto-detect format from extension
    m_config = RawAudioDetector::detectFromExtension(file_path);
}

RawAudioDemuxer::RawAudioDemuxer(std::unique_ptr<IOHandler> handler, const RawAudioConfig& config)
    : Demuxer(std::move(handler)), m_config(config) {
}

bool RawAudioDemuxer::parseContainer() {
    if (m_parsed) {
        return true;
    }
    
    try {
        // Get file size
        m_handler->seek(0, SEEK_END);
        m_file_size = m_handler->tell();
        m_handler->seek(0, SEEK_SET);
        
        if (m_file_size == 0) {
            return false;
        }
        
        // Calculate bytes per frame
        m_bytes_per_frame = m_config.channels * (m_config.bits_per_sample / 8);
        if (m_bytes_per_frame == 0) {
            return false;
        }
        
        // Calculate duration
        calculateDuration();
        
        // Create stream info
        StreamInfo stream_info;
        stream_info.stream_id = 1; // Single stream
        stream_info.codec_type = "audio";
        stream_info.codec_name = m_config.codec_name;
        stream_info.sample_rate = m_config.sample_rate;
        stream_info.channels = m_config.channels;
        stream_info.bits_per_sample = m_config.bits_per_sample;
        stream_info.duration_samples = m_file_size / m_bytes_per_frame;
        stream_info.duration_ms = m_duration_ms;
        
        // Calculate average bitrate
        if (m_duration_ms > 0) {
            stream_info.bitrate = static_cast<uint32_t>((m_file_size * 8 * 1000ULL) / m_duration_ms);
        }
        
        m_streams.push_back(stream_info);
        m_parsed = true;
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<StreamInfo> RawAudioDemuxer::getStreams() const {
    return m_streams;
}

StreamInfo RawAudioDemuxer::getStreamInfo(uint32_t stream_id) const {
    if (stream_id == 1 && !m_streams.empty()) {
        return m_streams[0];
    }
    return StreamInfo{};
}

MediaChunk RawAudioDemuxer::readChunk() {
    return readChunk(1); // Single stream
}

MediaChunk RawAudioDemuxer::readChunk(uint32_t stream_id) {
    if (stream_id != 1 || m_eof || m_current_offset >= m_file_size) {
        m_eof = true;
        return MediaChunk{};
    }
    
    // Read a reasonable chunk size (4KB by default)
    constexpr size_t CHUNK_SIZE = 4096;
    size_t bytes_to_read = std::min(CHUNK_SIZE, 
                                   static_cast<size_t>(m_file_size - m_current_offset));
    
    // Align to frame boundaries for multi-byte formats
    if (m_bytes_per_frame > 1) {
        bytes_to_read = (bytes_to_read / m_bytes_per_frame) * m_bytes_per_frame;
    }
    
    if (bytes_to_read == 0) {
        m_eof = true;
        return MediaChunk{};
    }
    
    MediaChunk chunk;
    chunk.stream_id = stream_id;
    chunk.data.resize(bytes_to_read);
    chunk.file_offset = m_current_offset;
    
    // Seek and read
    m_handler->seek(m_current_offset, SEEK_SET);
    size_t bytes_read = m_handler->read(chunk.data.data(), 1, bytes_to_read);
    
    if (bytes_read != bytes_to_read) {
        chunk.data.resize(bytes_read);
    }
    
    // Calculate timestamps
    size_t bytes_per_sample = m_streams[0].bits_per_sample / 8;
    if (bytes_per_sample == 0) bytes_per_sample = 1; // Avoid division by zero
    chunk.timestamp_samples = m_current_offset / bytes_per_sample;
    
    // Update position
    m_current_offset += bytes_read;
    m_position_ms = byteOffsetToMs(m_current_offset);
    
    return chunk;
}

bool RawAudioDemuxer::seekTo(uint64_t timestamp_ms) {
    uint64_t byte_offset = msToByteOffset(timestamp_ms);
    
    // Align to frame boundary
    if (m_bytes_per_frame > 1) {
        byte_offset = (byte_offset / m_bytes_per_frame) * m_bytes_per_frame;
    }
    
    // Clamp to valid range
    byte_offset = std::min(byte_offset, m_file_size);
    
    m_current_offset = byte_offset;
    m_position_ms = byteOffsetToMs(byte_offset);
    m_eof = (byte_offset >= m_file_size);
    
    return true;
}

bool RawAudioDemuxer::isEOF() const {
    return m_eof;
}

uint64_t RawAudioDemuxer::getDuration() const {
    return m_duration_ms;
}

uint64_t RawAudioDemuxer::getPosition() const {
    return m_position_ms;
}

bool RawAudioDetector::isRawAudioExtension(const std::string& file_path) {
    size_t dot_pos = file_path.find_last_of('.');
    if (dot_pos == std::string::npos) {
        return false;
    }
    
    std::string ext = file_path.substr(dot_pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    return s_extension_map.find(ext) != s_extension_map.end();
}

RawAudioConfig RawAudioDetector::detectFromExtension(const std::string& file_path) {
    size_t dot_pos = file_path.find_last_of('.');
    if (dot_pos == std::string::npos) {
        return RawAudioConfig(); // Default mu-law config
    }
    
    std::string ext = file_path.substr(dot_pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    auto it = s_extension_map.find(ext);
    if (it != s_extension_map.end()) {
        return it->second;
    }
    
    return RawAudioConfig(); // Default mu-law config
}

void RawAudioDemuxer::calculateDuration() {
    if (m_bytes_per_frame == 0 || m_config.sample_rate == 0) {
        m_duration_ms = 0;
        return;
    }
    
    uint64_t total_samples = m_file_size / m_bytes_per_frame;
    m_duration_ms = (total_samples * 1000ULL) / m_config.sample_rate;
}

uint64_t RawAudioDemuxer::byteOffsetToMs(uint64_t byte_offset) const {
    if (m_bytes_per_frame == 0 || m_config.sample_rate == 0) {
        return 0;
    }
    
    uint64_t samples = byte_offset / m_bytes_per_frame;
    return (samples * 1000ULL) / m_config.sample_rate;
}

uint64_t RawAudioDemuxer::msToByteOffset(uint64_t timestamp_ms) const {
    if (m_config.sample_rate == 0) {
        return 0;
    }
    
    uint64_t samples = (timestamp_ms * m_config.sample_rate) / 1000ULL;
    return samples * m_bytes_per_frame;
}

std::optional<RawAudioConfig> RawAudioDetector::detectRawAudio(const std::string& file_path, 
                                                               IOHandler* handler) {
    // First try extension-based detection
    if (RawAudioDetector::isRawAudioExtension(file_path)) {
        return RawAudioDetector::detectFromExtension(file_path);
    }
    
    // Could add content-based detection here if needed
    // For now, extension-based detection is sufficient
    
    return std::nullopt;
}