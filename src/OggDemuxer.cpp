/*
 * OggDemuxer.cpp - Ogg container demuxer implementation using libogg
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

OggDemuxer::OggDemuxer(std::unique_ptr<IOHandler> handler) 
    : Demuxer(std::move(handler)) {
    // Initialize libogg sync state
    ogg_sync_init(&m_sync_state);
}

OggDemuxer::~OggDemuxer() {
    // Clean up libogg streams
    for (auto& [stream_id, ogg_stream] : m_ogg_streams) {
        ogg_stream_clear(&ogg_stream);
    }
    
    // Clean up sync state
    ogg_sync_clear(&m_sync_state);
}

bool OggDemuxer::parseContainer() {
    if (m_parsed) {
        return true;
    }
    
    try {
        // Get file size
        m_handler->seek(0, SEEK_END);
        m_file_size = m_handler->tell();
        m_handler->seek(0, SEEK_SET);
        
        // Read initial data to parse headers
        if (!readIntoSyncBuffer(8192)) {
            return false;
        }
        
        // Parse headers using libogg
        bool all_headers_complete = false;
        int max_pages = 100; // Prevent infinite loops
        
        while (!all_headers_complete && max_pages-- > 0) {
            if (!processPages()) {
                // Need more data
                if (!readIntoSyncBuffer(4096)) {
                    break;
                }
                continue;
            }
            
            // Check if all audio streams have complete headers
            all_headers_complete = true;
            for (const auto& [id, stream] : m_streams) {
                if (stream.codec_type == "audio" && !stream.headers_complete) {
                    all_headers_complete = false;
                    break;
                }
            }
        }
        
        calculateDuration();
        m_parsed = true;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "OggDemuxer: Error parsing container: " << e.what() << std::endl;
        return false;
    }
}

bool OggDemuxer::readIntoSyncBuffer(size_t bytes) {
    char* buffer = ogg_sync_buffer(&m_sync_state, static_cast<long>(bytes));
    if (!buffer) {
        Debug::runtime("OggDemuxer: ogg_sync_buffer failed");
        return false;
    }
    
    long bytes_read = m_handler->read(buffer, 1, bytes);
    if (bytes_read <= 0) {
        // Check if this is actually EOF or an error
        long current_pos = m_handler->tell();
        m_handler->seek(0, SEEK_END);
        long file_size = m_handler->tell();
        m_handler->seek(current_pos, SEEK_SET);
        
        Debug::runtime("OggDemuxer: Failed to read data. bytes_read=", bytes_read, 
                       ", current_pos=", current_pos, ", file_size=", file_size);
        return false;
    }
    
    if (ogg_sync_wrote(&m_sync_state, bytes_read) != 0) {
        Debug::runtime("OggDemuxer: ogg_sync_wrote failed");
        return false;
    }
    
    return true;
}

bool OggDemuxer::processPages() {
    ogg_page page;
    
    while (ogg_sync_pageout(&m_sync_state, &page) == 1) {
        uint32_t stream_id = ogg_page_serialno(&page);
        
        // Initialize stream if it's new
        if (m_streams.find(stream_id) == m_streams.end()) {
            OggStream stream;
            stream.serial_number = stream_id;
            m_streams[stream_id] = stream;
            
            // Initialize libogg stream state
            ogg_stream_init(&m_ogg_streams[stream_id], stream_id);
        }
        
        // Add page to stream
        if (ogg_stream_pagein(&m_ogg_streams[stream_id], &page) != 0) {
            continue;
        }
        
        // Extract packets from this stream
        ogg_packet packet;
        while (ogg_stream_packetout(&m_ogg_streams[stream_id], &packet) == 1) {
            OggStream& stream = m_streams[stream_id];
            
            if (!stream.headers_complete) {
                // Try to identify codec from first packet
                if (stream.codec_name.empty()) {
                    std::vector<uint8_t> packet_data(packet.packet, packet.packet + packet.bytes);
                    stream.codec_name = identifyCodec(packet_data);
                    stream.codec_type = stream.codec_name.empty() ? "unknown" : "audio";
                }
                
                // Parse codec-specific headers
                OggPacket ogg_packet;
                ogg_packet.stream_id = stream_id;
                ogg_packet.data.assign(packet.packet, packet.packet + packet.bytes);
                ogg_packet.granule_position = packet.granulepos;
                ogg_packet.is_first_packet = packet.b_o_s;
                ogg_packet.is_last_packet = packet.e_o_s;
                
                bool is_header_packet = false;
                if (stream.codec_name == "vorbis") {
                    is_header_packet = parseVorbisHeaders(stream, ogg_packet);
                } else if (stream.codec_name == "flac") {
                    is_header_packet = parseFLACHeaders(stream, ogg_packet);
                } else if (stream.codec_name == "opus") {
                    is_header_packet = parseOpusHeaders(stream, ogg_packet);
                } else if (stream.codec_name == "speex") {
                    is_header_packet = parseSpeexHeaders(stream, ogg_packet);
                }
                
                // Only add to header packets if it was actually a valid header
                if (is_header_packet) {
                    stream.header_packets.push_back(ogg_packet);
                    
                    // Check if headers are complete (codec-specific logic)
                    if (stream.codec_name == "vorbis" && stream.header_packets.size() >= 3) {
                        stream.headers_complete = true;
                    } else if (stream.codec_name == "flac" && stream.header_packets.size() >= 1) {
                        stream.headers_complete = true;
                    } else if (stream.codec_name == "opus" && stream.header_packets.size() >= 2) {
                        stream.headers_complete = true;
                    }
                } else {
                    // If this packet wasn't a header but we haven't completed headers yet,
                    // this might be an audio packet that came before we finished parsing headers.
                    // Queue it for later processing after headers are complete.
                    OggPacket audio_packet;
                    audio_packet.stream_id = stream_id;
                    audio_packet.data.assign(packet.packet, packet.packet + packet.bytes);
                    audio_packet.granule_position = packet.granulepos;
                    audio_packet.is_first_packet = packet.b_o_s;
                    audio_packet.is_last_packet = packet.e_o_s;
                    
                    m_packet_queue.push(audio_packet);
                }
            } else {
                // Queue audio packets for later consumption
                OggPacket ogg_packet;
                ogg_packet.stream_id = stream_id;
                ogg_packet.data.assign(packet.packet, packet.packet + packet.bytes);
                ogg_packet.granule_position = packet.granulepos;
                ogg_packet.is_first_packet = packet.b_o_s;
                ogg_packet.is_last_packet = packet.e_o_s;
                
                m_packet_queue.push(ogg_packet);
            }
        }
    }
    
    return true;
}

std::vector<StreamInfo> OggDemuxer::getStreams() const {
    std::vector<StreamInfo> streams;
    
    for (const auto& [stream_id, stream] : m_streams) {
        if (stream.codec_type == "audio") {
            StreamInfo info;
            info.stream_id = stream_id;
            info.codec_name = stream.codec_name;
            info.codec_type = stream.codec_type;
            info.sample_rate = stream.sample_rate;
            info.channels = stream.channels;
            info.bitrate = stream.bitrate;
            streams.push_back(info);
        }
    }
    
    return streams;
}

StreamInfo OggDemuxer::getStreamInfo(uint32_t stream_id) const {
    auto it = m_streams.find(stream_id);
    if (it != m_streams.end()) {
        const OggStream& stream = it->second;
        StreamInfo info;
        info.stream_id = stream_id;
        info.codec_name = stream.codec_name;
        info.codec_type = stream.codec_type;
        info.sample_rate = stream.sample_rate;
        info.channels = stream.channels;
        info.bitrate = stream.bitrate;
        return info;
    }
    
    return StreamInfo{};
}

MediaChunk OggDemuxer::readChunk() {
    // Read from the best audio stream
    uint32_t stream_id = findBestAudioStream();
    if (stream_id == 0) {
        m_eof = true;
        return MediaChunk{};
    }
    
    return readChunk(stream_id);
}

MediaChunk OggDemuxer::readChunk(uint32_t stream_id) {
    // First, send header packets if they haven't been sent yet
    auto stream_it = m_streams.find(stream_id);
    if (stream_it != m_streams.end()) {
        OggStream& stream = stream_it->second;
        
        if (!stream.headers_sent && stream.headers_complete) {
            if (stream.next_header_index < stream.header_packets.size()) {
                const OggPacket& header_packet = stream.header_packets[stream.next_header_index];
                stream.next_header_index++;
                
                
                MediaChunk chunk;
                chunk.stream_id = stream_id;
                chunk.data = header_packet.data;
                chunk.timestamp_samples = header_packet.granule_position;
                chunk.timestamp_ms = granuleToMs(header_packet.granule_position, stream_id);
                chunk.is_keyframe = true;
                
                // Mark headers as sent when all have been delivered
                if (stream.next_header_index >= stream.header_packets.size()) {
                    stream.headers_sent = true;
                }
                
                return chunk;
            }
        }
    }
    
    // Then, return any queued audio packets
    while (!m_packet_queue.empty()) {
        OggPacket packet = m_packet_queue.front();
        m_packet_queue.pop();
        
        if (packet.stream_id == stream_id) {
            
            MediaChunk chunk;
            chunk.stream_id = stream_id;
            chunk.data = std::move(packet.data);
            chunk.timestamp_samples = packet.granule_position;
            chunk.timestamp_ms = granuleToMs(packet.granule_position, stream_id);
            chunk.is_keyframe = true;
            
            m_position_ms = chunk.timestamp_ms;
            return chunk;
        }
    }
    
    // Need to read more pages to get packets
    while (!m_eof) {
        if (!readIntoSyncBuffer(4096)) {
            Debug::runtime("OggDemuxer: Reached EOF at position ", m_position_ms, "ms out of ", m_duration_ms, "ms total duration");
            m_eof = true;
            break;
        }
        
        ogg_page page;
        while (ogg_sync_pageout(&m_sync_state, &page) == 1) {
            uint32_t page_stream_id = ogg_page_serialno(&page);
            
            if (m_ogg_streams.find(page_stream_id) == m_ogg_streams.end()) {
                continue; // Unknown stream
            }
            
            if (ogg_stream_pagein(&m_ogg_streams[page_stream_id], &page) != 0) {
                continue;
            }
            
            // Extract packets
            ogg_packet packet;
            while (ogg_stream_packetout(&m_ogg_streams[page_stream_id], &packet) == 1) {
                if (page_stream_id == stream_id) {
                    MediaChunk chunk;
                    chunk.stream_id = stream_id;
                    chunk.data.assign(packet.packet, packet.packet + packet.bytes);
                    chunk.timestamp_samples = packet.granulepos;
                    chunk.timestamp_ms = granuleToMs(packet.granulepos, stream_id);
                    chunk.is_keyframe = true;
                    
                    m_position_ms = chunk.timestamp_ms;
                    return chunk;
                }
            }
        }
    }
    
    m_eof = true;
    return MediaChunk{};
}

bool OggDemuxer::seekTo(uint64_t timestamp_ms) {
    // Clear packet queue
    while (!m_packet_queue.empty()) {
        m_packet_queue.pop();
    }
    
    // For simplicity, seek to beginning for now
    // A full implementation would do bisection search
    m_handler->seek(0, SEEK_SET);
    ogg_sync_reset(&m_sync_state);
    
    for (auto& [stream_id, ogg_stream] : m_ogg_streams) {
        ogg_stream_reset(&ogg_stream);
    }
    
    m_position_ms = 0;
    m_eof = false;
    
    return true;
}

bool OggDemuxer::isEOF() const {
    return m_eof;
}

uint64_t OggDemuxer::getDuration() const {
    return m_duration_ms;
}

uint64_t OggDemuxer::getPosition() const {
    return m_position_ms;
}

// Static function implementation
bool OggDemuxer::hasSignature(const std::vector<uint8_t>& data, const char* signature) {
    size_t sig_len = std::strlen(signature);
    if (data.size() < sig_len) {
        return false;
    }
    
    return std::memcmp(data.data(), signature, sig_len) == 0;
}

std::string OggDemuxer::identifyCodec(const std::vector<uint8_t>& packet_data) {
    if (OggDemuxer::hasSignature(packet_data, "\x01vorbis")) {
        return "vorbis";
    } else if (OggDemuxer::hasSignature(packet_data, "\x7f""FLAC")) {
        return "flac";
    } else if (OggDemuxer::hasSignature(packet_data, "OpusHead")) {
        return "opus";
    } else if (OggDemuxer::hasSignature(packet_data, "Speex   ")) {
        return "speex";
    }
    
    return "";
}


bool OggDemuxer::parseVorbisHeaders(OggStream& stream, const OggPacket& packet) {
    if (packet.data.size() < 7) {
        return false;
    }
    
    uint8_t packet_type = packet.data[0];
    
    if (packet_type == 1 && OggDemuxer::hasSignature(packet.data, "\x01vorbis")) {
        // Identification header
        if (packet.data.size() >= 30) {
            stream.channels = OggDemuxer::readLE<uint8_t>(packet.data, 11);
            stream.sample_rate = OggDemuxer::readLE<uint32_t>(packet.data, 12);
            stream.bitrate = OggDemuxer::readLE<uint32_t>(packet.data, 16); // Maximum bitrate
        }
        return true;
    } else if (packet_type == 3 && OggDemuxer::hasSignature(packet.data, "\x03vorbis")) {
        // Comment header - skip for now
        return true;
    } else if (packet_type == 5 && OggDemuxer::hasSignature(packet.data, "\x05vorbis")) {
        // Setup header
        stream.codec_setup_data = packet.data;
        return true;
    }
    
    return false;
}

bool OggDemuxer::parseFLACHeaders(OggStream& stream, const OggPacket& packet) {
    if (OggDemuxer::hasSignature(packet.data, "\x7f""FLAC")) {
        // Ogg FLAC identification header
        if (packet.data.size() >= 51) {
            // Skip Ogg FLAC header, parse embedded STREAMINFO
            size_t streaminfo_offset = 13; // After "\x7fFLAC" + version + header count + "fLaC" + STREAMINFO header
            
            if (packet.data.size() >= streaminfo_offset + 34) {
                // Parse STREAMINFO block
                uint32_t info1 = OggDemuxer::readBE<uint32_t>(packet.data, streaminfo_offset + 10);
                stream.sample_rate = (info1 >> 12) & 0xFFFFF;  // 20 bits
                stream.channels = ((info1 >> 9) & 0x7) + 1;    // 3 bits + 1
                
                // Extract total samples (36 bits, split across boundaries)
                uint64_t total_samples = ((uint64_t)(info1 & 0xF) << 32) | OggDemuxer::readBE<uint32_t>(packet.data, streaminfo_offset + 18);
                stream.total_samples = total_samples;
            }
        }
        return true;
    }
    
    return false;
}

bool OggDemuxer::parseOpusHeaders(OggStream& stream, const OggPacket& packet) {
    if (OggDemuxer::hasSignature(packet.data, "OpusHead")) {
        // Opus identification header
        if (packet.data.size() >= 19) {
            stream.channels = OggDemuxer::readLE<uint8_t>(packet.data, 9);
            // Note: Opus always runs at 48kHz internally, but can be presented at other rates
            stream.sample_rate = 48000;
        }
        return true;
    } else if (OggDemuxer::hasSignature(packet.data, "OpusTags")) {
        // Opus comment header
        return true;
    }
    
    return false;
}

bool OggDemuxer::parseSpeexHeaders(OggStream& stream, const OggPacket& packet) {
    if (OggDemuxer::hasSignature(packet.data, "Speex   ")) {
        // Speex header
        if (packet.data.size() >= 80) {
            stream.sample_rate = OggDemuxer::readLE<uint32_t>(packet.data, 36);
            stream.channels = OggDemuxer::readLE<uint32_t>(packet.data, 48);
        }
        return true;
    }
    
    return false;
}

void OggDemuxer::calculateDuration() {
    m_duration_ms = 0;
    
    for (const auto& [stream_id, stream] : m_streams) {
        if (stream.codec_type == "audio" && stream.sample_rate > 0) {
            if (stream.total_samples > 0) {
                uint64_t stream_duration = (stream.total_samples * 1000ULL) / stream.sample_rate;
                m_duration_ms = std::max(m_duration_ms, stream_duration);
            }
        }
    }
    
    // If we couldn't determine duration from container, try fallback methods
    if (m_duration_ms == 0) {
        // For now, set a reasonable default to prevent division by zero
        // TODO: Implement proper end-of-stream granule position reading
        Debug::runtime("OggDemuxer: Could not determine duration from headers, using fallback");
        m_duration_ms = 300000; // 5 minutes default
    }
}

uint64_t OggDemuxer::granuleToMs(uint64_t granule, uint32_t stream_id) const {
    auto it = m_streams.find(stream_id);
    if (it == m_streams.end() || it->second.sample_rate == 0) {
        return 0;
    }
    
    // Check for invalid granule positions
    if (granule == static_cast<uint64_t>(-1) || granule > 0x7FFFFFFFFFFFFFFFULL) {
        return 0;
    }
    
    return (granule * 1000ULL) / it->second.sample_rate;
}

uint64_t OggDemuxer::msToGranule(uint64_t timestamp_ms, uint32_t stream_id) const {
    auto it = m_streams.find(stream_id);
    if (it == m_streams.end() || it->second.sample_rate == 0) {
        return 0;
    }
    
    return (timestamp_ms * it->second.sample_rate) / 1000ULL;
}

uint32_t OggDemuxer::findBestAudioStream() const {
    // Find the first audio stream
    for (const auto& [stream_id, stream] : m_streams) {
        if (stream.codec_type == "audio") {
            return stream_id;
        }
    }
    
    return 0; // No audio streams found
}

bool OggDemuxer::seekToPage(uint64_t target_granule, uint32_t stream_id) {
    // Simplified seeking - a full implementation would use bisection search
    return seekTo(granuleToMs(target_granule, stream_id));
}