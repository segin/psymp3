/*
 * ChunkDemuxer.cpp - Universal chunk-based demuxer implementation
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace Demuxer {

ChunkDemuxer::ChunkDemuxer(std::unique_ptr<IOHandler> handler) 
    : Demuxer(std::move(handler)) {
    m_current_stream_id = 0;
    m_current_sample = 0;
    m_eof = false;
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
            Debug::log("chunk", "ChunkDemuxer: Unknown container format: 0x", std::hex, container_chunk.fourcc);
            return false; // Unknown container format
        }
        
        // Read form type using detected endianness
        m_form_type = readChunkValue<uint32_t>();
        
        Debug::log("chunk", "ChunkDemuxer: Container=0x", std::hex, m_container_fourcc, 
                   ", Form=0x", m_form_type, std::dec, ", BigEndian=", m_big_endian);
        
        // Support WAVE (RIFF), AIFF (FORM), and other IFF variants
        if (m_form_type != WAVE_FOURCC && m_form_type != AIFF_FOURCC) {
            Debug::log("chunk", "ChunkDemuxer: Unsupported form type: 0x", std::hex, m_form_type);
            return false;
        }
        
        // Parse chunks within the container with error recovery
        while (!m_handler->eof() && m_handler->tell() < static_cast<long>(container_chunk.data_offset + container_chunk.size)) {
            Chunk chunk;
            
            try {
                chunk = readChunkHeader();
            } catch (const std::exception& e) {
                reportError("ChunkHeader", "Exception reading chunk header: " + std::string(e.what()));
                
                if (skipToNextValidSection()) {
                    continue;
                } else {
                    break;
                }
            }
            
            // Validate chunk header
            if (!validateChunkHeader(chunk)) {
                if (handleChunkSizeError(chunk)) {
                    Debug::log("chunk", "ChunkDemuxer: Recovered from chunk size error");
                } else if (recoverFromCorruptedChunk(chunk)) {
                    continue;
                } else {
                    Debug::log("chunk", "ChunkDemuxer: Failed to recover from corrupted chunk, stopping parsing");
                    break;
                }
            }
            
            Debug::log("chunk", "ChunkDemuxer: Found chunk 0x", std::hex, chunk.fourcc, std::dec, 
                       " size=", chunk.size, " at offset=", chunk.data_offset);
            
            try {
                if (m_form_type == WAVE_FOURCC) {
                    // RIFF/WAV chunks
                    if (chunk.fourcc == FMT_FOURCC) {
                        if (!parseWaveFormat(chunk)) {
                            reportError("Format", "Failed to parse WAV format chunk");
                            if (!m_fallback_mode) {
                                enableFallbackMode();
                                continue; // Try to continue in fallback mode
                            }
                            return false;
                        }
                    } else if (chunk.fourcc == DATA_FOURCC) {
                        if (!parseWaveData(chunk)) {
                            reportError("Data", "Failed to parse WAV data chunk");
                            if (!m_fallback_mode) {
                                enableFallbackMode();
                                continue;
                            }
                            return false;
                        }
                    } else if (chunk.fourcc == FACT_FOURCC) {
                        // Parse fact chunk for compressed formats
                        parseWaveFact(chunk);
                    } else if (chunk.fourcc == LIST_FOURCC) {
                        // Parse LIST chunk for metadata
                        parseWaveList(chunk);
                    } else {
                        Debug::log("chunk", "ChunkDemuxer: Skipping unknown WAV chunk 0x", std::hex, chunk.fourcc);
                        skipChunk(chunk);
                    }
                } else if (m_form_type == AIFF_FOURCC) {
                    // FORM/AIFF chunks
                    if (chunk.fourcc == COMM_FOURCC) {
                        if (!parseAiffCommon(chunk)) {
                            reportError("Format", "Failed to parse AIFF common chunk");
                            if (!m_fallback_mode) {
                                enableFallbackMode();
                                continue;
                            }
                            return false;
                        }
                    } else if (chunk.fourcc == SSND_FOURCC) {
                        if (!parseAiffSoundData(chunk)) {
                            reportError("Data", "Failed to parse AIFF sound data chunk");
                            if (!m_fallback_mode) {
                                enableFallbackMode();
                                continue;
                            }
                            return false;
                        }
                    } else if (chunk.fourcc == 0x4E414D45) { // "NAME"
                        parseAiffName(chunk);
                    } else if (chunk.fourcc == 0x41555448) { // "AUTH"
                        parseAiffAuth(chunk);
                    } else if (chunk.fourcc == 0x28632920) { // "(c) "
                        parseAiffCopyright(chunk);
                    } else if (chunk.fourcc == 0x414E4E4F) { // "ANNO"
                        parseAiffAnnotation(chunk);
                    } else {
                        Debug::log("chunk", "ChunkDemuxer: Skipping unknown AIFF chunk 0x", std::hex, chunk.fourcc);
                        skipChunk(chunk);
                    }
                } else {
                    skipChunk(chunk);
                }
            } catch (const std::exception& e) {
                reportError("ChunkParsing", "Exception parsing chunk 0x" + 
                           std::to_string(chunk.fourcc) + ": " + e.what());
                
                if (recoverFromCorruptedChunk(chunk)) {
                    continue;
                } else {
                    if (!m_fallback_mode) {
                        enableFallbackMode();
                        continue;
                    }
                    break;
                }
            }
        }
        
        // Verify we found required chunks
        if (m_audio_streams.empty()) {
            Debug::log("chunk", "ChunkDemuxer: No audio streams found in container");
            return false;
        }
        
        Debug::log("chunk", "ChunkDemuxer: Successfully parsed container with ", m_audio_streams.size(), " audio streams");
        
        m_parsed = true;
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("chunk", "ChunkDemuxer: Exception parsing container: ", e.what());
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
        
        // Add metadata
        info.title = audio_data.title;
        info.artist = audio_data.artist;
        info.album = audio_data.album;
        
        // Calculate duration
        if (audio_data.has_fact_chunk && audio_data.total_samples > 0) {
            // Use fact chunk data for accurate duration
            info.duration_samples = audio_data.total_samples;
            info.duration_ms = (info.duration_samples * 1000ULL) / audio_data.sample_rate;
        } else if (audio_data.bytes_per_frame > 0) {
            // Calculate from data size
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
    
    // Determine chunk size based on format
    size_t chunk_size = 4096; // Default 4KB
    
    // For compressed formats, use block alignment if available
    if (stream_data.block_align > 1 && stream_data.format_tag != WAVE_FORMAT_PCM) {
        // For compressed formats, read in block-aligned chunks
        chunk_size = std::max(static_cast<size_t>(stream_data.block_align * 64), chunk_size);
    }
    
    size_t bytes_to_read = std::min(chunk_size, 
                                   static_cast<size_t>(stream_data.data_size - stream_data.current_offset));
    
    // Align to block boundary for compressed formats
    if (stream_data.block_align > 1 && stream_data.format_tag != WAVE_FORMAT_PCM) {
        bytes_to_read = (bytes_to_read / stream_data.block_align) * stream_data.block_align;
        if (bytes_to_read == 0) {
            bytes_to_read = stream_data.block_align;
        }
    }
    
    MediaChunk chunk;
    chunk.stream_id = stream_id;
    chunk.data.resize(bytes_to_read);
    chunk.file_offset = stream_data.data_offset + stream_data.current_offset;
    
    // Seek to the current position and read with error recovery
    bool seek_success = performIOWithRetry([this, &chunk]() {
        return m_handler->seek(chunk.file_offset, SEEK_SET) == 0;
    }, "seeking to chunk data");
    
    if (!seek_success) {
        reportError("IO", "Failed to seek to offset " + std::to_string(chunk.file_offset));
        return MediaChunk{};
    }
    
    size_t bytes_read = 0;
    bool read_success = performIOWithRetry([this, &chunk, bytes_to_read, &bytes_read]() {
        bytes_read = m_handler->read(chunk.data.data(), 1, bytes_to_read);
        return bytes_read > 0;
    }, "reading chunk data");
    
    if (!read_success && bytes_read == 0) {
        reportError("IO", "Failed to read chunk data");
        return MediaChunk{};
    }
    
    if (bytes_read != bytes_to_read) {
        chunk.data.resize(bytes_read);
        if (bytes_read == 0) {
            m_eof = true;
            return MediaChunk{};
        }
    }
    
    // Calculate timestamps
    if (stream_data.bytes_per_frame > 0) {
        chunk.timestamp_samples = m_current_sample;
        
        // Advance sample counter based on actual data read
        if (stream_data.format_tag == WAVE_FORMAT_PCM || stream_data.format_tag == WAVE_FORMAT_IEEE_FLOAT) {
            // For PCM, calculate samples directly from bytes
            m_current_sample += chunk.data.size() / stream_data.bytes_per_frame;
        } else {
            // For compressed formats, estimate based on average bitrate
            uint64_t bytes_per_ms = (stream_data.avg_bytes_per_sec + 999) / 1000; // Round up
            if (bytes_per_ms > 0) {
                uint64_t ms_increment = chunk.data.size() / bytes_per_ms;
                m_current_sample += (ms_increment * stream_data.sample_rate) / 1000;
            }
        }
    }
    
    // Update current offset
    stream_data.current_offset += bytes_read;
    
    // Update position
    m_position_ms = (m_current_sample * 1000ULL) / stream_data.sample_rate;
    
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
    m_current_sample = (timestamp_ms * stream_data.sample_rate) / 1000;
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
    
    // FourCC is always read as big-endian by convention
    chunk.fourcc = readBE<uint32_t>();
    
    // Size uses format endianness
    chunk.size = readChunkValue<uint32_t>();
    
    // Validate chunk size
    if (chunk.size > 0x7FFFFFFF) {
        Debug::log("chunk", "ChunkDemuxer: Suspicious chunk size: ", chunk.size, " for chunk 0x", std::hex, chunk.fourcc);
        // Cap at reasonable size to prevent memory issues
        chunk.size = std::min(chunk.size, static_cast<uint32_t>(0x10000000)); // 256MB max
    }
    
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
        case WAVE_FORMAT_EXTENSIBLE:
            return "pcm"; // Usually PCM in extensible format
        case 0x0050: // WAVE_FORMAT_MPEG
            return "mp2";
        case 0x0160: // WAVE_FORMAT_WMA1
        case 0x0161: // WAVE_FORMAT_WMA2
        case 0x0162: // WAVE_FORMAT_WMA3
            return "wma";
        case 0x0011: // WAVE_FORMAT_DVI_ADPCM
        case 0x0002: // WAVE_FORMAT_ADPCM
            return "adpcm";
        case 0x0031: // WAVE_FORMAT_GSM610
            return "gsm";
        case 0x0040: // WAVE_FORMAT_G721_ADPCM
            return "g721";
        case 0x0042: // WAVE_FORMAT_G728_CELP
            return "g728";
        default:
            Debug::log("chunk", "ChunkDemuxer: Unknown WAV format tag: 0x", std::hex, format_tag);
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
        case 0x696D6134: // "ima4" - IMA ADPCM
            return "adpcm";
        case 0x4D414320: // "MAC3" - MACE 3:1
        case 0x4D414336: // "MAC6" - MACE 6:1
            return "mace";
        case 0x47534D20: // "GSM " - GSM
            return "gsm";
        case 0x64766361: // "dvca" - DV audio
            return "dv";
        case 0x51444D32: // "QDM2" - QDesign Music 2
            return "qdm2";
        default:
            Debug::log("chunk", "ChunkDemuxer: Unknown AIFF compression: 0x", std::hex, compression);
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

void ChunkDemuxer::parseWaveFact(const Chunk& chunk) {
    if (m_audio_streams.empty()) {
        skipChunk(chunk);
        return;
    }
    
    auto& stream_data = m_audio_streams.begin()->second;
    
    if (chunk.size >= 4) {
        stream_data.total_samples = readLE<uint32_t>();
        stream_data.has_fact_chunk = true;
        
        Debug::log("chunk", "ChunkDemuxer: WAV fact chunk - total_samples=", stream_data.total_samples);
    }
    
    skipChunk(chunk);
}

void ChunkDemuxer::parseWaveList(const Chunk& chunk) {
    if (m_audio_streams.empty()) {
        skipChunk(chunk);
        return;
    }
    
    auto& stream_data = m_audio_streams.begin()->second;
    
    // Read LIST type
    uint32_t list_type = readLE<uint32_t>();
    
    if (list_type == 0x4F464E49) { // "INFO"
        // Parse INFO subchunks
        uint64_t list_end = chunk.data_offset + chunk.size;
        
        while (m_handler->tell() < static_cast<long>(list_end - 8)) {
            Chunk info_chunk = readChunkHeader();
            
            if (info_chunk.fourcc == 0x4D414E49) { // "INAM" - title
                stream_data.title = readFixedString(info_chunk.size);
                // Remove null terminator if present
                if (!stream_data.title.empty() && stream_data.title.back() == '\0') {
                    stream_data.title.pop_back();
                }
            } else if (info_chunk.fourcc == 0x54524149) { // "IART" - artist
                stream_data.artist = readFixedString(info_chunk.size);
                if (!stream_data.artist.empty() && stream_data.artist.back() == '\0') {
                    stream_data.artist.pop_back();
                }
            } else if (info_chunk.fourcc == 0x4D544E49) { // "ICMT" - comment
                stream_data.comment = readFixedString(info_chunk.size);
                if (!stream_data.comment.empty() && stream_data.comment.back() == '\0') {
                    stream_data.comment.pop_back();
                }
            } else if (info_chunk.fourcc == 0x44525049) { // "IPRD" - album
                stream_data.album = readFixedString(info_chunk.size);
                if (!stream_data.album.empty() && stream_data.album.back() == '\0') {
                    stream_data.album.pop_back();
                }
            } else if (info_chunk.fourcc == 0x50595249) { // "ICOP" - copyright
                stream_data.copyright = readFixedString(info_chunk.size);
                if (!stream_data.copyright.empty() && stream_data.copyright.back() == '\0') {
                    stream_data.copyright.pop_back();
                }
            } else {
                // Skip unknown INFO chunk
                skipChunk(info_chunk);
            }
        }
    }
    
    skipChunk(chunk);
}

void ChunkDemuxer::parseAiffName(const Chunk& chunk) {
    if (m_audio_streams.empty()) {
        skipChunk(chunk);
        return;
    }
    
    auto& stream_data = m_audio_streams.begin()->second;
    stream_data.title = readFixedString(chunk.size);
    
    Debug::log("chunk", "ChunkDemuxer: AIFF NAME - title=", stream_data.title);
}

void ChunkDemuxer::parseAiffAuth(const Chunk& chunk) {
    if (m_audio_streams.empty()) {
        skipChunk(chunk);
        return;
    }
    
    auto& stream_data = m_audio_streams.begin()->second;
    stream_data.artist = readFixedString(chunk.size);
    
    Debug::log("chunk", "ChunkDemuxer: AIFF AUTH - artist=", stream_data.artist);
}

void ChunkDemuxer::parseAiffCopyright(const Chunk& chunk) {
    if (m_audio_streams.empty()) {
        skipChunk(chunk);
        return;
    }
    
    auto& stream_data = m_audio_streams.begin()->second;
    stream_data.copyright = readFixedString(chunk.size);
    
    Debug::log("chunk", "ChunkDemuxer: AIFF (c) - copyright=", stream_data.copyright);
}

void ChunkDemuxer::parseAiffAnnotation(const Chunk& chunk) {
    if (m_audio_streams.empty()) {
        skipChunk(chunk);
        return;
    }
    
    auto& stream_data = m_audio_streams.begin()->second;
    stream_data.comment = readFixedString(chunk.size);
    
    Debug::log("chunk", "ChunkDemuxer: AIFF ANNO - comment=", stream_data.comment);
}

// Error recovery method implementations

bool ChunkDemuxer::skipToNextValidSection() const {
    if (!m_handler) {
        return false;
    }
    
    Debug::log("chunk", "ChunkDemuxer: Attempting to skip to next valid chunk");
    
    // Save current position
    long current_pos = m_handler->tell();
    if (current_pos < 0) {
        return false;
    }
    
    // Look for common chunk signatures
    const std::vector<std::vector<uint8_t>> chunk_signatures = {
        {'R', 'I', 'F', 'F'},  // RIFF
        {'F', 'O', 'R', 'M'},  // FORM (AIFF)
        {'L', 'I', 'S', 'T'},  // LIST
        {'f', 'm', 't', ' '},  // fmt 
        {'d', 'a', 't', 'a'},  // data
        {'C', 'O', 'M', 'M'},  // COMM (AIFF)
        {'S', 'S', 'N', 'D'}   // SSND (AIFF)
    };
    
    uint8_t buffer[4096];
    
    while (!m_handler->eof()) {
        size_t bytes_read = m_handler->read(buffer, 1, sizeof(buffer));
        if (bytes_read < 4) {
            break;
        }
        
        // Search for chunk signatures in buffer
        for (size_t i = 0; i <= bytes_read - 4; ++i) {
            for (const auto& signature : chunk_signatures) {
                if (std::memcmp(buffer + i, signature.data(), 4) == 0) {
                    // Found potential chunk, seek to it
                    long found_pos = m_handler->tell() - static_cast<long>(bytes_read) + static_cast<long>(i);
                    if (m_handler->seek(found_pos, SEEK_SET) == 0) {
                        Debug::log("chunk", "ChunkDemuxer: Found chunk signature at offset ", found_pos);
                        const_cast<ChunkDemuxer*>(this)->m_last_valid_chunk_position = found_pos;
                        return true;
                    }
                }
            }
        }
        
        // Overlap search - move back 3 bytes to catch signatures spanning buffer boundaries
        if (bytes_read >= 4) {
            m_handler->seek(-3, SEEK_CUR);
        }
    }
    
    Debug::log("chunk", "ChunkDemuxer: No valid chunk found, restoring position");
    m_handler->seek(current_pos, SEEK_SET);
    return false;
}

bool ChunkDemuxer::resetInternalState() const {
    Debug::log("chunk", "ChunkDemuxer: Resetting internal state");
    
    try {
        // Reset stream positions
        for (auto& [stream_id, stream_data] : const_cast<ChunkDemuxer*>(this)->m_audio_streams) {
            stream_data.current_offset = 0;
        }
        
        // Reset global state
        const_cast<ChunkDemuxer*>(this)->m_current_sample = 0;
        const_cast<ChunkDemuxer*>(this)->m_position_ms = 0;
        const_cast<ChunkDemuxer*>(this)->m_eof = false;
        
        Debug::log("chunk", "ChunkDemuxer: Internal state reset successfully");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("chunk", "ChunkDemuxer: Exception during state reset: ", e.what());
        return false;
    }
}

bool ChunkDemuxer::enableFallbackMode() const {
    Debug::log("chunk", "ChunkDemuxer: Enabling fallback parsing mode");
    
    const_cast<ChunkDemuxer*>(this)->m_fallback_mode = true;
    
    // In fallback mode, we're more lenient with:
    // - Chunk size validation
    // - Padding requirements
    // - Format validation
    // - Metadata parsing
    
    return true;
}

bool ChunkDemuxer::recoverFromCorruptedChunk(const Chunk& chunk) {
    Debug::log("chunk", "ChunkDemuxer: Attempting to recover from corrupted chunk 0x", std::hex, chunk.fourcc);
    
    // Try to skip past the corrupted chunk
    if (chunk.size > 0 && chunk.size < 0x10000000) { // Reasonable size limit
        long skip_pos = static_cast<long>(chunk.data_offset + chunk.size);
        
        // Add padding if chunk size is odd (RIFF/IFF requirement)
        if (chunk.size % 2 != 0) {
            skip_pos++;
        }
        
        if (m_handler->seek(skip_pos, SEEK_SET) == 0) {
            Debug::log("chunk", "ChunkDemuxer: Skipped corrupted chunk, now at offset ", skip_pos);
            return true;
        }
    }
    
    // If skipping failed, try to find next valid chunk
    return skipToNextValidSection();
}

bool ChunkDemuxer::validateChunkHeader(const Chunk& chunk) const {
    // Validate chunk size
    if (chunk.size == 0) {
        Debug::log("chunk", "ChunkDemuxer: Chunk has zero size");
        return m_fallback_mode; // Allow in fallback mode
    }
    
    if (chunk.size > 0x7FFFFFFF) {
        Debug::log("chunk", "ChunkDemuxer: Chunk size too large: ", chunk.size);
        return false;
    }
    
    // Validate data offset
    if (chunk.data_offset == 0) {
        Debug::log("chunk", "ChunkDemuxer: Chunk has zero data offset");
        return false;
    }
    
    // Check if chunk extends beyond reasonable file size
    if (!validateFilePosition(chunk.data_offset, chunk.size)) {
        Debug::log("chunk", "ChunkDemuxer: Chunk extends beyond file bounds");
        return m_fallback_mode; // Allow in fallback mode with clamping
    }
    
    // Validate FourCC (should contain printable characters or known binary patterns)
    uint8_t* fourcc_bytes = reinterpret_cast<uint8_t*>(const_cast<uint32_t*>(&chunk.fourcc));
    bool has_printable = false;
    for (int i = 0; i < 4; ++i) {
        if (std::isprint(fourcc_bytes[i]) || fourcc_bytes[i] == ' ') {
            has_printable = true;
            break;
        }
    }
    
    if (!has_printable && !m_fallback_mode) {
        Debug::log("chunk", "ChunkDemuxer: Chunk FourCC contains no printable characters: 0x", std::hex, chunk.fourcc);
        return false;
    }
    
    return true;
}

bool ChunkDemuxer::handleChunkSizeError(Chunk& chunk) {
    Debug::log("chunk", "ChunkDemuxer: Handling chunk size error for chunk 0x", std::hex, chunk.fourcc);
    
    // Get current file position and size
    long current_pos = m_handler->tell();
    if (current_pos < 0) {
        return false;
    }
    
    m_handler->seek(0, SEEK_END);
    long file_size = m_handler->tell();
    m_handler->seek(current_pos, SEEK_SET);
    
    if (file_size < 0) {
        return false;
    }
    
    // Calculate maximum possible chunk size
    uint64_t max_possible_size = static_cast<uint64_t>(file_size) - chunk.data_offset;
    
    if (chunk.size > max_possible_size) {
        Debug::log("chunk", "ChunkDemuxer: Clamping chunk size from ", chunk.size, " to ", max_possible_size);
        chunk.size = static_cast<uint32_t>(max_possible_size);
        
        // Enable fallback mode for future chunks
        enableFallbackMode();
        return true;
    }
    
    // Check for common chunk size corruption patterns
    if (chunk.size == 0xFFFFFFFF || chunk.size == 0) {
        // Try to find next chunk to calculate actual size
        long saved_pos = m_handler->tell();
        
        if (skipToNextValidSection()) {
            long next_chunk_pos = m_handler->tell();
            uint64_t calculated_size = static_cast<uint64_t>(next_chunk_pos) - chunk.data_offset;
            
            if (calculated_size > 0 && calculated_size < 0x10000000) {
                Debug::log("chunk", "ChunkDemuxer: Calculated chunk size: ", calculated_size);
                chunk.size = static_cast<uint32_t>(calculated_size);
                m_handler->seek(saved_pos, SEEK_SET);
                enableFallbackMode();
                return true;
            }
        }
        
        m_handler->seek(saved_pos, SEEK_SET);
    }
    
    return false;
}
} // namespace Demuxer
} // namespace PsyMP3
