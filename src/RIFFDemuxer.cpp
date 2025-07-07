/*
 * RIFFDemuxer.cpp - RIFF container demuxer implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "RIFFDemuxer.h"
#include "exceptions.h"
#include <algorithm>

RIFFDemuxer::RIFFDemuxer(std::unique_ptr<IOHandler> handler) 
    : Demuxer(std::move(handler)) {
}

bool RIFFDemuxer::parseContainer() {
    if (m_parsed) {
        return true;
    }
    
    try {
        m_handler->seek(0, SEEK_SET);
        
        // Read RIFF header
        RIFFChunk riff_chunk = readChunkHeader();
        if (riff_chunk.fourcc != RIFF_FOURCC) {
            return false;
        }
        
        // Read form type
        m_form_type = readFourCC();
        
        // For now, we only support WAVE files
        if (m_form_type != WAVE_FOURCC) {
            return false;
        }
        
        // Parse chunks within the RIFF
        while (!m_handler->eof() && m_handler->tell() < static_cast<long>(riff_chunk.data_offset + riff_chunk.size)) {
            RIFFChunk chunk = readChunkHeader();
            
            if (chunk.fourcc == FMT_FOURCC) {
                if (!parseWaveFormat(chunk)) {
                    return false;
                }
            } else if (chunk.fourcc == DATA_FOURCC) {
                if (!parseWaveData(chunk)) {
                    return false;
                }
            } else {
                // Skip unknown chunks
                skipChunk(chunk);
            }
        }
        
        // Verify we found required chunks
        if (m_audio_streams.empty()) {
            return false;
        }
        
        m_parsed = true;
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<StreamInfo> RIFFDemuxer::getStreams() const {
    std::vector<StreamInfo> streams;
    
    for (const auto& [stream_id, audio_data] : m_audio_streams) {
        StreamInfo info;
        info.stream_id = stream_id;
        info.codec_type = "audio";
        info.codec_name = formatTagToCodecName(audio_data.format_tag);
        info.codec_tag = audio_data.format_tag;
        info.sample_rate = audio_data.sample_rate;
        info.channels = audio_data.channels;
        info.bits_per_sample = audio_data.bits_per_sample;
        info.bitrate = audio_data.avg_bytes_per_sec * 8;
        info.codec_data = audio_data.extra_data;
        
        // Calculate duration
        if (audio_data.bytes_per_frame > 0) {
            info.duration_samples = audio_data.data_size / audio_data.bytes_per_frame;
            info.duration_ms = (info.duration_samples * 1000ULL) / audio_data.sample_rate;
        }
        
        streams.push_back(info);
    }
    
    return streams;
}

StreamInfo RIFFDemuxer::getStreamInfo(uint32_t stream_id) const {
    auto streams = getStreams();
    auto it = std::find_if(streams.begin(), streams.end(),
                          [stream_id](const StreamInfo& info) {
                              return info.stream_id == stream_id;
                          });
    
    if (it != streams.end()) {
        return *it;
    }
    
    return StreamInfo{}; // Empty info if not found
}

MediaChunk RIFFDemuxer::readChunk() {
    // For WAVE files, we typically only have one audio stream
    if (!m_audio_streams.empty()) {
        return readChunk(m_audio_streams.begin()->first);
    }
    
    return MediaChunk{}; // Empty chunk
}

MediaChunk RIFFDemuxer::readChunk(uint32_t stream_id) {
    auto it = m_audio_streams.find(stream_id);
    if (it == m_audio_streams.end()) {
        return MediaChunk{};
    }
    
    AudioStreamData& stream_data = it->second;
    
    // Check if we've reached the end of this stream
    if (stream_data.current_offset >= stream_data.data_size) {
        m_eof = true;
        return MediaChunk{};
    }
    
    // Read a reasonable chunk size (4KB by default)
    constexpr size_t CHUNK_SIZE = 4096;
    size_t bytes_to_read = std::min(CHUNK_SIZE, 
                                   static_cast<size_t>(stream_data.data_size - stream_data.current_offset));
    
    MediaChunk chunk;
    chunk.stream_id = stream_id;
    chunk.data.resize(bytes_to_read);
    chunk.file_offset = stream_data.data_offset + stream_data.current_offset;
    
    // Seek to the current position and read
    m_handler->seek(chunk.file_offset, SEEK_SET);
    size_t bytes_read = m_handler->read(chunk.data.data(), 1, bytes_to_read);
    
    if (bytes_read != bytes_to_read) {
        chunk.data.resize(bytes_read);
    }
    
    // Calculate timestamps
    if (stream_data.bytes_per_frame > 0) {
        chunk.timestamp_samples = stream_data.current_offset / stream_data.bytes_per_frame;
        chunk.timestamp_ms = (chunk.timestamp_samples * 1000ULL) / stream_data.sample_rate;
    }
    
    // Update current position
    stream_data.current_offset += bytes_read;
    m_position_ms = chunk.timestamp_ms;
    
    return chunk;
}

bool RIFFDemuxer::seekTo(uint64_t timestamp_ms) {
    if (m_audio_streams.empty()) {
        return false;
    }
    
    // For simplicity, seek the first audio stream
    auto& stream_data = m_audio_streams.begin()->second;
    
    uint64_t byte_offset = msToByteOffset(timestamp_ms, stream_data.stream_id);
    
    // Clamp to valid range
    byte_offset = std::min(byte_offset, stream_data.data_size);
    
    stream_data.current_offset = byte_offset;
    m_position_ms = timestamp_ms;
    m_eof = (byte_offset >= stream_data.data_size);
    
    return true;
}

bool RIFFDemuxer::isEOF() const {
    return m_eof;
}

uint64_t RIFFDemuxer::getDuration() const {
    return m_duration_ms;
}

uint64_t RIFFDemuxer::getPosition() const {
    return m_position_ms;
}

RIFFChunk RIFFDemuxer::readChunkHeader() {
    RIFFChunk chunk;
    chunk.fourcc = readLE<uint32_t>();
    chunk.size = readLE<uint32_t>();
    chunk.data_offset = m_handler->tell();
    return chunk;
}

bool RIFFDemuxer::parseWaveFormat(const RIFFChunk& chunk) {
    AudioStreamData stream_data;
    stream_data.stream_id = 0; // WAVE files typically have one audio stream
    
    // Read format header
    stream_data.format_tag = readLE<uint16_t>();
    stream_data.channels = readLE<uint16_t>();
    stream_data.sample_rate = readLE<uint32_t>();
    stream_data.avg_bytes_per_sec = readLE<uint32_t>();
    stream_data.block_align = readLE<uint16_t>();
    stream_data.bits_per_sample = readLE<uint16_t>();
    
    stream_data.bytes_per_frame = stream_data.channels * (stream_data.bits_per_sample / 8);
    
    // Read extra data if present
    if (chunk.size > 16) {
        uint16_t extra_size = readLE<uint16_t>();
        if (extra_size > 0 && chunk.size >= 18 + extra_size) {
            stream_data.extra_data.resize(extra_size);
            m_handler->read(stream_data.extra_data.data(), 1, extra_size);
        }
    }
    
    m_audio_streams[stream_data.stream_id] = stream_data;
    
    // Skip to end of chunk
    skipChunk(chunk);
    
    return true;
}

bool RIFFDemuxer::parseWaveData(const RIFFChunk& chunk) {
    // Should have format chunk first
    if (m_audio_streams.empty()) {
        return false;
    }
    
    auto& stream_data = m_audio_streams.begin()->second;
    stream_data.data_offset = chunk.data_offset;
    stream_data.data_size = chunk.size;
    stream_data.current_offset = 0;
    
    // Calculate duration
    if (stream_data.bytes_per_frame > 0) {
        uint64_t total_samples = stream_data.data_size / stream_data.bytes_per_frame;
        m_duration_ms = (total_samples * 1000ULL) / stream_data.sample_rate;
    }
    
    // Don't skip this chunk - we'll read from it during playback
    return true;
}

void RIFFDemuxer::skipChunk(const RIFFChunk& chunk) {
    long end_pos = chunk.data_offset + chunk.size;
    // Add padding byte if chunk size is odd
    if (chunk.size % 2 != 0) {
        end_pos++;
    }
    m_handler->seek(end_pos, SEEK_SET);
}

std::string RIFFDemuxer::formatTagToCodecName(uint16_t format_tag) const {
    switch (format_tag) {
        case WAVE_FORMAT_PCM:
            return "pcm";
        case WAVE_FORMAT_IEEE_FLOAT:
            return "pcm"; // PCM codec handles float
        case WAVE_FORMAT_ALAW:
            return "alaw";
        case WAVE_FORMAT_MULAW:
            return "mulaw";
        case WAVE_FORMAT_MPEGLAYER3:
            return "mp3";
        default:
            return "unknown";
    }
}

uint64_t RIFFDemuxer::byteOffsetToMs(uint64_t byte_offset, uint32_t stream_id) const {
    auto it = m_audio_streams.find(stream_id);
    if (it == m_audio_streams.end() || it->second.bytes_per_frame == 0) {
        return 0;
    }
    
    const auto& stream_data = it->second;
    uint64_t samples = byte_offset / stream_data.bytes_per_frame;
    return (samples * 1000ULL) / stream_data.sample_rate;
}

uint64_t RIFFDemuxer::msToByteOffset(uint64_t timestamp_ms, uint32_t stream_id) const {
    auto it = m_audio_streams.find(stream_id);
    if (it == m_audio_streams.end()) {
        return 0;
    }
    
    const auto& stream_data = it->second;
    uint64_t samples = (timestamp_ms * stream_data.sample_rate) / 1000ULL;
    return samples * stream_data.bytes_per_frame;
}