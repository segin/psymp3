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
        int max_pages = 500; // Increased limit to find all headers
        int consecutive_no_progress = 0;
        
        while (!all_headers_complete && max_pages-- > 0) {
            bool made_progress = processPages();
            
            if (!made_progress) {
                // Need more data
                if (!readIntoSyncBuffer(4096)) {
                    consecutive_no_progress++;
                    if (consecutive_no_progress > 10) {
                        if (Debug::runtime_debug_enabled) {
                            Debug::runtime("OggDemuxer: No progress for 10 iterations, stopping header parsing");
                        }
                        break;
                    }
                } else {
                    consecutive_no_progress = 0;
                }
                continue;
            }
            
            consecutive_no_progress = 0;
            
            // Check if all audio streams have complete headers
            all_headers_complete = true;
            for (const auto& [id, stream] : m_streams) {
                if (stream.codec_type == "audio" && !stream.headers_complete) {
                    all_headers_complete = false;
                    if (Debug::runtime_debug_enabled) {
                        Debug::runtime("OggDemuxer: Stream ", id, " codec=", stream.codec_name, 
                                       " headers not complete, header_count=", stream.header_packets.size());
                    }
                    break;
                }
            }
            
            if (all_headers_complete && Debug::runtime_debug_enabled) {
                Debug::runtime("OggDemuxer: All headers complete, stopping header parsing");
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
    bool processed_any_pages = false;
    
    while (ogg_sync_pageout(&m_sync_state, &page) == 1) {
        processed_any_pages = true;
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
                
                if (Debug::runtime_debug_enabled) {
                    Debug::runtime("OggDemuxer: Processing packet for stream ", stream_id, ", codec=", stream.codec_name, 
                                   ", packet_size=", packet.bytes, ", headers_complete=", stream.headers_complete);
                }
                
                bool is_header_packet = false;
                if (stream.codec_name == "vorbis") {
                    is_header_packet = parseVorbisHeaders(stream, ogg_packet);
                } else if (stream.codec_name == "flac") {
                    is_header_packet = parseFLACHeaders(stream, ogg_packet);
                } else if (stream.codec_name == "opus") {
                    is_header_packet = parseOpusHeaders(stream, ogg_packet);
                    if (Debug::runtime_debug_enabled) {
                        Debug::runtime("OggDemuxer: parseOpusHeaders returned ", is_header_packet, " for packet size ", ogg_packet.data.size());
                    }
                } else if (stream.codec_name == "speex") {
                    is_header_packet = parseSpeexHeaders(stream, ogg_packet);
                }
                
                // Only add to header packets if it was actually a valid header
                if (is_header_packet) {
                    stream.header_packets.push_back(ogg_packet);
                    
                    if (Debug::runtime_debug_enabled) {
                        Debug::runtime("OggDemuxer: Added header packet ", stream.header_packets.size(), " for stream ", stream_id, ", codec=", stream.codec_name);
                    }
                    
                    // Check if headers are complete (codec-specific logic)
                    if (stream.codec_name == "vorbis" && stream.header_packets.size() >= 3) {
                        stream.headers_complete = true;
                    } else if (stream.codec_name == "flac" && stream.header_packets.size() >= 1) {
                        stream.headers_complete = true;
                    } else if (stream.codec_name == "opus" && stream.header_packets.size() >= 2) {
                        stream.headers_complete = true;
                        if (Debug::runtime_debug_enabled) {
                            Debug::runtime("OggDemuxer: Opus headers complete for stream ", stream_id);
                        }
                    }
                } else {
                    // If this packet wasn't a header but we haven't completed headers yet,
                    // this might be an audio packet that came before we finished parsing headers.
                    // Queue it for later processing after headers are complete.
                    
                    if (Debug::runtime_debug_enabled) {
                        Debug::runtime("OggDemuxer: Non-header packet during header parsing, stream_id=", stream_id, 
                                       ", packet_size=", packet.bytes, ", header_packets_count=", stream.header_packets.size());
                    }
                    
                    // Limit packet queue size to prevent memory bloat
                    constexpr size_t MAX_PACKET_QUEUE_SIZE = 200;
                    if (m_packet_queue.size() < MAX_PACKET_QUEUE_SIZE) {
                        OggPacket audio_packet;
                        audio_packet.stream_id = stream_id;
                        audio_packet.data.assign(packet.packet, packet.packet + packet.bytes);
                        audio_packet.granule_position = packet.granulepos;
                        audio_packet.is_first_packet = packet.b_o_s;
                        audio_packet.is_last_packet = packet.e_o_s;
                        
                        m_packet_queue.push(audio_packet);
                    }
                }
            } else {
                // Queue audio packets for later consumption
                
                // Limit packet queue size to prevent memory bloat
                constexpr size_t MAX_PACKET_QUEUE_SIZE = 200;
                if (m_packet_queue.size() < MAX_PACKET_QUEUE_SIZE) {
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
    }
    
    return processed_any_pages;
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
                
                if (Debug::runtime_debug_enabled) {
                    std::string header_type = "unknown";
                    if (stream.codec_name == "opus") {
                        if (chunk.data.size() >= 8) {
                            if (std::memcmp(chunk.data.data(), "OpusHead", 8) == 0) {
                                header_type = "OpusHead";
                            } else if (std::memcmp(chunk.data.data(), "OpusTags", 8) == 0) {
                                header_type = "OpusTags";
                            }
                        }
                    }
                    Debug::runtime("OggDemuxer: Sending header packet ", stream.next_header_index, "/", stream.header_packets.size(), 
                                   ", type=", header_type, ", size=", chunk.data.size(), " bytes");
                }
                
                // Mark headers as sent when all have been delivered
                if (stream.next_header_index >= stream.header_packets.size()) {
                    stream.headers_sent = true;
                    if (Debug::runtime_debug_enabled) {
                        Debug::runtime("OggDemuxer: All headers sent for stream ", stream_id);
                    }
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
            // Check if this queued packet is actually a header packet that was missed
            auto stream_it = m_streams.find(stream_id);
            if (stream_it != m_streams.end()) {
                OggStream& stream = stream_it->second;
                
                // For Opus, check if this is a missed OpusTags header
                if (stream.codec_name == "opus" && !stream.headers_complete) {
                    if (packet.data.size() >= 8 && std::memcmp(packet.data.data(), "OpusTags", 8) == 0) {
                        if (Debug::runtime_debug_enabled) {
                            Debug::runtime("OggDemuxer: Found missed OpusTags header in queue, processing as header");
                        }
                        
                        // Process as header packet
                        if (parseOpusHeaders(stream, packet)) {
                            stream.header_packets.push_back(packet);
                            if (stream.header_packets.size() >= 2) {
                                stream.headers_complete = true;
                                stream.headers_sent = false;
                                stream.next_header_index = 0;
                                if (Debug::runtime_debug_enabled) {
                                    Debug::runtime("OggDemuxer: Opus headers now complete, will deliver headers first");
                                }
                            }
                        }
                        
                        // Try to return the first header packet instead
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
                                
                                if (Debug::runtime_debug_enabled) {
                                    std::string header_type = "unknown";
                                    if (chunk.data.size() >= 8) {
                                        if (std::memcmp(chunk.data.data(), "OpusHead", 8) == 0) {
                                            header_type = "OpusHead";
                                        } else if (std::memcmp(chunk.data.data(), "OpusTags", 8) == 0) {
                                            header_type = "OpusTags";
                                        }
                                    }
                                    Debug::runtime("OggDemuxer: Delivering header packet ", stream.next_header_index, "/", stream.header_packets.size(), 
                                                   ", type=", header_type, ", size=", chunk.data.size(), " bytes");
                                }
                                
                                if (stream.next_header_index >= stream.header_packets.size()) {
                                    stream.headers_sent = true;
                                }
                                
                                return chunk;
                            }
                        }
                        
                        continue; // Skip this packet since it was processed as header
                    }
                }
            }
            
            MediaChunk chunk;
            chunk.stream_id = stream_id;
            chunk.data = std::move(packet.data);
            chunk.timestamp_samples = packet.granule_position;
            chunk.timestamp_ms = granuleToMs(packet.granule_position, stream_id);
            chunk.is_keyframe = true;
            
            if (Debug::runtime_debug_enabled) {
                Debug::runtime("OggDemuxer: Queued packet granule=", packet.granule_position, 
                               ", timestamp_ms=", chunk.timestamp_ms, 
                               ", packet_size=", chunk.data.size());
            }
            
            // Only update position if we have a valid, increasing timestamp
            // For Opus, timestamp 0 is valid (start of stream)
            if (chunk.timestamp_ms >= m_position_ms) {
                m_position_ms = chunk.timestamp_ms;
            }
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
            
            // Get page granule position for interpolation
            uint64_t page_granule = ogg_page_granulepos(&page);
            
            // Extract packets
            ogg_packet packet;
            while (ogg_stream_packetout(&m_ogg_streams[page_stream_id], &packet) == 1) {
                if (page_stream_id == stream_id) {
                    MediaChunk chunk;
                    chunk.stream_id = stream_id;
                    chunk.data.assign(packet.packet, packet.packet + packet.bytes);
                    
                    // Use packet granule position if available, otherwise interpolate
                    uint64_t effective_granule = packet.granulepos;
                    if (effective_granule == static_cast<uint64_t>(-1)) {
                        // Use page granule position but only if it's from our stream
                        if (page_granule != static_cast<uint64_t>(-1)) {
                            effective_granule = page_granule;
                        } else {
                            effective_granule = 0;
                        }
                    }
                    
                    chunk.timestamp_samples = effective_granule;
                    chunk.timestamp_ms = granuleToMs(effective_granule, stream_id);
                    chunk.is_keyframe = true;
                    
                    if (Debug::runtime_debug_enabled) {
                        Debug::runtime("OggDemuxer: Packet granule=", packet.granulepos, 
                                       ", page_granule=", page_granule,
                                       ", effective_granule=", effective_granule,
                                       ", timestamp_ms=", chunk.timestamp_ms, 
                                       ", packet_size=", packet.bytes);
                    }
                    
                    // Only update position if we have a valid, increasing timestamp
                    // For Opus, timestamp 0 is valid (start of stream)
                    if (chunk.timestamp_ms >= m_position_ms) {
                        m_position_ms = chunk.timestamp_ms;
                    }
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
    
    // If seeking to beginning, do it directly
    if (timestamp_ms == 0) {
        m_handler->seek(0, SEEK_SET);
        ogg_sync_reset(&m_sync_state);
        
        for (auto& [stream_id, ogg_stream] : m_ogg_streams) {
            ogg_stream_reset(&ogg_stream);
        }
        
        // When seeking to beginning, headers need to be resent
        for (auto& [stream_id, stream] : m_streams) {
            if (stream.headers_complete) {
                stream.headers_sent = false;
                stream.next_header_index = 0;
            }
        }
        
        m_position_ms = 0;
        m_eof = false;
        return true;
    }
    
    // Find the best audio stream to use for seeking
    uint32_t stream_id = findBestAudioStream();
    if (stream_id == 0) {
        return false;
    }
    
    // Convert timestamp to granule position for seeking
    uint64_t target_granule = msToGranule(timestamp_ms, stream_id);
    
    // Use bisection search to find the target position
    return seekToPage(target_granule, stream_id);
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
    if (Debug::runtime_debug_enabled) {
        std::string first_bytes;
        for (size_t i = 0; i < std::min(size_t(16), packet.data.size()); i++) {
            char c = packet.data[i];
            if (c >= 32 && c <= 126) {
                first_bytes += c;
            } else {
                first_bytes += "\\x" + std::to_string(static_cast<unsigned char>(c));
            }
        }
        Debug::runtime("OggDemuxer: parseOpusHeaders called, packet_size=", packet.data.size(), ", first 16 bytes: '", first_bytes, "'");
    }
    
    if (OggDemuxer::hasSignature(packet.data, "OpusHead")) {
        // Opus identification header - must be first
        if (packet.data.size() >= 19) {
            stream.channels = OggDemuxer::readLE<uint8_t>(packet.data, 9);
            // Note: Opus always runs at 48kHz internally, but can be presented at other rates
            stream.sample_rate = 48000;
            
            if (Debug::runtime_debug_enabled) {
                Debug::runtime("OggDemuxer: OpusHead header found, channels=", stream.channels, ", packet_size=", packet.data.size());
            }
        }
        return true;
    } else if (OggDemuxer::hasSignature(packet.data, "OpusTags")) {
        // Opus comment header - must be second
        if (Debug::runtime_debug_enabled) {
            Debug::runtime("OggDemuxer: OpusTags header found, packet_size=", packet.data.size());
        }
        return true;
    }
    
    if (Debug::runtime_debug_enabled) {
        Debug::runtime("OggDemuxer: Unknown Opus packet, size=", packet.data.size());
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
    
    // If we couldn't determine duration from container, try reading last page
    if (m_duration_ms == 0) {
        uint64_t last_granule = getLastGranulePosition();
        if (last_granule > 0) {
            // Use the first audio stream for duration calculation
            uint32_t stream_id = findBestAudioStream();
            if (stream_id != 0) {
                m_duration_ms = granuleToMs(last_granule, stream_id);
                Debug::runtime("OggDemuxer: Duration from last granule: ", m_duration_ms, "ms");
            }
        }
    }
    
    // Final fallback - use a reasonable default
    if (m_duration_ms == 0) {
        Debug::runtime("OggDemuxer: Could not determine duration, using fallback");
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
    
    // For Opus, granule position 0 is valid (start of stream)
    // Don't return 0 for granule position 0 unless it's actually invalid
    
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

uint64_t OggDemuxer::getLastGranulePosition() {
    if (m_file_size == 0) {
        return 0;
    }
    
    // Save current position
    long current_pos = m_handler->tell();
    
    // Read from the end of file to find the last page
    const size_t SEARCH_SIZE = 65536; // 64KB should be enough to find last page
    long search_start = std::max(0L, static_cast<long>(m_file_size) - static_cast<long>(SEARCH_SIZE));
    
    m_handler->seek(search_start, SEEK_SET);
    
    ogg_sync_state temp_sync;
    ogg_sync_init(&temp_sync);
    
    uint64_t last_granule = 0;
    
    // Read data into temp sync buffer
    char* buffer = ogg_sync_buffer(&temp_sync, SEARCH_SIZE);
    if (buffer) {
        long bytes_read = m_handler->read(buffer, 1, SEARCH_SIZE);
        if (bytes_read > 0 && ogg_sync_wrote(&temp_sync, bytes_read) == 0) {
            // Find the last page by reading all pages in this chunk
            ogg_page page;
            while (ogg_sync_pageout(&temp_sync, &page) == 1) {
                uint64_t granule = ogg_page_granulepos(&page);
                if (granule != static_cast<uint64_t>(-1) && granule > last_granule) {
                    last_granule = granule;
                }
            }
        }
    }
    
    ogg_sync_clear(&temp_sync);
    
    // Restore original position
    m_handler->seek(current_pos, SEEK_SET);
    
    return last_granule;
}

bool OggDemuxer::seekToPage(uint64_t target_granule, uint32_t stream_id) {
    if (m_file_size == 0 || target_granule == 0) {
        return seekTo(0);
    }
    
    // Bisection search to find the page containing target_granule
    long left = 0;
    long right = m_file_size;
    long best_pos = 0;
    uint64_t best_granule = 0;
    
    constexpr int MAX_ITERATIONS = 32; // Limit iterations to prevent infinite loops
    int iterations = 0;
    
    while (left < right && iterations++ < MAX_ITERATIONS) {
        long mid = left + (right - left) / 2;
        
        // Seek to midpoint and find next page
        m_handler->seek(mid, SEEK_SET);
        ogg_sync_reset(&m_sync_state);
        
        // Read data to find a valid page
        if (!readIntoSyncBuffer(8192)) {
            break;
        }
        
        ogg_page page;
        bool found_page = false;
        uint64_t page_granule = 0;
        uint32_t page_stream_id = 0;
        
        // Look for a page from our target stream
        while (ogg_sync_pageout(&m_sync_state, &page) == 1) {
            page_stream_id = ogg_page_serialno(&page);
            page_granule = ogg_page_granulepos(&page);
            
            // Found a page from our target stream with valid granule
            if (page_stream_id == stream_id && page_granule != static_cast<uint64_t>(-1)) {
                found_page = true;
                break;
            }
        }
        
        if (!found_page) {
            // No valid page found, try moving right
            left = mid + 1;
            continue;
        }
        
        if (page_granule < target_granule) {
            // Page is before target, search right half
            left = mid + 1;
            best_pos = m_handler->tell() - 8192; // Approximate page position
            best_granule = page_granule;
        } else if (page_granule > target_granule) {
            // Page is after target, search left half
            right = mid;
        } else {
            // Exact match found
            best_pos = m_handler->tell() - 8192;
            best_granule = page_granule;
            break;
        }
    }
    
    // Seek to the best position found
    if (best_pos > 0) {
        m_handler->seek(best_pos, SEEK_SET);
        ogg_sync_reset(&m_sync_state);
        
        // Reset all stream states
        for (auto& [sid, ogg_stream] : m_ogg_streams) {
            ogg_stream_reset(&ogg_stream);
        }
        
        // For non-zero seeks, ensure headers are not resent since codec is already initialized
        for (auto& [sid, stream] : m_streams) {
            if (stream.headers_complete) {
                stream.headers_sent = true;  // Mark headers as already sent
            }
        }
        
        m_position_ms = granuleToMs(best_granule, stream_id);
        m_eof = false;
        
        // Clear packet queue since we've changed position
        while (!m_packet_queue.empty()) {
            m_packet_queue.pop();
        }
        
        if (Debug::runtime_debug_enabled) {
            Debug::runtime("OggDemuxer: Bisection seek to granule=", target_granule, 
                           ", found granule=", best_granule, 
                           ", position=", m_position_ms, "ms");
        }
        
        return true;
    }
    
    // Fallback to beginning if seeking failed
    return seekTo(0);
}