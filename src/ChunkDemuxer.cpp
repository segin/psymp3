/*
 * ChunkDemuxer.cpp - Universal chunk-based demuxer implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

ChunkDemuxer::ChunkDemuxer(std::unique_ptr<IOHandler> handler) 
    : Demuxer(std::move(handler)) {
}

bool ChunkDemuxer::parseContainer() {
    if (m_parsed) {
        return true;
    }
    
    try {
        m_handler->seek(0, SEEK_SET);
        
        // Read container header
        Chunk container_chunk = readChunkHeader();
        m_container_fourcc = container_chunk.fourcc;
        
        // Detect format and endianness
        if (container_chunk.fourcc == FORM_FOURCC) {
            m_big_endian = true; // IFF/AIFF uses big-endian
        } else if (container_chunk.fourcc == RIFF_FOURCC) {
            m_big_endian = false; // RIFF uses little-endian
        } else {
            return false; // Unknown container format
        }
        
        // Read form type using detected endianness
        m_form_type = readChunkValue<uint32_t>();
        
        // Support WAVE (RIFF) and AIFF (FORM) files
        if (m_form_type != WAVE_FOURCC && m_form_type != AIFF_FOURCC) {
            return false;
        }
        
        // Parse chunks within the container
        while (!m_handler->eof() && m_handler->tell() < static_cast<long>(container_chunk.data_offset + container_chunk.size)) {
            Chunk chunk = readChunkHeader();
            
            if (m_form_type == WAVE_FOURCC) {
                // RIFF/WAV chunks
                if (chunk.fourcc == FMT_FOURCC) {
                    if (!parseWaveFormat(chunk)) {
                        return false;
                    }
                } else if (chunk.fourcc == DATA_FOURCC) {
                    if (!parseWaveData(chunk)) {
                        return false;
                    }
                } else {
                    skipChunk(chunk);
                }
            } else if (m_form_type == AIFF_FOURCC) {
                // FORM/AIFF chunks
                if (chunk.fourcc == COMM_FOURCC) {
                    if (!parseAiffCommon(chunk)) {
                        return false;
                    }
                } else if (chunk.fourcc == SSND_FOURCC) {
                    if (!parseAiffSoundData(chunk)) {
                        return false;
                    }
                } else {
                    skipChunk(chunk);
                }
            } else {
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

std::vector<StreamInfo> ChunkDemuxer::getStreams() const {
    std::vector<StreamInfo> streams;
    
    for (const auto& [stream_id, audio_data] : m_audio_streams) {
        StreamInfo info;
        info.stream_id = stream_id;
        info.codec_type = "audio";
        info.codec_name = getCodecName(audio_data);
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

StreamInfo ChunkDemuxer::getStreamInfo(uint32_t stream_id) const {
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

MediaChunk ChunkDemuxer::readChunk() {
    // For WAVE files, we typically only have one audio stream
    if (!m_audio_streams.empty()) {
        return readChunk(m_audio_streams.begin()->first);
    }
    
    return MediaChunk{}; // Empty chunk
}

MediaChunk ChunkDemuxer::readChunk(uint32_t stream_id) {
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

bool ChunkDemuxer::seekTo(uint64_t timestamp_ms) {
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

bool ChunkDemuxer::isEOF() const {
    return m_eof;
}

uint64_t ChunkDemuxer::getDuration() const {
    return m_duration_ms;
}

uint64_t ChunkDemuxer::getPosition() const {
    return m_position_ms;
}

Chunk ChunkDemuxer::readChunkHeader() {
    Chunk chunk;
    chunk.fourcc = readBE<uint32_t>(); // FourCC is always big-endian by convention
    chunk.size = readChunkValue<uint32_t>(); // Size uses format endianness
    chunk.data_offset = m_handler->tell();
    return chunk;
}

bool ChunkDemuxer::parseWaveFormat(const Chunk& chunk) {
    AudioStreamData stream_data;
    stream_data.stream_id = 0; // WAVE files typically have one audio stream
    
    // Read format header (WAVE always uses little-endian)
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
        if (extra_size > 0 && chunk.size >= 18 + static_cast<uint32_t>(extra_size)) {
            stream_data.extra_data.resize(extra_size);
            m_handler->read(stream_data.extra_data.data(), 1, extra_size);
        }
    }
    
    m_audio_streams[stream_data.stream_id] = stream_data;
    
    // Skip to end of chunk
    skipChunk(chunk);
    
    return true;
}

bool ChunkDemuxer::parseWaveData(const Chunk& chunk) {
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

void ChunkDemuxer::skipChunk(const Chunk& chunk) {
    long end_pos = chunk.data_offset + chunk.size;
    // Add padding byte if chunk size is odd
    if (chunk.size % 2 != 0) {
        end_pos++;
    }
    m_handler->seek(end_pos, SEEK_SET);
}

std::string ChunkDemuxer::getCodecName(const AudioStreamData& stream) const {
    if (m_form_type == AIFF_FOURCC) {
        return aiffCompressionToCodecName(stream.compression_type);
    } else {
        return formatTagToCodecName(stream.format_tag);
    }
}

std::string ChunkDemuxer::formatTagToCodecName(uint16_t format_tag) const {
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

std::string ChunkDemuxer::aiffCompressionToCodecName(uint32_t compression) const {
    switch (compression) {
        case AIFF_NONE:
            return "pcm";
        case AIFF_SOWT:
            return "pcm"; // Byte-swapped PCM
        case AIFF_FL32:
        case AIFF_FL64:
            return "pcm"; // Float PCM
        case AIFF_ALAW:
            return "alaw";
        case AIFF_ULAW:
            return "mulaw";
        default:
            return "unknown";
    }
}

bool ChunkDemuxer::parseAiffCommon(const Chunk& chunk) {
    AudioStreamData stream_data;
    stream_data.stream_id = 0; // AIFF files typically have one audio stream
    
    // Read COMM chunk (all AIFF data is big-endian)
    stream_data.channels = readBE<uint16_t>();
    uint32_t num_sample_frames = readBE<uint32_t>();
    (void)num_sample_frames;
    stream_data.bits_per_sample = readBE<uint16_t>();
    
    // Read IEEE 80-bit extended precision sample rate
    uint8_t ieee80[10];
    m_handler->read(ieee80, 1, 10);
    stream_data.sample_rate = static_cast<uint32_t>(ieee80ToDouble(ieee80));
    
    stream_data.bytes_per_frame = stream_data.channels * (stream_data.bits_per_sample / 8);
    stream_data.avg_bytes_per_sec = stream_data.sample_rate * stream_data.bytes_per_frame;
    stream_data.block_align = stream_data.bytes_per_frame;
    
    // Check for compression type (AIFF-C)
    if (chunk.size > 18) {
        stream_data.compression_type = readBE<uint32_t>();
    } else {
        stream_data.compression_type = AIFF_NONE; // Uncompressed AIFF
    }
    
    // Convert compression to format tag for compatibility
    switch (stream_data.compression_type) {
        case AIFF_NONE:
        case AIFF_SOWT:
        case AIFF_FL32:
        case AIFF_FL64:
            stream_data.format_tag = WAVE_FORMAT_PCM;
            break;
        case AIFF_ALAW:
            stream_data.format_tag = WAVE_FORMAT_ALAW;
            break;
        case AIFF_ULAW:
            stream_data.format_tag = WAVE_FORMAT_MULAW;
            break;
        default:
            stream_data.format_tag = 0; // Unknown
            break;
    }
    
    m_audio_streams[stream_data.stream_id] = stream_data;
    skipChunk(chunk);
    return true;
}

bool ChunkDemuxer::parseAiffSoundData(const Chunk& chunk) {
    if (m_audio_streams.empty()) {
        return false;
    }
    
    auto& stream_data = m_audio_streams.begin()->second;
    
    // Read SSND chunk header (offset and blockSize)
    stream_data.ssnd_offset = readBE<uint32_t>();
    stream_data.ssnd_block_size = readBE<uint32_t>();
    
    // Audio data starts after the offset/blockSize fields
    stream_data.data_offset = chunk.data_offset + 8 + stream_data.ssnd_offset;
    stream_data.data_size = chunk.size - 8 - stream_data.ssnd_offset;
    stream_data.current_offset = 0;
    
    // Calculate duration
    if (stream_data.bytes_per_frame > 0) {
        uint64_t total_samples = stream_data.data_size / stream_data.bytes_per_frame;
        m_duration_ms = (total_samples * 1000ULL) / stream_data.sample_rate;
    }
    
    return true;
}

double ChunkDemuxer::ieee80ToDouble(const uint8_t ieee80[10]) const {
    // Convert IEEE 80-bit extended precision to double
    // This is a simplified conversion for AIFF sample rates
    
    uint16_t exponent = (static_cast<uint16_t>(ieee80[0]) << 8) | ieee80[1];
    uint64_t mantissa = 0;
    
    for (int i = 2; i < 10; ++i) {
        mantissa = (mantissa << 8) | ieee80[i];
    }
    
    if (exponent == 0 && mantissa == 0) {
        return 0.0;
    }
    
    // Extract sign, exponent, and mantissa
    bool sign = (exponent & 0x8000) != 0;
    exponent &= 0x7FFF;
    
    if (exponent == 0x7FFF) {
        return sign ? -INFINITY : INFINITY;
    }
    
    // Convert to double
    double result = static_cast<double>(mantissa) / (1ULL << 63);
    result *= std::pow(2.0, static_cast<int>(exponent) - 16383 - 63);
    
    return sign ? -result : result;
}

uint64_t ChunkDemuxer::byteOffsetToMs(uint64_t byte_offset, uint32_t stream_id) const {
    auto it = m_audio_streams.find(stream_id);
    if (it == m_audio_streams.end() || it->second.bytes_per_frame == 0) {
        return 0;
    }
    
    const auto& stream_data = it->second;
    uint64_t samples = byte_offset / stream_data.bytes_per_frame;
    return (samples * 1000ULL) / stream_data.sample_rate;
}

uint64_t ChunkDemuxer::msToByteOffset(uint64_t timestamp_ms, uint32_t stream_id) const {
    auto it = m_audio_streams.find(stream_id);
    if (it == m_audio_streams.end()) {
        return 0;
    }
    
    const auto& stream_data = it->second;
    uint64_t samples = (timestamp_ms * stream_data.sample_rate) / 1000ULL;
    return samples * stream_data.bytes_per_frame;
}