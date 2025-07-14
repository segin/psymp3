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
        // Get file size using IOHandler's getFileSize() method
        m_file_size = m_handler->getFileSize();
        if (m_file_size <= 0) {
            m_file_size = 0;
        }
        
        if (Debug::runtime_debug_enabled) {
            Debug::runtime("OggDemuxer: File size detection using getFileSize() - file_size=", m_file_size, " (hex=0x", std::hex, m_file_size, std::dec, ")");
        }
        // Log the actual file position and bytes read when the problem occurs
        Debug::runtime("OggDemuxer: Initial m_file_size in parseContainer: ", m_file_size);
        
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
        
        // Rewind to beginning of file for sequential packet reading
        m_handler->seek(0, SEEK_SET);
        
        // Clear sync state and reinitialize for sequential reading
        ogg_sync_clear(&m_sync_state);
        ogg_sync_init(&m_sync_state);
        
        // Reset all stream states for sequential reading
        for (auto& [stream_id, ogg_stream] : m_ogg_streams) {
            ogg_stream_clear(&ogg_stream);
            ogg_stream_init(&ogg_stream, stream_id);
        }
        
        if (Debug::runtime_debug_enabled) {
            Debug::runtime("OggDemuxer: Headers parsed, rewound to beginning for sequential packet reading");
        }
        
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
    
    off_t current_pos_before = m_handler->tell();
    long bytes_read = m_handler->read(buffer, 1, bytes);
    
    if (Debug::runtime_debug_enabled && m_position_ms > 33000 && m_position_ms < 34000) {
        Debug::runtime("OggDemuxer: readIntoSyncBuffer at ", m_position_ms, "ms - pos_before=", current_pos_before, 
                       ", requested=", bytes, ", bytes_read=", bytes_read);
    }
    
    if (bytes_read <= 0) {
        // Not at EOF, but got no data - this might be temporary
        // Return false but don't set m_eof
        return false;
    }
    
    if (ogg_sync_wrote(&m_sync_state, bytes_read) != 0) {
        Debug::runtime("OggDemuxer: ogg_sync_wrote failed");
        return false;
    }
    
    // Log file position advancement around the problem area
    if (Debug::runtime_debug_enabled && m_position_ms > 33000 && m_position_ms < 34000) {
        long pos_after_write = m_handler->tell();
        Debug::runtime("OggDemuxer: readIntoSyncBuffer success - read ", bytes_read, " bytes, pos advanced from ", 
                       current_pos_before, " to ", pos_after_write);
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
            if (Debug::runtime_debug_enabled) {
                Debug::runtime("OggDemuxer: Extracted packet from stream ", stream_id, ", size=", packet.bytes);
            }
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
                            Debug::runtime("OggDemuxer: Opus headers complete for stream ", stream_id, ", header_count=", stream.header_packets.size());
                        }
                    } else if (stream.codec_name == "opus") {
                        if (Debug::runtime_debug_enabled) {
                            Debug::runtime("OggDemuxer: Opus headers still incomplete for stream ", stream_id, ", header_count=", stream.header_packets.size(), " (need 2)");
                        }
                    }
                    
                    if (Debug::runtime_debug_enabled) {
                        Debug::runtime("OggDemuxer: Stream ", stream_id, " now has ", stream.header_packets.size(), " header packets, codec=", stream.codec_name, ", headers_complete=", stream.headers_complete);
                    }
                } else {
                    // If this packet wasn't a header but we haven't completed headers yet,
                    // this might be an audio packet that came before we finished parsing headers.
                    // Queue it for later processing after headers are complete.
                    
                    if (Debug::runtime_debug_enabled) {
                        Debug::runtime("OggDemuxer: Non-header packet during header parsing, stream_id=", stream_id, 
                                       ", packet_size=", packet.bytes, ", header_packets_count=", stream.header_packets.size());
                    }
                    
                    // Don't queue packets during header parsing - read sequentially after headers complete
                }
            } else {
                // Don't queue packets during header parsing - read sequentially after headers complete
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
            info.artist = stream.artist;
            info.title = stream.title;
            info.album = stream.album;
            info.duration_ms = m_duration_ms;
            info.duration_samples = stream.total_samples;
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
        info.artist = stream.artist;
        info.title = stream.title;
        info.album = stream.album;
        info.duration_ms = m_duration_ms;
        info.duration_samples = stream.total_samples;
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
    
    // Clear any remaining queued packets - we'll read sequentially instead
    while (!m_packet_queue.empty()) {
        m_packet_queue.pop();
    }
    
    // Need to read more pages to get packets
    while (!m_eof) {
        if (!readIntoSyncBuffer(4096)) {
            if (Debug::runtime_debug_enabled) {
                Debug::runtime("OggDemuxer: readChunk - readIntoSyncBuffer returned false. Current pos: ", m_position_ms, "ms, EOF: ", m_eof);
            }
            if (m_eof) { // If readIntoSyncBuffer set m_eof, then we are truly at EOF
                Debug::runtime("OggDemuxer: Reached EOF at position ", m_position_ms, "ms out of ", m_duration_ms, "ms total duration");
                break;
            }
            // If not EOF, it means a temporary read issue, so we continue trying
            continue;
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
            
            // Log large granule position jumps that might indicate file skipping
            if (Debug::runtime_debug_enabled && page_granule != static_cast<uint64_t>(-1) && m_position_ms > 30000 && m_position_ms < 40000) {
                uint64_t page_timestamp = granuleToMs(page_granule, page_stream_id);
                long current_file_pos = m_handler->tell();
                Debug::runtime("OggDemuxer: Found page with granule=", page_granule, ", timestamp=", page_timestamp, 
                               ", ms, file_pos=", current_file_pos, ", current_position=", m_position_ms, "ms");
            }
            
            // Track the maximum granule position seen for duration calculation
            if (page_granule != static_cast<uint64_t>(-1)) {
                m_max_granule_seen = std::max(m_max_granule_seen, page_granule);
                
                // Update duration estimate when we see a new maximum granule position
                if (m_max_granule_seen > 0) {
                    uint32_t stream_id_for_duration = findBestAudioStream(); // Use a different variable name to avoid conflict
                    if (stream_id_for_duration != 0) {
                        uint64_t estimated_duration = granuleToMs(m_max_granule_seen, stream_id_for_duration);
                        if (estimated_duration > m_duration_ms) {
                            m_duration_ms = estimated_duration;
                            if (Debug::runtime_debug_enabled) {
                                Debug::runtime("OggDemuxer: Updated duration estimate to ", m_duration_ms, "ms based on granule ", m_max_granule_seen);
                            }
                        }
                    }
                }
            }
            
            // Extract packets
            ogg_packet packet;
            while (ogg_stream_packetout(&m_ogg_streams[page_stream_id], &packet) == 1) {
                if (page_stream_id == stream_id) {
                    // If headers have already been sent, and this is a beginning-of-stream packet,
                    // it's a redundant header re-emitted by libogg after a seek/reset. Discard it.
                    if (stream_it->second.headers_sent && packet.b_o_s) {
                        if (Debug::runtime_debug_enabled) {
                            Debug::runtime("OggDemuxer: Discarding redundant BOS packet (likely header re-emission) for stream ", stream_id);
                        }
                        continue; // Skip this packet
                    }

                    MediaChunk chunk;
                    chunk.stream_id = stream_id;
                    chunk.data.assign(packet.packet, packet.packet + packet.bytes);
                    
                    // Use packet granule position if available, otherwise use page granule (RFC-compliant)
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
                    chunk.is_keyframe = false; // Audio data packets are not keyframes
                    
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
            stream.pre_skip = OggDemuxer::readLE<uint16_t>(packet.data, 10); // Read pre-skip
            
            if (Debug::runtime_debug_enabled) {
                Debug::runtime("OggDemuxer: OpusHead header found, channels=", stream.channels, ", pre_skip=", stream.pre_skip, ", packet_size=", packet.data.size());
            }
        }
        return true;
    } else if (OggDemuxer::hasSignature(packet.data, "OpusTags")) {
        // Opus comment header - must be second
        if (Debug::runtime_debug_enabled) {
            Debug::runtime("OggDemuxer: OpusTags header found, packet_size=", packet.data.size());
        }
        
        // Parse OpusTags metadata (follows Vorbis comment format)
        parseOpusTags(stream, packet);
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

void OggDemuxer::parseOpusTags(OggStream& stream, const OggPacket& packet) {
    // OpusTags format follows Vorbis comment structure:
    // 8 bytes: "OpusTags"
    // 4 bytes: vendor_length (little-endian)
    // vendor_length bytes: vendor string
    // 4 bytes: user_comment_list_length (little-endian)
    // For each comment:
    //   4 bytes: length (little-endian)
    //   length bytes: comment in UTF-8 format "FIELD=value"
    
    if (packet.data.size() < 16) {
        return; // Too small to contain valid OpusTags
    }
    
    size_t offset = 8; // Skip "OpusTags"
    
    // Read vendor string length
    uint32_t vendor_length = readLE<uint32_t>(packet.data, offset);
    offset += 4;
    
    if (offset + vendor_length > packet.data.size()) {
        return; // Invalid vendor length
    }
    
    offset += vendor_length; // Skip vendor string
    
    if (offset + 4 > packet.data.size()) {
        return; // No room for comment count
    }
    
    // Read number of user comments
    uint32_t comment_count = readLE<uint32_t>(packet.data, offset);
    offset += 4;
    
    // Parse each comment
    for (uint32_t i = 0; i < comment_count && offset < packet.data.size(); i++) {
        if (offset + 4 > packet.data.size()) {
            break;
        }
        
        uint32_t comment_length = readLE<uint32_t>(packet.data, offset);
        offset += 4;
        
        if (offset + comment_length > packet.data.size()) {
            break;
        }
        
        // Extract comment string
        std::string comment(reinterpret_cast<const char*>(packet.data.data() + offset), comment_length);
        offset += comment_length;
        
        // Parse FIELD=value format
        size_t equals_pos = comment.find('=');
        if (equals_pos != std::string::npos) {
            std::string field = comment.substr(0, equals_pos);
            std::string value = comment.substr(equals_pos + 1);
            
            // Convert field to uppercase for case-insensitive comparison
            std::transform(field.begin(), field.end(), field.begin(), ::toupper);
            
            if (field == "ARTIST") {
                stream.artist = value;
            } else if (field == "TITLE") {
                stream.title = value;
            } else if (field == "ALBUM") {
                stream.album = value;
            }
            
            if (Debug::runtime_debug_enabled) {
                // Don't dump large binary fields like METADATA_BLOCK_PICTURE
                if (field == "METADATA_BLOCK_PICTURE") {
                    Debug::runtime("OggDemuxer: OpusTags - ", field, "=<", value.size(), " bytes of binary data>");
                } else {
                    Debug::runtime("OggDemuxer: OpusTags - ", field, "=", value);
                }
            }
        }
    }
}

void OggDemuxer::calculateDuration() {
    if (Debug::runtime_debug_enabled) {
        Debug::runtime("OggDemuxer: calculateDuration() called");
    }
    m_duration_ms = 0;
    
    for (const auto& [stream_id, stream] : m_streams) {
        if (stream.codec_type == "audio" && stream.sample_rate > 0) {
            if (Debug::runtime_debug_enabled) {
                Debug::runtime("OggDemuxer: calculateDuration - Stream ", stream_id, " (", stream.codec_name, ") has total_samples=", stream.total_samples, ", sample_rate=", stream.sample_rate);
            }
            if (stream.total_samples > 0) {
                uint64_t stream_duration = (stream.total_samples * 1000ULL) / stream.sample_rate;
                m_duration_ms = std::max(m_duration_ms, stream_duration);
                if (Debug::runtime_debug_enabled) {
                    Debug::runtime("OggDemuxer: calculateDuration - Calculated duration from samples: ", stream_duration, "ms");
                }
            }
        }
    }
    
    // If we couldn't determine duration from container, try using tracked max granule
    if (m_duration_ms == 0) {
        if (m_max_granule_seen > 0) {
            if (Debug::runtime_debug_enabled) {
                Debug::runtime("OggDemuxer: calculateDuration - Using tracked max granule: ", m_max_granule_seen);
            }
            // Use the tracked maximum granule position
            uint32_t stream_id = findBestAudioStream();
            if (stream_id != 0) {
                m_duration_ms = granuleToMs(m_max_granule_seen, stream_id);
                if (Debug::runtime_debug_enabled) {
                    Debug::runtime("OggDemuxer: calculateDuration - Duration from tracked max granule: ", m_max_granule_seen, " -> ", m_duration_ms, "ms");
                }
            }
        } else {
            if (Debug::runtime_debug_enabled) {
                Debug::runtime("OggDemuxer: calculateDuration - Falling back to reading last page.");
            }
            // Fall back to reading last page
            uint64_t last_granule = getLastGranulePosition();
            if (Debug::runtime_debug_enabled) {
                Debug::runtime("OggDemuxer: calculateDuration - getLastGranulePosition returned: ", last_granule);
            }
            if (last_granule > 0) {
                // Use the first audio stream for duration calculation
                uint32_t stream_id = findBestAudioStream();
                if (stream_id != 0) {
                    m_duration_ms = granuleToMs(last_granule, stream_id);
                    if (Debug::runtime_debug_enabled) {
                        Debug::runtime("OggDemuxer: calculateDuration - Duration from last granule: ", m_duration_ms, "ms");
                    }
                }
            }
        }
    }
    
    // Final fallback - use a reasonable default
    if (m_duration_ms == 0) {
        Debug::runtime("OggDemuxer: calculateDuration - Could not determine duration - no fallback used");
        // No fallback - leave as 0 if duration cannot be determined
    }
}

uint64_t OggDemuxer::granuleToMs(uint64_t granule, uint32_t stream_id) const {
    auto it = m_streams.find(stream_id);
    if (it == m_streams.end() || it->second.sample_rate == 0) {
        if (Debug::runtime_debug_enabled) {
            Debug::runtime("OggDemuxer: granuleToMs - Invalid stream_id or sample_rate is 0. stream_id=", stream_id);
        }
        return 0;
    }
    
    // Check for invalid granule positions
    if (granule == static_cast<uint64_t>(-1) || granule > 0x7FFFFFFFFFFFFFFULL) {
        if (Debug::runtime_debug_enabled) {
            Debug::runtime("OggDemuxer: granuleToMs - Invalid granule position: ", granule);
        }
        return 0;
    }
    
    if (Debug::runtime_debug_enabled) {
        Debug::runtime("OggDemuxer: granuleToMs - Input granule=", granule, ", stream_id=", stream_id, ", codec=", it->second.codec_name, ", sample_rate=", it->second.sample_rate, ", pre_skip=", it->second.pre_skip);
    }
    
    // For Opus streams, granule positions are in 48kHz sample units
    // and represent the total decoded samples including pre-skip
    // However, for time calculation, we should use the granule directly
    // as it already accounts for the proper sample count
    if (it->second.codec_name == "opus") {
        // Opus always uses 48kHz for granule positions regardless of output rate
        // Granule position includes pre-skip, so subtract it for actual duration
        uint64_t effective_granule = granule;
        if (granule > it->second.pre_skip) {
            effective_granule = granule - it->second.pre_skip;
        } else {
            effective_granule = 0;
        }
        if (Debug::runtime_debug_enabled) {
            Debug::runtime("OggDemuxer: granuleToMs (Opus) - effective_granule=", effective_granule, ", result=", (effective_granule * 1000ULL) / 48000ULL);
        }
        return (effective_granule * 1000ULL) / 48000ULL;
    }
    
    uint64_t result = (granule * 1000ULL) / it->second.sample_rate;
    if (Debug::runtime_debug_enabled) {
        Debug::runtime("OggDemuxer: granuleToMs (Non-Opus) - result=", result);
    }
    return result;
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
        if (Debug::runtime_debug_enabled) {
            Debug::runtime("OggDemuxer: getLastGranulePosition - file_size is 0");
        }
        return 0;
    }
    
    if (Debug::runtime_debug_enabled) {
        Debug::runtime("OggDemuxer: getLastGranulePosition - file_size=", m_file_size);
    }
    
    // Save current position
    long current_pos = m_handler->tell();
    
    // Alternative approach: scan for OggS signatures manually
    // This handles cases where the buffer doesn't start at a page boundary
    const size_t SCAN_SIZE = 1048576; // 1MB
    size_t scan_size = std::min(SCAN_SIZE, m_file_size);
    long scan_start = std::max(0L, static_cast<long>(m_file_size) - static_cast<long>(scan_size));
    
    m_handler->seek(scan_start, SEEK_SET);
    
    std::vector<uint8_t> scan_buffer(scan_size);
    long bytes_read = m_handler->read(scan_buffer.data(), 1, scan_size);
    
    if (bytes_read > 0) {
        uint64_t last_granule = 0;
        size_t pages_found = 0;
        
        // Scan for "OggS" signatures
        for (size_t i = 0; i <= static_cast<size_t>(bytes_read) - 4; i++) {
            if (scan_buffer[i] == 'O' && scan_buffer[i+1] == 'g' && 
                scan_buffer[i+2] == 'g' && scan_buffer[i+3] == 'S') {
                
                // Found potential OGG page - check if we can parse the granule position
                if (i + 14 <= static_cast<size_t>(bytes_read)) {
                    // Granule position is at offset 6-13 in OGG page header
                    uint64_t granule = 0;
                    std::memcpy(&granule, &scan_buffer[i + 6], 8);
                    
                    if (granule != static_cast<uint64_t>(-1) && granule > last_granule) {
                        last_granule = granule;
                        pages_found++;
                        
                        // Found a page with valid granule position
                    }
                }
            }
        }
        
        if (Debug::runtime_debug_enabled) {
            Debug::runtime("OggDemuxer: getLastGranulePosition - manual scan found ", pages_found, " pages, last_granule=", last_granule);
        }
        
        if (pages_found > 0) {
            // Restore original position
            m_handler->seek(current_pos, SEEK_SET);
            return last_granule;
        }
    }
    
    // Restore original position
    m_handler->seek(current_pos, SEEK_SET);
    
    if (Debug::runtime_debug_enabled) {
        Debug::runtime("OggDemuxer: getLastGranulePosition - failed to find any pages with all search sizes");
    }
    
    return 0;
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