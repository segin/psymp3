/*
 * OggDemuxer.cpp - Ogg container demuxer implementation using libogg
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

// OggDemuxer is built if HAVE_OGGDEMUXER is defined
#ifdef HAVE_OGGDEMUXER

OggDemuxer::OggDemuxer(std::unique_ptr<IOHandler> handler) 
    : Demuxer(std::move(handler)) {
    Debug::log("loader", "OggDemuxer constructor called - initializing libogg sync state");
    Debug::log("ogg", "OggDemuxer constructor called - initializing libogg sync state");
    Debug::log("demuxer", "OggDemuxer constructor called - initializing libogg sync state");
    // Initialize libogg sync state
    int sync_result = ogg_sync_init(&m_sync_state);
    if (sync_result != 0) {
        Debug::log("loader", "OggDemuxer: ogg_sync_init failed with code: ", sync_result);
        Debug::log("ogg", "OggDemuxer: ogg_sync_init failed with code: ", sync_result);
        Debug::log("demuxer", "OggDemuxer: ogg_sync_init failed with code: ", sync_result);
        throw std::runtime_error("Failed to initialize ogg sync state");
    }
    Debug::log("loader", "OggDemuxer constructor completed successfully");
    Debug::log("ogg", "OggDemuxer constructor completed successfully");
    Debug::log("demuxer", "OggDemuxer constructor completed successfully");
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
    Debug::log("loader", "OggDemuxer::parseContainer called");
    Debug::log("ogg", "OggDemuxer::parseContainer called");
    Debug::log("demuxer", "OggDemuxer::parseContainer called");
    
    if (m_parsed) {
        Debug::log("loader", "OggDemuxer::parseContainer already parsed, returning true");
        Debug::log("ogg", "OggDemuxer::parseContainer already parsed, returning true");
        return true;
    }
    
    try {
        // Get file size using IOHandler's getFileSize() method
        m_file_size = m_handler->getFileSize();
        if (m_file_size <= 0) {
            m_file_size = 0;
        }
        
        Debug::log("loader", "OggDemuxer: File size detection using getFileSize() - file_size=", m_file_size, " (hex=0x", std::hex, m_file_size, std::dec, ")");
        Debug::log("ogg", "OggDemuxer: File size detection using getFileSize() - file_size=", m_file_size, " (hex=0x", std::hex, m_file_size, std::dec, ")");
        Debug::log("demuxer", "OggDemuxer: File size detection using getFileSize() - file_size=", m_file_size, " (hex=0x", std::hex, m_file_size, std::dec, ")");
        
        // Log the actual file position and bytes read when the problem occurs
        Debug::log("loader", "OggDemuxer: Initial m_file_size in parseContainer: ", m_file_size);
        Debug::log("ogg", "OggDemuxer: Initial m_file_size in parseContainer: ", m_file_size);
        Debug::log("demuxer", "OggDemuxer: Initial m_file_size in parseContainer: ", m_file_size);
        
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
                        Debug::log("ogg", "OggDemuxer: No progress for 10 iterations, stopping header parsing");
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
                    Debug::log("ogg", "OggDemuxer: Stream ", id, " codec=", stream.codec_name, 
                                       " headers not complete, header_count=", stream.header_packets.size());
                    break;
                }
            }
            
            if (all_headers_complete) {
                Debug::log("ogg", "OggDemuxer: All headers complete, stopping header parsing");
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
        
        Debug::log("ogg", "OggDemuxer: Headers parsed, rewound to beginning for sequential packet reading");
        
        m_parsed = true;
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("ogg", "Error parsing container: ", e.what());
        return false;
    }
}

bool OggDemuxer::readIntoSyncBuffer(size_t bytes) {
    try {
        // Validate input parameters
        if (bytes == 0 || bytes > 1048576) { // Limit to 1MB per read
            Debug::log("ogg", "OggDemuxer: Invalid buffer size requested: ", bytes);
            return false;
        }
        
        char* buffer = ogg_sync_buffer(&m_sync_state, static_cast<long>(bytes));
        if (!buffer) {
            Debug::log("ogg", "OggDemuxer: ogg_sync_buffer failed - out of memory or invalid sync state");
            return false;
        }
        
        off_t current_pos_before = m_handler->tell();
        if (current_pos_before < 0) {
            Debug::log("ogg", "OggDemuxer: Invalid file position before read");
            return false;
        }
        
        long bytes_read = m_handler->read(buffer, 1, bytes);
        
        Debug::log("ogg", "OggDemuxer: readIntoSyncBuffer at ", m_position_ms, "ms - pos_before=", current_pos_before, 
                       ", requested=", bytes, ", bytes_read=", bytes_read);
        
        if (bytes_read < 0) {
            Debug::log("ogg", "OggDemuxer: I/O error during read: ", bytes_read);
            return false;
        }
        
        if (bytes_read == 0) {
            // Check if we're at EOF or if this is an I/O error
            if (m_handler->eof()) {
                Debug::log("ogg", "OggDemuxer: Reached end of file");
                m_eof = true;
            } else {
                Debug::log("ogg", "OggDemuxer: No data available (temporary)");
            }
            return false;
        }
        
        // Validate that bytes_read doesn't exceed what we requested
        if (bytes_read > static_cast<long>(bytes)) {
            Debug::log("ogg", "OggDemuxer: Read more bytes than requested - potential buffer overflow");
            bytes_read = static_cast<long>(bytes);
        }
        
        int sync_result = ogg_sync_wrote(&m_sync_state, bytes_read);
        if (sync_result != 0) {
            Debug::log("ogg", "OggDemuxer: ogg_sync_wrote failed with code: ", sync_result);
            // Try to recover by resetting sync state
            ogg_sync_reset(&m_sync_state);
            return false;
        }
        
        // Log file position advancement around the problem area
        long pos_after_write = m_handler->tell();
        Debug::log("ogg", "OggDemuxer: readIntoSyncBuffer success - read ", bytes_read, " bytes, pos advanced from ", 
                       current_pos_before, " to ", pos_after_write);
        
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("ogg", "OggDemuxer: Exception in readIntoSyncBuffer: ", e.what());
        return false;
    }
}

bool OggDemuxer::processPages() {
    ogg_page page;
    bool processed_any_pages = false;
    
    try {
        while (ogg_sync_pageout(&m_sync_state, &page) == 1) {
            processed_any_pages = true;
            
            // Validate page structure before accessing any fields
            if (!validateOggPage(&page)) {
                Debug::log("ogg", "OggDemuxer: Invalid page structure, skipping page");
                continue;
            }
            
            uint32_t stream_id = ogg_page_serialno(&page);
            
            // Validate stream ID
            if (stream_id == 0) {
                Debug::log("ogg", "OggDemuxer: Invalid stream ID 0, skipping page");
                continue;
            }
            
            // Initialize stream if it's new
            if (m_streams.find(stream_id) == m_streams.end()) {
                OggStream stream;
                stream.serial_number = stream_id;
                m_streams[stream_id] = stream;
                
                // Initialize libogg stream state
                if (ogg_stream_init(&m_ogg_streams[stream_id], stream_id) != 0) {
                    Debug::log("ogg", "OggDemuxer: Failed to initialize ogg stream for ID ", stream_id);
                    m_streams.erase(stream_id);
                    continue;
                }
            }
            
            // Add page to stream with error handling
            int pagein_result = ogg_stream_pagein(&m_ogg_streams[stream_id], &page);
            if (pagein_result != 0) {
                Debug::log("ogg", "OggDemuxer: Failed to add page to stream ", stream_id, ", error code: ", pagein_result);
                continue;
            }
        
            // Extract packets from this stream
            ogg_packet packet;
            int packet_result;
            while ((packet_result = ogg_stream_packetout(&m_ogg_streams[stream_id], &packet)) == 1) {
                // Validate packet structure
                if (!validateOggPacket(&packet, stream_id)) {
                    Debug::log("ogg", "OggDemuxer: Invalid packet from stream ", stream_id, ", skipping");
                    continue;
                }
                
                Debug::log("ogg", "OggDemuxer: Extracted packet from stream ", stream_id, ", size=", packet.bytes);
                OggStream& stream = m_streams[stream_id];
                
                if (!stream.headers_complete) {
                    // Try to identify codec from first packet
                    if (stream.codec_name.empty()) {
                        try {
                            std::vector<uint8_t> packet_data(packet.packet, packet.packet + packet.bytes);
                            stream.codec_name = identifyCodec(packet_data);
                            stream.codec_type = stream.codec_name.empty() ? "unknown" : "audio";
                            
                            if (stream.codec_name.empty()) {
                                Debug::log("ogg", "OggDemuxer: Could not identify codec for stream ", stream_id);
                            }
                        } catch (const std::exception& e) {
                            Debug::log("ogg", "OggDemuxer: Exception identifying codec: ", e.what());
                            continue;
                        }
                    }
                    
                    // Parse codec-specific headers
                    try {
                        OggPacket ogg_packet;
                        ogg_packet.stream_id = stream_id;
                        ogg_packet.data.assign(packet.packet, packet.packet + packet.bytes);
                        ogg_packet.granule_position = packet.granulepos;
                        ogg_packet.is_first_packet = packet.b_o_s;
                        ogg_packet.is_last_packet = packet.e_o_s;
                
                        Debug::log("ogg", "OggDemuxer: Processing packet for stream ", stream_id, ", codec=", stream.codec_name, 
                                       ", packet_size=", packet.bytes, ", headers_complete=", stream.headers_complete);
                        
                        bool is_header_packet = false;
                        try {
#ifdef HAVE_VORBIS
                            if (stream.codec_name == "vorbis") {
                                is_header_packet = parseVorbisHeaders(stream, ogg_packet);
                            } else
#endif
#ifdef HAVE_FLAC
                            if (stream.codec_name == "flac") {
                                is_header_packet = parseFLACHeaders(stream, ogg_packet);
                            } else
#endif
#ifdef HAVE_OPUS
                            if (stream.codec_name == "opus") {
                                is_header_packet = parseOpusHeaders(stream, ogg_packet);
                                Debug::log("ogg", "OggDemuxer: parseOpusHeaders returned ", is_header_packet, " for packet size ", ogg_packet.data.size());
                            } else
#endif
                            if (stream.codec_name == "speex") {
                                is_header_packet = parseSpeexHeaders(stream, ogg_packet);
                            } else {
                                Debug::log("ogg", "OggDemuxer: Unknown codec ", stream.codec_name, " for stream ", stream_id);
                            }
                        } catch (const std::exception& e) {
                            Debug::log("ogg", "OggDemuxer: Exception parsing headers for codec ", stream.codec_name, ": ", e.what());
                            is_header_packet = false;
                        }
                
                // Only add to header packets if it was actually a valid header
                if (is_header_packet) {
                    stream.header_packets.push_back(ogg_packet);
                    
                    Debug::log("ogg", "OggDemuxer: Added header packet ", stream.header_packets.size(), " for stream ", stream_id, ", codec=", stream.codec_name);
                    
                    // Check if headers are complete (codec-specific logic)
                    if (stream.codec_name == "vorbis" && stream.header_packets.size() >= 3) {
                        stream.headers_complete = true;
                    } else if (stream.codec_name == "flac" && stream.header_packets.size() >= 1) {
                        stream.headers_complete = true;
                    } else if (stream.codec_name == "opus" && stream.header_packets.size() >= 2) {
                        stream.headers_complete = true;
                        Debug::log("ogg", "OggDemuxer: Opus headers complete for stream ", stream_id, ", header_count=", stream.header_packets.size());
                    } else if (stream.codec_name == "opus") {
                        Debug::log("ogg", "OggDemuxer: Opus headers still incomplete for stream ", stream_id, ", header_count=", stream.header_packets.size(), " (need 2)");
                    }
                    
                    Debug::log("ogg", "OggDemuxer: Stream ", stream_id, " now has ", stream.header_packets.size(), " header packets, codec=", stream.codec_name, ", headers_complete=", stream.headers_complete);
                } else {
                    // If this packet wasn't a header but we haven't completed headers yet,
                    // this might be an audio packet that came before we finished parsing headers.
                    // Queue it for later processing after headers are complete.
                    
                    Debug::log("ogg", "OggDemuxer: Non-header packet during header parsing, stream_id=", stream_id, 
                                       ", packet_size=", packet.bytes, ", header_packets_count=", stream.header_packets.size());
                    
                    // Queue the packet for later processing
                    stream.m_packet_queue.push_back(ogg_packet);
                    }
                    
                    } catch (const std::exception& e) {
                        Debug::log("ogg", "OggDemuxer: Exception creating OggPacket: ", e.what());
                        continue;
                    }
                } else {
                    // Headers are complete, queue the packet for normal processing
                    try {
                        stream.m_packet_queue.push_back(OggPacket{stream_id, 
                                                                  std::vector<uint8_t>(packet.packet, packet.packet + packet.bytes), 
                                                                  static_cast<uint64_t>(packet.granulepos), 
                                                                  (bool)packet.b_o_s, 
                                                                  (bool)packet.e_o_s});
                    } catch (const std::exception& e) {
                        Debug::log("ogg", "OggDemuxer: Exception queuing packet: ", e.what());
                        continue;
                    }
                }
            }
        }
        
        return processed_any_pages;
        
    } catch (const std::exception& e) {
        Debug::log("ogg", "OggDemuxer: Exception in processPages: ", e.what());
        return false;
    }
}

std::vector<StreamInfo> OggDemuxer::getStreams() const {
    std::vector<StreamInfo> streams;
    
    for (const auto& [stream_id, stream] : m_streams) {
        if (stream.codec_type == "audio" && stream.headers_complete) {
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
            
            // Populate codec_data with header packets for decoder initialization
            if (!stream.header_packets.empty()) {
                // For Ogg codecs, we need to provide all header packets
                // Concatenate all header packets with length prefixes
                for (const auto& header_packet : stream.header_packets) {
                    // Add 4-byte length prefix (little-endian)
                    uint32_t packet_size = static_cast<uint32_t>(header_packet.data.size());
                    info.codec_data.push_back(packet_size & 0xFF);
                    info.codec_data.push_back((packet_size >> 8) & 0xFF);
                    info.codec_data.push_back((packet_size >> 16) & 0xFF);
                    info.codec_data.push_back((packet_size >> 24) & 0xFF);
                    
                    // Add packet data
                    info.codec_data.insert(info.codec_data.end(), 
                                         header_packet.data.begin(), 
                                         header_packet.data.end());
                }
            }
            
            // Set codec tag based on codec type
            if (stream.codec_name == "vorbis") {
                info.codec_tag = 0x566F7262; // "Vorb"
            } else if (stream.codec_name == "opus") {
                info.codec_tag = 0x4F707573; // "Opus"
            } else if (stream.codec_name == "flac") {
                info.codec_tag = 0x664C6143; // "fLaC"
            } else if (stream.codec_name == "speex") {
                info.codec_tag = 0x53706578; // "Spex"
            }
            
            streams.push_back(info);
        }
    }
    
    return streams;
}

StreamInfo OggDemuxer::getStreamInfo(uint32_t stream_id) const {
    auto it = m_streams.find(stream_id);
    if (it != m_streams.end() && it->second.headers_complete) {
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
        
        // Populate codec_data with header packets for decoder initialization
        if (!stream.header_packets.empty()) {
            // For Ogg codecs, we need to provide all header packets
            // Concatenate all header packets with length prefixes
            for (const auto& header_packet : stream.header_packets) {
                // Add 4-byte length prefix (little-endian)
                uint32_t packet_size = static_cast<uint32_t>(header_packet.data.size());
                info.codec_data.push_back(packet_size & 0xFF);
                info.codec_data.push_back((packet_size >> 8) & 0xFF);
                info.codec_data.push_back((packet_size >> 16) & 0xFF);
                info.codec_data.push_back((packet_size >> 24) & 0xFF);
                
                // Add packet data
                info.codec_data.insert(info.codec_data.end(), 
                                     header_packet.data.begin(), 
                                     header_packet.data.end());
            }
        }
        
        // Set codec tag based on codec type
        if (stream.codec_name == "vorbis") {
            info.codec_tag = 0x566F7262; // "Vorb"
        } else if (stream.codec_name == "opus") {
            info.codec_tag = 0x4F707573; // "Opus"
        } else if (stream.codec_name == "flac") {
            info.codec_tag = 0x664C6143; // "fLaC"
        } else if (stream.codec_name == "speex") {
            info.codec_tag = 0x53706578; // "Spex"
        }
        
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
    auto stream_it = m_streams.find(stream_id);
    if (stream_it == m_streams.end()) {
        m_eof = true;
        return MediaChunk{};
    }
    OggStream& stream = stream_it->second;

    // First, send header packets if they haven't been sent yet
    if (!stream.headers_sent && stream.headers_complete) {
        if (stream.next_header_index < stream.header_packets.size()) {
            const OggPacket& header_packet = stream.header_packets[stream.next_header_index];
            stream.next_header_index++;
            
            MediaChunk chunk;
            chunk.stream_id = stream_id;
            chunk.data = header_packet.data;
            chunk.granule_position = 0;
            chunk.is_keyframe = true;
            
            if (stream.next_header_index >= stream.header_packets.size()) {
                stream.headers_sent = true;
            }
            return chunk;
        }
    }

    // If the queue is empty, read more data until we get a packet for our stream
    while (stream.m_packet_queue.empty() && !m_eof) {
        if (!readIntoSyncBuffer(4096)) {
            m_eof = true; // No more data from the source
            break;
        }
        processPages();
    }

    // If we still have no packets after trying to read, we are at the end
    if (stream.m_packet_queue.empty()) {
        m_eof = true;
        return MediaChunk{};
    }

    // Pop the next packet from the queue
    const OggPacket& ogg_packet = stream.m_packet_queue.front();
    
    MediaChunk chunk;
    chunk.stream_id = stream_id;
    chunk.data = ogg_packet.data;
    chunk.granule_position = ogg_packet.granule_position;

    stream.m_packet_queue.pop_front();
    
    // Update our internal position tracker for getPosition()
    if (chunk.granule_position != static_cast<uint64_t>(-1)) {
        uint64_t current_ms = granuleToMs(chunk.granule_position, stream_id);
        if (current_ms > m_position_ms) {
            m_position_ms = current_ms;
        }
    }

    return chunk;
}

bool OggDemuxer::seekTo(uint64_t timestamp_ms) {
    // Find the best audio stream to use for seeking
    uint32_t stream_id = findBestAudioStream();
    if (stream_id == 0) {
        return false;
    }

    // Clear all stream packet queues
    for (auto& [id, stream] : m_streams) {
        stream.m_packet_queue.clear();
    }
    
    // If seeking to beginning, do it directly
    if (timestamp_ms == 0) {
        m_handler->seek(0, SEEK_SET);
        ogg_sync_reset(&m_sync_state);
        
        for (auto& [id, ogg_stream] : m_ogg_streams) {
            ogg_stream_reset(&ogg_stream);
        }
        
        // Do NOT resend headers after seeking - decoder state should be maintained
        // Headers are only sent once at the beginning of playback
        for (auto& [id, stream] : m_streams) {
            stream.total_samples_processed = 0;
        }
        
        m_position_ms = 0;
        m_eof = false;
        return true;
    }
    
    // Convert timestamp to granule position for seeking
    uint64_t target_granule = msToGranule(timestamp_ms, stream_id);
    
    // Use bisection search to find the target position
    bool success = seekToPage(target_granule, stream_id);
    if (success) {
        // Update sample tracking for the target stream
        // Note: We do NOT resend headers after seeking - the decoder should
        // maintain its state and only needs new audio data packets.
        // Headers are only sent once at the beginning of playback.
        OggStream& stream = m_streams[stream_id];
        stream.total_samples_processed = target_granule;
    }
    return success;
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

uint64_t OggDemuxer::getGranulePosition(uint32_t stream_id) const {
    auto it = m_streams.find(stream_id);
    if (it != m_streams.end()) {
        return it->second.total_samples_processed;
    }
    return 0;
}

// Static function implementation
bool OggDemuxer::hasSignature(const std::vector<uint8_t>& data, const char* signature) {
    size_t sig_len = std::strlen(signature);
    if (data.size() < sig_len) {
        return false;
    }
    
    return std::memcmp(data.data(), signature, sig_len) == 0;
}

bool OggDemuxer::validateOggPage(const ogg_page* page) {
    if (!page) {
        Debug::log("ogg", "OggDemuxer: validateOggPage - null page pointer");
        return false;
    }
    
    // Check if page header is accessible
    if (!page->header || page->header_len < 27) {
        Debug::log("ogg", "OggDemuxer: validateOggPage - invalid header pointer or length");
        return false;
    }
    
    // Validate OggS capture pattern
    if (page->header[0] != 'O' || page->header[1] != 'g' || 
        page->header[2] != 'g' || page->header[3] != 'S') {
        Debug::log("ogg", "OggDemuxer: validateOggPage - invalid capture pattern");
        return false;
    }
    
    // Check version (should be 0)
    if (page->header[4] != 0) {
        Debug::log("ogg", "OggDemuxer: validateOggPage - unsupported version: ", static_cast<int>(page->header[4]));
        return false;
    }
    
    // Validate page body
    if (page->body_len > 0 && !page->body) {
        Debug::log("ogg", "OggDemuxer: validateOggPage - invalid body pointer with non-zero length");
        return false;
    }
    
    // Check segment count and validate against header length
    uint8_t segments = page->header[26];
    if (page->header_len != 27 + segments) {
        Debug::log("ogg", "OggDemuxer: validateOggPage - header length mismatch: expected ", 27 + segments, ", got ", page->header_len);
        return false;
    }
    
    // Validate that body length matches sum of segment lengths
    if (segments > 0) {
        long expected_body_len = 0;
        for (int i = 0; i < segments; i++) {
            expected_body_len += page->header[27 + i];
        }
        if (expected_body_len != page->body_len) {
            Debug::log("ogg", "OggDemuxer: validateOggPage - body length mismatch: expected ", expected_body_len, ", got ", page->body_len);
            return false;
        }
    }
    
    return true;
}

bool OggDemuxer::validateOggPacket(const ogg_packet* packet, uint32_t stream_id) {
    if (!packet) {
        Debug::log("ogg", "OggDemuxer: validateOggPacket - null packet pointer for stream ", stream_id);
        return false;
    }
    
    // Check packet data pointer and size
    if (packet->bytes <= 0) {
        Debug::log("ogg", "OggDemuxer: validateOggPacket - invalid packet size: ", packet->bytes, " for stream ", stream_id);
        return false;
    }
    
    if (!packet->packet) {
        Debug::log("ogg", "OggDemuxer: validateOggPacket - null packet data pointer for stream ", stream_id);
        return false;
    }
    
    // Reasonable size limit (10MB per packet should be more than enough for audio)
    if (packet->bytes > 10485760) {
        Debug::log("ogg", "OggDemuxer: validateOggPacket - packet too large: ", packet->bytes, " bytes for stream ", stream_id);
        return false;
    }
    
    // Validate granule position (should not be invalid unless it's a header packet)
    if (packet->granulepos < -1) {
        Debug::log("ogg", "OggDemuxer: validateOggPacket - invalid granule position: ", packet->granulepos, " for stream ", stream_id);
        return false;
    }
    
    return true;
}

std::string OggDemuxer::identifyCodec(const std::vector<uint8_t>& packet_data) {
#ifdef HAVE_VORBIS
    if (OggDemuxer::hasSignature(packet_data, "\x01vorbis")) {
        return "vorbis";
    }
#endif

#ifdef HAVE_FLAC
    if (OggDemuxer::hasSignature(packet_data, "\x7f""FLAC")) {
        return "flac";
    }
#endif

#ifdef HAVE_OPUS
    if (OggDemuxer::hasSignature(packet_data, "OpusHead")) {
        return "opus";
    }
#endif

    // Speex is not conditionally compiled in this implementation
    if (OggDemuxer::hasSignature(packet_data, "Speex   ")) {
        return "speex";
    }
    
    return "";
}


#ifdef HAVE_VORBIS
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
        // Comment header
        parseVorbisComments(stream, packet);
        return true;
    } else if (packet_type == 5 && OggDemuxer::hasSignature(packet.data, "\x05vorbis")) {
        // Setup header
        stream.codec_setup_data = packet.data;
        return true;
    }
    
    return false;
}
#endif // HAVE_VORBIS

#ifdef HAVE_VORBIS
void OggDemuxer::parseVorbisComments(OggStream& stream, const OggPacket& packet)
{
    // Vorbis comment header format:
    // 7 bytes: "\x03vorbis"
    // 4 bytes: vendor_length (little-endian)
    // vendor_length bytes: vendor string
    // 4 bytes: user_comment_list_length (little-endian)
    // For each comment:
    //   4 bytes: length (little-endian)
    //   length bytes: comment in UTF-8 format "FIELD=value"

    if (packet.data.size() < 15) { // 7 + 4 + 4
        return; // Too small
    }

    size_t offset = 7; // Skip "\x03vorbis"

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

            if (field == "METADATA_BLOCK_PICTURE") {
                Debug::log("ogg", "OggDemuxer: Vorbis Comments - ", field, "=<", value.size(), " bytes of binary data>");
            } else {
                Debug::log("ogg", "OggDemuxer: Vorbis Comments - ", field, "=", value);
            }
        }
    }
}
#endif // HAVE_VORBIS

#ifdef HAVE_FLAC
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
#endif // HAVE_FLAC

#ifdef HAVE_OPUS
bool OggDemuxer::parseOpusHeaders(OggStream& stream, const OggPacket& packet) {
    std::string first_bytes;
    for (size_t i = 0; i < std::min(size_t(16), packet.data.size()); i++) {
        char c = packet.data[i];
        if (c >= 32 && c <= 126) {
            first_bytes += c;
        } else {
            first_bytes += "\\x" + std::to_string(static_cast<unsigned char>(c));
        }
    }
    Debug::log("ogg", "OggDemuxer: parseOpusHeaders called, packet_size=", packet.data.size(), ", first 16 bytes: '", first_bytes, "'");
    
    if (OggDemuxer::hasSignature(packet.data, "OpusHead")) {
        // Opus identification header - must be first
        if (packet.data.size() >= 19) {
            stream.channels = OggDemuxer::readLE<uint8_t>(packet.data, 9);
            // Note: Opus always runs at 48kHz internally, but can be presented at other rates
            stream.sample_rate = 48000;
            stream.pre_skip = OggDemuxer::readLE<uint16_t>(packet.data, 10); // Read pre-skip
            
            Debug::log("ogg", "OggDemuxer: OpusHead header found, channels=", stream.channels, ", pre_skip=", stream.pre_skip, ", packet_size=", packet.data.size());
        }
        return true;
    } else if (OggDemuxer::hasSignature(packet.data, "OpusTags")) {
        // Opus comment header - must be second
        Debug::log("ogg", "OggDemuxer: OpusTags header found, packet_size=", packet.data.size());
        
        // Parse OpusTags metadata (follows Vorbis comment format)
        parseOpusTags(stream, packet);
        return true;
    }
    
    Debug::log("ogg", "OggDemuxer: Unknown Opus packet, size=", packet.data.size());
    
    return false;
}
#endif // HAVE_OPUS

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

#ifdef HAVE_OPUS
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
            
            // Don't dump large binary fields like METADATA_BLOCK_PICTURE
            if (field == "METADATA_BLOCK_PICTURE") {
                Debug::log("ogg", "OggDemuxer: OpusTags - ", field, "=<", value.size(), " bytes of binary data>");
            } else {
                Debug::log("ogg", "OggDemuxer: OpusTags - ", field, "=", value);
            }
        }
    }
}
#endif // HAVE_OPUS

void OggDemuxer::calculateDuration() {
    Debug::log("ogg", "OggDemuxer: calculateDuration() called");
    m_duration_ms = 0;
    
    for (const auto& [stream_id, stream] : m_streams) {
        if (stream.codec_type == "audio" && stream.sample_rate > 0) {
            Debug::log("ogg", "OggDemuxer: calculateDuration - Stream ", stream_id, " (", stream.codec_name, ") has total_samples=", stream.total_samples, ", sample_rate=", stream.sample_rate);
            if (stream.total_samples > 0) {
                uint64_t stream_duration = (stream.total_samples * 1000ULL) / stream.sample_rate;
                m_duration_ms = std::max(m_duration_ms, stream_duration);
                Debug::log("ogg", "OggDemuxer: calculateDuration - Calculated duration from samples: ", stream_duration, "ms");
            }
        }
    }
    
    // If we couldn't determine duration from container, try using tracked max granule
    if (m_duration_ms == 0) {
        if (m_max_granule_seen > 0) {
            Debug::log("ogg", "OggDemuxer: calculateDuration - Using tracked max granule: ", m_max_granule_seen);
            // Use the tracked maximum granule position
            uint32_t stream_id = findBestAudioStream();
            if (stream_id != 0) {
                m_duration_ms = granuleToMs(m_max_granule_seen, stream_id);
                Debug::log("ogg", "OggDemuxer: calculateDuration - Duration from tracked max granule: ", m_max_granule_seen, " -> ", m_duration_ms, "ms");
            }
        } else {
            Debug::log("ogg", "OggDemuxer: calculateDuration - Falling back to reading last page.");
            // Fall back to reading last page
            uint64_t last_granule = getLastGranulePosition();
            Debug::log("ogg", "OggDemuxer: calculateDuration - getLastGranulePosition returned: ", last_granule);
            if (last_granule > 0) {
                // Use the first audio stream for duration calculation
                uint32_t stream_id = findBestAudioStream();
                if (stream_id != 0) {
                    m_duration_ms = granuleToMs(last_granule, stream_id);
                    Debug::log("ogg", "OggDemuxer: calculateDuration - Duration from last granule: ", m_duration_ms, "ms");
                }
            }
        }
    }
    
    // Final fallback - use a reasonable default
    if (m_duration_ms == 0) {
        Debug::log("ogg", "OggDemuxer: calculateDuration - Could not determine duration - no fallback used");
        // No fallback - leave as 0 if duration cannot be determined
    }
}

uint64_t OggDemuxer::granuleToMs(uint64_t granule, uint32_t stream_id) const {
    auto it = m_streams.find(stream_id);
    if (it == m_streams.end() || it->second.sample_rate == 0) {
        Debug::log("ogg", "OggDemuxer: granuleToMs - Invalid stream_id or sample_rate is 0. stream_id=", stream_id);
        return 0;
    }
    
    // Check for invalid granule positions
    if (granule == static_cast<uint64_t>(-1) || granule > 0x7FFFFFFFFFFFFFFULL) {
        Debug::log("ogg", "OggDemuxer: granuleToMs - Invalid granule position: ", granule);
        return 0;
    }
    
    Debug::log("ogg", "OggDemuxer: granuleToMs - Input granule=", granule, ", stream_id=", stream_id, ", codec=", it->second.codec_name, ", sample_rate=", it->second.sample_rate, ", pre_skip=", it->second.pre_skip);
    
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
        Debug::log("ogg", "OggDemuxer: granuleToMs (Opus) - effective_granule=", effective_granule, ", result=", (effective_granule * 1000ULL) / 48000ULL);
        return (effective_granule * 1000ULL) / 48000ULL;
    }
    
    uint64_t result = (granule * 1000ULL) / it->second.sample_rate;
    Debug::log("ogg", "OggDemuxer: granuleToMs (Non-Opus) - result=", result);
    return result;
}

uint64_t OggDemuxer::msToGranule(uint64_t timestamp_ms, uint32_t stream_id) const {
    auto it = m_streams.find(stream_id);
    if (it == m_streams.end() || it->second.sample_rate == 0) {
        return 0;
    }
    
    const OggStream& stream = it->second;
    
    // For Opus streams, granule positions are in 48kHz sample units
    if (stream.codec_name == "opus") {
        // Convert milliseconds to 48kHz samples and add pre-skip
        uint64_t samples_48k = (timestamp_ms * 48000ULL) / 1000ULL;
        return samples_48k + stream.pre_skip;
    }
    
    return (timestamp_ms * stream.sample_rate) / 1000ULL;
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
        Debug::log("ogg", "OggDemuxer: getLastGranulePosition - file_size is 0");
        return 0;
    }
    
    Debug::log("ogg", "OggDemuxer: getLastGranulePosition - file_size=", m_file_size);
    
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
        
        Debug::log("ogg", "OggDemuxer: getLastGranulePosition - manual scan found ", pages_found, " pages, last_granule=", last_granule);
        
        if (pages_found > 0) {
            // Restore original position
            m_handler->seek(current_pos, SEEK_SET);
            return last_granule;
        }
    }
    
    // Restore original position
    m_handler->seek(current_pos, SEEK_SET);
    
    Debug::log("ogg", "OggDemuxer: getLastGranulePosition - failed to find any pages with all search sizes");
    
    return 0;
}

// According to RFC 6716, Section 3.1
static const int opus_samples_per_frame_lk[32] = {
    480, 960, 1920, 2880, // SILK-only NB
    480, 960, 1920, 2880, // SILK-only MB
    480, 960, 1920, 2880, // SILK-only WB
    480, 960,             // Hybrid SWB
    480, 960,             // Hybrid FB
    -1, -1,              // Reserved
    120, 240, 480, 960,    // CELT-only NB
    120, 240, 480, 960,    // CELT-only WB
    120, 240, 480, 960     // CELT-only SWB/FB
};

int OggDemuxer::getOpusPacketSampleCount(const OggPacket& packet) {
    if (packet.data.empty()) {
        return 0;
    }

    uint8_t toc = packet.data[0];
    int config = (toc >> 3) & 0x1F;
    int frames;

    switch (toc & 0x03) {
        case 0:
            frames = 1;
            break;
        case 1:
        case 2:
            frames = 2;
            break;
        case 3:
            if (packet.data.size() < 2) return 0;
            frames = packet.data[1] & 0x3F;
            break;
        default:
            return 0; // Unreachable
    }

    return frames * opus_samples_per_frame_lk[config];
}

bool OggDemuxer::seekToPage(uint64_t target_granule, uint32_t stream_id)
{
    if (m_file_size == 0) return false;

    Debug::log("ogg", "OggDemuxer: seekToPage - target_granule=", target_granule, ", stream_id=", stream_id, ", file_size=", m_file_size);

    // Implement proper bisection search algorithm
    long left = 0;
    long right = static_cast<long>(m_file_size);
    long best_pos = 0;
    uint64_t best_granule = 0;
    bool found_valid_page = false;
    
    // Maximum iterations to prevent infinite loops
    const int max_iterations = 32;
    int iterations = 0;
    
    while (left < right && iterations < max_iterations) {
        iterations++;
        long mid = left + (right - left) / 2;
        
        Debug::log("ogg", "OggDemuxer: seekToPage bisection iteration ", iterations, " - left=", left, ", mid=", mid, ", right=", right);
        
        // Find granule position at this file offset
        uint64_t granule_at_mid = findGranuleAtOffset(mid, stream_id);
        
        if (granule_at_mid == static_cast<uint64_t>(-1)) {
            // No valid page found at this position, try moving right
            left = mid + 1;
            Debug::log("ogg", "OggDemuxer: seekToPage - no valid page at offset ", mid, ", moving right");
            continue;
        }
        
        Debug::log("ogg", "OggDemuxer: seekToPage - found granule ", granule_at_mid, " at offset ", mid, ", target=", target_granule);
        
        // Update best position if this is closer to our target
        if (!found_valid_page || 
            (granule_at_mid <= target_granule && granule_at_mid > best_granule) ||
            (best_granule > target_granule && granule_at_mid < best_granule)) {
            best_pos = mid;
            best_granule = granule_at_mid;
            found_valid_page = true;
        }
        
        if (granule_at_mid < target_granule) {
            // Need to search right half
            left = mid + 1;
        } else if (granule_at_mid > target_granule) {
            // Need to search left half
            right = mid - 1;
        } else {
            // Exact match found
            best_pos = mid;
            best_granule = granule_at_mid;
            break;
        }
    }
    
    if (!found_valid_page) {
        Debug::log("ogg", "OggDemuxer: seekToPage - no valid pages found during bisection, seeking to beginning");
        m_handler->seek(0, SEEK_SET);
        ogg_sync_reset(&m_sync_state);
        m_position_ms = 0;
        m_eof = false;
        return true;
    }
    
    Debug::log("ogg", "OggDemuxer: seekToPage - bisection completed after ", iterations, " iterations, best_pos=", best_pos, ", best_granule=", best_granule);
    
    // Seek to the best position found
    m_handler->seek(best_pos, SEEK_SET);
    
    // Reset sync and stream states for clean reading
    ogg_sync_reset(&m_sync_state);
    for (auto& [sid, ogg_stream] : m_ogg_streams) {
        ogg_stream_reset(&ogg_stream);
    }
    
    // Update position tracking
    m_position_ms = granuleToMs(best_granule, stream_id);
    m_eof = false;
    
    Debug::log("ogg", "OggDemuxer: seekToPage completed - final position=", m_position_ms, "ms");
    return true;
}

uint64_t OggDemuxer::findGranuleAtOffset(long file_offset, uint32_t stream_id) {
    // Save current position
    long original_pos = m_handler->tell();
    
    // Seek to the target offset
    if (m_handler->seek(file_offset, SEEK_SET) != 0) {
        Debug::log("ogg", "OggDemuxer: findGranuleAtOffset - failed to seek to offset ", file_offset);
        m_handler->seek(original_pos, SEEK_SET);
        return static_cast<uint64_t>(-1);
    }
    
    // Create a temporary sync state for this search
    ogg_sync_state temp_sync;
    if (ogg_sync_init(&temp_sync) != 0) {
        Debug::log("ogg", "OggDemuxer: findGranuleAtOffset - failed to initialize temp sync state");
        m_handler->seek(original_pos, SEEK_SET);
        return static_cast<uint64_t>(-1);
    }
    
    uint64_t found_granule = static_cast<uint64_t>(-1);
    
    try {
        // Read data into temporary sync buffer
        const size_t read_size = 8192;
        char* buffer = ogg_sync_buffer(&temp_sync, read_size);
        if (!buffer) {
            throw std::runtime_error("Failed to get sync buffer");
        }
        
        long bytes_read = m_handler->read(buffer, 1, read_size);
        if (bytes_read <= 0) {
            throw std::runtime_error("Failed to read data");
        }
        
        if (ogg_sync_wrote(&temp_sync, bytes_read) != 0) {
            throw std::runtime_error("Failed to write to sync buffer");
        }
        
        // Look for pages in the buffer
        ogg_page page;
        while (ogg_sync_pageout(&temp_sync, &page) == 1) {
            // Validate page before accessing its fields
            if (!validateOggPage(&page)) {
                continue;
            }
            
            uint32_t page_stream_id = ogg_page_serialno(&page);
            uint64_t page_granule = ogg_page_granulepos(&page);
            
            if (page_stream_id == stream_id && page_granule != static_cast<uint64_t>(-1)) {
                found_granule = page_granule;
                Debug::log("ogg", "OggDemuxer: findGranuleAtOffset - found granule ", page_granule, " for stream ", stream_id, " at offset ", file_offset);
                break;
            }
        }
        
    } catch (const std::exception& e) {
        Debug::log("ogg", "OggDemuxer: findGranuleAtOffset - exception: ", e.what());
    }
    
    // Clean up temporary sync state
    ogg_sync_clear(&temp_sync);
    
    // Restore original position
    m_handler->seek(original_pos, SEEK_SET);
    
    return found_granule;
}

// Stub implementations for disabled codecs to prevent compilation errors

#ifndef HAVE_VORBIS
bool OggDemuxer::parseVorbisHeaders(OggStream& stream, const OggPacket& packet) {
    Debug::log("ogg", "OggDemuxer: Vorbis support not compiled in");
    return false;
}

void OggDemuxer::parseVorbisComments(OggStream& stream, const OggPacket& packet) {
    Debug::log("ogg", "OggDemuxer: Vorbis support not compiled in");
}
#endif // !HAVE_VORBIS

#ifndef HAVE_FLAC
bool OggDemuxer::parseFLACHeaders(OggStream& stream, const OggPacket& packet) {
    Debug::log("ogg", "OggDemuxer: FLAC support not compiled in");
    return false;
}
#endif // !HAVE_FLAC

#ifndef HAVE_OPUS
bool OggDemuxer::parseOpusHeaders(OggStream& stream, const OggPacket& packet) {
    Debug::log("ogg", "OggDemuxer: Opus support not compiled in");
    return false;
}

void OggDemuxer::parseOpusTags(OggStream& stream, const OggPacket& packet) {
    Debug::log("ogg", "OggDemuxer: Opus support not compiled in");
}
#endif // !HAVE_OPUS

#endif // HAVE_OGGDEMUXER